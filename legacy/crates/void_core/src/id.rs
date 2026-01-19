//! Unique identifier generation with generational indices

use core::sync::atomic::{AtomicU64, Ordering};
use core::hash::{Hash, Hasher};
use core::fmt;
use alloc::boxed::Box;
use alloc::string::String;

/// A unique identifier with a generation counter for safe reuse
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct Id {
    /// Lower 32 bits: index, Upper 32 bits: generation
    bits: u64,
}

impl Id {
    /// Create a new ID from index and generation
    #[inline]
    pub const fn new(index: u32, generation: u32) -> Self {
        Self {
            bits: (generation as u64) << 32 | index as u64,
        }
    }

    /// Create a null/invalid ID
    #[inline]
    pub const fn null() -> Self {
        Self { bits: u64::MAX }
    }

    /// Check if this ID is null
    #[inline]
    pub const fn is_null(&self) -> bool {
        self.bits == u64::MAX
    }

    /// Get the index portion
    #[inline]
    pub const fn index(&self) -> u32 {
        self.bits as u32
    }

    /// Get the generation portion
    #[inline]
    pub const fn generation(&self) -> u32 {
        (self.bits >> 32) as u32
    }

    /// Get the raw bits
    #[inline]
    pub const fn to_bits(&self) -> u64 {
        self.bits
    }

    /// Create from raw bits
    #[inline]
    pub const fn from_bits(bits: u64) -> Self {
        Self { bits }
    }

    /// Create an ID from a name using a simple hash
    pub fn from_name(name: &str) -> Self {
        // Simple FNV-1a hash
        let mut hash = 0xcbf29ce484222325u64;
        for byte in name.bytes() {
            hash ^= byte as u64;
            hash = hash.wrapping_mul(0x100000001b3);
        }
        Self { bits: hash }
    }
}

impl Hash for Id {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.bits.hash(state);
    }
}

impl fmt::Debug for Id {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.is_null() {
            write!(f, "Id(null)")
        } else {
            write!(f, "Id({}v{})", self.index(), self.generation())
        }
    }
}

impl fmt::Display for Id {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.is_null() {
            write!(f, "null")
        } else {
            write!(f, "{}v{}", self.index(), self.generation())
        }
    }
}

/// Thread-safe ID generator
pub struct IdGenerator {
    next: AtomicU64,
}

impl IdGenerator {
    /// Create a new ID generator
    pub const fn new() -> Self {
        Self {
            next: AtomicU64::new(0),
        }
    }

    /// Generate the next unique ID
    pub fn next(&self) -> Id {
        let index = self.next.fetch_add(1, Ordering::Relaxed);
        Id::new(index as u32, 0)
    }

    /// Generate a batch of IDs efficiently
    pub fn next_batch(&self, count: u32) -> impl Iterator<Item = Id> {
        let start = self.next.fetch_add(count as u64, Ordering::Relaxed);
        (0..count).map(move |i| Id::new((start + i as u64) as u32, 0))
    }
}

impl Default for IdGenerator {
    fn default() -> Self {
        Self::new()
    }
}

/// A string-based identifier for named resources
#[derive(Clone, PartialEq, Eq, Hash)]
pub struct NamedId {
    name: Box<str>,
    hash: u64,
}

impl NamedId {
    /// Create a new named ID
    pub fn new(name: &str) -> Self {
        // Simple FNV-1a hash
        let mut hash = 0xcbf29ce484222325u64;
        for byte in name.bytes() {
            hash ^= byte as u64;
            hash = hash.wrapping_mul(0x100000001b3);
        }

        Self {
            name: name.into(),
            hash,
        }
    }

    /// Get the name
    #[inline]
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get the precomputed hash
    #[inline]
    pub fn hash_value(&self) -> u64 {
        self.hash
    }
}

impl fmt::Debug for NamedId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "NamedId({:?})", self.name)
    }
}

impl fmt::Display for NamedId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.name)
    }
}

impl From<&str> for NamedId {
    fn from(s: &str) -> Self {
        Self::new(s)
    }
}

impl From<String> for NamedId {
    fn from(s: String) -> Self {
        Self::new(&s)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_id_creation() {
        let id = Id::new(42, 7);
        assert_eq!(id.index(), 42);
        assert_eq!(id.generation(), 7);
    }

    #[test]
    fn test_id_generator() {
        let gen = IdGenerator::new();
        let id1 = gen.next();
        let id2 = gen.next();
        assert_ne!(id1, id2);
        assert_eq!(id1.index(), 0);
        assert_eq!(id2.index(), 1);
    }
}
