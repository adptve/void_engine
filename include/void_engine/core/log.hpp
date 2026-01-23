#pragma once

/// @file log.hpp
/// @brief Logging utilities for void_engine

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

// =============================================================================
// Logging Macros
// =============================================================================

#define VOID_LOG_TRACE(...) spdlog::trace(__VA_ARGS__)
#define VOID_LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define VOID_LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define VOID_LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define VOID_LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define VOID_LOG_CRITICAL(...) spdlog::critical(__VA_ARGS__)

namespace void_core {

/// @brief Initialize the logging system
inline void init_logging() {
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug);
}

/// @brief Set log level
inline void set_log_level(spdlog::level::level_enum level) {
    spdlog::set_level(level);
}

} // namespace void_core
