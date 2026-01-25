# void_asset Integration Diagram

## Module Overview

The `void_asset` module provides comprehensive asset management for Void Engine with:
- Type-safe handles with reference counting
- Extensible loader system
- Tiered caching (hot/warm/cold)
- Asynchronous loading
- Hot-reload support
- Garbage collection

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         void_asset Module                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                      AssetServer                             │   │
│  │  - Coordinates all asset operations                          │   │
│  │  - Manages loader registry                                   │   │
│  │  - Processes async loads                                     │   │
│  │  - Emits events                                              │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                               │                                     │
│         ┌─────────────────────┼─────────────────────┐              │
│         │                     │                     │              │
│         ▼                     ▼                     ▼              │
│  ┌─────────────┐       ┌─────────────┐       ┌─────────────┐      │
│  │LoaderRegistry│       │AssetStorage │       │TieredCache  │      │
│  │             │       │             │       │             │      │
│  │ErasedLoader │       │AssetEntry   │       │HotCache     │      │
│  │TypedLoader  │       │HandleData   │       │WarmCache    │      │
│  │BytesLoader  │       │Metadata     │       │(disk)       │      │
│  │TextLoader   │       │             │       │             │      │
│  └─────────────┘       └─────────────┘       └─────────────┘      │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Handle System                              │   │
│  │                                                               │   │
│  │   Handle<T>    WeakHandle<T>    UntypedHandle    AssetRef<T> │   │
│  │      ▲              ▲                                         │   │
│  │      │              │                                         │   │
│  │      └──────────────┴─────────> HandleData (shared)          │   │
│  │                                  - strong_count               │   │
│  │                                  - weak_count                 │   │
│  │                                  - generation                 │   │
│  │                                  - state                      │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                   Hot-Reload System                           │   │
│  │                                                               │   │
│  │   PollingAssetWatcher  ──> AssetHotReloadManager              │   │
│  │          │                         │                          │   │
│  │   FileChangeEvent         AssetReloadResult                   │   │
│  │                                                               │   │
│  │   AssetHotReloadSystem (combined server + reload)            │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## Component Interactions

### Loading Flow

```
User Code                AssetServer              Loader              Storage
    │                         │                     │                    │
    │ load<T>("path")         │                     │                    │
    │────────────────────────>│                     │                    │
    │                         │                     │                    │
    │                         │ allocate_id()       │                    │
    │                         │────────────────────>│                    │
    │                         │<────────────────────│                    │
    │                         │                     │                    │
    │                         │ register_asset()    │                    │
    │                         │────────────────────────────────────────>│
    │                         │                     │                    │
    │ Handle<T> (Loading)     │                     │                    │
    │<────────────────────────│                     │                    │
    │                         │                     │                    │
    │                         │     [async]         │                    │
    │                         │ read_file()         │                    │
    │                         │ load_erased()       │                    │
    │                         │────────────────────>│                    │
    │                         │                     │                    │
    │                         │ std::unique_ptr<T>  │                    │
    │                         │<────────────────────│                    │
    │                         │                     │                    │
    │                         │ store()             │                    │
    │                         │────────────────────────────────────────>│
    │                         │                     │                    │
    │                         │ emit(Loaded)        │                    │
    │                         │                     │                    │
    │ handle.is_loaded()=true │                     │                    │
    │<────────────────────────│                     │                    │
```

### Caching Flow

```
┌───────────────────────────────────────────────────────────────┐
│                      Cache Lookup                              │
│                                                                │
│   Request ───> Hot Cache (memory) ───> [HIT] ───> Return      │
│                      │                                         │
│                  [MISS]                                        │
│                      │                                         │
│                      ▼                                         │
│                Warm Cache (disk) ───> [HIT] ───> Promote       │
│                      │                          to Hot         │
│                  [MISS]                                        │
│                      │                                         │
│                      ▼                                         │
│                Cold (load from source)                         │
│                      │                                         │
│                      ▼                                         │
│             Store in Hot Cache ───> Eviction?                  │
│                                          │                     │
│                                          ▼                     │
│                                   Move to Warm Cache           │
└───────────────────────────────────────────────────────────────┘
```

## Dependencies

```
void_asset
    │
    ├── void_core (types, error, version, hot_reload)
    │       │
    │       ├── Result<T, E>
    │       ├── Error / ErrorCode
    │       ├── Version
    │       ├── HotReloadable
    │       └── Id, NamedId
    │
    └── void_structures (optional)
            │
            └── (future: specialized containers)
```

## Key Types

| Type | Description | Location |
|------|-------------|----------|
| `AssetId` | Unique asset identifier | types.hpp |
| `AssetPath` | Normalized asset path | types.hpp |
| `Handle<T>` | Strong reference to asset | handle.hpp |
| `WeakHandle<T>` | Non-owning reference | handle.hpp |
| `AssetLoader<T>` | Type-specific loader interface | loader.hpp |
| `LoadContext` | Data passed to loaders | loader.hpp |
| `AssetStorage` | Central asset storage | storage.hpp |
| `AssetServer` | Main coordination point | server.hpp |
| `TieredCache` | Hot/warm cache system | cache.hpp |

## Event System

```
AssetEventType
├── Loaded      - Asset finished loading
├── Failed      - Load failed with error
├── Reloaded    - Asset was hot-reloaded
├── Unloaded    - Asset was unloaded
└── FileChanged - Source file changed (hot-reload)
```

## Hot-Reload Support

The `void_asset` module implements `void_core::HotReloadable`:

1. **Snapshot**: Serializes asset manifest (paths, IDs, ref counts)
2. **Restore**: Rebuilds manifest, queues assets for reload
3. **Handle Survival**: Handles point to stable IDs that survive reload

```cpp
// Server implements HotReloadable through adapter
auto reloadable = make_hot_reloadable(server);

// Register with hot-reload manager
void_core::global_hot_reload_system().register_watched(
    "asset_server", std::move(reloadable), "");
```

## Memory Management

- **Reference Counting**: Strong/weak counts in HandleData
- **Garbage Collection**: `collect_garbage()` finds unreferenced assets
- **Cache Eviction**: LRU with priority hints (Essential > High > Normal > Low)
- **Memory Budget**: Configurable hot cache size limit

## Thread Safety

| Component | Thread Safety |
|-----------|---------------|
| AssetStorage | Full (shared_mutex) |
| LoaderRegistry | Read-safe after init |
| TieredCache | Full (shared_mutex per tier) |
| Handle<T> | Atomic ref counts |
| AssetServer | Process from one thread |
