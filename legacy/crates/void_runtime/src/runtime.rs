//! Runtime - the core of the Metaverse OS
//!
//! Contains the kernel, ECS world, shell, and services.

use std::sync::Arc;

use void_kernel::{Kernel, KernelConfig, KernelState, RenderGraph};
use void_ecs::World;
use void_shell::Shell;
use void_ir::{
    Patch, PatchKind, LayerPatch,
    TransactionBuilder,
    patch::LayerType,
};
use void_kernel::app::{AppManifest, AppPermissions, ResourceRequirements, LayerRequest, AppId};

use crate::app_loader::{AppLoader, AppLoadError};

/// The Metaverse Runtime
pub struct Runtime {
    /// The immortal kernel
    kernel: Kernel,
    /// ECS world
    world: World,
    /// Shell interface
    shell: Shell,
    /// App loader
    app_loader: AppLoader,
    /// Shell output history
    shell_output: Vec<String>,
    /// Demo app ID
    demo_app: Option<AppId>,
    /// Current render graph
    render_graph: RenderGraph,
}

impl Runtime {
    /// Create a new runtime
    pub fn new() -> Self {
        log::info!("Creating Metaverse Runtime...");

        // Create kernel
        let mut kernel = Kernel::new(KernelConfig {
            target_fps: 60,
            enable_watchdog: true,
            ..Default::default()
        });

        // Create world
        let world = World::new();

        // Create shell
        let shell = Shell::new();

        // Create app loader with patch bus
        let app_loader = AppLoader::new(kernel.patch_bus().clone());

        // Start kernel
        kernel.start();

        let mut runtime = Self {
            kernel,
            world,
            shell,
            app_loader,
            shell_output: vec![
                "Metaverse OS v0.1.0".to_string(),
                "Type 'help' for commands".to_string(),
                "".to_string(),
            ],
            demo_app: None,
            render_graph: RenderGraph {
                frame: 0,
                layers: vec![],
                backend: void_kernel::Backend::Vulkan,
            },
        };

        // Load demo app
        runtime.load_demo_app();

        runtime
    }

    /// Load an app from a directory
    pub fn load_app(&mut self, path: &str) -> Result<AppId, AppLoadError> {
        log::info!("Loading app from: {}", path);
        let app_id = self.app_loader.load_app(path)?;
        self.shell_output.push(format!("Loaded app: {:?}", app_id));
        Ok(app_id)
    }

    /// Load the demo app
    fn load_demo_app(&mut self) {
        log::info!("Loading demo app...");

        let manifest = AppManifest {
            name: "demo".to_string(),
            version: "1.0.0".to_string(),
            description: Some("Demo application".to_string()),
            author: Some("Metaverse OS".to_string()),
            layers: vec![
                LayerRequest {
                    name: "content".to_string(),
                    layer_type: LayerType::Content,
                    priority: 0,
                },
            ],
            resources: ResourceRequirements::default(),
            permissions: AppPermissions::default(),
        };

        match self.kernel.app_manager_mut().load(manifest) {
            Ok(id) => {
                if let Err(e) = self.kernel.app_manager_mut().initialize(id) {
                    log::error!("Failed to initialize demo app: {:?}", e);
                } else {
                    self.demo_app = Some(id);
                    log::info!("Demo app loaded: {:?}", id);

                    // Create content layer
                    if let Some(handle) = self.kernel.app_manager().get_handle(id) {
                        let handle = handle.clone();
                        let tx = TransactionBuilder::new(handle.id())
                            .patch(Patch::new(
                                handle.id(),
                                PatchKind::Layer(LayerPatch::create("content", LayerType::Content, 0)),
                            ))
                            .build();
                        if let Err(e) = handle.submit(tx) {
                            log::error!("Failed to create layer: {:?}", e);
                        }
                    }
                }
            }
            Err(e) => {
                log::error!("Failed to load demo app: {:?}", e);
            }
        }
    }

    /// Update the runtime
    pub fn update(&mut self, delta: f32, frame: u64) {
        // Begin kernel frame
        let _ctx = self.kernel.begin_frame(delta);

        // Process transactions
        self.kernel.process_transactions(&mut self.world);

        // Build render graph
        self.render_graph = self.kernel.build_render_graph();

        // End kernel frame
        self.kernel.end_frame();

        // Periodic GC
        if frame % 600 == 0 {
            self.kernel.gc();
        }
    }

    /// Get the current render graph
    pub fn render_graph(&self) -> &RenderGraph {
        &self.render_graph
    }

    /// Get kernel state
    pub fn state(&self) -> KernelState {
        self.kernel.state()
    }

    /// Shutdown the runtime
    pub fn shutdown(&mut self) {
        log::info!("Shutting down runtime...");
        self.kernel.shutdown();
    }

    /// Execute a shell command
    pub fn execute_command(&mut self, command: &str) {
        log::info!("Shell: {}", command);
        self.shell_output.push(format!("vsh> {}", command));

        let result = match command.trim() {
            "help" => {
                vec![
                    "Available commands:".to_string(),
                    "  help     - Show this help".to_string(),
                    "  status   - Show kernel status".to_string(),
                    "  apps     - List running apps".to_string(),
                    "  layers   - List layers".to_string(),
                    "  health   - Show health metrics".to_string(),
                    "  clear    - Clear output".to_string(),
                    "  exit     - Shutdown".to_string(),
                ]
            }
            "status" => {
                let status = self.kernel.status();
                vec![
                    format!("State: {:?}", status.state),
                    format!("Frame: {}", status.frame),
                    format!("Uptime: {:.1}s", status.uptime_secs),
                    format!("FPS: {:.1}", status.avg_fps),
                    format!("Apps: {} ({} running)", status.app_count, status.running_apps),
                    format!("Layers: {}", status.layer_count),
                    format!("Health: {}", status.health_level),
                    format!("Backend: {}", status.backend),
                ]
            }
            "apps" => {
                let running = self.kernel.app_manager().running();
                if running.is_empty() {
                    vec!["No apps running".to_string()]
                } else {
                    running.iter().map(|id| {
                        if let Some(app) = self.kernel.app_manager().get(*id) {
                            format!("  {:?}: {} v{} ({:?})", id, app.manifest.name, app.manifest.version, app.state)
                        } else {
                            format!("  {:?}: unknown", id)
                        }
                    }).collect()
                }
            }
            "layers" => {
                let layers = self.kernel.layer_manager().all_ids();
                if layers.is_empty() {
                    vec!["No layers".to_string()]
                } else {
                    layers.iter().filter_map(|id| {
                        self.kernel.layer_manager().get(*id).map(|l| {
                            format!("  {:?}: {} (priority {}, {:?})", id, l.name, l.config.priority, l.config.layer_type)
                        })
                    }).collect()
                }
            }
            "health" => {
                if let Some(metrics) = self.kernel.health_metrics() {
                    vec![
                        format!("Frame time: {:.2}ms", metrics.avg_frame_time_ms),
                        format!("Peak frame time: {:.2}ms", metrics.peak_frame_time_ms),
                        format!("Memory: {} bytes", metrics.memory_used),
                        format!("Slow frames: {}", metrics.slow_frame_count),
                    ]
                } else {
                    vec!["Watchdog disabled".to_string()]
                }
            }
            "clear" => {
                self.shell_output.clear();
                vec![]
            }
            "exit" => {
                self.shutdown();
                vec!["Goodbye!".to_string()]
            }
            "" => vec![],
            cmd => {
                vec![format!("Unknown command: {}", cmd)]
            }
        };

        self.shell_output.extend(result);

        // Keep only last 50 lines
        if self.shell_output.len() > 50 {
            let drain_count = self.shell_output.len() - 50;
            self.shell_output.drain(0..drain_count);
        }
    }

    /// Get status text for overlay
    pub fn status_text(&self) -> Vec<String> {
        self.shell_output.clone()
    }
}
