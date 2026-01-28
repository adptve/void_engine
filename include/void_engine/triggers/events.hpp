/// @file events.hpp
/// @brief Event types emitted by the trigger system via EventBus
///
/// Architecture (from doc/review):
/// Triggers emit data events. Plugins subscribe via EventBus.
/// Callbacks are unsafe across hot-reload - event bus subscriptions
/// are re-established on plugin load, making them reload-safe.
///
/// All trigger events are immutable data structs. They carry full
/// context so subscribers don't need back-references to the trigger system.

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <string>
#include <vector>

namespace void_triggers {

// =============================================================================
// Trigger Lifecycle Events
// =============================================================================

/// @brief Emitted when an entity enters a trigger volume
///
/// Subscribers can use this to:
/// - Start combat encounters
/// - Begin dialogue
/// - Enable area-specific UI
/// - Play ambient audio
struct TriggerEnterEvent {
    TriggerId trigger_id;           ///< Which trigger was entered
    EntityId entity_id;             ///< Which entity entered
    std::string trigger_name;       ///< Trigger name for string-based lookup
    TriggerType trigger_type;       ///< Type of trigger
    TriggerFlags trigger_flags;     ///< Trigger flags
    Vec3 entity_position;           ///< Entity position at time of entry
    Vec3 trigger_position;          ///< Trigger center position
    double timestamp;               ///< Time of event
    std::uint32_t activation_count; ///< How many times this trigger has activated
    std::size_t entities_inside;    ///< Total entities now inside (including this one)
};

/// @brief Emitted when an entity exits a trigger volume
///
/// Subscribers can use this to:
/// - End combat encounters
/// - Close dialogue
/// - Disable area-specific UI
/// - Fade out ambient audio
struct TriggerExitEvent {
    TriggerId trigger_id;
    EntityId entity_id;
    std::string trigger_name;
    TriggerType trigger_type;
    TriggerFlags trigger_flags;
    Vec3 entity_position;           ///< Entity position at time of exit
    Vec3 trigger_position;
    double timestamp;
    std::uint32_t activation_count;
    std::size_t entities_remaining; ///< Entities still inside after this exit
};

/// @brief Emitted each frame while an entity stays inside a trigger volume
///
/// Only emitted for Stay-type triggers. Subscribers can use this to:
/// - Apply damage over time
/// - Accumulate resource gathering
/// - Update proximity-based effects
struct TriggerStayEvent {
    TriggerId trigger_id;
    EntityId entity_id;
    std::string trigger_name;
    Vec3 entity_position;
    Vec3 trigger_position;
    double timestamp;
    float delta_time;               ///< Frame dt for time-based calculations
    float time_inside;              ///< Total time entity has been inside
};

// =============================================================================
// Trigger Activation Events
// =============================================================================

/// @brief Emitted when a trigger is activated (conditions met, action executing)
///
/// This is the primary event for trigger responses. Subscribers can use this to:
/// - Execute gameplay logic
/// - Spawn enemies or items
/// - Change world state
/// - Start cutscenes
struct TriggerActivatedEvent {
    TriggerId trigger_id;
    EntityId entity_id;             ///< Entity that caused activation (may be invalid for timed triggers)
    std::string trigger_name;
    TriggerType trigger_type;
    TriggerFlags trigger_flags;
    TriggerEventType event_type;    ///< What caused activation (enter, exit, timer, etc.)
    Vec3 entity_position;
    Vec3 trigger_position;
    double timestamp;
    std::uint32_t activation_count; ///< Total activations (after this one)
    std::uint32_t max_activations;  ///< 0 = unlimited
    bool is_final_activation;       ///< True if this was the last allowed activation
};

/// @brief Emitted when a trigger starts its cooldown period
struct TriggerCooldownStartedEvent {
    TriggerId trigger_id;
    std::string trigger_name;
    float cooldown_duration;        ///< Total cooldown time
    double timestamp;
};

/// @brief Emitted when a trigger finishes its cooldown
struct TriggerCooldownEndedEvent {
    TriggerId trigger_id;
    std::string trigger_name;
    double timestamp;
};

// =============================================================================
// Trigger State Events
// =============================================================================

/// @brief Emitted when a trigger is enabled
struct TriggerEnabledEvent {
    TriggerId trigger_id;
    std::string trigger_name;
    double timestamp;
};

/// @brief Emitted when a trigger is disabled
struct TriggerDisabledEvent {
    TriggerId trigger_id;
    std::string trigger_name;
    double timestamp;
};

/// @brief Emitted when a trigger is created in the system
struct TriggerCreatedEvent {
    TriggerId trigger_id;
    std::string trigger_name;
    TriggerType trigger_type;
    TriggerFlags flags;
    double timestamp;
};

/// @brief Emitted when a trigger is destroyed/removed from the system
struct TriggerDestroyedEvent {
    TriggerId trigger_id;
    std::string trigger_name;
    std::uint32_t total_activations; ///< Lifetime activation count
    double timestamp;
};

/// @brief Emitted when a trigger is reset
struct TriggerResetEvent {
    TriggerId trigger_id;
    std::string trigger_name;
    double timestamp;
};

// =============================================================================
// Zone Events
// =============================================================================

/// @brief Emitted when a zone is created
struct ZoneCreatedEvent {
    ZoneId zone_id;
    std::string zone_name;
    Vec3 position;
    VolumeType volume_type;
    double timestamp;
};

/// @brief Emitted when a zone is destroyed
struct ZoneDestroyedEvent {
    ZoneId zone_id;
    std::string zone_name;
    double timestamp;
};

// =============================================================================
// Action Request Events
// =============================================================================

/// @brief Emitted when a trigger wants to spawn an entity
/// Plugins subscribe to this and perform the actual spawn via ECS
struct SpawnRequestEvent {
    TriggerId source_trigger;
    std::string prefab_name;
    Vec3 position;
    Quat rotation;
    std::uint32_t count;
    double timestamp;
};

/// @brief Emitted when a trigger wants to destroy an entity
/// Plugins subscribe to this and perform the actual destroy via ECS
struct DestroyRequestEvent {
    TriggerId source_trigger;
    EntityId target_entity;
    bool destroy_triggering_entity;  ///< Destroy the entity that triggered, if target is invalid
    double timestamp;
};

/// @brief Emitted when a trigger wants to teleport an entity
/// Plugins subscribe to this and perform the actual move via ECS
struct TeleportRequestEvent {
    TriggerId source_trigger;
    EntityId target_entity;
    Vec3 destination;
    Quat rotation;
    bool set_rotation;
    bool relative;                  ///< Offset from current position vs absolute
    double timestamp;
};

/// @brief Emitted when a trigger wants to play audio
/// Audio system subscribes and handles playback
struct PlayAudioRequestEvent {
    TriggerId source_trigger;
    std::string audio_path;
    Vec3 position;
    float volume;
    float pitch;
    bool spatial;                   ///< 3D positioned audio vs global
    double timestamp;
};

/// @brief Emitted when a trigger wants to play a visual effect
/// Effect system subscribes and handles rendering
struct PlayEffectRequestEvent {
    TriggerId source_trigger;
    std::string effect_path;
    Vec3 position;
    Quat rotation;
    float scale;
    EntityId attach_to_entity;      ///< If valid, attach effect to entity
    double timestamp;
};

/// @brief Emitted when a trigger wants to set a game variable
/// GameState system subscribes and handles the update
struct SetVariableRequestEvent {
    TriggerId source_trigger;
    std::string variable_name;
    VariableValue value;
    std::uint8_t operation;         ///< SetVariableAction::Operation as uint8_t
    double timestamp;
};

/// @brief Emitted when a trigger wants to enable/disable another trigger
struct EnableTriggerRequestEvent {
    TriggerId source_trigger;
    TriggerId target_trigger;
    bool enable;
    bool toggle;                    ///< If true, flip current state
    double timestamp;
};

/// @brief Emitted when a trigger wants to send a custom named event
/// Generic mechanism for trigger-to-plugin communication
struct TriggerCustomEvent {
    TriggerId source_trigger;
    EntityId entity_id;
    std::string event_name;
    Vec3 position;
    bool broadcast;                 ///< Send to all subscribers vs targeted
    double timestamp;
};

} // namespace void_triggers
