#pragma once

/// @file arena.hpp
/// @brief Arena allocator - fast linear allocation with bulk deallocation

#include "allocator.hpp"
#include <atomic>
#include <vector>
#include <cstring>

namespace void_memory {

/// Saved arena state for scoped allocations
struct ArenaState {
    std::size_t offset = 0;
};

/// Arena allocator - extremely fast for temporary allocations
///
/// Allocations are served linearly from a contiguous buffer.
/// Individual deallocations are not supported - only bulk reset.
/// Thread-safe via atomic offset.
class Arena : public IAllocator {
public:
    using State = ArenaState;

    /// Create a new arena with the given capacity in bytes
    explicit Arena(std::size_t capacity)
        : m_buffer(capacity)
        , m_offset(0)
        , m_capacity(capacity) {}

    /// Create with capacity in KB
    [[nodiscard]] static Arena with_capacity_kb(std::size_t kb) {
        return Arena(kb * 1024);
    }

    /// Create with capacity in MB
    [[nodiscard]] static Arena with_capacity_mb(std::size_t mb) {
        return Arena(mb * 1024 * 1024);
    }

    // =========================================================================
    // IAllocator Interface
    // =========================================================================

    [[nodiscard]] void* allocate(std::size_t size, std::size_t align) override {
        if (size == 0) {
            return reinterpret_cast<void*>(align); // Non-null aligned dangling pointer
        }

        std::size_t current = m_offset.load(std::memory_order_relaxed);

        while (true) {
            const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(m_buffer.data());

            // Calculate aligned offset
            const std::size_t aligned_offset = align_up(base + current, align) - base;
            const std::size_t new_offset = aligned_offset + size;

            if (new_offset > m_capacity) {
                return nullptr; // Out of memory
            }

            // Try to claim this allocation (lock-free)
            if (m_offset.compare_exchange_weak(current, new_offset,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return m_buffer.data() + aligned_offset;
            }
            // Retry on contention
        }
    }

    void deallocate(void* /*ptr*/, std::size_t /*size*/, std::size_t /*align*/) override {
        // Arena doesn't support individual deallocation
    }

    void reset() override {
        m_offset.store(0, std::memory_order_release);
    }

    [[nodiscard]] std::size_t capacity() const noexcept override {
        return m_capacity;
    }

    [[nodiscard]] std::size_t used() const noexcept override {
        return m_offset.load(std::memory_order_relaxed);
    }

    // =========================================================================
    // Arena-Specific API
    // =========================================================================

    /// Allocate and initialize a single value
    template<typename T>
    [[nodiscard]] T* alloc(const T& value) {
        T* ptr = allocate_typed<T>(1);
        if (ptr) {
            new (ptr) T(value);
        }
        return ptr;
    }

    /// Allocate a slice and copy values
    template<typename T>
    [[nodiscard]] T* alloc_slice(const T* values, std::size_t count) {
        T* ptr = allocate_typed<T>(count);
        if (ptr && values) {
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memcpy(ptr, values, sizeof(T) * count);
            } else {
                for (std::size_t i = 0; i < count; ++i) {
                    new (ptr + i) T(values[i]);
                }
            }
        }
        return ptr;
    }

    /// Allocate zeroed memory
    template<typename T>
    [[nodiscard]] T* alloc_zeroed(std::size_t count) {
        T* ptr = allocate_typed<T>(count);
        if (ptr) {
            std::memset(ptr, 0, sizeof(T) * count);
        }
        return ptr;
    }

    /// Save the current state for later restore
    [[nodiscard]] ArenaState save() const noexcept {
        return ArenaState{m_offset.load(std::memory_order_acquire)};
    }

    /// Restore to a previously saved state
    void restore(const ArenaState& state) noexcept {
        m_offset.store(state.offset, std::memory_order_release);
    }

    /// Get the current offset
    [[nodiscard]] std::size_t current_offset() const noexcept {
        return m_offset.load(std::memory_order_relaxed);
    }

    /// Get raw buffer pointer (for debugging/inspection)
    [[nodiscard]] const std::uint8_t* data() const noexcept {
        return m_buffer.data();
    }

private:
    std::vector<std::uint8_t> m_buffer;
    std::atomic<std::size_t> m_offset;
    std::size_t m_capacity;
};

/// Scoped arena allocation guard - restores arena state on destruction
using ArenaScope = AllocatorScope<Arena>;

} // namespace void_memory
