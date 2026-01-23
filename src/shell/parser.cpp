/// @file parser.cpp
/// @brief Shell parser implementation for void_shell

#include "parser.hpp"
#include "command.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <sstream>

namespace void_shell {

// =============================================================================
// Lexer Implementation
// =============================================================================

Lexer::Lexer(std::string_view input) : input_(input) {}

Token Lexer::next() {
    // Return peeked token if available
    if (peeked_) {
        Token token = std::move(*peeked_);
        peeked_.reset();
        return token;
    }

    skip_whitespace();

    if (at_end()) {
        return make_token(TokenType::Eof);
    }

    char c = current();

    // Comments
    if (c == '#') {
        skip_comment();
        return next();
    }

    // Newline
    if (c == '\n') {
        advance();
        return make_token(TokenType::Newline, "\n");
    }

    // String literals
    if (c == '"' || c == '\'') {
        char quote = c;
        advance();
        return scan_string(quote);
    }

    // Variable
    if (c == '$') {
        return scan_variable();
    }

    // Operators and punctuation
    if (c == '|') {
        advance();
        if (current() == '|') {
            advance();
            return make_token(TokenType::Or, "||");
        }
        return make_token(TokenType::Pipe, "|");
    }

    if (c == '&') {
        advance();
        if (current() == '&') {
            advance();
            return make_token(TokenType::And, "&&");
        }
        return make_token(TokenType::Ampersand, "&");
    }

    if (c == ';') {
        advance();
        return make_token(TokenType::Semicolon, ";");
    }

    if (c == '(') {
        advance();
        return make_token(TokenType::LeftParen, "(");
    }

    if (c == ')') {
        advance();
        return make_token(TokenType::RightParen, ")");
    }

    if (c == '{') {
        advance();
        return make_token(TokenType::LeftBrace, "{");
    }

    if (c == '}') {
        advance();
        return make_token(TokenType::RightBrace, "}");
    }

    if (c == '[') {
        advance();
        return make_token(TokenType::LeftBracket, "[");
    }

    if (c == ']') {
        advance();
        return make_token(TokenType::RightBracket, "]");
    }

    if (c == '=') {
        advance();
        return make_token(TokenType::Equals, "=");
    }

    if (c == ':') {
        advance();
        return make_token(TokenType::Colon, ":");
    }

    if (c == ',') {
        advance();
        return make_token(TokenType::Comma, ",");
    }

    if (c == '.') {
        advance();
        return make_token(TokenType::Dot, ".");
    }

    // Redirections
    if (c == '>') {
        advance();
        if (current() == '>') {
            advance();
            return make_token(TokenType::RedirectAppend, ">>");
        }
        return make_token(TokenType::Redirect, ">");
    }

    if (c == '<') {
        advance();
        return make_token(TokenType::RedirectInput, "<");
    }

    // Flags
    if (c == '-') {
        char next_c = peek_char();
        if (next_c == '-' || std::isalpha(static_cast<unsigned char>(next_c))) {
            return scan_flag();
        }
        // Could be negative number
        if (std::isdigit(static_cast<unsigned char>(next_c))) {
            return scan_number();
        }
        advance();
        return make_token(TokenType::Identifier, "-");
    }

    // Numbers
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return scan_number();
    }

    // Identifiers
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scan_identifier();
    }

    // Unknown - treat as identifier
    return scan_identifier();
}

Token Lexer::peek() {
    if (!peeked_) {
        peeked_ = next();
    }
    return *peeked_;
}

bool Lexer::at_end() const {
    return pos_ >= input_.size();
}

void Lexer::reset() {
    pos_ = 0;
    line_ = 1;
    column_ = 1;
    peeked_.reset();
}

std::vector<Token> Lexer::tokenize_all() {
    std::vector<Token> tokens;
    reset();

    while (true) {
        Token token = next();
        tokens.push_back(token);

        if (token.type == TokenType::Eof || token.type == TokenType::Error) {
            break;
        }
    }

    return tokens;
}

char Lexer::current() const {
    return at_end() ? '\0' : input_[pos_];
}

char Lexer::peek_char(std::size_t offset) const {
    return (pos_ + offset >= input_.size()) ? '\0' : input_[pos_ + offset];
}

void Lexer::advance() {
    if (!at_end()) {
        if (input_[pos_] == '\n') {
            line_++;
            column_ = 1;
        } else {
            column_++;
        }
        pos_++;
    }
}

void Lexer::skip_whitespace() {
    while (!at_end()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else {
            break;
        }
    }
}

void Lexer::skip_comment() {
    while (!at_end() && current() != '\n') {
        advance();
    }
}

Token Lexer::make_token(TokenType type, std::string value) {
    Token token;
    token.type = type;
    token.value = std::move(value);
    token.line = line_;
    token.column = column_;
    return token;
}

Token Lexer::scan_string(char quote) {
    std::string value;
    bool escaped = false;

    while (!at_end()) {
        char c = current();

        if (escaped) {
            switch (c) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case '$': value += '$'; break;
                default: value += c; break;
            }
            escaped = false;
            advance();
        } else if (c == '\\') {
            escaped = true;
            advance();
        } else if (c == quote) {
            advance(); // Skip closing quote
            return make_token(TokenType::String, value);
        } else {
            value += c;
            advance();
        }
    }

    return make_token(TokenType::Error, "Unterminated string");
}

Token Lexer::scan_number() {
    std::string value;
    bool is_float = false;

    // Handle negative sign
    if (current() == '-') {
        value += current();
        advance();
    }

    // Integer part
    while (!at_end() && std::isdigit(static_cast<unsigned char>(current()))) {
        value += current();
        advance();
    }

    // Decimal part
    if (current() == '.' && std::isdigit(static_cast<unsigned char>(peek_char()))) {
        is_float = true;
        value += current();
        advance();

        while (!at_end() && std::isdigit(static_cast<unsigned char>(current()))) {
            value += current();
            advance();
        }
    }

    // Exponent part
    if (current() == 'e' || current() == 'E') {
        is_float = true;
        value += current();
        advance();

        if (current() == '+' || current() == '-') {
            value += current();
            advance();
        }

        while (!at_end() && std::isdigit(static_cast<unsigned char>(current()))) {
            value += current();
            advance();
        }
    }

    return make_token(is_float ? TokenType::Float : TokenType::Integer, value);
}

Token Lexer::scan_identifier() {
    std::string value;

    auto is_delimiter = [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) ||
               c == '|' || c == '&' || c == ';' ||
               c == '(' || c == ')' || c == '{' || c == '}' ||
               c == '[' || c == ']' || c == '<' || c == '>' ||
               c == '=' || c == '#' || c == '"' || c == '\'';
    };

    while (!at_end() && !is_delimiter(current())) {
        value += current();
        advance();
    }

    // Check for boolean literals
    if (value == "true" || value == "false") {
        return make_token(TokenType::Boolean, value);
    }

    return make_token(TokenType::Identifier, value);
}

Token Lexer::scan_variable() {
    advance(); // Skip $

    std::string name;

    if (current() == '{') {
        // ${variable}
        advance();
        while (!at_end() && current() != '}') {
            name += current();
            advance();
        }
        if (current() == '}') {
            advance();
        }
    } else if (current() == '(') {
        // $(command) - command substitution
        advance();
        int depth = 1;
        while (!at_end() && depth > 0) {
            if (current() == '(') depth++;
            else if (current() == ')') depth--;
            if (depth > 0) {
                name += current();
            }
            advance();
        }
        return make_token(TokenType::Variable, "$(" + name + ")");
    } else {
        // $variable
        while (!at_end() && (std::isalnum(static_cast<unsigned char>(current())) || current() == '_')) {
            name += current();
            advance();
        }
    }

    return make_token(TokenType::Variable, name);
}

Token Lexer::scan_flag() {
    std::string value;

    value += current(); // -
    advance();

    if (current() == '-') {
        // Long flag --flag
        value += current();
        advance();
    }

    while (!at_end() && (std::isalnum(static_cast<unsigned char>(current())) ||
           current() == '-' || current() == '_')) {
        value += current();
        advance();
    }

    return make_token(TokenType::Flag, value);
}

// =============================================================================
// Parser Implementation
// =============================================================================

Parser::Parser() : registry_(nullptr) {}

Parser::Parser(const CommandRegistry* registry) : registry_(registry) {}

ShellResult<CommandLine> Parser::parse(std::string_view input) {
    lexer_ = std::make_unique<Lexer>(input);
    current_ = lexer_->next();
    error_.clear();

    return parse_command_line();
}

ShellResult<ParsedCommand> Parser::parse_command(std::string_view input) {
    lexer_ = std::make_unique<Lexer>(input);
    current_ = lexer_->next();
    error_.clear();

    return parse_single_command();
}

bool Parser::is_complete(std::string_view input) const {
    // Check for unclosed quotes
    int single_quotes = 0;
    int double_quotes = 0;
    int parens = 0;
    int braces = 0;
    int brackets = 0;
    bool escaped = false;

    for (char c : input) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (single_quotes % 2 == 0 && c == '"') {
            double_quotes++;
        } else if (double_quotes % 2 == 0 && c == '\'') {
            single_quotes++;
        } else if (single_quotes % 2 == 0 && double_quotes % 2 == 0) {
            switch (c) {
                case '(': parens++; break;
                case ')': parens--; break;
                case '{': braces++; break;
                case '}': braces--; break;
                case '[': brackets++; break;
                case ']': brackets--; break;
            }
        }
    }

    return single_quotes % 2 == 0 && double_quotes % 2 == 0 &&
           parens == 0 && braces == 0 && brackets == 0;
}

void Parser::advance() {
    current_ = lexer_->next();
}

bool Parser::check(TokenType type) const {
    return current_.type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (!check(type)) {
        set_error(message);
        return Token{TokenType::Error, message, 0, 0};
    }
    Token token = current_;
    advance();
    return token;
}

void Parser::set_error(const std::string& message) {
    error_ = message;
}

ShellResult<CommandLine> Parser::parse_command_line() {
    CommandLine line;

    while (!check(TokenType::Eof)) {
        // Skip newlines
        while (check(TokenType::Newline)) {
            advance();
        }

        if (check(TokenType::Eof)) {
            break;
        }

        auto cmd_result = parse_single_command();
        if (!cmd_result) {
            return cmd_result.error();
        }

        auto cmd = cmd_result.value();

        // Check for connector
        CommandLine::Connector connector = CommandLine::Connector::None;
        if (check(TokenType::Semicolon)) {
            connector = CommandLine::Connector::Sequence;
            advance();
        } else if (check(TokenType::And)) {
            connector = CommandLine::Connector::And;
            advance();
        } else if (check(TokenType::Or)) {
            connector = CommandLine::Connector::Or;
            advance();
        } else if (check(TokenType::Ampersand)) {
            cmd.background = true;
            advance();
        }

        line.commands.push_back(std::move(cmd));
        if (connector != CommandLine::Connector::None) {
            line.connectors.push_back(connector);
        }
    }

    return line;
}

ShellResult<ParsedCommand> Parser::parse_single_command() {
    ParsedCommand cmd;

    // Parse command name
    if (check(TokenType::Identifier) || check(TokenType::String)) {
        std::string name = current_.value;
        advance();

        // Expand alias
        name = expand_aliases(name);
        cmd.name = name;

        // Get command info for argument parsing
        const CommandInfo* info = nullptr;
        if (registry_) {
            auto* cmd = registry_->find(name);
            if (cmd) {
                info = &cmd->info();
            }
        }

        // Parse arguments
        auto args_result = parse_arguments(info);
        if (!args_result) {
            return args_result.error();
        }

        // Convert vector<CommandArg> to CommandArgs
        for (const auto& arg : args_result.value()) {
            if (arg.is_flag || !arg.name.empty()) {
                cmd.args.add(arg.name, arg.value, arg.is_flag);
            } else {
                cmd.args.add_positional(arg.value);
            }
        }
    }

    // Parse pipe
    if (check(TokenType::Pipe)) {
        advance();
        auto next_result = parse_single_command();
        if (!next_result) {
            return next_result.error();
        }
        cmd.pipe_to = std::make_shared<ParsedCommand>(next_result.value());
    }

    return cmd;
}

ShellResult<std::vector<CommandArg>> Parser::parse_arguments(const CommandInfo* info) {
    std::vector<CommandArg> args;
    std::string raw_input;

    while (!check(TokenType::Eof) &&
           !check(TokenType::Pipe) &&
           !check(TokenType::Semicolon) &&
           !check(TokenType::And) &&
           !check(TokenType::Or) &&
           !check(TokenType::Ampersand) &&
           !check(TokenType::Newline)) {

        // Handle redirects
        if (check(TokenType::Redirect) || check(TokenType::RedirectAppend) ||
            check(TokenType::RedirectInput)) {
            auto redirect_result = parse_redirect();
            if (!redirect_result) {
                return redirect_result.error();
            }
            // Store redirect info separately (handled by caller)
            advance();
            continue;
        }

        // Handle flags
        if (check(TokenType::Flag)) {
            std::string flag_name = current_.value;
            advance();

            CommandArg arg;
            arg.name = flag_name;
            arg.is_flag = true;

            // Check if flag takes a value
            if (!check(TokenType::Flag) && !check(TokenType::Pipe) &&
                !check(TokenType::Eof) && !check(TokenType::Semicolon) &&
                !check(TokenType::Newline) &&
                (check(TokenType::Identifier) || check(TokenType::String) ||
                 check(TokenType::Integer) || check(TokenType::Float))) {

                std::string value = expand_variables(current_.value);
                arg.value = value;
                raw_input += " " + value;
                advance();
            } else {
                arg.value = true;
            }

            args.push_back(std::move(arg));
            continue;
        }

        // Handle variables
        if (check(TokenType::Variable)) {
            std::string value = expand_variables("$" + current_.value);
            CommandArg arg;
            arg.value = value;
            args.push_back(std::move(arg));
            raw_input += " " + value;
            advance();
            continue;
        }

        // Handle other tokens as positional arguments
        std::string value = expand_variables(current_.value);
        CommandArg arg;
        arg.value = value;
        args.push_back(std::move(arg));
        raw_input += " " + value;
        advance();
    }

    return args;
}

ShellResult<Redirect> Parser::parse_redirect() {
    Redirect redirect;

    if (check(TokenType::Redirect)) {
        redirect.type = Redirect::Type::Output;
    } else if (check(TokenType::RedirectAppend)) {
        redirect.type = Redirect::Type::Append;
    } else if (check(TokenType::RedirectInput)) {
        redirect.type = Redirect::Type::Input;
    } else {
        return ShellError::InvalidSyntax;
    }

    advance();

    if (!check(TokenType::Identifier) && !check(TokenType::String)) {
        return ShellError::InvalidSyntax;
    }

    redirect.target = expand_variables(current_.value);
    return redirect;
}

std::string Parser::expand_variables(const std::string& input) {
    if (!expand_variables_) {
        return input;
    }

    std::string result;
    std::size_t i = 0;

    while (i < input.size()) {
        if (input[i] == '$') {
            i++;
            if (i >= input.size()) {
                result += '$';
                break;
            }

            std::string var_name;

            if (input[i] == '{') {
                i++;
                while (i < input.size() && input[i] != '}') {
                    var_name += input[i++];
                }
                if (i < input.size()) i++;
            } else {
                while (i < input.size() && (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_')) {
                    var_name += input[i++];
                }
            }

            if (resolver_) {
                auto value = resolver_(var_name);
                if (value) {
                    result += *value;
                }
            }
        } else if (input[i] == '\\' && i + 1 < input.size()) {
            i++;
            result += input[i++];
        } else {
            result += input[i++];
        }
    }

    return result;
}

std::string Parser::expand_aliases(const std::string& command) {
    if (alias_resolver_) {
        auto expanded = alias_resolver_(command);
        if (expanded) {
            return *expanded;
        }
    }
    return command;
}

// =============================================================================
// ExpressionEvaluator Implementation
// =============================================================================

ExpressionEvaluator::ExpressionEvaluator() = default;

ShellResult<double> ExpressionEvaluator::evaluate_arithmetic(std::string_view expr) {
    std::string_view input = expr;
    try {
        return parse_expression(input);
    } catch (...) {
        return ShellError::InvalidSyntax;
    }
}

ShellResult<bool> ExpressionEvaluator::evaluate_boolean(std::string_view expr) {
    auto result = evaluate_arithmetic(expr);
    if (!result) {
        return result.error();
    }
    return result.value() != 0.0;
}

ShellResult<std::string> ExpressionEvaluator::evaluate_string(std::string_view expr) {
    // Simple string interpolation
    std::string result;
    std::size_t i = 0;

    while (i < expr.size()) {
        if (expr[i] == '$' && i + 1 < expr.size()) {
            i++;
            std::string var_name;

            if (expr[i] == '{') {
                i++;
                while (i < expr.size() && expr[i] != '}') {
                    var_name += expr[i++];
                }
                if (i < expr.size()) i++;
            } else {
                while (i < expr.size() && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_')) {
                    var_name += expr[i++];
                }
            }

            if (resolver_) {
                auto value = resolver_(var_name);
                if (value) {
                    result += arg_value_to_string(*value);
                }
            }
        } else {
            result += expr[i++];
        }
    }

    return result;
}

void ExpressionEvaluator::skip_whitespace(std::string_view& input) {
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input[0]))) {
        input.remove_prefix(1);
    }
}

double ExpressionEvaluator::parse_expression(std::string_view& input) {
    return parse_term(input);
}

double ExpressionEvaluator::parse_term(std::string_view& input) {
    double left = parse_factor(input);

    skip_whitespace(input);
    while (!input.empty() && (input[0] == '+' || input[0] == '-')) {
        char op = input[0];
        input.remove_prefix(1);
        double right = parse_factor(input);

        if (op == '+') {
            left += right;
        } else {
            left -= right;
        }
        skip_whitespace(input);
    }

    return left;
}

double ExpressionEvaluator::parse_factor(std::string_view& input) {
    double left = parse_primary(input);

    skip_whitespace(input);
    while (!input.empty() && (input[0] == '*' || input[0] == '/' || input[0] == '%')) {
        char op = input[0];
        input.remove_prefix(1);
        double right = parse_primary(input);

        if (op == '*') {
            left *= right;
        } else if (op == '/') {
            left = right != 0.0 ? left / right : 0.0;
        } else {
            left = right != 0.0 ? std::fmod(left, right) : 0.0;
        }
        skip_whitespace(input);
    }

    return left;
}

double ExpressionEvaluator::parse_primary(std::string_view& input) {
    skip_whitespace(input);

    if (input.empty()) {
        return 0.0;
    }

    // Negation
    if (input[0] == '-') {
        input.remove_prefix(1);
        return -parse_primary(input);
    }

    // Parentheses
    if (input[0] == '(') {
        input.remove_prefix(1);
        double result = parse_expression(input);
        skip_whitespace(input);
        if (!input.empty() && input[0] == ')') {
            input.remove_prefix(1);
        }
        return result;
    }

    // Variable
    if (input[0] == '$') {
        input.remove_prefix(1);
        std::string name;

        if (!input.empty() && input[0] == '{') {
            input.remove_prefix(1);
            while (!input.empty() && input[0] != '}') {
                name += input[0];
                input.remove_prefix(1);
            }
            if (!input.empty()) input.remove_prefix(1);
        } else {
            while (!input.empty() && (std::isalnum(static_cast<unsigned char>(input[0])) || input[0] == '_')) {
                name += input[0];
                input.remove_prefix(1);
            }
        }

        return get_variable_value(name);
    }

    // Number
    if (std::isdigit(static_cast<unsigned char>(input[0])) || input[0] == '.') {
        std::string num;
        while (!input.empty() && (std::isdigit(static_cast<unsigned char>(input[0])) || input[0] == '.')) {
            num += input[0];
            input.remove_prefix(1);
        }
        // Handle exponent
        if (!input.empty() && (input[0] == 'e' || input[0] == 'E')) {
            num += input[0];
            input.remove_prefix(1);
            if (!input.empty() && (input[0] == '+' || input[0] == '-')) {
                num += input[0];
                input.remove_prefix(1);
            }
            while (!input.empty() && std::isdigit(static_cast<unsigned char>(input[0]))) {
                num += input[0];
                input.remove_prefix(1);
            }
        }
        return std::stod(num);
    }

    return 0.0;
}

double ExpressionEvaluator::get_variable_value(const std::string& name) {
    if (resolver_) {
        auto value = resolver_(name);
        if (value) {
            return std::visit([](auto&& v) -> double {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return 0.0;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    try { return std::stod(v); } catch (...) { return 0.0; }
                } else if constexpr (std::is_same_v<T, std::int64_t>) {
                    return static_cast<double>(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    return v;
                } else if constexpr (std::is_same_v<T, bool>) {
                    return v ? 1.0 : 0.0;
                } else {
                    return 0.0;
                }
            }, *value);
        }
    }
    return 0.0;
}

// =============================================================================
// GlobMatcher Implementation
// =============================================================================

GlobMatcher::GlobMatcher(std::string_view pattern) : pattern_(pattern) {
    // Split pattern into parts
    std::string current;
    for (char c : pattern_) {
        if (c == '/' || c == '\\') {
            if (!current.empty()) {
                parts_.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts_.push_back(current);
    }
}

bool GlobMatcher::matches(std::string_view path) const {
    return match_part(pattern_, path);
}

std::vector<std::string> GlobMatcher::expand(const std::string& base_path) const {
    std::vector<std::string> results;

    if (parts_.empty()) {
        return results;
    }

    // Use internal recursive expansion
    std::function<void(std::size_t, const std::string&)> expand_recursive;
    expand_recursive = [&](std::size_t part_idx, const std::string& path) {
        if (part_idx >= parts_.size()) {
            results.push_back(path);
            return;
        }

        const std::string& part = parts_[part_idx];

        // Handle ** pattern
        if (part == "**") {
            // Match current level
            if (part_idx + 1 < parts_.size()) {
                expand_recursive(part_idx + 1, path);
            }

            // Match subdirectories
            try {
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    if (entry.is_directory()) {
                        expand_recursive(part_idx, entry.path().string());
                    } else if (part_idx + 1 >= parts_.size()) {
                        results.push_back(entry.path().string());
                    }
                }
            } catch (...) {}
            return;
        }

        // Match pattern against directory entries
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                std::string name = entry.path().filename().string();

                if (match_part(part, name)) {
                    std::string new_path = entry.path().string();

                    if (part_idx + 1 >= parts_.size()) {
                        results.push_back(new_path);
                    } else if (entry.is_directory()) {
                        expand_recursive(part_idx + 1, new_path);
                    }
                }
            }
        } catch (...) {}
    };

    expand_recursive(0, base_path);
    return results;
}

bool GlobMatcher::is_glob_pattern(std::string_view str) {
    for (char c : str) {
        if (c == '*' || c == '?' || c == '[' || c == '{') {
            return true;
        }
    }
    return false;
}

bool GlobMatcher::match_part(std::string_view pattern, std::string_view str) const {
    std::size_t p = 0, s = 0;
    std::size_t star_p = std::string_view::npos;
    std::size_t star_s = 0;

    while (s < str.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == str[s])) {
            p++;
            s++;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star_p = p++;
            star_s = s;
        } else if (star_p != std::string_view::npos) {
            p = star_p + 1;
            s = ++star_s;
        } else {
            return false;
        }
    }

    while (p < pattern.size() && pattern[p] == '*') {
        p++;
    }

    return p == pattern.size();
}

bool GlobMatcher::match_recursive(const std::vector<std::string>& parts, std::size_t part_idx,
                                   const std::string& path) const {
    if (part_idx >= parts.size()) {
        return true;
    }

    const std::string& part = parts[part_idx];

    // Handle ** pattern
    if (part == "**") {
        // Match current level
        if (part_idx + 1 < parts.size() && match_recursive(parts, part_idx + 1, path)) {
            return true;
        }

        // Match subdirectories
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_directory()) {
                    if (match_recursive(parts, part_idx, entry.path().string())) {
                        return true;
                    }
                } else if (part_idx + 1 >= parts.size()) {
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    // Match pattern against directory entries
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            std::string name = entry.path().filename().string();

            if (match_part(part, name)) {
                if (part_idx + 1 >= parts.size()) {
                    return true;
                } else if (entry.is_directory()) {
                    if (match_recursive(parts, part_idx + 1, entry.path().string())) {
                        return true;
                    }
                }
            }
        }
    } catch (...) {}

    return false;
}

// =============================================================================
// BraceExpander Implementation
// =============================================================================

std::vector<std::string> BraceExpander::expand(std::string_view input) {
    return expand_recursive("", input);
}

bool BraceExpander::has_braces(std::string_view str) {
    int depth = 0;
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            i++;
            continue;
        }
        if (str[i] == '{') depth++;
        else if (str[i] == '}') {
            if (depth > 0) return true;
        }
    }
    return false;
}

std::vector<std::string> BraceExpander::expand_recursive(std::string_view prefix,
                                                          std::string_view remaining) {
    std::vector<std::string> results;

    // Find first brace
    std::size_t brace_start = std::string_view::npos;
    for (std::size_t i = 0; i < remaining.size(); ++i) {
        if (remaining[i] == '\\' && i + 1 < remaining.size()) {
            i++;
            continue;
        }
        if (remaining[i] == '{') {
            brace_start = i;
            break;
        }
    }

    if (brace_start == std::string_view::npos) {
        results.push_back(std::string(prefix) + std::string(remaining));
        return results;
    }

    // Find matching close brace
    auto [alternatives, brace_end] = parse_brace(remaining.substr(brace_start));
    if (brace_end == 0) {
        results.push_back(std::string(prefix) + std::string(remaining));
        return results;
    }

    std::string new_prefix = std::string(prefix) + std::string(remaining.substr(0, brace_start));
    std::string_view suffix = remaining.substr(brace_start + brace_end);

    for (const auto& alt : alternatives) {
        auto expanded = expand_recursive(new_prefix + alt, suffix);
        results.insert(results.end(), expanded.begin(), expanded.end());
    }

    return results;
}

std::pair<std::vector<std::string>, std::size_t> BraceExpander::parse_brace(std::string_view input) {
    std::vector<std::string> alternatives;

    if (input.empty() || input[0] != '{') {
        return {alternatives, 0};
    }

    std::string current;
    int depth = 1;
    std::size_t i = 1;

    while (i < input.size() && depth > 0) {
        char c = input[i];

        if (c == '\\' && i + 1 < input.size()) {
            current += input[i + 1];
            i += 2;
            continue;
        }

        if (c == '{') {
            depth++;
            current += c;
        } else if (c == '}') {
            depth--;
            if (depth == 0) {
                alternatives.push_back(current);
            } else {
                current += c;
            }
        } else if (c == ',' && depth == 1) {
            alternatives.push_back(current);
            current.clear();
        } else {
            current += c;
        }

        i++;
    }

    if (depth != 0) {
        return {{}, 0};
    }

    // Check for range pattern (e.g., {1..10})
    if (alternatives.size() == 1) {
        const std::string& content = alternatives[0];
        std::size_t dot_pos = content.find("..");
        if (dot_pos != std::string::npos) {
            std::string start_str = content.substr(0, dot_pos);
            std::string end_str = content.substr(dot_pos + 2);

            try {
                int start = std::stoi(start_str);
                int end = std::stoi(end_str);

                alternatives.clear();
                if (start <= end) {
                    for (int n = start; n <= end; n++) {
                        alternatives.push_back(std::to_string(n));
                    }
                } else {
                    for (int n = start; n >= end; n--) {
                        alternatives.push_back(std::to_string(n));
                    }
                }
            } catch (...) {
                // Not a range, keep original
            }
        }
    }

    return {alternatives, i};
}

// =============================================================================
// WordSplitter Implementation
// =============================================================================

std::vector<std::string> WordSplitter::split(std::string_view input) {
    std::vector<std::string> words;
    std::string current;
    bool in_quotes = false;
    char quote_char = 0;
    bool escaped = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (!in_quotes && (c == '"' || c == '\'')) {
            in_quotes = true;
            quote_char = c;
            continue;
        }

        if (in_quotes && c == quote_char) {
            in_quotes = false;
            quote_char = 0;
            continue;
        }

        if (!in_quotes && std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.empty()) {
        words.push_back(current);
    }

    return words;
}

std::string WordSplitter::join(const std::vector<std::string>& words) {
    std::string result;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i > 0) result += ' ';
        result += quote_if_needed(words[i]);
    }
    return result;
}

std::string WordSplitter::quote_if_needed(const std::string& str) {
    bool needs_quoting = false;
    for (char c : str) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '"' || c == '\'' ||
            c == '\\' || c == '$' || c == '`' || c == '!' || c == '*' ||
            c == '?' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == '|' || c == '&' || c == ';' || c == '<' || c == '>' ||
            c == '(' || c == ')' || c == '#') {
            needs_quoting = true;
            break;
        }
    }

    if (!needs_quoting) {
        return str;
    }

    // Use single quotes if no single quotes in string
    if (str.find('\'') == std::string::npos) {
        return "'" + str + "'";
    }

    // Use double quotes with escaping
    std::string result = "\"";
    for (char c : str) {
        if (c == '"' || c == '\\' || c == '$' || c == '`') {
            result += '\\';
        }
        result += c;
    }
    result += '"';
    return result;
}

std::string WordSplitter::unquote(const std::string& str) {
    if (str.size() < 2) {
        return str;
    }

    char first = str.front();
    char last = str.back();

    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        std::string result;
        bool escaped = false;

        for (std::size_t i = 1; i < str.size() - 1; ++i) {
            char c = str[i];

            if (escaped) {
                result += c;
                escaped = false;
            } else if (c == '\\' && first == '"') {
                escaped = true;
            } else {
                result += c;
            }
        }

        return result;
    }

    return str;
}

} // namespace void_shell
