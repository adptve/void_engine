/// @file types.hpp
/// @brief Common types and configurations for void_combat module

#pragma once

#include "fwd.hpp"
#include <void_engine/math/types.hpp>

#include <chrono>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace void_combat {

// =============================================================================
// Damage Types
// =============================================================================

/// @brief Standard damage type categories
enum class DamageCategory : std::uint8_t {
    Physical,       ///< Kinetic, piercing, slashing
    Fire,           ///< Heat, burn
    Ice,            ///< Cold, freeze
    Electric,       ///< Shock, lightning
    Poison,         ///< Toxin, venom
    Magic,          ///< Arcane, supernatural
    True,           ///< Ignores all defenses
    Healing,        ///< Negative damage (heals)
    Custom          ///< User-defined
};

/// @brief Damage flags
enum class DamageFlags : std::uint32_t {
    None            = 0,
    Critical        = 1 << 0,   ///< Critical hit
    Headshot        = 1 << 1,   ///< Headshot bonus
    Backstab        = 1 << 2,   ///< Backstab bonus
    IgnoreArmor     = 1 << 3,   ///< Bypass armor
    IgnoreShield    = 1 << 4,   ///< Bypass shields
    LifeSteal       = 1 << 5,   ///< Return health to attacker
    AreaOfEffect    = 1 << 6,   ///< Splash damage
    DamageOverTime  = 1 << 7,   ///< DOT tick
    Reflected       = 1 << 8,   ///< Reflected damage
    SelfDamage      = 1 << 9,   ///< Self-inflicted
    Environmental   = 1 << 10,  ///< World hazard
    FriendlyFire    = 1 << 11,  ///< Team damage
    Execution       = 1 << 12,  ///< Finisher damage
    Overflow        = 1 << 13,  ///< Overkill damage
};

inline DamageFlags operator|(DamageFlags a, DamageFlags b) {
    return static_cast<DamageFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline DamageFlags operator&(DamageFlags a, DamageFlags b) {
    return static_cast<DamageFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline bool has_flag(DamageFlags flags, DamageFlags flag) {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0;
}

/// @brief Damage type definition
struct DamageTypeDef {
    std::string name;
    DamageCategory category{DamageCategory::Physical};
    float base_resistance{0};        ///< Default resistance
    StatusEffectId apply_effect{};   ///< Status effect on hit
    float effect_chance{1.0f};       ///< Chance to apply effect
    std::uint32_t color{0xFFFFFFFF}; ///< UI color
};

// =============================================================================
// Health Types
// =============================================================================

/// @brief Health configuration
struct HealthConfig {
    float max_health{100.0f};
    float current_health{100.0f};
    float health_regen{0};           ///< HP per second
    float regen_delay{3.0f};         ///< Delay after damage before regen
    bool can_die{true};
    bool invulnerable{false};
};

/// @brief Shield configuration
struct ShieldConfig {
    float max_shield{0};
    float current_shield{0};
    float shield_regen{10.0f};       ///< Shield per second
    float regen_delay{2.0f};         ///< Delay after damage
    float damage_ratio{1.0f};        ///< How much damage shields absorb
    bool blocks_all_damage{false};   ///< Shield must break first
};

/// @brief Armor configuration
struct ArmorConfig {
    float armor_value{0};            ///< Flat damage reduction
    float armor_penetration{0};      ///< Ignore enemy armor
    float damage_reduction{0};       ///< Percentage reduction (0-1)

    // Type-specific resistances
    std::unordered_map<DamageTypeId, float> resistances;
};

// =============================================================================
// Weapon Types
// =============================================================================

/// @brief Weapon fire mode
enum class FireMode : std::uint8_t {
    SemiAuto,       ///< Single shot per trigger
    FullAuto,       ///< Continuous fire
    Burst,          ///< Fixed burst count
    Charge,         ///< Charge up before firing
    Beam,           ///< Continuous beam
    Melee           ///< Melee attack
};

/// @brief Weapon slot type
enum class WeaponSlot : std::uint8_t {
    Primary,
    Secondary,
    Melee,
    Special,
    Grenade
};

/// @brief Weapon configuration
struct WeaponConfig {
    std::string name;
    WeaponSlot slot{WeaponSlot::Primary};
    FireMode fire_mode{FireMode::SemiAuto};

    // Damage
    float base_damage{10.0f};
    DamageTypeId damage_type{};
    float critical_chance{0.05f};
    float critical_multiplier{2.0f};
    float headshot_multiplier{2.0f};

    // Fire rate
    float fire_rate{600.0f};         ///< Rounds per minute
    std::uint32_t burst_count{3};    ///< For burst fire mode
    float burst_delay{0.1f};         ///< Between bursts
    float charge_time{1.0f};         ///< For charge weapons

    // Ammo
    std::uint32_t magazine_size{30};
    std::uint32_t max_ammo{300};
    float reload_time{2.0f};
    bool auto_reload{true};

    // Accuracy
    float accuracy{1.0f};            ///< 1.0 = perfect
    float spread{0};                 ///< Bullet spread angle
    float recoil{0};                 ///< Camera kick
    float aim_down_sights_mult{0.5f};///< Spread reduction when aiming

    // Range
    float range{1000.0f};
    float falloff_start{50.0f};      ///< Start damage falloff
    float falloff_end{100.0f};       ///< End damage falloff (min damage)
    float min_damage_mult{0.5f};     ///< Minimum damage at max range

    // Projectile (if applicable)
    bool is_hitscan{true};
    float projectile_speed{100.0f};
    float projectile_gravity{0};
    std::uint32_t pellet_count{1};   ///< For shotguns

    // Effects
    StatusEffectId on_hit_effect{};
    float on_hit_chance{1.0f};
};

/// @brief Weapon state
struct WeaponState {
    std::uint32_t current_ammo{0};
    std::uint32_t reserve_ammo{0};
    bool is_reloading{false};
    bool is_firing{false};
    bool is_charging{false};
    float reload_progress{0};
    float charge_progress{0};
    float fire_cooldown{0};
    float spread_accumulation{0};
    std::uint32_t burst_shots_remaining{0};
};

// =============================================================================
// Projectile Types
// =============================================================================

/// @brief Projectile configuration
struct ProjectileConfig {
    float speed{50.0f};
    float gravity{0};
    float lifetime{5.0f};
    float radius{0.1f};
    float damage{10.0f};
    DamageTypeId damage_type{};
    float explosion_radius{0};       ///< 0 = no explosion
    float explosion_falloff{1.0f};   ///< Damage falloff from center
    bool destroy_on_hit{true};
    std::uint32_t max_penetrations{0};
    bool homing{false};
    float homing_strength{0};
};

/// @brief Projectile state
struct ProjectileState {
    void_math::Vec3 position{};
    void_math::Vec3 velocity{};
    void_math::Vec3 direction{};
    float lifetime_remaining{0};
    EntityId owner{};
    EntityId target{};               ///< For homing
    std::uint32_t penetrations{0};
    bool active{true};
};

// =============================================================================
// Status Effect Types
// =============================================================================

/// @brief Status effect type
enum class StatusEffectType : std::uint8_t {
    Buff,           ///< Positive effect
    Debuff,         ///< Negative effect
    Neutral         ///< Neither (e.g., transformation)
};

/// @brief Status effect stacking behavior
enum class StackBehavior : std::uint8_t {
    None,           ///< Doesn't stack
    Duration,       ///< Refresh duration
    Intensity,      ///< Stack intensity
    Both,           ///< Stack both
    Separate        ///< Multiple independent instances
};

/// @brief Status effect configuration
struct StatusEffectConfig {
    std::string name;
    StatusEffectType type{StatusEffectType::Debuff};
    StackBehavior stacking{StackBehavior::Duration};
    std::uint32_t max_stacks{1};

    float duration{5.0f};
    float tick_interval{1.0f};       ///< 0 = no tick

    // Stat modifications
    float damage_modifier{0};        ///< +/- percentage
    float speed_modifier{0};
    float defense_modifier{0};
    float attack_speed_modifier{0};

    // Periodic damage
    float damage_per_tick{0};
    DamageTypeId tick_damage_type{};

    // Movement effects
    bool root{false};                ///< Cannot move
    bool silence{false};             ///< Cannot use abilities
    bool disarm{false};              ///< Cannot attack
    bool stun{false};                ///< Cannot act
    bool invulnerable{false};

    // Visual/audio
    std::string vfx_id;
    std::string sfx_id;
    std::uint32_t icon_id{0};
};

/// @brief Active status effect instance
struct StatusEffectInstance {
    StatusEffectId effect_id{};
    float duration_remaining{0};
    float tick_timer{0};
    std::uint32_t stacks{1};
    EntityId source{};               ///< Who applied it
    bool permanent{false};
};

// =============================================================================
// Combat Events
// =============================================================================

/// @brief Damage information
struct DamageInfo {
    EntityId attacker{};
    EntityId victim{};
    WeaponId weapon{};
    DamageTypeId damage_type{};
    float base_damage{0};
    float final_damage{0};
    DamageFlags flags{DamageFlags::None};
    void_math::Vec3 hit_position{};
    void_math::Vec3 hit_normal{};
    std::string hit_bone;            ///< For skeletal hit detection
};

/// @brief Damage calculation result
struct DamageResult {
    float damage_dealt{0};
    float damage_absorbed_shield{0};
    float damage_absorbed_armor{0};
    float damage_mitigated{0};       ///< From resistances
    float final_damage{0};           ///< Final damage after all modifiers
    float health_before{0};
    float health_after{0};
    bool was_critical{false};
    bool was_headshot{false};
    bool was_fatal{false};
    bool was_overkill{false};
    float overkill_damage{0};
};

/// @brief Hit event data
struct HitEvent {
    DamageInfo damage_info;
    DamageResult result;
    float timestamp{0};
};

/// @brief Kill event data
struct KillEvent {
    EntityId killer{};
    EntityId victim{};
    WeaponId weapon{};
    DamageTypeId final_damage_type{};
    bool was_headshot{false};
    bool was_critical{false};
    bool was_melee{false};
    float total_damage_dealt{0};
    float timestamp{0};
    std::vector<EntityId> assists;   ///< Entities that assisted
};

/// @brief Death event data
struct DeathEvent {
    EntityId entity{};
    KillEvent kill_event;
    void_math::Vec3 death_position{};
    bool can_respawn{true};
    float respawn_time{5.0f};
};

// =============================================================================
// Combat System Configuration
// =============================================================================

/// @brief Combat system configuration
struct CombatConfig {
    // Damage calculation
    float global_damage_multiplier{1.0f};
    bool friendly_fire{false};
    float friendly_fire_multiplier{0.5f};
    bool self_damage{true};
    float self_damage_multiplier{0.5f};

    // Critical hits
    float base_critical_chance{0.05f};
    float base_critical_multiplier{2.0f};
    float headshot_multiplier{2.0f};
    float backstab_multiplier{1.5f};

    // Assist tracking
    float assist_window{10.0f};      ///< Seconds to count as assist
    float assist_damage_threshold{0.1f}; ///< Min damage % for assist

    // Hit feedback
    bool hit_numbers{true};
    bool hit_markers{true};
    bool kill_feed{true};

    // Respawn
    bool instant_respawn{false};
    float default_respawn_time{5.0f};
    bool spawn_protection{true};
    float spawn_protection_time{3.0f};
};

// =============================================================================
// Callbacks
// =============================================================================

/// @brief Damage callback type
using DamageCallback = std::function<void(const HitEvent&)>;

/// @brief Kill callback type
using KillCallback = std::function<void(const KillEvent&)>;

/// @brief Death callback type
using DeathCallback = std::function<void(const DeathEvent&)>;

/// @brief Weapon fire callback type
using WeaponFireCallback = std::function<void(EntityId owner, WeaponId weapon)>;

} // namespace void_combat
