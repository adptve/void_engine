//! SparseSet - Fast iteration with stable entity indices
//!
//! Used in ECS for component storage with O(1) operations.

use alloc::vec::Vec;

/// Sparse set for efficient entity-to-component mapping
pub struct SparseSet<T> {
    /// Sparse array: index -> dense index (or None)
    sparse: Vec<Option<usize>>,
    /// Dense array of values
    dense: Vec<T>,
    /// Dense array of indices (for reverse lookup)
    indices: Vec<usize>,
}

impl<T> SparseSet<T> {
    /// Create a new empty sparse set
    pub fn new() -> Self {
        Self {
            sparse: Vec::new(),
            dense: Vec::new(),
            indices: Vec::new(),
        }
    }

    /// Create with initial sparse capacity
    pub fn with_capacity(sparse_capacity: usize, dense_capacity: usize) -> Self {
        Self {
            sparse: Vec::with_capacity(sparse_capacity),
            dense: Vec::with_capacity(dense_capacity),
            indices: Vec::with_capacity(dense_capacity),
        }
    }

    /// Insert or update a value at the given index
    pub fn insert(&mut self, index: usize, value: T) -> Option<T> {
        // Grow sparse array if needed
        if index >= self.sparse.len() {
            self.sparse.resize(index + 1, None);
        }

        if let Some(dense_idx) = self.sparse[index] {
            // Update existing
            let old = core::mem::replace(&mut self.dense[dense_idx], value);
            Some(old)
        } else {
            // Insert new
            let dense_idx = self.dense.len();
            self.sparse[index] = Some(dense_idx);
            self.dense.push(value);
            self.indices.push(index);
            None
        }
    }

    /// Remove a value by index
    pub fn remove(&mut self, index: usize) -> Option<T> {
        if index >= self.sparse.len() {
            return None;
        }

        let dense_idx = self.sparse[index].take()?;

        // Swap-remove from dense arrays
        let value = self.dense.swap_remove(dense_idx);
        let _removed_index = self.indices.swap_remove(dense_idx);

        // Update the swapped element's sparse entry
        if dense_idx < self.dense.len() {
            let swapped_index = self.indices[dense_idx];
            self.sparse[swapped_index] = Some(dense_idx);
        }

        Some(value)
    }

    /// Get a reference to a value
    pub fn get(&self, index: usize) -> Option<&T> {
        let dense_idx = *self.sparse.get(index)?.as_ref()?;
        self.dense.get(dense_idx)
    }

    /// Get a mutable reference to a value
    pub fn get_mut(&mut self, index: usize) -> Option<&mut T> {
        let dense_idx = *self.sparse.get(index)?.as_ref()?;
        self.dense.get_mut(dense_idx)
    }

    /// Check if the set contains an index
    pub fn contains(&self, index: usize) -> bool {
        self.sparse.get(index).and_then(|o| o.as_ref()).is_some()
    }

    /// Get the number of elements
    pub fn len(&self) -> usize {
        self.dense.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.dense.is_empty()
    }

    /// Clear all elements
    pub fn clear(&mut self) {
        for idx in &self.indices {
            if *idx < self.sparse.len() {
                self.sparse[*idx] = None;
            }
        }
        self.dense.clear();
        self.indices.clear();
    }

    /// Iterate over all values
    pub fn iter(&self) -> impl Iterator<Item = (usize, &T)> {
        self.indices.iter().zip(self.dense.iter()).map(|(i, v)| (*i, v))
    }

    /// Iterate over values mutably
    pub fn iter_mut(&mut self) -> impl Iterator<Item = (usize, &mut T)> {
        self.indices.iter().zip(self.dense.iter_mut()).map(|(i, v)| (*i, v))
    }

    /// Iterate over indices only
    pub fn indices(&self) -> impl Iterator<Item = usize> + '_ {
        self.indices.iter().copied()
    }

    /// Iterate over values only (in dense order)
    pub fn values(&self) -> impl Iterator<Item = &T> {
        self.dense.iter()
    }

    /// Iterate over values mutably (in dense order)
    pub fn values_mut(&mut self) -> impl Iterator<Item = &mut T> {
        self.dense.iter_mut()
    }

    /// Get the dense array directly (for SIMD operations)
    pub fn as_slice(&self) -> &[T] {
        &self.dense
    }

    /// Get the dense array mutably
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        &mut self.dense
    }

    /// Get the indices array
    pub fn indices_slice(&self) -> &[usize] {
        &self.indices
    }
}

impl<T> Default for SparseSet<T> {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sparse_set_basic() {
        let mut set: SparseSet<i32> = SparseSet::new();

        set.insert(5, 50);
        set.insert(10, 100);
        set.insert(3, 30);

        assert_eq!(set.get(5), Some(&50));
        assert_eq!(set.get(10), Some(&100));
        assert_eq!(set.get(3), Some(&30));
        assert_eq!(set.get(0), None);
        assert_eq!(set.len(), 3);
    }

    #[test]
    fn test_sparse_set_remove() {
        let mut set: SparseSet<i32> = SparseSet::new();

        set.insert(0, 10);
        set.insert(1, 20);
        set.insert(2, 30);

        let removed = set.remove(1);
        assert_eq!(removed, Some(20));
        assert_eq!(set.get(1), None);
        assert_eq!(set.len(), 2);

        // Check that other elements are still accessible
        assert_eq!(set.get(0), Some(&10));
        assert_eq!(set.get(2), Some(&30));
    }

    #[test]
    fn test_sparse_set_iteration() {
        let mut set: SparseSet<i32> = SparseSet::new();

        set.insert(5, 50);
        set.insert(10, 100);

        let values: Vec<_> = set.values().cloned().collect();
        assert_eq!(values.len(), 2);
        assert!(values.contains(&50));
        assert!(values.contains(&100));
    }
}
