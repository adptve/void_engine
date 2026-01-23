/// @file types.cpp
/// @brief Core types implementation for void_scripting module

#include "types.hpp"

#include <sstream>

namespace void_scripting {

// =============================================================================
// WasmValue Implementation
// =============================================================================

std::string WasmValue::to_string() const {
    std::ostringstream oss;

    switch (type) {
        case WasmValType::I32:
            oss << "i32:" << i32;
            break;
        case WasmValType::I64:
            oss << "i64:" << i64;
            break;
        case WasmValType::F32:
            oss << "f32:" << f32;
            break;
        case WasmValType::F64:
            oss << "f64:" << f64;
            break;
        case WasmValType::V128:
            oss << "v128:[";
            for (std::size_t i = 0; i < 16; ++i) {
                if (i > 0) oss << ",";
                oss << static_cast<int>(v128[i]);
            }
            oss << "]";
            break;
        case WasmValType::FuncRef:
            oss << "funcref:" << ref;
            break;
        case WasmValType::ExternRef:
            oss << "externref:" << ref;
            break;
        default:
            oss << "unknown";
            break;
    }

    return oss.str();
}

// =============================================================================
// WasmFunctionType Implementation
// =============================================================================

std::string WasmFunctionType::to_string() const {
    std::ostringstream oss;
    oss << "(";

    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i > 0) oss << ", ";
        switch (params[i]) {
            case WasmValType::I32: oss << "i32"; break;
            case WasmValType::I64: oss << "i64"; break;
            case WasmValType::F32: oss << "f32"; break;
            case WasmValType::F64: oss << "f64"; break;
            case WasmValType::V128: oss << "v128"; break;
            case WasmValType::FuncRef: oss << "funcref"; break;
            case WasmValType::ExternRef: oss << "externref"; break;
            default: oss << "?"; break;
        }
    }

    oss << ") -> (";

    for (std::size_t i = 0; i < results.size(); ++i) {
        if (i > 0) oss << ", ";
        switch (results[i]) {
            case WasmValType::I32: oss << "i32"; break;
            case WasmValType::I64: oss << "i64"; break;
            case WasmValType::F32: oss << "f32"; break;
            case WasmValType::F64: oss << "f64"; break;
            case WasmValType::V128: oss << "v128"; break;
            case WasmValType::FuncRef: oss << "funcref"; break;
            case WasmValType::ExternRef: oss << "externref"; break;
            default: oss << "?"; break;
        }
    }

    oss << ")";
    return oss.str();
}

// =============================================================================
// WasmException Implementation
// =============================================================================

WasmException::WasmException(WasmError error, std::string message)
    : error_(error)
    , message_(std::move(message)) {
    // Prepend error name to message
    std::string prefix = wasm_error_name(error);
    message_ = prefix + ": " + message_;
}

} // namespace void_scripting
