# Void Engine: Implement ALL Headers

> **PRIMARY OBJECTIVE**: Every header file that defines a class or interface MUST have a working implementation integrated into the engine.
> **Review Standard**: John Carmack level - zero tolerance for stubs or incomplete code
> **Created**: 2026-01-25

---

## COPY BELOW THIS LINE INTO NEW CLAUDE SESSION

---

You are implementing ALL missing header implementations for the Void Engine. Your PRIMARY task is to ensure every header that defines a class or interface has a complete, working .cpp implementation.

## THE RULE

**For EVERY header that defines a class with non-inline methods → there MUST be a .cpp file with the implementation.**

No stubs. No TODOs. No placeholders. Production-ready code.

---

## CRITICAL: IMPLEMENTATION PROCESS

### Step 0: Before ANYTHING Else

Read the existing codebase patterns:
```
1. Read include/void_engine/core/hot_reload.hpp - understand HotReloadable interface
2. Read an existing complete module (e.g., src/audio/) - understand patterns
3. Read the legacy Rust code in legacy/crates/ for behavior reference
```

### Step 1: Work ONE MODULE at a time

Do NOT try to implement everything at once. For each module:
1. Read ALL headers in that module first
2. Understand the interfaces and dependencies
3. Implement each .cpp file
4. Add to CMakeLists.txt
5. Verify it compiles
6. Create the diagram
7. Create validation report
8. THEN move to next module

### Step 2: After EACH implementation

Ask the user to run:
```bash
cmake -B build && cmake --build build
```
Fix any compilation errors before proceeding.

### Step 3: Reference Legacy Code

For behavior reference, check:
```
legacy/crates/void_ir/src/          → IR module behavior
legacy/crates/void_presenter/src/   → Presenter behavior
legacy/crates/void_render/src/      → Render behavior
legacy/crates/void_asset/src/       → Asset behavior
legacy/crates/void_ecs/src/         → ECS behavior
```

### Context Limits

If you run out of context, tell the user:
"I've completed [X module]. To continue, start a new session with this prompt and specify: 'Continue from [next module]'"

---

---

## HEADERS REQUIRING IMPLEMENTATION

### PRIORITY 1: IR Module (8 headers)

| Header | What It Defines | Implementation Needed |
|--------|-----------------|----------------------|
| `ir/batch.hpp` | BatchOptimizer, PatchDeduplicator, PatchSplitter | `src/ir/batch.cpp` |
| `ir/bus.hpp` | PatchBus, AsyncPatchBus | `src/ir/bus.cpp` |
| `ir/ir.hpp` | Main dispatcher/executor | `src/ir/ir.cpp` |
| `ir/namespace.hpp` | Namespace management | `src/ir/namespace.cpp` |
| `ir/patch.hpp` | Patch type system, serialization | `src/ir/patch.cpp` |
| `ir/transaction.hpp` | Transaction management | `src/ir/transaction.cpp` |
| `ir/validation.hpp` | Validation logic | `src/ir/validation.cpp` |
| `ir/value.hpp` | Value type system | `src/ir/value.cpp` |

---

### PRIORITY 2: Presenter Module (12 headers)

| Header | What It Defines | Implementation Needed |
|--------|-----------------|----------------------|
| `presenter/presenter.hpp` | Core Presenter class | `src/presenter/presenter.cpp` |
| `presenter/multi_backend_presenter.hpp` | Multi-backend factory | `src/presenter/multi_backend_presenter.cpp` |
| `presenter/swapchain.hpp` | Swapchain management | `src/presenter/swapchain.cpp` |
| `presenter/surface.hpp` | Window surface abstraction | `src/presenter/surface.cpp` |
| `presenter/frame.hpp` | Frame structure | `src/presenter/frame.cpp` |
| `presenter/timing.hpp` | Frame timing | `src/presenter/timing.cpp` |
| `presenter/rehydration.hpp` | State preservation | `src/presenter/rehydration.cpp` |
| `presenter/presenter_module.hpp` | Module integration | `src/presenter/presenter_module.cpp` |
| `presenter/backends/null_backend.hpp` | Null backend | `src/presenter/backends/null_backend.cpp` |
| `presenter/backends/wgpu_backend.hpp` | WebGPU backend | `src/presenter/backends/wgpu_backend.cpp` |
| `presenter/xr/xr_types.hpp` | XR types | `src/presenter/xr/xr_types.cpp` |
| `presenter/drm.hpp` | Linux DRM backend | `src/presenter/drm_presenter.cpp` |

---

### PRIORITY 3: Render Module (10 headers)

| Header | What It Defines | Implementation Needed |
|--------|-----------------|----------------------|
| `render/camera.hpp` | Camera system | `src/render/camera.cpp` |
| `render/light.hpp` | Lighting system | `src/render/light.cpp` |
| `render/material.hpp` | Material/shader binding | `src/render/material.cpp` |
| `render/mesh.hpp` | Mesh data structures | `src/render/mesh.cpp` |
| `render/pass.hpp` | Render pass system | `src/render/pass.cpp` |
| `render/resource.hpp` | GPU resource management | `src/render/resource.cpp` |
| `render/shadow.hpp` | Shadow mapping | `src/render/shadow.cpp` |
| `render/debug.hpp` | Debug visualization | `src/render/debug.cpp` |
| `render/texture.hpp` | Texture management | `src/render/texture.cpp` |
| `render/render.hpp` | Main render system | `src/render/render.cpp` |

---

### PRIORITY 4: Asset Module (7 headers)

| Header | What It Defines | Implementation Needed |
|--------|-----------------|----------------------|
| `asset/asset.hpp` | Main asset system | `src/asset/asset.cpp` |
| `asset/cache.hpp` | Asset caching | `src/asset/cache.cpp` |
| `asset/handle.hpp` | Asset handle type | `src/asset/handle.cpp` |
| `asset/loader.hpp` | Base loader interface | `src/asset/loader.cpp` |
| `asset/server.hpp` | Asset server | `src/asset/server.cpp` |
| `asset/storage.hpp` | Asset storage/pooling | `src/asset/storage.cpp` |

---

### PRIORITY 5: ECS Module (7 headers)

| Header | What It Defines | Implementation Needed |
|--------|-----------------|----------------------|
| `ecs/archetype.hpp` | Entity grouping | `src/ecs/archetype.cpp` |
| `ecs/bundle.hpp` | Component bundles | `src/ecs/bundle.cpp` |
| `ecs/component.hpp` | Component storage | `src/ecs/component.cpp` |
| `ecs/entity.hpp` | Entity type | `src/ecs/entity.cpp` |
| `ecs/hierarchy.hpp` | Parent-child relationships | `src/ecs/hierarchy.cpp` |
| `ecs/query.hpp` | Query system | `src/ecs/query.cpp` |

---

### PRIORITY 6: Compositor Module (7 headers)

| Header | What It Defines | Implementation Needed |
|--------|-----------------|----------------------|
| `compositor/frame.hpp` | Frame composition | `src/compositor/frame.cpp` |
| `compositor/hdr.hpp` | HDR tone mapping | `src/compositor/hdr.cpp` |
| `compositor/output.hpp` | Output management | `src/compositor/output.cpp` |
| `compositor/vrr.hpp` | Variable refresh rate | `src/compositor/vrr.cpp` |
| `compositor/rehydration.hpp` | State preservation | `src/compositor/rehydration.cpp` |
| `compositor/input.hpp` | Input handling | `src/compositor/input.cpp` |
| `compositor/compositor_module.hpp` | Module integration | `src/compositor/compositor_module.cpp` |

---

### PRIORITY 7: Core Module (8 headers)

| Header | What It Defines | Implementation Needed |
|--------|-----------------|----------------------|
| `core/error.hpp` | Result<T> error type | `src/core/error.cpp` |
| `core/handle.hpp` | Handle allocation | `src/core/handle.cpp` |
| `core/id.hpp` | ID type with generation | `src/core/id.cpp` |
| `core/log.hpp` | Logging system | `src/core/log.cpp` |
| `core/type_registry.hpp` | RTTI and dynamic types | `src/core/type_registry.cpp` |
| `core/hot_reload.hpp` | Hot-reload manager | `src/core/hot_reload.cpp` |
| `core/plugin.hpp` | Plugin system | `src/core/plugin.cpp` |
| `core/version.hpp` | Version comparison | `src/core/version.cpp` |

---

### PRIORITY 8: Physics Module (5 headers)

| Header | What It Defines | Implementation Needed |
|--------|-----------------|----------------------|
| `physics/broadphase.hpp` | Broad-phase collision | `src/physics/broadphase.cpp` |
| `physics/collision.hpp` | Collision response | `src/physics/collision.cpp` |
| `physics/solver.hpp` | Constraint solver | `src/physics/solver.cpp` |
| `physics/physics.hpp` | Main physics system | `src/physics/physics.cpp` |

---

### PRIORITY 9: Services Module (4 headers)

| Header | What It Defines | Implementation Needed |
|--------|-----------------|----------------------|
| `services/event_bus.hpp` | Service event bus | `src/services/event_bus.cpp` |
| `services/service.hpp` | Service base class | `src/services/service.cpp` |
| `services/services.hpp` | Main umbrella | `src/services/services.cpp` |

---

## TOTAL: 68 HEADERS NEED IMPLEMENTATIONS

---

## IMPLEMENTATION REQUIREMENTS

### 1. HOT-RELOAD MANDATORY

Every stateful class MUST implement `void_core::HotReloadable`:

```cpp
class MyClass : public void_core::HotReloadable {
public:
    [[nodiscard]] Result<HotReloadSnapshot> snapshot() override;
    [[nodiscard]] Result<void> restore(HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const Version& new_version) const override;
    [[nodiscard]] Result<void> prepare_reload() override;
    [[nodiscard]] Result<void> finish_reload() override;
    [[nodiscard]] Version current_version() const override;
    [[nodiscard]] std::string type_name() const override;
};
```

### 2. SNAPSHOT PATTERN

```cpp
struct MyClassSnapshot {
    static constexpr uint32_t MAGIC = 0x4D594353;  // "MYCS"
    static constexpr uint32_t VERSION = 1;

    // ALL state that survives hot-reload
    // Use handles, not raw pointers
};

std::vector<uint8_t> serialize(const MyClassSnapshot& s);
std::optional<MyClassSnapshot> deserialize(std::span<const uint8_t> data);
```

### 3. NO ALLOCATIONS IN FRAME LOOP

```cpp
// BAD
void update() {
    auto temp = std::vector<int>();  // ❌ allocation every frame
}

// GOOD
void update() {
    m_scratch.clear();  // ✓ reuse existing capacity
}
```

### 4. RESULT<T> ERROR HANDLING

```cpp
Result<Texture> load_texture(std::string_view path) {
    auto file = TRY(read_file(path));
    auto data = TRY(parse_image(file));
    return Ok(Texture{std::move(data)});
}
```

### 5. INTERFACE DESIGN

```cpp
// Abstract interface
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(const Scene& scene) = 0;
};

// Factory function
std::unique_ptr<IRenderer> create_renderer(RendererType type);
```

---

## WORKFLOW

For EACH header that needs implementation:

1. **READ** the header file completely
2. **IDENTIFY** all non-inline methods that need implementation
3. **CHECK** if it needs hot-reload support
4. **CREATE** the .cpp file with ALL method implementations
5. **ADD** to CMakeLists.txt
6. **VERIFY** it compiles and links

---

## VERIFICATION CHECKLIST

Before marking ANY header as "implemented":

- [ ] .cpp file exists with ALL methods implemented
- [ ] Added to CMakeLists.txt
- [ ] Implements HotReloadable if stateful
- [ ] Snapshot structure defined with MAGIC and VERSION
- [ ] Binary serialization (no JSON)
- [ ] No raw pointers in snapshots (handles only)
- [ ] No allocations in hot paths
- [ ] Result<T> for fallible operations
- [ ] Compiles without warnings
- [ ] Links successfully

---

## START IMPLEMENTATION

Begin with Priority 1 (IR Module):

```
Let me read the first header that needs implementation:
include/void_engine/ir/batch.hpp

Then I will create src/ir/batch.cpp with all implementations...
```

Work through EVERY header systematically until ALL 68 are implemented.

**NO SHORTCUTS. NO STUBS. PRODUCTION READY.**

---

## DOCUMENTATION REQUIREMENTS

### Mermaid Diagrams

After implementing each module, create a Mermaid diagram in `doc/diagrams/`:

```
doc/diagrams/
├── ir_integration.md
├── presenter_integration.md
├── render_integration.md
├── asset_integration.md
├── ecs_integration.md
├── compositor_integration.md
├── core_integration.md
├── physics_integration.md
├── services_integration.md
└── full_integration.md      # Complete system overview
```

Each diagram MUST show:
1. **Class relationships** - inheritance, composition, dependencies
2. **Hot-reload flow** - snapshot → serialize → restore cycle
3. **Data flow** - how data moves through the module
4. **Integration points** - how module connects to others

Example diagram format:

```markdown
# IR Module Integration

## Class Diagram

​```mermaid
classDiagram
    class HotReloadable {
        <<interface>>
        +snapshot() Result~HotReloadSnapshot~
        +restore(snapshot) Result~void~
        +is_compatible(version) bool
    }

    class PatchBus {
        -m_queue LockFreeQueue
        -m_subscribers Map
        +publish(patch) void
        +subscribe(handler) SubscriberId
        +snapshot() Result~HotReloadSnapshot~
    }

    class BatchOptimizer {
        -m_pending_patches Vector
        +optimize(patches) Vector~Patch~
        +snapshot() Result~HotReloadSnapshot~
    }

    HotReloadable <|.. PatchBus
    HotReloadable <|.. BatchOptimizer
    PatchBus --> BatchOptimizer : uses
​```

## Hot-Reload Flow

​```mermaid
sequenceDiagram
    participant Engine
    participant PatchBus
    participant Storage

    Note over Engine: Hot-reload triggered
    Engine->>PatchBus: snapshot()
    PatchBus->>Storage: serialize state
    Storage-->>Engine: binary data

    Note over Engine: Code reloaded

    Engine->>PatchBus: restore(snapshot)
    PatchBus->>Storage: deserialize state
    Storage-->>PatchBus: restored state
    PatchBus-->>Engine: Ok()
​```

## Dependencies

​```mermaid
graph TD
    IR[void_ir] --> Core[void_core]
    IR --> Structures[void_structures]
    IR --> Event[void_event]

    subgraph "IR Module"
        PatchBus --> BatchOptimizer
        PatchBus --> Transaction
        Transaction --> Validation
        BatchOptimizer --> Patch
    end
​```
```

---

## POST-IMPLEMENTATION VALIDATION

After ALL headers in a module are implemented, perform these validations:

### 1. Hot-Reload Validation

```cpp
// Test hot-reload cycle for EVERY class that implements HotReloadable
void validate_hot_reload(HotReloadable& system) {
    // 1. Capture initial state
    auto snapshot1 = system.snapshot();
    ASSERT(snapshot1.is_ok());

    // 2. Modify state
    // ... make changes ...

    // 3. Take another snapshot
    auto snapshot2 = system.snapshot();
    ASSERT(snapshot2.is_ok());

    // 4. Restore to first snapshot
    auto result = system.restore(snapshot1.value());
    ASSERT(result.is_ok());

    // 5. Verify state matches original
    auto snapshot3 = system.snapshot();
    ASSERT(snapshots_equal(snapshot1.value(), snapshot3.value()));
}
```

**Validation Checklist**:
- [ ] Every HotReloadable class passes snapshot→restore→verify cycle
- [ ] Snapshots are binary (check for no JSON/text)
- [ ] Magic numbers are unique per class
- [ ] Version numbers are set
- [ ] No raw pointers in serialized data
- [ ] Handles survive reload (test with actual handle references)

### 2. Performance Validation

```cpp
// Test for allocations in hot paths
void validate_no_frame_allocations() {
    // 1. Pre-warm the system
    for (int i = 0; i < 100; i++) {
        system.update(0.016f);
    }

    // 2. Start allocation tracking
    auto alloc_count_before = get_allocation_count();

    // 3. Run frame loop
    for (int i = 0; i < 1000; i++) {
        system.update(0.016f);
    }

    // 4. Check allocations
    auto alloc_count_after = get_allocation_count();
    ASSERT(alloc_count_after == alloc_count_before,
           "Frame loop allocated memory!");
}
```

**Performance Checklist**:
- [ ] No allocations in update()/tick()/render() paths
- [ ] Pre-allocated buffers used (verify with memory profiler)
- [ ] No virtual calls in inner loops (or marked final)
- [ ] Cache-friendly data layout (SoA where applicable)
- [ ] Lock-free structures where concurrent access needed

### 3. Integration Validation

```cpp
// Test module integrates correctly with engine
void validate_integration() {
    Engine engine;
    engine.init();

    // 1. Module registers correctly
    ASSERT(engine.has_module<MyModule>());

    // 2. Module participates in hot-reload
    engine.trigger_hot_reload();
    ASSERT(engine.module<MyModule>().is_valid());

    // 3. Module handles shutdown
    engine.shutdown();
    // No crashes, no leaks
}
```

**Integration Checklist**:
- [ ] Module registers with engine
- [ ] Module participates in engine hot-reload cycle
- [ ] Module handles init/shutdown cleanly
- [ ] Dependencies resolved correctly
- [ ] No circular dependencies

### 4. Compile & Link Validation

```bash
# Must pass without warnings
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Must link successfully
# Check for undefined symbols
nm -u build/lib/libvoid_*.a | grep -v "^$"
```

**Build Checklist**:
- [ ] Compiles without warnings (-Wall -Wextra -Werror)
- [ ] Links without undefined symbols
- [ ] All .cpp files added to CMakeLists.txt
- [ ] Headers included correctly (no missing includes)

---

## VALIDATION REPORT

After completing each module, create a validation report:

```markdown
# [Module Name] Validation Report

## Implementation Status
- Total headers: X
- Implemented: X
- Remaining: 0

## Hot-Reload Validation
| Class | snapshot() | restore() | Cycle Test | Status |
|-------|------------|-----------|------------|--------|
| ClassA | ✅ | ✅ | ✅ | PASS |
| ClassB | ✅ | ✅ | ✅ | PASS |

## Performance Validation
| Test | Result | Notes |
|------|--------|-------|
| No frame allocations | ✅ PASS | 0 allocations in 1000 frames |
| Pre-allocated buffers | ✅ PASS | All vectors use reserve() |
| Lock-free queues | ✅ PASS | Using LockFreeQueue |

## Integration Validation
| Test | Result |
|------|--------|
| Module registration | ✅ PASS |
| Hot-reload participation | ✅ PASS |
| Clean shutdown | ✅ PASS |

## Build Validation
| Check | Result |
|-------|--------|
| Compiles without warnings | ✅ PASS |
| Links successfully | ✅ PASS |
| CMakeLists.txt updated | ✅ PASS |

## Diagram Created
- [x] `doc/diagrams/[module]_integration.md`
```

Save validation reports to `doc/validation/[module]_validation.md`

---

## FINAL CHECKLIST

Before marking the entire implementation COMPLETE:

- [ ] All 68 headers have .cpp implementations
- [ ] All implementations pass hot-reload validation
- [ ] All implementations pass performance validation
- [ ] All implementations pass integration validation
- [ ] All modules have Mermaid diagrams in `doc/diagrams/`
- [ ] All modules have validation reports in `doc/validation/`
- [ ] Full integration diagram created (`doc/diagrams/full_integration.md`)
- [ ] Engine compiles and links successfully
- [ ] Engine runs without crashes

---

## USER ACTIONS REQUIRED

The AI cannot do everything. The USER must:

### After Each Module Implementation
```bash
# 1. Compile and check for errors
cmake -B build && cmake --build build

# 2. Report any errors back to Claude for fixes

# 3. Run basic tests if available
./build/void_engine --test
```

### After All Modules Complete
```bash
# 1. Full rebuild
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 2. Run the engine
./build/void_engine

# 3. Test hot-reload
# (trigger reload while running)

# 4. Check for memory leaks
valgrind ./build/void_engine  # Linux
# or use AddressSanitizer
```

### If Errors Occur
Copy the EXACT error message back to Claude with:
- The file that failed
- The error message
- Any context about what was being implemented

---

## SESSION MANAGEMENT

This implementation will require MULTIPLE Claude sessions due to context limits.

### Starting a New Session
Copy this prompt and add:
```
Continue implementation from: [Module Name]
Completed modules: [list what's done]
```

### Tracking Progress
Keep a local file tracking:
```
## Implementation Progress

### Completed
- [ ] IR Module (8 headers)
- [ ] Presenter Module (12 headers)
...

### Current Session
Working on: [module]
Completed in this session: [list]
```

---

## ESTIMATED EFFORT

| Module | Headers | Est. Sessions | Est. Lines |
|--------|---------|---------------|------------|
| IR | 8 | 2-3 | ~2,000 |
| Presenter | 12 | 3-4 | ~3,000 |
| Render | 10 | 2-3 | ~2,500 |
| Asset | 7 | 1-2 | ~1,500 |
| ECS | 7 | 2-3 | ~2,000 |
| Compositor | 7 | 1-2 | ~1,500 |
| Core | 8 | 2-3 | ~2,000 |
| Physics | 5 | 1-2 | ~1,200 |
| Services | 4 | 1 | ~800 |
| **TOTAL** | **68** | **15-23** | **~16,500** |

This is NOT a single-session task. Plan accordingly.
