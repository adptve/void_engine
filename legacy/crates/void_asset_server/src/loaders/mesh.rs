//! Mesh loader for OBJ files
//!
//! Supports loading mesh data from OBJ files and provides primitive generators.

use serde::{Serialize, Deserialize};

/// Standard vertex format for all meshes
///
/// This vertex format supports all standard rendering attributes:
/// - Position: World-space vertex position
/// - Normal: Surface normal for lighting
/// - Tangent: Tangent vector with handedness for normal mapping
/// - UV0: Primary texture coordinates
/// - UV1: Secondary texture coordinates (lightmaps, detail)
/// - Color: Vertex color (RGBA)
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, bytemuck::Pod, bytemuck::Zeroable)]
pub struct Vertex {
    /// World-space position
    pub position: [f32; 3],
    /// Surface normal (normalized)
    pub normal: [f32; 3],
    /// Tangent with handedness in w component
    pub tangent: [f32; 4],
    /// Primary UV coordinates
    pub uv0: [f32; 2],
    /// Secondary UV coordinates (lightmaps, detail)
    pub uv1: [f32; 2],
    /// Vertex color (RGBA)
    pub color: [f32; 4],
}

impl Vertex {
    /// Create a vertex with just position and normal (other fields default)
    pub fn new(position: [f32; 3], normal: [f32; 3], uv: [f32; 2]) -> Self {
        Self {
            position,
            normal,
            tangent: [1.0, 0.0, 0.0, 1.0],
            uv0: uv,
            uv1: [0.0, 0.0],
            color: [1.0, 1.0, 1.0, 1.0],
        }
    }

    /// Create a vertex with all attributes
    pub fn full(
        position: [f32; 3],
        normal: [f32; 3],
        tangent: [f32; 4],
        uv0: [f32; 2],
        uv1: [f32; 2],
        color: [f32; 4],
    ) -> Self {
        Self { position, normal, tangent, uv0, uv1, color }
    }
}

/// Skinned vertex with bone weights for skeletal animation
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, bytemuck::Pod, bytemuck::Zeroable)]
pub struct SkinnedVertex {
    /// Base vertex data
    pub position: [f32; 3],
    pub normal: [f32; 3],
    pub tangent: [f32; 4],
    pub uv0: [f32; 2],
    pub uv1: [f32; 2],
    pub color: [f32; 4],
    /// Bone indices (up to 4 bones per vertex)
    pub joints: [u32; 4],
    /// Bone weights (must sum to 1.0)
    pub weights: [f32; 4],
}

/// Primitive topology for rendering
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum PrimitiveTopology {
    PointList,
    LineList,
    LineStrip,
    #[default]
    TriangleList,
    TriangleStrip,
}

/// A single drawable primitive within a mesh
#[derive(Clone, Debug)]
pub struct MeshPrimitive {
    /// Primitive index within the mesh
    pub index: u32,
    /// Vertex data
    pub vertices: Vec<Vertex>,
    /// Index data (None = non-indexed)
    pub indices: Option<Vec<u32>>,
    /// Primitive topology
    pub topology: PrimitiveTopology,
    /// Material index (references scene materials)
    pub material_index: Option<u32>,
    /// Bounding box for this primitive
    pub bounds: Bounds,
}

impl Default for MeshPrimitive {
    fn default() -> Self {
        Self {
            index: 0,
            vertices: Vec::new(),
            indices: None,
            topology: PrimitiveTopology::TriangleList,
            material_index: None,
            bounds: Bounds::default(),
        }
    }
}

/// Skeleton for skinned meshes
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct Skeleton {
    /// Joint names
    pub joint_names: Vec<String>,
    /// Inverse bind matrices (one per joint)
    pub inverse_bind_matrices: Vec<[[f32; 4]; 4]>,
    /// Parent indices (-1 for roots)
    pub parent_indices: Vec<i32>,
}

/// Morph target (blend shape)
#[derive(Clone, Debug)]
pub struct MorphTarget {
    /// Target name
    pub name: String,
    /// Position deltas
    pub position_deltas: Vec<[f32; 3]>,
    /// Normal deltas (optional)
    pub normal_deltas: Option<Vec<[f32; 3]>>,
    /// Tangent deltas (optional)
    pub tangent_deltas: Option<Vec<[f32; 3]>>,
}

/// Mesh data ready for GPU upload
#[derive(Clone, Debug)]
pub struct MeshAsset {
    /// Unique asset identifier
    pub id: u64,
    /// Asset path for reloading
    pub path: String,
    /// Mesh primitives (sub-meshes)
    pub primitives: Vec<MeshPrimitive>,
    /// Axis-aligned bounding box
    pub bounds: Bounds,
    /// Optional skeleton for skinned meshes
    pub skeleton: Option<Skeleton>,
    /// Optional morph targets
    pub morph_targets: Vec<MorphTarget>,
}

impl Default for MeshAsset {
    fn default() -> Self {
        Self {
            id: 0,
            path: String::new(),
            primitives: Vec::new(),
            bounds: Bounds::default(),
            skeleton: None,
            morph_targets: Vec::new(),
        }
    }
}

impl MeshAsset {
    /// Create from legacy format (vertices + indices + submeshes)
    pub fn from_legacy(vertices: Vec<Vertex>, indices: Vec<u32>, submeshes: Vec<SubMesh>) -> Self {
        let bounds = Bounds::from_vertices(&vertices);

        // Convert submeshes to primitives
        let primitives = submeshes.iter().enumerate().map(|(i, sub)| {
            let start = sub.index_offset as usize;
            let end = start + sub.index_count as usize;
            let prim_indices: Vec<u32> = indices[start..end].to_vec();

            // Extract vertices used by this primitive
            let mut prim_vertices = Vec::new();
            let mut index_remap = std::collections::HashMap::new();
            let mut new_indices = Vec::new();

            for &idx in &prim_indices {
                let new_idx = *index_remap.entry(idx).or_insert_with(|| {
                    let new_idx = prim_vertices.len() as u32;
                    prim_vertices.push(vertices[idx as usize]);
                    new_idx
                });
                new_indices.push(new_idx);
            }

            let prim_bounds = Bounds::from_vertices(&prim_vertices);

            MeshPrimitive {
                index: i as u32,
                vertices: prim_vertices,
                indices: Some(new_indices),
                topology: PrimitiveTopology::TriangleList,
                material_index: Some(sub.material_index as u32),
                bounds: prim_bounds,
            }
        }).collect();

        Self {
            id: 0,
            path: String::new(),
            primitives,
            bounds,
            skeleton: None,
            morph_targets: Vec::new(),
        }
    }

    /// Get total vertex count across all primitives
    pub fn vertex_count(&self) -> usize {
        self.primitives.iter().map(|p| p.vertices.len()).sum()
    }

    /// Get total index count across all primitives
    pub fn index_count(&self) -> usize {
        self.primitives.iter()
            .filter_map(|p| p.indices.as_ref())
            .map(|i| i.len())
            .sum()
    }
}

/// Legacy mesh data (for backwards compatibility)
#[derive(Clone, Debug)]
pub struct LegacyMeshAsset {
    /// Vertex data
    pub vertices: Vec<Vertex>,
    /// Index data (triangles)
    pub indices: Vec<u32>,
    /// Sub-meshes for multi-material support
    pub submeshes: Vec<SubMesh>,
    /// Bounding box
    pub bounds: Bounds,
}

/// A sub-mesh referencing a portion of the index buffer
#[derive(Clone, Debug)]
pub struct SubMesh {
    pub index_offset: u32,
    pub index_count: u32,
    pub material_index: usize,
}

/// Axis-aligned bounding box
#[derive(Clone, Debug, Default)]
pub struct Bounds {
    pub min: [f32; 3],
    pub max: [f32; 3],
}

impl Bounds {
    /// Calculate bounds from vertices
    pub fn from_vertices(vertices: &[Vertex]) -> Self {
        if vertices.is_empty() {
            return Self::default();
        }

        let mut min = [f32::MAX; 3];
        let mut max = [f32::MIN; 3];

        for v in vertices {
            for i in 0..3 {
                min[i] = min[i].min(v.position[i]);
                max[i] = max[i].max(v.position[i]);
            }
        }

        Self { min, max }
    }

    /// Get the center of the bounding box
    pub fn center(&self) -> [f32; 3] {
        [
            (self.min[0] + self.max[0]) * 0.5,
            (self.min[1] + self.max[1]) * 0.5,
            (self.min[2] + self.max[2]) * 0.5,
        ]
    }

    /// Get the size of the bounding box
    pub fn size(&self) -> [f32; 3] {
        [
            self.max[0] - self.min[0],
            self.max[1] - self.min[1],
            self.max[2] - self.min[2],
        ]
    }
}

/// Loader for mesh files
pub struct MeshLoader;

impl MeshLoader {
    /// Load a mesh from OBJ file bytes
    pub fn load(data: &[u8], path: &str) -> Result<MeshAsset, String> {
        let text = std::str::from_utf8(data)
            .map_err(|e| format!("Invalid UTF-8 in mesh {}: {}", path, e))?;

        Self::load_obj(text, path)
    }

    /// Parse OBJ format
    fn load_obj(text: &str, _path: &str) -> Result<MeshAsset, String> {
        let mut positions: Vec<[f32; 3]> = Vec::new();
        let mut normals: Vec<[f32; 3]> = Vec::new();
        let mut uvs: Vec<[f32; 2]> = Vec::new();
        let mut vertices: Vec<Vertex> = Vec::new();
        let mut indices: Vec<u32> = Vec::new();

        for line in text.lines() {
            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.is_empty() {
                continue;
            }

            match parts[0] {
                "v" if parts.len() >= 4 => {
                    positions.push([
                        parts[1].parse().unwrap_or(0.0),
                        parts[2].parse().unwrap_or(0.0),
                        parts[3].parse().unwrap_or(0.0),
                    ]);
                }
                "vn" if parts.len() >= 4 => {
                    normals.push([
                        parts[1].parse().unwrap_or(0.0),
                        parts[2].parse().unwrap_or(1.0),
                        parts[3].parse().unwrap_or(0.0),
                    ]);
                }
                "vt" if parts.len() >= 2 => {
                    uvs.push([
                        parts[1].parse().unwrap_or(0.0),
                        1.0 - parts.get(2).and_then(|s| s.parse().ok()).unwrap_or(0.0),
                    ]);
                }
                "f" if parts.len() >= 4 => {
                    // Parse face - triangulate if more than 3 vertices
                    let face_verts: Vec<Vertex> = parts[1..]
                        .iter()
                        .filter_map(|p| Self::parse_vertex_index(p, &positions, &normals, &uvs))
                        .collect();

                    // Triangulate (fan triangulation)
                    if face_verts.len() >= 3 {
                        for i in 1..face_verts.len() - 1 {
                            let base = vertices.len() as u32;
                            vertices.push(face_verts[0]);
                            vertices.push(face_verts[i]);
                            vertices.push(face_verts[i + 1]);
                            indices.push(base);
                            indices.push(base + 1);
                            indices.push(base + 2);
                        }
                    }
                }
                _ => {}
            }
        }

        // If no normals were provided, generate flat normals
        if normals.is_empty() {
            Self::generate_flat_normals(&mut vertices, &indices);
        }

        let bounds = Bounds::from_vertices(&vertices);
        let index_count = indices.len() as u32;

        // Create single primitive from the mesh
        let primitive = MeshPrimitive {
            index: 0,
            vertices,
            indices: Some(indices),
            topology: PrimitiveTopology::TriangleList,
            material_index: Some(0),
            bounds: bounds.clone(),
        };

        Ok(MeshAsset {
            id: 0,
            path: String::new(),
            primitives: vec![primitive],
            bounds,
            skeleton: None,
            morph_targets: Vec::new(),
        })
    }

    /// Parse a vertex index like "1/2/3" or "1//3" or "1"
    fn parse_vertex_index(
        s: &str,
        positions: &[[f32; 3]],
        normals: &[[f32; 3]],
        uvs: &[[f32; 2]],
    ) -> Option<Vertex> {
        let parts: Vec<&str> = s.split('/').collect();

        let pos_idx: usize = parts.first()?.parse::<i32>().ok()?.abs() as usize;
        let position = *positions.get(pos_idx.saturating_sub(1))?;

        let uv = if parts.len() > 1 && !parts[1].is_empty() {
            let uv_idx: usize = parts[1].parse::<i32>().ok()?.abs() as usize;
            uvs.get(uv_idx.saturating_sub(1)).copied().unwrap_or([0.0, 0.0])
        } else {
            [0.0, 0.0]
        };

        let normal = if parts.len() > 2 && !parts[2].is_empty() {
            let n_idx: usize = parts[2].parse::<i32>().ok()?.abs() as usize;
            normals.get(n_idx.saturating_sub(1)).copied().unwrap_or([0.0, 1.0, 0.0])
        } else {
            [0.0, 1.0, 0.0]
        };

        Some(Vertex::new(position, normal, uv))
    }

    /// Generate flat normals for a mesh without normals
    fn generate_flat_normals(vertices: &mut [Vertex], indices: &[u32]) {
        for chunk in indices.chunks(3) {
            if chunk.len() != 3 {
                continue;
            }

            let i0 = chunk[0] as usize;
            let i1 = chunk[1] as usize;
            let i2 = chunk[2] as usize;

            if i0 >= vertices.len() || i1 >= vertices.len() || i2 >= vertices.len() {
                continue;
            }

            let p0 = vertices[i0].position;
            let p1 = vertices[i1].position;
            let p2 = vertices[i2].position;

            // Calculate face normal
            let e1 = [p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]];
            let e2 = [p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]];

            let normal = [
                e1[1] * e2[2] - e1[2] * e2[1],
                e1[2] * e2[0] - e1[0] * e2[2],
                e1[0] * e2[1] - e1[1] * e2[0],
            ];

            // Normalize
            let len = (normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]).sqrt();
            let normal = if len > 0.0001 {
                [normal[0] / len, normal[1] / len, normal[2] / len]
            } else {
                [0.0, 1.0, 0.0]
            };

            vertices[i0].normal = normal;
            vertices[i1].normal = normal;
            vertices[i2].normal = normal;
        }
    }

    // =========================================================================
    // Primitive generators
    // =========================================================================

    /// Generate a cube mesh
    pub fn cube() -> MeshAsset {
        let vertices = vec![
            // Front face
            Vertex::new([-0.5, -0.5,  0.5], [0.0, 0.0, 1.0], [0.0, 1.0]),
            Vertex::new([ 0.5, -0.5,  0.5], [0.0, 0.0, 1.0], [1.0, 1.0]),
            Vertex::new([ 0.5,  0.5,  0.5], [0.0, 0.0, 1.0], [1.0, 0.0]),
            Vertex::new([-0.5,  0.5,  0.5], [0.0, 0.0, 1.0], [0.0, 0.0]),
            // Back face
            Vertex::new([-0.5, -0.5, -0.5], [0.0, 0.0, -1.0], [1.0, 1.0]),
            Vertex::new([-0.5,  0.5, -0.5], [0.0, 0.0, -1.0], [1.0, 0.0]),
            Vertex::new([ 0.5,  0.5, -0.5], [0.0, 0.0, -1.0], [0.0, 0.0]),
            Vertex::new([ 0.5, -0.5, -0.5], [0.0, 0.0, -1.0], [0.0, 1.0]),
            // Top face
            Vertex::new([-0.5,  0.5, -0.5], [0.0, 1.0, 0.0], [0.0, 1.0]),
            Vertex::new([-0.5,  0.5,  0.5], [0.0, 1.0, 0.0], [0.0, 0.0]),
            Vertex::new([ 0.5,  0.5,  0.5], [0.0, 1.0, 0.0], [1.0, 0.0]),
            Vertex::new([ 0.5,  0.5, -0.5], [0.0, 1.0, 0.0], [1.0, 1.0]),
            // Bottom face
            Vertex::new([-0.5, -0.5, -0.5], [0.0, -1.0, 0.0], [0.0, 0.0]),
            Vertex::new([ 0.5, -0.5, -0.5], [0.0, -1.0, 0.0], [1.0, 0.0]),
            Vertex::new([ 0.5, -0.5,  0.5], [0.0, -1.0, 0.0], [1.0, 1.0]),
            Vertex::new([-0.5, -0.5,  0.5], [0.0, -1.0, 0.0], [0.0, 1.0]),
            // Right face
            Vertex::new([ 0.5, -0.5, -0.5], [1.0, 0.0, 0.0], [1.0, 1.0]),
            Vertex::new([ 0.5,  0.5, -0.5], [1.0, 0.0, 0.0], [1.0, 0.0]),
            Vertex::new([ 0.5,  0.5,  0.5], [1.0, 0.0, 0.0], [0.0, 0.0]),
            Vertex::new([ 0.5, -0.5,  0.5], [1.0, 0.0, 0.0], [0.0, 1.0]),
            // Left face
            Vertex::new([-0.5, -0.5, -0.5], [-1.0, 0.0, 0.0], [0.0, 1.0]),
            Vertex::new([-0.5, -0.5,  0.5], [-1.0, 0.0, 0.0], [1.0, 1.0]),
            Vertex::new([-0.5,  0.5,  0.5], [-1.0, 0.0, 0.0], [1.0, 0.0]),
            Vertex::new([-0.5,  0.5, -0.5], [-1.0, 0.0, 0.0], [0.0, 0.0]),
        ];

        let indices = vec![
            0,  1,  2,  0,  2,  3,  // front
            4,  5,  6,  4,  6,  7,  // back
            8,  9,  10, 8,  10, 11, // top
            12, 13, 14, 12, 14, 15, // bottom
            16, 17, 18, 16, 18, 19, // right
            20, 21, 22, 20, 22, 23, // left
        ];

        let bounds = Bounds::from_vertices(&vertices);

        Self::create_mesh_asset(vertices, indices, bounds)
    }

    /// Helper to create MeshAsset from vertices and indices
    fn create_mesh_asset(vertices: Vec<Vertex>, indices: Vec<u32>, bounds: Bounds) -> MeshAsset {
        let index_count = indices.len() as u32;
        let primitive = MeshPrimitive {
            index: 0,
            vertices,
            indices: Some(indices),
            topology: PrimitiveTopology::TriangleList,
            material_index: Some(0),
            bounds: bounds.clone(),
        };

        MeshAsset {
            id: 0,
            path: String::new(),
            primitives: vec![primitive],
            bounds,
            skeleton: None,
            morph_targets: Vec::new(),
        }
    }

    /// Generate a UV sphere mesh
    pub fn sphere(segments: u32, rings: u32) -> MeshAsset {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();

        let segments = segments.max(3);
        let rings = rings.max(2);

        // Generate vertices
        for ring in 0..=rings {
            let v = ring as f32 / rings as f32;
            let phi = v * std::f32::consts::PI;

            for seg in 0..=segments {
                let u = seg as f32 / segments as f32;
                let theta = u * 2.0 * std::f32::consts::PI;

                let x = phi.sin() * theta.cos();
                let y = phi.cos();
                let z = phi.sin() * theta.sin();

                vertices.push(Vertex::new(
                    [x * 0.5, y * 0.5, z * 0.5],
                    [x, y, z],
                    [u, v],
                ));
            }
        }

        // Generate indices
        for ring in 0..rings {
            for seg in 0..segments {
                let curr_ring = ring * (segments + 1);
                let next_ring = (ring + 1) * (segments + 1);

                indices.push(curr_ring + seg);
                indices.push(next_ring + seg);
                indices.push(curr_ring + seg + 1);

                indices.push(curr_ring + seg + 1);
                indices.push(next_ring + seg);
                indices.push(next_ring + seg + 1);
            }
        }

        let bounds = Bounds::from_vertices(&vertices);
        Self::create_mesh_asset(vertices, indices, bounds)
    }

    /// Generate a cylinder mesh
    pub fn cylinder(segments: u32) -> MeshAsset {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();
        let segments = segments.max(3);

        // Side vertices
        for i in 0..=segments {
            let theta = i as f32 / segments as f32 * 2.0 * std::f32::consts::PI;
            let x = theta.cos();
            let z = theta.sin();
            let u = i as f32 / segments as f32;

            // Bottom
            vertices.push(Vertex::new([x * 0.5, -0.5, z * 0.5], [x, 0.0, z], [u, 1.0]));
            // Top
            vertices.push(Vertex::new([x * 0.5, 0.5, z * 0.5], [x, 0.0, z], [u, 0.0]));
        }

        // Side indices
        for i in 0..segments {
            let base = i * 2;
            indices.push(base);
            indices.push(base + 1);
            indices.push(base + 2);

            indices.push(base + 2);
            indices.push(base + 1);
            indices.push(base + 3);
        }

        // Top cap center
        let top_center = vertices.len() as u32;
        vertices.push(Vertex::new([0.0, 0.5, 0.0], [0.0, 1.0, 0.0], [0.5, 0.5]));

        // Top cap vertices
        for i in 0..=segments {
            let theta = i as f32 / segments as f32 * 2.0 * std::f32::consts::PI;
            let x = theta.cos();
            let z = theta.sin();
            vertices.push(Vertex::new(
                [x * 0.5, 0.5, z * 0.5],
                [0.0, 1.0, 0.0],
                [x * 0.5 + 0.5, z * 0.5 + 0.5],
            ));
        }

        // Top cap indices
        let top_start = top_center + 1;
        for i in 0..segments {
            indices.push(top_center);
            indices.push(top_start + i);
            indices.push(top_start + i + 1);
        }

        // Bottom cap center
        let bottom_center = vertices.len() as u32;
        vertices.push(Vertex::new([0.0, -0.5, 0.0], [0.0, -1.0, 0.0], [0.5, 0.5]));

        // Bottom cap vertices
        for i in 0..=segments {
            let theta = i as f32 / segments as f32 * 2.0 * std::f32::consts::PI;
            let x = theta.cos();
            let z = theta.sin();
            vertices.push(Vertex::new(
                [x * 0.5, -0.5, z * 0.5],
                [0.0, -1.0, 0.0],
                [x * 0.5 + 0.5, z * 0.5 + 0.5],
            ));
        }

        // Bottom cap indices (reversed winding)
        let bottom_start = bottom_center + 1;
        for i in 0..segments {
            indices.push(bottom_center);
            indices.push(bottom_start + i + 1);
            indices.push(bottom_start + i);
        }

        let bounds = Bounds::from_vertices(&vertices);
        Self::create_mesh_asset(vertices, indices, bounds)
    }

    /// Generate a diamond (octahedron) mesh - distinctive shape for plugins
    pub fn diamond() -> MeshAsset {
        // Octahedron: 6 vertices (top, bottom, 4 around middle)
        let top = [0.0, 0.7, 0.0];
        let bottom = [0.0, -0.7, 0.0];
        let front = [0.0, 0.0, 0.5];
        let back = [0.0, 0.0, -0.5];
        let left = [-0.5, 0.0, 0.0];
        let right = [0.5, 0.0, 0.0];

        // Helper to compute face normal
        fn face_normal(a: [f32; 3], b: [f32; 3], c: [f32; 3]) -> [f32; 3] {
            let e1 = [b[0] - a[0], b[1] - a[1], b[2] - a[2]];
            let e2 = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
            let n = [
                e1[1] * e2[2] - e1[2] * e2[1],
                e1[2] * e2[0] - e1[0] * e2[2],
                e1[0] * e2[1] - e1[1] * e2[0],
            ];
            let len = (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]).sqrt();
            if len > 0.0001 {
                [n[0] / len, n[1] / len, n[2] / len]
            } else {
                [0.0, 1.0, 0.0]
            }
        }

        // 8 triangular faces (4 top, 4 bottom)
        // Top faces
        let n_tf = face_normal(top, front, right);
        let n_tr = face_normal(top, right, back);
        let n_tb = face_normal(top, back, left);
        let n_tl = face_normal(top, left, front);
        // Bottom faces
        let n_bf = face_normal(bottom, right, front);
        let n_br = face_normal(bottom, back, right);
        let n_bb = face_normal(bottom, left, back);
        let n_bl = face_normal(bottom, front, left);

        let vertices = vec![
            // Top-front-right face
            Vertex::new(top, n_tf, [0.5, 0.0]),
            Vertex::new(front, n_tf, [0.0, 1.0]),
            Vertex::new(right, n_tf, [1.0, 1.0]),
            // Top-right-back face
            Vertex::new(top, n_tr, [0.5, 0.0]),
            Vertex::new(right, n_tr, [0.0, 1.0]),
            Vertex::new(back, n_tr, [1.0, 1.0]),
            // Top-back-left face
            Vertex::new(top, n_tb, [0.5, 0.0]),
            Vertex::new(back, n_tb, [0.0, 1.0]),
            Vertex::new(left, n_tb, [1.0, 1.0]),
            // Top-left-front face
            Vertex::new(top, n_tl, [0.5, 0.0]),
            Vertex::new(left, n_tl, [0.0, 1.0]),
            Vertex::new(front, n_tl, [1.0, 1.0]),
            // Bottom-front-right face (reversed winding)
            Vertex::new(bottom, n_bf, [0.5, 1.0]),
            Vertex::new(right, n_bf, [1.0, 0.0]),
            Vertex::new(front, n_bf, [0.0, 0.0]),
            // Bottom-right-back face
            Vertex::new(bottom, n_br, [0.5, 1.0]),
            Vertex::new(back, n_br, [1.0, 0.0]),
            Vertex::new(right, n_br, [0.0, 0.0]),
            // Bottom-back-left face
            Vertex::new(bottom, n_bb, [0.5, 1.0]),
            Vertex::new(left, n_bb, [1.0, 0.0]),
            Vertex::new(back, n_bb, [0.0, 0.0]),
            // Bottom-left-front face
            Vertex::new(bottom, n_bl, [0.5, 1.0]),
            Vertex::new(front, n_bl, [1.0, 0.0]),
            Vertex::new(left, n_bl, [0.0, 0.0]),
        ];

        let indices: Vec<u32> = (0..24).collect();

        let bounds = Bounds::from_vertices(&vertices);
        Self::create_mesh_asset(vertices, indices, bounds)
    }

    /// Generate a torus mesh
    pub fn torus(tube_segments: u32, ring_segments: u32, tube_radius: f32) -> MeshAsset {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();

        let tube_segments = tube_segments.max(3);
        let ring_segments = ring_segments.max(3);
        let ring_radius = 0.5 - tube_radius;

        for i in 0..=ring_segments {
            let u = i as f32 / ring_segments as f32;
            let theta = u * 2.0 * std::f32::consts::PI;

            for j in 0..=tube_segments {
                let v = j as f32 / tube_segments as f32;
                let phi = v * 2.0 * std::f32::consts::PI;

                let x = (ring_radius + tube_radius * phi.cos()) * theta.cos();
                let y = tube_radius * phi.sin();
                let z = (ring_radius + tube_radius * phi.cos()) * theta.sin();

                let nx = phi.cos() * theta.cos();
                let ny = phi.sin();
                let nz = phi.cos() * theta.sin();

                vertices.push(Vertex::new([x, y, z], [nx, ny, nz], [u, v]));
            }
        }

        for i in 0..ring_segments {
            for j in 0..tube_segments {
                let curr = i * (tube_segments + 1) + j;
                let next = (i + 1) * (tube_segments + 1) + j;

                indices.push(curr);
                indices.push(next);
                indices.push(curr + 1);

                indices.push(curr + 1);
                indices.push(next);
                indices.push(next + 1);
            }
        }

        let bounds = Bounds::from_vertices(&vertices);
        Self::create_mesh_asset(vertices, indices, bounds)
    }

    /// Generate a plane mesh
    pub fn plane(subdivisions: u32) -> MeshAsset {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();

        let subdivisions = subdivisions.max(1);
        let step = 1.0 / subdivisions as f32;

        for y in 0..=subdivisions {
            for x in 0..=subdivisions {
                let u = x as f32 * step;
                let v = y as f32 * step;

                vertices.push(Vertex::new([u - 0.5, 0.0, v - 0.5], [0.0, 1.0, 0.0], [u, v]));
            }
        }

        for y in 0..subdivisions {
            for x in 0..subdivisions {
                let curr = y * (subdivisions + 1) + x;
                let next = (y + 1) * (subdivisions + 1) + x;

                indices.push(curr);
                indices.push(next);
                indices.push(curr + 1);

                indices.push(curr + 1);
                indices.push(next);
                indices.push(next + 1);
            }
        }

        let bounds = Bounds::from_vertices(&vertices);
        Self::create_mesh_asset(vertices, indices, bounds)
    }
}
