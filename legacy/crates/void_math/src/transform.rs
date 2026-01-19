//! Transform component for 3D spatial data

use crate::vector::Vec3;
use crate::quaternion::Quat;
use crate::matrix::Mat4;

/// Complete 3D transform with position, rotation, and scale
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Transform {
    pub position: Vec3,
    pub rotation: Quat,
    pub scale: Vec3,
}

impl Transform {
    /// Identity transform
    pub const IDENTITY: Self = Self {
        position: Vec3::ZERO,
        rotation: Quat::IDENTITY,
        scale: Vec3::ONE,
    };

    /// Create a new transform
    #[inline]
    pub const fn new(position: Vec3, rotation: Quat, scale: Vec3) -> Self {
        Self { position, rotation, scale }
    }

    /// Create from position only
    #[inline]
    pub fn from_position(position: Vec3) -> Self {
        Self {
            position,
            rotation: Quat::IDENTITY,
            scale: Vec3::ONE,
        }
    }

    /// Create from position and rotation
    #[inline]
    pub fn from_position_rotation(position: Vec3, rotation: Quat) -> Self {
        Self {
            position,
            rotation,
            scale: Vec3::ONE,
        }
    }

    /// Create from position and scale
    #[inline]
    pub fn from_position_scale(position: Vec3, scale: Vec3) -> Self {
        Self {
            position,
            rotation: Quat::IDENTITY,
            scale,
        }
    }

    /// Set position (builder pattern)
    #[inline]
    pub fn with_position(mut self, position: Vec3) -> Self {
        self.position = position;
        self
    }

    /// Set rotation (builder pattern)
    #[inline]
    pub fn with_rotation(mut self, rotation: Quat) -> Self {
        self.rotation = rotation;
        self
    }

    /// Set scale (builder pattern)
    #[inline]
    pub fn with_scale(mut self, scale: Vec3) -> Self {
        self.scale = scale;
        self
    }

    /// Convert to a 4x4 transformation matrix
    pub fn to_matrix(&self) -> Mat4 {
        let rot = self.rotation.to_mat4();
        let scale = Mat4::from_scale(self.scale);
        let translation = Mat4::from_translation(self.position);
        translation * rot * scale
    }

    /// Transform a point
    #[inline]
    pub fn transform_point(&self, point: Vec3) -> Vec3 {
        self.position + self.rotation * (point * self.scale.x) // Assumes uniform scale for simplicity
    }

    /// Transform a direction (ignores position)
    #[inline]
    pub fn transform_direction(&self, direction: Vec3) -> Vec3 {
        self.rotation * direction
    }

    /// Get the forward direction (-Z in local space)
    #[inline]
    pub fn forward(&self) -> Vec3 {
        self.rotation * Vec3::NEG_Z
    }

    /// Get the right direction (+X in local space)
    #[inline]
    pub fn right(&self) -> Vec3 {
        self.rotation * Vec3::X
    }

    /// Get the up direction (+Y in local space)
    #[inline]
    pub fn up(&self) -> Vec3 {
        self.rotation * Vec3::Y
    }

    /// Compute the inverse transform
    pub fn inverse(&self) -> Self {
        let inv_rotation = self.rotation.inverse();
        let inv_scale = Vec3::new(1.0 / self.scale.x, 1.0 / self.scale.y, 1.0 / self.scale.z);
        let inv_position = inv_rotation * (-self.position * inv_scale.x);

        Self {
            position: inv_position,
            rotation: inv_rotation,
            scale: inv_scale,
        }
    }

    /// Combine two transforms (self applied first, then other)
    pub fn combine(&self, other: &Transform) -> Self {
        Self {
            position: self.position + self.rotation * (other.position * self.scale.x),
            rotation: self.rotation * other.rotation,
            scale: Vec3::new(
                self.scale.x * other.scale.x,
                self.scale.y * other.scale.y,
                self.scale.z * other.scale.z,
            ),
        }
    }

    /// Interpolate between two transforms
    pub fn lerp(&self, other: &Transform, t: f32) -> Self {
        Self {
            position: self.position.lerp(other.position, t),
            rotation: self.rotation.slerp(other.rotation, t),
            scale: self.scale.lerp(other.scale, t),
        }
    }

    /// Look at a target point
    pub fn look_at(&mut self, target: Vec3, up: Vec3) {
        let forward = (target - self.position).normalize();
        let right = forward.cross(up).normalize();
        let up = right.cross(forward);

        // Convert orthonormal basis to quaternion
        let trace = right.x + up.y + (-forward.z);

        if trace > 0.0 {
            let s = 0.5 / (trace + 1.0).sqrt();
            self.rotation = Quat::new(
                (up.z - (-forward.y)) * s,
                ((-forward.x) - right.z) * s,
                (right.y - up.x) * s,
                0.25 / s,
            );
        } else if right.x > up.y && right.x > (-forward.z) {
            let s = 2.0 * (1.0 + right.x - up.y - (-forward.z)).sqrt();
            self.rotation = Quat::new(
                0.25 * s,
                (right.y + up.x) / s,
                ((-forward.x) + right.z) / s,
                (up.z - (-forward.y)) / s,
            );
        } else if up.y > (-forward.z) {
            let s = 2.0 * (1.0 + up.y - right.x - (-forward.z)).sqrt();
            self.rotation = Quat::new(
                (right.y + up.x) / s,
                0.25 * s,
                (up.z + (-forward.y)) / s,
                ((-forward.x) - right.z) / s,
            );
        } else {
            let s = 2.0 * (1.0 + (-forward.z) - right.x - up.y).sqrt();
            self.rotation = Quat::new(
                ((-forward.x) + right.z) / s,
                (up.z + (-forward.y)) / s,
                0.25 * s,
                (right.y - up.x) / s,
            );
        }
    }

    /// Rotate around an axis
    pub fn rotate_around_axis(&mut self, axis: Vec3, angle: f32) {
        let rotation = Quat::from_axis_angle(axis, angle);
        self.rotation = rotation * self.rotation;
    }

    /// Translate in local space
    pub fn translate_local(&mut self, offset: Vec3) {
        self.position += self.rotation * offset;
    }

    /// Translate in world space
    pub fn translate_world(&mut self, offset: Vec3) {
        self.position += offset;
    }
}

impl Default for Transform {
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl core::ops::Mul for Transform {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self {
        self.combine(&rhs)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_transform_identity() {
        let t = Transform::IDENTITY;
        let point = Vec3::new(1.0, 2.0, 3.0);
        let result = t.transform_point(point);
        assert!((result - point).length() < 1e-6);
    }

    #[test]
    fn test_transform_translation() {
        let t = Transform::from_position(Vec3::new(1.0, 2.0, 3.0));
        let point = Vec3::ZERO;
        let result = t.transform_point(point);
        assert!((result - Vec3::new(1.0, 2.0, 3.0)).length() < 1e-6);
    }

    #[test]
    fn test_transform_combine() {
        let t1 = Transform::from_position(Vec3::new(1.0, 0.0, 0.0));
        let t2 = Transform::from_position(Vec3::new(0.0, 1.0, 0.0));
        let combined = t1.combine(&t2);
        assert!((combined.position - Vec3::new(1.0, 1.0, 0.0)).length() < 1e-6);
    }
}
