/// @file health.cpp
/// @brief Health system implementation for void_combat module

#include <void_engine/combat/health.hpp>

#include <algorithm>
#include <cmath>

namespace void_combat {

// =============================================================================
// HealthComponent Implementation
// =============================================================================

HealthComponent::HealthComponent() = default;

HealthComponent::HealthComponent(const HealthConfig& config)
    : m_max_health(config.max_health)
    , m_current_health(config.current_health)
    , m_health_regen(config.health_regen)
    , m_regen_delay(config.regen_delay)
    , m_can_die(config.can_die)
    , m_invulnerable(config.invulnerable) {
    m_alive = m_current_health > 0;
}

float HealthComponent::health_percent() const {
    if (m_max_health <= 0) return 0;
    return m_current_health / m_max_health;
}

bool HealthComponent::is_full_health() const {
    return m_current_health >= m_max_health;
}

void HealthComponent::set_health(float health) {
    m_current_health = std::clamp(health, 0.0f, m_max_health);
    if (m_current_health <= 0 && m_alive && m_can_die) {
        m_alive = false;
        if (m_on_death) {
            m_on_death();
        }
    }
}

void HealthComponent::set_max_health(float max_health) {
    float ratio = m_max_health > 0 ? m_current_health / m_max_health : 1.0f;
    m_max_health = std::max(1.0f, max_health);
    m_current_health = m_max_health * ratio;
}

void HealthComponent::heal(float amount) {
    if (!m_alive || amount <= 0) return;

    float before = m_current_health;
    m_current_health = std::min(m_current_health + amount, m_max_health);
    float healed = m_current_health - before;

    if (healed > 0 && m_on_heal) {
        m_on_heal(healed, m_current_health);
    }
}

float HealthComponent::take_damage(float amount) {
    if (!m_alive || m_invulnerable || amount <= 0) return 0;

    float before = m_current_health;
    m_current_health = std::max(0.0f, m_current_health - amount);
    float damage_taken = before - m_current_health;

    // Reset regen timer
    m_regen_timer = 0;

    if (m_on_damage) {
        m_on_damage(damage_taken, m_current_health);
    }

    if (m_current_health <= 0 && m_can_die) {
        m_alive = false;
        if (m_on_death) {
            m_on_death();
        }
    }

    return damage_taken;
}

void HealthComponent::update(float dt) {
    if (!m_alive || m_health_regen <= 0) return;

    m_regen_timer += dt;
    if (m_regen_timer >= m_regen_delay && m_current_health < m_max_health) {
        float regen_amount = m_health_regen * dt;
        heal(regen_amount);
    }
}

void HealthComponent::revive(float health_percent) {
    m_alive = true;
    m_current_health = m_max_health * std::clamp(health_percent, 0.0f, 1.0f);
    m_regen_timer = 0;
}

void HealthComponent::kill() {
    if (!m_can_die) return;

    m_current_health = 0;
    m_alive = false;
    if (m_on_death) {
        m_on_death();
    }
}

// =============================================================================
// ShieldComponent Implementation
// =============================================================================

ShieldComponent::ShieldComponent() = default;

ShieldComponent::ShieldComponent(const ShieldConfig& config)
    : m_max_shield(config.max_shield)
    , m_current_shield(config.current_shield)
    , m_shield_regen(config.shield_regen)
    , m_regen_delay(config.regen_delay)
    , m_damage_ratio(config.damage_ratio)
    , m_blocks_all_damage(config.blocks_all_damage) {
}

float ShieldComponent::shield_percent() const {
    if (m_max_shield <= 0) return 0;
    return m_current_shield / m_max_shield;
}

void ShieldComponent::set_shield(float shield) {
    bool was_full = m_current_shield >= m_max_shield;
    m_current_shield = std::clamp(shield, 0.0f, m_max_shield);

    if (m_current_shield <= 0 && was_full && m_on_break) {
        m_on_break();
    }
}

void ShieldComponent::set_max_shield(float max_shield) {
    float ratio = m_max_shield > 0 ? m_current_shield / m_max_shield : 1.0f;
    m_max_shield = std::max(0.0f, max_shield);
    m_current_shield = m_max_shield * ratio;
}

void ShieldComponent::recharge(float amount) {
    if (amount <= 0) return;
    m_current_shield = std::min(m_current_shield + amount, m_max_shield);
}

float ShieldComponent::absorb_damage(float damage) {
    if (m_current_shield <= 0 || damage <= 0) {
        return damage;  // All damage passes through
    }

    // Calculate how much shield absorbs
    float absorbed = damage * m_damage_ratio;
    float shield_damage = std::min(absorbed, m_current_shield);

    bool was_positive = m_current_shield > 0;
    m_current_shield -= shield_damage;

    // Reset regen timer
    m_regen_timer = 0;
    m_recharging = false;

    if (m_on_damage) {
        m_on_damage(shield_damage, m_current_shield);
    }

    if (m_current_shield <= 0 && was_positive && m_on_break) {
        m_on_break();
    }

    // Calculate remaining damage
    if (m_blocks_all_damage && shield_damage < absorbed) {
        // Shield blocked but didn't absorb all - remainder goes through
        float remaining = (absorbed - shield_damage) / m_damage_ratio;
        return remaining + (damage - absorbed) / m_damage_ratio;
    }

    return damage - shield_damage;
}

void ShieldComponent::update(float dt) {
    if (m_max_shield <= 0 || m_shield_regen <= 0) return;

    m_regen_timer += dt;
    if (m_regen_timer >= m_regen_delay) {
        if (!m_recharging && m_current_shield < m_max_shield) {
            m_recharging = true;
            if (m_on_recharge_start) {
                m_on_recharge_start();
            }
        }

        if (m_recharging && m_current_shield < m_max_shield) {
            float regen_amount = m_shield_regen * dt;
            m_current_shield = std::min(m_current_shield + regen_amount, m_max_shield);
        }
    }
}

// =============================================================================
// ArmorComponent Implementation
// =============================================================================

ArmorComponent::ArmorComponent() = default;

ArmorComponent::ArmorComponent(const ArmorConfig& config)
    : m_armor(config.armor_value)
    , m_damage_reduction(config.damage_reduction)
    , m_resistances(config.resistances) {
}

void ArmorComponent::set_resistance(DamageTypeId type, float resistance) {
    m_resistances[type] = std::clamp(resistance, -1.0f, 1.0f);  // -1 = vulnerable, 1 = immune
}

float ArmorComponent::resistance(DamageTypeId type) const {
    auto it = m_resistances.find(type);
    return it != m_resistances.end() ? it->second : 0.0f;
}

void ArmorComponent::clear_resistances() {
    m_resistances.clear();
}

float ArmorComponent::apply_armor(float incoming_damage, DamageTypeId damage_type, float armor_penetration) const {
    if (incoming_damage <= 0) return 0;

    float damage = incoming_damage;

    // Apply type resistance first
    float res = resistance(damage_type);
    damage *= (1.0f - res);

    // Apply percentage reduction
    damage *= (1.0f - m_damage_reduction);

    // Apply flat armor (reduced by penetration)
    float effective_armor = std::max(0.0f, m_armor - armor_penetration);
    damage = std::max(0.0f, damage - effective_armor);

    return damage;
}

float ArmorComponent::damage_mitigated(float incoming_damage, DamageTypeId damage_type, float armor_penetration) const {
    float final_damage = apply_armor(incoming_damage, damage_type, armor_penetration);
    return incoming_damage - final_damage;
}

// =============================================================================
// VitalsComponent Implementation
// =============================================================================

VitalsComponent::VitalsComponent() = default;

VitalsComponent::VitalsComponent(const HealthConfig& health, const ShieldConfig& shield, const ArmorConfig& armor)
    : m_health(health)
    , m_shield(shield)
    , m_armor(armor) {
}

float VitalsComponent::effective_health() const {
    return m_health.health() + m_shield.shield();
}

DamageResult VitalsComponent::apply_damage(const DamageInfo& info) {
    DamageResult result;
    result.health_before = m_health.health();

    float damage = info.base_damage;

    // Apply armor (unless ignored)
    if (!has_flag(info.flags, DamageFlags::IgnoreArmor)) {
        float mitigated = m_armor.damage_mitigated(damage, info.damage_type, 0);
        result.damage_mitigated = mitigated;
        damage = m_armor.apply_armor(damage, info.damage_type, 0);
    }

    // Apply shield (unless ignored)
    if (!has_flag(info.flags, DamageFlags::IgnoreShield) && m_shield.shield() > 0) {
        float before_shield = damage;
        damage = m_shield.absorb_damage(damage);
        result.damage_absorbed_shield = before_shield - damage;
    }

    // Apply to health
    result.damage_dealt = m_health.take_damage(damage);
    result.health_after = m_health.health();
    result.final_damage = result.damage_dealt;
    result.was_critical = has_flag(info.flags, DamageFlags::Critical);
    result.was_headshot = has_flag(info.flags, DamageFlags::Headshot);
    result.was_fatal = !m_health.is_alive();

    if (result.was_fatal && result.health_before > 0) {
        result.was_overkill = true;
        result.overkill_damage = damage - result.damage_dealt;
    }

    if (m_on_damage) {
        m_on_damage(result);
    }

    return result;
}

void VitalsComponent::heal(float amount, bool heal_shield) {
    m_health.heal(amount);
    if (heal_shield && m_health.is_full_health()) {
        float overflow = amount - (m_health.max_health() - m_health.health());
        if (overflow > 0) {
            m_shield.recharge(overflow);
        }
    }
}

void VitalsComponent::update(float dt) {
    m_health.update(dt);
    m_shield.update(dt);
}

} // namespace void_combat
