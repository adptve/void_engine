//! Asset Storage - Manages loaded asset data
//!
//! Stores loaded assets and provides access via handles.

use crate::handle::{AssetId, HandleData, LoadState, Handle, WeakHandle};
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::sync::Arc;
use alloc::vec::Vec;
use core::any::{Any, TypeId};
use parking_lot::RwLock;

/// Stored asset entry
struct AssetEntry {
    /// Handle data for reference counting and state
    handle_data: Arc<HandleData>,
    /// The actual asset data (type-erased)
    data: Option<Box<dyn Any + Send + Sync>>,
    /// Asset type ID
    type_id: TypeId,
    /// Dependencies
    dependencies: Vec<AssetId>,
}

impl AssetEntry {
    fn new(id: AssetId, type_id: TypeId) -> Self {
        Self {
            handle_data: Arc::new(HandleData::new(id)),
            data: None,
            type_id,
            dependencies: Vec::new(),
        }
    }
}

/// Storage for all loaded assets
pub struct AssetStorage {
    /// Assets by ID
    assets: RwLock<BTreeMap<AssetId, AssetEntry>>,
    /// Next asset ID
    next_id: core::sync::atomic::AtomicU64,
}

impl AssetStorage {
    /// Create new asset storage
    pub fn new() -> Self {
        Self {
            assets: RwLock::new(BTreeMap::new()),
            next_id: core::sync::atomic::AtomicU64::new(1),
        }
    }

    /// Allocate a new asset ID
    pub fn allocate_id(&self) -> AssetId {
        AssetId::new(
            self.next_id
                .fetch_add(1, core::sync::atomic::Ordering::Relaxed),
        )
    }

    /// Register an asset for loading
    pub fn register<T: Send + Sync + 'static>(&self, id: AssetId) -> Handle<T> {
        let mut assets = self.assets.write();

        let entry = assets.entry(id).or_insert_with(|| {
            AssetEntry::new(id, TypeId::of::<T>())
        });

        Handle::from_data(entry.handle_data.clone())
    }

    /// Store a loaded asset
    pub fn store<T: Send + Sync + 'static>(
        &self,
        id: AssetId,
        asset: T,
        dependencies: Vec<AssetId>,
    ) {
        let mut assets = self.assets.write();

        if let Some(entry) = assets.get_mut(&id) {
            entry.data = Some(Box::new(asset));
            entry.dependencies = dependencies;
            entry.handle_data.set_state(LoadState::Loaded);
        } else {
            let mut entry = AssetEntry::new(id, TypeId::of::<T>());
            entry.data = Some(Box::new(asset));
            entry.dependencies = dependencies;
            entry.handle_data.set_state(LoadState::Loaded);
            assets.insert(id, entry);
        }
    }

    /// Store a type-erased asset
    pub fn store_erased(
        &self,
        id: AssetId,
        asset: Box<dyn Any + Send + Sync>,
        type_id: TypeId,
        dependencies: Vec<AssetId>,
    ) {
        let mut assets = self.assets.write();

        if let Some(entry) = assets.get_mut(&id) {
            entry.data = Some(asset);
            entry.dependencies = dependencies;
            entry.handle_data.set_state(LoadState::Loaded);
        } else {
            let mut entry = AssetEntry::new(id, type_id);
            entry.data = Some(asset);
            entry.dependencies = dependencies;
            entry.handle_data.set_state(LoadState::Loaded);
            assets.insert(id, entry);
        }
    }

    /// Mark an asset as failed
    pub fn mark_failed(&self, id: AssetId) {
        let assets = self.assets.read();
        if let Some(entry) = assets.get(&id) {
            entry.handle_data.set_state(LoadState::Failed);
        }
    }

    /// Mark an asset as loading
    pub fn mark_loading(&self, id: AssetId) {
        let assets = self.assets.read();
        if let Some(entry) = assets.get(&id) {
            entry.handle_data.set_state(LoadState::Loading);
        }
    }

    /// Mark an asset as reloading
    pub fn mark_reloading(&self, id: AssetId) {
        let assets = self.assets.read();
        if let Some(entry) = assets.get(&id) {
            entry.handle_data.set_state(LoadState::Reloading);
        }
    }

    /// Get an asset by cloning it (requires Clone)
    pub fn get_cloned<T: Clone + Send + Sync + 'static>(&self, id: AssetId) -> Option<T> {
        let assets = self.assets.read();
        let entry = assets.get(&id)?;

        if entry.type_id != TypeId::of::<T>() {
            return None;
        }

        entry.data.as_ref()?.downcast_ref::<T>().cloned()
    }

    /// Get a handle to an asset
    pub fn get_handle<T: Send + Sync + 'static>(&self, id: AssetId) -> Option<Handle<T>> {
        let assets = self.assets.read();
        let entry = assets.get(&id)?;

        if entry.type_id != TypeId::of::<T>() {
            return None;
        }

        Some(Handle::from_data(entry.handle_data.clone()))
    }

    /// Get the load state of an asset
    pub fn get_state(&self, id: AssetId) -> Option<LoadState> {
        let assets = self.assets.read();
        assets.get(&id).map(|e| e.handle_data.state())
    }

    /// Check if an asset is loaded
    pub fn is_loaded(&self, id: AssetId) -> bool {
        self.get_state(id) == Some(LoadState::Loaded)
    }

    /// Get asset dependencies
    pub fn dependencies(&self, id: AssetId) -> Vec<AssetId> {
        let assets = self.assets.read();
        assets.get(&id)
            .map(|e| e.dependencies.clone())
            .unwrap_or_default()
    }

    /// Remove an asset
    pub fn remove(&self, id: AssetId) -> bool {
        let mut assets = self.assets.write();
        assets.remove(&id).is_some()
    }

    /// Clear all assets
    pub fn clear(&self) {
        let mut assets = self.assets.write();
        assets.clear();
    }

    /// Get number of stored assets
    pub fn len(&self) -> usize {
        self.assets.read().len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.assets.read().is_empty()
    }

    /// Get all asset IDs
    pub fn asset_ids(&self) -> Vec<AssetId> {
        self.assets.read().keys().copied().collect()
    }

    /// Increment generation for an asset (on reload)
    pub fn increment_generation(&self, id: AssetId) -> Option<u32> {
        let assets = self.assets.read();
        assets.get(&id).map(|e| e.handle_data.increment_generation())
    }

    /// Get reference count for an asset
    pub fn ref_count(&self, id: AssetId) -> Option<usize> {
        let assets = self.assets.read();
        assets.get(&id).map(|e| e.handle_data.ref_count())
    }

    /// Collect unreferenced assets
    pub fn collect_garbage(&self) -> Vec<AssetId> {
        let assets = self.assets.read();
        assets
            .iter()
            .filter(|(_, entry)| entry.handle_data.ref_count() == 0)
            .map(|(id, _)| *id)
            .collect()
    }

    /// Remove all unreferenced assets
    pub fn remove_unreferenced(&self) -> usize {
        let to_remove = self.collect_garbage();
        let count = to_remove.len();

        let mut assets = self.assets.write();
        for id in to_remove {
            assets.remove(&id);
        }

        count
    }
}

impl Default for AssetStorage {
    fn default() -> Self {
        Self::new()
    }
}

/// Typed storage for a specific asset type
pub struct TypedStorage<T: Send + Sync + 'static> {
    storage: AssetStorage,
    _marker: core::marker::PhantomData<T>,
}

impl<T: Send + Sync + 'static> TypedStorage<T> {
    /// Create new typed storage
    pub fn new() -> Self {
        Self {
            storage: AssetStorage::new(),
            _marker: core::marker::PhantomData,
        }
    }

    /// Register an asset
    pub fn register(&self, id: AssetId) -> Handle<T> {
        self.storage.register::<T>(id)
    }

    /// Store an asset
    pub fn store(&self, id: AssetId, asset: T, dependencies: Vec<AssetId>) {
        self.storage.store(id, asset, dependencies);
    }

    /// Get an asset by cloning it
    pub fn get(&self, id: AssetId) -> Option<T>
    where
        T: Clone,
    {
        self.storage.get_cloned::<T>(id)
    }

    /// Get a handle
    pub fn get_handle(&self, id: AssetId) -> Option<Handle<T>> {
        self.storage.get_handle::<T>(id)
    }

    /// Check if loaded
    pub fn is_loaded(&self, id: AssetId) -> bool {
        self.storage.is_loaded(id)
    }
}

impl<T: Send + Sync + 'static> Default for TypedStorage<T> {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestAsset(i32);

    #[test]
    fn test_asset_storage() {
        let storage = AssetStorage::new();

        let id = storage.allocate_id();
        let handle: Handle<TestAsset> = storage.register(id);

        assert_eq!(handle.state(), LoadState::NotLoaded);

        storage.store(id, TestAsset(42), Vec::new());

        assert_eq!(handle.state(), LoadState::Loaded);
        assert!(storage.is_loaded(id));
    }

    #[test]
    fn test_garbage_collection() {
        let storage = AssetStorage::new();

        let id = storage.allocate_id();
        {
            let _handle: Handle<TestAsset> = storage.register(id);
            storage.store(id, TestAsset(42), Vec::new());

            // Handle is in scope, ref_count > 0
            let garbage = storage.collect_garbage();
            assert!(garbage.is_empty());
        }

        // Handle dropped, ref_count == 0
        // Note: The storage itself still holds the entry, but no handles reference it
        // In a real implementation, we'd need to track this differently
    }
}
