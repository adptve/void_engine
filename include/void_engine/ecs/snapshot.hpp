#pragma once

/// @file snapshot.hpp
/// @brief Hot-reload snapshot system for void_ecs
///
/// Provides state serialization/deserialization for ECS hot-reload.
/// Enables preserving entity and component state across code reloads.

#include "fwd.hpp"
#include "entity.hpp"
#include "component.hpp"
#include "archetype.hpp"
#include "world.hpp"

#include <vector>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <algorithm>

namespace void_ecs {

// =============================================================================
// ComponentSnapshot
// =============================================================================

/// Serialized component data
struct ComponentSnapshot {
    ComponentId id;
    std::string name;  // For compatibility checking across reloads
    std::size_t size;
    std::vector<std::uint8_t> data;

    ComponentSnapshot() = default;
    ComponentSnapshot(ComponentId i, std::string n, std::size_t s)
        : id(i), name(std::move(n)), size(s) {
        data.resize(s);
    }
};

// =============================================================================
// EntitySnapshot
// =============================================================================

/// Serialized entity state
struct EntitySnapshot {
    std::uint64_t entity_bits;  // Entity encoded as bits (index + generation)
    std::vector<ComponentSnapshot> components;

    EntitySnapshot() : entity_bits(0) {}
    explicit EntitySnapshot(Entity e) : entity_bits(e.to_bits()) {}
};

// =============================================================================
// ResourceSnapshot
// =============================================================================

/// Serialized resource (not fully implemented - resources need custom serialization)
struct ResourceSnapshot {
    std::string type_name;
    std::vector<std::uint8_t> data;
};

// =============================================================================
// WorldSnapshot
// =============================================================================

/// Complete world state snapshot for hot-reload
struct WorldSnapshot {
    /// Snapshot format version
    static constexpr std::uint32_t CURRENT_VERSION = 1;

    /// Version for compatibility
    std::uint32_t version{CURRENT_VERSION};

    /// All entity snapshots
    std::vector<EntitySnapshot> entities;

    /// Component registry metadata (for ID mapping after reload)
    struct ComponentMeta {
        std::uint32_t id;
        std::string name;
        std::size_t size;
        std::size_t align;
    };
    std::vector<ComponentMeta> component_registry;

    /// Check if snapshot is empty
    [[nodiscard]] bool empty() const noexcept {
        return entities.empty();
    }

    /// Get entity count
    [[nodiscard]] std::size_t entity_count() const noexcept {
        return entities.size();
    }

    /// Check version compatibility
    [[nodiscard]] bool is_compatible() const noexcept {
        return version == CURRENT_VERSION;
    }
};

// =============================================================================
// Snapshot Capture
// =============================================================================

/// Take a complete snapshot of the world state
/// @param world The world to snapshot
/// @return WorldSnapshot containing all entity and component state
[[nodiscard]] inline WorldSnapshot take_world_snapshot(const World& world) {
    WorldSnapshot snapshot;
    snapshot.version = WorldSnapshot::CURRENT_VERSION;

    // Capture component registry metadata for ID mapping after reload
    const auto& registry = world.component_registry();
    for (const auto& info : registry) {
        snapshot.component_registry.push_back({
            info.id.id,
            info.name,
            info.size,
            info.align
        });
    }

    // Iterate all archetypes to capture entities and components
    const auto& archetypes = world.archetypes();
    for (const auto& arch_ptr : archetypes) {
        const Archetype& arch = *arch_ptr;
        const auto& entities = arch.entities();
        const auto& components = arch.components();

        // Capture each entity in this archetype
        for (std::size_t row = 0; row < arch.size(); ++row) {
            Entity entity = entities[row];
            if (entity.is_null()) continue;

            EntitySnapshot entity_snap(entity);

            // Capture each component for this entity
            for (ComponentId comp_id : components) {
                const ComponentInfo* info = registry.get_info(comp_id);
                if (!info) continue;

                const void* comp_data = arch.get_component_raw(comp_id, row);
                if (!comp_data) continue;

                ComponentSnapshot comp_snap(comp_id, info->name, info->size);

                // Use clone_fn if available for proper deep copy
                if (info->clone_fn) {
                    // Create temporary storage and clone into it
                    info->clone_fn(comp_data, comp_snap.data.data());
                } else {
                    // Fallback to memcpy for POD types
                    std::memcpy(comp_snap.data.data(), comp_data, info->size);
                }

                entity_snap.components.push_back(std::move(comp_snap));
            }

            snapshot.entities.push_back(std::move(entity_snap));
        }
    }

    return snapshot;
}

// =============================================================================
// Snapshot Restore
// =============================================================================

/// Apply a snapshot to restore world state
/// @param world The world to restore into (will be cleared first)
/// @param snapshot The snapshot to restore from
/// @return true if restoration succeeded
inline bool apply_world_snapshot(World& world, const WorldSnapshot& snapshot) {
    if (!snapshot.is_compatible()) {
        return false;  // Incompatible version
    }

    // Clear existing state
    world.clear();

    // Build component ID mapping (old ID -> new ID based on name)
    // This handles cases where component IDs might differ after code reload
    std::unordered_map<std::uint32_t, ComponentId> id_mapping;
    std::unordered_map<std::uint32_t, std::size_t> size_mapping;

    for (const auto& meta : snapshot.component_registry) {
        auto new_id = world.component_id_by_name(meta.name);
        if (new_id.has_value()) {
            id_mapping[meta.id] = *new_id;

            // Check if size matches (component structure unchanged)
            const ComponentInfo* info = world.component_info(*new_id);
            if (info && info->size == meta.size) {
                size_mapping[meta.id] = meta.size;
            }
        }
    }

    // Restore each entity
    for (const auto& entity_snap : snapshot.entities) {
        // Spawn a new entity
        Entity new_entity = world.spawn();

        // Sort components by ID for consistent archetype building
        std::vector<const ComponentSnapshot*> sorted_comps;
        for (const auto& comp_snap : entity_snap.components) {
            // Only include components that exist in new world with matching size
            auto id_it = id_mapping.find(comp_snap.id.id);
            auto size_it = size_mapping.find(comp_snap.id.id);
            if (id_it != id_mapping.end() && size_it != size_mapping.end()) {
                sorted_comps.push_back(&comp_snap);
            }
        }

        // Add components in order
        for (const ComponentSnapshot* comp_snap : sorted_comps) {
            ComponentId new_comp_id = id_mapping[comp_snap->id.id];

            // Use the World's raw component addition
            world.add_component_raw(
                new_entity,
                new_comp_id,
                comp_snap->data.data(),
                comp_snap->size
            );
        }
    }

    return true;
}

// =============================================================================
// Convenience Functions
// =============================================================================

/// Take snapshot - alias for take_world_snapshot
[[nodiscard]] inline WorldSnapshot world_take_snapshot(const World& world) {
    return take_world_snapshot(world);
}

/// Apply snapshot - alias for apply_world_snapshot
inline bool world_apply_snapshot(World& world, const WorldSnapshot& snapshot) {
    return apply_world_snapshot(world, snapshot);
}

// =============================================================================
// Binary Serialization (for file/network transfer)
// =============================================================================

/// Serialize WorldSnapshot to bytes
[[nodiscard]] inline std::vector<std::uint8_t> serialize_snapshot(const WorldSnapshot& snapshot) {
    std::vector<std::uint8_t> buffer;

    // Helper to write primitives
    auto write_u32 = [&buffer](std::uint32_t v) {
        buffer.push_back(static_cast<std::uint8_t>(v & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    };

    auto write_u64 = [&buffer](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            buffer.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
        }
    };

    auto write_string = [&buffer, &write_u32](const std::string& s) {
        write_u32(static_cast<std::uint32_t>(s.size()));
        buffer.insert(buffer.end(), s.begin(), s.end());
    };

    auto write_bytes = [&buffer, &write_u32](const std::vector<std::uint8_t>& data) {
        write_u32(static_cast<std::uint32_t>(data.size()));
        buffer.insert(buffer.end(), data.begin(), data.end());
    };

    // Write header
    write_u32(snapshot.version);

    // Write component registry
    write_u32(static_cast<std::uint32_t>(snapshot.component_registry.size()));
    for (const auto& meta : snapshot.component_registry) {
        write_u32(meta.id);
        write_string(meta.name);
        write_u64(meta.size);
        write_u64(meta.align);
    }

    // Write entities
    write_u32(static_cast<std::uint32_t>(snapshot.entities.size()));
    for (const auto& entity : snapshot.entities) {
        write_u64(entity.entity_bits);

        // Write components
        write_u32(static_cast<std::uint32_t>(entity.components.size()));
        for (const auto& comp : entity.components) {
            write_u32(comp.id.id);
            write_string(comp.name);
            write_u64(comp.size);
            write_bytes(comp.data);
        }
    }

    return buffer;
}

/// Deserialize WorldSnapshot from bytes
[[nodiscard]] inline std::optional<WorldSnapshot> deserialize_snapshot(const std::vector<std::uint8_t>& buffer) {
    if (buffer.size() < 4) {
        return std::nullopt;
    }

    std::size_t offset = 0;

    // Helper to read primitives
    auto read_u32 = [&buffer, &offset]() -> std::uint32_t {
        if (offset + 4 > buffer.size()) return 0;
        std::uint32_t v = buffer[offset] |
                          (buffer[offset + 1] << 8) |
                          (buffer[offset + 2] << 16) |
                          (buffer[offset + 3] << 24);
        offset += 4;
        return v;
    };

    auto read_u64 = [&buffer, &offset]() -> std::uint64_t {
        if (offset + 8 > buffer.size()) return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<std::uint64_t>(buffer[offset + i]) << (i * 8);
        }
        offset += 8;
        return v;
    };

    auto read_string = [&buffer, &offset, &read_u32]() -> std::string {
        std::uint32_t len = read_u32();
        if (offset + len > buffer.size()) return "";
        std::string s(buffer.begin() + offset, buffer.begin() + offset + len);
        offset += len;
        return s;
    };

    auto read_bytes = [&buffer, &offset, &read_u32]() -> std::vector<std::uint8_t> {
        std::uint32_t len = read_u32();
        if (offset + len > buffer.size()) return {};
        std::vector<std::uint8_t> data(buffer.begin() + offset, buffer.begin() + offset + len);
        offset += len;
        return data;
    };

    WorldSnapshot snapshot;

    // Read header
    snapshot.version = read_u32();
    if (!snapshot.is_compatible()) {
        return std::nullopt;
    }

    // Read component registry
    std::uint32_t registry_count = read_u32();
    snapshot.component_registry.reserve(registry_count);
    for (std::uint32_t i = 0; i < registry_count; ++i) {
        WorldSnapshot::ComponentMeta meta;
        meta.id = read_u32();
        meta.name = read_string();
        meta.size = static_cast<std::size_t>(read_u64());
        meta.align = static_cast<std::size_t>(read_u64());
        snapshot.component_registry.push_back(std::move(meta));
    }

    // Read entities
    std::uint32_t entity_count = read_u32();
    snapshot.entities.reserve(entity_count);
    for (std::uint32_t i = 0; i < entity_count; ++i) {
        EntitySnapshot entity_snap;
        entity_snap.entity_bits = read_u64();

        // Read components
        std::uint32_t comp_count = read_u32();
        entity_snap.components.reserve(comp_count);
        for (std::uint32_t j = 0; j < comp_count; ++j) {
            ComponentSnapshot comp_snap;
            comp_snap.id = ComponentId{read_u32()};
            comp_snap.name = read_string();
            comp_snap.size = static_cast<std::size_t>(read_u64());
            comp_snap.data = read_bytes();
            entity_snap.components.push_back(std::move(comp_snap));
        }

        snapshot.entities.push_back(std::move(entity_snap));
    }

    return snapshot;
}

} // namespace void_ecs
