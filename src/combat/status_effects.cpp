/// @file status_effects.cpp
/// @brief Status effect system implementation for void_combat module

#include <void_engine/combat/status_effects.hpp>

#include <algorithm>

namespace void_combat {

// =============================================================================
// StatusEffect Implementation
// =============================================================================

StatusEffect::StatusEffect() = default;

StatusEffect::StatusEffect(const StatusEffectConfig& config)
    : m_config(config) {
}

void StatusEffect::on_apply(EntityId target) {
    if (m_on_apply) {
        m_on_apply(target);
    }
}

void StatusEffect::on_remove(EntityId target) {
    if (m_on_remove) {
        m_on_remove(target);
    }
}

void StatusEffect::on_tick(EntityId target, float /*dt*/) {
    if (m_config.damage_per_tick > 0 && m_on_tick_callback) {
        float damage = m_config.damage_per_tick * static_cast<float>(m_current_stacks);
        m_on_tick_callback(target, damage);
    }
}

void StatusEffect::on_stack(EntityId /*target*/, std::uint32_t new_stacks) {
    m_current_stacks = new_stacks;
}

float StatusEffect::damage_modifier() const {
    return m_config.damage_modifier * static_cast<float>(m_current_stacks);
}

float StatusEffect::speed_modifier() const {
    return m_config.speed_modifier * static_cast<float>(m_current_stacks);
}

float StatusEffect::defense_modifier() const {
    return m_config.defense_modifier * static_cast<float>(m_current_stacks);
}

float StatusEffect::attack_speed_modifier() const {
    return m_config.attack_speed_modifier * static_cast<float>(m_current_stacks);
}

// =============================================================================
// StatusEffectRegistry Implementation
// =============================================================================

StatusEffectRegistry::StatusEffectRegistry() = default;
StatusEffectRegistry::~StatusEffectRegistry() = default;

StatusEffectId StatusEffectRegistry::register_effect(const StatusEffectConfig& config) {
    StatusEffectId id{m_next_id++};
    m_configs[id] = config;
    if (!config.name.empty()) {
        m_name_lookup[config.name] = id;
    }
    return id;
}

const StatusEffectConfig* StatusEffectRegistry::get_config(StatusEffectId id) const {
    auto it = m_configs.find(id);
    return it != m_configs.end() ? &it->second : nullptr;
}

StatusEffectId StatusEffectRegistry::find_effect(std::string_view name) const {
    auto it = m_name_lookup.find(std::string(name));
    return it != m_name_lookup.end() ? it->second : StatusEffectId{};
}

std::vector<StatusEffectId> StatusEffectRegistry::all_effects() const {
    std::vector<StatusEffectId> result;
    result.reserve(m_configs.size());
    for (const auto& [id, config] : m_configs) {
        result.push_back(id);
    }
    return result;
}

StatusEffectConfig StatusEffectRegistry::preset_burning() {
    StatusEffectConfig config;
    config.name = "Burning";
    config.type = StatusEffectType::Debuff;
    config.stacking = StackBehavior::Intensity;
    config.max_stacks = 5;
    config.duration = 5.0f;
    config.tick_interval = 1.0f;
    config.damage_per_tick = 5.0f;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_poison() {
    StatusEffectConfig config;
    config.name = "Poison";
    config.type = StatusEffectType::Debuff;
    config.stacking = StackBehavior::Both;
    config.max_stacks = 3;
    config.duration = 8.0f;
    config.tick_interval = 2.0f;
    config.damage_per_tick = 8.0f;
    config.speed_modifier = -0.1f;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_frozen() {
    StatusEffectConfig config;
    config.name = "Frozen";
    config.type = StatusEffectType::Debuff;
    config.stacking = StackBehavior::Duration;
    config.max_stacks = 1;
    config.duration = 3.0f;
    config.speed_modifier = -0.5f;
    config.attack_speed_modifier = -0.3f;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_stunned() {
    StatusEffectConfig config;
    config.name = "Stunned";
    config.type = StatusEffectType::Debuff;
    config.stacking = StackBehavior::Duration;
    config.max_stacks = 1;
    config.duration = 2.0f;
    config.stun = true;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_bleeding() {
    StatusEffectConfig config;
    config.name = "Bleeding";
    config.type = StatusEffectType::Debuff;
    config.stacking = StackBehavior::Intensity;
    config.max_stacks = 10;
    config.duration = 6.0f;
    config.tick_interval = 0.5f;
    config.damage_per_tick = 3.0f;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_regeneration() {
    StatusEffectConfig config;
    config.name = "Regeneration";
    config.type = StatusEffectType::Buff;
    config.stacking = StackBehavior::Intensity;
    config.max_stacks = 3;
    config.duration = 10.0f;
    config.tick_interval = 1.0f;
    config.damage_per_tick = -10.0f;  // Negative = healing
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_haste() {
    StatusEffectConfig config;
    config.name = "Haste";
    config.type = StatusEffectType::Buff;
    config.stacking = StackBehavior::Duration;
    config.max_stacks = 1;
    config.duration = 8.0f;
    config.speed_modifier = 0.3f;
    config.attack_speed_modifier = 0.2f;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_slow() {
    StatusEffectConfig config;
    config.name = "Slow";
    config.type = StatusEffectType::Debuff;
    config.stacking = StackBehavior::Intensity;
    config.max_stacks = 3;
    config.duration = 5.0f;
    config.speed_modifier = -0.2f;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_weakness() {
    StatusEffectConfig config;
    config.name = "Weakness";
    config.type = StatusEffectType::Debuff;
    config.stacking = StackBehavior::Duration;
    config.max_stacks = 1;
    config.duration = 6.0f;
    config.damage_modifier = -0.25f;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_strength() {
    StatusEffectConfig config;
    config.name = "Strength";
    config.type = StatusEffectType::Buff;
    config.stacking = StackBehavior::Duration;
    config.max_stacks = 1;
    config.duration = 10.0f;
    config.damage_modifier = 0.25f;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_shield() {
    StatusEffectConfig config;
    config.name = "Shield";
    config.type = StatusEffectType::Buff;
    config.stacking = StackBehavior::Duration;
    config.max_stacks = 1;
    config.duration = 8.0f;
    config.defense_modifier = 0.3f;
    return config;
}

StatusEffectConfig StatusEffectRegistry::preset_invulnerable() {
    StatusEffectConfig config;
    config.name = "Invulnerable";
    config.type = StatusEffectType::Buff;
    config.stacking = StackBehavior::Duration;
    config.max_stacks = 1;
    config.duration = 3.0f;
    config.invulnerable = true;
    return config;
}

// =============================================================================
// StatusEffectComponent Implementation
// =============================================================================

StatusEffectComponent::StatusEffectComponent() = default;

StatusEffectComponent::StatusEffectComponent(StatusEffectRegistry* registry)
    : m_registry(registry) {
}

StatusEffectComponent::~StatusEffectComponent() = default;

bool StatusEffectComponent::apply_effect(StatusEffectId effect_id, EntityId source) {
    if (!m_registry) return false;

    // Check immunity
    if (is_immune(effect_id)) return false;

    const StatusEffectConfig* config = m_registry->get_config(effect_id);
    if (!config) return false;

    // Check type immunity
    for (StatusEffectType type : m_type_immunities) {
        if (config->type == type) return false;
    }

    // Check if already has effect
    for (auto& existing : m_effects) {
        if (existing.effect_id == effect_id) {
            // Handle stacking
            stack_effect(existing, *config);
            return true;
        }
    }

    // Add new effect
    StatusEffectInstance instance;
    instance.effect_id = effect_id;
    instance.duration_remaining = config->duration;
    instance.tick_timer = 0;
    instance.stacks = 1;
    instance.source = source;
    m_effects.push_back(instance);

    if (m_on_applied) {
        m_on_applied(effect_id, 1);
    }

    return true;
}

void StatusEffectComponent::remove_effect(StatusEffectId effect_id, bool all_stacks) {
    auto it = std::find_if(m_effects.begin(), m_effects.end(),
        [effect_id](const StatusEffectInstance& e) { return e.effect_id == effect_id; });

    if (it != m_effects.end()) {
        if (all_stacks || it->stacks <= 1) {
            m_effects.erase(it);
            if (m_on_removed) {
                m_on_removed(effect_id);
            }
        } else {
            it->stacks--;
        }
    }
}

void StatusEffectComponent::remove_effects_of_type(StatusEffectType type) {
    if (!m_registry) return;

    auto it = m_effects.begin();
    while (it != m_effects.end()) {
        const StatusEffectConfig* config = m_registry->get_config(it->effect_id);
        if (config && config->type == type) {
            StatusEffectId id = it->effect_id;
            it = m_effects.erase(it);
            if (m_on_removed) {
                m_on_removed(id);
            }
        } else {
            ++it;
        }
    }
}

void StatusEffectComponent::clear_effects() {
    for (const auto& effect : m_effects) {
        if (m_on_removed) {
            m_on_removed(effect.effect_id);
        }
    }
    m_effects.clear();
}

bool StatusEffectComponent::has_effect(StatusEffectId effect_id) const {
    return std::any_of(m_effects.begin(), m_effects.end(),
        [effect_id](const StatusEffectInstance& e) { return e.effect_id == effect_id; });
}

const StatusEffectInstance* StatusEffectComponent::get_effect(StatusEffectId effect_id) const {
    auto it = std::find_if(m_effects.begin(), m_effects.end(),
        [effect_id](const StatusEffectInstance& e) { return e.effect_id == effect_id; });
    return it != m_effects.end() ? &(*it) : nullptr;
}

float StatusEffectComponent::total_damage_modifier() const {
    if (!m_registry) return 0;

    float total = 0;
    for (const auto& effect : m_effects) {
        const StatusEffectConfig* config = m_registry->get_config(effect.effect_id);
        if (config) {
            total += config->damage_modifier * static_cast<float>(effect.stacks);
        }
    }
    return total;
}

float StatusEffectComponent::total_speed_modifier() const {
    if (!m_registry) return 0;

    float total = 0;
    for (const auto& effect : m_effects) {
        const StatusEffectConfig* config = m_registry->get_config(effect.effect_id);
        if (config) {
            total += config->speed_modifier * static_cast<float>(effect.stacks);
        }
    }
    return total;
}

float StatusEffectComponent::total_defense_modifier() const {
    if (!m_registry) return 0;

    float total = 0;
    for (const auto& effect : m_effects) {
        const StatusEffectConfig* config = m_registry->get_config(effect.effect_id);
        if (config) {
            total += config->defense_modifier * static_cast<float>(effect.stacks);
        }
    }
    return total;
}

float StatusEffectComponent::total_attack_speed_modifier() const {
    if (!m_registry) return 0;

    float total = 0;
    for (const auto& effect : m_effects) {
        const StatusEffectConfig* config = m_registry->get_config(effect.effect_id);
        if (config) {
            total += config->attack_speed_modifier * static_cast<float>(effect.stacks);
        }
    }
    return total;
}

bool StatusEffectComponent::is_rooted() const {
    if (!m_registry) return false;

    for (const auto& effect : m_effects) {
        const StatusEffectConfig* config = m_registry->get_config(effect.effect_id);
        if (config && config->root) return true;
    }
    return false;
}

bool StatusEffectComponent::is_silenced() const {
    if (!m_registry) return false;

    for (const auto& effect : m_effects) {
        const StatusEffectConfig* config = m_registry->get_config(effect.effect_id);
        if (config && config->silence) return true;
    }
    return false;
}

bool StatusEffectComponent::is_disarmed() const {
    if (!m_registry) return false;

    for (const auto& effect : m_effects) {
        const StatusEffectConfig* config = m_registry->get_config(effect.effect_id);
        if (config && config->disarm) return true;
    }
    return false;
}

bool StatusEffectComponent::is_stunned() const {
    if (!m_registry) return false;

    for (const auto& effect : m_effects) {
        const StatusEffectConfig* config = m_registry->get_config(effect.effect_id);
        if (config && config->stun) return true;
    }
    return false;
}

bool StatusEffectComponent::is_invulnerable() const {
    if (!m_registry) return false;

    for (const auto& effect : m_effects) {
        const StatusEffectConfig* config = m_registry->get_config(effect.effect_id);
        if (config && config->invulnerable) return true;
    }
    return false;
}

bool StatusEffectComponent::can_act() const {
    return !is_stunned();
}

bool StatusEffectComponent::can_move() const {
    return !is_rooted() && !is_stunned();
}

bool StatusEffectComponent::can_attack() const {
    return !is_disarmed() && !is_stunned();
}

bool StatusEffectComponent::can_use_abilities() const {
    return !is_silenced() && !is_stunned();
}

void StatusEffectComponent::update(float dt) {
    if (!m_registry) return;

    auto it = m_effects.begin();
    while (it != m_effects.end()) {
        const StatusEffectConfig* config = m_registry->get_config(it->effect_id);
        if (!config) {
            it = m_effects.erase(it);
            continue;
        }

        // Handle ticks
        tick_effect(*it, *config, dt);

        // Update duration
        if (!it->permanent) {
            it->duration_remaining -= dt;
            if (it->duration_remaining <= 0) {
                StatusEffectId id = it->effect_id;
                it = m_effects.erase(it);
                if (m_on_removed) {
                    m_on_removed(id);
                }
                continue;
            }
        }

        ++it;
    }
}

void StatusEffectComponent::add_immunity(StatusEffectId effect_id) {
    if (std::find(m_immunities.begin(), m_immunities.end(), effect_id) == m_immunities.end()) {
        m_immunities.push_back(effect_id);
    }
}

void StatusEffectComponent::remove_immunity(StatusEffectId effect_id) {
    m_immunities.erase(std::remove(m_immunities.begin(), m_immunities.end(), effect_id), m_immunities.end());
}

bool StatusEffectComponent::is_immune(StatusEffectId effect_id) const {
    return std::find(m_immunities.begin(), m_immunities.end(), effect_id) != m_immunities.end();
}

void StatusEffectComponent::add_type_immunity(StatusEffectType type) {
    if (std::find(m_type_immunities.begin(), m_type_immunities.end(), type) == m_type_immunities.end()) {
        m_type_immunities.push_back(type);
    }
}

void StatusEffectComponent::remove_type_immunity(StatusEffectType type) {
    m_type_immunities.erase(std::remove(m_type_immunities.begin(), m_type_immunities.end(), type), m_type_immunities.end());
}

void StatusEffectComponent::stack_effect(StatusEffectInstance& instance, const StatusEffectConfig& config) {
    switch (config.stacking) {
        case StackBehavior::None:
            // Don't stack
            break;

        case StackBehavior::Duration:
            // Refresh duration
            instance.duration_remaining = config.duration;
            break;

        case StackBehavior::Intensity:
            // Add stack up to max
            if (instance.stacks < config.max_stacks) {
                instance.stacks++;
            }
            break;

        case StackBehavior::Both:
            // Refresh duration and add stack
            instance.duration_remaining = config.duration;
            if (instance.stacks < config.max_stacks) {
                instance.stacks++;
            }
            break;

        case StackBehavior::Separate:
            // Handled by adding new instance (not in this function)
            break;
    }

    if (m_on_applied) {
        m_on_applied(instance.effect_id, instance.stacks);
    }
}

void StatusEffectComponent::tick_effect(StatusEffectInstance& instance, const StatusEffectConfig& config, float dt) {
    if (config.tick_interval <= 0) return;

    instance.tick_timer += dt;
    while (instance.tick_timer >= config.tick_interval) {
        instance.tick_timer -= config.tick_interval;

        if (config.damage_per_tick != 0 && m_on_tick) {
            float damage = config.damage_per_tick * static_cast<float>(instance.stacks);
            m_on_tick(instance.effect_id, damage);
        }
    }
}

} // namespace void_combat
