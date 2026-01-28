/// @file actions.cpp
/// @brief Action system implementation for void_triggers module

#include <void_engine/triggers/actions.hpp>
#include <void_engine/event/event_bus.hpp>

#include <cmath>

namespace void_triggers {

// =============================================================================
// ActionSequence Implementation
// =============================================================================

ActionSequence::ActionSequence() = default;
ActionSequence::~ActionSequence() = default;

ActionResult ActionSequence::execute(const TriggerEvent& event, float dt) {
    if (m_parallel) {
        // Execute all actions
        bool any_running = false;
        for (auto& action : m_actions) {
            ActionResult result = action->execute(event, dt);
            if (result == ActionResult::Running) {
                any_running = true;
            }
        }
        return any_running ? ActionResult::Running : ActionResult::Success;
    }

    // Sequential execution
    while (m_current_index < m_actions.size()) {
        ActionResult result = m_actions[m_current_index]->execute(event, dt);
        if (result == ActionResult::Running) {
            return ActionResult::Running;
        }
        if (result == ActionResult::Failed || result == ActionResult::Cancelled) {
            return result;
        }
        ++m_current_index;
    }

    return ActionResult::Success;
}

void ActionSequence::reset() {
    m_current_index = 0;
    for (auto& action : m_actions) {
        action->reset();
    }
}

std::string ActionSequence::description() const {
    return "Sequence(" + std::to_string(m_actions.size()) + " actions)";
}

std::unique_ptr<IAction> ActionSequence::clone() const {
    auto result = std::make_unique<ActionSequence>();
    result->set_parallel(m_parallel);
    result->set_mode(m_mode);
    for (const auto& action : m_actions) {
        result->add(action->clone());
    }
    return result;
}

void ActionSequence::add(std::unique_ptr<IAction> action) {
    m_actions.push_back(std::move(action));
}

void ActionSequence::clear() {
    m_actions.clear();
    m_current_index = 0;
}

// =============================================================================
// CallbackAction Implementation
// =============================================================================

CallbackAction::CallbackAction() = default;

CallbackAction::CallbackAction(ActionCallback callback, const std::string& desc)
    : m_callback(std::move(callback))
    , m_description(desc) {
}

CallbackAction::~CallbackAction() = default;

ActionResult CallbackAction::execute(const TriggerEvent& event, float dt) {
    if (m_callback) {
        return m_callback(event, dt);
    }
    return ActionResult::Success;
}

void CallbackAction::reset() {
    // No state to reset
}

std::string CallbackAction::description() const {
    return m_description;
}

std::unique_ptr<IAction> CallbackAction::clone() const {
    return std::make_unique<CallbackAction>(m_callback, m_description);
}

// =============================================================================
// DelayedAction Implementation
// =============================================================================

DelayedAction::DelayedAction() = default;

DelayedAction::DelayedAction(std::unique_ptr<IAction> action, float delay)
    : m_action(std::move(action))
    , m_delay(delay) {
}

DelayedAction::~DelayedAction() = default;

ActionResult DelayedAction::execute(const TriggerEvent& event, float dt) {
    m_elapsed += dt;

    if (m_elapsed < m_delay) {
        return ActionResult::Running;
    }

    if (!m_executed && m_action) {
        ActionResult result = m_action->execute(event, dt);
        if (result != ActionResult::Running) {
            m_executed = true;
        }
        return result;
    }

    return ActionResult::Success;
}

void DelayedAction::reset() {
    m_elapsed = 0;
    m_executed = false;
    if (m_action) {
        m_action->reset();
    }
}

std::string DelayedAction::description() const {
    std::string inner = m_action ? m_action->description() : "None";
    return "Delay(" + std::to_string(m_delay) + "s, " + inner + ")";
}

std::unique_ptr<IAction> DelayedAction::clone() const {
    return std::make_unique<DelayedAction>(
        m_action ? m_action->clone() : nullptr,
        m_delay
    );
}

// =============================================================================
// SpawnAction Implementation
// =============================================================================

SpawnAction::SpawnAction() = default;

SpawnAction::SpawnAction(const std::string& prefab, const Vec3& offset)
    : m_prefab(prefab)
    , m_offset(offset) {
}

SpawnAction::~SpawnAction() = default;

ActionResult SpawnAction::execute(const TriggerEvent& event, float /*dt*/) {
    Vec3 spawn_pos = m_spawn_at_trigger ? event.position : m_offset;
    spawn_pos = spawn_pos + m_offset;

    // Prefer callback for direct execution; fall back to event bus
    if (m_spawn_callback) {
        for (std::uint32_t i = 0; i < m_count; ++i) {
            m_last_spawned = m_spawn_callback(m_prefab, spawn_pos, m_rotation);
        }
        return m_last_spawned ? ActionResult::Success : ActionResult::Failed;
    }

    if (m_event_bus) {
        m_event_bus->publish(SpawnRequestEvent{
            event.trigger,
            m_prefab,
            spawn_pos,
            m_rotation,
            m_count,
            event.timestamp
        });
        return ActionResult::Success;
    }

    return ActionResult::Failed;
}

void SpawnAction::reset() {
    m_last_spawned = EntityId{};
}

std::string SpawnAction::description() const {
    return "Spawn(" + m_prefab + ")";
}

std::unique_ptr<IAction> SpawnAction::clone() const {
    auto result = std::make_unique<SpawnAction>(m_prefab, m_offset);
    result->set_rotation(m_rotation);
    result->set_count(m_count);
    result->set_spawn_at_trigger(m_spawn_at_trigger);
    result->set_spawn_callback(m_spawn_callback);
    return result;
}

// =============================================================================
// DestroyAction Implementation
// =============================================================================

DestroyAction::DestroyAction() = default;
DestroyAction::~DestroyAction() = default;

ActionResult DestroyAction::execute(const TriggerEvent& event, float /*dt*/) {
    EntityId target = m_destroy_triggering ? event.entity : m_target;

    if (m_destroy_callback) {
        if (!target) return ActionResult::Failed;
        m_destroy_callback(target);
        return ActionResult::Success;
    }

    if (m_event_bus) {
        m_event_bus->publish(DestroyRequestEvent{
            event.trigger,
            target,
            m_destroy_triggering,
            event.timestamp
        });
        return ActionResult::Success;
    }

    return ActionResult::Failed;
}

void DestroyAction::reset() {
    // No state to reset
}

std::string DestroyAction::description() const {
    return "Destroy";
}

std::unique_ptr<IAction> DestroyAction::clone() const {
    auto result = std::make_unique<DestroyAction>();
    result->set_target_entity(m_target);
    result->set_destroy_triggering(m_destroy_triggering);
    result->set_destroy_callback(m_destroy_callback);
    return result;
}

// =============================================================================
// TeleportAction Implementation
// =============================================================================

TeleportAction::TeleportAction() = default;

TeleportAction::TeleportAction(const Vec3& destination)
    : m_destination(destination) {
}

TeleportAction::~TeleportAction() = default;

ActionResult TeleportAction::execute(const TriggerEvent& event, float /*dt*/) {
    Vec3 final_pos = m_destination;
    if (m_relative) {
        final_pos = event.position + m_destination;
    }

    if (m_teleport_callback) {
        m_teleport_callback(event.entity, final_pos, m_rotation);
        return ActionResult::Success;
    }

    if (m_event_bus) {
        m_event_bus->publish(TeleportRequestEvent{
            event.trigger,
            event.entity,
            final_pos,
            m_rotation,
            m_set_rotation,
            m_relative,
            event.timestamp
        });
        return ActionResult::Success;
    }

    return ActionResult::Failed;
}

void TeleportAction::reset() {
    // No state to reset
}

std::string TeleportAction::description() const {
    return "Teleport";
}

std::unique_ptr<IAction> TeleportAction::clone() const {
    auto result = std::make_unique<TeleportAction>(m_destination);
    if (m_set_rotation) {
        result->set_rotation(m_rotation);
    }
    result->set_relative(m_relative);
    result->set_teleport_callback(m_teleport_callback);
    return result;
}

// =============================================================================
// SetVariableAction Implementation
// =============================================================================

SetVariableAction::SetVariableAction() = default;

SetVariableAction::SetVariableAction(const std::string& variable, const VariableValue& value)
    : m_variable(variable)
    , m_value(value) {
}

SetVariableAction::~SetVariableAction() = default;

ActionResult SetVariableAction::execute(const TriggerEvent& event, float /*dt*/) {
    // Event bus path: emit a request event for GameState to handle
    if (!m_setter && m_event_bus) {
        m_event_bus->publish(SetVariableRequestEvent{
            event.trigger,
            m_variable,
            m_value,
            static_cast<std::uint8_t>(m_operation),
            event.timestamp
        });
        return ActionResult::Success;
    }

    if (!m_setter) {
        return ActionResult::Failed;
    }

    VariableValue new_value = m_value;

    if (m_operation != Operation::Set && m_getter) {
        VariableValue current = m_getter(m_variable);

        switch (m_operation) {
            case Operation::Add:
                if (current.type == VariableType::Int) {
                    new_value = VariableValue(current.as_int() + m_value.as_int());
                } else if (current.type == VariableType::Float) {
                    new_value = VariableValue(current.as_float() + m_value.as_float());
                }
                break;

            case Operation::Subtract:
                if (current.type == VariableType::Int) {
                    new_value = VariableValue(current.as_int() - m_value.as_int());
                } else if (current.type == VariableType::Float) {
                    new_value = VariableValue(current.as_float() - m_value.as_float());
                }
                break;

            case Operation::Multiply:
                if (current.type == VariableType::Int) {
                    new_value = VariableValue(current.as_int() * m_value.as_int());
                } else if (current.type == VariableType::Float) {
                    new_value = VariableValue(current.as_float() * m_value.as_float());
                }
                break;

            case Operation::Divide:
                if (current.type == VariableType::Int && m_value.as_int() != 0) {
                    new_value = VariableValue(current.as_int() / m_value.as_int());
                } else if (current.type == VariableType::Float && m_value.as_float() != 0) {
                    new_value = VariableValue(current.as_float() / m_value.as_float());
                }
                break;

            case Operation::Toggle:
                new_value = VariableValue(!current.as_bool());
                break;

            case Operation::Increment:
                new_value = VariableValue(current.as_int() + 1);
                break;

            case Operation::Decrement:
                new_value = VariableValue(current.as_int() - 1);
                break;

            default:
                break;
        }
    }

    m_setter(m_variable, new_value);
    return ActionResult::Success;
}

void SetVariableAction::reset() {
    // No state to reset
}

std::string SetVariableAction::description() const {
    return "SetVariable(" + m_variable + ")";
}

std::unique_ptr<IAction> SetVariableAction::clone() const {
    auto result = std::make_unique<SetVariableAction>(m_variable, m_value);
    result->set_operation(m_operation);
    result->set_variable_setter(m_setter);
    result->set_variable_getter(m_getter);
    return result;
}

// =============================================================================
// SendEventAction Implementation
// =============================================================================

SendEventAction::SendEventAction() = default;

SendEventAction::SendEventAction(const std::string& event_name)
    : m_event_name(event_name) {
}

SendEventAction::~SendEventAction() = default;

ActionResult SendEventAction::execute(const TriggerEvent& event, float /*dt*/) {
    if (m_event_name.empty()) {
        return ActionResult::Failed;
    }

    // Legacy callback path
    if (m_sender) {
        TriggerEvent new_event = event;
        new_event.custom_type = m_event_name;
        new_event.type = TriggerEventType::Custom;
        new_event.data = m_data;
        m_sender(m_event_name, new_event);
        return ActionResult::Success;
    }

    // Event bus path (hot-reload safe)
    if (m_event_bus) {
        m_event_bus->publish(TriggerCustomEvent{
            event.trigger,
            event.entity,
            m_event_name,
            event.position,
            m_broadcast,
            event.timestamp
        });
        return ActionResult::Success;
    }

    return ActionResult::Failed;
}

void SendEventAction::reset() {
    // No state to reset
}

std::string SendEventAction::description() const {
    return "SendEvent(" + m_event_name + ")";
}

std::unique_ptr<IAction> SendEventAction::clone() const {
    auto result = std::make_unique<SendEventAction>(m_event_name);
    result->set_target_entity(m_target);
    result->set_broadcast(m_broadcast);
    result->set_event_sender(m_sender);
    result->m_data = m_data;
    return result;
}

// =============================================================================
// PlayAudioAction Implementation
// =============================================================================

PlayAudioAction::PlayAudioAction() = default;

PlayAudioAction::PlayAudioAction(const std::string& audio_path)
    : m_audio_path(audio_path) {
}

PlayAudioAction::~PlayAudioAction() = default;

ActionResult PlayAudioAction::execute(const TriggerEvent& event, float /*dt*/) {
    if (m_audio_path.empty()) {
        return ActionResult::Failed;
    }

    Vec3 pos = m_spatial ? event.position : Vec3{};

    if (m_audio_callback) {
        m_audio_callback(m_audio_path, pos, m_volume, m_pitch);
        return ActionResult::Success;
    }

    if (m_event_bus) {
        m_event_bus->publish(PlayAudioRequestEvent{
            event.trigger,
            m_audio_path,
            pos,
            m_volume,
            m_pitch,
            m_spatial,
            event.timestamp
        });
        return ActionResult::Success;
    }

    return ActionResult::Failed;
}

void PlayAudioAction::reset() {
    // No state to reset
}

std::string PlayAudioAction::description() const {
    return "PlayAudio(" + m_audio_path + ")";
}

std::unique_ptr<IAction> PlayAudioAction::clone() const {
    auto result = std::make_unique<PlayAudioAction>(m_audio_path);
    result->set_volume(m_volume);
    result->set_pitch(m_pitch);
    result->set_spatial(m_spatial);
    result->set_audio_callback(m_audio_callback);
    return result;
}

// =============================================================================
// PlayEffectAction Implementation
// =============================================================================

PlayEffectAction::PlayEffectAction() = default;

PlayEffectAction::PlayEffectAction(const std::string& effect_path)
    : m_effect_path(effect_path) {
}

PlayEffectAction::~PlayEffectAction() = default;

ActionResult PlayEffectAction::execute(const TriggerEvent& event, float /*dt*/) {
    if (m_effect_path.empty()) {
        return ActionResult::Failed;
    }

    Vec3 pos = event.position + m_offset;

    if (m_effect_callback) {
        m_effect_callback(m_effect_path, pos, m_rotation, m_scale);
        return ActionResult::Success;
    }

    if (m_event_bus) {
        EntityId attach_entity = m_attach ? event.entity : EntityId{};
        m_event_bus->publish(PlayEffectRequestEvent{
            event.trigger,
            m_effect_path,
            pos,
            m_rotation,
            m_scale,
            attach_entity,
            event.timestamp
        });
        return ActionResult::Success;
    }

    return ActionResult::Failed;
}

void PlayEffectAction::reset() {
    // No state to reset
}

std::string PlayEffectAction::description() const {
    return "PlayEffect(" + m_effect_path + ")";
}

std::unique_ptr<IAction> PlayEffectAction::clone() const {
    auto result = std::make_unique<PlayEffectAction>(m_effect_path);
    result->set_offset(m_offset);
    result->set_rotation(m_rotation);
    result->set_scale(m_scale);
    result->set_attach_to_entity(m_attach);
    result->set_effect_callback(m_effect_callback);
    return result;
}

// =============================================================================
// EnableTriggerAction Implementation
// =============================================================================

EnableTriggerAction::EnableTriggerAction() = default;

EnableTriggerAction::EnableTriggerAction(TriggerId trigger, bool enable)
    : m_target(trigger)
    , m_enable(enable) {
}

EnableTriggerAction::~EnableTriggerAction() = default;

ActionResult EnableTriggerAction::execute(const TriggerEvent& event, float /*dt*/) {
    if (!m_target) {
        return ActionResult::Failed;
    }

    if (m_callback) {
        m_callback(m_target, m_toggle ? true : m_enable);
        return ActionResult::Success;
    }

    if (m_event_bus) {
        m_event_bus->publish(EnableTriggerRequestEvent{
            event.trigger,
            m_target,
            m_enable,
            m_toggle,
            event.timestamp
        });
        return ActionResult::Success;
    }

    return ActionResult::Failed;
}

void EnableTriggerAction::reset() {
    // No state to reset
}

std::string EnableTriggerAction::description() const {
    return m_enable ? "EnableTrigger" : "DisableTrigger";
}

std::unique_ptr<IAction> EnableTriggerAction::clone() const {
    auto result = std::make_unique<EnableTriggerAction>(m_target, m_enable);
    result->set_toggle(m_toggle);
    result->set_enable_callback(m_callback);
    return result;
}

// =============================================================================
// InterpolatedAction Implementation
// =============================================================================

InterpolatedAction::InterpolatedAction() = default;

InterpolatedAction::InterpolatedAction(float duration, InterpolationCallback callback)
    : m_duration(duration)
    , m_callback(std::move(callback)) {
}

InterpolatedAction::~InterpolatedAction() = default;

float InterpolatedAction::ease_value(float t) const {
    switch (m_ease_type) {
        case EaseType::Linear:
            return t;

        case EaseType::EaseIn:
            return t * t;

        case EaseType::EaseOut:
            return t * (2.0f - t);

        case EaseType::EaseInOut:
            return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;

        case EaseType::Bounce: {
            if (t < 1.0f / 2.75f) {
                return 7.5625f * t * t;
            } else if (t < 2.0f / 2.75f) {
                t -= 1.5f / 2.75f;
                return 7.5625f * t * t + 0.75f;
            } else if (t < 2.5f / 2.75f) {
                t -= 2.25f / 2.75f;
                return 7.5625f * t * t + 0.9375f;
            } else {
                t -= 2.625f / 2.75f;
                return 7.5625f * t * t + 0.984375f;
            }
        }

        case EaseType::Elastic: {
            if (t == 0 || t == 1) return t;
            float p = 0.3f;
            return std::pow(2.0f, -10.0f * t) * std::sin((t - p / 4.0f) * (2.0f * 3.14159f) / p) + 1.0f;
        }
    }

    return t;
}

ActionResult InterpolatedAction::execute(const TriggerEvent& /*event*/, float dt) {
    m_elapsed += dt;
    float progress = std::min(m_elapsed / m_duration, 1.0f);
    float eased = ease_value(progress);

    if (m_callback) {
        m_callback(eased);
    }

    return progress >= 1.0f ? ActionResult::Success : ActionResult::Running;
}

void InterpolatedAction::reset() {
    m_elapsed = 0;
}

std::string InterpolatedAction::description() const {
    return "Interpolate(" + std::to_string(m_duration) + "s)";
}

std::unique_ptr<IAction> InterpolatedAction::clone() const {
    auto result = std::make_unique<InterpolatedAction>(m_duration, m_callback);
    result->set_ease_type(m_ease_type);
    return result;
}

// =============================================================================
// ActionBuilder Implementation
// =============================================================================

std::unique_ptr<CallbackAction> ActionBuilder::callback(ActionCallback cb) {
    return std::make_unique<CallbackAction>(std::move(cb));
}

std::unique_ptr<DelayedAction> ActionBuilder::delay(std::unique_ptr<IAction> action, float seconds) {
    return std::make_unique<DelayedAction>(std::move(action), seconds);
}

std::unique_ptr<SpawnAction> ActionBuilder::spawn(const std::string& prefab) {
    return std::make_unique<SpawnAction>(prefab, Vec3{});
}

std::unique_ptr<DestroyAction> ActionBuilder::destroy() {
    return std::make_unique<DestroyAction>();
}

std::unique_ptr<TeleportAction> ActionBuilder::teleport(const Vec3& destination) {
    return std::make_unique<TeleportAction>(destination);
}

std::unique_ptr<SetVariableAction> ActionBuilder::set_var(const std::string& name, const VariableValue& value) {
    return std::make_unique<SetVariableAction>(name, value);
}

std::unique_ptr<SendEventAction> ActionBuilder::send_event(const std::string& event_name) {
    return std::make_unique<SendEventAction>(event_name);
}

std::unique_ptr<PlayAudioAction> ActionBuilder::play_audio(const std::string& path) {
    return std::make_unique<PlayAudioAction>(path);
}

std::unique_ptr<PlayEffectAction> ActionBuilder::play_effect(const std::string& path) {
    return std::make_unique<PlayEffectAction>(path);
}

std::unique_ptr<EnableTriggerAction> ActionBuilder::enable_trigger(TriggerId trigger, bool enable) {
    return std::make_unique<EnableTriggerAction>(trigger, enable);
}

std::unique_ptr<ActionSequence> ActionBuilder::sequence() {
    return std::make_unique<ActionSequence>();
}

std::unique_ptr<InterpolatedAction> ActionBuilder::interpolate(float duration,
                                                               InterpolatedAction::InterpolationCallback cb) {
    return std::make_unique<InterpolatedAction>(duration, std::move(cb));
}

} // namespace void_triggers
