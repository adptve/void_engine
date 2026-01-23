/// @file weapons.hpp
/// @brief Weapon systems for void_combat

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_combat {

// =============================================================================
// Weapon Interface
// =============================================================================

/// @brief Interface for all weapons
class IWeapon {
public:
    virtual ~IWeapon() = default;

    // Identity
    virtual WeaponId id() const = 0;
    virtual std::string_view name() const = 0;
    virtual WeaponSlot slot() const = 0;
    virtual FireMode fire_mode() const = 0;

    // Actions
    virtual bool can_fire() const = 0;
    virtual bool fire(const void_math::Vec3& origin, const void_math::Vec3& direction) = 0;
    virtual void start_firing() = 0;
    virtual void stop_firing() = 0;
    virtual bool reload() = 0;
    virtual void cancel_reload() = 0;

    // Charge (for charge weapons)
    virtual void start_charge() = 0;
    virtual void release_charge() = 0;
    virtual float charge_percent() const = 0;

    // Ammo
    virtual std::uint32_t current_ammo() const = 0;
    virtual std::uint32_t magazine_size() const = 0;
    virtual std::uint32_t reserve_ammo() const = 0;
    virtual std::uint32_t max_ammo() const = 0;
    virtual void add_ammo(std::uint32_t amount) = 0;
    virtual void set_ammo(std::uint32_t current, std::uint32_t reserve) = 0;

    // State queries
    virtual bool is_reloading() const = 0;
    virtual bool is_firing() const = 0;
    virtual bool is_charging() const = 0;
    virtual float reload_progress() const = 0;

    // Stats
    virtual float damage() const = 0;
    virtual float fire_rate() const = 0;
    virtual float range() const = 0;
    virtual float accuracy() const = 0;

    // Update
    virtual void update(float dt) = 0;

    // Configuration
    virtual const WeaponConfig& config() const = 0;
    virtual const WeaponState& state() const = 0;

    // Callbacks
    virtual void on_fire(WeaponFireCallback callback) = 0;
    virtual void on_reload_start(std::function<void()> callback) = 0;
    virtual void on_reload_complete(std::function<void()> callback) = 0;
};

// =============================================================================
// Base Weapon Implementation
// =============================================================================

/// @brief Base weapon implementation
class Weapon : public IWeapon {
public:
    Weapon();
    explicit Weapon(const WeaponConfig& config);
    ~Weapon() override = default;

    // IWeapon interface
    WeaponId id() const override { return m_id; }
    std::string_view name() const override { return m_config.name; }
    WeaponSlot slot() const override { return m_config.slot; }
    FireMode fire_mode() const override { return m_config.fire_mode; }

    bool can_fire() const override;
    bool fire(const void_math::Vec3& origin, const void_math::Vec3& direction) override;
    void start_firing() override;
    void stop_firing() override;
    bool reload() override;
    void cancel_reload() override;

    void start_charge() override;
    void release_charge() override;
    float charge_percent() const override;

    std::uint32_t current_ammo() const override { return m_state.current_ammo; }
    std::uint32_t magazine_size() const override { return m_config.magazine_size; }
    std::uint32_t reserve_ammo() const override { return m_state.reserve_ammo; }
    std::uint32_t max_ammo() const override { return m_config.max_ammo; }
    void add_ammo(std::uint32_t amount) override;
    void set_ammo(std::uint32_t current, std::uint32_t reserve) override;

    bool is_reloading() const override { return m_state.is_reloading; }
    bool is_firing() const override { return m_state.is_firing; }
    bool is_charging() const override { return m_state.is_charging; }
    float reload_progress() const override { return m_state.reload_progress; }

    float damage() const override { return m_config.base_damage; }
    float fire_rate() const override { return m_config.fire_rate; }
    float range() const override { return m_config.range; }
    float accuracy() const override { return m_config.accuracy; }

    void update(float dt) override;

    const WeaponConfig& config() const override { return m_config; }
    const WeaponState& state() const override { return m_state; }

    void on_fire(WeaponFireCallback callback) override { m_on_fire = std::move(callback); }
    void on_reload_start(std::function<void()> callback) override { m_on_reload_start = std::move(callback); }
    void on_reload_complete(std::function<void()> callback) override { m_on_reload_complete = std::move(callback); }

    // Additional methods
    void set_id(WeaponId id) { m_id = id; }
    void set_owner(EntityId owner) { m_owner = owner; }
    EntityId owner() const { return m_owner; }

    /// @brief Calculate final damage with modifiers
    float calculate_damage(float distance, bool is_critical, bool is_headshot) const;

    /// @brief Calculate spread angle
    float calculate_spread(bool is_aiming) const;

    /// @brief Apply recoil
    void apply_recoil();

    /// @brief Reset spread accumulation
    void reset_spread();

protected:
    virtual void on_fire_internal(const void_math::Vec3& origin, const void_math::Vec3& direction);
    virtual bool perform_fire();

    WeaponId m_id{};
    EntityId m_owner{};
    WeaponConfig m_config;
    WeaponState m_state;

    WeaponFireCallback m_on_fire;
    std::function<void()> m_on_reload_start;
    std::function<void()> m_on_reload_complete;

    std::mt19937 m_rng{std::random_device{}()};
};

// =============================================================================
// Hitscan Weapon
// =============================================================================

/// @brief Hitscan (instant) weapon
class HitscanWeapon : public Weapon {
public:
    using Weapon::Weapon;

    /// @brief Raycast callback for hit detection
    using RaycastFunc = std::function<bool(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        void_math::Vec3& hit_point,
        void_math::Vec3& hit_normal,
        EntityId& hit_entity
    )>;

    void set_raycast_func(RaycastFunc func) { m_raycast = std::move(func); }

    /// @brief Perform hitscan with hit callback
    using HitCallback = std::function<void(EntityId entity, const void_math::Vec3& hit_point,
                                           const void_math::Vec3& hit_normal, float damage)>;
    void perform_hitscan(const void_math::Vec3& origin, const void_math::Vec3& direction, HitCallback callback);

protected:
    void on_fire_internal(const void_math::Vec3& origin, const void_math::Vec3& direction) override;

private:
    RaycastFunc m_raycast;
    HitCallback m_hit_callback;
};

// =============================================================================
// Projectile Weapon
// =============================================================================

/// @brief Projectile-based weapon
class ProjectileWeapon : public Weapon {
public:
    using Weapon::Weapon;

    /// @brief Spawn projectile callback
    using SpawnProjectileFunc = std::function<ProjectileId(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        const ProjectileConfig& config,
        EntityId owner
    )>;

    void set_spawn_func(SpawnProjectileFunc func) { m_spawn_func = std::move(func); }
    void set_projectile_config(const ProjectileConfig& config) { m_projectile_config = config; }
    const ProjectileConfig& projectile_config() const { return m_projectile_config; }

protected:
    void on_fire_internal(const void_math::Vec3& origin, const void_math::Vec3& direction) override;

private:
    SpawnProjectileFunc m_spawn_func;
    ProjectileConfig m_projectile_config;
};

// =============================================================================
// Melee Weapon
// =============================================================================

/// @brief Melee weapon
class MeleeWeapon : public Weapon {
public:
    MeleeWeapon();
    explicit MeleeWeapon(const WeaponConfig& config);

    /// @brief Perform melee attack
    /// @param origin Attack origin
    /// @param direction Attack direction
    /// @param callback Called for each hit entity
    using MeleeHitCallback = std::function<void(EntityId entity, const void_math::Vec3& hit_point, float damage)>;
    void perform_attack(const void_math::Vec3& origin, const void_math::Vec3& direction, MeleeHitCallback callback);

    /// @brief Overlap query for melee hits
    using OverlapFunc = std::function<std::vector<EntityId>(
        const void_math::Vec3& origin,
        float radius,
        float arc_degrees,
        const void_math::Vec3& forward
    )>;

    void set_overlap_func(OverlapFunc func) { m_overlap_func = std::move(func); }

    // Melee-specific config
    void set_attack_radius(float radius) { m_attack_radius = radius; }
    float attack_radius() const { return m_attack_radius; }

    void set_attack_arc(float degrees) { m_attack_arc = degrees; }
    float attack_arc() const { return m_attack_arc; }

protected:
    void on_fire_internal(const void_math::Vec3& origin, const void_math::Vec3& direction) override;

private:
    OverlapFunc m_overlap_func;
    MeleeHitCallback m_hit_callback;
    float m_attack_radius{2.0f};
    float m_attack_arc{90.0f};
};

// =============================================================================
// Area/Explosive Weapon
// =============================================================================

/// @brief Area of effect weapon
class AreaWeapon : public ProjectileWeapon {
public:
    using ProjectileWeapon::ProjectileWeapon;

    void set_explosion_radius(float radius) { m_explosion_radius = radius; }
    float explosion_radius() const { return m_explosion_radius; }

    void set_explosion_falloff(float falloff) { m_explosion_falloff = falloff; }
    float explosion_falloff() const { return m_explosion_falloff; }

private:
    float m_explosion_radius{5.0f};
    float m_explosion_falloff{1.0f};
};

// =============================================================================
// Weapon Registry
// =============================================================================

/// @brief Registry for weapon templates
class WeaponRegistry {
public:
    WeaponRegistry();
    ~WeaponRegistry();

    /// @brief Register a weapon template
    WeaponId register_weapon(const WeaponConfig& config);

    /// @brief Create a weapon instance from template
    std::unique_ptr<IWeapon> create_weapon(WeaponId template_id) const;

    /// @brief Get weapon template config
    const WeaponConfig* get_config(WeaponId id) const;

    /// @brief Find weapon by name
    WeaponId find_weapon(std::string_view name) const;

    /// @brief Get all registered weapons
    std::vector<WeaponId> all_weapons() const;

    // Preset weapons
    static WeaponConfig preset_assault_rifle();
    static WeaponConfig preset_shotgun();
    static WeaponConfig preset_sniper();
    static WeaponConfig preset_pistol();
    static WeaponConfig preset_smg();
    static WeaponConfig preset_rocket_launcher();
    static WeaponConfig preset_melee_sword();

private:
    std::unordered_map<WeaponId, WeaponConfig> m_configs;
    std::unordered_map<std::string, WeaponId> m_name_lookup;
    std::uint32_t m_next_id{1};
};

// =============================================================================
// Weapon Inventory
// =============================================================================

/// @brief Manages a character's weapons
class WeaponInventory {
public:
    WeaponInventory();
    ~WeaponInventory();

    /// @brief Add a weapon to inventory
    bool add_weapon(std::unique_ptr<IWeapon> weapon);

    /// @brief Remove a weapon
    std::unique_ptr<IWeapon> remove_weapon(WeaponSlot slot);

    /// @brief Get weapon in slot
    IWeapon* get_weapon(WeaponSlot slot);
    const IWeapon* get_weapon(WeaponSlot slot) const;

    /// @brief Switch to weapon slot
    bool switch_to(WeaponSlot slot);

    /// @brief Get current weapon
    IWeapon* current_weapon();
    const IWeapon* current_weapon() const;

    WeaponSlot current_slot() const { return m_current_slot; }

    /// @brief Cycle to next/previous weapon
    void cycle_next();
    void cycle_previous();

    /// @brief Update all weapons
    void update(float dt);

    /// @brief Check if slot has weapon
    bool has_weapon(WeaponSlot slot) const;

    // Callbacks
    using SwitchCallback = std::function<void(WeaponSlot from, WeaponSlot to)>;
    void on_switch(SwitchCallback callback) { m_on_switch = std::move(callback); }

private:
    std::unordered_map<WeaponSlot, std::unique_ptr<IWeapon>> m_weapons;
    WeaponSlot m_current_slot{WeaponSlot::Primary};
    SwitchCallback m_on_switch;
};

} // namespace void_combat
