/// @file session.cpp
/// @brief Shell session management implementation for void_shell

#include "session.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <userenv.h>
#pragma comment(lib, "userenv.lib")
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace void_shell {

// =============================================================================
// Environment Implementation
// =============================================================================

Environment::Environment() {
    // Set default variables
    set("SHELL", "void_shell");
    set("SHELL_VERSION", "1.0.0");
}

Environment::Environment(const Environment* parent) : parent_(parent) {}

std::optional<std::string> Environment::get(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }

    if (parent_) {
        return parent_->get(name);
    }

    return std::nullopt;
}

void Environment::set(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    variables_[name] = value;
}

bool Environment::unset(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return variables_.erase(name) > 0;
}

bool Environment::has(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (variables_.count(name) > 0) {
        return true;
    }

    if (parent_) {
        return parent_->has(name);
    }

    return false;
}

std::vector<std::string> Environment::keys() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;

    // Get parent keys first
    if (parent_) {
        result = parent_->keys();
    }

    // Add local keys
    for (const auto& [key, value] : variables_) {
        if (std::find(result.begin(), result.end(), key) == result.end()) {
            result.push_back(key);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::unordered_map<std::string, std::string> Environment::all() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::unordered_map<std::string, std::string> result;

    // Get parent variables first
    if (parent_) {
        result = parent_->all();
    }

    // Override with local variables
    for (const auto& [key, value] : variables_) {
        result[key] = value;
    }

    return result;
}

void Environment::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    variables_.clear();
}

void Environment::import_system_env() {
    std::lock_guard<std::mutex> lock(mutex_);

#ifdef _WIN32
    // Windows environment
    LPWCH env_strings = GetEnvironmentStringsW();
    if (env_strings) {
        LPWCH current = env_strings;
        while (*current) {
            std::wstring ws(current);
            std::size_t eq_pos = ws.find(L'=');
            if (eq_pos != std::wstring::npos && eq_pos > 0) {
                std::wstring name = ws.substr(0, eq_pos);
                std::wstring value = ws.substr(eq_pos + 1);

                // Convert to UTF-8
                int name_size = WideCharToMultiByte(CP_UTF8, 0, name.c_str(), -1, nullptr, 0, nullptr, nullptr);
                int value_size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);

                std::string name_str(name_size - 1, 0);
                std::string value_str(value_size - 1, 0);

                WideCharToMultiByte(CP_UTF8, 0, name.c_str(), -1, name_str.data(), name_size, nullptr, nullptr);
                WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, value_str.data(), value_size, nullptr, nullptr);

                variables_[name_str] = value_str;
            }
            current += ws.size() + 1;
        }
        FreeEnvironmentStringsW(env_strings);
    }
#else
    // Unix environment
    extern char** environ;
    for (char** env = environ; *env; ++env) {
        std::string entry(*env);
        std::size_t eq_pos = entry.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = entry.substr(0, eq_pos);
            std::string value = entry.substr(eq_pos + 1);
            variables_[name] = value;
        }
    }
#endif
}

void Environment::export_to_system(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = variables_.find(name);
    if (it == variables_.end()) {
        return;
    }

#ifdef _WIN32
    // Convert to wide string
    int name_size = MultiByteToWideChar(CP_UTF8, 0, it->first.c_str(), -1, nullptr, 0);
    int value_size = MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(), -1, nullptr, 0);

    std::wstring name_wide(name_size - 1, 0);
    std::wstring value_wide(value_size - 1, 0);

    MultiByteToWideChar(CP_UTF8, 0, it->first.c_str(), -1, name_wide.data(), name_size);
    MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(), -1, value_wide.data(), value_size);

    SetEnvironmentVariableW(name_wide.c_str(), value_wide.c_str());
#else
    setenv(it->first.c_str(), it->second.c_str(), 1);
#endif
}

std::string Environment::expand(const std::string& str) const {
    std::string result;
    std::size_t i = 0;

    while (i < str.size()) {
        if (str[i] == '$') {
            i++;
            if (i >= str.size()) {
                result += '$';
                break;
            }

            std::string var_name;

            if (str[i] == '{') {
                // ${variable}
                i++;
                while (i < str.size() && str[i] != '}') {
                    var_name += str[i++];
                }
                if (i < str.size()) i++; // Skip }
            } else {
                // $variable
                while (i < str.size() && (std::isalnum(str[i]) || str[i] == '_')) {
                    var_name += str[i++];
                }
            }

            auto value = get(var_name);
            if (value) {
                result += *value;
            }
        } else if (str[i] == '\\' && i + 1 < str.size()) {
            i++;
            result += str[i++];
        } else {
            result += str[i++];
        }
    }

    return result;
}

std::string Environment::pwd() const {
    auto val = get("PWD");
    if (val) return *val;

    // Get from system
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    if (GetCurrentDirectoryW(MAX_PATH, buffer)) {
        int size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, buffer, -1, result.data(), size, nullptr, nullptr);
        return result;
    }
#else
    char buffer[PATH_MAX];
    if (getcwd(buffer, sizeof(buffer))) {
        return buffer;
    }
#endif

    return ".";
}

void Environment::set_pwd(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    variables_["PWD"] = path;
}

std::string Environment::home() const {
    auto val = get("HOME");
    if (val) return *val;

#ifdef _WIN32
    // Try USERPROFILE
    val = get("USERPROFILE");
    if (val) return *val;

    // Get from system
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        wchar_t buffer[MAX_PATH];
        DWORD size = MAX_PATH;
        if (GetUserProfileDirectoryW(token, buffer, &size)) {
            CloseHandle(token);
            int str_size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
            std::string result(str_size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, result.data(), str_size, nullptr, nullptr);
            return result;
        }
        CloseHandle(token);
    }
    return "C:\\Users\\Default";
#else
    // Get from passwd
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
        return pw->pw_dir;
    }
    return "/";
#endif
}

std::string Environment::user() const {
    auto val = get("USER");
    if (val) return *val;

    val = get("USERNAME");
    if (val) return *val;

#ifdef _WIN32
    wchar_t buffer[256];
    DWORD size = 256;
    if (GetUserNameW(buffer, &size)) {
        int str_size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
        std::string result(str_size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, buffer, -1, result.data(), str_size, nullptr, nullptr);
        return result;
    }
    return "user";
#else
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
        return pw->pw_name;
    }
    return "user";
#endif
}

// =============================================================================
// History Implementation
// =============================================================================

History::History() : max_size_(1000), next_index_(1) {}

History::History(std::size_t max_size) : max_size_(max_size), next_index_(1) {}

void History::add(const std::string& command, CommandStatus status,
                  int exit_code, std::chrono::microseconds duration) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Skip empty commands
    if (command.empty()) {
        return;
    }

    // Skip duplicates of last command
    if (!entries_.empty() && entries_.back().command == command) {
        return;
    }

    HistoryEntry entry;
    entry.index = next_index_++;
    entry.command = command;
    entry.timestamp = std::chrono::system_clock::now();
    entry.status = status;
    entry.exit_code = exit_code;
    entry.duration = duration;

    entries_.push_back(std::move(entry));

    // Trim to max size
    while (entries_.size() > max_size_) {
        entries_.pop_front();
    }
}

const HistoryEntry* History::get(std::size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (index >= entries_.size()) {
        return nullptr;
    }

    return &entries_[entries_.size() - 1 - index];
}

const HistoryEntry* History::get_absolute(std::size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& entry : entries_) {
        if (entry.index == index) {
            return &entry;
        }
    }

    return nullptr;
}

std::vector<const HistoryEntry*> History::search(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<const HistoryEntry*> results;

    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->command.find(query) != std::string::npos) {
            results.push_back(&(*it));
        }
    }

    return results;
}

std::vector<const HistoryEntry*> History::search_prefix(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<const HistoryEntry*> results;

    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->command.size() >= prefix.size() &&
            it->command.compare(0, prefix.size(), prefix) == 0) {
            results.push_back(&(*it));
        }
    }

    return results;
}

void History::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

void History::set_max_size(std::size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_size_ = size;

    while (entries_.size() > max_size_) {
        entries_.pop_front();
    }
}

bool History::load(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(path);
    if (!file) {
        return false;
    }

    entries_.clear();

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Format: timestamp|status|exit_code|duration|command
        std::istringstream iss(line);
        std::string timestamp_str, status_str, exit_code_str, duration_str, command;

        if (std::getline(iss, timestamp_str, '|') &&
            std::getline(iss, status_str, '|') &&
            std::getline(iss, exit_code_str, '|') &&
            std::getline(iss, duration_str, '|') &&
            std::getline(iss, command)) {

            HistoryEntry entry;
            entry.index = next_index_++;
            entry.command = command;

            try {
                auto time_point = std::stoll(timestamp_str);
                entry.timestamp = std::chrono::system_clock::time_point(
                    std::chrono::seconds(time_point));
                entry.status = static_cast<CommandStatus>(std::stoi(status_str));
                entry.exit_code = std::stoi(exit_code_str);
                entry.duration = std::chrono::microseconds(std::stoll(duration_str));
            } catch (...) {
                entry.timestamp = std::chrono::system_clock::now();
                entry.status = CommandStatus::Success;
                entry.exit_code = 0;
            }

            entries_.push_back(std::move(entry));

            while (entries_.size() > max_size_) {
                entries_.pop_front();
            }
        }
    }

    return true;
}

bool History::save(const std::filesystem::path& path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << "# void_shell history\n";
    file << "# Format: timestamp|status|exit_code|duration_us|command\n";

    for (const auto& entry : entries_) {
        auto time_point = std::chrono::duration_cast<std::chrono::seconds>(
            entry.timestamp.time_since_epoch()).count();

        file << time_point << '|'
             << static_cast<int>(entry.status) << '|'
             << entry.exit_code << '|'
             << entry.duration.count() << '|'
             << entry.command << '\n';
    }

    return true;
}

// =============================================================================
// Session Implementation
// =============================================================================

Session::Session(SessionId id, const ShellConfig& config)
    : id_(id), config_(config) {

    // Initialize working directory
    cwd_ = std::filesystem::current_path();
    env_.set_pwd(cwd_.string());

    // Import system environment
    env_.import_system_env();

    // Set session-specific variables
    env_.set("SESSION_ID", std::to_string(id.value));

    // Initialize history
    history_.set_max_size(config.max_history_size);

    // Set creation time
    stats_.created_at = std::chrono::system_clock::now();

    state_ = SessionState::Active;
}

Session::~Session() {
    // Cancel any running execution
    cancel();

    // Wait for and cleanup background jobs
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        for (auto& [job_id, job] : jobs_) {
            job->cancel();
            if (job->thread.joinable()) {
                job->thread.join();
            }
        }
        jobs_.clear();
    }

    state_ = SessionState::Closed;
}

Session::Session(Session&& other) noexcept
    : id_(other.id_),
      state_(other.state_),
      config_(std::move(other.config_)),
      cwd_(std::move(other.cwd_)),
      output_callback_(std::move(other.output_callback_)),
      error_callback_(std::move(other.error_callback_)),
      prompt_callback_(std::move(other.prompt_callback_)),
      completion_callback_(std::move(other.completion_callback_)),
      cancelled_(other.cancelled_.load()),
      last_result_(std::move(other.last_result_)),
      next_job_id_(other.next_job_id_),
      stats_(other.stats_),
      parser_(std::move(other.parser_)),
      registry_(other.registry_) {

    // Note: env_ and history_ have mutexes and can't be moved directly
    // They are default-constructed and we don't transfer state

    std::lock_guard<std::mutex> lock(other.jobs_mutex_);
    jobs_ = std::move(other.jobs_);
}

Session& Session::operator=(Session&& other) noexcept {
    if (this != &other) {
        id_ = other.id_;
        state_ = other.state_;
        config_ = std::move(other.config_);
        // Note: env_ and history_ have mutexes and can't be moved directly
        cwd_ = std::move(other.cwd_);
        output_callback_ = std::move(other.output_callback_);
        error_callback_ = std::move(other.error_callback_);
        prompt_callback_ = std::move(other.prompt_callback_);
        completion_callback_ = std::move(other.completion_callback_);
        cancelled_.store(other.cancelled_.load());
        last_result_ = std::move(other.last_result_);
        next_job_id_ = other.next_job_id_;
        stats_ = other.stats_;
        parser_ = std::move(other.parser_);
        registry_ = other.registry_;

        std::lock_guard<std::mutex> lock(other.jobs_mutex_);
        jobs_ = std::move(other.jobs_);
    }
    return *this;
}

CommandResult Session::execute(const std::string& input) {
    auto start_time = std::chrono::steady_clock::now();

    // Parse the input
    parser_.set_alias_resolver([this](const std::string& name) -> std::optional<std::string> {
        if (registry_) {
            return registry_->get_alias(name);
        }
        return std::nullopt;
    });

    parser_.set_variable_resolver([this](const std::string& name) -> std::optional<std::string> {
        return env_.get(name);
    });

    auto line_result = parser_.parse(input);
    if (!line_result) {
        return CommandResult::error("Parse error: " + parser_.error_message());
    }

    const auto& line = line_result.value();

    // Execute each command in the line
    CommandResult result;
    result.status = CommandStatus::Success;

    for (std::size_t i = 0; i < line.commands.size(); ++i) {
        const auto& cmd = line.commands[i];

        // Check command chaining conditions using connectors
        if (i > 0 && i - 1 < line.connectors.size()) {
            auto connector = line.connectors[i - 1];
            if (connector == CommandLine::Connector::And && last_result_.exit_code != 0) {
                continue;
            }
            if (connector == CommandLine::Connector::Or && last_result_.exit_code == 0) {
                continue;
            }
        }

        // Execute in background if requested
        if (cmd.background) {
            auto job_id = execute_background(input);
            result.output = "Started background job [" + std::to_string(job_id) + "]";
            continue;
        }

        result = execute(cmd);
        last_result_ = result;

        // Update special variables
        env_.set("?", std::to_string(result.exit_code));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    // Add to history
    history_.add(input, result.status, result.exit_code, duration);

    // Update stats
    update_stats(result, duration);

    return result;
}

CommandResult Session::execute(const ParsedCommand& cmd) {
    if (cmd.name.empty()) {
        return CommandResult::success("");
    }

    state_ = SessionState::Executing;
    cancelled_.store(false);

    // Check for pipeline
    if (cmd.pipe_to) {
        return execute_pipeline(cmd);
    }

    return execute_internal(cmd);
}

std::vector<CommandResult> Session::execute_line(const CommandLine& line) {
    std::vector<CommandResult> results;

    for (std::size_t i = 0; i < line.commands.size(); ++i) {
        const auto& cmd = line.commands[i];
        results.push_back(execute(cmd));

        // Handle chaining using connectors
        if (!results.empty() && i < line.connectors.size()) {
            const auto& last = results.back();
            auto connector = line.connectors[i];
            if (connector == CommandLine::Connector::And && last.exit_code != 0) {
                break;
            }
            if (connector == CommandLine::Connector::Or && last.exit_code == 0) {
                break;
            }
        }
    }

    state_ = SessionState::Active;
    return results;
}

std::uint32_t Session::execute_background(const std::string& input) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    auto job = std::make_unique<BackgroundJob>();
    job->job_id = next_job_id_++;
    job->command = input;
    job->started_at = std::chrono::system_clock::now();

    std::uint32_t job_id = job->job_id;

    // Create a copy of necessary data for the thread
    auto* job_ptr = job.get();

    job->thread = std::thread([this, job_ptr, input]() {
        // Execute the command
        job_ptr->result = execute(input);
        job_ptr->running.store(false);
    });

    jobs_[job_id] = std::move(job);
    return job_id;
}

void Session::cancel() {
    cancelled_.store(true);
}

CommandResult Session::execute_internal(const ParsedCommand& cmd) {
    if (!registry_) {
        return CommandResult::error("No command registry available");
    }

    // Find the command
    auto* command = registry_->find(cmd.name);
    if (!command) {
        return CommandResult::error("Unknown command: " + cmd.name);
    }

    // The cmd.args is already a CommandArgs object, use it directly
    // But we need a mutable copy for validation
    CommandArgs args = cmd.args;

    // Create context
    CommandContext ctx;
    ctx.cwd = cwd_;
    ctx.env = &env_;
    ctx.registry = registry_;
    ctx.session_id = id_;

    // Set up I/O callbacks
    ctx.output = [this](const std::string& text) {
        print(text);
    };
    ctx.error = [this](const std::string& text) {
        print_error(text);
    };

    // Handle redirections from cmd.redirects
    std::ofstream redirect_file;
    std::string stdin_content;

    for (const auto& redirect : cmd.redirects) {
        if (redirect.type == Redirect::Type::Output || redirect.type == Redirect::Type::Append) {
            auto mode = (redirect.type == Redirect::Type::Append) ? std::ios::app : std::ios::trunc;
            redirect_file.open(redirect.target, mode);
            if (redirect_file) {
                ctx.output = [&redirect_file](const std::string& text) {
                    redirect_file << text;
                };
            }
        } else if (redirect.type == Redirect::Type::Input) {
            std::ifstream input_file(redirect.target);
            if (input_file) {
                std::ostringstream ss;
                ss << input_file.rdbuf();
                stdin_content = ss.str();
                ctx.stdin_content = stdin_content;
            }
        }
    }

    // Validate arguments
    std::string validation_error;
    if (!command->validate(args, validation_error)) {
        return CommandResult::error(validation_error);
    }

    // Execute the command
    CommandResult result;
    try {
        result = command->execute(args, ctx);
    } catch (const std::exception& e) {
        result = CommandResult::error(std::string("Exception: ") + e.what());
    }

    state_ = SessionState::Active;
    return result;
}

CommandResult Session::execute_pipeline(const ParsedCommand& cmd) {
    // For pipelines, execute each command and pipe output to next
    std::string current_input;
    CommandResult result;

    // Execute first command
    result = execute_internal(cmd);
    current_input = result.output;

    // Execute pipeline commands by following pipe_to chain
    const ParsedCommand* pipe_cmd = cmd.pipe_to.get();
    while (pipe_cmd) {
        // Create context with piped input
        CommandContext ctx;
        ctx.cwd = cwd_;
        ctx.env = &env_;
        ctx.registry = registry_;
        ctx.session_id = id_;
        ctx.stdin_content = current_input;

        std::ostringstream output;
        ctx.output = [&output](const std::string& text) {
            output << text;
        };
        ctx.error = [this](const std::string& text) {
            print_error(text);
        };

        // Find and execute command
        auto* command = registry_->find(pipe_cmd->name);
        if (!command) {
            return CommandResult::error("Unknown command in pipeline: " + pipe_cmd->name);
        }

        // Use the args directly from pipe_cmd
        CommandArgs args = pipe_cmd->args;

        result = command->execute(args, ctx);
        current_input = output.str();

        if (result.status != CommandStatus::Success) {
            break;
        }

        // Move to next command in pipeline
        pipe_cmd = pipe_cmd->pipe_to.get();
    }

    result.output = current_input;
    return result;
}

void Session::update_stats(const CommandResult& result, std::chrono::microseconds duration) {
    stats_.commands_executed++;
    stats_.total_execution_time += duration;
    stats_.last_command_at = std::chrono::system_clock::now();

    if (result.status == CommandStatus::Success) {
        stats_.commands_succeeded++;
    } else {
        stats_.commands_failed++;
    }
}

bool Session::set_cwd(const std::filesystem::path& path) {
    std::filesystem::path new_path;

    if (path.is_absolute()) {
        new_path = path;
    } else {
        new_path = cwd_ / path;
    }

    // Normalize the path
    new_path = std::filesystem::weakly_canonical(new_path);

    if (!std::filesystem::exists(new_path)) {
        return false;
    }

    if (!std::filesystem::is_directory(new_path)) {
        return false;
    }

    cwd_ = new_path;
    env_.set_pwd(cwd_.string());

    return true;
}

void Session::print(const std::string& text) {
    if (output_callback_) {
        output_callback_(text);
    }
}

void Session::println(const std::string& text) {
    print(text + "\n");
}

void Session::print_error(const std::string& text) {
    if (error_callback_) {
        error_callback_(text);
    } else if (output_callback_) {
        output_callback_("Error: " + text);
    }
}

std::string Session::get_prompt() const {
    if (prompt_callback_) {
        return prompt_callback_();
    }

    // Expand prompt string
    std::string prompt = config_.prompt;

    // Replace common placeholders
    std::string result;
    for (std::size_t i = 0; i < prompt.size(); ++i) {
        if (prompt[i] == '\\' && i + 1 < prompt.size()) {
            switch (prompt[i + 1]) {
                case 'u': result += env_.user(); break;
                case 'h': {
                    auto host = env_.get("HOSTNAME");
                    result += host.value_or("localhost");
                    break;
                }
                case 'w': result += cwd_.string(); break;
                case 'W': result += cwd_.filename().string(); break;
                case '$': result += (env_.user() == "root" ? "#" : "$"); break;
                case 'n': result += "\n"; break;
                default: result += prompt[i + 1]; break;
            }
            i++;
        } else {
            result += prompt[i];
        }
    }

    return result;
}

std::vector<std::string> Session::complete(const std::string& input, std::size_t cursor_pos) {
    std::vector<std::string> completions;

    // Parse up to cursor position
    std::string partial = input.substr(0, cursor_pos);

    // Find the last word
    std::size_t last_space = partial.rfind(' ');
    std::string word = last_space != std::string::npos
        ? partial.substr(last_space + 1)
        : partial;

    // If we're completing the first word, complete command names
    if (last_space == std::string::npos) {
        if (registry_) {
            completions = registry_->complete_command(word);
        }
    } else {
        // Get the command name
        std::string cmd_name = partial.substr(0, partial.find(' '));

        // If it's a flag, complete flags
        if (!word.empty() && word[0] == '-') {
            // TODO: Complete flags from command info
        }
        // Otherwise complete based on command's completion function
        else if (registry_) {
            CommandArgs args;
            // Parse existing arguments to determine position
            auto line_result = parser_.parse(partial);
            if (line_result && !line_result.value().commands.empty()) {
                // Copy positional args from parsed command
                const auto& parsed_args = line_result.value().commands[0].args;
                for (const auto& arg : parsed_args.positional()) {
                    args.add_positional(arg.value);
                }
            }

            CommandContext ctx;
            ctx.cwd = cwd_;
            ctx.env = const_cast<Environment*>(&env_);
            ctx.registry = registry_;

            std::size_t arg_index = args.positional().size();
            completions = registry_->complete_argument(cmd_name, args, arg_index, word, ctx);
        }
    }

    // Custom completion callback
    if (completion_callback_) {
        auto custom = completion_callback_(input, cursor_pos);
        completions.insert(completions.end(), custom.begin(), custom.end());
    }

    // Sort and remove duplicates
    std::sort(completions.begin(), completions.end());
    completions.erase(std::unique(completions.begin(), completions.end()), completions.end());

    return completions;
}

std::vector<const BackgroundJob*> Session::jobs() const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    std::vector<const BackgroundJob*> result;
    for (const auto& [id, job] : jobs_) {
        result.push_back(job.get());
    }
    return result;
}

BackgroundJob* Session::get_job(std::uint32_t job_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    auto it = jobs_.find(job_id);
    return it != jobs_.end() ? it->second.get() : nullptr;
}

bool Session::cancel_job(std::uint32_t job_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    auto it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        return false;
    }

    it->second->cancel();
    return true;
}

bool Session::wait_job(std::uint32_t job_id, std::chrono::milliseconds timeout) {
    BackgroundJob* job = nullptr;

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return false;
        }
        job = it->second.get();
    }

    if (timeout.count() == 0) {
        // Wait indefinitely
        if (job->thread.joinable()) {
            job->thread.join();
        }
    } else {
        // Wait with timeout (poll-based)
        auto start = std::chrono::steady_clock::now();
        while (job->running.load()) {
            auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return true;
}

void Session::cleanup_finished_jobs() {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    for (auto it = jobs_.begin(); it != jobs_.end();) {
        if (it->second->is_done()) {
            if (it->second->thread.joinable()) {
                it->second->thread.join();
            }
            it = jobs_.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// SessionManager Implementation
// =============================================================================

SessionManager::SessionManager() : active_session_id_{0}, next_session_id_(1) {}

SessionManager::~SessionManager() {
    close_all_sessions();
}

SessionManager& SessionManager::instance() {
    static SessionManager instance;
    return instance;
}

Session* SessionManager::create_session(const ShellConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    SessionId id{next_session_id_++};
    auto session = std::make_unique<Session>(id, config);

    Session* ptr = session.get();
    sessions_[id] = std::move(session);

    if (!active_session_id_) {
        active_session_id_ = id;
    }

    if (on_created_) {
        on_created_(*ptr);
    }

    return ptr;
}

Session* SessionManager::get_session(SessionId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(id);
    return it != sessions_.end() ? it->second.get() : nullptr;
}

const Session* SessionManager::get_session(SessionId id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(id);
    return it != sessions_.end() ? it->second.get() : nullptr;
}

Session* SessionManager::active_session() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!active_session_id_) {
        return nullptr;
    }

    auto it = sessions_.find(active_session_id_);
    return it != sessions_.end() ? it->second.get() : nullptr;
}

void SessionManager::set_active_session(SessionId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessions_.count(id) > 0) {
        active_session_id_ = id;
    }
}

bool SessionManager::close_session(SessionId id) {
    std::unique_ptr<Session> session;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(id);
        if (it == sessions_.end()) {
            return false;
        }

        session = std::move(it->second);
        sessions_.erase(it);

        if (active_session_id_ == id) {
            active_session_id_ = sessions_.empty() ? SessionId{0} : sessions_.begin()->first;
        }
    }

    if (on_closed_ && session) {
        on_closed_(*session);
    }

    return true;
}

void SessionManager::close_all_sessions() {
    std::vector<std::unique_ptr<Session>> to_close;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [id, session] : sessions_) {
            to_close.push_back(std::move(session));
        }
        sessions_.clear();
        active_session_id_ = SessionId{0};
    }

    if (on_closed_) {
        for (auto& session : to_close) {
            on_closed_(*session);
        }
    }
}

std::vector<Session*> SessionManager::sessions() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Session*> result;
    result.reserve(sessions_.size());

    for (auto& [id, session] : sessions_) {
        result.push_back(session.get());
    }

    return result;
}

std::size_t SessionManager::session_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

} // namespace void_shell
