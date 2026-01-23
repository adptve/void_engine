#pragma once

/// @file lock_free_queue.hpp
/// @brief Lock-free MPMC queue for void_structures
///
/// LockFreeQueue provides a lock-free, thread-safe unbounded queue.
/// Uses the Michael-Scott algorithm for MPMC (multiple-producer multiple-consumer).
/// Ideal for job systems, event queues, and inter-thread communication.

#include <atomic>
#include <optional>
#include <memory>
#include <cstddef>

namespace void_structures {

/// Lock-free unbounded MPMC queue
/// @tparam T Stored value type (must be movable)
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<Node*> next{nullptr};
        std::optional<T> value;

        Node() = default;
        explicit Node(T val) : value(std::move(val)) {}
    };

    // Sentinel node for easier implementation
    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;
    alignas(64) std::atomic<std::size_t> size_{0};

public:
    using value_type = T;
    using size_type = std::size_t;

    // =========================================================================
    // Constructors / Destructor
    // =========================================================================

    /// Create empty queue
    LockFreeQueue() {
        Node* sentinel = new Node();
        head_.store(sentinel, std::memory_order_relaxed);
        tail_.store(sentinel, std::memory_order_relaxed);
    }

    /// Destructor - cleans up all nodes
    ~LockFreeQueue() {
        // Drain remaining items
        while (pop().has_value()) {}

        // Delete sentinel
        delete head_.load(std::memory_order_relaxed);
    }

    // Non-copyable
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    // Movable (but requires external synchronization during move)
    LockFreeQueue(LockFreeQueue&& other) noexcept {
        Node* sentinel = new Node();
        head_.store(sentinel, std::memory_order_relaxed);
        tail_.store(sentinel, std::memory_order_relaxed);

        // Move all elements from other
        std::optional<T> item;
        while ((item = other.pop()).has_value()) {
            push(std::move(*item));
        }
    }

    LockFreeQueue& operator=(LockFreeQueue&& other) noexcept {
        if (this != &other) {
            // Clear current queue
            while (pop().has_value()) {}

            // Move all elements from other
            std::optional<T> item;
            while ((item = other.pop()).has_value()) {
                push(std::move(*item));
            }
        }
        return *this;
    }

    // =========================================================================
    // Core Operations
    // =========================================================================

    /// Push value to back of queue
    /// @param value Value to enqueue
    void push(T value) {
        Node* new_node = new Node(std::move(value));

        while (true) {
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);

            // Check if tail is still the actual tail
            if (tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // Try to link new node to tail
                    if (tail->next.compare_exchange_weak(
                            next, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        // Success - try to swing tail to new node
                        tail_.compare_exchange_strong(
                            tail, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        size_.fetch_add(1, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    // Tail is falling behind - try to advance it
                    tail_.compare_exchange_weak(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                }
            }
        }
    }

    /// Alias for push()
    void enqueue(T value) {
        push(std::move(value));
    }

    /// Pop value from front of queue
    /// @return Value if available, nullopt if empty
    [[nodiscard]] std::optional<T> pop() {
        while (true) {
            Node* head = head_.load(std::memory_order_acquire);
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = head->next.load(std::memory_order_acquire);

            // Check if head is still valid
            if (head == head_.load(std::memory_order_acquire)) {
                if (head == tail) {
                    // Queue might be empty
                    if (next == nullptr) {
                        // Queue is definitely empty
                        return std::nullopt;
                    }
                    // Tail is falling behind - advance it
                    tail_.compare_exchange_weak(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                } else {
                    // Queue is not empty - try to dequeue
                    if (next != nullptr) {
                        T value = std::move(*next->value);
                        if (head_.compare_exchange_weak(
                                head, next,
                                std::memory_order_release,
                                std::memory_order_relaxed)) {
                            // Success - delete old head
                            delete head;
                            size_.fetch_sub(1, std::memory_order_relaxed);
                            return value;
                        }
                    }
                }
            }
        }
    }

    /// Alias for pop()
    [[nodiscard]] std::optional<T> dequeue() {
        return pop();
    }

    /// Try to pop without blocking (same as pop for lock-free queue)
    [[nodiscard]] std::optional<T> try_pop() {
        return pop();
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Check if queue is empty
    /// @note This is a snapshot - may change immediately after call
    [[nodiscard]] bool empty() const noexcept {
        return size_.load(std::memory_order_relaxed) == 0;
    }

    /// Alias for empty()
    [[nodiscard]] bool is_empty() const noexcept {
        return empty();
    }

    /// Get approximate size
    /// @note This is a snapshot - may change immediately after call
    [[nodiscard]] size_type size() const noexcept {
        return size_.load(std::memory_order_relaxed);
    }

    /// Alias for size()
    [[nodiscard]] size_type len() const noexcept {
        return size();
    }

    // =========================================================================
    // Bulk Operations
    // =========================================================================

    /// Push multiple values
    template<typename InputIt>
    void push_range(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            push(*first);
        }
    }

    /// Push from initializer list
    void push_range(std::initializer_list<T> values) {
        for (const auto& v : values) {
            push(v);
        }
    }

    /// Pop up to max_count values into output iterator
    /// @return Number of values actually popped
    template<typename OutputIt>
    size_type pop_batch(OutputIt out, size_type max_count) {
        size_type count = 0;
        while (count < max_count) {
            auto item = pop();
            if (!item.has_value()) {
                break;
            }
            *out++ = std::move(*item);
            ++count;
        }
        return count;
    }
};

} // namespace void_structures
