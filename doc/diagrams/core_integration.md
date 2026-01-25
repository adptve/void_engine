# void_core Module Integration Diagrams

## Class Diagram

```mermaid
classDiagram
    class Error {
        -ErrorCode m_code
        -Variant m_error
        -map~string,string~ m_context
        +code() ErrorCode
        +message() string
        +is~T~() bool
        +as~T~() T*
        +with_context(key, value) Error
    }

    class Result~T,E~ {
        -optional~T~ m_value
        -E m_error
        +is_ok() bool
        +is_err() bool
        +value() T
        +error() E
        +unwrap() T
        +map~F~(func) Result~U,E~
    }

    class Version {
        +uint16_t major
        +uint16_t minor
        +uint16_t patch
        +is_compatible_with(other) bool
        +parse(str) optional~Version~
        +to_u64() uint64_t
        +to_string() string
    }

    class Id {
        +uint64_t bits
        +create(index, generation) Id
        +is_null() bool
        +index() uint32_t
        +generation() uint32_t
    }

    class IdGenerator {
        -atomic~uint64_t~ m_next
        +next() Id
        +next_batch(count) Id
        +current() uint64_t
    }

    class NamedId {
        +string name
        +uint64_t hash
        +to_id() Id
    }

    class Handle~T~ {
        +uint32_t bits
        +create(index, generation) Handle~T~
        +is_null() bool
        +index() uint32_t
        +generation() uint8_t
    }

    class HandleAllocator~T~ {
        -vector~uint8_t~ m_generations
        -vector~uint32_t~ m_free_list
        +allocate() Handle~T~
        +free(handle) bool
        +is_valid(handle) bool
    }

    class HandleMap~T~ {
        -HandleAllocator~T~ m_allocator
        -vector~optional~T~~ m_values
        +insert(value) Handle~T~
        +remove(handle) optional~T~
        +get(handle) T*
    }

    class TypeInfo {
        +type_index type_id
        +string name
        +size_t size
        +size_t align
        +bool needs_drop
        +optional~TypeSchema~ schema
    }

    class TypeRegistry {
        -map m_by_id
        -map m_by_name
        -map m_constructors
        +register_type~T~() TypeRegistry
        +get~T~() TypeInfo*
        +create(type_id) unique_ptr~DynType~
    }

    class HotReloadable {
        <<interface>>
        +snapshot() Result~HotReloadSnapshot~
        +restore(snapshot) Result~void~
        +is_compatible(version) bool
        +prepare_reload() Result~void~
        +finish_reload() Result~void~
        +current_version() Version
        +type_name() string
    }

    class HotReloadSnapshot {
        +vector~uint8_t~ data
        +type_index type_id
        +string type_name
        +Version version
        +map~string,string~ metadata
    }

    class HotReloadManager {
        -map m_entries
        -map m_path_to_name
        -map m_pending_snapshots
        -queue m_pending_events
        -mutex m_queue_mutex
        +register_object(name, object) Result~void~
        +reload(name) Result~void~
        +complete_reload(name, new_object) Result~void~
        +process_pending() vector~Result~void~~
    }

    class FileWatcher {
        <<interface>>
        +watch(path) Result~void~
        +unwatch(path) Result~void~
        +poll() vector~ReloadEvent~
    }

    class Plugin {
        <<interface>>
        +id() PluginId
        +version() Version
        +dependencies() vector~PluginId~
        +on_load(ctx) Result~void~
        +on_unload(ctx) Result~PluginState~
        +on_reload(ctx, state) Result~void~
        +supports_hot_reload() bool
    }

    class PluginRegistry {
        -map m_plugins
        -map m_info
        -vector m_load_order
        +register_plugin(plugin) Result~void~
        +load(id, types) Result~void~
        +unload(id, types) Result~PluginState~
        +hot_reload(id, new_plugin, types) Result~void~
    }

    Error <|-- PluginError
    Error <|-- TypeRegistryError
    Error <|-- HotReloadError
    Error <|-- HandleError

    HandleAllocator~T~ --* HandleMap~T~
    Handle~T~ <.. HandleAllocator~T~ : creates
    Handle~T~ <.. HandleMap~T~ : uses

    HotReloadSnapshot <.. HotReloadable : produces
    HotReloadManager o-- HotReloadable : manages
    HotReloadManager ..> HotReloadSnapshot : stores

    FileWatcher <|-- MemoryFileWatcher
    FileWatcher <|-- PollingFileWatcher
    HotReloadSystem o-- HotReloadManager
    HotReloadSystem o-- FileWatcher

    Plugin ..> PluginState : produces
    Plugin ..> Version : uses
    Plugin ..> PluginId : uses
    PluginRegistry o-- Plugin : manages
    PluginRegistry ..> TypeRegistry : uses

    TypeRegistry o-- TypeInfo : stores
    TypeInfo ..> TypeSchema : contains
```

## Hot-Reload Sequence Diagram

```mermaid
sequenceDiagram
    participant FileWatcher
    participant HotReloadSystem
    participant HotReloadManager
    participant HotReloadable
    participant NewImplementation

    FileWatcher->>HotReloadSystem: poll() returns ReloadEvent::FileModified
    HotReloadSystem->>HotReloadManager: queue_event(event)
    HotReloadSystem->>HotReloadManager: process_pending()

    HotReloadManager->>HotReloadable: prepare_reload()
    Note over HotReloadable: Release GPU handles,<br/>file handles, etc.
    HotReloadable-->>HotReloadManager: Ok()

    HotReloadManager->>HotReloadable: snapshot()
    Note over HotReloadable: Serialize state to binary:<br/>MAGIC | VERSION | DATA
    HotReloadable-->>HotReloadManager: HotReloadSnapshot

    Note over HotReloadManager: Store snapshot,<br/>mark pending_reload = true

    Note over External: DLL/SO recompilation<br/>and loading

    External->>HotReloadManager: complete_reload(name, new_object)

    HotReloadManager->>NewImplementation: is_compatible(snapshot.version)
    NewImplementation-->>HotReloadManager: true

    HotReloadManager->>NewImplementation: restore(snapshot)
    Note over NewImplementation: Deserialize binary state,<br/>verify MAGIC/VERSION,<br/>restore internal state
    NewImplementation-->>HotReloadManager: Ok()

    HotReloadManager->>NewImplementation: finish_reload()
    Note over NewImplementation: Rebuild GPU handles,<br/>reopen files, etc.
    NewImplementation-->>HotReloadManager: Ok()

    Note over HotReloadManager: Replace object,<br/>update version,<br/>notify callbacks
```

## Plugin Lifecycle Sequence Diagram

```mermaid
sequenceDiagram
    participant Engine
    participant PluginRegistry
    participant Plugin
    participant TypeRegistry

    Engine->>PluginRegistry: register_plugin(plugin)
    Note over PluginRegistry: Store plugin,<br/>status = Registered

    Engine->>PluginRegistry: load(id, types)
    PluginRegistry->>PluginRegistry: Check dependencies

    PluginRegistry->>Plugin: register_types(types)
    Plugin->>TypeRegistry: register_type<MyComponent>()
    TypeRegistry-->>Plugin: Ok

    Note over PluginRegistry: status = Loading

    PluginRegistry->>Plugin: on_load(ctx)
    Note over Plugin: Initialize resources,<br/>register systems, etc.
    Plugin-->>PluginRegistry: Ok()

    Note over PluginRegistry: status = Active,<br/>add to load_order

    loop Every Frame
        Engine->>PluginRegistry: update_all(dt)
        PluginRegistry->>Plugin: on_update(dt)
    end

    Engine->>PluginRegistry: unload(id, types)
    Note over PluginRegistry: status = Unloading

    PluginRegistry->>Plugin: on_unload(ctx)
    Note over Plugin: Cleanup,<br/>serialize state
    Plugin-->>PluginRegistry: PluginState

    Note over PluginRegistry: status = Registered,<br/>remove from load_order
```

## Dependency Graph

```mermaid
graph TB
    subgraph void_core["void_core Module"]
        error["error.hpp/.cpp<br/>Error, Result&lt;T&gt;"]
        version["version.hpp/.cpp<br/>Version"]
        id["id.hpp/.cpp<br/>Id, IdGenerator, NamedId"]
        handle["handle.hpp/.cpp<br/>Handle&lt;T&gt;, HandleMap&lt;T&gt;"]
        log["log.hpp/.cpp<br/>Logging"]
        type_registry["type_registry.hpp/.cpp<br/>TypeRegistry, TypeInfo"]
        hot_reload["hot_reload.hpp/.cpp<br/>HotReloadable, HotReloadManager"]
        plugin["plugin.hpp/.cpp<br/>Plugin, PluginRegistry"]
        fwd["fwd.hpp<br/>Forward Declarations"]
    end

    subgraph external["External Dependencies"]
        spdlog["spdlog"]
        std["C++ STL"]
    end

    subgraph dependents["Dependent Modules"]
        void_ecs["void_ecs"]
        void_render["void_render"]
        void_asset["void_asset"]
        void_script["void_script"]
    end

    fwd --> error
    fwd --> version
    fwd --> id
    fwd --> handle
    fwd --> hot_reload
    fwd --> plugin

    error --> version
    hot_reload --> error
    hot_reload --> version
    plugin --> error
    plugin --> version
    plugin --> id
    plugin --> type_registry
    type_registry --> error
    id --> error
    handle --> error
    log --> spdlog

    void_ecs --> handle
    void_ecs --> id
    void_ecs --> type_registry
    void_ecs --> hot_reload

    void_render --> handle
    void_render --> hot_reload

    void_asset --> handle
    void_asset --> hot_reload
    void_asset --> plugin

    void_script --> plugin
    void_script --> type_registry
    void_script --> hot_reload
```

## Binary Serialization Format

```mermaid
graph LR
    subgraph "HotReloadSnapshot Binary Format"
        A["MAGIC (4 bytes)<br/>0x484F5453 'HOTS'"]
        B["VERSION (4 bytes)"]
        C["Snapshot Version (8 bytes)<br/>as u64"]
        D["Type Name Length (4 bytes)"]
        E["Type Name (N bytes)"]
        F["Data Length (8 bytes)"]
        G["Data Payload (M bytes)"]
        H["Metadata Count (4 bytes)"]
        I["Metadata Entries..."]

        A --> B --> C --> D --> E --> F --> G --> H --> I
    end
```

```mermaid
graph LR
    subgraph "Handle Binary Format"
        A["MAGIC (4 bytes)<br/>0x484E444C 'HNDL'"]
        B["VERSION (4 bytes)"]
        C["Handle Bits (4 bytes)<br/>[Gen:8 | Index:24]"]

        A --> B --> C
    end
```

## Module Initialization Flow

```mermaid
flowchart TD
    A[Engine Start] --> B[init_logging]
    B --> C[register_builtin_types]
    C --> D[Initialize global_type_registry]
    D --> E[Initialize global_plugin_registry]
    E --> F[Initialize global_hot_reload_system]
    F --> G[Load Plugins]
    G --> H{Plugin has dependencies?}
    H -->|Yes| I[Resolve load order]
    I --> J[Load in order]
    H -->|No| J
    J --> K[Plugin.register_types]
    K --> L[Plugin.on_load]
    L --> M[Engine Running]
    M --> N{Shutdown?}
    N -->|No| O{File Changed?}
    O -->|Yes| P[Hot-Reload Flow]
    P --> M
    O -->|No| Q[Plugin.on_update]
    Q --> M
    N -->|Yes| R[Plugin.on_unload in reverse order]
    R --> S[shutdown_plugin_registry]
    S --> T[shutdown_hot_reload_system]
    T --> U[shutdown_logging]
    U --> V[Engine Stop]
```
