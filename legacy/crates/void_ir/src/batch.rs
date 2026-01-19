//! Patch batching and optimization
//!
//! This module provides utilities for batching and optimizing patches:
//! - Merge redundant patches (e.g., multiple updates to same component)
//! - Eliminate no-op patches (e.g., destroy then create same entity)
//! - Reorder patches for optimal application
//! - Group related patches for better cache locality

use crate::patch::{ComponentOp, EntityOp, EntityRef, Patch, PatchKind};
use crate::transaction::{Transaction, TransactionBuilder};
use crate::namespace::NamespaceId;
use std::collections::HashMap;

/// A batch of related patches with optimization applied
#[derive(Debug, Clone)]
pub struct PatchBatch {
    /// Optimized patches
    patches: Vec<Patch>,
    /// Statistics about optimization
    stats: BatchStats,
}

impl PatchBatch {
    /// Create a new empty batch
    pub fn new() -> Self {
        Self {
            patches: Vec::new(),
            stats: BatchStats::default(),
        }
    }

    /// Create from a list of patches
    pub fn from_patches(patches: Vec<Patch>) -> Self {
        Self {
            stats: BatchStats {
                original_count: patches.len(),
                ..Default::default()
            },
            patches,
        }
    }

    /// Add a patch to the batch
    pub fn add(&mut self, patch: Patch) {
        self.patches.push(patch);
        self.stats.original_count += 1;
    }

    /// Get the patches
    pub fn patches(&self) -> &[Patch] {
        &self.patches
    }

    /// Get mutable patches
    pub fn patches_mut(&mut self) -> &mut Vec<Patch> {
        &mut self.patches
    }

    /// Get statistics
    pub fn stats(&self) -> &BatchStats {
        &self.stats
    }

    /// Convert to a transaction
    pub fn into_transaction(self, source: NamespaceId) -> Transaction {
        TransactionBuilder::new(source)
            .patches(self.patches)
            .build()
    }

    /// Optimize this batch
    pub fn optimize(&mut self) {
        let original_count = self.patches.len();

        // Step 1: Merge redundant patches
        self.merge_redundant();

        // Step 2: Eliminate contradictions
        self.eliminate_contradictions();

        // Step 3: Sort by priority and dependencies
        self.sort_optimal();

        self.stats.optimized_count = self.patches.len();
        self.stats.patches_merged = original_count.saturating_sub(self.patches.len());
    }

    /// Merge redundant patches (multiple updates to same target)
    fn merge_redundant(&mut self) {
        let mut merged: HashMap<PatchKey, Vec<Patch>> = HashMap::new();

        for patch in self.patches.drain(..) {
            let key = PatchKey::from_patch(&patch);

            match merged.get_mut(&key) {
                Some(existing_list) => {
                    // Try to merge with last patch in the list
                    if let Some(existing) = existing_list.last_mut() {
                        if let Some(merged_patch) = merge_patches(existing, &patch) {
                            *existing = merged_patch;
                            self.stats.patches_merged += 1;
                        } else {
                            // Can't merge, keep both
                            existing_list.push(patch);
                        }
                    } else {
                        existing_list.push(patch);
                    }
                }
                None => {
                    merged.insert(key, vec![patch]);
                }
            }
        }

        self.patches = merged.into_values().flatten().collect();
    }

    /// Eliminate contradictory patches (create then destroy, etc.)
    fn eliminate_contradictions(&mut self) {
        let mut entity_ops: HashMap<EntityRef, Vec<usize>> = HashMap::new();
        let mut to_remove: Vec<usize> = Vec::new();

        // Find all entity operations
        for (idx, patch) in self.patches.iter().enumerate() {
            if let PatchKind::Entity(ep) = &patch.kind {
                entity_ops.entry(ep.entity).or_default().push(idx);
            }
        }

        // Check for create -> destroy patterns
        for (entity, indices) in entity_ops {
            if indices.len() >= 2 {
                let ops: Vec<_> = indices
                    .iter()
                    .filter_map(|&idx| {
                        if let PatchKind::Entity(ep) = &self.patches[idx].kind {
                            Some((idx, &ep.op))
                        } else {
                            None
                        }
                    })
                    .collect();

                // Look for create followed by destroy
                for i in 0..ops.len() {
                    for j in (i + 1)..ops.len() {
                        let (idx1, op1) = ops[i];
                        let (idx2, op2) = ops[j];

                        if matches!(op1, EntityOp::Create { .. }) && matches!(op2, EntityOp::Destroy) {
                            // Create then destroy - eliminate both
                            to_remove.push(idx1);
                            to_remove.push(idx2);
                            self.stats.patches_eliminated += 2;
                        }
                    }
                }
            }
        }

        // Remove contradictory patches (in reverse order to preserve indices)
        to_remove.sort_unstable();
        to_remove.dedup();
        for &idx in to_remove.iter().rev() {
            self.patches.remove(idx);
        }
    }

    /// Sort patches for optimal application order
    fn sort_optimal(&mut self) {
        // Sort by:
        // 1. Priority (high to low)
        // 2. Type (Entity creates first, then components, then others)
        // 3. Namespace (group by namespace for better cache locality)
        self.patches.sort_by(|a, b| {
            // First by priority
            let priority_cmp = b.priority.cmp(&a.priority);
            if priority_cmp != std::cmp::Ordering::Equal {
                return priority_cmp;
            }

            // Then by type order
            let type_order_a = patch_type_order(&a.kind);
            let type_order_b = patch_type_order(&b.kind);
            let type_cmp = type_order_a.cmp(&type_order_b);
            if type_cmp != std::cmp::Ordering::Equal {
                return type_cmp;
            }

            // Finally by namespace
            a.source.raw().cmp(&b.source.raw())
        });
    }

    /// Check if batch is empty
    pub fn is_empty(&self) -> bool {
        self.patches.is_empty()
    }

    /// Get the number of patches
    pub fn len(&self) -> usize {
        self.patches.len()
    }
}

impl Default for PatchBatch {
    fn default() -> Self {
        Self::new()
    }
}

/// Statistics about batch optimization
#[derive(Debug, Clone, Default)]
pub struct BatchStats {
    /// Original patch count
    pub original_count: usize,
    /// Optimized patch count
    pub optimized_count: usize,
    /// Number of patches merged
    pub patches_merged: usize,
    /// Number of patches eliminated
    pub patches_eliminated: usize,
}

impl BatchStats {
    /// Get the optimization ratio (0.0 = no optimization, 1.0 = everything removed)
    pub fn optimization_ratio(&self) -> f64 {
        if self.original_count == 0 {
            0.0
        } else {
            1.0 - (self.optimized_count as f64 / self.original_count as f64)
        }
    }
}

/// Key for identifying duplicate patches
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
enum PatchKey {
    Entity(EntityRef),
    Component { entity: EntityRef, component: String },
    Layer(String),
    Asset(String),
    Hierarchy(EntityRef),
    Camera(EntityRef),
}

impl PatchKey {
    fn from_patch(patch: &Patch) -> Self {
        match &patch.kind {
            PatchKind::Entity(ep) => Self::Entity(ep.entity),
            PatchKind::Component(cp) => Self::Component {
                entity: cp.entity,
                component: cp.component.clone(),
            },
            PatchKind::Layer(lp) => Self::Layer(lp.layer_id.clone()),
            PatchKind::Asset(ap) => Self::Asset(ap.asset_id.clone()),
            PatchKind::Hierarchy(hp) => Self::Hierarchy(hp.entity),
            PatchKind::Camera(cp) => Self::Camera(cp.entity),
        }
    }
}

/// Merge two patches if possible
fn merge_patches(existing: &Patch, new: &Patch) -> Option<Patch> {
    match (&existing.kind, &new.kind) {
        (PatchKind::Component(cp1), PatchKind::Component(cp2)) => {
            if cp1.entity == cp2.entity && cp1.component == cp2.component {
                // Merge component patches
                match (&cp1.op, &cp2.op) {
                    (ComponentOp::Set { .. }, ComponentOp::Set { data }) => {
                        // Later set overwrites earlier set
                        Some(Patch {
                            source: new.source,
                            kind: PatchKind::Component(crate::patch::ComponentPatch {
                                entity: cp2.entity,
                                component: cp2.component.clone(),
                                op: ComponentOp::Set { data: data.clone() },
                            }),
                            priority: new.priority.max(existing.priority),
                            timestamp: new.timestamp,
                        })
                    }
                    (ComponentOp::Update { fields: fields1 }, ComponentOp::Update { fields: fields2 }) => {
                        // Merge update fields
                        let mut merged_fields = fields1.clone();
                        merged_fields.extend(fields2.clone());

                        Some(Patch {
                            source: new.source,
                            kind: PatchKind::Component(crate::patch::ComponentPatch {
                                entity: cp2.entity,
                                component: cp2.component.clone(),
                                op: ComponentOp::Update { fields: merged_fields },
                            }),
                            priority: new.priority.max(existing.priority),
                            timestamp: new.timestamp,
                        })
                    }
                    (ComponentOp::Set { .. }, ComponentOp::Update { fields }) => {
                        // Update after set - keep set and apply updates
                        // This is complex - for now just keep the latest
                        Some(new.clone())
                    }
                    _ => None,
                }
            } else {
                None
            }
        }
        (PatchKind::Layer(lp1), PatchKind::Layer(lp2)) => {
            if lp1.layer_id == lp2.layer_id {
                // Later layer operation overwrites earlier
                Some(new.clone())
            } else {
                None
            }
        }
        _ => None,
    }
}

/// Get the application order for a patch type
fn patch_type_order(kind: &PatchKind) -> u8 {
    match kind {
        PatchKind::Entity(ep) => match ep.op {
            EntityOp::Create { .. } => 0, // Create entities first
            EntityOp::Destroy => 4,        // Destroy entities last
            _ => 2,
        },
        PatchKind::Component(_) => 1, // Components after entity creation
        PatchKind::Layer(_) => 2,     // Layers in the middle
        PatchKind::Asset(_) => 3,     // Assets before destruction
        PatchKind::Hierarchy(_) => 2, // Hierarchy in the middle (after entity creation)
        PatchKind::Camera(_) => 2,    // Camera patches in the middle (after entity creation)
    }
}

/// Batch optimizer that can optimize multiple batches
pub struct BatchOptimizer {
    /// Whether to enable merging
    enable_merge: bool,
    /// Whether to enable contradiction elimination
    enable_eliminate: bool,
    /// Whether to enable sorting
    enable_sort: bool,
}

impl BatchOptimizer {
    /// Create a new optimizer with all optimizations enabled
    pub fn new() -> Self {
        Self {
            enable_merge: true,
            enable_eliminate: true,
            enable_sort: true,
        }
    }

    /// Disable all optimizations
    pub fn disabled() -> Self {
        Self {
            enable_merge: false,
            enable_eliminate: false,
            enable_sort: false,
        }
    }

    /// Set whether to merge redundant patches
    pub fn with_merge(mut self, enable: bool) -> Self {
        self.enable_merge = enable;
        self
    }

    /// Set whether to eliminate contradictions
    pub fn with_eliminate(mut self, enable: bool) -> Self {
        self.enable_eliminate = enable;
        self
    }

    /// Set whether to sort patches
    pub fn with_sort(mut self, enable: bool) -> Self {
        self.enable_sort = enable;
        self
    }

    /// Optimize a batch
    pub fn optimize(&self, batch: &mut PatchBatch) {
        if self.enable_merge {
            batch.merge_redundant();
        }

        if self.enable_eliminate {
            batch.eliminate_contradictions();
        }

        if self.enable_sort {
            batch.sort_optimal();
        }

        batch.stats.optimized_count = batch.patches.len();
    }

    /// Optimize a transaction
    pub fn optimize_transaction(&self, transaction: &mut Transaction) {
        let mut batch = PatchBatch::from_patches(std::mem::take(&mut transaction.patches));
        self.optimize(&mut batch);
        transaction.patches = batch.patches;
    }
}

impl Default for BatchOptimizer {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::patch::{ComponentPatch, EntityPatch};
    use crate::value::Value;

    #[test]
    fn test_merge_component_updates() {
        let ns = NamespaceId::new();
        let entity = EntityRef::new(ns, 1);

        let mut batch = PatchBatch::new();

        // Add two updates to the same component
        let mut fields1 = HashMap::new();
        fields1.insert("x".to_string(), Value::from(1.0));

        let mut fields2 = HashMap::new();
        fields2.insert("y".to_string(), Value::from(2.0));

        batch.add(Patch::new(
            ns,
            PatchKind::Component(ComponentPatch {
                entity,
                component: "Transform".to_string(),
                op: ComponentOp::Update { fields: fields1 },
            }),
        ));

        batch.add(Patch::new(
            ns,
            PatchKind::Component(ComponentPatch {
                entity,
                component: "Transform".to_string(),
                op: ComponentOp::Update { fields: fields2 },
            }),
        ));

        assert_eq!(batch.len(), 2);

        batch.optimize();

        // Should be merged into one patch
        assert_eq!(batch.len(), 1);
        assert_eq!(batch.stats.patches_merged, 1);
    }

    #[test]
    fn test_eliminate_contradictions() {
        let ns = NamespaceId::new();
        let entity = EntityRef::new(ns, 1);

        let mut batch = PatchBatch::new();

        // Create entity
        batch.add(Patch::new(
            ns,
            PatchKind::Entity(EntityPatch::create(ns, 1)),
        ));

        // Then destroy it
        batch.add(Patch::new(
            ns,
            PatchKind::Entity(EntityPatch::destroy(entity)),
        ));

        assert_eq!(batch.len(), 2);

        batch.optimize();

        // Both should be eliminated
        assert_eq!(batch.len(), 0);
        assert_eq!(batch.stats.patches_eliminated, 2);
    }

    #[test]
    fn test_patch_ordering() {
        let ns = NamespaceId::new();
        let entity = EntityRef::new(ns, 1);

        let mut batch = PatchBatch::new();

        // Add patches in wrong order
        batch.add(Patch::new(
            ns,
            PatchKind::Component(ComponentPatch::set(
                entity,
                "Transform",
                Value::from([0.0, 0.0, 0.0]),
            )),
        ));

        batch.add(Patch::new(
            ns,
            PatchKind::Entity(EntityPatch::create(ns, 1)),
        ));

        batch.optimize();

        // Entity creation should come before component set
        assert!(matches!(batch.patches[0].kind, PatchKind::Entity(_)));
        assert!(matches!(batch.patches[1].kind, PatchKind::Component(_)));
    }

    #[test]
    fn test_batch_stats() {
        let mut batch = PatchBatch::new();
        let ns = NamespaceId::new();
        let entity = EntityRef::new(ns, 1);

        // Add redundant patches
        for _ in 0..5 {
            batch.add(Patch::new(
                ns,
                PatchKind::Component(ComponentPatch::set(
                    entity,
                    "Transform",
                    Value::from([0.0, 0.0, 0.0]),
                )),
            ));
        }

        assert_eq!(batch.stats.original_count, 5);

        batch.optimize();

        assert_eq!(batch.len(), 1);
        assert!(batch.stats.optimization_ratio() > 0.0);
    }
}
