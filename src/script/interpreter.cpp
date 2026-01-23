#include "interpreter.hpp"
#include "parser.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace void_script {

// =============================================================================
// Environment Implementation
// =============================================================================

Environment::Environment(Environment* enclosing)
    : enclosing_(enclosing) {}

void Environment::define(const std::string& name, Value value) {
    variables_[name] = std::move(value);
}

Value Environment::get(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    if (enclosing_) {
        return enclosing_->get(name);
    }
    throw ScriptException(ScriptError::UndefinedVariable, "Undefined variable: " + name);
}

void Environment::assign(const std::string& name, Value value) {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        it->second = std::move(value);
        return;
    }
    if (enclosing_) {
        enclosing_->assign(name, std::move(value));
        return;
    }
    throw ScriptException(ScriptError::UndefinedVariable, "Undefined variable: " + name);
}

bool Environment::contains(const std::string& name) const {
    if (variables_.count(name)) return true;
    if (enclosing_) return enclosing_->contains(name);
    return false;
}

// =============================================================================
// ScriptFunction Implementation
// =============================================================================

ScriptFunction::ScriptFunction(const FunctionDecl* decl, Environment* closure, bool is_method)
    : declaration_(decl), closure_(closure), is_method_(is_method) {}

std::size_t ScriptFunction::arity() const {
    return declaration_->parameters.size();
}

std::string ScriptFunction::name() const {
    return declaration_->name;
}

std::string ScriptFunction::to_string() const {
    return "<fn " + declaration_->name + ">";
}

Value ScriptFunction::call(Interpreter& interp, const std::vector<Value>& args) {
    Environment env(closure_);

    // Bind parameters
    for (std::size_t i = 0; i < declaration_->parameters.size(); ++i) {
        const auto& param = declaration_->parameters[i];
        Value value;

        if (i < args.size()) {
            value = args[i];
        } else if (param.default_value) {
            value = interp.evaluate(*param.default_value);
        }

        env.define(param.name, std::move(value));
    }

    // Bind 'this' for methods
    if (bound_instance_) {
        env.define("this", Value::make_object(bound_instance_));
    }

    try {
        interp.execute_block(
            static_cast<const BlockStatement*>(declaration_->body.get())->statements,
            &env);
    } catch (const ReturnException& ret) {
        return ret.value;
    }

    return Value(nullptr);
}

std::shared_ptr<ScriptFunction> ScriptFunction::bind(std::shared_ptr<ClassInstance> instance) {
    auto bound = std::make_shared<ScriptFunction>(declaration_, closure_, true);
    bound->bound_instance_ = std::move(instance);
    return bound;
}

// =============================================================================
// ScriptClass Implementation
// =============================================================================

ScriptClass::ScriptClass(const ClassDecl* decl, std::shared_ptr<ScriptClass> superclass)
    : name_(decl->name), declaration_(decl), superclass_(std::move(superclass)) {}

std::size_t ScriptClass::arity() const {
    auto init = find_method("init");
    if (init) return init->arity();
    return 0;
}

std::string ScriptClass::to_string() const {
    return "<class " + name_ + ">";
}

Value ScriptClass::call(Interpreter& interp, const std::vector<Value>& args) {
    auto instance = std::make_shared<ClassInstance>(
        std::static_pointer_cast<ScriptClass>(shared_from_this()));

    // Initialize members with defaults
    for (const auto& member : declaration_->members) {
        if (member.default_value) {
            instance->set_property(member.name, interp.evaluate(*member.default_value));
        }
    }

    // Call constructor
    auto init = find_method("init");
    if (init) {
        init->bind(instance)->call(interp, args);
    }

    return Value::make_object(instance);
}

std::shared_ptr<ScriptFunction> ScriptClass::find_method(const std::string& name) const {
    auto it = methods_.find(name);
    if (it != methods_.end()) {
        return it->second;
    }
    if (superclass_) {
        return superclass_->find_method(name);
    }
    return nullptr;
}

void ScriptClass::add_method(const std::string& name, std::shared_ptr<ScriptFunction> method) {
    methods_[name] = std::move(method);
}

// =============================================================================
// ClassInstance Implementation
// =============================================================================

ClassInstance::ClassInstance(std::shared_ptr<ScriptClass> klass)
    : class_(std::move(klass)) {}

std::string ClassInstance::to_string() const {
    return "<" + class_->name() + " instance>";
}

bool ClassInstance::has_property(const std::string& name) const {
    if (Object::has_property(name)) return true;
    return class_->find_method(name) != nullptr;
}

Value ClassInstance::get_property(const std::string& name) const {
    if (Object::has_property(name)) {
        return Object::get_property(name);
    }

    auto method = class_->find_method(name);
    if (method) {
        // Bind the method to this instance
        auto bound = method->bind(std::const_pointer_cast<ClassInstance>(
            std::static_pointer_cast<const ClassInstance>(shared_from_this())));
        return Value::make_function(bound);
    }

    throw ScriptException(ScriptError::UndefinedProperty, "Undefined property: " + name);
}

void ClassInstance::set_property(const std::string& name, Value value) {
    Object::set_property(name, std::move(value));
}

// =============================================================================
// Interpreter Implementation
// =============================================================================

Interpreter::Interpreter() {
    globals_ = std::make_unique<Environment>();
    current_env_ = globals_.get();
    register_stdlib();
}

Interpreter::~Interpreter() = default;

Value Interpreter::execute(const Program& program) {
    start_time_ = std::chrono::steady_clock::now();
    Value result;

    for (const auto& stmt : program.statements) {
        result = execute(*stmt);
    }

    return result;
}

Value Interpreter::execute(const Statement& stmt) {
    check_timeout();

    if (auto* expr_stmt = dynamic_cast<const ExprStatement*>(&stmt)) {
        visit(*expr_stmt);
    } else if (auto* block = dynamic_cast<const BlockStatement*>(&stmt)) {
        visit(*block);
    } else if (auto* if_stmt = dynamic_cast<const IfStatement*>(&stmt)) {
        visit(*if_stmt);
    } else if (auto* while_stmt = dynamic_cast<const WhileStatement*>(&stmt)) {
        visit(*while_stmt);
    } else if (auto* for_stmt = dynamic_cast<const ForStatement*>(&stmt)) {
        visit(*for_stmt);
    } else if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(&stmt)) {
        visit(*foreach_stmt);
    } else if (auto* ret = dynamic_cast<const ReturnStatement*>(&stmt)) {
        visit(*ret);
    } else if (auto* brk = dynamic_cast<const BreakStatement*>(&stmt)) {
        visit(*brk);
    } else if (auto* cont = dynamic_cast<const ContinueStatement*>(&stmt)) {
        visit(*cont);
    } else if (auto* var = dynamic_cast<const VarDecl*>(&stmt)) {
        visit(*var);
    } else if (auto* func = dynamic_cast<const FunctionDecl*>(&stmt)) {
        visit(*func);
    } else if (auto* cls = dynamic_cast<const ClassDecl*>(&stmt)) {
        visit(*cls);
    }

    return Value(nullptr);
}

Value Interpreter::evaluate(const Expression& expr) {
    check_timeout();

    if (auto* lit = dynamic_cast<const LiteralExpr*>(&expr)) {
        return visit(*lit);
    } else if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        return visit(*id);
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        return visit(*bin);
    } else if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        return visit(*un);
    } else if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        return visit(*call);
    } else if (auto* mem = dynamic_cast<const MemberExpr*>(&expr)) {
        return visit(*mem);
    } else if (auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        return visit(*idx);
    } else if (auto* assign = dynamic_cast<const AssignExpr*>(&expr)) {
        return visit(*assign);
    } else if (auto* ternary = dynamic_cast<const TernaryExpr*>(&expr)) {
        return visit(*ternary);
    } else if (auto* arr = dynamic_cast<const ArrayExpr*>(&expr)) {
        return visit(*arr);
    } else if (auto* map = dynamic_cast<const MapExpr*>(&expr)) {
        return visit(*map);
    } else if (auto* this_expr = dynamic_cast<const ThisExpr*>(&expr)) {
        return visit(*this_expr);
    }

    return Value(nullptr);
}

Value Interpreter::run(std::string_view source, std::string_view filename) {
    Parser parser(source, filename);
    auto program = parser.parse_program();

    if (parser.has_errors()) {
        for (const auto& err : parser.errors()) {
            if (error_callback_) {
                error_callback_(err);
            }
        }
        return Value(nullptr);
    }

    return execute(*program);
}

Value Interpreter::run_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw ScriptException(ScriptError::FileNotFound, "File not found: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return run(buffer.str(), path);
}

void Interpreter::push_scope() {
    auto env = std::make_unique<Environment>(current_env_);
    current_env_ = env.get();
    scopes_.push_back(std::move(env));
}

void Interpreter::pop_scope() {
    if (!scopes_.empty()) {
        current_env_ = scopes_.back()->enclosing();
        scopes_.pop_back();
    }
}

Value Interpreter::execute_block(const std::vector<StmtPtr>& statements, Environment* env) {
    Environment* previous = current_env_;
    current_env_ = env;

    Value result;
    try {
        for (const auto& stmt : statements) {
            result = execute(*stmt);
        }
    } catch (...) {
        current_env_ = previous;
        throw;
    }

    current_env_ = previous;
    return result;
}

void Interpreter::define_native(const std::string& name, std::size_t arity, NativeFunction::Func func) {
    auto native = std::make_shared<NativeFunction>(name, arity, std::move(func));
    globals_->define(name, Value::make_function(native));
}

void Interpreter::define_constant(const std::string& name, Value value) {
    globals_->define(name, std::move(value));
}

void Interpreter::register_stdlib() {
    // Print function
    define_native("print", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (!args.empty()) {
            print(args[0].to_string());
        }
        return Value(nullptr);
    });

    // Type checking
    define_native("typeof", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value("null");
        return Value(args[0].type_name());
    });

    // String functions
    define_native("len", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0);
        if (args[0].is_string()) {
            return Value(static_cast<std::int64_t>(args[0].as_string().size()));
        }
        if (args[0].is_array()) {
            return Value(static_cast<std::int64_t>(args[0].as_array().size()));
        }
        return Value(0);
    });

    // Math functions
    define_native("abs", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::abs(args[0].as_number()));
    });

    define_native("sqrt", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::sqrt(args[0].as_number()));
    });

    define_native("floor", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::floor(args[0].as_number()));
    });

    define_native("ceil", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::ceil(args[0].as_number()));
    });

    define_native("sin", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::sin(args[0].as_number()));
    });

    define_native("cos", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::cos(args[0].as_number()));
    });

    // Array functions
    define_native("push", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array()) return Value(nullptr);
        const_cast<ValueArray&>(args[0].as_array()).push_back(args[1]);
        return Value(nullptr);
    });

    define_native("pop", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value(nullptr);
        auto& arr = const_cast<ValueArray&>(args[0].as_array());
        if (arr.empty()) return Value(nullptr);
        Value last = arr.back();
        arr.pop_back();
        return last;
    });

    // Assertions
    define_native("assert", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_truthy()) {
            std::string message = args.size() > 1 ? args[1].to_string() : "Assertion failed";
            throw ScriptException(ScriptError::AssertionFailed, message);
        }
        return Value(nullptr);
    });
}

std::string Interpreter::format_stack_trace() const {
    std::ostringstream ss;
    for (auto it = call_stack_.rbegin(); it != call_stack_.rend(); ++it) {
        ss << "  at " << it->function_name;
        ss << " (" << it->call_site.to_string() << ")\n";
    }
    return ss.str();
}

void Interpreter::print(const std::string& text) {
    if (print_callback_) {
        print_callback_(text);
    } else {
        std::cout << text << std::endl;
    }
}

void Interpreter::check_timeout() {
    if (timeout_.count() > 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        if (elapsed > timeout_) {
            throw ScriptException(ScriptError::Timeout, "Execution timeout");
        }
    }
}

// =============================================================================
// Expression Visitors
// =============================================================================

Value Interpreter::visit(const LiteralExpr& expr) {
    return expr.value;
}

Value Interpreter::visit(const IdentifierExpr& expr) {
    return current_env_->get(expr.name);
}

Value Interpreter::visit(const BinaryExpr& expr) {
    Value left = evaluate(*expr.left);

    // Short-circuit for logical operators
    if (expr.op == TokenType::And) {
        if (!left.is_truthy()) return Value(false);
        return Value(evaluate(*expr.right).is_truthy());
    }
    if (expr.op == TokenType::Or) {
        if (left.is_truthy()) return Value(true);
        return Value(evaluate(*expr.right).is_truthy());
    }

    Value right = evaluate(*expr.right);

    switch (expr.op) {
        // Arithmetic
        case TokenType::Plus:
            if (left.is_string() || right.is_string()) {
                return Value(left.to_string() + right.to_string());
            }
            return Value(left.as_number() + right.as_number());
        case TokenType::Minus:
            return Value(left.as_number() - right.as_number());
        case TokenType::Star:
            return Value(left.as_number() * right.as_number());
        case TokenType::Slash:
            if (right.as_number() == 0) {
                throw ScriptException(ScriptError::DivisionByZero, "Division by zero");
            }
            return Value(left.as_number() / right.as_number());
        case TokenType::Percent:
            return Value(std::fmod(left.as_number(), right.as_number()));
        case TokenType::Power:
            return Value(std::pow(left.as_number(), right.as_number()));

        // Comparison
        case TokenType::Equal:
            return Value(left.equals(right));
        case TokenType::NotEqual:
            return Value(!left.equals(right));
        case TokenType::Less:
            return Value(left < right);
        case TokenType::LessEqual:
            return Value(left <= right);
        case TokenType::Greater:
            return Value(left > right);
        case TokenType::GreaterEqual:
            return Value(left >= right);

        // Bitwise
        case TokenType::Ampersand:
            return Value(left.as_int() & right.as_int());
        case TokenType::Pipe:
            return Value(left.as_int() | right.as_int());
        case TokenType::Caret:
            return Value(left.as_int() ^ right.as_int());
        case TokenType::ShiftLeft:
            return Value(left.as_int() << right.as_int());
        case TokenType::ShiftRight:
            return Value(left.as_int() >> right.as_int());

        // Null coalesce
        case TokenType::QuestionQuestion:
            return left.is_null() ? right : left;

        default:
            throw ScriptException(ScriptError::InvalidOperation, "Unknown binary operator");
    }
}

Value Interpreter::visit(const UnaryExpr& expr) {
    Value operand = evaluate(*expr.operand);

    switch (expr.op) {
        case TokenType::Minus:
            return Value(-operand.as_number());
        case TokenType::Not:
            return Value(!operand.is_truthy());
        case TokenType::Tilde:
            return Value(~operand.as_int());
        case TokenType::Increment:
            // TODO: Modify variable
            return Value(operand.as_number() + 1);
        case TokenType::Decrement:
            return Value(operand.as_number() - 1);
        default:
            throw ScriptException(ScriptError::InvalidOperation, "Unknown unary operator");
    }
}

Value Interpreter::visit(const CallExpr& expr) {
    Value callee = evaluate(*expr.callee);

    std::vector<Value> args;
    for (const auto& arg : expr.arguments) {
        args.push_back(evaluate(*arg));
    }

    return call_value(std::move(callee), args);
}

Value Interpreter::call_value(Value callee, const std::vector<Value>& args) {
    if (!callee.is_callable()) {
        throw ScriptException(ScriptError::NotCallable, "Value is not callable");
    }

    Callable* callable = callee.as_callable();
    if (!callable) {
        throw ScriptException(ScriptError::NotCallable, "Value is not callable");
    }

    if (args.size() < callable->arity()) {
        throw ScriptException(ScriptError::WrongArgumentCount,
                              "Expected " + std::to_string(callable->arity()) +
                              " arguments but got " + std::to_string(args.size()));
    }

    // Check recursion limit
    if (call_stack_.size() >= max_depth_) {
        throw ScriptException(ScriptError::StackOverflow, "Stack overflow");
    }

    CallFrame frame;
    frame.function_name = callable->name();
    call_stack_.push_back(frame);

    Value result;
    try {
        result = callable->call(*this, args);
    } catch (...) {
        call_stack_.pop_back();
        throw;
    }

    call_stack_.pop_back();
    return result;
}

Value Interpreter::visit(const MemberExpr& expr) {
    Value object = evaluate(*expr.object);

    if (expr.optional && object.is_null()) {
        return Value(nullptr);
    }

    if (object.is_map()) {
        auto& map = object.as_map();
        auto it = map.find(expr.member);
        if (it != map.end()) {
            return it->second;
        }
        return Value(nullptr);
    }

    Object* obj = object.as_object();
    if (!obj) {
        throw ScriptException(ScriptError::NullReference, "Cannot access property of null");
    }

    return obj->get_property(expr.member);
}

Value Interpreter::visit(const IndexExpr& expr) {
    Value object = evaluate(*expr.object);
    Value index = evaluate(*expr.index);

    if (expr.optional && object.is_null()) {
        return Value(nullptr);
    }

    if (object.is_array()) {
        std::int64_t idx = index.as_int();
        const auto& arr = object.as_array();
        if (idx < 0 || static_cast<std::size_t>(idx) >= arr.size()) {
            throw ScriptException(ScriptError::IndexOutOfBounds, "Index out of bounds");
        }
        return arr[idx];
    }

    if (object.is_map() && index.is_string()) {
        const auto& map = object.as_map();
        auto it = map.find(index.as_string());
        if (it != map.end()) {
            return it->second;
        }
        return Value(nullptr);
    }

    if (object.is_string()) {
        std::int64_t idx = index.as_int();
        const auto& str = object.as_string();
        if (idx < 0 || static_cast<std::size_t>(idx) >= str.size()) {
            throw ScriptException(ScriptError::IndexOutOfBounds, "Index out of bounds");
        }
        return Value(std::string(1, str[idx]));
    }

    throw ScriptException(ScriptError::NotIndexable, "Value is not indexable");
}

Value Interpreter::visit(const AssignExpr& expr) {
    Value value = evaluate(*expr.value);

    if (auto* id = dynamic_cast<const IdentifierExpr*>(expr.target.get())) {
        if (expr.op != TokenType::Assign) {
            Value current = current_env_->get(id->name);
            switch (expr.op) {
                case TokenType::PlusAssign:
                    value = Value(current.as_number() + value.as_number());
                    break;
                case TokenType::MinusAssign:
                    value = Value(current.as_number() - value.as_number());
                    break;
                case TokenType::StarAssign:
                    value = Value(current.as_number() * value.as_number());
                    break;
                case TokenType::SlashAssign:
                    value = Value(current.as_number() / value.as_number());
                    break;
                default:
                    break;
            }
        }
        current_env_->assign(id->name, value);
        return value;
    }

    throw ScriptException(ScriptError::InvalidAssignmentTarget, "Invalid assignment target");
}

Value Interpreter::visit(const TernaryExpr& expr) {
    Value condition = evaluate(*expr.condition);
    if (condition.is_truthy()) {
        return evaluate(*expr.then_expr);
    }
    return evaluate(*expr.else_expr);
}

Value Interpreter::visit(const ArrayExpr& expr) {
    ValueArray elements;
    for (const auto& elem : expr.elements) {
        elements.push_back(evaluate(*elem));
    }
    return Value::make_array(std::move(elements));
}

Value Interpreter::visit(const MapExpr& expr) {
    ValueMap map;
    for (const auto& entry : expr.entries) {
        std::string key;
        if (auto* lit = dynamic_cast<const LiteralExpr*>(entry.key.get())) {
            key = lit->value.to_string();
        } else {
            key = evaluate(*entry.key).to_string();
        }
        map[key] = evaluate(*entry.value);
    }
    return Value::make_map(std::move(map));
}

Value Interpreter::visit(const ThisExpr& expr) {
    if (!current_instance_) {
        throw ScriptException(ScriptError::InvalidOperation, "'this' used outside of method");
    }
    return Value::make_object(current_instance_);
}

// =============================================================================
// Statement Visitors
// =============================================================================

void Interpreter::visit(const ExprStatement& stmt) {
    evaluate(*stmt.expression);
}

void Interpreter::visit(const BlockStatement& stmt) {
    push_scope();
    try {
        for (const auto& s : stmt.statements) {
            execute(*s);
        }
    } catch (...) {
        pop_scope();
        throw;
    }
    pop_scope();
}

void Interpreter::visit(const IfStatement& stmt) {
    Value condition = evaluate(*stmt.condition);
    if (condition.is_truthy()) {
        execute(*stmt.then_branch);
    } else if (stmt.else_branch) {
        execute(*stmt.else_branch);
    }
}

void Interpreter::visit(const WhileStatement& stmt) {
    while (evaluate(*stmt.condition).is_truthy()) {
        try {
            execute(*stmt.body);
        } catch (const BreakException&) {
            break;
        } catch (const ContinueException&) {
            continue;
        }
    }
}

void Interpreter::visit(const ForStatement& stmt) {
    push_scope();

    if (stmt.initializer) {
        execute(*stmt.initializer);
    }

    try {
        while (!stmt.condition || evaluate(*stmt.condition).is_truthy()) {
            try {
                execute(*stmt.body);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                // Fall through to increment
            }

            if (stmt.increment) {
                evaluate(*stmt.increment);
            }
        }
    } catch (...) {
        pop_scope();
        throw;
    }

    pop_scope();
}

void Interpreter::visit(const ForEachStatement& stmt) {
    Value iterable = evaluate(*stmt.iterable);

    if (!iterable.is_array()) {
        throw ScriptException(ScriptError::NotIterable, "Value is not iterable");
    }

    push_scope();
    try {
        for (const auto& item : iterable.as_array()) {
            current_env_->define(stmt.variable, item);
            try {
                execute(*stmt.body);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
    } catch (...) {
        pop_scope();
        throw;
    }
    pop_scope();
}

void Interpreter::visit(const ReturnStatement& stmt) {
    Value value;
    if (stmt.value) {
        value = evaluate(*stmt.value);
    }
    throw ReturnException(std::move(value));
}

void Interpreter::visit(const BreakStatement& stmt) {
    throw BreakException();
}

void Interpreter::visit(const ContinueStatement& stmt) {
    throw ContinueException();
}

void Interpreter::visit(const VarDecl& decl) {
    Value value;
    if (decl.initializer) {
        value = evaluate(*decl.initializer);
    }
    current_env_->define(decl.name, std::move(value));
}

void Interpreter::visit(const FunctionDecl& decl) {
    auto function = std::make_shared<ScriptFunction>(&decl, current_env_);
    current_env_->define(decl.name, Value::make_function(function));
}

void Interpreter::visit(const ClassDecl& decl) {
    std::shared_ptr<ScriptClass> superclass;
    if (decl.superclass) {
        Value super = current_env_->get(*decl.superclass);
        // TODO: Validate superclass
    }

    auto klass = std::make_shared<ScriptClass>(&decl, superclass);

    // Add methods
    for (const auto& method : decl.methods) {
        auto func = std::make_shared<ScriptFunction>(method.func.get(), current_env_, true);
        klass->add_method(method.func->name, func);
    }

    current_env_->define(decl.name, Value::make_function(klass));
}

// =============================================================================
// ScriptContext Implementation
// =============================================================================

ScriptContext::ScriptContext() = default;

Value ScriptContext::run(std::string_view source, std::string_view filename) {
    return interpreter_.run(source, filename);
}

void ScriptContext::set_global(const std::string& name, Value value) {
    interpreter_.globals().define(name, std::move(value));
}

Value ScriptContext::get_global(const std::string& name) {
    return interpreter_.globals().get(name);
}

void ScriptContext::register_function(const std::string& name, std::size_t arity,
                                       NativeFunction::Func func) {
    interpreter_.define_native(name, arity, std::move(func));
}

} // namespace void_script
