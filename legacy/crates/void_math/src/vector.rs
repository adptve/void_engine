//! Vector types with SIMD optimization

use core::ops::{Add, Sub, Mul, Div, Neg, AddAssign, SubAssign, MulAssign};

/// 2D vector
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(C)]
pub struct Vec2 {
    pub x: f32,
    pub y: f32,
}

impl Vec2 {
    pub const ZERO: Self = Self::new(0.0, 0.0);
    pub const ONE: Self = Self::new(1.0, 1.0);
    pub const X: Self = Self::new(1.0, 0.0);
    pub const Y: Self = Self::new(0.0, 1.0);

    #[inline]
    pub const fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }

    #[inline]
    pub fn splat(v: f32) -> Self {
        Self::new(v, v)
    }

    #[inline]
    pub fn dot(self, other: Self) -> f32 {
        self.x * other.x + self.y * other.y
    }

    #[inline]
    pub fn length_squared(self) -> f32 {
        self.dot(self)
    }

    #[inline]
    pub fn length(self) -> f32 {
        self.length_squared().sqrt()
    }

    #[inline]
    pub fn normalize(self) -> Self {
        let len = self.length();
        if len > 0.0 { self / len } else { Self::ZERO }
    }

    #[inline]
    pub fn lerp(self, other: Self, t: f32) -> Self {
        self + (other - self) * t
    }

    #[inline]
    pub fn perpendicular(self) -> Self {
        Self::new(-self.y, self.x)
    }

    #[inline]
    pub fn to_array(self) -> [f32; 2] {
        [self.x, self.y]
    }
}

/// 3D vector - the workhorse of 3D graphics
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(C, align(16))]
pub struct Vec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    #[cfg_attr(feature = "serde", serde(skip))]
    _pad: f32, // Padding for SIMD alignment
}

impl Vec3 {
    pub const ZERO: Self = Self::new(0.0, 0.0, 0.0);
    pub const ONE: Self = Self::new(1.0, 1.0, 1.0);
    pub const X: Self = Self::new(1.0, 0.0, 0.0);
    pub const Y: Self = Self::new(0.0, 1.0, 0.0);
    pub const Z: Self = Self::new(0.0, 0.0, 1.0);
    pub const NEG_X: Self = Self::new(-1.0, 0.0, 0.0);
    pub const NEG_Y: Self = Self::new(0.0, -1.0, 0.0);
    pub const NEG_Z: Self = Self::new(0.0, 0.0, -1.0);

    #[inline]
    pub const fn new(x: f32, y: f32, z: f32) -> Self {
        Self { x, y, z, _pad: 0.0 }
    }

    #[inline]
    pub fn splat(v: f32) -> Self {
        Self::new(v, v, v)
    }

    #[inline]
    pub fn dot(self, other: Self) -> f32 {
        self.x * other.x + self.y * other.y + self.z * other.z
    }

    #[inline]
    pub fn cross(self, other: Self) -> Self {
        Self::new(
            self.y * other.z - self.z * other.y,
            self.z * other.x - self.x * other.z,
            self.x * other.y - self.y * other.x,
        )
    }

    #[inline]
    pub fn length_squared(self) -> f32 {
        self.dot(self)
    }

    #[inline]
    pub fn length(self) -> f32 {
        self.length_squared().sqrt()
    }

    #[inline]
    pub fn normalize(self) -> Self {
        let len = self.length();
        if len > 0.0 { self / len } else { Self::ZERO }
    }

    #[inline]
    pub fn normalize_or_zero(self) -> Self {
        let len_sq = self.length_squared();
        if len_sq > 1e-10 {
            self / len_sq.sqrt()
        } else {
            Self::ZERO
        }
    }

    #[inline]
    pub fn lerp(self, other: Self, t: f32) -> Self {
        self + (other - self) * t
    }

    #[inline]
    pub fn reflect(self, normal: Self) -> Self {
        self - normal * 2.0 * self.dot(normal)
    }

    #[inline]
    pub fn project_onto(self, other: Self) -> Self {
        other * (self.dot(other) / other.length_squared())
    }

    #[inline]
    pub fn min(self, other: Self) -> Self {
        Self::new(
            self.x.min(other.x),
            self.y.min(other.y),
            self.z.min(other.z),
        )
    }

    #[inline]
    pub fn max(self, other: Self) -> Self {
        Self::new(
            self.x.max(other.x),
            self.y.max(other.y),
            self.z.max(other.z),
        )
    }

    #[inline]
    pub fn abs(self) -> Self {
        Self::new(self.x.abs(), self.y.abs(), self.z.abs())
    }

    #[inline]
    pub fn to_array(self) -> [f32; 3] {
        [self.x, self.y, self.z]
    }

    #[inline]
    pub fn extend(self, w: f32) -> Vec4 {
        Vec4::new(self.x, self.y, self.z, w)
    }
}

/// 4D vector - for homogeneous coordinates and colors
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(C, align(16))]
pub struct Vec4 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

impl Vec4 {
    pub const ZERO: Self = Self::new(0.0, 0.0, 0.0, 0.0);
    pub const ONE: Self = Self::new(1.0, 1.0, 1.0, 1.0);
    pub const X: Self = Self::new(1.0, 0.0, 0.0, 0.0);
    pub const Y: Self = Self::new(0.0, 1.0, 0.0, 0.0);
    pub const Z: Self = Self::new(0.0, 0.0, 1.0, 0.0);
    pub const W: Self = Self::new(0.0, 0.0, 0.0, 1.0);

    #[inline]
    pub const fn new(x: f32, y: f32, z: f32, w: f32) -> Self {
        Self { x, y, z, w }
    }

    #[inline]
    pub fn splat(v: f32) -> Self {
        Self::new(v, v, v, v)
    }

    #[inline]
    pub fn dot(self, other: Self) -> f32 {
        self.x * other.x + self.y * other.y + self.z * other.z + self.w * other.w
    }

    #[inline]
    pub fn length_squared(self) -> f32 {
        self.dot(self)
    }

    #[inline]
    pub fn length(self) -> f32 {
        self.length_squared().sqrt()
    }

    #[inline]
    pub fn normalize(self) -> Self {
        let len = self.length();
        if len > 0.0 { self / len } else { Self::ZERO }
    }

    #[inline]
    pub fn lerp(self, other: Self, t: f32) -> Self {
        self + (other - self) * t
    }

    #[inline]
    pub fn truncate(self) -> Vec3 {
        Vec3::new(self.x, self.y, self.z)
    }

    #[inline]
    pub fn xyz(self) -> Vec3 {
        self.truncate()
    }

    #[inline]
    pub fn to_array(self) -> [f32; 4] {
        [self.x, self.y, self.z, self.w]
    }
}

// Operator implementations for Vec2
impl Add for Vec2 {
    type Output = Self;
    #[inline] fn add(self, rhs: Self) -> Self { Self::new(self.x + rhs.x, self.y + rhs.y) }
}
impl Sub for Vec2 {
    type Output = Self;
    #[inline] fn sub(self, rhs: Self) -> Self { Self::new(self.x - rhs.x, self.y - rhs.y) }
}
impl Mul<f32> for Vec2 {
    type Output = Self;
    #[inline] fn mul(self, rhs: f32) -> Self { Self::new(self.x * rhs, self.y * rhs) }
}
impl Div<f32> for Vec2 {
    type Output = Self;
    #[inline] fn div(self, rhs: f32) -> Self { Self::new(self.x / rhs, self.y / rhs) }
}
impl Neg for Vec2 {
    type Output = Self;
    #[inline] fn neg(self) -> Self { Self::new(-self.x, -self.y) }
}

// Operator implementations for Vec3
impl Add for Vec3 {
    type Output = Self;
    #[inline] fn add(self, rhs: Self) -> Self { Self::new(self.x + rhs.x, self.y + rhs.y, self.z + rhs.z) }
}
impl Sub for Vec3 {
    type Output = Self;
    #[inline] fn sub(self, rhs: Self) -> Self { Self::new(self.x - rhs.x, self.y - rhs.y, self.z - rhs.z) }
}
impl Mul<f32> for Vec3 {
    type Output = Self;
    #[inline] fn mul(self, rhs: f32) -> Self { Self::new(self.x * rhs, self.y * rhs, self.z * rhs) }
}
impl Mul<Vec3> for f32 {
    type Output = Vec3;
    #[inline] fn mul(self, rhs: Vec3) -> Vec3 { Vec3::new(self * rhs.x, self * rhs.y, self * rhs.z) }
}
impl Div<f32> for Vec3 {
    type Output = Self;
    #[inline] fn div(self, rhs: f32) -> Self { Self::new(self.x / rhs, self.y / rhs, self.z / rhs) }
}
impl Neg for Vec3 {
    type Output = Self;
    #[inline] fn neg(self) -> Self { Self::new(-self.x, -self.y, -self.z) }
}
impl AddAssign for Vec3 {
    #[inline] fn add_assign(&mut self, rhs: Self) { *self = *self + rhs; }
}
impl SubAssign for Vec3 {
    #[inline] fn sub_assign(&mut self, rhs: Self) { *self = *self - rhs; }
}
impl MulAssign<f32> for Vec3 {
    #[inline] fn mul_assign(&mut self, rhs: f32) { *self = *self * rhs; }
}

// Operator implementations for Vec4
impl Add for Vec4 {
    type Output = Self;
    #[inline] fn add(self, rhs: Self) -> Self { Self::new(self.x + rhs.x, self.y + rhs.y, self.z + rhs.z, self.w + rhs.w) }
}
impl Sub for Vec4 {
    type Output = Self;
    #[inline] fn sub(self, rhs: Self) -> Self { Self::new(self.x - rhs.x, self.y - rhs.y, self.z - rhs.z, self.w - rhs.w) }
}
impl Mul<f32> for Vec4 {
    type Output = Self;
    #[inline] fn mul(self, rhs: f32) -> Self { Self::new(self.x * rhs, self.y * rhs, self.z * rhs, self.w * rhs) }
}
impl Div<f32> for Vec4 {
    type Output = Self;
    #[inline] fn div(self, rhs: f32) -> Self { Self::new(self.x / rhs, self.y / rhs, self.z / rhs, self.w / rhs) }
}
impl Neg for Vec4 {
    type Output = Self;
    #[inline] fn neg(self) -> Self { Self::new(-self.x, -self.y, -self.z, -self.w) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_vec3_dot() {
        let a = Vec3::new(1.0, 2.0, 3.0);
        let b = Vec3::new(4.0, 5.0, 6.0);
        assert_eq!(a.dot(b), 32.0);
    }

    #[test]
    fn test_vec3_cross() {
        let x = Vec3::X;
        let y = Vec3::Y;
        let z = x.cross(y);
        assert!((z - Vec3::Z).length() < 1e-6);
    }

    #[test]
    fn test_vec3_normalize() {
        let v = Vec3::new(3.0, 0.0, 4.0);
        let n = v.normalize();
        assert!((n.length() - 1.0).abs() < 1e-6);
    }
}
