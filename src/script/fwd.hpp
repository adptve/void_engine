#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_script module

#include <void_engine/core/handle.hpp>
#include <cstdint>

namespace void_script {

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Unique identifier for a script
struct ScriptIdTag {};
using ScriptId = void_core::Handle<ScriptIdTag>;

/// @brief Unique identifier for a function
struct FunctionIdTag {};
using FunctionId = void_core::Handle<FunctionIdTag>;

/// @brief Unique identifier for a class
struct ClassIdTag {};
using ClassId = void_core::Handle<ClassIdTag>;

/// @brief Unique identifier for a variable
struct VarIdTag {};
using VarId = void_core::Handle<VarIdTag>;

/// @brief Unique identifier for a module
struct ModuleIdTag {};
using ModuleId = void_core::Handle<ModuleIdTag>;

// =============================================================================
// Forward Declarations
// =============================================================================

// Lexer
struct Token;
struct SourceLocation;
class Lexer;

// AST Nodes
class AstNode;
class Expression;
class Statement;
class Declaration;

// Expressions
class LiteralExpr;
class IdentifierExpr;
class BinaryExpr;
class UnaryExpr;
class CallExpr;
class MemberExpr;
class IndexExpr;
class AssignExpr;
class TernaryExpr;
class LambdaExpr;
class ArrayExpr;
class MapExpr;

// Statements
class ExprStatement;
class BlockStatement;
class IfStatement;
class WhileStatement;
class ForStatement;
class ForEachStatement;
class ReturnStatement;
class BreakStatement;
class ContinueStatement;
class MatchStatement;
class TryCatchStatement;

// Declarations
class VarDecl;
class FunctionDecl;
class ClassDecl;
class ModuleDecl;
class ImportDecl;
class ExportDecl;

// Parser
class Parser;
class ParserError;

// Types
class Type;
class TypeChecker;

// Values
class Value;
class Object;
class Callable;
class NativeFunction;
class ScriptFunction;
class ClassInstance;

// Runtime
class Environment;
class CallFrame;
class Interpreter;
class VirtualMachine;

// Compilation
class Compiler;
class Chunk;
class BytecodeGenerator;

// System
class ScriptContext;
class ScriptEngine;
class NativeBinding;

} // namespace void_script
