#pragma once

/// @file world.hpp
/// @brief Main ECS container for void_ecs
///
/// World is the central container that manages entities, components, and
/// their storage in archetypes.

#include "fwd.hpp"
#include "entity.hpp"
#include "component.hpp"
#include "archetype.hpp"
#include "query.hpp"

#include <unordered_map>
#include <typeindex>
#include <any>
#include <memory>
#include <algorithm>

namespace void_ecs {

// =============================================================================
// Resources
// =============================================================================

/// Global resource storage (singletons)
class Resources {
private:
    std::unordered_map<std::type_index, std::any> resources_;

public:
    /// Insert or replace a resource
    template<typename R>
    void insert(R resource) {
        resources_[std::type_index(typeid(R))] = std::move(resource);
    }

    /// Remove and return a resource
    template<typename R>
    std::optional<R> remove() {
        std::type_index idx = std::type_index(typeid(R));
        auto it = resources_.find(idx);
        if (it == resources_.end()) {
            return std::nullopt;
        }
        R value = std::any_cast<R>(std::move(it->second));
        resources_.erase(it);
        return value;
    }

    /// Get immutable resource reference
    template<typename R>
    [[nodiscard]] const R* get() const {
        std::type_index idx = std::type_index(typeid(R));
        auto it = resources_.find(idx);
        if (it == resources_.end()) {
            return nullptr;
        }
        return std::any_cast<R>(&it->second);
    }

    /// Get mutable resource reference
    template<typename R>
    [[nodiscard]] R* get() {
        std::type_index idx = std::type_index(typeid(R));
        auto it = resources_.find(idx);
        if (it == resources_.end()) {
            return nullptr;
        }
        return std::any_cast<R>(&it->second);
    }

    /// Check if resource exists
    template<typename R>
    [[nodiscard]] bool contains() const {
        return resources_.count(std::type_index(typeid(R))) > 0;
    }

    /// Clear all resources
    void clear() {
        resources_.clear();
    }
};

// =============================================================================
// World
// =============================================================================

/// The main ECS container
///
/// Manages entities, components, archetypes, and resources.
class World {
public:
    using size_type = std::size_t;

private:
    EntityAllocator entities_;
    std::vector<EntityLocation> locations_;  // entity.index -> location
    ComponentRegistry components_;
    Archetypes archetypes_;
    Resources resources_;

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create empty world
    World() = default;

    /// Create with pre-allocated entity capacity
    explicit World(size_type entity_capacity)
        : entities_(entity_capacity)
    {
        locations_.reserve(entity_capacity);
    }

    // =========================================================================
    // Entity Management
    // =========================================================================

    /// Spawn a new entity (in empty archetype)
    [[nodiscard]] Entity spawn() {
        Entity entity = entities_.allocate();

        // Ensure location vector is large enough
        if (entity.index >= locations_.size()) {
            locations_.resize(entity.index + 1, EntityLocation::invalid());
        }

        // Add to empty archetype
        Archetype* empty_arch = archetypes_.get(archetypes_.empty());
        size_type row = empty_arch->add_entity(entity, {});
        locations_[entity.index] = EntityLocation{archetypes_.empty(), row};

        return entity;
    }

    /// Despawn an entity
    /// @return true if entity was alive and is now dead
    bool despawn(Entity entity) {
        if (!is_alive(entity)) {
            return false;
        }

        EntityLocation loc = locations_[entity.index];
        Archetype* arch = archetypes_.get(loc.archetype_id);
        if (!arch) {
            return false;
        }

        // Remove from archetype (may swap with last entity)
        auto swapped = arch->remove_entity(loc.row);

        // Update swapped entity's location
        if (swapped.has_value()) {
            locations_[swapped->index].row = loc.row;
        }

        // Invalidate location and deallocate
        locations_[entity.index] = EntityLocation::invalid();
        entities_.deallocate(entity);

        return true;
    }

    /// Check if entity is alive
    [[nodiscard]] bool is_alive(Entity entity) const noexcept {
        return entities_.is_alive(entity);
    }

    /// Get number of alive entities
    [[nodiscard]] size_type entity_count() const noexcept {
        return entities_.alive_count();
    }

    /// Get entity location
    [[nodiscard]] std::optional<EntityLocation> entity_location(Entity entity) const noexcept {
        if (!is_alive(entity)) {
            return std::nullopt;
        }
        if (entity.index >= locations_.size()) {
            return std::nullopt;
        }
        EntityLocation loc = locations_[entity.index];
        if (!loc.is_valid()) {
            return std::nullopt;
        }
        return loc;
    }

    // =========================================================================
    // Component Registration
    // =========================================================================

    /// Register a component type
    template<typename T>
    ComponentId register_component() {
        return components_.register_component<T>();
    }

    /// Register a cloneable component type
    template<typename T>
    ComponentId register_cloneable() {
        return components_.register_cloneable<T>();
    }

    /// Get component ID by type
    template<typename T>
    [[nodiscard]] std::optional<ComponentId> component_id() const {
        return components_.get_id<T>();
    }

    /// Get component ID by name
    [[nodiscard]] std::optional<ComponentId> component_id_by_name(const std::string& name) const {
        return components_.get_id_by_name(name);
    }

    /// Get component info
    [[nodiscard]] const ComponentInfo* component_info(ComponentId id) const noexcept {
        return components_.get_info(id);
    }

    /// Get component registry
    [[nodiscard]] const ComponentRegistry& component_registry() const noexcept {
        return components_;
    }

    // =========================================================================
    // Component Access
    // =========================================================================

    /// Add or update a component on an entity
    /// @return true if component was added/updated
    template<typename T>
    bool add_component(Entity entity, T component) {
        if (!is_alive(entity)) {
            return false;
        }

        // Ensure component is registered
        ComponentId comp_id = register_component<T>();

        EntityLocation loc = locations_[entity.index];
        Archetype* current_arch = archetypes_.get(loc.archetype_id);
        if (!current_arch) {
            return false;
        }

        // Check if archetype already has this component
        if (current_arch->has_component(comp_id)) {
            // Update existing component
            T* existing = current_arch->template get_component<T>(comp_id, loc.row);
            if (existing) {
                *existing = std::move(component);
                return true;
            }
            return false;
        }

        // Need to move entity to new archetype
        return move_entity_add_component(entity, loc, comp_id, &component);
    }

    /// Remove a component from an entity
    /// @return The removed component if it existed
    template<typename T>
    std::optional<T> remove_component(Entity entity) {
        if (!is_alive(entity)) {
            return std::nullopt;
        }

        auto comp_id_opt = component_id<T>();
        if (!comp_id_opt) {
            return std::nullopt;
        }
        ComponentId comp_id = *comp_id_opt;

        EntityLocation loc = locations_[entity.index];
        Archetype* current_arch = archetypes_.get(loc.archetype_id);
        if (!current_arch || !current_arch->has_component(comp_id)) {
            return std::nullopt;
        }

        // Get value before move
        T* comp_ptr = current_arch->template get_component<T>(comp_id, loc.row);
        if (!comp_ptr) {
            return std::nullopt;
        }
        T value = std::move(*comp_ptr);

        // Move entity to archetype without this component
        move_entity_remove_component(entity, loc, comp_id);

        return value;
    }

    /// Get immutable component reference
    template<typename T>
    [[nodiscard]] const T* get_component(Entity entity) const {
        if (!is_alive(entity)) {
            return nullptr;
        }

        auto comp_id_opt = components_.get_id<T>();
        if (!comp_id_opt) {
            return nullptr;
        }

        EntityLocation loc = locations_[entity.index];
        const Archetype* arch = archetypes_.get(loc.archetype_id);
        if (!arch) {
            return nullptr;
        }

        return arch->template get_component<T>(*comp_id_opt, loc.row);
    }

    /// Get mutable component reference
    template<typename T>
    [[nodiscard]] T* get_component(Entity entity) {
        if (!is_alive(entity)) {
            return nullptr;
        }

        auto comp_id_opt = components_.get_id<T>();
        if (!comp_id_opt) {
            return nullptr;
        }

        EntityLocation loc = locations_[entity.index];
        Archetype* arch = archetypes_.get(loc.archetype_id);
        if (!arch) {
            return nullptr;
        }

        return arch->template get_component<T>(*comp_id_opt, loc.row);
    }

    /// Check if entity has a component
    template<typename T>
    [[nodiscard]] bool has_component(Entity entity) const {
        if (!is_alive(entity)) {
            return false;
        }

        auto comp_id_opt = components_.get_id<T>();
        if (!comp_id_opt) {
            return false;
        }

        EntityLocation loc = locations_[entity.index];
        const Archetype* arch = archetypes_.get(loc.archetype_id);
        if (!arch) {
            return false;
        }

        return arch->has_component(*comp_id_opt);
    }

    // =========================================================================
    // Resources
    // =========================================================================

    /// Insert a resource
    template<typename R>
    void insert_resource(R resource) {
        resources_.insert(std::move(resource));
    }

    /// Remove a resource
    template<typename R>
    std::optional<R> remove_resource() {
        return resources_.remove<R>();
    }

    /// Get immutable resource
    template<typename R>
    [[nodiscard]] const R* resource() const {
        return resources_.get<R>();
    }

    /// Get mutable resource
    template<typename R>
    [[nodiscard]] R* resource() {
        return resources_.get<R>();
    }

    /// Check if resource exists
    template<typename R>
    [[nodiscard]] bool has_resource() const {
        return resources_.contains<R>();
    }

    // =========================================================================
    // Queries
    // =========================================================================

    /// Create a query state
    [[nodiscard]] QueryState query(QueryDescriptor descriptor) {
        QueryState state(std::move(descriptor));
        state.update(archetypes_);
        return state;
    }

    /// Update a query state (call when archetypes may have changed)
    void update_query(QueryState& state) {
        state.update(archetypes_);
    }

    /// Create a query iterator
    [[nodiscard]] QueryIter query_iter(const QueryState& state) const {
        return QueryIter(&archetypes_, &state);
    }

    // =========================================================================
    // Archetype Access
    // =========================================================================

    /// Get archetypes
    [[nodiscard]] const Archetypes& archetypes() const noexcept {
        return archetypes_;
    }

    /// Get mutable archetypes
    [[nodiscard]] Archetypes& archetypes() noexcept {
        return archetypes_;
    }

    // =========================================================================
    // Maintenance
    // =========================================================================

    /// Clear all entities and reset world
    void clear() {
        // Clear all archetypes
        for (auto& arch_ptr : archetypes_) {
            // Clear storages (calls destructors)
            for (auto& storage : arch_ptr->storages()) {
                storage.clear();
            }
        }

        entities_.clear();
        locations_.clear();
        resources_.clear();
    }

    // =========================================================================
    // Hot-Reload Support
    // =========================================================================

    /// Add raw component data to entity (for snapshot restore)
    /// @param entity Target entity
    /// @param comp_id Component type ID
    /// @param data Raw component data
    /// @param size Data size in bytes
    /// @return true if component was added
    bool add_component_raw(Entity entity, ComponentId comp_id, const void* data, std::size_t size) {
        if (!is_alive(entity)) {
            return false;
        }

        const ComponentInfo* info = components_.get_info(comp_id);
        if (!info || info->size != size) {
            return false;
        }

        EntityLocation loc = locations_[entity.index];
        Archetype* current_arch = archetypes_.get(loc.archetype_id);
        if (!current_arch) {
            return false;
        }

        // Check if archetype already has this component
        if (current_arch->has_component(comp_id)) {
            // Update existing - use clone_fn if available, else memcpy
            void* dest = current_arch->get_component_raw(comp_id, loc.row);
            if (dest) {
                if (info->clone_fn) {
                    // First destruct existing, then clone
                    if (info->drop_fn) {
                        info->drop_fn(dest);
                    }
                    info->clone_fn(data, dest);
                } else {
                    std::memcpy(dest, data, size);
                }
                return true;
            }
            return false;
        }

        // Need to move entity to new archetype with this component
        return move_entity_add_component_raw(entity, loc, comp_id, data, size);
    }

    /// Update entity location directly (for snapshot restore)
    void set_entity_location(Entity entity, EntityLocation location) {
        if (entity.index >= locations_.size()) {
            locations_.resize(entity.index + 1, EntityLocation::invalid());
        }
        locations_[entity.index] = location;
    }

    /// Get mutable locations vector (for snapshot restore)
    [[nodiscard]] std::vector<EntityLocation>& locations() noexcept {
        return locations_;
    }

    /// Get entity allocator (for snapshot restore)
    [[nodiscard]] EntityAllocator& entity_allocator() noexcept {
        return entities_;
    }

    /// Get mutable component registry (for snapshot restore)
    [[nodiscard]] ComponentRegistry& component_registry_mut() noexcept {
        return components_;
    }

private:
    /// Move entity to new archetype with raw component data
    bool move_entity_add_component_raw(Entity entity, EntityLocation old_loc,
                                        ComponentId new_comp_id, const void* data, std::size_t size) {
        Archetype* old_arch = archetypes_.get(old_loc.archetype_id);
        if (!old_arch) return false;

        // Build new component set
        std::vector<ComponentId> new_components = old_arch->components();
        new_components.push_back(new_comp_id);
        std::sort(new_components.begin(), new_components.end());

        // Get or create target archetype
        ArchetypeId new_arch_id = archetypes_.get_or_create(new_components, components_);
        Archetype* new_arch = archetypes_.get(new_arch_id);
        if (!new_arch) return false;

        // Prepare component data for new archetype
        std::vector<const void*> component_data;
        for (ComponentId comp_id : new_arch->components()) {
            if (comp_id == new_comp_id) {
                component_data.push_back(data);
            } else {
                component_data.push_back(old_arch->get_component_raw(comp_id, old_loc.row));
            }
        }

        // Add to new archetype
        size_type new_row = new_arch->add_entity(entity, component_data);

        // Remove from old archetype
        auto swapped = old_arch->remove_entity(old_loc.row);

        // Update swapped entity's location
        if (swapped.has_value()) {
            locations_[swapped->index].row = old_loc.row;
        }

        // Update this entity's location
        locations_[entity.index] = EntityLocation{new_arch_id, new_row};

        return true;
    }

    /// Move entity to a new archetype with an added component
    template<typename T>
    bool move_entity_add_component(Entity entity, EntityLocation old_loc,
                                    ComponentId new_comp_id, T* component) {
        Archetype* old_arch = archetypes_.get(old_loc.archetype_id);
        if (!old_arch) return false;

        // Build new component set
        std::vector<ComponentId> new_components = old_arch->components();
        new_components.push_back(new_comp_id);
        std::sort(new_components.begin(), new_components.end());

        // Get or create target archetype
        ArchetypeId new_arch_id = archetypes_.get_or_create(new_components, components_);
        Archetype* new_arch = archetypes_.get(new_arch_id);
        if (!new_arch) return false;

        // Prepare component data for new archetype
        std::vector<const void*> component_data;
        for (ComponentId comp_id : new_arch->components()) {
            if (comp_id == new_comp_id) {
                component_data.push_back(component);
            } else {
                component_data.push_back(old_arch->get_component_raw(comp_id, old_loc.row));
            }
        }

        // Add to new archetype
        size_type new_row = new_arch->add_entity(entity, component_data);

        // Remove from old archetype (without dropping - components were moved)
        auto swapped = old_arch->remove_entity(old_loc.row);

        // Update swapped entity's location
        if (swapped.has_value()) {
            locations_[swapped->index].row = old_loc.row;
        }

        // Update this entity's location
        locations_[entity.index] = EntityLocation{new_arch_id, new_row};

        return true;
    }

    /// Move entity to a new archetype with a removed component
    void move_entity_remove_component(Entity entity, EntityLocation old_loc,
                                       ComponentId removed_comp_id) {
        Archetype* old_arch = archetypes_.get(old_loc.archetype_id);
        if (!old_arch) return;

        // Build new component set
        std::vector<ComponentId> new_components;
        for (ComponentId comp_id : old_arch->components()) {
            if (comp_id != removed_comp_id) {
                new_components.push_back(comp_id);
            }
        }

        // Get or create target archetype
        ArchetypeId new_arch_id = archetypes_.get_or_create(new_components, components_);
        Archetype* new_arch = archetypes_.get(new_arch_id);
        if (!new_arch) return;

        // Prepare component data for new archetype
        std::vector<const void*> component_data;
        for (ComponentId comp_id : new_arch->components()) {
            component_data.push_back(old_arch->get_component_raw(comp_id, old_loc.row));
        }

        // Add to new archetype
        size_type new_row = new_arch->add_entity(entity, component_data);

        // Remove from old archetype
        auto swapped = old_arch->remove_entity(old_loc.row);

        // Update swapped entity's location
        if (swapped.has_value()) {
            locations_[swapped->index].row = old_loc.row;
        }

        // Update this entity's location
        locations_[entity.index] = EntityLocation{new_arch_id, new_row};
    }
};

// =============================================================================
// EntityBuilder
// =============================================================================

/// Fluent API for building entities with components
template<typename WorldT = World>
class EntityBuilder {
private:
    WorldT* world_;
    Entity entity_;

public:
    EntityBuilder(WorldT* world)
        : world_(world)
        , entity_(world->spawn()) {}

    /// Add a component
    template<typename T>
    EntityBuilder& with(T component) {
        world_->add_component(entity_, std::move(component));
        return *this;
    }

    /// Get the entity ID
    [[nodiscard]] Entity id() const noexcept {
        return entity_;
    }

    /// Finish building and return entity
    [[nodiscard]] Entity build() {
        return entity_;
    }

    /// Implicit conversion to Entity
    operator Entity() const noexcept {
        return entity_;
    }
};

/// Extension method for World
inline EntityBuilder<World> build_entity(World& world) {
    return EntityBuilder<World>(&world);
}

} // namespace void_ecs
