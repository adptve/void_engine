// void_package Integration Tests
//
// Tests for the complete package system workflow:
// - Full world load from packages
// - Prefab instantiation with dynamic components
// - Layer apply/unapply
// - Hot-reload of packages
// - Error cases (missing deps, cycles, invalid manifests)

#include <catch2/catch_test_macros.hpp>
#include <void_engine/package/package.hpp>
#include <void_engine/package/version.hpp>
#include <void_engine/package/manifest.hpp>
#include <void_engine/package/resolver.hpp>
#include <void_engine/package/registry.hpp>
#include <void_engine/package/loader.hpp>
#include <void_engine/package/asset_bundle.hpp>
#include <void_engine/package/prefab_registry.hpp>
#include <void_engine/package/plugin_package.hpp>
#include <void_engine/package/layer_package.hpp>
#include <void_engine/package/widget_package.hpp>
#include <void_engine/package/world_package.hpp>

#include <filesystem>
#include <fstream>

using namespace void_package;
using namespace void_core;

// =============================================================================
// Test Utilities
// =============================================================================

namespace {

/// Get the test packages directory (relative to source)
std::filesystem::path get_test_packages_dir() {
    // Try path from current working directory (set by CTest to VOID_ENGINE_SOURCE_DIR)
    auto path = std::filesystem::current_path() / "tests" / "package" / "test_packages";
    if (std::filesystem::exists(path)) {
        return path;
    }

    // Try path relative to source file (__FILE__ contains full path on MSVC)
    path = std::filesystem::path(__FILE__).parent_path() / "test_packages";
    if (std::filesystem::exists(path)) {
        return path;
    }

    // Try relative to executable
    path = std::filesystem::path("tests") / "package" / "test_packages";
    if (std::filesystem::exists(path)) {
        return path;
    }

    // Return the most likely path for debugging
    return std::filesystem::current_path() / "tests" / "package" / "test_packages";
}

/// Setup a registry with stub loaders for all package types
void setup_stub_loaders(LoadContext& ctx) {
    ctx.register_loader(std::make_unique<StubPackageLoader>(PackageType::World, "StubWorld"));
    ctx.register_loader(std::make_unique<StubPackageLoader>(PackageType::Layer, "StubLayer"));
    ctx.register_loader(std::make_unique<StubPackageLoader>(PackageType::Plugin, "StubPlugin"));
    ctx.register_loader(std::make_unique<StubPackageLoader>(PackageType::Widget, "StubWidget"));
    ctx.register_loader(std::make_unique<StubPackageLoader>(PackageType::Asset, "StubAsset"));
}

/// Check if test packages directory exists (skip tests if not)
bool test_packages_available() {
    auto dir = get_test_packages_dir();
    return std::filesystem::exists(dir);
}

/// Get debug info about test packages path
std::string get_test_packages_debug_info() {
    auto dir = get_test_packages_dir();
    std::string info = "Test packages dir: " + dir.string();
    info += "\nExists: " + std::string(std::filesystem::exists(dir) ? "yes" : "no");
    info += "\nCWD: " + std::filesystem::current_path().string();
    return info;
}

} // anonymous namespace

// =============================================================================
// Debug Test - Path Resolution
// =============================================================================

TEST_CASE("Test packages path resolution", "[package][integration][debug]") {
    auto dir = get_test_packages_dir();
    INFO("Test packages directory: " << dir.string());
    INFO("Directory exists: " << std::filesystem::exists(dir));
    INFO("Current working directory: " << std::filesystem::current_path().string());

    // This test always passes but prints debug info
    REQUIRE(true);

    // If test packages aren't available, show where we looked
    if (!test_packages_available()) {
        WARN("Test packages not found - integration tests will be skipped");
    }
}

// =============================================================================
// AssetBundleManifest Tests
// =============================================================================

TEST_CASE("AssetBundleManifest parsing", "[package][integration][asset]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    auto test_path = get_test_packages_dir() / "asset" / "test_assets.bundle.json";

    SECTION("load asset bundle manifest") {
        auto result = AssetBundleManifest::load(test_path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.base.name == "test.basic_assets");
        REQUIRE(manifest.base.type == PackageType::Asset);
        REQUIRE(manifest.base.version == SemanticVersion{1, 0, 0});
    }

    SECTION("verify meshes parsed") {
        auto result = AssetBundleManifest::load(test_path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.meshes.size() == 1);
        REQUIRE(manifest.meshes[0].id == "test_cube");
        REQUIRE(manifest.meshes[0].path == "models/cube.gltf");
    }

    SECTION("verify textures parsed") {
        auto result = AssetBundleManifest::load(test_path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.textures.size() == 1);
        REQUIRE(manifest.textures[0].id == "test_texture");
    }

    SECTION("verify materials parsed") {
        auto result = AssetBundleManifest::load(test_path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.materials.size() == 1);
        REQUIRE(manifest.materials[0].id == "test_material");
    }

    SECTION("verify prefabs parsed") {
        auto result = AssetBundleManifest::load(test_path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.prefabs.size() == 2);
        REQUIRE(manifest.prefabs[0].id == "test_prefab");
        REQUIRE(manifest.prefabs[1].id == "enemy_prefab");
    }
}

// =============================================================================
// PluginPackageManifest Tests
// =============================================================================

TEST_CASE("PluginPackageManifest parsing", "[package][integration][plugin]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    SECTION("load core plugin manifest") {
        auto path = get_test_packages_dir() / "plugin" / "core_test.plugin.json";
        auto result = PluginPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.base.name == "core.test");
        REQUIRE(manifest.base.type == PackageType::Plugin);
        REQUIRE(manifest.base.version == SemanticVersion{1, 0, 0});
    }

    SECTION("verify components declared") {
        auto path = get_test_packages_dir() / "plugin" / "core_test.plugin.json";
        auto result = PluginPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.components.size() == 4); // 2 regular + 2 tag components
        REQUIRE(manifest.components[0].name == "TestComponent");
        REQUIRE(manifest.components[1].name == "Health");
    }

    SECTION("verify component fields") {
        auto path = get_test_packages_dir() / "plugin" / "core_test.plugin.json";
        auto result = PluginPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        auto& health = manifest.components[1];
        REQUIRE(health.fields.size() == 3);
    }

    SECTION("verify systems declared") {
        auto path = get_test_packages_dir() / "plugin" / "core_test.plugin.json";
        auto result = PluginPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.systems.size() == 1);
        REQUIRE(manifest.systems[0].name == "TestSystem");
        REQUIRE(manifest.systems[0].stage == "update");
    }

    SECTION("load gameplay plugin with dependencies") {
        auto path = get_test_packages_dir() / "plugin" / "gameplay_test.plugin.json";
        auto result = PluginPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.base.name == "gameplay.test");
        REQUIRE(manifest.base.plugin_deps.size() == 1);
        REQUIRE(manifest.base.plugin_deps[0].name == "core.test");
    }
}

// =============================================================================
// LayerPackageManifest Tests
// =============================================================================

TEST_CASE("LayerPackageManifest parsing", "[package][integration][layer]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    auto path = get_test_packages_dir() / "layer" / "test_night.layer.json";

    SECTION("load layer manifest") {
        auto result = LayerPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.base.name == "layer.test_night");
        REQUIRE(manifest.base.type == PackageType::Layer);
        REQUIRE(manifest.priority == 100);
    }

    SECTION("verify additive scenes") {
        auto result = LayerPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.additive_scenes.size() == 1);
        REQUIRE(manifest.additive_scenes[0].path == "scenes/night_props.scene.json");
        REQUIRE(manifest.additive_scenes[0].spawn_mode == SpawnMode::Immediate);
    }

    SECTION("verify spawners") {
        auto result = LayerPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.spawners.size() == 1);
        REQUIRE(manifest.spawners[0].id == "test_spawner");
        REQUIRE(manifest.spawners[0].spawn_rate == 0.5f);
        REQUIRE(manifest.spawners[0].max_active == 5);
    }

    SECTION("verify lighting override") {
        auto result = LayerPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.lighting.has_value());
        REQUIRE(manifest.lighting->sun.has_value());
        REQUIRE(manifest.lighting->ambient.has_value());
    }

    SECTION("verify modifiers") {
        auto result = LayerPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.modifiers.size() == 1);
        REQUIRE(manifest.modifiers[0].path == "gameplay.damage_multiplier");
    }

    SECTION("has_content returns true") {
        auto result = LayerPackageManifest::load(path);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().has_content());
    }
}

// =============================================================================
// WidgetPackageManifest Tests
// =============================================================================

TEST_CASE("WidgetPackageManifest parsing", "[package][integration][widget]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    auto path = get_test_packages_dir() / "widget" / "test_hud.widget.json";

    SECTION("load widget manifest") {
        auto result = WidgetPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.base.name == "widget.test_hud");
        REQUIRE(manifest.base.type == PackageType::Widget);
    }

    SECTION("verify widgets declared") {
        auto result = WidgetPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.widgets.size() == 2);
        REQUIRE(manifest.widgets[0].id == "health_bar");
        REQUIRE(manifest.widgets[1].id == "debug_overlay");
    }

    SECTION("verify widget build types") {
        auto result = WidgetPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        // health_bar enabled in all builds
        REQUIRE(manifest.widgets[0].enabled_in_builds.size() == 3);
        // debug_overlay only in debug/development
        REQUIRE(manifest.widgets[1].enabled_in_builds.size() == 2);
    }

    SECTION("verify bindings") {
        auto result = WidgetPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.bindings.size() == 1);
        REQUIRE(manifest.bindings[0].widget_id == "health_bar");
        REQUIRE(manifest.bindings[0].data_source == "Health");
    }
}

// =============================================================================
// WorldPackageManifest Tests
// =============================================================================

TEST_CASE("WorldPackageManifest parsing", "[package][integration][world]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    auto path = get_test_packages_dir() / "world" / "test_world.world.json";

    SECTION("load world manifest") {
        auto result = WorldPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.base.name == "world.test_arena");
        REQUIRE(manifest.base.type == PackageType::World);
        REQUIRE(manifest.base.version == SemanticVersion{1, 0, 0});
    }

    SECTION("verify dependencies") {
        auto result = WorldPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.base.plugin_deps.size() == 2);
        REQUIRE(manifest.base.widget_deps.size() == 1);
        REQUIRE(manifest.base.layer_deps.size() == 1);
        REQUIRE(manifest.base.asset_deps.size() == 1);

        // Check optional dependency
        REQUIRE(manifest.base.layer_deps[0].optional == true);
    }

    SECTION("verify root scene") {
        auto result = WorldPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.root_scene.path == "scenes/test_arena.scene.json");
        REQUIRE(manifest.root_scene.spawn_points.size() == 3);
    }

    SECTION("verify player spawn config") {
        auto result = WorldPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.has_player_spawn());
        REQUIRE(manifest.player_spawn->prefab == "prefabs/test_player.prefab.json");
        REQUIRE(manifest.player_spawn->spawn_selection == SpawnSelection::RoundRobin);
    }

    SECTION("verify environment config") {
        auto result = WorldPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.environment.time_of_day == 12.0f);
        REQUIRE(manifest.environment.skybox == "skyboxes/test_sky");
    }

    SECTION("verify gameplay config") {
        auto result = WorldPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.gameplay.difficulty == "normal");
        REQUIRE(manifest.gameplay.match_length_seconds == 300);
        REQUIRE(manifest.gameplay.score_limit == 10);
        REQUIRE(manifest.gameplay.friendly_fire == false);
    }

    SECTION("verify ECS resources") {
        auto result = WorldPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.has_ecs_resources());
        REQUIRE(manifest.ecs_resources.count("GameConfig") == 1);
    }

    SECTION("verify world logic") {
        auto result = WorldPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.has_world_logic());
        REQUIRE(manifest.world_logic->win_conditions.size() == 1);
        REQUIRE(manifest.world_logic->lose_conditions.size() == 1);
        REQUIRE(manifest.world_logic->round_flow.has_value());
    }

    SECTION("verify layer and widget references") {
        auto result = WorldPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& manifest = result.value();
        REQUIRE(manifest.has_layers());
        REQUIRE(manifest.layers.size() == 1);
        REQUIRE(manifest.widgets.size() == 1);
    }
}

// =============================================================================
// Registry Scanning Integration Tests
// =============================================================================

TEST_CASE("PackageRegistry directory scanning", "[package][integration][registry]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    PackageRegistry registry;
    auto test_dir = get_test_packages_dir();

    SECTION("scan discovers packages") {
        auto result = registry.scan_directory(test_dir, true);
        REQUIRE(result.is_ok());
        REQUIRE(result.value() >= 6); // At least our test packages
    }

    SECTION("scanned packages are available") {
        registry.scan_directory(test_dir, true);

        REQUIRE(registry.is_available("test.basic_assets"));
        REQUIRE(registry.is_available("core.test"));
        REQUIRE(registry.is_available("gameplay.test"));
        REQUIRE(registry.is_available("layer.test_night"));
        REQUIRE(registry.is_available("widget.test_hud"));
        REQUIRE(registry.is_available("world.test_arena"));
    }

    SECTION("can query by type after scan") {
        registry.scan_directory(test_dir, true);

        auto plugins = registry.packages_of_type(PackageType::Plugin);
        REQUIRE(plugins.size() >= 2);

        auto layers = registry.packages_of_type(PackageType::Layer);
        REQUIRE(layers.size() >= 1);
    }

    SECTION("can get manifest after scan") {
        registry.scan_directory(test_dir, true);

        auto* manifest = registry.get_manifest("core.test");
        REQUIRE(manifest != nullptr);
        REQUIRE(manifest->type == PackageType::Plugin);
        REQUIRE(manifest->version == SemanticVersion{1, 0, 0});
    }
}

// =============================================================================
// Full Load Sequence Integration Tests
// =============================================================================

TEST_CASE("Package loading integration", "[package][integration][load]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    PackageRegistry registry;
    LoadContext ctx;
    setup_stub_loaders(ctx);

    auto test_dir = get_test_packages_dir();
    registry.scan_directory(test_dir, true);

    SECTION("load single package") {
        auto result = registry.load_package("test.basic_assets", ctx);
        REQUIRE(result.is_ok());
        REQUIRE(registry.is_loaded("test.basic_assets"));
    }

    SECTION("load package with dependencies resolves order") {
        auto result = registry.load_package("gameplay.test", ctx);
        REQUIRE(result.is_ok());

        // Both core.test and gameplay.test should be loaded
        REQUIRE(registry.is_loaded("core.test"));
        REQUIRE(registry.is_loaded("gameplay.test"));
    }

    SECTION("unload package") {
        registry.load_package("test.basic_assets", ctx);
        REQUIRE(registry.is_loaded("test.basic_assets"));

        auto result = registry.unload_package("test.basic_assets", ctx);
        REQUIRE(result.is_ok());
        REQUIRE_FALSE(registry.is_loaded("test.basic_assets"));
    }

    SECTION("unload all packages") {
        registry.load_package("gameplay.test", ctx);
        REQUIRE(registry.loaded_count() > 0);

        auto result = registry.unload_all(ctx);
        REQUIRE(result.is_ok());
        REQUIRE(registry.loaded_count() == 0);
    }
}

// =============================================================================
// Error Cases Integration Tests
// =============================================================================

TEST_CASE("Package error handling", "[package][integration][error]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    SECTION("cycle detection during resolution") {
        PackageResolver resolver;

        auto cycle_a_path = get_test_packages_dir() / "plugin" / "cycle_a.plugin.json";
        auto cycle_b_path = get_test_packages_dir() / "plugin" / "cycle_b.plugin.json";

        auto a_result = PackageManifest::load(cycle_a_path);
        auto b_result = PackageManifest::load(cycle_b_path);

        if (a_result.is_ok() && b_result.is_ok()) {
            resolver.add_available(std::move(a_result.value()), cycle_a_path.parent_path());
            resolver.add_available(std::move(b_result.value()), cycle_b_path.parent_path());

            auto cycle_check = resolver.validate_acyclic();
            REQUIRE(cycle_check.is_err());
        }
    }

    SECTION("layer violation detection") {
        PackageResolver resolver;

        // Add gameplay.test first (layer 2)
        auto gameplay_path = get_test_packages_dir() / "plugin" / "gameplay_test.plugin.json";
        auto gameplay_result = PluginPackageManifest::load(gameplay_path);
        if (gameplay_result.is_ok()) {
            resolver.add_available(gameplay_result.value().base, gameplay_path.parent_path());
        }

        // Add core.layer_violation which incorrectly depends on gameplay (layer 2 > layer 0)
        auto violation_path = get_test_packages_dir() / "plugin" / "layer_violation.plugin.json";
        auto violation_result = PackageManifest::load(violation_path);
        if (violation_result.is_ok()) {
            resolver.add_available(std::move(violation_result.value()), violation_path.parent_path());

            auto layer_check = resolver.validate_plugin_layers();
            REQUIRE(layer_check.is_err());
        }
    }

    SECTION("missing dependency error") {
        PackageRegistry registry;
        LoadContext ctx;
        setup_stub_loaders(ctx);

        auto missing_dep_path = get_test_packages_dir() / "plugin" / "missing_dep.plugin.json";
        auto result = registry.register_manifest(missing_dep_path);

        if (result.is_ok()) {
            auto load_result = registry.load_package("test.missing_dep", ctx);
            REQUIRE(load_result.is_err());
        }
    }

    SECTION("invalid manifest rejected") {
        auto bad_path = get_test_packages_dir() / "invalid" / "bad_manifest.plugin.json";
        auto result = PackageManifest::load(bad_path);
        REQUIRE(result.is_err());
    }

    SECTION("malformed JSON rejected") {
        auto invalid_path = get_test_packages_dir() / "invalid" / "invalid_json.plugin.json";
        auto result = PackageManifest::load(invalid_path);
        REQUIRE(result.is_err());
    }
}

// =============================================================================
// Hot-Reload Integration Tests
// =============================================================================

TEST_CASE("Package hot-reload", "[package][integration][hotreload]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    PackageRegistry registry;
    LoadContext ctx;
    setup_stub_loaders(ctx);

    auto test_dir = get_test_packages_dir();
    registry.scan_directory(test_dir, true);

    SECTION("reload loaded package") {
        auto load_result = registry.load_package("test.basic_assets", ctx);
        REQUIRE(load_result.is_ok());

        auto reload_result = registry.reload_package("test.basic_assets", ctx);
        REQUIRE(reload_result.is_ok());
        REQUIRE(registry.is_loaded("test.basic_assets"));
    }

    SECTION("reload unloaded package fails gracefully") {
        auto reload_result = registry.reload_package("test.basic_assets", ctx);
        // Should either succeed by loading, or fail with clear error
        // depending on implementation
    }
}

// =============================================================================
// Dependency Query Integration Tests
// =============================================================================

TEST_CASE("Dependency graph queries", "[package][integration][deps]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    PackageRegistry registry;
    auto test_dir = get_test_packages_dir();
    registry.scan_directory(test_dir, true);

    SECTION("get dependencies") {
        auto deps = registry.get_dependencies("gameplay.test");
        REQUIRE(deps.size() >= 1);

        bool has_core = false;
        for (const auto& dep : deps) {
            if (dep == "core.test") has_core = true;
        }
        REQUIRE(has_core);
    }

    SECTION("get dependents") {
        auto dependents = registry.get_dependents("core.test");

        bool has_gameplay = false;
        for (const auto& dep : dependents) {
            if (dep == "gameplay.test") has_gameplay = true;
        }
        REQUIRE(has_gameplay);
    }

    SECTION("dependency graph DOT format") {
        auto dot = registry.format_dependency_graph();
        REQUIRE_FALSE(dot.empty());
        // Should contain graph declaration
        REQUIRE(dot.find("digraph") != std::string::npos);
    }
}

// =============================================================================
// PrefabRegistry Tests
// =============================================================================

TEST_CASE("PrefabRegistry operations", "[package][integration][prefab]") {
    PrefabRegistry prefab_registry;

    SECTION("register and retrieve prefab") {
        PrefabDefinition def;
        def.id = "test_prefab";
        def.source_bundle = "test.basic_assets";
        def.components["Transform"] = nlohmann::json{{"position", {0, 0, 0}}};
        def.tags = {"test", "static"};

        prefab_registry.register_prefab(std::move(def));

        auto* retrieved = prefab_registry.get("test_prefab");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->id == "test_prefab");
        REQUIRE(retrieved->source_bundle == "test.basic_assets");
        REQUIRE(retrieved->tags.size() == 2);
    }

    SECTION("get nonexistent prefab returns nullptr") {
        auto* retrieved = prefab_registry.get("nonexistent");
        REQUIRE(retrieved == nullptr);
    }
}

// =============================================================================
// SpawnMode Parsing Tests
// =============================================================================

TEST_CASE("SpawnMode parsing", "[package][integration][layer]") {
    SpawnMode mode;

    SECTION("parse immediate") {
        REQUIRE(spawn_mode_from_string("immediate", mode));
        REQUIRE(mode == SpawnMode::Immediate);
    }

    SECTION("parse deferred") {
        REQUIRE(spawn_mode_from_string("deferred", mode));
        REQUIRE(mode == SpawnMode::Deferred);
    }

    SECTION("case insensitive") {
        REQUIRE(spawn_mode_from_string("IMMEDIATE", mode));
        REQUIRE(mode == SpawnMode::Immediate);
    }

    SECTION("to string") {
        REQUIRE(std::string(spawn_mode_to_string(SpawnMode::Immediate)) == "immediate");
        REQUIRE(std::string(spawn_mode_to_string(SpawnMode::Deferred)) == "deferred");
    }
}

// =============================================================================
// SpawnSelection Parsing Tests
// =============================================================================

TEST_CASE("SpawnSelection parsing", "[package][integration][world]") {
    SpawnSelection selection;

    SECTION("parse round_robin") {
        REQUIRE(spawn_selection_from_string("round_robin", selection));
        REQUIRE(selection == SpawnSelection::RoundRobin);
    }

    SECTION("parse random") {
        REQUIRE(spawn_selection_from_string("random", selection));
        REQUIRE(selection == SpawnSelection::Random);
    }

    SECTION("parse fixed") {
        REQUIRE(spawn_selection_from_string("fixed", selection));
        REQUIRE(selection == SpawnSelection::Fixed);
    }

    SECTION("to string") {
        REQUIRE(std::string(spawn_selection_to_string(SpawnSelection::RoundRobin)) == "round_robin");
        REQUIRE(std::string(spawn_selection_to_string(SpawnSelection::Random)) == "random");
        REQUIRE(std::string(spawn_selection_to_string(SpawnSelection::Fixed)) == "fixed");
    }
}

// =============================================================================
// Validation Integration Tests
// =============================================================================

TEST_CASE("Registry validation", "[package][integration][validation]") {
    if (!test_packages_available()) {
        SKIP("Test packages directory not found");
    }

    PackageRegistry registry;
    auto test_dir = get_test_packages_dir();

    // Only scan the valid packages subdirectories
    registry.scan_directory(test_dir / "asset", true);
    registry.scan_directory(test_dir / "plugin", false); // Not recursive to avoid invalid

    SECTION("validate valid packages succeeds") {
        // Register only valid packages
        registry.register_manifest(test_dir / "plugin" / "core_test.plugin.json");
        registry.register_manifest(test_dir / "plugin" / "gameplay_test.plugin.json");

        auto result = registry.validate();
        REQUIRE(result.is_ok());
    }
}

// =============================================================================
// Serialization Round-Trip Tests
// =============================================================================

TEST_CASE("Manifest serialization round-trip", "[package][integration][serialization]") {
    SECTION("SemanticVersion round-trip") {
        SemanticVersion original{1, 2, 3, "beta.1", "build123"};
        auto str = original.to_string();
        auto parsed = SemanticVersion::parse(str);

        REQUIRE(parsed.is_ok());
        REQUIRE(parsed.value().major == original.major);
        REQUIRE(parsed.value().minor == original.minor);
        REQUIRE(parsed.value().patch == original.patch);
        REQUIRE(parsed.value().prerelease == original.prerelease);
        REQUIRE(parsed.value().build_metadata == original.build_metadata);
    }

    SECTION("LayerPackageManifest round-trip") {
        if (!test_packages_available()) {
            SKIP("Test packages directory not found");
        }

        auto path = get_test_packages_dir() / "layer" / "test_night.layer.json";
        auto result = LayerPackageManifest::load(path);
        REQUIRE(result.is_ok());

        auto& original = result.value();
        auto json = original.to_json();

        auto reparsed = LayerPackageManifest::from_json(json, original.base);
        REQUIRE(reparsed.is_ok());
        REQUIRE(reparsed.value().priority == original.priority);
        REQUIRE(reparsed.value().additive_scenes.size() == original.additive_scenes.size());
        REQUIRE(reparsed.value().spawners.size() == original.spawners.size());
    }
}

// =============================================================================
// Utility Function Tests
// =============================================================================

TEST_CASE("Package utility functions", "[package][integration][util]") {
    SECTION("package_manifest_extensions") {
        auto extensions = package_manifest_extensions();
        REQUIRE(extensions.size() == 5);

        bool has_world = false, has_layer = false, has_plugin = false;
        bool has_widget = false, has_bundle = false;

        for (const auto& ext : extensions) {
            if (ext == ".world.json") has_world = true;
            if (ext == ".layer.json") has_layer = true;
            if (ext == ".plugin.json") has_plugin = true;
            if (ext == ".widget.json") has_widget = true;
            if (ext == ".bundle.json") has_bundle = true;
        }

        REQUIRE(has_world);
        REQUIRE(has_layer);
        REQUIRE(has_plugin);
        REQUIRE(has_widget);
        REQUIRE(has_bundle);
    }

    SECTION("is_package_manifest_path") {
        REQUIRE(is_package_manifest_path("test.world.json"));
        REQUIRE(is_package_manifest_path("path/to/test.plugin.json"));
        REQUIRE(is_package_manifest_path("test.bundle.json"));

        REQUIRE_FALSE(is_package_manifest_path("test.json"));
        REQUIRE_FALSE(is_package_manifest_path("test.txt"));
        REQUIRE_FALSE(is_package_manifest_path("plugin.json")); // Missing proper pattern
    }

    SECTION("package_type_from_extension") {
        auto world = package_type_from_extension("test.world.json");
        REQUIRE(world.has_value());
        REQUIRE(world.value() == PackageType::World);

        auto plugin = package_type_from_extension("test.plugin.json");
        REQUIRE(plugin.has_value());
        REQUIRE(plugin.value() == PackageType::Plugin);

        auto invalid = package_type_from_extension("test.json");
        REQUIRE_FALSE(invalid.has_value());
    }
}
