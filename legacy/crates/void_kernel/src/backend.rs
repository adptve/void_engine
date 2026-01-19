//! Backend selection for runtime GPU API switching
//!
//! The kernel can select between Vulkan, WebGPU, and WebGL backends
//! on a per-frame basis, enabling graceful fallback and platform adaptation.

use std::collections::HashMap;

/// Supported rendering backends
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Backend {
    /// Vulkan (desktop, high performance)
    Vulkan,
    /// WebGPU (cross-platform, modern)
    WebGPU,
    /// WebGL2 (browser fallback)
    WebGL2,
    /// Software renderer (fallback)
    Software,
}

impl Default for Backend {
    fn default() -> Self {
        Self::WebGPU
    }
}

impl std::fmt::Display for Backend {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Vulkan => write!(f, "Vulkan"),
            Self::WebGPU => write!(f, "WebGPU"),
            Self::WebGL2 => write!(f, "WebGL2"),
            Self::Software => write!(f, "Software"),
        }
    }
}

/// Capabilities of a backend
#[derive(Debug, Clone, Default)]
pub struct BackendCapabilities {
    /// Maximum texture dimension
    pub max_texture_size: u32,
    /// Maximum uniform buffer size
    pub max_uniform_buffer_size: u32,
    /// Maximum storage buffer size
    pub max_storage_buffer_size: u32,
    /// Supports compute shaders
    pub compute_shaders: bool,
    /// Supports geometry shaders
    pub geometry_shaders: bool,
    /// Supports tessellation
    pub tessellation: bool,
    /// Supports ray tracing
    pub ray_tracing: bool,
    /// Supports bindless resources
    pub bindless: bool,
    /// Maximum MSAA samples
    pub max_msaa_samples: u32,
    /// Supports depth clamp
    pub depth_clamp: bool,
}

impl BackendCapabilities {
    /// Create capabilities for Vulkan
    pub fn vulkan() -> Self {
        Self {
            max_texture_size: 16384,
            max_uniform_buffer_size: 65536,
            max_storage_buffer_size: 128 * 1024 * 1024,
            compute_shaders: true,
            geometry_shaders: true,
            tessellation: true,
            ray_tracing: false, // Depends on hardware
            bindless: true,
            max_msaa_samples: 8,
            depth_clamp: true,
        }
    }

    /// Create capabilities for WebGPU
    pub fn webgpu() -> Self {
        Self {
            max_texture_size: 8192,
            max_uniform_buffer_size: 65536,
            max_storage_buffer_size: 128 * 1024 * 1024,
            compute_shaders: true,
            geometry_shaders: false,
            tessellation: false,
            ray_tracing: false,
            bindless: false,
            max_msaa_samples: 4,
            depth_clamp: false,
        }
    }

    /// Create capabilities for WebGL2
    pub fn webgl2() -> Self {
        Self {
            max_texture_size: 4096,
            max_uniform_buffer_size: 16384,
            max_storage_buffer_size: 0,
            compute_shaders: false,
            geometry_shaders: false,
            tessellation: false,
            ray_tracing: false,
            bindless: false,
            max_msaa_samples: 4,
            depth_clamp: false,
        }
    }

    /// Create minimal capabilities for software
    pub fn software() -> Self {
        Self {
            max_texture_size: 2048,
            max_uniform_buffer_size: 16384,
            max_storage_buffer_size: 0,
            compute_shaders: false,
            geometry_shaders: false,
            tessellation: false,
            ray_tracing: false,
            bindless: false,
            max_msaa_samples: 1,
            depth_clamp: false,
        }
    }

    /// Check if a required capability is supported
    pub fn supports(&self, required: &BackendCapabilities) -> bool {
        self.max_texture_size >= required.max_texture_size
            && self.max_uniform_buffer_size >= required.max_uniform_buffer_size
            && (!required.compute_shaders || self.compute_shaders)
            && (!required.geometry_shaders || self.geometry_shaders)
            && (!required.tessellation || self.tessellation)
            && (!required.ray_tracing || self.ray_tracing)
    }
}

/// Backend selection strategy
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SelectionStrategy {
    /// Use the best available backend
    BestAvailable,
    /// Prefer a specific backend, fallback if unavailable
    Prefer(Backend),
    /// Force a specific backend (fail if unavailable)
    Force(Backend),
    /// Select based on capabilities needed
    ByCapabilities,
}

impl Default for SelectionStrategy {
    fn default() -> Self {
        Self::BestAvailable
    }
}

/// Backend selector - chooses and manages rendering backends
pub struct BackendSelector {
    /// Current active backend
    current: Backend,
    /// Available backends and their capabilities
    available: HashMap<Backend, BackendCapabilities>,
    /// Selection strategy
    strategy: SelectionStrategy,
    /// Required capabilities
    required_capabilities: BackendCapabilities,
    /// Backend preference order
    preference_order: Vec<Backend>,
}

impl BackendSelector {
    /// Create a new backend selector
    pub fn new() -> Self {
        let mut available = HashMap::new();

        // In a real implementation, we'd probe the system for available backends
        // For now, assume WebGPU is available
        available.insert(Backend::WebGPU, BackendCapabilities::webgpu());

        // Check for Vulkan (desktop only)
        #[cfg(not(target_arch = "wasm32"))]
        {
            available.insert(Backend::Vulkan, BackendCapabilities::vulkan());
        }

        // WebGL2 always available in browser
        #[cfg(target_arch = "wasm32")]
        {
            available.insert(Backend::WebGL2, BackendCapabilities::webgl2());
        }

        // Software always available as fallback
        available.insert(Backend::Software, BackendCapabilities::software());

        let preference_order = vec![Backend::Vulkan, Backend::WebGPU, Backend::WebGL2, Backend::Software];

        let current = Self::select_best(&available, &preference_order);

        Self {
            current,
            available,
            strategy: SelectionStrategy::BestAvailable,
            required_capabilities: BackendCapabilities::default(),
            preference_order,
        }
    }

    /// Get the current backend
    pub fn current(&self) -> Backend {
        self.current
    }

    /// Get capabilities of the current backend
    pub fn current_capabilities(&self) -> &BackendCapabilities {
        self.available.get(&self.current).unwrap()
    }

    /// Get all available backends
    pub fn available(&self) -> &HashMap<Backend, BackendCapabilities> {
        &self.available
    }

    /// Check if a backend is available
    pub fn is_available(&self, backend: Backend) -> bool {
        self.available.contains_key(&backend)
    }

    /// Set the selection strategy
    pub fn set_strategy(&mut self, strategy: SelectionStrategy) {
        self.strategy = strategy;
        self.reselect();
    }

    /// Set required capabilities
    pub fn set_required_capabilities(&mut self, caps: BackendCapabilities) {
        self.required_capabilities = caps;
        self.reselect();
    }

    /// Force a specific backend (returns false if not available)
    pub fn force_backend(&mut self, backend: Backend) -> bool {
        if self.is_available(backend) {
            self.current = backend;
            self.strategy = SelectionStrategy::Force(backend);
            true
        } else {
            false
        }
    }

    /// Select the best backend based on preferences
    fn select_best(
        available: &HashMap<Backend, BackendCapabilities>,
        preference_order: &[Backend],
    ) -> Backend {
        for backend in preference_order {
            if available.contains_key(backend) {
                return *backend;
            }
        }
        Backend::Software // Ultimate fallback
    }

    /// Re-select backend based on current strategy
    fn reselect(&mut self) {
        self.current = match self.strategy {
            SelectionStrategy::BestAvailable => {
                Self::select_best(&self.available, &self.preference_order)
            }
            SelectionStrategy::Prefer(preferred) => {
                if self.is_available(preferred) {
                    preferred
                } else {
                    Self::select_best(&self.available, &self.preference_order)
                }
            }
            SelectionStrategy::Force(forced) => forced,
            SelectionStrategy::ByCapabilities => {
                // Find the best backend that meets requirements
                let mut selected = None;
                for backend in &self.preference_order {
                    if let Some(caps) = self.available.get(backend) {
                        if caps.supports(&self.required_capabilities) {
                            selected = Some(*backend);
                            break;
                        }
                    }
                }
                selected.unwrap_or_else(|| Self::select_best(&self.available, &self.preference_order))
            }
        };
    }

    /// Get a report of backend status
    pub fn status_report(&self) -> String {
        let mut report = format!("Current backend: {}\n", self.current);
        report.push_str("Available backends:\n");
        for (backend, caps) in &self.available {
            report.push_str(&format!(
                "  - {} (max_tex: {}, compute: {})\n",
                backend, caps.max_texture_size, caps.compute_shaders
            ));
        }
        report
    }
}

impl Default for BackendSelector {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_backend_selection() {
        let selector = BackendSelector::new();
        assert!(selector.is_available(Backend::WebGPU) || selector.is_available(Backend::Software));
    }

    #[test]
    fn test_force_backend() {
        let mut selector = BackendSelector::new();

        // Software should always be available
        assert!(selector.force_backend(Backend::Software));
        assert_eq!(selector.current(), Backend::Software);
    }

    #[test]
    fn test_capabilities() {
        let vulkan_caps = BackendCapabilities::vulkan();
        let webgl_caps = BackendCapabilities::webgl2();

        // WebGL can't satisfy Vulkan's capabilities
        assert!(!webgl_caps.supports(&vulkan_caps));

        // Vulkan can satisfy WebGL's capabilities
        let webgl_required = BackendCapabilities {
            compute_shaders: false,
            ..Default::default()
        };
        assert!(vulkan_caps.supports(&webgl_required));
    }
}
