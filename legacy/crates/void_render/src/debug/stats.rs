//! Render Statistics Collection
//!
//! Collects and tracks rendering statistics for profiling and debugging:
//! - Frame timing (CPU/GPU time)
//! - Entity and draw call counts
//! - Memory usage estimates
//! - Culling statistics
//!
//! # Example
//!
//! ```ignore
//! use void_render::debug::{RenderStats, StatsCollector};
//!
//! let mut collector = StatsCollector::new();
//!
//! // Each frame
//! collector.begin_frame();
//! collector.set_entity_counts(1000, 800);
//! collector.begin_pass("shadows");
//! // ... render shadows ...
//! collector.end_pass(50);
//! collector.begin_pass("main");
//! // ... render main ...
//! collector.end_pass(200);
//! collector.add_triangles(500_000);
//!
//! let stats = collector.end_frame(frame_time);
//! println!("FPS: {:.1}", stats.fps());
//! ```

use alloc::string::String;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

/// Frame rendering statistics
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct RenderStats {
    // === Timing (in seconds) ===
    /// Total frame time (seconds)
    #[serde(skip)]
    pub frame_time: f64,

    /// CPU time spent on rendering (seconds)
    #[serde(skip)]
    pub cpu_time: f64,

    /// GPU time if available (seconds)
    #[serde(skip)]
    pub gpu_time: Option<f64>,

    /// Time breakdown by pass (name, seconds)
    #[serde(skip)]
    pub pass_times: Vec<(String, f64)>,

    // === Counts ===
    /// Total entities in scene
    pub entity_count: u32,

    /// Entities visible after culling
    pub visible_count: u32,

    /// Total draw calls issued
    pub draw_call_count: u32,

    /// Draw calls per pass (name, count)
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

    /// Texture memory (bytes)
    pub texture_memory: u64,

    /// Buffer memory (vertex, index, uniform) (bytes)
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

    /// Chunks currently loading
    pub chunks_loading: u32,

    /// Total chunks registered
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
    /// Create new empty stats
    pub fn new() -> Self {
        Self::default()
    }

    /// Calculate FPS from frame time
    pub fn fps(&self) -> f32 {
        if self.frame_time > 0.0 {
            (1.0 / self.frame_time) as f32
        } else {
            0.0
        }
    }

    /// Get frame time in milliseconds
    pub fn frame_time_ms(&self) -> f32 {
        (self.frame_time * 1000.0) as f32
    }

    /// Get CPU time in milliseconds
    pub fn cpu_time_ms(&self) -> f32 {
        (self.cpu_time * 1000.0) as f32
    }

    /// Get GPU time in milliseconds (if available)
    pub fn gpu_time_ms(&self) -> Option<f32> {
        self.gpu_time.map(|t| (t * 1000.0) as f32)
    }

    /// Get GPU memory usage in megabytes
    pub fn gpu_memory_mb(&self) -> f32 {
        self.gpu_memory_used as f32 / (1024.0 * 1024.0)
    }

    /// Get texture memory in megabytes
    pub fn texture_memory_mb(&self) -> f32 {
        self.texture_memory as f32 / (1024.0 * 1024.0)
    }

    /// Get buffer memory in megabytes
    pub fn buffer_memory_mb(&self) -> f32 {
        self.buffer_memory as f32 / (1024.0 * 1024.0)
    }

    /// Get culling efficiency (percentage of entities culled)
    pub fn culling_efficiency(&self) -> f32 {
        if self.entity_count > 0 {
            let culled = self.frustum_culled + self.occlusion_culled;
            culled as f32 / self.entity_count as f32 * 100.0
        } else {
            0.0
        }
    }

    /// Validate stats for consistency
    pub fn validate(&self) -> StatsValidation {
        let mut issues = Vec::new();

        // Check for impossible values
        if self.visible_count > self.entity_count {
            issues.push(StatsIssue::VisibleExceedsTotal);
        }

        if self.frustum_culled + self.occlusion_culled > self.entity_count {
            issues.push(StatsIssue::CulledExceedsTotal);
        }

        if self.frame_time > 1.0 {
            issues.push(StatsIssue::FrameTimeExceedsOneSecond);
        }

        if self.gpu_memory_used > 64 * 1024 * 1024 * 1024 {
            issues.push(StatsIssue::UnreasonableMemory);
        }

        if issues.is_empty() {
            StatsValidation::Valid
        } else {
            StatsValidation::Invalid(issues)
        }
    }

    /// Sanitize stats to fix impossible values
    pub fn sanitize(&mut self) {
        // Clamp visible to entity count
        self.visible_count = self.visible_count.min(self.entity_count);

        // Clamp culled counts
        let max_culled = self.entity_count.saturating_sub(self.visible_count);
        let total_culled = self.frustum_culled + self.occlusion_culled;
        if total_culled > max_culled {
            let scale = max_culled as f32 / total_culled as f32;
            self.frustum_culled = (self.frustum_culled as f32 * scale) as u32;
            self.occlusion_culled = (self.occlusion_culled as f32 * scale) as u32;
        }

        // Clamp memory to reasonable bounds
        const MAX_GPU_MEMORY: u64 = 64 * 1024 * 1024 * 1024; // 64GB
        self.gpu_memory_used = self.gpu_memory_used.min(MAX_GPU_MEMORY);
        self.texture_memory = self.texture_memory.min(MAX_GPU_MEMORY);
        self.buffer_memory = self.buffer_memory.min(MAX_GPU_MEMORY);
    }

    /// Reset all stats to zero
    pub fn reset(&mut self) {
        *self = Self::default();
    }

    /// Merge stats from another source (additive)
    pub fn merge(&mut self, other: &RenderStats) {
        self.entity_count += other.entity_count;
        self.visible_count += other.visible_count;
        self.draw_call_count += other.draw_call_count;
        self.triangle_count += other.triangle_count;
        self.vertex_count += other.vertex_count;
        self.instance_count += other.instance_count;
        self.frustum_culled += other.frustum_culled;
        self.occlusion_culled += other.occlusion_culled;
        self.lod_switches += other.lod_switches;
        self.light_count += other.light_count;
        self.shadow_light_count += other.shadow_light_count;
        self.shadow_map_updates += other.shadow_map_updates;
    }
}

/// Result of stats validation
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum StatsValidation {
    /// Stats are valid
    Valid,
    /// Stats have issues
    Invalid(Vec<StatsIssue>),
}

/// Specific issues found in stats
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum StatsIssue {
    /// visible_count > entity_count
    VisibleExceedsTotal,
    /// culled counts exceed entity_count
    CulledExceedsTotal,
    /// frame_time > 1 second (likely stall)
    FrameTimeExceedsOneSecond,
    /// Memory usage seems unreasonably high
    UnreasonableMemory,
}

/// Collects render statistics during a frame
#[derive(Clone, Debug, Default)]
pub struct StatsCollector {
    /// Current frame stats being collected
    current_stats: RenderStats,
    /// Current pass being timed (name, start time)
    current_pass: Option<(String, f64)>,
    /// Frame start time (for CPU timing)
    frame_start_time: f64,
}

impl StatsCollector {
    /// Create a new stats collector
    pub fn new() -> Self {
        Self::default()
    }

    /// Begin collecting stats for a new frame
    pub fn begin_frame(&mut self) {
        self.current_stats = RenderStats::new();
        self.current_pass = None;
        self.frame_start_time = 0.0; // Will be set externally if timing is needed
    }

    /// Begin collecting stats for a new frame with timing
    pub fn begin_frame_timed(&mut self, start_time: f64) {
        self.current_stats = RenderStats::new();
        self.current_pass = None;
        self.frame_start_time = start_time;
    }

    /// Begin a render pass
    pub fn begin_pass(&mut self, name: &str) {
        // End any previous pass first
        if self.current_pass.is_some() {
            self.end_pass(0);
        }
        self.current_pass = Some((String::from(name), 0.0));
    }

    /// Begin a render pass with timing
    pub fn begin_pass_timed(&mut self, name: &str, start_time: f64) {
        if self.current_pass.is_some() {
            self.end_pass(0);
        }
        self.current_pass = Some((String::from(name), start_time));
    }

    /// End the current render pass
    pub fn end_pass(&mut self, draw_calls: u32) {
        if let Some((name, _start_time)) = self.current_pass.take() {
            self.current_stats.draw_calls_per_pass.push((name, draw_calls));
            self.current_stats.draw_call_count += draw_calls;
        }
    }

    /// End the current render pass with timing
    pub fn end_pass_timed(&mut self, draw_calls: u32, end_time: f64) {
        if let Some((name, start_time)) = self.current_pass.take() {
            let duration = end_time - start_time;
            self.current_stats.pass_times.push((name.clone(), duration));
            self.current_stats.draw_calls_per_pass.push((name, draw_calls));
            self.current_stats.draw_call_count += draw_calls;
        }
    }

    /// Add triangle count
    pub fn add_triangles(&mut self, count: u64) {
        self.current_stats.triangle_count += count;
    }

    /// Add vertex count
    pub fn add_vertices(&mut self, count: u64) {
        self.current_stats.vertex_count += count;
    }

    /// Add instance count
    pub fn add_instances(&mut self, count: u32) {
        self.current_stats.instance_count += count;
    }

    /// Set entity counts
    pub fn set_entity_counts(&mut self, total: u32, visible: u32) {
        self.current_stats.entity_count = total;
        self.current_stats.visible_count = visible;
    }

    /// Set culling statistics
    pub fn set_culling_stats(&mut self, frustum: u32, occlusion: u32) {
        self.current_stats.frustum_culled = frustum;
        self.current_stats.occlusion_culled = occlusion;
    }

    /// Set LOD switch count
    pub fn set_lod_switches(&mut self, count: u32) {
        self.current_stats.lod_switches = count;
    }

    /// Set memory usage statistics
    pub fn set_memory_usage(&mut self, textures: u64, buffers: u64) {
        self.current_stats.texture_memory = textures;
        self.current_stats.buffer_memory = buffers;
        self.current_stats.gpu_memory_used = textures + buffers;
    }

    /// Set shader count
    pub fn set_shader_count(&mut self, count: u32) {
        self.current_stats.shader_count = count;
    }

    /// Set light statistics
    pub fn set_light_stats(&mut self, lights: u32, shadow_lights: u32, shadow_updates: u32) {
        self.current_stats.light_count = lights;
        self.current_stats.shadow_light_count = shadow_lights;
        self.current_stats.shadow_map_updates = shadow_updates;
    }

    /// Set streaming statistics
    pub fn set_streaming_stats(&mut self, loaded: u32, loading: u32, total: u32) {
        self.current_stats.chunks_loaded = loaded;
        self.current_stats.chunks_loading = loading;
        self.current_stats.chunks_total = total;
    }

    /// Set GPU time (if available from GPU queries)
    pub fn set_gpu_time(&mut self, time: f64) {
        self.current_stats.gpu_time = Some(time);
    }

    /// End the frame and return collected stats
    pub fn end_frame(&mut self, frame_time: f64) -> RenderStats {
        self.current_stats.frame_time = frame_time;
        self.current_stats.cpu_time = frame_time; // Approximate
        self.current_stats.clone()
    }

    /// End the frame with explicit CPU time
    pub fn end_frame_with_cpu_time(&mut self, frame_time: f64, cpu_time: f64) -> RenderStats {
        self.current_stats.frame_time = frame_time;
        self.current_stats.cpu_time = cpu_time;
        self.current_stats.clone()
    }

    /// Get reference to current stats (for inspection during frame)
    pub fn current(&self) -> &RenderStats {
        &self.current_stats
    }

    /// Get mutable reference to current stats
    pub fn current_mut(&mut self) -> &mut RenderStats {
        &mut self.current_stats
    }
}

/// Rolling average calculator for stats smoothing
#[derive(Clone, Debug)]
pub struct StatsAverager {
    /// History of frame times
    frame_times: alloc::collections::VecDeque<f32>,
    /// Maximum history size
    max_history: usize,
    /// Cached average
    cached_average: f32,
    /// Is cache valid?
    cache_valid: bool,
}

impl Default for StatsAverager {
    fn default() -> Self {
        Self::new(60)
    }
}

impl StatsAverager {
    /// Create with specified history size
    pub fn new(max_history: usize) -> Self {
        Self {
            frame_times: alloc::collections::VecDeque::with_capacity(max_history),
            max_history,
            cached_average: 0.0,
            cache_valid: false,
        }
    }

    /// Add a frame time sample
    pub fn add_sample(&mut self, frame_time_ms: f32) {
        // Validate input
        if !frame_time_ms.is_finite() || frame_time_ms < 0.0 || frame_time_ms > 10000.0 {
            return; // Skip invalid samples
        }

        if self.frame_times.len() >= self.max_history {
            self.frame_times.pop_front();
        }
        self.frame_times.push_back(frame_time_ms);
        self.cache_valid = false;
    }

    /// Get average frame time
    pub fn average_frame_time(&mut self) -> f32 {
        if self.cache_valid {
            return self.cached_average;
        }

        if self.frame_times.is_empty() {
            self.cached_average = 0.0;
        } else {
            let sum: f32 = self.frame_times.iter().sum();
            self.cached_average = sum / self.frame_times.len() as f32;
        }
        self.cache_valid = true;
        self.cached_average
    }

    /// Get average FPS
    pub fn average_fps(&mut self) -> f32 {
        let avg = self.average_frame_time();
        if avg > 0.0 {
            1000.0 / avg
        } else {
            0.0
        }
    }

    /// Get min frame time in history
    pub fn min_frame_time(&self) -> f32 {
        self.frame_times.iter().copied().fold(f32::MAX, f32::min)
    }

    /// Get max frame time in history
    pub fn max_frame_time(&self) -> f32 {
        self.frame_times.iter().copied().fold(0.0, f32::max)
    }

    /// Get 1% low FPS (99th percentile frame time)
    pub fn one_percent_low_fps(&self) -> f32 {
        if self.frame_times.is_empty() {
            return 0.0;
        }

        let mut sorted: Vec<f32> = self.frame_times.iter().copied().collect();
        sorted.sort_by(|a, b| a.partial_cmp(b).unwrap_or(core::cmp::Ordering::Equal));

        let idx = (sorted.len() * 99 / 100).min(sorted.len() - 1);
        let percentile_time = sorted[idx];

        if percentile_time > 0.0 {
            1000.0 / percentile_time
        } else {
            0.0
        }
    }

    /// Get history for graphing
    pub fn history(&self) -> &alloc::collections::VecDeque<f32> {
        &self.frame_times
    }

    /// Clear history
    pub fn clear(&mut self) {
        self.frame_times.clear();
        self.cache_valid = false;
    }
}

/// State snapshot for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct StatsCollectorState {
    /// Last collected stats
    pub last_stats: RenderStats,
}

impl From<&StatsCollector> for StatsCollectorState {
    fn from(collector: &StatsCollector) -> Self {
        Self {
            last_stats: collector.current_stats.clone(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_render_stats_new() {
        let stats = RenderStats::new();
        assert_eq!(stats.entity_count, 0);
        assert_eq!(stats.draw_call_count, 0);
    }

    #[test]
    fn test_render_stats_fps() {
        let mut stats = RenderStats::new();
        stats.frame_time = 0.016; // ~16ms
        let fps = stats.fps();
        assert!((fps - 62.5).abs() < 1.0);
    }

    #[test]
    fn test_render_stats_frame_time_ms() {
        let mut stats = RenderStats::new();
        stats.frame_time = 0.016;
        assert!((stats.frame_time_ms() - 16.0).abs() < 0.1);
    }

    #[test]
    fn test_render_stats_memory_mb() {
        let mut stats = RenderStats::new();
        stats.gpu_memory_used = 256 * 1024 * 1024; // 256MB
        assert!((stats.gpu_memory_mb() - 256.0).abs() < 0.1);
    }

    #[test]
    fn test_render_stats_culling_efficiency() {
        let mut stats = RenderStats::new();
        stats.entity_count = 100;
        stats.frustum_culled = 30;
        stats.occlusion_culled = 20;
        assert!((stats.culling_efficiency() - 50.0).abs() < 0.1);
    }

    #[test]
    fn test_render_stats_validate_valid() {
        let mut stats = RenderStats::new();
        stats.entity_count = 100;
        stats.visible_count = 80;
        stats.frustum_culled = 15;
        stats.occlusion_culled = 5;
        assert_eq!(stats.validate(), StatsValidation::Valid);
    }

    #[test]
    fn test_render_stats_validate_invalid() {
        let mut stats = RenderStats::new();
        stats.entity_count = 100;
        stats.visible_count = 150; // Invalid: more visible than total
        assert!(matches!(stats.validate(), StatsValidation::Invalid(_)));
    }

    #[test]
    fn test_render_stats_sanitize() {
        let mut stats = RenderStats::new();
        stats.entity_count = 100;
        stats.visible_count = 150;
        stats.sanitize();
        assert_eq!(stats.visible_count, 100);
    }

    #[test]
    fn test_stats_collector_basic() {
        let mut collector = StatsCollector::new();

        collector.begin_frame();
        collector.set_entity_counts(100, 80);
        collector.add_triangles(50000);

        let stats = collector.end_frame(0.016);

        assert_eq!(stats.entity_count, 100);
        assert_eq!(stats.visible_count, 80);
        assert_eq!(stats.triangle_count, 50000);
    }

    #[test]
    fn test_stats_collector_passes() {
        let mut collector = StatsCollector::new();

        collector.begin_frame();
        collector.begin_pass("shadows");
        collector.end_pass(50);
        collector.begin_pass("main");
        collector.end_pass(200);

        let stats = collector.end_frame(0.016);

        assert_eq!(stats.draw_call_count, 250);
        assert_eq!(stats.draw_calls_per_pass.len(), 2);
    }

    #[test]
    fn test_stats_averager() {
        let mut averager = StatsAverager::new(10);

        for _ in 0..5 {
            averager.add_sample(16.0);
        }

        assert!((averager.average_frame_time() - 16.0).abs() < 0.1);
        assert!((averager.average_fps() - 62.5).abs() < 1.0);
    }

    #[test]
    fn test_stats_averager_min_max() {
        let mut averager = StatsAverager::new(10);
        averager.add_sample(10.0);
        averager.add_sample(20.0);
        averager.add_sample(15.0);

        assert!((averager.min_frame_time() - 10.0).abs() < 0.1);
        assert!((averager.max_frame_time() - 20.0).abs() < 0.1);
    }

    #[test]
    fn test_render_stats_serialization() {
        let mut stats = RenderStats::new();
        stats.entity_count = 1000;
        stats.visible_count = 800;
        stats.draw_call_count = 150;
        stats.triangle_count = 500_000;
        stats.gpu_memory_used = 256 * 1024 * 1024;

        let serialized = serde_json::to_string(&stats).unwrap();
        let restored: RenderStats = serde_json::from_str(&serialized).unwrap();

        assert_eq!(restored.entity_count, 1000);
        assert_eq!(restored.visible_count, 800);
        assert_eq!(restored.draw_call_count, 150);
        assert_eq!(restored.triangle_count, 500_000);
    }

    #[test]
    fn test_render_stats_merge() {
        let mut stats1 = RenderStats::new();
        stats1.entity_count = 100;
        stats1.draw_call_count = 50;
        stats1.triangle_count = 10000;

        let mut stats2 = RenderStats::new();
        stats2.entity_count = 200;
        stats2.draw_call_count = 100;
        stats2.triangle_count = 20000;

        stats1.merge(&stats2);

        assert_eq!(stats1.entity_count, 300);
        assert_eq!(stats1.draw_call_count, 150);
        assert_eq!(stats1.triangle_count, 30000);
    }
}
