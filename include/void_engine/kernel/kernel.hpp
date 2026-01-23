/// @file kernel.hpp
/// @brief Core kernel interface and implementation
///
/// The kernel is the central orchestrator that manages:
/// - Module loading and lifecycle
/// - Service bootstrapping
/// - Plugin management
/// - Supervisor tree for fault tolerance
/// - Hot-reload coordination
/// - System scheduling

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "module_loader.hpp"
#include "supervisor.hpp"
#include "sandbox.hpp"

#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <void_engine/core/plugin.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace void_kernel {

// =============================================================================
// Kernel Interface
// =============================================================================

/// Kernel interface - the central orchestrator
class IKernel {
public:
    virtual ~IKernel() = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Initialize the kernel
    [[nodiscard]] virtual void_core::Result<void> initialize() = 0;

    /// Start the kernel (enters running state)
    [[nodiscard]] virtual void_core::Result<void> start() = 0;

    /// Update the kernel (call each frame)
    virtual void update(float dt) = 0;

    /// Stop the kernel
    virtual void stop() = 0;

    /// Shutdown the kernel
    virtual void shutdown() = 0;

    /// Get current phase
    [[nodiscard]] virtual KernelPhase phase() const = 0;

    /// Check if kernel is running
    [[nodiscard]] virtual bool is_running() const = 0;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Get configuration
    [[nodiscard]] virtual const KernelConfig& config() const = 0;

    /// Get statistics
    [[nodiscard]] virtual KernelStats stats() const = 0;

    // =========================================================================
    // Subsystems
    // =========================================================================

    /// Get module loader
    [[nodiscard]] virtual ModuleLoader& modules() = 0;
    [[nodiscard]] virtual const ModuleLoader& modules() const = 0;

    /// Get module registry
    [[nodiscard]] virtual ModuleRegistry& module_registry() = 0;
    [[nodiscard]] virtual const ModuleRegistry& module_registry() const = 0;

    /// Get supervisor tree
    [[nodiscard]] virtual SupervisorTree& supervisors() = 0;
    [[nodiscard]] virtual const SupervisorTree& supervisors() const = 0;

    /// Get hot-reload system
    [[nodiscard]] virtual void_core::HotReloadSystem& hot_reload() = 0;
    [[nodiscard]] virtual const void_core::HotReloadSystem& hot_reload() const = 0;

    /// Get plugin registry
    [[nodiscard]] virtual void_core::PluginRegistry& plugins() = 0;
    [[nodiscard]] virtual const void_core::PluginRegistry& plugins() const = 0;

    // =========================================================================
    // Sandbox Management
    // =========================================================================

    /// Create a sandbox with configuration
    [[nodiscard]] virtual std::shared_ptr<Sandbox> create_sandbox(const SandboxConfig& config) = 0;

    /// Get sandbox by name
    [[nodiscard]] virtual std::shared_ptr<Sandbox> get_sandbox(const std::string& name) = 0;

    /// Remove a sandbox
    virtual void remove_sandbox(const std::string& name) = 0;

    // =========================================================================
    // Events
    // =========================================================================

    /// Set callback for phase changes
    virtual void set_on_phase_change(std::function<void(const KernelPhaseEvent&)> callback) = 0;
};

// =============================================================================
// Kernel Implementation
// =============================================================================

/// Default kernel implementation
class Kernel : public IKernel {
public:
    /// Create kernel with configuration
    explicit Kernel(KernelConfig config = {});
    ~Kernel() override;

    // Non-copyable
    Kernel(const Kernel&) = delete;
    Kernel& operator=(const Kernel&) = delete;

    // =========================================================================
    // IKernel Implementation
    // =========================================================================

    [[nodiscard]] void_core::Result<void> initialize() override;
    [[nodiscard]] void_core::Result<void> start() override;
    void update(float dt) override;
    void stop() override;
    void shutdown() override;

    [[nodiscard]] KernelPhase phase() const override { return m_phase.load(); }
    [[nodiscard]] bool is_running() const override { return m_phase.load() == KernelPhase::Running; }

    [[nodiscard]] const KernelConfig& config() const override { return m_config; }
    [[nodiscard]] KernelStats stats() const override;

    [[nodiscard]] ModuleLoader& modules() override { return *m_module_loader; }
    [[nodiscard]] const ModuleLoader& modules() const override { return *m_module_loader; }

    [[nodiscard]] ModuleRegistry& module_registry() override { return *m_module_registry; }
    [[nodiscard]] const ModuleRegistry& module_registry() const override { return *m_module_registry; }

    [[nodiscard]] SupervisorTree& supervisors() override { return *m_supervisor_tree; }
    [[nodiscard]] const SupervisorTree& supervisors() const override { return *m_supervisor_tree; }

    [[nodiscard]] void_core::HotReloadSystem& hot_reload() override { return *m_hot_reload; }
    [[nodiscard]] const void_core::HotReloadSystem& hot_reload() const override { return *m_hot_reload; }

    [[nodiscard]] void_core::PluginRegistry& plugins() override { return *m_plugin_registry; }
    [[nodiscard]] const void_core::PluginRegistry& plugins() const override { return *m_plugin_registry; }

    [[nodiscard]] std::shared_ptr<Sandbox> create_sandbox(const SandboxConfig& config) override;
    [[nodiscard]] std::shared_ptr<Sandbox> get_sandbox(const std::string& name) override;
    void remove_sandbox(const std::string& name) override;

    void set_on_phase_change(std::function<void(const KernelPhaseEvent&)> callback) override;

    // =========================================================================
    // Extended API
    // =========================================================================

    /// Load modules from configuration
    void_core::Result<void> load_configured_modules();

    /// Load plugins from configuration
    void_core::Result<void> load_configured_plugins();

    /// Register a built-in module
    void_core::Result<void> register_module(std::unique_ptr<IModule> module);

    /// Request kernel shutdown (async)
    void request_shutdown();

    /// Check if shutdown was requested
    [[nodiscard]] bool shutdown_requested() const { return m_shutdown_requested.load(); }

    /// Get uptime
    [[nodiscard]] std::chrono::nanoseconds uptime() const;

    /// Get frame count
    [[nodiscard]] std::uint64_t frame_count() const { return m_frame_count.load(); }

private:
    // Phase transitions
    void set_phase(KernelPhase new_phase);

    // Initialization stages
    void_core::Result<void> init_core();
    void_core::Result<void> init_services();
    void_core::Result<void> init_modules();
    void_core::Result<void> init_plugins();

    // Shutdown stages
    void shutdown_plugins();
    void shutdown_modules();
    void shutdown_services();
    void shutdown_core();

    // Update stages
    void update_hot_reload(float dt);
    void update_supervisors(float dt);
    void update_modules(float dt);
    void update_plugins(float dt);

private:
    KernelConfig m_config;
    std::atomic<KernelPhase> m_phase{KernelPhase::PreInit};
    std::atomic<bool> m_shutdown_requested{false};

    // Subsystems
    std::unique_ptr<ModuleLoader> m_module_loader;
    std::unique_ptr<ModuleRegistry> m_module_registry;
    std::unique_ptr<SupervisorTree> m_supervisor_tree;
    std::unique_ptr<void_core::HotReloadSystem> m_hot_reload;
    std::unique_ptr<void_core::PluginRegistry> m_plugin_registry;

    // Sandboxes
    mutable std::mutex m_sandbox_mutex;
    std::unordered_map<std::string, std::shared_ptr<Sandbox>> m_sandboxes;

    // Statistics
    std::chrono::steady_clock::time_point m_start_time;
    std::atomic<std::uint64_t> m_frame_count{0};
    std::atomic<std::uint64_t> m_hot_reload_count{0};
    std::chrono::nanoseconds m_last_frame_time{0};

    // Moving average for frame time
    static constexpr std::size_t FRAME_TIME_SAMPLES = 60;
    std::array<std::chrono::nanoseconds, FRAME_TIME_SAMPLES> m_frame_times{};
    std::size_t m_frame_time_index = 0;

    // Callbacks
    std::function<void(const KernelPhaseEvent&)> m_on_phase_change;
};

// =============================================================================
// Kernel Builder
// =============================================================================

/// Fluent builder for kernel configuration
class KernelBuilder {
public:
    KernelBuilder() = default;

    /// Set kernel name
    KernelBuilder& name(const std::string& n) { m_config.name = n; return *this; }

    /// Set config path
    KernelBuilder& config_path(const std::string& path) { m_config.config_path = path; return *this; }

    /// Set module path
    KernelBuilder& module_path(const std::string& path) { m_config.module_path = path; return *this; }

    /// Set plugin path
    KernelBuilder& plugin_path(const std::string& path) { m_config.plugin_path = path; return *this; }

    /// Set asset path
    KernelBuilder& asset_path(const std::string& path) { m_config.asset_path = path; return *this; }

    /// Set target FPS
    KernelBuilder& target_fps(std::uint32_t fps) { m_config.target_fps = fps; return *this; }

    /// Enable/disable hot-reload
    KernelBuilder& hot_reload(bool enable) { m_config.enable_hot_reload = enable; return *this; }

    /// Enable/disable profiling
    KernelBuilder& profiling(bool enable) { m_config.enable_profiling = enable; return *this; }

    /// Enable/disable validation
    KernelBuilder& validation(bool enable) { m_config.enable_validation = enable; return *this; }

    /// Set worker thread count
    KernelBuilder& workers(std::uint32_t count) { m_config.worker_thread_count = count; return *this; }

    /// Build the kernel
    [[nodiscard]] std::unique_ptr<Kernel> build() {
        return std::make_unique<Kernel>(m_config);
    }

    /// Build and initialize the kernel
    [[nodiscard]] void_core::Result<std::unique_ptr<Kernel>> build_and_init() {
        auto kernel = build();
        auto result = kernel->initialize();
        if (!result) {
            return void_core::Error{result.error().message()};
        }
        return kernel;
    }

private:
    KernelConfig m_config;
};

// =============================================================================
// Global Kernel Access
// =============================================================================

/// Get the global kernel instance (nullptr if not set)
[[nodiscard]] IKernel* global_kernel();

/// Set the global kernel instance
void set_global_kernel(IKernel* kernel);

/// RAII guard for global kernel
class GlobalKernelGuard {
public:
    explicit GlobalKernelGuard(IKernel& kernel);
    ~GlobalKernelGuard();

    // Non-copyable, non-movable
    GlobalKernelGuard(const GlobalKernelGuard&) = delete;
    GlobalKernelGuard& operator=(const GlobalKernelGuard&) = delete;

private:
    IKernel* m_previous;
};

} // namespace void_kernel
