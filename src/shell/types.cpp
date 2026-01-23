/// @file types.cpp
/// @brief Core types implementation for void_shell

#include "types.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace void_shell {

// =============================================================================
// CommandArg Implementation
// =============================================================================

std::string CommandArg::as_string() const {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    if (std::holds_alternative<std::int64_t>(value)) {
        return std::to_string(std::get<std::int64_t>(value));
    }
    if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    }
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    return "";
}

std::int64_t CommandArg::as_int() const {
    if (std::holds_alternative<std::int64_t>(value)) {
        return std::get<std::int64_t>(value);
    }
    if (std::holds_alternative<double>(value)) {
        return static_cast<std::int64_t>(std::get<double>(value));
    }
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? 1 : 0;
    }
    if (std::holds_alternative<std::string>(value)) {
        try {
            return std::stoll(std::get<std::string>(value));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

double CommandArg::as_float() const {
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    }
    if (std::holds_alternative<std::int64_t>(value)) {
        return static_cast<double>(std::get<std::int64_t>(value));
    }
    if (std::holds_alternative<std::string>(value)) {
        try {
            return std::stod(std::get<std::string>(value));
        } catch (...) {
            return 0.0;
        }
    }
    return 0.0;
}

bool CommandArg::as_bool() const {
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value);
    }
    if (std::holds_alternative<std::int64_t>(value)) {
        return std::get<std::int64_t>(value) != 0;
    }
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value) != 0.0;
    }
    if (std::holds_alternative<std::string>(value)) {
        std::string s = std::get<std::string>(value);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s == "true" || s == "1" || s == "yes" || s == "on";
    }
    return false;
}

const std::vector<std::string>& CommandArg::as_list() const {
    static const std::vector<std::string> empty;
    if (std::holds_alternative<std::vector<std::string>>(value)) {
        return std::get<std::vector<std::string>>(value);
    }
    return empty;
}

// =============================================================================
// CommandArgs Implementation
// =============================================================================

void CommandArgs::add(const std::string& name, ArgValue value, bool is_flag) {
    CommandArg arg;
    arg.name = name;
    arg.value = std::move(value);
    arg.is_flag = is_flag;
    named_args_[name] = std::move(arg);
}

void CommandArgs::add_positional(ArgValue value) {
    CommandArg arg;
    arg.value = std::move(value);
    positional_args_.push_back(std::move(arg));
}

bool CommandArgs::has(const std::string& name) const {
    return named_args_.count(name) > 0;
}

const CommandArg* CommandArgs::get(const std::string& name) const {
    auto it = named_args_.find(name);
    if (it != named_args_.end()) {
        return &it->second;
    }
    return nullptr;
}

const CommandArg& CommandArgs::get_or_default(const std::string& name,
                                                const CommandArg& default_arg) const {
    auto* arg = get(name);
    return arg ? *arg : default_arg;
}

std::string CommandArgs::get_string(const std::string& name, const std::string& default_val) const {
    auto* arg = get(name);
    return arg ? arg->as_string() : default_val;
}

std::int64_t CommandArgs::get_int(const std::string& name, std::int64_t default_val) const {
    auto* arg = get(name);
    return arg ? arg->as_int() : default_val;
}

double CommandArgs::get_float(const std::string& name, double default_val) const {
    auto* arg = get(name);
    return arg ? arg->as_float() : default_val;
}

bool CommandArgs::get_bool(const std::string& name, bool default_val) const {
    auto* arg = get(name);
    return arg ? arg->as_bool() : default_val;
}

// =============================================================================
// CommandInfo Implementation
// =============================================================================

std::string CommandInfo::category_name() const {
    return void_shell::category_name(category);
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* token_type_name(TokenType type) {
    switch (type) {
        case TokenType::Identifier: return "Identifier";
        case TokenType::String: return "String";
        case TokenType::Integer: return "Integer";
        case TokenType::Float: return "Float";
        case TokenType::Boolean: return "Boolean";
        case TokenType::Pipe: return "Pipe";
        case TokenType::Redirect: return "Redirect";
        case TokenType::RedirectAppend: return "RedirectAppend";
        case TokenType::RedirectInput: return "RedirectInput";
        case TokenType::And: return "And";
        case TokenType::Or: return "Or";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::Ampersand: return "Ampersand";
        case TokenType::LeftParen: return "LeftParen";
        case TokenType::RightParen: return "RightParen";
        case TokenType::LeftBrace: return "LeftBrace";
        case TokenType::RightBrace: return "RightBrace";
        case TokenType::LeftBracket: return "LeftBracket";
        case TokenType::RightBracket: return "RightBracket";
        case TokenType::Variable: return "Variable";
        case TokenType::Equals: return "Equals";
        case TokenType::Colon: return "Colon";
        case TokenType::Comma: return "Comma";
        case TokenType::Dot: return "Dot";
        case TokenType::Flag: return "Flag";
        case TokenType::Newline: return "Newline";
        case TokenType::Eof: return "Eof";
        case TokenType::Error: return "Error";
        default: return "Unknown";
    }
}

const char* arg_type_name(ArgType type) {
    switch (type) {
        case ArgType::String: return "string";
        case ArgType::Integer: return "integer";
        case ArgType::Float: return "float";
        case ArgType::Boolean: return "boolean";
        case ArgType::Path: return "path";
        case ArgType::EntityId: return "entity";
        case ArgType::Any: return "any";
        default: return "unknown";
    }
}

const char* category_name(CommandCategory cat) {
    switch (cat) {
        case CommandCategory::General: return "General";
        case CommandCategory::FileSystem: return "File System";
        case CommandCategory::Variables: return "Variables";
        case CommandCategory::Scripting: return "Scripting";
        case CommandCategory::Debug: return "Debug";
        case CommandCategory::Engine: return "Engine";
        case CommandCategory::Ecs: return "ECS";
        case CommandCategory::Assets: return "Assets";
        case CommandCategory::Rendering: return "Rendering";
        case CommandCategory::Audio: return "Audio";
        case CommandCategory::Physics: return "Physics";
        case CommandCategory::Network: return "Network";
        case CommandCategory::Profile: return "Profile";
        case CommandCategory::Help: return "Help";
        case CommandCategory::Custom: return "Custom";
        default: return "Unknown";
    }
}

const char* status_name(CommandStatus status) {
    switch (status) {
        case CommandStatus::Success: return "Success";
        case CommandStatus::Error: return "Error";
        case CommandStatus::Cancelled: return "Cancelled";
        case CommandStatus::Pending: return "Pending";
        case CommandStatus::Background: return "Background";
        default: return "Unknown";
    }
}

ArgValue parse_arg_value(const std::string& str, ArgType type) {
    switch (type) {
        case ArgType::String:
        case ArgType::Path:
            return str;

        case ArgType::Integer: {
            std::int64_t value = 0;
            auto result = std::from_chars(str.data(), str.data() + str.size(), value);
            if (result.ec == std::errc()) {
                return value;
            }
            return std::monostate{};
        }

        case ArgType::Float: {
            try {
                return std::stod(str);
            } catch (...) {
                return std::monostate{};
            }
        }

        case ArgType::Boolean: {
            std::string lower = str;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
                return true;
            }
            if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
                return false;
            }
            return std::monostate{};
        }

        case ArgType::EntityId:
            // Parse as integer
            return parse_arg_value(str, ArgType::Integer);

        case ArgType::Any:
            // Try to detect type
            if (str == "true" || str == "false") {
                return str == "true";
            }

            // Try integer
            {
                std::int64_t value = 0;
                auto result = std::from_chars(str.data(), str.data() + str.size(), value);
                if (result.ec == std::errc() && result.ptr == str.data() + str.size()) {
                    return value;
                }
            }

            // Try float
            if (str.find('.') != std::string::npos || str.find('e') != std::string::npos ||
                str.find('E') != std::string::npos) {
                try {
                    return std::stod(str);
                } catch (...) {}
            }

            // Default to string
            return str;

        default:
            return std::monostate{};
    }
}

std::string arg_value_to_string(const ArgValue& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return "";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            std::string result;
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (i > 0) result += " ";
                result += v[i];
            }
            return result;
        } else {
            return "";
        }
    }, value);
}

} // namespace void_shell
