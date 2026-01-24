#pragma once

/// @file bundle.hpp
/// @brief Component bundles for void_ecs
///
/// Bundles allow grouping multiple components together for convenient
/// entity spawning and component addition.

#include "fwd.hpp"
#include "entity.hpp"
#include "component.hpp"
#include "world.hpp"
#include "hierarchy.hpp"

#include <tuple>
#include <type_traits>

namespace void_ecs {

// =============================================================================
// Bundle Concept
// =============================================================================

/// Concept for bundle types
template<typename T>
concept Bundle = requires(T bundle, World& world, Entity entity) {
    { bundle.add_to_entity(world, entity) };
};

// =============================================================================
// Tuple Bundle
// =============================================================================

/// Bundle implemented as a tuple of components
template<typename... Components>
class TupleBundle {
public:
    std::tuple<Components...> components;

    TupleBundle() = default;

    explicit TupleBundle(Components... comps)
        : components(std::move(comps)...) {}

    /// Add all components to an entity
    void add_to_entity(World& world, Entity entity) {
        add_impl(world, entity, std::index_sequence_for<Components...>{});
    }

private:
    template<std::size_t... Is>
    void add_impl(World& world, Entity entity, std::index_sequence<Is...>) {
        (world.add_component(entity, std::move(std::get<Is>(components))), ...);
    }
};

/// Create a tuple bundle from components
template<typename... Components>
[[nodiscard]] TupleBundle<std::decay_t<Components>...> make_bundle(Components&&... components) {
    return TupleBundle<std::decay_t<Components>...>(std::forward<Components>(components)...);
}

// =============================================================================
// Common Bundles
// =============================================================================

/// Transform bundle - position, rotation, scale
struct TransformBundle {
    LocalTransform local;
    GlobalTransform global;

    TransformBundle()
        : local(LocalTransform::identity())
        , global(GlobalTransform::identity()) {}

    explicit TransformBundle(const Vec3& position)
        : local(LocalTransform::from_position(position))
        , global(GlobalTransform::identity()) {}

    TransformBundle(const Vec3& position, const Quat& rotation)
        : local({position, rotation, Vec3::one()})
        , global(GlobalTransform::identity()) {}

    TransformBundle(const Vec3& position, const Quat& rotation, const Vec3& scale)
        : local({position, rotation, scale})
        , global(GlobalTransform::identity()) {}

    void add_to_entity(World& world, Entity entity) {
        world.add_component(entity, local);
        world.add_component(entity, global);
    }
};

/// Spatial bundle - transform with visibility
struct SpatialBundle {
    LocalTransform local;
    GlobalTransform global;
    Visible visible;
    InheritedVisibility inherited_visibility;

    SpatialBundle()
        : local(LocalTransform::identity())
        , global(GlobalTransform::identity())
        , visible(true)
        , inherited_visibility(true) {}

    explicit SpatialBundle(const Vec3& position)
        : local(LocalTransform::from_position(position))
        , global(GlobalTransform::identity())
        , visible(true)
        , inherited_visibility(true) {}

    SpatialBundle(const Vec3& position, bool is_visible)
        : local(LocalTransform::from_position(position))
        , global(GlobalTransform::identity())
        , visible(is_visible)
        , inherited_visibility(is_visible) {}

    void add_to_entity(World& world, Entity entity) {
        world.add_component(entity, local);
        world.add_component(entity, global);
        world.add_component(entity, visible);
        world.add_component(entity, inherited_visibility);
    }
};

/// Hierarchy bundle - parent/children support
struct HierarchyBundle {
    Children children;
    HierarchyDepth depth;

    HierarchyBundle()
        : children()
        , depth(0) {}

    void add_to_entity(World& world, Entity entity) {
        world.add_component(entity, children);
        world.add_component(entity, depth);
    }
};

// =============================================================================
// World Extensions for Bundles
// =============================================================================

/// Spawn an entity with a bundle
template<Bundle B>
[[nodiscard]] Entity spawn_with_bundle(World& world, B bundle) {
    Entity entity = world.spawn();
    bundle.add_to_entity(world, entity);
    return entity;
}

/// Spawn an entity with components (variadic)
template<typename... Components>
[[nodiscard]] Entity spawn_with(World& world, Components&&... components) {
    Entity entity = world.spawn();
    (world.add_component(entity, std::forward<Components>(components)), ...);
    return entity;
}

// =============================================================================
// EntityBuilder Extensions
// =============================================================================

/// Extended entity builder with bundle support
template<typename WorldT = World>
class BundleEntityBuilder {
private:
    WorldT* world_;
    Entity entity_;

public:
    explicit BundleEntityBuilder(WorldT* world)
        : world_(world)
        , entity_(world->spawn()) {}

    /// Add a single component
    template<typename T>
    BundleEntityBuilder& with(T component) {
        world_->add_component(entity_, std::move(component));
        return *this;
    }

    /// Add a bundle
    template<Bundle B>
    BundleEntityBuilder& with_bundle(B bundle) {
        bundle.add_to_entity(*world_, entity_);
        return *this;
    }

    /// Add multiple components at once
    template<typename... Components>
    BundleEntityBuilder& with_components(Components&&... components) {
        (world_->add_component(entity_, std::forward<Components>(components)), ...);
        return *this;
    }

    /// Set parent
    BundleEntityBuilder& child_of(Entity parent) {
        set_parent(*world_, entity_, parent);
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

/// Create a bundle-aware entity builder
inline BundleEntityBuilder<World> build_entity_ex(World& world) {
    return BundleEntityBuilder<World>(&world);
}

} // namespace void_ecs
