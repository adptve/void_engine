/// @file perception.cpp
/// @brief Perception system implementation for void_ai module

#include <void_engine/ai/perception.hpp>

#include <algorithm>
#include <cmath>

namespace void_ai {

// =============================================================================
// Utility Functions
// =============================================================================

namespace {

float vec3_length(const void_math::Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

void_math::Vec3 vec3_normalize(const void_math::Vec3& v) {
    float len = vec3_length(v);
    if (len > 1e-6f) {
        return {v.x / len, v.y / len, v.z / len};
    }
    return v;
}

float vec3_dot(const void_math::Vec3& a, const void_math::Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void_math::Vec3 vec3_sub(const void_math::Vec3& a, const void_math::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

float vec3_distance(const void_math::Vec3& a, const void_math::Vec3& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

constexpr float DEG_TO_RAD = 3.14159265358979f / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / 3.14159265358979f;

} // anonymous namespace

bool is_in_fov(const void_math::Vec3& forward,
               const void_math::Vec3& to_target,
               float fov_degrees) {
    float len = vec3_length(to_target);
    if (len < 1e-6f) return true;

    auto dir = vec3_normalize(to_target);
    float cos_angle = vec3_dot(forward, dir);
    float angle = std::acos(std::clamp(cos_angle, -1.0f, 1.0f)) * RAD_TO_DEG;

    return angle <= fov_degrees * 0.5f;
}

float angle_between(const void_math::Vec3& a, const void_math::Vec3& b) {
    float len_a = vec3_length(a);
    float len_b = vec3_length(b);
    if (len_a < 1e-6f || len_b < 1e-6f) return 0;

    float cos_angle = vec3_dot(a, b) / (len_a * len_b);
    return std::acos(std::clamp(cos_angle, -1.0f, 1.0f)) * RAD_TO_DEG;
}

float attenuate_sound(float loudness, float distance, float max_range) {
    if (distance >= max_range) return 0;
    if (distance <= 0) return loudness;

    // Inverse distance attenuation
    float attenuation = 1.0f - (distance / max_range);
    return loudness * attenuation * attenuation;
}

// =============================================================================
// SightSense Implementation
// =============================================================================

SightSense::SightSense() = default;

SightSense::SightSense(const SightConfig& config)
    : m_config(config) {
}

std::vector<Stimulus> SightSense::update(
    const void_math::Vec3& perceiver_position,
    const void_math::Vec3& perceiver_forward,
    const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& targets) {

    std::vector<Stimulus> stimuli;

    for (const auto& [target_id, target_pos] : targets) {
        float strength = 0;
        if (is_in_view(perceiver_position, perceiver_forward, target_pos, strength)) {
            // Check line of sight if enabled
            if (m_config.use_los_check && m_los_check) {
                if (!m_los_check(perceiver_position, target_pos)) {
                    continue;  // LOS blocked
                }
            }

            Stimulus stim;
            stim.type = StimulusType::Visual;
            stim.location = target_pos;
            stim.direction = vec3_normalize(vec3_sub(target_pos, perceiver_position));
            stim.strength = strength * m_range_multiplier;
            stim.source_id = target_id;
            stim.max_age = m_config.lose_sight_time;
            stimuli.push_back(stim);
        }
    }

    return stimuli;
}

bool SightSense::is_in_view(const void_math::Vec3& perceiver_pos,
                           const void_math::Vec3& perceiver_fwd,
                           const void_math::Vec3& target_pos,
                           float& out_strength) const {
    auto to_target = vec3_sub(target_pos, perceiver_pos);
    float dist = vec3_length(to_target);

    if (dist < 1e-6f) {
        out_strength = 1.0f;
        return true;
    }

    auto dir = vec3_normalize(to_target);
    float cos_angle = vec3_dot(perceiver_fwd, dir);
    float angle = std::acos(std::clamp(cos_angle, -1.0f, 1.0f)) * RAD_TO_DEG;

    // Check main vision cone
    if (dist <= m_config.view_distance && angle <= m_config.view_angle * 0.5f) {
        // Stronger the closer and more centered
        float dist_factor = 1.0f - (dist / m_config.view_distance);
        float angle_factor = 1.0f - (angle / (m_config.view_angle * 0.5f));
        out_strength = dist_factor * (0.5f + 0.5f * angle_factor);
        return true;
    }

    // Check peripheral vision
    if (dist <= m_config.peripheral_distance && angle <= m_config.peripheral_angle * 0.5f) {
        float dist_factor = 1.0f - (dist / m_config.peripheral_distance);
        out_strength = dist_factor * 0.3f;  // Peripheral detection is weaker
        return true;
    }

    return false;
}

// =============================================================================
// HearingSense Implementation
// =============================================================================

HearingSense::HearingSense() = default;

HearingSense::HearingSense(const HearingConfig& config)
    : m_config(config) {
}

std::vector<Stimulus> HearingSense::update(
    const void_math::Vec3& perceiver_position,
    const void_math::Vec3& /*perceiver_forward*/,
    const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& /*targets*/) {

    std::vector<Stimulus> stimuli;

    // Process sound events
    for (auto& event : m_sound_events) {
        float dist = vec3_distance(perceiver_position, event.position);

        // Calculate effective loudness
        float effective_range = m_config.max_range * m_range_multiplier;
        float loudness = attenuate_sound(event.loudness * m_config.loudness_scale,
                                         dist, effective_range);

        // Check for sound blocking
        if (m_config.blocked_by_walls && m_block_check) {
            float block_factor = m_block_check(perceiver_position, event.position);
            loudness *= (1.0f - block_factor);
        }

        if (loudness > 0.01f) {
            Stimulus stim;
            stim.type = StimulusType::Audio;
            stim.location = event.position;
            stim.direction = vec3_normalize(vec3_sub(event.position, perceiver_position));
            stim.strength = loudness;
            stim.age = event.age;
            stim.max_age = event.max_age;
            stim.source_id = event.source;
            stimuli.push_back(stim);
        }
    }

    return stimuli;
}

void HearingSense::add_sound_event(const void_math::Vec3& position,
                                   float loudness,
                                   PerceptionTargetId source,
                                   float duration) {
    SoundEvent event;
    event.position = position;
    event.loudness = loudness;
    event.source = source;
    event.max_age = duration;
    m_sound_events.push_back(event);
}

void HearingSense::clear_sound_events() {
    m_sound_events.clear();
}

// =============================================================================
// DamageSense Implementation
// =============================================================================

DamageSense::DamageSense() = default;

DamageSense::DamageSense(const DamageConfig& config)
    : m_config(config) {
}

std::vector<Stimulus> DamageSense::update(
    const void_math::Vec3& /*perceiver_position*/,
    const void_math::Vec3& /*perceiver_forward*/,
    const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& /*targets*/) {

    std::vector<Stimulus> stimuli;

    // Update ages and remove expired
    auto it = m_damage_events.begin();
    while (it != m_damage_events.end()) {
        Stimulus stim;
        stim.type = StimulusType::Damage;
        stim.location = it->position;
        stim.direction = it->direction;
        stim.strength = it->amount * (1.0f - it->age / m_config.memory_time);
        stim.age = it->age;
        stim.max_age = m_config.memory_time;
        stim.source_id = it->source;
        stimuli.push_back(stim);

        ++it;
    }

    return stimuli;
}

void DamageSense::register_damage(const void_math::Vec3& damage_position,
                                  const void_math::Vec3& damage_direction,
                                  float damage_amount,
                                  PerceptionTargetId source) {
    DamageEvent event;
    event.position = damage_position;
    event.direction = damage_direction;
    event.amount = damage_amount;
    event.source = source;
    m_damage_events.push_back(event);
}

void DamageSense::clear_damage_events() {
    m_damage_events.clear();
}

// =============================================================================
// ProximitySense Implementation
// =============================================================================

ProximitySense::ProximitySense() = default;

ProximitySense::ProximitySense(const ProximityConfig& config)
    : m_config(config) {
}

std::vector<Stimulus> ProximitySense::update(
    const void_math::Vec3& perceiver_position,
    const void_math::Vec3& /*perceiver_forward*/,
    const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& targets) {

    std::vector<Stimulus> stimuli;
    float range = m_config.range * m_range_multiplier;

    for (const auto& [target_id, target_pos] : targets) {
        float dist = vec3_distance(perceiver_position, target_pos);

        if (dist <= range) {
            // Check LOS if required
            if (m_config.los_required && m_los_check) {
                if (!m_los_check(perceiver_position, target_pos)) {
                    continue;
                }
            }

            Stimulus stim;
            stim.type = StimulusType::Proximity;
            stim.location = target_pos;
            stim.direction = vec3_normalize(vec3_sub(target_pos, perceiver_position));
            stim.strength = 1.0f - (dist / range);
            stim.source_id = target_id;
            stimuli.push_back(stim);
        }
    }

    return stimuli;
}

// =============================================================================
// PerceptionComponent Implementation
// =============================================================================

PerceptionComponent::PerceptionComponent() = default;
PerceptionComponent::~PerceptionComponent() = default;

void PerceptionComponent::add_sense(std::unique_ptr<ISense> sense) {
    m_senses.push_back(std::move(sense));
}

void PerceptionComponent::remove_sense(SenseType type) {
    m_senses.erase(
        std::remove_if(m_senses.begin(), m_senses.end(),
            [type](const auto& s) { return s->type() == type; }),
        m_senses.end());
}

ISense* PerceptionComponent::get_sense(SenseType type) {
    for (auto& sense : m_senses) {
        if (sense->type() == type) {
            return sense.get();
        }
    }
    return nullptr;
}

void PerceptionComponent::clear_senses() {
    m_senses.clear();
}

void PerceptionComponent::setup_default_senses() {
    add_sense(std::make_unique<SightSense>());
    add_sense(std::make_unique<HearingSense>());
    add_sense(std::make_unique<DamageSense>());
    add_sense(std::make_unique<ProximitySense>());
}

void PerceptionComponent::update(float dt,
                                  const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& targets) {
    std::vector<Stimulus> all_stimuli;

    // Update all senses
    for (auto& sense : m_senses) {
        if (!sense->is_enabled()) continue;

        auto stimuli = sense->update(m_position, m_forward, targets);
        all_stimuli.insert(all_stimuli.end(), stimuli.begin(), stimuli.end());
    }

    // Process stimuli
    process_stimuli(all_stimuli, dt);

    // Update known targets (age out old ones)
    update_known_targets(dt);
}

bool PerceptionComponent::knows_target(PerceptionTargetId id) const {
    for (const auto& target : m_known_targets) {
        if (target.target_id == id) {
            return true;
        }
    }
    return false;
}

const KnownTarget* PerceptionComponent::get_known_target(PerceptionTargetId id) const {
    for (const auto& target : m_known_targets) {
        if (target.target_id == id) {
            return &target;
        }
    }
    return nullptr;
}

KnownTarget* PerceptionComponent::get_known_target(PerceptionTargetId id) {
    for (auto& target : m_known_targets) {
        if (target.target_id == id) {
            return &target;
        }
    }
    return nullptr;
}

const KnownTarget* PerceptionComponent::highest_threat() const {
    const KnownTarget* best = nullptr;
    float best_strength = 0;

    for (const auto& target : m_known_targets) {
        if (target.strength > best_strength) {
            best_strength = target.strength;
            best = &target;
        }
    }

    return best;
}

const KnownTarget* PerceptionComponent::nearest_target() const {
    const KnownTarget* best = nullptr;
    float best_dist = std::numeric_limits<float>::max();

    for (const auto& target : m_known_targets) {
        float dist = vec3_distance(m_position, target.last_known_position);
        if (dist < best_dist) {
            best_dist = dist;
            best = &target;
        }
    }

    return best;
}

void PerceptionComponent::process_stimuli(const std::vector<Stimulus>& stimuli, float /*dt*/) {
    // Group stimuli by source
    std::unordered_map<std::uint32_t, std::vector<const Stimulus*>> by_source;
    for (const auto& stim : stimuli) {
        by_source[stim.source_id.value].push_back(&stim);
    }

    // Update known targets
    for (const auto& [source_id, source_stimuli] : by_source) {
        PerceptionTargetId target_id{source_id};

        // Find or create known target
        KnownTarget* known = get_known_target(target_id);
        bool is_new = (known == nullptr);

        if (is_new) {
            // Add new target
            if (m_known_targets.size() >= m_max_known_targets) {
                // Remove weakest target
                auto weakest = std::min_element(m_known_targets.begin(), m_known_targets.end(),
                    [](const KnownTarget& a, const KnownTarget& b) {
                        return a.strength < b.strength;
                    });
                if (weakest != m_known_targets.end()) {
                    fire_lost_event(*weakest);
                    m_known_targets.erase(weakest);
                }
            }

            KnownTarget new_target;
            new_target.target_id = target_id;
            m_known_targets.push_back(new_target);
            known = &m_known_targets.back();
        }

        // Update known target info
        float best_strength = 0;
        const Stimulus* best_stim = nullptr;
        std::uint32_t senses_mask = 0;

        for (const Stimulus* stim : source_stimuli) {
            if (stim->strength > best_strength) {
                best_strength = stim->strength;
                best_stim = stim;
            }
            senses_mask |= (1u << static_cast<int>(stim->type));
        }

        if (best_stim) {
            known->last_known_position = best_stim->location;
            known->strength = best_strength;
            known->last_seen_time = 0;
            known->currently_sensed = true;
            known->senses_mask = senses_mask;
        }

        if (is_new && best_stim) {
            fire_gained_event(*known, *best_stim);
        }
    }

    // Mark targets not in current stimuli as not currently sensed
    for (auto& target : m_known_targets) {
        if (by_source.find(target.target_id.value) == by_source.end()) {
            target.currently_sensed = false;
        }
    }
}

void PerceptionComponent::update_known_targets(float dt) {
    auto it = m_known_targets.begin();
    while (it != m_known_targets.end()) {
        if (!it->currently_sensed) {
            it->last_seen_time += dt;

            // Decay strength
            it->strength *= (1.0f - dt / m_forget_time);

            if (it->last_seen_time >= m_forget_time || it->strength < 0.01f) {
                fire_lost_event(*it);
                it = m_known_targets.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void PerceptionComponent::fire_gained_event(const KnownTarget& target, const Stimulus& stimulus) {
    if (m_on_gained) {
        PerceptionEvent event;
        event.sense = static_cast<SenseType>(stimulus.type);
        event.stimulus = stimulus;
        event.target_id = target.target_id;
        event.gained = true;
        event.strength = target.strength;
        m_on_gained(event);
    }
}

void PerceptionComponent::fire_lost_event(const KnownTarget& target) {
    if (m_on_lost) {
        PerceptionEvent event;
        event.sense = SenseType::Sight;  // Default
        event.target_id = target.target_id;
        event.gained = false;
        event.strength = 0;
        m_on_lost(event);
    }
}

// =============================================================================
// StimulusSource Implementation
// =============================================================================

StimulusSource::StimulusSource() = default;

void StimulusSource::add_tag(std::string_view tag) {
    std::string tag_str(tag);
    if (std::find(m_tags.begin(), m_tags.end(), tag_str) == m_tags.end()) {
        m_tags.push_back(tag_str);
    }
}

void StimulusSource::remove_tag(std::string_view tag) {
    m_tags.erase(std::remove(m_tags.begin(), m_tags.end(), std::string(tag)), m_tags.end());
}

bool StimulusSource::has_tag(std::string_view tag) const {
    return std::find(m_tags.begin(), m_tags.end(), std::string(tag)) != m_tags.end();
}

// =============================================================================
// PerceptionSystem Implementation
// =============================================================================

PerceptionSystem::PerceptionSystem() = default;
PerceptionSystem::~PerceptionSystem() = default;

PerceptionComponent* PerceptionSystem::create_perceiver() {
    m_perceivers.push_back(std::make_unique<PerceptionComponent>());
    return m_perceivers.back().get();
}

void PerceptionSystem::destroy_perceiver(PerceptionComponent* perceiver) {
    m_perceivers.erase(
        std::remove_if(m_perceivers.begin(), m_perceivers.end(),
            [perceiver](const auto& p) { return p.get() == perceiver; }),
        m_perceivers.end());
}

StimulusSource* PerceptionSystem::create_stimulus_source() {
    m_sources.push_back(std::make_unique<StimulusSource>());
    return m_sources.back().get();
}

void PerceptionSystem::destroy_stimulus_source(StimulusSource* source) {
    // Remove from target map
    if (source->target_id()) {
        m_target_map.erase(source->target_id());
    }

    m_sources.erase(
        std::remove_if(m_sources.begin(), m_sources.end(),
            [source](const auto& s) { return s.get() == source; }),
        m_sources.end());
}

PerceptionTargetId PerceptionSystem::register_target(StimulusSource* source) {
    PerceptionTargetId id{m_next_target_id++};
    source->set_target_id(id);
    m_target_map[id] = source;
    return id;
}

void PerceptionSystem::unregister_target(PerceptionTargetId id) {
    auto it = m_target_map.find(id);
    if (it != m_target_map.end()) {
        it->second->set_target_id(PerceptionTargetId{});
        m_target_map.erase(it);
    }
}

void PerceptionSystem::update(float dt) {
    auto targets = gather_targets();

    for (auto& perceiver : m_perceivers) {
        perceiver->update(dt, targets);
    }
}

std::vector<StimulusSource*> PerceptionSystem::get_sources_in_radius(
    const void_math::Vec3& center,
    float radius) const {
    std::vector<StimulusSource*> result;

    for (const auto& source : m_sources) {
        float dist = vec3_distance(center, source->position());
        if (dist <= radius) {
            result.push_back(source.get());
        }
    }

    return result;
}

void PerceptionSystem::set_team_relation(std::uint32_t team_a, std::uint32_t team_b, bool hostile) {
    auto key = make_team_key(team_a, team_b);
    m_team_relations[key] = hostile;
}

bool PerceptionSystem::is_hostile(std::uint32_t team_a, std::uint32_t team_b) const {
    if (team_a == team_b) return false;

    auto key = make_team_key(team_a, team_b);
    auto it = m_team_relations.find(key);
    return it != m_team_relations.end() ? it->second : true;  // Default hostile
}

void PerceptionSystem::broadcast_sound(const void_math::Vec3& position,
                                        float loudness,
                                        PerceptionTargetId source,
                                        float duration) {
    // Add sound to all perceivers' hearing senses
    for (auto& perceiver : m_perceivers) {
        auto* hearing = static_cast<HearingSense*>(perceiver->get_sense(SenseType::Hearing));
        if (hearing) {
            hearing->add_sound_event(position, loudness, source, duration);
        }
    }
}

std::uint64_t PerceptionSystem::make_team_key(std::uint32_t a, std::uint32_t b) const {
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint64_t>(a) << 32) | b;
}

std::vector<std::pair<PerceptionTargetId, void_math::Vec3>> PerceptionSystem::gather_targets() const {
    std::vector<std::pair<PerceptionTargetId, void_math::Vec3>> targets;
    targets.reserve(m_sources.size());

    for (const auto& source : m_sources) {
        if (source->target_id() && source->is_visible()) {
            targets.emplace_back(source->target_id(), source->position());
        }
    }

    return targets;
}

} // namespace void_ai
