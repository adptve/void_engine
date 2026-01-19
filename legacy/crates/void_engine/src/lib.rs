//! # void_engine - Void Engine
//!
//! The main engine orchestration crate that ties everything together:
//! - Application lifecycle management
//! - Plugin loading and management
//! - Frame timing and main loop
//! - Resource coordination
//!
//! ## Architecture
//!
//! The engine follows the "Everything is a Plugin" philosophy:
//! - Core systems are plugins
//! - Renderers are plugins
//! - XR support is a plugin
//! - Even the main loop can be replaced
//!
//! ## Example
//!
//! ```ignore
//! use void_engine::prelude::*;
//!
//! fn main() {
//!     Engine::builder()
//!         .with_plugin(RenderPlugin::new())
//!         .with_plugin(XrPlugin::new())
//!         .with_plugin(MyGamePlugin::new())
//!         .build()
//!         .run();
//! }
//! ```

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use core::any::{Any, TypeId};
use core::time::Duration;

// Re-export crates
pub use void_core;
pub use void_math;
pub use void_memory;
pub use void_structures;
pub use void_ecs;
pub use void_event;
pub use void_asset;
pub use void_render;
pub use void_graph;
pub use void_xr;
pub use void_ir;
pub use void_kernel;

use void_core::{Id, Version, Plugin as CorePlugin, PluginRegistry};
use void_kernel::{Kernel, KernelConfig, FrameContext};
use void_ecs::World;
use void_event::EventBus;
use void_asset::AssetServer;
use void_render::Compositor;
use void_xr::XrSystem;
use void_graph::NodeRegistry;

/// Engine state
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EngineState {
    /// Engine is not yet started
    Created,
    /// Engine is initializing
    Initializing,
    /// Engine is running
    Running,
    /// Engine is paused
    Paused,
    /// Engine is shutting down
    ShuttingDown,
    /// Engine has stopped
    Stopped,
}

impl Default for EngineState {
    fn default() -> Self {
        Self::Created
    }
}

/// Frame timing information
#[derive(Clone, Copy, Debug, Default)]
pub struct FrameTime {
    /// Delta time in seconds
    pub delta: f32,
    /// Total elapsed time in seconds
    pub total: f64,
    /// Frame number
    pub frame: u64,
    /// Target FPS
    pub target_fps: u32,
    /// Actual FPS
    pub actual_fps: f32,
}

/// Engine configuration
#[derive(Clone, Debug)]
pub struct EngineConfig {
    /// Application name
    pub app_name: String,
    /// Application version
    pub app_version: Version,
    /// Target FPS (0 = uncapped)
    pub target_fps: u32,
    /// Fixed timestep for physics (seconds)
    pub fixed_timestep: f32,
    /// Maximum delta time (to prevent spiral of death)
    pub max_delta_time: f32,
    /// Enable hot-reload
    pub hot_reload: bool,
    /// Asset directory
    pub asset_dir: String,
}

impl Default for EngineConfig {
    fn default() -> Self {
        Self {
            app_name: "Metaverse App".to_string(),
            app_version: Version::new(0, 1, 0),
            target_fps: 60,
            fixed_timestep: 1.0 / 60.0,
            max_delta_time: 0.25,
            hot_reload: true,
            asset_dir: "assets".to_string(),
        }
    }
}

/// Resources shared across the engine
pub struct Resources {
    /// Type-erased resources
    resources: BTreeMap<TypeId, Box<dyn Any + Send + Sync>>,
}

impl Resources {
    /// Create new resources
    pub fn new() -> Self {
        Self {
            resources: BTreeMap::new(),
        }
    }

    /// Insert a resource
    pub fn insert<R: Send + Sync + 'static>(&mut self, resource: R) {
        self.resources.insert(TypeId::of::<R>(), Box::new(resource));
    }

    /// Get a resource
    pub fn get<R: Send + Sync + 'static>(&self) -> Option<&R> {
        self.resources
            .get(&TypeId::of::<R>())
            .and_then(|r| r.downcast_ref())
    }

    /// Get a mutable resource
    pub fn get_mut<R: Send + Sync + 'static>(&mut self) -> Option<&mut R> {
        self.resources
            .get_mut(&TypeId::of::<R>())
            .and_then(|r| r.downcast_mut())
    }

    /// Remove a resource
    pub fn remove<R: Send + Sync + 'static>(&mut self) -> Option<R> {
        self.resources
            .remove(&TypeId::of::<R>())
            .and_then(|r| r.downcast().ok())
            .map(|b| *b)
    }

    /// Check if resource exists
    pub fn contains<R: Send + Sync + 'static>(&self) -> bool {
        self.resources.contains_key(&TypeId::of::<R>())
    }
}

impl Default for Resources {
    fn default() -> Self {
        Self::new()
    }
}

/// Engine plugin trait
pub trait EnginePlugin: Send + Sync {
    /// Get plugin name
    fn name(&self) -> &str;

    /// Get plugin version
    fn version(&self) -> Version {
        Version::new(0, 1, 0)
    }

    /// Initialize the plugin
    fn on_init(&mut self, engine: &mut EngineContext) {}

    /// Called when engine starts running
    fn on_start(&mut self, engine: &mut EngineContext) {}

    /// Called each frame before update
    fn on_pre_update(&mut self, engine: &mut EngineContext) {}

    /// Called each frame for main update
    fn on_update(&mut self, engine: &mut EngineContext) {}

    /// Called each frame after update
    fn on_post_update(&mut self, engine: &mut EngineContext) {}

    /// Called each frame for fixed timestep physics
    fn on_fixed_update(&mut self, engine: &mut EngineContext) {}

    /// Called each frame before rendering
    fn on_pre_render(&mut self, engine: &mut EngineContext) {}

    /// Called each frame for rendering
    fn on_render(&mut self, engine: &mut EngineContext) {}

    /// Called each frame after rendering
    fn on_post_render(&mut self, engine: &mut EngineContext) {}

    /// Called when engine is stopping
    fn on_stop(&mut self, engine: &mut EngineContext) {}

    /// Called when plugin is being unloaded
    fn on_shutdown(&mut self, engine: &mut EngineContext) {}
}

/// Context provided to plugins during callbacks
pub struct EngineContext<'a> {
    /// Engine configuration
    pub config: &'a EngineConfig,
    /// Frame timing
    pub time: &'a FrameTime,
    /// ECS World
    pub world: &'a mut World,
    /// Event bus
    pub events: &'a mut EventBus,
    /// Asset server
    pub assets: &'a AssetServer,
    /// Compositor
    pub compositor: &'a mut Compositor,
    /// XR system
    pub xr: &'a mut XrSystem,
    /// Visual scripting registry
    pub graph_registry: &'a mut NodeRegistry,
    /// Shared resources
    pub resources: &'a mut Resources,
    /// Kernel - access to IR patch bus, layers, and backend
    pub kernel: &'a mut Kernel,
    /// Kernel frame context (available during frame)
    pub frame_context: Option<FrameContext>,
    /// Engine state
    pub state: EngineState,
}

impl<'a> EngineContext<'a> {
    /// Request engine shutdown
    pub fn request_shutdown(&mut self) {
        // Would set a flag that the main loop checks
    }

    /// Get delta time
    pub fn delta_time(&self) -> f32 {
        self.time.delta
    }

    /// Get total elapsed time
    pub fn total_time(&self) -> f64 {
        self.time.total
    }

    /// Get frame number
    pub fn frame(&self) -> u64 {
        self.time.frame
    }
}

/// The main engine
pub struct Engine {
    /// Configuration
    config: EngineConfig,
    /// Current state
    state: EngineState,
    /// Plugins
    plugins: Vec<Box<dyn EnginePlugin>>,
    /// ECS World
    world: World,
    /// Event bus
    events: EventBus,
    /// Asset server
    assets: AssetServer,
    /// Compositor
    compositor: Compositor,
    /// XR system
    xr: XrSystem,
    /// Visual scripting registry
    graph_registry: NodeRegistry,
    /// Resources
    resources: Resources,
    /// Kernel - core orchestration for IR patches and layer composition
    kernel: Kernel,
    /// Frame timing
    time: FrameTime,
    /// Fixed timestep accumulator
    fixed_accumulator: f32,
    /// Should exit
    should_exit: bool,
}

impl Engine {
    /// Create a new engine builder
    pub fn builder() -> EngineBuilder {
        EngineBuilder::new()
    }

    /// Create with default configuration
    pub fn new() -> Self {
        Self::with_config(EngineConfig::default())
    }

    /// Create with configuration
    pub fn with_config(config: EngineConfig) -> Self {
        let asset_config = void_asset::AssetServerConfig {
            asset_dir: config.asset_dir.clone(),
            hot_reload: config.hot_reload,
            ..Default::default()
        };

        // Create kernel with matching config
        let kernel_config = KernelConfig {
            target_fps: config.target_fps,
            ..Default::default()
        };
        let kernel = Kernel::new(kernel_config);

        Self {
            time: FrameTime {
                target_fps: config.target_fps,
                ..Default::default()
            },
            config,
            state: EngineState::Created,
            plugins: Vec::new(),
            world: World::new(),
            events: EventBus::new(),
            assets: AssetServer::new(asset_config),
            compositor: Compositor::new(),
            xr: XrSystem::new(),
            graph_registry: void_graph::builtin::create_default_registry(),
            resources: Resources::new(),
            kernel,
            fixed_accumulator: 0.0,
            should_exit: false,
        }
    }

    /// Add a plugin
    pub fn add_plugin<P: EnginePlugin + 'static>(&mut self, plugin: P) {
        self.plugins.push(Box::new(plugin));
    }

    /// Initialize the engine
    pub fn initialize(&mut self) {
        if self.state != EngineState::Created {
            return;
        }

        self.state = EngineState::Initializing;

        // Initialize XR
        let _ = self.xr.initialize();

        // Start the kernel
        self.kernel.start();

        // Initialize plugins
        for plugin in &mut self.plugins {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: None,
                state: self.state,
            };
            plugin.on_init(&mut ctx);
        }

        self.state = EngineState::Running;

        // Call on_start
        for plugin in &mut self.plugins {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: None,
                state: self.state,
            };
            plugin.on_start(&mut ctx);
        }
    }

    /// Run a single frame
    pub fn update(&mut self, delta_time: f32) {
        if self.state != EngineState::Running {
            return;
        }

        // Update timing
        let delta = delta_time.min(self.config.max_delta_time);
        self.time.delta = delta;
        self.time.total += delta as f64;
        self.time.frame += 1;
        self.time.actual_fps = 1.0 / delta;

        // Begin kernel frame - this starts the frame context for IR processing
        let frame_ctx = self.kernel.begin_frame(delta);

        // Pre-update
        for plugin in &mut self.plugins {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: Some(frame_ctx),
                state: self.state,
            };
            plugin.on_pre_update(&mut ctx);
        }

        // Fixed timestep updates
        self.fixed_accumulator += delta;
        while self.fixed_accumulator >= self.config.fixed_timestep {
            for plugin in &mut self.plugins {
                let mut ctx = EngineContext {
                    config: &self.config,
                    time: &self.time,
                    world: &mut self.world,
                    events: &mut self.events,
                    assets: &self.assets,
                    compositor: &mut self.compositor,
                    xr: &mut self.xr,
                    graph_registry: &mut self.graph_registry,
                    resources: &mut self.resources,
                    kernel: &mut self.kernel,
                    frame_context: Some(frame_ctx),
                    state: self.state,
                };
                plugin.on_fixed_update(&mut ctx);
            }
            self.fixed_accumulator -= self.config.fixed_timestep;
        }

        // Main update - plugins can submit IR patches here
        for plugin in &mut self.plugins {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: Some(frame_ctx),
                state: self.state,
            };
            plugin.on_update(&mut ctx);
        }

        // Process IR transactions - apply all batched patches atomically
        let _results = self.kernel.process_transactions(&mut self.world);

        // Post-update
        for plugin in &mut self.plugins {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: Some(frame_ctx),
                state: self.state,
            };
            plugin.on_post_update(&mut ctx);
        }

        // Process events
        self.events.process();

        // Run ECS systems
        self.world.run_systems();
    }

    /// Render a single frame
    pub fn render(&mut self) {
        if self.state != EngineState::Running {
            return;
        }

        // Build kernel render graph from layers
        let _render_graph = self.kernel.build_render_graph();

        // Pre-render
        for plugin in &mut self.plugins {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: None,
                state: self.state,
            };
            plugin.on_pre_render(&mut ctx);
        }

        // Main render
        for plugin in &mut self.plugins {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: None,
                state: self.state,
            };
            plugin.on_render(&mut ctx);
        }

        // Post-render
        for plugin in &mut self.plugins {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: None,
                state: self.state,
            };
            plugin.on_post_render(&mut ctx);
        }

        // End kernel frame
        self.kernel.end_frame();
    }

    /// Shutdown the engine
    pub fn shutdown(&mut self) {
        if self.state == EngineState::Stopped {
            return;
        }

        self.state = EngineState::ShuttingDown;

        // Stop plugins
        for plugin in &mut self.plugins {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: None,
                state: self.state,
            };
            plugin.on_stop(&mut ctx);
        }

        // Shutdown plugins
        for plugin in self.plugins.iter_mut().rev() {
            let mut ctx = EngineContext {
                config: &self.config,
                time: &self.time,
                world: &mut self.world,
                events: &mut self.events,
                assets: &self.assets,
                compositor: &mut self.compositor,
                xr: &mut self.xr,
                graph_registry: &mut self.graph_registry,
                resources: &mut self.resources,
                kernel: &mut self.kernel,
                frame_context: None,
                state: self.state,
            };
            plugin.on_shutdown(&mut ctx);
        }

        // Shutdown kernel
        self.kernel.shutdown();

        // Shutdown XR
        self.xr.shutdown();

        self.state = EngineState::Stopped;
    }

    /// Get engine state
    pub fn state(&self) -> EngineState {
        self.state
    }

    /// Get configuration
    pub fn config(&self) -> &EngineConfig {
        &self.config
    }

    /// Get frame timing
    pub fn time(&self) -> &FrameTime {
        &self.time
    }

    /// Get ECS world
    pub fn world(&self) -> &World {
        &self.world
    }

    /// Get mutable ECS world
    pub fn world_mut(&mut self) -> &mut World {
        &mut self.world
    }

    /// Get event bus
    pub fn events(&self) -> &EventBus {
        &self.events
    }

    /// Get mutable event bus
    pub fn events_mut(&mut self) -> &mut EventBus {
        &mut self.events
    }

    /// Get asset server
    pub fn assets(&self) -> &AssetServer {
        &self.assets
    }

    /// Get compositor
    pub fn compositor(&self) -> &Compositor {
        &self.compositor
    }

    /// Get mutable compositor
    pub fn compositor_mut(&mut self) -> &mut Compositor {
        &mut self.compositor
    }

    /// Get XR system
    pub fn xr(&self) -> &XrSystem {
        &self.xr
    }

    /// Get mutable XR system
    pub fn xr_mut(&mut self) -> &mut XrSystem {
        &mut self.xr
    }

    /// Get resources
    pub fn resources(&self) -> &Resources {
        &self.resources
    }

    /// Get mutable resources
    pub fn resources_mut(&mut self) -> &mut Resources {
        &mut self.resources
    }

    /// Get kernel
    pub fn kernel(&self) -> &Kernel {
        &self.kernel
    }

    /// Get mutable kernel
    pub fn kernel_mut(&mut self) -> &mut Kernel {
        &mut self.kernel
    }
}

impl Default for Engine {
    fn default() -> Self {
        Self::new()
    }
}

/// Builder for constructing an engine
pub struct EngineBuilder {
    config: EngineConfig,
    plugins: Vec<Box<dyn EnginePlugin>>,
}

impl EngineBuilder {
    /// Create a new builder
    pub fn new() -> Self {
        Self {
            config: EngineConfig::default(),
            plugins: Vec::new(),
        }
    }

    /// Set application name
    pub fn app_name(mut self, name: &str) -> Self {
        self.config.app_name = name.to_string();
        self
    }

    /// Set application version
    pub fn app_version(mut self, version: Version) -> Self {
        self.config.app_version = version;
        self
    }

    /// Set target FPS
    pub fn target_fps(mut self, fps: u32) -> Self {
        self.config.target_fps = fps;
        self
    }

    /// Set fixed timestep
    pub fn fixed_timestep(mut self, timestep: f32) -> Self {
        self.config.fixed_timestep = timestep;
        self
    }

    /// Enable/disable hot-reload
    pub fn hot_reload(mut self, enabled: bool) -> Self {
        self.config.hot_reload = enabled;
        self
    }

    /// Set asset directory
    pub fn asset_dir(mut self, dir: &str) -> Self {
        self.config.asset_dir = dir.to_string();
        self
    }

    /// Add a plugin
    pub fn with_plugin<P: EnginePlugin + 'static>(mut self, plugin: P) -> Self {
        self.plugins.push(Box::new(plugin));
        self
    }

    /// Build the engine
    pub fn build(self) -> Engine {
        let mut engine = Engine::with_config(self.config);
        for plugin in self.plugins {
            engine.plugins.push(plugin);
        }
        engine
    }
}

impl Default for EngineBuilder {
    fn default() -> Self {
        Self::new()
    }
}

/// Prelude - commonly used types
pub mod prelude {
    pub use crate::{
        Engine, EngineBuilder, EngineConfig, EngineState,
        EnginePlugin, EngineContext, FrameTime, Resources,
    };

    // Re-export from sub-crates
    pub use void_core::prelude::*;
    pub use void_math::prelude::*;
    pub use void_ecs::prelude::*;
    pub use void_event::prelude::*;
    pub use void_asset::prelude::*;
    pub use void_render::prelude::*;
    pub use void_graph::prelude::*;
    pub use void_xr::prelude::*;

    // Re-export kernel types
    pub use void_kernel::{
        Kernel, KernelConfig, KernelState, FrameContext,
        app::{AppManifest, AppPermissions, ResourceRequirements, LayerRequest, AppId},
        layer::{LayerManager, LayerId},
        backend::{Backend, BackendSelector, BackendCapabilities},
        registry::AssetRegistry,
    };

    // Re-export IR types
    pub use void_ir::{
        Patch, PatchKind, EntityPatch, ComponentPatch, EntityRef, LayerPatch, AssetPatch,
        Transaction, TransactionId, TransactionBuilder, TransactionResult,
        PatchBus, NamespaceHandle,
        Namespace, NamespaceId, NamespacePermissions, ResourceLimits,
        Value,
        patch::LayerType,
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestPlugin {
        name: String,
        init_called: bool,
    }

    impl TestPlugin {
        fn new(name: &str) -> Self {
            Self {
                name: name.to_string(),
                init_called: false,
            }
        }
    }

    impl EnginePlugin for TestPlugin {
        fn name(&self) -> &str {
            &self.name
        }

        fn on_init(&mut self, _engine: &mut EngineContext) {
            self.init_called = true;
        }
    }

    #[test]
    fn test_engine_builder() {
        let engine = Engine::builder()
            .app_name("Test App")
            .target_fps(120)
            .build();

        assert_eq!(engine.config().app_name, "Test App");
        assert_eq!(engine.config().target_fps, 120);
    }

    #[test]
    fn test_engine_lifecycle() {
        let mut engine = Engine::new();
        assert_eq!(engine.state(), EngineState::Created);

        engine.initialize();
        assert_eq!(engine.state(), EngineState::Running);

        engine.update(1.0 / 60.0);
        assert_eq!(engine.time().frame, 1);

        engine.shutdown();
        assert_eq!(engine.state(), EngineState::Stopped);
    }
}
