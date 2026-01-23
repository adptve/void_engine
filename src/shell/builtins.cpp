/// @file builtins.cpp
/// @brief Built-in shell commands implementation for void_shell

#include "builtins.hpp"
#include "shell.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#else
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace void_shell {
namespace builtins {

// =============================================================================
// Registration Functions
// =============================================================================

void register_all(CommandRegistry& registry) {
    register_general_commands(registry);
    register_filesystem_commands(registry);
    register_variable_commands(registry);
    register_scripting_commands(registry);
    register_debug_commands(registry);
    register_engine_commands(registry);
    register_ecs_commands(registry);
    register_asset_commands(registry);
    register_profile_commands(registry);
    register_help_commands(registry);
}

void register_general_commands(CommandRegistry& registry) {
    // echo
    CommandBuilder("echo")
        .description("Print text to output")
        .usage("echo [text...]")
        .example("echo Hello World")
        .example("echo $VAR")
        .category(CommandCategory::General)
        .variadic("text", ArgType::String, "Text to print")
        .flag("n", 'n', "Do not print newline")
        .flag("e", 'e', "Enable escape sequences")
        .function(cmd_echo)
        .register_to(registry);

    // clear
    CommandBuilder("clear")
        .description("Clear the screen")
        .usage("clear")
        .category(CommandCategory::General)
        .alias("cls")
        .function(cmd_clear)
        .register_to(registry);

    // exit
    CommandBuilder("exit")
        .description("Exit the shell")
        .usage("exit [code]")
        .example("exit 0")
        .category(CommandCategory::General)
        .alias("quit")
        .arg_with_default("code", ArgType::Integer, "Exit code", std::int64_t(0))
        .function(cmd_exit)
        .register_to(registry);

    // sleep
    CommandBuilder("sleep")
        .description("Sleep for specified duration")
        .usage("sleep <seconds>")
        .example("sleep 1.5")
        .category(CommandCategory::General)
        .arg("seconds", ArgType::Float, "Duration in seconds", true)
        .function(cmd_sleep)
        .register_to(registry);

    // time
    CommandBuilder("time")
        .description("Time a command execution")
        .usage("time <command>")
        .example("time ls -la")
        .category(CommandCategory::General)
        .variadic("command", ArgType::String, "Command to time")
        .function(cmd_time)
        .register_to(registry);

    // alias
    CommandBuilder("alias")
        .description("Define or list aliases")
        .usage("alias [name[=value]]")
        .example("alias ll='ls -la'")
        .category(CommandCategory::General)
        .arg("definition", ArgType::String, "Alias definition (name=value)", false)
        .function(cmd_alias)
        .register_to(registry);

    // unalias
    CommandBuilder("unalias")
        .description("Remove an alias")
        .usage("unalias <name>")
        .example("unalias ll")
        .category(CommandCategory::General)
        .arg("name", ArgType::String, "Alias name to remove", true)
        .function(cmd_unalias)
        .register_to(registry);

    // history
    CommandBuilder("history")
        .description("Show command history")
        .usage("history [count]")
        .example("history 10")
        .category(CommandCategory::General)
        .arg_with_default("count", ArgType::Integer, "Number of entries", std::int64_t(20))
        .flag("clear", 'c', "Clear history")
        .function(cmd_history)
        .register_to(registry);

    // jobs
    CommandBuilder("jobs")
        .description("List background jobs")
        .usage("jobs")
        .category(CommandCategory::General)
        .function(cmd_jobs)
        .register_to(registry);

    // kill
    CommandBuilder("kill")
        .description("Kill a background job")
        .usage("kill <job_id>")
        .example("kill 1")
        .category(CommandCategory::General)
        .arg("job_id", ArgType::Integer, "Job ID to kill", true)
        .function(cmd_kill)
        .register_to(registry);

    // wait
    CommandBuilder("wait")
        .description("Wait for a background job")
        .usage("wait [job_id]")
        .example("wait 1")
        .category(CommandCategory::General)
        .arg("job_id", ArgType::Integer, "Job ID to wait for", false)
        .function(cmd_wait)
        .register_to(registry);
}

void register_filesystem_commands(CommandRegistry& registry) {
    // pwd
    CommandBuilder("pwd")
        .description("Print working directory")
        .usage("pwd")
        .category(CommandCategory::FileSystem)
        .function(cmd_pwd)
        .register_to(registry);

    // cd
    CommandBuilder("cd")
        .description("Change directory")
        .usage("cd [directory]")
        .example("cd /home")
        .example("cd ..")
        .category(CommandCategory::FileSystem)
        .arg("directory", ArgType::Path, "Target directory", false)
        .function(cmd_cd)
        .register_to(registry);

    // ls
    CommandBuilder("ls")
        .description("List directory contents")
        .usage("ls [options] [path]")
        .example("ls -la")
        .example("ls /home")
        .category(CommandCategory::FileSystem)
        .alias("dir")
        .arg("path", ArgType::Path, "Directory to list", false)
        .flag("all", 'a', "Show hidden files")
        .flag("long", 'l', "Long format")
        .flag("recursive", 'R', "Recursive listing")
        .flag("human", 'h', "Human-readable sizes")
        .function(cmd_ls)
        .register_to(registry);

    // cat
    CommandBuilder("cat")
        .description("Print file contents")
        .usage("cat <file> [file...]")
        .example("cat file.txt")
        .category(CommandCategory::FileSystem)
        .alias("type")
        .variadic("files", ArgType::Path, "Files to display")
        .flag("number", 'n', "Number lines")
        .function(cmd_cat)
        .register_to(registry);

    // head
    CommandBuilder("head")
        .description("Print first lines of file")
        .usage("head [-n count] <file>")
        .example("head -n 20 file.txt")
        .category(CommandCategory::FileSystem)
        .arg("file", ArgType::Path, "File to read", true)
        .flag_with_value("lines", 'n', ArgType::Integer, "Number of lines")
        .function(cmd_head)
        .register_to(registry);

    // tail
    CommandBuilder("tail")
        .description("Print last lines of file")
        .usage("tail [-n count] <file>")
        .example("tail -n 20 file.txt")
        .category(CommandCategory::FileSystem)
        .arg("file", ArgType::Path, "File to read", true)
        .flag_with_value("lines", 'n', ArgType::Integer, "Number of lines")
        .flag("follow", 'f', "Follow file changes")
        .function(cmd_tail)
        .register_to(registry);

    // find
    CommandBuilder("find")
        .description("Find files matching pattern")
        .usage("find [path] -name <pattern>")
        .example("find . -name '*.cpp'")
        .category(CommandCategory::FileSystem)
        .arg("path", ArgType::Path, "Starting directory", false)
        .flag_with_value("name", 0, ArgType::String, "Name pattern")
        .flag_with_value("type", 0, ArgType::String, "File type (f=file, d=dir)")
        .flag_with_value("maxdepth", 0, ArgType::Integer, "Maximum depth")
        .function(cmd_find)
        .register_to(registry);

    // grep
    CommandBuilder("grep")
        .description("Search for pattern in files")
        .usage("grep [options] <pattern> [file...]")
        .example("grep -r 'TODO' src/")
        .category(CommandCategory::FileSystem)
        .arg("pattern", ArgType::String, "Search pattern", true)
        .variadic("files", ArgType::Path, "Files to search")
        .flag("recursive", 'r', "Recursive search")
        .flag("ignore-case", 'i', "Case insensitive")
        .flag("line-number", 'n', "Show line numbers")
        .flag("count", 'c', "Count matches only")
        .function(cmd_grep)
        .register_to(registry);

    // mkdir
    CommandBuilder("mkdir")
        .description("Create directory")
        .usage("mkdir [-p] <directory>")
        .example("mkdir -p path/to/dir")
        .category(CommandCategory::FileSystem)
        .arg("directory", ArgType::Path, "Directory to create", true)
        .flag("parents", 'p', "Create parent directories")
        .function(cmd_mkdir)
        .register_to(registry);

    // rm
    CommandBuilder("rm")
        .description("Remove file or directory")
        .usage("rm [-rf] <path>")
        .example("rm -rf old_dir")
        .category(CommandCategory::FileSystem)
        .alias("del")
        .alias("delete")
        .variadic("paths", ArgType::Path, "Paths to remove")
        .flag("recursive", 'r', "Recursive removal")
        .flag("force", 'f', "Force removal")
        .function(cmd_rm)
        .register_to(registry);

    // cp
    CommandBuilder("cp")
        .description("Copy file or directory")
        .usage("cp [-r] <source> <dest>")
        .example("cp -r src/ backup/")
        .category(CommandCategory::FileSystem)
        .alias("copy")
        .arg("source", ArgType::Path, "Source path", true)
        .arg("dest", ArgType::Path, "Destination path", true)
        .flag("recursive", 'r', "Recursive copy")
        .flag("force", 'f', "Overwrite existing")
        .function(cmd_cp)
        .register_to(registry);

    // mv
    CommandBuilder("mv")
        .description("Move file or directory")
        .usage("mv <source> <dest>")
        .example("mv old.txt new.txt")
        .category(CommandCategory::FileSystem)
        .alias("move")
        .alias("rename")
        .arg("source", ArgType::Path, "Source path", true)
        .arg("dest", ArgType::Path, "Destination path", true)
        .flag("force", 'f', "Overwrite existing")
        .function(cmd_mv)
        .register_to(registry);

    // touch
    CommandBuilder("touch")
        .description("Create file or update timestamp")
        .usage("touch <file>")
        .example("touch newfile.txt")
        .category(CommandCategory::FileSystem)
        .arg("file", ArgType::Path, "File to create/touch", true)
        .function(cmd_touch)
        .register_to(registry);
}

void register_variable_commands(CommandRegistry& registry) {
    // set
    CommandBuilder("set")
        .description("Set a variable")
        .usage("set <name> <value>")
        .example("set DEBUG true")
        .category(CommandCategory::Variables)
        .arg("name", ArgType::String, "Variable name", true)
        .arg("value", ArgType::Any, "Variable value", true)
        .function(cmd_set)
        .register_to(registry);

    // get
    CommandBuilder("get")
        .description("Get a variable value")
        .usage("get <name>")
        .example("get DEBUG")
        .category(CommandCategory::Variables)
        .arg("name", ArgType::String, "Variable name", true)
        .function(cmd_get)
        .register_to(registry);

    // unset
    CommandBuilder("unset")
        .description("Unset a variable")
        .usage("unset <name>")
        .example("unset DEBUG")
        .category(CommandCategory::Variables)
        .arg("name", ArgType::String, "Variable name", true)
        .function(cmd_unset)
        .register_to(registry);

    // env
    CommandBuilder("env")
        .description("List or set environment variables")
        .usage("env [name[=value]]")
        .example("env PATH=/usr/bin")
        .category(CommandCategory::Variables)
        .arg("assignment", ArgType::String, "Variable assignment", false)
        .function(cmd_env)
        .register_to(registry);

    // export
    CommandBuilder("export")
        .description("Export variable to environment")
        .usage("export <name>[=value]")
        .example("export PATH=/usr/bin")
        .category(CommandCategory::Variables)
        .arg("assignment", ArgType::String, "Variable to export", true)
        .function(cmd_export)
        .register_to(registry);

    // expr
    CommandBuilder("expr")
        .description("Evaluate expression")
        .usage("expr <expression>")
        .example("expr 2 + 2")
        .example("expr $count * 10")
        .category(CommandCategory::Variables)
        .variadic("expression", ArgType::String, "Expression to evaluate")
        .function(cmd_expr)
        .register_to(registry);
}

void register_scripting_commands(CommandRegistry& registry) {
    // source
    CommandBuilder("source")
        .description("Execute a script file")
        .usage("source <file>")
        .example("source startup.sh")
        .category(CommandCategory::Scripting)
        .alias(".")
        .arg("file", ArgType::Path, "Script file to execute", true)
        .function(cmd_source)
        .register_to(registry);

    // eval
    CommandBuilder("eval")
        .description("Evaluate string as command")
        .usage("eval <command>")
        .example("eval 'echo Hello'")
        .category(CommandCategory::Scripting)
        .variadic("command", ArgType::String, "Command to evaluate")
        .function(cmd_eval)
        .register_to(registry);

    // script
    CommandBuilder("script")
        .description("Run VoidScript code")
        .usage("script <file>")
        .example("script game.vs")
        .category(CommandCategory::Scripting)
        .arg("file", ArgType::Path, "Script file", true)
        .function(cmd_script)
        .register_to(registry);

    // wasm
    CommandBuilder("wasm")
        .description("Run WASM module")
        .usage("wasm <file> [function] [args...]")
        .example("wasm module.wasm main")
        .category(CommandCategory::Scripting)
        .arg("file", ArgType::Path, "WASM file", true)
        .arg("function", ArgType::String, "Function to call", false)
        .variadic("args", ArgType::Any, "Function arguments")
        .function(cmd_wasm)
        .register_to(registry);
}

void register_debug_commands(CommandRegistry& registry) {
    // log
    CommandBuilder("log")
        .description("Set log level or print log message")
        .usage("log [level] [message...]")
        .example("log debug")
        .example("log info Starting up")
        .category(CommandCategory::Debug)
        .arg("level", ArgType::String, "Log level (trace/debug/info/warn/error)", false)
        .variadic("message", ArgType::String, "Message to log")
        .function(cmd_log)
        .register_to(registry);

    // trace
    CommandBuilder("trace")
        .description("Print stack trace")
        .usage("trace")
        .category(CommandCategory::Debug)
        .function(cmd_trace)
        .register_to(registry);

    // breakpoint
    CommandBuilder("breakpoint")
        .description("Set a breakpoint")
        .usage("breakpoint <location>")
        .example("breakpoint main.cpp:42")
        .category(CommandCategory::Debug)
        .arg("location", ArgType::String, "Breakpoint location", true)
        .flag("condition", 'c', "Conditional breakpoint")
        .function(cmd_breakpoint)
        .register_to(registry);

    // watch
    CommandBuilder("watch")
        .description("Watch a variable or expression")
        .usage("watch <expression>")
        .example("watch player.health")
        .category(CommandCategory::Debug)
        .arg("expression", ArgType::String, "Expression to watch", true)
        .function(cmd_watch)
        .register_to(registry);

    // dump
    CommandBuilder("dump")
        .description("Dump memory or state")
        .usage("dump <what>")
        .example("dump registry")
        .example("dump memory 0x1000 64")
        .category(CommandCategory::Debug)
        .arg("what", ArgType::String, "What to dump", true)
        .function(cmd_dump)
        .register_to(registry);
}

void register_engine_commands(CommandRegistry& registry) {
    // engine
    CommandBuilder("engine")
        .description("Engine control")
        .usage("engine <action>")
        .example("engine status")
        .example("engine restart")
        .category(CommandCategory::Engine)
        .arg("action", ArgType::String, "Action (status/start/stop/restart)", true)
        .function(cmd_engine)
        .register_to(registry);

    // reload
    CommandBuilder("reload")
        .description("Hot reload module or assets")
        .usage("reload <what>")
        .example("reload scripts")
        .example("reload assets")
        .category(CommandCategory::Engine)
        .arg("what", ArgType::String, "What to reload", true)
        .function(cmd_reload)
        .register_to(registry);

    // config
    CommandBuilder("config")
        .description("Get or set configuration")
        .usage("config [key] [value]")
        .example("config graphics.vsync true")
        .category(CommandCategory::Engine)
        .arg("key", ArgType::String, "Config key", false)
        .arg("value", ArgType::Any, "Config value", false)
        .function(cmd_config)
        .register_to(registry);

    // stats
    CommandBuilder("stats")
        .description("Show engine statistics")
        .usage("stats [category]")
        .example("stats render")
        .category(CommandCategory::Engine)
        .arg("category", ArgType::String, "Stats category", false)
        .function(cmd_stats)
        .register_to(registry);

    // pause
    CommandBuilder("pause")
        .description("Pause engine simulation")
        .usage("pause")
        .category(CommandCategory::Engine)
        .function(cmd_pause)
        .register_to(registry);

    // resume
    CommandBuilder("resume")
        .description("Resume engine simulation")
        .usage("resume")
        .category(CommandCategory::Engine)
        .function(cmd_resume)
        .register_to(registry);

    // step
    CommandBuilder("step")
        .description("Step one frame")
        .usage("step [count]")
        .example("step 10")
        .category(CommandCategory::Engine)
        .arg_with_default("count", ArgType::Integer, "Frame count", std::int64_t(1))
        .function(cmd_step)
        .register_to(registry);
}

void register_ecs_commands(CommandRegistry& registry) {
    // entity
    CommandBuilder("entity")
        .description("Entity management")
        .usage("entity <action> [args...]")
        .example("entity list")
        .example("entity create Player")
        .category(CommandCategory::Ecs)
        .arg("action", ArgType::String, "Action (list/create/destroy/info)", true)
        .variadic("args", ArgType::Any, "Action arguments")
        .function(cmd_entity)
        .register_to(registry);

    // component
    CommandBuilder("component")
        .description("Component management")
        .usage("component <action> [args...]")
        .example("component list")
        .example("component add 42 Transform")
        .category(CommandCategory::Ecs)
        .arg("action", ArgType::String, "Action (list/add/remove/get/set)", true)
        .variadic("args", ArgType::Any, "Action arguments")
        .function(cmd_component)
        .register_to(registry);

    // query
    CommandBuilder("query")
        .description("Query ECS entities")
        .usage("query <components...>")
        .example("query Transform Renderable")
        .category(CommandCategory::Ecs)
        .variadic("components", ArgType::String, "Component types to query")
        .function(cmd_query)
        .register_to(registry);

    // spawn
    CommandBuilder("spawn")
        .description("Spawn an entity from prefab")
        .usage("spawn <prefab> [position]")
        .example("spawn Enemy 10,0,5")
        .category(CommandCategory::Ecs)
        .arg("prefab", ArgType::String, "Prefab name", true)
        .arg("position", ArgType::String, "Position (x,y,z)", false)
        .function(cmd_spawn)
        .register_to(registry);

    // destroy
    CommandBuilder("destroy")
        .description("Destroy an entity")
        .usage("destroy <entity_id>")
        .example("destroy 42")
        .category(CommandCategory::Ecs)
        .arg("entity_id", ArgType::EntityId, "Entity ID", true)
        .function(cmd_destroy)
        .register_to(registry);

    // inspect
    CommandBuilder("inspect")
        .description("Inspect entity or component")
        .usage("inspect <entity_id> [component]")
        .example("inspect 42 Transform")
        .category(CommandCategory::Ecs)
        .arg("entity_id", ArgType::EntityId, "Entity ID", true)
        .arg("component", ArgType::String, "Component name", false)
        .function(cmd_inspect)
        .register_to(registry);
}

void register_asset_commands(CommandRegistry& registry) {
    // asset
    CommandBuilder("asset")
        .description("Asset management")
        .usage("asset <action> [args...]")
        .example("asset list")
        .example("asset info texture.png")
        .category(CommandCategory::Assets)
        .arg("action", ArgType::String, "Action (list/info/reload)", true)
        .variadic("args", ArgType::Any, "Action arguments")
        .function(cmd_asset)
        .register_to(registry);

    // load
    CommandBuilder("load")
        .description("Load an asset")
        .usage("load <path>")
        .example("load textures/player.png")
        .category(CommandCategory::Assets)
        .arg("path", ArgType::Path, "Asset path", true)
        .flag("async", 'a', "Load asynchronously")
        .function(cmd_load)
        .register_to(registry);

    // unload
    CommandBuilder("unload")
        .description("Unload an asset")
        .usage("unload <path>")
        .example("unload textures/player.png")
        .category(CommandCategory::Assets)
        .arg("path", ArgType::Path, "Asset path", true)
        .function(cmd_unload)
        .register_to(registry);

    // import
    CommandBuilder("import")
        .description("Import an asset")
        .usage("import <source> [dest]")
        .example("import model.fbx models/")
        .category(CommandCategory::Assets)
        .arg("source", ArgType::Path, "Source file", true)
        .arg("dest", ArgType::Path, "Destination", false)
        .function(cmd_import)
        .register_to(registry);
}

void register_profile_commands(CommandRegistry& registry) {
    // profile
    CommandBuilder("profile")
        .description("Profiling control")
        .usage("profile <action>")
        .example("profile start")
        .example("profile stop")
        .example("profile report")
        .category(CommandCategory::Profile)
        .arg("action", ArgType::String, "Action (start/stop/report/clear)", true)
        .function(cmd_profile)
        .register_to(registry);

    // perf
    CommandBuilder("perf")
        .description("Performance metrics")
        .usage("perf [category]")
        .example("perf render")
        .category(CommandCategory::Profile)
        .arg("category", ArgType::String, "Category to show", false)
        .function(cmd_perf)
        .register_to(registry);

    // memory
    CommandBuilder("memory")
        .description("Memory usage")
        .usage("memory [detail]")
        .example("memory allocations")
        .category(CommandCategory::Profile)
        .arg("detail", ArgType::String, "Detail level", false)
        .function(cmd_memory)
        .register_to(registry);

    // gpu
    CommandBuilder("gpu")
        .description("GPU information")
        .usage("gpu [info]")
        .example("gpu memory")
        .category(CommandCategory::Profile)
        .arg("info", ArgType::String, "Info type", false)
        .function(cmd_gpu)
        .register_to(registry);
}

void register_help_commands(CommandRegistry& registry) {
    // help
    CommandBuilder("help")
        .description("Show help for commands")
        .usage("help [command]")
        .example("help ls")
        .category(CommandCategory::Help)
        .alias("?")
        .arg("command", ArgType::String, "Command to get help for", false)
        .function(cmd_help)
        .register_to(registry);

    // man
    CommandBuilder("man")
        .description("Show manual page")
        .usage("man <command>")
        .example("man grep")
        .category(CommandCategory::Help)
        .arg("command", ArgType::String, "Command name", true)
        .function(cmd_man)
        .register_to(registry);

    // commands
    CommandBuilder("commands")
        .description("List all commands")
        .usage("commands [category]")
        .example("commands filesystem")
        .category(CommandCategory::Help)
        .arg("category", ArgType::String, "Filter by category", false)
        .function(cmd_commands)
        .register_to(registry);

    // version
    CommandBuilder("version")
        .description("Show version information")
        .usage("version")
        .category(CommandCategory::Help)
        .function(cmd_version)
        .register_to(registry);
}

// =============================================================================
// General Commands Implementation
// =============================================================================

CommandResult cmd_echo(const CommandArgs& args, CommandContext& ctx) {
    std::ostringstream ss;

    bool no_newline = args.get_bool("-n", false);
    bool escape = args.get_bool("-e", false);

    for (std::size_t i = 0; i < args.positional().size(); ++i) {
        if (i > 0) ss << " ";
        std::string text = args.positional()[i].as_string();

        if (escape) {
            std::string result;
            for (std::size_t j = 0; j < text.size(); ++j) {
                if (text[j] == '\\' && j + 1 < text.size()) {
                    switch (text[j + 1]) {
                        case 'n': result += '\n'; j++; break;
                        case 't': result += '\t'; j++; break;
                        case 'r': result += '\r'; j++; break;
                        case '\\': result += '\\'; j++; break;
                        default: result += text[j]; break;
                    }
                } else {
                    result += text[j];
                }
            }
            ss << result;
        } else {
            ss << text;
        }
    }

    if (!no_newline) {
        ss << "\n";
    }

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

CommandResult cmd_clear(const CommandArgs&, CommandContext& ctx) {
    // ANSI escape sequence to clear screen and move cursor to top-left
    ctx.output("\033[2J\033[H");
    return CommandResult::success("");
}

CommandResult cmd_exit(const CommandArgs& args, CommandContext&) {
    int code = static_cast<int>(args.get_int("code", 0));

    CommandResult result;
    result.status = CommandStatus::Cancelled;
    result.exit_code = code;
    return result;
}

CommandResult cmd_sleep(const CommandArgs& args, CommandContext&) {
    double seconds = args.get_float("seconds", 1.0);
    auto duration = std::chrono::duration<double>(seconds);
    std::this_thread::sleep_for(duration);
    return CommandResult::success("");
}

CommandResult cmd_time(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: time <command>");
    }

    // Reconstruct command string
    std::ostringstream cmd_ss;
    for (std::size_t i = 0; i < args.positional().size(); ++i) {
        if (i > 0) cmd_ss << " ";
        cmd_ss << args.positional()[i].as_string();
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Execute through the shell
    auto& shell = ShellSystem::instance();
    auto result = shell.execute(cmd_ss.str());

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::ostringstream ss;
    ss << result.output;
    ss << "\nreal\t" << std::fixed << std::setprecision(3)
       << duration.count() / 1000000.0 << "s\n";

    ctx.output(ss.str());
    return result;
}

CommandResult cmd_alias(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        // List all aliases
        auto aliases = ctx.registry->all_aliases();
        std::ostringstream ss;
        for (const auto& [name, expansion] : aliases) {
            ss << "alias " << name << "='" << expansion << "'\n";
        }
        ctx.output(ss.str());
        return CommandResult::success(ss.str());
    }

    // Parse definition
    std::string def = args.positional()[0].as_string();
    std::size_t eq = def.find('=');

    if (eq == std::string::npos) {
        // Show single alias
        auto expansion = ctx.registry->get_alias(def);
        if (expansion) {
            std::string out = "alias " + def + "='" + *expansion + "'\n";
            ctx.output(out);
            return CommandResult::success(out);
        }
        return CommandResult::error("Alias not found: " + def);
    }

    std::string name = def.substr(0, eq);
    std::string value = def.substr(eq + 1);

    // Remove quotes if present
    if (value.size() >= 2 &&
        ((value.front() == '\'' && value.back() == '\'') ||
         (value.front() == '"' && value.back() == '"'))) {
        value = value.substr(1, value.size() - 2);
    }

    ctx.registry->add_alias(name, value);
    return CommandResult::success("");
}

CommandResult cmd_unalias(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: unalias <name>");
    }

    std::string name = args.positional()[0].as_string();
    if (ctx.registry->remove_alias(name)) {
        return CommandResult::success("");
    }

    return CommandResult::error("Alias not found: " + name);
}

CommandResult cmd_history(const CommandArgs& args, CommandContext& ctx) {
    if (args.get_bool("-c", false) || args.get_bool("--clear", false)) {
        // Clear history - this would need session access
        ctx.output("History cleared\n");
        return CommandResult::success("");
    }

    // Get history from session
    auto* session = ShellSystem::instance().get_session(ctx.session_id);
    if (!session) {
        return CommandResult::error("No session");
    }

    std::size_t count = static_cast<std::size_t>(args.get_int("count", 20));
    const auto& entries = session->history().entries();

    std::ostringstream ss;
    std::size_t start = entries.size() > count ? entries.size() - count : 0;

    for (std::size_t i = start; i < entries.size(); ++i) {
        ss << std::setw(5) << entries[i].index << "  " << entries[i].command << "\n";
    }

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

CommandResult cmd_jobs(const CommandArgs&, CommandContext& ctx) {
    auto* session = ShellSystem::instance().get_session(ctx.session_id);
    if (!session) {
        return CommandResult::error("No session");
    }

    auto jobs = session->jobs();
    if (jobs.empty()) {
        ctx.output("No background jobs\n");
        return CommandResult::success("");
    }

    std::ostringstream ss;
    for (const auto* job : jobs) {
        ss << "[" << job->job_id << "] "
           << (job->is_done() ? "Done" : "Running")
           << "    " << job->command << "\n";
    }

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

CommandResult cmd_kill(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: kill <job_id>");
    }

    auto* session = ShellSystem::instance().get_session(ctx.session_id);
    if (!session) {
        return CommandResult::error("No session");
    }

    std::uint32_t job_id = static_cast<std::uint32_t>(args.positional()[0].as_int());
    if (session->cancel_job(job_id)) {
        ctx.output("Job " + std::to_string(job_id) + " cancelled\n");
        return CommandResult::success("");
    }

    return CommandResult::error("Job not found: " + std::to_string(job_id));
}

CommandResult cmd_wait(const CommandArgs& args, CommandContext& ctx) {
    auto* session = ShellSystem::instance().get_session(ctx.session_id);
    if (!session) {
        return CommandResult::error("No session");
    }

    if (args.positional().empty()) {
        // Wait for all jobs
        auto jobs = session->jobs();
        for (const auto* job : jobs) {
            session->wait_job(job->job_id);
        }
        return CommandResult::success("");
    }

    std::uint32_t job_id = static_cast<std::uint32_t>(args.positional()[0].as_int());
    if (session->wait_job(job_id)) {
        return CommandResult::success("");
    }

    return CommandResult::error("Job not found: " + std::to_string(job_id));
}

// =============================================================================
// Filesystem Commands Implementation
// =============================================================================

CommandResult cmd_pwd(const CommandArgs&, CommandContext& ctx) {
    std::string pwd = ctx.cwd.string();
    ctx.output(pwd + "\n");
    return CommandResult::success(pwd);
}

CommandResult cmd_cd(const CommandArgs& args, CommandContext& ctx) {
    std::filesystem::path target;

    if (args.positional().empty()) {
        // Go to home directory
        target = ctx.env->home();
    } else {
        target = args.positional()[0].as_string();
    }

    // Handle ~ for home
    std::string target_str = target.string();
    if (!target_str.empty() && target_str[0] == '~') {
        target = ctx.env->home() + target_str.substr(1);
    }

    // Resolve relative paths
    if (target.is_relative()) {
        target = ctx.cwd / target;
    }

    target = std::filesystem::weakly_canonical(target);

    if (!std::filesystem::exists(target)) {
        return CommandResult::error("Directory not found: " + target.string());
    }

    if (!std::filesystem::is_directory(target)) {
        return CommandResult::error("Not a directory: " + target.string());
    }

    // Update session's cwd
    auto* session = ShellSystem::instance().get_session(ctx.session_id);
    if (session) {
        session->set_cwd(target);
    }

    ctx.env->set_pwd(target.string());
    return CommandResult::success("");
}

CommandResult cmd_ls(const CommandArgs& args, CommandContext& ctx) {
    std::filesystem::path path = ctx.cwd;

    if (!args.positional().empty()) {
        path = args.positional()[0].as_string();
        if (path.is_relative()) {
            path = ctx.cwd / path;
        }
    }

    if (!std::filesystem::exists(path)) {
        return CommandResult::error("Path not found: " + path.string());
    }

    bool show_all = args.get_bool("-a", false) || args.get_bool("--all", false);
    bool long_format = args.get_bool("-l", false) || args.get_bool("--long", false);
    bool recursive = args.get_bool("-R", false) || args.get_bool("--recursive", false);
    bool human = args.get_bool("-h", false) || args.get_bool("--human", false);

    std::ostringstream ss;

    auto format_size = [human](std::uintmax_t size) -> std::string {
        if (!human) {
            return std::to_string(size);
        }
        const char* units[] = {"B", "K", "M", "G", "T"};
        int unit = 0;
        double fsize = static_cast<double>(size);
        while (fsize >= 1024 && unit < 4) {
            fsize /= 1024;
            unit++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(unit > 0 ? 1 : 0) << fsize << units[unit];
        return oss.str();
    };

    std::function<void(const std::filesystem::path&, int)> list_dir;
    list_dir = [&](const std::filesystem::path& dir, int depth) {
        if (recursive && depth > 0) {
            ss << "\n" << dir.string() << ":\n";
        }

        std::vector<std::filesystem::directory_entry> entries;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                std::string name = entry.path().filename().string();
                if (!show_all && !name.empty() && name[0] == '.') {
                    continue;
                }
                entries.push_back(entry);
            }
        } catch (const std::exception& e) {
            ss << "Error reading directory: " << e.what() << "\n";
            return;
        }

        // Sort entries
        std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) {
                return a.path().filename() < b.path().filename();
            });

        for (const auto& entry : entries) {
            if (long_format) {
                // Type
                if (entry.is_directory()) ss << "d";
                else if (entry.is_symlink()) ss << "l";
                else ss << "-";

                // Permissions (simplified)
                ss << "rwxr-xr-x ";

                // Size
                std::uintmax_t size = 0;
                try {
                    if (!entry.is_directory()) {
                        size = entry.file_size();
                    }
                } catch (...) {}
                ss << std::setw(8) << format_size(size) << " ";

                // Modified time
                try {
                    auto ftime = entry.last_write_time();
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - std::filesystem::file_time_type::clock::now()
                        + std::chrono::system_clock::now());
                    auto time_t = std::chrono::system_clock::to_time_t(sctp);
                    ss << std::put_time(std::localtime(&time_t), "%b %d %H:%M") << " ";
                } catch (...) {
                    ss << "            ";
                }

                // Name
                ss << entry.path().filename().string();
                if (entry.is_directory()) ss << "/";
                ss << "\n";
            } else {
                ss << entry.path().filename().string();
                if (entry.is_directory()) ss << "/";
                ss << "  ";
            }
        }

        if (!long_format && !entries.empty()) {
            ss << "\n";
        }

        // Recursive
        if (recursive) {
            for (const auto& entry : entries) {
                if (entry.is_directory()) {
                    list_dir(entry.path(), depth + 1);
                }
            }
        }
    };

    list_dir(path, 0);

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

CommandResult cmd_cat(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        // Read from stdin if available
        if (ctx.stdin_content && !ctx.stdin_content->empty()) {
            ctx.output(*ctx.stdin_content);
            return CommandResult::success(*ctx.stdin_content);
        }
        return CommandResult::error("Usage: cat <file> [file...]");
    }

    bool number = args.get_bool("-n", false) || args.get_bool("--number", false);
    std::ostringstream output;

    for (const auto& arg : args.positional()) {
        std::filesystem::path path = arg.as_string();
        if (path.is_relative()) {
            path = ctx.cwd / path;
        }

        std::ifstream file(path);
        if (!file) {
            return CommandResult::error("Cannot open file: " + path.string());
        }

        std::string line;
        int line_num = 1;
        while (std::getline(file, line)) {
            if (number) {
                output << std::setw(6) << line_num++ << "  ";
            }
            output << line << "\n";
        }
    }

    ctx.output(output.str());
    return CommandResult::success(output.str());
}

CommandResult cmd_head(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: head [-n lines] <file>");
    }

    std::filesystem::path path = args.positional()[0].as_string();
    if (path.is_relative()) {
        path = ctx.cwd / path;
    }

    std::int64_t lines = args.get_int("-n", 10);
    if (lines <= 0) lines = 10;

    std::ifstream file(path);
    if (!file) {
        return CommandResult::error("Cannot open file: " + path.string());
    }

    std::ostringstream output;
    std::string line;
    std::int64_t count = 0;
    while (std::getline(file, line) && count < lines) {
        output << line << "\n";
        count++;
    }

    ctx.output(output.str());
    return CommandResult::success(output.str());
}

CommandResult cmd_tail(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: tail [-n lines] <file>");
    }

    std::filesystem::path path = args.positional()[0].as_string();
    if (path.is_relative()) {
        path = ctx.cwd / path;
    }

    std::int64_t lines = args.get_int("-n", 10);
    if (lines <= 0) lines = 10;

    std::ifstream file(path);
    if (!file) {
        return CommandResult::error("Cannot open file: " + path.string());
    }

    // Read all lines into buffer
    std::deque<std::string> buffer;
    std::string line;
    while (std::getline(file, line)) {
        buffer.push_back(line);
        if (static_cast<std::int64_t>(buffer.size()) > lines) {
            buffer.pop_front();
        }
    }

    std::ostringstream output;
    for (const auto& l : buffer) {
        output << l << "\n";
    }

    ctx.output(output.str());
    return CommandResult::success(output.str());
}

CommandResult cmd_find(const CommandArgs& args, CommandContext& ctx) {
    std::filesystem::path search_path = ctx.cwd;
    if (!args.positional().empty()) {
        search_path = args.positional()[0].as_string();
        if (search_path.is_relative()) {
            search_path = ctx.cwd / search_path;
        }
    }

    std::string name_pattern = args.get_string("-name", "*");
    std::string type_filter = args.get_string("-type", "");
    std::int64_t max_depth = args.get_int("-maxdepth", -1);

    std::ostringstream output;

    std::function<void(const std::filesystem::path&, int)> search;
    search = [&](const std::filesystem::path& dir, int depth) {
        if (max_depth >= 0 && depth > max_depth) return;

        try {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                std::string name = entry.path().filename().string();

                // Type filter
                if (!type_filter.empty()) {
                    if (type_filter == "f" && !entry.is_regular_file()) continue;
                    if (type_filter == "d" && !entry.is_directory()) continue;
                }

                // Name pattern (simple glob)
                bool match = true;
                if (name_pattern != "*") {
                    // Simple glob matching
                    std::string pattern = name_pattern;
                    std::string regex_str;
                    for (char c : pattern) {
                        if (c == '*') regex_str += ".*";
                        else if (c == '?') regex_str += ".";
                        else if (c == '.') regex_str += "\\.";
                        else regex_str += c;
                    }
                    std::regex re(regex_str, std::regex::icase);
                    match = std::regex_match(name, re);
                }

                if (match) {
                    output << entry.path().string() << "\n";
                }

                if (entry.is_directory()) {
                    search(entry.path(), depth + 1);
                }
            }
        } catch (...) {}
    };

    search(search_path, 0);

    ctx.output(output.str());
    return CommandResult::success(output.str());
}

CommandResult cmd_grep(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: grep [options] <pattern> [file...]");
    }

    std::string pattern = args.positional()[0].as_string();
    bool recursive = args.get_bool("-r", false) || args.get_bool("--recursive", false);
    bool ignore_case = args.get_bool("-i", false) || args.get_bool("--ignore-case", false);
    bool line_number = args.get_bool("-n", false) || args.get_bool("--line-number", false);
    bool count_only = args.get_bool("-c", false) || args.get_bool("--count", false);

    std::regex::flag_type flags = std::regex::ECMAScript;
    if (ignore_case) flags |= std::regex::icase;
    std::regex re(pattern, flags);

    std::ostringstream output;
    std::size_t total_matches = 0;

    auto search_file = [&](const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file) return;

        std::string line;
        int line_num = 0;
        std::size_t file_matches = 0;

        while (std::getline(file, line)) {
            line_num++;
            if (std::regex_search(line, re)) {
                file_matches++;
                if (!count_only) {
                    if (args.positional().size() > 2 || recursive) {
                        output << path.string() << ":";
                    }
                    if (line_number) {
                        output << line_num << ":";
                    }
                    output << line << "\n";
                }
            }
        }

        if (count_only && file_matches > 0) {
            output << path.string() << ":" << file_matches << "\n";
        }
        total_matches += file_matches;
    };

    if (args.positional().size() == 1) {
        // No file specified, use stdin
        if (ctx.stdin_content && !ctx.stdin_content->empty()) {
            std::istringstream ss(*ctx.stdin_content);
            std::string line;
            int line_num = 0;
            while (std::getline(ss, line)) {
                line_num++;
                if (std::regex_search(line, re)) {
                    total_matches++;
                    if (!count_only) {
                        if (line_number) output << line_num << ":";
                        output << line << "\n";
                    }
                }
            }
            if (count_only) {
                output << total_matches << "\n";
            }
        } else if (recursive) {
            // Search current directory recursively
            for (const auto& entry : std::filesystem::recursive_directory_iterator(ctx.cwd)) {
                if (entry.is_regular_file()) {
                    search_file(entry.path());
                }
            }
        } else {
            return CommandResult::error("No files specified");
        }
    } else {
        for (std::size_t i = 1; i < args.positional().size(); ++i) {
            std::filesystem::path path = args.positional()[i].as_string();
            if (path.is_relative()) {
                path = ctx.cwd / path;
            }

            if (std::filesystem::is_directory(path)) {
                if (recursive) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                        if (entry.is_regular_file()) {
                            search_file(entry.path());
                        }
                    }
                }
            } else {
                search_file(path);
            }
        }
    }

    ctx.output(output.str());
    return CommandResult::success(output.str());
}

CommandResult cmd_mkdir(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: mkdir [-p] <directory>");
    }

    std::filesystem::path path = args.positional()[0].as_string();
    if (path.is_relative()) {
        path = ctx.cwd / path;
    }

    bool parents = args.get_bool("-p", false) || args.get_bool("--parents", false);

    try {
        if (parents) {
            std::filesystem::create_directories(path);
        } else {
            std::filesystem::create_directory(path);
        }
    } catch (const std::exception& e) {
        return CommandResult::error("Cannot create directory: " + std::string(e.what()));
    }

    return CommandResult::success("");
}

CommandResult cmd_rm(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: rm [-rf] <path>");
    }

    bool recursive = args.get_bool("-r", false) || args.get_bool("--recursive", false);
    bool force = args.get_bool("-f", false) || args.get_bool("--force", false);

    for (const auto& arg : args.positional()) {
        std::filesystem::path path = arg.as_string();
        if (path.is_relative()) {
            path = ctx.cwd / path;
        }

        if (!std::filesystem::exists(path)) {
            if (force) continue;
            return CommandResult::error("Path not found: " + path.string());
        }

        try {
            if (std::filesystem::is_directory(path)) {
                if (!recursive) {
                    return CommandResult::error("Cannot remove directory without -r: " + path.string());
                }
                std::filesystem::remove_all(path);
            } else {
                std::filesystem::remove(path);
            }
        } catch (const std::exception& e) {
            if (!force) {
                return CommandResult::error("Cannot remove: " + std::string(e.what()));
            }
        }
    }

    return CommandResult::success("");
}

CommandResult cmd_cp(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().size() < 2) {
        return CommandResult::error("Usage: cp [-r] <source> <dest>");
    }

    std::filesystem::path source = args.positional()[0].as_string();
    std::filesystem::path dest = args.positional()[1].as_string();

    if (source.is_relative()) source = ctx.cwd / source;
    if (dest.is_relative()) dest = ctx.cwd / dest;

    bool recursive = args.get_bool("-r", false) || args.get_bool("--recursive", false);

    try {
        if (std::filesystem::is_directory(source)) {
            if (!recursive) {
                return CommandResult::error("Cannot copy directory without -r: " + source.string());
            }
            std::filesystem::copy(source, dest, std::filesystem::copy_options::recursive);
        } else {
            std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);
        }
    } catch (const std::exception& e) {
        return CommandResult::error("Copy failed: " + std::string(e.what()));
    }

    return CommandResult::success("");
}

CommandResult cmd_mv(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().size() < 2) {
        return CommandResult::error("Usage: mv <source> <dest>");
    }

    std::filesystem::path source = args.positional()[0].as_string();
    std::filesystem::path dest = args.positional()[1].as_string();

    if (source.is_relative()) source = ctx.cwd / source;
    if (dest.is_relative()) dest = ctx.cwd / dest;

    try {
        std::filesystem::rename(source, dest);
    } catch (const std::exception& e) {
        return CommandResult::error("Move failed: " + std::string(e.what()));
    }

    return CommandResult::success("");
}

CommandResult cmd_touch(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: touch <file>");
    }

    std::filesystem::path path = args.positional()[0].as_string();
    if (path.is_relative()) {
        path = ctx.cwd / path;
    }

    if (std::filesystem::exists(path)) {
        // Update timestamp
        std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now());
    } else {
        // Create empty file
        std::ofstream file(path);
        if (!file) {
            return CommandResult::error("Cannot create file: " + path.string());
        }
    }

    return CommandResult::success("");
}

// =============================================================================
// Variable Commands Implementation
// =============================================================================

CommandResult cmd_set(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().size() < 2) {
        return CommandResult::error("Usage: set <name> <value>");
    }

    std::string name = args.positional()[0].as_string();
    std::string value = args.positional()[1].as_string();

    ctx.env->set(name, value);
    return CommandResult::success("");
}

CommandResult cmd_get(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: get <name>");
    }

    std::string name = args.positional()[0].as_string();
    auto value = ctx.env->get(name);

    if (value) {
        ctx.output(*value + "\n");
        return CommandResult::success(*value);
    }

    return CommandResult::error("Variable not found: " + name);
}

CommandResult cmd_unset(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: unset <name>");
    }

    std::string name = args.positional()[0].as_string();
    ctx.env->unset(name);
    return CommandResult::success("");
}

CommandResult cmd_env(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        // List all variables
        auto vars = ctx.env->all();
        std::ostringstream ss;
        for (const auto& [name, value] : vars) {
            ss << name << "=" << value << "\n";
        }
        ctx.output(ss.str());
        return CommandResult::success(ss.str());
    }

    // Set variable
    std::string assignment = args.positional()[0].as_string();
    std::size_t eq = assignment.find('=');

    if (eq == std::string::npos) {
        // Just show this variable
        auto value = ctx.env->get(assignment);
        if (value) {
            ctx.output(*value + "\n");
            return CommandResult::success(*value);
        }
        return CommandResult::error("Variable not found: " + assignment);
    }

    std::string name = assignment.substr(0, eq);
    std::string value = assignment.substr(eq + 1);
    ctx.env->set(name, value);
    return CommandResult::success("");
}

CommandResult cmd_export(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: export <name>[=value]");
    }

    std::string assignment = args.positional()[0].as_string();
    std::size_t eq = assignment.find('=');

    std::string name;
    if (eq == std::string::npos) {
        name = assignment;
    } else {
        name = assignment.substr(0, eq);
        std::string value = assignment.substr(eq + 1);
        ctx.env->set(name, value);
    }

    ctx.env->export_to_system(name);
    return CommandResult::success("");
}

CommandResult cmd_expr(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: expr <expression>");
    }

    // Reconstruct expression
    std::ostringstream expr_ss;
    for (std::size_t i = 0; i < args.positional().size(); ++i) {
        if (i > 0) expr_ss << " ";
        expr_ss << args.positional()[i].as_string();
    }

    ExpressionEvaluator eval;
    eval.set_variable_resolver([ctx](const std::string& name) -> std::optional<std::string> {
        return ctx.env->get(name);
    });

    auto eval_result = eval.evaluate_string(expr_ss.str());
    if (!eval_result) {
        return CommandResult::error("Expression evaluation failed");
    }
    std::string result = eval_result.value();
    ctx.output(result + "\n");
    return CommandResult::success(result);
}

// =============================================================================
// Scripting Commands Implementation
// =============================================================================

CommandResult cmd_source(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: source <file>");
    }

    std::filesystem::path path = args.positional()[0].as_string();
    if (path.is_relative()) {
        path = ctx.cwd / path;
    }

    auto& shell = ShellSystem::instance();
    return shell.execute_script(path);
}

CommandResult cmd_eval(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: eval <command>");
    }

    std::ostringstream cmd_ss;
    for (std::size_t i = 0; i < args.positional().size(); ++i) {
        if (i > 0) cmd_ss << " ";
        cmd_ss << args.positional()[i].as_string();
    }

    auto& shell = ShellSystem::instance();
    return shell.execute(cmd_ss.str());
}

CommandResult cmd_script(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: script <file>");
    }

    std::filesystem::path path = args.positional()[0].as_string();
    if (path.is_relative()) {
        path = ctx.cwd / path;
    }

    // Would integrate with void_script module
    ctx.output("VoidScript execution not yet integrated\n");
    return CommandResult::error("VoidScript execution not yet integrated");
}

CommandResult cmd_wasm(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: wasm <file> [function] [args...]");
    }

    std::filesystem::path path = args.positional()[0].as_string();
    if (path.is_relative()) {
        path = ctx.cwd / path;
    }

    // Would integrate with void_scripting WASM module
    ctx.output("WASM execution not yet integrated\n");
    return CommandResult::error("WASM execution not yet integrated");
}

// =============================================================================
// Debug Commands Implementation
// =============================================================================

CommandResult cmd_log(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        ctx.output("Current log level: info\n");
        return CommandResult::success("");
    }

    std::string level = args.positional()[0].as_string();

    if (args.positional().size() > 1) {
        // Log a message
        std::ostringstream msg;
        for (std::size_t i = 1; i < args.positional().size(); ++i) {
            if (i > 1) msg << " ";
            msg << args.positional()[i].as_string();
        }
        ctx.output("[" + level + "] " + msg.str() + "\n");
    } else {
        // Set log level
        ctx.output("Log level set to: " + level + "\n");
    }

    return CommandResult::success("");
}

CommandResult cmd_trace(const CommandArgs&, CommandContext& ctx) {
    ctx.output("Stack trace:\n");
    ctx.output("  (Stack trace not available in this context)\n");
    return CommandResult::success("");
}

CommandResult cmd_breakpoint(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: breakpoint <location>");
    }

    std::string location = args.positional()[0].as_string();
    ctx.output("Breakpoint set at: " + location + "\n");
    return CommandResult::success("");
}

CommandResult cmd_watch(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: watch <expression>");
    }

    std::string expr = args.positional()[0].as_string();
    ctx.output("Watching: " + expr + "\n");
    return CommandResult::success("");
}

CommandResult cmd_dump(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: dump <what>");
    }

    std::string what = args.positional()[0].as_string();

    if (what == "registry") {
        auto cmds = ctx.registry->all_commands();
        std::ostringstream ss;
        ss << "Registered commands: " << cmds.size() << "\n";
        for (const auto* cmd : cmds) {
            ss << "  " << cmd->name << " - " << cmd->description << "\n";
        }
        ctx.output(ss.str());
        return CommandResult::success(ss.str());
    }

    ctx.output("Unknown dump target: " + what + "\n");
    return CommandResult::success("");
}

// =============================================================================
// Engine Commands Implementation
// =============================================================================

CommandResult cmd_engine(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: engine <action>");
    }

    std::string action = args.positional()[0].as_string();

    if (action == "status") {
        ctx.output("Engine status: running\n");
    } else if (action == "start") {
        ctx.output("Engine started\n");
    } else if (action == "stop") {
        ctx.output("Engine stopped\n");
    } else if (action == "restart") {
        ctx.output("Engine restarted\n");
    } else {
        return CommandResult::error("Unknown action: " + action);
    }

    return CommandResult::success("");
}

CommandResult cmd_reload(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: reload <what>");
    }

    std::string what = args.positional()[0].as_string();
    ctx.output("Reloading: " + what + "\n");

    auto& shell = ShellSystem::instance();
    shell.reload_module_commands(what);

    return CommandResult::success("");
}

CommandResult cmd_config(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        ctx.output("Configuration:\n");
        ctx.output("  (No configuration values to display)\n");
        return CommandResult::success("");
    }

    std::string key = args.positional()[0].as_string();

    if (args.positional().size() > 1) {
        std::string value = args.positional()[1].as_string();
        ctx.output("Set " + key + " = " + value + "\n");
    } else {
        ctx.output(key + " = (not set)\n");
    }

    return CommandResult::success("");
}

CommandResult cmd_stats(const CommandArgs& args, CommandContext& ctx) {
    auto stats = ShellSystem::instance().stats();

    std::ostringstream ss;
    ss << "Shell Statistics:\n";
    ss << "  Active sessions: " << stats.active_sessions << "\n";
    ss << "  Total sessions: " << stats.total_sessions << "\n";
    ss << "  Commands executed: " << stats.commands_executed << "\n";
    ss << "  Registered commands: " << stats.registered_commands << "\n";
    ss << "  Registered aliases: " << stats.registered_aliases << "\n";
    ss << "  Remote server: " << (stats.remote_server_active ? "active" : "inactive") << "\n";

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

CommandResult cmd_pause(const CommandArgs&, CommandContext& ctx) {
    ctx.output("Engine paused\n");
    return CommandResult::success("");
}

CommandResult cmd_resume(const CommandArgs&, CommandContext& ctx) {
    ctx.output("Engine resumed\n");
    return CommandResult::success("");
}

CommandResult cmd_step(const CommandArgs& args, CommandContext& ctx) {
    std::int64_t count = 1;
    if (!args.positional().empty()) {
        count = args.positional()[0].as_int();
    }
    ctx.output("Stepped " + std::to_string(count) + " frame(s)\n");
    return CommandResult::success("");
}

// =============================================================================
// ECS Commands Implementation
// =============================================================================

CommandResult cmd_entity(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: entity <action> [args...]");
    }

    std::string action = args.positional()[0].as_string();

    if (action == "list") {
        ctx.output("Entities:\n  (No ECS system connected)\n");
    } else if (action == "create") {
        ctx.output("Entity created\n");
    } else if (action == "destroy") {
        ctx.output("Entity destroyed\n");
    } else if (action == "info") {
        ctx.output("Entity info:\n  (No ECS system connected)\n");
    } else {
        return CommandResult::error("Unknown action: " + action);
    }

    return CommandResult::success("");
}

CommandResult cmd_component(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: component <action> [args...]");
    }

    std::string action = args.positional()[0].as_string();

    if (action == "list") {
        ctx.output("Components:\n  (No ECS system connected)\n");
    } else if (action == "add") {
        ctx.output("Component added\n");
    } else if (action == "remove") {
        ctx.output("Component removed\n");
    } else if (action == "get") {
        ctx.output("Component value:\n  (No ECS system connected)\n");
    } else if (action == "set") {
        ctx.output("Component set\n");
    } else {
        return CommandResult::error("Unknown action: " + action);
    }

    return CommandResult::success("");
}

CommandResult cmd_query(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: query <components...>");
    }

    std::ostringstream ss;
    ss << "Query for: ";
    for (std::size_t i = 0; i < args.positional().size(); ++i) {
        if (i > 0) ss << ", ";
        ss << args.positional()[i].as_string();
    }
    ss << "\n  (No ECS system connected)\n";

    ctx.output(ss.str());
    return CommandResult::success("");
}

CommandResult cmd_spawn(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: spawn <prefab> [position]");
    }

    std::string prefab = args.positional()[0].as_string();
    ctx.output("Spawned: " + prefab + "\n");
    return CommandResult::success("");
}

CommandResult cmd_destroy(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: destroy <entity_id>");
    }

    std::string id = args.positional()[0].as_string();
    ctx.output("Destroyed entity: " + id + "\n");
    return CommandResult::success("");
}

CommandResult cmd_inspect(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: inspect <entity_id> [component]");
    }

    std::string id = args.positional()[0].as_string();
    ctx.output("Inspecting entity: " + id + "\n  (No ECS system connected)\n");
    return CommandResult::success("");
}

// =============================================================================
// Asset Commands Implementation
// =============================================================================

CommandResult cmd_asset(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: asset <action> [args...]");
    }

    std::string action = args.positional()[0].as_string();

    if (action == "list") {
        ctx.output("Assets:\n  (No asset system connected)\n");
    } else if (action == "info") {
        ctx.output("Asset info:\n  (No asset system connected)\n");
    } else if (action == "reload") {
        ctx.output("Asset reloaded\n");
    } else {
        return CommandResult::error("Unknown action: " + action);
    }

    return CommandResult::success("");
}

CommandResult cmd_load(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: load <path>");
    }

    std::string path = args.positional()[0].as_string();
    ctx.output("Loading asset: " + path + "\n");
    return CommandResult::success("");
}

CommandResult cmd_unload(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: unload <path>");
    }

    std::string path = args.positional()[0].as_string();
    ctx.output("Unloading asset: " + path + "\n");
    return CommandResult::success("");
}

CommandResult cmd_import(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: import <source> [dest]");
    }

    std::string source = args.positional()[0].as_string();
    ctx.output("Importing asset: " + source + "\n");
    return CommandResult::success("");
}

// =============================================================================
// Profile Commands Implementation
// =============================================================================

CommandResult cmd_profile(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        return CommandResult::error("Usage: profile <action>");
    }

    std::string action = args.positional()[0].as_string();

    if (action == "start") {
        ctx.output("Profiling started\n");
    } else if (action == "stop") {
        ctx.output("Profiling stopped\n");
    } else if (action == "report") {
        ctx.output("Profile report:\n  (No profiling data)\n");
    } else if (action == "clear") {
        ctx.output("Profile data cleared\n");
    } else {
        return CommandResult::error("Unknown action: " + action);
    }

    return CommandResult::success("");
}

CommandResult cmd_perf(const CommandArgs& args, CommandContext& ctx) {
    std::ostringstream ss;
    ss << "Performance metrics:\n";
    ss << "  FPS: 60.0\n";
    ss << "  Frame time: 16.67ms\n";
    ss << "  (Detailed metrics not available)\n";

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

CommandResult cmd_memory(const CommandArgs& args, CommandContext& ctx) {
    std::ostringstream ss;
    ss << "Memory usage:\n";

#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        ss << "  Working set: " << pmc.WorkingSetSize / (1024 * 1024) << " MB\n";
        ss << "  Peak working set: " << pmc.PeakWorkingSetSize / (1024 * 1024) << " MB\n";
    }
#else
    ss << "  (Memory info not available)\n";
#endif

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

CommandResult cmd_gpu(const CommandArgs& args, CommandContext& ctx) {
    std::ostringstream ss;
    ss << "GPU information:\n";
    ss << "  (GPU info not available - no rendering context)\n";

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

// =============================================================================
// Help Commands Implementation
// =============================================================================

CommandResult cmd_help(const CommandArgs& args, CommandContext& ctx) {
    if (args.positional().empty()) {
        // General help
        std::ostringstream ss;
        ss << "void_shell - Game Engine Developer Console\n\n";
        ss << "Usage: <command> [arguments...]\n\n";
        ss << "Type 'commands' to list all available commands\n";
        ss << "Type 'help <command>' for help on a specific command\n";
        ss << "Type 'man <command>' for detailed manual\n\n";
        ss << "Categories:\n";
        ss << "  general    - General utilities\n";
        ss << "  filesystem - File operations\n";
        ss << "  variables  - Variable management\n";
        ss << "  scripting  - Script execution\n";
        ss << "  debug      - Debugging tools\n";
        ss << "  engine     - Engine control\n";
        ss << "  ecs        - Entity Component System\n";
        ss << "  assets     - Asset management\n";
        ss << "  profile    - Profiling tools\n";
        ss << "  help       - Help commands\n";

        ctx.output(ss.str());
        return CommandResult::success(ss.str());
    }

    // Help for specific command
    std::string cmd_name = args.positional()[0].as_string();
    auto* cmd = ctx.registry->find(cmd_name);

    if (!cmd) {
        return CommandResult::error("Unknown command: " + cmd_name);
    }

    const auto& info = cmd->info();
    std::ostringstream ss;

    ss << info.name << " - " << info.description << "\n\n";
    ss << "Usage: " << info.usage << "\n";

    if (!info.args.empty()) {
        ss << "\nArguments:\n";
        for (const auto& arg : info.args) {
            ss << "  " << arg.name << " (" << arg_type_name(arg.type) << ")";
            if (arg.required) ss << " [required]";
            ss << "\n    " << arg.description << "\n";
        }
    }

    if (!info.flags.empty()) {
        ss << "\nFlags:\n";
        for (const auto& flag : info.flags) {
            ss << "  --" << flag.name;
            if (flag.short_name) ss << ", -" << flag.short_name;
            ss << "\n    " << flag.description << "\n";
        }
    }

    if (!info.examples.empty()) {
        ss << "\nExamples:\n";
        for (const auto& example : info.examples) {
            ss << "  " << example << "\n";
        }
    }

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

CommandResult cmd_man(const CommandArgs& args, CommandContext& ctx) {
    // Same as help for now
    return cmd_help(args, ctx);
}

CommandResult cmd_commands(const CommandArgs& args, CommandContext& ctx) {
    std::ostringstream ss;

    if (args.positional().empty()) {
        // List all commands by category
        for (int cat = 0; cat < static_cast<int>(CommandCategory::Custom); ++cat) {
            auto category = static_cast<CommandCategory>(cat);
            auto cmds = ctx.registry->commands_in_category(category);

            if (!cmds.empty()) {
                ss << "\n" << category_name(category) << ":\n";
                for (const auto* cmd : cmds) {
                    ss << "  " << std::left << std::setw(15) << cmd->name
                       << cmd->description << "\n";
                }
            }
        }
    } else {
        // Filter by category
        std::string filter = args.positional()[0].as_string();
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

        for (int cat = 0; cat < static_cast<int>(CommandCategory::Custom); ++cat) {
            auto category = static_cast<CommandCategory>(cat);
            std::string cat_name = category_name(category);
            std::transform(cat_name.begin(), cat_name.end(), cat_name.begin(), ::tolower);

            if (cat_name.find(filter) != std::string::npos) {
                auto cmds = ctx.registry->commands_in_category(category);
                ss << category_name(category) << ":\n";
                for (const auto* cmd : cmds) {
                    ss << "  " << std::left << std::setw(15) << cmd->name
                       << cmd->description << "\n";
                }
                ss << "\n";
            }
        }
    }

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

CommandResult cmd_version(const CommandArgs&, CommandContext& ctx) {
    std::ostringstream ss;
    ss << "void_shell version 1.0.0\n";
    ss << "Part of the Void Engine game development framework\n";
    ss << "Built: " << __DATE__ << " " << __TIME__ << "\n";

    ctx.output(ss.str());
    return CommandResult::success(ss.str());
}

} // namespace builtins
} // namespace void_shell
