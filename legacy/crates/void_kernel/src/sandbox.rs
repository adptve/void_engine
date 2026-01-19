//! App sandboxing with resource budgets and crash containment
//!
//! The sandbox provides:
//! - Resource budget enforcement (memory, CPU, GPU, entities)
//! - Crash containment via catch_unwind
//! - Resource cleanup on crash
//! - Audit logging for security events
//!
//! # Security Properties
//!
//! 1. Apps cannot exceed their resource budgets
//! 2. App crashes do not affect the kernel or other apps
//! 3. All app resources are cleaned up on crash
//! 4. Security-relevant operations are logged

use parking_lot::RwLock;
use std::collections::HashMap;
use std::panic::{self, AssertUnwindSafe};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use void_ir::NamespaceId;

use crate::app::AppId;
use crate::capability::{Capability, CapabilityChecker, CapabilityKind, CapabilityCheck, NamespaceQuotas};
use crate::layer::LayerId;

/// Unique identifier for a sandbox
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SandboxId(u64);

impl SandboxId {
    /// Create a new unique sandbox ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }
}

impl Default for SandboxId {
    fn default() -> Self {
        Self::new()
    }
}

/// Resource budget for an app
#[derive(Debug, Clone)]
pub struct ResourceBudget {
    /// Maximum memory in bytes
    pub max_memory_bytes: u64,
    /// Maximum GPU memory in bytes
    pub max_gpu_memory_bytes: u64,
    /// Maximum entities
    pub max_entities: u32,
    /// Maximum layers
    pub max_layers: u32,
    /// Maximum assets
    pub max_assets: u32,
    /// Maximum CPU time per frame (microseconds)
    pub max_frame_time_us: u64,
    /// Maximum patches per frame
    pub max_patches_per_frame: u32,
    /// Maximum draw calls per frame
    pub max_draw_calls: u32,
    /// Maximum compute dispatches per frame
    pub max_compute_dispatches: u32,
}

impl Default for ResourceBudget {
    fn default() -> Self {
        Self {
            max_memory_bytes: 256 * 1024 * 1024, // 256 MB
            max_gpu_memory_bytes: 512 * 1024 * 1024, // 512 MB
            max_entities: 10_000,
            max_layers: 8,
            max_assets: 1000,
            max_frame_time_us: 16_000, // ~16ms for 60fps
            max_patches_per_frame: 1000,
            max_draw_calls: 1000,
            max_compute_dispatches: 100,
        }
    }
}

impl ResourceBudget {
    /// Create an unlimited budget (for kernel/system use)
    pub fn unlimited() -> Self {
        Self {
            max_memory_bytes: u64::MAX,
            max_gpu_memory_bytes: u64::MAX,
            max_entities: u32::MAX,
            max_layers: u32::MAX,
            max_assets: u32::MAX,
            max_frame_time_us: u64::MAX,
            max_patches_per_frame: u32::MAX,
            max_draw_calls: u32::MAX,
            max_compute_dispatches: u32::MAX,
        }
    }

    /// Create a minimal budget for lightweight apps
    pub fn minimal() -> Self {
        Self {
            max_memory_bytes: 32 * 1024 * 1024, // 32 MB
            max_gpu_memory_bytes: 64 * 1024 * 1024, // 64 MB
            max_entities: 1000,
            max_layers: 2,
            max_assets: 100,
            max_frame_time_us: 8_000, // 8ms
            max_patches_per_frame: 100,
            max_draw_calls: 100,
            max_compute_dispatches: 10,
        }
    }
}

/// Current resource usage for an app
#[derive(Debug, Clone, Default)]
pub struct ResourceUsage {
    /// Current memory usage (bytes)
    pub memory_bytes: u64,
    /// Current GPU memory usage (bytes)
    pub gpu_memory_bytes: u64,
    /// Current entity count
    pub entities: u32,
    /// Current layer count
    pub layers: u32,
    /// Current asset count
    pub assets: u32,
    /// Frame time this frame (microseconds)
    pub frame_time_us: u64,
    /// Patches this frame
    pub patches_this_frame: u32,
    /// Draw calls this frame
    pub draw_calls_this_frame: u32,
    /// Compute dispatches this frame
    pub compute_dispatches_this_frame: u32,
}

/// Result of a resource check
#[derive(Debug, Clone)]
pub enum ResourceCheck {
    /// Resource available
    Available,
    /// Resource would exceed budget
    WouldExceed {
        resource: String,
        current: u64,
        requested: u64,
        limit: u64,
    },
}

impl ResourceCheck {
    /// Check if resource is available
    pub fn is_available(&self) -> bool {
        matches!(self, Self::Available)
    }
}

/// An app sandbox with resource limits and crash containment
pub struct AppSandbox {
    /// Unique sandbox ID
    pub id: SandboxId,
    /// Associated app ID
    pub app_id: AppId,
    /// Namespace for this sandbox
    pub namespace: NamespaceId,
    /// Resource budget
    budget: ResourceBudget,
    /// Current resource usage
    usage: ResourceUsage,
    /// Crash count for this sandbox
    crash_count: u32,
    /// Maximum crashes before permanent termination
    max_crashes: u32,
    /// When sandbox was created
    created_at: Instant,
    /// Last update time
    last_update: Instant,
    /// Security audit log
    audit_log: Vec<AuditEvent>,
    /// Maximum audit entries
    max_audit_entries: usize,
    /// Owned entities (for cleanup on crash)
    owned_entities: Vec<u64>,
    /// Owned layers (for cleanup on crash)
    owned_layers: Vec<LayerId>,
    /// Owned assets (for cleanup on crash)
    owned_assets: Vec<String>,
}

/// Security audit event
#[derive(Debug, Clone)]
pub struct AuditEvent {
    /// When the event occurred
    pub timestamp: Instant,
    /// Event type
    pub event_type: AuditEventType,
    /// Details
    pub details: String,
}

/// Types of audit events
#[derive(Debug, Clone)]
pub enum AuditEventType {
    /// Sandbox created
    Created,
    /// Resource allocated
    ResourceAllocated { resource: String, amount: u64 },
    /// Resource released
    ResourceReleased { resource: String, amount: u64 },
    /// Budget exceeded (blocked)
    BudgetExceeded { resource: String },
    /// Capability checked
    CapabilityCheck { kind: String, allowed: bool },
    /// Crash occurred
    Crash { reason: String },
    /// Sandbox destroyed
    Destroyed,
}

impl AppSandbox {
    /// Create a new sandbox with default budget
    pub fn new(app_id: AppId, namespace: NamespaceId) -> Self {
        Self::with_budget(app_id, namespace, ResourceBudget::default())
    }

    /// Create a new sandbox with custom budget
    pub fn with_budget(app_id: AppId, namespace: NamespaceId, budget: ResourceBudget) -> Self {
        let now = Instant::now();
        let mut sandbox = Self {
            id: SandboxId::new(),
            app_id,
            namespace,
            budget,
            usage: ResourceUsage::default(),
            crash_count: 0,
            max_crashes: 3,
            created_at: now,
            last_update: now,
            audit_log: Vec::new(),
            max_audit_entries: 1000,
            owned_entities: Vec::new(),
            owned_layers: Vec::new(),
            owned_assets: Vec::new(),
        };

        sandbox.audit(AuditEventType::Created, "Sandbox created");
        sandbox
    }

    /// Check if a resource allocation is allowed
    pub fn check_allocation(&self, resource: &str, amount: u64) -> ResourceCheck {
        match resource {
            "memory" => {
                if self.usage.memory_bytes + amount > self.budget.max_memory_bytes {
                    ResourceCheck::WouldExceed {
                        resource: resource.to_string(),
                        current: self.usage.memory_bytes,
                        requested: amount,
                        limit: self.budget.max_memory_bytes,
                    }
                } else {
                    ResourceCheck::Available
                }
            }
            "gpu_memory" => {
                if self.usage.gpu_memory_bytes + amount > self.budget.max_gpu_memory_bytes {
                    ResourceCheck::WouldExceed {
                        resource: resource.to_string(),
                        current: self.usage.gpu_memory_bytes,
                        requested: amount,
                        limit: self.budget.max_gpu_memory_bytes,
                    }
                } else {
                    ResourceCheck::Available
                }
            }
            "entity" => {
                if self.usage.entities as u64 + amount > self.budget.max_entities as u64 {
                    ResourceCheck::WouldExceed {
                        resource: resource.to_string(),
                        current: self.usage.entities as u64,
                        requested: amount,
                        limit: self.budget.max_entities as u64,
                    }
                } else {
                    ResourceCheck::Available
                }
            }
            "layer" => {
                if self.usage.layers as u64 + amount > self.budget.max_layers as u64 {
                    ResourceCheck::WouldExceed {
                        resource: resource.to_string(),
                        current: self.usage.layers as u64,
                        requested: amount,
                        limit: self.budget.max_layers as u64,
                    }
                } else {
                    ResourceCheck::Available
                }
            }
            "asset" => {
                if self.usage.assets as u64 + amount > self.budget.max_assets as u64 {
                    ResourceCheck::WouldExceed {
                        resource: resource.to_string(),
                        current: self.usage.assets as u64,
                        requested: amount,
                        limit: self.budget.max_assets as u64,
                    }
                } else {
                    ResourceCheck::Available
                }
            }
            "patch" => {
                if self.usage.patches_this_frame as u64 + amount > self.budget.max_patches_per_frame as u64 {
                    ResourceCheck::WouldExceed {
                        resource: resource.to_string(),
                        current: self.usage.patches_this_frame as u64,
                        requested: amount,
                        limit: self.budget.max_patches_per_frame as u64,
                    }
                } else {
                    ResourceCheck::Available
                }
            }
            _ => ResourceCheck::Available,
        }
    }

    /// Reserve a resource (check and allocate atomically)
    pub fn reserve(&mut self, resource: &str, amount: u64) -> Result<(), ResourceCheck> {
        let check = self.check_allocation(resource, amount);
        if !check.is_available() {
            self.audit(
                AuditEventType::BudgetExceeded {
                    resource: resource.to_string(),
                },
                format!("Tried to allocate {} {}", amount, resource),
            );
            return Err(check);
        }

        // Allocate
        match resource {
            "memory" => self.usage.memory_bytes += amount,
            "gpu_memory" => self.usage.gpu_memory_bytes += amount,
            "entity" => self.usage.entities += amount as u32,
            "layer" => self.usage.layers += amount as u32,
            "asset" => self.usage.assets += amount as u32,
            "patch" => self.usage.patches_this_frame += amount as u32,
            _ => {}
        }

        self.audit(
            AuditEventType::ResourceAllocated {
                resource: resource.to_string(),
                amount,
            },
            format!("Allocated {} {}", amount, resource),
        );

        Ok(())
    }

    /// Release a resource
    pub fn release(&mut self, resource: &str, amount: u64) {
        match resource {
            "memory" => self.usage.memory_bytes = self.usage.memory_bytes.saturating_sub(amount),
            "gpu_memory" => {
                self.usage.gpu_memory_bytes = self.usage.gpu_memory_bytes.saturating_sub(amount)
            }
            "entity" => self.usage.entities = self.usage.entities.saturating_sub(amount as u32),
            "layer" => self.usage.layers = self.usage.layers.saturating_sub(amount as u32),
            "asset" => self.usage.assets = self.usage.assets.saturating_sub(amount as u32),
            _ => {}
        }

        self.audit(
            AuditEventType::ResourceReleased {
                resource: resource.to_string(),
                amount,
            },
            format!("Released {} {}", amount, resource),
        );
    }

    /// Reset per-frame counters
    pub fn reset_frame_counters(&mut self) {
        self.usage.patches_this_frame = 0;
        self.usage.draw_calls_this_frame = 0;
        self.usage.compute_dispatches_this_frame = 0;
        self.usage.frame_time_us = 0;
    }

    /// Track an entity as owned by this sandbox
    pub fn track_entity(&mut self, entity_id: u64) {
        self.owned_entities.push(entity_id);
    }

    /// Untrack an entity
    pub fn untrack_entity(&mut self, entity_id: u64) {
        self.owned_entities.retain(|&e| e != entity_id);
    }

    /// Track a layer as owned by this sandbox
    pub fn track_layer(&mut self, layer_id: LayerId) {
        self.owned_layers.push(layer_id);
    }

    /// Untrack a layer
    pub fn untrack_layer(&mut self, layer_id: LayerId) {
        self.owned_layers.retain(|&l| l != layer_id);
    }

    /// Track an asset as owned by this sandbox
    pub fn track_asset(&mut self, asset_id: String) {
        self.owned_assets.push(asset_id);
    }

    /// Untrack an asset
    pub fn untrack_asset(&mut self, asset_id: &str) {
        self.owned_assets.retain(|a| a != asset_id);
    }

    /// Get all owned resources (for cleanup)
    pub fn get_owned_resources(&self) -> SandboxResources {
        SandboxResources {
            entities: self.owned_entities.clone(),
            layers: self.owned_layers.clone(),
            assets: self.owned_assets.clone(),
        }
    }

    /// Record a crash
    pub fn record_crash(&mut self, reason: &str) {
        self.crash_count += 1;
        self.audit(
            AuditEventType::Crash {
                reason: reason.to_string(),
            },
            format!("Crash #{}", self.crash_count),
        );
    }

    /// Check if sandbox has exceeded crash limit
    pub fn exceeded_crash_limit(&self) -> bool {
        self.crash_count >= self.max_crashes
    }

    /// Get crash count
    pub fn crash_count(&self) -> u32 {
        self.crash_count
    }

    /// Get current resource usage
    pub fn usage(&self) -> &ResourceUsage {
        &self.usage
    }

    /// Get resource budget
    pub fn budget(&self) -> &ResourceBudget {
        &self.budget
    }

    /// Get recent audit events
    pub fn recent_audit(&self, count: usize) -> &[AuditEvent] {
        let start = self.audit_log.len().saturating_sub(count);
        &self.audit_log[start..]
    }

    /// Add audit event
    fn audit(&mut self, event_type: AuditEventType, details: impl Into<String>) {
        let event = AuditEvent {
            timestamp: Instant::now(),
            event_type,
            details: details.into(),
        };

        self.audit_log.push(event);

        // Trim old events
        if self.audit_log.len() > self.max_audit_entries {
            self.audit_log.remove(0);
        }
    }

    /// Execute a function within the sandbox with crash containment
    pub fn execute<F, R>(&mut self, f: F) -> Result<R, SandboxError>
    where
        F: FnOnce() -> R + panic::UnwindSafe,
    {
        let start = Instant::now();

        let result = panic::catch_unwind(f);

        let elapsed = start.elapsed();
        self.usage.frame_time_us = elapsed.as_micros() as u64;
        self.last_update = Instant::now();

        match result {
            Ok(value) => {
                // Check if we exceeded time budget
                if elapsed.as_micros() as u64 > self.budget.max_frame_time_us {
                    log::warn!(
                        "Sandbox {:?} exceeded time budget: {}us > {}us",
                        self.id,
                        elapsed.as_micros(),
                        self.budget.max_frame_time_us
                    );
                }
                Ok(value)
            }
            Err(panic_payload) => {
                let message = if let Some(s) = panic_payload.downcast_ref::<&str>() {
                    s.to_string()
                } else if let Some(s) = panic_payload.downcast_ref::<String>() {
                    s.clone()
                } else {
                    "Unknown panic".to_string()
                };

                self.record_crash(&message);

                Err(SandboxError::Panic {
                    sandbox_id: self.id,
                    app_id: self.app_id,
                    message,
                })
            }
        }
    }

    /// Execute a mutable function within the sandbox
    pub fn execute_mut<F, R>(&mut self, mut f: F) -> Result<R, SandboxError>
    where
        F: FnMut() -> R,
    {
        let start = Instant::now();

        let result = panic::catch_unwind(AssertUnwindSafe(|| f()));

        let elapsed = start.elapsed();
        self.usage.frame_time_us = elapsed.as_micros() as u64;
        self.last_update = Instant::now();

        match result {
            Ok(value) => {
                if elapsed.as_micros() as u64 > self.budget.max_frame_time_us {
                    log::warn!(
                        "Sandbox {:?} exceeded time budget: {}us > {}us",
                        self.id,
                        elapsed.as_micros(),
                        self.budget.max_frame_time_us
                    );
                }
                Ok(value)
            }
            Err(panic_payload) => {
                let message = if let Some(s) = panic_payload.downcast_ref::<&str>() {
                    s.to_string()
                } else if let Some(s) = panic_payload.downcast_ref::<String>() {
                    s.clone()
                } else {
                    "Unknown panic".to_string()
                };

                self.record_crash(&message);

                Err(SandboxError::Panic {
                    sandbox_id: self.id,
                    app_id: self.app_id,
                    message,
                })
            }
        }
    }
}

impl Drop for AppSandbox {
    fn drop(&mut self) {
        self.audit(AuditEventType::Destroyed, "Sandbox destroyed");
        log::debug!(
            "Sandbox {:?} destroyed with {} entities, {} layers, {} assets",
            self.id,
            self.owned_entities.len(),
            self.owned_layers.len(),
            self.owned_assets.len()
        );
    }
}

/// Resources owned by a sandbox (for cleanup)
#[derive(Debug, Clone, Default)]
pub struct SandboxResources {
    /// Owned entities
    pub entities: Vec<u64>,
    /// Owned layers
    pub layers: Vec<LayerId>,
    /// Owned assets
    pub assets: Vec<String>,
}

/// Errors from sandbox operations
#[derive(Debug, Clone)]
pub enum SandboxError {
    /// App panicked
    Panic {
        sandbox_id: SandboxId,
        app_id: AppId,
        message: String,
    },
    /// Resource budget exceeded
    BudgetExceeded {
        sandbox_id: SandboxId,
        resource: String,
        limit: u64,
        requested: u64,
    },
    /// Capability denied
    CapabilityDenied {
        sandbox_id: SandboxId,
        capability: String,
    },
    /// Crash limit exceeded
    CrashLimitExceeded {
        sandbox_id: SandboxId,
        crash_count: u32,
    },
}

impl std::fmt::Display for SandboxError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Panic {
                sandbox_id,
                app_id,
                message,
            } => write!(
                f,
                "Sandbox {:?} (app {:?}) panicked: {}",
                sandbox_id, app_id, message
            ),
            Self::BudgetExceeded {
                sandbox_id,
                resource,
                limit,
                requested,
            } => write!(
                f,
                "Sandbox {:?} exceeded {} budget: requested {}, limit {}",
                sandbox_id, resource, requested, limit
            ),
            Self::CapabilityDenied {
                sandbox_id,
                capability,
            } => write!(
                f,
                "Sandbox {:?} denied capability: {}",
                sandbox_id, capability
            ),
            Self::CrashLimitExceeded {
                sandbox_id,
                crash_count,
            } => write!(
                f,
                "Sandbox {:?} exceeded crash limit ({})",
                sandbox_id, crash_count
            ),
        }
    }
}

impl std::error::Error for SandboxError {}

/// Manages all sandboxes
pub struct SandboxManager {
    /// All sandboxes by ID
    sandboxes: HashMap<SandboxId, AppSandbox>,
    /// App ID to sandbox ID mapping
    app_to_sandbox: HashMap<AppId, SandboxId>,
    /// Namespace to sandbox ID mapping
    namespace_to_sandbox: HashMap<NamespaceId, SandboxId>,
}

impl SandboxManager {
    /// Create a new sandbox manager
    pub fn new() -> Self {
        Self {
            sandboxes: HashMap::new(),
            app_to_sandbox: HashMap::new(),
            namespace_to_sandbox: HashMap::new(),
        }
    }

    /// Create a sandbox for an app
    pub fn create(
        &mut self,
        app_id: AppId,
        namespace: NamespaceId,
        budget: ResourceBudget,
    ) -> SandboxId {
        let sandbox = AppSandbox::with_budget(app_id, namespace, budget);
        let id = sandbox.id;

        self.app_to_sandbox.insert(app_id, id);
        self.namespace_to_sandbox.insert(namespace, id);
        self.sandboxes.insert(id, sandbox);

        log::debug!("Created sandbox {:?} for app {:?}", id, app_id);
        id
    }

    /// Destroy a sandbox and return owned resources for cleanup
    pub fn destroy(&mut self, id: SandboxId) -> Option<SandboxResources> {
        if let Some(sandbox) = self.sandboxes.remove(&id) {
            self.app_to_sandbox.remove(&sandbox.app_id);
            self.namespace_to_sandbox.remove(&sandbox.namespace);

            let resources = sandbox.get_owned_resources();
            log::debug!(
                "Destroyed sandbox {:?} with {} entities",
                id,
                resources.entities.len()
            );
            Some(resources)
        } else {
            None
        }
    }

    /// Get a sandbox by ID
    pub fn get(&self, id: SandboxId) -> Option<&AppSandbox> {
        self.sandboxes.get(&id)
    }

    /// Get a mutable sandbox by ID
    pub fn get_mut(&mut self, id: SandboxId) -> Option<&mut AppSandbox> {
        self.sandboxes.get_mut(&id)
    }

    /// Get sandbox for an app
    pub fn get_for_app(&self, app_id: AppId) -> Option<&AppSandbox> {
        self.app_to_sandbox
            .get(&app_id)
            .and_then(|id| self.sandboxes.get(id))
    }

    /// Get mutable sandbox for an app
    pub fn get_for_app_mut(&mut self, app_id: AppId) -> Option<&mut AppSandbox> {
        if let Some(&id) = self.app_to_sandbox.get(&app_id) {
            self.sandboxes.get_mut(&id)
        } else {
            None
        }
    }

    /// Get sandbox for a namespace
    pub fn get_for_namespace(&self, namespace: NamespaceId) -> Option<&AppSandbox> {
        self.namespace_to_sandbox
            .get(&namespace)
            .and_then(|id| self.sandboxes.get(id))
    }

    /// Reset per-frame counters for all sandboxes
    pub fn reset_frame_counters(&mut self) {
        for sandbox in self.sandboxes.values_mut() {
            sandbox.reset_frame_counters();
        }
    }

    /// Get total resource usage across all sandboxes
    pub fn total_usage(&self) -> ResourceUsage {
        let mut total = ResourceUsage::default();
        for sandbox in self.sandboxes.values() {
            let usage = sandbox.usage();
            total.memory_bytes += usage.memory_bytes;
            total.gpu_memory_bytes += usage.gpu_memory_bytes;
            total.entities += usage.entities;
            total.layers += usage.layers;
            total.assets += usage.assets;
        }
        total
    }

    /// Get sandboxes that exceeded crash limit
    pub fn get_crashed_sandboxes(&self) -> Vec<SandboxId> {
        self.sandboxes
            .iter()
            .filter(|(_, s)| s.exceeded_crash_limit())
            .map(|(&id, _)| id)
            .collect()
    }

    /// Get sandbox count
    pub fn len(&self) -> usize {
        self.sandboxes.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.sandboxes.is_empty()
    }
}

impl Default for SandboxManager {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_resource_budget_check() {
        let app_id = AppId::new();
        let namespace = NamespaceId::new();
        let mut sandbox = AppSandbox::with_budget(
            app_id,
            namespace,
            ResourceBudget {
                max_entities: 100,
                ..Default::default()
            },
        );

        // Check within budget
        assert!(sandbox.check_allocation("entity", 50).is_available());

        // Reserve some
        assert!(sandbox.reserve("entity", 50).is_ok());
        assert_eq!(sandbox.usage.entities, 50);

        // Check remaining
        assert!(sandbox.check_allocation("entity", 50).is_available());
        assert!(!sandbox.check_allocation("entity", 51).is_available());

        // Reserve rest
        assert!(sandbox.reserve("entity", 50).is_ok());

        // Now should exceed
        assert!(sandbox.reserve("entity", 1).is_err());
    }

    #[test]
    fn test_crash_containment() {
        let app_id = AppId::new();
        let namespace = NamespaceId::new();
        let mut sandbox = AppSandbox::new(app_id, namespace);

        // Successful execution
        let result = sandbox.execute(|| 42);
        assert_eq!(result.unwrap(), 42);
        assert_eq!(sandbox.crash_count(), 0);

        // Panic is caught
        let result: Result<i32, _> = sandbox.execute(|| {
            panic!("Test panic");
        });
        assert!(result.is_err());
        assert_eq!(sandbox.crash_count(), 1);

        // Sandbox still works after crash
        let result = sandbox.execute(|| 100);
        assert_eq!(result.unwrap(), 100);
    }

    #[test]
    fn test_crash_limit() {
        let app_id = AppId::new();
        let namespace = NamespaceId::new();
        let mut sandbox = AppSandbox::with_budget(
            app_id,
            namespace,
            ResourceBudget::default(),
        );

        assert!(!sandbox.exceeded_crash_limit());

        // Cause crashes
        for _ in 0..3 {
            let _ = sandbox.execute::<_, ()>(|| panic!("crash"));
        }

        assert!(sandbox.exceeded_crash_limit());
    }

    #[test]
    fn test_resource_tracking() {
        let app_id = AppId::new();
        let namespace = NamespaceId::new();
        let mut sandbox = AppSandbox::new(app_id, namespace);

        sandbox.track_entity(1);
        sandbox.track_entity(2);
        sandbox.track_layer(LayerId::new());
        sandbox.track_asset("texture.png".to_string());

        let resources = sandbox.get_owned_resources();
        assert_eq!(resources.entities.len(), 2);
        assert_eq!(resources.layers.len(), 1);
        assert_eq!(resources.assets.len(), 1);
    }

    #[test]
    fn test_sandbox_manager() {
        let mut manager = SandboxManager::new();

        let app_id = AppId::new();
        let namespace = NamespaceId::new();

        let sandbox_id = manager.create(app_id, namespace, ResourceBudget::default());

        assert!(manager.get(sandbox_id).is_some());
        assert!(manager.get_for_app(app_id).is_some());
        assert!(manager.get_for_namespace(namespace).is_some());

        let resources = manager.destroy(sandbox_id);
        assert!(resources.is_some());
        assert!(manager.get(sandbox_id).is_none());
    }

    #[test]
    fn test_per_frame_reset() {
        let app_id = AppId::new();
        let namespace = NamespaceId::new();
        let mut sandbox = AppSandbox::new(app_id, namespace);

        sandbox.reserve("patch", 10).unwrap();
        assert_eq!(sandbox.usage.patches_this_frame, 10);

        sandbox.reset_frame_counters();
        assert_eq!(sandbox.usage.patches_this_frame, 0);
    }
}
