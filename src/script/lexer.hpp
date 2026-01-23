#pragma once

/// @file lexer.hpp
/// @brief Lexical analyzer for VoidScript

#include "types.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_script {

/// @brief Lexical analyzer for VoidScript
class Lexer {
public:
    /// @brief Construct a lexer for the given source
    explicit Lexer(std::string_view source, std::string_view filename = "<script>");

    // ==========================================================================
    // Tokenization
    // ==========================================================================

    /// @brief Get the next token
    [[nodiscard]] Token next_token();

    /// @brief Peek at the next token without consuming it
    [[nodiscard]] Token peek_token();

    /// @brief Check if at end of file
    [[nodiscard]] bool is_at_end() const { return current_ >= source_.size(); }

    /// @brief Get all tokens
    [[nodiscard]] std::vector<Token> tokenize();

    // ==========================================================================
    // State
    // ==========================================================================

    /// @brief Get current source location
    [[nodiscard]] SourceLocation location() const;

    /// @brief Get source code
    [[nodiscard]] std::string_view source() const { return source_; }

    /// @brief Get filename
    [[nodiscard]] std::string_view filename() const { return filename_; }

    /// @brief Check if errors occurred
    [[nodiscard]] bool has_errors() const { return !errors_.empty(); }

    /// @brief Get errors
    [[nodiscard]] const std::vector<ScriptException>& errors() const { return errors_; }

private:
    // Character helpers
    [[nodiscard]] char peek() const;
    [[nodiscard]] char peek_next() const;
    char advance();
    bool match(char expected);

    // Skip helpers
    void skip_whitespace();
    void skip_line_comment();
    void skip_block_comment();

    // Token helpers
    Token make_token(TokenType type);
    Token error_token(const std::string& message);

    // Scanning
    Token scan_identifier();
    Token scan_number();
    Token scan_string(char quote);

    // Keyword lookup
    [[nodiscard]] TokenType check_keyword(std::size_t start, std::size_t length,
                                           const char* rest, TokenType type) const;
    [[nodiscard]] TokenType identifier_type() const;

    // State
    std::string_view source_;
    std::string_view filename_;
    std::size_t start_ = 0;
    std::size_t current_ = 0;
    std::uint32_t line_ = 1;
    std::uint32_t column_ = 1;

    std::vector<ScriptException> errors_;
    std::optional<Token> peeked_token_;

    // Keyword map
    static const std::unordered_map<std::string_view, TokenType> keywords_;
};

} // namespace void_script
