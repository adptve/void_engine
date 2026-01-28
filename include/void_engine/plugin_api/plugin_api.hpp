/// @file plugin_api.hpp
/// @brief Interface for gameplay plugins to access engine state
///
/// IPluginAPI is the contract between gameplay plugins and the engine.
/// Plugins use this interface to:
/// - Read state from stores (AI, Combat, Inventory)
/// - Submit commands to modify state
/// - Access engine services (events, time, etc.)

#pragma once

#include "fwd.hpp"
#include "state_stores.hpp"
#include "commands.hpp"

#include <void_engine/core/plugin.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace void_plugin_api {

// =============================================================================
// Plugin API Interface
// =============================================================================

/// @brief Interface for plugins to access engine state and services
class IPluginAPI {
public:
    virtual ~IPluginAPI() = default;

    // -------------------------------------------------------------------------
    // State Store Access (READ ONLY)
    // -------------------------------------------------------------------------

    /// @brief Get AI state store (read-only)
    [[nodiscard]] virtual const AIStateStore& ai_state() const = 0;

    /// @brief Get combat state store (read-only)
    [[nodiscard]] virtual const CombatStateStore& combat_state() const = 0;

    /// @brief Get inventory state store (read-only)
    [[nodiscard]] virtual const InventoryStateStore& inventory_state() const = 0;

    // -------------------------------------------------------------------------
    // Command Submission
    // -------------------------------------------------------------------------

    /// @brief Submit a command for execution
    virtual CommandResult submit_command(CommandPtr command) = 0;

    /// @brief Queue a command for deferred execution
    virtual void queue_command(CommandPtr command) = 0;

    // -------------------------------------------------------------------------
    // Convenience Methods - AI
    // -------------------------------------------------------------------------

    /// @brief Set a blackboard value
    virtual void set_blackboard_bool(EntityId entity, const std::string& key, bool value) {
        submit_command(std::make_unique<SetBlackboardCommand>(entity, key, value));
    }

    virtual void set_blackboard_int(EntityId entity, const std::string& key, int value) {
        submit_command(std::make_unique<SetBlackboardCommand>(entity, key, value));
    }

    virtual void set_blackboard_float(EntityId entity, const std::string& key, float value) {
        submit_command(std::make_unique<SetBlackboardCommand>(entity, key, value));
    }

    virtual void set_blackboard_string(EntityId entity, const std::string& key, const std::string& value) {
        submit_command(std::make_unique<SetBlackboardCommand>(entity, key, value));
    }

    virtual void set_blackboard_vec3(EntityId entity, const std::string& key, Vec3 value) {
        submit_command(std::make_unique<SetBlackboardCommand>(entity, key, value));
    }

    virtual void set_blackboard_entity(EntityId entity, const std::string& key, EntityId value) {
        submit_command(std::make_unique<SetBlackboardCommand>(entity, key, value));
    }

    /// @brief Request pathfinding
    virtual void request_path(EntityId entity, Vec3 destination) {
        submit_command(std::make_unique<RequestPathCommand>(entity, destination));
    }

    /// @brief Set perception target
    virtual void set_perception_target(EntityId entity, EntityId target) {
        submit_command(std::make_unique<SetPerceptionTargetCommand>(entity, target));
    }

    // -------------------------------------------------------------------------
    // Convenience Methods - Combat
    // -------------------------------------------------------------------------

    /// @brief Apply damage to an entity
    virtual CommandResult apply_damage(EntityId target, float amount, EntityId source = {},
                                       DamageType type = DamageType::Physical) {
        DamageInfo info;
        info.base_damage = amount;
        info.type = type;
        info.source = source;
        return submit_command(std::make_unique<ApplyDamageCommand>(target, info));
    }

    /// @brief Apply status effect
    virtual CommandResult apply_status_effect(EntityId target, const std::string& effect,
                                              float duration, EntityId source = {}) {
        return submit_command(std::make_unique<ApplyStatusEffectCommand>(target, effect, duration, source));
    }

    /// @brief Heal an entity
    virtual CommandResult heal_entity(EntityId target, float amount, EntityId source = {}) {
        return submit_command(std::make_unique<HealEntityCommand>(target, amount, source));
    }

    /// @brief Spawn a projectile
    virtual ProjectileId spawn_projectile(EntityId source, Vec3 position, Vec3 direction, float damage) {
        auto cmd = std::make_unique<SpawnProjectileCommand>(source, position, direction, damage);
        auto* ptr = cmd.get();
        submit_command(std::move(cmd));
        return ptr->spawned_id();
    }

    // -------------------------------------------------------------------------
    // Convenience Methods - Inventory
    // -------------------------------------------------------------------------

    /// @brief Add item to inventory
    virtual ItemInstanceId add_item(EntityId entity, ItemDefId item_def, std::uint32_t quantity = 1) {
        auto cmd = std::make_unique<AddItemCommand>(entity, item_def, quantity);
        auto* ptr = cmd.get();
        submit_command(std::move(cmd));
        return ptr->created_instance();
    }

    /// @brief Remove item from inventory
    virtual bool remove_item(EntityId entity, ItemInstanceId item, std::uint32_t quantity = 0) {
        return submit_command(std::make_unique<RemoveItemCommand>(entity, item, quantity)) == CommandResult::Success;
    }

    /// @brief Transfer item between entities
    virtual bool transfer_item(EntityId from, EntityId to, ItemInstanceId item, std::uint32_t quantity = 0) {
        return submit_command(std::make_unique<TransferItemCommand>(from, to, item, quantity)) == CommandResult::Success;
    }

    /// @brief Equip item
    virtual bool equip_item(EntityId entity, ItemInstanceId item, const std::string& slot = "") {
        return submit_command(std::make_unique<EquipItemCommand>(entity, item, slot)) == CommandResult::Success;
    }

    /// @brief Start crafting
    virtual bool start_crafting(EntityId entity, std::uint64_t recipe_id) {
        return submit_command(std::make_unique<StartCraftingCommand>(entity, recipe_id)) == CommandResult::Success;
    }

    // -------------------------------------------------------------------------
    // Engine Services
    // -------------------------------------------------------------------------

    /// @brief Get current game time
    [[nodiscard]] virtual double current_time() const = 0;

    /// @brief Get delta time for this frame
    [[nodiscard]] virtual float delta_time() const = 0;

    /// @brief Get current frame number
    [[nodiscard]] virtual std::uint32_t frame_number() const = 0;

    /// @brief Check if game is paused
    [[nodiscard]] virtual bool is_paused() const = 0;

    // -------------------------------------------------------------------------
    // Entity Queries
    // -------------------------------------------------------------------------

    /// @brief Check if entity exists
    [[nodiscard]] virtual bool entity_exists(EntityId entity) const = 0;

    /// @brief Get entity position
    [[nodiscard]] virtual Vec3 get_entity_position(EntityId entity) const = 0;

    /// @brief Get entities in radius
    [[nodiscard]] virtual std::vector<EntityId> get_entities_in_radius(Vec3 center, float radius) const = 0;

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    /// @brief Emit an event
    virtual void emit_event(const std::string& event_name, const std::any& data = {}) = 0;

    /// @brief Subscribe to an event
    using EventCallback = std::function<void(const std::any&)>;
    virtual void subscribe_event(const std::string& event_name, EventCallback callback) = 0;
};

// =============================================================================
// Gameplay Plugin Base Class
// =============================================================================

/// @brief Base class for gameplay plugins
///
/// GameplayPlugin extends void_core::Plugin and void_core::HotReloadable
/// to provide the foundation for hot-swappable gameplay systems.
class GameplayPlugin : public void_core::Plugin, public void_core::HotReloadable {
public:
    GameplayPlugin() = default;
    ~GameplayPlugin() override = default;

    // -------------------------------------------------------------------------
    // Plugin Interface
    // -------------------------------------------------------------------------

    /// @brief Called when plugin is loaded
    void_core::Result<void> on_load(void_core::PluginContext& ctx) override;

    /// @brief Called when plugin is unloaded
    void_core::Result<void_core::PluginState> on_unload(void_core::PluginContext& ctx) override;

    /// @brief Called when plugin is reloaded with previous state
    void_core::Result<void> on_reload(void_core::PluginContext& ctx, void_core::PluginState state) override;

    /// @brief Check if plugin supports hot-reload
    [[nodiscard]] bool supports_hot_reload() const override { return true; }

    // -------------------------------------------------------------------------
    // HotReloadable Interface
    // -------------------------------------------------------------------------

    /// @brief Capture current state as snapshot
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;

    /// @brief Restore state from snapshot
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;

    /// @brief Check if compatible with new version
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;

    /// @brief Get current version
    [[nodiscard]] void_core::Version current_version() const override { return version(); }

    /// @brief Get type name for debugging
    [[nodiscard]] std::string type_name() const override { return id().name(); }

    // -------------------------------------------------------------------------
    // Gameplay Plugin Interface (override these)
    // -------------------------------------------------------------------------

    /// @brief Called after plugin loads with API access
    virtual void on_plugin_load(IPluginAPI* api) {}

    /// @brief Called every frame
    virtual void on_tick(float dt) {}

    /// @brief Called at fixed timestep (physics rate)
    virtual void on_fixed_tick(float fixed_dt) {}

    /// @brief Called before plugin unloads
    virtual void on_plugin_unload() {}

    /// @brief Serialize runtime state for hot-reload (override if needed)
    virtual std::vector<std::uint8_t> serialize_runtime_state() const { return {}; }

    /// @brief Deserialize runtime state after hot-reload (override if needed)
    virtual void deserialize_runtime_state(const std::vector<std::uint8_t>& data) {}

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// @brief Get plugin API (available after on_load)
    [[nodiscard]] IPluginAPI* api() const { return m_api; }

protected:
    IPluginAPI* m_api{nullptr};
};

// =============================================================================
// Plugin API Implementation
// =============================================================================

/// @brief Implementation of IPluginAPI that connects to state stores
///
/// This implementation takes direct references to state stores and a command processor,
/// avoiding circular dependencies between modules.
class PluginAPIImpl : public IPluginAPI {
public:
    PluginAPIImpl(AIStateStore& ai, CombatStateStore& combat, InventoryStateStore& inventory,
                  CommandProcessor& processor);
    ~PluginAPIImpl() override = default;

    // State Store Access
    const AIStateStore& ai_state() const override;
    const CombatStateStore& combat_state() const override;
    const InventoryStateStore& inventory_state() const override;

    // Command Submission
    CommandResult submit_command(CommandPtr command) override;
    void queue_command(CommandPtr command) override;

    // Engine Services
    double current_time() const override;
    float delta_time() const override;
    std::uint32_t frame_number() const override;
    bool is_paused() const override;

    // Entity Queries
    bool entity_exists(EntityId entity) const override;
    Vec3 get_entity_position(EntityId entity) const override;
    std::vector<EntityId> get_entities_in_radius(Vec3 center, float radius) const override;

    // Events
    void emit_event(const std::string& event_name, const std::any& data) override;
    void subscribe_event(const std::string& event_name, EventCallback callback) override;

    // -------------------------------------------------------------------------
    // Internal - called by GameStateCore
    // -------------------------------------------------------------------------

    void set_delta_time(float dt) { m_delta_time = dt; }
    void set_frame_number(std::uint32_t frame) { m_frame_number = frame; }
    void set_paused(bool paused) { m_paused = paused; }
    void set_time(double time) { m_current_time = time; }

private:
    AIStateStore& m_ai_state;
    CombatStateStore& m_combat_state;
    InventoryStateStore& m_inventory_state;
    CommandProcessor& m_command_processor;

    float m_delta_time{0};
    std::uint32_t m_frame_number{0};
    bool m_paused{false};
    double m_current_time{0};

    std::unordered_map<std::string, std::vector<EventCallback>> m_event_subscriptions;
};

} // namespace void_plugin_api
