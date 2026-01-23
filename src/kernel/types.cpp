/// @file types.cpp
/// @brief Core type implementations for void_kernel

#include <void_engine/kernel/types.hpp>

#include <functional>

namespace void_kernel {

// =============================================================================
// ModuleId
// =============================================================================

ModuleId ModuleId::from_name(const std::string& name) {
    // FNV-1a hash
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : name) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return ModuleId{hash};
}

// =============================================================================
// ModuleState
// =============================================================================

const char* to_string(ModuleState state) {
    switch (state) {
        case ModuleState::Unloaded: return "Unloaded";
        case ModuleState::Loading: return "Loading";
        case ModuleState::Loaded: return "Loaded";
        case ModuleState::Initializing: return "Initializing";
        case ModuleState::Ready: return "Ready";
        case ModuleState::Running: return "Running";
        case ModuleState::Stopping: return "Stopping";
        case ModuleState::Unloading: return "Unloading";
        case ModuleState::Failed: return "Failed";
        case ModuleState::Reloading: return "Reloading";
    }
    return "Unknown";
}

// =============================================================================
// ModuleInfo
// =============================================================================

bool ModuleInfo::depends_on(const std::string& module_name) const {
    for (const auto& dep : dependencies) {
        if (dep == module_name) return true;
    }
    for (const auto& dep : optional_dependencies) {
        if (dep == module_name) return true;
    }
    return false;
}

// =============================================================================
// RestartStrategy
// =============================================================================

const char* to_string(RestartStrategy strategy) {
    switch (strategy) {
        case RestartStrategy::OneForOne: return "OneForOne";
        case RestartStrategy::OneForAll: return "OneForAll";
        case RestartStrategy::RestForOne: return "RestForOne";
        case RestartStrategy::Temporary: return "Temporary";
        case RestartStrategy::Transient: return "Transient";
    }
    return "Unknown";
}

// =============================================================================
// ChildState
// =============================================================================

const char* to_string(ChildState state) {
    switch (state) {
        case ChildState::Stopped: return "Stopped";
        case ChildState::Starting: return "Starting";
        case ChildState::Running: return "Running";
        case ChildState::Stopping: return "Stopping";
        case ChildState::Restarting: return "Restarting";
        case ChildState::Failed: return "Failed";
        case ChildState::Terminated: return "Terminated";
    }
    return "Unknown";
}

// =============================================================================
// SupervisorState
// =============================================================================

const char* to_string(SupervisorState state) {
    switch (state) {
        case SupervisorState::Stopped: return "Stopped";
        case SupervisorState::Starting: return "Starting";
        case SupervisorState::Running: return "Running";
        case SupervisorState::Stopping: return "Stopping";
        case SupervisorState::Failed: return "Failed";
    }
    return "Unknown";
}

// =============================================================================
// SandboxState
// =============================================================================

const char* to_string(SandboxState state) {
    switch (state) {
        case SandboxState::Created: return "Created";
        case SandboxState::Running: return "Running";
        case SandboxState::Suspended: return "Suspended";
        case SandboxState::Terminated: return "Terminated";
        case SandboxState::Violated: return "Violated";
    }
    return "Unknown";
}

// =============================================================================
// KernelPhase
// =============================================================================

const char* to_string(KernelPhase phase) {
    switch (phase) {
        case KernelPhase::PreInit: return "PreInit";
        case KernelPhase::CoreInit: return "CoreInit";
        case KernelPhase::ServiceInit: return "ServiceInit";
        case KernelPhase::ModuleInit: return "ModuleInit";
        case KernelPhase::PluginInit: return "PluginInit";
        case KernelPhase::Ready: return "Ready";
        case KernelPhase::Running: return "Running";
        case KernelPhase::Shutdown: return "Shutdown";
        case KernelPhase::Terminated: return "Terminated";
    }
    return "Unknown";
}

} // namespace void_kernel
