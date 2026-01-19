//! Asset registry with versioning and rollback
//!
//! The asset registry tracks all loaded assets with version information,
//! enabling hot-swap and rollback to last-known-good state.

use parking_lot::RwLock;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Instant;

/// Unique identifier for an asset
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AssetId(u64);

impl AssetId {
    /// Create a new unique asset ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for AssetId {
    fn default() -> Self {
        Self::new()
    }
}

/// Version of an asset
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct AssetVersion(u32);

impl AssetVersion {
    /// Initial version
    pub const INITIAL: Self = Self(1);

    /// Create a new version
    pub fn new(version: u32) -> Self {
        Self(version)
    }

    /// Get the raw version number
    pub fn raw(&self) -> u32 {
        self.0
    }

    /// Get the next version
    pub fn next(&self) -> Self {
        Self(self.0 + 1)
    }
}

impl Default for AssetVersion {
    fn default() -> Self {
        Self::INITIAL
    }
}

/// State of an asset
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AssetState {
    /// Asset is being loaded
    Loading,
    /// Asset is ready to use
    Ready,
    /// Asset failed to load
    Failed,
    /// Asset is being unloaded
    Unloading,
    /// Asset is being hot-swapped
    HotSwapping,
}

/// Entry for a single asset in the registry
#[derive(Debug)]
pub struct AssetEntry {
    /// Unique ID
    pub id: AssetId,
    /// Path/name of the asset
    pub path: String,
    /// Asset type (shader, texture, mesh, etc.)
    pub asset_type: String,
    /// Current version
    pub version: AssetVersion,
    /// Current state
    pub state: AssetState,
    /// When the asset was loaded
    pub loaded_at: Instant,
    /// When the asset was last used
    pub last_used: Instant,
    /// Reference count
    pub ref_count: u32,
    /// Previous versions (for rollback)
    pub previous_versions: Vec<VersionedData>,
    /// Maximum versions to keep
    pub max_versions: usize,
}

/// Versioned asset data for rollback
#[derive(Debug, Clone)]
pub struct VersionedData {
    /// Version number
    pub version: AssetVersion,
    /// When this version was created
    pub created_at: Instant,
    /// Whether this version is known-good
    pub is_known_good: bool,
    // In practice, this would contain the actual asset data or a reference to it
    // pub data: Arc<dyn AssetData>,
}

impl AssetEntry {
    /// Create a new asset entry
    pub fn new(path: impl Into<String>, asset_type: impl Into<String>) -> Self {
        let now = Instant::now();
        Self {
            id: AssetId::new(),
            path: path.into(),
            asset_type: asset_type.into(),
            version: AssetVersion::INITIAL,
            state: AssetState::Loading,
            loaded_at: now,
            last_used: now,
            ref_count: 0,
            previous_versions: Vec::new(),
            max_versions: 3,
        }
    }

    /// Mark as ready
    pub fn mark_ready(&mut self) {
        self.state = AssetState::Ready;
    }

    /// Mark as failed
    pub fn mark_failed(&mut self) {
        self.state = AssetState::Failed;
    }

    /// Increment reference count
    pub fn add_ref(&mut self) {
        self.ref_count += 1;
        self.last_used = Instant::now();
    }

    /// Decrement reference count
    pub fn release_ref(&mut self) {
        self.ref_count = self.ref_count.saturating_sub(1);
    }

    /// Begin hot-swap to a new version
    pub fn begin_hot_swap(&mut self) {
        // Save current version
        let versioned = VersionedData {
            version: self.version,
            created_at: self.loaded_at,
            is_known_good: self.state == AssetState::Ready,
        };
        self.previous_versions.push(versioned);

        // Trim old versions
        while self.previous_versions.len() > self.max_versions {
            self.previous_versions.remove(0);
        }

        // Move to next version
        self.version = self.version.next();
        self.state = AssetState::HotSwapping;
        self.loaded_at = Instant::now();
    }

    /// Complete hot-swap
    pub fn complete_hot_swap(&mut self, success: bool) {
        if success {
            self.state = AssetState::Ready;
        } else {
            // Rollback to previous version
            if let Some(prev) = self.previous_versions.pop() {
                self.version = prev.version;
                self.state = if prev.is_known_good {
                    AssetState::Ready
                } else {
                    AssetState::Failed
                };
            } else {
                self.state = AssetState::Failed;
            }
        }
    }

    /// Get the last known-good version
    pub fn last_known_good(&self) -> Option<AssetVersion> {
        self.previous_versions
            .iter()
            .rev()
            .find(|v| v.is_known_good)
            .map(|v| v.version)
    }

    /// Rollback to a specific version
    pub fn rollback_to(&mut self, version: AssetVersion) -> bool {
        if let Some(idx) = self.previous_versions.iter().position(|v| v.version == version) {
            let target = self.previous_versions[idx].clone();
            self.version = target.version;
            self.state = if target.is_known_good {
                AssetState::Ready
            } else {
                AssetState::Failed
            };
            // Remove versions after the rollback point
            self.previous_versions.truncate(idx);
            true
        } else {
            false
        }
    }
}

/// The asset registry
pub struct AssetRegistry {
    /// All registered assets
    assets: HashMap<AssetId, AssetEntry>,
    /// Path to ID mapping
    by_path: HashMap<String, AssetId>,
    /// Assets by type
    by_type: HashMap<String, Vec<AssetId>>,
    /// Pending loads
    pending_loads: Vec<AssetId>,
    /// Pending hot-swaps
    pending_swaps: Vec<AssetId>,
}

impl AssetRegistry {
    /// Create a new asset registry
    pub fn new() -> Self {
        Self {
            assets: HashMap::new(),
            by_path: HashMap::new(),
            by_type: HashMap::new(),
            pending_loads: Vec::new(),
            pending_swaps: Vec::new(),
        }
    }

    /// Register a new asset
    pub fn register(&mut self, path: impl Into<String>, asset_type: impl Into<String>) -> AssetId {
        let path = path.into();
        let asset_type = asset_type.into();

        // Check if already registered
        if let Some(&id) = self.by_path.get(&path) {
            return id;
        }

        let entry = AssetEntry::new(path.clone(), asset_type.clone());
        let id = entry.id;

        self.by_path.insert(path, id);
        self.by_type.entry(asset_type).or_default().push(id);
        self.pending_loads.push(id);
        self.assets.insert(id, entry);

        id
    }

    /// Get an asset by ID
    pub fn get(&self, id: AssetId) -> Option<&AssetEntry> {
        self.assets.get(&id)
    }

    /// Get a mutable asset by ID
    pub fn get_mut(&mut self, id: AssetId) -> Option<&mut AssetEntry> {
        self.assets.get_mut(&id)
    }

    /// Get an asset by path
    pub fn get_by_path(&self, path: &str) -> Option<&AssetEntry> {
        self.by_path.get(path).and_then(|id| self.assets.get(id))
    }

    /// Get all assets of a type
    pub fn get_by_type(&self, asset_type: &str) -> Vec<&AssetEntry> {
        self.by_type
            .get(asset_type)
            .map(|ids| ids.iter().filter_map(|id| self.assets.get(id)).collect())
            .unwrap_or_default()
    }

    /// Mark an asset as ready
    pub fn mark_ready(&mut self, id: AssetId) {
        if let Some(entry) = self.assets.get_mut(&id) {
            entry.mark_ready();
            self.pending_loads.retain(|&pid| pid != id);
        }
    }

    /// Mark an asset as failed
    pub fn mark_failed(&mut self, id: AssetId) {
        if let Some(entry) = self.assets.get_mut(&id) {
            entry.mark_failed();
            self.pending_loads.retain(|&pid| pid != id);
        }
    }

    /// Begin hot-swapping an asset
    pub fn begin_hot_swap(&mut self, id: AssetId) -> bool {
        if let Some(entry) = self.assets.get_mut(&id) {
            entry.begin_hot_swap();
            self.pending_swaps.push(id);
            true
        } else {
            false
        }
    }

    /// Complete hot-swap
    pub fn complete_hot_swap(&mut self, id: AssetId, success: bool) {
        if let Some(entry) = self.assets.get_mut(&id) {
            entry.complete_hot_swap(success);
            self.pending_swaps.retain(|&pid| pid != id);
        }
    }

    /// Rollback an asset to last known-good
    pub fn rollback_to_known_good(&mut self, id: AssetId) -> bool {
        if let Some(entry) = self.assets.get_mut(&id) {
            if let Some(version) = entry.last_known_good() {
                return entry.rollback_to(version);
            }
        }
        false
    }

    /// Unregister an asset
    pub fn unregister(&mut self, id: AssetId) {
        if let Some(entry) = self.assets.remove(&id) {
            self.by_path.remove(&entry.path);
            if let Some(type_list) = self.by_type.get_mut(&entry.asset_type) {
                type_list.retain(|&aid| aid != id);
            }
        }
    }

    /// Get pending loads
    pub fn pending_loads(&self) -> &[AssetId] {
        &self.pending_loads
    }

    /// Get pending hot-swaps
    pub fn pending_swaps(&self) -> &[AssetId] {
        &self.pending_swaps
    }

    /// Get total asset count
    pub fn len(&self) -> usize {
        self.assets.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.assets.is_empty()
    }

    /// Garbage collect unused assets
    pub fn gc(&mut self, max_age_secs: f32) -> Vec<AssetId> {
        let now = Instant::now();
        let mut removed = Vec::new();

        self.assets.retain(|&id, entry| {
            // Don't remove if in use
            if entry.ref_count > 0 {
                return true;
            }
            // Don't remove if recently used
            let age = now.duration_since(entry.last_used).as_secs_f32();
            if age < max_age_secs {
                return true;
            }
            // Remove
            removed.push(id);
            false
        });

        // Clean up indices
        for id in &removed {
            if let Some(entry) = self.assets.get(id) {
                self.by_path.remove(&entry.path);
                if let Some(type_list) = self.by_type.get_mut(&entry.asset_type) {
                    type_list.retain(|&aid| aid != *id);
                }
            }
        }

        removed
    }
}

impl Default for AssetRegistry {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_asset_registration() {
        let mut registry = AssetRegistry::new();
        let id = registry.register("shaders/test.wgsl", "shader");

        assert!(registry.get(id).is_some());
        assert!(registry.get_by_path("shaders/test.wgsl").is_some());
    }

    #[test]
    fn test_hot_swap() {
        let mut registry = AssetRegistry::new();
        let id = registry.register("shaders/test.wgsl", "shader");
        registry.mark_ready(id);

        let entry = registry.get(id).unwrap();
        assert_eq!(entry.version, AssetVersion::INITIAL);
        assert_eq!(entry.state, AssetState::Ready);

        // Begin hot-swap
        registry.begin_hot_swap(id);
        let entry = registry.get(id).unwrap();
        assert_eq!(entry.version.raw(), 2);
        assert_eq!(entry.state, AssetState::HotSwapping);

        // Complete successfully
        registry.complete_hot_swap(id, true);
        let entry = registry.get(id).unwrap();
        assert_eq!(entry.state, AssetState::Ready);
        assert_eq!(entry.previous_versions.len(), 1);
    }

    #[test]
    fn test_rollback() {
        let mut registry = AssetRegistry::new();
        let id = registry.register("shaders/test.wgsl", "shader");
        registry.mark_ready(id);

        // Hot-swap to v2
        registry.begin_hot_swap(id);
        registry.complete_hot_swap(id, true);

        // Hot-swap to v3, but fail
        registry.begin_hot_swap(id);
        registry.complete_hot_swap(id, false);

        // Should have rolled back to v2
        let entry = registry.get(id).unwrap();
        assert_eq!(entry.version.raw(), 2);
        assert_eq!(entry.state, AssetState::Ready);
    }
}
