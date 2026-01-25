# void_ir Integration Diagrams

This document provides Mermaid diagrams showing the void_ir module architecture and data flow.

## Class Diagram

```mermaid
classDiagram
    class Patch {
        +PatchKind kind()
        +target_entity() EntityRef
        +visit(F)
    }

    class PatchBatch {
        -vector~Patch~ m_patches
        +push(Patch)
        +append(PatchBatch)
        +size() size_t
        +reserve(size_t)
    }

    class PatchBus {
        -shared_mutex m_mutex
        -map~uint64_t, Subscription~ m_subscriptions
        -atomic~size_t~ m_sequence_number
        +subscribe(PatchFilter, Callback) SubscriptionId
        +unsubscribe(SubscriptionId)
        +publish(Patch, NamespaceId, TransactionId)
        +publish_batch(PatchBatch)
        +shutdown()
    }

    class AsyncPatchBus {
        -mutex m_queue_mutex
        -queue~PatchEvent~ m_queue
        -condition_variable m_condition
        +publish(Patch)
        +try_consume() optional~PatchEvent~
        +consume() optional~PatchEvent~
        +consume_all() vector~PatchEvent~
        +shutdown()
    }

    class Transaction {
        -TransactionId m_id
        -NamespaceId m_namespace
        -TransactionState m_state
        -PatchBatch m_patches
        -vector~TransactionId~ m_dependencies
        +id() TransactionId
        +state() TransactionState
        +patches() PatchBatch
        +add_patch(Patch)
        +submit()
        +commit()
        +rollback()
    }

    class TransactionBuilder {
        -NamespaceId m_namespace
        -PatchBatch m_patches
        +create_entity(EntityRef)
        +add_component(EntityRef, type, Value)
        +set_field(EntityRef, type, field, Value)
        +build(TransactionId) Transaction
    }

    class TransactionQueue {
        -vector~Transaction~ m_pending
        +enqueue(Transaction)
        +dequeue() optional~Transaction~
        +peek() Transaction*
    }

    class ConflictDetector {
        -map~uint64_t, vector~TransactionId~~ m_modified_entities
        -map~pair, vector~TransactionId~~ m_modified_components
        +track(Transaction)
        +detect() vector~Conflict~
        +check(Transaction) optional~Conflict~
        +clear()
    }

    class SchemaRegistry {
        -map~string, ComponentSchema~ m_schemas
        +register_schema(ComponentSchema)
        +get(type_name) ComponentSchema*
        +validate_patch(ComponentPatch) ValidationResult
    }

    class PatchValidator {
        -SchemaRegistry& m_schemas
        +validate(Patch, Namespace) ValidationResult
        +validate_batch(PatchBatch, Namespace) ValidationResult
    }

    class BatchOptimizer {
        -Options m_options
        -OptimizationStats m_stats
        +optimize(PatchBatch) PatchBatch
        +stats() OptimizationStats
    }

    class Namespace {
        -NamespaceId m_id
        -string m_name
        -NamespacePermissions m_permissions
        -ResourceLimits m_limits
        -ResourceUsage m_usage
        +allocate_entity() uint64_t
        +within_limits() bool
    }

    class NamespaceRegistry {
        -vector~Namespace~ m_namespaces
        -map~string, NamespaceId~ m_name_to_id
        +create(name) NamespaceId
        +get(NamespaceId) Namespace*
        +find_by_name(name) optional~NamespaceId~
    }

    class IRSystem {
        -NamespaceRegistry m_namespaces
        -SchemaRegistry m_schemas
        -PatchBus m_patch_bus
        -AsyncPatchBus m_async_bus
        -BatchOptimizer m_optimizer
        -TransactionQueue m_transaction_queue
        -ConflictDetector m_conflict_detector
        +begin_transaction(NamespaceId) TransactionBuilder
        +submit_transaction(Transaction)
        +validate_transaction(Transaction) ValidationResult
    }

    class HotReloadable {
        <<interface>>
        +snapshot() Result~HotReloadSnapshot~
        +restore(HotReloadSnapshot) Result~void~
        +prepare_reload() Result~void~
        +finish_reload() Result~void~
    }

    Patch --> PatchBatch
    PatchBatch --> Transaction
    Transaction --> TransactionQueue
    Transaction --> ConflictDetector
    TransactionBuilder --> Transaction
    PatchBus --> PatchEvent
    AsyncPatchBus --> PatchEvent
    IRSystem --> NamespaceRegistry
    IRSystem --> SchemaRegistry
    IRSystem --> PatchBus
    IRSystem --> AsyncPatchBus
    IRSystem --> TransactionQueue
    IRSystem --> ConflictDetector
    IRSystem --> BatchOptimizer
    PatchValidator --> SchemaRegistry
    IRSystem ..|> HotReloadable
```

## Patch Flow Sequence Diagram

```mermaid
sequenceDiagram
    participant Client
    participant TransactionBuilder
    participant Transaction
    participant PatchValidator
    participant SchemaRegistry
    participant BatchOptimizer
    participant TransactionQueue
    participant ConflictDetector
    participant PatchBus
    participant Subscribers

    Client->>TransactionBuilder: begin_transaction(namespace)
    TransactionBuilder->>TransactionBuilder: create_entity()
    TransactionBuilder->>TransactionBuilder: add_component()
    TransactionBuilder->>TransactionBuilder: set_field()
    TransactionBuilder->>Transaction: build(id)

    Client->>PatchValidator: validate(patches)
    PatchValidator->>SchemaRegistry: get(component_type)
    SchemaRegistry-->>PatchValidator: ComponentSchema
    PatchValidator->>PatchValidator: validate fields
    PatchValidator-->>Client: ValidationResult

    alt Validation Passed
        Client->>BatchOptimizer: optimize(patches)
        BatchOptimizer->>BatchOptimizer: eliminate_contradictions()
        BatchOptimizer->>BatchOptimizer: merge_consecutive()
        BatchOptimizer->>BatchOptimizer: sort_for_efficiency()
        BatchOptimizer-->>Client: optimized PatchBatch

        Client->>ConflictDetector: check(transaction)
        ConflictDetector-->>Client: optional<Conflict>

        alt No Conflict
            Client->>TransactionQueue: enqueue(transaction)
            Transaction->>Transaction: submit()
            TransactionQueue->>TransactionQueue: dequeue()
            Transaction->>Transaction: begin_apply()

            loop For each patch
                PatchBus->>Subscribers: callback(PatchEvent)
            end

            Transaction->>Transaction: commit()
            ConflictDetector->>ConflictDetector: track(transaction)
        else Conflict Detected
            Client->>Client: handle conflict
        end
    else Validation Failed
        Client->>Client: handle errors
    end
```

## Hot-Reload Sequence Diagram

```mermaid
sequenceDiagram
    participant HotReloadManager
    participant IRSystem
    participant PatchBus
    participant TransactionQueue
    participant ConflictDetector
    participant SchemaRegistry
    participant NamespaceRegistry

    Note over HotReloadManager: Reload Triggered

    HotReloadManager->>IRSystem: prepare_reload()
    IRSystem->>PatchBus: shutdown()
    PatchBus->>PatchBus: clear subscriptions
    IRSystem->>TransactionQueue: clear()
    IRSystem->>ConflictDetector: clear()
    IRSystem-->>HotReloadManager: Ok()

    HotReloadManager->>IRSystem: snapshot()

    par Parallel Snapshots
        IRSystem->>NamespaceRegistry: snapshot_state()
        NamespaceRegistry-->>IRSystem: binary data
    and
        IRSystem->>SchemaRegistry: snapshot_state()
        SchemaRegistry-->>IRSystem: binary data
    end

    IRSystem->>IRSystem: write MAGIC + VERSION
    IRSystem->>IRSystem: serialize state
    IRSystem-->>HotReloadManager: HotReloadSnapshot

    Note over HotReloadManager: DLL Unload/Reload

    HotReloadManager->>IRSystem: restore(snapshot)
    IRSystem->>IRSystem: verify MAGIC
    IRSystem->>IRSystem: check VERSION
    IRSystem->>NamespaceRegistry: restore_state()
    IRSystem->>SchemaRegistry: restore_state()
    IRSystem-->>HotReloadManager: Ok()

    HotReloadManager->>IRSystem: finish_reload()
    IRSystem->>PatchBus: reinitialize()
    IRSystem->>TransactionQueue: reinitialize()
    IRSystem->>ConflictDetector: reinitialize()
    IRSystem-->>HotReloadManager: Ok()

    Note over HotReloadManager: Clients re-subscribe
```

## Binary Snapshot Format

```mermaid
graph TD
    subgraph Snapshot Format
        A[4 bytes: MAGIC] --> B[4 bytes: VERSION]
        B --> C[Payload Data]
    end

    subgraph MAGIC Values
        M1[PBUS: 0x50425553]
        M2[APBU: 0x41504255]
        M3[BOPT: 0x424F5054]
        M4[NREG: 0x4E524547]
        M5[TXQU: 0x54585155]
        M6[SREG: 0x53524547]
        M7[PATC: 0x50415443]
        M8[VOID: 0x564F4944]
    end
```

## Component Relationships

```mermaid
graph TB
    subgraph IR Module
        Patch[Patch]
        Value[Value]
        PatchBatch[PatchBatch]
        Bus[PatchBus]
        AsyncBus[AsyncPatchBus]
        Batch[BatchOptimizer]
        Dedup[PatchDeduplicator]
        NS[Namespace]
        NSReg[NamespaceRegistry]
        TX[Transaction]
        TXBuilder[TransactionBuilder]
        TXQueue[TransactionQueue]
        Conflict[ConflictDetector]
        Schema[ComponentSchema]
        SchemaReg[SchemaRegistry]
        Validator[PatchValidator]
        Snap[SnapshotManager]
        IR[IRSystem]
    end

    Value --> Patch
    Patch --> PatchBatch
    PatchBatch --> TX
    TX --> TXBuilder
    TX --> TXQueue
    TX --> Conflict

    NS --> NSReg
    Schema --> SchemaReg
    SchemaReg --> Validator
    PatchBatch --> Validator

    PatchBatch --> Batch
    PatchBatch --> Dedup
    PatchBatch --> Bus
    PatchBatch --> AsyncBus

    NSReg --> IR
    SchemaReg --> IR
    Bus --> IR
    AsyncBus --> IR
    Batch --> IR
    Dedup --> IR
    TXQueue --> IR
    Conflict --> IR
    Snap --> IR
```
