#pragma once

/// @file interpreter.hpp
/// @brief Tree-walking interpreter for VoidScript

#include "ast.hpp"
#include "parser.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_script {

// =============================================================================
// Environment
// =============================================================================

/// @brief Variable environment scope
class Environment {
public:
    explicit Environment(Environment* enclosing = nullptr);

    /// @brief Define a new variable
    void define(const std::string& name, Value value);

    /// @brief Get a variable
    [[nodiscard]] Value get(const std::string& name) const;

    /// @brief Assign to an existing variable
    void assign(const std::string& name, Value value);

    /// @brief Check if variable exists
    [[nodiscard]] bool contains(const std::string& name) const;

    /// @brief Get enclosing environment
    [[nodiscard]] Environment* enclosing() const { return enclosing_; }

    /// @brief Get all variables
    [[nodiscard]] const std::unordered_map<std::string, Value>& variables() const { return variables_; }

private:
    std::unordered_map<std::string, Value> variables_;
    Environment* enclosing_;
};

// =============================================================================
// Script Function
// =============================================================================

/// @brief User-defined script function
class ScriptFunction : public Callable {
public:
    ScriptFunction(const FunctionDecl* decl, Environment* closure, bool is_method = false);

    [[nodiscard]] std::size_t arity() const override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] Value call(Interpreter& interp, const std::vector<Value>& args) override;

    /// @brief Bind 'this' for methods
    [[nodiscard]] std::shared_ptr<ScriptFunction> bind(std::shared_ptr<ClassInstance> instance);

private:
    const FunctionDecl* declaration_;
    Environment* closure_;
    bool is_method_;
    std::shared_ptr<ClassInstance> bound_instance_;
};

// =============================================================================
// Script Class
// =============================================================================

/// @brief User-defined script class
class ScriptClass : public Callable, public std::enable_shared_from_this<ScriptClass> {
public:
    explicit ScriptClass(const ClassDecl* decl, std::shared_ptr<ScriptClass> superclass = nullptr);

    [[nodiscard]] std::size_t arity() const override;
    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] Value call(Interpreter& interp, const std::vector<Value>& args) override;

    /// @brief Find a method
    [[nodiscard]] std::shared_ptr<ScriptFunction> find_method(const std::string& name) const;

    /// @brief Get superclass
    [[nodiscard]] std::shared_ptr<ScriptClass> superclass() const { return superclass_; }

    /// @brief Add a method
    void add_method(const std::string& name, std::shared_ptr<ScriptFunction> method);

private:
    std::string name_;
    const ClassDecl* declaration_;
    std::shared_ptr<ScriptClass> superclass_;
    std::unordered_map<std::string, std::shared_ptr<ScriptFunction>> methods_;
};

// =============================================================================
// Class Instance
// =============================================================================

/// @brief Instance of a script class
class ClassInstance : public Object, public std::enable_shared_from_this<ClassInstance> {
public:
    explicit ClassInstance(std::shared_ptr<ScriptClass> klass);

    [[nodiscard]] ValueType object_type() const override { return ValueType::Object; }
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] bool has_property(const std::string& name) const override;
    [[nodiscard]] Value get_property(const std::string& name) const override;
    void set_property(const std::string& name, Value value) override;

    [[nodiscard]] std::shared_ptr<ScriptClass> get_class() const { return class_; }

private:
    std::shared_ptr<ScriptClass> class_;
};

// =============================================================================
// Call Frame
// =============================================================================

/// @brief Call stack frame
struct CallFrame {
    const FunctionDecl* function = nullptr;
    Environment* environment = nullptr;
    SourceLocation call_site;
    std::string function_name;
};

// =============================================================================
// Control Flow Exceptions
// =============================================================================

/// @brief Return value exception
class ReturnException {
public:
    Value value;
    explicit ReturnException(Value v) : value(std::move(v)) {}
};

/// @brief Break exception
class BreakException {
public:
    std::optional<std::string> label;
};

/// @brief Continue exception
class ContinueException {
public:
    std::optional<std::string> label;
};

// =============================================================================
// Interpreter
// =============================================================================

/// @brief Tree-walking interpreter for VoidScript
class Interpreter {
public:
    Interpreter();
    ~Interpreter();

    // ==========================================================================
    // Execution
    // ==========================================================================

    /// @brief Execute a program
    Value execute(const Program& program);

    /// @brief Execute a statement
    Value execute(const Statement& stmt);

    /// @brief Evaluate an expression
    Value evaluate(const Expression& expr);

    /// @brief Execute source code
    Value run(std::string_view source, std::string_view filename = "<script>");

    /// @brief Execute a file
    Value run_file(const std::string& path);

    // ==========================================================================
    // Environment
    // ==========================================================================

    /// @brief Get global environment
    [[nodiscard]] Environment& globals() { return *globals_; }

    /// @brief Get current environment
    [[nodiscard]] Environment& current_env() { return *current_env_; }

    /// @brief Push a new scope
    void push_scope();

    /// @brief Pop the current scope
    void pop_scope();

    /// @brief Execute block in new scope
    Value execute_block(const std::vector<StmtPtr>& statements, Environment* env);

    // ==========================================================================
    // Native Bindings
    // ==========================================================================

    /// @brief Define a native function
    void define_native(const std::string& name, std::size_t arity, NativeFunction::Func func);

    /// @brief Define a native constant
    void define_constant(const std::string& name, Value value);

    /// @brief Register standard library
    void register_stdlib();

    // ==========================================================================
    // Call Stack
    // ==========================================================================

    /// @brief Get call stack
    [[nodiscard]] const std::vector<CallFrame>& call_stack() const { return call_stack_; }

    /// @brief Get current depth
    [[nodiscard]] std::size_t depth() const { return call_stack_.size(); }

    /// @brief Format stack trace
    [[nodiscard]] std::string format_stack_trace() const;

    // ==========================================================================
    // Configuration
    // ==========================================================================

    /// @brief Set max recursion depth
    void set_max_depth(std::size_t depth) { max_depth_ = depth; }

    /// @brief Set execution timeout
    void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }

    /// @brief Enable/disable debug mode
    void set_debug(bool enabled) { debug_mode_ = enabled; }

    // ==========================================================================
    // Callbacks
    // ==========================================================================

    using PrintCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const ScriptException&)>;

    void set_print_callback(PrintCallback cb) { print_callback_ = std::move(cb); }
    void set_error_callback(ErrorCallback cb) { error_callback_ = std::move(cb); }

    /// @brief Print a value
    void print(const std::string& text);

private:
    // Expression evaluation
    Value visit(const LiteralExpr& expr);
    Value visit(const IdentifierExpr& expr);
    Value visit(const BinaryExpr& expr);
    Value visit(const UnaryExpr& expr);
    Value visit(const CallExpr& expr);
    Value visit(const MemberExpr& expr);
    Value visit(const IndexExpr& expr);
    Value visit(const AssignExpr& expr);
    Value visit(const TernaryExpr& expr);
    Value visit(const LambdaExpr& expr);
    Value visit(const ArrayExpr& expr);
    Value visit(const MapExpr& expr);
    Value visit(const NewExpr& expr);
    Value visit(const ThisExpr& expr);
    Value visit(const SuperExpr& expr);

    // Statement execution
    void visit(const ExprStatement& stmt);
    void visit(const BlockStatement& stmt);
    void visit(const IfStatement& stmt);
    void visit(const WhileStatement& stmt);
    void visit(const ForStatement& stmt);
    void visit(const ForEachStatement& stmt);
    void visit(const ReturnStatement& stmt);
    void visit(const BreakStatement& stmt);
    void visit(const ContinueStatement& stmt);
    void visit(const MatchStatement& stmt);
    void visit(const TryCatchStatement& stmt);
    void visit(const ThrowStatement& stmt);

    // Declaration execution
    void visit(const VarDecl& decl);
    void visit(const FunctionDecl& decl);
    void visit(const ClassDecl& decl);
    void visit(const ImportDecl& decl);
    void visit(const ExportDecl& decl);

    // Helpers
    Value call_value(Value callee, const std::vector<Value>& args);
    void check_operands(TokenType op, const Value& left, const Value& right);
    void check_timeout();

    // State
    std::unique_ptr<Environment> globals_;
    Environment* current_env_;
    std::vector<std::unique_ptr<Environment>> scopes_;

    std::vector<CallFrame> call_stack_;
    std::size_t max_depth_ = 1000;

    std::chrono::milliseconds timeout_{0};
    std::chrono::steady_clock::time_point start_time_;

    bool debug_mode_ = false;

    PrintCallback print_callback_;
    ErrorCallback error_callback_;

    // For 'this' binding
    std::shared_ptr<ClassInstance> current_instance_;
};

// =============================================================================
// Script Context
// =============================================================================

/// @brief Script execution context
class ScriptContext {
public:
    ScriptContext();

    /// @brief Get the interpreter
    [[nodiscard]] Interpreter& interpreter() { return interpreter_; }

    /// @brief Execute code
    Value run(std::string_view source, std::string_view filename = "<script>");

    /// @brief Set a global variable
    void set_global(const std::string& name, Value value);

    /// @brief Get a global variable
    [[nodiscard]] Value get_global(const std::string& name);

    /// @brief Register a native function
    void register_function(const std::string& name, std::size_t arity, NativeFunction::Func func);

private:
    Interpreter interpreter_;
};

} // namespace void_script
