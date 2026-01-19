//! Bounding volumes for spatial queries and culling

use crate::vector::Vec3;
use crate::matrix::Mat4;

/// Axis-Aligned Bounding Box
#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct AABB {
    pub min: Vec3,
    pub max: Vec3,
}

impl AABB {
    /// Create an empty (inverted) AABB
    pub const EMPTY: Self = Self {
        min: Vec3::new(f32::MAX, f32::MAX, f32::MAX),
        max: Vec3::new(f32::MIN, f32::MIN, f32::MIN),
    };

    /// Create from min and max points
    #[inline]
    pub const fn new(min: Vec3, max: Vec3) -> Self {
        Self { min, max }
    }

    /// Create from center and half-extents
    #[inline]
    pub fn from_center_half_extents(center: Vec3, half_extents: Vec3) -> Self {
        Self {
            min: center - half_extents,
            max: center + half_extents,
        }
    }

    /// Create from a set of points
    pub fn from_points(points: &[Vec3]) -> Self {
        let mut aabb = Self::EMPTY;
        for &point in points {
            aabb = aabb.expand_to_include(point);
        }
        aabb
    }

    /// Get the center point
    #[inline]
    pub fn center(&self) -> Vec3 {
        (self.min + self.max) * 0.5
    }

    /// Get the half-extents
    #[inline]
    pub fn half_extents(&self) -> Vec3 {
        (self.max - self.min) * 0.5
    }

    /// Get the size (full extents)
    #[inline]
    pub fn size(&self) -> Vec3 {
        self.max - self.min
    }

    /// Get the volume
    #[inline]
    pub fn volume(&self) -> f32 {
        let size = self.size();
        size.x * size.y * size.z
    }

    /// Get the surface area
    #[inline]
    pub fn surface_area(&self) -> f32 {
        let size = self.size();
        2.0 * (size.x * size.y + size.y * size.z + size.z * size.x)
    }

    /// Check if the AABB is valid (min <= max)
    #[inline]
    pub fn is_valid(&self) -> bool {
        self.min.x <= self.max.x && self.min.y <= self.max.y && self.min.z <= self.max.z
    }

    /// Expand to include a point
    pub fn expand_to_include(self, point: Vec3) -> Self {
        Self {
            min: self.min.min(point),
            max: self.max.max(point),
        }
    }

    /// Expand to include another AABB
    pub fn expand_to_include_aabb(self, other: &AABB) -> Self {
        Self {
            min: self.min.min(other.min),
            max: self.max.max(other.max),
        }
    }

    /// Union of two AABBs (alias for expand_to_include_aabb)
    #[inline]
    pub fn union(&self, other: &AABB) -> Self {
        Self {
            min: self.min.min(other.min),
            max: self.max.max(other.max),
        }
    }

    /// Expand AABB by a uniform amount in all directions
    #[inline]
    pub fn expand(&self, amount: f32) -> Self {
        Self {
            min: self.min - Vec3::splat(amount),
            max: self.max + Vec3::splat(amount),
        }
    }

    /// Create an empty (inverted) AABB - function form
    #[inline]
    pub fn empty() -> Self {
        Self::EMPTY
    }

    /// Check if the AABB is empty (inverted or degenerate)
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.min.x > self.max.x || self.min.y > self.max.y || self.min.z > self.max.z
    }

    /// Check if a point is inside
    #[inline]
    pub fn contains_point(&self, point: Vec3) -> bool {
        point.x >= self.min.x && point.x <= self.max.x &&
        point.y >= self.min.y && point.y <= self.max.y &&
        point.z >= self.min.z && point.z <= self.max.z
    }

    /// Check if another AABB is fully contained
    #[inline]
    pub fn contains_aabb(&self, other: &AABB) -> bool {
        self.contains_point(other.min) && self.contains_point(other.max)
    }

    /// Check if two AABBs intersect
    #[inline]
    pub fn intersects(&self, other: &AABB) -> bool {
        self.min.x <= other.max.x && self.max.x >= other.min.x &&
        self.min.y <= other.max.y && self.max.y >= other.min.y &&
        self.min.z <= other.max.z && self.max.z >= other.min.z
    }

    /// Get the closest point on the AABB to a given point
    pub fn closest_point(&self, point: Vec3) -> Vec3 {
        Vec3::new(
            point.x.clamp(self.min.x, self.max.x),
            point.y.clamp(self.min.y, self.max.y),
            point.z.clamp(self.min.z, self.max.z),
        )
    }

    /// Get the squared distance to a point
    pub fn distance_squared_to_point(&self, point: Vec3) -> f32 {
        let closest = self.closest_point(point);
        (point - closest).length_squared()
    }

    /// Transform the AABB by a matrix (result is still axis-aligned)
    pub fn transform(&self, matrix: &Mat4) -> Self {
        let corners = [
            Vec3::new(self.min.x, self.min.y, self.min.z),
            Vec3::new(self.max.x, self.min.y, self.min.z),
            Vec3::new(self.min.x, self.max.y, self.min.z),
            Vec3::new(self.max.x, self.max.y, self.min.z),
            Vec3::new(self.min.x, self.min.y, self.max.z),
            Vec3::new(self.max.x, self.min.y, self.max.z),
            Vec3::new(self.min.x, self.max.y, self.max.z),
            Vec3::new(self.max.x, self.max.y, self.max.z),
        ];

        let mut result = Self::EMPTY;
        for corner in &corners {
            let transformed = matrix.transform_point(*corner);
            result = result.expand_to_include(transformed);
        }
        result
    }

    /// Get the 8 corners of the AABB
    pub fn corners(&self) -> [Vec3; 8] {
        [
            Vec3::new(self.min.x, self.min.y, self.min.z),
            Vec3::new(self.max.x, self.min.y, self.min.z),
            Vec3::new(self.min.x, self.max.y, self.min.z),
            Vec3::new(self.max.x, self.max.y, self.min.z),
            Vec3::new(self.min.x, self.min.y, self.max.z),
            Vec3::new(self.max.x, self.min.y, self.max.z),
            Vec3::new(self.min.x, self.max.y, self.max.z),
            Vec3::new(self.max.x, self.max.y, self.max.z),
        ]
    }
}

impl Default for AABB {
    fn default() -> Self {
        Self::EMPTY
    }
}

/// Bounding Sphere
#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct Sphere {
    pub center: Vec3,
    pub radius: f32,
}

impl Sphere {
    /// Create a new sphere
    #[inline]
    pub const fn new(center: Vec3, radius: f32) -> Self {
        Self { center, radius }
    }

    /// Create from an AABB (bounding sphere of the AABB)
    pub fn from_aabb(aabb: &AABB) -> Self {
        let center = aabb.center();
        let radius = aabb.half_extents().length();
        Self { center, radius }
    }

    /// Create from a set of points
    pub fn from_points(points: &[Vec3]) -> Self {
        if points.is_empty() {
            return Self::new(Vec3::ZERO, 0.0);
        }

        // Simple algorithm: compute AABB bounding sphere then refine
        let aabb = AABB::from_points(points);
        let mut sphere = Self::from_aabb(&aabb);

        // Ritter's bounding sphere refinement
        for &point in points {
            let dist = (point - sphere.center).length();
            if dist > sphere.radius {
                let new_radius = (sphere.radius + dist) * 0.5;
                let k = (new_radius - sphere.radius) / dist;
                sphere.center = sphere.center + (point - sphere.center) * k;
                sphere.radius = new_radius;
            }
        }

        sphere
    }

    /// Check if a point is inside
    #[inline]
    pub fn contains_point(&self, point: Vec3) -> bool {
        (point - self.center).length_squared() <= self.radius * self.radius
    }

    /// Check if another sphere is fully contained
    #[inline]
    pub fn contains_sphere(&self, other: &Sphere) -> bool {
        let dist = (other.center - self.center).length();
        dist + other.radius <= self.radius
    }

    /// Check if two spheres intersect
    #[inline]
    pub fn intersects_sphere(&self, other: &Sphere) -> bool {
        let combined_radius = self.radius + other.radius;
        (other.center - self.center).length_squared() <= combined_radius * combined_radius
    }

    /// Check if intersects AABB
    pub fn intersects_aabb(&self, aabb: &AABB) -> bool {
        aabb.distance_squared_to_point(self.center) <= self.radius * self.radius
    }

    /// Get the closest point on the sphere to a given point
    pub fn closest_point(&self, point: Vec3) -> Vec3 {
        let dir = (point - self.center).normalize_or_zero();
        self.center + dir * self.radius
    }

    /// Get the bounding AABB
    pub fn to_aabb(&self) -> AABB {
        AABB::from_center_half_extents(self.center, Vec3::splat(self.radius))
    }

    /// Transform the sphere by a matrix
    ///
    /// Note: For non-uniform scaling, the result will be conservative (sphere that contains
    /// the true transformed ellipsoid).
    pub fn transform(&self, matrix: &Mat4) -> Self {
        let center = matrix.transform_point(self.center);
        // Scale radius by maximum scale factor to ensure the sphere contains the transformed shape
        let scale_x = matrix.transform_vector(Vec3::X).length();
        let scale_y = matrix.transform_vector(Vec3::Y).length();
        let scale_z = matrix.transform_vector(Vec3::Z).length();
        let max_scale = scale_x.max(scale_y).max(scale_z);

        Self {
            center,
            radius: self.radius * max_scale,
        }
    }

    /// Get the volume
    #[inline]
    pub fn volume(&self) -> f32 {
        (4.0 / 3.0) * core::f32::consts::PI * self.radius * self.radius * self.radius
    }

    /// Get the surface area
    #[inline]
    pub fn surface_area(&self) -> f32 {
        4.0 * core::f32::consts::PI * self.radius * self.radius
    }
}

impl Default for Sphere {
    fn default() -> Self {
        Self::new(Vec3::ZERO, 0.0)
    }
}

/// Frustum for view-frustum culling
#[derive(Clone, Copy, Debug)]
pub struct Frustum {
    /// Planes: left, right, bottom, top, near, far
    /// Each plane is (normal.x, normal.y, normal.z, distance)
    pub planes: [[f32; 4]; 6],
}

impl Frustum {
    /// Extract frustum planes from a view-projection matrix
    pub fn from_matrix(mvp: &Mat4) -> Self {
        let m = mvp.to_array();

        let planes = [
            // Left
            [m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]],
            // Right
            [m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]],
            // Bottom
            [m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]],
            // Top
            [m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]],
            // Near
            [m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]],
            // Far
            [m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]],
        ];

        // Normalize planes
        let mut frustum = Self { planes };
        for plane in &mut frustum.planes {
            let len = (plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]).sqrt();
            if len > 0.0 {
                plane[0] /= len;
                plane[1] /= len;
                plane[2] /= len;
                plane[3] /= len;
            }
        }

        frustum
    }

    /// Test if a point is inside the frustum
    pub fn contains_point(&self, point: Vec3) -> bool {
        for plane in &self.planes {
            let dist = plane[0] * point.x + plane[1] * point.y + plane[2] * point.z + plane[3];
            if dist < 0.0 {
                return false;
            }
        }
        true
    }

    /// Test if a sphere intersects the frustum
    pub fn intersects_sphere(&self, sphere: &Sphere) -> bool {
        for plane in &self.planes {
            let dist = plane[0] * sphere.center.x + plane[1] * sphere.center.y
                + plane[2] * sphere.center.z + plane[3];
            if dist < -sphere.radius {
                return false;
            }
        }
        true
    }

    /// Test if an AABB intersects the frustum
    pub fn intersects_aabb(&self, aabb: &AABB) -> bool {
        for plane in &self.planes {
            let px = if plane[0] >= 0.0 { aabb.max.x } else { aabb.min.x };
            let py = if plane[1] >= 0.0 { aabb.max.y } else { aabb.min.y };
            let pz = if plane[2] >= 0.0 { aabb.max.z } else { aabb.min.z };

            let dist = plane[0] * px + plane[1] * py + plane[2] * pz + plane[3];
            if dist < 0.0 {
                return false;
            }
        }
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_aabb_contains_point() {
        let aabb = AABB::new(Vec3::ZERO, Vec3::ONE);
        assert!(aabb.contains_point(Vec3::new(0.5, 0.5, 0.5)));
        assert!(!aabb.contains_point(Vec3::new(1.5, 0.5, 0.5)));
    }

    #[test]
    fn test_aabb_intersects() {
        let a = AABB::new(Vec3::ZERO, Vec3::ONE);
        let b = AABB::new(Vec3::new(0.5, 0.5, 0.5), Vec3::new(1.5, 1.5, 1.5));
        let c = AABB::new(Vec3::new(2.0, 2.0, 2.0), Vec3::new(3.0, 3.0, 3.0));

        assert!(a.intersects(&b));
        assert!(!a.intersects(&c));
    }

    #[test]
    fn test_sphere_contains_point() {
        let sphere = Sphere::new(Vec3::ZERO, 1.0);
        assert!(sphere.contains_point(Vec3::new(0.5, 0.0, 0.0)));
        assert!(!sphere.contains_point(Vec3::new(1.5, 0.0, 0.0)));
    }
}
