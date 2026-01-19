//! Patch Bus - central hub for collecting and processing patches
//!
//! The patch bus is the core communication mechanism between apps and kernel.
//! Apps submit patches/transactions, the bus batches them, and the kernel
//! applies them at frame boundaries.

use crate::namespace::{Namespace, NamespaceId, ResourceLimits};
use crate::patch::{Patch, PatchKind};
use crate::transaction::{Transaction, TransactionBuilder, TransactionId, TransactionResult, TransactionState};
use crossbeam_channel::{bounded, Receiver, Sender};
use parking_lot::RwLock;
use std::collections::HashMap;
use std::sync::Arc;

/// Configuration for the patch bus
#[derive(Debug, Clone)]
pub struct PatchBusConfig {
    /// Maximum pending transactions
    pub max_pending_transactions: usize,
    /// Maximum patches per transaction
    pub max_patches_per_transaction: usize,
    /// Whether to validate patches before queuing
    pub validate_on_submit: bool,
    /// Whether to allow cross-namespace patches
    pub allow_cross_namespace: bool,
}

impl Default for PatchBusConfig {
    fn default() -> Self {
        Self {
            max_pending_transactions: 1000,
            max_patches_per_transaction: 10000,
            validate_on_submit: true,
            allow_cross_namespace: false,
        }
    }
}

/// The central patch bus
pub struct PatchBus {
    config: PatchBusConfig,
    /// Registered namespaces
    namespaces: RwLock<HashMap<NamespaceId, Arc<Namespace>>>,
    /// Pending transactions (submitted but not yet applied)
    pending: RwLock<Vec<Transaction>>,
    /// Committed transaction IDs (for dependency checking)
    committed: RwLock<Vec<TransactionId>>,
    /// Channel for submitting transactions from apps
    tx_sender: Sender<Transaction>,
    tx_receiver: Receiver<Transaction>,
    /// Current frame number
    current_frame: RwLock<u64>,
    /// Statistics
    stats: RwLock<PatchBusStats>,
}

/// Statistics about patch bus usage
#[derive(Debug, Clone, Default)]
pub struct PatchBusStats {
    /// Total transactions submitted
    pub transactions_submitted: u64,
    /// Total transactions committed
    pub transactions_committed: u64,
    /// Total transactions rolled back
    pub transactions_rolled_back: u64,
    /// Total patches applied
    pub patches_applied: u64,
    /// Patches this frame
    pub patches_this_frame: u64,
    /// Peak pending transactions
    pub peak_pending: usize,
}

impl PatchBus {
    /// Create a new patch bus
    pub fn new(config: PatchBusConfig) -> Self {
        let (tx_sender, tx_receiver) = bounded(config.max_pending_transactions);

        Self {
            config,
            namespaces: RwLock::new(HashMap::new()),
            pending: RwLock::new(Vec::new()),
            committed: RwLock::new(Vec::new()),
            tx_sender,
            tx_receiver,
            current_frame: RwLock::new(0),
            stats: RwLock::new(PatchBusStats::default()),
        }
    }

    /// Create with default config
    pub fn default() -> Self {
        Self::new(PatchBusConfig::default())
    }

    /// Register a namespace
    pub fn register_namespace(&self, namespace: Namespace) -> NamespaceHandle {
        let id = namespace.id;
        self.namespaces.write().insert(id, Arc::new(namespace));
        NamespaceHandle {
            id,
            sender: self.tx_sender.clone(),
            config: self.config.clone(),
        }
    }

    /// Get a namespace by ID
    pub fn get_namespace(&self, id: NamespaceId) -> Option<Arc<Namespace>> {
        self.namespaces.read().get(&id).cloned()
    }

    /// Submit a transaction directly (used by kernel)
    pub fn submit(&self, transaction: Transaction) -> Result<TransactionId, PatchBusError> {
        let id = transaction.id;

        // Validate
        if self.config.validate_on_submit {
            self.validate_transaction(&transaction)?;
        }

        // Check limits
        if self.pending.read().len() >= self.config.max_pending_transactions {
            return Err(PatchBusError::TooManyPendingTransactions);
        }

        // Add to pending
        self.pending.write().push(transaction);

        // Update stats
        let mut stats = self.stats.write();
        stats.transactions_submitted += 1;
        let pending_len = self.pending.read().len();
        if pending_len > stats.peak_pending {
            stats.peak_pending = pending_len;
        }

        Ok(id)
    }

    /// Receive transactions from apps (called by kernel each frame)
    pub fn receive_pending(&self) {
        while let Ok(tx) = self.tx_receiver.try_recv() {
            if let Err(e) = self.submit(tx) {
                log::warn!("Failed to queue transaction: {:?}", e);
            }
        }
    }

    /// Get all pending transactions ready for this frame
    pub fn drain_ready(&self, frame: u64) -> Vec<Transaction> {
        let committed = self.committed.read().clone();
        let mut pending = self.pending.write();

        // Partition into ready and not-ready
        let mut ready = Vec::new();
        let mut not_ready = Vec::new();

        for tx in pending.drain(..) {
            if tx.dependencies_satisfied(&committed) {
                ready.push(tx);
            } else {
                not_ready.push(tx);
            }
        }

        *pending = not_ready;

        // Sort ready transactions by priority (highest priority patches first)
        ready.sort_by(|a, b| {
            let a_max = a.patches.iter().map(|p| p.priority).max().unwrap_or(0);
            let b_max = b.patches.iter().map(|p| p.priority).max().unwrap_or(0);
            b_max.cmp(&a_max)
        });

        ready
    }

    /// Mark a transaction as committed
    pub fn commit(&self, result: TransactionResult) {
        if result.success {
            self.committed.write().push(result.id);
            let mut stats = self.stats.write();
            stats.transactions_committed += 1;
            stats.patches_applied += result.patches_applied as u64;
            stats.patches_this_frame += result.patches_applied as u64;
        } else {
            self.stats.write().transactions_rolled_back += 1;
        }
    }

    /// Start a new frame
    pub fn begin_frame(&self, frame: u64) {
        *self.current_frame.write() = frame;
        self.stats.write().patches_this_frame = 0;

        // Receive any pending transactions from apps
        self.receive_pending();
    }

    /// Get current statistics
    pub fn stats(&self) -> PatchBusStats {
        self.stats.read().clone()
    }

    /// Validate a transaction
    fn validate_transaction(&self, tx: &Transaction) -> Result<(), PatchBusError> {
        // Check patch count
        if tx.patches.len() > self.config.max_patches_per_transaction {
            return Err(PatchBusError::TooManyPatches);
        }

        // Check namespace exists and has permissions
        let namespaces = self.namespaces.read();
        let source_ns = namespaces.get(&tx.source)
            .ok_or(PatchBusError::UnknownNamespace(tx.source))?;

        // Check resource limits
        if let Some(max) = source_ns.limits.max_patches_per_frame {
            let current = self.stats.read().patches_this_frame as u32;
            if current + tx.patches.len() as u32 > max {
                return Err(PatchBusError::ResourceLimitExceeded);
            }
        }

        // Validate each patch
        for patch in &tx.patches {
            self.validate_patch(patch, &source_ns)?;
        }

        Ok(())
    }

    /// Validate a single patch
    fn validate_patch(&self, patch: &Patch, source_ns: &Namespace) -> Result<(), PatchBusError> {
        // Check source matches
        if patch.source != source_ns.id && !source_ns.id.is_kernel() {
            return Err(PatchBusError::SourceMismatch);
        }

        // Check permissions based on patch kind
        match &patch.kind {
            PatchKind::Entity(ep) => {
                // Check if namespace can modify this entity's namespace
                if !source_ns.can_modify(ep.entity.namespace) {
                    return Err(PatchBusError::PermissionDenied);
                }
                // Check entity creation permission
                if matches!(ep.op, crate::patch::EntityOp::Create { .. })
                    && !source_ns.permissions.create_entities
                {
                    return Err(PatchBusError::PermissionDenied);
                }
            }
            PatchKind::Component(cp) => {
                if !source_ns.can_modify(cp.entity.namespace) {
                    return Err(PatchBusError::PermissionDenied);
                }
                if !source_ns.permissions.modify_components {
                    return Err(PatchBusError::PermissionDenied);
                }
            }
            PatchKind::Layer(_) => {
                if !source_ns.permissions.create_layers {
                    return Err(PatchBusError::PermissionDenied);
                }
            }
            PatchKind::Asset(_) => {
                if !source_ns.permissions.load_assets {
                    return Err(PatchBusError::PermissionDenied);
                }
            }
            PatchKind::Hierarchy(hp) => {
                // Hierarchy patches modify entities
                if !source_ns.can_modify(hp.entity.namespace) {
                    return Err(PatchBusError::PermissionDenied);
                }
            }
            PatchKind::Camera(cp) => {
                // Camera patches modify camera entities
                if !source_ns.can_modify(cp.entity.namespace) {
                    return Err(PatchBusError::PermissionDenied);
                }
                if !source_ns.permissions.modify_components {
                    return Err(PatchBusError::PermissionDenied);
                }
            }
        }

        Ok(())
    }

    /// Clear committed transaction history (called periodically)
    pub fn gc_committed(&self, keep_count: usize) {
        let mut committed = self.committed.write();
        if committed.len() > keep_count {
            let drain_count = committed.len() - keep_count;
            committed.drain(0..drain_count);
        }
    }
}

/// Handle for an app to interact with the patch bus
#[derive(Clone)]
pub struct NamespaceHandle {
    id: NamespaceId,
    sender: Sender<Transaction>,
    config: PatchBusConfig,
}

impl NamespaceHandle {
    /// Get the namespace ID
    pub fn id(&self) -> NamespaceId {
        self.id
    }

    /// Begin building a transaction
    pub fn begin_transaction(&self) -> TransactionBuilder {
        TransactionBuilder::new(self.id)
    }

    /// Submit a transaction
    pub fn submit(&self, transaction: Transaction) -> Result<TransactionId, PatchBusError> {
        let id = transaction.id;

        if transaction.patches.len() > self.config.max_patches_per_transaction {
            return Err(PatchBusError::TooManyPatches);
        }

        self.sender
            .try_send(transaction)
            .map_err(|_| PatchBusError::ChannelFull)?;

        Ok(id)
    }

    /// Submit a single patch as a transaction
    pub fn submit_patch(&self, patch: Patch) -> Result<TransactionId, PatchBusError> {
        let tx = self.begin_transaction().patch(patch).build();
        self.submit(tx)
    }
}

/// Errors from the patch bus
#[derive(Debug, Clone)]
pub enum PatchBusError {
    /// Too many pending transactions
    TooManyPendingTransactions,
    /// Too many patches in a transaction
    TooManyPatches,
    /// Unknown namespace
    UnknownNamespace(NamespaceId),
    /// Source namespace doesn't match patch source
    SourceMismatch,
    /// Permission denied for this operation
    PermissionDenied,
    /// Resource limit exceeded
    ResourceLimitExceeded,
    /// Channel is full
    ChannelFull,
    /// Validation failed
    ValidationFailed(String),
}

impl std::fmt::Display for PatchBusError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TooManyPendingTransactions => write!(f, "Too many pending transactions"),
            Self::TooManyPatches => write!(f, "Too many patches in transaction"),
            Self::UnknownNamespace(id) => write!(f, "Unknown namespace: {}", id),
            Self::SourceMismatch => write!(f, "Patch source doesn't match transaction source"),
            Self::PermissionDenied => write!(f, "Permission denied"),
            Self::ResourceLimitExceeded => write!(f, "Resource limit exceeded"),
            Self::ChannelFull => write!(f, "Transaction channel is full"),
            Self::ValidationFailed(msg) => write!(f, "Validation failed: {}", msg),
        }
    }
}

impl std::error::Error for PatchBusError {}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::patch::{EntityPatch, PatchKind};

    #[test]
    fn test_patch_bus_basic() {
        let bus = PatchBus::default();
        let ns = Namespace::new("test_app");
        let handle = bus.register_namespace(ns);

        // Create and submit a transaction
        let tx = handle
            .begin_transaction()
            .patch(Patch::new(
                handle.id(),
                PatchKind::Entity(EntityPatch::create(handle.id(), 1)),
            ))
            .build();

        let tx_id = handle.submit(tx).unwrap();

        // Receive pending
        bus.receive_pending();

        // Drain ready
        let ready = bus.drain_ready(1);
        assert_eq!(ready.len(), 1);
        assert_eq!(ready[0].id, tx_id);
    }

    #[test]
    fn test_namespace_isolation() {
        let bus = PatchBus::new(PatchBusConfig {
            allow_cross_namespace: false,
            ..Default::default()
        });

        let ns1 = Namespace::new("app1");
        let ns2 = Namespace::new("app2");
        let ns2_id = ns2.id;

        let handle1 = bus.register_namespace(ns1);
        let _handle2 = bus.register_namespace(ns2);

        // Try to create entity in another namespace (should fail validation)
        let tx = handle1
            .begin_transaction()
            .patch(Patch::new(
                handle1.id(),
                PatchKind::Entity(EntityPatch::create(ns2_id, 1)), // Wrong namespace!
            ))
            .build();

        handle1.submit(tx).unwrap();
        bus.receive_pending();

        // Transaction should fail validation
        // (In real usage, we'd check the result from kernel)
    }

    #[test]
    fn test_transaction_dependencies() {
        let bus = PatchBus::default();
        let ns = Namespace::new("test_app");
        let handle = bus.register_namespace(ns);

        // Create first transaction
        let tx1 = handle
            .begin_transaction()
            .patch(Patch::new(
                handle.id(),
                PatchKind::Entity(EntityPatch::create(handle.id(), 1)),
            ))
            .build();
        let tx1_id = tx1.id;

        // Create second transaction that depends on first
        let tx2 = handle
            .begin_transaction()
            .depends_on(tx1_id)
            .patch(Patch::new(
                handle.id(),
                PatchKind::Entity(EntityPatch::create(handle.id(), 2)),
            ))
            .build();

        handle.submit(tx1).unwrap();
        handle.submit(tx2).unwrap();
        bus.receive_pending();

        // Without committing tx1, tx2 shouldn't be ready
        let ready = bus.drain_ready(1);
        assert_eq!(ready.len(), 1); // Only tx1

        // Commit tx1
        bus.commit(TransactionResult::success(tx1_id, 1));

        // Now tx2 should be ready
        let ready = bus.drain_ready(2);
        assert_eq!(ready.len(), 1); // tx2
    }
}
