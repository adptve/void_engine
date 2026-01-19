//! Layer management for isolation and composition
//!
//! Layers are the primary safety mechanism in Void Engine.
//! Each app renders to its own layers, and the kernel composites them.

use void_ir::patch::{BlendMode, LayerType};
use void_ir::NamespaceId;
use parking_lot::RwLock;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};

/// Unique identifier for a layer
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct LayerId(u64);

impl LayerId {
    /// Create a new unique layer ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for LayerId {
    fn default() -> Self {
        Self::new()
    }
}

/// Configuration for a layer
#[derive(Debug, Clone)]
pub struct LayerConfig {
    /// Layer type
    pub layer_type: LayerType,
    /// Render priority (higher = rendered later / on top)
    pub priority: i32,
    /// Blend mode for composition
    pub blend_mode: BlendMode,
    /// Whether the layer is visible
    pub visible: bool,
    /// Optional clear color (None = transparent)
    pub clear_color: Option<[f32; 4]>,
    /// Whether to use depth buffer
    pub use_depth: bool,
    /// Render scale (1.0 = full resolution)
    pub render_scale: f32,
}

impl Default for LayerConfig {
    fn default() -> Self {
        Self {
            layer_type: LayerType::Content,
            priority: 0,
            blend_mode: BlendMode::Normal,
            visible: true,
            clear_color: None,
            use_depth: true,
            render_scale: 1.0,
        }
    }
}

impl LayerConfig {
    /// Create a content layer config
    pub fn content(priority: i32) -> Self {
        Self {
            layer_type: LayerType::Content,
            priority,
            use_depth: true,
            ..Default::default()
        }
    }

    /// Create an effect layer config
    pub fn effect(priority: i32) -> Self {
        Self {
            layer_type: LayerType::Effect,
            priority,
            use_depth: false,
            ..Default::default()
        }
    }

    /// Create an overlay layer config
    pub fn overlay(priority: i32) -> Self {
        Self {
            layer_type: LayerType::Overlay,
            priority,
            use_depth: false,
            ..Default::default()
        }
    }

    /// Create a portal layer config
    pub fn portal(priority: i32) -> Self {
        Self {
            layer_type: LayerType::Portal,
            priority,
            use_depth: true,
            ..Default::default()
        }
    }
}

/// A render layer
#[derive(Debug)]
pub struct Layer {
    /// Unique identifier
    pub id: LayerId,
    /// Human-readable name
    pub name: String,
    /// Owning namespace
    pub owner: NamespaceId,
    /// Configuration
    pub config: LayerConfig,
    /// Whether the layer has pending changes
    pub dirty: bool,
    /// Frame when layer was last rendered
    pub last_rendered_frame: u64,
    // GPU resources would be stored here
    // pub color_target: Option<TextureHandle>,
    // pub depth_target: Option<TextureHandle>,
}

impl Layer {
    /// Create a new layer
    pub fn new(name: impl Into<String>, owner: NamespaceId, config: LayerConfig) -> Self {
        Self {
            id: LayerId::new(),
            name: name.into(),
            owner,
            config,
            dirty: true,
            last_rendered_frame: 0,
        }
    }

    /// Mark the layer as dirty (needs re-render)
    pub fn mark_dirty(&mut self) {
        self.dirty = true;
    }

    /// Clear the dirty flag
    pub fn clear_dirty(&mut self) {
        self.dirty = false;
    }

    /// Update visibility
    pub fn set_visible(&mut self, visible: bool) {
        if self.config.visible != visible {
            self.config.visible = visible;
            self.dirty = true;
        }
    }

    /// Update priority
    pub fn set_priority(&mut self, priority: i32) {
        if self.config.priority != priority {
            self.config.priority = priority;
            self.dirty = true;
        }
    }
}

/// Manages all layers
pub struct LayerManager {
    /// All registered layers
    layers: HashMap<LayerId, Layer>,
    /// Layers sorted by priority (cached)
    sorted_layers: Vec<LayerId>,
    /// Whether sort is dirty
    sort_dirty: bool,
    /// Maximum number of layers
    max_layers: usize,
    /// Layers by namespace (for isolation queries)
    by_namespace: HashMap<NamespaceId, Vec<LayerId>>,
}

impl LayerManager {
    /// Create a new layer manager
    pub fn new(max_layers: usize) -> Self {
        Self {
            layers: HashMap::new(),
            sorted_layers: Vec::new(),
            sort_dirty: false,
            max_layers,
            by_namespace: HashMap::new(),
        }
    }

    /// Create a new layer
    pub fn create_layer(
        &mut self,
        name: impl Into<String>,
        owner: NamespaceId,
        config: LayerConfig,
    ) -> Result<LayerId, LayerError> {
        if self.layers.len() >= self.max_layers {
            return Err(LayerError::TooManyLayers);
        }

        let layer = Layer::new(name, owner, config);
        let id = layer.id;

        self.layers.insert(id, layer);
        self.by_namespace.entry(owner).or_default().push(id);
        self.sort_dirty = true;

        log::debug!("Created layer {:?} for namespace {}", id, owner);
        Ok(id)
    }

    /// Destroy a layer
    pub fn destroy_layer(&mut self, id: LayerId) -> Result<(), LayerError> {
        if let Some(layer) = self.layers.remove(&id) {
            // Remove from namespace index
            if let Some(ns_layers) = self.by_namespace.get_mut(&layer.owner) {
                ns_layers.retain(|&lid| lid != id);
            }
            self.sorted_layers.retain(|&lid| lid != id);
            log::debug!("Destroyed layer {:?}", id);
            Ok(())
        } else {
            Err(LayerError::NotFound(id))
        }
    }

    /// Get a layer by ID
    pub fn get(&self, id: LayerId) -> Option<&Layer> {
        self.layers.get(&id)
    }

    /// Get a mutable layer by ID
    pub fn get_mut(&mut self, id: LayerId) -> Option<&mut Layer> {
        self.layers.get_mut(&id)
    }

    /// Get all layers for a namespace
    pub fn get_namespace_layers(&self, namespace: NamespaceId) -> Vec<LayerId> {
        self.by_namespace.get(&namespace).cloned().unwrap_or_default()
    }

    /// Destroy all layers owned by a namespace
    pub fn destroy_namespace_layers(&mut self, namespace: NamespaceId) {
        if let Some(layer_ids) = self.by_namespace.remove(&namespace) {
            for id in layer_ids {
                self.layers.remove(&id);
                self.sorted_layers.retain(|&lid| lid != id);
            }
            log::debug!("Destroyed all layers for namespace {}", namespace);
        }
    }

    /// Get visible layers in render order
    pub fn collect_visible(&mut self) -> Vec<LayerId> {
        // Re-sort if needed
        if self.sort_dirty {
            self.sorted_layers = self.layers.keys().copied().collect();
            self.sorted_layers.sort_by(|a, b| {
                let layer_a = self.layers.get(a).unwrap();
                let layer_b = self.layers.get(b).unwrap();
                layer_a.config.priority.cmp(&layer_b.config.priority)
            });
            self.sort_dirty = false;
        }

        // Filter to visible layers
        self.sorted_layers
            .iter()
            .copied()
            .filter(|id| {
                self.layers
                    .get(id)
                    .map(|l| l.config.visible)
                    .unwrap_or(false)
            })
            .collect()
    }

    /// Get all dirty layers
    pub fn collect_dirty(&self) -> Vec<LayerId> {
        self.layers
            .iter()
            .filter(|(_, l)| l.dirty && l.config.visible)
            .map(|(id, _)| *id)
            .collect()
    }

    /// Mark frame rendered for layers
    pub fn mark_rendered(&mut self, frame: u64, layer_ids: &[LayerId]) {
        for id in layer_ids {
            if let Some(layer) = self.layers.get_mut(id) {
                layer.last_rendered_frame = frame;
                layer.clear_dirty();
            }
        }
    }

    /// Get layer count
    pub fn len(&self) -> usize {
        self.layers.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.layers.is_empty()
    }

    /// Get all layer IDs
    pub fn all_ids(&self) -> Vec<LayerId> {
        self.layers.keys().copied().collect()
    }
}

/// Errors from layer operations
#[derive(Debug, Clone)]
pub enum LayerError {
    /// Too many layers
    TooManyLayers,
    /// Layer not found
    NotFound(LayerId),
    /// Permission denied
    PermissionDenied,
}

impl std::fmt::Display for LayerError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TooManyLayers => write!(f, "Maximum layer count exceeded"),
            Self::NotFound(id) => write!(f, "Layer {:?} not found", id),
            Self::PermissionDenied => write!(f, "Permission denied"),
        }
    }
}

impl std::error::Error for LayerError {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_layer_creation() {
        let mut manager = LayerManager::new(10);
        let ns = NamespaceId::new();

        let id = manager
            .create_layer("test", ns, LayerConfig::content(0))
            .unwrap();

        assert!(manager.get(id).is_some());
        assert_eq!(manager.len(), 1);
    }

    #[test]
    fn test_layer_ordering() {
        let mut manager = LayerManager::new(10);
        let ns = NamespaceId::new();

        let low = manager
            .create_layer("low", ns, LayerConfig::content(-1))
            .unwrap();
        let high = manager
            .create_layer("high", ns, LayerConfig::content(1))
            .unwrap();
        let mid = manager
            .create_layer("mid", ns, LayerConfig::content(0))
            .unwrap();

        let visible = manager.collect_visible();
        assert_eq!(visible, vec![low, mid, high]);
    }

    #[test]
    fn test_namespace_isolation() {
        let mut manager = LayerManager::new(10);
        let ns1 = NamespaceId::new();
        let ns2 = NamespaceId::new();

        let l1 = manager
            .create_layer("ns1_layer", ns1, LayerConfig::content(0))
            .unwrap();
        let l2 = manager
            .create_layer("ns2_layer", ns2, LayerConfig::content(0))
            .unwrap();

        assert_eq!(manager.get_namespace_layers(ns1), vec![l1]);
        assert_eq!(manager.get_namespace_layers(ns2), vec![l2]);

        // Destroy ns1's layers
        manager.destroy_namespace_layers(ns1);
        assert!(manager.get(l1).is_none());
        assert!(manager.get(l2).is_some());
    }
}
