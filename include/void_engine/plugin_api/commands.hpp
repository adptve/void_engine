/// @file commands.hpp
/// @brief Command pattern for state modification
///
/// Plugins submit commands to modify state. Commands are validated and executed
/// by the CommandProcessor, ensuring atomic state changes and supporting
/// undo/replay for networking.

#pragma once

#include "fwd.hpp"
#include "state_stores.hpp"

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace void_plugin_api {

// =============================================================================
// Command Base
// =============================================================================

/// @brief Result of command execution
enum class CommandResult : std::uint8_t {
    Success,
    Failed,
    InvalidEntity,
    InvalidTarget,
    InvalidState,
    InsufficientResources,
    PermissionDenied,
    Queued  // Command will execute later
};

/// @brief Command execution context
struct CommandContext {
    double timestamp{0};
    EntityId source;  // Entity that initiated command
    std::uint32_t frame{0};
    bool from_network{false};
    bool can_undo{true};
};

/// @brief Base interface for state commands
class IStateCommand {
public:
    virtual ~IStateCommand() = default;

    /// @brief Execute the command against state stores
    [[nodiscard]] virtual CommandResult execute(
        AIStateStore& ai_state,
        CombatStateStore& combat_state,
        InventoryStateStore& inventory_state,
        const CommandContext& ctx) = 0;

    /// @brief Validate before execution
    [[nodiscard]] virtual bool validate(
        const AIStateStore& ai_state,
        const CombatStateStore& combat_state,
        const InventoryStateStore& inventory_state) const = 0;

    /// @brief Get command type name for debugging
    [[nodiscard]] virtual const char* type_name() const = 0;

    /// @brief Get target entity (if applicable)
    [[nodiscard]] virtual EntityId target_entity() const { return EntityId{}; }

    /// @brief Serialize for networking/replay
    [[nodiscard]] virtual std::vector<std::uint8_t> serialize() const { return {}; }
};

using CommandPtr = std::unique_ptr<IStateCommand>;

// =============================================================================
// AI COMMANDS
// =============================================================================

/// @brief Set a value in an entity's blackboard
class SetBlackboardCommand : public IStateCommand {
public:
    enum class ValueType { Bool, Int, Float, String, Vec3, Entity };

    SetBlackboardCommand(EntityId entity, std::string key, bool value)
        : m_entity(entity), m_key(std::move(key)), m_type(ValueType::Bool), m_bool_value(value) {}

    SetBlackboardCommand(EntityId entity, std::string key, int value)
        : m_entity(entity), m_key(std::move(key)), m_type(ValueType::Int), m_int_value(value) {}

    SetBlackboardCommand(EntityId entity, std::string key, float value)
        : m_entity(entity), m_key(std::move(key)), m_type(ValueType::Float), m_float_value(value) {}

    SetBlackboardCommand(EntityId entity, std::string key, std::string value)
        : m_entity(entity), m_key(std::move(key)), m_type(ValueType::String), m_string_value(std::move(value)) {}

    SetBlackboardCommand(EntityId entity, std::string key, Vec3 value)
        : m_entity(entity), m_key(std::move(key)), m_type(ValueType::Vec3), m_vec3_value(value) {}

    SetBlackboardCommand(EntityId entity, std::string key, EntityId value)
        : m_entity(entity), m_key(std::move(key)), m_type(ValueType::Entity), m_entity_value(value) {}

    CommandResult execute(AIStateStore& ai_state, CombatStateStore&, InventoryStateStore&,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&, const InventoryStateStore&) const override;
    const char* type_name() const override { return "SetBlackboard"; }
    EntityId target_entity() const override { return m_entity; }

private:
    EntityId m_entity;
    std::string m_key;
    ValueType m_type;
    bool m_bool_value{false};
    int m_int_value{0};
    float m_float_value{0};
    std::string m_string_value;
    Vec3 m_vec3_value;
    EntityId m_entity_value;
};

/// @brief Request pathfinding for an entity
class RequestPathCommand : public IStateCommand {
public:
    RequestPathCommand(EntityId entity, Vec3 destination)
        : m_entity(entity), m_destination(destination) {}

    CommandResult execute(AIStateStore& ai_state, CombatStateStore&, InventoryStateStore&,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&, const InventoryStateStore&) const override;
    const char* type_name() const override { return "RequestPath"; }
    EntityId target_entity() const override { return m_entity; }

private:
    EntityId m_entity;
    Vec3 m_destination;
};

/// @brief Set the primary perception target for an entity
class SetPerceptionTargetCommand : public IStateCommand {
public:
    SetPerceptionTargetCommand(EntityId entity, EntityId target)
        : m_entity(entity), m_target(target) {}

    CommandResult execute(AIStateStore& ai_state, CombatStateStore&, InventoryStateStore&,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&, const InventoryStateStore&) const override;
    const char* type_name() const override { return "SetPerceptionTarget"; }
    EntityId target_entity() const override { return m_entity; }

private:
    EntityId m_entity;
    EntityId m_target;
};

// =============================================================================
// COMBAT COMMANDS
// =============================================================================

/// @brief Damage info for ApplyDamage command
struct DamageInfo {
    float base_damage{0};
    DamageType type{DamageType::Physical};
    EntityId source;
    bool can_crit{true};
    bool ignore_armor{false};
    float armor_penetration{0};
};

/// @brief Apply damage to an entity
class ApplyDamageCommand : public IStateCommand {
public:
    ApplyDamageCommand(EntityId target, const DamageInfo& damage)
        : m_target(target), m_damage(damage) {}

    CommandResult execute(AIStateStore&, CombatStateStore& combat_state, InventoryStateStore&,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore& combat_state,
                  const InventoryStateStore&) const override;
    const char* type_name() const override { return "ApplyDamage"; }
    EntityId target_entity() const override { return m_target; }

    /// @brief Get final damage after execution (for events)
    float final_damage() const { return m_final_damage; }
    bool was_crit() const { return m_was_crit; }
    bool killed_target() const { return m_killed; }

private:
    EntityId m_target;
    DamageInfo m_damage;
    float m_final_damage{0};
    bool m_was_crit{false};
    bool m_killed{false};
};

/// @brief Apply a status effect to an entity
class ApplyStatusEffectCommand : public IStateCommand {
public:
    ApplyStatusEffectCommand(EntityId target, std::string effect_name, float duration, EntityId source = {})
        : m_target(target), m_effect_name(std::move(effect_name)), m_duration(duration), m_source(source) {}

    void set_stacks(std::uint32_t stacks) { m_stacks = stacks; }
    void add_modifier(const std::string& stat, float value) { m_modifiers[stat] = value; }

    CommandResult execute(AIStateStore&, CombatStateStore& combat_state, InventoryStateStore&,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&, const InventoryStateStore&) const override;
    const char* type_name() const override { return "ApplyStatusEffect"; }
    EntityId target_entity() const override { return m_target; }

private:
    EntityId m_target;
    std::string m_effect_name;
    float m_duration;
    EntityId m_source;
    std::uint32_t m_stacks{1};
    std::unordered_map<std::string, float> m_modifiers;
};

/// @brief Heal an entity
class HealEntityCommand : public IStateCommand {
public:
    HealEntityCommand(EntityId target, float amount, EntityId source = {})
        : m_target(target), m_amount(amount), m_source(source) {}

    void set_heal_shield(bool heal_shield) { m_heal_shield = heal_shield; }
    void set_over_heal(bool over_heal) { m_over_heal = over_heal; }

    CommandResult execute(AIStateStore&, CombatStateStore& combat_state, InventoryStateStore&,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore& combat_state,
                  const InventoryStateStore&) const override;
    const char* type_name() const override { return "HealEntity"; }
    EntityId target_entity() const override { return m_target; }

private:
    EntityId m_target;
    float m_amount;
    EntityId m_source;
    bool m_heal_shield{true};
    bool m_over_heal{false};
};

/// @brief Spawn a projectile
class SpawnProjectileCommand : public IStateCommand {
public:
    SpawnProjectileCommand(EntityId source, Vec3 position, Vec3 direction, float damage)
        : m_source(source), m_position(position), m_direction(direction), m_damage(damage) {}

    void set_speed(float speed) { m_speed = speed; }
    void set_damage_type(DamageType type) { m_damage_type = type; }
    void set_homing(EntityId target) { m_target = target; m_homing = true; }
    void set_lifetime(float lifetime) { m_lifetime = lifetime; }
    void set_penetrating(std::uint32_t hits) { m_penetrating = true; m_hits = hits; }

    CommandResult execute(AIStateStore&, CombatStateStore& combat_state, InventoryStateStore&,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&, const InventoryStateStore&) const override;
    const char* type_name() const override { return "SpawnProjectile"; }
    EntityId target_entity() const override { return m_source; }

    /// @brief Get spawned projectile ID after execution
    ProjectileId spawned_id() const { return m_spawned_id; }

private:
    EntityId m_source;
    Vec3 m_position;
    Vec3 m_direction;
    float m_damage;
    float m_speed{20.0f};
    DamageType m_damage_type{DamageType::Physical};
    EntityId m_target;
    bool m_homing{false};
    float m_lifetime{10.0f};
    bool m_penetrating{false};
    std::uint32_t m_hits{1};
    ProjectileId m_spawned_id;
};

// =============================================================================
// INVENTORY COMMANDS
// =============================================================================

/// @brief Add an item to an entity's inventory
class AddItemCommand : public IStateCommand {
public:
    AddItemCommand(EntityId entity, ItemDefId item_def, std::uint32_t quantity = 1)
        : m_entity(entity), m_item_def(item_def), m_quantity(quantity) {}

    void set_target_slot(std::uint32_t slot) { m_target_slot = slot; m_has_target_slot = true; }
    void set_quality(float quality) { m_quality = quality; }
    void add_modifier(const std::string& modifier) { m_modifiers.push_back(modifier); }

    CommandResult execute(AIStateStore&, CombatStateStore&, InventoryStateStore& inventory_state,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&,
                  const InventoryStateStore& inventory_state) const override;
    const char* type_name() const override { return "AddItem"; }
    EntityId target_entity() const override { return m_entity; }

    /// @brief Get created item instance after execution
    ItemInstanceId created_instance() const { return m_created_instance; }
    std::uint32_t overflow_quantity() const { return m_overflow; }

private:
    EntityId m_entity;
    ItemDefId m_item_def;
    std::uint32_t m_quantity;
    std::uint32_t m_target_slot{0};
    bool m_has_target_slot{false};
    float m_quality{1.0f};
    std::vector<std::string> m_modifiers;
    ItemInstanceId m_created_instance;
    std::uint32_t m_overflow{0};
};

/// @brief Remove an item from an entity's inventory
class RemoveItemCommand : public IStateCommand {
public:
    RemoveItemCommand(EntityId entity, ItemInstanceId item, std::uint32_t quantity = 0)
        : m_entity(entity), m_item(item), m_quantity(quantity) {}  // 0 = remove all

    // Alternative: remove by definition
    RemoveItemCommand(EntityId entity, ItemDefId item_def, std::uint32_t quantity)
        : m_entity(entity), m_item_def(item_def), m_quantity(quantity), m_by_def(true) {}

    void set_destroy(bool destroy) { m_destroy = destroy; }

    CommandResult execute(AIStateStore&, CombatStateStore&, InventoryStateStore& inventory_state,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&,
                  const InventoryStateStore& inventory_state) const override;
    const char* type_name() const override { return "RemoveItem"; }
    EntityId target_entity() const override { return m_entity; }

    std::uint32_t removed_quantity() const { return m_removed; }

private:
    EntityId m_entity;
    ItemInstanceId m_item;
    ItemDefId m_item_def;
    std::uint32_t m_quantity;
    bool m_by_def{false};
    bool m_destroy{true};
    std::uint32_t m_removed{0};
};

/// @brief Transfer an item between entities
class TransferItemCommand : public IStateCommand {
public:
    TransferItemCommand(EntityId from, EntityId to, ItemInstanceId item, std::uint32_t quantity = 0)
        : m_from(from), m_to(to), m_item(item), m_quantity(quantity) {}

    CommandResult execute(AIStateStore&, CombatStateStore&, InventoryStateStore& inventory_state,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&,
                  const InventoryStateStore& inventory_state) const override;
    const char* type_name() const override { return "TransferItem"; }
    EntityId target_entity() const override { return m_from; }

private:
    EntityId m_from;
    EntityId m_to;
    ItemInstanceId m_item;
    std::uint32_t m_quantity;
};

/// @brief Equip an item
class EquipItemCommand : public IStateCommand {
public:
    EquipItemCommand(EntityId entity, ItemInstanceId item, std::string slot = "")
        : m_entity(entity), m_item(item), m_slot(std::move(slot)) {}  // Empty slot = auto-detect

    CommandResult execute(AIStateStore&, CombatStateStore&, InventoryStateStore& inventory_state,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&,
                  const InventoryStateStore& inventory_state) const override;
    const char* type_name() const override { return "EquipItem"; }
    EntityId target_entity() const override { return m_entity; }

    /// @brief Get unequipped item (if slot was occupied)
    ItemInstanceId unequipped_item() const { return m_unequipped; }

private:
    EntityId m_entity;
    ItemInstanceId m_item;
    std::string m_slot;
    ItemInstanceId m_unequipped;
};

/// @brief Start crafting a recipe
class StartCraftingCommand : public IStateCommand {
public:
    StartCraftingCommand(EntityId entity, std::uint64_t recipe_id)
        : m_entity(entity), m_recipe_id(recipe_id) {}

    CommandResult execute(AIStateStore&, CombatStateStore&, InventoryStateStore& inventory_state,
                          const CommandContext& ctx) override;
    bool validate(const AIStateStore&, const CombatStateStore&,
                  const InventoryStateStore& inventory_state) const override;
    const char* type_name() const override { return "StartCrafting"; }
    EntityId target_entity() const override { return m_entity; }

private:
    EntityId m_entity;
    std::uint64_t m_recipe_id;
};

// =============================================================================
// COMMAND PROCESSOR
// =============================================================================

/// @brief Processes and executes commands against state stores
class CommandProcessor {
public:
    using CommandCallback = std::function<void(const IStateCommand&, CommandResult)>;

    CommandProcessor(AIStateStore& ai, CombatStateStore& combat, InventoryStateStore& inventory)
        : m_ai_state(ai), m_combat_state(combat), m_inventory_state(inventory) {}

    /// @brief Submit a command for immediate execution
    CommandResult execute(CommandPtr command, const CommandContext& ctx);

    /// @brief Queue a command for deferred execution
    void queue(CommandPtr command, const CommandContext& ctx);

    /// @brief Process all queued commands
    std::vector<CommandResult> process_queue();

    /// @brief Register callback for command execution
    void on_command(CommandCallback callback) { m_callbacks.push_back(std::move(callback)); }

    /// @brief Get statistics
    std::uint64_t commands_executed() const { return m_commands_executed; }
    std::uint64_t commands_failed() const { return m_commands_failed; }

private:
    AIStateStore& m_ai_state;
    CombatStateStore& m_combat_state;
    InventoryStateStore& m_inventory_state;

    std::vector<std::pair<CommandPtr, CommandContext>> m_queue;
    std::vector<CommandCallback> m_callbacks;

    std::uint64_t m_commands_executed{0};
    std::uint64_t m_commands_failed{0};
};

} // namespace void_plugin_api
