//! Complete IR System Example
//!
//! This example demonstrates the full IR system with:
//! - Patch validation
//! - Transaction batching and optimization
//! - Snapshot-based rollback
//! - Conflict detection

use void_ir::{
    batch::{BatchOptimizer, PatchBatch},
    namespace::{Namespace, NamespaceId},
    patch::{ComponentPatch, EntityPatch, EntityRef, Patch, PatchKind},
    snapshot::{SnapshotManager, StateSnapshot},
    transaction::{ConflictDetector, TransactionBuilder},
    validation::{ComponentSchema, FieldSchema, PatchValidator, ValidationContext},
    value::Value,
    PatchBus, PatchBusConfig,
};

fn main() {
    println!("=== Complete IR System Demo ===\n");

    // 1. Setup
    println!("1. Setting up IR system components...");
    let mut patch_bus = PatchBus::new(PatchBusConfig::default());
    let mut snapshot_manager = SnapshotManager::new(10, 10 * 1024 * 1024);
    let mut conflict_detector = ConflictDetector::new();
    let batch_optimizer = BatchOptimizer::new();

    // Create namespaces
    let app1 = Namespace::new("physics_app");
    let app2 = Namespace::new("rendering_app");

    let app1_handle = patch_bus.register_namespace(app1.clone());
    let app2_handle = patch_bus.register_namespace(app2.clone());

    println!("   - Created namespaces: {} and {}", app1.name, app2.name);
    println!("   - Registered with patch bus\n");

    // 2. Validation Setup
    println!("2. Setting up validation...");
    let mut validator = PatchValidator::new();

    // Register namespaces with validator
    validator.context_mut().register_namespace(std::sync::Arc::new(app1.clone()));
    validator.context_mut().register_namespace(std::sync::Arc::new(app2.clone()));

    // Register component schemas
    let transform_schema = ComponentSchema::new("Transform")
        .with_required_field("position", FieldSchema::Vec3)
        .with_optional_field("rotation", FieldSchema::Vec4)
        .with_optional_field("scale", FieldSchema::Vec3);

    validator.context_mut().register_component_schema(transform_schema);
    println!("   - Registered Transform component schema\n");

    // 3. Create and Submit Transactions
    println!("3. Creating transactions...");

    let entity1 = EntityRef::new(app1.id, 1);
    let entity2 = EntityRef::new(app2.id, 1);

    // App 1: Create entity with transform
    let tx1 = app1_handle
        .begin_transaction()
        .description("Spawn physics object")
        .patch(Patch::new(
            app1.id,
            PatchKind::Entity(EntityPatch::create(app1.id, 1)),
        ))
        .patch(Patch::new(
            app1.id,
            PatchKind::Component(ComponentPatch::set(
                entity1,
                "Transform",
                Value::from_iter([
                    ("position", Value::from([0.0, 0.0, 0.0])),
                    ("scale", Value::from([1.0, 1.0, 1.0])),
                ]),
            )),
        ))
        .build();

    println!("   - App 1 transaction: {} patches", tx1.len());

    // App 2: Create entity
    let tx2 = app2_handle
        .begin_transaction()
        .description("Spawn render object")
        .patch(Patch::new(
            app2.id,
            PatchKind::Entity(EntityPatch::create(app2.id, 1)),
        ))
        .build();

    println!("   - App 2 transaction: {} patches\n", tx2.len());

    // 4. Validate Transactions
    println!("4. Validating transactions...");

    // First, register entities that we're about to create
    validator.context_mut().register_entity(entity1);
    validator.context_mut().register_entity(entity2);

    match validator.validate_patches(&tx1.patches) {
        Ok(_) => println!("   - Transaction 1: VALID"),
        Err(e) => println!("   - Transaction 1: INVALID - {}", e),
    }

    match validator.validate_patches(&tx2.patches) {
        Ok(_) => println!("   - Transaction 2: VALID\n"),
        Err(e) => println!("   - Transaction 2: INVALID - {}\n", e),
    }

    // 5. Conflict Detection
    println!("5. Checking for conflicts...");

    conflict_detector.add_transaction(&tx1);

    if conflict_detector.has_conflict(&tx2) {
        println!("   - CONFLICT detected between transactions!");
        let conflicts = conflict_detector.get_conflicts(&tx2);
        for conflict in conflicts {
            println!("     - {}", conflict);
        }
    } else {
        println!("   - No conflicts detected");
    }
    println!();

    // 6. Batch Optimization
    println!("6. Testing batch optimization...");

    let mut batch = PatchBatch::new();

    // Add redundant patches
    for i in 0..5 {
        batch.add(Patch::new(
            app1.id,
            PatchKind::Component(ComponentPatch::set(
                entity1,
                "Transform",
                Value::from_iter([("position", Value::from([i as f64, 0.0, 0.0]))]),
            )),
        ));
    }

    println!("   - Original patches: {}", batch.len());
    batch.optimize();
    println!("   - Optimized patches: {}", batch.len());
    println!("   - Optimization ratio: {:.1}%", batch.stats().optimization_ratio() * 100.0);
    println!("   - Patches merged: {}\n", batch.stats().patches_merged);

    // 7. Snapshot and Rollback
    println!("7. Creating state snapshot...");

    let mut snapshot1 = StateSnapshot::new(1);
    snapshot1.entities.insert(entity1, true, None, vec![]);
    snapshot1.components.set(
        entity1,
        "Transform".to_string(),
        Value::from_iter([("position", Value::from([0.0, 0.0, 0.0]))]),
    );

    let id1 = snapshot_manager.store(snapshot1);
    println!("   - Snapshot created (ID: {:?})", id1.raw());
    println!("   - Memory used: {} bytes\n", snapshot_manager.memory_used());

    // 8. Compute Diff
    println!("8. Computing state diff...");

    let mut snapshot2 = StateSnapshot::new(2);
    snapshot2.entities.insert(entity1, true, None, vec![]);
    snapshot2.entities.insert(entity2, true, None, vec![]);
    snapshot2.components.set(
        entity1,
        "Transform".to_string(),
        Value::from_iter([("position", Value::from([10.0, 5.0, 0.0]))]),
    );

    if let Some(snap1) = snapshot_manager.get(id1) {
        let diff = snapshot2.diff(snap1);
        println!("   - Diff contains {} patches", diff.len());
        for (i, patch) in diff.iter().enumerate() {
            println!("     {}. {:?}", i + 1, patch.kind);
        }
    }
    println!();

    // 9. Statistics
    println!("9. Final statistics:");
    let stats = patch_bus.stats();
    println!("   - Total transactions submitted: {}", stats.transactions_submitted);
    println!("   - Total transactions committed: {}", stats.transactions_committed);
    println!("   - Total patches applied: {}", stats.patches_applied);
    println!("   - Snapshots stored: {}", snapshot_manager.len());
    println!("   - Total memory: {} bytes", snapshot_manager.memory_used());

    println!("\n=== Demo Complete ===");
}
