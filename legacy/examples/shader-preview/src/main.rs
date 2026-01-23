//! 3D Model Viewer with PBR Rendering
//!
//! Controls:
//! - Mouse drag: Orbit camera
//! - Scroll wheel: Zoom
//! - WASD: Pan camera
//! - Shift: Speed up movement
//! - R: Reset camera
//! - 1-4: Switch between demo objects
//! - ESC: Exit
//!
//! Usage: cargo run -p shader-preview [optional-model.glb]

use std::sync::Arc;
use std::time::Instant;

use wgpu::util::DeviceExt;
use winit::{
    event::{ElementState, Event, MouseButton, MouseScrollDelta, WindowEvent},
    event_loop::EventLoop,
    keyboard::{Key, NamedKey},
    window::WindowBuilder,
};

use void_math::{Mat4, Vec3};
use void_render::camera::{Camera, Projection};
use void_render::camera_controller::{CameraController, CameraInput};
use void_asset_server::loaders::mesh::{MeshLoader, MeshAsset, Vertex};

// ============================================================================
// GPU Uniform Structures
// ============================================================================

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct CameraUniforms {
    view_proj: [[f32; 4]; 4],
    view: [[f32; 4]; 4],
    projection: [[f32; 4]; 4],
    camera_pos: [f32; 3],
    _padding: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct ModelUniforms {
    model: [[f32; 4]; 4],
    normal_matrix: [[f32; 4]; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct MaterialUniforms {
    base_color_factor: [f32; 4],
    emissive_factor: [f32; 3],
    metallic_factor: f32,
    roughness_factor: f32,
    normal_scale: f32,
    occlusion_strength: f32,
    alpha_cutoff: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct LightUniforms {
    direction: [f32; 3],
    _pad1: f32,
    color: [f32; 3],
    intensity: f32,
    ambient_color: [f32; 3],
    ambient_intensity: f32,
}

// ============================================================================
// GPU Vertex
// ============================================================================

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct GpuVertex {
    position: [f32; 3],
    normal: [f32; 3],
    uv: [f32; 2],
    tangent: [f32; 4],
}

impl GpuVertex {
    fn from_vertex(v: &Vertex) -> Self {
        // Generate tangent (simple approximation)
        let tangent = if v.normal[1].abs() < 0.999 {
            let t = [v.normal[2], 0.0, -v.normal[0]];
            let len = (t[0] * t[0] + t[2] * t[2]).sqrt();
            [t[0] / len, 0.0, t[2] / len, 1.0]
        } else {
            [1.0, 0.0, 0.0, 1.0]
        };

        Self {
            position: v.position,
            normal: v.normal,
            uv: v.uv,
            tangent,
        }
    }

    fn desc() -> wgpu::VertexBufferLayout<'static> {
        wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<GpuVertex>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &[
                wgpu::VertexAttribute {
                    offset: 0,
                    shader_location: 0,
                    format: wgpu::VertexFormat::Float32x3,
                },
                wgpu::VertexAttribute {
                    offset: 12,
                    shader_location: 1,
                    format: wgpu::VertexFormat::Float32x3,
                },
                wgpu::VertexAttribute {
                    offset: 24,
                    shader_location: 2,
                    format: wgpu::VertexFormat::Float32x2,
                },
                wgpu::VertexAttribute {
                    offset: 32,
                    shader_location: 3,
                    format: wgpu::VertexFormat::Float32x4,
                },
            ],
        }
    }
}

// ============================================================================
// Mesh GPU Resources
// ============================================================================

struct GpuMesh {
    vertex_buffer: wgpu::Buffer,
    index_buffer: wgpu::Buffer,
    index_count: u32,
}

impl GpuMesh {
    fn from_mesh_asset(device: &wgpu::Device, mesh: &MeshAsset) -> Self {
        let vertices: Vec<GpuVertex> = mesh.vertices.iter().map(GpuVertex::from_vertex).collect();

        let vertex_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("Vertex Buffer"),
            contents: bytemuck::cast_slice(&vertices),
            usage: wgpu::BufferUsages::VERTEX,
        });

        let index_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("Index Buffer"),
            contents: bytemuck::cast_slice(&mesh.indices),
            usage: wgpu::BufferUsages::INDEX,
        });

        Self {
            vertex_buffer,
            index_buffer,
            index_count: mesh.indices.len() as u32,
        }
    }
}

// ============================================================================
// Simple PBR Shader (embedded for simplicity)
// ============================================================================

const PBR_SHADER: &str = r#"
const PI: f32 = 3.14159265359;

struct CameraUniforms {
    view_proj: mat4x4<f32>,
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    camera_pos: vec3<f32>,
    _padding: f32,
}

struct ModelUniforms {
    model: mat4x4<f32>,
    normal_matrix: mat4x4<f32>,
}

struct MaterialUniforms {
    base_color_factor: vec4<f32>,
    emissive_factor: vec3<f32>,
    metallic_factor: f32,
    roughness_factor: f32,
    normal_scale: f32,
    occlusion_strength: f32,
    alpha_cutoff: f32,
}

struct LightUniforms {
    direction: vec3<f32>,
    _pad1: f32,
    color: vec3<f32>,
    intensity: f32,
    ambient_color: vec3<f32>,
    ambient_intensity: f32,
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(1) @binding(0) var<uniform> model: ModelUniforms;
@group(2) @binding(0) var<uniform> material: MaterialUniforms;
@group(3) @binding(0) var<uniform> light: LightUniforms;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) tangent: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_position: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
}

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    let world_pos = model.model * vec4<f32>(input.position, 1.0);
    output.world_position = world_pos.xyz;
    output.clip_position = camera.view_proj * world_pos;
    output.world_normal = normalize((model.normal_matrix * vec4<f32>(input.normal, 0.0)).xyz);
    output.uv = input.uv;
    return output;
}

fn fresnel_schlick(cos_theta: f32, f0: vec3<f32>) -> vec3<f32> {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

fn distribution_ggx(n_dot_h: f32, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let n_dot_h2 = n_dot_h * n_dot_h;
    var denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / denom;
}

fn geometry_schlick_ggx(n_dot_v: f32, roughness: f32) -> f32 {
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;
    return n_dot_v / (n_dot_v * (1.0 - k) + k);
}

fn geometry_smith(n_dot_v: f32, n_dot_l: f32, roughness: f32) -> f32 {
    return geometry_schlick_ggx(n_dot_v, roughness) * geometry_schlick_ggx(n_dot_l, roughness);
}

// Procedural HDR environment map - simulates a bright studio/outdoor scene
fn sample_environment(dir: vec3<f32>) -> vec3<f32> {
    let y = dir.y;

    // Base sky gradient - vibrant blue sky
    let sky_zenith = vec3<f32>(0.2, 0.5, 1.0);  // Deep blue
    let sky_horizon = vec3<f32>(0.8, 0.9, 1.0); // Light blue/white
    let ground_color = vec3<f32>(0.2, 0.18, 0.15); // Warm brown

    var env: vec3<f32>;
    if y > 0.0 {
        // Sky with exponential falloff for realism
        let t = pow(y, 0.3);
        env = mix(sky_horizon, sky_zenith, t);

        // Add bright sun (key light reflection)
        let sun_dir = normalize(vec3<f32>(0.5, 0.7, -0.5));
        let sun_dot = max(dot(dir, sun_dir), 0.0);

        // Sun disk - very bright, small
        let sun_disk = pow(sun_dot, 256.0) * 50.0;
        env += vec3<f32>(1.0, 0.95, 0.9) * sun_disk;

        // Sun corona/glow
        let sun_glow = pow(sun_dot, 8.0) * 2.0;
        env += vec3<f32>(1.0, 0.9, 0.7) * sun_glow;

        // Secondary highlight (fill reflection)
        let fill_dir = normalize(vec3<f32>(-0.7, 0.3, 0.6));
        let fill_dot = max(dot(dir, fill_dir), 0.0);
        env += vec3<f32>(0.4, 0.5, 0.6) * pow(fill_dot, 16.0) * 1.5;

    } else {
        // Ground with slight gradient
        let t = pow(-y, 0.5);
        env = mix(sky_horizon * 0.4, ground_color, t);

        // Ground bounce light
        env += vec3<f32>(0.15, 0.12, 0.1) * 0.5;
    }

    // Horizon glow
    let horizon_factor = 1.0 - abs(y);
    let horizon_glow = pow(horizon_factor, 8.0);
    env += vec3<f32>(1.0, 0.95, 0.85) * horizon_glow * 0.5;

    return env;
}

// Sample blurred environment based on roughness (fake mip levels)
fn sample_environment_lod(dir: vec3<f32>, roughness: f32) -> vec3<f32> {
    // More samples for rougher surfaces to simulate blur
    if roughness < 0.1 {
        return sample_environment(dir);
    }

    // Simplified blur - reduce intensity and add ambient term
    let sharp = sample_environment(dir);
    let blur_factor = roughness * roughness;

    // Blurred version is just a lower intensity + more ambient
    let ambient = vec3<f32>(0.5, 0.55, 0.65);
    return mix(sharp, ambient, blur_factor * 0.7);
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let base_color = material.base_color_factor.rgb;
    let metallic = material.metallic_factor;
    let roughness = max(material.roughness_factor, 0.04);

    let n = normalize(input.world_normal);
    let v = normalize(camera.camera_pos - input.world_position);
    let l = normalize(-light.direction);
    let h = normalize(v + l);
    let r = reflect(-v, n);  // Reflection direction

    let n_dot_v = max(dot(n, v), 0.001);
    let n_dot_l = max(dot(n, l), 0.0);
    let n_dot_h = max(dot(n, h), 0.0);
    let h_dot_v = max(dot(h, v), 0.0);

    // F0 - base reflectivity (dielectric = 0.04, metal = albedo color)
    let f0 = mix(vec3<f32>(0.04), base_color, metallic);

    // Cook-Torrance BRDF
    let d = distribution_ggx(n_dot_h, roughness);
    let g = geometry_smith(n_dot_v, n_dot_l, roughness);
    let f = fresnel_schlick(h_dot_v, f0);

    let specular = (d * g * f) / (4.0 * n_dot_v * n_dot_l + 0.0001);

    let ks = f;
    let kd = (vec3<f32>(1.0) - ks) * (1.0 - metallic);
    let diffuse = kd * base_color / PI;

    // Direct lighting
    let radiance = light.color * light.intensity;
    var direct = (diffuse + specular) * radiance * n_dot_l;

    // Fill light from opposite side
    let fill_dir = normalize(vec3<f32>(-0.6, 0.4, 0.7));
    let fill_n_dot_l = max(dot(n, fill_dir), 0.0);
    let fill_color = vec3<f32>(0.4, 0.45, 0.6);
    direct += diffuse * fill_color * fill_n_dot_l * 0.4;

    // Image-Based Lighting (environment reflections)
    let env = sample_environment_lod(r, roughness);
    let fresnel_env = fresnel_schlick(n_dot_v, f0);

    // Environment reflection - stronger for metals, subtle for dielectrics
    let env_strength = mix(0.15, 0.8, metallic);  // Reduced for non-metals
    let env_reflection = env * fresnel_env * env_strength;

    // Diffuse IBL (hemisphere sampling approximation)
    let hemisphere_blend = n.y * 0.5 + 0.5;
    let sky_irradiance = vec3<f32>(0.6, 0.65, 0.75);
    let ground_irradiance = vec3<f32>(0.25, 0.22, 0.2);
    let diffuse_ibl = mix(ground_irradiance, sky_irradiance, hemisphere_blend);

    // Strong diffuse color for non-metals
    let ambient_diffuse = kd * base_color * diffuse_ibl * (light.ambient_intensity + 0.3);

    // Combine ambient/IBL - diffuse dominates for non-metals
    let ambient = ambient_diffuse + env_reflection * (0.5 + metallic * 0.5);

    // Rim light for edge pop
    let rim = pow(1.0 - n_dot_v, 3.0) * 0.5;
    let rim_color = mix(vec3<f32>(0.4, 0.5, 0.7), light.color, 0.3);
    let rim_light = rim_color * rim;

    let emissive = material.emissive_factor;

    var color = direct + ambient + rim_light + emissive;

    // Tone mapping (ACES approximation)
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d2 = 0.59;
    let e = 0.14;
    color = clamp((color * (a * color + b)) / (color * (c * color + d2) + e), vec3<f32>(0.0), vec3<f32>(1.0));

    // Gamma correction
    color = pow(color, vec3<f32>(1.0 / 2.2));

    return vec4<f32>(color, material.base_color_factor.a);
}
"#;

// Grid/background shader for the floor
const GRID_SHADER: &str = r#"
struct CameraUniforms {
    view_proj: mat4x4<f32>,
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    camera_pos: vec3<f32>,
    _padding: f32,
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_pos: vec3<f32>,
}

@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> VertexOutput {
    var positions = array<vec3<f32>, 6>(
        vec3<f32>(-50.0, 0.0, -50.0),
        vec3<f32>( 50.0, 0.0, -50.0),
        vec3<f32>( 50.0, 0.0,  50.0),
        vec3<f32>(-50.0, 0.0, -50.0),
        vec3<f32>( 50.0, 0.0,  50.0),
        vec3<f32>(-50.0, 0.0,  50.0),
    );

    var output: VertexOutput;
    let pos = positions[idx];
    output.world_pos = pos;
    output.clip_position = camera.view_proj * vec4<f32>(pos, 1.0);
    return output;
}

fn grid(pos: vec2<f32>, scale: f32, width: f32) -> f32 {
    let coord = pos * scale;
    let grid_x = abs(fract(coord.x - 0.5) - 0.5) / fwidth(coord.x);
    let grid_y = abs(fract(coord.y - 0.5) - 0.5) / fwidth(coord.y);
    return 1.0 - min(min(grid_x, grid_y), 1.0);
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let pos = input.world_pos.xz;

    // Two-scale grid
    let grid1 = grid(pos, 1.0, 1.0) * 0.5;
    let grid2 = grid(pos, 0.1, 1.0) * 0.25;

    let intensity = max(grid1, grid2);

    // Fade with distance
    let dist = length(input.world_pos - camera.camera_pos);
    let fade = exp(-dist * 0.03);

    let color = vec3<f32>(0.3, 0.35, 0.4) * intensity * fade;
    let alpha = intensity * fade;

    if alpha < 0.01 {
        discard;
    }

    return vec4<f32>(color, alpha);
}
"#;

// Sky gradient shader with sun
const SKY_SHADER: &str = r#"
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> VertexOutput {
    var output: VertexOutput;
    let x = f32((idx << 1u) & 2u);
    let y = f32(idx & 2u);
    output.position = vec4<f32>(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.9999, 1.0);
    output.uv = vec2<f32>(x, y);
    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let uv = input.uv;
    let y = 1.0 - uv.y;

    // Vibrant sky colors
    let sky_zenith = vec3<f32>(0.1, 0.3, 0.7);     // Deep blue
    let sky_horizon = vec3<f32>(0.5, 0.7, 0.9);    // Light blue
    let ground_color = vec3<f32>(0.18, 0.15, 0.13); // Brown ground

    var color: vec3<f32>;
    if y > 0.5 {
        // Sky gradient
        let t = (y - 0.5) * 2.0;
        color = mix(sky_horizon, sky_zenith, pow(t, 0.4));

        // Sun - upper right
        let sun_uv = vec2<f32>(0.75, 0.8);
        let sun_dist = length(uv - sun_uv);

        // Bright sun disk
        let sun_disk = smoothstep(0.05, 0.02, sun_dist);
        color = mix(color, vec3<f32>(1.0, 0.95, 0.85), sun_disk);

        // Sun glow
        color += vec3<f32>(1.0, 0.8, 0.4) * exp(-sun_dist * 6.0) * 0.6;
        color += vec3<f32>(1.0, 0.9, 0.6) * exp(-sun_dist * 2.5) * 0.3;

    } else {
        // Ground gradient
        let t = (0.5 - y) * 2.0;
        color = mix(sky_horizon * 0.4, ground_color, pow(t, 0.6));
    }

    // Horizon glow
    let h = 1.0 - abs(y - 0.5) * 2.0;
    color += vec3<f32>(1.0, 0.85, 0.6) * pow(max(h, 0.0), 3.0) * 0.3;

    // Simple tone mapping
    color = color / (color + 0.5);

    return vec4<f32>(color, 1.0);
}
"#;

// ============================================================================
// Demo Objects
// ============================================================================

#[derive(Clone, Copy, PartialEq)]
enum DemoObject {
    Sphere,
    Cube,
    Torus,
    Diamond,
}

impl DemoObject {
    fn mesh(&self) -> MeshAsset {
        match self {
            DemoObject::Sphere => MeshLoader::sphere(64, 32),  // Higher resolution for smooth shading
            DemoObject::Cube => MeshLoader::cube(),
            DemoObject::Torus => MeshLoader::torus(48, 24, 0.15),
            DemoObject::Diamond => MeshLoader::diamond(),
        }
    }

    fn name(&self) -> &'static str {
        match self {
            DemoObject::Sphere => "Sphere",
            DemoObject::Cube => "Cube",
            DemoObject::Torus => "Torus",
            DemoObject::Diamond => "Diamond",
        }
    }
}

// ============================================================================
// Main
// ============================================================================

fn main() {
    env_logger::init();

    let event_loop = EventLoop::new().unwrap();
    let window = Arc::new(
        WindowBuilder::new()
            .with_title("3D Model Viewer - PBR Rendering")
            .with_inner_size(winit::dpi::LogicalSize::new(1280, 720))
            .build(&event_loop)
            .unwrap(),
    );

    log::info!("Creating GPU instance...");
    let instance = wgpu::Instance::new(wgpu::InstanceDescriptor::default());
    let surface = instance.create_surface(window.clone()).unwrap();

    let adapter = pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
        power_preference: wgpu::PowerPreference::HighPerformance,
        compatible_surface: Some(&surface),
        force_fallback_adapter: false,
    }))
    .expect("Failed to find GPU adapter");

    log::info!("Using adapter: {:?}", adapter.get_info().name);

    let (device, queue) = pollster::block_on(adapter.request_device(
        &wgpu::DeviceDescriptor {
            label: None,
            required_features: wgpu::Features::empty(),
            required_limits: wgpu::Limits::default(),
            memory_hints: wgpu::MemoryHints::default(),
        },
        None,
    ))
    .expect("Failed to create device");

    let size = window.inner_size();
    let mut config = surface
        .get_default_config(&adapter, size.width.max(1), size.height.max(1))
        .unwrap();
    config.present_mode = wgpu::PresentMode::AutoVsync;
    surface.configure(&device, &config);

    // Create depth texture
    let depth_format = wgpu::TextureFormat::Depth32Float;
    let mut depth_texture = create_depth_texture(&device, size.width, size.height, depth_format);

    // Create shaders
    let pbr_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: Some("PBR Shader"),
        source: wgpu::ShaderSource::Wgsl(PBR_SHADER.into()),
    });

    let grid_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: Some("Grid Shader"),
        source: wgpu::ShaderSource::Wgsl(GRID_SHADER.into()),
    });

    let sky_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: Some("Sky Shader"),
        source: wgpu::ShaderSource::Wgsl(SKY_SHADER.into()),
    });

    // Create bind group layouts
    let camera_bind_group_layout =
        device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("Camera Bind Group Layout"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX | wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

    let model_bind_group_layout =
        device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("Model Bind Group Layout"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

    let material_bind_group_layout =
        device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("Material Bind Group Layout"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

    let light_bind_group_layout =
        device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("Light Bind Group Layout"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

    // Create uniform buffers
    let camera_buffer = device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("Camera Uniform Buffer"),
        size: std::mem::size_of::<CameraUniforms>() as u64,
        usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        mapped_at_creation: false,
    });

    let model_buffer = device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("Model Uniform Buffer"),
        size: std::mem::size_of::<ModelUniforms>() as u64,
        usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        mapped_at_creation: false,
    });

    let material_buffer = device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("Material Uniform Buffer"),
        size: std::mem::size_of::<MaterialUniforms>() as u64,
        usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        mapped_at_creation: false,
    });

    let light_buffer = device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("Light Uniform Buffer"),
        size: std::mem::size_of::<LightUniforms>() as u64,
        usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        mapped_at_creation: false,
    });

    // Create bind groups
    let camera_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some("Camera Bind Group"),
        layout: &camera_bind_group_layout,
        entries: &[wgpu::BindGroupEntry {
            binding: 0,
            resource: camera_buffer.as_entire_binding(),
        }],
    });

    let model_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some("Model Bind Group"),
        layout: &model_bind_group_layout,
        entries: &[wgpu::BindGroupEntry {
            binding: 0,
            resource: model_buffer.as_entire_binding(),
        }],
    });

    let material_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some("Material Bind Group"),
        layout: &material_bind_group_layout,
        entries: &[wgpu::BindGroupEntry {
            binding: 0,
            resource: material_buffer.as_entire_binding(),
        }],
    });

    let light_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some("Light Bind Group"),
        layout: &light_bind_group_layout,
        entries: &[wgpu::BindGroupEntry {
            binding: 0,
            resource: light_buffer.as_entire_binding(),
        }],
    });

    // Create pipeline layouts
    let pbr_pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
        label: Some("PBR Pipeline Layout"),
        bind_group_layouts: &[
            &camera_bind_group_layout,
            &model_bind_group_layout,
            &material_bind_group_layout,
            &light_bind_group_layout,
        ],
        push_constant_ranges: &[],
    });

    let grid_pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
        label: Some("Grid Pipeline Layout"),
        bind_group_layouts: &[&camera_bind_group_layout],
        push_constant_ranges: &[],
    });

    let sky_pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
        label: Some("Sky Pipeline Layout"),
        bind_group_layouts: &[],
        push_constant_ranges: &[],
    });

    // Create pipelines
    let pbr_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: Some("PBR Pipeline"),
        layout: Some(&pbr_pipeline_layout),
        vertex: wgpu::VertexState {
            module: &pbr_shader,
            entry_point: "vs_main",
            buffers: &[GpuVertex::desc()],
            compilation_options: wgpu::PipelineCompilationOptions::default(),
        },
        fragment: Some(wgpu::FragmentState {
            module: &pbr_shader,
            entry_point: "fs_main",
            targets: &[Some(wgpu::ColorTargetState {
                format: config.format,
                blend: Some(wgpu::BlendState::REPLACE),
                write_mask: wgpu::ColorWrites::ALL,
            })],
            compilation_options: wgpu::PipelineCompilationOptions::default(),
        }),
        primitive: wgpu::PrimitiveState {
            topology: wgpu::PrimitiveTopology::TriangleList,
            cull_mode: Some(wgpu::Face::Back),
            ..Default::default()
        },
        depth_stencil: Some(wgpu::DepthStencilState {
            format: depth_format,
            depth_write_enabled: true,
            depth_compare: wgpu::CompareFunction::Less,
            stencil: wgpu::StencilState::default(),
            bias: wgpu::DepthBiasState::default(),
        }),
        multisample: wgpu::MultisampleState::default(),
        multiview: None,
        cache: None,
    });

    let grid_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: Some("Grid Pipeline"),
        layout: Some(&grid_pipeline_layout),
        vertex: wgpu::VertexState {
            module: &grid_shader,
            entry_point: "vs_main",
            buffers: &[],
            compilation_options: wgpu::PipelineCompilationOptions::default(),
        },
        fragment: Some(wgpu::FragmentState {
            module: &grid_shader,
            entry_point: "fs_main",
            targets: &[Some(wgpu::ColorTargetState {
                format: config.format,
                blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                write_mask: wgpu::ColorWrites::ALL,
            })],
            compilation_options: wgpu::PipelineCompilationOptions::default(),
        }),
        primitive: wgpu::PrimitiveState::default(),
        depth_stencil: Some(wgpu::DepthStencilState {
            format: depth_format,
            depth_write_enabled: false,
            depth_compare: wgpu::CompareFunction::Less,
            stencil: wgpu::StencilState::default(),
            bias: wgpu::DepthBiasState::default(),
        }),
        multisample: wgpu::MultisampleState::default(),
        multiview: None,
        cache: None,
    });

    let sky_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: Some("Sky Pipeline"),
        layout: Some(&sky_pipeline_layout),
        vertex: wgpu::VertexState {
            module: &sky_shader,
            entry_point: "vs_main",
            buffers: &[],
            compilation_options: wgpu::PipelineCompilationOptions::default(),
        },
        fragment: Some(wgpu::FragmentState {
            module: &sky_shader,
            entry_point: "fs_main",
            targets: &[Some(wgpu::ColorTargetState {
                format: config.format,
                blend: Some(wgpu::BlendState::REPLACE),
                write_mask: wgpu::ColorWrites::ALL,
            })],
            compilation_options: wgpu::PipelineCompilationOptions::default(),
        }),
        primitive: wgpu::PrimitiveState::default(),
        depth_stencil: Some(wgpu::DepthStencilState {
            format: depth_format,
            depth_write_enabled: false,
            depth_compare: wgpu::CompareFunction::LessEqual,
            stencil: wgpu::StencilState::default(),
            bias: wgpu::DepthBiasState::default(),
        }),
        multisample: wgpu::MultisampleState::default(),
        multiview: None,
        cache: None,
    });

    // Create initial mesh
    let mut current_object = DemoObject::Sphere;
    let mut mesh = GpuMesh::from_mesh_asset(&device, &current_object.mesh());

    // Camera setup
    let mut camera = Camera::perspective(
        60.0_f32.to_radians(),
        size.width as f32 / size.height as f32,
        0.1,
        1000.0,
    );
    camera.position = Vec3::new(0.0, 0.5, 3.0);

    let mut camera_controller = CameraController::orbit(Vec3::ZERO, 3.0);
    camera_controller.yaw = std::f32::consts::PI * 0.75;
    camera_controller.pitch = 0.4;
    camera_controller.mouse_sensitivity = 0.005; // Lower for smoother control
    camera_controller.zoom_speed = 0.3;

    // Input state
    let mut last_mouse_pos = (0.0f32, 0.0f32);
    let mut mouse_pressed = false;
    let mut camera_input = CameraInput::default();

    // Timing
    let start_time = Instant::now();
    let mut last_frame = Instant::now();

    // Material properties - start with colorful red plastic to show PBR clearly
    let mut material_preset = 0usize;
    // Format: (base_color RGBA, metallic, roughness, name)
    let material_presets: Vec<([f32; 4], f32, f32, &str)> = vec![
        ([0.9, 0.1, 0.1, 1.0], 0.0, 0.35, "Red Glossy"),      // Vibrant red, non-metal
        ([0.1, 0.5, 0.9, 1.0], 0.0, 0.25, "Blue Glossy"),     // Vibrant blue, non-metal
        ([1.0, 0.85, 0.0, 1.0], 0.8, 0.25, "Gold"),           // Bright gold
        ([0.95, 0.95, 0.97, 1.0], 0.95, 0.1, "Chrome"),       // Mirror chrome
        ([0.97, 0.74, 0.62, 1.0], 0.85, 0.3, "Copper"),       // Warm copper
        ([0.1, 0.8, 0.3, 1.0], 0.0, 0.4, "Green Plastic"),    // Green plastic
        ([0.95, 0.95, 0.95, 1.0], 0.0, 0.02, "Glass"),        // Smooth glass-like
        ([0.02, 0.02, 0.02, 1.0], 0.0, 0.8, "Rubber"),        // Matte black rubber
    ];
    let (mut base_color, metallic_init, roughness_init, _) = material_presets[material_preset];
    let mut metallic = metallic_init;
    let mut roughness = roughness_init;

    // Set initial window title with controls
    let title = format!("[{}] {} | Tab:Material 1-4:Shape Drag:Orbit Scroll:Zoom ESC:Exit",
        current_object.name(), material_presets[material_preset].3);
    window.set_title(&title);

    log::info!("");
    log::info!("=== 3D PBR Model Viewer ===");
    log::info!("Controls:");
    log::info!("  Mouse drag: Orbit camera");
    log::info!("  Scroll: Zoom in/out");
    log::info!("  1-4: Switch objects (Sphere, Cube, Torus, Diamond)");
    log::info!("  Tab: Cycle material presets (Gold, Silver, Copper, etc.)");
    log::info!("  M/N: Increase/decrease metallic");
    log::info!("  R/F: Increase/decrease roughness");
    log::info!("  ESC: Exit");
    log::info!("");
    log::info!("Current material: {}", material_presets[material_preset].3);

    event_loop
        .run(move |event, elwt| {
            match event {
                Event::WindowEvent {
                    event: WindowEvent::CloseRequested,
                    ..
                } => {
                    elwt.exit();
                }

                Event::WindowEvent {
                    event: WindowEvent::Resized(new_size),
                    ..
                } => {
                    if new_size.width > 0 && new_size.height > 0 {
                        config.width = new_size.width;
                        config.height = new_size.height;
                        surface.configure(&device, &config);
                        depth_texture = create_depth_texture(
                            &device,
                            new_size.width,
                            new_size.height,
                            depth_format,
                        );

                        // Update camera aspect ratio
                        if let Projection::Perspective { aspect, .. } = &mut camera.projection {
                            *aspect = new_size.width as f32 / new_size.height as f32;
                        }
                    }
                }

                Event::WindowEvent {
                    event: WindowEvent::KeyboardInput { event, .. },
                    ..
                } => {
                    if event.state == ElementState::Pressed {
                        match &event.logical_key {
                            Key::Named(NamedKey::Escape) => elwt.exit(),
                            Key::Named(NamedKey::Tab) => {
                                material_preset = (material_preset + 1) % material_presets.len();
                                let (color, met, rough, name) = material_presets[material_preset];
                                base_color = color;
                                metallic = met;
                                roughness = rough;
                                let title = format!("[{}] {} | Tab:Material 1-4:Shape Drag:Orbit Scroll:Zoom",
                                    current_object.name(), name);
                                window.set_title(&title);
                            }
                            Key::Character(c) => match c.as_str() {
                                "1" => {
                                    current_object = DemoObject::Sphere;
                                    mesh = GpuMesh::from_mesh_asset(&device, &current_object.mesh());
                                    let title = format!("[{}] {} | Tab:Material 1-4:Shape Drag:Orbit Scroll:Zoom",
                                        current_object.name(), material_presets[material_preset].3);
                                    window.set_title(&title);
                                }
                                "2" => {
                                    current_object = DemoObject::Cube;
                                    mesh = GpuMesh::from_mesh_asset(&device, &current_object.mesh());
                                    let title = format!("[{}] {} | Tab:Material 1-4:Shape Drag:Orbit Scroll:Zoom",
                                        current_object.name(), material_presets[material_preset].3);
                                    window.set_title(&title);
                                }
                                "3" => {
                                    current_object = DemoObject::Torus;
                                    mesh = GpuMesh::from_mesh_asset(&device, &current_object.mesh());
                                    let title = format!("[{}] {} | Tab:Material 1-4:Shape Drag:Orbit Scroll:Zoom",
                                        current_object.name(), material_presets[material_preset].3);
                                    window.set_title(&title);
                                }
                                "4" => {
                                    current_object = DemoObject::Diamond;
                                    mesh = GpuMesh::from_mesh_asset(&device, &current_object.mesh());
                                    let title = format!("[{}] {} | Tab:Material 1-4:Shape Drag:Orbit Scroll:Zoom",
                                        current_object.name(), material_presets[material_preset].3);
                                    window.set_title(&title);
                                }
                                "m" => {
                                    metallic = (metallic + 0.1).min(1.0);
                                    log::info!("Metallic: {:.1}", metallic);
                                }
                                "n" => {
                                    metallic = (metallic - 0.1).max(0.0);
                                    log::info!("Metallic: {:.1}", metallic);
                                }
                                "r" => {
                                    roughness = (roughness + 0.1).min(1.0);
                                    log::info!("Roughness: {:.1}", roughness);
                                }
                                "f" => {
                                    roughness = (roughness - 0.1).max(0.04);
                                    log::info!("Roughness: {:.1}", roughness);
                                }
                                _ => {}
                            },
                            _ => {}
                        }
                    }
                }

                Event::WindowEvent {
                    event: WindowEvent::MouseInput { state, button, .. },
                    ..
                } => {
                    if button == MouseButton::Left {
                        mouse_pressed = state == ElementState::Pressed;
                        camera_input.drag_active = mouse_pressed;
                    }
                }

                Event::WindowEvent {
                    event: WindowEvent::CursorMoved { position, .. },
                    ..
                } => {
                    let new_pos = (position.x as f32, position.y as f32);
                    if mouse_pressed {
                        camera_input.mouse_delta = (
                            new_pos.0 - last_mouse_pos.0,
                            new_pos.1 - last_mouse_pos.1,
                        );
                    } else {
                        camera_input.mouse_delta = (0.0, 0.0);
                    }
                    last_mouse_pos = new_pos;
                }

                Event::WindowEvent {
                    event: WindowEvent::MouseWheel { delta, .. },
                    ..
                } => {
                    let scroll = match delta {
                        MouseScrollDelta::LineDelta(_, y) => y,
                        MouseScrollDelta::PixelDelta(pos) => pos.y as f32 * 0.1,
                    };
                    camera_input.scroll_delta = scroll;
                }

                Event::WindowEvent {
                    event: WindowEvent::RedrawRequested,
                    ..
                } => {
                    let now = Instant::now();
                    let delta_time = (now - last_frame).as_secs_f32();
                    last_frame = now;
                    let time = start_time.elapsed().as_secs_f32();

                    // Update camera
                    camera_controller.update(&mut camera, &camera_input, delta_time);
                    camera_input.mouse_delta = (0.0, 0.0);
                    camera_input.scroll_delta = 0.0;

                    // Update uniforms
                    let camera_uniforms = CameraUniforms {
                        view_proj: camera.view_projection_matrix().to_cols_array_2d(),
                        view: camera.view_matrix().to_cols_array_2d(),
                        projection: camera.projection_matrix().to_cols_array_2d(),
                        camera_pos: camera.position.to_array(),
                        _padding: 0.0,
                    };
                    queue.write_buffer(&camera_buffer, 0, bytemuck::bytes_of(&camera_uniforms));

                    // Slowly rotate the model
                    let model_rotation = Mat4::from_rotation_y(time * 0.3);
                    let model_uniforms = ModelUniforms {
                        model: model_rotation.to_cols_array_2d(),
                        normal_matrix: model_rotation.to_cols_array_2d(),
                    };
                    queue.write_buffer(&model_buffer, 0, bytemuck::bytes_of(&model_uniforms));

                    let material_uniforms = MaterialUniforms {
                        base_color_factor: base_color,
                        emissive_factor: [0.0, 0.0, 0.0],
                        metallic_factor: metallic,
                        roughness_factor: roughness,
                        normal_scale: 1.0,
                        occlusion_strength: 1.0,
                        alpha_cutoff: 0.5,
                    };
                    queue.write_buffer(&material_buffer, 0, bytemuck::bytes_of(&material_uniforms));

                    // Light rotates slowly - moderate intensity to show material colors
                    let light_angle = time * 0.2;
                    let light_uniforms = LightUniforms {
                        direction: [light_angle.sin(), -0.6, light_angle.cos()],
                        _pad1: 0.0,
                        color: [1.0, 0.98, 0.95],
                        intensity: 1.8,  // Reduced to prevent washing out colors
                        ambient_color: [0.3, 0.35, 0.5],
                        ambient_intensity: 0.25,
                    };
                    queue.write_buffer(&light_buffer, 0, bytemuck::bytes_of(&light_uniforms));

                    // Render
                    let output = match surface.get_current_texture() {
                        Ok(t) => t,
                        Err(wgpu::SurfaceError::Lost) => {
                            surface.configure(&device, &config);
                            return;
                        }
                        Err(e) => {
                            log::error!("Surface error: {:?}", e);
                            return;
                        }
                    };

                    let view = output
                        .texture
                        .create_view(&wgpu::TextureViewDescriptor::default());
                    let depth_view = depth_texture.create_view(&wgpu::TextureViewDescriptor::default());

                    let mut encoder =
                        device.create_command_encoder(&wgpu::CommandEncoderDescriptor {
                            label: Some("Render Encoder"),
                        });

                    {
                        let mut render_pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                            label: Some("Main Render Pass"),
                            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                                view: &view,
                                resolve_target: None,
                                ops: wgpu::Operations {
                                    load: wgpu::LoadOp::Clear(wgpu::Color {
                                        r: 0.1,
                                        g: 0.1,
                                        b: 0.15,
                                        a: 1.0,
                                    }),
                                    store: wgpu::StoreOp::Store,
                                },
                            })],
                            depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                                view: &depth_view,
                                depth_ops: Some(wgpu::Operations {
                                    load: wgpu::LoadOp::Clear(1.0),
                                    store: wgpu::StoreOp::Store,
                                }),
                                stencil_ops: None,
                            }),
                            timestamp_writes: None,
                            occlusion_query_set: None,
                        });

                        // Draw sky
                        render_pass.set_pipeline(&sky_pipeline);
                        render_pass.draw(0..3, 0..1);

                        // Draw grid
                        render_pass.set_pipeline(&grid_pipeline);
                        render_pass.set_bind_group(0, &camera_bind_group, &[]);
                        render_pass.draw(0..6, 0..1);

                        // Draw model
                        render_pass.set_pipeline(&pbr_pipeline);
                        render_pass.set_bind_group(0, &camera_bind_group, &[]);
                        render_pass.set_bind_group(1, &model_bind_group, &[]);
                        render_pass.set_bind_group(2, &material_bind_group, &[]);
                        render_pass.set_bind_group(3, &light_bind_group, &[]);
                        render_pass.set_vertex_buffer(0, mesh.vertex_buffer.slice(..));
                        render_pass.set_index_buffer(mesh.index_buffer.slice(..), wgpu::IndexFormat::Uint32);
                        render_pass.draw_indexed(0..mesh.index_count, 0, 0..1);
                    }

                    queue.submit(std::iter::once(encoder.finish()));
                    output.present();
                    window.request_redraw();
                }

                _ => {}
            }
        })
        .unwrap();
}

fn create_depth_texture(
    device: &wgpu::Device,
    width: u32,
    height: u32,
    format: wgpu::TextureFormat,
) -> wgpu::Texture {
    device.create_texture(&wgpu::TextureDescriptor {
        label: Some("Depth Texture"),
        size: wgpu::Extent3d {
            width: width.max(1),
            height: height.max(1),
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
        view_formats: &[],
    })
}
