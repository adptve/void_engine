/// @file scene_loader.hpp
/// @brief Scene loading and management for void_runtime

#pragma once

#include "fwd.hpp"
#include "scene_types.hpp"
#include "layer.hpp"

#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace void_runtime {

// =============================================================================
// Scene Types
// =============================================================================

/// @brief Scene load state
enum class SceneLoadState {
    Unloaded,
    Loading,
    Loaded,
    Unloading,
    Error
};

/// @brief Scene load mode
enum class SceneLoadMode {
    Single,      // Replace current scene
    Additive,    // Add to current scene
    Background   // Load in background, don't activate
};

/// @brief Scene information
struct SceneInfo {
    std::string name;
    std::filesystem::path path;
    SceneLoadState state = SceneLoadState::Unloaded;
    std::size_t entity_count = 0;
    std::size_t memory_usage = 0;
    bool is_active = false;
    bool is_persistent = false;  // Don't unload on scene change
};

/// @brief Scene load progress
struct SceneLoadProgress {
    std::string current_stage;
    float progress = 0.0f;  // 0 to 1
    std::size_t objects_loaded = 0;
    std::size_t total_objects = 0;
    bool completed = false;
    std::string error;
};

// =============================================================================
// Scene Callbacks
// =============================================================================

using SceneLoadCallback = std::function<void(const std::string& scene_name, bool success)>;
using SceneProgressCallback = std::function<void(const SceneLoadProgress& progress)>;
using SceneUnloadCallback = std::function<void(const std::string& scene_name)>;

// =============================================================================
// Scene Loader
// =============================================================================

/// @brief Scene loading and management system
class SceneLoader {
public:
    SceneLoader();
    ~SceneLoader();

    // Non-copyable
    SceneLoader(const SceneLoader&) = delete;
    SceneLoader& operator=(const SceneLoader&) = delete;

    // ==========================================================================
    // Initialization
    // ==========================================================================

    /// @brief Initialize the scene loader
    bool initialize();

    /// @brief Shutdown the scene loader
    void shutdown();

    /// @brief Update (call every frame to process async operations)
    void update();

    // ==========================================================================
    // Scene Loading
    // ==========================================================================

    /// @brief Load a scene synchronously
    bool load_scene(const std::string& scene_name, SceneLoadMode mode = SceneLoadMode::Single);

    /// @brief Load a scene asynchronously
    void load_scene_async(const std::string& scene_name, SceneLoadMode mode = SceneLoadMode::Single,
                          SceneLoadCallback on_complete = nullptr,
                          SceneProgressCallback on_progress = nullptr);

    /// @brief Load a scene from file
    bool load_scene_from_file(const std::filesystem::path& path, SceneLoadMode mode = SceneLoadMode::Single);

    /// @brief Unload a scene
    void unload_scene(const std::string& scene_name);

    /// @brief Unload all scenes
    void unload_all_scenes();

    /// @brief Reload current scene
    void reload_current_scene();

    // ==========================================================================
    // Scene State
    // ==========================================================================

    /// @brief Get current active scene name
    std::string current_scene() const { return current_scene_; }

    /// @brief Check if a scene is loaded
    bool is_scene_loaded(const std::string& scene_name) const;

    /// @brief Get scene info
    const SceneInfo* get_scene_info(const std::string& scene_name) const;

    /// @brief Get all loaded scenes
    std::vector<std::string> loaded_scenes() const;

    /// @brief Get scene load state
    SceneLoadState get_load_state(const std::string& scene_name) const;

    /// @brief Get load progress for async loading
    SceneLoadProgress get_load_progress() const { return current_progress_; }

    /// @brief Check if any scene is loading
    bool is_loading() const;

    // ==========================================================================
    // Scene Search Paths
    // ==========================================================================

    /// @brief Add a scene search path
    void add_search_path(const std::filesystem::path& path);

    /// @brief Remove a scene search path
    void remove_search_path(const std::filesystem::path& path);

    /// @brief Get all search paths
    std::vector<std::filesystem::path> search_paths() const { return search_paths_; }

    /// @brief Find scene file by name
    std::filesystem::path find_scene_file(const std::string& scene_name) const;

    // ==========================================================================
    // Scene Registry
    // ==========================================================================

    /// @brief Register a scene for discovery
    void register_scene(const std::string& name, const std::filesystem::path& path);

    /// @brief Unregister a scene
    void unregister_scene(const std::string& name);

    /// @brief Get all registered scenes
    std::vector<std::string> registered_scenes() const;

    // ==========================================================================
    // Persistent Scenes
    // ==========================================================================

    /// @brief Mark scene as persistent (won't be unloaded on scene change)
    void set_scene_persistent(const std::string& scene_name, bool persistent);

    /// @brief Check if scene is persistent
    bool is_scene_persistent(const std::string& scene_name) const;

    // ==========================================================================
    // Callbacks
    // ==========================================================================

    /// @brief Set callback for when any scene is loaded
    void set_scene_loaded_callback(SceneLoadCallback callback);

    /// @brief Set callback for when any scene is unloaded
    void set_scene_unloaded_callback(SceneUnloadCallback callback);

    // ==========================================================================
    // Hot Reload
    // ==========================================================================

    /// @brief Enable scene hot reloading (watch for file changes)
    void enable_hot_reload(bool enable);

    /// @brief Check if hot reload is enabled
    bool is_hot_reload_enabled() const { return hot_reload_enabled_; }

    /// @brief Manually trigger hot reload check
    void check_for_changes();

    /// @brief Get parsed scene definition
    const SceneDefinition* get_scene_definition(const std::string& scene_name) const;

    /// @brief Get active scene definition
    const SceneDefinition* active_scene_definition() const;

private:
    struct SceneData {
        SceneInfo info;
        SceneDefinition definition;
        std::vector<std::uint64_t> entity_ids;
        bool pending_unload = false;
    };

    std::unordered_map<std::string, SceneData> loaded_scenes_;
    std::unordered_map<std::string, std::filesystem::path> registered_scenes_;
    std::vector<std::filesystem::path> search_paths_;

    std::string current_scene_;
    SceneLoadProgress current_progress_;

    SceneLoadCallback scene_loaded_callback_;
    SceneUnloadCallback scene_unloaded_callback_;

    bool hot_reload_enabled_ = false;
    std::unordered_map<std::string, std::filesystem::file_time_type> file_timestamps_;

    // Async loading
    struct AsyncLoadTask {
        std::string scene_name;
        std::filesystem::path path;
        SceneLoadMode mode;
        SceneLoadCallback on_complete;
        SceneProgressCallback on_progress;
        std::future<bool> future;
    };
    std::unique_ptr<AsyncLoadTask> current_async_task_;

    // Internal methods
    bool load_scene_internal(const std::filesystem::path& path, const std::string& name,
                             SceneLoadMode mode, SceneLoadProgress* progress = nullptr);
    void unload_scene_internal(const std::string& scene_name);
    void process_pending_unloads();
};

// =============================================================================
// Scene Format Support
// =============================================================================

/// @brief Scene file format handler
class SceneFormatHandler {
public:
    virtual ~SceneFormatHandler() = default;

    /// @brief Get supported file extensions
    virtual std::vector<std::string> extensions() const = 0;

    /// @brief Load scene from file
    virtual bool load(const std::filesystem::path& path, SceneLoadProgress* progress) = 0;

    /// @brief Save scene to file
    virtual bool save(const std::filesystem::path& path) = 0;
};

/// @brief JSON scene format handler
class JsonSceneFormat : public SceneFormatHandler {
public:
    std::vector<std::string> extensions() const override { return {".json", ".scene"}; }
    bool load(const std::filesystem::path& path, SceneLoadProgress* progress) override;
    bool save(const std::filesystem::path& path) override;
};

/// @brief Binary scene format handler (faster loading)
class BinarySceneFormat : public SceneFormatHandler {
public:
    std::vector<std::string> extensions() const override { return {".vscene", ".bin"}; }
    bool load(const std::filesystem::path& path, SceneLoadProgress* progress) override;
    bool save(const std::filesystem::path& path) override;
};

// =============================================================================
// Scene Builder
// =============================================================================

/// @brief Fluent builder for creating scenes programmatically
class SceneBuilder {
public:
    SceneBuilder(const std::string& name);

    SceneBuilder& entity(const std::string& name);
    SceneBuilder& component(const std::string& type, const std::string& data);
    SceneBuilder& prefab(const std::string& prefab_name);
    SceneBuilder& position(float x, float y, float z);
    SceneBuilder& rotation(float x, float y, float z, float w);
    SceneBuilder& scale(float x, float y, float z);
    SceneBuilder& parent(const std::string& parent_name);
    SceneBuilder& tag(const std::string& tag);

    /// @brief Build the scene
    bool build();

    /// @brief Save scene to file
    bool save(const std::filesystem::path& path);

private:
    struct EntityData {
        std::string name;
        std::string parent;
        std::vector<std::pair<std::string, std::string>> components;
        std::string prefab;
        float position[3] = {0, 0, 0};
        float rotation[4] = {0, 0, 0, 1};
        float scale_values[3] = {1, 1, 1};
        std::vector<std::string> tags;
    };

    std::string scene_name_;
    std::vector<EntityData> entities_;
    EntityData* current_entity_ = nullptr;
};

} // namespace void_runtime
