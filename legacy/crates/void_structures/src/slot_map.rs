//! SlotMap - Generational index-based storage
//!
//! Provides O(1) insertion, removal, and lookup with use-after-free detection.

use alloc::vec::Vec;
use core::marker::PhantomData;

/// Key for slot map access with generation tracking
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub struct SlotKey<T> {
    index: u32,
    generation: u32,
    _marker: PhantomData<fn() -> T>,
}

impl<T> SlotKey<T> {
    /// Create a new key
    #[inline]
    pub const fn new(index: u32, generation: u32) -> Self {
        Self {
            index,
            generation,
            _marker: PhantomData,
        }
    }

    /// Get the raw index
    #[inline]
    pub const fn index(&self) -> u32 {
        self.index
    }

    /// Get the generation
    #[inline]
    pub const fn generation(&self) -> u32 {
        self.generation
    }

    /// Create a null/invalid key
    #[inline]
    pub const fn null() -> Self {
        Self::new(u32::MAX, 0)
    }

    /// Check if key is null
    #[inline]
    pub const fn is_null(&self) -> bool {
        self.index == u32::MAX
    }
}

impl<T> Default for SlotKey<T> {
    fn default() -> Self {
        Self::null()
    }
}

impl<T> core::fmt::Debug for SlotKey<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        if self.is_null() {
            write!(f, "SlotKey(null)")
        } else {
            write!(f, "SlotKey({}v{})", self.index, self.generation)
        }
    }
}

/// Slot entry
struct Slot<T> {
    value: Option<T>,
    generation: u32,
}

/// SlotMap - generational index storage
pub struct SlotMap<T> {
    slots: Vec<Slot<T>>,
    free_list: Vec<u32>,
    len: usize,
}

impl<T> SlotMap<T> {
    /// Create a new empty slot map
    pub fn new() -> Self {
        Self::with_capacity(0)
    }

    /// Create with initial capacity
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            slots: Vec::with_capacity(capacity),
            free_list: Vec::new(),
            len: 0,
        }
    }

    /// Insert a value and get its key
    pub fn insert(&mut self, value: T) -> SlotKey<T> {
        self.len += 1;

        if let Some(index) = self.free_list.pop() {
            let slot = &mut self.slots[index as usize];
            slot.value = Some(value);
            SlotKey::new(index, slot.generation)
        } else {
            let index = self.slots.len() as u32;
            self.slots.push(Slot {
                value: Some(value),
                generation: 0,
            });
            SlotKey::new(index, 0)
        }
    }

    /// Remove a value by key
    pub fn remove(&mut self, key: SlotKey<T>) -> Option<T> {
        let slot = self.slots.get_mut(key.index as usize)?;

        if slot.generation != key.generation || slot.value.is_none() {
            return None;
        }

        slot.generation = slot.generation.wrapping_add(1);
        self.free_list.push(key.index);
        self.len -= 1;

        slot.value.take()
    }

    /// Get a reference to a value
    pub fn get(&self, key: SlotKey<T>) -> Option<&T> {
        let slot = self.slots.get(key.index as usize)?;
        if slot.generation != key.generation {
            return None;
        }
        slot.value.as_ref()
    }

    /// Get a mutable reference to a value
    pub fn get_mut(&mut self, key: SlotKey<T>) -> Option<&mut T> {
        let slot = self.slots.get_mut(key.index as usize)?;
        if slot.generation != key.generation {
            return None;
        }
        slot.value.as_mut()
    }

    /// Check if a key is valid
    pub fn contains_key(&self, key: SlotKey<T>) -> bool {
        self.slots
            .get(key.index as usize)
            .map(|s| s.generation == key.generation && s.value.is_some())
            .unwrap_or(false)
    }

    /// Get the number of elements
    pub fn len(&self) -> usize {
        self.len
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Clear all elements
    pub fn clear(&mut self) {
        for (i, slot) in self.slots.iter_mut().enumerate() {
            if slot.value.is_some() {
                slot.value = None;
                slot.generation = slot.generation.wrapping_add(1);
                self.free_list.push(i as u32);
            }
        }
        self.len = 0;
    }

    /// Iterate over all values
    pub fn iter(&self) -> impl Iterator<Item = (SlotKey<T>, &T)> {
        self.slots.iter().enumerate().filter_map(|(i, slot)| {
            slot.value.as_ref().map(|v| {
                (SlotKey::new(i as u32, slot.generation), v)
            })
        })
    }

    /// Iterate over all values mutably
    pub fn iter_mut(&mut self) -> impl Iterator<Item = (SlotKey<T>, &mut T)> {
        self.slots.iter_mut().enumerate().filter_map(|(i, slot)| {
            let gen = slot.generation;
            slot.value.as_mut().map(|v| {
                (SlotKey::new(i as u32, gen), v)
            })
        })
    }

    /// Iterate over keys only
    pub fn keys(&self) -> impl Iterator<Item = SlotKey<T>> + '_ {
        self.slots.iter().enumerate().filter_map(|(i, slot)| {
            if slot.value.is_some() {
                Some(SlotKey::new(i as u32, slot.generation))
            } else {
                None
            }
        })
    }

    /// Iterate over values only
    pub fn values(&self) -> impl Iterator<Item = &T> {
        self.slots.iter().filter_map(|slot| slot.value.as_ref())
    }

    /// Iterate over values mutably
    pub fn values_mut(&mut self) -> impl Iterator<Item = &mut T> {
        self.slots.iter_mut().filter_map(|slot| slot.value.as_mut())
    }

    /// Get capacity
    pub fn capacity(&self) -> usize {
        self.slots.capacity()
    }

    /// Reserve additional capacity
    pub fn reserve(&mut self, additional: usize) {
        self.slots.reserve(additional);
    }
}

impl<T> Default for SlotMap<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T> core::ops::Index<SlotKey<T>> for SlotMap<T> {
    type Output = T;

    fn index(&self, key: SlotKey<T>) -> &Self::Output {
        self.get(key).expect("Invalid slot key")
    }
}

impl<T> core::ops::IndexMut<SlotKey<T>> for SlotMap<T> {
    fn index_mut(&mut self, key: SlotKey<T>) -> &mut Self::Output {
        self.get_mut(key).expect("Invalid slot key")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_slot_map_basic() {
        let mut map: SlotMap<i32> = SlotMap::new();

        let key1 = map.insert(42);
        let key2 = map.insert(100);

        assert_eq!(map.get(key1), Some(&42));
        assert_eq!(map.get(key2), Some(&100));
        assert_eq!(map.len(), 2);
    }

    #[test]
    fn test_slot_map_remove() {
        let mut map: SlotMap<i32> = SlotMap::new();

        let key1 = map.insert(42);
        let removed = map.remove(key1);

        assert_eq!(removed, Some(42));
        assert_eq!(map.get(key1), None);
        assert_eq!(map.len(), 0);
    }

    #[test]
    fn test_slot_map_generation() {
        let mut map: SlotMap<i32> = SlotMap::new();

        let key1 = map.insert(42);
        map.remove(key1);

        let key2 = map.insert(100);

        // Same index, different generation
        assert_eq!(key1.index(), key2.index());
        assert_ne!(key1.generation(), key2.generation());

        // Old key should not work
        assert_eq!(map.get(key1), None);
        assert_eq!(map.get(key2), Some(&100));
    }
}
