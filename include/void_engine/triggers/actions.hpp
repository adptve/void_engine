/// @file actions.hpp
/// @brief Action system for void_triggers module

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "events.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declaration for event bus integration
namespace void_event { class EventBus; }

namespace void_triggers {

// =============================================================================
// IAction Interface
// =============================================================================

/// @brief Interface for trigger actions
class IAction {
public:
    virtual ~IAction() = default;

    /// @brief Execute the action
    /// @param event The trigger event
    /// @param dt Delta time (for continuous actions)
    /// @return Action result
    virtual ActionResult execute(const TriggerEvent& event, float dt) = 0;

    /// @brief Reset the action state
    virtual void reset() = 0;

    /// @brief Get action description
    virtual std::string description() const = 0;

    /// @brief Clone the action
    virtual std::unique_ptr<IAction> clone() const = 0;

    /// @brief Get execution mode
    virtual ActionMode mode() const { return ActionMode::Immediate; }

    /// @brief Check if action is complete
    virtual bool is_complete() const { return true; }
};

// =============================================================================
// ActionSequence
// =============================================================================

/// @brief Sequence of actions executed in order
class ActionSequence : public IAction {
public:
    ActionSequence();
    ~ActionSequence() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;
    ActionMode mode() const override { return m_mode; }
    bool is_complete() const override { return m_current_index >= m_actions.size(); }

    /// @brief Add an action to the sequence
    void add(std::unique_ptr<IAction> action);

    /// @brief Clear all actions
    void clear();

    /// @brief Get action count
    std::size_t count() const { return m_actions.size(); }

    /// @brief Set whether to run all at once or one per frame
    void set_parallel(bool parallel) { m_parallel = parallel; }

    /// @brief Set execution mode
    void set_mode(ActionMode mode) { m_mode = mode; }

    // Fluent builder
    ActionSequence& then(std::unique_ptr<IAction> action) {
        add(std::move(action));
        return *this;
    }

private:
    std::vector<std::unique_ptr<IAction>> m_actions;
    std::size_t m_current_index{0};
    bool m_parallel{false};
    ActionMode m_mode{ActionMode::Immediate};
};

// =============================================================================
// CallbackAction
// =============================================================================

/// @brief Custom callback action
class CallbackAction : public IAction {
public:
    CallbackAction();
    explicit CallbackAction(ActionCallback callback, const std::string& desc = "Callback");
    ~CallbackAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;

    void set_callback(ActionCallback callback) { m_callback = std::move(callback); }
    void set_description(const std::string& desc) { m_description = desc; }

private:
    ActionCallback m_callback;
    std::string m_description{"Callback"};
};

// =============================================================================
// DelayedAction
// =============================================================================

/// @brief Action that executes after a delay
class DelayedAction : public IAction {
public:
    DelayedAction();
    DelayedAction(std::unique_ptr<IAction> action, float delay);
    ~DelayedAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;
    ActionMode mode() const override { return ActionMode::Delayed; }
    bool is_complete() const override { return m_executed; }

    void set_delay(float delay) { m_delay = delay; }
    float delay() const { return m_delay; }

private:
    std::unique_ptr<IAction> m_action;
    float m_delay{1.0f};
    float m_elapsed{0};
    bool m_executed{false};
};

// =============================================================================
// SpawnAction
// =============================================================================

/// @brief Action that spawns entities
class SpawnAction : public IAction {
public:
    using SpawnCallback = std::function<EntityId(const std::string&, const Vec3&, const Quat&)>;

    SpawnAction();
    SpawnAction(const std::string& prefab, const Vec3& offset);
    ~SpawnAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;

    void set_prefab(const std::string& prefab) { m_prefab = prefab; }
    void set_offset(const Vec3& offset) { m_offset = offset; }
    void set_rotation(const Quat& rotation) { m_rotation = rotation; }
    void set_count(std::uint32_t count) { m_count = count; }
    void set_spawn_at_trigger(bool at_trigger) { m_spawn_at_trigger = at_trigger; }
    void set_spawn_callback(SpawnCallback callback) { m_spawn_callback = std::move(callback); }

    /// @brief Set event bus for hot-reload-safe event emission
    /// When set and no callback is provided, emits SpawnRequestEvent instead
    void set_event_bus(void_event::EventBus* bus) { m_event_bus = bus; }

    /// @brief Get last spawned entity
    EntityId last_spawned() const { return m_last_spawned; }

private:
    std::string m_prefab;
    Vec3 m_offset;
    Quat m_rotation{0, 0, 0, 1};
    std::uint32_t m_count{1};
    bool m_spawn_at_trigger{true};
    SpawnCallback m_spawn_callback;
    void_event::EventBus* m_event_bus{nullptr};
    EntityId m_last_spawned;
};

// =============================================================================
// DestroyAction
// =============================================================================

/// @brief Action that destroys entities
class DestroyAction : public IAction {
public:
    using DestroyCallback = std::function<void(EntityId)>;

    DestroyAction();
    ~DestroyAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;

    void set_target_entity(EntityId entity) { m_target = entity; m_destroy_triggering = false; }
    void set_destroy_triggering(bool destroy) { m_destroy_triggering = destroy; }
    void set_destroy_callback(DestroyCallback callback) { m_destroy_callback = std::move(callback); }
    void set_event_bus(void_event::EventBus* bus) { m_event_bus = bus; }

private:
    EntityId m_target;
    bool m_destroy_triggering{true};
    DestroyCallback m_destroy_callback;
    void_event::EventBus* m_event_bus{nullptr};
};

// =============================================================================
// TeleportAction
// =============================================================================

/// @brief Action that teleports entities
class TeleportAction : public IAction {
public:
    using TeleportCallback = std::function<void(EntityId, const Vec3&, const Quat&)>;

    TeleportAction();
    TeleportAction(const Vec3& destination);
    ~TeleportAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;

    void set_destination(const Vec3& dest) { m_destination = dest; }
    void set_rotation(const Quat& rotation) { m_rotation = rotation; m_set_rotation = true; }
    void set_relative(bool relative) { m_relative = relative; }
    void set_teleport_callback(TeleportCallback callback) { m_teleport_callback = std::move(callback); }
    void set_event_bus(void_event::EventBus* bus) { m_event_bus = bus; }

private:
    Vec3 m_destination;
    Quat m_rotation{0, 0, 0, 1};
    bool m_set_rotation{false};
    bool m_relative{false};
    TeleportCallback m_teleport_callback;
    void_event::EventBus* m_event_bus{nullptr};
};

// =============================================================================
// SetVariableAction
// =============================================================================

/// @brief Action that sets a variable value
class SetVariableAction : public IAction {
public:
    using VariableSetter = std::function<void(const std::string&, const VariableValue&)>;

    enum class Operation {
        Set,
        Add,
        Subtract,
        Multiply,
        Divide,
        Toggle,
        Increment,
        Decrement
    };

    SetVariableAction();
    SetVariableAction(const std::string& variable, const VariableValue& value);
    ~SetVariableAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;

    void set_variable(const std::string& name) { m_variable = name; }
    void set_value(const VariableValue& value) { m_value = value; }
    void set_operation(Operation op) { m_operation = op; }
    void set_variable_setter(VariableSetter setter) { m_setter = std::move(setter); }
    void set_event_bus(void_event::EventBus* bus) { m_event_bus = bus; }

    using VariableGetter = std::function<VariableValue(const std::string&)>;
    void set_variable_getter(VariableGetter getter) { m_getter = std::move(getter); }

private:
    std::string m_variable;
    VariableValue m_value;
    Operation m_operation{Operation::Set};
    VariableSetter m_setter;
    VariableGetter m_getter;
    void_event::EventBus* m_event_bus{nullptr};
};

// =============================================================================
// SendEventAction
// =============================================================================

/// @brief Action that sends a custom event
class SendEventAction : public IAction {
public:
    using EventSender = std::function<void(const std::string&, const TriggerEvent&)>;

    SendEventAction();
    explicit SendEventAction(const std::string& event_name);
    ~SendEventAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;

    void set_event_name(const std::string& name) { m_event_name = name; }
    void set_target_entity(EntityId entity) { m_target = entity; }
    void set_broadcast(bool broadcast) { m_broadcast = broadcast; }
    void set_event_sender(EventSender sender) { m_sender = std::move(sender); }
    void set_event_bus(void_event::EventBus* bus) { m_event_bus = bus; }

    template<typename T>
    void set_data(const std::string& key, const T& value) {
        m_data[key] = value;
    }

private:
    std::string m_event_name;
    EntityId m_target;
    bool m_broadcast{false};
    std::unordered_map<std::string, std::any> m_data;
    EventSender m_sender;
    void_event::EventBus* m_event_bus{nullptr};
};

// =============================================================================
// PlayAudioAction
// =============================================================================

/// @brief Action that plays audio
class PlayAudioAction : public IAction {
public:
    using AudioCallback = std::function<void(const std::string&, const Vec3&, float, float)>;

    PlayAudioAction();
    explicit PlayAudioAction(const std::string& audio_path);
    ~PlayAudioAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;

    void set_audio_path(const std::string& path) { m_audio_path = path; }
    void set_volume(float volume) { m_volume = volume; }
    void set_pitch(float pitch) { m_pitch = pitch; }
    void set_spatial(bool spatial) { m_spatial = spatial; }
    void set_audio_callback(AudioCallback callback) { m_audio_callback = std::move(callback); }
    void set_event_bus(void_event::EventBus* bus) { m_event_bus = bus; }

private:
    std::string m_audio_path;
    float m_volume{1.0f};
    float m_pitch{1.0f};
    bool m_spatial{true};
    AudioCallback m_audio_callback;
    void_event::EventBus* m_event_bus{nullptr};
};

// =============================================================================
// PlayEffectAction
// =============================================================================

/// @brief Action that plays visual effects
class PlayEffectAction : public IAction {
public:
    using EffectCallback = std::function<void(const std::string&, const Vec3&, const Quat&, float)>;

    PlayEffectAction();
    explicit PlayEffectAction(const std::string& effect_path);
    ~PlayEffectAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;

    void set_effect_path(const std::string& path) { m_effect_path = path; }
    void set_offset(const Vec3& offset) { m_offset = offset; }
    void set_rotation(const Quat& rotation) { m_rotation = rotation; }
    void set_scale(float scale) { m_scale = scale; }
    void set_attach_to_entity(bool attach) { m_attach = attach; }
    void set_effect_callback(EffectCallback callback) { m_effect_callback = std::move(callback); }
    void set_event_bus(void_event::EventBus* bus) { m_event_bus = bus; }

private:
    std::string m_effect_path;
    Vec3 m_offset;
    Quat m_rotation{0, 0, 0, 1};
    float m_scale{1.0f};
    bool m_attach{false};
    EffectCallback m_effect_callback;
    void_event::EventBus* m_event_bus{nullptr};
};

// =============================================================================
// EnableTriggerAction
// =============================================================================

/// @brief Action that enables/disables other triggers
class EnableTriggerAction : public IAction {
public:
    using TriggerEnableCallback = std::function<void(TriggerId, bool)>;

    EnableTriggerAction();
    EnableTriggerAction(TriggerId trigger, bool enable);
    ~EnableTriggerAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;

    void set_target_trigger(TriggerId trigger) { m_target = trigger; }
    void set_enable(bool enable) { m_enable = enable; }
    void set_toggle(bool toggle) { m_toggle = toggle; }
    void set_enable_callback(TriggerEnableCallback callback) { m_callback = std::move(callback); }
    void set_event_bus(void_event::EventBus* bus) { m_event_bus = bus; }

private:
    TriggerId m_target;
    bool m_enable{true};
    bool m_toggle{false};
    TriggerEnableCallback m_callback;
    void_event::EventBus* m_event_bus{nullptr};
};

// =============================================================================
// InterpolatedAction
// =============================================================================

/// @brief Action that interpolates a value over time
class InterpolatedAction : public IAction {
public:
    enum class EaseType {
        Linear,
        EaseIn,
        EaseOut,
        EaseInOut,
        Bounce,
        Elastic
    };

    using InterpolationCallback = std::function<void(float)>;  // 0-1 progress

    InterpolatedAction();
    InterpolatedAction(float duration, InterpolationCallback callback);
    ~InterpolatedAction() override;

    ActionResult execute(const TriggerEvent& event, float dt) override;
    void reset() override;
    std::string description() const override;
    std::unique_ptr<IAction> clone() const override;
    ActionMode mode() const override { return ActionMode::Interpolated; }
    bool is_complete() const override { return m_elapsed >= m_duration; }

    void set_duration(float duration) { m_duration = duration; }
    void set_ease_type(EaseType ease) { m_ease_type = ease; }
    void set_interpolation_callback(InterpolationCallback callback) { m_callback = std::move(callback); }

private:
    float ease_value(float t) const;

    float m_duration{1.0f};
    float m_elapsed{0};
    EaseType m_ease_type{EaseType::Linear};
    InterpolationCallback m_callback;
};

// =============================================================================
// Action Builder
// =============================================================================

/// @brief Fluent builder for actions
class ActionBuilder {
public:
    ActionBuilder() = default;

    /// @brief Create a callback action
    static std::unique_ptr<CallbackAction> callback(ActionCallback cb);

    /// @brief Create a delayed action
    static std::unique_ptr<DelayedAction> delay(std::unique_ptr<IAction> action, float seconds);

    /// @brief Create a spawn action
    static std::unique_ptr<SpawnAction> spawn(const std::string& prefab);

    /// @brief Create a destroy action
    static std::unique_ptr<DestroyAction> destroy();

    /// @brief Create a teleport action
    static std::unique_ptr<TeleportAction> teleport(const Vec3& destination);

    /// @brief Create a set variable action
    static std::unique_ptr<SetVariableAction> set_var(const std::string& name, const VariableValue& value);

    /// @brief Create a send event action
    static std::unique_ptr<SendEventAction> send_event(const std::string& event_name);

    /// @brief Create a play audio action
    static std::unique_ptr<PlayAudioAction> play_audio(const std::string& path);

    /// @brief Create a play effect action
    static std::unique_ptr<PlayEffectAction> play_effect(const std::string& path);

    /// @brief Create an enable trigger action
    static std::unique_ptr<EnableTriggerAction> enable_trigger(TriggerId trigger, bool enable);

    /// @brief Create a sequence
    static std::unique_ptr<ActionSequence> sequence();

    /// @brief Create an interpolated action
    static std::unique_ptr<InterpolatedAction> interpolate(float duration,
                                                           InterpolatedAction::InterpolationCallback cb);
};

} // namespace void_triggers
