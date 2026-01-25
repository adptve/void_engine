#pragma once

/// @file session.hpp
/// @brief Session management for void_services
///
/// Sessions represent client connections with:
/// - Unique identification
/// - Authentication state
/// - Permissions system
/// - Session variables (key-value storage)
/// - Timeout management

#include "fwd.hpp"
#include <void_engine/core/id.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <any>

namespace void_services {

// =============================================================================
// Session ID
// =============================================================================

/// Unique session identifier
struct SessionId {
    std::uint64_t id = 0;

    SessionId() = default;
    explicit SessionId(std::uint64_t value) : id(value) {}

    [[nodiscard]] bool is_valid() const { return id != 0; }

    bool operator==(const SessionId& other) const { return id == other.id; }
    bool operator!=(const SessionId& other) const { return id != other.id; }
    bool operator<(const SessionId& other) const { return id < other.id; }
};

} // namespace void_services

// Hash specialization must be defined before first use in unordered_map
template<>
struct std::hash<void_services::SessionId> {
    std::size_t operator()(const void_services::SessionId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.id);
    }
};

namespace void_services {

// =============================================================================
// Session State
// =============================================================================

/// Session lifecycle state
enum class SessionState {
    Created,        ///< Session created, not yet active
    Active,         ///< Session is active and usable
    Suspended,      ///< Session temporarily suspended
    Expired,        ///< Session expired due to timeout
    Terminated,     ///< Session explicitly terminated
};

/// Convert state to string
[[nodiscard]] inline const char* to_string(SessionState state) {
    switch (state) {
        case SessionState::Created: return "Created";
        case SessionState::Active: return "Active";
        case SessionState::Suspended: return "Suspended";
        case SessionState::Expired: return "Expired";
        case SessionState::Terminated: return "Terminated";
    }
    return "Unknown";
}

// =============================================================================
// Session
// =============================================================================

/// Represents a client session
class Session {
public:
    /// Create a new session
    explicit Session(SessionId id)
        : m_id(id)
        , m_state(SessionState::Created)
        , m_created_at(std::chrono::steady_clock::now())
        , m_last_activity(m_created_at)
        , m_authenticated(false)
    {
    }

    // =========================================================================
    // Identification
    // =========================================================================

    /// Get session ID
    [[nodiscard]] SessionId id() const { return m_id; }

    /// Get associated user ID (if authenticated)
    [[nodiscard]] std::optional<std::string> user_id() const {
        std::shared_lock lock(m_mutex);
        return m_user_id;
    }

    /// Set user ID (on authentication)
    void set_user_id(const std::string& user_id) {
        std::unique_lock lock(m_mutex);
        m_user_id = user_id;
        m_authenticated = true;
    }

    /// Check if authenticated
    [[nodiscard]] bool is_authenticated() const {
        std::shared_lock lock(m_mutex);
        return m_authenticated;
    }

    // =========================================================================
    // State
    // =========================================================================

    /// Get current state
    [[nodiscard]] SessionState state() const {
        return m_state.load();
    }

    /// Activate the session
    void activate() {
        m_state = SessionState::Active;
        touch();
    }

    /// Suspend the session
    void suspend() {
        m_state = SessionState::Suspended;
    }

    /// Resume a suspended session
    void resume() {
        if (m_state == SessionState::Suspended) {
            m_state = SessionState::Active;
            touch();
        }
    }

    /// Expire the session
    void expire() {
        m_state = SessionState::Expired;
    }

    /// Terminate the session
    void terminate() {
        m_state = SessionState::Terminated;
    }

    /// Restore session state directly (for hot-reload)
    /// @note This bypasses normal state transitions for restoration purposes
    void restore_state(SessionState state) {
        m_state = state;
    }

    /// Restore authentication state (for hot-reload)
    void restore_auth(std::optional<std::string> user_id, bool authenticated) {
        std::unique_lock lock(m_mutex);
        m_user_id = std::move(user_id);
        m_authenticated = authenticated;
    }

    /// Check if session is usable
    [[nodiscard]] bool is_active() const {
        return m_state == SessionState::Active;
    }

    // =========================================================================
    // Activity Tracking
    // =========================================================================

    /// Update last activity time
    void touch() {
        std::unique_lock lock(m_mutex);
        m_last_activity = std::chrono::steady_clock::now();
    }

    /// Get creation time
    [[nodiscard]] std::chrono::steady_clock::time_point created_at() const {
        return m_created_at;
    }

    /// Get last activity time
    [[nodiscard]] std::chrono::steady_clock::time_point last_activity() const {
        std::shared_lock lock(m_mutex);
        return m_last_activity;
    }

    /// Get idle duration
    [[nodiscard]] std::chrono::steady_clock::duration idle_time() const {
        std::shared_lock lock(m_mutex);
        return std::chrono::steady_clock::now() - m_last_activity;
    }

    /// Get session age
    [[nodiscard]] std::chrono::steady_clock::duration age() const {
        return std::chrono::steady_clock::now() - m_created_at;
    }

    // =========================================================================
    // Permissions
    // =========================================================================

    /// Check if session has a permission
    [[nodiscard]] bool has_permission(const std::string& permission) const {
        std::shared_lock lock(m_mutex);

        // Wildcard grants all permissions
        if (m_permissions.count("*")) {
            return true;
        }

        // Check exact match
        if (m_permissions.count(permission)) {
            return true;
        }

        // Check hierarchy (e.g., "assets.*" grants "assets.read")
        auto dot_pos = permission.rfind('.');
        while (dot_pos != std::string::npos) {
            std::string parent = permission.substr(0, dot_pos) + ".*";
            if (m_permissions.count(parent)) {
                return true;
            }
            dot_pos = permission.rfind('.', dot_pos - 1);
        }

        return false;
    }

    /// Grant a permission
    void grant_permission(const std::string& permission) {
        std::unique_lock lock(m_mutex);
        m_permissions.insert(permission);
    }

    /// Revoke a permission
    void revoke_permission(const std::string& permission) {
        std::unique_lock lock(m_mutex);
        m_permissions.erase(permission);
    }

    /// Get all permissions
    [[nodiscard]] std::unordered_set<std::string> permissions() const {
        std::shared_lock lock(m_mutex);
        return m_permissions;
    }

    /// Clear all permissions
    void clear_permissions() {
        std::unique_lock lock(m_mutex);
        m_permissions.clear();
    }

    // =========================================================================
    // Session Variables
    // =========================================================================

    /// Set a session variable
    template<typename T>
    void set(const std::string& key, T&& value) {
        std::unique_lock lock(m_mutex);
        m_variables[key] = std::forward<T>(value);
    }

    /// Get a session variable
    template<typename T>
    [[nodiscard]] std::optional<T> get(const std::string& key) const {
        std::shared_lock lock(m_mutex);
        auto it = m_variables.find(key);
        if (it == m_variables.end()) {
            return std::nullopt;
        }
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }

    /// Check if variable exists
    [[nodiscard]] bool has_variable(const std::string& key) const {
        std::shared_lock lock(m_mutex);
        return m_variables.count(key) > 0;
    }

    /// Remove a variable
    void remove_variable(const std::string& key) {
        std::unique_lock lock(m_mutex);
        m_variables.erase(key);
    }

    /// Clear all variables
    void clear_variables() {
        std::unique_lock lock(m_mutex);
        m_variables.clear();
    }

    // =========================================================================
    // Metadata
    // =========================================================================

    /// Set metadata
    void set_metadata(const std::string& key, const std::string& value) {
        std::unique_lock lock(m_mutex);
        m_metadata[key] = value;
    }

    /// Get metadata
    [[nodiscard]] std::optional<std::string> get_metadata(const std::string& key) const {
        std::shared_lock lock(m_mutex);
        auto it = m_metadata.find(key);
        return it != m_metadata.end() ? std::optional{it->second} : std::nullopt;
    }

    /// Get all metadata
    [[nodiscard]] std::unordered_map<std::string, std::string> metadata() const {
        std::shared_lock lock(m_mutex);
        return m_metadata;
    }

private:
    SessionId m_id;
    std::atomic<SessionState> m_state;
    std::chrono::steady_clock::time_point m_created_at;

    mutable std::shared_mutex m_mutex;
    std::chrono::steady_clock::time_point m_last_activity;
    std::optional<std::string> m_user_id;
    bool m_authenticated;
    std::unordered_set<std::string> m_permissions;
    std::unordered_map<std::string, std::any> m_variables;
    std::unordered_map<std::string, std::string> m_metadata;
};

// =============================================================================
// Session Manager Configuration
// =============================================================================

/// Session manager configuration
struct SessionManagerConfig {
    /// Maximum concurrent sessions (0 = unlimited)
    std::size_t max_sessions = 0;

    /// Session timeout (0 = no timeout)
    std::chrono::seconds session_timeout{3600}; // 1 hour

    /// Cleanup interval for expired sessions
    std::chrono::seconds cleanup_interval{60};

    /// Allow anonymous sessions
    bool allow_anonymous = true;

    /// Default permissions for new sessions
    std::vector<std::string> default_permissions;
};

// =============================================================================
// Session Manager Statistics
// =============================================================================

/// Session manager statistics
struct SessionStats {
    std::size_t active_sessions = 0;
    std::size_t total_created = 0;
    std::size_t total_terminated = 0;
    std::size_t total_expired = 0;
    std::size_t peak_concurrent = 0;
    std::size_t authenticated_sessions = 0;
    std::size_t anonymous_sessions = 0;
};

// =============================================================================
// Session Manager
// =============================================================================

/// Manages session lifecycles
class SessionManager {
public:
    explicit SessionManager(SessionManagerConfig config = {})
        : m_config(std::move(config))
        , m_next_session_id(1)
        , m_cleanup_running(false)
    {
    }

    ~SessionManager() {
        stop_cleanup();
    }

    // =========================================================================
    // Session Creation
    // =========================================================================

    /// Create a new session
    [[nodiscard]] std::shared_ptr<Session> create_session() {
        std::unique_lock lock(m_mutex);

        // Check capacity
        if (m_config.max_sessions > 0 && m_sessions.size() >= m_config.max_sessions) {
            return nullptr;
        }

        SessionId id(m_next_session_id++);
        auto session = std::make_shared<Session>(id);

        // Grant default permissions
        for (const auto& perm : m_config.default_permissions) {
            session->grant_permission(perm);
        }

        m_sessions[id] = session;
        ++m_stats.total_created;
        m_stats.peak_concurrent = std::max(m_stats.peak_concurrent, m_sessions.size());

        return session;
    }

    /// Create an authenticated session
    [[nodiscard]] std::shared_ptr<Session> create_authenticated_session(
        const std::string& user_id) {
        auto session = create_session();
        if (session) {
            session->set_user_id(user_id);
            session->activate();

            std::unique_lock lock(m_mutex);
            m_user_sessions[user_id].push_back(session->id());
        }
        return session;
    }

    // =========================================================================
    // Session Access
    // =========================================================================

    /// Get a session by ID
    [[nodiscard]] std::shared_ptr<Session> get(SessionId id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_sessions.find(id);
        return it != m_sessions.end() ? it->second : nullptr;
    }

    /// Get sessions for a user
    [[nodiscard]] std::vector<std::shared_ptr<Session>> get_user_sessions(
        const std::string& user_id) const {
        std::shared_lock lock(m_mutex);

        std::vector<std::shared_ptr<Session>> result;
        auto it = m_user_sessions.find(user_id);
        if (it != m_user_sessions.end()) {
            for (const auto& sid : it->second) {
                auto sit = m_sessions.find(sid);
                if (sit != m_sessions.end()) {
                    result.push_back(sit->second);
                }
            }
        }
        return result;
    }

    /// Get all active session IDs
    [[nodiscard]] std::vector<SessionId> list_active() const {
        std::shared_lock lock(m_mutex);

        std::vector<SessionId> result;
        for (const auto& [id, session] : m_sessions) {
            if (session->is_active()) {
                result.push_back(id);
            }
        }
        return result;
    }

    // =========================================================================
    // Session Lifecycle
    // =========================================================================

    /// Terminate a session
    void terminate(SessionId id) {
        std::unique_lock lock(m_mutex);

        auto it = m_sessions.find(id);
        if (it != m_sessions.end()) {
            it->second->terminate();

            // Remove from user sessions
            auto user_id = it->second->user_id();
            if (user_id) {
                auto uit = m_user_sessions.find(*user_id);
                if (uit != m_user_sessions.end()) {
                    uit->second.erase(
                        std::remove(uit->second.begin(), uit->second.end(), id),
                        uit->second.end());
                }
            }

            m_sessions.erase(it);
            ++m_stats.total_terminated;
        }
    }

    /// Terminate all sessions for a user
    void terminate_user(const std::string& user_id) {
        std::unique_lock lock(m_mutex);

        auto it = m_user_sessions.find(user_id);
        if (it != m_user_sessions.end()) {
            for (const auto& sid : it->second) {
                auto sit = m_sessions.find(sid);
                if (sit != m_sessions.end()) {
                    sit->second->terminate();
                    m_sessions.erase(sit);
                    ++m_stats.total_terminated;
                }
            }
            m_user_sessions.erase(it);
        }
    }

    // =========================================================================
    // Cleanup
    // =========================================================================

    /// Start automatic cleanup thread
    void start_cleanup() {
        if (m_cleanup_running) return;

        m_cleanup_running = true;
        m_cleanup_thread = std::thread(&SessionManager::cleanup_loop, this);
    }

    /// Stop cleanup thread
    void stop_cleanup() {
        m_cleanup_running = false;
        if (m_cleanup_thread.joinable()) {
            m_cleanup_thread.join();
        }
    }

    /// Manually run cleanup
    void cleanup_expired() {
        if (m_config.session_timeout.count() == 0) return;

        std::unique_lock lock(m_mutex);

        auto now = std::chrono::steady_clock::now();
        std::vector<SessionId> expired;

        for (const auto& [id, session] : m_sessions) {
            if (session->is_active()) {
                auto idle = std::chrono::duration_cast<std::chrono::seconds>(
                    now - session->last_activity());
                if (idle > m_config.session_timeout) {
                    session->expire();
                    expired.push_back(id);
                }
            }
        }

        for (const auto& id : expired) {
            m_sessions.erase(id);
            ++m_stats.total_expired;
        }
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get statistics
    [[nodiscard]] SessionStats stats() const {
        std::shared_lock lock(m_mutex);

        SessionStats s = m_stats;
        s.active_sessions = 0;
        s.authenticated_sessions = 0;
        s.anonymous_sessions = 0;

        for (const auto& [id, session] : m_sessions) {
            if (session->is_active()) {
                ++s.active_sessions;
                if (session->is_authenticated()) {
                    ++s.authenticated_sessions;
                } else {
                    ++s.anonymous_sessions;
                }
            }
        }

        return s;
    }

    /// Get configuration
    [[nodiscard]] const SessionManagerConfig& config() const { return m_config; }

    // =========================================================================
    // Hot-Reload Restore
    // =========================================================================

    /// Restore a session from snapshot data (for hot-reload)
    /// @param id Session ID to restore
    /// @param state Session state
    /// @param user_id Optional user ID
    /// @param authenticated Whether session is authenticated
    /// @param permissions Set of permission strings
    /// @param metadata Key-value metadata pairs
    /// @return The restored session, or nullptr if capacity exceeded
    [[nodiscard]] std::shared_ptr<Session> restore_session(
        std::uint64_t id,
        SessionState state,
        std::optional<std::string> user_id,
        bool authenticated,
        const std::vector<std::string>& permissions,
        const std::vector<std::pair<std::string, std::string>>& metadata) {

        std::unique_lock lock(m_mutex);

        // Check capacity
        if (m_config.max_sessions > 0 && m_sessions.size() >= m_config.max_sessions) {
            return nullptr;
        }

        // Update next ID to avoid conflicts
        if (id >= m_next_session_id) {
            m_next_session_id = id + 1;
        }

        SessionId session_id(id);
        auto session = std::make_shared<Session>(session_id);

        // Restore state
        session->restore_state(state);
        session->restore_auth(user_id, authenticated);

        // Restore permissions
        for (const auto& perm : permissions) {
            session->grant_permission(perm);
        }

        // Restore metadata
        for (const auto& [key, value] : metadata) {
            session->set_metadata(key, value);
        }

        m_sessions[session_id] = session;

        // Track user sessions
        if (user_id) {
            m_user_sessions[*user_id].push_back(session_id);
        }

        m_stats.peak_concurrent = std::max(m_stats.peak_concurrent, m_sessions.size());

        return session;
    }

    /// Restore session manager stats (for hot-reload)
    void restore_stats(std::uint64_t total_created, std::uint64_t total_terminated,
                       std::uint64_t total_expired, std::size_t peak_concurrent) {
        std::unique_lock lock(m_mutex);
        m_stats.total_created = total_created;
        m_stats.total_terminated = total_terminated;
        m_stats.total_expired = total_expired;
        m_stats.peak_concurrent = peak_concurrent;
    }

    /// Set next session ID (for hot-reload)
    void set_next_session_id(std::uint64_t id) {
        std::unique_lock lock(m_mutex);
        m_next_session_id = id;
    }

private:
    void cleanup_loop() {
        while (m_cleanup_running) {
            std::this_thread::sleep_for(m_config.cleanup_interval);
            if (m_cleanup_running) {
                cleanup_expired();
            }
        }
    }

    SessionManagerConfig m_config;
    mutable std::shared_mutex m_mutex;

    std::uint64_t m_next_session_id;
    std::unordered_map<SessionId, std::shared_ptr<Session>> m_sessions;
    std::unordered_map<std::string, std::vector<SessionId>> m_user_sessions;

    SessionStats m_stats;

    std::atomic<bool> m_cleanup_running;
    std::thread m_cleanup_thread;
};

} // namespace void_services
