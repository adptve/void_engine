/// @file command.cpp
/// @brief Command system implementation for void_shell

#include "command.hpp"

#include <algorithm>
#include <sstream>

namespace void_shell {

// =============================================================================
// FunctionCommand Implementation
// =============================================================================

FunctionCommand::FunctionCommand(CommandInfo info, CommandFunction func)
    : info_(std::move(info)), function_(std::move(func)) {}

CommandResult FunctionCommand::execute(const CommandArgs& args, CommandContext& ctx) {
    if (function_) {
        return function_(args, ctx);
    }
    return CommandResult::error("Command has no function bound");
}

bool FunctionCommand::validate(const CommandArgs& args, std::string& error) const {
    // Check required arguments
    std::size_t required_count = 0;
    for (const auto& spec : info_.args) {
        if (spec.required) {
            required_count++;
        }
    }

    if (args.positional().size() < required_count) {
        error = "Not enough arguments. Expected at least " +
                std::to_string(required_count) + ", got " +
                std::to_string(args.positional().size());
        return false;
    }

    // Validate argument types
    for (std::size_t i = 0; i < std::min(args.positional().size(), info_.args.size()); ++i) {
        const auto& spec = info_.args[i];
        const auto& arg = args.positional()[i];

        // Type validation could be more thorough, but basic check is ok
        if (spec.type == ArgType::Integer) {
            // Try to convert to int
            auto val = arg.as_int();
            (void)val; // Just check it doesn't throw
        } else if (spec.type == ArgType::Float) {
            auto val = arg.as_float();
            (void)val;
        }
    }

    return true;
}

std::vector<std::string> FunctionCommand::complete(const CommandArgs& args,
                                                    std::size_t arg_index,
                                                    const std::string& partial,
                                                    CommandContext& ctx) const {
    // If a custom completer is set, use it
    if (completer_) {
        return completer_(args, arg_index, partial, ctx);
    }

    // Default completion based on argument spec
    if (arg_index < info_.args.size()) {
        const auto& spec = info_.args[arg_index];

        if (spec.type == ArgType::Path) {
            // File path completion - delegate to context
            return ctx.complete_path(partial);
        } else if (spec.type == ArgType::Boolean) {
            std::vector<std::string> completions;
            if (partial.empty() || "true"[0] == partial[0]) {
                completions.push_back("true");
            }
            if (partial.empty() || "false"[0] == partial[0]) {
                completions.push_back("false");
            }
            return completions;
        }
    }

    return {};
}

// =============================================================================
// CommandBuilder Implementation
// =============================================================================

CommandBuilder::CommandBuilder(const std::string& name) {
    info_.name = name;
    info_.category = CommandCategory::General;
}

CommandBuilder& CommandBuilder::description(const std::string& desc) {
    info_.description = desc;
    return *this;
}

CommandBuilder& CommandBuilder::usage(const std::string& usage) {
    info_.usage = usage;
    return *this;
}

CommandBuilder& CommandBuilder::example(const std::string& example) {
    info_.examples.push_back(example);
    return *this;
}

CommandBuilder& CommandBuilder::category(CommandCategory cat) {
    info_.category = cat;
    return *this;
}

CommandBuilder& CommandBuilder::alias(const std::string& alias) {
    info_.aliases.push_back(alias);
    return *this;
}

CommandBuilder& CommandBuilder::arg(const std::string& name, ArgType type,
                                     const std::string& desc, bool required) {
    ArgSpec spec;
    spec.name = name;
    spec.type = type;
    spec.description = desc;
    spec.required = required;
    info_.args.push_back(std::move(spec));
    return *this;
}

CommandBuilder& CommandBuilder::arg_with_default(const std::string& name, ArgType type,
                                                  const std::string& desc,
                                                  const ArgValue& default_val) {
    ArgSpec spec;
    spec.name = name;
    spec.type = type;
    spec.description = desc;
    spec.required = false;
    spec.default_value = default_val;
    info_.args.push_back(std::move(spec));
    return *this;
}

CommandBuilder& CommandBuilder::flag(const std::string& name, char short_name,
                                      const std::string& desc) {
    FlagSpec spec;
    spec.name = name;
    spec.short_name = short_name;
    spec.description = desc;
    spec.takes_value = false;
    info_.flags.push_back(std::move(spec));
    return *this;
}

CommandBuilder& CommandBuilder::flag_with_value(const std::string& name, char short_name,
                                                 ArgType type, const std::string& desc) {
    FlagSpec spec;
    spec.name = name;
    spec.short_name = short_name;
    spec.description = desc;
    spec.takes_value = true;
    spec.value_type = type;
    info_.flags.push_back(std::move(spec));
    return *this;
}

CommandBuilder& CommandBuilder::variadic(const std::string& name, ArgType type,
                                          const std::string& desc) {
    info_.variadic = true;
    info_.variadic_name = name;
    info_.variadic_type = type;
    info_.variadic_desc = desc;
    return *this;
}

CommandBuilder& CommandBuilder::hidden(bool h) {
    info_.hidden = h;
    return *this;
}

CommandBuilder& CommandBuilder::privileged(bool p) {
    info_.privileged = p;
    return *this;
}

CommandBuilder& CommandBuilder::function(CommandFunction func) {
    function_ = std::move(func);
    return *this;
}

CommandBuilder& CommandBuilder::completer(CompletionFunction comp) {
    completer_ = std::move(comp);
    return *this;
}

std::unique_ptr<ICommand> CommandBuilder::build() {
    auto cmd = std::make_unique<FunctionCommand>(std::move(info_), std::move(function_));
    if (completer_) {
        cmd->set_completer(std::move(completer_));
    }
    return cmd;
}

CommandId CommandBuilder::register_to(CommandRegistry& registry) {
    return registry.register_command(build());
}

// =============================================================================
// CommandRegistry Implementation
// =============================================================================

CommandRegistry::CommandRegistry() = default;
CommandRegistry::~CommandRegistry() = default;

CommandId CommandRegistry::register_command(std::unique_ptr<ICommand> command) {
    std::lock_guard<std::mutex> lock(mutex_);

    CommandId id{next_id_++};
    const auto& info = command->info();

    // Store command
    commands_[id] = std::move(command);

    // Index by name
    name_to_id_[info.name] = id;

    // Index aliases
    for (const auto& alias : info.aliases) {
        name_to_id_[alias] = id;
    }

    // Index by category
    category_index_[info.category].push_back(id);

    return id;
}

CommandId CommandRegistry::register_command(const std::string& name,
                                            const std::string& description,
                                            CommandFunction func) {
    return CommandBuilder(name)
        .description(description)
        .function(std::move(func))
        .register_to(*this);
}

bool CommandRegistry::unregister_command(CommandId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = commands_.find(id);
    if (it == commands_.end()) {
        return false;
    }

    const auto& info = it->second->info();

    // Remove from name index
    name_to_id_.erase(info.name);

    // Remove aliases
    for (const auto& alias : info.aliases) {
        name_to_id_.erase(alias);
    }

    // Remove from category index
    auto& cat_cmds = category_index_[info.category];
    cat_cmds.erase(std::remove(cat_cmds.begin(), cat_cmds.end(), id), cat_cmds.end());

    // Remove from module index if tracked
    for (auto& [module, ids] : module_commands_) {
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    }

    commands_.erase(it);
    return true;
}

bool CommandRegistry::unregister_command(const std::string& name) {
    auto id = find_id(name);
    if (!id) {
        return false;
    }
    return unregister_command(id);
}

ICommand* CommandRegistry::find(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return find_internal(name);
}

const ICommand* CommandRegistry::find(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return find_internal(name);
}

ICommand* CommandRegistry::find_internal(const std::string& name) const {
    // Check aliases first
    auto alias_it = aliases_.find(name);
    if (alias_it != aliases_.end()) {
        auto it = name_to_id_.find(alias_it->second);
        if (it != name_to_id_.end()) {
            auto cmd_it = commands_.find(it->second);
            if (cmd_it != commands_.end()) {
                return cmd_it->second.get();
            }
        }
    }

    // Check direct name
    auto it = name_to_id_.find(name);
    if (it != name_to_id_.end()) {
        auto cmd_it = commands_.find(it->second);
        if (cmd_it != commands_.end()) {
            return cmd_it->second.get();
        }
    }

    return nullptr;
}

ICommand* CommandRegistry::get(CommandId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = commands_.find(id);
    return it != commands_.end() ? it->second.get() : nullptr;
}

const ICommand* CommandRegistry::get(CommandId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = commands_.find(id);
    return it != commands_.end() ? it->second.get() : nullptr;
}

CommandId CommandRegistry::find_id(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check aliases first
    auto alias_it = aliases_.find(name);
    if (alias_it != aliases_.end()) {
        auto it = name_to_id_.find(alias_it->second);
        if (it != name_to_id_.end()) {
            return it->second;
        }
    }

    auto it = name_to_id_.find(name);
    return it != name_to_id_.end() ? it->second : CommandId{};
}

bool CommandRegistry::exists(const std::string& name) const {
    return static_cast<bool>(find_id(name));
}

std::vector<const CommandInfo*> CommandRegistry::all_commands() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<const CommandInfo*> result;
    result.reserve(commands_.size());

    for (const auto& [id, cmd] : commands_) {
        if (!cmd->info().hidden) {
            result.push_back(&cmd->info());
        }
    }

    // Sort by name
    std::sort(result.begin(), result.end(),
              [](const CommandInfo* a, const CommandInfo* b) {
                  return a->name < b->name;
              });

    return result;
}

std::vector<const CommandInfo*> CommandRegistry::commands_in_category(CommandCategory cat) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<const CommandInfo*> result;

    auto it = category_index_.find(cat);
    if (it != category_index_.end()) {
        for (CommandId id : it->second) {
            auto cmd_it = commands_.find(id);
            if (cmd_it != commands_.end() && !cmd_it->second->info().hidden) {
                result.push_back(&cmd_it->second->info());
            }
        }
    }

    // Sort by name
    std::sort(result.begin(), result.end(),
              [](const CommandInfo* a, const CommandInfo* b) {
                  return a->name < b->name;
              });

    return result;
}

std::vector<std::string> CommandRegistry::command_names() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    result.reserve(commands_.size());

    for (const auto& [id, cmd] : commands_) {
        if (!cmd->info().hidden) {
            result.push_back(cmd->info().name);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::size_t CommandRegistry::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return commands_.size();
}

// =============================================================================
// Alias Management
// =============================================================================

AliasId CommandRegistry::add_alias(const std::string& name, const std::string& expansion) {
    std::lock_guard<std::mutex> lock(mutex_);

    AliasId id{next_alias_id_++};
    aliases_[name] = expansion;
    alias_ids_[id] = name;

    return id;
}

bool CommandRegistry::remove_alias(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = aliases_.find(name);
    if (it == aliases_.end()) {
        return false;
    }

    // Remove from id map
    for (auto aid_it = alias_ids_.begin(); aid_it != alias_ids_.end(); ++aid_it) {
        if (aid_it->second == name) {
            alias_ids_.erase(aid_it);
            break;
        }
    }

    aliases_.erase(it);
    return true;
}

bool CommandRegistry::remove_alias(AliasId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = alias_ids_.find(id);
    if (it == alias_ids_.end()) {
        return false;
    }

    aliases_.erase(it->second);
    alias_ids_.erase(it);
    return true;
}

std::optional<std::string> CommandRegistry::get_alias(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = aliases_.find(name);
    return it != aliases_.end() ? std::optional(it->second) : std::nullopt;
}

bool CommandRegistry::is_alias(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return aliases_.count(name) > 0;
}

std::unordered_map<std::string, std::string> CommandRegistry::all_aliases() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return aliases_;
}

std::size_t CommandRegistry::alias_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return aliases_.size();
}

// =============================================================================
// Completion
// =============================================================================

std::vector<std::string> CommandRegistry::complete_command(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> completions;

    // Complete from command names
    for (const auto& [name, id] : name_to_id_) {
        if (name.size() >= prefix.size() &&
            name.compare(0, prefix.size(), prefix) == 0) {
            // Check if not hidden
            auto it = commands_.find(id);
            if (it != commands_.end() && !it->second->info().hidden) {
                completions.push_back(name);
            }
        }
    }

    // Complete from aliases
    for (const auto& [name, expansion] : aliases_) {
        if (name.size() >= prefix.size() &&
            name.compare(0, prefix.size(), prefix) == 0) {
            completions.push_back(name);
        }
    }

    std::sort(completions.begin(), completions.end());
    completions.erase(std::unique(completions.begin(), completions.end()), completions.end());

    return completions;
}

std::vector<std::string> CommandRegistry::complete_argument(const std::string& command_name,
                                                             const CommandArgs& args,
                                                             std::size_t arg_index,
                                                             const std::string& partial,
                                                             CommandContext& ctx) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto* cmd = find_internal(command_name);
    if (!cmd) {
        return {};
    }

    return cmd->complete(args, arg_index, partial, ctx);
}

// =============================================================================
// Hot-Reload Support
// =============================================================================

void CommandRegistry::mark_module_commands(const std::string& module_name,
                                            const std::vector<CommandId>& command_ids) {
    std::lock_guard<std::mutex> lock(mutex_);
    module_commands_[module_name] = command_ids;
}

void CommandRegistry::unregister_module_commands(const std::string& module_name) {
    std::vector<CommandId> to_remove;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = module_commands_.find(module_name);
        if (it == module_commands_.end()) {
            return;
        }
        to_remove = it->second;
        module_commands_.erase(it);
    }

    // Unregister each command (outside lock to avoid deadlock)
    for (CommandId id : to_remove) {
        unregister_command(id);
    }
}

std::vector<CommandId> CommandRegistry::get_module_commands(const std::string& module_name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = module_commands_.find(module_name);
    return it != module_commands_.end() ? it->second : std::vector<CommandId>{};
}

// =============================================================================
// CommandContext Implementation
// =============================================================================

std::vector<std::string> CommandContext::complete_path(const std::string& partial) const {
    std::vector<std::string> completions;

    try {
        std::filesystem::path base_path;
        std::string prefix;

        if (partial.empty()) {
            base_path = cwd;
            prefix = "";
        } else {
            std::filesystem::path partial_path(partial);

            if (partial_path.is_absolute()) {
                if (std::filesystem::is_directory(partial_path)) {
                    base_path = partial_path;
                    prefix = partial;
                } else {
                    base_path = partial_path.parent_path();
                    prefix = partial_path.filename().string();
                }
            } else {
                std::filesystem::path full = cwd / partial_path;
                if (std::filesystem::is_directory(full)) {
                    base_path = full;
                    prefix = partial;
                } else {
                    base_path = full.parent_path();
                    prefix = partial_path.filename().string();
                }
            }
        }

        if (!std::filesystem::exists(base_path)) {
            return completions;
        }

        for (const auto& entry : std::filesystem::directory_iterator(base_path)) {
            std::string name = entry.path().filename().string();

            if (prefix.empty() || name.compare(0, prefix.size(), prefix) == 0) {
                std::string completion = entry.path().string();

                // Add trailing separator for directories
                if (entry.is_directory()) {
                    completion += std::filesystem::path::preferred_separator;
                }

                completions.push_back(completion);
            }
        }

        std::sort(completions.begin(), completions.end());

    } catch (const std::exception&) {
        // Ignore filesystem errors during completion
    }

    return completions;
}

} // namespace void_shell
