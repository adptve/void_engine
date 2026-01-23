#pragma once

/// @file entity.hpp
/// @brief Entity and EntityAllocator for void_ecs
///
/// Entity uses generational indices to detect use-after-free errors.
/// When an entity is despawned, its generation is incremented so old
/// references become invalid.

#include "fwd.hpp"
#include <vector>
#include <limits>
#include <cstdint>
#include <functional>
#include <string>
#include <format>

namespace void_ecs {

// =============================================================================
// Entity
// =============================================================================

/// Entity handle with generational index
///
/// Combines a slot index with a generation counter to detect stale references.
/// When an entity is destroyed and its slot reused, the generation increments,
/// making old Entity handles invalid.
struct Entity {
    EntityIndex index;
    Generation generation;

    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create entity with explicit index and generation
    constexpr Entity(EntityIndex idx, Generation gen) noexcept
        : index(idx), generation(gen) {}

    /// Create null entity
    constexpr Entity() noexcept
        : index(std::numeric_limits<EntityIndex>::max())
        , generation(std::numeric_limits<Generation>::max()) {}

    /// Create null entity (factory)
    [[nodiscard]] static constexpr Entity null() noexcept {
        return Entity{};
    }

    // =========================================================================
    // Properties
    // =========================================================================

    /// Check if this is a null/invalid entity
    [[nodiscard]] constexpr bool is_null() const noexcept {
        return index == std::numeric_limits<EntityIndex>::max() &&
               generation == std::numeric_limits<Generation>::max();
    }

    /// Check if this is a valid (non-null) entity
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return !is_null();
    }

    /// Explicit bool conversion
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return is_valid();
    }

    // =========================================================================
    // Bit Encoding
    // =========================================================================

    /// Encode as 64-bit value (generation in high 32 bits, index in low 32)
    [[nodiscard]] constexpr std::uint64_t to_bits() const noexcept {
        return (static_cast<std::uint64_t>(generation) << 32) |
               static_cast<std::uint64_t>(index);
    }

    /// Decode from 64-bit value
    [[nodiscard]] static constexpr Entity from_bits(std::uint64_t bits) noexcept {
        return Entity{
            static_cast<EntityIndex>(bits & 0xFFFFFFFF),
            static_cast<Generation>(bits >> 32)
        };
    }

    // =========================================================================
    // Comparison
    // =========================================================================

    [[nodiscard]] constexpr bool operator==(const Entity& other) const noexcept {
        return index == other.index && generation == other.generation;
    }

    [[nodiscard]] constexpr bool operator!=(const Entity& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool operator<(const Entity& other) const noexcept {
        if (index != other.index) return index < other.index;
        return generation < other.generation;
    }

    [[nodiscard]] constexpr bool operator<=(const Entity& other) const noexcept {
        return !(other < *this);
    }

    [[nodiscard]] constexpr bool operator>(const Entity& other) const noexcept {
        return other < *this;
    }

    [[nodiscard]] constexpr bool operator>=(const Entity& other) const noexcept {
        return !(*this < other);
    }

    // =========================================================================
    // String Representation
    // =========================================================================

    /// Format as string (e.g., "Entity(5v2)" or "Entity(null)")
    [[nodiscard]] std::string to_string() const {
        if (is_null()) {
            return "Entity(null)";
        }
        return std::format("Entity({}v{})", index, generation);
    }
};

} // namespace void_ecs

// =============================================================================
// std::hash Specialization
// =============================================================================

template<>
struct std::hash<void_ecs::Entity> {
    [[nodiscard]] std::size_t operator()(const void_ecs::Entity& e) const noexcept {
        return std::hash<std::uint64_t>{}(e.to_bits());
    }
};

namespace void_ecs {

// =============================================================================
// EntityAllocator
// =============================================================================

/// Allocates and tracks entity lifetimes
///
/// Uses a free list to recycle entity indices. When an entity is deallocated,
/// its generation is incremented so old references become invalid.
class EntityAllocator {
public:
    using size_type = std::size_t;

private:
    std::vector<Generation> generations_;  // Generation for each index
    std::vector<EntityIndex> free_list_;   // Available indices
    size_type alive_count_{0};

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create empty allocator
    EntityAllocator() = default;

    /// Create with pre-allocated capacity
    explicit EntityAllocator(size_type capacity) {
        reserve(capacity);
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Number of currently alive entities
    [[nodiscard]] size_type alive_count() const noexcept {
        return alive_count_;
    }

    /// Alias for alive_count()
    [[nodiscard]] size_type size() const noexcept {
        return alive_count_;
    }

    /// Total allocated slots
    [[nodiscard]] size_type capacity() const noexcept {
        return generations_.size();
    }

    /// Check if no entities are alive
    [[nodiscard]] bool empty() const noexcept {
        return alive_count_ == 0;
    }

    /// Reserve capacity for additional entities
    void reserve(size_type additional) {
        generations_.reserve(generations_.size() + additional);
        free_list_.reserve(free_list_.size() + additional);
    }

    // =========================================================================
    // Allocation
    // =========================================================================

    /// Allocate a new entity
    /// @return Newly created entity
    [[nodiscard]] Entity allocate() {
        EntityIndex index;
        Generation generation;

        if (!free_list_.empty()) {
            // Reuse freed slot
            index = free_list_.back();
            free_list_.pop_back();
            generation = generations_[index];
        } else {
            // Allocate new slot
            index = static_cast<EntityIndex>(generations_.size());
            generations_.push_back(0);
            generation = 0;
        }

        ++alive_count_;
        return Entity{index, generation};
    }

    /// Deallocate an entity
    /// @return true if entity was alive and is now dead
    bool deallocate(Entity entity) {
        if (!is_alive(entity)) {
            return false;
        }

        // Increment generation to invalidate old references
        ++generations_[entity.index];

        // Add to free list for reuse
        free_list_.push_back(entity.index);

        --alive_count_;
        return true;
    }

    // =========================================================================
    // Queries
    // =========================================================================

    /// Check if an entity is currently alive
    [[nodiscard]] bool is_alive(Entity entity) const noexcept {
        if (entity.is_null()) {
            return false;
        }
        if (entity.index >= generations_.size()) {
            return false;
        }
        return generations_[entity.index] == entity.generation;
    }

    /// Check if an entity is currently alive (alias)
    [[nodiscard]] bool contains(Entity entity) const noexcept {
        return is_alive(entity);
    }

    /// Get the current generation for an index
    /// @return Current generation, or nullopt if index is out of range
    [[nodiscard]] std::optional<Generation> current_generation(EntityIndex index) const noexcept {
        if (index >= generations_.size()) {
            return std::nullopt;
        }
        return generations_[index];
    }

    // =========================================================================
    // Bulk Operations
    // =========================================================================

    /// Clear all entities
    void clear() {
        generations_.clear();
        free_list_.clear();
        alive_count_ = 0;
    }
};

// =============================================================================
// ArchetypeId
// =============================================================================

/// Unique identifier for an archetype
struct ArchetypeId {
    std::uint32_t id;

    static constexpr std::uint32_t INVALID_ID = std::numeric_limits<std::uint32_t>::max();

    constexpr explicit ArchetypeId(std::uint32_t i = INVALID_ID) noexcept : id(i) {}

    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return id; }

    [[nodiscard]] constexpr bool is_valid() const noexcept { return id != INVALID_ID; }

    [[nodiscard]] static constexpr ArchetypeId invalid() noexcept {
        return ArchetypeId{INVALID_ID};
    }

    [[nodiscard]] constexpr bool operator==(const ArchetypeId& other) const noexcept {
        return id == other.id;
    }
    [[nodiscard]] constexpr bool operator!=(const ArchetypeId& other) const noexcept {
        return id != other.id;
    }
    [[nodiscard]] constexpr bool operator<(const ArchetypeId& other) const noexcept {
        return id < other.id;
    }
};

// =============================================================================
// EntityLocation
// =============================================================================

/// Location of an entity within archetype storage
struct EntityLocation {
    ArchetypeId archetype_id;
    std::size_t row;

    /// Create location
    constexpr EntityLocation(ArchetypeId arch, std::size_t r) noexcept;

    /// Create invalid location
    static EntityLocation invalid() noexcept;

    /// Check if location is valid
    [[nodiscard]] bool is_valid() const noexcept;
};

} // namespace void_ecs
