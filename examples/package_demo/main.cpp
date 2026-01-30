/// @file main.cpp
/// @brief Package System Demo
///
/// Demonstrates the void_engine package system by loading a complete game world
/// with plugins, assets, layers, and widgets - all defined in JSON manifests.

#include <void_engine/runtime/runtime.hpp>
#include <void_engine/package/package.hpp>
#include <void_engine/package/world_composer.hpp>
#include <void_engine/package/registry.hpp>
#include <void_engine/package/prefab_registry.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>

namespace {

/// Get the packages directory relative to the executable
std::filesystem::path get_packages_path() {
    // Try several common locations
    std::vector<std::filesystem::path> candidates = {
        "packages",
        "../packages",
        "../../packages",
        "examples/package_demo/packages",
        "../examples/package_demo/packages",
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path);
        }
    }

    // Default to current directory
    return std::filesystem::current_path() / "packages";
}

/// Print package system status
void print_package_status(void_runtime::Runtime& runtime) {
    auto* registry = runtime.package_registry();
    if (!registry) {
        spdlog::warn("No package registry available");
        return;
    }

    spdlog::info("=== Package System Status ===");
    spdlog::info("Available packages: {}", registry->available_count());
    spdlog::info("Loaded packages: {}", registry->loaded_count());

    // List by type
    auto plugins = registry->packages_of_type(void_package::PackageType::Plugin);
    spdlog::info("  Plugins: {}", plugins.size());
    for (const auto& name : plugins) {
        spdlog::info("    - {}", name);
    }

    auto assets = registry->packages_of_type(void_package::PackageType::Asset);
    spdlog::info("  Assets: {}", assets.size());
    for (const auto& name : assets) {
        spdlog::info("    - {}", name);
    }

    auto layers = registry->packages_of_type(void_package::PackageType::Layer);
    spdlog::info("  Layers: {}", layers.size());
    for (const auto& name : layers) {
        spdlog::info("    - {}", name);
    }

    auto widgets = registry->packages_of_type(void_package::PackageType::Widget);
    spdlog::info("  Widgets: {}", widgets.size());
    for (const auto& name : widgets) {
        spdlog::info("    - {}", name);
    }

    auto worlds = registry->packages_of_type(void_package::PackageType::World);
    spdlog::info("  Worlds: {}", worlds.size());
    for (const auto& name : worlds) {
        spdlog::info("    - {}", name);
    }
}

/// Print world composer status
void print_world_status(void_runtime::Runtime& runtime) {
    auto* composer = runtime.world_composer();
    if (!composer) {
        spdlog::warn("No world composer available");
        return;
    }

    spdlog::info("=== World Status ===");
    spdlog::info("{}", composer->format_state());
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    try {
    // Configure logging
    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== Package System Demo ===");

    // Determine packages path
    auto packages_path = get_packages_path();
    spdlog::info("Packages directory: {}", packages_path.string());

    if (!std::filesystem::exists(packages_path)) {
        spdlog::error("Packages directory not found: {}", packages_path.string());
        spdlog::error("Please run from the examples/package_demo directory or build directory");
        return EXIT_FAILURE;
    }

    // Configure runtime for windowed rendering
    void_runtime::RuntimeConfig config;
    config.mode = void_runtime::RuntimeMode::Windowed;  // Windowed mode for rendering
    config.content_path = packages_path.string();
    config.initial_world = "world.demo_arena";
    config.window_title = "Package System Demo";
    config.window_width = 1280;
    config.window_height = 720;
    config.target_fps = 60;
    config.vsync = true;
    config.enable_hot_reload = true;
    config.debug_mode = true;

    // Add built plugins directory to search paths
    // This allows the demo to find DLL-based plugins like base.health
    auto plugins_path = std::filesystem::absolute(packages_path / ".." / ".." / ".." / "build" / "plugins" / "Debug");
    if (std::filesystem::exists(plugins_path)) {
        config.plugin_paths.push_back(plugins_path);
        spdlog::info("Added plugin path: {}", plugins_path.string());
    } else {
        // Try release build path
        plugins_path = std::filesystem::absolute(packages_path / ".." / ".." / ".." / "build" / "plugins" / "Release");
        if (std::filesystem::exists(plugins_path)) {
            config.plugin_paths.push_back(plugins_path);
            spdlog::info("Added plugin path: {}", plugins_path.string());
        }
    }

    // Create runtime
    spdlog::info("Creating runtime...");
    void_runtime::Runtime runtime(config);

    // Initialize
    spdlog::info("Initializing runtime...");
    auto init_result = runtime.initialize();
    if (!init_result) {
        spdlog::error("Failed to initialize runtime: {}", init_result.error().message());
        spdlog::default_logger()->flush();
        return EXIT_FAILURE;
    }

    spdlog::info("Runtime initialization succeeded, entering demo code...");
    spdlog::default_logger()->flush();

    // Print package status after initialization
    print_package_status(runtime);
    print_world_status(runtime);

    // Demonstrate layer application
    if (auto* composer = runtime.world_composer()) {
        spdlog::info("=== Demonstrating Layer System ===");

        // Apply night mode layer
        spdlog::info("Applying night mode layer...");
        auto night_result = composer->apply_layer("layer.night_mode");
        if (night_result) {
            spdlog::info("Night mode applied successfully!");
        } else {
            spdlog::warn("Could not apply night mode: {}", night_result.error().message());
        }

        // List applied layers
        auto layers = composer->applied_layers();
        spdlog::info("Applied layers: {}", layers.size());
        for (const auto& layer : layers) {
            spdlog::info("  - {}", layer);
        }

        // Apply hard mode too
        spdlog::info("Applying hard mode layer...");
        auto hard_result = composer->apply_layer("layer.hard_mode");
        if (hard_result) {
            spdlog::info("Hard mode applied successfully!");
        } else {
            spdlog::warn("Could not apply hard mode: {}", hard_result.error().message());
        }

        print_world_status(runtime);
    }

    // Demonstrate prefab spawning
    if (auto* prefabs = runtime.prefab_registry()) {
        spdlog::info("=== Demonstrating Prefab System ===");

        // Check available prefabs
        auto all_prefabs = prefabs->all_prefab_ids();
        spdlog::info("Available prefabs: {}", all_prefabs.size());
        for (const auto& id : all_prefabs) {
            auto* def = prefabs->get(id);
            if (def) {
                spdlog::info("  - {} (components: {}, tags: {})",
                             id, def->components.size(), def->tags.size());
            }
        }

        // Spawn some enemies using the prefab system
        if (auto* ecs = runtime.ecs_world()) {
            spdlog::info("Spawning enemies from prefabs...");

            for (int i = 0; i < 3; i++) {
                void_package::TransformData transform;
                transform.position = {
                    static_cast<float>(i * 10 - 10),
                    1.0f,
                    static_cast<float>(i * 5)
                };

                auto result = prefabs->instantiate("enemy_prefab", *ecs, transform);
                if (result) {
                    spdlog::info("  Spawned enemy {} at ({}, {}, {})",
                                 i, transform.position[0], transform.position[1], transform.position[2]);
                } else {
                    spdlog::warn("  Failed to spawn enemy {}: {}", i, result.error().message());
                }
            }

            spdlog::info("Total entities in ECS: {}", ecs->entity_count());
        }
    }

    // Demonstrate world switching
    spdlog::info("=== Demonstrating World System ===");
    spdlog::info("Current world: {}", runtime.current_world());

    spdlog::info("=== Entering Main Loop ===");
    spdlog::info("Press ESC or close window to exit");
    spdlog::default_logger()->flush();

    // Run the engine main loop - this is what a production game does
    int exit_code = runtime.run();

    spdlog::info("Main loop exited with code: {}", exit_code);
    spdlog::default_logger()->flush();

    // Cleanup
    spdlog::info("Shutting down...");
    runtime.shutdown();

    spdlog::info("Goodbye!");
    return exit_code;

    } catch (const std::exception& e) {
        spdlog::error("FATAL EXCEPTION: {}", e.what());
        spdlog::default_logger()->flush();
        return EXIT_FAILURE;
    } catch (...) {
        spdlog::error("FATAL: Unknown exception caught");
        spdlog::default_logger()->flush();
        return EXIT_FAILURE;
    }
}
