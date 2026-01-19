//! Layer system for compositor
//!
//! Layers provide isolation and composition with:
//! - Health tracking (shader failures don't crash the frame)
//! - z-order sorting
//! - Viewport management (fullscreen vs windowed)
//! - Opacity and blend modes
//! - Dirty region tracking for performance

use crate::resource::{TextureDesc, TextureFormat, TextureUsage, TextureDimension};
use crate::graph::GraphHandle;
use alloc::string::String;
use alloc::vec::Vec;
use void_core::{Id, IdGenerator};
use core::sync::atomic::{AtomicU32, Ordering};

/// Global layer ID counter for unique IDs
static LAYER_ID_COUNTER: AtomicU32 = AtomicU32::new(0);

/// Unique identifier for a render layer
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct LayerId(pub Id);

impl LayerId {
    /// Create a new unique layer ID
    pub fn new() -> Self {
        let index = LAYER_ID_COUNTER.fetch_add(1, Ordering::Relaxed);
        Self(Id::new(index, 0))
    }

    /// Create from a name (deterministic)
    pub fn from_name(name: &str) -> Self {
        Self(Id::from_name(name))
    }

    /// Get the inner ID
    pub fn id(&self) -> Id {
        self.0
    }
}

impl Default for LayerId {
    fn default() -> Self {
        Self::new()
    }
}

/// Layer health status
#[derive(Clone, Debug, PartialEq)]
pub enum LayerHealth {
    /// Layer is healthy and rendering correctly
    Healthy,
    /// Shader compilation or execution failed
    ShaderFailed { error: String },
    /// Resource allocation failed (out of memory, etc.)
    ResourceFailed { error: String },
    /// Layer skipped due to budget constraints
    Skipped { reason: String },
}

impl LayerHealth {
    /// Check if the layer is healthy
    pub fn is_healthy(&self) -> bool {
        matches!(self, Self::Healthy)
    }

    /// Check if the layer failed
    pub fn is_failed(&self) -> bool {
        matches!(self, Self::ShaderFailed { .. } | Self::ResourceFailed { .. })
    }

    /// Check if the layer was skipped
    pub fn is_skipped(&self) -> bool {
        matches!(self, Self::Skipped { .. })
    }

    /// Get error message if failed
    pub fn error(&self) -> Option<&str> {
        match self {
            Self::ShaderFailed { error } | Self::ResourceFailed { error } => Some(error),
            _ => None,
        }
    }
}

/// Blend mode for layer composition
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BlendMode {
    /// Normal alpha blending: result = src * alpha + dst * (1 - alpha)
    Normal,
    /// Additive blending: result = src + dst
    Additive,
    /// Multiply: result = src * dst
    Multiply,
    /// Screen: result = 1 - (1 - src) * (1 - dst)
    Screen,
    /// Overlay: combines multiply and screen
    Overlay,
    /// Darken: result = min(src, dst)
    Darken,
    /// Lighten: result = max(src, dst)
    Lighten,
    /// Color dodge: result = dst / (1 - src)
    ColorDodge,
    /// Color burn: result = 1 - (1 - dst) / src
    ColorBurn,
    /// Hard light: like overlay but with src and dst swapped
    HardLight,
    /// Soft light: softer version of hard light
    SoftLight,
    /// Difference: result = abs(src - dst)
    Difference,
    /// Exclusion: result = src + dst - 2 * src * dst
    Exclusion,
    /// Replace (no blending)
    Replace,
}

impl Default for BlendMode {
    fn default() -> Self {
        Self::Normal
    }
}

impl BlendMode {
    /// Get WGSL shader code for this blend mode
    pub fn wgsl_code(&self) -> &'static str {
        match self {
            Self::Normal => "src.rgb * src.a + dst.rgb * (1.0 - src.a)",
            Self::Additive => "src.rgb + dst.rgb",
            Self::Multiply => "src.rgb * dst.rgb",
            Self::Screen => "vec3<f32>(1.0) - (vec3<f32>(1.0) - src.rgb) * (vec3<f32>(1.0) - dst.rgb)",
            Self::Overlay => {
                "mix(2.0 * src.rgb * dst.rgb, vec3<f32>(1.0) - 2.0 * (vec3<f32>(1.0) - src.rgb) * (vec3<f32>(1.0) - dst.rgb), step(0.5, dst.rgb))"
            }
            Self::Darken => "min(src.rgb, dst.rgb)",
            Self::Lighten => "max(src.rgb, dst.rgb)",
            Self::ColorDodge => "dst.rgb / (vec3<f32>(1.0) - src.rgb + vec3<f32>(0.001))",
            Self::ColorBurn => "vec3<f32>(1.0) - (vec3<f32>(1.0) - dst.rgb) / (src.rgb + vec3<f32>(0.001))",
            Self::HardLight => {
                "mix(2.0 * src.rgb * dst.rgb, vec3<f32>(1.0) - 2.0 * (vec3<f32>(1.0) - src.rgb) * (vec3<f32>(1.0) - dst.rgb), step(0.5, src.rgb))"
            }
            Self::SoftLight => {
                "mix(dst.rgb - (vec3<f32>(1.0) - 2.0 * src.rgb) * dst.rgb * (vec3<f32>(1.0) - dst.rgb), dst.rgb + (2.0 * src.rgb - vec3<f32>(1.0)) * (sqrt(dst.rgb) - dst.rgb), step(0.5, src.rgb))"
            }
            Self::Difference => "abs(src.rgb - dst.rgb)",
            Self::Exclusion => "src.rgb + dst.rgb - 2.0 * src.rgb * dst.rgb",
            Self::Replace => "src.rgb",
        }
    }

    /// Check if this blend mode needs the destination texture
    pub fn needs_destination(&self) -> bool {
        !matches!(self, Self::Replace)
    }
}

/// Viewport for layer rendering
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LayerViewport {
    /// X position in pixels (or normalized 0-1 if < 1.0)
    pub x: f32,
    /// Y position in pixels (or normalized 0-1 if < 1.0)
    pub y: f32,
    /// Width in pixels (or normalized 0-1 if < 1.0)
    pub width: f32,
    /// Height in pixels (or normalized 0-1 if < 1.0)
    pub height: f32,
}

impl LayerViewport {
    /// Create a fullscreen viewport
    pub fn fullscreen() -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            width: 1.0,
            height: 1.0,
        }
    }

    /// Create a viewport from pixel coordinates
    pub fn from_pixels(x: f32, y: f32, width: f32, height: f32) -> Self {
        Self { x, y, width, height }
    }

    /// Create a viewport from normalized coordinates (0-1)
    pub fn from_normalized(x: f32, y: f32, width: f32, height: f32) -> Self {
        Self { x, y, width, height }
    }

    /// Convert to pixel coordinates given output size
    pub fn to_pixels(&self, output_width: u32, output_height: u32) -> (u32, u32, u32, u32) {
        let x = if self.x < 1.0 {
            (self.x * output_width as f32) as u32
        } else {
            self.x as u32
        };

        let y = if self.y < 1.0 {
            (self.y * output_height as f32) as u32
        } else {
            self.y as u32
        };

        let width = if self.width <= 1.0 {
            (self.width * output_width as f32) as u32
        } else {
            self.width as u32
        };

        let height = if self.height <= 1.0 {
            (self.height * output_height as f32) as u32
        } else {
            self.height as u32
        };

        (x, y, width, height)
    }

    /// Check if this is a fullscreen viewport
    pub fn is_fullscreen(&self) -> bool {
        self.x == 0.0 && self.y == 0.0 && self.width == 1.0 && self.height == 1.0
    }
}

impl Default for LayerViewport {
    fn default() -> Self {
        Self::fullscreen()
    }
}

/// Layer configuration
#[derive(Clone, Debug)]
pub struct LayerConfig {
    /// Layer name (for debugging)
    pub name: String,
    /// Render priority (higher = rendered later / on top)
    pub z_order: i32,
    /// Opacity (0.0 = fully transparent, 1.0 = fully opaque)
    pub opacity: f32,
    /// Blend mode for composition
    pub blend_mode: BlendMode,
    /// Viewport (None = fullscreen)
    pub viewport: Option<LayerViewport>,
    /// Whether the layer is visible
    pub visible: bool,
    /// Clear color (None = transparent)
    pub clear_color: Option<[f32; 4]>,
    /// Render scale (1.0 = full resolution, 0.5 = half resolution)
    pub render_scale: f32,
    /// Whether to allocate a depth buffer
    pub use_depth: bool,
}

impl Default for LayerConfig {
    fn default() -> Self {
        Self {
            name: String::new(),
            z_order: 0,
            opacity: 1.0,
            blend_mode: BlendMode::Normal,
            viewport: None,
            visible: true,
            clear_color: None,
            render_scale: 1.0,
            use_depth: false,
        }
    }
}

/// A render layer
pub struct Layer {
    /// Layer ID
    id: LayerId,
    /// Configuration
    config: LayerConfig,
    /// Health status
    health: LayerHealth,
    /// Color render target
    color_target: Option<GraphHandle>,
    /// Depth render target
    depth_target: Option<GraphHandle>,
    /// Frame when layer was last rendered
    last_rendered_frame: u64,
    /// Whether the layer contents have changed
    dirty: bool,
}

impl Layer {
    /// Create a new layer
    pub fn new(id: LayerId, config: LayerConfig) -> Self {
        Self {
            id,
            config,
            health: LayerHealth::Healthy,
            color_target: None,
            depth_target: None,
            last_rendered_frame: 0,
            dirty: true,
        }
    }

    /// Get layer ID
    pub fn id(&self) -> LayerId {
        self.id
    }

    /// Get layer configuration
    pub fn config(&self) -> &LayerConfig {
        &self.config
    }

    /// Get mutable configuration
    pub fn config_mut(&mut self) -> &mut LayerConfig {
        self.dirty = true;
        &mut self.config
    }

    /// Get health status
    pub fn health(&self) -> &LayerHealth {
        &self.health
    }

    /// Set health status
    pub fn set_health(&mut self, health: LayerHealth) {
        self.health = health;
    }

    /// Get color target
    pub fn color_target(&self) -> Option<GraphHandle> {
        self.color_target
    }

    /// Set color target
    pub fn set_color_target(&mut self, target: GraphHandle) {
        self.color_target = Some(target);
    }

    /// Get depth target
    pub fn depth_target(&self) -> Option<GraphHandle> {
        self.depth_target
    }

    /// Set depth target
    pub fn set_depth_target(&mut self, target: GraphHandle) {
        self.depth_target = Some(target);
    }

    /// Check if layer is dirty
    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    /// Mark layer as dirty (needs re-render)
    pub fn mark_dirty(&mut self) {
        self.dirty = true;
    }

    /// Clear dirty flag
    pub fn clear_dirty(&mut self) {
        self.dirty = false;
    }

    /// Get last rendered frame
    pub fn last_rendered_frame(&self) -> u64 {
        self.last_rendered_frame
    }

    /// Set last rendered frame
    pub fn set_last_rendered_frame(&mut self, frame: u64) {
        self.last_rendered_frame = frame;
        self.dirty = false;
    }

    /// Create texture descriptor for color target
    pub fn color_target_desc(&self, output_width: u32, output_height: u32) -> TextureDesc {
        let (width, height) = if let Some(viewport) = &self.config.viewport {
            let (_, _, w, h) = viewport.to_pixels(output_width, output_height);
            (
                ((w as f32) * self.config.render_scale) as u32,
                ((h as f32) * self.config.render_scale) as u32,
            )
        } else {
            (
                ((output_width as f32) * self.config.render_scale) as u32,
                ((output_height as f32) * self.config.render_scale) as u32,
            )
        };

        TextureDesc {
            label: Some(alloc::format!("Layer {} Color", self.config.name)),
            size: [width.max(1), height.max(1), 1],
            mip_level_count: 1,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format: TextureFormat::Rgba8Unorm,
            usage: TextureUsage::RENDER_ATTACHMENT | TextureUsage::TEXTURE_BINDING,
        }
    }

    /// Create texture descriptor for depth target
    pub fn depth_target_desc(&self, output_width: u32, output_height: u32) -> TextureDesc {
        let (width, height) = if let Some(viewport) = &self.config.viewport {
            let (_, _, w, h) = viewport.to_pixels(output_width, output_height);
            (
                ((w as f32) * self.config.render_scale) as u32,
                ((h as f32) * self.config.render_scale) as u32,
            )
        } else {
            (
                ((output_width as f32) * self.config.render_scale) as u32,
                ((output_height as f32) * self.config.render_scale) as u32,
            )
        };

        TextureDesc {
            label: Some(alloc::format!("Layer {} Depth", self.config.name)),
            size: [width.max(1), height.max(1), 1],
            mip_level_count: 1,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format: TextureFormat::Depth24Plus,
            usage: TextureUsage::RENDER_ATTACHMENT,
        }
    }
}

/// Layer collection with sorting and filtering
pub struct LayerCollection {
    layers: Vec<Layer>,
    sort_dirty: bool,
}

impl LayerCollection {
    /// Create a new layer collection
    pub fn new() -> Self {
        Self {
            layers: Vec::new(),
            sort_dirty: false,
        }
    }

    /// Add a layer
    pub fn add(&mut self, layer: Layer) {
        self.layers.push(layer);
        self.sort_dirty = true;
    }

    /// Remove a layer by ID
    pub fn remove(&mut self, id: LayerId) -> Option<Layer> {
        if let Some(index) = self.layers.iter().position(|l| l.id == id) {
            Some(self.layers.remove(index))
        } else {
            None
        }
    }

    /// Get a layer by ID
    pub fn get(&self, id: LayerId) -> Option<&Layer> {
        self.layers.iter().find(|l| l.id == id)
    }

    /// Get a mutable layer by ID
    pub fn get_mut(&mut self, id: LayerId) -> Option<&mut Layer> {
        self.layers.iter_mut().find(|l| l.id == id)
    }

    /// Get all layers sorted by z-order
    pub fn sorted(&mut self) -> &[Layer] {
        if self.sort_dirty {
            self.layers.sort_by_key(|l| l.config.z_order);
            self.sort_dirty = false;
        }
        &self.layers
    }

    /// Get all visible layers sorted by z-order
    pub fn visible_sorted(&mut self) -> Vec<&Layer> {
        if self.sort_dirty {
            self.layers.sort_by_key(|l| l.config.z_order);
            self.sort_dirty = false;
        }
        self.layers
            .iter()
            .filter(|l| l.config.visible)
            .collect()
    }

    /// Get all mutable visible layers sorted by z-order
    pub fn visible_sorted_mut(&mut self) -> Vec<&mut Layer> {
        if self.sort_dirty {
            self.layers.sort_by_key(|l| l.config.z_order);
            self.sort_dirty = false;
        }
        self.layers
            .iter_mut()
            .filter(|l| l.config.visible)
            .collect()
    }

    /// Mark sort as dirty (call when z-order changes)
    pub fn mark_sort_dirty(&mut self) {
        self.sort_dirty = true;
    }

    /// Get layer count
    pub fn len(&self) -> usize {
        self.layers.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.layers.is_empty()
    }

    /// Clear all layers
    pub fn clear(&mut self) {
        self.layers.clear();
        self.sort_dirty = false;
    }
}

impl Default for LayerCollection {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_layer_health() {
        let health = LayerHealth::Healthy;
        assert!(health.is_healthy());
        assert!(!health.is_failed());

        let failed = LayerHealth::ShaderFailed {
            error: "compile error".into(),
        };
        assert!(!failed.is_healthy());
        assert!(failed.is_failed());
        assert_eq!(failed.error(), Some("compile error"));
    }

    #[test]
    fn test_blend_mode_wgsl() {
        let code = BlendMode::Normal.wgsl_code();
        assert!(code.contains("src.a"));

        let additive = BlendMode::Additive.wgsl_code();
        assert!(additive.contains("+"));
    }

    #[test]
    fn test_viewport() {
        let fullscreen = LayerViewport::fullscreen();
        assert!(fullscreen.is_fullscreen());

        let (x, y, w, h) = fullscreen.to_pixels(1920, 1080);
        assert_eq!((x, y, w, h), (0, 0, 1920, 1080));

        let windowed = LayerViewport::from_pixels(100.0, 100.0, 800.0, 600.0);
        assert!(!windowed.is_fullscreen());
    }

    #[test]
    fn test_layer_collection_sorting() {
        let mut collection = LayerCollection::new();

        let mut config_high = LayerConfig::default();
        config_high.z_order = 100;
        collection.add(Layer::new(LayerId::new(), config_high));

        let mut config_low = LayerConfig::default();
        config_low.z_order = 0;
        collection.add(Layer::new(LayerId::new(), config_low));

        let mut config_mid = LayerConfig::default();
        config_mid.z_order = 50;
        collection.add(Layer::new(LayerId::new(), config_mid));

        let sorted = collection.sorted();
        assert_eq!(sorted[0].config.z_order, 0);
        assert_eq!(sorted[1].config.z_order, 50);
        assert_eq!(sorted[2].config.z_order, 100);
    }

    #[test]
    fn test_layer_dirty_tracking() {
        let config = LayerConfig::default();
        let mut layer = Layer::new(LayerId::new(), config);

        assert!(layer.is_dirty());

        layer.clear_dirty();
        assert!(!layer.is_dirty());

        layer.mark_dirty();
        assert!(layer.is_dirty());
    }
}
