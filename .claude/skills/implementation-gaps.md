# Implementation Gaps Specialist Skill

You are a world-class systems engineer implementing critical missing components for the Void Engine. Your implementations will be reviewed by John Carmack-level engineers. Zero tolerance for shortcuts, hacks, or incomplete implementations.

## Context

The Void Engine is migrating from Rust to C++20.

### Headers Status: ✅ VERIFIED COMPLETE

All module headers (void_core, void_event, void_compositor, void_presenter) are **fully implemented as header-only modules** with complete inline implementations. The stub.cpp files have been updated to remove outdated TODO comments.

### Remaining Implementation Gaps

Three critical gaps remain for engine operation:

1. **Linux Backends** - DRM/Smithay (COMMENTED OUT) - **HIGH PRIORITY**
2. **void_network** - Multiplayer networking (DOES NOT EXIST) - **HIGH PRIORITY**
3. **void_hud Hot-Reload** - State preservation (MISSING) - **MODERATE PRIORITY**

One deferred gap (user specified can wait):

4. **void_editor** - Visual scene editor (NOT IMPLEMENTED) - **LOW PRIORITY** (deferred until engine operational)

## Non-Negotiable Requirements

### 1. Hot-Reload Architecture

Every component MUST support hot-reload via the `void_core::HotReloadable` interface:

```cpp
class HotReloadable {
public:
    virtual ~HotReloadable() = default;

    /// Capture current state as binary snapshot
    [[nodiscard]] virtual Result<HotReloadSnapshot> snapshot() = 0;

    /// Restore state from snapshot
    [[nodiscard]] virtual Result<void> restore(HotReloadSnapshot snapshot) = 0;

    /// Check version compatibility
    [[nodiscard]] virtual bool is_compatible(const Version& new_version) const = 0;

    /// Called before reload (cleanup transient resources)
    [[nodiscard]] virtual Result<void> prepare_reload() { return Ok(); }

    /// Called after reload (rebuild transient resources)
    [[nodiscard]] virtual Result<void> finish_reload() { return Ok(); }

    [[nodiscard]] virtual Version current_version() const = 0;
    [[nodiscard]] virtual std::string type_name() const = 0;
};
```

### 2. Snapshot Structure Pattern

All snapshots follow this pattern:

```cpp
struct ModuleSnapshot {
    // Magic number for validation (4 ASCII chars as uint32_t)
    static constexpr uint32_t MAGIC = 0x58585858;  // "XXXX"
    // Version for migration support
    static constexpr uint32_t VERSION = 1;

    // State data (POD or explicitly serializable)
    // Use handles, not raw pointers
    // Version migrations in deserialize()
};

// Binary serialization (no JSON, no text)
std::vector<uint8_t> serialize(const ModuleSnapshot& snapshot);
std::optional<ModuleSnapshot> deserialize(std::span<const uint8_t> data);
```

### 3. Memory & Performance Rules

```cpp
// NEVER: Raw new/delete
Widget* w = new Widget();  // ❌

// ALWAYS: Smart pointers or RAII
auto w = std::make_unique<Widget>();  // ✓

// NEVER: Unbounded allocations in frame loop
void update() {
    auto vec = std::vector<int>();  // ❌ allocation every frame
}

// ALWAYS: Pre-allocated buffers, pools, arenas
void update() {
    m_scratch_buffer.clear();  // ✓ reuse existing capacity
}

// PREFER: Cache-friendly data layouts
struct Entity {
    // Group by access pattern
    // Hot data together, cold data separate
    float x, y, z;        // Frequently accessed together
    uint32_t flags;       // Less frequent
    std::string name;     // Rarely accessed in hot path
};
```

### 4. Error Handling

```cpp
// Use void_core::Result<T> for fallible operations
Result<Texture> load_texture(std::string_view path);

// Never throw exceptions in hot paths
// Never ignore errors - propagate or handle explicitly

// Pattern for error propagation:
Result<Scene> load_scene(std::string_view path) {
    auto file = TRY(read_file(path));        // Propagate errors
    auto data = TRY(parse_scene(file));      // Using TRY macro
    return Ok(Scene{std::move(data)});
}
```

### 5. Interface Design

```cpp
// Abstract interfaces for extensibility
class IBackend {
public:
    virtual ~IBackend() = default;
    // Pure virtual methods
};

// Factory functions hide implementation
std::unique_ptr<IBackend> create_backend(BackendType type);

// Dependency injection, not service locators
class System {
    IBackend& m_backend;  // Injected dependency
public:
    explicit System(IBackend& backend) : m_backend(backend) {}
};
```

## Implementation Templates

### void_editor

```cpp
// include/void_engine/editor/editor.hpp
#pragma once

#include <void_engine/core/hot_reload.hpp>
#include <void_engine/ecs/world.hpp>
#include <void_engine/ir/ir.hpp>
#include <void_engine/scene/scene.hpp>

namespace void_editor {

// Forward declarations
class Command;
class Panel;
class Tool;
class Gizmo;

/// Editor selection modes
enum class SelectionMode : uint8_t {
    Single,     ///< Replace selection
    Add,        ///< Add to selection (Shift)
    Toggle,     ///< Toggle in selection (Ctrl)
    Subtract    ///< Remove from selection (Ctrl+Shift)
};

/// Editor snapshot for hot-reload
struct EditorSnapshot {
    static constexpr uint32_t MAGIC = 0x45445452;  // "EDTR"
    static constexpr uint32_t VERSION = 1;

    // Selection
    std::vector<void_ecs::EntityId> selected_entities;
    SelectionMode selection_mode;

    // Cameras
    std::vector<CameraState> viewport_cameras;

    // Tools
    std::string active_tool;
    GizmoMode gizmo_mode;

    // History (serialized)
    std::vector<std::vector<uint8_t>> undo_stack;
    std::vector<std::vector<uint8_t>> redo_stack;
    size_t history_index;

    // Layout
    std::vector<uint8_t> panel_layout;
};

/// Selection manager
class SelectionManager {
public:
    void select(void_ecs::EntityId entity, SelectionMode mode);
    void select_all(std::span<const void_ecs::EntityId> entities);
    void clear();
    void box_select(const AABB& bounds, SelectionMode mode);

    [[nodiscard]] bool is_selected(void_ecs::EntityId entity) const;
    [[nodiscard]] std::span<const void_ecs::EntityId> selected() const;
    [[nodiscard]] size_t count() const;

    // Callbacks
    void on_selection_changed(std::function<void()> callback);

private:
    std::vector<void_ecs::EntityId> m_selected;
    std::vector<std::function<void()>> m_callbacks;
};

/// Command for undo/redo
class Command {
public:
    virtual ~Command() = default;

    /// Execute the command
    [[nodiscard]] virtual void_core::Result<void> execute() = 0;

    /// Undo the command
    [[nodiscard]] virtual void_core::Result<void> undo() = 0;

    /// Get display name
    [[nodiscard]] virtual std::string_view name() const = 0;

    /// Serialize for hot-reload
    [[nodiscard]] virtual std::vector<uint8_t> serialize() const = 0;

    /// Can merge with previous command?
    [[nodiscard]] virtual bool can_merge(const Command& other) const { return false; }

    /// Merge with previous command
    virtual void merge(Command&& other) {}
};

/// Undo history
class UndoHistory {
public:
    explicit UndoHistory(size_t max_size = 100);

    void execute(std::unique_ptr<Command> cmd);
    void undo();
    void redo();
    void clear();

    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::string_view undo_name() const;
    [[nodiscard]] std::string_view redo_name() const;

    // Transaction support (group multiple commands)
    void begin_transaction(std::string_view name);
    void end_transaction();
    void cancel_transaction();

    // Hot-reload
    [[nodiscard]] std::vector<std::vector<uint8_t>> serialize_undo() const;
    [[nodiscard]] std::vector<std::vector<uint8_t>> serialize_redo() const;

private:
    std::vector<std::unique_ptr<Command>> m_undo_stack;
    std::vector<std::unique_ptr<Command>> m_redo_stack;
    size_t m_max_size;

    // Transaction state
    std::vector<std::unique_ptr<Command>> m_transaction;
    std::string m_transaction_name;
    bool m_in_transaction = false;
};

/// Main editor state
class EditorState : public void_core::HotReloadable {
public:
    EditorState(void_ecs::World& world, void_scene::SceneManager& scenes);

    // Core access
    [[nodiscard]] SelectionManager& selection() { return m_selection; }
    [[nodiscard]] UndoHistory& history() { return m_history; }
    [[nodiscard]] void_ecs::World& world() { return m_world; }

    // Commands
    void execute(std::unique_ptr<Command> cmd);

    // void_core::HotReloadable
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;
    [[nodiscard]] void_core::Version current_version() const override;
    [[nodiscard]] std::string type_name() const override { return "EditorState"; }

private:
    void_ecs::World& m_world;
    void_scene::SceneManager& m_scenes;
    SelectionManager m_selection;
    UndoHistory m_history;
};

// Serialization
std::vector<uint8_t> serialize(const EditorSnapshot& snapshot);
std::optional<EditorSnapshot> deserialize(std::span<const uint8_t> data);

} // namespace void_editor
```

### void_network

```cpp
// include/void_engine/network/network.hpp
#pragma once

#include <void_engine/core/hot_reload.hpp>
#include <void_engine/services/services.hpp>

namespace void_network {

/// Network error types
enum class NetworkErrorKind : uint8_t {
    NotConnected,
    ConnectionFailed,
    AuthFailed,
    Timeout,
    ProtocolMismatch,
    MessageTooLarge,
    InvalidMessage,
    Disconnected
};

/// Connection state
enum class ConnectionState : uint8_t {
    Disconnected,
    Connecting,
    Authenticating,
    Connected,
    Reconnecting
};

/// Entity authority
enum class Authority : uint8_t {
    None,       ///< No authority (receive-only)
    Owner,      ///< Full authority (can modify)
    Predicted   ///< Client-side prediction
};

/// Consistency level for replication
enum class ConsistencyLevel : uint8_t {
    Eventual,   ///< Best-effort delivery
    Reliable,   ///< Guaranteed delivery, may reorder
    Ordered     ///< Guaranteed delivery, in order
};

/// Network configuration
struct NetworkConfig {
    uint32_t max_reconnect_attempts = 5;
    std::chrono::milliseconds initial_reconnect_delay{500};
    std::chrono::milliseconds max_reconnect_delay{30000};
    std::chrono::milliseconds ping_interval{15000};
    std::chrono::milliseconds connection_timeout{10000};
    bool enable_compression = true;
    size_t compression_threshold = 1024;
    size_t max_message_size = 1024 * 1024;
};

/// Network statistics
struct NetworkStats {
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    double rtt_ms = 0.0;
    double avg_rtt_ms = 0.0;
    float packet_loss_percent = 0.0f;
    uint32_t reconnect_attempts = 0;
};

/// Entity update for replication
struct EntityUpdate {
    void_ecs::EntityId entity;
    std::vector<uint8_t> component_data;
    uint64_t timestamp;
    uint32_t sequence;
};

/// Network snapshot for hot-reload
struct NetworkSnapshot {
    static constexpr uint32_t MAGIC = 0x4E455457;  // "NETW"
    static constexpr uint32_t VERSION = 1;

    bool is_connected;
    ConnectionState state;
    std::string server_address;
    std::string session_id;

    std::unordered_map<void_ecs::EntityId, Authority> authorities;
    std::unordered_map<void_ecs::EntityId, uint64_t> entity_versions;

    NetworkStats stats;
    std::vector<EntityUpdate> pending_outgoing;
};

/// Abstract transport layer
class ITransport {
public:
    virtual ~ITransport() = default;

    [[nodiscard]] virtual void_core::Result<void> connect(std::string_view address) = 0;
    [[nodiscard]] virtual void_core::Result<void> disconnect() = 0;
    [[nodiscard]] virtual void_core::Result<void> send(std::span<const uint8_t> data) = 0;
    [[nodiscard]] virtual std::optional<std::vector<uint8_t>> receive() = 0;
    [[nodiscard]] virtual bool is_connected() const = 0;
};

/// Entity replicator
class EntityReplicator {
public:
    void claim_authority(void_ecs::EntityId entity);
    void release_authority(void_ecs::EntityId entity);
    [[nodiscard]] Authority get_authority(void_ecs::EntityId entity) const;

    void replicate_component(void_ecs::EntityId entity,
                            std::span<const uint8_t> data,
                            ConsistencyLevel consistency);

    void apply_incoming(const EntityUpdate& update);
    [[nodiscard]] std::vector<EntityUpdate> take_outgoing();

private:
    std::unordered_map<void_ecs::EntityId, Authority> m_authorities;
    std::unordered_map<void_ecs::EntityId, uint64_t> m_versions;
    std::vector<EntityUpdate> m_outgoing;
};

/// Main network service
class NetworkService : public void_services::IService,
                       public void_core::HotReloadable {
public:
    explicit NetworkService(NetworkConfig config = {});

    // Connection
    [[nodiscard]] void_core::Result<void> connect(std::string_view address);
    [[nodiscard]] void_core::Result<void> disconnect();
    [[nodiscard]] bool is_connected() const;
    [[nodiscard]] ConnectionState state() const;

    // Replication
    [[nodiscard]] EntityReplicator& replicator() { return m_replicator; }

    // Statistics
    [[nodiscard]] const NetworkStats& stats() const { return m_stats; }

    // Per-frame tick
    void tick();

    // void_services::IService
    void on_start() override;
    void on_stop() override;
    void on_update(float dt) override;

    // void_core::HotReloadable
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;
    [[nodiscard]] void_core::Version current_version() const override;
    [[nodiscard]] std::string type_name() const override { return "NetworkService"; }

private:
    NetworkConfig m_config;
    std::unique_ptr<ITransport> m_transport;
    EntityReplicator m_replicator;
    NetworkStats m_stats;
    ConnectionState m_state = ConnectionState::Disconnected;
    std::string m_server_address;

    void handle_reconnection();
    std::chrono::milliseconds calculate_backoff() const;
};

// Transport factories
std::unique_ptr<ITransport> create_websocket_transport();
std::unique_ptr<ITransport> create_mock_transport();

// Serialization
std::vector<uint8_t> serialize(const NetworkSnapshot& snapshot);
std::optional<NetworkSnapshot> deserialize(std::span<const uint8_t> data);

} // namespace void_network
```

### Linux Backends

```cpp
// include/void_engine/presenter/drm.hpp
#pragma once

#include <void_engine/presenter/presenter.hpp>
#include <void_engine/core/hot_reload.hpp>

#if defined(__linux__)

namespace void_presenter {

/// DRM presenter snapshot for hot-reload
struct DrmPresenterSnapshot {
    static constexpr uint32_t MAGIC = 0x44524D50;  // "DRMP"
    static constexpr uint32_t VERSION = 1;

    std::string device_path;
    uint32_t width;
    uint32_t height;
    uint32_t refresh_rate;
    uint64_t frame_number;
    PresenterConfig config;
};

/// DRM Presenter - Direct GPU rendering without display server
class DrmPresenter : public IPresenter, public void_core::HotReloadable {
public:
    /// Create presenter for specified DRM device
    [[nodiscard]] static void_core::Result<std::unique_ptr<DrmPresenter>>
        create(std::string_view device_path = "/dev/dri/card0");

    ~DrmPresenter() override;

    // Non-copyable, movable
    DrmPresenter(const DrmPresenter&) = delete;
    DrmPresenter& operator=(const DrmPresenter&) = delete;
    DrmPresenter(DrmPresenter&&) noexcept;
    DrmPresenter& operator=(DrmPresenter&&) noexcept;

    // IPresenter
    [[nodiscard]] PresenterId id() const override;
    [[nodiscard]] const PresenterCapabilities& capabilities() const override;
    [[nodiscard]] const PresenterConfig& config() const override;
    [[nodiscard]] void_core::Result<void> reconfigure(PresenterConfig config) override;
    [[nodiscard]] void_core::Result<void> resize(uint32_t width, uint32_t height) override;
    [[nodiscard]] void_core::Result<Frame> begin_frame() override;
    [[nodiscard]] void_core::Result<void> present(Frame frame) override;
    [[nodiscard]] std::pair<uint32_t, uint32_t> size() const override;
    [[nodiscard]] bool is_valid() const override;

    // void_core::HotReloadable
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;
    [[nodiscard]] void_core::Version current_version() const override;
    [[nodiscard]] std::string type_name() const override { return "DrmPresenter"; }

private:
    explicit DrmPresenter(std::string device_path);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/// Find available DRM device
[[nodiscard]] std::optional<std::string> find_drm_device();

/// List all DRM devices
[[nodiscard]] std::vector<std::string> list_drm_devices();

// Serialization
std::vector<uint8_t> serialize(const DrmPresenterSnapshot& snapshot);
std::optional<DrmPresenterSnapshot> deserialize(std::span<const uint8_t> data);

} // namespace void_presenter

#endif // __linux__
```

### void_hud Hot-Reload

```cpp
// include/void_engine/hud/hot_reload.hpp
#pragma once

#include <void_engine/hud/elements.hpp>
#include <void_engine/core/hot_reload.hpp>

namespace void_hud {

/// Serialized element state
struct ElementSnapshot {
    HudElementId id;
    HudElementType type;
    std::vector<uint8_t> state;
};

/// Animation snapshot
struct AnimationSnapshot {
    AnimationId id;
    HudElementId target;
    float progress;
    float duration;
    bool paused;
    std::vector<uint8_t> keyframe_data;
};

/// HUD system snapshot for hot-reload
struct HudSystemSnapshot {
    static constexpr uint32_t MAGIC = 0x48554453;  // "HUDS"
    static constexpr uint32_t VERSION = 1;

    std::vector<ElementSnapshot> elements;
    std::vector<HudLayerId> layer_order;
    std::vector<AnimationSnapshot> animations;
    std::vector<uint8_t> binding_state;

    // Viewport configuration
    float viewport_width;
    float viewport_height;
    float dpi_scale;
};

/// Make HudElement serializable
template<typename T>
struct ElementSerializer {
    static std::vector<uint8_t> serialize(const T& element);
    static void_core::Result<T> deserialize(std::span<const uint8_t> data);
};

/// Extended HudSystem with hot-reload
class HotReloadableHudSystem : public HudSystem, public void_core::HotReloadable {
public:
    using HudSystem::HudSystem;

    // void_core::HotReloadable
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;
    [[nodiscard]] void_core::Result<void> prepare_reload() override;
    [[nodiscard]] void_core::Result<void> finish_reload() override;
    [[nodiscard]] void_core::Version current_version() const override;
    [[nodiscard]] std::string type_name() const override { return "HudSystem"; }

private:
    HudSystemSnapshot take_system_snapshot() const;
    void_core::Result<void> apply_system_snapshot(const HudSystemSnapshot& snapshot);
};

// Serialization
std::vector<uint8_t> serialize(const HudSystemSnapshot& snapshot);
std::optional<HudSystemSnapshot> deserialize(std::span<const uint8_t> data);

} // namespace void_hud
```

## Code Review Checklist

Before considering any implementation complete, verify:

### Hot-Reload
- [ ] Implements `void_core::HotReloadable` interface
- [ ] Snapshot captures ALL significant state
- [ ] Binary serialization (no text formats)
- [ ] Magic number and version in snapshot
- [ ] Version migration path defined
- [ ] No raw pointers in snapshot (handles only)
- [ ] `prepare_reload()` cleans transient resources
- [ ] `finish_reload()` rebuilds transient resources

### Performance
- [ ] No allocations in frame-critical paths
- [ ] Lock-free where applicable
- [ ] Cache-friendly data layout
- [ ] Pre-allocated buffers used
- [ ] No virtual calls in inner loops
- [ ] SIMD opportunities identified

### Safety
- [ ] No raw `new`/`delete`
- [ ] RAII for all resources
- [ ] `Result<T>` for fallible operations
- [ ] Bounds checking in debug builds
- [ ] No undefined behavior

### Design
- [ ] Interface-based abstractions
- [ ] Factory functions for creation
- [ ] Dependency injection
- [ ] Clear ownership semantics
- [ ] No circular dependencies

### Documentation
- [ ] Doxygen comments on public API
- [ ] Architecture diagram (Mermaid)
- [ ] Usage examples
- [ ] Error handling documented
- [ ] Thread safety documented

### Testing
- [ ] Unit tests for public API
- [ ] Hot-reload test (save/restore cycle)
- [ ] Error path coverage
- [ ] Platform-specific tests (Linux)

## File Naming Conventions

```
include/void_engine/{module}/
├── {module}.hpp           # Main public header
├── fwd.hpp                # Forward declarations
├── types.hpp              # Type definitions
├── hot_reload.hpp         # Hot-reload support (if separate)
└── {subsystem}.hpp        # Additional headers

src/{module}/
├── {module}.cpp           # Main implementation
├── {subsystem}.cpp        # Subsystem implementations
├── hot_reload.cpp         # Serialization implementations
└── CMakeLists.txt         # Module build configuration
```

## CMake Integration

```cmake
# Example for new module
void_add_module(NAME void_network
    SOURCES
        network.cpp
        transport.cpp
        replication.cpp
        hot_reload.cpp
    DEPENDENCIES
        void_core
        void_services
        void_event
)

# Platform-specific sources
if(UNIX AND NOT APPLE)
    target_sources(void_presenter PRIVATE
        drm_presenter.cpp
    )
    target_link_libraries(void_presenter PRIVATE
        drm
        gbm
    )
    target_compile_definitions(void_presenter PRIVATE
        VOID_HAS_DRM
    )
endif()
```
