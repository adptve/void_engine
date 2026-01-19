//! Stack allocator - LIFO allocation

use core::cell::UnsafeCell;
use core::sync::atomic::{AtomicUsize, Ordering};
use alloc::vec::Vec;

use crate::{Allocator, align_up};

/// Allocation header for stack allocator
#[repr(C)]
struct StackHeader {
    /// Previous stack top (for deallocation)
    previous_top: usize,
    /// Size of this allocation (including header)
    size: usize,
}

/// Stack allocator - LIFO allocation with markers
///
/// Fast allocator for temporary data that follows a stack pattern.
/// Supports both individual deallocation (LIFO order) and bulk rollback via markers.
pub struct StackAllocator {
    /// Backing memory
    buffer: UnsafeCell<Vec<u8>>,
    /// Current top of stack
    top: AtomicUsize,
    /// Total capacity
    capacity: usize,
}

// Safety: Stack uses atomic top for thread-safe allocation
unsafe impl Send for StackAllocator {}
unsafe impl Sync for StackAllocator {}

impl StackAllocator {
    const HEADER_SIZE: usize = core::mem::size_of::<StackHeader>();
    const HEADER_ALIGN: usize = core::mem::align_of::<StackHeader>();

    /// Create a new stack allocator
    pub fn new(capacity: usize) -> Self {
        let mut buffer = Vec::with_capacity(capacity);
        buffer.resize(capacity, 0);

        Self {
            buffer: UnsafeCell::new(buffer),
            top: AtomicUsize::new(0),
            capacity,
        }
    }

    /// Create with capacity in KB
    pub fn with_capacity_kb(kb: usize) -> Self {
        Self::new(kb * 1024)
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

    /// Get a marker for the current stack position
    pub fn marker(&self) -> StackMarker {
        StackMarker(self.top.load(Ordering::Acquire))
    }

    /// Rollback to a previous marker
    pub fn rollback(&self, marker: StackMarker) {
        let current = self.top.load(Ordering::Acquire);
        if marker.0 <= current {
            self.top.store(marker.0, Ordering::Release);
        }
    }

    /// Get the current top position
    pub fn current_position(&self) -> usize {
        self.top.load(Ordering::Relaxed)
    }

    /// Pop the last allocation (must be the exact pointer returned)
    ///
    /// # Safety
    /// Must pop in LIFO order
    pub unsafe fn pop(&self, ptr: *mut u8, size: usize, align: usize) {
        self.deallocate(ptr, size, align);
    }
}

impl Allocator for StackAllocator {
    fn allocate(&self, size: usize, align: usize) -> Option<*mut u8> {
        if size == 0 {
            return Some(align as *mut u8);
        }

        let total_align = align.max(Self::HEADER_ALIGN);

        loop {
            let current_top = self.top.load(Ordering::Relaxed);

            let buffer = unsafe { &*self.buffer.get() };
            let base = buffer.as_ptr() as usize;

            // Calculate header and user pointer positions
            let header_offset = align_up(current_top, Self::HEADER_ALIGN);
            let user_offset = align_up(header_offset + Self::HEADER_SIZE, total_align);
            let new_top = user_offset + size;

            if new_top > self.capacity {
                return None;
            }

            // Try to claim this allocation
            match self.top.compare_exchange_weak(
                current_top,
                new_top,
                Ordering::AcqRel,
                Ordering::Relaxed,
            ) {
                Ok(_) => {
                    // Write header
                    let header_ptr = (base + header_offset) as *mut StackHeader;
                    unsafe {
                        (*header_ptr).previous_top = current_top;
                        (*header_ptr).size = new_top - header_offset;
                    }

                    return Some((base + user_offset) as *mut u8);
                }
                Err(_) => continue,
            }
        }
    }

    unsafe fn deallocate(&self, ptr: *mut u8, _size: usize, _align: usize) {
        if ptr.is_null() {
            return;
        }

        let buffer = unsafe { &*self.buffer.get() };
        let base = buffer.as_ptr() as usize;
        let user_offset = ptr as usize - base;

        // Find header (it's before the user pointer, aligned)
        let header_offset = align_up(user_offset - Self::HEADER_SIZE, Self::HEADER_ALIGN);
        if header_offset < Self::HEADER_SIZE {
            return;
        }
        let real_header_offset = user_offset - Self::HEADER_SIZE
            - ((user_offset - Self::HEADER_SIZE) % Self::HEADER_ALIGN);

        // Walk back to find the header
        let mut search_offset = user_offset.saturating_sub(Self::HEADER_SIZE);
        while search_offset >= Self::HEADER_ALIGN {
            let potential_header = (base + search_offset) as *mut StackHeader;
            let potential_end = search_offset + (*potential_header).size;

            // Check if this header points to our user data
            if potential_end >= user_offset && search_offset + Self::HEADER_SIZE <= user_offset {
                // Verify this is the top allocation
                let current_top = self.top.load(Ordering::Acquire);
                if potential_end == current_top {
                    self.top.store((*potential_header).previous_top, Ordering::Release);
                }
                return;
            }

            if search_offset < Self::HEADER_ALIGN {
                break;
            }
            search_offset -= Self::HEADER_ALIGN;
        }
    }

    fn reset(&mut self) {
        self.top.store(0, Ordering::Release);
    }

    fn capacity(&self) -> usize {
        self.capacity
    }

    fn used(&self) -> usize {
        self.top.load(Ordering::Relaxed)
    }
}

impl Default for StackAllocator {
    fn default() -> Self {
        Self::new(1024 * 1024)
    }
}

/// Marker for stack position
#[derive(Clone, Copy, Debug)]
pub struct StackMarker(usize);

/// Scoped stack allocator guard
pub struct StackScope<'a> {
    stack: &'a StackAllocator,
    marker: StackMarker,
}

impl<'a> StackScope<'a> {
    /// Create a new scoped stack
    pub fn new(stack: &'a StackAllocator) -> Self {
        Self {
            marker: stack.marker(),
            stack,
        }
    }

    /// Access the stack for allocations
    pub fn stack(&self) -> &StackAllocator {
        self.stack
    }
}

impl Drop for StackScope<'_> {
    fn drop(&mut self) {
        self.stack.rollback(self.marker);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_stack_allocation() {
        let stack = StackAllocator::new(1024);

        let a = stack.alloc(42i32).unwrap();
        let b = stack.alloc(3.14f32).unwrap();

        assert_eq!(*a, 42);
        assert_eq!(*b, 3.14);
    }

    #[test]
    fn test_stack_scope() {
        let stack = StackAllocator::new(1024);

        let initial = stack.used();

        {
            let _scope = StackScope::new(&stack);
            stack.alloc(42i32).unwrap();
            stack.alloc(42i32).unwrap();
            assert!(stack.used() > initial);
        }

        assert_eq!(stack.used(), initial);
    }

    #[test]
    fn test_stack_marker() {
        let stack = StackAllocator::new(1024);

        stack.alloc(1i32).unwrap();
        let marker = stack.marker();
        stack.alloc(2i32).unwrap();
        stack.alloc(3i32).unwrap();

        stack.rollback(marker);

        // Should be back to after first allocation
        let pos = stack.current_position();
        assert!(pos < stack.capacity() / 2);
    }
}
