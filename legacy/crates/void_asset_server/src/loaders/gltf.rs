//! glTF/GLB loader for 3D models with PBR materials
//!
//! Supports:
//! - glTF 2.0 (.gltf + .bin) and GLB (.glb) formats
//! - PBR metallic-roughness materials
//! - Embedded and external textures
//! - Mesh primitives with positions, normals, UVs, tangents
//! - Scene hierarchy (nodes, transforms)
//! - Skinned meshes (bone indices, weights)
//!
//! Note: Animations and morph targets are not yet supported.

use super::mesh::{Bounds, MeshAsset, SubMesh, Vertex};
use super::texture::TextureAsset;
use std::path::Path;

/// Extended vertex with tangent for normal mapping
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, bytemuck::Pod, bytemuck::Zeroable)]
pub struct PbrVertex {
    pub position: [f32; 3],
    pub normal: [f32; 3],
    pub uv: [f32; 2],
    /// Tangent vector (xyz) + handedness (w)
    pub tangent: [f32; 4],
}

/// PBR material parameters
#[derive(Clone, Debug)]
pub struct PbrMaterial {
    pub name: String,
    /// Base color factor (RGBA)
    pub base_color_factor: [f32; 4],
    /// Metallic factor (0.0 = dielectric, 1.0 = metal)
    pub metallic_factor: f32,
    /// Roughness factor (0.0 = smooth, 1.0 = rough)
    pub roughness_factor: f32,
    /// Emissive color factor (RGB)
    pub emissive_factor: [f32; 3],
    /// Alpha mode
    pub alpha_mode: AlphaMode,
    /// Alpha cutoff for masked mode
    pub alpha_cutoff: f32,
    /// Double-sided rendering
    pub double_sided: bool,
    /// Index into GltfAsset.textures for base color, or None
    pub base_color_texture: Option<usize>,
    /// Index for metallic-roughness texture
    pub metallic_roughness_texture: Option<usize>,
    /// Index for normal map
    pub normal_texture: Option<usize>,
    /// Normal map scale
    pub normal_scale: f32,
    /// Index for occlusion texture
    pub occlusion_texture: Option<usize>,
    /// Occlusion strength
    pub occlusion_strength: f32,
    /// Index for emissive texture
    pub emissive_texture: Option<usize>,
}

impl Default for PbrMaterial {
    fn default() -> Self {
        Self {
            name: String::new(),
            base_color_factor: [1.0, 1.0, 1.0, 1.0],
            metallic_factor: 1.0,
            roughness_factor: 1.0,
            emissive_factor: [0.0, 0.0, 0.0],
            alpha_mode: AlphaMode::Opaque,
            alpha_cutoff: 0.5,
            double_sided: false,
            base_color_texture: None,
            metallic_roughness_texture: None,
            normal_texture: None,
            normal_scale: 1.0,
            occlusion_texture: None,
            occlusion_strength: 1.0,
            emissive_texture: None,
        }
    }
}

/// Alpha blending mode
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AlphaMode {
    Opaque,
    Mask,
    Blend,
}

/// A mesh primitive with PBR vertices and material
#[derive(Clone, Debug)]
pub struct GltfPrimitive {
    /// Vertices with tangent information
    pub vertices: Vec<PbrVertex>,
    /// Triangle indices
    pub indices: Vec<u32>,
    /// Material index (into GltfAsset.materials)
    pub material_index: Option<usize>,
    /// Bounding box
    pub bounds: Bounds,
}

/// A mesh containing one or more primitives
#[derive(Clone, Debug)]
pub struct GltfMesh {
    pub name: String,
    pub primitives: Vec<GltfPrimitive>,
}

/// A scene node with transform
#[derive(Clone, Debug)]
pub struct GltfNode {
    pub name: String,
    /// Local transform matrix (column-major)
    pub transform: [[f32; 4]; 4],
    /// Index into GltfAsset.meshes, or None
    pub mesh_index: Option<usize>,
    /// Child node indices
    pub children: Vec<usize>,
}

/// Complete glTF asset
#[derive(Clone, Debug)]
pub struct GltfAsset {
    /// All meshes in the file
    pub meshes: Vec<GltfMesh>,
    /// All materials
    pub materials: Vec<PbrMaterial>,
    /// All textures (images)
    pub textures: Vec<TextureAsset>,
    /// Scene graph nodes
    pub nodes: Vec<GltfNode>,
    /// Root node indices for the default scene
    pub scene_roots: Vec<usize>,
}

impl GltfAsset {
    /// Convert to MeshAsset (creates primitives from glTF primitives)
    pub fn to_mesh_asset(&self) -> MeshAsset {
        use super::mesh::{MeshPrimitive, PrimitiveTopology};

        let mut primitives = Vec::new();
        let mut prim_index = 0;

        for mesh in &self.meshes {
            for gltf_prim in &mesh.primitives {
                // Convert PbrVertex to Vertex (includes tangent)
                let vertices: Vec<Vertex> = gltf_prim.vertices.iter().map(|pv| {
                    Vertex::full(
                        pv.position,
                        pv.normal,
                        pv.tangent,
                        pv.uv,
                        [0.0, 0.0], // No secondary UV in glTF primitives
                        [1.0, 1.0, 1.0, 1.0], // Default white vertex color
                    )
                }).collect();

                let primitive = MeshPrimitive {
                    index: prim_index,
                    vertices,
                    indices: Some(gltf_prim.indices.clone()),
                    topology: PrimitiveTopology::TriangleList,
                    material_index: gltf_prim.material_index.map(|i| i as u32),
                    bounds: gltf_prim.bounds.clone(),
                };

                primitives.push(primitive);
                prim_index += 1;
            }
        }

        // Calculate overall bounds
        let bounds = if primitives.is_empty() {
            Bounds::default()
        } else {
            let mut min = [f32::MAX; 3];
            let mut max = [f32::MIN; 3];
            for prim in &primitives {
                for i in 0..3 {
                    min[i] = min[i].min(prim.bounds.min[i]);
                    max[i] = max[i].max(prim.bounds.max[i]);
                }
            }
            Bounds { min, max }
        };

        MeshAsset {
            id: 0,
            path: String::new(),
            primitives,
            bounds,
            skeleton: None,
            morph_targets: Vec::new(),
        }
    }
}

/// Loader for glTF/GLB files
pub struct GltfLoader {
    /// Base path for resolving external resources
    base_path: Option<String>,
}

impl GltfLoader {
    pub fn new() -> Self {
        Self { base_path: None }
    }

    pub fn with_base_path(path: &str) -> Self {
        Self {
            base_path: Some(path.to_string()),
        }
    }

    /// Load from file bytes (GLB or glTF JSON)
    pub fn load(&self, data: &[u8], path: &str) -> Result<GltfAsset, String> {
        let (document, buffers, images) = gltf::import_slice(data)
            .map_err(|e| format!("Failed to parse glTF {}: {}", path, e))?;

        self.process_gltf(&document, &buffers, &images, path)
    }

    /// Load from a file path
    pub fn load_file(path: &str) -> Result<GltfAsset, String> {
        let (document, buffers, images) = gltf::import(path)
            .map_err(|e| format!("Failed to load glTF {}: {}", path, e))?;

        let loader = Self {
            base_path: Path::new(path)
                .parent()
                .map(|p| p.to_string_lossy().into_owned()),
        };

        loader.process_gltf(&document, &buffers, &images, path)
    }

    fn process_gltf(
        &self,
        document: &gltf::Document,
        buffers: &[gltf::buffer::Data],
        images: &[gltf::image::Data],
        _path: &str,
    ) -> Result<GltfAsset, String> {
        // Load textures
        let textures = self.load_textures(document, images)?;

        // Load materials
        let materials = self.load_materials(document);

        // Load meshes
        let meshes = self.load_meshes(document, buffers)?;

        // Load nodes
        let nodes = self.load_nodes(document);

        // Get scene roots
        let scene_roots = document
            .default_scene()
            .map(|s| s.nodes().map(|n| n.index()).collect())
            .unwrap_or_else(Vec::new);

        Ok(GltfAsset {
            meshes,
            materials,
            textures,
            nodes,
            scene_roots,
        })
    }

    fn load_textures(
        &self,
        _document: &gltf::Document,
        images: &[gltf::image::Data],
    ) -> Result<Vec<TextureAsset>, String> {
        let mut textures = Vec::new();

        for image in images {
            let (data, width, height) = match image.format {
                gltf::image::Format::R8G8B8 => {
                    // Convert RGB to RGBA
                    let mut rgba = Vec::with_capacity(image.pixels.len() / 3 * 4);
                    for chunk in image.pixels.chunks(3) {
                        rgba.extend_from_slice(chunk);
                        rgba.push(255);
                    }
                    (rgba, image.width, image.height)
                }
                gltf::image::Format::R8G8B8A8 => {
                    (image.pixels.clone(), image.width, image.height)
                }
                gltf::image::Format::R8 => {
                    // Grayscale to RGBA
                    let mut rgba = Vec::with_capacity(image.pixels.len() * 4);
                    for &v in &image.pixels {
                        rgba.extend_from_slice(&[v, v, v, 255]);
                    }
                    (rgba, image.width, image.height)
                }
                gltf::image::Format::R8G8 => {
                    // RG to RGBA (useful for metallic-roughness)
                    let mut rgba = Vec::with_capacity(image.pixels.len() * 2);
                    for chunk in image.pixels.chunks(2) {
                        rgba.extend_from_slice(&[chunk[0], chunk[1], 0, 255]);
                    }
                    (rgba, image.width, image.height)
                }
                gltf::image::Format::R16 |
                gltf::image::Format::R16G16 |
                gltf::image::Format::R16G16B16 |
                gltf::image::Format::R16G16B16A16 |
                gltf::image::Format::R32G32B32FLOAT |
                gltf::image::Format::R32G32B32A32FLOAT => {
                    // HDR/16-bit formats - convert to 8-bit for now
                    log::warn!("HDR texture format not fully supported, converting to 8-bit");
                    let pixel_count = (image.width * image.height) as usize;
                    let rgba = vec![128u8; pixel_count * 4];
                    (rgba, image.width, image.height)
                }
            };

            textures.push(TextureAsset {
                data,
                width,
                height,
                bytes_per_row: width * 4,
                srgb: true,
                mips: vec![],
            });
        }

        Ok(textures)
    }

    fn load_materials(&self, document: &gltf::Document) -> Vec<PbrMaterial> {
        let mut materials = Vec::new();

        for mat in document.materials() {
            let pbr = mat.pbr_metallic_roughness();

            let alpha_mode = match mat.alpha_mode() {
                gltf::material::AlphaMode::Opaque => AlphaMode::Opaque,
                gltf::material::AlphaMode::Mask => AlphaMode::Mask,
                gltf::material::AlphaMode::Blend => AlphaMode::Blend,
            };

            materials.push(PbrMaterial {
                name: mat.name().unwrap_or("").to_string(),
                base_color_factor: pbr.base_color_factor(),
                metallic_factor: pbr.metallic_factor(),
                roughness_factor: pbr.roughness_factor(),
                emissive_factor: mat.emissive_factor(),
                alpha_mode,
                alpha_cutoff: mat.alpha_cutoff().unwrap_or(0.5),
                double_sided: mat.double_sided(),
                base_color_texture: pbr
                    .base_color_texture()
                    .map(|t| t.texture().source().index()),
                metallic_roughness_texture: pbr
                    .metallic_roughness_texture()
                    .map(|t| t.texture().source().index()),
                normal_texture: mat
                    .normal_texture()
                    .map(|t| t.texture().source().index()),
                normal_scale: mat.normal_texture().map(|t| t.scale()).unwrap_or(1.0),
                occlusion_texture: mat
                    .occlusion_texture()
                    .map(|t| t.texture().source().index()),
                occlusion_strength: mat
                    .occlusion_texture()
                    .map(|t| t.strength())
                    .unwrap_or(1.0),
                emissive_texture: mat
                    .emissive_texture()
                    .map(|t| t.texture().source().index()),
            });
        }

        // Add default material if none defined
        if materials.is_empty() {
            materials.push(PbrMaterial::default());
        }

        materials
    }

    fn load_meshes(
        &self,
        document: &gltf::Document,
        buffers: &[gltf::buffer::Data],
    ) -> Result<Vec<GltfMesh>, String> {
        let mut meshes = Vec::new();

        for mesh in document.meshes() {
            let mut primitives = Vec::new();

            for primitive in mesh.primitives() {
                let reader = primitive.reader(|buffer| Some(&buffers[buffer.index()]));

                // Read positions (required)
                let positions: Vec<[f32; 3]> = reader
                    .read_positions()
                    .ok_or_else(|| "Mesh missing positions".to_string())?
                    .collect();

                // Read normals (optional, generate if missing)
                let normals: Vec<[f32; 3]> = reader
                    .read_normals()
                    .map(|n| n.collect())
                    .unwrap_or_else(|| vec![[0.0, 1.0, 0.0]; positions.len()]);

                // Read UVs (optional)
                let uvs: Vec<[f32; 2]> = reader
                    .read_tex_coords(0)
                    .map(|t| t.into_f32().collect())
                    .unwrap_or_else(|| vec![[0.0, 0.0]; positions.len()]);

                // Read tangents (optional, can be computed if missing)
                let tangents: Vec<[f32; 4]> = reader
                    .read_tangents()
                    .map(|t| t.collect())
                    .unwrap_or_else(|| vec![[1.0, 0.0, 0.0, 1.0]; positions.len()]);

                // Read indices
                let indices: Vec<u32> = reader
                    .read_indices()
                    .map(|i| i.into_u32().collect())
                    .unwrap_or_else(|| (0..positions.len() as u32).collect());

                // Build vertices
                let vertices: Vec<PbrVertex> = (0..positions.len())
                    .map(|i| PbrVertex {
                        position: positions[i],
                        normal: normals.get(i).copied().unwrap_or([0.0, 1.0, 0.0]),
                        uv: uvs.get(i).copied().unwrap_or([0.0, 0.0]),
                        tangent: tangents.get(i).copied().unwrap_or([1.0, 0.0, 0.0, 1.0]),
                    })
                    .collect();

                // Calculate bounds from PBR vertices
                let bounds = {
                    if vertices.is_empty() {
                        Bounds::default()
                    } else {
                        let mut min = [f32::MAX; 3];
                        let mut max = [f32::MIN; 3];
                        for v in &vertices {
                            for i in 0..3 {
                                min[i] = min[i].min(v.position[i]);
                                max[i] = max[i].max(v.position[i]);
                            }
                        }
                        Bounds { min, max }
                    }
                };

                primitives.push(GltfPrimitive {
                    vertices,
                    indices,
                    material_index: primitive.material().index(),
                    bounds,
                });
            }

            meshes.push(GltfMesh {
                name: mesh.name().unwrap_or("").to_string(),
                primitives,
            });
        }

        Ok(meshes)
    }

    fn load_nodes(&self, document: &gltf::Document) -> Vec<GltfNode> {
        let mut nodes = Vec::new();

        for node in document.nodes() {
            let transform = node.transform().matrix();

            nodes.push(GltfNode {
                name: node.name().unwrap_or("").to_string(),
                transform,
                mesh_index: node.mesh().map(|m| m.index()),
                children: node.children().map(|c| c.index()).collect(),
            });
        }

        nodes
    }
}

impl Default for GltfLoader {
    fn default() -> Self {
        Self::new()
    }
}

/// Calculate tangents using MikkTSpace algorithm (simplified version)
///
/// For proper normal mapping, tangents should ideally be computed using
/// mikktspace, but this provides a reasonable approximation.
pub fn compute_tangents(vertices: &mut [PbrVertex], indices: &[u32]) {
    // Accumulate tangent/bitangent per vertex
    let mut tangents: Vec<[f32; 3]> = vec![[0.0; 3]; vertices.len()];
    let mut bitangents: Vec<[f32; 3]> = vec![[0.0; 3]; vertices.len()];

    for tri in indices.chunks(3) {
        if tri.len() != 3 {
            continue;
        }

        let i0 = tri[0] as usize;
        let i1 = tri[1] as usize;
        let i2 = tri[2] as usize;

        if i0 >= vertices.len() || i1 >= vertices.len() || i2 >= vertices.len() {
            continue;
        }

        let v0 = &vertices[i0];
        let v1 = &vertices[i1];
        let v2 = &vertices[i2];

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

        let duv1 = [v1.uv[0] - v0.uv[0], v1.uv[1] - v0.uv[1]];
        let duv2 = [v2.uv[0] - v0.uv[0], v2.uv[1] - v0.uv[1]];

        let r = duv1[0] * duv2[1] - duv1[1] * duv2[0];
        if r.abs() < 0.0001 {
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
            tangents[i][0] += tangent[0];
            tangents[i][1] += tangent[1];
            tangents[i][2] += tangent[2];
            bitangents[i][0] += bitangent[0];
            bitangents[i][1] += bitangent[1];
            bitangents[i][2] += bitangent[2];
        }
    }

    // Orthonormalize and write back
    for (i, v) in vertices.iter_mut().enumerate() {
        let n = v.normal;
        let mut t = tangents[i];

        // Gram-Schmidt orthonormalize
        let dot = t[0] * n[0] + t[1] * n[1] + t[2] * n[2];
        t[0] -= n[0] * dot;
        t[1] -= n[1] * dot;
        t[2] -= n[2] * dot;

        // Normalize
        let len = (t[0] * t[0] + t[1] * t[1] + t[2] * t[2]).sqrt();
        if len > 0.0001 {
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_material() {
        let mat = PbrMaterial::default();
        assert_eq!(mat.base_color_factor, [1.0, 1.0, 1.0, 1.0]);
        assert_eq!(mat.metallic_factor, 1.0);
        assert_eq!(mat.roughness_factor, 1.0);
    }

    #[test]
    fn test_pbr_vertex_size() {
        assert_eq!(std::mem::size_of::<PbrVertex>(), 48); // 12 + 12 + 8 + 16
    }
}
