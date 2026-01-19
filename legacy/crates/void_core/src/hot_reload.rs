//! Hot-reload infrastructure for live code and asset updates
//!
//! Provides the foundation for reloading plugins, shaders, assets,
//! and even entire worlds at runtime without restarting.

use core::any::TypeId;
use alloc::boxed::Box;
use alloc::vec::Vec;
use alloc::string::String;
use alloc::string::ToString;
use alloc::collections::BTreeMap;

use crate::version::Version;
use crate::error::{HotReloadError, Result};

/// Snapshot of a hot-reloadable object's state
#[derive(Clone)]
pub struct HotReloadSnapshot {
    /// Serialized state data
    pub data: Vec<u8>,
    /// Type of the snapshotted object
    pub type_id: TypeId,
    /// Type name for debugging
    pub type_name: String,
    /// Version when snapshot was taken
    pub version: Version,
    /// Custom metadata
    pub metadata: BTreeMap<String, String>,
}

impl HotReloadSnapshot {
    /// Create a new snapshot
    pub fn new<T: 'static>(data: Vec<u8>, version: Version) -> Self {
        Self {
            data,
            type_id: TypeId::of::<T>(),
            type_name: core::any::type_name::<T>().into(),
            version,
            metadata: BTreeMap::new(),
        }
    }

    /// Add metadata to the snapshot
    pub fn with_metadata(mut self, key: &str, value: &str) -> Self {
        self.metadata.insert(key.into(), value.into());
        self
    }

    /// Check if this snapshot is for a specific type
    pub fn is_type<T: 'static>(&self) -> bool {
        self.type_id == TypeId::of::<T>()
    }
}

/// Trait for types that can be hot-reloaded
pub trait HotReloadable: Send + Sync {
    /// Create a snapshot of the current state
    fn snapshot(&self) -> Result<HotReloadSnapshot>;

    /// Restore from a snapshot
    fn restore(&mut self, snapshot: HotReloadSnapshot) -> Result<()>;

    /// Check if compatible with a new version
    fn is_compatible(&self, new_version: &Version) -> bool;

    /// Called before reload to prepare
    fn prepare_reload(&mut self) -> Result<()> {
        Ok(())
    }

    /// Called after reload completed
    fn finish_reload(&mut self) -> Result<()> {
        Ok(())
    }
}

/// Events that can trigger hot-reload
#[derive(Clone, Debug)]
pub enum ReloadEvent {
    /// A file was modified
    FileModified { path: String },
    /// A file was created
    FileCreated { path: String },
    /// A file was deleted
    FileDeleted { path: String },
    /// Manual reload request
    ManualRequest { target: String },
    /// Plugin reload
    PluginReload { plugin_id: String },
}

/// Callback for reload events
pub type ReloadCallback = Box<dyn Fn(&ReloadEvent) + Send + Sync>;

/// Central manager for hot-reload operations
pub struct HotReloadManager {
    /// Registered reloadable objects
    reloadables: BTreeMap<String, Box<dyn HotReloadable>>,
    /// Pending reload events
    pending: Vec<ReloadEvent>,
    /// Event callbacks
    callbacks: Vec<ReloadCallback>,
    /// Snapshots in progress
    snapshots: BTreeMap<String, HotReloadSnapshot>,
    /// Whether hot-reload is enabled
    enabled: bool,
}

impl HotReloadManager {
    /// Create a new hot-reload manager
    pub fn new() -> Self {
        Self {
            reloadables: BTreeMap::new(),
            pending: Vec::new(),
            callbacks: Vec::new(),
            snapshots: BTreeMap::new(),
            enabled: true,
        }
    }

    /// Enable or disable hot-reload
    pub fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    /// Check if hot-reload is enabled
    pub fn is_enabled(&self) -> bool {
        self.enabled
    }

    /// Register a reloadable object
    pub fn register(&mut self, name: &str, obj: Box<dyn HotReloadable>) {
        self.reloadables.insert(name.into(), obj);
    }

    /// Unregister a reloadable object
    pub fn unregister(&mut self, name: &str) -> Option<Box<dyn HotReloadable>> {
        self.reloadables.remove(name)
    }

    /// Add a reload event callback
    pub fn on_reload(&mut self, callback: ReloadCallback) {
        self.callbacks.push(callback);
    }

    /// Queue a reload event
    pub fn queue_event(&mut self, event: ReloadEvent) {
        if self.enabled {
            self.pending.push(event);
        }
    }

    /// Process pending reload events
    pub fn process_pending(&mut self) -> Vec<Result<()>> {
        if !self.enabled {
            return Vec::new();
        }

        let events = core::mem::take(&mut self.pending);
        let mut results = Vec::new();

        for event in events {
            // Notify callbacks
            for callback in &self.callbacks {
                callback(&event);
            }

            // Process based on event type
            match &event {
                ReloadEvent::FileModified { path } | ReloadEvent::FileCreated { path } => {
                    results.push(self.handle_file_change(path));
                }
                ReloadEvent::ManualRequest { target } => {
                    results.push(self.reload(target));
                }
                ReloadEvent::PluginReload { plugin_id } => {
                    results.push(self.reload(plugin_id));
                }
                _ => {}
            }
        }

        results
    }

    /// Reload a specific object
    pub fn reload(&mut self, name: &str) -> Result<()> {
        let obj = self.reloadables.get_mut(name)
            .ok_or_else(|| HotReloadError::RestoreFailed(
                format!("Object not found: {}", name).into()
            ))?;

        // Prepare for reload
        obj.prepare_reload()?;

        // Take snapshot
        let snapshot = obj.snapshot()?;
        self.snapshots.insert(name.into(), snapshot);

        Ok(())
    }

    /// Complete a reload by restoring from snapshot
    pub fn complete_reload(&mut self, name: &str, new_obj: Box<dyn HotReloadable>) -> Result<()> {
        let snapshot = self.snapshots.remove(name)
            .ok_or_else(|| HotReloadError::RestoreFailed(
                format!("No snapshot for: {}", name).into()
            ))?;

        let mut obj = new_obj;

        // Check compatibility
        if !obj.is_compatible(&snapshot.version) {
            return Err(HotReloadError::IncompatibleVersion {
                old: snapshot.version.to_string().into(),
                new: "unknown".into(),
            }.into());
        }

        // Restore state
        obj.restore(snapshot)?;

        // Finish reload
        obj.finish_reload()?;

        // Replace the old object
        self.reloadables.insert(name.into(), obj);

        Ok(())
    }

    /// Handle a file change event
    fn handle_file_change(&mut self, _path: &str) -> Result<()> {
        // This would be implemented to map file paths to reloadable objects
        // For now, just return Ok
        Ok(())
    }

    /// Get the number of registered reloadables
    pub fn len(&self) -> usize {
        self.reloadables.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.reloadables.is_empty()
    }

    /// Get pending event count
    pub fn pending_count(&self) -> usize {
        self.pending.len()
    }
}

impl Default for HotReloadManager {
    fn default() -> Self {
        Self::new()
    }
}

/// Watcher trait for file system changes
pub trait FileWatcher: Send + Sync {
    /// Start watching a path
    fn watch(&mut self, path: &str) -> Result<()>;

    /// Stop watching a path
    fn unwatch(&mut self, path: &str) -> Result<()>;

    /// Poll for changes
    fn poll(&mut self) -> Vec<ReloadEvent>;

    /// Check if currently watching a path
    fn is_watching(&self, path: &str) -> bool;
}

/// Simple in-memory file watcher for testing
pub struct MemoryFileWatcher {
    watching: Vec<String>,
    pending: Vec<ReloadEvent>,
}

impl MemoryFileWatcher {
    /// Create a new memory file watcher
    pub fn new() -> Self {
        Self {
            watching: Vec::new(),
            pending: Vec::new(),
        }
    }

    /// Simulate a file modification
    pub fn simulate_modify(&mut self, path: &str) {
        if self.is_watching(path) {
            self.pending.push(ReloadEvent::FileModified { path: path.into() });
        }
    }
}

impl FileWatcher for MemoryFileWatcher {
    fn watch(&mut self, path: &str) -> Result<()> {
        if !self.watching.contains(&path.to_string()) {
            self.watching.push(path.into());
        }
        Ok(())
    }

    fn unwatch(&mut self, path: &str) -> Result<()> {
        self.watching.retain(|p| p != path);
        Ok(())
    }

    fn poll(&mut self) -> Vec<ReloadEvent> {
        core::mem::take(&mut self.pending)
    }

    fn is_watching(&self, path: &str) -> bool {
        self.watching.iter().any(|p| p == path)
    }
}

impl Default for MemoryFileWatcher {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestReloadable {
        value: i32,
    }

    impl HotReloadable for TestReloadable {
        fn snapshot(&self) -> Result<HotReloadSnapshot> {
            Ok(HotReloadSnapshot::new::<Self>(
                self.value.to_le_bytes().to_vec(),
                Version::new(1, 0, 0),
            ))
        }

        fn restore(&mut self, snapshot: HotReloadSnapshot) -> Result<()> {
            if snapshot.data.len() >= 4 {
                let bytes: [u8; 4] = snapshot.data[..4].try_into().unwrap();
                self.value = i32::from_le_bytes(bytes);
            }
            Ok(())
        }

        fn is_compatible(&self, new_version: &Version) -> bool {
            new_version.major == 1
        }
    }

    #[test]
    fn test_hot_reload_snapshot() {
        let obj = TestReloadable { value: 42 };
        let snapshot = obj.snapshot().unwrap();

        assert!(snapshot.is_type::<TestReloadable>());
        assert_eq!(snapshot.data, 42i32.to_le_bytes().to_vec());
    }

    #[test]
    fn test_hot_reload_restore() {
        let mut obj = TestReloadable { value: 0 };
        let snapshot = HotReloadSnapshot::new::<TestReloadable>(
            42i32.to_le_bytes().to_vec(),
            Version::new(1, 0, 0),
        );

        obj.restore(snapshot).unwrap();
        assert_eq!(obj.value, 42);
    }
}
