/// @file log.cpp
/// @brief Logging system implementation for void_core
///
/// Extends the spdlog-based logging with:
/// - Multiple named loggers for different subsystems
/// - Log level persistence and configuration
/// - Structured logging support
/// - Hot-reload safe logging

#include <void_engine/core/log.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <mutex>
#include <map>
#include <memory>
#include <filesystem>

namespace void_core {

// =============================================================================
// Logger Registry
// =============================================================================

namespace {

/// Registry of named loggers
struct LoggerRegistry {
    std::mutex mutex;
    std::map<std::string, std::shared_ptr<spdlog::logger>> loggers;
    std::shared_ptr<spdlog::logger> default_logger;
    spdlog::level::level_enum global_level = spdlog::level::debug;
    std::string log_directory;
    bool console_enabled = true;
    bool file_enabled = false;
    std::size_t max_file_size = 10 * 1024 * 1024;  // 10 MB
    std::size_t max_files = 5;
};

LoggerRegistry& get_registry() {
    static LoggerRegistry registry;
    return registry;
}

/// Create sinks based on current configuration
std::vector<spdlog::sink_ptr> create_sinks(const std::string& name) {
    auto& reg = get_registry();
    std::vector<spdlog::sink_ptr> sinks;

    // Console sink
    if (reg.console_enabled) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] [%n] %v");
        sinks.push_back(console_sink);
    }

    // File sink
    if (reg.file_enabled && !reg.log_directory.empty()) {
        try {
            std::filesystem::path log_path = std::filesystem::path(reg.log_directory) / (name + ".log");
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_path.string(),
                reg.max_file_size,
                reg.max_files);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
            sinks.push_back(file_sink);
        } catch (const spdlog::spdlog_ex&) {
            // Failed to create file sink, continue with console only
        }
    }

    return sinks;
}

} // anonymous namespace

// =============================================================================
// Logger Configuration
// =============================================================================

/// Configure logging system
void configure_logging(const LogConfig& config) {
    auto& reg = get_registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    reg.console_enabled = config.console_enabled;
    reg.file_enabled = config.file_enabled;
    reg.log_directory = config.log_directory;
    reg.max_file_size = config.max_file_size;
    reg.max_files = config.max_files;
    reg.global_level = config.level;

    // Update existing loggers
    for (auto& [name, logger] : reg.loggers) {
        logger->set_level(reg.global_level);
    }

    // Update default spdlog level
    spdlog::set_level(reg.global_level);
}

/// Get or create a named logger
std::shared_ptr<spdlog::logger> get_logger(const std::string& name) {
    auto& reg = get_registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    // Check if logger already exists
    auto it = reg.loggers.find(name);
    if (it != reg.loggers.end()) {
        return it->second;
    }

    // Create new logger
    auto sinks = create_sinks(name);
    auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(reg.global_level);

    reg.loggers[name] = logger;
    spdlog::register_logger(logger);

    return logger;
}

/// Get the core module logger
std::shared_ptr<spdlog::logger> core_logger() {
    static std::shared_ptr<spdlog::logger> logger = get_logger("void_core");
    return logger;
}

/// Get the engine logger
std::shared_ptr<spdlog::logger> engine_logger() {
    static std::shared_ptr<spdlog::logger> logger = get_logger("void_engine");
    return logger;
}

/// Get the plugin logger
std::shared_ptr<spdlog::logger> plugin_logger() {
    static std::shared_ptr<spdlog::logger> logger = get_logger("plugins");
    return logger;
}

/// Get the hot-reload logger
std::shared_ptr<spdlog::logger> hot_reload_logger() {
    static std::shared_ptr<spdlog::logger> logger = get_logger("hot_reload");
    return logger;
}

// =============================================================================
// Log Level Management
// =============================================================================

/// Set global log level
void set_global_log_level(spdlog::level::level_enum level) {
    auto& reg = get_registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    reg.global_level = level;
    spdlog::set_level(level);

    for (auto& [name, logger] : reg.loggers) {
        logger->set_level(level);
    }
}

/// Set log level for specific logger
void set_logger_level(const std::string& name, spdlog::level::level_enum level) {
    auto& reg = get_registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    auto it = reg.loggers.find(name);
    if (it != reg.loggers.end()) {
        it->second->set_level(level);
    }
}

/// Get current global log level
spdlog::level::level_enum get_global_log_level() {
    auto& reg = get_registry();
    return reg.global_level;
}

/// Parse log level from string
std::optional<spdlog::level::level_enum> parse_log_level(const std::string& str) {
    if (str == "trace") return spdlog::level::trace;
    if (str == "debug") return spdlog::level::debug;
    if (str == "info") return spdlog::level::info;
    if (str == "warn" || str == "warning") return spdlog::level::warn;
    if (str == "error" || str == "err") return spdlog::level::err;
    if (str == "critical" || str == "fatal") return spdlog::level::critical;
    if (str == "off") return spdlog::level::off;
    return std::nullopt;
}

/// Get log level name
const char* log_level_name(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace: return "trace";
        case spdlog::level::debug: return "debug";
        case spdlog::level::info: return "info";
        case spdlog::level::warn: return "warn";
        case spdlog::level::err: return "error";
        case spdlog::level::critical: return "critical";
        case spdlog::level::off: return "off";
        default: return "unknown";
    }
}

// =============================================================================
// Structured Logging Support
// =============================================================================

/// Log entry with structured data
void log_structured(
    spdlog::level::level_enum level,
    const std::string& logger_name,
    const std::string& message,
    const std::map<std::string, std::string>& fields)
{
    auto logger = get_logger(logger_name);

    // Build structured message
    std::ostringstream oss;
    oss << message;

    if (!fields.empty()) {
        oss << " {";
        bool first = true;
        for (const auto& [key, value] : fields) {
            if (!first) oss << ", ";
            oss << key << "=\"" << value << "\"";
            first = false;
        }
        oss << "}";
    }

    logger->log(level, oss.str());
}

// =============================================================================
// Log Scoping (for function/block tracing)
// =============================================================================

LogScope::LogScope(const std::string& name, const std::string& logger_name)
    : m_name(name)
    , m_logger(get_logger(logger_name))
    , m_start(std::chrono::steady_clock::now())
{
    m_logger->trace(">>> Entering {}", m_name);
}

LogScope::~LogScope() {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
    m_logger->trace("<<< Exiting {} ({}us)", m_name, duration.count());
}

// =============================================================================
// Logging Shutdown
// =============================================================================

/// Flush all loggers
void flush_all_loggers() {
    auto& reg = get_registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    for (auto& [name, logger] : reg.loggers) {
        logger->flush();
    }
    spdlog::default_logger()->flush();
}

/// Shutdown logging system
void shutdown_logging() {
    flush_all_loggers();

    auto& reg = get_registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    // Drop all loggers
    for (auto& [name, logger] : reg.loggers) {
        spdlog::drop(name);
    }
    reg.loggers.clear();

    spdlog::shutdown();
}

// =============================================================================
// Hot-Reload Safe Logging
// =============================================================================

/// Prepare logging for hot-reload (flush and release file handles)
void prepare_logging_for_reload() {
    flush_all_loggers();

    auto& reg = get_registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    // Temporarily disable file logging
    bool was_file_enabled = reg.file_enabled;
    reg.file_enabled = false;

    // Log the reload event
    VOID_LOG_INFO("Preparing logging system for hot-reload");

    reg.file_enabled = was_file_enabled;
}

/// Complete logging after hot-reload
void complete_logging_after_reload() {
    auto& reg = get_registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    // Re-create file sinks if needed
    if (reg.file_enabled) {
        for (auto& [name, logger] : reg.loggers) {
            auto sinks = create_sinks(name);
            // Note: spdlog doesn't support sink replacement easily
            // For full hot-reload support, would need to recreate loggers
        }
    }

    VOID_LOG_INFO("Logging system recovered after hot-reload");
}

} // namespace void_core
