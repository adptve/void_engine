//! BitSet - Efficient bit-level storage

use alloc::vec;
use alloc::vec::Vec;

/// Fixed-size bitset
pub struct BitSet {
    bits: Vec<u64>,
    len: usize,
}

impl BitSet {
    /// Bits per word
    const BITS_PER_WORD: usize = 64;

    /// Create a new bitset with capacity for n bits
    pub fn new(capacity: usize) -> Self {
        let words = (capacity + Self::BITS_PER_WORD - 1) / Self::BITS_PER_WORD;
        Self {
            bits: vec![0; words],
            len: capacity,
        }
    }

    /// Create from an existing bitset
    pub fn from_bits(bits: &[u64], len: usize) -> Self {
        Self {
            bits: bits.to_vec(),
            len,
        }
    }

    /// Set a bit
    #[inline]
    pub fn set(&mut self, index: usize) {
        debug_assert!(index < self.len);
        let word = index / Self::BITS_PER_WORD;
        let bit = index % Self::BITS_PER_WORD;
        self.bits[word] |= 1u64 << bit;
    }

    /// Clear a bit
    #[inline]
    pub fn clear(&mut self, index: usize) {
        debug_assert!(index < self.len);
        let word = index / Self::BITS_PER_WORD;
        let bit = index % Self::BITS_PER_WORD;
        self.bits[word] &= !(1u64 << bit);
    }

    /// Toggle a bit
    #[inline]
    pub fn toggle(&mut self, index: usize) {
        debug_assert!(index < self.len);
        let word = index / Self::BITS_PER_WORD;
        let bit = index % Self::BITS_PER_WORD;
        self.bits[word] ^= 1u64 << bit;
    }

    /// Get a bit
    #[inline]
    pub fn get(&self, index: usize) -> bool {
        debug_assert!(index < self.len);
        let word = index / Self::BITS_PER_WORD;
        let bit = index % Self::BITS_PER_WORD;
        (self.bits[word] & (1u64 << bit)) != 0
    }

    /// Set all bits
    pub fn set_all(&mut self) {
        for word in &mut self.bits {
            *word = u64::MAX;
        }
        // Clear unused bits in the last word
        let last_bits = self.len % Self::BITS_PER_WORD;
        if last_bits != 0 && !self.bits.is_empty() {
            let mask = (1u64 << last_bits) - 1;
            *self.bits.last_mut().unwrap() &= mask;
        }
    }

    /// Clear all bits
    pub fn clear_all(&mut self) {
        for word in &mut self.bits {
            *word = 0;
        }
    }

    /// Count set bits
    pub fn count_ones(&self) -> usize {
        self.bits.iter().map(|w| w.count_ones() as usize).sum()
    }

    /// Count clear bits
    pub fn count_zeros(&self) -> usize {
        self.len - self.count_ones()
    }

    /// Check if any bit is set
    pub fn any(&self) -> bool {
        self.bits.iter().any(|&w| w != 0)
    }

    /// Check if all bits are set
    pub fn all(&self) -> bool {
        self.count_ones() == self.len
    }

    /// Check if no bits are set
    pub fn none(&self) -> bool {
        self.bits.iter().all(|&w| w == 0)
    }

    /// Get the capacity
    pub fn len(&self) -> usize {
        self.len
    }

    /// Check if empty (no capacity)
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Bitwise AND with another bitset
    pub fn and(&self, other: &BitSet) -> BitSet {
        let len = self.len.min(other.len);
        let bits: Vec<_> = self.bits.iter()
            .zip(other.bits.iter())
            .map(|(a, b)| a & b)
            .collect();
        BitSet { bits, len }
    }

    /// Bitwise OR with another bitset
    pub fn or(&self, other: &BitSet) -> BitSet {
        let len = self.len.max(other.len);
        let mut bits = vec![0u64; (len + Self::BITS_PER_WORD - 1) / Self::BITS_PER_WORD];

        for (i, word) in self.bits.iter().enumerate() {
            bits[i] = *word;
        }
        for (i, word) in other.bits.iter().enumerate() {
            bits[i] |= *word;
        }

        BitSet { bits, len }
    }

    /// Bitwise XOR with another bitset
    pub fn xor(&self, other: &BitSet) -> BitSet {
        let len = self.len.max(other.len);
        let mut bits = vec![0u64; (len + Self::BITS_PER_WORD - 1) / Self::BITS_PER_WORD];

        for (i, word) in self.bits.iter().enumerate() {
            bits[i] = *word;
        }
        for (i, word) in other.bits.iter().enumerate() {
            bits[i] ^= *word;
        }

        BitSet { bits, len }
    }

    /// Bitwise NOT
    pub fn not(&self) -> BitSet {
        let bits: Vec<_> = self.bits.iter().map(|w| !w).collect();
        let mut result = BitSet { bits, len: self.len };

        // Clear unused bits in the last word
        let last_bits = self.len % Self::BITS_PER_WORD;
        if last_bits != 0 && !result.bits.is_empty() {
            let mask = (1u64 << last_bits) - 1;
            *result.bits.last_mut().unwrap() &= mask;
        }

        result
    }

    /// Iterate over set bit indices
    pub fn iter_ones(&self) -> impl Iterator<Item = usize> + '_ {
        let len = self.len;
        self.bits.iter().enumerate().flat_map(move |(word_idx, &word)| {
            (0..Self::BITS_PER_WORD).filter_map(move |bit| {
                let index = word_idx * Self::BITS_PER_WORD + bit;
                if index < len && (word & (1u64 << bit)) != 0 {
                    Some(index)
                } else {
                    None
                }
            })
        })
    }

    /// Get the raw words
    pub fn as_words(&self) -> &[u64] {
        &self.bits
    }
}

impl Clone for BitSet {
    fn clone(&self) -> Self {
        Self {
            bits: self.bits.clone(),
            len: self.len,
        }
    }
}

impl Default for BitSet {
    fn default() -> Self {
        Self::new(64)
    }
}

impl core::fmt::Debug for BitSet {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "BitSet(len={}, ones={})", self.len, self.count_ones())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bitset_basic() {
        let mut bs = BitSet::new(100);

        bs.set(0);
        bs.set(50);
        bs.set(99);

        assert!(bs.get(0));
        assert!(bs.get(50));
        assert!(bs.get(99));
        assert!(!bs.get(1));
        assert_eq!(bs.count_ones(), 3);
    }

    #[test]
    fn test_bitset_operations() {
        let mut a = BitSet::new(8);
        let mut b = BitSet::new(8);

        a.set(0);
        a.set(1);
        b.set(1);
        b.set(2);

        let and = a.and(&b);
        assert!(and.get(1));
        assert!(!and.get(0));
        assert!(!and.get(2));

        let or = a.or(&b);
        assert!(or.get(0));
        assert!(or.get(1));
        assert!(or.get(2));
    }

    #[test]
    fn test_bitset_iter() {
        let mut bs = BitSet::new(100);
        bs.set(5);
        bs.set(42);
        bs.set(73);

        let ones: Vec<_> = bs.iter_ones().collect();
        assert_eq!(ones, vec![5, 42, 73]);
    }
}
