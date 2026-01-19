//! Pool allocator - fixed-size block allocation

use core::cell::UnsafeCell;
use core::sync::atomic::{AtomicPtr, AtomicUsize, Ordering};
use alloc::vec::Vec;

use crate::{Allocator, align_up};

/// Pool allocator for fixed-size blocks
///
/// Extremely fast for allocating objects of the same size.
/// Uses a lock-free free list for O(1) allocation and deallocation.
pub struct Pool {
    /// Backing memory
    buffer: UnsafeCell<Vec<u8>>,
    /// Block size (including alignment padding)
    block_size: usize,
    /// Number of blocks
    block_count: usize,
    /// Head of free list
    free_head: AtomicPtr<FreeNode>,
    /// Number of allocated blocks
    allocated: AtomicUsize,
}

#[repr(C)]
struct FreeNode {
    next: AtomicPtr<FreeNode>,
}

// Safety: Pool uses atomic operations for thread-safety
unsafe impl Send for Pool {}
unsafe impl Sync for Pool {}

impl Pool {
    /// Create a new pool for objects of the given size
    pub fn new(object_size: usize, object_align: usize, count: usize) -> Self {
        // Block size must be at least as large as FreeNode
        let min_size = core::mem::size_of::<FreeNode>();
        let block_size = align_up(object_size.max(min_size), object_align.max(core::mem::align_of::<FreeNode>()));

        let total_size = block_size * count;
        let mut buffer = Vec::with_capacity(total_size);
        buffer.resize(total_size, 0);

        let pool = Self {
            buffer: UnsafeCell::new(buffer),
            block_size,
            block_count: count,
            free_head: AtomicPtr::new(core::ptr::null_mut()),
            allocated: AtomicUsize::new(0),
        };

        // Initialize free list
        pool.initialize_free_list();

        pool
    }

    /// Create a pool for a specific type
    pub fn for_type<T>(count: usize) -> Self {
        Self::new(
            core::mem::size_of::<T>(),
            core::mem::align_of::<T>(),
            count,
        )
    }

    fn initialize_free_list(&self) {
        let buffer = unsafe { &*self.buffer.get() };
        let base = buffer.as_ptr() as *mut u8;

        let mut prev: *mut FreeNode = core::ptr::null_mut();

        // Build free list from end to start (so allocation returns from start)
        for i in (0..self.block_count).rev() {
            let block_ptr = unsafe { base.add(i * self.block_size) } as *mut FreeNode;
            unsafe {
                (*block_ptr).next = AtomicPtr::new(prev);
            }
            prev = block_ptr;
        }

        self.free_head.store(prev, Ordering::Release);
    }

    /// Allocate a block
    pub fn alloc_block(&self) -> Option<*mut u8> {
        loop {
            let head = self.free_head.load(Ordering::Acquire);
            if head.is_null() {
                return None; // Pool exhausted
            }

            let next = unsafe { (*head).next.load(Ordering::Relaxed) };

            match self.free_head.compare_exchange_weak(
                head,
                next,
                Ordering::AcqRel,
                Ordering::Relaxed,
            ) {
                Ok(_) => {
                    self.allocated.fetch_add(1, Ordering::Relaxed);
                    return Some(head as *mut u8);
                }
                Err(_) => continue, // Retry on contention
            }
        }
    }

    /// Free a block
    ///
    /// # Safety
    /// The pointer must have been allocated by this pool
    pub unsafe fn free_block(&self, ptr: *mut u8) {
        let node = ptr as *mut FreeNode;

        loop {
            let head = self.free_head.load(Ordering::Relaxed);
            (*node).next = AtomicPtr::new(head);

            match self.free_head.compare_exchange_weak(
                head,
                node,
                Ordering::AcqRel,
                Ordering::Relaxed,
            ) {
                Ok(_) => {
                    self.allocated.fetch_sub(1, Ordering::Relaxed);
                    return;
                }
                Err(_) => continue,
            }
        }
    }

    /// Get the block size
    pub fn block_size(&self) -> usize {
        self.block_size
    }

    /// Get the total number of blocks
    pub fn block_count(&self) -> usize {
        self.block_count
    }

    /// Get the number of allocated blocks
    pub fn allocated_count(&self) -> usize {
        self.allocated.load(Ordering::Relaxed)
    }

    /// Get the number of free blocks
    pub fn free_count(&self) -> usize {
        self.block_count - self.allocated_count()
    }
}

impl Allocator for Pool {
    fn allocate(&self, size: usize, align: usize) -> Option<*mut u8> {
        // Check that the requested allocation fits in a block
        let required = align_up(size, align);
        if required > self.block_size {
            return None;
        }

        self.alloc_block()
    }

    unsafe fn deallocate(&self, ptr: *mut u8, _size: usize, _align: usize) {
        self.free_block(ptr);
    }

    fn reset(&mut self) {
        self.allocated.store(0, Ordering::Release);
        self.initialize_free_list();
    }

    fn capacity(&self) -> usize {
        self.block_count * self.block_size
    }

    fn used(&self) -> usize {
        self.allocated_count() * self.block_size
    }
}

/// Type-safe pool wrapper
pub struct TypedPool<T> {
    pool: Pool,
    _marker: core::marker::PhantomData<T>,
}

impl<T> TypedPool<T> {
    /// Create a new typed pool
    pub fn new(count: usize) -> Self {
        Self {
            pool: Pool::for_type::<T>(count),
            _marker: core::marker::PhantomData,
        }
    }

    /// Allocate a new object
    pub fn alloc(&self, value: T) -> Option<&mut T> {
        let ptr = self.pool.alloc_block()? as *mut T;
        unsafe {
            ptr.write(value);
            Some(&mut *ptr)
        }
    }

    /// Free an object
    ///
    /// # Safety
    /// The reference must have been allocated by this pool
    pub unsafe fn free(&self, value: &mut T) {
        core::ptr::drop_in_place(value);
        self.pool.free_block(value as *mut T as *mut u8);
    }

    /// Get statistics
    pub fn stats(&self) -> PoolStats {
        PoolStats {
            block_size: self.pool.block_size(),
            total_blocks: self.pool.block_count(),
            allocated_blocks: self.pool.allocated_count(),
            free_blocks: self.pool.free_count(),
        }
    }
}

/// Pool statistics
#[derive(Clone, Copy, Debug)]
pub struct PoolStats {
    pub block_size: usize,
    pub total_blocks: usize,
    pub allocated_blocks: usize,
    pub free_blocks: usize,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pool_allocation() {
        let pool = Pool::for_type::<u64>(100);

        let ptr1 = pool.alloc_block().unwrap();
        let ptr2 = pool.alloc_block().unwrap();

        assert_ne!(ptr1, ptr2);
        assert_eq!(pool.allocated_count(), 2);

        unsafe {
            pool.free_block(ptr1);
        }
        assert_eq!(pool.allocated_count(), 1);
    }

    #[test]
    fn test_typed_pool() {
        let pool: TypedPool<i32> = TypedPool::new(10);

        let a = pool.alloc(42).unwrap();
        let b = pool.alloc(100).unwrap();

        assert_eq!(*a, 42);
        assert_eq!(*b, 100);

        unsafe {
            pool.free(a);
        }

        assert_eq!(pool.stats().allocated_blocks, 1);
    }
}
