# void_ir Module

Intermediate Representation for declarative state changes via patches and transactions.

## Overview

Applications never mutate state directly. Instead, they emit **patches** that describe what changes should be made. The kernel validates and applies patches atomically, enabling rollback, audit logging, and crash recovery.

## Patch Types

```cpp
namespace void_engine::ir {

enum class PatchKind {
    Entity,
    Component,
    Layer,
    Asset
};

// Entity operations
enum class EntityOp {
    Create,
    Destroy,
    Enable,
    Disable,
    SetParent,
    AddTag,
    RemoveTag
};

struct EntityPatch {
    EntityRef entity;
    EntityOp op;
    std::optional<EntityRef> parent;
    std::optional<std::string> archetype;
    std::optional<std::string> tag;
};

// Component operations
enum class ComponentOp {
    Set,     // Add or replace
    Update,  // Partial update
    Remove
};

struct ComponentPatch {
    EntityRef entity;
    std::string component_name;
    ComponentOp op;
    std::optional<Value> data;  // JSON-like value
};

// Layer operations
enum class LayerOp {
    Create,
    Update,
    Destroy
};

struct LayerPatch {
    LayerId layer_id;
    LayerOp op;
    std::optional<LayerConfig> config;
};

// Asset operations
enum class AssetOp {
    Load,
    Unload,
    Update
};

struct AssetPatch {
    AssetId asset_id;
    AssetOp op;
    std::optional<std::filesystem::path> path;
    std::optional<std::string> asset_type;
};

// Unified patch type
struct Patch {
    PatchKind kind;
    std::variant<EntityPatch, ComponentPatch, LayerPatch, AssetPatch> data;

    static Patch create_entity(EntityRef entity, std::string_view archetype);
    static Patch destroy_entity(EntityRef entity);
    static Patch set_component(EntityRef entity, std::string_view name, Value data);
    static Patch remove_component(EntityRef entity, std::string_view name);
    static Patch create_layer(LayerId id, LayerConfig config);
    static Patch load_asset(AssetId id, std::filesystem::path path);
};

} // namespace void_engine::ir
```

## Transactions

Atomic groups of patches:

```cpp
namespace void_engine::ir {

struct Transaction {
    std::vector<Patch> patches;
    NamespaceId source;
    std::string description;
    uint64_t sequence_number;
    TimePoint timestamp;
};

class TransactionBuilder {
public:
    explicit TransactionBuilder(NamespaceId source);

    TransactionBuilder& description(std::string desc);

    TransactionBuilder& create_entity(EntityRef entity, std::string_view archetype);
    TransactionBuilder& destroy_entity(EntityRef entity);

    TransactionBuilder& set_component(EntityRef entity, std::string_view name, Value data);
    TransactionBuilder& update_component(EntityRef entity, std::string_view name, Value partial);
    TransactionBuilder& remove_component(EntityRef entity, std::string_view name);

    TransactionBuilder& create_layer(LayerId id, LayerConfig config);
    TransactionBuilder& update_layer(LayerId id, LayerConfig config);
    TransactionBuilder& destroy_layer(LayerId id);

    TransactionBuilder& load_asset(AssetId id, std::filesystem::path path);
    TransactionBuilder& unload_asset(AssetId id);

    Transaction build();

private:
    Transaction m_transaction;
};

} // namespace void_engine::ir
```

## Patch Bus

Namespace-scoped patch queue and dispatch:

```cpp
namespace void_engine::ir {

class NamespaceHandle {
public:
    NamespaceId id() const;

    void submit(Transaction tx);
    void submit_patch(Patch patch);

    // Quotas
    size_t patches_remaining() const;
    bool can_submit() const;

private:
    NamespaceId m_id;
    PatchBus* m_bus;
};

class PatchBus {
public:
    // Namespace management
    NamespaceHandle register_namespace(Namespace ns);
    void unregister_namespace(NamespaceId id);

    // Submission
    void submit(Transaction tx);

    // Processing (called by kernel each frame)
    std::vector<Transaction> drain_pending();
    void process_transactions(std::span<Transaction const> txs, World& world);

    // Validation
    ValidationResult validate(Transaction const& tx) const;
    ValidationResult validate_patch(Patch const& patch, NamespaceId source) const;

private:
    std::unordered_map<NamespaceId, NamespaceQueue> m_queues;
    std::mutex m_mutex;
};

struct ValidationResult {
    bool valid;
    std::string error_message;
    std::vector<size_t> failed_patch_indices;
};

} // namespace void_engine::ir
```

## Patch Optimization

Merge and eliminate redundant patches:

```cpp
namespace void_engine::ir {

class PatchOptimizer {
public:
    // Merge compatible patches
    std::vector<Patch> merge(std::span<Patch const> patches);

    // Eliminate no-ops (e.g., create then destroy same entity)
    std::vector<Patch> eliminate_noop(std::span<Patch const> patches);

    // Reorder for better performance (creates before modifications)
    std::vector<Patch> reorder(std::span<Patch const> patches);

    // Full optimization pipeline
    std::vector<Patch> optimize(std::span<Patch const> patches);
};

} // namespace void_engine::ir
```

## Snapshot and Rollback

```cpp
namespace void_engine::ir {

struct WorldSnapshot {
    uint64_t frame_number;
    std::vector<uint8_t> entity_data;
    std::vector<uint8_t> component_data;
    std::vector<uint8_t> resource_data;
};

class SnapshotManager {
public:
    explicit SnapshotManager(size_t max_snapshots = 3);

    void capture(World const& world, uint64_t frame);
    bool rollback_to(World& world, uint64_t frame);

    std::optional<uint64_t> latest_frame() const;
    std::vector<uint64_t> available_frames() const;

private:
    size_t m_max_snapshots;
    std::deque<WorldSnapshot> m_snapshots;
};

} // namespace void_engine::ir
```

## Value Type

JSON-like dynamic value for component data:

```cpp
namespace void_engine::ir {

class Value {
public:
    using Null = std::monostate;
    using Bool = bool;
    using Int = int64_t;
    using Float = double;
    using String = std::string;
    using Array = std::vector<Value>;
    using Object = std::unordered_map<std::string, Value>;

    Value() = default;  // Null
    Value(bool b);
    Value(int64_t i);
    Value(double f);
    Value(std::string s);
    Value(Array arr);
    Value(Object obj);

    bool is_null() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_float() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    bool as_bool() const;
    int64_t as_int() const;
    double as_float() const;
    std::string const& as_string() const;
    Array const& as_array() const;
    Object const& as_object() const;

    // Object access
    Value const& operator[](std::string_view key) const;
    Value& operator[](std::string_view key);
    bool contains(std::string_view key) const;

    // Array access
    Value const& operator[](size_t index) const;
    Value& operator[](size_t index);
    size_t size() const;

    // Serialization
    static Value from_json(std::string_view json);
    std::string to_json() const;

private:
    std::variant<Null, Bool, Int, Float, String, Array, Object> m_data;
};

} // namespace void_engine::ir
```

## Usage Examples

### Building and Submitting Transactions

```cpp
// Get namespace handle
auto handle = patch_bus.register_namespace(my_namespace);

// Build transaction
auto tx = TransactionBuilder(handle.id())
    .description("Spawn player with equipment")
    .create_entity(player_ref, "Player")
    .set_component(player_ref, "Transform", Value::Object{
        {"position", Value::Array{0.0, 0.0, 0.0}},
        {"rotation", Value::Array{0.0, 0.0, 0.0, 1.0}},
        {"scale", Value::Array{1.0, 1.0, 1.0}}
    })
    .set_component(player_ref, "Health", Value::Object{
        {"max", 100},
        {"current", 100}
    })
    .build();

// Submit
handle.submit(tx);
```

### Processing in Kernel

```cpp
void Kernel::process_frame() {
    // Drain pending transactions
    auto transactions = m_patch_bus.drain_pending();

    // Validate all
    for (auto& tx : transactions) {
        auto result = m_patch_bus.validate(tx);
        if (!result.valid) {
            log_error("Transaction validation failed: {}", result.error_message);
            continue;
        }
    }

    // Capture snapshot for rollback
    m_snapshots.capture(m_world, m_frame_number);

    // Apply transactions
    m_patch_bus.process_transactions(transactions, m_world);
}
```
