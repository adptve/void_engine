//! Asset Handle - Reference to loaded assets
//!
//! Handles provide safe access to assets with:
//! - Reference counting
//! - Hot-reload notification
//! - Weak references

use core::marker::PhantomData;
use core::sync::atomic::{AtomicU32, AtomicUsize, Ordering};
use core::hash::{Hash, Hasher};
use alloc::sync::Arc;

/// Unique identifier for an asset
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct AssetId(pub u64);

impl AssetId {
    /// Create a new asset ID
    pub const fn new(id: u64) -> Self {
        Self(id)
    }

    /// Invalid asset ID
    pub const fn invalid() -> Self {
        Self(u64::MAX)
    }

    /// Check if valid
    pub const fn is_valid(&self) -> bool {
        self.0 != u64::MAX
    }

    /// Get raw ID value
    pub const fn id(&self) -> u64 {
        self.0
    }
}

impl Default for AssetId {
    fn default() -> Self {
        Self::invalid()
    }
}

/// Load state for an asset
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum LoadState {
    /// Asset is not loaded
    NotLoaded = 0,
    /// Asset is currently loading
    Loading = 1,
    /// Asset is loaded and ready
    Loaded = 2,
    /// Asset failed to load
    Failed = 3,
    /// Asset is being reloaded
    Reloading = 4,
}

impl From<u8> for LoadState {
    fn from(v: u8) -> Self {
        match v {
            0 => Self::NotLoaded,
            1 => Self::Loading,
            2 => Self::Loaded,
            3 => Self::Failed,
            4 => Self::Reloading,
            _ => Self::NotLoaded,
        }
    }
}

/// Internal handle data
pub struct HandleData {
    /// Asset ID
    pub id: AssetId,
    /// Load state
    state: AtomicU32,
    /// Reference count
    ref_count: AtomicUsize,
    /// Reload generation (incremented on each reload)
    generation: AtomicU32,
}

impl HandleData {
    /// Create new handle data
    pub fn new(id: AssetId) -> Self {
        Self {
            id,
            state: AtomicU32::new(LoadState::NotLoaded as u32),
            ref_count: AtomicUsize::new(0),
            generation: AtomicU32::new(0),
        }
    }

    /// Get current load state
    pub fn state(&self) -> LoadState {
        LoadState::from(self.state.load(Ordering::Acquire) as u8)
    }

    /// Set load state
    pub fn set_state(&self, state: LoadState) {
        self.state.store(state as u32, Ordering::Release);
    }

    /// Get reference count
    pub fn ref_count(&self) -> usize {
        self.ref_count.load(Ordering::Relaxed)
    }

    /// Increment reference count
    pub fn add_ref(&self) -> usize {
        self.ref_count.fetch_add(1, Ordering::Relaxed) + 1
    }

    /// Decrement reference count
    pub fn release(&self) -> usize {
        self.ref_count.fetch_sub(1, Ordering::Relaxed) - 1
    }

    /// Get generation
    pub fn generation(&self) -> u32 {
        self.generation.load(Ordering::Acquire)
    }

    /// Increment generation (on reload)
    pub fn increment_generation(&self) -> u32 {
        self.generation.fetch_add(1, Ordering::AcqRel) + 1
    }

    /// Check if loaded
    pub fn is_loaded(&self) -> bool {
        self.state() == LoadState::Loaded
    }
}

/// Strong handle to an asset
///
/// The asset will be kept loaded as long as there are strong handles.
pub struct Handle<T> {
    data: Arc<HandleData>,
    _marker: PhantomData<T>,
}

impl<T> Handle<T> {
    /// Create a new handle
    pub fn new(id: AssetId) -> Self {
        let data = Arc::new(HandleData::new(id));
        data.add_ref();
        Self {
            data,
            _marker: PhantomData,
        }
    }

    /// Create from existing handle data
    pub fn from_data(data: Arc<HandleData>) -> Self {
        data.add_ref();
        Self {
            data,
            _marker: PhantomData,
        }
    }

    /// Get the asset ID
    pub fn id(&self) -> AssetId {
        self.data.id
    }

    /// Get the load state
    pub fn state(&self) -> LoadState {
        self.data.state()
    }

    /// Check if the asset is loaded
    pub fn is_loaded(&self) -> bool {
        self.data.is_loaded()
    }

    /// Get the generation (for detecting reloads)
    pub fn generation(&self) -> u32 {
        self.data.generation()
    }

    /// Create a weak handle
    pub fn downgrade(&self) -> WeakHandle<T> {
        WeakHandle {
            data: Arc::downgrade(&self.data),
            _marker: PhantomData,
        }
    }

    /// Get the underlying handle data
    pub fn data(&self) -> &Arc<HandleData> {
        &self.data
    }

    /// Cast to an untyped handle
    pub fn untyped(self) -> UntypedHandle {
        UntypedHandle {
            data: self.data.clone(),
        }
    }
}

impl<T> Clone for Handle<T> {
    fn clone(&self) -> Self {
        self.data.add_ref();
        Self {
            data: self.data.clone(),
            _marker: PhantomData,
        }
    }
}

impl<T> Drop for Handle<T> {
    fn drop(&mut self) {
        self.data.release();
    }
}

impl<T> PartialEq for Handle<T> {
    fn eq(&self, other: &Self) -> bool {
        self.data.id == other.data.id
    }
}

impl<T> Eq for Handle<T> {}

impl<T> Hash for Handle<T> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.data.id.hash(state);
    }
}

impl<T> core::fmt::Debug for Handle<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("Handle")
            .field("id", &self.data.id)
            .field("state", &self.state())
            .field("generation", &self.generation())
            .finish()
    }
}

/// Weak handle to an asset
///
/// Does not keep the asset loaded. Can be upgraded to a strong handle.
pub struct WeakHandle<T> {
    data: alloc::sync::Weak<HandleData>,
    _marker: PhantomData<T>,
}

impl<T> WeakHandle<T> {
    /// Try to upgrade to a strong handle
    pub fn upgrade(&self) -> Option<Handle<T>> {
        self.data.upgrade().map(|data| {
            data.add_ref();
            Handle {
                data,
                _marker: PhantomData,
            }
        })
    }

    /// Check if the asset is still alive
    pub fn is_alive(&self) -> bool {
        self.data.strong_count() > 0
    }
}

impl<T> Clone for WeakHandle<T> {
    fn clone(&self) -> Self {
        Self {
            data: self.data.clone(),
            _marker: PhantomData,
        }
    }
}

impl<T> Default for WeakHandle<T> {
    fn default() -> Self {
        Self {
            data: alloc::sync::Weak::new(),
            _marker: PhantomData,
        }
    }
}

/// Untyped handle for type-erased asset storage
pub struct UntypedHandle {
    data: Arc<HandleData>,
}

impl UntypedHandle {
    /// Get the asset ID
    pub fn id(&self) -> AssetId {
        self.data.id
    }

    /// Get the load state
    pub fn state(&self) -> LoadState {
        self.data.state()
    }

    /// Check if loaded
    pub fn is_loaded(&self) -> bool {
        self.data.is_loaded()
    }

    /// Get generation
    pub fn generation(&self) -> u32 {
        self.data.generation()
    }

    /// Cast to a typed handle
    ///
    /// # Safety
    /// The caller must ensure the asset is actually of type T
    pub unsafe fn typed<T>(self) -> Handle<T> {
        self.data.add_ref();
        Handle {
            data: self.data.clone(),
            _marker: PhantomData,
        }
    }

    /// Get handle data
    pub fn data(&self) -> &Arc<HandleData> {
        &self.data
    }
}

impl Clone for UntypedHandle {
    fn clone(&self) -> Self {
        self.data.add_ref();
        Self {
            data: self.data.clone(),
        }
    }
}

impl Drop for UntypedHandle {
    fn drop(&mut self) {
        self.data.release();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestAsset;

    #[test]
    fn test_handle_basic() {
        let handle: Handle<TestAsset> = Handle::new(AssetId::new(42));

        assert_eq!(handle.id(), AssetId::new(42));
        assert_eq!(handle.state(), LoadState::NotLoaded);
        assert!(!handle.is_loaded());
    }

    #[test]
    fn test_handle_clone() {
        let handle1: Handle<TestAsset> = Handle::new(AssetId::new(42));
        let handle2 = handle1.clone();

        assert_eq!(handle1.id(), handle2.id());
        assert_eq!(handle1.data().ref_count(), 2);

        drop(handle2);
        assert_eq!(handle1.data().ref_count(), 1);
    }

    #[test]
    fn test_weak_handle() {
        let handle: Handle<TestAsset> = Handle::new(AssetId::new(42));
        let weak = handle.downgrade();

        assert!(weak.is_alive());
        assert!(weak.upgrade().is_some());

        drop(handle);
        assert!(!weak.is_alive());
        assert!(weak.upgrade().is_none());
    }
}
