//! MetaverseCore - The shared core metaverse scene
//!
//! The MetaverseCore represents THE central world that everyone shares.
//! Plugins and assets load INTO this world, contributing to the shared experience.
//!
//! This is fundamentally different from OverlayApps, which each have their OWN
//! isolated World and Scene.
//!
//! ## Contribution Model
//!
//! ```text
//! Plugin A ─┐
//! Plugin B ─┼──► MetaverseCore (ONE shared World)
//! Plugin C ─┘
//! ```
//!
//! Plugins can:
//! - Add entities to the shared world
//! - Register systems that run on shared entities
//! - Add assets (meshes, textures, etc.)
//! - Define behaviors for entities
//!
//! But plugins CANNOT:
//! - Have their own isolated World
//! - Override other plugins' entities (namespace protection still applies)

use void_ecs::World;
use void_ir::{Namespace, NamespaceHandle, NamespaceId, PatchBus};
use parking_lot::RwLock;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Instant;

use crate::layer::{LayerConfig, LayerId, LayerManager};
use crate::registry::AssetRegistry;
use crate::apply::{ApplyResult, PatchApplicator};

/// Unique identifier for a plugin
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct PluginId(u64);

impl PluginId {
    /// Create a new unique plugin ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for PluginId {
    fn default() -> Self {
        Self::new()
    }
}

/// State of a plugin
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PluginState {
    /// Plugin is being loaded
    Loading,
    /// Plugin is active
    Active,
    /// Plugin is disabled but still loaded
    Disabled,
    /// Plugin is being unloaded
    Unloading,
    /// Plugin has failed
    Failed,
}

/// Manifest describing a plugin
#[derive(Debug, Clone)]
pub struct PluginManifest {
    /// Plugin name
    pub name: String,
    /// Plugin version
    pub version: String,
    /// Plugin description
    pub description: Option<String>,
    /// Author
    pub author: Option<String>,
    /// Dependencies on other plugins
    pub dependencies: Vec<String>,
    /// Whether this plugin requires a dedicated layer
    pub layer: Option<PluginLayerRequest>,
    /// Resource requirements
    pub resources: PluginResources,
    /// Permissions
    pub permissions: PluginPermissions,
}

/// Layer request for a plugin
#[derive(Debug, Clone)]
pub struct PluginLayerRequest {
    /// Layer name
    pub name: String,
    /// Priority within the content layer
    pub priority: i32,
}

/// Resource requirements for a plugin
#[derive(Debug, Clone, Default)]
pub struct PluginResources {
    /// Maximum entities this plugin can create
    pub max_entities: Option<u32>,
    /// Maximum memory usage
    pub max_memory: Option<u64>,
}

/// Permissions for a plugin
#[derive(Debug, Clone, Default)]
pub struct PluginPermissions {
    /// Can access network
    pub network: bool,
    /// Can access filesystem
    pub filesystem: bool,
    /// Can load/execute scripts
    pub scripts: bool,
}

/// A loaded plugin
#[derive(Debug)]
pub struct Plugin {
    /// Unique ID
    pub id: PluginId,
    /// Manifest
    pub manifest: PluginManifest,
    /// Namespace ID for this plugin (within metaverse)
    pub namespace_id: NamespaceId,
    /// Current state
    pub state: PluginState,
    /// When the plugin was loaded
    pub loaded_at: Instant,
    /// Error message if failed
    pub error: Option<String>,
    /// Layer ID if plugin has one
    pub layer: Option<LayerId>,
    /// Metrics
    pub metrics: PluginMetrics,
}

/// Metrics for a plugin
#[derive(Debug, Clone, Default)]
pub struct PluginMetrics {
    /// Total entities created
    pub entities_created: u64,
    /// Current entity count
    pub entity_count: u32,
    /// Total patches emitted
    pub patches_emitted: u64,
}

impl Plugin {
    /// Create a new plugin
    pub fn new(manifest: PluginManifest, namespace_id: NamespaceId) -> Self {
        Self {
            id: PluginId::new(),
            manifest,
            namespace_id,
            state: PluginState::Loading,
            loaded_at: Instant::now(),
            error: None,
            layer: None,
            metrics: PluginMetrics::default(),
        }
    }

    /// Mark as failed
    pub fn mark_failed(&mut self, error: impl Into<String>) {
        self.state = PluginState::Failed;
        self.error = Some(error.into());
    }
}

/// The core metaverse - ONE shared world
pub struct MetaverseCore {
    /// The shared world - ALL plugins contribute to this
    world: World,
    /// Metaverse-level namespace (the "root" namespace)
    namespace_id: NamespaceId,
    /// Namespace handle for metaverse operations
    namespace_handle: Option<NamespaceHandle>,
    /// Loaded plugins
    plugins: HashMap<PluginId, Plugin>,
    /// Plugin namespace handles
    plugin_handles: HashMap<PluginId, NamespaceHandle>,
    /// Maximum plugins allowed
    max_plugins: usize,
    /// Patch applicator for this world
    applicator: PatchApplicator,
    /// Reference to shared patch bus
    patch_bus: Arc<PatchBus>,
    /// Content layer for the metaverse
    content_layer: Option<LayerId>,
}

impl MetaverseCore {
    /// Create a new metaverse core
    pub fn new(patch_bus: Arc<PatchBus>, max_plugins: usize) -> Self {
        // Create the metaverse namespace - parent of all plugin namespaces
        let metaverse_ns = Namespace::new("metaverse");
        let namespace_id = metaverse_ns.id;
        let namespace_handle = patch_bus.register_namespace(metaverse_ns);

        Self {
            world: World::new(),
            namespace_id,
            namespace_handle: Some(namespace_handle),
            plugins: HashMap::new(),
            plugin_handles: HashMap::new(),
            max_plugins,
            applicator: PatchApplicator::new(),
            patch_bus,
            content_layer: None,
        }
    }

    /// Get the shared world
    pub fn world(&self) -> &World {
        &self.world
    }

    /// Get mutable shared world
    pub fn world_mut(&mut self) -> &mut World {
        &mut self.world
    }

    /// Get the metaverse namespace ID
    pub fn namespace_id(&self) -> NamespaceId {
        self.namespace_id
    }

    /// Get the content layer ID
    pub fn content_layer(&self) -> Option<LayerId> {
        self.content_layer
    }

    /// Set the content layer (created by kernel)
    pub fn set_content_layer(&mut self, layer: LayerId) {
        self.content_layer = Some(layer);
    }

    /// Load a plugin into the metaverse
    pub fn load_plugin(&mut self, manifest: PluginManifest) -> Result<PluginId, PluginError> {
        if self.plugins.len() >= self.max_plugins {
            return Err(PluginError::TooManyPlugins);
        }

        // Check dependencies
        for dep in &manifest.dependencies {
            let found = self.plugins.values().any(|p| p.manifest.name == *dep);
            if !found {
                return Err(PluginError::MissingDependency(dep.clone()));
            }
        }

        // Create namespace for this plugin (child of metaverse namespace)
        let mut plugin_ns = Namespace::new(&manifest.name);

        // Apply resource limits
        if let Some(max_entities) = manifest.resources.max_entities {
            plugin_ns.limits.max_entities = Some(max_entities);
        }

        let plugin_ns_id = plugin_ns.id;
        let handle = self.patch_bus.register_namespace(plugin_ns);

        let plugin = Plugin::new(manifest, plugin_ns_id);
        let id = plugin.id;

        self.plugins.insert(id, plugin);
        self.plugin_handles.insert(id, handle);

        log::info!("Loaded plugin {:?} into metaverse", id);
        Ok(id)
    }

    /// Activate a plugin
    pub fn activate_plugin(&mut self, id: PluginId) -> Result<(), PluginError> {
        let plugin = self.plugins.get_mut(&id).ok_or(PluginError::NotFound(id))?;

        if plugin.state != PluginState::Loading && plugin.state != PluginState::Disabled {
            return Err(PluginError::InvalidState);
        }

        plugin.state = PluginState::Active;
        log::info!("Activated plugin {:?}", id);
        Ok(())
    }

    /// Disable a plugin (keep loaded but stop processing)
    pub fn disable_plugin(&mut self, id: PluginId) -> Result<(), PluginError> {
        let plugin = self.plugins.get_mut(&id).ok_or(PluginError::NotFound(id))?;

        if plugin.state != PluginState::Active {
            return Err(PluginError::InvalidState);
        }

        plugin.state = PluginState::Disabled;
        log::info!("Disabled plugin {:?}", id);
        Ok(())
    }

    /// Unload a plugin
    pub fn unload_plugin(&mut self, id: PluginId) -> Result<(), PluginError> {
        let plugin = self.plugins.get_mut(&id).ok_or(PluginError::NotFound(id))?;

        plugin.state = PluginState::Unloading;

        // In a real implementation:
        // 1. Destroy all entities created by this plugin
        // 2. Clean up any resources
        // 3. Release namespace

        self.plugin_handles.remove(&id);
        self.plugins.remove(&id);

        log::info!("Unloaded plugin {:?} from metaverse", id);
        Ok(())
    }

    /// Get a plugin by ID
    pub fn get_plugin(&self, id: PluginId) -> Option<&Plugin> {
        self.plugins.get(&id)
    }

    /// Get plugin handle for submitting patches
    pub fn get_plugin_handle(&self, id: PluginId) -> Option<&NamespaceHandle> {
        self.plugin_handles.get(&id)
    }

    /// Get all active plugins
    pub fn active_plugins(&self) -> Vec<PluginId> {
        self.plugins
            .iter()
            .filter(|(_, p)| p.state == PluginState::Active)
            .map(|(id, _)| *id)
            .collect()
    }

    /// Process transactions for the metaverse world
    pub fn process_transactions(
        &mut self,
        layer_manager: &mut LayerManager,
        asset_registry: &mut AssetRegistry,
    ) -> Vec<ApplyResult> {
        // Get current frame from patch bus
        let frame = self.patch_bus.current_frame();
        let transactions = self.patch_bus.drain_ready(frame);
        let mut results = Vec::with_capacity(transactions.len());

        for tx in transactions {
            // Only process transactions from metaverse/plugin namespaces
            // (OverlayApps have their own processing)
            let is_metaverse_tx = tx.source == self.namespace_id
                || self.plugins.values().any(|p| p.namespace_id == tx.source);

            if is_metaverse_tx {
                let result = self.applicator.apply(
                    tx,
                    &mut self.world,
                    layer_manager,
                    asset_registry,
                );
                self.patch_bus.commit(result.to_transaction_result());
                results.push(result);
            }
        }

        results
    }

    /// Get plugin count
    pub fn plugin_count(&self) -> usize {
        self.plugins.len()
    }
}

/// Errors from plugin operations
#[derive(Debug, Clone)]
pub enum PluginError {
    /// Too many plugins
    TooManyPlugins,
    /// Plugin not found
    NotFound(PluginId),
    /// Invalid state for operation
    InvalidState,
    /// Missing dependency
    MissingDependency(String),
    /// Load failed
    LoadFailed(String),
}

impl std::fmt::Display for PluginError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TooManyPlugins => write!(f, "Maximum plugin count exceeded"),
            Self::NotFound(id) => write!(f, "Plugin {:?} not found", id),
            Self::InvalidState => write!(f, "Invalid state for this operation"),
            Self::MissingDependency(dep) => write!(f, "Missing dependency: {}", dep),
            Self::LoadFailed(msg) => write!(f, "Load failed: {}", msg),
        }
    }
}

impl std::error::Error for PluginError {}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_manifest() -> PluginManifest {
        PluginManifest {
            name: "test_plugin".to_string(),
            version: "1.0.0".to_string(),
            description: None,
            author: None,
            dependencies: vec![],
            layer: None,
            resources: PluginResources::default(),
            permissions: PluginPermissions::default(),
        }
    }

    #[test]
    fn test_plugin_lifecycle() {
        let patch_bus = Arc::new(PatchBus::default());
        let mut metaverse = MetaverseCore::new(patch_bus, 10);

        // Load
        let id = metaverse.load_plugin(test_manifest()).unwrap();
        assert_eq!(metaverse.get_plugin(id).unwrap().state, PluginState::Loading);

        // Activate
        metaverse.activate_plugin(id).unwrap();
        assert_eq!(metaverse.get_plugin(id).unwrap().state, PluginState::Active);

        // Disable
        metaverse.disable_plugin(id).unwrap();
        assert_eq!(metaverse.get_plugin(id).unwrap().state, PluginState::Disabled);

        // Unload
        metaverse.unload_plugin(id).unwrap();
        assert!(metaverse.get_plugin(id).is_none());
    }

    #[test]
    fn test_dependency_check() {
        let patch_bus = Arc::new(PatchBus::default());
        let mut metaverse = MetaverseCore::new(patch_bus, 10);

        let mut manifest = test_manifest();
        manifest.dependencies = vec!["nonexistent".to_string()];

        let result = metaverse.load_plugin(manifest);
        assert!(matches!(result, Err(PluginError::MissingDependency(_))));
    }
}
