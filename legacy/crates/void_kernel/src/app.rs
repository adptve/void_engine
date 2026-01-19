//! App management - isolated modular apps
//!
//! Apps are the primary unit of isolation in Void Engine.
//! Each app runs in its own namespace and renders to its own layers.

use void_ir::{Namespace, NamespaceHandle, NamespaceId, PatchBus};
use parking_lot::RwLock;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Instant;

/// Unique identifier for an app
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AppId(u64);

impl AppId {
    /// Create a new unique app ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for AppId {
    fn default() -> Self {
        Self::new()
    }
}

/// State of an app
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AppState {
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

/// Manifest describing an app
#[derive(Debug, Clone)]
pub struct AppManifest {
    /// App name
    pub name: String,
    /// App version
    pub version: String,
    /// App description
    pub description: Option<String>,
    /// Author
    pub author: Option<String>,
    /// Required layers
    pub layers: Vec<LayerRequest>,
    /// Resource requirements
    pub resources: ResourceRequirements,
    /// Permissions requested
    pub permissions: AppPermissions,
}

/// Request for a layer
#[derive(Debug, Clone)]
pub struct LayerRequest {
    /// Layer name
    pub name: String,
    /// Layer type
    pub layer_type: void_ir::patch::LayerType,
    /// Priority
    pub priority: i32,
}

/// Resource requirements for an app
#[derive(Debug, Clone, Default)]
pub struct ResourceRequirements {
    /// Maximum entities
    pub max_entities: Option<u32>,
    /// Maximum memory (bytes)
    pub max_memory: Option<u64>,
    /// Maximum layers
    pub max_layers: Option<u32>,
}

/// Permissions for an app
#[derive(Debug, Clone, Default)]
pub struct AppPermissions {
    /// Can access network
    pub network: bool,
    /// Can access filesystem
    pub filesystem: bool,
    /// Can load scripts
    pub scripts: bool,
    /// Can access other app's entities (read-only)
    pub cross_app_read: bool,
}

/// An app instance
#[derive(Debug)]
pub struct App {
    /// Unique ID
    pub id: AppId,
    /// Manifest
    pub manifest: AppManifest,
    /// Namespace ID for this app
    pub namespace_id: NamespaceId,
    /// Current state
    pub state: AppState,
    /// When the app was loaded
    pub loaded_at: Instant,
    /// When the app was last active
    pub last_active: Instant,
    /// Error message if failed
    pub error: Option<String>,
    /// Allocated layers
    pub layers: Vec<crate::layer::LayerId>,
    /// Metrics
    pub metrics: AppMetrics,
}

/// Metrics for an app
#[derive(Debug, Clone, Default)]
pub struct AppMetrics {
    /// Total frames processed
    pub frames: u64,
    /// Total patches emitted
    pub patches_emitted: u64,
    /// Total patches applied
    pub patches_applied: u64,
    /// Total patches rejected
    pub patches_rejected: u64,
    /// Average update time (ms)
    pub avg_update_ms: f32,
    /// Peak update time (ms)
    pub peak_update_ms: f32,
    /// Entity count
    pub entity_count: u32,
}

impl App {
    /// Create a new app
    pub fn new(manifest: AppManifest, namespace_id: NamespaceId) -> Self {
        let now = Instant::now();
        Self {
            id: AppId::new(),
            manifest,
            namespace_id,
            state: AppState::Loading,
            loaded_at: now,
            last_active: now,
            error: None,
            layers: Vec::new(),
            metrics: AppMetrics::default(),
        }
    }

    /// Update state
    pub fn set_state(&mut self, state: AppState) {
        self.state = state;
        if state == AppState::Running {
            self.last_active = Instant::now();
        }
    }

    /// Mark as failed
    pub fn mark_failed(&mut self, error: impl Into<String>) {
        self.state = AppState::Failed;
        self.error = Some(error.into());
    }

    /// Update metrics after a frame
    pub fn update_metrics(&mut self, update_time_ms: f32, patches: u64) {
        self.metrics.frames += 1;
        self.metrics.patches_emitted += patches;

        // Update average
        let n = self.metrics.frames as f32;
        self.metrics.avg_update_ms =
            (self.metrics.avg_update_ms * (n - 1.0) + update_time_ms) / n;

        if update_time_ms > self.metrics.peak_update_ms {
            self.metrics.peak_update_ms = update_time_ms;
        }
    }

    /// Check if app is active
    pub fn is_active(&self) -> bool {
        matches!(self.state, AppState::Running | AppState::Paused)
    }
}

/// Manages all apps
pub struct AppManager {
    /// All registered apps
    apps: HashMap<AppId, App>,
    /// Namespace handles for apps
    handles: HashMap<AppId, NamespaceHandle>,
    /// Maximum number of apps
    max_apps: usize,
    /// Reference to patch bus
    patch_bus: Arc<PatchBus>,
}

impl AppManager {
    /// Create a new app manager
    pub fn new(max_apps: usize, patch_bus: Arc<PatchBus>) -> Self {
        Self {
            apps: HashMap::new(),
            handles: HashMap::new(),
            max_apps,
            patch_bus,
        }
    }

    /// Load an app from a manifest
    pub fn load(&mut self, manifest: AppManifest) -> Result<AppId, AppError> {
        if self.apps.len() >= self.max_apps {
            return Err(AppError::TooManyApps);
        }

        // Create namespace for this app
        let mut namespace = Namespace::new(&manifest.name);

        // Apply permission restrictions
        if !manifest.permissions.cross_app_read {
            namespace.permissions.cross_namespace_read = false;
        }

        // Apply resource limits
        if let Some(max_entities) = manifest.resources.max_entities {
            namespace.limits.max_entities = Some(max_entities);
        }
        if let Some(max_layers) = manifest.resources.max_layers {
            namespace.limits.max_layers = Some(max_layers);
        }

        let namespace_id = namespace.id;
        let handle = self.patch_bus.register_namespace(namespace);

        let app = App::new(manifest, namespace_id);
        let id = app.id;

        self.apps.insert(id, app);
        self.handles.insert(id, handle);

        log::info!("Loaded app {:?}", id);
        Ok(id)
    }

    /// Initialize an app
    pub fn initialize(&mut self, id: AppId) -> Result<(), AppError> {
        let app = self.apps.get_mut(&id).ok_or(AppError::NotFound(id))?;

        if app.state != AppState::Loading {
            return Err(AppError::InvalidState);
        }

        app.set_state(AppState::Initializing);

        // In a real implementation, we'd:
        // 1. Create requested layers
        // 2. Load app's scripts/WASM
        // 3. Call app's init function

        app.set_state(AppState::Running);
        log::info!("Initialized app {:?}", id);

        Ok(())
    }

    /// Start an app
    pub fn start(&mut self, id: AppId) -> Result<(), AppError> {
        let app = self.apps.get_mut(&id).ok_or(AppError::NotFound(id))?;

        match app.state {
            AppState::Paused | AppState::Initializing => {
                app.set_state(AppState::Running);
                Ok(())
            }
            AppState::Running => Ok(()), // Already running
            _ => Err(AppError::InvalidState),
        }
    }

    /// Pause an app
    pub fn pause(&mut self, id: AppId) -> Result<(), AppError> {
        let app = self.apps.get_mut(&id).ok_or(AppError::NotFound(id))?;

        if app.state != AppState::Running {
            return Err(AppError::InvalidState);
        }

        app.set_state(AppState::Paused);
        Ok(())
    }

    /// Unload an app
    pub fn unload(&mut self, id: AppId) -> Result<(), AppError> {
        let app = self.apps.get_mut(&id).ok_or(AppError::NotFound(id))?;

        app.set_state(AppState::Unloading);

        // In a real implementation, we'd:
        // 1. Destroy all app's entities
        // 2. Destroy all app's layers
        // 3. Unload app's scripts/WASM
        // 4. Release namespace

        app.set_state(AppState::Stopped);
        self.handles.remove(&id);

        log::info!("Unloaded app {:?}", id);
        Ok(())
    }

    /// Unload all apps
    pub fn unload_all(&mut self) {
        let ids: Vec<_> = self.apps.keys().copied().collect();
        for id in ids {
            if let Err(e) = self.unload(id) {
                log::warn!("Failed to unload app {:?}: {:?}", id, e);
            }
        }
    }

    /// Get an app by ID
    pub fn get(&self, id: AppId) -> Option<&App> {
        self.apps.get(&id)
    }

    /// Get a mutable app by ID
    pub fn get_mut(&mut self, id: AppId) -> Option<&mut App> {
        self.apps.get_mut(&id)
    }

    /// Get the namespace handle for an app
    pub fn get_handle(&self, id: AppId) -> Option<&NamespaceHandle> {
        self.handles.get(&id)
    }

    /// Get all running apps
    pub fn running(&self) -> Vec<AppId> {
        self.apps
            .iter()
            .filter(|(_, app)| app.state == AppState::Running)
            .map(|(id, _)| *id)
            .collect()
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
    pub fn gc(&mut self) -> Vec<AppId> {
        let stopped: Vec<_> = self
            .apps
            .iter()
            .filter(|(_, app)| app.state == AppState::Stopped)
            .map(|(id, _)| *id)
            .collect();

        for id in &stopped {
            self.apps.remove(id);
        }

        stopped
    }
}

/// Errors from app operations
#[derive(Debug, Clone)]
pub enum AppError {
    /// Too many apps
    TooManyApps,
    /// App not found
    NotFound(AppId),
    /// Invalid state for operation
    InvalidState,
    /// Permission denied
    PermissionDenied,
    /// Load failed
    LoadFailed(String),
}

impl std::fmt::Display for AppError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TooManyApps => write!(f, "Maximum app count exceeded"),
            Self::NotFound(id) => write!(f, "App {:?} not found", id),
            Self::InvalidState => write!(f, "Invalid state for this operation"),
            Self::PermissionDenied => write!(f, "Permission denied"),
            Self::LoadFailed(msg) => write!(f, "Load failed: {}", msg),
        }
    }
}

impl std::error::Error for AppError {}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_manifest() -> AppManifest {
        AppManifest {
            name: "test_app".to_string(),
            version: "1.0.0".to_string(),
            description: None,
            author: None,
            layers: vec![LayerRequest {
                name: "content".to_string(),
                layer_type: void_ir::patch::LayerType::Content,
                priority: 0,
            }],
            resources: ResourceRequirements::default(),
            permissions: AppPermissions::default(),
        }
    }

    #[test]
    fn test_app_lifecycle() {
        let patch_bus = Arc::new(PatchBus::default());
        let mut manager = AppManager::new(10, patch_bus);

        // Load
        let id = manager.load(test_manifest()).unwrap();
        assert_eq!(manager.get(id).unwrap().state, AppState::Loading);

        // Initialize
        manager.initialize(id).unwrap();
        assert_eq!(manager.get(id).unwrap().state, AppState::Running);

        // Pause
        manager.pause(id).unwrap();
        assert_eq!(manager.get(id).unwrap().state, AppState::Paused);

        // Resume
        manager.start(id).unwrap();
        assert_eq!(manager.get(id).unwrap().state, AppState::Running);

        // Unload
        manager.unload(id).unwrap();
        assert_eq!(manager.get(id).unwrap().state, AppState::Stopped);
    }

    #[test]
    fn test_app_limit() {
        let patch_bus = Arc::new(PatchBus::default());
        let mut manager = AppManager::new(2, patch_bus);

        manager.load(test_manifest()).unwrap();
        manager.load(test_manifest()).unwrap();

        // Third should fail
        assert!(matches!(
            manager.load(test_manifest()),
            Err(AppError::TooManyApps)
        ));
    }
}
