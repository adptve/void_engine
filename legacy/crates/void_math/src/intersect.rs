//! Intersection tests for raycasting and collision detection
//!
//! Provides ray intersection tests against various primitives:
//! - AABB (Axis-Aligned Bounding Box)
//! - Sphere
//! - Triangle (Möller-Trumbore algorithm)
//! - Plane

use crate::ray::Ray;
use crate::vector::Vec3;
use crate::bounds::{AABB, Sphere};

/// Result of a ray-triangle intersection
#[derive(Clone, Copy, Debug)]
pub struct TriangleHit {
    /// Distance along ray to hit point
    pub distance: f32,
    /// Barycentric coordinates [w, u, v] where w = 1 - u - v
    pub barycentric: [f32; 3],
}

/// Ray-AABB intersection using the slab method
///
/// Returns the distance along the ray to the intersection point,
/// or None if the ray doesn't intersect the AABB.
pub fn ray_aabb(ray: &Ray, aabb: &AABB) -> Option<f32> {
    let inv_dir = ray.inverse_direction();

    let t1 = (aabb.min.x - ray.origin.x) * inv_dir.x;
    let t2 = (aabb.max.x - ray.origin.x) * inv_dir.x;
    let t3 = (aabb.min.y - ray.origin.y) * inv_dir.y;
    let t4 = (aabb.max.y - ray.origin.y) * inv_dir.y;
    let t5 = (aabb.min.z - ray.origin.z) * inv_dir.z;
    let t6 = (aabb.max.z - ray.origin.z) * inv_dir.z;

    let tmin = t1.min(t2).max(t3.min(t4)).max(t5.min(t6));
    let tmax = t1.max(t2).min(t3.max(t4)).min(t5.max(t6));

    // If tmax < 0, ray is intersecting AABB but behind origin
    // If tmin > tmax, ray doesn't intersect
    if tmax < 0.0 || tmin > tmax {
        None
    } else {
        // Return the first positive intersection
        Some(if tmin < 0.0 { tmax } else { tmin })
    }
}

/// Ray-AABB intersection with normal
///
/// Returns (distance, normal) or None if no intersection.
pub fn ray_aabb_with_normal(ray: &Ray, aabb: &AABB) -> Option<(f32, Vec3)> {
    let t = ray_aabb(ray, aabb)?;
    let point = ray.at(t);

    // Determine which face was hit
    let epsilon = 0.0001;
    let normal = if (point.x - aabb.min.x).abs() < epsilon {
        Vec3::NEG_X
    } else if (point.x - aabb.max.x).abs() < epsilon {
        Vec3::X
    } else if (point.y - aabb.min.y).abs() < epsilon {
        Vec3::NEG_Y
    } else if (point.y - aabb.max.y).abs() < epsilon {
        Vec3::Y
    } else if (point.z - aabb.min.z).abs() < epsilon {
        Vec3::NEG_Z
    } else {
        Vec3::Z
    };

    Some((t, normal))
}

/// Ray-Sphere intersection
///
/// Returns the distance along the ray to the nearest intersection point,
/// or None if the ray doesn't intersect the sphere.
pub fn ray_sphere(ray: &Ray, sphere: &Sphere) -> Option<f32> {
    ray_sphere_at(ray, sphere.center, sphere.radius)
}

/// Ray-Sphere intersection with center and radius
pub fn ray_sphere_at(ray: &Ray, center: Vec3, radius: f32) -> Option<f32> {
    let oc = ray.origin - center;
    let a = ray.direction.dot(ray.direction);
    let b = 2.0 * oc.dot(ray.direction);
    let c = oc.dot(oc) - radius * radius;
    let discriminant = b * b - 4.0 * a * c;

    if discriminant < 0.0 {
        None
    } else {
        let sqrt_d = discriminant.sqrt();
        let t1 = (-b - sqrt_d) / (2.0 * a);
        let t2 = (-b + sqrt_d) / (2.0 * a);

        // Return the nearest positive intersection
        if t1 > 0.0 {
            Some(t1)
        } else if t2 > 0.0 {
            Some(t2)
        } else {
            None
        }
    }
}

/// Ray-Sphere intersection with normal
pub fn ray_sphere_with_normal(ray: &Ray, sphere: &Sphere) -> Option<(f32, Vec3)> {
    let t = ray_sphere(ray, sphere)?;
    let point = ray.at(t);
    let normal = (point - sphere.center).normalize();
    Some((t, normal))
}

/// Ray-Triangle intersection using Möller-Trumbore algorithm
///
/// Returns the distance and barycentric coordinates, or None if no intersection.
///
/// # Arguments
/// * `ray` - The ray to test
/// * `v0`, `v1`, `v2` - Triangle vertices
/// * `cull_backface` - If true, only front-facing triangles are hit
pub fn ray_triangle(
    ray: &Ray,
    v0: Vec3,
    v1: Vec3,
    v2: Vec3,
    cull_backface: bool,
) -> Option<TriangleHit> {
    const EPSILON: f32 = 0.0000001;

    let edge1 = v1 - v0;
    let edge2 = v2 - v0;
    let h = ray.direction.cross(edge2);
    let a = edge1.dot(h);

    // Check if ray is parallel to triangle
    if a.abs() < EPSILON {
        return None;
    }

    // Backface culling
    if cull_backface && a < 0.0 {
        return None;
    }

    let f = 1.0 / a;
    let s = ray.origin - v0;
    let u = f * s.dot(h);

    if !(0.0..=1.0).contains(&u) {
        return None;
    }

    let q = s.cross(edge1);
    let v = f * ray.direction.dot(q);

    if v < 0.0 || u + v > 1.0 {
        return None;
    }

    let t = f * edge2.dot(q);

    if t > EPSILON {
        let w = 1.0 - u - v;
        Some(TriangleHit {
            distance: t,
            barycentric: [w, u, v],
        })
    } else {
        None
    }
}

/// Calculate interpolated normal from barycentric coordinates
pub fn interpolate_normal(
    n0: Vec3,
    n1: Vec3,
    n2: Vec3,
    bary: [f32; 3],
) -> Vec3 {
    (n0 * bary[0] + n1 * bary[1] + n2 * bary[2]).normalize()
}

/// Calculate interpolated UV from barycentric coordinates
pub fn interpolate_uv(
    uv0: [f32; 2],
    uv1: [f32; 2],
    uv2: [f32; 2],
    bary: [f32; 3],
) -> [f32; 2] {
    [
        uv0[0] * bary[0] + uv1[0] * bary[1] + uv2[0] * bary[2],
        uv0[1] * bary[0] + uv1[1] * bary[1] + uv2[1] * bary[2],
    ]
}

/// Ray-Plane intersection
///
/// Plane is defined by a point on the plane and a normal.
/// Returns the distance to intersection or None if ray is parallel to plane.
pub fn ray_plane(ray: &Ray, plane_point: Vec3, plane_normal: Vec3) -> Option<f32> {
    let denom = plane_normal.dot(ray.direction);

    // Check if ray is parallel to plane
    if denom.abs() < 0.0001 {
        return None;
    }

    let t = (plane_point - ray.origin).dot(plane_normal) / denom;

    if t >= 0.0 {
        Some(t)
    } else {
        None
    }
}

/// Ray-Disk intersection
///
/// Disk is defined by center, normal, and radius.
pub fn ray_disk(ray: &Ray, center: Vec3, normal: Vec3, radius: f32) -> Option<f32> {
    let t = ray_plane(ray, center, normal)?;
    let point = ray.at(t);

    if (point - center).length_squared() <= radius * radius {
        Some(t)
    } else {
        None
    }
}

/// Ray-Capsule intersection
///
/// Capsule is defined by two end points and a radius.
pub fn ray_capsule(ray: &Ray, a: Vec3, b: Vec3, radius: f32) -> Option<f32> {
    let ab = b - a;
    let ao = ray.origin - a;

    let ab_dot_d = ab.dot(ray.direction);
    let ab_dot_ao = ab.dot(ao);
    let ab_dot_ab = ab.dot(ab);

    let m = ab_dot_d / ab_dot_ab;
    let n = ab_dot_ao / ab_dot_ab;

    let q = ray.direction - ab * m;
    let r = ao - ab * n;

    let a_coef = q.dot(q);
    let b_coef = 2.0 * q.dot(r);
    let c_coef = r.dot(r) - radius * radius;

    let discriminant = b_coef * b_coef - 4.0 * a_coef * c_coef;

    if discriminant < 0.0 {
        // No intersection with infinite cylinder, check spheres at endpoints
        let t1 = ray_sphere_at(ray, a, radius);
        let t2 = ray_sphere_at(ray, b, radius);
        return match (t1, t2) {
            (Some(t1), Some(t2)) => Some(t1.min(t2)),
            (Some(t), None) | (None, Some(t)) => Some(t),
            _ => None,
        };
    }

    let t = (-b_coef - discriminant.sqrt()) / (2.0 * a_coef);

    if t < 0.0 {
        return None;
    }

    // Check if hit point is within the capsule body
    let hit_param = m * t + n;

    if hit_param >= 0.0 && hit_param <= 1.0 {
        return Some(t);
    }

    // Hit is on the caps, check spheres
    if hit_param < 0.0 {
        ray_sphere_at(ray, a, radius)
    } else {
        ray_sphere_at(ray, b, radius)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ray_aabb_hit() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);
        let aabb = AABB::new(
            Vec3::new(-1.0, -1.0, 5.0),
            Vec3::new(1.0, 1.0, 7.0),
        );

        let hit = ray_aabb(&ray, &aabb);
        assert!(hit.is_some());
        assert!((hit.unwrap() - 5.0).abs() < 0.01);
    }

    #[test]
    fn test_ray_aabb_miss() {
        let ray = Ray::new(Vec3::ZERO, Vec3::X);
        let aabb = AABB::new(
            Vec3::new(-1.0, -1.0, 5.0),
            Vec3::new(1.0, 1.0, 7.0),
        );

        assert!(ray_aabb(&ray, &aabb).is_none());
    }

    #[test]
    fn test_ray_aabb_inside() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);
        let aabb = AABB::new(
            Vec3::new(-1.0, -1.0, -1.0),
            Vec3::new(1.0, 1.0, 1.0),
        );

        let hit = ray_aabb(&ray, &aabb);
        assert!(hit.is_some());
        // Should return the exit point
        assert!((hit.unwrap() - 1.0).abs() < 0.01);
    }

    #[test]
    fn test_ray_aabb_behind() {
        let ray = Ray::new(Vec3::new(0.0, 0.0, 10.0), Vec3::Z);
        let aabb = AABB::new(Vec3::ZERO, Vec3::ONE);

        // AABB is behind ray
        assert!(ray_aabb(&ray, &aabb).is_none());
    }

    #[test]
    fn test_ray_sphere_hit() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);
        let sphere = Sphere::new(Vec3::new(0.0, 0.0, 5.0), 1.0);

        let hit = ray_sphere(&ray, &sphere);
        assert!(hit.is_some());
        assert!((hit.unwrap() - 4.0).abs() < 0.01);
    }

    #[test]
    fn test_ray_sphere_miss() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);
        let sphere = Sphere::new(Vec3::new(10.0, 0.0, 5.0), 1.0);

        assert!(ray_sphere(&ray, &sphere).is_none());
    }

    #[test]
    fn test_ray_sphere_inside() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);
        let sphere = Sphere::new(Vec3::ZERO, 5.0);

        let hit = ray_sphere(&ray, &sphere);
        assert!(hit.is_some());
        // Should return exit point
        assert!((hit.unwrap() - 5.0).abs() < 0.01);
    }

    #[test]
    fn test_ray_triangle_hit() {
        // Ray going in +Z, triangle with CCW winding (normal points -Z)
        // so ray hits the front face
        let ray = Ray::new(Vec3::new(0.0, 0.0, -1.0), Vec3::Z);
        let v0 = Vec3::new(-1.0, -1.0, 0.0);
        let v1 = Vec3::new(0.0, 1.0, 0.0);  // Swapped v1 and v2 for -Z normal
        let v2 = Vec3::new(1.0, -1.0, 0.0);

        let hit = ray_triangle(&ray, v0, v1, v2, true);
        assert!(hit.is_some());

        let hit = hit.unwrap();
        assert!((hit.distance - 1.0).abs() < 0.01);

        // Barycentric should sum to 1
        let sum: f32 = hit.barycentric.iter().sum();
        assert!((sum - 1.0).abs() < 0.01);
    }

    #[test]
    fn test_ray_triangle_miss() {
        let ray = Ray::new(Vec3::new(10.0, 0.0, -1.0), Vec3::Z);
        let v0 = Vec3::new(-1.0, -1.0, 0.0);
        let v1 = Vec3::new(0.0, 1.0, 0.0);
        let v2 = Vec3::new(1.0, -1.0, 0.0);

        assert!(ray_triangle(&ray, v0, v1, v2, true).is_none());
    }

    #[test]
    fn test_ray_triangle_backface() {
        // Triangle with normal pointing -Z, ray coming from +Z side (backface)
        let ray = Ray::new(Vec3::new(0.0, 0.0, 1.0), Vec3::NEG_Z);
        let v0 = Vec3::new(-1.0, -1.0, 0.0);
        let v1 = Vec3::new(0.0, 1.0, 0.0);
        let v2 = Vec3::new(1.0, -1.0, 0.0);

        // With backface culling - should NOT hit (ray hitting from +Z but normal points -Z)
        assert!(ray_triangle(&ray, v0, v1, v2, true).is_none());

        // Without backface culling - should hit
        assert!(ray_triangle(&ray, v0, v1, v2, false).is_some());
    }

    #[test]
    fn test_ray_plane() {
        let ray = Ray::new(Vec3::new(0.0, 5.0, 0.0), Vec3::NEG_Y);
        let hit = ray_plane(&ray, Vec3::ZERO, Vec3::Y);

        assert!(hit.is_some());
        assert!((hit.unwrap() - 5.0).abs() < 0.01);
    }

    #[test]
    fn test_ray_plane_parallel() {
        let ray = Ray::new(Vec3::new(0.0, 5.0, 0.0), Vec3::X);
        let hit = ray_plane(&ray, Vec3::ZERO, Vec3::Y);

        assert!(hit.is_none());
    }

    #[test]
    fn test_ray_disk() {
        let ray = Ray::new(Vec3::new(0.0, 5.0, 0.0), Vec3::NEG_Y);

        // Hit within disk
        let hit = ray_disk(&ray, Vec3::ZERO, Vec3::Y, 2.0);
        assert!(hit.is_some());

        // Miss outside disk
        let ray2 = Ray::new(Vec3::new(5.0, 5.0, 0.0), Vec3::NEG_Y);
        let miss = ray_disk(&ray2, Vec3::ZERO, Vec3::Y, 2.0);
        assert!(miss.is_none());
    }

    #[test]
    fn test_interpolate_normal() {
        let n0 = Vec3::Y;
        let n1 = Vec3::Y;
        let n2 = Vec3::Y;
        let bary = [0.33, 0.33, 0.34];

        let normal = interpolate_normal(n0, n1, n2, bary);
        assert!((normal - Vec3::Y).length() < 0.01);
    }

    #[test]
    fn test_interpolate_uv() {
        let uv0 = [0.0, 0.0];
        let uv1 = [1.0, 0.0];
        let uv2 = [0.5, 1.0];
        let bary = [1.0, 0.0, 0.0];

        let uv = interpolate_uv(uv0, uv1, uv2, bary);
        assert!((uv[0] - 0.0).abs() < 0.01);
        assert!((uv[1] - 0.0).abs() < 0.01);
    }

    #[test]
    fn test_ray_capsule() {
        let ray = Ray::new(Vec3::new(5.0, 0.0, 0.0), Vec3::NEG_X);
        let hit = ray_capsule(&ray, Vec3::new(0.0, -1.0, 0.0), Vec3::new(0.0, 1.0, 0.0), 1.0);

        assert!(hit.is_some());
        assert!((hit.unwrap() - 4.0).abs() < 0.01);
    }
}
