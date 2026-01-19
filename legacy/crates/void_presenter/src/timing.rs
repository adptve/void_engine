//! Frame timing and synchronization
//!
//! Provides frame timing, VSync control, and pacing.

use std::time::{Duration, Instant};
use serde::{Serialize, Deserialize};

/// VSync mode
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum VSync {
    /// VSync disabled - render as fast as possible
    Off,
    /// VSync enabled - sync to display refresh
    On,
    /// Adaptive VSync - VSync when above refresh rate, tear when below
    Adaptive,
}

/// Present mode (Vulkan/wgpu terminology)
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum PresentMode {
    /// Immediate presentation - may tear
    Immediate,
    /// Mailbox - low latency, no tearing, may drop frames
    Mailbox,
    /// FIFO - no tearing, no frame drops, may increase latency
    Fifo,
    /// FIFO Relaxed - FIFO but allows tearing when late
    FifoRelaxed,
}

impl PresentMode {
    /// Check if this mode prevents tearing
    pub fn prevents_tearing(&self) -> bool {
        matches!(self, Self::Mailbox | Self::Fifo)
    }

    /// Check if this mode may drop frames
    pub fn may_drop_frames(&self) -> bool {
        matches!(self, Self::Immediate | Self::Mailbox)
    }

    /// Get description
    pub fn description(&self) -> &'static str {
        match self {
            Self::Immediate => "Immediate (may tear, lowest latency)",
            Self::Mailbox => "Mailbox (no tear, may drop frames)",
            Self::Fifo => "FIFO (no tear, no drops, higher latency)",
            Self::FifoRelaxed => "FIFO Relaxed (no tear normally, may tear when late)",
        }
    }
}

impl From<VSync> for PresentMode {
    fn from(vsync: VSync) -> Self {
        match vsync {
            VSync::Off => PresentMode::Immediate,
            VSync::On => PresentMode::Fifo,
            VSync::Adaptive => PresentMode::FifoRelaxed,
        }
    }
}

/// Frame timing information
#[derive(Debug, Clone)]
pub struct FrameTiming {
    /// Target frame duration
    target_frame_time: Duration,
    /// Last frame start
    last_frame_start: Option<Instant>,
    /// Last frame duration
    last_frame_duration: Duration,
    /// Frame time history for averaging
    frame_times: Vec<Duration>,
    /// Maximum history size
    history_size: usize,
    /// Total elapsed time
    total_elapsed: Duration,
    /// Frame count
    frame_count: u64,
}

impl FrameTiming {
    /// Create new frame timing with target FPS
    pub fn new(target_fps: u32) -> Self {
        Self {
            target_frame_time: Duration::from_secs_f64(1.0 / target_fps.max(1) as f64),
            last_frame_start: None,
            last_frame_duration: Duration::ZERO,
            frame_times: Vec::with_capacity(120),
            history_size: 120,
            total_elapsed: Duration::ZERO,
            frame_count: 0,
        }
    }

    /// Create with unlimited FPS
    pub fn unlimited() -> Self {
        Self {
            target_frame_time: Duration::ZERO,
            last_frame_start: None,
            last_frame_duration: Duration::ZERO,
            frame_times: Vec::with_capacity(120),
            history_size: 120,
            total_elapsed: Duration::ZERO,
            frame_count: 0,
        }
    }

    /// Set target FPS
    pub fn set_target_fps(&mut self, fps: u32) {
        self.target_frame_time = if fps == 0 {
            Duration::ZERO
        } else {
            Duration::from_secs_f64(1.0 / fps as f64)
        };
    }

    /// Get target frame time
    pub fn target_frame_time(&self) -> Duration {
        self.target_frame_time
    }

    /// Get target FPS
    pub fn target_fps(&self) -> f64 {
        if self.target_frame_time.is_zero() {
            f64::INFINITY
        } else {
            1.0 / self.target_frame_time.as_secs_f64()
        }
    }

    /// Mark frame start
    pub fn begin_frame(&mut self) -> Instant {
        let now = Instant::now();

        if let Some(last_start) = self.last_frame_start {
            self.last_frame_duration = now - last_start;
            self.total_elapsed += self.last_frame_duration;

            // Update history
            if self.frame_times.len() >= self.history_size {
                self.frame_times.remove(0);
            }
            self.frame_times.push(self.last_frame_duration);
        }

        self.last_frame_start = Some(now);
        self.frame_count += 1;
        now
    }

    /// Get time to wait before next frame (for frame pacing)
    pub fn time_to_wait(&self) -> Duration {
        if self.target_frame_time.is_zero() {
            return Duration::ZERO;
        }

        let Some(last_start) = self.last_frame_start else {
            return Duration::ZERO;
        };

        let elapsed = Instant::now() - last_start;
        if elapsed < self.target_frame_time {
            self.target_frame_time - elapsed
        } else {
            Duration::ZERO
        }
    }

    /// Wait until next frame target (blocking)
    pub fn wait_for_next_frame(&self) {
        let wait = self.time_to_wait();
        if !wait.is_zero() {
            std::thread::sleep(wait);
        }
    }

    /// Get last frame duration
    pub fn last_frame_duration(&self) -> Duration {
        self.last_frame_duration
    }

    /// Get last frame duration as delta time (seconds)
    pub fn delta_time(&self) -> f32 {
        self.last_frame_duration.as_secs_f32()
    }

    /// Get average frame duration
    pub fn average_frame_duration(&self) -> Duration {
        if self.frame_times.is_empty() {
            self.target_frame_time
        } else {
            let sum: Duration = self.frame_times.iter().sum();
            sum / self.frame_times.len() as u32
        }
    }

    /// Get average FPS
    pub fn average_fps(&self) -> f64 {
        let avg = self.average_frame_duration();
        if avg.is_zero() {
            0.0
        } else {
            1.0 / avg.as_secs_f64()
        }
    }

    /// Get instant FPS (from last frame)
    pub fn instant_fps(&self) -> f64 {
        if self.last_frame_duration.is_zero() {
            0.0
        } else {
            1.0 / self.last_frame_duration.as_secs_f64()
        }
    }

    /// Get frame time percentile (0-100)
    pub fn frame_time_percentile(&self, percentile: u32) -> Duration {
        if self.frame_times.is_empty() {
            return Duration::ZERO;
        }

        let mut sorted = self.frame_times.clone();
        sorted.sort();

        let idx = ((percentile.min(100) as f64 / 100.0) * (sorted.len() - 1) as f64) as usize;
        sorted[idx]
    }

    /// Get total elapsed time
    pub fn total_elapsed(&self) -> Duration {
        self.total_elapsed
    }

    /// Get total frame count
    pub fn frame_count(&self) -> u64 {
        self.frame_count
    }

    /// Reset timing
    pub fn reset(&mut self) {
        self.last_frame_start = None;
        self.last_frame_duration = Duration::ZERO;
        self.frame_times.clear();
        self.total_elapsed = Duration::ZERO;
        self.frame_count = 0;
    }
}

impl Default for FrameTiming {
    fn default() -> Self {
        Self::new(60)
    }
}

/// Frame limiter for CPU-side frame pacing
pub struct FrameLimiter {
    target_frame_time: Duration,
    last_frame: Instant,
    oversleep_compensation: Duration,
}

impl FrameLimiter {
    /// Create new frame limiter with target FPS
    pub fn new(target_fps: u32) -> Self {
        Self {
            target_frame_time: Duration::from_secs_f64(1.0 / target_fps.max(1) as f64),
            last_frame: Instant::now(),
            oversleep_compensation: Duration::ZERO,
        }
    }

    /// Create unlimited (no limiting)
    pub fn unlimited() -> Self {
        Self {
            target_frame_time: Duration::ZERO,
            last_frame: Instant::now(),
            oversleep_compensation: Duration::ZERO,
        }
    }

    /// Set target FPS
    pub fn set_target_fps(&mut self, fps: u32) {
        self.target_frame_time = if fps == 0 {
            Duration::ZERO
        } else {
            Duration::from_secs_f64(1.0 / fps as f64)
        };
    }

    /// Wait for next frame (blocking with busy-wait for accuracy)
    pub fn wait(&mut self) {
        if self.target_frame_time.is_zero() {
            self.last_frame = Instant::now();
            return;
        }

        let elapsed = self.last_frame.elapsed();
        let target = self.target_frame_time.saturating_sub(self.oversleep_compensation);

        if elapsed < target {
            let sleep_time = target - elapsed;

            // Sleep most of the time, then busy-wait for accuracy
            if sleep_time > Duration::from_millis(2) {
                std::thread::sleep(sleep_time - Duration::from_millis(1));
            }

            // Busy-wait for the rest
            while self.last_frame.elapsed() < self.target_frame_time {}
        }

        // Track oversleep for compensation
        let actual_sleep = self.last_frame.elapsed();
        if actual_sleep > self.target_frame_time {
            self.oversleep_compensation = (actual_sleep - self.target_frame_time).min(Duration::from_millis(5));
        } else {
            self.oversleep_compensation = Duration::ZERO;
        }

        self.last_frame = Instant::now();
    }

    /// Mark frame start (for when you don't want to wait)
    pub fn mark_frame(&mut self) {
        self.last_frame = Instant::now();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_present_mode() {
        assert!(PresentMode::Fifo.prevents_tearing());
        assert!(!PresentMode::Immediate.prevents_tearing());
        assert!(PresentMode::Mailbox.may_drop_frames());
        assert!(!PresentMode::Fifo.may_drop_frames());
    }

    #[test]
    fn test_frame_timing() {
        let mut timing = FrameTiming::new(60);

        // Use approximate comparison for floating point
        assert!((timing.target_fps() - 60.0).abs() < 0.01);
        assert!(timing.target_frame_time() > Duration::ZERO);

        timing.begin_frame();
        std::thread::sleep(Duration::from_millis(1));
        timing.begin_frame();

        assert!(timing.last_frame_duration() > Duration::ZERO);
        assert!(timing.frame_count() >= 2);
    }

    #[test]
    fn test_frame_timing_unlimited() {
        let timing = FrameTiming::unlimited();
        assert!(timing.target_fps().is_infinite());
        assert!(timing.time_to_wait().is_zero());
    }

    #[test]
    fn test_frame_limiter() {
        let mut limiter = FrameLimiter::new(1000); // High FPS for test speed
        let start = Instant::now();

        for _ in 0..10 {
            limiter.wait();
        }

        // Should have taken at least ~10ms (10 frames at 1000fps)
        assert!(start.elapsed() >= Duration::from_millis(5));
    }

    #[test]
    fn test_vsync_to_present_mode() {
        assert_eq!(PresentMode::from(VSync::Off), PresentMode::Immediate);
        assert_eq!(PresentMode::from(VSync::On), PresentMode::Fifo);
        assert_eq!(PresentMode::from(VSync::Adaptive), PresentMode::FifoRelaxed);
    }
}
