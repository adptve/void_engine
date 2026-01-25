#pragma once

/// @file handle.hpp
/// @brief Type-safe generational handles for void_core

#include "fwd.hpp"
#include "error.hpp"
#include <cstdint>
#include <vector>
#include <optional>
#include <atomic>
#include <functional>
#include <cassert>
#include <ostream>

namespace void_core {

// =============================================================================
// Handle Constants
// =============================================================================

namespace handle_constants {
    /// Maximum index value (24 bits)
    constexpr std::uint32_t MAX_INDEX = (1u << 24) - 1;

    /// Maximum generation value (8 bits)
    constexpr std::uint8_t MAX_GENERATION = UINT8_MAX;

    /// Null handle bits
    constexpr std::uint32_t NULL_BITS = UINT32_MAX;
}

// =============================================================================
// Handle<T>
// =============================================================================

/// Type-safe generational index handle
/// Layout: [Generation(8 bits) | Index(24 bits)]
template<typename T>
struct Handle {
    std::uint32_t bits = handle_constants::NULL_BITS;

    /// Default constructor (null handle)
    constexpr Handle() noexcept = default;

    /// Construct from index and generation
    [[nodiscard]] static constexpr Handle create(std::uint32_t index, std::uint8_t generation) noexcept {
        Handle h;
        h.bits = (static_cast<std::uint32_t>(generation) << 24) | (index & handle_constants::MAX_INDEX);
        return h;
    }

    /// Create null handle
    [[nodiscard]] static constexpr Handle null() noexcept {
        return Handle{};
    }

    /// Check if null
    [[nodiscard]] constexpr bool is_null() const noexcept {
        return bits == handle_constants::NULL_BITS;
    }

    /// Check if valid (not null)
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return bits != handle_constants::NULL_BITS;
    }

    /// Get index component
    [[nodiscard]] constexpr std::uint32_t index() const noexcept {
        return bits & handle_constants::MAX_INDEX;
    }

    /// Get generation component
    [[nodiscard]] constexpr std::uint8_t generation() const noexcept {
        return static_cast<std::uint8_t>(bits >> 24);
    }

    /// Get raw bits
    [[nodiscard]] constexpr std::uint32_t to_bits() const noexcept {
        return bits;
    }

    /// Create from raw bits
    [[nodiscard]] static constexpr Handle from_bits(std::uint32_t raw) noexcept {
        Handle h;
        h.bits = raw;
        return h;
    }

    /// Cast to different handle type (unsafe)
    template<typename U>
    [[nodiscard]] constexpr Handle<U> cast() const noexcept {
        return Handle<U>::from_bits(bits);
    }

    /// Comparison operators
    constexpr bool operator==(const Handle&) const noexcept = default;
    constexpr bool operator!=(const Handle&) const noexcept = default;

    /// Explicit bool conversion
    explicit constexpr operator bool() const noexcept {
        return is_valid();
    }
};

// =============================================================================
// HandleAllocator<T>
// =============================================================================

/// Manages allocation and generation tracking for handles
template<typename T>
class HandleAllocator {
public:
    /// Constructor
    HandleAllocator() = default;

    /// Constructor with reserved capacity
    explicit HandleAllocator(std::size_t capacity) {
        m_generations.reserve(capacity);
        m_free_list.reserve(capacity);
    }

    /// Allocate new handle
    [[nodiscard]] Handle<T> allocate() {
        std::uint32_t index;
        std::uint8_t generation;

        if (!m_free_list.empty()) {
            // Reuse freed slot
            index = m_free_list.back();
            m_free_list.pop_back();
            generation = m_generations[index];
        } else {
            // Create fresh slot
            index = static_cast<std::uint32_t>(m_generations.size());
            if (index > handle_constants::MAX_INDEX) {
                // Overflow - return null handle
                return Handle<T>::null();
            }
            m_generations.push_back(0);
            generation = 0;
        }

        return Handle<T>::create(index, generation);
    }

    /// Free handle (returns true if successful)
    bool free(Handle<T> handle) {
        if (handle.is_null()) {
            return false;
        }

        std::uint32_t index = handle.index();
        if (index >= m_generations.size()) {
            return false;
        }

        // Verify generation matches
        if (m_generations[index] != handle.generation()) {
            return false;  // Stale handle
        }

        // Increment generation (wrapping)
        m_generations[index] = static_cast<std::uint8_t>(
            (static_cast<std::uint16_t>(m_generations[index]) + 1) % 256
        );

        m_free_list.push_back(index);
        return true;
    }

    /// Check if handle is valid
    [[nodiscard]] bool is_valid(Handle<T> handle) const {
        if (handle.is_null()) {
            return false;
        }

        std::uint32_t index = handle.index();
        if (index >= m_generations.size()) {
            return false;
        }

        return m_generations[index] == handle.generation();
    }

    /// Get current generation for index
    [[nodiscard]] std::uint8_t generation_at(std::uint32_t index) const {
        if (index >= m_generations.size()) {
            return 0;
        }
        return m_generations[index];
    }

    /// Get allocated count (total slots minus free)
    [[nodiscard]] std::size_t len() const noexcept {
        return m_generations.size() - m_free_list.size();
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return len() == 0;
    }

    /// Get capacity (total slots including freed)
    [[nodiscard]] std::size_t capacity() const noexcept {
        return m_generations.size();
    }

    /// Get free count
    [[nodiscard]] std::size_t free_count() const noexcept {
        return m_free_list.size();
    }

    /// Clear all allocations
    void clear() {
        m_generations.clear();
        m_free_list.clear();
    }

    /// Reserve capacity
    void reserve(std::size_t capacity) {
        m_generations.reserve(capacity);
        m_free_list.reserve(capacity);
    }

private:
    std::vector<std::uint8_t> m_generations;
    std::vector<std::uint32_t> m_free_list;
};

// =============================================================================
// HandleMap<T>
// =============================================================================

/// Storage container pairing allocator with values
template<typename T>
class HandleMap {
public:
    /// Constructor
    HandleMap() = default;

    /// Constructor with reserved capacity
    explicit HandleMap(std::size_t capacity) {
        m_allocator.reserve(capacity);
        m_values.reserve(capacity);
    }

    /// Insert value and get handle
    [[nodiscard]] Handle<T> insert(T value) {
        Handle<T> handle = m_allocator.allocate();
        if (handle.is_null()) {
            return handle;
        }

        std::uint32_t index = handle.index();

        // Ensure values vector is large enough
        if (index >= m_values.size()) {
            m_values.resize(index + 1);
        }

        m_values[index] = std::move(value);
        return handle;
    }

    /// Remove value by handle
    [[nodiscard]] std::optional<T> remove(Handle<T> handle) {
        if (!m_allocator.is_valid(handle)) {
            return std::nullopt;
        }

        std::uint32_t index = handle.index();
        std::optional<T> result = std::move(m_values[index]);
        m_values[index].reset();

        m_allocator.free(handle);
        return result;
    }

    /// Get value by handle (const)
    [[nodiscard]] const T* get(Handle<T> handle) const {
        if (!m_allocator.is_valid(handle)) {
            return nullptr;
        }
        std::uint32_t index = handle.index();
        if (index >= m_values.size() || !m_values[index].has_value()) {
            return nullptr;
        }
        return &m_values[index].value();
    }

    /// Get value by handle (mutable)
    [[nodiscard]] T* get_mut(Handle<T> handle) {
        if (!m_allocator.is_valid(handle)) {
            return nullptr;
        }
        std::uint32_t index = handle.index();
        if (index >= m_values.size() || !m_values[index].has_value()) {
            return nullptr;
        }
        return &m_values[index].value();
    }

    /// Check if handle is valid
    [[nodiscard]] bool contains(Handle<T> handle) const {
        return m_allocator.is_valid(handle) &&
               handle.index() < m_values.size() &&
               m_values[handle.index()].has_value();
    }

    /// Get count
    [[nodiscard]] std::size_t len() const noexcept {
        return m_allocator.len();
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return m_allocator.is_empty();
    }

    /// Clear all
    void clear() {
        m_allocator.clear();
        m_values.clear();
    }

    /// Reserve capacity
    void reserve(std::size_t capacity) {
        m_allocator.reserve(capacity);
        m_values.reserve(capacity);
    }

    /// Get allocator
    [[nodiscard]] const HandleAllocator<T>& allocator() const noexcept {
        return m_allocator;
    }

    /// Iterate over all valid entries
    template<typename F>
    void for_each(F&& func) const {
        for (std::size_t i = 0; i < m_values.size(); ++i) {
            if (m_values[i].has_value()) {
                std::uint8_t gen = m_allocator.generation_at(static_cast<std::uint32_t>(i));
                Handle<T> handle = Handle<T>::create(static_cast<std::uint32_t>(i), gen);
                if (m_allocator.is_valid(handle)) {
                    func(handle, m_values[i].value());
                }
            }
        }
    }

    /// Iterate over all valid entries (mutable)
    template<typename F>
    void for_each_mut(F&& func) {
        for (std::size_t i = 0; i < m_values.size(); ++i) {
            if (m_values[i].has_value()) {
                std::uint8_t gen = m_allocator.generation_at(static_cast<std::uint32_t>(i));
                Handle<T> handle = Handle<T>::create(static_cast<std::uint32_t>(i), gen);
                if (m_allocator.is_valid(handle)) {
                    func(handle, m_values[i].value());
                }
            }
        }
    }

    /// Get result wrapper with error handling
    [[nodiscard]] Result<std::reference_wrapper<const T>> get_result(Handle<T> handle) const {
        if (handle.is_null()) {
            return Err<std::reference_wrapper<const T>>(HandleError::null());
        }
        if (!m_allocator.is_valid(handle)) {
            return Err<std::reference_wrapper<const T>>(HandleError::stale());
        }
        const T* ptr = get(handle);
        if (!ptr) {
            return Err<std::reference_wrapper<const T>>(HandleError::out_of_bounds());
        }
        return Ok(std::cref(*ptr));
    }

private:
    HandleAllocator<T> m_allocator;
    std::vector<std::optional<T>> m_values;
};

/// Output stream operator for Handle
template<typename T>
std::ostream& operator<<(std::ostream& os, const Handle<T>& h) {
    if (h.is_null()) {
        return os << "Handle<T>(null)";
    }
    return os << "Handle<T>(" << h.index() << "v" << static_cast<int>(h.generation()) << ")";
}

// =============================================================================
// Handle Pool Statistics (Implemented in handle.cpp)
// =============================================================================

/// Statistics for a handle allocator
struct HandlePoolStats {
    std::size_t total_allocated = 0;      // Total slots ever allocated
    std::size_t active_count = 0;         // Currently active handles
    std::size_t free_count = 0;           // Handles in free list
    std::size_t peak_active = 0;          // Peak concurrent active handles
    float fragmentation_ratio = 0.0f;     // free_count / total_allocated
};

/// Compute statistics for a handle allocator
template<typename T>
HandlePoolStats compute_pool_stats(const HandleAllocator<T>& allocator);

// =============================================================================
// Handle Serialization (Implemented in handle.cpp)
// =============================================================================

namespace serialization {

/// Serialize a handle to binary
template<typename T>
std::vector<std::uint8_t> serialize_handle(Handle<T> handle);

/// Deserialize a handle from binary
template<typename T>
Result<Handle<T>> deserialize_handle(const std::vector<std::uint8_t>& data);

} // namespace serialization

// =============================================================================
// Handle Compaction (Implemented in handle.cpp)
// =============================================================================

/// Compaction result for handle allocators
struct CompactionResult {
    std::size_t handles_moved = 0;
    std::size_t bytes_saved = 0;
    bool success = false;
};

/// Compact a HandleMap by removing gaps in storage
/// NOTE: This invalidates all handles! Only use during controlled shutdown/reload.
template<typename T>
CompactionResult compact_handle_map(HandleMap<T>& map);

// =============================================================================
// Debug Utilities (Implemented in handle.cpp)
// =============================================================================

namespace debug {

/// Format a raw handle value for debugging
std::string format_handle_bits(std::uint32_t bits);

/// Handle validation result
struct HandleValidation {
    bool is_valid = false;
    bool index_in_range = false;
    bool generation_matches = false;
    std::string error_message;
};

/// Validate a handle against allocator state
template<typename T>
HandleValidation validate_handle(Handle<T> handle, const HandleAllocator<T>& allocator);

} // namespace debug

} // namespace void_core

/// Hash specialization for Handle
template<typename T>
struct std::hash<void_core::Handle<T>> {
    std::size_t operator()(const void_core::Handle<T>& h) const noexcept {
        return std::hash<std::uint32_t>{}(h.bits);
    }
};
