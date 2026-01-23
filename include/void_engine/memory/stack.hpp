#pragma once

/// @file stack.hpp
/// @brief Stack allocator - LIFO allocation with markers

#include "allocator.hpp"
#include <atomic>
#include <vector>

namespace void_memory {

/// Marker for stack position
struct StackMarker {
    std::size_t offset = 0;
};

/// Stack allocator - LIFO allocation with markers
///
/// Fast allocator for temporary data that follows a stack pattern.
/// Supports both individual deallocation (LIFO order) and bulk rollback via markers.
class StackAllocator : public IAllocator {
public:
    using State = StackMarker;

    /// Create a new stack allocator
    explicit StackAllocator(std::size_t capacity)
        : m_buffer(capacity)
        , m_top(0)
        , m_capacity(capacity) {}

    /// Create with capacity in KB
    [[nodiscard]] static StackAllocator with_capacity_kb(std::size_t kb) {
        return StackAllocator(kb * 1024);
    }

    /// Create with capacity in MB
    [[nodiscard]] static StackAllocator with_capacity_mb(std::size_t mb) {
        return StackAllocator(mb * 1024 * 1024);
    }

    // =========================================================================
    // IAllocator Interface
    // =========================================================================

    [[nodiscard]] void* allocate(std::size_t size, std::size_t align) override {
        if (size == 0) {
            return reinterpret_cast<void*>(align);
        }

        const std::size_t total_align = std::max(align, HEADER_ALIGN);

        while (true) {
            std::size_t current_top = m_top.load(std::memory_order_relaxed);
            const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(m_buffer.data());

            // Calculate header and user pointer positions
            const std::size_t header_offset = align_up(current_top, HEADER_ALIGN);
            const std::size_t user_offset = align_up(header_offset + HEADER_SIZE, total_align);
            const std::size_t new_top = user_offset + size;

            if (new_top > m_capacity) {
                return nullptr;
            }

            // Try to claim this allocation
            if (m_top.compare_exchange_weak(current_top, new_top,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                // Write header
                auto* header = reinterpret_cast<StackHeader*>(m_buffer.data() + header_offset);
                header->previous_top = current_top;
                header->allocation_end = new_top;

                return m_buffer.data() + user_offset;
            }
            // Retry on contention
        }
    }

    void deallocate(void* ptr, std::size_t /*size*/, std::size_t /*align*/) override {
        if (!ptr) return;

        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(m_buffer.data());
        const std::uintptr_t user_ptr = reinterpret_cast<std::uintptr_t>(ptr);
        const std::size_t user_offset = user_ptr - base;

        // Search backwards for the header
        std::size_t search_offset = user_offset > HEADER_SIZE ? user_offset - HEADER_SIZE : 0;
        search_offset = align_down(search_offset, HEADER_ALIGN);

        while (search_offset >= HEADER_ALIGN) {
            auto* header = reinterpret_cast<StackHeader*>(m_buffer.data() + search_offset);

            // Check if this header's allocation contains our user pointer
            if (header->allocation_end >= user_offset &&
                search_offset + HEADER_SIZE <= user_offset) {
                // Verify this is the top allocation
                const std::size_t current_top = m_top.load(std::memory_order_acquire);
                if (header->allocation_end == current_top) {
                    m_top.store(header->previous_top, std::memory_order_release);
                }
                return;
            }

            if (search_offset < HEADER_ALIGN) break;
            search_offset -= HEADER_ALIGN;
        }
    }

    void reset() override {
        m_top.store(0, std::memory_order_release);
    }

    [[nodiscard]] std::size_t capacity() const noexcept override {
        return m_capacity;
    }

    [[nodiscard]] std::size_t used() const noexcept override {
        return m_top.load(std::memory_order_relaxed);
    }

    // =========================================================================
    // Stack-Specific API
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

    /// Get a marker for the current stack position
    [[nodiscard]] StackMarker marker() const noexcept {
        return StackMarker{m_top.load(std::memory_order_acquire)};
    }

    /// Alias for marker() to match Allocator scope interface
    [[nodiscard]] StackMarker save() const noexcept {
        return marker();
    }

    /// Rollback to a previous marker
    void rollback(const StackMarker& mark) noexcept {
        const std::size_t current = m_top.load(std::memory_order_acquire);
        if (mark.offset <= current) {
            m_top.store(mark.offset, std::memory_order_release);
        }
    }

    /// Alias for rollback() to match Allocator scope interface
    void restore(const StackMarker& mark) noexcept {
        rollback(mark);
    }

    /// Get the current top position
    [[nodiscard]] std::size_t current_position() const noexcept {
        return m_top.load(std::memory_order_relaxed);
    }

private:
    struct StackHeader {
        std::size_t previous_top;
        std::size_t allocation_end;
    };

    static constexpr std::size_t HEADER_SIZE = sizeof(StackHeader);
    static constexpr std::size_t HEADER_ALIGN = alignof(StackHeader);

    std::vector<std::uint8_t> m_buffer;
    std::atomic<std::size_t> m_top;
    std::size_t m_capacity;
};

/// Scoped stack allocation guard - restores stack position on destruction
using StackScope = AllocatorScope<StackAllocator>;

} // namespace void_memory
