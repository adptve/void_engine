/// @file test_channel.cpp
/// @brief Tests for EventChannel and BroadcastChannel

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

TEST_CASE("EventChannel: creation", "[event][channel]") {
    EventChannel<TestEvent> channel;
    REQUIRE(channel.empty());
    REQUIRE(channel.size() == 0);
}

TEST_CASE("EventChannel: send and receive", "[event][channel]") {
    EventChannel<TestEvent> channel;

    channel.send(TestEvent{42});
    REQUIRE_FALSE(channel.empty());

    auto event = channel.receive();
    REQUIRE(event.has_value());
    REQUIRE(event->value == 42);

    REQUIRE(channel.empty());
}

TEST_CASE("EventChannel: FIFO order", "[event][channel]") {
    EventChannel<TestEvent> channel;

    channel.send(TestEvent{1});
    channel.send(TestEvent{2});
    channel.send(TestEvent{3});

    REQUIRE(channel.receive()->value == 1);
    REQUIRE(channel.receive()->value == 2);
    REQUIRE(channel.receive()->value == 3);
    REQUIRE_FALSE(channel.receive().has_value());
}

TEST_CASE("EventChannel: drain", "[event][channel]") {
    EventChannel<TestEvent> channel;

    channel.send(TestEvent{1});
    channel.send(TestEvent{2});
    channel.send(TestEvent{3});

    auto events = channel.drain();
    REQUIRE(events.size() == 3);
    REQUIRE(events[0].value == 1);
    REQUIRE(events[1].value == 2);
    REQUIRE(events[2].value == 3);

    REQUIRE(channel.empty());
}

TEST_CASE("EventChannel: drain_batch", "[event][channel]") {
    EventChannel<TestEvent> channel;

    for (int i = 0; i < 10; ++i) {
        channel.send(TestEvent{i});
    }

    auto batch = channel.drain_batch(5);
    REQUIRE(batch.size() == 5);
    REQUIRE(channel.size() == 5);

    auto rest = channel.drain();
    REQUIRE(rest.size() == 5);
}

TEST_CASE("EventChannel: send_batch", "[event][channel]") {
    EventChannel<TestEvent> channel;

    std::vector<TestEvent> events = {{1}, {2}, {3}};
    channel.send_batch(events.begin(), events.end());

    REQUIRE(channel.size() == 3);
}

TEST_CASE("EventChannel: send_batch initializer_list", "[event][channel]") {
    EventChannel<TestEvent> channel;

    channel.send_batch({TestEvent{1}, TestEvent{2}, TestEvent{3}});

    REQUIRE(channel.size() == 3);
}

TEST_CASE("EventChannel: for_each", "[event][channel]") {
    EventChannel<TestEvent> channel;

    channel.send(TestEvent{1});
    channel.send(TestEvent{2});
    channel.send(TestEvent{3});

    int sum = 0;
    auto count = channel.for_each([&sum](const TestEvent& e) {
        sum += e.value;
    });

    REQUIRE(count == 3);
    REQUIRE(sum == 6);
    REQUIRE(channel.empty());
}

TEST_CASE("EventChannel: for_each_while", "[event][channel]") {
    EventChannel<TestEvent> channel;

    channel.send(TestEvent{1});
    channel.send(TestEvent{2});
    channel.send(TestEvent{3});

    int sum = 0;
    auto count = channel.for_each_while([&sum](const TestEvent& e) {
        sum += e.value;
        return e.value < 2; // Stop after seeing 2
    });

    REQUIRE(count == 2);
    REQUIRE(sum == 3); // 1 + 2
}

TEST_CASE("EventChannel: try_receive alias", "[event][channel]") {
    EventChannel<TestEvent> channel;

    REQUIRE_FALSE(channel.try_receive().has_value());

    channel.send(TestEvent{42});
    auto event = channel.try_receive();
    REQUIRE(event.has_value());
    REQUIRE(event->value == 42);
}

TEST_CASE("EventChannel: is_empty and len aliases", "[event][channel]") {
    EventChannel<TestEvent> channel;

    REQUIRE(channel.is_empty());
    REQUIRE(channel.len() == 0);

    channel.send(TestEvent{1});

    REQUIRE_FALSE(channel.is_empty());
    REQUIRE(channel.len() == 1);
}

TEST_CASE("EventChannel: thread safety", "[event][channel]") {
    EventChannel<TestEvent> channel;
    std::atomic<int> received{0};

    constexpr int events_per_thread = 100;
    constexpr int num_producers = 4;

    std::vector<std::thread> producers;
    for (int t = 0; t < num_producers; ++t) {
        producers.emplace_back([&channel]() {
            for (int i = 0; i < events_per_thread; ++i) {
                channel.send(TestEvent{i});
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }

    // Drain all
    auto events = channel.drain();
    REQUIRE(events.size() == events_per_thread * num_producers);
}

// =============================================================================
// BroadcastChannel Tests
// =============================================================================

TEST_CASE("BroadcastChannel: creation", "[event][broadcast]") {
    BroadcastChannel<TestEvent> broadcast;
    REQUIRE(broadcast.receiver_count() == 0);
}

TEST_CASE("BroadcastChannel: create_receiver", "[event][broadcast]") {
    BroadcastChannel<TestEvent> broadcast;

    auto recv1 = broadcast.create_receiver();
    REQUIRE(recv1 != nullptr);
    REQUIRE(broadcast.receiver_count() == 1);

    auto recv2 = broadcast.create_receiver();
    REQUIRE(recv2 != nullptr);
    REQUIRE(broadcast.receiver_count() == 2);
}

TEST_CASE("BroadcastChannel: send to all receivers", "[event][broadcast]") {
    BroadcastChannel<TestEvent> broadcast;

    auto recv1 = broadcast.create_receiver();
    auto recv2 = broadcast.create_receiver();
    auto recv3 = broadcast.create_receiver();

    broadcast.send(TestEvent{42});

    // Each receiver should get the event
    REQUIRE(recv1->receive()->value == 42);
    REQUIRE(recv2->receive()->value == 42);
    REQUIRE(recv3->receive()->value == 42);
}

TEST_CASE("BroadcastChannel: dead receiver cleanup", "[event][broadcast]") {
    BroadcastChannel<TestEvent> broadcast;

    auto recv1 = broadcast.create_receiver();
    {
        auto recv2 = broadcast.create_receiver();
        REQUIRE(broadcast.receiver_count() == 2);
    }
    // recv2 is now destroyed

    // Send should clean up dead receivers
    broadcast.send(TestEvent{1});

    // Only recv1 should remain
    REQUIRE(recv1->receive()->value == 1);
    REQUIRE(broadcast.receiver_count() == 1);
}

TEST_CASE("BroadcastChannel: multiple sends", "[event][broadcast]") {
    BroadcastChannel<TestEvent> broadcast;

    auto recv = broadcast.create_receiver();

    broadcast.send(TestEvent{1});
    broadcast.send(TestEvent{2});
    broadcast.send(TestEvent{3});

    auto events = recv->drain();
    REQUIRE(events.size() == 3);
    REQUIRE(events[0].value == 1);
    REQUIRE(events[1].value == 2);
    REQUIRE(events[2].value == 3);
}

TEST_CASE("MpscChannel: alias works", "[event][channel]") {
    MpscChannel<TestEvent> channel;

    channel.send(TestEvent{42});
    auto event = channel.receive();
    REQUIRE(event.has_value());
    REQUIRE(event->value == 42);
}
