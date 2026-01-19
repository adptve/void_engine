//! # void_math - SIMD-Optimized Math Library
//!
//! High-performance math primitives for 3D graphics and physics.
//! Uses SIMD acceleration where available for maximum throughput.

#![cfg_attr(not(feature = "std"), no_std)]

pub mod vector;
pub mod matrix;
pub mod quaternion;
pub mod transform;
pub mod bounds;
pub mod frustum;
pub mod ray;
pub mod intersect;
pub mod precision;

pub use vector::*;
pub use matrix::*;
pub use quaternion::*;
pub use transform::*;
pub use bounds::*;
pub use frustum::*;
pub use ray::*;
pub use intersect::*;
pub use precision::*;

/// Common math constants
pub mod consts {
    pub const PI: f32 = core::f32::consts::PI;
    pub const TAU: f32 = PI * 2.0;
    pub const FRAC_PI_2: f32 = PI / 2.0;
    pub const FRAC_PI_4: f32 = PI / 4.0;
    pub const DEG_TO_RAD: f32 = PI / 180.0;
    pub const RAD_TO_DEG: f32 = 180.0 / PI;
    pub const EPSILON: f32 = 1e-6;
}

/// Convert degrees to radians
#[inline]
pub fn radians(degrees: f32) -> f32 {
    degrees * consts::DEG_TO_RAD
}

/// Convert radians to degrees
#[inline]
pub fn degrees(radians: f32) -> f32 {
    radians * consts::RAD_TO_DEG
}

/// Linear interpolation
#[inline]
pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a + (b - a) * t
}

/// Clamp value between min and max
#[inline]
pub fn clamp(value: f32, min: f32, max: f32) -> f32 {
    if value < min { min }
    else if value > max { max }
    else { value }
}

/// Smooth step interpolation
#[inline]
pub fn smoothstep(edge0: f32, edge1: f32, x: f32) -> f32 {
    let t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    t * t * (3.0 - 2.0 * t)
}

/// Fast inverse square root (Quake-style, modernized)
#[inline]
pub fn fast_inv_sqrt(x: f32) -> f32 {
    let half = 0.5 * x;
    let i = x.to_bits();
    let i = 0x5f3759df - (i >> 1);
    let y = f32::from_bits(i);
    y * (1.5 - half * y * y)
}

pub mod prelude {
    pub use crate::vector::{Vec2, Vec3, Vec4};
    pub use crate::matrix::{Mat3, Mat4};
    pub use crate::quaternion::Quat;
    pub use crate::transform::Transform;
    pub use crate::bounds::{AABB, Sphere, Frustum};
    pub use crate::frustum::{Plane, FrustumPlanes, FrustumTestResult};
    pub use crate::ray::Ray;
    pub use crate::intersect::{
        ray_aabb, ray_sphere, ray_triangle, ray_plane,
        TriangleHit, interpolate_normal, interpolate_uv,
    };
    pub use crate::precision::{
        Vec3d, PrecisionStatus, PrecisionError,
        check_precision, world_to_local_safe, local_to_world,
    };
    pub use crate::{radians, degrees, lerp, clamp, smoothstep};
}
