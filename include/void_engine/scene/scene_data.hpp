#pragma once

/// @file scene_data.hpp
/// @brief Data structures for parsed scene data

#include <void_engine/core/hot_reload.hpp>
#include <void_engine/math/vec.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace void_scene {

// =============================================================================
// Forward Declarations
// =============================================================================

struct SceneData;
struct CameraData;
struct LightData;
struct EntityData;
struct MaterialData;
struct TransformData;
struct AnimationData;

// =============================================================================
// Basic Types
// =============================================================================

using Vec2 = std::array<float, 2>;
using Vec3 = std::array<float, 3>;
using Vec4 = std::array<float, 4>;
using Color3 = std::array<float, 3>;
using Color4 = std::array<float, 4>;

// =============================================================================
// Transform Data
// =============================================================================

struct TransformData {
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Vec3 rotation = {0.0f, 0.0f, 0.0f};  // Euler angles in degrees
    std::variant<float, Vec3> scale = 1.0f;  // Uniform or non-uniform

    /// Get scale as Vec3
    [[nodiscard]] Vec3 scale_vec3() const {
        if (std::holds_alternative<float>(scale)) {
            float s = std::get<float>(scale);
            return {s, s, s};
        }
        return std::get<Vec3>(scale);
    }
};

// =============================================================================
// Material Data
// =============================================================================

/// Texture reference - either a path or inline color
struct TextureOrValue {
    std::optional<std::string> texture_path;
    std::optional<Color4> color;
    std::optional<float> value;

    /// Check if has texture
    [[nodiscard]] bool has_texture() const { return texture_path.has_value(); }

    /// Check if has color
    [[nodiscard]] bool has_color() const { return color.has_value(); }

    /// Check if has value
    [[nodiscard]] bool has_value() const { return value.has_value(); }
};

struct TransmissionData {
    float factor = 0.0f;
    float ior = 1.5f;
    float thickness = 0.0f;
    Color3 attenuation_color = {1.0f, 1.0f, 1.0f};
    float attenuation_distance = 1.0f;
};

struct SheenData {
    Color3 color = {0.0f, 0.0f, 0.0f};
    float roughness = 0.5f;
};

struct ClearcoatData {
    float intensity = 0.0f;
    float roughness = 0.0f;
};

struct AnisotropyData {
    float strength = 0.0f;
    float rotation = 0.0f;
};

struct MaterialData {
    TextureOrValue albedo;
    std::optional<std::string> normal_map;
    TextureOrValue metallic;
    TextureOrValue roughness;
    std::optional<Color3> emissive;

    // Advanced material properties (Phase 7)
    std::optional<TransmissionData> transmission;
    std::optional<SheenData> sheen;
    std::optional<ClearcoatData> clearcoat;
    std::optional<AnisotropyData> anisotropy;
};

// =============================================================================
// Animation Data
// =============================================================================

enum class AnimationType : std::uint8_t {
    None,
    Rotate,
    Oscillate,
    Orbit,
    Pulse,
    Path,
};

struct AnimationData {
    AnimationType type = AnimationType::None;
    Vec3 axis = {0.0f, 1.0f, 0.0f};
    float speed = 1.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float phase = 0.0f;

    // Oscillate specific
    bool rotate = false;  // If true, oscillates rotation instead of position

    // Orbit specific
    Vec3 center = {0.0f, 0.0f, 0.0f};
    float radius = 1.0f;
    float start_angle = 0.0f;
    bool face_center = false;

    // Pulse specific
    float min_scale = 0.8f;
    float max_scale = 1.2f;

    // Path specific
    std::vector<Vec3> points;
    float duration = 1.0f;
    bool loop_animation = false;
    bool ping_pong = false;
    std::string interpolation = "linear";
    bool orient_to_path = false;
    std::string easing = "linear";
};

// =============================================================================
// Pickable Data
// =============================================================================

struct PickableData {
    bool enabled = true;
    int priority = 0;
    std::string bounds = "mesh";
    bool highlight_on_hover = false;
};

// =============================================================================
// Input Events Data
// =============================================================================

struct InputEventsData {
    std::string on_click;
    std::string on_pointer_enter;
    std::string on_pointer_exit;
};

// =============================================================================
// Entity Data
// =============================================================================

struct EntityData {
    std::string name;
    std::string mesh;
    std::string layer = "world";
    bool visible = true;

    TransformData transform;
    std::optional<MaterialData> material;
    std::optional<AnimationData> animation;
    std::optional<PickableData> pickable;
    std::optional<InputEventsData> input_events;
};

// =============================================================================
// Camera Data
// =============================================================================

enum class CameraType : std::uint8_t {
    Perspective,
    Orthographic,
};

enum class CameraControlMode : std::uint8_t {
    Fps,    // First-person shooter style (WASD + mouse look)
    Orbit,  // Orbit around a target point (default for editors/viewers)
    Fly,    // Free fly mode (6DOF)
};

struct PerspectiveData {
    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    std::string aspect = "auto";
};

struct OrthographicData {
    float left = -10.0f;
    float right = 10.0f;
    float bottom = -10.0f;
    float top = 10.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
};

struct CameraTransformData {
    Vec3 position = {0.0f, 0.0f, 5.0f};
    Vec3 target = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
};

struct CameraData {
    std::string name;
    bool active = false;
    CameraType type = CameraType::Perspective;
    CameraControlMode control_mode = CameraControlMode::Orbit;
    CameraTransformData transform;
    PerspectiveData perspective;
    OrthographicData orthographic;
};

// =============================================================================
// Light Data
// =============================================================================

enum class LightType : std::uint8_t {
    Directional,
    Point,
    Spot,
};

struct DirectionalLightData {
    Vec3 direction = {0.0f, -1.0f, 0.0f};
    Color3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    bool cast_shadows = false;
};

struct PointLightData {
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Color3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    bool cast_shadows = false;
    struct {
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
    } attenuation;
};

struct SpotLightData {
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Vec3 direction = {0.0f, -1.0f, 0.0f};
    Color3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float inner_angle = 30.0f;
    float outer_angle = 45.0f;
    bool cast_shadows = false;
};

struct LightData {
    std::string name;
    LightType type = LightType::Directional;
    bool enabled = true;
    DirectionalLightData directional;
    PointLightData point;
    SpotLightData spot;
};

// =============================================================================
// Shadow Data
// =============================================================================

struct ShadowCascadeLevel {
    std::uint32_t resolution = 1024;
    float distance = 50.0f;
    float bias = 0.001f;
};

struct ShadowData {
    bool enabled = true;
    std::uint32_t atlas_size = 4096;
    float max_shadow_distance = 50.0f;
    float shadow_fade_distance = 5.0f;

    struct {
        std::uint32_t count = 3;
        std::string split_scheme = "practical";
        float lambda = 0.5f;
        std::vector<ShadowCascadeLevel> levels;
    } cascades;

    struct {
        std::string method = "pcf";
        std::uint32_t pcf_samples = 16;
        float pcf_radius = 1.5f;
        bool soft_shadows = true;
        bool contact_hardening = false;
    } filtering;
};

// =============================================================================
// Environment Data
// =============================================================================

struct SkyData {
    Color3 zenith_color = {0.1f, 0.3f, 0.6f};
    Color3 horizon_color = {0.5f, 0.7f, 0.9f};
    Color3 ground_color = {0.15f, 0.12f, 0.1f};
    float sun_size = 0.03f;
    float sun_intensity = 50.0f;
    float sun_falloff = 3.0f;
    float fog_density = 0.0f;
};

struct EnvironmentData {
    std::optional<std::string> environment_map;
    float ambient_intensity = 0.1f;
    SkyData sky;
};

// =============================================================================
// Particle Emitter Data
// =============================================================================

struct ParticleEmitterData {
    std::string name;
    Vec3 position = {0.0f, 0.0f, 0.0f};
    float emit_rate = 100.0f;
    std::uint32_t max_particles = 1000;
    Vec2 lifetime = {1.0f, 2.0f};
    Vec2 speed = {1.0f, 2.0f};
    Vec2 size = {0.1f, 0.2f};
    Color4 color_start = {1.0f, 1.0f, 1.0f, 1.0f};
    Color4 color_end = {1.0f, 1.0f, 1.0f, 0.0f};
    Vec3 gravity = {0.0f, -9.8f, 0.0f};
    float spread = 0.5f;
    Vec3 direction = {0.0f, 1.0f, 0.0f};
    bool enabled = true;
};

// =============================================================================
// Texture Data
// =============================================================================

struct TextureData {
    std::string name;
    std::string path;
    bool srgb = true;
    bool mipmap = true;
    bool hdr = false;
};

// =============================================================================
// Debug Data
// =============================================================================

struct DebugVisualizationData {
    bool enabled = false;
    bool bounds = false;
    bool wireframe = false;
    bool normals = false;
    bool light_volumes = false;
    bool shadow_cascades = false;
    bool lod_levels = false;
    bool skeleton = false;
};

struct DebugStatsData {
    bool enabled = false;
    std::string position = "top_left";
    int font_size = 14;
    float background_alpha = 0.7f;
    bool fps = true;
    bool frame_time = true;
    bool draw_calls = true;
    bool triangles = true;
    bool entities_total = true;
    bool entities_visible = true;
    bool gpu_memory = false;
    bool cpu_time = true;
};

struct DebugControlsData {
    std::string toggle_key = "F3";
    std::string cycle_mode_key = "F4";
    std::string reload_shaders_key = "F5";
};

struct DebugData {
    bool enabled = false;
    DebugStatsData stats;
    DebugVisualizationData visualization;
    DebugControlsData controls;
};

// =============================================================================
// Picking Data (Phase 10)
// =============================================================================

struct PickingGpuData {
    Vec2 buffer_size = {256.0f, 256.0f};
    int readback_delay = 1;
};

struct PickingData {
    bool enabled = true;
    std::string method = "gpu";  // "gpu" or "raycast"
    float max_distance = 100.0f;
    std::vector<std::string> layer_mask;
    PickingGpuData gpu;
};

// =============================================================================
// Spatial Query Data (Phase 14)
// =============================================================================

struct BvhData {
    int max_leaf_size = 4;
    std::string build_quality = "medium";
};

struct SpatialQueriesData {
    bool frustum_culling = true;
    bool occlusion_culling = false;
    int max_query_results = 500;
};

struct SpatialData {
    std::string structure = "bvh";
    bool auto_rebuild = true;
    float rebuild_threshold = 0.3f;
    BvhData bvh;
    SpatialQueriesData queries;
};

// =============================================================================
// Input Data
// =============================================================================

struct InputCameraData {
    std::string orbit_button = "left";
    std::string pan_button = "middle";
    bool zoom_scroll = true;
    float orbit_sensitivity = 0.005f;
    float pan_sensitivity = 0.01f;
    float zoom_sensitivity = 0.1f;
    bool invert_y = false;
    bool invert_x = false;
    float min_distance = 0.5f;
    float max_distance = 50.0f;
};

struct InputData {
    InputCameraData camera;
    std::map<std::string, std::string> bindings;
};

// =============================================================================
// Scene Data (Root)
// =============================================================================

struct SceneMetadata {
    std::string name;
    std::string description;
    std::string version;
};

struct SceneData {
    SceneMetadata metadata;
    std::vector<CameraData> cameras;
    std::vector<LightData> lights;
    std::optional<ShadowData> shadows;
    std::optional<EnvironmentData> environment;
    std::optional<PickingData> picking;
    std::optional<SpatialData> spatial;
    std::vector<EntityData> entities;
    std::vector<ParticleEmitterData> particle_emitters;
    std::vector<TextureData> textures;
    std::optional<DebugData> debug;
    std::optional<InputData> input;

    /// Get active camera (or first if none active)
    [[nodiscard]] const CameraData* active_camera() const {
        for (const auto& cam : cameras) {
            if (cam.active) return &cam;
        }
        return cameras.empty() ? nullptr : &cameras[0];
    }

    /// Find entity by name
    [[nodiscard]] const EntityData* find_entity(const std::string& name) const {
        for (const auto& entity : entities) {
            if (entity.name == name) return &entity;
        }
        return nullptr;
    }

    /// Find light by name
    [[nodiscard]] const LightData* find_light(const std::string& name) const {
        for (const auto& light : lights) {
            if (light.name == name) return &light;
        }
        return nullptr;
    }
};

} // namespace void_scene
