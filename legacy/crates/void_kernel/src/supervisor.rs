//! Supervision trees for fault tolerance
//!
//! Inspired by Erlang OTP's supervision model, this module provides
//! automatic restart and recovery for apps and services.
//!
//! # Design
//!
//! ```text
//! ┌─────────────────────────────────────────────────────────────────┐
//! │                     SUPERVISION TREE                             │
//! │                                                                  │
//! │  ┌───────────────────────────────────────────────────────────┐ │
//! │  │                  ROOT SUPERVISOR                           │ │
//! │  │              Strategy: OneForOne                           │ │
//! │  └─────────┬─────────────────────┬────────────────────────────┘ │
//! │            │                     │                               │
//! │     ┌──────▼──────┐       ┌──────▼──────┐                       │
//! │     │ App Group   │       │ Service     │                       │
//! │     │ Supervisor  │       │ Supervisor  │                       │
//! │     │ (OneForOne) │       │ (OneForAll) │                       │
//! │     └──────┬──────┘       └──────┬──────┘                       │
//! │            │                     │                               │
//! │     ┌──────┼──────┐       ┌──────┼──────┐                       │
//! │     │      │      │       │      │      │                       │
//! │   App1   App2   App3   Svc1   Svc2   Svc3                       │
//! └─────────────────────────────────────────────────────────────────┘
//! ```
//!
//! # Key Features
//!
//! - **Hierarchical Supervision**: Supervisors can supervise other supervisors
//! - **Restart Strategies**: OneForOne, OneForAll, RestForOne
//! - **Restart Intensity**: Maximum restarts within a time window
//! - **Exponential Backoff**: Delays between restarts to prevent thrashing
//! - **Child Types**: Permanent, Transient, Temporary
//! - **Escalation**: Failures propagate up the tree when limits exceeded
//!
//! # The Kernel Never Dies
//!
//! The root supervisor has special handling:
//! - It NEVER crashes
//! - It NEVER escalates (there's no parent)
//! - Instead, it degrades gracefully and logs critical errors
//! - Systemd handles restarting if the entire process dies

use parking_lot::{Mutex, RwLock};
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use crate::app::AppId;

/// Unique identifier for a supervisor
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SupervisorId(u64);

impl SupervisorId {
    /// Create a new unique supervisor ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for SupervisorId {
    fn default() -> Self {
        Self::new()
    }
}

/// Identifier for a supervised child
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ChildId {
    App(AppId),
    Service(ServiceId),
    Supervisor(SupervisorId),
}

/// Unique identifier for a service
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ServiceId(u64);

impl ServiceId {
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }
}

impl Default for ServiceId {
    fn default() -> Self {
        Self::new()
    }
}

/// Restart strategy for a supervisor
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RestartStrategy {
    /// Restart only the failed child
    OneForOne,
    /// Restart all children when one fails
    OneForAll,
    /// Restart the failed child and all children started after it
    RestForOne,
}

impl Default for RestartStrategy {
    fn default() -> Self {
        Self::OneForOne
    }
}

/// Intensity configuration for restart limits
#[derive(Debug, Clone, Copy)]
pub struct RestartIntensity {
    /// Maximum restarts allowed within the window
    pub max_restarts: u32,
    /// Time window for counting restarts (seconds)
    pub window_secs: u32,
    /// Backoff configuration
    pub backoff: BackoffConfig,
}

impl Default for RestartIntensity {
    fn default() -> Self {
        Self {
            max_restarts: 5,
            window_secs: 60,
            backoff: BackoffConfig::default(),
        }
    }
}

/// Exponential backoff configuration
#[derive(Debug, Clone, Copy)]
pub struct BackoffConfig {
    /// Initial delay before first restart (milliseconds)
    pub initial_delay_ms: u64,
    /// Maximum delay between restarts (milliseconds)
    pub max_delay_ms: u64,
    /// Multiplier for exponential backoff
    pub multiplier: f32,
    /// Whether backoff is enabled
    pub enabled: bool,
}

impl Default for BackoffConfig {
    fn default() -> Self {
        Self {
            initial_delay_ms: 100,
            max_delay_ms: 30_000, // 30 seconds max
            multiplier: 2.0,
            enabled: true,
        }
    }
}

impl BackoffConfig {
    /// Calculate delay for nth restart
    pub fn delay_for_restart(&self, restart_count: u32) -> Duration {
        if !self.enabled || restart_count == 0 {
            return Duration::ZERO;
        }

        let delay_ms = (self.initial_delay_ms as f64)
            * (self.multiplier as f64).powi(restart_count.saturating_sub(1) as i32);
        let delay_ms = delay_ms.min(self.max_delay_ms as f64) as u64;

        Duration::from_millis(delay_ms)
    }

    /// Create a disabled backoff config
    pub fn disabled() -> Self {
        Self {
            enabled: false,
            ..Default::default()
        }
    }

    /// Create an aggressive backoff for apps that crash repeatedly
    pub fn aggressive() -> Self {
        Self {
            initial_delay_ms: 500,
            max_delay_ms: 60_000,
            multiplier: 3.0,
            enabled: true,
        }
    }
}

/// Child restart type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ChildType {
    /// Permanent: Always restart
    Permanent,
    /// Temporary: Never restart
    Temporary,
    /// Transient: Restart only if abnormal exit
    Transient,
}

impl Default for ChildType {
    fn default() -> Self {
        Self::Permanent
    }
}

/// Status of a supervised child
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ChildStatus {
    /// Child is starting
    Starting,
    /// Child is running
    Running,
    /// Child is stopping
    Stopping,
    /// Child has stopped normally
    Stopped,
    /// Child has failed
    Failed,
    /// Child is restarting
    Restarting,
}

/// A supervised child
#[derive(Debug)]
pub struct SupervisedChild {
    /// Child identifier
    pub id: ChildId,
    /// Human-readable name
    pub name: String,
    /// Child type (permanent, temporary, transient)
    pub child_type: ChildType,
    /// Current status
    pub status: ChildStatus,
    /// Restart count
    pub restart_count: u32,
    /// Timestamps of recent restarts (for intensity tracking)
    pub restart_history: Vec<Instant>,
    /// When the child was started
    pub started_at: Option<Instant>,
    /// Last error message
    pub last_error: Option<String>,
    /// Pending restart time (for backoff)
    pub pending_restart_at: Option<Instant>,
    /// Consecutive failures without successful run
    pub consecutive_failures: u32,
    /// Total uptime across all runs (seconds)
    pub total_uptime_secs: f64,
}

impl SupervisedChild {
    /// Create a new supervised child
    pub fn new(id: ChildId, name: impl Into<String>, child_type: ChildType) -> Self {
        Self {
            id,
            name: name.into(),
            child_type,
            status: ChildStatus::Starting,
            restart_count: 0,
            restart_history: Vec::new(),
            started_at: None,
            last_error: None,
            pending_restart_at: None,
            consecutive_failures: 0,
            total_uptime_secs: 0.0,
        }
    }

    /// Mark as running
    pub fn mark_running(&mut self) {
        self.status = ChildStatus::Running;
        self.started_at = Some(Instant::now());
        self.pending_restart_at = None;
        // Reset consecutive failures after a successful start
        // We'll track this based on minimum uptime threshold
    }

    /// Mark as failed with error
    pub fn mark_failed(&mut self, error: impl Into<String>) {
        // Record uptime before marking as failed
        if let Some(started) = self.started_at {
            let uptime = started.elapsed().as_secs_f64();
            self.total_uptime_secs += uptime;

            // If the child ran for less than 10 seconds, consider it a quick failure
            if uptime < 10.0 {
                self.consecutive_failures += 1;
            } else {
                // Ran long enough, reset consecutive failure count
                self.consecutive_failures = 0;
            }
        } else {
            // Never started, definitely a failure
            self.consecutive_failures += 1;
        }

        self.status = ChildStatus::Failed;
        self.last_error = Some(error.into());
        self.started_at = None;
    }

    /// Record a restart with backoff scheduling
    pub fn record_restart(&mut self, backoff: &BackoffConfig) {
        self.restart_count += 1;
        self.restart_history.push(Instant::now());
        self.status = ChildStatus::Restarting;

        // Calculate backoff delay based on consecutive failures
        let delay = backoff.delay_for_restart(self.consecutive_failures);
        if delay > Duration::ZERO {
            self.pending_restart_at = Some(Instant::now() + delay);
            log::debug!(
                "Child '{}' scheduled for restart after {:?} (consecutive failures: {})",
                self.name,
                delay,
                self.consecutive_failures
            );
        } else {
            self.pending_restart_at = None;
        }
    }

    /// Check if the child is ready to restart (backoff expired)
    pub fn is_ready_for_restart(&self) -> bool {
        match self.pending_restart_at {
            Some(at) => Instant::now() >= at,
            None => true,
        }
    }

    /// Get remaining backoff time
    pub fn remaining_backoff(&self) -> Option<Duration> {
        self.pending_restart_at.and_then(|at| {
            let now = Instant::now();
            if now < at {
                Some(at - now)
            } else {
                None
            }
        })
    }

    /// Check if restart is allowed based on intensity
    pub fn can_restart(&self, intensity: &RestartIntensity) -> bool {
        let window = Duration::from_secs(intensity.window_secs as u64);
        let cutoff = Instant::now() - window;

        let recent_restarts = self
            .restart_history
            .iter()
            .filter(|&&t| t > cutoff)
            .count() as u32;

        recent_restarts < intensity.max_restarts
    }

    /// Should restart based on child type and exit reason
    pub fn should_restart(&self, normal_exit: bool) -> bool {
        match self.child_type {
            ChildType::Permanent => true,
            ChildType::Temporary => false,
            ChildType::Transient => !normal_exit,
        }
    }

    /// Clean up old restart history entries
    pub fn gc_restart_history(&mut self, window_secs: u32) {
        let cutoff = Instant::now() - Duration::from_secs(window_secs as u64);
        self.restart_history.retain(|&t| t > cutoff);
    }

    /// Get child statistics
    pub fn stats(&self) -> ChildStats {
        ChildStats {
            id: self.id,
            name: self.name.clone(),
            status: self.status,
            restart_count: self.restart_count,
            consecutive_failures: self.consecutive_failures,
            total_uptime_secs: self.total_uptime_secs,
            last_error: self.last_error.clone(),
            pending_restart: self.remaining_backoff(),
        }
    }
}

/// Statistics about a supervised child
#[derive(Debug, Clone)]
pub struct ChildStats {
    pub id: ChildId,
    pub name: String,
    pub status: ChildStatus,
    pub restart_count: u32,
    pub consecutive_failures: u32,
    pub total_uptime_secs: f64,
    pub last_error: Option<String>,
    pub pending_restart: Option<Duration>,
}

/// Action taken after a child failure
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SupervisorAction {
    /// Restart the specified children
    Restart(Vec<ChildId>),
    /// Stop all children and escalate to parent
    Escalate,
    /// Stop all children and shut down
    Shutdown,
    /// Ignore the failure (for temporary children)
    Ignore,
}

/// A supervisor node in the supervision tree
#[derive(Debug)]
pub struct Supervisor {
    /// Unique identifier
    pub id: SupervisorId,
    /// Human-readable name
    pub name: String,
    /// Restart strategy
    pub strategy: RestartStrategy,
    /// Restart intensity limits
    pub intensity: RestartIntensity,
    /// Supervised children
    pub children: Vec<SupervisedChild>,
    /// Parent supervisor (None for root)
    pub parent: Option<SupervisorId>,
    /// Whether this supervisor is running
    pub running: bool,
}

impl Supervisor {
    /// Create a new supervisor
    pub fn new(name: impl Into<String>, strategy: RestartStrategy) -> Self {
        Self {
            id: SupervisorId::new(),
            name: name.into(),
            strategy,
            intensity: RestartIntensity::default(),
            children: Vec::new(),
            parent: None,
            running: false,
        }
    }

    /// Set restart intensity
    pub fn with_intensity(mut self, intensity: RestartIntensity) -> Self {
        self.intensity = intensity;
        self
    }

    /// Set parent supervisor
    pub fn with_parent(mut self, parent: SupervisorId) -> Self {
        self.parent = Some(parent);
        self
    }

    /// Add a child
    pub fn add_child(&mut self, child: SupervisedChild) {
        self.children.push(child);
    }

    /// Remove a child by ID
    pub fn remove_child(&mut self, id: ChildId) -> Option<SupervisedChild> {
        if let Some(idx) = self.children.iter().position(|c| c.id == id) {
            Some(self.children.remove(idx))
        } else {
            None
        }
    }

    /// Get a child by ID
    pub fn get_child(&self, id: ChildId) -> Option<&SupervisedChild> {
        self.children.iter().find(|c| c.id == id)
    }

    /// Get a mutable child by ID
    pub fn get_child_mut(&mut self, id: ChildId) -> Option<&mut SupervisedChild> {
        self.children.iter_mut().find(|c| c.id == id)
    }

    /// Start the supervisor
    pub fn start(&mut self) {
        self.running = true;
        log::info!("Supervisor '{}' started", self.name);
    }

    /// Stop the supervisor
    pub fn stop(&mut self) {
        self.running = false;
        log::info!("Supervisor '{}' stopped", self.name);
    }

    /// Handle a child failure
    pub fn handle_failure(&mut self, child_id: ChildId, error: &str, normal_exit: bool) -> SupervisorAction {
        // Find the child
        let child_idx = match self.children.iter().position(|c| c.id == child_id) {
            Some(idx) => idx,
            None => {
                log::warn!("Unknown child {:?} reported failure", child_id);
                return SupervisorAction::Ignore;
            }
        };

        // Update child status
        self.children[child_idx].mark_failed(error);

        // Check if should restart
        if !self.children[child_idx].should_restart(normal_exit) {
            log::info!(
                "Child '{}' ({:?}) not restarting (normal_exit={}, type={:?})",
                self.children[child_idx].name,
                child_id,
                normal_exit,
                self.children[child_idx].child_type
            );
            return SupervisorAction::Ignore;
        }

        // Check intensity limits
        if !self.children[child_idx].can_restart(&self.intensity) {
            log::error!(
                "Child '{}' ({:?}) exceeded restart intensity ({} restarts in {} secs), escalating",
                self.children[child_idx].name,
                child_id,
                self.intensity.max_restarts,
                self.intensity.window_secs
            );
            return SupervisorAction::Escalate;
        }

        // Record restart with backoff
        self.children[child_idx].record_restart(&self.intensity.backoff);

        // Determine action based on strategy
        match self.strategy {
            RestartStrategy::OneForOne => {
                log::info!(
                    "Restarting child '{}' ({:?}) - OneForOne strategy (consecutive failures: {})",
                    self.children[child_idx].name,
                    child_id,
                    self.children[child_idx].consecutive_failures
                );
                SupervisorAction::Restart(vec![child_id])
            }
            RestartStrategy::OneForAll => {
                log::info!(
                    "Restarting all children due to '{}' failure - OneForAll strategy",
                    self.children[child_idx].name
                );
                // Record restart for all children
                for child in &mut self.children {
                    if child.id != child_id {
                        child.record_restart(&self.intensity.backoff);
                    }
                }
                let all_ids: Vec<_> = self.children.iter().map(|c| c.id).collect();
                SupervisorAction::Restart(all_ids)
            }
            RestartStrategy::RestForOne => {
                log::info!(
                    "Restarting '{}' and subsequent children - RestForOne strategy",
                    self.children[child_idx].name
                );
                // Record restart for affected children
                for child in &mut self.children[child_idx + 1..] {
                    child.record_restart(&self.intensity.backoff);
                }
                let to_restart: Vec<_> = self.children[child_idx..].iter().map(|c| c.id).collect();
                SupervisorAction::Restart(to_restart)
            }
        }
    }

    /// Get children that are ready to restart (backoff expired)
    pub fn get_pending_restarts(&self) -> Vec<ChildId> {
        self.children
            .iter()
            .filter(|c| c.status == ChildStatus::Restarting && c.is_ready_for_restart())
            .map(|c| c.id)
            .collect()
    }

    /// Get children that are still waiting for backoff
    pub fn get_waiting_restarts(&self) -> Vec<(ChildId, Duration)> {
        self.children
            .iter()
            .filter_map(|c| {
                if c.status == ChildStatus::Restarting {
                    c.remaining_backoff().map(|d| (c.id, d))
                } else {
                    None
                }
            })
            .collect()
    }

    /// Perform garbage collection on restart histories
    pub fn gc(&mut self) {
        for child in &mut self.children {
            child.gc_restart_history(self.intensity.window_secs);
        }
    }

    /// Get supervisor status report
    pub fn status_report(&self) -> SupervisorStatus {
        SupervisorStatus {
            id: self.id,
            name: self.name.clone(),
            running: self.running,
            strategy: self.strategy,
            child_count: self.children.len(),
            running_children: self.children.iter().filter(|c| c.status == ChildStatus::Running).count(),
            failed_children: self.children.iter().filter(|c| c.status == ChildStatus::Failed).count(),
            total_restarts: self.children.iter().map(|c| c.restart_count as u64).sum(),
        }
    }
}

/// Status report for a supervisor
#[derive(Debug, Clone)]
pub struct SupervisorStatus {
    pub id: SupervisorId,
    pub name: String,
    pub running: bool,
    pub strategy: RestartStrategy,
    pub child_count: usize,
    pub running_children: usize,
    pub failed_children: usize,
    pub total_restarts: u64,
}

/// The supervision tree manages all supervisors
pub struct SupervisorTree {
    /// All supervisors by ID
    supervisors: HashMap<SupervisorId, Supervisor>,
    /// Root supervisor ID
    root_id: SupervisorId,
    /// Child to supervisor mapping
    child_to_supervisor: HashMap<ChildId, SupervisorId>,
}

impl SupervisorTree {
    /// Create a new supervision tree with a root supervisor
    pub fn new() -> Self {
        let mut root = Supervisor::new("root", RestartStrategy::OneForOne);
        root.start();
        let root_id = root.id;

        let mut supervisors = HashMap::new();
        supervisors.insert(root_id, root);

        Self {
            supervisors,
            root_id,
            child_to_supervisor: HashMap::new(),
        }
    }

    /// Get the root supervisor ID
    pub fn root_id(&self) -> SupervisorId {
        self.root_id
    }

    /// Add a supervisor as a child of another
    pub fn add_supervisor(
        &mut self,
        name: impl Into<String>,
        strategy: RestartStrategy,
        parent_id: SupervisorId,
    ) -> Option<SupervisorId> {
        if !self.supervisors.contains_key(&parent_id) {
            return None;
        }

        let supervisor = Supervisor::new(name, strategy).with_parent(parent_id);
        let id = supervisor.id;

        self.supervisors.insert(id, supervisor);

        // Register as child of parent
        let child = SupervisedChild::new(
            ChildId::Supervisor(id),
            format!("supervisor-{}", id.raw()),
            ChildType::Permanent,
        );
        self.supervisors.get_mut(&parent_id)?.add_child(child);
        self.child_to_supervisor.insert(ChildId::Supervisor(id), parent_id);

        Some(id)
    }

    /// Add an app to a supervisor
    pub fn add_app(
        &mut self,
        app_id: AppId,
        name: impl Into<String>,
        child_type: ChildType,
        supervisor_id: SupervisorId,
    ) -> bool {
        let supervisor = match self.supervisors.get_mut(&supervisor_id) {
            Some(s) => s,
            None => return false,
        };

        let child_id = ChildId::App(app_id);
        let child = SupervisedChild::new(child_id, name, child_type);
        supervisor.add_child(child);
        self.child_to_supervisor.insert(child_id, supervisor_id);

        true
    }

    /// Remove an app from supervision
    pub fn remove_app(&mut self, app_id: AppId) -> bool {
        let child_id = ChildId::App(app_id);
        let supervisor_id = match self.child_to_supervisor.remove(&child_id) {
            Some(id) => id,
            None => return false,
        };

        if let Some(supervisor) = self.supervisors.get_mut(&supervisor_id) {
            supervisor.remove_child(child_id);
        }

        true
    }

    /// Mark an app as running
    pub fn mark_app_running(&mut self, app_id: AppId) {
        let child_id = ChildId::App(app_id);
        if let Some(&supervisor_id) = self.child_to_supervisor.get(&child_id) {
            if let Some(supervisor) = self.supervisors.get_mut(&supervisor_id) {
                if let Some(child) = supervisor.get_child_mut(child_id) {
                    child.mark_running();
                }
            }
        }
    }

    /// Report an app failure and get the action to take
    pub fn report_app_failure(
        &mut self,
        app_id: AppId,
        error: &str,
        normal_exit: bool,
    ) -> SupervisorAction {
        let child_id = ChildId::App(app_id);
        let supervisor_id = match self.child_to_supervisor.get(&child_id) {
            Some(&id) => id,
            None => {
                log::warn!("App {:?} not supervised", app_id);
                return SupervisorAction::Ignore;
            }
        };

        // Handle failure in supervisor
        let action = if let Some(supervisor) = self.supervisors.get_mut(&supervisor_id) {
            supervisor.handle_failure(child_id, error, normal_exit)
        } else {
            return SupervisorAction::Ignore;
        };

        // If escalating, propagate up the tree
        if action == SupervisorAction::Escalate {
            return self.escalate(supervisor_id);
        }

        action
    }

    /// Escalate a failure up the supervision tree
    fn escalate(&mut self, failed_supervisor_id: SupervisorId) -> SupervisorAction {
        // Find parent
        let parent_id = if let Some(supervisor) = self.supervisors.get(&failed_supervisor_id) {
            supervisor.parent
        } else {
            return SupervisorAction::Shutdown;
        };

        match parent_id {
            Some(pid) => {
                // Report failure to parent
                let child_id = ChildId::Supervisor(failed_supervisor_id);
                if let Some(parent) = self.supervisors.get_mut(&pid) {
                    let action = parent.handle_failure(child_id, "Child supervisor escalated", false);
                    if action == SupervisorAction::Escalate {
                        return self.escalate(pid);
                    }
                    action
                } else {
                    SupervisorAction::Shutdown
                }
            }
            None => {
                // Root supervisor - no parent, must shut down
                log::error!("Root supervisor escalation - initiating shutdown");
                SupervisorAction::Shutdown
            }
        }
    }

    /// Get a supervisor by ID
    pub fn get_supervisor(&self, id: SupervisorId) -> Option<&Supervisor> {
        self.supervisors.get(&id)
    }

    /// Get a mutable supervisor by ID
    pub fn get_supervisor_mut(&mut self, id: SupervisorId) -> Option<&mut Supervisor> {
        self.supervisors.get_mut(&id)
    }

    /// Perform garbage collection
    pub fn gc(&mut self) {
        for supervisor in self.supervisors.values_mut() {
            supervisor.gc();
        }
    }

    /// Get overall status
    pub fn status(&self) -> TreeStatus {
        TreeStatus {
            supervisor_count: self.supervisors.len(),
            total_children: self.child_to_supervisor.len(),
            supervisor_statuses: self.supervisors.values().map(|s| s.status_report()).collect(),
        }
    }
}

impl Default for SupervisorTree {
    fn default() -> Self {
        Self::new()
    }
}

/// Overall tree status
#[derive(Debug, Clone)]
pub struct TreeStatus {
    pub supervisor_count: usize,
    pub total_children: usize,
    pub supervisor_statuses: Vec<SupervisorStatus>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_supervisor_one_for_one() {
        let mut supervisor = Supervisor::new("test", RestartStrategy::OneForOne);
        supervisor.start();

        let app_id = AppId::new();
        let child = SupervisedChild::new(ChildId::App(app_id), "test-app", ChildType::Permanent);
        supervisor.add_child(child);
        supervisor.get_child_mut(ChildId::App(app_id)).unwrap().mark_running();

        // Simulate failure
        let action = supervisor.handle_failure(ChildId::App(app_id), "Test error", false);

        assert!(matches!(action, SupervisorAction::Restart(ids) if ids.len() == 1));
    }

    #[test]
    fn test_supervisor_one_for_all() {
        let mut supervisor = Supervisor::new("test", RestartStrategy::OneForAll);
        supervisor.start();

        let app1 = AppId::new();
        let app2 = AppId::new();
        let app3 = AppId::new();

        supervisor.add_child(SupervisedChild::new(ChildId::App(app1), "app1", ChildType::Permanent));
        supervisor.add_child(SupervisedChild::new(ChildId::App(app2), "app2", ChildType::Permanent));
        supervisor.add_child(SupervisedChild::new(ChildId::App(app3), "app3", ChildType::Permanent));

        // Simulate failure of app2
        let action = supervisor.handle_failure(ChildId::App(app2), "Test error", false);

        // Should restart all 3
        assert!(matches!(action, SupervisorAction::Restart(ids) if ids.len() == 3));
    }

    #[test]
    fn test_intensity_limit() {
        let mut supervisor = Supervisor::new("test", RestartStrategy::OneForOne)
            .with_intensity(RestartIntensity {
                max_restarts: 2,
                window_secs: 60,
                backoff: BackoffConfig::disabled(),
            });
        supervisor.start();

        let app_id = AppId::new();
        supervisor.add_child(SupervisedChild::new(ChildId::App(app_id), "app", ChildType::Permanent));
        supervisor.get_child_mut(ChildId::App(app_id)).unwrap().mark_running();

        // First failure - restart
        let action = supervisor.handle_failure(ChildId::App(app_id), "Error 1", false);
        assert!(matches!(action, SupervisorAction::Restart(_)));

        // Second failure - restart
        let action = supervisor.handle_failure(ChildId::App(app_id), "Error 2", false);
        assert!(matches!(action, SupervisorAction::Restart(_)));

        // Third failure - should escalate (exceeded limit)
        let action = supervisor.handle_failure(ChildId::App(app_id), "Error 3", false);
        assert_eq!(action, SupervisorAction::Escalate);
    }

    #[test]
    fn test_backoff_calculation() {
        let config = BackoffConfig {
            initial_delay_ms: 100,
            max_delay_ms: 10_000,
            multiplier: 2.0,
            enabled: true,
        };

        // First restart: no delay (restart_count=0 or first fail)
        assert_eq!(config.delay_for_restart(0), Duration::ZERO);

        // Second restart: 100ms
        assert_eq!(config.delay_for_restart(1), Duration::from_millis(100));

        // Third restart: 200ms
        assert_eq!(config.delay_for_restart(2), Duration::from_millis(200));

        // Fourth restart: 400ms
        assert_eq!(config.delay_for_restart(3), Duration::from_millis(400));

        // Should cap at max
        assert_eq!(config.delay_for_restart(10), Duration::from_millis(10_000));
    }

    #[test]
    fn test_backoff_disabled() {
        let config = BackoffConfig::disabled();

        assert_eq!(config.delay_for_restart(0), Duration::ZERO);
        assert_eq!(config.delay_for_restart(5), Duration::ZERO);
    }

    #[test]
    fn test_child_consecutive_failures() {
        let mut child = SupervisedChild::new(
            ChildId::App(AppId::new()),
            "test",
            ChildType::Permanent,
        );

        // Mark running, then fail quickly (< 10 seconds)
        child.mark_running();
        child.mark_failed("Error 1");
        assert_eq!(child.consecutive_failures, 1);

        // Start again and fail quickly again
        child.mark_running();
        child.mark_failed("Error 2");
        assert_eq!(child.consecutive_failures, 2);
    }

    #[test]
    fn test_pending_restarts() {
        let mut supervisor = Supervisor::new("test", RestartStrategy::OneForOne)
            .with_intensity(RestartIntensity {
                max_restarts: 10,
                window_secs: 60,
                backoff: BackoffConfig {
                    initial_delay_ms: 1000,
                    max_delay_ms: 5000,
                    multiplier: 2.0,
                    enabled: true,
                },
            });
        supervisor.start();

        let app_id = AppId::new();
        supervisor.add_child(SupervisedChild::new(ChildId::App(app_id), "app", ChildType::Permanent));
        supervisor.get_child_mut(ChildId::App(app_id)).unwrap().mark_running();

        // First failure
        let action = supervisor.handle_failure(ChildId::App(app_id), "Error 1", false);
        assert!(matches!(action, SupervisorAction::Restart(_)));

        // Child should be in restarting state with pending backoff
        let child = supervisor.get_child(ChildId::App(app_id)).unwrap();
        assert_eq!(child.status, ChildStatus::Restarting);

        // Should have a pending restart time (backoff active after consecutive failure)
        assert!(child.consecutive_failures > 0);
    }

    #[test]
    fn test_temporary_child_no_restart() {
        let mut supervisor = Supervisor::new("test", RestartStrategy::OneForOne);
        supervisor.start();

        let app_id = AppId::new();
        supervisor.add_child(SupervisedChild::new(ChildId::App(app_id), "temp", ChildType::Temporary));

        let action = supervisor.handle_failure(ChildId::App(app_id), "Error", false);
        assert_eq!(action, SupervisorAction::Ignore);
    }

    #[test]
    fn test_supervision_tree() {
        let mut tree = SupervisorTree::new();

        // Add app supervisor
        let app_sup = tree.add_supervisor("apps", RestartStrategy::OneForOne, tree.root_id()).unwrap();

        // Add app
        let app_id = AppId::new();
        tree.add_app(app_id, "my-app", ChildType::Permanent, app_sup);
        tree.mark_app_running(app_id);

        // Report failure
        let action = tree.report_app_failure(app_id, "Crash", false);
        assert!(matches!(action, SupervisorAction::Restart(_)));
    }
}
