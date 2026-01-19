//! Scene Renderer - Bridges kernel entities to GPU rendering
//!
//! This module extracts entity data from the kernel/ECS and renders
//! actual 3D content using void_render infrastructure.

use std::collections::HashMap;
use std::path::Path;
use std::sync::Arc;
use wgpu::*;
use wgpu::util::DeviceExt;

use void_render::extraction::{DrawCall, ExtractedMaterial, ExtractedScene, MeshTypeId, RenderExtractor};
use void_render::camera::{Camera, Projection};
use void_render::camera_controller::{CameraController, CameraInput, CameraMode};
use void_math::{Vec3, Vec4, Mat4, Quat};

use crate::texture_manager::{TextureManager, TextureHandle, TextureOptions, PbrTextures};
use void_asset_server::loaders::gltf::GltfLoader;
use crate::scene_loader::{
    SceneDefinition, EntityDef, ParticleEmitterDef, MeshDef, PrimitiveMesh,
    ColorOrTexture, ValueOrTexture, InputConfig, CameraInputConfig, MouseButton,
    AnimationDef, RotateAnimation, OscillateAnimation, PathAnimation, OrbitAnimation,
    PulseAnimation, InterpolationMode, EasingFunction, SkyDef,
};

/// A renderable entity extracted from kernel patches
#[derive(Clone, Debug)]
pub struct SceneEntity {
    pub entity_id: u64,
    pub transform: Transform,
    pub material: MaterialData,
    pub mesh: MeshType,
    pub layer: String,
    pub visible: bool,
    /// Animation definition (if any)
    pub animation: Option<AnimationDef>,
    /// Base transform (before animation)
    pub base_transform: Transform,
}

/// Runtime animation state for an entity
#[derive(Clone, Debug, Default)]
pub struct AnimationState {
    /// Current animation time
    pub time: f32,
    /// For path animation: current segment index
    pub path_segment: usize,
    /// For ping-pong: current direction (true = forward)
    pub forward: bool,
}

/// Transform component data
#[derive(Clone, Debug, Default)]
pub struct Transform {
    pub position: [f32; 3],
    pub rotation: [f32; 4], // quaternion
    pub scale: [f32; 3],
}

impl Transform {
    pub fn to_matrix(&self) -> [[f32; 4]; 4] {
        let pos = Vec3::new(self.position[0], self.position[1], self.position[2]);
        let rot = Quat::new(self.rotation[0], self.rotation[1], self.rotation[2], self.rotation[3]);
        let scale = Vec3::new(self.scale[0], self.scale[1], self.scale[2]);

        // Build matrix: translate * rotate * scale
        let scale_mat = Mat4::from_scale(scale);
        let rot_mat = Mat4::from_quat(rot);
        let trans_mat = Mat4::from_translation(pos);
        let mat = trans_mat * rot_mat * scale_mat;
        mat.to_cols_array_2d()
    }
}

/// Material component data
#[derive(Clone, Debug)]
pub struct MaterialData {
    pub base_color: [f32; 4],
    pub metallic: f32,
    pub roughness: f32,
    pub emissive: [f32; 3],
    pub shader: Option<String>,
    /// Texture handles (optional)
    pub textures: PbrTextures,
    /// Normal map strength (0.0 = no effect, 1.0 = full)
    pub normal_scale: f32,
    /// Ambient occlusion strength
    pub ao_strength: f32,
}

impl Default for MaterialData {
    fn default() -> Self {
        Self {
            base_color: [1.0, 1.0, 1.0, 1.0],
            metallic: 0.0,
            roughness: 0.5,
            emissive: [0.0, 0.0, 0.0],
            shader: None,
            textures: PbrTextures::new(),
            normal_scale: 1.0,
            ao_strength: 1.0,
        }
    }
}

/// Mesh type
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum MeshType {
    Sphere,
    Cube,
    Torus,
    Diamond,
    Plane,
    Quad,
    FullscreenQuad,
    Custom(String),
}

impl Default for MeshType {
    fn default() -> Self {
        Self::Cube
    }
}

// ============================================================================
// PARTICLE SYSTEM
// ============================================================================

/// Individual particle state
#[derive(Clone, Debug)]
pub struct Particle {
    pub position: [f32; 3],
    pub velocity: [f32; 3],
    pub color: [f32; 4],
    pub size: f32,
    pub age: f32,      // 0.0 = just born, 1.0 = dead
    pub lifetime: f32, // total lifetime in seconds
}

impl Default for Particle {
    fn default() -> Self {
        Self {
            position: [0.0, 0.0, 0.0],
            velocity: [0.0, 1.0, 0.0],
            color: [1.0, 1.0, 1.0, 1.0],
            size: 0.1,
            age: 0.0,
            lifetime: 2.0,
        }
    }
}

/// Particle emitter configuration
#[derive(Clone, Debug)]
pub struct ParticleEmitter {
    pub position: [f32; 3],
    pub emit_rate: f32,           // particles per second
    pub max_particles: usize,
    pub lifetime_min: f32,
    pub lifetime_max: f32,
    pub speed_min: f32,
    pub speed_max: f32,
    pub size_min: f32,
    pub size_max: f32,
    pub color_start: [f32; 4],
    pub color_end: [f32; 4],
    pub gravity: [f32; 3],
    pub spread: f32,              // cone angle in radians (0 = straight up)
    pub direction: [f32; 3],      // emit direction (normalized)
    pub enabled: bool,
    // Internal state (pub(crate) for ..Default::default())
    pub(crate) emit_accumulator: f32,
}

impl Default for ParticleEmitter {
    fn default() -> Self {
        Self {
            position: [0.0, 0.0, 0.0],
            emit_rate: 50.0,
            max_particles: 1000,
            lifetime_min: 1.0,
            lifetime_max: 3.0,
            speed_min: 1.0,
            speed_max: 3.0,
            size_min: 0.05,
            size_max: 0.15,
            color_start: [1.0, 0.8, 0.2, 1.0], // Orange/yellow
            color_end: [1.0, 0.2, 0.0, 0.0],   // Red, fading out
            gravity: [0.0, -2.0, 0.0],
            spread: 0.5, // ~30 degrees
            direction: [0.0, 1.0, 0.0],
            enabled: true,
            emit_accumulator: 0.0,
        }
    }
}

/// Particle system managing multiple particles
pub struct ParticleSystem {
    pub emitter: ParticleEmitter,
    pub particles: Vec<Particle>,
    // GPU resources
    instance_buffer: Option<Buffer>,
    instance_count: u32,
}

impl ParticleSystem {
    pub fn new(emitter: ParticleEmitter) -> Self {
        let max = emitter.max_particles;
        Self {
            emitter,
            particles: Vec::with_capacity(max),
            instance_buffer: None,
            instance_count: 0,
        }
    }

    /// Initialize GPU resources
    pub fn init_gpu(&mut self, device: &Device) {
        // Create instance buffer for particle data
        // Each particle instance: position(3) + color(4) + size(1) + age(1) = 9 floats = 36 bytes
        let buffer_size = (self.emitter.max_particles * 36) as u64;
        self.instance_buffer = Some(device.create_buffer(&BufferDescriptor {
            label: Some("particle_instance_buffer"),
            size: buffer_size,
            usage: BufferUsages::VERTEX | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        }));
    }

    /// Update all particles
    pub fn update(&mut self, delta_time: f32) {
        if !self.emitter.enabled {
            return;
        }

        // Emit new particles
        self.emitter.emit_accumulator += self.emitter.emit_rate * delta_time;
        while self.emitter.emit_accumulator >= 1.0 && self.particles.len() < self.emitter.max_particles {
            self.emitter.emit_accumulator -= 1.0;
            self.spawn_particle();
        }

        // Update existing particles
        let gravity = self.emitter.gravity;
        let color_start = self.emitter.color_start;
        let color_end = self.emitter.color_end;

        for particle in &mut self.particles {
            // Age the particle
            particle.age += delta_time / particle.lifetime;

            // Apply gravity
            particle.velocity[0] += gravity[0] * delta_time;
            particle.velocity[1] += gravity[1] * delta_time;
            particle.velocity[2] += gravity[2] * delta_time;

            // Update position
            particle.position[0] += particle.velocity[0] * delta_time;
            particle.position[1] += particle.velocity[1] * delta_time;
            particle.position[2] += particle.velocity[2] * delta_time;

            // Interpolate color based on age
            let t = particle.age.clamp(0.0, 1.0);
            particle.color[0] = color_start[0] + (color_end[0] - color_start[0]) * t;
            particle.color[1] = color_start[1] + (color_end[1] - color_start[1]) * t;
            particle.color[2] = color_start[2] + (color_end[2] - color_start[2]) * t;
            particle.color[3] = color_start[3] + (color_end[3] - color_start[3]) * t;
        }

        // Remove dead particles
        self.particles.retain(|p| p.age < 1.0);
    }

    fn spawn_particle(&mut self) {
        use std::f32::consts::PI;

        // Random values (simple LCG for speed)
        let rand = || -> f32 {
            static mut SEED: u32 = 12345;
            unsafe {
                SEED = SEED.wrapping_mul(1103515245).wrapping_add(12345);
                (SEED as f32 / u32::MAX as f32)
            }
        };

        // Random direction within cone
        let spread = self.emitter.spread;
        let theta = rand() * 2.0 * PI; // azimuth
        let phi = rand() * spread;      // polar angle from direction

        // Create local coordinate system from direction
        let dir = Vec3::new(
            self.emitter.direction[0],
            self.emitter.direction[1],
            self.emitter.direction[2],
        ).normalize();

        // Find perpendicular vectors
        let up = if dir.y.abs() < 0.99 {
            Vec3::Y
        } else {
            Vec3::X
        };
        let right = dir.cross(up).normalize();
        let up = right.cross(dir);

        // Calculate emission direction
        let sin_phi = phi.sin();
        let emit_dir = dir * phi.cos()
            + right * sin_phi * theta.cos()
            + up * sin_phi * theta.sin();

        // Random speed
        let speed = self.emitter.speed_min + rand() * (self.emitter.speed_max - self.emitter.speed_min);
        let lifetime = self.emitter.lifetime_min + rand() * (self.emitter.lifetime_max - self.emitter.lifetime_min);
        let size = self.emitter.size_min + rand() * (self.emitter.size_max - self.emitter.size_min);

        self.particles.push(Particle {
            position: self.emitter.position,
            velocity: [emit_dir.x * speed, emit_dir.y * speed, emit_dir.z * speed],
            color: self.emitter.color_start,
            size,
            age: 0.0,
            lifetime,
        });
    }

    /// Upload particle data to GPU
    pub fn upload(&mut self, queue: &Queue) {
        if let Some(buffer) = &self.instance_buffer {
            // Pack particle data: position(3) + color(4) + size(1) + age_normalized(1)
            let mut data: Vec<f32> = Vec::with_capacity(self.particles.len() * 9);
            for p in &self.particles {
                data.extend_from_slice(&p.position);
                data.extend_from_slice(&p.color);
                data.push(p.size);
                // Normalize age to 0-1 range for shader
                let age_normalized = (p.age / p.lifetime).clamp(0.0, 1.0);
                data.push(age_normalized);
            }

            if !data.is_empty() {
                queue.write_buffer(buffer, 0, bytemuck::cast_slice(&data));
            }
            self.instance_count = self.particles.len() as u32;
        }
    }

    pub fn particle_count(&self) -> usize {
        self.particles.len()
    }
}

/// GPU mesh data
pub struct GpuMesh {
    pub vertex_buffer: Buffer,
    pub index_buffer: Buffer,
    pub index_count: u32,
}

/// Shadow mapping configuration
#[derive(Clone, Debug)]
pub struct ShadowConfig {
    /// Shadow map resolution (width and height)
    pub resolution: u32,
    /// Enable/disable shadows
    pub enabled: bool,
    /// Shadow bias to prevent shadow acne
    pub bias: f32,
    /// Normal bias for slope-based adjustment
    pub normal_bias: f32,
    /// PCF (Percentage Closer Filtering) sample count for soft shadows
    /// 1 = hard shadows, 4/9/16 = progressively softer
    pub pcf_samples: u32,
    /// Shadow softness (spread of PCF samples)
    pub softness: f32,
    /// Shadow distance (how far from camera shadows are rendered)
    pub distance: f32,
}

impl Default for ShadowConfig {
    fn default() -> Self {
        Self {
            resolution: 2048,
            enabled: true,
            bias: 0.005,
            normal_bias: 0.02,
            pcf_samples: 9,  // 3x3 PCF for nice soft shadows
            softness: 1.5,
            distance: 50.0,
        }
    }
}

impl ShadowConfig {
    /// High quality shadows (4K, 16 samples)
    pub fn high_quality() -> Self {
        Self {
            resolution: 4096,
            pcf_samples: 16,
            softness: 2.0,
            ..Default::default()
        }
    }

    /// Low quality shadows for performance (1K, hard shadows)
    pub fn low_quality() -> Self {
        Self {
            resolution: 1024,
            pcf_samples: 1,
            softness: 0.0,
            ..Default::default()
        }
    }

    /// Disable shadows entirely
    pub fn disabled() -> Self {
        Self {
            enabled: false,
            ..Default::default()
        }
    }
}

/// Scene state for rendering
pub struct SceneRenderer {
    // Entities by ID
    entities: HashMap<u64, SceneEntity>,

    // Camera
    pub camera: Camera,
    pub camera_controller: CameraController,

    // GPU resources
    meshes: HashMap<MeshType, GpuMesh>,

    // Render pipelines
    pbr_pipeline: Option<RenderPipeline>,
    pbr_textured_pipeline: Option<RenderPipeline>,
    sky_pipeline: Option<RenderPipeline>,
    shadow_pipeline: Option<RenderPipeline>,

    // Uniform buffers
    camera_buffer: Buffer,
    model_buffer: Buffer,
    material_buffer: Buffer,
    light_buffer: Buffer,
    shadow_buffer: Buffer,
    sky_buffer: Buffer,

    // Bind groups
    camera_bind_group: BindGroup,
    camera_env_bind_group_layout: BindGroupLayout,
    particle_camera_bind_group: BindGroup,  // Camera-only bind group for particles
    model_bind_group: BindGroup,
    material_bind_group: BindGroup,
    light_bind_group: BindGroup,
    shadow_bind_group: BindGroup,
    shadow_bind_group_layout: BindGroupLayout,
    camera_bind_group_layout: BindGroupLayout,
    texture_bind_group_layout: BindGroupLayout,

    // Texture management
    pub texture_manager: TextureManager,
    default_texture_bind_group: BindGroup,

    // Environment map
    environment_handle: TextureHandle,
    has_environment_map: bool,

    // Shadow mapping
    shadow_config: ShadowConfig,
    shadow_texture: Option<Texture>,
    shadow_view: Option<TextureView>,
    shadow_sampler: Option<Sampler>,

    // Depth texture
    depth_texture: Option<Texture>,
    depth_view: Option<TextureView>,

    // Screen size
    size: (u32, u32),

    // Light settings
    pub light_direction: [f32; 3],
    pub light_color: [f32; 3],
    pub light_intensity: f32,
    pub ambient_intensity: f32,

    // Extractor
    extractor: RenderExtractor,

    // Particle system
    pub particle_system: Option<ParticleSystem>,
    particle_pipeline: Option<RenderPipeline>,
    particle_quad_buffer: Option<Buffer>,
    time: f32,

    // Input configuration
    pub input_config: InputConfig,

    // Sky configuration
    pub sky_config: SkyDef,

    // Animation states by entity ID
    animation_states: HashMap<u64, AnimationState>,

    // ========== New Phase Features ==========

    // Camera definitions from scene (Phase 2)
    scene_cameras: Vec<crate::scene_loader::CameraDef>,
    active_camera_index: usize,

    // Light definitions from scene (Phase 5)
    scene_lights: Vec<SceneLight>,

    // Shadow configuration from scene (Phase 6)
    scene_shadow_config: crate::scene_loader::ShadowsDef,

    // Picking configuration (Phase 10)
    picking_config: crate::scene_loader::PickingDef,
    hovered_entity: Option<u64>,
    selected_entity: Option<u64>,

    // Spatial configuration (Phase 14)
    spatial_config: crate::scene_loader::SpatialDef,

    // Debug configuration (Phase 18)
    debug_config: crate::scene_loader::DebugDef,
    debug_stats: DebugStats,

    // Advanced material support (Phase 7)
    // Stored per-entity in MaterialData
}

/// Runtime light data for rendering
#[derive(Clone, Debug)]
pub struct SceneLight {
    pub name: String,
    pub light_type: SceneLightType,
    pub enabled: bool,
    pub color: [f32; 3],
    pub intensity: f32,
    pub cast_shadows: bool,
}

/// Light type with associated data
#[derive(Clone, Debug)]
pub enum SceneLightType {
    Directional {
        direction: [f32; 3],
    },
    Point {
        position: [f32; 3],
        range: f32,
        attenuation: [f32; 3], // constant, linear, quadratic
    },
    Spot {
        position: [f32; 3],
        direction: [f32; 3],
        range: f32,
        inner_angle: f32,
        outer_angle: f32,
    },
}

/// Debug statistics for overlay
#[derive(Clone, Debug, Default)]
pub struct DebugStats {
    pub fps: f32,
    pub frame_time_ms: f32,
    pub draw_calls: u32,
    pub triangles: u32,
    pub entities_total: u32,
    pub entities_visible: u32,
    pub cpu_time_ms: f32,
}

/// Result of loading a glTF file - includes mesh, material, and textures
struct GltfLoadResult {
    mesh_type: MeshType,
    base_color: [f32; 4],
    metallic: f32,
    roughness: f32,
    textures: PbrTextures,
}

impl SceneRenderer {
    pub fn new(device: &Device, queue: &Queue, format: TextureFormat, size: (u32, u32)) -> Self {
        // Create camera
        let mut camera = Camera::perspective(
            60.0_f32.to_radians(),
            size.0 as f32 / size.1 as f32,
            0.1,
            100.0,
        );
        camera.position = Vec3::new(0.0, 1.0, 3.0);

        // Create camera controller (orbit mode)
        let mut camera_controller = CameraController::orbit(Vec3::ZERO, 3.0);
        camera_controller.pitch = 0.3;

        // Create uniform buffers
        let camera_buffer = device.create_buffer(&BufferDescriptor {
            label: Some("camera_buffer"),
            size: 256, // CameraUniforms
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let model_buffer = device.create_buffer(&BufferDescriptor {
            label: Some("model_buffer"),
            size: 256, // ModelUniforms
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let material_buffer = device.create_buffer(&BufferDescriptor {
            label: Some("material_buffer"),
            size: 64, // MaterialUniforms
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let light_buffer = device.create_buffer(&BufferDescriptor {
            label: Some("light_buffer"),
            size: 64, // LightUniforms
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        // Sky uniform buffer (for procedural sky configuration)
        // Matches SkyUniforms in sky.wgsl: 5 vec4s = 80 bytes
        let sky_buffer = device.create_buffer(&BufferDescriptor {
            label: Some("sky_buffer"),
            size: 80,
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        // Create bind group layouts for all uniform types
        let uniform_bind_group_layout = device.create_bind_group_layout(&BindGroupLayoutDescriptor {
            label: Some("uniform_bind_group_layout"),
            entries: &[BindGroupLayoutEntry {
                binding: 0,
                visibility: ShaderStages::VERTEX | ShaderStages::FRAGMENT,
                ty: BindingType::Buffer {
                    ty: BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

        // Camera + environment bind group layout (group 0 with env map)
        let camera_env_bind_group_layout = device.create_bind_group_layout(&BindGroupLayoutDescriptor {
            label: Some("camera_env_bind_group_layout"),
            entries: &[
                // Camera uniforms
                BindGroupLayoutEntry {
                    binding: 0,
                    visibility: ShaderStages::VERTEX | ShaderStages::FRAGMENT,
                    ty: BindingType::Buffer {
                        ty: BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                // Environment texture
                BindGroupLayoutEntry {
                    binding: 1,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Texture {
                        sample_type: TextureSampleType::Float { filterable: true },
                        view_dimension: TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                // Environment sampler
                BindGroupLayoutEntry {
                    binding: 2,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Sampler(SamplerBindingType::Filtering),
                    count: None,
                },
                // Sky uniforms (for procedural sky configuration)
                BindGroupLayoutEntry {
                    binding: 3,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Buffer {
                        ty: BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
            ],
        });

        // Create texture manager with default textures (needed for bind groups)
        let texture_manager = TextureManager::new(device, queue);

        // Create bind groups
        // Use default_white as placeholder environment until HDR is loaded
        let default_env_handle = texture_manager.default_white;
        let default_env = texture_manager.get(default_env_handle).unwrap();
        let camera_bind_group = device.create_bind_group(&BindGroupDescriptor {
            label: Some("camera_env_bind_group"),
            layout: &camera_env_bind_group_layout,
            entries: &[
                BindGroupEntry {
                    binding: 0,
                    resource: camera_buffer.as_entire_binding(),
                },
                BindGroupEntry {
                    binding: 1,
                    resource: BindingResource::TextureView(&default_env.view),
                },
                BindGroupEntry {
                    binding: 2,
                    resource: BindingResource::Sampler(&default_env.sampler),
                },
                BindGroupEntry {
                    binding: 3,
                    resource: sky_buffer.as_entire_binding(),
                },
            ],
        });

        let model_bind_group = device.create_bind_group(&BindGroupDescriptor {
            label: Some("model_bind_group"),
            layout: &uniform_bind_group_layout,
            entries: &[BindGroupEntry {
                binding: 0,
                resource: model_buffer.as_entire_binding(),
            }],
        });

        // Particle-only camera bind group (just camera buffer, no env map)
        let particle_camera_bind_group = device.create_bind_group(&BindGroupDescriptor {
            label: Some("particle_camera_bind_group"),
            layout: &uniform_bind_group_layout,
            entries: &[BindGroupEntry {
                binding: 0,
                resource: camera_buffer.as_entire_binding(),
            }],
        });

        let material_bind_group = device.create_bind_group(&BindGroupDescriptor {
            label: Some("material_bind_group"),
            layout: &uniform_bind_group_layout,
            entries: &[BindGroupEntry {
                binding: 0,
                resource: material_buffer.as_entire_binding(),
            }],
        });

        // Shadow configuration (default quality)
        let shadow_config = ShadowConfig::default();

        // Create shadow map texture (needed before light+shadow bind group)
        let (shadow_texture, shadow_view, shadow_sampler) = Self::create_shadow_map(device, shadow_config.resolution);

        // Shadow uniform buffer (light view-projection matrix + shadow config)
        // Layout: mat4x4 (64 bytes) + vec4 config (16 bytes) = 80 bytes, padded to 96
        let shadow_buffer = device.create_buffer(&BufferDescriptor {
            label: Some("shadow_buffer"),
            size: 96,
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        // Combined light + shadow bind group layout (group 3)
        // This stays within the 4 bind group limit
        let light_shadow_bind_group_layout = device.create_bind_group_layout(&BindGroupLayoutDescriptor {
            label: Some("light_shadow_bind_group_layout"),
            entries: &[
                // Light uniforms (binding 0)
                BindGroupLayoutEntry {
                    binding: 0,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Buffer {
                        ty: BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                // Shadow map texture (binding 1)
                BindGroupLayoutEntry {
                    binding: 1,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Texture {
                        sample_type: TextureSampleType::Depth,
                        view_dimension: TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                // Shadow comparison sampler (binding 2)
                BindGroupLayoutEntry {
                    binding: 2,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Sampler(SamplerBindingType::Comparison),
                    count: None,
                },
                // Shadow uniforms - light space matrix + config (binding 3)
                BindGroupLayoutEntry {
                    binding: 3,
                    visibility: ShaderStages::VERTEX | ShaderStages::FRAGMENT,
                    ty: BindingType::Buffer {
                        ty: BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
            ],
        });

        // Combined light + shadow bind group
        let light_bind_group = device.create_bind_group(&BindGroupDescriptor {
            label: Some("light_shadow_bind_group"),
            layout: &light_shadow_bind_group_layout,
            entries: &[
                BindGroupEntry {
                    binding: 0,
                    resource: light_buffer.as_entire_binding(),
                },
                BindGroupEntry {
                    binding: 1,
                    resource: BindingResource::TextureView(&shadow_view),
                },
                BindGroupEntry {
                    binding: 2,
                    resource: BindingResource::Sampler(&shadow_sampler),
                },
                BindGroupEntry {
                    binding: 3,
                    resource: shadow_buffer.as_entire_binding(),
                },
            ],
        });

        // Create combined material+texture bind group layout (group 2)
        // This combines material uniform with textures to stay within 4 bind group limit
        let material_texture_bind_group_layout = device.create_bind_group_layout(&BindGroupLayoutDescriptor {
            label: Some("material_texture_bind_group_layout"),
            entries: &[
                // Material uniform buffer (binding 0)
                BindGroupLayoutEntry {
                    binding: 0,
                    visibility: ShaderStages::VERTEX | ShaderStages::FRAGMENT,
                    ty: BindingType::Buffer {
                        ty: BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                // Albedo texture (binding 1)
                BindGroupLayoutEntry {
                    binding: 1,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Texture {
                        sample_type: TextureSampleType::Float { filterable: true },
                        view_dimension: TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                // Albedo sampler (binding 2)
                BindGroupLayoutEntry {
                    binding: 2,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Sampler(SamplerBindingType::Filtering),
                    count: None,
                },
                // Normal texture (binding 3)
                BindGroupLayoutEntry {
                    binding: 3,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Texture {
                        sample_type: TextureSampleType::Float { filterable: true },
                        view_dimension: TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                // Normal sampler (binding 4)
                BindGroupLayoutEntry {
                    binding: 4,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Sampler(SamplerBindingType::Filtering),
                    count: None,
                },
                // Metallic-roughness texture (binding 5)
                BindGroupLayoutEntry {
                    binding: 5,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Texture {
                        sample_type: TextureSampleType::Float { filterable: true },
                        view_dimension: TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                // Metallic-roughness sampler (binding 6)
                BindGroupLayoutEntry {
                    binding: 6,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Sampler(SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        });

        // For backwards compatibility, keep a reference as texture_bind_group_layout
        let texture_bind_group_layout = material_texture_bind_group_layout;

        // Create default material+texture bind group (all white/normal defaults)
        let default_white = texture_manager.get(texture_manager.default_white).unwrap();
        let default_normal = texture_manager.get(texture_manager.default_normal).unwrap();

        let default_texture_bind_group = device.create_bind_group(&BindGroupDescriptor {
            label: Some("default_material_texture_bind_group"),
            layout: &texture_bind_group_layout,
            entries: &[
                BindGroupEntry { binding: 0, resource: material_buffer.as_entire_binding() },
                BindGroupEntry { binding: 1, resource: BindingResource::TextureView(&default_white.view) },
                BindGroupEntry { binding: 2, resource: BindingResource::Sampler(&default_white.sampler) },
                BindGroupEntry { binding: 3, resource: BindingResource::TextureView(&default_normal.view) },
                BindGroupEntry { binding: 4, resource: BindingResource::Sampler(&default_normal.sampler) },
                BindGroupEntry { binding: 5, resource: BindingResource::TextureView(&default_white.view) },
                BindGroupEntry { binding: 6, resource: BindingResource::Sampler(&default_white.sampler) },
            ],
        });

        // Create PBR shader
        let pbr_shader = device.create_shader_module(ShaderModuleDescriptor {
            label: Some("pbr_shader"),
            source: ShaderSource::Wgsl(include_str!("shaders/pbr.wgsl").into()),
        });

        // Create sky shader
        let sky_shader = device.create_shader_module(ShaderModuleDescriptor {
            label: Some("sky_shader"),
            source: ShaderSource::Wgsl(include_str!("shaders/sky.wgsl").into()),
        });

        // Create shadow depth shader
        let shadow_shader = device.create_shader_module(ShaderModuleDescriptor {
            label: Some("shadow_shader"),
            source: ShaderSource::Wgsl(include_str!("shaders/shadow.wgsl").into()),
        });

        // Shadow pass bind group layout (light space uniform only)
        let shadow_pass_bind_group_layout = device.create_bind_group_layout(&BindGroupLayoutDescriptor {
            label: Some("shadow_pass_bind_group_layout"),
            entries: &[BindGroupLayoutEntry {
                binding: 0,
                visibility: ShaderStages::VERTEX,
                ty: BindingType::Buffer {
                    ty: BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

        // Shadow pass pipeline layout (light space + model)
        let shadow_pipeline_layout = device.create_pipeline_layout(&PipelineLayoutDescriptor {
            label: Some("shadow_pipeline_layout"),
            bind_group_layouts: &[
                &shadow_pass_bind_group_layout, // group 0: light space
                &uniform_bind_group_layout,     // group 1: model
            ],
            push_constant_ranges: &[],
        });

        // Shadow pass bind group (reuses shadow_buffer)
        let shadow_pass_bind_group = device.create_bind_group(&BindGroupDescriptor {
            label: Some("shadow_pass_bind_group"),
            layout: &shadow_pass_bind_group_layout,
            entries: &[BindGroupEntry {
                binding: 0,
                resource: shadow_buffer.as_entire_binding(),
            }],
        });

        // Create pipeline layout for PBR (with textures)
        // Bind group layout:
        // Group 0: Camera + Environment (per-frame)
        // Group 1: Model (per-object)
        // Group 2: Material + Textures (per-object, combined)
        // Group 3: Light + Shadow (per-frame, combined for 4-group limit)
        let pipeline_layout = device.create_pipeline_layout(&PipelineLayoutDescriptor {
            label: Some("pbr_pipeline_layout"),
            bind_group_layouts: &[
                &camera_env_bind_group_layout,    // group 0: camera + environment
                &uniform_bind_group_layout,       // group 1: model
                &texture_bind_group_layout,       // group 2: material + textures (combined)
                &light_shadow_bind_group_layout,  // group 3: light + shadow (combined)
            ],
            push_constant_ranges: &[],
        });

        // Create pipeline layout for sky (camera + environment)
        let sky_pipeline_layout = device.create_pipeline_layout(&PipelineLayoutDescriptor {
            label: Some("sky_pipeline_layout"),
            bind_group_layouts: &[
                &camera_env_bind_group_layout, // group 0: camera + environment
            ],
            push_constant_ranges: &[],
        });

        // Vertex layout: position (3), normal (3), uv (2) = 8 floats = 32 bytes
        let vertex_layout = VertexBufferLayout {
            array_stride: 32,
            step_mode: VertexStepMode::Vertex,
            attributes: &[
                VertexAttribute {
                    format: VertexFormat::Float32x3,
                    offset: 0,
                    shader_location: 0, // position
                },
                VertexAttribute {
                    format: VertexFormat::Float32x3,
                    offset: 12,
                    shader_location: 1, // normal
                },
                VertexAttribute {
                    format: VertexFormat::Float32x2,
                    offset: 24,
                    shader_location: 2, // uv
                },
            ],
        };

        // Create PBR render pipeline
        let pbr_pipeline = device.create_render_pipeline(&RenderPipelineDescriptor {
            label: Some("pbr_pipeline"),
            layout: Some(&pipeline_layout),
            vertex: VertexState {
                module: &pbr_shader,
                entry_point: "vs_main",
                buffers: &[vertex_layout],
                compilation_options: PipelineCompilationOptions::default(),
            },
            fragment: Some(FragmentState {
                module: &pbr_shader,
                entry_point: "fs_main",
                targets: &[Some(ColorTargetState {
                    format,
                    // Use REPLACE for opaque PBR materials (no alpha blending)
                    // This prevents transparency artifacts from incorrect alpha values
                    blend: Some(BlendState::REPLACE),
                    write_mask: ColorWrites::ALL,
                })],
                compilation_options: PipelineCompilationOptions::default(),
            }),
            primitive: PrimitiveState {
                topology: PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: FrontFace::Ccw,
                cull_mode: Some(Face::Back),
                unclipped_depth: false,
                polygon_mode: PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: Some(DepthStencilState {
                format: TextureFormat::Depth32Float,
                depth_write_enabled: true,
                depth_compare: CompareFunction::Less,
                stencil: StencilState::default(),
                bias: DepthBiasState::default(),
            }),
            multisample: MultisampleState::default(),
            multiview: None,
            cache: None,
        });

        // Create sky pipeline (fullscreen triangle, no vertex buffer)
        let sky_pipeline = device.create_render_pipeline(&RenderPipelineDescriptor {
            label: Some("sky_pipeline"),
            layout: Some(&sky_pipeline_layout),
            vertex: VertexState {
                module: &sky_shader,
                entry_point: "vs_main",
                buffers: &[], // No vertex buffer - uses vertex_index
                compilation_options: PipelineCompilationOptions::default(),
            },
            fragment: Some(FragmentState {
                module: &sky_shader,
                entry_point: "fs_main",
                targets: &[Some(ColorTargetState {
                    format,
                    blend: None,
                    write_mask: ColorWrites::ALL,
                })],
                compilation_options: PipelineCompilationOptions::default(),
            }),
            primitive: PrimitiveState {
                topology: PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: FrontFace::Ccw,
                cull_mode: None, // No culling for fullscreen
                unclipped_depth: false,
                polygon_mode: PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: Some(DepthStencilState {
                format: TextureFormat::Depth32Float,
                depth_write_enabled: false, // Don't write depth
                depth_compare: CompareFunction::LessEqual, // Pass at far depth
                stencil: StencilState::default(),
                bias: DepthBiasState::default(),
            }),
            multisample: MultisampleState::default(),
            multiview: None,
            cache: None,
        });

        // Shadow vertex layout (same structure as PBR)
        let shadow_vertex_layout = VertexBufferLayout {
            array_stride: 32,
            step_mode: VertexStepMode::Vertex,
            attributes: &[
                VertexAttribute {
                    format: VertexFormat::Float32x3,
                    offset: 0,
                    shader_location: 0, // position
                },
                VertexAttribute {
                    format: VertexFormat::Float32x3,
                    offset: 12,
                    shader_location: 1, // normal
                },
                VertexAttribute {
                    format: VertexFormat::Float32x2,
                    offset: 24,
                    shader_location: 2, // uv
                },
            ],
        };

        // Create shadow depth pipeline (depth-only, no color output)
        let shadow_pipeline = device.create_render_pipeline(&RenderPipelineDescriptor {
            label: Some("shadow_pipeline"),
            layout: Some(&shadow_pipeline_layout),
            vertex: VertexState {
                module: &shadow_shader,
                entry_point: "vs_main",
                buffers: &[shadow_vertex_layout],
                compilation_options: PipelineCompilationOptions::default(),
            },
            fragment: Some(FragmentState {
                module: &shadow_shader,
                entry_point: "fs_main",
                targets: &[], // No color output - depth only
                compilation_options: PipelineCompilationOptions::default(),
            }),
            primitive: PrimitiveState {
                topology: PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: FrontFace::Ccw,
                cull_mode: Some(Face::Back),
                unclipped_depth: false,
                polygon_mode: PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: Some(DepthStencilState {
                format: TextureFormat::Depth32Float,
                depth_write_enabled: true,
                depth_compare: CompareFunction::Less,
                stencil: StencilState::default(),
                bias: DepthBiasState {
                    constant: 2,      // Constant bias to reduce shadow acne
                    slope_scale: 2.0, // Slope-scaled bias
                    clamp: 0.0,
                },
            }),
            multisample: MultisampleState::default(),
            multiview: None,
            cache: None,
        });

        // Create built-in meshes
        let meshes = Self::create_builtin_meshes(device);

        // Create depth texture
        let (depth_texture, depth_view) = Self::create_depth_texture(device, size);

        log::info!("SceneRenderer created with PBR, sky, and shadow pipelines");

        Self {
            entities: HashMap::new(),
            camera,
            camera_controller,
            meshes,
            pbr_pipeline: Some(pbr_pipeline),
            pbr_textured_pipeline: None, // TODO: Create textured pipeline
            sky_pipeline: Some(sky_pipeline),
            shadow_pipeline: Some(shadow_pipeline),
            camera_buffer,
            model_buffer,
            material_buffer,
            light_buffer,
            shadow_buffer,
            sky_buffer,
            camera_bind_group,
            camera_env_bind_group_layout,
            particle_camera_bind_group,
            model_bind_group,
            material_bind_group,
            light_bind_group,
            shadow_bind_group: shadow_pass_bind_group,
            shadow_bind_group_layout: shadow_pass_bind_group_layout,
            camera_bind_group_layout: uniform_bind_group_layout,
            texture_bind_group_layout,
            texture_manager,
            default_texture_bind_group,
            environment_handle: TextureHandle::INVALID,
            has_environment_map: false,
            shadow_config,
            shadow_texture: Some(shadow_texture),
            shadow_view: Some(shadow_view),
            shadow_sampler: Some(shadow_sampler),
            depth_texture: Some(depth_texture),
            depth_view: Some(depth_view),
            size,
            light_direction: [0.5, -0.7, 0.5],
            light_color: [1.0, 0.95, 0.85],  // Warm sunlight
            light_intensity: 2.5,
            ambient_intensity: 0.15,
            extractor: RenderExtractor::default(),
            particle_system: None,
            particle_pipeline: None,
            particle_quad_buffer: None,
            time: 0.0,
            input_config: InputConfig::default(),
            sky_config: SkyDef::default(),
            animation_states: HashMap::new(),

            // New phase features
            scene_cameras: Vec::new(),
            active_camera_index: 0,
            scene_lights: Vec::new(),
            scene_shadow_config: crate::scene_loader::ShadowsDef::default(),
            picking_config: crate::scene_loader::PickingDef::default(),
            hovered_entity: None,
            selected_entity: None,
            spatial_config: crate::scene_loader::SpatialDef::default(),
            debug_config: crate::scene_loader::DebugDef::default(),
            debug_stats: DebugStats::default(),
        }
    }

    /// Initialize particle rendering (call after new())
    pub fn init_particles(&mut self, device: &Device, format: TextureFormat) {
        // Create quad vertex buffer for billboarding
        // 6 vertices for 2 triangles: positions only (billboard in shader)
        let quad_vertices: &[f32] = &[
            -1.0, -1.0,  // bottom-left
             1.0, -1.0,  // bottom-right
             1.0,  1.0,  // top-right
            -1.0, -1.0,  // bottom-left
             1.0,  1.0,  // top-right
            -1.0,  1.0,  // top-left
        ];

        self.particle_quad_buffer = Some(device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("particle_quad_buffer"),
            contents: bytemuck::cast_slice(quad_vertices),
            usage: BufferUsages::VERTEX,
        }));

        // Load particle shader
        let shader_source = include_str!("shaders/particle.wgsl");
        let shader_module = device.create_shader_module(ShaderModuleDescriptor {
            label: Some("particle_shader"),
            source: ShaderSource::Wgsl(shader_source.into()),
        });

        // Create particle pipeline
        let pipeline_layout = device.create_pipeline_layout(&PipelineLayoutDescriptor {
            label: Some("particle_pipeline_layout"),
            bind_group_layouts: &[&self.camera_bind_group_layout],
            push_constant_ranges: &[],
        });

        self.particle_pipeline = Some(device.create_render_pipeline(&RenderPipelineDescriptor {
            label: Some("particle_pipeline"),
            layout: Some(&pipeline_layout),
            vertex: VertexState {
                module: &shader_module,
                entry_point: "vs_main",
                compilation_options: Default::default(),
                buffers: &[
                    // Quad vertex buffer (per-vertex)
                    VertexBufferLayout {
                        array_stride: 8, // 2 floats
                        step_mode: VertexStepMode::Vertex,
                        attributes: &[
                            VertexAttribute {
                                format: VertexFormat::Float32x2,
                                offset: 0,
                                shader_location: 0, // quad_pos
                            },
                        ],
                    },
                    // Instance buffer (per-particle)
                    VertexBufferLayout {
                        array_stride: 36, // 9 floats (pos:3 + color:4 + size:1 + age:1)
                        step_mode: VertexStepMode::Instance,
                        attributes: &[
                            VertexAttribute {
                                format: VertexFormat::Float32x3,
                                offset: 0,
                                shader_location: 1, // particle_pos
                            },
                            VertexAttribute {
                                format: VertexFormat::Float32x4,
                                offset: 12,
                                shader_location: 2, // particle_color
                            },
                            VertexAttribute {
                                format: VertexFormat::Float32,
                                offset: 28,
                                shader_location: 3, // particle_size
                            },
                            VertexAttribute {
                                format: VertexFormat::Float32,
                                offset: 32,
                                shader_location: 4, // particle_age
                            },
                        ],
                    },
                ],
            },
            fragment: Some(FragmentState {
                module: &shader_module,
                entry_point: "fs_main",
                compilation_options: Default::default(),
                targets: &[Some(ColorTargetState {
                    format,
                    blend: Some(BlendState {
                        color: BlendComponent {
                            src_factor: BlendFactor::SrcAlpha,
                            dst_factor: BlendFactor::One, // Additive blending
                            operation: BlendOperation::Add,
                        },
                        alpha: BlendComponent {
                            src_factor: BlendFactor::One,
                            dst_factor: BlendFactor::One,
                            operation: BlendOperation::Add,
                        },
                    }),
                    write_mask: ColorWrites::ALL,
                })],
            }),
            primitive: PrimitiveState {
                topology: PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: FrontFace::Ccw,
                cull_mode: None, // Don't cull billboards
                polygon_mode: PolygonMode::Fill,
                unclipped_depth: false,
                conservative: false,
            },
            depth_stencil: Some(DepthStencilState {
                format: TextureFormat::Depth32Float,
                depth_write_enabled: false, // Particles don't write depth
                depth_compare: CompareFunction::Less,
                stencil: StencilState::default(),
                bias: DepthBiasState::default(),
            }),
            multisample: MultisampleState::default(),
            multiview: None,
            cache: None,
        }));

        log::info!("Particle rendering initialized");
    }

    /// Add a particle emitter to the scene
    pub fn add_particle_emitter(&mut self, device: &Device, emitter: ParticleEmitter) {
        let mut system = ParticleSystem::new(emitter);
        system.init_gpu(device);
        self.particle_system = Some(system);
    }

    /// Update particles (call each frame with delta time)
    pub fn update_particles(&mut self, delta_time: f32) {
        self.time += delta_time;
        if let Some(system) = &mut self.particle_system {
            system.update(delta_time);
        }
    }

    /// Load a texture from a file path
    pub fn load_texture(&mut self, device: &Device, queue: &Queue, path: &Path, options: &TextureOptions) -> Result<TextureHandle, String> {
        self.texture_manager.load_from_file(device, queue, path, options)
    }

    /// Load an HDR environment map for reflections
    pub fn load_environment_map(&mut self, device: &Device, queue: &Queue, path: &Path) -> Result<(), String> {
        let handle = self.texture_manager.load_hdr_from_file(device, queue, path)?;
        self.environment_handle = handle;
        self.has_environment_map = true;

        // Recreate camera bind group with the environment texture
        let env_texture = self.texture_manager.get(handle).unwrap();
        self.camera_bind_group = device.create_bind_group(&BindGroupDescriptor {
            label: Some("camera_env_bind_group"),
            layout: &self.camera_env_bind_group_layout,
            entries: &[
                BindGroupEntry {
                    binding: 0,
                    resource: self.camera_buffer.as_entire_binding(),
                },
                BindGroupEntry {
                    binding: 1,
                    resource: BindingResource::TextureView(&env_texture.view),
                },
                BindGroupEntry {
                    binding: 2,
                    resource: BindingResource::Sampler(&env_texture.sampler),
                },
                BindGroupEntry {
                    binding: 3,
                    resource: self.sky_buffer.as_entire_binding(),
                },
            ],
        });

        log::info!("Environment map loaded: {}", path.display());
        Ok(())
    }

    /// Check if environment map is loaded
    pub fn has_environment(&self) -> bool {
        self.has_environment_map
    }

    fn create_depth_texture(device: &Device, size: (u32, u32)) -> (Texture, TextureView) {
        let texture = device.create_texture(&TextureDescriptor {
            label: Some("depth_texture"),
            size: Extent3d {
                width: size.0.max(1),
                height: size.1.max(1),
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format: TextureFormat::Depth32Float,
            usage: TextureUsages::RENDER_ATTACHMENT | TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });

        let view = texture.create_view(&TextureViewDescriptor::default());
        (texture, view)
    }

    /// Create shadow map texture and sampler
    fn create_shadow_map(device: &Device, resolution: u32) -> (Texture, TextureView, Sampler) {
        let texture = device.create_texture(&TextureDescriptor {
            label: Some("shadow_map"),
            size: Extent3d {
                width: resolution,
                height: resolution,
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format: TextureFormat::Depth32Float,
            usage: TextureUsages::RENDER_ATTACHMENT | TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });

        let view = texture.create_view(&TextureViewDescriptor::default());

        // Comparison sampler for PCF shadow sampling
        let sampler = device.create_sampler(&SamplerDescriptor {
            label: Some("shadow_sampler"),
            address_mode_u: AddressMode::ClampToEdge,
            address_mode_v: AddressMode::ClampToEdge,
            address_mode_w: AddressMode::ClampToEdge,
            mag_filter: FilterMode::Linear,
            min_filter: FilterMode::Linear,
            mipmap_filter: FilterMode::Nearest,
            compare: Some(CompareFunction::LessEqual), // Comparison sampler for shadows
            ..Default::default()
        });

        (texture, view, sampler)
    }

    fn create_builtin_meshes(device: &Device) -> HashMap<MeshType, GpuMesh> {
        let mut meshes = HashMap::new();

        // Sphere
        let (vertices, indices) = generate_sphere(32, 24, 0.5);
        meshes.insert(MeshType::Sphere, Self::create_mesh(device, &vertices, &indices));

        // Cube
        let (vertices, indices) = generate_cube(1.0);
        meshes.insert(MeshType::Cube, Self::create_mesh(device, &vertices, &indices));

        // Torus
        let (vertices, indices) = generate_torus(0.4, 0.15, 32, 16);
        meshes.insert(MeshType::Torus, Self::create_mesh(device, &vertices, &indices));

        // Diamond
        let (vertices, indices) = generate_diamond(0.5, 0.8);
        meshes.insert(MeshType::Diamond, Self::create_mesh(device, &vertices, &indices));

        // Plane
        let (vertices, indices) = generate_plane(10.0);
        meshes.insert(MeshType::Plane, Self::create_mesh(device, &vertices, &indices));

        meshes
    }

    fn create_mesh(device: &Device, vertices: &[[f32; 8]], indices: &[u32]) -> GpuMesh {
        let vertex_buffer = device.create_buffer_init(&util::BufferInitDescriptor {
            label: Some("mesh_vertices"),
            contents: bytemuck::cast_slice(vertices),
            usage: BufferUsages::VERTEX,
        });

        let index_buffer = device.create_buffer_init(&util::BufferInitDescriptor {
            label: Some("mesh_indices"),
            contents: bytemuck::cast_slice(indices),
            usage: BufferUsages::INDEX,
        });

        GpuMesh {
            vertex_buffer,
            index_buffer,
            index_count: indices.len() as u32,
        }
    }

    /// Load a glTF/GLB mesh file and create GPU buffers
    /// Returns the mesh type, material properties, and textures if successful
    fn load_gltf_mesh(
        &mut self,
        device: &Device,
        queue: &Queue,
        mesh_path: &Path,
    ) -> Option<GltfLoadResult> {
        let path_str = mesh_path.to_string_lossy().to_string();

        // Check if mesh already loaded (textures may need reloading for different entities)
        let mesh_key = MeshType::Custom(path_str.clone());
        let mesh_already_loaded = self.meshes.contains_key(&mesh_key);

        // Load the glTF file
        let gltf_asset = match GltfLoader::load_file(&path_str) {
            Ok(asset) => asset,
            Err(e) => {
                log::error!("Failed to load glTF {}: {}", path_str, e);
                return None;
            }
        };

        // Load GPU mesh if not already cached
        if !mesh_already_loaded {
            // Collect all vertices and indices from all meshes/primitives
            let mut all_vertices: Vec<[f32; 8]> = Vec::new();
            let mut all_indices: Vec<u32> = Vec::new();

            for mesh in &gltf_asset.meshes {
                for primitive in &mesh.primitives {
                    let base_index = all_vertices.len() as u32;

                    // Convert PbrVertex to [f32; 8] format: [pos(3), normal(3), uv(2)]
                    for v in &primitive.vertices {
                        all_vertices.push([
                            v.position[0], v.position[1], v.position[2],
                            v.normal[0], v.normal[1], v.normal[2],
                            v.uv[0], v.uv[1],
                        ]);
                    }

                    // Offset indices for combined mesh
                    for idx in &primitive.indices {
                        all_indices.push(base_index + idx);
                    }
                }
            }

            if all_vertices.is_empty() || all_indices.is_empty() {
                log::warn!("glTF {} has no geometry", path_str);
                return None;
            }

            log::info!(
                "Loaded glTF {}: {} vertices, {} indices",
                path_str,
                all_vertices.len(),
                all_indices.len()
            );

            // Create GPU mesh
            let gpu_mesh = Self::create_mesh(device, &all_vertices, &all_indices);
            self.meshes.insert(mesh_key.clone(), gpu_mesh);
        }

        // Extract material properties from the first material
        let material = gltf_asset.materials.first();
        let base_color = material.map(|m| m.base_color_factor).unwrap_or([1.0, 1.0, 1.0, 1.0]);
        let metallic = material.map(|m| m.metallic_factor).unwrap_or(0.0);
        let roughness = material.map(|m| m.roughness_factor).unwrap_or(0.5);

        // Load textures from glTF
        let mut textures = PbrTextures::new();

        if let Some(mat) = material {
            // Load base color texture
            if let Some(tex_idx) = mat.base_color_texture {
                if let Some(tex_asset) = gltf_asset.textures.get(tex_idx) {
                    match self.texture_manager.load_from_rgba(
                        device, queue,
                        &tex_asset.data,
                        tex_asset.width,
                        tex_asset.height,
                        &TextureOptions::default(),
                    ) {
                        Ok(handle) => {
                            textures.albedo = handle;
                            log::info!("Loaded glTF albedo texture ({}x{})", tex_asset.width, tex_asset.height);
                        }
                        Err(e) => log::warn!("Failed to load glTF albedo texture: {}", e),
                    }
                }
            }

            // Load normal texture
            if let Some(tex_idx) = mat.normal_texture {
                if let Some(tex_asset) = gltf_asset.textures.get(tex_idx) {
                    match self.texture_manager.load_from_rgba(
                        device, queue,
                        &tex_asset.data,
                        tex_asset.width,
                        tex_asset.height,
                        &TextureOptions::normal_map(),
                    ) {
                        Ok(handle) => {
                            textures.normal = handle;
                            log::info!("Loaded glTF normal texture ({}x{})", tex_asset.width, tex_asset.height);
                        }
                        Err(e) => log::warn!("Failed to load glTF normal texture: {}", e),
                    }
                }
            }

            // Load metallic-roughness texture
            if let Some(tex_idx) = mat.metallic_roughness_texture {
                if let Some(tex_asset) = gltf_asset.textures.get(tex_idx) {
                    match self.texture_manager.load_from_rgba(
                        device, queue,
                        &tex_asset.data,
                        tex_asset.width,
                        tex_asset.height,
                        &TextureOptions::linear(),
                    ) {
                        Ok(handle) => {
                            textures.metallic_roughness = handle;
                            log::info!("Loaded glTF metallic-roughness texture ({}x{})", tex_asset.width, tex_asset.height);
                        }
                        Err(e) => log::warn!("Failed to load glTF metallic-roughness texture: {}", e),
                    }
                }
            }
        }

        Some(GltfLoadResult {
            mesh_type: mesh_key,
            base_color,
            metallic,
            roughness,
            textures,
        })
    }

    /// Resize the renderer
    pub fn resize(&mut self, device: &Device, new_size: (u32, u32)) {
        if new_size.0 > 0 && new_size.1 > 0 {
            self.size = new_size;
            let (depth_texture, depth_view) = Self::create_depth_texture(device, new_size);
            self.depth_texture = Some(depth_texture);
            self.depth_view = Some(depth_view);

            // Update camera aspect ratio
            if let Projection::Perspective { ref mut aspect, .. } = self.camera.projection {
                *aspect = new_size.0 as f32 / new_size.1 as f32;
            }
        }
    }

    /// Add or update an entity
    pub fn set_entity(&mut self, entity: SceneEntity) {
        self.entities.insert(entity.entity_id, entity);
    }

    /// Remove an entity
    pub fn remove_entity(&mut self, entity_id: u64) {
        self.entities.remove(&entity_id);
    }

    /// Update camera from input
    pub fn update_camera(&mut self, input: &CameraInput, delta_time: f32) {
        self.camera_controller.update(&mut self.camera, input, delta_time);
    }

    /// Update all entity animations
    pub fn update_animations(&mut self, delta_time: f32) {
        // Collect entity IDs that have animations
        let entity_ids: Vec<u64> = self.entities.keys().copied().collect();

        for entity_id in entity_ids {
            // Get animation and base transform (need to clone to avoid borrow issues)
            let (animation, base_transform) = {
                let entity = self.entities.get(&entity_id);
                match entity {
                    Some(e) => (e.animation.clone(), e.base_transform.clone()),
                    None => continue,
                }
            };

            if let Some(anim) = animation {
                // Get or create animation state and update time
                let state = self.animation_states.entry(entity_id).or_default();
                state.time += delta_time;
                let mut state_copy = state.clone();

                // Calculate new transform based on animation type
                let new_transform = Self::calculate_animated_transform_static(&anim, &base_transform, &mut state_copy);

                // Update state if it changed (for path animation segment tracking)
                if let Some(state) = self.animation_states.get_mut(&entity_id) {
                    *state = state_copy;
                }

                // Apply to entity
                if let Some(entity) = self.entities.get_mut(&entity_id) {
                    entity.transform = new_transform;
                }
            }
        }
    }

    /// Calculate the animated transform for an entity (static version to avoid borrow issues)
    fn calculate_animated_transform_static(
        animation: &AnimationDef,
        base: &Transform,
        state: &mut AnimationState,
    ) -> Transform {
        match animation {
            AnimationDef::Rotate(rot) => Self::apply_rotate_animation(rot, base, state.time),
            AnimationDef::Oscillate(osc) => Self::apply_oscillate_animation(osc, base, state.time),
            AnimationDef::Path(path) => Self::apply_path_animation(path, base, state),
            AnimationDef::Orbit(orbit) => Self::apply_orbit_animation(orbit, base, state.time),
            AnimationDef::Pulse(pulse) => Self::apply_pulse_animation(pulse, base, state.time),
        }
    }

    fn apply_rotate_animation(rot: &RotateAnimation, base: &Transform, time: f32) -> Transform {
        let angle = time * rot.speed;
        let axis = Vec3::new(rot.axis[0], rot.axis[1], rot.axis[2]).normalize();
        let rotation_quat = Quat::from_axis_angle(axis, angle);

        // Combine with base rotation
        let base_quat = Quat::new(
            base.rotation[0], base.rotation[1], base.rotation[2], base.rotation[3]
        );
        let final_quat = rotation_quat * base_quat;

        Transform {
            position: base.position,
            rotation: [final_quat.x, final_quat.y, final_quat.z, final_quat.w],
            scale: base.scale,
        }
    }

    fn apply_oscillate_animation(osc: &OscillateAnimation, base: &Transform, time: f32) -> Transform {
        let phase = (time * osc.frequency * std::f32::consts::TAU) + (osc.phase * std::f32::consts::TAU);
        let offset = phase.sin() * osc.amplitude;

        if osc.rotate {
            // Oscillate rotation
            let axis = Vec3::new(osc.axis[0], osc.axis[1], osc.axis[2]).normalize();
            let rotation_quat = Quat::from_axis_angle(axis, offset);
            let base_quat = Quat::new(
                base.rotation[0], base.rotation[1], base.rotation[2], base.rotation[3]
            );
            let final_quat = rotation_quat * base_quat;

            Transform {
                position: base.position,
                rotation: [final_quat.x, final_quat.y, final_quat.z, final_quat.w],
                scale: base.scale,
            }
        } else {
            // Oscillate position
            let axis = Vec3::new(osc.axis[0], osc.axis[1], osc.axis[2]).normalize();
            Transform {
                position: [
                    base.position[0] + axis.x * offset,
                    base.position[1] + axis.y * offset,
                    base.position[2] + axis.z * offset,
                ],
                rotation: base.rotation,
                scale: base.scale,
            }
        }
    }

    fn apply_path_animation(path: &PathAnimation, base: &Transform, state: &mut AnimationState) -> Transform {
        if path.points.is_empty() {
            return base.clone();
        }

        let total_time = path.duration;
        let mut t = state.time / total_time;

        // Handle looping and ping-pong
        if path.loop_animation {
            if path.ping_pong {
                // Ping-pong: 0->1->0->1...
                let cycle = (t as i32) % 2;
                t = t.fract();
                if cycle == 1 {
                    t = 1.0 - t;
                }
            } else {
                t = t.fract();
            }
        } else {
            t = t.clamp(0.0, 1.0);
        }

        // Apply easing
        t = path.easing.apply(t);

        // Interpolate along path
        let num_segments = path.points.len() - 1;
        if num_segments == 0 {
            return Transform {
                position: path.points[0],
                rotation: base.rotation,
                scale: base.scale,
            };
        }

        let segment_t = t * num_segments as f32;
        let segment_idx = (segment_t as usize).min(num_segments - 1);
        let local_t = segment_t - segment_idx as f32;

        let p0 = path.points[segment_idx];
        let p1 = path.points[(segment_idx + 1).min(path.points.len() - 1)];

        // Interpolate position
        let position = match path.interpolation {
            InterpolationMode::Linear => [
                p0[0] + (p1[0] - p0[0]) * local_t,
                p0[1] + (p1[1] - p0[1]) * local_t,
                p0[2] + (p1[2] - p0[2]) * local_t,
            ],
            InterpolationMode::CatmullRom => {
                // Simplified - use linear for now, full catmull-rom needs 4 points
                [
                    p0[0] + (p1[0] - p0[0]) * local_t,
                    p0[1] + (p1[1] - p0[1]) * local_t,
                    p0[2] + (p1[2] - p0[2]) * local_t,
                ]
            }
            InterpolationMode::Bezier => {
                // Simplified - use smooth step
                let smooth_t = local_t * local_t * (3.0 - 2.0 * local_t);
                [
                    p0[0] + (p1[0] - p0[0]) * smooth_t,
                    p0[1] + (p1[1] - p0[1]) * smooth_t,
                    p0[2] + (p1[2] - p0[2]) * smooth_t,
                ]
            }
            InterpolationMode::Step => p0,
        };

        // Orient to path direction if enabled
        let rotation = if path.orient_to_path && local_t > 0.01 {
            let dir = Vec3::new(p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]);
            if dir.length_squared() > 0.0001 {
                let dir = dir.normalize();
                // Calculate rotation to face direction
                let forward = Vec3::new(0.0, 0.0, -1.0);
                let dot = forward.dot(dir);
                if dot.abs() < 0.9999 {
                    let axis = forward.cross(dir).normalize();
                    let angle = dot.acos();
                    let q = Quat::from_axis_angle(axis, angle);
                    [q.x, q.y, q.z, q.w]
                } else if dot < 0.0 {
                    // Facing opposite direction
                    [0.0, 1.0, 0.0, 0.0]
                } else {
                    base.rotation
                }
            } else {
                base.rotation
            }
        } else {
            base.rotation
        };

        Transform {
            position,
            rotation,
            scale: base.scale,
        }
    }

    fn apply_orbit_animation(orbit: &OrbitAnimation, base: &Transform, time: f32) -> Transform {
        let angle = orbit.start_angle + time * orbit.speed;
        let axis = Vec3::new(orbit.axis[0], orbit.axis[1], orbit.axis[2]).normalize();

        // Calculate position on orbit circle
        // Default orbit is in XZ plane when axis is Y
        let (right, forward) = if axis.y.abs() > 0.99 {
            (Vec3::new(1.0, 0.0, 0.0), Vec3::new(0.0, 0.0, 1.0))
        } else {
            let right = Vec3::Y.cross(axis).normalize();
            let forward = axis.cross(right).normalize();
            (right, forward)
        };

        let x = angle.cos() * orbit.radius;
        let z = angle.sin() * orbit.radius;
        let offset = right * x + forward * z;

        let position = [
            orbit.center[0] + offset.x,
            orbit.center[1] + offset.y,
            orbit.center[2] + offset.z,
        ];

        // Face center if enabled
        let rotation = if orbit.face_center {
            let to_center = Vec3::new(
                orbit.center[0] - position[0],
                orbit.center[1] - position[1],
                orbit.center[2] - position[2],
            );
            if to_center.length_squared() > 0.0001 {
                let dir = to_center.normalize();
                let forward = Vec3::new(0.0, 0.0, -1.0);
                let dot = forward.dot(dir);
                if dot.abs() < 0.9999 {
                    let axis = forward.cross(dir).normalize();
                    let angle = dot.acos();
                    let q = Quat::from_axis_angle(axis, angle);
                    [q.x, q.y, q.z, q.w]
                } else {
                    base.rotation
                }
            } else {
                base.rotation
            }
        } else {
            base.rotation
        };

        Transform {
            position,
            rotation,
            scale: base.scale,
        }
    }

    fn apply_pulse_animation(pulse: &PulseAnimation, base: &Transform, time: f32) -> Transform {
        let phase = (time * pulse.frequency * std::f32::consts::TAU) + (pulse.phase * std::f32::consts::TAU);
        let t = (phase.sin() + 1.0) * 0.5;  // 0 to 1
        let scale_mult = pulse.min_scale + (pulse.max_scale - pulse.min_scale) * t;

        Transform {
            position: base.position,
            rotation: base.rotation,
            scale: [
                base.scale[0] * scale_mult,
                base.scale[1] * scale_mult,
                base.scale[2] * scale_mult,
            ],
        }
    }

    /// Get visible entities
    pub fn visible_entities(&self) -> impl Iterator<Item = &SceneEntity> {
        self.entities.values().filter(|e| e.visible)
    }

    /// Get entity count
    pub fn entity_count(&self) -> usize {
        self.entities.len()
    }

    // ========================================================================
    // Scene Loading from TOML
    // ========================================================================

    /// Apply a scene definition loaded from scene.toml
    /// This replaces hardcoded scene setup with declarative configuration
    pub fn apply_scene(
        &mut self,
        device: &Device,
        queue: &Queue,
        format: TextureFormat,
        scene: &SceneDefinition,
        asset_base_path: &Path,
    ) {
        log::info!("Applying scene: {}", scene.scene.name);

        // Apply cameras (Phase 2)
        self.apply_cameras(&scene.cameras);

        // Apply lights (Phase 5)
        self.apply_lights(&scene.lights);

        // Apply shadow configuration (Phase 6)
        self.scene_shadow_config = scene.shadows.clone();
        log::info!("Shadow config: enabled={}, cascades={}",
            self.scene_shadow_config.enabled,
            self.scene_shadow_config.cascades.count);

        // Apply picking configuration (Phase 10)
        self.picking_config = scene.picking.clone();
        log::info!("Picking config: enabled={}, method={:?}",
            self.picking_config.enabled,
            self.picking_config.method);

        // Apply spatial configuration (Phase 14)
        self.spatial_config = scene.spatial.clone();

        // Apply debug configuration (Phase 18)
        self.debug_config = scene.debug.clone();

        // Apply environment settings
        self.apply_environment(&scene.environment, device, queue, asset_base_path);

        // Apply input configuration
        self.apply_input_config(&scene.input);

        // Create entities
        for (idx, entity_def) in scene.entities.iter().enumerate() {
            let entity_id = idx as u64 + 1;
            if let Some(entity) = self.create_entity_from_def(device, queue, entity_def, entity_id, asset_base_path) {
                // Initialize animation state if entity has animation
                if entity.animation.is_some() {
                    self.animation_states.insert(entity_id, AnimationState::default());
                }
                self.set_entity(entity);
            }
        }
        log::info!("Created {} entities from scene", self.entity_count());

        // Initialize particles if we have emitters
        if !scene.particle_emitters.is_empty() {
            self.init_particles(device, format);

            // Create particle emitters
            for emitter_def in &scene.particle_emitters {
                let emitter = self.create_emitter_from_def(emitter_def);
                self.add_particle_emitter(device, emitter);
                log::info!("Created particle emitter: {}", emitter_def.name);
            }
        }
    }

    /// Apply input configuration
    fn apply_input_config(&mut self, config: &InputConfig) {
        self.input_config = config.clone();

        // Apply camera settings
        let cam = &config.camera;
        self.camera_controller.configure(
            cam.orbit_sensitivity,
            cam.pan_sensitivity,
            cam.zoom_sensitivity,
            cam.min_distance,
            cam.max_distance,
            cam.invert_x,
            cam.invert_y,
        );

        log::info!("Applied input configuration");
    }

    /// Apply camera definitions from scene (Phase 2)
    fn apply_cameras(&mut self, cameras: &[crate::scene_loader::CameraDef]) {
        use crate::scene_loader::{CameraType, CameraControlMode};

        self.scene_cameras = cameras.to_vec();

        // Find and apply the active camera
        for (idx, cam_def) in cameras.iter().enumerate() {
            if cam_def.active {
                self.active_camera_index = idx;

                // Apply camera transform
                self.camera.position = Vec3::new(
                    cam_def.transform.position[0],
                    cam_def.transform.position[1],
                    cam_def.transform.position[2],
                );

                let target = Vec3::new(
                    cam_def.transform.target[0],
                    cam_def.transform.target[1],
                    cam_def.transform.target[2],
                );

                // Apply camera control mode
                match cam_def.control_mode {
                    CameraControlMode::Fps => {
                        self.camera_controller.mode = CameraMode::Fps;
                        // For FPS mode, set initial direction from position to target
                        let forward = (target - self.camera.position).normalize();
                        self.camera_controller.set_direction(forward);
                        log::info!("Camera mode: FPS (first-person)");
                    }
                    CameraControlMode::Orbit => {
                        self.camera_controller.mode = CameraMode::Orbit;
                        self.camera_controller.orbit_target = target;
                        let diff = self.camera.position - target;
                        self.camera_controller.orbit_distance = diff.length();
                        self.camera_controller.yaw = diff.x.atan2(diff.z);
                        self.camera_controller.pitch = (diff.y / diff.length()).asin();
                        log::info!("Camera mode: Orbit");
                    }
                    CameraControlMode::Fly => {
                        self.camera_controller.mode = CameraMode::Fly;
                        let forward = (target - self.camera.position).normalize();
                        self.camera_controller.set_direction(forward);
                        log::info!("Camera mode: Fly (6DOF)");
                    }
                }

                // Legacy: also set orbit values for backwards compatibility
                self.camera_controller.orbit_target = target;
                let diff = self.camera.position - target;
                self.camera_controller.orbit_distance = diff.length();

                // Apply projection settings
                match cam_def.camera_type {
                    CameraType::Perspective => {
                        if let Some(persp) = &cam_def.perspective {
                            self.camera = Camera::perspective(
                                persp.fov.to_radians(),
                                self.size.0 as f32 / self.size.1 as f32,
                                persp.near,
                                persp.far,
                            );
                            self.camera.position = Vec3::new(
                                cam_def.transform.position[0],
                                cam_def.transform.position[1],
                                cam_def.transform.position[2],
                            );
                        }
                    }
                    CameraType::Orthographic => {
                        if let Some(ortho) = &cam_def.orthographic {
                            // Calculate width and height from left/right/bottom/top
                            let width = ortho.right - ortho.left;
                            let height = ortho.top - ortho.bottom;
                            self.camera = Camera::orthographic(
                                width, height,
                                ortho.near, ortho.far,
                            );
                            self.camera.position = Vec3::new(
                                cam_def.transform.position[0],
                                cam_def.transform.position[1],
                                cam_def.transform.position[2],
                            );
                        }
                    }
                }

                log::info!("Active camera: {} ({:?})", cam_def.name, cam_def.camera_type);
                break;
            }
        }

        if cameras.is_empty() {
            log::info!("No cameras in scene, using default camera");
        } else {
            log::info!("Loaded {} camera(s) from scene", cameras.len());
        }
    }

    /// Switch to a different camera by index
    pub fn switch_camera(&mut self, index: usize) {
        if index < self.scene_cameras.len() {
            self.active_camera_index = index;
            let cam_def = &self.scene_cameras[index];

            self.camera.position = Vec3::new(
                cam_def.transform.position[0],
                cam_def.transform.position[1],
                cam_def.transform.position[2],
            );

            let target = Vec3::new(
                cam_def.transform.target[0],
                cam_def.transform.target[1],
                cam_def.transform.target[2],
            );

            self.camera_controller.orbit_target = target;
            let diff = self.camera.position - target;
            self.camera_controller.orbit_distance = diff.length();

            log::info!("Switched to camera: {}", cam_def.name);
        }
    }

    /// Get list of camera names
    pub fn camera_names(&self) -> Vec<&str> {
        self.scene_cameras.iter().map(|c| c.name.as_str()).collect()
    }

    /// Apply light definitions from scene (Phase 5)
    fn apply_lights(&mut self, lights: &[crate::scene_loader::LightDef]) {
        use crate::scene_loader::LightType;

        self.scene_lights.clear();

        for light_def in lights {
            if !light_def.enabled {
                continue;
            }

            let scene_light = match &light_def.light_type {
                LightType::Directional => {
                    if let Some(dir) = &light_def.directional {
                        // Also update the main light direction for backwards compatibility
                        self.light_direction = dir.direction;
                        self.light_color = dir.color;
                        self.light_intensity = dir.intensity;

                        SceneLight {
                            name: light_def.name.clone(),
                            light_type: SceneLightType::Directional {
                                direction: dir.direction,
                            },
                            enabled: true,
                            color: dir.color,
                            intensity: dir.intensity,
                            cast_shadows: dir.cast_shadows,
                        }
                    } else {
                        continue;
                    }
                }
                LightType::Point => {
                    if let Some(point) = &light_def.point {
                        SceneLight {
                            name: light_def.name.clone(),
                            light_type: SceneLightType::Point {
                                position: point.position,
                                range: point.range,
                                attenuation: [
                                    point.attenuation.constant,
                                    point.attenuation.linear,
                                    point.attenuation.quadratic,
                                ],
                            },
                            enabled: true,
                            color: point.color,
                            intensity: point.intensity,
                            cast_shadows: point.cast_shadows,
                        }
                    } else {
                        continue;
                    }
                }
                LightType::Spot => {
                    if let Some(spot) = &light_def.spot {
                        SceneLight {
                            name: light_def.name.clone(),
                            light_type: SceneLightType::Spot {
                                position: spot.position,
                                direction: spot.direction,
                                range: spot.range,
                                inner_angle: spot.inner_angle.to_radians(),
                                outer_angle: spot.outer_angle.to_radians(),
                            },
                            enabled: true,
                            color: spot.color,
                            intensity: spot.intensity,
                            cast_shadows: spot.cast_shadows,
                        }
                    } else {
                        continue;
                    }
                }
                LightType::Area => {
                    log::warn!("Area lights not yet supported: {}", light_def.name);
                    continue;
                }
            };

            log::info!("Loaded light: {} ({:?})", scene_light.name,
                match &scene_light.light_type {
                    SceneLightType::Directional { .. } => "directional",
                    SceneLightType::Point { .. } => "point",
                    SceneLightType::Spot { .. } => "spot",
                });
            self.scene_lights.push(scene_light);
        }

        log::info!("Loaded {} light(s) from scene", self.scene_lights.len());
    }

    /// Get the scene lights for rendering
    pub fn lights(&self) -> &[SceneLight] {
        &self.scene_lights
    }

    /// Apply environment settings from scene definition
    fn apply_environment(
        &mut self,
        env: &crate::scene_loader::EnvironmentDef,
        device: &Device,
        queue: &Queue,
        asset_base_path: &Path,
    ) {
        // Apply lighting
        self.light_direction = env.light_direction;
        self.light_color = env.light_color;
        self.light_intensity = env.light_intensity;
        self.ambient_intensity = env.ambient_intensity;

        // Apply sky configuration
        self.sky_config = env.sky.clone();

        // Load environment map if specified
        if let Some(env_map_path) = &env.environment_map {
            let full_path = asset_base_path.join(env_map_path);
            if full_path.exists() {
                match self.load_environment_map(device, queue, &full_path) {
                    Ok(_) => log::info!("Loaded environment map: {}", full_path.display()),
                    Err(e) => log::warn!("Failed to load environment map: {}", e),
                }
            } else {
                log::warn!("Environment map not found: {}", full_path.display());
            }
        }
    }

    /// Create a SceneEntity from an EntityDef
    fn create_entity_from_def(
        &mut self,
        device: &Device,
        queue: &Queue,
        def: &EntityDef,
        entity_id: u64,
        asset_base_path: &Path,
    ) -> Option<SceneEntity> {
        // Convert mesh definition and get material/textures from glTF if applicable
        let (mesh, gltf_material) = match &def.mesh {
            MeshDef::Primitive(p) => {
                let mesh_type = match p {
                    PrimitiveMesh::Sphere => MeshType::Sphere,
                    PrimitiveMesh::Cube => MeshType::Cube,
                    PrimitiveMesh::Plane => MeshType::Plane,
                    PrimitiveMesh::Torus => MeshType::Torus,
                    PrimitiveMesh::Diamond => MeshType::Diamond,
                    PrimitiveMesh::Cylinder => MeshType::Sphere, // TODO: Add cylinder
                    PrimitiveMesh::Cone => MeshType::Sphere,     // TODO: Add cone
                };
                (mesh_type, None)
            },
            MeshDef::File { path } => {
                // Resolve path relative to asset base
                let full_path = asset_base_path.join(path);
                match self.load_gltf_mesh(device, queue, &full_path) {
                    Some(result) => (result.mesh_type.clone(), Some(result)),
                    None => {
                        log::warn!("Failed to load mesh {}, using sphere", path);
                        (MeshType::Sphere, None)
                    }
                }
            }
        };

        // Load textures - use glTF textures if available, otherwise load from material definition
        let textures = if let Some(ref gltf) = gltf_material {
            gltf.textures.clone()
        } else {
            self.load_material_textures(device, queue, &def.material, asset_base_path)
        };

        // Convert transform - convert Euler angles (degrees) to quaternion
        let scale = def.transform.scale.to_array();
        let euler_deg = def.transform.rotation;
        let euler_rad = [
            euler_deg[0].to_radians(),
            euler_deg[1].to_radians(),
            euler_deg[2].to_radians(),
        ];
        // Use YXZ order which is common for 3D rotation (yaw-pitch-roll)
        let rotation_quat = Quat::from_euler_yxz(euler_rad[1], euler_rad[0], euler_rad[2]);
        let transform = Transform {
            position: def.transform.position,
            rotation: [rotation_quat.x, rotation_quat.y, rotation_quat.z, rotation_quat.w],
            scale,
        };

        // Convert material - use glTF material properties if available
        let material = if let Some(ref gltf) = gltf_material {
            MaterialData {
                base_color: gltf.base_color,
                metallic: gltf.metallic,
                roughness: gltf.roughness,
                emissive: def.material.emissive, // Keep emissive from scene def
                textures,
                ..Default::default()
            }
        } else {
            MaterialData {
                base_color: def.material.albedo.to_color(),
                metallic: def.material.metallic.to_value(),
                roughness: def.material.roughness.to_value(),
                emissive: def.material.emissive,
                textures,
                ..Default::default()
            }
        };

        Some(SceneEntity {
            entity_id,
            transform: transform.clone(),
            base_transform: transform,
            material,
            mesh,
            layer: def.layer.clone(),
            visible: def.visible,
            animation: def.animation.clone(),
        })
    }

    /// Load textures referenced by a material definition
    fn load_material_textures(
        &mut self,
        device: &Device,
        queue: &Queue,
        mat: &crate::scene_loader::MaterialDef,
        asset_base_path: &Path,
    ) -> PbrTextures {
        let mut textures = PbrTextures::new();

        // Load albedo texture
        if let Some(path) = mat.albedo.texture_path() {
            let full_path = asset_base_path.join(path);
            log::info!("Looking for albedo texture: {} (exists: {})", full_path.display(), full_path.exists());
            if full_path.exists() {
                match self.texture_manager.load_from_file(
                    device, queue, &full_path, &TextureOptions::default()
                ) {
                    Ok(handle) => {
                        textures.albedo = handle;
                        log::info!("Loaded albedo: {}", path);
                    }
                    Err(e) => {
                        log::error!("Failed to load albedo {}: {}", path, e);
                    }
                }
            } else {
                log::warn!("Albedo texture not found: {}", full_path.display());
            }
        }

        // Load normal map
        if let Some(path) = &mat.normal_map {
            let full_path = asset_base_path.join(path);
            if full_path.exists() {
                if let Ok(handle) = self.texture_manager.load_from_file(
                    device, queue, &full_path, &TextureOptions::normal_map()
                ) {
                    textures.normal = handle;
                    log::info!("Loaded normal: {}", path);
                }
            }
        }

        // Load roughness texture (stored in metallic_roughness channel)
        if let Some(path) = mat.roughness.texture_path() {
            let full_path = asset_base_path.join(path);
            if full_path.exists() {
                if let Ok(handle) = self.texture_manager.load_from_file(
                    device, queue, &full_path, &TextureOptions::linear()
                ) {
                    textures.metallic_roughness = handle;
                    log::info!("Loaded roughness: {}", path);
                }
            }
        }

        // Load metallic texture (using roughness path pattern if metallic has texture)
        if let Some(path) = mat.metallic.texture_path() {
            let full_path = asset_base_path.join(path);
            if full_path.exists() {
                if let Ok(handle) = self.texture_manager.load_from_file(
                    device, queue, &full_path, &TextureOptions::linear()
                ) {
                    textures.metallic_roughness = handle;
                    log::info!("Loaded metallic: {}", path);
                }
            }
        }

        // Load AO texture
        if let Some(path) = &mat.ao_map {
            let full_path = asset_base_path.join(path);
            if full_path.exists() {
                if let Ok(handle) = self.texture_manager.load_from_file(
                    device, queue, &full_path, &TextureOptions::linear()
                ) {
                    textures.ao = handle;
                    log::info!("Loaded AO: {}", path);
                }
            }
        }

        textures
    }

    /// Create a ParticleEmitter from a ParticleEmitterDef
    fn create_emitter_from_def(&self, def: &ParticleEmitterDef) -> ParticleEmitter {
        ParticleEmitter {
            position: def.position,
            emit_rate: def.emit_rate,
            max_particles: def.max_particles,
            lifetime_min: def.lifetime[0],
            lifetime_max: def.lifetime[1],
            speed_min: def.speed[0],
            speed_max: def.speed[1],
            size_min: def.size[0],
            size_max: def.size[1],
            color_start: def.color_start,
            color_end: def.color_end,
            gravity: def.gravity,
            spread: def.spread,
            direction: def.direction,
            enabled: def.enabled,
            ..Default::default()
        }
    }

    /// Update uniforms and render the scene
    pub fn render(
        &mut self,
        device: &Device,
        queue: &Queue,
        view: &TextureView,
        frame: u64,
    ) {
        // Update camera uniforms
        let view_mat = self.camera.view_matrix();
        let proj_mat = self.camera.projection_matrix();
        let view_proj = proj_mat * view_mat;

        #[repr(C)]
        #[derive(Copy, Clone, bytemuck::Pod, bytemuck::Zeroable)]
        struct CameraUniforms {
            view_proj: [[f32; 4]; 4],
            view: [[f32; 4]; 4],
            projection: [[f32; 4]; 4],
            camera_pos: [f32; 3],
            _padding: f32,
        }

        let camera_uniforms = CameraUniforms {
            view_proj: view_proj.to_cols_array_2d(),
            view: view_mat.to_cols_array_2d(),
            projection: proj_mat.to_cols_array_2d(),
            camera_pos: self.camera.position.to_array(),
            _padding: 0.0,
        };

        queue.write_buffer(&self.camera_buffer, 0, bytemuck::bytes_of(&camera_uniforms));

        // Update light uniforms
        #[repr(C)]
        #[derive(Copy, Clone, bytemuck::Pod, bytemuck::Zeroable)]
        struct LightUniforms {
            direction: [f32; 3],
            _pad1: f32,
            color: [f32; 3],
            intensity: f32,
            ambient_color: [f32; 3],
            ambient_intensity: f32,
        }

        let light_uniforms = LightUniforms {
            direction: self.light_direction,
            _pad1: 0.0,
            color: self.light_color,
            intensity: self.light_intensity,
            ambient_color: [0.3, 0.35, 0.5],
            ambient_intensity: self.ambient_intensity,
        };

        queue.write_buffer(&self.light_buffer, 0, bytemuck::bytes_of(&light_uniforms));

        // Update sky uniforms (for procedural sky when no HDR map is loaded)
        #[repr(C)]
        #[derive(Copy, Clone, bytemuck::Pod, bytemuck::Zeroable)]
        struct SkyUniforms {
            zenith_color: [f32; 3],
            _pad0: f32,
            horizon_color: [f32; 3],
            _pad1: f32,
            ground_color: [f32; 3],
            _pad2: f32,
            sun_direction: [f32; 3],
            sun_size: f32,
            sun_intensity: f32,
            sun_falloff: f32,
            fog_density: f32,
            use_environment_map: f32,
        }

        let sky_uniforms = SkyUniforms {
            zenith_color: [0.2, 0.4, 0.9],     // Deep blue
            _pad0: 0.0,
            horizon_color: [0.7, 0.8, 1.0],   // Light blue
            _pad1: 0.0,
            ground_color: [0.15, 0.12, 0.1],  // Warm brown
            _pad2: 0.0,
            sun_direction: [-0.5, -0.7, 0.5], // Matching light direction
            sun_size: 0.03,                   // Angular size (radians)
            sun_intensity: 5.0,               // HDR sun brightness
            sun_falloff: 16.0,                // Glow falloff
            fog_density: 0.0,                 // No fog by default
            use_environment_map: if self.has_environment_map { 1.0 } else { 0.0 },
        };

        queue.write_buffer(&self.sky_buffer, 0, bytemuck::bytes_of(&sky_uniforms));

        // Collect visible entities for drawing (clone to avoid borrow conflicts)
        let visible_entities: Vec<SceneEntity> = self.entities.values()
            .filter(|e| e.visible)
            .cloned()
            .collect();

        // ========== SHADOW PASS ==========
        if self.shadow_config.enabled {
            // Calculate light space matrix (orthographic from light direction)
            let light_dir = Vec3::new(
                self.light_direction[0],
                self.light_direction[1],
                self.light_direction[2]
            ).normalize();

            // Create orthographic projection covering the scene (wgpu depth [0,1])
            let shadow_distance = self.shadow_config.distance;
            let light_proj = Mat4::orthographic_rh_zo(
                -shadow_distance, shadow_distance,
                -shadow_distance, shadow_distance,
                0.1, shadow_distance * 2.0
            );

            // Light view matrix - look from above the scene center
            let light_pos = Vec3::ZERO - light_dir * shadow_distance;
            let light_view = Mat4::look_at(light_pos, Vec3::ZERO, Vec3::Y);
            let light_view_proj = light_proj * light_view;

            // Update shadow uniforms
            #[repr(C)]
            #[derive(Copy, Clone, bytemuck::Pod, bytemuck::Zeroable)]
            struct ShadowUniforms {
                light_view_proj: [[f32; 4]; 4],
                config: [f32; 4], // bias, normal_bias, softness, enabled
            }

            let shadow_uniforms = ShadowUniforms {
                light_view_proj: light_view_proj.to_cols_array_2d(),
                config: [
                    self.shadow_config.bias,
                    self.shadow_config.normal_bias,
                    self.shadow_config.softness,
                    1.0, // enabled
                ],
            };

            queue.write_buffer(&self.shadow_buffer, 0, bytemuck::bytes_of(&shadow_uniforms));

            // Render shadow pass - each entity needs its own command buffer
            // to ensure buffer updates are synchronized with draw calls
            if let (Some(shadow_pipeline), Some(shadow_view)) = (&self.shadow_pipeline, &self.shadow_view) {
                // First pass: clear the shadow map
                {
                    let mut encoder = device.create_command_encoder(&CommandEncoderDescriptor {
                        label: Some("shadow_clear_encoder"),
                    });

                    {
                        let _shadow_pass = encoder.begin_render_pass(&RenderPassDescriptor {
                            label: Some("shadow_clear_pass"),
                            color_attachments: &[],
                            depth_stencil_attachment: Some(RenderPassDepthStencilAttachment {
                                view: shadow_view,
                                depth_ops: Some(Operations {
                                    load: LoadOp::Clear(1.0),
                                    store: StoreOp::Store,
                                }),
                                stencil_ops: None,
                            }),
                            occlusion_query_set: None,
                            timestamp_writes: None,
                        });
                        // Pass ends immediately - just clears the depth
                    }

                    queue.submit(std::iter::once(encoder.finish()));
                }

                // Render each entity with its own command buffer for proper buffer sync
                for entity in &visible_entities {
                    // Skip the plane - it doesn't cast shadows
                    if entity.mesh == MeshType::Plane {
                        continue;
                    }

                    if let Some(mesh) = self.meshes.get(&entity.mesh) {
                        // Update model matrix for this entity
                        let model_data = [
                            entity.transform.to_matrix(),
                            Mat4::IDENTITY.to_cols_array_2d(),
                        ];
                        queue.write_buffer(&self.model_buffer, 0, bytemuck::cast_slice(&model_data));

                        let mut encoder = device.create_command_encoder(&CommandEncoderDescriptor {
                            label: Some("shadow_entity_encoder"),
                        });

                        {
                            let mut shadow_pass = encoder.begin_render_pass(&RenderPassDescriptor {
                                label: Some("shadow_entity_pass"),
                                color_attachments: &[],
                                depth_stencil_attachment: Some(RenderPassDepthStencilAttachment {
                                    view: shadow_view,
                                    depth_ops: Some(Operations {
                                        load: LoadOp::Load, // Keep previous depth
                                        store: StoreOp::Store,
                                    }),
                                    stencil_ops: None,
                                }),
                                occlusion_query_set: None,
                                timestamp_writes: None,
                            });

                            shadow_pass.set_pipeline(shadow_pipeline);
                            shadow_pass.set_bind_group(0, &self.shadow_bind_group, &[]);
                            shadow_pass.set_bind_group(1, &self.model_bind_group, &[]);
                            shadow_pass.set_vertex_buffer(0, mesh.vertex_buffer.slice(..));
                            shadow_pass.set_index_buffer(mesh.index_buffer.slice(..), IndexFormat::Uint32);
                            shadow_pass.draw_indexed(0..mesh.index_count, 0, 0..1);
                        }

                        queue.submit(std::iter::once(encoder.finish()));
                    }
                }
            }
        }

        // Begin render pass - clear and draw sky
        let mut encoder = device.create_command_encoder(&CommandEncoderDescriptor {
            label: Some("scene_encoder"),
        });

        {
            let depth_view = self.depth_view.as_ref().unwrap();

            let mut render_pass = encoder.begin_render_pass(&RenderPassDescriptor {
                label: Some("scene_pass"),
                color_attachments: &[Some(RenderPassColorAttachment {
                    view,
                    resolve_target: None,
                    ops: Operations {
                        load: LoadOp::Clear(Color {
                            r: 0.0,
                            g: 0.0,
                            b: 0.0,
                            a: 1.0,
                        }),
                        store: StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: Some(RenderPassDepthStencilAttachment {
                    view: depth_view,
                    depth_ops: Some(Operations {
                        load: LoadOp::Clear(1.0),
                        store: StoreOp::Store,
                    }),
                    stencil_ops: None,
                }),
                occlusion_query_set: None,
                timestamp_writes: None,
            });

            // Draw sky first (renders at far depth with LessEqual)
            if let Some(sky_pipeline) = &self.sky_pipeline {
                render_pass.set_pipeline(sky_pipeline);
                render_pass.set_bind_group(0, &self.camera_bind_group, &[]);
                render_pass.draw(0..3, 0..1); // Fullscreen triangle
            }
        }

        // Submit clear + sky pass
        queue.submit(std::iter::once(encoder.finish()));

        // Render each entity with its own model and material uniforms
        for entity in &visible_entities {
            self.render_entity(device, queue, view, entity);
        }
    }

    /// Render a single entity
    fn render_entity(
        &self,
        device: &Device,
        queue: &Queue,
        view: &TextureView,
        entity: &SceneEntity,
    ) {
        let pbr_pipeline = match &self.pbr_pipeline {
            Some(p) => p,
            None => return,
        };

        let mesh = match self.meshes.get(&entity.mesh) {
            Some(m) => m,
            None => return,
        };

        // Update model uniforms
        #[repr(C)]
        #[derive(Copy, Clone, bytemuck::Pod, bytemuck::Zeroable)]
        struct ModelUniforms {
            model: [[f32; 4]; 4],
            normal_matrix: [[f32; 4]; 4],
        }

        let model_matrix = entity.transform.to_matrix();
        // Normal matrix is transpose of inverse of model matrix
        // For simplicity, use model matrix (works for uniform scaling)
        let model_uniforms = ModelUniforms {
            model: model_matrix,
            normal_matrix: model_matrix,
        };

        queue.write_buffer(&self.model_buffer, 0, bytemuck::bytes_of(&model_uniforms));

        // Update material uniforms
        #[repr(C)]
        #[derive(Copy, Clone, bytemuck::Pod, bytemuck::Zeroable)]
        struct MaterialUniforms {
            base_color: [f32; 4],
            metallic: f32,
            roughness: f32,
            emissive_r: f32,
            emissive_g: f32,
            emissive_b: f32,
            normal_scale: f32,
            ao_strength: f32,
            texture_flags: u32,
        }

        let tex_flags = entity.material.textures.get_flags();

        let material_uniforms = MaterialUniforms {
            base_color: entity.material.base_color,
            metallic: entity.material.metallic,
            roughness: entity.material.roughness,
            emissive_r: entity.material.emissive[0],
            emissive_g: entity.material.emissive[1],
            emissive_b: entity.material.emissive[2],
            normal_scale: entity.material.normal_scale,
            ao_strength: entity.material.ao_strength,
            texture_flags: tex_flags,
        };

        queue.write_buffer(&self.material_buffer, 0, bytemuck::bytes_of(&material_uniforms));

        // Get textures for this entity (or use defaults)
        let textures = &entity.material.textures;
        let default_white = self.texture_manager.get(self.texture_manager.default_white).unwrap();
        let default_normal = self.texture_manager.get(self.texture_manager.default_normal).unwrap();

        // Get entity's textures or defaults
        let albedo = self.texture_manager.get(textures.albedo).unwrap_or(default_white);
        let normal = self.texture_manager.get(textures.normal).unwrap_or(default_normal);
        let metallic_roughness = self.texture_manager.get(textures.metallic_roughness).unwrap_or(default_white);

        // Create per-entity material+texture bind group
        let entity_material_bind_group = device.create_bind_group(&BindGroupDescriptor {
            label: Some("entity_material_texture_bind_group"),
            layout: &self.texture_bind_group_layout,
            entries: &[
                BindGroupEntry { binding: 0, resource: self.material_buffer.as_entire_binding() },
                BindGroupEntry { binding: 1, resource: BindingResource::TextureView(&albedo.view) },
                BindGroupEntry { binding: 2, resource: BindingResource::Sampler(&albedo.sampler) },
                BindGroupEntry { binding: 3, resource: BindingResource::TextureView(&normal.view) },
                BindGroupEntry { binding: 4, resource: BindingResource::Sampler(&normal.sampler) },
                BindGroupEntry { binding: 5, resource: BindingResource::TextureView(&metallic_roughness.view) },
                BindGroupEntry { binding: 6, resource: BindingResource::Sampler(&metallic_roughness.sampler) },
            ],
        });

        // Create encoder for this entity
        let mut encoder = device.create_command_encoder(&CommandEncoderDescriptor {
            label: Some("entity_encoder"),
        });

        {
            let depth_view = self.depth_view.as_ref().unwrap();

            let mut render_pass = encoder.begin_render_pass(&RenderPassDescriptor {
                label: Some("entity_pass"),
                color_attachments: &[Some(RenderPassColorAttachment {
                    view,
                    resolve_target: None,
                    ops: Operations {
                        load: LoadOp::Load, // Keep previous content
                        store: StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: Some(RenderPassDepthStencilAttachment {
                    view: depth_view,
                    depth_ops: Some(Operations {
                        load: LoadOp::Load, // Keep depth
                        store: StoreOp::Store,
                    }),
                    stencil_ops: None,
                }),
                occlusion_query_set: None,
                timestamp_writes: None,
            });

            render_pass.set_pipeline(pbr_pipeline);
            render_pass.set_bind_group(0, &self.camera_bind_group, &[]);
            render_pass.set_bind_group(1, &self.model_bind_group, &[]);
            // Group 2: Combined material + textures for this entity
            render_pass.set_bind_group(2, &entity_material_bind_group, &[]);
            render_pass.set_bind_group(3, &self.light_bind_group, &[]);

            render_pass.set_vertex_buffer(0, mesh.vertex_buffer.slice(..));
            render_pass.set_index_buffer(mesh.index_buffer.slice(..), IndexFormat::Uint32);
            render_pass.draw_indexed(0..mesh.index_count, 0, 0..1);
        }

        queue.submit(std::iter::once(encoder.finish()));
    }

    /// Render particles (called after all entities)
    pub fn render_particles(
        &mut self,
        device: &Device,
        queue: &Queue,
        view: &TextureView,
    ) {
        // ========== PARTICLE PASS ==========
        if let (Some(particle_pipeline), Some(quad_buffer), Some(system)) =
            (&self.particle_pipeline, &self.particle_quad_buffer, &mut self.particle_system)
        {
            // Debug: log particle count every 60 frames
            static mut FRAME_COUNT: u32 = 0;
            unsafe {
                FRAME_COUNT += 1;
                if FRAME_COUNT % 60 == 0 {
                    log::info!("Particles: {} instances, {} in vec", system.instance_count, system.particles.len());
                }
            }

            if !system.particles.is_empty() {
                // Upload particle data to GPU (this also updates instance_count)
                system.upload(queue);

                if let Some(instance_buffer) = &system.instance_buffer {
                    let mut particle_encoder = device.create_command_encoder(&CommandEncoderDescriptor {
                        label: Some("particle_encoder"),
                    });

                    {
                        let depth_view = self.depth_view.as_ref().unwrap();

                        let mut particle_pass = particle_encoder.begin_render_pass(&RenderPassDescriptor {
                            label: Some("particle_pass"),
                            color_attachments: &[Some(RenderPassColorAttachment {
                                view,
                                resolve_target: None,
                                ops: Operations {
                                    load: LoadOp::Load, // Keep entity rendering
                                    store: StoreOp::Store,
                                },
                            })],
                            depth_stencil_attachment: Some(RenderPassDepthStencilAttachment {
                                view: depth_view,
                                depth_ops: Some(Operations {
                                    load: LoadOp::Load, // Read depth for occlusion
                                    store: StoreOp::Store,
                                }),
                                stencil_ops: None,
                            }),
                            occlusion_query_set: None,
                            timestamp_writes: None,
                        });

                        particle_pass.set_pipeline(particle_pipeline);
                        particle_pass.set_bind_group(0, &self.particle_camera_bind_group, &[]);
                        particle_pass.set_vertex_buffer(0, quad_buffer.slice(..));
                        particle_pass.set_vertex_buffer(1, instance_buffer.slice(..));
                        particle_pass.draw(0..6, 0..system.instance_count); // 6 vertices per quad, N instances
                    }

                    queue.submit(std::iter::once(particle_encoder.finish()));
                }
            }
        }
    }
}

// ============================================================================
// Mesh generation helpers
// ============================================================================

/// Generate sphere vertices and indices
/// Returns (vertices: [[pos.xyz, normal.xyz, uv.xy]], indices)
fn generate_sphere(segments: u32, rings: u32, radius: f32) -> (Vec<[f32; 8]>, Vec<u32>) {
    let mut vertices = Vec::new();
    let mut indices = Vec::new();

    for ring in 0..=rings {
        let phi = std::f32::consts::PI * ring as f32 / rings as f32;
        let y = phi.cos();
        let ring_radius = phi.sin();

        for seg in 0..=segments {
            let theta = 2.0 * std::f32::consts::PI * seg as f32 / segments as f32;
            let x = ring_radius * theta.cos();
            let z = ring_radius * theta.sin();

            let nx = x;
            let ny = y;
            let nz = z;

            let u = seg as f32 / segments as f32;
            let v = ring as f32 / rings as f32;

            vertices.push([
                x * radius, y * radius, z * radius,
                nx, ny, nz,
                u, v,
            ]);
        }
    }

    for ring in 0..rings {
        for seg in 0..segments {
            let current = ring * (segments + 1) + seg;
            let next = current + segments + 1;

            // Counter-clockwise winding for front faces (viewed from outside)
            indices.push(current);
            indices.push(current + 1);
            indices.push(next);

            indices.push(current + 1);
            indices.push(next + 1);
            indices.push(next);
        }
    }

    (vertices, indices)
}

/// Generate cube vertices and indices
fn generate_cube(size: f32) -> (Vec<[f32; 8]>, Vec<u32>) {
    let h = size * 0.5;

    // Each face has its own vertices for proper normals
    let vertices = vec![
        // Front face
        [-h, -h,  h,  0.0,  0.0,  1.0, 0.0, 1.0],
        [ h, -h,  h,  0.0,  0.0,  1.0, 1.0, 1.0],
        [ h,  h,  h,  0.0,  0.0,  1.0, 1.0, 0.0],
        [-h,  h,  h,  0.0,  0.0,  1.0, 0.0, 0.0],
        // Back face
        [ h, -h, -h,  0.0,  0.0, -1.0, 0.0, 1.0],
        [-h, -h, -h,  0.0,  0.0, -1.0, 1.0, 1.0],
        [-h,  h, -h,  0.0,  0.0, -1.0, 1.0, 0.0],
        [ h,  h, -h,  0.0,  0.0, -1.0, 0.0, 0.0],
        // Top face
        [-h,  h,  h,  0.0,  1.0,  0.0, 0.0, 1.0],
        [ h,  h,  h,  0.0,  1.0,  0.0, 1.0, 1.0],
        [ h,  h, -h,  0.0,  1.0,  0.0, 1.0, 0.0],
        [-h,  h, -h,  0.0,  1.0,  0.0, 0.0, 0.0],
        // Bottom face
        [-h, -h, -h,  0.0, -1.0,  0.0, 0.0, 1.0],
        [ h, -h, -h,  0.0, -1.0,  0.0, 1.0, 1.0],
        [ h, -h,  h,  0.0, -1.0,  0.0, 1.0, 0.0],
        [-h, -h,  h,  0.0, -1.0,  0.0, 0.0, 0.0],
        // Right face
        [ h, -h,  h,  1.0,  0.0,  0.0, 0.0, 1.0],
        [ h, -h, -h,  1.0,  0.0,  0.0, 1.0, 1.0],
        [ h,  h, -h,  1.0,  0.0,  0.0, 1.0, 0.0],
        [ h,  h,  h,  1.0,  0.0,  0.0, 0.0, 0.0],
        // Left face
        [-h, -h, -h, -1.0,  0.0,  0.0, 0.0, 1.0],
        [-h, -h,  h, -1.0,  0.0,  0.0, 1.0, 1.0],
        [-h,  h,  h, -1.0,  0.0,  0.0, 1.0, 0.0],
        [-h,  h, -h, -1.0,  0.0,  0.0, 0.0, 0.0],
    ];

    let indices = vec![
        0, 1, 2, 0, 2, 3,       // Front
        4, 5, 6, 4, 6, 7,       // Back
        8, 9, 10, 8, 10, 11,    // Top
        12, 13, 14, 12, 14, 15, // Bottom
        16, 17, 18, 16, 18, 19, // Right
        20, 21, 22, 20, 22, 23, // Left
    ];

    (vertices, indices)
}

/// Generate torus vertices and indices
fn generate_torus(major_radius: f32, minor_radius: f32, major_segments: u32, minor_segments: u32) -> (Vec<[f32; 8]>, Vec<u32>) {
    let mut vertices = Vec::new();
    let mut indices = Vec::new();

    for i in 0..=major_segments {
        let u = i as f32 / major_segments as f32;
        let theta = u * 2.0 * std::f32::consts::PI;

        let center_x = major_radius * theta.cos();
        let center_z = major_radius * theta.sin();

        for j in 0..=minor_segments {
            let v = j as f32 / minor_segments as f32;
            let phi = v * 2.0 * std::f32::consts::PI;

            let x = (major_radius + minor_radius * phi.cos()) * theta.cos();
            let y = minor_radius * phi.sin();
            let z = (major_radius + minor_radius * phi.cos()) * theta.sin();

            // Normal
            let nx = phi.cos() * theta.cos();
            let ny = phi.sin();
            let nz = phi.cos() * theta.sin();

            vertices.push([x, y, z, nx, ny, nz, u, v]);
        }
    }

    for i in 0..major_segments {
        for j in 0..minor_segments {
            let current = i * (minor_segments + 1) + j;
            let next = current + minor_segments + 1;

            // Counter-clockwise winding for front faces (viewed from outside)
            indices.push(current);
            indices.push(current + 1);
            indices.push(next);

            indices.push(current + 1);
            indices.push(next + 1);
            indices.push(next);
        }
    }

    (vertices, indices)
}

/// Generate diamond (octahedron) vertices and indices
fn generate_diamond(radius: f32, height: f32) -> (Vec<[f32; 8]>, Vec<u32>) {
    let mut vertices = Vec::new();
    let mut indices = Vec::new();

    // Top point
    let top = [0.0, height * 0.5, 0.0];
    // Bottom point
    let bottom = [0.0, -height * 0.5, 0.0];
    // Middle ring
    let mid_points = [
        [radius, 0.0, 0.0],
        [0.0, 0.0, radius],
        [-radius, 0.0, 0.0],
        [0.0, 0.0, -radius],
    ];

    // Create triangles with proper normals
    for i in 0..4 {
        let next = (i + 1) % 4;
        let p0 = mid_points[i];
        let p1 = mid_points[next];

        // Top triangle
        let n = normalize_vec3(cross_vec3(
            sub_vec3(p1, p0),
            sub_vec3(top, p0),
        ));

        let base = vertices.len() as u32;
        vertices.push([p0[0], p0[1], p0[2], n[0], n[1], n[2], 0.0, 0.5]);
        vertices.push([p1[0], p1[1], p1[2], n[0], n[1], n[2], 1.0, 0.5]);
        vertices.push([top[0], top[1], top[2], n[0], n[1], n[2], 0.5, 0.0]);
        indices.extend_from_slice(&[base, base + 1, base + 2]);

        // Bottom triangle
        let n = normalize_vec3(cross_vec3(
            sub_vec3(bottom, p0),
            sub_vec3(p1, p0),
        ));

        let base = vertices.len() as u32;
        vertices.push([p0[0], p0[1], p0[2], n[0], n[1], n[2], 0.0, 0.5]);
        vertices.push([bottom[0], bottom[1], bottom[2], n[0], n[1], n[2], 0.5, 1.0]);
        vertices.push([p1[0], p1[1], p1[2], n[0], n[1], n[2], 1.0, 0.5]);
        indices.extend_from_slice(&[base, base + 1, base + 2]);
    }

    (vertices, indices)
}

/// Generate plane vertices and indices (double-sided)
fn generate_plane(size: f32) -> (Vec<[f32; 8]>, Vec<u32>) {
    let h = size * 0.5;

    let vertices = vec![
        // Top face (normal pointing up)
        [-h, 0.0, -h, 0.0, 1.0, 0.0, 0.0, 0.0],
        [ h, 0.0, -h, 0.0, 1.0, 0.0, 1.0, 0.0],
        [ h, 0.0,  h, 0.0, 1.0, 0.0, 1.0, 1.0],
        [-h, 0.0,  h, 0.0, 1.0, 0.0, 0.0, 1.0],
        // Bottom face (normal pointing down)
        [-h, 0.0, -h, 0.0, -1.0, 0.0, 0.0, 0.0],
        [ h, 0.0, -h, 0.0, -1.0, 0.0, 1.0, 0.0],
        [ h, 0.0,  h, 0.0, -1.0, 0.0, 1.0, 1.0],
        [-h, 0.0,  h, 0.0, -1.0, 0.0, 0.0, 1.0],
    ];

    let indices = vec![
        // Top face
        0, 2, 1, 0, 3, 2,
        // Bottom face (reversed winding)
        4, 5, 6, 4, 6, 7,
    ];

    (vertices, indices)
}

// Vector math helpers
fn sub_vec3(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
}

fn cross_vec3(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

fn normalize_vec3(v: [f32; 3]) -> [f32; 3] {
    let len = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    if len > 0.0001 {
        [v[0] / len, v[1] / len, v[2] / len]
    } else {
        [0.0, 1.0, 0.0]
    }
}
