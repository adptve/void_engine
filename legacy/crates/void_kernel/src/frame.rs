//! Frame timing and context

use std::time::Instant;

/// Frame timing information
#[derive(Debug, Clone)]
pub struct FrameTiming {
    /// Target FPS
    pub target_fps: u32,
    /// Target frame time in seconds
    pub target_frame_time: f32,
    /// Accumulated time for fixed update
    pub accumulator: f32,
    /// Total elapsed time
    pub total_time: f32,
    /// Last frame's delta time
    pub delta_time: f32,
    /// Frame start time
    pub frame_start: Instant,
    /// Average FPS (smoothed)
    pub avg_fps: f32,
    /// Frame count for FPS calculation
    fps_frame_count: u32,
    fps_time_accumulator: f32,
}

impl FrameTiming {
    /// Create new frame timing
    pub fn new(target_fps: u32) -> Self {
        Self {
            target_fps,
            target_frame_time: 1.0 / target_fps as f32,
            accumulator: 0.0,
            total_time: 0.0,
            delta_time: 0.0,
            frame_start: Instant::now(),
            avg_fps: target_fps as f32,
            fps_frame_count: 0,
            fps_time_accumulator: 0.0,
        }
    }

    /// Reset timing (e.g., after pause)
    pub fn reset(&mut self) {
        self.frame_start = Instant::now();
        self.accumulator = 0.0;
    }

    /// Update timing for a new frame
    pub fn update(&mut self, delta_time: f32) {
        self.delta_time = delta_time;
        self.total_time += delta_time;
        self.accumulator += delta_time;
        self.frame_start = Instant::now();

        // Update FPS calculation
        self.fps_frame_count += 1;
        self.fps_time_accumulator += delta_time;
        if self.fps_time_accumulator >= 1.0 {
            self.avg_fps = self.fps_frame_count as f32 / self.fps_time_accumulator;
            self.fps_frame_count = 0;
            self.fps_time_accumulator = 0.0;
        }
    }

    /// Consume fixed timestep from accumulator
    pub fn consume_fixed_step(&mut self, fixed_timestep: f32) -> bool {
        if self.accumulator >= fixed_timestep {
            self.accumulator -= fixed_timestep;
            true
        } else {
            false
        }
    }

    /// Get interpolation factor for rendering between fixed steps
    pub fn interpolation_factor(&self, fixed_timestep: f32) -> f32 {
        self.accumulator / fixed_timestep
    }
}

/// Context for the current frame
#[derive(Debug, Clone, Copy)]
pub struct FrameContext {
    /// Current frame number
    pub frame: u64,
    /// Delta time since last frame
    pub delta_time: f32,
    /// Total time since start
    pub total_time: f32,
    /// Current frame state
    pub state: FrameState,
}

impl FrameContext {
    /// Get smoothed delta time (clamped)
    pub fn smooth_delta(&self) -> f32 {
        self.delta_time.min(0.1)
    }
}

/// State of the current frame
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameState {
    /// Processing logic
    Processing,
    /// Rendering
    Rendering,
    /// Presenting
    Presenting,
    /// Complete
    Complete,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_frame_timing() {
        let mut timing = FrameTiming::new(60);

        // Simulate a few frames
        timing.update(1.0 / 60.0);
        timing.update(1.0 / 60.0);
        timing.update(1.0 / 60.0);

        assert!(timing.total_time > 0.0);
        assert!(timing.delta_time > 0.0);
    }

    #[test]
    fn test_fixed_timestep() {
        let mut timing = FrameTiming::new(60);
        let fixed_step = 1.0 / 60.0;

        // Accumulate enough time for one fixed step
        timing.update(fixed_step);

        assert!(timing.consume_fixed_step(fixed_step));
        assert!(!timing.consume_fixed_step(fixed_step)); // No more steps
    }
}
