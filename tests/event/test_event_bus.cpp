/// @file test_event_bus.cpp
/// @brief Tests for EventBus

#include <catch2/catch_test_macros.hpp>
#include <void_engine/event/event.hpp>
#include <atomic>
#include <thread>
#include <vector>

using namespace void_event;

// Test event types
struct TestEvent {
    int value;
};

struct OtherEvent {
    std::string message;
};

TEST_CASE("EventBus: creation", "[event][bus]") {
    EventBus bus;
    REQUIRE(bus.pending_count() == 0);
    REQUIRE_FALSE(bus.has_pending());
}

TEST_CASE("EventBus: publish and process", "[event][bus]") {
    EventBus bus;
    std::atomic<int> received{0};

    bus.subscribe<TestEvent>([&received](const TestEvent& e) {
        received.store(e.value, std::memory_order_relaxed);
    });

    bus.publish(TestEvent{42});
    REQUIRE(bus.has_pending());

    bus.process();
    REQUIRE(received.load() == 42);
    REQUIRE_FALSE(bus.has_pending());
}

TEST_CASE("EventBus: multiple subscribers", "[event][bus]") {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe<TestEvent>([&count](const TestEvent&) {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    bus.subscribe<TestEvent>([&count](const TestEvent&) {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    bus.publish(TestEvent{1});
    bus.process();

    REQUIRE(count.load() == 2);
}

TEST_CASE("EventBus: different event types", "[event][bus]") {
    EventBus bus;
    std::atomic<int> int_count{0};
    std::atomic<int> str_count{0};

    bus.subscribe<TestEvent>([&int_count](const TestEvent&) {
        int_count.fetch_add(1, std::memory_order_relaxed);
    });

    bus.subscribe<OtherEvent>([&str_count](const OtherEvent&) {
        str_count.fetch_add(1, std::memory_order_relaxed);
    });

    bus.publish(TestEvent{1});
    bus.publish(OtherEvent{"hello"});
    bus.process();

    REQUIRE(int_count.load() == 1);
    REQUIRE(str_count.load() == 1);
}

TEST_CASE("EventBus: unsubscribe", "[event][bus]") {
    EventBus bus;
    std::atomic<int> count{0};

    auto sub_id = bus.subscribe<TestEvent>([&count](const TestEvent&) {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    bus.publish(TestEvent{1});
    bus.process();
    REQUIRE(count.load() == 1);

    bus.unsubscribe(sub_id);

    bus.publish(TestEvent{2});
    bus.process();
    REQUIRE(count.load() == 1); // Still 1, not incremented
}

TEST_CASE("EventBus: priority ordering", "[event][bus]") {
    EventBus bus;
    std::vector<std::string> order;
    std::mutex order_mutex;

    bus.subscribe_with_priority<TestEvent>(
        [&](const TestEvent&) {
            std::lock_guard<std::mutex> lock(order_mutex);
            order.push_back("low");
        },
        Priority::Low);

    bus.subscribe_with_priority<TestEvent>(
        [&](const TestEvent&) {
            std::lock_guard<std::mutex> lock(order_mutex);
            order.push_back("high");
        },
        Priority::High);

    bus.subscribe_with_priority<TestEvent>(
        [&](const TestEvent&) {
            std::lock_guard<std::mutex> lock(order_mutex);
            order.push_back("normal");
        },
        Priority::Normal);

    bus.publish(TestEvent{1});
    bus.process();

    REQUIRE(order.size() == 3);
    // Higher priority handlers should run first
    REQUIRE(order[0] == "high");
    REQUIRE(order[1] == "normal");
    REQUIRE(order[2] == "low");
}

TEST_CASE("EventBus: event priority ordering", "[event][bus]") {
    EventBus bus;
    std::vector<int> received;
    std::mutex received_mutex;

    bus.subscribe<TestEvent>([&](const TestEvent& e) {
        std::lock_guard<std::mutex> lock(received_mutex);
        received.push_back(e.value);
    });

    // Publish with different priorities
    bus.publish_with_priority(TestEvent{1}, Priority::Low);
    bus.publish_with_priority(TestEvent{2}, Priority::Critical);
    bus.publish_with_priority(TestEvent{3}, Priority::Normal);

    bus.process();

    REQUIRE(received.size() == 3);
    // Critical should be first, then Normal, then Low
    REQUIRE(received[0] == 2);
    REQUIRE(received[1] == 3);
    REQUIRE(received[2] == 1);
}

TEST_CASE("EventBus: clear pending", "[event][bus]") {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe<TestEvent>([&count](const TestEvent&) {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    bus.publish(TestEvent{1});
    bus.publish(TestEvent{2});
    REQUIRE(bus.pending_count() == 2);

    bus.clear();
    REQUIRE(bus.pending_count() == 0);

    bus.process();
    REQUIRE(count.load() == 0); // Nothing processed
}

TEST_CASE("EventBus: process_batch", "[event][bus]") {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe<TestEvent>([&count](const TestEvent&) {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    for (int i = 0; i < 10; ++i) {
        bus.publish(TestEvent{i});
    }

    // Process only 5
    bus.process_batch(5);
    REQUIRE(count.load() == 5);

    // Process remaining
    bus.process();
    REQUIRE(count.load() == 10);
}

TEST_CASE("EventBus: timestamp increments", "[event][bus]") {
    EventBus bus;

    std::uint64_t ts1 = bus.timestamp();
    bus.process();
    std::uint64_t ts2 = bus.timestamp();

    REQUIRE(ts2 > ts1);
}

TEST_CASE("EventBus: thread safety", "[event][bus]") {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe<TestEvent>([&count](const TestEvent&) {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    constexpr int events_per_thread = 100;
    constexpr int num_threads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&bus]() {
            for (int i = 0; i < events_per_thread; ++i) {
                bus.publish(TestEvent{i});
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Process all events
    bus.process();

    REQUIRE(count.load() == events_per_thread * num_threads);
}

TEST_CASE("SubscriberId: validity", "[event][bus]") {
    SubscriberId invalid;
    REQUIRE_FALSE(invalid.is_valid());

    SubscriberId valid(42);
    REQUIRE(valid.is_valid());
}

TEST_CASE("Priority: ordering", "[event][bus]") {
    REQUIRE(static_cast<int>(Priority::Low) < static_cast<int>(Priority::Normal));
    REQUIRE(static_cast<int>(Priority::Normal) < static_cast<int>(Priority::High));
    REQUIRE(static_cast<int>(Priority::High) < static_cast<int>(Priority::Critical));
}
