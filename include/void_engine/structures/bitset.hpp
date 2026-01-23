#pragma once

/// @file bitset.hpp
/// @brief Efficient bit-level storage for void_structures
///
/// BitSet provides compact storage and fast operations on bits.
/// Ideal for entity masks, component presence tracking, and collision masks.

#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <bit>

namespace void_structures {

/// Dynamic bit-level storage
class BitSet {
public:
    using word_type = std::uint64_t;
    using size_type = std::size_t;

    static constexpr size_type BITS_PER_WORD = 64;

private:
    std::vector<word_type> bits_;
    size_type len_;  // Capacity in bits

    /// Calculate number of words needed for n bits
    [[nodiscard]] static constexpr size_type words_for_bits(size_type n) noexcept {
        return (n + BITS_PER_WORD - 1) / BITS_PER_WORD;
    }

    /// Get word index for bit index
    [[nodiscard]] static constexpr size_type word_index(size_type bit) noexcept {
        return bit / BITS_PER_WORD;
    }

    /// Get bit position within word
    [[nodiscard]] static constexpr size_type bit_offset(size_type bit) noexcept {
        return bit % BITS_PER_WORD;
    }

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create bitset with given capacity (in bits)
    explicit BitSet(size_type capacity = 64)
        : bits_(words_for_bits(capacity), 0)
        , len_(capacity) {}

    /// Create from existing words
    BitSet(const std::vector<word_type>& words, size_type len)
        : bits_(words)
        , len_(len) {
        // Ensure we have enough words
        size_type needed = words_for_bits(len);
        if (bits_.size() < needed) {
            bits_.resize(needed, 0);
        }
    }

    /// Create from initializer list of set bit indices
    BitSet(std::initializer_list<size_type> set_bits, size_type capacity) : BitSet(capacity) {
        for (size_type bit : set_bits) {
            if (bit < len_) {
                set(bit);
            }
        }
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Capacity in bits
    [[nodiscard]] size_type size() const noexcept { return len_; }

    /// Alias for size()
    [[nodiscard]] size_type len() const noexcept { return len_; }

    /// Check if capacity is zero
    [[nodiscard]] bool empty() const noexcept { return len_ == 0; }

    /// Alias for empty()
    [[nodiscard]] bool is_empty() const noexcept { return empty(); }

    /// Number of storage words
    [[nodiscard]] size_type word_count() const noexcept { return bits_.size(); }

    /// Resize to new capacity (in bits)
    void resize(size_type new_capacity) {
        size_type new_words = words_for_bits(new_capacity);
        bits_.resize(new_words, 0);
        len_ = new_capacity;

        // Clear any bits beyond new capacity in the last word
        if (new_capacity > 0) {
            size_type last_word_bits = new_capacity % BITS_PER_WORD;
            if (last_word_bits > 0) {
                word_type mask = (word_type(1) << last_word_bits) - 1;
                bits_.back() &= mask;
            }
        }
    }

    // =========================================================================
    // Bit Operations
    // =========================================================================

    /// Set bit to 1
    void set(size_type index) {
        if (index >= len_) return;
        bits_[word_index(index)] |= (word_type(1) << bit_offset(index));
    }

    /// Set bit to 0
    void clear(size_type index) {
        if (index >= len_) return;
        bits_[word_index(index)] &= ~(word_type(1) << bit_offset(index));
    }

    /// Set bit to specified value
    void set(size_type index, bool value) {
        if (value) {
            set(index);
        } else {
            clear(index);
        }
    }

    /// Toggle bit
    void toggle(size_type index) {
        if (index >= len_) return;
        bits_[word_index(index)] ^= (word_type(1) << bit_offset(index));
    }

    /// Get bit value
    [[nodiscard]] bool get(size_type index) const noexcept {
        if (index >= len_) return false;
        return (bits_[word_index(index)] >> bit_offset(index)) & 1;
    }

    /// Alias for get()
    [[nodiscard]] bool test(size_type index) const noexcept {
        return get(index);
    }

    /// Index operator
    [[nodiscard]] bool operator[](size_type index) const noexcept {
        return get(index);
    }

    // =========================================================================
    // Bulk Operations
    // =========================================================================

    /// Set all bits to 1
    void set_all() {
        std::fill(bits_.begin(), bits_.end(), ~word_type(0));
        // Clear bits beyond capacity
        if (len_ > 0) {
            size_type last_word_bits = len_ % BITS_PER_WORD;
            if (last_word_bits > 0) {
                bits_.back() &= (word_type(1) << last_word_bits) - 1;
            }
        }
    }

    /// Set all bits to 0
    void clear_all() {
        std::fill(bits_.begin(), bits_.end(), 0);
    }

    // =========================================================================
    // Aggregation
    // =========================================================================

    /// Count number of set bits
    [[nodiscard]] size_type count_ones() const noexcept {
        size_type count = 0;
        for (word_type word : bits_) {
            count += static_cast<size_type>(std::popcount(word));
        }
        return count;
    }

    /// Count number of clear bits
    [[nodiscard]] size_type count_zeros() const noexcept {
        return len_ - count_ones();
    }

    /// Check if any bit is set
    [[nodiscard]] bool any() const noexcept {
        for (word_type word : bits_) {
            if (word != 0) return true;
        }
        return false;
    }

    /// Check if all bits are set
    [[nodiscard]] bool all() const noexcept {
        if (len_ == 0) return true;

        // Check all full words
        size_type full_words = len_ / BITS_PER_WORD;
        for (size_type i = 0; i < full_words; ++i) {
            if (bits_[i] != ~word_type(0)) return false;
        }

        // Check last partial word
        size_type remaining = len_ % BITS_PER_WORD;
        if (remaining > 0) {
            word_type mask = (word_type(1) << remaining) - 1;
            if ((bits_.back() & mask) != mask) return false;
        }

        return true;
    }

    /// Check if no bits are set
    [[nodiscard]] bool none() const noexcept {
        return !any();
    }

    // =========================================================================
    // Bitwise Set Operations
    // =========================================================================

    /// Bitwise AND
    [[nodiscard]] BitSet operator&(const BitSet& other) const {
        size_type result_len = std::min(len_, other.len_);
        BitSet result(result_len);
        size_type words = std::min(bits_.size(), other.bits_.size());
        for (size_type i = 0; i < words; ++i) {
            result.bits_[i] = bits_[i] & other.bits_[i];
        }
        return result;
    }

    /// Bitwise OR
    [[nodiscard]] BitSet operator|(const BitSet& other) const {
        size_type result_len = std::max(len_, other.len_);
        BitSet result(result_len);
        for (size_type i = 0; i < bits_.size(); ++i) {
            result.bits_[i] = bits_[i];
        }
        for (size_type i = 0; i < other.bits_.size(); ++i) {
            result.bits_[i] |= other.bits_[i];
        }
        return result;
    }

    /// Bitwise XOR
    [[nodiscard]] BitSet operator^(const BitSet& other) const {
        size_type result_len = std::max(len_, other.len_);
        BitSet result(result_len);
        for (size_type i = 0; i < bits_.size(); ++i) {
            result.bits_[i] = bits_[i];
        }
        for (size_type i = 0; i < other.bits_.size(); ++i) {
            result.bits_[i] ^= other.bits_[i];
        }
        return result;
    }

    /// Bitwise NOT
    [[nodiscard]] BitSet operator~() const {
        BitSet result(len_);
        for (size_type i = 0; i < bits_.size(); ++i) {
            result.bits_[i] = ~bits_[i];
        }
        // Clear bits beyond capacity
        if (len_ > 0) {
            size_type last_word_bits = len_ % BITS_PER_WORD;
            if (last_word_bits > 0) {
                result.bits_.back() &= (word_type(1) << last_word_bits) - 1;
            }
        }
        return result;
    }

    /// In-place AND
    BitSet& operator&=(const BitSet& other) {
        size_type words = std::min(bits_.size(), other.bits_.size());
        for (size_type i = 0; i < words; ++i) {
            bits_[i] &= other.bits_[i];
        }
        // Clear words beyond other's size
        for (size_type i = words; i < bits_.size(); ++i) {
            bits_[i] = 0;
        }
        return *this;
    }

    /// In-place OR
    BitSet& operator|=(const BitSet& other) {
        // Grow if needed
        if (other.len_ > len_) {
            resize(other.len_);
        }
        for (size_type i = 0; i < other.bits_.size(); ++i) {
            bits_[i] |= other.bits_[i];
        }
        return *this;
    }

    /// In-place XOR
    BitSet& operator^=(const BitSet& other) {
        if (other.len_ > len_) {
            resize(other.len_);
        }
        for (size_type i = 0; i < other.bits_.size(); ++i) {
            bits_[i] ^= other.bits_[i];
        }
        return *this;
    }

    // =========================================================================
    // Iterator over set bits
    // =========================================================================

    /// Iterator that yields indices of set bits
    class SetBitIterator {
        const BitSet* bitset_;
        size_type current_;

        void advance_to_next() {
            while (current_ < bitset_->len_ && !bitset_->get(current_)) {
                ++current_;
            }
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = size_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const size_type*;
        using reference = size_type;

        SetBitIterator(const BitSet* bs, size_type start) : bitset_(bs), current_(start) {
            advance_to_next();
        }

        size_type operator*() const { return current_; }

        SetBitIterator& operator++() {
            ++current_;
            advance_to_next();
            return *this;
        }

        SetBitIterator operator++(int) {
            SetBitIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const SetBitIterator& other) const {
            return current_ == other.current_;
        }

        bool operator!=(const SetBitIterator& other) const {
            return !(*this == other);
        }
    };

    /// Range for iterating over set bit indices
    class SetBitRange {
        const BitSet* bitset_;
    public:
        explicit SetBitRange(const BitSet* bs) : bitset_(bs) {}
        SetBitIterator begin() const { return SetBitIterator(bitset_, 0); }
        SetBitIterator end() const { return SetBitIterator(bitset_, bitset_->len_); }
    };

    /// Iterate over indices of set bits
    [[nodiscard]] SetBitRange iter_ones() const { return SetBitRange(this); }

    // =========================================================================
    // Direct Access
    // =========================================================================

    /// Direct access to raw word storage
    [[nodiscard]] const std::vector<word_type>& as_words() const noexcept {
        return bits_;
    }

    /// Mutable direct access to raw word storage
    [[nodiscard]] std::vector<word_type>& as_words() noexcept {
        return bits_;
    }

    // =========================================================================
    // Comparison
    // =========================================================================

    bool operator==(const BitSet& other) const noexcept {
        if (len_ != other.len_) return false;
        return bits_ == other.bits_;
    }

    bool operator!=(const BitSet& other) const noexcept {
        return !(*this == other);
    }
};

} // namespace void_structures
