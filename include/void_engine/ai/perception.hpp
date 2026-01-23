/// @file perception.hpp
/// @brief AI perception and sensing system

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace void_ai {

// =============================================================================
// Sense Interface
// =============================================================================

/// @brief Base interface for AI senses
class ISense {
public:
    virtual ~ISense() = default;

    /// @brief Get the sense type
    virtual SenseType type() const = 0;

    /// @brief Get sense name
    virtual std::string_view name() const = 0;

    /// @brief Update the sense (check for stimuli)
    /// @param perceiver Position and forward direction of the perceiver
    /// @param targets Available targets to sense
    /// @return List of perceived stimuli
    virtual std::vector<Stimulus> update(
        const void_math::Vec3& perceiver_position,
        const void_math::Vec3& perceiver_forward,
        const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& targets) = 0;

    /// @brief Enable/disable the sense
    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

    /// @brief Set sense range multiplier
    void set_range_multiplier(float mult) { m_range_multiplier = mult; }
    float range_multiplier() const { return m_range_multiplier; }

protected:
    bool m_enabled{true};
    float m_range_multiplier{1.0f};
};

// =============================================================================
// Sight Sense
// =============================================================================

/// @brief Visual perception sense
class SightSense : public ISense {
public:
    SightSense();
    explicit SightSense(const SightConfig& config);

    SenseType type() const override { return SenseType::Sight; }
    std::string_view name() const override { return "Sight"; }

    std::vector<Stimulus> update(
        const void_math::Vec3& perceiver_position,
        const void_math::Vec3& perceiver_forward,
        const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& targets) override;

    // Configuration
    void set_config(const SightConfig& config) { m_config = config; }
    const SightConfig& config() const { return m_config; }

    // Line of sight callback (for raycast checks)
    using LOSCheck = std::function<bool(const void_math::Vec3& from, const void_math::Vec3& to)>;
    void set_los_check(LOSCheck check) { m_los_check = std::move(check); }

private:
    SightConfig m_config;
    LOSCheck m_los_check;

    bool is_in_view(const void_math::Vec3& perceiver_pos,
                   const void_math::Vec3& perceiver_fwd,
                   const void_math::Vec3& target_pos,
                   float& out_strength) const;
};

// =============================================================================
// Hearing Sense
// =============================================================================

/// @brief Audio perception sense
class HearingSense : public ISense {
public:
    HearingSense();
    explicit HearingSense(const HearingConfig& config);

    SenseType type() const override { return SenseType::Hearing; }
    std::string_view name() const override { return "Hearing"; }

    std::vector<Stimulus> update(
        const void_math::Vec3& perceiver_position,
        const void_math::Vec3& perceiver_forward,
        const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& targets) override;

    // Sound events
    void add_sound_event(const void_math::Vec3& position,
                        float loudness,
                        PerceptionTargetId source,
                        float duration = 1.0f);
    void clear_sound_events();

    // Configuration
    void set_config(const HearingConfig& config) { m_config = config; }
    const HearingConfig& config() const { return m_config; }

    // Sound blocking callback
    using SoundBlockCheck = std::function<float(const void_math::Vec3& from, const void_math::Vec3& to)>;
    void set_block_check(SoundBlockCheck check) { m_block_check = std::move(check); }

private:
    struct SoundEvent {
        void_math::Vec3 position;
        float loudness;
        PerceptionTargetId source;
        float age{0};
        float max_age{1.0f};
    };

    HearingConfig m_config;
    std::vector<SoundEvent> m_sound_events;
    SoundBlockCheck m_block_check;
};

// =============================================================================
// Damage Sense
// =============================================================================

/// @brief Damage perception sense (knows about damage sources)
class DamageSense : public ISense {
public:
    DamageSense();
    explicit DamageSense(const DamageConfig& config);

    SenseType type() const override { return SenseType::Damage; }
    std::string_view name() const override { return "Damage"; }

    std::vector<Stimulus> update(
        const void_math::Vec3& perceiver_position,
        const void_math::Vec3& perceiver_forward,
        const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& targets) override;

    // Damage events
    void register_damage(const void_math::Vec3& damage_position,
                        const void_math::Vec3& damage_direction,
                        float damage_amount,
                        PerceptionTargetId source);
    void clear_damage_events();

    // Configuration
    void set_config(const DamageConfig& config) { m_config = config; }
    const DamageConfig& config() const { return m_config; }

private:
    struct DamageEvent {
        void_math::Vec3 position;
        void_math::Vec3 direction;
        float amount;
        PerceptionTargetId source;
        float age{0};
    };

    DamageConfig m_config;
    std::vector<DamageEvent> m_damage_events;
};

// =============================================================================
// Proximity Sense
// =============================================================================

/// @brief Simple distance-based detection
class ProximitySense : public ISense {
public:
    ProximitySense();
    explicit ProximitySense(const ProximityConfig& config);

    SenseType type() const override { return SenseType::Proximity; }
    std::string_view name() const override { return "Proximity"; }

    std::vector<Stimulus> update(
        const void_math::Vec3& perceiver_position,
        const void_math::Vec3& perceiver_forward,
        const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& targets) override;

    // Configuration
    void set_config(const ProximityConfig& config) { m_config = config; }
    const ProximityConfig& config() const { return m_config; }

    // LOS check (optional)
    using LOSCheck = std::function<bool(const void_math::Vec3& from, const void_math::Vec3& to)>;
    void set_los_check(LOSCheck check) { m_los_check = std::move(check); }

private:
    ProximityConfig m_config;
    LOSCheck m_los_check;
};

// =============================================================================
// Perception Component
// =============================================================================

/// @brief Component that manages an entity's perception
class PerceptionComponent {
public:
    PerceptionComponent();
    ~PerceptionComponent();

    // Sense management
    void add_sense(std::unique_ptr<ISense> sense);
    void remove_sense(SenseType type);
    ISense* get_sense(SenseType type);
    void clear_senses();

    // Default sense setup
    void setup_default_senses();

    // Update position
    void set_position(const void_math::Vec3& position) { m_position = position; }
    void set_forward(const void_math::Vec3& forward) { m_forward = forward; }
    const void_math::Vec3& position() const { return m_position; }
    const void_math::Vec3& forward() const { return m_forward; }

    // Team affiliation
    void set_team(std::uint32_t team) { m_team = team; }
    std::uint32_t team() const { return m_team; }

    // Update perception
    void update(float dt, const std::vector<std::pair<PerceptionTargetId, void_math::Vec3>>& targets);

    // Known targets
    const std::vector<KnownTarget>& known_targets() const { return m_known_targets; }
    bool knows_target(PerceptionTargetId id) const;
    const KnownTarget* get_known_target(PerceptionTargetId id) const;
    KnownTarget* get_known_target(PerceptionTargetId id);

    // Get highest threat
    const KnownTarget* highest_threat() const;

    // Get nearest known target
    const KnownTarget* nearest_target() const;

    // Events
    using PerceptionCallback = std::function<void(const PerceptionEvent&)>;
    void on_target_gained(PerceptionCallback callback) { m_on_gained = std::move(callback); }
    void on_target_lost(PerceptionCallback callback) { m_on_lost = std::move(callback); }

    // Forget all targets
    void clear_known_targets() { m_known_targets.clear(); }

    // Settings
    void set_forget_time(float time) { m_forget_time = time; }
    float forget_time() const { return m_forget_time; }

    void set_max_known_targets(std::size_t max) { m_max_known_targets = max; }

private:
    std::vector<std::unique_ptr<ISense>> m_senses;
    std::vector<KnownTarget> m_known_targets;

    void_math::Vec3 m_position{};
    void_math::Vec3 m_forward{0, 0, 1};
    std::uint32_t m_team{0};

    float m_forget_time{10.0f};
    std::size_t m_max_known_targets{20};

    PerceptionCallback m_on_gained;
    PerceptionCallback m_on_lost;

    void process_stimuli(const std::vector<Stimulus>& stimuli, float dt);
    void update_known_targets(float dt);
    void fire_gained_event(const KnownTarget& target, const Stimulus& stimulus);
    void fire_lost_event(const KnownTarget& target);
};

// =============================================================================
// Stimulus Source
// =============================================================================

/// @brief Component that makes an entity perceptible
class StimulusSource {
public:
    StimulusSource();

    // Identity
    void set_target_id(PerceptionTargetId id) { m_target_id = id; }
    PerceptionTargetId target_id() const { return m_target_id; }

    // Team
    void set_team(std::uint32_t team) { m_team = team; }
    std::uint32_t team() const { return m_team; }

    // Position
    void set_position(const void_math::Vec3& pos) { m_position = pos; }
    const void_math::Vec3& position() const { return m_position; }

    // Velocity (for prediction)
    void set_velocity(const void_math::Vec3& vel) { m_velocity = vel; }
    const void_math::Vec3& velocity() const { return m_velocity; }

    // Visibility
    void set_visible(bool visible) { m_visible = visible; }
    bool is_visible() const { return m_visible; }

    void set_visibility_multiplier(float mult) { m_visibility_multiplier = mult; }
    float visibility_multiplier() const { return m_visibility_multiplier; }

    // Sound generation
    void set_noise_level(float level) { m_noise_level = level; }
    float noise_level() const { return m_noise_level; }

    // Tags for filtering
    void add_tag(std::string_view tag);
    void remove_tag(std::string_view tag);
    bool has_tag(std::string_view tag) const;

private:
    PerceptionTargetId m_target_id{};
    std::uint32_t m_team{0};
    void_math::Vec3 m_position{};
    void_math::Vec3 m_velocity{};
    bool m_visible{true};
    float m_visibility_multiplier{1.0f};
    float m_noise_level{0};
    std::vector<std::string> m_tags;
};

// =============================================================================
// Perception System
// =============================================================================

/// @brief High-level perception management system
class PerceptionSystem {
public:
    PerceptionSystem();
    ~PerceptionSystem();

    // Perceiver management
    PerceptionComponent* create_perceiver();
    void destroy_perceiver(PerceptionComponent* perceiver);

    // Stimulus source management
    StimulusSource* create_stimulus_source();
    void destroy_stimulus_source(StimulusSource* source);
    PerceptionTargetId register_target(StimulusSource* source);
    void unregister_target(PerceptionTargetId id);

    // Update all perceivers
    void update(float dt);

    // Query
    std::vector<StimulusSource*> get_sources_in_radius(const void_math::Vec3& center,
                                                       float radius) const;

    // Team management
    void set_team_relation(std::uint32_t team_a, std::uint32_t team_b, bool hostile);
    bool is_hostile(std::uint32_t team_a, std::uint32_t team_b) const;

    // Statistics
    std::size_t perceiver_count() const { return m_perceivers.size(); }
    std::size_t source_count() const { return m_sources.size(); }

    // Global sound events
    void broadcast_sound(const void_math::Vec3& position,
                        float loudness,
                        PerceptionTargetId source,
                        float duration = 1.0f);

private:
    std::vector<std::unique_ptr<PerceptionComponent>> m_perceivers;
    std::vector<std::unique_ptr<StimulusSource>> m_sources;
    std::unordered_map<PerceptionTargetId, StimulusSource*> m_target_map;
    std::uint32_t m_next_target_id{1};

    // Team relations (pair of team IDs -> hostile)
    std::unordered_map<std::uint64_t, bool> m_team_relations;

    std::uint64_t make_team_key(std::uint32_t a, std::uint32_t b) const;
    std::vector<std::pair<PerceptionTargetId, void_math::Vec3>> gather_targets() const;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Check if angle is within field of view
bool is_in_fov(const void_math::Vec3& forward,
               const void_math::Vec3& to_target,
               float fov_degrees);

/// @brief Calculate angle between two vectors (in degrees)
float angle_between(const void_math::Vec3& a, const void_math::Vec3& b);

/// @brief Attenuate sound based on distance
float attenuate_sound(float loudness, float distance, float max_range);

} // namespace void_ai
