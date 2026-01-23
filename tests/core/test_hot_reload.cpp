// void_core Hot-reload system tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace void_core;

// =============================================================================
// Test HotReloadable Implementation
// =============================================================================

class TestReloadable : public HotReloadable {
public:
    int value = 0;
    std::string name;
    Version ver{1, 0, 0};

    TestReloadable() = default;
    TestReloadable(int v, std::string n) : value(v), name(std::move(n)) {}

    Result<HotReloadSnapshot> snapshot() override {
        std::vector<std::uint8_t> data;
        data.resize(sizeof(int));
        std::memcpy(data.data(), &value, sizeof(int));

        return Ok(HotReloadSnapshot{
            std::move(data),
            std::type_index(typeid(TestReloadable)),
            "TestReloadable",
            ver
        });
    }

    Result<void> restore(HotReloadSnapshot snap) override {
        if (snap.data.size() >= sizeof(int)) {
            std::memcpy(&value, snap.data.data(), sizeof(int));
        }
        return Ok();
    }

    bool is_compatible(const Version& new_version) const override {
        return ver.is_compatible_with(new_version);
    }

    Version current_version() const override {
        return ver;
    }

    std::string type_name() const override {
        return "TestReloadable";
    }
};

// =============================================================================
// ReloadEvent Tests
// =============================================================================

TEST_CASE("ReloadEvent construction", "[core][hot_reload]") {
    SECTION("default") {
        ReloadEvent event;
        REQUIRE(event.type == ReloadEventType::FileModified);
        REQUIRE(event.path.empty());
    }

    SECTION("with type and path") {
        ReloadEvent event(ReloadEventType::FileCreated, "/path/to/file");
        REQUIRE(event.type == ReloadEventType::FileCreated);
        REQUIRE(event.path == "/path/to/file");
    }

    SECTION("rename event") {
        ReloadEvent event("/old/path", "/new/path");
        REQUIRE(event.type == ReloadEventType::FileRenamed);
        REQUIRE(event.path == "/new/path");
        REQUIRE(event.old_path == "/old/path");
    }
}

TEST_CASE("ReloadEvent factory methods", "[core][hot_reload]") {
    SECTION("modified") {
        auto event = ReloadEvent::modified("/test/file.cpp");
        REQUIRE(event.type == ReloadEventType::FileModified);
        REQUIRE(event.path == "/test/file.cpp");
    }

    SECTION("created") {
        auto event = ReloadEvent::created("/test/new.cpp");
        REQUIRE(event.type == ReloadEventType::FileCreated);
    }

    SECTION("deleted") {
        auto event = ReloadEvent::deleted("/test/removed.cpp");
        REQUIRE(event.type == ReloadEventType::FileDeleted);
    }

    SECTION("renamed") {
        auto event = ReloadEvent::renamed("/old.cpp", "/new.cpp");
        REQUIRE(event.type == ReloadEventType::FileRenamed);
        REQUIRE(event.old_path == "/old.cpp");
        REQUIRE(event.path == "/new.cpp");
    }

    SECTION("force_reload") {
        auto event = ReloadEvent::force_reload("/force/reload.cpp");
        REQUIRE(event.type == ReloadEventType::ForceReload);
    }
}

TEST_CASE("ReloadEventType names", "[core][hot_reload]") {
    REQUIRE(std::string(reload_event_type_name(ReloadEventType::FileModified)) == "FileModified");
    REQUIRE(std::string(reload_event_type_name(ReloadEventType::FileCreated)) == "FileCreated");
    REQUIRE(std::string(reload_event_type_name(ReloadEventType::FileDeleted)) == "FileDeleted");
    REQUIRE(std::string(reload_event_type_name(ReloadEventType::FileRenamed)) == "FileRenamed");
    REQUIRE(std::string(reload_event_type_name(ReloadEventType::ForceReload)) == "ForceReload");
}

// =============================================================================
// HotReloadSnapshot Tests
// =============================================================================

TEST_CASE("HotReloadSnapshot construction", "[core][hot_reload]") {
    SECTION("empty") {
        HotReloadSnapshot snap = HotReloadSnapshot::empty();
        REQUIRE(snap.is_empty());
        REQUIRE(snap.data.empty());
    }

    SECTION("with data") {
        std::vector<std::uint8_t> data{1, 2, 3, 4};
        HotReloadSnapshot snap(
            data,
            std::type_index(typeid(int)),
            "int",
            Version{1, 0, 0}
        );
        REQUIRE_FALSE(snap.is_empty());
        REQUIRE(snap.data.size() == 4);
        REQUIRE(snap.type_name == "int");
        REQUIRE(snap.version == Version{1, 0, 0});
    }
}

TEST_CASE("HotReloadSnapshot metadata", "[core][hot_reload]") {
    HotReloadSnapshot snap;

    snap.with_metadata("key1", "value1")
        .with_metadata("key2", "value2");

    const std::string* v1 = snap.get_metadata("key1");
    REQUIRE(v1 != nullptr);
    REQUIRE(*v1 == "value1");

    const std::string* v2 = snap.get_metadata("key2");
    REQUIRE(v2 != nullptr);
    REQUIRE(*v2 == "value2");

    REQUIRE(snap.get_metadata("nonexistent") == nullptr);
}

TEST_CASE("HotReloadSnapshot is_type", "[core][hot_reload]") {
    HotReloadSnapshot snap(
        {},
        std::type_index(typeid(int)),
        "int",
        Version::zero()
    );

    REQUIRE(snap.is_type<int>());
    REQUIRE_FALSE(snap.is_type<float>());
}

// =============================================================================
// HotReloadManager Tests
// =============================================================================

TEST_CASE("HotReloadManager construction", "[core][hot_reload]") {
    HotReloadManager manager;
    REQUIRE(manager.is_empty());
    REQUIRE(manager.len() == 0);
}

TEST_CASE("HotReloadManager register_object", "[core][hot_reload]") {
    HotReloadManager manager;

    auto obj = std::make_unique<TestReloadable>(42, "test");
    auto result = manager.register_object("test_obj", std::move(obj), "/path/test.cpp");

    REQUIRE(result.is_ok());
    REQUIRE(manager.len() == 1);
    REQUIRE(manager.contains("test_obj"));
}

TEST_CASE("HotReloadManager register null", "[core][hot_reload]") {
    HotReloadManager manager;

    auto result = manager.register_object("null_obj", nullptr);
    REQUIRE(result.is_err());
}

TEST_CASE("HotReloadManager register duplicate", "[core][hot_reload]") {
    HotReloadManager manager;

    manager.register_object("obj", std::make_unique<TestReloadable>());
    auto result = manager.register_object("obj", std::make_unique<TestReloadable>());

    REQUIRE(result.is_err());
    REQUIRE(result.error().code() == ErrorCode::AlreadyExists);
}

TEST_CASE("HotReloadManager unregister", "[core][hot_reload]") {
    HotReloadManager manager;

    manager.register_object("obj", std::make_unique<TestReloadable>());
    REQUIRE(manager.contains("obj"));

    bool removed = manager.unregister_object("obj");
    REQUIRE(removed);
    REQUIRE_FALSE(manager.contains("obj"));

    // Remove again should fail
    REQUIRE_FALSE(manager.unregister_object("obj"));
}

TEST_CASE("HotReloadManager get", "[core][hot_reload]") {
    HotReloadManager manager;

    auto obj = std::make_unique<TestReloadable>(42, "test");
    manager.register_object("obj", std::move(obj));

    SECTION("get") {
        HotReloadable* ptr = manager.get("obj");
        REQUIRE(ptr != nullptr);
    }

    SECTION("get const") {
        const HotReloadManager& const_mgr = manager;
        const HotReloadable* ptr = const_mgr.get("obj");
        REQUIRE(ptr != nullptr);
    }

    SECTION("get_as") {
        TestReloadable* ptr = manager.get_as<TestReloadable>("obj");
        REQUIRE(ptr != nullptr);
        REQUIRE(ptr->value == 42);
    }

    SECTION("get not found") {
        REQUIRE(manager.get("unknown") == nullptr);
    }
}

TEST_CASE("HotReloadManager reload workflow", "[core][hot_reload]") {
    HotReloadManager manager;

    auto obj = std::make_unique<TestReloadable>(42, "original");
    manager.register_object("obj", std::move(obj), "/path/test.cpp");

    // Start reload - takes snapshot
    auto reload_result = manager.reload("obj");
    REQUIRE(reload_result.is_ok());
    REQUIRE(manager.is_pending("obj"));

    // Complete reload with new object
    auto new_obj = std::make_unique<TestReloadable>(0, "new");
    auto complete_result = manager.complete_reload("obj", std::move(new_obj));
    REQUIRE(complete_result.is_ok());
    REQUIRE_FALSE(manager.is_pending("obj"));

    // Value should be restored
    auto* reloaded = manager.get_as<TestReloadable>("obj");
    REQUIRE(reloaded != nullptr);
    REQUIRE(reloaded->value == 42);  // State preserved!
}

TEST_CASE("HotReloadManager cancel_reload", "[core][hot_reload]") {
    HotReloadManager manager;

    manager.register_object("obj", std::make_unique<TestReloadable>());
    manager.reload("obj");
    REQUIRE(manager.is_pending("obj"));

    manager.cancel_reload("obj");
    REQUIRE_FALSE(manager.is_pending("obj"));
}

TEST_CASE("HotReloadManager queue_event", "[core][hot_reload]") {
    HotReloadManager manager;

    manager.register_object("obj", std::make_unique<TestReloadable>(), "/test.cpp");

    manager.queue_event(ReloadEvent::modified("/test.cpp"));
    REQUIRE(manager.pending_count() == 1);

    auto results = manager.process_pending();
    REQUIRE(results.size() == 1);
    REQUIRE(manager.pending_count() == 0);
}

TEST_CASE("HotReloadManager find_by_path", "[core][hot_reload]") {
    HotReloadManager manager;

    manager.register_object("obj1", std::make_unique<TestReloadable>(), "/path/a.cpp");
    manager.register_object("obj2", std::make_unique<TestReloadable>(), "/path/b.cpp");

    const std::string* name = manager.find_by_path("/path/a.cpp");
    REQUIRE(name != nullptr);
    REQUIRE(*name == "obj1");

    REQUIRE(manager.find_by_path("/unknown.cpp") == nullptr);
}

TEST_CASE("HotReloadManager on_reload callback", "[core][hot_reload]") {
    HotReloadManager manager;

    std::vector<std::pair<std::string, bool>> callbacks;
    manager.on_reload([&callbacks](const std::string& name, bool success) {
        callbacks.emplace_back(name, success);
    });

    manager.register_object("obj", std::make_unique<TestReloadable>());
    manager.reload("obj");
    manager.complete_reload("obj", std::make_unique<TestReloadable>());

    REQUIRE(callbacks.size() == 1);
    REQUIRE(callbacks[0].first == "obj");
    REQUIRE(callbacks[0].second == true);
}

TEST_CASE("HotReloadManager for_each", "[core][hot_reload]") {
    HotReloadManager manager;

    manager.register_object("obj1", std::make_unique<TestReloadable>(1, "one"));
    manager.register_object("obj2", std::make_unique<TestReloadable>(2, "two"));

    std::vector<std::string> names;
    manager.for_each([&names](const std::string& name, const HotReloadable& /*obj*/) {
        names.push_back(name);
    });

    REQUIRE(names.size() == 2);
}

// =============================================================================
// MemoryFileWatcher Tests
// =============================================================================

TEST_CASE("MemoryFileWatcher construction", "[core][hot_reload]") {
    MemoryFileWatcher watcher;
    REQUIRE(watcher.watched_count() == 0);
}

TEST_CASE("MemoryFileWatcher watch/unwatch", "[core][hot_reload]") {
    MemoryFileWatcher watcher;

    SECTION("watch") {
        auto result = watcher.watch("/test/file.cpp");
        REQUIRE(result.is_ok());
        REQUIRE(watcher.is_watching("/test/file.cpp"));
        REQUIRE(watcher.watched_count() == 1);
    }

    SECTION("watch duplicate") {
        watcher.watch("/test/file.cpp");
        auto result = watcher.watch("/test/file.cpp");
        REQUIRE(result.is_err());
    }

    SECTION("unwatch") {
        watcher.watch("/test/file.cpp");
        auto result = watcher.unwatch("/test/file.cpp");
        REQUIRE(result.is_ok());
        REQUIRE_FALSE(watcher.is_watching("/test/file.cpp"));
    }

    SECTION("unwatch not watched") {
        auto result = watcher.unwatch("/not/watched.cpp");
        REQUIRE(result.is_err());
    }
}

TEST_CASE("MemoryFileWatcher simulate events", "[core][hot_reload]") {
    MemoryFileWatcher watcher;
    watcher.watch("/test/file.cpp");

    SECTION("simulate_modify") {
        watcher.simulate_modify("/test/file.cpp");
        auto events = watcher.poll();
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == ReloadEventType::FileModified);
        REQUIRE(events[0].path == "/test/file.cpp");
    }

    SECTION("simulate_create") {
        watcher.simulate_create("/test/new.cpp");
        auto events = watcher.poll();
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == ReloadEventType::FileCreated);
    }

    SECTION("simulate_delete") {
        watcher.simulate_delete("/test/file.cpp");
        auto events = watcher.poll();
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == ReloadEventType::FileDeleted);
    }

    SECTION("simulate_rename") {
        watcher.simulate_rename("/old.cpp", "/new.cpp");
        auto events = watcher.poll();
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == ReloadEventType::FileRenamed);
        REQUIRE(events[0].old_path == "/old.cpp");
        REQUIRE(events[0].path == "/new.cpp");
    }

    SECTION("poll clears events") {
        watcher.simulate_modify("/test/file.cpp");
        watcher.poll();
        auto events = watcher.poll();
        REQUIRE(events.empty());
    }
}

TEST_CASE("MemoryFileWatcher clear", "[core][hot_reload]") {
    MemoryFileWatcher watcher;
    watcher.watch("/a.cpp");
    watcher.watch("/b.cpp");
    watcher.simulate_modify("/a.cpp");

    watcher.clear();

    REQUIRE(watcher.watched_count() == 0);
    REQUIRE(watcher.poll().empty());
}

// =============================================================================
// PollingFileWatcher Tests
// =============================================================================

TEST_CASE("PollingFileWatcher construction", "[core][hot_reload]") {
    PollingFileWatcher watcher;
    REQUIRE(watcher.watched_count() == 0);
}

TEST_CASE("PollingFileWatcher watch non-existent", "[core][hot_reload]") {
    PollingFileWatcher watcher;

    // Should succeed even for non-existent files
    auto result = watcher.watch("/non/existent/path/file.cpp");
    REQUIRE(result.is_ok());
    REQUIRE(watcher.is_watching("/non/existent/path/file.cpp"));
}

TEST_CASE("PollingFileWatcher interval", "[core][hot_reload]") {
    PollingFileWatcher watcher(std::chrono::milliseconds(50));

    watcher.watch("/test.cpp");

    // First poll should work
    auto events1 = watcher.poll();

    // Immediate second poll should return empty due to interval
    auto events2 = watcher.poll();
    REQUIRE(events2.empty());
}

// =============================================================================
// HotReloadSystem Tests
// =============================================================================

TEST_CASE("HotReloadSystem construction", "[core][hot_reload]") {
    SECTION("default") {
        HotReloadSystem system;
        REQUIRE(system.manager().is_empty());
    }

    SECTION("with custom watcher") {
        auto watcher = std::make_unique<MemoryFileWatcher>();
        HotReloadSystem system(std::move(watcher));
        REQUIRE(system.manager().is_empty());
    }
}

TEST_CASE("HotReloadSystem register_watched", "[core][hot_reload]") {
    auto watcher = std::make_unique<MemoryFileWatcher>();
    auto* watcher_ptr = watcher.get();
    HotReloadSystem system(std::move(watcher));

    auto result = system.register_watched(
        "obj",
        std::make_unique<TestReloadable>(),
        "/test.cpp"
    );

    REQUIRE(result.is_ok());
    REQUIRE(system.manager().contains("obj"));
    REQUIRE(watcher_ptr->is_watching("/test.cpp"));
}

TEST_CASE("HotReloadSystem update", "[core][hot_reload]") {
    auto watcher = std::make_unique<MemoryFileWatcher>();
    auto* watcher_ptr = watcher.get();
    HotReloadSystem system(std::move(watcher));

    system.register_watched("obj", std::make_unique<TestReloadable>(), "/test.cpp");

    // Simulate file change
    watcher_ptr->simulate_modify("/test.cpp");

    auto results = system.update();
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].is_ok());

    // Object should be pending reload
    REQUIRE(system.manager().is_pending("obj"));
}
