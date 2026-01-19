//! GPU resources for rendering.
//!
//! This module contains mesh buffers, pipelines, and shaders
//! for the 3D viewport rendering.

use std::collections::HashMap;
use wgpu::util::DeviceExt;
use void_asset_server::loaders::{MeshLoader, Vertex};
use crate::core::editor_state::MeshType;

/// GPU mesh data.
pub struct GpuMesh {
    pub vertex_buffer: wgpu::Buffer,
    pub index_buffer: wgpu::Buffer,
    pub index_count: u32,
}

/// Maximum number of entities that can be rendered.
pub const MAX_ENTITIES: usize = 256;

/// Uniforms passed to shaders.
#[repr(C)]
#[derive(Copy, Clone, bytemuck::Pod, bytemuck::Zeroable)]
pub struct Uniforms {
    pub view_proj: [[f32; 4]; 4],
    pub model: [[f32; 4]; 4],
    pub color: [f32; 4],
}

/// All GPU resources for the editor viewport.
pub struct GpuResources {
    pub meshes: HashMap<MeshType, GpuMesh>,
    pub grid_mesh: GpuMesh,
    pub pipeline: wgpu::RenderPipeline,
    pub line_pipeline: wgpu::RenderPipeline,
    pub uniform_buffer: wgpu::Buffer,
    pub uniform_bind_group: wgpu::BindGroup,
    pub uniform_alignment: u32,
}

impl GpuResources {
    /// Create new GPU resources.
    pub fn new(device: &wgpu::Device, format: wgpu::TextureFormat) -> Self {
        // Create shader
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("Editor Shader"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SOURCE.into()),
        });

        // Calculate uniform buffer alignment
        let uniform_size = std::mem::size_of::<Uniforms>() as u32;
        let uniform_alignment = 256u32; // wgpu requires 256-byte alignment for dynamic offsets
        let aligned_size = ((uniform_size + uniform_alignment - 1) / uniform_alignment) * uniform_alignment;

        // Create uniform buffer
        let uniform_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Uniform Buffer"),
            size: (aligned_size as usize * MAX_ENTITIES) as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        // Create bind group layout
        let bind_group_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("Uniform Bind Group Layout"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX | wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: true,
                    min_binding_size: wgpu::BufferSize::new(uniform_size as u64),
                },
                count: None,
            }],
        });

        // Create bind group
        let uniform_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("Uniform Bind Group"),
            layout: &bind_group_layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: wgpu::BindingResource::Buffer(wgpu::BufferBinding {
                    buffer: &uniform_buffer,
                    offset: 0,
                    size: wgpu::BufferSize::new(uniform_size as u64),
                }),
            }],
        });

        // Create pipeline layout
        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("Pipeline Layout"),
            bind_group_layouts: &[&bind_group_layout],
            push_constant_ranges: &[],
        });

        // Create render pipeline
        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("Editor Pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: "vs_main",
                buffers: &[wgpu::VertexBufferLayout {
                    array_stride: std::mem::size_of::<Vertex>() as u64,
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
                    ],
                }],
                compilation_options: Default::default(),
            },
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: "fs_main",
                targets: &[Some(wgpu::ColorTargetState {
                    format,
                    blend: Some(wgpu::BlendState::REPLACE),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
                compilation_options: Default::default(),
            }),
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: Some(wgpu::Face::Back),
                unclipped_depth: false,
                polygon_mode: wgpu::PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: wgpu::TextureFormat::Depth32Float,
                depth_write_enabled: true,
                depth_compare: wgpu::CompareFunction::Less,
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState::default(),
            multiview: None,
            cache: None,
        });

        // Create line pipeline
        let line_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("Grid Line Pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: "vs_main",
                buffers: &[wgpu::VertexBufferLayout {
                    array_stride: std::mem::size_of::<Vertex>() as u64,
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
                    ],
                }],
                compilation_options: Default::default(),
            },
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: "fs_main",
                targets: &[Some(wgpu::ColorTargetState {
                    format,
                    blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
                compilation_options: Default::default(),
            }),
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::LineList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None,
                unclipped_depth: false,
                polygon_mode: wgpu::PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: wgpu::TextureFormat::Depth32Float,
                depth_write_enabled: true,
                depth_compare: wgpu::CompareFunction::Less,
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState::default(),
            multiview: None,
            cache: None,
        });

        // Create meshes
        let meshes = Self::create_meshes(device);
        let grid_mesh = Self::create_grid_mesh(device, 20, 1.0);

        Self {
            meshes,
            grid_mesh,
            pipeline,
            line_pipeline,
            uniform_buffer,
            uniform_bind_group,
            uniform_alignment: aligned_size,
        }
    }

    fn create_meshes(device: &wgpu::Device) -> HashMap<MeshType, GpuMesh> {
        let mut meshes = HashMap::new();

        let mesh_data = [
            (MeshType::Cube, MeshLoader::cube()),
            (MeshType::Sphere, MeshLoader::sphere(16, 12)),
            (MeshType::Cylinder, MeshLoader::cylinder(16)),
            (MeshType::Diamond, MeshLoader::diamond()),
            (MeshType::Torus, MeshLoader::torus(16, 24, 0.2)),
            (MeshType::Plane, MeshLoader::plane(1)),
        ];

        for (mesh_type, asset) in mesh_data {
            // Use the first primitive's data
            let primitive = &asset.primitives[0];
            let indices = primitive.indices.as_ref().expect("Mesh must have indices");

            let vertex_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
                label: Some(&format!("{:?} Vertex Buffer", mesh_type)),
                contents: bytemuck::cast_slice(&primitive.vertices),
                usage: wgpu::BufferUsages::VERTEX,
            });

            let index_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
                label: Some(&format!("{:?} Index Buffer", mesh_type)),
                contents: bytemuck::cast_slice(indices),
                usage: wgpu::BufferUsages::INDEX,
            });

            meshes.insert(mesh_type, GpuMesh {
                vertex_buffer,
                index_buffer,
                index_count: indices.len() as u32,
            });
        }

        meshes
    }

    fn create_grid_mesh(device: &wgpu::Device, size: i32, spacing: f32) -> GpuMesh {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();

        let half = size as f32 * spacing / 2.0;
        let mut idx = 0u32;

        for i in -size..=size {
            let pos = i as f32 * spacing;

            // Lines parallel to X axis
            vertices.push(Vertex {
                position: [-half, 0.0, pos],
                normal: [0.0, 1.0, 0.0],
                tangent: [1.0, 0.0, 0.0, 1.0],
                uv0: [0.0, 0.0],
                uv1: [0.0, 0.0],
                color: [1.0, 1.0, 1.0, 1.0],
            });
            vertices.push(Vertex {
                position: [half, 0.0, pos],
                normal: [0.0, 1.0, 0.0],
                tangent: [1.0, 0.0, 0.0, 1.0],
                uv0: [1.0, 0.0],
                uv1: [0.0, 0.0],
                color: [1.0, 1.0, 1.0, 1.0],
            });
            indices.push(idx);
            indices.push(idx + 1);
            idx += 2;

            // Lines parallel to Z axis
            vertices.push(Vertex {
                position: [pos, 0.0, -half],
                normal: [0.0, 1.0, 0.0],
                tangent: [1.0, 0.0, 0.0, 1.0],
                uv0: [0.0, 0.0],
                uv1: [0.0, 0.0],
                color: [1.0, 1.0, 1.0, 1.0],
            });
            vertices.push(Vertex {
                position: [pos, 0.0, half],
                normal: [0.0, 1.0, 0.0],
                tangent: [1.0, 0.0, 0.0, 1.0],
                uv0: [0.0, 1.0],
                uv1: [0.0, 0.0],
                color: [1.0, 1.0, 1.0, 1.0],
            });
            indices.push(idx);
            indices.push(idx + 1);
            idx += 2;
        }

        let vertex_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("Grid Vertex Buffer"),
            contents: bytemuck::cast_slice(&vertices),
            usage: wgpu::BufferUsages::VERTEX,
        });

        let index_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("Grid Index Buffer"),
            contents: bytemuck::cast_slice(&indices),
            usage: wgpu::BufferUsages::INDEX,
        });

        GpuMesh {
            vertex_buffer,
            index_buffer,
            index_count: indices.len() as u32,
        }
    }
}

/// Shader source for the editor.
pub const SHADER_SOURCE: &str = r#"
struct Uniforms {
    view_proj: mat4x4<f32>,
    model: mat4x4<f32>,
    color: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
}

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) normal: vec3<f32>,
    @location(1) uv: vec2<f32>,
}

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.view_proj * uniforms.model * vec4<f32>(in.position, 1.0);
    out.normal = (uniforms.model * vec4<f32>(in.normal, 0.0)).xyz;
    out.uv = in.uv;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let light_dir = normalize(vec3<f32>(0.5, 1.0, 0.3));
    let normal = normalize(in.normal);
    let diffuse = max(dot(normal, light_dir), 0.0);
    let ambient = 0.3;
    let lighting = ambient + diffuse * 0.7;
    return vec4<f32>(uniforms.color.rgb * lighting, uniforms.color.a);
}
"#;
