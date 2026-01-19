//! Debug & Introspection Hooks
//!
//! Provides visibility into renderer operations:
//! - Frame statistics (entity counts, draw calls, GPU memory)
//! - Debug visualization flags (bounds, normals, wireframe, etc.)
//! - Stats overlay for on-screen display
//!
//! # Example
//!
//! ```ignore
//! use void_render::debug::{RenderStats, StatsCollector, DebugVisualization, DebugConfig};
//!
//! // Collect stats each frame
//! let mut collector = StatsCollector::new();
//! collector.begin_frame();
//! collector.set_entity_counts(1000, 800);
//! collector.add_triangles(500_000);
//! let stats = collector.end_frame(0.016);
//!
//! println!("FPS: {:.1}, Draw calls: {}", stats.fps(), stats.draw_call_count);
//!
//! // Configure debug visualizations
//! let mut config = DebugConfig::default();
//! config.enable(DebugVisualization::BOUNDS);
//! config.enable(DebugVisualization::WIREFRAME);
//! ```

pub mod stats;
pub mod visualization;

pub use stats::{
    RenderStats, StatsCollector, StatsAverager,
    StatsValidation, StatsIssue, StatsCollectorState,
};

pub use visualization::{
    DebugVisualization, DebugConfig, DebugUpdateQueue, DebugConfigState,
};
