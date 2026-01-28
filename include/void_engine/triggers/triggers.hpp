/// @file triggers.hpp
/// @brief Main trigger system header for void_triggers module

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "volumes.hpp"
#include "conditions.hpp"
#include "actions.hpp"
#include "events.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declaration for event bus integration
namespace void_event { class EventBus; }

namespace void_triggers {

// =============================================================================
// Trigger
// =============================================================================

/// @brief Core trigger class
class Trigger {
public:
    Trigger();
    explicit Trigger(const TriggerConfig& config);
    ~Trigger();

    // Move/copy
    Trigger(Trigger&&) noexcept;
    Trigger& operator=(Trigger&&) noexcept;
    Trigger(const Trigger&) = delete;
    Trigger& operator=(const Trigger&) = delete;

    // Identity
    TriggerId id() const { return m_id; }
    const std::string& name() const { return m_config.name; }
    TriggerType type() const { return m_config.type; }
    TriggerFlags flags() const { return m_config.flags; }

    // State
    TriggerState state() const { return m_state; }
    bool is_enabled() const { return m_enabled && m_state != TriggerState::Disabled; }
    bool is_active() const { return m_state == TriggerState::Active; }
    bool can_activate() const;

    void enable() { m_enabled = true; m_state = TriggerState::Inactive; }
    void disable() { m_enabled = false; m_state = TriggerState::Disabled; }

    // Configuration
    const TriggerConfig& config() const { return m_config; }
    void set_config(const TriggerConfig& config) { m_config = config; }

    // Volume
    void set_volume(std::unique_ptr<ITriggerVolume> volume);
    ITriggerVolume* volume() { return m_volume.get(); }
    const ITriggerVolume* volume() const { return m_volume.get(); }

    // Conditions
    void set_condition(std::unique_ptr<ICondition> condition);
    void add_condition(std::unique_ptr<ICondition> condition);
    ICondition* condition() { return m_condition.get(); }
    bool check_conditions(const TriggerEvent& event) const;

    // Actions
    void set_action(std::unique_ptr<IAction> action);
    void add_action(std::unique_ptr<IAction> action);
    IAction* action() { return m_action.get(); }

    // Execution
    /// @brief Try to activate the trigger
    bool try_activate(const TriggerEvent& event);

    /// @brief Update the trigger
    void update(float dt, const TriggerEvent& event);

    /// @brief Reset the trigger
    void reset();

    // Statistics
    std::uint32_t activation_count() const { return m_activation_count; }
    double last_activation_time() const { return m_last_activation; }
    float cooldown_remaining() const { return m_cooldown_remaining; }

    // State restoration (for hot reload)
    void set_state(TriggerState state) { m_state = state; }
    void set_activation_count(std::uint32_t count) { m_activation_count = count; }
    void set_last_activation(double time) { m_last_activation = time; }
    void set_cooldown_remaining(float time) { m_cooldown_remaining = time; }

    // Entities inside
    const std::unordered_set<EntityId>& entities_inside() const { return m_entities_inside; }
    void add_entity(EntityId entity);
    void remove_entity(EntityId entity);
    bool has_entity(EntityId entity) const;

    // Callbacks
    void set_on_enter(TriggerCallback callback) { m_on_enter = std::move(callback); }
    void set_on_exit(TriggerCallback callback) { m_on_exit = std::move(callback); }
    void set_on_stay(TriggerCallback callback) { m_on_stay = std::move(callback); }
    void set_on_activate(TriggerCallback callback) { m_on_activate = std::move(callback); }

    // Invoke callbacks
    void invoke_on_enter(const TriggerEvent& event) { if (m_on_enter) m_on_enter(event); }
    void invoke_on_exit(const TriggerEvent& event) { if (m_on_exit) m_on_exit(event); }
    void invoke_on_stay(const TriggerEvent& event) { if (m_on_stay) m_on_stay(event); }
    void invoke_on_activate(const TriggerEvent& event) { if (m_on_activate) m_on_activate(event); }

    // Internal
    void set_id(TriggerId id) { m_id = id; }

private:
    void execute_action(const TriggerEvent& event);
    void start_cooldown();

    TriggerId m_id;
    TriggerConfig m_config;
    TriggerState m_state{TriggerState::Inactive};
    bool m_enabled{true};

    std::unique_ptr<ITriggerVolume> m_volume;
    std::unique_ptr<ICondition> m_condition;
    std::unique_ptr<IAction> m_action;

    std::unordered_set<EntityId> m_entities_inside;

    std::uint32_t m_activation_count{0};
    double m_last_activation{0};
    float m_cooldown_remaining{0};
    float m_delay_remaining{0};
    bool m_action_pending{false};

    TriggerCallback m_on_enter;
    TriggerCallback m_on_exit;
    TriggerCallback m_on_stay;
    TriggerCallback m_on_activate;
};

// =============================================================================
// TriggerZone
// =============================================================================

/// @brief Named zone that can contain multiple triggers
class TriggerZone {
public:
    TriggerZone();
    explicit TriggerZone(const ZoneConfig& config);
    ~TriggerZone();

    // Identity
    ZoneId id() const { return m_id; }
    const std::string& name() const { return m_config.name; }

    // Configuration
    const ZoneConfig& config() const { return m_config; }
    void set_config(const ZoneConfig& config);

    // Volume
    ITriggerVolume* volume() { return m_volume.get(); }
    const ITriggerVolume* volume() const { return m_volume.get(); }
    void set_volume(std::unique_ptr<ITriggerVolume> volume);

    // Position
    Vec3 position() const { return m_config.position; }
    void set_position(const Vec3& pos);

    // Enabled state
    bool is_enabled() const { return m_config.enabled; }
    void set_enabled(bool enabled) { m_config.enabled = enabled; }

    // Containment test
    bool contains(const Vec3& point) const;
    bool contains_entity(EntityId entity, EntityPositionCallback pos_getter) const;

    // Associated triggers
    void add_trigger(TriggerId trigger);
    void remove_trigger(TriggerId trigger);
    const std::vector<TriggerId>& triggers() const { return m_triggers; }

    // Internal
    void set_id(ZoneId id) { m_id = id; }

private:
    ZoneId m_id;
    ZoneConfig m_config;
    std::unique_ptr<ITriggerVolume> m_volume;
    std::vector<TriggerId> m_triggers;
};

// =============================================================================
// TriggerSystem
// =============================================================================

/// @brief Main trigger system
///
/// The TriggerSystem emits events via the EventBus as the primary
/// communication path. This is hot-reload safe because:
/// - Events are data, not function pointers
/// - Subscribers re-register on plugin load
/// - No dangling references after DLL unload
///
/// Legacy callbacks are still supported for internal wiring and
/// non-plugin code, but event bus emission is always performed.
class TriggerSystem {
public:
    TriggerSystem();
    explicit TriggerSystem(const TriggerSystemConfig& config);
    ~TriggerSystem();

    // -------------------------------------------------------------------------
    // Event Bus Integration (hot-reload safe)
    // -------------------------------------------------------------------------

    /// @brief Set the event bus for event emission
    ///
    /// When set, the trigger system emits typed events for every trigger
    /// interaction (enter, exit, stay, activate, state changes).
    /// Plugins subscribe via EventBus and re-register on hot-reload.
    void set_event_bus(void_event::EventBus* event_bus) { m_event_bus = event_bus; }

    /// @brief Get the current event bus
    [[nodiscard]] void_event::EventBus* event_bus() const { return m_event_bus; }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    const TriggerSystemConfig& config() const { return m_config; }
    void set_config(const TriggerSystemConfig& config);

    // Trigger management
    TriggerId create_trigger(const TriggerConfig& config);
    Trigger* get_trigger(TriggerId id);
    const Trigger* get_trigger(TriggerId id) const;
    bool remove_trigger(TriggerId id);
    std::vector<TriggerId> all_triggers() const;
    std::size_t trigger_count() const { return m_triggers.size(); }

    // Zone management
    ZoneId create_zone(const ZoneConfig& config);
    TriggerZone* get_zone(ZoneId id);
    const TriggerZone* get_zone(ZoneId id) const;
    bool remove_zone(ZoneId id);
    std::vector<ZoneId> all_zones() const;

    // Trigger by name
    Trigger* find_trigger(const std::string& name);
    TriggerZone* find_zone(const std::string& name);

    // Entity tracking
    /// @brief Notify system of entity position update
    void update_entity(EntityId entity, const Vec3& position);

    /// @brief Remove entity from tracking
    void remove_entity(EntityId entity);

    /// @brief Get entities in a trigger
    std::vector<EntityId> entities_in_trigger(TriggerId trigger) const;

    /// @brief Get triggers containing entity
    std::vector<TriggerId> triggers_containing(EntityId entity) const;

    // Manual triggering
    /// @brief Manually fire a trigger
    bool fire_trigger(TriggerId trigger, const TriggerEvent& event);

    /// @brief Send custom event to triggers
    void send_event(const std::string& event_type, EntityId entity, const Vec3& position);

    // Update
    void update(float dt);

    // Callbacks
    void set_position_getter(EntityPositionCallback callback) { m_position_getter = std::move(callback); }
    void set_tags_getter(EntityTagsCallback callback) { m_tags_getter = std::move(callback); }

    /// @brief Set player check callback
    using IsPlayerCallback = std::function<bool(EntityId entity)>;
    void set_player_checker(IsPlayerCallback callback) { m_is_player = std::move(callback); }

    // Global callbacks
    void set_on_trigger_enter(TriggerCallback callback) { m_on_trigger_enter = std::move(callback); }
    void set_on_trigger_exit(TriggerCallback callback) { m_on_trigger_exit = std::move(callback); }
    void set_on_trigger_activate(TriggerCallback callback) { m_on_trigger_activate = std::move(callback); }

    // Time
    double current_time() const { return m_current_time; }
    void set_time(double time) { m_current_time = time; }

    // Statistics
    struct Stats {
        std::uint64_t total_triggers{0};
        std::uint64_t total_zones{0};
        std::uint64_t total_activations{0};
        std::uint64_t entities_tracked{0};
        std::uint64_t collision_checks{0};
    };

    const Stats& stats() const { return m_stats; }

    // Serialization
    struct Snapshot {
        struct TriggerData {
            std::uint64_t id;
            std::string name;
            std::uint8_t state;
            std::uint32_t activation_count;
            double last_activation;
            float cooldown_remaining;
            bool enabled;
        };
        std::vector<TriggerData> triggers;
        double current_time;
    };

    Snapshot take_snapshot() const;
    void apply_snapshot(const Snapshot& snapshot);

    // Clear
    void clear();

private:
    void process_entity_enter(EntityId entity, Trigger& trigger);
    void process_entity_exit(EntityId entity, Trigger& trigger);
    void process_entity_stay(EntityId entity, Trigger& trigger, float dt);
    bool check_entity_filter(EntityId entity, const Trigger& trigger) const;
    TriggerEvent create_event(TriggerEventType type, TriggerId trigger, EntityId entity, const Vec3& position);

    // Event bus emission helpers
    void emit_enter_event(const Trigger& trigger, EntityId entity, const Vec3& position);
    void emit_exit_event(const Trigger& trigger, EntityId entity, const Vec3& position);
    void emit_stay_event(const Trigger& trigger, EntityId entity, const Vec3& position, float dt);
    void emit_activated_event(const Trigger& trigger, EntityId entity, const Vec3& position, TriggerEventType cause);
    void emit_cooldown_started(const Trigger& trigger);
    void emit_cooldown_ended(const Trigger& trigger);
    void emit_state_change(const Trigger& trigger, bool enabled);

    TriggerSystemConfig m_config;

    // Event bus for hot-reload-safe event emission
    void_event::EventBus* m_event_bus{nullptr};

    std::unordered_map<TriggerId, std::unique_ptr<Trigger>> m_triggers;
    std::unordered_map<ZoneId, std::unique_ptr<TriggerZone>> m_zones;
    std::unordered_map<std::string, TriggerId> m_trigger_names;
    std::unordered_map<std::string, ZoneId> m_zone_names;

    // Entity tracking
    std::unordered_map<EntityId, Vec3> m_entity_positions;
    std::unordered_map<EntityId, std::unordered_set<TriggerId>> m_entity_triggers;

    // Per-entity per-trigger stay time tracking (for TriggerStayEvent.time_inside)
    struct EntityTriggerKey {
        EntityId entity;
        TriggerId trigger;
        bool operator==(const EntityTriggerKey&) const = default;
    };
    struct EntityTriggerKeyHash {
        std::size_t operator()(const EntityTriggerKey& k) const noexcept {
            auto h1 = std::hash<std::uint64_t>{}(k.entity.value);
            auto h2 = std::hash<std::uint64_t>{}(k.trigger.value);
            return h1 ^ (h2 * 2654435761ULL);
        }
    };
    std::unordered_map<EntityTriggerKey, float, EntityTriggerKeyHash> m_entity_stay_times;

    // Spatial acceleration
    struct SpatialCell {
        std::vector<TriggerId> triggers;
    };
    std::unordered_map<std::int64_t, SpatialCell> m_spatial_grid;

    EntityPositionCallback m_position_getter;
    EntityTagsCallback m_tags_getter;
    IsPlayerCallback m_is_player;

    // Legacy callbacks (still supported for non-plugin code)
    TriggerCallback m_on_trigger_enter;
    TriggerCallback m_on_trigger_exit;
    TriggerCallback m_on_trigger_activate;

    double m_current_time{0};
    Stats m_stats;

    std::uint64_t m_next_trigger_id{1};
    std::uint64_t m_next_zone_id{1};
    std::uint64_t m_next_event_id{1};
};

// =============================================================================
// Prelude - Convenient Namespace
// =============================================================================

namespace prelude {
    using void_triggers::Vec3;
    using void_triggers::Quat;
    using void_triggers::AABB;
    using void_triggers::Sphere;

    using void_triggers::TriggerType;
    using void_triggers::VolumeType;
    using void_triggers::TriggerState;
    using void_triggers::TriggerFlags;
    using void_triggers::CompareOp;
    using void_triggers::LogicalOp;
    using void_triggers::ActionMode;
    using void_triggers::ActionResult;
    using void_triggers::TriggerEventType;

    using void_triggers::TriggerId;
    using void_triggers::ZoneId;
    using void_triggers::ConditionId;
    using void_triggers::ActionId;
    using void_triggers::EntityId;

    using void_triggers::TriggerEvent;
    using void_triggers::TriggerConfig;
    using void_triggers::ZoneConfig;
    using void_triggers::TriggerSystemConfig;
    using void_triggers::VariableValue;

    using void_triggers::ITriggerVolume;
    using void_triggers::BoxVolume;
    using void_triggers::SphereVolume;
    using void_triggers::CapsuleVolume;
    using void_triggers::OrientedBoxVolume;
    using void_triggers::CompositeVolume;
    using void_triggers::VolumeFactory;

    using void_triggers::ICondition;
    using void_triggers::ConditionGroup;
    using void_triggers::VariableCondition;
    using void_triggers::EntityCondition;
    using void_triggers::TimerCondition;
    using void_triggers::CountCondition;
    using void_triggers::RandomCondition;
    using void_triggers::DistanceCondition;
    using void_triggers::TagCondition;
    using void_triggers::CallbackCondition;
    using void_triggers::ConditionBuilder;

    using void_triggers::IAction;
    using void_triggers::ActionSequence;
    using void_triggers::CallbackAction;
    using void_triggers::DelayedAction;
    using void_triggers::SpawnAction;
    using void_triggers::DestroyAction;
    using void_triggers::TeleportAction;
    using void_triggers::SetVariableAction;
    using void_triggers::SendEventAction;
    using void_triggers::PlayAudioAction;
    using void_triggers::PlayEffectAction;
    using void_triggers::EnableTriggerAction;
    using void_triggers::InterpolatedAction;
    using void_triggers::ActionBuilder;

    using void_triggers::Trigger;
    using void_triggers::TriggerZone;
    using void_triggers::TriggerSystem;

    // Event types (hot-reload safe)
    using void_triggers::TriggerEnterEvent;
    using void_triggers::TriggerExitEvent;
    using void_triggers::TriggerStayEvent;
    using void_triggers::TriggerActivatedEvent;
    using void_triggers::TriggerCooldownStartedEvent;
    using void_triggers::TriggerCooldownEndedEvent;
    using void_triggers::TriggerEnabledEvent;
    using void_triggers::TriggerDisabledEvent;
    using void_triggers::TriggerCreatedEvent;
    using void_triggers::TriggerDestroyedEvent;
    using void_triggers::TriggerResetEvent;
    using void_triggers::ZoneCreatedEvent;
    using void_triggers::ZoneDestroyedEvent;

    // Action request events (for plugin-based handling)
    using void_triggers::SpawnRequestEvent;
    using void_triggers::DestroyRequestEvent;
    using void_triggers::TeleportRequestEvent;
    using void_triggers::PlayAudioRequestEvent;
    using void_triggers::PlayEffectRequestEvent;
    using void_triggers::SetVariableRequestEvent;
    using void_triggers::EnableTriggerRequestEvent;
    using void_triggers::TriggerCustomEvent;
}

} // namespace void_triggers
