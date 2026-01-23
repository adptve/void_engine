#pragma once

/// @file ecs.hpp
/// @brief Main include for void_ecs module
///
/// This header includes all void_ecs components:
/// - Entity: Generational entity handles
/// - Component: Type-erased component storage
/// - Archetype: Cache-efficient entity grouping
/// - Query: Filtered entity iteration
/// - System: Game logic execution
/// - World: Main ECS container
///
/// @example Basic usage:
/// @code
/// #include <void_engine/ecs/ecs.hpp>
///
/// using namespace void_ecs;
///
/// struct Position { float x, y, z; };
/// struct Velocity { float x, y, z; };
///
/// int main() {
///     World world;
///
///     // Register components
///     world.register_component<Position>();
///     world.register_component<Velocity>();
///
///     // Spawn entities
///     Entity e = world.spawn();
///     world.add_component(e, Position{0, 0, 0});
///     world.add_component(e, Velocity{1, 0, 0});
///
///     // Or use builder pattern
///     Entity e2 = build_entity(world)
///         .with(Position{5, 0, 0})
///         .with(Velocity{0, 1, 0})
///         .build();
///
///     // Query and iterate
///     auto pos_id = *world.component_id<Position>();
///     auto vel_id = *world.component_id<Velocity>();
///
///     auto query = world.query(
///         QueryDescriptor()
///             .read(pos_id)
///             .write(vel_id)
///             .build()
///     );
///
///     // Iterate with query
///     auto iter = world.query_iter(query);
///     while (!iter.empty()) {
///         Entity entity = iter.entity();
///         // Process entity...
///         iter.next();
///     }
/// }
/// @endcode

#include "fwd.hpp"
#include "entity.hpp"
#include "component.hpp"
#include "archetype.hpp"
#include "query.hpp"
#include "world.hpp"
#include "system.hpp"

namespace void_ecs {

/// Version information
struct Version {
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;
    static constexpr int PATCH = 0;
};

} // namespace void_ecs
