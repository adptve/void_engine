#pragma once

/// @file snapshot.hpp
/// @brief Snapshot and rollback system for void_ir

#include "fwd.hpp"
#include "namespace.hpp"
#include "value.hpp"
#include "patch.hpp"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <memory>

namespace void_ir {

// SnapshotId is defined in fwd.hpp

// =============================================================================
// EntitySnapshot
// =============================================================================

/// Snapshot of a single entity's state
struct EntitySnapshot {
    EntityRef entity;
    std::string name;
    bool enabled = true;
    std::unordered_map<std::string, Value> components;

    /// Check if entity has component
    [[nodiscard]] bool has_component(const std::string& type) const {
        return components.find(type) != components.end();
    }

    /// Get component value
    [[nodiscard]] const Value* get_component(const std::string& type) const {
        auto it = components.find(type);
        if (it == components.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Clone the snapshot
    [[nodiscard]] EntitySnapshot clone() const {
        EntitySnapshot s;
        s.entity = entity;
        s.name = name;
        s.enabled = enabled;
        s.components = components;
        return s;
    }
};

// =============================================================================
// LayerSnapshot
// =============================================================================

/// Snapshot of a layer
struct LayerSnapshot {
    LayerId layer;
    std::string name;
    std::int32_t order = 0;
    bool visible = true;
    bool locked = false;
    std::vector<EntityRef> entities;

    [[nodiscard]] LayerSnapshot clone() const {
        return *this;
    }
};

// =============================================================================
// HierarchySnapshot
// =============================================================================

/// Snapshot of parent-child relationships
struct HierarchySnapshot {
    std::unordered_map<std::uint64_t, EntityRef> parents;  // entity_id -> parent
    std::unordered_map<std::uint64_t, std::vector<EntityRef>> children;  // entity_id -> children

    /// Get parent of entity
    [[nodiscard]] std::optional<EntityRef> get_parent(EntityRef entity) const {
        auto it = parents.find(entity.entity_id);
        if (it == parents.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    /// Get children of entity
    [[nodiscard]] const std::vector<EntityRef>* get_children(EntityRef entity) const {
        auto it = children.find(entity.entity_id);
        if (it == children.end()) {
            return nullptr;
        }
        return &it->second;
    }
};

// =============================================================================
// Snapshot
// =============================================================================

/// Full snapshot of namespace state
class Snapshot {
public:
    /// Construct with ID
    explicit Snapshot(SnapshotId id, NamespaceId ns)
        : m_id(id)
        , m_namespace(ns)
        , m_timestamp(std::chrono::steady_clock::now()) {}

    /// Get snapshot ID
    [[nodiscard]] SnapshotId id() const noexcept { return m_id; }

    /// Get namespace
    [[nodiscard]] NamespaceId namespace_id() const noexcept { return m_namespace; }

    /// Get timestamp
    [[nodiscard]] std::chrono::steady_clock::time_point timestamp() const noexcept {
        return m_timestamp;
    }

    /// Get description
    [[nodiscard]] const std::string& description() const noexcept {
        return m_description;
    }

    /// Set description
    void set_description(std::string desc) {
        m_description = std::move(desc);
    }

    // -------------------------------------------------------------------------
    // Entity access
    // -------------------------------------------------------------------------

    /// Get all entity snapshots
    [[nodiscard]] const std::unordered_map<std::uint64_t, EntitySnapshot>& entities() const {
        return m_entities;
    }

    /// Get entity snapshot
    [[nodiscard]] const EntitySnapshot* get_entity(EntityRef ref) const {
        auto it = m_entities.find(ref.entity_id);
        if (it == m_entities.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Add entity snapshot
    void add_entity(EntitySnapshot snapshot) {
        m_entities[snapshot.entity.entity_id] = std::move(snapshot);
    }

    /// Remove entity
    void remove_entity(EntityRef ref) {
        m_entities.erase(ref.entity_id);
    }

    /// Get entity count
    [[nodiscard]] std::size_t entity_count() const noexcept {
        return m_entities.size();
    }

    // -------------------------------------------------------------------------
    // Layer access
    // -------------------------------------------------------------------------

    /// Get all layer snapshots
    [[nodiscard]] const std::unordered_map<std::uint32_t, LayerSnapshot>& layers() const {
        return m_layers;
    }

    /// Get layer snapshot
    [[nodiscard]] const LayerSnapshot* get_layer(LayerId id) const {
        auto it = m_layers.find(id.value);
        if (it == m_layers.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Add layer snapshot
    void add_layer(LayerSnapshot snapshot) {
        m_layers[snapshot.layer.value] = std::move(snapshot);
    }

    /// Remove layer
    void remove_layer(LayerId id) {
        m_layers.erase(id.value);
    }

    // -------------------------------------------------------------------------
    // Hierarchy access
    // -------------------------------------------------------------------------

    /// Get hierarchy
    [[nodiscard]] const HierarchySnapshot& hierarchy() const noexcept {
        return m_hierarchy;
    }

    /// Get mutable hierarchy
    [[nodiscard]] HierarchySnapshot& hierarchy_mut() noexcept {
        return m_hierarchy;
    }

    // -------------------------------------------------------------------------
    // Cloning
    // -------------------------------------------------------------------------

    /// Deep clone the snapshot
    [[nodiscard]] Snapshot clone() const {
        Snapshot s(m_id, m_namespace);
        s.m_timestamp = m_timestamp;
        s.m_description = m_description;
        s.m_entities = m_entities;
        s.m_layers = m_layers;
        s.m_hierarchy = m_hierarchy;
        return s;
    }

private:
    SnapshotId m_id;
    NamespaceId m_namespace;
    std::chrono::steady_clock::time_point m_timestamp;
    std::string m_description;
    std::unordered_map<std::uint64_t, EntitySnapshot> m_entities;
    std::unordered_map<std::uint32_t, LayerSnapshot> m_layers;
    HierarchySnapshot m_hierarchy;
};

// =============================================================================
// SnapshotDelta
// =============================================================================

/// Difference between two snapshots
class SnapshotDelta {
public:
    /// Entity changes
    struct EntityChange {
        EntityRef entity;
        enum class Type { Added, Removed, Modified } type;
        std::optional<EntitySnapshot> old_state;
        std::optional<EntitySnapshot> new_state;
    };

    /// Component changes
    struct ComponentChange {
        EntityRef entity;
        std::string component_type;
        enum class Type { Added, Removed, Modified } type;
        std::optional<Value> old_value;
        std::optional<Value> new_value;
    };

    /// Get entity changes
    [[nodiscard]] const std::vector<EntityChange>& entity_changes() const {
        return m_entity_changes;
    }

    /// Get component changes
    [[nodiscard]] const std::vector<ComponentChange>& component_changes() const {
        return m_component_changes;
    }

    /// Add entity change
    void add_entity_change(EntityChange change) {
        m_entity_changes.push_back(std::move(change));
    }

    /// Add component change
    void add_component_change(ComponentChange change) {
        m_component_changes.push_back(std::move(change));
    }

    /// Check if delta is empty
    [[nodiscard]] bool empty() const noexcept {
        return m_entity_changes.empty() && m_component_changes.empty();
    }

    /// Convert delta to patches for replay
    [[nodiscard]] PatchBatch to_patches() const {
        PatchBatch batch;

        for (const auto& ec : m_entity_changes) {
            if (ec.type == EntityChange::Type::Added && ec.new_state) {
                batch.push(EntityPatch::create(ec.entity, ec.new_state->name));
                for (const auto& [type, value] : ec.new_state->components) {
                    batch.push(ComponentPatch::add(ec.entity, type, value.clone()));
                }
            }
            else if (ec.type == EntityChange::Type::Removed) {
                batch.push(EntityPatch::destroy(ec.entity));
            }
        }

        for (const auto& cc : m_component_changes) {
            if (cc.type == ComponentChange::Type::Added && cc.new_value) {
                batch.push(ComponentPatch::add(cc.entity, cc.component_type, cc.new_value->clone()));
            }
            else if (cc.type == ComponentChange::Type::Removed) {
                batch.push(ComponentPatch::remove(cc.entity, cc.component_type));
            }
            else if (cc.type == ComponentChange::Type::Modified && cc.new_value) {
                batch.push(ComponentPatch::set(cc.entity, cc.component_type, cc.new_value->clone()));
            }
        }

        return batch;
    }

    /// Compute delta between two snapshots
    [[nodiscard]] static SnapshotDelta compute(const Snapshot& from, const Snapshot& to) {
        SnapshotDelta delta;

        // Find added/modified entities
        for (const auto& [id, new_entity] : to.entities()) {
            const EntitySnapshot* old_entity = from.get_entity(new_entity.entity);

            if (!old_entity) {
                // Added
                EntityChange change;
                change.entity = new_entity.entity;
                change.type = EntityChange::Type::Added;
                change.new_state = new_entity.clone();
                delta.add_entity_change(std::move(change));
            }
            else {
                // Check for modifications
                for (const auto& [type, new_value] : new_entity.components) {
                    const Value* old_value = old_entity->get_component(type);

                    if (!old_value) {
                        // Component added
                        ComponentChange cc;
                        cc.entity = new_entity.entity;
                        cc.component_type = type;
                        cc.type = ComponentChange::Type::Added;
                        cc.new_value = new_value.clone();
                        delta.add_component_change(std::move(cc));
                    }
                    else if (*old_value != new_value) {
                        // Component modified
                        ComponentChange cc;
                        cc.entity = new_entity.entity;
                        cc.component_type = type;
                        cc.type = ComponentChange::Type::Modified;
                        cc.old_value = old_value->clone();
                        cc.new_value = new_value.clone();
                        delta.add_component_change(std::move(cc));
                    }
                }

                // Check for removed components
                for (const auto& [type, old_value] : old_entity->components) {
                    if (!new_entity.has_component(type)) {
                        ComponentChange cc;
                        cc.entity = new_entity.entity;
                        cc.component_type = type;
                        cc.type = ComponentChange::Type::Removed;
                        cc.old_value = old_value.clone();
                        delta.add_component_change(std::move(cc));
                    }
                }
            }
        }

        // Find removed entities
        for (const auto& [id, old_entity] : from.entities()) {
            if (!to.get_entity(old_entity.entity)) {
                EntityChange change;
                change.entity = old_entity.entity;
                change.type = EntityChange::Type::Removed;
                change.old_state = old_entity.clone();
                delta.add_entity_change(std::move(change));
            }
        }

        return delta;
    }

private:
    std::vector<EntityChange> m_entity_changes;
    std::vector<ComponentChange> m_component_changes;
};

// =============================================================================
// SnapshotManager
// =============================================================================

/// Manages snapshots for a namespace
class SnapshotManager {
public:
    /// Construct with max snapshots (0 = unlimited)
    explicit SnapshotManager(std::size_t max_snapshots = 0)
        : m_max_snapshots(max_snapshots) {}

    /// Create a new snapshot
    [[nodiscard]] SnapshotId create(NamespaceId ns, std::string description = "") {
        SnapshotId id(m_next_id++);
        Snapshot snapshot(id, ns);
        snapshot.set_description(std::move(description));

        // Enforce limit
        if (m_max_snapshots > 0 && m_snapshots.size() >= m_max_snapshots) {
            // Remove oldest
            if (!m_order.empty()) {
                m_snapshots.erase(m_order.front().value);
                m_order.erase(m_order.begin());
            }
        }

        m_snapshots.emplace(id.value, std::move(snapshot));
        m_order.push_back(id);

        return id;
    }

    /// Get snapshot by ID
    [[nodiscard]] Snapshot* get(SnapshotId id) {
        auto it = m_snapshots.find(id.value);
        if (it == m_snapshots.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Get snapshot by ID (const)
    [[nodiscard]] const Snapshot* get(SnapshotId id) const {
        auto it = m_snapshots.find(id.value);
        if (it == m_snapshots.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Get the most recent snapshot
    [[nodiscard]] const Snapshot* latest() const {
        if (m_order.empty()) {
            return nullptr;
        }
        return get(m_order.back());
    }

    /// Get snapshot at index (0 = oldest)
    [[nodiscard]] const Snapshot* at_index(std::size_t index) const {
        if (index >= m_order.size()) {
            return nullptr;
        }
        return get(m_order[index]);
    }

    /// Delete a snapshot
    bool remove(SnapshotId id) {
        if (m_snapshots.erase(id.value) > 0) {
            m_order.erase(
                std::remove(m_order.begin(), m_order.end(), id),
                m_order.end()
            );
            return true;
        }
        return false;
    }

    /// Delete all snapshots before given ID
    std::size_t remove_before(SnapshotId id) {
        std::size_t removed = 0;
        auto it = m_order.begin();
        while (it != m_order.end() && *it < id) {
            m_snapshots.erase(it->value);
            it = m_order.erase(it);
            ++removed;
        }
        return removed;
    }

    /// Get snapshot count
    [[nodiscard]] std::size_t size() const noexcept {
        return m_snapshots.size();
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return m_snapshots.empty();
    }

    /// Clear all snapshots
    void clear() {
        m_snapshots.clear();
        m_order.clear();
    }

    /// Set max snapshots
    void set_max_snapshots(std::size_t max) {
        m_max_snapshots = max;

        // Trim if needed
        while (m_max_snapshots > 0 && m_snapshots.size() > m_max_snapshots) {
            if (!m_order.empty()) {
                m_snapshots.erase(m_order.front().value);
                m_order.erase(m_order.begin());
            }
        }
    }

    /// Get max snapshots
    [[nodiscard]] std::size_t max_snapshots() const noexcept {
        return m_max_snapshots;
    }

    /// Get all snapshot IDs in order
    [[nodiscard]] const std::vector<SnapshotId>& snapshot_ids() const noexcept {
        return m_order;
    }

private:
    std::unordered_map<std::uint64_t, Snapshot> m_snapshots;
    std::vector<SnapshotId> m_order;  // In chronological order
    std::uint64_t m_next_id = 0;
    std::size_t m_max_snapshots = 0;
};

// =============================================================================
// Binary Serialization for Hot-Reload
// =============================================================================

/// Binary serializer helper
class BinaryWriter {
public:
    /// Write primitive types
    void write_u8(std::uint8_t v) { m_buffer.push_back(v); }

    void write_u32(std::uint32_t v) {
        m_buffer.push_back(static_cast<std::uint8_t>(v & 0xFF));
        m_buffer.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        m_buffer.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        m_buffer.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }

    void write_u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            m_buffer.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
        }
    }

    void write_i64(std::int64_t v) {
        write_u64(static_cast<std::uint64_t>(v));
    }

    void write_f64(double v) {
        std::uint64_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        write_u64(bits);
    }

    void write_bool(bool v) {
        write_u8(v ? 1 : 0);
    }

    void write_string(const std::string& s) {
        write_u32(static_cast<std::uint32_t>(s.size()));
        m_buffer.insert(m_buffer.end(), s.begin(), s.end());
    }

    void write_bytes(const std::vector<std::uint8_t>& data) {
        write_u32(static_cast<std::uint32_t>(data.size()));
        m_buffer.insert(m_buffer.end(), data.begin(), data.end());
    }

    /// Get the buffer
    [[nodiscard]] std::vector<std::uint8_t> take() {
        return std::move(m_buffer);
    }

    [[nodiscard]] const std::vector<std::uint8_t>& buffer() const {
        return m_buffer;
    }

private:
    std::vector<std::uint8_t> m_buffer;
};

/// Binary deserializer helper
class BinaryReader {
public:
    explicit BinaryReader(const std::vector<std::uint8_t>& data)
        : m_data(data), m_offset(0) {}

    [[nodiscard]] bool has_remaining(std::size_t bytes) const {
        return m_offset + bytes <= m_data.size();
    }

    [[nodiscard]] std::uint8_t read_u8() {
        if (!has_remaining(1)) return 0;
        return m_data[m_offset++];
    }

    [[nodiscard]] std::uint32_t read_u32() {
        if (!has_remaining(4)) return 0;
        std::uint32_t v = m_data[m_offset] |
                          (m_data[m_offset + 1] << 8) |
                          (m_data[m_offset + 2] << 16) |
                          (m_data[m_offset + 3] << 24);
        m_offset += 4;
        return v;
    }

    [[nodiscard]] std::uint64_t read_u64() {
        if (!has_remaining(8)) return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<std::uint64_t>(m_data[m_offset + i]) << (i * 8);
        }
        m_offset += 8;
        return v;
    }

    [[nodiscard]] std::int64_t read_i64() {
        return static_cast<std::int64_t>(read_u64());
    }

    [[nodiscard]] double read_f64() {
        std::uint64_t bits = read_u64();
        double v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    [[nodiscard]] bool read_bool() {
        return read_u8() != 0;
    }

    [[nodiscard]] std::string read_string() {
        std::uint32_t len = read_u32();
        if (!has_remaining(len)) return "";
        std::string s(m_data.begin() + m_offset, m_data.begin() + m_offset + len);
        m_offset += len;
        return s;
    }

    [[nodiscard]] std::vector<std::uint8_t> read_bytes() {
        std::uint32_t len = read_u32();
        if (!has_remaining(len)) return {};
        std::vector<std::uint8_t> data(m_data.begin() + m_offset,
                                        m_data.begin() + m_offset + len);
        m_offset += len;
        return data;
    }

    [[nodiscard]] bool valid() const { return m_offset <= m_data.size(); }

private:
    const std::vector<std::uint8_t>& m_data;
    std::size_t m_offset;
};

/// Serialize a Value to binary
inline void serialize_value(BinaryWriter& writer, const Value& value) {
    writer.write_u8(static_cast<std::uint8_t>(value.type()));

    switch (value.type()) {
        case ValueType::Null:
            break;
        case ValueType::Bool:
            writer.write_bool(value.as_bool());
            break;
        case ValueType::Int:
            writer.write_i64(value.as_int());
            break;
        case ValueType::Float:
            writer.write_f64(value.as_float());
            break;
        case ValueType::String:
            writer.write_string(value.as_string());
            break;
        case ValueType::Vec2:
            {
                const auto& v = value.as_vec2();
                writer.write_f64(v.x);
                writer.write_f64(v.y);
            }
            break;
        case ValueType::Vec3:
            {
                const auto& v = value.as_vec3();
                writer.write_f64(v.x);
                writer.write_f64(v.y);
                writer.write_f64(v.z);
            }
            break;
        case ValueType::Vec4:
            {
                const auto& v = value.as_vec4();
                writer.write_f64(v.x);
                writer.write_f64(v.y);
                writer.write_f64(v.z);
                writer.write_f64(v.w);
            }
            break;
        case ValueType::Mat4:
            {
                const auto& m = value.as_mat4();
                for (int i = 0; i < 16; ++i) {
                    writer.write_f64(m.data[i]);
                }
            }
            break;
        case ValueType::Array:
            {
                const auto& arr = value.as_array();
                writer.write_u32(static_cast<std::uint32_t>(arr.size()));
                for (const auto& elem : arr) {
                    serialize_value(writer, elem);
                }
            }
            break;
        case ValueType::Object:
            {
                const auto& obj = value.as_object();
                writer.write_u32(static_cast<std::uint32_t>(obj.size()));
                for (const auto& [key, val] : obj) {
                    writer.write_string(key);
                    serialize_value(writer, val);
                }
            }
            break;
        case ValueType::Bytes:
            writer.write_bytes(value.as_bytes());
            break;
        case ValueType::EntityRef:
            {
                const auto& ref = value.as_entity_ref();
                writer.write_u32(ref.namespace_id);
                writer.write_u64(ref.entity_id);
            }
            break;
        case ValueType::AssetRef:
            {
                const auto& ref = value.as_asset_ref();
                writer.write_string(ref.path);
                writer.write_u64(ref.uuid);
            }
            break;
    }
}

/// Deserialize a Value from binary
[[nodiscard]] inline Value deserialize_value(BinaryReader& reader) {
    ValueType type = static_cast<ValueType>(reader.read_u8());

    switch (type) {
        case ValueType::Null:
            return Value::null();
        case ValueType::Bool:
            return Value(reader.read_bool());
        case ValueType::Int:
            return Value(reader.read_i64());
        case ValueType::Float:
            return Value(reader.read_f64());
        case ValueType::String:
            return Value(reader.read_string());
        case ValueType::Vec2:
            return Value(Vec2{reader.read_f64(), reader.read_f64()});
        case ValueType::Vec3:
            return Value(Vec3{reader.read_f64(), reader.read_f64(), reader.read_f64()});
        case ValueType::Vec4:
            return Value(Vec4{reader.read_f64(), reader.read_f64(),
                              reader.read_f64(), reader.read_f64()});
        case ValueType::Mat4:
            {
                Mat4 m;
                for (int i = 0; i < 16; ++i) {
                    m.data[i] = reader.read_f64();
                }
                return Value(m);
            }
        case ValueType::Array:
            {
                std::uint32_t count = reader.read_u32();
                std::vector<Value> arr;
                arr.reserve(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    arr.push_back(deserialize_value(reader));
                }
                return Value(std::move(arr));
            }
        case ValueType::Object:
            {
                std::uint32_t count = reader.read_u32();
                std::unordered_map<std::string, Value> obj;
                for (std::uint32_t i = 0; i < count; ++i) {
                    std::string key = reader.read_string();
                    Value val = deserialize_value(reader);
                    obj.emplace(std::move(key), std::move(val));
                }
                return Value(std::move(obj));
            }
        case ValueType::Bytes:
            return Value(reader.read_bytes());
        case ValueType::EntityRef:
            {
                std::uint32_t ns = reader.read_u32();
                std::uint64_t id = reader.read_u64();
                return Value(ValueEntityRef{ns, id});
            }
        case ValueType::AssetRef:
            {
                std::string path = reader.read_string();
                std::uint64_t uuid = reader.read_u64();
                return Value(ValueAssetRef{std::move(path), uuid});
            }
    }

    return Value::null();
}

/// Serialize a Snapshot to binary for hot-reload
[[nodiscard]] inline std::vector<std::uint8_t> serialize_snapshot(const Snapshot& snapshot) {
    BinaryWriter writer;

    // Header
    writer.write_u32(1);  // Version
    writer.write_u64(snapshot.id().value);
    writer.write_u32(snapshot.namespace_id().value);
    writer.write_string(snapshot.description());

    // Entities
    const auto& entities = snapshot.entities();
    writer.write_u32(static_cast<std::uint32_t>(entities.size()));
    for (const auto& [id, entity] : entities) {
        writer.write_u32(entity.entity.namespace_id.value);
        writer.write_u64(entity.entity.entity_id);
        writer.write_string(entity.name);
        writer.write_bool(entity.enabled);

        // Components
        writer.write_u32(static_cast<std::uint32_t>(entity.components.size()));
        for (const auto& [type, value] : entity.components) {
            writer.write_string(type);
            serialize_value(writer, value);
        }
    }

    // Layers
    const auto& layers = snapshot.layers();
    writer.write_u32(static_cast<std::uint32_t>(layers.size()));
    for (const auto& [id, layer] : layers) {
        writer.write_u32(layer.layer.value);
        writer.write_string(layer.name);
        writer.write_u32(static_cast<std::uint32_t>(layer.order));
        writer.write_bool(layer.visible);
        writer.write_bool(layer.locked);

        // Entities in layer
        writer.write_u32(static_cast<std::uint32_t>(layer.entities.size()));
        for (const auto& ref : layer.entities) {
            writer.write_u32(ref.namespace_id.value);
            writer.write_u64(ref.entity_id);
        }
    }

    // Hierarchy
    const auto& hierarchy = snapshot.hierarchy();
    writer.write_u32(static_cast<std::uint32_t>(hierarchy.parents.size()));
    for (const auto& [entity_id, parent] : hierarchy.parents) {
        writer.write_u64(entity_id);
        writer.write_u32(parent.namespace_id.value);
        writer.write_u64(parent.entity_id);
    }

    writer.write_u32(static_cast<std::uint32_t>(hierarchy.children.size()));
    for (const auto& [entity_id, child_list] : hierarchy.children) {
        writer.write_u64(entity_id);
        writer.write_u32(static_cast<std::uint32_t>(child_list.size()));
        for (const auto& child : child_list) {
            writer.write_u32(child.namespace_id.value);
            writer.write_u64(child.entity_id);
        }
    }

    return writer.take();
}

/// Deserialize a Snapshot from binary for hot-reload
[[nodiscard]] inline std::optional<Snapshot> deserialize_snapshot(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 4) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    // Header
    std::uint32_t version = reader.read_u32();
    if (version != 1) {
        return std::nullopt;  // Incompatible version
    }

    SnapshotId id{reader.read_u64()};
    NamespaceId ns{reader.read_u32()};
    std::string description = reader.read_string();

    Snapshot snapshot(id, ns);
    snapshot.set_description(std::move(description));

    // Entities
    std::uint32_t entity_count = reader.read_u32();
    for (std::uint32_t i = 0; i < entity_count; ++i) {
        EntitySnapshot entity;
        entity.entity.namespace_id = NamespaceId{reader.read_u32()};
        entity.entity.entity_id = reader.read_u64();
        entity.name = reader.read_string();
        entity.enabled = reader.read_bool();

        // Components
        std::uint32_t comp_count = reader.read_u32();
        for (std::uint32_t j = 0; j < comp_count; ++j) {
            std::string type = reader.read_string();
            Value value = deserialize_value(reader);
            entity.components.emplace(std::move(type), std::move(value));
        }

        snapshot.add_entity(std::move(entity));
    }

    // Layers
    std::uint32_t layer_count = reader.read_u32();
    for (std::uint32_t i = 0; i < layer_count; ++i) {
        LayerSnapshot layer;
        layer.layer = LayerId{reader.read_u32()};
        layer.name = reader.read_string();
        layer.order = static_cast<std::int32_t>(reader.read_u32());
        layer.visible = reader.read_bool();
        layer.locked = reader.read_bool();

        std::uint32_t entity_count = reader.read_u32();
        for (std::uint32_t j = 0; j < entity_count; ++j) {
            EntityRef ref;
            ref.namespace_id = NamespaceId{reader.read_u32()};
            ref.entity_id = reader.read_u64();
            layer.entities.push_back(ref);
        }

        snapshot.add_layer(std::move(layer));
    }

    // Hierarchy
    std::uint32_t parent_count = reader.read_u32();
    for (std::uint32_t i = 0; i < parent_count; ++i) {
        std::uint64_t entity_id = reader.read_u64();
        EntityRef parent;
        parent.namespace_id = NamespaceId{reader.read_u32()};
        parent.entity_id = reader.read_u64();
        snapshot.hierarchy_mut().parents[entity_id] = parent;
    }

    std::uint32_t children_count = reader.read_u32();
    for (std::uint32_t i = 0; i < children_count; ++i) {
        std::uint64_t entity_id = reader.read_u64();
        std::uint32_t child_count = reader.read_u32();
        std::vector<EntityRef> children;
        children.reserve(child_count);
        for (std::uint32_t j = 0; j < child_count; ++j) {
            EntityRef child;
            child.namespace_id = NamespaceId{reader.read_u32()};
            child.entity_id = reader.read_u64();
            children.push_back(child);
        }
        snapshot.hierarchy_mut().children[entity_id] = std::move(children);
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return snapshot;
}

/// Convenience functions for hot-reload workflow
[[nodiscard]] inline std::vector<std::uint8_t> take_ir_snapshot(
    const SnapshotManager& manager) {
    const Snapshot* latest = manager.latest();
    if (!latest) {
        return {};
    }
    return serialize_snapshot(*latest);
}

inline std::optional<Snapshot> restore_ir_snapshot(
    const std::vector<std::uint8_t>& data) {
    return deserialize_snapshot(data);
}

} // namespace void_ir

// Hash specialization
template<>
struct std::hash<void_ir::SnapshotId> {
    std::size_t operator()(const void_ir::SnapshotId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
