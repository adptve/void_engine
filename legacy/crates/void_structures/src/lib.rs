//! # void_structures - High-Performance Data Structures
//!
//! Lock-free and cache-friendly data structures for game engines:
//! - SlotMap: Generational index-based storage
//! - SparseSet: Fast iteration with stable indices
//! - LockFreeQueue: Multi-producer multi-consumer queue

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

pub mod slot_map;
pub mod sparse_set;
pub mod lock_free_queue;
pub mod bitset;

pub use slot_map::{SlotMap, SlotKey};
pub use sparse_set::SparseSet;
pub use lock_free_queue::LockFreeQueue;
pub use bitset::BitSet;

pub mod prelude {
    pub use crate::slot_map::{SlotMap, SlotKey};
    pub use crate::sparse_set::SparseSet;
    pub use crate::lock_free_queue::LockFreeQueue;
    pub use crate::bitset::BitSet;
}
