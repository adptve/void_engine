/// @file engine_module.hpp
/// @brief Main include header for void_engine
///
/// void_engine provides the high-level application framework:
/// - Engine facade for orchestrating all systems
/// - Application interface for game code
/// - Lifecycle management with hooks
/// - Configuration system with hot-reload
/// - Time management
///
/// ## Quick Start
///
/// ### Simple Application
/// ```cpp
/// #include <void_engine/engine/engine_module.hpp>
///
/// int main() {
///     return void_engine::run_app("MyGame",
///         // Init
///         [](void_engine::Engine& engine) {
///             // Initialize game state
///             return void_core::Ok();
///         },
///         // Update
///         [](void_engine::Engine& engine, float dt) {
///             // Update game logic
///         },
///         // Render
///         [](void_engine::Engine& engine) {
///             // Render frame
///         },
///         // Shutdown
///         [](void_engine::Engine& engine) {
///             // Cleanup
///         }
///     );
/// }
/// ```
///
/// ### Custom Application Class
/// ```cpp
/// class MyGame : public void_engine::AppBase {
/// public:
///     MyGame() : AppBase(AppConfig::from_name("MyGame")) {}
///
///     void_core::Result<void> on_init(Engine& engine) override {
///         // Initialize
///         return void_core::Ok();
///     }
///
///     void on_update(Engine& engine, float dt) override {
///         // Update
///     }
///
///     void on_render(Engine& engine) override {
///         // Render
///     }
///
///     void on_shutdown(Engine& engine) override {
///         // Cleanup
///     }
/// };
///
/// int main() {
///     auto engine = void_engine::EngineBuilder()
///         .name("MyGame")
///         .window_size(1920, 1080)
///         .features(void_engine::EngineFeature::Game)
///         .build_with_app(std::make_unique<MyGame>());
///
///     if (!engine) {
///         return 1;
///     }
///
///     engine.value()->run();
///     return 0;
/// }
/// ```
///
/// ### Engine Builder
/// ```cpp
/// auto engine = void_engine::EngineBuilder()
///     .name("MyGame")
///     .version("1.0.0")
///     .organization("MyStudio")
///     .window_title("My Awesome Game")
///     .window_size(1920, 1080)
///     .window_mode(void_engine::WindowMode::Borderless)
///     .vsync(true)
///     .graphics_backend(void_engine::GraphicsBackend::Vulkan)
///     .anti_aliasing(void_engine::AntiAliasing::TAA)
///     .target_fps(144)
///     .features(void_engine::EngineFeature::Game | void_engine::EngineFeature::HotReload)
///     .debug(true)
///     .build();
/// ```
///
/// ### Lifecycle Hooks
/// ```cpp
/// engine->lifecycle().on_init("my_hook", [](Engine& engine) {
///     // Called during initialization
///     return void_core::Ok();
/// });
///
/// engine->lifecycle().on_shutdown("my_hook", [](Engine& engine) {
///     // Called during shutdown
///     return void_core::Ok();
/// });
/// ```
///
/// ### Configuration
/// ```cpp
/// auto& config = engine->config_manager();
///
/// // Load from file
/// config.load_json("config.json", "user");
///
/// // Get values
/// auto width = config.get_int("window.width", 1920);
/// auto fullscreen = config.get_bool("window.fullscreen", false);
///
/// // Set values
/// config.set_int("window.width", 2560, "user");
///
/// // Save to file
/// config.save_json("config.json", "user");
/// ```

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "app.hpp"
#include "lifecycle.hpp"
#include "config.hpp"
#include "engine.hpp"

namespace void_engine {

/// Prelude - commonly used types
namespace prelude {
    using void_engine::Engine;
    using void_engine::EngineBuilder;
    using void_engine::EngineConfig;
    using void_engine::EngineState;
    using void_engine::EngineFeature;
    using void_engine::EngineStats;
    using void_engine::IApp;
    using void_engine::AppBase;
    using void_engine::AppConfig;
    using void_engine::SimpleApp;
    using void_engine::LifecycleManager;
    using void_engine::LifecyclePhase;
    using void_engine::LifecycleHook;
    using void_engine::ConfigManager;
    using void_engine::ConfigLayer;
    using void_engine::TimeState;
    using void_engine::WindowConfig;
    using void_engine::WindowMode;
    using void_engine::RenderConfig;
    using void_engine::GraphicsBackend;
    using void_engine::AntiAliasing;
    using void_engine::AudioConfig;
    using void_engine::InputConfig;
    using void_engine::AssetConfig;
} // namespace prelude

} // namespace void_engine
