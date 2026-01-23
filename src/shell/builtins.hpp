#pragma once

/// @file builtins.hpp
/// @brief Built-in shell commands

#include "command.hpp"
#include "session.hpp"

namespace void_shell {
namespace builtins {

// =============================================================================
// Command Registration
// =============================================================================

/// @brief Register all built-in commands
void register_all(CommandRegistry& registry);

/// @brief Register general commands
void register_general_commands(CommandRegistry& registry);

/// @brief Register filesystem commands
void register_filesystem_commands(CommandRegistry& registry);

/// @brief Register variable commands
void register_variable_commands(CommandRegistry& registry);

/// @brief Register scripting commands
void register_scripting_commands(CommandRegistry& registry);

/// @brief Register debug commands
void register_debug_commands(CommandRegistry& registry);

/// @brief Register engine commands
void register_engine_commands(CommandRegistry& registry);

/// @brief Register ECS commands
void register_ecs_commands(CommandRegistry& registry);

/// @brief Register asset commands
void register_asset_commands(CommandRegistry& registry);

/// @brief Register profile commands
void register_profile_commands(CommandRegistry& registry);

/// @brief Register help commands
void register_help_commands(CommandRegistry& registry);

// =============================================================================
// General Commands
// =============================================================================

/// @brief echo - Print text
CommandResult cmd_echo(const CommandArgs& args, CommandContext& ctx);

/// @brief clear - Clear screen
CommandResult cmd_clear(const CommandArgs& args, CommandContext& ctx);

/// @brief exit - Exit shell
CommandResult cmd_exit(const CommandArgs& args, CommandContext& ctx);

/// @brief sleep - Sleep for duration
CommandResult cmd_sleep(const CommandArgs& args, CommandContext& ctx);

/// @brief time - Time a command
CommandResult cmd_time(const CommandArgs& args, CommandContext& ctx);

/// @brief alias - Define alias
CommandResult cmd_alias(const CommandArgs& args, CommandContext& ctx);

/// @brief unalias - Remove alias
CommandResult cmd_unalias(const CommandArgs& args, CommandContext& ctx);

/// @brief history - Show history
CommandResult cmd_history(const CommandArgs& args, CommandContext& ctx);

/// @brief jobs - List background jobs
CommandResult cmd_jobs(const CommandArgs& args, CommandContext& ctx);

/// @brief kill - Kill background job
CommandResult cmd_kill(const CommandArgs& args, CommandContext& ctx);

/// @brief wait - Wait for job
CommandResult cmd_wait(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// Filesystem Commands
// =============================================================================

/// @brief pwd - Print working directory
CommandResult cmd_pwd(const CommandArgs& args, CommandContext& ctx);

/// @brief cd - Change directory
CommandResult cmd_cd(const CommandArgs& args, CommandContext& ctx);

/// @brief ls - List directory
CommandResult cmd_ls(const CommandArgs& args, CommandContext& ctx);

/// @brief cat - Print file contents
CommandResult cmd_cat(const CommandArgs& args, CommandContext& ctx);

/// @brief head - Print first lines
CommandResult cmd_head(const CommandArgs& args, CommandContext& ctx);

/// @brief tail - Print last lines
CommandResult cmd_tail(const CommandArgs& args, CommandContext& ctx);

/// @brief find - Find files
CommandResult cmd_find(const CommandArgs& args, CommandContext& ctx);

/// @brief grep - Search in files
CommandResult cmd_grep(const CommandArgs& args, CommandContext& ctx);

/// @brief mkdir - Create directory
CommandResult cmd_mkdir(const CommandArgs& args, CommandContext& ctx);

/// @brief rm - Remove file/directory
CommandResult cmd_rm(const CommandArgs& args, CommandContext& ctx);

/// @brief cp - Copy file/directory
CommandResult cmd_cp(const CommandArgs& args, CommandContext& ctx);

/// @brief mv - Move file/directory
CommandResult cmd_mv(const CommandArgs& args, CommandContext& ctx);

/// @brief touch - Create/update file
CommandResult cmd_touch(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// Variable Commands
// =============================================================================

/// @brief set - Set variable
CommandResult cmd_set(const CommandArgs& args, CommandContext& ctx);

/// @brief get - Get variable
CommandResult cmd_get(const CommandArgs& args, CommandContext& ctx);

/// @brief unset - Unset variable
CommandResult cmd_unset(const CommandArgs& args, CommandContext& ctx);

/// @brief env - List/set environment
CommandResult cmd_env(const CommandArgs& args, CommandContext& ctx);

/// @brief export - Export variable
CommandResult cmd_export(const CommandArgs& args, CommandContext& ctx);

/// @brief expr - Evaluate expression
CommandResult cmd_expr(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// Scripting Commands
// =============================================================================

/// @brief source - Execute script file
CommandResult cmd_source(const CommandArgs& args, CommandContext& ctx);

/// @brief eval - Evaluate string as command
CommandResult cmd_eval(const CommandArgs& args, CommandContext& ctx);

/// @brief script - Run VoidScript
CommandResult cmd_script(const CommandArgs& args, CommandContext& ctx);

/// @brief wasm - Run WASM module
CommandResult cmd_wasm(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// Debug Commands
// =============================================================================

/// @brief log - Set log level
CommandResult cmd_log(const CommandArgs& args, CommandContext& ctx);

/// @brief trace - Stack trace
CommandResult cmd_trace(const CommandArgs& args, CommandContext& ctx);

/// @brief breakpoint - Set breakpoint
CommandResult cmd_breakpoint(const CommandArgs& args, CommandContext& ctx);

/// @brief watch - Watch variable
CommandResult cmd_watch(const CommandArgs& args, CommandContext& ctx);

/// @brief dump - Dump memory/state
CommandResult cmd_dump(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// Engine Commands
// =============================================================================

/// @brief engine - Engine control
CommandResult cmd_engine(const CommandArgs& args, CommandContext& ctx);

/// @brief reload - Hot reload
CommandResult cmd_reload(const CommandArgs& args, CommandContext& ctx);

/// @brief config - Configuration
CommandResult cmd_config(const CommandArgs& args, CommandContext& ctx);

/// @brief stats - Engine statistics
CommandResult cmd_stats(const CommandArgs& args, CommandContext& ctx);

/// @brief pause - Pause engine
CommandResult cmd_pause(const CommandArgs& args, CommandContext& ctx);

/// @brief resume - Resume engine
CommandResult cmd_resume(const CommandArgs& args, CommandContext& ctx);

/// @brief step - Step frame
CommandResult cmd_step(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// ECS Commands
// =============================================================================

/// @brief entity - Entity management
CommandResult cmd_entity(const CommandArgs& args, CommandContext& ctx);

/// @brief component - Component management
CommandResult cmd_component(const CommandArgs& args, CommandContext& ctx);

/// @brief query - ECS query
CommandResult cmd_query(const CommandArgs& args, CommandContext& ctx);

/// @brief spawn - Spawn entity
CommandResult cmd_spawn(const CommandArgs& args, CommandContext& ctx);

/// @brief destroy - Destroy entity
CommandResult cmd_destroy(const CommandArgs& args, CommandContext& ctx);

/// @brief inspect - Inspect entity/component
CommandResult cmd_inspect(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// Asset Commands
// =============================================================================

/// @brief asset - Asset management
CommandResult cmd_asset(const CommandArgs& args, CommandContext& ctx);

/// @brief load - Load asset
CommandResult cmd_load(const CommandArgs& args, CommandContext& ctx);

/// @brief unload - Unload asset
CommandResult cmd_unload(const CommandArgs& args, CommandContext& ctx);

/// @brief import - Import asset
CommandResult cmd_import(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// Profile Commands
// =============================================================================

/// @brief profile - Profiling control
CommandResult cmd_profile(const CommandArgs& args, CommandContext& ctx);

/// @brief perf - Performance metrics
CommandResult cmd_perf(const CommandArgs& args, CommandContext& ctx);

/// @brief memory - Memory usage
CommandResult cmd_memory(const CommandArgs& args, CommandContext& ctx);

/// @brief gpu - GPU info
CommandResult cmd_gpu(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// Help Commands
// =============================================================================

/// @brief help - Show help
CommandResult cmd_help(const CommandArgs& args, CommandContext& ctx);

/// @brief man - Manual page
CommandResult cmd_man(const CommandArgs& args, CommandContext& ctx);

/// @brief commands - List commands
CommandResult cmd_commands(const CommandArgs& args, CommandContext& ctx);

/// @brief version - Show version
CommandResult cmd_version(const CommandArgs& args, CommandContext& ctx);

// =============================================================================
// Network Commands
// =============================================================================

/// @brief connect - Connect to remote
CommandResult cmd_connect(const CommandArgs& args, CommandContext& ctx);

/// @brief disconnect - Disconnect from remote
CommandResult cmd_disconnect(const CommandArgs& args, CommandContext& ctx);

/// @brief remote - Remote server control
CommandResult cmd_remote(const CommandArgs& args, CommandContext& ctx);

/// @brief sessions - List sessions
CommandResult cmd_sessions(const CommandArgs& args, CommandContext& ctx);

} // namespace builtins
} // namespace void_shell
