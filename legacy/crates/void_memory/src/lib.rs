//! # void_memory - Custom Memory Allocators
//!
//! High-performance memory allocators for game engine use cases:
//! - Arena: Linear allocation, bulk deallocation
//! - Pool: Fixed-size block allocation
//! - FreeList: General-purpose with fragmentation management
//! - Stack: LIFO allocation

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

pub mod arena;
pub mod pool;
pub mod free_list;
pub mod stack;

#[cfg(feature = "tracking")]
pub mod tracker;

pub use arena::Arena;
pub use pool::Pool;
pub use free_list::FreeList;
pub use stack::StackAllocator;

/// Common trait for all allocators
pub trait Allocator: Send + Sync {
    /// Allocate memory with the given layout
    fn allocate(&self, size: usize, align: usize) -> Option<*mut u8>;

    /// Deallocate memory
    /// # Safety
    /// The pointer must have been allocated by this allocator
    unsafe fn deallocate(&self, ptr: *mut u8, size: usize, align: usize);

    /// Reset the allocator, freeing all allocations
    fn reset(&mut self);

    /// Get the total capacity
    fn capacity(&self) -> usize;

    /// Get the currently used memory
    fn used(&self) -> usize;

    /// Get the available memory
    fn available(&self) -> usize {
        self.capacity() - self.used()
    }
}

/// Align a value up to the given alignment
#[inline]
pub const fn align_up(value: usize, align: usize) -> usize {
    debug_assert!(align.is_power_of_two());
    (value + align - 1) & !(align - 1)
}

/// Align a value down to the given alignment
#[inline]
pub const fn align_down(value: usize, align: usize) -> usize {
    debug_assert!(align.is_power_of_two());
    value & !(align - 1)
}

/// Check if a pointer is aligned
#[inline]
pub fn is_aligned(ptr: *const u8, align: usize) -> bool {
    (ptr as usize) & (align - 1) == 0
}

pub mod prelude {
    pub use crate::{Allocator, Arena, Pool, FreeList, StackAllocator};
    pub use crate::{align_up, align_down, is_aligned};
}
