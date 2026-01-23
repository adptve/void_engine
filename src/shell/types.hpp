#pragma once

/// @file types.hpp
/// @brief Core types for void_shell

#include "fwd.hpp"

#include <any>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace void_shell {

// =============================================================================
// Token Types
// =============================================================================

/// @brief Token type enumeration
enum class TokenType {
    // Literals
    Identifier,
    String,
    Integer,
    Float,
    Boolean,

    // Operators
    Pipe,           // |
    Redirect,       // >
    RedirectAppend, // >>
    RedirectInput,  // <
    And,            // &&
    Or,             // ||
    Semicolon,      // ;
    Ampersand,      // &

    // Delimiters
    LeftParen,      // (
    RightParen,     // )
    LeftBrace,      // {
    RightBrace,     // }
    LeftBracket,    // [
    RightBracket,   // ]

    // Special
    Variable,       // $var
    Equals,         // =
    Colon,          // :
    Comma,          // ,
    Dot,            // .
    Flag,           // --flag or -f
    Newline,
    Eof,
    Error
};

/// @brief Token structure
struct Token {
    TokenType type = TokenType::Error;
    std::string value;
    std::size_t line = 0;
    std::size_t column = 0;

    bool is_literal() const {
        return type == TokenType::Identifier ||
               type == TokenType::String ||
               type == TokenType::Integer ||
               type == TokenType::Float ||
               type == TokenType::Boolean;
    }

    bool is_operator() const {
        return type == TokenType::Pipe ||
               type == TokenType::Redirect ||
               type == TokenType::RedirectAppend ||
               type == TokenType::RedirectInput ||
               type == TokenType::And ||
               type == TokenType::Or ||
               type == TokenType::Semicolon ||
               type == TokenType::Ampersand;
    }
};

// =============================================================================
// Argument Types
// =============================================================================

/// @brief Argument type enumeration
enum class ArgType {
    String,
    Integer,
    Float,
    Boolean,
    Path,
    EntityId,
    Any
};

/// @brief Argument value variant
using ArgValue = std::variant<
    std::monostate,
    std::string,
    std::int64_t,
    double,
    bool,
    std::vector<std::string>
>;

/// @brief Command argument specification
struct ArgSpec {
    std::string name;
    std::string short_name;  // Single character for -x style
    std::string description;
    ArgType type = ArgType::String;
    bool required = false;
    bool positional = false;
    bool variadic = false;    // Can accept multiple values
    ArgValue default_value;

    static ArgSpec positional_arg(const std::string& name, ArgType type = ArgType::String) {
        ArgSpec spec;
        spec.name = name;
        spec.type = type;
        spec.required = true;
        spec.positional = true;
        return spec;
    }

    static ArgSpec optional_arg(const std::string& name, const std::string& short_name = "",
                                 ArgType type = ArgType::String) {
        ArgSpec spec;
        spec.name = name;
        spec.short_name = short_name;
        spec.type = type;
        spec.required = false;
        spec.positional = false;
        return spec;
    }

    static ArgSpec flag(const std::string& name, const std::string& short_name = "") {
        ArgSpec spec;
        spec.name = name;
        spec.short_name = short_name;
        spec.type = ArgType::Boolean;
        spec.default_value = false;
        return spec;
    }
};

/// @brief Parsed command argument
struct CommandArg {
    std::string name;
    ArgValue value;
    bool is_flag = false;

    std::string as_string() const;
    std::int64_t as_int() const;
    double as_float() const;
    bool as_bool() const;
    const std::vector<std::string>& as_list() const;

    bool has_value() const {
        return !std::holds_alternative<std::monostate>(value);
    }
};

/// @brief Parsed command arguments container
class CommandArgs {
public:
    CommandArgs() = default;

    void add(const std::string& name, ArgValue value, bool is_flag = false);
    void add_positional(ArgValue value);

    bool has(const std::string& name) const;
    const CommandArg* get(const std::string& name) const;
    const CommandArg& get_or_default(const std::string& name, const CommandArg& default_arg) const;

    std::string get_string(const std::string& name, const std::string& default_val = "") const;
    std::int64_t get_int(const std::string& name, std::int64_t default_val = 0) const;
    double get_float(const std::string& name, double default_val = 0.0) const;
    bool get_bool(const std::string& name, bool default_val = false) const;

    const std::vector<CommandArg>& positional() const { return positional_args_; }
    std::size_t positional_count() const { return positional_args_.size(); }

    const std::string& raw_input() const { return raw_input_; }
    void set_raw_input(const std::string& input) { raw_input_ = input; }

private:
    std::unordered_map<std::string, CommandArg> named_args_;
    std::vector<CommandArg> positional_args_;
    std::string raw_input_;
};

// =============================================================================
// Command Result
// =============================================================================

/// @brief Command execution status
enum class CommandStatus {
    Success,
    Error,
    Cancelled,
    Pending,
    Background
};

/// @brief Command execution result
struct CommandResult {
    CommandStatus status = CommandStatus::Success;
    int exit_code = 0;
    std::string output;
    std::string error_message;
    std::any data;  // Optional typed return data
    std::chrono::microseconds execution_time{0};

    static CommandResult success(const std::string& output = "") {
        CommandResult r;
        r.status = CommandStatus::Success;
        r.output = output;
        return r;
    }

    static CommandResult error(const std::string& message, int code = 1) {
        CommandResult r;
        r.status = CommandStatus::Error;
        r.exit_code = code;
        r.error_message = message;
        return r;
    }

    static CommandResult cancelled() {
        CommandResult r;
        r.status = CommandStatus::Cancelled;
        return r;
    }

    bool ok() const { return status == CommandStatus::Success; }
    operator bool() const { return ok(); }
};

// =============================================================================
// Parsed Command
// =============================================================================

/// @brief Redirect specification
struct Redirect {
    enum class Type { Output, Append, Input };
    Type type;
    std::string target;  // File path or descriptor
};

/// @brief Parsed command structure
struct ParsedCommand {
    std::string name;
    CommandArgs args;
    std::vector<Redirect> redirects;
    bool background = false;  // Run with &

    // Pipeline support
    std::shared_ptr<ParsedCommand> pipe_to;  // Next command in pipeline

    bool is_valid() const { return !name.empty(); }
};

/// @brief Command line with multiple commands
struct CommandLine {
    std::vector<ParsedCommand> commands;
    enum class Connector { None, And, Or, Sequence };
    std::vector<Connector> connectors;

    bool is_empty() const { return commands.empty(); }
};

// =============================================================================
// Command Metadata
// =============================================================================

/// @brief Command category
enum class CommandCategory {
    General,
    FileSystem,
    Variables,
    Scripting,
    Debug,
    Engine,
    Ecs,
    Assets,
    Rendering,
    Audio,
    Physics,
    Network,
    Profile,
    Help,
    Custom
};

/// @brief Flag specification
struct FlagSpec {
    std::string name;
    char short_name = '\0';
    std::string description;
    bool takes_value = false;
    ArgType value_type = ArgType::String;
};

/// @brief Command metadata
struct CommandInfo {
    CommandId id = 0;
    std::string name;
    std::string description;
    std::string usage;
    std::vector<std::string> examples;
    CommandCategory category = CommandCategory::General;
    std::vector<ArgSpec> args;
    std::vector<FlagSpec> flags;
    std::vector<std::string> aliases;
    bool hidden = false;
    bool privileged = false;
    bool requires_session = false;
    bool can_background = false;

    // Variadic argument support
    bool variadic = false;
    std::string variadic_name;
    ArgType variadic_type = ArgType::String;
    std::string variadic_desc;

    std::string category_name() const;
};

// =============================================================================
// History Entry
// =============================================================================

/// @brief History entry
struct HistoryEntry {
    std::size_t index = 0;
    std::string command;
    std::chrono::system_clock::time_point timestamp;
    CommandStatus status = CommandStatus::Success;
    int exit_code = 0;
    std::chrono::microseconds duration{0};
};

// =============================================================================
// Shell Configuration
// =============================================================================

/// @brief Shell configuration
struct ShellConfig {
    std::string prompt = "> ";
    std::string continuation_prompt = "... ";
    std::size_t max_history_size = 1000;
    bool save_history = true;
    std::string history_file = ".void_shell_history";
    bool echo_commands = false;
    bool color_output = true;
    std::chrono::milliseconds command_timeout{30000};
    bool allow_background = true;
    bool allow_remote = false;
    std::uint16_t remote_port = 9876;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Get string name for token type
const char* token_type_name(TokenType type);

/// @brief Get string name for argument type
const char* arg_type_name(ArgType type);

/// @brief Get string name for command category
const char* category_name(CommandCategory cat);

/// @brief Get string name for command status
const char* status_name(CommandStatus status);

/// @brief Parse a string to ArgValue based on type
ArgValue parse_arg_value(const std::string& str, ArgType type);

/// @brief Convert ArgValue to string
std::string arg_value_to_string(const ArgValue& value);

} // namespace void_shell
