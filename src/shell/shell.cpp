/// @file shell.cpp
/// @brief Main shell system implementation for void_shell

#include "shell.hpp"
#include "builtins.hpp"
#include "remote.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace void_shell {

// =============================================================================
// ShellSystem Implementation
// =============================================================================

static ShellSystem* g_instance = nullptr;

ShellSystem::ShellSystem()
    : registry_(std::make_unique<CommandRegistry>()),
      session_manager_(std::make_unique<SessionManager>()) {
    g_instance = this;
}

ShellSystem::~ShellSystem() {
    shutdown();
    if (g_instance == this) {
        g_instance = nullptr;
    }
}

ShellSystem& ShellSystem::instance() {
    if (!g_instance) {
        static ShellSystem static_instance;
        g_instance = &static_instance;
    }
    return *g_instance;
}

ShellSystem* ShellSystem::instance_ptr() {
    return g_instance;
}

void ShellSystem::initialize(const ShellConfig& config) {
    if (initialized_) {
        return;
    }

    config_ = config;

    // Register built-in commands
    register_builtins();

    // Create initial session
    auto* session = session_manager_->create_session(config);
    if (session) {
        // Load history
        if (!config.history_file.empty()) {
            session->history().load(config.history_file);
        }
    }

    // Start remote server if configured
    if (config.allow_remote) {
        start_remote_server(config.remote_port);
    }

    initialized_ = true;
}

void ShellSystem::shutdown() {
    if (!initialized_) {
        return;
    }

    // Stop REPL
    stop_repl();

    // Stop remote server
    stop_remote_server();

    // Save history for all sessions
    for (auto* session : session_manager_->sessions()) {
        if (!config_.history_file.empty() && config_.save_history) {
            session->history().save(config_.history_file);
        }
    }

    // Close all sessions
    session_manager_->close_all_sessions();

    initialized_ = false;
}

void ShellSystem::set_config(const ShellConfig& config) {
    config_ = config;

    // Apply to existing sessions
    for (auto* session : session_manager_->sessions()) {
        session->history().set_max_size(config.max_history_size);
    }
}

Session* ShellSystem::create_session() {
    return session_manager_->create_session(config_);
}

Session* ShellSystem::get_session(SessionId id) {
    return session_manager_->get_session(id);
}

Session* ShellSystem::active_session() {
    auto* session = session_manager_->active_session();
    if (!session) {
        session = create_session();
    }
    return session;
}

void ShellSystem::close_session(SessionId id) {
    auto* session = session_manager_->get_session(id);
    if (session && !config_.history_file.empty() && config_.save_history) {
        session->history().save(config_.history_file);
    }
    session_manager_->close_session(id);
}

std::vector<Session*> ShellSystem::sessions() {
    return session_manager_->sessions();
}

CommandResult ShellSystem::execute(const std::string& input) {
    auto* session = active_session();
    if (!session) {
        return CommandResult::error("No active session");
    }

    emit_command_started(session->id(), input);
    auto start = std::chrono::steady_clock::now();

    auto result = session->execute(input);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    emit_command_completed(session->id(), input, result, duration);

    return result;
}

CommandResult ShellSystem::execute(SessionId session_id, const std::string& input) {
    auto* session = session_manager_->get_session(session_id);
    if (!session) {
        return CommandResult::error("Session not found");
    }

    emit_command_started(session_id, input);
    auto start = std::chrono::steady_clock::now();

    auto result = session->execute(input);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    emit_command_completed(session_id, input, result, duration);

    return result;
}

CommandResult ShellSystem::execute_silent(const std::string& input) {
    auto* session = active_session();
    if (!session) {
        return CommandResult::error("No active session");
    }

    // Save callbacks
    auto saved_output = session->config().color_output;

    // Disable output
    session->set_output_callback(nullptr);
    session->set_error_callback(nullptr);

    auto result = session->execute(input);

    return result;
}

void ShellSystem::register_builtins() {
    builtins::register_all(*registry_);
}

void ShellSystem::run_repl() {
    run_repl(
        []() -> std::string {
            std::string line;
            std::getline(std::cin, line);
            return line;
        },
        [](const std::string& text) {
            std::cout << text;
            std::cout.flush();
        },
        [](const std::string& text) {
            std::cerr << text;
            std::cerr.flush();
        }
    );
}

void ShellSystem::run_repl(std::function<std::string()> read_line,
                           OutputCallback output,
                           ErrorCallback error) {
    if (repl_running_.load()) {
        return;
    }

    repl_running_.store(true);

    auto* session = active_session();
    if (!session) {
        session = create_session();
    }

    session->set_output_callback(output);
    session->set_error_callback(error);

    while (repl_running_.load()) {
        // Print prompt
        output(session->get_prompt());

        // Read input
        std::string input;
        try {
            input = read_line();
        } catch (...) {
            break;
        }

        // Check for exit
        if (!repl_running_.load()) {
            break;
        }

        // Skip empty lines
        if (input.empty()) {
            continue;
        }

        // Execute command
        auto result = execute(input);

        // Handle exit status
        if (result.status == CommandStatus::Cancelled) {
            break;
        }
    }

    repl_running_.store(false);
}

void ShellSystem::stop_repl() {
    repl_running_.store(false);
}

bool ShellSystem::start_remote_server(std::uint16_t port) {
    if (!config_.allow_remote) {
        return false;
    }

    if (!remote_server_) {
        remote_server_ = std::make_unique<RemoteServer>();
    }

    if (remote_server_->is_running()) {
        return true;
    }

    // Set up callbacks
    remote_server_->set_command_callback(
        [this](ConnectionId conn_id, const std::string& command) -> CommandResult {
            return execute(command);
        }
    );

    return remote_server_->start(port);
}

void ShellSystem::stop_remote_server() {
    if (remote_server_) {
        remote_server_->stop();
    }
}

bool ShellSystem::is_remote_server_running() const {
    return remote_server_ && remote_server_->is_running();
}

std::uint16_t ShellSystem::remote_server_port() const {
    return remote_server_ ? remote_server_->port() : 0;
}

CommandResult ShellSystem::execute_script(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        return CommandResult::error("Cannot open script file: " + path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return execute_script_string(ss.str());
}

CommandResult ShellSystem::execute_script_string(const std::string& script) {
    auto* session = active_session();
    if (!session) {
        return CommandResult::error("No active session");
    }

    CommandResult last_result;
    last_result.status = CommandStatus::Success;

    std::istringstream stream(script);
    std::string line;
    int line_number = 0;

    while (std::getline(stream, line)) {
        line_number++;

        // Skip empty lines and comments
        std::size_t first_non_space = line.find_first_not_of(" \t");
        if (first_non_space == std::string::npos) {
            continue;
        }
        if (line[first_non_space] == '#') {
            continue;
        }

        // Handle line continuations
        while (!line.empty() && line.back() == '\\') {
            line.pop_back();
            std::string next_line;
            if (std::getline(stream, next_line)) {
                line_number++;
                line += next_line;
            } else {
                break;
            }
        }

        last_result = session->execute(line);

        if (last_result.status != CommandStatus::Success) {
            last_result.error_message = "Error at line " + std::to_string(line_number) +
                                ": " + last_result.error_message;
            break;
        }
    }

    return last_result;
}

void ShellSystem::reload_module_commands(const std::string& module_name) {
    // Unregister old commands
    registry_->unregister_module_commands(module_name);

    // The module system should re-register its commands after this
    // This is typically called after a hot-reload completes
}

void ShellSystem::track_module_commands(const std::string& module_name,
                                         const std::vector<CommandId>& commands) {
    registry_->mark_module_commands(module_name, commands);
}

ShellSystem::Stats ShellSystem::stats() const {
    Stats s;

    s.total_sessions = session_manager_->session_count();

    for (auto* session : session_manager_->sessions()) {
        if (session->state() == SessionState::Active ||
            session->state() == SessionState::Executing) {
            s.active_sessions++;
        }
        s.commands_executed += session->stats().commands_executed;
    }

    s.registered_commands = registry_->count();
    s.registered_aliases = registry_->alias_count();
    s.remote_server_active = is_remote_server_running();

    return s;
}

void ShellSystem::emit_command_started(SessionId session, const std::string& command) {
    if (event_bus_) {
        CommandStartedEvent event;
        event.session_id = session;
        event.command = command;
        event.timestamp = std::chrono::system_clock::now();
        event_bus_->publish(event);
    }
}

void ShellSystem::emit_command_completed(SessionId session, const std::string& command,
                                          const CommandResult& result,
                                          std::chrono::microseconds duration) {
    if (event_bus_) {
        CommandCompletedEvent event;
        event.session_id = session;
        event.command = command;
        event.status = result.status;
        event.exit_code = result.exit_code;
        event.duration = duration;
        event_bus_->publish(event);
    }
}

// =============================================================================
// ShellBuilder Implementation
// =============================================================================

ShellBuilder::ShellBuilder() {
    // Default configuration
    config_.prompt = "\\u@void:\\w$ ";
    config_.max_history_size = 1000;
    config_.save_history = true;
    config_.color_output = true;
    config_.command_timeout = std::chrono::milliseconds(30000);
    config_.allow_background = true;
    config_.allow_remote = false;
    config_.remote_port = 9876;
}

ShellBuilder& ShellBuilder::prompt(const std::string& p) {
    config_.prompt = p;
    return *this;
}

ShellBuilder& ShellBuilder::max_history(std::size_t size) {
    config_.max_history_size = size;
    return *this;
}

ShellBuilder& ShellBuilder::history_file(const std::string& path) {
    config_.history_file = path;
    return *this;
}

ShellBuilder& ShellBuilder::save_history(bool save) {
    config_.save_history = save;
    return *this;
}

ShellBuilder& ShellBuilder::color_output(bool color) {
    config_.color_output = color;
    return *this;
}

ShellBuilder& ShellBuilder::command_timeout(std::chrono::milliseconds timeout) {
    config_.command_timeout = timeout;
    return *this;
}

ShellBuilder& ShellBuilder::allow_background(bool allow) {
    config_.allow_background = allow;
    return *this;
}

ShellBuilder& ShellBuilder::allow_remote(bool allow) {
    config_.allow_remote = allow;
    return *this;
}

ShellBuilder& ShellBuilder::remote_port(std::uint16_t port) {
    config_.remote_port = port;
    return *this;
}

ShellSystem& ShellBuilder::build() {
    auto& shell = ShellSystem::instance();
    shell.initialize(config_);
    return shell;
}

} // namespace void_shell
