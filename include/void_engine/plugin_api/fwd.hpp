/// @file fwd.hpp
/// @brief Forward declarations for void_plugin_api module

#pragma once

#include <void_engine/core/id.hpp>
#include <cstdint>
#include <functional>

namespace void_plugin_api {

// =============================================================================
// Handle Types
// =============================================================================

// Use canonical EntityId from void_core
using EntityId = void_core::EntityId;

/// @brief Command identifier for tracking
struct CommandId {
    std::uint64_t value{0};
    bool operator==(const CommandId&) const = default;
    explicit operator bool() const { return value != 0; }
};

// =============================================================================
// Forward Declarations - Plugin API
// =============================================================================

class IPluginAPI;
class PluginAPIImpl;
class GameplayPlugin;

// =============================================================================
// Forward Declarations - State Stores
// =============================================================================

struct AIStateStore;
struct CombatStateStore;
struct InventoryStateStore;

// =============================================================================
// Forward Declarations - Commands
// =============================================================================

class IStateCommand;

// AI Commands
class SetBlackboardCommand;
class RequestPathCommand;
class SetPerceptionTargetCommand;

// Combat Commands
class ApplyDamageCommand;
class ApplyStatusEffectCommand;
class HealEntityCommand;
class SpawnProjectileCommand;

// Inventory Commands
class AddItemCommand;
class RemoveItemCommand;
class TransferItemCommand;
class EquipItemCommand;
class StartCraftingCommand;

// =============================================================================
// Forward Declarations - Core
// =============================================================================

class GameStateCore;
class CommandProcessor;

} // namespace void_plugin_api

// =============================================================================
// Hash Specializations
// =============================================================================

namespace std {


template<>
struct hash<void_plugin_api::CommandId> {
    std::size_t operator()(const void_plugin_api::CommandId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace std
