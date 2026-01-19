//! Watchdog for kernel health monitoring
//!
//! The watchdog monitors:
//! - Frame timing (detects hangs)
//! - Memory usage
//! - App responsiveness
//! - GPU health
//!
//! When issues are detected, it triggers recovery actions.
//!
//! # Systemd Integration
//!
//! The watchdog can notify systemd of health status via the sd_notify protocol:
//! - `READY=1` when kernel is initialized
//! - `WATCHDOG=1` on each health check
//! - `STATUS=...` with current health metrics
//! - `STOPPING=1` on graceful shutdown
//!
//! To enable, set environment variable `NOTIFY_SOCKET` and configure
//! the systemd service with `WatchdogSec=`.

use parking_lot::{Mutex, RwLock};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};
use std::thread::{self, JoinHandle};
use std::io::Write;

/// Watchdog configuration
#[derive(Debug, Clone)]
pub struct WatchdogConfig {
    /// Check interval (how often the watchdog runs)
    pub check_interval: Duration,
    /// Maximum time between frame heartbeats before triggering alert
    pub frame_timeout: Duration,
    /// Maximum memory usage (bytes) before warning
    pub memory_warning_threshold: u64,
    /// Maximum memory usage (bytes) before critical alert
    pub memory_critical_threshold: u64,
    /// Maximum frame time (ms) before warning
    pub frame_time_warning_ms: f32,
    /// Maximum frame time (ms) before critical alert
    pub frame_time_critical_ms: f32,
    /// Number of slow frames before triggering action
    pub slow_frame_threshold: u32,
    /// Enable systemd notify integration
    pub enable_systemd_notify: bool,
    /// Stuck frame timeout before emergency action (seconds)
    pub stuck_frame_timeout_secs: u32,
    /// Enable degraded mode on persistent issues
    pub enable_degraded_mode: bool,
}

impl Default for WatchdogConfig {
    fn default() -> Self {
        Self {
            check_interval: Duration::from_secs(1),
            frame_timeout: Duration::from_secs(5),
            memory_warning_threshold: 1024 * 1024 * 1024, // 1 GB
            memory_critical_threshold: 2 * 1024 * 1024 * 1024, // 2 GB
            frame_time_warning_ms: 33.33, // 30 FPS
            frame_time_critical_ms: 100.0, // 10 FPS
            slow_frame_threshold: 10,
            enable_systemd_notify: true,
            stuck_frame_timeout_secs: 60,
            enable_degraded_mode: true,
        }
    }
}

/// Systemd notification helper
///
/// Implements the sd_notify protocol for systemd service integration.
/// This allows the kernel to report its status to systemd.
#[derive(Debug)]
pub struct SystemdNotifier {
    /// Socket path from NOTIFY_SOCKET environment variable
    socket_path: Option<String>,
    /// Whether notifications are enabled
    enabled: bool,
}

impl SystemdNotifier {
    /// Create a new systemd notifier
    pub fn new() -> Self {
        let socket_path = std::env::var("NOTIFY_SOCKET").ok();
        let enabled = socket_path.is_some();

        if enabled {
            log::info!("Systemd notify enabled via socket: {:?}", socket_path);
        } else {
            log::debug!("Systemd notify disabled (NOTIFY_SOCKET not set)");
        }

        Self {
            socket_path,
            enabled,
        }
    }

    /// Check if systemd notifications are available
    pub fn is_enabled(&self) -> bool {
        self.enabled
    }

    /// Send a notification to systemd
    #[cfg(unix)]
    fn send(&self, message: &str) -> std::io::Result<()> {
        use std::os::unix::net::UnixDatagram;

        if let Some(ref path) = self.socket_path {
            let socket = UnixDatagram::unbound()?;
            socket.send_to(message.as_bytes(), path)?;
        }
        Ok(())
    }

    /// Send a notification to systemd (Windows stub)
    #[cfg(not(unix))]
    fn send(&self, _message: &str) -> std::io::Result<()> {
        // No-op on Windows
        Ok(())
    }

    /// Notify that the service is ready
    pub fn notify_ready(&self) {
        if !self.enabled {
            return;
        }

        if let Err(e) = self.send("READY=1") {
            log::warn!("Failed to send READY notification: {}", e);
        } else {
            log::info!("Notified systemd: READY");
        }
    }

    /// Send watchdog heartbeat
    pub fn notify_watchdog(&self) {
        if !self.enabled {
            return;
        }

        if let Err(e) = self.send("WATCHDOG=1") {
            log::warn!("Failed to send WATCHDOG notification: {}", e);
        }
    }

    /// Update status message
    pub fn notify_status(&self, status: &str) {
        if !self.enabled {
            return;
        }

        let message = format!("STATUS={}", status);
        if let Err(e) = self.send(&message) {
            log::warn!("Failed to send STATUS notification: {}", e);
        }
    }

    /// Notify that the service is stopping
    pub fn notify_stopping(&self) {
        if !self.enabled {
            return;
        }

        if let Err(e) = self.send("STOPPING=1") {
            log::warn!("Failed to send STOPPING notification: {}", e);
        } else {
            log::info!("Notified systemd: STOPPING");
        }
    }

    /// Notify with main PID (for forking services)
    pub fn notify_mainpid(&self, pid: u32) {
        if !self.enabled {
            return;
        }

        let message = format!("MAINPID={}", pid);
        if let Err(e) = self.send(&message) {
            log::warn!("Failed to send MAINPID notification: {}", e);
        }
    }

    /// Notify entering degraded mode
    pub fn notify_degraded(&self, reason: &str) {
        if !self.enabled {
            return;
        }

        let message = format!("STATUS=DEGRADED: {}\nWATCHDOG=1", reason);
        if let Err(e) = self.send(&message) {
            log::warn!("Failed to send degraded notification: {}", e);
        }
    }
}

impl Default for SystemdNotifier {
    fn default() -> Self {
        Self::new()
    }
}

/// Degraded mode state
#[derive(Debug, Clone)]
pub struct DegradedModeState {
    /// Whether degraded mode is active
    pub active: bool,
    /// When degraded mode was entered
    pub entered_at: Option<Instant>,
    /// Reason for degraded mode
    pub reason: Option<String>,
    /// Number of times degraded mode has been entered
    pub entry_count: u32,
    /// Actions taken in degraded mode
    pub actions_taken: Vec<DegradedAction>,
}

impl Default for DegradedModeState {
    fn default() -> Self {
        Self {
            active: false,
            entered_at: None,
            reason: None,
            entry_count: 0,
            actions_taken: Vec::new(),
        }
    }
}

/// Actions that can be taken in degraded mode
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DegradedAction {
    /// Reduced frame rate
    ReducedFrameRate { target_fps: u32 },
    /// Disabled non-essential apps
    DisabledApps { count: u32 },
    /// Evicted assets to free memory
    EvictedAssets { bytes_freed: u64 },
    /// Switched to software rendering
    SoftwareRendering,
    /// Skipped optional processing
    SkippedOptionalProcessing,
}

/// Health status levels
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum HealthLevel {
    /// Everything is healthy
    Healthy,
    /// Minor issues, still operational
    Warning,
    /// Significant issues, degraded performance
    Degraded,
    /// Critical issues, immediate action needed
    Critical,
    /// System is unresponsive
    Unresponsive,
}

impl std::fmt::Display for HealthLevel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Healthy => write!(f, "Healthy"),
            Self::Warning => write!(f, "Warning"),
            Self::Degraded => write!(f, "Degraded"),
            Self::Critical => write!(f, "Critical"),
            Self::Unresponsive => write!(f, "Unresponsive"),
        }
    }
}

/// Alert types from watchdog
#[derive(Debug, Clone)]
pub enum WatchdogAlert {
    /// Frame processing is taking too long
    SlowFrames {
        count: u32,
        avg_frame_time_ms: f32,
    },
    /// No frame heartbeat received
    FrameTimeout {
        last_heartbeat: Instant,
    },
    /// Memory usage high
    HighMemory {
        used_bytes: u64,
        threshold: u64,
    },
    /// GPU appears hung
    GpuHang,
    /// App is unresponsive
    AppUnresponsive {
        app_id: u64,
        last_active: Instant,
    },
    /// Custom alert
    Custom {
        message: String,
    },
}

/// Health metrics from the watchdog
#[derive(Debug, Clone)]
pub struct HealthMetrics {
    /// Current health level
    pub level: HealthLevel,
    /// Current frame number
    pub current_frame: u64,
    /// Average frame time (ms)
    pub avg_frame_time_ms: f32,
    /// Peak frame time (ms) in recent window
    pub peak_frame_time_ms: f32,
    /// Consecutive slow frames
    pub slow_frame_count: u32,
    /// Memory usage (bytes)
    pub memory_used: u64,
    /// Time since last frame heartbeat
    pub time_since_heartbeat: Duration,
    /// Number of alerts
    pub alert_count: u32,
    /// Recent alerts
    pub recent_alerts: Vec<WatchdogAlert>,
}

/// Shared state between watchdog and kernel
pub struct WatchdogState {
    /// Last frame heartbeat time
    last_heartbeat: RwLock<Instant>,
    /// Current frame number
    frame_number: AtomicU64,
    /// Recent frame times (ring buffer)
    frame_times: Mutex<Vec<f32>>,
    /// Slow frame counter
    slow_frame_count: AtomicU64,
    /// Memory usage
    memory_used: AtomicU64,
    /// Alerts
    alerts: Mutex<Vec<WatchdogAlert>>,
    /// Overall health level
    health_level: RwLock<HealthLevel>,
    /// Running flag
    running: AtomicBool,
    /// Degraded mode state
    degraded_mode: RwLock<DegradedModeState>,
    /// Consecutive unresponsive intervals
    unresponsive_count: AtomicU64,
    /// Last known good frame
    last_good_frame: AtomicU64,
}

impl WatchdogState {
    /// Create new watchdog state
    pub fn new() -> Self {
        Self {
            last_heartbeat: RwLock::new(Instant::now()),
            frame_number: AtomicU64::new(0),
            frame_times: Mutex::new(Vec::with_capacity(60)),
            slow_frame_count: AtomicU64::new(0),
            memory_used: AtomicU64::new(0),
            alerts: Mutex::new(Vec::new()),
            health_level: RwLock::new(HealthLevel::Healthy),
            running: AtomicBool::new(true),
            degraded_mode: RwLock::new(DegradedModeState::default()),
            unresponsive_count: AtomicU64::new(0),
            last_good_frame: AtomicU64::new(0),
        }
    }

    /// Enter degraded mode
    pub fn enter_degraded_mode(&self, reason: impl Into<String>) {
        let mut mode = self.degraded_mode.write();
        if !mode.active {
            mode.active = true;
            mode.entered_at = Some(Instant::now());
            mode.reason = Some(reason.into());
            mode.entry_count += 1;
            log::warn!("Entering degraded mode: {:?}", mode.reason);
        }
    }

    /// Exit degraded mode
    pub fn exit_degraded_mode(&self) {
        let mut mode = self.degraded_mode.write();
        if mode.active {
            let duration = mode.entered_at.map(|t| t.elapsed()).unwrap_or_default();
            log::info!("Exiting degraded mode after {:?}", duration);
            mode.active = false;
            mode.entered_at = None;
            mode.reason = None;
            mode.actions_taken.clear();
        }
    }

    /// Record an action taken in degraded mode
    pub fn record_degraded_action(&self, action: DegradedAction) {
        let mut mode = self.degraded_mode.write();
        if mode.active && !mode.actions_taken.contains(&action) {
            mode.actions_taken.push(action);
        }
    }

    /// Check if in degraded mode
    pub fn is_degraded(&self) -> bool {
        self.degraded_mode.read().active
    }

    /// Get degraded mode state
    pub fn degraded_state(&self) -> DegradedModeState {
        self.degraded_mode.read().clone()
    }

    /// Increment unresponsive counter
    pub fn increment_unresponsive(&self) -> u64 {
        self.unresponsive_count.fetch_add(1, Ordering::Relaxed) + 1
    }

    /// Reset unresponsive counter
    pub fn reset_unresponsive(&self) {
        self.unresponsive_count.store(0, Ordering::Relaxed);
    }

    /// Get unresponsive count
    pub fn unresponsive_count(&self) -> u64 {
        self.unresponsive_count.load(Ordering::Relaxed)
    }

    /// Mark frame as good
    pub fn mark_good_frame(&self, frame: u64) {
        self.last_good_frame.store(frame, Ordering::Relaxed);
    }

    /// Get last known good frame
    pub fn last_good_frame(&self) -> u64 {
        self.last_good_frame.load(Ordering::Relaxed)
    }

    /// Record a frame heartbeat
    pub fn heartbeat(&self, frame: u64, frame_time_ms: f32) {
        *self.last_heartbeat.write() = Instant::now();
        self.frame_number.store(frame, Ordering::Relaxed);

        // Update frame times ring buffer
        let mut times = self.frame_times.lock();
        if times.len() >= 60 {
            times.remove(0);
        }
        times.push(frame_time_ms);
    }

    /// Update memory usage
    pub fn update_memory(&self, bytes: u64) {
        self.memory_used.store(bytes, Ordering::Relaxed);
    }

    /// Add an alert
    pub fn add_alert(&self, alert: WatchdogAlert) {
        let mut alerts = self.alerts.lock();
        alerts.push(alert);
        // Keep only last 100 alerts
        if alerts.len() > 100 {
            alerts.remove(0);
        }
    }

    /// Set health level
    pub fn set_health(&self, level: HealthLevel) {
        *self.health_level.write() = level;
    }

    /// Get current health level
    pub fn health(&self) -> HealthLevel {
        *self.health_level.read()
    }

    /// Check if running
    pub fn is_running(&self) -> bool {
        self.running.load(Ordering::Relaxed)
    }

    /// Stop the watchdog
    pub fn stop(&self) {
        self.running.store(false, Ordering::Relaxed);
    }

    /// Get health metrics
    pub fn metrics(&self) -> HealthMetrics {
        let times = self.frame_times.lock();
        let avg = if times.is_empty() {
            0.0
        } else {
            times.iter().sum::<f32>() / times.len() as f32
        };
        let peak = times.iter().cloned().fold(0.0f32, f32::max);
        let alerts = self.alerts.lock();

        HealthMetrics {
            level: *self.health_level.read(),
            current_frame: self.frame_number.load(Ordering::Relaxed),
            avg_frame_time_ms: avg,
            peak_frame_time_ms: peak,
            slow_frame_count: self.slow_frame_count.load(Ordering::Relaxed) as u32,
            memory_used: self.memory_used.load(Ordering::Relaxed),
            time_since_heartbeat: self.last_heartbeat.read().elapsed(),
            alert_count: alerts.len() as u32,
            recent_alerts: alerts.iter().rev().take(10).cloned().collect(),
        }
    }
}

impl Default for WatchdogState {
    fn default() -> Self {
        Self::new()
    }
}

/// The watchdog thread
pub struct Watchdog {
    /// Configuration
    config: WatchdogConfig,
    /// Shared state
    state: Arc<WatchdogState>,
    /// Watchdog thread handle
    thread_handle: Option<JoinHandle<()>>,
    /// Callback for alerts
    alert_callback: Option<Box<dyn Fn(WatchdogAlert) + Send + Sync>>,
    /// Systemd notifier
    systemd_notifier: Arc<SystemdNotifier>,
}

impl Watchdog {
    /// Create a new watchdog
    pub fn new(config: WatchdogConfig) -> Self {
        let systemd_notifier = if config.enable_systemd_notify {
            Arc::new(SystemdNotifier::new())
        } else {
            Arc::new(SystemdNotifier {
                socket_path: None,
                enabled: false,
            })
        };

        Self {
            config,
            state: Arc::new(WatchdogState::new()),
            thread_handle: None,
            alert_callback: None,
            systemd_notifier,
        }
    }

    /// Set alert callback
    pub fn set_alert_callback<F>(&mut self, callback: F)
    where
        F: Fn(WatchdogAlert) + Send + Sync + 'static,
    {
        self.alert_callback = Some(Box::new(callback));
    }

    /// Get the state handle for the kernel to use
    pub fn state(&self) -> Arc<WatchdogState> {
        Arc::clone(&self.state)
    }

    /// Get the systemd notifier
    pub fn systemd_notifier(&self) -> &Arc<SystemdNotifier> {
        &self.systemd_notifier
    }

    /// Notify systemd that we're ready
    pub fn notify_ready(&self) {
        self.systemd_notifier.notify_ready();
    }

    /// Notify systemd that we're stopping
    pub fn notify_stopping(&self) {
        self.systemd_notifier.notify_stopping();
    }

    /// Start the watchdog thread
    pub fn start(&mut self) {
        if self.thread_handle.is_some() {
            return;
        }

        let state = Arc::clone(&self.state);
        let config = self.config.clone();
        let notifier = Arc::clone(&self.systemd_notifier);

        let handle = thread::Builder::new()
            .name("watchdog".to_string())
            .spawn(move || {
                Self::watchdog_loop(state, config, notifier);
            })
            .expect("Failed to start watchdog thread");

        self.thread_handle = Some(handle);
        log::info!("Watchdog started");
    }

    /// Stop the watchdog thread
    pub fn stop(&mut self) {
        self.notify_stopping();
        self.state.stop();

        if let Some(handle) = self.thread_handle.take() {
            let _ = handle.join();
        }

        log::info!("Watchdog stopped");
    }

    /// The main watchdog loop
    fn watchdog_loop(state: Arc<WatchdogState>, config: WatchdogConfig, notifier: Arc<SystemdNotifier>) {
        let stuck_timeout = Duration::from_secs(config.stuck_frame_timeout_secs as u64);

        while state.is_running() {
            thread::sleep(config.check_interval);

            if !state.is_running() {
                break;
            }

            // Check frame heartbeat
            let heartbeat_elapsed = state.last_heartbeat.read().elapsed();
            if heartbeat_elapsed > config.frame_timeout {
                let unresponsive_count = state.increment_unresponsive();
                log::error!(
                    "Frame timeout! No heartbeat for {:?} (unresponsive count: {})",
                    heartbeat_elapsed,
                    unresponsive_count
                );
                state.add_alert(WatchdogAlert::FrameTimeout {
                    last_heartbeat: *state.last_heartbeat.read(),
                });
                state.set_health(HealthLevel::Unresponsive);

                // Enter degraded mode if timeout persists
                if config.enable_degraded_mode && heartbeat_elapsed > stuck_timeout {
                    state.enter_degraded_mode("Frame stuck for too long");
                    notifier.notify_degraded("Frame stuck timeout exceeded");
                }

                // Still send watchdog heartbeat to systemd to prevent restart
                // The kernel is alive, just processing is stuck
                notifier.notify_watchdog();
                continue;
            } else {
                // Frame heartbeat received, reset unresponsive count
                state.reset_unresponsive();

                // Exit degraded mode if we've recovered
                if state.is_degraded() {
                    let current_frame = state.frame_number.load(Ordering::Relaxed);
                    let last_good = state.last_good_frame();
                    // Need several good frames before exiting degraded mode
                    if current_frame > last_good + 60 {
                        state.exit_degraded_mode();
                    }
                }
            }

            // Check frame times
            let mut health = HealthLevel::Healthy;
            let avg_frame_time: f32;
            {
                let times = state.frame_times.lock();
                if !times.is_empty() {
                    let avg = times.iter().sum::<f32>() / times.len() as f32;
                    avg_frame_time = avg;
                    let slow_count = times
                        .iter()
                        .filter(|&&t| t > config.frame_time_warning_ms)
                        .count() as u32;

                    state.slow_frame_count.store(slow_count as u64, Ordering::Relaxed);

                    if slow_count >= config.slow_frame_threshold {
                        log::warn!("Slow frames detected: {} slow frames, avg {}ms", slow_count, avg);
                        state.add_alert(WatchdogAlert::SlowFrames {
                            count: slow_count,
                            avg_frame_time_ms: avg,
                        });

                        if avg > config.frame_time_critical_ms {
                            health = HealthLevel::Critical;

                            // Enter degraded mode for critical performance issues
                            if config.enable_degraded_mode {
                                state.enter_degraded_mode("Critical frame time exceeded");
                                state.record_degraded_action(DegradedAction::ReducedFrameRate { target_fps: 30 });
                            }
                        } else {
                            health = HealthLevel::Degraded;
                        }
                    } else if avg > config.frame_time_warning_ms {
                        health = HealthLevel::Warning;
                    } else {
                        // Good frame time, mark as good
                        let current_frame = state.frame_number.load(Ordering::Relaxed);
                        state.mark_good_frame(current_frame);
                    }
                } else {
                    avg_frame_time = 0.0;
                }
            }

            // Check memory
            let memory = state.memory_used.load(Ordering::Relaxed);
            if memory > config.memory_critical_threshold {
                log::error!("Critical memory usage: {} bytes", memory);
                state.add_alert(WatchdogAlert::HighMemory {
                    used_bytes: memory,
                    threshold: config.memory_critical_threshold,
                });
                health = HealthLevel::Critical;

                // Enter degraded mode for memory pressure
                if config.enable_degraded_mode {
                    state.enter_degraded_mode("Critical memory pressure");
                }
            } else if memory > config.memory_warning_threshold {
                log::warn!("High memory usage: {} bytes", memory);
                state.add_alert(WatchdogAlert::HighMemory {
                    used_bytes: memory,
                    threshold: config.memory_warning_threshold,
                });
                if health < HealthLevel::Warning {
                    health = HealthLevel::Warning;
                }
            }

            state.set_health(health);

            // Send systemd watchdog heartbeat
            notifier.notify_watchdog();

            // Update systemd status with current metrics
            let current_frame = state.frame_number.load(Ordering::Relaxed);
            let status = format!(
                "frame={} health={:?} fps={:.1}",
                current_frame,
                health,
                if avg_frame_time > 0.0 { 1000.0 / avg_frame_time } else { 0.0 }
            );
            notifier.notify_status(&status);
        }
    }

    /// Get current health metrics
    pub fn metrics(&self) -> HealthMetrics {
        self.state.metrics()
    }

    /// Get current health level
    pub fn health(&self) -> HealthLevel {
        self.state.health()
    }
}

impl Drop for Watchdog {
    fn drop(&mut self) {
        self.stop();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_watchdog_state() {
        let state = WatchdogState::new();

        // Record some heartbeats
        state.heartbeat(1, 16.0);
        state.heartbeat(2, 17.0);
        state.heartbeat(3, 15.0);

        let metrics = state.metrics();
        assert_eq!(metrics.current_frame, 3);
        assert!(metrics.avg_frame_time_ms > 15.0 && metrics.avg_frame_time_ms < 18.0);
    }

    #[test]
    fn test_health_levels() {
        let state = WatchdogState::new();

        assert_eq!(state.health(), HealthLevel::Healthy);

        state.set_health(HealthLevel::Warning);
        assert_eq!(state.health(), HealthLevel::Warning);

        state.set_health(HealthLevel::Critical);
        assert_eq!(state.health(), HealthLevel::Critical);
    }

    #[test]
    fn test_alert_tracking() {
        let state = WatchdogState::new();

        state.add_alert(WatchdogAlert::SlowFrames {
            count: 5,
            avg_frame_time_ms: 50.0,
        });

        let metrics = state.metrics();
        assert_eq!(metrics.alert_count, 1);
    }

    #[test]
    fn test_watchdog_lifecycle() {
        let mut watchdog = Watchdog::new(WatchdogConfig::default());
        let state = watchdog.state();

        watchdog.start();

        // Send heartbeats
        state.heartbeat(1, 16.0);
        thread::sleep(Duration::from_millis(100));
        state.heartbeat(2, 16.0);

        assert!(state.is_running());

        watchdog.stop();
        assert!(!state.is_running());
    }

    #[test]
    fn test_degraded_mode() {
        let state = WatchdogState::new();

        // Initially not degraded
        assert!(!state.is_degraded());

        // Enter degraded mode
        state.enter_degraded_mode("Test reason");
        assert!(state.is_degraded());

        let degraded_state = state.degraded_state();
        assert!(degraded_state.active);
        assert_eq!(degraded_state.reason, Some("Test reason".to_string()));
        assert_eq!(degraded_state.entry_count, 1);

        // Record an action
        state.record_degraded_action(DegradedAction::ReducedFrameRate { target_fps: 30 });
        let degraded_state = state.degraded_state();
        assert_eq!(degraded_state.actions_taken.len(), 1);

        // Exit degraded mode
        state.exit_degraded_mode();
        assert!(!state.is_degraded());
    }

    #[test]
    fn test_unresponsive_tracking() {
        let state = WatchdogState::new();

        assert_eq!(state.unresponsive_count(), 0);

        // Increment
        assert_eq!(state.increment_unresponsive(), 1);
        assert_eq!(state.increment_unresponsive(), 2);
        assert_eq!(state.unresponsive_count(), 2);

        // Reset
        state.reset_unresponsive();
        assert_eq!(state.unresponsive_count(), 0);
    }

    #[test]
    fn test_good_frame_tracking() {
        let state = WatchdogState::new();

        assert_eq!(state.last_good_frame(), 0);

        state.mark_good_frame(100);
        assert_eq!(state.last_good_frame(), 100);

        state.mark_good_frame(200);
        assert_eq!(state.last_good_frame(), 200);
    }

    #[test]
    fn test_systemd_notifier_disabled() {
        // Should not panic even without NOTIFY_SOCKET
        let notifier = SystemdNotifier::new();

        // These should all be no-ops since NOTIFY_SOCKET is not set
        notifier.notify_ready();
        notifier.notify_watchdog();
        notifier.notify_status("test");
        notifier.notify_stopping();
    }
}
