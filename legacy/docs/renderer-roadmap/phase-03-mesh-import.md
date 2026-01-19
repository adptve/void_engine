# Phase 3: Mesh Import (External Geometry)

## Status: Not Started

## User Story

> As a scene author, I want to load external mesh assets instead of being limited to primitives.

## Requirements Checklist

- [ ] Support mesh loading from files (minimum: glTF)
- [ ] Allow entities to reference mesh assets by path
- [ ] Support vertex attributes: Position, Normal, UVs, Tangents (optional)
- [ ] Allow reuse of mesh assets across entities
- [ ] Validate mesh compatibility at load time

## Current State Analysis

### Existing Implementation

**void_asset_server/src/loaders/mesh.rs:**
```rust
pub struct MeshAsset {
    pub vertices: Vec<Vertex>,
    pub indices: Vec<u32>,
    pub bounds: AABB,
}

pub struct Vertex {
    pub position: [f32; 3],
    pub normal: [f32; 3],
    pub tex_coords: [f32; 2],
}
```

**void_ecs/src/render_components.rs:**
```rust
pub struct MeshRenderer {
    pub mesh_type: MeshType,
    pub mesh_asset: Option<String>,  // Path reference
    pub cast_shadows: bool,
    pub receive_shadows: bool,
}

pub enum MeshType {
    Cube,
    Sphere,
    Cylinder,
    Plane,
    Quad,
    Custom,
}
```

### Gaps
1. No tangent support in vertex format
2. No multiple UV sets
3. No vertex colors
4. No sub-mesh support
5. No LOD mesh references
6. No skinned mesh support (skeletal animation)
7. No morph target support
8. Limited glTF feature extraction
9. No GPU buffer caching
10. No mesh instancing preparation

## Implementation Specification

### 1. Enhanced Vertex Format

```rust
// crates/void_render/src/mesh.rs (REFACTOR)

/// Standard vertex format for all meshes
#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
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
    pub const LAYOUT: wgpu::VertexBufferLayout<'static> = wgpu::VertexBufferLayout {
        array_stride: std::mem::size_of::<Vertex>() as u64,
        step_mode: wgpu::VertexStepMode::Vertex,
        attributes: &[
            // position
            wgpu::VertexAttribute {
                offset: 0,
                shader_location: 0,
                format: wgpu::VertexFormat::Float32x3,
            },
            // normal
            wgpu::VertexAttribute {
                offset: 12,
                shader_location: 1,
                format: wgpu::VertexFormat::Float32x3,
            },
            // tangent
            wgpu::VertexAttribute {
                offset: 24,
                shader_location: 2,
                format: wgpu::VertexFormat::Float32x4,
            },
            // uv0
            wgpu::VertexAttribute {
                offset: 40,
                shader_location: 3,
                format: wgpu::VertexFormat::Float32x2,
            },
            // uv1
            wgpu::VertexAttribute {
                offset: 48,
                shader_location: 4,
                format: wgpu::VertexFormat::Float32x2,
            },
            // color
            wgpu::VertexAttribute {
                offset: 56,
                shader_location: 5,
                format: wgpu::VertexFormat::Float32x4,
            },
        ],
    };
}

/// Skinned vertex with bone weights
#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
pub struct SkinnedVertex {
    /// Base vertex data
    pub vertex: Vertex,

    /// Bone indices (up to 4 bones)
    pub joints: [u32; 4],

    /// Bone weights (sum to 1.0)
    pub weights: [f32; 4],
}
```

### 2. Mesh Asset Structure

```rust
// crates/void_asset_server/src/loaders/mesh.rs (REFACTOR)

use std::sync::Arc;

/// Loaded mesh asset ready for GPU upload
#[derive(Clone, Debug)]
pub struct MeshAsset {
    /// Unique asset identifier
    pub id: AssetId,

    /// Asset path for reloading
    pub path: String,

    /// Mesh primitives (sub-meshes)
    pub primitives: Vec<MeshPrimitive>,

    /// Axis-aligned bounding box
    pub bounds: AABB,

    /// Optional skeleton for skinned meshes
    pub skeleton: Option<Skeleton>,

    /// Optional morph targets
    pub morph_targets: Vec<MorphTarget>,
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
    pub bounds: AABB,
}

#[derive(Clone, Copy, Debug, Default)]
pub enum PrimitiveTopology {
    PointList,
    LineList,
    LineStrip,
    #[default]
    TriangleList,
    TriangleStrip,
}

/// Skeleton for skinned meshes
#[derive(Clone, Debug)]
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
    pub name: String,
    pub position_deltas: Vec<[f32; 3]>,
    pub normal_deltas: Option<Vec<[f32; 3]>>,
    pub tangent_deltas: Option<Vec<[f32; 3]>>,
}
```

### 3. glTF Loader

```rust
// crates/void_asset_server/src/loaders/gltf_loader.rs (NEW FILE)

use gltf::{Document, Gltf};
use std::path::Path;

pub struct GltfLoader;

impl GltfLoader {
    /// Load a glTF file and return all meshes
    pub fn load<P: AsRef<Path>>(path: P) -> Result<GltfScene, GltfError> {
        let (document, buffers, images) = gltf::import(path.as_ref())?;

        let mut meshes = Vec::new();
        let mut materials = Vec::new();
        let mut textures = Vec::new();

        // Load textures
        for image in document.images() {
            textures.push(Self::load_texture(&image, &images)?);
        }

        // Load materials
        for material in document.materials() {
            materials.push(Self::load_material(&material, &textures)?);
        }

        // Load meshes
        for mesh in document.meshes() {
            meshes.push(Self::load_mesh(&mesh, &buffers)?);
        }

        // Load scene hierarchy
        let nodes = Self::load_nodes(&document, &meshes)?;

        Ok(GltfScene {
            meshes,
            materials,
            textures,
            nodes,
        })
    }

    fn load_mesh(mesh: &gltf::Mesh, buffers: &[gltf::buffer::Data])
        -> Result<MeshAsset, GltfError>
    {
        let mut primitives = Vec::new();
        let mut mesh_bounds = AABB::empty();

        for primitive in mesh.primitives() {
            let reader = primitive.reader(|buffer| Some(&buffers[buffer.index()]));

            // Read positions (required)
            let positions: Vec<[f32; 3]> = reader
                .read_positions()
                .ok_or(GltfError::MissingPositions)?
                .collect();

            // Read normals (generate if missing)
            let normals: Vec<[f32; 3]> = reader
                .read_normals()
                .map(|n| n.collect())
                .unwrap_or_else(|| Self::generate_normals(&positions, &indices));

            // Read tangents (generate if missing and needed)
            let tangents: Vec<[f32; 4]> = reader
                .read_tangents()
                .map(|t| t.collect())
                .unwrap_or_else(|| vec![[1.0, 0.0, 0.0, 1.0]; positions.len()]);

            // Read UVs
            let uv0: Vec<[f32; 2]> = reader
                .read_tex_coords(0)
                .map(|tc| tc.into_f32().collect())
                .unwrap_or_else(|| vec![[0.0, 0.0]; positions.len()]);

            let uv1: Vec<[f32; 2]> = reader
                .read_tex_coords(1)
                .map(|tc| tc.into_f32().collect())
                .unwrap_or_else(|| vec![[0.0, 0.0]; positions.len()]);

            // Read vertex colors
            let colors: Vec<[f32; 4]> = reader
                .read_colors(0)
                .map(|c| c.into_rgba_f32().collect())
                .unwrap_or_else(|| vec![[1.0, 1.0, 1.0, 1.0]; positions.len()]);

            // Read indices
            let indices: Option<Vec<u32>> = reader
                .read_indices()
                .map(|i| i.into_u32().collect());

            // Assemble vertices
            let vertices: Vec<Vertex> = (0..positions.len())
                .map(|i| Vertex {
                    position: positions[i],
                    normal: normals[i],
                    tangent: tangents[i],
                    uv0: uv0[i],
                    uv1: uv1[i],
                    color: colors[i],
                })
                .collect();

            // Compute bounds
            let bounds = AABB::from_points(&positions);
            mesh_bounds = mesh_bounds.union(&bounds);

            primitives.push(MeshPrimitive {
                index: primitives.len() as u32,
                vertices,
                indices,
                topology: Self::convert_topology(primitive.mode()),
                material_index: primitive.material().index().map(|i| i as u32),
                bounds,
            });
        }

        Ok(MeshAsset {
            id: AssetId::new(),
            path: String::new(),  // Set by caller
            primitives,
            bounds: mesh_bounds,
            skeleton: None,  // TODO: skeletal mesh support
            morph_targets: Vec::new(),  // TODO: morph target support
        })
    }

    fn generate_normals(positions: &[[f32; 3]], indices: &Option<Vec<u32>>)
        -> Vec<[f32; 3]>
    {
        // Generate flat normals from triangles
        let mut normals = vec![[0.0f32; 3]; positions.len()];

        let get_indices = |i: usize| -> (usize, usize, usize) {
            match indices {
                Some(idx) => (
                    idx[i * 3] as usize,
                    idx[i * 3 + 1] as usize,
                    idx[i * 3 + 2] as usize,
                ),
                None => (i * 3, i * 3 + 1, i * 3 + 2),
            }
        };

        let tri_count = indices
            .as_ref()
            .map(|i| i.len() / 3)
            .unwrap_or(positions.len() / 3);

        for i in 0..tri_count {
            let (i0, i1, i2) = get_indices(i);
            let v0 = Vec3::from(positions[i0]);
            let v1 = Vec3::from(positions[i1]);
            let v2 = Vec3::from(positions[i2]);

            let normal = (v1 - v0).cross(v2 - v0).normalize();

            normals[i0] = add_vec3(normals[i0], normal.into());
            normals[i1] = add_vec3(normals[i1], normal.into());
            normals[i2] = add_vec3(normals[i2], normal.into());
        }

        // Normalize accumulated normals
        for n in &mut normals {
            let len = (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]).sqrt();
            if len > 0.0001 {
                n[0] /= len;
                n[1] /= len;
                n[2] /= len;
            } else {
                *n = [0.0, 1.0, 0.0];  // Default up normal
            }
        }

        normals
    }
}

#[derive(Debug)]
pub enum GltfError {
    Io(std::io::Error),
    Gltf(gltf::Error),
    MissingPositions,
    InvalidPrimitive,
    UnsupportedFeature(String),
}
```

### 4. GPU Mesh Cache

```rust
// crates/void_render/src/mesh_cache.rs (NEW FILE)

use std::collections::HashMap;
use std::sync::Arc;
use wgpu::{Buffer, Device, Queue};

/// GPU mesh buffer cache for efficient rendering
pub struct MeshCache {
    /// Cached GPU buffers by asset ID
    meshes: HashMap<AssetId, GpuMesh>,

    /// Device reference for buffer creation
    device: Arc<Device>,

    /// Queue for buffer uploads
    queue: Arc<Queue>,
}

/// GPU-ready mesh data
pub struct GpuMesh {
    /// Asset ID for reference tracking
    pub asset_id: AssetId,

    /// Per-primitive GPU buffers
    pub primitives: Vec<GpuPrimitive>,

    /// Bounding box for culling
    pub bounds: AABB,

    /// Reference count for cleanup
    ref_count: u32,
}

pub struct GpuPrimitive {
    /// Vertex buffer
    pub vertex_buffer: Buffer,

    /// Index buffer (optional)
    pub index_buffer: Option<Buffer>,

    /// Number of vertices
    pub vertex_count: u32,

    /// Number of indices (0 if non-indexed)
    pub index_count: u32,

    /// Material index
    pub material_index: Option<u32>,
}

impl MeshCache {
    pub fn new(device: Arc<Device>, queue: Arc<Queue>) -> Self {
        Self {
            meshes: HashMap::new(),
            device,
            queue,
        }
    }

    /// Get or upload mesh to GPU
    pub fn get_or_upload(&mut self, asset: &MeshAsset) -> &GpuMesh {
        if !self.meshes.contains_key(&asset.id) {
            let gpu_mesh = self.upload_mesh(asset);
            self.meshes.insert(asset.id, gpu_mesh);
        }

        self.meshes.get(&asset.id).unwrap()
    }

    fn upload_mesh(&self, asset: &MeshAsset) -> GpuMesh {
        let primitives = asset.primitives.iter()
            .map(|p| self.upload_primitive(p))
            .collect();

        GpuMesh {
            asset_id: asset.id,
            primitives,
            bounds: asset.bounds,
            ref_count: 1,
        }
    }

    fn upload_primitive(&self, primitive: &MeshPrimitive) -> GpuPrimitive {
        use wgpu::util::DeviceExt;

        let vertex_buffer = self.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("Mesh Vertex Buffer"),
            contents: bytemuck::cast_slice(&primitive.vertices),
            usage: wgpu::BufferUsages::VERTEX,
        });

        let index_buffer = primitive.indices.as_ref().map(|indices| {
            self.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
                label: Some("Mesh Index Buffer"),
                contents: bytemuck::cast_slice(indices),
                usage: wgpu::BufferUsages::INDEX,
            })
        });

        GpuPrimitive {
            vertex_buffer,
            index_buffer,
            vertex_count: primitive.vertices.len() as u32,
            index_count: primitive.indices.as_ref().map(|i| i.len() as u32).unwrap_or(0),
            material_index: primitive.material_index,
        }
    }

    /// Release mesh reference (cleanup when count reaches 0)
    pub fn release(&mut self, asset_id: AssetId) {
        if let Some(mesh) = self.meshes.get_mut(&asset_id) {
            mesh.ref_count = mesh.ref_count.saturating_sub(1);
            if mesh.ref_count == 0 {
                self.meshes.remove(&asset_id);
            }
        }
    }

    /// Force cleanup of all unreferenced meshes
    pub fn cleanup(&mut self) {
        self.meshes.retain(|_, mesh| mesh.ref_count > 0);
    }
}
```

### 5. Enhanced MeshRenderer Component

```rust
// crates/void_ecs/src/render_components.rs (modifications)

/// Mesh rendering component
#[derive(Clone, Debug)]
pub struct MeshRenderer {
    /// Built-in primitive type (if not using asset)
    pub primitive: Option<PrimitiveType>,

    /// Asset path for custom meshes
    pub mesh_asset: Option<String>,

    /// Cached asset ID (set by asset system)
    pub asset_id: Option<AssetId>,

    /// Shadow casting enabled
    pub cast_shadows: bool,

    /// Shadow receiving enabled
    pub receive_shadows: bool,

    /// Render layer mask
    pub layer_mask: u32,

    /// LOD bias (0 = auto, positive = higher detail)
    pub lod_bias: f32,
}

#[derive(Clone, Copy, Debug)]
pub enum PrimitiveType {
    Cube,
    Sphere { segments: u32 },
    Cylinder { segments: u32 },
    Plane { subdivisions: u32 },
    Quad,
    Capsule { segments: u32 },
}

impl Default for MeshRenderer {
    fn default() -> Self {
        Self {
            primitive: Some(PrimitiveType::Cube),
            mesh_asset: None,
            asset_id: None,
            cast_shadows: true,
            receive_shadows: true,
            layer_mask: 1,
            lod_bias: 0.0,
        }
    }
}

impl MeshRenderer {
    pub fn from_asset(path: impl Into<String>) -> Self {
        Self {
            primitive: None,
            mesh_asset: Some(path.into()),
            asset_id: None,
            cast_shadows: true,
            receive_shadows: true,
            layer_mask: 1,
            lod_bias: 0.0,
        }
    }

    pub fn cube() -> Self {
        Self { primitive: Some(PrimitiveType::Cube), ..default() }
    }

    pub fn sphere(segments: u32) -> Self {
        Self { primitive: Some(PrimitiveType::Sphere { segments }), ..default() }
    }
}
```

### 6. Mesh Validation

```rust
// crates/void_asset_server/src/loaders/mesh_validator.rs (NEW FILE)

/// Validates mesh data for GPU compatibility
pub struct MeshValidator;

impl MeshValidator {
    pub fn validate(mesh: &MeshAsset) -> Result<(), ValidationError> {
        for (i, primitive) in mesh.primitives.iter().enumerate() {
            Self::validate_primitive(i, primitive)?;
        }
        Ok(())
    }

    fn validate_primitive(index: usize, primitive: &MeshPrimitive)
        -> Result<(), ValidationError>
    {
        // Check vertex count
        if primitive.vertices.is_empty() {
            return Err(ValidationError::EmptyPrimitive { index });
        }

        // Check for degenerate triangles
        if let Some(indices) = &primitive.indices {
            if indices.len() % 3 != 0 {
                return Err(ValidationError::InvalidIndexCount {
                    index,
                    count: indices.len()
                });
            }

            // Check index bounds
            let max_index = primitive.vertices.len() as u32;
            for (i, idx) in indices.iter().enumerate() {
                if *idx >= max_index {
                    return Err(ValidationError::IndexOutOfBounds {
                        primitive_index: index,
                        index_position: i,
                        index_value: *idx,
                        vertex_count: max_index,
                    });
                }
            }
        }

        // Check for NaN/Inf in positions
        for (i, vertex) in primitive.vertices.iter().enumerate() {
            for (j, val) in vertex.position.iter().enumerate() {
                if !val.is_finite() {
                    return Err(ValidationError::InvalidPosition {
                        primitive_index: index,
                        vertex_index: i,
                        component: j,
                    });
                }
            }
        }

        // Validate normals are normalized
        for (i, vertex) in primitive.vertices.iter().enumerate() {
            let len_sq = vertex.normal.iter().map(|n| n * n).sum::<f32>();
            if (len_sq - 1.0).abs() > 0.01 {
                // Just warn, auto-normalize
                log::warn!(
                    "Primitive {} vertex {} has unnormalized normal (len={})",
                    index, i, len_sq.sqrt()
                );
            }
        }

        Ok(())
    }
}

#[derive(Debug)]
pub enum ValidationError {
    EmptyPrimitive { index: usize },
    InvalidIndexCount { index: usize, count: usize },
    IndexOutOfBounds {
        primitive_index: usize,
        index_position: usize,
        index_value: u32,
        vertex_count: u32,
    },
    InvalidPosition {
        primitive_index: usize,
        vertex_index: usize,
        component: usize,
    },
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_render/src/mesh.rs` | CREATE | New vertex formats |
| `void_render/src/mesh_cache.rs` | CREATE | GPU buffer caching |
| `void_asset_server/src/loaders/gltf_loader.rs` | CREATE | Full glTF loading |
| `void_asset_server/src/loaders/mesh_validator.rs` | CREATE | Validation logic |
| `void_asset_server/src/loaders/mesh.rs` | MODIFY | Use new MeshAsset |
| `void_ecs/src/render_components.rs` | MODIFY | Enhanced MeshRenderer |
| `void_render/src/extraction.rs` | MODIFY | Handle mesh assets |
| `void_runtime/src/scene_renderer.rs` | MODIFY | Use mesh cache |
| `void_editor/src/panels/asset_browser.rs` | MODIFY | Mesh previews |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_gltf_cube_load() {
    let scene = GltfLoader::load("test_assets/cube.glb").unwrap();
    assert_eq!(scene.meshes.len(), 1);
    assert_eq!(scene.meshes[0].primitives.len(), 1);
    assert_eq!(scene.meshes[0].primitives[0].vertices.len(), 24);  // 6 faces * 4 verts
}

#[test]
fn test_gltf_with_uvs() {
    let scene = GltfLoader::load("test_assets/textured.glb").unwrap();
    let uv = scene.meshes[0].primitives[0].vertices[0].uv0;
    assert!(uv[0] >= 0.0 && uv[0] <= 1.0);
}

#[test]
fn test_tangent_generation() {
    let scene = GltfLoader::load("test_assets/no_tangents.glb").unwrap();
    // Should auto-generate tangents
    let tangent = scene.meshes[0].primitives[0].vertices[0].tangent;
    assert!((tangent[3].abs() - 1.0).abs() < 0.01);  // Handedness should be +-1
}

#[test]
fn test_mesh_validation() {
    let bad_mesh = MeshAsset {
        primitives: vec![MeshPrimitive {
            vertices: vec![],  // Empty!
            ..default()
        }],
        ..default()
    };

    assert!(MeshValidator::validate(&bad_mesh).is_err());
}

#[test]
fn test_mesh_cache() {
    let (device, queue) = create_test_device();
    let mut cache = MeshCache::new(device, queue);

    let asset = load_test_mesh();
    let gpu_mesh = cache.get_or_upload(&asset);

    assert_eq!(gpu_mesh.primitives.len(), asset.primitives.len());
}
```

### Integration Tests
```rust
#[test]
fn test_render_gltf_model() {
    // Full pipeline: load → cache → render
}

#[test]
fn test_hot_reload_mesh() {
    // Modify .glb file, verify reload
}
```

## Performance Considerations

1. **Async Loading**: Load meshes on background thread
2. **Buffer Reuse**: Pool vertex/index buffers
3. **Compression**: Support Draco mesh compression
4. **Streaming**: Support progressive mesh loading
5. **Memory Budget**: Track GPU memory usage

## Hot-Swap Support

### Serialization

All mesh-related components support serde for state persistence during hot-reload:

```rust
// crates/void_ecs/src/render_components.rs

use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MeshRenderer {
    pub primitive: Option<PrimitiveType>,
    pub mesh_asset: Option<String>,
    #[serde(skip)]  // Runtime-only, rebuilt from mesh_asset
    pub asset_id: Option<AssetId>,
    pub cast_shadows: bool,
    pub receive_shadows: bool,
    pub layer_mask: u32,
    pub lod_bias: f32,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum PrimitiveType {
    Cube,
    Sphere { segments: u32 },
    Cylinder { segments: u32 },
    Plane { subdivisions: u32 },
    Quad,
    Capsule { segments: u32 },
}

// MeshAsset is asset-side, uses ron/json for hot-reload manifest
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MeshAssetManifest {
    pub path: String,
    pub primitives_count: usize,
    pub bounds: AABB,
    pub has_skeleton: bool,
    pub morph_target_count: usize,
}
```

### HotReloadable Implementation

```rust
// crates/void_render/src/mesh_cache.rs

use void_core::hot_reload::{HotReloadable, ReloadContext, ReloadResult};

impl HotReloadable for MeshCache {
    fn type_name(&self) -> &'static str {
        "MeshCache"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        // Serialize mesh references (not GPU buffers)
        let state: Vec<MeshCacheEntry> = self.meshes.iter()
            .map(|(id, mesh)| MeshCacheEntry {
                asset_id: *id,
                asset_path: mesh.asset_path.clone(),
                ref_count: mesh.ref_count,
            })
            .collect();
        bincode::serialize(&state).map_err(HotReloadError::Serialization)
    }

    fn deserialize_state(&mut self, data: &[u8], ctx: &ReloadContext) -> ReloadResult {
        let entries: Vec<MeshCacheEntry> = bincode::deserialize(data)?;

        // Queue mesh re-uploads for frame boundary
        for entry in entries {
            self.pending_reloads.push(PendingMeshReload {
                asset_id: entry.asset_id,
                asset_path: entry.asset_path,
                ref_count: entry.ref_count,
            });
        }

        Ok(())
    }
}

#[derive(Serialize, Deserialize)]
struct MeshCacheEntry {
    asset_id: AssetId,
    asset_path: String,
    ref_count: u32,
}
```

### Asset Dependencies

```rust
// crates/void_asset_server/src/loaders/mesh.rs

use void_asset::AssetDependent;

impl AssetDependent for MeshRenderer {
    fn asset_paths(&self) -> Vec<&str> {
        self.mesh_asset.as_deref().into_iter().collect()
    }

    fn on_asset_changed(&mut self, path: &str, ctx: &AssetReloadContext) {
        if self.mesh_asset.as_deref() == Some(path) {
            // Invalidate cached asset ID, will be reloaded next frame
            self.asset_id = None;
            log::info!("Mesh asset invalidated for hot-reload: {}", path);
        }
    }

    fn reload_priority(&self) -> u32 {
        // Meshes should reload before materials that depend on them
        100
    }
}
```

### GPU Buffer Double-Buffering

```rust
// crates/void_render/src/mesh_cache.rs

/// GPU mesh with double-buffered vertex/index data for seamless hot-reload
pub struct GpuMesh {
    pub asset_id: AssetId,
    pub asset_path: String,

    /// Active buffer set (0 or 1)
    active_buffer: usize,

    /// Double-buffered primitives for seamless swaps
    primitives: [Vec<GpuPrimitive>; 2],

    pub bounds: AABB,
    ref_count: u32,

    /// Pending update (set during hot-reload, applied at frame boundary)
    pending_update: Option<MeshAsset>,
}

impl GpuMesh {
    /// Queue asset update for next frame boundary
    pub fn queue_update(&mut self, new_asset: MeshAsset) {
        self.pending_update = Some(new_asset);
    }

    /// Apply pending update at frame boundary (called between frames)
    pub fn apply_pending_update(&mut self, device: &Device, queue: &Queue) -> bool {
        if let Some(asset) = self.pending_update.take() {
            let inactive = 1 - self.active_buffer;

            // Upload to inactive buffer set
            self.primitives[inactive] = asset.primitives.iter()
                .map(|p| self.upload_primitive(device, p))
                .collect();

            self.bounds = asset.bounds;

            // Swap active buffer
            self.active_buffer = inactive;

            log::debug!("Applied mesh hot-reload for {}", self.asset_path);
            return true;
        }
        false
    }

    /// Get current active primitives for rendering
    pub fn primitives(&self) -> &[GpuPrimitive] {
        &self.primitives[self.active_buffer]
    }
}
```

### Frame-Boundary Updates

```rust
// crates/void_render/src/mesh_cache.rs

impl MeshCache {
    /// Process pending hot-reload updates at frame boundary
    pub fn process_pending_updates(&mut self, device: &Device, queue: &Queue) {
        // Process pending reloads from deserialization
        for reload in self.pending_reloads.drain(..) {
            if let Ok(asset) = self.asset_server.load_mesh(&reload.asset_path) {
                let gpu_mesh = self.upload_mesh_double_buffered(device, &asset);
                self.meshes.insert(reload.asset_id, gpu_mesh);
            }
        }

        // Process live asset changes
        let mut updated = Vec::new();
        for (id, mesh) in &mut self.meshes {
            if mesh.apply_pending_update(device, queue) {
                updated.push(*id);
            }
        }

        if !updated.is_empty() {
            log::info!("Hot-reloaded {} meshes at frame boundary", updated.len());
        }
    }

    /// Called by asset watcher when mesh file changes
    pub fn notify_asset_changed(&mut self, path: &str) {
        for mesh in self.meshes.values_mut() {
            if mesh.asset_path == path {
                if let Ok(asset) = self.asset_server.load_mesh(path) {
                    mesh.queue_update(asset);
                }
            }
        }
    }
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_mesh_renderer_serialization() {
        let renderer = MeshRenderer {
            primitive: None,
            mesh_asset: Some("models/cube.glb".to_string()),
            asset_id: Some(AssetId(42)),  // Should be skipped
            cast_shadows: true,
            receive_shadows: true,
            layer_mask: 1,
            lod_bias: 0.0,
        };

        let serialized = serde_json::to_string(&renderer).unwrap();
        let deserialized: MeshRenderer = serde_json::from_str(&serialized).unwrap();

        assert_eq!(deserialized.mesh_asset, Some("models/cube.glb".to_string()));
        assert!(deserialized.asset_id.is_none());  // Runtime field not serialized
    }

    #[test]
    fn test_mesh_cache_hot_reload() {
        let (device, queue) = create_test_device();
        let mut cache = MeshCache::new(device.clone(), queue.clone());

        // Load initial mesh
        let asset = create_test_mesh_asset("test.glb");
        let _mesh = cache.get_or_upload(&asset);

        // Serialize state
        let state = cache.serialize_state().unwrap();

        // Create new cache and restore
        let mut new_cache = MeshCache::new(device.clone(), queue.clone());
        new_cache.deserialize_state(&state, &ReloadContext::default()).unwrap();

        // Process pending reloads
        new_cache.process_pending_updates(&device, &queue);

        assert!(new_cache.meshes.contains_key(&asset.id));
    }

    #[test]
    fn test_gpu_mesh_double_buffer_swap() {
        let (device, queue) = create_test_device();
        let mut mesh = GpuMesh::new(&device, &create_test_mesh_asset("cube.glb"));

        let initial_active = mesh.active_buffer;

        // Queue update
        let updated_asset = create_test_mesh_asset("cube_v2.glb");
        mesh.queue_update(updated_asset);

        // Apply at frame boundary
        mesh.apply_pending_update(&device, &queue);

        assert_ne!(mesh.active_buffer, initial_active);
        assert!(mesh.pending_update.is_none());
    }

    #[test]
    fn test_asset_dependency_tracking() {
        let mut renderer = MeshRenderer::from_asset("models/character.glb");

        let paths = renderer.asset_paths();
        assert_eq!(paths, vec!["models/character.glb"]);

        // Simulate asset change
        renderer.on_asset_changed("models/character.glb", &AssetReloadContext::default());
        assert!(renderer.asset_id.is_none());
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
// crates/void_asset_server/src/loaders/gltf_loader.rs

impl GltfLoader {
    /// Load glTF with panic recovery
    pub fn load_safe<P: AsRef<Path>>(path: P) -> Result<GltfScene, GltfError> {
        let path_str = path.as_ref().to_string_lossy().to_string();

        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            Self::load(&path)
        }))
        .map_err(|panic| {
            log::error!("Panic during glTF load for {}: {:?}", path_str, panic);
            GltfError::LoadPanic(path_str)
        })?
    }
}

// crates/void_render/src/mesh_cache.rs

impl MeshCache {
    /// Get or upload mesh with fallback to error mesh
    pub fn get_or_upload_safe(&mut self, asset: &MeshAsset) -> &GpuMesh {
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.get_or_upload_internal(asset)
        }));

        match result {
            Ok(mesh) => mesh,
            Err(panic) => {
                log::error!(
                    "Panic during mesh upload for {:?}: {:?}",
                    asset.id, panic
                );
                self.get_error_mesh()
            }
        }
    }

    /// Returns a simple error-indicator mesh (magenta cube)
    fn get_error_mesh(&mut self) -> &GpuMesh {
        if !self.meshes.contains_key(&ERROR_MESH_ID) {
            let error_mesh = self.create_error_cube();
            self.meshes.insert(ERROR_MESH_ID, error_mesh);
        }
        self.meshes.get(&ERROR_MESH_ID).unwrap()
    }

    fn create_error_cube(&self) -> GpuMesh {
        // Simple magenta cube for error visualization
        let vertices = create_cube_vertices([1.0, 0.0, 1.0, 1.0]); // Magenta
        // ... upload to GPU
        GpuMesh {
            asset_id: ERROR_MESH_ID,
            asset_path: "<error>".to_string(),
            active_buffer: 0,
            primitives: [vec![error_primitive], vec![]],
            bounds: AABB::unit(),
            ref_count: u32::MAX,  // Never cleanup
            pending_update: None,
        }
    }
}
```

### Fallback Behavior

```rust
// crates/void_render/src/extraction.rs

impl SceneExtractor {
    /// Extract mesh data with graceful degradation
    pub fn extract_mesh_safe(
        &self,
        renderer: &MeshRenderer,
        mesh_cache: &mut MeshCache,
    ) -> ExtractedMesh {
        // Try to get the requested mesh
        if let Some(asset_id) = renderer.asset_id {
            if let Some(gpu_mesh) = mesh_cache.get_safe(asset_id) {
                return ExtractedMesh::Loaded(gpu_mesh);
            }
        }

        // Fallback: try to use primitive type
        if let Some(primitive) = renderer.primitive {
            if let Some(gpu_mesh) = mesh_cache.get_primitive_safe(primitive) {
                return ExtractedMesh::Primitive(gpu_mesh);
            }
        }

        // Final fallback: error mesh
        ExtractedMesh::Error(mesh_cache.get_error_mesh())
    }
}

pub enum ExtractedMesh<'a> {
    Loaded(&'a GpuMesh),
    Primitive(&'a GpuMesh),
    Error(&'a GpuMesh),
}
```

## Acceptance Criteria

### Functional

- [ ] glTF 2.0 files load correctly (.gltf and .glb)
- [ ] All vertex attributes extracted (position, normal, tangent, UV, color)
- [ ] Missing tangents are auto-generated
- [ ] Missing normals are auto-generated
- [ ] Multi-primitive meshes supported
- [ ] Mesh assets cached and reused across entities
- [ ] Invalid meshes rejected with clear errors
- [ ] GPU buffers created and managed efficiently
- [ ] Editor asset browser shows mesh thumbnails
- [ ] Hot-reload updates mesh at runtime

### Hot-Swap Compliance

- [ ] MeshRenderer implements Serialize/Deserialize (serde)
- [ ] MeshCache implements HotReloadable trait
- [ ] MeshRenderer implements AssetDependent for mesh file tracking
- [ ] GPU buffers use double-buffering for seamless hot-swap
- [ ] Frame-boundary update queue processes pending mesh reloads
- [ ] Asset change notifications trigger mesh re-upload
- [ ] Panic recovery returns error mesh instead of crashing
- [ ] Hot-swap tests pass (serialization roundtrip, buffer swap, dependency tracking)

## Dependencies

- **None** (foundational for rendering)

## Dependents

- Phase 4: Instancing (mesh instance data)
- Phase 8: Keyframe Animation (skinned meshes)
- Phase 14: Spatial Queries (mesh bounds)
- Phase 15: LOD System (LOD mesh variants)

---

**Estimated Complexity**: High
**Primary Crates**: void_asset_server, void_render
**Reviewer Notes**: Ensure all glTF extensions are properly handled or explicitly rejected
