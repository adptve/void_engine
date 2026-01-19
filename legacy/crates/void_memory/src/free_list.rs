//! Free list allocator - general purpose with fragmentation management

use core::cell::UnsafeCell;
use alloc::vec::Vec;
use parking_lot::Mutex;

use crate::{Allocator, align_up};

/// Free block header
#[derive(Clone, Copy)]
struct BlockHeader {
    size: usize,
    is_free: bool,
}

/// Free list allocator
///
/// General-purpose allocator that tracks free blocks.
/// Supports variable-size allocations with configurable placement policy.
pub struct FreeList {
    /// Backing memory
    buffer: UnsafeCell<Vec<u8>>,
    /// List of free blocks (offset, size)
    free_blocks: Mutex<Vec<(usize, usize)>>,
    /// Total capacity
    capacity: usize,
    /// Currently used memory
    used: Mutex<usize>,
    /// Placement policy
    policy: PlacementPolicy,
}

/// Placement policy for choosing free blocks
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PlacementPolicy {
    /// First block that fits
    FirstFit,
    /// Best fitting block (smallest that fits)
    BestFit,
    /// Worst fitting block (largest)
    WorstFit,
}

// Safety: FreeList uses mutex for synchronization
unsafe impl Send for FreeList {}
unsafe impl Sync for FreeList {}

impl FreeList {
    /// Header size for each allocation
    const HEADER_SIZE: usize = core::mem::size_of::<BlockHeader>();
    const HEADER_ALIGN: usize = core::mem::align_of::<BlockHeader>();

    /// Create a new free list allocator
    pub fn new(capacity: usize) -> Self {
        Self::with_policy(capacity, PlacementPolicy::FirstFit)
    }

    /// Create with a specific placement policy
    pub fn with_policy(capacity: usize, policy: PlacementPolicy) -> Self {
        let mut buffer = Vec::with_capacity(capacity);
        buffer.resize(capacity, 0);

        let mut free_blocks = Vec::new();
        free_blocks.push((0, capacity));

        Self {
            buffer: UnsafeCell::new(buffer),
            free_blocks: Mutex::new(free_blocks),
            capacity,
            used: Mutex::new(0),
            policy,
        }
    }

    /// Find a suitable free block
    fn find_block(&self, blocks: &[(usize, usize)], required_size: usize) -> Option<usize> {
        match self.policy {
            PlacementPolicy::FirstFit => {
                blocks.iter().position(|(_, size)| *size >= required_size)
            }
            PlacementPolicy::BestFit => {
                let mut best = None;
                let mut best_size = usize::MAX;

                for (i, (_, size)) in blocks.iter().enumerate() {
                    if *size >= required_size && *size < best_size {
                        best = Some(i);
                        best_size = *size;
                    }
                }
                best
            }
            PlacementPolicy::WorstFit => {
                let mut worst = None;
                let mut worst_size = 0;

                for (i, (_, size)) in blocks.iter().enumerate() {
                    if *size >= required_size && *size > worst_size {
                        worst = Some(i);
                        worst_size = *size;
                    }
                }
                worst
            }
        }
    }

    /// Coalesce adjacent free blocks
    fn coalesce(&self, blocks: &mut Vec<(usize, usize)>) {
        if blocks.len() < 2 {
            return;
        }

        blocks.sort_by_key(|(offset, _)| *offset);

        let mut i = 0;
        while i < blocks.len() - 1 {
            let (offset1, size1) = blocks[i];
            let (offset2, size2) = blocks[i + 1];

            if offset1 + size1 == offset2 {
                // Merge blocks
                blocks[i] = (offset1, size1 + size2);
                blocks.remove(i + 1);
            } else {
                i += 1;
            }
        }
    }

    /// Get allocation statistics
    pub fn stats(&self) -> FreeListStats {
        let blocks = self.free_blocks.lock();
        let used = *self.used.lock();

        FreeListStats {
            capacity: self.capacity,
            used,
            free: self.capacity - used,
            free_blocks: blocks.len(),
            largest_free_block: blocks.iter().map(|(_, s)| *s).max().unwrap_or(0),
        }
    }
}

impl Allocator for FreeList {
    fn allocate(&self, size: usize, align: usize) -> Option<*mut u8> {
        if size == 0 {
            return Some(align as *mut u8);
        }

        // Account for header and alignment
        let total_align = align.max(Self::HEADER_ALIGN);
        let total_size = align_up(Self::HEADER_SIZE + size, total_align);

        let mut blocks = self.free_blocks.lock();

        // Find a suitable block
        let block_idx = self.find_block(&blocks, total_size)?;
        let (offset, block_size) = blocks[block_idx];

        // Calculate aligned user pointer
        let buffer = unsafe { &*self.buffer.get() };
        let base = buffer.as_ptr() as usize;
        let header_ptr = base + offset;
        let user_ptr = align_up(header_ptr + Self::HEADER_SIZE, align);
        let actual_size = (user_ptr - header_ptr) + size;

        // Split or remove block
        if block_size > actual_size + Self::HEADER_SIZE + 16 {
            // Split: update this block with remaining size
            blocks[block_idx] = (offset + actual_size, block_size - actual_size);
        } else {
            // Use entire block
            blocks.remove(block_idx);
        }

        // Write header
        let header = BlockHeader {
            size: actual_size,
            is_free: false,
        };
        unsafe {
            core::ptr::write(header_ptr as *mut BlockHeader, header);
        }

        *self.used.lock() += actual_size;

        Some(user_ptr as *mut u8)
    }

    unsafe fn deallocate(&self, ptr: *mut u8, _size: usize, _align: usize) {
        if ptr.is_null() {
            return;
        }

        let buffer = unsafe { &*self.buffer.get() };
        let base = buffer.as_ptr() as usize;
        let user_ptr = ptr as usize;

        // Find header (search backwards from user pointer)
        let header_ptr = user_ptr - Self::HEADER_SIZE;
        let header = &mut *(header_ptr as *mut BlockHeader);

        if header.is_free {
            return; // Double free protection
        }

        let offset = header_ptr - base;
        let size = header.size;

        header.is_free = true;

        let mut blocks = self.free_blocks.lock();
        blocks.push((offset, size));

        // Coalesce adjacent blocks
        self.coalesce(&mut blocks);

        *self.used.lock() -= size;
    }

    fn reset(&mut self) {
        let mut blocks = self.free_blocks.lock();
        blocks.clear();
        blocks.push((0, self.capacity));
        *self.used.lock() = 0;
    }

    fn capacity(&self) -> usize {
        self.capacity
    }

    fn used(&self) -> usize {
        *self.used.lock()
    }
}

/// Free list statistics
#[derive(Clone, Copy, Debug)]
pub struct FreeListStats {
    pub capacity: usize,
    pub used: usize,
    pub free: usize,
    pub free_blocks: usize,
    pub largest_free_block: usize,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_free_list_basic() {
        let alloc = FreeList::new(1024);

        let ptr1 = alloc.allocate(64, 8).unwrap();
        let ptr2 = alloc.allocate(128, 16).unwrap();

        assert!(!ptr1.is_null());
        assert!(!ptr2.is_null());
        assert_ne!(ptr1, ptr2);

        unsafe {
            alloc.deallocate(ptr1, 64, 8);
            alloc.deallocate(ptr2, 128, 16);
        }
    }

    #[test]
    fn test_free_list_coalesce() {
        let alloc = FreeList::new(1024);

        let ptr1 = alloc.allocate(100, 8).unwrap();
        let ptr2 = alloc.allocate(100, 8).unwrap();
        let ptr3 = alloc.allocate(100, 8).unwrap();

        unsafe {
            alloc.deallocate(ptr1, 100, 8);
            alloc.deallocate(ptr3, 100, 8);
            // Blocks are now fragmented

            alloc.deallocate(ptr2, 100, 8);
            // After freeing ptr2, all blocks should coalesce
        }

        let stats = alloc.stats();
        assert_eq!(stats.free_blocks, 1); // Should be one big free block
    }
}
