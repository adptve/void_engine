//! Namespace management for app isolation
//!
//! Namespaces provide logical separation between apps. Each app operates
//! in its own namespace and cannot directly access resources in other namespaces.
//!
//! # Security Properties
//!
//! 1. Apps cannot forge namespace IDs
//! 2. Apps can only modify entities in their own namespace
//! 3. Cross-namespace reads require explicit capability
//! 4. Resource limits are enforced per-namespace

use parking_lot::RwLock;
use std::collections::{HashMap, HashSet};
use std::sync::Arc;
use std::time::Instant;

use void_ir::NamespaceId;

use crate::app::AppId;
use crate::capability::{Capability, CapabilityChecker, CapabilityKind};
use crate::layer::LayerId;

/// Entity reference within a namespace
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct NamespacedEntity {
    /// The namespace this entity belongs to
    pub namespace: NamespaceId,
    /// Local entity ID within the namespace
    pub local_id: u64,
}

impl NamespacedEntity {
    /// Create a new namespaced entity reference
    pub fn new(namespace: NamespaceId, local_id: u64) -> Self {
        Self { namespace, local_id }
    }
}

/// Result of a namespace access check
#[derive(Debug, Clone)]
pub enum NamespaceAccess {
    /// Access allowed
    Allowed,
    /// Access denied - not same namespace
    DeniedNotOwner,
    /// Access denied - missing cross-namespace capability
    DeniedMissingCapability,
    /// Access denied - namespace not found
    DeniedNotFound,
}

impl NamespaceAccess {
    /// Check if access is allowed
    pub fn is_allowed(&self) -> bool {
        matches!(self, Self::Allowed)
    }
}

/// Tracking info for a namespace
#[derive(Debug)]
pub struct NamespaceInfo {
    /// The namespace ID
    pub id: NamespaceId,
    /// Associated app ID (if any)
    pub app_id: Option<AppId>,
    /// Human-readable name
    pub name: String,
    /// When this namespace was created
    pub created_at: Instant,
    /// Entities owned by this namespace
    pub entities: HashSet<u64>,
    /// Layers owned by this namespace
    pub layers: HashSet<LayerId>,
    /// Assets loaded by this namespace
    pub assets: HashSet<String>,
    /// Exported entities (available to other namespaces)
    pub exports: HashMap<u64, EntityExport>,
}

impl NamespaceInfo {
    /// Create a new namespace info
    pub fn new(id: NamespaceId, name: impl Into<String>) -> Self {
        Self {
            id,
            app_id: None,
            name: name.into(),
            created_at: Instant::now(),
            entities: HashSet::new(),
            layers: HashSet::new(),
            assets: HashSet::new(),
            exports: HashMap::new(),
        }
    }

    /// Set the associated app ID
    pub fn with_app(mut self, app_id: AppId) -> Self {
        self.app_id = Some(app_id);
        self
    }
}

/// An entity exported for cross-namespace access
#[derive(Debug, Clone)]
pub struct EntityExport {
    /// Entity local ID
    pub local_id: u64,
    /// Which components can be read
    pub readable_components: Vec<String>,
    /// Which components can be written (usually empty for safety)
    pub writable_components: Vec<String>,
    /// Access control
    pub access: ExportAccess,
}

/// Who can access an exported entity
#[derive(Debug, Clone)]
pub enum ExportAccess {
    /// Any namespace can access
    Public,
    /// Only specific namespaces
    Allowlist(Vec<NamespaceId>),
    /// Only namespaces with a specific capability
    CapabilityRequired(String),
}

/// Manages all namespaces in the kernel
pub struct NamespaceManager {
    /// All registered namespaces
    namespaces: HashMap<NamespaceId, NamespaceInfo>,
    /// App ID to namespace ID mapping
    app_to_namespace: HashMap<AppId, NamespaceId>,
    /// Reference to capability checker
    capability_checker: Arc<RwLock<CapabilityChecker>>,
    /// Kernel namespace (always exists, has full access)
    kernel_namespace: NamespaceId,
}

impl NamespaceManager {
    /// Create a new namespace manager
    pub fn new(capability_checker: Arc<RwLock<CapabilityChecker>>) -> Self {
        let kernel_namespace = NamespaceId::KERNEL;

        let mut namespaces = HashMap::new();
        namespaces.insert(
            kernel_namespace,
            NamespaceInfo::new(kernel_namespace, "kernel"),
        );

        Self {
            namespaces,
            app_to_namespace: HashMap::new(),
            capability_checker,
            kernel_namespace,
        }
    }

    /// Create a new namespace for an app
    pub fn create_namespace(
        &mut self,
        name: impl Into<String>,
        app_id: Option<AppId>,
    ) -> NamespaceId {
        let id = NamespaceId::new();
        let mut info = NamespaceInfo::new(id, name);

        if let Some(app_id) = app_id {
            info.app_id = Some(app_id);
            self.app_to_namespace.insert(app_id, id);
        }

        self.namespaces.insert(id, info);
        log::debug!("Created namespace {}", id);

        id
    }

    /// Destroy a namespace and cleanup all its resources
    pub fn destroy_namespace(&mut self, id: NamespaceId) -> Option<NamespaceInfo> {
        if id.is_kernel() {
            log::error!("Cannot destroy kernel namespace");
            return None;
        }

        if let Some(info) = self.namespaces.remove(&id) {
            // Remove from app mapping
            if let Some(app_id) = info.app_id {
                self.app_to_namespace.remove(&app_id);
            }

            // Revoke all capabilities for this namespace
            self.capability_checker.write().revoke_all(id);

            log::debug!("Destroyed namespace {} with {} entities", id, info.entities.len());
            Some(info)
        } else {
            None
        }
    }

    /// Get namespace info
    pub fn get(&self, id: NamespaceId) -> Option<&NamespaceInfo> {
        self.namespaces.get(&id)
    }

    /// Get mutable namespace info
    pub fn get_mut(&mut self, id: NamespaceId) -> Option<&mut NamespaceInfo> {
        self.namespaces.get_mut(&id)
    }

    /// Get namespace for an app
    pub fn get_for_app(&self, app_id: AppId) -> Option<NamespaceId> {
        self.app_to_namespace.get(&app_id).copied()
    }

    /// Check if a namespace can access an entity in another namespace
    pub fn check_access(
        &self,
        requester: NamespaceId,
        target_namespace: NamespaceId,
        target_entity: u64,
        write: bool,
    ) -> NamespaceAccess {
        // Kernel has full access
        if requester.is_kernel() {
            return NamespaceAccess::Allowed;
        }

        // Same namespace - always allowed
        if requester == target_namespace {
            return NamespaceAccess::Allowed;
        }

        // Cross-namespace access - need to check exports and capabilities
        let target_info = match self.namespaces.get(&target_namespace) {
            Some(info) => info,
            None => return NamespaceAccess::DeniedNotFound,
        };

        // Check if entity is exported
        if let Some(export) = target_info.exports.get(&target_entity) {
            // Check access control
            match &export.access {
                ExportAccess::Public => {
                    if write && export.writable_components.is_empty() {
                        return NamespaceAccess::DeniedMissingCapability;
                    }
                    return NamespaceAccess::Allowed;
                }
                ExportAccess::Allowlist(allowed) => {
                    if allowed.contains(&requester) {
                        if write && export.writable_components.is_empty() {
                            return NamespaceAccess::DeniedMissingCapability;
                        }
                        return NamespaceAccess::Allowed;
                    }
                }
                ExportAccess::CapabilityRequired(cap_name) => {
                    let checker = self.capability_checker.read();
                    if checker.has_capability(requester, cap_name) {
                        if write && export.writable_components.is_empty() {
                            return NamespaceAccess::DeniedMissingCapability;
                        }
                        return NamespaceAccess::Allowed;
                    }
                }
            }
        }

        // Check cross-namespace read capability
        if !write {
            let checker = self.capability_checker.read();
            let required = CapabilityKind::CrossNamespaceRead {
                allowed_namespaces: Some(vec![target_namespace]),
            };
            if checker.check(requester, &required).is_allowed() {
                return NamespaceAccess::Allowed;
            }
        }

        NamespaceAccess::DeniedMissingCapability
    }

    /// Register an entity as owned by a namespace
    pub fn register_entity(&mut self, namespace: NamespaceId, local_id: u64) -> bool {
        if let Some(info) = self.namespaces.get_mut(&namespace) {
            info.entities.insert(local_id);
            true
        } else {
            false
        }
    }

    /// Unregister an entity from a namespace
    pub fn unregister_entity(&mut self, namespace: NamespaceId, local_id: u64) -> bool {
        if let Some(info) = self.namespaces.get_mut(&namespace) {
            info.entities.remove(&local_id);
            info.exports.remove(&local_id);
            true
        } else {
            false
        }
    }

    /// Register a layer as owned by a namespace
    pub fn register_layer(&mut self, namespace: NamespaceId, layer_id: LayerId) -> bool {
        if let Some(info) = self.namespaces.get_mut(&namespace) {
            info.layers.insert(layer_id);
            true
        } else {
            false
        }
    }

    /// Unregister a layer from a namespace
    pub fn unregister_layer(&mut self, namespace: NamespaceId, layer_id: LayerId) -> bool {
        if let Some(info) = self.namespaces.get_mut(&namespace) {
            info.layers.remove(&layer_id)
        } else {
            false
        }
    }

    /// Export an entity for cross-namespace access
    pub fn export_entity(
        &mut self,
        namespace: NamespaceId,
        local_id: u64,
        export: EntityExport,
    ) -> bool {
        if let Some(info) = self.namespaces.get_mut(&namespace) {
            if info.entities.contains(&local_id) {
                info.exports.insert(local_id, export);
                log::debug!("Exported entity {} from namespace {}", local_id, namespace);
                return true;
            }
        }
        false
    }

    /// Unexport an entity
    pub fn unexport_entity(&mut self, namespace: NamespaceId, local_id: u64) -> bool {
        if let Some(info) = self.namespaces.get_mut(&namespace) {
            info.exports.remove(&local_id).is_some()
        } else {
            false
        }
    }

    /// Get all entities owned by a namespace
    pub fn get_entities(&self, namespace: NamespaceId) -> Option<&HashSet<u64>> {
        self.namespaces.get(&namespace).map(|i| &i.entities)
    }

    /// Get all layers owned by a namespace
    pub fn get_layers(&self, namespace: NamespaceId) -> Option<&HashSet<LayerId>> {
        self.namespaces.get(&namespace).map(|i| &i.layers)
    }

    /// Count entities in a namespace
    pub fn entity_count(&self, namespace: NamespaceId) -> usize {
        self.namespaces
            .get(&namespace)
            .map(|i| i.entities.len())
            .unwrap_or(0)
    }

    /// Count layers in a namespace
    pub fn layer_count(&self, namespace: NamespaceId) -> usize {
        self.namespaces
            .get(&namespace)
            .map(|i| i.layers.len())
            .unwrap_or(0)
    }

    /// Get kernel namespace ID
    pub fn kernel_namespace(&self) -> NamespaceId {
        self.kernel_namespace
    }

    /// Check if a namespace exists
    pub fn exists(&self, id: NamespaceId) -> bool {
        self.namespaces.contains_key(&id)
    }
}

/// Error when namespace operations fail
#[derive(Debug, Clone)]
pub enum NamespaceError {
    /// Namespace not found
    NotFound(NamespaceId),
    /// Access denied
    AccessDenied {
        requester: NamespaceId,
        target: NamespaceId,
        reason: String,
    },
    /// Entity not found
    EntityNotFound {
        namespace: NamespaceId,
        entity: u64,
    },
    /// Resource limit exceeded
    LimitExceeded {
        namespace: NamespaceId,
        resource: String,
        limit: u32,
        current: u32,
    },
}

impl std::fmt::Display for NamespaceError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::NotFound(id) => write!(f, "Namespace {} not found", id),
            Self::AccessDenied {
                requester,
                target,
                reason,
            } => write!(
                f,
                "Access denied: {} cannot access {} ({})",
                requester, target, reason
            ),
            Self::EntityNotFound { namespace, entity } => {
                write!(f, "Entity {} not found in namespace {}", entity, namespace)
            }
            Self::LimitExceeded {
                namespace,
                resource,
                limit,
                current,
            } => write!(
                f,
                "Namespace {} exceeded {} limit ({}/{})",
                namespace, resource, current, limit
            ),
        }
    }
}

impl std::error::Error for NamespaceError {}

#[cfg(test)]
mod tests {
    use super::*;

    fn create_test_checker() -> Arc<RwLock<CapabilityChecker>> {
        Arc::new(RwLock::new(CapabilityChecker::default()))
    }

    #[test]
    fn test_create_namespace() {
        let checker = create_test_checker();
        let mut manager = NamespaceManager::new(checker);

        let app_id = AppId::new();
        let ns_id = manager.create_namespace("test_app", Some(app_id));

        assert!(manager.exists(ns_id));
        assert_eq!(manager.get_for_app(app_id), Some(ns_id));
    }

    #[test]
    fn test_destroy_namespace() {
        let checker = create_test_checker();
        let mut manager = NamespaceManager::new(checker);

        let ns_id = manager.create_namespace("test_app", None);
        manager.register_entity(ns_id, 1);
        manager.register_entity(ns_id, 2);

        let info = manager.destroy_namespace(ns_id);
        assert!(info.is_some());
        assert_eq!(info.unwrap().entities.len(), 2);
        assert!(!manager.exists(ns_id));
    }

    #[test]
    fn test_cannot_destroy_kernel() {
        let checker = create_test_checker();
        let mut manager = NamespaceManager::new(checker);

        let result = manager.destroy_namespace(NamespaceId::KERNEL);
        assert!(result.is_none());
        assert!(manager.exists(NamespaceId::KERNEL));
    }

    #[test]
    fn test_same_namespace_access() {
        let checker = create_test_checker();
        let manager = NamespaceManager::new(checker);

        let ns = NamespaceId::new();
        let access = manager.check_access(ns, ns, 1, true);
        assert!(access.is_allowed());
    }

    #[test]
    fn test_cross_namespace_denied() {
        let checker = create_test_checker();
        let mut manager = NamespaceManager::new(checker);

        let ns1 = manager.create_namespace("app1", None);
        let ns2 = manager.create_namespace("app2", None);

        manager.register_entity(ns2, 1);

        // ns1 trying to access ns2's entity - should fail
        let access = manager.check_access(ns1, ns2, 1, false);
        assert!(!access.is_allowed());
    }

    #[test]
    fn test_kernel_access() {
        let checker = create_test_checker();
        let mut manager = NamespaceManager::new(checker);

        let app_ns = manager.create_namespace("app", None);
        manager.register_entity(app_ns, 1);

        // Kernel can access anything
        let access = manager.check_access(NamespaceId::KERNEL, app_ns, 1, true);
        assert!(access.is_allowed());
    }

    #[test]
    fn test_entity_export_public() {
        let checker = create_test_checker();
        let mut manager = NamespaceManager::new(checker);

        let ns1 = manager.create_namespace("app1", None);
        let ns2 = manager.create_namespace("app2", None);

        // ns2 has an entity and exports it
        manager.register_entity(ns2, 1);
        manager.export_entity(
            ns2,
            1,
            EntityExport {
                local_id: 1,
                readable_components: vec!["Transform".to_string()],
                writable_components: vec![],
                access: ExportAccess::Public,
            },
        );

        // ns1 can read exported entity
        let access = manager.check_access(ns1, ns2, 1, false);
        assert!(access.is_allowed());

        // ns1 cannot write (no writable components)
        let access = manager.check_access(ns1, ns2, 1, true);
        assert!(!access.is_allowed());
    }

    #[test]
    fn test_entity_export_allowlist() {
        let checker = create_test_checker();
        let mut manager = NamespaceManager::new(checker);

        let ns1 = manager.create_namespace("app1", None);
        let ns2 = manager.create_namespace("app2", None);
        let ns3 = manager.create_namespace("app3", None);

        // ns2 exports entity only to ns1
        manager.register_entity(ns2, 1);
        manager.export_entity(
            ns2,
            1,
            EntityExport {
                local_id: 1,
                readable_components: vec!["Transform".to_string()],
                writable_components: vec![],
                access: ExportAccess::Allowlist(vec![ns1]),
            },
        );

        // ns1 can access
        let access = manager.check_access(ns1, ns2, 1, false);
        assert!(access.is_allowed());

        // ns3 cannot access
        let access = manager.check_access(ns3, ns2, 1, false);
        assert!(!access.is_allowed());
    }
}
