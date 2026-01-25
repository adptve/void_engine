#pragma once

/// @file log.hpp
/// @brief Logging utilities for void_engine

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <string>
#include <map>
#include <memory>
#include <optional>
#include <chrono>

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

// =============================================================================
// Basic Logging (Inline)
// =============================================================================

/// @brief Initialize the logging system (basic)
inline void init_logging() {
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug);
}

/// @brief Set log level (basic)
inline void set_log_level(spdlog::level::level_enum level) {
    spdlog::set_level(level);
}

// =============================================================================
// Log Configuration
// =============================================================================

/// Configuration for the logging system
struct LogConfig {
    bool console_enabled = true;
    bool file_enabled = false;
    std::string log_directory;
    std::size_t max_file_size = 10 * 1024 * 1024;  // 10 MB
    std::size_t max_files = 5;
    spdlog::level::level_enum level = spdlog::level::debug;
};

/// Configure logging system with full options
void configure_logging(const LogConfig& config);

// =============================================================================
// Named Loggers
// =============================================================================

/// Get or create a named logger
std::shared_ptr<spdlog::logger> get_logger(const std::string& name);

/// Get the core module logger
std::shared_ptr<spdlog::logger> core_logger();

/// Get the engine logger
std::shared_ptr<spdlog::logger> engine_logger();

/// Get the plugin logger
std::shared_ptr<spdlog::logger> plugin_logger();

/// Get the hot-reload logger
std::shared_ptr<spdlog::logger> hot_reload_logger();

// =============================================================================
// Log Level Management
// =============================================================================

/// Set global log level
void set_global_log_level(spdlog::level::level_enum level);

/// Set log level for specific logger
void set_logger_level(const std::string& name, spdlog::level::level_enum level);

/// Get current global log level
spdlog::level::level_enum get_global_log_level();

/// Parse log level from string
std::optional<spdlog::level::level_enum> parse_log_level(const std::string& str);

/// Get log level name
const char* log_level_name(spdlog::level::level_enum level);

// =============================================================================
// Structured Logging
// =============================================================================

/// Log entry with structured data
void log_structured(
    spdlog::level::level_enum level,
    const std::string& logger_name,
    const std::string& message,
    const std::map<std::string, std::string>& fields);

// =============================================================================
// Log Scoping (RAII)
// =============================================================================

/// RAII log scope for function/block tracing
class LogScope {
public:
    LogScope(const std::string& name, const std::string& logger_name = "void_core");
    ~LogScope();

    // Non-copyable, non-movable
    LogScope(const LogScope&) = delete;
    LogScope& operator=(const LogScope&) = delete;
    LogScope(LogScope&&) = delete;
    LogScope& operator=(LogScope&&) = delete;

private:
    std::string m_name;
    std::shared_ptr<spdlog::logger> m_logger;
    std::chrono::steady_clock::time_point m_start;
};

/// Macro for easy scope logging
#define VOID_LOG_SCOPE(name) ::void_core::LogScope _log_scope_##__LINE__(name)
#define VOID_LOG_FUNC() ::void_core::LogScope _log_scope_func(__FUNCTION__)

// =============================================================================
// Lifecycle
// =============================================================================

/// Flush all loggers
void flush_all_loggers();

/// Shutdown logging system
void shutdown_logging();

// =============================================================================
// Hot-Reload Support
// =============================================================================

/// Prepare logging for hot-reload (flush and release file handles)
void prepare_logging_for_reload();

/// Complete logging after hot-reload
void complete_logging_after_reload();

} // namespace void_core
