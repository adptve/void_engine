// void_structures Queue tests (LockFreeQueue and BoundedQueue)

#include <catch2/catch_test_macros.hpp>
#include <void_engine/structures/structures.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include <set>

using namespace void_structures;

// =============================================================================
// LockFreeQueue Basic Tests
// =============================================================================

TEST_CASE("LockFreeQueue construction", "[structures][queue][lockfree]") {
    LockFreeQueue<int> queue;
    REQUIRE(queue.empty());
    REQUIRE(queue.size() == 0);
}

TEST_CASE("LockFreeQueue push and pop", "[structures][queue][lockfree]") {
    LockFreeQueue<int> queue;

    SECTION("single push/pop") {
        queue.push(42);
        REQUIRE(queue.size() == 1);
        REQUIRE_FALSE(queue.empty());

        auto value = queue.pop();
        REQUIRE(value.has_value());
        REQUIRE(*value == 42);
        REQUIRE(queue.empty());
    }

    SECTION("multiple push/pop FIFO order") {
        queue.push(1);
        queue.push(2);
        queue.push(3);

        REQUIRE(*queue.pop() == 1);
        REQUIRE(*queue.pop() == 2);
        REQUIRE(*queue.pop() == 3);
    }

    SECTION("pop from empty returns nullopt") {
        auto value = queue.pop();
        REQUIRE_FALSE(value.has_value());
    }
}

TEST_CASE("LockFreeQueue aliases", "[structures][queue][lockfree]") {
    LockFreeQueue<int> queue;

    queue.enqueue(42);
    REQUIRE(queue.len() == 1);
    REQUIRE_FALSE(queue.is_empty());

    auto value = queue.dequeue();
    REQUIRE(value.has_value());
    REQUIRE(*value == 42);
}

TEST_CASE("LockFreeQueue bulk operations", "[structures][queue][lockfree]") {
    LockFreeQueue<int> queue;

    SECTION("push_range") {
        std::vector<int> values = {1, 2, 3, 4, 5};
        queue.push_range(values.begin(), values.end());
        REQUIRE(queue.size() == 5);
    }

    SECTION("push_range initializer list") {
        queue.push_range({10, 20, 30});
        REQUIRE(queue.size() == 3);
    }

    SECTION("pop_batch") {
        queue.push_range({1, 2, 3, 4, 5});

        std::vector<int> out;
        auto count = queue.pop_batch(std::back_inserter(out), 3);

        REQUIRE(count == 3);
        REQUIRE(out.size() == 3);
        REQUIRE(queue.size() == 2);
    }
}

TEST_CASE("LockFreeQueue with strings", "[structures][queue][lockfree]") {
    LockFreeQueue<std::string> queue;

    queue.push("hello");
    queue.push("world");

    REQUIRE(*queue.pop() == "hello");
    REQUIRE(*queue.pop() == "world");
}

// =============================================================================
// BoundedQueue Basic Tests
// =============================================================================

TEST_CASE("BoundedQueue construction", "[structures][queue][bounded]") {
    BoundedQueue<int> queue(16);

    REQUIRE(queue.empty());
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.capacity() >= 16);  // Rounded to power of 2
}

TEST_CASE("BoundedQueue capacity rounding", "[structures][queue][bounded]") {
    BoundedQueue<int> q10(10);
    BoundedQueue<int> q16(16);
    BoundedQueue<int> q17(17);

    // Capacity is rounded up to next power of 2
    REQUIRE(q10.capacity() == 16);
    REQUIRE(q16.capacity() == 16);
    REQUIRE(q17.capacity() == 32);
}

TEST_CASE("BoundedQueue push and pop", "[structures][queue][bounded]") {
    BoundedQueue<int> queue(8);

    SECTION("single push/pop") {
        REQUIRE(queue.try_push(42));
        REQUIRE(queue.size() == 1);

        auto value = queue.try_pop();
        REQUIRE(value.has_value());
        REQUIRE(*value == 42);
    }

    SECTION("FIFO order") {
        queue.try_push(1);
        queue.try_push(2);
        queue.try_push(3);

        REQUIRE(*queue.try_pop() == 1);
        REQUIRE(*queue.try_pop() == 2);
        REQUIRE(*queue.try_pop() == 3);
    }

    SECTION("pop empty returns nullopt") {
        auto value = queue.try_pop();
        REQUIRE_FALSE(value.has_value());
    }
}

TEST_CASE("BoundedQueue full behavior", "[structures][queue][bounded]") {
    BoundedQueue<int> queue(4);

    // Fill the queue
    REQUIRE(queue.try_push(1));
    REQUIRE(queue.try_push(2));
    REQUIRE(queue.try_push(3));
    REQUIRE(queue.try_push(4));

    REQUIRE(queue.full());
    REQUIRE_FALSE(queue.try_push(5));  // Should fail

    // After removing one, can push again
    queue.try_pop();
    REQUIRE_FALSE(queue.full());
    REQUIRE(queue.try_push(5));
}

TEST_CASE("BoundedQueue aliases", "[structures][queue][bounded]") {
    BoundedQueue<int> queue(8);

    queue.enqueue(42);
    REQUIRE(queue.len() == 1);
    REQUIRE_FALSE(queue.is_empty());
    REQUIRE_FALSE(queue.is_full());

    auto value = queue.dequeue();
    REQUIRE(value.has_value());
}

TEST_CASE("BoundedQueue wrap around", "[structures][queue][bounded]") {
    BoundedQueue<int> queue(4);

    // Fill partially
    queue.try_push(1);
    queue.try_push(2);

    // Remove first
    REQUIRE(*queue.try_pop() == 1);

    // Add more (should wrap around internally)
    queue.try_push(3);
    queue.try_push(4);
    queue.try_push(5);

    // Verify FIFO order maintained through wrap
    REQUIRE(*queue.try_pop() == 2);
    REQUIRE(*queue.try_pop() == 3);
    REQUIRE(*queue.try_pop() == 4);
    REQUIRE(*queue.try_pop() == 5);
}

TEST_CASE("BoundedQueue batch operations", "[structures][queue][bounded]") {
    BoundedQueue<int> queue(16);

    SECTION("try_push_batch") {
        std::vector<int> values = {1, 2, 3, 4, 5};
        auto count = queue.try_push_batch(values.begin(), values.end());
        REQUIRE(count == 5);
        REQUIRE(queue.size() == 5);
    }

    SECTION("try_pop_batch") {
        for (int i = 0; i < 5; ++i) {
            queue.try_push(i);
        }

        std::vector<int> out;
        auto count = queue.try_pop_batch(std::back_inserter(out), 3);

        REQUIRE(count == 3);
        REQUIRE(out.size() == 3);
    }
}

// =============================================================================
// Concurrent Tests (Basic - more thorough testing would use sanitizers)
// =============================================================================

TEST_CASE("LockFreeQueue concurrent basic", "[structures][queue][lockfree][concurrent]") {
    LockFreeQueue<int> queue;
    constexpr int NUM_ITEMS = 1000;
    std::atomic<int> sum{0};

    std::thread producer([&] {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            queue.push(i);
        }
    });

    std::thread consumer([&] {
        int count = 0;
        while (count < NUM_ITEMS) {
            auto value = queue.pop();
            if (value.has_value()) {
                sum += *value;
                ++count;
            }
        }
    });

    producer.join();
    consumer.join();

    // Sum of 0..999 = 499500
    REQUIRE(sum == (NUM_ITEMS * (NUM_ITEMS - 1)) / 2);
}

TEST_CASE("BoundedQueue concurrent basic", "[structures][queue][bounded][concurrent]") {
    BoundedQueue<int> queue(64);
    constexpr int NUM_ITEMS = 1000;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<int> sum{0};

    std::thread producer([&] {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            while (!queue.try_push(i)) {
                // Spin until we can push
            }
            ++produced;
        }
    });

    std::thread consumer([&] {
        int count = 0;
        while (count < NUM_ITEMS) {
            auto value = queue.try_pop();
            if (value.has_value()) {
                sum += *value;
                ++count;
            }
        }
        consumed = count;
    });

    producer.join();
    consumer.join();

    REQUIRE(produced == NUM_ITEMS);
    REQUIRE(consumed == NUM_ITEMS);
    REQUIRE(sum == (NUM_ITEMS * (NUM_ITEMS - 1)) / 2);
}

TEST_CASE("LockFreeQueue multiple producers", "[structures][queue][lockfree][concurrent]") {
    LockFreeQueue<int> queue;
    constexpr int ITEMS_PER_PRODUCER = 100;
    constexpr int NUM_PRODUCERS = 4;
    std::atomic<int> total_produced{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                queue.push(p * ITEMS_PER_PRODUCER + i);
                ++total_produced;
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }

    REQUIRE(total_produced == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    REQUIRE(queue.size() == NUM_PRODUCERS * ITEMS_PER_PRODUCER);

    // Drain and verify all unique
    std::set<int> values;
    while (auto v = queue.pop()) {
        values.insert(*v);
    }
    REQUIRE(values.size() == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
}

// =============================================================================
// RingBuffer Type Alias Test
// =============================================================================

TEST_CASE("RingBuffer alias", "[structures][queue][bounded]") {
    RingBuffer<int> buffer(8);

    buffer.try_push(1);
    buffer.try_push(2);

    REQUIRE(buffer.size() == 2);
    REQUIRE(*buffer.try_pop() == 1);
}
