/// @file fwd.hpp
/// @brief Forward declarations for void_kernel
///
/// Provides forward declarations for all kernel types to minimize
/// header dependencies and compilation times.

#pragma once

#include <cstdint>
#include <memory>

namespace void_kernel {

// =============================================================================
// Module System
// =============================================================================

/// Unique identifier for a module
struct ModuleId;

/// Module information (name, version, dependencies)
struct ModuleInfo;

/// Module state enumeration
enum class ModuleState : std::uint8_t;

/// Module load result
struct ModuleLoadResult;

/// Module interface
class IModule;

/// Platform-specific module handle
class ModuleHandle;

/// Module loader for dynamic libraries
class ModuleLoader;

/// Module registry for managing loaded modules
class ModuleRegistry;

// =============================================================================
// Supervisor System
// =============================================================================

/// Restart strategy for supervised tasks
enum class RestartStrategy : std::uint8_t;

/// Restart intensity limits
struct RestartLimits;

/// Supervisor child specification
struct ChildSpec;

/// Supervisor configuration
struct SupervisorConfig;

/// Supervisor state
enum class SupervisorState : std::uint8_t;

/// Child process state
enum class ChildState : std::uint8_t;

/// Child handle for managing supervised processes
class ChildHandle;

/// Supervisor for managing child processes
class Supervisor;

/// Supervisor tree for hierarchical supervision
class SupervisorTree;

// =============================================================================
// Sandbox System
// =============================================================================

/// Permission flags
enum class Permission : std::uint32_t;

/// Permission set
class PermissionSet;

/// Resource limits
struct ResourceLimits;

/// Sandbox configuration
struct SandboxConfig;

/// Sandbox state
enum class SandboxState : std::uint8_t;

/// Sandbox for isolated execution
class Sandbox;

/// Sandbox factory for creating sandboxes
class SandboxFactory;

// =============================================================================
// Kernel System
// =============================================================================

/// Kernel phase enumeration
enum class KernelPhase : std::uint8_t;

/// Kernel configuration
struct KernelConfig;

/// Kernel statistics
struct KernelStats;

/// Kernel interface
class IKernel;

/// Default kernel implementation
class Kernel;

// =============================================================================
// Events
// =============================================================================

/// Module loaded event
struct ModuleLoadedEvent;

/// Module unloaded event
struct ModuleUnloadedEvent;

/// Module reload event
struct ModuleReloadEvent;

/// Supervisor child event
struct ChildEvent;

/// Sandbox violation event
struct SandboxViolationEvent;

/// Kernel phase change event
struct KernelPhaseEvent;

// =============================================================================
// Smart Pointer Aliases
// =============================================================================

using ModulePtr = std::shared_ptr<IModule>;
using ModuleWeakPtr = std::weak_ptr<IModule>;
using SupervisorPtr = std::shared_ptr<Supervisor>;
using SandboxPtr = std::shared_ptr<Sandbox>;
using KernelPtr = std::shared_ptr<IKernel>;

} // namespace void_kernel
