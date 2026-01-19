//! Built-in Custom Render Passes
//!
//! Provides common render passes that can be used out of the box:
//! - Bloom: HDR bloom post-process effect
//! - Outline: Selection outline effect
//! - SSAO: Screen-space ambient occlusion (placeholder)
//! - Fog: Distance-based fog effect (placeholder)
//!
//! # Example
//!
//! ```ignore
//! use void_render::pass::builtin::*;
//! use void_render::pass::registry::PassRegistry;
//!
//! let mut registry = PassRegistry::new();
//!
//! // Add bloom effect
//! registry.register(BloomPass::new(1.0, 0.5))?;
//!
//! // Add outline for selected entities
//! let mut outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);
//! outline.set_selected(vec![entity1, entity2]);
//! registry.register(outline)?;
//! ```

mod bloom;
mod outline;
mod ssao;
mod fog;

pub use bloom::{BloomPass, BloomPassConfig, BloomPassSimple};
pub use outline::{OutlinePass, OutlinePassConfig};
pub use ssao::{SSAOPass, SSAOPassConfig};
pub use fog::{FogPass, FogPassConfig, FogType};
