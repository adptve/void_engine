//! Compositor - renders layers and composites them with text support

use std::sync::Arc;
use wgpu::*;
use void_kernel::RenderGraph;
use glyphon::{
    Attrs, Buffer as TextBuffer, Color as TextColor, Family, FontSystem, Metrics,
    Shaping, SwashCache, TextArea, TextAtlas, TextBounds, TextRenderer, Viewport,
};

/// Vertex for rendering
#[repr(C)]
#[derive(Copy, Clone, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct Vertex {
    position: [f32; 2],
    color: [f32; 4],
}

impl Vertex {
    const ATTRIBS: [VertexAttribute; 2] = vertex_attr_array![
        0 => Float32x2,
        1 => Float32x4,
    ];

    fn desc() -> VertexBufferLayout<'static> {
        VertexBufferLayout {
            array_stride: std::mem::size_of::<Vertex>() as BufferAddress,
            step_mode: VertexStepMode::Vertex,
            attributes: &Self::ATTRIBS,
        }
    }
}

/// The compositor renders layers and composites them
pub struct Compositor {
    pipeline: RenderPipeline,
    vertex_buffer: Buffer,
    size: (u32, u32),
    // Text rendering
    font_system: FontSystem,
    swash_cache: SwashCache,
    text_atlas: TextAtlas,
    text_renderer: TextRenderer,
    text_buffer: TextBuffer,
    cache: glyphon::Cache,
}

impl Compositor {
    /// Create a new compositor
    pub fn new(device: &Arc<Device>, queue: &Arc<Queue>, format: TextureFormat) -> Self {
        log::info!("Creating compositor...");

        // Create shader
        let shader = device.create_shader_module(ShaderModuleDescriptor {
            label: Some("compositor_shader"),
            source: ShaderSource::Wgsl(include_str!("shaders/compositor.wgsl").into()),
        });

        // Create pipeline layout
        let pipeline_layout = device.create_pipeline_layout(&PipelineLayoutDescriptor {
            label: Some("compositor_layout"),
            bind_group_layouts: &[],
            push_constant_ranges: &[],
        });

        // Create render pipeline
        let pipeline = device.create_render_pipeline(&RenderPipelineDescriptor {
            label: Some("compositor_pipeline"),
            layout: Some(&pipeline_layout),
            vertex: VertexState {
                module: &shader,
                entry_point: "vs_main",
                buffers: &[Vertex::desc()],
                compilation_options: PipelineCompilationOptions::default(),
            },
            fragment: Some(FragmentState {
                module: &shader,
                entry_point: "fs_main",
                targets: &[Some(ColorTargetState {
                    format,
                    blend: Some(BlendState::ALPHA_BLENDING),
                    write_mask: ColorWrites::ALL,
                })],
                compilation_options: PipelineCompilationOptions::default(),
            }),
            primitive: PrimitiveState {
                topology: PrimitiveTopology::TriangleList,
                ..Default::default()
            },
            depth_stencil: None,
            multisample: MultisampleState::default(),
            multiview: None,
            cache: None,
        });

        // Create vertex buffer
        let vertex_buffer = device.create_buffer(&BufferDescriptor {
            label: Some("compositor_vertices"),
            size: 8192 * std::mem::size_of::<Vertex>() as u64,
            usage: BufferUsages::VERTEX | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        // Initialize text rendering
        let mut font_system = FontSystem::new();
        let swash_cache = SwashCache::new();
        let cache = glyphon::Cache::new(device.as_ref());
        let mut text_atlas = TextAtlas::new(device.as_ref(), queue.as_ref(), &cache, format);
        let text_renderer = TextRenderer::new(
            &mut text_atlas,
            device.as_ref(),
            MultisampleState::default(),
            None,
        );

        // Create text buffer
        let mut text_buffer = TextBuffer::new(
            &mut font_system,
            Metrics::new(18.0, 22.0),
        );
        text_buffer.set_size(&mut font_system, Some(1280.0), Some(720.0));

        log::info!("Compositor created with font rendering");

        Self {
            pipeline,
            vertex_buffer,
            size: (1280, 720),
            font_system,
            swash_cache,
            text_atlas,
            text_renderer,
            text_buffer,
            cache,
        }
    }

    /// Resize the compositor
    pub fn resize(&mut self, width: u32, height: u32) {
        self.size = (width, height);
        self.text_buffer.set_size(
            &mut self.font_system,
            Some(width as f32),
            Some(height as f32),
        );
    }

    /// Render a frame
    pub fn render(
        &mut self,
        device: &Arc<Device>,
        queue: &Arc<Queue>,
        view: &TextureView,
        render_graph: &RenderGraph,
        frame: u64,
        show_shell: bool,
        shell_output: Vec<String>,
    ) {
        let time = frame as f32 * 0.02;
        let mut vertices: Vec<Vertex> = Vec::new();

        // Note: 3D scene is rendered by SceneRenderer before this
        // We only render 2D overlays here

        // === Shell overlay ===
        if show_shell {
            // Shell background panel (left half)
            vertices.extend_from_slice(&self.make_quad(
                -0.98, -0.98, 0.96, 1.96,
                [0.03, 0.03, 0.06, 0.95],
            ));

            // Shell header bar
            vertices.extend_from_slice(&self.make_quad(
                -0.96, 0.88, 0.92, 0.08,
                [0.08, 0.08, 0.15, 1.0],
            ));

            // Header accent line
            vertices.extend_from_slice(&self.make_quad(
                -0.96, 0.87, 0.92, 0.005,
                [0.3, 0.5, 0.9, 1.0],
            ));

            // Status indicator (pulsing green dot)
            let pulse = 0.5 + 0.5 * (time * 2.0).sin();
            vertices.extend_from_slice(&self.make_quad(
                -0.08, 0.895, 0.025, 0.025,
                [0.0, pulse * 0.8 + 0.2, 0.0, 1.0],
            ));

            // Input line background at bottom
            vertices.extend_from_slice(&self.make_quad(
                -0.96, -0.96, 0.92, 0.05,
                [0.08, 0.08, 0.12, 1.0],
            ));
        } else {
            // Mini status indicator when shell is hidden
            vertices.extend_from_slice(&self.make_quad(
                0.90, 0.90, 0.08, 0.08,
                [0.1, 0.1, 0.2, 0.8],
            ));

            // Green dot if running
            vertices.extend_from_slice(&self.make_quad(
                0.92, 0.92, 0.04, 0.04,
                [0.0, 0.8, 0.0, 1.0],
            ));
        }

        // === Frame counter (bottom right) ===
        vertices.extend_from_slice(&self.make_quad(
            0.75, -0.98, 0.23, 0.05,
            [0.06, 0.06, 0.1, 0.9],
        ));

        // Update vertex buffer
        queue.write_buffer(&self.vertex_buffer, 0, bytemuck::cast_slice(&vertices));

        // === Prepare text ===
        let (width, height) = self.size;

        if show_shell {
            // Build shell text content
            let mut text_content = String::new();
            text_content.push_str("══════════════════════════════════════════\n");
            text_content.push_str("       METAVERSE OS SHELL v0.1.0\n");
            text_content.push_str("══════════════════════════════════════════\n\n");

            for line in shell_output.iter().take(22) {
                text_content.push_str(line);
                text_content.push('\n');
            }

            // Add prompt if no input shown
            if shell_output.is_empty() || !shell_output.last().map(|s| s.starts_with(">")).unwrap_or(false) {
                text_content.push_str("\n> ");
            }

            // Update text buffer
            self.text_buffer.set_text(
                &mut self.font_system,
                &text_content,
                Attrs::new()
                    .family(Family::Monospace)
                    .color(TextColor::rgb(180, 190, 210)),
                Shaping::Advanced,
            );
        } else {
            self.text_buffer.set_text(
                &mut self.font_system,
                "",
                Attrs::new(),
                Shaping::Advanced,
            );
        }

        // Create text areas
        let text_areas: Vec<TextArea> = if show_shell {
            vec![TextArea {
                buffer: &self.text_buffer,
                left: 18.0,
                top: 45.0,
                scale: 1.0,
                bounds: TextBounds {
                    left: 0,
                    top: 0,
                    right: (width as f32 * 0.48) as i32,
                    bottom: (height as f32 * 0.92) as i32,
                },
                default_color: TextColor::rgb(180, 190, 210),
                custom_glyphs: &[],
            }]
        } else {
            vec![]
        };

        // Create viewport for text rendering
        let mut viewport = Viewport::new(device.as_ref(), &self.cache);
        viewport.update(queue.as_ref(), glyphon::Resolution { width, height });

        // Prepare text renderer
        self.text_renderer.prepare(
            device.as_ref(),
            queue.as_ref(),
            &mut self.font_system,
            &mut self.text_atlas,
            &viewport,
            text_areas,
            &mut self.swash_cache,
        ).expect("Failed to prepare text");

        // Create command encoder
        let mut encoder = device.create_command_encoder(&CommandEncoderDescriptor {
            label: Some("compositor_encoder"),
        });

        // Render pass - Load existing content (scene renderer draws first)
        {
            let mut render_pass = encoder.begin_render_pass(&RenderPassDescriptor {
                label: Some("compositor_pass"),
                color_attachments: &[Some(RenderPassColorAttachment {
                    view,
                    resolve_target: None,
                    ops: Operations {
                        load: LoadOp::Load, // Don't clear - scene_renderer already rendered
                        store: StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                occlusion_query_set: None,
                timestamp_writes: None,
            });

            // Draw background and shapes
            render_pass.set_pipeline(&self.pipeline);
            render_pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
            render_pass.draw(0..vertices.len() as u32, 0..1);

            // Draw text
            self.text_renderer.render(&self.text_atlas, &viewport, &mut render_pass)
                .expect("Failed to render text");
        }

        queue.submit(std::iter::once(encoder.finish()));

        // Trim atlas occasionally
        self.text_atlas.trim();
    }

    /// Create a quad from position and size
    fn make_quad(&self, x: f32, y: f32, w: f32, h: f32, color: [f32; 4]) -> [Vertex; 6] {
        [
            Vertex { position: [x, y], color },
            Vertex { position: [x + w, y], color },
            Vertex { position: [x, y + h], color },
            Vertex { position: [x, y + h], color },
            Vertex { position: [x + w, y], color },
            Vertex { position: [x + w, y + h], color },
        ]
    }
}
