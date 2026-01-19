//! Asset service
//!
//! Handles loading, caching, and lifecycle of assets.

use std::any::Any;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::time::Instant;
use thiserror::Error;
use serde::{Serialize, Deserialize};

use crate::service::{Service, ServiceId, ServiceState, ServiceHealth, ServiceConfig, ServiceResult, ServiceError};

/// Asset handle - reference to a loaded asset
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct AssetHandle(u64);

impl AssetHandle {
    /// Create a new asset handle
    pub fn new(id: u64) -> Self {
        Self(id)
    }

    /// Get the raw ID
    pub fn id(&self) -> u64 {
        self.0
    }
}

/// Asset loading state
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum AssetState {
    /// Not loaded
    Unloaded,
    /// Currently loading
    Loading,
    /// Loaded and ready
    Loaded,
    /// Failed to load
    Failed,
    /// Unloading
    Unloading,
}

/// Asset metadata
#[derive(Debug, Clone)]
pub struct AssetMetadata {
    /// Asset handle
    pub handle: AssetHandle,
    /// Source path
    pub path: PathBuf,
    /// Asset type
    pub asset_type: String,
    /// Current state
    pub state: AssetState,
    /// Size in bytes (if known)
    pub size_bytes: Option<u64>,
    /// Load time in milliseconds
    pub load_time_ms: Option<u64>,
    /// Last access time
    pub last_accessed: Instant,
    /// Reference count
    pub ref_count: u32,
}

/// Asset errors
#[derive(Debug, Error)]
pub enum AssetError {
    #[error("Asset not found: {0}")]
    NotFound(String),

    #[error("Failed to load asset: {0}")]
    LoadFailed(String),

    #[error("Invalid asset path: {0}")]
    InvalidPath(String),

    #[error("Asset type mismatch: expected {expected}, got {actual}")]
    TypeMismatch { expected: String, actual: String },

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
}

/// Loaded asset data
pub struct LoadedAsset {
    /// Asset metadata
    pub metadata: AssetMetadata,
    /// Raw bytes
    pub data: Vec<u8>,
}

/// Asset service configuration
#[derive(Debug, Clone)]
pub struct AssetServiceConfig {
    /// Base service config
    pub service: ServiceConfig,
    /// Root asset directory
    pub root_path: PathBuf,
    /// Maximum cache size in bytes
    pub max_cache_bytes: u64,
    /// Cache eviction policy
    pub eviction_policy: EvictionPolicy,
    /// Enable hot-reload watching
    pub hot_reload: bool,
}

impl Default for AssetServiceConfig {
    fn default() -> Self {
        Self {
            service: ServiceConfig::new("assets"),
            root_path: PathBuf::from("assets"),
            max_cache_bytes: 256 * 1024 * 1024, // 256 MB
            eviction_policy: EvictionPolicy::Lru,
            hot_reload: false,
        }
    }
}

/// Cache eviction policy
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum EvictionPolicy {
    /// Least Recently Used
    Lru,
    /// Least Frequently Used
    Lfu,
    /// First In First Out
    Fifo,
}

/// Asset cache entry
struct CacheEntry {
    /// Asset data
    data: Vec<u8>,
    /// Metadata
    metadata: AssetMetadata,
    /// Access count for LFU
    access_count: u64,
}

/// Asset service - manages asset loading and caching
pub struct AssetService {
    /// Service configuration
    config: AssetServiceConfig,
    /// Current state
    state: ServiceState,
    /// Next handle ID
    next_handle: u64,
    /// Loaded assets cache
    cache: HashMap<AssetHandle, CacheEntry>,
    /// Path to handle mapping
    path_to_handle: HashMap<PathBuf, AssetHandle>,
    /// Current cache size
    cache_size: u64,
    /// Load statistics
    stats: AssetStats,
}

/// Asset loading statistics
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct AssetStats {
    /// Total assets loaded
    pub total_loaded: u64,
    /// Cache hits
    pub cache_hits: u64,
    /// Cache misses
    pub cache_misses: u64,
    /// Total bytes loaded
    pub bytes_loaded: u64,
    /// Failed loads
    pub failed_loads: u64,
}

impl AssetService {
    /// Create a new asset service
    pub fn new(config: AssetServiceConfig) -> Self {
        Self {
            config,
            state: ServiceState::Stopped,
            next_handle: 1,
            cache: HashMap::new(),
            path_to_handle: HashMap::new(),
            cache_size: 0,
            stats: AssetStats::default(),
        }
    }

    /// Create with default configuration
    pub fn with_defaults() -> Self {
        Self::new(AssetServiceConfig::default())
    }

    /// Load an asset from path
    pub fn load(&mut self, path: impl AsRef<Path>) -> Result<AssetHandle, AssetError> {
        let path = path.as_ref();
        let full_path = self.config.root_path.join(path);

        // Check cache
        if let Some(handle) = self.path_to_handle.get(&full_path).cloned() {
            if let Some(entry) = self.cache.get_mut(&handle) {
                entry.metadata.last_accessed = Instant::now();
                entry.metadata.ref_count += 1;
                entry.access_count += 1;
                self.stats.cache_hits += 1;
                return Ok(handle);
            }
        }

        self.stats.cache_misses += 1;

        // Load from disk
        let start = Instant::now();
        let data = std::fs::read(&full_path).map_err(|e| {
            self.stats.failed_loads += 1;
            AssetError::Io(e)
        })?;
        let load_time = start.elapsed().as_millis() as u64;

        // Create handle
        let handle = AssetHandle::new(self.next_handle);
        self.next_handle += 1;

        // Detect asset type from extension
        let asset_type = full_path
            .extension()
            .and_then(|e| e.to_str())
            .unwrap_or("unknown")
            .to_string();

        let size = data.len() as u64;

        // Evict if needed
        self.evict_if_needed(size);

        // Create metadata
        let metadata = AssetMetadata {
            handle: handle.clone(),
            path: full_path.clone(),
            asset_type,
            state: AssetState::Loaded,
            size_bytes: Some(size),
            load_time_ms: Some(load_time),
            last_accessed: Instant::now(),
            ref_count: 1,
        };

        // Cache entry
        let entry = CacheEntry {
            data,
            metadata,
            access_count: 1,
        };

        self.cache.insert(handle.clone(), entry);
        self.path_to_handle.insert(full_path, handle.clone());
        self.cache_size += size;
        self.stats.total_loaded += 1;
        self.stats.bytes_loaded += size;

        Ok(handle)
    }

    /// Get asset data by handle
    pub fn get(&self, handle: &AssetHandle) -> Option<&[u8]> {
        self.cache.get(handle).map(|e| e.data.as_slice())
    }

    /// Get asset metadata
    pub fn metadata(&self, handle: &AssetHandle) -> Option<&AssetMetadata> {
        self.cache.get(handle).map(|e| &e.metadata)
    }

    /// Unload an asset
    pub fn unload(&mut self, handle: &AssetHandle) -> Result<(), AssetError> {
        if let Some(entry) = self.cache.remove(handle) {
            if let Some(size) = entry.metadata.size_bytes {
                self.cache_size = self.cache_size.saturating_sub(size);
            }
            self.path_to_handle.remove(&entry.metadata.path);
            Ok(())
        } else {
            Err(AssetError::NotFound(format!("{:?}", handle)))
        }
    }

    /// Get loading statistics
    pub fn stats(&self) -> &AssetStats {
        &self.stats
    }

    /// Get current cache size
    pub fn cache_size(&self) -> u64 {
        self.cache_size
    }

    /// Get number of cached assets
    pub fn cached_count(&self) -> usize {
        self.cache.len()
    }

    /// Evict assets if cache is too large
    fn evict_if_needed(&mut self, new_size: u64) {
        while self.cache_size + new_size > self.config.max_cache_bytes && !self.cache.is_empty() {
            let to_evict = match self.config.eviction_policy {
                EvictionPolicy::Lru => self.find_lru_entry(),
                EvictionPolicy::Lfu => self.find_lfu_entry(),
                EvictionPolicy::Fifo => self.find_fifo_entry(),
            };

            if let Some(handle) = to_evict {
                let _ = self.unload(&handle);
            } else {
                break;
            }
        }
    }

    fn find_lru_entry(&self) -> Option<AssetHandle> {
        self.cache.iter()
            .filter(|(_, e)| e.metadata.ref_count == 0)
            .min_by_key(|(_, e)| e.metadata.last_accessed)
            .map(|(h, _)| h.clone())
    }

    fn find_lfu_entry(&self) -> Option<AssetHandle> {
        self.cache.iter()
            .filter(|(_, e)| e.metadata.ref_count == 0)
            .min_by_key(|(_, e)| e.access_count)
            .map(|(h, _)| h.clone())
    }

    fn find_fifo_entry(&self) -> Option<AssetHandle> {
        // Use handle ID as insertion order proxy
        self.cache.iter()
            .filter(|(_, e)| e.metadata.ref_count == 0)
            .min_by_key(|(h, _)| h.id())
            .map(|(h, _)| h.clone())
    }

    /// Clear entire cache
    pub fn clear_cache(&mut self) {
        self.cache.clear();
        self.path_to_handle.clear();
        self.cache_size = 0;
    }

    /// Reload an asset
    pub fn reload(&mut self, handle: &AssetHandle) -> Result<(), AssetError> {
        let path = self.cache.get(handle)
            .map(|e| e.metadata.path.clone())
            .ok_or_else(|| AssetError::NotFound(format!("{:?}", handle)))?;

        self.unload(handle)?;
        let _ = self.load(&path)?;
        Ok(())
    }
}

impl Service for AssetService {
    fn id(&self) -> &ServiceId {
        &self.config.service.id
    }

    fn state(&self) -> ServiceState {
        self.state
    }

    fn health(&self) -> ServiceHealth {
        if self.state == ServiceState::Running {
            let mut health = ServiceHealth::healthy();
            health.metrics.insert("cache_size".to_string(), self.cache_size as f64);
            health.metrics.insert("cached_assets".to_string(), self.cache.len() as f64);
            health.metrics.insert("cache_hit_rate".to_string(), {
                let total = self.stats.cache_hits + self.stats.cache_misses;
                if total > 0 {
                    self.stats.cache_hits as f64 / total as f64
                } else {
                    0.0
                }
            });
            health
        } else {
            ServiceHealth::default()
        }
    }

    fn config(&self) -> &ServiceConfig {
        &self.config.service
    }

    fn start(&mut self) -> ServiceResult<()> {
        if !self.config.root_path.exists() {
            // Create root directory if it doesn't exist
            std::fs::create_dir_all(&self.config.root_path)
                .map_err(|e| ServiceError::StartFailed(e.to_string()))?;
        }
        self.state = ServiceState::Running;
        Ok(())
    }

    fn stop(&mut self) -> ServiceResult<()> {
        self.clear_cache();
        self.state = ServiceState::Stopped;
        Ok(())
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};

    static TEST_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn temp_dir() -> PathBuf {
        let id = TEST_COUNTER.fetch_add(1, Ordering::SeqCst);
        let dir = std::env::temp_dir().join(format!("void_assets_test_{}_{}", std::process::id(), id));
        let _ = std::fs::remove_dir_all(&dir); // Clean any previous run
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn test_asset_handle() {
        let h1 = AssetHandle::new(1);
        let h2 = AssetHandle::new(2);
        assert_eq!(h1.id(), 1);
        assert_ne!(h1, h2);
    }

    #[test]
    fn test_asset_service_creation() {
        let service = AssetService::with_defaults();
        assert_eq!(service.state(), ServiceState::Stopped);
        assert_eq!(service.cached_count(), 0);
    }

    #[test]
    fn test_load_asset() {
        let root = temp_dir();

        // Create test file
        let test_file = root.join("test.txt");
        std::fs::write(&test_file, b"hello world").unwrap();

        let config = AssetServiceConfig {
            root_path: root.clone(),
            ..Default::default()
        };

        let mut service = AssetService::new(config);
        service.start().unwrap();

        let handle = service.load("test.txt").unwrap();
        assert_eq!(service.cached_count(), 1);

        let data = service.get(&handle).unwrap();
        assert_eq!(data, b"hello world");

        // Cleanup
        std::fs::remove_dir_all(root).ok();
    }

    #[test]
    fn test_cache_hit() {
        let root = temp_dir();
        std::fs::write(root.join("test.txt"), b"data").unwrap();

        let config = AssetServiceConfig {
            root_path: root.clone(),
            ..Default::default()
        };

        let mut service = AssetService::new(config);
        service.start().unwrap();

        // First load - cache miss
        let h1 = service.load("test.txt").unwrap();
        assert_eq!(service.stats().cache_misses, 1);
        assert_eq!(service.stats().cache_hits, 0);

        // Second load - cache hit
        let h2 = service.load("test.txt").unwrap();
        assert_eq!(h1, h2);
        assert_eq!(service.stats().cache_hits, 1);

        std::fs::remove_dir_all(root).ok();
    }

    #[test]
    fn test_unload_asset() {
        let root = temp_dir();
        std::fs::write(root.join("test.txt"), b"data").unwrap();

        let config = AssetServiceConfig {
            root_path: root.clone(),
            ..Default::default()
        };

        let mut service = AssetService::new(config);
        service.start().unwrap();

        let handle = service.load("test.txt").unwrap();
        assert_eq!(service.cached_count(), 1);

        service.unload(&handle).unwrap();
        assert_eq!(service.cached_count(), 0);
        assert!(service.get(&handle).is_none());

        std::fs::remove_dir_all(root).ok();
    }

    #[test]
    fn test_cache_eviction() {
        let root = temp_dir();

        // Create test files
        for i in 0..5 {
            std::fs::write(root.join(format!("file{}.txt", i)), vec![0u8; 100]).unwrap();
        }

        let config = AssetServiceConfig {
            root_path: root.clone(),
            max_cache_bytes: 250, // Only fits 2 files
            eviction_policy: EvictionPolicy::Fifo,
            ..Default::default()
        };

        let mut service = AssetService::new(config);
        service.start().unwrap();

        // Load all files
        for i in 0..5 {
            // Release reference before loading next
            let handle = service.load(format!("file{}.txt", i)).unwrap();
            if let Some(entry) = service.cache.get_mut(&handle) {
                entry.metadata.ref_count = 0; // Allow eviction
            }
        }

        // Should have evicted older files
        assert!(service.cache_size <= 250);

        std::fs::remove_dir_all(root).ok();
    }

    #[test]
    fn test_metadata() {
        let root = temp_dir();
        std::fs::write(root.join("test.txt"), b"hello").unwrap();

        let config = AssetServiceConfig {
            root_path: root.clone(),
            ..Default::default()
        };

        let mut service = AssetService::new(config);
        service.start().unwrap();

        let handle = service.load("test.txt").unwrap();
        let meta = service.metadata(&handle).unwrap();

        assert_eq!(meta.state, AssetState::Loaded);
        assert_eq!(meta.size_bytes, Some(5));
        assert_eq!(meta.asset_type, "txt");

        std::fs::remove_dir_all(root).ok();
    }

    #[test]
    fn test_service_lifecycle() {
        let root = temp_dir();
        let config = AssetServiceConfig {
            root_path: root.clone(),
            ..Default::default()
        };

        let mut service = AssetService::new(config);

        assert_eq!(service.state(), ServiceState::Stopped);

        service.start().unwrap();
        assert_eq!(service.state(), ServiceState::Running);

        service.stop().unwrap();
        assert_eq!(service.state(), ServiceState::Stopped);

        std::fs::remove_dir_all(root).ok();
    }
}
