#pragma once

/// @file ast.hpp
/// @brief Abstract Syntax Tree nodes for VoidScript

#include "types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace void_script {

// =============================================================================
// AST Node Base
// =============================================================================

/// @brief Base class for all AST nodes
class AstNode {
public:
    virtual ~AstNode() = default;

    [[nodiscard]] SourceSpan span() const { return span_; }
    void set_span(SourceSpan span) { span_ = span; }

    // Visitor pattern
    template <typename Visitor>
    auto accept(Visitor& visitor);

protected:
    SourceSpan span_;
};

using AstPtr = std::unique_ptr<AstNode>;

// =============================================================================
// Expressions
// =============================================================================

/// @brief Base class for expressions
class Expression : public AstNode {
public:
    virtual ~Expression() = default;
};

using ExprPtr = std::unique_ptr<Expression>;

/// @brief Literal value expression
class LiteralExpr : public Expression {
public:
    Value value;

    explicit LiteralExpr(Value v) : value(std::move(v)) {}
};

/// @brief Identifier expression
class IdentifierExpr : public Expression {
public:
    std::string name;

    explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
};

/// @brief Binary operator expression
class BinaryExpr : public Expression {
public:
    TokenType op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(TokenType op, ExprPtr left, ExprPtr right)
        : op(op), left(std::move(left)), right(std::move(right)) {}
};

/// @brief Unary operator expression
class UnaryExpr : public Expression {
public:
    TokenType op;
    ExprPtr operand;
    bool prefix;  ///< true for prefix (++x), false for postfix (x++)

    UnaryExpr(TokenType op, ExprPtr operand, bool prefix = true)
        : op(op), operand(std::move(operand)), prefix(prefix) {}
};

/// @brief Function call expression
class CallExpr : public Expression {
public:
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    CallExpr(ExprPtr callee, std::vector<ExprPtr> args)
        : callee(std::move(callee)), arguments(std::move(args)) {}
};

/// @brief Member access expression (a.b)
class MemberExpr : public Expression {
public:
    ExprPtr object;
    std::string member;
    bool optional;  ///< true for a?.b

    MemberExpr(ExprPtr obj, std::string member, bool optional = false)
        : object(std::move(obj)), member(std::move(member)), optional(optional) {}
};

/// @brief Index access expression (a[b])
class IndexExpr : public Expression {
public:
    ExprPtr object;
    ExprPtr index;
    bool optional;  ///< true for a?[b]

    IndexExpr(ExprPtr obj, ExprPtr idx, bool optional = false)
        : object(std::move(obj)), index(std::move(idx)), optional(optional) {}
};

/// @brief Assignment expression
class AssignExpr : public Expression {
public:
    TokenType op;  ///< = or += -= etc.
    ExprPtr target;
    ExprPtr value;

    AssignExpr(TokenType op, ExprPtr target, ExprPtr value)
        : op(op), target(std::move(target)), value(std::move(value)) {}
};

/// @brief Ternary conditional expression (a ? b : c)
class TernaryExpr : public Expression {
public:
    ExprPtr condition;
    ExprPtr then_expr;
    ExprPtr else_expr;

    TernaryExpr(ExprPtr cond, ExprPtr then_e, ExprPtr else_e)
        : condition(std::move(cond)), then_expr(std::move(then_e)), else_expr(std::move(else_e)) {}
};

/// @brief Lambda expression
class LambdaExpr : public Expression {
public:
    struct Parameter {
        std::string name;
        std::optional<std::string> type;
        std::optional<ExprPtr> default_value;
    };

    std::vector<Parameter> parameters;
    std::optional<std::string> return_type;
    std::unique_ptr<class Statement> body;

    LambdaExpr(std::vector<Parameter> params, std::unique_ptr<Statement> body)
        : parameters(std::move(params)), body(std::move(body)) {}
};

/// @brief Array literal expression
class ArrayExpr : public Expression {
public:
    std::vector<ExprPtr> elements;

    explicit ArrayExpr(std::vector<ExprPtr> elems) : elements(std::move(elems)) {}
};

/// @brief Map/object literal expression
class MapExpr : public Expression {
public:
    struct Entry {
        ExprPtr key;
        ExprPtr value;
    };

    std::vector<Entry> entries;

    explicit MapExpr(std::vector<Entry> ents) : entries(std::move(ents)) {}
};

/// @brief New expression (new Class(...))
class NewExpr : public Expression {
public:
    ExprPtr class_expr;
    std::vector<ExprPtr> arguments;

    NewExpr(ExprPtr cls, std::vector<ExprPtr> args)
        : class_expr(std::move(cls)), arguments(std::move(args)) {}
};

/// @brief This expression
class ThisExpr : public Expression {};

/// @brief Super expression
class SuperExpr : public Expression {
public:
    std::string method;
};

/// @brief Await expression
class AwaitExpr : public Expression {
public:
    ExprPtr operand;

    explicit AwaitExpr(ExprPtr operand) : operand(std::move(operand)) {}
};

/// @brief Yield expression
class YieldExpr : public Expression {
public:
    ExprPtr value;
    bool delegate;  ///< yield* for delegation

    explicit YieldExpr(ExprPtr value, bool delegate = false)
        : value(std::move(value)), delegate(delegate) {}
};

/// @brief Range expression (a..b or a..=b)
class RangeExpr : public Expression {
public:
    ExprPtr start;
    ExprPtr end;
    bool inclusive;

    RangeExpr(ExprPtr start, ExprPtr end, bool inclusive)
        : start(std::move(start)), end(std::move(end)), inclusive(inclusive) {}
};

// =============================================================================
// Statements
// =============================================================================

/// @brief Base class for statements
class Statement : public AstNode {
public:
    virtual ~Statement() = default;
};

using StmtPtr = std::unique_ptr<Statement>;

/// @brief Expression statement
class ExprStatement : public Statement {
public:
    ExprPtr expression;

    explicit ExprStatement(ExprPtr expr) : expression(std::move(expr)) {}
};

/// @brief Block statement
class BlockStatement : public Statement {
public:
    std::vector<StmtPtr> statements;

    explicit BlockStatement(std::vector<StmtPtr> stmts = {}) : statements(std::move(stmts)) {}
};

/// @brief If statement
class IfStatement : public Statement {
public:
    ExprPtr condition;
    StmtPtr then_branch;
    StmtPtr else_branch;

    IfStatement(ExprPtr cond, StmtPtr then_b, StmtPtr else_b = nullptr)
        : condition(std::move(cond)), then_branch(std::move(then_b)), else_branch(std::move(else_b)) {}
};

/// @brief While statement
class WhileStatement : public Statement {
public:
    ExprPtr condition;
    StmtPtr body;

    WhileStatement(ExprPtr cond, StmtPtr body)
        : condition(std::move(cond)), body(std::move(body)) {}
};

/// @brief For statement
class ForStatement : public Statement {
public:
    StmtPtr initializer;
    ExprPtr condition;
    ExprPtr increment;
    StmtPtr body;

    ForStatement(StmtPtr init, ExprPtr cond, ExprPtr incr, StmtPtr body)
        : initializer(std::move(init)), condition(std::move(cond)),
          increment(std::move(incr)), body(std::move(body)) {}
};

/// @brief For-each statement
class ForEachStatement : public Statement {
public:
    std::string variable;
    ExprPtr iterable;
    StmtPtr body;

    ForEachStatement(std::string var, ExprPtr iter, StmtPtr body)
        : variable(std::move(var)), iterable(std::move(iter)), body(std::move(body)) {}
};

/// @brief Return statement
class ReturnStatement : public Statement {
public:
    ExprPtr value;

    explicit ReturnStatement(ExprPtr value = nullptr) : value(std::move(value)) {}
};

/// @brief Break statement
class BreakStatement : public Statement {
public:
    std::optional<std::string> label;
};

/// @brief Continue statement
class ContinueStatement : public Statement {
public:
    std::optional<std::string> label;
};

/// @brief Match statement
class MatchStatement : public Statement {
public:
    struct Arm {
        ExprPtr pattern;
        ExprPtr guard;
        StmtPtr body;
    };

    ExprPtr subject;
    std::vector<Arm> arms;

    MatchStatement(ExprPtr subj, std::vector<Arm> arms)
        : subject(std::move(subj)), arms(std::move(arms)) {}
};

/// @brief Try-catch statement
class TryCatchStatement : public Statement {
public:
    struct CatchClause {
        std::string variable;
        std::optional<std::string> type;
        StmtPtr body;
    };

    StmtPtr try_block;
    std::vector<CatchClause> catch_clauses;
    StmtPtr finally_block;

    TryCatchStatement(StmtPtr try_b, std::vector<CatchClause> catches, StmtPtr finally_b = nullptr)
        : try_block(std::move(try_b)), catch_clauses(std::move(catches)),
          finally_block(std::move(finally_b)) {}
};

/// @brief Throw statement
class ThrowStatement : public Statement {
public:
    ExprPtr value;

    explicit ThrowStatement(ExprPtr value) : value(std::move(value)) {}
};

// =============================================================================
// Declarations
// =============================================================================

/// @brief Variable declaration
class VarDecl : public Statement {
public:
    std::string name;
    std::optional<std::string> type;
    ExprPtr initializer;
    bool is_const;
    bool is_pub;

    VarDecl(std::string name, ExprPtr init, bool is_const = false, bool is_pub = false)
        : name(std::move(name)), initializer(std::move(init)), is_const(is_const), is_pub(is_pub) {}
};

/// @brief Function declaration
class FunctionDecl : public Statement {
public:
    struct Parameter {
        std::string name;
        std::optional<std::string> type;
        ExprPtr default_value;
        bool is_variadic = false;
    };

    std::string name;
    std::vector<Parameter> parameters;
    std::optional<std::string> return_type;
    StmtPtr body;
    bool is_async;
    bool is_generator;
    bool is_pub;

    FunctionDecl(std::string name, std::vector<Parameter> params, StmtPtr body)
        : name(std::move(name)), parameters(std::move(params)), body(std::move(body)),
          is_async(false), is_generator(false), is_pub(false) {}
};

/// @brief Class declaration
class ClassDecl : public Statement {
public:
    struct Member {
        std::string name;
        std::optional<std::string> type;
        ExprPtr default_value;
        bool is_pub;
        bool is_static;
    };

    struct Method {
        std::unique_ptr<FunctionDecl> func;
        bool is_static;
        bool is_pub;
    };

    std::string name;
    std::optional<std::string> superclass;
    std::vector<std::string> interfaces;
    std::vector<Member> members;
    std::vector<Method> methods;
    std::unique_ptr<FunctionDecl> constructor;
    bool is_pub;

    explicit ClassDecl(std::string name) : name(std::move(name)), is_pub(false) {}
};

/// @brief Import declaration
class ImportDecl : public Statement {
public:
    struct ImportItem {
        std::string name;
        std::optional<std::string> alias;
    };

    std::string module_path;
    std::vector<ImportItem> items;
    bool import_all;
    std::optional<std::string> alias;  ///< For "import * as alias"
};

/// @brief Export declaration
class ExportDecl : public Statement {
public:
    StmtPtr declaration;
    std::optional<std::string> alias;
};

/// @brief Module declaration
class ModuleDecl : public Statement {
public:
    std::string name;
    std::vector<StmtPtr> statements;

    ModuleDecl(std::string name, std::vector<StmtPtr> stmts)
        : name(std::move(name)), statements(std::move(stmts)) {}
};

// =============================================================================
// Program
// =============================================================================

/// @brief Root of the AST
class Program : public AstNode {
public:
    std::vector<StmtPtr> statements;

    explicit Program(std::vector<StmtPtr> stmts = {}) : statements(std::move(stmts)) {}
};

} // namespace void_script
