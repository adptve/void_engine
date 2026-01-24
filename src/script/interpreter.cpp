#include "interpreter.hpp"
#include "parser.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <set>
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
    } else if (auto* match = dynamic_cast<const MatchStatement*>(&stmt)) {
        visit(*match);
    } else if (auto* try_catch = dynamic_cast<const TryCatchStatement*>(&stmt)) {
        visit(*try_catch);
    } else if (auto* throw_stmt = dynamic_cast<const ThrowStatement*>(&stmt)) {
        visit(*throw_stmt);
    } else if (auto* import_decl = dynamic_cast<const ImportDecl*>(&stmt)) {
        visit(*import_decl);
    } else if (auto* export_decl = dynamic_cast<const ExportDecl*>(&stmt)) {
        visit(*export_decl);
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
    } else if (auto* super_expr = dynamic_cast<const SuperExpr*>(&expr)) {
        return visit(*super_expr);
    } else if (auto* lambda = dynamic_cast<const LambdaExpr*>(&expr)) {
        return visit(*lambda);
    } else if (auto* new_expr = dynamic_cast<const NewExpr*>(&expr)) {
        return visit(*new_expr);
    } else if (auto* range = dynamic_cast<const RangeExpr*>(&expr)) {
        return visit(*range);
    } else if (auto* await_expr = dynamic_cast<const AwaitExpr*>(&expr)) {
        return visit(*await_expr);
    } else if (auto* yield_expr = dynamic_cast<const YieldExpr*>(&expr)) {
        return visit(*yield_expr);
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
    // =========================================================================
    // I/O Functions
    // =========================================================================

    // print - output to console (variadic)
    define_native("print", 0, [this](Interpreter&, const std::vector<Value>& args) {
        std::string output;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) output += " ";
            output += args[i].to_string();
        }
        print(output);
        return Value(nullptr);
    });

    // println - print with newline
    define_native("println", 0, [this](Interpreter&, const std::vector<Value>& args) {
        std::string output;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) output += " ";
            output += args[i].to_string();
        }
        print(output);
        return Value(nullptr);
    });

    // debug - debug representation
    define_native("debug", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (!args.empty()) {
            print("[DEBUG] " + args[0].type_name() + ": " + args[0].to_string());
        }
        return Value(nullptr);
    });

    // =========================================================================
    // Type Functions
    // =========================================================================

    define_native("typeof", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value("null");
        return Value(args[0].type_name());
    });

    define_native("type", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value("null");
        return Value(args[0].type_name());
    });

    // Type conversions
    define_native("str", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value("");
        return Value(args[0].to_string());
    });

    define_native("int", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(static_cast<std::int64_t>(0));
        return Value(args[0].as_int());
    });

    define_native("float", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(args[0].as_number());
    });

    define_native("bool", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        return Value(args[0].is_truthy());
    });

    // Type checks
    define_native("is_null", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(args.empty() || args[0].is_null());
    });

    define_native("is_bool", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(!args.empty() && args[0].is_bool());
    });

    define_native("is_int", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(!args.empty() && args[0].is_int());
    });

    define_native("is_float", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(!args.empty() && args[0].is_float());
    });

    define_native("is_number", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(!args.empty() && args[0].is_number());
    });

    define_native("is_string", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(!args.empty() && args[0].is_string());
    });

    define_native("is_array", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(!args.empty() && args[0].is_array());
    });

    define_native("is_object", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(!args.empty() && (args[0].is_map() || args[0].is_object()));
    });

    define_native("is_function", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(!args.empty() && args[0].is_callable());
    });

    define_native("is_callable", 1, [](Interpreter&, const std::vector<Value>& args) {
        return Value(!args.empty() && args[0].is_callable());
    });

    // =========================================================================
    // Collection Functions
    // =========================================================================

    define_native("len", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(static_cast<std::int64_t>(0));
        if (args[0].is_string()) {
            return Value(static_cast<std::int64_t>(args[0].as_string().size()));
        }
        if (args[0].is_array()) {
            return Value(static_cast<std::int64_t>(args[0].as_array().size()));
        }
        if (args[0].is_map()) {
            return Value(static_cast<std::int64_t>(args[0].as_map().size()));
        }
        return Value(static_cast<std::int64_t>(0));
    });

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

    define_native("first", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value(nullptr);
        const auto& arr = args[0].as_array();
        return arr.empty() ? Value(nullptr) : arr.front();
    });

    define_native("last", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value(nullptr);
        const auto& arr = args[0].as_array();
        return arr.empty() ? Value(nullptr) : arr.back();
    });

    define_native("keys", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_map()) return Value::make_array({});
        ValueArray keys;
        for (const auto& [k, v] : args[0].as_map()) {
            keys.push_back(Value(k));
        }
        return Value::make_array(std::move(keys));
    });

    define_native("values", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_map()) return Value::make_array({});
        ValueArray values;
        for (const auto& [k, v] : args[0].as_map()) {
            values.push_back(v);
        }
        return Value::make_array(std::move(values));
    });

    define_native("has_key", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_map()) return Value(false);
        const auto& map = args[0].as_map();
        return Value(map.count(args[1].to_string()) > 0);
    });

    define_native("get", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(nullptr);
        if (args[0].is_array()) {
            std::int64_t idx = args[1].as_int();
            const auto& arr = args[0].as_array();
            if (idx >= 0 && static_cast<std::size_t>(idx) < arr.size()) {
                return arr[idx];
            }
        } else if (args[0].is_map()) {
            const auto& map = args[0].as_map();
            auto it = map.find(args[1].to_string());
            if (it != map.end()) return it->second;
        }
        return args.size() > 2 ? args[2] : Value(nullptr);
    });

    define_native("set", 3, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 3) return Value(nullptr);
        if (args[0].is_array()) {
            std::int64_t idx = args[1].as_int();
            auto& arr = const_cast<ValueArray&>(args[0].as_array());
            if (idx >= 0 && static_cast<std::size_t>(idx) < arr.size()) {
                arr[idx] = args[2];
            }
        } else if (args[0].is_map()) {
            auto& map = const_cast<ValueMap&>(args[0].as_map());
            map[args[1].to_string()] = args[2];
        }
        return Value(nullptr);
    });

    define_native("range", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value::make_array({});
        std::int64_t start = 0, end = 0, step = 1;
        if (args.size() == 1) {
            end = args[0].as_int();
        } else if (args.size() == 2) {
            start = args[0].as_int();
            end = args[1].as_int();
        } else {
            start = args[0].as_int();
            end = args[1].as_int();
            step = args[2].as_int();
            if (step == 0) step = 1;
        }
        ValueArray arr;
        if (step > 0) {
            for (std::int64_t i = start; i < end; i += step) {
                arr.push_back(Value(i));
            }
        } else {
            for (std::int64_t i = start; i > end; i += step) {
                arr.push_back(Value(i));
            }
        }
        return Value::make_array(std::move(arr));
    });

    define_native("enumerate", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value::make_array({});
        ValueArray result;
        const auto& arr = args[0].as_array();
        for (std::size_t i = 0; i < arr.size(); ++i) {
            ValueArray pair;
            pair.push_back(Value(static_cast<std::int64_t>(i)));
            pair.push_back(arr[i]);
            result.push_back(Value::make_array(std::move(pair)));
        }
        return Value::make_array(std::move(result));
    });

    define_native("zip", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array() || !args[1].is_array()) {
            return Value::make_array({});
        }
        ValueArray result;
        const auto& a = args[0].as_array();
        const auto& b = args[1].as_array();
        std::size_t len = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < len; ++i) {
            ValueArray pair;
            pair.push_back(a[i]);
            pair.push_back(b[i]);
            result.push_back(Value::make_array(std::move(pair)));
        }
        return Value::make_array(std::move(result));
    });

    define_native("map", 2, [](Interpreter& interp, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array() || !args[1].is_callable()) {
            return Value::make_array({});
        }
        ValueArray result;
        const auto& arr = args[0].as_array();
        for (const auto& item : arr) {
            result.push_back(interp.call_value(args[1], {item}));
        }
        return Value::make_array(std::move(result));
    });

    define_native("filter", 2, [](Interpreter& interp, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array() || !args[1].is_callable()) {
            return Value::make_array({});
        }
        ValueArray result;
        const auto& arr = args[0].as_array();
        for (const auto& item : arr) {
            if (interp.call_value(args[1], {item}).is_truthy()) {
                result.push_back(item);
            }
        }
        return Value::make_array(std::move(result));
    });

    define_native("reduce", 3, [](Interpreter& interp, const std::vector<Value>& args) {
        if (args.size() < 3 || !args[0].is_array() || !args[1].is_callable()) {
            return Value(nullptr);
        }
        Value acc = args[2];
        const auto& arr = args[0].as_array();
        for (const auto& item : arr) {
            acc = interp.call_value(args[1], {acc, item});
        }
        return acc;
    });

    define_native("slice", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array()) return Value::make_array({});
        const auto& arr = args[0].as_array();
        std::int64_t start = args[1].as_int();
        std::int64_t end = args.size() > 2 ? args[2].as_int() : static_cast<std::int64_t>(arr.size());

        if (start < 0) start = std::max<std::int64_t>(0, static_cast<std::int64_t>(arr.size()) + start);
        if (end < 0) end = std::max<std::int64_t>(0, static_cast<std::int64_t>(arr.size()) + end);
        start = std::min<std::int64_t>(start, arr.size());
        end = std::min<std::int64_t>(end, arr.size());

        ValueArray result;
        for (std::int64_t i = start; i < end; ++i) {
            result.push_back(arr[i]);
        }
        return Value::make_array(std::move(result));
    });

    define_native("reverse", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value::make_array({});
        ValueArray result = args[0].as_array();
        std::reverse(result.begin(), result.end());
        return Value::make_array(std::move(result));
    });

    define_native("concat", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array() || !args[1].is_array()) {
            return Value::make_array({});
        }
        ValueArray result = args[0].as_array();
        const auto& b = args[1].as_array();
        result.insert(result.end(), b.begin(), b.end());
        return Value::make_array(std::move(result));
    });

    define_native("flatten", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value::make_array({});
        ValueArray result;
        for (const auto& item : args[0].as_array()) {
            if (item.is_array()) {
                const auto& sub = item.as_array();
                result.insert(result.end(), sub.begin(), sub.end());
            } else {
                result.push_back(item);
            }
        }
        return Value::make_array(std::move(result));
    });

    define_native("sort", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value::make_array({});
        ValueArray result = args[0].as_array();
        std::sort(result.begin(), result.end(), [](const Value& a, const Value& b) {
            if (a.is_number() && b.is_number()) {
                return a.as_number() < b.as_number();
            }
            return a.to_string() < b.to_string();
        });
        return Value::make_array(std::move(result));
    });

    define_native("unique", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value::make_array({});
        ValueArray result;
        std::set<std::string> seen;
        for (const auto& item : args[0].as_array()) {
            std::string key = item.to_string();
            if (seen.find(key) == seen.end()) {
                seen.insert(key);
                result.push_back(item);
            }
        }
        return Value::make_array(std::move(result));
    });

    define_native("find_index", 2, [](Interpreter& interp, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array()) return Value(static_cast<std::int64_t>(-1));
        const auto& arr = args[0].as_array();
        if (args[1].is_callable()) {
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (interp.call_value(args[1], {arr[i]}).is_truthy()) {
                    return Value(static_cast<std::int64_t>(i));
                }
            }
        } else {
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (arr[i].equals(args[1])) {
                    return Value(static_cast<std::int64_t>(i));
                }
            }
        }
        return Value(static_cast<std::int64_t>(-1));
    });

    define_native("index_of", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array()) return Value(static_cast<std::int64_t>(-1));
        const auto& arr = args[0].as_array();
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (arr[i].equals(args[1])) {
                return Value(static_cast<std::int64_t>(i));
            }
        }
        return Value(static_cast<std::int64_t>(-1));
    });

    define_native("sum", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value(0.0);
        double sum = 0;
        for (const auto& item : args[0].as_array()) {
            if (item.is_number()) sum += item.as_number();
        }
        return Value(sum);
    });

    define_native("product", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_array()) return Value(1.0);
        double prod = 1;
        for (const auto& item : args[0].as_array()) {
            if (item.is_number()) prod *= item.as_number();
        }
        return Value(prod);
    });

    define_native("any", 2, [](Interpreter& interp, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array() || !args[1].is_callable()) {
            return Value(false);
        }
        for (const auto& item : args[0].as_array()) {
            if (interp.call_value(args[1], {item}).is_truthy()) {
                return Value(true);
            }
        }
        return Value(false);
    });

    define_native("all", 2, [](Interpreter& interp, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array() || !args[1].is_callable()) {
            return Value(true);
        }
        for (const auto& item : args[0].as_array()) {
            if (!interp.call_value(args[1], {item}).is_truthy()) {
                return Value(false);
            }
        }
        return Value(true);
    });

    define_native("count", 2, [](Interpreter& interp, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array()) return Value(static_cast<std::int64_t>(0));
        std::int64_t count = 0;
        const auto& arr = args[0].as_array();
        if (args[1].is_callable()) {
            for (const auto& item : arr) {
                if (interp.call_value(args[1], {item}).is_truthy()) ++count;
            }
        } else {
            for (const auto& item : arr) {
                if (item.equals(args[1])) ++count;
            }
        }
        return Value(count);
    });

    define_native("take", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array()) return Value::make_array({});
        const auto& arr = args[0].as_array();
        std::int64_t n = args[1].as_int();
        n = std::min<std::int64_t>(n, arr.size());
        ValueArray result(arr.begin(), arr.begin() + n);
        return Value::make_array(std::move(result));
    });

    define_native("drop", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array()) return Value::make_array({});
        const auto& arr = args[0].as_array();
        std::int64_t n = args[1].as_int();
        n = std::min<std::int64_t>(n, arr.size());
        ValueArray result(arr.begin() + n, arr.end());
        return Value::make_array(std::move(result));
    });

    define_native("insert", 3, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 3 || !args[0].is_array()) return Value(nullptr);
        auto& arr = const_cast<ValueArray&>(args[0].as_array());
        std::int64_t idx = args[1].as_int();
        idx = std::clamp<std::int64_t>(idx, 0, arr.size());
        arr.insert(arr.begin() + idx, args[2]);
        return Value(nullptr);
    });

    define_native("remove", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array()) return Value(nullptr);
        auto& arr = const_cast<ValueArray&>(args[0].as_array());
        std::int64_t idx = args[1].as_int();
        if (idx >= 0 && static_cast<std::size_t>(idx) < arr.size()) {
            Value removed = arr[idx];
            arr.erase(arr.begin() + idx);
            return removed;
        }
        return Value(nullptr);
    });

    define_native("clear", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(nullptr);
        if (args[0].is_array()) {
            const_cast<ValueArray&>(args[0].as_array()).clear();
        } else if (args[0].is_map()) {
            const_cast<ValueMap&>(args[0].as_map()).clear();
        }
        return Value(nullptr);
    });

    define_native("merge", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_map() || !args[1].is_map()) {
            return Value::make_map({});
        }
        ValueMap result = args[0].as_map();
        for (const auto& [k, v] : args[1].as_map()) {
            result[k] = v;
        }
        return Value::make_map(std::move(result));
    });

    // =========================================================================
    // String Functions
    // =========================================================================

    define_native("upper", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return Value(s);
    });

    define_native("lower", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return Value(s);
    });

    define_native("capitalize", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        if (!s.empty()) {
            s[0] = std::toupper(s[0]);
            std::transform(s.begin() + 1, s.end(), s.begin() + 1, ::tolower);
        }
        return Value(s);
    });

    define_native("trim", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        auto start = std::find_if_not(s.begin(), s.end(), ::isspace);
        auto end = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
        return Value(start < end ? std::string(start, end) : "");
    });

    define_native("trim_start", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        auto start = std::find_if_not(s.begin(), s.end(), ::isspace);
        return Value(std::string(start, s.end()));
    });

    define_native("trim_end", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        auto end = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
        return Value(std::string(s.begin(), end));
    });

    define_native("split", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value::make_array({});
        std::string s = args[0].as_string();
        std::string delim = args[1].to_string();
        ValueArray result;
        if (delim.empty()) {
            for (char c : s) {
                result.push_back(Value(std::string(1, c)));
            }
        } else {
            std::size_t pos = 0, found;
            while ((found = s.find(delim, pos)) != std::string::npos) {
                result.push_back(Value(s.substr(pos, found - pos)));
                pos = found + delim.length();
            }
            result.push_back(Value(s.substr(pos)));
        }
        return Value::make_array(std::move(result));
    });

    define_native("join", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_array()) return Value("");
        std::string delim = args[1].to_string();
        std::string result;
        const auto& arr = args[0].as_array();
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) result += delim;
            result += arr[i].to_string();
        }
        return Value(result);
    });

    define_native("chars", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_string()) return Value::make_array({});
        ValueArray result;
        for (char c : args[0].as_string()) {
            result.push_back(Value(std::string(1, c)));
        }
        return Value::make_array(std::move(result));
    });

    define_native("contains", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value(false);
        return Value(args[0].as_string().find(args[1].to_string()) != std::string::npos);
    });

    define_native("starts_with", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value(false);
        const auto& s = args[0].as_string();
        const auto& prefix = args[1].to_string();
        return Value(s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0);
    });

    define_native("ends_with", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value(false);
        const auto& s = args[0].as_string();
        const auto& suffix = args[1].to_string();
        return Value(s.size() >= suffix.size() &&
                     s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
    });

    define_native("replace", 3, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 3 || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        std::string from = args[1].to_string();
        std::string to = args[2].to_string();
        if (!from.empty()) {
            auto pos = s.find(from);
            if (pos != std::string::npos) {
                s.replace(pos, from.length(), to);
            }
        }
        return Value(s);
    });

    define_native("replace_all", 3, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 3 || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        std::string from = args[1].to_string();
        std::string to = args[2].to_string();
        if (!from.empty()) {
            std::size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.length(), to);
                pos += to.length();
            }
        }
        return Value(s);
    });

    define_native("substr", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value("");
        const auto& s = args[0].as_string();
        std::int64_t start = args[1].as_int();
        std::size_t len = args.size() > 2 ? args[2].as_int() : std::string::npos;
        if (start < 0) start = std::max<std::int64_t>(0, static_cast<std::int64_t>(s.size()) + start);
        return Value(s.substr(start, len));
    });

    define_native("pad_left", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        std::size_t width = args[1].as_int();
        char pad_char = args.size() > 2 && args[2].is_string() && !args[2].as_string().empty()
                        ? args[2].as_string()[0] : ' ';
        if (s.size() < width) {
            s.insert(0, width - s.size(), pad_char);
        }
        return Value(s);
    });

    define_native("pad_right", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        std::size_t width = args[1].as_int();
        char pad_char = args.size() > 2 && args[2].is_string() && !args[2].as_string().empty()
                        ? args[2].as_string()[0] : ' ';
        if (s.size() < width) {
            s.append(width - s.size(), pad_char);
        }
        return Value(s);
    });

    define_native("repeat", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value("");
        std::string s = args[0].as_string();
        std::int64_t count = args[1].as_int();
        std::string result;
        result.reserve(s.size() * count);
        for (std::int64_t i = 0; i < count; ++i) {
            result += s;
        }
        return Value(result);
    });

    define_native("char_at", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value("");
        const auto& s = args[0].as_string();
        std::int64_t idx = args[1].as_int();
        if (idx >= 0 && static_cast<std::size_t>(idx) < s.size()) {
            return Value(std::string(1, s[idx]));
        }
        return Value("");
    });

    define_native("char_code", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[0].is_string()) return Value(static_cast<std::int64_t>(0));
        const auto& s = args[0].as_string();
        std::int64_t idx = args[1].as_int();
        if (idx >= 0 && static_cast<std::size_t>(idx) < s.size()) {
            return Value(static_cast<std::int64_t>(static_cast<unsigned char>(s[idx])));
        }
        return Value(static_cast<std::int64_t>(0));
    });

    define_native("from_char_code", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value("");
        std::int64_t code = args[0].as_int();
        if (code >= 0 && code <= 127) {
            return Value(std::string(1, static_cast<char>(code)));
        }
        return Value("");
    });

    define_native("is_empty", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(true);
        if (args[0].is_string()) return Value(args[0].as_string().empty());
        if (args[0].is_array()) return Value(args[0].as_array().empty());
        if (args[0].is_map()) return Value(args[0].as_map().empty());
        return Value(args[0].is_null());
    });

    define_native("is_blank", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_string()) return Value(true);
        const auto& s = args[0].as_string();
        return Value(std::all_of(s.begin(), s.end(), ::isspace));
    });

    // =========================================================================
    // Math Functions
    // =========================================================================

    define_native("abs", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::abs(args[0].as_number()));
    });

    define_native("sign", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        double v = args[0].as_number();
        return Value(v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0));
    });

    define_native("min", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(0.0);
        return Value(std::min(args[0].as_number(), args[1].as_number()));
    });

    define_native("max", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(0.0);
        return Value(std::max(args[0].as_number(), args[1].as_number()));
    });

    define_native("floor", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::floor(args[0].as_number()));
    });

    define_native("ceil", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::ceil(args[0].as_number()));
    });

    define_native("round", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::round(args[0].as_number()));
    });

    define_native("trunc", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::trunc(args[0].as_number()));
    });

    define_native("sqrt", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::sqrt(args[0].as_number()));
    });

    define_native("cbrt", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::cbrt(args[0].as_number()));
    });

    define_native("pow", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(0.0);
        return Value(std::pow(args[0].as_number(), args[1].as_number()));
    });

    define_native("exp", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(1.0);
        return Value(std::exp(args[0].as_number()));
    });

    define_native("log", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::log(args[0].as_number()));
    });

    define_native("log10", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::log10(args[0].as_number()));
    });

    define_native("log2", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::log2(args[0].as_number()));
    });

    define_native("sin", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::sin(args[0].as_number()));
    });

    define_native("cos", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::cos(args[0].as_number()));
    });

    define_native("tan", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::tan(args[0].as_number()));
    });

    define_native("asin", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::asin(args[0].as_number()));
    });

    define_native("acos", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::acos(args[0].as_number()));
    });

    define_native("atan", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::atan(args[0].as_number()));
    });

    define_native("atan2", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(0.0);
        return Value(std::atan2(args[0].as_number(), args[1].as_number()));
    });

    define_native("sinh", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::sinh(args[0].as_number()));
    });

    define_native("cosh", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::cosh(args[0].as_number()));
    });

    define_native("tanh", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(std::tanh(args[0].as_number()));
    });

    define_native("hypot", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(0.0);
        return Value(std::hypot(args[0].as_number(), args[1].as_number()));
    });

    define_native("fract", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        double v = args[0].as_number();
        return Value(v - std::floor(v));
    });

    define_native("mod", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(0.0);
        return Value(std::fmod(args[0].as_number(), args[1].as_number()));
    });

    define_native("clamp", 3, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 3) return Value(0.0);
        double v = args[0].as_number();
        double lo = args[1].as_number();
        double hi = args[2].as_number();
        return Value(std::clamp(v, lo, hi));
    });

    define_native("lerp", 3, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 3) return Value(0.0);
        double a = args[0].as_number();
        double b = args[1].as_number();
        double t = args[2].as_number();
        return Value(a + t * (b - a));
    });

    define_native("map_range", 5, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 5) return Value(0.0);
        double v = args[0].as_number();
        double in_min = args[1].as_number();
        double in_max = args[2].as_number();
        double out_min = args[3].as_number();
        double out_max = args[4].as_number();
        return Value(out_min + (v - in_min) * (out_max - out_min) / (in_max - in_min));
    });

    define_native("radians", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(args[0].as_number() * 3.14159265358979323846 / 180.0);
    });

    define_native("degrees", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(args[0].as_number() * 180.0 / 3.14159265358979323846);
    });

    // Random number generator
    static std::mt19937 rng(std::random_device{}());

    define_native("random", 0, [](Interpreter&, const std::vector<Value>& args) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value(dist(rng));
    });

    define_native("random_int", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(static_cast<std::int64_t>(0));
        std::int64_t min_val = args[0].as_int();
        std::int64_t max_val = args[1].as_int();
        std::uniform_int_distribution<std::int64_t> dist(min_val, max_val);
        return Value(dist(rng));
    });

    define_native("random_range", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(0.0);
        double min_val = args[0].as_number();
        double max_val = args[1].as_number();
        std::uniform_real_distribution<double> dist(min_val, max_val);
        return Value(dist(rng));
    });

    // Math constants
    define_constant("PI", Value(3.14159265358979323846));
    define_constant("E", Value(2.71828182845904523536));
    define_constant("TAU", Value(6.28318530717958647692));
    define_constant("INFINITY", Value(std::numeric_limits<double>::infinity()));
    define_constant("NEG_INFINITY", Value(-std::numeric_limits<double>::infinity()));
    define_constant("NAN", Value(std::numeric_limits<double>::quiet_NaN()));

    // =========================================================================
    // Time Functions
    // =========================================================================

    define_native("now", 0, [](Interpreter&, const std::vector<Value>& args) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        return Value(static_cast<double>(ms));
    });

    define_native("now_secs", 0, [](Interpreter&, const std::vector<Value>& args) {
        auto now = std::chrono::system_clock::now();
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        return Value(static_cast<double>(secs));
    });

    define_native("performance_now", 0, [](Interpreter&, const std::vector<Value>& args) {
        auto now = std::chrono::high_resolution_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        return Value(static_cast<double>(ns) / 1e6);  // Return milliseconds
    });

    // =========================================================================
    // Utility Functions
    // =========================================================================

    define_native("assert", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_truthy()) {
            std::string message = args.size() > 1 ? args[1].to_string() : "Assertion failed";
            throw ScriptException(ScriptError::AssertionFailed, message);
        }
        return Value(nullptr);
    });

    define_native("panic", 1, [](Interpreter&, const std::vector<Value>& args) {
        std::string message = args.empty() ? "Panic!" : args[0].to_string();
        throw ScriptException(ScriptError::RuntimeError, "PANIC: " + message);
    });

    define_native("clone", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(nullptr);
        // Deep clone for arrays and maps
        if (args[0].is_array()) {
            return Value::make_array(args[0].as_array());
        }
        if (args[0].is_map()) {
            return Value::make_map(args[0].as_map());
        }
        return args[0];  // Primitives are already copied by value
    });

    define_native("hash", 1, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(static_cast<std::int64_t>(0));
        std::hash<std::string> hasher;
        return Value(static_cast<std::int64_t>(hasher(args[0].to_string())));
    });

    define_native("default", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(nullptr);
        if (args[0].is_null()) {
            return args.size() > 1 ? args[1] : Value(nullptr);
        }
        return args[0];
    });

    define_native("coalesce", 0, [](Interpreter&, const std::vector<Value>& args) {
        for (const auto& arg : args) {
            if (!arg.is_null()) return arg;
        }
        return Value(nullptr);
    });

    define_native("identity", 1, [](Interpreter&, const std::vector<Value>& args) {
        return args.empty() ? Value(nullptr) : args[0];
    });

    define_native("noop", 0, [](Interpreter&, const std::vector<Value>& args) {
        return Value(nullptr);
    });

    define_native("equals", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        return Value(args[0].equals(args[1]));
    });

    define_native("compare", 2, [](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(static_cast<std::int64_t>(0));
        if (args[0].is_number() && args[1].is_number()) {
            double a = args[0].as_number();
            double b = args[1].as_number();
            return Value(static_cast<std::int64_t>(a < b ? -1 : (a > b ? 1 : 0)));
        }
        std::string a = args[0].to_string();
        std::string b = args[1].to_string();
        return Value(static_cast<std::int64_t>(a < b ? -1 : (a > b ? 1 : 0)));
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
// Hot-Reload Support
// =============================================================================

Interpreter::Snapshot Interpreter::take_snapshot() const {
    Snapshot snapshot;

    // Capture all global variables (excluding native functions)
    for (const auto& [name, value] : globals_->variables()) {
        // Skip native functions and callable objects for snapshot
        if (!value.is_callable()) {
            snapshot.global_variables[name] = value;
        }
    }

    return snapshot;
}

void Interpreter::apply_snapshot(const Snapshot& snapshot) {
    // Restore global variables
    for (const auto& [name, value] : snapshot.global_variables) {
        // Only restore if variable exists or is new data
        if (globals_->contains(name)) {
            globals_->assign(name, value);
        } else {
            globals_->define(name, value);
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
        case TokenType::Increment: {
            double new_val = operand.as_number() + 1;
            // Modify the variable if operand is an identifier
            if (auto* id = dynamic_cast<const IdentifierExpr*>(expr.operand.get())) {
                current_env_->assign(id->name, Value(new_val));
            }
            return expr.prefix ? Value(new_val) : operand;
        }
        case TokenType::Decrement: {
            double new_val = operand.as_number() - 1;
            if (auto* id = dynamic_cast<const IdentifierExpr*>(expr.operand.get())) {
                current_env_->assign(id->name, Value(new_val));
            }
            return expr.prefix ? Value(new_val) : operand;
        }
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
        if (!super.is_callable()) {
            throw ScriptException(ScriptError::TypeError, "Superclass must be a class");
        }
        Callable* callable = super.as_callable();
        ScriptClass* super_class = dynamic_cast<ScriptClass*>(callable);
        if (!super_class) {
            throw ScriptException(ScriptError::TypeError, "Superclass must be a class");
        }
        superclass = std::static_pointer_cast<ScriptClass>(super_class->shared_from_this());
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
// Additional Expression Visitors
// =============================================================================

Value Interpreter::visit(const LambdaExpr& expr) {
    // Create an anonymous function declaration
    auto func_decl = std::make_unique<FunctionDecl>("", std::vector<FunctionDecl::Parameter>{}, nullptr);

    // Convert lambda parameters to function parameters
    for (const auto& param : expr.parameters) {
        FunctionDecl::Parameter fp;
        fp.name = param.name;
        fp.type = param.type;
        // Note: default_value ownership is tricky here, lambdas typically don't have defaults
        func_decl->parameters.push_back(std::move(fp));
    }
    func_decl->body = std::unique_ptr<Statement>(const_cast<Statement*>(expr.body.get()));

    // Store the raw pointer before moving
    const FunctionDecl* raw_decl = func_decl.get();
    lambda_storage_.push_back(std::move(func_decl));

    auto function = std::make_shared<ScriptFunction>(raw_decl, current_env_);
    return Value::make_function(function);
}

Value Interpreter::visit(const NewExpr& expr) {
    Value class_val = evaluate(*expr.class_expr);

    std::vector<Value> args;
    for (const auto& arg : expr.arguments) {
        args.push_back(evaluate(*arg));
    }

    return call_value(std::move(class_val), args);
}

Value Interpreter::visit(const SuperExpr& expr) {
    if (!current_instance_) {
        throw ScriptException(ScriptError::InvalidOperation, "'super' used outside of method");
    }

    auto klass = current_instance_->get_class();
    auto superclass = klass->superclass();
    if (!superclass) {
        throw ScriptException(ScriptError::InvalidOperation, "No superclass");
    }

    auto method = superclass->find_method(expr.method);
    if (!method) {
        throw ScriptException(ScriptError::UndefinedProperty, "Undefined method: " + expr.method);
    }

    return Value::make_function(method->bind(current_instance_));
}

Value Interpreter::visit(const RangeExpr& expr) {
    std::int64_t start = evaluate(*expr.start).as_int();
    std::int64_t end = evaluate(*expr.end).as_int();

    ValueArray arr;
    if (expr.inclusive) {
        for (std::int64_t i = start; i <= end; ++i) {
            arr.push_back(Value(i));
        }
    } else {
        for (std::int64_t i = start; i < end; ++i) {
            arr.push_back(Value(i));
        }
    }
    return Value::make_array(std::move(arr));
}

Value Interpreter::visit(const AwaitExpr& expr) {
    // For now, await just evaluates the expression synchronously
    // Full async support would require coroutine infrastructure
    return evaluate(*expr.operand);
}

Value Interpreter::visit(const YieldExpr& expr) {
    // Yield support would require generator infrastructure
    // For now, just return the value
    if (expr.value) {
        return evaluate(*expr.value);
    }
    return Value(nullptr);
}

// =============================================================================
// Additional Statement Visitors
// =============================================================================

void Interpreter::visit(const MatchStatement& stmt) {
    Value subject = evaluate(*stmt.subject);

    for (const auto& arm : stmt.arms) {
        // Evaluate pattern (for simple value matching)
        Value pattern = arm.pattern ? evaluate(*arm.pattern) : Value(nullptr);

        bool matched = false;

        // Check if pattern matches
        if (!arm.pattern) {
            // Default/wildcard arm
            matched = true;
        } else if (subject.equals(pattern)) {
            matched = true;
        }

        // Check guard if present
        if (matched && arm.guard) {
            matched = evaluate(*arm.guard).is_truthy();
        }

        if (matched) {
            execute(*arm.body);
            return;
        }
    }
}

void Interpreter::visit(const TryCatchStatement& stmt) {
    try {
        execute(*stmt.try_block);
    } catch (const ScriptException& e) {
        // Find matching catch clause
        bool handled = false;
        for (const auto& clause : stmt.catch_clauses) {
            // For now, catch all exceptions (type checking would go here)
            push_scope();
            current_env_->define(clause.variable, Value(e.what()));
            try {
                execute(*clause.body);
            } catch (...) {
                pop_scope();
                if (stmt.finally_block) {
                    execute(*stmt.finally_block);
                }
                throw;
            }
            pop_scope();
            handled = true;
            break;
        }

        if (!handled) {
            if (stmt.finally_block) {
                execute(*stmt.finally_block);
            }
            throw;
        }
    } catch (...) {
        if (stmt.finally_block) {
            execute(*stmt.finally_block);
        }
        throw;
    }

    if (stmt.finally_block) {
        execute(*stmt.finally_block);
    }
}

void Interpreter::visit(const ThrowStatement& stmt) {
    Value value = evaluate(*stmt.value);
    throw ScriptException(ScriptError::UserException, value.to_string());
}

void Interpreter::visit(const ImportDecl& decl) {
    // Load the module
    std::string module_path = decl.module_path;

    // Try to find and load the module file
    // For now, we support loading .vs files from the same directory
    if (!module_path.ends_with(".vs")) {
        module_path += ".vs";
    }

    // Check if module is already loaded
    auto it = modules_.find(module_path);
    if (it == modules_.end()) {
        // Load and execute the module
        try {
            std::ifstream file(module_path);
            if (!file) {
                throw ScriptException(ScriptError::ModuleNotFound, "Module not found: " + module_path);
            }

            std::stringstream buffer;
            buffer << file.rdbuf();

            // Create a new environment for the module
            auto module_env = std::make_unique<Environment>(globals_.get());
            Environment* prev_env = current_env_;
            current_env_ = module_env.get();

            Parser parser(buffer.str(), module_path);
            auto program = parser.parse_program();

            if (!parser.has_errors()) {
                execute(*program);
            }

            // Store module exports
            modules_[module_path] = std::move(module_env);
            current_env_ = prev_env;
        } catch (const std::exception& e) {
            throw ScriptException(ScriptError::ModuleNotFound, "Failed to load module: " + module_path);
        }
    }

    // Import items into current scope
    auto* module_env = modules_[module_path].get();

    if (decl.import_all) {
        // Import everything
        if (decl.alias) {
            // import * as alias
            ValueMap map;
            for (const auto& [name, value] : module_env->variables()) {
                map[name] = value;
            }
            current_env_->define(*decl.alias, Value::make_map(std::move(map)));
        } else {
            // import *
            for (const auto& [name, value] : module_env->variables()) {
                current_env_->define(name, value);
            }
        }
    } else {
        // Import specific items
        for (const auto& item : decl.items) {
            if (!module_env->contains(item.name)) {
                throw ScriptException(ScriptError::UndefinedVariable,
                    "Module does not export: " + item.name);
            }
            std::string local_name = item.alias.value_or(item.name);
            current_env_->define(local_name, module_env->get(item.name));
        }
    }
}

void Interpreter::visit(const ExportDecl& decl) {
    // Execute the declaration
    if (decl.declaration) {
        execute(*decl.declaration);
    }
    // Exports are tracked by the module system when the module is loaded
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
