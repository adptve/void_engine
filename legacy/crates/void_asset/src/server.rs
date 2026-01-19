//! Asset Server - Central asset management
//!
//! The AssetServer is the main interface for loading and managing assets.
//! It supports:
//! - Async loading
//! - Hot-reloading
//! - Dependency tracking

use crate::handle::{AssetId, Handle, LoadState};
use crate::loader::{LoadContext, LoadError, LoadResult, LoaderRegistry, ErasedLoader, AssetLoader};
use crate::storage::AssetStorage;
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use alloc::sync::Arc;
use core::any::TypeId;
use parking_lot::RwLock;

/// Asset path type
pub type AssetPath = String;

/// Event from the asset server
#[derive(Clone, Debug)]
pub enum AssetEvent {
    /// Asset finished loading
    Loaded(AssetId),
    /// Asset failed to load
    Failed(AssetId, String),
    /// Asset was reloaded
    Reloaded(AssetId),
    /// Asset was unloaded
    Unloaded(AssetId),
    /// File changed on disk
    FileChanged(AssetPath),
}

/// Asset server configuration
#[derive(Clone, Debug)]
pub struct AssetServerConfig {
    /// Base asset directory
    pub asset_dir: String,
    /// Enable hot-reload
    pub hot_reload: bool,
    /// Maximum concurrent loads
    pub max_concurrent_loads: usize,
}

impl Default for AssetServerConfig {
    fn default() -> Self {
        Self {
            asset_dir: "assets".to_string(),
            hot_reload: true,
            max_concurrent_loads: 8,
        }
    }
}

/// Asset metadata
#[derive(Clone, Debug)]
pub struct AssetMeta {
    /// Asset path
    pub path: AssetPath,
    /// Asset type ID
    pub type_id: TypeId,
    /// Asset type name
    pub type_name: &'static str,
    /// Load time in milliseconds
    pub load_time_ms: u64,
    /// File size in bytes
    pub file_size: usize,
}

/// The main asset server
pub struct AssetServer {
    /// Configuration
    config: AssetServerConfig,
    /// Asset storage
    storage: AssetStorage,
    /// Loader registry
    loaders: RwLock<LoaderRegistry>,
    /// Path to asset ID mapping
    path_map: RwLock<BTreeMap<AssetPath, AssetId>>,
    /// Asset ID to metadata mapping
    meta_map: RwLock<BTreeMap<AssetId, AssetMeta>>,
    /// Pending load queue
    load_queue: RwLock<Vec<(AssetId, AssetPath, TypeId)>>,
    /// Event queue
    events: RwLock<Vec<AssetEvent>>,
    /// File watcher (platform-specific)
    #[cfg(feature = "hot-reload")]
    watcher: RwLock<Option<Box<dyn FileWatcher>>>,
}

impl AssetServer {
    /// Create a new asset server
    pub fn new(config: AssetServerConfig) -> Self {
        Self {
            config,
            storage: AssetStorage::new(),
            loaders: RwLock::new(LoaderRegistry::new()),
            path_map: RwLock::new(BTreeMap::new()),
            meta_map: RwLock::new(BTreeMap::new()),
            load_queue: RwLock::new(Vec::new()),
            events: RwLock::new(Vec::new()),
            #[cfg(feature = "hot-reload")]
            watcher: RwLock::new(None),
        }
    }

    /// Create with default configuration
    pub fn default_config() -> Self {
        Self::new(AssetServerConfig::default())
    }

    /// Get the configuration
    pub fn config(&self) -> &AssetServerConfig {
        &self.config
    }

    /// Register a loader
    pub fn register_loader<L: AssetLoader + 'static>(&self, loader: L) {
        self.loaders.write().register(loader);
    }

    /// Load an asset by path
    pub fn load<T: Send + Sync + 'static>(&self, path: impl Into<String>) -> Handle<T> {
        let path = path.into();
        let type_id = TypeId::of::<T>();

        // Check if already loaded
        if let Some(&id) = self.path_map.read().get(&path) {
            if let Some(handle) = self.storage.get_handle::<T>(id) {
                return handle;
            }
        }

        // Allocate new ID
        let id = self.storage.allocate_id();
        let handle = self.storage.register::<T>(id);

        // Map path to ID
        self.path_map.write().insert(path.clone(), id);

        // Queue for loading
        self.load_queue.write().push((id, path.clone(), type_id));
        self.storage.mark_loading(id);

        handle
    }

    /// Load an asset with explicit type
    pub fn load_untyped(&self, path: impl Into<String>, type_id: TypeId) -> AssetId {
        let path = path.into();

        // Check if already loaded
        if let Some(&id) = self.path_map.read().get(&path) {
            return id;
        }

        // Allocate new ID
        let id = self.storage.allocate_id();

        // Map path to ID
        self.path_map.write().insert(path.clone(), id);

        // Queue for loading
        self.load_queue.write().push((id, path, type_id));
        self.storage.mark_loading(id);

        id
    }

    /// Get a handle to an already-loaded asset
    pub fn get_handle<T: Send + Sync + 'static>(&self, path: &str) -> Option<Handle<T>> {
        let id = *self.path_map.read().get(path)?;
        self.storage.get_handle::<T>(id)
    }

    /// Get an asset ID by path
    pub fn get_id(&self, path: &str) -> Option<AssetId> {
        self.path_map.read().get(path).copied()
    }

    /// Get the path for an asset ID
    pub fn get_path(&self, id: AssetId) -> Option<String> {
        self.path_map.read()
            .iter()
            .find(|(_, &v)| v == id)
            .map(|(k, _)| k.clone())
    }

    /// Check if an asset is loaded
    pub fn is_loaded(&self, id: AssetId) -> bool {
        self.storage.is_loaded(id)
    }

    /// Get asset load state
    pub fn get_state(&self, id: AssetId) -> Option<LoadState> {
        self.storage.get_state(id)
    }

    /// Get asset metadata
    pub fn get_meta(&self, id: AssetId) -> Option<AssetMeta> {
        self.meta_map.read().get(&id).cloned()
    }

    /// Process pending loads
    ///
    /// Call this each frame to process the load queue.
    /// Returns the number of assets processed.
    pub fn process(&self, read_file: impl Fn(&str) -> Option<Vec<u8>>) -> usize {
        let queue: Vec<_> = self.load_queue.write().drain(..).collect();
        let count = queue.len();

        for (id, path, type_id) in queue {
            match self.load_asset(&path, id, type_id, &read_file) {
                Ok((asset, deps)) => {
                    self.storage.store_erased(id, asset, type_id, deps.clone());

                    // Store metadata
                    self.meta_map.write().insert(id, AssetMeta {
                        path: path.clone(),
                        type_id,
                        type_name: "unknown", // Would need type registry
                        load_time_ms: 0,
                        file_size: 0,
                    });

                    self.events.write().push(AssetEvent::Loaded(id));
                }
                Err(e) => {
                    self.storage.mark_failed(id);
                    self.events.write().push(AssetEvent::Failed(id, alloc::format!("{}", e)));
                }
            }
        }

        count
    }

    /// Load a single asset
    fn load_asset(
        &self,
        path: &str,
        id: AssetId,
        _type_id: TypeId,
        read_file: impl Fn(&str) -> Option<Vec<u8>>,
    ) -> LoadResult<(Box<dyn core::any::Any + Send + Sync>, Vec<AssetId>)> {
        // Read file data
        let full_path = if path.starts_with('/') || path.contains(':') {
            path.to_string()
        } else {
            alloc::format!("{}/{}", self.config.asset_dir, path)
        };

        let data = read_file(&full_path)
            .ok_or_else(|| LoadError::NotFound(full_path.clone()))?;

        // Create load context
        let mut ctx = LoadContext::new(path, &data, id);

        // Load using registry
        let loaders = self.loaders.read();
        let asset = loaders.load(&mut ctx)?;

        Ok((asset, ctx.dependencies))
    }

    /// Reload an asset
    pub fn reload(&self, id: AssetId, read_file: impl Fn(&str) -> Option<Vec<u8>>) -> bool {
        let path = match self.get_path(id) {
            Some(p) => p,
            None => return false,
        };

        let meta = match self.get_meta(id) {
            Some(m) => m,
            None => return false,
        };

        self.storage.mark_reloading(id);

        match self.load_asset(&path, id, meta.type_id, read_file) {
            Ok((asset, deps)) => {
                self.storage.store_erased(id, asset, meta.type_id, deps);
                self.storage.increment_generation(id);
                self.events.write().push(AssetEvent::Reloaded(id));
                true
            }
            Err(_) => {
                self.storage.mark_failed(id);
                false
            }
        }
    }

    /// Unload an asset
    pub fn unload(&self, id: AssetId) -> bool {
        if let Some(path) = self.get_path(id) {
            self.path_map.write().remove(&path);
        }
        self.meta_map.write().remove(&id);

        if self.storage.remove(id) {
            self.events.write().push(AssetEvent::Unloaded(id));
            true
        } else {
            false
        }
    }

    /// Drain events
    pub fn drain_events(&self) -> Vec<AssetEvent> {
        self.events.write().drain(..).collect()
    }

    /// Get pending load count
    pub fn pending_count(&self) -> usize {
        self.load_queue.read().len()
    }

    /// Get total loaded asset count
    pub fn loaded_count(&self) -> usize {
        self.storage.len()
    }

    /// Collect garbage (remove unreferenced assets)
    pub fn collect_garbage(&self) -> usize {
        let removed_ids = self.storage.collect_garbage();
        let count = removed_ids.len();

        for id in removed_ids {
            if let Some(path) = self.get_path(id) {
                self.path_map.write().remove(&path);
            }
            self.meta_map.write().remove(&id);
            self.storage.remove(id);
            self.events.write().push(AssetEvent::Unloaded(id));
        }

        count
    }
}

impl Default for AssetServer {
    fn default() -> Self {
        Self::default_config()
    }
}

/// File watcher trait for hot-reload
#[cfg(feature = "hot-reload")]
pub trait FileWatcher: Send + Sync {
    /// Start watching a path
    fn watch(&mut self, path: &str) -> bool;

    /// Stop watching a path
    fn unwatch(&mut self, path: &str) -> bool;

    /// Poll for changes
    fn poll(&mut self) -> Vec<String>;
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::loader::AssetLoader;

    struct TextAsset(String);

    struct TextLoader;

    impl AssetLoader for TextLoader {
        type Asset = TextAsset;

        fn extensions(&self) -> &[&str] {
            &["txt"]
        }

        fn load(&self, ctx: &mut LoadContext) -> LoadResult<Self::Asset> {
            let text = ctx.read_string()?;
            Ok(TextAsset(text.to_string()))
        }
    }

    #[test]
    fn test_asset_server() {
        let server = AssetServer::default_config();
        server.register_loader(TextLoader);

        // Mock file reader
        let read_file = |path: &str| -> Option<Vec<u8>> {
            if path.ends_with("test.txt") {
                Some(b"Hello, Asset!".to_vec())
            } else {
                None
            }
        };

        let handle: Handle<TextAsset> = server.load("test.txt");
        assert_eq!(handle.state(), LoadState::Loading);

        // Process loads
        let processed = server.process(read_file);
        assert_eq!(processed, 1);

        // Check events
        let events = server.drain_events();
        assert_eq!(events.len(), 1);
        assert!(matches!(events[0], AssetEvent::Loaded(_)));
    }
}
