#pragma once

/// @file shell.hpp
/// @brief Main shell system facade

#include "types.hpp"
#include "command.hpp"
#include "session.hpp"

#include <void_engine/event/event_bus.hpp>

#include <atomic>
#include <memory>
#include <thread>

namespace void_shell {

// Forward declarations
class RemoteServer;

// =============================================================================
// Shell Events
// =============================================================================

/// @brief Event emitted when a command starts
struct CommandStartedEvent {
    SessionId session_id;
    std::string command;
    std::chrono::system_clock::time_point timestamp;
};

/// @brief Event emitted when a command completes
struct CommandCompletedEvent {
    SessionId session_id;
    std::string command;
    CommandStatus status;
    int exit_code;
    std::chrono::microseconds duration;
};

/// @brief Event emitted when a session is created
struct SessionCreatedEvent {
    SessionId session_id;
};

/// @brief Event emitted when a session is closed
struct SessionClosedEvent {
    SessionId session_id;
};

/// @brief Event emitted on shell output
struct ShellOutputEvent {
    SessionId session_id;
    std::string text;
    bool is_error;
};

// =============================================================================
// Shell System
// =============================================================================

/// @brief Main shell system
class ShellSystem {
public:
    ShellSystem();
    ~ShellSystem();

    // Singleton access
    static ShellSystem& instance();
    static ShellSystem* instance_ptr();

    // ==========================================================================
    // Initialization
    // ==========================================================================

    /// @brief Initialize the shell system
    void initialize(const ShellConfig& config = {});

    /// @brief Shutdown the shell system
    void shutdown();

    /// @brief Check if initialized
    bool is_initialized() const { return initialized_; }

    // ==========================================================================
    // Configuration
    // ==========================================================================

    const ShellConfig& config() const { return config_; }
    void set_config(const ShellConfig& config);

    // ==========================================================================
    // Session Management
    // ==========================================================================

    /// @brief Create a new session
    Session* create_session();

    /// @brief Get session by ID
    Session* get_session(SessionId id);

    /// @brief Get active session (creates one if none exists)
    Session* active_session();

    /// @brief Close a session
    void close_session(SessionId id);

    /// @brief Get all sessions
    std::vector<Session*> sessions();

    // ==========================================================================
    // Command Execution
    // ==========================================================================

    /// @brief Execute command in active session
    CommandResult execute(const std::string& input);

    /// @brief Execute command in specific session
    CommandResult execute(SessionId session_id, const std::string& input);

    /// @brief Execute silently (no output callbacks)
    CommandResult execute_silent(const std::string& input);

    // ==========================================================================
    // Command Registry
    // ==========================================================================

    CommandRegistry& commands() { return *registry_; }
    const CommandRegistry& commands() const { return *registry_; }

    /// @brief Register built-in commands
    void register_builtins();

    // ==========================================================================
    // REPL
    // ==========================================================================

    /// @brief Run interactive REPL
    void run_repl();

    /// @brief Run REPL with custom input/output
    void run_repl(std::function<std::string()> read_line,
                  OutputCallback output,
                  ErrorCallback error);

    /// @brief Stop REPL
    void stop_repl();

    /// @brief Check if REPL is running
    bool is_repl_running() const { return repl_running_.load(); }

    // ==========================================================================
    // Remote Shell
    // ==========================================================================

    /// @brief Start remote shell server
    bool start_remote_server(std::uint16_t port = 9876);

    /// @brief Stop remote shell server
    void stop_remote_server();

    /// @brief Check if remote server is running
    bool is_remote_server_running() const;

    /// @brief Get remote server port
    std::uint16_t remote_server_port() const;

    // ==========================================================================
    // Script Execution
    // ==========================================================================

    /// @brief Execute a script file
    CommandResult execute_script(const std::filesystem::path& path);

    /// @brief Execute a script string
    CommandResult execute_script_string(const std::string& script);

    // ==========================================================================
    // Hot-Reload Support
    // ==========================================================================

    /// @brief Reload commands from a module
    void reload_module_commands(const std::string& module_name);

    /// @brief Register commands for hot-reload tracking
    void track_module_commands(const std::string& module_name,
                                const std::vector<CommandId>& commands);

    // ==========================================================================
    // Event Bus Integration
    // ==========================================================================

    void set_event_bus(void_event::EventBus* bus) { event_bus_ = bus; }
    void_event::EventBus* event_bus() { return event_bus_; }

    // ==========================================================================
    // Statistics
    // ==========================================================================

    struct Stats {
        std::size_t total_sessions = 0;
        std::size_t active_sessions = 0;
        std::size_t commands_executed = 0;
        std::size_t registered_commands = 0;
        std::size_t registered_aliases = 0;
        bool remote_server_active = false;
    };

    Stats stats() const;

private:
    bool initialized_ = false;
    ShellConfig config_;

    std::unique_ptr<CommandRegistry> registry_;
    std::unique_ptr<SessionManager> session_manager_;
    std::unique_ptr<RemoteServer> remote_server_;

    void_event::EventBus* event_bus_ = nullptr;

    std::atomic<bool> repl_running_{false};

    // Event emission
    void emit_command_started(SessionId session, const std::string& command);
    void emit_command_completed(SessionId session, const std::string& command,
                                 const CommandResult& result,
                                 std::chrono::microseconds duration);
};

// =============================================================================
// Shell Builder
// =============================================================================

/// @brief Fluent builder for shell configuration
class ShellBuilder {
public:
    ShellBuilder();

    ShellBuilder& prompt(const std::string& p);
    ShellBuilder& max_history(std::size_t size);
    ShellBuilder& history_file(const std::string& path);
    ShellBuilder& save_history(bool save);
    ShellBuilder& color_output(bool color);
    ShellBuilder& command_timeout(std::chrono::milliseconds timeout);
    ShellBuilder& allow_background(bool allow);
    ShellBuilder& allow_remote(bool allow);
    ShellBuilder& remote_port(std::uint16_t port);

    /// @brief Build and initialize shell
    ShellSystem& build();

    /// @brief Get configuration
    const ShellConfig& config() const { return config_; }

private:
    ShellConfig config_;
};

// =============================================================================
// Global Shell Access
// =============================================================================

/// @brief Get global shell instance
inline ShellSystem& shell() {
    return ShellSystem::instance();
}

/// @brief Execute command in global shell
inline CommandResult shell_exec(const std::string& input) {
    return ShellSystem::instance().execute(input);
}

} // namespace void_shell
