#pragma once

/// @file scene_serializer.hpp
/// @brief Scene serialization to TOML format

#include <void_engine/scene/scene_data.hpp>
#include <void_engine/core/result.hpp>

#include <filesystem>
#include <string>
#include <sstream>

namespace void_scene {

// =============================================================================
// Scene Serializer
// =============================================================================

/// Serializes scene data to TOML format
class SceneSerializer {
public:
    SceneSerializer() = default;

    /// Serialize scene to file
    [[nodiscard]] void_core::Result<void> save(
        const SceneData& scene,
        const std::filesystem::path& path);

    /// Serialize scene to string
    [[nodiscard]] std::string serialize(const SceneData& scene);

    /// Get last error message
    [[nodiscard]] const std::string& last_error() const noexcept { return m_last_error; }

private:
    std::string m_last_error;

    // Serialization helpers
    void write_header(std::ostringstream& ss, const std::string& section);
    void write_array_header(std::ostringstream& ss, const std::string& section);

    void serialize_metadata(std::ostringstream& ss, const SceneMetadata& meta);
    void serialize_camera(std::ostringstream& ss, const CameraData& camera);
    void serialize_light(std::ostringstream& ss, const LightData& light);
    void serialize_entity(std::ostringstream& ss, const EntityData& entity);
    void serialize_transform(std::ostringstream& ss, const TransformData& transform, int indent);
    void serialize_material(std::ostringstream& ss, const MaterialData& material, int indent);
    void serialize_animation(std::ostringstream& ss, const AnimationData& anim, int indent);
    void serialize_environment(std::ostringstream& ss, const EnvironmentData& env);
    void serialize_shadows(std::ostringstream& ss, const ShadowData& shadows);
    void serialize_picking(std::ostringstream& ss, const PickingData& picking);
    void serialize_spatial(std::ostringstream& ss, const SpatialData& spatial);
    void serialize_debug(std::ostringstream& ss, const DebugData& debug);
    void serialize_input(std::ostringstream& ss, const InputData& input);
    void serialize_texture(std::ostringstream& ss, const TextureData& texture);
    void serialize_particle_emitter(std::ostringstream& ss, const ParticleEmitterData& emitter);

    // Value formatting
    static std::string format_vec2(const Vec2& v);
    static std::string format_vec3(const Vec3& v);
    static std::string format_vec4(const Vec4& v);
    static std::string format_color3(const Color3& c);
    static std::string format_string(const std::string& s);
    static std::string format_bool(bool b);
    static std::string indent_str(int level);
};

// =============================================================================
// Scene Differ
// =============================================================================

/// Compares two scenes and generates diff for incremental updates
class SceneDiffer {
public:
    struct EntityDiff {
        std::string name;
        bool added = false;
        bool removed = false;
        bool transform_changed = false;
        bool material_changed = false;
        bool animation_changed = false;
    };

    struct SceneDiff {
        std::vector<EntityDiff> entity_diffs;
        bool cameras_changed = false;
        bool lights_changed = false;
        bool environment_changed = false;
        bool shadows_changed = false;

        [[nodiscard]] bool has_changes() const {
            return !entity_diffs.empty() || cameras_changed ||
                   lights_changed || environment_changed || shadows_changed;
        }
    };

    /// Compare two scenes and return differences
    [[nodiscard]] SceneDiff diff(const SceneData& old_scene, const SceneData& new_scene);

private:
    bool compare_transforms(const TransformData& a, const TransformData& b);
    bool compare_materials(const MaterialData& a, const MaterialData& b);
    bool compare_animations(const AnimationData& a, const AnimationData& b);
};

} // namespace void_scene
