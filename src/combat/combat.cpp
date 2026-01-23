/// @file combat.cpp
/// @brief Main combat system implementation for void_combat module

#include <void_engine/combat/combat.hpp>

#include <algorithm>
#include <cmath>

namespace void_combat {

// =============================================================================
// DamageTypeRegistry Implementation
// =============================================================================

DamageTypeRegistry::DamageTypeRegistry() = default;
DamageTypeRegistry::~DamageTypeRegistry() = default;

DamageTypeId DamageTypeRegistry::register_type(const DamageTypeDef& def) {
    DamageTypeId id{m_next_id++};
    m_types[id] = def;
    if (!def.name.empty()) {
        m_name_lookup[def.name] = id;
    }
    return id;
}

const DamageTypeDef* DamageTypeRegistry::get_type(DamageTypeId id) const {
    auto it = m_types.find(id);
    return it != m_types.end() ? &it->second : nullptr;
}

DamageTypeId DamageTypeRegistry::find_type(std::string_view name) const {
    auto it = m_name_lookup.find(std::string(name));
    return it != m_name_lookup.end() ? it->second : DamageTypeId{};
}

DamageTypeDef DamageTypeRegistry::preset_physical() {
    DamageTypeDef def;
    def.name = "Physical";
    def.category = DamageCategory::Physical;
    def.color = 0xFFFFFFFF;
    return def;
}

DamageTypeDef DamageTypeRegistry::preset_fire() {
    DamageTypeDef def;
    def.name = "Fire";
    def.category = DamageCategory::Fire;
    def.effect_chance = 0.25f;
    def.color = 0xFFFF6600;
    return def;
}

DamageTypeDef DamageTypeRegistry::preset_ice() {
    DamageTypeDef def;
    def.name = "Ice";
    def.category = DamageCategory::Ice;
    def.effect_chance = 0.2f;
    def.color = 0xFF66CCFF;
    return def;
}

DamageTypeDef DamageTypeRegistry::preset_electric() {
    DamageTypeDef def;
    def.name = "Electric";
    def.category = DamageCategory::Electric;
    def.effect_chance = 0.15f;
    def.color = 0xFFFFFF00;
    return def;
}

DamageTypeDef DamageTypeRegistry::preset_poison() {
    DamageTypeDef def;
    def.name = "Poison";
    def.category = DamageCategory::Poison;
    def.effect_chance = 0.3f;
    def.color = 0xFF00FF00;
    return def;
}

DamageTypeDef DamageTypeRegistry::preset_true() {
    DamageTypeDef def;
    def.name = "True";
    def.category = DamageCategory::True;
    def.color = 0xFFFFFFFF;
    return def;
}

// =============================================================================
// DamageProcessor Implementation
// =============================================================================

DamageProcessor::DamageProcessor() = default;

DamageProcessor::DamageProcessor(DamageTypeRegistry* types)
    : m_types(types) {
}

DamageProcessor::~DamageProcessor() = default;

DamageResult DamageProcessor::calculate_damage(const DamageInfo& info, const VitalsComponent& target) const {
    DamageResult result;
    result.health_before = target.health().health();

    float damage = info.base_damage * m_global_multiplier;

    // Apply critical
    if (has_flag(info.flags, DamageFlags::Critical)) {
        damage *= m_crit_multiplier;
        result.was_critical = true;
    }

    // Apply headshot
    if (has_flag(info.flags, DamageFlags::Headshot)) {
        damage *= m_headshot_multiplier;
        result.was_headshot = true;
    }

    // Store final damage (before armor/shield)
    result.final_damage = damage;

    return result;
}

DamageResult DamageProcessor::apply_damage(const DamageInfo& info, VitalsComponent& target) {
    DamageInfo modified_info = info;

    // Calculate modifiers
    float damage = info.base_damage * m_global_multiplier;

    if (has_flag(info.flags, DamageFlags::Critical)) {
        damage *= m_crit_multiplier;
        modified_info.flags = modified_info.flags | DamageFlags::Critical;
    }

    if (has_flag(info.flags, DamageFlags::Headshot)) {
        damage *= m_headshot_multiplier;
        modified_info.flags = modified_info.flags | DamageFlags::Headshot;
    }

    modified_info.final_damage = damage;

    return target.apply_damage(modified_info);
}

// =============================================================================
// ProjectileSystem Implementation
// =============================================================================

ProjectileSystem::ProjectileSystem() = default;
ProjectileSystem::~ProjectileSystem() = default;

ProjectileId ProjectileSystem::spawn(const ProjectileConfig& config, const void_math::Vec3& origin,
                                     const void_math::Vec3& direction, EntityId owner) {
    ProjectileId id{m_next_id++};

    ProjectileData data;
    data.id = id;
    data.config = config;
    data.state.position = origin;
    data.state.direction = direction;
    data.state.velocity = {
        direction.x * config.speed,
        direction.y * config.speed,
        direction.z * config.speed
    };
    data.state.lifetime_remaining = config.lifetime;
    data.state.owner = owner;
    data.state.active = true;

    m_projectiles.push_back(std::move(data));

    return id;
}

void ProjectileSystem::destroy(ProjectileId id) {
    m_projectiles.erase(
        std::remove_if(m_projectiles.begin(), m_projectiles.end(),
            [id](const ProjectileData& p) { return p.id == id; }),
        m_projectiles.end());
}

const ProjectileState* ProjectileSystem::get_state(ProjectileId id) const {
    for (const auto& proj : m_projectiles) {
        if (proj.id == id) {
            return &proj.state;
        }
    }
    return nullptr;
}

void ProjectileSystem::update(float dt) {
    auto it = m_projectiles.begin();
    while (it != m_projectiles.end()) {
        auto& proj = *it;

        if (!proj.state.active) {
            it = m_projectiles.erase(it);
            continue;
        }

        // Update lifetime
        proj.state.lifetime_remaining -= dt;
        if (proj.state.lifetime_remaining <= 0) {
            it = m_projectiles.erase(it);
            continue;
        }

        // Apply gravity
        proj.state.velocity.y -= proj.config.gravity * dt;

        // Store old position for raycast
        void_math::Vec3 old_pos = proj.state.position;

        // Update position
        proj.state.position.x += proj.state.velocity.x * dt;
        proj.state.position.y += proj.state.velocity.y * dt;
        proj.state.position.z += proj.state.velocity.z * dt;

        // Check for hits
        if (m_raycast) {
            void_math::Vec3 hit_point, hit_normal;
            EntityId hit_entity;

            if (m_raycast(old_pos, proj.state.position, hit_point, hit_normal, hit_entity)) {
                // Hit something
                if (m_on_hit) {
                    m_on_hit(proj.id, hit_entity, hit_point, proj.config.damage);
                }

                if (proj.config.destroy_on_hit) {
                    if (proj.state.penetrations >= proj.config.max_penetrations) {
                        it = m_projectiles.erase(it);
                        continue;
                    }
                    proj.state.penetrations++;
                }
            }
        }

        // Homing behavior
        if (proj.config.homing && proj.state.target) {
            // Would need target position lookup
        }

        ++it;
    }
}

void ProjectileSystem::clear() {
    m_projectiles.clear();
}

// =============================================================================
// HitDetection Implementation
// =============================================================================

HitDetection::HitDetection() = default;
HitDetection::~HitDetection() = default;

bool HitDetection::validate_hit(const DamageInfo& /*info*/) const {
    // Basic validation - could add more checks
    return true;
}

bool HitDetection::is_headshot(const void_math::Vec3& hit_position, EntityId target) const {
    if (!m_get_head_pos) return false;

    void_math::Vec3 head_pos = m_get_head_pos(target);
    float dx = hit_position.x - head_pos.x;
    float dy = hit_position.y - head_pos.y;
    float dz = hit_position.z - head_pos.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    return dist <= m_headshot_radius;
}

bool HitDetection::is_backstab(EntityId attacker, EntityId victim) const {
    if (!m_get_position || !m_get_forward) return false;

    void_math::Vec3 attacker_pos = m_get_position(attacker);
    void_math::Vec3 victim_pos = m_get_position(victim);
    void_math::Vec3 victim_forward = m_get_forward(victim);

    // Direction from victim to attacker
    float dx = attacker_pos.x - victim_pos.x;
    float dz = attacker_pos.z - victim_pos.z;
    float len = std::sqrt(dx * dx + dz * dz);
    if (len > 0) {
        dx /= len;
        dz /= len;
    }

    // Dot product with victim's forward (negative = behind)
    float dot = victim_forward.x * dx + victim_forward.z * dz;

    // Convert backstab angle to cos
    float cos_angle = std::cos(m_backstab_angle * 3.14159f / 180.0f);

    return dot < -cos_angle;  // Attacker is behind victim
}

// =============================================================================
// KillTracker Implementation
// =============================================================================

KillTracker::KillTracker() = default;
KillTracker::~KillTracker() = default;

void KillTracker::register_damage(EntityId attacker, EntityId victim, float damage) {
    DamageRecord record;
    record.attacker = attacker;
    record.damage = damage;
    record.timestamp = m_current_time;
    m_damage_history[victim].push_back(record);

    // Track stats
    m_stats[attacker].total_damage_dealt += damage;
    m_stats[victim].total_damage_taken += damage;
}

KillEvent KillTracker::record_kill(EntityId killer, EntityId victim, const DamageInfo& final_blow) {
    KillEvent event;
    event.killer = killer;
    event.victim = victim;
    event.weapon = final_blow.weapon;
    event.final_damage_type = final_blow.damage_type;
    event.was_headshot = has_flag(final_blow.flags, DamageFlags::Headshot);
    event.was_critical = has_flag(final_blow.flags, DamageFlags::Critical);
    event.timestamp = m_current_time;

    // Calculate total damage and assists
    auto& history = m_damage_history[victim];
    float total_damage = 0;
    std::unordered_map<std::uint64_t, float> damage_by_attacker;

    for (const auto& record : history) {
        if (m_current_time - record.timestamp <= m_assist_window) {
            total_damage += record.damage;
            damage_by_attacker[record.attacker.value] += record.damage;
        }
    }

    event.total_damage_dealt = total_damage;

    // Find assists (excluding killer)
    for (const auto& [attacker_id, damage] : damage_by_attacker) {
        if (attacker_id != killer.value && damage / total_damage >= m_assist_threshold) {
            event.assists.push_back(EntityId{attacker_id});
        }
    }

    // Update stats
    m_stats[killer].kills++;
    m_stats[victim].deaths++;
    for (const auto& assist : event.assists) {
        m_stats[assist].assists++;
    }

    // Clear damage history for victim
    m_damage_history.erase(victim);

    if (m_on_kill) {
        m_on_kill(event);
    }

    return event;
}

void KillTracker::clear_history(EntityId entity) {
    m_damage_history.erase(entity);
}

KillTracker::KillStats KillTracker::get_stats(EntityId entity) const {
    auto it = m_stats.find(entity);
    return it != m_stats.end() ? it->second : KillStats{};
}

// =============================================================================
// CombatSystem Implementation
// =============================================================================

CombatSystem::CombatSystem()
    : CombatSystem(CombatConfig{}) {
}

CombatSystem::CombatSystem(const CombatConfig& config)
    : m_config(config)
    , m_damage_processor(&m_damage_types) {
    m_damage_processor.set_critical_multiplier(config.base_critical_multiplier);
    m_damage_processor.set_headshot_multiplier(config.headshot_multiplier);
    m_damage_processor.set_global_damage_multiplier(config.global_damage_multiplier);

    setup_preset_damage_types();
    setup_preset_status_effects();
}

CombatSystem::~CombatSystem() = default;

DamageResult CombatSystem::apply_damage(const DamageInfo& info, VitalsComponent& target) {
    auto result = m_damage_processor.apply_damage(info, target);

    m_stats.total_damage_events++;

    // Register damage for assist tracking
    m_kill_tracker.register_damage(info.attacker, info.victim, result.damage_dealt);

    // Check for kill
    if (result.was_fatal) {
        auto kill_event = m_kill_tracker.record_kill(info.attacker, info.victim, info);
        m_stats.total_kills++;

        if (m_on_kill) {
            m_on_kill(kill_event);
        }

        if (m_on_death) {
            DeathEvent death_event;
            death_event.entity = info.victim;
            death_event.kill_event = kill_event;
            death_event.death_position = info.hit_position;
            death_event.respawn_time = m_config.default_respawn_time;
            m_on_death(death_event);
        }
    }

    if (m_on_damage) {
        HitEvent hit_event;
        hit_event.damage_info = info;
        hit_event.result = result;
        m_on_damage(hit_event);
    }

    return result;
}

void CombatSystem::apply_status_effect(StatusEffectId effect, EntityId /*target*/, EntityId /*source*/) {
    // This would typically look up the target's StatusEffectComponent and apply
    // For now, the application is handled by the component directly
    (void)effect;
}

void CombatSystem::update(float dt) {
    m_projectiles.update(dt);
}

void CombatSystem::set_config(const CombatConfig& config) {
    m_config = config;
    m_damage_processor.set_critical_multiplier(config.base_critical_multiplier);
    m_damage_processor.set_headshot_multiplier(config.headshot_multiplier);
    m_damage_processor.set_global_damage_multiplier(config.global_damage_multiplier);
}

CombatSystem::Stats CombatSystem::stats() const {
    Stats s = m_stats;
    s.active_projectiles = m_projectiles.active_count();
    return s;
}

CombatSystem::Snapshot CombatSystem::take_snapshot() const {
    return Snapshot{};
}

void CombatSystem::apply_snapshot(const Snapshot& /*snapshot*/) {
    // Restore state from snapshot
}

void CombatSystem::setup_preset_damage_types() {
    m_damage_types.register_type(DamageTypeRegistry::preset_physical());
    m_damage_types.register_type(DamageTypeRegistry::preset_fire());
    m_damage_types.register_type(DamageTypeRegistry::preset_ice());
    m_damage_types.register_type(DamageTypeRegistry::preset_electric());
    m_damage_types.register_type(DamageTypeRegistry::preset_poison());
    m_damage_types.register_type(DamageTypeRegistry::preset_true());
}

void CombatSystem::setup_preset_status_effects() {
    m_status_effects.register_effect(StatusEffectRegistry::preset_burning());
    m_status_effects.register_effect(StatusEffectRegistry::preset_poison());
    m_status_effects.register_effect(StatusEffectRegistry::preset_frozen());
    m_status_effects.register_effect(StatusEffectRegistry::preset_stunned());
    m_status_effects.register_effect(StatusEffectRegistry::preset_bleeding());
    m_status_effects.register_effect(StatusEffectRegistry::preset_regeneration());
    m_status_effects.register_effect(StatusEffectRegistry::preset_haste());
    m_status_effects.register_effect(StatusEffectRegistry::preset_slow());
    m_status_effects.register_effect(StatusEffectRegistry::preset_weakness());
    m_status_effects.register_effect(StatusEffectRegistry::preset_strength());
    m_status_effects.register_effect(StatusEffectRegistry::preset_shield());
    m_status_effects.register_effect(StatusEffectRegistry::preset_invulnerable());
}

} // namespace void_combat
