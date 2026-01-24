#pragma once

/// @file instance.hpp
/// @brief C++ class instance management for void_cpp module

#include "types.hpp"
#include "module.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_cpp {

// =============================================================================
// World Context (Engine API for C++ code)
// =============================================================================

/// @brief World context providing engine API to C++ code
struct FfiWorldContext {
    void* world_ptr = nullptr;

    // Entity operations
    FfiEntityId (*spawn_entity)(void* world, const char* prefab_name) = nullptr;
    void (*destroy_entity)(void* world, FfiEntityId entity) = nullptr;
    bool (*entity_exists)(void* world, FfiEntityId entity) = nullptr;
    FfiVec3 (*get_entity_position)(void* world, FfiEntityId entity) = nullptr;
    void (*set_entity_position)(void* world, FfiEntityId entity, FfiVec3 pos) = nullptr;
    FfiQuat (*get_entity_rotation)(void* world, FfiEntityId entity) = nullptr;
    void (*set_entity_rotation)(void* world, FfiEntityId entity, FfiQuat rot) = nullptr;
    FfiVec3 (*get_entity_scale)(void* world, FfiEntityId entity) = nullptr;
    void (*set_entity_scale)(void* world, FfiEntityId entity, FfiVec3 scale) = nullptr;

    // Physics
    void (*apply_force)(void* world, FfiEntityId entity, FfiVec3 force) = nullptr;
    void (*apply_impulse)(void* world, FfiEntityId entity, FfiVec3 impulse) = nullptr;
    void (*set_velocity)(void* world, FfiEntityId entity, FfiVec3 velocity) = nullptr;
    FfiVec3 (*get_velocity)(void* world, FfiEntityId entity) = nullptr;
    FfiHitResult (*raycast)(void* world, FfiVec3 origin, FfiVec3 direction, float max_dist) = nullptr;

    // Audio
    void (*play_sound)(void* world, const char* sound_name) = nullptr;
    void (*play_sound_at)(void* world, const char* sound_name, FfiVec3 position) = nullptr;
    void (*stop_sound)(void* world, const char* sound_name) = nullptr;

    // Logging
    void (*log_message)(void* world, int level, const char* message) = nullptr;

    // Time
    float (*get_delta_time)(void* world) = nullptr;
    double (*get_time)(void* world) = nullptr;
    std::uint64_t (*get_frame_count)(void* world) = nullptr;

    // Input
    bool (*is_key_pressed)(void* world, int key_code) = nullptr;
    bool (*is_key_just_pressed)(void* world, int key_code) = nullptr;
    bool (*is_mouse_button_pressed)(void* world, int button) = nullptr;
    FfiVec3 (*get_mouse_position)(void* world) = nullptr;
};

// =============================================================================
// C++ Library
// =============================================================================

/// @brief Wrapper for a loaded C++ library
class CppLibrary {
public:
    CppLibrary(ModuleId module_id, const std::filesystem::path& path);
    ~CppLibrary();

    // Non-copyable, movable
    CppLibrary(const CppLibrary&) = delete;
    CppLibrary& operator=(const CppLibrary&) = delete;
    CppLibrary(CppLibrary&&) noexcept;
    CppLibrary& operator=(CppLibrary&&) noexcept;

    // Identity
    [[nodiscard]] ModuleId module_id() const { return module_id_; }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }
    [[nodiscard]] const FfiLibraryInfo& info() const { return info_; }

    // Class access
    [[nodiscard]] bool has_class(const std::string& name) const;
    [[nodiscard]] const FfiClassInfo* get_class_info(const std::string& name) const;
    [[nodiscard]] const FfiClassVTable* get_class_vtable(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> class_names() const;

    // Instance creation
    [[nodiscard]] CppHandle create_instance(const std::string& class_name) const;
    void destroy_instance(const std::string& class_name, CppHandle handle) const;

    // Entity/world context
    void set_instance_entity(CppHandle handle, FfiEntityId entity) const;
    void set_instance_world_context(CppHandle handle, const FfiWorldContext* context) const;

    // Validation
    [[nodiscard]] bool is_valid() const { return valid_; }

private:
    ModuleId module_id_;
    std::filesystem::path path_;
    FfiLibraryInfo info_{};
    bool valid_ = false;

    // Function pointers
    GetLibraryInfoFn get_library_info_ = nullptr;
    GetClassInfoFn get_class_info_ = nullptr;
    GetClassVTableFn get_class_vtable_ = nullptr;
    SetEntityIdFn set_entity_id_ = nullptr;
    SetWorldContextFn set_world_context_ = nullptr;

    // Cached class info
    mutable std::unordered_map<std::string, const FfiClassInfo*> class_cache_;
    mutable std::unordered_map<std::string, const FfiClassVTable*> vtable_cache_;
};

// =============================================================================
// C++ Class Instance
// =============================================================================

/// @brief Individual C++ object instance
class CppClassInstance {
public:
    CppClassInstance(InstanceId id, const std::string& class_name,
                     CppHandle handle, CppLibrary* library);
    ~CppClassInstance();

    // Non-copyable, movable
    CppClassInstance(const CppClassInstance&) = delete;
    CppClassInstance& operator=(const CppClassInstance&) = delete;
    CppClassInstance(CppClassInstance&&) noexcept;
    CppClassInstance& operator=(CppClassInstance&&) noexcept;

    // Identity
    [[nodiscard]] InstanceId id() const { return id_; }
    [[nodiscard]] const std::string& class_name() const { return class_name_; }
    [[nodiscard]] CppHandle handle() const { return handle_; }
    [[nodiscard]] InstanceState state() const { return state_; }
    [[nodiscard]] FfiEntityId entity_id() const { return entity_id_; }

    // Entity association
    void set_entity(FfiEntityId entity);

    // Properties
    [[nodiscard]] const PropertyMap& properties() const { return properties_; }
    void set_property(const std::string& name, PropertyValue value);
    [[nodiscard]] const PropertyValue* get_property(const std::string& name) const;

    // Lifecycle
    void begin_play();
    void tick(float delta_time);
    void fixed_tick(float delta_time);
    void end_play();

    // Events
    void on_collision_enter(FfiEntityId other, FfiHitResult hit);
    void on_collision_exit(FfiEntityId other);
    void on_trigger_enter(FfiEntityId other);
    void on_trigger_exit(FfiEntityId other);
    void on_damage(FfiDamageInfo damage);
    void on_death(FfiEntityId killer);
    void on_interact(FfiEntityId interactor);
    void on_input_action(FfiInputAction action);

    // Hot-reload serialization
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;
    bool deserialize(const std::vector<std::uint8_t>& data);
    void on_reload();

    // Library association (for hot-reload)
    void set_library(CppLibrary* library) { library_ = library; }
    [[nodiscard]] CppLibrary* library() const { return library_; }

private:
    InstanceId id_;
    std::string class_name_;
    CppHandle handle_;
    CppLibrary* library_;
    InstanceState state_ = InstanceState::Created;
    FfiEntityId entity_id_{};
    PropertyMap properties_;
    bool begun_ = false;
};

// =============================================================================
// C++ Class Registry
// =============================================================================

/// @brief Central registry for C++ libraries and instances
class CppClassRegistry {
public:
    CppClassRegistry();
    ~CppClassRegistry();

    // Singleton access
    [[nodiscard]] static CppClassRegistry& instance();

    // ==========================================================================
    // Library Management
    // ==========================================================================

    /// @brief Load a C++ library
    CppResult<CppLibrary*> load_library(const std::filesystem::path& path);

    /// @brief Unload a library
    bool unload_library(const std::filesystem::path& path);
    bool unload_library(ModuleId module_id);

    /// @brief Get a loaded library
    [[nodiscard]] CppLibrary* get_library(const std::filesystem::path& path);
    [[nodiscard]] CppLibrary* get_library(ModuleId module_id);

    /// @brief Check if library is loaded
    [[nodiscard]] bool is_library_loaded(const std::filesystem::path& path) const;

    /// @brief Get all loaded library paths
    [[nodiscard]] std::vector<std::filesystem::path> loaded_libraries() const;

    /// @brief Check if a class exists
    [[nodiscard]] bool has_class(const std::string& name) const;

    /// @brief Get all class names
    [[nodiscard]] std::vector<std::string> class_names() const;

    // ==========================================================================
    // Instance Management
    // ==========================================================================

    /// @brief Create a new instance
    CppResult<CppClassInstance*> create_instance(const std::string& class_name,
                                                  FfiEntityId entity = FfiEntityId::invalid(),
                                                  const PropertyMap& properties = {});

    /// @brief Destroy an instance
    bool destroy_instance(InstanceId id);

    /// @brief Destroy instance for entity
    bool destroy_instance_for_entity(FfiEntityId entity);

    /// @brief Get an instance
    [[nodiscard]] CppClassInstance* get_instance(InstanceId id);
    [[nodiscard]] const CppClassInstance* get_instance(InstanceId id) const;

    /// @brief Get instance for entity
    [[nodiscard]] CppClassInstance* get_instance_for_entity(FfiEntityId entity);

    /// @brief Get all instances
    [[nodiscard]] std::vector<CppClassInstance*> instances();

    /// @brief Get instance count
    [[nodiscard]] std::size_t instance_count() const;

    // ==========================================================================
    // Lifecycle
    // ==========================================================================

    /// @brief Call BeginPlay on all instances
    void begin_play_all();

    /// @brief Call Tick on all active instances
    void tick_all(float delta_time);

    /// @brief Call FixedTick on all active instances
    void fixed_tick_all(float delta_time);

    /// @brief Call EndPlay on all instances
    void end_play_all();

    // ==========================================================================
    // World Context
    // ==========================================================================

    /// @brief Set the world context for all instances
    void set_world_context(const FfiWorldContext* context);

    /// @brief Get the current world context
    [[nodiscard]] const FfiWorldContext* world_context() const { return world_context_; }

    // ==========================================================================
    // Hot-Reload
    // ==========================================================================

    /// @brief Prepare for reload (save all instance states)
    struct SavedInstanceState {
        InstanceId id;
        FfiEntityId entity;
        std::string class_name;
        PropertyMap properties;
        std::vector<std::uint8_t> serialized_data;
    };

    /// @brief Save instances before reload
    std::vector<SavedInstanceState> prepare_reload(const std::filesystem::path& library_path);

    /// @brief Complete reload (restore instances)
    void complete_reload(const std::filesystem::path& library_path,
                        const std::vector<SavedInstanceState>& saved_states);

private:
    mutable std::mutex mutex_;

    // Libraries
    std::unordered_map<std::filesystem::path, std::unique_ptr<CppLibrary>> libraries_;
    std::unordered_map<ModuleId, std::filesystem::path> module_to_path_;
    std::unordered_map<std::string, std::filesystem::path> class_to_library_;

    // Instances
    std::unordered_map<InstanceId, std::unique_ptr<CppClassInstance>> instances_;
    std::unordered_map<FfiEntityId, InstanceId> entity_to_instance_;

    // World context
    const FfiWorldContext* world_context_ = nullptr;

    // ID generation
    inline static std::atomic<InstanceId> next_instance_id_{1};
};

} // namespace void_cpp
