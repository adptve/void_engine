//! World-Space Precision Management
//!
//! Utilities for handling precision issues in large worlds:
//! - Double-precision (f64) vector operations
//! - Precision status checking
//! - Safe coordinate conversion
//!
//! # Example
//!
//! ```ignore
//! use void_math::precision::{Vec3d, check_precision, PrecisionStatus};
//!
//! // High-precision world position (1000km from origin)
//! let world_pos = Vec3d::new(1_000_000.0, 500.0, 0.0);
//!
//! // Convert to local coordinates relative to origin
//! let origin = Vec3d::new(1_000_000.0, 0.0, 0.0);
//! let local = (world_pos - origin).to_f32();
//!
//! // Check precision status
//! assert_eq!(check_precision(local), PrecisionStatus::Good);
//! ```

use core::ops::{Add, Sub, Mul, Div, Neg, AddAssign, SubAssign, MulAssign};

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

/// Double-precision 3D vector for large-world coordinates
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Vec3d {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

impl Vec3d {
    /// Zero vector
    pub const ZERO: Self = Self::new(0.0, 0.0, 0.0);

    /// Unit vectors
    pub const X: Self = Self::new(1.0, 0.0, 0.0);
    pub const Y: Self = Self::new(0.0, 1.0, 0.0);
    pub const Z: Self = Self::new(0.0, 0.0, 1.0);

    /// Create a new vector
    #[inline]
    pub const fn new(x: f64, y: f64, z: f64) -> Self {
        Self { x, y, z }
    }

    /// Create from array
    #[inline]
    pub const fn from_array(arr: [f64; 3]) -> Self {
        Self::new(arr[0], arr[1], arr[2])
    }

    /// Create a vector with all components equal
    #[inline]
    pub fn splat(v: f64) -> Self {
        Self::new(v, v, v)
    }

    /// Convert to array
    #[inline]
    pub fn to_array(self) -> [f64; 3] {
        [self.x, self.y, self.z]
    }

    /// Dot product
    #[inline]
    pub fn dot(self, other: Self) -> f64 {
        self.x * other.x + self.y * other.y + self.z * other.z
    }

    /// Cross product
    #[inline]
    pub fn cross(self, other: Self) -> Self {
        Self::new(
            self.y * other.z - self.z * other.y,
            self.z * other.x - self.x * other.z,
            self.x * other.y - self.y * other.x,
        )
    }

    /// Squared length
    #[inline]
    pub fn length_squared(self) -> f64 {
        self.dot(self)
    }

    /// Length (magnitude)
    #[inline]
    pub fn length(self) -> f64 {
        self.length_squared().sqrt()
    }

    /// Distance to another point
    #[inline]
    pub fn distance(self, other: Self) -> f64 {
        (self - other).length()
    }

    /// Squared distance to another point
    #[inline]
    pub fn distance_squared(self, other: Self) -> f64 {
        (self - other).length_squared()
    }

    /// Normalize to unit length
    #[inline]
    pub fn normalize(self) -> Self {
        let len = self.length();
        if len > 0.0 {
            self / len
        } else {
            Self::ZERO
        }
    }

    /// Normalize, returning zero if length is too small
    #[inline]
    pub fn normalize_or_zero(self) -> Self {
        let len = self.length();
        if len > f64::EPSILON {
            self / len
        } else {
            Self::ZERO
        }
    }

    /// Convert to single-precision (f32) array
    #[inline]
    pub fn to_f32(self) -> [f32; 3] {
        [self.x as f32, self.y as f32, self.z as f32]
    }

    /// Create from single-precision values
    #[inline]
    pub fn from_f32(arr: [f32; 3]) -> Self {
        Self::new(arr[0] as f64, arr[1] as f64, arr[2] as f64)
    }

    /// Linear interpolation
    #[inline]
    pub fn lerp(self, other: Self, t: f64) -> Self {
        self + (other - self) * t
    }

    /// Component-wise minimum
    #[inline]
    pub fn min(self, other: Self) -> Self {
        Self::new(
            self.x.min(other.x),
            self.y.min(other.y),
            self.z.min(other.z),
        )
    }

    /// Component-wise maximum
    #[inline]
    pub fn max(self, other: Self) -> Self {
        Self::new(
            self.x.max(other.x),
            self.y.max(other.y),
            self.z.max(other.z),
        )
    }

    /// Absolute value of all components
    #[inline]
    pub fn abs(self) -> Self {
        Self::new(self.x.abs(), self.y.abs(), self.z.abs())
    }

    /// Check if all components are finite (not NaN or infinity)
    #[inline]
    pub fn is_finite(self) -> bool {
        self.x.is_finite() && self.y.is_finite() && self.z.is_finite()
    }

    /// Maximum component value
    #[inline]
    pub fn max_component(self) -> f64 {
        self.x.max(self.y).max(self.z)
    }

    /// Minimum component value
    #[inline]
    pub fn min_component(self) -> f64 {
        self.x.min(self.y).min(self.z)
    }
}

// Operator implementations
impl Add for Vec3d {
    type Output = Self;
    #[inline]
    fn add(self, rhs: Self) -> Self {
        Self::new(self.x + rhs.x, self.y + rhs.y, self.z + rhs.z)
    }
}

impl Sub for Vec3d {
    type Output = Self;
    #[inline]
    fn sub(self, rhs: Self) -> Self {
        Self::new(self.x - rhs.x, self.y - rhs.y, self.z - rhs.z)
    }
}

impl Mul<f64> for Vec3d {
    type Output = Self;
    #[inline]
    fn mul(self, rhs: f64) -> Self {
        Self::new(self.x * rhs, self.y * rhs, self.z * rhs)
    }
}

impl Mul<Vec3d> for f64 {
    type Output = Vec3d;
    #[inline]
    fn mul(self, rhs: Vec3d) -> Vec3d {
        Vec3d::new(self * rhs.x, self * rhs.y, self * rhs.z)
    }
}

impl Div<f64> for Vec3d {
    type Output = Self;
    #[inline]
    fn div(self, rhs: f64) -> Self {
        Self::new(self.x / rhs, self.y / rhs, self.z / rhs)
    }
}

impl Neg for Vec3d {
    type Output = Self;
    #[inline]
    fn neg(self) -> Self {
        Self::new(-self.x, -self.y, -self.z)
    }
}

impl AddAssign for Vec3d {
    #[inline]
    fn add_assign(&mut self, rhs: Self) {
        self.x += rhs.x;
        self.y += rhs.y;
        self.z += rhs.z;
    }
}

impl SubAssign for Vec3d {
    #[inline]
    fn sub_assign(&mut self, rhs: Self) {
        self.x -= rhs.x;
        self.y -= rhs.y;
        self.z -= rhs.z;
    }
}

impl MulAssign<f64> for Vec3d {
    #[inline]
    fn mul_assign(&mut self, rhs: f64) {
        self.x *= rhs;
        self.y *= rhs;
        self.z *= rhs;
    }
}

impl From<[f64; 3]> for Vec3d {
    fn from(arr: [f64; 3]) -> Self {
        Self::from_array(arr)
    }
}

impl From<Vec3d> for [f64; 3] {
    fn from(v: Vec3d) -> Self {
        v.to_array()
    }
}

/// Precision status for coordinate values
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum PrecisionStatus {
    /// Coordinates have good precision (< 100km from origin)
    Good,
    /// Coordinates are losing precision (100km - 1000km)
    Warning,
    /// Coordinates have significant precision loss (> 1000km)
    Critical,
}

impl PrecisionStatus {
    /// Check if precision is acceptable for rendering
    pub fn is_acceptable(self) -> bool {
        matches!(self, PrecisionStatus::Good | PrecisionStatus::Warning)
    }

    /// Check if rebasing is recommended
    pub fn needs_rebase(self) -> bool {
        matches!(self, PrecisionStatus::Warning | PrecisionStatus::Critical)
    }
}

/// Default thresholds for precision checking (in meters)
pub const PRECISION_WARNING_THRESHOLD: f32 = 100_000.0; // 100km
pub const PRECISION_CRITICAL_THRESHOLD: f32 = 1_000_000.0; // 1000km

/// Check if f32 position is losing precision
///
/// Returns the precision status based on distance from origin.
/// At ~100km, f32 precision drops to about 1cm.
/// At ~1000km, f32 precision drops to about 10cm.
pub fn check_precision(pos: [f32; 3]) -> PrecisionStatus {
    let max_coord = pos[0].abs().max(pos[1].abs()).max(pos[2].abs());

    if max_coord > PRECISION_CRITICAL_THRESHOLD {
        PrecisionStatus::Critical
    } else if max_coord > PRECISION_WARNING_THRESHOLD {
        PrecisionStatus::Warning
    } else {
        PrecisionStatus::Good
    }
}

/// Check precision with custom thresholds
pub fn check_precision_with_thresholds(
    pos: [f32; 3],
    warning_threshold: f32,
    critical_threshold: f32,
) -> PrecisionStatus {
    let max_coord = pos[0].abs().max(pos[1].abs()).max(pos[2].abs());

    if max_coord > critical_threshold {
        PrecisionStatus::Critical
    } else if max_coord > warning_threshold {
        PrecisionStatus::Warning
    } else {
        PrecisionStatus::Good
    }
}

/// Error types for precision-related operations
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum PrecisionError {
    /// Coordinate overflow (infinity or NaN)
    Overflow,
    /// Significant precision loss detected
    PrecisionLoss,
    /// Input coordinates are invalid
    InvalidInput,
}

impl core::fmt::Display for PrecisionError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            PrecisionError::Overflow => write!(f, "Coordinate overflow (infinity or NaN)"),
            PrecisionError::PrecisionLoss => write!(f, "Significant precision loss detected"),
            PrecisionError::InvalidInput => write!(f, "Invalid input coordinates"),
        }
    }
}

/// Safe conversion from f64 world coordinates to f32 local coordinates
///
/// Subtracts the origin and converts to f32, checking for precision issues.
pub fn world_to_local_safe(
    world: [f64; 3],
    origin: [f64; 3],
) -> Result<[f32; 3], PrecisionError> {
    let local = [
        (world[0] - origin[0]) as f32,
        (world[1] - origin[1]) as f32,
        (world[2] - origin[2]) as f32,
    ];

    // Check for infinity or NaN
    if local.iter().any(|v| !v.is_finite()) {
        return Err(PrecisionError::Overflow);
    }

    // Check for precision loss
    if check_precision(local) == PrecisionStatus::Critical {
        return Err(PrecisionError::PrecisionLoss);
    }

    Ok(local)
}

/// Convert local f32 coordinates back to world f64 coordinates
pub fn local_to_world(local: [f32; 3], origin: [f64; 3]) -> [f64; 3] {
    [
        origin[0] + local[0] as f64,
        origin[1] + local[1] as f64,
        origin[2] + local[2] as f64,
    ]
}

/// Calculate the direction from origin to a world position (normalized)
///
/// Useful for billboard rendering when position has precision issues.
pub fn direction_from_origin(world: [f64; 3], origin: [f64; 3]) -> [f32; 3] {
    let dx = world[0] - origin[0];
    let dy = world[1] - origin[1];
    let dz = world[2] - origin[2];
    let len = (dx * dx + dy * dy + dz * dz).sqrt();

    if len > f64::EPSILON {
        [(dx / len) as f32, (dy / len) as f32, (dz / len) as f32]
    } else {
        [0.0, 0.0, 1.0] // Default forward direction
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_vec3d_basic_ops() {
        let a = Vec3d::new(1.0, 2.0, 3.0);
        let b = Vec3d::new(4.0, 5.0, 6.0);

        let sum = a + b;
        assert_eq!(sum.x, 5.0);
        assert_eq!(sum.y, 7.0);
        assert_eq!(sum.z, 9.0);

        let diff = b - a;
        assert_eq!(diff.x, 3.0);
        assert_eq!(diff.y, 3.0);
        assert_eq!(diff.z, 3.0);

        let scaled = a * 2.0;
        assert_eq!(scaled.x, 2.0);
        assert_eq!(scaled.y, 4.0);
        assert_eq!(scaled.z, 6.0);
    }

    #[test]
    fn test_vec3d_length() {
        let v = Vec3d::new(3.0, 4.0, 0.0);
        assert!((v.length() - 5.0).abs() < 1e-10);

        let unit = v.normalize();
        assert!((unit.length() - 1.0).abs() < 1e-10);
    }

    #[test]
    fn test_vec3d_dot_cross() {
        let a = Vec3d::X;
        let b = Vec3d::Y;

        assert_eq!(a.dot(b), 0.0);
        assert_eq!(a.dot(a), 1.0);

        let cross = a.cross(b);
        assert!((cross.x - 0.0).abs() < 1e-10);
        assert!((cross.y - 0.0).abs() < 1e-10);
        assert!((cross.z - 1.0).abs() < 1e-10);
    }

    #[test]
    fn test_vec3d_to_f32() {
        let v = Vec3d::new(1.5, 2.5, 3.5);
        let arr = v.to_f32();
        assert_eq!(arr, [1.5f32, 2.5f32, 3.5f32]);
    }

    #[test]
    fn test_check_precision() {
        // Good precision near origin
        assert_eq!(check_precision([100.0, 200.0, 300.0]), PrecisionStatus::Good);

        // Warning at 100km+
        assert_eq!(check_precision([150_000.0, 0.0, 0.0]), PrecisionStatus::Warning);

        // Critical at 1000km+
        assert_eq!(check_precision([2_000_000.0, 0.0, 0.0]), PrecisionStatus::Critical);
    }

    #[test]
    fn test_precision_status_methods() {
        assert!(PrecisionStatus::Good.is_acceptable());
        assert!(PrecisionStatus::Warning.is_acceptable());
        assert!(!PrecisionStatus::Critical.is_acceptable());

        assert!(!PrecisionStatus::Good.needs_rebase());
        assert!(PrecisionStatus::Warning.needs_rebase());
        assert!(PrecisionStatus::Critical.needs_rebase());
    }

    #[test]
    fn test_world_to_local_safe() {
        let origin = [1_000_000.0f64, 0.0, 0.0];
        let world = [1_000_100.0f64, 50.0, 0.0];

        let local = world_to_local_safe(world, origin).unwrap();
        assert!((local[0] - 100.0).abs() < 0.01);
        assert!((local[1] - 50.0).abs() < 0.01);
        assert!((local[2] - 0.0).abs() < 0.01);
    }

    #[test]
    fn test_world_to_local_precision_loss() {
        let origin = [0.0f64, 0.0, 0.0];
        let world = [10_000_000.0f64, 0.0, 0.0]; // 10,000 km - too far

        let result = world_to_local_safe(world, origin);
        assert_eq!(result, Err(PrecisionError::PrecisionLoss));
    }

    #[test]
    fn test_local_to_world() {
        let origin = [1_000_000.0f64, 500.0, -2_000.0];
        let local = [100.0f32, 50.0, 0.0];

        let world = local_to_world(local, origin);
        assert!((world[0] - 1_000_100.0).abs() < 0.01);
        assert!((world[1] - 550.0).abs() < 0.01);
        assert!((world[2] - -2_000.0).abs() < 0.01);
    }

    #[test]
    fn test_direction_from_origin() {
        let origin = [0.0f64, 0.0, 0.0];
        let world = [1000.0f64, 0.0, 0.0];

        let dir = direction_from_origin(world, origin);
        assert!((dir[0] - 1.0).abs() < 0.001);
        assert!(dir[1].abs() < 0.001);
        assert!(dir[2].abs() < 0.001);
    }

    #[cfg(feature = "serde")]
    #[test]
    fn test_vec3d_serialization() {
        let v = Vec3d::new(1.5, 2.5, 3.5);
        let serialized = bincode::serialize(&v).unwrap();
        let deserialized: Vec3d = bincode::deserialize(&serialized).unwrap();
        assert_eq!(v, deserialized);
    }

    #[cfg(feature = "serde")]
    #[test]
    fn test_precision_status_serialization() {
        let status = PrecisionStatus::Warning;
        let serialized = bincode::serialize(&status).unwrap();
        let deserialized: PrecisionStatus = bincode::deserialize(&serialized).unwrap();
        assert_eq!(status, deserialized);
    }

    #[cfg(feature = "serde")]
    #[test]
    fn test_precision_error_serialization() {
        let error = PrecisionError::PrecisionLoss;
        let serialized = bincode::serialize(&error).unwrap();
        let deserialized: PrecisionError = bincode::deserialize(&serialized).unwrap();
        assert_eq!(error, deserialized);
    }
}
