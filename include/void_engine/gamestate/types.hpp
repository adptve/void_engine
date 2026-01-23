/// @file types.hpp
/// @brief Core types and enumerations for void_gamestate module

#pragma once

#include "fwd.hpp"

#include <any>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_gamestate {

// =============================================================================
// Variable Types
// =============================================================================

/// @brief Type of game variable
enum class VariableType : std::uint8_t {
    Bool,
    Int,
    Float,
    String,
    Vector3,
    Color,
    EntityRef,
    Custom
};

/// @brief Variable scope
enum class VariableScope : std::uint8_t {
    Global,         ///< Global game state
    Level,          ///< Per-level state
    Entity,         ///< Per-entity state
    Session,        ///< Current play session only
    Persistent      ///< Persists across sessions
};

/// @brief Variable persistence flags
enum class PersistenceFlags : std::uint8_t {
    None            = 0,
    SaveToFile      = 1 << 0,   ///< Include in save files
    SyncNetwork     = 1 << 1,   ///< Sync across network
    ResetOnLoad     = 1 << 2,   ///< Reset when loading
    Track           = 1 << 3,   ///< Track changes for replay
};

inline PersistenceFlags operator|(PersistenceFlags a, PersistenceFlags b) {
    return static_cast<PersistenceFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline bool has_flag(PersistenceFlags flags, PersistenceFlags flag) {
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) != 0;
}

// =============================================================================
// Game Phase Types
// =============================================================================

/// @brief Game phase type
enum class PhaseType : std::uint8_t {
    Menu,
    Loading,
    Gameplay,
    Pause,
    Cutscene,
    Dialog,
    Inventory,
    Combat,
    GameOver,
    Victory,
    Credits,
    Custom
};

/// @brief Phase transition type
enum class TransitionType : std::uint8_t {
    Immediate,
    FadeOut,
    FadeIn,
    CrossFade,
    Wipe,
    Dissolve,
    Custom
};

// =============================================================================
// Save/Load Types
// =============================================================================

/// @brief Save type
enum class SaveType : std::uint8_t {
    Manual,         ///< Player initiated
    Auto,           ///< Automatic save
    Checkpoint,     ///< Checkpoint save
    Quick,          ///< Quick save
    Cloud           ///< Cloud save
};

/// @brief Save result
enum class SaveResult : std::uint8_t {
    Success,
    Failed,
    NoSpace,
    Corrupted,
    VersionMismatch,
    Cancelled,
    InProgress
};

/// @brief Load result
enum class LoadResult : std::uint8_t {
    Success,
    Failed,
    NotFound,
    Corrupted,
    VersionMismatch,
    InProgress
};

// =============================================================================
// Objective Types
// =============================================================================

/// @brief Objective state
enum class ObjectiveState : std::uint8_t {
    Hidden,         ///< Not yet revealed
    Inactive,       ///< Revealed but not active
    Active,         ///< Currently trackable
    Completed,      ///< Successfully completed
    Failed,         ///< Failed
    Abandoned       ///< Abandoned by player
};

/// @brief Objective type
enum class ObjectiveType : std::uint8_t {
    Primary,        ///< Main story objective
    Secondary,      ///< Side objective
    Hidden,         ///< Hidden/secret objective
    Optional,       ///< Optional objective
    Timed,          ///< Time-limited
    Repeatable      ///< Can be completed multiple times
};

// =============================================================================
// Vector3 (local definition)
// =============================================================================

struct Vec3 {
    float x{0}, y{0}, z{0};
};

struct Color {
    float r{1}, g{1}, b{1}, a{1};
};

// =============================================================================
// Variable Structures
// =============================================================================

/// @brief Game variable value
struct VariableValue {
    VariableType type{VariableType::Bool};
    std::any value;

    VariableValue() = default;

    explicit VariableValue(bool v) : type(VariableType::Bool), value(v) {}
    explicit VariableValue(int v) : type(VariableType::Int), value(v) {}
    explicit VariableValue(float v) : type(VariableType::Float), value(v) {}
    explicit VariableValue(const std::string& v) : type(VariableType::String), value(v) {}
    explicit VariableValue(const char* v) : type(VariableType::String), value(std::string(v)) {}
    explicit VariableValue(const Vec3& v) : type(VariableType::Vector3), value(v) {}
    explicit VariableValue(const Color& v) : type(VariableType::Color), value(v) {}
    explicit VariableValue(EntityId v) : type(VariableType::EntityRef), value(v) {}

    bool as_bool() const;
    int as_int() const;
    float as_float() const;
    std::string as_string() const;
    Vec3 as_vector3() const;
    Color as_color() const;
    EntityId as_entity() const;

    bool operator==(const VariableValue& other) const;
    bool operator!=(const VariableValue& other) const { return !(*this == other); }
};

/// @brief Game variable definition
struct GameVariable {
    VariableId id;
    std::string name;
    std::string description;
    VariableType type{VariableType::Bool};
    VariableScope scope{VariableScope::Global};
    PersistenceFlags persistence{PersistenceFlags::SaveToFile};

    VariableValue default_value;
    VariableValue current_value;

    // Constraints
    bool has_min{false};
    bool has_max{false};
    float min_value{0};
    float max_value{0};
    std::vector<std::string> allowed_values;    ///< For string enums

    // Metadata
    std::string category;
    std::vector<std::string> tags;
    double last_modified{0};
};

/// @brief Variable binding for UI/scripting
struct VariableBinding {
    VariableId variable;
    std::string path;                           ///< Property path
    std::function<void(const VariableValue&)> on_change;
    bool two_way{false};                        ///< Bidirectional binding
};

// =============================================================================
// Save/Load Structures
// =============================================================================

/// @brief Save file metadata
struct SaveMetadata {
    SaveSlotId slot_id;
    std::string name;
    SaveType type{SaveType::Manual};
    double timestamp{0};
    double play_time{0};
    std::string game_version;
    std::string level_name;
    std::string screenshot_path;
    std::uint32_t save_version{1};
    std::unordered_map<std::string, std::string> custom_data;
};

/// @brief Serialized save data
struct SaveData {
    SaveMetadata metadata;
    std::vector<std::uint8_t> variable_data;
    std::vector<std::uint8_t> entity_data;
    std::vector<std::uint8_t> world_data;
    std::vector<std::uint8_t> custom_data;
    std::uint32_t checksum{0};
};

/// @brief Save slot information
struct SaveSlot {
    SaveSlotId id;
    std::string file_path;
    SaveMetadata metadata;
    bool is_empty{true};
    bool is_corrupted{false};
    std::uint64_t file_size{0};
};

// =============================================================================
// Objective Structures
// =============================================================================

/// @brief Objective definition
struct ObjectiveDef {
    ObjectiveId id;
    std::string name;
    std::string description;
    std::string hint;
    ObjectiveType type{ObjectiveType::Primary};

    // Progress tracking
    bool trackable{true};
    std::uint32_t required_count{1};            ///< For counted objectives
    float time_limit{0};                        ///< 0 = no limit

    // Dependencies
    std::vector<ObjectiveId> prerequisites;
    std::vector<ObjectiveId> conflicts;         ///< Mutually exclusive

    // Rewards
    std::string reward_description;

    // UI
    std::string icon_path;
    std::string marker_path;
    Vec3 target_position;
    EntityId target_entity;
};

/// @brief Objective progress
struct ObjectiveProgress {
    ObjectiveId objective_id;
    ObjectiveState state{ObjectiveState::Hidden};
    std::uint32_t current_count{0};
    float time_elapsed{0};
    double started_time{0};
    double completed_time{0};
    std::vector<std::string> completed_steps;
};

// =============================================================================
// Phase Structures
// =============================================================================

/// @brief Game phase definition
struct GamePhase {
    GamePhaseId id;
    std::string name;
    PhaseType type{PhaseType::Gameplay};

    // Transitions
    TransitionType enter_transition{TransitionType::Immediate};
    TransitionType exit_transition{TransitionType::Immediate};
    float transition_duration{0.5f};

    // Settings
    bool pause_game{false};
    bool show_hud{true};
    bool allow_input{true};
    bool allow_pause{true};

    // Associated data
    std::string scene_name;
    std::string music_track;
    std::unordered_map<std::string, std::string> custom_data;
};

/// @brief Phase transition
struct PhaseTransition {
    GamePhaseId from_phase;
    GamePhaseId to_phase;
    TransitionType type{TransitionType::Immediate};
    float duration{0.5f};
    std::function<bool()> condition;            ///< Condition to check
    std::function<void()> on_start;
    std::function<void()> on_complete;
};

// =============================================================================
// Event Structures
// =============================================================================

/// @brief Variable change event
struct VariableChangeEvent {
    VariableId variable;
    std::string name;
    VariableValue old_value;
    VariableValue new_value;
    double timestamp{0};
    EntityId source_entity;
};

/// @brief Save event
struct SaveEvent {
    SaveSlotId slot;
    SaveType type{SaveType::Manual};
    SaveResult result{SaveResult::Success};
    std::string error_message;
    double timestamp{0};
};

/// @brief Load event
struct LoadEvent {
    SaveSlotId slot;
    LoadResult result{LoadResult::Success};
    std::string error_message;
    double timestamp{0};
};

/// @brief Phase change event
struct PhaseChangeEvent {
    GamePhaseId old_phase;
    GamePhaseId new_phase;
    TransitionType transition{TransitionType::Immediate};
    double timestamp{0};
};

/// @brief Objective event
struct ObjectiveEvent {
    ObjectiveId objective;
    ObjectiveState old_state;
    ObjectiveState new_state;
    double timestamp{0};
};

// =============================================================================
// Configuration
// =============================================================================

/// @brief Game state system configuration
struct GameStateConfig {
    // Save/Load
    std::string save_directory{"saves"};
    std::uint32_t max_save_slots{10};
    std::uint32_t max_auto_saves{3};
    float auto_save_interval{300.0f};           ///< 5 minutes
    bool compress_saves{true};
    bool encrypt_saves{false};

    // Variables
    std::uint32_t max_variables{10000};
    bool track_variable_history{true};
    float history_retention{3600.0f};           ///< 1 hour

    // Objectives
    std::uint32_t max_objectives{1000};
    bool auto_track_objectives{true};

    // Checkpoints
    bool enable_checkpoints{true};
    std::uint32_t max_checkpoints{50};
};

// =============================================================================
// Callback Types
// =============================================================================

using VariableChangeCallback = std::function<void(const VariableChangeEvent&)>;
using SaveCallback = std::function<void(const SaveEvent&)>;
using LoadCallback = std::function<void(const LoadEvent&)>;
using PhaseChangeCallback = std::function<void(const PhaseChangeEvent&)>;
using ObjectiveCallback = std::function<void(const ObjectiveEvent&)>;
using TransitionCallback = std::function<void(float progress)>;

} // namespace void_gamestate
