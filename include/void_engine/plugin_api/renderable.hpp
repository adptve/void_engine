/// @file renderable.hpp
/// @brief Render contract for plugin-to-engine communication
///
/// RenderableDesc is the high-level description plugins use to request
/// that an entity be rendered. This is the RENDER CONTRACT:
///
/// - Plugins describe WHAT they want (mesh, material, visibility)
/// - Engine handles HOW (transforms RenderableDesc into render components)
///
/// This abstraction allows:
/// - Plugins to remain independent of render system internals
/// - Engine to change render implementation without breaking plugins
/// - Hot-reload to preserve visual appearance
///
/// Plugins NEVER directly manipulate TransformComponent, MeshComponent,
/// MaterialComponent, etc. They always go through make_renderable().

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace void_plugin_api {

// =============================================================================
// Material Description
// =============================================================================

/// @brief High-level material description for plugins
///
/// This matches the subset of MaterialComponent that plugins typically need.
/// The engine translates this into full MaterialComponent with appropriate
/// texture handles, render flags, etc.
struct MaterialDesc {
    /// Base color (RGBA, linear color space)
    std::array<float, 4> albedo = {0.8f, 0.8f, 0.8f, 1.0f};

    /// Metallic factor (0 = dielectric, 1 = metal)
    float metallic = 0.0f;

    /// Roughness factor (0 = smooth/mirror, 1 = rough/diffuse)
    float roughness = 0.5f;

    /// Ambient occlusion factor (0 = fully occluded, 1 = no occlusion)
    float ao = 1.0f;

    /// Emissive color (RGB)
    std::array<float, 3> emissive = {0.0f, 0.0f, 0.0f};

    /// Emissive strength multiplier
    float emissive_strength = 0.0f;

    /// Path to albedo texture (empty = use albedo color)
    std::string albedo_texture;

    /// Path to normal map texture
    std::string normal_texture;

    /// Path to metallic-roughness texture (R = metallic, G = roughness)
    std::string metallic_roughness_texture;

    /// Path to occlusion texture
    std::string occlusion_texture;

    /// Path to emissive texture
    std::string emissive_texture;

    /// Double-sided rendering
    bool double_sided = false;

    /// Alpha blending enabled
    bool alpha_blend = false;

    /// Alpha cutoff threshold for alpha testing
    float alpha_cutoff = 0.5f;

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /// Create default PBR material
    [[nodiscard]] static MaterialDesc pbr_default() {
        return MaterialDesc{};
    }

    /// Create material from solid color
    [[nodiscard]] static MaterialDesc from_color(float r, float g, float b, float a = 1.0f) {
        MaterialDesc m;
        m.albedo = {r, g, b, a};
        return m;
    }

    /// Create metallic material
    [[nodiscard]] static MaterialDesc make_metallic(float r, float g, float b,
                                                     float metalness = 1.0f,
                                                     float rough = 0.3f) {
        MaterialDesc mat;
        mat.albedo = {r, g, b, 1.0f};
        mat.metallic = metalness;
        mat.roughness = rough;
        return mat;
    }

    /// Create emissive material (glowing)
    [[nodiscard]] static MaterialDesc make_emissive(float r, float g, float b,
                                                     float strength = 1.0f) {
        MaterialDesc mat;
        mat.albedo = {r, g, b, 1.0f};
        mat.emissive = {r, g, b};
        mat.emissive_strength = strength;
        return mat;
    }

    /// Create transparent material
    [[nodiscard]] static MaterialDesc make_transparent(float r, float g, float b, float alpha) {
        MaterialDesc mat;
        mat.albedo = {r, g, b, alpha};
        mat.alpha_blend = true;
        return mat;
    }
};

// =============================================================================
// RenderableDesc
// =============================================================================

/// @brief High-level description of how to render an entity
///
/// Plugins use this to request rendering without knowing render internals.
/// Pass to PluginContext::make_renderable() to apply.
///
/// @code
/// RenderableDesc desc;
/// desc.mesh_builtin = "cube";
/// desc.material = MaterialDesc::from_color(1.0f, 0.0f, 0.0f);  // Red
/// desc.visible = true;
/// ctx.make_renderable(entity, desc);
/// @endcode
struct RenderableDesc {
    // =========================================================================
    // Mesh Source (choose one)
    // =========================================================================

    /// Built-in mesh name: "cube", "sphere", "plane", "cylinder", "cone", "capsule"
    std::string mesh_builtin;

    /// Path to mesh asset file (glTF, GLB, etc.)
    std::string mesh_asset;

    /// Submesh index when using multi-mesh models
    std::uint32_t submesh_index = 0;

    // =========================================================================
    // Material
    // =========================================================================

    /// Material description
    MaterialDesc material;

    /// Path to shared material asset (overrides inline material if set)
    std::string material_asset;

    // =========================================================================
    // Visibility & Sorting
    // =========================================================================

    /// Whether the entity should be rendered
    bool visible = true;

    /// Render layer mask (which cameras/layers can see this)
    /// Default: visible in all layers
    std::uint32_t layer_mask = 0xFFFFFFFF;

    /// Render order within layer (lower = rendered first)
    /// Use for controlling transparency sorting
    std::int32_t render_order = 0;

    // =========================================================================
    // Transform Overrides (optional)
    // =========================================================================

    /// If true, use the transform values below instead of existing TransformComponent
    bool override_transform = false;

    /// Position (world space)
    std::array<float, 3> position = {0.0f, 0.0f, 0.0f};

    /// Rotation as quaternion (x, y, z, w)
    std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 1.0f};

    /// Scale
    std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};

    // =========================================================================
    // Advanced
    // =========================================================================

    /// Cast shadows
    bool cast_shadows = true;

    /// Receive shadows
    bool receive_shadows = true;

    /// Enable frustum culling
    bool frustum_cull = true;

    /// Enable occlusion culling
    bool occlusion_cull = true;

    /// LOD bias (-1 to 1, negative = prefer higher detail)
    float lod_bias = 0.0f;

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /// Create renderable using built-in mesh
    [[nodiscard]] static RenderableDesc builtin(const std::string& mesh_name) {
        RenderableDesc desc;
        desc.mesh_builtin = mesh_name;
        return desc;
    }

    /// Create renderable from asset path
    [[nodiscard]] static RenderableDesc from_asset(const std::string& path) {
        RenderableDesc desc;
        desc.mesh_asset = path;
        return desc;
    }

    /// Create colored cube
    [[nodiscard]] static RenderableDesc colored_cube(float r, float g, float b) {
        RenderableDesc desc;
        desc.mesh_builtin = "cube";
        desc.material = MaterialDesc::from_color(r, g, b);
        return desc;
    }

    /// Create colored sphere
    [[nodiscard]] static RenderableDesc colored_sphere(float r, float g, float b) {
        RenderableDesc desc;
        desc.mesh_builtin = "sphere";
        desc.material = MaterialDesc::from_color(r, g, b);
        return desc;
    }

    /// Create invisible (for entities that have visual children but no own mesh)
    [[nodiscard]] static RenderableDesc invisible() {
        RenderableDesc desc;
        desc.visible = false;
        return desc;
    }

    // =========================================================================
    // Builder Pattern
    // =========================================================================

    /// Set mesh from built-in
    RenderableDesc& with_mesh(const std::string& name) {
        mesh_builtin = name;
        mesh_asset.clear();
        return *this;
    }

    /// Set mesh from asset
    RenderableDesc& with_mesh_asset(const std::string& path) {
        mesh_asset = path;
        mesh_builtin.clear();
        return *this;
    }

    /// Set material
    RenderableDesc& with_material(const MaterialDesc& mat) {
        material = mat;
        return *this;
    }

    /// Set color (convenience for simple materials)
    RenderableDesc& with_color(float r, float g, float b, float a = 1.0f) {
        material.albedo = {r, g, b, a};
        return *this;
    }

    /// Set visibility
    RenderableDesc& with_visibility(bool vis) {
        visible = vis;
        return *this;
    }

    /// Set layer mask
    RenderableDesc& with_layer(std::uint32_t mask) {
        layer_mask = mask;
        return *this;
    }

    /// Set render order
    RenderableDesc& with_order(std::int32_t order) {
        render_order = order;
        return *this;
    }

    /// Set position
    RenderableDesc& at_position(float x, float y, float z) {
        override_transform = true;
        position = {x, y, z};
        return *this;
    }

    /// Set scale (uniform)
    RenderableDesc& with_scale(float s) {
        override_transform = true;
        scale = {s, s, s};
        return *this;
    }

    /// Set scale (non-uniform)
    RenderableDesc& with_scale(float x, float y, float z) {
        override_transform = true;
        scale = {x, y, z};
        return *this;
    }

    // =========================================================================
    // Validation
    // =========================================================================

    /// Check if description has valid mesh source
    [[nodiscard]] bool has_mesh() const {
        return !mesh_builtin.empty() || !mesh_asset.empty();
    }

    /// Check if description is valid
    [[nodiscard]] bool is_valid() const {
        // Must have either builtin mesh, asset mesh, or be invisible
        return has_mesh() || !visible;
    }
};

} // namespace void_plugin_api
