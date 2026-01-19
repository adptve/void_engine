//! Integration tests for void_kernel
//!
//! Tests kernel lifecycle, app isolation, fault tolerance, and invariants

use void_kernel::*;
use void_ir::{Namespace, NamespaceId, Patch, PatchKind, EntityPatch, TransactionBuilder};

#[test]
fn test_kernel_creation_and_lifecycle() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    assert_eq!(kernel.state(), KernelState::Initializing);
    assert_eq!(kernel.frame(), 0);

    kernel.start();
    assert_eq!(kernel.state(), KernelState::Running);

    kernel.pause();
    assert_eq!(kernel.state(), KernelState::Paused);

    kernel.resume();
    assert_eq!(kernel.state(), KernelState::Running);

    kernel.shutdown();
    assert_eq!(kernel.state(), KernelState::Stopped);
}

#[test]
fn test_kernel_frame_progression() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();
    assert_eq!(kernel.frame(), 0);

    let ctx = kernel.begin_frame(0.016); // ~60 FPS
    assert_eq!(ctx.frame, 1);
    assert_eq!(kernel.frame(), 1);

    kernel.end_frame();

    let ctx = kernel.begin_frame(0.016);
    assert_eq!(ctx.frame, 2);
    assert_eq!(kernel.frame(), 2);
}

#[test]
fn test_kernel_max_delta_clamping() {
    let mut config = KernelConfig::default();
    config.max_delta_time = 0.1;

    let mut kernel = Kernel::new(config);
    kernel.start();

    // Try to pass huge delta (lag spike)
    let ctx = kernel.begin_frame(1.0); // 1 second lag

    // Should be clamped to max_delta_time
    assert!((ctx.delta_time - 0.1).abs() < 0.001);
}

#[test]
fn test_patch_bus_integration() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    let patch_bus = kernel.patch_bus();

    // Register a namespace
    let ns = Namespace::new("test_app");
    let ns_id = ns.id;
    let handle = patch_bus.register_namespace(ns);

    assert_eq!(handle.id(), ns_id);
}

#[test]
fn test_layer_manager_integration() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    let layer_manager = kernel.layer_manager();
    assert_eq!(layer_manager.len(), 0);

    // Layer operations would be tested through patch application
}

#[test]
fn test_app_manager_integration() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    let app_manager = kernel.app_manager();
    assert_eq!(app_manager.len(), 0);
}

#[test]
fn test_asset_registry_integration() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    let asset_registry = kernel.asset_registry();
    assert_eq!(asset_registry.len(), 0);
}

#[test]
fn test_kernel_status_report() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();
    kernel.begin_frame(0.016);

    let status = kernel.status();
    assert_eq!(status.state, KernelState::Running);
    assert!(status.frame > 0);
    assert_eq!(status.app_count, 0);
    assert_eq!(status.layer_count, 0);
    assert_eq!(status.asset_count, 0);

    // Status should display nicely
    let status_str = status.to_string();
    assert!(status_str.contains("Running"));
}

#[test]
fn test_kernel_watchdog_integration() {
    let mut config = KernelConfig::default();
    config.enable_watchdog = true;

    let mut kernel = Kernel::new(config);
    kernel.start();

    // Send a heartbeat
    kernel.begin_frame(0.016);

    let health = kernel.health_level();
    assert_eq!(health, HealthLevel::Healthy);

    let metrics = kernel.health_metrics();
    assert!(metrics.is_some());
}

#[test]
fn test_kernel_watchdog_disabled() {
    let mut config = KernelConfig::default();
    config.enable_watchdog = false;

    let kernel = Kernel::new(config);

    let health = kernel.health_level();
    assert_eq!(health, HealthLevel::Healthy); // Default when disabled

    let metrics = kernel.health_metrics();
    assert!(metrics.is_none());
}

#[test]
fn test_supervisor_tree_integration() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    let tree = kernel.supervisor_tree();
    let tree_guard = tree.read();

    // Should have root + apps supervisor
    assert!(tree_guard.len() >= 1);
}

#[test]
fn test_kernel_garbage_collection() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    // Run GC (should not crash)
    kernel.gc();

    // GC multiple times
    kernel.gc();
    kernel.gc();
}

#[test]
fn test_capability_checker_integration() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    let checker = kernel.capability_checker();
    // Checker should be initialized

    // Reset frame quotas
    kernel.reset_frame_quotas();
}

#[test]
fn test_emergency_shutdown_check() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    // Initially should not need shutdown
    assert!(!kernel.needs_emergency_shutdown());
}

#[test]
fn test_recovery_stats() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    let stats = kernel.recovery_stats();
    assert_eq!(stats.panic_count, 0);
    assert_eq!(stats.recovery_count, 0);
}

#[test]
fn test_render_graph_building() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    let render_graph = kernel.build_render_graph();
    assert_eq!(render_graph.frame, 0);
    assert_eq!(render_graph.layers.len(), 0); // No layers yet
}

#[test]
fn test_transaction_processing() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);
    let mut world = void_ecs::World::new();

    kernel.start();
    kernel.begin_frame(0.016);

    // Register namespace and submit transaction
    let ns = Namespace::new("test_app");
    let ns_id = ns.id;
    let handle = kernel.patch_bus().register_namespace(ns);

    let tx = handle
        .begin_transaction()
        .patch(Patch::new(
            ns_id,
            PatchKind::Entity(EntityPatch::create(ns_id, 1)),
        ))
        .build();

    handle.submit(tx).unwrap();
    kernel.patch_bus().receive_pending();

    // Process transactions
    let results = kernel.process_transactions(&mut world);
    assert!(results.len() > 0);
}

#[test]
fn test_kernel_config_defaults() {
    let config = KernelConfig::default();

    assert_eq!(config.target_fps, 60);
    assert!(config.fixed_timestep > 0.0);
    assert!(config.hot_reload);
    assert!(config.enable_watchdog);
    assert!(config.rollback_frames > 0);
    assert!(config.max_apps > 0);
    assert!(config.max_layers > 0);
}

#[test]
fn test_kernel_state_transitions() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    // Initializing -> Running
    kernel.start();
    assert_eq!(kernel.state(), KernelState::Running);

    // Running -> Paused
    kernel.pause();
    assert_eq!(kernel.state(), KernelState::Paused);

    // Paused -> Running
    kernel.resume();
    assert_eq!(kernel.state(), KernelState::Running);

    // Running -> ShuttingDown -> Stopped
    kernel.shutdown();
    assert_eq!(kernel.state(), KernelState::Stopped);
}

#[test]
fn test_pause_does_not_affect_stopped_kernel() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    // Try to pause before starting
    kernel.pause();
    assert_eq!(kernel.state(), KernelState::Initializing); // No change
}

#[test]
fn test_resume_only_works_when_paused() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();
    assert_eq!(kernel.state(), KernelState::Running);

    // Resume when not paused
    kernel.resume();
    assert_eq!(kernel.state(), KernelState::Running); // No change
}

#[test]
fn test_multiple_frames() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    for i in 1..=10 {
        kernel.begin_frame(0.016);
        assert_eq!(kernel.frame(), i);
        kernel.end_frame();
    }
}

#[test]
fn test_backend_selector() {
    let config = KernelConfig::default();
    let kernel = Kernel::new(config);

    let selector = kernel.backend_selector();
    let backend = selector.current();

    // Should have a default backend selected
    assert!(matches!(backend, Backend::Wgpu | Backend::Vulkan | Backend::Mock));
}

#[test]
fn test_capability_creation() {
    let cap_id = CapabilityId::new("test_capability");
    let cap = Capability::new(cap_id, CapabilityKind::CreateEntity);

    assert_eq!(cap.id().name(), "test_capability");
}

#[test]
fn test_namespace_quotas() {
    let quotas = NamespaceQuotas::default();

    assert!(quotas.max_entities > 0);
    assert!(quotas.max_layers > 0);
}

#[test]
fn test_health_level_ordering() {
    use std::cmp::Ordering;

    assert_eq!(HealthLevel::Healthy.cmp(&HealthLevel::Degraded), Ordering::Greater);
    assert_eq!(HealthLevel::Degraded.cmp(&HealthLevel::Critical), Ordering::Greater);
    assert_eq!(HealthLevel::Critical.cmp(&HealthLevel::Dead), Ordering::Greater);
}

#[test]
fn test_watchdog_config_defaults() {
    let config = WatchdogConfig::default();

    assert!(config.heartbeat_timeout_ms > 0);
    assert!(config.check_interval_ms > 0);
}

#[test]
fn test_supervisor_restart_strategies() {
    let one_for_one = RestartStrategy::OneForOne;
    let one_for_all = RestartStrategy::OneForAll;
    let rest_for_one = RestartStrategy::RestForOne;

    // Strategies should be different
    assert_ne!(
        std::mem::discriminant(&one_for_one),
        std::mem::discriminant(&one_for_all)
    );
}

#[test]
fn test_layer_manager_max_capacity() {
    let layer_manager = LayerManager::new(10);
    assert_eq!(layer_manager.capacity(), 10);
}

#[test]
fn test_app_manager_max_capacity() {
    let patch_bus = std::sync::Arc::new(void_ir::PatchBus::default());
    let app_manager = AppManager::new(20, patch_bus);
    assert_eq!(app_manager.capacity(), 20);
}

#[test]
fn test_kernel_never_panics_on_bad_transaction() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);
    let mut world = void_ecs::World::new();

    kernel.start();
    kernel.begin_frame(0.016);

    // Create an invalid transaction (entity in wrong namespace, etc.)
    let ns1 = Namespace::new("app1");
    let ns2 = Namespace::new("app2");
    let ns2_id = ns2.id;

    let handle1 = kernel.patch_bus().register_namespace(ns1);
    kernel.patch_bus().register_namespace(ns2);

    // Try to create entity in different namespace
    let bad_tx = handle1
        .begin_transaction()
        .patch(Patch::new(
            handle1.id(),
            PatchKind::Entity(EntityPatch::create(ns2_id, 1)), // Wrong namespace!
        ))
        .build();

    handle1.submit(bad_tx).unwrap();
    kernel.patch_bus().receive_pending();

    // Should not panic, just fail gracefully
    let results = kernel.process_transactions(&mut world);
    // Transaction should fail but kernel continues
}

#[test]
fn test_kernel_survives_multiple_bad_frames() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    // Process many frames with no work
    for _ in 0..100 {
        kernel.begin_frame(0.016);
        kernel.end_frame();
    }

    // Kernel should still be running
    assert_eq!(kernel.state(), KernelState::Running);
    assert_eq!(kernel.frame(), 100);
}

#[test]
fn test_frame_timing_accumulation() {
    let config = KernelConfig::default();
    let mut kernel = Kernel::new(config);

    kernel.start();

    for _ in 0..60 {
        kernel.begin_frame(0.016); // One second of frames
        kernel.end_frame();
    }

    let status = kernel.status();
    assert!(status.uptime_secs >= 0.9); // ~1 second
}
