/// @file types.hpp
/// @brief Core types and enumerations for void_triggers module

#pragma once

#include "fwd.hpp"

#include <any>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_triggers {

// =============================================================================
// Geometry Types
// =============================================================================

/// @brief 3D Vector
struct Vec3 {
    float x{0}, y{0}, z{0};

    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
    float length_squared() const { return x * x + y * y + z * z; }
};

/// @brief Quaternion for orientation
struct Quat {
    float x{0}, y{0}, z{0}, w{1};
};

/// @brief Axis-Aligned Bounding Box
struct AABB {
    Vec3 min;
    Vec3 max;

    Vec3 center() const {
        return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f};
    }

    Vec3 extents() const {
        return {(max.x - min.x) * 0.5f, (max.y - min.y) * 0.5f, (max.z - min.z) * 0.5f};
    }

    bool contains(const Vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool intersects(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
};

/// @brief Sphere shape
struct Sphere {
    Vec3 center;
    float radius{1.0f};

    bool contains(const Vec3& point) const {
        Vec3 diff = {point.x - center.x, point.y - center.y, point.z - center.z};
        return diff.length_squared() <= radius * radius;
    }
};

/// @brief Capsule shape (cylinder with hemispherical caps)
struct Capsule {
    Vec3 start;
    Vec3 end;
    float radius{0.5f};
};

/// @brief Oriented Bounding Box
struct OrientedBox {
    Vec3 center;
    Vec3 half_extents;
    Quat orientation;
};

// =============================================================================
// Trigger Enumerations
// =============================================================================

/// @brief Type of trigger activation
enum class TriggerType : std::uint8_t {
    Enter,          ///< Triggered on entry
    Exit,           ///< Triggered on exit
    Stay,           ///< Triggered while inside
    EnterExit,      ///< Triggered on both enter and exit
    Interact,       ///< Triggered by interaction (e.g., button press)
    Proximity,      ///< Triggered by proximity (distance-based)
    Timed,          ///< Triggered by timer
    Event,          ///< Triggered by event
    Sequence,       ///< Triggered by sequence of events
    Custom          ///< Custom trigger logic
};

/// @brief Volume shape type
enum class VolumeType : std::uint8_t {
    Box,
    Sphere,
    Capsule,
    OrientedBox,
    Mesh,
    Composite
};

/// @brief Trigger state
enum class TriggerState : std::uint8_t {
    Inactive,       ///< Not yet triggered
    Active,         ///< Currently active
    Triggered,      ///< Has been triggered
    Cooldown,       ///< In cooldown period
    Disabled        ///< Manually disabled
};

/// @brief Trigger activation flags
enum class TriggerFlags : std::uint32_t {
    None            = 0,
    OneShot         = 1 << 0,   ///< Only trigger once
    RequireAllTags  = 1 << 1,   ///< Entity must have all tags
    RequireAnyTag   = 1 << 2,   ///< Entity must have at least one tag
    IgnorePlayer    = 1 << 3,   ///< Don't trigger on player
    PlayerOnly      = 1 << 4,   ///< Only trigger on player
    Debug           = 1 << 5,   ///< Show debug visualization
    Persistent      = 1 << 6,   ///< Persist across saves
    DelayedReset    = 1 << 7,   ///< Reset after delay instead of immediate
    ChainedTrigger  = 1 << 8,   ///< Part of a trigger chain
    Interruptible   = 1 << 9,   ///< Can be interrupted during execution
};

inline TriggerFlags operator|(TriggerFlags a, TriggerFlags b) {
    return static_cast<TriggerFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline TriggerFlags operator&(TriggerFlags a, TriggerFlags b) {
    return static_cast<TriggerFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline bool has_flag(TriggerFlags flags, TriggerFlags flag) {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0;
}

// =============================================================================
// Condition Enumerations
// =============================================================================

/// @brief Comparison operators
enum class CompareOp : std::uint8_t {
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Contains,
    NotContains
};

/// @brief Logical operators for combining conditions
enum class LogicalOp : std::uint8_t {
    And,
    Or,
    Not,
    Xor,
    Nand,
    Nor
};

/// @brief Variable types
enum class VariableType : std::uint8_t {
    Bool,
    Int,
    Float,
    String,
    Vector,
    Entity,
    Custom
};

// =============================================================================
// Action Enumerations
// =============================================================================

/// @brief Action execution mode
enum class ActionMode : std::uint8_t {
    Immediate,      ///< Execute immediately
    Delayed,        ///< Execute after delay
    Continuous,     ///< Execute continuously while active
    Interpolated    ///< Interpolate over time
};

/// @brief Action result
enum class ActionResult : std::uint8_t {
    Success,
    Failed,
    Running,        ///< Still executing
    Cancelled
};

// =============================================================================
// Event Types
// =============================================================================

/// @brief Trigger event type
enum class TriggerEventType : std::uint8_t {
    Enter,
    Exit,
    Interact,
    Activate,
    Deactivate,
    Timer,
    Condition,
    Custom
};

/// @brief Trigger event data
struct TriggerEvent {
    TriggerEventId id;
    TriggerEventType type{TriggerEventType::Enter};
    TriggerId trigger;
    EntityId entity;            ///< Entity that triggered
    Vec3 position;              ///< Position of trigger
    double timestamp{0};
    std::string custom_type;
    std::unordered_map<std::string, std::any> data;

    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (...) {}
        }
        return default_value;
    }

    template<typename T>
    void set(const std::string& key, const T& value) {
        data[key] = value;
    }
};

// =============================================================================
// Configuration Structures
// =============================================================================

/// @brief Configuration for a trigger
struct TriggerConfig {
    std::string name;
    TriggerType type{TriggerType::Enter};
    TriggerFlags flags{TriggerFlags::None};

    // Activation
    std::uint32_t max_activations{0};   ///< 0 = unlimited
    float cooldown{0};                   ///< Cooldown between activations
    float delay{0};                      ///< Delay before action execution
    float duration{0};                   ///< Duration for continuous triggers

    // Filtering
    std::vector<std::string> required_tags;
    std::vector<std::string> excluded_tags;
    std::uint32_t layer_mask{0xFFFFFFFF};

    // Proximity (for proximity triggers)
    float proximity_radius{5.0f};
    float proximity_angle{360.0f};       ///< Cone angle for directional

    // Priority
    int priority{0};                     ///< Higher priority triggers first
};

/// @brief Trigger zone configuration
struct ZoneConfig {
    std::string name;
    VolumeType volume_type{VolumeType::Box};
    Vec3 position;
    Quat rotation{0, 0, 0, 1};

    // Box
    Vec3 half_extents{1, 1, 1};

    // Sphere
    float radius{1.0f};

    // Capsule
    float capsule_height{2.0f};
    float capsule_radius{0.5f};

    bool enabled{true};
};

/// @brief Configuration for trigger system
struct TriggerSystemConfig {
    std::uint32_t max_triggers{10000};
    std::uint32_t max_zones{5000};
    float update_frequency{60.0f};      ///< Updates per second
    bool spatial_hashing{true};
    float spatial_cell_size{10.0f};
    bool debug_rendering{false};
};

// =============================================================================
// Callback Types
// =============================================================================

using TriggerCallback = std::function<void(const TriggerEvent&)>;
using ConditionCallback = std::function<bool(const TriggerEvent&)>;
using ActionCallback = std::function<ActionResult(const TriggerEvent&, float dt)>;
using EntityPositionCallback = std::function<Vec3(EntityId)>;
using EntityTagsCallback = std::function<std::vector<std::string>(EntityId)>;

// =============================================================================
// Variable Value
// =============================================================================

/// @brief Runtime variable value
struct VariableValue {
    VariableType type{VariableType::Bool};
    std::any value;

    VariableValue() = default;

    explicit VariableValue(bool v) : type(VariableType::Bool), value(v) {}
    explicit VariableValue(int v) : type(VariableType::Int), value(v) {}
    explicit VariableValue(float v) : type(VariableType::Float), value(v) {}
    explicit VariableValue(const std::string& v) : type(VariableType::String), value(v) {}
    explicit VariableValue(const Vec3& v) : type(VariableType::Vector), value(v) {}

    bool as_bool() const {
        if (type == VariableType::Bool) return std::any_cast<bool>(value);
        if (type == VariableType::Int) return std::any_cast<int>(value) != 0;
        if (type == VariableType::Float) return std::any_cast<float>(value) != 0;
        return false;
    }

    int as_int() const {
        if (type == VariableType::Int) return std::any_cast<int>(value);
        if (type == VariableType::Float) return static_cast<int>(std::any_cast<float>(value));
        if (type == VariableType::Bool) return std::any_cast<bool>(value) ? 1 : 0;
        return 0;
    }

    float as_float() const {
        if (type == VariableType::Float) return std::any_cast<float>(value);
        if (type == VariableType::Int) return static_cast<float>(std::any_cast<int>(value));
        if (type == VariableType::Bool) return std::any_cast<bool>(value) ? 1.0f : 0.0f;
        return 0;
    }

    std::string as_string() const {
        if (type == VariableType::String) return std::any_cast<std::string>(value);
        return "";
    }

    Vec3 as_vector() const {
        if (type == VariableType::Vector) return std::any_cast<Vec3>(value);
        return Vec3{};
    }

    bool compare(const VariableValue& other, CompareOp op) const;
};

// =============================================================================
// Trigger Info
// =============================================================================

/// @brief Runtime trigger information
struct TriggerInfo {
    TriggerId id;
    std::string name;
    TriggerType type{TriggerType::Enter};
    TriggerFlags flags{TriggerFlags::None};
    Vec3 position;
    bool enabled{true};
    std::uint32_t activation_count{0};
    double last_activation{0};
    std::vector<EntityId> entities_inside;
};

} // namespace void_triggers
