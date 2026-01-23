/// @file asset_loader.cpp
/// @brief Scene asset loading implementation

#include <void_engine/scene/asset_loader.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>

// Forward declare stb_image functions (defined in texture.cpp)
extern unsigned char* stbi_load(const char* filename, int* x, int* y, int* comp, int req_comp);
extern float* stbi_loadf(const char* filename, int* x, int* y, int* comp, int req_comp);
extern void stbi_image_free(void* data);

namespace void_scene {

// =============================================================================
// SceneAssetLoader Implementation
// =============================================================================

SceneAssetLoader::SceneAssetLoader() = default;

SceneAssetLoader::~SceneAssetLoader() {
    shutdown();
}

void_core::Result<void> SceneAssetLoader::initialize(const std::filesystem::path& base_path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_base_path = base_path;

    if (!std::filesystem::exists(base_path)) {
        spdlog::warn("Asset base path does not exist: {}", base_path.string());
    }

    spdlog::info("SceneAssetLoader initialized with base path: {}", base_path.string());
    return void_core::Ok();
}

void SceneAssetLoader::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_assets.clear();
    m_path_to_handle.clear();
    m_next_handle = 1;

    spdlog::info("SceneAssetLoader shut down");
}

void_core::Result<void> SceneAssetLoader::load_scene_assets(
    const SceneData& scene,
    ProgressCallback progress)
{
    // Collect all assets referenced in the scene
    std::vector<std::pair<std::string, AssetType>> assets_to_load;
    collect_scene_assets(scene, assets_to_load);

    LoadProgress prog;
    prog.total = static_cast<std::uint32_t>(assets_to_load.size());

    for (const auto& [path, type] : assets_to_load) {
        prog.current_asset = path;
        if (progress) progress(prog);

        AssetHandle handle;
        if (type == AssetType::Texture) {
            handle = load_texture(path);
        } else if (type == AssetType::Model) {
            handle = load_model(path);
        }

        if (!handle.is_valid()) {
            spdlog::warn("Failed to load asset: {}", path);
        }

        prog.loaded++;
        prog.percent = static_cast<float>(prog.loaded) / static_cast<float>(prog.total);
    }

    if (progress) progress(prog);

    spdlog::info("Loaded {} scene assets", prog.loaded);
    return void_core::Ok();
}

std::future<void_core::Result<void>> SceneAssetLoader::load_scene_assets_async(
    const SceneData& scene,
    ProgressCallback progress)
{
    return std::async(std::launch::async, [this, scene, progress]() {
        return load_scene_assets(scene, progress);
    });
}

AssetHandle SceneAssetLoader::load_texture(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if already loaded
    auto it = m_path_to_handle.find(path);
    if (it != m_path_to_handle.end()) {
        return AssetHandle{it->second, AssetType::Texture};
    }

    // Create new entry
    std::uint64_t handle_id = m_next_handle++;
    AssetEntry entry;
    entry.metadata.name = std::filesystem::path(path).filename().string();
    entry.metadata.path = path;
    entry.metadata.type = AssetType::Texture;
    entry.metadata.state = AssetState::Loading;

    // Actually load the texture
    auto result = load_texture_internal(entry);
    if (!result) {
        entry.metadata.state = AssetState::Failed;
        entry.metadata.error_message = result.error().message();

        if (m_on_failed) {
            m_on_failed(AssetHandle{handle_id, AssetType::Texture}, entry.metadata.error_message);
        }
    } else {
        entry.metadata.state = AssetState::Loaded;

        if (m_on_loaded) {
            m_on_loaded(AssetHandle{handle_id, AssetType::Texture}, entry.metadata);
        }
    }

    m_assets[handle_id] = std::move(entry);
    m_path_to_handle[path] = handle_id;

    return AssetHandle{handle_id, AssetType::Texture};
}

AssetHandle SceneAssetLoader::load_model(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if already loaded
    auto it = m_path_to_handle.find(path);
    if (it != m_path_to_handle.end()) {
        return AssetHandle{it->second, AssetType::Model};
    }

    // Create new entry
    std::uint64_t handle_id = m_next_handle++;
    AssetEntry entry;
    entry.metadata.name = std::filesystem::path(path).filename().string();
    entry.metadata.path = path;
    entry.metadata.type = AssetType::Model;
    entry.metadata.state = AssetState::Loading;

    // Actually load the model
    auto result = load_model_internal(entry);
    if (!result) {
        entry.metadata.state = AssetState::Failed;
        entry.metadata.error_message = result.error().message();

        if (m_on_failed) {
            m_on_failed(AssetHandle{handle_id, AssetType::Model}, entry.metadata.error_message);
        }
    } else {
        entry.metadata.state = AssetState::Loaded;

        if (m_on_loaded) {
            m_on_loaded(AssetHandle{handle_id, AssetType::Model}, entry.metadata);
        }
    }

    m_assets[handle_id] = std::move(entry);
    m_path_to_handle[path] = handle_id;

    return AssetHandle{handle_id, AssetType::Model};
}

void SceneAssetLoader::unload(AssetHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_assets.find(handle.id);
    if (it != m_assets.end()) {
        m_path_to_handle.erase(it->second.metadata.path);
        m_assets.erase(it);
    }
}

void SceneAssetLoader::unload_scene_assets(const std::filesystem::path& scene_path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::uint64_t> to_remove;
    for (const auto& [id, entry] : m_assets) {
        if (entry.scene_owner == scene_path) {
            to_remove.push_back(id);
        }
    }

    for (std::uint64_t id : to_remove) {
        auto it = m_assets.find(id);
        if (it != m_assets.end()) {
            m_path_to_handle.erase(it->second.metadata.path);
            m_assets.erase(it);
        }
    }
}

const LoadedTexture* SceneAssetLoader::get_texture(AssetHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (handle.type != AssetType::Texture) return nullptr;

    auto it = m_assets.find(handle.id);
    if (it != m_assets.end() && it->second.texture) {
        return it->second.texture.get();
    }
    return nullptr;
}

const LoadedModel* SceneAssetLoader::get_model(AssetHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (handle.type != AssetType::Model) return nullptr;

    auto it = m_assets.find(handle.id);
    if (it != m_assets.end() && it->second.model) {
        return it->second.model.get();
    }
    return nullptr;
}

const AssetMetadata* SceneAssetLoader::get_metadata(AssetHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_assets.find(handle.id);
    if (it != m_assets.end()) {
        return &it->second.metadata;
    }
    return nullptr;
}

bool SceneAssetLoader::is_loaded(AssetHandle handle) const {
    return get_state(handle) == AssetState::Loaded;
}

AssetState SceneAssetLoader::get_state(AssetHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_assets.find(handle.id);
    if (it != m_assets.end()) {
        return it->second.metadata.state;
    }
    return AssetState::Unloaded;
}

AssetHandle SceneAssetLoader::find_by_path(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_path_to_handle.find(path);
    if (it != m_path_to_handle.end()) {
        auto asset_it = m_assets.find(it->second);
        if (asset_it != m_assets.end()) {
            return AssetHandle{it->second, asset_it->second.metadata.type};
        }
    }
    return AssetHandle::invalid();
}

void SceneAssetLoader::update() {
    if (!m_hot_reload_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, entry] : m_assets) {
        if (entry.metadata.state != AssetState::Loaded) continue;

        auto full_path = resolve_path(entry.metadata.path);
        std::error_code ec;
        auto current_mtime = std::filesystem::last_write_time(full_path, ec);
        if (ec) continue;

        if (current_mtime != entry.metadata.last_modified) {
            entry.metadata.last_modified = current_mtime;
            entry.metadata.state = AssetState::Loading;

            // Reload
            void_core::Result<void> result;
            if (entry.metadata.type == AssetType::Texture) {
                result = load_texture_internal(entry);
            } else if (entry.metadata.type == AssetType::Model) {
                result = load_model_internal(entry);
            }

            if (!result) {
                entry.metadata.state = AssetState::Failed;
                entry.metadata.error_message = result.error().message();
                spdlog::warn("Failed to reload asset: {}", entry.metadata.path);
            } else {
                entry.metadata.state = AssetState::Loaded;
                spdlog::info("Reloaded asset: {}", entry.metadata.path);
            }
        }
    }
}

void SceneAssetLoader::reload_modified() {
    update();  // Same implementation for now
}

void SceneAssetLoader::on_asset_loaded(
    std::function<void(AssetHandle, const AssetMetadata&)> callback)
{
    m_on_loaded = std::move(callback);
}

void SceneAssetLoader::on_asset_failed(
    std::function<void(AssetHandle, const std::string&)> callback)
{
    m_on_failed = std::move(callback);
}

std::uint64_t SceneAssetLoader::total_memory_usage() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::uint64_t total = 0;
    for (const auto& [id, entry] : m_assets) {
        total += entry.metadata.size_bytes;
    }
    return total;
}

std::vector<AssetHandle> SceneAssetLoader::loaded_assets() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<AssetHandle> result;
    for (const auto& [id, entry] : m_assets) {
        if (entry.metadata.state == AssetState::Loaded) {
            result.push_back(AssetHandle{id, entry.metadata.type});
        }
    }
    return result;
}

void_core::Result<void> SceneAssetLoader::load_texture_internal(AssetEntry& entry) {
    auto full_path = resolve_path(entry.metadata.path);

    if (!std::filesystem::exists(full_path)) {
        return void_core::Err(void_core::Error("File not found: " + full_path.string()));
    }

    // Check file extension for HDR
    std::string ext = full_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    bool is_hdr = (ext == ".hdr" || ext == ".exr");

    entry.texture = std::make_unique<LoadedTexture>();
    entry.texture->name = entry.metadata.name;
    entry.texture->is_hdr = is_hdr;

    int width, height, channels;
    std::string path_str = full_path.string();

    if (is_hdr) {
        float* data = stbi_loadf(path_str.c_str(), &width, &height, &channels, 4);
        if (!data) {
            entry.texture.reset();
            return void_core::Err(void_core::Error("Failed to load HDR texture: " + path_str));
        }

        entry.texture->width = static_cast<std::uint32_t>(width);
        entry.texture->height = static_cast<std::uint32_t>(height);
        entry.texture->channels = 4;

        std::size_t size = width * height * 4 * sizeof(float);
        entry.texture->pixels.resize(size);
        std::memcpy(entry.texture->pixels.data(), data, size);

        stbi_image_free(data);
    } else {
        unsigned char* data = stbi_load(path_str.c_str(), &width, &height, &channels, 4);
        if (!data) {
            entry.texture.reset();
            return void_core::Err(void_core::Error("Failed to load texture: " + path_str));
        }

        entry.texture->width = static_cast<std::uint32_t>(width);
        entry.texture->height = static_cast<std::uint32_t>(height);
        entry.texture->channels = 4;

        std::size_t size = width * height * 4;
        entry.texture->pixels.resize(size);
        std::memcpy(entry.texture->pixels.data(), data, size);

        stbi_image_free(data);
    }

    entry.metadata.size_bytes = entry.texture->pixels.size();

    std::error_code ec;
    entry.metadata.last_modified = std::filesystem::last_write_time(full_path, ec);

    spdlog::debug("Loaded texture: {} ({}x{}, {} bytes)",
                  entry.metadata.name,
                  entry.texture->width, entry.texture->height,
                  entry.metadata.size_bytes);

    return void_core::Ok();
}

void_core::Result<void> SceneAssetLoader::load_model_internal(AssetEntry& entry) {
    auto full_path = resolve_path(entry.metadata.path);

    if (!std::filesystem::exists(full_path)) {
        return void_core::Err(void_core::Error("File not found: " + full_path.string()));
    }

    std::string ext = full_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    entry.model = std::make_unique<LoadedModel>();
    entry.model->name = entry.metadata.name;
    entry.model->source_path = full_path.string();

    // For now, we create a placeholder - actual glTF loading is in void_render
    // This allows the asset loader to track the model even if full loading
    // happens elsewhere

    if (ext == ".glb" || ext == ".gltf") {
        // glTF model - would integrate with tinygltf here
        // For now, mark as loaded placeholder

        std::error_code ec;
        entry.metadata.size_bytes = std::filesystem::file_size(full_path, ec);
        entry.metadata.last_modified = std::filesystem::last_write_time(full_path, ec);

        spdlog::debug("Loaded model reference: {} ({} bytes)",
                      entry.metadata.name, entry.metadata.size_bytes);
    } else if (ext == ".obj") {
        // OBJ model - would parse here
        spdlog::debug("OBJ model loading not fully implemented: {}", full_path.string());
    } else {
        return void_core::Err(void_core::Error("Unsupported model format: " + ext));
    }

    return void_core::Ok();
}

void SceneAssetLoader::collect_scene_assets(
    const SceneData& scene,
    std::vector<std::pair<std::string, AssetType>>& assets)
{
    // Collect textures from texture list
    for (const auto& tex : scene.textures) {
        if (!tex.path.empty()) {
            assets.emplace_back(tex.path, AssetType::Texture);
        }
    }

    // Collect textures and models from entities
    for (const auto& entity : scene.entities) {
        // Model path
        if (!entity.mesh.empty()) {
            // Check if it's a file path (contains / or .)
            if (entity.mesh.find('/') != std::string::npos ||
                entity.mesh.find('\\') != std::string::npos ||
                entity.mesh.find('.') != std::string::npos) {
                assets.emplace_back(entity.mesh, AssetType::Model);
            }
        }

        // Material textures
        if (entity.material) {
            if (entity.material->albedo.has_texture()) {
                assets.emplace_back(*entity.material->albedo.texture_path, AssetType::Texture);
            }
            if (entity.material->normal_map) {
                assets.emplace_back(*entity.material->normal_map, AssetType::Texture);
            }
            if (entity.material->metallic.has_texture()) {
                assets.emplace_back(*entity.material->metallic.texture_path, AssetType::Texture);
            }
            if (entity.material->roughness.has_texture()) {
                assets.emplace_back(*entity.material->roughness.texture_path, AssetType::Texture);
            }
        }
    }

    // Environment map
    if (scene.environment && scene.environment->environment_map) {
        assets.emplace_back(*scene.environment->environment_map, AssetType::Texture);
    }

    // Remove duplicates
    std::sort(assets.begin(), assets.end());
    assets.erase(std::unique(assets.begin(), assets.end()), assets.end());
}

std::filesystem::path SceneAssetLoader::resolve_path(const std::string& path) const {
    std::filesystem::path p(path);

    // If absolute path, use as-is
    if (p.is_absolute()) {
        return p;
    }

    // Otherwise, resolve relative to base path
    return m_base_path / p;
}

// =============================================================================
// AssetCache Implementation
// =============================================================================

AssetCache::AssetCache(std::uint64_t max_memory_bytes)
    : m_max_size(max_memory_bytes)
{
}

void AssetCache::add(AssetHandle handle, std::uint64_t size_bytes) {
    CacheEntry entry;
    entry.handle = handle;
    entry.size_bytes = size_bytes;
    entry.last_access = ++m_access_counter;

    m_entries[handle.id] = entry;
    m_current_size += size_bytes;
}

void AssetCache::touch(AssetHandle handle) {
    auto it = m_entries.find(handle.id);
    if (it != m_entries.end()) {
        it->second.last_access = ++m_access_counter;
    }
}

void AssetCache::remove(AssetHandle handle) {
    auto it = m_entries.find(handle.id);
    if (it != m_entries.end()) {
        m_current_size -= it->second.size_bytes;
        m_entries.erase(it);
    }
}

std::vector<AssetHandle> AssetCache::get_eviction_candidates(std::uint64_t required_bytes) {
    std::vector<AssetHandle> candidates;

    if (m_current_size + required_bytes <= m_max_size) {
        return candidates;  // No eviction needed
    }

    // Sort entries by last access (LRU)
    std::vector<CacheEntry*> sorted_entries;
    for (auto& [id, entry] : m_entries) {
        sorted_entries.push_back(&entry);
    }
    std::sort(sorted_entries.begin(), sorted_entries.end(),
              [](const CacheEntry* a, const CacheEntry* b) {
                  return a->last_access < b->last_access;
              });

    // Collect candidates until we have enough space
    std::uint64_t freed = 0;
    for (CacheEntry* entry : sorted_entries) {
        candidates.push_back(entry->handle);
        freed += entry->size_bytes;

        if (m_current_size - freed + required_bytes <= m_max_size) {
            break;
        }
    }

    return candidates;
}

void AssetCache::clear() {
    m_entries.clear();
    m_current_size = 0;
}

} // namespace void_scene
