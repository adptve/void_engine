/// @file combat.hpp
/// @brief Main header for void_combat module
///
/// This file provides a unified interface to the void_combat module, which implements
/// comprehensive combat systems including health, damage, weapons, and status effects.
/// The systems are designed for AAA-quality game combat with full hot-reload support.
///
/// # Module Overview
///
/// ## Health System
/// Complete health, shield, and armor system:
/// - Health with regeneration and death handling
/// - Rechargeable shields with damage absorption
/// - Armor with flat and percentage reduction
/// - Type-specific damage resistances
///
/// ## Damage System
/// Flexible damage calculation:
/// - Multiple damage types (physical, elemental, etc.)
/// - Critical hits and headshots
/// - Damage falloff over distance
/// - Armor penetration
///
/// ## Weapons
/// Full weapon system:
/// - Hitscan and projectile weapons
/// - Multiple fire modes (semi, auto, burst, charge)
/// - Ammo management and reloading
/// - Accuracy, spread, and recoil
/// - Melee and area weapons
///
/// ## Status Effects
/// Comprehensive buff/debuff system:
/// - Duration and tick-based effects
/// - Stacking behaviors
/// - Stat modifications
/// - Crowd control (stun, root, silence)
///
/// # Example Usage
///
/// @code
/// #include <void_engine/combat/combat.hpp>
/// using namespace void_combat;
///
/// // Create combat system
/// CombatSystem combat(CombatConfig{});
///
/// // Register damage types
/// auto fire_damage = combat.damage_types().register_type(DamageTypeDef{
///     .name = "Fire",
///     .category = DamageCategory::Fire,
///     .apply_effect = burning_effect_id,
///     .effect_chance = 0.3f
/// });
///
/// // Create health component
/// VitalsComponent vitals(
///     HealthConfig{.max_health = 100},
///     ShieldConfig{.max_shield = 50, .shield_regen = 10},
///     ArmorConfig{.armor_value = 10}
/// );
///
/// // Create weapon
/// auto rifle_config = WeaponRegistry::preset_assault_rifle();
/// auto rifle = combat.weapons().create_weapon(rifle_config);
///
/// // Apply damage
/// DamageInfo info;
/// info.attacker = player_id;
/// info.victim = enemy_id;
/// info.base_damage = 25.0f;
/// info.damage_type = fire_damage;
///
/// auto result = combat.apply_damage(info, vitals);
///
/// // Apply status effect
/// status_component.apply_effect(burning_id, player_id);
///
/// // Update systems
/// combat.update(dt);
/// @endcode

#pragma once

// Core types and forward declarations
#include "fwd.hpp"
#include "types.hpp"

// Subsystems
#include "health.hpp"
#include "weapons.hpp"
#include "status_effects.hpp"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace void_combat {

// =============================================================================
// Damage Type Registry
// =============================================================================

/// @brief Registry for damage types
class DamageTypeRegistry {
public:
    DamageTypeRegistry();
    ~DamageTypeRegistry();

    /// @brief Register a new damage type
    DamageTypeId register_type(const DamageTypeDef& def);

    /// @brief Get damage type definition
    const DamageTypeDef* get_type(DamageTypeId id) const;

    /// @brief Find damage type by name
    DamageTypeId find_type(std::string_view name) const;

    // Preset damage types
    static DamageTypeDef preset_physical();
    static DamageTypeDef preset_fire();
    static DamageTypeDef preset_ice();
    static DamageTypeDef preset_electric();
    static DamageTypeDef preset_poison();
    static DamageTypeDef preset_true();

private:
    std::unordered_map<DamageTypeId, DamageTypeDef> m_types;
    std::unordered_map<std::string, DamageTypeId> m_name_lookup;
    std::uint32_t m_next_id{1};
};

// =============================================================================
// Damage Processor
// =============================================================================

/// @brief Processes damage calculations
class DamageProcessor {
public:
    DamageProcessor();
    explicit DamageProcessor(DamageTypeRegistry* types);
    ~DamageProcessor();

    /// @brief Calculate damage
    DamageResult calculate_damage(const DamageInfo& info, const VitalsComponent& target) const;

    /// @brief Apply damage to target
    DamageResult apply_damage(const DamageInfo& info, VitalsComponent& target);

    // Configuration
    void set_global_damage_multiplier(float mult) { m_global_multiplier = mult; }
    float global_damage_multiplier() const { return m_global_multiplier; }

    void set_critical_multiplier(float mult) { m_crit_multiplier = mult; }
    float critical_multiplier() const { return m_crit_multiplier; }

    void set_headshot_multiplier(float mult) { m_headshot_multiplier = mult; }
    float headshot_multiplier() const { return m_headshot_multiplier; }

    // Type registry
    void set_type_registry(DamageTypeRegistry* registry) { m_types = registry; }
    DamageTypeRegistry* type_registry() const { return m_types; }

private:
    DamageTypeRegistry* m_types{nullptr};
    float m_global_multiplier{1.0f};
    float m_crit_multiplier{2.0f};
    float m_headshot_multiplier{2.0f};
};

// =============================================================================
// Projectile System
// =============================================================================

/// @brief Manages active projectiles
class ProjectileSystem {
public:
    ProjectileSystem();
    ~ProjectileSystem();

    /// @brief Spawn a projectile
    ProjectileId spawn(const ProjectileConfig& config, const void_math::Vec3& origin,
                       const void_math::Vec3& direction, EntityId owner);

    /// @brief Destroy a projectile
    void destroy(ProjectileId id);

    /// @brief Get projectile state
    const ProjectileState* get_state(ProjectileId id) const;

    /// @brief Update all projectiles
    void update(float dt);

    /// @brief Set physics raycast function
    using RaycastFunc = std::function<bool(
        const void_math::Vec3& from,
        const void_math::Vec3& to,
        void_math::Vec3& hit_point,
        void_math::Vec3& hit_normal,
        EntityId& hit_entity
    )>;
    void set_raycast_func(RaycastFunc func) { m_raycast = std::move(func); }

    /// @brief Set hit callback
    using HitCallback = std::function<void(ProjectileId projectile, EntityId hit_entity,
                                           const void_math::Vec3& hit_point, float damage)>;
    void on_hit(HitCallback callback) { m_on_hit = std::move(callback); }

    /// @brief Set target position callback (for homing projectiles)
    using GetTargetPositionFunc = std::function<void_math::Vec3(EntityId target)>;
    void set_target_position_func(GetTargetPositionFunc func) { m_get_target_position = std::move(func); }

    /// @brief Set target for a homing projectile
    void set_projectile_target(ProjectileId projectile, EntityId target);

    /// @brief Get active projectile count
    std::size_t active_count() const { return m_projectiles.size(); }

    /// @brief Clear all projectiles
    void clear();

private:
    struct ProjectileData {
        ProjectileId id;
        ProjectileConfig config;
        ProjectileState state;
    };

    std::vector<ProjectileData> m_projectiles;
    std::uint32_t m_next_id{1};
    RaycastFunc m_raycast;
    HitCallback m_on_hit;
    GetTargetPositionFunc m_get_target_position;
};

// =============================================================================
// Hit Detection
// =============================================================================

/// @brief Hit detection and validation
class HitDetection {
public:
    HitDetection();
    ~HitDetection();

    /// @brief Validate a hit
    bool validate_hit(const DamageInfo& info) const;

    /// @brief Check if headshot
    bool is_headshot(const void_math::Vec3& hit_position, EntityId target) const;

    /// @brief Check if backstab
    bool is_backstab(EntityId attacker, EntityId victim) const;

    /// @brief Get position query callback
    using GetPositionFunc = std::function<void_math::Vec3(EntityId entity)>;
    void set_position_func(GetPositionFunc func) { m_get_position = std::move(func); }

    /// @brief Get forward direction callback
    using GetForwardFunc = std::function<void_math::Vec3(EntityId entity)>;
    void set_forward_func(GetForwardFunc func) { m_get_forward = std::move(func); }

    /// @brief Get head position callback (for headshot detection)
    using GetHeadPosFunc = std::function<void_math::Vec3(EntityId entity)>;
    void set_head_pos_func(GetHeadPosFunc func) { m_get_head_pos = std::move(func); }

    // Configuration
    void set_headshot_radius(float radius) { m_headshot_radius = radius; }
    float headshot_radius() const { return m_headshot_radius; }

    void set_backstab_angle(float degrees) { m_backstab_angle = degrees; }
    float backstab_angle() const { return m_backstab_angle; }

private:
    GetPositionFunc m_get_position;
    GetForwardFunc m_get_forward;
    GetHeadPosFunc m_get_head_pos;
    float m_headshot_radius{0.2f};
    float m_backstab_angle{60.0f};
};

// =============================================================================
// Kill Tracker
// =============================================================================

/// @brief Tracks kills and assists
class KillTracker {
public:
    KillTracker();
    ~KillTracker();

    /// @brief Register damage dealt (for assist tracking)
    void register_damage(EntityId attacker, EntityId victim, float damage);

    /// @brief Record a kill
    KillEvent record_kill(EntityId killer, EntityId victim, const DamageInfo& final_blow);

    /// @brief Clear damage history for an entity
    void clear_history(EntityId entity);

    /// @brief Get kill stats for entity
    struct KillStats {
        std::uint32_t kills{0};
        std::uint32_t deaths{0};
        std::uint32_t assists{0};
        float total_damage_dealt{0};
        float total_damage_taken{0};
    };
    KillStats get_stats(EntityId entity) const;

    // Configuration
    void set_assist_window(float seconds) { m_assist_window = seconds; }
    void set_assist_threshold(float percent) { m_assist_threshold = percent; }

    // Callbacks
    void on_kill(KillCallback callback) { m_on_kill = std::move(callback); }

private:
    struct DamageRecord {
        EntityId attacker;
        float damage;
        float timestamp;
    };

    std::unordered_map<EntityId, std::vector<DamageRecord>> m_damage_history;
    std::unordered_map<EntityId, KillStats> m_stats;
    float m_current_time{0};
    float m_assist_window{10.0f};
    float m_assist_threshold{0.1f};
    KillCallback m_on_kill;
};

// =============================================================================
// Combat System
// =============================================================================

/// @brief Main combat system manager
class CombatSystem {
public:
    CombatSystem();
    explicit CombatSystem(const CombatConfig& config);
    ~CombatSystem();

    // Subsystem access
    DamageTypeRegistry& damage_types() { return m_damage_types; }
    const DamageTypeRegistry& damage_types() const { return m_damage_types; }

    DamageProcessor& damage_processor() { return m_damage_processor; }
    const DamageProcessor& damage_processor() const { return m_damage_processor; }

    WeaponRegistry& weapons() { return m_weapons; }
    const WeaponRegistry& weapons() const { return m_weapons; }

    StatusEffectRegistry& status_effects() { return m_status_effects; }
    const StatusEffectRegistry& status_effects() const { return m_status_effects; }

    ProjectileSystem& projectiles() { return m_projectiles; }
    const ProjectileSystem& projectiles() const { return m_projectiles; }

    HitDetection& hit_detection() { return m_hit_detection; }
    const HitDetection& hit_detection() const { return m_hit_detection; }

    KillTracker& kill_tracker() { return m_kill_tracker; }
    const KillTracker& kill_tracker() const { return m_kill_tracker; }

    // High-level operations
    DamageResult apply_damage(const DamageInfo& info, VitalsComponent& target);
    void apply_status_effect(StatusEffectId effect, EntityId target, EntityId source = EntityId{});

    // Update
    void update(float dt);

    // Configuration
    const CombatConfig& config() const { return m_config; }
    void set_config(const CombatConfig& config);

    // Global callbacks
    void on_damage(DamageCallback callback) { m_on_damage = std::move(callback); }
    void on_kill(KillCallback callback) { m_on_kill = std::move(callback); }
    void on_death(DeathCallback callback) { m_on_death = std::move(callback); }

    // Statistics
    struct Stats {
        std::size_t active_projectiles{0};
        std::uint64_t total_damage_events{0};
        std::uint64_t total_kills{0};
    };
    Stats stats() const;

    // Hot reload support
    struct Snapshot {
        CombatConfig config;
        Stats stats;

        // Projectile state
        struct ProjectileSnapshot {
            std::uint32_t id;
            ProjectileConfig config;
            ProjectileState state;
        };
        std::vector<ProjectileSnapshot> projectiles;
        std::uint32_t next_projectile_id{1};

        // Kill tracker state
        struct KillStatsSnapshot {
            std::uint64_t entity_id;
            std::uint32_t kills;
            std::uint32_t deaths;
            std::uint32_t assists;
            float total_damage_dealt;
            float total_damage_taken;
        };
        std::vector<KillStatsSnapshot> kill_stats;
        float kill_tracker_time{0};
    };
    Snapshot take_snapshot() const;
    void apply_snapshot(const Snapshot& snapshot);

private:
    void setup_preset_damage_types();
    void setup_preset_status_effects();

    CombatConfig m_config;
    DamageTypeRegistry m_damage_types;
    DamageProcessor m_damage_processor;
    WeaponRegistry m_weapons;
    StatusEffectRegistry m_status_effects;
    ProjectileSystem m_projectiles;
    HitDetection m_hit_detection;
    KillTracker m_kill_tracker;

    DamageCallback m_on_damage;
    KillCallback m_on_kill;
    DeathCallback m_on_death;

    Stats m_stats;
};

// =============================================================================
// Prelude Namespace
// =============================================================================

/// @brief Commonly used types for convenient imports
namespace prelude {
    // IDs
    using void_combat::WeaponId;
    using void_combat::DamageTypeId;
    using void_combat::StatusEffectId;
    using void_combat::ProjectileId;
    using void_combat::EntityId;

    // Types
    using void_combat::DamageCategory;
    using void_combat::DamageFlags;
    using void_combat::FireMode;
    using void_combat::WeaponSlot;
    using void_combat::StatusEffectType;
    using void_combat::StackBehavior;

    // Config structs
    using void_combat::HealthConfig;
    using void_combat::ShieldConfig;
    using void_combat::ArmorConfig;
    using void_combat::WeaponConfig;
    using void_combat::StatusEffectConfig;
    using void_combat::CombatConfig;

    // Events
    using void_combat::DamageInfo;
    using void_combat::DamageResult;
    using void_combat::HitEvent;
    using void_combat::KillEvent;
    using void_combat::DeathEvent;

    // Components
    using void_combat::HealthComponent;
    using void_combat::ShieldComponent;
    using void_combat::ArmorComponent;
    using void_combat::VitalsComponent;
    using void_combat::StatusEffectComponent;

    // Weapons
    using void_combat::IWeapon;
    using void_combat::Weapon;
    using void_combat::HitscanWeapon;
    using void_combat::ProjectileWeapon;
    using void_combat::MeleeWeapon;
    using void_combat::WeaponInventory;

    // Systems
    using void_combat::CombatSystem;
    using void_combat::DamageProcessor;
    using void_combat::ProjectileSystem;
}

} // namespace void_combat
