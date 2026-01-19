//! Void Engine Visual Editor
//!
//! Entry point for the visual scene editor application.
//!
//! This is a thin wrapper that initializes the editor application
//! using the modular library components.

use std::sync::Arc;

use egui::Context as EguiContext;
use egui_wgpu::ScreenDescriptor;
use egui_winit::State as EguiState;
use winit::{
    dpi::PhysicalSize,
    event::{ElementState, Event, MouseButton, WindowEvent},
    event_loop::{ActiveEventLoop, ControlFlow, EventLoop},
    keyboard::{Key, NamedKey},
    window::{Window, WindowAttributes},
};

use void_editor::{
    core::{EditorState, SelectionMode, MeshType},
    gpu::{GpuResources, Uniforms, MAX_ENTITIES},
    panels::Console,
    viewport::gizmos::GizmoMode,
    tools::{ToolId, SELECTION_TOOL_ID, MOVE_TOOL_ID, ROTATE_TOOL_ID, SCALE_TOOL_ID, CREATION_TOOL_ID},
};

// ============================================================================
// Application
// ============================================================================

struct EditorApp {
    window: Arc<Window>,
    device: wgpu::Device,
    queue: wgpu::Queue,
    surface: wgpu::Surface<'static>,
    surface_config: wgpu::SurfaceConfiguration,
    depth_texture: wgpu::Texture,
    depth_view: wgpu::TextureView,

    egui_ctx: EguiContext,
    egui_state: EguiState,
    egui_renderer: egui_wgpu::Renderer,

    gpu: GpuResources,
    state: EditorState,

    // Frame timing
    last_frame_time: std::time::Instant,
    frame_times: [f32; 60],
    frame_time_index: usize,
}

impl EditorApp {
    fn create_depth_texture(device: &wgpu::Device, width: u32, height: u32) -> (wgpu::Texture, wgpu::TextureView) {
        let texture = device.create_texture(&wgpu::TextureDescriptor {
            label: Some("Depth Texture"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Depth32Float,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            view_formats: &[],
        });
        let view = texture.create_view(&wgpu::TextureViewDescriptor::default());
        (texture, view)
    }

    fn new(event_loop: &ActiveEventLoop) -> Self {
        let window_attributes = WindowAttributes::default()
            .with_title("Void Engine Editor")
            .with_inner_size(PhysicalSize::new(1280, 720));
        let window = Arc::new(event_loop.create_window(window_attributes).unwrap());

        let size = window.inner_size();

        // Create wgpu instance - use DX12 on Windows to avoid potential Vulkan driver issues
        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
            #[cfg(target_os = "windows")]
            backends: wgpu::Backends::DX12,
            #[cfg(not(target_os = "windows"))]
            backends: wgpu::Backends::PRIMARY,
            ..Default::default()
        });

        let surface = instance.create_surface(window.clone()).unwrap();

        let adapter = pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
            power_preference: wgpu::PowerPreference::HighPerformance,
            compatible_surface: Some(&surface),
            force_fallback_adapter: false,
        })).expect("Failed to find adapter");

        let (device, queue) = pollster::block_on(adapter.request_device(
            &wgpu::DeviceDescriptor {
                label: Some("Editor Device"),
                required_features: wgpu::Features::empty(),
                required_limits: wgpu::Limits::default(),
                memory_hints: wgpu::MemoryHints::default(),
            },
            None,
        )).expect("Failed to create device");

        let caps = surface.get_capabilities(&adapter);
        let format = caps.formats.iter()
            .find(|f| f.is_srgb())
            .copied()
            .unwrap_or(caps.formats[0]);

        let surface_config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            format,
            width: size.width,
            height: size.height,
            present_mode: wgpu::PresentMode::AutoVsync,
            alpha_mode: caps.alpha_modes[0],
            view_formats: vec![],
            desired_maximum_frame_latency: 2,
        };
        surface.configure(&device, &surface_config);

        // Create depth texture
        let (depth_texture, depth_view) = Self::create_depth_texture(&device, size.width, size.height);

        // Setup egui
        let egui_ctx = EguiContext::default();
        let egui_state = EguiState::new(
            egui_ctx.clone(),
            egui_ctx.viewport_id(),
            &window,
            Some(window.scale_factor() as f32),
            None,
            None, // max_texture_side
        );
        let egui_renderer = egui_wgpu::Renderer::new(&device, format, None, 1, false);

        // Create GPU resources
        let gpu = GpuResources::new(&device, format);

        // Create editor state
        let mut state = EditorState::new();
        state.console.info("Void Engine Editor initialized");
        state.console.info("Welcome to the world-class 3D scene editor!");

        // Skip asset/prefab init for now to debug heap corruption
        // state.init_asset_database();
        // state.init_prefab_library();

        Self {
            window,
            device,
            queue,
            surface,
            surface_config,
            depth_texture,
            depth_view,
            egui_ctx,
            egui_state,
            egui_renderer,
            gpu,
            state,
            last_frame_time: std::time::Instant::now(),
            frame_times: [16.67; 60], // Initialize to ~60 FPS
            frame_time_index: 0,
        }
    }

    fn resize(&mut self, size: PhysicalSize<u32>) {
        if size.width > 0 && size.height > 0 {
            self.surface_config.width = size.width;
            self.surface_config.height = size.height;
            self.surface.configure(&self.device, &self.surface_config);

            let (texture, view) = Self::create_depth_texture(&self.device, size.width, size.height);
            self.depth_texture = texture;
            self.depth_view = view;

            self.state.viewport.resize(size.width, size.height);
        }
    }

    fn update_window_title(&self) {
        let scene_name = self.state.scene_path.as_ref()
            .and_then(|p| p.file_stem())
            .map(|s| s.to_string_lossy().to_string())
            .unwrap_or_else(|| "Untitled".to_string());

        let modified = if self.state.scene_modified { " *" } else { "" };

        let title = format!("{}{} - Void Engine Editor", scene_name, modified);
        self.window.set_title(&title);
    }

    fn update_frame_timing(&mut self) {
        let now = std::time::Instant::now();
        let dt = now.duration_since(self.last_frame_time).as_secs_f32() * 1000.0;
        self.last_frame_time = now;

        self.frame_times[self.frame_time_index] = dt;
        self.frame_time_index = (self.frame_time_index + 1) % self.frame_times.len();
    }

    fn average_fps(&self) -> f32 {
        let avg_ms: f32 = self.frame_times.iter().sum::<f32>() / self.frame_times.len() as f32;
        if avg_ms > 0.0 { 1000.0 / avg_ms } else { 0.0 }
    }

    fn average_frame_time_ms(&self) -> f32 {
        self.frame_times.iter().sum::<f32>() / self.frame_times.len() as f32
    }

    fn render(&mut self) {
        // Update frame timing
        self.update_frame_timing();

        // Update window title to reflect current scene
        self.update_window_title();

        // Process pending thumbnail generation
        self.state.process_thumbnails();

        let output = match self.surface.get_current_texture() {
            Ok(t) => t,
            Err(_) => return,
        };
        let view = output.texture.create_view(&wgpu::TextureViewDescriptor::default());

        // Run egui
        let raw_input = self.egui_state.take_egui_input(self.window.as_ref());
        let ctx = self.egui_ctx.clone();
        let full_output = ctx.run(raw_input, |ctx| {
            self.draw_ui(ctx);
        });

        self.egui_state.handle_platform_output(self.window.as_ref(), full_output.platform_output);
        let paint_jobs = self.egui_ctx.tessellate(full_output.shapes, full_output.pixels_per_point);

        let screen_descriptor = ScreenDescriptor {
            size_in_pixels: [self.surface_config.width, self.surface_config.height],
            pixels_per_point: self.window.scale_factor() as f32,
        };

        for (id, delta) in &full_output.textures_delta.set {
            self.egui_renderer.update_texture(&self.device, &self.queue, *id, delta);
        }

        let mut encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor {
            label: Some("Editor Encoder"),
        });

        // Render 3D scene
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("3D Pass"),
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
                    view: &self.depth_view,
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(1.0),
                        store: wgpu::StoreOp::Store,
                    }),
                    stencil_ops: None,
                }),
                timestamp_writes: None,
                occlusion_query_set: None,
            });

            let aspect = self.surface_config.width as f32 / self.surface_config.height as f32;
            let view_proj = self.compute_view_projection(aspect);
            let alignment = self.gpu.uniform_alignment as u64;

            // Draw grid first (if enabled)
            if self.state.preferences.show_grid {
                let grid_uniforms = Uniforms {
                    view_proj,
                    model: identity_matrix(),
                    color: [0.3, 0.3, 0.35, 0.8],
                };
                self.queue.write_buffer(&self.gpu.uniform_buffer, 0, bytemuck::bytes_of(&grid_uniforms));

                pass.set_pipeline(&self.gpu.line_pipeline);
                pass.set_bind_group(0, &self.gpu.uniform_bind_group, &[0]);
                pass.set_vertex_buffer(0, self.gpu.grid_mesh.vertex_buffer.slice(..));
                pass.set_index_buffer(self.gpu.grid_mesh.index_buffer.slice(..), wgpu::IndexFormat::Uint32);
                pass.draw_indexed(0..self.gpu.grid_mesh.index_count, 0, 0..1);
            }

            // Write all uniforms to buffer
            let entity_start_slot = 1;
            for (idx, entity) in self.state.entities.iter().enumerate() {
                if idx >= MAX_ENTITIES - 1 {
                    break;
                }

                let model = self.compute_model_matrix(entity);
                let color = if self.state.selection.is_selected(entity.id) {
                    [1.0, 0.8, 0.2, 1.0] // Yellow for selected
                } else {
                    [entity.color[0], entity.color[1], entity.color[2], 1.0]
                };

                let uniforms = Uniforms {
                    view_proj,
                    model,
                    color,
                };

                let offset = (idx + entity_start_slot) as u64 * alignment;
                self.queue.write_buffer(&self.gpu.uniform_buffer, offset, bytemuck::bytes_of(&uniforms));
            }

            pass.set_pipeline(&self.gpu.pipeline);

            // Draw each entity
            for (idx, entity) in self.state.entities.iter().enumerate() {
                if idx >= MAX_ENTITIES - 1 {
                    break;
                }

                let dynamic_offset = ((idx + entity_start_slot) as u32) * self.gpu.uniform_alignment;
                pass.set_bind_group(0, &self.gpu.uniform_bind_group, &[dynamic_offset]);

                if let Some(mesh) = self.gpu.meshes.get(&entity.mesh_type) {
                    pass.set_vertex_buffer(0, mesh.vertex_buffer.slice(..));
                    pass.set_index_buffer(mesh.index_buffer.slice(..), wgpu::IndexFormat::Uint32);
                    pass.draw_indexed(0..mesh.index_count, 0, 0..1);
                }
            }
        }

        // Render egui
        self.egui_renderer.update_buffers(&self.device, &self.queue, &mut encoder, &paint_jobs, &screen_descriptor);
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Egui Pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &view,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Load,
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
            });
            self.egui_renderer.render(&mut pass, &paint_jobs, &screen_descriptor);
        }

        for id in &full_output.textures_delta.free {
            self.egui_renderer.free_texture(id);
        }

        self.queue.submit(std::iter::once(encoder.finish()));
        output.present();
    }

    fn compute_view_projection(&self, aspect: f32) -> [[f32; 4]; 4] {
        let yaw = self.state.viewport.camera_yaw;
        let pitch = self.state.viewport.camera_pitch;
        let dist = self.state.viewport.camera_distance;

        let eye = [
            dist * pitch.cos() * yaw.sin(),
            dist * pitch.sin() + 1.0,
            dist * pitch.cos() * yaw.cos(),
        ];

        let target = [0.0, 1.0, 0.0];
        let up = [0.0, 1.0, 0.0];

        let view = look_at(eye, target, up);
        let proj = perspective(std::f32::consts::FRAC_PI_4, aspect, 0.1, 100.0);
        mat4_mul(proj, view)
    }

    fn compute_model_matrix(&self, entity: &void_editor::core::editor_state::SceneEntity) -> [[f32; 4]; 4] {
        let scale = scale_matrix(entity.transform.scale);
        let rotation = rotation_matrix(entity.transform.rotation);
        let translation = translation_matrix(entity.transform.position);
        mat4_mul(translation, mat4_mul(rotation, scale))
    }

    fn switch_tool(&mut self, tool_id: ToolId) {
        // Take tools out temporarily to avoid borrow conflict
        let mut tools = std::mem::take(&mut self.state.tools);
        tools.switch_to(tool_id, &mut self.state);
        let tool_name = tools.active().map(|t| t.name().to_string());
        // Put tools back
        self.state.tools = tools;

        if let Some(name) = tool_name {
            self.state.set_status(format!("{} tool", name));
        }
    }

    fn handle_asset_drop(&mut self, asset: void_editor::core::DraggedAsset) {
        use void_editor::panels::AssetType;

        match asset.asset_type {
            AssetType::Mesh => {
                // Create a mesh entity from the dropped asset
                let entity_name = asset.path.file_stem()
                    .map(|s| s.to_string_lossy().to_string())
                    .unwrap_or_else(|| asset.name.clone());

                // Determine mesh type from file extension
                let mesh_type = if asset.path.extension().map(|e| e == "obj" || e == "gltf" || e == "glb").unwrap_or(false) {
                    // For now, create a cube placeholder for external meshes
                    // In a full implementation, this would load the actual mesh
                    MeshType::Cube
                } else {
                    MeshType::Cube
                };

                let id = self.state.create_entity(entity_name.clone(), mesh_type);

                // Position at camera target
                let camera_target = self.state.viewport.camera_target();
                if let Some(entity) = self.state.get_entity_mut(id) {
                    entity.transform.position = camera_target;
                }

                // Select the new entity
                self.state.selection.select(id, SelectionMode::Replace);
                self.state.set_status(format!("Dropped: {} (mesh)", entity_name));
            }
            AssetType::Scene => {
                // For scenes, we could load/merge them
                self.state.set_status(format!("Scene drop not yet implemented: {}", asset.name));
            }
            _ => {
                self.state.set_status(format!("Cannot drop {} assets", asset.asset_type.name()));
            }
        }
    }

    fn draw_ui(&mut self, ctx: &EguiContext) {
        // Top menu bar
        egui::TopBottomPanel::top("menu_bar").show(ctx, |ui| {
            egui::menu::bar(ui, |ui| {
                self.file_menu(ui);
                self.edit_menu(ui);
                self.view_menu(ui);
                self.create_menu(ui);
                self.help_menu(ui);
            });
        });

        // Tool toolbar
        egui::TopBottomPanel::top("tool_toolbar").show(ctx, |ui| {
            ui.horizontal(|ui| {
                // Tool selection
                ui.label("Tools:");

                let active_tool = self.state.tools.active_id();

                if ui.selectable_label(active_tool == Some(SELECTION_TOOL_ID), "Select (Q)").clicked() {
                    self.switch_tool(SELECTION_TOOL_ID);
                }
                if ui.selectable_label(active_tool == Some(MOVE_TOOL_ID), "Move (W)").clicked() {
                    self.switch_tool(MOVE_TOOL_ID);
                    self.state.gizmos.set_mode(GizmoMode::Translate);
                }
                if ui.selectable_label(active_tool == Some(ROTATE_TOOL_ID), "Rotate (E)").clicked() {
                    self.switch_tool(ROTATE_TOOL_ID);
                    self.state.gizmos.set_mode(GizmoMode::Rotate);
                }
                if ui.selectable_label(active_tool == Some(SCALE_TOOL_ID), "Scale (R)").clicked() {
                    self.switch_tool(SCALE_TOOL_ID);
                    self.state.gizmos.set_mode(GizmoMode::Scale);
                }
                if ui.selectable_label(active_tool == Some(CREATION_TOOL_ID), "Create (C)").clicked() {
                    self.switch_tool(CREATION_TOOL_ID);
                }

                ui.separator();

                // Gizmo options (only show for transform tools)
                if matches!(active_tool, Some(id) if id == MOVE_TOOL_ID || id == ROTATE_TOOL_ID || id == SCALE_TOOL_ID) {
                    // Snap toggle
                    let snap_text = if self.state.gizmos.snap.enabled { "Snap: ON" } else { "Snap: OFF" };
                    if ui.button(snap_text).clicked() {
                        self.state.gizmos.toggle_snap();
                    }

                    // Space toggle
                    let space_text = match self.state.gizmos.space {
                        void_editor::viewport::GizmoSpace::Local => "Local",
                        void_editor::viewport::GizmoSpace::World => "World",
                    };
                    if ui.button(format!("Space: {}", space_text)).clicked() {
                        self.state.gizmos.toggle_space();
                    }
                }

                // Creation tool options
                if active_tool == Some(CREATION_TOOL_ID) {
                    ui.label("Primitive:");
                    let current_type = self.state.creation_primitive_type;
                    for mesh_type in MeshType::all() {
                        if ui.selectable_label(current_type == *mesh_type, mesh_type.name()).clicked() {
                            self.state.creation_primitive_type = *mesh_type;
                            self.state.set_status(format!("Primitive: {}", mesh_type.name()));
                        }
                    }
                }

                // Show active tool name
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if let Some(tool) = self.state.tools.active() {
                        ui.label(format!("Active: {}", tool.name()));
                    }
                });
            });
        });

        // Bottom status bar
        let fps = self.average_fps();
        let frame_time = self.average_frame_time_ms();
        let selection_count = self.state.selection.count();
        let camera_distance = self.state.viewport.camera_distance;

        egui::TopBottomPanel::bottom("status_bar").show(ctx, |ui| {
            ui.horizontal(|ui| {
                // Status message
                ui.label(&self.state.status_message);
                ui.separator();

                // Entity count
                ui.label(format!("Entities: {}", self.state.entities.len()));
                ui.separator();

                // Selection info
                if selection_count > 1 {
                    ui.label(format!("{} selected", selection_count));
                } else if let Some(id) = self.state.selection.primary() {
                    if let Some(entity) = self.state.get_entity(id) {
                        ui.label(format!(
                            "{} ({:.1}, {:.1}, {:.1})",
                            entity.name,
                            entity.transform.position[0],
                            entity.transform.position[1],
                            entity.transform.position[2]
                        ));
                    }
                } else {
                    ui.label("No selection");
                }

                // Right-aligned info
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    // FPS counter
                    ui.label(format!("{:.0} FPS ({:.1}ms)", fps, frame_time));
                    ui.separator();

                    // Camera distance
                    ui.label(format!("Zoom: {:.1}", camera_distance));
                    ui.separator();

                    // Modified indicator and scene name
                    if self.state.is_modified() {
                        ui.colored_label(egui::Color32::from_rgb(255, 180, 100), "*");
                    }
                    if let Some(path) = &self.state.scene_path {
                        ui.label(path.file_name().unwrap_or_default().to_string_lossy().to_string());
                    } else {
                        ui.label("Untitled");
                    }
                });
            });
        });

        // Hierarchy panel (left)
        if self.state.preferences.show_hierarchy {
            self.hierarchy_panel(ctx);
        }

        // Inspector panel (right)
        if self.state.preferences.show_inspector {
            self.inspector_panel(ctx);
        }

        // Bottom panels (console, asset browser)
        if self.state.preferences.show_console || self.state.preferences.show_asset_browser {
            self.bottom_panels(ctx);
        }

        // Render gizmos as overlay
        self.render_gizmo_overlay(ctx);

        // About dialog
        if self.state.show_about {
            self.about_dialog(ctx);
        }

        // Keyboard shortcuts dialog
        if self.state.show_shortcuts {
            self.shortcuts_dialog(ctx);
        }

        // Stats overlay
        if self.state.preferences.show_stats {
            self.stats_overlay(ctx);
        }
    }

    fn render_gizmo_overlay(&mut self, ctx: &EguiContext) {
        // Only render if there's a selection
        if let Some(id) = self.state.selection.primary() {
            // Copy entity transform to avoid borrow conflicts
            let entity_transform = self.state.get_entity(id).map(|e| e.transform);

            if let Some(transform) = entity_transform {
                // Update gizmo renderer with current view-projection
                let aspect = self.surface_config.width as f32 / self.surface_config.height as f32;
                let view_proj = self.compute_view_projection(aspect);
                let camera_eye = self.state.viewport.camera_eye();

                self.state.gizmos.update_renderer(
                    view_proj,
                    [self.surface_config.width as f32, self.surface_config.height as f32],
                    camera_eye,
                );

                // Update gizmo scale based on camera distance
                self.state.gizmos.update_scale(camera_eye, transform.position);

                // Get values for rendering
                let mode = self.state.gizmos.mode;
                let hovered_part = self.state.gizmos.hovered_part;
                let gizmo_scale = self.state.gizmos.gizmo_scale;

                // Paint gizmos using egui painter
                let painter = ctx.layer_painter(egui::LayerId::new(
                    egui::Order::Foreground,
                    egui::Id::new("gizmo_overlay"),
                ));

                self.state.gizmos.renderer.render_gizmo(
                    &painter,
                    &transform,
                    mode,
                    hovered_part,
                    gizmo_scale,
                );
            }
        }
    }

    fn file_menu(&mut self, ui: &mut egui::Ui) {
        ui.menu_button("File", |ui| {
            if ui.button("New Scene (Ctrl+N)").clicked() {
                self.state.new_scene();
                ui.close_menu();
            }
            if ui.button("Open... (Ctrl+O)").clicked() {
                if let Some(path) = rfd::FileDialog::new()
                    .add_filter("Scene Files", &["toml", "scene.toml"])
                    .add_filter("All Files", &["*"])
                    .pick_file()
                {
                    if let Err(e) = self.state.load_scene(path) {
                        self.state.console.error(format!("Failed to load scene: {}", e));
                    }
                }
                ui.close_menu();
            }

            // Recent files submenu
            ui.menu_button("Open Recent", |ui| {
                let recent: Vec<_> = self.state.recent_files.files().cloned().collect();
                if recent.is_empty() {
                    ui.label("No recent files");
                } else {
                    for path in recent {
                        let name = path.file_name()
                            .map(|n| n.to_string_lossy().to_string())
                            .unwrap_or_else(|| path.to_string_lossy().to_string());
                        if ui.button(&name).clicked() {
                            if let Err(e) = self.state.load_scene(path) {
                                self.state.console.error(format!("Failed to load scene: {}", e));
                            }
                            ui.close_menu();
                        }
                    }
                    ui.separator();
                    if ui.button("Clear Recent").clicked() {
                        self.state.recent_files.clear();
                        ui.close_menu();
                    }
                }
            });

            ui.separator();

            // Save - use current path or prompt
            let save_label = if self.state.scene_path.is_some() {
                "Save (Ctrl+S)"
            } else {
                "Save (Ctrl+S)..."
            };
            if ui.button(save_label).clicked() {
                match self.state.save_scene_current() {
                    Ok(true) => {} // Saved successfully
                    Ok(false) => {
                        // No path set, show save dialog
                        if let Some(path) = rfd::FileDialog::new()
                            .add_filter("Scene Files", &["toml"])
                            .set_file_name("scene.toml")
                            .save_file()
                        {
                            if let Err(e) = self.state.save_scene(path) {
                                self.state.console.error(format!("Failed to save scene: {}", e));
                            }
                        }
                    }
                    Err(e) => {
                        self.state.console.error(format!("Failed to save scene: {}", e));
                    }
                }
                ui.close_menu();
            }

            if ui.button("Save As...").clicked() {
                if let Some(path) = rfd::FileDialog::new()
                    .add_filter("Scene Files", &["toml"])
                    .set_file_name("scene.toml")
                    .save_file()
                {
                    if let Err(e) = self.state.save_scene(path) {
                        self.state.console.error(format!("Failed to save scene: {}", e));
                    }
                }
                ui.close_menu();
            }

            ui.separator();

            // Show current file info
            if let Some(path) = &self.state.scene_path {
                let name = path.file_name()
                    .map(|n| n.to_string_lossy().to_string())
                    .unwrap_or_else(|| "Unknown".to_string());
                let modified = if self.state.scene_modified { " *" } else { "" };
                ui.label(format!("Current: {}{}", name, modified));
                ui.separator();
            }

            if ui.button("Exit").clicked() {
                std::process::exit(0);
            }
        });
    }

    fn edit_menu(&mut self, ui: &mut egui::Ui) {
        ui.menu_button("Edit", |ui| {
            let can_undo = self.state.can_undo();
            let can_redo = self.state.can_redo();

            // Undo button with description
            let undo_label = if let Some(desc) = self.state.undo_description() {
                format!("Undo: {} (Ctrl+Z)", desc)
            } else {
                "Undo (Ctrl+Z)".to_string()
            };
            if ui.add_enabled(can_undo, egui::Button::new(undo_label)).clicked() {
                self.state.undo();
                ui.close_menu();
            }

            // Redo button with description
            let redo_label = if let Some(desc) = self.state.redo_description() {
                format!("Redo: {} (Ctrl+Y)", desc)
            } else {
                "Redo (Ctrl+Y)".to_string()
            };
            if ui.add_enabled(can_redo, egui::Button::new(redo_label)).clicked() {
                self.state.redo();
                ui.close_menu();
            }

            ui.separator();

            // History info
            ui.label(format!(
                "History: {} undo, {} redo",
                self.state.history.undo_count(),
                self.state.history.redo_count()
            ));

            ui.separator();

            let has_selection = !self.state.selection.is_empty();

            if ui.add_enabled(has_selection, egui::Button::new("Delete (Del)")).clicked() {
                self.state.delete_selected_cmd();
                ui.close_menu();
            }
            if ui.add_enabled(has_selection, egui::Button::new("Duplicate (Ctrl+D)")).clicked() {
                self.state.duplicate_selected_cmd();
                ui.close_menu();
            }

            ui.separator();

            if ui.button("Select All (Ctrl+A)").clicked() {
                self.state.select_all();
                ui.close_menu();
            }
            if ui.button("Deselect All (Escape)").clicked() {
                self.state.deselect_all();
                ui.close_menu();
            }
        });
    }

    fn view_menu(&mut self, ui: &mut egui::Ui) {
        ui.menu_button("View", |ui| {
            ui.label("Panels");
            ui.checkbox(&mut self.state.preferences.show_hierarchy, "Hierarchy");
            ui.checkbox(&mut self.state.preferences.show_inspector, "Inspector");
            ui.checkbox(&mut self.state.preferences.show_asset_browser, "Asset Browser");
            ui.checkbox(&mut self.state.preferences.show_console, "Console");
            ui.separator();
            ui.label("Viewport");
            ui.checkbox(&mut self.state.preferences.show_grid, "Show Grid");
            ui.checkbox(&mut self.state.preferences.show_gizmos, "Show Gizmos");
            ui.checkbox(&mut self.state.preferences.show_stats, "Show Stats");
            ui.checkbox(&mut self.state.preferences.snap_to_grid, "Snap to Grid");
            ui.separator();
            if ui.button("Reset Camera").clicked() {
                self.state.viewport.reset_camera();
                ui.close_menu();
            }
        });
    }

    fn create_menu(&mut self, ui: &mut egui::Ui) {
        ui.menu_button("Create", |ui| {
            ui.menu_button("Primitive", |ui| {
                use void_editor::core::editor_state::MeshType;
                for mesh_type in MeshType::all() {
                    if ui.button(mesh_type.name()).clicked() {
                        self.state.create_entity(mesh_type.name().to_string(), *mesh_type);
                        ui.close_menu();
                    }
                }
            });

            ui.separator();

            // Prefab submenu
            ui.menu_button("From Prefab", |ui| {
                let prefab_names: Vec<String> = self.state.prefabs.list().map(|s| s.to_string()).collect();
                if prefab_names.is_empty() {
                    ui.label("No prefabs available");
                } else {
                    for name in prefab_names {
                        if ui.button(&name).clicked() {
                            self.state.instantiate_prefab(&name);
                            ui.close_menu();
                        }
                    }
                }
            });

            ui.separator();

            // Save as prefab
            if ui.button("Save Selection as Prefab...").clicked() {
                // For now, use the entity name as prefab name
                if let Some(id) = self.state.selection.primary() {
                    if let Some(entity) = self.state.get_entity(id) {
                        let prefab_name = entity.name.clone();
                        if let Err(e) = self.state.save_selected_as_prefab(&prefab_name) {
                            self.state.console.error(format!("Failed to save prefab: {}", e));
                        }
                    }
                } else {
                    self.state.set_status("No entity selected");
                }
                ui.close_menu();
            }
        });
    }

    fn help_menu(&mut self, ui: &mut egui::Ui) {
        ui.menu_button("Help", |ui| {
            if ui.button("Keyboard Shortcuts (F1)").clicked() {
                self.state.show_shortcuts = true;
                ui.close_menu();
            }
            ui.separator();
            if ui.button("About").clicked() {
                self.state.show_about = true;
                ui.close_menu();
            }
        });
    }

    fn hierarchy_panel(&mut self, ctx: &EguiContext) {
        egui::SidePanel::left("hierarchy")
            .default_width(200.0)
            .show(ctx, |ui| {
                ui.heading("Hierarchy");
                ui.separator();

                ui.horizontal(|ui| {
                    if ui.button("+ Add").clicked() {
                        use void_editor::core::editor_state::MeshType;
                        self.state.create_entity("New Entity".to_string(), MeshType::Cube);
                    }
                });
                ui.separator();

                egui::ScrollArea::vertical().show(ui, |ui| {
                    let mut selection_action: Option<(void_editor::core::EntityId, SelectionMode)> = None;

                    for entity in &self.state.entities {
                        let is_selected = self.state.selection.is_selected(entity.id);

                        ui.horizontal(|ui| {
                            let icon = match entity.mesh_type {
                                void_editor::core::editor_state::MeshType::Cube => "[C]",
                                void_editor::core::editor_state::MeshType::Sphere => "[S]",
                                void_editor::core::editor_state::MeshType::Cylinder => "[Y]",
                                void_editor::core::editor_state::MeshType::Diamond => "[D]",
                                void_editor::core::editor_state::MeshType::Torus => "[T]",
                                void_editor::core::editor_state::MeshType::Plane => "[P]",
                            };
                            ui.label(icon);

                            let response = ui.selectable_label(is_selected, &entity.name);
                            if response.clicked() {
                                let modifiers = ui.input(|i| i.modifiers);
                                let mode = SelectionMode::from_modifiers(modifiers.shift, modifiers.ctrl);
                                selection_action = Some((entity.id, mode));
                            }
                        });
                    }

                    if let Some((id, mode)) = selection_action {
                        self.state.selection.select(id, mode);
                    }

                    if self.state.entities.is_empty() {
                        ui.label("No entities in scene");
                        ui.label("");
                        ui.label("Use + Add or Create menu");
                    }
                });
            });
    }

    fn inspector_panel(&mut self, ctx: &EguiContext) {
        egui::SidePanel::right("inspector")
            .default_width(250.0)
            .show(ctx, |ui| {
                ui.heading("Inspector");
                ui.separator();

                if let Some(id) = self.state.selection.primary() {
                    let entity_data = self.state.get_entity(id).cloned();

                    if let Some(mut entity) = entity_data {
                        let mut modified = false;

                        // Name
                        ui.horizontal(|ui| {
                            ui.label("Name:");
                            if ui.text_edit_singleline(&mut entity.name).changed() {
                                modified = true;
                            }
                        });

                        ui.separator();

                        // Transform
                        egui::CollapsingHeader::new("Transform")
                            .default_open(true)
                            .show(ui, |ui| {
                                ui.label("Position");
                                ui.horizontal(|ui| {
                                    ui.label("X:");
                                    if ui.add(egui::DragValue::new(&mut entity.transform.position[0]).speed(0.1)).changed() {
                                        modified = true;
                                    }
                                    ui.label("Y:");
                                    if ui.add(egui::DragValue::new(&mut entity.transform.position[1]).speed(0.1)).changed() {
                                        modified = true;
                                    }
                                    ui.label("Z:");
                                    if ui.add(egui::DragValue::new(&mut entity.transform.position[2]).speed(0.1)).changed() {
                                        modified = true;
                                    }
                                });

                                ui.label("Rotation");
                                ui.horizontal(|ui| {
                                    ui.label("X:");
                                    if ui.add(egui::DragValue::new(&mut entity.transform.rotation[0]).speed(0.01)).changed() {
                                        modified = true;
                                    }
                                    ui.label("Y:");
                                    if ui.add(egui::DragValue::new(&mut entity.transform.rotation[1]).speed(0.01)).changed() {
                                        modified = true;
                                    }
                                    ui.label("Z:");
                                    if ui.add(egui::DragValue::new(&mut entity.transform.rotation[2]).speed(0.01)).changed() {
                                        modified = true;
                                    }
                                });

                                ui.label("Scale");
                                ui.horizontal(|ui| {
                                    ui.label("X:");
                                    if ui.add(egui::DragValue::new(&mut entity.transform.scale[0]).speed(0.1)).changed() {
                                        modified = true;
                                    }
                                    ui.label("Y:");
                                    if ui.add(egui::DragValue::new(&mut entity.transform.scale[1]).speed(0.1)).changed() {
                                        modified = true;
                                    }
                                    ui.label("Z:");
                                    if ui.add(egui::DragValue::new(&mut entity.transform.scale[2]).speed(0.1)).changed() {
                                        modified = true;
                                    }
                                });
                            });

                        ui.separator();

                        // Appearance
                        egui::CollapsingHeader::new("Appearance")
                            .default_open(true)
                            .show(ui, |ui| {
                                ui.horizontal(|ui| {
                                    ui.label("Mesh:");
                                    use void_editor::core::editor_state::MeshType;
                                    let current_mesh = entity.mesh_type;
                                    egui::ComboBox::from_id_source("mesh_type")
                                        .selected_text(current_mesh.name())
                                        .show_ui(ui, |ui| {
                                            for mesh_type in MeshType::all() {
                                                if ui.selectable_value(&mut entity.mesh_type, *mesh_type, mesh_type.name()).changed() {
                                                    modified = true;
                                                }
                                            }
                                        });
                                });

                                ui.horizontal(|ui| {
                                    ui.label("Color:");
                                    if ui.color_edit_button_rgb(&mut entity.color).changed() {
                                        modified = true;
                                    }
                                });
                            });

                        ui.separator();

                        // Actions
                        ui.horizontal(|ui| {
                            if ui.button("Duplicate").clicked() {
                                self.state.duplicate_selected();
                            }
                            if ui.button("Delete").clicked() {
                                self.state.delete_selected();
                            }
                        });

                        // Apply modifications
                        if modified {
                            if let Some(e) = self.state.get_entity_mut(id) {
                                e.name = entity.name;
                                e.transform = entity.transform;
                                e.mesh_type = entity.mesh_type;
                                e.color = entity.color;
                            }
                            self.state.scene_modified = true;
                        }
                    }
                } else {
                    ui.label("No entity selected");
                    ui.label("");
                    ui.label("Select an entity from");
                    ui.label("the Hierarchy panel.");
                }
            });
    }

    fn bottom_panels(&mut self, ctx: &EguiContext) {
        egui::TopBottomPanel::bottom("bottom_panel")
            .default_height(180.0)
            .resizable(true)
            .show(ctx, |ui| {
                ui.horizontal(|ui| {
                    if self.state.preferences.show_console {
                        ui.selectable_label(true, "Console");
                    }
                    if self.state.preferences.show_asset_browser {
                        ui.selectable_label(false, "Assets");
                    }
                });
                ui.separator();

                ui.horizontal(|ui| {
                    // Console
                    if self.state.preferences.show_console {
                        ui.group(|ui| {
                            ui.set_min_width(300.0);

                            ui.horizontal(|ui| {
                                ui.label("Console");
                                ui.separator();
                                if ui.small_button("Clear").clicked() {
                                    self.state.console.clear();
                                }
                                ui.checkbox(&mut self.state.console.show_info, "Info");
                                ui.checkbox(&mut self.state.console.show_warnings, "Warn");
                                ui.checkbox(&mut self.state.console.show_errors, "Error");
                            });

                            ui.separator();

                            egui::ScrollArea::vertical()
                                .auto_shrink([false, false])
                                .stick_to_bottom(self.state.console.auto_scroll)
                                .show(ui, |ui| {
                                    for entry in self.state.console.entries() {
                                        if self.state.console.should_show(entry) {
                                            ui.horizontal(|ui| {
                                                ui.colored_label(entry.level.color(), entry.level.prefix());
                                                ui.label(&entry.message);
                                            });
                                        }
                                    }
                                });
                        });
                    }

                    // Asset Browser
                    if self.state.preferences.show_asset_browser {
                        ui.group(|ui| {
                            ui.set_min_width(300.0);

                            ui.horizontal(|ui| {
                                ui.label("Assets");
                                ui.separator();
                                if ui.small_button("Up").clicked() {
                                    self.state.asset_browser.navigate_up();
                                }
                                if ui.small_button("Refresh").clicked() {
                                    self.state.asset_browser.refresh();
                                }
                            });

                            ui.horizontal(|ui| {
                                ui.label("Path:");
                                let path_str = self.state.asset_browser.current_path.to_string_lossy();
                                ui.label(path_str.as_ref());
                            });

                            ui.separator();

                            // Collect entries to avoid borrow issues
                            let entries: Vec<_> = self.state.asset_browser.filtered_entries()
                                .map(|(idx, entry)| (idx, entry.name.clone(), entry.is_directory, entry.asset_type, entry.path.clone()))
                                .collect();
                            let selected = self.state.asset_browser.selected_asset;

                            egui::ScrollArea::vertical()
                                .auto_shrink([false, false])
                                .show(ui, |ui| {
                                    let mut nav_action: Option<std::path::PathBuf> = None;
                                    let mut select_action: Option<usize> = None;
                                    let mut drag_start: Option<(std::path::PathBuf, String, void_editor::panels::AssetType)> = None;

                                    for (idx, name, is_directory, asset_type, path) in &entries {
                                        let is_selected = selected == Some(*idx);

                                        // Make non-directory assets draggable
                                        let response = if !*is_directory && (*asset_type == void_editor::panels::AssetType::Mesh || *asset_type == void_editor::panels::AssetType::Scene) {
                                            let drag_id = egui::Id::new(("asset_drag", path));
                                            let response = ui.horizontal(|ui| {
                                                ui.label(asset_type.icon());
                                                ui.selectable_label(is_selected, name)
                                            }).inner;

                                            // Handle drag
                                            if response.drag_started() {
                                                drag_start = Some((path.clone(), name.clone(), *asset_type));
                                            }

                                            response
                                        } else {
                                            ui.horizontal(|ui| {
                                                if *is_directory {
                                                    ui.label("[D]");
                                                } else {
                                                    ui.label(asset_type.icon());
                                                }
                                                ui.selectable_label(is_selected, name)
                                            }).inner
                                        };

                                        if response.clicked() {
                                            if *is_directory {
                                                nav_action = Some(path.clone());
                                            } else {
                                                select_action = Some(*idx);
                                            }
                                        }
                                    }

                                    // Apply actions after iteration
                                    if let Some(path) = nav_action {
                                        self.state.asset_browser.navigate_to(path);
                                    }
                                    if let Some(idx) = select_action {
                                        self.state.asset_browser.selected_asset = Some(idx);
                                    }
                                    if let Some((path, name, asset_type)) = drag_start {
                                        self.state.drag_asset = Some(void_editor::core::DraggedAsset {
                                            path,
                                            name,
                                            asset_type,
                                        });
                                    }
                                });
                        });
                    }
                });
            });
    }

    fn about_dialog(&mut self, ctx: &EguiContext) {
        egui::Window::new("About")
            .collapsible(false)
            .resizable(false)
            .anchor(egui::Align2::CENTER_CENTER, [0.0, 0.0])
            .show(ctx, |ui| {
                ui.heading("Void Engine Editor");
                ui.label(format!("Version {}", void_editor::VERSION));
                ui.separator();
                ui.label("A world-class 3D scene editor for the");
                ui.label("Void Engine Metaverse OS.");
                ui.separator();
                ui.label("Features:");
                ui.label("  - Scene hierarchy with multi-select");
                ui.label("  - Transform editing with gizmos");
                ui.label("  - Asset browser with hot-reload");
                ui.label("  - Console/logging");
                ui.label("  - Undo/Redo system");
                ui.label("  - void_ecs integration");
                ui.separator();
                ui.label("Controls:");
                ui.label("  Right-click drag: Orbit camera");
                ui.label("  Scroll: Zoom");
                ui.label("  Delete: Delete selected");
                ui.separator();
                if ui.button("Close").clicked() {
                    self.state.show_about = false;
                }
            });
    }

    fn shortcuts_dialog(&mut self, ctx: &EguiContext) {
        egui::Window::new("Keyboard Shortcuts")
            .collapsible(false)
            .resizable(false)
            .anchor(egui::Align2::CENTER_CENTER, [0.0, 0.0])
            .min_width(350.0)
            .show(ctx, |ui| {
                ui.heading("Keyboard Shortcuts");
                ui.separator();

                egui::Grid::new("shortcuts_grid")
                    .num_columns(2)
                    .spacing([40.0, 8.0])
                    .striped(true)
                    .show(ui, |ui| {
                        // File operations
                        ui.strong("File Operations");
                        ui.label("");
                        ui.end_row();

                        ui.label("Ctrl+N");
                        ui.label("New Scene");
                        ui.end_row();

                        ui.label("Ctrl+O");
                        ui.label("Open Scene");
                        ui.end_row();

                        ui.label("Ctrl+S");
                        ui.label("Save Scene");
                        ui.end_row();

                        ui.label("Ctrl+Shift+S");
                        ui.label("Save As...");
                        ui.end_row();

                        ui.label("");
                        ui.label("");
                        ui.end_row();

                        // Edit operations
                        ui.strong("Edit Operations");
                        ui.label("");
                        ui.end_row();

                        ui.label("Ctrl+Z");
                        ui.label("Undo");
                        ui.end_row();

                        ui.label("Ctrl+Y");
                        ui.label("Redo");
                        ui.end_row();

                        ui.label("Delete");
                        ui.label("Delete Selected");
                        ui.end_row();

                        ui.label("Ctrl+D");
                        ui.label("Duplicate Selected");
                        ui.end_row();

                        ui.label("Escape");
                        ui.label("Deselect All");
                        ui.end_row();

                        ui.label("");
                        ui.label("");
                        ui.end_row();

                        // Tool switching
                        ui.strong("Tools");
                        ui.label("");
                        ui.end_row();

                        ui.label("Q");
                        ui.label("Selection Tool");
                        ui.end_row();

                        ui.label("W");
                        ui.label("Move Tool");
                        ui.end_row();

                        ui.label("E");
                        ui.label("Rotate Tool");
                        ui.end_row();

                        ui.label("R");
                        ui.label("Scale Tool");
                        ui.end_row();

                        ui.label("C");
                        ui.label("Creation Tool");
                        ui.end_row();

                        ui.label("");
                        ui.label("");
                        ui.end_row();

                        // Viewport
                        ui.strong("Viewport");
                        ui.label("");
                        ui.end_row();

                        ui.label("Right-click drag");
                        ui.label("Orbit Camera");
                        ui.end_row();

                        ui.label("Scroll wheel");
                        ui.label("Zoom In/Out");
                        ui.end_row();

                        ui.label("");
                        ui.label("");
                        ui.end_row();

                        // Help
                        ui.strong("Help");
                        ui.label("");
                        ui.end_row();

                        ui.label("F1");
                        ui.label("Show Shortcuts");
                        ui.end_row();
                    });

                ui.separator();
                ui.horizontal(|ui| {
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if ui.button("Close").clicked() {
                            self.state.show_shortcuts = false;
                        }
                    });
                });
            });
    }

    fn stats_overlay(&self, ctx: &EguiContext) {
        // Collect stats
        let entity_count = self.state.entities.len();
        let selection_count = self.state.selection.count();
        let undo_count = self.state.history.undo_count();
        let redo_count = self.state.history.redo_count();
        let fps = self.average_fps();
        let frame_time = self.average_frame_time_ms();

        // Count entities by mesh type
        let mut mesh_counts: std::collections::HashMap<MeshType, usize> = std::collections::HashMap::new();
        for entity in &self.state.entities {
            *mesh_counts.entry(entity.mesh_type).or_insert(0) += 1;
        }

        egui::Window::new("Statistics")
            .anchor(egui::Align2::RIGHT_TOP, [-10.0, 40.0])
            .resizable(false)
            .collapsible(true)
            .default_width(200.0)
            .show(ctx, |ui| {
                // Performance section
                ui.heading("Performance");
                egui::Grid::new("stats_performance")
                    .num_columns(2)
                    .spacing([20.0, 4.0])
                    .show(ui, |ui| {
                        ui.label("FPS:");
                        ui.label(format!("{:.1}", fps));
                        ui.end_row();

                        ui.label("Frame Time:");
                        ui.label(format!("{:.2} ms", frame_time));
                        ui.end_row();
                    });

                ui.separator();

                // Scene section
                ui.heading("Scene");
                egui::Grid::new("stats_scene")
                    .num_columns(2)
                    .spacing([20.0, 4.0])
                    .show(ui, |ui| {
                        ui.label("Total Entities:");
                        ui.label(format!("{}", entity_count));
                        ui.end_row();

                        ui.label("Selected:");
                        ui.label(format!("{}", selection_count));
                        ui.end_row();
                    });

                // Entity breakdown by type
                if !mesh_counts.is_empty() {
                    ui.separator();
                    ui.heading("Entities by Type");
                    egui::Grid::new("stats_entities_by_type")
                        .num_columns(2)
                        .spacing([20.0, 4.0])
                        .show(ui, |ui| {
                            for mesh_type in MeshType::all() {
                                if let Some(count) = mesh_counts.get(mesh_type) {
                                    ui.label(format!("{}:", mesh_type.name()));
                                    ui.label(format!("{}", count));
                                    ui.end_row();
                                }
                            }
                        });
                }

                ui.separator();

                // History section
                ui.heading("History");
                egui::Grid::new("stats_history")
                    .num_columns(2)
                    .spacing([20.0, 4.0])
                    .show(ui, |ui| {
                        ui.label("Undo Stack:");
                        ui.label(format!("{}", undo_count));
                        ui.end_row();

                        ui.label("Redo Stack:");
                        ui.label(format!("{}", redo_count));
                        ui.end_row();
                    });

                ui.separator();

                // Camera section
                ui.heading("Camera");
                egui::Grid::new("stats_camera")
                    .num_columns(2)
                    .spacing([20.0, 4.0])
                    .show(ui, |ui| {
                        ui.label("Distance:");
                        ui.label(format!("{:.2}", self.state.viewport.camera_distance));
                        ui.end_row();

                        ui.label("Yaw:");
                        ui.label(format!("{:.1}°", self.state.viewport.camera_yaw.to_degrees()));
                        ui.end_row();

                        ui.label("Pitch:");
                        ui.label(format!("{:.1}°", self.state.viewport.camera_pitch.to_degrees()));
                        ui.end_row();
                    });
            });
    }

    fn handle_event(&mut self, event: &WindowEvent) -> bool {
        let response = self.egui_state.on_window_event(self.window.as_ref(), event);
        if response.consumed {
            return true;
        }

        match event {
            WindowEvent::Resized(size) => {
                self.resize(*size);
            }
            WindowEvent::MouseWheel { delta, .. } => {
                let scroll = match delta {
                    winit::event::MouseScrollDelta::LineDelta(_, y) => *y,
                    winit::event::MouseScrollDelta::PixelDelta(pos) => pos.y as f32 * 0.1,
                };
                self.state.viewport.camera_distance = (self.state.viewport.camera_distance - scroll * 0.5).clamp(1.0, 50.0);
            }
            WindowEvent::MouseInput { state, button, .. } => {
                if *button == MouseButton::Right {
                    self.state.viewport.mouse_orbiting = *state == ElementState::Pressed;
                }
                // Handle asset drop on left mouse release
                if *button == MouseButton::Left && *state == ElementState::Released {
                    if let Some(drag_asset) = self.state.drag_asset.take() {
                        // Create entity from dropped asset
                        self.handle_asset_drop(drag_asset);
                    }
                }
            }
            WindowEvent::CursorMoved { position, .. } => {
                let new_pos = [position.x as f32, position.y as f32];
                if self.state.viewport.mouse_orbiting {
                    let dx = new_pos[0] - self.state.viewport.mouse_pos[0];
                    let dy = new_pos[1] - self.state.viewport.mouse_pos[1];
                    self.state.viewport.camera_yaw += dx * 0.01;
                    self.state.viewport.camera_pitch = (self.state.viewport.camera_pitch - dy * 0.01).clamp(-1.4, 1.4);
                }
                self.state.viewport.mouse_pos = new_pos;
            }
            WindowEvent::KeyboardInput { event, .. } => {
                if event.state == ElementState::Pressed {
                    let modifiers = self.egui_state.egui_input().modifiers;
                    let ctrl = modifiers.ctrl || modifiers.mac_cmd;

                    match &event.logical_key {
                        // Delete selected entities
                        Key::Named(NamedKey::Delete) => {
                            self.state.delete_selected_cmd();
                        }
                        // Escape - deselect all
                        Key::Named(NamedKey::Escape) => {
                            self.state.deselect_all();
                        }
                        // Ctrl+Z - Undo
                        Key::Character(c) if ctrl && c.to_lowercase() == "z" => {
                            self.state.undo();
                        }
                        // Ctrl+Y - Redo
                        Key::Character(c) if ctrl && c.to_lowercase() == "y" => {
                            self.state.redo();
                        }
                        // Ctrl+D - Duplicate
                        Key::Character(c) if ctrl && c.to_lowercase() == "d" => {
                            self.state.duplicate_selected_cmd();
                        }
                        // Ctrl+A - Select All
                        Key::Character(c) if ctrl && c.to_lowercase() == "a" => {
                            self.state.select_all();
                        }
                        // Ctrl+N - New Scene
                        Key::Character(c) if ctrl && c.to_lowercase() == "n" => {
                            self.state.new_scene();
                        }
                        // Ctrl+S - Save Scene
                        Key::Character(c) if ctrl && c.to_lowercase() == "s" => {
                            match self.state.save_scene_current() {
                                Ok(true) => {} // Saved
                                Ok(false) => {
                                    // No path, show save dialog
                                    if let Some(path) = rfd::FileDialog::new()
                                        .add_filter("Scene Files", &["toml"])
                                        .set_file_name("scene.toml")
                                        .save_file()
                                    {
                                        if let Err(e) = self.state.save_scene(path) {
                                            self.state.console.error(format!("Failed to save: {}", e));
                                        }
                                    }
                                }
                                Err(e) => {
                                    self.state.console.error(format!("Failed to save: {}", e));
                                }
                            }
                        }
                        // Ctrl+O - Open Scene
                        Key::Character(c) if ctrl && c.to_lowercase() == "o" => {
                            if let Some(path) = rfd::FileDialog::new()
                                .add_filter("Scene Files", &["toml", "scene.toml"])
                                .pick_file()
                            {
                                if let Err(e) = self.state.load_scene(path) {
                                    self.state.console.error(format!("Failed to load: {}", e));
                                }
                            }
                        }
                        // Tool shortcuts (non-ctrl)
                        Key::Character(c) if !ctrl && c.to_lowercase() == "q" => {
                            self.switch_tool(SELECTION_TOOL_ID);
                        }
                        Key::Character(c) if !ctrl && c.to_lowercase() == "w" => {
                            self.switch_tool(MOVE_TOOL_ID);
                            self.state.gizmos.set_mode(GizmoMode::Translate);
                        }
                        Key::Character(c) if !ctrl && c.to_lowercase() == "e" => {
                            self.switch_tool(ROTATE_TOOL_ID);
                            self.state.gizmos.set_mode(GizmoMode::Rotate);
                        }
                        Key::Character(c) if !ctrl && c.to_lowercase() == "r" => {
                            self.switch_tool(SCALE_TOOL_ID);
                            self.state.gizmos.set_mode(GizmoMode::Scale);
                        }
                        Key::Character(c) if !ctrl && c.to_lowercase() == "c" => {
                            self.switch_tool(CREATION_TOOL_ID);
                        }
                        // Toggle snap
                        Key::Character(c) if !ctrl && c.to_lowercase() == "x" => {
                            self.state.gizmos.toggle_snap();
                            let status = if self.state.gizmos.snap.enabled { "Snap ON" } else { "Snap OFF" };
                            self.state.set_status(status);
                        }
                        // F1 - Show keyboard shortcuts
                        Key::Named(NamedKey::F1) => {
                            self.state.show_shortcuts = true;
                        }
                        _ => {}
                    }
                }
            }
            _ => {}
        }
        false
    }
}

// ============================================================================
// Math Helpers
// ============================================================================

fn identity_matrix() -> [[f32; 4]; 4] {
    [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
}

fn look_at(eye: [f32; 3], target: [f32; 3], up: [f32; 3]) -> [[f32; 4]; 4] {
    let f = normalize_vec3([
        target[0] - eye[0],
        target[1] - eye[1],
        target[2] - eye[2],
    ]);
    let s = normalize_vec3(cross(f, up));
    let u = cross(s, f);

    [
        [s[0], u[0], -f[0], 0.0],
        [s[1], u[1], -f[1], 0.0],
        [s[2], u[2], -f[2], 0.0],
        [-dot(s, eye), -dot(u, eye), dot(f, eye), 1.0],
    ]
}

fn perspective(fov: f32, aspect: f32, near: f32, far: f32) -> [[f32; 4]; 4] {
    let f = 1.0 / (fov / 2.0).tan();
    [
        [f / aspect, 0.0, 0.0, 0.0],
        [0.0, f, 0.0, 0.0],
        [0.0, 0.0, (far + near) / (near - far), -1.0],
        [0.0, 0.0, (2.0 * far * near) / (near - far), 0.0],
    ]
}

fn translation_matrix(pos: [f32; 3]) -> [[f32; 4]; 4] {
    [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [pos[0], pos[1], pos[2], 1.0],
    ]
}

fn scale_matrix(scale: [f32; 3]) -> [[f32; 4]; 4] {
    [
        [scale[0], 0.0, 0.0, 0.0],
        [0.0, scale[1], 0.0, 0.0],
        [0.0, 0.0, scale[2], 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
}

fn rotation_matrix(euler: [f32; 3]) -> [[f32; 4]; 4] {
    let (sx, cx) = euler[0].sin_cos();
    let (sy, cy) = euler[1].sin_cos();
    let (sz, cz) = euler[2].sin_cos();

    [
        [cy * cz, cy * sz, -sy, 0.0],
        [sx * sy * cz - cx * sz, sx * sy * sz + cx * cz, sx * cy, 0.0],
        [cx * sy * cz + sx * sz, cx * sy * sz - sx * cz, cx * cy, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
}

fn mat4_mul(a: [[f32; 4]; 4], b: [[f32; 4]; 4]) -> [[f32; 4]; 4] {
    let mut result = [[0.0; 4]; 4];
    for i in 0..4 {
        for j in 0..4 {
            for k in 0..4 {
                result[i][j] += a[k][j] * b[i][k];
            }
        }
    }
    result
}

fn normalize_vec3(v: [f32; 3]) -> [f32; 3] {
    let len = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    if len > 0.0001 {
        [v[0] / len, v[1] / len, v[2] / len]
    } else {
        [0.0, 0.0, 0.0]
    }
}

fn cross(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

fn dot(a: [f32; 3], b: [f32; 3]) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

// ============================================================================
// Main
// ============================================================================

/// Application handler wrapper for winit 0.30 event loop
struct AppHandler {
    app: Option<EditorApp>,
}

impl winit::application::ApplicationHandler for AppHandler {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        if self.app.is_none() {
            self.app = Some(EditorApp::new(event_loop));
        }
    }

    fn window_event(
        &mut self,
        event_loop: &ActiveEventLoop,
        window_id: winit::window::WindowId,
        event: WindowEvent,
    ) {
        let Some(app) = self.app.as_mut() else { return };
        if window_id != app.window.id() { return }

        match &event {
            WindowEvent::CloseRequested => {
                event_loop.exit();
            }
            WindowEvent::RedrawRequested => {
                app.render();
            }
            _ => {
                app.handle_event(&event);
            }
        }
    }

    fn about_to_wait(&mut self, _event_loop: &ActiveEventLoop) {
        if let Some(app) = self.app.as_ref() {
            app.window.request_redraw();
        }
    }
}

fn main() {
    env_logger::init();

    let event_loop = EventLoop::new().unwrap();
    let mut handler = AppHandler { app: None };
    event_loop.run_app(&mut handler).unwrap();
}
