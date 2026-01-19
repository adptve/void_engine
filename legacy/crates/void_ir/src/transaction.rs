//! Transactions - atomic batches of patches
//!
//! Transactions group patches that should succeed or fail together.
//! The kernel applies transactions atomically at frame boundaries.

use crate::namespace::NamespaceId;
use crate::patch::Patch;
use serde::{Deserialize, Serialize};
use std::sync::atomic::{AtomicU64, Ordering};

/// Unique identifier for a transaction
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct TransactionId(u64);

impl TransactionId {
    /// Create a new unique transaction ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID value
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for TransactionId {
    fn default() -> Self {
        Self::new()
    }
}

/// The state of a transaction
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum TransactionState {
    /// Transaction is being built
    Building,
    /// Transaction is queued for application
    Pending,
    /// Transaction is being applied
    Applying,
    /// Transaction was successfully applied
    Committed,
    /// Transaction failed and was rolled back
    RolledBack,
    /// Transaction was cancelled before application
    Cancelled,
}

/// A batch of patches that are applied atomically
#[derive(Debug, Clone)]
pub struct Transaction {
    /// Unique identifier
    pub id: TransactionId,
    /// The namespace that created this transaction
    pub source: NamespaceId,
    /// Current state
    pub state: TransactionState,
    /// The patches in this transaction
    pub patches: Vec<Patch>,
    /// Frame number when transaction was created
    pub created_frame: u64,
    /// Frame number when transaction was applied (if committed)
    pub applied_frame: Option<u64>,
    /// Dependencies on other transactions (must be applied first)
    pub dependencies: Vec<TransactionId>,
    /// Human-readable description for debugging
    pub description: Option<String>,
}

impl Transaction {
    /// Create a new empty transaction
    pub fn new(source: NamespaceId) -> Self {
        Self {
            id: TransactionId::new(),
            source,
            state: TransactionState::Building,
            patches: Vec::new(),
            created_frame: 0,
            applied_frame: None,
            dependencies: Vec::new(),
            description: None,
        }
    }

    /// Create a transaction with a description
    pub fn with_description(source: NamespaceId, description: impl Into<String>) -> Self {
        Self {
            description: Some(description.into()),
            ..Self::new(source)
        }
    }

    /// Add a patch to the transaction
    pub fn add_patch(&mut self, patch: Patch) {
        debug_assert_eq!(self.state, TransactionState::Building);
        self.patches.push(patch);
    }

    /// Add a patch (builder pattern)
    pub fn with_patch(mut self, patch: Patch) -> Self {
        self.add_patch(patch);
        self
    }

    /// Add multiple patches
    pub fn with_patches(mut self, patches: impl IntoIterator<Item = Patch>) -> Self {
        for patch in patches {
            self.add_patch(patch);
        }
        self
    }

    /// Add a dependency on another transaction
    pub fn depends_on(&mut self, other: TransactionId) {
        if !self.dependencies.contains(&other) {
            self.dependencies.push(other);
        }
    }

    /// Mark the transaction as ready for application
    pub fn submit(&mut self) {
        debug_assert_eq!(self.state, TransactionState::Building);
        self.state = TransactionState::Pending;
    }

    /// Check if the transaction is empty
    pub fn is_empty(&self) -> bool {
        self.patches.is_empty()
    }

    /// Get the number of patches
    pub fn len(&self) -> usize {
        self.patches.len()
    }

    /// Sort patches by priority (highest first)
    pub fn sort_by_priority(&mut self) {
        self.patches.sort_by(|a, b| b.priority.cmp(&a.priority));
    }

    /// Check if all dependencies are satisfied
    pub fn dependencies_satisfied(&self, committed: &[TransactionId]) -> bool {
        self.dependencies.iter().all(|dep| committed.contains(dep))
    }
}

/// Builder for transactions with a fluent API
pub struct TransactionBuilder {
    transaction: Transaction,
}

impl TransactionBuilder {
    /// Start building a new transaction
    pub fn new(source: NamespaceId) -> Self {
        Self {
            transaction: Transaction::new(source),
        }
    }

    /// Set the description
    pub fn description(mut self, desc: impl Into<String>) -> Self {
        self.transaction.description = Some(desc.into());
        self
    }

    /// Add a patch
    pub fn patch(mut self, patch: Patch) -> Self {
        self.transaction.add_patch(patch);
        self
    }

    /// Add multiple patches
    pub fn patches(mut self, patches: impl IntoIterator<Item = Patch>) -> Self {
        for patch in patches {
            self.transaction.add_patch(patch);
        }
        self
    }

    /// Add a dependency
    pub fn depends_on(mut self, other: TransactionId) -> Self {
        self.transaction.depends_on(other);
        self
    }

    /// Set the created frame
    pub fn frame(mut self, frame: u64) -> Self {
        self.transaction.created_frame = frame;
        self
    }

    /// Build and submit the transaction
    pub fn build(mut self) -> Transaction {
        self.transaction.submit();
        self.transaction
    }

    /// Build without submitting (stays in Building state)
    pub fn build_draft(self) -> Transaction {
        self.transaction
    }
}

/// Result of applying a transaction
#[derive(Debug, Clone)]
pub struct TransactionResult {
    /// The transaction that was applied
    pub id: TransactionId,
    /// Whether it succeeded
    pub success: bool,
    /// Error message if failed
    pub error: Option<String>,
    /// Number of patches successfully applied
    pub patches_applied: usize,
    /// Undo data for rollback (if supported)
    pub undo_patches: Vec<Patch>,
}

impl TransactionResult {
    /// Create a successful result
    pub fn success(id: TransactionId, patches_applied: usize) -> Self {
        Self {
            id,
            success: true,
            error: None,
            patches_applied,
            undo_patches: Vec::new(),
        }
    }

    /// Create a failed result
    pub fn failure(id: TransactionId, error: impl Into<String>, patches_applied: usize) -> Self {
        Self {
            id,
            success: false,
            error: Some(error.into()),
            patches_applied,
            undo_patches: Vec::new(),
        }
    }

    /// Add undo patches for rollback
    pub fn with_undo(mut self, undo_patches: Vec<Patch>) -> Self {
        self.undo_patches = undo_patches;
        self
    }
}

/// Conflict detection for transactions
#[derive(Debug, Clone)]
pub struct ConflictDetector {
    /// Entities being modified
    entities: std::collections::HashSet<crate::patch::EntityRef>,
    /// Components being modified (entity, component)
    components: std::collections::HashSet<(crate::patch::EntityRef, String)>,
    /// Layers being modified
    layers: std::collections::HashSet<String>,
    /// Assets being modified
    assets: std::collections::HashSet<String>,
}

impl ConflictDetector {
    /// Create a new conflict detector
    pub fn new() -> Self {
        Self {
            entities: std::collections::HashSet::new(),
            components: std::collections::HashSet::new(),
            layers: std::collections::HashSet::new(),
            assets: std::collections::HashSet::new(),
        }
    }

    /// Add a transaction to the conflict detector
    pub fn add_transaction(&mut self, transaction: &Transaction) {
        for patch in &transaction.patches {
            self.add_patch(patch);
        }
    }

    /// Add a patch to the conflict detector
    pub fn add_patch(&mut self, patch: &Patch) {
        use crate::patch::PatchKind;

        match &patch.kind {
            PatchKind::Entity(ep) => {
                self.entities.insert(ep.entity);
            }
            PatchKind::Component(cp) => {
                self.components.insert((cp.entity, cp.component.clone()));
            }
            PatchKind::Layer(lp) => {
                self.layers.insert(lp.layer_id.clone());
            }
            PatchKind::Asset(ap) => {
                self.assets.insert(ap.asset_id.clone());
            }
            PatchKind::Hierarchy(hp) => {
                // Hierarchy patches affect entities
                self.entities.insert(hp.entity);
            }
            PatchKind::Camera(cp) => {
                // Camera patches affect entities
                self.entities.insert(cp.entity);
            }
        }
    }

    /// Check if a transaction conflicts with existing modifications
    pub fn has_conflict(&self, transaction: &Transaction) -> bool {
        transaction.patches.iter().any(|patch| self.has_patch_conflict(patch))
    }

    /// Check if a patch conflicts
    fn has_patch_conflict(&self, patch: &Patch) -> bool {
        use crate::patch::PatchKind;

        match &patch.kind {
            PatchKind::Entity(ep) => self.entities.contains(&ep.entity),
            PatchKind::Component(cp) => self.components.contains(&(cp.entity, cp.component.clone())),
            PatchKind::Layer(lp) => self.layers.contains(&lp.layer_id),
            PatchKind::Asset(ap) => self.assets.contains(&ap.asset_id),
            PatchKind::Hierarchy(hp) => self.entities.contains(&hp.entity),
            PatchKind::Camera(cp) => self.entities.contains(&cp.entity),
        }
    }

    /// Get all conflicts for a transaction
    pub fn get_conflicts(&self, transaction: &Transaction) -> Vec<Conflict> {
        let mut conflicts = Vec::new();

        for patch in &transaction.patches {
            if let Some(conflict) = self.get_patch_conflict(patch) {
                conflicts.push(conflict);
            }
        }

        conflicts
    }

    /// Get conflict for a single patch
    fn get_patch_conflict(&self, patch: &Patch) -> Option<Conflict> {
        use crate::patch::PatchKind;

        match &patch.kind {
            PatchKind::Entity(ep) => {
                if self.entities.contains(&ep.entity) {
                    Some(Conflict::Entity(ep.entity))
                } else {
                    None
                }
            }
            PatchKind::Component(cp) => {
                if self.components.contains(&(cp.entity, cp.component.clone())) {
                    Some(Conflict::Component {
                        entity: cp.entity,
                        component: cp.component.clone(),
                    })
                } else {
                    None
                }
            }
            PatchKind::Layer(lp) => {
                if self.layers.contains(&lp.layer_id) {
                    Some(Conflict::Layer(lp.layer_id.clone()))
                } else {
                    None
                }
            }
            PatchKind::Asset(ap) => {
                if self.assets.contains(&ap.asset_id) {
                    Some(Conflict::Asset(ap.asset_id.clone()))
                } else {
                    None
                }
            }
            PatchKind::Hierarchy(hp) => {
                if self.entities.contains(&hp.entity) {
                    Some(Conflict::Entity(hp.entity))
                } else {
                    None
                }
            }
            PatchKind::Camera(cp) => {
                if self.entities.contains(&cp.entity) {
                    Some(Conflict::Entity(cp.entity))
                } else {
                    None
                }
            }
        }
    }

    /// Clear all tracked modifications
    pub fn clear(&mut self) {
        self.entities.clear();
        self.components.clear();
        self.layers.clear();
        self.assets.clear();
    }
}

impl Default for ConflictDetector {
    fn default() -> Self {
        Self::new()
    }
}

/// A conflict between transactions
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Conflict {
    /// Conflict on an entity
    Entity(crate::patch::EntityRef),
    /// Conflict on a component
    Component {
        entity: crate::patch::EntityRef,
        component: String,
    },
    /// Conflict on a layer
    Layer(String),
    /// Conflict on an asset
    Asset(String),
}

impl std::fmt::Display for Conflict {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Entity(entity) => write!(f, "Entity conflict: {:?}", entity),
            Self::Component { entity, component } => {
                write!(f, "Component conflict: {:?}.{}", entity, component)
            }
            Self::Layer(layer_id) => write!(f, "Layer conflict: {}", layer_id),
            Self::Asset(asset_id) => write!(f, "Asset conflict: {}", asset_id),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::patch::{EntityPatch, PatchKind};

    #[test]
    fn test_transaction_builder() {
        let ns = NamespaceId::new();
        let tx = TransactionBuilder::new(ns)
            .description("Spawn player")
            .patch(Patch::new(
                ns,
                PatchKind::Entity(EntityPatch::create(ns, 1)),
            ))
            .build();

        assert_eq!(tx.state, TransactionState::Pending);
        assert_eq!(tx.len(), 1);
        assert_eq!(tx.description.as_deref(), Some("Spawn player"));
    }

    #[test]
    fn test_transaction_dependencies() {
        let ns = NamespaceId::new();
        let tx1 = TransactionBuilder::new(ns).build();
        let tx2 = TransactionBuilder::new(ns).depends_on(tx1.id).build();

        assert!(tx2.dependencies_satisfied(&[tx1.id]));
        assert!(!tx2.dependencies_satisfied(&[]));
    }

    #[test]
    fn test_conflict_detection() {
        use crate::patch::{ComponentPatch, EntityRef};

        let ns = NamespaceId::new();
        let entity = EntityRef::new(ns, 1);

        let mut detector = ConflictDetector::new();

        let tx1 = TransactionBuilder::new(ns)
            .patch(Patch::new(
                ns,
                PatchKind::Component(ComponentPatch::set(
                    entity,
                    "Transform",
                    crate::value::Value::from([0.0, 0.0, 0.0]),
                )),
            ))
            .build();

        detector.add_transaction(&tx1);

        // Same entity, same component - should conflict
        let tx2 = TransactionBuilder::new(ns)
            .patch(Patch::new(
                ns,
                PatchKind::Component(ComponentPatch::set(
                    entity,
                    "Transform",
                    crate::value::Value::from([1.0, 1.0, 1.0]),
                )),
            ))
            .build();

        assert!(detector.has_conflict(&tx2));

        // Same entity, different component - no conflict
        let tx3 = TransactionBuilder::new(ns)
            .patch(Patch::new(
                ns,
                PatchKind::Component(ComponentPatch::set(
                    entity,
                    "Velocity",
                    crate::value::Value::from([0.0, 0.0, 0.0]),
                )),
            ))
            .build();

        assert!(!detector.has_conflict(&tx3));
    }

    #[test]
    fn test_get_conflicts() {
        use crate::patch::{ComponentPatch, EntityRef};

        let ns = NamespaceId::new();
        let entity = EntityRef::new(ns, 1);

        let mut detector = ConflictDetector::new();

        let tx1 = TransactionBuilder::new(ns)
            .patch(Patch::new(
                ns,
                PatchKind::Component(ComponentPatch::set(
                    entity,
                    "Transform",
                    crate::value::Value::from([0.0, 0.0, 0.0]),
                )),
            ))
            .build();

        detector.add_transaction(&tx1);

        let tx2 = TransactionBuilder::new(ns)
            .patch(Patch::new(
                ns,
                PatchKind::Component(ComponentPatch::set(
                    entity,
                    "Transform",
                    crate::value::Value::from([1.0, 1.0, 1.0]),
                )),
            ))
            .build();

        let conflicts = detector.get_conflicts(&tx2);
        assert_eq!(conflicts.len(), 1);
        assert!(matches!(conflicts[0], Conflict::Component { .. }));
    }
}
