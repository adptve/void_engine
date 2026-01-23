#pragma once

/// @file archetype.hpp
/// @brief Archetype storage for void_ecs
///
/// Archetypes group entities with identical component sets for cache-efficient
/// iteration. Each archetype stores components in parallel arrays (SoA layout).

#include "fwd.hpp"
#include "entity.hpp"
#include "component.hpp"
#include <void_engine/structures/bitset.hpp>

#include <vector>
#include <map>
#include <optional>
#include <algorithm>
#include <cassert>

namespace void_ecs {

// =============================================================================
// EntityLocation Implementation
// =============================================================================

inline constexpr EntityLocation::EntityLocation(ArchetypeId arch, std::size_t r) noexcept
    : archetype_id(arch), row(r) {}

inline EntityLocation EntityLocation::invalid() noexcept {
    return EntityLocation{ArchetypeId::invalid(), std::numeric_limits<std::size_t>::max()};
}

inline bool EntityLocation::is_valid() const noexcept {
    return archetype_id.is_valid();
}

// =============================================================================
// ArchetypeEdge
// =============================================================================

/// Edge in the archetype graph for fast component add/remove transitions
struct ArchetypeEdge {
    ArchetypeId add{ArchetypeId::INVALID_ID};     // Archetype when adding this component
    ArchetypeId remove{ArchetypeId::INVALID_ID};  // Archetype when removing this component
};

// =============================================================================
// Archetype
// =============================================================================

/// Container for entities with identical component sets
///
/// Stores entities and their components in parallel arrays for cache efficiency.
/// Uses swap-remove for O(1) entity removal.
class Archetype {
public:
    using size_type = std::size_t;

private:
    ArchetypeId id_;
    std::vector<ComponentId> components_;           // Sorted component IDs
    void_structures::BitSet component_mask_;        // For fast matching
    std::vector<ComponentStorage> storages_;        // Storage per component
    std::map<ComponentId, size_type> component_indices_; // ComponentId -> storage index
    std::vector<Entity> entities_;                  // Entities in this archetype
    std::map<ComponentId, ArchetypeEdge> edges_;    // Graph edges

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create archetype with given component set
    Archetype(ArchetypeId arch_id, std::vector<ComponentInfo> component_infos)
        : id_(arch_id)
        , component_mask_(256)  // Support up to 256 component types initially
    {
        // Sort components by ID for consistent ordering
        std::sort(component_infos.begin(), component_infos.end(),
            [](const auto& a, const auto& b) { return a.id < b.id; });

        components_.reserve(component_infos.size());
        storages_.reserve(component_infos.size());

        for (size_type i = 0; i < component_infos.size(); ++i) {
            const auto& info = component_infos[i];
            components_.push_back(info.id);
            component_mask_.set(info.id.id);
            component_indices_[info.id] = i;
            storages_.emplace_back(info);
        }
    }

    /// Create empty archetype
    explicit Archetype(ArchetypeId arch_id)
        : id_(arch_id)
        , component_mask_(256) {}

    // =========================================================================
    // Properties
    // =========================================================================

    /// Get archetype ID
    [[nodiscard]] ArchetypeId id() const noexcept { return id_; }

    /// Get sorted component IDs
    [[nodiscard]] const std::vector<ComponentId>& components() const noexcept {
        return components_;
    }

    /// Get component mask for fast matching
    [[nodiscard]] const void_structures::BitSet& component_mask() const noexcept {
        return component_mask_;
    }

    /// Check if archetype has a component
    [[nodiscard]] bool has_component(ComponentId id) const noexcept {
        return id.id < component_mask_.size() && component_mask_.get(id.id);
    }

    /// Number of entities in this archetype
    [[nodiscard]] size_type len() const noexcept { return entities_.size(); }

    /// Alias for len()
    [[nodiscard]] size_type size() const noexcept { return len(); }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept { return entities_.empty(); }

    /// Get entity list
    [[nodiscard]] const std::vector<Entity>& entities() const noexcept {
        return entities_;
    }

    // =========================================================================
    // Storage Access
    // =========================================================================

    /// Get component storage by ID
    [[nodiscard]] ComponentStorage* storage(ComponentId id) noexcept {
        auto it = component_indices_.find(id);
        if (it == component_indices_.end()) return nullptr;
        return &storages_[it->second];
    }

    /// Get const component storage by ID
    [[nodiscard]] const ComponentStorage* storage(ComponentId id) const noexcept {
        auto it = component_indices_.find(id);
        if (it == component_indices_.end()) return nullptr;
        return &storages_[it->second];
    }

    /// Get all storages
    [[nodiscard]] std::vector<ComponentStorage>& storages() noexcept {
        return storages_;
    }

    /// Get all storages (const)
    [[nodiscard]] const std::vector<ComponentStorage>& storages() const noexcept {
        return storages_;
    }

    /// Get storage index for component
    [[nodiscard]] std::optional<size_type> storage_index(ComponentId id) const noexcept {
        auto it = component_indices_.find(id);
        if (it == component_indices_.end()) return std::nullopt;
        return it->second;
    }

    // =========================================================================
    // Entity Operations
    // =========================================================================

    /// Reserve capacity for entities
    void reserve(size_type additional) {
        entities_.reserve(entities_.size() + additional);
        for (auto& storage : storages_) {
            storage.reserve(additional);
        }
    }

    /// Add entity with component data
    /// @param entity The entity to add
    /// @param component_data Pointers to component data (in component order)
    /// @return Row index of the new entity
    size_type add_entity(Entity entity, const std::vector<const void*>& component_data) {
        assert(component_data.size() == storages_.size());

        size_type row = entities_.size();
        entities_.push_back(entity);

        for (size_type i = 0; i < storages_.size(); ++i) {
            storages_[i].push_raw_bytes(component_data[i]);
        }

        return row;
    }

    /// Remove entity at row (swap-remove)
    /// @return Entity that was swapped into this row (if any), for location updates
    std::optional<Entity> remove_entity(size_type row) {
        if (row >= entities_.size()) return std::nullopt;

        size_type last_row = entities_.size() - 1;
        std::optional<Entity> swapped;

        if (row != last_row) {
            swapped = entities_[last_row];
            entities_[row] = entities_[last_row];
        }

        entities_.pop_back();

        // Remove from all storages
        for (auto& storage : storages_) {
            storage.swap_remove(row);
        }

        return swapped;
    }

    /// Get entity at row
    [[nodiscard]] Entity entity_at(size_type row) const noexcept {
        if (row >= entities_.size()) return Entity::null();
        return entities_[row];
    }

    // =========================================================================
    // Component Access
    // =========================================================================

    /// Get typed component for entity at row
    template<typename T>
    [[nodiscard]] const T* get_component(ComponentId id, size_type row) const {
        const auto* stor = storage(id);
        if (!stor || row >= stor->size()) return nullptr;
        return &stor->template get<T>(row);
    }

    /// Get mutable typed component for entity at row
    template<typename T>
    [[nodiscard]] T* get_component(ComponentId id, size_type row) {
        auto* stor = storage(id);
        if (!stor || row >= stor->size()) return nullptr;
        return &stor->template get<T>(row);
    }

    /// Get raw component pointer
    [[nodiscard]] void* get_component_raw(ComponentId id, size_type row) noexcept {
        auto* stor = storage(id);
        if (!stor) return nullptr;
        return stor->get_raw(row);
    }

    /// Get const raw component pointer
    [[nodiscard]] const void* get_component_raw(ComponentId id, size_type row) const noexcept {
        const auto* stor = storage(id);
        if (!stor) return nullptr;
        return stor->get_raw(row);
    }

    // =========================================================================
    // Graph Edges
    // =========================================================================

    /// Get edge for component
    [[nodiscard]] const ArchetypeEdge* edge(ComponentId id) const noexcept {
        auto it = edges_.find(id);
        if (it == edges_.end()) return nullptr;
        return &it->second;
    }

    /// Set edge for component
    void set_edge(ComponentId id, ArchetypeEdge edge) {
        edges_[id] = edge;
    }

    /// Get or create edge for component
    ArchetypeEdge& edge_mut(ComponentId id) {
        return edges_[id];
    }
};

// =============================================================================
// Archetypes
// =============================================================================

/// Manager for all archetypes
///
/// Maintains a collection of archetypes and provides lookup by component signature.
class Archetypes {
public:
    using size_type = std::size_t;

private:
    std::vector<std::unique_ptr<Archetype>> archetypes_;
    std::map<std::vector<ComponentId>, ArchetypeId> signature_map_;
    ArchetypeId empty_archetype_;

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create with empty archetype
    Archetypes() {
        // Create the empty archetype (no components)
        auto empty = std::make_unique<Archetype>(ArchetypeId{0});
        archetypes_.push_back(std::move(empty));
        signature_map_[{}] = ArchetypeId{0};
        empty_archetype_ = ArchetypeId{0};
    }

    // =========================================================================
    // Properties
    // =========================================================================

    /// Get empty archetype ID
    [[nodiscard]] ArchetypeId empty() const noexcept {
        return empty_archetype_;
    }

    /// Number of archetypes
    [[nodiscard]] size_type size() const noexcept {
        return archetypes_.size();
    }

    /// Alias for size()
    [[nodiscard]] size_type len() const noexcept {
        return size();
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return archetypes_.empty();
    }

    // =========================================================================
    // Access
    // =========================================================================

    /// Get archetype by ID
    [[nodiscard]] Archetype* get(ArchetypeId id) noexcept {
        if (id.id >= archetypes_.size()) return nullptr;
        return archetypes_[id.id].get();
    }

    /// Get const archetype by ID
    [[nodiscard]] const Archetype* get(ArchetypeId id) const noexcept {
        if (id.id >= archetypes_.size()) return nullptr;
        return archetypes_[id.id].get();
    }

    /// Get two mutable archetypes by ID (for moves)
    [[nodiscard]] std::pair<Archetype*, Archetype*> get_pair(ArchetypeId id1, ArchetypeId id2) noexcept {
        Archetype* a1 = get(id1);
        Archetype* a2 = get(id2);
        return {a1, a2};
    }

    // =========================================================================
    // Lookup and Creation
    // =========================================================================

    /// Find archetype by component signature
    [[nodiscard]] std::optional<ArchetypeId> find(const std::vector<ComponentId>& components) const {
        // Sort for consistent lookup
        std::vector<ComponentId> sorted = components;
        std::sort(sorted.begin(), sorted.end());

        auto it = signature_map_.find(sorted);
        if (it != signature_map_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Get or create archetype with given component infos
    ArchetypeId get_or_create(std::vector<ComponentInfo> component_infos) {
        // Extract and sort IDs for lookup
        std::vector<ComponentId> ids;
        ids.reserve(component_infos.size());
        for (const auto& info : component_infos) {
            ids.push_back(info.id);
        }
        std::sort(ids.begin(), ids.end());

        // Check if exists
        auto it = signature_map_.find(ids);
        if (it != signature_map_.end()) {
            return it->second;
        }

        // Create new archetype
        ArchetypeId new_id{static_cast<std::uint32_t>(archetypes_.size())};
        auto archetype = std::make_unique<Archetype>(new_id, std::move(component_infos));
        archetypes_.push_back(std::move(archetype));
        signature_map_[ids] = new_id;

        return new_id;
    }

    /// Get or create archetype with given component IDs (using registry for infos)
    ArchetypeId get_or_create(const std::vector<ComponentId>& component_ids,
                               const ComponentRegistry& registry) {
        std::vector<ComponentInfo> infos;
        infos.reserve(component_ids.size());

        for (ComponentId id : component_ids) {
            const ComponentInfo* info = registry.get_info(id);
            if (info) {
                infos.push_back(*info);
            }
        }

        return get_or_create(std::move(infos));
    }

    // =========================================================================
    // Iteration
    // =========================================================================

    /// Iterate over all archetypes
    [[nodiscard]] auto begin() noexcept {
        return archetypes_.begin();
    }

    [[nodiscard]] auto end() noexcept {
        return archetypes_.end();
    }

    [[nodiscard]] auto begin() const noexcept {
        return archetypes_.begin();
    }

    [[nodiscard]] auto end() const noexcept {
        return archetypes_.end();
    }
};

} // namespace void_ecs

// Hash specialization for ArchetypeId
template<>
struct std::hash<void_ecs::ArchetypeId> {
    [[nodiscard]] std::size_t operator()(const void_ecs::ArchetypeId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.id);
    }
};
