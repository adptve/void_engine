/// @file health.hpp
/// @brief Health, shield, and armor systems for void_combat

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace void_combat {

// =============================================================================
// Health Component Interface
// =============================================================================

/// @brief Interface for health components
class IHealthComponent {
public:
    virtual ~IHealthComponent() = default;

    // Health queries
    virtual float health() const = 0;
    virtual float max_health() const = 0;
    virtual float health_percent() const = 0;
    virtual bool is_alive() const = 0;
    virtual bool is_full_health() const = 0;

    // Health modification
    virtual void set_health(float health) = 0;
    virtual void set_max_health(float max_health) = 0;
    virtual void heal(float amount) = 0;
    virtual float take_damage(float amount) = 0;

    // Regeneration
    virtual void set_health_regen(float per_second) = 0;
    virtual float health_regen() const = 0;

    // State
    virtual void set_invulnerable(bool invulnerable) = 0;
    virtual bool is_invulnerable() const = 0;

    // Update
    virtual void update(float dt) = 0;

    // Callbacks
    using DamageCallback = std::function<void(float damage, float remaining)>;
    using HealCallback = std::function<void(float amount, float remaining)>;
    using DeathCallback = std::function<void()>;

    virtual void on_damage(DamageCallback callback) = 0;
    virtual void on_heal(HealCallback callback) = 0;
    virtual void on_death(DeathCallback callback) = 0;
};

// =============================================================================
// Health Component Implementation
// =============================================================================

/// @brief Standard health component
class HealthComponent : public IHealthComponent {
public:
    HealthComponent();
    explicit HealthComponent(const HealthConfig& config);
    ~HealthComponent() override = default;

    // IHealthComponent interface
    float health() const override { return m_current_health; }
    float max_health() const override { return m_max_health; }
    float health_percent() const override;
    bool is_alive() const override { return m_alive; }
    bool is_full_health() const override;

    void set_health(float health) override;
    void set_max_health(float max_health) override;
    void heal(float amount) override;
    float take_damage(float amount) override;

    void set_health_regen(float per_second) override { m_health_regen = per_second; }
    float health_regen() const override { return m_health_regen; }

    void set_invulnerable(bool invulnerable) override { m_invulnerable = invulnerable; }
    bool is_invulnerable() const override { return m_invulnerable; }

    void update(float dt) override;

    void on_damage(DamageCallback callback) override { m_on_damage = std::move(callback); }
    void on_heal(HealCallback callback) override { m_on_heal = std::move(callback); }
    void on_death(DeathCallback callback) override { m_on_death = std::move(callback); }

    // Additional methods
    void revive(float health_percent = 1.0f);
    void kill();

    void set_regen_delay(float delay) { m_regen_delay = delay; }
    float regen_delay() const { return m_regen_delay; }

    void set_can_die(bool can_die) { m_can_die = can_die; }
    bool can_die() const { return m_can_die; }

private:
    float m_max_health{100.0f};
    float m_current_health{100.0f};
    float m_health_regen{0};
    float m_regen_delay{3.0f};
    float m_regen_timer{0};
    bool m_alive{true};
    bool m_can_die{true};
    bool m_invulnerable{false};

    DamageCallback m_on_damage;
    HealCallback m_on_heal;
    DeathCallback m_on_death;
};

// =============================================================================
// Shield Component
// =============================================================================

/// @brief Shield component for damage absorption
class ShieldComponent {
public:
    ShieldComponent();
    explicit ShieldComponent(const ShieldConfig& config);
    ~ShieldComponent() = default;

    // Shield queries
    float shield() const { return m_current_shield; }
    float max_shield() const { return m_max_shield; }
    float shield_percent() const;
    bool has_shield() const { return m_current_shield > 0; }
    bool is_full_shield() const { return m_current_shield >= m_max_shield; }

    // Shield modification
    void set_shield(float shield);
    void set_max_shield(float max_shield);
    void recharge(float amount);

    /// @brief Absorb damage
    /// @return Damage that passed through shield
    float absorb_damage(float damage);

    // Regeneration
    void set_shield_regen(float per_second) { m_shield_regen = per_second; }
    float shield_regen() const { return m_shield_regen; }

    void set_regen_delay(float delay) { m_regen_delay = delay; }
    float regen_delay() const { return m_regen_delay; }

    // Configuration
    void set_damage_ratio(float ratio) { m_damage_ratio = ratio; }
    float damage_ratio() const { return m_damage_ratio; }

    void set_blocks_all_damage(bool blocks) { m_blocks_all_damage = blocks; }
    bool blocks_all_damage() const { return m_blocks_all_damage; }

    // Update
    void update(float dt);

    // Callbacks
    using ShieldDamageCallback = std::function<void(float damage, float remaining)>;
    using ShieldBreakCallback = std::function<void()>;
    using ShieldRechargeCallback = std::function<void()>;

    void on_damage(ShieldDamageCallback callback) { m_on_damage = std::move(callback); }
    void on_break(ShieldBreakCallback callback) { m_on_break = std::move(callback); }
    void on_recharge_start(ShieldRechargeCallback callback) { m_on_recharge_start = std::move(callback); }

private:
    float m_max_shield{0};
    float m_current_shield{0};
    float m_shield_regen{10.0f};
    float m_regen_delay{2.0f};
    float m_regen_timer{0};
    float m_damage_ratio{1.0f};
    bool m_blocks_all_damage{false};
    bool m_recharging{false};

    ShieldDamageCallback m_on_damage;
    ShieldBreakCallback m_on_break;
    ShieldRechargeCallback m_on_recharge_start;
};

// =============================================================================
// Armor Component
// =============================================================================

/// @brief Armor component for damage reduction
class ArmorComponent {
public:
    ArmorComponent();
    explicit ArmorComponent(const ArmorConfig& config);
    ~ArmorComponent() = default;

    // Armor value (flat reduction)
    void set_armor(float armor) { m_armor = armor; }
    float armor() const { return m_armor; }

    // Damage reduction (percentage)
    void set_damage_reduction(float reduction) { m_damage_reduction = reduction; }
    float damage_reduction() const { return m_damage_reduction; }

    // Resistances
    void set_resistance(DamageTypeId type, float resistance);
    float resistance(DamageTypeId type) const;
    void clear_resistances();

    /// @brief Calculate damage after armor
    /// @param incoming_damage Base damage
    /// @param damage_type Type of damage
    /// @param armor_penetration Attacker's armor pen
    /// @return Final damage after armor
    float apply_armor(float incoming_damage, DamageTypeId damage_type, float armor_penetration = 0) const;

    /// @brief Get damage mitigated by armor
    float damage_mitigated(float incoming_damage, DamageTypeId damage_type, float armor_penetration = 0) const;

private:
    float m_armor{0};
    float m_damage_reduction{0};
    std::unordered_map<DamageTypeId, float> m_resistances;
};

// =============================================================================
// Combined Vitals Component
// =============================================================================

/// @brief Combined health, shield, and armor component
class VitalsComponent {
public:
    VitalsComponent();
    VitalsComponent(const HealthConfig& health, const ShieldConfig& shield, const ArmorConfig& armor);
    ~VitalsComponent() = default;

    // Component access
    HealthComponent& health() { return m_health; }
    const HealthComponent& health() const { return m_health; }

    ShieldComponent& shield() { return m_shield; }
    const ShieldComponent& shield() const { return m_shield; }

    ArmorComponent& armor() { return m_armor; }
    const ArmorComponent& armor() const { return m_armor; }

    // Convenience methods
    bool is_alive() const { return m_health.is_alive(); }
    float effective_health() const; ///< Health + shield

    /// @brief Apply damage through all systems
    /// @return Final damage result
    DamageResult apply_damage(const DamageInfo& info);

    /// @brief Heal through all systems
    void heal(float amount, bool heal_shield = true);

    // Update
    void update(float dt);

    // Combined callbacks
    using VitalsDamageCallback = std::function<void(const DamageResult&)>;
    void on_damage(VitalsDamageCallback callback) { m_on_damage = std::move(callback); }

private:
    HealthComponent m_health;
    ShieldComponent m_shield;
    ArmorComponent m_armor;
    VitalsDamageCallback m_on_damage;
};

} // namespace void_combat
