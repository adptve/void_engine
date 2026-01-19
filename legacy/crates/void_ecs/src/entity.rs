//! Entity - Unique identifiers for game objects
//!
//! Entities are lightweight identifiers with generational indices
//! to detect use-after-free.

use alloc::vec::Vec;
use core::fmt;
use core::hash::{Hash, Hasher};

/// Entity identifier with generation for ABA protection
#[derive(Clone, Copy, PartialOrd, Ord)]
pub struct Entity {
    /// Index into entity storage
    index: u32,
    /// Generation to detect stale references
    generation: u32,
}

impl Entity {
    /// Create a new entity
    #[inline]
    pub const fn new(index: u32, generation: u32) -> Self {
        Self { index, generation }
    }

    /// Create an invalid/null entity
    #[inline]
    pub const fn null() -> Self {
        Self {
            index: u32::MAX,
            generation: u32::MAX,
        }
    }

    /// Get the entity index
    #[inline]
    pub const fn index(&self) -> u32 {
        self.index
    }

    /// Get the generation
    #[inline]
    pub const fn generation(&self) -> u32 {
        self.generation
    }

    /// Check if this is a null entity
    #[inline]
    pub const fn is_null(&self) -> bool {
        self.index == u32::MAX
    }

    /// Convert to u64 for efficient storage
    #[inline]
    pub const fn to_bits(&self) -> u64 {
        ((self.generation as u64) << 32) | (self.index as u64)
    }

    /// Create from u64
    #[inline]
    pub const fn from_bits(bits: u64) -> Self {
        Self {
            index: bits as u32,
            generation: (bits >> 32) as u32,
        }
    }
}

impl PartialEq for Entity {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.index == other.index && self.generation == other.generation
    }
}

impl Eq for Entity {}

impl Hash for Entity {
    #[inline]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.to_bits().hash(state);
    }
}

impl Default for Entity {
    fn default() -> Self {
        Self::null()
    }
}

impl fmt::Debug for Entity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.is_null() {
            write!(f, "Entity(null)")
        } else {
            write!(f, "Entity({}v{})", self.index, self.generation)
        }
    }
}

impl fmt::Display for Entity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.is_null() {
            write!(f, "null")
        } else {
            write!(f, "{}v{}", self.index, self.generation)
        }
    }
}

/// Entity allocator with free list
pub struct EntityAllocator {
    /// Current generations for each index
    generations: Vec<u32>,
    /// Free indices
    free_list: Vec<u32>,
    /// Number of alive entities
    alive_count: usize,
}

impl EntityAllocator {
    /// Create a new entity allocator
    pub fn new() -> Self {
        Self {
            generations: Vec::new(),
            free_list: Vec::new(),
            alive_count: 0,
        }
    }

    /// Create with initial capacity
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            generations: Vec::with_capacity(capacity),
            free_list: Vec::new(),
            alive_count: 0,
        }
    }

    /// Allocate a new entity
    pub fn allocate(&mut self) -> Entity {
        self.alive_count += 1;

        if let Some(index) = self.free_list.pop() {
            let generation = self.generations[index as usize];
            Entity::new(index, generation)
        } else {
            let index = self.generations.len() as u32;
            self.generations.push(0);
            Entity::new(index, 0)
        }
    }

    /// Deallocate an entity
    pub fn deallocate(&mut self, entity: Entity) -> bool {
        if !self.is_alive(entity) {
            return false;
        }

        self.generations[entity.index as usize] =
            self.generations[entity.index as usize].wrapping_add(1);
        self.free_list.push(entity.index);
        self.alive_count -= 1;
        true
    }

    /// Check if an entity is alive
    #[inline]
    pub fn is_alive(&self, entity: Entity) -> bool {
        if entity.is_null() {
            return false;
        }
        self.generations
            .get(entity.index as usize)
            .map(|&gen| gen == entity.generation)
            .unwrap_or(false)
    }

    /// Get the number of alive entities
    #[inline]
    pub fn alive_count(&self) -> usize {
        self.alive_count
    }

    /// Get total capacity
    #[inline]
    pub fn capacity(&self) -> usize {
        self.generations.len()
    }

    /// Reserve additional capacity
    pub fn reserve(&mut self, additional: usize) {
        self.generations.reserve(additional);
    }
}

impl Default for EntityAllocator {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_entity_creation() {
        let e = Entity::new(5, 3);
        assert_eq!(e.index(), 5);
        assert_eq!(e.generation(), 3);
        assert!(!e.is_null());
    }

    #[test]
    fn test_entity_null() {
        let e = Entity::null();
        assert!(e.is_null());
    }

    #[test]
    fn test_entity_bits() {
        let e = Entity::new(123, 456);
        let bits = e.to_bits();
        let restored = Entity::from_bits(bits);
        assert_eq!(e, restored);
    }

    #[test]
    fn test_allocator() {
        let mut alloc = EntityAllocator::new();

        let e1 = alloc.allocate();
        let e2 = alloc.allocate();

        assert!(alloc.is_alive(e1));
        assert!(alloc.is_alive(e2));
        assert_eq!(alloc.alive_count(), 2);

        alloc.deallocate(e1);
        assert!(!alloc.is_alive(e1));
        assert_eq!(alloc.alive_count(), 1);

        // Reallocate - should reuse index with new generation
        let e3 = alloc.allocate();
        assert_eq!(e3.index(), e1.index());
        assert_ne!(e3.generation(), e1.generation());
    }
}
