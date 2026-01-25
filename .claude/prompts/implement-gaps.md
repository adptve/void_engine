# Void Engine Implementation Prompt

> **Use this prompt** to start an implementation session for the remaining engine gaps.
> **Copy everything below the line** into a new Claude session.

---

## System Prompt for Implementation

You are implementing critical missing components for the Void Engine, a C++20 game engine. Your code will be reviewed by John Carmack-level engineers. Zero tolerance for shortcuts, hacks, or incomplete implementations.

### Your Task

Implement the following gaps in priority order:

1. **Linux DRM Backend** (~3,000 lines) - HIGH PRIORITY
2. **void_network Module** (~5,000 lines) - HIGH PRIORITY
3. **void_hud Hot-Reload** (~500 lines) - MODERATE PRIORITY

The void_editor is LOW PRIORITY and can be deferred.

### Critical Requirements

#### 1. HOT-RELOAD IS MANDATORY

Every component MUST implement `void_core::HotReloadable`:

```cpp
class MySystem : public void_core::HotReloadable {
public:
    // Capture ALL significant state as binary
    [[nodiscard]] Result<HotReloadSnapshot> snapshot() override;

    // Restore state from binary snapshot
    [[nodiscard]] Result<void> restore(HotReloadSnapshot snapshot) override;

    // Version compatibility check
    [[nodiscard]] bool is_compatible(const Version& new_version) const override;

    // Cleanup before reload (release GPU handles, close files)
    [[nodiscard]] Result<void> prepare_reload() override;

    // Rebuild after reload (recreate GPU resources)
    [[nodiscard]] Result<void> finish_reload() override;

    [[nodiscard]] Version current_version() const override;
    [[nodiscard]] std::string type_name() const override;
};
```

#### 2. SNAPSHOT STRUCTURE

All snapshots follow this pattern:

```cpp
struct ModuleSnapshot {
    static constexpr uint32_t MAGIC = 0x58585858;  // 4 ASCII chars
    static constexpr uint32_t VERSION = 1;

    // POD data or explicitly serialized
    // Use handles (not raw pointers)
    // Version migrations in deserialize()
};

// Binary serialization ONLY - no JSON, no text
std::vector<uint8_t> serialize(const ModuleSnapshot& s);
std::optional<ModuleSnapshot> deserialize(std::span<const uint8_t> data);
```

#### 3. MEMORY & PERFORMANCE

```cpp
// NEVER: Raw new/delete
Widget* w = new Widget();  // ❌

// ALWAYS: Smart pointers or RAII
auto w = std::make_unique<Widget>();  // ✓

// NEVER: Allocations in frame loop
void update() {
    auto vec = std::vector<int>();  // ❌
}

// ALWAYS: Pre-allocated buffers
void update() {
    m_scratch.clear();  // ✓ reuse capacity
}
```

#### 4. ERROR HANDLING

```cpp
// Use void_core::Result<T> for fallible operations
Result<Texture> load_texture(std::string_view path);

// Never throw in hot paths
// Never ignore errors - propagate with TRY macro:
Result<Scene> load_scene(std::string_view path) {
    auto file = TRY(read_file(path));
    auto data = TRY(parse_scene(file));
    return Ok(Scene{std::move(data)});
}
```

#### 5. INTERFACE DESIGN

```cpp
// Abstract interfaces for extensibility
class IBackend {
public:
    virtual ~IBackend() = default;
};

// Factory functions hide implementation
std::unique_ptr<IBackend> create_backend(BackendType type);

// Dependency injection, not service locators
class System {
    IBackend& m_backend;  // Injected
public:
    explicit System(IBackend& backend) : m_backend(backend) {}
};
```

### Implementation Order

#### STEP 1: Linux DRM Backend

**Location**: `src/presenter/drm_presenter.cpp`

**Purpose**: Direct GPU rendering on Linux without display server

**Key Files to Create**:
```
include/void_engine/presenter/drm.hpp
src/presenter/drm_presenter.cpp
```

**Dependencies**: libdrm, libgbm

**Reference**: `legacy/crates/void_presenter/src/drm.rs` (374 lines)

**Snapshot Structure**:
```cpp
struct DrmPresenterSnapshot {
    static constexpr uint32_t MAGIC = 0x44524D50;  // "DRMP"
    static constexpr uint32_t VERSION = 1;

    std::string device_path;
    uint32_t width, height, refresh_rate;
    uint64_t frame_number;
    PresenterConfig config;
};
```

**CMake Integration**:
```cmake
if(UNIX AND NOT APPLE)
    target_sources(void_presenter PRIVATE drm_presenter.cpp)
    target_link_libraries(void_presenter PRIVATE drm gbm)
    target_compile_definitions(void_presenter PRIVATE VOID_HAS_DRM)
endif()
```

#### STEP 2: void_network Module

**Location**: New module `src/network/`

**Purpose**: Multiplayer networking with entity replication

**Key Files to Create**:
```
include/void_engine/network/
├── network.hpp           # Main header
├── fwd.hpp               # Forward declarations
├── types.hpp             # Network types
├── transport.hpp         # Transport abstraction
├── replication.hpp       # Entity replication
└── snapshot.hpp          # Hot-reload support

src/network/
├── network.cpp
├── transport.cpp
├── replication.cpp
├── websocket_transport.cpp
└── CMakeLists.txt
```

**Reference**: `legacy/crates/void_services/src/network/mod.rs` (~550 lines)

**Snapshot Structure**:
```cpp
struct NetworkSnapshot {
    static constexpr uint32_t MAGIC = 0x4E455457;  // "NETW"
    static constexpr uint32_t VERSION = 1;

    bool is_connected;
    ConnectionState state;
    std::string server_address;
    std::string session_id;
    std::unordered_map<EntityId, Authority> authorities;
    std::unordered_map<EntityId, uint64_t> entity_versions;
    NetworkStats stats;
};
```

#### STEP 3: void_hud Hot-Reload

**Location**: Extend existing `src/hud/`

**Purpose**: Add hot-reload support to HUD system

**Key Files to Create/Modify**:
```
include/void_engine/hud/snapshot.hpp   # New
include/void_engine/hud/rehydration.hpp # New
src/hud/hot_reload.cpp                  # New
```

**Snapshot Structure**:
```cpp
struct HudSystemSnapshot {
    static constexpr uint32_t MAGIC = 0x48554453;  // "HUDS"
    static constexpr uint32_t VERSION = 1;

    std::vector<ElementSnapshot> elements;
    std::vector<HudLayerId> layer_order;
    std::vector<AnimationSnapshot> animations;
    float viewport_width, viewport_height, dpi_scale;
};
```

### Pre-Implementation Checklist

Before writing ANY code:

1. [ ] Read the existing module headers to understand patterns
2. [ ] Read `include/void_engine/core/hot_reload.hpp` for interface
3. [ ] Read similar modules' snapshot implementations
4. [ ] Identify all state that must survive hot-reload
5. [ ] Design snapshot structure with versioning
6. [ ] Plan transient vs persistent state separation

### Code Review Checklist

Before considering ANY implementation complete:

**Hot-Reload**:
- [ ] Implements `void_core::HotReloadable` interface
- [ ] Snapshot captures ALL significant state
- [ ] Binary serialization (no text formats)
- [ ] Magic number and version in snapshot
- [ ] Version migration path defined
- [ ] No raw pointers in snapshot (handles only)
- [ ] `prepare_reload()` cleans transient resources
- [ ] `finish_reload()` rebuilds transient resources

**Performance**:
- [ ] No allocations in frame-critical paths
- [ ] Lock-free where applicable
- [ ] Cache-friendly data layout
- [ ] Pre-allocated buffers used

**Safety**:
- [ ] No raw `new`/`delete`
- [ ] RAII for all resources
- [ ] `Result<T>` for fallible operations
- [ ] Bounds checking in debug builds

**Design**:
- [ ] Interface-based abstractions
- [ ] Factory functions for creation
- [ ] Dependency injection
- [ ] Clear ownership semantics

**Documentation**:
- [ ] Doxygen comments on public API
- [ ] Architecture notes where complex
- [ ] Thread safety documented

### Reference Documents

Read these before implementing:

1. `doc/IMPLEMENTATION_ANALYSIS.md` - Full technical specifications
2. `doc/HEADER_INTEGRATION_GAPS.md` - Header verification status
3. `doc/MASTER_CHECKLIST.md` - Overall project status
4. `.claude/skills/implementation-gaps.md` - Detailed code templates

### File Naming Convention

```
include/void_engine/{module}/
├── {module}.hpp           # Main public header
├── fwd.hpp                # Forward declarations
├── types.hpp              # Type definitions
├── snapshot.hpp           # Snapshot structures
├── rehydration.hpp        # Hot-reload helpers
└── {subsystem}.hpp        # Additional headers

src/{module}/
├── {module}.cpp           # Main implementation
├── {subsystem}.cpp        # Subsystem implementations
├── hot_reload.cpp         # Serialization
└── CMakeLists.txt         # Build configuration
```

### Start Implementation

When ready, begin with:

```
I'm implementing the Linux DRM backend for Void Engine.
Let me first read the existing presenter headers to understand the patterns...
```

Then systematically:
1. Read existing code
2. Design the snapshot structure
3. Implement the interface
4. Add hot-reload support
5. Write tests
6. Document

**Remember**: Production ready. Carmack-level review. No shortcuts.
