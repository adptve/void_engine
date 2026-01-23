/// @file scene_types.hpp
/// @brief Complete scene definition types for void_runtime
/// @details Matches legacy Rust scene_loader.rs structure for full TOML scene support

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace void_runtime {

// =============================================================================
// Forward Declarations
// =============================================================================

struct SceneDefinition;
struct SceneMetadata;
struct CameraDef;
struct LightDef;
struct ShadowsDef;
struct EnvironmentDef;
struct PickingDef;
struct SpatialDef;
struct DebugDef;
struct EntityDef;
struct ParticleEmitterDef;
struct TextureDef;
struct InputConfig;

// =============================================================================
// Basic Types
// =============================================================================

using Vec2 = std::array<float, 2>;
using Vec3 = std::array<float, 3>;
using Vec4 = std::array<float, 4>;
using Color3 = std::array<float, 3>;
using Color4 = std::array<float, 4>;
using Quat = std::array<float, 4>;

/// @brief Variant for script/config values
using ScriptValue = std::variant<
    bool,
    std::int64_t,
    double,
    std::string,
    Vec2,
    Vec3,
    Vec4,
    std::vector<std::string>
>;

// =============================================================================
// Scene Metadata
// =============================================================================

/// @brief Scene metadata
struct SceneMetadata {
    std::string name = "Untitled Scene";
    std::string description;
    std::string version = "1.0.0";
    std::string author;
    std::vector<std::string> tags;
};

// =============================================================================
// Camera System
// =============================================================================

/// @brief Camera type
enum class CameraType {
    Perspective,
    Orthographic
};

/// @brief Camera control mode
enum class CameraControlMode {
    None,
    Fps,      // First-person shooter
    Orbit,    // Orbit around target
    Fly,      // Free flight
    Follow,   // Follow entity
    Rail,     // On-rails
    Cinematic // Scripted
};

/// @brief Camera transform definition
struct CameraTransformDef {
    Vec3 position = {0.0f, 0.0f, 5.0f};
    Vec3 target = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
};

/// @brief Perspective projection settings
struct PerspectiveDef {
    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    std::string aspect = "auto";  // "auto" or explicit ratio
};

/// @brief Orthographic projection settings
struct OrthographicDef {
    float left = -10.0f;
    float right = 10.0f;
    float bottom = -10.0f;
    float top = 10.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
};

/// @brief Camera constraints
struct CameraConstraintsDef {
    std::optional<float> min_pitch;
    std::optional<float> max_pitch;
    std::optional<float> min_yaw;
    std::optional<float> max_yaw;
    std::optional<float> min_distance;
    std::optional<float> max_distance;
    std::optional<Vec3> bounds_min;
    std::optional<Vec3> bounds_max;
};

/// @brief Camera definition
struct CameraDef {
    std::string name;
    bool active = false;
    CameraType type = CameraType::Perspective;
    CameraControlMode control_mode = CameraControlMode::None;
    CameraTransformDef transform;
    PerspectiveDef perspective;
    OrthographicDef orthographic;
    CameraConstraintsDef constraints;

    // Control settings
    float move_speed = 5.0f;
    float look_sensitivity = 0.1f;
    float zoom_speed = 1.0f;
    bool invert_y = false;

    // Follow camera settings
    std::string follow_target;
    Vec3 follow_offset = {0.0f, 2.0f, -5.0f};
    float follow_smoothing = 5.0f;
};

// =============================================================================
// Lighting System
// =============================================================================

/// @brief Light type
enum class LightType {
    Directional,
    Point,
    Spot,
    Area,
    Hemisphere
};

/// @brief Directional light settings
struct DirectionalLightDef {
    Vec3 direction = {0.0f, -1.0f, 0.0f};
    Color3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    bool cast_shadows = true;
};

/// @brief Point light settings
struct PointLightDef {
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Color3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    bool cast_shadows = false;
    float shadow_bias = 0.001f;
};

/// @brief Spot light settings
struct SpotLightDef {
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Vec3 direction = {0.0f, -1.0f, 0.0f};
    Color3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float inner_angle = 30.0f;
    float outer_angle = 45.0f;
    bool cast_shadows = true;
    float shadow_bias = 0.001f;
};

/// @brief Area light settings
struct AreaLightDef {
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Vec3 direction = {0.0f, -1.0f, 0.0f};
    Color3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float width = 1.0f;
    float height = 1.0f;
    bool two_sided = false;
};

/// @brief Hemisphere light settings
struct HemisphereLightDef {
    Color3 sky_color = {0.6f, 0.8f, 1.0f};
    Color3 ground_color = {0.3f, 0.2f, 0.1f};
    float intensity = 0.5f;
};

/// @brief Light definition
struct LightDef {
    std::string name;
    LightType type = LightType::Point;
    bool enabled = true;
    std::string layer = "world";

    // Type-specific settings
    DirectionalLightDef directional;
    PointLightDef point;
    SpotLightDef spot;
    AreaLightDef area;
    HemisphereLightDef hemisphere;

    // Animation
    bool animate = false;
    std::string animation_type;  // "flicker", "pulse", "color_cycle"
    float animation_speed = 1.0f;
};

// =============================================================================
// Shadows
// =============================================================================

/// @brief Shadow quality preset
enum class ShadowQuality {
    Off,
    Low,
    Medium,
    High,
    Ultra
};

/// @brief Shadow filter type
enum class ShadowFilter {
    None,
    PCF,       // Percentage Closer Filtering
    PCSS,      // Percentage Closer Soft Shadows
    VSM,       // Variance Shadow Maps
    ESM        // Exponential Shadow Maps
};

/// @brief Shadow cascade settings
struct ShadowCascadeDef {
    int count = 4;
    std::vector<float> splits;  // Split distances
    float blend_distance = 5.0f;
    bool stabilize = true;
};

/// @brief Shadows configuration
struct ShadowsDef {
    bool enabled = true;
    ShadowQuality quality = ShadowQuality::Medium;
    ShadowFilter filter = ShadowFilter::PCF;
    int map_size = 2048;
    float bias = 0.001f;
    float normal_bias = 0.01f;
    float max_distance = 100.0f;
    ShadowCascadeDef cascades;
    bool contact_shadows = false;
    float contact_shadow_length = 0.1f;
};

// =============================================================================
// Environment
// =============================================================================

/// @brief Sky type
enum class SkyType {
    None,
    Color,
    Gradient,
    Skybox,
    Procedural,
    HDRI
};

/// @brief Sky definition
struct SkyDef {
    SkyType type = SkyType::Color;
    Color3 color = {0.5f, 0.7f, 1.0f};
    Color3 horizon_color = {0.8f, 0.9f, 1.0f};
    Color3 ground_color = {0.3f, 0.25f, 0.2f};
    std::string texture;  // Skybox or HDRI path
    float rotation = 0.0f;
    float exposure = 1.0f;

    // Procedural sky
    float sun_size = 0.04f;
    float atmosphere_density = 1.0f;
    float rayleigh_coefficient = 1.0f;
    float mie_coefficient = 0.005f;
};

/// @brief Fog definition
struct FogDef {
    bool enabled = false;
    Color3 color = {0.5f, 0.6f, 0.7f};
    float density = 0.01f;
    float start = 10.0f;
    float end = 100.0f;
    float height_falloff = 0.5f;
    bool height_fog = false;
    float max_opacity = 1.0f;
};

/// @brief Ambient occlusion settings
struct AmbientOcclusionDef {
    bool enabled = true;
    float intensity = 1.0f;
    float radius = 0.5f;
    float bias = 0.025f;
    int samples = 16;
    bool temporal = true;
};

/// @brief Environment definition
struct EnvironmentDef {
    SkyDef sky;
    FogDef fog;
    AmbientOcclusionDef ambient_occlusion;
    Color3 ambient_color = {0.1f, 0.1f, 0.15f};
    float ambient_intensity = 0.3f;
    std::string environment_map;
    float environment_intensity = 1.0f;
    std::string reflection_probe;
};

// =============================================================================
// Picking Configuration
// =============================================================================

/// @brief Picking mode
enum class PickingMode {
    None,
    Click,
    Hover,
    Both
};

/// @brief Picking configuration
struct PickingDef {
    bool enabled = false;
    PickingMode mode = PickingMode::Click;
    float max_distance = 1000.0f;
    std::vector<std::string> layers;  // Layers to pick from
    bool highlight_on_hover = true;
    Color4 highlight_color = {1.0f, 1.0f, 0.0f, 0.3f};
};

// =============================================================================
// Spatial Configuration
// =============================================================================

/// @brief Spatial structure type
enum class SpatialType {
    None,
    BVH,
    Octree,
    Grid
};

/// @brief Spatial query configuration
struct SpatialDef {
    SpatialType type = SpatialType::BVH;
    int max_objects_per_node = 8;
    int max_depth = 16;
    Vec3 world_bounds_min = {-1000.0f, -1000.0f, -1000.0f};
    Vec3 world_bounds_max = {1000.0f, 1000.0f, 1000.0f};
    float grid_cell_size = 10.0f;
    bool dynamic_update = true;
};

// =============================================================================
// Debug Configuration
// =============================================================================

/// @brief Debug visualization configuration
struct DebugDef {
    bool show_wireframe = false;
    bool show_normals = false;
    bool show_bounds = false;
    bool show_colliders = false;
    bool show_lights = false;
    bool show_cameras = false;
    bool show_skeleton = false;
    bool show_navmesh = false;
    bool show_fps = false;
    bool show_stats = false;
    Color3 wireframe_color = {1.0f, 1.0f, 1.0f};
    Color3 bounds_color = {0.0f, 1.0f, 0.0f};
    Color3 collider_color = {0.0f, 0.0f, 1.0f};
};

// =============================================================================
// Transform Definition
// =============================================================================

/// @brief Transform definition
struct TransformDef {
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Vec3 rotation = {0.0f, 0.0f, 0.0f};  // Euler angles in degrees
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    Quat quaternion = {0.0f, 0.0f, 0.0f, 1.0f};  // Optional quaternion rotation
    bool use_quaternion = false;
};

// =============================================================================
// Mesh Definition
// =============================================================================

/// @brief Mesh primitive type
enum class MeshPrimitive {
    None,
    Cube,
    Sphere,
    Cylinder,
    Capsule,
    Cone,
    Plane,
    Quad,
    Torus,
    Custom
};

/// @brief Mesh definition
struct MeshDef {
    std::string file;  // Path to mesh file (.obj, .gltf, .fbx)
    MeshPrimitive primitive = MeshPrimitive::None;

    // Primitive parameters
    Vec3 size = {1.0f, 1.0f, 1.0f};  // For cube
    float radius = 0.5f;              // For sphere, cylinder, capsule
    float height = 1.0f;              // For cylinder, capsule, cone
    int segments = 32;                // Tessellation
    int rings = 16;
    float inner_radius = 0.25f;       // For torus
    float outer_radius = 0.5f;

    // LOD
    std::vector<std::string> lod_files;
    std::vector<float> lod_distances;
};

// =============================================================================
// Material Definition (PBR + Advanced)
// =============================================================================

/// @brief Color or texture reference
struct ColorOrTexture {
    Color4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    std::string texture;
    Vec2 uv_scale = {1.0f, 1.0f};
    Vec2 uv_offset = {0.0f, 0.0f};
    bool has_texture = false;
};

/// @brief Float or texture reference
struct FloatOrTexture {
    float value = 0.0f;
    std::string texture;
    bool has_texture = false;
};

/// @brief Transmission (glass/water) settings
struct TransmissionDef {
    bool enabled = false;
    float factor = 0.0f;
    std::string texture;
    float ior = 1.5f;  // Index of refraction
    float thickness = 0.0f;
    Color3 attenuation_color = {1.0f, 1.0f, 1.0f};
    float attenuation_distance = 0.0f;
};

/// @brief Sheen (velvet/fabric) settings
struct SheenDef {
    bool enabled = false;
    Color3 color = {0.0f, 0.0f, 0.0f};
    float roughness = 0.0f;
    std::string color_texture;
    std::string roughness_texture;
};

/// @brief Clearcoat settings
struct ClearcoatDef {
    bool enabled = false;
    float factor = 0.0f;
    float roughness = 0.0f;
    std::string texture;
    std::string roughness_texture;
    std::string normal_texture;
};

/// @brief Anisotropy settings
struct AnisotropyDef {
    bool enabled = false;
    float strength = 0.0f;
    float rotation = 0.0f;
    std::string texture;
    std::string direction_texture;
};

/// @brief Subsurface scattering settings
struct SubsurfaceDef {
    bool enabled = false;
    float factor = 0.0f;
    Color3 color = {1.0f, 0.2f, 0.1f};
    float radius = 1.0f;
    std::string texture;
};

/// @brief Iridescence settings
struct IridescenceDef {
    bool enabled = false;
    float factor = 0.0f;
    float ior = 1.3f;
    float thickness_min = 100.0f;
    float thickness_max = 400.0f;
    std::string texture;
    std::string thickness_texture;
};

/// @brief Material definition (full PBR + advanced)
struct MaterialDef {
    std::string name;
    std::string shader;  // Custom shader override

    // Basic PBR
    ColorOrTexture albedo;
    FloatOrTexture metallic;
    FloatOrTexture roughness;
    std::string normal_map;
    float normal_scale = 1.0f;
    std::string occlusion_map;
    float occlusion_strength = 1.0f;
    ColorOrTexture emissive;
    float emissive_intensity = 1.0f;

    // Alpha
    float alpha_cutoff = 0.5f;
    bool alpha_blend = false;
    bool double_sided = false;

    // Advanced materials
    TransmissionDef transmission;
    SheenDef sheen;
    ClearcoatDef clearcoat;
    AnisotropyDef anisotropy;
    SubsurfaceDef subsurface;
    IridescenceDef iridescence;

    // Displacement/parallax
    std::string height_map;
    float height_scale = 0.1f;
    bool parallax_occlusion = false;

    // Detail textures
    std::string detail_albedo;
    std::string detail_normal;
    Vec2 detail_scale = {1.0f, 1.0f};
};

// =============================================================================
// Animation Definition
// =============================================================================

/// @brief Animation type
enum class AnimationType {
    None,
    Rotate,
    Oscillate,
    Path,
    Orbit,
    Pulse,
    Skeletal,
    Morph
};

/// @brief Animation easing
enum class AnimationEasing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Bounce,
    Elastic
};

/// @brief Rotation animation
struct RotateAnimDef {
    Vec3 axis = {0.0f, 1.0f, 0.0f};
    float speed = 1.0f;  // Rotations per second
    bool local_space = true;
};

/// @brief Oscillate animation
struct OscillateAnimDef {
    Vec3 axis = {0.0f, 1.0f, 0.0f};
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float phase = 0.0f;
    AnimationEasing easing = AnimationEasing::Linear;
};

/// @brief Path animation waypoint
struct PathWaypoint {
    Vec3 position;
    Quat rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    float time = 0.0f;
    AnimationEasing easing = AnimationEasing::Linear;
};

/// @brief Path animation
struct PathAnimDef {
    std::vector<PathWaypoint> waypoints;
    bool loop = true;
    bool ping_pong = false;
    float duration = 1.0f;
    bool orient_to_path = false;
};

/// @brief Orbit animation
struct OrbitAnimDef {
    Vec3 center = {0.0f, 0.0f, 0.0f};
    Vec3 axis = {0.0f, 1.0f, 0.0f};
    float radius = 5.0f;
    float speed = 1.0f;
    bool face_center = true;
};

/// @brief Pulse animation
struct PulseAnimDef {
    Vec3 scale_min = {0.9f, 0.9f, 0.9f};
    Vec3 scale_max = {1.1f, 1.1f, 1.1f};
    float frequency = 1.0f;
    AnimationEasing easing = AnimationEasing::EaseInOut;
};

/// @brief Animation definition
struct AnimationDef {
    AnimationType type = AnimationType::None;
    bool enabled = true;
    bool play_on_start = true;

    // Type-specific settings
    RotateAnimDef rotate;
    OscillateAnimDef oscillate;
    PathAnimDef path;
    OrbitAnimDef orbit;
    PulseAnimDef pulse;

    // Skeletal animation
    std::string animation_file;
    std::string animation_name;
    float speed = 1.0f;
    bool loop = true;
    float blend_time = 0.2f;
};

// =============================================================================
// Physics Definition
// =============================================================================

/// @brief Physics body type
enum class PhysicsBodyType {
    Static,
    Dynamic,
    Kinematic
};

/// @brief Collider shape type
enum class ColliderShape {
    Box,
    Sphere,
    Capsule,
    Cylinder,
    Mesh,
    Convex,
    Compound
};

/// @brief Capsule axis
enum class CapsuleAxis {
    X,
    Y,
    Z
};

/// @brief Physics material
struct PhysicsMaterialDef {
    float friction = 0.5f;
    float restitution = 0.3f;
    float density = 1.0f;
};

/// @brief Collider definition
struct ColliderDef {
    ColliderShape shape = ColliderShape::Box;
    Vec3 size = {1.0f, 1.0f, 1.0f};
    float radius = 0.5f;
    float height = 1.0f;
    CapsuleAxis capsule_axis = CapsuleAxis::Y;
    Vec3 offset = {0.0f, 0.0f, 0.0f};
    Quat rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    std::string mesh;  // For mesh/convex colliders
    PhysicsMaterialDef material;
    bool is_trigger = false;
};

/// @brief Collision groups
struct CollisionGroupsDef {
    std::uint32_t group = 1;
    std::uint32_t mask = 0xFFFFFFFF;
    std::vector<std::string> collides_with;
    std::vector<std::string> ignores;
};

/// @brief Joint type
enum class JointType {
    Fixed,
    Hinge,
    Slider,
    Ball,
    Distance,
    Cone,
    Spring
};

/// @brief Joint definition
struct JointDef {
    JointType type = JointType::Fixed;
    std::string connected_body;
    Vec3 anchor = {0.0f, 0.0f, 0.0f};
    Vec3 connected_anchor = {0.0f, 0.0f, 0.0f};
    Vec3 axis = {1.0f, 0.0f, 0.0f};
    float min_limit = 0.0f;
    float max_limit = 0.0f;
    float spring_stiffness = 0.0f;
    float spring_damping = 0.0f;
    bool enable_collision = false;
    float break_force = -1.0f;
    float break_torque = -1.0f;
};

/// @brief Character controller definition
struct CharacterControllerDef {
    float height = 1.8f;
    float radius = 0.3f;
    float step_offset = 0.3f;
    float slope_limit = 45.0f;
    float skin_width = 0.02f;
    Vec3 center = {0.0f, 0.9f, 0.0f};
};

/// @brief Physics definition
struct PhysicsDef {
    PhysicsBodyType body_type = PhysicsBodyType::Static;
    float mass = 1.0f;
    float linear_damping = 0.0f;
    float angular_damping = 0.05f;
    Vec3 center_of_mass = {0.0f, 0.0f, 0.0f};
    bool use_gravity = true;
    bool is_kinematic = false;
    bool continuous_collision = false;

    std::vector<ColliderDef> colliders;
    CollisionGroupsDef collision_groups;
    std::vector<JointDef> joints;

    std::optional<CharacterControllerDef> character_controller;

    // Constraints
    bool freeze_position_x = false;
    bool freeze_position_y = false;
    bool freeze_position_z = false;
    bool freeze_rotation_x = false;
    bool freeze_rotation_y = false;
    bool freeze_rotation_z = false;
};

// =============================================================================
// Particle Emitter Definition
// =============================================================================

/// @brief Particle emission shape
enum class EmissionShape {
    Point,
    Sphere,
    Hemisphere,
    Cone,
    Box,
    Circle,
    Edge,
    Mesh
};

/// @brief Particle emitter definition
struct ParticleEmitterDef {
    std::string name;
    Vec3 position = {0.0f, 0.0f, 0.0f};
    bool enabled = true;
    std::string layer = "particles";

    // Emission
    EmissionShape shape = EmissionShape::Point;
    float emission_rate = 10.0f;
    int max_particles = 1000;
    Vec3 shape_size = {1.0f, 1.0f, 1.0f};
    float shape_radius = 1.0f;
    float shape_angle = 45.0f;

    // Particle properties
    float lifetime_min = 1.0f;
    float lifetime_max = 2.0f;
    float speed_min = 1.0f;
    float speed_max = 5.0f;
    float size_min = 0.1f;
    float size_max = 0.5f;
    Color4 color_start = {1.0f, 1.0f, 1.0f, 1.0f};
    Color4 color_end = {1.0f, 1.0f, 1.0f, 0.0f};

    // Physics
    Vec3 gravity = {0.0f, -9.81f, 0.0f};
    float drag = 0.0f;
    bool world_space = true;

    // Rendering
    std::string texture;
    std::string material;
    bool additive_blend = false;
    bool face_camera = true;

    // Animation
    int texture_rows = 1;
    int texture_cols = 1;
    float animation_speed = 1.0f;
    bool random_start_frame = false;
};

// =============================================================================
// Game Systems Definitions
// =============================================================================

/// @brief Health component
struct HealthDef {
    float max_health = 100.0f;
    float current_health = 100.0f;
    float max_shields = 0.0f;
    float current_shields = 0.0f;
    float max_armor = 0.0f;
    float current_armor = 0.0f;
    float health_regen = 0.0f;
    float shield_regen = 0.0f;
    float regen_delay = 3.0f;
    bool invulnerable = false;
    float invulnerability_time = 0.0f;
};

/// @brief Weapon type
enum class WeaponType {
    Hitscan,
    Projectile,
    Melee,
    Beam,
    Area
};

/// @brief Weapon definition
struct WeaponDef {
    std::string name;
    WeaponType type = WeaponType::Hitscan;
    float damage = 10.0f;
    float fire_rate = 10.0f;  // Rounds per second
    float range = 100.0f;
    float spread = 0.0f;
    int magazine_size = 30;
    int current_ammo = 30;
    int reserve_ammo = 90;
    float reload_time = 2.0f;
    std::string damage_type = "physical";

    // Projectile settings
    float projectile_speed = 50.0f;
    float projectile_gravity = 0.0f;
    std::string projectile_prefab;

    // Melee settings
    float melee_arc = 90.0f;
    float attack_duration = 0.5f;

    // Effects
    std::string fire_sound;
    std::string reload_sound;
    std::string impact_effect;
    std::string muzzle_flash;
    Vec3 recoil = {0.0f, 0.0f, 0.0f};
};

/// @brief Inventory slot
struct InventorySlotDef {
    std::string item_id;
    int count = 1;
};

/// @brief Inventory definition
struct InventoryDef {
    int max_slots = 20;
    float max_weight = 100.0f;
    std::vector<InventorySlotDef> starting_items;
};

/// @brief AI behavior type
enum class AiBehavior {
    Idle,
    Patrol,
    Guard,
    Follow,
    Flee,
    Attack,
    Custom
};

/// @brief AI definition
struct AiDef {
    AiBehavior behavior = AiBehavior::Idle;
    float detection_range = 20.0f;
    float attack_range = 5.0f;
    float fov = 120.0f;
    float move_speed = 3.0f;
    float turn_speed = 180.0f;
    std::vector<Vec3> patrol_points;
    std::string target_tag;
    std::string behavior_tree;
    std::string blackboard_preset;
};

/// @brief Trigger action
struct TriggerActionDef {
    std::string type;  // "spawn", "destroy", "activate", "deactivate", "teleport", etc.
    std::string target;
    std::unordered_map<std::string, ScriptValue> parameters;
};

/// @brief Trigger definition
struct TriggerDef {
    ColliderShape shape = ColliderShape::Box;
    Vec3 size = {1.0f, 1.0f, 1.0f};
    float radius = 1.0f;
    bool once = false;
    float cooldown = 0.0f;
    std::vector<std::string> filter_tags;
    std::vector<TriggerActionDef> on_enter;
    std::vector<TriggerActionDef> on_exit;
    std::vector<TriggerActionDef> on_stay;
};

/// @brief Event binding definition
struct EventBindingDef {
    std::string event_name;
    std::string handler;  // Function or blueprint
    std::unordered_map<std::string, ScriptValue> parameters;
};

/// @brief Script component definition
struct ScriptDef {
    std::string cpp_class;
    std::string blueprint;
    std::string voidscript;
    std::string wasm_module;
    std::unordered_map<std::string, ScriptValue> properties;
    std::vector<EventBindingDef> event_bindings;
};

/// @brief LOD level definition
struct LodLevelDef {
    std::string mesh;
    float distance = 0.0f;
    float screen_size = 1.0f;
};

/// @brief LOD definition
struct LodDef {
    std::vector<LodLevelDef> levels;
    float bias = 0.0f;
    bool fade_transition = true;
    float fade_duration = 0.2f;
};

/// @brief Render settings for entity
struct RenderSettingsDef {
    bool visible = true;
    bool cast_shadows = true;
    bool receive_shadows = true;
    bool static_object = false;
    int render_order = 0;
    std::string render_layer;
};

// =============================================================================
// Entity Definition
// =============================================================================

/// @brief Complete entity definition
struct EntityDef {
    std::string name;
    std::string prefab;  // Prefab to instantiate
    std::string parent;  // Parent entity name
    std::string layer = "world";
    std::vector<std::string> tags;
    bool active = true;

    // Core components
    TransformDef transform;
    std::optional<MeshDef> mesh;
    std::optional<MaterialDef> material;
    std::optional<AnimationDef> animation;
    std::optional<PhysicsDef> physics;

    // Game systems
    std::optional<HealthDef> health;
    std::optional<WeaponDef> weapon;
    std::optional<InventoryDef> inventory;
    std::optional<AiDef> ai;
    std::optional<TriggerDef> trigger;
    std::optional<ScriptDef> script;

    // Rendering
    std::optional<LodDef> lod;
    RenderSettingsDef render_settings;

    // Light attachment
    std::optional<LightDef> light;

    // Custom properties
    std::unordered_map<std::string, ScriptValue> properties;

    // Child entities (for hierarchy)
    std::vector<EntityDef> children;
};

// =============================================================================
// Texture Definition
// =============================================================================

/// @brief Texture filter mode
enum class TextureFilter {
    Nearest,
    Linear,
    Trilinear,
    Anisotropic
};

/// @brief Texture wrap mode
enum class TextureWrap {
    Repeat,
    Clamp,
    Mirror,
    Border
};

/// @brief Texture definition for preloading
struct TextureDef {
    std::string name;
    std::string path;
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Repeat;
    bool generate_mips = true;
    bool srgb = true;
    int max_anisotropy = 8;
};

// =============================================================================
// Input Configuration
// =============================================================================

/// @brief Input binding
struct InputBindingDef {
    std::string action;
    std::vector<std::string> keys;
    std::vector<std::string> mouse_buttons;
    std::vector<std::string> gamepad_buttons;
    std::string gamepad_axis;
    float dead_zone = 0.1f;
    bool invert = false;
};

/// @brief Input configuration
struct InputConfig {
    std::vector<InputBindingDef> bindings;
    float mouse_sensitivity = 1.0f;
    float gamepad_sensitivity = 1.0f;
    bool invert_y = false;
};

// =============================================================================
// Item Definitions (for scene-level items)
// =============================================================================

/// @brief Item type
enum class ItemType {
    Misc,
    Consumable,
    Equipment,
    Weapon,
    Key,
    Quest,
    Currency
};

/// @brief Item rarity
enum class ItemRarity {
    Common,
    Uncommon,
    Rare,
    Epic,
    Legendary
};

/// @brief Consumable effect
struct ConsumableEffectDef {
    std::string type;  // "restore_health", "restore_mana", "buff", etc.
    float amount = 0.0f;
    float duration = 0.0f;
    std::string status_effect;
};

/// @brief Item definition
struct ItemDef {
    std::string id;
    std::string name;
    std::string description;
    ItemType type = ItemType::Misc;
    ItemRarity rarity = ItemRarity::Common;
    int max_stack = 1;
    float weight = 0.0f;
    int value = 0;
    std::string icon;
    std::string model;

    // Consumable
    float use_time = 0.0f;
    std::string use_animation;
    std::vector<ConsumableEffectDef> effects;

    // Equipment
    std::string slot;  // "head", "chest", "weapon", etc.
    std::unordered_map<std::string, float> stats;
};

// =============================================================================
// Status Effect Definition
// =============================================================================

/// @brief Status effect type
enum class StatusEffectType {
    Buff,
    Debuff,
    Dot,       // Damage over time
    Hot,       // Heal over time
    Crowd_Control
};

/// @brief Status effect definition
struct StatusEffectDef {
    std::string name;
    StatusEffectType type = StatusEffectType::Buff;
    float duration = 5.0f;
    float tick_rate = 1.0f;
    bool stacks = false;
    int max_stacks = 1;
    std::string icon;
    std::vector<ConsumableEffectDef> effects;
};

// =============================================================================
// Quest Definition
// =============================================================================

/// @brief Quest objective type
enum class ObjectiveType {
    Kill,
    Collect,
    Talk,
    Reach,
    Escort,
    Defend,
    Custom
};

/// @brief Quest objective
struct QuestObjectiveDef {
    std::string id;
    std::string description;
    ObjectiveType type = ObjectiveType::Custom;
    std::string target;
    int count = 1;
    bool optional = false;
    std::string marker;
};

/// @brief Quest reward
struct QuestRewardDef {
    std::string type;  // "item", "xp", "currency", "unlock"
    std::string item;
    int count = 1;
    int xp = 0;
    int currency = 0;
};

/// @brief Quest definition
struct QuestDef {
    std::string id;
    std::string name;
    std::string description;
    bool auto_start = false;
    std::vector<std::string> prerequisites;
    std::vector<QuestObjectiveDef> objectives;
    std::vector<QuestRewardDef> rewards;
    std::string on_complete_event;
};

// =============================================================================
// Loot Table Definition
// =============================================================================

/// @brief Loot entry
struct LootEntryDef {
    std::string item_id;
    float weight = 1.0f;
    int count_min = 1;
    int count_max = 1;
};

/// @brief Loot table
struct LootTableDef {
    std::string id;
    std::vector<LootEntryDef> entries;
    int rolls = 1;
    bool allow_duplicates = false;
};

// =============================================================================
// Audio Configuration
// =============================================================================

/// @brief Ambient sound definition
struct AmbientSoundDef {
    std::string name;
    std::string file;
    float volume = 1.0f;
    bool loop = true;
    Vec3 position = {0.0f, 0.0f, 0.0f};
    float min_distance = 1.0f;
    float max_distance = 50.0f;
    bool spatial = true;
};

/// @brief Music track definition
struct MusicTrackDef {
    std::string name;
    std::string file;
    float volume = 1.0f;
    bool loop = true;
    float fade_in = 1.0f;
    float fade_out = 1.0f;
};

/// @brief Reverb zone definition
struct ReverbZoneDef {
    std::string name;
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Vec3 size = {10.0f, 10.0f, 10.0f};
    std::string preset;  // "none", "room", "hall", "cave", "outdoor"
    float mix = 1.0f;
};

/// @brief Scene audio configuration
struct AudioConfigDef {
    std::vector<AmbientSoundDef> ambient;
    std::vector<MusicTrackDef> music;
    std::vector<ReverbZoneDef> reverb_zones;
    std::string default_music;
    float master_volume = 1.0f;
};

// =============================================================================
// Navigation Configuration
// =============================================================================

/// @brief NavMesh configuration
struct NavMeshConfigDef {
    float agent_radius = 0.5f;
    float agent_height = 2.0f;
    float max_slope = 45.0f;
    float step_height = 0.3f;
    float cell_size = 0.3f;
    float cell_height = 0.2f;
    std::vector<std::string> walkable_layers;
};

/// @brief Navigation area definition
struct NavAreaDef {
    std::string name;
    float cost = 1.0f;
    Color3 color = {0.0f, 1.0f, 0.0f};
};

/// @brief Navigation configuration
struct NavigationConfigDef {
    NavMeshConfigDef navmesh;
    std::vector<NavAreaDef> areas;
    bool auto_generate = true;
    bool realtime_update = false;
};

// =============================================================================
// Scene Definition (Root)
// =============================================================================

/// @brief Complete scene definition loaded from scene.toml
struct SceneDefinition {
    // Metadata
    SceneMetadata scene;

    // Camera system
    std::vector<CameraDef> cameras;

    // Lighting
    std::vector<LightDef> lights;
    ShadowsDef shadows;
    EnvironmentDef environment;

    // Systems
    PickingDef picking;
    SpatialDef spatial;
    DebugDef debug;
    InputConfig input;

    // Content
    std::vector<EntityDef> entities;
    std::vector<ParticleEmitterDef> particle_emitters;
    std::vector<TextureDef> textures;

    // Game systems
    std::vector<ItemDef> items;
    std::vector<StatusEffectDef> status_effects;
    std::vector<QuestDef> quests;
    std::vector<LootTableDef> loot_tables;

    // Audio
    std::optional<AudioConfigDef> audio;

    // Navigation
    std::optional<NavigationConfigDef> navigation;

    // Global scripts
    std::vector<ScriptDef> scripts;

    // Prefabs to preload
    std::vector<std::string> prefabs;

    // Scene-level properties
    std::unordered_map<std::string, ScriptValue> properties;
};

} // namespace void_runtime
