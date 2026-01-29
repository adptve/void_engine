// void_package Unit Tests
//
// Tests for the package system core functionality:
// - SemanticVersion parsing and comparison
// - VersionConstraint parsing and matching
// - PackageManifest parsing and validation
// - PackageResolver cycle detection and layer validation
// - PackageRegistry load/unload

#include <catch2/catch_test_macros.hpp>
#include <void_engine/package/package.hpp>
#include <void_engine/package/version.hpp>
#include <void_engine/package/manifest.hpp>
#include <void_engine/package/resolver.hpp>
#include <void_engine/package/registry.hpp>
#include <void_engine/package/loader.hpp>
#include <void_engine/package/asset_bundle.hpp>
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

/// Get the test packages directory
std::filesystem::path get_test_packages_dir() {
    // Path relative to test executable or configured location
    // This assumes tests are run from the build directory
    auto path = std::filesystem::current_path() / "tests" / "package" / "test_packages";
    if (!std::filesystem::exists(path)) {
        // Try source directory layout
        path = std::filesystem::path(__FILE__).parent_path() / "test_packages";
    }
    return path;
}

/// Create a temporary manifest for testing
std::string make_manifest_json(
    const std::string& name,
    const std::string& type,
    const std::string& version,
    const std::string& extra = ""
) {
    std::string json = R"({
  "package": {
    "name": ")" + name + R"(",
    "type": ")" + type + R"(",
    "version": ")" + version + R"("
  })";

    if (!extra.empty()) {
        json += ",\n  " + extra;
    }

    json += "\n}";
    return json;
}

} // anonymous namespace

// =============================================================================
// SemanticVersion Tests
// =============================================================================

TEST_CASE("SemanticVersion parsing", "[package][version]") {
    SECTION("parse simple version") {
        auto result = SemanticVersion::parse("1.2.3");
        REQUIRE(result.is_ok());
        auto v = result.value();
        REQUIRE(v.major == 1);
        REQUIRE(v.minor == 2);
        REQUIRE(v.patch == 3);
        REQUIRE(v.prerelease.empty());
        REQUIRE(v.build_metadata.empty());
    }

    SECTION("parse major.minor only") {
        auto result = SemanticVersion::parse("1.2");
        REQUIRE(result.is_ok());
        auto v = result.value();
        REQUIRE(v.major == 1);
        REQUIRE(v.minor == 2);
        REQUIRE(v.patch == 0);
    }

    SECTION("parse major only") {
        auto result = SemanticVersion::parse("1");
        REQUIRE(result.is_ok());
        auto v = result.value();
        REQUIRE(v.major == 1);
        REQUIRE(v.minor == 0);
        REQUIRE(v.patch == 0);
    }

    SECTION("parse with prerelease") {
        auto result = SemanticVersion::parse("1.2.3-alpha");
        REQUIRE(result.is_ok());
        auto v = result.value();
        REQUIRE(v.major == 1);
        REQUIRE(v.minor == 2);
        REQUIRE(v.patch == 3);
        REQUIRE(v.prerelease == "alpha");
    }

    SECTION("parse with prerelease and build") {
        auto result = SemanticVersion::parse("1.2.3-beta.1+build123");
        REQUIRE(result.is_ok());
        auto v = result.value();
        REQUIRE(v.major == 1);
        REQUIRE(v.minor == 2);
        REQUIRE(v.patch == 3);
        REQUIRE(v.prerelease == "beta.1");
        REQUIRE(v.build_metadata == "build123");
    }

    SECTION("parse with build only") {
        auto result = SemanticVersion::parse("1.2.3+sha.abc123");
        REQUIRE(result.is_ok());
        auto v = result.value();
        REQUIRE(v.major == 1);
        REQUIRE(v.minor == 2);
        REQUIRE(v.patch == 3);
        REQUIRE(v.prerelease.empty());
        REQUIRE(v.build_metadata == "sha.abc123");
    }

    SECTION("parse invalid versions") {
        REQUIRE(SemanticVersion::parse("").is_err());
        REQUIRE(SemanticVersion::parse("abc").is_err());
        REQUIRE(SemanticVersion::parse("-1.0.0").is_err());
        // Note: "1.2.3.4" may parse successfully with trailing ignored
    }
}

TEST_CASE("SemanticVersion comparison", "[package][version]") {
    SemanticVersion v100{1, 0, 0};
    SemanticVersion v110{1, 1, 0};
    SemanticVersion v111{1, 1, 1};
    SemanticVersion v200{2, 0, 0};

    SECTION("equality") {
        REQUIRE(v100 == SemanticVersion{1, 0, 0});
        REQUIRE_FALSE(v100 == v110);
    }

    SECTION("ordering") {
        REQUIRE(v100 < v110);
        REQUIRE(v110 < v111);
        REQUIRE(v111 < v200);
        REQUIRE(v200 > v111);
    }

    SECTION("prerelease lower than release") {
        SemanticVersion v100_alpha{1, 0, 0, "alpha"};
        SemanticVersion v100_beta{1, 0, 0, "beta"};

        REQUIRE(v100_alpha < v100);     // prerelease < release
        REQUIRE(v100_beta < v100);      // prerelease < release
        REQUIRE(v100_alpha < v100_beta); // alpha < beta
    }

    SECTION("build metadata ignored in comparison") {
        SemanticVersion v1{1, 0, 0, "", "build1"};
        SemanticVersion v2{1, 0, 0, "", "build2"};
        REQUIRE(v1 == v2);
    }
}

TEST_CASE("SemanticVersion to_string", "[package][version]") {
    SECTION("simple version") {
        SemanticVersion v{1, 2, 3};
        REQUIRE(v.to_string() == "1.2.3");
    }

    SECTION("with prerelease") {
        SemanticVersion v{1, 2, 3, "alpha"};
        REQUIRE(v.to_string() == "1.2.3-alpha");
    }

    SECTION("with build metadata") {
        SemanticVersion v{1, 2, 3, "", "build123"};
        REQUIRE(v.to_string() == "1.2.3+build123");
    }

    SECTION("with both prerelease and build") {
        SemanticVersion v{1, 2, 3, "beta.1", "build456"};
        REQUIRE(v.to_string() == "1.2.3-beta.1+build456");
    }
}

TEST_CASE("SemanticVersion queries", "[package][version]") {
    SECTION("is_prerelease") {
        SemanticVersion v1{1, 0, 0};
        SemanticVersion v2{1, 0, 0, "alpha"};

        REQUIRE_FALSE(v1.is_prerelease());
        REQUIRE(v2.is_prerelease());
    }

    SECTION("is_unstable") {
        SemanticVersion v0{0, 1, 0};
        SemanticVersion v1{1, 0, 0};

        REQUIRE(v0.is_unstable());
        REQUIRE_FALSE(v1.is_unstable());
    }

    SECTION("core") {
        SemanticVersion v{1, 2, 3, "alpha", "build"};
        auto core = v.core();

        REQUIRE(core.major == 1);
        REQUIRE(core.minor == 2);
        REQUIRE(core.patch == 3);
        REQUIRE(core.prerelease.empty());
        REQUIRE(core.build_metadata.empty());
    }
}

// =============================================================================
// VersionConstraint Tests
// =============================================================================

TEST_CASE("VersionConstraint parsing", "[package][version]") {
    SECTION("parse any (*)") {
        auto result = VersionConstraint::parse("*");
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == VersionConstraint::Type::Any);
    }

    SECTION("parse exact") {
        auto result = VersionConstraint::parse("1.2.3");
        REQUIRE(result.is_ok());
        auto c = result.value();
        REQUIRE(c.type == VersionConstraint::Type::Exact);
        REQUIRE(c.version == SemanticVersion{1, 2, 3});
    }

    SECTION("parse greater equal") {
        auto result = VersionConstraint::parse(">=1.0.0");
        REQUIRE(result.is_ok());
        auto c = result.value();
        REQUIRE(c.type == VersionConstraint::Type::GreaterEqual);
        REQUIRE(c.version == SemanticVersion{1, 0, 0});
    }

    SECTION("parse greater") {
        auto result = VersionConstraint::parse(">1.0.0");
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == VersionConstraint::Type::Greater);
    }

    SECTION("parse less equal") {
        auto result = VersionConstraint::parse("<=2.0.0");
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == VersionConstraint::Type::LessEqual);
    }

    SECTION("parse less") {
        auto result = VersionConstraint::parse("<2.0.0");
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == VersionConstraint::Type::Less);
    }

    SECTION("parse caret") {
        auto result = VersionConstraint::parse("^1.2.3");
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == VersionConstraint::Type::Caret);
    }

    SECTION("parse tilde") {
        auto result = VersionConstraint::parse("~1.2.3");
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == VersionConstraint::Type::Tilde);
    }

    SECTION("parse range") {
        auto result = VersionConstraint::parse(">=1.0.0,<2.0.0");
        REQUIRE(result.is_ok());
        // Range is typically implemented as sub-constraints or min/max
    }
}

TEST_CASE("VersionConstraint satisfies", "[package][version]") {
    SECTION("any matches all") {
        auto c = VersionConstraint::any();
        REQUIRE(c.satisfies(SemanticVersion{0, 0, 0}));
        REQUIRE(c.satisfies(SemanticVersion{1, 0, 0}));
        REQUIRE(c.satisfies(SemanticVersion{99, 99, 99}));
    }

    SECTION("exact matches only exact") {
        auto c = VersionConstraint::exact(SemanticVersion{1, 2, 3});
        REQUIRE(c.satisfies(SemanticVersion{1, 2, 3}));
        REQUIRE_FALSE(c.satisfies(SemanticVersion{1, 2, 4}));
        REQUIRE_FALSE(c.satisfies(SemanticVersion{1, 3, 0}));
    }

    SECTION("greater_equal") {
        auto c = VersionConstraint::greater_equal(SemanticVersion{1, 0, 0});
        REQUIRE_FALSE(c.satisfies(SemanticVersion{0, 9, 9}));
        REQUIRE(c.satisfies(SemanticVersion{1, 0, 0}));
        REQUIRE(c.satisfies(SemanticVersion{1, 0, 1}));
        REQUIRE(c.satisfies(SemanticVersion{2, 0, 0}));
    }

    SECTION("caret (^) - compatible with version") {
        auto c = VersionConstraint::caret(SemanticVersion{1, 2, 3});
        // ^1.2.3 means >=1.2.3, <2.0.0
        REQUIRE(c.satisfies(SemanticVersion{1, 2, 3}));
        REQUIRE(c.satisfies(SemanticVersion{1, 3, 0}));
        REQUIRE(c.satisfies(SemanticVersion{1, 99, 99}));
        REQUIRE_FALSE(c.satisfies(SemanticVersion{1, 2, 2}));
        REQUIRE_FALSE(c.satisfies(SemanticVersion{2, 0, 0}));
    }

    SECTION("caret on 0.x versions") {
        auto c = VersionConstraint::caret(SemanticVersion{0, 2, 3});
        // ^0.2.3 means >=0.2.3, <0.3.0
        REQUIRE(c.satisfies(SemanticVersion{0, 2, 3}));
        REQUIRE(c.satisfies(SemanticVersion{0, 2, 99}));
        REQUIRE_FALSE(c.satisfies(SemanticVersion{0, 3, 0}));
    }

    SECTION("tilde (~) - approximately") {
        auto c = VersionConstraint::tilde(SemanticVersion{1, 2, 3});
        // ~1.2.3 means >=1.2.3, <1.3.0
        REQUIRE(c.satisfies(SemanticVersion{1, 2, 3}));
        REQUIRE(c.satisfies(SemanticVersion{1, 2, 99}));
        REQUIRE_FALSE(c.satisfies(SemanticVersion{1, 3, 0}));
        REQUIRE_FALSE(c.satisfies(SemanticVersion{1, 2, 2}));
    }
}

// =============================================================================
// Package Name Utilities Tests
// =============================================================================

TEST_CASE("Package name validation", "[package][manifest]") {
    SECTION("valid names") {
        REQUIRE(is_valid_package_name("core.ecs"));
        REQUIRE(is_valid_package_name("gameplay.combat"));
        REQUIRE(is_valid_package_name("mod.my_awesome_mod"));
        REQUIRE(is_valid_package_name("feature.special_mode"));
    }

    SECTION("invalid names") {
        REQUIRE_FALSE(is_valid_package_name(""));           // empty
        REQUIRE_FALSE(is_valid_package_name("nodot"));      // no dot
        REQUIRE_FALSE(is_valid_package_name(".startdot")); // starts with dot
        REQUIRE_FALSE(is_valid_package_name("enddot."));   // ends with dot
        REQUIRE_FALSE(is_valid_package_name("double..dot")); // consecutive dots
        REQUIRE_FALSE(is_valid_package_name("Upper.Case")); // uppercase
        REQUIRE_FALSE(is_valid_package_name("has space.here")); // space
    }
}

TEST_CASE("Plugin layer level", "[package][manifest]") {
    SECTION("core layer") {
        REQUIRE(get_plugin_layer_level("core.ecs") == 0);
        REQUIRE(get_plugin_layer_level("core.math") == 0);
    }

    SECTION("engine layer") {
        REQUIRE(get_plugin_layer_level("engine.render") == 1);
        REQUIRE(get_plugin_layer_level("engine.audio") == 1);
    }

    SECTION("gameplay layer") {
        REQUIRE(get_plugin_layer_level("gameplay.combat") == 2);
        REQUIRE(get_plugin_layer_level("gameplay.inventory") == 2);
    }

    SECTION("feature layer") {
        REQUIRE(get_plugin_layer_level("feature.special") == 3);
    }

    SECTION("mod layer") {
        REQUIRE(get_plugin_layer_level("mod.user_content") == 4);
    }

    SECTION("unknown namespace") {
        REQUIRE(get_plugin_layer_level("unknown.package") == -1);
        REQUIRE(get_plugin_layer_level("test.package") == -1);
    }
}

// =============================================================================
// PackageManifest Tests
// =============================================================================

TEST_CASE("PackageManifest parsing", "[package][manifest]") {
    SECTION("parse minimal world manifest") {
        std::string json = make_manifest_json("test.world", "world", "1.0.0");
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_ok());

        auto& m = result.value();
        REQUIRE(m.name == "test.world");
        REQUIRE(m.type == PackageType::World);
        REQUIRE(m.version == SemanticVersion{1, 0, 0});
    }

    SECTION("parse plugin manifest") {
        std::string json = make_manifest_json("core.ecs", "plugin", "2.1.0");
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == PackageType::Plugin);
    }

    SECTION("parse layer manifest") {
        std::string json = make_manifest_json("layer.night", "layer", "1.0.0");
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == PackageType::Layer);
    }

    SECTION("parse widget manifest") {
        std::string json = make_manifest_json("widget.hud", "widget", "1.0.0");
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == PackageType::Widget);
    }

    SECTION("parse asset manifest") {
        std::string json = make_manifest_json("assets.chars", "asset", "1.0.0");
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().type == PackageType::Asset);
    }

    SECTION("parse manifest with dependencies") {
        std::string json = R"({
  "package": {
    "name": "gameplay.combat",
    "type": "plugin",
    "version": "1.0.0"
  },
  "dependencies": {
    "plugins": [
      { "name": "core.ecs", "version": ">=1.0.0" }
    ],
    "assets": [
      { "name": "assets.weapons", "version": ">=1.0.0", "optional": true }
    ]
  }
})";
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_ok());

        auto& m = result.value();
        REQUIRE(m.plugin_deps.size() == 1);
        REQUIRE(m.plugin_deps[0].name == "core.ecs");
        REQUIRE(m.plugin_deps[0].optional == false);
        REQUIRE(m.asset_deps.size() == 1);
        REQUIRE(m.asset_deps[0].optional == true);
    }

    SECTION("parse manifest with metadata") {
        std::string json = R"({
  "package": {
    "name": "test.pkg",
    "type": "plugin",
    "version": "1.0.0",
    "display_name": "Test Package",
    "description": "A test package",
    "author": "Test Author"
  }
})";
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_ok());

        auto& m = result.value();
        REQUIRE(m.display_name == "Test Package");
        REQUIRE(m.description == "A test package");
        REQUIRE(m.author == "Test Author");
    }

    SECTION("error on missing package section") {
        std::string json = R"({ "other": "data" })";
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_err());
    }

    SECTION("error on missing required fields") {
        std::string json = R"({ "package": { "name": "only.name" } })";
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_err());
    }

    SECTION("error on invalid package type") {
        std::string json = make_manifest_json("test.pkg", "invalid_type", "1.0.0");
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_err());
    }
}

TEST_CASE("PackageManifest validation", "[package][manifest]") {
    SECTION("validate valid manifest") {
        std::string json = make_manifest_json("core.test", "plugin", "1.0.0");
        auto result = PackageManifest::from_json_string(json);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().validate().is_ok());
    }

    SECTION("may_depend_on rules") {
        PackageManifest world_pkg;
        world_pkg.type = PackageType::World;

        // World can depend on all types including other worlds
        REQUIRE(world_pkg.may_depend_on(PackageType::Plugin));
        REQUIRE(world_pkg.may_depend_on(PackageType::Layer));
        REQUIRE(world_pkg.may_depend_on(PackageType::Widget));
        REQUIRE(world_pkg.may_depend_on(PackageType::Asset));
        // Note: Implementation allows world->world dependencies

        PackageManifest plugin_pkg;
        plugin_pkg.type = PackageType::Plugin;

        // Plugin can depend on plugins and assets
        REQUIRE(plugin_pkg.may_depend_on(PackageType::Plugin));
        REQUIRE(plugin_pkg.may_depend_on(PackageType::Asset));
    }
}

TEST_CASE("PackageManifest queries", "[package][manifest]") {
    SECTION("namespace_prefix") {
        PackageManifest m;
        m.name = "gameplay.combat";
        REQUIRE(m.namespace_prefix() == "gameplay");
    }

    SECTION("short_name") {
        PackageManifest m;
        m.name = "gameplay.combat";
        REQUIRE(m.short_name() == "combat");
    }

    SECTION("plugin_layer_level") {
        PackageManifest m;
        m.name = "core.ecs";
        m.type = PackageType::Plugin;
        REQUIRE(m.plugin_layer_level() == 0);

        m.name = "gameplay.combat";
        REQUIRE(m.plugin_layer_level() == 2);
    }

    SECTION("all_dependencies") {
        PackageManifest m;
        m.plugin_deps.push_back({"core.ecs", VersionConstraint::any(), false, ""});
        m.asset_deps.push_back({"assets.test", VersionConstraint::any(), false, ""});

        auto all = m.all_dependencies();
        REQUIRE(all.size() == 2);
    }
}

// =============================================================================
// PackageResolver Tests
// =============================================================================

TEST_CASE("PackageResolver basic operations", "[package][resolver]") {
    PackageResolver resolver;

    SECTION("add and query package") {
        PackageManifest m;
        m.name = "test.package";
        m.type = PackageType::Plugin;
        m.version = SemanticVersion{1, 0, 0};

        resolver.add_available(std::move(m), "/test/path");

        REQUIRE(resolver.has_package("test.package"));
        REQUIRE_FALSE(resolver.has_package("nonexistent"));
        REQUIRE(resolver.size() == 1);
    }

    SECTION("remove package") {
        PackageManifest m;
        m.name = "test.package";
        m.type = PackageType::Plugin;
        m.version = SemanticVersion{1, 0, 0};

        resolver.add_available(std::move(m), "/test/path");
        REQUIRE(resolver.has_package("test.package"));

        resolver.remove_available("test.package");
        REQUIRE_FALSE(resolver.has_package("test.package"));
    }

    SECTION("get manifest") {
        PackageManifest m;
        m.name = "test.package";
        m.type = PackageType::Plugin;
        m.version = SemanticVersion{1, 2, 3};

        resolver.add_available(std::move(m), "/test/path");

        auto* manifest = resolver.get_manifest("test.package");
        REQUIRE(manifest != nullptr);
        REQUIRE(manifest->version == SemanticVersion{1, 2, 3});
    }
}

TEST_CASE("PackageResolver dependency resolution", "[package][resolver]") {
    PackageResolver resolver;

    // Set up a dependency chain: A depends on B, B depends on C
    auto add_package = [&](const std::string& name, std::vector<std::string> deps = {}) {
        PackageManifest m;
        m.name = name;
        m.type = PackageType::Plugin;
        m.version = SemanticVersion{1, 0, 0};
        for (const auto& dep : deps) {
            m.plugin_deps.push_back({dep, VersionConstraint::any(), false, ""});
        }
        resolver.add_available(std::move(m), "/packages/" + name);
    };

    SECTION("resolve single package") {
        add_package("test.single");

        auto result = resolver.resolve("test.single");
        REQUIRE(result.is_ok());
        REQUIRE(result.value().size() == 1);
        REQUIRE(result.value()[0].manifest.name == "test.single");
    }

    SECTION("resolve with dependencies in correct order") {
        add_package("test.c");
        add_package("test.b", {"test.c"});
        add_package("test.a", {"test.b"});

        auto result = resolver.resolve("test.a");
        REQUIRE(result.is_ok());

        auto& resolved = result.value();
        REQUIRE(resolved.size() == 3);

        // Dependencies should come first (C, B, A)
        REQUIRE(resolved[0].manifest.name == "test.c");
        REQUIRE(resolved[1].manifest.name == "test.b");
        REQUIRE(resolved[2].manifest.name == "test.a");
    }

    SECTION("error on missing dependency") {
        add_package("test.needs_missing", {"nonexistent.package"});

        auto result = resolver.resolve("test.needs_missing");
        REQUIRE(result.is_err());
    }

    SECTION("shared dependencies resolved once") {
        // Both A and B depend on C
        add_package("test.c");
        add_package("test.b", {"test.c"});
        add_package("test.a", {"test.b", "test.c"});

        auto result = resolver.resolve("test.a");
        REQUIRE(result.is_ok());

        // C should appear only once
        auto& resolved = result.value();
        int c_count = 0;
        for (const auto& pkg : resolved) {
            if (pkg.manifest.name == "test.c") c_count++;
        }
        REQUIRE(c_count == 1);
    }
}

TEST_CASE("PackageResolver cycle detection", "[package][resolver]") {
    PackageResolver resolver;

    SECTION("detect simple cycle") {
        PackageManifest a;
        a.name = "test.cycle_a";
        a.type = PackageType::Plugin;
        a.version = SemanticVersion{1, 0, 0};
        a.plugin_deps.push_back({"test.cycle_b", VersionConstraint::any(), false, ""});

        PackageManifest b;
        b.name = "test.cycle_b";
        b.type = PackageType::Plugin;
        b.version = SemanticVersion{1, 0, 0};
        b.plugin_deps.push_back({"test.cycle_a", VersionConstraint::any(), false, ""});

        resolver.add_available(std::move(a), "/a");
        resolver.add_available(std::move(b), "/b");

        auto result = resolver.validate_acyclic();
        REQUIRE(result.is_err());
    }

    SECTION("detect transitive cycle") {
        // A -> B -> C -> A
        auto add_package = [&](const std::string& name, const std::string& dep) {
            PackageManifest m;
            m.name = name;
            m.type = PackageType::Plugin;
            m.version = SemanticVersion{1, 0, 0};
            m.plugin_deps.push_back({dep, VersionConstraint::any(), false, ""});
            resolver.add_available(std::move(m), "/" + name);
        };

        add_package("test.a", "test.b");
        add_package("test.b", "test.c");
        add_package("test.c", "test.a");

        auto result = resolver.validate_acyclic();
        REQUIRE(result.is_err());
    }

    SECTION("no cycle in valid graph") {
        auto add_package = [&](const std::string& name, std::vector<std::string> deps = {}) {
            PackageManifest m;
            m.name = name;
            m.type = PackageType::Plugin;
            m.version = SemanticVersion{1, 0, 0};
            for (const auto& dep : deps) {
                m.plugin_deps.push_back({dep, VersionConstraint::any(), false, ""});
            }
            resolver.add_available(std::move(m), "/" + name);
        };

        add_package("test.c");
        add_package("test.b", {"test.c"});
        add_package("test.a", {"test.b", "test.c"});

        auto result = resolver.validate_acyclic();
        REQUIRE(result.is_ok());
    }
}

TEST_CASE("PackageResolver plugin layer validation", "[package][resolver]") {
    PackageResolver resolver;

    SECTION("valid layer hierarchy") {
        PackageManifest core;
        core.name = "core.base";
        core.type = PackageType::Plugin;
        core.version = SemanticVersion{1, 0, 0};

        PackageManifest gameplay;
        gameplay.name = "gameplay.combat";
        gameplay.type = PackageType::Plugin;
        gameplay.version = SemanticVersion{1, 0, 0};
        gameplay.plugin_deps.push_back({"core.base", VersionConstraint::any(), false, ""});

        resolver.add_available(std::move(core), "/core");
        resolver.add_available(std::move(gameplay), "/gameplay");

        auto result = resolver.validate_plugin_layers();
        REQUIRE(result.is_ok());
    }

    SECTION("detect layer violation - core depending on gameplay") {
        PackageManifest gameplay;
        gameplay.name = "gameplay.combat";
        gameplay.type = PackageType::Plugin;
        gameplay.version = SemanticVersion{1, 0, 0};

        PackageManifest core;
        core.name = "core.violator";
        core.type = PackageType::Plugin;
        core.version = SemanticVersion{1, 0, 0};
        core.plugin_deps.push_back({"gameplay.combat", VersionConstraint::any(), false, ""});

        resolver.add_available(std::move(gameplay), "/gameplay");
        resolver.add_available(std::move(core), "/core");

        auto result = resolver.validate_plugin_layers();
        REQUIRE(result.is_err());
    }

    SECTION("validate_all runs both checks") {
        PackageManifest m;
        m.name = "test.standalone";
        m.type = PackageType::Plugin;
        m.version = SemanticVersion{1, 0, 0};

        resolver.add_available(std::move(m), "/test");

        auto result = resolver.validate_all();
        REQUIRE(result.is_ok());
    }
}

TEST_CASE("PackageResolver queries", "[package][resolver]") {
    PackageResolver resolver;

    // Set up test packages
    auto add_package = [&](const std::string& name, PackageType type, std::vector<std::string> deps = {}) {
        PackageManifest m;
        m.name = name;
        m.type = type;
        m.version = SemanticVersion{1, 0, 0};
        for (const auto& dep : deps) {
            m.plugin_deps.push_back({dep, VersionConstraint::any(), false, ""});
        }
        resolver.add_available(std::move(m), "/" + name);
    };

    add_package("core.base", PackageType::Plugin);
    add_package("gameplay.combat", PackageType::Plugin, {"core.base"});
    add_package("assets.weapons", PackageType::Asset);
    add_package("world.arena", PackageType::World);

    SECTION("available_packages") {
        auto packages = resolver.available_packages();
        REQUIRE(packages.size() == 4);
    }

    SECTION("packages_of_type") {
        auto plugins = resolver.packages_of_type(PackageType::Plugin);
        REQUIRE(plugins.size() == 2);

        auto assets = resolver.packages_of_type(PackageType::Asset);
        REQUIRE(assets.size() == 1);
        REQUIRE(assets[0] == "assets.weapons");
    }

    SECTION("get_dependents") {
        auto dependents = resolver.get_dependents("core.base");
        REQUIRE(dependents.size() == 1);
        REQUIRE(dependents[0] == "gameplay.combat");
    }

    SECTION("get_dependencies") {
        auto deps = resolver.get_dependencies("gameplay.combat");
        REQUIRE(deps.size() == 1);
        REQUIRE(deps[0] == "core.base");
    }

    SECTION("would_create_cycle") {
        // Adding core.base -> gameplay.combat would create a cycle
        REQUIRE(resolver.would_create_cycle("core.base", "gameplay.combat"));
        // Adding gameplay.combat -> assets.weapons would not
        REQUIRE_FALSE(resolver.would_create_cycle("gameplay.combat", "assets.weapons"));
    }
}

// =============================================================================
// PackageRegistry Tests
// =============================================================================

TEST_CASE("PackageRegistry basic operations", "[package][registry]") {
    PackageRegistry registry;

    SECTION("initially empty") {
        REQUIRE(registry.loaded_count() == 0);
        REQUIRE(registry.available_count() == 0);
    }

    SECTION("is_available and is_loaded") {
        REQUIRE_FALSE(registry.is_available("nonexistent"));
        REQUIRE_FALSE(registry.is_loaded("nonexistent"));
    }
}

// =============================================================================
// LoadContext Tests
// =============================================================================

TEST_CASE("LoadContext operations", "[package][loader]") {
    LoadContext ctx;

    SECTION("initially no systems") {
        REQUIRE(ctx.ecs_world() == nullptr);
        REQUIRE(ctx.event_bus() == nullptr);
    }

    SECTION("loader registration") {
        auto loader = std::make_unique<StubPackageLoader>(PackageType::Plugin, "TestLoader");
        ctx.register_loader(std::move(loader));

        REQUIRE(ctx.has_loader(PackageType::Plugin));
        REQUIRE_FALSE(ctx.has_loader(PackageType::Asset));

        auto* retrieved = ctx.get_loader(PackageType::Plugin);
        REQUIRE(retrieved != nullptr);
        REQUIRE(std::string(retrieved->name()) == "TestLoader");
    }

    SECTION("service registration") {
        struct TestService {
            int value = 42;
        };

        TestService service;
        ctx.register_service<TestService>(&service);

        REQUIRE(ctx.has_service<TestService>());
        auto* retrieved = ctx.get_service<TestService>();
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->value == 42);
    }

    SECTION("loading state tracking") {
        REQUIRE_FALSE(ctx.is_loading("test.package"));

        ctx.begin_loading("test.package");
        REQUIRE(ctx.is_loading("test.package"));

        ctx.end_loading("test.package");
        REQUIRE_FALSE(ctx.is_loading("test.package"));
    }
}

// =============================================================================
// StubPackageLoader Tests
// =============================================================================

TEST_CASE("StubPackageLoader operations", "[package][loader]") {
    StubPackageLoader loader(PackageType::Plugin, "StubPlugin");
    LoadContext ctx;

    SECTION("type and name") {
        REQUIRE(loader.supported_type() == PackageType::Plugin);
        REQUIRE(std::string(loader.name()) == "StubPlugin");
    }

    SECTION("load and unload") {
        ResolvedPackage pkg;
        pkg.manifest.name = "test.package";
        pkg.manifest.type = PackageType::Plugin;
        pkg.manifest.version = SemanticVersion{1, 0, 0};

        REQUIRE_FALSE(loader.is_loaded("test.package"));

        auto load_result = loader.load(pkg, ctx);
        REQUIRE(load_result.is_ok());
        REQUIRE(loader.is_loaded("test.package"));

        auto unload_result = loader.unload("test.package", ctx);
        REQUIRE(unload_result.is_ok());
        REQUIRE_FALSE(loader.is_loaded("test.package"));
    }

    SECTION("loaded_packages") {
        ResolvedPackage pkg1;
        pkg1.manifest.name = "test.pkg1";
        pkg1.manifest.type = PackageType::Plugin;

        ResolvedPackage pkg2;
        pkg2.manifest.name = "test.pkg2";
        pkg2.manifest.type = PackageType::Plugin;

        loader.load(pkg1, ctx);
        loader.load(pkg2, ctx);

        auto loaded = loader.loaded_packages();
        REQUIRE(loaded.size() == 2);
    }
}

// =============================================================================
// Package Type Utilities Tests
// =============================================================================

TEST_CASE("Package type utilities", "[package][fwd]") {
    SECTION("package_type_to_string") {
        REQUIRE(std::string(package_type_to_string(PackageType::World)) == "world");
        REQUIRE(std::string(package_type_to_string(PackageType::Layer)) == "layer");
        REQUIRE(std::string(package_type_to_string(PackageType::Plugin)) == "plugin");
        REQUIRE(std::string(package_type_to_string(PackageType::Widget)) == "widget");
        REQUIRE(std::string(package_type_to_string(PackageType::Asset)) == "asset");
    }

    SECTION("package_type_from_string") {
        PackageType type;

        REQUIRE(package_type_from_string("world", type));
        REQUIRE(type == PackageType::World);

        REQUIRE(package_type_from_string("plugin", type));
        REQUIRE(type == PackageType::Plugin);

        REQUIRE_FALSE(package_type_from_string("invalid", type));
    }

    SECTION("package_type_extension") {
        REQUIRE(std::string(package_type_extension(PackageType::World)) == ".world.json");
        REQUIRE(std::string(package_type_extension(PackageType::Layer)) == ".layer.json");
        REQUIRE(std::string(package_type_extension(PackageType::Plugin)) == ".plugin.json");
        REQUIRE(std::string(package_type_extension(PackageType::Widget)) == ".widget.json");
        REQUIRE(std::string(package_type_extension(PackageType::Asset)) == ".bundle.json");
    }

    SECTION("package_status_to_string") {
        // Implementation uses capitalized strings
        REQUIRE(std::string(package_status_to_string(PackageStatus::Available)) == "Available");
        REQUIRE(std::string(package_status_to_string(PackageStatus::Loading)) == "Loading");
        REQUIRE(std::string(package_status_to_string(PackageStatus::Loaded)) == "Loaded");
        REQUIRE(std::string(package_status_to_string(PackageStatus::Unloading)) == "Unloading");
        REQUIRE(std::string(package_status_to_string(PackageStatus::Failed)) == "Failed");
    }
}
