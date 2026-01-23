#pragma once

/// @file bounded_queue.hpp
/// @brief Fixed-capacity lock-free ring buffer queue for void_structures
///
/// BoundedQueue provides a lock-free, thread-safe bounded MPMC queue.
/// Uses a circular buffer with power-of-2 capacity for efficient modulo.
/// Ideal for work-stealing, task scheduling, and fixed-size buffers.

#include <atomic>
#include <optional>
#include <vector>
#include <cstddef>
#include <memory>
#include <bit>
#include <cassert>
#include <new>

namespace void_structures {

/// Lock-free bounded MPMC ring buffer queue
/// @tparam T Stored value type (must be movable)
template<typename T>
class BoundedQueue {
private:
    struct Cell {
        std::atomic<std::size_t> sequence;
        std::aligned_storage_t<sizeof(T), alignof(T)> storage;

        T* get_ptr() noexcept {
            return std::launder(reinterpret_cast<T*>(&storage));
        }

        const T* get_ptr() const noexcept {
            return std::launder(reinterpret_cast<const T*>(&storage));
        }
    };

    std::unique_ptr<Cell[]> buffer_;
    std::size_t mask_;  // capacity - 1 (for efficient modulo)

    alignas(64) std::atomic<std::size_t> enqueue_pos_{0};
    alignas(64) std::atomic<std::size_t> dequeue_pos_{0};

    /// Round up to next power of 2
    [[nodiscard]] static constexpr std::size_t next_power_of_2(std::size_t n) noexcept {
        if (n == 0) return 1;
        return std::size_t{1} << (sizeof(std::size_t) * 8 - std::countl_zero(n - 1));
    }

public:
    using value_type = T;
    using size_type = std::size_t;

    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create bounded queue with given capacity
    /// @param capacity Maximum number of elements (rounded up to power of 2)
    explicit BoundedQueue(size_type capacity)
        : buffer_(std::make_unique<Cell[]>(next_power_of_2(capacity)))
        , mask_(next_power_of_2(capacity) - 1)
    {
        assert(capacity > 0 && "Capacity must be positive");

        // Initialize sequence numbers
        for (size_type i = 0; i <= mask_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    /// Destructor - cleans up remaining elements
    ~BoundedQueue() {
        // Destroy any remaining elements
        while (true) {
            size_type pos = dequeue_pos_.load(std::memory_order_relaxed);
            Cell& cell = buffer_[pos & mask_];
            size_type seq = cell.sequence.load(std::memory_order_acquire);

            if (static_cast<std::ptrdiff_t>(seq - (pos + 1)) < 0) {
                // Queue is empty
                break;
            }

            if (seq == pos + 1) {
                // Element exists - destroy it
                cell.get_ptr()->~T();
                dequeue_pos_.store(pos + 1, std::memory_order_relaxed);
            }
        }
    }

    // Non-copyable
    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    // Non-movable (due to atomics)
    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;

    // =========================================================================
    // Core Operations
    // =========================================================================

    /// Try to push value to queue
    /// @return true if successful, false if queue is full
    [[nodiscard]] bool try_push(T value) {
        Cell* cell;
        size_type pos = enqueue_pos_.load(std::memory_order_relaxed);

        while (true) {
            cell = &buffer_[pos & mask_];
            size_type seq = cell->sequence.load(std::memory_order_acquire);
            std::ptrdiff_t diff = static_cast<std::ptrdiff_t>(seq) -
                                  static_cast<std::ptrdiff_t>(pos);

            if (diff == 0) {
                // Cell is ready for enqueue
                if (enqueue_pos_.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                // Queue is full
                return false;
            } else {
                // Another thread advanced enqueue_pos
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }

        // Construct value in cell
        new (&cell->storage) T(std::move(value));
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    /// Push value, blocking if full (spins)
    void push(T value) {
        while (!try_push(std::move(value))) {
            // Spin - could add backoff here
        }
    }

    /// Alias for push()
    void enqueue(T value) {
        push(std::move(value));
    }

    /// Try to pop value from queue
    /// @return Value if available, nullopt if empty
    [[nodiscard]] std::optional<T> try_pop() {
        Cell* cell;
        size_type pos = dequeue_pos_.load(std::memory_order_relaxed);

        while (true) {
            cell = &buffer_[pos & mask_];
            size_type seq = cell->sequence.load(std::memory_order_acquire);
            std::ptrdiff_t diff = static_cast<std::ptrdiff_t>(seq) -
                                  static_cast<std::ptrdiff_t>(pos + 1);

            if (diff == 0) {
                // Cell has data ready
                if (dequeue_pos_.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                // Queue is empty
                return std::nullopt;
            } else {
                // Another thread advanced dequeue_pos
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }

        // Move value out and destroy
        T value = std::move(*cell->get_ptr());
        cell->get_ptr()->~T();
        cell->sequence.store(pos + mask_ + 1, std::memory_order_release);
        return value;
    }

    /// Pop value, blocking if empty (spins)
    [[nodiscard]] std::optional<T> pop() {
        return try_pop();
    }

    /// Alias for pop()
    [[nodiscard]] std::optional<T> dequeue() {
        return pop();
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Get the capacity of the queue
    [[nodiscard]] size_type capacity() const noexcept {
        return mask_ + 1;
    }

    /// Check if queue appears empty
    /// @note This is a snapshot - may change immediately after call
    [[nodiscard]] bool empty() const noexcept {
        size_type enq = enqueue_pos_.load(std::memory_order_relaxed);
        size_type deq = dequeue_pos_.load(std::memory_order_relaxed);
        return enq == deq;
    }

    /// Alias for empty()
    [[nodiscard]] bool is_empty() const noexcept {
        return empty();
    }

    /// Check if queue appears full
    /// @note This is a snapshot - may change immediately after call
    [[nodiscard]] bool full() const noexcept {
        size_type enq = enqueue_pos_.load(std::memory_order_relaxed);
        size_type deq = dequeue_pos_.load(std::memory_order_relaxed);
        return enq - deq >= capacity();
    }

    /// Alias for full()
    [[nodiscard]] bool is_full() const noexcept {
        return full();
    }

    /// Get approximate size
    /// @note This is a snapshot - may change immediately after call
    [[nodiscard]] size_type size() const noexcept {
        size_type enq = enqueue_pos_.load(std::memory_order_relaxed);
        size_type deq = dequeue_pos_.load(std::memory_order_relaxed);
        return enq >= deq ? enq - deq : 0;
    }

    /// Alias for size()
    [[nodiscard]] size_type len() const noexcept {
        return size();
    }

    // =========================================================================
    // Bulk Operations
    // =========================================================================

    /// Try to push multiple values
    /// @return Number of values successfully pushed
    template<typename InputIt>
    size_type try_push_batch(InputIt first, InputIt last) {
        size_type count = 0;
        for (; first != last; ++first) {
            if (!try_push(*first)) {
                break;
            }
            ++count;
        }
        return count;
    }

    /// Try to pop multiple values into output iterator
    /// @return Number of values successfully popped
    template<typename OutputIt>
    size_type try_pop_batch(OutputIt out, size_type max_count) {
        size_type count = 0;
        while (count < max_count) {
            auto item = try_pop();
            if (!item.has_value()) {
                break;
            }
            *out++ = std::move(*item);
            ++count;
        }
        return count;
    }
};

/// Type alias for common use case
template<typename T>
using RingBuffer = BoundedQueue<T>;

} // namespace void_structures
