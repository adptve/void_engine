//! Metaverse OS Runtime
//!
//! This is the main entry point for the Metaverse Operating System.
//! The runtime is a persistent process that:
//! - Never crashes (all panics are caught)
//! - Composites layers from multiple apps
//! - Provides a shell interface
//! - Hot-swaps components without restart
//!
//! Run with: cargo run -p void_runtime
//!       or: cargo run --bin metaverse

mod runtime;
mod presenter;
mod compositor;
mod scene_renderer;
mod scene_loader;
mod texture_manager;
mod input;
mod boot_config;
mod app_loader;
mod game_systems;

#[cfg(all(feature = "drm-backend", target_os = "linux"))]
mod drm_main;

#[cfg(feature = "smithay")]
mod smithay_main;

use boot_config::{Backend, BootConfig};

use std::sync::Arc;

use void_kernel::KernelState;

use crate::runtime::Runtime;
use crate::presenter::DesktopPresenter;
use crate::compositor::Compositor;
use crate::scene_renderer::SceneRenderer;
use crate::input::InputState;
use void_render::camera_controller::CameraInput;
use std::path::Path;

fn main() {
    // Initialize logging
    env_logger::Builder::from_env(
        env_logger::Env::default().default_filter_or("info")
    ).init();

    // Print banner
    println!();
    println!("╔═══════════════════════════════════════════════════════════╗");
    println!("║           METAVERSE OPERATING SYSTEM v0.1.0               ║");
    println!("║                                                           ║");
    println!("║  The kernel never dies. All updates via hot-swap.         ║");
    println!("╚═══════════════════════════════════════════════════════════╝");
    println!();

    // Install panic handler to prevent crashes
    std::panic::set_hook(Box::new(|panic_info| {
        log::error!("PANIC CAUGHT (kernel continues): {}", panic_info);
    }));

    // Load boot configuration
    let config = BootConfig::load();
    config.print_summary();

    // Run with configured backend
    run_with_backend(config.effective_backend(), &config);
}

/// Run with the specified backend, falling back as needed
fn run_with_backend(backend: Backend, config: &BootConfig) {
    log::info!("Starting with backend: {}", backend);

    let result = match backend {
        Backend::Smithay => run_smithay_backend(),
        Backend::Winit => run_winit_backend(config),
        Backend::Xr => run_xr_backend(config),
        Backend::Cli => {
            run_cli_mode(config);
            Ok(())
        }
        Backend::Auto => {
            // Should not happen - effective_backend resolves Auto
            run_cli_mode(config);
            Ok(())
        }
    };

    // Handle failure with fallback
    if let Err(e) = result {
        log::warn!("Backend {} failed: {}", backend, e);

        if config.fallback != backend && config.fallback != Backend::Auto {
            log::info!("Falling back to: {}", config.fallback);
            run_with_backend(config.fallback, config);
        } else {
            log::info!("Falling back to CLI mode");
            run_cli_mode(config);
        }
    }
}

/// Run Smithay backend (DRM/KMS compositor)
fn run_smithay_backend() -> Result<(), Box<dyn std::error::Error>> {
    #[cfg(feature = "smithay")]
    {
        log::info!("Starting Smithay compositor...");
        smithay_main::run_smithay();
        Ok(())
    }

    #[cfg(not(feature = "smithay"))]
    {
        Err("Smithay backend not compiled in. Enable 'smithay' feature.".into())
    }
}

/// Run winit backend (window on existing display server)
fn run_winit_backend(config: &BootConfig) -> Result<(), Box<dyn std::error::Error>> {
    // Check if display is available
    #[cfg(target_os = "linux")]
    {
        let has_display = std::env::var("DISPLAY").is_ok()
            || std::env::var("WAYLAND_DISPLAY").is_ok();
        if !has_display {
            return Err("No display server available (DISPLAY/WAYLAND_DISPLAY not set)".into());
        }
    }

    try_run_windowed(config.startup_app.clone())
}

/// Run XR backend (OpenXR for VR/AR)
fn run_xr_backend(config: &BootConfig) -> Result<(), Box<dyn std::error::Error>> {
    #[cfg(feature = "xr")]
    {
        log::info!("Starting XR session...");
        // TODO: Implement XR main loop using void_xr and XrPresenter
        // For now, fall back to winit with XR overlay capability
        log::warn!("XR standalone mode not yet implemented, using winit with XR support");
        try_run_windowed(config.startup_app.clone())
    }

    #[cfg(not(feature = "xr"))]
    {
        Err("XR backend not compiled in. Enable 'xr' feature.".into())
    }
}

/// Run in CLI-only mode when no display is available
fn run_cli_mode(config: &BootConfig) {
    use std::io::{self, Write};

    let mut runtime = crate::runtime::Runtime::new();

    // Load startup app if specified
    if let Some(app_path) = &config.startup_app {
        match runtime.load_app(app_path) {
            Ok(id) => log::info!("Loaded startup app: {:?}", id),
            Err(e) => log::error!("Failed to load startup app: {:?}", e),
        }
    }

    let mut input = String::new();

    println!("Metaverse OS - CLI Mode");
    println!("Type 'help' for commands, 'exit' to quit.");
    println!();

    loop {
        print!("vsh> ");
        io::stdout().flush().unwrap();

        input.clear();
        if io::stdin().read_line(&mut input).is_err() {
            break;
        }

        let cmd = input.trim();
        if cmd.is_empty() {
            continue;
        }

        if cmd == "exit" || cmd == "quit" {
            println!("Shutting down...");
            runtime.shutdown();
            break;
        }

        runtime.execute_command(cmd);

        // Print status
        for line in runtime.status_text() {
            println!("{}", line);
        }

        if runtime.state() == void_kernel::KernelState::Stopped {
            break;
        }
    }
}

fn try_run_windowed(startup_app: Option<String>) -> Result<(), Box<dyn std::error::Error>> {
    use winit::{
        application::ApplicationHandler,
        event::{ElementState, WindowEvent, MouseScrollDelta},
        event_loop::{ActiveEventLoop, ControlFlow, EventLoop},
        keyboard::{Key, NamedKey},
        window::{Window, WindowAttributes},
    };

    /// Application state for windowed mode
    struct WindowedApp {
        startup_app: Option<String>,
        window: Option<Arc<Window>>,
        presenter: Option<DesktopPresenter>,
        scene_renderer: Option<SceneRenderer>,
        compositor: Option<Compositor>,
        runtime: Option<Runtime>,
        game_world: Option<crate::game_systems::GameWorld>,
        game_systems: Option<crate::game_systems::GameSystemsManager>,
        input: InputState,
        last_mouse_pos: Option<(f32, f32)>,
        scroll_delta: f32,
        show_shell: bool,
        shell_input: String,
        frame: u64,
        last_time: std::time::Instant,
        jump_pressed: bool,
    }

    impl WindowedApp {
        fn new(startup_app: Option<String>) -> Self {
            Self {
                startup_app,
                window: None,
                presenter: None,
                scene_renderer: None,
                compositor: None,
                runtime: None,
                game_world: None,
                game_systems: None,
                input: InputState::new(),
                last_mouse_pos: None,
                scroll_delta: 0.0,
                show_shell: true,
                shell_input: String::new(),
                frame: 0,
                last_time: std::time::Instant::now(),
                jump_pressed: false,
            }
        }
    }

    impl ApplicationHandler for WindowedApp {
        fn resumed(&mut self, event_loop: &ActiveEventLoop) {
            // Create window
            let window_attrs = WindowAttributes::default()
                .with_title("Metaverse OS")
                .with_inner_size(winit::dpi::LogicalSize::new(1280, 720));

            let window = Arc::new(
                event_loop.create_window(window_attrs).expect("Failed to create window")
            );
            self.window = Some(window.clone());

            // Create presenter (GPU interface)
            let presenter = pollster::block_on(DesktopPresenter::new(window.clone()));

            // Create scene renderer (3D content)
            let initial_size = presenter.size();
            let mut scene_renderer = SceneRenderer::new(
                presenter.device().as_ref(),
                presenter.queue().as_ref(),
                presenter.format(),
                initial_size,
            );

            // Load scene from file
            // Priority: 1) startup_app (as file or directory), 2) examples/model-viewer/scene.toml, 3) empty scene
            let scene_paths = [
                self.startup_app.as_ref().map(|p| {
                    let path = Path::new(p);
                    // If it's already a .toml file, use directly; otherwise append scene.toml
                    if path.extension().map_or(false, |ext| ext == "toml") {
                        path.to_path_buf()
                    } else {
                        path.join("scene.toml")
                    }
                }),
                Some(Path::new("examples/model-viewer/scene.toml").to_path_buf()),
            ];

            // Create game world for physics and triggers
            let mut game_world = crate::game_systems::GameWorld::new();
            // Create game systems manager for combat, inventory, AI, etc.
            let mut game_systems = crate::game_systems::GameSystemsManager::new();

            let mut scene_loaded = false;
            for scene_path_opt in scene_paths.iter().flatten() {
                if scene_path_opt.exists() {
                    log::info!("Loading scene from: {}", scene_path_opt.display());
                    match scene_loader::load_scene(scene_path_opt) {
                        Ok(scene) => {
                            // Asset base path: use the scene file's parent directory
                            // This allows each example to have its own assets (models, textures, etc.)
                            let effective_asset_path = scene_path_opt
                                .parent()
                                .map(|p| p.to_path_buf())
                                .unwrap_or_else(|| Path::new(".").to_path_buf());

                            scene_renderer.apply_scene(
                                presenter.device().as_ref(),
                                presenter.queue().as_ref(),
                                presenter.format(),
                                &scene,
                                &effective_asset_path,
                            );

                            // Setup physics from scene definition
                            setup_physics_from_scene(&mut game_world, &scene);

                            // Setup game systems from scene definition
                            setup_game_systems_from_scene(&mut game_systems, &scene);

                            scene_loaded = true;
                            break;
                        }
                        Err(e) => {
                            log::warn!("Failed to load scene from {}: {}", scene_path_opt.display(), e);
                        }
                    }
                }
            }

            if !scene_loaded {
                log::info!("No scene.toml found, starting with empty scene");
            }

            self.game_world = Some(game_world);
            self.game_systems = Some(game_systems);

            // Create compositor (2D overlay, shell)
            let compositor = Compositor::new(
                presenter.device(),
                presenter.queue(),
                presenter.format(),
            );

            // Create runtime (kernel + world + shell)
            let mut runtime = Runtime::new();

            // Load startup app if specified
            if let Some(app_path) = &self.startup_app {
                match runtime.load_app(app_path) {
                    Ok(id) => log::info!("Loaded startup app: {:?}", id),
                    Err(e) => log::error!("Failed to load startup app: {:?}", e),
                }
            }

            log::info!("Runtime initialized. Press ` to toggle shell, ESC to exit.");
            log::info!("Controls: WASD to move, Q/E or Space/Ctrl for up/down, Shift to sprint");
            log::info!("Left mouse: orbit, Right/Middle mouse: pan, Scroll: zoom, F: reset camera");

            self.presenter = Some(presenter);
            self.scene_renderer = Some(scene_renderer);
            self.compositor = Some(compositor);
            self.runtime = Some(runtime);
        }

        fn window_event(
            &mut self,
            event_loop: &ActiveEventLoop,
            _window_id: winit::window::WindowId,
            event: WindowEvent,
        ) {
            let Some(window) = &self.window else { return };
            let Some(presenter) = &mut self.presenter else { return };
            let Some(scene_renderer) = &mut self.scene_renderer else { return };
            let Some(compositor) = &mut self.compositor else { return };
            let Some(runtime) = &mut self.runtime else { return };

            match event {
                WindowEvent::CloseRequested => {
                    log::info!("Shutdown requested...");
                    runtime.shutdown();
                    event_loop.exit();
                }

                WindowEvent::Resized(size) => {
                    presenter.resize((size.width, size.height));
                    scene_renderer.resize(presenter.device().as_ref(), (size.width, size.height));
                    compositor.resize(size.width, size.height);
                }

                WindowEvent::MouseWheel { delta, .. } => {
                    // Accumulate scroll delta (don't overwrite - multiple events can arrive per frame)
                    self.scroll_delta += match delta {
                        MouseScrollDelta::LineDelta(_, y) => y,  // Full line delta
                        MouseScrollDelta::PixelDelta(pos) => pos.y as f32 * 0.02,
                    };
                }

                WindowEvent::KeyboardInput { event, .. } => {
                    self.input.handle_key(&event);

                    if event.state == ElementState::Pressed {
                        match &event.logical_key {
                            Key::Named(NamedKey::Escape) => {
                                log::info!("Escape pressed, shutting down...");
                                runtime.shutdown();
                                event_loop.exit();
                            }
                            Key::Character(c) if c == "`" => {
                                self.show_shell = !self.show_shell;
                                log::info!("Shell overlay: {}", if self.show_shell { "ON" } else { "OFF" });
                            }
                            Key::Named(NamedKey::Enter) if self.show_shell => {
                                if !self.shell_input.is_empty() {
                                    runtime.execute_command(&self.shell_input);
                                    self.shell_input.clear();
                                }
                            }
                            Key::Named(NamedKey::Backspace) if self.show_shell => {
                                self.shell_input.pop();
                            }
                            Key::Character(c) if self.show_shell => {
                                self.shell_input.push_str(c);
                            }
                            _ => {}
                        }
                    }

                    // Check if runtime wants to exit
                    if runtime.state() == KernelState::Stopped {
                        event_loop.exit();
                    }
                }

                WindowEvent::CursorMoved { position, .. } => {
                    self.input.set_mouse_position(position.x as f32, position.y as f32);
                }

                WindowEvent::MouseInput { state, button, .. } => {
                    self.input.handle_mouse_button(button, state);
                }

                WindowEvent::RedrawRequested => {
                    self.frame += 1;

                    // Calculate delta time
                    let now = std::time::Instant::now();
                    let delta = (now - self.last_time).as_secs_f32();
                    self.last_time = now;

                    // Update runtime
                    runtime.update(delta, self.frame);

                    // Calculate mouse delta for camera
                    let current_mouse = self.input.mouse_position;
                    let mouse_delta = if let Some(last_pos) = self.last_mouse_pos {
                        (current_mouse.0 - last_pos.0, current_mouse.1 - last_pos.1)
                    } else {
                        (0.0, 0.0)
                    };
                    self.last_mouse_pos = Some(current_mouse);

                    // Mouse button states
                    let left_mouse = self.input.is_mouse_pressed(winit::event::MouseButton::Left);
                    let right_mouse = self.input.is_mouse_pressed(winit::event::MouseButton::Right);
                    let middle_mouse = self.input.is_mouse_pressed(winit::event::MouseButton::Middle);

                    // WASD + QE movement
                    let mut move_forward = 0.0f32;
                    let mut move_right = 0.0f32;
                    let mut move_up = 0.0f32;

                    if self.input.is_key_pressed("w") || self.input.is_key_pressed("W") { move_forward += 1.0; }
                    if self.input.is_key_pressed("s") || self.input.is_key_pressed("S") { move_forward -= 1.0; }
                    if self.input.is_key_pressed("d") || self.input.is_key_pressed("D") { move_right += 1.0; }
                    if self.input.is_key_pressed("a") || self.input.is_key_pressed("A") { move_right -= 1.0; }
                    if self.input.is_key_pressed("q") || self.input.is_key_pressed("Q") { move_up -= 1.0; }
                    if self.input.is_key_pressed("e") || self.input.is_key_pressed("E") { move_up += 1.0; }
                    if self.input.is_key_pressed("Space") { move_up += 1.0; }
                    if self.input.is_key_pressed("ControlLeft") || self.input.is_key_pressed("ControlRight") { move_up -= 1.0; }

                    // F key - reset camera to default
                    if self.input.is_key_pressed("f") || self.input.is_key_pressed("F") {
                        scene_renderer.camera_controller.orbit_target = void_math::Vec3::ZERO;
                        scene_renderer.camera_controller.orbit_distance = 5.0;
                        scene_renderer.camera_controller.yaw = 0.0;
                        scene_renderer.camera_controller.pitch = 0.3;
                    }

                    let sprint = self.input.is_key_pressed("ShiftLeft") || self.input.is_key_pressed("ShiftRight");

                    // Check for jump (Space key - only trigger on first press)
                    let space_pressed = self.input.is_key_pressed("Space");
                    let jump = space_pressed && !self.jump_pressed;
                    self.jump_pressed = space_pressed;

                    // Check if physics-based FPS movement is active
                    let game_world = self.game_world.as_mut();
                    let physics_active = game_world.as_ref().map_or(false, |gw| gw.enabled);

                    if physics_active {
                        // Physics-based FPS camera mode
                        let game_world = game_world.unwrap();

                        // Mouse look - always active in FPS mode
                        let sensitivity = 0.003;
                        scene_renderer.camera_controller.yaw -= mouse_delta.0 * sensitivity;
                        scene_renderer.camera_controller.pitch -= mouse_delta.1 * sensitivity;
                        scene_renderer.camera_controller.pitch = scene_renderer.camera_controller.pitch.clamp(-1.5, 1.5);

                        // Update player movement through physics
                        let yaw = scene_renderer.camera_controller.yaw;
                        game_world.update_player_movement(move_forward, move_right, jump, yaw, delta);

                        // Step physics
                        game_world.step(delta);

                        // Update game systems (combat, inventory, AI, etc.)
                        if let Some(game_systems) = &mut self.game_systems {
                            // Sync player position to game systems
                            if let Some(player_pos) = game_world.get_player_position() {
                                game_systems.entity_positions.insert(game_world.player.entity_id, player_pos);
                            }
                            game_systems.update(delta);
                        }

                        // Sync camera position with player
                        if let Some(eye_pos) = game_world.get_player_eye_position() {
                            scene_renderer.camera.position = void_math::Vec3::new(eye_pos[0], eye_pos[1], eye_pos[2]);
                            scene_renderer.camera_controller.orbit_target = scene_renderer.camera.position;
                        }

                        // Update camera rotation from yaw/pitch
                        let yaw = scene_renderer.camera_controller.yaw;
                        let pitch = scene_renderer.camera_controller.pitch;
                        scene_renderer.camera.rotation = void_math::Quat::from_euler_yxz(yaw, pitch, 0.0);

                        // Log physics and game systems debug info periodically
                        if self.frame % 300 == 1 {
                            log::info!("Physics: {}", game_world.debug_info());
                            if let Some(gs) = &self.game_systems {
                                log::info!("{}", gs.debug_info());
                            }
                        }
                    } else {
                        // Standard orbit camera mode

                        // Right/middle mouse: pan the orbit target in screen space
                        if (right_mouse || middle_mouse) && !left_mouse {
                            let pan_speed = scene_renderer.camera_controller.orbit_distance * 0.002;
                            let camera_right = scene_renderer.camera.right();
                            let camera_up = scene_renderer.camera.up();
                            scene_renderer.camera_controller.orbit_target =
                                scene_renderer.camera_controller.orbit_target
                                - camera_right * mouse_delta.0 * pan_speed
                                + camera_up * mouse_delta.1 * pan_speed;
                        }

                        // Left mouse: orbit rotation
                        let camera_input = CameraInput {
                            move_forward,
                            move_right,
                            move_up,
                            mouse_delta: if left_mouse { mouse_delta } else { (0.0, 0.0) },
                            scroll_delta: self.scroll_delta,
                            sprint,
                            drag_active: left_mouse,
                        };
                        scene_renderer.update_camera(&camera_input, delta);
                    }

                    scene_renderer.update_particles(delta);
                    scene_renderer.update_animations(delta);
                    self.scroll_delta = 0.0; // Reset scroll after use

                    // Acquire frame
                    let output = match presenter.acquire_frame() {
                        Ok(t) => t,
                        Err(wgpu::SurfaceError::Lost) => {
                            presenter.resize(presenter.size());
                            return;
                        }
                        Err(wgpu::SurfaceError::OutOfMemory) => {
                            log::error!("Out of GPU memory!");
                            event_loop.exit();
                            return;
                        }
                        Err(e) => {
                            log::warn!("Surface error: {:?}", e);
                            return;
                        }
                    };

                    let view = output.texture.create_view(
                        &wgpu::TextureViewDescriptor::default(),
                    );

                    // Render 3D scene first
                    scene_renderer.render(
                        presenter.device().as_ref(),
                        presenter.queue().as_ref(),
                        &view,
                        self.frame,
                    );

                    // Render particles (after entities, before compositor)
                    scene_renderer.render_particles(
                        presenter.device().as_ref(),
                        presenter.queue().as_ref(),
                        &view,
                    );

                    // Render 2D compositor overlay on top
                    let mut shell_lines = runtime.status_text();
                    if self.show_shell && !self.shell_input.is_empty() {
                        shell_lines.push(format!("> {}", self.shell_input));
                    }
                    compositor.render(
                        presenter.device(),
                        presenter.queue(),
                        &view,
                        runtime.render_graph(),
                        self.frame,
                        self.show_shell,
                        shell_lines,
                    );

                    // Present
                    output.present();

                    // Log status periodically
                    if self.frame % 300 == 1 {
                        let state = runtime.state();
                        log::info!(
                            "Frame {}: State={:?}, Layers={}, Entities={}, FPS={:.1}",
                            self.frame,
                            state,
                            runtime.render_graph().layers.len(),
                            scene_renderer.entity_count(),
                            1.0 / delta
                        );
                    }
                }

                _ => {}
            }
        }

        fn about_to_wait(&mut self, _event_loop: &ActiveEventLoop) {
            if let Some(window) = &self.window {
                window.request_redraw();
            }
        }
    }

    // Create event loop - this may fail if no display is available
    let event_loop = EventLoop::new()?;
    event_loop.set_control_flow(ControlFlow::Poll);

    // Create and run app
    let mut app = WindowedApp::new(startup_app);
    event_loop.run_app(&mut app)?;

    Ok(())
}

/// Setup physics bodies and triggers from scene definition
fn setup_physics_from_scene(
    game_world: &mut crate::game_systems::GameWorld,
    scene: &scene_loader::SceneDefinition,
) {
    use void_physics::prelude::ColliderShape;
    use void_triggers::prelude::{TriggerComponent, TriggerVolume};

    let mut entity_id_counter: u64 = 1;

    for entity_def in &scene.entities {
        let entity_id = entity_id_counter;
        entity_id_counter += 1;

        let position = entity_def.transform.position;
        let scale = entity_def.transform.scale.to_array();

        // Handle physics body
        if let Some(physics_def) = &entity_def.physics {
            let shape = match &physics_def.shape {
                scene_loader::PhysicsShapeDef::Box { half_extents } => ColliderShape::Box {
                    half_extents: [
                        half_extents[0] * scale[0],
                        half_extents[1] * scale[1],
                        half_extents[2] * scale[2],
                    ],
                },
                scene_loader::PhysicsShapeDef::Sphere { radius } => ColliderShape::Sphere {
                    radius: *radius * scale[0].max(scale[1]).max(scale[2]),
                },
                scene_loader::PhysicsShapeDef::Capsule { half_height, radius } => {
                    ColliderShape::CapsuleY {
                        half_height: *half_height * scale[1],
                        radius: *radius * scale[0].max(scale[2]),
                    }
                }
                scene_loader::PhysicsShapeDef::Cylinder { half_height, radius } => {
                    ColliderShape::CylinderY {
                        half_height: *half_height * scale[1],
                        radius: *radius * scale[0].max(scale[2]),
                    }
                }
                scene_loader::PhysicsShapeDef::MeshBounds => {
                    // Use scale as box half extents
                    ColliderShape::Box {
                        half_extents: [scale[0] * 0.5, scale[1] * 0.5, scale[2] * 0.5],
                    }
                }
            };

            match physics_def.body_type.as_str() {
                "static" => {
                    game_world.create_static_body(entity_id, position, shape);
                    log::debug!("Created static body for entity '{}' (id={})", entity_def.name, entity_id);
                }
                "dynamic" => {
                    game_world.create_dynamic_body(entity_id, position, shape, physics_def.mass);
                    log::debug!("Created dynamic body for entity '{}' (id={})", entity_def.name, entity_id);
                }
                "player" => {
                    game_world.create_player_body(entity_id, position);
                    log::info!("Created player body for entity '{}' (id={}) at {:?}", entity_def.name, entity_id, position);
                }
                _ => {
                    log::warn!("Unknown physics body type '{}' for entity '{}'", physics_def.body_type, entity_def.name);
                }
            }
        }

        // Handle trigger volumes
        if let Some(trigger_def) = &entity_def.trigger {
            let volume = match &trigger_def.shape {
                scene_loader::TriggerShapeDef::Box { half_extents } => TriggerVolume::Box {
                    half_extents: [
                        half_extents[0] * scale[0],
                        half_extents[1] * scale[1],
                        half_extents[2] * scale[2],
                    ],
                },
                scene_loader::TriggerShapeDef::Sphere { radius } => TriggerVolume::Sphere {
                    radius: *radius * scale[0].max(scale[1]).max(scale[2]),
                },
            };

            game_world.create_trigger(entity_id, position, scale, volume);
            log::debug!("Created trigger for entity '{}' (id={})", entity_def.name, entity_id);
        }
    }

    // Enable game systems if we have a player
    if game_world.player.body_handle.is_some() {
        game_world.enable();
        log::info!("Game physics enabled with player at {:?}", game_world.get_player_position());
    } else {
        log::info!("No player body found in scene - physics system inactive");
    }
}

/// Setup game systems (combat, inventory, AI, etc.) from scene definition
fn setup_game_systems_from_scene(
    game_systems: &mut crate::game_systems::GameSystemsManager,
    scene: &scene_loader::SceneDefinition,
) {
    let mut entity_id_counter: u64 = 1;

    // Register scene-level definitions
    game_systems.register_items(&scene.items);
    game_systems.register_status_effects(&scene.status_effects);
    game_systems.register_quests(&scene.quests);

    log::info!(
        "Registered {} items, {} status effects, {} quests from scene",
        scene.items.len(),
        scene.status_effects.len(),
        scene.quests.len()
    );

    // Process each entity for game components
    for entity_def in &scene.entities {
        let entity_id = entity_id_counter;
        entity_id_counter += 1;

        // Store entity name
        game_systems.entity_names.insert(entity_id, entity_def.name.clone());

        // Store entity position
        let position = entity_def.transform.position;
        game_systems.entity_positions.insert(entity_id, position);

        // Check for player entity
        if let Some(physics_def) = &entity_def.physics {
            if physics_def.body_type == "player" {
                game_systems.player_entity = Some(entity_id);
                log::info!("Player entity registered: '{}' (id={})", entity_def.name, entity_id);
            }
        }

        // Health component
        if let Some(health_def) = &entity_def.health {
            game_systems.create_health(entity_id, health_def);
            log::debug!(
                "Created health component for '{}' (id={}) - {} HP",
                entity_def.name, entity_id, health_def.max_health
            );
        }

        // Weapon component
        if let Some(weapon_def) = &entity_def.weapon {
            game_systems.create_weapon(entity_id, weapon_def);
            log::debug!(
                "Created weapon component for '{}' (id={}) - {}",
                entity_def.name, entity_id, weapon_def.name
            );
        }

        // Inventory component
        if let Some(inventory_def) = &entity_def.inventory {
            game_systems.create_inventory(entity_id, inventory_def);
            log::debug!(
                "Created inventory component for '{}' (id={}) - {} slots",
                entity_def.name, entity_id, inventory_def.slots
            );
        }

        // AI component
        if let Some(ai_def) = &entity_def.ai {
            game_systems.create_ai(entity_id, ai_def);
            log::debug!(
                "Created AI component for '{}' (id={})",
                entity_def.name, entity_id
            );
        }
    }

    // Summary
    log::info!(
        "Game systems initialized: {} health, {} weapons, {} inventories, {} AI entities",
        game_systems.health_components.len(),
        game_systems.weapon_components.len(),
        game_systems.inventory_components.len(),
        game_systems.ai_states.len()
    );
}

