/// @file scene_loader.cpp
/// @brief Scene loading implementation for void_runtime

#include "scene_loader.hpp"
#include "scene_parser.hpp"

#include <algorithm>
#include <fstream>
#include <future>
#include <sstream>
#include <thread>

namespace void_runtime {

// =============================================================================
// SceneLoader Implementation
// =============================================================================

SceneLoader::SceneLoader() = default;
SceneLoader::~SceneLoader() = default;

bool SceneLoader::initialize() {
    return true;
}

void SceneLoader::shutdown() {
    // Wait for any pending async loads
    if (current_async_task_ && current_async_task_->future.valid()) {
        current_async_task_->future.wait();
    }

    unload_all_scenes();
}

void SceneLoader::update() {
    // Process async loading
    if (current_async_task_ && current_async_task_->future.valid()) {
        auto status = current_async_task_->future.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            bool success = current_async_task_->future.get();

            if (current_async_task_->on_complete) {
                current_async_task_->on_complete(current_async_task_->scene_name, success);
            }

            if (success && scene_loaded_callback_) {
                scene_loaded_callback_(current_async_task_->scene_name, true);
            }

            current_progress_.completed = true;
            current_async_task_.reset();
        }
    }

    // Process pending unloads
    process_pending_unloads();

    // Check for hot reload changes
    if (hot_reload_enabled_) {
        check_for_changes();
    }
}

bool SceneLoader::load_scene(const std::string& scene_name, SceneLoadMode mode) {
    auto path = find_scene_file(scene_name);
    if (path.empty()) {
        auto it = registered_scenes_.find(scene_name);
        if (it != registered_scenes_.end()) {
            path = it->second;
        }
    }

    if (path.empty()) {
        return false;
    }

    return load_scene_internal(path, scene_name, mode);
}

void SceneLoader::load_scene_async(const std::string& scene_name, SceneLoadMode mode,
                                    SceneLoadCallback on_complete,
                                    SceneProgressCallback on_progress) {
    // Find the scene file
    auto path = find_scene_file(scene_name);
    if (path.empty()) {
        auto it = registered_scenes_.find(scene_name);
        if (it != registered_scenes_.end()) {
            path = it->second;
        }
    }

    if (path.empty()) {
        if (on_complete) {
            on_complete(scene_name, false);
        }
        return;
    }

    // Reset progress
    current_progress_ = SceneLoadProgress{};
    current_progress_.current_stage = "Starting";

    // Create async task
    current_async_task_ = std::make_unique<AsyncLoadTask>();
    current_async_task_->scene_name = scene_name;
    current_async_task_->path = path;
    current_async_task_->mode = mode;
    current_async_task_->on_complete = on_complete;
    current_async_task_->on_progress = on_progress;

    // Launch async load
    current_async_task_->future = std::async(std::launch::async,
        [this, path, scene_name, mode]() -> bool {
            return load_scene_internal(path, scene_name, mode, &current_progress_);
        });
}

bool SceneLoader::load_scene_from_file(const std::filesystem::path& path, SceneLoadMode mode) {
    std::string scene_name = path.stem().string();
    return load_scene_internal(path, scene_name, mode);
}

void SceneLoader::unload_scene(const std::string& scene_name) {
    auto it = loaded_scenes_.find(scene_name);
    if (it == loaded_scenes_.end()) {
        return;
    }

    // Mark for unload if persistent and we're in the middle of something
    if (it->second.info.is_persistent) {
        it->second.pending_unload = true;
        return;
    }

    unload_scene_internal(scene_name);
}

void SceneLoader::unload_all_scenes() {
    std::vector<std::string> to_unload;
    for (const auto& [name, data] : loaded_scenes_) {
        if (!data.info.is_persistent) {
            to_unload.push_back(name);
        }
    }

    for (const auto& name : to_unload) {
        unload_scene_internal(name);
    }
}

void SceneLoader::reload_current_scene() {
    if (current_scene_.empty()) {
        return;
    }

    std::string scene_name = current_scene_;
    unload_scene(scene_name);
    load_scene(scene_name, SceneLoadMode::Single);
}

bool SceneLoader::is_scene_loaded(const std::string& scene_name) const {
    auto it = loaded_scenes_.find(scene_name);
    return it != loaded_scenes_.end() &&
           it->second.info.state == SceneLoadState::Loaded;
}

const SceneInfo* SceneLoader::get_scene_info(const std::string& scene_name) const {
    auto it = loaded_scenes_.find(scene_name);
    return it != loaded_scenes_.end() ? &it->second.info : nullptr;
}

std::vector<std::string> SceneLoader::loaded_scenes() const {
    std::vector<std::string> result;
    for (const auto& [name, data] : loaded_scenes_) {
        if (data.info.state == SceneLoadState::Loaded) {
            result.push_back(name);
        }
    }
    return result;
}

SceneLoadState SceneLoader::get_load_state(const std::string& scene_name) const {
    auto it = loaded_scenes_.find(scene_name);
    return it != loaded_scenes_.end() ? it->second.info.state : SceneLoadState::Unloaded;
}

bool SceneLoader::is_loading() const {
    return current_async_task_ && current_async_task_->future.valid();
}

void SceneLoader::add_search_path(const std::filesystem::path& path) {
    if (std::find(search_paths_.begin(), search_paths_.end(), path) == search_paths_.end()) {
        search_paths_.push_back(path);
    }
}

void SceneLoader::remove_search_path(const std::filesystem::path& path) {
    search_paths_.erase(
        std::remove(search_paths_.begin(), search_paths_.end(), path),
        search_paths_.end());
}

std::filesystem::path SceneLoader::find_scene_file(const std::string& scene_name) const {
    // Try various extensions (TOML first as per legacy system)
    std::vector<std::string> extensions = {".toml", ".scene", ".json", ".vscene", ".bin"};

    // Check registered scenes first
    auto it = registered_scenes_.find(scene_name);
    if (it != registered_scenes_.end() && std::filesystem::exists(it->second)) {
        return it->second;
    }

    // Search in paths
    for (const auto& search_path : search_paths_) {
        // Try with extensions
        for (const auto& ext : extensions) {
            auto path = search_path / (scene_name + ext);
            if (std::filesystem::exists(path)) {
                return path;
            }
        }

        // Try as-is
        auto path = search_path / scene_name;
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    // Try current directory
    for (const auto& ext : extensions) {
        auto path = std::filesystem::path(scene_name + ext);
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return {};
}

void SceneLoader::register_scene(const std::string& name, const std::filesystem::path& path) {
    registered_scenes_[name] = path;
}

void SceneLoader::unregister_scene(const std::string& name) {
    registered_scenes_.erase(name);
}

std::vector<std::string> SceneLoader::registered_scenes() const {
    std::vector<std::string> result;
    for (const auto& [name, path] : registered_scenes_) {
        result.push_back(name);
    }
    return result;
}

void SceneLoader::set_scene_persistent(const std::string& scene_name, bool persistent) {
    auto it = loaded_scenes_.find(scene_name);
    if (it != loaded_scenes_.end()) {
        it->second.info.is_persistent = persistent;
    }
}

bool SceneLoader::is_scene_persistent(const std::string& scene_name) const {
    auto it = loaded_scenes_.find(scene_name);
    return it != loaded_scenes_.end() && it->second.info.is_persistent;
}

void SceneLoader::set_scene_loaded_callback(SceneLoadCallback callback) {
    scene_loaded_callback_ = std::move(callback);
}

void SceneLoader::set_scene_unloaded_callback(SceneUnloadCallback callback) {
    scene_unloaded_callback_ = std::move(callback);
}

void SceneLoader::enable_hot_reload(bool enable) {
    hot_reload_enabled_ = enable;

    if (enable) {
        // Record current timestamps
        for (const auto& [name, data] : loaded_scenes_) {
            if (std::filesystem::exists(data.info.path)) {
                file_timestamps_[name] = std::filesystem::last_write_time(data.info.path);
            }
        }
    }
}

void SceneLoader::check_for_changes() {
    for (const auto& [name, data] : loaded_scenes_) {
        if (!std::filesystem::exists(data.info.path)) {
            continue;
        }

        auto current_time = std::filesystem::last_write_time(data.info.path);
        auto it = file_timestamps_.find(name);

        if (it != file_timestamps_.end() && current_time != it->second) {
            // File changed, reload scene
            file_timestamps_[name] = current_time;

            // Queue for reload
            if (name == current_scene_) {
                reload_current_scene();
            }
        }
    }
}

bool SceneLoader::load_scene_internal(const std::filesystem::path& path,
                                       const std::string& name,
                                       SceneLoadMode mode,
                                       SceneLoadProgress* progress) {
    if (progress) {
        progress->current_stage = "Loading";
        progress->progress = 0.0f;
    }

    // Handle scene mode
    if (mode == SceneLoadMode::Single) {
        // Unload all non-persistent scenes
        std::vector<std::string> to_unload;
        for (const auto& [scene_name, data] : loaded_scenes_) {
            if (!data.info.is_persistent && scene_name != name) {
                to_unload.push_back(scene_name);
            }
        }
        for (const auto& scene_name : to_unload) {
            unload_scene_internal(scene_name);
        }
    }

    // Create scene data
    SceneData scene_data;
    scene_data.info.name = name;
    scene_data.info.path = path;
    scene_data.info.state = SceneLoadState::Loading;

    if (progress) {
        progress->current_stage = "Parsing";
        progress->progress = 0.1f;
    }

    // Parse the scene file using SceneParser
    auto parsed = SceneParser::parse_file(path);
    if (!parsed) {
        scene_data.info.state = SceneLoadState::Error;
        if (progress) {
            progress->error = "Failed to parse scene: " + SceneParser::last_error();
        }
        return false;
    }

    scene_data.definition = std::move(*parsed);

    if (progress) {
        progress->current_stage = "Creating entities";
        progress->progress = 0.3f;
    }

    // Count total objects
    std::size_t total_objects = scene_data.definition.entities.size() +
                                scene_data.definition.lights.size() +
                                scene_data.definition.particle_emitters.size();

    if (progress) {
        progress->total_objects = total_objects;
    }

    // Create layers for this scene
    auto& layer_manager = LayerManager::instance();
    for (const auto& entity : scene_data.definition.entities) {
        // Ensure layer exists
        if (!entity.layer.empty()) {
            if (!layer_manager.has_layer(entity.layer)) {
                // Determine layer config based on name
                LayerConfig config = LayerConfig::content();
                if (entity.layer == "ui" || entity.layer == "hud") {
                    config = LayerConfig::overlay();
                } else if (entity.layer == "transparent" || entity.layer == "particles") {
                    config = LayerConfig::content(10);
                    config.clear_mode = ClearMode::None;
                } else if (entity.layer == "background") {
                    config = LayerConfig::content(-50);
                } else if (entity.layer == "debug") {
                    config = LayerConfig::debug();
                }
                layer_manager.create_layer(entity.layer, config);
            }
        }
    }

    // Process entities (would integrate with ECS in full implementation)
    std::size_t objects_processed = 0;
    for (const auto& entity_def : scene_data.definition.entities) {
        // In a full implementation, this would:
        // 1. Create ECS entity
        // 2. Add transform component
        // 3. Add mesh/material components
        // 4. Add physics components
        // 5. Add game system components (health, weapon, AI, etc.)
        // 6. Assign entity to appropriate layer

        // For now, generate placeholder entity IDs
        static std::uint64_t entity_counter = 1;
        std::uint64_t entity_id = entity_counter++;
        scene_data.entity_ids.push_back(entity_id);

        // Assign to layer
        if (!entity_def.layer.empty()) {
            layer_manager.assign_entity_to_layer(entity_id, entity_def.layer);
        } else {
            layer_manager.assign_entity_to_layer(entity_id, "world");
        }

        objects_processed++;
        if (progress) {
            progress->objects_loaded = objects_processed;
            progress->progress = 0.3f + 0.6f * objects_processed / std::max(total_objects, std::size_t(1));
        }
    }

    // Process lights
    for (const auto& light_def : scene_data.definition.lights) {
        // Would create light entities/components
        objects_processed++;
        if (progress) {
            progress->objects_loaded = objects_processed;
            progress->progress = 0.3f + 0.6f * objects_processed / std::max(total_objects, std::size_t(1));
        }
    }

    // Process particle emitters
    for (const auto& emitter_def : scene_data.definition.particle_emitters) {
        // Would create particle system entities
        objects_processed++;
        if (progress) {
            progress->objects_loaded = objects_processed;
            progress->progress = 0.3f + 0.6f * objects_processed / std::max(total_objects, std::size_t(1));
        }
    }

    if (progress) {
        progress->current_stage = "Finalizing";
        progress->progress = 0.95f;
    }

    // Update scene info from parsed definition
    scene_data.info.entity_count = scene_data.entity_ids.size();

    // Estimate memory usage
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    scene_data.info.memory_usage = file ? static_cast<std::size_t>(file.tellg()) : std::size_t{0};

    // Mark as loaded
    scene_data.info.state = SceneLoadState::Loaded;
    scene_data.info.is_active = (mode != SceneLoadMode::Background);

    // Store scene
    loaded_scenes_[name] = std::move(scene_data);

    // Update current scene
    if (mode == SceneLoadMode::Single || mode == SceneLoadMode::Additive) {
        current_scene_ = name;
    }

    // Record timestamp for hot reload
    if (hot_reload_enabled_) {
        file_timestamps_[name] = std::filesystem::last_write_time(path);
    }

    if (progress) {
        progress->current_stage = "Complete";
        progress->progress = 1.0f;
        progress->completed = true;
    }

    return true;
}

const SceneDefinition* SceneLoader::get_scene_definition(const std::string& scene_name) const {
    auto it = loaded_scenes_.find(scene_name);
    return it != loaded_scenes_.end() ? &it->second.definition : nullptr;
}

const SceneDefinition* SceneLoader::active_scene_definition() const {
    if (current_scene_.empty()) return nullptr;
    return get_scene_definition(current_scene_);
}

void SceneLoader::unload_scene_internal(const std::string& scene_name) {
    auto it = loaded_scenes_.find(scene_name);
    if (it == loaded_scenes_.end()) {
        return;
    }

    it->second.info.state = SceneLoadState::Unloading;

    // Destroy entities
    for (auto entity_id : it->second.entity_ids) {
        // Would call ECS to destroy entity
        (void)entity_id;
    }

    // Remove from file timestamps
    file_timestamps_.erase(scene_name);

    // Notify
    if (scene_unloaded_callback_) {
        scene_unloaded_callback_(scene_name);
    }

    // Remove from loaded scenes
    loaded_scenes_.erase(it);

    // Update current scene if needed
    if (current_scene_ == scene_name) {
        current_scene_.clear();
        if (!loaded_scenes_.empty()) {
            current_scene_ = loaded_scenes_.begin()->first;
        }
    }
}

void SceneLoader::process_pending_unloads() {
    std::vector<std::string> to_unload;

    for (auto& [name, data] : loaded_scenes_) {
        if (data.pending_unload && !data.info.is_persistent) {
            to_unload.push_back(name);
        }
    }

    for (const auto& name : to_unload) {
        unload_scene_internal(name);
    }
}

// =============================================================================
// Scene Format Handlers
// =============================================================================

bool JsonSceneFormat::load(const std::filesystem::path& path, SceneLoadProgress* progress) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    // Simple implementation - real version would use JSON parser
    if (progress) {
        progress->current_stage = "Parsing JSON";
        progress->progress = 0.5f;
    }

    return true;
}

bool JsonSceneFormat::save(const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    // Write empty scene
    file << "{\n  \"entities\": []\n}\n";
    return true;
}

bool BinarySceneFormat::load(const std::filesystem::path& path, SceneLoadProgress* progress) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    if (progress) {
        progress->current_stage = "Loading binary";
        progress->progress = 0.5f;
    }

    return true;
}

bool BinarySceneFormat::save(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    // Write header
    const char header[] = "VSCN";
    file.write(header, 4);

    // Write version
    std::uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Write entity count (0)
    std::uint32_t count = 0;
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    return true;
}

// =============================================================================
// SceneBuilder Implementation
// =============================================================================

SceneBuilder::SceneBuilder(const std::string& name) : scene_name_(name) {}

SceneBuilder& SceneBuilder::entity(const std::string& name) {
    entities_.push_back(EntityData{});
    current_entity_ = &entities_.back();
    current_entity_->name = name;
    return *this;
}

SceneBuilder& SceneBuilder::component(const std::string& type, const std::string& data) {
    if (current_entity_) {
        current_entity_->components.emplace_back(type, data);
    }
    return *this;
}

SceneBuilder& SceneBuilder::prefab(const std::string& prefab_name) {
    if (current_entity_) {
        current_entity_->prefab = prefab_name;
    }
    return *this;
}

SceneBuilder& SceneBuilder::position(float x, float y, float z) {
    if (current_entity_) {
        current_entity_->position[0] = x;
        current_entity_->position[1] = y;
        current_entity_->position[2] = z;
    }
    return *this;
}

SceneBuilder& SceneBuilder::rotation(float x, float y, float z, float w) {
    if (current_entity_) {
        current_entity_->rotation[0] = x;
        current_entity_->rotation[1] = y;
        current_entity_->rotation[2] = z;
        current_entity_->rotation[3] = w;
    }
    return *this;
}

SceneBuilder& SceneBuilder::scale(float x, float y, float z) {
    if (current_entity_) {
        current_entity_->scale_values[0] = x;
        current_entity_->scale_values[1] = y;
        current_entity_->scale_values[2] = z;
    }
    return *this;
}

SceneBuilder& SceneBuilder::parent(const std::string& parent_name) {
    if (current_entity_) {
        current_entity_->parent = parent_name;
    }
    return *this;
}

SceneBuilder& SceneBuilder::tag(const std::string& t) {
    if (current_entity_) {
        current_entity_->tags.push_back(t);
    }
    return *this;
}

bool SceneBuilder::build() {
    // Create entities in ECS
    // This would integrate with the ECS system
    return true;
}

bool SceneBuilder::save(const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    // Write as JSON
    file << "{\n";
    file << "  \"name\": \"" << scene_name_ << "\",\n";
    file << "  \"entities\": [\n";

    for (std::size_t i = 0; i < entities_.size(); ++i) {
        const auto& entity = entities_[i];
        file << "    {\n";
        file << "      \"name\": \"" << entity.name << "\"";

        if (!entity.parent.empty()) {
            file << ",\n      \"parent\": \"" << entity.parent << "\"";
        }

        if (!entity.prefab.empty()) {
            file << ",\n      \"prefab\": \"" << entity.prefab << "\"";
        }

        file << ",\n      \"position\": [" << entity.position[0] << ", "
             << entity.position[1] << ", " << entity.position[2] << "]";

        file << ",\n      \"rotation\": [" << entity.rotation[0] << ", "
             << entity.rotation[1] << ", " << entity.rotation[2] << ", "
             << entity.rotation[3] << "]";

        file << ",\n      \"scale\": [" << entity.scale_values[0] << ", "
             << entity.scale_values[1] << ", " << entity.scale_values[2] << "]";

        if (!entity.components.empty()) {
            file << ",\n      \"components\": [\n";
            for (std::size_t j = 0; j < entity.components.size(); ++j) {
                file << "        {\"type\": \"" << entity.components[j].first
                     << "\", \"data\": " << entity.components[j].second << "}";
                if (j < entity.components.size() - 1) file << ",";
                file << "\n";
            }
            file << "      ]";
        }

        if (!entity.tags.empty()) {
            file << ",\n      \"tags\": [";
            for (std::size_t j = 0; j < entity.tags.size(); ++j) {
                file << "\"" << entity.tags[j] << "\"";
                if (j < entity.tags.size() - 1) file << ", ";
            }
            file << "]";
        }

        file << "\n    }";
        if (i < entities_.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    return true;
}

} // namespace void_runtime
