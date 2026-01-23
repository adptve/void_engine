#pragma once

/// @file query.hpp
/// @brief Query system for void_ecs
///
/// Queries provide efficient filtered iteration over entities based on
/// component requirements. Uses bitmask matching for fast archetype filtering.

#include "fwd.hpp"
#include "entity.hpp"
#include "component.hpp"
#include "archetype.hpp"

#include <vector>
#include <optional>

namespace void_ecs {

// =============================================================================
// Access
// =============================================================================

/// Component access mode for queries
enum class Access : std::uint8_t {
    Read,           // Immutable component access (required)
    Write,          // Mutable component access (required)
    OptionalRead,   // Component may or may not exist (read if present)
    OptionalWrite,  // Component may or may not exist (write if present)
    Without,        // Component must NOT be present
};

// =============================================================================
// ComponentAccess
// =============================================================================

/// Single component access requirement
struct ComponentAccess {
    ComponentId id;
    Access access;

    ComponentAccess(ComponentId i, Access a) : id(i), access(a) {}

    /// Check if this access is required (not optional)
    [[nodiscard]] bool is_required() const noexcept {
        return access == Access::Read || access == Access::Write;
    }

    /// Check if this access is optional
    [[nodiscard]] bool is_optional() const noexcept {
        return access == Access::OptionalRead || access == Access::OptionalWrite;
    }

    /// Check if this is an exclusion
    [[nodiscard]] bool is_excluded() const noexcept {
        return access == Access::Without;
    }

    /// Check if this access is writable
    [[nodiscard]] bool is_write() const noexcept {
        return access == Access::Write || access == Access::OptionalWrite;
    }
};

// =============================================================================
// QueryDescriptor
// =============================================================================

/// Builder for query requirements
///
/// Example:
/// @code
/// auto query = QueryDescriptor()
///     .read(position_id)
///     .write(velocity_id)
///     .without(static_id)
///     .build();
/// @endcode
class QueryDescriptor {
private:
    std::vector<ComponentAccess> components_;
    void_structures::BitSet required_mask_{256};
    void_structures::BitSet excluded_mask_{256};
    bool built_{false};

public:
    QueryDescriptor() = default;

    // =========================================================================
    // Builder Methods
    // =========================================================================

    /// Add required read access
    QueryDescriptor& read(ComponentId id) {
        components_.emplace_back(id, Access::Read);
        return *this;
    }

    /// Add required write access
    QueryDescriptor& write(ComponentId id) {
        components_.emplace_back(id, Access::Write);
        return *this;
    }

    /// Add optional read access
    QueryDescriptor& optional_read(ComponentId id) {
        components_.emplace_back(id, Access::OptionalRead);
        return *this;
    }

    /// Add optional write access
    QueryDescriptor& optional_write(ComponentId id) {
        components_.emplace_back(id, Access::OptionalWrite);
        return *this;
    }

    /// Add exclusion filter
    QueryDescriptor& without(ComponentId id) {
        components_.emplace_back(id, Access::Without);
        return *this;
    }

    /// Build the query (computes bitmasks)
    QueryDescriptor& build() {
        required_mask_.clear_all();
        excluded_mask_.clear_all();

        for (const auto& access : components_) {
            if (access.is_required()) {
                required_mask_.set(access.id.id);
            } else if (access.is_excluded()) {
                excluded_mask_.set(access.id.id);
            }
        }

        built_ = true;
        return *this;
    }

    // =========================================================================
    // Query Properties
    // =========================================================================

    /// Get component access requirements
    [[nodiscard]] const std::vector<ComponentAccess>& accesses() const noexcept {
        return components_;
    }

    /// Get required component mask
    [[nodiscard]] const void_structures::BitSet& required_mask() const noexcept {
        return required_mask_;
    }

    /// Get excluded component mask
    [[nodiscard]] const void_structures::BitSet& excluded_mask() const noexcept {
        return excluded_mask_;
    }

    /// Check if query matches an archetype
    [[nodiscard]] bool matches_archetype(const Archetype& archetype) const noexcept {
        const auto& arch_mask = archetype.component_mask();

        // Check required components (all must be present)
        for (auto idx : required_mask_.iter_ones()) {
            if (!arch_mask.get(idx)) {
                return false;
            }
        }

        // Check excluded components (none must be present)
        for (auto idx : excluded_mask_.iter_ones()) {
            if (arch_mask.get(idx)) {
                return false;
            }
        }

        return true;
    }

    /// Check if this query conflicts with another (for parallelization)
    [[nodiscard]] bool conflicts_with(const QueryDescriptor& other) const noexcept {
        // Check for write conflicts
        for (const auto& access : components_) {
            if (access.is_write()) {
                // Our write conflicts with any access to the same component
                for (const auto& other_access : other.components_) {
                    if (access.id == other_access.id && !other_access.is_excluded()) {
                        return true;
                    }
                }
            }
        }

        // Check for our reads conflicting with their writes
        for (const auto& access : components_) {
            if (!access.is_excluded() && !access.is_write()) {
                for (const auto& other_access : other.components_) {
                    if (access.id == other_access.id && other_access.is_write()) {
                        return true;
                    }
                }
            }
        }

        return false;
    }
};

// =============================================================================
// QueryState
// =============================================================================

/// Cached state for a query
///
/// Caches which archetypes match the query to avoid recomputation.
class QueryState {
private:
    QueryDescriptor descriptor_;
    std::vector<ArchetypeId> matched_archetypes_;
    std::size_t last_archetype_count_{0};

public:
    explicit QueryState(QueryDescriptor descriptor)
        : descriptor_(std::move(descriptor)) {}

    /// Update matched archetypes if needed
    void update(const Archetypes& archetypes) {
        if (archetypes.size() == last_archetype_count_) {
            return;  // No new archetypes
        }

        // Check new archetypes
        for (std::size_t i = last_archetype_count_; i < archetypes.size(); ++i) {
            ArchetypeId id{static_cast<std::uint32_t>(i)};
            const Archetype* arch = archetypes.get(id);
            if (arch && descriptor_.matches_archetype(*arch)) {
                matched_archetypes_.push_back(id);
            }
        }

        last_archetype_count_ = archetypes.size();
    }

    /// Get matched archetype IDs
    [[nodiscard]] const std::vector<ArchetypeId>& matched_archetypes() const noexcept {
        return matched_archetypes_;
    }

    /// Get query descriptor
    [[nodiscard]] const QueryDescriptor& descriptor() const noexcept {
        return descriptor_;
    }

    /// Clear cache (forces recomputation on next update)
    void invalidate() {
        matched_archetypes_.clear();
        last_archetype_count_ = 0;
    }
};

// =============================================================================
// ArchetypeQueryIter
// =============================================================================

/// Iterator over entities within a single archetype
template<bool IsConst>
class ArchetypeQueryIter {
public:
    using archetype_type = std::conditional_t<IsConst, const Archetype, Archetype>;
    using size_type = std::size_t;

private:
    archetype_type* archetype_;
    size_type current_{0};
    size_type len_;

public:
    explicit ArchetypeQueryIter(archetype_type* arch)
        : archetype_(arch)
        , len_(arch ? arch->size() : 0) {}

    /// Get remaining count
    [[nodiscard]] size_type remaining() const noexcept {
        return current_ < len_ ? len_ - current_ : 0;
    }

    /// Check if exhausted
    [[nodiscard]] bool empty() const noexcept {
        return remaining() == 0;
    }

    /// Get entity at current position
    [[nodiscard]] Entity entity() const noexcept {
        return archetype_->entity_at(current_);
    }

    /// Get current row index
    [[nodiscard]] size_type row() const noexcept {
        return current_;
    }

    /// Get typed component at current position
    template<typename T>
    [[nodiscard]] auto get_component(ComponentId id) const {
        if constexpr (IsConst) {
            return archetype_->template get_component<T>(id, current_);
        } else {
            return archetype_->template get_component<T>(id, current_);
        }
    }

    /// Advance to next entity
    bool next() {
        if (current_ < len_) {
            ++current_;
            return current_ < len_;
        }
        return false;
    }

    /// Reset to beginning
    void reset() {
        current_ = 0;
    }
};

// =============================================================================
// QueryIter
// =============================================================================

/// Iterator over multiple archetypes
class QueryIter {
public:
    using size_type = std::size_t;

private:
    const Archetypes* archetypes_;
    const std::vector<ArchetypeId>* matched_;
    size_type archetype_index_{0};
    size_type row_{0};

public:
    QueryIter(const Archetypes* archetypes, const QueryState* state)
        : archetypes_(archetypes)
        , matched_(&state->matched_archetypes())
    {
        // Skip empty archetypes
        skip_empty();
    }

    /// Check if exhausted
    [[nodiscard]] bool empty() const noexcept {
        return archetype_index_ >= matched_->size();
    }

    /// Get current archetype ID
    [[nodiscard]] ArchetypeId archetype_id() const noexcept {
        if (empty()) return ArchetypeId::invalid();
        return (*matched_)[archetype_index_];
    }

    /// Get current archetype
    [[nodiscard]] const Archetype* archetype() const noexcept {
        if (empty()) return nullptr;
        return archetypes_->get(archetype_id());
    }

    /// Get current row
    [[nodiscard]] size_type row() const noexcept {
        return row_;
    }

    /// Get current entity
    [[nodiscard]] Entity entity() const noexcept {
        const Archetype* arch = archetype();
        if (!arch) return Entity::null();
        return arch->entity_at(row_);
    }

    /// Advance to next entity
    bool next() {
        if (empty()) return false;

        const Archetype* arch = archetype();
        if (!arch) return false;

        ++row_;

        if (row_ >= arch->size()) {
            // Move to next archetype
            ++archetype_index_;
            row_ = 0;
            skip_empty();
        }

        return !empty();
    }

private:
    void skip_empty() {
        while (archetype_index_ < matched_->size()) {
            const Archetype* arch = archetypes_->get((*matched_)[archetype_index_]);
            if (arch && !arch->empty()) {
                break;
            }
            ++archetype_index_;
        }
    }
};

// =============================================================================
// Query Result Tuple Helper
// =============================================================================

/// Helper to get multiple components at once
template<typename... Ts>
struct QueryResult {
    Entity entity;
    std::tuple<Ts*...> components;

    QueryResult(Entity e, Ts*... comps)
        : entity(e), components(comps...) {}
};

} // namespace void_ecs
