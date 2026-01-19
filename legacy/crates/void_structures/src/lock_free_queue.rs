//! Lock-free multi-producer multi-consumer queue

use alloc::boxed::Box;
use alloc::vec::Vec;
use core::sync::atomic::{AtomicPtr, AtomicUsize, Ordering};
use core::cell::UnsafeCell;

/// Node in the queue
struct Node<T> {
    value: UnsafeCell<Option<T>>,
    next: AtomicPtr<Node<T>>,
}

impl<T> Node<T> {
    fn new(value: Option<T>) -> *mut Self {
        Box::into_raw(Box::new(Self {
            value: UnsafeCell::new(value),
            next: AtomicPtr::new(core::ptr::null_mut()),
        }))
    }
}

/// Lock-free MPMC queue
pub struct LockFreeQueue<T> {
    head: AtomicPtr<Node<T>>,
    tail: AtomicPtr<Node<T>>,
    len: AtomicUsize,
}

unsafe impl<T: Send> Send for LockFreeQueue<T> {}
unsafe impl<T: Send> Sync for LockFreeQueue<T> {}

impl<T> LockFreeQueue<T> {
    /// Create a new empty queue
    pub fn new() -> Self {
        let sentinel = Node::new(None);
        Self {
            head: AtomicPtr::new(sentinel),
            tail: AtomicPtr::new(sentinel),
            len: AtomicUsize::new(0),
        }
    }

    /// Push a value to the back of the queue
    pub fn push(&self, value: T) {
        let node = Node::new(Some(value));

        loop {
            let tail = self.tail.load(Ordering::Acquire);
            let next = unsafe { (*tail).next.load(Ordering::Acquire) };

            if next.is_null() {
                // Try to link new node
                if unsafe {
                    (*tail)
                        .next
                        .compare_exchange(
                            core::ptr::null_mut(),
                            node,
                            Ordering::AcqRel,
                            Ordering::Relaxed,
                        )
                        .is_ok()
                } {
                    // Try to advance tail (ok if it fails, another thread will do it)
                    let _ = self.tail.compare_exchange(
                        tail,
                        node,
                        Ordering::AcqRel,
                        Ordering::Relaxed,
                    );
                    self.len.fetch_add(1, Ordering::Relaxed);
                    return;
                }
            } else {
                // Tail is falling behind, try to advance it
                let _ = self.tail.compare_exchange(
                    tail,
                    next,
                    Ordering::AcqRel,
                    Ordering::Relaxed,
                );
            }
        }
    }

    /// Pop a value from the front of the queue
    pub fn pop(&self) -> Option<T> {
        loop {
            let head = self.head.load(Ordering::Acquire);
            let tail = self.tail.load(Ordering::Acquire);
            let next = unsafe { (*head).next.load(Ordering::Acquire) };

            if head == tail {
                if next.is_null() {
                    // Queue is empty
                    return None;
                }
                // Tail is falling behind, advance it
                let _ = self.tail.compare_exchange(
                    tail,
                    next,
                    Ordering::AcqRel,
                    Ordering::Relaxed,
                );
            } else if !next.is_null() {
                // Try to advance head
                let value = unsafe { (*next).value.get().read() };

                if self
                    .head
                    .compare_exchange(head, next, Ordering::AcqRel, Ordering::Relaxed)
                    .is_ok()
                {
                    // Successfully dequeued, free the old sentinel
                    unsafe {
                        drop(Box::from_raw(head));
                    }
                    self.len.fetch_sub(1, Ordering::Relaxed);
                    return value;
                }
            }
        }
    }

    /// Check if the queue is empty (approximate)
    pub fn is_empty(&self) -> bool {
        let head = self.head.load(Ordering::Relaxed);
        let next = unsafe { (*head).next.load(Ordering::Relaxed) };
        next.is_null()
    }

    /// Get approximate length
    pub fn len(&self) -> usize {
        self.len.load(Ordering::Relaxed)
    }
}

impl<T> Default for LockFreeQueue<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T> Drop for LockFreeQueue<T> {
    fn drop(&mut self) {
        // Drain all remaining elements
        while self.pop().is_some() {}

        // Free the sentinel
        let head = self.head.load(Ordering::Relaxed);
        if !head.is_null() {
            unsafe {
                drop(Box::from_raw(head));
            }
        }
    }
}

/// Bounded lock-free queue with fixed capacity
pub struct BoundedQueue<T> {
    buffer: Box<[UnsafeCell<Option<T>>]>,
    head: AtomicUsize,
    tail: AtomicUsize,
    capacity: usize,
}

unsafe impl<T: Send> Send for BoundedQueue<T> {}
unsafe impl<T: Send> Sync for BoundedQueue<T> {}

impl<T> BoundedQueue<T> {
    /// Create a new bounded queue with the given capacity
    pub fn new(capacity: usize) -> Self {
        let capacity = capacity.next_power_of_two();
        let buffer: Vec<_> = (0..capacity).map(|_| UnsafeCell::new(None)).collect();

        Self {
            buffer: buffer.into_boxed_slice(),
            head: AtomicUsize::new(0),
            tail: AtomicUsize::new(0),
            capacity,
        }
    }

    /// Try to push a value
    pub fn try_push(&self, value: T) -> Result<(), T> {
        loop {
            let tail = self.tail.load(Ordering::Relaxed);
            let head = self.head.load(Ordering::Acquire);

            if tail.wrapping_sub(head) >= self.capacity {
                return Err(value); // Full
            }

            if self
                .tail
                .compare_exchange_weak(
                    tail,
                    tail.wrapping_add(1),
                    Ordering::AcqRel,
                    Ordering::Relaxed,
                )
                .is_ok()
            {
                let index = tail & (self.capacity - 1);
                unsafe {
                    *self.buffer[index].get() = Some(value);
                }
                return Ok(());
            }
        }
    }

    /// Try to pop a value
    pub fn try_pop(&self) -> Option<T> {
        loop {
            let head = self.head.load(Ordering::Relaxed);
            let tail = self.tail.load(Ordering::Acquire);

            if head == tail {
                return None; // Empty
            }

            let index = head & (self.capacity - 1);
            let value = unsafe { (*self.buffer[index].get()).take() };

            if value.is_none() {
                // Another thread beat us, retry
                continue;
            }

            if self
                .head
                .compare_exchange_weak(
                    head,
                    head.wrapping_add(1),
                    Ordering::AcqRel,
                    Ordering::Relaxed,
                )
                .is_ok()
            {
                return value;
            } else {
                // CAS failed, put value back and retry
                unsafe {
                    *self.buffer[index].get() = value;
                }
            }
        }
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.head.load(Ordering::Relaxed) == self.tail.load(Ordering::Relaxed)
    }

    /// Check if full
    pub fn is_full(&self) -> bool {
        let tail = self.tail.load(Ordering::Relaxed);
        let head = self.head.load(Ordering::Relaxed);
        tail.wrapping_sub(head) >= self.capacity
    }

    /// Get approximate length
    pub fn len(&self) -> usize {
        let tail = self.tail.load(Ordering::Relaxed);
        let head = self.head.load(Ordering::Relaxed);
        tail.wrapping_sub(head)
    }

    /// Get capacity
    pub fn capacity(&self) -> usize {
        self.capacity
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_lock_free_queue_basic() {
        let queue: LockFreeQueue<i32> = LockFreeQueue::new();

        queue.push(1);
        queue.push(2);
        queue.push(3);

        assert_eq!(queue.pop(), Some(1));
        assert_eq!(queue.pop(), Some(2));
        assert_eq!(queue.pop(), Some(3));
        assert_eq!(queue.pop(), None);
    }

    #[test]
    fn test_bounded_queue() {
        let queue: BoundedQueue<i32> = BoundedQueue::new(4);

        assert!(queue.try_push(1).is_ok());
        assert!(queue.try_push(2).is_ok());
        assert!(queue.try_push(3).is_ok());
        assert!(queue.try_push(4).is_ok());
        assert!(queue.try_push(5).is_err()); // Full

        assert_eq!(queue.try_pop(), Some(1));
        assert!(queue.try_push(5).is_ok()); // Space available now
    }
}
