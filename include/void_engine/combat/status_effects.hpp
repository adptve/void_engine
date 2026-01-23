/// @file status_effects.hpp
/// @brief Status effect system for void_combat

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_combat {

// =============================================================================
// Status Effect Interface
// =============================================================================

/// @brief Interface for status effects
class IStatusEffect {
public:
    virtual ~IStatusEffect() = default;

    // Identity
    virtual StatusEffectId id() const = 0;
    virtual std::string_view name() const = 0;
    virtual StatusEffectType type() const = 0;

    // Application
    virtual void on_apply(EntityId target) = 0;
    virtual void on_remove(EntityId target) = 0;
    virtual void on_tick(EntityId target, float dt) = 0;
    virtual void on_stack(EntityId target, std::uint32_t new_stacks) = 0;

    // Configuration
    virtual const StatusEffectConfig& config() const = 0;

    // Modifiers (computed for current stacks)
    virtual float damage_modifier() const = 0;
    virtual float speed_modifier() const = 0;
    virtual float defense_modifier() const = 0;
    virtual float attack_speed_modifier() const = 0;

    // Control effects
    virtual bool is_rooted() const = 0;
    virtual bool is_silenced() const = 0;
    virtual bool is_disarmed() const = 0;
    virtual bool is_stunned() const = 0;
    virtual bool is_invulnerable() const = 0;
};

// =============================================================================
// Status Effect Implementation
// =============================================================================

/// @brief Standard status effect implementation
class StatusEffect : public IStatusEffect {
public:
    StatusEffect();
    explicit StatusEffect(const StatusEffectConfig& config);
    ~StatusEffect() override = default;

    // IStatusEffect interface
    StatusEffectId id() const override { return m_id; }
    std::string_view name() const override { return m_config.name; }
    StatusEffectType type() const override { return m_config.type; }

    void on_apply(EntityId target) override;
    void on_remove(EntityId target) override;
    void on_tick(EntityId target, float dt) override;
    void on_stack(EntityId target, std::uint32_t new_stacks) override;

    const StatusEffectConfig& config() const override { return m_config; }

    float damage_modifier() const override;
    float speed_modifier() const override;
    float defense_modifier() const override;
    float attack_speed_modifier() const override;

    bool is_rooted() const override { return m_config.root; }
    bool is_silenced() const override { return m_config.silence; }
    bool is_disarmed() const override { return m_config.disarm; }
    bool is_stunned() const override { return m_config.stun; }
    bool is_invulnerable() const override { return m_config.invulnerable; }

    // Additional methods
    void set_id(StatusEffectId id) { m_id = id; }
    void set_stacks(std::uint32_t stacks) { m_current_stacks = stacks; }
    std::uint32_t stacks() const { return m_current_stacks; }

    // Callbacks
    using ApplyCallback = std::function<void(EntityId target)>;
    using RemoveCallback = std::function<void(EntityId target)>;
    using TickCallback = std::function<void(EntityId target, float damage)>;

    void set_on_apply(ApplyCallback callback) { m_on_apply = std::move(callback); }
    void set_on_remove(RemoveCallback callback) { m_on_remove = std::move(callback); }
    void set_on_tick(TickCallback callback) { m_on_tick_callback = std::move(callback); }

protected:
    StatusEffectId m_id{};
    StatusEffectConfig m_config;
    std::uint32_t m_current_stacks{1};

    ApplyCallback m_on_apply;
    RemoveCallback m_on_remove;
    TickCallback m_on_tick_callback;
};

// =============================================================================
// Status Effect Registry
// =============================================================================

/// @brief Registry for status effect templates
class StatusEffectRegistry {
public:
    StatusEffectRegistry();
    ~StatusEffectRegistry();

    /// @brief Register a status effect template
    StatusEffectId register_effect(const StatusEffectConfig& config);

    /// @brief Get effect config
    const StatusEffectConfig* get_config(StatusEffectId id) const;

    /// @brief Find effect by name
    StatusEffectId find_effect(std::string_view name) const;

    /// @brief Get all registered effects
    std::vector<StatusEffectId> all_effects() const;

    // Preset effects
    static StatusEffectConfig preset_burning();
    static StatusEffectConfig preset_poison();
    static StatusEffectConfig preset_frozen();
    static StatusEffectConfig preset_stunned();
    static StatusEffectConfig preset_bleeding();
    static StatusEffectConfig preset_regeneration();
    static StatusEffectConfig preset_haste();
    static StatusEffectConfig preset_slow();
    static StatusEffectConfig preset_weakness();
    static StatusEffectConfig preset_strength();
    static StatusEffectConfig preset_shield();
    static StatusEffectConfig preset_invulnerable();

private:
    std::unordered_map<StatusEffectId, StatusEffectConfig> m_configs;
    std::unordered_map<std::string, StatusEffectId> m_name_lookup;
    std::uint32_t m_next_id{1};
};

// =============================================================================
// Status Effect Component
// =============================================================================

/// @brief Component that manages status effects on an entity
class StatusEffectComponent {
public:
    StatusEffectComponent();
    explicit StatusEffectComponent(StatusEffectRegistry* registry);
    ~StatusEffectComponent();

    /// @brief Apply a status effect
    /// @param effect_id Effect to apply
    /// @param source Entity that applied the effect
    /// @return True if effect was applied (or stacked)
    bool apply_effect(StatusEffectId effect_id, EntityId source = EntityId{});

    /// @brief Remove a status effect
    /// @param effect_id Effect to remove
    /// @param all_stacks Remove all stacks
    void remove_effect(StatusEffectId effect_id, bool all_stacks = true);

    /// @brief Remove all effects of a type
    void remove_effects_of_type(StatusEffectType type);

    /// @brief Remove all effects
    void clear_effects();

    /// @brief Check if entity has effect
    bool has_effect(StatusEffectId effect_id) const;

    /// @brief Get effect instance
    const StatusEffectInstance* get_effect(StatusEffectId effect_id) const;

    /// @brief Get all active effects
    const std::vector<StatusEffectInstance>& active_effects() const { return m_effects; }

    /// @brief Get effect count
    std::size_t effect_count() const { return m_effects.size(); }

    // Computed modifiers (sum of all effects)
    float total_damage_modifier() const;
    float total_speed_modifier() const;
    float total_defense_modifier() const;
    float total_attack_speed_modifier() const;

    // Control state
    bool is_rooted() const;
    bool is_silenced() const;
    bool is_disarmed() const;
    bool is_stunned() const;
    bool is_invulnerable() const;
    bool can_act() const;
    bool can_move() const;
    bool can_attack() const;
    bool can_use_abilities() const;

    /// @brief Update all effects
    void update(float dt);

    /// @brief Set owner entity
    void set_owner(EntityId owner) { m_owner = owner; }
    EntityId owner() const { return m_owner; }

    // Callbacks
    using EffectAppliedCallback = std::function<void(StatusEffectId effect, std::uint32_t stacks)>;
    using EffectRemovedCallback = std::function<void(StatusEffectId effect)>;
    using EffectTickCallback = std::function<void(StatusEffectId effect, float damage)>;

    void on_effect_applied(EffectAppliedCallback callback) { m_on_applied = std::move(callback); }
    void on_effect_removed(EffectRemovedCallback callback) { m_on_removed = std::move(callback); }
    void on_effect_tick(EffectTickCallback callback) { m_on_tick = std::move(callback); }

    // Immunity
    void add_immunity(StatusEffectId effect_id);
    void remove_immunity(StatusEffectId effect_id);
    bool is_immune(StatusEffectId effect_id) const;
    void add_type_immunity(StatusEffectType type);
    void remove_type_immunity(StatusEffectType type);

private:
    void stack_effect(StatusEffectInstance& instance, const StatusEffectConfig& config);
    void tick_effect(StatusEffectInstance& instance, const StatusEffectConfig& config, float dt);

    EntityId m_owner{};
    StatusEffectRegistry* m_registry{nullptr};
    std::vector<StatusEffectInstance> m_effects;
    std::vector<StatusEffectId> m_immunities;
    std::vector<StatusEffectType> m_type_immunities;

    EffectAppliedCallback m_on_applied;
    EffectRemovedCallback m_on_removed;
    EffectTickCallback m_on_tick;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Check if effect is a buff
inline bool is_buff(const StatusEffectConfig& config) {
    return config.type == StatusEffectType::Buff;
}

/// @brief Check if effect is a debuff
inline bool is_debuff(const StatusEffectConfig& config) {
    return config.type == StatusEffectType::Debuff;
}

/// @brief Check if effect is crowd control
inline bool is_cc(const StatusEffectConfig& config) {
    return config.root || config.stun || config.silence || config.disarm;
}

/// @brief Check if effect is damage over time
inline bool is_dot(const StatusEffectConfig& config) {
    return config.damage_per_tick > 0 && config.tick_interval > 0;
}

} // namespace void_combat
