# Migration Skill

You are an expert in code migration, API transitions, and dependency upgrades.

## Migration Strategies

### Strangler Fig Pattern

```
Phase 1: Introduce new alongside old
┌─────────────────────────────────────┐
│           Application               │
│    ┌─────────┐    ┌─────────┐      │
│    │   Old   │    │   New   │      │
│    │ System  │    │ System  │      │
│    └─────────┘    └─────────┘      │
│         │              │            │
│         └──────┬───────┘            │
│                ▼                    │
│          Router/Facade              │
└─────────────────────────────────────┘

Phase 2: Gradually route traffic to new
Phase 3: Remove old system
```

### Parallel Implementation

```cpp
// Run both implementations, compare results
template<typename T>
T migrate_with_verification(auto old_impl, auto new_impl) {
    auto old_result = old_impl();
    auto new_result = new_impl();

    if (old_result != new_result) {
        log_divergence(old_result, new_result);
        return old_result;  // Fallback to known behavior
    }
    return new_result;
}
```

### API Deprecation Cycle

```cpp
// Phase 1: Mark deprecated
[[deprecated("Use new_function() instead")]]
void old_function();

// Phase 2: Emit runtime warnings
void old_function() {
    static bool warned = false;
    if (!warned) {
        log_warning("old_function() is deprecated");
        warned = true;
    }
    // ... implementation
}

// Phase 3: Redirect to new implementation
void old_function() {
    new_function(default_params);
}

// Phase 4: Remove
// Delete old_function entirely
```

## C++ Standard Migration

### C++11 → C++14 → C++17 → C++20

```cpp
// C++11 → C++14
auto lambda = [](auto x) { return x * 2; };  // Generic lambdas
constexpr int factorial(int n) {              // Relaxed constexpr
    int result = 1;
    for (int i = 2; i <= n; ++i) result *= i;
    return result;
}

// C++14 → C++17
if (auto it = map.find(key); it != map.end()) {  // if with initializer
    use(it->second);
}
auto [key, value] = *map.begin();  // Structured bindings
std::optional<T> maybe_value;       // std::optional

// C++17 → C++20
template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

auto result = data | std::views::filter(pred);  // Ranges
std::span<int> view(arr);                        // std::span
```

### Modernization Patterns

```cpp
// Raw pointer → smart pointer
// Before
Widget* create() { return new Widget(); }
void use(Widget* w) { delete w; }

// After
std::unique_ptr<Widget> create() {
    return std::make_unique<Widget>();
}
void use(std::unique_ptr<Widget> w) { /* auto-deleted */ }

// Manual loop → algorithm
// Before
bool found = false;
for (auto& x : vec) {
    if (x == target) { found = true; break; }
}

// After
bool found = std::ranges::find(vec, target) != vec.end();
// or
bool found = std::ranges::contains(vec, target);  // C++23

// Callback → std::function / template
// Before
typedef void (*Callback)(int);
void register(Callback cb);

// After
void register(std::function<void(int)> cb);
// or
template<typename F> void register(F&& cb);
```

## Library Migration

### Dependency Replacement

```cpp
// 1. Create abstraction layer
namespace graphics {
    class ITexture {
    public:
        virtual ~ITexture() = default;
        virtual void bind(int slot) = 0;
    };
}

// 2. Implement for old library
class OldLibTexture : public ITexture { /* ... */ };

// 3. Implement for new library
class NewLibTexture : public ITexture { /* ... */ };

// 4. Switch implementation via factory
std::unique_ptr<ITexture> create_texture(/* ... */) {
#if USE_NEW_GRAPHICS
    return std::make_unique<NewLibTexture>(/* ... */);
#else
    return std::make_unique<OldLibTexture>(/* ... */);
#endif
}
```

### CMake Migration

```cmake
# Old: manual flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
include_directories(${SOME_INCLUDE})
link_libraries(${SOME_LIB})

# New: modern CMake targets
target_compile_features(mylib PUBLIC cxx_std_20)
target_include_directories(mylib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mylib PUBLIC dependency::dependency)
```

## Data Migration

### Schema Evolution

```cpp
// Versioned serialization
struct SaveData {
    static constexpr uint32_t k_current_version = 3;
    uint32_t version;
    // ... fields
};

SaveData load(std::istream& in) {
    SaveData data;
    read(in, data.version);

    switch (data.version) {
        case 1: load_v1(in, data); migrate_v1_to_v2(data); [[fallthrough]];
        case 2: migrate_v2_to_v3(data); [[fallthrough]];
        case 3: break;  // Current version
        default: throw UnsupportedVersion{data.version};
    }
    return data;
}

void migrate_v1_to_v2(SaveData& data) {
    data.new_field = compute_default(data.old_field);
}
```

### Asset Pipeline Migration

```cpp
// Convert old format to new during build
class AssetConverter {
public:
    void convert(fs::path old_asset, fs::path new_asset) {
        auto old_data = load_old_format(old_asset);
        auto new_data = transform(old_data);
        save_new_format(new_asset, new_data);
    }
};

// Runtime fallback for unconverted assets
Asset load_asset(fs::path path) {
    if (path.extension() == ".old") {
        return convert_and_load(path);
    }
    return load_native(path);
}
```

## Migration Checklist

### Before Migration
- [ ] Full test coverage on affected code
- [ ] Document current behavior
- [ ] Identify all call sites
- [ ] Plan rollback strategy

### During Migration
- [ ] Feature flags for gradual rollout
- [ ] Parallel run with comparison logging
- [ ] Monitor error rates and performance
- [ ] Keep old code path available

### After Migration
- [ ] Remove deprecated code
- [ ] Update documentation
- [ ] Remove feature flags
- [ ] Archive migration scripts
