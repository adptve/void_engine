//! Frame representation
//!
//! Represents a single presentable frame with timing and state information.

use std::time::{Duration, Instant};
use serde::{Serialize, Deserialize};

/// Frame state
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum FrameState {
    /// Frame is being prepared
    Preparing,
    /// Frame is being rendered
    Rendering,
    /// Frame is ready for presentation
    Ready,
    /// Frame has been presented
    Presented,
    /// Frame was dropped (missed deadline)
    Dropped,
}

/// A single presentable frame
#[derive(Debug)]
pub struct Frame {
    /// Frame number
    number: u64,
    /// Frame state
    state: FrameState,
    /// Target size
    size: (u32, u32),
    /// Frame creation time
    created_at: Instant,
    /// Frame deadline (for latency targeting)
    deadline: Option<Instant>,
    /// Render start time
    render_start: Option<Instant>,
    /// Render end time
    render_end: Option<Instant>,
    /// Present time
    presented_at: Option<Instant>,
    /// User data
    user_data: Option<Box<dyn std::any::Any + Send + Sync>>,
}

impl Frame {
    /// Create a new frame
    pub fn new(number: u64, size: (u32, u32)) -> Self {
        Self {
            number,
            state: FrameState::Preparing,
            size,
            created_at: Instant::now(),
            deadline: None,
            render_start: None,
            render_end: None,
            presented_at: None,
            user_data: None,
        }
    }

    /// Create with deadline
    pub fn with_deadline(mut self, deadline: Instant) -> Self {
        self.deadline = Some(deadline);
        self
    }

    /// Set deadline from target FPS
    pub fn with_target_fps(self, fps: u32) -> Self {
        let frame_time = Duration::from_secs_f64(1.0 / fps as f64);
        let deadline = self.created_at + frame_time;
        self.with_deadline(deadline)
    }

    /// Get frame number
    pub fn number(&self) -> u64 {
        self.number
    }

    /// Get frame state
    pub fn state(&self) -> FrameState {
        self.state
    }

    /// Get frame size
    pub fn size(&self) -> (u32, u32) {
        self.size
    }

    /// Get creation time
    pub fn created_at(&self) -> Instant {
        self.created_at
    }

    /// Get deadline
    pub fn deadline(&self) -> Option<Instant> {
        self.deadline
    }

    /// Check if frame missed its deadline
    pub fn missed_deadline(&self) -> bool {
        if let Some(deadline) = self.deadline {
            Instant::now() > deadline
        } else {
            false
        }
    }

    /// Get time until deadline
    pub fn time_until_deadline(&self) -> Option<Duration> {
        self.deadline.map(|d| {
            let now = Instant::now();
            if now < d {
                d - now
            } else {
                Duration::ZERO
            }
        })
    }

    /// Mark render start
    pub fn begin_render(&mut self) {
        self.state = FrameState::Rendering;
        self.render_start = Some(Instant::now());
    }

    /// Mark render end
    pub fn end_render(&mut self) {
        self.state = FrameState::Ready;
        self.render_end = Some(Instant::now());
    }

    /// Mark as presented
    pub fn mark_presented(&mut self) {
        self.state = FrameState::Presented;
        self.presented_at = Some(Instant::now());
    }

    /// Mark as dropped
    pub fn mark_dropped(&mut self) {
        self.state = FrameState::Dropped;
    }

    /// Get render duration
    pub fn render_duration(&self) -> Option<Duration> {
        match (self.render_start, self.render_end) {
            (Some(start), Some(end)) => Some(end - start),
            _ => None,
        }
    }

    /// Get total frame time (creation to presentation)
    pub fn total_duration(&self) -> Option<Duration> {
        self.presented_at.map(|p| p - self.created_at)
    }

    /// Get latency (creation to now)
    pub fn current_latency(&self) -> Duration {
        Instant::now() - self.created_at
    }

    /// Set user data
    pub fn set_user_data<T: Send + Sync + 'static>(&mut self, data: T) {
        self.user_data = Some(Box::new(data));
    }

    /// Get user data
    pub fn user_data<T: 'static>(&self) -> Option<&T> {
        self.user_data.as_ref().and_then(|d| d.downcast_ref())
    }

    /// Take user data
    pub fn take_user_data<T: 'static>(&mut self) -> Option<T> {
        let data = self.user_data.take()?;
        data.downcast().ok().map(|b| *b)
    }
}

/// Frame output descriptor
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FrameOutput {
    /// Frame number
    pub frame_number: u64,
    /// Frame width
    pub width: u32,
    /// Frame height
    pub height: u32,
    /// Render time in microseconds
    pub render_time_us: u64,
    /// Total time in microseconds
    pub total_time_us: u64,
    /// Whether deadline was missed
    pub missed_deadline: bool,
    /// Whether frame was dropped
    pub dropped: bool,
}

impl FrameOutput {
    /// Create from frame
    pub fn from_frame(frame: &Frame) -> Self {
        Self {
            frame_number: frame.number,
            width: frame.size.0,
            height: frame.size.1,
            render_time_us: frame.render_duration().map(|d| d.as_micros() as u64).unwrap_or(0),
            total_time_us: frame.total_duration().map(|d| d.as_micros() as u64).unwrap_or(0),
            missed_deadline: frame.missed_deadline(),
            dropped: frame.state == FrameState::Dropped,
        }
    }
}

/// Frame statistics
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct FrameStats {
    /// Total frames
    pub total_frames: u64,
    /// Presented frames
    pub presented_frames: u64,
    /// Dropped frames
    pub dropped_frames: u64,
    /// Average render time (microseconds)
    pub avg_render_time_us: f64,
    /// Average total time (microseconds)
    pub avg_total_time_us: f64,
    /// Min render time (microseconds)
    pub min_render_time_us: u64,
    /// Max render time (microseconds)
    pub max_render_time_us: u64,
    /// Deadline miss count
    pub deadline_misses: u64,
}

impl FrameStats {
    /// Update stats with new frame output
    pub fn update(&mut self, output: &FrameOutput) {
        self.total_frames += 1;

        if output.dropped {
            self.dropped_frames += 1;
        } else {
            self.presented_frames += 1;
        }

        if output.missed_deadline {
            self.deadline_misses += 1;
        }

        // Update render time stats
        if output.render_time_us > 0 {
            let n = self.presented_frames as f64;
            self.avg_render_time_us =
                (self.avg_render_time_us * (n - 1.0) + output.render_time_us as f64) / n;

            self.avg_total_time_us =
                (self.avg_total_time_us * (n - 1.0) + output.total_time_us as f64) / n;

            if self.min_render_time_us == 0 || output.render_time_us < self.min_render_time_us {
                self.min_render_time_us = output.render_time_us;
            }
            if output.render_time_us > self.max_render_time_us {
                self.max_render_time_us = output.render_time_us;
            }
        }
    }

    /// Get frame drop rate (0.0 - 1.0)
    pub fn drop_rate(&self) -> f64 {
        if self.total_frames == 0 {
            0.0
        } else {
            self.dropped_frames as f64 / self.total_frames as f64
        }
    }

    /// Get deadline miss rate (0.0 - 1.0)
    pub fn deadline_miss_rate(&self) -> f64 {
        if self.total_frames == 0 {
            0.0
        } else {
            self.deadline_misses as f64 / self.total_frames as f64
        }
    }

    /// Get average FPS based on total time
    pub fn average_fps(&self) -> f64 {
        if self.avg_total_time_us > 0.0 {
            1_000_000.0 / self.avg_total_time_us
        } else {
            0.0
        }
    }

    /// Reset statistics
    pub fn reset(&mut self) {
        *self = Self::default();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::thread;

    #[test]
    fn test_frame_creation() {
        let frame = Frame::new(1, (1920, 1080));
        assert_eq!(frame.number(), 1);
        assert_eq!(frame.size(), (1920, 1080));
        assert_eq!(frame.state(), FrameState::Preparing);
    }

    #[test]
    fn test_frame_lifecycle() {
        let mut frame = Frame::new(1, (800, 600));

        assert_eq!(frame.state(), FrameState::Preparing);

        frame.begin_render();
        assert_eq!(frame.state(), FrameState::Rendering);

        thread::sleep(Duration::from_millis(1));

        frame.end_render();
        assert_eq!(frame.state(), FrameState::Ready);
        assert!(frame.render_duration().is_some());

        frame.mark_presented();
        assert_eq!(frame.state(), FrameState::Presented);
        assert!(frame.total_duration().is_some());
    }

    #[test]
    fn test_frame_deadline() {
        let frame = Frame::new(1, (800, 600))
            .with_target_fps(60);

        assert!(frame.deadline().is_some());
        assert!(!frame.missed_deadline());
        assert!(frame.time_until_deadline().unwrap() > Duration::ZERO);
    }

    #[test]
    fn test_frame_user_data() {
        let mut frame = Frame::new(1, (800, 600));

        frame.set_user_data(42u32);
        assert_eq!(frame.user_data::<u32>(), Some(&42));

        let taken = frame.take_user_data::<u32>();
        assert_eq!(taken, Some(42));
        assert!(frame.user_data::<u32>().is_none());
    }

    #[test]
    fn test_frame_stats() {
        let mut stats = FrameStats::default();

        let output1 = FrameOutput {
            frame_number: 1,
            width: 800,
            height: 600,
            render_time_us: 1000,
            total_time_us: 16666,
            missed_deadline: false,
            dropped: false,
        };

        stats.update(&output1);
        assert_eq!(stats.total_frames, 1);
        assert_eq!(stats.presented_frames, 1);
        assert_eq!(stats.dropped_frames, 0);

        let output2 = FrameOutput {
            frame_number: 2,
            width: 800,
            height: 600,
            render_time_us: 2000,
            total_time_us: 20000,
            missed_deadline: true,
            dropped: true,
        };

        stats.update(&output2);
        assert_eq!(stats.total_frames, 2);
        assert_eq!(stats.dropped_frames, 1);
        assert_eq!(stats.deadline_misses, 1);
        assert_eq!(stats.drop_rate(), 0.5);
    }
}
