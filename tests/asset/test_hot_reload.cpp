/// @file test_hot_reload.cpp
/// @brief Tests for void_asset hot-reload system

#include <catch2/catch_test_macros.hpp>
#include <void_engine/asset/hot_reload.hpp>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace void_asset;

// =============================================================================
// FileChangeType Tests
// =============================================================================

TEST_CASE("FileChangeType: names are correct", "[asset][hot_reload]") {
    REQUIRE(std::string(file_change_type_name(FileChangeType::Created)) == "Created");
    REQUIRE(std::string(file_change_type_name(FileChangeType::Modified)) == "Modified");
    REQUIRE(std::string(file_change_type_name(FileChangeType::Deleted)) == "Deleted");
    REQUIRE(std::string(file_change_type_name(FileChangeType::Renamed)) == "Renamed");
}

// =============================================================================
// AssetChangeEvent Tests
// =============================================================================

TEST_CASE("AssetChangeEvent: factory methods", "[asset][hot_reload]") {
    auto created = AssetChangeEvent::created(AssetPath("new.txt"));
    REQUIRE(created.type == FileChangeType::Created);
    REQUIRE(created.path.str() == "new.txt");

    auto modified = AssetChangeEvent::modified(AssetPath("changed.txt"));
    REQUIRE(modified.type == FileChangeType::Modified);

    auto deleted = AssetChangeEvent::deleted(AssetPath("removed.txt"));
    REQUIRE(deleted.type == FileChangeType::Deleted);

    auto renamed = AssetChangeEvent::renamed(AssetPath("old.txt"), AssetPath("new.txt"));
    REQUIRE(renamed.type == FileChangeType::Renamed);
    REQUIRE(renamed.old_path.str() == "old.txt");
    REQUIRE(renamed.path.str() == "new.txt");
}

TEST_CASE("AssetChangeEvent: timestamp is set", "[asset][hot_reload]") {
    auto before = std::chrono::steady_clock::now();
    auto event = AssetChangeEvent::created(AssetPath("test.txt"));
    auto after = std::chrono::steady_clock::now();

    REQUIRE(event.timestamp >= before);
    REQUIRE(event.timestamp <= after);
}

// =============================================================================
// AssetReloadResult Tests
// =============================================================================

TEST_CASE("AssetReloadResult: success", "[asset][hot_reload]") {
    auto result = AssetReloadResult::ok(
        AssetId{42},
        AssetPath("test.txt"),
        5,
        std::chrono::milliseconds{100}
    );

    REQUIRE(result.success);
    REQUIRE(result.id == AssetId{42});
    REQUIRE(result.path.str() == "test.txt");
    REQUIRE(result.new_generation == 5);
    REQUIRE(result.duration == std::chrono::milliseconds{100});
    REQUIRE(result.error.empty());
}

TEST_CASE("AssetReloadResult: failure", "[asset][hot_reload]") {
    auto result = AssetReloadResult::failed(
        AssetId{42},
        AssetPath("test.txt"),
        "File not found"
    );

    REQUIRE_FALSE(result.success);
    REQUIRE(result.id == AssetId{42});
    REQUIRE(result.error == "File not found");
}

// =============================================================================
// FileModificationTracker Tests
// =============================================================================

TEST_CASE("FileModificationTracker: default empty", "[asset][hot_reload]") {
    FileModificationTracker tracker;
    REQUIRE(tracker.size() == 0);
}

TEST_CASE("FileModificationTracker: clear", "[asset][hot_reload]") {
    FileModificationTracker tracker;
    // Can't easily test update without filesystem, but we can test clear
    tracker.clear();
    REQUIRE(tracker.size() == 0);
}

// =============================================================================
// AssetHotReloadConfig Tests
// =============================================================================

TEST_CASE("AssetHotReloadConfig: defaults", "[asset][hot_reload]") {
    AssetHotReloadConfig config;
    REQUIRE(config.enabled == true);
    REQUIRE(config.poll_interval == std::chrono::milliseconds{100});
    REQUIRE(config.debounce_time == std::chrono::milliseconds{50});
    REQUIRE(config.reload_dependencies == true);
    REQUIRE(config.notify_on_failure == true);
    REQUIRE(config.max_concurrent_reloads == 4);
}

TEST_CASE("AssetHotReloadConfig: builder pattern", "[asset][hot_reload]") {
    auto config = AssetHotReloadConfig()
        .with_enabled(false)
        .with_poll_interval(std::chrono::milliseconds{200})
        .with_debounce_time(std::chrono::milliseconds{100})
        .with_reload_dependencies(false);

    REQUIRE(config.enabled == false);
    REQUIRE(config.poll_interval == std::chrono::milliseconds{200});
    REQUIRE(config.debounce_time == std::chrono::milliseconds{100});
    REQUIRE(config.reload_dependencies == false);
}

// =============================================================================
// PollingAssetWatcher Tests
// =============================================================================

TEST_CASE("PollingAssetWatcher: construction", "[asset][hot_reload]") {
    PollingAssetWatcher watcher;
    REQUIRE_FALSE(watcher.is_watching());
}

TEST_CASE("PollingAssetWatcher: start and stop", "[asset][hot_reload]") {
    PollingAssetWatcher watcher(std::chrono::milliseconds{50});

    REQUIRE_FALSE(watcher.is_watching());

    watcher.start();
    REQUIRE(watcher.is_watching());

    watcher.stop();
    REQUIRE_FALSE(watcher.is_watching());
}

TEST_CASE("PollingAssetWatcher: poll returns empty initially", "[asset][hot_reload]") {
    PollingAssetWatcher watcher;
    auto events = watcher.poll();
    REQUIRE(events.empty());
}

TEST_CASE("PollingAssetWatcher: extension filter", "[asset][hot_reload]") {
    PollingAssetWatcher watcher;

    REQUIRE(watcher.extensions().empty());

    watcher.add_extension("txt");
    watcher.add_extension("json");

    REQUIRE(watcher.extensions().size() == 2);
    REQUIRE(watcher.extensions().count("txt") == 1);
    REQUIRE(watcher.extensions().count("json") == 1);

    watcher.clear_extensions();
    REQUIRE(watcher.extensions().empty());
}

TEST_CASE("PollingAssetWatcher: set poll interval", "[asset][hot_reload]") {
    PollingAssetWatcher watcher(std::chrono::milliseconds{100});
    watcher.set_poll_interval(std::chrono::milliseconds{200});
    // Can't directly verify, but it shouldn't crash
}

TEST_CASE("PollingAssetWatcher: callback", "[asset][hot_reload]") {
    PollingAssetWatcher watcher;

    bool callback_called = false;
    watcher.set_callback([&callback_called](const AssetChangeEvent&) {
        callback_called = true;
    });

    // Callback won't be called until there are actual file changes
    // which we can't easily simulate in a unit test
}

TEST_CASE("PollingAssetWatcher: add and remove paths", "[asset][hot_reload]") {
    PollingAssetWatcher watcher;

    // These shouldn't crash even with non-existent paths
    watcher.add_path("nonexistent/path");
    watcher.remove_path("nonexistent/path");
}

// =============================================================================
// AssetHotReloadManager Tests
// =============================================================================

TEST_CASE("AssetHotReloadManager: construction", "[asset][hot_reload]") {
    AssetServer server;
    AssetHotReloadManager manager(server);

    REQUIRE_FALSE(manager.is_running());
    REQUIRE(manager.pending_count() == 0);
}

TEST_CASE("AssetHotReloadManager: construction with config", "[asset][hot_reload]") {
    AssetServer server;
    auto config = AssetHotReloadConfig()
        .with_poll_interval(std::chrono::milliseconds{200});

    AssetHotReloadManager manager(server, config);

    REQUIRE(manager.config().poll_interval == std::chrono::milliseconds{200});
}

TEST_CASE("AssetHotReloadManager: start and stop", "[asset][hot_reload]") {
    AssetServer server;
    AssetHotReloadManager manager(server);

    REQUIRE_FALSE(manager.is_running());

    manager.start();
    REQUIRE(manager.is_running());

    manager.stop();
    REQUIRE_FALSE(manager.is_running());
}

TEST_CASE("AssetHotReloadManager: start when disabled", "[asset][hot_reload]") {
    AssetServer server;
    auto config = AssetHotReloadConfig().with_enabled(false);
    AssetHotReloadManager manager(server, config);

    manager.start();
    REQUIRE_FALSE(manager.is_running());  // Should not start when disabled
}

TEST_CASE("AssetHotReloadManager: drain_results empty initially", "[asset][hot_reload]") {
    AssetServer server;
    AssetHotReloadManager manager(server);

    auto results = manager.drain_results();
    REQUIRE(results.empty());
}

TEST_CASE("AssetHotReloadManager: reload non-existent path", "[asset][hot_reload]") {
    AssetServer server;
    AssetHotReloadManager manager(server);

    auto result = manager.reload("nonexistent.txt");

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error.find("not found") != std::string::npos);
}

TEST_CASE("AssetHotReloadManager: reload callback", "[asset][hot_reload]") {
    AssetServer server;
    AssetHotReloadManager manager(server);

    bool callback_called = false;
    manager.set_callback([&callback_called](const AssetReloadResult&) {
        callback_called = true;
    });

    // Trigger a reload (will fail, but callback should be called)
    manager.reload("test.txt");

    // Callback isn't called for non-existent assets before they're loaded
}

TEST_CASE("AssetHotReloadManager: watcher access", "[asset][hot_reload]") {
    AssetServer server;
    AssetHotReloadManager manager(server);

    auto& watcher = manager.watcher();
    REQUIRE_FALSE(watcher.is_watching());
}

// =============================================================================
// AssetHotReloadSystem Tests
// =============================================================================

TEST_CASE("AssetHotReloadSystem: construction", "[asset][hot_reload]") {
    AssetHotReloadSystem system;

    REQUIRE(system.server().total_count() == 0);
    REQUIRE_FALSE(system.reload_manager().is_running());
}

TEST_CASE("AssetHotReloadSystem: construction with config", "[asset][hot_reload]") {
    auto server_config = AssetServerConfig()
        .with_asset_dir("test_assets");

    auto reload_config = AssetHotReloadConfig()
        .with_poll_interval(std::chrono::milliseconds{150});

    AssetHotReloadSystem system(server_config, reload_config);

    REQUIRE(system.server().config().asset_dir == "test_assets");
    REQUIRE(system.reload_manager().config().poll_interval ==
            std::chrono::milliseconds{150});
}

TEST_CASE("AssetHotReloadSystem: start and stop", "[asset][hot_reload]") {
    AssetHotReloadSystem system;

    system.start();
    REQUIRE(system.reload_manager().is_running());

    system.stop();
    REQUIRE_FALSE(system.reload_manager().is_running());
}

TEST_CASE("AssetHotReloadSystem: load delegates to server", "[asset][hot_reload]") {
    AssetHotReloadSystem system;

    // Register loader on the server
    struct TestAsset { int x; };
    class TestLoader : public AssetLoader<TestAsset> {
    public:
        std::vector<std::string> extensions() const override { return {"test"}; }
        LoadResult<TestAsset> load(LoadContext&) override {
            return void_core::Ok(std::make_unique<TestAsset>());
        }
    };

    system.register_loader(std::make_unique<TestLoader>());

    auto handle = system.load<TestAsset>("file.test");

    REQUIRE(handle.is_valid());
    REQUIRE(system.server().total_count() == 1);
}

TEST_CASE("AssetHotReloadSystem: unload", "[asset][hot_reload]") {
    AssetHotReloadSystem system;

    struct TestAsset { int x; };
    class TestLoader : public AssetLoader<TestAsset> {
    public:
        std::vector<std::string> extensions() const override { return {"test"}; }
        LoadResult<TestAsset> load(LoadContext&) override {
            return void_core::Ok(std::make_unique<TestAsset>());
        }
    };

    system.register_loader(std::make_unique<TestLoader>());
    auto handle = system.load<TestAsset>("file.test");

    REQUIRE(system.server().total_count() == 1);

    system.unload(handle.id());

    REQUIRE(system.server().total_count() == 0);
}

TEST_CASE("AssetHotReloadSystem: drain_events", "[asset][hot_reload]") {
    AssetHotReloadSystem system;

    auto events = system.drain_events();
    REQUIRE(events.empty());
}

TEST_CASE("AssetHotReloadSystem: drain_reload_results", "[asset][hot_reload]") {
    AssetHotReloadSystem system;

    auto results = system.drain_reload_results();
    REQUIRE(results.empty());
}

TEST_CASE("AssetHotReloadSystem: process calls both server and manager", "[asset][hot_reload]") {
    AssetHotReloadSystem system;

    struct TestAsset { int x; };
    class TestLoader : public AssetLoader<TestAsset> {
    public:
        std::vector<std::string> extensions() const override { return {"test"}; }
        LoadResult<TestAsset> load(LoadContext&) override {
            return void_core::Ok(std::make_unique<TestAsset>());
        }
    };

    system.register_loader(std::make_unique<TestLoader>());
    auto handle = system.load<TestAsset>("file.test");

    // Process should handle the pending load (though it will fail without a real file)
    system.process();

    // The asset is in the server
    REQUIRE(system.server().total_count() == 1);
}
