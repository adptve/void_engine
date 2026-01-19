//! Crash recovery and state restoration
//!
//! This module handles:
//! - Catching panics in app code
//! - Restoring kernel state after failures
//! - Emergency shutdown procedures
//!
//! # Recovery Philosophy
//!
//! 1. Apps can crash - kernel must survive
//! 2. State can be corrupted - rollback must work
//! 3. GPU can hang - must be able to reset
//! 4. Memory can leak - must be able to GC

use parking_lot::RwLock;
use std::panic::{self, AssertUnwindSafe};
use std::sync::Arc;
use std::time::{Duration, Instant};

use crate::app::AppId;
use crate::layer::LayerId;
use crate::supervisor::{SupervisorAction, SupervisorTree};

/// Recovery context for an operation
pub struct RecoveryContext {
    /// Operation description
    pub operation: String,
    /// Start time
    pub started_at: Instant,
    /// Related app (if any)
    pub app_id: Option<AppId>,
    /// Related layer (if any)
    pub layer_id: Option<LayerId>,
}

impl RecoveryContext {
    /// Create a new recovery context
    pub fn new(operation: impl Into<String>) -> Self {
        Self {
            operation: operation.into(),
            started_at: Instant::now(),
            app_id: None,
            layer_id: None,
        }
    }

    /// Set the related app
    pub fn with_app(mut self, app_id: AppId) -> Self {
        self.app_id = Some(app_id);
        self
    }

    /// Set the related layer
    pub fn with_layer(mut self, layer_id: LayerId) -> Self {
        self.layer_id = Some(layer_id);
        self
    }
}

/// Result of a recovery attempt
#[derive(Debug, Clone)]
pub enum RecoveryResult {
    /// Operation succeeded
    Success,
    /// Operation failed but was recovered
    Recovered {
        error: String,
        action_taken: String,
    },
    /// Operation failed and could not be recovered
    Failed {
        error: String,
        fatal: bool,
    },
}

/// Panic info captured from catch_unwind
#[derive(Debug, Clone)]
pub struct PanicInfo {
    /// Panic message
    pub message: String,
    /// Location (file:line)
    pub location: Option<String>,
    /// Related operation
    pub operation: String,
    /// When it occurred
    pub timestamp: Instant,
}

impl PanicInfo {
    /// Extract panic info from a caught panic
    pub fn from_panic(payload: &Box<dyn std::any::Any + Send>, operation: &str) -> Self {
        let message = if let Some(s) = payload.downcast_ref::<&str>() {
            s.to_string()
        } else if let Some(s) = payload.downcast_ref::<String>() {
            s.clone()
        } else {
            "Unknown panic".to_string()
        };

        Self {
            message,
            location: None, // Would need panic hook to capture
            operation: operation.to_string(),
            timestamp: Instant::now(),
        }
    }
}

/// Recovery manager handles crash recovery
pub struct RecoveryManager {
    /// Supervisor tree for restart decisions
    supervisor_tree: Arc<RwLock<SupervisorTree>>,
    /// Recent panics
    panics: Vec<PanicInfo>,
    /// Maximum panics to track
    max_panics: usize,
    /// Emergency shutdown flag
    emergency_shutdown: bool,
}

impl RecoveryManager {
    /// Create a new recovery manager
    pub fn new(supervisor_tree: Arc<RwLock<SupervisorTree>>) -> Self {
        Self {
            supervisor_tree,
            panics: Vec::new(),
            max_panics: 100,
            emergency_shutdown: false,
        }
    }

    /// Execute an operation with panic recovery
    pub fn execute_with_recovery<F, R>(&mut self, context: RecoveryContext, f: F) -> RecoveryResult
    where
        F: FnOnce() -> Result<R, String> + panic::UnwindSafe,
    {
        let result = panic::catch_unwind(f);

        match result {
            Ok(Ok(_)) => RecoveryResult::Success,
            Ok(Err(e)) => {
                // Operation returned an error, but didn't panic
                log::warn!("Operation '{}' failed: {}", context.operation, e);
                self.handle_error(&context, &e)
            }
            Err(panic_payload) => {
                // Operation panicked
                let panic_info = PanicInfo::from_panic(&panic_payload, &context.operation);
                log::error!(
                    "Panic in operation '{}': {}",
                    context.operation,
                    panic_info.message
                );
                self.handle_panic(&context, panic_info)
            }
        }
    }

    /// Execute an app operation with recovery
    pub fn execute_app_operation<F, R>(
        &mut self,
        app_id: AppId,
        operation: &str,
        f: F,
    ) -> RecoveryResult
    where
        F: FnOnce() -> Result<R, String> + panic::UnwindSafe,
    {
        let context = RecoveryContext::new(operation).with_app(app_id);
        self.execute_with_recovery(context, f)
    }

    /// Handle a non-panic error
    fn handle_error(&mut self, context: &RecoveryContext, error: &str) -> RecoveryResult {
        // If this is app-related, report to supervisor
        if let Some(app_id) = context.app_id {
            let action = self.supervisor_tree.write().report_app_failure(
                app_id,
                error,
                false, // Not a normal exit
            );

            match action {
                SupervisorAction::Restart(ids) => RecoveryResult::Recovered {
                    error: error.to_string(),
                    action_taken: format!("Restarting {} entities", ids.len()),
                },
                SupervisorAction::Escalate => RecoveryResult::Failed {
                    error: error.to_string(),
                    fatal: false,
                },
                SupervisorAction::Shutdown => RecoveryResult::Failed {
                    error: error.to_string(),
                    fatal: true,
                },
                SupervisorAction::Ignore => RecoveryResult::Success,
            }
        } else {
            RecoveryResult::Failed {
                error: error.to_string(),
                fatal: false,
            }
        }
    }

    /// Handle a panic
    fn handle_panic(&mut self, context: &RecoveryContext, panic_info: PanicInfo) -> RecoveryResult {
        // Record panic
        self.panics.push(panic_info.clone());
        if self.panics.len() > self.max_panics {
            self.panics.remove(0);
        }

        // If app-related, kill the app and potentially restart
        if let Some(app_id) = context.app_id {
            let action = self.supervisor_tree.write().report_app_failure(
                app_id,
                &panic_info.message,
                false,
            );

            match action {
                SupervisorAction::Restart(ids) => RecoveryResult::Recovered {
                    error: panic_info.message,
                    action_taken: format!("Restarting {} entities after panic", ids.len()),
                },
                SupervisorAction::Escalate => {
                    log::error!("Supervisor escalation after panic");
                    RecoveryResult::Failed {
                        error: panic_info.message,
                        fatal: false,
                    }
                }
                SupervisorAction::Shutdown => {
                    log::error!("Shutdown triggered after panic");
                    self.emergency_shutdown = true;
                    RecoveryResult::Failed {
                        error: panic_info.message,
                        fatal: true,
                    }
                }
                SupervisorAction::Ignore => RecoveryResult::Success,
            }
        } else {
            // Non-app panic - this is more serious
            log::error!("Non-app panic in '{}'", context.operation);
            RecoveryResult::Failed {
                error: panic_info.message,
                fatal: false,
            }
        }
    }

    /// Check if emergency shutdown is needed
    pub fn needs_shutdown(&self) -> bool {
        self.emergency_shutdown
    }

    /// Get recent panics
    pub fn recent_panics(&self) -> &[PanicInfo] {
        &self.panics
    }

    /// Clear panic history
    pub fn clear_panics(&mut self) {
        self.panics.clear();
    }

    /// Perform emergency recovery actions
    pub fn emergency_recovery(&mut self) -> EmergencyRecoveryResult {
        log::warn!("Performing emergency recovery");

        let mut result = EmergencyRecoveryResult {
            apps_killed: 0,
            layers_destroyed: 0,
            assets_unloaded: 0,
            memory_freed: 0,
        };

        // In a full implementation, we would:
        // 1. Kill all apps
        // 2. Destroy all layers
        // 3. Unload all assets
        // 4. Force GC
        // 5. Reset GPU state

        log::warn!("Emergency recovery complete: {:?}", result);
        result
    }

    /// Get recovery statistics
    pub fn stats(&self) -> RecoveryStats {
        RecoveryStats {
            total_panics: self.panics.len(),
            emergency_shutdown_pending: self.emergency_shutdown,
            recent_panic_count: self
                .panics
                .iter()
                .filter(|p| p.timestamp.elapsed().as_secs() < 60)
                .count(),
        }
    }
}

/// Result of emergency recovery
#[derive(Debug, Clone)]
pub struct EmergencyRecoveryResult {
    pub apps_killed: u32,
    pub layers_destroyed: u32,
    pub assets_unloaded: u32,
    pub memory_freed: u64,
}

/// Recovery statistics
#[derive(Debug, Clone)]
pub struct RecoveryStats {
    pub total_panics: usize,
    pub emergency_shutdown_pending: bool,
    pub recent_panic_count: usize,
}

// =============================================================================
// STATE SNAPSHOT SYSTEM
// =============================================================================

/// Snapshot of recoverable state at a point in time
#[derive(Debug, Clone)]
pub struct StateSnapshot {
    /// Snapshot ID
    pub id: SnapshotId,
    /// Frame number when snapshot was taken
    pub frame: u64,
    /// When the snapshot was created
    pub created_at: Instant,
    /// App states at snapshot time
    pub app_states: Vec<AppSnapshot>,
    /// Layer states at snapshot time
    pub layer_states: Vec<LayerSnapshot>,
    /// Description of what triggered the snapshot
    pub reason: String,
    /// Whether this is a rollback target
    pub is_rollback_point: bool,
}

/// Unique identifier for a snapshot
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SnapshotId(u64);

impl SnapshotId {
    pub fn new() -> Self {
        use std::sync::atomic::{AtomicU64, Ordering};
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for SnapshotId {
    fn default() -> Self {
        Self::new()
    }
}

/// Snapshot of an app's state
#[derive(Debug, Clone)]
pub struct AppSnapshot {
    pub app_id: AppId,
    pub name: String,
    pub state: crate::app::AppState,
    pub layer_ids: Vec<LayerId>,
}

/// Snapshot of a layer's state
#[derive(Debug, Clone)]
pub struct LayerSnapshot {
    pub layer_id: LayerId,
    pub name: String,
    pub visible: bool,
    pub z_index: i32,
    pub owner_app: Option<AppId>,
}

/// Manages state snapshots for rollback
pub struct SnapshotManager {
    /// Maximum number of snapshots to keep
    max_snapshots: usize,
    /// Stored snapshots (oldest first)
    snapshots: Vec<StateSnapshot>,
    /// Last known good snapshot ID
    last_known_good: Option<SnapshotId>,
}

impl SnapshotManager {
    /// Create a new snapshot manager
    pub fn new(max_snapshots: usize) -> Self {
        Self {
            max_snapshots,
            snapshots: Vec::with_capacity(max_snapshots),
            last_known_good: None,
        }
    }

    /// Take a snapshot of current state
    pub fn take_snapshot(
        &mut self,
        frame: u64,
        app_states: Vec<AppSnapshot>,
        layer_states: Vec<LayerSnapshot>,
        reason: impl Into<String>,
        is_rollback_point: bool,
    ) -> SnapshotId {
        let snapshot = StateSnapshot {
            id: SnapshotId::new(),
            frame,
            created_at: Instant::now(),
            app_states,
            layer_states,
            reason: reason.into(),
            is_rollback_point,
        };

        let id = snapshot.id;

        // If this is a rollback point, mark it as last known good
        if is_rollback_point {
            self.last_known_good = Some(id);
        }

        self.snapshots.push(snapshot);

        // Evict old snapshots, but keep rollback points longer
        while self.snapshots.len() > self.max_snapshots {
            // Find first non-rollback snapshot to remove
            if let Some(idx) = self.snapshots.iter().position(|s| !s.is_rollback_point) {
                self.snapshots.remove(idx);
            } else {
                // All are rollback points, remove oldest
                self.snapshots.remove(0);
            }
        }

        log::debug!("Created snapshot {} at frame {}", id.raw(), frame);
        id
    }

    /// Get the last known good snapshot
    pub fn last_known_good(&self) -> Option<&StateSnapshot> {
        self.last_known_good.and_then(|id| self.get_snapshot(id))
    }

    /// Get a snapshot by ID
    pub fn get_snapshot(&self, id: SnapshotId) -> Option<&StateSnapshot> {
        self.snapshots.iter().find(|s| s.id == id)
    }

    /// Get the most recent snapshot
    pub fn latest(&self) -> Option<&StateSnapshot> {
        self.snapshots.last()
    }

    /// Get all snapshots
    pub fn all_snapshots(&self) -> &[StateSnapshot] {
        &self.snapshots
    }

    /// Clear old snapshots but keep rollback points
    pub fn gc(&mut self, max_age_secs: u64) {
        let cutoff = Duration::from_secs(max_age_secs);
        self.snapshots.retain(|s| {
            s.is_rollback_point || s.created_at.elapsed() < cutoff
        });
    }
}

// =============================================================================
// HOT-SWAP LIFECYCLE
// =============================================================================

/// Stage in the hot-swap lifecycle
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum HotSwapStage {
    /// Loading new version, validating, compiling
    Stage,
    /// Warming caches, allocating resources
    Prepare,
    /// Atomic switch at frame boundary
    Commit,
    /// Monitoring for N frames, checking health
    Verify,
    /// Success: mark old for cleanup
    Retire,
    /// Failure: switch back to old version
    Rollback,
    /// Hot-swap completed
    Complete,
    /// Hot-swap failed
    Failed,
}

/// A hot-swap operation in progress
#[derive(Debug)]
pub struct HotSwapOperation {
    /// Operation ID
    pub id: HotSwapId,
    /// What kind of swap
    pub kind: HotSwapKind,
    /// Current stage
    pub stage: HotSwapStage,
    /// When the operation started
    pub started_at: Instant,
    /// Frame when commit happened
    pub commit_frame: Option<u64>,
    /// Frames to monitor after commit
    pub verify_frames: u32,
    /// Snapshot taken before commit (for rollback)
    pub rollback_snapshot: Option<SnapshotId>,
    /// Error if failed
    pub error: Option<String>,
    /// Verification errors collected during verify phase
    pub verification_errors: Vec<String>,
}

/// Unique identifier for a hot-swap operation
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct HotSwapId(u64);

impl HotSwapId {
    pub fn new() -> Self {
        use std::sync::atomic::{AtomicU64, Ordering};
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }
}

impl Default for HotSwapId {
    fn default() -> Self {
        Self::new()
    }
}

/// Kind of hot-swap operation
#[derive(Debug, Clone)]
pub enum HotSwapKind {
    /// Hot-swap an app
    App {
        app_id: AppId,
        old_version: String,
        new_version: String,
    },
    /// Hot-swap a shader
    Shader {
        shader_name: String,
        old_hash: u64,
        new_hash: u64,
    },
    /// Hot-swap an asset
    Asset {
        asset_path: String,
        old_version: u32,
        new_version: u32,
    },
    /// Switch rendering backend
    Backend {
        from: String,
        to: String,
    },
}

impl HotSwapOperation {
    /// Create a new hot-swap operation
    pub fn new(kind: HotSwapKind, verify_frames: u32) -> Self {
        Self {
            id: HotSwapId::new(),
            kind,
            stage: HotSwapStage::Stage,
            started_at: Instant::now(),
            commit_frame: None,
            verify_frames,
            rollback_snapshot: None,
            error: None,
            verification_errors: Vec::new(),
        }
    }

    /// Advance to next stage
    pub fn advance(&mut self, new_stage: HotSwapStage) {
        log::info!(
            "Hot-swap {} advancing from {:?} to {:?}",
            self.id.0,
            self.stage,
            new_stage
        );
        self.stage = new_stage;
    }

    /// Record commit frame
    pub fn record_commit(&mut self, frame: u64, snapshot_id: SnapshotId) {
        self.commit_frame = Some(frame);
        self.rollback_snapshot = Some(snapshot_id);
        self.stage = HotSwapStage::Commit;
    }

    /// Add a verification error
    pub fn add_verification_error(&mut self, error: impl Into<String>) {
        self.verification_errors.push(error.into());
    }

    /// Check if verification period is complete
    pub fn is_verification_complete(&self, current_frame: u64) -> bool {
        if let Some(commit_frame) = self.commit_frame {
            current_frame >= commit_frame + self.verify_frames as u64
        } else {
            false
        }
    }

    /// Check if verification passed
    pub fn verification_passed(&self) -> bool {
        self.verification_errors.is_empty()
    }

    /// Mark as failed
    pub fn mark_failed(&mut self, error: impl Into<String>) {
        self.error = Some(error.into());
        self.stage = HotSwapStage::Failed;
    }

    /// Duration since start
    pub fn elapsed(&self) -> Duration {
        self.started_at.elapsed()
    }
}

/// Manages hot-swap operations
pub struct HotSwapManager {
    /// Active operations
    operations: Vec<HotSwapOperation>,
    /// Completed operations (for history)
    completed: Vec<HotSwapOperation>,
    /// Maximum history to keep
    max_history: usize,
    /// Default verification frames
    default_verify_frames: u32,
}

impl HotSwapManager {
    /// Create a new hot-swap manager
    pub fn new() -> Self {
        Self {
            operations: Vec::new(),
            completed: Vec::new(),
            max_history: 100,
            default_verify_frames: 60, // ~1 second at 60fps
        }
    }

    /// Start a new hot-swap operation
    pub fn start(&mut self, kind: HotSwapKind) -> HotSwapId {
        let op = HotSwapOperation::new(kind, self.default_verify_frames);
        let id = op.id;
        self.operations.push(op);
        log::info!("Started hot-swap operation {}", id.0);
        id
    }

    /// Get an operation by ID
    pub fn get(&self, id: HotSwapId) -> Option<&HotSwapOperation> {
        self.operations.iter().find(|op| op.id == id)
    }

    /// Get a mutable operation by ID
    pub fn get_mut(&mut self, id: HotSwapId) -> Option<&mut HotSwapOperation> {
        self.operations.iter_mut().find(|op| op.id == id)
    }

    /// Complete an operation (moves to history)
    pub fn complete(&mut self, id: HotSwapId) {
        if let Some(idx) = self.operations.iter().position(|op| op.id == id) {
            let mut op = self.operations.remove(idx);
            op.stage = HotSwapStage::Complete;
            log::info!("Hot-swap {} completed in {:?}", id.0, op.elapsed());
            self.completed.push(op);

            // Trim history
            while self.completed.len() > self.max_history {
                self.completed.remove(0);
            }
        }
    }

    /// Get all active operations
    pub fn active(&self) -> &[HotSwapOperation] {
        &self.operations
    }

    /// Get operations that need verification check
    pub fn needs_verification(&self, current_frame: u64) -> Vec<HotSwapId> {
        self.operations
            .iter()
            .filter(|op| {
                op.stage == HotSwapStage::Verify && op.is_verification_complete(current_frame)
            })
            .map(|op| op.id)
            .collect()
    }

    /// Get operation statistics
    pub fn stats(&self) -> HotSwapStats {
        let total_completed = self.completed.len();
        let successful = self.completed
            .iter()
            .filter(|op| op.stage == HotSwapStage::Complete && op.error.is_none())
            .count();
        let failed = self.completed
            .iter()
            .filter(|op| op.stage == HotSwapStage::Failed || op.error.is_some())
            .count();
        let rolled_back = self.completed
            .iter()
            .filter(|op| op.stage == HotSwapStage::Rollback)
            .count();

        HotSwapStats {
            active: self.operations.len(),
            completed: total_completed,
            successful,
            failed,
            rolled_back,
        }
    }
}

impl Default for HotSwapManager {
    fn default() -> Self {
        Self::new()
    }
}

/// Hot-swap statistics
#[derive(Debug, Clone)]
pub struct HotSwapStats {
    pub active: usize,
    pub completed: usize,
    pub successful: usize,
    pub failed: usize,
    pub rolled_back: usize,
}

/// Guard that ensures cleanup on panic
pub struct PanicGuard<F>
where
    F: FnMut(),
{
    cleanup: Option<F>,
    defused: bool,
}

impl<F> PanicGuard<F>
where
    F: FnMut(),
{
    /// Create a new panic guard with cleanup function
    pub fn new(cleanup: F) -> Self {
        Self {
            cleanup: Some(cleanup),
            defused: false,
        }
    }

    /// Defuse the guard (prevent cleanup from running)
    pub fn defuse(&mut self) {
        self.defused = true;
    }
}

impl<F> Drop for PanicGuard<F>
where
    F: FnMut(),
{
    fn drop(&mut self) {
        if !self.defused {
            if let Some(mut cleanup) = self.cleanup.take() {
                cleanup();
            }
        }
    }
}

/// Safe wrapper for app callback execution
pub fn execute_app_callback<F, R>(app_id: AppId, callback_name: &str, f: F) -> Result<R, String>
where
    F: FnOnce() -> R + panic::UnwindSafe,
{
    match panic::catch_unwind(f) {
        Ok(result) => Ok(result),
        Err(_) => Err(format!(
            "App {:?} panicked in callback '{}'",
            app_id, callback_name
        )),
    }
}

/// Safe wrapper for any closure that might panic
pub fn catch_panic<F, R>(f: F) -> Result<R, String>
where
    F: FnOnce() -> R + panic::UnwindSafe,
{
    match panic::catch_unwind(f) {
        Ok(result) => Ok(result),
        Err(payload) => {
            let message = if let Some(s) = payload.downcast_ref::<&str>() {
                s.to_string()
            } else if let Some(s) = payload.downcast_ref::<String>() {
                s.clone()
            } else {
                "Unknown panic".to_string()
            };
            Err(message)
        }
    }
}

/// Safe wrapper for mutable closures
pub fn catch_panic_mut<F, R>(mut f: F) -> Result<R, String>
where
    F: FnMut() -> R,
{
    match panic::catch_unwind(AssertUnwindSafe(|| f())) {
        Ok(result) => Ok(result),
        Err(payload) => {
            let message = if let Some(s) = payload.downcast_ref::<&str>() {
                s.to_string()
            } else if let Some(s) = payload.downcast_ref::<String>() {
                s.clone()
            } else {
                "Unknown panic".to_string()
            };
            Err(message)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn create_test_supervisor_tree() -> Arc<RwLock<SupervisorTree>> {
        Arc::new(RwLock::new(SupervisorTree::new()))
    }

    #[test]
    fn test_catch_panic() {
        // Should succeed
        let result = catch_panic(|| 42);
        assert_eq!(result, Ok(42));

        // Should catch panic
        let result: Result<i32, String> = catch_panic(|| {
            panic!("Test panic");
        });
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Test panic"));
    }

    #[test]
    fn test_recovery_context() {
        let ctx = RecoveryContext::new("test_operation");
        assert_eq!(ctx.operation, "test_operation");
        assert!(ctx.app_id.is_none());

        let app_id = AppId::new();
        let ctx = ctx.with_app(app_id);
        assert!(ctx.app_id.is_some());
    }

    #[test]
    fn test_panic_guard() {
        use std::sync::atomic::{AtomicBool, Ordering};

        let cleanup_ran = Arc::new(AtomicBool::new(false));
        let cleanup_ran_clone = Arc::clone(&cleanup_ran);

        // Test that cleanup runs on panic
        let result = panic::catch_unwind(AssertUnwindSafe(|| {
            let _guard = PanicGuard::new(move || {
                cleanup_ran_clone.store(true, Ordering::SeqCst);
            });
            panic!("Test");
        }));

        assert!(result.is_err());
        assert!(cleanup_ran.load(Ordering::SeqCst));
    }

    #[test]
    fn test_panic_guard_defuse() {
        use std::sync::atomic::{AtomicBool, Ordering};

        let cleanup_ran = Arc::new(AtomicBool::new(false));
        let cleanup_ran_clone = Arc::clone(&cleanup_ran);

        {
            let mut guard = PanicGuard::new(move || {
                cleanup_ran_clone.store(true, Ordering::SeqCst);
            });
            guard.defuse();
        }

        // Cleanup should not have run because guard was defused
        assert!(!cleanup_ran.load(Ordering::SeqCst));
    }

    #[test]
    fn test_recovery_manager_success() {
        let tree = create_test_supervisor_tree();
        let mut manager = RecoveryManager::new(tree);

        let result = manager.execute_with_recovery(
            RecoveryContext::new("test"),
            || Ok::<_, String>(42),
        );

        assert!(matches!(result, RecoveryResult::Success));
    }

    #[test]
    fn test_recovery_manager_error() {
        let tree = create_test_supervisor_tree();
        let mut manager = RecoveryManager::new(tree);

        let result = manager.execute_with_recovery(
            RecoveryContext::new("test"),
            || Err::<i32, _>("Test error".to_string()),
        );

        assert!(matches!(result, RecoveryResult::Failed { .. }));
    }

    #[test]
    fn test_snapshot_manager() {
        let mut manager = SnapshotManager::new(5);

        // Take a snapshot
        let id = manager.take_snapshot(
            100,
            vec![],
            vec![],
            "Test snapshot",
            true,
        );

        assert!(manager.get_snapshot(id).is_some());
        assert!(manager.last_known_good().is_some());
        assert_eq!(manager.latest().unwrap().frame, 100);
    }

    #[test]
    fn test_snapshot_eviction() {
        let mut manager = SnapshotManager::new(3);

        // Take 5 snapshots, only 3 should remain
        for i in 0..5 {
            manager.take_snapshot(i as u64, vec![], vec![], format!("Snap {}", i), false);
        }

        assert_eq!(manager.all_snapshots().len(), 3);
    }

    #[test]
    fn test_hotswap_lifecycle() {
        let mut manager = HotSwapManager::new();

        let id = manager.start(HotSwapKind::Shader {
            shader_name: "test.wgsl".to_string(),
            old_hash: 1234,
            new_hash: 5678,
        });

        let op = manager.get_mut(id).unwrap();
        assert_eq!(op.stage, HotSwapStage::Stage);

        // Advance through stages
        op.advance(HotSwapStage::Prepare);
        assert_eq!(op.stage, HotSwapStage::Prepare);

        // Record commit
        op.record_commit(100, SnapshotId::new());
        assert_eq!(op.commit_frame, Some(100));

        // Verify
        op.advance(HotSwapStage::Verify);

        // Check verification complete at frame 160 (60 verify frames default)
        assert!(!op.is_verification_complete(150));
        assert!(op.is_verification_complete(160));

        // Complete the operation
        manager.complete(id);

        let stats = manager.stats();
        assert_eq!(stats.active, 0);
        assert_eq!(stats.completed, 1);
    }

    #[test]
    fn test_hotswap_verification_failure() {
        let mut manager = HotSwapManager::new();

        let id = manager.start(HotSwapKind::Backend {
            from: "vulkan".to_string(),
            to: "webgpu".to_string(),
        });

        let op = manager.get_mut(id).unwrap();
        op.advance(HotSwapStage::Verify);
        op.add_verification_error("GPU context lost");

        assert!(!op.verification_passed());
        assert_eq!(op.verification_errors.len(), 1);
    }
}
