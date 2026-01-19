//! 3D Ray for intersection testing
//!
//! Rays are used for raycasting, picking, and collision detection.

use crate::vector::Vec3;
use crate::matrix::Mat4;

/// 3D ray for intersection testing
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Ray {
    /// Ray origin point
    pub origin: Vec3,
    /// Ray direction (should be normalized)
    pub direction: Vec3,
}

impl Ray {
    /// Create a new ray with normalized direction
    #[inline]
    pub fn new(origin: Vec3, direction: Vec3) -> Self {
        Self {
            origin,
            direction: direction.normalize(),
        }
    }

    /// Create a ray from two points
    #[inline]
    pub fn from_points(start: Vec3, end: Vec3) -> Self {
        Self::new(start, end - start)
    }

    /// Create a ray along the positive X axis from origin
    pub const X_AXIS: Self = Self {
        origin: Vec3::ZERO,
        direction: Vec3::X,
    };

    /// Create a ray along the positive Y axis from origin
    pub const Y_AXIS: Self = Self {
        origin: Vec3::ZERO,
        direction: Vec3::Y,
    };

    /// Create a ray along the positive Z axis from origin
    pub const Z_AXIS: Self = Self {
        origin: Vec3::ZERO,
        direction: Vec3::Z,
    };

    /// Get a point at distance t along the ray
    #[inline]
    pub fn at(&self, t: f32) -> Vec3 {
        self.origin + self.direction * t
    }

    /// Get the closest point on the ray to a given point
    pub fn closest_point(&self, point: Vec3) -> Vec3 {
        let t = (point - self.origin).dot(self.direction);
        if t <= 0.0 {
            self.origin
        } else {
            self.at(t)
        }
    }

    /// Get the distance from a point to the ray
    pub fn distance_to_point(&self, point: Vec3) -> f32 {
        let closest = self.closest_point(point);
        (point - closest).length()
    }

    /// Get the squared distance from a point to the ray
    pub fn distance_squared_to_point(&self, point: Vec3) -> f32 {
        let closest = self.closest_point(point);
        (point - closest).length_squared()
    }

    /// Transform the ray by a matrix
    ///
    /// The origin is transformed as a point, the direction as a vector.
    pub fn transform(&self, matrix: &Mat4) -> Self {
        let origin = matrix.transform_point(self.origin);
        let direction = matrix.transform_vector(self.direction);
        Self::new(origin, direction)
    }

    /// Get the inverse direction (1.0 / direction component)
    ///
    /// Useful for optimized AABB intersection.
    #[inline]
    pub fn inverse_direction(&self) -> Vec3 {
        Vec3::new(
            1.0 / self.direction.x,
            1.0 / self.direction.y,
            1.0 / self.direction.z,
        )
    }

    /// Check if the ray direction is valid (non-zero length)
    #[inline]
    pub fn is_valid(&self) -> bool {
        self.direction.length_squared() > 1e-10
    }
}

impl Default for Ray {
    fn default() -> Self {
        Self::Z_AXIS
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ray_creation() {
        let ray = Ray::new(Vec3::ZERO, Vec3::new(0.0, 0.0, 1.0));
        assert_eq!(ray.origin, Vec3::ZERO);
        assert!((ray.direction.z - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_ray_from_points() {
        let ray = Ray::from_points(Vec3::ZERO, Vec3::new(0.0, 0.0, 5.0));
        assert_eq!(ray.origin, Vec3::ZERO);
        assert!((ray.direction.z - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_ray_at() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);
        let point = ray.at(5.0);
        assert!((point.z - 5.0).abs() < 0.001);
    }

    #[test]
    fn test_ray_closest_point() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);

        // Point on the ray
        let point = Vec3::new(0.0, 0.0, 5.0);
        let closest = ray.closest_point(point);
        assert!((closest - point).length() < 0.001);

        // Point off the ray
        let point = Vec3::new(1.0, 0.0, 5.0);
        let closest = ray.closest_point(point);
        assert!((closest.z - 5.0).abs() < 0.001);
        assert!(closest.x.abs() < 0.001);
    }

    #[test]
    fn test_ray_distance_to_point() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);

        // Point on the ray should have distance 0
        let point = Vec3::new(0.0, 0.0, 5.0);
        assert!(ray.distance_to_point(point) < 0.001);

        // Point 1 unit off the ray
        let point = Vec3::new(1.0, 0.0, 5.0);
        assert!((ray.distance_to_point(point) - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_ray_direction_normalized() {
        let ray = Ray::new(Vec3::ZERO, Vec3::new(0.0, 0.0, 10.0));
        assert!((ray.direction.length() - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_ray_inverse_direction() {
        let ray = Ray::new(Vec3::ZERO, Vec3::new(2.0, 4.0, 8.0).normalize());
        let inv = ray.inverse_direction();

        // inv * dir should be ~1.0 for each component
        assert!((ray.direction.x * inv.x - 1.0).abs() < 0.001);
        assert!((ray.direction.y * inv.y - 1.0).abs() < 0.001);
        assert!((ray.direction.z * inv.z - 1.0).abs() < 0.001);
    }
}
