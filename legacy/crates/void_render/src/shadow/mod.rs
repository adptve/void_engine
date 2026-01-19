//! Shadow Mapping System
//!
//! Backend-agnostic shadow mapping infrastructure for directional, spot,
//! and point light shadows.
//!
//! # Architecture
//!
//! The shadow system is split into:
//!
//! - **Config**: Global and per-light shadow settings
//! - **Cascade**: Cascaded shadow map calculations for directional lights
//! - **Atlas**: Shadow map allocation management
//! - **Data**: GPU-ready data structures for shader uniforms
//!
//! # Usage
//!
//! ```ignore
//! use void_render::shadow::*;
//!
//! // Configure shadows
//! let mut config = ShadowConfig::default();
//! config.cascade_count = 4;
//! config.validate();
//!
//! // Create atlas for shadow map allocation
//! let mut atlas = ShadowAtlas::new(config.default_resolution, config.max_shadow_maps);
//!
//! // Per-frame shadow setup
//! atlas.begin_frame();
//!
//! // Allocate shadow maps for lights
//! for (light_id, light) in shadow_casting_lights {
//!     if light.is_directional() {
//!         atlas.allocate_cascades(light_id, 2048, config.cascade_count);
//!     } else {
//!         atlas.allocate(light_id, 2048);
//!     }
//! }
//!
//! // Calculate cascade matrices for directional light
//! let mut cascades = ShadowCascades::new(config.cascade_count);
//! cascades.update(
//!     &camera_view,
//!     &camera_proj,
//!     light_direction,
//!     0.1,
//!     config.shadow_distance,
//!     config.cascade_lambda,
//!     2048,
//! );
//!
//! // Build GPU shadow buffer
//! let mut shadow_buffer = ShadowBuffer::new();
//! shadow_buffer.add_directional(GpuCascadeShadow::from_cascades(...));
//! shadow_buffer.update_uniforms();
//!
//! // Upload to GPU (backend-specific)
//! let bytes = shadow_buffer.directional_shadows_bytes();
//! ```
//!
//! # Hot-Reload Support
//!
//! All shadow configuration and allocation data supports serde serialization
//! for seamless hot-reloading. GPU resources (textures, buffers) need to be
//! recreated by the backend after reload.

pub mod config;
pub mod cascade;
pub mod atlas;
pub mod data;

// Re-exports
pub use config::{
    ShadowConfig,
    LightShadowSettings,
    ShadowUpdateMode,
    ShadowQuality,
};

pub use cascade::{
    ShadowCascades,
    CascadeMatrixResult,
    MAX_CASCADES,
};

pub use atlas::{
    ShadowAtlas,
    ShadowAllocation,
    AtlasStats,
    ShadowAtlasState,
    LightId,
};

pub use data::{
    GpuShadowLight,
    GpuCascadeShadow,
    GpuShadowUniforms,
    ShadowBuffer,
    ShadowCasterData,
    ShadowReceiverData,
    MAX_SHADOW_LIGHTS,
};
