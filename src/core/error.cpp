/// @file error.cpp
/// @brief Error handling implementation for void_core
///
/// The error system is primarily template-based and header-only.
/// This file provides:
/// - Compilation verification for error types
/// - Explicit template instantiations for common Result types
/// - Error formatting utilities

#include <void_engine/core/error.hpp>
#include <atomic>
#include <sstream>
#include <vector>

namespace void_core {

// =============================================================================
// Error Message Formatting (Out-of-line for complex cases)
// =============================================================================

namespace detail {

/// Format plugin error with full context
std::string format_plugin_error(const PluginError& err) {
    std::ostringstream oss;
    oss << "[PluginError] " << err.message;

    if (!err.plugin_id.empty()) {
        oss << " (plugin: " << err.plugin_id << ")";
    }
    if (!err.dependency.empty()) {
        oss << " (dependency: " << err.dependency << ")";
    }
    if (!err.expected.empty() && !err.found.empty()) {
        oss << " (expected: " << err.expected << ", found: " << err.found << ")";
    }

    return oss.str();
}

/// Format type registry error with full context
std::string format_type_registry_error(const TypeRegistryError& err) {
    std::ostringstream oss;
    oss << "[TypeRegistryError] " << err.message;

    if (!err.type_name.empty()) {
        oss << " (type: " << err.type_name << ")";
    }
    if (!err.expected.empty() && !err.found.empty()) {
        oss << " (expected: " << err.expected << ", found: " << err.found << ")";
    }

    return oss.str();
}

/// Format hot-reload error with full context
std::string format_hot_reload_error(const HotReloadError& err) {
    std::ostringstream oss;
    oss << "[HotReloadError] " << err.message;

    if (!err.old_version.empty() && !err.new_version.empty()) {
        oss << " (version: " << err.old_version << " -> " << err.new_version << ")";
    }

    return oss.str();
}

/// Format handle error with full context
std::string format_handle_error(const HandleError& err) {
    std::ostringstream oss;
    oss << "[HandleError] " << err.message;
    return oss.str();
}

} // namespace detail

// =============================================================================
// Error Chain Support
// =============================================================================

/// Build a full error message with context chain
std::string build_error_chain(const Error& error) {
    std::ostringstream oss;

    // Error code
    oss << "[" << error_code_name(error.code()) << "] ";

    // Main message based on variant type
    std::visit([&oss](const auto& err) {
        using T = std::decay_t<decltype(err)>;
        if constexpr (std::is_same_v<T, std::string>) {
            oss << err;
        } else if constexpr (std::is_same_v<T, PluginError>) {
            oss << detail::format_plugin_error(err);
        } else if constexpr (std::is_same_v<T, TypeRegistryError>) {
            oss << detail::format_type_registry_error(err);
        } else if constexpr (std::is_same_v<T, HotReloadError>) {
            oss << detail::format_hot_reload_error(err);
        } else if constexpr (std::is_same_v<T, HandleError>) {
            oss << detail::format_handle_error(err);
        }
    }, error.variant());

    return oss.str();
}

// =============================================================================
// Explicit Template Instantiations
// =============================================================================

// Common Result types used throughout the engine
// Note: std::size_t == std::uint64_t on this platform, so we only instantiate once
template class Result<void, Error>;
template class Result<bool, Error>;
template class Result<int, Error>;
template class Result<std::uint32_t, Error>;
template class Result<std::uint64_t, Error>;
template class Result<float, Error>;
template class Result<double, Error>;
template class Result<std::string, Error>;
template class Result<std::vector<std::uint8_t>, Error>;

// =============================================================================
// Error Statistics (Debug/Development)
// =============================================================================

namespace debug {

/// Global error statistics for debugging
struct ErrorStats {
    std::atomic<std::uint64_t> total_errors{0};
    std::atomic<std::uint64_t> plugin_errors{0};
    std::atomic<std::uint64_t> type_registry_errors{0};
    std::atomic<std::uint64_t> hot_reload_errors{0};
    std::atomic<std::uint64_t> handle_errors{0};
    std::atomic<std::uint64_t> generic_errors{0};
};

static ErrorStats s_error_stats;

/// Record error occurrence
void record_error(const Error& error) {
    s_error_stats.total_errors.fetch_add(1, std::memory_order_relaxed);

    if (error.is<PluginError>()) {
        s_error_stats.plugin_errors.fetch_add(1, std::memory_order_relaxed);
    } else if (error.is<TypeRegistryError>()) {
        s_error_stats.type_registry_errors.fetch_add(1, std::memory_order_relaxed);
    } else if (error.is<HotReloadError>()) {
        s_error_stats.hot_reload_errors.fetch_add(1, std::memory_order_relaxed);
    } else if (error.is<HandleError>()) {
        s_error_stats.handle_errors.fetch_add(1, std::memory_order_relaxed);
    } else {
        s_error_stats.generic_errors.fetch_add(1, std::memory_order_relaxed);
    }
}

/// Get total error count
std::uint64_t total_error_count() {
    return s_error_stats.total_errors.load(std::memory_order_relaxed);
}

/// Reset error statistics
void reset_error_stats() {
    s_error_stats.total_errors.store(0, std::memory_order_relaxed);
    s_error_stats.plugin_errors.store(0, std::memory_order_relaxed);
    s_error_stats.type_registry_errors.store(0, std::memory_order_relaxed);
    s_error_stats.hot_reload_errors.store(0, std::memory_order_relaxed);
    s_error_stats.handle_errors.store(0, std::memory_order_relaxed);
    s_error_stats.generic_errors.store(0, std::memory_order_relaxed);
}

/// Get error statistics as formatted string
std::string error_stats_summary() {
    std::ostringstream oss;
    oss << "Error Statistics:\n"
        << "  Total: " << s_error_stats.total_errors.load() << "\n"
        << "  Plugin: " << s_error_stats.plugin_errors.load() << "\n"
        << "  TypeRegistry: " << s_error_stats.type_registry_errors.load() << "\n"
        << "  HotReload: " << s_error_stats.hot_reload_errors.load() << "\n"
        << "  Handle: " << s_error_stats.handle_errors.load() << "\n"
        << "  Generic: " << s_error_stats.generic_errors.load() << "\n";
    return oss.str();
}

} // namespace debug

} // namespace void_core
