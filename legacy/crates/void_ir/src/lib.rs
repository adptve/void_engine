//! # Void IR - Intermediate Representation & Patch Bus
//!
//! This crate provides the core data flow mechanism for Void Engine.
//! Apps do not render directly - they emit IR patches that are batched
//! into transactions and applied by the kernel.
//!
//! ## Architecture
//!
//! ```text
//! App A ──┐
//!         ├──► Patch Bus ──► Transaction ──► Kernel Applies ──► World State
//! App B ──┘
//! ```
//!
//! ## Key Concepts
//!
//! - **Patch**: A single declarative operation (create/update/destroy)
//! - **Transaction**: A batch of patches that succeed or fail atomically
//! - **Namespace**: App-scoped isolation for patches
//! - **PatchBus**: Central hub that collects and batches patches

pub mod patch;
pub mod transaction;
pub mod bus;
pub mod namespace;
pub mod value;
pub mod validation;
pub mod snapshot;
pub mod batch;

pub use patch::{
    Patch, PatchKind, EntityPatch, ComponentPatch, EntityRef, LayerPatch, AssetPatch,
    HierarchyPatch, HierarchyOp,
    CameraPatch, CameraOp, ProjectionData, ViewportData,
    EntityOp, ComponentOp, LayerOp, AssetOp,
    LayerType, BlendMode,
};
pub use transaction::{
    Transaction, TransactionId, TransactionState, TransactionBuilder, TransactionResult,
    ConflictDetector, Conflict,
};
pub use bus::{PatchBus, PatchBusConfig, PatchBusError, NamespaceHandle};
pub use namespace::{Namespace, NamespaceId, NamespacePermissions, ResourceLimits};
pub use value::Value;
pub use validation::{
    ValidationContext, ValidationError, ValidationResult, PatchValidator,
    ComponentSchema, FieldSchema,
};
pub use snapshot::{
    StateSnapshot, SnapshotId, SnapshotManager,
    EntitySnapshot, ComponentSnapshot, LayerSnapshot, AssetSnapshot,
};
pub use batch::{PatchBatch, BatchOptimizer, BatchStats};
