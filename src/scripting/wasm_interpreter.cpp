/// @file wasm_interpreter.cpp
/// @brief WebAssembly bytecode interpreter implementation

#include "wasm_interpreter.hpp"

#include <void_engine/core/log.hpp>

#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace void_scripting {

// =============================================================================
// Bit manipulation helpers
// =============================================================================

namespace {

inline std::uint32_t rotl32(std::uint32_t n, std::uint32_t c) {
    const std::uint32_t mask = 31;
    c &= mask;
    return (n << c) | (n >> ((-c) & mask));
}

inline std::uint32_t rotr32(std::uint32_t n, std::uint32_t c) {
    const std::uint32_t mask = 31;
    c &= mask;
    return (n >> c) | (n << ((-c) & mask));
}

inline std::uint64_t rotl64(std::uint64_t n, std::uint64_t c) {
    const std::uint64_t mask = 63;
    c &= mask;
    return (n << c) | (n >> ((-c) & mask));
}

inline std::uint64_t rotr64(std::uint64_t n, std::uint64_t c) {
    const std::uint64_t mask = 63;
    c &= mask;
    return (n >> c) | (n << ((-c) & mask));
}

inline int clz32(std::uint32_t v) {
    if (v == 0) return 32;
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanReverse(&idx, v);
    return 31 - idx;
#else
    return __builtin_clz(v);
#endif
}

inline int ctz32(std::uint32_t v) {
    if (v == 0) return 32;
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward(&idx, v);
    return idx;
#else
    return __builtin_ctz(v);
#endif
}

inline int popcnt32(std::uint32_t v) {
#if defined(_MSC_VER)
    return __popcnt(v);
#else
    return __builtin_popcount(v);
#endif
}

inline int clz64(std::uint64_t v) {
    if (v == 0) return 64;
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanReverse64(&idx, v);
    return 63 - idx;
#else
    return __builtin_clzll(v);
#endif
}

inline int ctz64(std::uint64_t v) {
    if (v == 0) return 64;
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, v);
    return idx;
#else
    return __builtin_ctzll(v);
#endif
}

inline int popcnt64(std::uint64_t v) {
#if defined(_MSC_VER)
    return static_cast<int>(__popcnt64(v));
#else
    return __builtin_popcountll(v);
#endif
}

} // anonymous namespace

// =============================================================================
// WasmInterpreter Implementation
// =============================================================================

WasmInterpreter::WasmInterpreter() {
    stack_.reserve(1024);
    call_stack_.reserve(64);
}

WasmInterpreter::~WasmInterpreter() = default;

// =============================================================================
// Parsing helpers
// =============================================================================

std::uint8_t WasmInterpreter::read_byte() {
    if (pos_ >= binary_.size()) {
        throw WasmException(WasmError::InvalidModule, "Unexpected end of binary");
    }
    return binary_[pos_++];
}

std::uint32_t WasmInterpreter::read_u32_leb128() {
    std::uint32_t result = 0;
    std::uint32_t shift = 0;
    std::uint8_t byte;
    do {
        byte = read_byte();
        result |= static_cast<std::uint32_t>(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    return result;
}

std::int32_t WasmInterpreter::read_i32_leb128() {
    std::int32_t result = 0;
    std::uint32_t shift = 0;
    std::uint8_t byte;
    do {
        byte = read_byte();
        result |= static_cast<std::int32_t>(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    // Sign extend
    if (shift < 32 && (byte & 0x40)) {
        result |= (~0u << shift);
    }
    return result;
}

std::int64_t WasmInterpreter::read_i64_leb128() {
    std::int64_t result = 0;
    std::uint64_t shift = 0;
    std::uint8_t byte;
    do {
        byte = read_byte();
        result |= static_cast<std::int64_t>(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    // Sign extend
    if (shift < 64 && (byte & 0x40)) {
        result |= (~0ull << shift);
    }
    return result;
}

float WasmInterpreter::read_f32() {
    float value;
    std::memcpy(&value, binary_.data() + pos_, 4);
    pos_ += 4;
    return value;
}

double WasmInterpreter::read_f64() {
    double value;
    std::memcpy(&value, binary_.data() + pos_, 8);
    pos_ += 8;
    return value;
}

std::string WasmInterpreter::read_name() {
    std::uint32_t len = read_u32_leb128();
    if (pos_ + len > binary_.size()) {
        throw WasmException(WasmError::InvalidModule, "Name too long");
    }
    std::string name(reinterpret_cast<const char*>(binary_.data() + pos_), len);
    pos_ += len;
    return name;
}

WasmValType WasmInterpreter::read_valtype() {
    std::uint8_t type = read_byte();
    switch (type) {
        case 0x7f: return WasmValType::I32;
        case 0x7e: return WasmValType::I64;
        case 0x7d: return WasmValType::F32;
        case 0x7c: return WasmValType::F64;
        case 0x7b: return WasmValType::V128;
        case 0x70: return WasmValType::FuncRef;
        case 0x6f: return WasmValType::ExternRef;
        default:
            throw WasmException(WasmError::InvalidModule, "Unknown value type");
    }
}

// =============================================================================
// Section Parsing
// =============================================================================

void WasmInterpreter::parse_type_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    module.types.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint8_t form = read_byte();
        if (form != 0x60) {
            throw WasmException(WasmError::InvalidModule, "Invalid function type form");
        }

        WasmFunctionType func_type;

        std::uint32_t param_count = read_u32_leb128();
        func_type.params.reserve(param_count);
        for (std::uint32_t j = 0; j < param_count; ++j) {
            func_type.params.push_back(read_valtype());
        }

        std::uint32_t result_count = read_u32_leb128();
        func_type.results.reserve(result_count);
        for (std::uint32_t j = 0; j < result_count; ++j) {
            func_type.results.push_back(read_valtype());
        }

        module.types.push_back(std::move(func_type));
    }
}

void WasmInterpreter::parse_import_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    module.imports.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        WasmImport import;
        import.module = read_name();
        import.name = read_name();

        std::uint8_t kind = read_byte();
        switch (kind) {
            case 0x00: { // Function
                import.kind = WasmExternKind::Func;
                std::uint32_t type_idx = read_u32_leb128();
                if (type_idx < module.types.size()) {
                    import.func_type = module.types[type_idx];
                }
                module.num_imported_functions++;
                break;
            }
            case 0x01: { // Table
                import.kind = WasmExternKind::Table;
                WasmTableType table;
                table.element_type = read_valtype();
                std::uint8_t limits_flags = read_byte();
                table.limits.min = read_u32_leb128();
                if (limits_flags & 0x01) {
                    table.limits.max = read_u32_leb128();
                }
                import.table_type = table;
                break;
            }
            case 0x02: { // Memory
                import.kind = WasmExternKind::Memory;
                WasmMemoryType memory;
                std::uint8_t limits_flags = read_byte();
                memory.limits.min = read_u32_leb128();
                if (limits_flags & 0x01) {
                    memory.limits.max = read_u32_leb128();
                }
                memory.shared = (limits_flags & 0x02) != 0;
                import.memory_type = memory;
                break;
            }
            case 0x03: { // Global
                import.kind = WasmExternKind::Global;
                WasmGlobalType global;
                global.value_type = read_valtype();
                global.mutable_ = read_byte() != 0;
                import.global_type = global;
                break;
            }
            default:
                throw WasmException(WasmError::InvalidModule, "Unknown import kind");
        }

        module.imports.push_back(std::move(import));
    }
}

void WasmInterpreter::parse_function_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    module.function_type_indices.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        module.function_type_indices.push_back(read_u32_leb128());
    }
}

void WasmInterpreter::parse_table_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    module.tables.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        ParsedModule::TableDef table;
        table.elem_type = read_valtype();
        std::uint8_t limits_flags = read_byte();
        table.min = read_u32_leb128();
        if (limits_flags & 0x01) {
            table.max = read_u32_leb128();
        }
        module.tables.push_back(table);
    }
}

void WasmInterpreter::parse_memory_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    if (count > 0) {
        std::uint8_t limits_flags = read_byte();
        module.initial_memory_pages = read_u32_leb128();
        if (limits_flags & 0x01) {
            module.max_memory_pages = read_u32_leb128();
        }
    }
}

void WasmInterpreter::parse_global_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    module.globals.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        ParsedModule::GlobalDef global;
        global.type = read_valtype();
        global.mutable_ = read_byte() != 0;

        // Parse init expression
        std::vector<std::uint8_t> init_code;
        while (true) {
            std::uint8_t byte = read_byte();
            init_code.push_back(byte);
            if (byte == 0x0B) break;  // end
        }
        global.init_value = evaluate_init_expr(init_code);
        module.globals.push_back(global);
    }
}

void WasmInterpreter::parse_export_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    module.exports.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        WasmExport exp;
        exp.name = read_name();

        std::uint8_t kind = read_byte();
        switch (kind) {
            case 0x00: exp.kind = WasmExternKind::Func; break;
            case 0x01: exp.kind = WasmExternKind::Table; break;
            case 0x02: exp.kind = WasmExternKind::Memory; break;
            case 0x03: exp.kind = WasmExternKind::Global; break;
            default:
                throw WasmException(WasmError::InvalidModule, "Unknown export kind");
        }

        exp.index = read_u32_leb128();
        module.exports.push_back(std::move(exp));
    }
}

void WasmInterpreter::parse_start_section(ParsedModule& module) {
    module.start_function = read_u32_leb128();
}

void WasmInterpreter::parse_element_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    module.elem_segments.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t flags = read_u32_leb128();
        std::uint32_t table_idx = 0;

        if (flags & 0x02) {
            table_idx = read_u32_leb128();
        }

        // Skip offset expression for now (active segments)
        if (!(flags & 0x01)) {
            // Active segment - read offset expression
            while (read_byte() != 0x0B) {}
        }

        std::uint32_t elem_count = read_u32_leb128();
        std::vector<std::uint32_t> elems;
        elems.reserve(elem_count);

        for (std::uint32_t j = 0; j < elem_count; ++j) {
            if (flags & 0x04) {
                // Element expressions
                while (read_byte() != 0x0B) {}
            } else {
                elems.push_back(read_u32_leb128());
            }
        }

        module.elem_segments.push_back(std::move(elems));
    }
}

void WasmInterpreter::parse_code_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    module.functions.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        WasmFunction func;
        func.type_index = module.function_type_indices[i];

        std::uint32_t body_size = read_u32_leb128();
        std::size_t body_end = pos_ + body_size;

        // Local declarations
        std::uint32_t local_decl_count = read_u32_leb128();
        for (std::uint32_t j = 0; j < local_decl_count; ++j) {
            std::uint32_t local_count = read_u32_leb128();
            WasmValType local_type = read_valtype();
            for (std::uint32_t k = 0; k < local_count; ++k) {
                func.locals.push_back(local_type);
            }
        }

        // Function code
        func.code_offset = pos_;
        std::size_t code_size = body_end - pos_;
        func.code.assign(binary_.data() + pos_, binary_.data() + body_end);
        pos_ = body_end;

        module.functions.push_back(std::move(func));
    }
}

void WasmInterpreter::parse_data_section(ParsedModule& module) {
    std::uint32_t count = read_u32_leb128();
    module.data_segments.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t flags = read_u32_leb128();
        std::size_t offset = 0;

        if (!(flags & 0x01)) {
            // Active segment - read offset expression
            std::vector<std::uint8_t> init_code;
            while (true) {
                std::uint8_t byte = read_byte();
                init_code.push_back(byte);
                if (byte == 0x0B) break;
            }
            StackValue val = evaluate_init_expr(init_code);
            offset = static_cast<std::size_t>(val.u32);
        }

        std::uint32_t data_size = read_u32_leb128();
        std::vector<std::uint8_t> data(binary_.data() + pos_, binary_.data() + pos_ + data_size);
        pos_ += data_size;

        module.data_segments.emplace_back(offset, std::move(data));
    }
}

// =============================================================================
// Module Parsing
// =============================================================================

WasmResult<ParsedModule> WasmInterpreter::parse_module(std::span<const std::uint8_t> binary) {
    binary_ = binary;
    pos_ = 0;

    // Check magic and version
    if (binary.size() < 8) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid WASM module"};
    }

    if (binary[0] != 0x00 || binary[1] != 0x61 ||
        binary[2] != 0x73 || binary[3] != 0x6d) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid WASM module"};
    }

    std::uint32_t version = binary[4] | (binary[5] << 8) |
                           (binary[6] << 16) | (binary[7] << 24);
    if (version != 1) {
        return void_core::Error{void_core::ErrorCode::NotSupported, "Unsupported WASM feature"};
    }

    pos_ = 8;
    ParsedModule module;

    try {
        while (pos_ < binary.size()) {
            std::uint8_t section_id = read_byte();
            std::uint32_t section_size = read_u32_leb128();
            std::size_t section_end = pos_ + section_size;

            switch (section_id) {
                case 0: // Custom
                    pos_ = section_end;
                    break;
                case 1: // Type
                    parse_type_section(module);
                    break;
                case 2: // Import
                    parse_import_section(module);
                    break;
                case 3: // Function
                    parse_function_section(module);
                    break;
                case 4: // Table
                    parse_table_section(module);
                    break;
                case 5: // Memory
                    parse_memory_section(module);
                    break;
                case 6: // Global
                    parse_global_section(module);
                    break;
                case 7: // Export
                    parse_export_section(module);
                    break;
                case 8: // Start
                    parse_start_section(module);
                    break;
                case 9: // Element
                    parse_element_section(module);
                    break;
                case 10: // Code
                    parse_code_section(module);
                    break;
                case 11: // Data
                    parse_data_section(module);
                    break;
                default:
                    pos_ = section_end;
                    break;
            }

            pos_ = section_end;
        }
    } catch (const WasmException& e) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, e.message()};
    }

    return std::move(module);
}

// =============================================================================
// Init Expression Evaluation
// =============================================================================

StackValue WasmInterpreter::evaluate_init_expr(std::span<const std::uint8_t> code) {
    StackValue result{};
    std::size_t i = 0;

    while (i < code.size()) {
        std::uint8_t op = code[i++];
        switch (static_cast<WasmOpcode>(op)) {
            case WasmOpcode::I32Const: {
                std::int32_t val = 0;
                std::uint32_t shift = 0;
                std::uint8_t byte;
                do {
                    byte = code[i++];
                    val |= static_cast<std::int32_t>(byte & 0x7f) << shift;
                    shift += 7;
                } while (byte & 0x80);
                if (shift < 32 && (byte & 0x40)) val |= (~0u << shift);
                result.i32 = val;
                break;
            }
            case WasmOpcode::I64Const: {
                std::int64_t val = 0;
                std::uint64_t shift = 0;
                std::uint8_t byte;
                do {
                    byte = code[i++];
                    val |= static_cast<std::int64_t>(byte & 0x7f) << shift;
                    shift += 7;
                } while (byte & 0x80);
                if (shift < 64 && (byte & 0x40)) val |= (~0ull << shift);
                result.i64 = val;
                break;
            }
            case WasmOpcode::F32Const:
                std::memcpy(&result.f32, code.data() + i, 4);
                i += 4;
                break;
            case WasmOpcode::F64Const:
                std::memcpy(&result.f64, code.data() + i, 8);
                i += 8;
                break;
            case WasmOpcode::GlobalGet: {
                // Read global index, return 0 for now
                while (code[i++] & 0x80) {}
                result.i64 = 0;
                break;
            }
            case WasmOpcode::End:
                return result;
            default:
                break;
        }
    }

    return result;
}

// =============================================================================
// Stack Operations
// =============================================================================

StackValue WasmInterpreter::pop() {
    if (stack_.empty()) {
        throw WasmException(WasmError::StackUnderflow, "Stack underflow");
    }
    StackValue v = stack_.back();
    stack_.pop_back();
    return v;
}

// =============================================================================
// Label Operations
// =============================================================================

void WasmInterpreter::push_label(LabelKind kind, std::size_t pc, std::size_t arity, std::size_t end_pc) {
    Label label;
    label.kind = kind;
    label.pc = pc;
    label.stack_height = stack_.size();
    label.arity = arity;
    label.end_pc = end_pc;
    call_stack_.back().labels.push_back(label);
}

void WasmInterpreter::pop_label() {
    call_stack_.back().labels.pop_back();
}

Label& WasmInterpreter::get_label(std::uint32_t depth) {
    auto& labels = call_stack_.back().labels;
    return labels[labels.size() - 1 - depth];
}

void WasmInterpreter::branch(std::uint32_t depth) {
    Label& label = get_label(depth);

    // Save result values
    std::vector<StackValue> results;
    for (std::size_t i = 0; i < label.arity; ++i) {
        results.push_back(pop());
    }

    // Unwind stack to label height
    stack_.resize(label.stack_height);

    // Push results back
    for (auto it = results.rbegin(); it != results.rend(); ++it) {
        push(*it);
    }

    // Pop labels up to and including target
    auto& labels = call_stack_.back().labels;
    labels.resize(labels.size() - depth);
}

// =============================================================================
// Host Function Calls
// =============================================================================

void WasmInterpreter::register_host_function(HostFunctionEntry entry) {
    std::string key = entry.module + "." + entry.name;
    host_functions_[key] = std::move(entry);
}

WasmResult<std::vector<WasmValue>> WasmInterpreter::call_host_function(
    const std::string& module,
    const std::string& name,
    std::span<const WasmValue> args) {

    std::string key = module + "." + name;
    auto it = host_functions_.find(key);
    if (it == host_functions_.end()) {
        return void_core::Error{void_core::ErrorCode::NotFound, "WASM export not found"};
    }

    return it->second.callback(args, it->second.user_data);
}

// =============================================================================
// Fuel
// =============================================================================

bool WasmInterpreter::consume_fuel() {
    if (!fuel_enabled_) return true;
    if (fuel_ == 0) return false;
    --fuel_;
    return true;
}

// =============================================================================
// Main Execution
// =============================================================================

WasmResult<std::vector<WasmValue>> WasmInterpreter::execute(
    const ParsedModule& module,
    WasmMemory& memory,
    std::uint32_t function_index,
    std::span<const WasmValue> args) {

    // Initialize globals
    globals_.clear();
    globals_.reserve(module.globals.size());
    for (const auto& g : module.globals) {
        globals_.push_back(g.init_value);
    }

    // Initialize tables
    tables_.clear();
    for (const auto& t : module.tables) {
        tables_.push_back(std::vector<std::uint32_t>(t.min, 0xFFFFFFFF));
    }

    // Apply element segments to tables
    for (std::size_t i = 0; i < module.elem_segments.size() && i < tables_.size(); ++i) {
        const auto& seg = module.elem_segments[i];
        auto& table = tables_[0];  // Simplified: always table 0
        for (std::size_t j = 0; j < seg.size() && j < table.size(); ++j) {
            table[j] = seg[j];
        }
    }

    // Apply data segments to memory
    for (const auto& [offset, data] : module.data_segments) {
        if (offset + data.size() <= memory.size()) {
            memory.write_bytes(offset, data);
        }
    }

    // Clear execution state
    stack_.clear();
    call_stack_.clear();

    // Check if it's an imported function
    if (function_index < module.num_imported_functions) {
        // Find the import
        std::uint32_t import_idx = 0;
        for (const auto& imp : module.imports) {
            if (imp.kind == WasmExternKind::Func) {
                if (import_idx == function_index) {
                    return call_host_function(imp.module, imp.name, args);
                }
                ++import_idx;
            }
        }
        return void_core::Error{void_core::ErrorCode::NotFound, "WASM export not found"};
    }

    // Get the function
    std::uint32_t local_func_idx = function_index - module.num_imported_functions;
    if (local_func_idx >= module.functions.size()) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "WASM invalid function"};
    }

    const WasmFunction& func = module.functions[local_func_idx];
    const WasmFunctionType& func_type = module.types[func.type_index];

    // Create call frame
    CallFrame frame;
    frame.function_index = function_index;
    frame.return_pc = 0;
    frame.stack_base = 0;

    // Initialize locals from parameters
    for (std::size_t i = 0; i < args.size(); ++i) {
        StackValue v;
        switch (args[i].type) {
            case WasmValType::I32: v.i32 = args[i].i32; break;
            case WasmValType::I64: v.i64 = args[i].i64; break;
            case WasmValType::F32: v.f32 = args[i].f32; break;
            case WasmValType::F64: v.f64 = args[i].f64; break;
            default: v.i64 = 0; break;
        }
        frame.locals.push_back(v);
    }

    // Add declared locals (initialized to 0)
    for (const auto& local_type : func.locals) {
        frame.locals.push_back(StackValue{});
    }

    call_stack_.push_back(std::move(frame));

    // Push function label
    push_label(LabelKind::Function, 0, func_type.results.size(), func.code.size());

    // Execute the code
    auto result = execute_code(module, memory, func.code);
    if (!result) {
        return result.error();
    }

    // Collect results
    std::vector<WasmValue> results;
    for (std::size_t i = 0; i < func_type.results.size(); ++i) {
        if (stack_.empty()) {
            return void_core::Error{void_core::ErrorCode::InvalidState, "WASM stack underflow"};
        }
        StackValue v = pop();
        WasmValue wv;
        wv.type = func_type.results[func_type.results.size() - 1 - i];
        switch (wv.type) {
            case WasmValType::I32: wv.i32 = v.i32; break;
            case WasmValType::I64: wv.i64 = v.i64; break;
            case WasmValType::F32: wv.f32 = v.f32; break;
            case WasmValType::F64: wv.f64 = v.f64; break;
            default: break;
        }
        results.push_back(wv);
    }

    std::reverse(results.begin(), results.end());
    return std::move(results);
}

WasmResult<void> WasmInterpreter::execute_code(
    const ParsedModule& module,
    WasmMemory& memory,
    const std::vector<std::uint8_t>& code) {

    std::size_t pc = 0;
    CallFrame* frame = &call_stack_.back();

    auto read_byte_local = [&]() -> std::uint8_t {
        return code[pc++];
    };

    auto read_u32_leb_local = [&]() -> std::uint32_t {
        std::uint32_t result = 0;
        std::uint32_t shift = 0;
        std::uint8_t byte;
        do {
            byte = code[pc++];
            result |= static_cast<std::uint32_t>(byte & 0x7f) << shift;
            shift += 7;
        } while (byte & 0x80);
        return result;
    };

    auto read_i32_leb_local = [&]() -> std::int32_t {
        std::int32_t result = 0;
        std::uint32_t shift = 0;
        std::uint8_t byte;
        do {
            byte = code[pc++];
            result |= static_cast<std::int32_t>(byte & 0x7f) << shift;
            shift += 7;
        } while (byte & 0x80);
        if (shift < 32 && (byte & 0x40)) result |= (~0u << shift);
        return result;
    };

    auto read_i64_leb_local = [&]() -> std::int64_t {
        std::int64_t result = 0;
        std::uint64_t shift = 0;
        std::uint8_t byte;
        do {
            byte = code[pc++];
            result |= static_cast<std::int64_t>(byte & 0x7f) << shift;
            shift += 7;
        } while (byte & 0x80);
        if (shift < 64 && (byte & 0x40)) result |= (~0ull << shift);
        return result;
    };

    auto read_f32_local = [&]() -> float {
        float v;
        std::memcpy(&v, code.data() + pc, 4);
        pc += 4;
        return v;
    };

    auto read_f64_local = [&]() -> double {
        double v;
        std::memcpy(&v, code.data() + pc, 8);
        pc += 8;
        return v;
    };

    // Find matching end for block/loop/if
    auto find_end = [&](std::size_t start) -> std::size_t {
        std::size_t depth = 1;
        std::size_t i = start;
        while (i < code.size() && depth > 0) {
            std::uint8_t op = code[i++];
            switch (static_cast<WasmOpcode>(op)) {
                case WasmOpcode::Block:
                case WasmOpcode::Loop:
                case WasmOpcode::If:
                    ++depth;
                    // Skip block type
                    if (code[i] == 0x40 || (code[i] >= 0x7c && code[i] <= 0x7f)) {
                        ++i;
                    } else {
                        while (code[i++] & 0x80) {}
                    }
                    break;
                case WasmOpcode::End:
                    --depth;
                    break;
                default:
                    // Skip instruction operands (simplified)
                    break;
            }
        }
        return i;
    };

    while (pc < code.size()) {
        if (!consume_fuel()) {
            return void_core::Error{void_core::ErrorCode::Timeout, "WASM fuel exhausted"};
        }

        std::uint8_t opcode = read_byte_local();
        WasmOpcode op = static_cast<WasmOpcode>(opcode);

        switch (op) {
            // ================================================================
            // Control Flow
            // ================================================================
            case WasmOpcode::Unreachable:
                return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};

            case WasmOpcode::Nop:
                break;

            case WasmOpcode::Block: {
                std::uint8_t block_type = read_byte_local();
                std::size_t arity = (block_type == 0x40) ? 0 : 1;
                std::size_t end_pc = find_end(pc);
                push_label(LabelKind::Block, pc, arity, end_pc);
                break;
            }

            case WasmOpcode::Loop: {
                std::uint8_t block_type = read_byte_local();
                std::size_t arity = 0;  // Loop branches don't pass values
                std::size_t end_pc = find_end(pc);
                push_label(LabelKind::Loop, pc, arity, end_pc);
                break;
            }

            case WasmOpcode::If: {
                std::uint8_t block_type = read_byte_local();
                std::size_t arity = (block_type == 0x40) ? 0 : 1;
                StackValue cond = pop();
                std::size_t end_pc = find_end(pc);

                if (cond.i32) {
                    push_label(LabelKind::If, pc, arity, end_pc);
                } else {
                    // Find else or end
                    std::size_t depth = 1;
                    while (pc < code.size() && depth > 0) {
                        std::uint8_t byte = code[pc++];
                        WasmOpcode inner_op = static_cast<WasmOpcode>(byte);
                        if (inner_op == WasmOpcode::Block || inner_op == WasmOpcode::Loop ||
                            inner_op == WasmOpcode::If) {
                            ++depth;
                            pc++;  // Skip block type
                        } else if (inner_op == WasmOpcode::End) {
                            --depth;
                        } else if (inner_op == WasmOpcode::Else && depth == 1) {
                            push_label(LabelKind::Else, pc, arity, end_pc);
                            break;
                        }
                    }
                }
                break;
            }

            case WasmOpcode::Else: {
                // Jump to end
                Label& label = get_label(0);
                pc = label.end_pc - 1;
                break;
            }

            case WasmOpcode::End: {
                if (frame->labels.empty()) {
                    return void_core::Ok();  // Function end
                }
                pop_label();
                break;
            }

            case WasmOpcode::Br: {
                std::uint32_t depth = read_u32_leb_local();
                Label& target = get_label(depth);
                if (target.kind == LabelKind::Loop) {
                    pc = target.pc;
                } else {
                    pc = target.end_pc - 1;
                }
                branch(depth);
                break;
            }

            case WasmOpcode::BrIf: {
                std::uint32_t depth = read_u32_leb_local();
                StackValue cond = pop();
                if (cond.i32) {
                    Label& target = get_label(depth);
                    if (target.kind == LabelKind::Loop) {
                        pc = target.pc;
                    } else {
                        pc = target.end_pc - 1;
                    }
                    branch(depth);
                }
                break;
            }

            case WasmOpcode::BrTable: {
                std::uint32_t count = read_u32_leb_local();
                std::vector<std::uint32_t> targets(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    targets[i] = read_u32_leb_local();
                }
                std::uint32_t default_target = read_u32_leb_local();

                StackValue idx = pop();
                std::uint32_t target = (static_cast<std::uint32_t>(idx.i32) < count)
                    ? targets[idx.i32] : default_target;

                Label& label = get_label(target);
                if (label.kind == LabelKind::Loop) {
                    pc = label.pc;
                } else {
                    pc = label.end_pc - 1;
                }
                branch(target);
                break;
            }

            case WasmOpcode::Return:
                return void_core::Ok();

            case WasmOpcode::Call: {
                std::uint32_t func_idx = read_u32_leb_local();

                // Check if imported function
                if (func_idx < module.num_imported_functions) {
                    std::uint32_t imp_idx = 0;
                    for (const auto& imp : module.imports) {
                        if (imp.kind == WasmExternKind::Func) {
                            if (imp_idx == func_idx) {
                                // Pop arguments
                                std::vector<WasmValue> args;
                                for (std::size_t i = 0; i < imp.func_type->params.size(); ++i) {
                                    StackValue v = pop();
                                    WasmValue wv;
                                    wv.type = imp.func_type->params[imp.func_type->params.size() - 1 - i];
                                    switch (wv.type) {
                                        case WasmValType::I32: wv.i32 = v.i32; break;
                                        case WasmValType::I64: wv.i64 = v.i64; break;
                                        case WasmValType::F32: wv.f32 = v.f32; break;
                                        case WasmValType::F64: wv.f64 = v.f64; break;
                                        default: break;
                                    }
                                    args.push_back(wv);
                                }
                                std::reverse(args.begin(), args.end());

                                auto result = call_host_function(imp.module, imp.name, args);
                                if (!result) {
                                    return result.error();
                                }

                                // Push results
                                for (const auto& rv : result.value()) {
                                    StackValue sv;
                                    switch (rv.type) {
                                        case WasmValType::I32: sv.i32 = rv.i32; break;
                                        case WasmValType::I64: sv.i64 = rv.i64; break;
                                        case WasmValType::F32: sv.f32 = rv.f32; break;
                                        case WasmValType::F64: sv.f64 = rv.f64; break;
                                        default: break;
                                    }
                                    push(sv);
                                }
                                break;
                            }
                            ++imp_idx;
                        }
                    }
                } else {
                    // Local function call
                    std::uint32_t local_idx = func_idx - module.num_imported_functions;
                    const WasmFunction& callee = module.functions[local_idx];
                    const WasmFunctionType& callee_type = module.types[callee.type_index];

                    // Pop arguments
                    std::vector<WasmValue> args;
                    for (std::size_t i = 0; i < callee_type.params.size(); ++i) {
                        StackValue v = pop();
                        WasmValue wv;
                        wv.type = callee_type.params[callee_type.params.size() - 1 - i];
                        switch (wv.type) {
                            case WasmValType::I32: wv.i32 = v.i32; break;
                            case WasmValType::I64: wv.i64 = v.i64; break;
                            case WasmValType::F32: wv.f32 = v.f32; break;
                            case WasmValType::F64: wv.f64 = v.f64; break;
                            default: break;
                        }
                        args.push_back(wv);
                    }
                    std::reverse(args.begin(), args.end());

                    // Recursive call
                    auto result = execute(module, memory, func_idx, args);
                    if (!result) {
                        return result.error();
                    }

                    // Push results
                    for (const auto& rv : result.value()) {
                        StackValue sv;
                        switch (rv.type) {
                            case WasmValType::I32: sv.i32 = rv.i32; break;
                            case WasmValType::I64: sv.i64 = rv.i64; break;
                            case WasmValType::F32: sv.f32 = rv.f32; break;
                            case WasmValType::F64: sv.f64 = rv.f64; break;
                            default: break;
                        }
                        push(sv);
                    }
                }
                break;
            }

            case WasmOpcode::CallIndirect: {
                std::uint32_t type_idx = read_u32_leb_local();
                std::uint32_t table_idx = read_u32_leb_local();

                StackValue idx = pop();
                if (table_idx >= tables_.size() ||
                    static_cast<std::uint32_t>(idx.u32) >= tables_[table_idx].size()) {
                    return void_core::Error{void_core::ErrorCode::InvalidArgument, "WASM out of bounds"};
                }

                std::uint32_t func_idx = tables_[table_idx][idx.u32];
                if (func_idx == 0xFFFFFFFF) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }

                // Validate function type matches
                if (type_idx >= module.types.size()) {
                    return void_core::Error{void_core::ErrorCode::InvalidArgument, "WASM invalid type index"};
                }
                const WasmFunctionType& expected_type = module.types[type_idx];

                // Check if imported function
                if (func_idx < module.num_imported_functions) {
                    std::uint32_t imp_idx = 0;
                    for (const auto& imp : module.imports) {
                        if (imp.kind == WasmExternKind::Func) {
                            if (imp_idx == func_idx) {
                                // Verify type
                                if (imp.func_type &&
                                    (imp.func_type->params != expected_type.params ||
                                     imp.func_type->results != expected_type.results)) {
                                    return void_core::Error{void_core::ErrorCode::InvalidArgument, "WASM type mismatch"};
                                }

                                // Pop arguments
                                std::vector<WasmValue> args;
                                for (std::size_t i = 0; i < expected_type.params.size(); ++i) {
                                    StackValue v = pop();
                                    WasmValue wv;
                                    wv.type = expected_type.params[expected_type.params.size() - 1 - i];
                                    switch (wv.type) {
                                        case WasmValType::I32: wv.i32 = v.i32; break;
                                        case WasmValType::I64: wv.i64 = v.i64; break;
                                        case WasmValType::F32: wv.f32 = v.f32; break;
                                        case WasmValType::F64: wv.f64 = v.f64; break;
                                        default: break;
                                    }
                                    args.push_back(wv);
                                }
                                std::reverse(args.begin(), args.end());

                                auto result = call_host_function(imp.module, imp.name, args);
                                if (!result) {
                                    return result.error();
                                }

                                // Push results
                                for (const auto& rv : result.value()) {
                                    StackValue sv;
                                    switch (rv.type) {
                                        case WasmValType::I32: sv.i32 = rv.i32; break;
                                        case WasmValType::I64: sv.i64 = rv.i64; break;
                                        case WasmValType::F32: sv.f32 = rv.f32; break;
                                        case WasmValType::F64: sv.f64 = rv.f64; break;
                                        default: break;
                                    }
                                    push(sv);
                                }
                                break;
                            }
                            ++imp_idx;
                        }
                    }
                } else {
                    // Local function call
                    std::uint32_t local_idx = func_idx - module.num_imported_functions;
                    if (local_idx >= module.functions.size()) {
                        return void_core::Error{void_core::ErrorCode::InvalidArgument, "WASM invalid function"};
                    }

                    const WasmFunction& callee = module.functions[local_idx];
                    const WasmFunctionType& callee_type = module.types[callee.type_index];

                    // Verify type
                    if (callee_type.params != expected_type.params ||
                        callee_type.results != expected_type.results) {
                        return void_core::Error{void_core::ErrorCode::InvalidArgument, "WASM type mismatch"};
                    }

                    // Pop arguments
                    std::vector<WasmValue> args;
                    for (std::size_t i = 0; i < callee_type.params.size(); ++i) {
                        StackValue v = pop();
                        WasmValue wv;
                        wv.type = callee_type.params[callee_type.params.size() - 1 - i];
                        switch (wv.type) {
                            case WasmValType::I32: wv.i32 = v.i32; break;
                            case WasmValType::I64: wv.i64 = v.i64; break;
                            case WasmValType::F32: wv.f32 = v.f32; break;
                            case WasmValType::F64: wv.f64 = v.f64; break;
                            default: break;
                        }
                        args.push_back(wv);
                    }
                    std::reverse(args.begin(), args.end());

                    // Recursive call
                    auto result = execute(module, memory, func_idx, args);
                    if (!result) {
                        return result.error();
                    }

                    // Push results
                    for (const auto& rv : result.value()) {
                        StackValue sv;
                        switch (rv.type) {
                            case WasmValType::I32: sv.i32 = rv.i32; break;
                            case WasmValType::I64: sv.i64 = rv.i64; break;
                            case WasmValType::F32: sv.f32 = rv.f32; break;
                            case WasmValType::F64: sv.f64 = rv.f64; break;
                            default: break;
                        }
                        push(sv);
                    }
                }
                break;
            }

            // ================================================================
            // Parametric
            // ================================================================
            case WasmOpcode::Drop:
                pop();
                break;

            case WasmOpcode::Select: {
                StackValue cond = pop();
                StackValue val2 = pop();
                StackValue val1 = pop();
                push(cond.i32 ? val1 : val2);
                break;
            }

            // ================================================================
            // Variables
            // ================================================================
            case WasmOpcode::LocalGet: {
                std::uint32_t idx = read_u32_leb_local();
                push(frame->locals[idx]);
                break;
            }

            case WasmOpcode::LocalSet: {
                std::uint32_t idx = read_u32_leb_local();
                frame->locals[idx] = pop();
                break;
            }

            case WasmOpcode::LocalTee: {
                std::uint32_t idx = read_u32_leb_local();
                frame->locals[idx] = top();
                break;
            }

            case WasmOpcode::GlobalGet: {
                std::uint32_t idx = read_u32_leb_local();
                push(globals_[idx]);
                break;
            }

            case WasmOpcode::GlobalSet: {
                std::uint32_t idx = read_u32_leb_local();
                globals_[idx] = pop();
                break;
            }

            // ================================================================
            // Memory
            // ================================================================
            case WasmOpcode::I32Load: {
                read_u32_leb_local();  // align
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{memory.read<std::int32_t>(ea)});
                break;
            }

            case WasmOpcode::I64Load: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{memory.read<std::int64_t>(ea)});
                break;
            }

            case WasmOpcode::F32Load: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{memory.read<float>(ea)});
                break;
            }

            case WasmOpcode::F64Load: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{memory.read<double>(ea)});
                break;
            }

            case WasmOpcode::I32Load8S: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int32_t>(memory.read<std::int8_t>(ea))});
                break;
            }

            case WasmOpcode::I32Load8U: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int32_t>(memory.read<std::uint8_t>(ea))});
                break;
            }

            case WasmOpcode::I32Load16S: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int32_t>(memory.read<std::int16_t>(ea))});
                break;
            }

            case WasmOpcode::I32Load16U: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int32_t>(memory.read<std::uint16_t>(ea))});
                break;
            }

            case WasmOpcode::I64Load8S: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int64_t>(memory.read<std::int8_t>(ea))});
                break;
            }

            case WasmOpcode::I64Load8U: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int64_t>(memory.read<std::uint8_t>(ea))});
                break;
            }

            case WasmOpcode::I64Load16S: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int64_t>(memory.read<std::int16_t>(ea))});
                break;
            }

            case WasmOpcode::I64Load16U: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int64_t>(memory.read<std::uint16_t>(ea))});
                break;
            }

            case WasmOpcode::I64Load32S: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int64_t>(memory.read<std::int32_t>(ea))});
                break;
            }

            case WasmOpcode::I64Load32U: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                push(StackValue{static_cast<std::int64_t>(memory.read<std::uint32_t>(ea))});
                break;
            }

            case WasmOpcode::I32Store: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue val = pop();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                memory.write<std::int32_t>(ea, val.i32);
                break;
            }

            case WasmOpcode::I64Store: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue val = pop();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                memory.write<std::int64_t>(ea, val.i64);
                break;
            }

            case WasmOpcode::F32Store: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue val = pop();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                memory.write<float>(ea, val.f32);
                break;
            }

            case WasmOpcode::F64Store: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue val = pop();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                memory.write<double>(ea, val.f64);
                break;
            }

            case WasmOpcode::I32Store8: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue val = pop();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                memory.write<std::uint8_t>(ea, static_cast<std::uint8_t>(val.i32));
                break;
            }

            case WasmOpcode::I32Store16: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue val = pop();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                memory.write<std::uint16_t>(ea, static_cast<std::uint16_t>(val.i32));
                break;
            }

            case WasmOpcode::I64Store8: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue val = pop();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                memory.write<std::uint8_t>(ea, static_cast<std::uint8_t>(val.i64));
                break;
            }

            case WasmOpcode::I64Store16: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue val = pop();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                memory.write<std::uint16_t>(ea, static_cast<std::uint16_t>(val.i64));
                break;
            }

            case WasmOpcode::I64Store32: {
                read_u32_leb_local();
                std::uint32_t offset = read_u32_leb_local();
                StackValue val = pop();
                StackValue addr = pop();
                std::uint32_t ea = addr.u32 + offset;
                memory.write<std::uint32_t>(ea, static_cast<std::uint32_t>(val.i64));
                break;
            }

            case WasmOpcode::MemorySize:
                read_byte_local();  // memory index (always 0)
                push(StackValue{static_cast<std::int32_t>(memory.pages())});
                break;

            case WasmOpcode::MemoryGrow: {
                read_byte_local();
                StackValue delta = pop();
                auto result = memory.grow(delta.u32);
                if (result) {
                    push(StackValue{static_cast<std::int32_t>(result.value())});
                } else {
                    push(StackValue{static_cast<std::int32_t>(-1)});
                }
                break;
            }

            // ================================================================
            // Constants
            // ================================================================
            case WasmOpcode::I32Const:
                push(StackValue{read_i32_leb_local()});
                break;

            case WasmOpcode::I64Const:
                push(StackValue{read_i64_leb_local()});
                break;

            case WasmOpcode::F32Const:
                push(StackValue{read_f32_local()});
                break;

            case WasmOpcode::F64Const:
                push(StackValue{read_f64_local()});
                break;

            // ================================================================
            // i32 Comparison
            // ================================================================
            case WasmOpcode::I32Eqz: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i32 == 0)});
                break;
            }

            case WasmOpcode::I32Eq: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i32 == b.i32)});
                break;
            }

            case WasmOpcode::I32Ne: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i32 != b.i32)});
                break;
            }

            case WasmOpcode::I32LtS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i32 < b.i32)});
                break;
            }

            case WasmOpcode::I32LtU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.u32 < b.u32)});
                break;
            }

            case WasmOpcode::I32GtS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i32 > b.i32)});
                break;
            }

            case WasmOpcode::I32GtU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.u32 > b.u32)});
                break;
            }

            case WasmOpcode::I32LeS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i32 <= b.i32)});
                break;
            }

            case WasmOpcode::I32LeU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.u32 <= b.u32)});
                break;
            }

            case WasmOpcode::I32GeS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i32 >= b.i32)});
                break;
            }

            case WasmOpcode::I32GeU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.u32 >= b.u32)});
                break;
            }

            // ================================================================
            // i64 Comparison
            // ================================================================
            case WasmOpcode::I64Eqz: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i64 == 0)});
                break;
            }

            case WasmOpcode::I64Eq: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i64 == b.i64)});
                break;
            }

            case WasmOpcode::I64Ne: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i64 != b.i64)});
                break;
            }

            case WasmOpcode::I64LtS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i64 < b.i64)});
                break;
            }

            case WasmOpcode::I64LtU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.u64 < b.u64)});
                break;
            }

            case WasmOpcode::I64GtS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i64 > b.i64)});
                break;
            }

            case WasmOpcode::I64GtU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.u64 > b.u64)});
                break;
            }

            case WasmOpcode::I64LeS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i64 <= b.i64)});
                break;
            }

            case WasmOpcode::I64LeU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.u64 <= b.u64)});
                break;
            }

            case WasmOpcode::I64GeS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i64 >= b.i64)});
                break;
            }

            case WasmOpcode::I64GeU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.u64 >= b.u64)});
                break;
            }

            // ================================================================
            // f32 Comparison
            // ================================================================
            case WasmOpcode::F32Eq: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f32 == b.f32)});
                break;
            }

            case WasmOpcode::F32Ne: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f32 != b.f32)});
                break;
            }

            case WasmOpcode::F32Lt: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f32 < b.f32)});
                break;
            }

            case WasmOpcode::F32Gt: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f32 > b.f32)});
                break;
            }

            case WasmOpcode::F32Le: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f32 <= b.f32)});
                break;
            }

            case WasmOpcode::F32Ge: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f32 >= b.f32)});
                break;
            }

            // ================================================================
            // f64 Comparison
            // ================================================================
            case WasmOpcode::F64Eq: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f64 == b.f64)});
                break;
            }

            case WasmOpcode::F64Ne: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f64 != b.f64)});
                break;
            }

            case WasmOpcode::F64Lt: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f64 < b.f64)});
                break;
            }

            case WasmOpcode::F64Gt: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f64 > b.f64)});
                break;
            }

            case WasmOpcode::F64Le: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f64 <= b.f64)});
                break;
            }

            case WasmOpcode::F64Ge: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.f64 >= b.f64)});
                break;
            }

            // ================================================================
            // i32 Arithmetic
            // ================================================================
            case WasmOpcode::I32Clz: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(clz32(a.u32))});
                break;
            }

            case WasmOpcode::I32Ctz: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(ctz32(a.u32))});
                break;
            }

            case WasmOpcode::I32Popcnt: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(popcnt32(a.u32))});
                break;
            }

            case WasmOpcode::I32Add: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i32 + b.i32});
                break;
            }

            case WasmOpcode::I32Sub: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i32 - b.i32});
                break;
            }

            case WasmOpcode::I32Mul: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i32 * b.i32});
                break;
            }

            case WasmOpcode::I32DivS: {
                StackValue b = pop();
                StackValue a = pop();
                if (b.i32 == 0) return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                if (a.i32 == std::numeric_limits<std::int32_t>::min() && b.i32 == -1) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{a.i32 / b.i32});
                break;
            }

            case WasmOpcode::I32DivU: {
                StackValue b = pop();
                StackValue a = pop();
                if (b.u32 == 0) return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                push(StackValue{static_cast<std::int32_t>(a.u32 / b.u32)});
                break;
            }

            case WasmOpcode::I32RemS: {
                StackValue b = pop();
                StackValue a = pop();
                if (b.i32 == 0) return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                push(StackValue{a.i32 % b.i32});
                break;
            }

            case WasmOpcode::I32RemU: {
                StackValue b = pop();
                StackValue a = pop();
                if (b.u32 == 0) return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                push(StackValue{static_cast<std::int32_t>(a.u32 % b.u32)});
                break;
            }

            case WasmOpcode::I32And: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i32 & b.i32});
                break;
            }

            case WasmOpcode::I32Or: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i32 | b.i32});
                break;
            }

            case WasmOpcode::I32Xor: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i32 ^ b.i32});
                break;
            }

            case WasmOpcode::I32Shl: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i32 << (b.i32 & 31)});
                break;
            }

            case WasmOpcode::I32ShrS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i32 >> (b.i32 & 31)});
                break;
            }

            case WasmOpcode::I32ShrU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.u32 >> (b.u32 & 31))});
                break;
            }

            case WasmOpcode::I32Rotl: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(rotl32(a.u32, b.u32))});
                break;
            }

            case WasmOpcode::I32Rotr: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(rotr32(a.u32, b.u32))});
                break;
            }

            // ================================================================
            // i64 Arithmetic
            // ================================================================
            case WasmOpcode::I64Clz: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(clz64(a.u64))});
                break;
            }

            case WasmOpcode::I64Ctz: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(ctz64(a.u64))});
                break;
            }

            case WasmOpcode::I64Popcnt: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(popcnt64(a.u64))});
                break;
            }

            case WasmOpcode::I64Add: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i64 + b.i64});
                break;
            }

            case WasmOpcode::I64Sub: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i64 - b.i64});
                break;
            }

            case WasmOpcode::I64Mul: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i64 * b.i64});
                break;
            }

            case WasmOpcode::I64DivS: {
                StackValue b = pop();
                StackValue a = pop();
                if (b.i64 == 0) return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                if (a.i64 == std::numeric_limits<std::int64_t>::min() && b.i64 == -1) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{a.i64 / b.i64});
                break;
            }

            case WasmOpcode::I64DivU: {
                StackValue b = pop();
                StackValue a = pop();
                if (b.u64 == 0) return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                push(StackValue{static_cast<std::int64_t>(a.u64 / b.u64)});
                break;
            }

            case WasmOpcode::I64RemS: {
                StackValue b = pop();
                StackValue a = pop();
                if (b.i64 == 0) return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                push(StackValue{a.i64 % b.i64});
                break;
            }

            case WasmOpcode::I64RemU: {
                StackValue b = pop();
                StackValue a = pop();
                if (b.u64 == 0) return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                push(StackValue{static_cast<std::int64_t>(a.u64 % b.u64)});
                break;
            }

            case WasmOpcode::I64And: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i64 & b.i64});
                break;
            }

            case WasmOpcode::I64Or: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i64 | b.i64});
                break;
            }

            case WasmOpcode::I64Xor: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i64 ^ b.i64});
                break;
            }

            case WasmOpcode::I64Shl: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i64 << (b.i64 & 63)});
                break;
            }

            case WasmOpcode::I64ShrS: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.i64 >> (b.i64 & 63)});
                break;
            }

            case WasmOpcode::I64ShrU: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(a.u64 >> (b.u64 & 63))});
                break;
            }

            case WasmOpcode::I64Rotl: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(rotl64(a.u64, b.u64))});
                break;
            }

            case WasmOpcode::I64Rotr: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(rotr64(a.u64, b.u64))});
                break;
            }

            // ================================================================
            // f32 Arithmetic
            // ================================================================
            case WasmOpcode::F32Abs: {
                StackValue a = pop();
                push(StackValue{std::abs(a.f32)});
                break;
            }

            case WasmOpcode::F32Neg: {
                StackValue a = pop();
                push(StackValue{-a.f32});
                break;
            }

            case WasmOpcode::F32Ceil: {
                StackValue a = pop();
                push(StackValue{std::ceil(a.f32)});
                break;
            }

            case WasmOpcode::F32Floor: {
                StackValue a = pop();
                push(StackValue{std::floor(a.f32)});
                break;
            }

            case WasmOpcode::F32Trunc: {
                StackValue a = pop();
                push(StackValue{std::trunc(a.f32)});
                break;
            }

            case WasmOpcode::F32Nearest: {
                StackValue a = pop();
                push(StackValue{std::nearbyint(a.f32)});
                break;
            }

            case WasmOpcode::F32Sqrt: {
                StackValue a = pop();
                push(StackValue{std::sqrt(a.f32)});
                break;
            }

            case WasmOpcode::F32Add: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.f32 + b.f32});
                break;
            }

            case WasmOpcode::F32Sub: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.f32 - b.f32});
                break;
            }

            case WasmOpcode::F32Mul: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.f32 * b.f32});
                break;
            }

            case WasmOpcode::F32Div: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.f32 / b.f32});
                break;
            }

            case WasmOpcode::F32Min: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{std::fmin(a.f32, b.f32)});
                break;
            }

            case WasmOpcode::F32Max: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{std::fmax(a.f32, b.f32)});
                break;
            }

            case WasmOpcode::F32Copysign: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{std::copysign(a.f32, b.f32)});
                break;
            }

            // ================================================================
            // f64 Arithmetic
            // ================================================================
            case WasmOpcode::F64Abs: {
                StackValue a = pop();
                push(StackValue{std::abs(a.f64)});
                break;
            }

            case WasmOpcode::F64Neg: {
                StackValue a = pop();
                push(StackValue{-a.f64});
                break;
            }

            case WasmOpcode::F64Ceil: {
                StackValue a = pop();
                push(StackValue{std::ceil(a.f64)});
                break;
            }

            case WasmOpcode::F64Floor: {
                StackValue a = pop();
                push(StackValue{std::floor(a.f64)});
                break;
            }

            case WasmOpcode::F64Trunc: {
                StackValue a = pop();
                push(StackValue{std::trunc(a.f64)});
                break;
            }

            case WasmOpcode::F64Nearest: {
                StackValue a = pop();
                push(StackValue{std::nearbyint(a.f64)});
                break;
            }

            case WasmOpcode::F64Sqrt: {
                StackValue a = pop();
                push(StackValue{std::sqrt(a.f64)});
                break;
            }

            case WasmOpcode::F64Add: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.f64 + b.f64});
                break;
            }

            case WasmOpcode::F64Sub: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.f64 - b.f64});
                break;
            }

            case WasmOpcode::F64Mul: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.f64 * b.f64});
                break;
            }

            case WasmOpcode::F64Div: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{a.f64 / b.f64});
                break;
            }

            case WasmOpcode::F64Min: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{std::fmin(a.f64, b.f64)});
                break;
            }

            case WasmOpcode::F64Max: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{std::fmax(a.f64, b.f64)});
                break;
            }

            case WasmOpcode::F64Copysign: {
                StackValue b = pop();
                StackValue a = pop();
                push(StackValue{std::copysign(a.f64, b.f64)});
                break;
            }

            // ================================================================
            // Conversions
            // ================================================================
            case WasmOpcode::I32WrapI64: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(a.i64)});
                break;
            }

            case WasmOpcode::I32TruncF32S: {
                StackValue a = pop();
                if (std::isnan(a.f32) || a.f32 < static_cast<float>(std::numeric_limits<std::int32_t>::min()) ||
                    a.f32 >= static_cast<float>(std::numeric_limits<std::int32_t>::max()) + 1.0f) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{static_cast<std::int32_t>(a.f32)});
                break;
            }

            case WasmOpcode::I32TruncF32U: {
                StackValue a = pop();
                if (std::isnan(a.f32) || a.f32 < 0.0f ||
                    a.f32 >= static_cast<float>(std::numeric_limits<std::uint32_t>::max()) + 1.0f) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{static_cast<std::uint32_t>(a.f32)});
                break;
            }

            case WasmOpcode::I32TruncF64S: {
                StackValue a = pop();
                if (std::isnan(a.f64) || a.f64 < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
                    a.f64 >= static_cast<double>(std::numeric_limits<std::int32_t>::max()) + 1.0) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{static_cast<std::int32_t>(a.f64)});
                break;
            }

            case WasmOpcode::I32TruncF64U: {
                StackValue a = pop();
                if (std::isnan(a.f64) || a.f64 < 0.0 ||
                    a.f64 >= static_cast<double>(std::numeric_limits<std::uint32_t>::max()) + 1.0) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{static_cast<std::uint32_t>(a.f64)});
                break;
            }

            case WasmOpcode::I64ExtendI32S: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(a.i32)});
                break;
            }

            case WasmOpcode::I64ExtendI32U: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(a.u32)});
                break;
            }

            case WasmOpcode::I64TruncF32S: {
                StackValue a = pop();
                if (std::isnan(a.f32)) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{static_cast<std::int64_t>(a.f32)});
                break;
            }

            case WasmOpcode::I64TruncF32U: {
                StackValue a = pop();
                if (std::isnan(a.f32) || a.f32 < 0.0f) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{static_cast<std::uint64_t>(a.f32)});
                break;
            }

            case WasmOpcode::I64TruncF64S: {
                StackValue a = pop();
                if (std::isnan(a.f64)) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{static_cast<std::int64_t>(a.f64)});
                break;
            }

            case WasmOpcode::I64TruncF64U: {
                StackValue a = pop();
                if (std::isnan(a.f64) || a.f64 < 0.0) {
                    return void_core::Error{void_core::ErrorCode::InvalidState, "WASM trap"};
                }
                push(StackValue{static_cast<std::uint64_t>(a.f64)});
                break;
            }

            case WasmOpcode::F32ConvertI32S: {
                StackValue a = pop();
                push(StackValue{static_cast<float>(a.i32)});
                break;
            }

            case WasmOpcode::F32ConvertI32U: {
                StackValue a = pop();
                push(StackValue{static_cast<float>(a.u32)});
                break;
            }

            case WasmOpcode::F32ConvertI64S: {
                StackValue a = pop();
                push(StackValue{static_cast<float>(a.i64)});
                break;
            }

            case WasmOpcode::F32ConvertI64U: {
                StackValue a = pop();
                push(StackValue{static_cast<float>(a.u64)});
                break;
            }

            case WasmOpcode::F32DemoteF64: {
                StackValue a = pop();
                push(StackValue{static_cast<float>(a.f64)});
                break;
            }

            case WasmOpcode::F64ConvertI32S: {
                StackValue a = pop();
                push(StackValue{static_cast<double>(a.i32)});
                break;
            }

            case WasmOpcode::F64ConvertI32U: {
                StackValue a = pop();
                push(StackValue{static_cast<double>(a.u32)});
                break;
            }

            case WasmOpcode::F64ConvertI64S: {
                StackValue a = pop();
                push(StackValue{static_cast<double>(a.i64)});
                break;
            }

            case WasmOpcode::F64ConvertI64U: {
                StackValue a = pop();
                push(StackValue{static_cast<double>(a.u64)});
                break;
            }

            case WasmOpcode::F64PromoteF32: {
                StackValue a = pop();
                push(StackValue{static_cast<double>(a.f32)});
                break;
            }

            case WasmOpcode::I32ReinterpretF32: {
                StackValue a = pop();
                std::int32_t v;
                std::memcpy(&v, &a.f32, 4);
                push(StackValue{v});
                break;
            }

            case WasmOpcode::I64ReinterpretF64: {
                StackValue a = pop();
                std::int64_t v;
                std::memcpy(&v, &a.f64, 8);
                push(StackValue{v});
                break;
            }

            case WasmOpcode::F32ReinterpretI32: {
                StackValue a = pop();
                float v;
                std::memcpy(&v, &a.i32, 4);
                push(StackValue{v});
                break;
            }

            case WasmOpcode::F64ReinterpretI64: {
                StackValue a = pop();
                double v;
                std::memcpy(&v, &a.i64, 8);
                push(StackValue{v});
                break;
            }

            // ================================================================
            // Sign Extension
            // ================================================================
            case WasmOpcode::I32Extend8S: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(static_cast<std::int8_t>(a.i32))});
                break;
            }

            case WasmOpcode::I32Extend16S: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int32_t>(static_cast<std::int16_t>(a.i32))});
                break;
            }

            case WasmOpcode::I64Extend8S: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(static_cast<std::int8_t>(a.i64))});
                break;
            }

            case WasmOpcode::I64Extend16S: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(static_cast<std::int16_t>(a.i64))});
                break;
            }

            case WasmOpcode::I64Extend32S: {
                StackValue a = pop();
                push(StackValue{static_cast<std::int64_t>(static_cast<std::int32_t>(a.i64))});
                break;
            }

            // ================================================================
            // Multi-byte opcodes
            // ================================================================
            case WasmOpcode::PrefixFC: {
                std::uint32_t sub_op = read_u32_leb_local();
                switch (sub_op) {
                    case 0: // i32.trunc_sat_f32_s
                    case 1: // i32.trunc_sat_f32_u
                    case 2: // i32.trunc_sat_f64_s
                    case 3: // i32.trunc_sat_f64_u
                    case 4: // i64.trunc_sat_f32_s
                    case 5: // i64.trunc_sat_f32_u
                    case 6: // i64.trunc_sat_f64_s
                    case 7: // i64.trunc_sat_f64_u
                        // Saturating truncation - similar to regular trunc but clamped
                        // Simplified implementation
                        break;
                    case 8: // memory.init
                    case 9: // data.drop
                    case 10: // memory.copy
                    case 11: // memory.fill
                    case 12: // table.init
                    case 13: // elem.drop
                    case 14: // table.copy
                    case 15: // table.grow
                    case 16: // table.size
                    case 17: // table.fill
                        // Bulk memory operations - skip for now
                        break;
                    default:
                        break;
                }
                break;
            }

            default:
                // Unknown opcode
                VOID_LOG_WARN("[WasmInterpreter] Unknown opcode: 0x{:02X}", opcode);
                break;
        }
    }

    return void_core::Ok();
}

} // namespace void_scripting
