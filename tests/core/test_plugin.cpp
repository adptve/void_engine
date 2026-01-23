// void_core Plugin system tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <void_engine/core/plugin.hpp>
#include <string>
#include <unordered_set>
#include <vector>

using namespace void_core;

// =============================================================================
// Test Plugin Implementations
// =============================================================================

class TestPlugin : public Plugin {
public:
    VOID_DEFINE_PLUGIN(TestPlugin, "test_plugin", 1, 0, 0)

    int load_count = 0;
    int update_count = 0;
    int unload_count = 0;
    float total_dt = 0.0f;

    Result<void> on_load(PluginContext& /*ctx*/) override {
        load_count++;
        return Ok();
    }

    void on_update(float dt) override {
        update_count++;
        total_dt += dt;
    }

    Result<PluginState> on_unload(PluginContext& /*ctx*/) override {
        unload_count++;
        return Ok(PluginState::empty());
    }
};

class DependentPlugin : public Plugin {
public:
    VOID_DEFINE_PLUGIN(DependentPlugin, "dependent_plugin", 1, 0, 0)

    std::vector<PluginId> dependencies() const override {
        return {PluginId("test_plugin")};
    }

    Result<void> on_load(PluginContext& /*ctx*/) override {
        return Ok();
    }

    Result<PluginState> on_unload(PluginContext& /*ctx*/) override {
        return Ok(PluginState::empty());
    }
};

class FailingPlugin : public Plugin {
public:
    VOID_DEFINE_PLUGIN(FailingPlugin, "failing_plugin", 1, 0, 0)

    Result<void> on_load(PluginContext& /*ctx*/) override {
        return Err<void>(Error("Load failed intentionally"));
    }

    Result<PluginState> on_unload(PluginContext& /*ctx*/) override {
        return Ok(PluginState::empty());
    }
};

class HotReloadablePlugin : public Plugin {
public:
    VOID_DEFINE_PLUGIN(HotReloadablePlugin, "hot_reloadable", 1, 0, 0)

    int state_value = 0;

    bool supports_hot_reload() const override {
        return true;
    }

    Result<void> on_load(PluginContext& /*ctx*/) override {
        return Ok();
    }

    Result<PluginState> on_unload(PluginContext& /*ctx*/) override {
        // Serialize state
        PluginState state;
        state.data.resize(sizeof(int));
        std::memcpy(state.data.data(), &state_value, sizeof(int));
        state.type_name = "HotReloadablePlugin";
        state.version = version();
        return Ok(std::move(state));
    }

    Result<void> on_reload(PluginContext& ctx, PluginState state) override {
        if (!state.is_empty() && state.data.size() >= sizeof(int)) {
            std::memcpy(&state_value, state.data.data(), sizeof(int));
        }
        return on_load(ctx);
    }
};

// =============================================================================
// PluginId Tests
// =============================================================================

TEST_CASE("PluginId construction", "[core][plugin]") {
    SECTION("from string") {
        PluginId id("test_plugin");
        REQUIRE(id.name() == "test_plugin");
        REQUIRE(id.hash() != 0);
    }

    SECTION("from const char*") {
        PluginId id("test_plugin");
        REQUIRE(id.name() == "test_plugin");
    }

    SECTION("default") {
        PluginId id;
        REQUIRE(id.name().empty());
    }
}

TEST_CASE("PluginId comparison", "[core][plugin]") {
    PluginId a("alpha");
    PluginId b("alpha");
    PluginId c("beta");

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE(a != c);
    bool has_ordering = (a < c) || (c < a);
    REQUIRE(has_ordering);  // Some ordering exists
}

TEST_CASE("PluginId hashing", "[core][plugin]") {
    PluginId id1("test1");
    PluginId id2("test1");
    PluginId id3("test2");

    std::unordered_set<PluginId> set;
    set.insert(id1);

    REQUIRE(set.count(id2) == 1);
    REQUIRE(set.count(id3) == 0);
}

// =============================================================================
// PluginStatus Tests
// =============================================================================

TEST_CASE("PluginStatus names", "[core][plugin]") {
    REQUIRE(std::string(plugin_status_name(PluginStatus::Registered)) == "Registered");
    REQUIRE(std::string(plugin_status_name(PluginStatus::Loading)) == "Loading");
    REQUIRE(std::string(plugin_status_name(PluginStatus::Active)) == "Active");
    REQUIRE(std::string(plugin_status_name(PluginStatus::Unloading)) == "Unloading");
    REQUIRE(std::string(plugin_status_name(PluginStatus::Failed)) == "Failed");
    REQUIRE(std::string(plugin_status_name(PluginStatus::Disabled)) == "Disabled");
}

// =============================================================================
// PluginState Tests
// =============================================================================

TEST_CASE("PluginState construction", "[core][plugin]") {
    SECTION("empty") {
        PluginState state = PluginState::empty();
        REQUIRE(state.is_empty());
        REQUIRE(state.data.empty());
    }

    SECTION("with data") {
        std::vector<std::uint8_t> data{1, 2, 3, 4};
        PluginState state(data, "TestType", Version{1, 0, 0});
        REQUIRE_FALSE(state.is_empty());
        REQUIRE(state.data.size() == 4);
        REQUIRE(state.type_name == "TestType");
        REQUIRE(state.version == Version{1, 0, 0});
    }
}

// =============================================================================
// PluginContext Tests
// =============================================================================

TEST_CASE("PluginContext data storage", "[core][plugin]") {
    PluginContext ctx;

    SECTION("insert and get") {
        ctx.insert("key1", 42);
        ctx.insert("key2", std::string("hello"));

        const int* val1 = ctx.get<int>("key1");
        REQUIRE(val1 != nullptr);
        REQUIRE(*val1 == 42);

        const std::string* val2 = ctx.get<std::string>("key2");
        REQUIRE(val2 != nullptr);
        REQUIRE(*val2 == "hello");
    }

    SECTION("get_mut") {
        ctx.insert("value", 10);
        int* ptr = ctx.get_mut<int>("value");
        REQUIRE(ptr != nullptr);
        *ptr = 20;
        REQUIRE(*ctx.get<int>("value") == 20);
    }

    SECTION("contains") {
        ctx.insert("exists", 1);
        REQUIRE(ctx.contains("exists"));
        REQUIRE_FALSE(ctx.contains("not_exists"));
    }

    SECTION("remove") {
        ctx.insert("to_remove", 1);
        REQUIRE(ctx.contains("to_remove"));
        REQUIRE(ctx.remove("to_remove"));
        REQUIRE_FALSE(ctx.contains("to_remove"));
        REQUIRE_FALSE(ctx.remove("to_remove"));  // Already removed
    }
}

// =============================================================================
// Plugin Base Class Tests
// =============================================================================

TEST_CASE("Plugin info", "[core][plugin]") {
    TestPlugin plugin;

    PluginInfo info = plugin.info();
    REQUIRE(info.id.name() == "test_plugin");
    REQUIRE(info.version == Version{1, 0, 0});
    REQUIRE(info.dependencies.empty());
    REQUIRE_FALSE(info.supports_hot_reload);
}

TEST_CASE("Plugin default implementations", "[core][plugin]") {
    TestPlugin plugin;

    REQUIRE(plugin.dependencies().empty());
    REQUIRE_FALSE(plugin.supports_hot_reload());
}

// =============================================================================
// PluginRegistry Tests
// =============================================================================

TEST_CASE("PluginRegistry construction", "[core][plugin]") {
    PluginRegistry registry;
    REQUIRE(registry.is_empty());
    REQUIRE(registry.len() == 0);
    REQUIRE(registry.active_count() == 0);
}

TEST_CASE("PluginRegistry register", "[core][plugin]") {
    PluginRegistry registry;

    auto result = registry.register_plugin(std::make_unique<TestPlugin>());
    REQUIRE(result.is_ok());
    REQUIRE(registry.len() == 1);

    const PluginInfo* info = registry.info(PluginId("test_plugin"));
    REQUIRE(info != nullptr);
    REQUIRE(info->status == PluginStatus::Registered);
}

TEST_CASE("PluginRegistry register null", "[core][plugin]") {
    PluginRegistry registry;

    auto result = registry.register_plugin(nullptr);
    REQUIRE(result.is_err());
}

TEST_CASE("PluginRegistry register duplicate", "[core][plugin]") {
    PluginRegistry registry;

    registry.register_plugin(std::make_unique<TestPlugin>());
    auto result = registry.register_plugin(std::make_unique<TestPlugin>());
    REQUIRE(result.is_err());
    REQUIRE(result.error().code() == ErrorCode::AlreadyExists);
}

TEST_CASE("PluginRegistry load", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    registry.register_plugin(std::make_unique<TestPlugin>());

    auto result = registry.load(PluginId("test_plugin"), types);
    REQUIRE(result.is_ok());

    REQUIRE(registry.is_active(PluginId("test_plugin")));
    REQUIRE(registry.active_count() == 1);

    auto* plugin = dynamic_cast<TestPlugin*>(registry.get(PluginId("test_plugin")));
    REQUIRE(plugin != nullptr);
    REQUIRE(plugin->load_count == 1);
}

TEST_CASE("PluginRegistry load not found", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    auto result = registry.load(PluginId("unknown"), types);
    REQUIRE(result.is_err());
    REQUIRE(result.error().code() == ErrorCode::NotFound);
}

TEST_CASE("PluginRegistry load with dependencies", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    registry.register_plugin(std::make_unique<TestPlugin>());
    registry.register_plugin(std::make_unique<DependentPlugin>());

    SECTION("missing dependency fails") {
        auto result = registry.load(PluginId("dependent_plugin"), types);
        REQUIRE(result.is_err());
        REQUIRE(result.error().code() == ErrorCode::DependencyMissing);
    }

    SECTION("with dependency loaded succeeds") {
        registry.load(PluginId("test_plugin"), types);
        auto result = registry.load(PluginId("dependent_plugin"), types);
        REQUIRE(result.is_ok());
    }
}

TEST_CASE("PluginRegistry load failure", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    registry.register_plugin(std::make_unique<FailingPlugin>());

    auto result = registry.load(PluginId("failing_plugin"), types);
    REQUIRE(result.is_err());

    const PluginInfo* info = registry.info(PluginId("failing_plugin"));
    REQUIRE(info != nullptr);
    REQUIRE(info->status == PluginStatus::Failed);
}

TEST_CASE("PluginRegistry unload", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    registry.register_plugin(std::make_unique<TestPlugin>());
    registry.load(PluginId("test_plugin"), types);

    auto* plugin = dynamic_cast<TestPlugin*>(registry.get(PluginId("test_plugin")));

    auto result = registry.unload(PluginId("test_plugin"), types);
    REQUIRE(result.is_ok());
    REQUIRE(plugin->unload_count == 1);
    REQUIRE_FALSE(registry.is_active(PluginId("test_plugin")));
}

TEST_CASE("PluginRegistry unload not active", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    registry.register_plugin(std::make_unique<TestPlugin>());

    auto result = registry.unload(PluginId("test_plugin"), types);
    REQUIRE(result.is_err());
    REQUIRE(result.error().code() == ErrorCode::InvalidState);
}

TEST_CASE("PluginRegistry update_all", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    registry.register_plugin(std::make_unique<TestPlugin>());
    registry.load(PluginId("test_plugin"), types);

    auto* plugin = dynamic_cast<TestPlugin*>(registry.get(PluginId("test_plugin")));

    registry.update_all(0.016f);
    registry.update_all(0.016f);
    registry.update_all(0.016f);

    REQUIRE(plugin->update_count == 3);
    REQUIRE(plugin->total_dt == Catch::Approx(0.048f));
}

TEST_CASE("PluginRegistry hot_reload", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    auto plugin = std::make_unique<HotReloadablePlugin>();
    plugin->state_value = 42;

    registry.register_plugin(std::move(plugin));
    registry.load(PluginId("hot_reloadable"), types);

    // Create new version
    auto new_plugin = std::make_unique<HotReloadablePlugin>();

    auto result = registry.hot_reload(
        PluginId("hot_reloadable"),
        std::move(new_plugin),
        types);

    REQUIRE(result.is_ok());

    auto* reloaded = dynamic_cast<HotReloadablePlugin*>(
        registry.get(PluginId("hot_reloadable")));
    REQUIRE(reloaded != nullptr);
    REQUIRE(reloaded->state_value == 42);  // State preserved
}

TEST_CASE("PluginRegistry hot_reload not supported", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    registry.register_plugin(std::make_unique<TestPlugin>());
    registry.load(PluginId("test_plugin"), types);

    auto result = registry.hot_reload(
        PluginId("test_plugin"),
        std::make_unique<TestPlugin>(),
        types);

    REQUIRE(result.is_err());
    REQUIRE(result.error().code() == ErrorCode::InvalidState);
}

TEST_CASE("PluginRegistry get", "[core][plugin]") {
    PluginRegistry registry;

    registry.register_plugin(std::make_unique<TestPlugin>());

    REQUIRE(registry.get(PluginId("test_plugin")) != nullptr);
    REQUIRE(registry.get(PluginId("unknown")) == nullptr);

    const PluginRegistry& const_reg = registry;
    REQUIRE(const_reg.get(PluginId("test_plugin")) != nullptr);
}

TEST_CASE("PluginRegistry load_order", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    registry.register_plugin(std::make_unique<TestPlugin>());
    registry.register_plugin(std::make_unique<HotReloadablePlugin>());

    registry.load(PluginId("test_plugin"), types);
    registry.load(PluginId("hot_reloadable"), types);

    const auto& order = registry.load_order();
    REQUIRE(order.size() == 2);
    REQUIRE(order[0].name() == "test_plugin");
    REQUIRE(order[1].name() == "hot_reloadable");
}

TEST_CASE("PluginRegistry for_each_active", "[core][plugin]") {
    PluginRegistry registry;
    TypeRegistry types;

    registry.register_plugin(std::make_unique<TestPlugin>());
    registry.register_plugin(std::make_unique<HotReloadablePlugin>());

    registry.load(PluginId("test_plugin"), types);
    // hot_reloadable not loaded

    std::vector<std::string> names;
    registry.for_each_active([&names](const Plugin& p) {
        names.push_back(p.id().name());
    });

    REQUIRE(names.size() == 1);
    REQUIRE(names[0] == "test_plugin");
}
