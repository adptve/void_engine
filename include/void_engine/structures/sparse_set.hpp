#pragma once

/// @file sparse_set.hpp
/// @brief Cache-friendly sparse set for void_structures
///
/// SparseSet provides O(1) operations with cache-friendly iteration.
/// Values are stored contiguously in a dense array for fast iteration,
/// while a sparse array provides O(1) lookup by index.
/// Ideal for ECS component storage.

#include <vector>
#include <optional>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <span>

namespace void_structures {

/// Cache-friendly sparse set with stable external indices
/// @tparam T Stored value type
template<typename T>
class SparseSet {
public:
    using value_type = T;
    using size_type = std::size_t;
    using index_type = std::size_t;

private:
    std::vector<std::optional<size_type>> sparse_;  // External index -> dense index
    std::vector<T> dense_;                           // Contiguous value storage
    std::vector<index_type> indices_;                // Dense index -> external index

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create empty sparse set
    SparseSet() = default;

    /// Create with initial capacities
    /// @param sparse_capacity Initial sparse array capacity
    /// @param dense_capacity Initial dense array capacity
    SparseSet(size_type sparse_capacity, size_type dense_capacity) {
        sparse_.reserve(sparse_capacity);
        dense_.reserve(dense_capacity);
        indices_.reserve(dense_capacity);
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Number of stored elements
    [[nodiscard]] size_type size() const noexcept { return dense_.size(); }

    /// Alias for size()
    [[nodiscard]] size_type len() const noexcept { return size(); }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept { return dense_.empty(); }

    /// Alias for empty()
    [[nodiscard]] bool is_empty() const noexcept { return empty(); }

    /// Sparse array capacity (maximum index + 1)
    [[nodiscard]] size_type sparse_capacity() const noexcept { return sparse_.size(); }

    /// Dense array capacity
    [[nodiscard]] size_type dense_capacity() const noexcept { return dense_.capacity(); }

    // =========================================================================
    // Core Operations
    // =========================================================================

    /// Insert value at sparse index
    /// @param index External sparse index
    /// @param value Value to store
    /// @return Old value if updating, nullopt if new insertion
    std::optional<T> insert(index_type index, T value) {
        // Grow sparse array if needed
        if (index >= sparse_.size()) {
            sparse_.resize(index + 1, std::nullopt);
        }

        if (sparse_[index].has_value()) {
            // Update existing value
            size_type dense_idx = *sparse_[index];
            T old_value = std::move(dense_[dense_idx]);
            dense_[dense_idx] = std::move(value);
            return old_value;
        } else {
            // Insert new value
            size_type dense_idx = dense_.size();
            sparse_[index] = dense_idx;
            dense_.push_back(std::move(value));
            indices_.push_back(index);
            return std::nullopt;
        }
    }

    /// Insert with in-place construction
    template<typename... Args>
    std::optional<T> emplace(index_type index, Args&&... args) {
        if (index >= sparse_.size()) {
            sparse_.resize(index + 1, std::nullopt);
        }

        if (sparse_[index].has_value()) {
            size_type dense_idx = *sparse_[index];
            T old_value = std::move(dense_[dense_idx]);
            dense_[dense_idx] = T(std::forward<Args>(args)...);
            return old_value;
        } else {
            size_type dense_idx = dense_.size();
            sparse_[index] = dense_idx;
            dense_.emplace_back(std::forward<Args>(args)...);
            indices_.push_back(index);
            return std::nullopt;
        }
    }

    /// Remove value by sparse index using swap-remove
    /// @return Removed value if present
    std::optional<T> remove(index_type index) {
        if (!contains(index)) {
            return std::nullopt;
        }

        size_type dense_idx = *sparse_[index];
        size_type last_dense_idx = dense_.size() - 1;

        // Get value to return
        T value = std::move(dense_[dense_idx]);

        if (dense_idx != last_dense_idx) {
            // Swap with last element
            index_type last_sparse_idx = indices_[last_dense_idx];
            dense_[dense_idx] = std::move(dense_[last_dense_idx]);
            indices_[dense_idx] = last_sparse_idx;
            sparse_[last_sparse_idx] = dense_idx;
        }

        // Remove last element
        dense_.pop_back();
        indices_.pop_back();
        sparse_[index] = std::nullopt;

        return value;
    }

    /// Remove without returning value
    /// @return true if value was present and removed
    bool erase(index_type index) {
        return remove(index).has_value();
    }

    /// Remove all elements
    void clear() {
        for (auto& entry : sparse_) {
            entry = std::nullopt;
        }
        dense_.clear();
        indices_.clear();
    }

    // =========================================================================
    // Lookup
    // =========================================================================

    /// Check if sparse index is present
    [[nodiscard]] bool contains(index_type index) const noexcept {
        return index < sparse_.size() && sparse_[index].has_value();
    }

    /// Get value by sparse index
    /// @return Pointer to value or nullptr
    [[nodiscard]] const T* get(index_type index) const noexcept {
        if (!contains(index)) {
            return nullptr;
        }
        return &dense_[*sparse_[index]];
    }

    /// Get mutable value by sparse index
    [[nodiscard]] T* get(index_type index) noexcept {
        if (!contains(index)) {
            return nullptr;
        }
        return &dense_[*sparse_[index]];
    }

    /// Get value (throws if not present)
    [[nodiscard]] const T& at(index_type index) const {
        if (!contains(index)) {
            throw std::out_of_range("SparseSet: index not present");
        }
        return dense_[*sparse_[index]];
    }

    /// Get mutable value (throws if not present)
    [[nodiscard]] T& at(index_type index) {
        if (!contains(index)) {
            throw std::out_of_range("SparseSet: index not present");
        }
        return dense_[*sparse_[index]];
    }

    /// Get dense index for sparse index
    /// @return Dense index or nullopt if not present
    [[nodiscard]] std::optional<size_type> dense_index_of(index_type sparse_index) const noexcept {
        if (!contains(sparse_index)) {
            return std::nullopt;
        }
        return sparse_[sparse_index];
    }

    // =========================================================================
    // Direct Array Access (for SIMD and bulk operations)
    // =========================================================================

    /// Direct access to dense value array
    [[nodiscard]] std::span<const T> as_slice() const noexcept {
        return {dense_.data(), dense_.size()};
    }

    /// Mutable direct access to dense value array
    [[nodiscard]] std::span<T> as_mut_slice() noexcept {
        return {dense_.data(), dense_.size()};
    }

    /// Direct access to indices array
    [[nodiscard]] std::span<const index_type> indices_slice() const noexcept {
        return {indices_.data(), indices_.size()};
    }

    /// Get raw pointer to dense data
    [[nodiscard]] const T* data() const noexcept { return dense_.data(); }

    /// Get mutable raw pointer to dense data
    [[nodiscard]] T* data() noexcept { return dense_.data(); }

    // =========================================================================
    // Iterators (dense order for cache efficiency)
    // =========================================================================

    /// Iterator yielding (sparse_index, value&) pairs
    template<bool IsConst>
    class Iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using dense_vec = std::conditional_t<IsConst, const std::vector<T>, std::vector<T>>;
        using value_ref = std::conditional_t<IsConst, const T&, T&>;
        using pair_type = std::pair<index_type, value_ref>;

    private:
        dense_vec* dense_;
        const std::vector<index_type>* indices_;
        size_type pos_;

    public:
        Iterator(dense_vec* d, const std::vector<index_type>* i, size_type p)
            : dense_(d), indices_(i), pos_(p) {}

        pair_type operator*() const {
            return {(*indices_)[pos_], (*dense_)[pos_]};
        }

        Iterator& operator++() { ++pos_; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++pos_; return tmp; }
        Iterator& operator--() { --pos_; return *this; }
        Iterator operator--(int) { Iterator tmp = *this; --pos_; return tmp; }

        Iterator& operator+=(difference_type n) { pos_ += n; return *this; }
        Iterator& operator-=(difference_type n) { pos_ -= n; return *this; }

        Iterator operator+(difference_type n) const { return Iterator(dense_, indices_, pos_ + n); }
        Iterator operator-(difference_type n) const { return Iterator(dense_, indices_, pos_ - n); }
        difference_type operator-(const Iterator& other) const {
            return static_cast<difference_type>(pos_) - static_cast<difference_type>(other.pos_);
        }

        bool operator==(const Iterator& other) const { return pos_ == other.pos_; }
        bool operator!=(const Iterator& other) const { return pos_ != other.pos_; }
        bool operator<(const Iterator& other) const { return pos_ < other.pos_; }
        bool operator<=(const Iterator& other) const { return pos_ <= other.pos_; }
        bool operator>(const Iterator& other) const { return pos_ > other.pos_; }
        bool operator>=(const Iterator& other) const { return pos_ >= other.pos_; }
    };

    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    iterator begin() { return iterator(&dense_, &indices_, 0); }
    iterator end() { return iterator(&dense_, &indices_, dense_.size()); }
    const_iterator begin() const { return const_iterator(&dense_, &indices_, 0); }
    const_iterator end() const { return const_iterator(&dense_, &indices_, dense_.size()); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

    // =========================================================================
    // Value-only iteration
    // =========================================================================

    /// Iterator for dense values only
    auto values() noexcept { return std::span<T>(dense_); }
    auto values() const noexcept { return std::span<const T>(dense_); }

    /// Iterator for sparse indices only (in dense order)
    auto indices() const noexcept { return std::span<const index_type>(indices_); }

    // =========================================================================
    // Utility
    // =========================================================================

    /// Sort the dense array by sparse index (for deterministic iteration)
    void sort_by_index() {
        // Create index permutation
        std::vector<size_type> perm(dense_.size());
        for (size_type i = 0; i < perm.size(); ++i) {
            perm[i] = i;
        }
        std::sort(perm.begin(), perm.end(), [this](size_type a, size_type b) {
            return indices_[a] < indices_[b];
        });

        // Apply permutation to dense and indices arrays
        std::vector<T> new_dense;
        std::vector<index_type> new_indices;
        new_dense.reserve(dense_.size());
        new_indices.reserve(indices_.size());

        for (size_type i : perm) {
            new_dense.push_back(std::move(dense_[i]));
            new_indices.push_back(indices_[i]);
        }

        dense_ = std::move(new_dense);
        indices_ = std::move(new_indices);

        // Update sparse array
        for (size_type dense_idx = 0; dense_idx < indices_.size(); ++dense_idx) {
            sparse_[indices_[dense_idx]] = dense_idx;
        }
    }
};

} // namespace void_structures
