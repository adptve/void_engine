//! App namespaces for isolation
//!
//! Each app operates within its own namespace, providing:
//! - Entity isolation (app can only modify its own entities)
//! - Component isolation (app's components don't conflict)
//! - Layer ownership (app's layers are scoped)

use serde::{Deserialize, Serialize};
use std::fmt;
use std::sync::atomic::{AtomicU64, Ordering};

/// Unique identifier for a namespace
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct NamespaceId(u64);

impl NamespaceId {
    /// The kernel namespace (ID 0) - has access to everything
    pub const KERNEL: Self = Self(0);

    /// Create a new unique namespace ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID value
    pub fn raw(&self) -> u64 {
        self.0
    }

    /// Alias for raw() - get as u64
    pub fn as_u64(&self) -> u64 {
        self.0
    }

    /// Create from a raw u64 value
    ///
    /// Note: This should only be used when reconstructing a NamespaceId
    /// from serialized data, not for creating new namespaces.
    pub fn from_raw(id: u64) -> Self {
        Self(id)
    }

    /// Check if this is the kernel namespace
    pub fn is_kernel(&self) -> bool {
        self.0 == 0
    }
}

impl Default for NamespaceId {
    fn default() -> Self {
        Self::new()
    }
}

impl fmt::Display for NamespaceId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.is_kernel() {
            write!(f, "kernel")
        } else {
            write!(f, "ns:{}", self.0)
        }
    }
}

/// A namespace defines the scope of an app's operations
#[derive(Debug, Clone)]
pub struct Namespace {
    /// Unique identifier
    pub id: NamespaceId,
    /// Human-readable name
    pub name: String,
    /// Permissions for this namespace
    pub permissions: NamespacePermissions,
    /// Resource limits
    pub limits: ResourceLimits,
}

impl Namespace {
    /// Create a new namespace with default permissions
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            id: NamespaceId::new(),
            name: name.into(),
            permissions: NamespacePermissions::default(),
            limits: ResourceLimits::default(),
        }
    }

    /// Create the kernel namespace with full permissions
    pub fn kernel() -> Self {
        Self {
            id: NamespaceId::KERNEL,
            name: "kernel".to_string(),
            permissions: NamespacePermissions::all(),
            limits: ResourceLimits::unlimited(),
        }
    }

    /// Check if this namespace can access another namespace's entities
    pub fn can_access(&self, other: NamespaceId) -> bool {
        self.id.is_kernel() || self.id == other || self.permissions.cross_namespace_read
    }

    /// Check if this namespace can modify another namespace's entities
    pub fn can_modify(&self, other: NamespaceId) -> bool {
        self.id.is_kernel() || self.id == other
    }
}

/// Permissions for what a namespace can do
#[derive(Debug, Clone)]
pub struct NamespacePermissions {
    /// Can create entities
    pub create_entities: bool,
    /// Can destroy entities (own namespace only unless kernel)
    pub destroy_entities: bool,
    /// Can create/modify components
    pub modify_components: bool,
    /// Can read entities from other namespaces
    pub cross_namespace_read: bool,
    /// Can request layers
    pub create_layers: bool,
    /// Can load assets
    pub load_assets: bool,
    /// Can register new component types
    pub register_components: bool,
    /// Can execute scripts
    pub execute_scripts: bool,
}

impl Default for NamespacePermissions {
    fn default() -> Self {
        Self {
            create_entities: true,
            destroy_entities: true,
            modify_components: true,
            cross_namespace_read: true,
            create_layers: true,
            load_assets: true,
            register_components: false, // Apps shouldn't register new component types by default
            execute_scripts: true,
        }
    }
}

impl NamespacePermissions {
    /// All permissions enabled
    pub fn all() -> Self {
        Self {
            create_entities: true,
            destroy_entities: true,
            modify_components: true,
            cross_namespace_read: true,
            create_layers: true,
            load_assets: true,
            register_components: true,
            execute_scripts: true,
        }
    }

    /// Minimal permissions (read-only)
    pub fn read_only() -> Self {
        Self {
            create_entities: false,
            destroy_entities: false,
            modify_components: false,
            cross_namespace_read: true,
            create_layers: false,
            load_assets: false,
            register_components: false,
            execute_scripts: false,
        }
    }
}

/// Resource limits for a namespace
#[derive(Debug, Clone)]
pub struct ResourceLimits {
    /// Maximum number of entities
    pub max_entities: Option<u32>,
    /// Maximum number of components per entity
    pub max_components_per_entity: Option<u32>,
    /// Maximum number of layers
    pub max_layers: Option<u32>,
    /// Maximum memory usage in bytes
    pub max_memory_bytes: Option<u64>,
    /// Maximum patches per frame
    pub max_patches_per_frame: Option<u32>,
}

impl Default for ResourceLimits {
    fn default() -> Self {
        Self {
            max_entities: Some(10_000),
            max_components_per_entity: Some(64),
            max_layers: Some(8),
            max_memory_bytes: Some(256 * 1024 * 1024), // 256 MB
            max_patches_per_frame: Some(1000),
        }
    }
}

impl ResourceLimits {
    /// No limits (for kernel)
    pub fn unlimited() -> Self {
        Self {
            max_entities: None,
            max_components_per_entity: None,
            max_layers: None,
            max_memory_bytes: None,
            max_patches_per_frame: None,
        }
    }

    /// Check if entity limit would be exceeded
    pub fn check_entity_limit(&self, current: u32) -> bool {
        self.max_entities.map_or(true, |max| current < max)
    }

    /// Check if patch limit would be exceeded
    pub fn check_patch_limit(&self, current: u32) -> bool {
        self.max_patches_per_frame.map_or(true, |max| current < max)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_namespace_id() {
        let id1 = NamespaceId::new();
        let id2 = NamespaceId::new();
        assert_ne!(id1, id2);
        assert!(!id1.is_kernel());
        assert!(NamespaceId::KERNEL.is_kernel());
    }

    #[test]
    fn test_namespace_access() {
        let kernel = Namespace::kernel();
        let app1 = Namespace::new("app1");
        let app2 = Namespace::new("app2");

        // Kernel can access everything
        assert!(kernel.can_access(app1.id));
        assert!(kernel.can_modify(app1.id));

        // Apps can access each other (read)
        assert!(app1.can_access(app2.id));

        // Apps cannot modify each other
        assert!(!app1.can_modify(app2.id));

        // Apps can modify themselves
        assert!(app1.can_modify(app1.id));
    }
}
