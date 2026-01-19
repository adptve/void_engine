# Void GUI v2 - App System Architecture

## Overview

The App system in Void GUI v2 is the primary unit of isolation and execution. Unlike traditional GUI frameworks where "apps" might simply be scenes or views, Void's Apps are **fully isolated process-like entities** with their own namespaces, capabilities, resource budgets, and lifecycle management. The architecture draws inspiration from microkernel designs (seL4), actor systems (Erlang OTP), and modern container orchestration.

---

## 1. What Apps Actually Are

Apps in Void Engine are **isolated modular units** that:

1. **Run in their own namespace** - Each app gets a unique `NamespaceId` that scopes all its operations
2. **Own their own layers** - Apps render to layers they create and own (can have multiple)
3. **Have explicit capabilities** - All permissions are granted via unforgeable capability tokens
4. **Are resource-constrained** - Memory, entities, layers, and CPU time are budgeted
5. **Are supervised** - Part of an Erlang-style supervision tree for fault tolerance
6. **Are sandboxed** - Crash containment prevents one app from affecting others

### Core Data Structures

```rust
// From crates/void_kernel/src/app.rs
pub struct App {
    pub id: AppId,                      // Unique identifier (atomic counter)
    pub manifest: AppManifest,          // Configuration from package
    pub namespace_id: NamespaceId,      // Isolation boundary
    pub state: AppState,                // Current lifecycle state
    pub loaded_at: Instant,             // When loaded
    pub last_active: Instant,           // Last activity timestamp
    pub error: Option<String>,          // Error if failed
    pub layers: Vec<LayerId>,           // Owned layers
    pub metrics: AppMetrics,            // Performance metrics
}
```

### App Manifest

The manifest defines what an app needs and what permissions it requests:

```rust
pub struct AppManifest {
    pub name: String,
    pub version: String,
    pub description: Option<String>,
    pub author: Option<String>,
    pub layers: Vec<LayerRequest>,       // Requested layers
    pub resources: ResourceRequirements, // Resource limits
    pub permissions: AppPermissions,     // Requested permissions
}
```

---

## 2. App Creation Flow

```mermaid
sequenceDiagram
    participant Client
    participant Daemon as KernelDaemon
    participant AppMgr as AppManager
    participant NsMgr as NamespaceManager
    participant CapChk as CapabilityChecker
    participant SuperTree as SupervisorTree

    Client->>Daemon: RegisterApp(registration)
    Daemon->>AppMgr: load(manifest)

    Note over AppMgr: Check app limit
    AppMgr->>NsMgr: Create namespace
    NsMgr-->>AppMgr: NamespaceId

    Note over AppMgr: Create App with Loading state
    AppMgr->>CapChk: Apply permission restrictions
    AppMgr->>CapChk: Apply resource limits
    AppMgr-->>Daemon: AppId

    Daemon->>CapChk: grant_default_app_capabilities
    Daemon->>CapChk: grant requested capabilities

    Daemon->>AppMgr: initialize(app_id)
    Note over AppMgr: State: Loading -> Initializing
    Note over AppMgr: Create layers, load scripts
    Note over AppMgr: State: Initializing -> Running

    Daemon->>SuperTree: add_app(app_id, supervisor_id)
    SuperTree->>SuperTree: mark_app_running(app_id)

    Daemon-->>Client: AppRegistered { app_id, namespace_id, capabilities }
```

### Key Steps in App Creation

| Step | Description | Location |
|------|-------------|----------|
| 1. Manifest Validation | Validate required fields | `app.rs:227` |
| 2. Namespace Creation | Assign unique `NamespaceId` | `app.rs:233` |
| 3. Permission Mapping | Translate manifest permissions | `app.rs:237` |
| 4. Resource Limits | Apply limits to namespace | `app.rs:242-245` |
| 5. App Instantiation | Create app in `Loading` state | `app.rs:251` |
| 6. Capability Granting | Grant default + requested capabilities | `daemon.rs:575` |
| 7. Initialization | Transition to `Running` | `app.rs:262` |
| 8. Supervision Registration | Add to supervision tree | `daemon.rs:539` |

---

## 3. App Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> Loading: load()
    Loading --> Initializing: initialize()
    Initializing --> Running: init complete
    Initializing --> Failed: init error
    Running --> Paused: pause()
    Paused --> Running: start()/resume()
    Running --> Unloading: unload()
    Paused --> Unloading: unload()
    Unloading --> Stopped: cleanup complete
    Loading --> Failed: load error
    Running --> Failed: crash
    Failed --> [*]: gc()
    Stopped --> [*]: gc()
```

### State Definitions

| State | Description |
|-------|-------------|
| `Loading` | App manifest parsed, namespace created, waiting for initialization |
| `Initializing` | Creating layers, loading scripts/WASM, calling app's init function |
| `Running` | App is active, processing frames, emitting patches |
| `Paused` | App is temporarily suspended but resources retained |
| `Unloading` | Destroying entities, layers, releasing namespace |
| `Stopped` | App has cleanly terminated, awaiting GC |
| `Failed` | App encountered an unrecoverable error |

### State Transition Code

```rust
// From app.rs
pub fn start(&mut self, id: AppId) -> Result<(), AppError> {
    let app = self.apps.get_mut(&id).ok_or(AppError::NotFound(id))?;
    match app.state {
        AppState::Paused | AppState::Initializing => {
            app.set_state(AppState::Running);
            Ok(())
        }
        AppState::Running => Ok(()), // Already running
        _ => Err(AppError::InvalidState),
    }
}
```

---

## 4. App Loading/Unloading Flow

```mermaid
flowchart TD
    subgraph "Load Phase"
        L1[Parse Package Manifest] --> L2[Validate Resources]
        L2 --> L3[Create Namespace]
        L3 --> L4[Apply Permission Restrictions]
        L4 --> L5[Apply Resource Limits]
        L5 --> L6[Create App Instance]
        L6 --> L7[Grant Capabilities]
        L7 --> L8[State: Loading]
    end

    subgraph "Initialize Phase"
        I1[Create Requested Layers] --> I2[Load Scripts/WASM]
        I2 --> I3[Call App Init Function]
        I3 --> I4[State: Initializing -> Running]
    end

    subgraph "Unload Phase"
        U1[State: Running/Paused -> Unloading] --> U2[Destroy All Entities]
        U2 --> U3[Destroy All Layers]
        U3 --> U4[Unload Scripts/WASM]
        U4 --> U5[Release Namespace]
        U5 --> U6[Revoke Capabilities]
        U6 --> U7[State: Unloading -> Stopped]
    end

    L8 --> I1
    I4 --> Running[App Running]
    Running --> U1
```

### Loading Implementation

```rust
// From app.rs - AppManager::load()
pub fn load(&mut self, manifest: AppManifest) -> Result<AppId, AppError> {
    if self.apps.len() >= self.max_apps {
        return Err(AppError::TooManyApps);
    }

    // Create namespace for this app
    let mut namespace = Namespace::new(&manifest.name);

    // Apply permission restrictions
    if !manifest.permissions.cross_app_read {
        namespace.permissions.cross_namespace_read = false;
    }

    // Apply resource limits
    if let Some(max_entities) = manifest.resources.max_entities {
        namespace.limits.max_entities = Some(max_entities);
    }
    if let Some(max_layers) = manifest.resources.max_layers {
        namespace.limits.max_layers = Some(max_layers);
    }

    let namespace_id = namespace.id;
    let handle = self.patch_bus.register_namespace(namespace);

    let app = App::new(manifest, namespace_id);
    let id = app.id;

    self.apps.insert(id, app);
    self.handles.insert(id, handle);

    Ok(id)
}
```

### Unloading Implementation

```rust
// From app.rs - AppManager::unload()
pub fn unload(&mut self, id: AppId) -> Result<(), AppError> {
    let app = self.apps.get_mut(&id).ok_or(AppError::NotFound(id))?;
    app.set_state(AppState::Unloading);

    // 1. Destroy all app's entities
    // 2. Destroy all app's layers
    // 3. Unload app's scripts/WASM
    // 4. Release namespace

    app.set_state(AppState::Stopped);
    self.handles.remove(&id);
    Ok(())
}
```

---

## 5. Layer Ownership

```mermaid
flowchart TB
    subgraph App1["App (AppId=1)"]
        subgraph Namespace1["Namespace (ns:1)"]
            L1[Layer: content<br/>Priority: 0<br/>Type: Content]
            L2[Layer: ui<br/>Priority: 10<br/>Type: Overlay]
            L3[Layer: effects<br/>Priority: 5<br/>Type: Effect]
        end
    end

    subgraph App2["App (AppId=2)"]
        subgraph Namespace2["Namespace (ns:2)"]
            L4[Layer: world<br/>Priority: 0<br/>Type: Content]
            L5[Layer: overlay<br/>Priority: 20<br/>Type: Overlay]
        end
    end

    subgraph Compositor["Layer Compositor"]
        SortedLayers["Sorted by Priority:<br/>L1(0), L4(0), L3(5), L2(10), L5(20)"]
    end

    L1 --> Compositor
    L2 --> Compositor
    L3 --> Compositor
    L4 --> Compositor
    L5 --> Compositor

    Compositor --> Output[Final Composited Output]
```

### Layer-App Relationship

Each layer is owned by exactly one namespace (and thus one app):

```rust
// From layer.rs
pub struct Layer {
    pub id: LayerId,
    pub name: String,
    pub owner: NamespaceId,  // <-- Ownership binding
    pub config: LayerConfig,
    pub dirty: bool,
    pub last_rendered_frame: u64,
}
```

### Layer Types

```rust
pub enum LayerType {
    Content,  // 3D world content
    Effect,   // Post-processing effects
    Overlay,  // UI overlays
    Portal,   // 3D portal surfaces
}
```

### Layer Manager Tracking

```rust
pub struct LayerManager {
    layers: HashMap<LayerId, Layer>,
    by_namespace: HashMap<NamespaceId, Vec<LayerId>>,  // Per-namespace index
    sorted_layers: Vec<LayerId>,                        // Priority-sorted
}
```

### Layer Destruction on App Unload

```rust
pub fn destroy_namespace_layers(&mut self, namespace: NamespaceId) {
    if let Some(layer_ids) = self.by_namespace.remove(&namespace) {
        for id in layer_ids {
            self.layers.remove(&id);
            self.sorted_layers.retain(|&lid| lid != id);
        }
        log::debug!("Destroyed all layers for namespace {}", namespace);
    }
}
```

---

## 6. Namespace Isolation Model

```mermaid
flowchart TB
    subgraph Kernel["Kernel (NamespaceId::KERNEL = 0)"]
        KernelNS["Full Access to All Resources"]
    end

    subgraph App1["App 1 (ns:1)"]
        NS1["Namespace"]
        E1["Entities 1-100"]
        L1["Layers 1-3"]
        C1["Components"]
    end

    subgraph App2["App 2 (ns:2)"]
        NS2["Namespace"]
        E2["Entities 101-200"]
        L2["Layers 4-5"]
        C2["Components"]
    end

    Kernel --> |"Full Access"| App1
    Kernel --> |"Full Access"| App2

    NS1 --> |"Owns"| E1
    NS1 --> |"Owns"| L1
    NS2 --> |"Owns"| E2
    NS2 --> |"Owns"| L2

    App1 -.-> |"Read if Exported"| E2
    App2 -.-> |"Read if Exported"| E1
    App1 -.- |"Cannot Modify"| E2
    App2 -.- |"Cannot Modify"| E1
```

### Security Properties

1. **Apps cannot forge namespace IDs** - IDs are generated atomically
2. **Apps can only modify entities in their own namespace**
3. **Cross-namespace reads require explicit capability**
4. **Resource limits are enforced per-namespace**

### Access Control Implementation

```rust
// From namespace.rs
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

    // Cross-namespace access - check exports and capabilities
    if write {
        return NamespaceAccess::Denied(NamespaceError::CrossNamespaceWrite);
    }
    // Check if entity is exported...
}
```

### Entity Export System

Apps can selectively export entities for cross-namespace access:

```rust
pub struct EntityExport {
    pub local_id: u64,
    pub readable_components: Vec<String>,
    pub writable_components: Vec<String>,
    pub access: ExportAccess,
}

pub enum ExportAccess {
    Public,                           // Any namespace can access
    Allowlist(Vec<NamespaceId>),     // Only specific namespaces
    CapabilityRequired(String),       // Requires a specific capability
}
```

---

## 7. Capability System

```mermaid
flowchart LR
    subgraph "Capability Types"
        CE["CreateEntities<br/>max: 10000"]
        DE["DestroyEntities"]
        MC["ModifyComponents"]
        CL["CreateLayers<br/>max: 8"]
        LA["LoadAssets"]
        AN["AccessNetwork"]
        CNR["CrossNamespaceRead"]
        HS["HotSwap<br/>ADMIN ONLY"]
        KA["KernelAdmin<br/>ADMIN ONLY"]
    end

    subgraph App["App Namespace (ns:1)"]
        AppIcon["App"]
    end

    subgraph Kernel["Kernel (ns:0)"]
        Grantor["Capability Grantor"]
    end

    Grantor --> |"Grants"| CE
    Grantor --> |"Grants"| DE
    Grantor --> |"Grants"| MC
    Grantor --> |"Grants"| CL
    Grantor --> |"Grants"| LA

    CE --> AppIcon
    DE --> AppIcon
    MC --> AppIcon
    CL --> AppIcon
    LA --> AppIcon

    AN -.-> |"Not Granted"| AppIcon
    CNR -.-> |"Not Granted"| AppIcon
    HS -.- |"Never for Apps"| AppIcon
    KA -.- |"Never for Apps"| AppIcon
```

### Capability Structure

```rust
pub struct Capability {
    pub id: CapabilityId,           // Unforgeable token
    pub kind: CapabilityKind,       // What it permits
    pub holder: NamespaceId,        // Who holds it
    pub grantor: NamespaceId,       // Who granted it
    pub created_at: Instant,        // When created
    pub expires_at: Option<Instant>, // Optional expiration
    pub delegable: bool,            // Can be passed to others
    pub reason: Option<String>,     // For auditing
}
```

### Capability Types

```rust
pub enum CapabilityKind {
    CreateEntities { max: Option<u32> },
    DestroyEntities,
    ModifyComponents { allowed_types: Option<Vec<String>> },
    CreateLayers { max: Option<u32> },
    ModifyLayers { allowed_layers: Option<Vec<LayerId>> },
    LoadAssets { allowed_paths: Option<Vec<String>> },
    AccessNetwork { allowed_hosts: Option<Vec<String>> },
    AccessFilesystem { allowed_paths: Option<Vec<String>> },
    CrossNamespaceRead { allowed_namespaces: Option<Vec<NamespaceId>> },
    ExecuteScripts,
    HotSwap,              // Admin only
    ManageCapabilities,   // Admin only
    KernelAdmin,          // Admin only
}
```

### Default App Capabilities

```rust
pub fn grant_default_app_capabilities(&mut self, namespace: NamespaceId, grantor: NamespaceId) {
    self.grant(Capability::new(
        CapabilityKind::CreateEntities { max: Some(10000) },
        namespace, grantor,
    ));
    self.grant(Capability::new(
        CapabilityKind::DestroyEntities,
        namespace, grantor,
    ));
    self.grant(Capability::new(
        CapabilityKind::ModifyComponents { allowed_types: None },
        namespace, grantor,
    ));
    self.grant(Capability::new(
        CapabilityKind::CreateLayers { max: Some(8) },
        namespace, grantor,
    ));
    self.grant(Capability::new(
        CapabilityKind::LoadAssets { allowed_paths: None },
        namespace, grantor,
    ));
}
```

---

## 8. Resource Budgets and Sandboxing

```mermaid
flowchart TB
    subgraph Sandbox["AppSandbox"]
        Budget["ResourceBudget"]
        Usage["ResourceUsage"]
        Audit["AuditLog"]
        Owned["OwnedResources"]
    end

    Budget --> MaxMem["max_memory_bytes: 256MB"]
    Budget --> MaxGPU["max_gpu_memory_bytes: 512MB"]
    Budget --> MaxEnt["max_entities: 10,000"]
    Budget --> MaxLay["max_layers: 8"]
    Budget --> MaxFT["max_frame_time_us: 16,000"]
    Budget --> MaxPatch["max_patches_per_frame: 1000"]

    Usage --> CurMem["memory_bytes (tracked)"]
    Usage --> CurEnt["entities (tracked)"]
    Usage --> CurLay["layers (tracked)"]
    Usage --> FrameTime["frame_time_us (tracked)"]

    Owned --> |"Cleanup on crash"| Entities["Entities"]
    Owned --> |"Cleanup on crash"| Layers["Layers"]
    Owned --> |"Cleanup on crash"| Assets["Assets"]
```

### ResourceBudget Structure

```rust
pub struct ResourceBudget {
    pub max_memory_bytes: u64,           // Default: 256 MB
    pub max_gpu_memory_bytes: u64,       // Default: 512 MB
    pub max_entities: u32,               // Default: 10,000
    pub max_layers: u32,                 // Default: 8
    pub max_assets: u32,                 // Default: 1000
    pub max_frame_time_us: u64,          // Default: 16,000 (~16ms)
    pub max_patches_per_frame: u32,      // Default: 1000
    pub max_draw_calls: u32,             // Default: 1000
    pub max_compute_dispatches: u32,     // Default: 100
}
```

### Crash Containment

```rust
pub fn execute<F, R>(&mut self, f: F) -> Result<R, SandboxError>
where
    F: FnOnce() -> R + panic::UnwindSafe,
{
    let start = Instant::now();
    let result = panic::catch_unwind(f);

    match result {
        Ok(value) => {
            if elapsed.as_micros() as u64 > self.budget.max_frame_time_us {
                log::warn!("Sandbox exceeded time budget");
            }
            Ok(value)
        }
        Err(panic_payload) => {
            self.record_crash(&message);
            Err(SandboxError::Panic { ... })
        }
    }
}
```

### Crash Limits

```rust
pub fn exceeded_crash_limit(&self) -> bool {
    self.crash_count >= self.max_crashes  // Default: 3
}
```

---

## 9. Supervision Integration

```mermaid
flowchart TB
    subgraph SupervisionTree
        Root["Root Supervisor<br/>Strategy: OneForOne"]

        Root --> AppSup["App Supervisor<br/>Strategy: OneForOne"]
        Root --> SvcSup["Service Supervisor<br/>Strategy: OneForAll"]

        AppSup --> App1["App 1<br/>Type: Permanent"]
        AppSup --> App2["App 2<br/>Type: Transient"]
        AppSup --> App3["App 3<br/>Type: Temporary"]

        SvcSup --> Svc1["Service 1"]
        SvcSup --> Svc2["Service 2"]
    end
```

### Restart Strategies

```rust
pub enum RestartStrategy {
    OneForOne,  // Restart only the failed child
    OneForAll,  // Restart all children when one fails
    RestForOne, // Restart failed child and all started after it
}
```

### Child Types

```rust
pub enum ChildType {
    Permanent,  // Always restart
    Temporary,  // Never restart
    Transient,  // Restart only if abnormal exit
}
```

### Restart Intensity

```rust
pub struct RestartIntensity {
    pub max_restarts: u32,      // Default: 5
    pub window_secs: u32,       // Default: 60s
    pub backoff: BackoffConfig,
}

pub struct BackoffConfig {
    pub initial_delay_ms: u64,  // Default: 100ms
    pub max_delay_ms: u64,      // Default: 30,000ms
    pub multiplier: f32,        // Default: 2.0
}
```

### Failure Handling Flow

```mermaid
sequenceDiagram
    participant App
    participant Sandbox as AppSandbox
    participant Recovery as RecoveryManager
    participant Supervisor as SupervisorTree
    participant Kernel

    App->>Sandbox: Execute callback
    Sandbox->>Sandbox: catch_unwind(callback)
    Note over Sandbox: Panic occurs!

    Sandbox->>Recovery: record_crash(message)
    Recovery->>Supervisor: report_app_failure(app_id, error, false)

    alt Restart Allowed
        Supervisor->>Supervisor: Check intensity limits
        Supervisor->>Supervisor: Calculate backoff delay
        Supervisor-->>Recovery: SupervisorAction::Restart([app_id])
        Recovery->>Kernel: Schedule restart after backoff
    else Intensity Exceeded
        Supervisor-->>Recovery: SupervisorAction::Escalate
        Recovery->>Supervisor: escalate(supervisor_id)
        alt Has Parent
            Supervisor->>Supervisor: Report to parent
        else Root Supervisor
            Supervisor-->>Recovery: SupervisorAction::Shutdown
            Recovery->>Kernel: emergency_shutdown = true
        end
    end
```

---

## 10. Package Format

Apps are distributed as `.mvp` (Metaverse Package) files:

```
myapp.mvp (ZIP archive)
├── manifest.toml      # App manifest
├── icon.png           # App icon (optional)
├── assets/            # Asset files
│   ├── models/
│   ├── textures/
│   └── audio/
├── scripts/           # VoidScript files
│   └── main.vs
├── wasm/              # WASM modules (optional)
│   └── logic.wasm
└── signature          # Package signature (optional)
```

### Package Manifest (TOML)

```toml
[package]
name = "my-app"
version = "1.0.0"
author = "Developer"
description = "My awesome app"

[app]
entry = "scripts/main.vs"

[[layers]]
name = "content"
type = "content"
priority = 0

[[layers]]
name = "ui"
type = "overlay"
priority = 100

[resources]
max_entities = 5000
max_layers = 4
max_memory_mb = 128

[permissions]
cross_app_read = false
network_access = false
filesystem_access = false
```

---

## Key Design Principles

| Principle | Implementation |
|-----------|----------------|
| **Kernel Never Dies** | All app failures contained via `catch_unwind` |
| **Apps Are Untrusted** | Capability checks on every operation |
| **Declarative State** | Changes only via IR patches |
| **Layer Isolation** | Failed shader = black layer, not crash |
| **Hot-Swap Everything** | No restart ever required |
| **Mandatory Rollback** | Last-known-good always available |
| **Resource Budgets** | Every app is sandboxed |
| **Supervision Trees** | Automatic restart with backoff |

---

## Critical Source Files

| File | Purpose |
|------|---------|
| `crates/void_kernel/src/app.rs` | Core App struct, AppManager, lifecycle |
| `crates/void_kernel/src/layer.rs` | Layer ownership, LayerManager |
| `crates/void_kernel/src/namespace.rs` | Namespace isolation, access control |
| `crates/void_kernel/src/capability.rs` | Capability system, permissions |
| `crates/void_kernel/src/sandbox.rs` | ResourceBudget, crash containment |
| `crates/void_kernel/src/supervisor.rs` | Supervision tree, restart strategies |
| `crates/void_kernel/src/daemon.rs` | App registration, lifecycle orchestration |
| `crates/void_kernel/src/package.rs` | Package parsing, manifest loading |
