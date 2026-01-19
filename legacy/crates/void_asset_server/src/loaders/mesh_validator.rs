//! Mesh validation utilities
//!
//! Validates mesh data for correctness before GPU upload:
//! - Index bounds checking
//! - Degenerate triangle detection
//! - Normal validation
//! - UV range checking
//! - Topology verification

use super::mesh::{MeshAsset, MeshPrimitive, Vertex, PrimitiveTopology};

/// Validation error types
#[derive(Clone, Debug)]
pub enum MeshValidationError {
    /// Index references non-existent vertex
    IndexOutOfBounds {
        primitive_index: u32,
        index_offset: usize,
        index_value: u32,
        vertex_count: usize,
    },
    /// Degenerate triangle (zero area)
    DegenerateTriangle {
        primitive_index: u32,
        triangle_index: usize,
        indices: [u32; 3],
    },
    /// Normal vector is not normalized or zero
    InvalidNormal {
        primitive_index: u32,
        vertex_index: usize,
        normal: [f32; 3],
    },
    /// UV coordinates outside valid range
    InvalidUV {
        primitive_index: u32,
        vertex_index: usize,
        uv: [f32; 2],
    },
    /// Vertex contains NaN or infinity
    InvalidVertex {
        primitive_index: u32,
        vertex_index: usize,
        reason: String,
    },
    /// Empty mesh
    EmptyMesh,
    /// Empty primitive
    EmptyPrimitive {
        primitive_index: u32,
    },
    /// Wrong index count for topology
    InvalidIndexCount {
        primitive_index: u32,
        topology: PrimitiveTopology,
        index_count: usize,
    },
}

/// Validation options
#[derive(Clone, Debug)]
pub struct ValidationOptions {
    /// Check for degenerate triangles
    pub check_degenerate_triangles: bool,
    /// Check normal vectors
    pub check_normals: bool,
    /// Check UV coordinates (warn if outside 0-1)
    pub check_uv_range: bool,
    /// Minimum area for valid triangle
    pub min_triangle_area: f32,
    /// Maximum allowed UV value
    pub max_uv_value: f32,
}

impl Default for ValidationOptions {
    fn default() -> Self {
        Self {
            check_degenerate_triangles: true,
            check_normals: true,
            check_uv_range: false, // UVs can be outside 0-1 for tiling
            min_triangle_area: 1e-10,
            max_uv_value: 100.0,
        }
    }
}

/// Mesh validation result
#[derive(Clone, Debug, Default)]
pub struct ValidationResult {
    /// Errors found
    pub errors: Vec<MeshValidationError>,
    /// Warnings (non-fatal issues)
    pub warnings: Vec<String>,
    /// Statistics
    pub stats: MeshStats,
}

impl ValidationResult {
    /// Check if validation passed (no errors)
    pub fn is_valid(&self) -> bool {
        self.errors.is_empty()
    }
}

/// Mesh statistics
#[derive(Clone, Debug, Default)]
pub struct MeshStats {
    /// Total vertex count
    pub vertex_count: usize,
    /// Total index count
    pub index_count: usize,
    /// Total primitive count
    pub primitive_count: usize,
    /// Total triangle count
    pub triangle_count: usize,
    /// Bounding box dimensions
    pub bounds_size: [f32; 3],
}

/// Mesh validator
pub struct MeshValidator {
    options: ValidationOptions,
}

impl MeshValidator {
    /// Create a new validator with default options
    pub fn new() -> Self {
        Self {
            options: ValidationOptions::default(),
        }
    }

    /// Create a validator with custom options
    pub fn with_options(options: ValidationOptions) -> Self {
        Self { options }
    }

    /// Validate a mesh asset
    pub fn validate(&self, mesh: &MeshAsset) -> ValidationResult {
        let mut result = ValidationResult::default();

        // Check for empty mesh
        if mesh.primitives.is_empty() {
            result.errors.push(MeshValidationError::EmptyMesh);
            return result;
        }

        // Validate each primitive
        for (prim_idx, primitive) in mesh.primitives.iter().enumerate() {
            self.validate_primitive(prim_idx as u32, primitive, &mut result);
        }

        // Calculate statistics
        result.stats = self.calculate_stats(mesh);

        result
    }

    /// Validate a single primitive
    fn validate_primitive(
        &self,
        prim_idx: u32,
        primitive: &MeshPrimitive,
        result: &mut ValidationResult,
    ) {
        // Check for empty primitive
        if primitive.vertices.is_empty() {
            result.errors.push(MeshValidationError::EmptyPrimitive {
                primitive_index: prim_idx,
            });
            return;
        }

        let vertex_count = primitive.vertices.len();

        // Validate indices if present
        if let Some(ref indices) = primitive.indices {
            // Check index count for topology
            match primitive.topology {
                PrimitiveTopology::TriangleList => {
                    if indices.len() % 3 != 0 {
                        result.errors.push(MeshValidationError::InvalidIndexCount {
                            primitive_index: prim_idx,
                            topology: primitive.topology,
                            index_count: indices.len(),
                        });
                    }
                }
                PrimitiveTopology::LineList => {
                    if indices.len() % 2 != 0 {
                        result.errors.push(MeshValidationError::InvalidIndexCount {
                            primitive_index: prim_idx,
                            topology: primitive.topology,
                            index_count: indices.len(),
                        });
                    }
                }
                _ => {}
            }

            // Check index bounds
            for (idx_offset, &index) in indices.iter().enumerate() {
                if index as usize >= vertex_count {
                    result.errors.push(MeshValidationError::IndexOutOfBounds {
                        primitive_index: prim_idx,
                        index_offset: idx_offset,
                        index_value: index,
                        vertex_count,
                    });
                }
            }

            // Check for degenerate triangles
            if self.options.check_degenerate_triangles
                && primitive.topology == PrimitiveTopology::TriangleList
            {
                for (tri_idx, tri) in indices.chunks(3).enumerate() {
                    if tri.len() != 3 {
                        continue;
                    }

                    let indices = [tri[0], tri[1], tri[2]];

                    // Skip if any index is out of bounds (already reported)
                    if indices.iter().any(|&i| i as usize >= vertex_count) {
                        continue;
                    }

                    let v0 = primitive.vertices[indices[0] as usize].position;
                    let v1 = primitive.vertices[indices[1] as usize].position;
                    let v2 = primitive.vertices[indices[2] as usize].position;

                    if is_degenerate_triangle(v0, v1, v2, self.options.min_triangle_area) {
                        result.errors.push(MeshValidationError::DegenerateTriangle {
                            primitive_index: prim_idx,
                            triangle_index: tri_idx,
                            indices,
                        });
                    }
                }
            }
        }

        // Validate vertices
        for (vert_idx, vertex) in primitive.vertices.iter().enumerate() {
            self.validate_vertex(prim_idx, vert_idx, vertex, result);
        }
    }

    /// Validate a single vertex
    fn validate_vertex(
        &self,
        prim_idx: u32,
        vert_idx: usize,
        vertex: &Vertex,
        result: &mut ValidationResult,
    ) {
        // Check for NaN/infinity in position
        if vertex.position.iter().any(|&v| !v.is_finite()) {
            result.errors.push(MeshValidationError::InvalidVertex {
                primitive_index: prim_idx,
                vertex_index: vert_idx,
                reason: "Position contains NaN or infinity".to_string(),
            });
        }

        // Check normal
        if self.options.check_normals {
            let len_sq = vertex.normal.iter().map(|&n| n * n).sum::<f32>();

            if len_sq < 0.9 || len_sq > 1.1 {
                // Allow some tolerance for floating point errors
                if len_sq < 0.01 {
                    result.errors.push(MeshValidationError::InvalidNormal {
                        primitive_index: prim_idx,
                        vertex_index: vert_idx,
                        normal: vertex.normal,
                    });
                } else {
                    result.warnings.push(format!(
                        "Primitive {} vertex {}: Normal not normalized (length^2 = {})",
                        prim_idx, vert_idx, len_sq
                    ));
                }
            }
        }

        // Check UV range
        if self.options.check_uv_range {
            let max_uv = self.options.max_uv_value;
            if vertex.uv0.iter().any(|&u| u.abs() > max_uv) {
                result.errors.push(MeshValidationError::InvalidUV {
                    primitive_index: prim_idx,
                    vertex_index: vert_idx,
                    uv: vertex.uv0,
                });
            }
        }
    }

    /// Calculate mesh statistics
    fn calculate_stats(&self, mesh: &MeshAsset) -> MeshStats {
        let mut stats = MeshStats {
            primitive_count: mesh.primitives.len(),
            bounds_size: mesh.bounds.size(),
            ..Default::default()
        };

        for primitive in &mesh.primitives {
            stats.vertex_count += primitive.vertices.len();

            if let Some(ref indices) = primitive.indices {
                stats.index_count += indices.len();

                if primitive.topology == PrimitiveTopology::TriangleList {
                    stats.triangle_count += indices.len() / 3;
                }
            } else {
                // Non-indexed
                if primitive.topology == PrimitiveTopology::TriangleList {
                    stats.triangle_count += primitive.vertices.len() / 3;
                }
            }
        }

        stats
    }
}

impl Default for MeshValidator {
    fn default() -> Self {
        Self::new()
    }
}

/// Check if a triangle is degenerate (zero or near-zero area)
fn is_degenerate_triangle(
    v0: [f32; 3],
    v1: [f32; 3],
    v2: [f32; 3],
    min_area: f32,
) -> bool {
    // Calculate edges
    let e1 = [v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]];
    let e2 = [v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]];

    // Cross product
    let cross = [
        e1[1] * e2[2] - e1[2] * e2[1],
        e1[2] * e2[0] - e1[0] * e2[2],
        e1[0] * e2[1] - e1[1] * e2[0],
    ];

    // Area = 0.5 * |cross|
    let area_sq = cross.iter().map(|&c| c * c).sum::<f32>() * 0.25;

    area_sq < min_area * min_area
}

/// Compute tangents for a mesh (MikkTSpace-like algorithm)
pub fn compute_tangents(mesh: &mut MeshAsset) {
    for primitive in &mut mesh.primitives {
        if primitive.topology != PrimitiveTopology::TriangleList {
            continue;
        }

        let indices = match &primitive.indices {
            Some(indices) => indices.clone(),
            None => (0..primitive.vertices.len() as u32).collect(),
        };

        let vertex_count = primitive.vertices.len();
        let mut tangents: Vec<[f32; 3]> = vec![[0.0; 3]; vertex_count];
        let mut bitangents: Vec<[f32; 3]> = vec![[0.0; 3]; vertex_count];

        // Accumulate tangent/bitangent per vertex
        for tri in indices.chunks(3) {
            if tri.len() != 3 {
                continue;
            }

            let i0 = tri[0] as usize;
            let i1 = tri[1] as usize;
            let i2 = tri[2] as usize;

            if i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count {
                continue;
            }

            let v0 = &primitive.vertices[i0];
            let v1 = &primitive.vertices[i1];
            let v2 = &primitive.vertices[i2];

            let edge1 = [
                v1.position[0] - v0.position[0],
                v1.position[1] - v0.position[1],
                v1.position[2] - v0.position[2],
            ];
            let edge2 = [
                v2.position[0] - v0.position[0],
                v2.position[1] - v0.position[1],
                v2.position[2] - v0.position[2],
            ];

            let duv1 = [v1.uv0[0] - v0.uv0[0], v1.uv0[1] - v0.uv0[1]];
            let duv2 = [v2.uv0[0] - v0.uv0[0], v2.uv0[1] - v0.uv0[1]];

            let r = duv1[0] * duv2[1] - duv1[1] * duv2[0];
            if r.abs() < 1e-10 {
                continue;
            }
            let r = 1.0 / r;

            let tangent = [
                (duv2[1] * edge1[0] - duv1[1] * edge2[0]) * r,
                (duv2[1] * edge1[1] - duv1[1] * edge2[1]) * r,
                (duv2[1] * edge1[2] - duv1[1] * edge2[2]) * r,
            ];
            let bitangent = [
                (-duv2[0] * edge1[0] + duv1[0] * edge2[0]) * r,
                (-duv2[0] * edge1[1] + duv1[0] * edge2[1]) * r,
                (-duv2[0] * edge1[2] + duv1[0] * edge2[2]) * r,
            ];

            for i in [i0, i1, i2] {
                for j in 0..3 {
                    tangents[i][j] += tangent[j];
                    bitangents[i][j] += bitangent[j];
                }
            }
        }

        // Orthonormalize and write back
        for (i, v) in primitive.vertices.iter_mut().enumerate() {
            let n = v.normal;
            let mut t = tangents[i];

            // Gram-Schmidt orthonormalize
            let dot = t[0] * n[0] + t[1] * n[1] + t[2] * n[2];
            t[0] -= n[0] * dot;
            t[1] -= n[1] * dot;
            t[2] -= n[2] * dot;

            // Normalize
            let len = (t[0] * t[0] + t[1] * t[1] + t[2] * t[2]).sqrt();
            if len > 1e-6 {
                t[0] /= len;
                t[1] /= len;
                t[2] /= len;
            } else {
                t = [1.0, 0.0, 0.0];
            }

            // Calculate handedness
            let b = bitangents[i];
            let cross = [
                n[1] * t[2] - n[2] * t[1],
                n[2] * t[0] - n[0] * t[2],
                n[0] * t[1] - n[1] * t[0],
            ];
            let handedness = if cross[0] * b[0] + cross[1] * b[1] + cross[2] * b[2] < 0.0 {
                -1.0
            } else {
                1.0
            };

            v.tangent = [t[0], t[1], t[2], handedness];
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::loaders::mesh::MeshLoader;

    #[test]
    fn test_validate_cube() {
        let mesh = MeshLoader::cube();
        let validator = MeshValidator::new();
        let result = validator.validate(&mesh);

        assert!(result.is_valid(), "Cube mesh should be valid: {:?}", result.errors);
        assert_eq!(result.stats.primitive_count, 1);
        assert_eq!(result.stats.triangle_count, 12);
    }

    #[test]
    fn test_validate_sphere() {
        let mesh = MeshLoader::sphere(16, 8);
        // UV spheres have degenerate triangles at poles - this is expected
        // Use lenient validation options
        let options = ValidationOptions {
            check_degenerate_triangles: false, // Poles have degenerate tris
            ..Default::default()
        };
        let validator = MeshValidator::with_options(options);
        let result = validator.validate(&mesh);

        assert!(result.is_valid(), "Sphere mesh should be valid: {:?}", result.errors);
        assert!(result.stats.triangle_count > 0, "Sphere should have triangles");
    }

    #[test]
    fn test_validate_empty_mesh() {
        let mesh = MeshAsset::default();
        let validator = MeshValidator::new();
        let result = validator.validate(&mesh);

        assert!(!result.is_valid());
        assert!(matches!(result.errors[0], MeshValidationError::EmptyMesh));
    }
}
