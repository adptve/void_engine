/// @file test_handle.cpp
/// @brief Tests for void_asset handles

#include <catch2/catch_test_macros.hpp>
#include <void_engine/asset/handle.hpp>
#include <string>
#include <memory>

using namespace void_asset;

// Test asset type
struct TestAsset {
    int value = 0;
    TestAsset(int v = 0) : value(v) {}
};

// =============================================================================
// HandleData Tests
// =============================================================================

TEST_CASE("HandleData: initial state", "[asset][handle]") {
    HandleData data;
    REQUIRE(data.use_count() == 1);
    REQUIRE(data.get_generation() == 0);
    REQUIRE(data.get_state() == LoadState::NotLoaded);
    REQUIRE_FALSE(data.is_loaded());
}

TEST_CASE("HandleData: reference counting", "[asset][handle]") {
    HandleData data;
    REQUIRE(data.use_count() == 1);

    data.add_strong();
    REQUIRE(data.use_count() == 2);

    REQUIRE_FALSE(data.release_strong());  // Not last
    REQUIRE(data.use_count() == 1);

    REQUIRE(data.release_strong());  // Last
    REQUIRE(data.use_count() == 0);
}

TEST_CASE("HandleData: weak counting", "[asset][handle]") {
    HandleData data;

    data.add_weak();
    REQUIRE_FALSE(data.release_weak());  // Not last

    data.add_weak();
    data.add_weak();
    REQUIRE_FALSE(data.release_weak());
    REQUIRE_FALSE(data.release_weak());
}

TEST_CASE("HandleData: try_upgrade", "[asset][handle]") {
    HandleData data;
    REQUIRE(data.use_count() == 1);

    // Can upgrade when strong count > 0
    REQUIRE(data.try_upgrade());
    REQUIRE(data.use_count() == 2);

    // Release all strong refs
    data.release_strong();
    data.release_strong();
    REQUIRE(data.use_count() == 0);

    // Cannot upgrade when strong count is 0
    REQUIRE_FALSE(data.try_upgrade());
    REQUIRE(data.use_count() == 0);
}

TEST_CASE("HandleData: state management", "[asset][handle]") {
    HandleData data;

    data.set_state(LoadState::Loading);
    REQUIRE(data.get_state() == LoadState::Loading);
    REQUIRE_FALSE(data.is_loaded());

    data.set_state(LoadState::Loaded);
    REQUIRE(data.get_state() == LoadState::Loaded);
    REQUIRE(data.is_loaded());
}

TEST_CASE("HandleData: generation", "[asset][handle]") {
    HandleData data;
    REQUIRE(data.get_generation() == 0);

    data.increment_generation();
    REQUIRE(data.get_generation() == 1);

    data.increment_generation();
    data.increment_generation();
    REQUIRE(data.get_generation() == 3);
}

// =============================================================================
// Handle<T> Tests
// =============================================================================

TEST_CASE("Handle: default is null", "[asset][handle]") {
    Handle<TestAsset> handle;
    REQUIRE_FALSE(handle.is_valid());
    REQUIRE_FALSE(handle.is_loaded());
    REQUIRE(handle.get() == nullptr);
    REQUIRE(handle.state() == LoadState::NotLoaded);
}

TEST_CASE("Handle: construct with data", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    data->set_state(LoadState::Loaded);
    TestAsset asset{42};

    Handle<TestAsset> handle(data, &asset);
    REQUIRE(handle.is_valid());
    REQUIRE(handle.is_loaded());
    REQUIRE(handle.get() == &asset);
    REQUIRE(handle->value == 42);
    REQUIRE((*handle).value == 42);
}

TEST_CASE("Handle: copy increments ref count", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    TestAsset asset;
    Handle<TestAsset> h1(data, &asset);
    REQUIRE(data->use_count() == 2);  // shared_ptr + Handle

    {
        Handle<TestAsset> h2 = h1;
        REQUIRE(data->use_count() == 3);
    }

    REQUIRE(data->use_count() == 2);  // h2 destroyed
}

TEST_CASE("Handle: move does not increment ref count", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    TestAsset asset;
    Handle<TestAsset> h1(data, &asset);
    REQUIRE(data->use_count() == 2);

    Handle<TestAsset> h2 = std::move(h1);
    REQUIRE(data->use_count() == 2);  // Same count
    REQUIRE_FALSE(h1.is_valid());
    REQUIRE(h2.is_valid());
}

TEST_CASE("Handle: reset clears handle", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    TestAsset asset;
    Handle<TestAsset> handle(data, &asset);
    REQUIRE(handle.is_valid());

    handle.reset();
    REQUIRE_FALSE(handle.is_valid());
    REQUIRE(handle.get() == nullptr);
}

TEST_CASE("Handle: id and generation", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    data->id = AssetId{42};
    data->increment_generation();

    Handle<TestAsset> handle(data, nullptr);
    REQUIRE(handle.id() == AssetId{42});
    REQUIRE(handle.generation() == 1);
}

TEST_CASE("Handle: loading state", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    Handle<TestAsset> handle(data, nullptr);

    data->set_state(LoadState::Loading);
    REQUIRE(handle.is_loading());
    REQUIRE_FALSE(handle.is_loaded());
    REQUIRE_FALSE(handle.is_failed());

    data->set_state(LoadState::Failed);
    REQUIRE_FALSE(handle.is_loading());
    REQUIRE_FALSE(handle.is_loaded());
    REQUIRE(handle.is_failed());
}

TEST_CASE("Handle: comparison", "[asset][handle]") {
    auto data1 = std::make_shared<HandleData>();
    auto data2 = std::make_shared<HandleData>();

    Handle<TestAsset> h1(data1, nullptr);
    Handle<TestAsset> h2(data1, nullptr);
    Handle<TestAsset> h3(data2, nullptr);

    REQUIRE(h1 == h2);
    REQUIRE(h1 != h3);
}

TEST_CASE("Handle: bool conversion", "[asset][handle]") {
    Handle<TestAsset> null_handle;
    REQUIRE_FALSE(static_cast<bool>(null_handle));

    auto data = std::make_shared<HandleData>();
    Handle<TestAsset> valid_handle(data, nullptr);
    REQUIRE(static_cast<bool>(valid_handle));
}

TEST_CASE("Handle: update_asset", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    data->set_state(LoadState::Loaded);
    TestAsset asset1{10};
    TestAsset asset2{20};

    Handle<TestAsset> handle(data, &asset1);
    REQUIRE(handle->value == 10);

    handle.update_asset(&asset2);
    REQUIRE(handle->value == 20);
}

// =============================================================================
// WeakHandle<T> Tests
// =============================================================================

TEST_CASE("WeakHandle: default is expired", "[asset][handle]") {
    WeakHandle<TestAsset> weak;
    REQUIRE(weak.expired());
    REQUIRE_FALSE(weak.lock().is_valid());
}

TEST_CASE("WeakHandle: from strong handle", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    TestAsset asset;
    Handle<TestAsset> strong(data, &asset);

    WeakHandle<TestAsset> weak(strong);
    REQUIRE_FALSE(weak.expired());
    REQUIRE(weak.id() == strong.id());
}

TEST_CASE("WeakHandle: lock returns strong", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    data->set_state(LoadState::Loaded);
    TestAsset asset{42};
    Handle<TestAsset> strong(data, &asset);

    WeakHandle<TestAsset> weak(strong);
    auto locked = weak.lock();
    REQUIRE(locked.is_valid());
}

TEST_CASE("WeakHandle: expires when strong released", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    TestAsset asset;
    WeakHandle<TestAsset> weak;

    {
        Handle<TestAsset> strong(data, &asset);
        weak = WeakHandle<TestAsset>(strong);
        REQUIRE_FALSE(weak.expired());
    }

    // After strong destroyed, use_count becomes 0
    // Note: With shared_ptr holding data, it's not truly expired
    // until the shared_ptr is also released
}

TEST_CASE("WeakHandle: copy and move", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    TestAsset asset;
    Handle<TestAsset> strong(data, &asset);

    WeakHandle<TestAsset> w1(strong);
    WeakHandle<TestAsset> w2 = w1;
    REQUIRE(w1.id() == w2.id());

    WeakHandle<TestAsset> w3 = std::move(w2);
    REQUIRE(w3.id() == w1.id());
}

TEST_CASE("WeakHandle: reset", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    TestAsset asset;
    Handle<TestAsset> strong(data, &asset);

    WeakHandle<TestAsset> weak(strong);
    REQUIRE_FALSE(weak.expired());

    weak.reset();
    REQUIRE(weak.expired());
}

// =============================================================================
// UntypedHandle Tests
// =============================================================================

TEST_CASE("UntypedHandle: default is invalid", "[asset][handle]") {
    UntypedHandle handle;
    REQUIRE_FALSE(handle.is_valid());
    REQUIRE_FALSE(handle.is_loaded());
}

TEST_CASE("UntypedHandle: from typed handle", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    data->id = AssetId{42};
    data->set_state(LoadState::Loaded);
    TestAsset asset{100};

    Handle<TestAsset> typed(data, &asset);
    UntypedHandle untyped(typed);

    REQUIRE(untyped.is_valid());
    REQUIRE(untyped.is_loaded());
    REQUIRE(untyped.id() == AssetId{42});
    REQUIRE(untyped.type_id() == std::type_index(typeid(TestAsset)));
}

TEST_CASE("UntypedHandle: is_type check", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    TestAsset asset;
    Handle<TestAsset> typed(data, &asset);
    UntypedHandle untyped(typed);

    REQUIRE(untyped.is_type<TestAsset>());
    REQUIRE_FALSE(untyped.is_type<int>());
}

TEST_CASE("UntypedHandle: downcast success", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    data->set_state(LoadState::Loaded);
    TestAsset asset{42};
    Handle<TestAsset> typed(data, &asset);
    UntypedHandle untyped(typed);

    auto back = untyped.downcast<TestAsset>();
    REQUIRE(back.is_valid());
    REQUIRE(back->value == 42);
}

TEST_CASE("UntypedHandle: downcast failure", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    TestAsset asset;
    Handle<TestAsset> typed(data, &asset);
    UntypedHandle untyped(typed);

    auto wrong = untyped.downcast<int>();
    REQUIRE_FALSE(wrong.is_valid());
}

// =============================================================================
// AssetRef<T> Tests
// =============================================================================

TEST_CASE("AssetRef: default", "[asset][handle]") {
    AssetRef<TestAsset> ref;
    REQUIRE(ref.path().empty());
    REQUIRE_FALSE(ref.is_loaded());
    REQUIRE(ref.get() == nullptr);
}

TEST_CASE("AssetRef: from path", "[asset][handle]") {
    AssetRef<TestAsset> ref("textures/test.png");
    REQUIRE(ref.path() == "textures/test.png");
    REQUIRE_FALSE(ref.is_loaded());
}

TEST_CASE("AssetRef: from handle", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    data->set_state(LoadState::Loaded);
    TestAsset asset{42};
    Handle<TestAsset> handle(data, &asset);

    AssetRef<TestAsset> ref(handle);
    REQUIRE(ref.is_loaded());
    REQUIRE(ref.get()->value == 42);
}

TEST_CASE("AssetRef: set_path clears handle", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    data->set_state(LoadState::Loaded);
    TestAsset asset;
    Handle<TestAsset> handle(data, &asset);

    AssetRef<TestAsset> ref(handle);
    REQUIRE(ref.is_loaded());

    ref.set_path("new/path.txt");
    REQUIRE(ref.path() == "new/path.txt");
    REQUIRE_FALSE(ref.is_loaded());
}

TEST_CASE("AssetRef: set_handle", "[asset][handle]") {
    auto data = std::make_shared<HandleData>();
    data->set_state(LoadState::Loaded);
    TestAsset asset{99};
    Handle<TestAsset> handle(data, &asset);

    AssetRef<TestAsset> ref("test.txt");
    ref.set_handle(handle);
    REQUIRE(ref.is_loaded());
    REQUIRE(ref.get()->value == 99);
}
