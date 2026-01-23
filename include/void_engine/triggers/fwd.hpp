/// @file fwd.hpp
/// @brief Forward declarations for void_triggers module

#pragma once

#include <cstdint>
#include <functional>

namespace void_triggers {

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Unique identifier for triggers
struct TriggerId {
    std::uint64_t value{0};
    bool operator==(const TriggerId&) const = default;
    bool operator!=(const TriggerId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for trigger zones
struct ZoneId {
    std::uint64_t value{0};
    bool operator==(const ZoneId&) const = default;
    bool operator!=(const ZoneId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for conditions
struct ConditionId {
    std::uint64_t value{0};
    bool operator==(const ConditionId&) const = default;
    bool operator!=(const ConditionId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for actions
struct ActionId {
    std::uint64_t value{0};
    bool operator==(const ActionId&) const = default;
    bool operator!=(const ActionId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for trigger events
struct TriggerEventId {
    std::uint64_t value{0};
    bool operator==(const TriggerEventId&) const = default;
    bool operator!=(const TriggerEventId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Entity identifier (from ECS)
struct EntityId {
    std::uint64_t value{0};
    bool operator==(const EntityId&) const = default;
    bool operator!=(const EntityId&) const = default;
    explicit operator bool() const { return value != 0; }
};

// =============================================================================
// Forward Declarations - Volumes
// =============================================================================

struct AABB;
struct Sphere;
struct Capsule;
struct OrientedBox;
class ITriggerVolume;
class BoxVolume;
class SphereVolume;
class CapsuleVolume;
class MeshVolume;
class CompositeVolume;

// =============================================================================
// Forward Declarations - Conditions
// =============================================================================

class ICondition;
class ConditionGroup;
class VariableCondition;
class EntityCondition;
class TimerCondition;
class CountCondition;
class RandomCondition;
class DistanceCondition;
class TagCondition;

// =============================================================================
// Forward Declarations - Actions
// =============================================================================

class IAction;
class ActionSequence;
class CallbackAction;
class SpawnAction;
class DestroyAction;
class TeleportAction;
class SetVariableAction;
class SendEventAction;
class PlayAudioAction;
class PlayEffectAction;

// =============================================================================
// Forward Declarations - Triggers
// =============================================================================

struct TriggerConfig;
enum class TriggerState : std::uint8_t;
class Trigger;
class TriggerZone;
class ProximityTrigger;
class InteractionTrigger;
class TimedTrigger;
class SequenceTrigger;

// =============================================================================
// Forward Declarations - System
// =============================================================================

struct TriggerSystemConfig;
class TriggerRegistry;
class ConditionRegistry;
class ActionRegistry;
class TriggerSystem;

} // namespace void_triggers

// =============================================================================
// Hash Specializations
// =============================================================================

namespace std {

template<>
struct hash<void_triggers::TriggerId> {
    std::size_t operator()(const void_triggers::TriggerId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_triggers::ZoneId> {
    std::size_t operator()(const void_triggers::ZoneId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_triggers::ConditionId> {
    std::size_t operator()(const void_triggers::ConditionId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_triggers::ActionId> {
    std::size_t operator()(const void_triggers::ActionId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_triggers::TriggerEventId> {
    std::size_t operator()(const void_triggers::TriggerEventId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_triggers::EntityId> {
    std::size_t operator()(const void_triggers::EntityId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace std
