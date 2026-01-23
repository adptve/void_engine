#pragma once

/// @file ir.hpp
/// @brief Main include for void_ir module
///
/// void_ir is a declarative intermediate representation and patch system
/// for atomic state changes. It provides:
/// - Namespace isolation for multi-tenant applications
/// - Atomic transactions at frame boundaries
/// - Type-safe value representation
/// - Patch-based state modifications
/// - Inter-thread communication via PatchBus
/// - Schema-based validation
/// - Snapshot/rollback support
/// - Batch optimization
///
/// @example Basic usage:
/// @code
/// #include <void_engine/ir/ir.hpp>
///
/// using namespace void_ir;
///
/// int main() {
///     // Create namespace registry
///     NamespaceRegistry namespaces;
///     NamespaceId game_ns = namespaces.create("game");
///
///     // Get namespace for entity allocation
///     Namespace* ns = namespaces.get(game_ns);
///
///     // Create entity reference
///     EntityRef player(game_ns, ns->allocate_entity());
///
///     // Build a transaction
///     TransactionBuilder builder(game_ns);
///     builder
///         .description("Create player")
///         .create_entity(player, "Player")
///         .add_component(player, "Transform", Value::empty_object())
///         .set_position(player, Vec3{0, 0, 0});
///
///     // Build and submit
///     Transaction tx = builder.build(TransactionId(0));
///     tx.submit();
///
///     // Apply patches...
/// }
/// @endcode
///
/// @example Using the PatchBus:
/// @code
/// PatchBus bus;
///
/// // Subscribe to component changes
/// auto sub_id = bus.subscribe(
///     PatchFilter::for_kinds({PatchKind::Component}),
///     [](const PatchEvent& event) {
///         // Handle patch...
///     }
/// );
///
/// // Publish patches
/// bus.publish(ComponentPatch::set(entity, "Health", Value(100)), ns_id);
///
/// // Unsubscribe when done
/// bus.unsubscribe(sub_id);
/// @endcode
///
/// @example Schema validation:
/// @code
/// SchemaRegistry schemas;
///
/// // Define component schema
/// ComponentSchema transform_schema("Transform");
/// transform_schema
///     .field(FieldDescriptor::vec3("position"))
///     .field(FieldDescriptor::vec4("rotation"))
///     .field(FieldDescriptor::vec3("scale").with_default(Value(Vec3{1, 1, 1})));
///
/// schemas.register_schema(std::move(transform_schema));
///
/// // Validate patches
/// PatchValidator validator(schemas);
/// auto result = validator.validate(patch, permissions);
/// if (!result.valid) {
///     std::cerr << result.first_error() << std::endl;
/// }
/// @endcode
///
/// @example Snapshots and rollback:
/// @code
/// SnapshotManager snapshots(10);  // Keep max 10 snapshots
///
/// // Create snapshot before changes
/// SnapshotId before_id = snapshots.create(ns_id, "Before changes");
/// Snapshot* before = snapshots.get(before_id);
///
/// // ... make changes ...
///
/// // Create snapshot after changes
/// SnapshotId after_id = snapshots.create(ns_id, "After changes");
/// Snapshot* after = snapshots.get(after_id);
///
/// // Compute difference
/// SnapshotDelta delta = SnapshotDelta::compute(*before, *after);
///
/// // Convert to patches for replay or rollback
/// PatchBatch replay_patches = delta.to_patches();
/// @endcode
///
/// @example Batch optimization:
/// @code
/// BatchOptimizer optimizer;
///
/// // Optimize a batch of patches
/// PatchBatch optimized = optimizer.optimize(raw_patches);
///
/// // Check optimization results
/// const auto& stats = optimizer.stats();
/// std::cout << "Reduced " << stats.original_count << " patches to "
///           << stats.optimized_count << " ("
///           << stats.reduction_percent() << "% reduction)" << std::endl;
/// @endcode

#include "fwd.hpp"
#include "namespace.hpp"
#include "value.hpp"
#include "patch.hpp"
#include "transaction.hpp"
#include "bus.hpp"
#include "validation.hpp"
#include "snapshot.hpp"
#include "batch.hpp"

namespace void_ir {

/// Version information
struct Version {
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;
    static constexpr int PATCH = 0;
};

} // namespace void_ir
