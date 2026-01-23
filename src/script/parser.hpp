#pragma once

/// @file parser.hpp
/// @brief Parser for VoidScript

#include "ast.hpp"
#include "lexer.hpp"

#include <memory>
#include <string>
#include <vector>

namespace void_script {

/// @brief Parser for VoidScript
class Parser {
public:
    /// @brief Construct a parser for the given source
    explicit Parser(std::string_view source, std::string_view filename = "<script>");

    /// @brief Construct a parser from a lexer
    explicit Parser(Lexer lexer);

    // ==========================================================================
    // Parsing
    // ==========================================================================

    /// @brief Parse the entire program
    [[nodiscard]] std::unique_ptr<Program> parse_program();

    /// @brief Parse a single statement
    [[nodiscard]] StmtPtr parse_statement();

    /// @brief Parse a single expression
    [[nodiscard]] ExprPtr parse_expression();

    // ==========================================================================
    // State
    // ==========================================================================

    /// @brief Check if errors occurred
    [[nodiscard]] bool has_errors() const { return !errors_.empty(); }

    /// @brief Get errors
    [[nodiscard]] const std::vector<ScriptException>& errors() const { return errors_; }

private:
    // Token helpers
    [[nodiscard]] Token current() const { return current_; }
    [[nodiscard]] Token previous() const { return previous_; }
    [[nodiscard]] bool check(TokenType type) const;
    [[nodiscard]] bool is_at_end() const;

    Token advance();
    bool match(TokenType type);
    bool match(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);

    // Error handling
    void error(const std::string& message);
    void error(const Token& token, const std::string& message);
    void synchronize();

    // ==========================================================================
    // Expression Parsing (Pratt parser)
    // ==========================================================================

    /// @brief Parse expression with given precedence
    ExprPtr parse_precedence(int precedence);

    /// @brief Parse prefix expression
    ExprPtr parse_prefix();

    /// @brief Parse infix expression
    ExprPtr parse_infix(ExprPtr left, int precedence);

    // Specific expression parsers
    ExprPtr parse_literal();
    ExprPtr parse_identifier();
    ExprPtr parse_grouping();
    ExprPtr parse_array();
    ExprPtr parse_map();
    ExprPtr parse_unary();
    ExprPtr parse_binary(ExprPtr left);
    ExprPtr parse_call(ExprPtr callee);
    ExprPtr parse_member(ExprPtr object);
    ExprPtr parse_index(ExprPtr object);
    ExprPtr parse_assignment(ExprPtr target);
    ExprPtr parse_ternary(ExprPtr condition);
    ExprPtr parse_lambda();
    ExprPtr parse_new();
    ExprPtr parse_this();
    ExprPtr parse_super();
    ExprPtr parse_await();
    ExprPtr parse_yield();

    // ==========================================================================
    // Statement Parsing
    // ==========================================================================

    StmtPtr parse_declaration();
    StmtPtr parse_var_declaration();
    StmtPtr parse_function_declaration();
    StmtPtr parse_class_declaration();
    StmtPtr parse_import_declaration();
    StmtPtr parse_export_declaration();
    StmtPtr parse_module_declaration();

    StmtPtr parse_simple_statement();
    StmtPtr parse_expression_statement();
    StmtPtr parse_block_statement();
    StmtPtr parse_if_statement();
    StmtPtr parse_while_statement();
    StmtPtr parse_for_statement();
    StmtPtr parse_return_statement();
    StmtPtr parse_break_statement();
    StmtPtr parse_continue_statement();
    StmtPtr parse_match_statement();
    StmtPtr parse_try_statement();
    StmtPtr parse_throw_statement();

    // Helpers
    std::vector<FunctionDecl::Parameter> parse_parameters();
    std::vector<ExprPtr> parse_arguments();
    std::optional<std::string> parse_type_annotation();

    // ==========================================================================
    // Precedence
    // ==========================================================================

    enum class Precedence {
        None = 0,
        Assignment,     // = += -= etc.
        Ternary,        // ?:
        NullCoalesce,   // ??
        Or,             // ||
        And,            // &&
        BitwiseOr,      // |
        BitwiseXor,     // ^
        BitwiseAnd,     // &
        Equality,       // == !=
        Comparison,     // < <= > >= <=>
        Shift,          // << >>
        Range,          // .. ..=
        Term,           // + -
        Factor,         // * / %
        Power,          // **
        Unary,          // ! - ~ ++ --
        Call,           // () [] .
        Primary
    };

    [[nodiscard]] static int get_precedence(TokenType type);
    [[nodiscard]] static bool is_right_associative(TokenType type);

    // State
    Lexer lexer_;
    Token current_;
    Token previous_;
    std::vector<ScriptException> errors_;
    bool panic_mode_ = false;
};

} // namespace void_script
