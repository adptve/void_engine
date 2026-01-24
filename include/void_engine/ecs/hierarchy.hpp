#pragma once

/// @file hierarchy.hpp
/// @brief Entity hierarchy system for void_ecs
///
/// Provides parent-child relationships between entities with automatic
/// transform propagation. Supports hierarchical scene graphs.

#include "fwd.hpp"
#include "entity.hpp"
#include "component.hpp"
#include "world.hpp"
#include "query.hpp"

#include <vector>
#include <algorithm>
#include <cmath>
#include <array>

namespace void_ecs {

// =============================================================================
// Transform Types (using simple arrays for compatibility)
// =============================================================================

/// 3D Vector type
struct Vec3 {
    float x{0}, y{0}, z{0};

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    [[nodiscard]] float length() const { return std::sqrt(x*x + y*y + z*z); }
    [[nodiscard]] Vec3 normalized() const {
        float len = length();
        if (len > 0.0001f) return *this * (1.0f / len);
        return {0, 0, 0};
    }

    static constexpr Vec3 zero() { return {0, 0, 0}; }
    static constexpr Vec3 one() { return {1, 1, 1}; }
    static constexpr Vec3 up() { return {0, 1, 0}; }
    static constexpr Vec3 forward() { return {0, 0, 1}; }
    static constexpr Vec3 right() { return {1, 0, 0}; }
};

/// Quaternion for rotations
struct Quat {
    float x{0}, y{0}, z{0}, w{1};

    constexpr Quat() = default;
    constexpr Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    /// Create from axis-angle (radians)
    [[nodiscard]] static Quat from_axis_angle(const Vec3& axis, float angle) {
        float half = angle * 0.5f;
        float s = std::sin(half);
        Vec3 n = axis.normalized();
        return {n.x * s, n.y * s, n.z * s, std::cos(half)};
    }

    /// Create from Euler angles (radians, XYZ order)
    [[nodiscard]] static Quat from_euler(const Vec3& euler) {
        float cx = std::cos(euler.x * 0.5f);
        float sx = std::sin(euler.x * 0.5f);
        float cy = std::cos(euler.y * 0.5f);
        float sy = std::sin(euler.y * 0.5f);
        float cz = std::cos(euler.z * 0.5f);
        float sz = std::sin(euler.z * 0.5f);

        return {
            sx * cy * cz - cx * sy * sz,
            cx * sy * cz + sx * cy * sz,
            cx * cy * sz - sx * sy * cz,
            cx * cy * cz + sx * sy * sz
        };
    }

    /// Identity quaternion
    [[nodiscard]] static constexpr Quat identity() { return {0, 0, 0, 1}; }

    /// Multiply quaternions
    [[nodiscard]] Quat operator*(const Quat& other) const {
        return {
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z
        };
    }

    /// Rotate a vector
    [[nodiscard]] Vec3 rotate(const Vec3& v) const {
        Vec3 qv{x, y, z};
        Vec3 uv = Vec3{
            qv.y * v.z - qv.z * v.y,
            qv.z * v.x - qv.x * v.z,
            qv.x * v.y - qv.y * v.x
        };
        Vec3 uuv = Vec3{
            qv.y * uv.z - qv.z * uv.y,
            qv.z * uv.x - qv.x * uv.z,
            qv.x * uv.y - qv.y * uv.x
        };
        return v + (uv * w + uuv) * 2.0f;
    }

    /// Normalize
    [[nodiscard]] Quat normalized() const {
        float len = std::sqrt(x*x + y*y + z*z + w*w);
        if (len > 0.0001f) {
            float inv = 1.0f / len;
            return {x * inv, y * inv, z * inv, w * inv};
        }
        return identity();
    }

    /// Inverse
    [[nodiscard]] Quat inverse() const {
        float len_sq = x*x + y*y + z*z + w*w;
        if (len_sq > 0.0001f) {
            float inv = 1.0f / len_sq;
            return {-x * inv, -y * inv, -z * inv, w * inv};
        }
        return identity();
    }
};

/// 4x4 Transform matrix (column-major)
struct Mat4 {
    std::array<float, 16> m{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    constexpr Mat4() = default;

    [[nodiscard]] static constexpr Mat4 identity() { return {}; }

    /// Create from translation, rotation, scale
    [[nodiscard]] static Mat4 from_trs(const Vec3& translation, const Quat& rotation, const Vec3& scale) {
        Mat4 result;

        // Rotation matrix from quaternion
        float xx = rotation.x * rotation.x;
        float yy = rotation.y * rotation.y;
        float zz = rotation.z * rotation.z;
        float xy = rotation.x * rotation.y;
        float xz = rotation.x * rotation.z;
        float yz = rotation.y * rotation.z;
        float wx = rotation.w * rotation.x;
        float wy = rotation.w * rotation.y;
        float wz = rotation.w * rotation.z;

        result.m[0]  = (1.0f - 2.0f * (yy + zz)) * scale.x;
        result.m[1]  = (2.0f * (xy + wz)) * scale.x;
        result.m[2]  = (2.0f * (xz - wy)) * scale.x;
        result.m[3]  = 0.0f;

        result.m[4]  = (2.0f * (xy - wz)) * scale.y;
        result.m[5]  = (1.0f - 2.0f * (xx + zz)) * scale.y;
        result.m[6]  = (2.0f * (yz + wx)) * scale.y;
        result.m[7]  = 0.0f;

        result.m[8]  = (2.0f * (xz + wy)) * scale.z;
        result.m[9]  = (2.0f * (yz - wx)) * scale.z;
        result.m[10] = (1.0f - 2.0f * (xx + yy)) * scale.z;
        result.m[11] = 0.0f;

        result.m[12] = translation.x;
        result.m[13] = translation.y;
        result.m[14] = translation.z;
        result.m[15] = 1.0f;

        return result;
    }

    /// Matrix multiply
    [[nodiscard]] Mat4 operator*(const Mat4& other) const {
        Mat4 result;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                result.m[col * 4 + row] =
                    m[0 * 4 + row] * other.m[col * 4 + 0] +
                    m[1 * 4 + row] * other.m[col * 4 + 1] +
                    m[2 * 4 + row] * other.m[col * 4 + 2] +
                    m[3 * 4 + row] * other.m[col * 4 + 3];
            }
        }
        return result;
    }

    /// Transform a point
    [[nodiscard]] Vec3 transform_point(const Vec3& p) const {
        float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
        return {
            (m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12]) / w,
            (m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13]) / w,
            (m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]) / w
        };
    }

    /// Get translation
    [[nodiscard]] Vec3 translation() const {
        return {m[12], m[13], m[14]};
    }
};

// =============================================================================
// Hierarchy Components
// =============================================================================

/// Parent component - reference to parent entity
struct Parent {
    Entity entity;

    Parent() : entity(Entity::null()) {}
    explicit Parent(Entity e) : entity(e) {}
};

/// Children component - list of child entities
struct Children {
    std::vector<Entity> entities;

    Children() = default;
    explicit Children(std::initializer_list<Entity> list) : entities(list) {}

    void add(Entity child) {
        if (std::find(entities.begin(), entities.end(), child) == entities.end()) {
            entities.push_back(child);
        }
    }

    void remove(Entity child) {
        entities.erase(std::remove(entities.begin(), entities.end(), child), entities.end());
    }

    [[nodiscard]] bool contains(Entity child) const {
        return std::find(entities.begin(), entities.end(), child) != entities.end();
    }

    [[nodiscard]] std::size_t count() const { return entities.size(); }
    [[nodiscard]] bool empty() const { return entities.empty(); }
};

/// Local transform - position/rotation/scale relative to parent
struct LocalTransform {
    Vec3 position{0, 0, 0};
    Quat rotation{0, 0, 0, 1};
    Vec3 scale{1, 1, 1};

    LocalTransform() = default;
    LocalTransform(const Vec3& pos, const Quat& rot, const Vec3& scl)
        : position(pos), rotation(rot), scale(scl) {}

    [[nodiscard]] static LocalTransform from_position(const Vec3& pos) {
        return {pos, Quat::identity(), Vec3::one()};
    }

    [[nodiscard]] static LocalTransform identity() {
        return {{0, 0, 0}, Quat::identity(), {1, 1, 1}};
    }

    [[nodiscard]] Mat4 to_matrix() const {
        return Mat4::from_trs(position, rotation, scale);
    }
};

/// Global transform - computed world-space transform
struct GlobalTransform {
    Mat4 matrix{Mat4::identity()};

    GlobalTransform() = default;
    explicit GlobalTransform(const Mat4& m) : matrix(m) {}

    [[nodiscard]] static GlobalTransform identity() {
        return GlobalTransform{Mat4::identity()};
    }

    [[nodiscard]] Vec3 position() const { return matrix.translation(); }

    [[nodiscard]] Vec3 transform_point(const Vec3& p) const {
        return matrix.transform_point(p);
    }
};

/// Hierarchy depth - depth in hierarchy tree (root = 0)
struct HierarchyDepth {
    std::uint32_t depth{0};

    HierarchyDepth() = default;
    explicit HierarchyDepth(std::uint32_t d) : depth(d) {}
};

/// Visibility component
struct Visible {
    bool visible{true};

    Visible() = default;
    explicit Visible(bool v) : visible(v) {}
};

/// Inherited visibility - computed from parent chain
struct InheritedVisibility {
    bool visible{true};

    InheritedVisibility() = default;
    explicit InheritedVisibility(bool v) : visible(v) {}
};

// =============================================================================
// Hierarchy Commands
// =============================================================================

/// Set parent of an entity
inline void set_parent(World& world, Entity child, Entity parent) {
    // Remove from old parent's children
    if (world.has_component<Parent>(child)) {
        const Parent* old_parent = world.get_component<Parent>(child);
        if (old_parent && world.is_alive(old_parent->entity)) {
            Children* old_children = world.get_component<Children>(old_parent->entity);
            if (old_children) {
                old_children->remove(child);
            }
        }
    }

    // Set new parent
    world.add_component(child, Parent{parent});

    // Add to new parent's children
    if (world.is_alive(parent)) {
        if (!world.has_component<Children>(parent)) {
            world.add_component(parent, Children{});
        }
        Children* children = world.get_component<Children>(parent);
        if (children) {
            children->add(child);
        }
    }
}

/// Remove parent from entity (make root)
inline void remove_parent(World& world, Entity child) {
    if (!world.has_component<Parent>(child)) {
        return;
    }

    const Parent* parent = world.get_component<Parent>(child);
    if (parent && world.is_alive(parent->entity)) {
        Children* children = world.get_component<Children>(parent->entity);
        if (children) {
            children->remove(child);
        }
    }

    world.remove_component<Parent>(child);
}

/// Despawn entity and all descendants
inline void despawn_recursive(World& world, Entity entity) {
    // First, despawn all children recursively
    if (world.has_component<Children>(entity)) {
        const Children* children = world.get_component<Children>(entity);
        if (children) {
            // Copy the list since we're modifying it
            std::vector<Entity> to_despawn = children->entities;
            for (Entity child : to_despawn) {
                despawn_recursive(world, child);
            }
        }
    }

    // Remove from parent's children list
    remove_parent(world, entity);

    // Despawn the entity itself
    world.despawn(entity);
}

// =============================================================================
// Hierarchy Validation
// =============================================================================

/// Check for cycles in hierarchy (returns true if cycle detected)
inline bool has_hierarchy_cycle(const World& world, Entity child, Entity new_parent) {
    Entity current = new_parent;
    while (world.is_alive(current)) {
        if (current == child) {
            return true;  // Cycle detected
        }

        const Parent* parent = world.get_component<Parent>(current);
        if (!parent || parent->entity.is_null()) {
            break;
        }
        current = parent->entity;
    }
    return false;
}

// =============================================================================
// Transform Propagation
// =============================================================================

/// Update global transforms for all entities
/// Should be called each frame after local transforms change
inline void propagate_transforms(World& world) {
    // Get component IDs
    auto local_id = world.component_id<LocalTransform>();
    auto global_id = world.component_id<GlobalTransform>();
    auto parent_id = world.component_id<Parent>();
    auto depth_id = world.component_id<HierarchyDepth>();

    if (!local_id || !global_id) {
        return;  // Components not registered
    }

    // First pass: Update depth and find roots
    std::vector<Entity> roots;

    // Query all entities with transforms
    auto query_desc = QueryDescriptor()
        .read(*local_id)
        .build();

    QueryState state = world.query(query_desc);
    QueryIter iter = world.query_iter(state);

    while (!iter.empty()) {
        Entity entity = iter.entity();

        // Calculate depth
        std::uint32_t depth = 0;
        Entity current = entity;

        while (true) {
            const Parent* p = world.get_component<Parent>(current);
            if (!p || p->entity.is_null() || !world.is_alive(p->entity)) {
                break;
            }
            ++depth;
            current = p->entity;
        }

        // Update depth component
        if (world.has_component<HierarchyDepth>(entity)) {
            HierarchyDepth* d = world.get_component<HierarchyDepth>(entity);
            if (d) d->depth = depth;
        } else {
            world.add_component(entity, HierarchyDepth{depth});
        }

        // Track roots
        if (depth == 0) {
            roots.push_back(entity);
        }

        iter.next();
    }

    // Second pass: Propagate transforms from roots
    std::vector<Entity> current_level = roots;
    std::vector<Entity> next_level;

    while (!current_level.empty()) {
        for (Entity entity : current_level) {
            // Get local transform
            const LocalTransform* local = world.get_component<LocalTransform>(entity);
            if (!local) continue;

            // Compute global transform
            Mat4 global_mat = local->to_matrix();

            // If has parent, multiply by parent's global
            const Parent* parent = world.get_component<Parent>(entity);
            if (parent && world.is_alive(parent->entity)) {
                const GlobalTransform* parent_global = world.get_component<GlobalTransform>(parent->entity);
                if (parent_global) {
                    global_mat = parent_global->matrix * global_mat;
                }
            }

            // Set global transform
            if (world.has_component<GlobalTransform>(entity)) {
                GlobalTransform* g = world.get_component<GlobalTransform>(entity);
                if (g) g->matrix = global_mat;
            } else {
                world.add_component(entity, GlobalTransform{global_mat});
            }

            // Queue children for next level
            const Children* children = world.get_component<Children>(entity);
            if (children) {
                for (Entity child : children->entities) {
                    if (world.is_alive(child)) {
                        next_level.push_back(child);
                    }
                }
            }
        }

        current_level = std::move(next_level);
        next_level.clear();
    }
}

/// Update visibility inheritance for all entities
inline void propagate_visibility(World& world) {
    auto visible_id = world.component_id<Visible>();
    auto inherited_id = world.component_id<InheritedVisibility>();
    auto parent_id = world.component_id<Parent>();
    auto children_id = world.component_id<Children>();

    if (!visible_id) return;

    // Find roots with visibility
    std::vector<Entity> roots;

    auto query_desc = QueryDescriptor()
        .read(*visible_id)
        .build();

    QueryState state = world.query(query_desc);
    QueryIter iter = world.query_iter(state);

    while (!iter.empty()) {
        Entity entity = iter.entity();

        // Check if root
        const Parent* p = world.get_component<Parent>(entity);
        if (!p || p->entity.is_null() || !world.is_alive(p->entity)) {
            roots.push_back(entity);
        }

        iter.next();
    }

    // Propagate from roots
    std::vector<Entity> current_level = roots;
    std::vector<Entity> next_level;

    while (!current_level.empty()) {
        for (Entity entity : current_level) {
            // Get visibility
            const Visible* vis = world.get_component<Visible>(entity);
            bool entity_visible = vis ? vis->visible : true;

            // Get parent's inherited visibility
            bool parent_visible = true;
            const Parent* parent = world.get_component<Parent>(entity);
            if (parent && world.is_alive(parent->entity)) {
                const InheritedVisibility* parent_inh = world.get_component<InheritedVisibility>(parent->entity);
                if (parent_inh) {
                    parent_visible = parent_inh->visible;
                }
            }

            // Compute inherited visibility
            bool inherited = entity_visible && parent_visible;

            // Set inherited visibility
            if (world.has_component<InheritedVisibility>(entity)) {
                InheritedVisibility* inh = world.get_component<InheritedVisibility>(entity);
                if (inh) inh->visible = inherited;
            } else {
                world.add_component(entity, InheritedVisibility{inherited});
            }

            // Queue children
            const Children* children = world.get_component<Children>(entity);
            if (children) {
                for (Entity child : children->entities) {
                    if (world.is_alive(child)) {
                        next_level.push_back(child);
                    }
                }
            }
        }

        current_level = std::move(next_level);
        next_level.clear();
    }
}

// =============================================================================
// Hierarchy System (for SystemScheduler)
// =============================================================================

/// Create a system that propagates transforms
[[nodiscard]] inline std::unique_ptr<System> make_transform_propagation_system() {
    return make_system(
        SystemDescriptor("TransformPropagation")
            .set_stage(SystemStage::PostUpdate),
        [](World& world) {
            propagate_transforms(world);
        }
    );
}

/// Create a system that propagates visibility
[[nodiscard]] inline std::unique_ptr<System> make_visibility_propagation_system() {
    return make_system(
        SystemDescriptor("VisibilityPropagation")
            .set_stage(SystemStage::PostUpdate),
        [](World& world) {
            propagate_visibility(world);
        }
    );
}

} // namespace void_ecs
