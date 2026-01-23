#pragma once

/// @file pool.hpp
/// @brief Pool allocator - fixed-size block allocation

#include "allocator.hpp"
#include <atomic>
#include <vector>

namespace void_memory {

/// Pool statistics
struct PoolStats {
    std::size_t block_size = 0;
    std::size_t total_blocks = 0;
    std::size_t allocated_blocks = 0;
    std::size_t free_blocks = 0;
};

/// Pool allocator for fixed-size blocks
///
/// Extremely fast for allocating objects of the same size.
/// Uses a lock-free free list for O(1) allocation and deallocation.
class Pool : public IAllocator {
public:
    /// Create a new pool for objects of the given size
    Pool(std::size_t object_size, std::size_t object_align, std::size_t count)
        : m_block_size(calculate_block_size(object_size, object_align))
        , m_block_count(count)
        , m_buffer(m_block_size * count)
        , m_free_head(nullptr)
        , m_allocated(0)
    {
        initialize_free_list();
    }

    /// Create a pool for a specific type
    template<typename T>
    [[nodiscard]] static Pool for_type(std::size_t count) {
        return Pool(sizeof(T), alignof(T), count);
    }

    // Non-copyable
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    // Movable
    Pool(Pool&& other) noexcept
        : m_block_size(other.m_block_size)
        , m_block_count(other.m_block_count)
        , m_buffer(std::move(other.m_buffer))
        , m_free_head(other.m_free_head.load(std::memory_order_relaxed))
        , m_allocated(other.m_allocated.load(std::memory_order_relaxed))
    {
        other.m_free_head.store(nullptr, std::memory_order_relaxed);
        other.m_allocated.store(0, std::memory_order_relaxed);
    }

    Pool& operator=(Pool&& other) noexcept {
        if (this != &other) {
            m_block_size = other.m_block_size;
            m_block_count = other.m_block_count;
            m_buffer = std::move(other.m_buffer);
            m_free_head.store(other.m_free_head.load(std::memory_order_relaxed), std::memory_order_relaxed);
            m_allocated.store(other.m_allocated.load(std::memory_order_relaxed), std::memory_order_relaxed);
            other.m_free_head.store(nullptr, std::memory_order_relaxed);
            other.m_allocated.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    // =========================================================================
    // IAllocator Interface
    // =========================================================================

    [[nodiscard]] void* allocate(std::size_t size, std::size_t align) override {
        // Check that the requested allocation fits in a block
        const std::size_t required = align_up(size, align);
        if (required > m_block_size) {
            return nullptr;
        }
        return alloc_block();
    }

    void deallocate(void* ptr, std::size_t /*size*/, std::size_t /*align*/) override {
        if (ptr) {
            free_block(ptr);
        }
    }

    void reset() override {
        m_allocated.store(0, std::memory_order_release);
        initialize_free_list();
    }

    [[nodiscard]] std::size_t capacity() const noexcept override {
        return m_block_count * m_block_size;
    }

    [[nodiscard]] std::size_t used() const noexcept override {
        return allocated_count() * m_block_size;
    }

    // =========================================================================
    // Pool-Specific API
    // =========================================================================

    /// Allocate a block
    [[nodiscard]] void* alloc_block() {
        while (true) {
            FreeNode* head = m_free_head.load(std::memory_order_acquire);
            if (!head) {
                return nullptr; // Pool exhausted
            }

            FreeNode* next = head->next.load(std::memory_order_relaxed);

            if (m_free_head.compare_exchange_weak(head, next,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                m_allocated.fetch_add(1, std::memory_order_relaxed);
                return head;
            }
            // Retry on contention
        }
    }

    /// Free a block
    void free_block(void* ptr) {
        if (!ptr) return;

        auto* node = static_cast<FreeNode*>(ptr);

        while (true) {
            FreeNode* head = m_free_head.load(std::memory_order_relaxed);
            node->next.store(head, std::memory_order_relaxed);

            if (m_free_head.compare_exchange_weak(head, node,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                m_allocated.fetch_sub(1, std::memory_order_relaxed);
                return;
            }
            // Retry on contention
        }
    }

    /// Get the block size
    [[nodiscard]] std::size_t block_size() const noexcept { return m_block_size; }

    /// Get the total number of blocks
    [[nodiscard]] std::size_t block_count() const noexcept { return m_block_count; }

    /// Get the number of allocated blocks
    [[nodiscard]] std::size_t allocated_count() const noexcept {
        return m_allocated.load(std::memory_order_relaxed);
    }

    /// Get the number of free blocks
    [[nodiscard]] std::size_t free_count() const noexcept {
        return m_block_count - allocated_count();
    }

    /// Get pool statistics
    [[nodiscard]] PoolStats stats() const noexcept {
        return PoolStats{
            .block_size = m_block_size,
            .total_blocks = m_block_count,
            .allocated_blocks = allocated_count(),
            .free_blocks = free_count()
        };
    }

private:
    struct FreeNode {
        std::atomic<FreeNode*> next{nullptr};
    };

    static std::size_t calculate_block_size(std::size_t object_size, std::size_t object_align) {
        const std::size_t min_size = sizeof(FreeNode);
        const std::size_t min_align = alignof(FreeNode);
        return align_up(std::max(object_size, min_size), std::max(object_align, min_align));
    }

    void initialize_free_list() {
        std::uint8_t* base = m_buffer.data();

        FreeNode* prev = nullptr;

        // Build free list from end to start (so allocation returns from start)
        for (std::size_t i = m_block_count; i > 0; --i) {
            auto* node = reinterpret_cast<FreeNode*>(base + (i - 1) * m_block_size);
            node->next.store(prev, std::memory_order_relaxed);
            prev = node;
        }

        m_free_head.store(prev, std::memory_order_release);
    }

    std::size_t m_block_size;
    std::size_t m_block_count;
    std::vector<std::uint8_t> m_buffer;
    std::atomic<FreeNode*> m_free_head;
    std::atomic<std::size_t> m_allocated;
};

/// Type-safe pool wrapper
template<typename T>
class TypedPool {
public:
    /// Create a new typed pool
    explicit TypedPool(std::size_t count)
        : m_pool(Pool::for_type<T>(count)) {}

    /// Allocate and construct a new object
    template<typename... Args>
    [[nodiscard]] T* alloc(Args&&... args) {
        void* ptr = m_pool.alloc_block();
        if (!ptr) return nullptr;
        return new (ptr) T(std::forward<Args>(args)...);
    }

    /// Destruct and free an object
    void free(T* ptr) {
        if (ptr) {
            ptr->~T();
            m_pool.free_block(ptr);
        }
    }

    /// Get statistics
    [[nodiscard]] PoolStats stats() const noexcept {
        return m_pool.stats();
    }

    /// Get the underlying pool
    [[nodiscard]] Pool& pool() noexcept { return m_pool; }
    [[nodiscard]] const Pool& pool() const noexcept { return m_pool; }

private:
    Pool m_pool;
};

} // namespace void_memory
