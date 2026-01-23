#pragma once

/// @file slot_map.hpp
/// @brief Generational index-based storage for void_structures
///
/// SlotMap provides O(1) insertion, removal, and lookup with use-after-free
/// detection through generational indices. Ideal for entity storage in ECS
/// systems and asset handle management.

#include <vector>
#include <optional>
#include <cstdint>
#include <functional>
#include <cassert>
#include <limits>
#include <iterator>

namespace void_structures {

// =============================================================================
// SlotKey - Generational Key
// =============================================================================

/// Generational key with use-after-free detection
/// @tparam T Value type (provides compile-time type safety, not stored)
template<typename T>
struct SlotKey {
    std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t generation = 0;

    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor - creates null key
    constexpr SlotKey() noexcept = default;

    /// Create key with specific index and generation
    constexpr SlotKey(std::uint32_t idx, std::uint32_t gen) noexcept
        : index(idx), generation(gen) {}

    /// Create a null/invalid key
    [[nodiscard]] static constexpr SlotKey null() noexcept {
        return SlotKey{};
    }

    // =========================================================================
    // Queries
    // =========================================================================

    /// Check if key is null/invalid
    [[nodiscard]] constexpr bool is_null() const noexcept {
        return index == std::numeric_limits<std::uint32_t>::max();
    }

    /// Check if key is valid (not null)
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return !is_null();
    }

    /// Get the raw index
    [[nodiscard]] constexpr std::uint32_t get_index() const noexcept {
        return index;
    }

    /// Get the generation counter
    [[nodiscard]] constexpr std::uint32_t get_generation() const noexcept {
        return generation;
    }

    // =========================================================================
    // Operators
    // =========================================================================

    constexpr bool operator==(const SlotKey& other) const noexcept {
        return index == other.index && generation == other.generation;
    }

    constexpr bool operator!=(const SlotKey& other) const noexcept {
        return !(*this == other);
    }

    /// Explicit bool conversion (true if valid)
    explicit constexpr operator bool() const noexcept {
        return is_valid();
    }
};

} // namespace void_structures

// =============================================================================
// std::hash specialization for SlotKey
// =============================================================================

namespace std {

template<typename T>
struct hash<void_structures::SlotKey<T>> {
    std::size_t operator()(const void_structures::SlotKey<T>& key) const noexcept {
        // Combine index and generation using FNV-like mixing
        std::size_t h = static_cast<std::size_t>(key.index);
        h ^= static_cast<std::size_t>(key.generation) << 16;
        h ^= static_cast<std::size_t>(key.generation) >> 16;
        return h;
    }
};

} // namespace std

namespace void_structures {

// =============================================================================
// SlotMap - Generational Index Storage
// =============================================================================

/// Generational index-based storage with O(1) operations
/// @tparam T Stored value type
template<typename T>
class SlotMap {
public:
    using key_type = SlotKey<T>;
    using value_type = T;
    using size_type = std::size_t;

private:
    /// Internal slot storage
    struct Slot {
        std::uint32_t generation = 0;
        bool occupied = false;
        // Use aligned storage to support types without default constructors
        alignas(T) unsigned char storage[sizeof(T)];

        T* get_ptr() noexcept {
            return reinterpret_cast<T*>(storage);
        }

        const T* get_ptr() const noexcept {
            return reinterpret_cast<const T*>(storage);
        }

        template<typename... Args>
        void construct(Args&&... args) {
            new (storage) T(std::forward<Args>(args)...);
            occupied = true;
        }

        void destroy() {
            if (occupied) {
                get_ptr()->~T();
                occupied = false;
            }
        }
    };

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_list_;
    size_type len_ = 0;

public:
    // =========================================================================
    // Constructors / Destructor
    // =========================================================================

    /// Create empty slot map
    SlotMap() = default;

    /// Create with preallocated capacity
    explicit SlotMap(size_type capacity) {
        slots_.reserve(capacity);
    }

    /// Destructor - destroys all stored values
    ~SlotMap() {
        clear();
    }

    /// Move constructor
    SlotMap(SlotMap&& other) noexcept
        : slots_(std::move(other.slots_))
        , free_list_(std::move(other.free_list_))
        , len_(other.len_) {
        other.len_ = 0;
    }

    /// Move assignment
    SlotMap& operator=(SlotMap&& other) noexcept {
        if (this != &other) {
            clear();
            slots_ = std::move(other.slots_);
            free_list_ = std::move(other.free_list_);
            len_ = other.len_;
            other.len_ = 0;
        }
        return *this;
    }

    // Disable copy (expensive and usually not intended)
    SlotMap(const SlotMap&) = delete;
    SlotMap& operator=(const SlotMap&) = delete;

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Number of active elements
    [[nodiscard]] size_type size() const noexcept { return len_; }

    /// Alias for size()
    [[nodiscard]] size_type len() const noexcept { return len_; }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept { return len_ == 0; }

    /// Alias for empty()
    [[nodiscard]] bool is_empty() const noexcept { return empty(); }

    /// Total allocated capacity
    [[nodiscard]] size_type capacity() const noexcept { return slots_.capacity(); }

    /// Reserve space for additional elements
    void reserve(size_type additional) {
        slots_.reserve(slots_.size() + additional);
    }

    // =========================================================================
    // Insertion
    // =========================================================================

    /// Insert a value and return its key
    /// @return Key for the inserted value (O(1) amortized)
    key_type insert(T value) {
        if (!free_list_.empty()) {
            // Reuse a freed slot
            std::uint32_t idx = free_list_.back();
            free_list_.pop_back();

            Slot& slot = slots_[idx];
            slot.construct(std::move(value));
            ++len_;

            return key_type(idx, slot.generation);
        } else {
            // Allocate new slot
            std::uint32_t idx = static_cast<std::uint32_t>(slots_.size());
            slots_.emplace_back();
            Slot& slot = slots_.back();
            slot.construct(std::move(value));
            ++len_;

            return key_type(idx, slot.generation);
        }
    }

    /// Insert with in-place construction
    template<typename... Args>
    key_type emplace(Args&&... args) {
        if (!free_list_.empty()) {
            std::uint32_t idx = free_list_.back();
            free_list_.pop_back();

            Slot& slot = slots_[idx];
            slot.construct(std::forward<Args>(args)...);
            ++len_;

            return key_type(idx, slot.generation);
        } else {
            std::uint32_t idx = static_cast<std::uint32_t>(slots_.size());
            slots_.emplace_back();
            Slot& slot = slots_.back();
            slot.construct(std::forward<Args>(args)...);
            ++len_;

            return key_type(idx, slot.generation);
        }
    }

    // =========================================================================
    // Removal
    // =========================================================================

    /// Remove value by key
    /// @return The removed value if key was valid
    std::optional<T> remove(key_type key) {
        if (!contains_key(key)) {
            return std::nullopt;
        }

        Slot& slot = slots_[key.index];
        T value = std::move(*slot.get_ptr());
        slot.destroy();
        slot.generation++;  // Increment generation to invalidate old keys
        free_list_.push_back(key.index);
        --len_;

        return value;
    }

    /// Remove value by key without returning it
    /// @return true if key was valid and value was removed
    bool erase(key_type key) {
        if (!contains_key(key)) {
            return false;
        }

        Slot& slot = slots_[key.index];
        slot.destroy();
        slot.generation++;
        free_list_.push_back(key.index);
        --len_;

        return true;
    }

    /// Remove all elements
    void clear() {
        for (auto& slot : slots_) {
            if (slot.occupied) {
                slot.destroy();
                slot.generation++;
            }
        }
        free_list_.clear();
        // Add all slots to free list
        for (std::uint32_t i = 0; i < slots_.size(); ++i) {
            free_list_.push_back(i);
        }
        len_ = 0;
    }

    // =========================================================================
    // Lookup
    // =========================================================================

    /// Check if key is valid
    [[nodiscard]] bool contains_key(key_type key) const noexcept {
        if (key.is_null() || key.index >= slots_.size()) {
            return false;
        }
        const Slot& slot = slots_[key.index];
        return slot.occupied && slot.generation == key.generation;
    }

    /// Get immutable reference to value
    /// @return Pointer to value or nullptr if key invalid
    [[nodiscard]] const T* get(key_type key) const noexcept {
        if (!contains_key(key)) {
            return nullptr;
        }
        return slots_[key.index].get_ptr();
    }

    /// Get mutable reference to value
    /// @return Pointer to value or nullptr if key invalid
    [[nodiscard]] T* get(key_type key) noexcept {
        if (!contains_key(key)) {
            return nullptr;
        }
        return slots_[key.index].get_ptr();
    }

    /// Get immutable reference (throws if invalid)
    [[nodiscard]] const T& at(key_type key) const {
        const T* ptr = get(key);
        if (!ptr) {
            throw std::out_of_range("SlotMap: invalid key");
        }
        return *ptr;
    }

    /// Get mutable reference (throws if invalid)
    [[nodiscard]] T& at(key_type key) {
        T* ptr = get(key);
        if (!ptr) {
            throw std::out_of_range("SlotMap: invalid key");
        }
        return *ptr;
    }

    /// Index operator (asserts on invalid key)
    [[nodiscard]] const T& operator[](key_type key) const {
        assert(contains_key(key) && "SlotMap: invalid key");
        return *slots_[key.index].get_ptr();
    }

    /// Index operator (asserts on invalid key)
    [[nodiscard]] T& operator[](key_type key) {
        assert(contains_key(key) && "SlotMap: invalid key");
        return *slots_[key.index].get_ptr();
    }

    // =========================================================================
    // Iterators
    // =========================================================================

    /// Iterator for SlotMap that yields (key, value&) pairs
    template<bool IsConst>
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using slot_vec = std::conditional_t<IsConst, const std::vector<Slot>, std::vector<Slot>>;
        using value_ref = std::conditional_t<IsConst, const T&, T&>;
        using pair_type = std::pair<key_type, value_ref>;

    private:
        slot_vec* slots_;
        std::uint32_t index_;

        void skip_empty() {
            while (index_ < slots_->size() && !(*slots_)[index_].occupied) {
                ++index_;
            }
        }

    public:
        Iterator(slot_vec* slots, std::uint32_t start)
            : slots_(slots), index_(start) {
            skip_empty();
        }

        pair_type operator*() const {
            const Slot& slot = (*slots_)[index_];
            return {key_type(index_, slot.generation), *const_cast<Slot&>(slot).get_ptr()};
        }

        Iterator& operator++() {
            ++index_;
            skip_empty();
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return slots_ == other.slots_ && index_ == other.index_;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
    };

    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    iterator begin() { return iterator(&slots_, 0); }
    iterator end() { return iterator(&slots_, static_cast<std::uint32_t>(slots_.size())); }
    const_iterator begin() const { return const_iterator(&slots_, 0); }
    const_iterator end() const { return const_iterator(&slots_, static_cast<std::uint32_t>(slots_.size())); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

    // =========================================================================
    // Range-based helpers
    // =========================================================================

    /// Iterate over keys only
    class KeyRange {
        const SlotMap* map_;
    public:
        explicit KeyRange(const SlotMap* m) : map_(m) {}

        class Iterator {
            const std::vector<Slot>* slots_;
            std::uint32_t index_;
            void skip_empty() {
                while (index_ < slots_->size() && !(*slots_)[index_].occupied) {
                    ++index_;
                }
            }
        public:
            Iterator(const std::vector<Slot>* s, std::uint32_t i) : slots_(s), index_(i) { skip_empty(); }
            key_type operator*() const {
                return key_type(index_, (*slots_)[index_].generation);
            }
            Iterator& operator++() { ++index_; skip_empty(); return *this; }
            bool operator!=(const Iterator& o) const { return index_ != o.index_; }
        };

        Iterator begin() const { return Iterator(&map_->slots_, 0); }
        Iterator end() const { return Iterator(&map_->slots_, static_cast<std::uint32_t>(map_->slots_.size())); }
    };

    [[nodiscard]] KeyRange keys() const { return KeyRange(this); }

    /// Iterate over values only
    class ValueRange {
        SlotMap* map_;
    public:
        explicit ValueRange(SlotMap* m) : map_(m) {}

        class Iterator {
            std::vector<Slot>* slots_;
            std::uint32_t index_;
            void skip_empty() {
                while (index_ < slots_->size() && !(*slots_)[index_].occupied) {
                    ++index_;
                }
            }
        public:
            Iterator(std::vector<Slot>* s, std::uint32_t i) : slots_(s), index_(i) { skip_empty(); }
            T& operator*() const { return *(*slots_)[index_].get_ptr(); }
            Iterator& operator++() { ++index_; skip_empty(); return *this; }
            bool operator!=(const Iterator& o) const { return index_ != o.index_; }
        };

        Iterator begin() const { return Iterator(&map_->slots_, 0); }
        Iterator end() const { return Iterator(&map_->slots_, static_cast<std::uint32_t>(map_->slots_.size())); }
    };

    [[nodiscard]] ValueRange values() { return ValueRange(this); }

    /// Const iterate over values only
    class ConstValueRange {
        const SlotMap* map_;
    public:
        explicit ConstValueRange(const SlotMap* m) : map_(m) {}

        class Iterator {
            const std::vector<Slot>* slots_;
            std::uint32_t index_;
            void skip_empty() {
                while (index_ < slots_->size() && !(*slots_)[index_].occupied) {
                    ++index_;
                }
            }
        public:
            Iterator(const std::vector<Slot>* s, std::uint32_t i) : slots_(s), index_(i) { skip_empty(); }
            const T& operator*() const { return *(*slots_)[index_].get_ptr(); }
            Iterator& operator++() { ++index_; skip_empty(); return *this; }
            bool operator!=(const Iterator& o) const { return index_ != o.index_; }
        };

        Iterator begin() const { return Iterator(&map_->slots_, 0); }
        Iterator end() const { return Iterator(&map_->slots_, static_cast<std::uint32_t>(map_->slots_.size())); }
    };

    [[nodiscard]] ConstValueRange values() const { return ConstValueRange(this); }
};

} // namespace void_structures
