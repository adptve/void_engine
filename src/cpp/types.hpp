#pragma once

/// @file types.hpp
/// @brief Core types for void_cpp module

#include "fwd.hpp"
#include <void_engine/core/error.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace void_cpp {

// =============================================================================
// Compiler Types
// =============================================================================

/// @brief Compiler backend type
enum class CompilerType : std::uint8_t {
    Auto,       ///< Auto-detect based on platform
    MSVC,       ///< Microsoft Visual C++
    Clang,      ///< Clang/LLVM
    GCC,        ///< GNU Compiler Collection
    ClangCL,    ///< Clang with MSVC compatibility

    Count
};

/// @brief Build configuration
enum class BuildConfig : std::uint8_t {
    Debug,          ///< Debug build with symbols
    Release,        ///< Optimized release build
    RelWithDebInfo, ///< Release with debug info
    MinSizeRel,     ///< Minimum size release

    Count
};

/// @brief C++ standard version
enum class CppStandard : std::uint8_t {
    Cpp17,
    Cpp20,
    Cpp23,

    Default = Cpp20
};

/// @brief Optimization level
enum class OptimizationLevel : std::uint8_t {
    O0,     ///< No optimization
    O1,     ///< Basic optimization
    O2,     ///< Standard optimization
    O3,     ///< Aggressive optimization
    Os,     ///< Optimize for size
    Oz,     ///< Aggressive size optimization

    Count
};

/// @brief Warning level
enum class WarningLevel : std::uint8_t {
    Off,
    Low,
    Default,
    High,
    All,
    Error,  ///< Treat warnings as errors

    Count
};

/// @brief Compile output type
enum class OutputType : std::uint8_t {
    SharedLibrary,  ///< .dll / .so / .dylib
    StaticLibrary,  ///< .lib / .a
    Executable,     ///< .exe / executable
    Object,         ///< .obj / .o

    Count
};

// =============================================================================
// Compiler Configuration
// =============================================================================

/// @brief Compiler configuration
struct CompilerConfig {
    CompilerType compiler = CompilerType::Auto;
    CppStandard standard = CppStandard::Default;
    BuildConfig config = BuildConfig::Debug;
    OptimizationLevel optimization = OptimizationLevel::O0;
    WarningLevel warnings = WarningLevel::Default;
    OutputType output_type = OutputType::SharedLibrary;

    // Paths
    std::filesystem::path output_dir;
    std::filesystem::path intermediate_dir;
    std::filesystem::path compiler_path;

    // Include paths
    std::vector<std::filesystem::path> include_paths;

    // Library paths
    std::vector<std::filesystem::path> library_paths;

    // Libraries to link
    std::vector<std::string> libraries;

    // Preprocessor definitions
    std::vector<std::string> defines;

    // Additional compiler flags
    std::vector<std::string> compiler_flags;

    // Additional linker flags
    std::vector<std::string> linker_flags;

    // Generate debug info
    bool debug_info = true;

    // Generate PDB (MSVC)
    bool generate_pdb = true;

    // Enable RTTI
    bool rtti = true;

    // Enable exceptions
    bool exceptions = true;

    // Enable incremental linking
    bool incremental_link = true;

    // Max parallel jobs
    std::size_t max_parallel_jobs = 0;  // 0 = auto

    // Builder helper
    class Builder;
};

/// @brief Compiler config builder
class CompilerConfig::Builder {
public:
    Builder() = default;

    Builder& compiler(CompilerType type) { config_.compiler = type; return *this; }
    Builder& standard(CppStandard std) { config_.standard = std; return *this; }
    Builder& build_config(BuildConfig cfg) { config_.config = cfg; return *this; }
    Builder& optimization(OptimizationLevel opt) { config_.optimization = opt; return *this; }
    Builder& warnings(WarningLevel warn) { config_.warnings = warn; return *this; }
    Builder& output_type(OutputType type) { config_.output_type = type; return *this; }

    Builder& output_dir(const std::filesystem::path& path) { config_.output_dir = path; return *this; }
    Builder& intermediate_dir(const std::filesystem::path& path) { config_.intermediate_dir = path; return *this; }
    Builder& compiler_path(const std::filesystem::path& path) { config_.compiler_path = path; return *this; }

    Builder& include_path(const std::filesystem::path& path) { config_.include_paths.push_back(path); return *this; }
    Builder& library_path(const std::filesystem::path& path) { config_.library_paths.push_back(path); return *this; }
    Builder& library(const std::string& lib) { config_.libraries.push_back(lib); return *this; }
    Builder& define(const std::string& def) { config_.defines.push_back(def); return *this; }

    Builder& compiler_flag(const std::string& flag) { config_.compiler_flags.push_back(flag); return *this; }
    Builder& linker_flag(const std::string& flag) { config_.linker_flags.push_back(flag); return *this; }

    Builder& debug_info(bool enable) { config_.debug_info = enable; return *this; }
    Builder& generate_pdb(bool enable) { config_.generate_pdb = enable; return *this; }
    Builder& rtti(bool enable) { config_.rtti = enable; return *this; }
    Builder& exceptions(bool enable) { config_.exceptions = enable; return *this; }
    Builder& incremental_link(bool enable) { config_.incremental_link = enable; return *this; }
    Builder& max_parallel_jobs(std::size_t jobs) { config_.max_parallel_jobs = jobs; return *this; }

    CompilerConfig build() { return std::move(config_); }

private:
    CompilerConfig config_;
};

// =============================================================================
// Compile Result
// =============================================================================

/// @brief Compilation status
enum class CompileStatus : std::uint8_t {
    Pending,
    Compiling,
    Linking,
    Success,
    Warning,
    Error,
    Cancelled
};

/// @brief Diagnostic severity
enum class DiagnosticSeverity : std::uint8_t {
    Note,
    Warning,
    Error,
    Fatal
};

/// @brief Compilation diagnostic
struct CompileDiagnostic {
    DiagnosticSeverity severity;
    std::filesystem::path file;
    std::size_t line = 0;
    std::size_t column = 0;
    std::string code;
    std::string message;
};

/// @brief Compilation result
struct CompileResult {
    CompileStatus status = CompileStatus::Pending;
    std::filesystem::path output_path;
    std::filesystem::path pdb_path;

    std::vector<CompileDiagnostic> diagnostics;

    std::chrono::milliseconds compile_time{0};
    std::chrono::milliseconds link_time{0};

    std::size_t error_count = 0;
    std::size_t warning_count = 0;

    [[nodiscard]] bool success() const {
        return status == CompileStatus::Success || status == CompileStatus::Warning;
    }

    [[nodiscard]] bool has_errors() const {
        return status == CompileStatus::Error;
    }

    [[nodiscard]] std::vector<CompileDiagnostic> errors() const {
        std::vector<CompileDiagnostic> result;
        for (const auto& d : diagnostics) {
            if (d.severity == DiagnosticSeverity::Error ||
                d.severity == DiagnosticSeverity::Fatal) {
                result.push_back(d);
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<CompileDiagnostic> warnings() const {
        std::vector<CompileDiagnostic> result;
        for (const auto& d : diagnostics) {
            if (d.severity == DiagnosticSeverity::Warning) {
                result.push_back(d);
            }
        }
        return result;
    }
};

// =============================================================================
// Module Types
// =============================================================================

/// @brief Module state
enum class ModuleState : std::uint8_t {
    Unloaded,
    Loading,
    Loaded,
    Active,
    Unloading,
    Error
};

/// @brief Symbol type
enum class SymbolType : std::uint8_t {
    Function,
    Variable,
    VTable,
    TypeInfo,
    Unknown
};

/// @brief Symbol visibility
enum class SymbolVisibility : std::uint8_t {
    Default,
    Hidden,
    Protected,
    Internal
};

/// @brief Symbol information
struct SymbolInfo {
    SymbolId id;
    std::string name;
    std::string demangled_name;
    SymbolType type = SymbolType::Unknown;
    SymbolVisibility visibility = SymbolVisibility::Default;
    void* address = nullptr;
    std::size_t size = 0;
};

/// @brief Module information
struct ModuleInfo {
    ModuleId id;
    std::string name;
    std::filesystem::path path;
    std::filesystem::path pdb_path;
    ModuleState state = ModuleState::Unloaded;

    std::chrono::system_clock::time_point load_time;
    std::chrono::system_clock::time_point file_time;

    std::vector<SymbolInfo> symbols;
    std::vector<std::string> dependencies;

    std::size_t size_bytes = 0;
};

// =============================================================================
// Hot Reload Types
// =============================================================================

/// @brief File change type
enum class FileChangeType : std::uint8_t {
    Created,
    Modified,
    Deleted,
    Renamed
};

/// @brief File change event
struct FileChangeEvent {
    FileChangeType type;
    std::filesystem::path path;
    std::filesystem::path old_path;  // For renamed files
    std::chrono::system_clock::time_point timestamp;
};

/// @brief Reload callback type
using ReloadCallback = std::function<void(ModuleId module_id, bool success)>;

/// @brief Pre-reload callback (for state saving)
using PreReloadCallback = std::function<void(ModuleId module_id, void** state)>;

/// @brief Post-reload callback (for state restoring)
using PostReloadCallback = std::function<void(ModuleId module_id, void* state)>;

// =============================================================================
// Error Types
// =============================================================================

/// @brief C++ module errors
enum class CppError {
    None = 0,

    // Compiler errors
    CompilerNotFound,
    CompilationFailed,
    LinkFailed,
    InvalidSource,
    MissingDependency,

    // Module errors
    ModuleNotFound,
    LoadFailed,
    UnloadFailed,
    SymbolNotFound,
    InvalidModule,

    // Hot reload errors
    ReloadFailed,
    StatePreservationFailed,
    StateRestorationFailed,
    FileWatchFailed,

    // General errors
    InvalidPath,
    IoError,
    Timeout,

    Count
};

/// @brief Get error name
[[nodiscard]] constexpr const char* cpp_error_name(CppError error);

/// @brief C++ exception
class CppException : public std::exception {
public:
    CppException(CppError error, std::string message);

    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }
    [[nodiscard]] CppError error() const { return error_; }
    [[nodiscard]] const std::string& message() const { return message_; }

private:
    CppError error_;
    std::string message_;
};

/// @brief Result type for C++ operations
template <typename T>
using CppResult = void_core::Result<T, CppError>;

// =============================================================================
// Implementation
// =============================================================================

constexpr const char* cpp_error_name(CppError error) {
    switch (error) {
        case CppError::None: return "None";
        case CppError::CompilerNotFound: return "Compiler not found";
        case CppError::CompilationFailed: return "Compilation failed";
        case CppError::LinkFailed: return "Link failed";
        case CppError::InvalidSource: return "Invalid source";
        case CppError::MissingDependency: return "Missing dependency";
        case CppError::ModuleNotFound: return "Module not found";
        case CppError::LoadFailed: return "Load failed";
        case CppError::UnloadFailed: return "Unload failed";
        case CppError::SymbolNotFound: return "Symbol not found";
        case CppError::InvalidModule: return "Invalid module";
        case CppError::ReloadFailed: return "Reload failed";
        case CppError::StatePreservationFailed: return "State preservation failed";
        case CppError::StateRestorationFailed: return "State restoration failed";
        case CppError::FileWatchFailed: return "File watch failed";
        case CppError::InvalidPath: return "Invalid path";
        case CppError::IoError: return "I/O error";
        case CppError::Timeout: return "Timeout";
        default: return "Unknown error";
    }
}

} // namespace void_cpp
