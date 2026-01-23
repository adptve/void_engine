#pragma once

/// @file asset.hpp
/// @brief Main include file for void_asset module
///
/// void_asset provides a complete asset management system with:
/// - Type-safe asset handles with reference counting
/// - Extensible loader system for custom asset types
/// - Asynchronous loading with progress tracking
/// - Hot-reload support for live content updates
/// - Event system for asset state changes
/// - Garbage collection for unused assets
///
/// @section usage Basic Usage
/// @code
/// #include <void_engine/asset/asset.hpp>
///
/// using namespace void_asset;
///
/// // Create asset server
/// AssetServer server(AssetServerConfig()
///     .with_asset_dir("assets")
///     .with_hot_reload(true));
///
/// // Register custom loader
/// server.register_loader(std::make_unique<MyTextureLoader>());
///
/// // Load asset (returns immediately with handle)
/// auto texture = server.load<Texture>("textures/player.png");
///
/// // Process loads in game loop
/// while (running) {
///     server.process();
///
///     // Check if loaded
///     if (texture.is_loaded()) {
///         render(*texture);
///     }
///
///     // Handle events
///     for (auto& event : server.drain_events()) {
///         if (event.type == AssetEventType::Loaded) {
///             std::cout << "Loaded: " << event.path.str() << "\n";
///         }
///     }
/// }
/// @endcode
///
/// @section custom_loader Custom Asset Loaders
/// @code
/// struct Texture {
///     int width, height;
///     std::vector<uint8_t> pixels;
/// };
///
/// class TextureLoader : public AssetLoader<Texture> {
/// public:
///     std::vector<std::string> extensions() const override {
///         return {"png", "jpg", "bmp"};
///     }
///
///     LoadResult<Texture> load(LoadContext& ctx) override {
///         auto texture = std::make_unique<Texture>();
///         // Parse ctx.data() into texture...
///         return void_core::Ok(std::move(texture));
///     }
/// };
/// @endcode
///
/// @section hot_reload Hot Reload
/// @code
/// // Use the combined system for hot-reload
/// AssetHotReloadSystem system(
///     AssetServerConfig().with_asset_dir("assets"),
///     AssetHotReloadConfig().with_enabled(true)
/// );
///
/// system.start();
///
/// auto texture = system.load<Texture>("player.png");
///
/// while (running) {
///     system.process();  // Handles loading and hot-reload
///
///     // Asset will automatically update when file changes
///     if (texture.is_loaded()) {
///         render(*texture);
///     }
/// }
///
/// system.stop();
/// @endcode
///
/// @section dependencies Asset Dependencies
/// @code
/// class MaterialLoader : public AssetLoader<Material> {
/// public:
///     LoadResult<Material> load(LoadContext& ctx) override {
///         auto material = std::make_unique<Material>();
///
///         // Register dependencies
///         ctx.add_dependency(AssetPath("shaders/default.vert"));
///         ctx.add_dependency(AssetPath("shaders/default.frag"));
///
///         return void_core::Ok(std::move(material));
///     }
/// };
/// @endcode
///
/// @section garbage_collection Garbage Collection
/// @code
/// // Handles go out of scope...
/// {
///     auto texture = server.load<Texture>("unused.png");
/// }  // Handle destroyed
///
/// // Later, collect unreferenced assets
/// std::size_t collected = server.collect_garbage();
/// std::cout << "Collected " << collected << " assets\n";
/// @endcode

#include "fwd.hpp"
#include "types.hpp"
#include "handle.hpp"
#include "loader.hpp"
#include "storage.hpp"
#include "server.hpp"
#include "hot_reload.hpp"

namespace void_asset {

/// @brief Module version
constexpr const char* VOID_ASSET_VERSION = "0.1.0";

/// @brief Check if module is available
constexpr bool VOID_ASSET_AVAILABLE = true;

} // namespace void_asset
