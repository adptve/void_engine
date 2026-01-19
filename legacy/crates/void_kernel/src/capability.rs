//! Capability-based access control
//!
//! Inspired by seL4's capability model, this module implements unforgeable
//! permission tokens that explicitly grant access to kernel resources.
//!
//! # Design Principles
//!
//! 1. **Unforgeable** - Capabilities cannot be guessed or manufactured
//! 2. **Explicit** - All permissions must be explicitly granted
//! 3. **Revocable** - Capabilities can be revoked at any time
//! 4. **Fine-grained** - Specific capabilities for specific operations
//! 5. **Auditable** - All capability operations are logged

use parking_lot::RwLock;
use std::collections::{HashMap, HashSet};
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Instant;

use crate::app::AppId;
use crate::layer::LayerId;
use void_ir::NamespaceId;

/// Unique identifier for a capability
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct CapabilityId(u64);

impl CapabilityId {
    /// Create a new unique capability ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for CapabilityId {
    fn default() -> Self {
        Self::new()
    }
}

/// Type of capability
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CapabilityKind {
    /// Create entities (optionally limited to max count)
    CreateEntities { max: Option<u32> },

    /// Destroy entities
    DestroyEntities,

    /// Modify components (optionally limited to specific component types)
    ModifyComponents { allowed_types: Option<Vec<String>> },

    /// Create layers (optionally limited to max count)
    CreateLayers { max: Option<u32> },

    /// Modify layers (optionally limited to specific layer IDs)
    ModifyLayers { allowed_layers: Option<Vec<LayerId>> },

    /// Load assets (optionally limited to path patterns)
    LoadAssets { allowed_paths: Option<Vec<String>> },

    /// Access network (optionally limited to specific hosts)
    AccessNetwork { allowed_hosts: Option<Vec<String>> },

    /// Access filesystem (optionally limited to specific paths)
    AccessFilesystem { allowed_paths: Option<Vec<String>> },

    /// Read entities from other namespaces
    CrossNamespaceRead { allowed_namespaces: Option<Vec<NamespaceId>> },

    /// Execute scripts
    ExecuteScripts,

    /// Admin: Hot-swap modules
    HotSwap,

    /// Admin: Manage capabilities for other namespaces
    ManageCapabilities,

    /// Admin: Full kernel access
    KernelAdmin,
}

impl CapabilityKind {
    /// Check if this is an admin capability
    pub fn is_admin(&self) -> bool {
        matches!(
            self,
            Self::HotSwap | Self::ManageCapabilities | Self::KernelAdmin
        )
    }

    /// Get a human-readable name
    pub fn name(&self) -> &'static str {
        match self {
            Self::CreateEntities { .. } => "create_entities",
            Self::DestroyEntities => "destroy_entities",
            Self::ModifyComponents { .. } => "modify_components",
            Self::CreateLayers { .. } => "create_layers",
            Self::ModifyLayers { .. } => "modify_layers",
            Self::LoadAssets { .. } => "load_assets",
            Self::AccessNetwork { .. } => "access_network",
            Self::AccessFilesystem { .. } => "access_filesystem",
            Self::CrossNamespaceRead { .. } => "cross_namespace_read",
            Self::ExecuteScripts => "execute_scripts",
            Self::HotSwap => "hot_swap",
            Self::ManageCapabilities => "manage_capabilities",
            Self::KernelAdmin => "kernel_admin",
        }
    }
}

/// A capability token
#[derive(Debug, Clone)]
pub struct Capability {
    /// Unique identifier
    pub id: CapabilityId,
    /// Kind of capability
    pub kind: CapabilityKind,
    /// Namespace that holds this capability
    pub holder: NamespaceId,
    /// Namespace that granted this capability
    pub grantor: NamespaceId,
    /// When this capability was created
    pub created_at: Instant,
    /// When this capability expires (None = never)
    pub expires_at: Option<Instant>,
    /// Whether this capability can be delegated to others
    pub delegable: bool,
    /// Reason for granting (for auditing)
    pub reason: Option<String>,
}

impl Capability {
    /// Create a new capability
    pub fn new(kind: CapabilityKind, holder: NamespaceId, grantor: NamespaceId) -> Self {
        Self {
            id: CapabilityId::new(),
            kind,
            holder,
            grantor,
            created_at: Instant::now(),
            expires_at: None,
            delegable: false,
            reason: None,
        }
    }

    /// Set expiration
    pub fn with_expiry(mut self, expires_at: Instant) -> Self {
        self.expires_at = Some(expires_at);
        self
    }

    /// Set as delegable
    pub fn delegable(mut self) -> Self {
        self.delegable = true;
        self
    }

    /// Set reason
    pub fn with_reason(mut self, reason: impl Into<String>) -> Self {
        self.reason = Some(reason.into());
        self
    }

    /// Check if capability is expired
    pub fn is_expired(&self) -> bool {
        if let Some(expires) = self.expires_at {
            Instant::now() > expires
        } else {
            false
        }
    }

    /// Check if capability is valid
    pub fn is_valid(&self) -> bool {
        !self.is_expired()
    }
}

/// Result of a capability check
#[derive(Debug, Clone)]
pub enum CapabilityCheck {
    /// Capability granted
    Allowed,
    /// Capability denied
    Denied { reason: String },
    /// Capability would exceed quota
    QuotaExceeded { limit: u32, current: u32 },
    /// Capability expired
    Expired,
}

impl CapabilityCheck {
    pub fn is_allowed(&self) -> bool {
        matches!(self, Self::Allowed)
    }
}

/// Audit log entry
#[derive(Debug, Clone)]
pub struct AuditEntry {
    /// When this happened
    pub timestamp: Instant,
    /// What namespace
    pub namespace: NamespaceId,
    /// What operation
    pub operation: String,
    /// What capability was used
    pub capability_id: Option<CapabilityId>,
    /// Result
    pub result: CapabilityCheck,
}

/// Runtime quota tracking per namespace
#[derive(Debug, Default)]
pub struct NamespaceQuotas {
    /// Current entity count
    pub entities: u32,
    /// Current layer count
    pub layers: u32,
    /// Current asset count
    pub assets: u32,
    /// Patches this frame
    pub patches_this_frame: u32,
    /// Memory usage (bytes)
    pub memory_bytes: u64,
}

/// The capability checker manages all capability checks
pub struct CapabilityChecker {
    /// All capabilities by namespace
    capabilities: HashMap<NamespaceId, Vec<Capability>>,
    /// Quick lookup: namespace -> capability kinds they have
    capability_index: HashMap<NamespaceId, HashSet<String>>,
    /// Runtime quota tracking
    quotas: HashMap<NamespaceId, NamespaceQuotas>,
    /// Audit log
    audit_log: Vec<AuditEntry>,
    /// Maximum audit log entries
    max_audit_entries: usize,
    /// Kernel namespace (has all capabilities)
    kernel_namespace: NamespaceId,
}

impl CapabilityChecker {
    /// Create a new capability checker
    pub fn new(kernel_namespace: NamespaceId) -> Self {
        Self {
            capabilities: HashMap::new(),
            capability_index: HashMap::new(),
            quotas: HashMap::new(),
            audit_log: Vec::new(),
            max_audit_entries: 10000,
            kernel_namespace,
        }
    }

    /// Grant a capability
    pub fn grant(&mut self, capability: Capability) {
        let holder = capability.holder;
        let kind_name = capability.kind.name().to_string();

        // Add to capabilities list
        self.capabilities
            .entry(holder)
            .or_default()
            .push(capability.clone());

        // Update index
        self.capability_index
            .entry(holder)
            .or_default()
            .insert(kind_name);

        log::debug!(
            "Granted capability {} to namespace {}",
            capability.kind.name(),
            holder
        );
    }

    /// Grant default capabilities for a new app
    pub fn grant_default_app_capabilities(&mut self, namespace: NamespaceId, grantor: NamespaceId) {
        // Standard app capabilities
        self.grant(Capability::new(
            CapabilityKind::CreateEntities { max: Some(10000) },
            namespace,
            grantor,
        ));
        self.grant(Capability::new(
            CapabilityKind::DestroyEntities,
            namespace,
            grantor,
        ));
        self.grant(Capability::new(
            CapabilityKind::ModifyComponents { allowed_types: None },
            namespace,
            grantor,
        ));
        self.grant(Capability::new(
            CapabilityKind::CreateLayers { max: Some(8) },
            namespace,
            grantor,
        ));
        self.grant(Capability::new(
            CapabilityKind::LoadAssets { allowed_paths: None },
            namespace,
            grantor,
        ));
    }

    /// Revoke a capability by ID
    pub fn revoke(&mut self, cap_id: CapabilityId) -> bool {
        for (ns, caps) in &mut self.capabilities {
            if let Some(idx) = caps.iter().position(|c| c.id == cap_id) {
                let cap = caps.remove(idx);
                log::debug!(
                    "Revoked capability {} from namespace {}",
                    cap.kind.name(),
                    ns
                );

                // Update index
                let kind_name = cap.kind.name();
                let has_other = caps.iter().any(|c| c.kind.name() == kind_name);
                if !has_other {
                    if let Some(index) = self.capability_index.get_mut(ns) {
                        index.remove(kind_name);
                    }
                }

                return true;
            }
        }
        false
    }

    /// Revoke all capabilities for a namespace
    pub fn revoke_all(&mut self, namespace: NamespaceId) {
        self.capabilities.remove(&namespace);
        self.capability_index.remove(&namespace);
        log::debug!("Revoked all capabilities for namespace {}", namespace);
    }

    /// Check if a namespace has a capability
    pub fn check(&self, namespace: NamespaceId, required: &CapabilityKind) -> CapabilityCheck {
        // Kernel has all capabilities
        if namespace.is_kernel() || namespace == self.kernel_namespace {
            return CapabilityCheck::Allowed;
        }

        // Find matching capability
        let caps = match self.capabilities.get(&namespace) {
            Some(caps) => caps,
            None => {
                return CapabilityCheck::Denied {
                    reason: format!("No capabilities for namespace {}", namespace),
                }
            }
        };

        // Find matching capability
        for cap in caps {
            if self.capability_matches(&cap.kind, required) {
                if cap.is_expired() {
                    return CapabilityCheck::Expired;
                }

                // Check quotas if applicable
                if let Some(check) = self.check_quota(namespace, &cap.kind) {
                    return check;
                }

                return CapabilityCheck::Allowed;
            }
        }

        CapabilityCheck::Denied {
            reason: format!(
                "Namespace {} lacks capability {}",
                namespace,
                required.name()
            ),
        }
    }

    /// Check if a capability kind matches the required kind
    fn capability_matches(&self, held: &CapabilityKind, required: &CapabilityKind) -> bool {
        // KernelAdmin matches everything
        if matches!(held, CapabilityKind::KernelAdmin) {
            return true;
        }

        match (held, required) {
            // Same kind
            (CapabilityKind::CreateEntities { .. }, CapabilityKind::CreateEntities { .. }) => true,
            (CapabilityKind::DestroyEntities, CapabilityKind::DestroyEntities) => true,
            (CapabilityKind::ModifyComponents { allowed_types: held_types },
             CapabilityKind::ModifyComponents { allowed_types: req_types }) => {
                // If held has no restrictions, allow all
                if held_types.is_none() {
                    return true;
                }
                // If required has restrictions, check they're subset
                match (held_types, req_types) {
                    (Some(held), Some(req)) => req.iter().all(|r| held.contains(r)),
                    (Some(_), None) => false, // Held is restricted, required is not
                    _ => true,
                }
            }
            (CapabilityKind::CreateLayers { .. }, CapabilityKind::CreateLayers { .. }) => true,
            (CapabilityKind::ModifyLayers { .. }, CapabilityKind::ModifyLayers { .. }) => true,
            (CapabilityKind::LoadAssets { .. }, CapabilityKind::LoadAssets { .. }) => true,
            (CapabilityKind::AccessNetwork { .. }, CapabilityKind::AccessNetwork { .. }) => true,
            (CapabilityKind::AccessFilesystem { .. }, CapabilityKind::AccessFilesystem { .. }) => true,
            (CapabilityKind::CrossNamespaceRead { .. }, CapabilityKind::CrossNamespaceRead { .. }) => true,
            (CapabilityKind::ExecuteScripts, CapabilityKind::ExecuteScripts) => true,
            (CapabilityKind::HotSwap, CapabilityKind::HotSwap) => true,
            (CapabilityKind::ManageCapabilities, CapabilityKind::ManageCapabilities) => true,
            _ => false,
        }
    }

    /// Check quota limits
    fn check_quota(&self, namespace: NamespaceId, kind: &CapabilityKind) -> Option<CapabilityCheck> {
        let quotas = self.quotas.get(&namespace)?;

        match kind {
            CapabilityKind::CreateEntities { max: Some(max) } => {
                if quotas.entities >= *max {
                    return Some(CapabilityCheck::QuotaExceeded {
                        limit: *max,
                        current: quotas.entities,
                    });
                }
            }
            CapabilityKind::CreateLayers { max: Some(max) } => {
                if quotas.layers >= *max {
                    return Some(CapabilityCheck::QuotaExceeded {
                        limit: *max,
                        current: quotas.layers,
                    });
                }
            }
            _ => {}
        }

        None
    }

    /// Quick check using the index (for hot path)
    pub fn has_capability(&self, namespace: NamespaceId, kind_name: &str) -> bool {
        if namespace.is_kernel() || namespace == self.kernel_namespace {
            return true;
        }

        self.capability_index
            .get(&namespace)
            .map(|kinds| kinds.contains(kind_name))
            .unwrap_or(false)
    }

    /// Update quota (called after operations)
    pub fn update_quota(&mut self, namespace: NamespaceId, f: impl FnOnce(&mut NamespaceQuotas)) {
        f(self.quotas.entry(namespace).or_default());
    }

    /// Reset frame-based quotas
    pub fn reset_frame_quotas(&mut self) {
        for quota in self.quotas.values_mut() {
            quota.patches_this_frame = 0;
        }
    }

    /// Get quotas for a namespace
    pub fn get_quotas(&self, namespace: NamespaceId) -> Option<&NamespaceQuotas> {
        self.quotas.get(&namespace)
    }

    /// Add audit entry
    pub fn audit(
        &mut self,
        namespace: NamespaceId,
        operation: &str,
        capability_id: Option<CapabilityId>,
        result: CapabilityCheck,
    ) {
        let entry = AuditEntry {
            timestamp: Instant::now(),
            namespace,
            operation: operation.to_string(),
            capability_id,
            result,
        };

        self.audit_log.push(entry);

        // Trim old entries
        if self.audit_log.len() > self.max_audit_entries {
            self.audit_log.remove(0);
        }
    }

    /// Get recent audit entries
    pub fn recent_audit(&self, count: usize) -> &[AuditEntry] {
        let start = self.audit_log.len().saturating_sub(count);
        &self.audit_log[start..]
    }

    /// Get all capabilities for a namespace
    pub fn get_capabilities(&self, namespace: NamespaceId) -> Vec<&Capability> {
        self.capabilities
            .get(&namespace)
            .map(|caps| caps.iter().collect())
            .unwrap_or_default()
    }

    /// Garbage collect expired capabilities
    pub fn gc_expired(&mut self) {
        for caps in self.capabilities.values_mut() {
            caps.retain(|c| !c.is_expired());
        }

        // Rebuild index
        self.capability_index.clear();
        for (ns, caps) in &self.capabilities {
            for cap in caps {
                self.capability_index
                    .entry(*ns)
                    .or_default()
                    .insert(cap.kind.name().to_string());
            }
        }
    }
}

impl Default for CapabilityChecker {
    fn default() -> Self {
        Self::new(NamespaceId::KERNEL)
    }
}

/// Get the required capability for a patch kind
pub fn required_capability_for_patch(
    patch_kind: &void_ir::patch::PatchKind,
) -> CapabilityKind {
    use void_ir::patch::{PatchKind, EntityOp, ComponentOp, LayerOp, AssetOp, HierarchyOp};

    match patch_kind {
        PatchKind::Entity(ep) => match &ep.op {
            EntityOp::Create { .. } => CapabilityKind::CreateEntities { max: None },
            EntityOp::Destroy => CapabilityKind::DestroyEntities,
            EntityOp::Enable | EntityOp::Disable | EntityOp::SetParent { .. }
            | EntityOp::AddTag { .. } | EntityOp::RemoveTag { .. } => {
                CapabilityKind::ModifyComponents { allowed_types: None }
            }
        },
        PatchKind::Component(cp) => match &cp.op {
            ComponentOp::Set { .. } | ComponentOp::Update { .. } | ComponentOp::Remove => {
                CapabilityKind::ModifyComponents {
                    allowed_types: Some(vec![cp.component.clone()]),
                }
            }
        },
        PatchKind::Layer(lp) => match &lp.op {
            LayerOp::Create { .. } => CapabilityKind::CreateLayers { max: None },
            LayerOp::Update { .. } | LayerOp::Destroy => {
                CapabilityKind::ModifyLayers { allowed_layers: None }
            }
        },
        PatchKind::Asset(ap) => match &ap.op {
            AssetOp::Load { path, .. } => CapabilityKind::LoadAssets {
                allowed_paths: Some(vec![path.clone()]),
            },
            AssetOp::Unload | AssetOp::Update { .. } => CapabilityKind::LoadAssets {
                allowed_paths: None,
            },
        },
        PatchKind::Hierarchy(hp) => match &hp.op {
            // Hierarchy operations that destroy entities need destroy capability
            HierarchyOp::DespawnRecursive => CapabilityKind::DestroyEntities,
            // All other hierarchy operations modify components (Parent, Children, etc.)
            _ => CapabilityKind::ModifyComponents { allowed_types: None },
        },
        PatchKind::Camera(_) => {
            // Camera operations modify the Camera component
            CapabilityKind::ModifyComponents {
                allowed_types: Some(vec!["Camera".to_string()]),
            }
        }
    }
}

/// Enforce a capability check for a patch, returning an error if denied
pub fn enforce_patch_capability(
    checker: &CapabilityChecker,
    namespace: NamespaceId,
    patch_kind: &void_ir::patch::PatchKind,
) -> Result<(), CapabilityError> {
    let required = required_capability_for_patch(patch_kind);
    let check = checker.check(namespace, &required);

    match check {
        CapabilityCheck::Allowed => Ok(()),
        CapabilityCheck::Denied { reason } => Err(CapabilityError::PermissionDenied {
            namespace,
            required: required.name().to_string(),
            reason,
        }),
        CapabilityCheck::QuotaExceeded { limit, current } => Err(CapabilityError::QuotaExceeded {
            namespace,
            resource: required.name().to_string(),
            limit,
            current,
        }),
        CapabilityCheck::Expired => Err(CapabilityError::CapabilityExpired {
            namespace,
            capability: required.name().to_string(),
        }),
    }
}

/// Errors from capability operations
#[derive(Debug, Clone)]
pub enum CapabilityError {
    /// Permission denied
    PermissionDenied {
        namespace: NamespaceId,
        required: String,
        reason: String,
    },
    /// Quota exceeded
    QuotaExceeded {
        namespace: NamespaceId,
        resource: String,
        limit: u32,
        current: u32,
    },
    /// Capability expired
    CapabilityExpired {
        namespace: NamespaceId,
        capability: String,
    },
}

impl std::fmt::Display for CapabilityError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::PermissionDenied {
                namespace,
                required,
                reason,
            } => write!(
                f,
                "Permission denied: namespace {} lacks {} ({})",
                namespace, required, reason
            ),
            Self::QuotaExceeded {
                namespace,
                resource,
                limit,
                current,
            } => write!(
                f,
                "Quota exceeded: namespace {} has {}/{} {}",
                namespace, current, limit, resource
            ),
            Self::CapabilityExpired {
                namespace,
                capability,
            } => write!(
                f,
                "Capability expired: namespace {} had {} but it expired",
                namespace, capability
            ),
        }
    }
}

impl std::error::Error for CapabilityError {}

/// Helper macro for capability checks
#[macro_export]
macro_rules! require_capability {
    ($checker:expr, $namespace:expr, $kind:expr) => {
        match $checker.check($namespace, &$kind) {
            CapabilityCheck::Allowed => {}
            check => {
                return Err(format!(
                    "Capability check failed for {}: {:?}",
                    $kind.name(),
                    check
                ))
            }
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_capability_creation() {
        let holder = NamespaceId::new();
        let grantor = NamespaceId::KERNEL;

        let cap = Capability::new(
            CapabilityKind::CreateEntities { max: Some(100) },
            holder,
            grantor,
        );

        assert!(cap.is_valid());
        assert!(!cap.is_expired());
        assert!(!cap.delegable);
    }

    #[test]
    fn test_capability_grant_and_check() {
        let kernel_ns = NamespaceId::KERNEL;
        let mut checker = CapabilityChecker::new(kernel_ns);

        let app_ns = NamespaceId::new();

        // Initially no capabilities
        let check = checker.check(app_ns, &CapabilityKind::CreateEntities { max: None });
        assert!(!check.is_allowed());

        // Grant capability
        checker.grant(Capability::new(
            CapabilityKind::CreateEntities { max: Some(100) },
            app_ns,
            kernel_ns,
        ));

        // Now should be allowed
        let check = checker.check(app_ns, &CapabilityKind::CreateEntities { max: None });
        assert!(check.is_allowed());
    }

    #[test]
    fn test_kernel_has_all_capabilities() {
        let kernel_ns = NamespaceId::KERNEL;
        let checker = CapabilityChecker::new(kernel_ns);

        // Kernel should have all capabilities
        assert!(checker
            .check(kernel_ns, &CapabilityKind::CreateEntities { max: None })
            .is_allowed());
        assert!(checker
            .check(kernel_ns, &CapabilityKind::HotSwap)
            .is_allowed());
        assert!(checker
            .check(kernel_ns, &CapabilityKind::KernelAdmin)
            .is_allowed());
    }

    #[test]
    fn test_capability_revocation() {
        let kernel_ns = NamespaceId::KERNEL;
        let mut checker = CapabilityChecker::new(kernel_ns);

        let app_ns = NamespaceId::new();

        // Grant
        let cap = Capability::new(
            CapabilityKind::CreateEntities { max: None },
            app_ns,
            kernel_ns,
        );
        let cap_id = cap.id;
        checker.grant(cap);

        assert!(checker
            .check(app_ns, &CapabilityKind::CreateEntities { max: None })
            .is_allowed());

        // Revoke
        assert!(checker.revoke(cap_id));

        assert!(!checker
            .check(app_ns, &CapabilityKind::CreateEntities { max: None })
            .is_allowed());
    }

    #[test]
    fn test_default_app_capabilities() {
        let kernel_ns = NamespaceId::KERNEL;
        let mut checker = CapabilityChecker::new(kernel_ns);

        let app_ns = NamespaceId::new();
        checker.grant_default_app_capabilities(app_ns, kernel_ns);

        // Should have basic capabilities
        assert!(checker
            .check(app_ns, &CapabilityKind::CreateEntities { max: None })
            .is_allowed());
        assert!(checker
            .check(app_ns, &CapabilityKind::DestroyEntities)
            .is_allowed());
        assert!(checker
            .check(app_ns, &CapabilityKind::CreateLayers { max: None })
            .is_allowed());

        // Should NOT have admin capabilities
        assert!(!checker
            .check(app_ns, &CapabilityKind::HotSwap)
            .is_allowed());
        assert!(!checker
            .check(app_ns, &CapabilityKind::KernelAdmin)
            .is_allowed());
    }

    #[test]
    fn test_quota_enforcement() {
        let kernel_ns = NamespaceId::KERNEL;
        let mut checker = CapabilityChecker::new(kernel_ns);

        let app_ns = NamespaceId::new();

        // Grant with limit of 2 entities
        checker.grant(Capability::new(
            CapabilityKind::CreateEntities { max: Some(2) },
            app_ns,
            kernel_ns,
        ));

        // Set current quota to 2
        checker.update_quota(app_ns, |q| q.entities = 2);

        // Should fail quota check
        let check = checker.check(app_ns, &CapabilityKind::CreateEntities { max: Some(2) });
        assert!(matches!(check, CapabilityCheck::QuotaExceeded { .. }));
    }

    #[test]
    fn test_quick_check() {
        let kernel_ns = NamespaceId::KERNEL;
        let mut checker = CapabilityChecker::new(kernel_ns);

        let app_ns = NamespaceId::new();

        assert!(!checker.has_capability(app_ns, "create_entities"));

        checker.grant(Capability::new(
            CapabilityKind::CreateEntities { max: None },
            app_ns,
            kernel_ns,
        ));

        assert!(checker.has_capability(app_ns, "create_entities"));
    }
}
