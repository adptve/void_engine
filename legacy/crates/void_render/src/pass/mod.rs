//! Multi-Pass Rendering System
//!
//! Provides infrastructure for rendering entities across multiple passes:
//! - Depth prepass for early-Z optimization
//! - Shadow mapping passes
//! - Main color pass
//! - Transparent pass with proper sorting
//! - Custom passes for special effects
//!
//! # Phase 12: Multi-Pass Entities
//!
//! ```ignore
//! use void_render::pass::{RenderPassSystem, RenderPassFlags, PassConfig};
//!
//! let mut system = RenderPassSystem::new();
//!
//! // Register a custom outline pass
//! system.register_pass(PassConfig {
//!     name: "outline".into(),
//!     flags: RenderPassFlags::CUSTOM_0,
//!     sort: PassSortMode::None,
//!     ..Default::default()
//! });
//!
//! // Each frame
//! system.begin_frame();
//!
//! // Add draw calls for entities
//! for entity in entities {
//!     let render_passes = world.get::<RenderPasses>(entity);
//!     system.add_draw_call(draw_call, render_passes.flags, &render_passes.custom_passes);
//! }
//!
//! // Sort and render
//! system.sort_passes(camera_pos);
//! for (name, config, draw_calls) in system.passes_in_order() {
//!     // Execute render pass
//! }
//! ```
//!
//! # Phase 13: Custom Render Passes
//!
//! ```ignore
//! use void_render::pass::{PassRegistry, CustomRenderPass};
//! use void_render::pass::builtin::BloomPass;
//!
//! let mut registry = PassRegistry::new();
//!
//! // Register custom passes
//! registry.register(BloomPass::new(1.0, 0.5))?;
//!
//! // Create pass groups
//! registry.create_group("post_effects", vec!["bloom".into()]);
//!
//! // Execute all passes
//! registry.execute(&mut context)?;
//! ```

// Phase 12: Multi-Pass Entities
pub mod flags;
pub mod system;

// Phase 13: Custom Render Passes
pub mod custom;
pub mod registry;
pub mod builtin;

// Phase 12 exports
pub use flags::{PassId, RenderPassFlags};
pub use system::{
    BlendMode, CullMode, PassConfig, PassDrawCall, PassQuality, PassSortMode,
    RenderPassSystem, RenderPassSystemState,
};

// Phase 13 exports
pub use custom::{
    CustomRenderPass, ResourceRef, GBufferChannel, ResourceRequirements,
    TextureFormatHint, PassFeatures, PassPriority, CameraRenderData,
    TextureViewHandle, BufferHandle, PipelineHandle, PassResources,
    PassSetupContext, PassExecuteContext, PassError, PassConfigData, CustomPassState,
};
pub use registry::{
    PassRegistry, ResourceBudget, PassRegistryUpdate, PassUpdateQueue,
    PassRegistryStats, PassRegistryState,
};
pub use builtin::{
    BloomPass, BloomPassConfig, BloomPassSimple,
    OutlinePass, OutlinePassConfig,
    SSAOPass, SSAOPassConfig,
    FogPass, FogPassConfig, FogType,
};

use alloc::string::String;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use void_ecs::Entity;

/// Pending render pass updates to be applied at frame boundary
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum RenderPassUpdate {
    /// Register a new pass
    RegisterPass { config: PassConfig },
    /// Unregister a pass
    UnregisterPass { name: String },
    /// Enable/disable a pass
    SetPassEnabled { name: String, enabled: bool },
    /// Update pass configuration
    UpdatePassConfig { name: String, config: PassConfig },
    /// Reorder passes
    SetPassOrder { order: Vec<String> },
    /// Try to restore quality
    TryRestoreQuality,
}

/// Queue for render pass updates
#[derive(Debug, Default)]
pub struct RenderPassUpdateQueue {
    updates: Vec<RenderPassUpdate>,
}

impl RenderPassUpdateQueue {
    /// Create a new update queue
    pub fn new() -> Self {
        Self {
            updates: Vec::new(),
        }
    }

    /// Queue an update
    pub fn queue(&mut self, update: RenderPassUpdate) {
        self.updates.push(update);
    }

    /// Queue pass registration
    pub fn register_pass(&mut self, config: PassConfig) {
        self.queue(RenderPassUpdate::RegisterPass { config });
    }

    /// Queue pass unregistration
    pub fn unregister_pass(&mut self, name: impl Into<String>) {
        self.queue(RenderPassUpdate::UnregisterPass { name: name.into() });
    }

    /// Queue enable/disable
    pub fn set_pass_enabled(&mut self, name: impl Into<String>, enabled: bool) {
        self.queue(RenderPassUpdate::SetPassEnabled {
            name: name.into(),
            enabled,
        });
    }

    /// Queue pass order change
    pub fn set_pass_order(&mut self, order: Vec<String>) {
        self.queue(RenderPassUpdate::SetPassOrder { order });
    }

    /// Queue quality restoration
    pub fn try_restore_quality(&mut self) {
        self.queue(RenderPassUpdate::TryRestoreQuality);
    }

    /// Apply all pending updates
    pub fn apply_pending(&mut self, system: &mut RenderPassSystem) {
        for update in self.updates.drain(..) {
            match update {
                RenderPassUpdate::RegisterPass { config } => {
                    system.register_pass(config);
                }
                RenderPassUpdate::UnregisterPass { name } => {
                    system.unregister_pass(&name);
                }
                RenderPassUpdate::SetPassEnabled { name, enabled } => {
                    system.set_pass_enabled(&name, enabled);
                }
                RenderPassUpdate::UpdatePassConfig { name, config } => {
                    system.update_pass_config(&name, config);
                }
                RenderPassUpdate::SetPassOrder { order } => {
                    system.set_pass_order(order);
                }
                RenderPassUpdate::TryRestoreQuality => {
                    system.try_restore_quality();
                }
            }
        }
    }

    /// Check if queue is empty
    pub fn is_empty(&self) -> bool {
        self.updates.is_empty()
    }

    /// Get number of pending updates
    pub fn len(&self) -> usize {
        self.updates.len()
    }
}

/// Statistics for render pass system
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct RenderPassStats {
    /// Total draw calls across all passes
    pub total_draw_calls: usize,
    /// Draw calls per pass
    pub draw_calls_per_pass: Vec<(String, usize)>,
    /// Number of enabled passes
    pub enabled_passes: usize,
    /// Current quality level
    pub quality: PassQuality,
}

impl RenderPassSystem {
    /// Get statistics for the current frame
    pub fn get_stats(&self) -> RenderPassStats {
        RenderPassStats {
            total_draw_calls: self.total_draw_calls(),
            draw_calls_per_pass: self
                .passes_in_order()
                .map(|(name, _, calls)| (name.to_string(), calls.len()))
                .collect(),
            enabled_passes: self.passes_in_order().count(),
            quality: self.quality(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_render_pass_update_queue() {
        let mut queue = RenderPassUpdateQueue::new();

        queue.set_pass_enabled("shadow", false);
        queue.register_pass(PassConfig {
            name: "custom".into(),
            flags: RenderPassFlags::CUSTOM_0,
            sort: PassSortMode::None,
            clear_color: None,
            clear_depth: false,
            depth_test: true,
            depth_write: false,
            cull_mode: CullMode::Back,
            blend_mode: None,
            priority: 50,
        });

        assert_eq!(queue.len(), 2);

        let mut system = RenderPassSystem::new();
        queue.apply_pending(&mut system);

        assert!(!system.is_pass_enabled("shadow"));
        assert!(system.get_pass_config("custom").is_some());
    }

    #[test]
    fn test_render_pass_stats() {
        let mut system = RenderPassSystem::new();
        system.begin_frame();

        // Add some draw calls
        for i in 0..5 {
            let draw_call = PassDrawCall {
                entity_bits: i as u64,
                ..Default::default()
            };
            system.add_draw_call(draw_call, RenderPassFlags::OPAQUE, &[]);
        }

        let stats = system.get_stats();
        assert_eq!(stats.total_draw_calls, 15); // 5 * 3 passes (main, shadow, depth)
        assert_eq!(stats.quality, PassQuality::Full);
    }

    #[test]
    fn test_render_pass_update_serialization() {
        let updates = vec![
            RenderPassUpdate::SetPassEnabled {
                name: "shadow".into(),
                enabled: false,
            },
            RenderPassUpdate::SetPassOrder {
                order: vec!["main".into(), "shadow".into()],
            },
            RenderPassUpdate::TryRestoreQuality,
        ];

        for update in updates {
            let json = serde_json::to_string(&update).unwrap();
            let restored: RenderPassUpdate = serde_json::from_str(&json).unwrap();
            // Verify it deserializes without error
            let _ = restored;
        }
    }
}
