//! Arena allocator - fast linear allocation with bulk deallocation

use core::cell::UnsafeCell;
use core::sync::atomic::{AtomicUsize, Ordering};
use alloc::vec::Vec;

use crate::{Allocator, align_up};

/// Arena allocator - extremely fast for temporary allocations
///
/// Allocations are served linearly from a contiguous buffer.
/// Individual deallocations are not supported - only bulk reset.
pub struct Arena {
    /// Backing memory
    buffer: UnsafeCell<Vec<u8>>,
    /// Current allocation offset
    offset: AtomicUsize,
    /// Total capacity
    capacity: usize,
}

// Safety: Arena uses atomic offset for thread-safe allocation
unsafe impl Send for Arena {}
unsafe impl Sync for Arena {}

impl Arena {
    /// Create a new arena with the given capacity
    pub fn new(capacity: usize) -> Self {
        let mut buffer = Vec::with_capacity(capacity);
        buffer.resize(capacity, 0);

        Self {
            buffer: UnsafeCell::new(buffer),
            offset: AtomicUsize::new(0),
            capacity,
        }
    }

    /// Create a new arena with the given capacity in KB
    pub fn with_capacity_kb(kb: usize) -> Self {
        Self::new(kb * 1024)
    }

    /// Create a new arena with the given capacity in MB
    pub fn with_capacity_mb(mb: usize) -> Self {
        Self::new(mb * 1024 * 1024)
    }

    /// Allocate aligned memory
    pub fn alloc_aligned<T>(&self, count: usize) -> Option<*mut T> {
        let size = core::mem::size_of::<T>() * count;
        let align = core::mem::align_of::<T>();
        self.allocate(size, align).map(|ptr| ptr as *mut T)
    }

    /// Allocate a single value
    pub fn alloc<T: Copy>(&self, value: T) -> Option<&mut T> {
        let ptr = self.alloc_aligned::<T>(1)?;
        unsafe {
            ptr.write(value);
            Some(&mut *ptr)
        }
    }

    /// Allocate a slice
    pub fn alloc_slice<T: Copy>(&self, values: &[T]) -> Option<&mut [T]> {
        let ptr = self.alloc_aligned::<T>(values.len())?;
        unsafe {
            core::ptr::copy_nonoverlapping(values.as_ptr(), ptr, values.len());
            Some(core::slice::from_raw_parts_mut(ptr, values.len()))
        }
    }

    /// Allocate zeroed memory
    pub fn alloc_zeroed<T>(&self, count: usize) -> Option<&mut [T]> {
        let size = core::mem::size_of::<T>() * count;
        let align = core::mem::align_of::<T>();
        let ptr = self.allocate(size, align)? as *mut T;

        unsafe {
            core::ptr::write_bytes(ptr, 0, count);
            Some(core::slice::from_raw_parts_mut(ptr, count))
        }
    }

    /// Get the current offset
    pub fn current_offset(&self) -> usize {
        self.offset.load(Ordering::Relaxed)
    }

    /// Save the current state for later restore
    pub fn save(&self) -> ArenaState {
        ArenaState {
            offset: self.offset.load(Ordering::Acquire),
        }
    }

    /// Restore to a previously saved state
    pub fn restore(&self, state: ArenaState) {
        self.offset.store(state.offset, Ordering::Release);
    }
}

impl Allocator for Arena {
    fn allocate(&self, size: usize, align: usize) -> Option<*mut u8> {
        if size == 0 {
            return Some(align as *mut u8); // Non-null aligned dangling pointer
        }

        loop {
            let current = self.offset.load(Ordering::Relaxed);

            // Get buffer pointer
            let buffer = unsafe { &*self.buffer.get() };
            let base = buffer.as_ptr() as usize;

            // Calculate aligned offset
            let aligned_offset = align_up(base + current, align) - base;
            let new_offset = aligned_offset + size;

            if new_offset > self.capacity {
                return None; // Out of memory
            }

            // Try to claim this allocation
            match self.offset.compare_exchange_weak(
                current,
                new_offset,
                Ordering::AcqRel,
                Ordering::Relaxed,
            ) {
                Ok(_) => {
                    let ptr = unsafe { buffer.as_ptr().add(aligned_offset) as *mut u8 };
                    return Some(ptr);
                }
                Err(_) => continue, // Retry on contention
            }
        }
    }

    unsafe fn deallocate(&self, _ptr: *mut u8, _size: usize, _align: usize) {
        // Arena doesn't support individual deallocation
    }

    fn reset(&mut self) {
        self.offset.store(0, Ordering::Release);
    }

    fn capacity(&self) -> usize {
        self.capacity
    }

    fn used(&self) -> usize {
        self.offset.load(Ordering::Relaxed)
    }
}

impl Default for Arena {
    fn default() -> Self {
        Self::new(1024 * 1024) // 1 MB default
    }
}

/// Saved arena state for scoped allocations
#[derive(Clone, Copy)]
pub struct ArenaState {
    offset: usize,
}

/// Scoped arena allocation guard
pub struct ArenaScope<'a> {
    arena: &'a Arena,
    state: ArenaState,
}

impl<'a> ArenaScope<'a> {
    /// Create a new scoped arena
    pub fn new(arena: &'a Arena) -> Self {
        Self {
            state: arena.save(),
            arena,
        }
    }

    /// Access the arena for allocations
    pub fn arena(&self) -> &Arena {
        self.arena
    }
}

impl Drop for ArenaScope<'_> {
    fn drop(&mut self) {
        self.arena.restore(self.state);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_arena_allocation() {
        let arena = Arena::new(1024);

        let a = arena.alloc(42i32).unwrap();
        let b = arena.alloc(3.14f32).unwrap();

        assert_eq!(*a, 42);
        assert_eq!(*b, 3.14);
    }

    #[test]
    fn test_arena_slice() {
        let arena = Arena::new(1024);

        let data = [1, 2, 3, 4, 5];
        let slice = arena.alloc_slice(&data).unwrap();

        assert_eq!(slice, &[1, 2, 3, 4, 5]);
    }

    #[test]
    fn test_arena_scope() {
        let arena = Arena::new(1024);

        let initial = arena.used();

        {
            let _scope = ArenaScope::new(&arena);
            arena.alloc(42i32).unwrap();
            arena.alloc(42i32).unwrap();
            assert!(arena.used() > initial);
        }

        // Memory reclaimed after scope
        assert_eq!(arena.used(), initial);
    }
}
