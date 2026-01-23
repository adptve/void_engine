/// @file weapons.cpp
/// @brief Weapon system implementation for void_combat module

#include <void_engine/combat/weapons.hpp>

#include <algorithm>
#include <cmath>

namespace void_combat {

// =============================================================================
// Weapon Implementation
// =============================================================================

Weapon::Weapon() = default;

Weapon::Weapon(const WeaponConfig& config)
    : m_config(config) {
    m_state.current_ammo = config.magazine_size;
    m_state.reserve_ammo = config.max_ammo;
}

bool Weapon::can_fire() const {
    if (m_state.is_reloading) return false;
    if (m_state.current_ammo == 0) return false;
    if (m_state.fire_cooldown > 0) return false;
    return true;
}

bool Weapon::fire(const void_math::Vec3& origin, const void_math::Vec3& direction) {
    if (!can_fire()) return false;

    if (!perform_fire()) return false;

    on_fire_internal(origin, direction);
    return true;
}

void Weapon::start_firing() {
    m_state.is_firing = true;

    if (m_config.fire_mode == FireMode::Charge && !m_state.is_charging) {
        start_charge();
    }
}

void Weapon::stop_firing() {
    m_state.is_firing = false;

    if (m_config.fire_mode == FireMode::Charge && m_state.is_charging) {
        release_charge();
    }
}

bool Weapon::reload() {
    if (m_state.is_reloading) return false;
    if (m_state.current_ammo >= m_config.magazine_size) return false;
    if (m_state.reserve_ammo == 0) return false;

    m_state.is_reloading = true;
    m_state.reload_progress = 0;

    if (m_on_reload_start) {
        m_on_reload_start();
    }

    return true;
}

void Weapon::cancel_reload() {
    m_state.is_reloading = false;
    m_state.reload_progress = 0;
}

void Weapon::start_charge() {
    if (m_config.fire_mode != FireMode::Charge) return;
    m_state.is_charging = true;
    m_state.charge_progress = 0;
}

void Weapon::release_charge() {
    if (!m_state.is_charging) return;
    m_state.is_charging = false;
    // Fire if charged enough (e.g., > 50%)
}

float Weapon::charge_percent() const {
    return m_state.charge_progress / m_config.charge_time;
}

void Weapon::add_ammo(std::uint32_t amount) {
    m_state.reserve_ammo = std::min(m_state.reserve_ammo + amount, m_config.max_ammo);
}

void Weapon::set_ammo(std::uint32_t current, std::uint32_t reserve) {
    m_state.current_ammo = std::min(current, m_config.magazine_size);
    m_state.reserve_ammo = std::min(reserve, m_config.max_ammo);
}

void Weapon::update(float dt) {
    // Update fire cooldown
    if (m_state.fire_cooldown > 0) {
        m_state.fire_cooldown -= dt;
    }

    // Update reload
    if (m_state.is_reloading) {
        m_state.reload_progress += dt / m_config.reload_time;
        if (m_state.reload_progress >= 1.0f) {
            // Reload complete
            std::uint32_t needed = m_config.magazine_size - m_state.current_ammo;
            std::uint32_t available = std::min(needed, m_state.reserve_ammo);
            m_state.current_ammo += available;
            m_state.reserve_ammo -= available;
            m_state.is_reloading = false;
            m_state.reload_progress = 0;

            if (m_on_reload_complete) {
                m_on_reload_complete();
            }
        }
    }

    // Update charge
    if (m_state.is_charging) {
        m_state.charge_progress = std::min(m_state.charge_progress + dt, m_config.charge_time);
    }

    // Auto-fire for full-auto weapons
    if (m_state.is_firing && m_config.fire_mode == FireMode::FullAuto && can_fire()) {
        perform_fire();
    }

    // Burst fire
    if (m_config.fire_mode == FireMode::Burst && m_state.burst_shots_remaining > 0) {
        if (m_state.fire_cooldown <= 0 && can_fire()) {
            if (perform_fire()) {
                m_state.burst_shots_remaining--;
            }
        }
    }

    // Spread decay
    m_state.spread_accumulation = std::max(0.0f, m_state.spread_accumulation - dt * 2.0f);

    // Auto-reload
    if (m_config.auto_reload && m_state.current_ammo == 0 && m_state.reserve_ammo > 0 && !m_state.is_reloading) {
        reload();
    }
}

float Weapon::calculate_damage(float distance, bool is_critical, bool is_headshot) const {
    float damage = m_config.base_damage;

    // Critical hit
    if (is_critical) {
        damage *= m_config.critical_multiplier;
    }

    // Headshot
    if (is_headshot) {
        damage *= m_config.headshot_multiplier;
    }

    // Distance falloff
    if (distance > m_config.falloff_start) {
        float falloff_range = m_config.falloff_end - m_config.falloff_start;
        float falloff_progress = std::min(1.0f, (distance - m_config.falloff_start) / falloff_range);
        float falloff_mult = 1.0f - falloff_progress * (1.0f - m_config.min_damage_mult);
        damage *= falloff_mult;
    }

    return damage;
}

float Weapon::calculate_spread(bool is_aiming) const {
    float spread = m_config.spread;
    spread += m_state.spread_accumulation;

    if (is_aiming) {
        spread *= m_config.aim_down_sights_mult;
    }

    return spread * (1.0f - m_config.accuracy);
}

void Weapon::apply_recoil() {
    m_state.spread_accumulation += m_config.recoil * 0.1f;
}

void Weapon::reset_spread() {
    m_state.spread_accumulation = 0;
}

void Weapon::on_fire_internal(const void_math::Vec3& origin, const void_math::Vec3& direction) {
    (void)origin;
    (void)direction;

    if (m_on_fire) {
        m_on_fire(m_owner, m_id);
    }
}

bool Weapon::perform_fire() {
    if (!can_fire()) return false;

    m_state.current_ammo--;

    // Calculate fire rate cooldown
    float rpm = m_config.fire_rate;
    float seconds_per_shot = 60.0f / rpm;
    m_state.fire_cooldown = seconds_per_shot;

    // Start burst if applicable
    if (m_config.fire_mode == FireMode::Burst && m_state.burst_shots_remaining == 0) {
        m_state.burst_shots_remaining = m_config.burst_count - 1;  // -1 because we just fired
    }

    apply_recoil();

    return true;
}

// =============================================================================
// HitscanWeapon Implementation
// =============================================================================

void HitscanWeapon::perform_hitscan(const void_math::Vec3& origin, const void_math::Vec3& direction, HitCallback callback) {
    m_hit_callback = std::move(callback);

    if (!m_raycast || !m_hit_callback) return;

    // For each pellet (shotgun support)
    for (std::uint32_t i = 0; i < m_config.pellet_count; ++i) {
        void_math::Vec3 final_dir = direction;

        // Apply spread
        float spread = calculate_spread(false);  // TODO: Get aiming state
        if (spread > 0) {
            std::uniform_real_distribution<float> dist(-spread, spread);
            final_dir.x += dist(m_rng);
            final_dir.y += dist(m_rng);
            // Renormalize
            float len = std::sqrt(final_dir.x * final_dir.x + final_dir.y * final_dir.y + final_dir.z * final_dir.z);
            if (len > 0) {
                final_dir.x /= len;
                final_dir.y /= len;
                final_dir.z /= len;
            }
        }

        void_math::Vec3 hit_point, hit_normal;
        EntityId hit_entity;

        if (m_raycast(origin, final_dir, m_config.range, hit_point, hit_normal, hit_entity)) {
            // Calculate distance
            float dx = hit_point.x - origin.x;
            float dy = hit_point.y - origin.y;
            float dz = hit_point.z - origin.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            // Roll for critical
            std::uniform_real_distribution<float> crit_dist(0.0f, 1.0f);
            bool is_critical = crit_dist(m_rng) < m_config.critical_chance;

            float damage = calculate_damage(distance, is_critical, false);  // Headshot detected elsewhere

            m_hit_callback(hit_entity, hit_point, hit_normal, damage);
        }
    }
}

void HitscanWeapon::on_fire_internal(const void_math::Vec3& origin, const void_math::Vec3& direction) {
    Weapon::on_fire_internal(origin, direction);
    // Hitscan handled via perform_hitscan
}

// =============================================================================
// ProjectileWeapon Implementation
// =============================================================================

void ProjectileWeapon::on_fire_internal(const void_math::Vec3& origin, const void_math::Vec3& direction) {
    Weapon::on_fire_internal(origin, direction);

    if (m_spawn_func) {
        void_math::Vec3 final_dir = direction;

        // Apply spread
        float spread = calculate_spread(false);
        if (spread > 0) {
            std::uniform_real_distribution<float> dist(-spread, spread);
            final_dir.x += dist(m_rng);
            final_dir.y += dist(m_rng);
            // Renormalize
            float len = std::sqrt(final_dir.x * final_dir.x + final_dir.y * final_dir.y + final_dir.z * final_dir.z);
            if (len > 0) {
                final_dir.x /= len;
                final_dir.y /= len;
                final_dir.z /= len;
            }
        }

        // Use weapon damage if projectile config doesn't specify
        ProjectileConfig config = m_projectile_config;
        if (config.damage <= 0) {
            config.damage = m_config.base_damage;
        }
        if (!config.damage_type) {
            config.damage_type = m_config.damage_type;
        }

        m_spawn_func(origin, final_dir, config, m_owner);
    }
}

// =============================================================================
// MeleeWeapon Implementation
// =============================================================================

MeleeWeapon::MeleeWeapon() {
    m_config.fire_mode = FireMode::Melee;
    m_config.magazine_size = 1;
    m_config.max_ammo = 1;
    m_config.is_hitscan = false;
}

MeleeWeapon::MeleeWeapon(const WeaponConfig& config)
    : Weapon(config) {
    m_config.fire_mode = FireMode::Melee;
}

void MeleeWeapon::perform_attack(const void_math::Vec3& origin, const void_math::Vec3& direction, MeleeHitCallback callback) {
    m_hit_callback = std::move(callback);

    if (!m_overlap_func || !m_hit_callback) return;

    auto hits = m_overlap_func(origin, m_attack_radius, m_attack_arc, direction);

    for (EntityId hit_entity : hits) {
        float damage = m_config.base_damage;

        // Roll for critical
        std::uniform_real_distribution<float> crit_dist(0.0f, 1.0f);
        if (crit_dist(m_rng) < m_config.critical_chance) {
            damage *= m_config.critical_multiplier;
        }

        m_hit_callback(hit_entity, origin, damage);
    }
}

void MeleeWeapon::on_fire_internal(const void_math::Vec3& origin, const void_math::Vec3& direction) {
    Weapon::on_fire_internal(origin, direction);
    // Melee handled via perform_attack
}

// =============================================================================
// WeaponRegistry Implementation
// =============================================================================

WeaponRegistry::WeaponRegistry() = default;
WeaponRegistry::~WeaponRegistry() = default;

WeaponId WeaponRegistry::register_weapon(const WeaponConfig& config) {
    WeaponId id{m_next_id++};
    m_configs[id] = config;
    if (!config.name.empty()) {
        m_name_lookup[config.name] = id;
    }
    return id;
}

std::unique_ptr<IWeapon> WeaponRegistry::create_weapon(WeaponId template_id) const {
    auto it = m_configs.find(template_id);
    if (it == m_configs.end()) return nullptr;

    const auto& config = it->second;

    std::unique_ptr<Weapon> weapon;
    if (config.fire_mode == FireMode::Melee) {
        weapon = std::make_unique<MeleeWeapon>(config);
    } else if (config.is_hitscan) {
        weapon = std::make_unique<HitscanWeapon>(config);
    } else {
        weapon = std::make_unique<ProjectileWeapon>(config);
    }

    weapon->set_id(template_id);
    return weapon;
}

const WeaponConfig* WeaponRegistry::get_config(WeaponId id) const {
    auto it = m_configs.find(id);
    return it != m_configs.end() ? &it->second : nullptr;
}

WeaponId WeaponRegistry::find_weapon(std::string_view name) const {
    auto it = m_name_lookup.find(std::string(name));
    return it != m_name_lookup.end() ? it->second : WeaponId{};
}

std::vector<WeaponId> WeaponRegistry::all_weapons() const {
    std::vector<WeaponId> result;
    result.reserve(m_configs.size());
    for (const auto& [id, config] : m_configs) {
        result.push_back(id);
    }
    return result;
}

WeaponConfig WeaponRegistry::preset_assault_rifle() {
    WeaponConfig config;
    config.name = "Assault Rifle";
    config.slot = WeaponSlot::Primary;
    config.fire_mode = FireMode::FullAuto;
    config.base_damage = 25.0f;
    config.fire_rate = 600.0f;
    config.magazine_size = 30;
    config.max_ammo = 300;
    config.reload_time = 2.0f;
    config.accuracy = 0.9f;
    config.spread = 0.02f;
    config.recoil = 0.15f;
    config.range = 100.0f;
    config.falloff_start = 50.0f;
    config.falloff_end = 100.0f;
    config.is_hitscan = true;
    return config;
}

WeaponConfig WeaponRegistry::preset_shotgun() {
    WeaponConfig config;
    config.name = "Shotgun";
    config.slot = WeaponSlot::Primary;
    config.fire_mode = FireMode::SemiAuto;
    config.base_damage = 15.0f;  // Per pellet
    config.fire_rate = 60.0f;
    config.magazine_size = 8;
    config.max_ammo = 64;
    config.reload_time = 3.0f;
    config.accuracy = 0.7f;
    config.spread = 0.1f;
    config.range = 30.0f;
    config.falloff_start = 10.0f;
    config.falloff_end = 30.0f;
    config.min_damage_mult = 0.2f;
    config.pellet_count = 8;
    config.is_hitscan = true;
    return config;
}

WeaponConfig WeaponRegistry::preset_sniper() {
    WeaponConfig config;
    config.name = "Sniper Rifle";
    config.slot = WeaponSlot::Primary;
    config.fire_mode = FireMode::SemiAuto;
    config.base_damage = 100.0f;
    config.fire_rate = 40.0f;
    config.critical_chance = 0.1f;
    config.critical_multiplier = 2.5f;
    config.headshot_multiplier = 3.0f;
    config.magazine_size = 5;
    config.max_ammo = 30;
    config.reload_time = 3.5f;
    config.accuracy = 1.0f;
    config.spread = 0;
    config.recoil = 0.5f;
    config.aim_down_sights_mult = 0.0f;
    config.range = 500.0f;
    config.falloff_start = 200.0f;
    config.falloff_end = 500.0f;
    config.is_hitscan = true;
    return config;
}

WeaponConfig WeaponRegistry::preset_pistol() {
    WeaponConfig config;
    config.name = "Pistol";
    config.slot = WeaponSlot::Secondary;
    config.fire_mode = FireMode::SemiAuto;
    config.base_damage = 30.0f;
    config.fire_rate = 300.0f;
    config.magazine_size = 12;
    config.max_ammo = 120;
    config.reload_time = 1.5f;
    config.accuracy = 0.85f;
    config.spread = 0.03f;
    config.range = 50.0f;
    config.is_hitscan = true;
    return config;
}

WeaponConfig WeaponRegistry::preset_smg() {
    WeaponConfig config;
    config.name = "SMG";
    config.slot = WeaponSlot::Primary;
    config.fire_mode = FireMode::FullAuto;
    config.base_damage = 18.0f;
    config.fire_rate = 900.0f;
    config.magazine_size = 35;
    config.max_ammo = 350;
    config.reload_time = 1.8f;
    config.accuracy = 0.8f;
    config.spread = 0.04f;
    config.recoil = 0.1f;
    config.range = 60.0f;
    config.falloff_start = 30.0f;
    config.falloff_end = 60.0f;
    config.is_hitscan = true;
    return config;
}

WeaponConfig WeaponRegistry::preset_rocket_launcher() {
    WeaponConfig config;
    config.name = "Rocket Launcher";
    config.slot = WeaponSlot::Special;
    config.fire_mode = FireMode::SemiAuto;
    config.base_damage = 150.0f;
    config.fire_rate = 30.0f;
    config.magazine_size = 1;
    config.max_ammo = 10;
    config.reload_time = 3.0f;
    config.accuracy = 1.0f;
    config.range = 200.0f;
    config.is_hitscan = false;
    config.projectile_speed = 30.0f;
    config.projectile_gravity = 0.5f;
    return config;
}

WeaponConfig WeaponRegistry::preset_melee_sword() {
    WeaponConfig config;
    config.name = "Sword";
    config.slot = WeaponSlot::Melee;
    config.fire_mode = FireMode::Melee;
    config.base_damage = 50.0f;
    config.fire_rate = 90.0f;
    config.critical_chance = 0.15f;
    config.critical_multiplier = 2.0f;
    config.magazine_size = 1;
    config.max_ammo = 1;
    config.range = 2.5f;
    config.is_hitscan = false;
    return config;
}

// =============================================================================
// WeaponInventory Implementation
// =============================================================================

WeaponInventory::WeaponInventory() = default;
WeaponInventory::~WeaponInventory() = default;

bool WeaponInventory::add_weapon(std::unique_ptr<IWeapon> weapon) {
    if (!weapon) return false;

    WeaponSlot slot = weapon->slot();
    m_weapons[slot] = std::move(weapon);
    return true;
}

std::unique_ptr<IWeapon> WeaponInventory::remove_weapon(WeaponSlot slot) {
    auto it = m_weapons.find(slot);
    if (it == m_weapons.end()) return nullptr;

    auto weapon = std::move(it->second);
    m_weapons.erase(it);
    return weapon;
}

IWeapon* WeaponInventory::get_weapon(WeaponSlot slot) {
    auto it = m_weapons.find(slot);
    return it != m_weapons.end() ? it->second.get() : nullptr;
}

const IWeapon* WeaponInventory::get_weapon(WeaponSlot slot) const {
    auto it = m_weapons.find(slot);
    return it != m_weapons.end() ? it->second.get() : nullptr;
}

bool WeaponInventory::switch_to(WeaponSlot slot) {
    if (!has_weapon(slot)) return false;

    WeaponSlot old_slot = m_current_slot;
    m_current_slot = slot;

    if (m_on_switch) {
        m_on_switch(old_slot, slot);
    }

    return true;
}

IWeapon* WeaponInventory::current_weapon() {
    return get_weapon(m_current_slot);
}

const IWeapon* WeaponInventory::current_weapon() const {
    return get_weapon(m_current_slot);
}

void WeaponInventory::cycle_next() {
    static const WeaponSlot order[] = {
        WeaponSlot::Primary, WeaponSlot::Secondary, WeaponSlot::Melee,
        WeaponSlot::Special, WeaponSlot::Grenade
    };

    int current_idx = 0;
    for (int i = 0; i < 5; ++i) {
        if (order[i] == m_current_slot) {
            current_idx = i;
            break;
        }
    }

    for (int i = 1; i <= 5; ++i) {
        int idx = (current_idx + i) % 5;
        if (has_weapon(order[idx])) {
            switch_to(order[idx]);
            return;
        }
    }
}

void WeaponInventory::cycle_previous() {
    static const WeaponSlot order[] = {
        WeaponSlot::Primary, WeaponSlot::Secondary, WeaponSlot::Melee,
        WeaponSlot::Special, WeaponSlot::Grenade
    };

    int current_idx = 0;
    for (int i = 0; i < 5; ++i) {
        if (order[i] == m_current_slot) {
            current_idx = i;
            break;
        }
    }

    for (int i = 1; i <= 5; ++i) {
        int idx = (current_idx - i + 5) % 5;
        if (has_weapon(order[idx])) {
            switch_to(order[idx]);
            return;
        }
    }
}

void WeaponInventory::update(float dt) {
    for (auto& [slot, weapon] : m_weapons) {
        weapon->update(dt);
    }
}

bool WeaponInventory::has_weapon(WeaponSlot slot) const {
    return m_weapons.find(slot) != m_weapons.end();
}

} // namespace void_combat
