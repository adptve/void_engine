//! GPU Instancing Data Structures
//!
//! Provides per-instance data for GPU-based instanced rendering:
//! - Model matrix for transformation
//! - Normal matrix for lighting
//! - Color tint for per-instance coloring
//! - Custom data for material overrides
//!
//! # Hot-Swap Support
//!
//! All structures support serde serialization for hot-reload.
//! GPU buffers are recreated after deserialization.

use alloc::vec::Vec;
use serde::{Serialize, Deserialize};

/// Per-instance data uploaded to GPU
///
/// This structure is uploaded to a vertex buffer with instance step mode.
/// Each field maps to vertex attributes at specific shader locations.
#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct InstanceData {
    /// Model matrix (4x4 column-major)
    /// Shader locations: 10, 11, 12, 13
    pub model_matrix: [[f32; 4]; 4],

    /// Inverse transpose of upper-left 3x3 for normal transformation
    /// Stored as 3 vec4s for GPU alignment
    /// Shader locations: 14, 15, 16
    pub normal_matrix: [[f32; 4]; 3],

    /// Per-instance color tint (RGBA)
    /// Shader location: 17
    pub color_tint: [f32; 4],

    /// Custom data (material override index, flags, animation state, etc.)
    /// Shader location: 18
    pub custom: [f32; 4],
}

impl Default for InstanceData {
    fn default() -> Self {
        Self {
            model_matrix: IDENTITY_MATRIX,
            normal_matrix: IDENTITY_NORMAL_MATRIX,
            color_tint: [1.0, 1.0, 1.0, 1.0],
            custom: [0.0; 4],
        }
    }
}

impl InstanceData {
    /// Size of InstanceData in bytes
    pub const SIZE: usize = core::mem::size_of::<Self>();

    /// Create instance data from a model matrix and color
    pub fn new(model_matrix: [[f32; 4]; 4], color_tint: [f32; 4]) -> Self {
        let normal_matrix = compute_normal_matrix(&model_matrix);
        Self {
            model_matrix,
            normal_matrix,
            color_tint,
            custom: [0.0; 4],
        }
    }

    /// Create instance data with custom data
    pub fn with_custom(
        model_matrix: [[f32; 4]; 4],
        color_tint: [f32; 4],
        custom: [f32; 4],
    ) -> Self {
        let normal_matrix = compute_normal_matrix(&model_matrix);
        Self {
            model_matrix,
            normal_matrix,
            color_tint,
            custom,
        }
    }

    /// Create from position only (identity rotation/scale)
    pub fn from_position(x: f32, y: f32, z: f32) -> Self {
        let mut matrix = IDENTITY_MATRIX;
        matrix[3][0] = x;
        matrix[3][1] = y;
        matrix[3][2] = z;
        Self::new(matrix, [1.0, 1.0, 1.0, 1.0])
    }

    /// Create from position and uniform scale
    pub fn from_position_scale(x: f32, y: f32, z: f32, scale: f32) -> Self {
        let matrix = [
            [scale, 0.0, 0.0, 0.0],
            [0.0, scale, 0.0, 0.0],
            [0.0, 0.0, scale, 0.0],
            [x, y, z, 1.0],
        ];
        Self::new(matrix, [1.0, 1.0, 1.0, 1.0])
    }

    /// Set material override index in custom data
    pub fn with_material_index(mut self, index: u32) -> Self {
        self.custom[0] = index as f32;
        self
    }

    /// Set flags in custom data
    pub fn with_flags(mut self, flags: u32) -> Self {
        self.custom[1] = f32::from_bits(flags);
        self
    }
}

/// Compute the normal matrix (inverse transpose of upper-left 3x3)
///
/// For uniform scale, this is just the upper-left 3x3.
/// For non-uniform scale, we need the full inverse transpose.
pub fn compute_normal_matrix(model: &[[f32; 4]; 4]) -> [[f32; 4]; 3] {
    // Extract 3x3 submatrix
    let m00 = model[0][0];
    let m01 = model[0][1];
    let m02 = model[0][2];
    let m10 = model[1][0];
    let m11 = model[1][1];
    let m12 = model[1][2];
    let m20 = model[2][0];
    let m21 = model[2][1];
    let m22 = model[2][2];

    // Calculate determinant of 3x3
    let det = m00 * (m11 * m22 - m12 * m21)
            - m01 * (m10 * m22 - m12 * m20)
            + m02 * (m10 * m21 - m11 * m20);

    if det.abs() < 1e-10 {
        // Singular matrix, return identity
        return IDENTITY_NORMAL_MATRIX;
    }

    let inv_det = 1.0 / det;

    // Calculate inverse of 3x3 (cofactor matrix transposed, divided by det)
    // Then transpose again for inverse transpose = cofactor / det
    let n00 = (m11 * m22 - m12 * m21) * inv_det;
    let n01 = (m02 * m21 - m01 * m22) * inv_det;
    let n02 = (m01 * m12 - m02 * m11) * inv_det;
    let n10 = (m12 * m20 - m10 * m22) * inv_det;
    let n11 = (m00 * m22 - m02 * m20) * inv_det;
    let n12 = (m02 * m10 - m00 * m12) * inv_det;
    let n20 = (m10 * m21 - m11 * m20) * inv_det;
    let n21 = (m01 * m20 - m00 * m21) * inv_det;
    let n22 = (m00 * m11 - m01 * m10) * inv_det;

    // Return as 3 vec4s (transpose for column-major GPU layout)
    [
        [n00, n10, n20, 0.0],
        [n01, n11, n21, 0.0],
        [n02, n12, n22, 0.0],
    ]
}

/// Identity 4x4 matrix
const IDENTITY_MATRIX: [[f32; 4]; 4] = [
    [1.0, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.0, 0.0],
    [0.0, 0.0, 1.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
];

/// Identity normal matrix (3x3 as 3 vec4s)
const IDENTITY_NORMAL_MATRIX: [[f32; 4]; 3] = [
    [1.0, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.0, 0.0],
    [0.0, 0.0, 1.0, 0.0],
];

/// Batch of instances for a single mesh/material combination
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct InstanceBatch {
    /// Instance data for GPU upload
    pub instances: Vec<InstanceData>,

    /// Entity IDs corresponding to each instance (for picking)
    pub entity_ids: Vec<u64>,
}

impl InstanceBatch {
    /// Create a new empty batch
    pub fn new() -> Self {
        Self {
            instances: Vec::new(),
            entity_ids: Vec::new(),
        }
    }

    /// Create a batch with pre-allocated capacity
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            instances: Vec::with_capacity(capacity),
            entity_ids: Vec::with_capacity(capacity),
        }
    }

    /// Add an instance to the batch
    pub fn push(&mut self, entity_id: u64, instance: InstanceData) {
        self.instances.push(instance);
        self.entity_ids.push(entity_id);
    }

    /// Clear the batch for reuse
    pub fn clear(&mut self) {
        self.instances.clear();
        self.entity_ids.clear();
    }

    /// Get instance count
    pub fn len(&self) -> usize {
        self.instances.len()
    }

    /// Check if batch is empty
    pub fn is_empty(&self) -> bool {
        self.instances.is_empty()
    }

    /// Get entity ID at instance index
    pub fn entity_at(&self, index: usize) -> Option<u64> {
        self.entity_ids.get(index).copied()
    }

    /// Get instance data as bytes for GPU upload
    pub fn as_bytes(&self) -> &[u8] {
        bytemuck::cast_slice(&self.instances)
    }
}

/// Key for batching instances by mesh and material
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
pub struct BatchKey {
    /// Mesh asset ID
    pub mesh_id: u64,
    /// Material ID (0 for default material)
    pub material_id: u64,
    /// Render layer mask
    pub layer_mask: u32,
}

impl BatchKey {
    /// Create a batch key
    pub fn new(mesh_id: u64, material_id: u64) -> Self {
        Self {
            mesh_id,
            material_id,
            layer_mask: 1,
        }
    }

    /// Create a batch key with layer mask
    pub fn with_layer(mesh_id: u64, material_id: u64, layer_mask: u32) -> Self {
        Self {
            mesh_id,
            material_id,
            layer_mask,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_instance_data_size() {
        // Verify GPU-friendly size (must be 16-byte aligned)
        assert_eq!(InstanceData::SIZE % 16, 0);
        // 4x4 matrix (64) + 3x4 normal (48) + color (16) + custom (16) = 144
        assert_eq!(InstanceData::SIZE, 144);
    }

    #[test]
    fn test_instance_data_default() {
        let instance = InstanceData::default();
        assert_eq!(instance.color_tint, [1.0, 1.0, 1.0, 1.0]);
        assert_eq!(instance.custom, [0.0, 0.0, 0.0, 0.0]);
    }

    #[test]
    fn test_instance_from_position() {
        let instance = InstanceData::from_position(1.0, 2.0, 3.0);
        assert_eq!(instance.model_matrix[3][0], 1.0);
        assert_eq!(instance.model_matrix[3][1], 2.0);
        assert_eq!(instance.model_matrix[3][2], 3.0);
    }

    #[test]
    fn test_normal_matrix_identity() {
        let normal = compute_normal_matrix(&IDENTITY_MATRIX);
        assert_eq!(normal[0][0], 1.0);
        assert_eq!(normal[1][1], 1.0);
        assert_eq!(normal[2][2], 1.0);
    }

    #[test]
    fn test_normal_matrix_scale() {
        let scaled = [
            [2.0, 0.0, 0.0, 0.0],
            [0.0, 2.0, 0.0, 0.0],
            [0.0, 0.0, 2.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ];
        let normal = compute_normal_matrix(&scaled);
        // Inverse of 2 is 0.5
        assert!((normal[0][0] - 0.5).abs() < 0.001);
        assert!((normal[1][1] - 0.5).abs() < 0.001);
        assert!((normal[2][2] - 0.5).abs() < 0.001);
    }

    #[test]
    fn test_instance_batch() {
        let mut batch = InstanceBatch::with_capacity(10);

        batch.push(1, InstanceData::from_position(0.0, 0.0, 0.0));
        batch.push(2, InstanceData::from_position(1.0, 0.0, 0.0));
        batch.push(3, InstanceData::from_position(2.0, 0.0, 0.0));

        assert_eq!(batch.len(), 3);
        assert_eq!(batch.entity_at(0), Some(1));
        assert_eq!(batch.entity_at(1), Some(2));
        assert_eq!(batch.entity_at(2), Some(3));
    }

    #[test]
    fn test_instance_data_serialization() {
        let instance = InstanceData::from_position(1.0, 2.0, 3.0);
        let json = serde_json::to_string(&instance).unwrap();
        let restored: InstanceData = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.model_matrix[3][0], 1.0);
        assert_eq!(restored.model_matrix[3][1], 2.0);
        assert_eq!(restored.model_matrix[3][2], 3.0);
    }

    #[test]
    fn test_batch_key() {
        let key1 = BatchKey::new(1, 0);
        let key2 = BatchKey::new(1, 0);
        let key3 = BatchKey::new(1, 1);

        assert_eq!(key1, key2);
        assert_ne!(key1, key3);
    }
}
