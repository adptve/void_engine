//! Matrix types for transformations

use crate::vector::{Vec3, Vec4};
use core::ops::{Mul, MulAssign};

/// 3x3 matrix (column-major)
#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub struct Mat3 {
    pub cols: [Vec3; 3],
}

impl Mat3 {
    pub const IDENTITY: Self = Self {
        cols: [Vec3::X, Vec3::Y, Vec3::Z],
    };

    pub const ZERO: Self = Self {
        cols: [Vec3::ZERO, Vec3::ZERO, Vec3::ZERO],
    };

    #[inline]
    pub const fn from_cols(c0: Vec3, c1: Vec3, c2: Vec3) -> Self {
        Self { cols: [c0, c1, c2] }
    }

    #[inline]
    pub fn from_scale(scale: Vec3) -> Self {
        Self::from_cols(
            Vec3::new(scale.x, 0.0, 0.0),
            Vec3::new(0.0, scale.y, 0.0),
            Vec3::new(0.0, 0.0, scale.z),
        )
    }

    #[inline]
    pub fn transpose(&self) -> Self {
        Self::from_cols(
            Vec3::new(self.cols[0].x, self.cols[1].x, self.cols[2].x),
            Vec3::new(self.cols[0].y, self.cols[1].y, self.cols[2].y),
            Vec3::new(self.cols[0].z, self.cols[1].z, self.cols[2].z),
        )
    }

    #[inline]
    pub fn determinant(&self) -> f32 {
        self.cols[0].x * (self.cols[1].y * self.cols[2].z - self.cols[2].y * self.cols[1].z)
            - self.cols[1].x * (self.cols[0].y * self.cols[2].z - self.cols[2].y * self.cols[0].z)
            + self.cols[2].x * (self.cols[0].y * self.cols[1].z - self.cols[1].y * self.cols[0].z)
    }

    pub fn to_mat4(&self) -> Mat4 {
        Mat4::from_cols(
            self.cols[0].extend(0.0),
            self.cols[1].extend(0.0),
            self.cols[2].extend(0.0),
            Vec4::W,
        )
    }
}

impl Default for Mat3 {
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl Mul<Vec3> for Mat3 {
    type Output = Vec3;

    #[inline]
    fn mul(self, rhs: Vec3) -> Vec3 {
        self.cols[0] * rhs.x + self.cols[1] * rhs.y + self.cols[2] * rhs.z
    }
}

/// 4x4 matrix (column-major) - the main transformation matrix
#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C, align(16))]
pub struct Mat4 {
    pub cols: [Vec4; 4],
}

impl Mat4 {
    pub const IDENTITY: Self = Self {
        cols: [Vec4::X, Vec4::Y, Vec4::Z, Vec4::W],
    };

    pub const ZERO: Self = Self {
        cols: [Vec4::ZERO, Vec4::ZERO, Vec4::ZERO, Vec4::ZERO],
    };

    #[inline]
    pub const fn from_cols(c0: Vec4, c1: Vec4, c2: Vec4, c3: Vec4) -> Self {
        Self { cols: [c0, c1, c2, c3] }
    }

    #[inline]
    pub fn from_translation(translation: Vec3) -> Self {
        Self::from_cols(
            Vec4::X,
            Vec4::Y,
            Vec4::Z,
            translation.extend(1.0),
        )
    }

    #[inline]
    pub fn from_scale(scale: Vec3) -> Self {
        Self::from_cols(
            Vec4::new(scale.x, 0.0, 0.0, 0.0),
            Vec4::new(0.0, scale.y, 0.0, 0.0),
            Vec4::new(0.0, 0.0, scale.z, 0.0),
            Vec4::W,
        )
    }

    #[inline]
    pub fn from_rotation_x(angle: f32) -> Self {
        let (sin, cos) = angle.sin_cos();
        Self::from_cols(
            Vec4::X,
            Vec4::new(0.0, cos, sin, 0.0),
            Vec4::new(0.0, -sin, cos, 0.0),
            Vec4::W,
        )
    }

    #[inline]
    pub fn from_rotation_y(angle: f32) -> Self {
        let (sin, cos) = angle.sin_cos();
        Self::from_cols(
            Vec4::new(cos, 0.0, -sin, 0.0),
            Vec4::Y,
            Vec4::new(sin, 0.0, cos, 0.0),
            Vec4::W,
        )
    }

    #[inline]
    pub fn from_rotation_z(angle: f32) -> Self {
        let (sin, cos) = angle.sin_cos();
        Self::from_cols(
            Vec4::new(cos, sin, 0.0, 0.0),
            Vec4::new(-sin, cos, 0.0, 0.0),
            Vec4::Z,
            Vec4::W,
        )
    }

    /// Create a rotation matrix from an axis and angle (Rodrigues' rotation formula)
    #[inline]
    pub fn from_axis_angle(axis: Vec3, angle: f32) -> Self {
        let (sin, cos) = angle.sin_cos();
        let axis = axis.normalize();
        let t = 1.0 - cos;

        let x = axis.x;
        let y = axis.y;
        let z = axis.z;

        Self::from_cols(
            Vec4::new(
                t * x * x + cos,
                t * x * y + sin * z,
                t * x * z - sin * y,
                0.0,
            ),
            Vec4::new(
                t * x * y - sin * z,
                t * y * y + cos,
                t * y * z + sin * x,
                0.0,
            ),
            Vec4::new(
                t * x * z + sin * y,
                t * y * z - sin * x,
                t * z * z + cos,
                0.0,
            ),
            Vec4::W,
        )
    }

    /// Create a look-at view matrix
    pub fn look_at(eye: Vec3, target: Vec3, up: Vec3) -> Self {
        let forward = (target - eye).normalize();
        let right = forward.cross(up).normalize();
        let up = right.cross(forward);

        Self::from_cols(
            Vec4::new(right.x, up.x, -forward.x, 0.0),
            Vec4::new(right.y, up.y, -forward.y, 0.0),
            Vec4::new(right.z, up.z, -forward.z, 0.0),
            Vec4::new(-right.dot(eye), -up.dot(eye), forward.dot(eye), 1.0),
        )
    }

    /// Create a perspective projection matrix
    pub fn perspective(fov_y: f32, aspect: f32, near: f32, far: f32) -> Self {
        let f = 1.0 / (fov_y / 2.0).tan();
        let nf = 1.0 / (near - far);

        Self::from_cols(
            Vec4::new(f / aspect, 0.0, 0.0, 0.0),
            Vec4::new(0.0, f, 0.0, 0.0),
            Vec4::new(0.0, 0.0, (far + near) * nf, -1.0),
            Vec4::new(0.0, 0.0, 2.0 * far * near * nf, 0.0),
        )
    }

    /// Create an orthographic projection matrix (OpenGL style, depth [-1, 1])
    pub fn orthographic(left: f32, right: f32, bottom: f32, top: f32, near: f32, far: f32) -> Self {
        let rml = right - left;
        let tmb = top - bottom;
        let fmn = far - near;

        Self::from_cols(
            Vec4::new(2.0 / rml, 0.0, 0.0, 0.0),
            Vec4::new(0.0, 2.0 / tmb, 0.0, 0.0),
            Vec4::new(0.0, 0.0, -2.0 / fmn, 0.0),
            Vec4::new(-(right + left) / rml, -(top + bottom) / tmb, -(far + near) / fmn, 1.0),
        )
    }

    /// Create an orthographic projection matrix for wgpu/Vulkan (depth [0, 1])
    /// Right-handed coordinate system with zero-to-one depth range
    pub fn orthographic_rh_zo(left: f32, right: f32, bottom: f32, top: f32, near: f32, far: f32) -> Self {
        let rml = right - left;
        let tmb = top - bottom;
        let fmn = far - near;

        Self::from_cols(
            Vec4::new(2.0 / rml, 0.0, 0.0, 0.0),
            Vec4::new(0.0, 2.0 / tmb, 0.0, 0.0),
            Vec4::new(0.0, 0.0, -1.0 / fmn, 0.0),
            Vec4::new(-(right + left) / rml, -(top + bottom) / tmb, -near / fmn, 1.0),
        )
    }

    /// Create a rotation matrix from a quaternion
    pub fn from_quat(q: crate::Quat) -> Self {
        let x2 = q.x + q.x;
        let y2 = q.y + q.y;
        let z2 = q.z + q.z;

        let xx = q.x * x2;
        let xy = q.x * y2;
        let xz = q.x * z2;
        let yy = q.y * y2;
        let yz = q.y * z2;
        let zz = q.z * z2;
        let wx = q.w * x2;
        let wy = q.w * y2;
        let wz = q.w * z2;

        Self::from_cols(
            Vec4::new(1.0 - (yy + zz), xy + wz, xz - wy, 0.0),
            Vec4::new(xy - wz, 1.0 - (xx + zz), yz + wx, 0.0),
            Vec4::new(xz + wy, yz - wx, 1.0 - (xx + yy), 0.0),
            Vec4::W,
        )
    }

    /// Create a transformation matrix from rotation and translation
    pub fn from_rotation_translation(rotation: crate::Quat, translation: Vec3) -> Self {
        let mut m = Self::from_quat(rotation);
        m.cols[3] = translation.extend(1.0);
        m
    }

    #[inline]
    pub fn transpose(&self) -> Self {
        Self::from_cols(
            Vec4::new(self.cols[0].x, self.cols[1].x, self.cols[2].x, self.cols[3].x),
            Vec4::new(self.cols[0].y, self.cols[1].y, self.cols[2].y, self.cols[3].y),
            Vec4::new(self.cols[0].z, self.cols[1].z, self.cols[2].z, self.cols[3].z),
            Vec4::new(self.cols[0].w, self.cols[1].w, self.cols[2].w, self.cols[3].w),
        )
    }

    /// Get the translation component
    #[inline]
    pub fn get_translation(&self) -> Vec3 {
        self.cols[3].truncate()
    }

    /// Transform a point (w=1)
    #[inline]
    pub fn transform_point(&self, point: Vec3) -> Vec3 {
        let v = *self * point.extend(1.0);
        v.truncate() / v.w
    }

    /// Transform a vector (w=0)
    #[inline]
    pub fn transform_vector(&self, vector: Vec3) -> Vec3 {
        (*self * vector.extend(0.0)).truncate()
    }

    /// Compute the inverse of this matrix
    pub fn inverse(&self) -> Self {
        let a = self.cols[0];
        let b = self.cols[1];
        let c = self.cols[2];
        let d = self.cols[3];

        let s0 = a.x * b.y - b.x * a.y;
        let s1 = a.x * b.z - b.x * a.z;
        let s2 = a.x * b.w - b.x * a.w;
        let s3 = a.y * b.z - b.y * a.z;
        let s4 = a.y * b.w - b.y * a.w;
        let s5 = a.z * b.w - b.z * a.w;

        let c5 = c.z * d.w - d.z * c.w;
        let c4 = c.y * d.w - d.y * c.w;
        let c3 = c.y * d.z - d.y * c.z;
        let c2 = c.x * d.w - d.x * c.w;
        let c1 = c.x * d.z - d.x * c.z;
        let c0 = c.x * d.y - d.x * c.y;

        let det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
        let inv_det = 1.0 / det;

        Self::from_cols(
            Vec4::new(
                (b.y * c5 - b.z * c4 + b.w * c3) * inv_det,
                (-a.y * c5 + a.z * c4 - a.w * c3) * inv_det,
                (d.y * s5 - d.z * s4 + d.w * s3) * inv_det,
                (-c.y * s5 + c.z * s4 - c.w * s3) * inv_det,
            ),
            Vec4::new(
                (-b.x * c5 + b.z * c2 - b.w * c1) * inv_det,
                (a.x * c5 - a.z * c2 + a.w * c1) * inv_det,
                (-d.x * s5 + d.z * s2 - d.w * s1) * inv_det,
                (c.x * s5 - c.z * s2 + c.w * s1) * inv_det,
            ),
            Vec4::new(
                (b.x * c4 - b.y * c2 + b.w * c0) * inv_det,
                (-a.x * c4 + a.y * c2 - a.w * c0) * inv_det,
                (d.x * s4 - d.y * s2 + d.w * s0) * inv_det,
                (-c.x * s4 + c.y * s2 - c.w * s0) * inv_det,
            ),
            Vec4::new(
                (-b.x * c3 + b.y * c1 - b.z * c0) * inv_det,
                (a.x * c3 - a.y * c1 + a.z * c0) * inv_det,
                (-d.x * s3 + d.y * s1 - d.z * s0) * inv_det,
                (c.x * s3 - c.y * s1 + c.z * s0) * inv_det,
            ),
        )
    }

    /// Convert to flat array (column-major)
    pub fn to_array(&self) -> [f32; 16] {
        [
            self.cols[0].x, self.cols[0].y, self.cols[0].z, self.cols[0].w,
            self.cols[1].x, self.cols[1].y, self.cols[1].z, self.cols[1].w,
            self.cols[2].x, self.cols[2].y, self.cols[2].z, self.cols[2].w,
            self.cols[3].x, self.cols[3].y, self.cols[3].z, self.cols[3].w,
        ]
    }

    /// Convert to 2D array (column-major) - useful for GPU uniforms
    pub fn to_cols_array_2d(&self) -> [[f32; 4]; 4] {
        [
            [self.cols[0].x, self.cols[0].y, self.cols[0].z, self.cols[0].w],
            [self.cols[1].x, self.cols[1].y, self.cols[1].z, self.cols[1].w],
            [self.cols[2].x, self.cols[2].y, self.cols[2].z, self.cols[2].w],
            [self.cols[3].x, self.cols[3].y, self.cols[3].z, self.cols[3].w],
        ]
    }

    /// Alias for from_translation - create translation matrix
    #[inline]
    pub fn translation(v: Vec3) -> Self {
        Self::from_translation(v)
    }

    /// Alias for from_scale - create scale matrix
    #[inline]
    pub fn scale(v: Vec3) -> Self {
        Self::from_scale(v)
    }
}

impl Default for Mat4 {
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl Mul for Mat4 {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self {
        Self::from_cols(
            self * rhs.cols[0],
            self * rhs.cols[1],
            self * rhs.cols[2],
            self * rhs.cols[3],
        )
    }
}

impl Mul<Vec4> for Mat4 {
    type Output = Vec4;

    #[inline]
    fn mul(self, rhs: Vec4) -> Vec4 {
        self.cols[0] * rhs.x + self.cols[1] * rhs.y + self.cols[2] * rhs.z + self.cols[3] * rhs.w
    }
}

impl MulAssign for Mat4 {
    fn mul_assign(&mut self, rhs: Self) {
        *self = *self * rhs;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mat4_identity() {
        let m = Mat4::IDENTITY;
        let v = Vec4::new(1.0, 2.0, 3.0, 1.0);
        let result = m * v;
        assert_eq!(result, v);
    }

    #[test]
    fn test_mat4_translation() {
        let m = Mat4::from_translation(Vec3::new(1.0, 2.0, 3.0));
        let point = Vec3::ZERO;
        let result = m.transform_point(point);
        assert!((result - Vec3::new(1.0, 2.0, 3.0)).length() < 1e-6);
    }

    #[test]
    fn test_mat4_inverse() {
        let m = Mat4::from_translation(Vec3::new(1.0, 2.0, 3.0));
        let inv = m.inverse();
        let result = m * inv;

        for i in 0..4 {
            for j in 0..4 {
                let expected = if i == j { 1.0 } else { 0.0 };
                let actual = match j {
                    0 => result.cols[i].x,
                    1 => result.cols[i].y,
                    2 => result.cols[i].z,
                    3 => result.cols[i].w,
                    _ => unreachable!(),
                };
                assert!((actual - expected).abs() < 1e-5);
            }
        }
    }
}
