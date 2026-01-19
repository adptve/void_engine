//! Quaternion for 3D rotations

use crate::vector::Vec3;
use crate::matrix::{Mat3, Mat4};
use crate::vector::Vec4;
use core::ops::{Mul, MulAssign};

/// Quaternion representing a 3D rotation
#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C, align(16))]
pub struct Quat {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

impl Quat {
    /// Identity quaternion (no rotation)
    pub const IDENTITY: Self = Self::new(0.0, 0.0, 0.0, 1.0);

    /// Create a new quaternion
    #[inline]
    pub const fn new(x: f32, y: f32, z: f32, w: f32) -> Self {
        Self { x, y, z, w }
    }

    /// Create from a Vec4
    #[inline]
    pub const fn from_vec4(v: Vec4) -> Self {
        Self::new(v.x, v.y, v.z, v.w)
    }

    /// Create from axis and angle (radians)
    pub fn from_axis_angle(axis: Vec3, angle: f32) -> Self {
        let half = angle * 0.5;
        let (sin, cos) = half.sin_cos();
        let axis = axis.normalize();
        Self::new(axis.x * sin, axis.y * sin, axis.z * sin, cos)
    }

    /// Create from Euler angles (radians, XYZ order)
    pub fn from_euler(x: f32, y: f32, z: f32) -> Self {
        let (sx, cx) = (x * 0.5).sin_cos();
        let (sy, cy) = (y * 0.5).sin_cos();
        let (sz, cz) = (z * 0.5).sin_cos();

        Self::new(
            sx * cy * cz - cx * sy * sz,
            cx * sy * cz + sx * cy * sz,
            cx * cy * sz - sx * sy * cz,
            cx * cy * cz + sx * sy * sz,
        )
    }

    /// Create from Euler angles (radians, YXZ order - common for cameras)
    /// y = yaw, x = pitch, z = roll
    pub fn from_euler_yxz(y: f32, x: f32, z: f32) -> Self {
        let (sx, cx) = (x * 0.5).sin_cos();
        let (sy, cy) = (y * 0.5).sin_cos();
        let (sz, cz) = (z * 0.5).sin_cos();

        Self::new(
            cy * sx * cz + sy * cx * sz,
            sy * cx * cz - cy * sx * sz,
            cy * cx * sz - sy * sx * cz,
            cy * cx * cz + sy * sx * sz,
        )
    }

    /// Create from rotation around X axis
    #[inline]
    pub fn from_rotation_x(angle: f32) -> Self {
        let half = angle * 0.5;
        Self::new(half.sin(), 0.0, 0.0, half.cos())
    }

    /// Create from rotation around Y axis
    #[inline]
    pub fn from_rotation_y(angle: f32) -> Self {
        let half = angle * 0.5;
        Self::new(0.0, half.sin(), 0.0, half.cos())
    }

    /// Create from rotation around Z axis
    #[inline]
    pub fn from_rotation_z(angle: f32) -> Self {
        let half = angle * 0.5;
        Self::new(0.0, 0.0, half.sin(), half.cos())
    }

    /// Create from a rotation matrix (extracts rotation component)
    pub fn from_rotation_matrix(m: &crate::Mat4) -> Self {
        // Extract rotation from the 3x3 part of the matrix
        let trace = m.cols[0].x + m.cols[1].y + m.cols[2].z;

        if trace > 0.0 {
            let s = (trace + 1.0).sqrt() * 2.0;
            let w = 0.25 * s;
            let x = (m.cols[1].z - m.cols[2].y) / s;
            let y = (m.cols[2].x - m.cols[0].z) / s;
            let z = (m.cols[0].y - m.cols[1].x) / s;
            Self::new(x, y, z, w)
        } else if m.cols[0].x > m.cols[1].y && m.cols[0].x > m.cols[2].z {
            let s = (1.0 + m.cols[0].x - m.cols[1].y - m.cols[2].z).sqrt() * 2.0;
            let x = 0.25 * s;
            let y = (m.cols[0].y + m.cols[1].x) / s;
            let z = (m.cols[2].x + m.cols[0].z) / s;
            let w = (m.cols[1].z - m.cols[2].y) / s;
            Self::new(x, y, z, w)
        } else if m.cols[1].y > m.cols[2].z {
            let s = (1.0 + m.cols[1].y - m.cols[0].x - m.cols[2].z).sqrt() * 2.0;
            let x = (m.cols[0].y + m.cols[1].x) / s;
            let y = 0.25 * s;
            let z = (m.cols[1].z + m.cols[2].y) / s;
            let w = (m.cols[2].x - m.cols[0].z) / s;
            Self::new(x, y, z, w)
        } else {
            let s = (1.0 + m.cols[2].z - m.cols[0].x - m.cols[1].y).sqrt() * 2.0;
            let x = (m.cols[2].x + m.cols[0].z) / s;
            let y = (m.cols[1].z + m.cols[2].y) / s;
            let z = 0.25 * s;
            let w = (m.cols[0].y - m.cols[1].x) / s;
            Self::new(x, y, z, w)
        }
    }

    /// Create quaternion that rotates from one direction to another
    pub fn from_rotation_arc(from: Vec3, to: Vec3) -> Self {
        let from = from.normalize();
        let to = to.normalize();

        let dot = from.dot(to);

        if dot > 0.99999 {
            return Self::IDENTITY;
        }

        if dot < -0.99999 {
            // Vectors are opposite, pick arbitrary perpendicular axis
            let axis = Vec3::X.cross(from);
            let axis = if axis.length_squared() < 1e-6 {
                Vec3::Y.cross(from)
            } else {
                axis
            };
            return Self::from_axis_angle(axis.normalize(), core::f32::consts::PI);
        }

        let axis = from.cross(to);
        let s = ((1.0 + dot) * 2.0).sqrt();
        let inv_s = 1.0 / s;

        Self::new(
            axis.x * inv_s,
            axis.y * inv_s,
            axis.z * inv_s,
            s * 0.5,
        )
    }

    /// Get the length squared
    #[inline]
    pub fn length_squared(self) -> f32 {
        self.x * self.x + self.y * self.y + self.z * self.z + self.w * self.w
    }

    /// Get the length
    #[inline]
    pub fn length(self) -> f32 {
        self.length_squared().sqrt()
    }

    /// Normalize the quaternion
    #[inline]
    pub fn normalize(self) -> Self {
        let len = self.length();
        if len > 0.0 {
            Self::new(self.x / len, self.y / len, self.z / len, self.w / len)
        } else {
            Self::IDENTITY
        }
    }

    /// Conjugate (inverse for unit quaternions)
    #[inline]
    pub fn conjugate(self) -> Self {
        Self::new(-self.x, -self.y, -self.z, self.w)
    }

    /// Inverse
    #[inline]
    pub fn inverse(self) -> Self {
        let len_sq = self.length_squared();
        if len_sq > 0.0 {
            let inv = 1.0 / len_sq;
            Self::new(-self.x * inv, -self.y * inv, -self.z * inv, self.w * inv)
        } else {
            Self::IDENTITY
        }
    }

    /// Dot product
    #[inline]
    pub fn dot(self, other: Self) -> f32 {
        self.x * other.x + self.y * other.y + self.z * other.z + self.w * other.w
    }

    /// Spherical linear interpolation
    pub fn slerp(self, other: Self, t: f32) -> Self {
        let mut dot = self.dot(other);
        let mut other = other;

        // Ensure shortest path - use epsilon to avoid floating point issues
        // near 90-degree differences (dot ≈ 0)
        const EPSILON: f32 = 1e-6;
        if dot < -EPSILON {
            other = Self::new(-other.x, -other.y, -other.z, -other.w);
            dot = -dot;
        }

        // Clamp dot to valid range for acos
        dot = dot.clamp(-1.0, 1.0);

        // Use linear interpolation for nearly identical quaternions
        if dot > 0.9995 {
            return Self::new(
                self.x + (other.x - self.x) * t,
                self.y + (other.y - self.y) * t,
                self.z + (other.z - self.z) * t,
                self.w + (other.w - self.w) * t,
            ).normalize();
        }

        let theta = dot.acos();
        let sin_theta = theta.sin();
        let s1 = ((1.0 - t) * theta).sin() / sin_theta;
        let s2 = (t * theta).sin() / sin_theta;

        Self::new(
            self.x * s1 + other.x * s2,
            self.y * s1 + other.y * s2,
            self.z * s1 + other.z * s2,
            self.w * s1 + other.w * s2,
        )
    }

    /// Linear interpolation (faster but less accurate than slerp)
    pub fn lerp(self, other: Self, t: f32) -> Self {
        let mut other = other;
        if self.dot(other) < 0.0 {
            other = Self::new(-other.x, -other.y, -other.z, -other.w);
        }

        Self::new(
            self.x + (other.x - self.x) * t,
            self.y + (other.y - self.y) * t,
            self.z + (other.z - self.z) * t,
            self.w + (other.w - self.w) * t,
        ).normalize()
    }

    /// Rotate a vector
    pub fn rotate(self, v: Vec3) -> Vec3 {
        let qv = Vec3::new(self.x, self.y, self.z);
        let uv = qv.cross(v);
        let uuv = qv.cross(uv);
        v + (uv * self.w + uuv) * 2.0
    }

    /// Convert to axis-angle representation
    pub fn to_axis_angle(self) -> (Vec3, f32) {
        let q = if self.w < 0.0 {
            Self::new(-self.x, -self.y, -self.z, -self.w)
        } else {
            self
        };

        let angle = 2.0 * q.w.acos();
        let s = (1.0 - q.w * q.w).sqrt();

        if s < 1e-6 {
            (Vec3::Y, angle)
        } else {
            (Vec3::new(q.x / s, q.y / s, q.z / s), angle)
        }
    }

    /// Convert to Euler angles (XYZ order)
    pub fn to_euler(self) -> Vec3 {
        let sinr_cosp = 2.0 * (self.w * self.x + self.y * self.z);
        let cosr_cosp = 1.0 - 2.0 * (self.x * self.x + self.y * self.y);
        let x = sinr_cosp.atan2(cosr_cosp);

        let sinp = 2.0 * (self.w * self.y - self.z * self.x);
        let y = if sinp.abs() >= 1.0 {
            (core::f32::consts::PI / 2.0).copysign(sinp)
        } else {
            sinp.asin()
        };

        let siny_cosp = 2.0 * (self.w * self.z + self.x * self.y);
        let cosy_cosp = 1.0 - 2.0 * (self.y * self.y + self.z * self.z);
        let z = siny_cosp.atan2(cosy_cosp);

        Vec3::new(x, y, z)
    }

    /// Convert to 3x3 rotation matrix
    pub fn to_mat3(self) -> Mat3 {
        let x2 = self.x + self.x;
        let y2 = self.y + self.y;
        let z2 = self.z + self.z;
        let xx = self.x * x2;
        let xy = self.x * y2;
        let xz = self.x * z2;
        let yy = self.y * y2;
        let yz = self.y * z2;
        let zz = self.z * z2;
        let wx = self.w * x2;
        let wy = self.w * y2;
        let wz = self.w * z2;

        Mat3::from_cols(
            Vec3::new(1.0 - (yy + zz), xy + wz, xz - wy),
            Vec3::new(xy - wz, 1.0 - (xx + zz), yz + wx),
            Vec3::new(xz + wy, yz - wx, 1.0 - (xx + yy)),
        )
    }

    /// Convert to 4x4 rotation matrix
    pub fn to_mat4(self) -> Mat4 {
        self.to_mat3().to_mat4()
    }

    /// Convert to Vec4
    #[inline]
    pub fn to_vec4(self) -> Vec4 {
        Vec4::new(self.x, self.y, self.z, self.w)
    }
}

impl Default for Quat {
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl Mul for Quat {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self {
        Self::new(
            self.w * rhs.x + self.x * rhs.w + self.y * rhs.z - self.z * rhs.y,
            self.w * rhs.y - self.x * rhs.z + self.y * rhs.w + self.z * rhs.x,
            self.w * rhs.z + self.x * rhs.y - self.y * rhs.x + self.z * rhs.w,
            self.w * rhs.w - self.x * rhs.x - self.y * rhs.y - self.z * rhs.z,
        )
    }
}

impl MulAssign for Quat {
    fn mul_assign(&mut self, rhs: Self) {
        *self = *self * rhs;
    }
}

impl Mul<Vec3> for Quat {
    type Output = Vec3;

    fn mul(self, rhs: Vec3) -> Vec3 {
        self.rotate(rhs)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_quaternion_identity() {
        let q = Quat::IDENTITY;
        let v = Vec3::new(1.0, 2.0, 3.0);
        let result = q * v;
        assert!((result - v).length() < 1e-6);
    }

    #[test]
    fn test_quaternion_rotation_y() {
        let q = Quat::from_rotation_y(core::f32::consts::PI / 2.0);
        let v = Vec3::X;
        let result = q * v;
        assert!((result - Vec3::NEG_Z).length() < 1e-5);
    }

    #[test]
    fn test_quaternion_slerp() {
        let q1 = Quat::IDENTITY;
        let q2 = Quat::from_rotation_y(core::f32::consts::PI);

        let mid = q1.slerp(q2, 0.5);
        let expected = Quat::from_rotation_y(core::f32::consts::PI / 2.0);

        assert!((mid.dot(expected)).abs() > 0.999);
    }
}
