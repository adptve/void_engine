#include "types.hpp"

#include <sstream>

namespace void_script {

// =============================================================================
// SourceLocation Implementation
// =============================================================================

std::string SourceLocation::to_string() const {
    std::ostringstream ss;
    if (!file.empty()) {
        ss << file << ":";
    }
    ss << line << ":" << column;
    return ss.str();
}

// =============================================================================
// Token Implementation
// =============================================================================

bool Token::is_keyword() const {
    return type >= TokenType::Let && type <= TokenType::Impl;
}

bool Token::is_operator() const {
    return type >= TokenType::Plus && type <= TokenType::Decrement;
}

bool Token::is_literal() const {
    return type == TokenType::Integer || type == TokenType::Float ||
           type == TokenType::String || type == TokenType::True ||
           type == TokenType::False || type == TokenType::Null;
}

bool Token::is_assignment() const {
    return type == TokenType::Assign || type == TokenType::PlusAssign ||
           type == TokenType::MinusAssign || type == TokenType::StarAssign ||
           type == TokenType::SlashAssign || type == TokenType::PercentAssign ||
           type == TokenType::AmpersandAssign || type == TokenType::PipeAssign ||
           type == TokenType::CaretAssign || type == TokenType::ShiftLeftAssign ||
           type == TokenType::ShiftRightAssign;
}

std::string Token::to_string() const {
    std::ostringstream ss;
    ss << token_type_name(type);
    if (!lexeme.empty()) {
        ss << " '" << lexeme << "'";
    }
    ss << " at " << location.to_string();
    return ss.str();
}

// =============================================================================
// Value Implementation
// =============================================================================

bool Value::is_callable() const {
    if (type_ == ValueType::Function) return true;
    if (type_ == ValueType::Class) return true;
    if (object_value_) {
        return dynamic_cast<Callable*>(object_value_.get()) != nullptr;
    }
    return false;
}

bool Value::as_bool() const {
    if (type_ != ValueType::Bool) {
        throw ScriptException(ScriptError::TypeMismatch, "Expected bool");
    }
    return bool_value_;
}

std::int64_t Value::as_int() const {
    if (type_ == ValueType::Int) return int_value_;
    if (type_ == ValueType::Float) return static_cast<std::int64_t>(float_value_);
    throw ScriptException(ScriptError::TypeMismatch, "Expected integer");
}

double Value::as_float() const {
    if (type_ == ValueType::Float) return float_value_;
    if (type_ == ValueType::Int) return static_cast<double>(int_value_);
    throw ScriptException(ScriptError::TypeMismatch, "Expected float");
}

double Value::as_number() const {
    if (type_ == ValueType::Float) return float_value_;
    if (type_ == ValueType::Int) return static_cast<double>(int_value_);
    throw ScriptException(ScriptError::TypeMismatch, "Expected number");
}

const std::string& Value::as_string() const {
    if (type_ != ValueType::String || !string_value_) {
        throw ScriptException(ScriptError::TypeMismatch, "Expected string");
    }
    return *string_value_;
}

ValueArray& Value::as_array() {
    if (type_ != ValueType::Array || !array_value_) {
        throw ScriptException(ScriptError::TypeMismatch, "Expected array");
    }
    return *array_value_;
}

const ValueArray& Value::as_array() const {
    if (type_ != ValueType::Array || !array_value_) {
        throw ScriptException(ScriptError::TypeMismatch, "Expected array");
    }
    return *array_value_;
}

ValueMap& Value::as_map() {
    if (type_ != ValueType::Map || !map_value_) {
        throw ScriptException(ScriptError::TypeMismatch, "Expected map");
    }
    return *map_value_;
}

const ValueMap& Value::as_map() const {
    if (type_ != ValueType::Map || !map_value_) {
        throw ScriptException(ScriptError::TypeMismatch, "Expected map");
    }
    return *map_value_;
}

Object* Value::as_object() {
    return object_value_.get();
}

const Object* Value::as_object() const {
    return object_value_.get();
}

Callable* Value::as_callable() {
    if (!object_value_) return nullptr;
    return dynamic_cast<Callable*>(object_value_.get());
}

void Value::set_object(std::shared_ptr<Object> obj) {
    object_value_ = std::move(obj);
    type_ = obj ? obj->object_type() : ValueType::Null;
}

bool Value::is_truthy() const {
    switch (type_) {
        case ValueType::Null: return false;
        case ValueType::Bool: return bool_value_;
        case ValueType::Int: return int_value_ != 0;
        case ValueType::Float: return float_value_ != 0.0;
        case ValueType::String: return string_value_ && !string_value_->empty();
        case ValueType::Array: return array_value_ && !array_value_->empty();
        case ValueType::Map: return map_value_ && !map_value_->empty();
        default: return true;
    }
}

bool Value::equals(const Value& other) const {
    if (type_ != other.type_) return false;

    switch (type_) {
        case ValueType::Null: return true;
        case ValueType::Bool: return bool_value_ == other.bool_value_;
        case ValueType::Int: return int_value_ == other.int_value_;
        case ValueType::Float: return float_value_ == other.float_value_;
        case ValueType::String:
            return string_value_ && other.string_value_ &&
                   *string_value_ == *other.string_value_;
        case ValueType::Array:
            if (!array_value_ || !other.array_value_) return false;
            if (array_value_->size() != other.array_value_->size()) return false;
            for (std::size_t i = 0; i < array_value_->size(); ++i) {
                if (!(*array_value_)[i].equals((*other.array_value_)[i])) return false;
            }
            return true;
        default:
            return object_value_.get() == other.object_value_.get();
    }
}

int Value::compare(const Value& other) const {
    if (type_ != other.type_) {
        return static_cast<int>(type_) - static_cast<int>(other.type_);
    }

    switch (type_) {
        case ValueType::Null: return 0;
        case ValueType::Bool: return static_cast<int>(bool_value_) - static_cast<int>(other.bool_value_);
        case ValueType::Int:
            if (int_value_ < other.int_value_) return -1;
            if (int_value_ > other.int_value_) return 1;
            return 0;
        case ValueType::Float:
            if (float_value_ < other.float_value_) return -1;
            if (float_value_ > other.float_value_) return 1;
            return 0;
        case ValueType::String:
            if (string_value_ && other.string_value_) {
                return string_value_->compare(*other.string_value_);
            }
            return 0;
        default:
            return 0;
    }
}

std::string Value::to_string() const {
    std::ostringstream ss;

    switch (type_) {
        case ValueType::Null:
            ss << "null";
            break;
        case ValueType::Bool:
            ss << (bool_value_ ? "true" : "false");
            break;
        case ValueType::Int:
            ss << int_value_;
            break;
        case ValueType::Float:
            ss << float_value_;
            break;
        case ValueType::String:
            if (string_value_) ss << *string_value_;
            break;
        case ValueType::Array:
            if (array_value_) {
                ss << "[";
                for (std::size_t i = 0; i < array_value_->size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << (*array_value_)[i].to_string();
                }
                ss << "]";
            }
            break;
        case ValueType::Map:
            if (map_value_) {
                ss << "{";
                bool first = true;
                for (const auto& [key, val] : *map_value_) {
                    if (!first) ss << ", ";
                    ss << key << ": " << val.to_string();
                    first = false;
                }
                ss << "}";
            }
            break;
        case ValueType::Function:
            if (object_value_) ss << object_value_->to_string();
            else ss << "<function>";
            break;
        case ValueType::Object:
            if (object_value_) ss << object_value_->to_string();
            else ss << "<object>";
            break;
        default:
            ss << "<" << value_type_name(type_) << ">";
            break;
    }

    return ss.str();
}

std::string Value::type_name() const {
    return value_type_name(type_);
}

Value Value::make_array(ValueArray arr) {
    return Value(std::move(arr));
}

Value Value::make_map(ValueMap map) {
    return Value(std::move(map));
}

Value Value::make_object(std::shared_ptr<Object> obj) {
    Value v;
    v.set_object(std::move(obj));
    return v;
}

Value Value::make_function(std::shared_ptr<Callable> fn) {
    Value v;
    v.type_ = ValueType::Function;
    v.object_value_ = std::move(fn);
    return v;
}

// =============================================================================
// Object Implementation
// =============================================================================

bool Object::has_property(const std::string& name) const {
    return properties_.count(name) > 0;
}

Value Object::get_property(const std::string& name) const {
    auto it = properties_.find(name);
    if (it == properties_.end()) {
        throw ScriptException(ScriptError::UndefinedProperty, "Undefined property: " + name);
    }
    return it->second;
}

void Object::set_property(const std::string& name, Value value) {
    properties_[name] = std::move(value);
}

bool Object::has_method(const std::string& name) const {
    auto it = properties_.find(name);
    return it != properties_.end() && it->second.is_callable();
}

Value Object::call_method(const std::string& name, const std::vector<Value>& args, Interpreter& interp) {
    auto it = properties_.find(name);
    if (it == properties_.end() || !it->second.is_callable()) {
        throw ScriptException(ScriptError::UndefinedProperty, "Undefined method: " + name);
    }

    Callable* callable = it->second.as_callable();
    return callable->call(interp, args);
}

// =============================================================================
// NativeFunction Implementation
// =============================================================================

NativeFunction::NativeFunction(std::string name, std::size_t arity, Func func)
    : name_(std::move(name)), arity_(arity), func_(std::move(func)) {}

std::string NativeFunction::to_string() const {
    return "<native fn " + name_ + ">";
}

Value NativeFunction::call(Interpreter& interp, const std::vector<Value>& args) {
    return func_(interp, args);
}

// =============================================================================
// ScriptException Implementation
// =============================================================================

ScriptException::ScriptException(ScriptError error, std::string message, SourceLocation location)
    : error_(error), message_(std::move(message)), location_(location) {}

std::string ScriptException::format() const {
    std::ostringstream ss;
    ss << script_error_name(error_) << ": " << message_;
    if (location_.line > 0) {
        ss << " at " << location_.to_string();
    }
    return ss.str();
}

} // namespace void_script
