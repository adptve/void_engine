//! Frustum culling types
//!
//! Provides structured frustum and plane types for view-frustum culling.

use crate::vector::Vec3;
use crate::matrix::Mat4;
use crate::bounds::{AABB, Sphere};

/// Plane in 3D space (ax + by + cz + d = 0)
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Plane {
    /// Plane normal (unit vector)
    pub normal: Vec3,
    /// Distance from origin along normal
    pub distance: f32,
}

impl Plane {
    /// Create a new plane from normal and distance
    ///
    /// The normal will be normalized automatically.
    #[inline]
    pub fn new(normal: Vec3, distance: f32) -> Self {
        let len = normal.length();
        if len > 1e-10 {
            Self {
                normal: normal / len,
                distance: distance / len,
            }
        } else {
            Self {
                normal: Vec3::Y,
                distance: 0.0,
            }
        }
    }

    /// Create a plane from a point on the plane and its normal
    pub fn from_point_normal(point: Vec3, normal: Vec3) -> Self {
        let normal = normal.normalize();
        Self {
            normal,
            distance: -normal.dot(point),
        }
    }

    /// Create a plane from three points (counter-clockwise winding)
    pub fn from_points(p0: Vec3, p1: Vec3, p2: Vec3) -> Self {
        let v1 = p1 - p0;
        let v2 = p2 - p0;
        let normal = v1.cross(v2).normalize();
        Self {
            normal,
            distance: -normal.dot(p0),
        }
    }

    /// Get the signed distance from a point to the plane
    ///
    /// Positive = in front (same side as normal)
    /// Negative = behind (opposite side of normal)
    /// Zero = on the plane
    #[inline]
    pub fn distance_to_point(&self, point: Vec3) -> f32 {
        self.normal.dot(point) + self.distance
    }

    /// Check if a point is in front of the plane
    #[inline]
    pub fn is_in_front(&self, point: Vec3) -> bool {
        self.distance_to_point(point) > 0.0
    }

    /// Check if a point is behind the plane
    #[inline]
    pub fn is_behind(&self, point: Vec3) -> bool {
        self.distance_to_point(point) < 0.0
    }

    /// Get the closest point on the plane to a given point
    pub fn closest_point(&self, point: Vec3) -> Vec3 {
        point - self.normal * self.distance_to_point(point)
    }

    /// Project a point onto the plane
    pub fn project_point(&self, point: Vec3) -> Vec3 {
        self.closest_point(point)
    }
}

impl Default for Plane {
    fn default() -> Self {
        Self {
            normal: Vec3::Y,
            distance: 0.0,
        }
    }
}

/// Result of frustum containment test
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum FrustumTestResult {
    /// Object is completely inside the frustum
    Inside,
    /// Object is completely outside the frustum
    Outside,
    /// Object intersects the frustum boundary
    Intersecting,
}

impl FrustumTestResult {
    /// Check if the object is at least partially visible
    #[inline]
    pub fn is_visible(&self) -> bool {
        *self != FrustumTestResult::Outside
    }

    /// Check if the object is completely inside
    #[inline]
    pub fn is_inside(&self) -> bool {
        *self == FrustumTestResult::Inside
    }
}

/// View frustum for culling using structured Plane types
///
/// The six planes are: left, right, bottom, top, near, far
/// All planes have normals pointing inward (toward the visible region).
#[derive(Clone, Debug)]
pub struct FrustumPlanes {
    /// Frustum planes (left, right, bottom, top, near, far)
    pub planes: [Plane; 6],
}

impl FrustumPlanes {
    /// Plane indices
    pub const LEFT: usize = 0;
    pub const RIGHT: usize = 1;
    pub const BOTTOM: usize = 2;
    pub const TOP: usize = 3;
    pub const NEAR: usize = 4;
    pub const FAR: usize = 5;

    /// Extract frustum planes from a view-projection matrix
    ///
    /// Uses the Gribb/Hartmann method for extracting planes from the
    /// combined view-projection matrix.
    pub fn from_view_projection(vp: &Mat4) -> Self {
        let m = vp.to_array();

        // Extract planes from the matrix rows
        // Left plane:   row3 + row0
        let left = Plane::new(
            Vec3::new(m[3] + m[0], m[7] + m[4], m[11] + m[8]),
            m[15] + m[12],
        );

        // Right plane:  row3 - row0
        let right = Plane::new(
            Vec3::new(m[3] - m[0], m[7] - m[4], m[11] - m[8]),
            m[15] - m[12],
        );

        // Bottom plane: row3 + row1
        let bottom = Plane::new(
            Vec3::new(m[3] + m[1], m[7] + m[5], m[11] + m[9]),
            m[15] + m[13],
        );

        // Top plane:    row3 - row1
        let top = Plane::new(
            Vec3::new(m[3] - m[1], m[7] - m[5], m[11] - m[9]),
            m[15] - m[13],
        );

        // Near plane:   row3 + row2
        let near = Plane::new(
            Vec3::new(m[3] + m[2], m[7] + m[6], m[11] + m[10]),
            m[15] + m[14],
        );

        // Far plane:    row3 - row2
        let far = Plane::new(
            Vec3::new(m[3] - m[2], m[7] - m[6], m[11] - m[10]),
            m[15] - m[14],
        );

        Self {
            planes: [left, right, bottom, top, near, far],
        }
    }

    /// Test if an AABB is inside, outside, or intersecting the frustum
    pub fn contains_aabb(&self, aabb: &AABB) -> FrustumTestResult {
        let mut result = FrustumTestResult::Inside;

        for plane in &self.planes {
            // Get the corner most aligned with plane normal (p-vertex)
            let p = Vec3::new(
                if plane.normal.x >= 0.0 { aabb.max.x } else { aabb.min.x },
                if plane.normal.y >= 0.0 { aabb.max.y } else { aabb.min.y },
                if plane.normal.z >= 0.0 { aabb.max.z } else { aabb.min.z },
            );

            // Get the corner least aligned with plane normal (n-vertex)
            let n = Vec3::new(
                if plane.normal.x >= 0.0 { aabb.min.x } else { aabb.max.x },
                if plane.normal.y >= 0.0 { aabb.min.y } else { aabb.max.y },
                if plane.normal.z >= 0.0 { aabb.min.z } else { aabb.max.z },
            );

            // If p-vertex is outside, entire AABB is outside
            if plane.distance_to_point(p) < 0.0 {
                return FrustumTestResult::Outside;
            }

            // If n-vertex is outside, AABB intersects
            if plane.distance_to_point(n) < 0.0 {
                result = FrustumTestResult::Intersecting;
            }
        }

        result
    }

    /// Test if a sphere is inside, outside, or intersecting the frustum
    pub fn contains_sphere(&self, sphere: &Sphere) -> FrustumTestResult {
        let mut result = FrustumTestResult::Inside;

        for plane in &self.planes {
            let dist = plane.distance_to_point(sphere.center);

            // If sphere is completely behind plane, it's outside
            if dist < -sphere.radius {
                return FrustumTestResult::Outside;
            }

            // If sphere center is within radius of plane, it's intersecting
            if dist < sphere.radius {
                result = FrustumTestResult::Intersecting;
            }
        }

        result
    }

    /// Test if a point is inside the frustum
    pub fn contains_point(&self, point: Vec3) -> bool {
        for plane in &self.planes {
            if plane.distance_to_point(point) < 0.0 {
                return false;
            }
        }
        true
    }

    /// Quick visibility test - returns true if AABB might be visible
    ///
    /// This is faster than `contains_aabb` but less precise.
    pub fn is_aabb_visible(&self, aabb: &AABB) -> bool {
        for plane in &self.planes {
            // Get the corner most aligned with plane normal
            let p = Vec3::new(
                if plane.normal.x >= 0.0 { aabb.max.x } else { aabb.min.x },
                if plane.normal.y >= 0.0 { aabb.max.y } else { aabb.min.y },
                if plane.normal.z >= 0.0 { aabb.max.z } else { aabb.min.z },
            );

            if plane.distance_to_point(p) < 0.0 {
                return false;
            }
        }
        true
    }

    /// Quick visibility test - returns true if sphere might be visible
    pub fn is_sphere_visible(&self, sphere: &Sphere) -> bool {
        for plane in &self.planes {
            if plane.distance_to_point(sphere.center) < -sphere.radius {
                return false;
            }
        }
        true
    }
}

impl Default for FrustumPlanes {
    fn default() -> Self {
        Self {
            planes: [Plane::default(); 6],
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_plane_distance_to_point() {
        // XY plane (z = 0) with normal pointing up (+Z)
        let plane = Plane::from_point_normal(Vec3::ZERO, Vec3::Z);

        assert!((plane.distance_to_point(Vec3::new(0.0, 0.0, 5.0)) - 5.0).abs() < 1e-6);
        assert!((plane.distance_to_point(Vec3::new(0.0, 0.0, -3.0)) + 3.0).abs() < 1e-6);
        assert!(plane.distance_to_point(Vec3::new(10.0, 20.0, 0.0)).abs() < 1e-6);
    }

    #[test]
    fn test_plane_from_points() {
        let plane = Plane::from_points(
            Vec3::ZERO,
            Vec3::X,
            Vec3::Y,
        );

        // Normal should point in +Z direction (counter-clockwise winding)
        assert!((plane.normal - Vec3::Z).length() < 1e-6);
        assert!(plane.distance.abs() < 1e-6);
    }

    #[test]
    fn test_frustum_contains_aabb() {
        // Create a simple orthographic-like frustum
        let frustum = FrustumPlanes {
            planes: [
                Plane::from_point_normal(Vec3::new(-10.0, 0.0, 0.0), Vec3::X),  // Left
                Plane::from_point_normal(Vec3::new(10.0, 0.0, 0.0), Vec3::NEG_X),  // Right
                Plane::from_point_normal(Vec3::new(0.0, -10.0, 0.0), Vec3::Y),  // Bottom
                Plane::from_point_normal(Vec3::new(0.0, 10.0, 0.0), Vec3::NEG_Y),  // Top
                Plane::from_point_normal(Vec3::new(0.0, 0.0, 0.1), Vec3::Z),  // Near
                Plane::from_point_normal(Vec3::new(0.0, 0.0, 100.0), Vec3::NEG_Z),  // Far
            ],
        };

        // AABB inside
        let inside = AABB::new(Vec3::new(-1.0, -1.0, 1.0), Vec3::new(1.0, 1.0, 2.0));
        assert_ne!(frustum.contains_aabb(&inside), FrustumTestResult::Outside);

        // AABB outside (far beyond near plane in -Z)
        let outside = AABB::new(Vec3::new(-1.0, -1.0, -100.0), Vec3::new(1.0, 1.0, -99.0));
        assert_eq!(frustum.contains_aabb(&outside), FrustumTestResult::Outside);
    }

    #[test]
    fn test_frustum_contains_sphere() {
        // Create a simple orthographic-like frustum
        let frustum = FrustumPlanes {
            planes: [
                Plane::from_point_normal(Vec3::new(-10.0, 0.0, 0.0), Vec3::X),  // Left
                Plane::from_point_normal(Vec3::new(10.0, 0.0, 0.0), Vec3::NEG_X),  // Right
                Plane::from_point_normal(Vec3::new(0.0, -10.0, 0.0), Vec3::Y),  // Bottom
                Plane::from_point_normal(Vec3::new(0.0, 10.0, 0.0), Vec3::NEG_Y),  // Top
                Plane::from_point_normal(Vec3::new(0.0, 0.0, 0.1), Vec3::Z),  // Near
                Plane::from_point_normal(Vec3::new(0.0, 0.0, 100.0), Vec3::NEG_Z),  // Far
            ],
        };

        // Sphere inside
        let inside = Sphere::new(Vec3::new(0.0, 0.0, 50.0), 1.0);
        assert_ne!(frustum.contains_sphere(&inside), FrustumTestResult::Outside);

        // Sphere outside
        let outside = Sphere::new(Vec3::new(100.0, 0.0, 50.0), 1.0);
        assert_eq!(frustum.contains_sphere(&outside), FrustumTestResult::Outside);
    }

    #[test]
    fn test_frustum_test_result() {
        assert!(FrustumTestResult::Inside.is_visible());
        assert!(FrustumTestResult::Intersecting.is_visible());
        assert!(!FrustumTestResult::Outside.is_visible());

        assert!(FrustumTestResult::Inside.is_inside());
        assert!(!FrustumTestResult::Intersecting.is_inside());
        assert!(!FrustumTestResult::Outside.is_inside());
    }
}
