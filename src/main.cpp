/// @file main.cpp
/// @brief Production entry point for void_engine
///
/// This is a thin entry point that delegates to Runtime.
/// The validation harness is preserved in main-bootstrap.cpp.
///
/// Architecture invariants (from doc/review):
/// - ECS is authoritative
/// - Scene == World
/// - Plugins contain systems
/// - Widgets are reactive views
/// - Layers are patches, not owners
/// - Kernel orchestrates reload
/// - Runtime owns lifecycle
/// - Everything is loadable via API

#include <void_engine/runtime/runtime.hpp>
#include <void_engine/runtime/runtime_config.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

/// @brief Parse command line arguments into RuntimeConfig
void_runtime::RuntimeConfig parse_arguments(int argc, char* argv[]) {
    void_runtime::RuntimeConfig config;

    std::vector<std::string> args(argv + 1, argv + argc);

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        // Mode selection
        if (arg == "--headless") {
            config.mode = void_runtime::RuntimeMode::Headless;
        } else if (arg == "--windowed") {
            config.mode = void_runtime::RuntimeMode::Windowed;
        } else if (arg == "--xr") {
            config.mode = void_runtime::RuntimeMode::XR;
        } else if (arg == "--editor") {
            config.mode = void_runtime::RuntimeMode::Editor;
        }

        // Manifest / world specification
        else if (arg == "--manifest" && i + 1 < args.size()) {
            config.manifest_path = args[++i];
        } else if (arg == "--world" && i + 1 < args.size()) {
            config.initial_world = args[++i];
        } else if (arg == "--api-endpoint" && i + 1 < args.size()) {
            config.api_endpoint = args[++i];
        }

        // Debug / development flags
        else if (arg == "--debug") {
            config.debug_mode = true;
        } else if (arg == "--validate") {
            config.validation_mode = true;
        } else if (arg == "--no-hot-reload") {
            config.enable_hot_reload = false;
        } else if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
        }

        // Window settings
        else if (arg == "--width" && i + 1 < args.size()) {
            config.window_width = std::stoi(args[++i]);
        } else if (arg == "--height" && i + 1 < args.size()) {
            config.window_height = std::stoi(args[++i]);
        } else if (arg == "--fullscreen") {
            config.fullscreen = true;
        }

        // Help
        else if (arg == "--help" || arg == "-h") {
            config.show_help = true;
        }

        // Version
        else if (arg == "--version") {
            config.show_version = true;
        }
    }

    return config;
}

/// @brief Print usage information
void print_usage(const char* program_name) {
    spdlog::info("void_engine - ECS-first, hot-reloadable, XR-native game engine");
    spdlog::info("");
    spdlog::info("Usage: {} [options]", program_name);
    spdlog::info("");
    spdlog::info("Mode selection:");
    spdlog::info("  --headless          Run without graphics (server/compute mode)");
    spdlog::info("  --windowed          Run in windowed mode (default)");
    spdlog::info("  --xr                Run in XR mode");
    spdlog::info("  --editor            Run in editor mode");
    spdlog::info("");
    spdlog::info("Content loading:");
    spdlog::info("  --manifest <path>   Load manifest file for world/plugin definitions");
    spdlog::info("  --world <name>      Initial world to load");
    spdlog::info("  --api-endpoint <url> API endpoint for content delivery");
    spdlog::info("");
    spdlog::info("Development:");
    spdlog::info("  --debug             Enable debug mode");
    spdlog::info("  --validate          Run validation harness");
    spdlog::info("  --no-hot-reload     Disable hot-reload");
    spdlog::info("  --verbose, -v       Verbose logging");
    spdlog::info("");
    spdlog::info("Window:");
    spdlog::info("  --width <n>         Window width (default: 1920)");
    spdlog::info("  --height <n>        Window height (default: 1080)");
    spdlog::info("  --fullscreen        Run in fullscreen mode");
    spdlog::info("");
    spdlog::info("Other:");
    spdlog::info("  --help, -h          Show this help");
    spdlog::info("  --version           Show version");
}

/// @brief Print version information
void print_version() {
    spdlog::info("void_engine version 0.12.0");
    spdlog::info("ECS-first, hot-reloadable, XR-native game engine");
    spdlog::info("");
    spdlog::info("Architecture:");
    spdlog::info("  - ECS authoritative (flecs)");
    spdlog::info("  - Hot-reload via Kernel orchestration");
    spdlog::info("  - Plugin-based gameplay");
    spdlog::info("  - Reactive widget system");
    spdlog::info("  - API-driven content delivery");
}

/// @brief Initialize logging
void init_logging(bool verbose) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("void_engine", console_sink);

    if (verbose) {
        logger->set_level(spdlog::level::trace);
    } else {
        logger->set_level(spdlog::level::info);
    }

    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_default_logger(logger);
}

} // anonymous namespace

/// @brief Application entry point
///
/// Production main follows the architecture:
/// 1. Parse CLI / load config
/// 2. Create Runtime
/// 3. Call run()
///
/// The Runtime handles all lifecycle management:
/// - Kernel initialization (stages, hot-reload)
/// - Foundation/infrastructure boot
/// - World loading via API
/// - Plugin/widget activation
/// - Frame loop execution
/// - Graceful shutdown
int main(int argc, char* argv[]) {
    // Parse command line
    auto config = parse_arguments(argc, argv);

    // Initialize logging first
    init_logging(config.verbose);

    // Handle help/version
    if (config.show_help) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (config.show_version) {
        print_version();
        return EXIT_SUCCESS;
    }

    // Validation mode redirects to the bootstrap harness
    if (config.validation_mode) {
        spdlog::info("Validation mode requested - use main-bootstrap executable");
        spdlog::info("Build with: cmake --build build --target void_engine_bootstrap");
        return EXIT_SUCCESS;
    }

    spdlog::info("void_engine starting...");
    spdlog::info("  Mode: {}", void_runtime::to_string(config.mode));

    if (!config.manifest_path.empty()) {
        spdlog::info("  Manifest: {}", config.manifest_path);
    }
    if (!config.initial_world.empty()) {
        spdlog::info("  Initial world: {}", config.initial_world);
    }
    if (!config.api_endpoint.empty()) {
        spdlog::info("  API endpoint: {}", config.api_endpoint);
    }

    // Create and run the Runtime
    // Runtime handles: Kernel init, boot sequence, world loading, main loop, shutdown
    void_runtime::Runtime runtime(config);

    auto result = runtime.initialize();
    if (!result) {
        spdlog::error("Runtime initialization failed: {}", result.error().message());
        return EXIT_FAILURE;
    }

    spdlog::info("Runtime initialized");

    // Run the main loop (blocks until exit)
    int exit_code = runtime.run();

    // Graceful shutdown
    runtime.shutdown();

    spdlog::info("void_engine shutdown complete");
    return exit_code;
}
