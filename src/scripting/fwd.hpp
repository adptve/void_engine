#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_scripting module

#include <void_engine/core/handle.hpp>
#include <cstdint>

namespace void_scripting {

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Unique identifier for a WASM module
struct WasmModuleIdTag {};
using WasmModuleId = void_core::Handle<WasmModuleIdTag>;

/// @brief Unique identifier for a WASM instance
struct WasmInstanceIdTag {};
using WasmInstanceId = void_core::Handle<WasmInstanceIdTag>;

/// @brief Unique identifier for a host function
struct HostFunctionIdTag {};
using HostFunctionId = void_core::Handle<HostFunctionIdTag>;

/// @brief Unique identifier for a plugin
struct PluginIdTag {};
using PluginId = void_core::Handle<PluginIdTag>;

// =============================================================================
// Forward Declarations
// =============================================================================

// WASM types
struct WasmValue;
struct WasmType;
struct WasmFunctionType;
struct WasmTableType;
struct WasmMemoryType;
struct WasmGlobalType;

// WASM objects
class WasmModule;
class WasmInstance;
class WasmFunction;
class WasmMemory;
class WasmTable;
class WasmGlobal;

// Runtime
class WasmRuntime;
class WasmStore;
class WasmEngine;

// Host API
class HostApi;
class HostFunction;
class HostMemory;

// Plugin system
class Plugin;
class PluginHost;
class PluginRegistry;

// System
class ScriptingSystem;

} // namespace void_scripting
