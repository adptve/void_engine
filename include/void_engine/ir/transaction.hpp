#pragma once

/// @file transaction.hpp
/// @brief Atomic transaction system for void_ir

#include "fwd.hpp"
#include "namespace.hpp"
#include "patch.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <functional>

namespace void_ir {

// =============================================================================
// TransactionId
// =============================================================================

/// Unique transaction identifier
struct TransactionId {
    std::uint64_t value = UINT64_MAX;

    constexpr TransactionId() noexcept = default;
    constexpr explicit TransactionId(std::uint64_t v) noexcept : value(v) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value != UINT64_MAX;
    }

    [[nodiscard]] static constexpr TransactionId invalid() noexcept {
        return TransactionId{};
    }

    constexpr bool operator==(const TransactionId& other) const noexcept = default;
    constexpr bool operator<(const TransactionId& other) const noexcept {
        return value < other.value;
    }
};

// =============================================================================
// TransactionState
// =============================================================================

/// Transaction state
enum class TransactionState : std::uint8_t {
    Building = 0,   // Under construction
    Pending,        // Submitted, awaiting application
    Applying,       // Currently being applied
    Committed,      // Successfully applied
    RolledBack,     // Rolled back
    Failed          // Failed to apply
};

/// Get state name
[[nodiscard]] inline const char* transaction_state_name(TransactionState state) noexcept {
    switch (state) {
        case TransactionState::Building: return "Building";
        case TransactionState::Pending: return "Pending";
        case TransactionState::Applying: return "Applying";
        case TransactionState::Committed: return "Committed";
        case TransactionState::RolledBack: return "RolledBack";
        case TransactionState::Failed: return "Failed";
        default: return "Unknown";
    }
}

// =============================================================================
// TransactionPriority
// =============================================================================

/// Transaction priority for ordering
enum class TransactionPriority : std::uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// =============================================================================
// TransactionMetadata
// =============================================================================

/// Transaction metadata
struct TransactionMetadata {
    std::string description;
    std::string source;  // Who/what created this transaction
    TransactionPriority priority = TransactionPriority::Normal;
    std::chrono::steady_clock::time_point created_at;
    std::optional<std::chrono::steady_clock::time_point> applied_at;

    TransactionMetadata() : created_at(std::chrono::steady_clock::now()) {}
};

// =============================================================================
// TransactionResult
// =============================================================================

/// Result of transaction application
struct TransactionResult {
    bool success = false;
    std::string error_message;
    std::size_t patches_applied = 0;
    std::size_t patches_failed = 0;
    std::vector<std::size_t> failed_indices;

    [[nodiscard]] static TransactionResult ok(std::size_t count) {
        TransactionResult r;
        r.success = true;
        r.patches_applied = count;
        return r;
    }

    [[nodiscard]] static TransactionResult failed(std::string message) {
        TransactionResult r;
        r.success = false;
        r.error_message = std::move(message);
        return r;
    }

    [[nodiscard]] static TransactionResult partial(
        std::size_t applied, std::size_t failed, std::vector<std::size_t> indices) {
        TransactionResult r;
        r.success = false;
        r.patches_applied = applied;
        r.patches_failed = failed;
        r.failed_indices = std::move(indices);
        r.error_message = "Partial failure";
        return r;
    }
};

// =============================================================================
// Transaction
// =============================================================================

/// Atomic transaction containing patches
class Transaction {
public:
    /// Construct with ID and namespace
    explicit Transaction(TransactionId id, NamespaceId ns)
        : m_id(id)
        , m_namespace(ns)
        , m_state(TransactionState::Building) {}

    /// Get transaction ID
    [[nodiscard]] TransactionId id() const noexcept { return m_id; }

    /// Get target namespace
    [[nodiscard]] NamespaceId namespace_id() const noexcept { return m_namespace; }

    /// Get current state
    [[nodiscard]] TransactionState state() const noexcept { return m_state; }

    /// Get state name
    [[nodiscard]] const char* state_name() const noexcept {
        return transaction_state_name(m_state);
    }

    /// Get metadata
    [[nodiscard]] const TransactionMetadata& metadata() const noexcept {
        return m_metadata;
    }

    /// Get mutable metadata
    [[nodiscard]] TransactionMetadata& metadata() noexcept {
        return m_metadata;
    }

    /// Get patches
    [[nodiscard]] const PatchBatch& patches() const noexcept {
        return m_patches;
    }

    /// Get mutable patches (only valid in Building state)
    [[nodiscard]] PatchBatch& patches_mut() {
        if (m_state != TransactionState::Building) {
            throw std::runtime_error("Cannot modify patches after submission");
        }
        return m_patches;
    }

    /// Add a patch (only valid in Building state)
    void add_patch(Patch patch) {
        if (m_state != TransactionState::Building) {
            throw std::runtime_error("Cannot add patches after submission");
        }
        m_patches.push(std::move(patch));
    }

    /// Get patch count
    [[nodiscard]] std::size_t patch_count() const noexcept {
        return m_patches.size();
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return m_patches.empty();
    }

    /// Submit transaction (move from Building to Pending)
    void submit() {
        if (m_state != TransactionState::Building) {
            throw std::runtime_error("Transaction already submitted");
        }
        m_state = TransactionState::Pending;
    }

    /// Mark as applying
    void begin_apply() {
        if (m_state != TransactionState::Pending) {
            throw std::runtime_error("Transaction not in Pending state");
        }
        m_state = TransactionState::Applying;
    }

    /// Mark as committed
    void commit() {
        if (m_state != TransactionState::Applying) {
            throw std::runtime_error("Transaction not in Applying state");
        }
        m_state = TransactionState::Committed;
        m_metadata.applied_at = std::chrono::steady_clock::now();
    }

    /// Mark as rolled back
    void rollback() {
        m_state = TransactionState::RolledBack;
    }

    /// Mark as failed
    void fail(std::string error) {
        m_state = TransactionState::Failed;
        m_error = std::move(error);
    }

    /// Get error message (if failed)
    [[nodiscard]] const std::string& error() const noexcept {
        return m_error;
    }

    /// Get the snapshot ID to rollback to (if set)
    [[nodiscard]] std::optional<SnapshotId> rollback_snapshot() const noexcept {
        return m_rollback_snapshot;
    }

    /// Set rollback snapshot
    void set_rollback_snapshot(SnapshotId id) {
        m_rollback_snapshot = id;
    }

private:
    TransactionId m_id;
    NamespaceId m_namespace;
    TransactionState m_state;
    TransactionMetadata m_metadata;
    PatchBatch m_patches;
    std::string m_error;
    std::optional<SnapshotId> m_rollback_snapshot;
};

// =============================================================================
// TransactionBuilder
// =============================================================================

/// Fluent builder for transactions
class TransactionBuilder {
public:
    /// Start building for namespace
    explicit TransactionBuilder(NamespaceId ns)
        : m_namespace(ns) {}

    /// Set description
    TransactionBuilder& description(std::string desc) {
        m_description = std::move(desc);
        return *this;
    }

    /// Set source
    TransactionBuilder& source(std::string src) {
        m_source = std::move(src);
        return *this;
    }

    /// Set priority
    TransactionBuilder& priority(TransactionPriority p) {
        m_priority = p;
        return *this;
    }

    /// Add entity creation
    TransactionBuilder& create_entity(EntityRef ref, std::string name = "") {
        m_patches.push(EntityPatch::create(ref, std::move(name)));
        return *this;
    }

    /// Add entity deletion
    TransactionBuilder& delete_entity(EntityRef ref) {
        m_patches.push(EntityPatch::destroy(ref));
        return *this;
    }

    /// Add component
    TransactionBuilder& add_component(EntityRef ref, std::string type, Value value) {
        m_patches.push(ComponentPatch::add(ref, std::move(type), std::move(value)));
        return *this;
    }

    /// Remove component
    TransactionBuilder& remove_component(EntityRef ref, std::string type) {
        m_patches.push(ComponentPatch::remove(ref, std::move(type)));
        return *this;
    }

    /// Set component
    TransactionBuilder& set_component(EntityRef ref, std::string type, Value value) {
        m_patches.push(ComponentPatch::set(ref, std::move(type), std::move(value)));
        return *this;
    }

    /// Set component field
    TransactionBuilder& set_field(EntityRef ref, std::string type,
                                   std::string field, Value value) {
        m_patches.push(ComponentPatch::set_field(ref, std::move(type),
                                                  std::move(field), std::move(value)));
        return *this;
    }

    /// Set transform position
    TransactionBuilder& set_position(EntityRef ref, Vec3 pos) {
        m_patches.push(TransformPatch::set_position(ref, pos));
        return *this;
    }

    /// Set transform rotation
    TransactionBuilder& set_rotation(EntityRef ref, Vec4 rot) {
        m_patches.push(TransformPatch::set_rotation(ref, rot));
        return *this;
    }

    /// Set transform scale
    TransactionBuilder& set_scale(EntityRef ref, Vec3 scale) {
        m_patches.push(TransformPatch::set_scale(ref, scale));
        return *this;
    }

    /// Set parent
    TransactionBuilder& set_parent(EntityRef entity, EntityRef parent) {
        m_patches.push(HierarchyPatch::set_parent(entity, parent));
        return *this;
    }

    /// Clear parent
    TransactionBuilder& clear_parent(EntityRef entity) {
        m_patches.push(HierarchyPatch::clear_parent(entity));
        return *this;
    }

    /// Add custom patch
    TransactionBuilder& patch(Patch p) {
        m_patches.push(std::move(p));
        return *this;
    }

    /// Get patch count
    [[nodiscard]] std::size_t patch_count() const noexcept {
        return m_patches.size();
    }

    /// Build the transaction (requires ID generator)
    [[nodiscard]] Transaction build(TransactionId id) {
        Transaction tx(id, m_namespace);
        tx.metadata().description = std::move(m_description);
        tx.metadata().source = std::move(m_source);
        tx.metadata().priority = m_priority;

        for (auto& patch : m_patches) {
            tx.add_patch(std::move(patch));
        }

        return tx;
    }

private:
    NamespaceId m_namespace;
    std::string m_description;
    std::string m_source;
    TransactionPriority m_priority = TransactionPriority::Normal;
    PatchBatch m_patches;
};

// =============================================================================
// TransactionQueue
// =============================================================================

/// Queue of pending transactions
class TransactionQueue {
public:
    /// Add transaction to queue
    void enqueue(Transaction tx) {
        tx.submit();
        m_pending.push_back(std::move(tx));
    }

    /// Get next transaction to apply
    [[nodiscard]] std::optional<Transaction> dequeue() {
        if (m_pending.empty()) {
            return std::nullopt;
        }

        // Find highest priority
        auto it = std::max_element(m_pending.begin(), m_pending.end(),
            [](const Transaction& a, const Transaction& b) {
                return a.metadata().priority < b.metadata().priority;
            });

        Transaction tx = std::move(*it);
        m_pending.erase(it);
        return tx;
    }

    /// Peek at front without removing
    [[nodiscard]] const Transaction* peek() const {
        if (m_pending.empty()) {
            return nullptr;
        }

        auto it = std::max_element(m_pending.begin(), m_pending.end(),
            [](const Transaction& a, const Transaction& b) {
                return a.metadata().priority < b.metadata().priority;
            });

        return &(*it);
    }

    /// Get queue size
    [[nodiscard]] std::size_t size() const noexcept {
        return m_pending.size();
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return m_pending.empty();
    }

    /// Clear all pending transactions
    void clear() {
        m_pending.clear();
    }

    /// Get total patch count across all transactions
    [[nodiscard]] std::size_t total_patch_count() const {
        std::size_t count = 0;
        for (const auto& tx : m_pending) {
            count += tx.patch_count();
        }
        return count;
    }

private:
    std::vector<Transaction> m_pending;
};

} // namespace void_ir

// Hash specialization
template<>
struct std::hash<void_ir::TransactionId> {
    std::size_t operator()(const void_ir::TransactionId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
