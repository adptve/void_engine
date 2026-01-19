# Void GUI v2 - Metaverse Operating System

## Executive Summary

Void GUI v2 is a **production-grade metaverse operating system kernel** written entirely in Rust. It implements a revolutionary architecture where the kernel never dies, all components are hot-swappable, and applications are completely isolated through capability-based security and namespace separation.

**Version:** 0.1.0
**License:** MIT OR Apache-2.0
**Crates:** 25 workspace members
**Platform:** Linux (primary), Windows, WASM

---

## What Makes This Project Unique

### 1. The Kernel Never Dies

Unlike traditional applications, Void GUI v2 is designed to **never crash**. Every potential failure point is wrapped in panic handlers, and a sophisticated supervision tree (borrowed from Erlang/OTP) automatically restarts failed components.

```mermaid
flowchart TD
    subgraph "Crash Containment"
        APP[App Code]
        CATCH[catch_unwind]
        SUPER[Supervisor]
        RESTART[Restart Strategy]
        LOG[Audit Log]
    end

    APP -->|panic!| CATCH
    CATCH -->|caught| SUPER
    SUPER --> RESTART
    RESTART -->|OneForOne| R1[Restart Failed Only]
    RESTART -->|OneForAll| R2[Restart All Children]
    RESTART -->|RestForOne| R3[Restart Failed + Later]
    CATCH --> LOG
```

### 2. Everything is Hot-Swappable

Every component can be replaced at runtime without stopping the system:

| Component | Hot-Swap Mechanism | Zero-Downtime |
|-----------|-------------------|---------------|
| **Plugins** | `void_core::hot_reload` | ✅ State preserved |
| **Assets** | File watching + `void_asset_server` | ✅ Atomic swap |
| **Shaders** | Naga recompilation | ✅ Frame-boundary swap |
| **Services** | Lifecycle API | ✅ Graceful restart |
| **Renderers** | Backend switching | ✅ Mid-frame capable |
| **Layers** | Layer composition | ✅ Instant |

### 3. Declarative State via IR Patches

Applications never mutate state directly. Instead, they emit **declarative patches** that the kernel validates and applies atomically:

```mermaid
sequenceDiagram
    participant App
    participant PatchBus
    participant Validator
    participant Kernel
    participant World

    App->>PatchBus: emit Patch
    PatchBus->>PatchBus: Queue by namespace
    Note over PatchBus: Frame boundary
    PatchBus->>Validator: Batch validate
    Validator->>Validator: Check capabilities
    Validator->>Validator: Check schema
    Validator->>Kernel: Valid transactions
    Kernel->>World: Apply atomically
    Kernel->>Kernel: Create snapshot
    Note over Kernel: Rollback available
```

### 4. seL4-Inspired Capability Security

Security isn't role-based—it's **capability-based**, inspired by the seL4 microkernel:

```mermaid
flowchart LR
    subgraph "Capability System"
        CAP[Capability Token]
        KIND[CapabilityKind]
        AUDIT[Audit Log]
    end

    CAP --> KIND
    KIND --> CE[CreateEntities]
    KIND --> MC[ModifyComponents]
    KIND --> LA[LoadAssets]
    KIND --> HS[HotSwap]
    KIND --> KA[KernelAdmin]

    CAP -->|every check| AUDIT
```

**Properties:**
- **Unforgeable**: Cannot be guessed or manufactured
- **Explicit**: All permissions must be granted
- **Revocable**: Can be revoked at any time
- **Auditable**: Every capability check is logged

### 5. Erlang-Style Supervision Trees

Borrowed from Erlang/OTP, the most battle-tested fault-tolerant system:

```mermaid
flowchart TD
    subgraph "Supervision Tree"
        ROOT[Root Supervisor<br/>Never crashes]
        S1[App Supervisor 1]
        S2[App Supervisor 2]
        A1[App A]
        A2[App B]
        A3[App C]
        A4[App D]
    end

    ROOT --> S1
    ROOT --> S2
    S1 --> A1
    S1 --> A2
    S2 --> A3
    S2 --> A4

    style ROOT fill:#4CAF50
    style S1 fill:#2196F3
    style S2 fill:#2196F3
```

---

## System Architecture

### High-Level Overview

```mermaid
flowchart TB
    subgraph "Entry Layer"
        RT[void_runtime<br/>Main Process]
    end

    subgraph "Interface Layer"
        SHELL[void_shell<br/>vsh REPL]
        EDITOR[void_editor<br/>Visual Editor]
    end

    subgraph "Orchestration Layer"
        KERNEL[void_kernel<br/>Never-Dying Kernel]
        ENGINE[void_engine<br/>App Framework]
    end

    subgraph "Communication Layer"
        IR[void_ir<br/>Patches & Transactions]
        EVENT[void_event<br/>Event Bus]
    end

    subgraph "Storage Layer"
        ECS[void_ecs<br/>Archetype Storage]
        ASSET[void_asset<br/>Asset Registry]
    end

    subgraph "Rendering Layer"
        RENDER[void_render<br/>Render Graph]
        SHADER[void_shader<br/>Naga Pipeline]
        UI[void_ui<br/>Immediate UI]
        PRES[void_presenter<br/>Output Adapters]
        COMP[void_compositor<br/>Wayland]
    end

    subgraph "Foundation Layer"
        CORE[void_core<br/>Plugin System]
        MATH[void_math<br/>Primitives]
        MEM[void_memory<br/>Allocators]
        STRUCT[void_structures<br/>Lock-free DS]
    end

    RT --> KERNEL
    RT --> SHELL
    RT --> EDITOR

    KERNEL --> IR
    KERNEL --> ECS
    KERNEL --> RENDER

    ENGINE --> IR
    ENGINE --> EVENT

    IR --> ECS

    RENDER --> SHADER
    RENDER --> PRES
    PRES --> COMP

    CORE --> MEM
    CORE --> STRUCT
    ECS --> CORE
    ASSET --> CORE
```

### Crate Dependency Graph

```mermaid
flowchart BT
    subgraph "Zero Dependencies"
        void_math[void_math]
        void_memory[void_memory]
    end

    subgraph "Core Foundation"
        void_structures[void_structures]
        void_core[void_core]
        void_event[void_event]
    end

    subgraph "Data Layer"
        void_ecs[void_ecs]
        void_ir[void_ir]
        void_asset[void_asset]
    end

    subgraph "Graphics"
        void_render[void_render]
        void_shader[void_shader]
        void_ui[void_ui]
        void_presenter[void_presenter]
        void_compositor[void_compositor]
    end

    subgraph "Runtime"
        void_engine[void_engine]
        void_kernel[void_kernel]
        void_runtime[void_runtime]
    end

    void_structures --> void_memory
    void_core --> void_structures
    void_event --> void_core

    void_ecs --> void_core
    void_ir --> void_ecs
    void_asset --> void_core

    void_render --> void_ir
    void_shader --> void_asset
    void_presenter --> void_render
    void_compositor --> void_presenter

    void_engine --> void_ir
    void_engine --> void_ecs
    void_kernel --> void_engine
    void_kernel --> void_render
    void_runtime --> void_kernel
```

---

## Core Systems Deep Dive

### Plugin System (void_core)

The foundation of extensibility—everything is a plugin:

```mermaid
stateDiagram-v2
    [*] --> Registered
    Registered --> Loading: load()
    Loading --> Active: on_load() success
    Loading --> Failed: on_load() error
    Active --> Unloading: unload()
    Active --> Active: on_config()
    Unloading --> Disabled: on_unload()
    Failed --> Loading: retry
    Disabled --> Loading: reload
    Disabled --> [*]
```

**Key Features:**
- Dynamic registration at runtime
- Hot-reload with state preservation
- Type registry for runtime introspection
- Versioned compatibility checking

### IR Patch System (void_ir)

The revolutionary declarative state management:

```mermaid
flowchart LR
    subgraph "Patch Types"
        EP[EntityPatch<br/>Create/Destroy/Enable]
        CP[ComponentPatch<br/>Set/Update/Remove]
        LP[LayerPatch<br/>Create/Update/Destroy]
        AP[AssetPatch<br/>Load/Unload/Update]
    end

    subgraph "Transaction"
        TX[Transaction<br/>Atomic Group]
    end

    subgraph "Validation"
        CAP[Capability Check]
        NS[Namespace Check]
        SCHEMA[Schema Check]
    end

    subgraph "Optimization"
        MERGE[Merge Patches]
        ELIM[Eliminate No-ops]
        ORDER[Reorder for Perf]
    end

    EP --> TX
    CP --> TX
    LP --> TX
    AP --> TX

    TX --> CAP
    CAP --> NS
    NS --> SCHEMA

    SCHEMA --> MERGE
    MERGE --> ELIM
    ELIM --> ORDER
```

**Transaction Properties:**
- ✅ Atomic: All-or-nothing
- ✅ Validated: Schema/capability checked
- ✅ Rollback-capable: Full snapshot support
- ✅ Conflict-aware: Concurrent modification detection

### Entity-Component System (void_ecs)

Archetype-based storage for cache-efficient iteration:

```mermaid
flowchart TB
    subgraph "World"
        CR[ComponentRegistry<br/>Type → ID]
        EA[EntityAllocator<br/>Generational IDs]
        RES[Resources<br/>Global Data]
    end

    subgraph "Archetypes"
        A1["Archetype [Pos, Vel]<br/>Entities: 1000"]
        A2["Archetype [Pos, Vel, Sprite]<br/>Entities: 500"]
        A3["Archetype [Pos, AI]<br/>Entities: 100"]
    end

    subgraph "Queries"
        Q1["Query<&Pos, &mut Vel>"]
        Q2["Query<&Pos, &Sprite>"]
    end

    CR --> A1
    CR --> A2
    CR --> A3

    Q1 --> A1
    Q1 --> A2
    Q2 --> A2
```

**Key Features:**
- Generational entity IDs (use-after-free prevention)
- Dynamic component registration
- Cache-efficient archetype storage
- Type-safe query system

### Kernel Architecture (void_kernel)

The never-dying heart of the system:

```mermaid
flowchart TB
    subgraph "Frame Loop"
        BF[begin_frame]
        PT[process_transactions]
        BRG[build_render_graph]
        EF[end_frame]
        GC[garbage_collect]
    end

    subgraph "Subsystems"
        SUPER[Supervision Tree]
        CAP[Capability Manager]
        NS[Namespace Manager]
        LAYER[Layer Manager]
        HOTSWAP[Hot-Swap Manager]
        WATCH[Watchdog]
    end

    BF --> PT
    PT --> BRG
    BRG --> EF
    EF --> GC
    GC --> BF

    PT --> SUPER
    PT --> CAP
    PT --> NS
    LAYER --> BRG
    HOTSWAP --> PT
    WATCH --> BF
```

### Rendering Pipeline

Multi-layer composition with crash isolation:

```mermaid
flowchart LR
    subgraph "App A"
        A1[Layer 1<br/>3D Content]
        A2[Layer 2<br/>UI Overlay]
    end

    subgraph "App B"
        B1[Layer 3<br/>Effects]
        B2[Layer 4<br/>HUD]
    end

    subgraph "Compositor"
        C[Layer<br/>Compositor]
    end

    subgraph "Presenters"
        P1[Desktop<br/>wgpu+winit]
        P2[DRM<br/>Direct GPU]
        P3[Wayland<br/>Smithay]
        P4[XR<br/>OpenXR]
    end

    A1 --> C
    A2 --> C
    B1 --> C
    B2 --> C

    C --> P1
    C --> P2
    C --> P3
    C --> P4
```

---

## Namespace Isolation Model

Apps are completely isolated through namespace separation:

```mermaid
flowchart TB
    subgraph "Kernel Namespace (0)"
        K_ENT[Kernel Entities]
        K_SYS[System Resources]
    end

    subgraph "App A Namespace (1)"
        A_ENT[App A Entities]
        A_RES[App A Resources]
        A_CAP[Capabilities:<br/>CreateEntities<br/>ModifyComponents]
    end

    subgraph "App B Namespace (2)"
        B_ENT[App B Entities]
        B_RES[App B Resources]
        B_CAP[Capabilities:<br/>CreateEntities<br/>LoadAssets]
    end

    K_ENT -.->|full access| A_ENT
    K_ENT -.->|full access| B_ENT

    A_ENT x--x|blocked| B_ENT
```

**Guarantees:**
- Apps can only modify their own entities
- Cross-namespace access requires explicit capability
- Entity IDs include namespace for isolation
- Kernel namespace has full access

---

## Resource Budget System

Apps are sandboxed with strict resource limits:

```mermaid
pie title "Resource Budget Example"
    "Memory (512MB)" : 512
    "GPU Memory (256MB)" : 256
    "Entities (10K)" : 100
    "Layers (8)" : 8
    "Draw Calls (1K)" : 100
```

| Resource | Limit | Enforcement |
|----------|-------|-------------|
| Memory | `max_memory_bytes` | Tracked per-allocation |
| GPU Memory | `max_gpu_memory_bytes` | Texture/buffer limits |
| Entities | `max_entities` | Entity creation blocked |
| Layers | `max_layers` | Layer creation blocked |
| Frame Time | `max_frame_time_us` | Watchdog termination |
| Patches/Frame | `max_patches_per_frame` | Patch dropped |
| Draw Calls | `max_draw_calls` | Culling enforced |

---

## Hot-Swap Deep Dive

### State Preservation Flow

```mermaid
sequenceDiagram
    participant Kernel
    participant HotSwap
    participant OldModule
    participant NewModule
    participant State

    Kernel->>HotSwap: hot_swap(ModuleId)
    HotSwap->>OldModule: snapshot()
    OldModule->>State: Serialize state
    HotSwap->>OldModule: unload()
    HotSwap->>NewModule: load()
    NewModule->>State: Deserialize state
    HotSwap->>NewModule: restore(state)

    alt Success
        HotSwap->>Kernel: SwapComplete
    else Failure
        HotSwap->>OldModule: restore(snapshot)
        HotSwap->>Kernel: SwapFailed, Rolled Back
    end
```

### Shader Hot-Reload

```mermaid
flowchart LR
    subgraph "File System"
        SRC[shader.wgsl]
    end

    subgraph "Asset Server"
        WATCH[File Watcher<br/>notify]
        LOAD[Asset Loader]
    end

    subgraph "Shader Pipeline"
        NAGA[Naga<br/>Validation]
        COMPILE[wgpu<br/>Compilation]
        CACHE[Shader Cache]
    end

    SRC -->|change| WATCH
    WATCH --> LOAD
    LOAD --> NAGA
    NAGA -->|valid| COMPILE
    COMPILE --> CACHE
    CACHE -->|frame boundary| SWAP[Atomic Swap]
```

---

## Service Architecture

```mermaid
stateDiagram-v2
    [*] --> Stopped
    Stopped --> Starting: start()
    Starting --> Running: init complete
    Starting --> Failed: init error
    Running --> Stopping: stop()
    Running --> Running: handle_request()
    Stopping --> Stopped: cleanup done
    Failed --> Starting: retry
```

**Built-in Services:**
- `AssetService`: Loading/caching with hot-reload
- `SessionService`: User session management
- `AudioService`: Audio playback (rodio backend)
- `EventBus`: Inter-service event routing
- `NetworkService`: WebSocket communication

---

## External Dependencies

### Minimal Core Philosophy

The core crates have **zero external dependencies**:

| Crate | External Deps |
|-------|---------------|
| void_core | 0 |
| void_math | 0 |
| void_memory | 0 |
| void_structures | parking_lot only |

### Carefully Selected Dependencies

| Category | Crate | Purpose |
|----------|-------|---------|
| Serialization | serde, bincode | State serialization |
| Concurrency | parking_lot, crossbeam | Locks, channels |
| Graphics | wgpu, winit, naga | GPU abstraction |
| UI | egui, glyphon | Immediate-mode UI |
| Async | tokio | Service runtime |
| Files | notify | Hot-reload watching |

---

## Feature Flags

| Feature | Description | Default |
|---------|-------------|---------|
| `hot-reload` | Enable hot-reload support | ✅ |
| `smithay-compositor` | Full Wayland compositor | ❌ |
| `xr` | OpenXR VR/AR support | ❌ |
| `audio-backend` | Audio via rodio | ❌ |
| `drm-backend` | Direct GPU rendering | ❌ |

---

## Configuration Reference

### Kernel Configuration

```rust
pub struct KernelConfig {
    pub target_fps: u32,              // Default: 60
    pub fixed_timestep: f32,          // Default: 1/60
    pub max_delta_time: f32,          // Default: 0.25
    pub hot_reload: bool,             // Default: true
    pub rollback_frames: u32,         // Default: 3
    pub max_apps: u32,                // Default: 64
    pub max_layers: u32,              // Default: 128
    pub enable_watchdog: bool,        // Default: true
}
```

### Boot Configuration

```rust
pub enum Backend {
    Smithay,  // DRM/KMS Wayland compositor
    Winit,    // Window on existing display
    Xr,       // OpenXR VR/AR
    Cli,      // Command-line only
    Auto,     // Auto-detect
}
```

---

## Queryable Code Index

### By Pattern

| Pattern | Primary Crate | Key File |
|---------|---------------|----------|
| Plugin System | void_core | `src/plugin.rs` |
| Type Registry | void_core | `src/registry.rs` |
| Hot Reload | void_core | `src/hot_reload.rs` |
| ECS World | void_ecs | `src/world.rs` |
| Archetypes | void_ecs | `src/archetype.rs` |
| IR Patches | void_ir | `src/patch.rs` |
| Transactions | void_ir | `src/transaction.rs` |
| Patch Bus | void_ir | `src/bus.rs` |
| Supervision | void_kernel | `src/supervision.rs` |
| Capabilities | void_kernel | `src/capability.rs` |
| Recovery | void_kernel | `src/recovery.rs` |
| Sandboxing | void_kernel | `src/sandbox.rs` |
| Render Graph | void_render | `src/graph.rs` |
| Layer Composition | void_render | `src/compositor.rs` |
| Shader Pipeline | void_shader | `src/pipeline.rs` |
| Presenters | void_presenter | `src/lib.rs` |

### By Trait

| Trait | Crate | Purpose |
|-------|-------|---------|
| `Plugin` | void_core | Pluggable components |
| `HotReloadable` | void_core | Hot-swap support |
| `Component` | void_ecs | ECS components |
| `Bundle` | void_ecs | Component groups |
| `Service` | void_services | Async services |
| `Presenter` | void_presenter | Output adapters |

### By Enum

| Enum | Crate | Purpose |
|------|-------|---------|
| `PluginStatus` | void_core | Plugin lifecycle |
| `PatchKind` | void_ir | Patch types |
| `RestartStrategy` | void_kernel | Supervisor strategy |
| `CapabilityKind` | void_kernel | Permission types |
| `RecoveryResult` | void_kernel | Recovery outcomes |
| `LayerType` | void_kernel | Render layer types |
| `Backend` | void_runtime | Output backends |

---

## Testing Strategy

```bash
# Full workspace test
cargo test --workspace

# Single crate
cargo test -p void_kernel

# With features
cargo test -p void_runtime --features "hot-reload"

# Doc tests
cargo test --doc --workspace
```

---

## Build Profiles

```toml
[profile.dev]
opt-level = 0
debug = true

[profile.release]
opt-level = 3
lto = true
codegen-units = 1

[profile.release-with-debug]
inherits = "release"
debug = true
```

---

## Versioning

All crates share the same version and are released together:
- Current: `0.1.0`
- SemVer: Breaking changes bump major version
- Changelog: See `CHANGELOG.md`

---

## License

Dual-licensed under:
- MIT License
- Apache License 2.0

Choose whichever license works best for your use case.
