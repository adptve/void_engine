/// @file patch.cpp
/// @brief Patch serialization and utility implementations for void_ir

#include <void_engine/ir/patch.hpp>
#include <void_engine/ir/snapshot.hpp>
#include <unordered_set>

namespace void_ir {

namespace {
    constexpr std::uint32_t PATCH_MAGIC = 0x50415443;  // "PATC"
    constexpr std::uint32_t PATCH_VERSION = 1;

    constexpr std::uint32_t BATCH_MAGIC = 0x42415443;  // "BATC"
    constexpr std::uint32_t BATCH_VERSION = 1;
}

void serialize_entity_ref(BinaryWriter& writer, const EntityRef& ref) {
    writer.write_u32(ref.namespace_id.value);
    writer.write_u64(ref.entity_id);
}

EntityRef deserialize_entity_ref(BinaryReader& reader) {
    EntityRef ref;
    ref.namespace_id = NamespaceId{reader.read_u32()};
    ref.entity_id = reader.read_u64();
    return ref;
}

void serialize_entity_patch(BinaryWriter& writer, const EntityPatch& patch) {
    serialize_entity_ref(writer, patch.entity);
    writer.write_u8(static_cast<std::uint8_t>(patch.operation));
    writer.write_string(patch.name);
}

EntityPatch deserialize_entity_patch(BinaryReader& reader) {
    EntityPatch patch;
    patch.entity = deserialize_entity_ref(reader);
    patch.operation = static_cast<EntityOp>(reader.read_u8());
    patch.name = reader.read_string();
    return patch;
}

void serialize_component_patch(BinaryWriter& writer, const ComponentPatch& patch) {
    serialize_entity_ref(writer, patch.entity);
    writer.write_string(patch.component_type);
    writer.write_u8(static_cast<std::uint8_t>(patch.operation));
    writer.write_string(patch.field_path);
    serialize_value(writer, patch.value);
}

ComponentPatch deserialize_component_patch(BinaryReader& reader) {
    ComponentPatch patch;
    patch.entity = deserialize_entity_ref(reader);
    patch.component_type = reader.read_string();
    patch.operation = static_cast<ComponentOp>(reader.read_u8());
    patch.field_path = reader.read_string();
    patch.value = deserialize_value(reader);
    return patch;
}

void serialize_layer_patch(BinaryWriter& writer, const LayerPatch& patch) {
    writer.write_u32(patch.layer.value);
    writer.write_u8(static_cast<std::uint8_t>(patch.operation));
    writer.write_string(patch.name);
    writer.write_u32(static_cast<std::uint32_t>(patch.order));
    writer.write_bool(patch.flag);
    serialize_entity_ref(writer, patch.entity);
}

LayerPatch deserialize_layer_patch(BinaryReader& reader) {
    LayerPatch patch;
    patch.layer = LayerId{reader.read_u32()};
    patch.operation = static_cast<LayerOp>(reader.read_u8());
    patch.name = reader.read_string();
    patch.order = static_cast<std::int32_t>(reader.read_u32());
    patch.flag = reader.read_bool();
    patch.entity = deserialize_entity_ref(reader);
    return patch;
}

void serialize_asset_patch(BinaryWriter& writer, const AssetPatch& patch) {
    serialize_entity_ref(writer, patch.entity);
    writer.write_string(patch.component_type);
    writer.write_string(patch.field_path);
    writer.write_u8(static_cast<std::uint8_t>(patch.operation));
    writer.write_string(patch.asset.path);
    writer.write_u64(patch.asset.uuid);
}

AssetPatch deserialize_asset_patch(BinaryReader& reader) {
    AssetPatch patch;
    patch.entity = deserialize_entity_ref(reader);
    patch.component_type = reader.read_string();
    patch.field_path = reader.read_string();
    patch.operation = static_cast<AssetOp>(reader.read_u8());
    patch.asset.path = reader.read_string();
    patch.asset.uuid = reader.read_u64();
    return patch;
}

void serialize_hierarchy_patch(BinaryWriter& writer, const HierarchyPatch& patch) {
    serialize_entity_ref(writer, patch.entity);
    writer.write_u8(static_cast<std::uint8_t>(patch.operation));
    serialize_entity_ref(writer, patch.parent);
    writer.write_u32(static_cast<std::uint32_t>(patch.sibling_index));
}

HierarchyPatch deserialize_hierarchy_patch(BinaryReader& reader) {
    HierarchyPatch patch;
    patch.entity = deserialize_entity_ref(reader);
    patch.operation = static_cast<HierarchyOp>(reader.read_u8());
    patch.parent = deserialize_entity_ref(reader);
    patch.sibling_index = static_cast<std::int32_t>(reader.read_u32());
    return patch;
}

void serialize_camera_patch(BinaryWriter& writer, const CameraPatch& patch) {
    serialize_entity_ref(writer, patch.entity);
    writer.write_u8(static_cast<std::uint8_t>(patch.property));
    serialize_value(writer, patch.value);
}

CameraPatch deserialize_camera_patch(BinaryReader& reader) {
    CameraPatch patch;
    patch.entity = deserialize_entity_ref(reader);
    patch.property = static_cast<CameraProperty>(reader.read_u8());
    patch.value = deserialize_value(reader);
    return patch;
}

void serialize_transform_patch(BinaryWriter& writer, const TransformPatch& patch) {
    serialize_entity_ref(writer, patch.entity);
    writer.write_u8(static_cast<std::uint8_t>(patch.property));
    serialize_value(writer, patch.value);
}

TransformPatch deserialize_transform_patch(BinaryReader& reader) {
    TransformPatch patch;
    patch.entity = deserialize_entity_ref(reader);
    patch.property = static_cast<TransformProperty>(reader.read_u8());
    patch.value = deserialize_value(reader);
    return patch;
}

void serialize_custom_patch(BinaryWriter& writer, const CustomPatch& patch) {
    writer.write_string(patch.type_name);
    serialize_entity_ref(writer, patch.entity);
    serialize_value(writer, patch.data);
}

CustomPatch deserialize_custom_patch(BinaryReader& reader) {
    CustomPatch patch;
    patch.type_name = reader.read_string();
    patch.entity = deserialize_entity_ref(reader);
    patch.data = deserialize_value(reader);
    return patch;
}

void serialize_patch(BinaryWriter& writer, const Patch& patch) {
    writer.write_u8(static_cast<std::uint8_t>(patch.kind()));

    patch.visit([&](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, EntityPatch>) {
            serialize_entity_patch(writer, p);
        } else if constexpr (std::is_same_v<T, ComponentPatch>) {
            serialize_component_patch(writer, p);
        } else if constexpr (std::is_same_v<T, LayerPatch>) {
            serialize_layer_patch(writer, p);
        } else if constexpr (std::is_same_v<T, AssetPatch>) {
            serialize_asset_patch(writer, p);
        } else if constexpr (std::is_same_v<T, HierarchyPatch>) {
            serialize_hierarchy_patch(writer, p);
        } else if constexpr (std::is_same_v<T, CameraPatch>) {
            serialize_camera_patch(writer, p);
        } else if constexpr (std::is_same_v<T, TransformPatch>) {
            serialize_transform_patch(writer, p);
        } else if constexpr (std::is_same_v<T, CustomPatch>) {
            serialize_custom_patch(writer, p);
        }
    });
}

Patch deserialize_patch(BinaryReader& reader) {
    PatchKind kind = static_cast<PatchKind>(reader.read_u8());

    switch (kind) {
        case PatchKind::Entity:
            return Patch(deserialize_entity_patch(reader));
        case PatchKind::Component:
            return Patch(deserialize_component_patch(reader));
        case PatchKind::Layer:
            return Patch(deserialize_layer_patch(reader));
        case PatchKind::Asset:
            return Patch(deserialize_asset_patch(reader));
        case PatchKind::Hierarchy:
            return Patch(deserialize_hierarchy_patch(reader));
        case PatchKind::Camera:
            return Patch(deserialize_camera_patch(reader));
        case PatchKind::Transform:
            return Patch(deserialize_transform_patch(reader));
        case PatchKind::Custom:
            return Patch(deserialize_custom_patch(reader));
        default:
            return Patch{};
    }
}

std::vector<std::uint8_t> serialize_patch_binary(const Patch& patch) {
    BinaryWriter writer;

    writer.write_u32(PATCH_MAGIC);
    writer.write_u32(PATCH_VERSION);

    serialize_patch(writer, patch);

    return writer.take();
}

std::optional<Patch> deserialize_patch_binary(const std::vector<std::uint8_t>& data) {
    if (data.size() < 8) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != PATCH_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != PATCH_VERSION) {
        return std::nullopt;
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return deserialize_patch(reader);
}

std::vector<std::uint8_t> serialize_patch_batch_binary(const PatchBatch& batch) {
    BinaryWriter writer;

    writer.write_u32(BATCH_MAGIC);
    writer.write_u32(BATCH_VERSION);

    writer.write_u32(static_cast<std::uint32_t>(batch.size()));

    for (const auto& patch : batch) {
        serialize_patch(writer, patch);
    }

    return writer.take();
}

std::optional<PatchBatch> deserialize_patch_batch_binary(const std::vector<std::uint8_t>& data) {
    if (data.size() < 12) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != BATCH_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != BATCH_VERSION) {
        return std::nullopt;
    }

    std::uint32_t count = reader.read_u32();

    PatchBatch batch;
    batch.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        batch.push(deserialize_patch(reader));
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return batch;
}

std::size_t count_patches_by_kind(const PatchBatch& batch, PatchKind kind) {
    std::size_t count = 0;
    for (const auto& patch : batch) {
        if (patch.kind() == kind) {
            ++count;
        }
    }
    return count;
}

std::vector<EntityRef> collect_affected_entities(const PatchBatch& batch) {
    std::vector<EntityRef> entities;
    std::unordered_set<std::uint64_t> seen;

    for (const auto& patch : batch) {
        auto target = patch.target_entity();
        if (target && seen.insert(target->entity_id).second) {
            entities.push_back(*target);
        }
    }

    return entities;
}

bool patch_affects_entity(const Patch& patch, EntityRef entity) {
    auto target = patch.target_entity();
    return target && *target == entity;
}

PatchBatch filter_patches_for_entity(const PatchBatch& batch, EntityRef entity) {
    PatchBatch filtered;

    for (const auto& patch : batch) {
        if (patch_affects_entity(patch, entity)) {
            filtered.push(Patch(patch));
        }
    }

    return filtered;
}

PatchBatch filter_patches_by_kind(const PatchBatch& batch, PatchKind kind) {
    PatchBatch filtered;

    for (const auto& patch : batch) {
        if (patch.kind() == kind) {
            filtered.push(Patch(patch));
        }
    }

    return filtered;
}

} // namespace void_ir
