#pragma once

/// @file patch.hpp
/// @brief Patch types for declarative state changes

#include "fwd.hpp"
#include "namespace.hpp"
#include "value.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <memory>

namespace void_ir {

// =============================================================================
// PatchKind
// =============================================================================

/// Patch type discriminator
enum class PatchKind : std::uint8_t {
    Entity = 0,      // Entity create/delete
    Component,       // Component add/remove/modify
    Layer,           // Layer operations
    Asset,           // Asset reference
    Hierarchy,       // Parent-child relationships
    Camera,          // Camera properties
    Transform,       // Transform (position, rotation, scale)
    Custom           // User-defined patch
};

/// Get string name for patch kind
[[nodiscard]] inline const char* patch_kind_name(PatchKind kind) noexcept {
    switch (kind) {
        case PatchKind::Entity: return "Entity";
        case PatchKind::Component: return "Component";
        case PatchKind::Layer: return "Layer";
        case PatchKind::Asset: return "Asset";
        case PatchKind::Hierarchy: return "Hierarchy";
        case PatchKind::Camera: return "Camera";
        case PatchKind::Transform: return "Transform";
        case PatchKind::Custom: return "Custom";
        default: return "Unknown";
    }
}

// =============================================================================
// EntityPatch
// =============================================================================

/// Entity operation type
enum class EntityOp : std::uint8_t {
    Create = 0,
    Delete,
    Enable,
    Disable
};

/// Patch for entity creation/deletion
struct EntityPatch {
    EntityRef entity;
    EntityOp operation = EntityOp::Create;
    std::string name;  // Optional entity name

    /// Create entity creation patch
    [[nodiscard]] static EntityPatch create(EntityRef ref, std::string name = "") {
        return EntityPatch{ref, EntityOp::Create, std::move(name)};
    }

    /// Create entity deletion patch
    [[nodiscard]] static EntityPatch destroy(EntityRef ref) {
        return EntityPatch{ref, EntityOp::Delete, ""};
    }

    /// Create enable patch
    [[nodiscard]] static EntityPatch enable(EntityRef ref) {
        return EntityPatch{ref, EntityOp::Enable, ""};
    }

    /// Create disable patch
    [[nodiscard]] static EntityPatch disable(EntityRef ref) {
        return EntityPatch{ref, EntityOp::Disable, ""};
    }
};

// =============================================================================
// ComponentPatch
// =============================================================================

/// Component operation type
enum class ComponentOp : std::uint8_t {
    Add = 0,
    Remove,
    Set,       // Set entire component
    SetField   // Set single field
};

/// Patch for component modifications
struct ComponentPatch {
    EntityRef entity;
    std::string component_type;
    ComponentOp operation = ComponentOp::Set;
    std::string field_path;  // For SetField operation
    Value value;             // New value (for Add, Set, SetField)

    /// Add component to entity
    [[nodiscard]] static ComponentPatch add(EntityRef ref, std::string type, Value val) {
        return ComponentPatch{ref, std::move(type), ComponentOp::Add, "", std::move(val)};
    }

    /// Remove component from entity
    [[nodiscard]] static ComponentPatch remove(EntityRef ref, std::string type) {
        return ComponentPatch{ref, std::move(type), ComponentOp::Remove, "", Value::null()};
    }

    /// Set entire component value
    [[nodiscard]] static ComponentPatch set(EntityRef ref, std::string type, Value val) {
        return ComponentPatch{ref, std::move(type), ComponentOp::Set, "", std::move(val)};
    }

    /// Set single field
    [[nodiscard]] static ComponentPatch set_field(
        EntityRef ref, std::string type, std::string field, Value val) {
        return ComponentPatch{ref, std::move(type), ComponentOp::SetField,
                              std::move(field), std::move(val)};
    }
};

// =============================================================================
// LayerPatch
// =============================================================================

/// Layer operation type
enum class LayerOp : std::uint8_t {
    Create = 0,
    Delete,
    Rename,
    SetOrder,
    SetVisible,
    SetLocked,
    AddEntity,
    RemoveEntity
};

/// Patch for layer operations
struct LayerPatch {
    LayerId layer;
    LayerOp operation = LayerOp::Create;
    std::string name;
    std::int32_t order = 0;
    bool flag = true;         // For SetVisible, SetLocked
    EntityRef entity;         // For AddEntity, RemoveEntity

    /// Create new layer
    [[nodiscard]] static LayerPatch create(LayerId id, std::string name, std::int32_t order = 0) {
        LayerPatch p;
        p.layer = id;
        p.operation = LayerOp::Create;
        p.name = std::move(name);
        p.order = order;
        return p;
    }

    /// Delete layer
    [[nodiscard]] static LayerPatch destroy(LayerId id) {
        LayerPatch p;
        p.layer = id;
        p.operation = LayerOp::Delete;
        return p;
    }

    /// Rename layer
    [[nodiscard]] static LayerPatch rename(LayerId id, std::string new_name) {
        LayerPatch p;
        p.layer = id;
        p.operation = LayerOp::Rename;
        p.name = std::move(new_name);
        return p;
    }

    /// Set layer order
    [[nodiscard]] static LayerPatch set_order(LayerId id, std::int32_t order) {
        LayerPatch p;
        p.layer = id;
        p.operation = LayerOp::SetOrder;
        p.order = order;
        return p;
    }

    /// Set visibility
    [[nodiscard]] static LayerPatch set_visible(LayerId id, bool visible) {
        LayerPatch p;
        p.layer = id;
        p.operation = LayerOp::SetVisible;
        p.flag = visible;
        return p;
    }

    /// Set locked state
    [[nodiscard]] static LayerPatch set_locked(LayerId id, bool locked) {
        LayerPatch p;
        p.layer = id;
        p.operation = LayerOp::SetLocked;
        p.flag = locked;
        return p;
    }

    /// Add entity to layer
    [[nodiscard]] static LayerPatch add_entity(LayerId id, EntityRef entity) {
        LayerPatch p;
        p.layer = id;
        p.operation = LayerOp::AddEntity;
        p.entity = entity;
        return p;
    }

    /// Remove entity from layer
    [[nodiscard]] static LayerPatch remove_entity(LayerId id, EntityRef entity) {
        LayerPatch p;
        p.layer = id;
        p.operation = LayerOp::RemoveEntity;
        p.entity = entity;
        return p;
    }
};

// =============================================================================
// AssetPatch
// =============================================================================

/// Asset operation type
enum class AssetOp : std::uint8_t {
    Load = 0,
    Unload,
    SetRef
};

/// Patch for asset references
struct AssetPatch {
    EntityRef entity;
    std::string component_type;  // Component that holds the asset ref
    std::string field_path;      // Field path to the asset ref
    AssetOp operation = AssetOp::SetRef;
    AssetRef asset;

    /// Load asset
    [[nodiscard]] static AssetPatch load(EntityRef ref, std::string comp,
                                          std::string field, AssetRef asset) {
        AssetPatch p;
        p.entity = ref;
        p.component_type = std::move(comp);
        p.field_path = std::move(field);
        p.operation = AssetOp::Load;
        p.asset = std::move(asset);
        return p;
    }

    /// Unload asset
    [[nodiscard]] static AssetPatch unload(EntityRef ref, std::string comp,
                                            std::string field) {
        AssetPatch p;
        p.entity = ref;
        p.component_type = std::move(comp);
        p.field_path = std::move(field);
        p.operation = AssetOp::Unload;
        return p;
    }

    /// Set asset reference
    [[nodiscard]] static AssetPatch set_ref(EntityRef ref, std::string comp,
                                             std::string field, AssetRef asset) {
        AssetPatch p;
        p.entity = ref;
        p.component_type = std::move(comp);
        p.field_path = std::move(field);
        p.operation = AssetOp::SetRef;
        p.asset = std::move(asset);
        return p;
    }
};

// =============================================================================
// HierarchyPatch
// =============================================================================

/// Hierarchy operation type
enum class HierarchyOp : std::uint8_t {
    SetParent = 0,
    ClearParent,
    SetSiblingIndex
};

/// Patch for hierarchy modifications
struct HierarchyPatch {
    EntityRef entity;
    HierarchyOp operation = HierarchyOp::SetParent;
    EntityRef parent;           // For SetParent
    std::int32_t sibling_index = 0;  // For SetSiblingIndex

    /// Set parent
    [[nodiscard]] static HierarchyPatch set_parent(EntityRef entity, EntityRef parent) {
        HierarchyPatch p;
        p.entity = entity;
        p.operation = HierarchyOp::SetParent;
        p.parent = parent;
        return p;
    }

    /// Clear parent (make root)
    [[nodiscard]] static HierarchyPatch clear_parent(EntityRef entity) {
        HierarchyPatch p;
        p.entity = entity;
        p.operation = HierarchyOp::ClearParent;
        return p;
    }

    /// Set sibling index
    [[nodiscard]] static HierarchyPatch set_sibling_index(EntityRef entity, std::int32_t index) {
        HierarchyPatch p;
        p.entity = entity;
        p.operation = HierarchyOp::SetSiblingIndex;
        p.sibling_index = index;
        return p;
    }
};

// =============================================================================
// CameraPatch
// =============================================================================

/// Camera property type
enum class CameraProperty : std::uint8_t {
    Position = 0,
    Target,
    Up,
    Fov,
    Near,
    Far,
    Orthographic,
    OrthoSize,
    Viewport,
    ClearColor,
    Depth,
    Active
};

/// Patch for camera modifications
struct CameraPatch {
    EntityRef entity;
    CameraProperty property = CameraProperty::Position;
    Value value;

    /// Set camera position
    [[nodiscard]] static CameraPatch set_position(EntityRef ref, Vec3 pos) {
        return CameraPatch{ref, CameraProperty::Position, Value(pos)};
    }

    /// Set camera target
    [[nodiscard]] static CameraPatch set_target(EntityRef ref, Vec3 target) {
        return CameraPatch{ref, CameraProperty::Target, Value(target)};
    }

    /// Set camera up vector
    [[nodiscard]] static CameraPatch set_up(EntityRef ref, Vec3 up) {
        return CameraPatch{ref, CameraProperty::Up, Value(up)};
    }

    /// Set field of view (degrees)
    [[nodiscard]] static CameraPatch set_fov(EntityRef ref, float fov) {
        return CameraPatch{ref, CameraProperty::Fov, Value(fov)};
    }

    /// Set near plane
    [[nodiscard]] static CameraPatch set_near(EntityRef ref, float near_plane) {
        return CameraPatch{ref, CameraProperty::Near, Value(near_plane)};
    }

    /// Set far plane
    [[nodiscard]] static CameraPatch set_far(EntityRef ref, float far_plane) {
        return CameraPatch{ref, CameraProperty::Far, Value(far_plane)};
    }

    /// Set orthographic mode
    [[nodiscard]] static CameraPatch set_orthographic(EntityRef ref, bool ortho) {
        return CameraPatch{ref, CameraProperty::Orthographic, Value(ortho)};
    }

    /// Set orthographic size
    [[nodiscard]] static CameraPatch set_ortho_size(EntityRef ref, float size) {
        return CameraPatch{ref, CameraProperty::OrthoSize, Value(size)};
    }

    /// Set viewport (x, y, width, height normalized)
    [[nodiscard]] static CameraPatch set_viewport(EntityRef ref, Vec4 viewport) {
        return CameraPatch{ref, CameraProperty::Viewport, Value(viewport)};
    }

    /// Set clear color (RGBA)
    [[nodiscard]] static CameraPatch set_clear_color(EntityRef ref, Vec4 color) {
        return CameraPatch{ref, CameraProperty::ClearColor, Value(color)};
    }

    /// Set depth (render order)
    [[nodiscard]] static CameraPatch set_depth(EntityRef ref, float depth) {
        return CameraPatch{ref, CameraProperty::Depth, Value(depth)};
    }

    /// Set active state
    [[nodiscard]] static CameraPatch set_active(EntityRef ref, bool active) {
        return CameraPatch{ref, CameraProperty::Active, Value(active)};
    }
};

// =============================================================================
// TransformPatch
// =============================================================================

/// Transform property type
enum class TransformProperty : std::uint8_t {
    Position = 0,
    Rotation,
    Scale,
    LocalPosition,
    LocalRotation,
    LocalScale,
    Matrix
};

/// Patch for transform modifications
struct TransformPatch {
    EntityRef entity;
    TransformProperty property = TransformProperty::Position;
    Value value;

    /// Set world position
    [[nodiscard]] static TransformPatch set_position(EntityRef ref, Vec3 pos) {
        return TransformPatch{ref, TransformProperty::Position, Value(pos)};
    }

    /// Set world rotation (quaternion as Vec4: x, y, z, w)
    [[nodiscard]] static TransformPatch set_rotation(EntityRef ref, Vec4 rot) {
        return TransformPatch{ref, TransformProperty::Rotation, Value(rot)};
    }

    /// Set world scale
    [[nodiscard]] static TransformPatch set_scale(EntityRef ref, Vec3 scale) {
        return TransformPatch{ref, TransformProperty::Scale, Value(scale)};
    }

    /// Set local position
    [[nodiscard]] static TransformPatch set_local_position(EntityRef ref, Vec3 pos) {
        return TransformPatch{ref, TransformProperty::LocalPosition, Value(pos)};
    }

    /// Set local rotation
    [[nodiscard]] static TransformPatch set_local_rotation(EntityRef ref, Vec4 rot) {
        return TransformPatch{ref, TransformProperty::LocalRotation, Value(rot)};
    }

    /// Set local scale
    [[nodiscard]] static TransformPatch set_local_scale(EntityRef ref, Vec3 scale) {
        return TransformPatch{ref, TransformProperty::LocalScale, Value(scale)};
    }

    /// Set transform matrix directly
    [[nodiscard]] static TransformPatch set_matrix(EntityRef ref, Mat4 matrix) {
        return TransformPatch{ref, TransformProperty::Matrix, Value(std::move(matrix))};
    }
};

// =============================================================================
// CustomPatch
// =============================================================================

/// Custom user-defined patch
struct CustomPatch {
    std::string type_name;
    EntityRef entity;
    Value data;

    [[nodiscard]] static CustomPatch create(std::string type, EntityRef ref, Value data) {
        return CustomPatch{std::move(type), ref, std::move(data)};
    }
};

// =============================================================================
// Patch (variant wrapper)
// =============================================================================

/// Unified patch type
class Patch {
public:
    using Variant = std::variant<
        EntityPatch,
        ComponentPatch,
        LayerPatch,
        AssetPatch,
        HierarchyPatch,
        CameraPatch,
        TransformPatch,
        CustomPatch
    >;

    /// Default constructor
    Patch() = default;

    /// Copy constructor
    Patch(const Patch&) = default;

    /// Move constructor
    Patch(Patch&&) = default;

    /// Copy assignment
    Patch& operator=(const Patch&) = default;

    /// Move assignment
    Patch& operator=(Patch&&) = default;

    /// Construct from any patch type (excluding Patch itself)
    template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Patch>>>
    Patch(T&& patch) : m_data(std::forward<T>(patch)) {}

    /// Get patch kind
    [[nodiscard]] PatchKind kind() const noexcept {
        return static_cast<PatchKind>(m_data.index());
    }

    /// Get kind name
    [[nodiscard]] const char* kind_name() const noexcept {
        return patch_kind_name(kind());
    }

    /// Check if specific type
    template<typename T>
    [[nodiscard]] bool is() const noexcept {
        return std::holds_alternative<T>(m_data);
    }

    /// Get as specific type (throws if wrong type)
    template<typename T>
    [[nodiscard]] const T& as() const {
        return std::get<T>(m_data);
    }

    /// Get as specific type (throws if wrong type)
    template<typename T>
    [[nodiscard]] T& as() {
        return std::get<T>(m_data);
    }

    /// Try to get as specific type
    template<typename T>
    [[nodiscard]] const T* try_as() const noexcept {
        return std::get_if<T>(&m_data);
    }

    /// Visit with callable
    template<typename F>
    decltype(auto) visit(F&& visitor) const {
        return std::visit(std::forward<F>(visitor), m_data);
    }

    /// Visit with callable (mutable)
    template<typename F>
    decltype(auto) visit(F&& visitor) {
        return std::visit(std::forward<F>(visitor), m_data);
    }

    /// Get the primary entity affected by this patch
    [[nodiscard]] std::optional<EntityRef> target_entity() const {
        return std::visit([](const auto& p) -> std::optional<EntityRef> {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, EntityPatch>) {
                return p.entity;
            } else if constexpr (std::is_same_v<T, ComponentPatch>) {
                return p.entity;
            } else if constexpr (std::is_same_v<T, LayerPatch>) {
                if (p.operation == LayerOp::AddEntity || p.operation == LayerOp::RemoveEntity) {
                    return p.entity;
                }
                return std::nullopt;
            } else if constexpr (std::is_same_v<T, AssetPatch>) {
                return p.entity;
            } else if constexpr (std::is_same_v<T, HierarchyPatch>) {
                return p.entity;
            } else if constexpr (std::is_same_v<T, CameraPatch>) {
                return p.entity;
            } else if constexpr (std::is_same_v<T, TransformPatch>) {
                return p.entity;
            } else if constexpr (std::is_same_v<T, CustomPatch>) {
                return p.entity;
            }
            return std::nullopt;
        }, m_data);
    }

private:
    Variant m_data;
};

// =============================================================================
// PatchBatch
// =============================================================================

/// Collection of patches for batch operations
class PatchBatch {
public:
    /// Default constructor
    PatchBatch() = default;

    /// Reserve capacity
    void reserve(std::size_t capacity) {
        m_patches.reserve(capacity);
    }

    /// Add a patch
    void push(Patch patch) {
        m_patches.push_back(std::move(patch));
    }

    /// Add patches from another batch
    void append(const PatchBatch& other) {
        m_patches.insert(m_patches.end(), other.m_patches.begin(), other.m_patches.end());
    }

    /// Add patches from another batch (move)
    void append(PatchBatch&& other) {
        m_patches.insert(m_patches.end(),
                         std::make_move_iterator(other.m_patches.begin()),
                         std::make_move_iterator(other.m_patches.end()));
        other.clear();
    }

    /// Get patches
    [[nodiscard]] const std::vector<Patch>& patches() const noexcept {
        return m_patches;
    }

    /// Get mutable patches
    [[nodiscard]] std::vector<Patch>& patches() noexcept {
        return m_patches;
    }

    /// Get patch count
    [[nodiscard]] std::size_t size() const noexcept {
        return m_patches.size();
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return m_patches.empty();
    }

    /// Clear all patches
    void clear() {
        m_patches.clear();
    }

    /// Iterator support
    [[nodiscard]] auto begin() noexcept { return m_patches.begin(); }
    [[nodiscard]] auto end() noexcept { return m_patches.end(); }
    [[nodiscard]] auto begin() const noexcept { return m_patches.begin(); }
    [[nodiscard]] auto end() const noexcept { return m_patches.end(); }

private:
    std::vector<Patch> m_patches;
};

} // namespace void_ir
