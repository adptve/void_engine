//! # Void Kernel
//!
//! The kernel is the heart of Void Engine. It orchestrates:
//! - Frame assembly and tick lifecycle
//! - IR transaction application
//! - Layer composition
//! - Backend selection
//! - Hot-swap and rollback
//! - Supervision and fault tolerance
//!
//! ## Architecture
//!
//! ```text
//! Apps emit IR patches ──► Patch Bus ──► Kernel applies ──► Layers ──► Composite ──► Present
//!                                                │
//!                                                ▼
//!                                    Supervisor Tree (monitors)
//!                                                │
//!                                                ▼
//!                                    Watchdog (health checks)
//! ```
//!
//! The kernel never executes app code directly. It only processes declarative
//! patches from the IR system, maintaining full isolation.
//!
//! ## Key Invariants
//!
//! 1. **Kernel never dies** - All failures are contained
//! 2. **Apps are untrusted** - Capability checks everywhere
//! 3. **Declarative state** - Changes via patches only
//! 4. **Layer isolation** - Failed shader = black layer
//! 5. **Hot-swap everything** - No restart ever required
//! 6. **Mandatory rollback** - Last-known-good always available

pub mod frame;
pub mod layer;
pub mod apply;
pub mod backend;
pub mod registry;
pub mod app;
pub mod supervisor;
pub mod watchdog;
pub mod recovery;
pub mod capability;
pub mod namespace;
pub mod sandbox;
pub mod package;
pub mod manifest;

// Daemon IPC and persistence modules
pub mod ipc;
pub mod client;
pub mod persist;
pub mod daemon;

pub use frame::{FrameContext, FrameState, FrameTiming};
pub use layer::{Layer, LayerId, LayerManager, LayerConfig};
pub use apply::{PatchApplicator, ApplyResult};
pub use backend::{Backend, BackendSelector, BackendCapabilities};
pub use registry::{AssetRegistry, AssetVersion, AssetState};
pub use app::{App, AppId, AppState, AppManager};
pub use supervisor::{
    Supervisor, SupervisorId, SupervisorTree, SupervisorAction,
    RestartStrategy, RestartIntensity, ChildType, ChildStatus,
    BackoffConfig, ChildStats, ChildId, SupervisedChild, ServiceId,
};
pub use watchdog::{
    Watchdog, WatchdogConfig, WatchdogState, HealthLevel, HealthMetrics, WatchdogAlert,
    SystemdNotifier, DegradedModeState, DegradedAction,
};
pub use recovery::{
    RecoveryManager, RecoveryContext, RecoveryResult, PanicInfo,
    RecoveryStats, EmergencyRecoveryResult, PanicGuard,
    StateSnapshot, SnapshotId, SnapshotManager, AppSnapshot, LayerSnapshot,
    HotSwapStage, HotSwapOperation, HotSwapId, HotSwapKind, HotSwapManager, HotSwapStats,
    catch_panic, catch_panic_mut, execute_app_callback,
};
pub use capability::{
    Capability, CapabilityId, CapabilityKind, CapabilityCheck,
    CapabilityChecker, NamespaceQuotas, CapabilityError,
    required_capability_for_patch, enforce_patch_capability,
};
pub use namespace::{
    NamespaceManager, NamespaceInfo, NamespacedEntity, NamespaceAccess,
    NamespaceError, EntityExport, ExportAccess,
};
pub use sandbox::{
    AppSandbox, SandboxId, SandboxManager, SandboxError,
    ResourceBudget, ResourceUsage, ResourceCheck, SandboxResources,
};
pub use package::{
    Package, PackageManifest, PackageMetadata, AppConfig, AppType,
    PackageBuilder, PackageLoader, PackageError, PackageEntry,
    PACKAGE_EXTENSION, PACKAGE_VERSION, MAX_PACKAGE_SIZE,
};
pub use manifest::{
    parse_manifest, load_manifest, load_app_manifest,
    parse_extended_manifest, load_extended_manifest,
    ExtendedManifest, ManifestError, ManifestResult,
};

// IPC and daemon exports
pub use ipc::{
    ClientMessage, KernelMessage, MessageId, IpcError, ErrorCode,
    CapabilityRequest, CapabilityRequestKind, CapabilityInfo,
    AppRegistration, KernelStatusReport, SerializableTransaction,
    PROTOCOL_VERSION, MAX_MESSAGE_SIZE,
    write_message, read_client_message, write_client_message, read_kernel_message,
};
pub use client::{
    Client, ClientId, ClientState, ClientInfo, ClientStats,
    ClientManager, ClientManagerStats, ClientError,
};
pub use persist::{
    PersistConfig, PersistedState, PersistedApp, PersistedLayer, PersistedCapability,
    StatePersister, PersistStats, PersistError, StateVersion,
};
pub use daemon::{
    DaemonConfig, DaemonState, KernelDaemon, DaemonStats, DaemonError,
    run_daemon,
};

use void_ir::{PatchBus, Namespace, NamespaceId};
use parking_lot::RwLock;
use std::sync::Arc;

/// Configuration for the kernel
#[derive(Debug, Clone)]
pub struct KernelConfig {
    /// Target frames per second
    pub target_fps: u32,
    /// Fixed timestep for physics/logic (seconds)
    pub fixed_timestep: f32,
    /// Maximum frame delta (caps lag spikes)
    pub max_delta_time: f32,
    /// Enable hot-reload
    pub hot_reload: bool,
    /// Number of frames to keep for rollback
    pub rollback_frames: u32,
    /// Maximum apps
    pub max_apps: u32,
    /// Maximum layers
    pub max_layers: u32,
    /// Enable watchdog monitoring
    pub enable_watchdog: bool,
    /// Watchdog configuration
    pub watchdog_config: WatchdogConfig,
}

impl Default for KernelConfig {
    fn default() -> Self {
        Self {
            target_fps: 60,
            fixed_timestep: 1.0 / 60.0,
            max_delta_time: 0.25,
            hot_reload: true,
            rollback_frames: 3,
            max_apps: 64,
            max_layers: 128,
            enable_watchdog: true,
            watchdog_config: WatchdogConfig::default(),
        }
    }
}

/// The main kernel struct
pub struct Kernel {
    config: KernelConfig,
    /// Current frame number
    frame: u64,
    /// Frame timing
    timing: FrameTiming,
    /// The patch bus for IR communication
    patch_bus: Arc<PatchBus>,
    /// Layer manager
    layer_manager: LayerManager,
    /// Patch applicator
    applicator: PatchApplicator,
    /// Backend selector
    backend_selector: BackendSelector,
    /// Asset registry
    asset_registry: AssetRegistry,
    /// App manager
    app_manager: AppManager,
    /// Kernel namespace (has full access)
    kernel_namespace: NamespaceId,
    /// Current state
    state: KernelState,
    /// Supervision tree for fault tolerance
    supervisor_tree: Arc<RwLock<SupervisorTree>>,
    /// Watchdog for health monitoring
    watchdog: Option<Watchdog>,
    /// Watchdog state handle
    watchdog_state: Option<Arc<WatchdogState>>,
    /// Recovery manager
    recovery_manager: RecoveryManager,
    /// Capability checker for access control
    capability_checker: CapabilityChecker,
}

/// Kernel lifecycle state
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KernelState {
    /// Kernel is initializing
    Initializing,
    /// Kernel is running
    Running,
    /// Kernel is paused (apps still loaded)
    Paused,
    /// Kernel is shutting down
    ShuttingDown,
    /// Kernel has stopped
    Stopped,
}

impl Kernel {
    /// Create a new kernel
    pub fn new(config: KernelConfig) -> Self {
        let patch_bus = Arc::new(PatchBus::default());

        // Register the kernel namespace
        let kernel_ns = Namespace::kernel();
        let kernel_namespace = kernel_ns.id;
        patch_bus.register_namespace(kernel_ns);

        // Create supervision tree
        let supervisor_tree = Arc::new(RwLock::new(SupervisorTree::new()));

        // Create app supervisor under root
        {
            let mut tree = supervisor_tree.write();
            let root_id = tree.root_id();
            tree.add_supervisor("apps", RestartStrategy::OneForOne, root_id);
        }

        // Create watchdog if enabled
        let (watchdog, watchdog_state) = if config.enable_watchdog {
            let mut wd = Watchdog::new(config.watchdog_config.clone());
            let state = wd.state();
            (Some(wd), Some(state))
        } else {
            (None, None)
        };

        // Create recovery manager
        let recovery_manager = RecoveryManager::new(Arc::clone(&supervisor_tree));

        // Create capability checker
        let capability_checker = CapabilityChecker::new(kernel_namespace);

        Self {
            config: config.clone(),
            frame: 0,
            timing: FrameTiming::new(config.target_fps),
            patch_bus: patch_bus.clone(),
            layer_manager: LayerManager::new(config.max_layers as usize),
            applicator: PatchApplicator::new(),
            backend_selector: BackendSelector::new(),
            asset_registry: AssetRegistry::new(),
            app_manager: AppManager::new(config.max_apps as usize, patch_bus),
            kernel_namespace,
            state: KernelState::Initializing,
            supervisor_tree,
            watchdog,
            watchdog_state,
            recovery_manager,
            capability_checker,
        }
    }

    /// Get the capability checker
    pub fn capability_checker(&self) -> &CapabilityChecker {
        &self.capability_checker
    }

    /// Get mutable capability checker
    pub fn capability_checker_mut(&mut self) -> &mut CapabilityChecker {
        &mut self.capability_checker
    }

    /// Get the patch bus
    pub fn patch_bus(&self) -> &Arc<PatchBus> {
        &self.patch_bus
    }

    /// Get the layer manager
    pub fn layer_manager(&self) -> &LayerManager {
        &self.layer_manager
    }

    /// Get mutable layer manager
    pub fn layer_manager_mut(&mut self) -> &mut LayerManager {
        &mut self.layer_manager
    }

    /// Get the app manager
    pub fn app_manager(&self) -> &AppManager {
        &self.app_manager
    }

    /// Get mutable app manager
    pub fn app_manager_mut(&mut self) -> &mut AppManager {
        &mut self.app_manager
    }

    /// Get the asset registry
    pub fn asset_registry(&self) -> &AssetRegistry {
        &self.asset_registry
    }

    /// Get mutable asset registry
    pub fn asset_registry_mut(&mut self) -> &mut AssetRegistry {
        &mut self.asset_registry
    }

    /// Get the backend selector
    pub fn backend_selector(&self) -> &BackendSelector {
        &self.backend_selector
    }

    /// Get current frame number
    pub fn frame(&self) -> u64 {
        self.frame
    }

    /// Get current state
    pub fn state(&self) -> KernelState {
        self.state
    }

    /// Start the kernel
    pub fn start(&mut self) {
        log::info!("Kernel starting...");
        self.state = KernelState::Running;
        self.timing.reset();

        // Start watchdog
        if let Some(ref mut watchdog) = self.watchdog {
            watchdog.start();
        }
    }

    /// Pause the kernel
    pub fn pause(&mut self) {
        if self.state == KernelState::Running {
            log::info!("Kernel paused");
            self.state = KernelState::Paused;
        }
    }

    /// Resume the kernel
    pub fn resume(&mut self) {
        if self.state == KernelState::Paused {
            log::info!("Kernel resumed");
            self.state = KernelState::Running;
            self.timing.reset();
        }
    }

    /// Begin a new frame
    pub fn begin_frame(&mut self, delta_time: f32) -> FrameContext {
        self.frame += 1;
        let delta = delta_time.min(self.config.max_delta_time);
        self.timing.update(delta);

        // Begin frame on patch bus
        self.patch_bus.begin_frame(self.frame);

        // Send heartbeat to watchdog
        if let Some(ref state) = self.watchdog_state {
            state.heartbeat(self.frame, delta * 1000.0);
        }

        FrameContext {
            frame: self.frame,
            delta_time: delta,
            total_time: self.timing.total_time,
            state: FrameState::Processing,
        }
    }

    /// Get the supervisor tree
    pub fn supervisor_tree(&self) -> &Arc<RwLock<SupervisorTree>> {
        &self.supervisor_tree
    }

    /// Get health metrics from watchdog
    pub fn health_metrics(&self) -> Option<HealthMetrics> {
        self.watchdog_state.as_ref().map(|s| s.metrics())
    }

    /// Get current health level
    pub fn health_level(&self) -> HealthLevel {
        self.watchdog_state
            .as_ref()
            .map(|s| s.health())
            .unwrap_or(HealthLevel::Healthy)
    }

    /// Get recovery statistics
    pub fn recovery_stats(&self) -> recovery::RecoveryStats {
        self.recovery_manager.stats()
    }

    /// Check if emergency shutdown is needed
    pub fn needs_emergency_shutdown(&self) -> bool {
        self.recovery_manager.needs_shutdown()
    }

    /// Process pending transactions
    pub fn process_transactions(&mut self, world: &mut void_ecs::World) -> Vec<ApplyResult> {
        let transactions = self.patch_bus.drain_ready(self.frame);
        let mut results = Vec::with_capacity(transactions.len());

        for tx in transactions {
            let result = self.applicator.apply(
                tx,
                world,
                &mut self.layer_manager,
                &mut self.asset_registry,
            );

            // Report result to patch bus
            self.patch_bus.commit(result.to_transaction_result());
            results.push(result);
        }

        results
    }

    /// Build the render graph for this frame
    pub fn build_render_graph(&mut self) -> RenderGraph {
        let layers = self.layer_manager.collect_visible();
        let backend = self.backend_selector.current();

        RenderGraph {
            frame: self.frame,
            layers,
            backend,
        }
    }

    /// End the frame
    pub fn end_frame(&mut self) {
        // Garbage collect old committed transactions
        self.patch_bus.gc_committed(1000);
    }

    /// Shutdown the kernel
    pub fn shutdown(&mut self) {
        log::info!("Kernel shutting down...");
        self.state = KernelState::ShuttingDown;

        // Stop watchdog first
        if let Some(ref mut watchdog) = self.watchdog {
            watchdog.stop();
        }

        // Unload all apps
        self.app_manager.unload_all();

        self.state = KernelState::Stopped;
        log::info!("Kernel stopped");
    }

    /// Perform garbage collection
    pub fn gc(&mut self) {
        // GC supervision tree restart histories
        self.supervisor_tree.write().gc();

        // GC committed transactions
        self.patch_bus.gc_committed(1000);

        // GC unused assets
        let removed = self.asset_registry.gc(60.0);
        if !removed.is_empty() {
            log::debug!("GC removed {} unused assets", removed.len());
        }

        // GC stopped apps
        let removed = self.app_manager.gc();
        if !removed.is_empty() {
            log::debug!("GC removed {} stopped apps", removed.len());
        }

        // GC expired capabilities
        self.capability_checker.gc_expired();
    }

    /// Reset frame-based quotas (call at start of each frame)
    pub fn reset_frame_quotas(&mut self) {
        self.capability_checker.reset_frame_quotas();
    }

    /// Get kernel status report
    pub fn status(&self) -> KernelStatus {
        KernelStatus {
            state: self.state,
            frame: self.frame,
            uptime_secs: self.timing.total_time,
            avg_fps: self.timing.avg_fps,
            app_count: self.app_manager.len(),
            running_apps: self.app_manager.running().len(),
            layer_count: self.layer_manager.len(),
            asset_count: self.asset_registry.len(),
            health_level: self.health_level(),
            backend: self.backend_selector.current(),
        }
    }
}

/// Kernel status report
#[derive(Debug, Clone)]
pub struct KernelStatus {
    pub state: KernelState,
    pub frame: u64,
    pub uptime_secs: f32,
    pub avg_fps: f32,
    pub app_count: usize,
    pub running_apps: usize,
    pub layer_count: usize,
    pub asset_count: usize,
    pub health_level: HealthLevel,
    pub backend: Backend,
}

impl std::fmt::Display for KernelStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Kernel Status:\n\
             - State: {:?}\n\
             - Frame: {}\n\
             - Uptime: {:.1}s\n\
             - FPS: {:.1}\n\
             - Apps: {} ({} running)\n\
             - Layers: {}\n\
             - Assets: {}\n\
             - Health: {}\n\
             - Backend: {}",
            self.state,
            self.frame,
            self.uptime_secs,
            self.avg_fps,
            self.app_count,
            self.running_apps,
            self.layer_count,
            self.asset_count,
            self.health_level,
            self.backend
        )
    }
}

/// A simplified render graph for this frame
#[derive(Debug)]
pub struct RenderGraph {
    /// Frame number
    pub frame: u64,
    /// Layers to render (in order)
    pub layers: Vec<LayerId>,
    /// Backend to use
    pub backend: Backend,
}
