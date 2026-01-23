#pragma once

/// @file snapshot.hpp
/// @brief Snapshot and rollback system for void_ir

#include "fwd.hpp"
#include "namespace.hpp"
#include "value.hpp"
#include "patch.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <memory>

namespace void_ir {

// SnapshotId is defined in fwd.hpp

// =============================================================================
// EntitySnapshot
// =============================================================================

/// Snapshot of a single entity's state
struct EntitySnapshot {
    EntityRef entity;
    std::string name;
    bool enabled = true;
    std::unordered_map<std::string, Value> components;

    /// Check if entity has component
    [[nodiscard]] bool has_component(const std::string& type) const {
        return components.find(type) != components.end();
    }

    /// Get component value
    [[nodiscard]] const Value* get_component(const std::string& type) const {
        auto it = components.find(type);
        if (it == components.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Clone the snapshot
    [[nodiscard]] EntitySnapshot clone() const {
        EntitySnapshot s;
        s.entity = entity;
        s.name = name;
        s.enabled = enabled;
        s.components = components;
        return s;
    }
};

// =============================================================================
// LayerSnapshot
// =============================================================================

/// Snapshot of a layer
struct LayerSnapshot {
    LayerId layer;
    std::string name;
    std::int32_t order = 0;
    bool visible = true;
    bool locked = false;
    std::vector<EntityRef> entities;

    [[nodiscard]] LayerSnapshot clone() const {
        return *this;
    }
};

// =============================================================================
// HierarchySnapshot
// =============================================================================

/// Snapshot of parent-child relationships
struct HierarchySnapshot {
    std::unordered_map<std::uint64_t, EntityRef> parents;  // entity_id -> parent
    std::unordered_map<std::uint64_t, std::vector<EntityRef>> children;  // entity_id -> children

    /// Get parent of entity
    [[nodiscard]] std::optional<EntityRef> get_parent(EntityRef entity) const {
        auto it = parents.find(entity.entity_id);
        if (it == parents.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    /// Get children of entity
    [[nodiscard]] const std::vector<EntityRef>* get_children(EntityRef entity) const {
        auto it = children.find(entity.entity_id);
        if (it == children.end()) {
            return nullptr;
        }
        return &it->second;
    }
};

// =============================================================================
// Snapshot
// =============================================================================

/// Full snapshot of namespace state
class Snapshot {
public:
    /// Construct with ID
    explicit Snapshot(SnapshotId id, NamespaceId ns)
        : m_id(id)
        , m_namespace(ns)
        , m_timestamp(std::chrono::steady_clock::now()) {}

    /// Get snapshot ID
    [[nodiscard]] SnapshotId id() const noexcept { return m_id; }

    /// Get namespace
    [[nodiscard]] NamespaceId namespace_id() const noexcept { return m_namespace; }

    /// Get timestamp
    [[nodiscard]] std::chrono::steady_clock::time_point timestamp() const noexcept {
        return m_timestamp;
    }

    /// Get description
    [[nodiscard]] const std::string& description() const noexcept {
        return m_description;
    }

    /// Set description
    void set_description(std::string desc) {
        m_description = std::move(desc);
    }

    // -------------------------------------------------------------------------
    // Entity access
    // -------------------------------------------------------------------------

    /// Get all entity snapshots
    [[nodiscard]] const std::unordered_map<std::uint64_t, EntitySnapshot>& entities() const {
        return m_entities;
    }

    /// Get entity snapshot
    [[nodiscard]] const EntitySnapshot* get_entity(EntityRef ref) const {
        auto it = m_entities.find(ref.entity_id);
        if (it == m_entities.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Add entity snapshot
    void add_entity(EntitySnapshot snapshot) {
        m_entities[snapshot.entity.entity_id] = std::move(snapshot);
    }

    /// Remove entity
    void remove_entity(EntityRef ref) {
        m_entities.erase(ref.entity_id);
    }

    /// Get entity count
    [[nodiscard]] std::size_t entity_count() const noexcept {
        return m_entities.size();
    }

    // -------------------------------------------------------------------------
    // Layer access
    // -------------------------------------------------------------------------

    /// Get all layer snapshots
    [[nodiscard]] const std::unordered_map<std::uint32_t, LayerSnapshot>& layers() const {
        return m_layers;
    }

    /// Get layer snapshot
    [[nodiscard]] const LayerSnapshot* get_layer(LayerId id) const {
        auto it = m_layers.find(id.value);
        if (it == m_layers.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Add layer snapshot
    void add_layer(LayerSnapshot snapshot) {
        m_layers[snapshot.layer.value] = std::move(snapshot);
    }

    /// Remove layer
    void remove_layer(LayerId id) {
        m_layers.erase(id.value);
    }

    // -------------------------------------------------------------------------
    // Hierarchy access
    // -------------------------------------------------------------------------

    /// Get hierarchy
    [[nodiscard]] const HierarchySnapshot& hierarchy() const noexcept {
        return m_hierarchy;
    }

    /// Get mutable hierarchy
    [[nodiscard]] HierarchySnapshot& hierarchy_mut() noexcept {
        return m_hierarchy;
    }

    // -------------------------------------------------------------------------
    // Cloning
    // -------------------------------------------------------------------------

    /// Deep clone the snapshot
    [[nodiscard]] Snapshot clone() const {
        Snapshot s(m_id, m_namespace);
        s.m_timestamp = m_timestamp;
        s.m_description = m_description;
        s.m_entities = m_entities;
        s.m_layers = m_layers;
        s.m_hierarchy = m_hierarchy;
        return s;
    }

private:
    SnapshotId m_id;
    NamespaceId m_namespace;
    std::chrono::steady_clock::time_point m_timestamp;
    std::string m_description;
    std::unordered_map<std::uint64_t, EntitySnapshot> m_entities;
    std::unordered_map<std::uint32_t, LayerSnapshot> m_layers;
    HierarchySnapshot m_hierarchy;
};

// =============================================================================
// SnapshotDelta
// =============================================================================

/// Difference between two snapshots
class SnapshotDelta {
public:
    /// Entity changes
    struct EntityChange {
        EntityRef entity;
        enum class Type { Added, Removed, Modified } type;
        std::optional<EntitySnapshot> old_state;
        std::optional<EntitySnapshot> new_state;
    };

    /// Component changes
    struct ComponentChange {
        EntityRef entity;
        std::string component_type;
        enum class Type { Added, Removed, Modified } type;
        std::optional<Value> old_value;
        std::optional<Value> new_value;
    };

    /// Get entity changes
    [[nodiscard]] const std::vector<EntityChange>& entity_changes() const {
        return m_entity_changes;
    }

    /// Get component changes
    [[nodiscard]] const std::vector<ComponentChange>& component_changes() const {
        return m_component_changes;
    }

    /// Add entity change
    void add_entity_change(EntityChange change) {
        m_entity_changes.push_back(std::move(change));
    }

    /// Add component change
    void add_component_change(ComponentChange change) {
        m_component_changes.push_back(std::move(change));
    }

    /// Check if delta is empty
    [[nodiscard]] bool empty() const noexcept {
        return m_entity_changes.empty() && m_component_changes.empty();
    }

    /// Convert delta to patches for replay
    [[nodiscard]] PatchBatch to_patches() const {
        PatchBatch batch;

        for (const auto& ec : m_entity_changes) {
            if (ec.type == EntityChange::Type::Added && ec.new_state) {
                batch.push(EntityPatch::create(ec.entity, ec.new_state->name));
                for (const auto& [type, value] : ec.new_state->components) {
                    batch.push(ComponentPatch::add(ec.entity, type, value.clone()));
                }
            }
            else if (ec.type == EntityChange::Type::Removed) {
                batch.push(EntityPatch::destroy(ec.entity));
            }
        }

        for (const auto& cc : m_component_changes) {
            if (cc.type == ComponentChange::Type::Added && cc.new_value) {
                batch.push(ComponentPatch::add(cc.entity, cc.component_type, cc.new_value->clone()));
            }
            else if (cc.type == ComponentChange::Type::Removed) {
                batch.push(ComponentPatch::remove(cc.entity, cc.component_type));
            }
            else if (cc.type == ComponentChange::Type::Modified && cc.new_value) {
                batch.push(ComponentPatch::set(cc.entity, cc.component_type, cc.new_value->clone()));
            }
        }

        return batch;
    }

    /// Compute delta between two snapshots
    [[nodiscard]] static SnapshotDelta compute(const Snapshot& from, const Snapshot& to) {
        SnapshotDelta delta;

        // Find added/modified entities
        for (const auto& [id, new_entity] : to.entities()) {
            const EntitySnapshot* old_entity = from.get_entity(new_entity.entity);

            if (!old_entity) {
                // Added
                EntityChange change;
                change.entity = new_entity.entity;
                change.type = EntityChange::Type::Added;
                change.new_state = new_entity.clone();
                delta.add_entity_change(std::move(change));
            }
            else {
                // Check for modifications
                for (const auto& [type, new_value] : new_entity.components) {
                    const Value* old_value = old_entity->get_component(type);

                    if (!old_value) {
                        // Component added
                        ComponentChange cc;
                        cc.entity = new_entity.entity;
                        cc.component_type = type;
                        cc.type = ComponentChange::Type::Added;
                        cc.new_value = new_value.clone();
                        delta.add_component_change(std::move(cc));
                    }
                    else if (*old_value != new_value) {
                        // Component modified
                        ComponentChange cc;
                        cc.entity = new_entity.entity;
                        cc.component_type = type;
                        cc.type = ComponentChange::Type::Modified;
                        cc.old_value = old_value->clone();
                        cc.new_value = new_value.clone();
                        delta.add_component_change(std::move(cc));
                    }
                }

                // Check for removed components
                for (const auto& [type, old_value] : old_entity->components) {
                    if (!new_entity.has_component(type)) {
                        ComponentChange cc;
                        cc.entity = new_entity.entity;
                        cc.component_type = type;
                        cc.type = ComponentChange::Type::Removed;
                        cc.old_value = old_value.clone();
                        delta.add_component_change(std::move(cc));
                    }
                }
            }
        }

        // Find removed entities
        for (const auto& [id, old_entity] : from.entities()) {
            if (!to.get_entity(old_entity.entity)) {
                EntityChange change;
                change.entity = old_entity.entity;
                change.type = EntityChange::Type::Removed;
                change.old_state = old_entity.clone();
                delta.add_entity_change(std::move(change));
            }
        }

        return delta;
    }

private:
    std::vector<EntityChange> m_entity_changes;
    std::vector<ComponentChange> m_component_changes;
};

// =============================================================================
// SnapshotManager
// =============================================================================

/// Manages snapshots for a namespace
class SnapshotManager {
public:
    /// Construct with max snapshots (0 = unlimited)
    explicit SnapshotManager(std::size_t max_snapshots = 0)
        : m_max_snapshots(max_snapshots) {}

    /// Create a new snapshot
    [[nodiscard]] SnapshotId create(NamespaceId ns, std::string description = "") {
        SnapshotId id(m_next_id++);
        Snapshot snapshot(id, ns);
        snapshot.set_description(std::move(description));

        // Enforce limit
        if (m_max_snapshots > 0 && m_snapshots.size() >= m_max_snapshots) {
            // Remove oldest
            if (!m_order.empty()) {
                m_snapshots.erase(m_order.front().value);
                m_order.erase(m_order.begin());
            }
        }

        m_snapshots.emplace(id.value, std::move(snapshot));
        m_order.push_back(id);

        return id;
    }

    /// Get snapshot by ID
    [[nodiscard]] Snapshot* get(SnapshotId id) {
        auto it = m_snapshots.find(id.value);
        if (it == m_snapshots.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Get snapshot by ID (const)
    [[nodiscard]] const Snapshot* get(SnapshotId id) const {
        auto it = m_snapshots.find(id.value);
        if (it == m_snapshots.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Get the most recent snapshot
    [[nodiscard]] const Snapshot* latest() const {
        if (m_order.empty()) {
            return nullptr;
        }
        return get(m_order.back());
    }

    /// Get snapshot at index (0 = oldest)
    [[nodiscard]] const Snapshot* at_index(std::size_t index) const {
        if (index >= m_order.size()) {
            return nullptr;
        }
        return get(m_order[index]);
    }

    /// Delete a snapshot
    bool remove(SnapshotId id) {
        if (m_snapshots.erase(id.value) > 0) {
            m_order.erase(
                std::remove(m_order.begin(), m_order.end(), id),
                m_order.end()
            );
            return true;
        }
        return false;
    }

    /// Delete all snapshots before given ID
    std::size_t remove_before(SnapshotId id) {
        std::size_t removed = 0;
        auto it = m_order.begin();
        while (it != m_order.end() && *it < id) {
            m_snapshots.erase(it->value);
            it = m_order.erase(it);
            ++removed;
        }
        return removed;
    }

    /// Get snapshot count
    [[nodiscard]] std::size_t size() const noexcept {
        return m_snapshots.size();
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return m_snapshots.empty();
    }

    /// Clear all snapshots
    void clear() {
        m_snapshots.clear();
        m_order.clear();
    }

    /// Set max snapshots
    void set_max_snapshots(std::size_t max) {
        m_max_snapshots = max;

        // Trim if needed
        while (m_max_snapshots > 0 && m_snapshots.size() > m_max_snapshots) {
            if (!m_order.empty()) {
                m_snapshots.erase(m_order.front().value);
                m_order.erase(m_order.begin());
            }
        }
    }

    /// Get max snapshots
    [[nodiscard]] std::size_t max_snapshots() const noexcept {
        return m_max_snapshots;
    }

    /// Get all snapshot IDs in order
    [[nodiscard]] const std::vector<SnapshotId>& snapshot_ids() const noexcept {
        return m_order;
    }

private:
    std::unordered_map<std::uint64_t, Snapshot> m_snapshots;
    std::vector<SnapshotId> m_order;  // In chronological order
    std::uint64_t m_next_id = 0;
    std::size_t m_max_snapshots = 0;
};

} // namespace void_ir

// Hash specialization
template<>
struct std::hash<void_ir::SnapshotId> {
    std::size_t operator()(const void_ir::SnapshotId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
