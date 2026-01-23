/// @file types.cpp
/// @brief Core types implementation for void_cpp module

#include "types.hpp"

namespace void_cpp {

// =============================================================================
// CppException Implementation
// =============================================================================

CppException::CppException(CppError error, std::string message)
    : error_(error)
    , message_(std::move(message)) {
    // Prepend error name
    std::string prefix = cpp_error_name(error);
    message_ = prefix + ": " + message_;
}

} // namespace void_cpp
