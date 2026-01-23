/// @file test_storage.cpp
/// @brief Tests for void_asset storage

#include <catch2/catch_test_macros.hpp>
#include <void_engine/asset/storage.hpp>
#include <string>
#include <memory>

using namespace void_asset;

// Test asset types
struct TestAsset {
    int value = 0;
    TestAsset(int v = 0) : value(v) {}
};

struct OtherAsset {
    std::string name;
};

// =============================================================================
// AssetEntry Tests
// =============================================================================

TEST_CASE("AssetEntry: default construction", "[asset][storage]") {
    AssetEntry entry;
    REQUIRE(entry.asset == nullptr);
    REQUIRE(entry.handle_data == nullptr);
}

TEST_CASE("AssetEntry: templated construction", "[asset][storage]") {
    auto handle_data = std::make_shared<HandleData>();
    auto asset = std::make_unique<TestAsset>(42);
    AssetMetadata meta;
    meta.id = AssetId{1};

    AssetEntry entry(handle_data, std::move(asset), meta);

    REQUIRE(entry.handle_data == handle_data);
    REQUIRE(entry.asset != nullptr);
    REQUIRE(entry.type_id == std::type_index(typeid(TestAsset)));
    REQUIRE(entry.get<TestAsset>()->value == 42);
}

TEST_CASE("AssetEntry: get with wrong type returns nullptr", "[asset][storage]") {
    auto handle_data = std::make_shared<HandleData>();
    auto asset = std::make_unique<TestAsset>(42);
    AssetMetadata meta;

    AssetEntry entry(handle_data, std::move(asset), meta);

    REQUIRE(entry.get<TestAsset>() != nullptr);
    REQUIRE(entry.get<OtherAsset>() == nullptr);
}

TEST_CASE("AssetEntry: move construction", "[asset][storage]") {
    auto handle_data = std::make_shared<HandleData>();
    auto asset = std::make_unique<TestAsset>(100);
    AssetMetadata meta;

    AssetEntry entry1(handle_data, std::move(asset), meta);
    AssetEntry entry2(std::move(entry1));

    REQUIRE(entry1.asset == nullptr);  // Moved from
    REQUIRE(entry2.asset != nullptr);
    REQUIRE(entry2.get<TestAsset>()->value == 100);
}

TEST_CASE("AssetEntry: move assignment", "[asset][storage]") {
    auto hd1 = std::make_shared<HandleData>();
    auto hd2 = std::make_shared<HandleData>();
    auto asset1 = std::make_unique<TestAsset>(10);
    auto asset2 = std::make_unique<TestAsset>(20);
    AssetMetadata meta;

    AssetEntry entry1(hd1, std::move(asset1), meta);
    AssetEntry entry2(hd2, std::move(asset2), meta);

    entry2 = std::move(entry1);

    REQUIRE(entry1.asset == nullptr);
    REQUIRE(entry2.get<TestAsset>()->value == 10);
}

// =============================================================================
// AssetStorage Tests
// =============================================================================

TEST_CASE("AssetStorage: allocate_id", "[asset][storage]") {
    AssetStorage storage;

    auto id1 = storage.allocate_id();
    auto id2 = storage.allocate_id();
    auto id3 = storage.allocate_id();

    REQUIRE(id1.raw() == 1);
    REQUIRE(id2.raw() == 2);
    REQUIRE(id3.raw() == 3);
}

TEST_CASE("AssetStorage: register_asset", "[asset][storage]") {
    AssetStorage storage;

    auto id = storage.allocate_id();
    auto handle = storage.register_asset<TestAsset>(id, AssetPath("test.txt"));

    REQUIRE(handle.is_valid());
    REQUIRE(handle.id() == id);
    REQUIRE(handle.state() == LoadState::Loading);
    REQUIRE(storage.contains(id));
    REQUIRE(storage.len() == 1);
}

TEST_CASE("AssetStorage: store asset", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));

    auto asset = std::make_unique<TestAsset>(42);
    storage.store(id, std::move(asset));

    REQUIRE(storage.is_loaded(id));
    REQUIRE(storage.get_state(id) == LoadState::Loaded);

    auto* retrieved = storage.get<TestAsset>(id);
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->value == 42);
}

TEST_CASE("AssetStorage: store_erased", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));

    TestAsset* raw = new TestAsset(99);
    storage.store_erased(id, raw, std::type_index(typeid(TestAsset)),
        [](void* ptr) { delete static_cast<TestAsset*>(ptr); });

    REQUIRE(storage.is_loaded(id));
    auto* retrieved = storage.get<TestAsset>(id);
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->value == 99);
}

TEST_CASE("AssetStorage: mark_failed", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));

    storage.mark_failed(id, "Test error");

    REQUIRE(storage.get_state(id) == LoadState::Failed);
    auto* meta = storage.get_metadata(id);
    REQUIRE(meta != nullptr);
    REQUIRE(meta->error_message == "Test error");
}

TEST_CASE("AssetStorage: mark_reloading", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));
    storage.store(id, std::make_unique<TestAsset>());

    storage.mark_reloading(id);

    REQUIRE(storage.get_state(id) == LoadState::Reloading);
}

TEST_CASE("AssetStorage: get_handle", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));

    auto asset = std::make_unique<TestAsset>(42);
    storage.store(id, std::move(asset));

    auto handle = storage.get_handle<TestAsset>(id);
    REQUIRE(handle.is_valid());
    REQUIRE(handle.is_loaded());
    REQUIRE(handle->value == 42);
}

TEST_CASE("AssetStorage: get_handle wrong type returns invalid", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));
    storage.store(id, std::make_unique<TestAsset>());

    auto handle = storage.get_handle<OtherAsset>(id);
    REQUIRE_FALSE(handle.is_valid());
}

TEST_CASE("AssetStorage: get_metadata", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test/path.txt"));

    const auto* meta = storage.get_metadata(id);
    REQUIRE(meta != nullptr);
    REQUIRE(meta->id == id);
    REQUIRE(meta->path.str() == "test/path.txt");
}

TEST_CASE("AssetStorage: get_id by path", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("my/asset.txt"));

    auto found = storage.get_id(AssetPath("my/asset.txt"));
    REQUIRE(found.has_value());
    REQUIRE(*found == id);

    auto not_found = storage.get_id(AssetPath("other.txt"));
    REQUIRE_FALSE(not_found.has_value());
}

TEST_CASE("AssetStorage: contains", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();

    REQUIRE_FALSE(storage.contains(id));

    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));

    REQUIRE(storage.contains(id));
}

TEST_CASE("AssetStorage: is_loaded", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));

    REQUIRE_FALSE(storage.is_loaded(id));

    storage.store(id, std::make_unique<TestAsset>());

    REQUIRE(storage.is_loaded(id));
}

TEST_CASE("AssetStorage: remove", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));
    storage.store(id, std::make_unique<TestAsset>());

    REQUIRE(storage.contains(id));
    REQUIRE(storage.remove(id));
    REQUIRE_FALSE(storage.contains(id));
    REQUIRE_FALSE(storage.remove(id));  // Already removed
}

TEST_CASE("AssetStorage: remove clears path mapping", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));

    REQUIRE(storage.get_id(AssetPath("test.txt")).has_value());

    storage.remove(id);

    REQUIRE_FALSE(storage.get_id(AssetPath("test.txt")).has_value());
}

TEST_CASE("AssetStorage: collect_garbage", "[asset][storage]") {
    AssetStorage storage;

    // Create asset with handle that goes out of scope
    auto id = storage.allocate_id();
    {
        auto handle = storage.register_asset<TestAsset>(id, AssetPath("test.txt"));
        storage.store(id, std::make_unique<TestAsset>());
        // handle destroyed here, reducing ref count
    }

    auto unreferenced = storage.collect_garbage();
    REQUIRE(unreferenced.size() == 1);
    REQUIRE(unreferenced[0] == id);
}

TEST_CASE("AssetStorage: remove_unreferenced", "[asset][storage]") {
    AssetStorage storage;

    auto id = storage.allocate_id();
    {
        auto handle = storage.register_asset<TestAsset>(id, AssetPath("test.txt"));
        storage.store(id, std::make_unique<TestAsset>());
    }

    REQUIRE(storage.len() == 1);
    auto removed = storage.remove_unreferenced();
    REQUIRE(removed == 1);
    REQUIRE(storage.len() == 0);
}

TEST_CASE("AssetStorage: loaded_count", "[asset][storage]") {
    AssetStorage storage;

    auto id1 = storage.allocate_id();
    auto id2 = storage.allocate_id();
    auto id3 = storage.allocate_id();

    storage.register_asset<TestAsset>(id1, AssetPath("a.txt"));
    storage.register_asset<TestAsset>(id2, AssetPath("b.txt"));
    storage.register_asset<TestAsset>(id3, AssetPath("c.txt"));

    REQUIRE(storage.loaded_count() == 0);

    storage.store(id1, std::make_unique<TestAsset>());
    REQUIRE(storage.loaded_count() == 1);

    storage.store(id2, std::make_unique<TestAsset>());
    REQUIRE(storage.loaded_count() == 2);

    storage.mark_failed(id3, "Error");
    REQUIRE(storage.loaded_count() == 2);  // Failed doesn't count
}

TEST_CASE("AssetStorage: clear", "[asset][storage]") {
    AssetStorage storage;

    auto id1 = storage.allocate_id();
    auto id2 = storage.allocate_id();
    storage.register_asset<TestAsset>(id1, AssetPath("a.txt"));
    storage.register_asset<TestAsset>(id2, AssetPath("b.txt"));

    REQUIRE(storage.len() == 2);

    storage.clear();

    REQUIRE(storage.len() == 0);
    REQUIRE_FALSE(storage.contains(id1));
    REQUIRE_FALSE(storage.contains(id2));
}

TEST_CASE("AssetStorage: for_each", "[asset][storage]") {
    AssetStorage storage;

    auto id1 = storage.allocate_id();
    auto id2 = storage.allocate_id();
    storage.register_asset<TestAsset>(id1, AssetPath("a.txt"));
    storage.register_asset<TestAsset>(id2, AssetPath("b.txt"));

    std::vector<AssetId> visited;
    storage.for_each([&visited](AssetId id, const AssetMetadata&) {
        visited.push_back(id);
    });

    REQUIRE(visited.size() == 2);
    REQUIRE(std::find(visited.begin(), visited.end(), id1) != visited.end());
    REQUIRE(std::find(visited.begin(), visited.end(), id2) != visited.end());
}

TEST_CASE("AssetStorage: replaces old asset on store", "[asset][storage]") {
    AssetStorage storage;
    auto id = storage.allocate_id();
    storage.register_asset<TestAsset>(id, AssetPath("test.txt"));

    storage.store(id, std::make_unique<TestAsset>(10));
    REQUIRE(storage.get<TestAsset>(id)->value == 10);

    storage.store(id, std::make_unique<TestAsset>(20));
    REQUIRE(storage.get<TestAsset>(id)->value == 20);
}
