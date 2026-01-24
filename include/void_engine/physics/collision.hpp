/// @file collision.hpp
/// @brief Collision detection algorithms for void_physics
///
/// Implements GJK (Gilbert-Johnson-Keerthi) for intersection detection
/// and EPA (Expanding Polytope Algorithm) for contact generation.

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "shape.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/quat.hpp>
#include <void_engine/math/bounds.hpp>

#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace void_physics {

// =============================================================================
// Constants
// =============================================================================

/// GJK/EPA tolerance
constexpr float k_collision_epsilon = 1e-6f;

/// Maximum GJK iterations
constexpr int k_max_gjk_iterations = 64;

/// Maximum EPA iterations
constexpr int k_max_epa_iterations = 64;

/// Maximum EPA faces
constexpr int k_max_epa_faces = 256;

// =============================================================================
// Simplex
// =============================================================================

/// Support point with Minkowski difference tracking
struct SupportPoint {
    void_math::Vec3 point;      ///< Point in Minkowski difference
    void_math::Vec3 support_a;  ///< Support point on shape A
    void_math::Vec3 support_b;  ///< Support point on shape B
};

/// GJK Simplex (0-4 vertices)
class Simplex {
public:
    Simplex() = default;

    /// Add a point to the simplex
    void push_front(const SupportPoint& point) {
        m_points[3] = m_points[2];
        m_points[2] = m_points[1];
        m_points[1] = m_points[0];
        m_points[0] = point;
        m_size = std::min(m_size + 1, 4);
    }

    /// Get point at index
    [[nodiscard]] const SupportPoint& operator[](int i) const { return m_points[i]; }
    [[nodiscard]] SupportPoint& operator[](int i) { return m_points[i]; }

    /// Get size
    [[nodiscard]] int size() const { return m_size; }

    /// Clear simplex
    void clear() { m_size = 0; }

    /// Assign from 2 points
    void set_line(const SupportPoint& a, const SupportPoint& b) {
        m_points[0] = a;
        m_points[1] = b;
        m_size = 2;
    }

    /// Assign from 3 points
    void set_triangle(const SupportPoint& a, const SupportPoint& b, const SupportPoint& c) {
        m_points[0] = a;
        m_points[1] = b;
        m_points[2] = c;
        m_size = 3;
    }

    /// Assign from 4 points
    void set_tetrahedron(const SupportPoint& a, const SupportPoint& b,
                          const SupportPoint& c, const SupportPoint& d) {
        m_points[0] = a;
        m_points[1] = b;
        m_points[2] = c;
        m_points[3] = d;
        m_size = 4;
    }

private:
    std::array<SupportPoint, 4> m_points;
    int m_size = 0;
};

// =============================================================================
// GJK Result
// =============================================================================

/// Result of GJK intersection test
struct GjkResult {
    bool intersecting = false;      ///< Shapes are intersecting
    Simplex simplex;                ///< Final simplex (for EPA)
    void_math::Vec3 direction;      ///< Last search direction
    int iterations = 0;             ///< Iterations used
};

// =============================================================================
// Contact Manifold
// =============================================================================

/// Single contact point
struct Contact {
    void_math::Vec3 point_a;        ///< Contact point on shape A (world space)
    void_math::Vec3 point_b;        ///< Contact point on shape B (world space)
    void_math::Vec3 normal;         ///< Contact normal (A -> B)
    float depth = 0.0f;             ///< Penetration depth
    float friction = 0.5f;          ///< Combined friction
    float restitution = 0.0f;       ///< Combined restitution
};

/// Contact manifold (multiple contact points)
struct ContactManifold {
    BodyId body_a;
    BodyId body_b;
    ShapeId shape_a;
    ShapeId shape_b;
    std::vector<Contact> contacts;
    bool is_sensor = false;         ///< True if either shape is a trigger

    /// Get number of contacts
    [[nodiscard]] std::size_t size() const { return contacts.size(); }

    /// Check if empty
    [[nodiscard]] bool empty() const { return contacts.empty(); }

    /// Get average contact normal
    [[nodiscard]] void_math::Vec3 average_normal() const {
        if (contacts.empty()) return {0, 1, 0};
        void_math::Vec3 sum{0, 0, 0};
        for (const auto& c : contacts) {
            sum = sum + c.normal;
        }
        return void_math::normalize(sum);
    }

    /// Get deepest penetration
    [[nodiscard]] float max_depth() const {
        float max_d = 0.0f;
        for (const auto& c : contacts) {
            max_d = std::max(max_d, c.depth);
        }
        return max_d;
    }
};

// =============================================================================
// EPA Face
// =============================================================================

/// EPA polytope face
struct EpaFace {
    std::array<int, 3> indices;     ///< Vertex indices
    void_math::Vec3 normal;         ///< Face normal
    float distance = 0.0f;          ///< Distance from origin
};

// =============================================================================
// Collision Detection Class
// =============================================================================

/// Collision detection using GJK and EPA algorithms
class CollisionDetector {
public:
    // =========================================================================
    // Shape Transform
    // =========================================================================

    /// Transformed shape wrapper for collision queries
    struct TransformedShape {
        const IShape* shape = nullptr;
        void_math::Vec3 position{0, 0, 0};
        void_math::Quat rotation{};

        /// Get support point in world space
        [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& world_dir) const {
            // Transform direction to local space
            void_math::Quat inv_rot = void_math::conjugate(rotation);
            void_math::Vec3 local_dir = void_math::rotate(inv_rot, world_dir);

            // Get support in local space
            void_math::Vec3 local_support = shape->support(local_dir);

            // Transform back to world space
            return position + void_math::rotate(rotation, local_support);
        }

        /// Get world-space AABB
        [[nodiscard]] void_math::AABB world_bounds() const {
            void_math::AABB local = shape->local_bounds();
            // Transform AABB (conservative approximation)
            void_math::Vec3 corners[8] = {
                {local.min.x, local.min.y, local.min.z},
                {local.max.x, local.min.y, local.min.z},
                {local.min.x, local.max.y, local.min.z},
                {local.max.x, local.max.y, local.min.z},
                {local.min.x, local.min.y, local.max.z},
                {local.max.x, local.min.y, local.max.z},
                {local.min.x, local.max.y, local.max.z},
                {local.max.x, local.max.y, local.max.z},
            };

            void_math::Vec3 world_min = position + void_math::rotate(rotation, corners[0]);
            void_math::Vec3 world_max = world_min;

            for (int i = 1; i < 8; ++i) {
                void_math::Vec3 world_corner = position + void_math::rotate(rotation, corners[i]);
                world_min.x = std::min(world_min.x, world_corner.x);
                world_min.y = std::min(world_min.y, world_corner.y);
                world_min.z = std::min(world_min.z, world_corner.z);
                world_max.x = std::max(world_max.x, world_corner.x);
                world_max.y = std::max(world_max.y, world_corner.y);
                world_max.z = std::max(world_max.z, world_corner.z);
            }

            return {world_min, world_max};
        }
    };

    // =========================================================================
    // GJK Algorithm
    // =========================================================================

    /// Run GJK intersection test
    [[nodiscard]] static GjkResult gjk(const TransformedShape& shape_a,
                                        const TransformedShape& shape_b) {
        GjkResult result;
        result.direction = {1, 0, 0};

        // Get initial support point
        SupportPoint support = get_support(shape_a, shape_b, result.direction);
        result.simplex.push_front(support);

        // Search toward origin
        result.direction = -support.point;

        for (int i = 0; i < k_max_gjk_iterations; ++i) {
            result.iterations = i + 1;

            // Normalize direction
            float dir_len = void_math::length(result.direction);
            if (dir_len < k_collision_epsilon) {
                // Origin on simplex - intersection
                result.intersecting = true;
                return result;
            }
            result.direction = result.direction / dir_len;

            // Get new support point
            support = get_support(shape_a, shape_b, result.direction);

            // Check if we passed the origin
            float dot = void_math::dot(support.point, result.direction);
            if (dot < 0) {
                // No intersection
                result.intersecting = false;
                return result;
            }

            // Add to simplex
            result.simplex.push_front(support);

            // Process simplex and get new direction
            if (do_simplex(result.simplex, result.direction)) {
                result.intersecting = true;
                return result;
            }
        }

        // Didn't converge - assume no intersection
        result.intersecting = false;
        return result;
    }

    // =========================================================================
    // EPA Algorithm
    // =========================================================================

    /// Run EPA to get contact information
    [[nodiscard]] static std::optional<Contact> epa(const TransformedShape& shape_a,
                                                     const TransformedShape& shape_b,
                                                     const Simplex& simplex) {
        // Build initial polytope from simplex
        std::vector<SupportPoint> vertices;
        std::vector<EpaFace> faces;

        // Ensure we have a tetrahedron
        if (simplex.size() < 4) {
            return std::nullopt;
        }

        // Copy simplex vertices
        for (int i = 0; i < 4; ++i) {
            vertices.push_back(simplex[i]);
        }

        // Create initial faces (tetrahedron)
        // Order vertices so normals point outward
        faces.push_back(make_face(vertices, 0, 1, 2));
        faces.push_back(make_face(vertices, 0, 3, 1));
        faces.push_back(make_face(vertices, 0, 2, 3));
        faces.push_back(make_face(vertices, 1, 3, 2));

        // Fix winding if needed
        for (auto& face : faces) {
            if (face.distance < 0) {
                std::swap(face.indices[0], face.indices[1]);
                face.normal = -face.normal;
                face.distance = -face.distance;
            }
        }

        // Expand polytope
        for (int iter = 0; iter < k_max_epa_iterations; ++iter) {
            // Find closest face to origin
            int closest_face = 0;
            float min_dist = faces[0].distance;

            for (std::size_t i = 1; i < faces.size(); ++i) {
                if (faces[i].distance < min_dist) {
                    min_dist = faces[i].distance;
                    closest_face = static_cast<int>(i);
                }
            }

            // Get support point in face normal direction
            SupportPoint support = get_support(shape_a, shape_b, faces[closest_face].normal);

            // Check if we're done expanding
            float support_dist = void_math::dot(support.point, faces[closest_face].normal);
            if (support_dist - min_dist < k_collision_epsilon) {
                // Converged - build contact from closest face
                const EpaFace& face = faces[closest_face];

                Contact contact;
                contact.normal = face.normal;
                contact.depth = face.distance;

                // Barycentric interpolation for contact points
                const SupportPoint& v0 = vertices[face.indices[0]];
                const SupportPoint& v1 = vertices[face.indices[1]];
                const SupportPoint& v2 = vertices[face.indices[2]];

                // Project origin onto face to get barycentric coordinates
                void_math::Vec3 closest = face.normal * face.distance;
                auto [u, v, w] = barycentric(closest, v0.point, v1.point, v2.point);

                contact.point_a = v0.support_a * u + v1.support_a * v + v2.support_a * w;
                contact.point_b = v0.support_b * u + v1.support_b * v + v2.support_b * w;

                return contact;
            }

            // Add support point to polytope
            int new_vertex = static_cast<int>(vertices.size());
            vertices.push_back(support);

            // Remove faces visible from new point
            std::vector<std::pair<int, int>> horizon;
            std::vector<EpaFace> remaining_faces;

            for (const auto& face : faces) {
                void_math::Vec3 to_point = support.point - vertices[face.indices[0]].point;
                if (void_math::dot(face.normal, to_point) > k_collision_epsilon) {
                    // Face is visible - add edges to horizon
                    add_edge(horizon, face.indices[0], face.indices[1]);
                    add_edge(horizon, face.indices[1], face.indices[2]);
                    add_edge(horizon, face.indices[2], face.indices[0]);
                } else {
                    remaining_faces.push_back(face);
                }
            }

            faces = std::move(remaining_faces);

            // Create new faces from horizon edges to new vertex
            for (const auto& [i, j] : horizon) {
                EpaFace new_face = make_face(vertices, i, j, new_vertex);
                if (new_face.distance >= 0) {
                    faces.push_back(new_face);
                }
            }

            // Safety check
            if (faces.empty() || faces.size() > k_max_epa_faces) {
                break;
            }
        }

        return std::nullopt;
    }

    // =========================================================================
    // High-Level Collision Test
    // =========================================================================

    /// Test collision between two shapes and generate contacts
    [[nodiscard]] static std::optional<ContactManifold> collide(
        const TransformedShape& shape_a,
        const TransformedShape& shape_b,
        BodyId body_a,
        BodyId body_b) {

        // AABB early-out
        void_math::AABB aabb_a = shape_a.world_bounds();
        void_math::AABB aabb_b = shape_b.world_bounds();

        if (!void_math::intersects(aabb_a, aabb_b)) {
            return std::nullopt;
        }

        // Run GJK
        GjkResult gjk_result = gjk(shape_a, shape_b);
        if (!gjk_result.intersecting) {
            return std::nullopt;
        }

        // Run EPA for contact information
        auto contact = epa(shape_a, shape_b, gjk_result.simplex);
        if (!contact) {
            return std::nullopt;
        }

        // Build manifold
        ContactManifold manifold;
        manifold.body_a = body_a;
        manifold.body_b = body_b;
        manifold.shape_a = shape_a.shape->id();
        manifold.shape_b = shape_b.shape->id();
        manifold.contacts.push_back(*contact);

        return manifold;
    }

    // =========================================================================
    // Specialized Collision Tests
    // =========================================================================

    /// Sphere-Sphere collision (optimized)
    [[nodiscard]] static std::optional<ContactManifold> collide_sphere_sphere(
        const void_math::Vec3& pos_a, float radius_a,
        const void_math::Vec3& pos_b, float radius_b,
        BodyId body_a, BodyId body_b,
        ShapeId shape_a, ShapeId shape_b) {

        void_math::Vec3 diff = pos_b - pos_a;
        float dist_sq = void_math::dot(diff, diff);
        float sum_radius = radius_a + radius_b;

        if (dist_sq >= sum_radius * sum_radius) {
            return std::nullopt;
        }

        float dist = std::sqrt(dist_sq);

        ContactManifold manifold;
        manifold.body_a = body_a;
        manifold.body_b = body_b;
        manifold.shape_a = shape_a;
        manifold.shape_b = shape_b;

        Contact contact;
        if (dist < k_collision_epsilon) {
            // Centers coincide - pick arbitrary normal
            contact.normal = {0, 1, 0};
            contact.depth = sum_radius;
            contact.point_a = pos_a;
            contact.point_b = pos_b;
        } else {
            contact.normal = diff / dist;
            contact.depth = sum_radius - dist;
            contact.point_a = pos_a + contact.normal * radius_a;
            contact.point_b = pos_b - contact.normal * radius_b;
        }

        manifold.contacts.push_back(contact);
        return manifold;
    }

    /// Sphere-Plane collision (optimized)
    [[nodiscard]] static std::optional<ContactManifold> collide_sphere_plane(
        const void_math::Vec3& sphere_pos, float radius,
        const void_math::Vec3& plane_normal, float plane_dist,
        BodyId body_sphere, BodyId body_plane,
        ShapeId shape_sphere, ShapeId shape_plane) {

        float sphere_dist = void_math::dot(sphere_pos, plane_normal) - plane_dist;

        if (sphere_dist >= radius) {
            return std::nullopt;
        }

        ContactManifold manifold;
        manifold.body_a = body_sphere;
        manifold.body_b = body_plane;
        manifold.shape_a = shape_sphere;
        manifold.shape_b = shape_plane;

        Contact contact;
        contact.normal = -plane_normal;  // Points from plane to sphere
        contact.depth = radius - sphere_dist;
        contact.point_a = sphere_pos - plane_normal * radius;
        contact.point_b = sphere_pos - plane_normal * sphere_dist;

        manifold.contacts.push_back(contact);
        return manifold;
    }

    /// Box-Plane collision (optimized)
    [[nodiscard]] static std::optional<ContactManifold> collide_box_plane(
        const void_math::Vec3& box_pos, const void_math::Quat& box_rot,
        const void_math::Vec3& half_extents,
        const void_math::Vec3& plane_normal, float plane_dist,
        BodyId body_box, BodyId body_plane,
        ShapeId shape_box, ShapeId shape_plane) {

        // Get box vertices in world space
        void_math::Vec3 corners[8];
        int corner_idx = 0;
        for (int x = -1; x <= 1; x += 2) {
            for (int y = -1; y <= 1; y += 2) {
                for (int z = -1; z <= 1; z += 2) {
                    void_math::Vec3 local = half_extents * void_math::Vec3{
                        static_cast<float>(x),
                        static_cast<float>(y),
                        static_cast<float>(z)
                    };
                    corners[corner_idx++] = box_pos + void_math::rotate(box_rot, local);
                }
            }
        }

        // Find penetrating corners
        ContactManifold manifold;
        manifold.body_a = body_box;
        manifold.body_b = body_plane;
        manifold.shape_a = shape_box;
        manifold.shape_b = shape_plane;

        for (int i = 0; i < 8; ++i) {
            float dist = void_math::dot(corners[i], plane_normal) - plane_dist;
            if (dist < 0) {
                Contact contact;
                contact.normal = -plane_normal;
                contact.depth = -dist;
                contact.point_a = corners[i];
                contact.point_b = corners[i] - plane_normal * dist;
                manifold.contacts.push_back(contact);
            }
        }

        if (manifold.contacts.empty()) {
            return std::nullopt;
        }

        return manifold;
    }

private:
    // =========================================================================
    // GJK Helper Functions
    // =========================================================================

    /// Get Minkowski difference support point
    [[nodiscard]] static SupportPoint get_support(const TransformedShape& shape_a,
                                                   const TransformedShape& shape_b,
                                                   const void_math::Vec3& direction) {
        SupportPoint sp;
        sp.support_a = shape_a.support(direction);
        sp.support_b = shape_b.support(-direction);
        sp.point = sp.support_a - sp.support_b;
        return sp;
    }

    /// Process simplex and update search direction
    /// Returns true if simplex contains origin
    [[nodiscard]] static bool do_simplex(Simplex& simplex, void_math::Vec3& direction) {
        switch (simplex.size()) {
            case 2: return do_simplex_line(simplex, direction);
            case 3: return do_simplex_triangle(simplex, direction);
            case 4: return do_simplex_tetrahedron(simplex, direction);
            default: return false;
        }
    }

    /// Line simplex
    [[nodiscard]] static bool do_simplex_line(Simplex& simplex, void_math::Vec3& direction) {
        const void_math::Vec3& a = simplex[0].point;
        const void_math::Vec3& b = simplex[1].point;

        void_math::Vec3 ab = b - a;
        void_math::Vec3 ao = -a;

        if (void_math::dot(ab, ao) > 0) {
            // Origin is between a and b
            direction = void_math::cross(void_math::cross(ab, ao), ab);
        } else {
            // Origin is beyond a
            simplex.set_line(simplex[0], simplex[0]);
            simplex.clear();
            simplex.push_front(simplex[0]);
            direction = ao;
        }

        return false;
    }

    /// Triangle simplex
    [[nodiscard]] static bool do_simplex_triangle(Simplex& simplex, void_math::Vec3& direction) {
        const void_math::Vec3& a = simplex[0].point;
        const void_math::Vec3& b = simplex[1].point;
        const void_math::Vec3& c = simplex[2].point;

        void_math::Vec3 ab = b - a;
        void_math::Vec3 ac = c - a;
        void_math::Vec3 ao = -a;

        void_math::Vec3 abc = void_math::cross(ab, ac);

        // Check if origin is above or below triangle
        void_math::Vec3 abc_x_ac = void_math::cross(abc, ac);
        if (void_math::dot(abc_x_ac, ao) > 0) {
            if (void_math::dot(ac, ao) > 0) {
                simplex.set_line(simplex[0], simplex[2]);
                direction = void_math::cross(void_math::cross(ac, ao), ac);
            } else {
                return do_simplex_line(simplex, direction);
            }
        } else {
            void_math::Vec3 ab_x_abc = void_math::cross(ab, abc);
            if (void_math::dot(ab_x_abc, ao) > 0) {
                return do_simplex_line(simplex, direction);
            } else {
                if (void_math::dot(abc, ao) > 0) {
                    direction = abc;
                } else {
                    simplex.set_triangle(simplex[0], simplex[2], simplex[1]);
                    direction = -abc;
                }
            }
        }

        return false;
    }

    /// Tetrahedron simplex
    [[nodiscard]] static bool do_simplex_tetrahedron(Simplex& simplex, void_math::Vec3& direction) {
        const void_math::Vec3& a = simplex[0].point;
        const void_math::Vec3& b = simplex[1].point;
        const void_math::Vec3& c = simplex[2].point;
        const void_math::Vec3& d = simplex[3].point;

        void_math::Vec3 ab = b - a;
        void_math::Vec3 ac = c - a;
        void_math::Vec3 ad = d - a;
        void_math::Vec3 ao = -a;

        void_math::Vec3 abc = void_math::cross(ab, ac);
        void_math::Vec3 acd = void_math::cross(ac, ad);
        void_math::Vec3 adb = void_math::cross(ad, ab);

        // Check each face
        if (void_math::dot(abc, ao) > 0) {
            simplex.set_triangle(simplex[0], simplex[1], simplex[2]);
            return do_simplex_triangle(simplex, direction);
        }
        if (void_math::dot(acd, ao) > 0) {
            simplex.set_triangle(simplex[0], simplex[2], simplex[3]);
            return do_simplex_triangle(simplex, direction);
        }
        if (void_math::dot(adb, ao) > 0) {
            simplex.set_triangle(simplex[0], simplex[3], simplex[1]);
            return do_simplex_triangle(simplex, direction);
        }

        // Origin is inside tetrahedron
        return true;
    }

    // =========================================================================
    // EPA Helper Functions
    // =========================================================================

    /// Create EPA face and compute normal/distance
    [[nodiscard]] static EpaFace make_face(const std::vector<SupportPoint>& vertices,
                                            int i, int j, int k) {
        EpaFace face;
        face.indices = {i, j, k};

        void_math::Vec3 a = vertices[i].point;
        void_math::Vec3 b = vertices[j].point;
        void_math::Vec3 c = vertices[k].point;

        void_math::Vec3 ab = b - a;
        void_math::Vec3 ac = c - a;

        face.normal = void_math::normalize(void_math::cross(ab, ac));
        face.distance = void_math::dot(face.normal, a);

        return face;
    }

    /// Add edge to horizon, removing if already present (silhouette)
    static void add_edge(std::vector<std::pair<int, int>>& horizon, int a, int b) {
        // Check if reverse edge exists
        for (auto it = horizon.begin(); it != horizon.end(); ++it) {
            if (it->first == b && it->second == a) {
                horizon.erase(it);
                return;
            }
        }
        horizon.emplace_back(a, b);
    }

    /// Compute barycentric coordinates
    [[nodiscard]] static std::tuple<float, float, float> barycentric(
        const void_math::Vec3& p,
        const void_math::Vec3& a,
        const void_math::Vec3& b,
        const void_math::Vec3& c) {

        void_math::Vec3 v0 = b - a;
        void_math::Vec3 v1 = c - a;
        void_math::Vec3 v2 = p - a;

        float d00 = void_math::dot(v0, v0);
        float d01 = void_math::dot(v0, v1);
        float d11 = void_math::dot(v1, v1);
        float d20 = void_math::dot(v2, v0);
        float d21 = void_math::dot(v2, v1);

        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < k_collision_epsilon) {
            return {1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f};
        }

        float v = (d11 * d20 - d01 * d21) / denom;
        float w = (d00 * d21 - d01 * d20) / denom;
        float u = 1.0f - v - w;

        return {u, v, w};
    }
};

// =============================================================================
// Collision Pair
// =============================================================================

/// Represents a potential collision pair
struct CollisionPair {
    BodyId body_a;
    BodyId body_b;
    ShapeId shape_a;
    ShapeId shape_b;

    [[nodiscard]] bool operator==(const CollisionPair& other) const {
        return (body_a == other.body_a && body_b == other.body_b &&
                shape_a == other.shape_a && shape_b == other.shape_b) ||
               (body_a == other.body_b && body_b == other.body_a &&
                shape_a == other.shape_b && shape_b == other.shape_a);
    }
};

} // namespace void_physics

// Hash for CollisionPair
template<>
struct std::hash<void_physics::CollisionPair> {
    std::size_t operator()(const void_physics::CollisionPair& pair) const noexcept {
        // Order-independent hash
        std::size_t h1 = std::hash<std::uint64_t>{}(pair.body_a.value);
        std::size_t h2 = std::hash<std::uint64_t>{}(pair.body_b.value);
        if (h1 > h2) std::swap(h1, h2);
        return h1 ^ (h2 << 1);
    }
};
