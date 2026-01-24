/// @file triggers.cpp
/// @brief Main trigger system implementation for void_triggers module

#include <void_engine/triggers/triggers.hpp>

#include <algorithm>
#include <cmath>

namespace void_triggers {

// =============================================================================
// Trigger Implementation
// =============================================================================

Trigger::Trigger() = default;

Trigger::Trigger(const TriggerConfig& config)
    : m_config(config) {
}

Trigger::~Trigger() = default;

Trigger::Trigger(Trigger&&) noexcept = default;
Trigger& Trigger::operator=(Trigger&&) noexcept = default;

bool Trigger::can_activate() const {
    if (!m_enabled || m_state == TriggerState::Disabled) {
        return false;
    }

    if (m_state == TriggerState::Cooldown && m_cooldown_remaining > 0) {
        return false;
    }

    if (has_flag(m_config.flags, TriggerFlags::OneShot) && m_activation_count > 0) {
        return false;
    }

    if (m_config.max_activations > 0 && m_activation_count >= m_config.max_activations) {
        return false;
    }

    return true;
}

void Trigger::set_volume(std::unique_ptr<ITriggerVolume> volume) {
    m_volume = std::move(volume);
}

void Trigger::set_condition(std::unique_ptr<ICondition> condition) {
    m_condition = std::move(condition);
}

void Trigger::add_condition(std::unique_ptr<ICondition> condition) {
    if (!m_condition) {
        m_condition = std::make_unique<ConditionGroup>(LogicalOp::And);
    }

    if (auto* group = dynamic_cast<ConditionGroup*>(m_condition.get())) {
        group->add(std::move(condition));
    }
}

bool Trigger::check_conditions(const TriggerEvent& event) const {
    if (!m_condition) {
        return true;
    }
    return m_condition->evaluate(event);
}

void Trigger::set_action(std::unique_ptr<IAction> action) {
    m_action = std::move(action);
}

void Trigger::add_action(std::unique_ptr<IAction> action) {
    if (!m_action) {
        m_action = std::make_unique<ActionSequence>();
    }

    if (auto* sequence = dynamic_cast<ActionSequence*>(m_action.get())) {
        sequence->add(std::move(action));
    }
}

bool Trigger::try_activate(const TriggerEvent& event) {
    if (!can_activate()) {
        return false;
    }

    if (!check_conditions(event)) {
        return false;
    }

    // Delay handling
    if (m_config.delay > 0 && !m_action_pending) {
        m_delay_remaining = m_config.delay;
        m_action_pending = true;
        m_state = TriggerState::Active;
        return true;
    }

    execute_action(event);
    return true;
}

void Trigger::update(float dt, const TriggerEvent& event) {
    // Handle cooldown
    if (m_state == TriggerState::Cooldown) {
        m_cooldown_remaining -= dt;
        if (m_cooldown_remaining <= 0) {
            m_cooldown_remaining = 0;
            m_state = TriggerState::Inactive;
        }
    }

    // Handle delayed action
    if (m_action_pending) {
        m_delay_remaining -= dt;
        if (m_delay_remaining <= 0) {
            m_action_pending = false;
            execute_action(event);
        }
    }

    // Handle continuous triggers
    if (m_config.type == TriggerType::Stay && m_state == TriggerState::Active) {
        if (m_on_stay) {
            m_on_stay(event);
        }

        // Execute continuous action
        if (m_action && !m_action->is_complete()) {
            m_action->execute(event, dt);
        }
    }

    // Handle continuous action execution
    if (m_action && m_action->mode() == ActionMode::Continuous) {
        if (!m_action->is_complete()) {
            m_action->execute(event, dt);
        }
    }
}

void Trigger::reset() {
    m_state = TriggerState::Inactive;
    m_activation_count = 0;
    m_cooldown_remaining = 0;
    m_delay_remaining = 0;
    m_action_pending = false;
    m_entities_inside.clear();

    if (m_action) {
        m_action->reset();
    }
}

void Trigger::add_entity(EntityId entity) {
    m_entities_inside.insert(entity);
}

void Trigger::remove_entity(EntityId entity) {
    m_entities_inside.erase(entity);
}

bool Trigger::has_entity(EntityId entity) const {
    return m_entities_inside.find(entity) != m_entities_inside.end();
}

void Trigger::execute_action(const TriggerEvent& event) {
    m_activation_count++;
    m_last_activation = event.timestamp;
    m_state = TriggerState::Triggered;

    if (m_on_activate) {
        m_on_activate(event);
    }

    if (m_action) {
        m_action->execute(event, 0);
    }

    start_cooldown();
}

void Trigger::start_cooldown() {
    if (m_config.cooldown > 0) {
        m_cooldown_remaining = m_config.cooldown;
        m_state = TriggerState::Cooldown;
    } else {
        m_state = TriggerState::Inactive;
    }
}

// =============================================================================
// TriggerZone Implementation
// =============================================================================

TriggerZone::TriggerZone() = default;

TriggerZone::TriggerZone(const ZoneConfig& config)
    : m_config(config) {
    m_volume = VolumeFactory::create_from_config(config);
}

TriggerZone::~TriggerZone() = default;

void TriggerZone::set_config(const ZoneConfig& config) {
    m_config = config;
    m_volume = VolumeFactory::create_from_config(config);
}

void TriggerZone::set_volume(std::unique_ptr<ITriggerVolume> volume) {
    m_volume = std::move(volume);
}

void TriggerZone::set_position(const Vec3& pos) {
    m_config.position = pos;
    if (m_volume) {
        m_volume->set_center(pos);
    }
}

bool TriggerZone::contains(const Vec3& point) const {
    if (!m_config.enabled || !m_volume) {
        return false;
    }
    return m_volume->contains(point);
}

bool TriggerZone::contains_entity(EntityId entity, EntityPositionCallback pos_getter) const {
    if (!pos_getter) {
        return false;
    }
    Vec3 pos = pos_getter(entity);
    return contains(pos);
}

void TriggerZone::add_trigger(TriggerId trigger) {
    m_triggers.push_back(trigger);
}

void TriggerZone::remove_trigger(TriggerId trigger) {
    m_triggers.erase(
        std::remove(m_triggers.begin(), m_triggers.end(), trigger),
        m_triggers.end()
    );
}

// =============================================================================
// TriggerSystem Implementation
// =============================================================================

TriggerSystem::TriggerSystem()
    : TriggerSystem(TriggerSystemConfig{}) {
}

TriggerSystem::TriggerSystem(const TriggerSystemConfig& config)
    : m_config(config) {
}

TriggerSystem::~TriggerSystem() = default;

void TriggerSystem::set_config(const TriggerSystemConfig& config) {
    m_config = config;
}

TriggerId TriggerSystem::create_trigger(const TriggerConfig& config) {
    TriggerId id{m_next_trigger_id++};

    auto trigger = std::make_unique<Trigger>(config);
    trigger->set_id(id);

    if (!config.name.empty()) {
        m_trigger_names[config.name] = id;
    }

    m_triggers[id] = std::move(trigger);
    m_stats.total_triggers = m_triggers.size();

    return id;
}

Trigger* TriggerSystem::get_trigger(TriggerId id) {
    auto it = m_triggers.find(id);
    return it != m_triggers.end() ? it->second.get() : nullptr;
}

const Trigger* TriggerSystem::get_trigger(TriggerId id) const {
    auto it = m_triggers.find(id);
    return it != m_triggers.end() ? it->second.get() : nullptr;
}

bool TriggerSystem::remove_trigger(TriggerId id) {
    auto it = m_triggers.find(id);
    if (it == m_triggers.end()) {
        return false;
    }

    // Remove from name lookup
    if (!it->second->name().empty()) {
        m_trigger_names.erase(it->second->name());
    }

    // Remove from entity tracking
    for (auto& [entity, triggers] : m_entity_triggers) {
        triggers.erase(id);
    }

    m_triggers.erase(it);
    m_stats.total_triggers = m_triggers.size();
    return true;
}

std::vector<TriggerId> TriggerSystem::all_triggers() const {
    std::vector<TriggerId> result;
    result.reserve(m_triggers.size());
    for (const auto& [id, trigger] : m_triggers) {
        result.push_back(id);
    }
    return result;
}

ZoneId TriggerSystem::create_zone(const ZoneConfig& config) {
    ZoneId id{m_next_zone_id++};

    auto zone = std::make_unique<TriggerZone>(config);
    zone->set_id(id);

    if (!config.name.empty()) {
        m_zone_names[config.name] = id;
    }

    m_zones[id] = std::move(zone);
    m_stats.total_zones = m_zones.size();

    return id;
}

TriggerZone* TriggerSystem::get_zone(ZoneId id) {
    auto it = m_zones.find(id);
    return it != m_zones.end() ? it->second.get() : nullptr;
}

const TriggerZone* TriggerSystem::get_zone(ZoneId id) const {
    auto it = m_zones.find(id);
    return it != m_zones.end() ? it->second.get() : nullptr;
}

bool TriggerSystem::remove_zone(ZoneId id) {
    auto it = m_zones.find(id);
    if (it == m_zones.end()) {
        return false;
    }

    if (!it->second->name().empty()) {
        m_zone_names.erase(it->second->name());
    }

    m_zones.erase(it);
    m_stats.total_zones = m_zones.size();
    return true;
}

std::vector<ZoneId> TriggerSystem::all_zones() const {
    std::vector<ZoneId> result;
    result.reserve(m_zones.size());
    for (const auto& [id, zone] : m_zones) {
        result.push_back(id);
    }
    return result;
}

Trigger* TriggerSystem::find_trigger(const std::string& name) {
    auto it = m_trigger_names.find(name);
    if (it != m_trigger_names.end()) {
        return get_trigger(it->second);
    }
    return nullptr;
}

TriggerZone* TriggerSystem::find_zone(const std::string& name) {
    auto it = m_zone_names.find(name);
    if (it != m_zone_names.end()) {
        return get_zone(it->second);
    }
    return nullptr;
}

void TriggerSystem::update_entity(EntityId entity, const Vec3& position) {
    Vec3 old_position = m_entity_positions[entity];
    m_entity_positions[entity] = position;

    auto& entity_triggers = m_entity_triggers[entity];

    // Check all triggers
    for (auto& [id, trigger] : m_triggers) {
        if (!trigger->is_enabled()) {
            continue;
        }

        ITriggerVolume* volume = trigger->volume();
        if (!volume) {
            continue;
        }

        m_stats.collision_checks++;

        bool was_inside = entity_triggers.find(id) != entity_triggers.end();
        bool is_inside = volume->contains(position);

        if (!check_entity_filter(entity, *trigger)) {
            continue;
        }

        if (is_inside && !was_inside) {
            // Enter
            entity_triggers.insert(id);
            process_entity_enter(entity, *trigger);
        } else if (!is_inside && was_inside) {
            // Exit
            entity_triggers.erase(id);
            process_entity_exit(entity, *trigger);
        }
    }

    m_stats.entities_tracked = m_entity_positions.size();
}

void TriggerSystem::remove_entity(EntityId entity) {
    // Process exits for all triggers
    auto it = m_entity_triggers.find(entity);
    if (it != m_entity_triggers.end()) {
        for (auto trigger_id : it->second) {
            if (auto* trigger = get_trigger(trigger_id)) {
                process_entity_exit(entity, *trigger);
            }
        }
        m_entity_triggers.erase(it);
    }

    m_entity_positions.erase(entity);
    m_stats.entities_tracked = m_entity_positions.size();
}

std::vector<EntityId> TriggerSystem::entities_in_trigger(TriggerId trigger) const {
    std::vector<EntityId> result;
    if (const auto* t = get_trigger(trigger)) {
        const auto& inside = t->entities_inside();
        result.reserve(inside.size());
        for (auto entity : inside) {
            result.push_back(entity);
        }
    }
    return result;
}

std::vector<TriggerId> TriggerSystem::triggers_containing(EntityId entity) const {
    std::vector<TriggerId> result;
    auto it = m_entity_triggers.find(entity);
    if (it != m_entity_triggers.end()) {
        result.reserve(it->second.size());
        for (auto id : it->second) {
            result.push_back(id);
        }
    }
    return result;
}

bool TriggerSystem::fire_trigger(TriggerId trigger, const TriggerEvent& event) {
    Trigger* t = get_trigger(trigger);
    if (!t) {
        return false;
    }

    return t->try_activate(event);
}

void TriggerSystem::send_event(const std::string& event_type, EntityId entity, const Vec3& position) {
    TriggerEvent event;
    event.id = TriggerEventId{m_next_event_id++};
    event.type = TriggerEventType::Custom;
    event.custom_type = event_type;
    event.entity = entity;
    event.position = position;
    event.timestamp = m_current_time;

    for (auto& [id, trigger] : m_triggers) {
        if (trigger->config().type == TriggerType::Event) {
            TriggerEvent trigger_event = event;
            trigger_event.trigger = id;
            trigger->try_activate(trigger_event);
        }
    }
}

void TriggerSystem::update(float dt) {
    m_current_time += dt;

    // Update all triggers
    for (auto& [id, trigger] : m_triggers) {
        if (!trigger->is_enabled()) {
            continue;
        }

        // Process stay events
        if (trigger->config().type == TriggerType::Stay) {
            for (auto entity : trigger->entities_inside()) {
                Vec3 pos = m_entity_positions[entity];
                TriggerEvent event = create_event(TriggerEventType::Activate, id, entity, pos);
                trigger->update(dt, event);
            }
        } else {
            TriggerEvent event;
            event.trigger = id;
            event.timestamp = m_current_time;
            trigger->update(dt, event);
        }

        // Handle timed triggers
        if (trigger->config().type == TriggerType::Timed) {
            TriggerEvent event = create_event(TriggerEventType::Timer, id, EntityId{}, Vec3{});
            trigger->try_activate(event);
        }
    }
}

void TriggerSystem::process_entity_enter(EntityId entity, Trigger& trigger) {
    trigger.add_entity(entity);

    Vec3 pos = m_entity_positions[entity];
    TriggerEvent event = create_event(TriggerEventType::Enter, trigger.id(), entity, pos);

    if (trigger.config().type == TriggerType::Enter ||
        trigger.config().type == TriggerType::EnterExit) {
        trigger.try_activate(event);
        m_stats.total_activations++;
    }

    if (m_on_trigger_enter) {
        m_on_trigger_enter(event);
    }

    trigger.invoke_on_enter(event);
}

void TriggerSystem::process_entity_exit(EntityId entity, Trigger& trigger) {
    trigger.remove_entity(entity);

    Vec3 pos = m_entity_positions[entity];
    TriggerEvent event = create_event(TriggerEventType::Exit, trigger.id(), entity, pos);

    if (trigger.config().type == TriggerType::Exit ||
        trigger.config().type == TriggerType::EnterExit) {
        trigger.try_activate(event);
        m_stats.total_activations++;
    }

    if (m_on_trigger_exit) {
        m_on_trigger_exit(event);
    }

    trigger.invoke_on_exit(event);
}

void TriggerSystem::process_entity_stay(EntityId entity, Trigger& trigger, float dt) {
    Vec3 pos = m_entity_positions[entity];
    TriggerEvent event = create_event(TriggerEventType::Activate, trigger.id(), entity, pos);

    trigger.update(dt, event);
}

bool TriggerSystem::check_entity_filter(EntityId entity, const Trigger& trigger) const {
    const auto& config = trigger.config();

    // Check player flags using the player check callback
    if (m_is_player) {
        bool is_player = m_is_player(entity);

        if (has_flag(config.flags, TriggerFlags::PlayerOnly)) {
            if (!is_player) return false;
        }

        if (has_flag(config.flags, TriggerFlags::IgnorePlayer)) {
            if (is_player) return false;
        }
    }

    // Check tags
    if (!config.required_tags.empty() && m_tags_getter) {
        std::vector<std::string> tags = m_tags_getter(entity);

        if (has_flag(config.flags, TriggerFlags::RequireAllTags)) {
            for (const auto& req_tag : config.required_tags) {
                bool found = std::find(tags.begin(), tags.end(), req_tag) != tags.end();
                if (!found) return false;
            }
        } else if (has_flag(config.flags, TriggerFlags::RequireAnyTag)) {
            bool found_any = false;
            for (const auto& req_tag : config.required_tags) {
                bool found = std::find(tags.begin(), tags.end(), req_tag) != tags.end();
                if (found) {
                    found_any = true;
                    break;
                }
            }
            if (!found_any) return false;
        }
    }

    // Check excluded tags
    if (!config.excluded_tags.empty() && m_tags_getter) {
        std::vector<std::string> tags = m_tags_getter(entity);
        for (const auto& excl_tag : config.excluded_tags) {
            bool found = std::find(tags.begin(), tags.end(), excl_tag) != tags.end();
            if (found) return false;
        }
    }

    return true;
}

TriggerEvent TriggerSystem::create_event(TriggerEventType type, TriggerId trigger,
                                          EntityId entity, const Vec3& position) {
    TriggerEvent event;
    event.id = TriggerEventId{m_next_event_id++};
    event.type = type;
    event.trigger = trigger;
    event.entity = entity;
    event.position = position;
    event.timestamp = m_current_time;
    return event;
}

TriggerSystem::Snapshot TriggerSystem::take_snapshot() const {
    Snapshot snapshot;
    snapshot.current_time = m_current_time;

    for (const auto& [id, trigger] : m_triggers) {
        Snapshot::TriggerData data;
        data.id = id.value;
        data.name = trigger->name();
        data.state = static_cast<std::uint8_t>(trigger->state());
        data.activation_count = trigger->activation_count();
        data.last_activation = trigger->last_activation_time();
        data.cooldown_remaining = trigger->cooldown_remaining();
        data.enabled = trigger->is_enabled();
        snapshot.triggers.push_back(data);
    }

    return snapshot;
}

void TriggerSystem::apply_snapshot(const Snapshot& snapshot) {
    m_current_time = snapshot.current_time;

    for (const auto& data : snapshot.triggers) {
        TriggerId id{data.id};
        if (auto* trigger = get_trigger(id)) {
            // Restore enabled state
            if (data.enabled) {
                trigger->enable();
            } else {
                trigger->disable();
            }

            // Restore full trigger state
            trigger->set_state(static_cast<TriggerState>(data.state));
            trigger->set_activation_count(data.activation_count);
            trigger->set_last_activation(data.last_activation);
            trigger->set_cooldown_remaining(data.cooldown_remaining);
        }
    }
}

void TriggerSystem::clear() {
    m_triggers.clear();
    m_zones.clear();
    m_trigger_names.clear();
    m_zone_names.clear();
    m_entity_positions.clear();
    m_entity_triggers.clear();
    m_spatial_grid.clear();
    m_stats = Stats{};
}

} // namespace void_triggers
