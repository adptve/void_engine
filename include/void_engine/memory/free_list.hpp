#pragma once

/// @file free_list.hpp
/// @brief Free list allocator - general purpose with fragmentation management

#include "allocator.hpp"
#include <vector>
#include <mutex>
#include <algorithm>
#include <optional>

namespace void_memory {

/// Placement policy for choosing free blocks
enum class PlacementPolicy {
    FirstFit,   ///< First block that fits
    BestFit,    ///< Best fitting block (smallest that fits)
    WorstFit    ///< Worst fitting block (largest)
};

/// Free list statistics
struct FreeListStats {
    std::size_t capacity = 0;
    std::size_t used = 0;
    std::size_t free = 0;
    std::size_t free_blocks = 0;
    std::size_t largest_free_block = 0;
    std::size_t fragmentation_ratio = 0; ///< 0-100
};

/// Free list allocator
///
/// General-purpose allocator that tracks free blocks.
/// Supports variable-size allocations with configurable placement policy.
/// Thread-safe via mutex.
class FreeList : public IAllocator {
public:
    /// Create a new free list allocator
    explicit FreeList(std::size_t capacity)
        : FreeList(capacity, PlacementPolicy::FirstFit) {}

    /// Create with a specific placement policy
    FreeList(std::size_t capacity, PlacementPolicy policy)
        : m_buffer(capacity)
        , m_capacity(capacity)
        , m_used(0)
        , m_policy(policy)
    {
        m_free_blocks.push_back({0, capacity});
    }

    // =========================================================================
    // IAllocator Interface
    // =========================================================================

    [[nodiscard]] void* allocate(std::size_t size, std::size_t align) override {
        if (size == 0) {
            return reinterpret_cast<void*>(align);
        }

        // Account for header and alignment
        const std::size_t total_align = std::max(align, HEADER_ALIGN);
        const std::size_t total_size = align_up(HEADER_SIZE + size, total_align);

        std::lock_guard<std::mutex> lock(m_mutex);

        // Find a suitable block
        auto block_idx = find_block(total_size);
        if (!block_idx) {
            return nullptr;
        }

        auto [offset, block_size] = m_free_blocks[*block_idx];

        // Calculate aligned user pointer
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(m_buffer.data());
        const std::size_t header_ptr = offset;
        const std::size_t user_ptr = align_up(header_ptr + HEADER_SIZE, align);
        const std::size_t actual_size = (user_ptr - header_ptr) + size;

        // Split or remove block
        constexpr std::size_t MIN_SPLIT_SIZE = 32;
        if (block_size > actual_size + HEADER_SIZE + MIN_SPLIT_SIZE) {
            // Split: update this block with remaining size
            m_free_blocks[*block_idx] = {offset + actual_size, block_size - actual_size};
        } else {
            // Use entire block
            m_free_blocks.erase(m_free_blocks.begin() + *block_idx);
        }

        // Write header
        auto* header = reinterpret_cast<BlockHeader*>(m_buffer.data() + header_ptr);
        header->size = actual_size;
        header->is_free = false;

        m_used += actual_size;

        return m_buffer.data() + user_ptr;
    }

    void deallocate(void* ptr, std::size_t /*size*/, std::size_t /*align*/) override {
        if (!ptr) return;

        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(m_buffer.data());
        const std::uintptr_t user_ptr = reinterpret_cast<std::uintptr_t>(ptr);
        const std::size_t user_offset = user_ptr - base;

        // Find header (search backwards from user pointer)
        const std::size_t header_offset = user_offset - HEADER_SIZE;
        auto* header = reinterpret_cast<BlockHeader*>(m_buffer.data() + header_offset);

        std::lock_guard<std::mutex> lock(m_mutex);

        if (header->is_free) {
            return; // Double free protection
        }

        const std::size_t offset = header_offset;
        const std::size_t size = header->size;

        header->is_free = true;

        m_free_blocks.push_back({offset, size});
        m_used -= size;

        // Coalesce adjacent blocks
        coalesce();
    }

    void reset() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_free_blocks.clear();
        m_free_blocks.push_back({0, m_capacity});
        m_used = 0;
    }

    [[nodiscard]] std::size_t capacity() const noexcept override {
        return m_capacity;
    }

    [[nodiscard]] std::size_t used() const noexcept override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_used;
    }

    // =========================================================================
    // FreeList-Specific API
    // =========================================================================

    /// Get allocation statistics
    [[nodiscard]] FreeListStats stats() const {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::size_t largest = 0;
        for (const auto& [offset, size] : m_free_blocks) {
            largest = std::max(largest, size);
        }

        const std::size_t free = m_capacity - m_used;
        const std::size_t frag = free > 0 && m_free_blocks.size() > 1
            ? static_cast<std::size_t>(100.0 * (1.0 - static_cast<double>(largest) / static_cast<double>(free)))
            : 0;

        return FreeListStats{
            .capacity = m_capacity,
            .used = m_used,
            .free = free,
            .free_blocks = m_free_blocks.size(),
            .largest_free_block = largest,
            .fragmentation_ratio = frag
        };
    }

    /// Get the placement policy
    [[nodiscard]] PlacementPolicy policy() const noexcept {
        return m_policy;
    }

    /// Set the placement policy
    void set_policy(PlacementPolicy policy) noexcept {
        m_policy = policy;
    }

    /// Get the number of free blocks
    [[nodiscard]] std::size_t free_block_count() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_free_blocks.size();
    }

private:
    struct BlockHeader {
        std::size_t size;
        bool is_free;
    };

    static constexpr std::size_t HEADER_SIZE = sizeof(BlockHeader);
    static constexpr std::size_t HEADER_ALIGN = alignof(BlockHeader);

    /// Find a suitable free block
    [[nodiscard]] std::optional<std::size_t> find_block(std::size_t required_size) const {
        switch (m_policy) {
            case PlacementPolicy::FirstFit: {
                for (std::size_t i = 0; i < m_free_blocks.size(); ++i) {
                    if (m_free_blocks[i].second >= required_size) {
                        return i;
                    }
                }
                return std::nullopt;
            }

            case PlacementPolicy::BestFit: {
                std::optional<std::size_t> best;
                std::size_t best_size = std::numeric_limits<std::size_t>::max();

                for (std::size_t i = 0; i < m_free_blocks.size(); ++i) {
                    const std::size_t size = m_free_blocks[i].second;
                    if (size >= required_size && size < best_size) {
                        best = i;
                        best_size = size;
                    }
                }
                return best;
            }

            case PlacementPolicy::WorstFit: {
                std::optional<std::size_t> worst;
                std::size_t worst_size = 0;

                for (std::size_t i = 0; i < m_free_blocks.size(); ++i) {
                    const std::size_t size = m_free_blocks[i].second;
                    if (size >= required_size && size > worst_size) {
                        worst = i;
                        worst_size = size;
                    }
                }
                return worst;
            }
        }
        return std::nullopt;
    }

    /// Coalesce adjacent free blocks
    void coalesce() {
        if (m_free_blocks.size() < 2) return;

        // Sort by offset
        std::sort(m_free_blocks.begin(), m_free_blocks.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        // Merge adjacent blocks
        std::size_t i = 0;
        while (i < m_free_blocks.size() - 1) {
            auto [offset1, size1] = m_free_blocks[i];
            auto [offset2, size2] = m_free_blocks[i + 1];

            if (offset1 + size1 == offset2) {
                // Merge blocks
                m_free_blocks[i] = {offset1, size1 + size2};
                m_free_blocks.erase(m_free_blocks.begin() + i + 1);
            } else {
                ++i;
            }
        }
    }

    std::vector<std::uint8_t> m_buffer;
    mutable std::mutex m_mutex;
    std::vector<std::pair<std::size_t, std::size_t>> m_free_blocks; // (offset, size)
    std::size_t m_capacity;
    std::size_t m_used;
    PlacementPolicy m_policy;
};

} // namespace void_memory
