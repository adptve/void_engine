/// @file scene_parser.hpp
/// @brief TOML and JSON scene file parser for void_runtime
/// @details Parses scene.toml files matching legacy Rust format

#pragma once

#include "scene_types.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace void_runtime {

// =============================================================================
// TOML Value Types
// =============================================================================

class TomlValue;
using TomlArray = std::vector<TomlValue>;
using TomlTable = std::unordered_map<std::string, TomlValue>;

/// @brief TOML value variant
class TomlValue {
public:
    using ValueType = std::variant<
        std::monostate,    // null/empty
        bool,
        std::int64_t,
        double,
        std::string,
        TomlArray,
        std::shared_ptr<TomlTable>
    >;

    TomlValue() : value_(std::monostate{}) {}
    TomlValue(bool v) : value_(v) {}
    TomlValue(std::int64_t v) : value_(v) {}
    TomlValue(int v) : value_(static_cast<std::int64_t>(v)) {}
    TomlValue(double v) : value_(v) {}
    TomlValue(float v) : value_(static_cast<double>(v)) {}
    TomlValue(const std::string& v) : value_(v) {}
    TomlValue(const char* v) : value_(std::string(v)) {}
    TomlValue(TomlArray v) : value_(std::move(v)) {}
    TomlValue(TomlTable v) : value_(std::make_shared<TomlTable>(std::move(v))) {}

    bool is_null() const { return std::holds_alternative<std::monostate>(value_); }
    bool is_bool() const { return std::holds_alternative<bool>(value_); }
    bool is_int() const { return std::holds_alternative<std::int64_t>(value_); }
    bool is_float() const { return std::holds_alternative<double>(value_); }
    bool is_string() const { return std::holds_alternative<std::string>(value_); }
    bool is_array() const { return std::holds_alternative<TomlArray>(value_); }
    bool is_table() const { return std::holds_alternative<std::shared_ptr<TomlTable>>(value_); }

    bool as_bool(bool def = false) const;
    std::int64_t as_int(std::int64_t def = 0) const;
    double as_float(double def = 0.0) const;
    std::string as_string(const std::string& def = "") const;
    const TomlArray& as_array() const;
    const TomlTable& as_table() const;
    TomlTable& as_table_mut();

    // Helper for nested access
    const TomlValue& operator[](const std::string& key) const;
    const TomlValue& operator[](std::size_t index) const;
    bool has(const std::string& key) const;
    std::size_t size() const;

    // Type conversions
    Vec2 as_vec2(const Vec2& def = {0, 0}) const;
    Vec3 as_vec3(const Vec3& def = {0, 0, 0}) const;
    Vec4 as_vec4(const Vec4& def = {0, 0, 0, 0}) const;
    Color3 as_color3(const Color3& def = {1, 1, 1}) const;
    Color4 as_color4(const Color4& def = {1, 1, 1, 1}) const;

private:
    ValueType value_;
    static const TomlValue null_value_;
    static const TomlArray empty_array_;
    static const TomlTable empty_table_;
};

// =============================================================================
// TOML Parser
// =============================================================================

/// @brief TOML parser for scene files
class TomlParser {
public:
    /// @brief Parse TOML from string
    static std::optional<TomlValue> parse(const std::string& content);

    /// @brief Parse TOML from file
    static std::optional<TomlValue> parse_file(const std::filesystem::path& path);

    /// @brief Get last error message
    static std::string last_error();

private:
    struct ParserState {
        const char* input = nullptr;
        const char* current = nullptr;
        const char* end = nullptr;
        int line = 1;
        int column = 1;
        std::string error;
    };

    static bool parse_document(ParserState& state, TomlTable& root);
    static bool parse_table_header(ParserState& state, std::vector<std::string>& path, bool& is_array);
    static bool parse_key_value(ParserState& state, TomlTable& table);
    static bool parse_key(ParserState& state, std::string& key);
    static bool parse_value(ParserState& state, TomlValue& value);
    static bool parse_string(ParserState& state, std::string& str);
    static bool parse_basic_string(ParserState& state, std::string& str);
    static bool parse_literal_string(ParserState& state, std::string& str);
    static bool parse_multiline_basic_string(ParserState& state, std::string& str);
    static bool parse_multiline_literal_string(ParserState& state, std::string& str);
    static bool parse_number(ParserState& state, TomlValue& value);
    static bool parse_bool(ParserState& state, bool& value);
    static bool parse_array(ParserState& state, TomlArray& arr);
    static bool parse_inline_table(ParserState& state, TomlTable& table);

    static void skip_whitespace(ParserState& state);
    static void skip_whitespace_and_newlines(ParserState& state);
    static void skip_comment(ParserState& state);
    static void skip_to_newline(ParserState& state);
    static bool is_bare_key_char(char c);
    static bool expect(ParserState& state, char c);
    static bool peek(ParserState& state, char c);
    static char current(ParserState& state);
    static void advance(ParserState& state);
    static bool at_end(ParserState& state);
    static void set_error(ParserState& state, const std::string& msg);

    static TomlTable* get_or_create_table(TomlTable& root, const std::vector<std::string>& path);
    static TomlArray* get_or_create_array(TomlTable& root, const std::vector<std::string>& path);

    static thread_local std::string last_error_;
};

// =============================================================================
// Scene Parser
// =============================================================================

/// @brief Scene file parser supporting TOML and JSON
class SceneParser {
public:
    /// @brief Parse scene from file
    static std::optional<SceneDefinition> parse_file(const std::filesystem::path& path);

    /// @brief Parse scene from TOML string
    static std::optional<SceneDefinition> parse_toml(const std::string& content);

    /// @brief Parse scene from JSON string
    static std::optional<SceneDefinition> parse_json(const std::string& content);

    /// @brief Get last error
    static std::string last_error() { return last_error_; }

private:
    // Parse helpers
    static void parse_scene_metadata(const TomlValue& value, SceneMetadata& metadata);
    static void parse_camera(const TomlValue& value, CameraDef& camera);
    static void parse_light(const TomlValue& value, LightDef& light);
    static void parse_shadows(const TomlValue& value, ShadowsDef& shadows);
    static void parse_environment(const TomlValue& value, EnvironmentDef& env);
    static void parse_picking(const TomlValue& value, PickingDef& picking);
    static void parse_spatial(const TomlValue& value, SpatialDef& spatial);
    static void parse_debug(const TomlValue& value, DebugDef& debug);
    static void parse_input(const TomlValue& value, InputConfig& input);
    static void parse_entity(const TomlValue& value, EntityDef& entity);
    static void parse_particle_emitter(const TomlValue& value, ParticleEmitterDef& emitter);
    static void parse_texture(const TomlValue& value, TextureDef& texture);

    // Component parsers
    static void parse_transform(const TomlValue& value, TransformDef& transform);
    static void parse_mesh(const TomlValue& value, MeshDef& mesh);
    static void parse_material(const TomlValue& value, MaterialDef& material);
    static void parse_animation(const TomlValue& value, AnimationDef& anim);
    static void parse_physics(const TomlValue& value, PhysicsDef& physics);
    static void parse_collider(const TomlValue& value, ColliderDef& collider);
    static void parse_health(const TomlValue& value, HealthDef& health);
    static void parse_weapon(const TomlValue& value, WeaponDef& weapon);
    static void parse_inventory(const TomlValue& value, InventoryDef& inventory);
    static void parse_ai(const TomlValue& value, AiDef& ai);
    static void parse_trigger(const TomlValue& value, TriggerDef& trigger);
    static void parse_script(const TomlValue& value, ScriptDef& script);
    static void parse_lod(const TomlValue& value, LodDef& lod);
    static void parse_render_settings(const TomlValue& value, RenderSettingsDef& settings);

    // Game data parsers
    static void parse_item(const TomlValue& value, ItemDef& item);
    static void parse_status_effect(const TomlValue& value, StatusEffectDef& effect);
    static void parse_quest(const TomlValue& value, QuestDef& quest);
    static void parse_loot_table(const TomlValue& value, LootTableDef& loot);
    static void parse_audio_config(const TomlValue& value, AudioConfigDef& audio);
    static void parse_navigation_config(const TomlValue& value, NavigationConfigDef& nav);

    // Enum parsers
    static CameraType parse_camera_type(const std::string& str);
    static CameraControlMode parse_camera_control_mode(const std::string& str);
    static LightType parse_light_type(const std::string& str);
    static ShadowQuality parse_shadow_quality(const std::string& str);
    static ShadowFilter parse_shadow_filter(const std::string& str);
    static SkyType parse_sky_type(const std::string& str);
    static MeshPrimitive parse_mesh_primitive(const std::string& str);
    static AnimationType parse_animation_type(const std::string& str);
    static AnimationEasing parse_animation_easing(const std::string& str);
    static PhysicsBodyType parse_physics_body_type(const std::string& str);
    static ColliderShape parse_collider_shape(const std::string& str);
    static CapsuleAxis parse_capsule_axis(const std::string& str);
    static JointType parse_joint_type(const std::string& str);
    static EmissionShape parse_emission_shape(const std::string& str);
    static WeaponType parse_weapon_type(const std::string& str);
    static AiBehavior parse_ai_behavior(const std::string& str);
    static ItemType parse_item_type(const std::string& str);
    static ItemRarity parse_item_rarity(const std::string& str);
    static StatusEffectType parse_status_effect_type(const std::string& str);
    static ObjectiveType parse_objective_type(const std::string& str);
    static PickingMode parse_picking_mode(const std::string& str);
    static SpatialType parse_spatial_type(const std::string& str);
    static TextureFilter parse_texture_filter(const std::string& str);
    static TextureWrap parse_texture_wrap(const std::string& str);

    static thread_local std::string last_error_;
};

} // namespace void_runtime
