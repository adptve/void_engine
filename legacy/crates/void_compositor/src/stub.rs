//! Stub compositor for non-Linux platforms
//!
//! Provides a minimal implementation that allows the code to compile
//! on Windows/macOS for development purposes.

use crate::{
    CompositorConfig, CompositorCapabilities, CompositorError,
    FrameScheduler, InputEvent, RenderTarget, RenderFormat,
};

/// Stub compositor for non-Linux platforms
pub struct Compositor {
    config: CompositorConfig,
    frame_scheduler: FrameScheduler,
    frame_number: u64,
}

impl Compositor {
    /// Create a new stub compositor
    pub fn new(config: CompositorConfig) -> Result<Self, CompositorError> {
        log::warn!("Using stub compositor - Smithay is only available on Linux");

        let frame_scheduler = FrameScheduler::new(config.target_fps);

        Ok(Self {
            config,
            frame_scheduler,
            frame_number: 0,
        })
    }

    /// Get compositor capabilities
    pub fn capabilities(&self) -> CompositorCapabilities {
        CompositorCapabilities {
            refresh_rates: vec![60],
            max_resolution: (1920, 1080),
            current_resolution: (1920, 1080),
            vrr_supported: false,
            hdr_supported: false,
            display_count: 1,
        }
    }

    /// Run one iteration of the event loop
    pub fn dispatch(&mut self) -> Result<(), CompositorError> {
        // Stub: just advance frame
        std::thread::sleep(std::time::Duration::from_millis(16));
        self.frame_scheduler.on_frame_callback();
        Ok(())
    }

    /// Check if a frame should be rendered
    pub fn should_render(&self) -> bool {
        self.frame_scheduler.should_render()
    }

    /// Begin a new frame
    pub fn begin_frame(&mut self) -> Result<StubRenderTarget, CompositorError> {
        self.frame_number = self.frame_scheduler.begin_frame();
        Ok(StubRenderTarget {
            size: (1920, 1080),
        })
    }

    /// End the current frame
    pub fn end_frame(&mut self, _target: StubRenderTarget) -> Result<(), CompositorError> {
        self.frame_scheduler.end_frame();
        Ok(())
    }

    /// Get the frame scheduler
    pub fn frame_scheduler(&self) -> &FrameScheduler {
        &self.frame_scheduler
    }

    /// Get mutable frame scheduler
    pub fn frame_scheduler_mut(&mut self) -> &mut FrameScheduler {
        &mut self.frame_scheduler
    }

    /// Poll for input events
    pub fn poll_input(&mut self) -> Vec<InputEvent> {
        // Stub: no input
        Vec::new()
    }

    /// Get current frame number
    pub fn frame_number(&self) -> u64 {
        self.frame_number
    }

    /// Check if compositor is running
    pub fn is_running(&self) -> bool {
        true
    }

    /// Request shutdown
    pub fn shutdown(&mut self) {
        log::info!("Stub compositor shutdown requested");
    }
}

/// Stub render target
pub struct StubRenderTarget {
    size: (u32, u32),
}

impl RenderTarget for StubRenderTarget {
    fn size(&self) -> (u32, u32) {
        self.size
    }

    fn format(&self) -> RenderFormat {
        RenderFormat::Bgra8UnormSrgb
    }

    fn present(&mut self) -> Result<(), CompositorError> {
        Ok(())
    }
}
