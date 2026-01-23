#pragma once

/// @file types.hpp
/// @brief Core types for void_scripting module

#include "fwd.hpp"
#include <void_engine/core/error.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace void_scripting {

// =============================================================================
// WASM Value Types
// =============================================================================

/// @brief WASM value types
enum class WasmValType : std::uint8_t {
    I32,        ///< 32-bit integer
    I64,        ///< 64-bit integer
    F32,        ///< 32-bit float
    F64,        ///< 64-bit float
    V128,       ///< 128-bit vector
    FuncRef,    ///< Function reference
    ExternRef,  ///< External reference

    Count
};

/// @brief WASM value
struct WasmValue {
    WasmValType type = WasmValType::I32;

    union {
        std::int32_t i32;
        std::int64_t i64;
        float f32;
        double f64;
        std::array<std::uint8_t, 16> v128;
        void* ref;
    };

    WasmValue() : i64(0) {}
    WasmValue(std::int32_t v) : type(WasmValType::I32), i32(v) {}
    WasmValue(std::int64_t v) : type(WasmValType::I64), i64(v) {}
    WasmValue(float v) : type(WasmValType::F32), f32(v) {}
    WasmValue(double v) : type(WasmValType::F64), f64(v) {}

    [[nodiscard]] std::string to_string() const;
};

// =============================================================================
// WASM Type Definitions
// =============================================================================

/// @brief WASM function signature
struct WasmFunctionType {
    std::vector<WasmValType> params;
    std::vector<WasmValType> results;

    [[nodiscard]] std::string to_string() const;
};

/// @brief WASM memory limits
struct WasmLimits {
    std::uint32_t min = 0;
    std::optional<std::uint32_t> max;
};

/// @brief WASM memory type
struct WasmMemoryType {
    WasmLimits limits;
    bool shared = false;
};

/// @brief WASM table type
struct WasmTableType {
    WasmValType element_type = WasmValType::FuncRef;
    WasmLimits limits;
};

/// @brief WASM global type
struct WasmGlobalType {
    WasmValType value_type = WasmValType::I32;
    bool mutable_ = false;
};

// =============================================================================
// Import/Export Types
// =============================================================================

/// @brief Import/Export kind
enum class WasmExternKind : std::uint8_t {
    Func,
    Table,
    Memory,
    Global
};

/// @brief Import descriptor
struct WasmImport {
    std::string module;
    std::string name;
    WasmExternKind kind;

    // Type info based on kind
    std::optional<WasmFunctionType> func_type;
    std::optional<WasmTableType> table_type;
    std::optional<WasmMemoryType> memory_type;
    std::optional<WasmGlobalType> global_type;
};

/// @brief Export descriptor
struct WasmExport {
    std::string name;
    WasmExternKind kind;
    std::uint32_t index;
};

// =============================================================================
// Module Info
// =============================================================================

/// @brief Information about a compiled WASM module
struct WasmModuleInfo {
    std::string name;
    std::vector<WasmImport> imports;
    std::vector<WasmExport> exports;

    // Sections
    std::size_t num_functions = 0;
    std::size_t num_tables = 0;
    std::size_t num_memories = 0;
    std::size_t num_globals = 0;

    // Custom sections
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> custom_sections;

    // Start function
    std::optional<std::uint32_t> start_function;
};

// =============================================================================
// Runtime Configuration
// =============================================================================

/// @brief WASM runtime backend
enum class WasmBackend : std::uint8_t {
    Wasmtime,   ///< wasmtime (Rust-based, fast)
    Wasmer,     ///< wasmer (Rust-based)
    Wasm3,      ///< wasm3 (C, interpreter)
    V8,         ///< V8 JavaScript engine
    Native,     ///< Native execution (AOT compiled)

    Default = Wasmtime
};

/// @brief Runtime configuration
struct WasmConfig {
    WasmBackend backend = WasmBackend::Default;

    // Memory limits
    std::size_t max_memory_pages = 65536;  ///< 4 GB max
    std::size_t max_table_elements = 10000;
    std::size_t max_instances = 1000;

    // Execution limits
    std::size_t max_stack_size = 1024 * 1024;  ///< 1 MB
    std::uint64_t fuel_limit = 0;  ///< 0 = unlimited

    // Features
    bool enable_simd = true;
    bool enable_threads = true;
    bool enable_reference_types = true;
    bool enable_bulk_memory = true;
    bool enable_multi_value = true;

    // Debug
    bool enable_debug_info = false;
    bool enable_profiling = false;
};

// =============================================================================
// Error Types
// =============================================================================

/// @brief WASM error types
enum class WasmError {
    None = 0,

    // Compilation errors
    InvalidModule,
    CompilationFailed,
    ValidationFailed,
    UnsupportedFeature,

    // Linking errors
    ImportNotFound,
    ImportTypeMismatch,
    ExportNotFound,

    // Runtime errors
    OutOfMemory,
    StackOverflow,
    StackUnderflow,
    Unreachable,
    DivisionByZero,
    IntegerOverflow,
    InvalidConversion,
    IndirectCallTypeMismatch,
    UndefinedElement,
    UninitializedElement,
    OutOfBounds,
    Trap,
    FuelExhausted,

    // Host errors
    HostFunctionFailed,
    InvalidArguments,

    Count
};

/// @brief Get error name
[[nodiscard]] constexpr const char* wasm_error_name(WasmError error);

/// @brief WASM exception
class WasmException : public std::exception {
public:
    WasmException(WasmError error, std::string message);

    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }
    [[nodiscard]] WasmError error() const { return error_; }
    [[nodiscard]] const std::string& message() const { return message_; }

private:
    WasmError error_;
    std::string message_;
};

/// @brief Result type for WASM operations
template <typename T>
using WasmResult = void_core::Result<T, void_core::Error>;

// =============================================================================
// Callback Types
// =============================================================================

/// @brief Host function callback signature
using HostFunctionCallback = std::function<WasmResult<std::vector<WasmValue>>(
    std::span<const WasmValue> args, void* user_data)>;

/// @brief Memory access callback
using MemoryAccessCallback = std::function<void(
    std::size_t offset, std::size_t size, bool is_write)>;

// =============================================================================
// Implementation
// =============================================================================

constexpr const char* wasm_error_name(WasmError error) {
    switch (error) {
        case WasmError::None: return "None";
        case WasmError::InvalidModule: return "Invalid module";
        case WasmError::CompilationFailed: return "Compilation failed";
        case WasmError::ValidationFailed: return "Validation failed";
        case WasmError::UnsupportedFeature: return "Unsupported feature";
        case WasmError::ImportNotFound: return "Import not found";
        case WasmError::ImportTypeMismatch: return "Import type mismatch";
        case WasmError::ExportNotFound: return "Export not found";
        case WasmError::OutOfMemory: return "Out of memory";
        case WasmError::StackOverflow: return "Stack overflow";
        case WasmError::StackUnderflow: return "Stack underflow";
        case WasmError::Unreachable: return "Unreachable";
        case WasmError::DivisionByZero: return "Division by zero";
        case WasmError::IntegerOverflow: return "Integer overflow";
        case WasmError::InvalidConversion: return "Invalid conversion";
        case WasmError::IndirectCallTypeMismatch: return "Indirect call type mismatch";
        case WasmError::UndefinedElement: return "Undefined element";
        case WasmError::UninitializedElement: return "Uninitialized element";
        case WasmError::OutOfBounds: return "Out of bounds";
        case WasmError::Trap: return "Trap";
        case WasmError::FuelExhausted: return "Fuel exhausted";
        case WasmError::HostFunctionFailed: return "Host function failed";
        case WasmError::InvalidArguments: return "Invalid arguments";
        default: return "Unknown error";
    }
}

} // namespace void_scripting
