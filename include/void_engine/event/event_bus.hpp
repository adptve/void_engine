#pragma once

/// @file event_bus.hpp
/// @brief Lock-free event bus for void_event
///
/// High-performance event system with:
/// - Lock-free event queues
/// - Priority-based delivery
/// - Typed and dynamic events
/// - Observer pattern support

#include "fwd.hpp"
#include <void_engine/core/id.hpp>
#include <void_engine/structures/lock_free_queue.hpp>

#include <algorithm>
#include <any>
#include <typeindex>
#include <functional>
#include <map>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <optional>

namespace void_event {

// =============================================================================
// Priority
// =============================================================================

/// Event priority for ordering delivery
enum class Priority : std::uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// =============================================================================
// SubscriberId
// =============================================================================

/// Unique identifier for a subscription
struct SubscriberId {
    std::uint64_t id = 0;

    constexpr SubscriberId() = default;
    constexpr explicit SubscriberId(std::uint64_t value) : id(value) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept { return id != 0; }

    constexpr auto operator<=>(const SubscriberId&) const noexcept = default;
    constexpr bool operator==(const SubscriberId&) const noexcept = default;
};

// =============================================================================
// EventEnvelope
// =============================================================================

/// Event envelope containing metadata and type-erased data
class EventEnvelope {
public:
    /// Event type identifier
    std::type_index type_id;
    /// Event data (type-erased)
    std::any data;
    /// Event priority
    Priority priority;
    /// Timestamp
    std::uint64_t timestamp;
    /// Optional source ID
    std::optional<void_core::Id> source;

    /// Create envelope from typed event
    template<typename E>
    static EventEnvelope create(E&& event, Priority prio, std::uint64_t ts) {
        EventEnvelope env;
        env.type_id = std::type_index(typeid(std::decay_t<E>));
        env.data = std::forward<E>(event);
        env.priority = prio;
        env.timestamp = ts;
        env.source = std::nullopt;
        return env;
    }

    /// Try to get the event as a specific type
    template<typename E>
    [[nodiscard]] const E* try_get() const {
        return std::any_cast<E>(&data);
    }

    /// Try to extract the event (move out)
    template<typename E>
    [[nodiscard]] std::optional<E> try_take() {
        if (auto* ptr = std::any_cast<E>(&data)) {
            return std::move(*ptr);
        }
        return std::nullopt;
    }

private:
    EventEnvelope() : type_id(typeid(void)) {}
};

// =============================================================================
// EventBus
// =============================================================================

/// Dynamic handler type
using DynamicHandler = std::function<void(const std::any&)>;

/// Event bus for publishing and subscribing to events
class EventBus {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create a new event bus
    EventBus() : m_next_subscriber_id(1), m_timestamp(0) {}

    // Non-copyable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // Movable
    EventBus(EventBus&&) = default;
    EventBus& operator=(EventBus&&) = default;

    // =========================================================================
    // Publishing
    // =========================================================================

    /// Publish an event with default (Normal) priority
    template<typename E>
    void publish(E&& event) {
        publish_with_priority(std::forward<E>(event), Priority::Normal);
    }

    /// Publish an event with specified priority
    template<typename E>
    void publish_with_priority(E&& event, Priority priority) {
        auto envelope = EventEnvelope::create(
            std::forward<E>(event),
            priority,
            m_timestamp.load(std::memory_order_relaxed)
        );
        m_queue.push(std::move(envelope));
    }

    // =========================================================================
    // Subscribing
    // =========================================================================

    /// Subscribe to an event type with default (Normal) priority
    template<typename E, typename F>
    SubscriberId subscribe(F&& handler) {
        return subscribe_with_priority<E>(std::forward<F>(handler), Priority::Normal);
    }

    /// Subscribe to an event type with specified priority
    template<typename E, typename F>
    SubscriberId subscribe_with_priority(F&& handler, Priority priority) {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);

        SubscriberId sub_id(m_next_subscriber_id++);
        std::type_index type_id(typeid(E));

        // Wrap handler to work with std::any
        DynamicHandler wrapped = [h = std::forward<F>(handler)](const std::any& data) {
            if (const E* event = std::any_cast<E>(&data)) {
                h(*event);
            }
        };

        auto& handlers = m_handlers[type_id];
        handlers.push_back({sub_id, priority, std::move(wrapped)});

        // Sort by priority (higher priority first)
        std::sort(handlers.begin(), handlers.end(),
            [](const auto& a, const auto& b) {
                return static_cast<int>(std::get<1>(a)) > static_cast<int>(std::get<1>(b));
            });

        return sub_id;
    }

    /// Unsubscribe from events
    void unsubscribe(SubscriberId id) {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);

        for (auto& [type_id, handlers] : m_handlers) {
            handlers.erase(
                std::remove_if(handlers.begin(), handlers.end(),
                    [id](const auto& entry) {
                        return std::get<0>(entry) == id;
                    }),
                handlers.end());
        }
    }

    // =========================================================================
    // Processing
    // =========================================================================

    /// Process all pending events
    void process() {
        // Collect all pending events
        std::vector<EventEnvelope> events;
        while (auto envelope = m_queue.pop()) {
            events.push_back(std::move(*envelope));
        }

        // Sort by priority (higher priority first)
        std::sort(events.begin(), events.end(),
            [](const EventEnvelope& a, const EventEnvelope& b) {
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            });

        // Dispatch events
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        for (const auto& envelope : events) {
            auto it = m_handlers.find(envelope.type_id);
            if (it != m_handlers.end()) {
                for (const auto& [sub_id, priority, handler] : it->second) {
                    handler(envelope.data);
                }
            }
        }

        m_timestamp.fetch_add(1, std::memory_order_relaxed);
    }

    /// Process up to max_events
    void process_batch(std::size_t max_events) {
        std::vector<EventEnvelope> events;
        events.reserve(max_events);

        for (std::size_t i = 0; i < max_events; ++i) {
            auto envelope = m_queue.pop();
            if (!envelope) break;
            events.push_back(std::move(*envelope));
        }

        std::sort(events.begin(), events.end(),
            [](const EventEnvelope& a, const EventEnvelope& b) {
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            });

        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        for (const auto& envelope : events) {
            auto it = m_handlers.find(envelope.type_id);
            if (it != m_handlers.end()) {
                for (const auto& [sub_id, priority, handler] : it->second) {
                    handler(envelope.data);
                }
            }
        }

        m_timestamp.fetch_add(1, std::memory_order_relaxed);
    }

    // =========================================================================
    // Queue Management
    // =========================================================================

    /// Clear all pending events without processing
    void clear() {
        while (m_queue.pop()) {}
    }

    /// Get pending event count (approximate)
    [[nodiscard]] std::size_t pending_count() const {
        return m_queue.size();
    }

    /// Check if there are pending events
    [[nodiscard]] bool has_pending() const {
        return !m_queue.empty();
    }

    /// Get current timestamp
    [[nodiscard]] std::uint64_t timestamp() const noexcept {
        return m_timestamp.load(std::memory_order_relaxed);
    }

private:
    using HandlerEntry = std::tuple<SubscriberId, Priority, DynamicHandler>;

    void_structures::LockFreeQueue<EventEnvelope> m_queue;
    std::map<std::type_index, std::vector<HandlerEntry>> m_handlers;
    std::mutex m_handlers_mutex;
    std::uint64_t m_next_subscriber_id;
    std::atomic<std::uint64_t> m_timestamp;
};

} // namespace void_event
