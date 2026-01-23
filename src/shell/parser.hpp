#pragma once

/// @file parser.hpp
/// @brief Shell input lexer and parser

#include "types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace void_shell {

// =============================================================================
// Lexer
// =============================================================================

/// @brief Tokenizer for shell input
class Lexer {
public:
    explicit Lexer(std::string_view input);

    /// @brief Get next token
    Token next();

    /// @brief Peek at next token without consuming
    Token peek();

    /// @brief Check if at end of input
    bool at_end() const;

    /// @brief Get current position
    std::size_t position() const { return pos_; }

    /// @brief Get current line
    std::size_t line() const { return line_; }

    /// @brief Get current column
    std::size_t column() const { return column_; }

    /// @brief Reset to beginning
    void reset();

    /// @brief Get all tokens (for debugging)
    std::vector<Token> tokenize_all();

private:
    std::string_view input_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
    std::optional<Token> peeked_;

    char current() const;
    char peek_char(std::size_t offset = 1) const;
    void advance();
    void skip_whitespace();
    void skip_comment();

    Token make_token(TokenType type, std::string value = "");
    Token scan_string(char quote);
    Token scan_number();
    Token scan_identifier();
    Token scan_variable();
    Token scan_flag();
};

// =============================================================================
// Parser
// =============================================================================

/// @brief Parser for shell command lines
class Parser {
public:
    Parser();
    explicit Parser(const CommandRegistry* registry);

    /// @brief Parse a command line
    ShellResult<CommandLine> parse(std::string_view input);

    /// @brief Parse a single command
    ShellResult<ParsedCommand> parse_command(std::string_view input);

    /// @brief Check if input is complete (no unclosed quotes, etc.)
    bool is_complete(std::string_view input) const;

    /// @brief Get parse error message
    const std::string& error_message() const { return error_; }

    /// @brief Enable/disable variable expansion
    void set_expand_variables(bool expand) { expand_variables_ = expand; }

    /// @brief Set variable resolver
    using VariableResolver = std::function<std::optional<std::string>(const std::string&)>;
    void set_variable_resolver(VariableResolver resolver) { resolver_ = std::move(resolver); }

    /// @brief Set alias resolver
    using AliasResolver = std::function<std::optional<std::string>(const std::string&)>;
    void set_alias_resolver(AliasResolver resolver) { alias_resolver_ = std::move(resolver); }

private:
    const CommandRegistry* registry_ = nullptr;
    std::string error_;
    bool expand_variables_ = true;
    VariableResolver resolver_;
    AliasResolver alias_resolver_;

    // Parsing state
    std::unique_ptr<Lexer> lexer_;
    Token current_;

    void advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    void set_error(const std::string& message);

    // Parsing methods
    ShellResult<CommandLine> parse_command_line();
    ShellResult<ParsedCommand> parse_single_command();
    ShellResult<std::vector<CommandArg>> parse_arguments(const CommandInfo* info);
    ShellResult<Redirect> parse_redirect();

    // Variable expansion
    std::string expand_variables(const std::string& input);
    std::string expand_aliases(const std::string& command);
};

// =============================================================================
// Expression Evaluator
// =============================================================================

/// @brief Simple expression evaluator for shell expressions
class ExpressionEvaluator {
public:
    ExpressionEvaluator();

    /// @brief Evaluate an arithmetic expression
    ShellResult<double> evaluate_arithmetic(std::string_view expr);

    /// @brief Evaluate a boolean expression
    ShellResult<bool> evaluate_boolean(std::string_view expr);

    /// @brief Evaluate a string expression (with interpolation)
    ShellResult<std::string> evaluate_string(std::string_view expr);

    /// @brief Set variable resolver for expression variables
    using VariableResolver = std::function<std::optional<ArgValue>(const std::string&)>;
    void set_variable_resolver(VariableResolver resolver) { resolver_ = std::move(resolver); }

private:
    VariableResolver resolver_;

    // Arithmetic parsing
    double parse_expression(std::string_view& input);
    double parse_term(std::string_view& input);
    double parse_factor(std::string_view& input);
    double parse_primary(std::string_view& input);
    double get_variable_value(const std::string& name);
    void skip_whitespace(std::string_view& input);
};

// =============================================================================
// Glob Pattern Matcher
// =============================================================================

/// @brief Glob pattern matching for file paths
class GlobMatcher {
public:
    explicit GlobMatcher(std::string_view pattern);

    /// @brief Check if a path matches the pattern
    bool matches(std::string_view path) const;

    /// @brief Expand glob pattern to matching paths
    std::vector<std::string> expand(const std::string& base_path = ".") const;

    /// @brief Check if pattern contains glob characters
    static bool is_glob_pattern(std::string_view str);

private:
    std::string pattern_;
    std::vector<std::string> parts_;

    bool match_part(std::string_view pattern, std::string_view str) const;
    bool match_recursive(const std::vector<std::string>& parts, std::size_t part_idx,
                         const std::string& path) const;
};

// =============================================================================
// Brace Expansion
// =============================================================================

/// @brief Brace expansion (e.g., file{1,2,3}.txt -> file1.txt file2.txt file3.txt)
class BraceExpander {
public:
    /// @brief Expand braces in a string
    static std::vector<std::string> expand(std::string_view input);

    /// @brief Check if string contains brace patterns
    static bool has_braces(std::string_view str);

private:
    static std::vector<std::string> expand_recursive(std::string_view prefix,
                                                       std::string_view remaining);
    static std::pair<std::vector<std::string>, std::size_t> parse_brace(std::string_view input);
};

// =============================================================================
// Word Splitter
// =============================================================================

/// @brief Split a command line into words respecting quotes
class WordSplitter {
public:
    /// @brief Split input into words
    static std::vector<std::string> split(std::string_view input);

    /// @brief Join words into a command line with proper quoting
    static std::string join(const std::vector<std::string>& words);

    /// @brief Quote a string if necessary
    static std::string quote_if_needed(const std::string& str);

    /// @brief Unquote a string
    static std::string unquote(const std::string& str);
};

} // namespace void_shell
