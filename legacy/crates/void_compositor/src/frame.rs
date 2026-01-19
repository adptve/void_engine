//! Frame scheduling and timing
//!
//! This module provides frame timing control, which is a key differentiator
//! for the Metaverse OS. It allows precise control over when frames are
//! rendered and presented.

use std::time::{Duration, Instant};
use std::collections::VecDeque;
use crate::vrr::VrrConfig;

/// Frame state
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameState {
    /// Waiting for frame callback from compositor
    WaitingForCallback,
    /// Ready to render
    ReadyToRender,
    /// Currently rendering
    Rendering,
    /// Waiting for presentation
    WaitingForPresent,
    /// Frame was presented
    Presented,
    /// Frame was dropped (missed deadline)
    Dropped,
}

/// Presentation feedback from the display
#[derive(Debug, Clone)]
pub struct PresentationFeedback {
    /// When the frame was actually presented (display scanout)
    pub presented_at: Instant,
    /// Sequence number
    pub sequence: u64,
    /// Time from commit to presentation
    pub latency: Duration,
    /// Was VSync used
    pub vsync: bool,
    /// Refresh rate at presentation
    pub refresh_rate: u32,
}

/// Frame scheduler - controls when frames are rendered
///
/// This is where Metaverse OS adds value on top of Smithay.
/// Smithay handles the low-level compositor work, we handle
/// the high-level frame scheduling policy.
pub struct FrameScheduler {
    /// Target refresh rate
    target_fps: u32,
    /// Frame budget (time allowed for rendering)
    frame_budget: Duration,
    /// When the last frame was presented
    last_presentation: Instant,
    /// Current frame number
    frame_number: u64,
    /// Frame state
    state: FrameState,
    /// Recent frame times for statistics
    frame_times: VecDeque<Duration>,
    /// Maximum history size
    max_history: usize,
    /// Presentation feedback history
    feedback_history: VecDeque<PresentationFeedback>,
    /// Callback ready flag
    callback_ready: bool,
    /// VRR configuration (if supported)
    vrr_config: Option<VrrConfig>,
    /// Content velocity estimation (for VRR adaptation)
    content_velocity: f32,
}

impl FrameScheduler {
    /// Create a new frame scheduler
    pub fn new(target_fps: u32) -> Self {
        let frame_budget = if target_fps > 0 {
            Duration::from_secs_f64(1.0 / target_fps as f64)
        } else {
            Duration::from_millis(16) // ~60fps default
        };

        Self {
            target_fps,
            frame_budget,
            last_presentation: Instant::now(),
            frame_number: 0,
            state: FrameState::WaitingForCallback,
            frame_times: VecDeque::with_capacity(120),
            max_history: 120,
            feedback_history: VecDeque::with_capacity(10),
            callback_ready: false,
            vrr_config: None,
            content_velocity: 0.0,
        }
    }

    /// Called when compositor signals frame callback
    pub fn on_frame_callback(&mut self) {
        self.callback_ready = true;
        self.state = FrameState::ReadyToRender;
    }

    /// Check if we should render a frame now
    pub fn should_render(&self) -> bool {
        self.callback_ready && self.state == FrameState::ReadyToRender
    }

    /// Begin a new frame
    pub fn begin_frame(&mut self) -> u64 {
        self.state = FrameState::Rendering;
        self.callback_ready = false;
        self.frame_number += 1;
        self.frame_number
    }

    /// End the current frame (called after commit)
    pub fn end_frame(&mut self) {
        self.state = FrameState::WaitingForPresent;
    }

    /// Called when presentation feedback is received
    pub fn on_presentation_feedback(&mut self, feedback: PresentationFeedback) {
        let frame_time = feedback.presented_at.duration_since(self.last_presentation);
        self.last_presentation = feedback.presented_at;

        // Update statistics
        if self.frame_times.len() >= self.max_history {
            self.frame_times.pop_front();
        }
        self.frame_times.push_back(frame_time);

        // Store feedback
        if self.feedback_history.len() >= 10 {
            self.feedback_history.pop_front();
        }
        self.feedback_history.push_back(feedback);

        self.state = FrameState::Presented;
    }

    /// Mark frame as dropped (missed deadline)
    pub fn drop_frame(&mut self) {
        self.state = FrameState::Dropped;
        log::warn!("Frame {} dropped", self.frame_number);
    }

    /// Get current frame number
    pub fn frame_number(&self) -> u64 {
        self.frame_number
    }

    /// Get current frame state
    pub fn state(&self) -> FrameState {
        self.state
    }

    /// Get target FPS
    pub fn target_fps(&self) -> u32 {
        self.target_fps
    }

    /// Set target FPS
    pub fn set_target_fps(&mut self, fps: u32) {
        self.target_fps = fps;
        self.frame_budget = if fps > 0 {
            Duration::from_secs_f64(1.0 / fps as f64)
        } else {
            Duration::from_millis(16)
        };
    }

    /// Get frame budget
    pub fn frame_budget(&self) -> Duration {
        self.frame_budget
    }

    /// Get average frame time
    pub fn average_frame_time(&self) -> Duration {
        if self.frame_times.is_empty() {
            return self.frame_budget;
        }
        let total: Duration = self.frame_times.iter().sum();
        total / self.frame_times.len() as u32
    }

    /// Get current FPS (based on recent frames)
    pub fn current_fps(&self) -> f64 {
        let avg = self.average_frame_time();
        if avg.as_secs_f64() > 0.0 {
            1.0 / avg.as_secs_f64()
        } else {
            0.0
        }
    }

    /// Get frame time percentile (for performance analysis)
    pub fn frame_time_percentile(&self, percentile: f64) -> Duration {
        if self.frame_times.is_empty() {
            return self.frame_budget;
        }

        let mut sorted: Vec<_> = self.frame_times.iter().cloned().collect();
        sorted.sort();

        let index = ((percentile / 100.0) * (sorted.len() - 1) as f64) as usize;
        sorted[index.min(sorted.len() - 1)]
    }

    /// Get 99th percentile frame time (useful for judging smoothness)
    pub fn frame_time_99th(&self) -> Duration {
        self.frame_time_percentile(99.0)
    }

    /// Check if we're hitting target framerate
    pub fn hitting_target(&self) -> bool {
        if self.target_fps == 0 {
            return true;
        }
        let target_time = Duration::from_secs_f64(1.0 / self.target_fps as f64);
        self.average_frame_time() <= target_time * 11 / 10 // 10% tolerance
    }

    /// Get latest presentation feedback
    pub fn latest_feedback(&self) -> Option<&PresentationFeedback> {
        self.feedback_history.back()
    }

    /// Calculate time remaining in frame budget
    pub fn time_remaining(&self) -> Duration {
        let elapsed = self.last_presentation.elapsed();
        let budget = self.effective_frame_budget();
        if elapsed >= budget {
            Duration::ZERO
        } else {
            budget - elapsed
        }
    }

    /// Set VRR configuration
    pub fn set_vrr_config(&mut self, config: Option<VrrConfig>) {
        self.vrr_config = config;
        if let Some(vrr) = &self.vrr_config {
            if vrr.is_active() {
                // Update frame budget to match VRR current rate
                self.frame_budget = vrr.frame_time();
                log::info!("VRR enabled: {}", vrr.range_string());
            }
        }
    }

    /// Get VRR configuration
    pub fn vrr_config(&self) -> Option<&VrrConfig> {
        self.vrr_config.as_ref()
    }

    /// Get mutable VRR configuration
    pub fn vrr_config_mut(&mut self) -> Option<&mut VrrConfig> {
        self.vrr_config.as_mut()
    }

    /// Update content velocity for VRR adaptation
    ///
    /// Content velocity is a normalized value (0.0-1.0) that represents
    /// how much the scene is changing. Higher values indicate more motion.
    pub fn update_content_velocity(&mut self, velocity: f32) {
        // Smooth the velocity to avoid rapid changes
        let alpha = 0.1; // Exponential smoothing factor
        self.content_velocity = self.content_velocity * (1.0 - alpha) + velocity.clamp(0.0, 1.0) * alpha;

        // Adapt VRR refresh rate if enabled
        if let Some(vrr) = &mut self.vrr_config {
            vrr.adapt_refresh_rate(self.frame_budget, self.content_velocity);
            self.frame_budget = vrr.frame_time();
        }
    }

    /// Get current content velocity
    pub fn content_velocity(&self) -> f32 {
        self.content_velocity
    }

    /// Get effective frame budget (considering VRR)
    fn effective_frame_budget(&self) -> Duration {
        if let Some(vrr) = &self.vrr_config {
            if vrr.is_active() {
                return vrr.frame_time();
            }
        }
        self.frame_budget
    }

    /// Check if VRR is active
    pub fn is_vrr_active(&self) -> bool {
        self.vrr_config.as_ref().map(|v| v.is_active()).unwrap_or(false)
    }
}

impl Default for FrameScheduler {
    fn default() -> Self {
        Self::new(60)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_frame_scheduler_creation() {
        let scheduler = FrameScheduler::new(60);
        assert_eq!(scheduler.target_fps(), 60);
        assert_eq!(scheduler.frame_number(), 0);
    }

    #[test]
    fn test_frame_lifecycle() {
        let mut scheduler = FrameScheduler::new(60);

        // Initially waiting
        assert_eq!(scheduler.state(), FrameState::WaitingForCallback);
        assert!(!scheduler.should_render());

        // Callback received
        scheduler.on_frame_callback();
        assert_eq!(scheduler.state(), FrameState::ReadyToRender);
        assert!(scheduler.should_render());

        // Begin frame
        let frame = scheduler.begin_frame();
        assert_eq!(frame, 1);
        assert_eq!(scheduler.state(), FrameState::Rendering);

        // End frame
        scheduler.end_frame();
        assert_eq!(scheduler.state(), FrameState::WaitingForPresent);
    }

    #[test]
    fn test_fps_calculation() {
        let scheduler = FrameScheduler::new(60);
        assert_eq!(scheduler.target_fps(), 60);

        let budget = scheduler.frame_budget();
        assert!(budget.as_millis() >= 16 && budget.as_millis() <= 17);
    }
}
