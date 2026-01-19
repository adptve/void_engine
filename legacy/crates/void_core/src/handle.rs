//! Type-safe handles for resource management
//!
//! Handles provide a safe way to reference resources without direct pointers.
//! They use generational indices to detect use-after-free scenarios.

use core::marker::PhantomData;
use core::sync::atomic::{AtomicU32, Ordering};
use core::hash::{Hash, Hasher};
use core::fmt;
use alloc::vec::Vec;

/// A type-safe handle to a resource of type T
#[repr(transparent)]
pub struct Handle<T> {
    /// Lower 24 bits: index, Upper 8 bits: generation
    bits: u32,
    _marker: PhantomData<*const T>,
}

impl<T> Handle<T> {
    /// Maximum index value (24 bits)
    pub const MAX_INDEX: u32 = (1 << 24) - 1;
    /// Maximum generation value (8 bits)
    pub const MAX_GENERATION: u8 = u8::MAX;

    /// Create a new handle from index and generation
    #[inline]
    pub const fn new(index: u32, generation: u8) -> Self {
        debug_assert!(index <= Self::MAX_INDEX);
        Self {
            bits: (generation as u32) << 24 | index,
            _marker: PhantomData,
        }
    }

    /// Create an invalid/null handle
    #[inline]
    pub const fn null() -> Self {
        Self {
            bits: u32::MAX,
            _marker: PhantomData,
        }
    }

    /// Check if this handle is null
    #[inline]
    pub const fn is_null(&self) -> bool {
        self.bits == u32::MAX
    }

    /// Get the index portion
    #[inline]
    pub const fn index(&self) -> u32 {
        self.bits & Self::MAX_INDEX
    }

    /// Get the generation portion
    #[inline]
    pub const fn generation(&self) -> u8 {
        (self.bits >> 24) as u8
    }

    /// Convert to raw bits for serialization
    #[inline]
    pub const fn to_bits(&self) -> u32 {
        self.bits
    }

    /// Create from raw bits
    #[inline]
    pub const fn from_bits(bits: u32) -> Self {
        Self {
            bits,
            _marker: PhantomData,
        }
    }

    /// Cast to a handle of a different type (unsafe - you must ensure type compatibility)
    #[inline]
    pub const fn cast<U>(self) -> Handle<U> {
        Handle {
            bits: self.bits,
            _marker: PhantomData,
        }
    }
}

// Manual trait implementations to avoid T bounds
impl<T> Clone for Handle<T> {
    #[inline]
    fn clone(&self) -> Self {
        *self
    }
}

impl<T> Copy for Handle<T> {}

impl<T> PartialEq for Handle<T> {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.bits == other.bits
    }
}

impl<T> Eq for Handle<T> {}

impl<T> Hash for Handle<T> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.bits.hash(state);
    }
}

impl<T> fmt::Debug for Handle<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.is_null() {
            write!(f, "Handle<{}>(null)", core::any::type_name::<T>())
        } else {
            write!(f, "Handle<{}>({}v{})",
                core::any::type_name::<T>(),
                self.index(),
                self.generation()
            )
        }
    }
}

impl<T> Default for Handle<T> {
    fn default() -> Self {
        Self::null()
    }
}

/// Allocates handles with proper generation tracking
pub struct HandleAllocator<T> {
    /// Generations for each slot
    generations: Vec<u8>,
    /// Free list of available indices
    free_list: Vec<u32>,
    /// Next fresh index if free list is empty
    next_fresh: AtomicU32,
    _marker: PhantomData<T>,
}

impl<T> HandleAllocator<T> {
    /// Create a new handle allocator with initial capacity
    pub fn new() -> Self {
        Self::with_capacity(256)
    }

    /// Create with specific initial capacity
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            generations: Vec::with_capacity(capacity),
            free_list: Vec::with_capacity(capacity / 4),
            next_fresh: AtomicU32::new(0),
            _marker: PhantomData,
        }
    }

    /// Allocate a new handle
    pub fn allocate(&mut self) -> Handle<T> {
        if let Some(index) = self.free_list.pop() {
            let gen = self.generations[index as usize];
            Handle::new(index, gen)
        } else {
            let index = self.next_fresh.fetch_add(1, Ordering::Relaxed);
            if index > Handle::<T>::MAX_INDEX {
                panic!("Handle allocator exhausted");
            }
            self.generations.push(0);
            Handle::new(index, 0)
        }
    }

    /// Free a handle, making its index available for reuse
    pub fn free(&mut self, handle: Handle<T>) -> bool {
        let index = handle.index() as usize;
        if index >= self.generations.len() {
            return false;
        }

        let gen = &mut self.generations[index];
        if *gen != handle.generation() {
            return false; // Already freed or wrong generation
        }

        // Increment generation (wrapping)
        *gen = gen.wrapping_add(1);
        self.free_list.push(handle.index());
        true
    }

    /// Check if a handle is still valid
    pub fn is_valid(&self, handle: Handle<T>) -> bool {
        if handle.is_null() {
            return false;
        }
        let index = handle.index() as usize;
        if index >= self.generations.len() {
            return false;
        }
        self.generations[index] == handle.generation()
    }

    /// Get the number of allocated handles
    pub fn len(&self) -> usize {
        self.generations.len() - self.free_list.len()
    }

    /// Check if no handles are allocated
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Get the total capacity (including freed slots)
    pub fn capacity(&self) -> usize {
        self.generations.len()
    }
}

impl<T> Default for HandleAllocator<T> {
    fn default() -> Self {
        Self::new()
    }
}

/// A handle map that stores values associated with handles
pub struct HandleMap<T> {
    allocator: HandleAllocator<T>,
    values: Vec<Option<T>>,
}

impl<T> HandleMap<T> {
    /// Create a new handle map
    pub fn new() -> Self {
        Self {
            allocator: HandleAllocator::new(),
            values: Vec::new(),
        }
    }

    /// Insert a value and get a handle to it
    pub fn insert(&mut self, value: T) -> Handle<T> {
        let handle = self.allocator.allocate();
        let index = handle.index() as usize;

        if index >= self.values.len() {
            self.values.resize_with(index + 1, || None);
        }
        self.values[index] = Some(value);
        handle
    }

    /// Remove a value by its handle
    pub fn remove(&mut self, handle: Handle<T>) -> Option<T> {
        if !self.allocator.is_valid(handle) {
            return None;
        }

        self.allocator.free(handle);
        self.values[handle.index() as usize].take()
    }

    /// Get a reference to a value by its handle
    pub fn get(&self, handle: Handle<T>) -> Option<&T> {
        if !self.allocator.is_valid(handle) {
            return None;
        }
        self.values.get(handle.index() as usize)?.as_ref()
    }

    /// Get a mutable reference to a value by its handle
    pub fn get_mut(&mut self, handle: Handle<T>) -> Option<&mut T> {
        if !self.allocator.is_valid(handle) {
            return None;
        }
        self.values.get_mut(handle.index() as usize)?.as_mut()
    }

    /// Check if a handle is valid
    pub fn contains(&self, handle: Handle<T>) -> bool {
        self.allocator.is_valid(handle)
    }

    /// Get the number of values
    pub fn len(&self) -> usize {
        self.allocator.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.allocator.is_empty()
    }

    /// Iterate over all valid handles and values
    pub fn iter(&self) -> impl Iterator<Item = (Handle<T>, &T)> {
        self.values
            .iter()
            .enumerate()
            .filter_map(move |(i, opt)| {
                opt.as_ref().map(|v| {
                    let gen = self.allocator.generations[i];
                    (Handle::new(i as u32, gen), v)
                })
            })
    }

    /// Iterate over all valid handles and mutable values
    pub fn iter_mut(&mut self) -> impl Iterator<Item = (Handle<T>, &mut T)> {
        let gens = &self.allocator.generations;
        self.values
            .iter_mut()
            .enumerate()
            .filter_map(move |(i, opt)| {
                opt.as_mut().map(|v| {
                    let gen = gens[i];
                    (Handle::new(i as u32, gen), v)
                })
            })
    }
}

impl<T> Default for HandleMap<T> {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_handle_allocation() {
        let mut alloc: HandleAllocator<i32> = HandleAllocator::new();
        let h1 = alloc.allocate();
        let h2 = alloc.allocate();

        assert!(alloc.is_valid(h1));
        assert!(alloc.is_valid(h2));
        assert_ne!(h1, h2);

        alloc.free(h1);
        assert!(!alloc.is_valid(h1));

        let h3 = alloc.allocate();
        assert_eq!(h3.index(), h1.index());
        assert_ne!(h3.generation(), h1.generation());
    }

    #[test]
    fn test_handle_map() {
        let mut map: HandleMap<String> = HandleMap::new();
        let h1 = map.insert("hello".to_string());
        let h2 = map.insert("world".to_string());

        assert_eq!(map.get(h1), Some(&"hello".to_string()));
        assert_eq!(map.get(h2), Some(&"world".to_string()));

        map.remove(h1);
        assert_eq!(map.get(h1), None);
    }
}
