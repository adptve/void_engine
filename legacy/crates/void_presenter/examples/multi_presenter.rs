//! Multi-presenter example
//!
//! Demonstrates:
//! - Using PresenterManager for multiple presenters
//! - Multiple windows with separate presenters
//! - Synchronized frame presentation
//! - Presenter disconnection and reconnection

use void_presenter::{
    DesktopPresenter, PresenterId, PresenterConfig, Presenter, PresenterManager,
    PresentMode, SurfaceFormat,
};

use winit::{
    application::ApplicationHandler,
    event::WindowEvent,
    event_loop::{ActiveEventLoop, ControlFlow, EventLoop},
    window::{Window, WindowAttributes, WindowId},
};

use std::sync::Arc;
use std::collections::HashMap;

struct App {
    windows: HashMap<WindowId, Arc<Window>>,
    presenter_manager: PresenterManager,
    window_to_presenter: HashMap<WindowId, PresenterId>,
    frame_count: u64,
}

impl App {
    fn new() -> Self {
        Self {
            windows: HashMap::new(),
            presenter_manager: PresenterManager::new(),
            window_to_presenter: HashMap::new(),
            frame_count: 0,
        }
    }

    fn create_window(&mut self, event_loop: &ActiveEventLoop, title: &str, position: (i32, i32)) {
        let window_attrs = WindowAttributes::default()
            .with_title(title)
            .with_inner_size(winit::dpi::LogicalSize::new(800, 600))
            .with_position(winit::dpi::PhysicalPosition::new(position.0, position.1));

        let window = Arc::new(event_loop.create_window(window_attrs).unwrap());
        let window_id = window.id();

        // Create presenter for this window
        let presenter_id = self.presenter_manager.allocate_id();

        let config = PresenterConfig {
            format: SurfaceFormat::Bgra8UnormSrgb,
            present_mode: PresentMode::Fifo,
            size: (800, 600),
            enable_hdr: false,
            target_frame_rate: 60,
            allow_tearing: false,
        };

        let presenter = pollster::block_on(async {
            DesktopPresenter::with_config(presenter_id, window.clone(), config)
                .await
                .expect("Failed to create presenter")
        });

        log::info!("Created presenter {:?} for window '{}'", presenter_id, title);

        self.presenter_manager.register(Box::new(presenter));
        self.windows.insert(window_id, window.clone());
        self.window_to_presenter.insert(window_id, presenter_id);

        window.request_redraw();
    }
}

impl ApplicationHandler for App {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        if self.windows.is_empty() {
            log::info!("Creating multiple windows...");

            // Create three windows
            self.create_window(event_loop, "Void Presenter - Window 1", (100, 100));
            self.create_window(event_loop, "Void Presenter - Window 2", (920, 100));
            self.create_window(event_loop, "Void Presenter - Window 3", (100, 720));

            log::info!("Created {} presenters", self.presenter_manager.count());
        }
    }

    fn window_event(
        &mut self,
        event_loop: &ActiveEventLoop,
        window_id: WindowId,
        event: WindowEvent,
    ) {
        match event {
            WindowEvent::CloseRequested => {
                // Remove this window's presenter
                if let Some(presenter_id) = self.window_to_presenter.remove(&window_id) {
                    self.presenter_manager.unregister(presenter_id);
                    self.windows.remove(&window_id);

                    log::info!("Closed presenter {:?}, {} remaining", presenter_id, self.presenter_manager.count());
                }

                // Exit if all windows closed
                if self.windows.is_empty() {
                    log::info!("All windows closed, exiting");
                    event_loop.exit();
                }
            }

            WindowEvent::Resized(physical_size) => {
                if let Some(&presenter_id) = self.window_to_presenter.get(&window_id) {
                    // Note: In real implementation, you'd need to get mutable access to the specific presenter
                    log::info!("Window {:?} resized to {}x{}", window_id, physical_size.width, physical_size.height);
                }
            }

            WindowEvent::RedrawRequested => {
                // Begin frames on all presenters
                let frames = self.presenter_manager.begin_all_frames();

                self.frame_count += 1;

                // Process each frame
                let mut frames_to_present = Vec::new();
                for (presenter_id, result) in frames {
                    match result {
                        Ok(mut frame) => {
                            // Mark render (in real app, you'd render different content per window)
                            frame.begin_render();
                            frame.end_render();

                            frames_to_present.push((presenter_id, frame));
                        }
                        Err(e) => {
                            log::error!("Failed to begin frame for presenter {:?}: {:?}", presenter_id, e);
                        }
                    }
                }

                // Present all frames
                let results = self.presenter_manager.present_all(frames_to_present);

                // Check for errors
                for (presenter_id, result) in results {
                    if let Err(e) = result {
                        log::error!("Failed to present frame for presenter {:?}: {:?}", presenter_id, e);
                    }
                }

                // Log every 60 frames
                if self.frame_count % 60 == 0 {
                    log::info!("Presented {} frames across {} presenters",
                        self.frame_count,
                        self.presenter_manager.count()
                    );
                }

                // Request next frame for all windows
                for window in self.windows.values() {
                    window.request_redraw();
                }
            }

            WindowEvent::KeyboardInput { event, .. } => {
                if event.physical_key == winit::keyboard::PhysicalKey::Code(winit::keyboard::KeyCode::Escape) {
                    log::info!("Escape pressed, closing all windows");
                    event_loop.exit();
                }

                // Press 'S' to show presenter stats
                if event.physical_key == winit::keyboard::PhysicalKey::Code(winit::keyboard::KeyCode::KeyS) {
                    log::info!("=== Presenter Statistics ===");
                    log::info!("Active presenters: {}", self.presenter_manager.count());
                    log::info!("Total frames: {}", self.frame_count);
                    log::info!("Presenter IDs: {:?}", self.presenter_manager.all_ids());
                }

                // Press 'R' to test rehydration on all presenters
                if event.physical_key == winit::keyboard::PhysicalKey::Code(winit::keyboard::KeyCode::KeyR) {
                    log::info!("Testing rehydration on all presenters...");

                    let states = self.presenter_manager.rehydration_states();
                    log::info!("Captured {} presenter states", states.len());

                    for (id, state) in states {
                        log::info!("Presenter {:?} state: {:?}", id, state);
                    }

                    log::info!("Rehydration test complete");
                }
            }

            _ => {}
        }
    }
}

fn main() {
    // Initialize logging
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    log::info!("Starting multi-presenter example");
    log::info!("Commands:");
    log::info!("  ESC - Exit");
    log::info!("  S   - Show presenter statistics");
    log::info!("  R   - Test rehydration");
    log::info!("  Close individual windows to remove presenters");

    let event_loop = EventLoop::new().unwrap();
    event_loop.set_control_flow(ControlFlow::Poll);

    let mut app = App::new();
    event_loop.run_app(&mut app).unwrap();
}
