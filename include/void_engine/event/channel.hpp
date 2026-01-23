#pragma once

/// @file channel.hpp
/// @brief Typed event channels for void_event
///
/// EventChannel provides a lock-free, typed queue for single-type events.
/// Use when you need a simpler, more efficient alternative to EventBus
/// for specific event types.

#include "fwd.hpp"
#include <void_engine/structures/lock_free_queue.hpp>

#include <vector>
#include <optional>

namespace void_event {

/// Lock-free typed event channel
/// @tparam E Event type
template<typename E>
class EventChannel {
public:
    using value_type = E;
    using size_type = std::size_t;

    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create a new channel
    EventChannel() = default;

    // Non-copyable
    EventChannel(const EventChannel&) = delete;
    EventChannel& operator=(const EventChannel&) = delete;

    // Movable
    EventChannel(EventChannel&&) = default;
    EventChannel& operator=(EventChannel&&) = default;

    // =========================================================================
    // Send / Receive
    // =========================================================================

    /// Send an event to the channel
    void send(E event) {
        m_queue.push(std::move(event));
    }

    /// Receive an event from the channel
    /// @return Event if available, nullopt if empty
    [[nodiscard]] std::optional<E> receive() {
        return m_queue.pop();
    }

    /// Try to receive (alias for receive)
    [[nodiscard]] std::optional<E> try_receive() {
        return m_queue.pop();
    }

    // =========================================================================
    // Bulk Operations
    // =========================================================================

    /// Drain all events from the channel
    [[nodiscard]] std::vector<E> drain() {
        std::vector<E> events;
        while (auto event = m_queue.pop()) {
            events.push_back(std::move(*event));
        }
        return events;
    }

    /// Drain up to max_count events
    [[nodiscard]] std::vector<E> drain_batch(size_type max_count) {
        std::vector<E> events;
        events.reserve(max_count);
        for (size_type i = 0; i < max_count; ++i) {
            auto event = m_queue.pop();
            if (!event) break;
            events.push_back(std::move(*event));
        }
        return events;
    }

    /// Send multiple events
    template<typename InputIt>
    void send_batch(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            m_queue.push(*first);
        }
    }

    /// Send multiple events from initializer list
    void send_batch(std::initializer_list<E> events) {
        for (const auto& e : events) {
            m_queue.push(e);
        }
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Check if channel is empty
    [[nodiscard]] bool empty() const noexcept {
        return m_queue.empty();
    }

    /// Alias for empty()
    [[nodiscard]] bool is_empty() const noexcept {
        return m_queue.empty();
    }

    /// Get pending event count (approximate)
    [[nodiscard]] size_type size() const noexcept {
        return m_queue.size();
    }

    /// Alias for size()
    [[nodiscard]] size_type len() const noexcept {
        return m_queue.size();
    }

    // =========================================================================
    // Iteration Support
    // =========================================================================

    /// Process each event with a callable
    /// @param func Callable taking E&
    /// @return Number of events processed
    template<typename F>
    size_type for_each(F&& func) {
        size_type count = 0;
        while (auto event = m_queue.pop()) {
            func(*event);
            ++count;
        }
        return count;
    }

    /// Process each event, stopping if func returns false
    /// @param func Callable taking E& and returning bool
    /// @return Number of events processed before stop
    template<typename F>
    size_type for_each_while(F&& func) {
        size_type count = 0;
        while (auto event = m_queue.pop()) {
            ++count;
            if (!func(*event)) {
                break;
            }
        }
        return count;
    }

private:
    void_structures::LockFreeQueue<E> m_queue;
};

// =============================================================================
// Multi-Producer Single-Consumer Channel
// =============================================================================

/// MPSC variant with a single reader
/// Same API as EventChannel but explicitly documents usage pattern
template<typename E>
using MpscChannel = EventChannel<E>;

// =============================================================================
// Broadcast Channel
// =============================================================================

/// Broadcast channel that delivers events to all receivers
template<typename E>
class BroadcastChannel {
public:
    using value_type = E;
    using size_type = std::size_t;
    using Receiver = std::shared_ptr<EventChannel<E>>;

    /// Create a new broadcast channel
    BroadcastChannel() = default;

    /// Create a new receiver
    [[nodiscard]] Receiver create_receiver() {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto receiver = std::make_shared<EventChannel<E>>();
        m_receivers.push_back(receiver);
        return receiver;
    }

    /// Send event to all receivers
    void send(const E& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Remove dead receivers
        m_receivers.erase(
            std::remove_if(m_receivers.begin(), m_receivers.end(),
                [](const std::weak_ptr<EventChannel<E>>& wp) {
                    return wp.expired();
                }),
            m_receivers.end());

        // Send to live receivers
        for (auto& weak_recv : m_receivers) {
            if (auto recv = weak_recv.lock()) {
                recv->send(event);
            }
        }
    }

    /// Get receiver count (approximate)
    [[nodiscard]] size_type receiver_count() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_receivers.size();
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::weak_ptr<EventChannel<E>>> m_receivers;
};

} // namespace void_event
