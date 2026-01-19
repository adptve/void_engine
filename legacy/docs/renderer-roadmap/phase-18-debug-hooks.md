# Phase 18: Debug & Introspection Hooks

## Status: Not Started

## User Story

> As a developer, I want visibility into what the renderer is doing.

## Requirements Checklist

- [ ] Expose entity counts per frame
- [ ] Expose draw call counts
- [ ] Expose GPU memory usage estimates
- [ ] Allow debug visualization toggles: Bounds, Normals, Light volumes

## Implementation Specification

### 1. Render Statistics

```rust
// crates/void_render/src/debug/stats.rs (NEW FILE)

use std::time::{Duration, Instant};

/// Frame rendering statistics
#[derive(Clone, Debug, Default)]
pub struct RenderStats {
    // === Timing ===
    /// Total frame time
    pub frame_time: Duration,

    /// CPU time spent on rendering
    pub cpu_time: Duration,

    /// GPU time (if available)
    pub gpu_time: Option<Duration>,

    /// Time breakdown by pass
    pub pass_times: Vec<(String, Duration)>,

    // === Counts ===
    /// Total entities in scene
    pub entity_count: u32,

    /// Entities visible after culling
    pub visible_count: u32,

    /// Total draw calls issued
    pub draw_call_count: u32,

    /// Draw calls per pass
    pub draw_calls_per_pass: Vec<(String, u32)>,

    /// Triangles rendered
    pub triangle_count: u64,

    /// Vertices processed
    pub vertex_count: u64,

    /// Instances rendered
    pub instance_count: u32,

    // === Memory ===
    /// Estimated GPU memory usage (bytes)
    pub gpu_memory_used: u64,

    /// Texture memory
    pub texture_memory: u64,

    /// Buffer memory (vertex, index, uniform)
    pub buffer_memory: u64,

    /// Shader program count
    pub shader_count: u32,

    // === Culling ===
    /// Entities culled by frustum
    pub frustum_culled: u32,

    /// Entities culled by occlusion
    pub occlusion_culled: u32,

    /// LOD switches this frame
    pub lod_switches: u32,

    // === Streaming ===
    /// Chunks loaded
    pub chunks_loaded: u32,

    /// Chunks loading
    pub chunks_loading: u32,

    /// Total chunks
    pub chunks_total: u32,

    // === Lights & Shadows ===
    /// Active lights
    pub light_count: u32,

    /// Shadow casting lights
    pub shadow_light_count: u32,

    /// Shadow map updates this frame
    pub shadow_map_updates: u32,
}

impl RenderStats {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn fps(&self) -> f32 {
        if self.frame_time.as_secs_f32() > 0.0 {
            1.0 / self.frame_time.as_secs_f32()
        } else {
            0.0
        }
    }

    pub fn frame_time_ms(&self) -> f32 {
        self.frame_time.as_secs_f32() * 1000.0
    }

    pub fn cpu_time_ms(&self) -> f32 {
        self.cpu_time.as_secs_f32() * 1000.0
    }

    pub fn gpu_memory_mb(&self) -> f32 {
        self.gpu_memory_used as f32 / (1024.0 * 1024.0)
    }
}

/// Collects render statistics during frame
pub struct StatsCollector {
    frame_start: Instant,
    current_stats: RenderStats,
    pass_start: Option<(String, Instant)>,
}

impl StatsCollector {
    pub fn new() -> Self {
        Self {
            frame_start: Instant::now(),
            current_stats: RenderStats::new(),
            pass_start: None,
        }
    }

    pub fn begin_frame(&mut self) {
        self.current_stats = RenderStats::new();
        self.frame_start = Instant::now();
    }

    pub fn begin_pass(&mut self, name: &str) {
        self.pass_start = Some((name.to_string(), Instant::now()));
    }

    pub fn end_pass(&mut self, draw_calls: u32) {
        if let Some((name, start)) = self.pass_start.take() {
            let duration = start.elapsed();
            self.current_stats.pass_times.push((name.clone(), duration));
            self.current_stats.draw_calls_per_pass.push((name, draw_calls));
            self.current_stats.draw_call_count += draw_calls;
        }
    }

    pub fn add_triangles(&mut self, count: u64) {
        self.current_stats.triangle_count += count;
    }

    pub fn add_vertices(&mut self, count: u64) {
        self.current_stats.vertex_count += count;
    }

    pub fn add_instances(&mut self, count: u32) {
        self.current_stats.instance_count += count;
    }

    pub fn set_entity_counts(&mut self, total: u32, visible: u32) {
        self.current_stats.entity_count = total;
        self.current_stats.visible_count = visible;
    }

    pub fn set_culling_stats(&mut self, frustum: u32, occlusion: u32) {
        self.current_stats.frustum_culled = frustum;
        self.current_stats.occlusion_culled = occlusion;
    }

    pub fn set_memory_usage(&mut self, textures: u64, buffers: u64) {
        self.current_stats.texture_memory = textures;
        self.current_stats.buffer_memory = buffers;
        self.current_stats.gpu_memory_used = textures + buffers;
    }

    pub fn end_frame(&mut self) -> RenderStats {
        self.current_stats.frame_time = self.frame_start.elapsed();
        self.current_stats.cpu_time = self.current_stats.frame_time;  // Approximate
        self.current_stats.clone()
    }
}
```

### 2. Debug Visualization

```rust
// crates/void_render/src/debug/visualization.rs (NEW FILE)

use bitflags::bitflags;

bitflags! {
    /// Debug visualization flags
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct DebugVisualization: u32 {
        /// Show bounding boxes
        const BOUNDS = 1 << 0;

        /// Show bounding spheres
        const SPHERES = 1 << 1;

        /// Show vertex normals
        const NORMALS = 1 << 2;

        /// Show tangent vectors
        const TANGENTS = 1 << 3;

        /// Show wireframe
        const WIREFRAME = 1 << 4;

        /// Show light volumes/ranges
        const LIGHT_VOLUMES = 1 << 5;

        /// Show light directions
        const LIGHT_DIRECTIONS = 1 << 6;

        /// Show shadow frustums
        const SHADOW_FRUSTUMS = 1 << 7;

        /// Show camera frustum
        const CAMERA_FRUSTUM = 1 << 8;

        /// Show collision shapes
        const COLLIDERS = 1 << 9;

        /// Show skeleton bones
        const BONES = 1 << 10;

        /// Show UV layout
        const UV_CHECKER = 1 << 11;

        /// Show mipmap levels
        const MIPMAPS = 1 << 12;

        /// Show overdraw
        const OVERDRAW = 1 << 13;

        /// Show LOD levels
        const LOD_COLORS = 1 << 14;

        /// Show chunk boundaries
        const CHUNK_BOUNDS = 1 << 15;
    }
}

/// Debug renderer configuration
#[derive(Clone, Debug)]
pub struct DebugConfig {
    /// Active visualizations
    pub flags: DebugVisualization,

    /// Normal display length
    pub normal_length: f32,

    /// Bounds line width
    pub line_width: f32,

    /// Colors
    pub bounds_color: [f32; 4],
    pub normals_color: [f32; 4],
    pub tangents_color: [f32; 4],
    pub wireframe_color: [f32; 4],
    pub light_color: [f32; 4],
}

impl Default for DebugConfig {
    fn default() -> Self {
        Self {
            flags: DebugVisualization::empty(),
            normal_length: 0.1,
            line_width: 1.0,
            bounds_color: [0.0, 1.0, 0.0, 1.0],
            normals_color: [0.0, 0.0, 1.0, 1.0],
            tangents_color: [1.0, 0.0, 0.0, 1.0],
            wireframe_color: [1.0, 1.0, 1.0, 0.5],
            light_color: [1.0, 1.0, 0.0, 0.5],
        }
    }
}
```

### 3. Debug Renderer

```rust
// crates/void_render/src/debug/renderer.rs (NEW FILE)

use wgpu::{Device, Queue, RenderPass};
use void_math::{Vec3, AABB, Sphere};

/// Renders debug visualizations
pub struct DebugRenderer {
    config: DebugConfig,

    // Line rendering
    line_pipeline: wgpu::RenderPipeline,
    line_buffer: wgpu::Buffer,
    line_count: u32,

    // Sphere rendering
    sphere_mesh: GpuMesh,

    // Statistics overlay
    stats_renderer: Option<StatsOverlay>,
}

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct DebugVertex {
    position: [f32; 3],
    color: [f32; 4],
}

impl DebugRenderer {
    pub fn new(device: &Device, config: DebugConfig) -> Self {
        // Create line pipeline
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("Debug Shader"),
            source: wgpu::ShaderSource::Wgsl(include_str!("shaders/debug.wgsl").into()),
        });

        let line_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("Debug Line Pipeline"),
            // ... pipeline config
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::LineList,
                ..Default::default()
            },
            ..Default::default()
        });

        let line_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Debug Lines"),
            size: 1024 * 1024,  // 1MB
            usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        Self {
            config,
            line_pipeline,
            line_buffer,
            line_count: 0,
            sphere_mesh: generate_debug_sphere(device),
            stats_renderer: None,
        }
    }

    /// Clear debug geometry
    pub fn begin_frame(&mut self) {
        self.line_count = 0;
    }

    /// Add AABB visualization
    pub fn draw_aabb(&mut self, aabb: &AABB, color: [f32; 4]) {
        let min = aabb.min;
        let max = aabb.max;

        // 12 edges of box
        let edges = [
            // Bottom
            ([min.x, min.y, min.z], [max.x, min.y, min.z]),
            ([max.x, min.y, min.z], [max.x, min.y, max.z]),
            ([max.x, min.y, max.z], [min.x, min.y, max.z]),
            ([min.x, min.y, max.z], [min.x, min.y, min.z]),
            // Top
            ([min.x, max.y, min.z], [max.x, max.y, min.z]),
            ([max.x, max.y, min.z], [max.x, max.y, max.z]),
            ([max.x, max.y, max.z], [min.x, max.y, max.z]),
            ([min.x, max.y, max.z], [min.x, max.y, min.z]),
            // Verticals
            ([min.x, min.y, min.z], [min.x, max.y, min.z]),
            ([max.x, min.y, min.z], [max.x, max.y, min.z]),
            ([max.x, min.y, max.z], [max.x, max.y, max.z]),
            ([min.x, min.y, max.z], [min.x, max.y, max.z]),
        ];

        for (start, end) in edges {
            self.add_line(start, end, color);
        }
    }

    /// Add sphere visualization
    pub fn draw_sphere(&mut self, center: Vec3, radius: f32, color: [f32; 4]) {
        // Draw 3 circles (XY, XZ, YZ planes)
        let segments = 32;
        for i in 0..segments {
            let a1 = (i as f32 / segments as f32) * std::f32::consts::TAU;
            let a2 = ((i + 1) as f32 / segments as f32) * std::f32::consts::TAU;

            // XY circle
            self.add_line(
                [center.x + radius * a1.cos(), center.y + radius * a1.sin(), center.z],
                [center.x + radius * a2.cos(), center.y + radius * a2.sin(), center.z],
                color,
            );

            // XZ circle
            self.add_line(
                [center.x + radius * a1.cos(), center.y, center.z + radius * a1.sin()],
                [center.x + radius * a2.cos(), center.y, center.z + radius * a2.sin()],
                color,
            );

            // YZ circle
            self.add_line(
                [center.x, center.y + radius * a1.cos(), center.z + radius * a1.sin()],
                [center.x, center.y + radius * a2.cos(), center.z + radius * a2.sin()],
                color,
            );
        }
    }

    /// Add normal visualization
    pub fn draw_normal(&mut self, position: Vec3, normal: Vec3, color: [f32; 4]) {
        let end = position + normal * self.config.normal_length;
        self.add_line(position.into(), end.into(), color);
    }

    /// Add frustum visualization
    pub fn draw_frustum(&mut self, corners: &[[f32; 3]; 8], color: [f32; 4]) {
        // Near plane
        self.add_line(corners[0], corners[1], color);
        self.add_line(corners[1], corners[3], color);
        self.add_line(corners[3], corners[2], color);
        self.add_line(corners[2], corners[0], color);

        // Far plane
        self.add_line(corners[4], corners[5], color);
        self.add_line(corners[5], corners[7], color);
        self.add_line(corners[7], corners[6], color);
        self.add_line(corners[6], corners[4], color);

        // Connecting edges
        self.add_line(corners[0], corners[4], color);
        self.add_line(corners[1], corners[5], color);
        self.add_line(corners[2], corners[6], color);
        self.add_line(corners[3], corners[7], color);
    }

    fn add_line(&mut self, start: [f32; 3], end: [f32; 3], color: [f32; 4]) {
        // Add to line buffer
        // (actual implementation would batch these)
        self.line_count += 2;
    }

    /// Upload debug geometry and render
    pub fn render(&self, pass: &mut RenderPass, queue: &Queue) {
        if self.line_count == 0 {
            return;
        }

        pass.set_pipeline(&self.line_pipeline);
        pass.set_vertex_buffer(0, self.line_buffer.slice(..));
        pass.draw(0..self.line_count, 0..1);
    }
}
```

### 4. Statistics Overlay

```rust
// crates/void_render/src/debug/overlay.rs (NEW FILE)

/// On-screen statistics display
pub struct StatsOverlay {
    /// History for graphs
    frame_times: VecDeque<f32>,
    max_history: usize,

    /// Font renderer
    font: BitmapFont,
}

impl StatsOverlay {
    pub fn new(font: BitmapFont) -> Self {
        Self {
            frame_times: VecDeque::with_capacity(120),
            max_history: 120,
            font,
        }
    }

    pub fn update(&mut self, stats: &RenderStats) {
        self.frame_times.push_back(stats.frame_time_ms());
        if self.frame_times.len() > self.max_history {
            self.frame_times.pop_front();
        }
    }

    pub fn render(&self, ui: &mut egui::Ui, stats: &RenderStats) {
        egui::Window::new("Render Stats")
            .default_pos([10.0, 10.0])
            .show(ui.ctx(), |ui| {
                // FPS
                ui.heading(format!("FPS: {:.1}", stats.fps()));

                ui.separator();

                // Timing
                ui.label(format!("Frame: {:.2} ms", stats.frame_time_ms()));
                ui.label(format!("CPU: {:.2} ms", stats.cpu_time_ms()));
                if let Some(gpu) = stats.gpu_time {
                    ui.label(format!("GPU: {:.2} ms", gpu.as_secs_f32() * 1000.0));
                }

                ui.separator();

                // Counts
                ui.label(format!("Entities: {} / {} visible",
                    stats.visible_count, stats.entity_count));
                ui.label(format!("Draw calls: {}", stats.draw_call_count));
                ui.label(format!("Triangles: {}", stats.triangle_count));
                ui.label(format!("Instances: {}", stats.instance_count));

                ui.separator();

                // Memory
                ui.label(format!("GPU Memory: {:.1} MB", stats.gpu_memory_mb()));
                ui.label(format!("  Textures: {:.1} MB",
                    stats.texture_memory as f32 / 1024.0 / 1024.0));
                ui.label(format!("  Buffers: {:.1} MB",
                    stats.buffer_memory as f32 / 1024.0 / 1024.0));

                ui.separator();

                // Culling
                ui.label(format!("Frustum culled: {}", stats.frustum_culled));
                ui.label(format!("Occlusion culled: {}", stats.occlusion_culled));

                // Frame time graph
                ui.separator();
                let points: Vec<_> = self.frame_times.iter()
                    .enumerate()
                    .map(|(i, &t)| [i as f32, t])
                    .collect();

                egui::plot::Plot::new("frame_times")
                    .height(60.0)
                    .show(ui, |plot_ui| {
                        plot_ui.line(egui::plot::Line::new(points));
                    });
            });
    }
}
```

### 5. Integration

```rust
// crates/void_render/src/lib.rs (additions)

pub mod debug;

pub use debug::{
    stats::RenderStats,
    stats::StatsCollector,
    visualization::DebugVisualization,
    visualization::DebugConfig,
    renderer::DebugRenderer,
    overlay::StatsOverlay,
};
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_render/src/debug/mod.rs` | CREATE | Debug module |
| `void_render/src/debug/stats.rs` | CREATE | Statistics collection |
| `void_render/src/debug/visualization.rs` | CREATE | Visualization flags |
| `void_render/src/debug/renderer.rs` | CREATE | Debug renderer |
| `void_render/src/debug/overlay.rs` | CREATE | Stats overlay |
| `void_render/src/debug/shaders/debug.wgsl` | CREATE | Debug shaders |
| `void_editor/src/panels/stats.rs` | CREATE | Stats panel |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_stats_collection() {
    let mut collector = StatsCollector::new();

    collector.begin_frame();
    collector.set_entity_counts(100, 80);
    collector.add_triangles(50000);

    let stats = collector.end_frame();

    assert_eq!(stats.entity_count, 100);
    assert_eq!(stats.visible_count, 80);
    assert_eq!(stats.triangle_count, 50000);
}

#[test]
fn test_fps_calculation() {
    let mut stats = RenderStats::default();
    stats.frame_time = Duration::from_millis(16);

    assert!((stats.fps() - 62.5).abs() < 1.0);
}
```

## Hot-Swap Support

### Serialization

All debug components derive `Serialize` and `Deserialize` for state preservation:

```rust
use serde::{Serialize, Deserialize};
use bitflags::bitflags;

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct RenderStats {
    // Timing - not serialized (recomputed each frame)
    #[serde(skip)]
    pub frame_time: Duration,
    #[serde(skip)]
    pub cpu_time: Duration,
    #[serde(skip)]
    pub gpu_time: Option<Duration>,
    #[serde(skip)]
    pub pass_times: Vec<(String, Duration)>,

    // Counts - serialized for comparison across hot-reloads
    pub entity_count: u32,
    pub visible_count: u32,
    pub draw_call_count: u32,
    pub draw_calls_per_pass: Vec<(String, u32)>,
    pub triangle_count: u64,
    pub vertex_count: u64,
    pub instance_count: u32,

    // Memory - serialized for tracking
    pub gpu_memory_used: u64,
    pub texture_memory: u64,
    pub buffer_memory: u64,
    pub shader_count: u32,

    // Culling stats
    pub frustum_culled: u32,
    pub occlusion_culled: u32,
    pub lod_switches: u32,

    // Streaming stats
    pub chunks_loaded: u32,
    pub chunks_loading: u32,
    pub chunks_total: u32,

    // Lights
    pub light_count: u32,
    pub shadow_light_count: u32,
    pub shadow_map_updates: u32,
}

// Use serde with bitflags
bitflags! {
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
    #[serde(transparent)]
    pub struct DebugVisualization: u32 {
        const BOUNDS = 1 << 0;
        const SPHERES = 1 << 1;
        const NORMALS = 1 << 2;
        const TANGENTS = 1 << 3;
        const WIREFRAME = 1 << 4;
        const LIGHT_VOLUMES = 1 << 5;
        const LIGHT_DIRECTIONS = 1 << 6;
        const SHADOW_FRUSTUMS = 1 << 7;
        const CAMERA_FRUSTUM = 1 << 8;
        const COLLIDERS = 1 << 9;
        const BONES = 1 << 10;
        const UV_CHECKER = 1 << 11;
        const MIPMAPS = 1 << 12;
        const OVERDRAW = 1 << 13;
        const LOD_COLORS = 1 << 14;
        const CHUNK_BOUNDS = 1 << 15;
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DebugConfig {
    pub flags: DebugVisualization,
    pub normal_length: f32,
    pub line_width: f32,
    pub bounds_color: [f32; 4],
    pub normals_color: [f32; 4],
    pub tangents_color: [f32; 4],
    pub wireframe_color: [f32; 4],
    pub light_color: [f32; 4],
}

#[derive(Serialize, Deserialize)]
pub struct StatsOverlayState {
    pub frame_time_history: VecDeque<f32>,
    pub max_history: usize,
    pub visible: bool,
    pub position: [f32; 2],
    pub expanded_sections: Vec<String>,
}
```

### HotReloadable Implementation

```rust
use void_core::hot_reload::{HotReloadable, HotReloadError};

impl HotReloadable for RenderStats {
    fn type_name(&self) -> &'static str {
        "RenderStats"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8]) -> Result<(), HotReloadError> {
        let loaded: RenderStats = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialization(e.to_string()))?;

        // Preserve counts for comparison
        self.entity_count = loaded.entity_count;
        self.visible_count = loaded.visible_count;
        self.draw_call_count = loaded.draw_call_count;
        self.triangle_count = loaded.triangle_count;
        self.gpu_memory_used = loaded.gpu_memory_used;

        // Timing fields are recomputed each frame (skipped in serde)

        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}

impl HotReloadable for DebugConfig {
    fn type_name(&self) -> &'static str {
        "DebugConfig"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8]) -> Result<(), HotReloadError> {
        *self = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialization(e.to_string()))?;
        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}

impl HotReloadable for StatsOverlay {
    fn type_name(&self) -> &'static str {
        "StatsOverlay"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        let state = StatsOverlayState {
            frame_time_history: self.frame_times.clone(),
            max_history: self.max_history,
            visible: self.visible,
            position: self.position,
            expanded_sections: self.expanded_sections.clone(),
        };
        bincode::serialize(&state).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8]) -> Result<(), HotReloadError> {
        let state: StatsOverlayState = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialization(e.to_string()))?;

        self.frame_times = state.frame_time_history;
        self.max_history = state.max_history;
        self.visible = state.visible;
        self.position = state.position;
        self.expanded_sections = state.expanded_sections;

        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}

impl HotReloadable for DebugRenderer {
    fn type_name(&self) -> &'static str {
        "DebugRenderer"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        // Only serialize configuration, not GPU resources
        self.config.serialize_state()
    }

    fn deserialize_state(&mut self, data: &[u8]) -> Result<(), HotReloadError> {
        self.config.deserialize_state(data)?;

        // GPU resources (pipelines, buffers) are recreated on demand
        self.needs_pipeline_rebuild = true;

        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}
```

### Frame-Boundary Updates

```rust
pub struct DebugUpdateQueue {
    /// Pending config changes
    pending_config: Option<DebugConfig>,
    /// Pending visualization flag toggles
    pending_flag_changes: Vec<(DebugVisualization, bool)>,
}

impl DebugUpdateQueue {
    pub fn queue_config_change(&mut self, config: DebugConfig) {
        self.pending_config = Some(config);
    }

    pub fn queue_flag_toggle(&mut self, flag: DebugVisualization, enabled: bool) {
        self.pending_flag_changes.push((flag, enabled));
    }

    /// Apply at frame boundary for safe state transition
    pub fn apply_at_frame_boundary(&mut self, renderer: &mut DebugRenderer) {
        // Apply config changes
        if let Some(config) = self.pending_config.take() {
            renderer.config = config;
            renderer.needs_pipeline_rebuild = true;
        }

        // Apply flag toggles
        for (flag, enabled) in self.pending_flag_changes.drain(..) {
            if enabled {
                renderer.config.flags.insert(flag);
            } else {
                renderer.config.flags.remove(flag);
            }
        }
    }
}

/// Synchronize debug state after hot-reload
pub fn post_reload_debug_sync(
    renderer: &mut DebugRenderer,
    overlay: &mut StatsOverlay,
) {
    // Clear any stale GPU state
    renderer.line_count = 0;
    renderer.needs_pipeline_rebuild = true;

    // Trim history if max_history changed
    while overlay.frame_times.len() > overlay.max_history {
        overlay.frame_times.pop_front();
    }

    log::info!("Debug state synchronized after hot-reload");
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_render_stats_serialization_roundtrip() {
        let mut stats = RenderStats::default();
        stats.entity_count = 1000;
        stats.visible_count = 800;
        stats.draw_call_count = 150;
        stats.triangle_count = 500_000;
        stats.gpu_memory_used = 256 * 1024 * 1024;

        let serialized = stats.serialize_state().unwrap();

        let mut restored = RenderStats::default();
        restored.deserialize_state(&serialized).unwrap();

        assert_eq!(restored.entity_count, 1000);
        assert_eq!(restored.visible_count, 800);
        assert_eq!(restored.draw_call_count, 150);
        assert_eq!(restored.triangle_count, 500_000);
        assert_eq!(restored.gpu_memory_used, 256 * 1024 * 1024);
    }

    #[test]
    fn test_debug_visualization_flags_serialization() {
        let flags = DebugVisualization::BOUNDS
            | DebugVisualization::NORMALS
            | DebugVisualization::WIREFRAME;

        let serialized = bincode::serialize(&flags).unwrap();
        let restored: DebugVisualization = bincode::deserialize(&serialized).unwrap();

        assert!(restored.contains(DebugVisualization::BOUNDS));
        assert!(restored.contains(DebugVisualization::NORMALS));
        assert!(restored.contains(DebugVisualization::WIREFRAME));
        assert!(!restored.contains(DebugVisualization::LIGHT_VOLUMES));
    }

    #[test]
    fn test_debug_config_serialization_roundtrip() {
        let config = DebugConfig {
            flags: DebugVisualization::BOUNDS | DebugVisualization::COLLIDERS,
            normal_length: 0.5,
            line_width: 2.0,
            bounds_color: [1.0, 0.0, 0.0, 1.0],
            normals_color: [0.0, 1.0, 0.0, 1.0],
            tangents_color: [0.0, 0.0, 1.0, 1.0],
            wireframe_color: [1.0, 1.0, 1.0, 0.5],
            light_color: [1.0, 1.0, 0.0, 0.8],
        };

        let serialized = config.serialize_state().unwrap();

        let mut restored = DebugConfig::default();
        restored.deserialize_state(&serialized).unwrap();

        assert_eq!(restored.flags, config.flags);
        assert_eq!(restored.normal_length, 0.5);
        assert_eq!(restored.line_width, 2.0);
        assert_eq!(restored.bounds_color, [1.0, 0.0, 0.0, 1.0]);
    }

    #[test]
    fn test_stats_overlay_state_preserved() {
        let mut overlay = StatsOverlay::new(BitmapFont::default());
        overlay.frame_times.push_back(16.0);
        overlay.frame_times.push_back(17.0);
        overlay.frame_times.push_back(15.5);
        overlay.visible = true;
        overlay.position = [100.0, 50.0];

        let serialized = overlay.serialize_state().unwrap();

        let mut restored = StatsOverlay::new(BitmapFont::default());
        restored.deserialize_state(&serialized).unwrap();

        assert_eq!(restored.frame_times.len(), 3);
        assert!(restored.visible);
        assert_eq!(restored.position, [100.0, 50.0]);
    }

    #[test]
    fn test_debug_renderer_config_only_serialized() {
        // GPU resources should not be serialized
        let config = DebugConfig {
            flags: DebugVisualization::WIREFRAME,
            ..Default::default()
        };

        // Simulate renderer with GPU state
        let mut renderer = MockDebugRenderer {
            config: config.clone(),
            line_count: 500,  // Should not be preserved
            needs_pipeline_rebuild: false,
        };

        let serialized = renderer.serialize_state().unwrap();

        let mut restored = MockDebugRenderer::default();
        restored.deserialize_state(&serialized).unwrap();

        // Config preserved
        assert!(restored.config.flags.contains(DebugVisualization::WIREFRAME));
        // GPU state reset
        assert!(restored.needs_pipeline_rebuild);
    }

    #[test]
    fn test_frame_boundary_flag_toggle() {
        let mut queue = DebugUpdateQueue::default();
        let mut renderer = MockDebugRenderer::default();

        // Queue flag toggles
        queue.queue_flag_toggle(DebugVisualization::BOUNDS, true);
        queue.queue_flag_toggle(DebugVisualization::NORMALS, true);

        // Apply at boundary
        queue.apply_at_frame_boundary(&mut renderer);

        assert!(renderer.config.flags.contains(DebugVisualization::BOUNDS));
        assert!(renderer.config.flags.contains(DebugVisualization::NORMALS));
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
impl StatsCollector {
    pub fn collect_with_recovery(&mut self) -> RenderStats {
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.end_frame()
        }));

        match result {
            Ok(stats) => stats,
            Err(panic) => {
                log::error!("Stats collection panicked: {:?}", panic);
                self.recover_from_panic()
            }
        }
    }

    fn recover_from_panic(&mut self) -> RenderStats {
        // Return safe default stats
        let stats = RenderStats {
            frame_time: Duration::from_millis(16),
            ..Default::default()
        };

        // Reset collector state
        self.current_stats = RenderStats::default();
        self.pass_start = None;

        log::warn!("Stats collector recovered with default values");
        stats
    }
}

impl DebugRenderer {
    pub fn render_with_recovery(&self, pass: &mut RenderPass, queue: &Queue) {
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.render(pass, queue)
        }));

        if let Err(panic) = result {
            log::error!("Debug render panicked: {:?}", panic);
            // Debug rendering is non-critical - scene renders without it
        }
    }
}
```

### Fallback Rendering

```rust
impl DebugRenderer {
    pub fn render_with_fallback(&mut self, pass: &mut RenderPass, queue: &Queue) {
        // Check if pipeline is valid
        if self.needs_pipeline_rebuild {
            match self.rebuild_pipeline() {
                Ok(()) => self.needs_pipeline_rebuild = false,
                Err(e) => {
                    log::warn!("Failed to rebuild debug pipeline: {:?}", e);
                    // Skip debug rendering this frame
                    return;
                }
            }
        }

        // Check if we have valid geometry
        if self.line_count == 0 {
            return;
        }

        // Validate buffer before rendering
        if !self.validate_line_buffer() {
            log::warn!("Invalid debug line buffer, skipping render");
            self.line_count = 0;
            return;
        }

        self.render(pass, queue);
    }

    fn validate_line_buffer(&self) -> bool {
        // Check buffer hasn't been destroyed
        self.line_buffer.size() > 0
    }

    fn rebuild_pipeline(&mut self) -> Result<(), PipelineError> {
        // Attempt to recreate pipeline
        // Returns error if GPU resources unavailable
        Ok(())
    }
}
```

### Stats Validation

```rust
impl RenderStats {
    pub fn validate(&self) -> StatsValidation {
        let mut issues = Vec::new();

        // Check for impossible values
        if self.visible_count > self.entity_count {
            issues.push("visible_count > entity_count");
        }

        if self.frustum_culled + self.occlusion_culled + self.visible_count as u32
            > self.entity_count {
            issues.push("culled + visible > total");
        }

        if self.frame_time.as_secs() > 1 {
            issues.push("frame_time > 1 second (likely stall)");
        }

        if issues.is_empty() {
            StatsValidation::Valid
        } else {
            StatsValidation::Invalid(issues)
        }
    }

    pub fn sanitize(&mut self) {
        // Clamp impossible values
        self.visible_count = self.visible_count.min(self.entity_count);

        // Ensure non-negative
        // (all fields are unsigned, so this is implicit)

        // Clamp memory to reasonable bounds
        const MAX_GPU_MEMORY: u64 = 64 * 1024 * 1024 * 1024; // 64GB
        self.gpu_memory_used = self.gpu_memory_used.min(MAX_GPU_MEMORY);
    }
}

pub enum StatsValidation {
    Valid,
    Invalid(Vec<&'static str>),
}
```

### Overlay Resilience

```rust
impl StatsOverlay {
    pub fn render_safe(&self, ui: &mut egui::Ui, stats: &RenderStats) {
        // Validate stats before display
        let stats = match stats.validate() {
            StatsValidation::Valid => stats.clone(),
            StatsValidation::Invalid(issues) => {
                log::warn!("Invalid stats detected: {:?}", issues);
                let mut sanitized = stats.clone();
                sanitized.sanitize();
                sanitized
            }
        };

        // Wrap UI rendering in catch_unwind
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.render(ui, &stats)
        }));

        if let Err(_) = result {
            // Show minimal fallback UI
            ui.label("Stats overlay error - check logs");
        }
    }

    pub fn update_safe(&mut self, stats: &RenderStats) {
        // Validate frame time before adding to history
        let frame_time = stats.frame_time_ms();

        if frame_time.is_finite() && frame_time > 0.0 && frame_time < 10000.0 {
            self.frame_times.push_back(frame_time);
        } else {
            // Use placeholder for invalid frame time
            self.frame_times.push_back(16.67);
        }

        // Maintain history limit
        while self.frame_times.len() > self.max_history {
            self.frame_times.pop_front();
        }
    }
}
```

## Acceptance Criteria

### Functional

- [ ] Frame statistics collected accurately
- [ ] Draw call count accurate
- [ ] GPU memory estimated
- [ ] Bounds visualization works
- [ ] Normal visualization works
- [ ] Light volume visualization works
- [ ] Wireframe mode works
- [ ] LOD coloring works
- [ ] Statistics overlay renders
- [ ] Performance: <0.1ms overhead

### Hot-Swap Compliance

- [ ] RenderStats derives Serialize/Deserialize (with timing fields skipped)
- [ ] DebugVisualization flags derive Serialize/Deserialize
- [ ] DebugConfig derives Serialize/Deserialize
- [ ] StatsOverlayState derives Serialize/Deserialize
- [ ] RenderStats implements HotReloadable trait
- [ ] DebugConfig implements HotReloadable trait
- [ ] StatsOverlay implements HotReloadable trait
- [ ] DebugRenderer implements HotReloadable (config only, not GPU resources)
- [ ] Frame time history preserved across hot-reload
- [ ] Visualization flags preserved across hot-reload
- [ ] Debug colors and settings preserved across hot-reload
- [ ] GPU pipeline marked for rebuild after hot-reload
- [ ] Frame-boundary update queue for safe flag toggling
- [ ] Hot-swap tests pass in CI

## Dependencies

- All previous phases (debug visualizes everything)

## Dependents

- None (terminal feature)

---

**Estimated Complexity**: Low
**Primary Crates**: void_render
**Reviewer Notes**: Ensure debug rendering doesn't affect release builds
