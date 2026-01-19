//! Component - Data attached to entities
//!
//! Components are plain data with no behavior. They can be dynamically
//! registered at runtime by plugins.

use core::any::TypeId;
use core::alloc::Layout;
use alloc::boxed::Box;
use alloc::vec::Vec;
use alloc::string::String;
use alloc::collections::BTreeMap;
use core::ptr::NonNull;

/// Unique identifier for a component type
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ComponentId(pub u32);

impl ComponentId {
    /// Invalid component ID
    pub const INVALID: Self = Self(u32::MAX);

    /// Create a new component ID
    #[inline]
    pub const fn new(id: u32) -> Self {
        Self(id)
    }

    /// Get the raw ID value
    #[inline]
    pub const fn id(&self) -> u32 {
        self.0
    }

    /// Check if this is a valid ID
    #[inline]
    pub const fn is_valid(&self) -> bool {
        self.0 != u32::MAX
    }
}

/// Information about a component type
#[derive(Clone)]
pub struct ComponentInfo {
    /// Unique ID for this component
    pub id: ComponentId,
    /// Type name for debugging
    pub name: String,
    /// Memory layout
    pub layout: Layout,
    /// TypeId for compile-time registered components
    pub type_id: Option<TypeId>,
    /// Drop function (if the component needs dropping)
    pub drop_fn: Option<fn(NonNull<u8>)>,
    /// Clone function (for component cloning)
    pub clone_fn: Option<fn(NonNull<u8>, NonNull<u8>)>,
}

impl ComponentInfo {
    /// Create info for a Rust type
    pub fn of<T: Component>() -> Self {
        Self {
            id: ComponentId::INVALID, // Will be assigned by registry
            name: String::from(core::any::type_name::<T>()),
            layout: Layout::new::<T>(),
            type_id: Some(TypeId::of::<T>()),
            drop_fn: if core::mem::needs_drop::<T>() {
                Some(|ptr| unsafe {
                    core::ptr::drop_in_place(ptr.as_ptr() as *mut T);
                })
            } else {
                None
            },
            clone_fn: None, // Only for Clone components
        }
    }

    /// Create info for a cloneable Rust type
    pub fn of_cloneable<T: Component + Clone>() -> Self {
        let mut info = Self::of::<T>();
        info.clone_fn = Some(|src, dst| unsafe {
            let value = (src.as_ptr() as *const T).read();
            let cloned = value.clone();
            (dst.as_ptr() as *mut T).write(cloned);
            core::mem::forget(value); // Don't drop the original
        });
        info
    }

    /// Get the size in bytes
    #[inline]
    pub fn size(&self) -> usize {
        self.layout.size()
    }

    /// Get the alignment
    #[inline]
    pub fn align(&self) -> usize {
        self.layout.align()
    }
}

/// Trait for component types
pub trait Component: Send + Sync + 'static {
    /// Get component info
    fn component_info() -> ComponentInfo
    where
        Self: Sized,
    {
        ComponentInfo::of::<Self>()
    }
}

// Blanket implementation for all suitable types
impl<T: Send + Sync + 'static> Component for T {}

/// Registry for component types
pub struct ComponentRegistry {
    /// Registered components by ID
    components: Vec<ComponentInfo>,
    /// TypeId to ComponentId mapping
    type_map: BTreeMap<TypeId, ComponentId>,
    /// Name to ComponentId mapping
    name_map: BTreeMap<String, ComponentId>,
}

impl ComponentRegistry {
    /// Create a new component registry
    pub fn new() -> Self {
        Self {
            components: Vec::new(),
            type_map: BTreeMap::new(),
            name_map: BTreeMap::new(),
        }
    }

    /// Register a component type
    pub fn register<T: Component>(&mut self) -> ComponentId {
        let type_id = TypeId::of::<T>();

        // Check if already registered
        if let Some(&id) = self.type_map.get(&type_id) {
            return id;
        }

        let id = ComponentId::new(self.components.len() as u32);
        let mut info = ComponentInfo::of::<T>();
        info.id = id;

        self.type_map.insert(type_id, id);
        self.name_map.insert(info.name.clone(), id);
        self.components.push(info);

        id
    }

    /// Register a component type with clone support
    pub fn register_cloneable<T: Component + Clone>(&mut self) -> ComponentId {
        let type_id = TypeId::of::<T>();

        // Check if already registered
        if let Some(&id) = self.type_map.get(&type_id) {
            return id;
        }

        let id = ComponentId::new(self.components.len() as u32);
        let mut info = ComponentInfo::of_cloneable::<T>();
        info.id = id;

        self.type_map.insert(type_id, id);
        self.name_map.insert(info.name.clone(), id);
        self.components.push(info);

        id
    }

    /// Register a dynamic component (from plugins, scripts, etc.)
    pub fn register_dynamic(&mut self, mut info: ComponentInfo) -> ComponentId {
        // Check if name already registered
        if let Some(&id) = self.name_map.get(&info.name) {
            return id;
        }

        let id = ComponentId::new(self.components.len() as u32);
        info.id = id;

        self.name_map.insert(info.name.clone(), id);
        self.components.push(info);

        id
    }

    /// Get component ID by type
    pub fn get_id<T: Component>(&self) -> Option<ComponentId> {
        self.type_map.get(&TypeId::of::<T>()).copied()
    }

    /// Get component ID by name
    pub fn get_id_by_name(&self, name: &str) -> Option<ComponentId> {
        self.name_map.get(name).copied()
    }

    /// Get component info by ID
    pub fn get_info(&self, id: ComponentId) -> Option<&ComponentInfo> {
        self.components.get(id.0 as usize)
    }

    /// Get all registered components
    pub fn iter(&self) -> impl Iterator<Item = &ComponentInfo> {
        self.components.iter()
    }

    /// Get number of registered components
    pub fn len(&self) -> usize {
        self.components.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.components.is_empty()
    }
}

impl Default for ComponentRegistry {
    fn default() -> Self {
        Self::new()
    }
}

/// Storage for components of a single type
pub struct ComponentStorage {
    /// Component info
    info: ComponentInfo,
    /// Raw storage
    data: Vec<u8>,
    /// Number of components stored
    len: usize,
    /// Capacity in number of components
    capacity: usize,
}

impl ComponentStorage {
    /// Create new storage for a component type
    pub fn new(info: ComponentInfo) -> Self {
        Self {
            info,
            data: Vec::new(),
            len: 0,
            capacity: 0,
        }
    }

    /// Create with initial capacity
    pub fn with_capacity(info: ComponentInfo, capacity: usize) -> Self {
        let byte_capacity = capacity * info.layout.size();
        Self {
            data: Vec::with_capacity(byte_capacity),
            info,
            len: 0,
            capacity,
        }
    }

    /// Get the component info
    #[inline]
    pub fn info(&self) -> &ComponentInfo {
        &self.info
    }

    /// Get number of components
    #[inline]
    pub fn len(&self) -> usize {
        self.len
    }

    /// Check if empty
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Get capacity
    #[inline]
    pub fn capacity(&self) -> usize {
        self.capacity
    }

    /// Reserve space for additional components
    pub fn reserve(&mut self, additional: usize) {
        let new_capacity = self.len + additional;
        if new_capacity > self.capacity {
            let byte_capacity = new_capacity * self.info.layout.size();
            self.data.reserve(byte_capacity - self.data.len());
            self.capacity = new_capacity;
        }
    }

    /// Push a component (type-erased)
    ///
    /// # Safety
    /// The data must be valid for the component type
    pub unsafe fn push_raw(&mut self, data: NonNull<u8>) {
        if self.len >= self.capacity {
            self.reserve(self.capacity.max(1));
        }

        let size = self.info.layout.size();
        let offset = self.len * size;

        // Extend data vector if needed
        if offset + size > self.data.len() {
            self.data.resize(offset + size, 0);
        }

        core::ptr::copy_nonoverlapping(
            data.as_ptr(),
            self.data.as_mut_ptr().add(offset),
            size,
        );

        self.len += 1;
    }

    /// Push a typed component
    pub fn push<T: Component>(&mut self, value: T) {
        debug_assert_eq!(
            self.info.type_id,
            Some(TypeId::of::<T>()),
            "Component type mismatch"
        );

        if self.len >= self.capacity {
            self.reserve(self.capacity.max(1));
        }

        let size = self.info.layout.size();
        let offset = self.len * size;

        // Extend data vector if needed
        if offset + size > self.data.len() {
            self.data.resize(offset + size, 0);
        }

        unsafe {
            core::ptr::write(
                self.data.as_mut_ptr().add(offset) as *mut T,
                value,
            );
        }

        self.len += 1;
    }

    /// Get a component by index
    ///
    /// # Safety
    /// Index must be valid and T must match the component type
    #[inline]
    pub unsafe fn get<T: Component>(&self, index: usize) -> &T {
        debug_assert!(index < self.len);
        let offset = index * self.info.layout.size();
        &*(self.data.as_ptr().add(offset) as *const T)
    }

    /// Get a component mutably by index
    ///
    /// # Safety
    /// Index must be valid and T must match the component type
    #[inline]
    pub unsafe fn get_mut<T: Component>(&mut self, index: usize) -> &mut T {
        debug_assert!(index < self.len);
        let offset = index * self.info.layout.size();
        &mut *(self.data.as_mut_ptr().add(offset) as *mut T)
    }

    /// Get raw pointer to component at index
    #[inline]
    pub fn get_raw(&self, index: usize) -> Option<NonNull<u8>> {
        if index >= self.len {
            return None;
        }
        let offset = index * self.info.layout.size();
        NonNull::new(unsafe { self.data.as_ptr().add(offset) as *mut u8 })
    }

    /// Swap-remove a component by index
    pub fn swap_remove(&mut self, index: usize) {
        if index >= self.len {
            return;
        }

        let size = self.info.layout.size();
        let last_index = self.len - 1;

        // Drop the removed component
        if let Some(drop_fn) = self.info.drop_fn {
            let ptr = unsafe {
                NonNull::new_unchecked(self.data.as_mut_ptr().add(index * size))
            };
            drop_fn(ptr);
        }

        // Swap with last if not already last
        if index != last_index {
            unsafe {
                core::ptr::copy_nonoverlapping(
                    self.data.as_ptr().add(last_index * size),
                    self.data.as_mut_ptr().add(index * size),
                    size,
                );
            }
        }

        self.len -= 1;
    }

    /// Swap-remove a component by index WITHOUT dropping (for moves)
    ///
    /// Used when the component data has been moved elsewhere and shouldn't be dropped.
    pub fn swap_remove_no_drop(&mut self, index: usize) {
        if index >= self.len {
            return;
        }

        let size = self.info.layout.size();
        let last_index = self.len - 1;

        // NO drop - the component has been moved

        // Swap with last if not already last
        if index != last_index {
            unsafe {
                core::ptr::copy_nonoverlapping(
                    self.data.as_ptr().add(last_index * size),
                    self.data.as_mut_ptr().add(index * size),
                    size,
                );
            }
        }

        self.len -= 1;
    }

    /// Clear all components
    pub fn clear(&mut self) {
        // Drop all components
        if let Some(drop_fn) = self.info.drop_fn {
            let size = self.info.layout.size();
            for i in 0..self.len {
                let ptr = unsafe {
                    NonNull::new_unchecked(self.data.as_mut_ptr().add(i * size))
                };
                drop_fn(ptr);
            }
        }

        self.len = 0;
    }

    /// Get a slice of all component data
    #[inline]
    pub fn as_slice<T: Component>(&self) -> &[T] {
        debug_assert_eq!(
            self.info.type_id,
            Some(TypeId::of::<T>()),
            "Component type mismatch"
        );
        unsafe {
            core::slice::from_raw_parts(self.data.as_ptr() as *const T, self.len)
        }
    }

    /// Get a mutable slice of all component data
    #[inline]
    pub fn as_mut_slice<T: Component>(&mut self) -> &mut [T] {
        debug_assert_eq!(
            self.info.type_id,
            Some(TypeId::of::<T>()),
            "Component type mismatch"
        );
        unsafe {
            core::slice::from_raw_parts_mut(self.data.as_mut_ptr() as *mut T, self.len)
        }
    }
}

impl Drop for ComponentStorage {
    fn drop(&mut self) {
        self.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Debug, Clone, PartialEq)]
    struct Position {
        x: f32,
        y: f32,
    }

    #[derive(Debug, Clone, PartialEq)]
    struct Velocity {
        x: f32,
        y: f32,
    }

    #[test]
    fn test_component_registry() {
        let mut registry = ComponentRegistry::new();

        let pos_id = registry.register::<Position>();
        let vel_id = registry.register::<Velocity>();

        assert_ne!(pos_id, vel_id);
        assert_eq!(registry.get_id::<Position>(), Some(pos_id));
        assert_eq!(registry.get_id::<Velocity>(), Some(vel_id));
    }

    #[test]
    fn test_component_storage() {
        let mut registry = ComponentRegistry::new();
        let pos_id = registry.register::<Position>();
        let info = registry.get_info(pos_id).unwrap().clone();

        let mut storage = ComponentStorage::new(info);

        storage.push(Position { x: 1.0, y: 2.0 });
        storage.push(Position { x: 3.0, y: 4.0 });

        assert_eq!(storage.len(), 2);

        unsafe {
            assert_eq!(storage.get::<Position>(0), &Position { x: 1.0, y: 2.0 });
            assert_eq!(storage.get::<Position>(1), &Position { x: 3.0, y: 4.0 });
        }
    }

    #[test]
    fn test_component_storage_swap_remove() {
        let mut registry = ComponentRegistry::new();
        let pos_id = registry.register::<Position>();
        let info = registry.get_info(pos_id).unwrap().clone();

        let mut storage = ComponentStorage::new(info);

        storage.push(Position { x: 1.0, y: 1.0 });
        storage.push(Position { x: 2.0, y: 2.0 });
        storage.push(Position { x: 3.0, y: 3.0 });

        storage.swap_remove(0);

        assert_eq!(storage.len(), 2);
        // Position at index 0 should now be the old last element
        unsafe {
            assert_eq!(storage.get::<Position>(0), &Position { x: 3.0, y: 3.0 });
            assert_eq!(storage.get::<Position>(1), &Position { x: 2.0, y: 2.0 });
        }
    }
}
