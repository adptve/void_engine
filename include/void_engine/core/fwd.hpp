#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_core module

#include <cstdint>

namespace void_core {

// =============================================================================
// Error Types
// =============================================================================

enum class ErrorKind : std::uint8_t;
class Error;

template<typename T, typename E = Error>
class Result;

// =============================================================================
// Version
// =============================================================================

struct Version;

// =============================================================================
// ID Types
// =============================================================================

struct Id;
class IdGenerator;
struct NamedId;

// =============================================================================
// Handle Types
// =============================================================================

template<typename T>
struct Handle;

template<typename T>
class HandleAllocator;

template<typename T>
class HandleMap;

// =============================================================================
// Type Registry
// =============================================================================

enum class PrimitiveType : std::uint8_t;
struct FieldInfo;
struct VariantInfo;
struct TypeSchema;
struct TypeInfo;
class TypeRegistry;

// =============================================================================
// Plugin System
// =============================================================================

struct PluginId;
enum class PluginStatus : std::uint8_t;
struct PluginState;
struct PluginContext;
struct PluginInfo;
class Plugin;
class PluginRegistry;

// =============================================================================
// Hot-Reload System
// =============================================================================

struct HotReloadSnapshot;
class HotReloadable;
enum class ReloadEventType : std::uint8_t;
struct ReloadEvent;
class HotReloadManager;
class FileWatcher;
class MemoryFileWatcher;

} // namespace void_core
