# Void Engine - Rust to C++ Migration Gap Analysis

This document identifies features from the **Rust legacy crates** (`legacy/crates/`) that must be migrated to the **modern C++ implementation** (`void_engine/`). The Rust implementation is being deprecated in favor of the C++ codebase.

---

## Migration Direction

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                      │
│   RUST LEGACY CRATES              C++ MODERN IMPLEMENTATION         │
│   (legacy/crates/)      ────►     (void_engine/)                    │
│                                                                      │
│   Being deprecated                Target platform                    │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

**Goal:** Ensure 100% feature parity by migrating all Rust features to C++.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Migration Status Overview](#2-migration-status-overview)
3. [Critical Features to Migrate](#3-critical-features-to-migrate)
4. [High Priority Features](#4-high-priority-features)
5. [Medium Priority Features](#5-medium-priority-features)
6. [Low Priority Features](#6-low-priority-features)
7. [Architecture Adaptations](#7-architecture-adaptations)
8. [Migration Checklist](#8-migration-checklist)
9. [Implementation Guidelines](#9-implementation-guidelines)

---

## 1. Executive Summary

### Current State

| Aspect | C++ (Target) | Rust (Legacy) | Migration Status |
|--------|--------------|---------------|------------------|
| Language | C++20 | Rust 2021 | C++ is target |
| Module Count | 31 modules | 33 crates | Feature parity needed |
| Plugin System | Subsystem-based | Trait-based plugins | **Needs expansion** |
| State Updates | Direct mutation | IR patch transactions | **Needs implementation** |
| Render Graph | Implicit passes | Explicit RenderGraph | **Needs implementation** |
| Fault Tolerance | Basic | Supervisor/Watchdog | **Needs implementation** |
| Platform Output | GLFW/OpenGL | wgpu multi-backend | **Needs abstraction** |

### Features Requiring Migration (Rust → C++)

| Priority | Feature | Rust Source | Effort |
|----------|---------|-------------|--------|
| **CRITICAL** | IR Patch/Transaction System | `void_ir` | High |
| **CRITICAL** | Kernel Orchestration | `void_kernel` | High |
| **HIGH** | Supervisor/Watchdog | `void_kernel` | Medium |
| **HIGH** | Explicit Render Graph | `void_render` | Medium |
| **HIGH** | Layer-Based Compositing | `void_render` | Medium |
| **HIGH** | Multi-Backend Rendering | `void_presenter` | High |
| **MEDIUM** | Extended Plugin Lifecycle | `void_engine` | Medium |
| **MEDIUM** | XR Stereo Pipeline | `void_xr` | Medium |
| **MEDIUM** | Visual Scripting (Full) | `void_graph` | Medium |
| **LOW** | Full Hand Tracking | `void_xr` | Low |
| **LOW** | Shell (vsh) | `void_shell` | Low |

### Features Already Complete in C++

These features exist in C++ and do **not** need migration:
- Scene File Loading (TOML parser)
- Scene Instantiator
- Live Scene Manager with hot-reload
- Animation System
- ECS Scene Bridge
- 3-Tier Asset Cache
- Full PBR Materials
- Character Controller
- Frame Timing Statistics

---

## 2. Migration Status Overview

```
┌────────────────────────────────────────────────────────────────────┐
│                     MIGRATION STATUS MATRIX                         │
├────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Feature                    C++         Rust        Status         │
│  ─────────────────────────────────────────────────────────────      │
│  Scene File Loading         ✓           -           COMPLETE       │
│  Scene Hot-Reload           ✓           -           COMPLETE       │
│  Animation System           ✓           -           COMPLETE       │
│  ECS World                  ✓           ✓           COMPLETE       │
│  Event Bus                  ✓           ✓           COMPLETE       │
│  Asset Server               ✓           ✓           COMPLETE       │
│  3-Tier Asset Cache         ✓           -           COMPLETE       │
│  PBR Materials              ✓           △           COMPLETE       │
│  Physics                    ✓           ✓           COMPLETE       │
│  Services                   ✓           ✓           COMPLETE       │
│  VRR/HDR                    ✓           ✓           COMPLETE       │
│  ─────────────────────────────────────────────────────────────      │
│  IR Patch System            ✗           ✓           TO MIGRATE     │
│  Kernel Orchestration       ✗           ✓           TO MIGRATE     │
│  Supervisor/Watchdog        ✗           ✓           TO MIGRATE     │
│  Explicit Render Graph      ✗           ✓           TO MIGRATE     │
│  Layer Compositing          ✗           ✓           TO MIGRATE     │
│  Multi-Backend Render       ✗           ✓           TO MIGRATE     │
│  Extended Lifecycle Hooks   ✗           ✓           TO MIGRATE     │
│  XR Stereo Pipeline         △           ✓           TO MIGRATE     │
│  Visual Scripting (Full)    △           ✓           TO MIGRATE     │
│  Full Hand Tracking         ✗           ✓           TO MIGRATE     │
│  Shell (vsh)                ✗           ✓           TO MIGRATE     │
│                                                                     │
│  Legend: ✓ = Complete, △ = Partial, ✗ = Missing, - = N/A          │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

---

## 3. Critical Features to Migrate

### 3.1 IR Patch/Transaction System

**Source:** `legacy/crates/void_ir/`

**What it provides:**
- Atomic state updates via patches
- Transaction batching for consistency
- Namespace-based organization (ecs, render, asset, physics)
- Debuggable change history

**Rust Implementation:**
```rust
pub struct Patch {
    pub namespace: Namespace,
    pub op: PatchOp,
    pub target: PatchTarget,
    pub data: PatchData,
}

pub enum PatchOp {
    Create,
    Update,
    Delete,
    Transform,
}

pub struct Transaction {
    pub id: TransactionId,
    pub patches: Vec<Patch>,
    pub timestamp: Instant,
}

pub struct PatchBus {
    pending: Vec<Patch>,
    transactions: Vec<Transaction>,
}
```

**Required C++ Implementation:**
```cpp
// include/void_engine/ir/patch.hpp

namespace void_ir {

enum class PatchOp {
    Create,
    Update,
    Delete,
    Transform
};

enum class Namespace {
    Ecs,
    Render,
    Asset,
    Physics
};

struct Patch {
    Namespace namespace_id;
    PatchOp op;
    PatchTarget target;
    PatchData data;
};

struct Transaction {
    uint64_t id;
    std::vector<Patch> patches;
    std::chrono::steady_clock::time_point timestamp;
};

class PatchBus {
public:
    void emit(Patch patch);
    Transaction commit();
    void apply(World& world, const Transaction& txn);

private:
    std::vector<Patch> m_pending;
    std::vector<Transaction> m_transactions;
};

} // namespace void_ir
```

**Files to create:**
- `include/void_engine/ir/patch.hpp`
- `include/void_engine/ir/transaction.hpp`
- `include/void_engine/ir/patch_bus.hpp`
- `include/void_engine/ir/namespace.hpp`
- `src/ir/patch.cpp`
- `src/ir/transaction.cpp`
- `src/ir/patch_bus.cpp`

---

### 3.2 Kernel Orchestration

**Source:** `legacy/crates/void_kernel/`

**What it provides:**
- Frame lifecycle management (begin_frame, end_frame)
- Transaction processing coordination
- Render graph building
- Layer management
- App isolation

**Rust Implementation:**
```rust
pub struct Kernel {
    patch_bus: PatchBus,
    layer_manager: LayerManager,
    app_manager: AppManager,
    supervisor: Supervisor,
    watchdog: Watchdog,
    recovery_manager: RecoveryManager,
}

impl Kernel {
    pub fn begin_frame(&mut self, delta: f32) -> FrameContext;
    pub fn process_transactions(&mut self, world: &mut World);
    pub fn build_render_graph(&self) -> RenderGraph;
    pub fn end_frame(&mut self);
}
```

**Required C++ Implementation:**
```cpp
// include/void_engine/kernel/kernel.hpp

namespace void_kernel {

class Kernel {
public:
    FrameContext begin_frame(float delta_time);
    void process_transactions(World& world);
    RenderGraph build_render_graph();
    void end_frame();

    PatchBus& patch_bus() { return m_patch_bus; }
    LayerManager& layers() { return m_layer_manager; }

private:
    PatchBus m_patch_bus;
    LayerManager m_layer_manager;
    AppManager m_app_manager;
    Supervisor m_supervisor;
    Watchdog m_watchdog;
    RecoveryManager m_recovery_manager;
};

struct FrameContext {
    uint64_t frame;
    float delta_time;
    double total_time;
    FrameState state;
};

} // namespace void_kernel
```

**Files to create:**
- `include/void_engine/kernel/kernel.hpp`
- `include/void_engine/kernel/frame_context.hpp`
- `include/void_engine/kernel/layer_manager.hpp`
- `include/void_engine/kernel/app_manager.hpp`
- `src/kernel/kernel.cpp`
- `src/kernel/layer_manager.cpp`
- `src/kernel/app_manager.cpp`

---

## 4. High Priority Features

### 4.1 Supervisor/Watchdog

**Source:** `legacy/crates/void_kernel/` (supervisor, watchdog modules)

**What it provides:**
- Fault-tolerant app execution
- Health monitoring with timeouts
- Automatic crash recovery
- State snapshots for restoration

**Required C++ Implementation:**
```cpp
// include/void_engine/kernel/supervisor.hpp

namespace void_kernel {

class Watchdog {
public:
    void start(std::chrono::milliseconds timeout);
    void pet();  // Reset timeout
    bool is_healthy() const;
    void on_timeout(std::function<void()> callback);

private:
    std::chrono::steady_clock::time_point m_last_pet;
    std::chrono::milliseconds m_timeout;
    std::function<void()> m_timeout_callback;
};

class RecoveryManager {
public:
    void create_snapshot(const World& world);
    bool restore_snapshot(World& world);
    void clear_snapshots();

private:
    std::vector<WorldSnapshot> m_snapshots;
};

class Supervisor {
public:
    void monitor(App& app);
    void on_fault(std::function<void(const Fault&)> handler);
    void restart_app(App& app);

private:
    Watchdog m_watchdog;
    RecoveryManager m_recovery;
};

} // namespace void_kernel
```

**Files to create:**
- `include/void_engine/kernel/supervisor.hpp`
- `include/void_engine/kernel/watchdog.hpp`
- `include/void_engine/kernel/recovery_manager.hpp`
- `src/kernel/supervisor.cpp`
- `src/kernel/watchdog.cpp`
- `src/kernel/recovery_manager.cpp`

---

### 4.2 Explicit Render Graph

**Source:** `legacy/crates/void_render/`

**What it provides:**
- Declarative render pass definitions
- Automatic dependency resolution
- Pass scheduling optimization
- Resource lifetime management

**Rust Implementation:**
```rust
pub struct RenderGraph {
    pub passes: Vec<RenderPass>,
    pub dependencies: Vec<(PassId, PassId)>,
}

pub struct RenderPass {
    pub id: PassId,
    pub inputs: Vec<ResourceId>,
    pub outputs: Vec<ResourceId>,
    pub execute: Box<dyn Fn(&mut RenderContext)>,
}
```

**Required C++ Implementation:**
```cpp
// include/void_engine/render/render_graph.hpp

namespace void_render {

using PassId = uint32_t;
using ResourceId = uint32_t;

struct RenderPass {
    PassId id;
    std::vector<ResourceId> inputs;
    std::vector<ResourceId> outputs;
    std::function<void(RenderContext&)> execute;
};

class RenderGraph {
public:
    PassId add_pass(const std::string& name, RenderPass pass);
    void add_dependency(PassId from, PassId to);
    void compile();  // Topological sort, optimize
    void execute(RenderContext& ctx);

private:
    std::vector<RenderPass> m_passes;
    std::vector<std::pair<PassId, PassId>> m_dependencies;
    std::vector<PassId> m_execution_order;
};

} // namespace void_render
```

**Files to create:**
- `include/void_engine/render/render_graph.hpp`
- `include/void_engine/render/render_pass.hpp`
- `include/void_engine/render/render_resource.hpp`
- `src/render/render_graph.cpp`
- `src/render/render_pass.cpp`

---

### 4.3 Layer-Based Compositing

**Source:** `legacy/crates/void_render/`

**What it provides:**
- Priority-sorted render layers
- Blend modes between layers
- Per-layer post-processing
- Layer visibility control

**Rust Implementation:**
```rust
pub struct Layer {
    pub id: LayerId,
    pub priority: i32,
    pub visible: bool,
    pub blend_mode: BlendMode,
    pub post_processing: Vec<PostProcess>,
}

pub struct Compositor {
    layers: Vec<Layer>,
    renderers: HashMap<LayerId, Box<dyn CompositorRenderer>>,
}
```

**Required C++ Implementation:**
```cpp
// include/void_engine/render/compositor.hpp

namespace void_render {

enum class BlendMode {
    Opaque,
    Alpha,
    Additive,
    Multiply
};

struct Layer {
    LayerId id;
    int32_t priority;
    bool visible{true};
    BlendMode blend_mode{BlendMode::Alpha};
    std::vector<std::unique_ptr<PostProcess>> post_processing;
};

class Compositor {
public:
    LayerId create_layer(const LayerConfig& config);
    void set_renderer(LayerId layer, std::unique_ptr<ICompositorRenderer> renderer);
    void set_layer_visible(LayerId layer, bool visible);
    void set_layer_priority(LayerId layer, int32_t priority);

    Frame begin_frame();
    void execute(const RenderGraph& graph, Frame& frame);
    void end_frame(Frame frame);

private:
    std::vector<Layer> m_layers;
    std::unordered_map<LayerId, std::unique_ptr<ICompositorRenderer>> m_renderers;
};

} // namespace void_render
```

**Files to create:**
- `include/void_engine/render/layer.hpp`
- `include/void_engine/render/blend_mode.hpp`
- `include/void_engine/render/compositor_renderer.hpp`
- Update existing `compositor.hpp`
- `src/render/layer.cpp`

---

### 4.4 Multi-Backend Rendering

**Source:** `legacy/crates/void_presenter/`

**What it provides:**
- Abstract presenter interface
- Desktop backend (Vulkan/Metal/DX12 via wgpu-native)
- Web backend (WebGPU)
- XR backend (OpenXR)

**Rust Implementation:**
```rust
pub trait Presenter: Send + Sync {
    fn begin_frame(&mut self) -> Frame;
    fn present(&mut self, frame: Frame);
    fn reconfigure(&mut self, config: SurfaceConfig);
    fn resize(&mut self, width: u32, height: u32);
}

pub struct DesktopPresenter { /* wgpu */ }
pub struct WebPresenter { /* WebGPU */ }
pub struct XrPresenter { /* OpenXR */ }
```

**Required C++ Implementation:**
```cpp
// include/void_engine/presenter/presenter.hpp

namespace void_presenter {

class IPresenter {
public:
    virtual ~IPresenter() = default;

    virtual Frame begin_frame() = 0;
    virtual void present(Frame frame) = 0;
    virtual void reconfigure(const SurfaceConfig& config) = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual Backend backend() const = 0;
};

enum class Backend {
    OpenGL,
    Vulkan,
    Metal,
    DX12,
    WebGPU
};

// Concrete implementations
class OpenGLPresenter : public IPresenter { /* existing GLFW */ };
class VulkanPresenter : public IPresenter { /* new */ };
class WebGPUPresenter : public IPresenter { /* new, for wasm */ };

// Factory
std::unique_ptr<IPresenter> create_presenter(Backend backend, const PresenterConfig& config);

} // namespace void_presenter
```

**Files to create:**
- Update `include/void_engine/presenter/presenter.hpp` (abstract interface)
- `include/void_engine/presenter/vulkan_presenter.hpp`
- `include/void_engine/presenter/webgpu_presenter.hpp`
- `src/presenter/vulkan_presenter.cpp`
- `src/presenter/webgpu_presenter.cpp`
- `src/presenter/presenter_factory.cpp`

---

## 5. Medium Priority Features

### 5.1 Extended Plugin Lifecycle

**Source:** `legacy/crates/void_engine/`

**Gap:** C++ has 4 lifecycle hooks, Rust has 11.

**Rust Lifecycle:**
```rust
on_init()
on_start()
on_pre_update()
on_fixed_update()
on_update()
on_post_update()
on_pre_render()
on_render()
on_post_render()
on_stop()
on_shutdown()
```

**Required C++ Addition:**
```cpp
// Update include/void_engine/engine/subsystem.hpp

class IEngineSubsystem {
public:
    virtual ~IEngineSubsystem() = default;

    // Existing
    virtual void init() {}
    virtual void shutdown() {}

    // NEW: Add these hooks
    virtual void on_start() {}
    virtual void on_stop() {}
    virtual void on_pre_update(float dt) {}
    virtual void on_fixed_update(float fixed_dt) {}
    virtual void on_update(float dt) {}
    virtual void on_post_update(float dt) {}
    virtual void on_pre_render() {}
    virtual void on_render() {}
    virtual void on_post_render() {}
};
```

---

### 5.2 XR Stereo Pipeline

**Source:** `legacy/crates/void_xr/`

**Gap:** C++ has basic XR, Rust has full stereo rendering pipeline.

**Required additions:**
- Per-eye view rendering
- Stereo projection matrices
- Reprojection support
- Foveated rendering hints

---

### 5.3 Visual Scripting (Full)

**Source:** `legacy/crates/void_graph/`

**Gap:** C++ has basic graph, Rust has Blueprint-compatible system.

**Required additions:**
- Flow control nodes (Branch, Sequence, ForLoop, WhileLoop)
- Full math node library
- Event nodes (OnTick, OnInput, OnCollision)
- Custom node registration
- Graph executor with proper flow

---

## 6. Low Priority Features

### 6.1 Full Hand Tracking

**Source:** `legacy/crates/void_xr/`

**Required:**
- 26-joint hand skeleton
- Pinch/grip detection
- Gesture recognition

### 6.2 Shell (vsh)

**Source:** `legacy/crates/void_shell/`

**Required:**
- Command-line interface
- REPL for engine interaction
- Scripting integration

---

## 7. Architecture Adaptations

### State Update Model Migration

**Current C++ (Direct Mutation):**
```cpp
world.query<TransformComponent>([](Entity e, TransformComponent& t) {
    t.position += velocity * dt;  // Direct write
});
```

**Target C++ (IR Patches):**
```cpp
// Option A: Explicit patches (full Rust parity)
kernel.patch_bus().emit(Patch{
    .namespace_id = Namespace::Ecs,
    .op = PatchOp::Update,
    .target = PatchTarget::component(entity, typeid(TransformComponent)),
    .data = TransformPatchData{.position = new_pos}
});

// Option B: Helper that generates patches (ergonomic wrapper)
world.patch<TransformComponent>(entity, [](TransformComponent& t) {
    t.position += velocity * dt;
});  // Internally creates and emits patch
```

**Recommendation:** Implement Option B as a wrapper around Option A for ergonomics.

### Render Loop Update

**Current C++ Loop:**
```
Frame Timing → Input → Events → Assets → Physics → Scene/ECS
    → Renderer Update → Render → Compositor → Present → Frame End
```

**Target C++ Loop (matches Rust):**
```
Timing → Kernel Begin → Pre-Update → Fixed Update → Update
    → IR Transactions → Post-Update → Events → ECS Systems
    → Build Graph → Pre-Render → Render → Post-Render → Present → Kernel End
```

---

## 8. Migration Checklist

### Phase 1: Core Infrastructure (Critical)

- [ ] **void_ir module**
  - [ ] `Patch` struct
  - [ ] `PatchOp` enum
  - [ ] `Namespace` enum
  - [ ] `Transaction` struct
  - [ ] `PatchBus` class
  - [ ] Unit tests

- [ ] **void_kernel module**
  - [ ] `Kernel` class
  - [ ] `FrameContext` struct
  - [ ] `begin_frame()` / `end_frame()`
  - [ ] `process_transactions()`
  - [ ] `build_render_graph()`
  - [ ] Unit tests

### Phase 2: Fault Tolerance (High)

- [ ] **Supervisor/Watchdog**
  - [ ] `Watchdog` class
  - [ ] `RecoveryManager` class
  - [ ] `Supervisor` class
  - [ ] State snapshot/restore
  - [ ] Integration tests

### Phase 3: Rendering (High)

- [ ] **Render Graph**
  - [ ] `RenderGraph` class
  - [ ] `RenderPass` struct
  - [ ] Dependency resolution
  - [ ] Pass scheduling
  - [ ] Unit tests

- [ ] **Layer Compositing**
  - [ ] `Layer` struct
  - [ ] `BlendMode` enum
  - [ ] Update `Compositor`
  - [ ] Per-layer post-processing

- [ ] **Multi-Backend**
  - [ ] Abstract `IPresenter` interface
  - [ ] `VulkanPresenter` (optional)
  - [ ] `WebGPUPresenter` (optional)
  - [ ] Presenter factory

### Phase 4: Engine Integration (Medium)

- [ ] **Extended Lifecycle**
  - [ ] Add `on_start()` / `on_stop()`
  - [ ] Add `on_pre_update()` / `on_post_update()`
  - [ ] Add `on_pre_render()` / `on_post_render()`
  - [ ] Update engine loop to call all hooks

- [ ] **Update main loop**
  - [ ] Integrate Kernel
  - [ ] Use IR transactions
  - [ ] Use RenderGraph

### Phase 5: XR & Extras (Medium/Low)

- [ ] **XR Stereo Pipeline**
  - [ ] Per-eye rendering
  - [ ] Stereo matrices
  - [ ] Reprojection

- [ ] **Visual Scripting**
  - [ ] Flow control nodes
  - [ ] Event nodes
  - [ ] Graph executor

- [ ] **Hand Tracking**
  - [ ] Joint tracking
  - [ ] Gesture detection

- [ ] **Shell**
  - [ ] Command interface
  - [ ] REPL

---

## 9. Implementation Guidelines

### Naming Conventions

| Rust | C++ |
|------|-----|
| `snake_case` functions | `snake_case` functions |
| `PascalCase` types | `PascalCase` types |
| `m_` prefix for members | `m_` prefix for members |
| `k_` prefix for constants | `k_` prefix for constants |

### Error Handling

| Rust | C++ |
|------|-----|
| `Result<T, E>` | `std::expected<T, Error>` (C++23) or custom `Result<T>` |
| `Option<T>` | `std::optional<T>` |
| `panic!()` | `throw` or `std::terminate()` |

### Memory Management

| Rust | C++ |
|------|-----|
| `Box<T>` | `std::unique_ptr<T>` |
| `Rc<T>` | `std::shared_ptr<T>` |
| `Arc<T>` | `std::shared_ptr<T>` (with atomic) |
| `&T` / `&mut T` | `const T&` / `T&` |

### Thread Safety

| Rust | C++ |
|------|-----|
| `Mutex<T>` | `std::mutex` + `T` |
| `RwLock<T>` | `std::shared_mutex` + `T` |
| `Atomic<T>` | `std::atomic<T>` |
| `Send + Sync` traits | Document thread-safety requirements |

---

## Summary

**Total features to migrate:** 11

| Priority | Count | Features |
|----------|-------|----------|
| Critical | 2 | IR System, Kernel |
| High | 4 | Supervisor, Render Graph, Layers, Multi-Backend |
| Medium | 3 | Lifecycle, XR Stereo, Visual Scripting |
| Low | 2 | Hand Tracking, Shell |

**Estimated effort:** High (multi-phase implementation)

**Recommended order:**
1. IR Patch System (foundation for everything)
2. Kernel Orchestration (integrates IR)
3. Render Graph (modern rendering)
4. Extended Lifecycle (plugin API)
5. Everything else

---

*Migration guide for void_engine: Rust legacy → C++ modern implementation*
