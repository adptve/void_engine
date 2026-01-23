#pragma once

/// @file allocator.hpp
/// @brief Base allocator interface for void_memory

#include "fwd.hpp"
#include <cstddef>
#include <cstdint>
#include <memory_resource>

namespace void_memory {

// =============================================================================
// Alignment Utilities
// =============================================================================

/// Align a value up to the given alignment
[[nodiscard]] constexpr std::size_t align_up(std::size_t value, std::size_t align) noexcept {
    return (value + align - 1) & ~(align - 1);
}

/// Align a value down to the given alignment
[[nodiscard]] constexpr std::size_t align_down(std::size_t value, std::size_t align) noexcept {
    return value & ~(align - 1);
}

/// Check if a pointer is aligned
[[nodiscard]] inline bool is_aligned(const void* ptr, std::size_t align) noexcept {
    return (reinterpret_cast<std::uintptr_t>(ptr) & (align - 1)) == 0;
}

// =============================================================================
// Allocator Interface
// =============================================================================

/// Common interface for all custom allocators
class IAllocator {
public:
    virtual ~IAllocator() = default;

    /// Allocate memory with the given size and alignment
    [[nodiscard]] virtual void* allocate(std::size_t size, std::size_t align) = 0;

    /// Deallocate memory
    /// @note Some allocators (Arena) may not support individual deallocation
    virtual void deallocate(void* ptr, std::size_t size, std::size_t align) = 0;

    /// Reset the allocator, freeing all allocations
    virtual void reset() = 0;

    /// Get the total capacity
    [[nodiscard]] virtual std::size_t capacity() const noexcept = 0;

    /// Get the currently used memory
    [[nodiscard]] virtual std::size_t used() const noexcept = 0;

    /// Get the available memory
    [[nodiscard]] std::size_t available() const noexcept {
        return capacity() - used();
    }

    /// Check if the allocator is empty
    [[nodiscard]] bool is_empty() const noexcept {
        return used() == 0;
    }

    /// Check if the allocator is full
    [[nodiscard]] bool is_full() const noexcept {
        return available() == 0;
    }

    /// Allocate typed memory
    template<typename T>
    [[nodiscard]] T* allocate_typed(std::size_t count = 1) {
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }

    /// Allocate and construct a single object
    template<typename T, typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        T* ptr = allocate_typed<T>(1);
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }

    /// Destruct and deallocate an object
    template<typename T>
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr, sizeof(T), alignof(T));
        }
    }
};

// =============================================================================
// PMR Adapter
// =============================================================================

/// Adapter to use IAllocator as a std::pmr::memory_resource
class AllocatorResource : public std::pmr::memory_resource {
public:
    explicit AllocatorResource(IAllocator& allocator) noexcept
        : m_allocator(&allocator) {}

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        return m_allocator->allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        m_allocator->deallocate(p, bytes, alignment);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        if (auto* o = dynamic_cast<const AllocatorResource*>(&other)) {
            return m_allocator == o->m_allocator;
        }
        return false;
    }

private:
    IAllocator* m_allocator;
};

// =============================================================================
// Scoped Allocator Guard
// =============================================================================

/// RAII guard that saves and restores allocator state
/// Works with Arena and StackAllocator
template<typename Allocator>
class AllocatorScope {
public:
    explicit AllocatorScope(Allocator& allocator) noexcept
        : m_allocator(&allocator)
        , m_saved_state(allocator.save()) {}

    ~AllocatorScope() {
        m_allocator->restore(m_saved_state);
    }

    // Non-copyable, non-movable
    AllocatorScope(const AllocatorScope&) = delete;
    AllocatorScope& operator=(const AllocatorScope&) = delete;
    AllocatorScope(AllocatorScope&&) = delete;
    AllocatorScope& operator=(AllocatorScope&&) = delete;

    /// Access the allocator
    Allocator& allocator() noexcept { return *m_allocator; }
    const Allocator& allocator() const noexcept { return *m_allocator; }

private:
    Allocator* m_allocator;
    typename Allocator::State m_saved_state;
};

} // namespace void_memory
