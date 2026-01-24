/// @file query.hpp
/// @brief Scene query system for void_physics

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "body.hpp"
#include "shape.hpp"
#include "broadphase.hpp"
#include "collision.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/ray.hpp>
#include <void_engine/math/transform.hpp>

#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>
#include <limits>

namespace void_physics {

// =============================================================================
// Query System
// =============================================================================

/// Physics query system for raycasts, shape casts, and overlaps
class QuerySystem {
public:
    QuerySystem() = default;

    /// Set broadphase reference
    void set_broadphase(BroadPhaseBvh* broadphase) { m_broadphase = broadphase; }

    /// Set body accessor
    void set_body_accessor(std::function<IRigidbody*(BodyId)> accessor) {
        m_get_body = std::move(accessor);
    }

    // =========================================================================
    // Raycast
    // =========================================================================

    /// Cast ray and get first hit
    [[nodiscard]] RaycastHit raycast(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        RaycastHit result;
        result.distance = max_distance;

        auto dir = void_math::normalize(direction);

        if (!m_broadphase || !m_get_body) return result;

        m_broadphase->raycast(origin, dir, max_distance,
            [&](BodyId body_id, ShapeId shape_id, float t) -> bool {
                auto* body = m_get_body(body_id);
                if (!body) return true;

                if (!passes_filter(*body, filter, layer_mask)) return true;

                // Get shape for detailed raycast
                const IShape* shape = body->get_shape_by_id(shape_id);
                if (!shape) shape = body->get_shape(0);
                if (!shape) return true;

                // Transform ray to local space
                auto inv_rot = void_math::conjugate(body->rotation());
                auto local_origin = void_math::rotate(inv_rot, origin - body->position());
                auto local_dir = void_math::rotate(inv_rot, dir);

                // Perform shape-specific raycast
                float hit_t = 0.0f;
                void_math::Vec3 hit_normal;

                if (raycast_shape(*shape, local_origin, local_dir, max_distance, hit_t, hit_normal)) {
                    if (hit_t < result.distance) {
                        result.hit = true;
                        result.body = body_id;
                        result.shape = shape_id;
                        result.distance = hit_t;
                        result.fraction = hit_t / max_distance;
                        result.position = origin + dir * hit_t;
                        result.normal = void_math::normalize(void_math::rotate(body->rotation(), hit_normal));

                        if (has_flag(filter, QueryFilter::AnyHit)) {
                            return false; // Stop search
                        }
                    }
                }

                return true; // Continue search
            });

        return result;
    }

    /// Cast ray and get all hits
    [[nodiscard]] std::vector<RaycastHit> raycast_all(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        std::vector<RaycastHit> results;

        auto dir = void_math::normalize(direction);

        if (!m_broadphase || !m_get_body) return results;

        m_broadphase->raycast(origin, dir, max_distance,
            [&](BodyId body_id, ShapeId shape_id, float /*t*/) -> bool {
                auto* body = m_get_body(body_id);
                if (!body) return true;

                if (!passes_filter(*body, filter, layer_mask)) return true;

                const IShape* shape = body->get_shape_by_id(shape_id);
                if (!shape) shape = body->get_shape(0);
                if (!shape) return true;

                auto inv_rot = void_math::conjugate(body->rotation());
                auto local_origin = void_math::rotate(inv_rot, origin - body->position());
                auto local_dir = void_math::rotate(inv_rot, dir);

                float hit_t = 0.0f;
                void_math::Vec3 hit_normal;

                if (raycast_shape(*shape, local_origin, local_dir, max_distance, hit_t, hit_normal)) {
                    RaycastHit hit;
                    hit.hit = true;
                    hit.body = body_id;
                    hit.shape = shape_id;
                    hit.distance = hit_t;
                    hit.fraction = hit_t / max_distance;
                    hit.position = origin + dir * hit_t;
                    hit.normal = void_math::normalize(void_math::rotate(body->rotation(), hit_normal));
                    results.push_back(hit);
                }

                return true;
            });

        // Sort by distance
        std::sort(results.begin(), results.end(),
            [](const RaycastHit& a, const RaycastHit& b) {
                return a.distance < b.distance;
            });

        return results;
    }

    /// Cast ray with callback
    void raycast_callback(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask,
        std::function<bool(const RaycastHit&)> callback) const
    {
        auto dir = void_math::normalize(direction);

        if (!m_broadphase || !m_get_body) return;

        m_broadphase->raycast(origin, dir, max_distance,
            [&](BodyId body_id, ShapeId shape_id, float /*t*/) -> bool {
                auto* body = m_get_body(body_id);
                if (!body) return true;

                if (!passes_filter(*body, filter, layer_mask)) return true;

                const IShape* shape = body->get_shape_by_id(shape_id);
                if (!shape) shape = body->get_shape(0);
                if (!shape) return true;

                auto inv_rot = void_math::conjugate(body->rotation());
                auto local_origin = void_math::rotate(inv_rot, origin - body->position());
                auto local_dir = void_math::rotate(inv_rot, dir);

                float hit_t = 0.0f;
                void_math::Vec3 hit_normal;

                if (raycast_shape(*shape, local_origin, local_dir, max_distance, hit_t, hit_normal)) {
                    RaycastHit hit;
                    hit.hit = true;
                    hit.body = body_id;
                    hit.shape = shape_id;
                    hit.distance = hit_t;
                    hit.fraction = hit_t / max_distance;
                    hit.position = origin + dir * hit_t;
                    hit.normal = void_math::normalize(void_math::rotate(body->rotation(), hit_normal));

                    return callback(hit);
                }

                return true;
            });
    }

    // =========================================================================
    // Shape Cast
    // =========================================================================

    /// Cast shape and get first hit
    [[nodiscard]] ShapeCastHit shape_cast(
        const IShape& shape,
        const void_math::Transform& start,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        ShapeCastHit result;

        auto dir = void_math::normalize(direction);

        if (!m_broadphase || !m_get_body) return result;

        // Compute swept AABB
        auto start_aabb = shape.local_bounds();
        start_aabb.min = start_aabb.min + start.position;
        start_aabb.max = start_aabb.max + start.position;

        auto end_pos = start.position + dir * max_distance;
        auto end_aabb = shape.local_bounds();
        end_aabb.min = end_aabb.min + end_pos;
        end_aabb.max = end_aabb.max + end_pos;

        void_math::AABB swept_aabb;
        swept_aabb.min = void_math::min(start_aabb.min, end_aabb.min);
        swept_aabb.max = void_math::max(start_aabb.max, end_aabb.max);

        float best_t = max_distance;
        std::vector<std::pair<BodyId, ShapeId>> candidates;
        m_broadphase->query_aabb(swept_aabb, candidates);

        for (const auto& [body_id, shape_id] : candidates) {
            auto* body = m_get_body(body_id);
            if (!body) continue;

            if (!passes_filter(*body, filter, layer_mask)) continue;

            const IShape* target_shape = body->get_shape_by_id(shape_id);
            if (!target_shape) target_shape = body->get_shape(0);
            if (!target_shape) continue;

            // Binary search for time of impact
            TransformedShape cast_shape{shape, start.position, start.rotation};
            TransformedShape target{*target_shape, body->position(), body->rotation()};

            float t = shape_cast_binary_search(cast_shape, dir, max_distance, target);

            if (t < best_t) {
                best_t = t;

                // Get contact info at time t
                cast_shape.position = start.position + dir * t;
                auto manifold = CollisionDetector::collide(cast_shape, target, BodyId{0}, body_id);

                result.hit = true;
                result.body = body_id;
                result.shape = shape_id;
                result.distance = t;
                result.fraction = t / max_distance;
                result.position = start.position + dir * t;

                if (manifold && !manifold->contacts.empty()) {
                    result.normal = manifold->normal;
                    result.contact_point = manifold->contacts[0].point_a;
                }

                if (has_flag(filter, QueryFilter::AnyHit)) {
                    break;
                }
            }
        }

        return result;
    }

    /// Sphere cast (convenience)
    [[nodiscard]] ShapeCastHit sphere_cast(
        float radius,
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        SphereShape sphere(radius);
        void_math::Transform start;
        start.position = origin;
        return shape_cast(sphere, start, direction, max_distance, filter, layer_mask);
    }

    /// Box cast (convenience)
    [[nodiscard]] ShapeCastHit box_cast(
        const void_math::Vec3& half_extents,
        const void_math::Transform& start,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        BoxShape box(half_extents);
        return shape_cast(box, start, direction, max_distance, filter, layer_mask);
    }

    /// Capsule cast (convenience)
    [[nodiscard]] ShapeCastHit capsule_cast(
        float radius,
        float height,
        const void_math::Transform& start,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        CapsuleShape capsule(radius, height);
        return shape_cast(capsule, start, direction, max_distance, filter, layer_mask);
    }

    // =========================================================================
    // Overlap
    // =========================================================================

    /// Test if shape overlaps any bodies
    [[nodiscard]] bool overlap_test(
        const IShape& shape,
        const void_math::Transform& transform,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        if (!m_broadphase || !m_get_body) return false;

        auto aabb = shape.local_bounds();
        aabb.min = aabb.min + transform.position;
        aabb.max = aabb.max + transform.position;

        std::vector<std::pair<BodyId, ShapeId>> candidates;
        m_broadphase->query_aabb(aabb, candidates);

        TransformedShape query_shape{shape, transform.position, transform.rotation};

        for (const auto& [body_id, shape_id] : candidates) {
            auto* body = m_get_body(body_id);
            if (!body) continue;

            if (!passes_filter(*body, filter, layer_mask)) continue;

            const IShape* target_shape = body->get_shape_by_id(shape_id);
            if (!target_shape) target_shape = body->get_shape(0);
            if (!target_shape) continue;

            TransformedShape target{*target_shape, body->position(), body->rotation()};

            auto gjk = CollisionDetector::gjk(query_shape, target);
            if (gjk.intersecting) {
                return true;
            }
        }

        return false;
    }

    /// Get all overlapping bodies
    [[nodiscard]] std::vector<OverlapResult> overlap_all(
        const IShape& shape,
        const void_math::Transform& transform,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        std::vector<OverlapResult> results;

        if (!m_broadphase || !m_get_body) return results;

        auto aabb = shape.local_bounds();
        aabb.min = aabb.min + transform.position;
        aabb.max = aabb.max + transform.position;

        std::vector<std::pair<BodyId, ShapeId>> candidates;
        m_broadphase->query_aabb(aabb, candidates);

        TransformedShape query_shape{shape, transform.position, transform.rotation};

        for (const auto& [body_id, shape_id] : candidates) {
            auto* body = m_get_body(body_id);
            if (!body) continue;

            if (!passes_filter(*body, filter, layer_mask)) continue;

            const IShape* target_shape = body->get_shape_by_id(shape_id);
            if (!target_shape) target_shape = body->get_shape(0);
            if (!target_shape) continue;

            TransformedShape target{*target_shape, body->position(), body->rotation()};

            auto gjk = CollisionDetector::gjk(query_shape, target);
            if (gjk.intersecting) {
                results.push_back({body_id, shape_id});
            }
        }

        return results;
    }

    /// Sphere overlap (convenience)
    [[nodiscard]] std::vector<OverlapResult> overlap_sphere(
        const void_math::Vec3& center,
        float radius,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        SphereShape sphere(radius);
        void_math::Transform transform;
        transform.position = center;
        return overlap_all(sphere, transform, filter, layer_mask);
    }

    /// Box overlap (convenience)
    [[nodiscard]] std::vector<OverlapResult> overlap_box(
        const void_math::Vec3& center,
        const void_math::Vec3& half_extents,
        const void_math::Quat& rotation,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        BoxShape box(half_extents);
        void_math::Transform transform;
        transform.position = center;
        transform.rotation = rotation;
        return overlap_all(box, transform, filter, layer_mask);
    }

    // =========================================================================
    // Point Queries
    // =========================================================================

    /// Get closest body to point
    [[nodiscard]] BodyId closest_body(
        const void_math::Vec3& point,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        BodyId result = BodyId::invalid();
        float best_dist = max_distance;

        if (!m_broadphase || !m_get_body) return result;

        void_math::AABB query_aabb;
        query_aabb.min = point - void_math::Vec3{max_distance, max_distance, max_distance};
        query_aabb.max = point + void_math::Vec3{max_distance, max_distance, max_distance};

        std::vector<std::pair<BodyId, ShapeId>> candidates;
        m_broadphase->query_aabb(query_aabb, candidates);

        for (const auto& [body_id, shape_id] : candidates) {
            auto* body = m_get_body(body_id);
            if (!body) continue;

            if (!passes_filter(*body, filter, layer_mask)) continue;

            auto closest = body->closest_point(point);
            float dist = void_math::length(closest - point);

            if (dist < best_dist) {
                best_dist = dist;
                result = body_id;
            }
        }

        return result;
    }

    /// Get all bodies containing point
    [[nodiscard]] std::vector<BodyId> bodies_at_point(
        const void_math::Vec3& point,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        std::vector<BodyId> results;

        if (!m_broadphase || !m_get_body) return results;

        std::vector<std::pair<BodyId, ShapeId>> candidates;
        m_broadphase->query_point(point, candidates);

        for (const auto& [body_id, shape_id] : candidates) {
            auto* body = m_get_body(body_id);
            if (!body) continue;

            if (!passes_filter(*body, filter, layer_mask)) continue;

            if (body->contains_point(point)) {
                results.push_back(body_id);
            }
        }

        return results;
    }

private:
    /// Check if body passes filter
    [[nodiscard]] bool passes_filter(
        const IRigidbody& body,
        QueryFilter filter,
        CollisionLayer layer_mask) const
    {
        // Check body type
        switch (body.type()) {
            case BodyType::Static:
                if (!has_flag(filter, QueryFilter::Static)) return false;
                break;
            case BodyType::Kinematic:
                if (!has_flag(filter, QueryFilter::Kinematic)) return false;
                break;
            case BodyType::Dynamic:
                if (!has_flag(filter, QueryFilter::Dynamic)) return false;
                break;
        }

        // Check trigger
        if (body.is_trigger() && !has_flag(filter, QueryFilter::Triggers)) return false;

        // Check layer mask
        if ((body.collision_mask().layer & layer_mask) == 0) return false;

        return true;
    }

    /// Raycast against a shape
    [[nodiscard]] bool raycast_shape(
        const IShape& shape,
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        float& out_t,
        void_math::Vec3& out_normal) const
    {
        switch (shape.type()) {
            case ShapeType::Sphere:
                return raycast_sphere(static_cast<const SphereShape&>(shape), origin, direction, max_distance, out_t, out_normal);
            case ShapeType::Box:
                return raycast_box(static_cast<const BoxShape&>(shape), origin, direction, max_distance, out_t, out_normal);
            case ShapeType::Capsule:
                return raycast_capsule(static_cast<const CapsuleShape&>(shape), origin, direction, max_distance, out_t, out_normal);
            case ShapeType::Plane:
                return raycast_plane(static_cast<const PlaneShape&>(shape), origin, direction, max_distance, out_t, out_normal);
            default:
                // For convex shapes, use GJK raycast
                return raycast_convex(shape, origin, direction, max_distance, out_t, out_normal);
        }
    }

    /// Raycast against sphere
    [[nodiscard]] bool raycast_sphere(
        const SphereShape& sphere,
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        float& out_t,
        void_math::Vec3& out_normal) const
    {
        float radius = sphere.radius();
        auto center = sphere.center();

        auto oc = origin - center;
        float a = void_math::dot(direction, direction);
        float b = 2.0f * void_math::dot(oc, direction);
        float c = void_math::dot(oc, oc) - radius * radius;

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f) return false;

        float sqrt_d = std::sqrt(discriminant);
        float t = (-b - sqrt_d) / (2.0f * a);

        if (t < 0.0f) {
            t = (-b + sqrt_d) / (2.0f * a);
        }

        if (t < 0.0f || t > max_distance) return false;

        out_t = t;
        out_normal = void_math::normalize(origin + direction * t - center);
        return true;
    }

    /// Raycast against box (AABB in local space)
    [[nodiscard]] bool raycast_box(
        const BoxShape& box,
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        float& out_t,
        void_math::Vec3& out_normal) const
    {
        auto half = box.half_extents();

        float t_min = 0.0f;
        float t_max = max_distance;
        int hit_axis = -1;
        float hit_sign = 1.0f;

        for (int i = 0; i < 3; ++i) {
            float dir = i == 0 ? direction.x : (i == 1 ? direction.y : direction.z);
            float orig = i == 0 ? origin.x : (i == 1 ? origin.y : origin.z);
            float h = i == 0 ? half.x : (i == 1 ? half.y : half.z);

            if (std::abs(dir) < 0.0001f) {
                if (orig < -h || orig > h) return false;
            } else {
                float inv_d = 1.0f / dir;
                float t1 = (-h - orig) * inv_d;
                float t2 = (h - orig) * inv_d;

                float sign = 1.0f;
                if (t1 > t2) {
                    std::swap(t1, t2);
                    sign = -1.0f;
                }

                if (t1 > t_min) {
                    t_min = t1;
                    hit_axis = i;
                    hit_sign = -sign;
                }
                t_max = std::min(t_max, t2);

                if (t_min > t_max) return false;
            }
        }

        if (t_min > max_distance) return false;

        out_t = t_min;
        out_normal = void_math::Vec3{0, 0, 0};
        if (hit_axis == 0) out_normal.x = hit_sign;
        else if (hit_axis == 1) out_normal.y = hit_sign;
        else if (hit_axis == 2) out_normal.z = hit_sign;

        return true;
    }

    /// Raycast against capsule
    [[nodiscard]] bool raycast_capsule(
        const CapsuleShape& capsule,
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        float& out_t,
        void_math::Vec3& out_normal) const
    {
        float radius = capsule.radius();
        float half_height = capsule.half_height();

        // Capsule axis is Y
        void_math::Vec3 p1{0, -half_height, 0};
        void_math::Vec3 p2{0, half_height, 0};

        // Test cylinder part
        float dx = direction.x;
        float dz = direction.z;
        float ox = origin.x;
        float oz = origin.z;

        float a = dx * dx + dz * dz;
        float b = 2.0f * (ox * dx + oz * dz);
        float c = ox * ox + oz * oz - radius * radius;

        float best_t = max_distance + 1.0f;
        void_math::Vec3 best_normal;

        if (a > 0.0001f) {
            float discriminant = b * b - 4.0f * a * c;
            if (discriminant >= 0.0f) {
                float sqrt_d = std::sqrt(discriminant);
                float t = (-b - sqrt_d) / (2.0f * a);
                if (t >= 0.0f && t <= max_distance) {
                    float y = origin.y + direction.y * t;
                    if (y >= -half_height && y <= half_height) {
                        if (t < best_t) {
                            best_t = t;
                            auto hit = origin + direction * t;
                            best_normal = void_math::normalize(void_math::Vec3{hit.x, 0, hit.z});
                        }
                    }
                }
            }
        }

        // Test hemisphere caps
        SphereShape cap1(radius);
        SphereShape cap2(radius);
        float t1, t2;
        void_math::Vec3 n1, n2;

        auto orig1 = origin - p1;
        auto orig2 = origin - p2;

        if (raycast_sphere(cap1, orig1, direction, max_distance, t1, n1)) {
            if (t1 < best_t && (origin + direction * t1 - p1).y <= 0) {
                best_t = t1;
                best_normal = n1;
            }
        }

        if (raycast_sphere(cap2, orig2, direction, max_distance, t2, n2)) {
            if (t2 < best_t && (origin + direction * t2 - p2).y >= 0) {
                best_t = t2;
                best_normal = n2;
            }
        }

        if (best_t <= max_distance) {
            out_t = best_t;
            out_normal = best_normal;
            return true;
        }

        return false;
    }

    /// Raycast against plane
    [[nodiscard]] bool raycast_plane(
        const PlaneShape& plane,
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        float& out_t,
        void_math::Vec3& out_normal) const
    {
        auto normal = plane.normal();
        float d = plane.distance();

        float denom = void_math::dot(normal, direction);
        if (std::abs(denom) < 0.0001f) return false;

        float t = -(void_math::dot(normal, origin) + d) / denom;
        if (t < 0.0f || t > max_distance) return false;

        out_t = t;
        out_normal = denom < 0.0f ? normal : -normal;
        return true;
    }

    /// Raycast against convex shape using GJK
    [[nodiscard]] bool raycast_convex(
        const IShape& shape,
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        float& out_t,
        void_math::Vec3& out_normal) const
    {
        // GJK-based raycast (simplified)
        // Use a small sphere and shape cast
        const float epsilon = 0.001f;
        float t = 0.0f;

        TransformedShape target{shape, void_math::Vec3{0, 0, 0}, void_math::Quat{}};

        while (t < max_distance) {
            auto point = origin + direction * t;

            // Check if point is inside shape
            // Use support function to find closest point direction
            auto support = shape.support(void_math::normalize(-point));
            float dist = void_math::length(point - support);

            if (dist < epsilon) {
                out_t = t;
                out_normal = void_math::normalize(point - support);
                return true;
            }

            // Advance
            float step = std::max(epsilon, dist * 0.5f);
            t += step;
        }

        return false;
    }

    /// Shape cast binary search
    [[nodiscard]] float shape_cast_binary_search(
        const TransformedShape& shape,
        const void_math::Vec3& direction,
        float max_distance,
        const TransformedShape& target) const
    {
        float t_min = 0.0f;
        float t_max = max_distance;

        // First check if already overlapping
        auto gjk = CollisionDetector::gjk(shape, target);
        if (gjk.intersecting) {
            return 0.0f;
        }

        // Check if will hit at all
        TransformedShape end_shape = shape;
        end_shape.position = shape.position + direction * max_distance;
        gjk = CollisionDetector::gjk(end_shape, target);
        if (!gjk.intersecting) {
            return max_distance + 1.0f; // No hit
        }

        // Binary search for TOI
        const int max_iterations = 20;
        for (int i = 0; i < max_iterations; ++i) {
            float t = (t_min + t_max) * 0.5f;

            TransformedShape moved = shape;
            moved.position = shape.position + direction * t;

            gjk = CollisionDetector::gjk(moved, target);

            if (gjk.intersecting) {
                t_max = t;
            } else {
                t_min = t;
            }

            if (t_max - t_min < 0.001f) {
                break;
            }
        }

        return t_max;
    }

private:
    BroadPhaseBvh* m_broadphase = nullptr;
    std::function<IRigidbody*(BodyId)> m_get_body;
};

} // namespace void_physics
