#pragma once

/// @file command.hpp
/// @brief Command interface and registry

#include "types.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_shell {

// Forward declarations
class Session;
class Environment;

// =============================================================================
// Command Interface
// =============================================================================

/// @brief Command execution context
struct CommandContext {
    Session* session = nullptr;
    OutputCallback output;
    ErrorCallback error;
    std::function<bool()> is_cancelled;
    std::any user_data;
    std::optional<std::string> stdin_content;  // For piped input
    CommandRegistry* registry = nullptr;       // Command registry reference
    std::filesystem::path cwd;                 // Current working directory
    Environment* env = nullptr;                // Environment variables
    SessionId session_id = 0;                  // Session ID for convenience

    void print(const std::string& text) const {
        if (output) output(text);
    }

    void println(const std::string& text) const {
        if (output) output(text + "\n");
    }

    void print_error(const std::string& text) const {
        if (error) error(text);
    }

    bool cancelled() const {
        return is_cancelled && is_cancelled();
    }

    // Path completion helper
    std::vector<std::string> complete_path(const std::string& partial) const;
};

/// @brief Command interface
class ICommand {
public:
    virtual ~ICommand() = default;

    /// @brief Execute the command
    virtual CommandResult execute(const CommandArgs& args, CommandContext& ctx) = 0;

    /// @brief Get command metadata
    virtual const CommandInfo& info() const = 0;

    /// @brief Validate arguments before execution
    virtual bool validate(const CommandArgs& args, std::string& error) const {
        (void)args;
        (void)error;
        return true;
    }

    /// @brief Get completions for argument at position
    virtual std::vector<std::string> complete(const CommandArgs& args,
                                               std::size_t arg_index,
                                               const std::string& partial,
                                               CommandContext& ctx) const {
        (void)args;
        (void)arg_index;
        (void)partial;
        (void)ctx;
        return {};
    }
};

// =============================================================================
// Function Command
// =============================================================================

/// @brief Command function type
using CommandFunction = std::function<CommandResult(const CommandArgs&, CommandContext&)>;

/// @brief Completion function type
using CompletionFunction = std::function<std::vector<std::string>(
    const CommandArgs& args, std::size_t arg_index,
    const std::string& partial, CommandContext& ctx)>;

/// @brief Function-based command implementation
class FunctionCommand : public ICommand {
public:
    FunctionCommand(CommandInfo info, CommandFunction func);

    CommandResult execute(const CommandArgs& args, CommandContext& ctx) override;
    const CommandInfo& info() const override { return info_; }

    bool validate(const CommandArgs& args, std::string& error) const;

    std::vector<std::string> complete(const CommandArgs& args,
                                       std::size_t arg_index,
                                       const std::string& partial,
                                       CommandContext& ctx) const;

    void set_completer(CompletionFunction comp) { completer_ = std::move(comp); }

private:
    CommandInfo info_;
    CommandFunction function_;
    CompletionFunction completer_;
};

// =============================================================================
// Command Builder
// =============================================================================

/// @brief Fluent builder for commands
class CommandBuilder {
public:
    CommandBuilder(const std::string& name);

    CommandBuilder& description(const std::string& desc);
    CommandBuilder& usage(const std::string& usage);
    CommandBuilder& example(const std::string& example);
    CommandBuilder& category(CommandCategory cat);
    CommandBuilder& alias(const std::string& alias);
    CommandBuilder& hidden(bool h = true);
    CommandBuilder& privileged(bool p = true);

    // Argument builders
    CommandBuilder& arg(const std::string& name, ArgType type,
                         const std::string& desc, bool required = true);
    CommandBuilder& arg_with_default(const std::string& name, ArgType type,
                                      const std::string& desc, const ArgValue& default_val);
    CommandBuilder& flag(const std::string& name, char short_name,
                          const std::string& desc);
    CommandBuilder& flag_with_value(const std::string& name, char short_name,
                                     ArgType type, const std::string& desc);
    CommandBuilder& variadic(const std::string& name, ArgType type,
                              const std::string& desc);

    // Callback setters
    CommandBuilder& function(CommandFunction func);
    CommandBuilder& completer(CompletionFunction comp);

    // Build and register
    std::unique_ptr<ICommand> build();
    CommandId register_to(CommandRegistry& registry);

    // Get info for inspection
    const CommandInfo& get_info() const { return info_; }

private:
    CommandInfo info_;
    CommandFunction function_;
    CompletionFunction completer_;
};

// =============================================================================
// Command Alias
// =============================================================================

/// @brief Command alias definition
struct CommandAlias {
    AliasId id;
    std::string name;
    std::string expansion;  // Full command string to execute
    bool user_defined = true;
};

// =============================================================================
// Command Registry
// =============================================================================

/// @brief Command registry managing all available commands
class CommandRegistry {
public:
    CommandRegistry();
    ~CommandRegistry();

    // Singleton access
    static CommandRegistry& instance();

    // ==========================================================================
    // Command Registration
    // ==========================================================================

    /// @brief Register a command
    CommandId register_command(std::unique_ptr<ICommand> command);

    /// @brief Register a command using builder pattern
    CommandId register_command(const std::string& name, const std::string& description,
                                CommandFunction callback);

    /// @brief Unregister a command
    bool unregister_command(CommandId id);

    /// @brief Unregister by name
    bool unregister_command(const std::string& name);

    /// @brief Check if command exists
    bool exists(const std::string& name) const;

    /// @brief Find command by name (non-const)
    ICommand* find(const std::string& name);

    /// @brief Find command by name (const)
    const ICommand* find(const std::string& name) const;

    /// @brief Find command ID by name
    CommandId find_id(const std::string& name) const;

    /// @brief Get command by ID (non-const)
    ICommand* get(CommandId id);

    /// @brief Get command by ID (const)
    const ICommand* get(CommandId id) const;

    /// @brief Get command info
    const CommandInfo* get_info(const std::string& name) const;

    // ==========================================================================
    // Aliases
    // ==========================================================================

    /// @brief Register an alias
    AliasId add_alias(const std::string& name, const std::string& expansion);

    /// @brief Unregister an alias by name
    bool remove_alias(const std::string& name);

    /// @brief Unregister an alias by ID
    bool remove_alias(AliasId id);

    /// @brief Check if alias exists
    bool is_alias(const std::string& name) const;

    /// @brief Get alias expansion
    std::optional<std::string> get_alias(const std::string& name) const;

    /// @brief Get all aliases as map
    std::unordered_map<std::string, std::string> all_aliases() const;

    /// @brief Get alias count
    std::size_t alias_count() const;

    // ==========================================================================
    // Querying
    // ==========================================================================

    /// @brief Get all registered commands
    std::vector<const CommandInfo*> all_commands() const;

    /// @brief Get all command names
    std::vector<std::string> command_names() const;

    /// @brief Get commands by category
    std::vector<const CommandInfo*> commands_in_category(CommandCategory cat) const;

    /// @brief Get command count
    std::size_t count() const;

    /// @brief Search commands by name/description
    std::vector<const CommandInfo*> search(const std::string& query) const;

    // ==========================================================================
    // Completion
    // ==========================================================================

    /// @brief Get command completions for partial input
    std::vector<std::string> complete_command(const std::string& prefix) const;

    /// @brief Get argument completions for a command
    std::vector<std::string> complete_argument(const std::string& command_name,
                                                const CommandArgs& args,
                                                std::size_t arg_index,
                                                const std::string& partial,
                                                CommandContext& ctx) const;

    // ==========================================================================
    // Hot-Reload Support
    // ==========================================================================

    /// @brief Mark commands for a module (for hot-reload tracking)
    void mark_module_commands(const std::string& module_name,
                               const std::vector<CommandId>& command_ids);

    /// @brief Unregister all commands from a module
    void unregister_module_commands(const std::string& module_name);

    /// @brief Get commands for a module
    std::vector<CommandId> get_module_commands(const std::string& module_name) const;

    /// @brief Callback when commands are reloaded
    using ReloadCallback = std::function<void(const std::string& module_name)>;
    void set_reload_callback(ReloadCallback callback);

private:
    mutable std::mutex mutex_;

    std::unordered_map<CommandId, std::unique_ptr<ICommand>> commands_;
    std::unordered_map<std::string, CommandId> name_to_id_;
    std::unordered_map<std::string, std::string> aliases_;
    std::unordered_map<AliasId, std::string> alias_ids_;
    std::unordered_map<CommandCategory, std::vector<CommandId>> category_index_;

    // Internal helper for finding commands
    ICommand* find_internal(const std::string& name) const;

    // Module tracking for hot-reload
    std::unordered_map<std::string, std::vector<CommandId>> module_commands_;

    ReloadCallback reload_callback_;

    std::uint32_t next_id_ = 1;
    std::uint32_t next_alias_id_ = 1;
};

// =============================================================================
// Command Registration Macro
// =============================================================================

/// @brief Helper macro for command registration
#define VOID_REGISTER_COMMAND(name, callback, ...) \
    void_shell::CommandRegistry::instance().register_command( \
        name, callback, [](void_shell::CommandBuilder& b) { __VA_ARGS__ })

} // namespace void_shell
