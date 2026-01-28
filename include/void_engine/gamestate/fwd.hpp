/// @file fwd.hpp
/// @brief Forward declarations for void_gamestate module

#pragma once

#include <void_engine/core/id.hpp>
#include <cstdint>
#include <functional>

namespace void_gamestate {

// Use canonical EntityId from void_core
using EntityId = void_core::EntityId;

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Unique identifier for game variables
struct VariableId {
    std::uint64_t value{0};
    bool operator==(const VariableId&) const = default;
    bool operator!=(const VariableId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for save slots
struct SaveSlotId {
    std::uint64_t value{0};
    bool operator==(const SaveSlotId&) const = default;
    bool operator!=(const SaveSlotId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for checkpoints
struct CheckpointId {
    std::uint64_t value{0};
    bool operator==(const CheckpointId&) const = default;
    bool operator!=(const CheckpointId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for game phases/states
struct GamePhaseId {
    std::uint64_t value{0};
    bool operator==(const GamePhaseId&) const = default;
    bool operator!=(const GamePhaseId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for objectives
struct ObjectiveId {
    std::uint64_t value{0};
    bool operator==(const ObjectiveId&) const = default;
    bool operator!=(const ObjectiveId&) const = default;
    explicit operator bool() const { return value != 0; }
};


// =============================================================================
// Forward Declarations - Variables
// =============================================================================

struct GameVariable;
struct VariableBinding;
class VariableStore;
class GlobalVariables;
class EntityVariables;

// =============================================================================
// Forward Declarations - State Machine
// =============================================================================

struct GamePhase;
struct PhaseTransition;
class IPhaseState;
class GameStateMachine;
class PhaseCondition;

// =============================================================================
// Forward Declarations - Save/Load
// =============================================================================

struct SaveMetadata;
struct SaveData;
struct SaveSlot;
class ISaveable;
class SaveSerializer;
class SaveManager;
class AutoSaveManager;
class CheckpointManager;

// =============================================================================
// Forward Declarations - Objectives
// =============================================================================

struct ObjectiveDef;
struct ObjectiveProgress;
class ObjectiveTracker;
class QuestSystem;

// =============================================================================
// Forward Declarations - System
// =============================================================================

struct GameStateConfig;
class GameStateSystem;

} // namespace void_gamestate

// =============================================================================
// Hash Specializations
// =============================================================================

namespace std {

template<>
struct hash<void_gamestate::VariableId> {
    std::size_t operator()(const void_gamestate::VariableId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_gamestate::SaveSlotId> {
    std::size_t operator()(const void_gamestate::SaveSlotId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_gamestate::CheckpointId> {
    std::size_t operator()(const void_gamestate::CheckpointId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_gamestate::GamePhaseId> {
    std::size_t operator()(const void_gamestate::GamePhaseId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_gamestate::ObjectiveId> {
    std::size_t operator()(const void_gamestate::ObjectiveId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace std
