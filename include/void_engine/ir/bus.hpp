#pragma once

/// @file bus.hpp
/// @brief Inter-thread patch bus for void_ir

#include "fwd.hpp"
#include "patch.hpp"
#include "transaction.hpp"
#include <cstdint>
#include <vector>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <optional>

namespace void_ir {

// =============================================================================
// SubscriptionId
// =============================================================================

/// Subscription handle for patch bus
struct SubscriptionId {
    std::uint64_t value = UINT64_MAX;

    constexpr SubscriptionId() noexcept = default;
    constexpr explicit SubscriptionId(std::uint64_t v) noexcept : value(v) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value != UINT64_MAX;
    }

    [[nodiscard]] static constexpr SubscriptionId invalid() noexcept {
        return SubscriptionId{};
    }

    constexpr bool operator==(const SubscriptionId& other) const noexcept = default;
};

// =============================================================================
// PatchFilter
// =============================================================================

/// Filter for patch subscriptions
struct PatchFilter {
    /// Filter by namespace (nullopt = all)
    std::optional<NamespaceId> namespace_id;

    /// Filter by patch kind (empty = all)
    std::vector<PatchKind> kinds;

    /// Filter by entity (nullopt = all)
    std::optional<EntityRef> entity;

    /// Filter by component type (empty = all)
    std::vector<std::string> component_types;

    /// Create filter for all patches
    [[nodiscard]] static PatchFilter all() {
        return PatchFilter{};
    }

    /// Create filter for specific namespace
    [[nodiscard]] static PatchFilter for_namespace(NamespaceId ns) {
        PatchFilter f;
        f.namespace_id = ns;
        return f;
    }

    /// Create filter for specific entity
    [[nodiscard]] static PatchFilter for_entity(EntityRef ref) {
        PatchFilter f;
        f.entity = ref;
        return f;
    }

    /// Create filter for specific patch kinds
    [[nodiscard]] static PatchFilter for_kinds(std::vector<PatchKind> kinds) {
        PatchFilter f;
        f.kinds = std::move(kinds);
        return f;
    }

    /// Create filter for component patches
    [[nodiscard]] static PatchFilter for_components(std::vector<std::string> types) {
        PatchFilter f;
        f.kinds = {PatchKind::Component};
        f.component_types = std::move(types);
        return f;
    }

    /// Check if patch matches filter
    [[nodiscard]] bool matches(const Patch& patch, NamespaceId patch_ns) const {
        // Check namespace
        if (namespace_id && *namespace_id != patch_ns) {
            return false;
        }

        // Check kind
        if (!kinds.empty()) {
            bool kind_match = false;
            for (PatchKind k : kinds) {
                if (k == patch.kind()) {
                    kind_match = true;
                    break;
                }
            }
            if (!kind_match) {
                return false;
            }
        }

        // Check entity
        if (entity) {
            auto target = patch.target_entity();
            if (!target || *target != *entity) {
                return false;
            }
        }

        // Check component type
        if (!component_types.empty() && patch.is<ComponentPatch>()) {
            const auto& cp = patch.as<ComponentPatch>();
            bool type_match = false;
            for (const auto& type : component_types) {
                if (type == cp.component_type) {
                    type_match = true;
                    break;
                }
            }
            if (!type_match) {
                return false;
            }
        }

        return true;
    }
};

// =============================================================================
// PatchEvent
// =============================================================================

/// Event containing a patch and metadata
struct PatchEvent {
    Patch patch;
    NamespaceId namespace_id;
    TransactionId transaction_id;
    std::size_t sequence_number = 0;

    PatchEvent(Patch p, NamespaceId ns, TransactionId tx, std::size_t seq)
        : patch(std::move(p))
        , namespace_id(ns)
        , transaction_id(tx)
        , sequence_number(seq) {}
};

// =============================================================================
// PatchBus
// =============================================================================

/// Thread-safe patch event bus
class PatchBus {
public:
    using Callback = std::function<void(const PatchEvent&)>;

    /// Default constructor
    PatchBus() = default;

    /// Destructor
    ~PatchBus() {
        shutdown();
    }

    // Non-copyable
    PatchBus(const PatchBus&) = delete;
    PatchBus& operator=(const PatchBus&) = delete;

    /// Subscribe to patches with filter
    [[nodiscard]] SubscriptionId subscribe(PatchFilter filter, Callback callback) {
        std::unique_lock lock(m_mutex);

        SubscriptionId id(m_next_subscription_id++);
        m_subscriptions.emplace(id.value, Subscription{
            std::move(filter),
            std::move(callback)
        });

        return id;
    }

    /// Unsubscribe
    bool unsubscribe(SubscriptionId id) {
        std::unique_lock lock(m_mutex);
        return m_subscriptions.erase(id.value) > 0;
    }

    /// Publish a single patch
    void publish(Patch patch, NamespaceId ns, TransactionId tx = TransactionId::invalid()) {
        std::shared_lock lock(m_mutex);

        std::size_t seq = m_sequence_number.fetch_add(1, std::memory_order_relaxed);
        PatchEvent event(std::move(patch), ns, tx, seq);

        for (auto& [id, sub] : m_subscriptions) {
            if (sub.filter.matches(event.patch, ns)) {
                sub.callback(event);
            }
        }
    }

    /// Publish a batch of patches
    void publish_batch(const PatchBatch& batch, NamespaceId ns,
                       TransactionId tx = TransactionId::invalid()) {
        std::shared_lock lock(m_mutex);

        for (const auto& patch : batch) {
            std::size_t seq = m_sequence_number.fetch_add(1, std::memory_order_relaxed);
            PatchEvent event(Patch(patch), ns, tx, seq);

            for (auto& [id, sub] : m_subscriptions) {
                if (sub.filter.matches(event.patch, ns)) {
                    sub.callback(event);
                }
            }
        }
    }

    /// Publish a transaction (all patches in order)
    void publish_transaction(const Transaction& tx) {
        publish_batch(tx.patches(), tx.namespace_id(), tx.id());
    }

    /// Get current sequence number
    [[nodiscard]] std::size_t sequence_number() const noexcept {
        return m_sequence_number.load(std::memory_order_relaxed);
    }

    /// Get subscription count
    [[nodiscard]] std::size_t subscription_count() const {
        std::shared_lock lock(m_mutex);
        return m_subscriptions.size();
    }

    /// Shutdown the bus
    void shutdown() {
        std::unique_lock lock(m_mutex);
        m_subscriptions.clear();
    }

private:
    struct Subscription {
        PatchFilter filter;
        Callback callback;
    };

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::uint64_t, Subscription> m_subscriptions;
    std::uint64_t m_next_subscription_id = 0;
    std::atomic<std::size_t> m_sequence_number{0};
};

// =============================================================================
// AsyncPatchBus
// =============================================================================

/// Async patch bus with queue for decoupled consumption
class AsyncPatchBus {
public:
    /// Default constructor
    AsyncPatchBus() = default;

    /// Destructor
    ~AsyncPatchBus() {
        shutdown();
    }

    // Non-copyable
    AsyncPatchBus(const AsyncPatchBus&) = delete;
    AsyncPatchBus& operator=(const AsyncPatchBus&) = delete;

    /// Publish a patch (non-blocking)
    void publish(Patch patch, NamespaceId ns, TransactionId tx = TransactionId::invalid()) {
        std::size_t seq = m_sequence_number.fetch_add(1, std::memory_order_relaxed);

        {
            std::lock_guard lock(m_queue_mutex);
            m_queue.emplace(std::move(patch), ns, tx, seq);
        }

        m_condition.notify_one();
    }

    /// Publish a batch
    void publish_batch(const PatchBatch& batch, NamespaceId ns,
                       TransactionId tx = TransactionId::invalid()) {
        {
            std::lock_guard lock(m_queue_mutex);
            for (const auto& patch : batch) {
                std::size_t seq = m_sequence_number.fetch_add(1, std::memory_order_relaxed);
                m_queue.emplace(Patch(patch), ns, tx, seq);
            }
        }

        m_condition.notify_all();
    }

    /// Try to consume a patch (non-blocking)
    [[nodiscard]] std::optional<PatchEvent> try_consume() {
        std::lock_guard lock(m_queue_mutex);

        if (m_queue.empty()) {
            return std::nullopt;
        }

        PatchEvent event = std::move(m_queue.front());
        m_queue.pop();
        return event;
    }

    /// Consume a patch (blocking)
    [[nodiscard]] std::optional<PatchEvent> consume() {
        std::unique_lock lock(m_queue_mutex);

        m_condition.wait(lock, [this] {
            return !m_queue.empty() || m_shutdown;
        });

        if (m_shutdown && m_queue.empty()) {
            return std::nullopt;
        }

        PatchEvent event = std::move(m_queue.front());
        m_queue.pop();
        return event;
    }

    /// Consume with timeout
    [[nodiscard]] std::optional<PatchEvent> consume_timeout(
        std::chrono::milliseconds timeout) {
        std::unique_lock lock(m_queue_mutex);

        bool ready = m_condition.wait_for(lock, timeout, [this] {
            return !m_queue.empty() || m_shutdown;
        });

        if (!ready || (m_shutdown && m_queue.empty())) {
            return std::nullopt;
        }

        PatchEvent event = std::move(m_queue.front());
        m_queue.pop();
        return event;
    }

    /// Consume all available patches
    [[nodiscard]] std::vector<PatchEvent> consume_all() {
        std::lock_guard lock(m_queue_mutex);

        std::vector<PatchEvent> events;
        events.reserve(m_queue.size());

        while (!m_queue.empty()) {
            events.push_back(std::move(m_queue.front()));
            m_queue.pop();
        }

        return events;
    }

    /// Get queue size
    [[nodiscard]] std::size_t queue_size() const {
        std::lock_guard lock(m_queue_mutex);
        return m_queue.size();
    }

    /// Check if queue is empty
    [[nodiscard]] bool empty() const {
        std::lock_guard lock(m_queue_mutex);
        return m_queue.empty();
    }

    /// Shutdown the bus (wake up waiting consumers)
    void shutdown() {
        {
            std::lock_guard lock(m_queue_mutex);
            m_shutdown = true;
        }
        m_condition.notify_all();
    }

    /// Check if shutdown
    [[nodiscard]] bool is_shutdown() const {
        std::lock_guard lock(m_queue_mutex);
        return m_shutdown;
    }

private:
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_condition;
    std::queue<PatchEvent> m_queue;
    std::atomic<std::size_t> m_sequence_number{0};
    bool m_shutdown = false;
};

} // namespace void_ir

// Hash specialization
template<>
struct std::hash<void_ir::SubscriptionId> {
    std::size_t operator()(const void_ir::SubscriptionId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
