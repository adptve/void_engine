#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_shell

#include <void_engine/core/error.hpp>

#include <cstdint>
#include <functional>

namespace void_shell {

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Command identifier
struct CommandId {
    std::uint32_t value = 0;

    bool operator==(const CommandId&) const = default;
    bool operator!=(const CommandId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Session identifier
struct SessionId {
    std::uint32_t value = 0;

    bool operator==(const SessionId&) const = default;
    bool operator!=(const SessionId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Alias identifier
struct AliasId {
    std::uint32_t value = 0;

    bool operator==(const AliasId&) const = default;
    bool operator!=(const AliasId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Connection identifier for remote sessions
struct ConnectionId {
    std::uint32_t value = 0;

    bool operator==(const ConnectionId&) const = default;
    bool operator!=(const ConnectionId&) const = default;
    explicit operator bool() const { return value != 0; }
};

// =============================================================================
// Forward Declarations
// =============================================================================

// Types
struct CommandArg;
struct CommandResult;
struct ParsedCommand;
struct Token;

// Commands
class ICommand;
class CommandRegistry;
class CommandBuilder;

// Parser
class Lexer;
class Parser;

// Session
class Environment;
class History;
class Session;
class SessionManager;

// Shell
class Shell;
class ShellSystem;

// Remote
class RemoteServer;
class RemoteClient;

// =============================================================================
// Result Types
// =============================================================================

/// @brief Shell error codes
enum class ShellError {
    None = 0,
    CommandNotFound,
    InvalidArguments,
    InvalidSyntax,
    ExecutionFailed,
    PermissionDenied,
    SessionNotFound,
    ConnectionFailed,
    Timeout,
    Cancelled,
    IoError,
    InternalError
};

/// @brief Shell result type
template <typename T>
using ShellResult = void_core::Result<T, ShellError>;

// =============================================================================
// Callback Types
// =============================================================================

/// @brief Output callback for shell output
using OutputCallback = std::function<void(const std::string&)>;

/// @brief Error callback for shell errors
using ErrorCallback = std::function<void(const std::string&)>;

/// @brief Prompt callback for custom prompts
using PromptCallback = std::function<std::string()>;

/// @brief Completion callback for tab completion
using CompletionCallback = std::function<std::vector<std::string>(
    const std::string& input, std::size_t cursor_pos)>;

} // namespace void_shell

// Hash specializations
template <>
struct std::hash<void_shell::CommandId> {
    std::size_t operator()(const void_shell::CommandId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

template <>
struct std::hash<void_shell::SessionId> {
    std::size_t operator()(const void_shell::SessionId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

template <>
struct std::hash<void_shell::ConnectionId> {
    std::size_t operator()(const void_shell::ConnectionId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

template <>
struct std::hash<void_shell::AliasId> {
    std::size_t operator()(const void_shell::AliasId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};
