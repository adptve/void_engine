//! Compositor - Layer composition with shader failure isolation
//!
//! The compositor provides:
//! - Layer-based composition with blend modes
//! - Shader failure isolation (failed shader = black layer, NOT crashed frame)
//! - Dynamic renderer switching at runtime
//! - Post-processing effects
//! - Performance optimizations (dirty tracking, layer caching)
//!
//! ## Core Principle
//!
//! Shaders NEVER compose with shaders. Only shader OUTPUTS compose.
//! This ensures that a failed shader in one layer doesn't crash the entire frame.

use crate::graph::{RenderGraph, GraphHandle, PassId};
use crate::resource::{TextureFormat, TextureDesc, TextureUsage, TextureDimension};
use crate::layer::{Layer as RenderLayer, LayerId, LayerConfig, LayerHealth, BlendMode, LayerViewport, LayerCollection};
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::format;
use alloc::string::String;
use alloc::vec::Vec;
use core::any::Any;
use void_core::Id;

// Import layer types (not re-exported to avoid conflicts with lib.rs)
use crate::layer::Layer as RenderLayerInternal;
// Re-export only Viewport alias since it's useful
pub use crate::layer::LayerViewport as Viewport;

/// Trait for renderers that can contribute to the compositor
pub trait CompositorRenderer: Send + Sync {
    /// Get the renderer ID
    fn id(&self) -> RendererId;

    /// Get the renderer name
    fn name(&self) -> &str;

    /// Initialize the renderer
    fn initialize(&mut self, context: &mut RendererContext) -> Result<(), String>;

    /// Shutdown the renderer
    fn shutdown(&mut self, context: &mut RendererContext);

    /// Render to a layer
    fn render(&self, context: &mut RendererContext, layer: &Layer);

    /// Handle resize
    fn resize(&mut self, context: &mut RendererContext, width: u32, height: u32);

    /// Get supported features
    fn features(&self) -> RendererFeatures {
        RendererFeatures::default()
    }
}

/// Unique identifier for a renderer
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct RendererId(pub Id);

impl RendererId {
    pub fn from_name(name: &str) -> Self {
        Self(Id::from_name(name))
    }
}

/// Renderer feature flags
#[derive(Clone, Copy, Debug, Default)]
pub struct RendererFeatures {
    pub supports_transparency: bool,
    pub supports_hdr: bool,
    pub supports_shadows: bool,
    pub supports_reflections: bool,
    pub supports_post_processing: bool,
    pub supports_instancing: bool,
    pub supports_compute: bool,
}

/// Context for renderer operations
pub struct RendererContext<'a> {
    /// The compositor
    pub compositor: &'a Compositor,
    /// Backend-specific data
    pub backend_data: &'a mut dyn Any,
    /// Current frame index
    pub frame_index: u64,
    /// Delta time
    pub delta_time: f32,
}

/// A compositor layer (wraps the core RenderLayer with renderer assignment)
pub struct Layer {
    /// Core layer data
    core: RenderLayerInternal,
    /// Assigned renderer
    renderer_id: Option<RendererId>,
}

impl Layer {
    /// Create a new layer
    pub fn new(id: LayerId, config: LayerConfig) -> Self {
        Self {
            core: RenderLayerInternal::new(id, config),
            renderer_id: None,
        }
    }

    /// Get the layer ID
    pub fn id(&self) -> LayerId {
        self.core.id()
    }

    /// Get the layer configuration
    pub fn config(&self) -> &LayerConfig {
        self.core.config()
    }

    /// Get mutable configuration
    pub fn config_mut(&mut self) -> &mut LayerConfig {
        self.core.config_mut()
    }

    /// Get layer health
    pub fn health(&self) -> &LayerHealth {
        self.core.health()
    }

    /// Set layer health
    pub fn set_health(&mut self, health: LayerHealth) {
        self.core.set_health(health);
    }

    /// Get the assigned renderer
    pub fn renderer(&self) -> Option<RendererId> {
        self.renderer_id
    }

    /// Assign a renderer
    pub fn set_renderer(&mut self, renderer: RendererId) {
        self.renderer_id = Some(renderer);
    }

    /// Clear the assigned renderer
    pub fn clear_renderer(&mut self) {
        self.renderer_id = None;
    }

    /// Get render target
    pub fn render_target(&self) -> Option<GraphHandle> {
        self.core.color_target()
    }

    /// Set render target
    pub fn set_render_target(&mut self, target: GraphHandle) {
        self.core.set_color_target(target);
    }

    /// Get depth target
    pub fn depth_target(&self) -> Option<GraphHandle> {
        self.core.depth_target()
    }

    /// Set depth target
    pub fn set_depth_target(&mut self, target: GraphHandle) {
        self.core.set_depth_target(target);
    }

    /// Mark layer as dirty
    pub fn mark_dirty(&mut self) {
        self.core.mark_dirty();
    }

    /// Check if layer is dirty
    pub fn is_dirty(&self) -> bool {
        self.core.is_dirty()
    }

    /// Get core layer (for direct access)
    pub fn core(&self) -> &RenderLayerInternal {
        &self.core
    }

    /// Get mutable core layer
    pub fn core_mut(&mut self) -> &mut RenderLayerInternal {
        &mut self.core
    }
}

/// Post-processing effect
pub trait PostProcess: Send + Sync {
    /// Get effect name
    fn name(&self) -> &str;

    /// Apply the effect
    fn apply(&self, context: &mut PostProcessContext);

    /// Get required inputs
    fn inputs(&self) -> &[&str];

    /// Get outputs
    fn outputs(&self) -> &[&str];
}

/// Context for post-processing
pub struct PostProcessContext<'a> {
    /// Input textures
    pub inputs: &'a BTreeMap<String, GraphHandle>,
    /// Output textures
    pub outputs: &'a mut BTreeMap<String, GraphHandle>,
    /// Backend data
    pub backend_data: &'a mut dyn Any,
}

/// The main compositor
pub struct Compositor {
    /// Layers
    layers: BTreeMap<LayerId, Layer>,
    /// Registered renderers
    renderers: BTreeMap<RendererId, Box<dyn CompositorRenderer>>,
    /// Post-processing effects
    post_effects: Vec<Box<dyn PostProcess>>,
    /// Active renderer (for quick switching)
    active_renderer: Option<RendererId>,
    /// Fallback renderer
    fallback_renderer: Option<RendererId>,
    /// Compositor render graph
    graph: RenderGraph,
    /// Output format
    output_format: TextureFormat,
    /// Output size
    output_size: [u32; 2],
    /// HDR enabled
    hdr_enabled: bool,
    /// Frame index
    frame_index: u64,
}

impl Compositor {
    /// Create a new compositor
    pub fn new() -> Self {
        Self {
            layers: BTreeMap::new(),
            renderers: BTreeMap::new(),
            post_effects: Vec::new(),
            active_renderer: None,
            fallback_renderer: None,
            graph: RenderGraph::new(),
            output_format: TextureFormat::Bgra8UnormSrgb,
            output_size: [1920, 1080],
            hdr_enabled: false,
            frame_index: 0,
        }
    }

    /// Set output size
    pub fn set_output_size(&mut self, width: u32, height: u32) {
        self.output_size = [width, height];
        self.graph.set_viewport_size(width, height);
    }

    /// Get output size
    pub fn output_size(&self) -> [u32; 2] {
        self.output_size
    }

    /// Set output format
    pub fn set_output_format(&mut self, format: TextureFormat) {
        self.output_format = format;
    }

    /// Enable/disable HDR
    pub fn set_hdr(&mut self, enabled: bool) {
        self.hdr_enabled = enabled;
        if enabled {
            self.output_format = TextureFormat::Rgba16Float;
        } else {
            self.output_format = TextureFormat::Bgra8UnormSrgb;
        }
    }

    /// Create a new layer
    pub fn create_layer(&mut self, name: &str, config: LayerConfig) -> LayerId {
        let id = LayerId::from_name(name);
        let mut layer_config = config;
        layer_config.name = name.to_string();

        let layer = Layer::new(id, layer_config);
        self.layers.insert(id, layer);
        id
    }

    /// Get a layer
    pub fn layer(&self, id: LayerId) -> Option<&Layer> {
        self.layers.get(&id)
    }

    /// Get a mutable layer
    pub fn layer_mut(&mut self, id: LayerId) -> Option<&mut Layer> {
        self.layers.get_mut(&id)
    }

    /// Remove a layer
    pub fn remove_layer(&mut self, id: LayerId) -> bool {
        self.layers.remove(&id).is_some()
    }

    /// Register a renderer
    pub fn register_renderer<R: CompositorRenderer + 'static>(&mut self, renderer: R) {
        let id = renderer.id();
        self.renderers.insert(id, Box::new(renderer));

        // Set as active if it's the first one
        if self.active_renderer.is_none() {
            self.active_renderer = Some(id);
        }
    }

    /// Get a renderer
    pub fn renderer(&self, id: RendererId) -> Option<&dyn CompositorRenderer> {
        self.renderers.get(&id).map(|r| r.as_ref())
    }

    /// Set the active renderer
    pub fn set_active_renderer(&mut self, id: RendererId) -> bool {
        if self.renderers.contains_key(&id) {
            self.active_renderer = Some(id);
            true
        } else {
            false
        }
    }

    /// Get the active renderer
    pub fn active_renderer(&self) -> Option<RendererId> {
        self.active_renderer
    }

    /// Set fallback renderer
    pub fn set_fallback_renderer(&mut self, id: RendererId) {
        if self.renderers.contains_key(&id) {
            self.fallback_renderer = Some(id);
        }
    }

    /// Switch renderer at runtime
    pub fn switch_renderer(&mut self, id: RendererId) -> Result<(), String> {
        if !self.renderers.contains_key(&id) {
            return Err(format!("Renderer {:?} not registered", id));
        }

        self.active_renderer = Some(id);
        Ok(())
    }

    /// Assign a renderer to a layer
    pub fn assign_renderer_to_layer(&mut self, layer: LayerId, renderer: RendererId) -> bool {
        if !self.renderers.contains_key(&renderer) {
            return false;
        }

        if let Some(l) = self.layers.get_mut(&layer) {
            l.set_renderer(renderer);
            true
        } else {
            false
        }
    }

    /// Add a post-processing effect
    pub fn add_post_effect<P: PostProcess + 'static>(&mut self, effect: P) {
        self.post_effects.push(Box::new(effect));
    }

    /// Clear all post-processing effects
    pub fn clear_post_effects(&mut self) {
        self.post_effects.clear();
    }

    /// Prepare the frame
    pub fn begin_frame(&mut self) {
        self.frame_index += 1;
        self.graph.clear();
    }

    /// Build the render graph for this frame
    pub fn build_graph(&mut self) -> &RenderGraph {
        // Sort layers by z_order
        let mut sorted_layers: Vec<_> = self.layers.values().collect();
        sorted_layers.sort_by_key(|l| l.config().z_order);

        // Create render targets for each layer
        for (i, layer) in sorted_layers.iter().enumerate() {
            if !layer.config().visible {
                continue;
            }

            let color_format = if self.hdr_enabled {
                TextureFormat::Rgba16Float
            } else {
                TextureFormat::Rgba8Unorm
            };

            // Create layer render target
            let _color_target = self.graph.create_texture(
                &format!("layer_{}_color", i),
                TextureDesc {
                    label: Some(format!("Layer {} Color", layer.config().name)),
                    size: [self.output_size[0], self.output_size[1], 1],
                    mip_level_count: 1,
                    sample_count: 1,
                    dimension: TextureDimension::D2,
                    format: color_format,
                    usage: TextureUsage::RENDER_ATTACHMENT | TextureUsage::TEXTURE_BINDING,
                },
            );

            let _depth_target = self.graph.create_texture(
                &format!("layer_{}_depth", i),
                TextureDesc {
                    label: Some(format!("Layer {} Depth", layer.config().name)),
                    size: [self.output_size[0], self.output_size[1], 1],
                    mip_level_count: 1,
                    sample_count: 1,
                    dimension: TextureDimension::D2,
                    format: TextureFormat::Depth24Plus,
                    usage: TextureUsage::RENDER_ATTACHMENT | TextureUsage::TEXTURE_BINDING,
                },
            );
        }

        // Compile the graph
        let _ = self.graph.compile();

        &self.graph
    }

    /// Execute the compositor
    pub fn execute(&self, backend_data: &mut dyn Any) {
        self.graph.execute(backend_data);
    }

    /// Composite all layers into the final output
    ///
    /// This is the core composition pipeline:
    /// 1. Collect visible layers sorted by z-order
    /// 2. For each layer:
    ///    - If healthy: composite normally
    ///    - If shader failed: render black rectangle
    ///    - If resource failed: skip
    ///    - If skipped: skip
    /// 3. Apply blend modes and opacity
    /// 4. Respect viewports
    pub fn composite_layers(&mut self) -> Result<(), String> {
        // Sort layers by z-order (priority)
        let mut sorted_layers: Vec<_> = self.layers.values().collect();
        sorted_layers.sort_by_key(|l| l.config().z_order);

        // Filter to visible layers
        let visible_layers: Vec<_> = sorted_layers
            .into_iter()
            .filter(|l| l.config().visible)
            .collect();

        if visible_layers.is_empty() {
            return Ok(());
        }

        // Create composition target if needed
        let composite_target = self.graph.create_texture(
            "composite_output",
            TextureDesc {
                label: Some("Composite Output".into()),
                size: [self.output_size[0], self.output_size[1], 1],
                mip_level_count: 1,
                sample_count: 1,
                dimension: TextureDimension::D2,
                format: self.output_format,
                usage: TextureUsage::RENDER_ATTACHMENT | TextureUsage::TEXTURE_BINDING,
            },
        );

        // Composite each layer
        for layer in visible_layers {
            match layer.health() {
                LayerHealth::Healthy => {
                    self.composite_layer_normal(layer, composite_target)?;
                }
                LayerHealth::ShaderFailed { error } => {
                    log::warn!(
                        "Layer {} shader failed: {}. Rendering black.",
                        layer.config().name,
                        error
                    );
                    self.composite_layer_black(layer, composite_target)?;
                }
                LayerHealth::ResourceFailed { error } => {
                    log::warn!(
                        "Layer {} resource failed: {}. Skipping.",
                        layer.config().name,
                        error
                    );
                    // Skip this layer
                    continue;
                }
                LayerHealth::Skipped { reason } => {
                    log::debug!(
                        "Layer {} skipped: {}",
                        layer.config().name,
                        reason
                    );
                    continue;
                }
            }
        }

        Ok(())
    }

    /// Composite a healthy layer normally
    fn composite_layer_normal(&self, layer: &Layer, target: GraphHandle) -> Result<(), String> {
        let source = layer.render_target()
            .ok_or_else(|| format!("Layer {} has no render target", layer.config().name))?;

        // Get blend mode and opacity
        let blend_mode = layer.config().blend_mode;
        let opacity = layer.config().opacity;

        // Apply viewport transformation if needed
        let _viewport = layer.config().viewport;

        // TODO: Create render pass to composite this layer onto the target
        // This would use the blend shader for the given blend mode
        // For now, this is a placeholder that would be filled in with actual GPU commands

        log::trace!(
            "Compositing layer {} with blend mode {:?}, opacity {}",
            layer.config().name,
            blend_mode,
            opacity
        );

        Ok(())
    }

    /// Composite a failed layer as black
    fn composite_layer_black(&self, layer: &Layer, target: GraphHandle) -> Result<(), String> {
        let _viewport = layer.config().viewport;

        // TODO: Create render pass to draw a black quad
        // This ensures the failed shader doesn't crash the frame

        log::trace!(
            "Compositing layer {} as black (shader failed)",
            layer.config().name
        );

        Ok(())
    }

    /// Collect layers that need re-rendering (dirty layers)
    pub fn collect_dirty_layers(&self) -> Vec<LayerId> {
        self.layers
            .values()
            .filter(|l| l.is_dirty())
            .map(|l| l.id())
            .collect()
    }

    /// Mark layers as rendered for a frame
    pub fn mark_layers_rendered(&mut self, layer_ids: &[LayerId]) {
        for id in layer_ids {
            if let Some(layer) = self.layers.get_mut(id) {
                layer.core_mut().set_last_rendered_frame(self.frame_index);
            }
        }
    }

    /// Attempt to recover failed layers
    ///
    /// This could involve recompiling shaders, reallocating resources, etc.
    pub fn attempt_layer_recovery(&mut self) {
        for layer in self.layers.values_mut() {
            match layer.health() {
                LayerHealth::ShaderFailed { .. } => {
                    // Try to recover by marking as healthy
                    // In a real implementation, this would try to reload the shader
                    log::info!("Attempting to recover layer {}", layer.config().name);
                    // layer.set_health(LayerHealth::Healthy);
                }
                LayerHealth::ResourceFailed { .. } => {
                    // Try to reallocate resources
                    log::info!("Attempting to reallocate resources for layer {}", layer.config().name);
                    // In a real implementation, this would try to allocate GPU memory
                }
                _ => {}
            }
        }
    }

    /// End the frame
    pub fn end_frame(&mut self) {
        // Mark all visible layers as rendered
        let visible_ids: Vec<_> = self.layers
            .values()
            .filter(|l| l.config().visible)
            .map(|l| l.id())
            .collect();

        self.mark_layers_rendered(&visible_ids);
    }

    /// Get render graph
    pub fn graph(&self) -> &RenderGraph {
        &self.graph
    }

    /// Get mutable render graph
    pub fn graph_mut(&mut self) -> &mut RenderGraph {
        &mut self.graph
    }

    /// Get frame index
    pub fn frame_index(&self) -> u64 {
        self.frame_index
    }

    /// Get layer count
    pub fn layer_count(&self) -> usize {
        self.layers.len()
    }

    /// Get renderer count
    pub fn renderer_count(&self) -> usize {
        self.renderers.len()
    }

    /// Iterate over layers (sorted by z-order)
    pub fn layers_sorted(&self) -> Vec<&Layer> {
        let mut layers: Vec<_> = self.layers.values().collect();
        layers.sort_by_key(|l| l.config().z_order);
        layers
    }
}

impl Default for Compositor {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct DummyRenderer {
        id: RendererId,
        name: String,
    }

    impl DummyRenderer {
        fn new(name: &str) -> Self {
            Self {
                id: RendererId::from_name(name),
                name: name.to_string(),
            }
        }
    }

    impl CompositorRenderer for DummyRenderer {
        fn id(&self) -> RendererId {
            self.id
        }

        fn name(&self) -> &str {
            &self.name
        }

        fn initialize(&mut self, _context: &mut RendererContext) -> Result<(), String> {
            Ok(())
        }

        fn shutdown(&mut self, _context: &mut RendererContext) {}

        fn render(&self, _context: &mut RendererContext, _layer: &Layer) {}

        fn resize(&mut self, _context: &mut RendererContext, _width: u32, _height: u32) {}
    }

    #[test]
    fn test_compositor() {
        let mut compositor = Compositor::new();

        // Register renderers
        compositor.register_renderer(DummyRenderer::new("forward"));
        compositor.register_renderer(DummyRenderer::new("deferred"));

        assert_eq!(compositor.renderer_count(), 2);

        // Create layers
        let mut config_bg = LayerConfig::default();
        config_bg.name = "background".into();
        config_bg.z_order = 0;
        let _background = compositor.create_layer("background", config_bg);

        let mut config_main = LayerConfig::default();
        config_main.name = "main".into();
        config_main.z_order = 10;
        let _main = compositor.create_layer("main", config_main);

        let mut config_ui = LayerConfig::default();
        config_ui.name = "ui".into();
        config_ui.z_order = 100;
        config_ui.blend_mode = BlendMode::Normal;
        let _ui = compositor.create_layer("ui", config_ui);

        assert_eq!(compositor.layer_count(), 3);

        // Switch renderer
        let result = compositor.switch_renderer(RendererId::from_name("deferred"));
        assert!(result.is_ok());
    }
}
