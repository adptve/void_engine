#pragma once

/// @file session.hpp
/// @brief Shell session management

#include "types.hpp"
#include "command.hpp"
#include "parser.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace void_shell {

// =============================================================================
// Environment
// =============================================================================

/// @brief Shell environment variables
class Environment {
public:
    Environment();
    explicit Environment(const Environment* parent);

    /// @brief Get a variable value
    std::optional<std::string> get(const std::string& name) const;

    /// @brief Set a variable value
    void set(const std::string& name, const std::string& value);

    /// @brief Unset a variable
    bool unset(const std::string& name);

    /// @brief Check if variable exists
    bool has(const std::string& name) const;

    /// @brief Get all variable names
    std::vector<std::string> keys() const;

    /// @brief Get all variables as key-value pairs
    std::unordered_map<std::string, std::string> all() const;

    /// @brief Clear all local variables (not inherited)
    void clear();

    /// @brief Import from system environment
    void import_system_env();

    /// @brief Export to system environment
    void export_to_system(const std::string& name) const;

    /// @brief Expand variables in a string
    std::string expand(const std::string& str) const;

    /// @brief Get typed value
    template <typename T>
    std::optional<T> get_as(const std::string& name) const;

    // Special variables
    std::string pwd() const;
    void set_pwd(const std::string& path);
    std::string home() const;
    std::string user() const;

private:
    const Environment* parent_ = nullptr;
    std::unordered_map<std::string, std::string> variables_;
    mutable std::mutex mutex_;
};

// =============================================================================
// History
// =============================================================================

/// @brief Command history management
class History {
public:
    History();
    explicit History(std::size_t max_size);

    /// @brief Add entry to history
    void add(const std::string& command, CommandStatus status = CommandStatus::Success,
             int exit_code = 0, std::chrono::microseconds duration = {});

    /// @brief Get entry by index (0 = most recent)
    const HistoryEntry* get(std::size_t index) const;

    /// @brief Get entry by absolute index
    const HistoryEntry* get_absolute(std::size_t index) const;

    /// @brief Search history
    std::vector<const HistoryEntry*> search(const std::string& query) const;

    /// @brief Search history with prefix
    std::vector<const HistoryEntry*> search_prefix(const std::string& prefix) const;

    /// @brief Get all entries
    const std::deque<HistoryEntry>& entries() const { return entries_; }

    /// @brief Get number of entries
    std::size_t size() const { return entries_.size(); }

    /// @brief Clear history
    void clear();

    /// @brief Get maximum size
    std::size_t max_size() const { return max_size_; }

    /// @brief Set maximum size
    void set_max_size(std::size_t size);

    /// @brief Load history from file
    bool load(const std::filesystem::path& path);

    /// @brief Save history to file
    bool save(const std::filesystem::path& path) const;

    /// @brief Get next entry index
    std::size_t next_index() const { return next_index_; }

private:
    std::deque<HistoryEntry> entries_;
    std::size_t max_size_ = 1000;
    std::size_t next_index_ = 1;
    mutable std::mutex mutex_;
};

// =============================================================================
// Session State
// =============================================================================

/// @brief Session state enumeration
enum class SessionState {
    Created,
    Active,
    Executing,
    Waiting,
    Suspended,
    Closed
};

// =============================================================================
// Background Job
// =============================================================================

/// @brief Background job information
struct BackgroundJob {
    std::uint32_t job_id = 0;
    std::string command;
    std::chrono::system_clock::time_point started_at;
    std::atomic<bool> running{true};
    std::atomic<bool> cancelled{false};
    std::thread thread;
    CommandResult result;

    bool is_done() const { return !running.load(); }
    void cancel() { cancelled.store(true); }
};

// =============================================================================
// Session
// =============================================================================

/// @brief Shell session
class Session {
public:
    Session(SessionId id, const ShellConfig& config);
    ~Session();

    // Non-copyable
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Movable
    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;

    // ==========================================================================
    // Identity
    // ==========================================================================

    SessionId id() const { return id_; }
    SessionState state() const { return state_; }
    const ShellConfig& config() const { return config_; }

    // ==========================================================================
    // Execution
    // ==========================================================================

    /// @brief Execute a command line
    CommandResult execute(const std::string& input);

    /// @brief Execute a parsed command
    CommandResult execute(const ParsedCommand& cmd);

    /// @brief Execute a command line (returns all results for pipelines)
    std::vector<CommandResult> execute_line(const CommandLine& line);

    /// @brief Execute in background
    std::uint32_t execute_background(const std::string& input);

    /// @brief Cancel current execution
    void cancel();

    /// @brief Check if cancelled
    bool is_cancelled() const { return cancelled_.load(); }

    // ==========================================================================
    // Environment & History
    // ==========================================================================

    Environment& env() { return env_; }
    const Environment& env() const { return env_; }

    History& history() { return history_; }
    const History& history() const { return history_; }

    // ==========================================================================
    // Working Directory
    // ==========================================================================

    std::filesystem::path cwd() const { return cwd_; }
    bool set_cwd(const std::filesystem::path& path);

    // ==========================================================================
    // I/O
    // ==========================================================================

    void set_output_callback(OutputCallback cb) { output_callback_ = std::move(cb); }
    void set_error_callback(ErrorCallback cb) { error_callback_ = std::move(cb); }
    void set_prompt_callback(PromptCallback cb) { prompt_callback_ = std::move(cb); }

    void print(const std::string& text);
    void println(const std::string& text);
    void print_error(const std::string& text);
    std::string get_prompt() const;

    // ==========================================================================
    // Tab Completion
    // ==========================================================================

    std::vector<std::string> complete(const std::string& input, std::size_t cursor_pos);
    void set_completion_callback(CompletionCallback cb) { completion_callback_ = std::move(cb); }

    // ==========================================================================
    // Background Jobs
    // ==========================================================================

    std::vector<const BackgroundJob*> jobs() const;
    BackgroundJob* get_job(std::uint32_t job_id);
    bool cancel_job(std::uint32_t job_id);
    bool wait_job(std::uint32_t job_id, std::chrono::milliseconds timeout = {});
    void cleanup_finished_jobs();

    // ==========================================================================
    // Statistics
    // ==========================================================================

    struct Stats {
        std::size_t commands_executed = 0;
        std::size_t commands_succeeded = 0;
        std::size_t commands_failed = 0;
        std::chrono::microseconds total_execution_time{0};
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point last_command_at;
    };

    Stats stats() const { return stats_; }

    // ==========================================================================
    // Last Result
    // ==========================================================================

    const CommandResult& last_result() const { return last_result_; }
    int last_exit_code() const { return last_result_.exit_code; }

private:
    SessionId id_;
    SessionState state_ = SessionState::Created;
    ShellConfig config_;

    Environment env_;
    History history_;
    std::filesystem::path cwd_;

    OutputCallback output_callback_;
    ErrorCallback error_callback_;
    PromptCallback prompt_callback_;
    CompletionCallback completion_callback_;

    std::atomic<bool> cancelled_{false};
    CommandResult last_result_;

    // Background jobs
    std::unordered_map<std::uint32_t, std::unique_ptr<BackgroundJob>> jobs_;
    std::uint32_t next_job_id_ = 1;
    mutable std::mutex jobs_mutex_;

    Stats stats_;

    Parser parser_;
    CommandRegistry* registry_ = nullptr;

    // Internal execution
    CommandResult execute_internal(const ParsedCommand& cmd);
    CommandResult execute_pipeline(const ParsedCommand& cmd);
    void update_stats(const CommandResult& result, std::chrono::microseconds duration);
};

// =============================================================================
// Session Manager
// =============================================================================

/// @brief Manages multiple shell sessions
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    // Singleton access
    static SessionManager& instance();

    // ==========================================================================
    // Session Management
    // ==========================================================================

    /// @brief Create a new session
    Session* create_session(const ShellConfig& config = {});

    /// @brief Get session by ID
    Session* get_session(SessionId id);
    const Session* get_session(SessionId id) const;

    /// @brief Get active session
    Session* active_session();

    /// @brief Set active session
    void set_active_session(SessionId id);

    /// @brief Close a session
    bool close_session(SessionId id);

    /// @brief Close all sessions
    void close_all_sessions();

    /// @brief Get all sessions
    std::vector<Session*> sessions();

    /// @brief Get session count
    std::size_t session_count() const;

    // ==========================================================================
    // Events
    // ==========================================================================

    using SessionCallback = std::function<void(Session&)>;
    void set_session_created_callback(SessionCallback cb) { on_created_ = std::move(cb); }
    void set_session_closed_callback(SessionCallback cb) { on_closed_ = std::move(cb); }

private:
    std::unordered_map<SessionId, std::unique_ptr<Session>> sessions_;
    SessionId active_session_id_;
    std::uint32_t next_session_id_ = 1;
    mutable std::mutex mutex_;

    SessionCallback on_created_;
    SessionCallback on_closed_;
};

// =============================================================================
// Template Implementation
// =============================================================================

template <typename T>
std::optional<T> Environment::get_as(const std::string& name) const {
    auto str = get(name);
    if (!str) return std::nullopt;

    if constexpr (std::is_same_v<T, std::string>) {
        return *str;
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, long> ||
                         std::is_same_v<T, std::int64_t>) {
        try {
            return static_cast<T>(std::stoll(*str));
        } catch (...) {
            return std::nullopt;
        }
    } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
        try {
            return static_cast<T>(std::stod(*str));
        } catch (...) {
            return std::nullopt;
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        std::string lower = *str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
    } else {
        return std::nullopt;
    }
}

} // namespace void_shell
