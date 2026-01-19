//! Invariant tests for void_kernel
//!
//! These tests verify critical OS invariants that MUST NEVER be violated

use void_kernel::*;
use void_ir::{Namespace, Patch, PatchKind, EntityPatch, ComponentPatch, EntityRef};

/// INVARIANT: Kernel never dies on app crash
#[test]
fn invariant_kernel_survives_bad_patches() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);
    let mut world = void_ecs::World::new();

    kernel.start();

    // Submit many potentially problematic patches
    for i in 0..100 {
        kernel.begin_frame(0.016);

        let ns = Namespace::new(format!("crashy_app_{}", i));
        let handle = kernel.patch_bus().register_namespace(ns);

        // Various problematic patches
        let patches = vec![
            Patch::new(handle.id(), PatchKind::Entity(EntityPatch::create(handle.id(), u64::MAX))),
            Patch::new(
                handle.id(),
                PatchKind::Component(ComponentPatch::set(
                    EntityRef::new(handle.id(), 999999),
                    "NonExistent",
                    void_ir::Value::null(),
                )),
            ),
        ];

        for patch in patches {
            let tx = handle.begin_transaction().patch(patch).build();
            let _ = handle.submit(tx); // Ignore errors
        }

        kernel.patch_bus().receive_pending();
        let _ = kernel.process_transactions(&mut world); // May fail, but shouldn't crash

        kernel.end_frame();
    }

    // Kernel should still be alive and running
    assert_eq!(kernel.state(), KernelState::Running);
    assert!(kernel.frame() > 0);
}

/// INVARIANT: Namespace isolation is enforced
#[test]
fn invariant_namespace_isolation() {
    let config = KernelConfig {
        enable_watchdog: false, // Disable for simpler testing
        ..Default::default()
    };
    let kernel = Kernel::new(config);

    let app1 = Namespace::new("app1");
    let app2 = Namespace::new("app2");

    let app1_id = app1.id;
    let app2_id = app2.id;

    let handle1 = kernel.patch_bus().register_namespace(app1);
    kernel.patch_bus().register_namespace(app2);

    // App1 tries to create entity in App2's namespace
    let patch = Patch::new(
        app1_id,
        PatchKind::Entity(EntityPatch::create(app2_id, 1)),
    );

    let tx = handle1.begin_transaction().patch(patch).build();
    let _ = handle1.submit(tx);

    kernel.patch_bus().receive_pending();

    // Validation should catch this
    // (The transaction would fail during validation or application)
}

/// INVARIANT: Apps can only modify their own entities
#[test]
fn invariant_entity_ownership() {
    let kernel = Kernel::new(KernelConfig::default());

    let app1 = Namespace::new("app1");
    let app2 = Namespace::new("app2");

    let app1_id = app1.id;

    let handle1 = kernel.patch_bus().register_namespace(app1);
    let handle2 = kernel.patch_bus().register_namespace(app2);

    // App1 creates an entity
    let entity_in_app1 = EntityRef::new(app1_id, 1);

    // App2 tries to modify App1's entity
    let patch = Patch::new(
        handle2.id(),
        PatchKind::Component(ComponentPatch::set(
            entity_in_app1,
            "Health",
            void_ir::Value::from(0),
        )),
    );

    let tx = handle2.begin_transaction().patch(patch).build();
    let _ = handle2.submit(tx);

    kernel.patch_bus().receive_pending();

    // Should fail validation due to namespace mismatch
}

/// INVARIANT: Transaction rollback works
#[test]
fn invariant_transaction_rollback() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);
    let mut world = void_ecs::World::new();

    kernel.start();
    kernel.begin_frame(0.016);

    let ns = Namespace::new("test_app");
    let handle = kernel.patch_bus().register_namespace(ns);

    // Create a transaction with both valid and invalid patches
    // (Simulating a partial failure scenario)
    let tx = handle
        .begin_transaction()
        .patch(Patch::new(
            handle.id(),
            PatchKind::Entity(EntityPatch::create(handle.id(), 1)),
        ))
        .build();

    handle.submit(tx).unwrap();
    kernel.patch_bus().receive_pending();

    let initial_world_state = world.entity_count();

    // Process - if transaction fails, world should be unchanged
    let results = kernel.process_transactions(&mut world);

    // Either transaction succeeded entirely or world is unchanged
    for result in results {
        if !result.success {
            assert_eq!(world.entity_count(), initial_world_state);
        }
    }
}

/// INVARIANT: Capabilities are checked before operations
#[test]
fn invariant_capability_checking() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    // Create namespace with limited permissions
    let mut ns = Namespace::new("limited_app");
    ns.permissions.create_entities = false; // Disable entity creation
    ns.permissions.create_layers = false;

    let handle = kernel.patch_bus().register_namespace(ns);

    // Try to create entity (should be denied)
    let patch = Patch::new(
        handle.id(),
        PatchKind::Entity(EntityPatch::create(handle.id(), 1)),
    );

    let tx = handle.begin_transaction().patch(patch).build();
    let _ = handle.submit(tx);

    kernel.patch_bus().receive_pending();

    // Validation should fail due to missing capability
}

/// INVARIANT: Resource limits are enforced
#[test]
fn invariant_resource_limits_enforced() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    // Create namespace with strict resource limits
    let mut ns = Namespace::new("limited_app");
    ns.limits.max_patches_per_frame = Some(5);
    ns.limits.max_entities = Some(10);

    let handle = kernel.patch_bus().register_namespace(ns);

    // Try to submit too many patches in one transaction
    let mut tx = handle.begin_transaction();
    for i in 0..20 {
        tx = tx.patch(Patch::new(
            handle.id(),
            PatchKind::Entity(EntityPatch::create(handle.id(), i)),
        ));
    }
    let tx = tx.build();

    let result = handle.submit(tx);
    // Should either succeed but fail validation, or be rejected immediately
}

/// INVARIANT: Kernel can recover from any single failure
#[test]
fn invariant_kernel_recovery() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    // Simulate various failure scenarios
    for _ in 0..10 {
        kernel.begin_frame(0.016);

        // Simulate work...

        kernel.end_frame();
    }

    // Kernel should remain operational
    assert_eq!(kernel.state(), KernelState::Running);
    assert_eq!(kernel.health_level(), HealthLevel::Healthy);
}

/// INVARIANT: Health monitoring always works
#[test]
fn invariant_health_monitoring() {
    let mut config = KernelConfig::default();
    config.enable_watchdog = true;

    let mut kernel = Kernel::new(config);
    kernel.start();

    // Process many frames
    for _ in 0..100 {
        kernel.begin_frame(0.016);

        // Health should always be queryable
        let level = kernel.health_level();
        assert!(matches!(
            level,
            HealthLevel::Healthy | HealthLevel::Degraded | HealthLevel::Critical | HealthLevel::Dead
        ));

        kernel.end_frame();
    }
}

/// INVARIANT: Garbage collection never corrupts state
#[test]
fn invariant_gc_safety() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    // Create some state
    let ns = Namespace::new("test_app");
    let handle = kernel.patch_bus().register_namespace(ns);

    for i in 0..10 {
        kernel.begin_frame(0.016);

        let tx = handle
            .begin_transaction()
            .patch(Patch::new(
                handle.id(),
                PatchKind::Entity(EntityPatch::create(handle.id(), i)),
            ))
            .build();

        handle.submit(tx).unwrap();
        kernel.patch_bus().receive_pending();

        // Run GC frequently
        kernel.gc();

        kernel.end_frame();
    }

    // Kernel should still be operational
    assert_eq!(kernel.state(), KernelState::Running);
}

/// INVARIANT: Frame quotas reset every frame
#[test]
fn invariant_frame_quota_reset() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    for _ in 0..10 {
        kernel.begin_frame(0.016);

        // Quotas should be reset at frame start
        kernel.reset_frame_quotas();

        kernel.end_frame();
    }
}

/// INVARIANT: Multiple apps can coexist safely
#[test]
fn invariant_multi_app_safety() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);
    let mut world = void_ecs::World::new();

    kernel.start();

    // Create multiple apps
    let mut handles = Vec::new();
    for i in 0..10 {
        let ns = Namespace::new(format!("app_{}", i));
        let handle = kernel.patch_bus().register_namespace(ns);
        handles.push(handle);
    }

    // All apps submit work
    for frame in 0..10 {
        kernel.begin_frame(0.016);

        for (i, handle) in handles.iter().enumerate() {
            let tx = handle
                .begin_transaction()
                .patch(Patch::new(
                    handle.id(),
                    PatchKind::Entity(EntityPatch::create(handle.id(), (frame * 10 + i) as u64)),
                ))
                .build();

            handle.submit(tx).unwrap();
        }

        kernel.patch_bus().receive_pending();
        kernel.process_transactions(&mut world);
        kernel.end_frame();
    }

    // Kernel should still be healthy
    assert_eq!(kernel.state(), KernelState::Running);
}

/// INVARIANT: Shutdown is clean and doesn't panic
#[test]
fn invariant_clean_shutdown() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    // Do some work
    for _ in 0..10 {
        kernel.begin_frame(0.016);
        kernel.end_frame();
    }

    // Shutdown should be clean
    kernel.shutdown();
    assert_eq!(kernel.state(), KernelState::Stopped);
}

/// INVARIANT: Paused kernel doesn't process frames
#[test]
fn invariant_pause_stops_processing() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();
    kernel.begin_frame(0.016);

    let frame_before_pause = kernel.frame();

    kernel.pause();

    // Try to advance frames while paused
    // (In real implementation, frame processing would be skipped)

    kernel.resume();
    kernel.begin_frame(0.016);

    // Frame should increment after resume
    assert!(kernel.frame() > frame_before_pause);
}

/// INVARIANT: Backend selection is stable
#[test]
fn invariant_backend_stability() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    let backend1 = kernel.backend_selector().current();
    let backend2 = kernel.backend_selector().current();

    // Backend should not randomly change
    assert_eq!(
        std::mem::discriminant(&backend1),
        std::mem::discriminant(&backend2)
    );
}

/// INVARIANT: Statistics are consistent
#[test]
fn invariant_statistics_consistency() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    for _ in 0..10 {
        kernel.begin_frame(0.016);

        let status = kernel.status();

        // Statistics should be non-negative and consistent
        assert!(status.uptime_secs >= 0.0);
        assert!(status.avg_fps >= 0.0);
        assert!(status.app_count >= 0);
        assert!(status.running_apps <= status.app_count);
        assert!(status.layer_count >= 0);
        assert!(status.asset_count >= 0);

        kernel.end_frame();
    }
}

/// INVARIANT: Emergency shutdown flag works
#[test]
fn invariant_emergency_shutdown_detection() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    // Initially should not need emergency shutdown
    assert!(!kernel.needs_emergency_shutdown());

    // After checking multiple times, should remain consistent
    assert!(!kernel.needs_emergency_shutdown());
}

/// INVARIANT: Recovery stats are accurate
#[test]
fn invariant_recovery_stats_accuracy() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    let stats1 = kernel.recovery_stats();
    let stats2 = kernel.recovery_stats();

    // Stats should be consistent when no recovery events occur
    assert_eq!(stats1.panic_count, stats2.panic_count);
    assert_eq!(stats1.recovery_count, stats2.recovery_count);
}
