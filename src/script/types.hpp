#pragma once

/// @file types.hpp
/// @brief Core types and enumerations for void_script

#include "fwd.hpp"
#include <void_engine/core/error.hpp>

#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace void_script {

// =============================================================================
// Token Types
// =============================================================================

/// @brief Token types for the lexer
enum class TokenType : std::uint8_t {
    // Literals
    Integer,            ///< 123, 0xFF, 0b1010
    Float,              ///< 1.5, 1e10, .5
    String,             ///< "hello", 'world'
    True,               ///< true
    False,              ///< false
    Null,               ///< null

    // Identifiers and Keywords
    Identifier,         ///< foo, bar_123
    Let,                ///< let
    Const,              ///< const
    Var,                ///< var
    Fn,                 ///< fn
    Return,             ///< return
    If,                 ///< if
    Else,               ///< else
    While,              ///< while
    For,                ///< for
    In,                 ///< in
    Break,              ///< break
    Continue,           ///< continue
    Match,              ///< match
    Class,              ///< class
    Struct,             ///< struct
    Enum,               ///< enum
    This,               ///< this
    Super,              ///< super
    New,                ///< new
    Import,             ///< import
    Export,             ///< export
    From,               ///< from
    As,                 ///< as
    Module,             ///< module
    Pub,                ///< pub
    Try,                ///< try
    Catch,              ///< catch
    Finally,            ///< finally
    Throw,              ///< throw
    Async,              ///< async
    Await,              ///< await
    Yield,              ///< yield
    Type,               ///< type
    Interface,          ///< interface
    Impl,               ///< impl
    Static,             ///< static

    // Operators
    Plus,               ///< +
    Minus,              ///< -
    Star,               ///< *
    Slash,              ///< /
    Percent,            ///< %
    Power,              ///< **
    Ampersand,          ///< &
    Pipe,               ///< |
    Caret,              ///< ^
    Tilde,              ///< ~
    ShiftLeft,          ///< <<
    ShiftRight,         ///< >>

    // Comparison
    Equal,              ///< ==
    NotEqual,           ///< !=
    Less,               ///< <
    LessEqual,          ///< <=
    Greater,            ///< >
    GreaterEqual,       ///< >=
    Spaceship,          ///< <=>

    // Logical
    And,                ///< &&
    Or,                 ///< ||
    Not,                ///< !

    // Assignment
    Assign,             ///< =
    PlusAssign,         ///< +=
    MinusAssign,        ///< -=
    StarAssign,         ///< *=
    SlashAssign,        ///< /=
    PercentAssign,      ///< %=
    AmpersandAssign,    ///< &=
    PipeAssign,         ///< |=
    CaretAssign,        ///< ^=
    ShiftLeftAssign,    ///< <<=
    ShiftRightAssign,   ///< >>=

    // Punctuation
    LeftParen,          ///< (
    RightParen,         ///< )
    LeftBrace,          ///< {
    RightBrace,         ///< }
    LeftBracket,        ///< [
    RightBracket,       ///< ]
    Comma,              ///< ,
    Dot,                ///< .
    DotDot,             ///< ..
    DotDotDot,          ///< ...
    Colon,              ///< :
    ColonColon,         ///< ::
    Semicolon,          ///< ;
    Arrow,              ///< ->
    FatArrow,           ///< =>
    Question,           ///< ?
    QuestionQuestion,   ///< ??
    QuestionDot,        ///< ?.
    At,                 ///< @
    Hash,               ///< #
    Backslash,          ///< backslash
    Increment,          ///< ++
    Decrement,          ///< --

    // Special
    Newline,            ///< Significant newline
    Eof,                ///< End of file
    Error,              ///< Lexer error

    Count
};

/// @brief Get string name for token type
[[nodiscard]] constexpr const char* token_type_name(TokenType type);

// =============================================================================
// Source Location
// =============================================================================

/// @brief Location in source code
struct SourceLocation {
    std::string_view file;      ///< Source file path
    std::uint32_t line = 1;     ///< Line number (1-based)
    std::uint32_t column = 1;   ///< Column number (1-based)
    std::uint32_t offset = 0;   ///< Byte offset from start

    [[nodiscard]] std::string to_string() const;
};

/// @brief Source span
struct SourceSpan {
    SourceLocation start;
    SourceLocation end;
};

// =============================================================================
// Token
// =============================================================================

/// @brief Lexical token
struct Token {
    TokenType type = TokenType::Error;
    std::string_view lexeme;        ///< Text of the token
    SourceLocation location;

    // Literal values
    std::int64_t int_value = 0;
    double float_value = 0.0;
    std::string string_value;

    [[nodiscard]] bool is(TokenType t) const { return type == t; }
    [[nodiscard]] bool is_keyword() const;
    [[nodiscard]] bool is_operator() const;
    [[nodiscard]] bool is_literal() const;
    [[nodiscard]] bool is_assignment() const;
    [[nodiscard]] std::string to_string() const;
};

// =============================================================================
// Value Type
// =============================================================================

/// @brief Runtime type enumeration
enum class ValueType : std::uint8_t {
    Null,
    Bool,
    Int,
    Float,
    String,
    Array,
    Map,
    Object,
    Function,
    Class,
    Module,
    Native,

    Count
};

/// @brief Get string name for value type
[[nodiscard]] constexpr const char* value_type_name(ValueType type);

// =============================================================================
// Value
// =============================================================================

// Forward declaration for recursive types
class Value;
using ValuePtr = std::shared_ptr<Value>;
using ValueArray = std::vector<Value>;
using ValueMap = std::unordered_map<std::string, Value>;

/// @brief Runtime value
class Value {
public:
    // Constructors
    Value() : type_(ValueType::Null) {}
    Value(std::nullptr_t) : type_(ValueType::Null) {}
    Value(bool v) : type_(ValueType::Bool), bool_value_(v) {}
    Value(std::int64_t v) : type_(ValueType::Int), int_value_(v) {}
    Value(int v) : type_(ValueType::Int), int_value_(v) {}
    Value(double v) : type_(ValueType::Float), float_value_(v) {}
    Value(const std::string& v) : type_(ValueType::String), string_value_(std::make_shared<std::string>(v)) {}
    Value(std::string&& v) : type_(ValueType::String), string_value_(std::make_shared<std::string>(std::move(v))) {}
    Value(const char* v) : type_(ValueType::String), string_value_(std::make_shared<std::string>(v)) {}
    Value(ValueArray arr) : type_(ValueType::Array), array_value_(std::make_shared<ValueArray>(std::move(arr))) {}
    Value(ValueMap map) : type_(ValueType::Map), map_value_(std::make_shared<ValueMap>(std::move(map))) {}

    // Type checking
    [[nodiscard]] ValueType type() const { return type_; }
    [[nodiscard]] bool is_null() const { return type_ == ValueType::Null; }
    [[nodiscard]] bool is_bool() const { return type_ == ValueType::Bool; }
    [[nodiscard]] bool is_int() const { return type_ == ValueType::Int; }
    [[nodiscard]] bool is_float() const { return type_ == ValueType::Float; }
    [[nodiscard]] bool is_number() const { return is_int() || is_float(); }
    [[nodiscard]] bool is_string() const { return type_ == ValueType::String; }
    [[nodiscard]] bool is_array() const { return type_ == ValueType::Array; }
    [[nodiscard]] bool is_map() const { return type_ == ValueType::Map; }
    [[nodiscard]] bool is_object() const { return type_ == ValueType::Object; }
    [[nodiscard]] bool is_function() const { return type_ == ValueType::Function; }
    [[nodiscard]] bool is_callable() const;

    // Value access
    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] std::int64_t as_int() const;
    [[nodiscard]] double as_float() const;
    [[nodiscard]] double as_number() const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] ValueArray& as_array();
    [[nodiscard]] const ValueArray& as_array() const;
    [[nodiscard]] ValueMap& as_map();
    [[nodiscard]] const ValueMap& as_map() const;
    [[nodiscard]] Object* as_object();
    [[nodiscard]] const Object* as_object() const;
    [[nodiscard]] Callable* as_callable();

    // Object storage
    void set_object(std::shared_ptr<Object> obj);
    [[nodiscard]] std::shared_ptr<Object> get_object_ptr() const { return object_value_; }

    // Truthiness
    [[nodiscard]] bool is_truthy() const;

    // Comparison
    [[nodiscard]] bool equals(const Value& other) const;
    [[nodiscard]] int compare(const Value& other) const;

    // Operators
    [[nodiscard]] bool operator==(const Value& other) const { return equals(other); }
    [[nodiscard]] bool operator!=(const Value& other) const { return !equals(other); }
    [[nodiscard]] bool operator<(const Value& other) const { return compare(other) < 0; }
    [[nodiscard]] bool operator<=(const Value& other) const { return compare(other) <= 0; }
    [[nodiscard]] bool operator>(const Value& other) const { return compare(other) > 0; }
    [[nodiscard]] bool operator>=(const Value& other) const { return compare(other) >= 0; }

    // Conversion
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] std::string type_name() const;

    // Static constructors
    static Value make_array(ValueArray arr = {});
    static Value make_map(ValueMap map = {});
    static Value make_object(std::shared_ptr<Object> obj);
    static Value make_function(std::shared_ptr<Callable> fn);

private:
    ValueType type_;

    // Value storage (union-like)
    bool bool_value_ = false;
    std::int64_t int_value_ = 0;
    double float_value_ = 0.0;
    std::shared_ptr<std::string> string_value_;
    std::shared_ptr<ValueArray> array_value_;
    std::shared_ptr<ValueMap> map_value_;
    std::shared_ptr<Object> object_value_;
};

// =============================================================================
// Object Base Class
// =============================================================================

/// @brief Base class for script objects
class Object {
public:
    virtual ~Object() = default;

    [[nodiscard]] virtual ValueType object_type() const = 0;
    [[nodiscard]] virtual std::string to_string() const = 0;

    // Property access
    [[nodiscard]] virtual bool has_property(const std::string& name) const;
    [[nodiscard]] virtual Value get_property(const std::string& name) const;
    virtual void set_property(const std::string& name, Value value);

    // Method call
    [[nodiscard]] virtual bool has_method(const std::string& name) const;
    [[nodiscard]] virtual Value call_method(const std::string& name,
                                             const std::vector<Value>& args,
                                             Interpreter& interp);

protected:
    std::unordered_map<std::string, Value> properties_;
};

// =============================================================================
// Callable Interface
// =============================================================================

/// @brief Interface for callable objects
class Callable : public Object {
public:
    ~Callable() override = default;

    [[nodiscard]] ValueType object_type() const override { return ValueType::Function; }

    [[nodiscard]] virtual std::size_t arity() const = 0;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual Value call(Interpreter& interp, const std::vector<Value>& args) = 0;
};

/// @brief Native function binding
class NativeFunction : public Callable {
public:
    using Func = std::function<Value(Interpreter&, const std::vector<Value>&)>;

    NativeFunction(std::string name, std::size_t arity, Func func);

    [[nodiscard]] std::size_t arity() const override { return arity_; }
    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] Value call(Interpreter& interp, const std::vector<Value>& args) override;

private:
    std::string name_;
    std::size_t arity_;
    Func func_;
};

// =============================================================================
// Script Errors
// =============================================================================

/// @brief Script error types
enum class ScriptError {
    None = 0,

    // Lexer errors
    UnexpectedCharacter,
    UnterminatedString,
    UnterminatedComment,
    InvalidNumber,
    InvalidEscape,

    // Parser errors
    UnexpectedToken,
    ExpectedExpression,
    ExpectedStatement,
    ExpectedIdentifier,
    ExpectedType,
    ExpectedSemicolon,
    ExpectedRightParen,
    ExpectedRightBrace,
    ExpectedRightBracket,
    TooManyParameters,
    TooManyArguments,
    InvalidAssignmentTarget,

    // Type errors
    TypeMismatch,
    UndefinedVariable,
    UndefinedFunction,
    UndefinedType,
    UndefinedProperty,
    NotCallable,
    NotIndexable,
    NotIterable,
    WrongArgumentCount,

    // Runtime errors
    DivisionByZero,
    IndexOutOfBounds,
    StackOverflow,
    RecursionLimit,
    InvalidOperation,
    NullReference,
    AssertionFailed,
    Timeout,

    // System errors
    FileNotFound,
    ImportError,
    ModuleNotFound,
    CircularImport,

    Count
};

/// @brief Get error name
[[nodiscard]] constexpr const char* script_error_name(ScriptError error);

/// @brief Script exception
class ScriptException : public std::exception {
public:
    ScriptException(ScriptError error, std::string message, SourceLocation location = {});

    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }
    [[nodiscard]] ScriptError error() const { return error_; }
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] const SourceLocation& location() const { return location_; }
    [[nodiscard]] std::string format() const;

private:
    ScriptError error_;
    std::string message_;
    SourceLocation location_;
};

/// @brief Result type for script operations
template <typename T>
using ScriptResult = void_core::Result<T, ScriptError>;

// =============================================================================
// Implementation Details
// =============================================================================

constexpr const char* token_type_name(TokenType type) {
    switch (type) {
        case TokenType::Integer: return "Integer";
        case TokenType::Float: return "Float";
        case TokenType::String: return "String";
        case TokenType::True: return "true";
        case TokenType::False: return "false";
        case TokenType::Null: return "null";
        case TokenType::Identifier: return "Identifier";
        case TokenType::Let: return "let";
        case TokenType::Const: return "const";
        case TokenType::Var: return "var";
        case TokenType::Fn: return "fn";
        case TokenType::Return: return "return";
        case TokenType::If: return "if";
        case TokenType::Else: return "else";
        case TokenType::While: return "while";
        case TokenType::For: return "for";
        case TokenType::In: return "in";
        case TokenType::Break: return "break";
        case TokenType::Continue: return "continue";
        case TokenType::Match: return "match";
        case TokenType::Class: return "class";
        case TokenType::Struct: return "struct";
        case TokenType::Enum: return "enum";
        case TokenType::This: return "this";
        case TokenType::Super: return "super";
        case TokenType::New: return "new";
        case TokenType::Import: return "import";
        case TokenType::Export: return "export";
        case TokenType::From: return "from";
        case TokenType::As: return "as";
        case TokenType::Module: return "module";
        case TokenType::Pub: return "pub";
        case TokenType::Try: return "try";
        case TokenType::Catch: return "catch";
        case TokenType::Finally: return "finally";
        case TokenType::Throw: return "throw";
        case TokenType::Async: return "async";
        case TokenType::Await: return "await";
        case TokenType::Yield: return "yield";
        case TokenType::Type: return "type";
        case TokenType::Interface: return "interface";
        case TokenType::Impl: return "impl";
        case TokenType::Static: return "static";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Percent: return "%";
        case TokenType::Power: return "**";
        case TokenType::Equal: return "==";
        case TokenType::NotEqual: return "!=";
        case TokenType::Less: return "<";
        case TokenType::LessEqual: return "<=";
        case TokenType::Greater: return ">";
        case TokenType::GreaterEqual: return ">=";
        case TokenType::And: return "&&";
        case TokenType::Or: return "||";
        case TokenType::Not: return "!";
        case TokenType::Assign: return "=";
        case TokenType::LeftParen: return "(";
        case TokenType::RightParen: return ")";
        case TokenType::LeftBrace: return "{";
        case TokenType::RightBrace: return "}";
        case TokenType::LeftBracket: return "[";
        case TokenType::RightBracket: return "]";
        case TokenType::Comma: return ",";
        case TokenType::Dot: return ".";
        case TokenType::Colon: return ":";
        case TokenType::Semicolon: return ";";
        case TokenType::Arrow: return "->";
        case TokenType::FatArrow: return "=>";
        case TokenType::Eof: return "EOF";
        case TokenType::Error: return "Error";
        default: return "Unknown";
    }
}

constexpr const char* value_type_name(ValueType type) {
    switch (type) {
        case ValueType::Null: return "null";
        case ValueType::Bool: return "bool";
        case ValueType::Int: return "int";
        case ValueType::Float: return "float";
        case ValueType::String: return "string";
        case ValueType::Array: return "array";
        case ValueType::Map: return "map";
        case ValueType::Object: return "object";
        case ValueType::Function: return "function";
        case ValueType::Class: return "class";
        case ValueType::Module: return "module";
        case ValueType::Native: return "native";
        default: return "unknown";
    }
}

constexpr const char* script_error_name(ScriptError error) {
    switch (error) {
        case ScriptError::None: return "None";
        case ScriptError::UnexpectedCharacter: return "Unexpected character";
        case ScriptError::UnterminatedString: return "Unterminated string";
        case ScriptError::UnterminatedComment: return "Unterminated comment";
        case ScriptError::InvalidNumber: return "Invalid number";
        case ScriptError::InvalidEscape: return "Invalid escape sequence";
        case ScriptError::UnexpectedToken: return "Unexpected token";
        case ScriptError::ExpectedExpression: return "Expected expression";
        case ScriptError::ExpectedStatement: return "Expected statement";
        case ScriptError::ExpectedIdentifier: return "Expected identifier";
        case ScriptError::ExpectedType: return "Expected type";
        case ScriptError::ExpectedSemicolon: return "Expected semicolon";
        case ScriptError::ExpectedRightParen: return "Expected ')'";
        case ScriptError::ExpectedRightBrace: return "Expected '}'";
        case ScriptError::ExpectedRightBracket: return "Expected ']'";
        case ScriptError::TooManyParameters: return "Too many parameters";
        case ScriptError::TooManyArguments: return "Too many arguments";
        case ScriptError::InvalidAssignmentTarget: return "Invalid assignment target";
        case ScriptError::TypeMismatch: return "Type mismatch";
        case ScriptError::UndefinedVariable: return "Undefined variable";
        case ScriptError::UndefinedFunction: return "Undefined function";
        case ScriptError::UndefinedType: return "Undefined type";
        case ScriptError::UndefinedProperty: return "Undefined property";
        case ScriptError::NotCallable: return "Not callable";
        case ScriptError::NotIndexable: return "Not indexable";
        case ScriptError::NotIterable: return "Not iterable";
        case ScriptError::WrongArgumentCount: return "Wrong argument count";
        case ScriptError::DivisionByZero: return "Division by zero";
        case ScriptError::IndexOutOfBounds: return "Index out of bounds";
        case ScriptError::StackOverflow: return "Stack overflow";
        case ScriptError::RecursionLimit: return "Recursion limit exceeded";
        case ScriptError::InvalidOperation: return "Invalid operation";
        case ScriptError::NullReference: return "Null reference";
        case ScriptError::AssertionFailed: return "Assertion failed";
        case ScriptError::Timeout: return "Execution timeout";
        case ScriptError::FileNotFound: return "File not found";
        case ScriptError::ImportError: return "Import error";
        case ScriptError::ModuleNotFound: return "Module not found";
        case ScriptError::CircularImport: return "Circular import";
        default: return "Unknown error";
    }
}

} // namespace void_script
