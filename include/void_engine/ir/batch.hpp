#pragma once

/// @file batch.hpp
/// @brief Batch optimization for void_ir patches

#include "fwd.hpp"
#include "patch.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace void_ir {

// =============================================================================
// OptimizationStats
// =============================================================================

/// Statistics from batch optimization
struct OptimizationStats {
    std::size_t original_count = 0;
    std::size_t optimized_count = 0;
    std::size_t merged_count = 0;
    std::size_t eliminated_count = 0;
    std::size_t reordered_count = 0;

    /// Calculate reduction percentage
    [[nodiscard]] double reduction_percent() const noexcept {
        if (original_count == 0) return 0.0;
        return 100.0 * (1.0 - static_cast<double>(optimized_count) /
                              static_cast<double>(original_count));
    }

    /// Combine stats
    OptimizationStats& operator+=(const OptimizationStats& other) {
        original_count += other.original_count;
        optimized_count += other.optimized_count;
        merged_count += other.merged_count;
        eliminated_count += other.eliminated_count;
        reordered_count += other.reordered_count;
        return *this;
    }
};

// =============================================================================
// BatchOptimizer
// =============================================================================

/// Optimizes patch batches by merging, eliminating, and sorting
class BatchOptimizer {
public:
    /// Optimization flags
    struct Options {
        /// Merge consecutive patches to same entity/component
        bool merge_consecutive = true;

        /// Eliminate contradictory patches (e.g., create then delete)
        bool eliminate_contradictions = true;

        /// Sort patches for optimal application order
        bool sort_for_efficiency = true;

        /// Coalesce field patches into full component patches
        bool coalesce_field_patches = true;

        /// Remove redundant patches (setting same value)
        bool remove_redundant = true;

        /// Create with all optimizations enabled
        [[nodiscard]] static Options all() {
            return Options{};
        }

        /// Create with no optimizations
        [[nodiscard]] static Options none() {
            Options o;
            o.merge_consecutive = false;
            o.eliminate_contradictions = false;
            o.sort_for_efficiency = false;
            o.coalesce_field_patches = false;
            o.remove_redundant = false;
            return o;
        }
    };

    /// Construct with options
    explicit BatchOptimizer(Options options = Options::all())
        : m_options(options) {}

    /// Optimize a batch of patches
    [[nodiscard]] PatchBatch optimize(const PatchBatch& input) {
        m_stats = OptimizationStats{};
        m_stats.original_count = input.size();

        if (input.empty()) {
            m_stats.optimized_count = 0;
            return PatchBatch{};
        }

        // Copy patches for modification
        std::vector<Patch> patches(input.begin(), input.end());

        // Apply optimizations
        if (m_options.eliminate_contradictions) {
            eliminate_contradictions(patches);
        }

        if (m_options.merge_consecutive) {
            merge_consecutive(patches);
        }

        if (m_options.coalesce_field_patches) {
            coalesce_field_patches(patches);
        }

        if (m_options.sort_for_efficiency) {
            sort_for_efficiency(patches);
        }

        // Build result batch
        PatchBatch result;
        result.reserve(patches.size());
        for (auto& p : patches) {
            result.push(std::move(p));
        }

        m_stats.optimized_count = result.size();
        return result;
    }

    /// Get stats from last optimization
    [[nodiscard]] const OptimizationStats& stats() const noexcept {
        return m_stats;
    }

    /// Get options
    [[nodiscard]] const Options& options() const noexcept {
        return m_options;
    }

    /// Set options
    void set_options(Options options) {
        m_options = options;
    }

private:
    /// Eliminate contradictory patches
    void eliminate_contradictions(std::vector<Patch>& patches) {
        // Track entities that are created and then deleted
        std::unordered_set<std::uint64_t> created_then_deleted;

        // First pass: find create-delete pairs
        std::unordered_map<std::uint64_t, std::size_t> create_indices;

        for (std::size_t i = 0; i < patches.size(); ++i) {
            const auto* ep = patches[i].try_as<EntityPatch>();
            if (!ep) continue;

            if (ep->operation == EntityOp::Create) {
                create_indices[ep->entity.entity_id] = i;
            }
            else if (ep->operation == EntityOp::Delete) {
                auto it = create_indices.find(ep->entity.entity_id);
                if (it != create_indices.end()) {
                    created_then_deleted.insert(ep->entity.entity_id);
                }
            }
        }

        // Second pass: remove patches for created-then-deleted entities
        if (!created_then_deleted.empty()) {
            auto new_end = std::remove_if(patches.begin(), patches.end(),
                [&](const Patch& p) {
                    auto target = p.target_entity();
                    if (!target) return false;
                    return created_then_deleted.count(target->entity_id) > 0;
                });

            std::size_t removed = std::distance(new_end, patches.end());
            patches.erase(new_end, patches.end());
            m_stats.eliminated_count += removed;
        }

        // Remove enable/disable pairs
        std::unordered_map<std::uint64_t, std::size_t> enable_indices;
        std::vector<std::size_t> to_remove;

        for (std::size_t i = 0; i < patches.size(); ++i) {
            const auto* ep = patches[i].try_as<EntityPatch>();
            if (!ep) continue;

            if (ep->operation == EntityOp::Enable) {
                enable_indices[ep->entity.entity_id] = i;
            }
            else if (ep->operation == EntityOp::Disable) {
                auto it = enable_indices.find(ep->entity.entity_id);
                if (it != enable_indices.end()) {
                    to_remove.push_back(it->second);
                    to_remove.push_back(i);
                    enable_indices.erase(it);
                }
            }
        }

        // Remove in reverse order to preserve indices
        std::sort(to_remove.begin(), to_remove.end(), std::greater<>());
        for (std::size_t idx : to_remove) {
            if (idx < patches.size()) {
                patches.erase(patches.begin() + static_cast<std::ptrdiff_t>(idx));
                m_stats.eliminated_count++;
            }
        }
    }

    /// Merge consecutive patches to same entity/component
    void merge_consecutive(std::vector<Patch>& patches) {
        if (patches.size() < 2) return;

        std::vector<Patch> result;
        result.reserve(patches.size());

        for (std::size_t i = 0; i < patches.size(); ++i) {
            // Try to merge with last patch in result
            if (!result.empty() && try_merge(result.back(), patches[i])) {
                m_stats.merged_count++;
                continue;
            }
            result.push_back(std::move(patches[i]));
        }

        patches = std::move(result);
    }

    /// Try to merge two patches, returning true if successful
    bool try_merge(Patch& target, const Patch& source) {
        // Only merge component patches to same entity/type
        const auto* target_cp = target.try_as<ComponentPatch>();
        const auto* source_cp = source.try_as<ComponentPatch>();

        if (!target_cp || !source_cp) return false;

        if (target_cp->entity != source_cp->entity) return false;
        if (target_cp->component_type != source_cp->component_type) return false;

        // Merge Set operations
        if (target_cp->operation == ComponentOp::Set &&
            source_cp->operation == ComponentOp::Set) {
            // Later value wins
            target.as<ComponentPatch>().value = source_cp->value.clone();
            return true;
        }

        // Merge SetField into Set
        if (target_cp->operation == ComponentOp::Set &&
            source_cp->operation == ComponentOp::SetField) {
            // Apply field update to the Set value
            if (target_cp->value.is_object()) {
                target.as<ComponentPatch>().value.as_object_mut()[source_cp->field_path] =
                    source_cp->value.clone();
                return true;
            }
        }

        return false;
    }

    /// Coalesce multiple SetField patches into a single Set patch
    void coalesce_field_patches(std::vector<Patch>& patches) {
        // Group SetField patches by entity+component
        struct Key {
            std::uint64_t entity_id;
            std::string component_type;

            bool operator==(const Key& other) const {
                return entity_id == other.entity_id &&
                       component_type == other.component_type;
            }
        };

        struct KeyHash {
            std::size_t operator()(const Key& k) const {
                return std::hash<std::uint64_t>{}(k.entity_id) ^
                       (std::hash<std::string>{}(k.component_type) << 1);
            }
        };

        std::unordered_map<Key, std::vector<std::size_t>, KeyHash> field_patch_groups;

        for (std::size_t i = 0; i < patches.size(); ++i) {
            const auto* cp = patches[i].try_as<ComponentPatch>();
            if (!cp || cp->operation != ComponentOp::SetField) continue;

            Key key{cp->entity.entity_id, cp->component_type};
            field_patch_groups[key].push_back(i);
        }

        // Coalesce groups with 3+ patches
        std::unordered_set<std::size_t> indices_to_remove;

        for (auto& [key, indices] : field_patch_groups) {
            if (indices.size() < 3) continue;

            // Build combined value
            Value combined = Value::empty_object();

            EntityRef entity;
            std::string component_type;

            for (std::size_t idx : indices) {
                const auto& cp = patches[idx].as<ComponentPatch>();
                entity = cp.entity;
                component_type = cp.component_type;
                combined.as_object_mut()[cp.field_path] = cp.value.clone();
                indices_to_remove.insert(idx);
            }

            // Add coalesced patch (will be added to end)
            patches.push_back(ComponentPatch::set(entity, component_type, std::move(combined)));
            m_stats.merged_count += indices.size() - 1;
        }

        // Remove coalesced patches
        if (!indices_to_remove.empty()) {
            std::vector<std::size_t> sorted_indices(indices_to_remove.begin(),
                                                      indices_to_remove.end());
            std::sort(sorted_indices.begin(), sorted_indices.end(), std::greater<>());

            for (std::size_t idx : sorted_indices) {
                patches.erase(patches.begin() + static_cast<std::ptrdiff_t>(idx));
            }
        }
    }

    /// Sort patches for optimal application order
    void sort_for_efficiency(std::vector<Patch>& patches) {
        // Sort by: Entity patches first, then by entity ID, then by patch kind
        std::stable_sort(patches.begin(), patches.end(),
            [](const Patch& a, const Patch& b) {
                // Entity patches first
                bool a_entity = a.is<EntityPatch>();
                bool b_entity = b.is<EntityPatch>();
                if (a_entity != b_entity) {
                    return a_entity;
                }

                // Within entity patches: creates before others
                if (a_entity && b_entity) {
                    const auto& ae = a.as<EntityPatch>();
                    const auto& be = b.as<EntityPatch>();
                    if (ae.operation != be.operation) {
                        return ae.operation == EntityOp::Create;
                    }
                }

                // Then by entity ID for cache locality
                auto a_entity_id = a.target_entity();
                auto b_entity_id = b.target_entity();
                if (a_entity_id && b_entity_id) {
                    if (a_entity_id->entity_id != b_entity_id->entity_id) {
                        return a_entity_id->entity_id < b_entity_id->entity_id;
                    }
                }

                // Finally by kind
                return static_cast<int>(a.kind()) < static_cast<int>(b.kind());
            });

        m_stats.reordered_count = patches.size();
    }

    Options m_options;
    OptimizationStats m_stats;
};

// =============================================================================
// PatchDeduplicator
// =============================================================================

/// Removes duplicate patches
class PatchDeduplicator {
public:
    /// Deduplicate patches (keeps last occurrence)
    [[nodiscard]] PatchBatch deduplicate(const PatchBatch& input) {
        m_removed_count = 0;

        if (input.size() < 2) {
            return input;
        }

        // Track seen patches by a hash
        std::unordered_map<std::size_t, std::size_t> seen;  // hash -> last index

        // First pass: find last occurrence of each patch
        for (std::size_t i = 0; i < input.size(); ++i) {
            std::size_t hash = compute_patch_hash(input.patches()[i]);
            seen[hash] = i;
        }

        // Second pass: keep only last occurrences
        PatchBatch result;
        result.reserve(seen.size());

        std::vector<bool> keep(input.size(), false);
        for (const auto& [hash, idx] : seen) {
            keep[idx] = true;
        }

        for (std::size_t i = 0; i < input.size(); ++i) {
            if (keep[i]) {
                result.push(Patch(input.patches()[i]));
            } else {
                m_removed_count++;
            }
        }

        return result;
    }

    /// Get count of removed duplicates
    [[nodiscard]] std::size_t removed_count() const noexcept {
        return m_removed_count;
    }

private:
    std::size_t compute_patch_hash(const Patch& patch) {
        std::size_t h = static_cast<std::size_t>(patch.kind());

        auto target = patch.target_entity();
        if (target) {
            h ^= std::hash<std::uint64_t>{}(target->entity_id) << 1;
            h ^= std::hash<std::uint32_t>{}(target->namespace_id.value) << 2;
        }

        // Add type-specific hash
        if (const auto* cp = patch.try_as<ComponentPatch>()) {
            h ^= std::hash<std::string>{}(cp->component_type) << 3;
            h ^= static_cast<std::size_t>(cp->operation) << 4;
            if (!cp->field_path.empty()) {
                h ^= std::hash<std::string>{}(cp->field_path) << 5;
            }
        }

        return h;
    }

    std::size_t m_removed_count = 0;
};

// =============================================================================
// PatchSplitter
// =============================================================================

/// Splits patches by namespace or entity
class PatchSplitter {
public:
    /// Split by namespace
    [[nodiscard]] std::unordered_map<std::uint32_t, PatchBatch>
    split_by_namespace(const PatchBatch& input) {
        std::unordered_map<std::uint32_t, PatchBatch> result;

        for (const auto& patch : input) {
            auto target = patch.target_entity();
            if (target) {
                result[target->namespace_id.value].push(Patch(patch));
            }
        }

        return result;
    }

    /// Split by entity
    [[nodiscard]] std::unordered_map<std::uint64_t, PatchBatch>
    split_by_entity(const PatchBatch& input) {
        std::unordered_map<std::uint64_t, PatchBatch> result;

        for (const auto& patch : input) {
            auto target = patch.target_entity();
            if (target) {
                result[target->entity_id].push(Patch(patch));
            }
        }

        return result;
    }

    /// Split by patch kind
    [[nodiscard]] std::unordered_map<PatchKind, PatchBatch>
    split_by_kind(const PatchBatch& input) {
        std::unordered_map<PatchKind, PatchBatch> result;

        for (const auto& patch : input) {
            result[patch.kind()].push(Patch(patch));
        }

        return result;
    }
};

} // namespace void_ir
