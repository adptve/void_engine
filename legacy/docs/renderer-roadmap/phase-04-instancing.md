# Phase 4: Instancing & Repetition

## Status: Not Started

## User Story

> As a scene author, I want to efficiently render many copies of the same mesh.

## Requirements Checklist

- [ ] Support instanced entities referencing a single mesh
- [ ] Per-instance transform overrides
- [ ] Optional per-instance material overrides
- [ ] GPU instancing where supported
- [ ] Graceful fallback when instancing is unavailable

## Current State Analysis

### Existing Implementation

The current renderer has basic draw call extraction but no instancing support. Each entity generates a separate draw call even when using the same mesh.

**void_render/src/extraction.rs:**
```rust
pub struct DrawCall {
    pub entity_id: EntityId,
    pub model_matrix: [[f32; 4]; 4],
    pub mesh_type_id: MeshTypeId,
    // ... per-draw data
}
```

### Gaps
1. No instance buffer generation
2. No mesh batching by asset ID
3. No per-instance data packing
4. No GPU instanced draw calls
5. No fallback path for non-instancing GPUs
6. No instance count limits

## Implementation Specification

### 1. Instance Data Structure

```rust
// crates/void_render/src/instancing.rs (NEW FILE)

use bytemuck::{Pod, Zeroable};

/// Per-instance data uploaded to GPU
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct InstanceData {
    /// Model matrix (4x4 row-major)
    pub model_matrix: [[f32; 4]; 4],

    /// Inverse transpose for normal transformation (3x3 in 4x3 layout)
    pub normal_matrix: [[f32; 4]; 3],

    /// Per-instance color tint (RGBA)
    pub color_tint: [f32; 4],

    /// Custom data (material override index, flags, etc.)
    pub custom: [f32; 4],
}

impl InstanceData {
    pub const LAYOUT: wgpu::VertexBufferLayout<'static> = wgpu::VertexBufferLayout {
        array_stride: std::mem::size_of::<InstanceData>() as u64,
        step_mode: wgpu::VertexStepMode::Instance,
        attributes: &[
            // model_matrix (4 vec4s)
            wgpu::VertexAttribute {
                offset: 0,
                shader_location: 10,
                format: wgpu::VertexFormat::Float32x4,
            },
            wgpu::VertexAttribute {
                offset: 16,
                shader_location: 11,
                format: wgpu::VertexFormat::Float32x4,
            },
            wgpu::VertexAttribute {
                offset: 32,
                shader_location: 12,
                format: wgpu::VertexFormat::Float32x4,
            },
            wgpu::VertexAttribute {
                offset: 48,
                shader_location: 13,
                format: wgpu::VertexFormat::Float32x4,
            },
            // normal_matrix (3 vec4s)
            wgpu::VertexAttribute {
                offset: 64,
                shader_location: 14,
                format: wgpu::VertexFormat::Float32x4,
            },
            wgpu::VertexAttribute {
                offset: 80,
                shader_location: 15,
                format: wgpu::VertexFormat::Float32x4,
            },
            wgpu::VertexAttribute {
                offset: 96,
                shader_location: 16,
                format: wgpu::VertexFormat::Float32x4,
            },
            // color_tint
            wgpu::VertexAttribute {
                offset: 112,
                shader_location: 17,
                format: wgpu::VertexFormat::Float32x4,
            },
            // custom
            wgpu::VertexAttribute {
                offset: 128,
                shader_location: 18,
                format: wgpu::VertexFormat::Float32x4,
            },
        ],
    };

    pub fn from_transform(global: &GlobalTransform, color: [f32; 4]) -> Self {
        let model = global.matrix;
        let normal = compute_normal_matrix(&model);

        Self {
            model_matrix: model,
            normal_matrix: normal,
            color_tint: color,
            custom: [0.0; 4],
        }
    }
}

fn compute_normal_matrix(model: &[[f32; 4]; 4]) -> [[f32; 4]; 3] {
    // Extract 3x3 and compute inverse transpose
    // For uniform scale, this equals the upper-left 3x3
    // For non-uniform, need full inverse transpose
    [
        [model[0][0], model[0][1], model[0][2], 0.0],
        [model[1][0], model[1][1], model[1][2], 0.0],
        [model[2][0], model[2][1], model[2][2], 0.0],
    ]
}
```

### 2. Instance Batch System

```rust
// crates/void_render/src/instance_batcher.rs (NEW FILE)

use std::collections::HashMap;
use wgpu::{Buffer, Device, Queue};

/// Batches entities by mesh for instanced rendering
pub struct InstanceBatcher {
    /// Batches keyed by (mesh_asset_id, material_id)
    batches: HashMap<BatchKey, InstanceBatch>,

    /// Maximum instances per batch (GPU limit aware)
    max_instances: u32,

    /// Device for buffer creation
    device: Arc<Device>,
}

#[derive(Clone, Copy, PartialEq, Eq, Hash)]
struct BatchKey {
    mesh_id: AssetId,
    material_id: Option<AssetId>,
}

pub struct InstanceBatch {
    /// Instance data for GPU upload
    instances: Vec<InstanceData>,

    /// GPU buffer (created on first use)
    buffer: Option<Buffer>,

    /// Buffer capacity
    buffer_capacity: u32,

    /// Entities in this batch (for picking)
    entities: Vec<Entity>,
}

impl InstanceBatcher {
    pub fn new(device: Arc<Device>, max_instances: u32) -> Self {
        Self {
            batches: HashMap::new(),
            max_instances: max_instances.min(65536),  // WebGPU limit
            device,
        }
    }

    /// Clear all batches for new frame
    pub fn begin_frame(&mut self) {
        for batch in self.batches.values_mut() {
            batch.instances.clear();
            batch.entities.clear();
        }
    }

    /// Add entity to appropriate batch
    pub fn add_instance(
        &mut self,
        entity: Entity,
        mesh_id: AssetId,
        material_id: Option<AssetId>,
        transform: &GlobalTransform,
        color: [f32; 4],
    ) {
        let key = BatchKey { mesh_id, material_id };

        let batch = self.batches.entry(key).or_insert_with(|| InstanceBatch {
            instances: Vec::with_capacity(256),
            buffer: None,
            buffer_capacity: 0,
            entities: Vec::with_capacity(256),
        });

        if batch.instances.len() < self.max_instances as usize {
            batch.instances.push(InstanceData::from_transform(transform, color));
            batch.entities.push(entity);
        } else {
            log::warn!("Instance batch overflow for mesh {:?}", mesh_id);
        }
    }

    /// Upload all batches to GPU
    pub fn upload(&mut self, queue: &Queue) {
        for batch in self.batches.values_mut() {
            if batch.instances.is_empty() {
                continue;
            }

            let required_capacity = batch.instances.len() as u32;

            // Grow buffer if needed
            if batch.buffer.is_none() || batch.buffer_capacity < required_capacity {
                let new_capacity = required_capacity.next_power_of_two().max(64);
                batch.buffer = Some(self.device.create_buffer(&wgpu::BufferDescriptor {
                    label: Some("Instance Buffer"),
                    size: (new_capacity as usize * std::mem::size_of::<InstanceData>()) as u64,
                    usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST,
                    mapped_at_creation: false,
                }));
                batch.buffer_capacity = new_capacity;
            }

            // Upload data
            if let Some(buffer) = &batch.buffer {
                queue.write_buffer(
                    buffer,
                    0,
                    bytemuck::cast_slice(&batch.instances),
                );
            }
        }
    }

    /// Get all non-empty batches for rendering
    pub fn batches(&self) -> impl Iterator<Item = (&BatchKey, &InstanceBatch)> {
        self.batches.iter().filter(|(_, b)| !b.instances.is_empty())
    }

    /// Get entity at instance index (for picking)
    pub fn get_entity(&self, key: &BatchKey, instance_index: u32) -> Option<Entity> {
        self.batches.get(key)?.entities.get(instance_index as usize).copied()
    }
}

impl InstanceBatch {
    pub fn buffer(&self) -> Option<&Buffer> {
        self.buffer.as_ref()
    }

    pub fn instance_count(&self) -> u32 {
        self.instances.len() as u32
    }
}
```

### 3. Instanced Draw Command

```rust
// crates/void_render/src/draw_command.rs (NEW FILE)

/// GPU draw command for instanced rendering
pub enum DrawCommand {
    /// Standard indexed draw
    Indexed {
        mesh: Arc<GpuMesh>,
        primitive_index: u32,
        instance_buffer: Arc<Buffer>,
        instance_count: u32,
        base_instance: u32,
    },

    /// Non-indexed draw
    NonIndexed {
        mesh: Arc<GpuMesh>,
        primitive_index: u32,
        instance_buffer: Arc<Buffer>,
        instance_count: u32,
        base_instance: u32,
    },

    /// Fallback for non-instancing path
    Single {
        mesh: Arc<GpuMesh>,
        primitive_index: u32,
        model_matrix: [[f32; 4]; 4],
    },
}

impl DrawCommand {
    pub fn execute<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        match self {
            DrawCommand::Indexed {
                mesh, primitive_index, instance_buffer, instance_count, base_instance
            } => {
                let prim = &mesh.primitives[*primitive_index as usize];
                pass.set_vertex_buffer(0, prim.vertex_buffer.slice(..));
                pass.set_vertex_buffer(1, instance_buffer.slice(..));
                pass.set_index_buffer(
                    prim.index_buffer.as_ref().unwrap().slice(..),
                    wgpu::IndexFormat::Uint32
                );
                pass.draw_indexed(
                    0..prim.index_count,
                    0,
                    *base_instance..(*base_instance + *instance_count)
                );
            }
            DrawCommand::NonIndexed {
                mesh, primitive_index, instance_buffer, instance_count, base_instance
            } => {
                let prim = &mesh.primitives[*primitive_index as usize];
                pass.set_vertex_buffer(0, prim.vertex_buffer.slice(..));
                pass.set_vertex_buffer(1, instance_buffer.slice(..));
                pass.draw(
                    0..prim.vertex_count,
                    *base_instance..(*base_instance + *instance_count)
                );
            }
            DrawCommand::Single { mesh, primitive_index, model_matrix } => {
                // Fallback: upload single instance
                // This path is for WebGL 1.0 or similar
                todo!("Implement non-instancing fallback");
            }
        }
    }
}
```

### 4. Instanced Component

```rust
// crates/void_ecs/src/components/instance.rs (NEW FILE)

/// Marker for entities that should be instanced together
#[derive(Clone, Debug)]
pub struct InstanceGroup {
    /// Group identifier (entities with same ID are batched)
    pub group_id: InstanceGroupId,

    /// Per-instance color tint override
    pub color_tint: Option<[f32; 4]>,

    /// Custom instance data
    pub custom_data: [f32; 4],
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct InstanceGroupId(pub u64);

impl InstanceGroupId {
    /// Auto-generate from mesh asset
    pub fn from_mesh(mesh_id: AssetId) -> Self {
        Self(mesh_id.0)
    }

    /// Manual grouping
    pub fn manual(id: u64) -> Self {
        Self(id)
    }
}

/// System-managed instance data (computed each frame)
#[derive(Clone, Debug, Default)]
pub struct ComputedInstanceData {
    /// Batch index for this entity
    pub batch_index: u32,

    /// Instance index within batch
    pub instance_index: u32,
}
```

### 5. Extraction with Instancing

```rust
// crates/void_render/src/extraction.rs (modifications)

impl SceneExtractor {
    pub fn extract_instanced(
        &self,
        world: &World,
        batcher: &mut InstanceBatcher,
        mesh_cache: &MeshCache,
    ) {
        batcher.begin_frame();

        // Query all renderable entities
        for (entity, (mesh_renderer, global_transform, material, visible)) in
            world.query::<(&MeshRenderer, &GlobalTransform, &Material, Option<&Visible>)>()
        {
            // Skip invisible
            if visible.map(|v| !v.visible).unwrap_or(false) {
                continue;
            }

            // Get mesh asset ID
            let mesh_id = match mesh_renderer.asset_id {
                Some(id) => id,
                None => continue,  // Mesh not loaded yet
            };

            // Get material ID if using asset materials
            let material_id = material.shader.as_ref()
                .and_then(|s| AssetId::from_path(s));

            // Color tint from material
            let color = material.base_color;

            // Add to batch
            batcher.add_instance(
                entity,
                mesh_id,
                material_id,
                global_transform,
                color,
            );
        }
    }
}
```

### 6. Shader Support

```wgsl
// crates/void_runtime/src/shaders/instanced.wgsl

struct InstanceInput {
    @location(10) model_matrix_0: vec4<f32>,
    @location(11) model_matrix_1: vec4<f32>,
    @location(12) model_matrix_2: vec4<f32>,
    @location(13) model_matrix_3: vec4<f32>,
    @location(14) normal_matrix_0: vec4<f32>,
    @location(15) normal_matrix_1: vec4<f32>,
    @location(16) normal_matrix_2: vec4<f32>,
    @location(17) color_tint: vec4<f32>,
    @location(18) custom: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_position: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) color: vec4<f32>,
};

@vertex
fn vs_main(
    vertex: VertexInput,
    instance: InstanceInput,
) -> VertexOutput {
    let model_matrix = mat4x4<f32>(
        instance.model_matrix_0,
        instance.model_matrix_1,
        instance.model_matrix_2,
        instance.model_matrix_3,
    );

    let normal_matrix = mat3x3<f32>(
        instance.normal_matrix_0.xyz,
        instance.normal_matrix_1.xyz,
        instance.normal_matrix_2.xyz,
    );

    var out: VertexOutput;

    let world_pos = model_matrix * vec4<f32>(vertex.position, 1.0);
    out.clip_position = camera.view_proj * world_pos;
    out.world_position = world_pos.xyz;
    out.world_normal = normalize(normal_matrix * vertex.normal);
    out.uv = vertex.uv0;
    out.color = vertex.color * instance.color_tint;

    return out;
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_render/src/instancing.rs` | CREATE | Instance data structure |
| `void_render/src/instance_batcher.rs` | CREATE | Batch management |
| `void_render/src/draw_command.rs` | CREATE | Draw command abstraction |
| `void_ecs/src/components/instance.rs` | CREATE | Instance components |
| `void_render/src/extraction.rs` | MODIFY | Instanced extraction |
| `void_runtime/src/shaders/instanced.wgsl` | CREATE | Instanced shader |
| `void_runtime/src/scene_renderer.rs` | MODIFY | Use instanced rendering |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_instance_batching() {
    let mut batcher = InstanceBatcher::new(device.clone(), 1000);

    let mesh_id = AssetId(1);
    let transform = GlobalTransform::default();

    // Add 100 instances of same mesh
    for i in 0..100 {
        batcher.add_instance(
            Entity::from_raw(i),
            mesh_id,
            None,
            &transform,
            [1.0; 4],
        );
    }

    let batches: Vec<_> = batcher.batches().collect();
    assert_eq!(batches.len(), 1);
    assert_eq!(batches[0].1.instance_count(), 100);
}

#[test]
fn test_batch_overflow() {
    let mut batcher = InstanceBatcher::new(device.clone(), 10);

    let mesh_id = AssetId(1);

    // Add more than max
    for i in 0..20 {
        batcher.add_instance(
            Entity::from_raw(i),
            mesh_id,
            None,
            &GlobalTransform::default(),
            [1.0; 4],
        );
    }

    // Should cap at max_instances
    let batch = batcher.batches().next().unwrap().1;
    assert_eq!(batch.instance_count(), 10);
}

#[test]
fn test_material_separation() {
    let mut batcher = InstanceBatcher::new(device.clone(), 1000);

    let mesh_id = AssetId(1);
    let mat_a = Some(AssetId(10));
    let mat_b = Some(AssetId(20));

    batcher.add_instance(Entity::from_raw(0), mesh_id, mat_a, &default(), [1.0; 4]);
    batcher.add_instance(Entity::from_raw(1), mesh_id, mat_b, &default(), [1.0; 4]);

    // Should create two batches
    assert_eq!(batcher.batches().count(), 2);
}
```

### Performance Tests
```rust
#[bench]
fn bench_10k_instances(b: &mut Bencher) {
    // Measure frame time with 10,000 instanced cubes
}

#[bench]
fn bench_10k_individual_draws(b: &mut Bencher) {
    // Compare with non-instanced path
}
```

## Performance Considerations

1. **Buffer Pooling**: Reuse instance buffers across frames
2. **Frustum Culling First**: Only batch visible instances
3. **Sort by Depth**: Front-to-back for opaque, back-to-front for transparent
4. **GPU-Driven**: Consider indirect draw for massive instance counts
5. **LOD Integration**: Batch by LOD level

## Hot-Swap Support

### Serialization

All instancing-related components and data structures support serde:

```rust
// crates/void_render/src/instancing.rs

use serde::{Serialize, Deserialize};

/// Per-instance data - serializable for hot-reload state capture
#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct InstanceData {
    pub model_matrix: [[f32; 4]; 4],
    pub normal_matrix: [[f32; 4]; 3],
    pub color_tint: [f32; 4],
    pub custom: [f32; 4],
}

// crates/void_ecs/src/components/instance.rs

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct InstanceGroup {
    pub group_id: InstanceGroupId,
    pub color_tint: Option<[f32; 4]>,
    pub custom_data: [f32; 4],
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct InstanceGroupId(pub u64);

/// Computed instance data - skipped during serialization (runtime-only)
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct ComputedInstanceData {
    #[serde(skip)]
    pub batch_index: u32,
    #[serde(skip)]
    pub instance_index: u32,
}
```

### HotReloadable Implementation

```rust
// crates/void_render/src/instance_batcher.rs

use void_core::hot_reload::{HotReloadable, ReloadContext, ReloadResult};

impl HotReloadable for InstanceBatcher {
    fn type_name(&self) -> &'static str {
        "InstanceBatcher"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        // Serialize batch configuration (not GPU buffers)
        let state = InstanceBatcherState {
            max_instances: self.max_instances,
            batch_keys: self.batches.keys().cloned().collect(),
        };
        bincode::serialize(&state).map_err(HotReloadError::Serialization)
    }

    fn deserialize_state(&mut self, data: &[u8], ctx: &ReloadContext) -> ReloadResult {
        let state: InstanceBatcherState = bincode::deserialize(data)?;

        self.max_instances = state.max_instances;

        // Pre-allocate batches for known keys
        for key in state.batch_keys {
            self.batches.entry(key).or_insert_with(|| InstanceBatch {
                instances: Vec::with_capacity(256),
                buffer: None,
                buffer_capacity: 0,
                entities: Vec::with_capacity(256),
            });
        }

        // Mark for rebuild on next frame
        self.needs_rebuild = true;

        Ok(())
    }
}

#[derive(Serialize, Deserialize)]
struct InstanceBatcherState {
    max_instances: u32,
    batch_keys: Vec<BatchKey>,
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
struct BatchKey {
    mesh_id: AssetId,
    material_id: Option<AssetId>,
}
```

### Instance Data Serialization

```rust
// crates/void_render/src/instance_batcher.rs

impl InstanceBatch {
    /// Serialize batch for hot-reload (CPU-side data only)
    pub fn serialize(&self) -> SerializedBatch {
        SerializedBatch {
            instances: self.instances.clone(),
            entities: self.entities.clone(),
        }
    }

    /// Restore from serialized state (GPU buffer rebuilt on next upload)
    pub fn deserialize(&mut self, data: SerializedBatch) {
        self.instances = data.instances;
        self.entities = data.entities;
        // GPU buffer will be recreated on next upload()
        self.buffer = None;
        self.buffer_capacity = 0;
    }
}

#[derive(Serialize, Deserialize)]
pub struct SerializedBatch {
    instances: Vec<InstanceData>,
    entities: Vec<Entity>,
}
```

### Frame-Boundary Updates

```rust
// crates/void_render/src/instance_batcher.rs

impl InstanceBatcher {
    /// Process hot-reload at frame boundary
    pub fn process_hot_reload(&mut self, ctx: &ReloadContext) {
        if self.needs_rebuild {
            // Clear all GPU buffers, they will be recreated on next upload
            for batch in self.batches.values_mut() {
                batch.buffer = None;
                batch.buffer_capacity = 0;
            }
            self.needs_rebuild = false;
            log::info!("InstanceBatcher rebuilt after hot-reload");
        }
    }

    /// Called when instance component changes during live editing
    pub fn invalidate_entity(&mut self, entity: Entity) {
        for batch in self.batches.values_mut() {
            if let Some(pos) = batch.entities.iter().position(|e| *e == entity) {
                // Mark for update (will be refreshed on next extraction)
                batch.dirty_instances.insert(pos);
            }
        }
    }
}

impl InstanceBatch {
    /// Track dirty instances for incremental updates
    dirty_instances: HashSet<usize>,

    /// Update only changed instances (optimization for live editing)
    pub fn upload_incremental(&mut self, queue: &Queue) {
        if self.dirty_instances.is_empty() {
            return;
        }

        if let Some(buffer) = &self.buffer {
            for idx in self.dirty_instances.drain() {
                if idx < self.instances.len() {
                    let offset = (idx * std::mem::size_of::<InstanceData>()) as u64;
                    queue.write_buffer(
                        buffer,
                        offset,
                        bytemuck::bytes_of(&self.instances[idx]),
                    );
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
    fn test_instance_data_serialization() {
        let instance = InstanceData {
            model_matrix: [[1.0, 0.0, 0.0, 0.0]; 4],
            normal_matrix: [[1.0, 0.0, 0.0, 0.0]; 3],
            color_tint: [1.0, 0.5, 0.0, 1.0],
            custom: [42.0, 0.0, 0.0, 0.0],
        };

        let bytes = bincode::serialize(&instance).unwrap();
        let restored: InstanceData = bincode::deserialize(&bytes).unwrap();

        assert_eq!(restored.color_tint, [1.0, 0.5, 0.0, 1.0]);
        assert_eq!(restored.custom[0], 42.0);
    }

    #[test]
    fn test_instance_group_serialization() {
        let group = InstanceGroup {
            group_id: InstanceGroupId(12345),
            color_tint: Some([1.0, 0.0, 0.0, 1.0]),
            custom_data: [1.0, 2.0, 3.0, 4.0],
        };

        let json = serde_json::to_string(&group).unwrap();
        let restored: InstanceGroup = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.group_id.0, 12345);
        assert_eq!(restored.color_tint, Some([1.0, 0.0, 0.0, 1.0]));
    }

    #[test]
    fn test_instance_batcher_hot_reload() {
        let (device, _queue) = create_test_device();
        let mut batcher = InstanceBatcher::new(device.clone(), 1000);

        // Add some instances
        let mesh_id = AssetId(1);
        for i in 0..10 {
            batcher.add_instance(
                Entity::from_raw(i),
                mesh_id,
                None,
                &GlobalTransform::default(),
                [1.0; 4],
            );
        }

        // Serialize
        let state = batcher.serialize_state().unwrap();

        // Create new batcher and restore
        let mut new_batcher = InstanceBatcher::new(device.clone(), 1000);
        new_batcher.deserialize_state(&state, &ReloadContext::default()).unwrap();

        assert!(new_batcher.needs_rebuild);
        assert!(new_batcher.batches.contains_key(&BatchKey { mesh_id, material_id: None }));
    }

    #[test]
    fn test_batch_incremental_update() {
        let (device, queue) = create_test_device();
        let mut batcher = InstanceBatcher::new(device.clone(), 1000);

        let mesh_id = AssetId(1);
        let entity = Entity::from_raw(0);

        batcher.add_instance(entity, mesh_id, None, &GlobalTransform::default(), [1.0; 4]);
        batcher.upload(&queue);

        // Mark entity as dirty
        batcher.invalidate_entity(entity);

        let batch = batcher.batches.get(&BatchKey { mesh_id, material_id: None }).unwrap();
        assert!(batch.dirty_instances.contains(&0));
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
// crates/void_render/src/instance_batcher.rs

impl InstanceBatcher {
    /// Add instance with panic recovery
    pub fn add_instance_safe(
        &mut self,
        entity: Entity,
        mesh_id: AssetId,
        material_id: Option<AssetId>,
        transform: &GlobalTransform,
        color: [f32; 4],
    ) -> bool {
        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.add_instance(entity, mesh_id, material_id, transform, color)
        }))
        .map_err(|panic| {
            log::error!(
                "Panic adding instance for entity {:?}, mesh {:?}: {:?}",
                entity, mesh_id, panic
            );
        })
        .is_ok()
    }

    /// Upload with GPU error recovery
    pub fn upload_safe(&mut self, queue: &Queue) {
        for (key, batch) in &mut self.batches {
            if let Err(e) = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                batch.upload_to_gpu(queue);
            })) {
                log::error!("Failed to upload batch {:?}: {:?}", key, e);
                // Mark batch for cleanup, will skip rendering
                batch.upload_failed = true;
            }
        }
    }
}
```

### Fallback Behavior

```rust
// crates/void_render/src/instance_batcher.rs

impl InstanceBatcher {
    /// Get batches, filtering out failed uploads
    pub fn batches_safe(&self) -> impl Iterator<Item = (&BatchKey, &InstanceBatch)> {
        self.batches.iter()
            .filter(|(_, b)| !b.instances.is_empty() && !b.upload_failed)
    }

    /// Render with fallback to individual draws on batch failure
    pub fn render_batch_safe<'a>(
        &'a self,
        key: &BatchKey,
        pass: &mut wgpu::RenderPass<'a>,
        fallback_renderer: &'a FallbackRenderer,
    ) {
        if let Some(batch) = self.batches.get(key) {
            if batch.upload_failed || batch.buffer.is_none() {
                // Fallback: render instances individually (slow but works)
                log::warn!("Using fallback rendering for batch {:?}", key);
                for (i, entity) in batch.entities.iter().enumerate() {
                    fallback_renderer.render_single(pass, *entity, &batch.instances[i]);
                }
            } else {
                // Normal instanced rendering
                batch.render(pass);
            }
        }
    }
}

impl InstanceBatch {
    upload_failed: bool,

    pub fn render<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        if let Some(buffer) = &self.buffer {
            pass.set_vertex_buffer(1, buffer.slice(..));
            // Draw call handled by caller
        }
    }
}
```

## Acceptance Criteria

### Functional

- [ ] Entities with same mesh automatically batched
- [ ] Per-instance transforms work correctly
- [ ] Per-instance color tint works
- [ ] Instance buffer uploaded efficiently
- [ ] Draw call count reduced (verify with debug stats)
- [ ] 10,000 instances render at 60 FPS
- [ ] Fallback path works on non-instancing GPUs
- [ ] Entity picking works with instanced entities
- [ ] Editor shows instance count per mesh

### Hot-Swap Compliance

- [ ] InstanceData implements Serialize/Deserialize (serde + bytemuck)
- [ ] InstanceGroup component implements Serialize/Deserialize
- [ ] InstanceBatcher implements HotReloadable trait
- [ ] Batch GPU buffers recreated after hot-reload (not serialized)
- [ ] Incremental instance updates supported for live editing
- [ ] Entity invalidation triggers batch dirty marking
- [ ] Panic recovery falls back to individual draw calls
- [ ] Hot-swap tests pass (data serialization, batcher reload, incremental updates)

## Dependencies

- **Phase 3: Mesh Import** - Need mesh assets to instance

## Dependents

- Phase 15: LOD System (LOD-aware instancing)
- Phase 16: Scene Streaming (chunk-based instancing)

---

**Estimated Complexity**: Medium
**Primary Crates**: void_render
**Reviewer Notes**: Verify WebGPU instance limits are respected
