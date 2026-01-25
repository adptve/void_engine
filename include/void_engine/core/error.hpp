#pragma once

/// @file error.hpp
/// @brief Error handling types for void_core

#include "fwd.hpp"
#include <cstdint>
#include <string>
#include <variant>
#include <optional>
#include <utility>
#include <map>
#include <stdexcept>

namespace void_core {

// =============================================================================
// ErrorCode
// =============================================================================

/// General error code for categorizing errors
enum class ErrorCode : std::uint8_t {
    Unknown = 0,
    NotFound,
    AlreadyExists,
    InvalidArgument,
    InvalidState,
    IOError,
    ParseError,
    CompileError,
    ValidationError,
    IncompatibleVersion,
    DependencyMissing,
    Timeout,
    OutOfMemory,
    PermissionDenied,
    NotSupported,
};

/// Get error code name
[[nodiscard]] inline const char* error_code_name(ErrorCode code) {
    switch (code) {
        case ErrorCode::Unknown: return "Unknown";
        case ErrorCode::NotFound: return "NotFound";
        case ErrorCode::AlreadyExists: return "AlreadyExists";
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::InvalidState: return "InvalidState";
        case ErrorCode::IOError: return "IOError";
        case ErrorCode::ParseError: return "ParseError";
        case ErrorCode::CompileError: return "CompileError";
        case ErrorCode::ValidationError: return "ValidationError";
        case ErrorCode::IncompatibleVersion: return "IncompatibleVersion";
        case ErrorCode::DependencyMissing: return "DependencyMissing";
        case ErrorCode::Timeout: return "Timeout";
        case ErrorCode::OutOfMemory: return "OutOfMemory";
        case ErrorCode::PermissionDenied: return "PermissionDenied";
        case ErrorCode::NotSupported: return "NotSupported";
        default: return "Unknown";
    }
}

// =============================================================================
// Error Kinds
// =============================================================================

/// Plugin-related errors
struct PluginError {
    enum class Kind : std::uint8_t {
        NotFound,           // Plugin not found by ID
        AlreadyRegistered,  // Plugin ID already registered
        MissingDependency,  // Dependency not satisfied
        VersionMismatch,    // Version incompatibility
        InitFailed,         // Plugin initialization failed
        InvalidState,       // Plugin in invalid state for operation
    };

    Kind kind;
    std::string message;
    std::string plugin_id;
    std::string dependency;  // For MissingDependency
    std::string expected;    // For VersionMismatch
    std::string found;       // For VersionMismatch

    /// Factory methods
    [[nodiscard]] static PluginError not_found(const std::string& id) {
        return PluginError{Kind::NotFound, "Plugin not found: " + id, id, {}, {}, {}};
    }

    [[nodiscard]] static PluginError already_registered(const std::string& id) {
        return PluginError{Kind::AlreadyRegistered, "Plugin already registered: " + id, id, {}, {}, {}};
    }

    [[nodiscard]] static PluginError missing_dependency(const std::string& plugin, const std::string& dep) {
        return PluginError{Kind::MissingDependency,
            "Plugin '" + plugin + "' missing dependency: " + dep, plugin, dep, {}, {}};
    }

    [[nodiscard]] static PluginError version_mismatch(const std::string& expected_ver, const std::string& found_ver) {
        return PluginError{Kind::VersionMismatch,
            "Version mismatch: expected " + expected_ver + ", found " + found_ver,
            {}, {}, expected_ver, found_ver};
    }

    [[nodiscard]] static PluginError init_failed(const std::string& id, const std::string& reason) {
        return PluginError{Kind::InitFailed, "Plugin '" + id + "' init failed: " + reason, id, {}, {}, {}};
    }

    [[nodiscard]] static PluginError invalid_state(const std::string& id, const std::string& reason) {
        return PluginError{Kind::InvalidState, "Plugin '" + id + "' invalid state: " + reason, id, {}, {}, {}};
    }
};

/// Type registry errors
struct TypeRegistryError {
    enum class Kind : std::uint8_t {
        NotRegistered,      // Type not registered
        AlreadyRegistered,  // Type already registered
        TypeMismatch,       // Cast type mismatch
    };

    Kind kind;
    std::string message;
    std::string type_name;
    std::string expected;  // For TypeMismatch
    std::string found;     // For TypeMismatch

    [[nodiscard]] static TypeRegistryError not_registered(const std::string& name) {
        return TypeRegistryError{Kind::NotRegistered, "Type not registered: " + name, name, {}, {}};
    }

    [[nodiscard]] static TypeRegistryError already_registered(const std::string& name) {
        return TypeRegistryError{Kind::AlreadyRegistered, "Type already registered: " + name, name, {}, {}};
    }

    [[nodiscard]] static TypeRegistryError type_mismatch(const std::string& expected_t, const std::string& found_t) {
        return TypeRegistryError{Kind::TypeMismatch,
            "Type mismatch: expected " + expected_t + ", found " + found_t,
            {}, expected_t, found_t};
    }
};

/// Hot-reload errors
struct HotReloadError {
    enum class Kind : std::uint8_t {
        SnapshotFailed,       // Snapshot creation failed
        RestoreFailed,        // Restore from snapshot failed
        IncompatibleVersion,  // Version incompatibility for reload
        WatchError,           // File watching error
        AlreadyRegistered,    // Object already registered
        NotFound,             // Object not found
        InvalidState,         // Invalid state for operation
    };

    Kind kind;
    std::string message;
    std::string old_version;
    std::string new_version;

    [[nodiscard]] static HotReloadError snapshot_failed(const std::string& reason) {
        return HotReloadError{Kind::SnapshotFailed, "Snapshot failed: " + reason, {}, {}};
    }

    [[nodiscard]] static HotReloadError restore_failed(const std::string& reason) {
        return HotReloadError{Kind::RestoreFailed, "Restore failed: " + reason, {}, {}};
    }

    [[nodiscard]] static HotReloadError incompatible_version(const std::string& old_ver, const std::string& new_ver) {
        return HotReloadError{Kind::IncompatibleVersion,
            "Incompatible versions: " + old_ver + " -> " + new_ver, old_ver, new_ver};
    }

    [[nodiscard]] static HotReloadError watch_error(const std::string& reason) {
        return HotReloadError{Kind::WatchError, "Watch error: " + reason, {}, {}};
    }

    [[nodiscard]] static HotReloadError already_registered(const std::string& name) {
        return HotReloadError{Kind::AlreadyRegistered, "Already registered: " + name, {}, {}};
    }

    [[nodiscard]] static HotReloadError not_found(const std::string& name) {
        return HotReloadError{Kind::NotFound, "Not found: " + name, {}, {}};
    }

    [[nodiscard]] static HotReloadError invalid_state(const std::string& reason) {
        return HotReloadError{Kind::InvalidState, "Invalid state: " + reason, {}, {}};
    }

    [[nodiscard]] static HotReloadError invalid_state(const std::string& name, const std::string& reason) {
        return HotReloadError{Kind::InvalidState, name + ": " + reason, {}, {}};
    }
};

/// Handle errors
struct HandleError {
    enum class Kind : std::uint8_t {
        Null,         // Handle is null
        Stale,        // Handle generation mismatch (already freed)
        OutOfBounds,  // Handle index out of bounds
    };

    Kind kind;
    std::string message;

    [[nodiscard]] static HandleError null() {
        return HandleError{Kind::Null, "Handle is null"};
    }

    [[nodiscard]] static HandleError stale() {
        return HandleError{Kind::Stale, "Handle is stale (generation mismatch)"};
    }

    [[nodiscard]] static HandleError out_of_bounds() {
        return HandleError{Kind::OutOfBounds, "Handle index out of bounds"};
    }
};

// =============================================================================
// Error
// =============================================================================

/// Main error type (variant of all error kinds)
class Error {
public:
    using Variant = std::variant<
        PluginError,
        TypeRegistryError,
        HotReloadError,
        HandleError,
        std::string  // Generic message
    >;

    /// Constructors
    Error() : m_code(ErrorCode::Unknown), m_error("Unknown error") {}
    Error(PluginError err) : m_code(to_error_code(err.kind)), m_error(std::move(err)) {}
    Error(TypeRegistryError err) : m_code(to_error_code(err.kind)), m_error(std::move(err)) {}
    Error(HotReloadError err) : m_code(to_error_code(err.kind)), m_error(std::move(err)) {}
    Error(HandleError err) : m_code(to_error_code(err.kind)), m_error(std::move(err)) {}
    Error(const std::string& msg) : m_code(ErrorCode::Unknown), m_error(msg) {}
    Error(const char* msg) : m_code(ErrorCode::Unknown), m_error(std::string(msg)) {}

    /// Construct with error code and message
    Error(ErrorCode code, const std::string& msg) : m_code(code), m_error(msg) {}
    Error(ErrorCode code, const char* msg) : m_code(code), m_error(std::string(msg)) {}

    /// Get error code
    [[nodiscard]] ErrorCode code() const noexcept { return m_code; }

    /// Get error message
    [[nodiscard]] std::string message() const {
        return std::visit([](const auto& err) -> std::string {
            using T = std::decay_t<decltype(err)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return err;
            } else {
                return err.message;
            }
        }, m_error);
    }

    /// Check error type
    template<typename T>
    [[nodiscard]] bool is() const {
        return std::holds_alternative<T>(m_error);
    }

    /// Get error as specific type
    template<typename T>
    [[nodiscard]] const T* as() const {
        return std::get_if<T>(&m_error);
    }

    /// Get underlying variant
    [[nodiscard]] const Variant& variant() const noexcept { return m_error; }

    /// Add context information
    Error& with_context(const std::string& key, const std::string& value) {
        m_context[key] = value;
        return *this;
    }

    /// Get context value
    [[nodiscard]] const std::string* get_context(const std::string& key) const {
        auto it = m_context.find(key);
        return it != m_context.end() ? &it->second : nullptr;
    }

private:
    static ErrorCode to_error_code(PluginError::Kind kind) {
        switch (kind) {
            case PluginError::Kind::NotFound: return ErrorCode::NotFound;
            case PluginError::Kind::AlreadyRegistered: return ErrorCode::AlreadyExists;
            case PluginError::Kind::MissingDependency: return ErrorCode::DependencyMissing;
            case PluginError::Kind::VersionMismatch: return ErrorCode::IncompatibleVersion;
            case PluginError::Kind::InitFailed: return ErrorCode::InvalidState;
            case PluginError::Kind::InvalidState: return ErrorCode::InvalidState;
            default: return ErrorCode::Unknown;
        }
    }

    static ErrorCode to_error_code(TypeRegistryError::Kind kind) {
        switch (kind) {
            case TypeRegistryError::Kind::NotRegistered: return ErrorCode::NotFound;
            case TypeRegistryError::Kind::AlreadyRegistered: return ErrorCode::AlreadyExists;
            case TypeRegistryError::Kind::TypeMismatch: return ErrorCode::InvalidArgument;
            default: return ErrorCode::Unknown;
        }
    }

    static ErrorCode to_error_code(HotReloadError::Kind kind) {
        switch (kind) {
            case HotReloadError::Kind::SnapshotFailed: return ErrorCode::InvalidState;
            case HotReloadError::Kind::RestoreFailed: return ErrorCode::InvalidState;
            case HotReloadError::Kind::IncompatibleVersion: return ErrorCode::IncompatibleVersion;
            case HotReloadError::Kind::WatchError: return ErrorCode::IOError;
            case HotReloadError::Kind::AlreadyRegistered: return ErrorCode::AlreadyExists;
            case HotReloadError::Kind::NotFound: return ErrorCode::NotFound;
            case HotReloadError::Kind::InvalidState: return ErrorCode::InvalidState;
            default: return ErrorCode::Unknown;
        }
    }

    static ErrorCode to_error_code(HandleError::Kind kind) {
        switch (kind) {
            case HandleError::Kind::Null: return ErrorCode::InvalidArgument;
            case HandleError::Kind::Stale: return ErrorCode::InvalidState;
            case HandleError::Kind::OutOfBounds: return ErrorCode::InvalidArgument;
            default: return ErrorCode::Unknown;
        }
    }

    ErrorCode m_code;
    Variant m_error;
    std::map<std::string, std::string> m_context;
};

// =============================================================================
// Result<T, E>
// =============================================================================

/// Result type (similar to Rust's Result<T, E>)
/// @tparam T Value type
/// @tparam E Error type (defaults to Error)
template<typename T, typename E>
class Result {
public:
    using value_type = T;
    using error_type = E;

    /// Success constructor
    Result(T value) : m_value(std::move(value)) {}

    /// Error constructor
    Result(E error) : m_error(std::move(error)) {}

    /// Check if result is ok
    [[nodiscard]] bool is_ok() const noexcept { return m_value.has_value(); }

    /// Check if result is error
    [[nodiscard]] bool is_err() const noexcept { return !m_value.has_value(); }

    /// Get value (undefined if error)
    [[nodiscard]] T& value() & { return *m_value; }
    [[nodiscard]] const T& value() const& { return *m_value; }
    [[nodiscard]] T&& value() && { return std::move(*m_value); }

    /// Get error (undefined if ok)
    [[nodiscard]] E& error() & { return m_error; }
    [[nodiscard]] const E& error() const& { return m_error; }

    /// Get value or default
    [[nodiscard]] T value_or(T default_value) const {
        return m_value.has_value() ? *m_value : std::move(default_value);
    }

    /// Operator bool (true if ok)
    explicit operator bool() const noexcept { return m_value.has_value(); }

    /// Dereference operator (returns value)
    [[nodiscard]] T& operator*() & { return *m_value; }
    [[nodiscard]] const T& operator*() const& { return *m_value; }
    [[nodiscard]] T&& operator*() && { return std::move(*m_value); }

    /// Arrow operator
    [[nodiscard]] T* operator->() { return &(*m_value); }
    [[nodiscard]] const T* operator->() const { return &(*m_value); }

    /// Unwrap (throws if error)
    [[nodiscard]] T& unwrap() & {
        if (!m_value.has_value()) {
            throw std::runtime_error("Result contains error");
        }
        return *m_value;
    }

    [[nodiscard]] T&& unwrap() && {
        if (!m_value.has_value()) {
            throw std::runtime_error("Result contains error");
        }
        return std::move(*m_value);
    }

    /// Map success value
    template<typename F>
    auto map(F&& func) -> Result<decltype(func(std::declval<T>())), E> {
        using U = decltype(func(std::declval<T>()));
        if (m_value.has_value()) {
            return Result<U, E>(func(std::move(*m_value)));
        }
        return Result<U, E>(std::move(m_error));
    }

    /// Chain operations
    template<typename F>
    auto and_then(F&& func) -> decltype(func(std::declval<T>())) {
        if (m_value.has_value()) {
            return func(std::move(*m_value));
        }
        using ResultType = decltype(func(std::declval<T>()));
        return ResultType(std::move(m_error));
    }

    /// Handle error case
    template<typename F>
    auto or_else(F&& func) -> Result<T, E> {
        if (m_value.has_value()) {
            return Result<T, E>(std::move(*m_value));
        }
        return func(m_error);
    }

private:
    std::optional<T> m_value;
    E m_error;
};

/// Partial specialization for void result
template<typename E>
class Result<void, E> {
public:
    using value_type = void;
    using error_type = E;

    /// Success constructor
    Result() : m_has_value(true) {}

    /// Error constructor
    Result(E error) : m_error(std::move(error)), m_has_value(false) {}

    /// Static factory for success
    [[nodiscard]] static Result ok() { return Result(); }

    /// Check if result is ok
    [[nodiscard]] bool is_ok() const noexcept { return m_has_value; }

    /// Check if result is error
    [[nodiscard]] bool is_err() const noexcept { return !m_has_value; }

    /// Get error
    [[nodiscard]] E& error() & { return m_error; }
    [[nodiscard]] const E& error() const& { return m_error; }

    /// Operator bool
    explicit operator bool() const noexcept { return m_has_value; }

    /// Unwrap
    void unwrap() const {
        if (!m_has_value) {
            throw std::runtime_error("Result contains error");
        }
    }

private:
    E m_error;
    bool m_has_value;
};

/// Helper for creating Ok result
template<typename T>
Result<T> Ok(T value) {
    return Result<T>(std::move(value));
}

/// Helper for creating Ok void result
inline Result<void> Ok() {
    return Result<void>();
}

/// Helper for creating Err result
template<typename T = void>
Result<T> Err(Error error) {
    return Result<T>(std::move(error));
}

template<typename T = void>
Result<T> Err(const std::string& message) {
    return Result<T>(Error(message));
}

// =============================================================================
// Error Utilities (Implemented in error.cpp)
// =============================================================================

/// Build a full error message with context chain
std::string build_error_chain(const Error& error);

namespace debug {

/// Record error occurrence (for statistics)
void record_error(const Error& error);

/// Get total error count
std::uint64_t total_error_count();

/// Reset error statistics
void reset_error_stats();

/// Get error statistics as formatted string
std::string error_stats_summary();

} // namespace debug

} // namespace void_core
