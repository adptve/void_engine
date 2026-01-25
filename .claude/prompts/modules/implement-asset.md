# Implement void_asset Module

> **You own**: `src/asset/` and `include/void_engine/asset/`
> **Do NOT modify** any other directories
> **Depends on**: void_core (assume it exists)

---

## YOUR TASK

Implement these 7 headers:

| Header | Create |
|--------|--------|
| `asset.hpp` | `src/asset/asset.cpp` |
| `cache.hpp` | `src/asset/cache.cpp` |
| `handle.hpp` | `src/asset/handle.cpp` |
| `loader.hpp` | `src/asset/loader.cpp` |
| `server.hpp` | `src/asset/server.cpp` |
| `storage.hpp` | `src/asset/storage.cpp` |

---

## REQUIREMENTS

### Hot-Reload
```cpp
struct AssetServerSnapshot {
    static constexpr uint32_t MAGIC = 0x41535456;  // "ASTV"
    static constexpr uint32_t VERSION = 1;
    // Asset manifest (paths, types, ref counts)
    // Loaded asset HANDLES (not data)
    // Cache configuration
};
```

### 3-Tier Cache
1. Hot cache (in memory, recently used)
2. Warm cache (compressed, quick decompress)
3. Cold storage (disk, async load)

### Performance
- Async loading with callbacks
- Reference counting for lifetime
- Memory budget enforcement

---

## START

```
Read include/void_engine/asset/asset.hpp
```
