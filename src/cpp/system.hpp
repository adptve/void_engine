#pragma once

/// @file system.hpp
/// @brief Main C++ runtime system

#include "hot_reload.hpp"
#include "instance.hpp"

#include <void_engine/event/event.hpp>

namespace void_cpp {

// =============================================================================
// C++ System Events
// =============================================================================

/// @brief Event: Module loaded
struct ModuleLoadedEvent {
    ModuleId module_id;
    std::string module_name;
    std::filesystem::path path;
};

/// @brief Event: Module unloaded
struct ModuleUnloadedEvent {
    ModuleId module_id;
    std::string module_name;
};

/// @brief Event: Compilation started
struct CompilationStartedEvent {
    CompileJobId job_id;
    std::string output_name;
};

/// @brief Event: Compilation completed
struct CompilationCompletedEvent {
    CompileJobId job_id;
    std::string output_name;
    bool success;
    std::size_t error_count;
    std::size_t warning_count;
};

// =============================================================================
// C++ System
// =============================================================================

/// @brief Main C++ runtime compilation and hot reload system
class CppSystem {
public:
    CppSystem();
    ~CppSystem();

    // Singleton access
    [[nodiscard]] static CppSystem& instance();
    [[nodiscard]] static CppSystem* instance_ptr();

    // ==========================================================================
    // Initialization
    // ==========================================================================

    /// @brief Initialize the system
    void initialize(const CompilerConfig& config = {});

    /// @brief Shutdown the system
    void shutdown();

    /// @brief Check if initialized
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    // ==========================================================================
    // Subsystems
    // ==========================================================================

    /// @brief Get compiler
    [[nodiscard]] Compiler& compiler() { return *compiler_; }
    [[nodiscard]] const Compiler& compiler() const { return *compiler_; }

    /// @brief Get module registry
    [[nodiscard]] ModuleRegistry& modules() { return *modules_; }
    [[nodiscard]] const ModuleRegistry& modules() const { return *modules_; }

    /// @brief Get hot reloader
    [[nodiscard]] HotReloader& hot_reloader() { return *hot_reloader_; }
    [[nodiscard]] const HotReloader& hot_reloader() const { return *hot_reloader_; }

    /// @brief Get class registry
    [[nodiscard]] CppClassRegistry& class_registry() { return CppClassRegistry::instance(); }
    [[nodiscard]] const CppClassRegistry& class_registry() const { return CppClassRegistry::instance(); }

    // ==========================================================================
    // Instance Lifecycle
    // ==========================================================================

    /// @brief Call BeginPlay on all instances
    void begin_play_all() { CppClassRegistry::instance().begin_play_all(); }

    /// @brief Call Tick on all active instances
    void tick_all(float delta_time) { CppClassRegistry::instance().tick_all(delta_time); }

    /// @brief Call FixedTick on all active instances
    void fixed_tick_all(float delta_time) { CppClassRegistry::instance().fixed_tick_all(delta_time); }

    /// @brief Call EndPlay on all instances
    void end_play_all() { CppClassRegistry::instance().end_play_all(); }

    /// @brief Set world context for all instances
    void set_world_context(const FfiWorldContext* context) { CppClassRegistry::instance().set_world_context(context); }

    // ==========================================================================
    // Quick Access
    // ==========================================================================

    /// @brief Compile and load a module
    CppResult<DynamicModule*> compile_and_load(
        const std::vector<std::filesystem::path>& sources,
        const std::string& module_name);

    /// @brief Load a pre-compiled module
    CppResult<DynamicModule*> load_module(const std::filesystem::path& path);

    /// @brief Load a pre-compiled module with a name
    CppResult<DynamicModule*> load_module(
        const std::string& name,
        const std::filesystem::path& path);

    /// @brief Unload a module
    bool unload_module(ModuleId id);

    /// @brief Reload a module
    CppResult<void> reload_module(ModuleId id);

    // ==========================================================================
    // Hot Reload
    // ==========================================================================

    /// @brief Enable hot reload
    void enable_hot_reload(bool enable = true);

    /// @brief Check if hot reload is enabled
    [[nodiscard]] bool is_hot_reload_enabled() const;

    /// @brief Add source directory for hot reload watching
    void add_source_directory(const std::filesystem::path& dir);

    /// @brief Register module sources for hot reload
    void register_module_sources(
        ModuleId module_id,
        const std::vector<std::filesystem::path>& sources);

    // ==========================================================================
    // Update
    // ==========================================================================

    /// @brief Update the system (call each frame)
    void update(float delta_time);

    // ==========================================================================
    // Events
    // ==========================================================================

    /// @brief Set event bus
    void set_event_bus(void_event::EventBus* bus);

    /// @brief Get event bus
    [[nodiscard]] void_event::EventBus* event_bus() const { return event_bus_; }

    // ==========================================================================
    // Statistics
    // ==========================================================================

    struct Stats {
        std::size_t modules_loaded = 0;
        std::size_t compilations_total = 0;
        std::size_t compilations_failed = 0;
        std::size_t reloads_total = 0;
        std::size_t reloads_successful = 0;
        std::chrono::milliseconds total_compile_time{0};
    };

    [[nodiscard]] Stats stats() const;

private:
    std::unique_ptr<Compiler> compiler_;
    std::unique_ptr<ModuleRegistry> modules_;
    std::unique_ptr<HotReloader> hot_reloader_;

    void_event::EventBus* event_bus_ = nullptr;
    bool initialized_ = false;

    mutable Stats stats_;
};

// =============================================================================
// Prelude Namespace
// =============================================================================

namespace prelude {

using void_cpp::CppSystem;
using void_cpp::Compiler;
using void_cpp::CompilerConfig;
using void_cpp::CompileResult;
using void_cpp::DynamicModule;
using void_cpp::ModuleRegistry;
using void_cpp::HotReloader;
using void_cpp::FileWatcher;
using void_cpp::StatePreserver;

// Instance management
using void_cpp::CppLibrary;
using void_cpp::CppClassInstance;
using void_cpp::CppClassRegistry;
using void_cpp::CppHandle;
using void_cpp::FfiEntityId;
using void_cpp::FfiVec3;
using void_cpp::FfiQuat;
using void_cpp::FfiTransform;
using void_cpp::FfiWorldContext;
using void_cpp::FfiClassInfo;
using void_cpp::FfiClassVTable;
using void_cpp::PropertyValue;
using void_cpp::PropertyMap;
using void_cpp::InstanceId;
using void_cpp::InstanceState;

using void_cpp::CompilerType;
using void_cpp::BuildConfig;
using void_cpp::CppStandard;
using void_cpp::ModuleState;
using void_cpp::CppError;

} // namespace prelude

} // namespace void_cpp
