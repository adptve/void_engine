#pragma once

/// @file wasm_interpreter.hpp
/// @brief WebAssembly bytecode interpreter

#include "types.hpp"
#include "wasm.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <stack>
#include <variant>
#include <vector>

namespace void_scripting {

// =============================================================================
// WASM Opcodes
// =============================================================================

enum class WasmOpcode : std::uint8_t {
    // Control flow
    Unreachable = 0x00,
    Nop = 0x01,
    Block = 0x02,
    Loop = 0x03,
    If = 0x04,
    Else = 0x05,
    End = 0x0B,
    Br = 0x0C,
    BrIf = 0x0D,
    BrTable = 0x0E,
    Return = 0x0F,
    Call = 0x10,
    CallIndirect = 0x11,

    // Parametric
    Drop = 0x1A,
    Select = 0x1B,

    // Variable access
    LocalGet = 0x20,
    LocalSet = 0x21,
    LocalTee = 0x22,
    GlobalGet = 0x23,
    GlobalSet = 0x24,

    // Memory
    I32Load = 0x28,
    I64Load = 0x29,
    F32Load = 0x2A,
    F64Load = 0x2B,
    I32Load8S = 0x2C,
    I32Load8U = 0x2D,
    I32Load16S = 0x2E,
    I32Load16U = 0x2F,
    I64Load8S = 0x30,
    I64Load8U = 0x31,
    I64Load16S = 0x32,
    I64Load16U = 0x33,
    I64Load32S = 0x34,
    I64Load32U = 0x35,
    I32Store = 0x36,
    I64Store = 0x37,
    F32Store = 0x38,
    F64Store = 0x39,
    I32Store8 = 0x3A,
    I32Store16 = 0x3B,
    I64Store8 = 0x3C,
    I64Store16 = 0x3D,
    I64Store32 = 0x3E,
    MemorySize = 0x3F,
    MemoryGrow = 0x40,

    // Constants
    I32Const = 0x41,
    I64Const = 0x42,
    F32Const = 0x43,
    F64Const = 0x44,

    // i32 comparison
    I32Eqz = 0x45,
    I32Eq = 0x46,
    I32Ne = 0x47,
    I32LtS = 0x48,
    I32LtU = 0x49,
    I32GtS = 0x4A,
    I32GtU = 0x4B,
    I32LeS = 0x4C,
    I32LeU = 0x4D,
    I32GeS = 0x4E,
    I32GeU = 0x4F,

    // i64 comparison
    I64Eqz = 0x50,
    I64Eq = 0x51,
    I64Ne = 0x52,
    I64LtS = 0x53,
    I64LtU = 0x54,
    I64GtS = 0x55,
    I64GtU = 0x56,
    I64LeS = 0x57,
    I64LeU = 0x58,
    I64GeS = 0x59,
    I64GeU = 0x5A,

    // f32 comparison
    F32Eq = 0x5B,
    F32Ne = 0x5C,
    F32Lt = 0x5D,
    F32Gt = 0x5E,
    F32Le = 0x5F,
    F32Ge = 0x60,

    // f64 comparison
    F64Eq = 0x61,
    F64Ne = 0x62,
    F64Lt = 0x63,
    F64Gt = 0x64,
    F64Le = 0x65,
    F64Ge = 0x66,

    // i32 arithmetic
    I32Clz = 0x67,
    I32Ctz = 0x68,
    I32Popcnt = 0x69,
    I32Add = 0x6A,
    I32Sub = 0x6B,
    I32Mul = 0x6C,
    I32DivS = 0x6D,
    I32DivU = 0x6E,
    I32RemS = 0x6F,
    I32RemU = 0x70,
    I32And = 0x71,
    I32Or = 0x72,
    I32Xor = 0x73,
    I32Shl = 0x74,
    I32ShrS = 0x75,
    I32ShrU = 0x76,
    I32Rotl = 0x77,
    I32Rotr = 0x78,

    // i64 arithmetic
    I64Clz = 0x79,
    I64Ctz = 0x7A,
    I64Popcnt = 0x7B,
    I64Add = 0x7C,
    I64Sub = 0x7D,
    I64Mul = 0x7E,
    I64DivS = 0x7F,
    I64DivU = 0x80,
    I64RemS = 0x81,
    I64RemU = 0x82,
    I64And = 0x83,
    I64Or = 0x84,
    I64Xor = 0x85,
    I64Shl = 0x86,
    I64ShrS = 0x87,
    I64ShrU = 0x88,
    I64Rotl = 0x89,
    I64Rotr = 0x8A,

    // f32 arithmetic
    F32Abs = 0x8B,
    F32Neg = 0x8C,
    F32Ceil = 0x8D,
    F32Floor = 0x8E,
    F32Trunc = 0x8F,
    F32Nearest = 0x90,
    F32Sqrt = 0x91,
    F32Add = 0x92,
    F32Sub = 0x93,
    F32Mul = 0x94,
    F32Div = 0x95,
    F32Min = 0x96,
    F32Max = 0x97,
    F32Copysign = 0x98,

    // f64 arithmetic
    F64Abs = 0x99,
    F64Neg = 0x9A,
    F64Ceil = 0x9B,
    F64Floor = 0x9C,
    F64Trunc = 0x9D,
    F64Nearest = 0x9E,
    F64Sqrt = 0x9F,
    F64Add = 0xA0,
    F64Sub = 0xA1,
    F64Mul = 0xA2,
    F64Div = 0xA3,
    F64Min = 0xA4,
    F64Max = 0xA5,
    F64Copysign = 0xA6,

    // Conversions
    I32WrapI64 = 0xA7,
    I32TruncF32S = 0xA8,
    I32TruncF32U = 0xA9,
    I32TruncF64S = 0xAA,
    I32TruncF64U = 0xAB,
    I64ExtendI32S = 0xAC,
    I64ExtendI32U = 0xAD,
    I64TruncF32S = 0xAE,
    I64TruncF32U = 0xAF,
    I64TruncF64S = 0xB0,
    I64TruncF64U = 0xB1,
    F32ConvertI32S = 0xB2,
    F32ConvertI32U = 0xB3,
    F32ConvertI64S = 0xB4,
    F32ConvertI64U = 0xB5,
    F32DemoteF64 = 0xB6,
    F64ConvertI32S = 0xB7,
    F64ConvertI32U = 0xB8,
    F64ConvertI64S = 0xB9,
    F64ConvertI64U = 0xBA,
    F64PromoteF32 = 0xBB,
    I32ReinterpretF32 = 0xBC,
    I64ReinterpretF64 = 0xBD,
    F32ReinterpretI32 = 0xBE,
    F64ReinterpretI64 = 0xBF,

    // Sign extension
    I32Extend8S = 0xC0,
    I32Extend16S = 0xC1,
    I64Extend8S = 0xC2,
    I64Extend16S = 0xC3,
    I64Extend32S = 0xC4,

    // Prefix for multi-byte opcodes
    PrefixFC = 0xFC,
    PrefixFD = 0xFD,
};

// =============================================================================
// Interpreter Value
// =============================================================================

/// @brief Stack value union
union StackValue {
    std::int32_t i32;
    std::uint32_t u32;
    std::int64_t i64;
    std::uint64_t u64;
    float f32;
    double f64;

    StackValue() : i64(0) {}
    explicit StackValue(std::int32_t v) : i32(v) {}
    explicit StackValue(std::uint32_t v) : u32(v) {}
    explicit StackValue(std::int64_t v) : i64(v) {}
    explicit StackValue(std::uint64_t v) : u64(v) {}
    explicit StackValue(float v) : f32(v) {}
    explicit StackValue(double v) : f64(v) {}
};

// =============================================================================
// Control Frame
// =============================================================================

enum class LabelKind {
    Block,
    Loop,
    If,
    Else,
    Function
};

struct Label {
    LabelKind kind;
    std::size_t pc;              // Program counter (for loop continuation)
    std::size_t stack_height;    // Stack height at entry
    std::size_t arity;           // Number of results
    std::size_t end_pc;          // PC of matching end
};

// =============================================================================
// Call Frame
// =============================================================================

struct CallFrame {
    std::uint32_t function_index;
    std::size_t return_pc;
    std::size_t stack_base;
    std::vector<StackValue> locals;
    std::vector<Label> labels;
};

// =============================================================================
// Parsed Function
// =============================================================================

struct WasmFunction {
    std::uint32_t type_index;
    std::vector<WasmValType> locals;
    std::vector<std::uint8_t> code;
    std::size_t code_offset;  // Offset in module binary
};

// =============================================================================
// Parsed Module (for interpretation)
// =============================================================================

struct ParsedModule {
    std::vector<WasmFunctionType> types;
    std::vector<WasmImport> imports;
    std::vector<std::uint32_t> function_type_indices;
    std::vector<WasmFunction> functions;
    std::vector<WasmExport> exports;
    std::optional<std::uint32_t> start_function;

    // Memory
    std::size_t initial_memory_pages = 0;
    std::optional<std::size_t> max_memory_pages;
    std::vector<std::pair<std::size_t, std::vector<std::uint8_t>>> data_segments;

    // Globals
    struct GlobalDef {
        WasmValType type;
        bool mutable_;
        StackValue init_value;
    };
    std::vector<GlobalDef> globals;

    // Tables
    struct TableDef {
        WasmValType elem_type;
        std::size_t min;
        std::optional<std::size_t> max;
    };
    std::vector<TableDef> tables;
    std::vector<std::vector<std::uint32_t>> elem_segments;

    // Count imported functions (to offset function indices)
    std::uint32_t num_imported_functions = 0;
};

// =============================================================================
// WASM Interpreter
// =============================================================================

class WasmInterpreter {
public:
    using HostFunction = std::function<WasmResult<std::vector<WasmValue>>(
        std::span<const WasmValue>, void*)>;

    struct HostFunctionEntry {
        std::string module;
        std::string name;
        WasmFunctionType signature;
        HostFunction callback;
        void* user_data = nullptr;
    };

    WasmInterpreter();
    ~WasmInterpreter();

    /// @brief Parse a module for interpretation
    WasmResult<ParsedModule> parse_module(std::span<const std::uint8_t> binary);

    /// @brief Execute a function
    WasmResult<std::vector<WasmValue>> execute(
        const ParsedModule& module,
        WasmMemory& memory,
        std::uint32_t function_index,
        std::span<const WasmValue> args);

    /// @brief Register a host function
    void register_host_function(HostFunctionEntry entry);

    /// @brief Set fuel limit (0 = unlimited)
    void set_fuel(std::uint64_t fuel) { fuel_ = fuel; fuel_enabled_ = fuel > 0; }

    /// @brief Get remaining fuel
    std::uint64_t remaining_fuel() const { return fuel_; }

private:
    // Execution state
    std::vector<StackValue> stack_;
    std::vector<CallFrame> call_stack_;
    std::vector<StackValue> globals_;
    std::vector<std::vector<std::uint32_t>> tables_;

    // Host functions
    std::unordered_map<std::string, HostFunctionEntry> host_functions_;

    // Fuel
    std::uint64_t fuel_ = 0;
    bool fuel_enabled_ = false;

    // Parsing helpers
    std::size_t pos_ = 0;
    std::span<const std::uint8_t> binary_;

    std::uint8_t read_byte();
    std::uint32_t read_u32_leb128();
    std::int32_t read_i32_leb128();
    std::int64_t read_i64_leb128();
    float read_f32();
    double read_f64();
    std::string read_name();
    WasmValType read_valtype();

    // Section parsing
    void parse_type_section(ParsedModule& module);
    void parse_import_section(ParsedModule& module);
    void parse_function_section(ParsedModule& module);
    void parse_table_section(ParsedModule& module);
    void parse_memory_section(ParsedModule& module);
    void parse_global_section(ParsedModule& module);
    void parse_export_section(ParsedModule& module);
    void parse_start_section(ParsedModule& module);
    void parse_element_section(ParsedModule& module);
    void parse_code_section(ParsedModule& module);
    void parse_data_section(ParsedModule& module);

    // Execution
    WasmResult<void> execute_function(
        const ParsedModule& module,
        WasmMemory& memory,
        std::uint32_t func_index);

    WasmResult<void> execute_code(
        const ParsedModule& module,
        WasmMemory& memory,
        const std::vector<std::uint8_t>& code);

    // Stack operations
    void push(StackValue v) { stack_.push_back(v); }
    StackValue pop();
    StackValue& top() { return stack_.back(); }

    // Label operations
    void push_label(LabelKind kind, std::size_t pc, std::size_t arity, std::size_t end_pc);
    void pop_label();
    Label& get_label(std::uint32_t depth);

    // Branch
    void branch(std::uint32_t depth);

    // Host function calls
    WasmResult<std::vector<WasmValue>> call_host_function(
        const std::string& module,
        const std::string& name,
        std::span<const WasmValue> args);

    // Helpers
    bool consume_fuel();
    StackValue evaluate_init_expr(std::span<const std::uint8_t> code);
};

} // namespace void_scripting
