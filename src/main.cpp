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
#include <void_engine/runtime/mode_selector.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

/// @brief Initialize logging
void init_logging(bool verbose, bool debug) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("void_engine", console_sink);

    if (verbose) {
        logger->set_level(spdlog::level::trace);
    } else if (debug) {
        logger->set_level(spdlog::level::debug);
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
/// 1. Parse CLI / load config via ModeSelector
/// 2. Validate mode against platform capabilities
/// 3. Create Runtime with validated config
/// 4. Call run()
///
/// The Runtime handles all lifecycle management:
/// - Kernel initialization (stages, hot-reload)
/// - Foundation/infrastructure boot
/// - World loading via API
/// - Plugin/widget activation
/// - Frame loop execution
/// - Graceful shutdown
int main(int argc, char* argv[]) {
    // Create mode selector and apply configuration sources
    // Priority order: defaults < environment < manifest < CLI
    void_runtime::ModeSelector selector;
    selector.apply_defaults();
    selector.apply_environment();

    // Parse CLI arguments
    if (auto result = selector.apply_cli(argc, argv); !result) {
        spdlog::error("Failed to parse arguments: {}", result.error().message());
        return EXIT_FAILURE;
    }

    // Initialize logging based on current config state
    init_logging(selector.current_config().verbose, selector.current_config().debug_mode);

    // Handle help/version before anything else
    if (selector.help_requested()) {
        void_runtime::ModeSelector::print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (selector.version_requested()) {
        void_runtime::ModeSelector::print_version();
        return EXIT_SUCCESS;
    }

    // Load manifest if specified (may override other settings)
    if (!selector.current_config().manifest_path.empty()) {
        if (auto result = selector.apply_manifest(selector.current_config().manifest_path); !result) {
            spdlog::warn("Failed to load manifest: {}", result.error().message());
        }
    }

    // Select and validate mode
    auto mode_result = selector.select_mode();
    if (mode_result.fallback_used) {
        spdlog::warn("Requested mode '{}' not available: {}",
                     void_runtime::mode_to_string(mode_result.requested_mode),
                     mode_result.fallback_reason);
        spdlog::warn("Falling back to '{}' mode",
                     void_runtime::mode_to_string(mode_result.selected_mode));
    }

    // Build final configuration
    auto config = selector.build_config();

    // Handle validation mode
    if (config.validation_mode) {
        spdlog::info("Validation mode requested - use main-bootstrap executable");
        spdlog::info("Build with: cmake --build build --target void_engine_bootstrap");
        return EXIT_SUCCESS;
    }

    // Log startup information
    spdlog::info("void_engine starting...");
    spdlog::info("  Mode: {}", void_runtime::to_string(config.mode));
    spdlog::info("  Window: {}x{} {}",
                 config.window_width, config.window_height,
                 config.fullscreen ? "(fullscreen)" : "(windowed)");

    if (!config.manifest_path.empty()) {
        spdlog::info("  Manifest: {}", config.manifest_path);
    }
    if (!config.initial_world.empty()) {
        spdlog::info("  Initial world: {}", config.initial_world);
    }
    if (!config.api_endpoint.empty()) {
        spdlog::info("  API endpoint: {}", config.api_endpoint);
    }
    if (config.debug_mode) {
        spdlog::info("  Debug mode: enabled");
    }
    if (config.gpu_validation) {
        spdlog::info("  GPU validation: enabled");
    }
    if (!config.enable_hot_reload) {
        spdlog::info("  Hot-reload: disabled");
    }

    // Log available modes for debugging
    if (config.verbose) {
        spdlog::debug("Available modes:");
        for (auto mode : selector.available_modes()) {
            spdlog::debug("  - {}", void_runtime::mode_to_string(mode));
        }
    }

    // Create and run the Runtime
    void_runtime::Runtime runtime(config);

    auto result = runtime.initialize();
    if (!result) {
        spdlog::error("Runtime initialization failed: {}", result.error().message());
        return EXIT_FAILURE;
    }

    spdlog::info("Runtime initialized successfully");

    // Run the main loop (blocks until exit)
    int exit_code = runtime.run();

    // Graceful shutdown
    runtime.shutdown();

    spdlog::info("void_engine shutdown complete (exit code: {})", exit_code);
    return exit_code;
}
