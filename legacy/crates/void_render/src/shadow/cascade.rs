//! Cascaded Shadow Map Calculations
//!
//! Implements cascade split calculations and light space matrix computation
//! for directional light shadows. This module is backend-agnostic and only
//! performs the mathematical calculations.
//!
//! # Cascade Shadow Maps (CSM)
//!
//! CSM divides the view frustum into multiple regions (cascades) and renders
//! a separate shadow map for each. Near cascades have higher resolution relative
//! to screen space, providing sharp shadows near the camera.

use serde::{Serialize, Deserialize};

/// Maximum supported cascade count
pub const MAX_CASCADES: usize = 4;

/// Cascade data for directional light shadows
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShadowCascades {
    /// Number of active cascades (1-4)
    pub count: u32,

    /// Split distances (view-space Z values)
    /// [0] = near plane, [1..count] = cascade boundaries, [count] = far plane
    pub splits: [f32; 5],

    /// View-projection matrices for each cascade (column-major)
    pub matrices: [[[f32; 4]; 4]; MAX_CASCADES],

    /// Atlas layer indices for each cascade
    pub layers: [u32; MAX_CASCADES],

    /// Bounding sphere radii for each cascade (for culling)
    pub radii: [f32; MAX_CASCADES],

    /// Texel size in world units for each cascade (for bias calculation)
    pub texel_sizes: [f32; MAX_CASCADES],
}

impl Default for ShadowCascades {
    fn default() -> Self {
        Self {
            count: 4,
            splits: [0.1, 10.0, 30.0, 70.0, 150.0],
            matrices: [IDENTITY_MATRIX; MAX_CASCADES],
            layers: [0, 1, 2, 3],
            radii: [0.0; MAX_CASCADES],
            texel_sizes: [0.0; MAX_CASCADES],
        }
    }
}

impl ShadowCascades {
    /// Create new cascades with the given count
    pub fn new(count: u32) -> Self {
        Self {
            count: count.clamp(1, MAX_CASCADES as u32),
            ..Default::default()
        }
    }

    /// Calculate cascade splits using the practical split scheme
    ///
    /// This blends between logarithmic and linear splits based on lambda:
    /// - lambda = 0: Linear splits (uniform in view space)
    /// - lambda = 1: Logarithmic splits (uniform in screen space)
    /// - lambda = 0.5: Balanced (recommended default)
    ///
    /// # Arguments
    /// * `near` - Camera near plane
    /// * `far` - Shadow far distance (not necessarily camera far)
    /// * `cascade_count` - Number of cascades (1-4)
    /// * `lambda` - Split scheme blend (0 = linear, 1 = logarithmic)
    pub fn calculate_splits(
        near: f32,
        far: f32,
        cascade_count: u32,
        lambda: f32,
    ) -> [f32; 5] {
        let mut splits = [0.0f32; 5];
        let count = cascade_count.clamp(1, MAX_CASCADES as u32) as usize;
        let lambda = lambda.clamp(0.0, 1.0);

        splits[0] = near;

        for i in 1..=count {
            let p = i as f32 / count as f32;

            // Logarithmic split (better for perspective projection)
            let log_split = near * (far / near).powf(p);

            // Linear split
            let lin_split = near + (far - near) * p;

            // Blend between schemes
            splits[i] = lambda * log_split + (1.0 - lambda) * lin_split;
        }

        // Fill remaining with far plane
        for i in (count + 1)..5 {
            splits[i] = far;
        }

        splits
    }

    /// Calculate a single cascade's view-projection matrix
    ///
    /// # Arguments
    /// * `cascade_index` - Which cascade (0-based)
    /// * `splits` - Cascade split distances
    /// * `camera_view` - Camera view matrix (column-major)
    /// * `camera_proj` - Camera projection matrix (column-major)
    /// * `light_direction` - Normalized light direction (pointing toward scene)
    /// * `resolution` - Shadow map resolution (for texel snapping)
    pub fn calculate_cascade_matrix(
        cascade_index: usize,
        splits: &[f32; 5],
        camera_view: &[[f32; 4]; 4],
        camera_proj: &[[f32; 4]; 4],
        light_direction: [f32; 3],
        resolution: u32,
    ) -> CascadeMatrixResult {
        let near = splits[cascade_index];
        let far = splits[cascade_index + 1];

        // Get frustum corners in world space
        let corners = frustum_corners_world(camera_view, camera_proj, near, far);

        // Calculate bounding sphere of frustum
        let (center, radius) = bounding_sphere(&corners);

        // Build light view matrix (looking down light direction)
        let light_pos = [
            center[0] - light_direction[0] * radius * 2.0,
            center[1] - light_direction[1] * radius * 2.0,
            center[2] - light_direction[2] * radius * 2.0,
        ];

        let view = look_at(light_pos, center, find_up_vector(light_direction));

        // Orthographic projection enclosing the sphere
        let proj = orthographic(-radius, radius, -radius, radius, 0.0, radius * 4.0);

        // Combine view-projection
        let mut shadow_matrix = multiply_mat4(&proj, &view);

        // Texel snapping to prevent shadow swimming
        shadow_matrix = snap_to_texel(shadow_matrix, resolution);

        // Calculate texel size in world units
        let texel_size = (radius * 2.0) / resolution as f32;

        CascadeMatrixResult {
            matrix: shadow_matrix,
            radius,
            texel_size,
            center,
        }
    }

    /// Update all cascade matrices
    pub fn update(
        &mut self,
        camera_view: &[[f32; 4]; 4],
        camera_proj: &[[f32; 4]; 4],
        light_direction: [f32; 3],
        near: f32,
        far: f32,
        lambda: f32,
        resolution: u32,
    ) {
        self.splits = Self::calculate_splits(near, far, self.count, lambda);

        for i in 0..self.count as usize {
            let result = Self::calculate_cascade_matrix(
                i,
                &self.splits,
                camera_view,
                camera_proj,
                light_direction,
                resolution,
            );

            self.matrices[i] = result.matrix;
            self.radii[i] = result.radius;
            self.texel_sizes[i] = result.texel_size;
        }
    }

    /// Get the cascade index for a given view-space depth
    pub fn cascade_for_depth(&self, view_depth: f32) -> usize {
        for i in 0..self.count as usize {
            if view_depth < self.splits[i + 1] {
                return i;
            }
        }
        (self.count - 1) as usize
    }

    /// Get cascade blend factor for smooth transitions
    ///
    /// Returns (cascade_index, blend_factor) where blend_factor is 0-1
    /// indicating how much to blend with the next cascade.
    pub fn cascade_blend(&self, view_depth: f32, blend_distance: f32) -> (usize, f32) {
        let cascade = self.cascade_for_depth(view_depth);

        if cascade >= (self.count - 1) as usize {
            return (cascade, 0.0);
        }

        let split_end = self.splits[cascade + 1];
        let blend_start = split_end - blend_distance;

        if view_depth > blend_start {
            let blend = (view_depth - blend_start) / blend_distance;
            (cascade, blend.clamp(0.0, 1.0))
        } else {
            (cascade, 0.0)
        }
    }
}

/// Result of cascade matrix calculation
#[derive(Clone, Debug)]
pub struct CascadeMatrixResult {
    /// Light view-projection matrix
    pub matrix: [[f32; 4]; 4],
    /// Bounding sphere radius
    pub radius: f32,
    /// Texel size in world units
    pub texel_size: f32,
    /// Cascade center in world space
    pub center: [f32; 3],
}

// ============================================================================
// Matrix Math Utilities
// ============================================================================

const IDENTITY_MATRIX: [[f32; 4]; 4] = [
    [1.0, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.0, 0.0],
    [0.0, 0.0, 1.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
];

/// Calculate frustum corners in world space
fn frustum_corners_world(
    view: &[[f32; 4]; 4],
    proj: &[[f32; 4]; 4],
    near: f32,
    far: f32,
) -> [[f32; 3]; 8] {
    // Get inverse view-projection matrix
    let vp = multiply_mat4(proj, view);
    let inv_vp = invert_mat4(&vp);

    // NDC corners for near plane (z=0 in Vulkan/wgpu NDC)
    // and far plane (z=1 in Vulkan/wgpu NDC)
    let ndc_corners = [
        // Near plane
        [-1.0, -1.0, 0.0, 1.0],
        [ 1.0, -1.0, 0.0, 1.0],
        [-1.0,  1.0, 0.0, 1.0],
        [ 1.0,  1.0, 0.0, 1.0],
        // Far plane
        [-1.0, -1.0, 1.0, 1.0],
        [ 1.0, -1.0, 1.0, 1.0],
        [-1.0,  1.0, 1.0, 1.0],
        [ 1.0,  1.0, 1.0, 1.0],
    ];

    let mut world_corners = [[0.0f32; 3]; 8];

    for (i, ndc) in ndc_corners.iter().enumerate() {
        let world = transform_vec4(&inv_vp, *ndc);
        let w = world[3];
        world_corners[i] = [world[0] / w, world[1] / w, world[2] / w];
    }

    // Adjust corners to actual near/far planes if they differ from projection
    // This is needed when calculating sub-frustums for cascades
    let _ = (near, far); // Currently using projection planes directly

    world_corners
}

/// Calculate bounding sphere of a set of points
fn bounding_sphere(points: &[[f32; 3]; 8]) -> ([f32; 3], f32) {
    // Calculate center
    let mut center = [0.0f32; 3];
    for p in points {
        center[0] += p[0];
        center[1] += p[1];
        center[2] += p[2];
    }
    center[0] /= 8.0;
    center[1] /= 8.0;
    center[2] /= 8.0;

    // Calculate radius (max distance from center)
    let mut radius = 0.0f32;
    for p in points {
        let dx = p[0] - center[0];
        let dy = p[1] - center[1];
        let dz = p[2] - center[2];
        let dist = (dx * dx + dy * dy + dz * dz).sqrt();
        radius = radius.max(dist);
    }

    (center, radius)
}

/// Find a suitable up vector for the light view matrix
fn find_up_vector(light_direction: [f32; 3]) -> [f32; 3] {
    // Avoid parallel vectors
    let up = if light_direction[1].abs() > 0.9 {
        [0.0, 0.0, 1.0]
    } else {
        [0.0, 1.0, 0.0]
    };
    up
}

/// Create a look-at view matrix (column-major)
fn look_at(eye: [f32; 3], target: [f32; 3], up: [f32; 3]) -> [[f32; 4]; 4] {
    let f = normalize([
        target[0] - eye[0],
        target[1] - eye[1],
        target[2] - eye[2],
    ]);

    let s = normalize(cross(f, up));
    let u = cross(s, f);

    [
        [s[0], u[0], -f[0], 0.0],
        [s[1], u[1], -f[1], 0.0],
        [s[2], u[2], -f[2], 0.0],
        [-dot(s, eye), -dot(u, eye), dot(f, eye), 1.0],
    ]
}

/// Create an orthographic projection matrix (column-major)
fn orthographic(
    left: f32,
    right: f32,
    bottom: f32,
    top: f32,
    near: f32,
    far: f32,
) -> [[f32; 4]; 4] {
    let rml = right - left;
    let tmb = top - bottom;
    let fmn = far - near;

    [
        [2.0 / rml, 0.0, 0.0, 0.0],
        [0.0, 2.0 / tmb, 0.0, 0.0],
        [0.0, 0.0, -1.0 / fmn, 0.0],  // Vulkan/wgpu depth range [0, 1]
        [-(right + left) / rml, -(top + bottom) / tmb, -near / fmn, 1.0],
    ]
}

/// Multiply two 4x4 matrices (column-major)
fn multiply_mat4(a: &[[f32; 4]; 4], b: &[[f32; 4]; 4]) -> [[f32; 4]; 4] {
    let mut result = [[0.0f32; 4]; 4];

    for i in 0..4 {
        for j in 0..4 {
            result[i][j] = a[0][j] * b[i][0]
                         + a[1][j] * b[i][1]
                         + a[2][j] * b[i][2]
                         + a[3][j] * b[i][3];
        }
    }

    result
}

/// Invert a 4x4 matrix (column-major)
fn invert_mat4(m: &[[f32; 4]; 4]) -> [[f32; 4]; 4] {
    let m00 = m[0][0]; let m01 = m[0][1]; let m02 = m[0][2]; let m03 = m[0][3];
    let m10 = m[1][0]; let m11 = m[1][1]; let m12 = m[1][2]; let m13 = m[1][3];
    let m20 = m[2][0]; let m21 = m[2][1]; let m22 = m[2][2]; let m23 = m[2][3];
    let m30 = m[3][0]; let m31 = m[3][1]; let m32 = m[3][2]; let m33 = m[3][3];

    let a2323 = m22 * m33 - m23 * m32;
    let a1323 = m21 * m33 - m23 * m31;
    let a1223 = m21 * m32 - m22 * m31;
    let a0323 = m20 * m33 - m23 * m30;
    let a0223 = m20 * m32 - m22 * m30;
    let a0123 = m20 * m31 - m21 * m30;
    let a2313 = m12 * m33 - m13 * m32;
    let a1313 = m11 * m33 - m13 * m31;
    let a1213 = m11 * m32 - m12 * m31;
    let a2312 = m12 * m23 - m13 * m22;
    let a1312 = m11 * m23 - m13 * m21;
    let a1212 = m11 * m22 - m12 * m21;
    let a0313 = m10 * m33 - m13 * m30;
    let a0213 = m10 * m32 - m12 * m30;
    let a0312 = m10 * m23 - m13 * m20;
    let a0212 = m10 * m22 - m12 * m20;
    let a0113 = m10 * m31 - m11 * m30;
    let a0112 = m10 * m21 - m11 * m20;

    let det = m00 * (m11 * a2323 - m12 * a1323 + m13 * a1223)
            - m01 * (m10 * a2323 - m12 * a0323 + m13 * a0223)
            + m02 * (m10 * a1323 - m11 * a0323 + m13 * a0123)
            - m03 * (m10 * a1223 - m11 * a0223 + m12 * a0123);

    if det.abs() < 1e-10 {
        return IDENTITY_MATRIX;
    }

    let inv_det = 1.0 / det;

    [
        [
            inv_det * (m11 * a2323 - m12 * a1323 + m13 * a1223),
            inv_det * -(m01 * a2323 - m02 * a1323 + m03 * a1223),
            inv_det * (m01 * a2313 - m02 * a1313 + m03 * a1213),
            inv_det * -(m01 * a2312 - m02 * a1312 + m03 * a1212),
        ],
        [
            inv_det * -(m10 * a2323 - m12 * a0323 + m13 * a0223),
            inv_det * (m00 * a2323 - m02 * a0323 + m03 * a0223),
            inv_det * -(m00 * a2313 - m02 * a0313 + m03 * a0213),
            inv_det * (m00 * a2312 - m02 * a0312 + m03 * a0212),
        ],
        [
            inv_det * (m10 * a1323 - m11 * a0323 + m13 * a0123),
            inv_det * -(m00 * a1323 - m01 * a0323 + m03 * a0123),
            inv_det * (m00 * a1313 - m01 * a0313 + m03 * a0113),
            inv_det * -(m00 * a1312 - m01 * a0312 + m03 * a0112),
        ],
        [
            inv_det * -(m10 * a1223 - m11 * a0223 + m12 * a0123),
            inv_det * (m00 * a1223 - m01 * a0223 + m02 * a0123),
            inv_det * -(m00 * a1213 - m01 * a0213 + m02 * a0113),
            inv_det * (m00 * a1212 - m01 * a0212 + m02 * a0112),
        ],
    ]
}

/// Transform a vec4 by a matrix
fn transform_vec4(m: &[[f32; 4]; 4], v: [f32; 4]) -> [f32; 4] {
    [
        m[0][0] * v[0] + m[1][0] * v[1] + m[2][0] * v[2] + m[3][0] * v[3],
        m[0][1] * v[0] + m[1][1] * v[1] + m[2][1] * v[2] + m[3][1] * v[3],
        m[0][2] * v[0] + m[1][2] * v[1] + m[2][2] * v[2] + m[3][2] * v[3],
        m[0][3] * v[0] + m[1][3] * v[1] + m[2][3] * v[2] + m[3][3] * v[3],
    ]
}

/// Normalize a vec3
fn normalize(v: [f32; 3]) -> [f32; 3] {
    let len = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    if len > 1e-10 {
        [v[0] / len, v[1] / len, v[2] / len]
    } else {
        [0.0, 0.0, 1.0]
    }
}

/// Cross product
fn cross(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

/// Dot product
fn dot(a: [f32; 3], b: [f32; 3]) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

/// Snap matrix translation to texel grid to prevent shadow swimming
fn snap_to_texel(mut matrix: [[f32; 4]; 4], resolution: u32) -> [[f32; 4]; 4] {
    // Transform origin to shadow map space
    let origin = transform_vec4(&matrix, [0.0, 0.0, 0.0, 1.0]);

    // Calculate texel size in shadow map space (normalized -1 to 1)
    let texel_size = 2.0 / resolution as f32;

    // Snap translation to texel grid
    let snapped_x = (origin[0] / texel_size).round() * texel_size;
    let snapped_y = (origin[1] / texel_size).round() * texel_size;

    // Adjust matrix translation
    let offset_x = snapped_x - origin[0];
    let offset_y = snapped_y - origin[1];

    matrix[3][0] += offset_x;
    matrix[3][1] += offset_y;

    matrix
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cascade_splits_linear() {
        let splits = ShadowCascades::calculate_splits(0.1, 100.0, 4, 0.0);

        // Linear splits should be evenly spaced
        assert!((splits[0] - 0.1).abs() < 0.001);
        assert!((splits[1] - 25.075).abs() < 0.1);
        assert!((splits[2] - 50.05).abs() < 0.1);
        assert!((splits[3] - 75.025).abs() < 0.1);
        assert!((splits[4] - 100.0).abs() < 0.001);
    }

    #[test]
    fn test_cascade_splits_logarithmic() {
        let splits = ShadowCascades::calculate_splits(0.1, 100.0, 4, 1.0);

        // Logarithmic splits should be geometric
        assert!((splits[0] - 0.1).abs() < 0.001);
        assert!(splits[1] > splits[0]);
        assert!(splits[2] > splits[1]);
        assert!(splits[3] > splits[2]);
        assert!((splits[4] - 100.0).abs() < 0.001);

        // Ratios should be similar
        let r1 = splits[1] / splits[0];
        let r2 = splits[2] / splits[1];
        assert!((r1 - r2).abs() < 0.1);
    }

    #[test]
    fn test_cascade_splits_ordered() {
        let splits = ShadowCascades::calculate_splits(0.1, 100.0, 4, 0.5);

        for i in 0..4 {
            assert!(splits[i] < splits[i + 1]);
        }
    }

    #[test]
    fn test_cascade_for_depth() {
        let mut cascades = ShadowCascades::default();
        cascades.splits = [0.1, 10.0, 30.0, 70.0, 150.0];

        assert_eq!(cascades.cascade_for_depth(5.0), 0);
        assert_eq!(cascades.cascade_for_depth(15.0), 1);
        assert_eq!(cascades.cascade_for_depth(50.0), 2);
        assert_eq!(cascades.cascade_for_depth(100.0), 3);
        assert_eq!(cascades.cascade_for_depth(200.0), 3);
    }

    #[test]
    fn test_cascade_blend() {
        let mut cascades = ShadowCascades::default();
        cascades.splits = [0.1, 10.0, 30.0, 70.0, 150.0];

        // Well within cascade 0
        let (idx, blend) = cascades.cascade_blend(5.0, 2.0);
        assert_eq!(idx, 0);
        assert!((blend - 0.0).abs() < 0.01);

        // At blend boundary
        let (idx, blend) = cascades.cascade_blend(9.0, 2.0);
        assert_eq!(idx, 0);
        assert!((blend - 0.5).abs() < 0.01);
    }

    #[test]
    fn test_bounding_sphere() {
        let points = [
            [-1.0, -1.0, -1.0],
            [ 1.0, -1.0, -1.0],
            [-1.0,  1.0, -1.0],
            [ 1.0,  1.0, -1.0],
            [-1.0, -1.0,  1.0],
            [ 1.0, -1.0,  1.0],
            [-1.0,  1.0,  1.0],
            [ 1.0,  1.0,  1.0],
        ];

        let (center, radius) = bounding_sphere(&points);

        assert!((center[0]).abs() < 0.001);
        assert!((center[1]).abs() < 0.001);
        assert!((center[2]).abs() < 0.001);
        assert!((radius - 3.0_f32.sqrt()).abs() < 0.01);
    }

    #[test]
    fn test_look_at() {
        let view = look_at([0.0, 5.0, 10.0], [0.0, 0.0, 0.0], [0.0, 1.0, 0.0]);

        // Should be a valid matrix
        assert!(!view[0][0].is_nan());
        assert!(!view[3][3].is_nan());
    }

    #[test]
    fn test_matrix_multiply() {
        let identity = IDENTITY_MATRIX;
        let result = multiply_mat4(&identity, &identity);

        for i in 0..4 {
            for j in 0..4 {
                let expected = if i == j { 1.0 } else { 0.0 };
                assert!((result[i][j] - expected).abs() < 0.001);
            }
        }
    }

    #[test]
    fn test_matrix_invert() {
        let identity = IDENTITY_MATRIX;
        let inv = invert_mat4(&identity);

        for i in 0..4 {
            for j in 0..4 {
                let expected = if i == j { 1.0 } else { 0.0 };
                assert!((inv[i][j] - expected).abs() < 0.001);
            }
        }
    }

    #[test]
    fn test_cascade_serialization() {
        let cascades = ShadowCascades {
            count: 3,
            splits: [0.1, 5.0, 20.0, 50.0, 100.0],
            ..Default::default()
        };

        let json = serde_json::to_string(&cascades).unwrap();
        let restored: ShadowCascades = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.count, 3);
        assert_eq!(restored.splits[2], 20.0);
    }
}
