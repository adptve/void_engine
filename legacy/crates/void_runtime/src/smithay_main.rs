//! Smithay-based compositor main loop
//!
//! This module provides the main loop when running with the Smithay compositor.
//! It replaces the winit-based rendering with direct DRM access via Smithay.
//!
//! Key responsibilities:
//! - Input routing from libinput to apps
//! - Render graph integration for layer composition
//! - Frame timing and presentation

use std::sync::Arc;
use std::collections::HashMap;

use void_compositor::{
    Compositor, CompositorConfig, FrameScheduler,
    backend::{detect_available_backends, select_backend, BackendSelector},
    InputEvent as SmithayInputEvent,
    input::{
        KeyboardEvent, PointerEvent as SmithayPointerEvent, TouchEvent as SmithayTouchEvent,
        KeyState, ButtonState, PointerButton,
    },
};
use void_kernel::{LayerId, RenderGraph};

use crate::runtime::Runtime;
use crate::input::{
    InputManager, InputTarget,
    UnifiedInputEvent, UnifiedKeyEvent, UnifiedButtonState, UnifiedMouseButton,
    InputModifiers, ShortcutAction,
};

/// Convert Smithay input events to unified input events
fn convert_smithay_input(event: SmithayInputEvent) -> Option<UnifiedInputEvent> {
    match event {
        SmithayInputEvent::Keyboard(ke) => {
            Some(UnifiedInputEvent::Keyboard(UnifiedKeyEvent {
                keycode: ke.keycode,
                key_name: format!("key_{}", ke.keycode),
                state: match ke.state {
                    KeyState::Pressed => UnifiedButtonState::Pressed,
                    KeyState::Released => UnifiedButtonState::Released,
                },
                modifiers: InputModifiers {
                    shift: ke.modifiers.shift,
                    ctrl: ke.modifiers.ctrl,
                    alt: ke.modifiers.alt,
                    logo: ke.modifiers.logo,
                    caps_lock: ke.modifiers.caps_lock,
                    num_lock: ke.modifiers.num_lock,
                },
                time_ms: ke.time_ms,
                is_repeat: false,
            }))
        }
        SmithayInputEvent::Pointer(pe) => {
            match pe {
                SmithayPointerEvent::Motion { position, delta, time_ms } => {
                    let (x, y) = position.map(|p| (p.x, p.y)).unwrap_or((0.0, 0.0));
                    Some(UnifiedInputEvent::PointerMotion {
                        x,
                        y,
                        delta_x: delta.x,
                        delta_y: delta.y,
                        time_ms,
                    })
                }
                SmithayPointerEvent::Button { button, state, time_ms } => {
                    let unified_button = match button {
                        PointerButton::Left => UnifiedMouseButton::Left,
                        PointerButton::Right => UnifiedMouseButton::Right,
                        PointerButton::Middle => UnifiedMouseButton::Middle,
                        PointerButton::Other(n) => UnifiedMouseButton::Other(n),
                    };
                    Some(UnifiedInputEvent::PointerButton {
                        button: unified_button,
                        state: match state {
                            ButtonState::Pressed => UnifiedButtonState::Pressed,
                            ButtonState::Released => UnifiedButtonState::Released,
                        },
                        time_ms,
                    })
                }
                SmithayPointerEvent::Axis { horizontal, vertical, time_ms, .. } => {
                    Some(UnifiedInputEvent::Scroll {
                        delta_x: horizontal as f32,
                        delta_y: vertical as f32,
                        time_ms,
                    })
                }
            }
        }
        SmithayInputEvent::Touch(te) => {
            // Convert touch events
            match te {
                SmithayTouchEvent::Down { slot, position, time_ms } => {
                    Some(UnifiedInputEvent::Touch(crate::input::UnifiedTouchEvent::Down {
                        id: slot as u64,
                        x: position.x,
                        y: position.y,
                        time_ms,
                    }))
                }
                SmithayTouchEvent::Motion { slot, position, time_ms } => {
                    Some(UnifiedInputEvent::Touch(crate::input::UnifiedTouchEvent::Motion {
                        id: slot as u64,
                        x: position.x,
                        y: position.y,
                        time_ms,
                    }))
                }
                SmithayTouchEvent::Up { slot, time_ms } => {
                    Some(UnifiedInputEvent::Touch(crate::input::UnifiedTouchEvent::Up {
                        id: slot as u64,
                        time_ms,
                    }))
                }
                SmithayTouchEvent::Cancel { slot, time_ms } => {
                    Some(UnifiedInputEvent::Touch(crate::input::UnifiedTouchEvent::Cancel {
                        id: slot as u64,
                        time_ms,
                    }))
                }
            }
        }
        SmithayInputEvent::Device(_) => None, // Device hotplug events handled elsewhere
    }
}

/// Layer renderer for Smithay compositor
///
/// Manages GPU resources for rendering the render graph layers.
pub struct SmithayLayerRenderer {
    /// Layer textures (rendered content from each app)
    layer_textures: HashMap<LayerId, LayerTexture>,
    /// Compositor shader for blending layers
    compositor_pipeline: Option<()>, // Placeholder - would be a wgpu pipeline
    /// Screen size
    screen_size: (u32, u32),
}

/// Texture representing a rendered layer
struct LayerTexture {
    /// Layer ID
    layer_id: LayerId,
    /// Last rendered frame
    last_frame: u64,
    /// Texture is dirty (needs re-render)
    dirty: bool,
}

impl SmithayLayerRenderer {
    /// Create a new layer renderer
    pub fn new(screen_size: (u32, u32)) -> Self {
        Self {
            layer_textures: HashMap::new(),
            compositor_pipeline: None,
            screen_size,
        }
    }

    /// Resize the renderer
    pub fn resize(&mut self, width: u32, height: u32) {
        self.screen_size = (width, height);
        // Invalidate all layer textures on resize
        for texture in self.layer_textures.values_mut() {
            texture.dirty = true;
        }
    }

    /// Render the render graph to the output
    ///
    /// This composites all layers in the render graph according to their
    /// priority and blend modes.
    pub fn render(&mut self, render_graph: &RenderGraph, frame: u64) {
        // Ensure we have textures for all layers
        for layer_id in &render_graph.layers {
            self.layer_textures.entry(*layer_id).or_insert_with(|| {
                LayerTexture {
                    layer_id: *layer_id,
                    last_frame: 0,
                    dirty: true,
                }
            });
        }

        // Remove textures for layers no longer in the graph
        self.layer_textures.retain(|id, _| render_graph.layers.contains(id));

        // Render each dirty layer
        for layer_id in &render_graph.layers {
            if let Some(texture) = self.layer_textures.get_mut(layer_id) {
                if texture.dirty || texture.last_frame < frame.saturating_sub(1) {
                    // In a real implementation, this would:
                    // 1. Get the layer's render commands from the app
                    // 2. Execute them to a texture
                    // 3. Mark the texture as clean
                    texture.last_frame = frame;
                    texture.dirty = false;
                    log::trace!("Rendered layer {:?} for frame {}", layer_id, frame);
                }
            }
        }

        // Composite all layers to the output
        // In a real implementation, this would:
        // 1. Clear the output to the background color
        // 2. For each layer in order:
        //    a. Sample the layer's texture
        //    b. Apply the layer's blend mode
        //    c. Blend with the accumulated output
        // 3. Present the final composited image

        log::trace!(
            "Composited {} layers for frame {}",
            render_graph.layers.len(),
            frame
        );
    }

    /// Get the number of cached layer textures
    pub fn texture_count(&self) -> usize {
        self.layer_textures.len()
    }
}

/// Run the Smithay-based compositor
pub fn run_smithay() {
    log::info!("Starting Smithay compositor");

    // Detect available backends
    let available = detect_available_backends();
    log::info!("Available backends: {:?}", available.iter().map(|b| b.name()).collect::<Vec<_>>());

    // Select best backend (prefer DRM for direct access)
    let backend = match select_backend(BackendSelector::Auto, &available) {
        Ok(b) => {
            log::info!("Selected backend: {}", b.name());
            b
        }
        Err(e) => {
            log::error!("Failed to select backend: {}", e);
            return;
        }
    };

    // Create compositor configuration
    let config = CompositorConfig {
        target_fps: 60,
        vsync: true,
        allow_tearing: false,
        xwayland: false,
    };

    // Create compositor
    let mut compositor = match Compositor::new(config) {
        Ok(c) => c,
        Err(e) => {
            log::error!("Failed to create compositor: {}", e);
            log::info!("Falling back to stub compositor for development");

            // On non-Linux or when Smithay fails, we get the stub
            match Compositor::new(CompositorConfig::default()) {
                Ok(c) => c,
                Err(e) => {
                    log::error!("Even stub compositor failed: {}", e);
                    return;
                }
            }
        }
    };

    // Log capabilities
    let caps = compositor.capabilities();
    log::info!(
        "Compositor capabilities: {}x{} @ {:?}Hz, {} displays",
        caps.current_resolution.0,
        caps.current_resolution.1,
        caps.refresh_rates,
        caps.display_count
    );

    // Create runtime
    let mut runtime = Runtime::new();

    // Create input manager for routing
    let mut input_manager = InputManager::new();
    input_manager.set_screen_size(caps.current_resolution.0, caps.current_resolution.1);

    // Create layer renderer
    let mut layer_renderer = SmithayLayerRenderer::new(caps.current_resolution);

    // Shell state
    let mut show_shell = true;
    let mut shell_input = String::new();

    // Frame counter
    let mut frame: u64 = 0;
    let mut last_time = std::time::Instant::now();

    // Install signal handler for graceful shutdown
    let running = Arc::new(std::sync::atomic::AtomicBool::new(true));
    let r = running.clone();

    if let Err(e) = ctrlc::set_handler(move || {
        log::info!("Received Ctrl+C, shutting down...");
        r.store(false, std::sync::atomic::Ordering::SeqCst);
    }) {
        log::warn!("Failed to set Ctrl+C handler: {}", e);
    }

    log::info!("Smithay compositor running. Press Ctrl+C to exit, ` to toggle shell.");

    // Main loop
    while compositor.is_running() && running.load(std::sync::atomic::Ordering::SeqCst) {
        // Dispatch compositor events (input, DRM, etc.)
        if let Err(e) = compositor.dispatch() {
            log::error!("Compositor dispatch error: {}", e);
            break;
        }

        // Poll and route input events
        let input_events = compositor.poll_input();
        for smithay_event in input_events {
            if let Some(unified_event) = convert_smithay_input(smithay_event) {
                // Handle shell-specific input when shell is focused
                let handled = handle_shell_input(
                    &unified_event,
                    &mut show_shell,
                    &mut shell_input,
                    &mut runtime,
                );

                if !handled {
                    // Route to the appropriate target
                    input_manager.dispatch(unified_event);
                }
            }
        }

        // Process routed input events
        while let Some((event, target)) = input_manager.poll() {
            match target {
                InputTarget::Shell => {
                    // Shell already handled above
                    log::trace!("Input to shell: {:?}", event);
                }
                InputTarget::App(app_id) => {
                    // Forward to app (would go through kernel's app manager)
                    log::trace!("Input to app {:?}: {:?}", app_id, event);
                }
                InputTarget::Layer(layer_id) => {
                    // Forward to layer owner
                    log::trace!("Input to layer {:?}: {:?}", layer_id, event);
                }
                InputTarget::Compositor => {
                    // Handle compositor-level shortcuts
                    if let UnifiedInputEvent::Keyboard(ref ke) = event {
                        if ke.state == UnifiedButtonState::Pressed {
                            if let Some(action) = input_manager.check_shortcut(&ke.key_name, &ke.modifiers) {
                                match action {
                                    ShortcutAction::ToggleShell => {
                                        show_shell = !show_shell;
                                        log::info!("Shell overlay: {}", if show_shell { "ON" } else { "OFF" });
                                    }
                                    ShortcutAction::Quit => {
                                        log::info!("Quit shortcut pressed");
                                        runtime.shutdown();
                                    }
                                    ShortcutAction::NextWindow => {
                                        log::info!("Next window shortcut pressed");
                                        // Would cycle focus to next app
                                    }
                                    ShortcutAction::PreviousWindow => {
                                        log::info!("Previous window shortcut pressed");
                                        // Would cycle focus to previous app
                                    }
                                    ShortcutAction::Custom(ref name) => {
                                        log::info!("Custom shortcut: {}", name);
                                    }
                                }
                            }
                        }
                    }
                }
                InputTarget::None => {
                    // Discard
                }
            }
        }

        // Check for runtime shutdown
        if runtime.state() == void_kernel::KernelState::Stopped {
            log::info!("Runtime shutdown requested");
            break;
        }

        // Check if we should render
        if compositor.should_render() {
            frame += 1;

            // Calculate delta time
            let now = std::time::Instant::now();
            let delta = (now - last_time).as_secs_f32();
            last_time = now;

            // Update runtime
            runtime.update(delta, frame);

            // Get the render graph from runtime
            let render_graph = runtime.render_graph();

            // Begin frame
            let target = match compositor.begin_frame() {
                Ok(t) => t,
                Err(e) => {
                    log::error!("Failed to begin frame: {}", e);
                    continue;
                }
            };

            // Render layers from the render graph
            layer_renderer.render(render_graph, frame);

            // If shell is visible, it would be rendered as an overlay here
            if show_shell {
                // Build shell output
                let mut shell_lines = runtime.status_text();
                if !shell_input.is_empty() {
                    shell_lines.push(format!("> {}", shell_input));
                }
                // Shell overlay rendering would happen here
                log::trace!("Shell overlay: {} lines", shell_lines.len());
            }

            // End frame (presents to display)
            if let Err(e) = compositor.end_frame(target) {
                log::error!("Failed to end frame: {}", e);
            }

            // Log status periodically
            if frame % 300 == 1 {
                let scheduler = compositor.frame_scheduler();
                log::info!(
                    "Frame {}: FPS={:.1}, target={}Hz, frame_time={:.2}ms, layers={}, textures={}",
                    frame,
                    scheduler.current_fps(),
                    scheduler.target_fps(),
                    scheduler.average_frame_time().as_secs_f64() * 1000.0,
                    render_graph.layers.len(),
                    layer_renderer.texture_count()
                );
            }
        }
    }

    log::info!("Smithay compositor shutdown");
    runtime.shutdown();
}

/// Handle shell-specific input
///
/// Returns true if the input was consumed by the shell.
fn handle_shell_input(
    event: &UnifiedInputEvent,
    show_shell: &mut bool,
    shell_input: &mut String,
    runtime: &mut Runtime,
) -> bool {
    if let UnifiedInputEvent::Keyboard(ke) = event {
        if ke.state != UnifiedButtonState::Pressed {
            return false;
        }

        // Check for backtick to toggle shell
        if ke.key_name == "`" {
            *show_shell = !*show_shell;
            log::info!("Shell overlay: {}", if *show_shell { "ON" } else { "OFF" });
            return true;
        }

        // Only handle other input if shell is visible
        if !*show_shell {
            return false;
        }

        match ke.key_name.as_str() {
            "Enter" | "Return" => {
                if !shell_input.is_empty() {
                    runtime.execute_command(shell_input);
                    shell_input.clear();
                }
                return true;
            }
            "Backspace" => {
                shell_input.pop();
                return true;
            }
            "Escape" => {
                if !shell_input.is_empty() {
                    shell_input.clear();
                    return true;
                }
                // Let Escape propagate if input is empty
                return false;
            }
            key if key.len() == 1 => {
                // Single character input
                if let Some(c) = key.chars().next() {
                    if c.is_ascii() && !c.is_control() {
                        shell_input.push(c);
                        return true;
                    }
                }
            }
            _ => {}
        }
    }

    false
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_backend_detection() {
        let available = detect_available_backends();
        // At minimum, headless should always be available
        assert!(!available.is_empty());
    }

    #[test]
    fn test_layer_renderer_creation() {
        let renderer = SmithayLayerRenderer::new((1920, 1080));
        assert_eq!(renderer.screen_size, (1920, 1080));
        assert_eq!(renderer.texture_count(), 0);
    }

    #[test]
    fn test_layer_renderer_resize() {
        let mut renderer = SmithayLayerRenderer::new((1920, 1080));
        renderer.resize(2560, 1440);
        assert_eq!(renderer.screen_size, (2560, 1440));
    }
}
