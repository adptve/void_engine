//! Basic desktop presenter example
//!
//! Demonstrates:
//! - Creating a desktop presenter
//! - Frame loop with begin/present
//! - Proper resize handling
//! - Rehydration support

use void_presenter::{
    DesktopPresenter, PresenterId, PresenterConfig, Presenter,
    PresentMode, SurfaceFormat,
};

use winit::{
    application::ApplicationHandler,
    event::WindowEvent,
    event_loop::{ActiveEventLoop, ControlFlow, EventLoop},
    window::{Window, WindowAttributes},
};

use std::sync::Arc;

struct App {
    window: Option<Arc<Window>>,
    presenter: Option<DesktopPresenter>,
    frame_count: u64,
}

impl App {
    fn new() -> Self {
        Self {
            window: None,
            presenter: None,
            frame_count: 0,
        }
    }
}

impl ApplicationHandler for App {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        if self.window.is_none() {
            let window_attrs = WindowAttributes::default()
                .with_title("Void Desktop Presenter - Basic Example")
                .with_inner_size(winit::dpi::LogicalSize::new(1280, 720));

            let window = Arc::new(event_loop.create_window(window_attrs).unwrap());

            // Create presenter
            let id = PresenterId::new(1);
            let config = PresenterConfig {
                format: SurfaceFormat::Bgra8UnormSrgb,
                present_mode: PresentMode::Fifo,
                size: (1280, 720),
                enable_hdr: false,
                target_frame_rate: 60,
                allow_tearing: false,
            };

            let presenter = pollster::block_on(async {
                DesktopPresenter::with_config(id, window.clone(), config)
                    .await
                    .expect("Failed to create presenter")
            });

            log::info!("Desktop presenter created successfully");
            log::info!("Capabilities: {:#?}", presenter.capabilities());

            self.window = Some(window.clone());
            self.presenter = Some(presenter);

            window.request_redraw();
        }
    }

    fn window_event(
        &mut self,
        event_loop: &ActiveEventLoop,
        _window_id: winit::window::WindowId,
        event: WindowEvent,
    ) {
        match event {
            WindowEvent::CloseRequested => {
                log::info!("Close requested, exiting");
                event_loop.exit();
            }

            WindowEvent::Resized(physical_size) => {
                if let Some(presenter) = &mut self.presenter {
                    log::info!("Window resized to: {}x{}", physical_size.width, physical_size.height);

                    if let Err(e) = presenter.resize(physical_size.width, physical_size.height) {
                        log::error!("Failed to resize presenter: {:?}", e);
                    }
                }
            }

            WindowEvent::RedrawRequested => {
                if let Some(presenter) = &mut self.presenter {
                    // Begin frame
                    let mut frame = match presenter.begin_frame() {
                        Ok(frame) => frame,
                        Err(e) => {
                            log::error!("Failed to begin frame: {:?}", e);
                            return;
                        }
                    };

                    self.frame_count += 1;

                    // Mark render as complete (in real app, you'd render here)
                    frame.begin_render();
                    frame.end_render();

                    // Present
                    if let Err(e) = presenter.present(frame) {
                        log::error!("Failed to present frame: {:?}", e);
                    }

                    // Log every 60 frames
                    if self.frame_count % 60 == 0 {
                        log::info!("Presented {} frames", self.frame_count);
                    }
                }

                // Request next frame
                if let Some(window) = &self.window {
                    window.request_redraw();
                }
            }

            WindowEvent::KeyboardInput { event, .. } => {
                if event.physical_key == winit::keyboard::PhysicalKey::Code(winit::keyboard::KeyCode::Escape) {
                    log::info!("Escape pressed, exiting");
                    event_loop.exit();
                }

                // Press 'R' to test rehydration
                if event.physical_key == winit::keyboard::PhysicalKey::Code(winit::keyboard::KeyCode::KeyR) {
                    if let Some(presenter) = &mut self.presenter {
                        log::info!("Testing rehydration...");

                        // Capture state
                        let state = presenter.rehydration_state();
                        log::info!("Captured state: {:?}", state);

                        // Restore state (simulates disconnect/reconnect)
                        if let Err(e) = presenter.rehydrate(state) {
                            log::error!("Rehydration failed: {:?}", e);
                        } else {
                            log::info!("Rehydration successful!");
                        }
                    }
                }
            }

            _ => {}
        }
    }
}

fn main() {
    // Initialize logging
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    log::info!("Starting desktop presenter basic example");
    log::info!("Press ESC to exit");
    log::info!("Press R to test rehydration");

    let event_loop = EventLoop::new().unwrap();
    event_loop.set_control_flow(ControlFlow::Poll);

    let mut app = App::new();
    event_loop.run_app(&mut app).unwrap();
}
