//! Render Pass System
//!
//! Manages render pass configurations and draw call distribution.

use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use super::flags::{PassId, RenderPassFlags};

/// Sort mode for draw calls within a pass
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum PassSortMode {
    /// No sorting
    None,
    /// Front to back (for opaque, reduces overdraw)
    #[default]
    FrontToBack,
    /// Back to front (for transparent)
    BackToFront,
    /// Sort by material to minimize state changes
    ByMaterial,
    /// Sort by custom key
    Custom,
}

/// Face culling mode
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum CullMode {
    /// No culling
    None,
    /// Cull back faces (default)
    #[default]
    Back,
    /// Cull front faces (used for shadow mapping)
    Front,
}

/// Blend mode for transparent rendering
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum BlendMode {
    /// Standard alpha blending
    Alpha,
    /// Additive blending
    Additive,
    /// Multiplicative blending
    Multiply,
    /// Premultiplied alpha
    Premultiplied,
}

/// Configuration for a render pass
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PassConfig {
    /// Pass name
    pub name: String,
    /// Pass flags (which entities participate)
    pub flags: RenderPassFlags,
    /// Sort mode
    pub sort: PassSortMode,
    /// Clear color (None = don't clear)
    pub clear_color: Option<[f32; 4]>,
    /// Clear depth
    pub clear_depth: bool,
    /// Enable depth testing
    pub depth_test: bool,
    /// Enable depth writing
    pub depth_write: bool,
    /// Face culling mode
    pub cull_mode: CullMode,
    /// Blend mode (None = opaque)
    pub blend_mode: Option<BlendMode>,
    /// Priority (lower = earlier)
    pub priority: i32,
}

impl PassConfig {
    /// Create a depth prepass config
    pub fn depth_prepass() -> Self {
        Self {
            name: "depth_prepass".into(),
            flags: RenderPassFlags::DEPTH_PREPASS,
            sort: PassSortMode::FrontToBack,
            clear_color: None,
            clear_depth: true,
            depth_test: true,
            depth_write: true,
            cull_mode: CullMode::Back,
            blend_mode: None,
            priority: 0,
        }
    }

    /// Create a shadow pass config
    pub fn shadow() -> Self {
        Self {
            name: "shadow".into(),
            flags: RenderPassFlags::SHADOW,
            sort: PassSortMode::FrontToBack,
            clear_color: None,
            clear_depth: true,
            depth_test: true,
            depth_write: true,
            cull_mode: CullMode::Front, // Front-face culling for shadows
            blend_mode: None,
            priority: 10,
        }
    }

    /// Create a main pass config
    pub fn main() -> Self {
        Self {
            name: "main".into(),
            flags: RenderPassFlags::MAIN,
            sort: PassSortMode::ByMaterial,
            clear_color: Some([0.1, 0.1, 0.1, 1.0]),
            clear_depth: false, // Use depth from prepass
            depth_test: true,
            depth_write: false, // Already written in prepass
            cull_mode: CullMode::Back,
            blend_mode: None,
            priority: 20,
        }
    }

    /// Create a transparent pass config
    pub fn transparent() -> Self {
        Self {
            name: "transparent".into(),
            flags: RenderPassFlags::TRANSPARENT,
            sort: PassSortMode::BackToFront,
            clear_color: None,
            clear_depth: false,
            depth_test: true,
            depth_write: false,
            cull_mode: CullMode::None, // Draw both sides
            blend_mode: Some(BlendMode::Alpha),
            priority: 30,
        }
    }
}

/// Draw call data for a pass
#[derive(Clone, Debug, Default)]
pub struct PassDrawCall {
    /// Entity bits
    pub entity_bits: u64,
    /// Mesh handle/index
    pub mesh_index: u32,
    /// Material handle/index
    pub material_index: u32,
    /// Instance index in buffer
    pub instance_index: u32,
    /// Instance count
    pub instance_count: u32,
    /// Distance from camera (for sorting)
    pub camera_distance: f32,
    /// Custom sort key
    pub sort_key: i32,
    /// Material override (if any)
    pub material_override: Option<String>,
}

impl PassDrawCall {
    /// Get material key for sorting
    pub fn material_key(&self) -> u32 {
        self.material_index
    }
}

/// Data for a single render pass
struct RenderPassData {
    /// Pass configuration
    config: PassConfig,
    /// Draw calls for this frame
    draw_calls: Vec<PassDrawCall>,
    /// Is pass enabled
    enabled: bool,
}

/// Render pass quality levels for graceful degradation
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum PassQuality {
    /// All passes enabled
    #[default]
    Full,
    /// Skip optional passes (reflection, refraction)
    Essential,
    /// Main and depth only
    Minimal,
    /// Main pass only
    Emergency,
}

/// Manages render passes and draw call distribution
pub struct RenderPassSystem {
    /// Named passes
    passes: BTreeMap<String, RenderPassData>,
    /// Pass execution order (by name)
    order: Vec<String>,
    /// Quality level
    quality: PassQuality,
    /// Error counter for degradation
    error_count: u32,
    /// Frame counter
    frame_count: u32,
}

impl Default for RenderPassSystem {
    fn default() -> Self {
        Self::new()
    }
}

impl RenderPassSystem {
    /// Create a new render pass system with standard passes
    pub fn new() -> Self {
        let mut system = Self {
            passes: BTreeMap::new(),
            order: Vec::new(),
            quality: PassQuality::Full,
            error_count: 0,
            frame_count: 0,
        };

        // Register standard passes in order
        system.register_pass(PassConfig::depth_prepass());
        system.register_pass(PassConfig::shadow());
        system.register_pass(PassConfig::main());
        system.register_pass(PassConfig::transparent());

        system
    }

    /// Register a render pass
    pub fn register_pass(&mut self, config: PassConfig) {
        let name = config.name.clone();
        let priority = config.priority;

        self.passes.insert(
            name.clone(),
            RenderPassData {
                config,
                draw_calls: Vec::new(),
                enabled: true,
            },
        );

        // Insert in order by priority
        let insert_pos = self
            .order
            .iter()
            .position(|n| {
                self.passes
                    .get(n)
                    .map(|p| p.config.priority > priority)
                    .unwrap_or(false)
            })
            .unwrap_or(self.order.len());

        self.order.insert(insert_pos, name);
    }

    /// Unregister a pass
    pub fn unregister_pass(&mut self, name: &str) {
        self.passes.remove(name);
        self.order.retain(|n| n != name);
    }

    /// Get pass configuration
    pub fn get_pass_config(&self, name: &str) -> Option<&PassConfig> {
        self.passes.get(name).map(|p| &p.config)
    }

    /// Get mutable pass configuration
    pub fn get_pass_config_mut(&mut self, name: &str) -> Option<&mut PassConfig> {
        self.passes.get_mut(name).map(|p| &mut p.config)
    }

    /// Update pass configuration
    pub fn update_pass_config(&mut self, name: &str, config: PassConfig) {
        if let Some(pass) = self.passes.get_mut(name) {
            pass.config = config;
        }
    }

    /// Enable/disable a pass
    pub fn set_pass_enabled(&mut self, name: &str, enabled: bool) {
        if let Some(pass) = self.passes.get_mut(name) {
            pass.enabled = enabled;
        }
    }

    /// Check if pass is enabled
    pub fn is_pass_enabled(&self, name: &str) -> bool {
        self.passes.get(name).map(|p| p.enabled).unwrap_or(false)
    }

    /// Set pass execution order
    pub fn set_pass_order(&mut self, order: Vec<String>) {
        self.order = order;
    }

    /// Get pass order
    pub fn pass_order(&self) -> &[String] {
        &self.order
    }

    /// Clear all draw calls for new frame
    pub fn begin_frame(&mut self) {
        self.frame_count = self.frame_count.wrapping_add(1);
        for pass in self.passes.values_mut() {
            pass.draw_calls.clear();
        }
    }

    /// Add draw call to appropriate passes based on entity's render pass flags
    pub fn add_draw_call(
        &mut self,
        draw_call: PassDrawCall,
        entity_flags: RenderPassFlags,
        custom_passes: &[String],
    ) {
        for (name, pass) in &mut self.passes {
            let should_include = entity_flags.intersects(pass.config.flags)
                || custom_passes.iter().any(|p| p == name);

            if should_include && pass.enabled {
                pass.draw_calls.push(draw_call.clone());
            }
        }
    }

    /// Sort all passes
    pub fn sort_passes(&mut self, camera_pos: [f32; 3]) {
        for pass in self.passes.values_mut() {
            if !pass.enabled {
                continue;
            }
            Self::sort_pass_draw_calls(pass, camera_pos);
        }
    }

    fn sort_pass_draw_calls(pass: &mut RenderPassData, _camera_pos: [f32; 3]) {
        match pass.config.sort {
            PassSortMode::None => {}
            PassSortMode::FrontToBack => {
                pass.draw_calls.sort_by(|a, b| {
                    a.camera_distance
                        .partial_cmp(&b.camera_distance)
                        .unwrap_or(core::cmp::Ordering::Equal)
                });
            }
            PassSortMode::BackToFront => {
                pass.draw_calls.sort_by(|a, b| {
                    b.camera_distance
                        .partial_cmp(&a.camera_distance)
                        .unwrap_or(core::cmp::Ordering::Equal)
                });
            }
            PassSortMode::ByMaterial => {
                pass.draw_calls.sort_by_key(|c| c.material_key());
            }
            PassSortMode::Custom => {
                pass.draw_calls.sort_by_key(|c| c.sort_key);
            }
        }
    }

    /// Get passes in execution order
    pub fn passes_in_order(&self) -> impl Iterator<Item = (&str, &PassConfig, &[PassDrawCall])> {
        self.order.iter().filter_map(|name| {
            self.passes.get(name).filter(|p| p.enabled).map(|p| {
                (name.as_str(), &p.config, p.draw_calls.as_slice())
            })
        })
    }

    /// Get draw calls for a specific pass
    pub fn get_draw_calls(&self, name: &str) -> Option<&[PassDrawCall]> {
        self.passes
            .get(name)
            .filter(|p| p.enabled)
            .map(|p| p.draw_calls.as_slice())
    }

    /// Get total draw call count across all passes
    pub fn total_draw_calls(&self) -> usize {
        self.passes
            .values()
            .filter(|p| p.enabled)
            .map(|p| p.draw_calls.len())
            .sum()
    }

    /// Get current quality level
    pub fn quality(&self) -> PassQuality {
        self.quality
    }

    /// Record an error
    pub fn record_error(&mut self) {
        self.error_count = self.error_count.saturating_add(1);
    }

    /// Check health and potentially degrade quality
    pub fn check_health_and_degrade(&mut self) {
        const ERROR_THRESHOLD: f32 = 0.05;

        if self.frame_count == 0 {
            return;
        }

        let error_rate = self.error_count as f32 / self.frame_count as f32;

        if error_rate > ERROR_THRESHOLD {
            self.quality = match self.quality {
                PassQuality::Full => {
                    self.disable_optional_passes();
                    PassQuality::Essential
                }
                PassQuality::Essential => {
                    self.disable_non_essential_passes();
                    PassQuality::Minimal
                }
                PassQuality::Minimal => {
                    self.enable_only_main_pass();
                    PassQuality::Emergency
                }
                PassQuality::Emergency => PassQuality::Emergency,
            };
            self.error_count = 0;
            self.frame_count = 0;
        }
    }

    /// Try to restore full quality
    pub fn try_restore_quality(&mut self) {
        match self.quality {
            PassQuality::Emergency => {
                self.set_pass_enabled("depth_prepass", true);
                self.quality = PassQuality::Minimal;
            }
            PassQuality::Minimal => {
                self.set_pass_enabled("shadow", true);
                self.set_pass_enabled("transparent", true);
                self.quality = PassQuality::Essential;
            }
            PassQuality::Essential => {
                self.set_pass_enabled("reflection", true);
                self.set_pass_enabled("refraction", true);
                self.set_pass_enabled("motion_vectors", true);
                self.quality = PassQuality::Full;
            }
            PassQuality::Full => {}
        }
        self.error_count = 0;
        self.frame_count = 0;
    }

    fn disable_optional_passes(&mut self) {
        self.set_pass_enabled("reflection", false);
        self.set_pass_enabled("refraction", false);
        self.set_pass_enabled("motion_vectors", false);
    }

    fn disable_non_essential_passes(&mut self) {
        self.disable_optional_passes();
        self.set_pass_enabled("transparent", false);
        self.set_pass_enabled("shadow", false);
    }

    fn enable_only_main_pass(&mut self) {
        for (name, pass) in &mut self.passes {
            pass.enabled = name == "main";
        }
    }
}

/// Serializable state for hot-reload
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct RenderPassSystemState {
    /// Pass configurations
    pub pass_configs: Vec<PassConfig>,
    /// Pass order
    pub order: Vec<String>,
    /// Enabled states
    pub enabled: BTreeMap<String, bool>,
    /// Quality level
    pub quality: PassQuality,
}

impl RenderPassSystem {
    /// Save state for hot-reload
    pub fn save_state(&self) -> RenderPassSystemState {
        RenderPassSystemState {
            pass_configs: self.passes.values().map(|p| p.config.clone()).collect(),
            order: self.order.clone(),
            enabled: self.passes.iter().map(|(n, p)| (n.clone(), p.enabled)).collect(),
            quality: self.quality,
        }
    }

    /// Restore state from hot-reload
    pub fn restore_state(&mut self, state: RenderPassSystemState) {
        // Restore pass configs
        for config in state.pass_configs {
            if let Some(pass) = self.passes.get_mut(&config.name) {
                pass.config = config;
            } else {
                self.register_pass(config);
            }
        }

        // Restore enabled states
        for (name, enabled) in state.enabled {
            self.set_pass_enabled(&name, enabled);
        }

        // Restore order
        self.order = state.order;

        // Restore quality
        self.quality = state.quality;

        // Clear draw calls
        for pass in self.passes.values_mut() {
            pass.draw_calls.clear();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_render_pass_system_new() {
        let system = RenderPassSystem::new();
        assert!(system.get_pass_config("main").is_some());
        assert!(system.get_pass_config("shadow").is_some());
        assert!(system.get_pass_config("depth_prepass").is_some());
        assert!(system.get_pass_config("transparent").is_some());
    }

    #[test]
    fn test_render_pass_system_order() {
        let system = RenderPassSystem::new();
        let order = system.pass_order();

        // depth_prepass should come before main
        let depth_pos = order.iter().position(|n| n == "depth_prepass").unwrap();
        let main_pos = order.iter().position(|n| n == "main").unwrap();
        assert!(depth_pos < main_pos);
    }

    #[test]
    fn test_render_pass_system_add_draw_call() {
        let mut system = RenderPassSystem::new();
        system.begin_frame();

        let draw_call = PassDrawCall {
            entity_bits: 1,
            mesh_index: 0,
            material_index: 0,
            instance_index: 0,
            instance_count: 1,
            camera_distance: 10.0,
            sort_key: 0,
            material_override: None,
        };

        // Opaque entity
        system.add_draw_call(draw_call.clone(), RenderPassFlags::OPAQUE, &[]);

        // Should be in main, shadow, depth_prepass
        assert_eq!(system.get_draw_calls("main").unwrap().len(), 1);
        assert_eq!(system.get_draw_calls("shadow").unwrap().len(), 1);
        assert_eq!(system.get_draw_calls("depth_prepass").unwrap().len(), 1);
        assert_eq!(system.get_draw_calls("transparent").unwrap().len(), 0);
    }

    #[test]
    fn test_render_pass_system_transparent() {
        let mut system = RenderPassSystem::new();
        system.begin_frame();

        let draw_call = PassDrawCall::default();

        // Transparent entity
        system.add_draw_call(
            draw_call,
            RenderPassFlags::MAIN | RenderPassFlags::TRANSPARENT,
            &[],
        );

        assert_eq!(system.get_draw_calls("main").unwrap().len(), 1);
        assert_eq!(system.get_draw_calls("transparent").unwrap().len(), 1);
        assert_eq!(system.get_draw_calls("shadow").unwrap().len(), 0);
    }

    #[test]
    fn test_render_pass_system_custom_pass() {
        let mut system = RenderPassSystem::new();

        // Register custom pass
        system.register_pass(PassConfig {
            name: "outline".into(),
            flags: RenderPassFlags::CUSTOM_0,
            sort: PassSortMode::None,
            clear_color: None,
            clear_depth: false,
            depth_test: true,
            depth_write: false,
            cull_mode: CullMode::Back,
            blend_mode: Some(BlendMode::Alpha),
            priority: 25,
        });

        system.begin_frame();

        let draw_call = PassDrawCall::default();
        system.add_draw_call(draw_call, RenderPassFlags::NONE, &["outline".into()]);

        assert_eq!(system.get_draw_calls("outline").unwrap().len(), 1);
    }

    #[test]
    fn test_render_pass_system_sorting() {
        let mut system = RenderPassSystem::new();
        system.begin_frame();

        // Add draw calls with different distances
        for i in 0..3 {
            let draw_call = PassDrawCall {
                entity_bits: i as u64,
                camera_distance: (3 - i) as f32 * 10.0,
                ..Default::default()
            };
            system.add_draw_call(draw_call, RenderPassFlags::DEPTH_PREPASS, &[]);
        }

        system.sort_passes([0.0, 0.0, 0.0]);

        // Depth prepass should be sorted front-to-back
        let calls = system.get_draw_calls("depth_prepass").unwrap();
        assert!(calls[0].camera_distance <= calls[1].camera_distance);
        assert!(calls[1].camera_distance <= calls[2].camera_distance);
    }

    #[test]
    fn test_render_pass_system_enable_disable() {
        let mut system = RenderPassSystem::new();

        assert!(system.is_pass_enabled("shadow"));
        system.set_pass_enabled("shadow", false);
        assert!(!system.is_pass_enabled("shadow"));

        system.begin_frame();
        let draw_call = PassDrawCall::default();
        system.add_draw_call(draw_call, RenderPassFlags::OPAQUE, &[]);

        // Shadow pass should have no draw calls since it's disabled
        assert!(system.get_draw_calls("shadow").is_none());
    }

    #[test]
    fn test_render_pass_system_state() {
        let mut system = RenderPassSystem::new();
        system.set_pass_enabled("shadow", false);

        let state = system.save_state();

        let mut new_system = RenderPassSystem::new();
        new_system.restore_state(state);

        assert!(!new_system.is_pass_enabled("shadow"));
    }

    #[test]
    fn test_pass_config_serialization() {
        let config = PassConfig::main();
        let json = serde_json::to_string(&config).unwrap();
        let restored: PassConfig = serde_json::from_str(&json).unwrap();
        assert_eq!(config.name, restored.name);
        assert_eq!(config.clear_color, restored.clear_color);
    }

    #[test]
    fn test_sort_mode_serialization() {
        let modes = [
            PassSortMode::None,
            PassSortMode::FrontToBack,
            PassSortMode::BackToFront,
            PassSortMode::ByMaterial,
            PassSortMode::Custom,
        ];

        for mode in modes {
            let json = serde_json::to_string(&mode).unwrap();
            let restored: PassSortMode = serde_json::from_str(&json).unwrap();
            assert_eq!(mode, restored);
        }
    }

    #[test]
    fn test_blend_mode_serialization() {
        let modes = [
            BlendMode::Alpha,
            BlendMode::Additive,
            BlendMode::Multiply,
            BlendMode::Premultiplied,
        ];

        for mode in modes {
            let json = serde_json::to_string(&mode).unwrap();
            let restored: BlendMode = serde_json::from_str(&json).unwrap();
            assert_eq!(mode, restored);
        }
    }

    #[test]
    fn test_cull_mode_serialization() {
        let modes = [CullMode::None, CullMode::Back, CullMode::Front];

        for mode in modes {
            let json = serde_json::to_string(&mode).unwrap();
            let restored: CullMode = serde_json::from_str(&json).unwrap();
            assert_eq!(mode, restored);
        }
    }

    #[test]
    fn test_quality_degradation() {
        let mut system = RenderPassSystem::new();
        assert_eq!(system.quality(), PassQuality::Full);

        // Simulate high error rate
        system.frame_count = 100;
        system.error_count = 10; // 10% error rate

        system.check_health_and_degrade();
        assert_eq!(system.quality(), PassQuality::Essential);
    }

    #[test]
    fn test_quality_restoration() {
        let mut system = RenderPassSystem::new();
        system.quality = PassQuality::Minimal;

        system.try_restore_quality();
        assert_eq!(system.quality(), PassQuality::Essential);

        system.try_restore_quality();
        assert_eq!(system.quality(), PassQuality::Full);
    }
}
