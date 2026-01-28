/// @file kernel.cpp
/// @brief Core kernel implementation

#include <void_engine/kernel/kernel.hpp>

#include <algorithm>
#include <numeric>
#include <thread>

namespace void_kernel {

// =============================================================================
// Global Kernel
// =============================================================================

static IKernel* g_kernel = nullptr;

IKernel* global_kernel() {
    return g_kernel;
}

void set_global_kernel(IKernel* kernel) {
    g_kernel = kernel;
}

GlobalKernelGuard::GlobalKernelGuard(IKernel& kernel) : m_previous(g_kernel) {
    g_kernel = &kernel;
}

GlobalKernelGuard::~GlobalKernelGuard() {
    g_kernel = m_previous;
}

// =============================================================================
// Kernel Implementation
// =============================================================================

Kernel::Kernel(KernelConfig config)
    : m_config(std::move(config))
    , m_module_loader(std::make_unique<ModuleLoader>())
    , m_module_registry(std::make_unique<ModuleRegistry>())
    , m_supervisor_tree(std::make_unique<SupervisorTree>())
    , m_hot_reload(std::make_unique<void_core::HotReloadSystem>())
    , m_plugin_registry(std::make_unique<void_core::PluginRegistry>())
    , m_hot_reload_enabled(config.enable_hot_reload) {

    // Configure module loader
    m_module_loader->add_search_path(m_config.module_path);
    m_module_loader->add_search_path(m_config.plugin_path);
    m_module_loader->set_hot_reload_enabled(m_config.enable_hot_reload);

    // Initialize frame time array
    m_frame_times.fill(std::chrono::nanoseconds{0});

    // Initialize stage arrays
    m_stage_dirty.fill(false);
    for (auto& config : m_stage_configs) {
        config = StageConfig{true, false};
    }
}

Kernel::~Kernel() {
    shutdown();
}

void_core::Result<void> Kernel::initialize() {
    if (m_phase.load() != KernelPhase::PreInit) {
        return void_core::Error{"Kernel already initialized"};
    }

    // Phase: CoreInit
    auto core_result = init_core();
    if (!core_result) return core_result;

    // Phase: ServiceInit
    auto service_result = init_services();
    if (!service_result) return service_result;

    // Phase: ModuleInit
    auto module_result = init_modules();
    if (!module_result) return module_result;

    // Phase: PluginInit
    auto plugin_result = init_plugins();
    if (!plugin_result) return plugin_result;

    // Ready
    set_phase(KernelPhase::Ready);
    return void_core::Ok();
}

void_core::Result<void> Kernel::start() {
    if (m_phase.load() != KernelPhase::Ready) {
        return void_core::Error{"Kernel not in ready state"};
    }

    // Start supervisor tree
    auto result = m_supervisor_tree->start();
    if (!result) {
        return result;
    }

    m_start_time = std::chrono::steady_clock::now();
    set_phase(KernelPhase::Running);

    return void_core::Ok();
}

void Kernel::update(float dt) {
    if (m_phase.load() != KernelPhase::Running) {
        return;
    }

    auto frame_start = std::chrono::steady_clock::now();

    // Update hot-reload system
    update_hot_reload(dt);

    // Check supervisors
    update_supervisors(dt);

    // Update modules
    update_modules(dt);

    // Update plugins
    update_plugins(dt);

    // Track frame time
    auto frame_end = std::chrono::steady_clock::now();
    m_last_frame_time = frame_end - frame_start;
    m_frame_times[m_frame_time_index] = m_last_frame_time;
    m_frame_time_index = (m_frame_time_index + 1) % FRAME_TIME_SAMPLES;

    m_frame_count.fetch_add(1);

    // Check for shutdown request
    if (m_shutdown_requested.load()) {
        stop();
    }
}

void Kernel::stop() {
    if (m_phase.load() != KernelPhase::Running) {
        return;
    }

    set_phase(KernelPhase::Shutdown);

    // Stop supervisor tree
    m_supervisor_tree->stop();
}

void Kernel::shutdown() {
    if (m_phase.load() == KernelPhase::Terminated) {
        return;
    }

    // Ensure we're not running
    if (m_phase.load() == KernelPhase::Running) {
        stop();
    }

    set_phase(KernelPhase::Shutdown);

    // Shutdown in reverse order
    shutdown_plugins();
    shutdown_modules();
    shutdown_services();
    shutdown_core();

    set_phase(KernelPhase::Terminated);
}

KernelStats Kernel::stats() const {
    KernelStats stats;

    stats.frame_count = m_frame_count.load();
    stats.hot_reloads = m_hot_reload_count.load();
    stats.last_frame_time = m_last_frame_time;

    // Calculate average frame time
    auto total = std::accumulate(m_frame_times.begin(), m_frame_times.end(),
                                  std::chrono::nanoseconds{0});
    stats.avg_frame_time = total / FRAME_TIME_SAMPLES;

    // Module stats
    stats.total_modules = m_module_loader->loaded_modules().size();
    stats.active_modules = stats.total_modules;

    // Plugin stats
    stats.total_plugins = m_plugin_registry->len();
    stats.active_plugins = m_plugin_registry->active_count();

    // Supervisor stats
    stats.supervisor_restarts = m_supervisor_tree->total_restart_count();

    // Sandbox stats (count violations)
    std::lock_guard<std::mutex> lock(m_sandbox_mutex);
    for (const auto& [name, sandbox] : m_sandboxes) {
        stats.sandbox_violations += sandbox->violation_count();
    }

    // Uptime
    if (m_phase.load() >= KernelPhase::Running) {
        stats.uptime = std::chrono::steady_clock::now() - m_start_time;
    }

    return stats;
}

std::shared_ptr<Sandbox> Kernel::create_sandbox(const SandboxConfig& config) {
    auto sandbox = std::make_shared<Sandbox>(config);

    std::lock_guard<std::mutex> lock(m_sandbox_mutex);
    m_sandboxes[config.name] = sandbox;

    return sandbox;
}

std::shared_ptr<Sandbox> Kernel::get_sandbox(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_sandbox_mutex);
    auto it = m_sandboxes.find(name);
    if (it != m_sandboxes.end()) {
        return it->second;
    }
    return nullptr;
}

void Kernel::remove_sandbox(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_sandbox_mutex);
    m_sandboxes.erase(name);
}

void Kernel::set_on_phase_change(std::function<void(const KernelPhaseEvent&)> callback) {
    m_on_phase_change = std::move(callback);
}

void_core::Result<void> Kernel::load_configured_modules() {
    // In a real implementation, this would read from a config file
    // and load the specified modules
    return void_core::Ok();
}

void_core::Result<void> Kernel::load_configured_plugins() {
    // In a real implementation, this would read from a config file
    // and load the specified plugins
    return void_core::Ok();
}

void_core::Result<void> Kernel::register_module(std::unique_ptr<IModule> module) {
    return m_module_registry->register_module(std::move(module));
}

void Kernel::request_shutdown() {
    m_shutdown_requested.store(true);
}

std::chrono::nanoseconds Kernel::uptime() const {
    if (m_phase.load() < KernelPhase::Running) {
        return std::chrono::nanoseconds{0};
    }
    return std::chrono::steady_clock::now() - m_start_time;
}

void Kernel::set_phase(KernelPhase new_phase) {
    KernelPhase old_phase = m_phase.exchange(new_phase);

    if (m_on_phase_change && old_phase != new_phase) {
        KernelPhaseEvent event{
            .old_phase = old_phase,
            .new_phase = new_phase,
            .timestamp = std::chrono::system_clock::now(),
        };
        m_on_phase_change(event);
    }
}

void_core::Result<void> Kernel::init_core() {
    set_phase(KernelPhase::CoreInit);

    // Create root supervisor
    SupervisorConfig sup_config;
    sup_config.name = m_config.name + "_supervisor";
    sup_config.strategy = RestartStrategy::OneForOne;
    sup_config.limits = RestartLimits{5, std::chrono::seconds(60)};

    auto result = m_supervisor_tree->create_root(std::move(sup_config));
    if (!result) {
        return void_core::Error{"Failed to create root supervisor: " + result.error().message()};
    }

    // Configure hot-reload
    if (m_config.enable_hot_reload) {
        m_hot_reload->set_poll_interval(m_config.hot_reload_poll_interval);

        // Watch asset directory
        m_hot_reload->watch_directory(m_config.asset_path);

        // Watch module directory
        m_hot_reload->watch_directory(m_config.module_path);
    }

    return void_core::Ok();
}

void_core::Result<void> Kernel::init_services() {
    set_phase(KernelPhase::ServiceInit);

    // Services are initialized here
    // In a real implementation, this would start the ServiceRegistry

    return void_core::Ok();
}

void_core::Result<void> Kernel::init_modules() {
    set_phase(KernelPhase::ModuleInit);

    // Load configured modules
    auto result = load_configured_modules();
    if (!result) {
        return result;
    }

    // Initialize module registry
    auto init_result = m_module_registry->initialize_all();
    if (!init_result) {
        return init_result;
    }

    return void_core::Ok();
}

void_core::Result<void> Kernel::init_plugins() {
    set_phase(KernelPhase::PluginInit);

    // Load configured plugins
    auto result = load_configured_plugins();
    if (!result) {
        return result;
    }

    return void_core::Ok();
}

void Kernel::shutdown_plugins() {
    m_plugin_registry->unload_all();
}

void Kernel::shutdown_modules() {
    m_module_registry->shutdown_all();
    m_module_loader->unload_all();
}

void Kernel::shutdown_services() {
    // Shutdown services
}

void Kernel::shutdown_core() {
    // Stop hot-reload
    m_hot_reload->stop_watching();

    // Clear sandboxes
    {
        std::lock_guard<std::mutex> lock(m_sandbox_mutex);
        m_sandboxes.clear();
    }
}

void Kernel::update_hot_reload(float dt) {
    if (!m_config.enable_hot_reload) {
        return;
    }

    // Poll for file changes
    m_hot_reload->update();

    // Check for modified modules
    auto modified = m_module_loader->get_modified_modules();
    for (const auto& id : modified) {
        auto result = m_module_loader->reload_module(id);
        if (result) {
            m_hot_reload_count.fetch_add(1);
        }
    }
}

void Kernel::update_supervisors(float dt) {
    m_supervisor_tree->check_all();
}

void Kernel::update_modules(float dt) {
    m_module_registry->update_all(dt);
}

void Kernel::update_plugins(float dt) {
    m_plugin_registry->update_all(dt);
}

// =============================================================================
// Stage Scheduler Implementation
// =============================================================================

void Kernel::register_system(Stage stage, const std::string& name,
                              SystemFunc func, std::int32_t priority) {
    if (stage == Stage::_Count) return;

    std::lock_guard<std::mutex> lock(m_stage_mutex);
    auto stage_idx = static_cast<std::size_t>(stage);

    // Check if system already exists
    auto& systems = m_stage_systems[stage_idx];
    auto it = std::find_if(systems.begin(), systems.end(),
                           [&name](const SystemInfo& s) { return s.name == name; });

    if (it != systems.end()) {
        // Update existing
        it->func = std::move(func);
        it->priority = priority;
    } else {
        // Add new
        systems.push_back(SystemInfo{name, std::move(func), priority, true});
    }

    m_stage_dirty[stage_idx] = true;
}

void Kernel::unregister_system(Stage stage, const std::string& name) {
    if (stage == Stage::_Count) return;

    std::lock_guard<std::mutex> lock(m_stage_mutex);
    auto stage_idx = static_cast<std::size_t>(stage);

    auto& systems = m_stage_systems[stage_idx];
    systems.erase(
        std::remove_if(systems.begin(), systems.end(),
                       [&name](const SystemInfo& s) { return s.name == name; }),
        systems.end());
}

void Kernel::set_system_enabled(Stage stage, const std::string& name, bool enabled) {
    if (stage == Stage::_Count) return;

    std::lock_guard<std::mutex> lock(m_stage_mutex);
    auto stage_idx = static_cast<std::size_t>(stage);

    auto& systems = m_stage_systems[stage_idx];
    auto it = std::find_if(systems.begin(), systems.end(),
                           [&name](const SystemInfo& s) { return s.name == name; });

    if (it != systems.end()) {
        it->enabled = enabled;
    }
}

void Kernel::run_stage(Stage stage, float dt) {
    if (stage == Stage::_Count) return;
    if (m_phase.load() != KernelPhase::Running) return;

    auto stage_idx = static_cast<std::size_t>(stage);

    // Check if stage is enabled
    if (!m_stage_configs[stage_idx].enabled) return;

    // Sort by priority if dirty (under lock)
    {
        std::lock_guard<std::mutex> lock(m_stage_mutex);
        if (m_stage_dirty[stage_idx]) {
            auto& systems = m_stage_systems[stage_idx];
            std::stable_sort(systems.begin(), systems.end(),
                             [](const SystemInfo& a, const SystemInfo& b) {
                                 return a.priority < b.priority;
                             });
            m_stage_dirty[stage_idx] = false;
        }
    }

    // Execute systems (make a copy to allow modification during iteration)
    std::vector<SystemInfo> systems_copy;
    {
        std::lock_guard<std::mutex> lock(m_stage_mutex);
        systems_copy = m_stage_systems[stage_idx];
    }

    for (const auto& sys : systems_copy) {
        if (sys.enabled && sys.func) {
            sys.func(dt);
        }
    }

    // Special handling for built-in stages
    switch (stage) {
        case Stage::HotReloadPoll:
            if (m_hot_reload_enabled) {
                update_hot_reload(dt);
            }
            update_supervisors(dt);
            break;

        case Stage::Update:
            update_modules(dt);
            update_plugins(dt);
            break;

        default:
            break;
    }
}

void Kernel::enable_hot_reload(std::uint32_t poll_ms, std::uint32_t debounce_ms) {
    m_hot_reload_enabled = true;
    m_hot_reload_poll_ms = poll_ms;
    m_hot_reload_debounce_ms = debounce_ms;

    if (m_hot_reload) {
        m_hot_reload->set_poll_interval(std::chrono::milliseconds(poll_ms));
    }
}

void Kernel::disable_hot_reload() {
    m_hot_reload_enabled = false;
    if (m_hot_reload) {
        m_hot_reload->stop_watching();
    }
}

StageConfig Kernel::get_stage_config(Stage stage) const {
    if (stage == Stage::_Count) return StageConfig{};
    return m_stage_configs[static_cast<std::size_t>(stage)];
}

void Kernel::set_stage_config(Stage stage, const StageConfig& config) {
    if (stage == Stage::_Count) return;
    m_stage_configs[static_cast<std::size_t>(stage)] = config;
}

} // namespace void_kernel
