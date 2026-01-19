//! Trigger volume shapes

use serde::{Deserialize, Serialize};

/// Trigger volume shapes
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum TriggerVolume {
    /// Axis-aligned box
    Box {
        /// Half-extents (width/2, height/2, depth/2)
        half_extents: [f32; 3],
    },
    /// Sphere
    Sphere {
        /// Radius
        radius: f32,
    },
    /// Capsule (cylinder with hemisphere caps)
    Capsule {
        /// Radius
        radius: f32,
        /// Half-height (cylinder portion only)
        half_height: f32,
        /// Axis (0 = X, 1 = Y, 2 = Z)
        axis: u8,
    },
    /// Cylinder
    Cylinder {
        /// Radius
        radius: f32,
        /// Half-height
        half_height: f32,
        /// Axis (0 = X, 1 = Y, 2 = Z)
        axis: u8,
    },
    /// Cone (for directional triggers like spotlights)
    Cone {
        /// Base radius
        radius: f32,
        /// Height
        height: f32,
        /// Direction axis (0 = X, 1 = Y, 2 = Z), negative for opposite
        axis: i8,
    },
    /// Custom convex hull (list of points)
    ConvexHull {
        /// Hull points
        points: Vec<[f32; 3]>,
    },
}

impl TriggerVolume {
    /// Create a box trigger volume
    pub fn box_shape(width: f32, height: f32, depth: f32) -> Self {
        Self::Box {
            half_extents: [width / 2.0, height / 2.0, depth / 2.0],
        }
    }

    /// Create a cube trigger volume
    pub fn cube(size: f32) -> Self {
        Self::box_shape(size, size, size)
    }

    /// Create a sphere trigger volume
    pub fn sphere(radius: f32) -> Self {
        Self::Sphere { radius }
    }

    /// Create a capsule trigger volume (Y-axis aligned)
    pub fn capsule(radius: f32, height: f32) -> Self {
        Self::Capsule {
            radius,
            half_height: height / 2.0,
            axis: 1, // Y-axis
        }
    }

    /// Create a capsule aligned to a specific axis
    pub fn capsule_axis(radius: f32, height: f32, axis: u8) -> Self {
        Self::Capsule {
            radius,
            half_height: height / 2.0,
            axis,
        }
    }

    /// Create a cylinder trigger volume (Y-axis aligned)
    pub fn cylinder(radius: f32, height: f32) -> Self {
        Self::Cylinder {
            radius,
            half_height: height / 2.0,
            axis: 1, // Y-axis
        }
    }

    /// Create a cone trigger volume (pointing down -Y by default)
    pub fn cone(radius: f32, height: f32) -> Self {
        Self::Cone {
            radius,
            height,
            axis: -1, // -Y direction
        }
    }

    /// Create a convex hull from points
    pub fn convex_hull(points: Vec<[f32; 3]>) -> Self {
        Self::ConvexHull { points }
    }

    /// Check if a point is inside this volume (at origin)
    pub fn contains_point(&self, point: [f32; 3]) -> bool {
        match self {
            Self::Box { half_extents } => {
                point[0].abs() <= half_extents[0]
                    && point[1].abs() <= half_extents[1]
                    && point[2].abs() <= half_extents[2]
            }
            Self::Sphere { radius } => {
                let dist_sq =
                    point[0] * point[0] + point[1] * point[1] + point[2] * point[2];
                dist_sq <= radius * radius
            }
            Self::Capsule {
                radius,
                half_height,
                axis,
            } => {
                let axis = *axis as usize;
                // Project point onto the capsule's axis
                let axis_pos = point[axis].clamp(-*half_height, *half_height);
                let mut closest = [0.0f32; 3];
                closest[axis] = axis_pos;

                let dx = point[0] - closest[0];
                let dy = point[1] - closest[1];
                let dz = point[2] - closest[2];
                let dist_sq = dx * dx + dy * dy + dz * dz;

                dist_sq <= radius * radius
            }
            Self::Cylinder {
                radius,
                half_height,
                axis,
            } => {
                let axis = *axis as usize;
                // Check height
                if point[axis].abs() > *half_height {
                    return false;
                }
                // Check radius (in the plane perpendicular to axis)
                let mut dist_sq = 0.0;
                for i in 0..3 {
                    if i != axis {
                        dist_sq += point[i] * point[i];
                    }
                }
                dist_sq <= radius * radius
            }
            Self::Cone {
                radius,
                height,
                axis,
            } => {
                let axis_idx = axis.unsigned_abs() as usize;
                let axis_dir = if *axis < 0 { -1.0 } else { 1.0 };

                // Height along cone axis (0 = apex, height = base)
                let h = point[axis_idx] * axis_dir;
                if h < 0.0 || h > *height {
                    return false;
                }

                // Radius at this height
                let r_at_h = *radius * (h / *height);

                // Distance from axis in perpendicular plane
                let mut dist_sq = 0.0;
                for i in 0..3 {
                    if i != axis_idx {
                        dist_sq += point[i] * point[i];
                    }
                }
                dist_sq <= r_at_h * r_at_h
            }
            Self::ConvexHull { points } => {
                // Simple point-in-convex-hull test using face normals
                // This is a simplified version; a real implementation would compute faces
                if points.len() < 4 {
                    return false;
                }
                // For now, use bounding sphere as approximation
                let center = Self::hull_center(points);
                let max_dist_sq = points
                    .iter()
                    .map(|p| {
                        let dx = p[0] - center[0];
                        let dy = p[1] - center[1];
                        let dz = p[2] - center[2];
                        dx * dx + dy * dy + dz * dz
                    })
                    .fold(0.0f32, f32::max);

                let dx = point[0] - center[0];
                let dy = point[1] - center[1];
                let dz = point[2] - center[2];
                let dist_sq = dx * dx + dy * dy + dz * dz;

                dist_sq <= max_dist_sq
            }
        }
    }

    /// Check if a point (with transform applied) is inside
    pub fn contains_point_transformed(
        &self,
        point: [f32; 3],
        position: [f32; 3],
        _rotation: [f32; 4], // quaternion (x, y, z, w)
        scale: [f32; 3],
    ) -> bool {
        // Transform point to local space
        let local = [
            (point[0] - position[0]) / scale[0],
            (point[1] - position[1]) / scale[1],
            (point[2] - position[2]) / scale[2],
        ];

        // Note: Full rotation support would require quaternion inverse transform
        // For now, we assume axis-aligned triggers

        self.contains_point(local)
    }

    /// Check if two volumes overlap (simplified AABB test)
    pub fn overlaps_aabb(&self, other: &TriggerVolume) -> bool {
        let aabb1 = self.bounding_box();
        let aabb2 = other.bounding_box();

        for i in 0..3 {
            if aabb1.1[i] < aabb2.0[i] || aabb1.0[i] > aabb2.1[i] {
                return false;
            }
        }
        true
    }

    /// Get axis-aligned bounding box (min, max)
    pub fn bounding_box(&self) -> ([f32; 3], [f32; 3]) {
        match self {
            Self::Box { half_extents } => {
                (
                    [-half_extents[0], -half_extents[1], -half_extents[2]],
                    [half_extents[0], half_extents[1], half_extents[2]],
                )
            }
            Self::Sphere { radius } => ([-*radius, -*radius, -*radius], [*radius, *radius, *radius]),
            Self::Capsule {
                radius,
                half_height,
                axis,
            } => {
                let axis = *axis as usize;
                let mut min = [-*radius, -*radius, -*radius];
                let mut max = [*radius, *radius, *radius];
                min[axis] = -*half_height - *radius;
                max[axis] = *half_height + *radius;
                (min, max)
            }
            Self::Cylinder {
                radius,
                half_height,
                axis,
            } => {
                let axis = *axis as usize;
                let mut min = [-*radius, -*radius, -*radius];
                let mut max = [*radius, *radius, *radius];
                min[axis] = -*half_height;
                max[axis] = *half_height;
                (min, max)
            }
            Self::Cone {
                radius,
                height,
                axis,
            } => {
                let axis_idx = axis.unsigned_abs() as usize;
                let mut min = [-*radius, -*radius, -*radius];
                let mut max = [*radius, *radius, *radius];
                if *axis > 0 {
                    min[axis_idx] = 0.0;
                    max[axis_idx] = *height;
                } else {
                    min[axis_idx] = -*height;
                    max[axis_idx] = 0.0;
                }
                (min, max)
            }
            Self::ConvexHull { points } => {
                if points.is_empty() {
                    return ([0.0, 0.0, 0.0], [0.0, 0.0, 0.0]);
                }
                let mut min = points[0];
                let mut max = points[0];
                for p in points.iter().skip(1) {
                    for i in 0..3 {
                        min[i] = min[i].min(p[i]);
                        max[i] = max[i].max(p[i]);
                    }
                }
                (min, max)
            }
        }
    }

    /// Get approximate center of convex hull
    fn hull_center(points: &[[f32; 3]]) -> [f32; 3] {
        if points.is_empty() {
            return [0.0, 0.0, 0.0];
        }
        let mut center = [0.0f32; 3];
        for p in points {
            center[0] += p[0];
            center[1] += p[1];
            center[2] += p[2];
        }
        let n = points.len() as f32;
        [center[0] / n, center[1] / n, center[2] / n]
    }
}

impl Default for TriggerVolume {
    fn default() -> Self {
        Self::box_shape(1.0, 1.0, 1.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_box_contains() {
        let volume = TriggerVolume::box_shape(2.0, 2.0, 2.0);

        assert!(volume.contains_point([0.0, 0.0, 0.0]));
        assert!(volume.contains_point([0.9, 0.9, 0.9]));
        assert!(!volume.contains_point([1.5, 0.0, 0.0]));
    }

    #[test]
    fn test_sphere_contains() {
        let volume = TriggerVolume::sphere(1.0);

        assert!(volume.contains_point([0.0, 0.0, 0.0]));
        assert!(volume.contains_point([0.5, 0.5, 0.5]));
        assert!(!volume.contains_point([1.0, 1.0, 0.0])); // Outside radius
    }

    #[test]
    fn test_capsule_contains() {
        let volume = TriggerVolume::capsule(0.5, 2.0); // Y-axis

        assert!(volume.contains_point([0.0, 0.0, 0.0]));
        assert!(volume.contains_point([0.0, 1.0, 0.0])); // At top of cylinder
        assert!(volume.contains_point([0.0, 1.4, 0.0])); // In top hemisphere
        assert!(!volume.contains_point([0.0, 2.0, 0.0])); // Above capsule
    }

    #[test]
    fn test_bounding_box() {
        let volume = TriggerVolume::box_shape(2.0, 4.0, 2.0);
        let (min, max) = volume.bounding_box();

        assert_eq!(min, [-1.0, -2.0, -1.0]);
        assert_eq!(max, [1.0, 2.0, 1.0]);
    }
}
