#pragma once

/// @file package.hpp
/// @brief Main include file for void_package module
///
/// This header includes all public package system headers.
/// For most use cases, including this single header is sufficient.
///
/// # Package System Overview
///
/// The void_engine package system organizes content into five types:
///
/// | Type | Purpose | Extension |
/// |------|---------|-----------|
/// | World | Composition root, game mode config | .world.json |
/// | Layer | Patches/variants on worlds | .layer.json |
/// | Plugin | Systems, components, logic | .plugin.json |
/// | Widget | UI, tools, overlays | .widget.json |
/// | Asset | Pure content (models, textures) | .bundle.json |
///
/// # Basic Usage
///
/// ```cpp
/// #include <void_engine/package/package.hpp>
///
/// using namespace void_package;
///
/// // Create registry and scan for packages
/// PackageRegistry registry;
/// registry.scan_directory("content/packages/");
///
/// // Create load context with engine systems
/// LoadContext ctx;
/// ctx.set_ecs_world(&ecs_world);
/// ctx.set_event_bus(&event_bus);
///
/// // Register loaders for each package type
/// ctx.register_loader(std::make_unique<AssetBundleLoader>());
/// ctx.register_loader(std::make_unique<PluginPackageLoader>());
/// // ... etc
///
/// // Load a world package (and all dependencies)
/// auto result = registry.load_package("arena_dm", ctx);
/// if (!result) {
///     log_error("Failed to load: {}", result.error().message());
/// }
/// ```
///
/// # Dependency Rules
///
/// Packages can depend on other packages following these rules:
/// - world → layer, plugin, widget, asset
/// - layer → plugin, widget, asset
/// - plugin → plugin (lower layer only), asset
/// - widget → plugin, asset
/// - asset → asset (prefer none)
///
/// Plugin layers (dependencies flow downward only):
/// - core.* (0) → Foundation
/// - engine.* (1) → Engine-level
/// - gameplay.* (2) → Gameplay systems
/// - feature.* (3) → Specific features
/// - mod.* (4) → Mods/creator content
///
/// # Version Constraints
///
/// Dependencies can specify version constraints:
/// - `"1.2.3"` - Exact version
/// - `">=1.0.0"` - Minimum version
/// - `"^1.2.3"` - Compatible (same major, >= specified)
/// - `"~1.2.3"` - Approximately (same major.minor, >= specified)
/// - `">=1.0.0,<2.0.0"` - Range

#include "fwd.hpp"
#include "version.hpp"
#include "manifest.hpp"
#include "resolver.hpp"
#include "loader.hpp"
#include "registry.hpp"

// Phase 2: Asset bundles
#include "asset_bundle.hpp"
#include "prefab_registry.hpp"
#include "definition_registry.hpp"
#include "component_schema.hpp"

// Phase 3: Plugins
#include "plugin_package.hpp"
#include "dynamic_library.hpp"

// Phase 4: Widgets
#include "widget_package.hpp"
#include "widget_manager.hpp"
#include "widget.hpp"

// Phase 5: Layers
#include "layer_package.hpp"
#include "layer_applier.hpp"

// Phase 6: Worlds
#include "world_package.hpp"
#include "world_composer.hpp"
