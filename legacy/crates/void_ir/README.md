# void_ir - Intermediate Representation & Patch System

The declarative state change system for Metaverse OS. Apps emit IR patches instead of mutating state directly, enabling atomic transactions, rollback, and complete isolation.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  App Code                                                   │
│      │                                                      │
│      ▼                                                      │
│  Patch Submission (with namespace)                          │
│      │                                                      │
│      ▼                                                      │
│  Validation (capability check, limits, schema)              │
│      │                                                      │
│      ▼                                                      │
│  Queue in PatchBus (per-namespace)                          │
│      │                                                      │
│      ▼                                                      │
│  Frame Start: Collect into Transaction                      │
│      │                                                      │
│      ▼                                                      │
│  Batch Optimization (merge, eliminate, reorder)             │
│      │                                                      │
│      ▼                                                      │
│  Conflict Detection (check for concurrent modifications)    │
│      │                                                      │
│      ▼                                                      │
│  Snapshot (for rollback)                                    │
│      │                                                      │
│      ▼                                                      │
│  Apply Transaction (atomic, with rollback)                  │
└─────────────────────────────────────────────────────────────┘
```

## Core Principles

### 1. Declarative Intent
Patches describe **WHAT**, not **HOW**:
```rust
// ❌ Imperative (direct mutation)
world.entities.create(entity);
entity.add_component(Transform { position: [0, 0, 0] });

// ✓ Declarative (IR patch)
let tx = TransactionBuilder::new(namespace)
    .patch(Patch::new(ns, PatchKind::Entity(EntityPatch::create(ns, 1))))
    .patch(Patch::new(ns, PatchKind::Component(ComponentPatch::set(
        entity, "Transform", Value::from([0.0, 0.0, 0.0])
    ))))
    .build();
```

### 2. Namespace Isolation
Patches ONLY affect owning namespace:
```rust
let app1 = Namespace::new("my_app");
let app2 = Namespace::new("other_app");

// App 1 can modify its own entities
let entity1 = EntityRef::new(app1.id, 1);
// ✓ OK
ComponentPatch::set(entity1, "Transform", data);

// App 1 CANNOT modify app 2's entities
let entity2 = EntityRef::new(app2.id, 1);
// ❌ REJECTED by validation
ComponentPatch::set(entity2, "Transform", data);
```

### 3. Validation First
Validate BEFORE queuing:
```rust
let validator = PatchValidator::new();
validator.context_mut().register_component_schema(
    ComponentSchema::new("Transform")
        .with_required_field("position", FieldSchema::Vec3)
);

// Validation catches errors early
match validator.validate_patch(&patch) {
    Ok(_) => patch_bus.submit(tx),
    Err(e) => eprintln!("Invalid patch: {}", e),
}
```

### 4. Atomic Application
Transaction applies all-or-nothing:
```rust
// Take snapshot before applying
let snapshot = take_snapshot();

match apply_transaction(tx) {
    Ok(_) => commit(),
    Err(e) => {
        // ANY failure = full rollback
        restore_snapshot(snapshot);
    }
}
```

### 5. Frame Boundary
Patches apply at frame boundaries, never mid-frame:
```rust
// Frame N: Apps submit patches
app1.submit(tx1);
app2.submit(tx2);

// Frame N+1 start: Kernel collects and applies
let ready = patch_bus.drain_ready(frame);
for tx in ready {
    apply(tx); // Applied atomically
}
```

## Modules

### Core Types (`patch.rs`)

**Patch Types:**
- `EntityPatch`: Create, Destroy, Enable, Disable, SetParent, Tags
- `ComponentPatch`: Set, Update, Remove
- `LayerPatch`: Create, Update, Destroy
- `AssetPatch`: Load, Unload, Update

**Example:**
```rust
use void_ir::{Patch, PatchKind, EntityPatch, ComponentPatch, Value};

let entity = EntityRef::new(namespace, 1);

// Entity creation
let create_patch = Patch::new(
    namespace,
    PatchKind::Entity(EntityPatch::create(namespace, 1))
);

// Component set
let transform_patch = Patch::new(
    namespace,
    PatchKind::Component(ComponentPatch::set(
        entity,
        "Transform",
        Value::from_iter([
            ("position", Value::from([0.0, 5.0, 0.0])),
            ("rotation", Value::from([0.0, 0.0, 0.0, 1.0])),
        ])
    ))
);
```

### Transactions (`transaction.rs`)

**Transaction Builder:**
```rust
let tx = TransactionBuilder::new(namespace)
    .description("Spawn player")
    .patch(create_entity_patch)
    .patch(add_transform_patch)
    .patch(add_mesh_patch)
    .depends_on(previous_tx_id)
    .build();
```

**Conflict Detection:**
```rust
let mut detector = ConflictDetector::new();

detector.add_transaction(&tx1);

if detector.has_conflict(&tx2) {
    let conflicts = detector.get_conflicts(&tx2);
    for conflict in conflicts {
        println!("Conflict: {}", conflict);
    }
}
```

### Validation (`validation.rs`)

**Component Schemas:**
```rust
let schema = ComponentSchema::new("Transform")
    .with_required_field("position", FieldSchema::Vec3)
    .with_optional_field("rotation", FieldSchema::Vec4)
    .with_optional_field("scale", FieldSchema::Vec3);

validator.context_mut().register_component_schema(schema);
```

**Validation:**
```rust
match validator.validate_patch(&patch) {
    Ok(_) => println!("Valid!"),
    Err(ValidationError::EntityNotFound(entity)) => {
        eprintln!("Entity {:?} doesn't exist", entity);
    }
    Err(ValidationError::TypeMismatch { expected, got }) => {
        eprintln!("Type mismatch: expected {}, got {}", expected, got);
    }
    Err(e) => eprintln!("Validation error: {}", e),
}
```

### Snapshots (`snapshot.rs`)

**Creating Snapshots:**
```rust
let mut snapshot = StateSnapshot::new(frame);

// Record entity state
snapshot.entities.insert(entity, enabled, parent, tags);

// Record component state
snapshot.components.set(entity, "Transform", transform_data);

// Store snapshot
let snapshot_id = snapshot_manager.store(snapshot);
```

**Rollback:**
```rust
// Compute diff for rollback
let undo_patches = new_snapshot.diff(&old_snapshot);

// Apply undo patches to rollback
for patch in undo_patches {
    apply_patch(patch)?;
}
```

**Snapshot Manager:**
```rust
let mut manager = SnapshotManager::new(
    max_snapshots: 100,
    max_memory: 100 * 1024 * 1024 // 100 MB
);

// Automatically GCs old snapshots
let id = manager.store(snapshot);
```

### Batch Optimization (`batch.rs`)

**Optimization:**
```rust
let mut batch = PatchBatch::from_patches(patches);

// Optimize:
// - Merge redundant patches
// - Eliminate contradictions (create then destroy)
// - Reorder for optimal application
batch.optimize();

println!("Optimization ratio: {:.1}%",
    batch.stats().optimization_ratio() * 100.0);
```

**Custom Optimization:**
```rust
let optimizer = BatchOptimizer::new()
    .with_merge(true)
    .with_eliminate(true)
    .with_sort(true);

optimizer.optimize(&mut batch);
```

### Patch Bus (`bus.rs`)

**Configuration:**
```rust
let config = PatchBusConfig {
    max_pending_transactions: 1000,
    max_patches_per_transaction: 10000,
    validate_on_submit: true,
    allow_cross_namespace: false,
};

let patch_bus = PatchBus::new(config);
```

**Namespace Handles:**
```rust
let namespace = Namespace::new("my_app");
let handle = patch_bus.register_namespace(namespace);

// Submit transactions
let tx = handle.begin_transaction()
    .patch(patch1)
    .patch(patch2)
    .build();

handle.submit(tx)?;
```

**Frame Processing:**
```rust
// Called by kernel each frame
patch_bus.begin_frame(frame_number);

// Get ready transactions (dependencies satisfied)
let ready = patch_bus.drain_ready(frame_number);

for tx in ready {
    match apply_transaction(tx) {
        Ok(result) => patch_bus.commit(result),
        Err(e) => eprintln!("Transaction failed: {}", e),
    }
}
```

## Patch Categories

| Category | Operations | Capability Required |
|----------|------------|---------------------|
| Entity | Create, Destroy | CreateEntities |
| Component | Set, Remove | CreateEntities |
| Layer | Create, Destroy, Configure | Render3D/Overlay/Effect |
| Asset | Load, Unload | LoadAsset |
| Transform | Position, Rotation, Scale | CreateEntities |

## Patch Limits

```rust
pub struct ResourceLimits {
    max_entities: Option<u32>,              // e.g., 10_000
    max_components_per_entity: Option<u32>, // e.g., 64
    max_layers: Option<u32>,                // e.g., 8
    max_memory_bytes: Option<u64>,          // e.g., 256 MB
    max_patches_per_frame: Option<u32>,     // e.g., 1000
}
```

## Usage Example

```rust
use void_ir::*;

// 1. Setup
let mut patch_bus = PatchBus::default();
let mut validator = PatchValidator::new();
let mut snapshot_manager = SnapshotManager::new(100, 100 * 1024 * 1024);
let optimizer = BatchOptimizer::new();

// 2. Register namespace
let namespace = Namespace::new("physics_app");
let handle = patch_bus.register_namespace(namespace.clone());
validator.context_mut().register_namespace(Arc::new(namespace));

// 3. Create transaction
let entity = EntityRef::new(handle.id(), 1);
let tx = handle.begin_transaction()
    .description("Spawn physics object")
    .patch(Patch::new(
        handle.id(),
        PatchKind::Entity(EntityPatch::create(handle.id(), 1))
    ))
    .patch(Patch::new(
        handle.id(),
        PatchKind::Component(ComponentPatch::set(
            entity,
            "Transform",
            Value::from([0.0, 0.0, 0.0])
        ))
    ))
    .build();

// 4. Validate
validator.validate_patches(&tx.patches)?;

// 5. Submit
handle.submit(tx)?;

// 6. Process (kernel-side)
patch_bus.begin_frame(frame);
let ready = patch_bus.drain_ready(frame);

for mut tx in ready {
    // Optimize
    let mut batch = PatchBatch::from_patches(tx.patches);
    batch.optimize();

    // Take snapshot
    let snapshot = take_current_snapshot();
    let snapshot_id = snapshot_manager.store(snapshot);

    // Apply
    match apply_patches(batch.patches()) {
        Ok(result) => patch_bus.commit(result),
        Err(e) => {
            // Rollback
            if let Some(snapshot) = snapshot_manager.get(snapshot_id) {
                rollback_to_snapshot(snapshot);
            }
        }
    }
}
```

## Implementation Checklist

When implementing IR/Patch features:

- [ ] Is capability checked at submission?
- [ ] Is namespace isolation enforced?
- [ ] Are limits enforced before queuing?
- [ ] Is application atomic (all-or-nothing)?
- [ ] Can patches be validated without applying?
- [ ] Is rollback supported via snapshots?
- [ ] Are conflicts detected?
- [ ] Are batches optimized?

## Constraints

- **NEVER** allow cross-namespace patches
- **NEVER** apply patches mid-frame
- **ALWAYS** validate before queuing
- **ALWAYS** enforce capability requirements
- **ALWAYS** support transaction rollback
- **ALWAYS** detect conflicts
- **ALWAYS** apply atomically

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Patch submission | O(1) | Queue to channel |
| Validation | O(n) | Per patch in transaction |
| Conflict detection | O(n) | Per patch checked |
| Batch optimization | O(n log n) | Sort + merge |
| Snapshot creation | O(entities + components) | Full state copy |
| Snapshot diff | O(entities + components) | Compare two snapshots |
| Transaction apply | O(patches) | Linear with patch count |

## Memory Usage

- **Snapshots**: ~(entities × 64 bytes) + (components × 128 bytes)
- **Transactions**: ~(patches × 256 bytes)
- **Patch Bus**: ~(pending_txs × avg_tx_size)

Snapshot manager automatically GCs old snapshots when limits are exceeded.

## Thread Safety

- `PatchBus`: Thread-safe via `crossbeam-channel` and `parking_lot::RwLock`
- `SnapshotManager`: Single-threaded (kernel-owned)
- `ValidationContext`: Single-threaded (kernel-owned)
- `ConflictDetector`: Single-threaded (kernel-owned)

## Testing

Run tests:
```bash
cargo test -p void_ir
```

Run example:
```bash
cargo run --package void_ir --example complete_ir_system
```

## Integration with Kernel

The kernel uses the IR system as follows:

```rust
impl Kernel {
    pub fn run_frame(&mut self) {
        // 1. Receive pending patches
        self.patch_bus.begin_frame(self.frame_number);

        // 2. Drain ready transactions
        let ready = self.patch_bus.drain_ready(self.frame_number);

        // 3. Detect conflicts
        let mut detector = ConflictDetector::new();
        for tx in &ready {
            if detector.has_conflict(tx) {
                // Handle conflict (reject or retry)
            }
            detector.add_transaction(tx);
        }

        // 4. Optimize batches
        for tx in ready {
            let mut batch = PatchBatch::from_patches(tx.patches);
            batch.optimize();

            // 5. Take snapshot
            let snapshot = self.take_snapshot();
            let snapshot_id = self.snapshot_manager.store(snapshot);

            // 6. Apply atomically
            match self.apply_batch(batch) {
                Ok(result) => self.patch_bus.commit(result),
                Err(e) => {
                    // Rollback
                    if let Some(snap) = self.snapshot_manager.get(snapshot_id) {
                        self.restore_snapshot(snap);
                    }
                }
            }
        }
    }
}
```

## See Also

- [docs/VISION.md](../../docs/VISION.md) - Overall architecture
- [docs/architecture/01-KERNEL-ARCHITECTURE.md](../../docs/architecture/01-KERNEL-ARCHITECTURE.md) - Kernel design
- [crates/void_kernel](../void_kernel) - Kernel implementation
