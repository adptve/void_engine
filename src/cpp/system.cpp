/// @file system.cpp
/// @brief Main C++ runtime system implementation

#include "system.hpp"

#include <void_engine/core/log.hpp>

namespace void_cpp {

// =============================================================================
// CppSystem Implementation
// =============================================================================

namespace {
    CppSystem* g_system_instance = nullptr;
}

CppSystem::CppSystem() {
    g_system_instance = this;
}

CppSystem::~CppSystem() {
    if (initialized_) {
        shutdown();
    }
    if (g_system_instance == this) {
        g_system_instance = nullptr;
    }
}

CppSystem& CppSystem::instance() {
    if (!g_system_instance) {
        static CppSystem default_instance;
        g_system_instance = &default_instance;
    }
    return *g_system_instance;
}

CppSystem* CppSystem::instance_ptr() {
    return g_system_instance;
}

void CppSystem::initialize(const CompilerConfig& config) {
    if (initialized_) {
        VOID_LOG_WARN("[CppSystem] Already initialized");
        return;
    }

    VOID_LOG_INFO("[CppSystem] Initializing...");

    // Create compiler
    compiler_ = std::make_unique<Compiler>(config);

    // Create module registry
    modules_ = std::make_unique<ModuleRegistry>();

    // Create hot reloader
    hot_reloader_ = std::make_unique<HotReloader>(*compiler_, *modules_);

    // Setup hot reloader config
    hot_reloader_->set_compiler_config(config);

    initialized_ = true;

    VOID_LOG_INFO("[CppSystem] Initialized with compiler: {}",
                  static_cast<int>(compiler_->config().compiler));
}

void CppSystem::shutdown() {
    if (!initialized_) {
        return;
    }

    VOID_LOG_INFO("[CppSystem] Shutting down...");

    // Stop hot reload first
    if (hot_reloader_) {
        hot_reloader_->stop();
        hot_reloader_.reset();
    }

    // Unload all modules
    if (modules_) {
        modules_->unload_all();
        modules_.reset();
    }

    // Cleanup compiler
    compiler_.reset();

    initialized_ = false;

    VOID_LOG_INFO("[CppSystem] Shutdown complete");
}

CppResult<DynamicModule*> CppSystem::compile_and_load(
    const std::vector<std::filesystem::path>& sources,
    const std::string& module_name) {

    if (!initialized_) {
        return CppError::InvalidModule;
    }

    // Emit start event
    if (event_bus_) {
        CompilationStartedEvent event;
        event.output_name = module_name;
        event_bus_->publish(event);
    }

    stats_.compilations_total++;

    // Compile
    auto compile_result = compiler_->compile(sources, module_name);
    if (!compile_result) {
        stats_.compilations_failed++;

        if (event_bus_) {
            CompilationCompletedEvent event;
            event.output_name = module_name;
            event.success = false;
            event_bus_->publish(event);
        }

        return CppError::CompilationFailed;
    }

    const auto& result = compile_result.value();
    stats_.total_compile_time += result.compile_time + result.link_time;

    if (!result.success()) {
        stats_.compilations_failed++;

        if (event_bus_) {
            CompilationCompletedEvent event;
            event.output_name = module_name;
            event.success = false;
            event.error_count = result.error_count;
            event.warning_count = result.warning_count;
            event_bus_->publish(event);
        }

        return CppError::CompilationFailed;
    }

    // Emit completion event
    if (event_bus_) {
        CompilationCompletedEvent event;
        event.output_name = module_name;
        event.success = true;
        event.error_count = result.error_count;
        event.warning_count = result.warning_count;
        event_bus_->publish(event);
    }

    // Load
    auto load_result = modules_->load(module_name, result.output_path);
    if (!load_result) {
        return CppError::LoadFailed;
    }

    auto* module = load_result.value();

    // Emit loaded event
    if (event_bus_) {
        ModuleLoadedEvent event;
        event.module_id = module->id();
        event.module_name = module->name();
        event.path = module->path();
        event_bus_->publish(event);
    }

    // Register for hot reload
    hot_reloader_->register_module(module->id(), sources);

    return module;
}

CppResult<DynamicModule*> CppSystem::load_module(const std::filesystem::path& path) {
    return load_module(path.stem().string(), path);
}

CppResult<DynamicModule*> CppSystem::load_module(
    const std::string& name,
    const std::filesystem::path& path) {

    if (!initialized_) {
        return CppError::InvalidModule;
    }

    auto result = modules_->load(name, path);
    if (!result) {
        return result;
    }

    auto* module = result.value();

    // Emit event
    if (event_bus_) {
        ModuleLoadedEvent event;
        event.module_id = module->id();
        event.module_name = module->name();
        event.path = module->path();
        event_bus_->publish(event);
    }

    return result;
}

bool CppSystem::unload_module(ModuleId id) {
    if (!initialized_) {
        return false;
    }

    auto* module = modules_->get(id);
    if (!module) {
        return false;
    }

    std::string name = module->name();

    // Unregister from hot reload
    hot_reloader_->unregister_module(id);

    // Unload
    bool success = modules_->unload(id);

    if (success && event_bus_) {
        ModuleUnloadedEvent event;
        event.module_id = id;
        event.module_name = name;
        event_bus_->publish(event);
    }

    return success;
}

CppResult<void> CppSystem::reload_module(ModuleId id) {
    if (!initialized_) {
        return CppError::InvalidModule;
    }

    return hot_reloader_->reload(id);
}

void CppSystem::enable_hot_reload(bool enable) {
    if (!initialized_) return;

    hot_reloader_->set_enabled(enable);

    if (enable) {
        hot_reloader_->start();
    } else {
        hot_reloader_->stop();
    }
}

bool CppSystem::is_hot_reload_enabled() const {
    return initialized_ && hot_reloader_->is_enabled();
}

void CppSystem::add_source_directory(const std::filesystem::path& dir) {
    if (!initialized_) return;

    hot_reloader_->add_source_directory(dir);
}

void CppSystem::register_module_sources(
    ModuleId module_id,
    const std::vector<std::filesystem::path>& sources) {

    if (!initialized_) return;

    hot_reloader_->register_module(module_id, sources);
}

void CppSystem::update(float delta_time) {
    if (!initialized_) return;

    hot_reloader_->update();
}

void CppSystem::set_event_bus(void_event::EventBus* bus) {
    event_bus_ = bus;
    if (hot_reloader_) {
        hot_reloader_->set_event_bus(bus);
    }
}

CppSystem::Stats CppSystem::stats() const {
    Stats s = stats_;

    if (modules_) {
        s.modules_loaded = modules_->modules().size();
    }

    if (hot_reloader_) {
        auto hr_stats = hot_reloader_->stats();
        s.reloads_total = hr_stats.total_reloads;
        s.reloads_successful = hr_stats.successful_reloads;
    }

    return s;
}

} // namespace void_cpp
