#pragma once

/// @file event_bus.hpp
/// @brief Inter-service communication via publish/subscribe events
///
/// The EventBus provides:
/// - Type-safe event publishing and subscription
/// - Event priorities for ordered processing
/// - Queued event processing for deferred handling
/// - Thread-safe access
/// - Wildcard subscriptions for event categories

#include <algorithm>
#include <any>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace void_services {

// =============================================================================
// Event Priority
// =============================================================================

/// Priority levels for event processing
enum class EventPriority : std::uint8_t {
    Low = 0,      ///< Low priority, processed last
    Normal = 1,   ///< Default priority
    High = 2,     ///< High priority, processed before normal
    Critical = 3, ///< Critical priority, processed first
};

// =============================================================================
// Event Base
// =============================================================================

/// Unique subscription identifier
struct SubscriptionId {
    std::uint64_t id;

    bool operator==(const SubscriptionId& other) const { return id == other.id; }
    bool operator!=(const SubscriptionId& other) const { return id != other.id; }
};

/// Base event interface for type-erased storage
class IEvent {
public:
    virtual ~IEvent() = default;
    [[nodiscard]] virtual std::type_index type() const = 0;
    [[nodiscard]] virtual const std::string& category() const = 0;
    [[nodiscard]] virtual EventPriority priority() const = 0;
    [[nodiscard]] virtual std::chrono::steady_clock::time_point timestamp() const = 0;
};

/// Typed event wrapper
template <typename T>
class TypedEvent : public IEvent {
public:
    explicit TypedEvent(T data, std::string category = "", EventPriority prio = EventPriority::Normal)
        : m_data(std::move(data))
        , m_category(std::move(category))
        , m_priority(prio)
        , m_timestamp(std::chrono::steady_clock::now()) {}

    [[nodiscard]] std::type_index type() const override { return typeid(T); }
    [[nodiscard]] const std::string& category() const override { return m_category; }
    [[nodiscard]] EventPriority priority() const override { return m_priority; }
    [[nodiscard]] std::chrono::steady_clock::time_point timestamp() const override { return m_timestamp; }
    [[nodiscard]] const T& data() const { return m_data; }
    [[nodiscard]] T& data() { return m_data; }

private:
    T m_data;
    std::string m_category;
    EventPriority m_priority;
    std::chrono::steady_clock::time_point m_timestamp;
};

// =============================================================================
// Event Handler
// =============================================================================

/// Type-erased event handler
class IEventHandler {
public:
    virtual ~IEventHandler() = default;
    [[nodiscard]] virtual std::type_index event_type() const = 0;
    [[nodiscard]] virtual EventPriority priority() const = 0;
    virtual void handle(const IEvent& event) = 0;
};

/// Typed event handler
template <typename T>
class TypedEventHandler : public IEventHandler {
public:
    using HandlerFunc = std::function<void(const T&)>;

    explicit TypedEventHandler(HandlerFunc handler, EventPriority prio = EventPriority::Normal)
        : m_handler(std::move(handler)), m_priority(prio) {}

    [[nodiscard]] std::type_index event_type() const override { return typeid(T); }
    [[nodiscard]] EventPriority priority() const override { return m_priority; }

    void handle(const IEvent& event) override {
        if (auto* typed = dynamic_cast<const TypedEvent<T>*>(&event)) {
            m_handler(typed->data());
        }
    }

private:
    HandlerFunc m_handler;
    EventPriority m_priority;
};

// =============================================================================
// Event Statistics
// =============================================================================

/// Statistics for event bus operations
struct EventBusStats {
    std::uint64_t events_published = 0;
    std::uint64_t events_queued = 0;
    std::uint64_t events_processed = 0;
    std::uint64_t events_dropped = 0;
    std::size_t active_subscriptions = 0;
    std::size_t queue_size = 0;
    std::size_t max_queue_size = 0;
};

// =============================================================================
// Event Bus
// =============================================================================

/// Central event bus for publish/subscribe communication
class EventBus {
public:
    /// Configuration for the event bus
    struct Config {
        std::size_t max_queue_size = 10000;    ///< Maximum queued events
        bool drop_on_queue_full = true;         ///< Drop events when queue is full
        bool process_immediate = false;         ///< Process events immediately on publish
    };

    /// Default constructor
    EventBus() : m_config{}, m_next_subscription_id(1), m_enabled(true) {}

    /// Constructor with custom config
    explicit EventBus(Config config)
        : m_config(config), m_next_subscription_id(1), m_enabled(true) {}

    ~EventBus() = default;

    // Non-copyable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // =========================================================================
    // Subscription
    // =========================================================================

    /// Subscribe to events of a specific type
    /// @tparam T Event data type
    /// @param handler Callback to invoke when event is published
    /// @param priority Handler priority (higher priority handlers invoked first)
    /// @return Subscription ID for later unsubscription
    template <typename T>
    [[nodiscard]] SubscriptionId subscribe(
        std::function<void(const T&)> handler,
        EventPriority priority = EventPriority::Normal) {

        auto typed_handler = std::make_shared<TypedEventHandler<T>>(
            std::move(handler), priority);

        std::unique_lock lock(m_mutex);

        SubscriptionId id{m_next_subscription_id++};
        std::type_index type_idx = typeid(T);

        m_handlers[type_idx].push_back({id, typed_handler});

        // Sort handlers by priority (higher priority first)
        std::sort(m_handlers[type_idx].begin(), m_handlers[type_idx].end(),
            [](const auto& a, const auto& b) {
                return static_cast<int>(a.handler->priority()) >
                       static_cast<int>(b.handler->priority());
            });

        return id;
    }

    /// Subscribe to events matching a category pattern
    /// @param category Category to match (supports trailing * for wildcard)
    /// @param handler Callback receiving the raw IEvent
    /// @param priority Handler priority
    /// @return Subscription ID
    [[nodiscard]] SubscriptionId subscribe_category(
        std::string category,
        std::function<void(const IEvent&)> handler,
        EventPriority priority = EventPriority::Normal) {

        std::unique_lock lock(m_mutex);

        SubscriptionId id{m_next_subscription_id++};
        m_category_handlers.push_back({id, std::move(category), std::move(handler), priority});

        // Sort by priority
        std::sort(m_category_handlers.begin(), m_category_handlers.end(),
            [](const auto& a, const auto& b) {
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            });

        return id;
    }

    /// Unsubscribe from events
    /// @param id Subscription ID returned from subscribe
    /// @return true if subscription was found and removed
    bool unsubscribe(SubscriptionId id) {
        std::unique_lock lock(m_mutex);

        // Check type handlers
        for (auto& [type, handlers] : m_handlers) {
            auto it = std::find_if(handlers.begin(), handlers.end(),
                [id](const auto& h) { return h.id == id; });
            if (it != handlers.end()) {
                handlers.erase(it);
                return true;
            }
        }

        // Check category handlers
        auto it = std::find_if(m_category_handlers.begin(), m_category_handlers.end(),
            [id](const auto& h) { return h.id == id; });
        if (it != m_category_handlers.end()) {
            m_category_handlers.erase(it);
            return true;
        }

        return false;
    }

    // =========================================================================
    // Publishing
    // =========================================================================

    /// Publish an event immediately to all subscribers
    /// @tparam T Event data type
    /// @param data Event data
    /// @param category Optional category for category-based subscriptions
    /// @param priority Event priority
    template <typename T>
    void publish(T data, std::string category = "", EventPriority priority = EventPriority::Normal) {
        if (!m_enabled.load()) return;

        auto event = std::make_shared<TypedEvent<T>>(std::move(data), std::move(category), priority);

        if (m_config.process_immediate) {
            dispatch_event(*event);
        } else {
            queue_event(event);
        }

        m_stats.events_published++;
    }

    /// Queue an event for later processing
    /// @tparam T Event data type
    /// @param data Event data
    /// @param category Optional category
    /// @param priority Event priority
    template <typename T>
    void queue(T data, std::string category = "", EventPriority priority = EventPriority::Normal) {
        if (!m_enabled.load()) return;

        auto event = std::make_shared<TypedEvent<T>>(std::move(data), std::move(category), priority);
        queue_event(event);
    }

    /// Process all queued events
    /// @return Number of events processed
    std::size_t process_queue() {
        std::vector<std::shared_ptr<IEvent>> events;

        {
            std::unique_lock lock(m_queue_mutex);

            // Extract all events from priority queues
            while (!m_event_queue_critical.empty()) {
                events.push_back(m_event_queue_critical.front());
                m_event_queue_critical.pop();
            }
            while (!m_event_queue_high.empty()) {
                events.push_back(m_event_queue_high.front());
                m_event_queue_high.pop();
            }
            while (!m_event_queue_normal.empty()) {
                events.push_back(m_event_queue_normal.front());
                m_event_queue_normal.pop();
            }
            while (!m_event_queue_low.empty()) {
                events.push_back(m_event_queue_low.front());
                m_event_queue_low.pop();
            }

            m_stats.queue_size = 0;
        }

        // Dispatch events outside the lock
        for (const auto& event : events) {
            dispatch_event(*event);
            m_stats.events_processed++;
        }

        return events.size();
    }

    /// Process up to max_events from the queue
    /// @param max_events Maximum events to process
    /// @return Number of events processed
    std::size_t process_queue(std::size_t max_events) {
        std::vector<std::shared_ptr<IEvent>> events;
        events.reserve(max_events);

        {
            std::unique_lock lock(m_queue_mutex);

            // Extract events respecting priority order
            auto extract = [&](std::queue<std::shared_ptr<IEvent>>& q) {
                while (!q.empty() && events.size() < max_events) {
                    events.push_back(q.front());
                    q.pop();
                }
            };

            extract(m_event_queue_critical);
            extract(m_event_queue_high);
            extract(m_event_queue_normal);
            extract(m_event_queue_low);

            m_stats.queue_size = m_event_queue_critical.size() +
                                  m_event_queue_high.size() +
                                  m_event_queue_normal.size() +
                                  m_event_queue_low.size();
        }

        // Dispatch events outside the lock
        for (const auto& event : events) {
            dispatch_event(*event);
            m_stats.events_processed++;
        }

        return events.size();
    }

    // =========================================================================
    // Control
    // =========================================================================

    /// Enable or disable event processing
    void set_enabled(bool enabled) { m_enabled.store(enabled); }

    /// Check if event processing is enabled
    [[nodiscard]] bool is_enabled() const { return m_enabled.load(); }

    /// Clear all queued events
    void clear_queue() {
        std::unique_lock lock(m_queue_mutex);
        std::queue<std::shared_ptr<IEvent>> empty1, empty2, empty3, empty4;
        std::swap(m_event_queue_critical, empty1);
        std::swap(m_event_queue_high, empty2);
        std::swap(m_event_queue_normal, empty3);
        std::swap(m_event_queue_low, empty4);
        m_stats.queue_size = 0;
    }

    /// Remove all subscriptions
    void clear_subscriptions() {
        std::unique_lock lock(m_mutex);
        m_handlers.clear();
        m_category_handlers.clear();
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get event bus statistics
    [[nodiscard]] EventBusStats stats() const {
        EventBusStats s = m_stats;

        std::shared_lock lock(m_mutex);
        for (const auto& [type, handlers] : m_handlers) {
            s.active_subscriptions += handlers.size();
        }
        s.active_subscriptions += m_category_handlers.size();

        return s;
    }

    /// Get current queue size
    [[nodiscard]] std::size_t queue_size() const {
        std::unique_lock lock(m_queue_mutex);
        return m_event_queue_critical.size() +
               m_event_queue_high.size() +
               m_event_queue_normal.size() +
               m_event_queue_low.size();
    }

    /// Get the configuration
    [[nodiscard]] const Config& config() const { return m_config; }

private:
    /// Queue an event for later processing
    void queue_event(std::shared_ptr<IEvent> event) {
        std::unique_lock lock(m_queue_mutex);

        std::size_t current_size = m_event_queue_critical.size() +
                                    m_event_queue_high.size() +
                                    m_event_queue_normal.size() +
                                    m_event_queue_low.size();

        if (current_size >= m_config.max_queue_size) {
            if (m_config.drop_on_queue_full) {
                m_stats.events_dropped++;
                return;
            }
        }

        // Queue based on priority
        switch (event->priority()) {
            case EventPriority::Critical:
                m_event_queue_critical.push(event);
                break;
            case EventPriority::High:
                m_event_queue_high.push(event);
                break;
            case EventPriority::Normal:
                m_event_queue_normal.push(event);
                break;
            case EventPriority::Low:
                m_event_queue_low.push(event);
                break;
        }

        m_stats.events_queued++;
        m_stats.queue_size = current_size + 1;
        if (m_stats.queue_size > m_stats.max_queue_size) {
            m_stats.max_queue_size = m_stats.queue_size;
        }
    }

    /// Dispatch an event to all matching handlers
    void dispatch_event(const IEvent& event) {
        std::shared_lock lock(m_mutex);

        // Dispatch to type handlers
        auto type_idx = event.type();
        auto it = m_handlers.find(type_idx);
        if (it != m_handlers.end()) {
            for (const auto& handler : it->second) {
                handler.handler->handle(event);
            }
        }

        // Dispatch to category handlers
        const auto& cat = event.category();
        for (const auto& handler : m_category_handlers) {
            if (matches_category(handler.category, cat)) {
                handler.handler(event);
            }
        }
    }

    /// Check if a category pattern matches an event category
    [[nodiscard]] static bool matches_category(const std::string& pattern, const std::string& category) {
        if (pattern.empty()) return true;
        if (pattern == category) return true;

        // Wildcard matching (e.g., "audio.*" matches "audio.play", "audio.stop")
        if (!pattern.empty() && pattern.back() == '*') {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            return category.compare(0, prefix.size(), prefix) == 0;
        }

        return false;
    }

    /// Handler entry with ID
    struct HandlerEntry {
        SubscriptionId id;
        std::shared_ptr<IEventHandler> handler;
    };

    /// Category handler entry
    struct CategoryHandlerEntry {
        SubscriptionId id;
        std::string category;
        std::function<void(const IEvent&)> handler;
        EventPriority priority;
    };

    Config m_config;
    std::atomic<std::uint64_t> m_next_subscription_id;
    std::atomic<bool> m_enabled;

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_handlers;
    std::vector<CategoryHandlerEntry> m_category_handlers;

    mutable std::mutex m_queue_mutex;
    std::queue<std::shared_ptr<IEvent>> m_event_queue_critical;
    std::queue<std::shared_ptr<IEvent>> m_event_queue_high;
    std::queue<std::shared_ptr<IEvent>> m_event_queue_normal;
    std::queue<std::shared_ptr<IEvent>> m_event_queue_low;

    mutable EventBusStats m_stats;
};

// =============================================================================
// Shared Event Bus
// =============================================================================

/// Thread-safe shared event bus wrapper
class SharedEventBus {
public:
    SharedEventBus() : m_bus(std::make_shared<EventBus>()) {}
    explicit SharedEventBus(EventBus::Config config)
        : m_bus(std::make_shared<EventBus>(config)) {}

    /// Get the underlying event bus
    [[nodiscard]] std::shared_ptr<EventBus> get() const { return m_bus; }
    [[nodiscard]] EventBus* operator->() const { return m_bus.get(); }
    [[nodiscard]] EventBus& operator*() const { return *m_bus; }

private:
    std::shared_ptr<EventBus> m_bus;
};

// =============================================================================
// RAII Subscription Guard
// =============================================================================

/// RAII guard that automatically unsubscribes when destroyed
class SubscriptionGuard {
public:
    SubscriptionGuard() = default;

    SubscriptionGuard(std::shared_ptr<EventBus> bus, SubscriptionId id)
        : m_bus(std::move(bus)), m_id(id) {}

    ~SubscriptionGuard() {
        if (auto bus = m_bus.lock()) {
            bus->unsubscribe(m_id);
        }
    }

    // Move-only
    SubscriptionGuard(SubscriptionGuard&& other) noexcept
        : m_bus(std::move(other.m_bus)), m_id(other.m_id) {
        other.m_bus.reset();
    }

    SubscriptionGuard& operator=(SubscriptionGuard&& other) noexcept {
        if (this != &other) {
            if (auto bus = m_bus.lock()) {
                bus->unsubscribe(m_id);
            }
            m_bus = std::move(other.m_bus);
            m_id = other.m_id;
            other.m_bus.reset();
        }
        return *this;
    }

    SubscriptionGuard(const SubscriptionGuard&) = delete;
    SubscriptionGuard& operator=(const SubscriptionGuard&) = delete;

    /// Release ownership without unsubscribing
    SubscriptionId release() {
        m_bus.reset();
        return m_id;
    }

    /// Get the subscription ID
    [[nodiscard]] SubscriptionId id() const { return m_id; }

private:
    std::weak_ptr<EventBus> m_bus;
    SubscriptionId m_id{0};
};

// =============================================================================
// Convenience Functions
// =============================================================================

/// Create a subscription guard for automatic cleanup
[[nodiscard]] inline SubscriptionGuard make_subscription_guard(
    std::shared_ptr<EventBus> bus, SubscriptionId id) {
    return SubscriptionGuard(std::move(bus), id);
}

} // namespace void_services
