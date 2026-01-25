/// @file broadphase.hpp
/// @brief Broad phase collision detection using BVH
///
/// Implements a dynamic AABB tree for efficient broad phase collision
/// detection and spatial queries (raycasts, overlaps).

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "collision.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/bounds.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <stack>
#include <unordered_map>
#include <vector>

namespace void_physics {

// =============================================================================
// Constants
// =============================================================================

/// AABB fattening margin for dynamic objects
constexpr float k_aabb_margin = 0.05f;

/// Velocity multiplier for predictive AABB expansion
constexpr float k_velocity_multiplier = 2.0f;

/// Null node index
constexpr int k_null_node = -1;

// =============================================================================
// BVH Node
// =============================================================================

/// Dynamic BVH node
struct BvhNode {
    void_math::AABB aabb;           ///< Bounding box
    int parent = k_null_node;       ///< Parent node
    int left = k_null_node;         ///< Left child (or next free node)
    int right = k_null_node;        ///< Right child
    int height = 0;                 ///< Tree height at this node
    bool is_leaf = false;           ///< True if leaf node

    // Leaf data
    BodyId body_id;                 ///< Body ID (leaf only)
    ShapeId shape_id;               ///< Shape ID (leaf only)

    [[nodiscard]] bool is_branch() const { return !is_leaf; }
};

// =============================================================================
// Broad Phase BVH
// =============================================================================

/// Dynamic AABB tree for broad phase collision detection
class BroadPhaseBvh {
public:
    using BodyShapeKey = std::pair<BodyId, ShapeId>;

    BroadPhaseBvh() {
        // Pre-allocate some nodes
        m_nodes.reserve(256);
    }

    // =========================================================================
    // Proxy Management
    // =========================================================================

    /// Insert a new AABB into the tree
    /// @return Node index
    int insert(const void_math::AABB& aabb, BodyId body_id, ShapeId shape_id) {
        int node_idx = allocate_node();
        BvhNode& node = m_nodes[node_idx];

        // Fatten AABB
        void_math::Vec3 margin{k_aabb_margin, k_aabb_margin, k_aabb_margin};
        node.aabb.min = aabb.min - margin;
        node.aabb.max = aabb.max + margin;
        node.is_leaf = true;
        node.body_id = body_id;
        node.shape_id = shape_id;
        node.height = 0;

        // Insert into tree
        insert_leaf(node_idx);

        // Track for lookup
        BodyShapeKey key{body_id, shape_id};
        m_proxy_map[key] = node_idx;

        return node_idx;
    }

    /// Remove an AABB from the tree
    void remove(BodyId body_id, ShapeId shape_id) {
        BodyShapeKey key{body_id, shape_id};
        auto it = m_proxy_map.find(key);
        if (it == m_proxy_map.end()) {
            return;
        }

        int node_idx = it->second;
        m_proxy_map.erase(it);

        remove_leaf(node_idx);
        free_node(node_idx);
    }

    /// Update an AABB in the tree
    /// @return true if the proxy was moved
    bool update(BodyId body_id, ShapeId shape_id, const void_math::AABB& aabb,
                const void_math::Vec3& velocity = {0, 0, 0}) {
        BodyShapeKey key{body_id, shape_id};
        auto it = m_proxy_map.find(key);
        if (it == m_proxy_map.end()) {
            insert(aabb, body_id, shape_id);
            return true;
        }

        int node_idx = it->second;
        BvhNode& node = m_nodes[node_idx];

        // Check if AABB still fits in fattened bounds
        if (contains(node.aabb, aabb)) {
            return false;
        }

        // Remove and re-insert
        remove_leaf(node_idx);

        // Compute new fattened AABB with velocity prediction
        void_math::Vec3 margin{k_aabb_margin, k_aabb_margin, k_aabb_margin};
        void_math::Vec3 velocity_ext = velocity * k_velocity_multiplier;

        node.aabb.min = aabb.min - margin;
        node.aabb.max = aabb.max + margin;

        // Expand in velocity direction
        if (velocity_ext.x > 0) node.aabb.max.x += velocity_ext.x;
        else node.aabb.min.x += velocity_ext.x;
        if (velocity_ext.y > 0) node.aabb.max.y += velocity_ext.y;
        else node.aabb.min.y += velocity_ext.y;
        if (velocity_ext.z > 0) node.aabb.max.z += velocity_ext.z;
        else node.aabb.min.z += velocity_ext.z;

        insert_leaf(node_idx);
        return true;
    }

    /// Clear all nodes
    void clear() {
        m_nodes.clear();
        m_proxy_map.clear();
        m_root = k_null_node;
        m_free_list = k_null_node;
    }

    /// Remove proxies for bodies that no longer exist
    /// @param predicate Returns true for bodies that should be removed
    template<typename Predicate>
    void remove_invalid(Predicate&& predicate) {
        std::vector<BodyShapeKey> to_remove;
        for (const auto& [key, node_idx] : m_proxy_map) {
            if (predicate(key.first)) {
                to_remove.push_back(key);
            }
        }
        for (const auto& key : to_remove) {
            remove(key.first, key.second);
        }
    }

    // =========================================================================
    // Queries
    // =========================================================================

    /// Query all overlapping pairs
    void query_pairs(std::vector<CollisionPair>& pairs) const {
        pairs.clear();

        if (m_root == k_null_node) return;

        for (const auto& [key, node_idx] : m_proxy_map) {
            const BvhNode& node = m_nodes[node_idx];
            query_subtree(m_root, node.aabb, node.body_id, node.shape_id, pairs);
        }

        // Remove duplicates (A-B and B-A)
        std::sort(pairs.begin(), pairs.end(), [](const CollisionPair& a, const CollisionPair& b) {
            if (a.body_a.value != b.body_a.value) return a.body_a.value < b.body_a.value;
            return a.body_b.value < b.body_b.value;
        });
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    }

    /// Query AABBs overlapping with given AABB
    void query_aabb(const void_math::AABB& aabb,
                    std::vector<std::pair<BodyId, ShapeId>>& results) const {
        results.clear();

        if (m_root == k_null_node) return;

        std::stack<int> stack;
        stack.push(m_root);

        while (!stack.empty()) {
            int node_idx = stack.top();
            stack.pop();

            if (node_idx == k_null_node) continue;

            const BvhNode& node = m_nodes[node_idx];

            if (!void_math::intersects(node.aabb, aabb)) {
                continue;
            }

            if (node.is_leaf) {
                results.emplace_back(node.body_id, node.shape_id);
            } else {
                stack.push(node.left);
                stack.push(node.right);
            }
        }
    }

    /// Raycast through the BVH
    /// @param callback Called for each potential hit, return false to stop
    void raycast(const void_math::Vec3& origin,
                 const void_math::Vec3& direction,
                 float max_distance,
                 const std::function<bool(BodyId, ShapeId, float)>& callback) const {
        if (m_root == k_null_node) return;

        void_math::Vec3 inv_dir{
            std::abs(direction.x) > 1e-6f ? 1.0f / direction.x : 1e6f,
            std::abs(direction.y) > 1e-6f ? 1.0f / direction.y : 1e6f,
            std::abs(direction.z) > 1e-6f ? 1.0f / direction.z : 1e6f
        };

        std::stack<int> stack;
        stack.push(m_root);

        while (!stack.empty()) {
            int node_idx = stack.top();
            stack.pop();

            if (node_idx == k_null_node) continue;

            const BvhNode& node = m_nodes[node_idx];

            float t;
            if (!ray_aabb_intersect(origin, inv_dir, node.aabb, max_distance, t)) {
                continue;
            }

            if (node.is_leaf) {
                if (!callback(node.body_id, node.shape_id, t)) {
                    return;  // Early exit
                }
            } else {
                // Push children (closer one last for depth-first)
                const BvhNode& left = m_nodes[node.left];
                const BvhNode& right = m_nodes[node.right];

                float t_left, t_right;
                bool hit_left = ray_aabb_intersect(origin, inv_dir, left.aabb, max_distance, t_left);
                bool hit_right = ray_aabb_intersect(origin, inv_dir, right.aabb, max_distance, t_right);

                if (hit_left && hit_right) {
                    if (t_left < t_right) {
                        stack.push(node.right);
                        stack.push(node.left);
                    } else {
                        stack.push(node.left);
                        stack.push(node.right);
                    }
                } else if (hit_left) {
                    stack.push(node.left);
                } else if (hit_right) {
                    stack.push(node.right);
                }
            }
        }
    }

    /// Point query - find all bodies containing point
    void query_point(const void_math::Vec3& point,
                     std::vector<std::pair<BodyId, ShapeId>>& results) const {
        results.clear();

        if (m_root == k_null_node) return;

        std::stack<int> stack;
        stack.push(m_root);

        while (!stack.empty()) {
            int node_idx = stack.top();
            stack.pop();

            if (node_idx == k_null_node) continue;

            const BvhNode& node = m_nodes[node_idx];

            if (!void_math::contains(node.aabb, point)) {
                continue;
            }

            if (node.is_leaf) {
                results.emplace_back(node.body_id, node.shape_id);
            } else {
                stack.push(node.left);
                stack.push(node.right);
            }
        }
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get number of proxies (leaf nodes)
    [[nodiscard]] std::size_t proxy_count() const { return m_proxy_map.size(); }

    /// Get total node count
    [[nodiscard]] std::size_t node_count() const { return m_nodes.size(); }

    /// Get tree height
    [[nodiscard]] int height() const {
        if (m_root == k_null_node) return 0;
        return m_nodes[m_root].height;
    }

    /// Validate tree structure (debug)
    [[nodiscard]] bool validate() const {
        if (m_root == k_null_node) return true;
        return validate_node(m_root, k_null_node);
    }

private:
    // =========================================================================
    // Node Allocation
    // =========================================================================

    int allocate_node() {
        if (m_free_list != k_null_node) {
            int node_idx = m_free_list;
            m_free_list = m_nodes[node_idx].left;
            m_nodes[node_idx] = BvhNode{};
            return node_idx;
        }

        int node_idx = static_cast<int>(m_nodes.size());
        m_nodes.push_back(BvhNode{});
        return node_idx;
    }

    void free_node(int node_idx) {
        m_nodes[node_idx].left = m_free_list;
        m_nodes[node_idx].height = -1;
        m_free_list = node_idx;
    }

    // =========================================================================
    // Tree Operations
    // =========================================================================

    void insert_leaf(int leaf_idx) {
        if (m_root == k_null_node) {
            m_root = leaf_idx;
            m_nodes[leaf_idx].parent = k_null_node;
            return;
        }

        // Find best sibling
        void_math::AABB leaf_aabb = m_nodes[leaf_idx].aabb;
        int sibling = find_best_sibling(leaf_aabb);

        // Create new parent
        int old_parent = m_nodes[sibling].parent;
        int new_parent = allocate_node();

        m_nodes[new_parent].parent = old_parent;
        m_nodes[new_parent].aabb = void_math::combine(leaf_aabb, m_nodes[sibling].aabb);
        m_nodes[new_parent].height = m_nodes[sibling].height + 1;
        m_nodes[new_parent].is_leaf = false;

        if (old_parent != k_null_node) {
            if (m_nodes[old_parent].left == sibling) {
                m_nodes[old_parent].left = new_parent;
            } else {
                m_nodes[old_parent].right = new_parent;
            }
        } else {
            m_root = new_parent;
        }

        m_nodes[new_parent].left = sibling;
        m_nodes[new_parent].right = leaf_idx;
        m_nodes[sibling].parent = new_parent;
        m_nodes[leaf_idx].parent = new_parent;

        // Rebalance tree
        rebalance(m_nodes[leaf_idx].parent);
    }

    void remove_leaf(int leaf_idx) {
        if (leaf_idx == m_root) {
            m_root = k_null_node;
            return;
        }

        int parent = m_nodes[leaf_idx].parent;
        int grandparent = m_nodes[parent].parent;
        int sibling = (m_nodes[parent].left == leaf_idx)
            ? m_nodes[parent].right
            : m_nodes[parent].left;

        if (grandparent != k_null_node) {
            if (m_nodes[grandparent].left == parent) {
                m_nodes[grandparent].left = sibling;
            } else {
                m_nodes[grandparent].right = sibling;
            }
            m_nodes[sibling].parent = grandparent;
            free_node(parent);

            rebalance(grandparent);
        } else {
            m_root = sibling;
            m_nodes[sibling].parent = k_null_node;
            free_node(parent);
        }
    }

    int find_best_sibling(const void_math::AABB& aabb) const {
        int best = m_root;
        float best_cost = surface_area(void_math::combine(aabb, m_nodes[m_root].aabb));

        std::stack<std::pair<int, float>> stack;
        stack.emplace(m_root, 0.0f);

        while (!stack.empty()) {
            auto [node_idx, inherited_cost] = stack.top();
            stack.pop();

            const BvhNode& node = m_nodes[node_idx];
            void_math::AABB combined = void_math::combine(aabb, node.aabb);
            float direct_cost = surface_area(combined);

            float cost = direct_cost + inherited_cost;
            if (cost < best_cost) {
                best_cost = cost;
                best = node_idx;
            }

            if (!node.is_leaf) {
                float delta = direct_cost - surface_area(node.aabb);
                float child_inherited = inherited_cost + delta;

                // Lower bound on child costs
                float child_lower_bound = surface_area(aabb) + child_inherited;
                if (child_lower_bound < best_cost) {
                    stack.emplace(node.left, child_inherited);
                    stack.emplace(node.right, child_inherited);
                }
            }
        }

        return best;
    }

    void rebalance(int node_idx) {
        while (node_idx != k_null_node) {
            node_idx = balance(node_idx);

            BvhNode& node = m_nodes[node_idx];
            int left = node.left;
            int right = node.right;

            node.height = 1 + std::max(
                left != k_null_node ? m_nodes[left].height : 0,
                right != k_null_node ? m_nodes[right].height : 0
            );

            node.aabb = void_math::combine(m_nodes[left].aabb, m_nodes[right].aabb);

            node_idx = node.parent;
        }
    }

    int balance(int node_idx) {
        BvhNode& node = m_nodes[node_idx];

        if (node.is_leaf || node.height < 2) {
            return node_idx;
        }

        int left = node.left;
        int right = node.right;

        int balance_factor = m_nodes[right].height - m_nodes[left].height;

        // Rotate right
        if (balance_factor > 1) {
            int right_left = m_nodes[right].left;
            int right_right = m_nodes[right].right;

            // Swap node and right
            m_nodes[right].left = node_idx;
            m_nodes[right].parent = node.parent;
            node.parent = right;

            if (m_nodes[right].parent != k_null_node) {
                if (m_nodes[m_nodes[right].parent].left == node_idx) {
                    m_nodes[m_nodes[right].parent].left = right;
                } else {
                    m_nodes[m_nodes[right].parent].right = right;
                }
            } else {
                m_root = right;
            }

            // Rotate
            if (m_nodes[right_left].height > m_nodes[right_right].height) {
                m_nodes[right].right = right_left;
                node.right = right_right;
                m_nodes[right_right].parent = node_idx;

                node.aabb = void_math::combine(m_nodes[left].aabb, m_nodes[right_right].aabb);
                m_nodes[right].aabb = void_math::combine(node.aabb, m_nodes[right_left].aabb);

                node.height = 1 + std::max(m_nodes[left].height, m_nodes[right_right].height);
                m_nodes[right].height = 1 + std::max(node.height, m_nodes[right_left].height);
            } else {
                m_nodes[right].right = right_right;
                node.right = right_left;
                m_nodes[right_left].parent = node_idx;

                node.aabb = void_math::combine(m_nodes[left].aabb, m_nodes[right_left].aabb);
                m_nodes[right].aabb = void_math::combine(node.aabb, m_nodes[right_right].aabb);

                node.height = 1 + std::max(m_nodes[left].height, m_nodes[right_left].height);
                m_nodes[right].height = 1 + std::max(node.height, m_nodes[right_right].height);
            }

            return right;
        }

        // Rotate left
        if (balance_factor < -1) {
            int left_left = m_nodes[left].left;
            int left_right = m_nodes[left].right;

            m_nodes[left].left = node_idx;
            m_nodes[left].parent = node.parent;
            node.parent = left;

            if (m_nodes[left].parent != k_null_node) {
                if (m_nodes[m_nodes[left].parent].left == node_idx) {
                    m_nodes[m_nodes[left].parent].left = left;
                } else {
                    m_nodes[m_nodes[left].parent].right = left;
                }
            } else {
                m_root = left;
            }

            if (m_nodes[left_left].height > m_nodes[left_right].height) {
                m_nodes[left].right = left_left;
                node.left = left_right;
                m_nodes[left_right].parent = node_idx;

                node.aabb = void_math::combine(m_nodes[right].aabb, m_nodes[left_right].aabb);
                m_nodes[left].aabb = void_math::combine(node.aabb, m_nodes[left_left].aabb);

                node.height = 1 + std::max(m_nodes[right].height, m_nodes[left_right].height);
                m_nodes[left].height = 1 + std::max(node.height, m_nodes[left_left].height);
            } else {
                m_nodes[left].right = left_right;
                node.left = left_left;
                m_nodes[left_left].parent = node_idx;

                node.aabb = void_math::combine(m_nodes[right].aabb, m_nodes[left_left].aabb);
                m_nodes[left].aabb = void_math::combine(node.aabb, m_nodes[left_right].aabb);

                node.height = 1 + std::max(m_nodes[right].height, m_nodes[left_left].height);
                m_nodes[left].height = 1 + std::max(node.height, m_nodes[left_right].height);
            }

            return left;
        }

        return node_idx;
    }

    // =========================================================================
    // Query Helpers
    // =========================================================================

    void query_subtree(int node_idx, const void_math::AABB& query_aabb,
                       BodyId exclude_body, ShapeId exclude_shape,
                       std::vector<CollisionPair>& pairs) const {
        if (node_idx == k_null_node) return;

        std::stack<int> stack;
        stack.push(node_idx);

        while (!stack.empty()) {
            int idx = stack.top();
            stack.pop();

            if (idx == k_null_node) continue;

            const BvhNode& node = m_nodes[idx];

            if (!void_math::intersects(node.aabb, query_aabb)) {
                continue;
            }

            if (node.is_leaf) {
                // Don't self-collide
                if (node.body_id != exclude_body || node.shape_id != exclude_shape) {
                    // Order bodies consistently
                    CollisionPair pair;
                    if (exclude_body.value < node.body_id.value) {
                        pair.body_a = exclude_body;
                        pair.body_b = node.body_id;
                        pair.shape_a = exclude_shape;
                        pair.shape_b = node.shape_id;
                    } else {
                        pair.body_a = node.body_id;
                        pair.body_b = exclude_body;
                        pair.shape_a = node.shape_id;
                        pair.shape_b = exclude_shape;
                    }
                    pairs.push_back(pair);
                }
            } else {
                stack.push(node.left);
                stack.push(node.right);
            }
        }
    }

    [[nodiscard]] static bool ray_aabb_intersect(const void_math::Vec3& origin,
                                                  const void_math::Vec3& inv_dir,
                                                  const void_math::AABB& aabb,
                                                  float max_dist,
                                                  float& t_out) {
        float t1 = (aabb.min.x - origin.x) * inv_dir.x;
        float t2 = (aabb.max.x - origin.x) * inv_dir.x;
        float t3 = (aabb.min.y - origin.y) * inv_dir.y;
        float t4 = (aabb.max.y - origin.y) * inv_dir.y;
        float t5 = (aabb.min.z - origin.z) * inv_dir.z;
        float t6 = (aabb.max.z - origin.z) * inv_dir.z;

        float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
        float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

        if (tmax < 0 || tmin > tmax || tmin > max_dist) {
            return false;
        }

        t_out = tmin >= 0 ? tmin : tmax;
        return true;
    }

    [[nodiscard]] static bool contains(const void_math::AABB& outer, const void_math::AABB& inner) {
        return outer.min.x <= inner.min.x && outer.max.x >= inner.max.x &&
               outer.min.y <= inner.min.y && outer.max.y >= inner.max.y &&
               outer.min.z <= inner.min.z && outer.max.z >= inner.max.z;
    }

    [[nodiscard]] static float surface_area(const void_math::AABB& aabb) {
        void_math::Vec3 d = aabb.max - aabb.min;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    [[nodiscard]] bool validate_node(int node_idx, int expected_parent) const {
        if (node_idx == k_null_node) return true;

        const BvhNode& node = m_nodes[node_idx];

        if (node.parent != expected_parent) return false;

        if (node.is_leaf) {
            if (node.left != k_null_node || node.right != k_null_node) return false;
            if (node.height != 0) return false;
        } else {
            if (!validate_node(node.left, node_idx)) return false;
            if (!validate_node(node.right, node_idx)) return false;

            int expected_height = 1 + std::max(
                m_nodes[node.left].height,
                m_nodes[node.right].height
            );
            if (node.height != expected_height) return false;
        }

        return true;
    }

private:
    std::vector<BvhNode> m_nodes;
    std::unordered_map<BodyShapeKey, int,
        std::hash<std::pair<BodyId, ShapeId>>> m_proxy_map;
    int m_root = k_null_node;
    int m_free_list = k_null_node;
};

} // namespace void_physics

// Hash for pair<BodyId, ShapeId>
template<>
struct std::hash<std::pair<void_physics::BodyId, void_physics::ShapeId>> {
    std::size_t operator()(const std::pair<void_physics::BodyId, void_physics::ShapeId>& p) const noexcept {
        return std::hash<std::uint64_t>{}(p.first.value) ^
               (std::hash<std::uint64_t>{}(p.second.value) << 1);
    }
};
