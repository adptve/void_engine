/// @file components.hpp
/// @brief ECS render components for void_engine
///
/// These components integrate with void_ecs::World and are designed for
/// hot-reload compatibility. Components reference assets by handle, allowing
/// assets to be reloaded without invalidating entities.

#pragma once

#include <void_engine/render/render_handles.hpp>

#include <array>
#include <cstdint>
#include <string>

// Forward declare ECS types
namespace void_ecs {
class World;
}

namespace void_render {

// =============================================================================
// TransformComponent
// =============================================================================

/// @brief 3D transformation component
///
/// Stores TRS (translation, rotation, scale) and cached world matrix.
/// The world matrix is updated by the TransformSystem when hierarchy changes.
struct TransformComponent {
    // Local transform (relative to parent)
    std::array<float, 3> position = {0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 1.0f};  // Quaternion (x, y, z, w)
    std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};

    // Cached world matrix (column-major, updated by TransformSystem)
    std::array<float, 16> world_matrix = {{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    }};

    // Dirty flag - set when local transform changes
    bool dirty = true;

    /// Compute local matrix from TRS
    [[nodiscard]] std::array<float, 16> local_matrix() const noexcept;

    /// Set position
    void set_position(float x, float y, float z) noexcept {
        position = {x, y, z};
        dirty = true;
    }

    /// Set rotation from euler angles (degrees)
    void set_rotation_euler(float pitch, float yaw, float roll) noexcept;

    /// Set rotation from quaternion
    void set_rotation(float x, float y, float z, float w) noexcept {
        rotation = {x, y, z, w};
        dirty = true;
    }

    /// Set uniform scale
    void set_scale(float s) noexcept {
        scale = {s, s, s};
        dirty = true;
    }

    /// Set non-uniform scale
    void set_scale(float x, float y, float z) noexcept {
        scale = {x, y, z};
        dirty = true;
    }
};

// =============================================================================
// MeshComponent
// =============================================================================

/// @brief Reference to a GPU mesh
///
/// Points to a mesh in the RenderContext. Can reference:
/// - Built-in mesh by name ("sphere", "cube", etc.)
/// - Loaded model mesh by handle
struct MeshComponent {
    // Either a built-in mesh name or empty if using handle
    std::string builtin_mesh;

    // Handle to loaded mesh (from model)
    AssetMeshHandle mesh_handle;

    // Submesh index within a multi-mesh model
    std::uint32_t submesh_index = 0;

    /// Check if using built-in mesh
    [[nodiscard]] bool is_builtin() const noexcept {
        return !builtin_mesh.empty();
    }

    /// Create from built-in mesh name
    [[nodiscard]] static MeshComponent builtin(const std::string& name) {
        MeshComponent c;
        c.builtin_mesh = name;
        return c;
    }

    /// Create from mesh handle
    [[nodiscard]] static MeshComponent from_handle(AssetMeshHandle handle, std::uint32_t submesh = 0) {
        MeshComponent c;
        c.mesh_handle = handle;
        c.submesh_index = submesh;
        return c;
    }
};

// =============================================================================
// MaterialComponent
// =============================================================================

/// @brief PBR material component
///
/// Can use inline material values or reference a shared MaterialAsset.
/// When asset_handle is valid, inline values are overrides.
struct MaterialComponent {
    // Reference to shared material asset (optional)
    AssetMaterialHandle asset_handle;

    // Inline PBR values (used directly or as overrides)
    std::array<float, 4> albedo = {0.8f, 0.8f, 0.8f, 1.0f};
    float metallic_value = 0.0f;
    float roughness_value = 0.5f;
    float ao_value = 1.0f;
    std::array<float, 3> emissive = {0.0f, 0.0f, 0.0f};
    float emissive_strength = 0.0f;

    // Texture handles (override asset textures if valid)
    AssetTextureHandle albedo_texture;
    AssetTextureHandle normal_texture;
    AssetTextureHandle metallic_roughness_texture;
    AssetTextureHandle occlusion_texture;
    AssetTextureHandle emissive_texture;

    // Render flags
    bool double_sided = false;
    bool alpha_blend = false;
    float alpha_cutoff = 0.5f;

    /// Create default material
    [[nodiscard]] static MaterialComponent pbr_default() {
        return MaterialComponent{};
    }

    /// Create from albedo color
    [[nodiscard]] static MaterialComponent from_color(float r, float g, float b, float a = 1.0f) {
        MaterialComponent m;
        m.albedo = {r, g, b, a};
        return m;
    }

    /// Create metallic material
    [[nodiscard]] static MaterialComponent make_metallic(float r, float g, float b, float metalness = 1.0f, float rough = 0.3f) {
        MaterialComponent m;
        m.albedo = {r, g, b, 1.0f};
        m.metallic_value = metalness;
        m.roughness_value = rough;
        return m;
    }
};

// =============================================================================
// ModelComponent
// =============================================================================

/// @brief Reference to a loaded 3D model asset
///
/// When attached to an entity, the ModelLoaderSystem will:
/// 1. Load the model if not already loaded
/// 2. Create child entities for each node in the model
/// 3. Attach MeshComponent and MaterialComponent to children
///
/// The model can be hot-reloaded - when the source file changes,
/// the asset updates and all referencing entities see the new mesh.
struct ModelComponent {
    // Path to model file (glTF, GLB)
    std::string path;

    // Handle to loaded model (populated by ModelLoaderSystem)
    ModelHandle model_handle;

    // Loading state
    enum class State : std::uint8_t {
        Unloaded,
        Loading,
        Loaded,
        Failed
    };
    State state = State::Unloaded;

    // Error message if loading failed
    std::string error;

    // Options
    bool generate_tangents = true;
    bool flip_uvs = false;
    float scale_factor = 1.0f;

    /// Create from path
    [[nodiscard]] static ModelComponent from_path(const std::string& model_path) {
        ModelComponent c;
        c.path = model_path;
        return c;
    }

    /// Check if loaded
    [[nodiscard]] bool is_loaded() const noexcept {
        return state == State::Loaded && model_handle.is_valid();
    }
};

// =============================================================================
// LightComponent
// =============================================================================

/// @brief Light source component
struct LightComponent {
    enum class Type : std::uint8_t {
        Directional,
        Point,
        Spot
    };

    Type type = Type::Point;
    std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    // Point/Spot light
    float range = 10.0f;

    // Spot light
    float inner_cone_angle = 30.0f;  // Degrees
    float outer_cone_angle = 45.0f;  // Degrees

    // Shadow casting
    bool cast_shadows = false;
    std::uint32_t shadow_resolution = 1024;

    /// Create directional light
    [[nodiscard]] static LightComponent directional(float r, float g, float b, float intensity_val = 1.0f) {
        LightComponent l;
        l.type = Type::Directional;
        l.color = {r, g, b};
        l.intensity = intensity_val;
        return l;
    }

    /// Create point light
    [[nodiscard]] static LightComponent point(float r, float g, float b, float intensity_val = 1.0f, float range_val = 10.0f) {
        LightComponent l;
        l.type = Type::Point;
        l.color = {r, g, b};
        l.intensity = intensity_val;
        l.range = range_val;
        return l;
    }

    /// Create spot light
    [[nodiscard]] static LightComponent spot(float r, float g, float b, float intensity_val = 1.0f,
                                              float inner = 30.0f, float outer = 45.0f) {
        LightComponent l;
        l.type = Type::Spot;
        l.color = {r, g, b};
        l.intensity = intensity_val;
        l.inner_cone_angle = inner;
        l.outer_cone_angle = outer;
        return l;
    }
};

// =============================================================================
// CameraComponent
// =============================================================================

/// @brief Camera component for rendering viewpoints
struct CameraComponent {
    enum class Projection : std::uint8_t {
        Perspective,
        Orthographic
    };

    Projection projection = Projection::Perspective;

    // Perspective settings
    float fov = 60.0f;  // Degrees
    float near_plane = 0.1f;
    float far_plane = 1000.0f;

    // Orthographic settings
    float ortho_size = 10.0f;

    // Render target (0 = main window)
    std::uint32_t render_target = 0;

    // Priority (higher = rendered first for multi-camera)
    std::int32_t priority = 0;

    // Active flag
    bool active = true;

    /// Create perspective camera
    [[nodiscard]] static CameraComponent perspective(float fov_degrees = 60.0f,
                                                      float near_val = 0.1f, float far_val = 1000.0f) {
        CameraComponent c;
        c.projection = Projection::Perspective;
        c.fov = fov_degrees;
        c.near_plane = near_val;
        c.far_plane = far_val;
        return c;
    }

    /// Create orthographic camera
    [[nodiscard]] static CameraComponent orthographic(float size = 10.0f,
                                                       float near_val = 0.1f, float far_val = 1000.0f) {
        CameraComponent c;
        c.projection = Projection::Orthographic;
        c.ortho_size = size;
        c.near_plane = near_val;
        c.far_plane = far_val;
        return c;
    }
};

// =============================================================================
// RenderableTag
// =============================================================================

/// @brief Tag component marking an entity as renderable
///
/// Entities with this tag are processed by the render system.
/// Use to enable/disable rendering without removing mesh/material.
struct RenderableTag {
    bool visible = true;
    std::uint32_t layer_mask = 0xFFFFFFFF;  // Which render layers to appear in
    std::int32_t render_order = 0;  // Sort order within layer
};

// =============================================================================
// HierarchyComponent
// =============================================================================

/// @brief Parent-child hierarchy component
///
/// Enables transform inheritance. When parent moves, children follow.
struct HierarchyComponent {
    std::uint64_t parent_id = 0;  // 0 = no parent (root)
    std::uint32_t parent_generation = 0;

    // Cached child count (maintained by HierarchySystem)
    std::uint32_t child_count = 0;

    /// Check if has parent
    [[nodiscard]] bool has_parent() const noexcept {
        return parent_id != 0;
    }

    /// Set parent entity
    void set_parent(std::uint64_t id, std::uint32_t gen) noexcept {
        parent_id = id;
        parent_generation = gen;
    }

    /// Clear parent (become root)
    void clear_parent() noexcept {
        parent_id = 0;
        parent_generation = 0;
    }
};

// =============================================================================
// AnimationComponent
// =============================================================================

/// @brief Animation state component
struct AnimationComponent {
    enum class Type : std::uint8_t {
        None,
        Rotate,
        Oscillate,
        Orbit,
        Pulse,
        Skeletal
    };

    Type type = Type::None;

    // Animation parameters
    std::array<float, 3> axis = {0.0f, 1.0f, 0.0f};
    float speed = 1.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float phase = 0.0f;

    // Orbit parameters
    std::array<float, 3> orbit_center = {0.0f, 0.0f, 0.0f};
    float orbit_radius = 1.0f;

    // State
    float elapsed_time = 0.0f;
    bool playing = true;
    bool loop = true;
};

// =============================================================================
// Component Registration Helper
// =============================================================================

/// @brief Register all render components with ECS world
void register_render_components(void_ecs::World& world);

} // namespace void_render
