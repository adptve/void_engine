/// @file test_rehydration.cpp
/// @brief Tests for void_presenter rehydration types

#include <void_engine/presenter/rehydration.hpp>
#include <cassert>
#include <iostream>

using namespace void_presenter;

void test_rehydration_state_strings() {
    std::cout << "  test_rehydration_state_strings...";

    RehydrationState state;

    // Set and get
    state.set_string("name", "test_presenter");
    auto name = state.get_string("name");
    assert(name.has_value());
    assert(*name == "test_presenter");

    // Missing key
    auto missing = state.get_string("nonexistent");
    assert(!missing.has_value());

    // Builder pattern
    state.with_string("key2", "value2");
    assert(state.get_string("key2").has_value());

    std::cout << " PASSED\n";
}

void test_rehydration_state_integers() {
    std::cout << "  test_rehydration_state_integers...";

    RehydrationState state;

    // Signed int
    state.set_int("count", -42);
    auto count = state.get_int("count");
    assert(count.has_value());
    assert(*count == -42);

    // Unsigned via builder
    state.with_uint("frame", 12345);
    auto frame = state.get_uint("frame");
    assert(frame.has_value());
    assert(*frame == 12345);

    std::cout << " PASSED\n";
}

void test_rehydration_state_floats() {
    std::cout << "  test_rehydration_state_floats...";

    RehydrationState state;

    state.set_float("scale", 3.14159);
    auto scale = state.get_float("scale");
    assert(scale.has_value());
    assert(*scale > 3.14 && *scale < 3.15);

    std::cout << " PASSED\n";
}

void test_rehydration_state_bools() {
    std::cout << "  test_rehydration_state_bools...";

    RehydrationState state;

    state.set_bool("enabled", true);
    state.set_bool("paused", false);

    assert(state.get_bool("enabled") == true);
    assert(state.get_bool("paused") == false);
    assert(!state.get_bool("missing").has_value());

    std::cout << " PASSED\n";
}

void test_rehydration_state_binary() {
    std::cout << "  test_rehydration_state_binary...";

    RehydrationState state;

    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5, 255, 0, 128};
    state.set_binary("buffer", data);

    auto retrieved = state.get_binary("buffer");
    assert(retrieved != nullptr);
    assert(*retrieved == data);

    assert(state.get_binary("missing") == nullptr);

    std::cout << " PASSED\n";
}

void test_rehydration_state_nested() {
    std::cout << "  test_rehydration_state_nested...";

    RehydrationState state;

    RehydrationState inner;
    inner.set_int("x", 10);
    inner.set_int("y", 20);
    inner.set_int("z", 30);

    state.set_nested("position", std::move(inner));

    // Get nested
    const auto* pos = state.get_nested("position");
    assert(pos != nullptr);
    assert(pos->get_int("x") == 10);
    assert(pos->get_int("y") == 20);
    assert(pos->get_int("z") == 30);

    // Mutable nested
    auto* pos_mut = state.get_nested_mut("position");
    assert(pos_mut != nullptr);
    pos_mut->set_int("w", 40);
    assert(state.get_nested("position")->get_int("w") == 40);

    std::cout << " PASSED\n";
}

void test_rehydration_state_is_empty() {
    std::cout << "  test_rehydration_state_is_empty...";

    RehydrationState state;
    assert(state.is_empty());

    state.set_int("test", 1);
    assert(!state.is_empty());

    state.clear();
    assert(state.is_empty());

    std::cout << " PASSED\n";
}

void test_rehydration_state_merge() {
    std::cout << "  test_rehydration_state_merge...";

    RehydrationState state1;
    state1.set_string("a", "1");
    state1.set_int("x", 100);

    RehydrationState state2;
    state2.set_string("b", "2");
    state2.set_int("y", 200);

    state1.merge(state2);

    assert(state1.get_string("a") == "1");
    assert(state1.get_string("b") == "2");
    assert(state1.get_int("x") == 100);
    assert(state1.get_int("y") == 200);

    std::cout << " PASSED\n";
}

void test_rehydration_store() {
    std::cout << "  test_rehydration_store...";

    RehydrationStore store;

    // Initially empty
    assert(store.size() == 0);
    assert(!store.contains("test"));

    // Store
    RehydrationState state;
    state.set_int("version", 1);
    store.store("presenter_1", std::move(state));

    assert(store.size() == 1);
    assert(store.contains("presenter_1"));

    // Retrieve
    auto retrieved = store.retrieve("presenter_1");
    assert(retrieved.has_value());
    assert(retrieved->get_int("version") == 1);

    // Keys
    auto keys = store.keys();
    assert(keys.size() == 1);
    assert(keys[0] == "presenter_1");

    // Remove
    auto removed = store.remove("presenter_1");
    assert(removed.has_value());
    assert(!store.contains("presenter_1"));
    assert(store.size() == 0);

    std::cout << " PASSED\n";
}

void test_rehydration_store_multiple() {
    std::cout << "  test_rehydration_store_multiple...";

    RehydrationStore store;

    for (int i = 0; i < 5; ++i) {
        RehydrationState state;
        state.set_int("id", i);
        store.store("item_" + std::to_string(i), std::move(state));
    }

    assert(store.size() == 5);

    auto keys = store.keys();
    assert(keys.size() == 5);

    // Clear
    store.clear();
    assert(store.size() == 0);

    std::cout << " PASSED\n";
}

void test_rehydration_builder_pattern() {
    std::cout << "  test_rehydration_builder_pattern...";

    RehydrationState state;
    state.with_string("name", "test")
         .with_int("count", 42)
         .with_uint("id", 1234)
         .with_float("scale", 1.5)
         .with_bool("enabled", true);

    assert(state.get_string("name") == "test");
    assert(state.get_int("count") == 42);
    assert(state.get_uint("id") == 1234);
    assert(state.get_float("scale").value() == 1.5);
    assert(state.get_bool("enabled") == true);

    std::cout << " PASSED\n";
}

int main() {
    std::cout << "Running presenter rehydration tests...\n";

    test_rehydration_state_strings();
    test_rehydration_state_integers();
    test_rehydration_state_floats();
    test_rehydration_state_bools();
    test_rehydration_state_binary();
    test_rehydration_state_nested();
    test_rehydration_state_is_empty();
    test_rehydration_state_merge();
    test_rehydration_store();
    test_rehydration_store_multiple();
    test_rehydration_builder_pattern();

    std::cout << "All presenter rehydration tests passed!\n";
    return 0;
}
