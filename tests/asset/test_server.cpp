/// @file test_server.cpp
/// @brief Tests for void_asset server

#include <catch2/catch_test_macros.hpp>
#include <void_engine/asset/server.hpp>
#include <string>
#include <memory>
#include <vector>

using namespace void_asset;

// Test asset type
struct TestAsset {
    std::string content;
    TestAsset(std::string c = "") : content(std::move(c)) {}
};

// Test loader
class TestAssetLoader : public AssetLoader<TestAsset> {
public:
    std::vector<std::string> extensions() const override {
        return {"test"};
    }

    LoadResult<TestAsset> load(LoadContext& ctx) override {
        auto asset = std::make_unique<TestAsset>(ctx.data_as_string());
        return void_core::Ok(std::move(asset));
    }

    std::string type_name() const override {
        return "TestAsset";
    }
};

// =============================================================================
// AssetServerConfig Tests
// =============================================================================

TEST_CASE("AssetServerConfig: defaults", "[asset][server]") {
    AssetServerConfig config;
    REQUIRE(config.asset_dir == "assets");
    REQUIRE(config.hot_reload == true);
    REQUIRE(config.max_concurrent_loads == 4);
    REQUIRE(config.auto_garbage_collect == true);
}

TEST_CASE("AssetServerConfig: builder pattern", "[asset][server]") {
    auto config = AssetServerConfig()
        .with_asset_dir("custom/assets")
        .with_hot_reload(false)
        .with_max_concurrent_loads(8);

    REQUIRE(config.asset_dir == "custom/assets");
    REQUIRE(config.hot_reload == false);
    REQUIRE(config.max_concurrent_loads == 8);
}

// =============================================================================
// AssetServer Tests
// =============================================================================

TEST_CASE("AssetServer: construction", "[asset][server]") {
    AssetServer server;

    // Built-in loaders should be registered
    REQUIRE(server.loaders().supports_extension("bin"));
    REQUIRE(server.loaders().supports_extension("txt"));
    REQUIRE(server.total_count() == 0);
    REQUIRE(server.loaded_count() == 0);
}

TEST_CASE("AssetServer: construction with config", "[asset][server]") {
    auto config = AssetServerConfig()
        .with_asset_dir("test_assets")
        .with_max_concurrent_loads(2);

    AssetServer server(config);

    REQUIRE(server.config().asset_dir == "test_assets");
    REQUIRE(server.config().max_concurrent_loads == 2);
}

TEST_CASE("AssetServer: register_loader", "[asset][server]") {
    AssetServer server;

    REQUIRE_FALSE(server.loaders().supports_extension("test"));

    server.register_loader(std::make_unique<TestAssetLoader>());

    REQUIRE(server.loaders().supports_extension("test"));
}

TEST_CASE("AssetServer: load returns handle", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("data/file.test");

    REQUIRE(handle.is_valid());
    REQUIRE(handle.state() == LoadState::Loading);
    REQUIRE(server.total_count() == 1);
    REQUIRE(server.pending_count() == 1);
}

TEST_CASE("AssetServer: load returns same handle for same path", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto h1 = server.load<TestAsset>("data/file.test");
    auto h2 = server.load<TestAsset>("data/file.test");

    REQUIRE(h1.id() == h2.id());
    REQUIRE(server.total_count() == 1);
    REQUIRE(server.pending_count() == 1);
}

TEST_CASE("AssetServer: load returns invalid handle for unsupported extension", "[asset][server]") {
    AssetServer server;

    auto handle = server.load<TestAsset>("file.unsupported");

    REQUIRE_FALSE(handle.is_valid());
    REQUIRE(server.total_count() == 0);
}

TEST_CASE("AssetServer: load_untyped", "[asset][server]") {
    AssetServer server;

    auto id = server.load_untyped("data.bin");

    REQUIRE(id.is_valid());
    REQUIRE(server.pending_count() == 1);
}

TEST_CASE("AssetServer: load_untyped invalid extension returns invalid id", "[asset][server]") {
    AssetServer server;

    auto id = server.load_untyped("file.xyz");

    REQUIRE_FALSE(id.is_valid());
}

TEST_CASE("AssetServer: process with custom file reader", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test.test");
    REQUIRE(handle.state() == LoadState::Loading);

    // Custom reader that returns test data
    server.process([](const std::string&) -> std::optional<std::vector<std::uint8_t>> {
        std::string content = "Test Content";
        return std::vector<std::uint8_t>(content.begin(), content.end());
    });

    // Now it should be loaded
    REQUIRE(server.pending_count() == 0);

    // Get fresh handle to check state
    auto loaded_handle = server.get_handle<TestAsset>("test.test");
    REQUIRE(loaded_handle.is_loaded());
    REQUIRE(loaded_handle->content == "Test Content");
}

TEST_CASE("AssetServer: process handles load failure", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("missing.test");

    // Reader returns nullopt (file not found)
    server.process([](const std::string&) -> std::optional<std::vector<std::uint8_t>> {
        return std::nullopt;
    });

    REQUIRE(server.get_state(handle.id()) == LoadState::Failed);
}

TEST_CASE("AssetServer: get_handle for existing asset", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto h1 = server.load<TestAsset>("test.test");

    server.process([](const std::string&) -> std::optional<std::vector<std::uint8_t>> {
        return std::vector<std::uint8_t>{'H', 'e', 'l', 'l', 'o'};
    });

    auto h2 = server.get_handle<TestAsset>("test.test");

    REQUIRE(h1.id() == h2.id());
    REQUIRE(h2.is_loaded());
}

TEST_CASE("AssetServer: get_handle for non-existent returns invalid", "[asset][server]") {
    AssetServer server;

    auto handle = server.get_handle<TestAsset>("nonexistent.test");

    REQUIRE_FALSE(handle.is_valid());
}

TEST_CASE("AssetServer: get_id by path", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("my/asset.test");

    auto id = server.get_id("my/asset.test");
    REQUIRE(id.has_value());
    REQUIRE(*id == handle.id());

    auto missing = server.get_id("other.test");
    REQUIRE_FALSE(missing.has_value());
}

TEST_CASE("AssetServer: get_path by id", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test/path.test");

    auto path = server.get_path(handle.id());
    REQUIRE(path.has_value());
    REQUIRE(path->str() == "test/path.test");

    auto missing = server.get_path(AssetId{9999});
    REQUIRE_FALSE(missing.has_value());
}

TEST_CASE("AssetServer: is_loaded", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test.test");

    REQUIRE_FALSE(server.is_loaded(handle.id()));

    server.process([](const std::string&) {
        return std::vector<std::uint8_t>{'x'};
    });

    REQUIRE(server.is_loaded(handle.id()));
}

TEST_CASE("AssetServer: get_state", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test.test");
    REQUIRE(server.get_state(handle.id()) == LoadState::Loading);

    server.process([](const std::string&) {
        return std::vector<std::uint8_t>{'x'};
    });
    REQUIRE(server.get_state(handle.id()) == LoadState::Loaded);
}

TEST_CASE("AssetServer: get_metadata", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test/file.test");

    const auto* meta = server.get_metadata(handle.id());
    REQUIRE(meta != nullptr);
    REQUIRE(meta->path.str() == "test/file.test");
}

TEST_CASE("AssetServer: unload", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test.test");
    server.process([](const std::string&) {
        return std::vector<std::uint8_t>{'x'};
    });

    REQUIRE(server.total_count() == 1);

    bool removed = server.unload(handle.id());
    REQUIRE(removed);
    REQUIRE(server.total_count() == 0);
}

TEST_CASE("AssetServer: reload with custom reader", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test.test");
    server.process([](const std::string&) {
        return std::vector<std::uint8_t>{'A'};
    });

    auto loaded = server.get_handle<TestAsset>("test.test");
    REQUIRE(loaded->content == "A");

    // Reload with new content
    auto result = server.reload(handle.id(), [](const std::string&) {
        return std::vector<std::uint8_t>{'B'};
    });

    REQUIRE(result);
    auto reloaded = server.get_handle<TestAsset>("test.test");
    REQUIRE(reloaded->content == "B");
}

TEST_CASE("AssetServer: reload non-existent returns error", "[asset][server]") {
    AssetServer server;

    auto result = server.reload(AssetId{9999});

    REQUIRE_FALSE(result);
}

TEST_CASE("AssetServer: drain_events", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test.test");

    server.process([](const std::string&) {
        return std::vector<std::uint8_t>{'x'};
    });

    auto events = server.drain_events();

    REQUIRE(events.size() == 1);
    REQUIRE(events[0].type == AssetEventType::Loaded);
    REQUIRE(events[0].id == handle.id());

    // Second drain should be empty
    auto events2 = server.drain_events();
    REQUIRE(events2.empty());
}

TEST_CASE("AssetServer: drain_events after failure", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test.test");

    server.process([](const std::string&) {
        return std::nullopt;  // Simulate failure
    });

    auto events = server.drain_events();

    REQUIRE(events.size() == 1);
    REQUIRE(events[0].type == AssetEventType::Failed);
    REQUIRE_FALSE(events[0].error.empty());
}

TEST_CASE("AssetServer: drain_events after unload", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    auto handle = server.load<TestAsset>("test.test");
    server.process([](const std::string&) {
        return std::vector<std::uint8_t>{'x'};
    });
    server.drain_events();  // Clear load event

    server.unload(handle.id());

    auto events = server.drain_events();
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].type == AssetEventType::Unloaded);
}

TEST_CASE("AssetServer: collect_garbage", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    {
        auto handle = server.load<TestAsset>("test.test");
        server.process([](const std::string&) {
            return std::vector<std::uint8_t>{'x'};
        });
        // handle goes out of scope here
    }

    REQUIRE(server.total_count() == 1);

    auto collected = server.collect_garbage();
    REQUIRE(collected == 1);
    REQUIRE(server.total_count() == 0);
}

TEST_CASE("AssetServer: counts", "[asset][server]") {
    AssetServer server;
    server.register_loader(std::make_unique<TestAssetLoader>());

    REQUIRE(server.total_count() == 0);
    REQUIRE(server.loaded_count() == 0);
    REQUIRE(server.pending_count() == 0);

    auto h1 = server.load<TestAsset>("a.test");
    auto h2 = server.load<TestAsset>("b.test");
    auto h3 = server.load<TestAsset>("c.test");

    REQUIRE(server.total_count() == 3);
    REQUIRE(server.pending_count() == 3);
    REQUIRE(server.loaded_count() == 0);

    server.process([](const std::string&) {
        return std::vector<std::uint8_t>{'x'};
    });

    REQUIRE(server.pending_count() == 0);
    REQUIRE(server.loaded_count() == 3);
}

TEST_CASE("AssetServer: storage access", "[asset][server]") {
    AssetServer server;

    auto& storage = server.storage();
    REQUIRE(storage.len() == 0);

    const auto& const_storage = std::as_const(server).storage();
    REQUIRE(const_storage.len() == 0);
}

TEST_CASE("AssetServer: loaders access", "[asset][server]") {
    AssetServer server;

    auto& loaders = server.loaders();
    REQUIRE(loaders.len() >= 2);  // BytesLoader and TextLoader

    const auto& const_loaders = std::as_const(server).loaders();
    REQUIRE(const_loaders.len() >= 2);
}
