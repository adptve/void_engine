# Phase 12: Module Changes Required

This document lists all module changes required to fully implement the architecture from `doc/review/`.

## Summary

The new architecture requires:
1. **Runtime** - Application lifecycle owner (CREATED - needs expansion)
2. **Kernel** - Stage scheduler + hot-reload orchestrator (NEEDS UPDATES)
3. **Scene == World** - World management, not graph ownership (NEEDS REFACTOR)
4. **ECS-first** - ECS is authoritative, graphs are derived views (PARTIALLY DONE)
5. **Plugin-based gameplay** - AI/Combat/Inventory as plugins, not modules (DONE)
6. **Event-based triggers** - Triggers emit events, not callbacks (NEEDS REFACTOR)
7. **Widget bindings** - Widgets bind to ECS/world state (DONE)

---

## 1. Kernel Module (`src/kernel/`)

### Current State
- Has reload manager for hot-reload
- Has service registry
- Missing: Stage scheduler

### Required Changes

#### 1.1 Add Stage Enum
```cpp
// include/void_engine/kernel/kernel.hpp
enum class Stage : std::uint8_t {
    Input,
    HotReloadPoll,
    EventDispatch,
    Update,
    FixedUpdate,
    PostFixed,
    RenderPrepare,
    Render,
    UI,
    Audio,
    Streaming
};
```

#### 1.2 Add Stage Scheduler
```cpp
class Kernel {
public:
    // Stage management
    void register_system(Stage stage, std::string_view name, SystemFunc func);
    void unregister_system(Stage stage, std::string_view name);
    void run_stage(Stage stage, float dt);

    // Hot-reload orchestration (sole authority)
    void enable_hot_reload(std::uint32_t poll_ms, std::uint32_t debounce_ms);
    void trigger_reload(const std::string& unit_name);

    // Lifecycle
    Result<void> initialize();
    void shutdown();
};
```

#### 1.3 Centralize Hot-Reload
- Remove hot-reload logic from individual modules
- Kernel orchestrates: snapshot → unload → reload → restore → post
- Kernel publishes reload lifecycle events to EventBus

### Files to Modify
- `include/void_engine/kernel/kernel.hpp` - Add Stage enum, scheduler API
- `src/kernel/kernel.cpp` - Implement stage scheduler
- `include/void_engine/kernel/reload_manager.hpp` - Integrate with Kernel
- `src/kernel/reload_manager.cpp` - Remove independent reload, use Kernel orchestration

---

## 2. Scene Module (`src/scene/`)

### Current State
- Scene graph with transforms/hierarchy
- Scene acts like a "level file" concept

### Required Changes

#### 2.1 Scene == World
A Scene should become a World instance that owns:
- One ECS world (entities + components)
- World spatial context (XR anchors / VR origin)
- Active layer set
- Active plugin set
- Active widget set

#### 2.2 Remove Authoritative Transforms from Scene Graph
- Scene graph becomes a **derived view** for render submission
- ECS owns authoritative transforms
- Graph is rebuildable/cacheable

#### 2.3 Add World Management
```cpp
// include/void_engine/scene/world.hpp
class World {
public:
    // ECS ownership
    void_ecs::World& ecs() { return m_ecs; }

    // Layer management
    void activate_layer(const Layer& layer);
    void deactivate_layer(const std::string& layer_id);

    // Plugin activation
    void activate_plugin(const std::string& plugin_id);
    void deactivate_plugin(const std::string& plugin_id);

    // Widget activation
    void activate_widget_set(const std::string& widget_set_id);

    // Spatial context
    void set_spatial_context(SpatialContext ctx);

private:
    void_ecs::World m_ecs;
    std::vector<Layer> m_active_layers;
    // ...
};
```

### Files to Modify
- `include/void_engine/scene/scene.hpp` - Refactor to World concept
- `src/scene/scene.cpp` - Remove authoritative transform ownership
- `include/void_engine/scene/scene_graph.hpp` - Make into derived view
- NEW: `include/void_engine/scene/world.hpp` - World class
- NEW: `include/void_engine/scene/layer.hpp` - Layer concept

---

## 3. Triggers Module (`src/triggers/`)

### Current State
- Trigger callbacks embed gameplay logic
- Callbacks are unsafe across hot-reload

### Required Changes

#### 3.1 Event-Based Triggers
Triggers should emit data events, not call callbacks:

```cpp
// Trigger events
struct TriggerEnterEvent {
    TriggerId trigger;
    EntityId entity;
    Vec3 position;
    double timestamp;
};

struct TriggerExitEvent {
    TriggerId trigger;
    EntityId entity;
};

struct TriggerStayEvent {
    TriggerId trigger;
    EntityId entity;
    float duration;
};
```

#### 3.2 Plugins Consume Events
Plugins subscribe to trigger events and decide what to do:
```cpp
// In a gameplay plugin
event_bus.subscribe<TriggerEnterEvent>([](const TriggerEnterEvent& e) {
    // React to trigger
});
```

### Files to Modify
- `include/void_engine/triggers/trigger_events.hpp` - Add event types
- `src/triggers/trigger_system.cpp` - Emit events instead of callbacks
- Remove callback registration API (or deprecate)

---

## 4. ECS Module (`src/ecs/`)

### Current State
- ECS implementation exists
- Used for some entity/component storage

### Required Changes

#### 4.1 Ensure ECS is Authoritative
All gameplay state should live in ECS:
- Transforms (Position, Rotation, Scale)
- Hierarchy (Parent, Children)
- Gameplay state (Health, AI state, etc.)
- XR anchoring
- Network identity

#### 4.2 Entity Identity Unification
Single canonical EntityId across:
- ECS
- GameStateCore (if kept)
- Widget bindings
- Networking

### Files to Modify
- `include/void_engine/ecs/components.hpp` - Ensure all state components exist
- `src/ecs/` - Verify entity lifecycle is authoritative

---

## 5. GameState Module (`src/gamestate/`)

### Current State
- Separate GameStateCore with its own EntityId
- AIStateStore, CombatStateStore, InventoryStateStore

### Required Changes

#### Option A: ECS-First (Recommended)
- Move gameplay state into ECS components
- GameStateCore becomes optional convenience layer over ECS
- Remove separate EntityId - use ECS entity IDs

#### Option B: Hybrid (If Needed)
- Keep stores but use ECS EntityId
- Explicit spawn/despawn sync rules
- Clear ownership boundaries

### Files to Modify
- `include/void_engine/gamestate/gamestate_core.hpp` - Unify EntityId
- `src/gamestate/gamestate_core.cpp` - Sync with ECS lifecycle
- Consider moving stores to ECS components

---

## 6. Runtime Module (`src/runtime/`)

### Current State
- Old Application/Bootstrap (preserved as `runtime_legacy.hpp/cpp`)
- New Runtime class (created)

### Required Changes

#### 6.1 Complete Runtime Implementation
- Integrate with real Kernel
- Platform initialization (GLFW, Vulkan, OpenXR)
- World loading from API/manifest
- Plugin activation
- Widget activation

#### 6.2 Mode Support
- Headless: No graphics
- Windowed: GLFW + Vulkan/OpenGL
- XR: OpenXR integration
- Editor: GLFW + ImGui

### Files to Create/Modify
- `src/runtime/runtime.cpp` - Complete implementation
- `src/runtime/platform_windowed.cpp` - GLFW/Vulkan init
- `src/runtime/platform_xr.cpp` - OpenXR init (future)
- `src/runtime/platform_headless.cpp` - Headless mode

---

## 7. CMakeLists.txt Updates

### main.cpp vs main-bootstrap.cpp
- `main.cpp` - New thin production entry point
- `main-bootstrap.cpp` - Validation harness (separate target)

```cmake
# Production executable
add_executable(void_engine src/main.cpp)

# Validation harness (separate)
add_executable(void_engine_bootstrap src/main-bootstrap.cpp)
```

### Runtime Module Build
```cmake
# Add runtime module
add_subdirectory(src/runtime)
target_link_libraries(void_engine PRIVATE void_runtime)
```

---

## 8. Priority Order for Implementation

### Phase 12a: Core Infrastructure
1. Kernel stage scheduler
2. Kernel hot-reload orchestration
3. Runtime completion

### Phase 12b: World Management
4. Scene → World refactor
5. ECS authoritative transforms
6. Entity ID unification

### Phase 12c: Event-Based Triggers
7. Trigger event emission
8. Remove callback patterns

### Phase 12d: Platform Integration
9. Windowed mode (GLFW + Vulkan)
10. Mode selection

---

## 9. Testing Strategy

### Unit Tests
- Kernel stage execution order
- Kernel hot-reload lifecycle
- World load/unload
- Trigger event emission

### Integration Tests
- Full boot sequence
- World switch
- Hot-reload cycle
- Plugin activation/deactivation

### Validation Harness
- main-bootstrap.cpp serves as comprehensive integration test
- Tests all phases work together

---

## 10. Architecture Invariants (Must Not Violate)

From `doc/review/`:

1. **ECS is authoritative** - All entity state lives in ECS
2. **Scene == World** - Not a graph, not a level file
3. **Plugins contain systems** - Gameplay is plugin content
4. **Widgets are reactive views** - Bind to world state
5. **Layers are patches, not owners** - Additive composition
6. **Kernel orchestrates reload** - Single authority
7. **Runtime owns lifecycle** - Not main.cpp
8. **Everything is loadable via API** - No built-in content assumptions
