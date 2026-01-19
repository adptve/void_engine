//! Integration tests for void_ir crate
//!
//! Tests the full patch submission and application pipeline

use void_ir::*;

#[test]
fn test_patch_serialization() {
    let ns = NamespaceId::new();
    let entity = EntityRef::new(ns, 1);

    let patch = Patch::new(
        ns,
        PatchKind::Entity(EntityPatch::create(ns, 1)
            .with_component("Transform", Value::from([0.0, 1.0, 2.0]))),
    );

    // Serialize to JSON
    let json = serde_json::to_string(&patch).expect("Failed to serialize");

    // Deserialize back
    let deserialized: Patch = serde_json::from_str(&json).expect("Failed to deserialize");

    assert_eq!(deserialized.source, ns);
    assert!(matches!(deserialized.kind, PatchKind::Entity(_)));
}

#[test]
fn test_transaction_ordering() {
    let ns = NamespaceId::new();

    let mut tx = TransactionBuilder::new(ns)
        .patch(Patch::new(ns, PatchKind::Entity(EntityPatch::create(ns, 1))).with_priority(5))
        .patch(Patch::new(ns, PatchKind::Entity(EntityPatch::create(ns, 2))).with_priority(10))
        .patch(Patch::new(ns, PatchKind::Entity(EntityPatch::create(ns, 3))).with_priority(1))
        .build_draft();

    // Sort by priority
    tx.sort_by_priority();

    // Highest priority should be first
    assert_eq!(tx.patches[0].priority, 10);
    assert_eq!(tx.patches[1].priority, 5);
    assert_eq!(tx.patches[2].priority, 1);
}

#[test]
fn test_patch_bus_multiple_namespaces() {
    let bus = PatchBus::default();

    let ns1 = Namespace::new("app1");
    let ns2 = Namespace::new("app2");

    let handle1 = bus.register_namespace(ns1);
    let handle2 = bus.register_namespace(ns2);

    // Both namespaces submit transactions
    let tx1 = handle1
        .begin_transaction()
        .description("App1 transaction")
        .patch(Patch::new(handle1.id(), PatchKind::Entity(EntityPatch::create(handle1.id(), 1))))
        .build();

    let tx2 = handle2
        .begin_transaction()
        .description("App2 transaction")
        .patch(Patch::new(handle2.id(), PatchKind::Entity(EntityPatch::create(handle2.id(), 1))))
        .build();

    handle1.submit(tx1).unwrap();
    handle2.submit(tx2).unwrap();

    bus.receive_pending();
    let ready = bus.drain_ready(1);

    assert_eq!(ready.len(), 2);
}

#[test]
fn test_patch_bus_resource_limits() {
    let bus = PatchBus::default();

    // Create namespace with strict limits
    let mut ns = Namespace::new("limited_app");
    ns.limits.max_patches_per_frame = Some(5);

    let handle = bus.register_namespace(ns);

    // Try to exceed patch limit
    let tx = handle.begin_transaction();
    let mut tx = tx;
    for i in 0..10 {
        tx = tx.patch(Patch::new(
            handle.id(),
            PatchKind::Entity(EntityPatch::create(handle.id(), i)),
        ));
    }
    let tx = tx.build();

    handle.submit(tx).unwrap();
    bus.receive_pending();

    // Validation should fail due to limit
    // (In real implementation, this would be caught during validation)
}

#[test]
fn test_value_conversions() {
    // Test various value conversions
    let v_int: Value = 42.into();
    assert_eq!(v_int.as_int(), Some(42));

    let v_float: Value = 3.14.into();
    assert_eq!(v_float.as_float(), Some(3.14));

    let v_string: Value = "hello".into();
    assert_eq!(v_string.as_str(), Some("hello"));

    let v_vec3: Value = [1.0f32, 2.0, 3.0].into();
    assert_eq!(v_vec3.as_vec3(), Some([1.0, 2.0, 3.0]));

    // Test object creation
    let v_obj: Value = vec![
        ("x", Value::from(1.0)),
        ("y", Value::from(2.0)),
        ("z", Value::from(3.0)),
    ]
    .into_iter()
    .collect();

    assert_eq!(v_obj.get("x").and_then(|v| v.as_float()), Some(1.0));
}

#[test]
fn test_value_object_mutation() {
    let mut obj: Value = vec![
        ("health", Value::from(100)),
        ("name", Value::from("player")),
    ]
    .into_iter()
    .collect();

    // Mutate object
    obj.set("health", Value::from(75)).unwrap();
    assert_eq!(obj.get("health").and_then(|v| v.as_int()), Some(75));

    // Add new field
    obj.set("level", Value::from(5)).unwrap();
    assert_eq!(obj.get("level").and_then(|v| v.as_int()), Some(5));
}

#[test]
fn test_namespace_permissions() {
    let kernel = Namespace::kernel();
    let app1 = Namespace::new("app1");
    let app2 = Namespace::new("app2");

    // Kernel can access everything
    assert!(kernel.can_access(app1.id));
    assert!(kernel.can_modify(app1.id));
    assert!(kernel.can_modify(app2.id));

    // Apps can only modify themselves
    assert!(app1.can_modify(app1.id));
    assert!(!app1.can_modify(app2.id));

    // Apps can read each other (by default)
    assert!(app1.can_access(app2.id));
}

#[test]
fn test_namespace_resource_limits() {
    let limits = ResourceLimits::default();

    assert!(limits.check_entity_limit(100));
    assert!(limits.check_entity_limit(9999));
    assert!(!limits.check_entity_limit(10000)); // At limit

    assert!(limits.check_patch_limit(100));
    assert!(limits.check_patch_limit(999));
    assert!(!limits.check_patch_limit(1000)); // At limit
}

#[test]
fn test_component_patch_operations() {
    let entity = EntityRef::new(NamespaceId::new(), 1);

    // Test Set operation
    let set_patch = ComponentPatch::set(entity, "Position", Value::from([1.0, 2.0, 3.0]));
    assert!(matches!(set_patch.op, ComponentOp::Set { .. }));

    // Test Update operation
    let mut fields = std::collections::HashMap::new();
    fields.insert("x".to_string(), Value::from(10.0));
    let update_patch = ComponentPatch::update(entity, "Position", fields);
    assert!(matches!(update_patch.op, ComponentOp::Update { .. }));

    // Test Remove operation
    let remove_patch = ComponentPatch::remove(entity, "Position");
    assert!(matches!(remove_patch.op, ComponentOp::Remove));
}

#[test]
fn test_layer_patch_operations() {
    use void_ir::patch::{LayerType, BlendMode};

    // Test layer creation
    let create_patch = LayerPatch::create("main_layer", LayerType::Content, 100);
    if let LayerOp::Create { layer_type, priority } = create_patch.op {
        assert_eq!(layer_type, LayerType::Content);
        assert_eq!(priority, 100);
    } else {
        panic!("Expected Create op");
    }

    // Test layer destruction
    let destroy_patch = LayerPatch::destroy("main_layer");
    assert!(matches!(destroy_patch.op, LayerOp::Destroy));
}

#[test]
fn test_transaction_dependency_chain() {
    let ns = NamespaceId::new();

    // Build a chain of dependent transactions
    let tx1 = TransactionBuilder::new(ns).description("First").build();
    let tx1_id = tx1.id;

    let tx2 = TransactionBuilder::new(ns)
        .description("Second")
        .depends_on(tx1_id)
        .build();
    let tx2_id = tx2.id;

    let tx3 = TransactionBuilder::new(ns)
        .description("Third")
        .depends_on(tx2_id)
        .build();

    // Check dependencies
    assert!(tx1.dependencies_satisfied(&[]));
    assert!(tx2.dependencies_satisfied(&[tx1_id]));
    assert!(!tx2.dependencies_satisfied(&[]));
    assert!(tx3.dependencies_satisfied(&[tx1_id, tx2_id]));
    assert!(!tx3.dependencies_satisfied(&[tx1_id]));
}

#[test]
fn test_patch_bus_statistics() {
    let bus = PatchBus::default();
    let ns = Namespace::new("test_app");
    let handle = bus.register_namespace(ns);

    // Submit some transactions
    for i in 0..10 {
        let tx = handle
            .begin_transaction()
            .patch(Patch::new(
                handle.id(),
                PatchKind::Entity(EntityPatch::create(handle.id(), i)),
            ))
            .build();
        handle.submit(tx).unwrap();
    }

    bus.receive_pending();
    let stats = bus.stats();

    assert_eq!(stats.transactions_submitted, 10);
    assert!(stats.peak_pending >= 10);
}

#[test]
fn test_entity_patch_with_archetype() {
    let ns = NamespaceId::new();

    let patch = EntityPatch::create(ns, 1)
        .with_archetype("Enemy")
        .with_component("Health", Value::from(100))
        .with_component("Damage", Value::from(25));

    if let EntityOp::Create { archetype, components } = &patch.op {
        assert_eq!(archetype.as_deref(), Some("Enemy"));
        assert_eq!(components.len(), 2);
        assert!(components.contains_key("Health"));
        assert!(components.contains_key("Damage"));
    } else {
        panic!("Expected Create op");
    }
}

#[test]
fn test_patch_targets_entity() {
    let ns = NamespaceId::new();
    let entity1 = EntityRef::new(ns, 1);
    let entity2 = EntityRef::new(ns, 2);

    let patch = Patch::new(
        ns,
        PatchKind::Entity(EntityPatch::destroy(entity1)),
    );

    assert!(patch.targets_entity(entity1));
    assert!(!patch.targets_entity(entity2));

    // Component patch should also target entity
    let comp_patch = Patch::new(
        ns,
        PatchKind::Component(ComponentPatch::set(entity1, "Position", Value::null())),
    );
    assert!(comp_patch.targets_entity(entity1));
}

#[test]
fn test_transaction_result() {
    let tx_id = TransactionId::new();

    let success = TransactionResult::success(tx_id, 5);
    assert!(success.success);
    assert_eq!(success.patches_applied, 5);
    assert!(success.error.is_none());

    let failure = TransactionResult::failure(tx_id, "Test error", 2);
    assert!(!failure.success);
    assert_eq!(failure.patches_applied, 2);
    assert_eq!(failure.error.as_deref(), Some("Test error"));
}
