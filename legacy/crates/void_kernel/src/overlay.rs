//! OverlayApp - Isolated apps with their own World
//!
//! OverlayApps are completely isolated from the MetaverseCore.
//! Each OverlayApp has:
//! - Its OWN World (ECS)
//! - Its OWN Scene
//! - Its OWN layers (rendered ON TOP of metaverse)
//!
//! ## Isolation Model
//!
//! ```text
//! OverlayApp A ──► Own World A ──► Overlay Layer A
//! OverlayApp B ──► Own World B ──► Overlay Layer B
//!
//! Composition:
//!   MetaverseCore (bottom)
//!   + OverlayApp A Layer
//!   + OverlayApp B Layer (top)
//! ```
//!
//! OverlayApps are perfect for:
//! - HUD/UI overlays
//! - Debug visualizations
//! - Companion apps (minimaps, inventories)
//! - Picture-in-picture views
//!
//! They CANNOT access the metaverse's World directly.

use void_ecs::World;
use void_ir::{Namespace, NamespaceHandle, NamespaceId, PatchBus};
use void_ir::patch::LayerType;
use parking_lot::RwLock;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Instant;

use crate::layer::{LayerConfig, LayerId, LayerManager};
use crate::registry::AssetRegistry;
use crate::apply::{ApplyResult, PatchApplicator};

/// Unique identifier for an overlay app
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct OverlayAppId(u64);

impl OverlayAppId {
    /// Create a new unique overlay app ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for OverlayAppId {
    fn default() -> Self {
        Self::new()
    }
}

/// State of an overlay app
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OverlayAppState {
    /// App is being loaded
    Loading,
    /// App is initializing
    Initializing,
    /// App is running
    Running,
    /// App is paused
    Paused,
    /// App is being unloaded
    Unloading,
    /// App has stopped
    Stopped,
    /// App has failed
    Failed,
}

/// Manifest describing an overlay app
#[derive(Debug, Clone)]
pub struct OverlayAppManifest {
    /// App name
    pub name: String,
    /// App version
    pub version: String,
    /// App description
    pub description: Option<String>,
    /// Author
    pub author: Option<String>,
    /// Layer configuration
    pub layer: OverlayLayerConfig,
    /// Resource requirements
    pub resources: OverlayResources,
    /// Permissions
    pub permissions: OverlayPermissions,
}

/// Layer configuration for overlay
#[derive(Debug, Clone)]
pub struct OverlayLayerConfig {
    /// Layer name
    pub name: String,
    /// Priority (higher = on top)
    pub priority: i32,
    /// Layer type (usually Overlay)
    pub layer_type: LayerType,
}

impl Default for OverlayLayerConfig {
    fn default() -> Self {
        Self {
            name: "overlay".to_string(),
            priority: 100,
            layer_type: LayerType::Overlay,
        }
    }
}

/// Resource requirements for overlay app
#[derive(Debug, Clone, Default)]
pub struct OverlayResources {
    /// Maximum entities
    pub max_entities: Option<u32>,
    /// Maximum memory
    pub max_memory: Option<u64>,
}

/// Permissions for overlay app
#[derive(Debug, Clone, Default)]
pub struct OverlayPermissions {
    /// Can access network
    pub network: bool,
    /// Can access filesystem
    pub filesystem: bool,
    /// Can read metaverse entities (read-only)
    pub metaverse_read: bool,
}

/// An overlay app instance with its OWN world
pub struct OverlayApp {
    /// Unique ID
    pub id: OverlayAppId,
    /// Manifest
    pub manifest: OverlayAppManifest,
    /// THIS APP'S OWN WORLD - completely isolated
    world: World,
    /// Namespace ID for this app
    pub namespace_id: NamespaceId,
    /// Current state
    pub state: OverlayAppState,
    /// When the app was loaded
    pub loaded_at: Instant,
    /// When the app was last active
    pub last_active: Instant,
    /// Error message if failed
    pub error: Option<String>,
    /// This app's layer
    pub layer: Option<LayerId>,
    /// Patch applicator for this app's world
    applicator: PatchApplicator,
    /// Metrics
    pub metrics: OverlayAppMetrics,
}

impl std::fmt::Debug for OverlayApp {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("OverlayApp")
            .field("id", &self.id)
            .field("name", &self.manifest.name)
            .field("state", &self.state)
            .field("layer", &self.layer)
            .field("metrics", &self.metrics)
            .finish()
    }
}

/// Metrics for an overlay app
#[derive(Debug, Clone, Default)]
pub struct OverlayAppMetrics {
    /// Total frames processed
    pub frames: u64,
    /// Total patches emitted
    pub patches_emitted: u64,
    /// Total patches applied
    pub patches_applied: u64,
    /// Entity count
    pub entity_count: u32,
}

impl OverlayApp {
    /// Create a new overlay app
    pub fn new(manifest: OverlayAppManifest, namespace_id: NamespaceId) -> Self {
        let now = Instant::now();
        Self {
            id: OverlayAppId::new(),
            manifest,
            world: World::new(), // OWN WORLD
            namespace_id,
            state: OverlayAppState::Loading,
            loaded_at: now,
            last_active: now,
            error: None,
            layer: None,
            applicator: PatchApplicator::new(),
            metrics: OverlayAppMetrics::default(),
        }
    }

    /// Get this app's world (immutable)
    pub fn world(&self) -> &World {
        &self.world
    }

    /// Get this app's world (mutable)
    pub fn world_mut(&mut self) -> &mut World {
        &mut self.world
    }

    /// Update state
    pub fn set_state(&mut self, state: OverlayAppState) {
        self.state = state;
        if state == OverlayAppState::Running {
            self.last_active = Instant::now();
        }
    }

    /// Mark as failed
    pub fn mark_failed(&mut self, error: impl Into<String>) {
        self.state = OverlayAppState::Failed;
        self.error = Some(error.into());
    }

    /// Check if app is active
    pub fn is_active(&self) -> bool {
        matches!(self.state, OverlayAppState::Running | OverlayAppState::Paused)
    }

    /// Process transactions for THIS app's world
    pub fn process_transactions(
        &mut self,
        patch_bus: &PatchBus,
        layer_manager: &mut LayerManager,
        asset_registry: &mut AssetRegistry,
    ) -> Vec<ApplyResult> {
        let frame = patch_bus.current_frame();
        let transactions = patch_bus.drain_ready_for_namespace(frame, self.namespace_id);
        let mut results = Vec::with_capacity(transactions.len());

        for tx in transactions {
            let result = self.applicator.apply(
                tx,
                &mut self.world,  // Apply to THIS app's world
                layer_manager,
                asset_registry,
            );
            patch_bus.commit(result.to_transaction_result());

            self.metrics.patches_applied += result.patches_applied as u64;
            results.push(result);
        }

        self.metrics.frames += 1;
        results
    }
}

/// Manages all overlay apps
pub struct OverlayAppManager {
    /// All registered overlay apps
    apps: HashMap<OverlayAppId, OverlayApp>,
    /// Namespace handles for apps
    handles: HashMap<OverlayAppId, NamespaceHandle>,
    /// Maximum number of apps
    max_apps: usize,
    /// Reference to patch bus
    patch_bus: Arc<PatchBus>,
}

impl OverlayAppManager {
    /// Create a new overlay app manager
    pub fn new(max_apps: usize, patch_bus: Arc<PatchBus>) -> Self {
        Self {
            apps: HashMap::new(),
            handles: HashMap::new(),
            max_apps,
            patch_bus,
        }
    }

    /// Load an overlay app from a manifest
    pub fn load(&mut self, manifest: OverlayAppManifest) -> Result<OverlayAppId, OverlayAppError> {
        if self.apps.len() >= self.max_apps {
            return Err(OverlayAppError::TooManyApps);
        }

        // Create namespace for this overlay app (completely separate from metaverse)
        let mut namespace = Namespace::new(&format!("overlay:{}", manifest.name));

        // Apply resource limits
        if let Some(max_entities) = manifest.resources.max_entities {
            namespace.limits.max_entities = Some(max_entities);
        }

        // Overlay apps cannot write to other namespaces by default
        namespace.permissions.cross_namespace_read = manifest.permissions.metaverse_read;

        let namespace_id = namespace.id;
        let handle = self.patch_bus.register_namespace(namespace);

        let app = OverlayApp::new(manifest, namespace_id);
        let id = app.id;

        self.apps.insert(id, app);
        self.handles.insert(id, handle);

        log::info!("Loaded overlay app {:?}", id);
        Ok(id)
    }

    /// Initialize an overlay app
    pub fn initialize(&mut self, id: OverlayAppId, layer_manager: &mut LayerManager) -> Result<(), OverlayAppError> {
        let app = self.apps.get_mut(&id).ok_or(OverlayAppError::NotFound(id))?;

        if app.state != OverlayAppState::Loading {
            return Err(OverlayAppError::InvalidState);
        }

        app.set_state(OverlayAppState::Initializing);

        // Create the overlay layer
        let layer_config = LayerConfig::overlay(app.manifest.layer.priority);
        match layer_manager.create_layer(&app.manifest.layer.name, app.namespace_id, layer_config) {
            Ok(layer_id) => {
                app.layer = Some(layer_id);
            }
            Err(e) => {
                app.mark_failed(format!("Failed to create layer: {}", e));
                return Err(OverlayAppError::LoadFailed(e.to_string()));
            }
        }

        app.set_state(OverlayAppState::Running);
        log::info!("Initialized overlay app {:?}", id);

        Ok(())
    }

    /// Start an overlay app
    pub fn start(&mut self, id: OverlayAppId) -> Result<(), OverlayAppError> {
        let app = self.apps.get_mut(&id).ok_or(OverlayAppError::NotFound(id))?;

        match app.state {
            OverlayAppState::Paused | OverlayAppState::Initializing => {
                app.set_state(OverlayAppState::Running);
                Ok(())
            }
            OverlayAppState::Running => Ok(()),
            _ => Err(OverlayAppError::InvalidState),
        }
    }

    /// Pause an overlay app
    pub fn pause(&mut self, id: OverlayAppId) -> Result<(), OverlayAppError> {
        let app = self.apps.get_mut(&id).ok_or(OverlayAppError::NotFound(id))?;

        if app.state != OverlayAppState::Running {
            return Err(OverlayAppError::InvalidState);
        }

        app.set_state(OverlayAppState::Paused);
        Ok(())
    }

    /// Unload an overlay app
    pub fn unload(&mut self, id: OverlayAppId, layer_manager: &mut LayerManager) -> Result<(), OverlayAppError> {
        let app = self.apps.get_mut(&id).ok_or(OverlayAppError::NotFound(id))?;

        app.set_state(OverlayAppState::Unloading);

        // Destroy the app's layer
        if let Some(layer_id) = app.layer.take() {
            let _ = layer_manager.destroy_layer(layer_id);
        }

        // App's World is dropped automatically when removed
        app.set_state(OverlayAppState::Stopped);
        self.handles.remove(&id);

        log::info!("Unloaded overlay app {:?}", id);
        Ok(())
    }

    /// Unload all overlay apps
    pub fn unload_all(&mut self, layer_manager: &mut LayerManager) {
        let ids: Vec<_> = self.apps.keys().copied().collect();
        for id in ids {
            if let Err(e) = self.unload(id, layer_manager) {
                log::warn!("Failed to unload overlay app {:?}: {:?}", id, e);
            }
        }
    }

    /// Get an overlay app by ID
    pub fn get(&self, id: OverlayAppId) -> Option<&OverlayApp> {
        self.apps.get(&id)
    }

    /// Get a mutable overlay app by ID
    pub fn get_mut(&mut self, id: OverlayAppId) -> Option<&mut OverlayApp> {
        self.apps.get_mut(&id)
    }

    /// Get the namespace handle for an overlay app
    pub fn get_handle(&self, id: OverlayAppId) -> Option<&NamespaceHandle> {
        self.handles.get(&id)
    }

    /// Get all running overlay apps
    pub fn running(&self) -> Vec<OverlayAppId> {
        self.apps
            .iter()
            .filter(|(_, app)| app.state == OverlayAppState::Running)
            .map(|(id, _)| *id)
            .collect()
    }

    /// Process transactions for all running overlay apps
    pub fn process_all_transactions(
        &mut self,
        layer_manager: &mut LayerManager,
        asset_registry: &mut AssetRegistry,
    ) -> Vec<ApplyResult> {
        let mut all_results = Vec::new();

        let running_ids = self.running();
        for id in running_ids {
            if let Some(app) = self.apps.get_mut(&id) {
                let results = app.process_transactions(
                    &self.patch_bus,
                    layer_manager,
                    asset_registry,
                );
                all_results.extend(results);
            }
        }

        all_results
    }

    /// Get app count
    pub fn len(&self) -> usize {
        self.apps.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.apps.is_empty()
    }

    /// Remove stopped apps
    pub fn gc(&mut self) -> Vec<OverlayAppId> {
        let stopped: Vec<_> = self
            .apps
            .iter()
            .filter(|(_, app)| app.state == OverlayAppState::Stopped)
            .map(|(id, _)| *id)
            .collect();

        for id in &stopped {
            self.apps.remove(id);
        }

        stopped
    }
}

/// Errors from overlay app operations
#[derive(Debug, Clone)]
pub enum OverlayAppError {
    /// Too many apps
    TooManyApps,
    /// App not found
    NotFound(OverlayAppId),
    /// Invalid state for operation
    InvalidState,
    /// Permission denied
    PermissionDenied,
    /// Load failed
    LoadFailed(String),
}

impl std::fmt::Display for OverlayAppError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TooManyApps => write!(f, "Maximum overlay app count exceeded"),
            Self::NotFound(id) => write!(f, "Overlay app {:?} not found", id),
            Self::InvalidState => write!(f, "Invalid state for this operation"),
            Self::PermissionDenied => write!(f, "Permission denied"),
            Self::LoadFailed(msg) => write!(f, "Load failed: {}", msg),
        }
    }
}

impl std::error::Error for OverlayAppError {}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_manifest() -> OverlayAppManifest {
        OverlayAppManifest {
            name: "test_overlay".to_string(),
            version: "1.0.0".to_string(),
            description: None,
            author: None,
            layer: OverlayLayerConfig::default(),
            resources: OverlayResources::default(),
            permissions: OverlayPermissions::default(),
        }
    }

    #[test]
    fn test_overlay_has_own_world() {
        let manifest = test_manifest();
        let ns = NamespaceId::new();
        let app = OverlayApp::new(manifest, ns);

        // Each app has its own world
        assert_eq!(app.world().entity_count(), 0);
    }

    #[test]
    fn test_overlay_lifecycle() {
        let patch_bus = Arc::new(PatchBus::default());
        let mut manager = OverlayAppManager::new(10, patch_bus);
        let mut layer_manager = crate::layer::LayerManager::new(10);

        // Load
        let id = manager.load(test_manifest()).unwrap();
        assert_eq!(manager.get(id).unwrap().state, OverlayAppState::Loading);

        // Initialize (creates layer)
        manager.initialize(id, &mut layer_manager).unwrap();
        assert_eq!(manager.get(id).unwrap().state, OverlayAppState::Running);
        assert!(manager.get(id).unwrap().layer.is_some());

        // Each app has its own world
        let world1 = manager.get(id).unwrap().world();

        // Load another app
        let id2 = manager.load(test_manifest()).unwrap();
        manager.initialize(id2, &mut layer_manager).unwrap();

        // Verify worlds are separate
        // (In a real test we'd add entities and verify isolation)
    }
}
