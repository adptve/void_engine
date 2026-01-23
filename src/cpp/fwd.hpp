#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_cpp module

#include <void_engine/core/handle.hpp>
#include <cstdint>

namespace void_cpp {

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Unique identifier for a compiled module
struct ModuleIdTag {};
using ModuleId = void_core::Handle<ModuleIdTag>;

/// @brief Unique identifier for a symbol
struct SymbolIdTag {};
using SymbolId = void_core::Handle<SymbolIdTag>;

/// @brief Unique identifier for a compilation job
struct CompileJobIdTag {};
using CompileJobId = void_core::Handle<CompileJobIdTag>;

/// @brief Unique identifier for a file watcher
struct WatcherIdTag {};
using WatcherId = void_core::Handle<WatcherIdTag>;

// =============================================================================
// Forward Declarations
// =============================================================================

// Compiler
class Compiler;
class CompilerConfig;
class CompileResult;
class CompileJob;
class CompileQueue;

// Modules
class DynamicModule;
class ModuleRegistry;
class ModuleLoader;

// Symbols
class Symbol;
class SymbolTable;
class SymbolResolver;

// Hot reload
class HotReloader;
class FileWatcher;
class StatePreserver;
class ReloadContext;

// System
class CppSystem;

} // namespace void_cpp
