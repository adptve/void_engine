//! Outline Pass
//!
//! Renders selection outlines around entities using jump flood algorithm
//! or edge detection.
//!
//! # Algorithm
//!
//! 1. Render selected entities to stencil buffer
//! 2. Apply jump flood algorithm to expand the mask
//! 3. Detect edges and apply outline color
//! 4. Composite over main color
//!
//! # Example
//!
//! ```ignore
//! use void_render::pass::builtin::OutlinePass;
//!
//! let mut outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);
//! outline.set_selected_entities(vec![1, 2, 3]); // Entity bits
//! registry.register(outline)?;
//! ```

use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use crate::pass::custom::{
    CustomRenderPass, PassConfigData, PassError, PassExecuteContext, PassPriority,
    PassSetupContext, ResourceRef, ResourceRequirements, TextureFormatHint,
};

/// Configuration for the outline pass
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct OutlinePassConfig {
    /// Outline color (RGBA)
    pub color: [f32; 4],

    /// Outline width in pixels
    pub width: f32,

    /// Whether to use soft edges
    pub soft_edges: bool,

    /// Edge falloff (for soft edges)
    pub falloff: f32,

    /// Whether outline is enabled
    pub enabled: bool,
}

impl Default for OutlinePassConfig {
    fn default() -> Self {
        Self {
            color: [1.0, 0.5, 0.0, 1.0], // Orange
            width: 2.0,
            soft_edges: true,
            falloff: 1.0,
            enabled: true,
        }
    }
}

/// Selection outline render pass
pub struct OutlinePass {
    /// Outline color
    color: [f32; 4],

    /// Outline width in pixels
    width: f32,

    /// Use soft edges
    soft_edges: bool,

    /// Edge falloff
    falloff: f32,

    /// Whether the pass is enabled
    enabled: bool,

    /// Selected entity bits
    selected_entities: Vec<u64>,

    /// Whether GPU resources are initialized
    initialized: bool,

    /// Current surface size
    surface_size: (u32, u32),
}

impl OutlinePass {
    /// Create a new outline pass
    pub fn new(color: [f32; 4], width: f32) -> Self {
        Self {
            color,
            width: width.max(0.5),
            soft_edges: true,
            falloff: 1.0,
            enabled: true,
            selected_entities: Vec::new(),
            initialized: false,
            surface_size: (1920, 1080),
        }
    }

    /// Create with full configuration
    pub fn with_config(config: OutlinePassConfig) -> Self {
        Self {
            color: config.color,
            width: config.width.max(0.5),
            soft_edges: config.soft_edges,
            falloff: config.falloff,
            enabled: config.enabled,
            selected_entities: Vec::new(),
            initialized: false,
            surface_size: (1920, 1080),
        }
    }

    /// Set outline color
    pub fn set_color(&mut self, color: [f32; 4]) {
        self.color = color;
    }

    /// Get outline color
    pub fn color(&self) -> [f32; 4] {
        self.color
    }

    /// Set outline width
    pub fn set_width(&mut self, width: f32) {
        self.width = width.max(0.5);
    }

    /// Get outline width
    pub fn width(&self) -> f32 {
        self.width
    }

    /// Set soft edges
    pub fn set_soft_edges(&mut self, soft: bool) {
        self.soft_edges = soft;
    }

    /// Get soft edges setting
    pub fn soft_edges(&self) -> bool {
        self.soft_edges
    }

    /// Set edge falloff
    pub fn set_falloff(&mut self, falloff: f32) {
        self.falloff = falloff.max(0.1);
    }

    /// Get edge falloff
    pub fn falloff(&self) -> f32 {
        self.falloff
    }

    /// Set selected entity bits
    pub fn set_selected_entities(&mut self, entities: Vec<u64>) {
        self.selected_entities = entities;
    }

    /// Add entity to selection
    pub fn add_selected(&mut self, entity_bits: u64) {
        if !self.selected_entities.contains(&entity_bits) {
            self.selected_entities.push(entity_bits);
        }
    }

    /// Remove entity from selection
    pub fn remove_selected(&mut self, entity_bits: u64) {
        self.selected_entities.retain(|&e| e != entity_bits);
    }

    /// Clear selection
    pub fn clear_selection(&mut self) {
        self.selected_entities.clear();
    }

    /// Get selected entities
    pub fn selected_entities(&self) -> &[u64] {
        &self.selected_entities
    }

    /// Check if any entities are selected
    pub fn has_selection(&self) -> bool {
        !self.selected_entities.is_empty()
    }

    /// Get current configuration
    pub fn config(&self) -> OutlinePassConfig {
        OutlinePassConfig {
            color: self.color,
            width: self.width,
            soft_edges: self.soft_edges,
            falloff: self.falloff,
            enabled: self.enabled,
        }
    }
}

impl CustomRenderPass for OutlinePass {
    fn name(&self) -> &str {
        "outline"
    }

    fn dependencies(&self) -> &[&str] {
        &["main"]
    }

    fn reads(&self) -> Vec<ResourceRef> {
        vec![ResourceRef::MainColor, ResourceRef::MainDepth]
    }

    fn writes(&self) -> Vec<ResourceRef> {
        vec![ResourceRef::MainColor]
    }

    fn setup(&mut self, context: &PassSetupContext) -> Result<(), PassError> {
        self.surface_size = context.surface_size;

        // In a real implementation, this would create:
        // - Stencil texture for selection mask
        // - Jump flood textures (ping-pong)
        // - Outline render pipeline
        // - Composite pipeline

        self.initialized = true;
        Ok(())
    }

    fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError> {
        if !self.enabled || !self.initialized || self.selected_entities.is_empty() {
            return Ok(());
        }

        // Get required resources
        let _main_color = context
            .resources
            .main_color()
            .ok_or_else(|| PassError::Resource("main_color not found".into()))?;

        let _main_depth = context
            .resources
            .main_depth()
            .ok_or_else(|| PassError::Resource("main_depth not found".into()))?;

        // In a real implementation:
        // 1. Render selected entities to stencil
        // 2. Run jump flood algorithm to expand mask
        // 3. Detect edges between mask and background
        // 4. Apply outline color with falloff
        // 5. Composite over main color (respecting depth)

        Ok(())
    }

    fn cleanup(&mut self) {
        self.initialized = false;
    }

    fn resource_requirements(&self) -> ResourceRequirements {
        // Stencil + 2 jump flood textures (R16 format)
        let stencil_size = self.surface_size.0 as u64 * self.surface_size.1 as u64;
        let jfa_size = stencil_size * 2 * 2; // R16 = 2 bytes, 2 textures

        ResourceRequirements {
            memory_bytes: stencil_size + jfa_size,
            render_targets: 2,
            compute: false,
            time_budget_ms: 0.5,
            texture_formats: vec![TextureFormatHint::R16Float],
            ..Default::default()
        }
    }

    fn priority(&self) -> PassPriority {
        // Run after main post-process but before UI
        PassPriority(PassPriority::POST_PROCESS.0 + 50)
    }

    fn is_enabled(&self) -> bool {
        self.enabled
    }

    fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    fn on_resize(&mut self, new_size: (u32, u32)) {
        self.surface_size = new_size;
        // In a real implementation, would recreate stencil and JFA textures
    }

    fn get_config(&self) -> Option<PassConfigData> {
        Some(PassConfigData {
            name: "outline".to_string(),
            enabled: self.enabled,
            priority: self.priority().0,
            config: serde_json::to_value(self.config()).unwrap_or_default(),
        })
    }

    fn apply_config(&mut self, config: &PassConfigData) -> Result<(), PassError> {
        self.enabled = config.enabled;

        if let Ok(outline_config) =
            serde_json::from_value::<OutlinePassConfig>(config.config.clone())
        {
            self.color = outline_config.color;
            self.width = outline_config.width.max(0.5);
            self.soft_edges = outline_config.soft_edges;
            self.falloff = outline_config.falloff;
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_outline_pass_creation() {
        let outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);
        assert_eq!(outline.name(), "outline");
        assert_eq!(outline.color(), [1.0, 0.5, 0.0, 1.0]);
        assert_eq!(outline.width(), 2.0);
        assert!(outline.is_enabled());
    }

    #[test]
    fn test_outline_pass_config() {
        let config = OutlinePassConfig {
            color: [0.0, 1.0, 0.0, 1.0],
            width: 3.0,
            soft_edges: false,
            falloff: 0.5,
            enabled: true,
        };

        let outline = OutlinePass::with_config(config);
        assert_eq!(outline.color(), [0.0, 1.0, 0.0, 1.0]);
        assert_eq!(outline.width(), 3.0);
        assert!(!outline.soft_edges());
    }

    #[test]
    fn test_outline_pass_selection() {
        let mut outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);

        assert!(!outline.has_selection());

        outline.add_selected(1);
        outline.add_selected(2);
        outline.add_selected(3);

        assert!(outline.has_selection());
        assert_eq!(outline.selected_entities().len(), 3);

        // Adding duplicate shouldn't increase count
        outline.add_selected(1);
        assert_eq!(outline.selected_entities().len(), 3);

        outline.remove_selected(2);
        assert_eq!(outline.selected_entities().len(), 2);

        outline.clear_selection();
        assert!(!outline.has_selection());
    }

    #[test]
    fn test_outline_pass_setters() {
        let mut outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);

        outline.set_color([0.0, 0.0, 1.0, 1.0]);
        assert_eq!(outline.color(), [0.0, 0.0, 1.0, 1.0]);

        outline.set_width(5.0);
        assert_eq!(outline.width(), 5.0);

        outline.set_soft_edges(false);
        assert!(!outline.soft_edges());

        outline.set_falloff(2.0);
        assert_eq!(outline.falloff(), 2.0);
    }

    #[test]
    fn test_outline_pass_width_clamping() {
        let mut outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 0.1);
        assert_eq!(outline.width(), 0.5); // Clamped to minimum

        outline.set_width(-1.0);
        assert_eq!(outline.width(), 0.5);
    }

    #[test]
    fn test_outline_pass_dependencies() {
        let outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);
        assert_eq!(outline.dependencies(), &["main"]);
    }

    #[test]
    fn test_outline_pass_resources() {
        let outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);
        let reads = outline.reads();
        let writes = outline.writes();

        assert!(reads.contains(&ResourceRef::MainColor));
        assert!(reads.contains(&ResourceRef::MainDepth));
        assert!(writes.contains(&ResourceRef::MainColor));
    }

    #[test]
    fn test_outline_pass_config_serialization() {
        let config = OutlinePassConfig {
            color: [1.0, 0.5, 0.0, 1.0],
            width: 2.5,
            soft_edges: true,
            falloff: 1.5,
            enabled: true,
        };

        let json = serde_json::to_string(&config).unwrap();
        let restored: OutlinePassConfig = serde_json::from_str(&json).unwrap();

        assert_eq!(config.color, restored.color);
        assert_eq!(config.width, restored.width);
        assert_eq!(config.soft_edges, restored.soft_edges);
    }

    #[test]
    fn test_outline_pass_execute_skips_empty_selection() {
        let outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);
        let ctx = PassExecuteContext::default();

        // Should not error when no selection
        assert!(outline.execute(&ctx).is_ok());
    }

    #[test]
    fn test_outline_pass_setup() {
        let mut outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);
        let ctx = PassSetupContext {
            surface_size: (1920, 1080),
            ..Default::default()
        };

        assert!(outline.setup(&ctx).is_ok());
        assert!(outline.initialized);
    }

    #[test]
    fn test_outline_pass_get_apply_config() {
        let mut outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);

        let config = outline.get_config().unwrap();
        assert_eq!(config.name, "outline");

        let modified = PassConfigData {
            name: "outline".into(),
            enabled: false,
            priority: 150,
            config: serde_json::json!({
                "color": [0.0, 1.0, 0.0, 1.0],
                "width": 4.0,
                "soft_edges": false,
                "falloff": 2.0,
                "enabled": false
            }),
        };

        outline.apply_config(&modified).unwrap();

        assert!(!outline.is_enabled());
        assert_eq!(outline.color(), [0.0, 1.0, 0.0, 1.0]);
        assert_eq!(outline.width(), 4.0);
        assert!(!outline.soft_edges());
    }

    #[test]
    fn test_outline_pass_set_selected_entities() {
        let mut outline = OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0);

        outline.set_selected_entities(vec![10, 20, 30]);
        assert_eq!(outline.selected_entities(), &[10, 20, 30]);

        outline.set_selected_entities(vec![40]);
        assert_eq!(outline.selected_entities(), &[40]);
    }
}
