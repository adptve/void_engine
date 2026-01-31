/// @file main.cpp
/// @brief Package-based primitives demo
///
/// This example exercises the package-driven ECS world loading by:
/// - Scanning package manifests under ./packages
/// - Loading a world package (which pulls in asset bundles)
/// - Rendering a cube and sphere from model files (non-built-in meshes)

#include <void_engine/runtime/runtime.hpp>
#include <void_engine/package/package.hpp>
#include <void_engine/package/world_composer.hpp>
#include <void_engine/package/registry.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <vector>

namespace {

std::filesystem::path find_packages_path() {
    std::vector<std::filesystem::path> candidates = {
        "packages",
        "../packages",
        "../../packages",
        "examples/package_primitives/packages",
        "../examples/package_primitives/packages",
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path);
        }
    }

    return std::filesystem::current_path() / "packages";
}

void log_package_counts(void_runtime::Runtime& runtime) {
    auto* registry = runtime.package_registry();
    if (!registry) {
        spdlog::warn("No package registry available");
        return;
    }

    spdlog::info("Package counts: {} available / {} loaded",
                 registry->available_count(), registry->loaded_count());
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== Package Primitives Demo ===");

    auto packages_path = find_packages_path();
    spdlog::info("Packages directory: {}", packages_path.string());

    if (!std::filesystem::exists(packages_path)) {
        spdlog::error("Packages directory not found: {}", packages_path.string());
        return EXIT_FAILURE;
    }

    std::error_code ec;
    std::filesystem::current_path(packages_path, ec);
    if (ec) {
        spdlog::warn("Failed to set working directory to packages: {}", ec.message());
    }

    void_runtime::RuntimeConfig config;
    config.mode = void_runtime::RuntimeMode::Windowed;
    config.content_path = packages_path.string();
    config.initial_world = "world.primitives_demo";
    config.window_title = "Package Primitives Demo";
    config.window_width = 1280;
    config.window_height = 720;
    config.target_fps = 60;
    config.vsync = true;
    config.enable_hot_reload = false;
    config.debug_mode = true;

    void_runtime::Runtime runtime(config);

    auto init_result = runtime.initialize();
    if (!init_result) {
        spdlog::error("Failed to initialize runtime: {}", init_result.error().message());
        return EXIT_FAILURE;
    }

    log_package_counts(runtime);

    if (auto* composer = runtime.world_composer()) {
        spdlog::info("World state: {}", composer->current_world_name());
    }

    spdlog::info("Entering main loop...");
    int exit_code = runtime.run();

    runtime.shutdown();
    return exit_code;
}
