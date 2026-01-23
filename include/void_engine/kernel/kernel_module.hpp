/// @file kernel_module.hpp
/// @brief Main include header for void_kernel
///
/// void_kernel provides the core engine orchestration:
///
/// ## Features
///
/// - **Module Loading**
///   - Dynamic library loading (Windows/Unix)
///   - Hot-reload support
///   - Dependency resolution
///
/// - **Supervision**
///   - Erlang-style fault tolerance
///   - Multiple restart strategies
///   - Hierarchical supervisor trees
///
/// - **Sandboxing**
///   - Permission-based access control
///   - Resource limits and tracking
///   - Violation detection
///
/// - **Kernel Orchestration**
///   - System lifecycle management
///   - Phase-based initialization
///   - Statistics and monitoring
///
/// ## Quick Start
///
/// ```cpp
/// #include <void_engine/kernel/kernel_module.hpp>
///
/// using namespace void_kernel;
///
/// // Create kernel with builder
/// auto kernel = KernelBuilder()
///     .name("my_engine")
///     .target_fps(60)
///     .hot_reload(true)
///     .build();
///
/// // Initialize and start
/// kernel->initialize();
/// kernel->start();
///
/// // Main loop
/// while (kernel->is_running()) {
///     kernel->update(dt);
/// }
///
/// // Cleanup
/// kernel->shutdown();
/// ```
///
/// ## Supervision Example
///
/// ```cpp
/// // Create supervisor for worker threads
/// auto& tree = kernel->supervisors();
/// auto* root = tree.create_root(SupervisorConfig()
///     .with_strategy(RestartStrategy::OneForOne)
///     .with_limits(5, std::chrono::seconds(60)));
///
/// // Add workers
/// root->add_child(make_worker("render_thread", []() {
///     // Rendering loop
/// }));
///
/// root->add_child(make_worker("audio_thread", []() {
///     // Audio processing
/// }));
///
/// // Start supervision
/// tree.start();
/// ```
///
/// ## Sandbox Example
///
/// ```cpp
/// // Create sandbox for untrusted script
/// auto sandbox = kernel->create_sandbox(
///     SandboxConfig::untrusted("script_sandbox"));
///
/// // Run code in sandbox
/// {
///     SandboxGuard guard(*sandbox);
///     // Code here runs with restricted permissions
///     // Violations are automatically detected
/// }
/// ```

#pragma once

// Core types
#include "fwd.hpp"
#include "types.hpp"

// Module loading
#include "module_loader.hpp"

// Supervision
#include "supervisor.hpp"

// Sandboxing
#include "sandbox.hpp"

// Kernel
#include "kernel.hpp"

namespace void_kernel {

/// Prelude - commonly used types for convenience
namespace prelude {
    // Types
    using void_kernel::ModuleId;
    using void_kernel::ModuleInfo;
    using void_kernel::ModuleState;
    using void_kernel::ModuleLoadResult;

    // Supervisor types
    using void_kernel::RestartStrategy;
    using void_kernel::RestartLimits;
    using void_kernel::ChildSpec;
    using void_kernel::ChildState;
    using void_kernel::SupervisorConfig;
    using void_kernel::SupervisorState;

    // Sandbox types
    using void_kernel::Permission;
    using void_kernel::PermissionSet;
    using void_kernel::ResourceLimits;
    using void_kernel::SandboxConfig;
    using void_kernel::SandboxState;

    // Kernel types
    using void_kernel::KernelPhase;
    using void_kernel::KernelConfig;
    using void_kernel::KernelStats;

    // Events
    using void_kernel::ModuleLoadedEvent;
    using void_kernel::ModuleUnloadedEvent;
    using void_kernel::ModuleReloadEvent;
    using void_kernel::ChildEvent;
    using void_kernel::ChildEventType;
    using void_kernel::SandboxViolationEvent;
    using void_kernel::KernelPhaseEvent;

    // Module system
    using void_kernel::IModule;
    using void_kernel::ModuleHandle;
    using void_kernel::ModuleLoader;
    using void_kernel::ModuleRegistry;

    // Supervision
    using void_kernel::ChildHandle;
    using void_kernel::Supervisor;
    using void_kernel::SupervisorTree;

    // Sandbox
    using void_kernel::ResourceUsageTracker;
    using void_kernel::Sandbox;
    using void_kernel::SandboxFactory;
    using void_kernel::SandboxGuard;

    // Kernel
    using void_kernel::IKernel;
    using void_kernel::Kernel;
    using void_kernel::KernelBuilder;
    using void_kernel::GlobalKernelGuard;

    // Helper functions
    using void_kernel::make_child;
    using void_kernel::make_worker;
    using void_kernel::make_task;
    using void_kernel::make_temporary;
    using void_kernel::current_sandbox;
    using void_kernel::global_kernel;
} // namespace prelude

} // namespace void_kernel
