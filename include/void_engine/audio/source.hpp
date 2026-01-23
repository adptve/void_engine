/// @file source.hpp
/// @brief Audio source interface and implementations for void_audio

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/math/math.hpp>

#include <functional>
#include <string>

namespace void_audio {

// =============================================================================
// Audio Source Interface
// =============================================================================

/// Interface for audio sources
class IAudioSource {
public:
    virtual ~IAudioSource() = default;

    // =========================================================================
    // Identity
    // =========================================================================

    /// Get source ID
    [[nodiscard]] virtual SourceId id() const = 0;

    /// Get source name
    [[nodiscard]] virtual const std::string& name() const = 0;

    /// Get/set user data
    [[nodiscard]] virtual void* user_data() const = 0;
    virtual void set_user_data(void* data) = 0;

    // =========================================================================
    // Playback Control
    // =========================================================================

    /// Play the source
    virtual void play() = 0;

    /// Pause the source
    virtual void pause() = 0;

    /// Stop the source
    virtual void stop() = 0;

    /// Get current playback state
    [[nodiscard]] virtual AudioState state() const = 0;

    /// Check if currently playing
    [[nodiscard]] virtual bool is_playing() const = 0;

    /// Check if paused
    [[nodiscard]] virtual bool is_paused() const = 0;

    /// Check if stopped
    [[nodiscard]] virtual bool is_stopped() const = 0;

    // =========================================================================
    // Buffer
    // =========================================================================

    /// Get attached buffer
    [[nodiscard]] virtual BufferId buffer() const = 0;

    /// Set buffer
    virtual void set_buffer(BufferId buffer) = 0;

    // =========================================================================
    // Volume and Pan
    // =========================================================================

    /// Get volume (0 to 1)
    [[nodiscard]] virtual float volume() const = 0;

    /// Set volume
    virtual void set_volume(float volume) = 0;

    /// Get pitch multiplier
    [[nodiscard]] virtual float pitch() const = 0;

    /// Set pitch
    virtual void set_pitch(float pitch) = 0;

    /// Get stereo pan (-1 left to 1 right)
    [[nodiscard]] virtual float pan() const = 0;

    /// Set pan
    virtual void set_pan(float pan) = 0;

    /// Check if muted
    [[nodiscard]] virtual bool is_muted() const = 0;

    /// Set muted
    virtual void set_muted(bool muted) = 0;

    // =========================================================================
    // Looping
    // =========================================================================

    /// Check if looping
    [[nodiscard]] virtual bool is_looping() const = 0;

    /// Set looping
    virtual void set_looping(bool loop) = 0;

    /// Get loop count (0 = infinite when looping)
    [[nodiscard]] virtual std::uint32_t loop_count() const = 0;

    /// Set loop count
    virtual void set_loop_count(std::uint32_t count) = 0;

    /// Get current loop iteration
    [[nodiscard]] virtual std::uint32_t current_loop() const = 0;

    // =========================================================================
    // Time and Position
    // =========================================================================

    /// Get playback position in seconds
    [[nodiscard]] virtual float playback_position() const = 0;

    /// Set playback position in seconds
    virtual void set_playback_position(float seconds) = 0;

    /// Get playback position in samples
    [[nodiscard]] virtual std::uint64_t playback_sample() const = 0;

    /// Set playback position in samples
    virtual void set_playback_sample(std::uint64_t sample) = 0;

    /// Get total duration
    [[nodiscard]] virtual float duration() const = 0;

    // =========================================================================
    // Fading
    // =========================================================================

    /// Fade volume over time
    virtual void fade_to(float target_volume, float duration_seconds) = 0;

    /// Fade in from zero
    virtual void fade_in(float duration_seconds) = 0;

    /// Fade out to zero
    virtual void fade_out(float duration_seconds) = 0;

    /// Fade out and stop
    virtual void fade_out_and_stop(float duration_seconds) = 0;

    /// Check if currently fading
    [[nodiscard]] virtual bool is_fading() const = 0;

    // =========================================================================
    // Bus Routing
    // =========================================================================

    /// Get output bus
    [[nodiscard]] virtual BusId output_bus() const = 0;

    /// Set output bus
    virtual void set_output_bus(BusId bus) = 0;

    // =========================================================================
    // Priority
    // =========================================================================

    /// Get priority (lower = higher priority)
    [[nodiscard]] virtual std::uint8_t priority() const = 0;

    /// Set priority
    virtual void set_priority(std::uint8_t priority) = 0;

    // =========================================================================
    // 3D Audio
    // =========================================================================

    /// Get spatialization mode
    [[nodiscard]] virtual SpatializationMode spatialization() const = 0;

    /// Set spatialization mode
    virtual void set_spatialization(SpatializationMode mode) = 0;

    /// Get 3D position
    [[nodiscard]] virtual void_math::Vec3 position() const = 0;

    /// Set 3D position
    virtual void set_position(const void_math::Vec3& pos) = 0;

    /// Get velocity (for Doppler effect)
    [[nodiscard]] virtual void_math::Vec3 velocity() const = 0;

    /// Set velocity
    virtual void set_velocity(const void_math::Vec3& vel) = 0;

    /// Get direction (for cone attenuation)
    [[nodiscard]] virtual void_math::Vec3 direction() const = 0;

    /// Set direction
    virtual void set_direction(const void_math::Vec3& dir) = 0;

    // =========================================================================
    // Attenuation
    // =========================================================================

    /// Get attenuation settings
    [[nodiscard]] virtual AttenuationSettings attenuation() const = 0;

    /// Set attenuation settings
    virtual void set_attenuation(const AttenuationSettings& settings) = 0;

    /// Get reference distance
    [[nodiscard]] virtual float reference_distance() const = 0;

    /// Set reference distance
    virtual void set_reference_distance(float distance) = 0;

    /// Get max distance
    [[nodiscard]] virtual float max_distance() const = 0;

    /// Set max distance
    virtual void set_max_distance(float distance) = 0;

    /// Get rolloff factor
    [[nodiscard]] virtual float rolloff_factor() const = 0;

    /// Set rolloff factor
    virtual void set_rolloff_factor(float factor) = 0;

    // =========================================================================
    // Cone
    // =========================================================================

    /// Get cone settings
    [[nodiscard]] virtual ConeSettings cone() const = 0;

    /// Set cone settings
    virtual void set_cone(const ConeSettings& settings) = 0;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /// Set callback for when source finishes playing
    virtual void on_finished(std::function<void(SourceId)> callback) = 0;

    /// Set callback for when source loops
    virtual void on_loop(std::function<void(SourceId, std::uint32_t)> callback) = 0;

    // =========================================================================
    // Native Handle
    // =========================================================================

    /// Get native handle (backend-specific)
    [[nodiscard]] virtual void* native_handle() const = 0;
};

// =============================================================================
// Audio Source Implementation
// =============================================================================

/// Standard audio source implementation
class AudioSource : public IAudioSource {
public:
    AudioSource() = default;
    explicit AudioSource(const AudioSourceConfig& config);

    // Identity
    [[nodiscard]] SourceId id() const override { return m_id; }
    [[nodiscard]] const std::string& name() const override { return m_name; }
    [[nodiscard]] void* user_data() const override { return m_user_data; }
    void set_user_data(void* data) override { m_user_data = data; }

    /// Set source ID
    void set_id(SourceId id) { m_id = id; }

    // Playback
    void play() override;
    void pause() override;
    void stop() override;
    [[nodiscard]] AudioState state() const override { return m_state; }
    [[nodiscard]] bool is_playing() const override { return m_state == AudioState::Playing; }
    [[nodiscard]] bool is_paused() const override { return m_state == AudioState::Paused; }
    [[nodiscard]] bool is_stopped() const override { return m_state == AudioState::Stopped; }

    // Buffer
    [[nodiscard]] BufferId buffer() const override { return m_buffer; }
    void set_buffer(BufferId buffer) override { m_buffer = buffer; }

    // Volume and pan
    [[nodiscard]] float volume() const override { return m_volume; }
    void set_volume(float volume) override;
    [[nodiscard]] float pitch() const override { return m_pitch; }
    void set_pitch(float pitch) override;
    [[nodiscard]] float pan() const override { return m_pan; }
    void set_pan(float pan) override;
    [[nodiscard]] bool is_muted() const override { return m_muted; }
    void set_muted(bool muted) override { m_muted = muted; }

    // Looping
    [[nodiscard]] bool is_looping() const override { return m_looping; }
    void set_looping(bool loop) override { m_looping = loop; }
    [[nodiscard]] std::uint32_t loop_count() const override { return m_loop_count; }
    void set_loop_count(std::uint32_t count) override { m_loop_count = count; }
    [[nodiscard]] std::uint32_t current_loop() const override { return m_current_loop; }

    // Time
    [[nodiscard]] float playback_position() const override { return m_playback_position; }
    void set_playback_position(float seconds) override;
    [[nodiscard]] std::uint64_t playback_sample() const override { return m_playback_sample; }
    void set_playback_sample(std::uint64_t sample) override;
    [[nodiscard]] float duration() const override { return m_duration; }

    /// Set duration (called by system)
    void set_duration(float d) { m_duration = d; }

    // Fading
    void fade_to(float target_volume, float duration_seconds) override;
    void fade_in(float duration_seconds) override;
    void fade_out(float duration_seconds) override;
    void fade_out_and_stop(float duration_seconds) override;
    [[nodiscard]] bool is_fading() const override { return m_fading; }

    // Bus
    [[nodiscard]] BusId output_bus() const override { return m_output_bus; }
    void set_output_bus(BusId bus) override { m_output_bus = bus; }

    // Priority
    [[nodiscard]] std::uint8_t priority() const override { return m_priority; }
    void set_priority(std::uint8_t priority) override { m_priority = priority; }

    // 3D Audio
    [[nodiscard]] SpatializationMode spatialization() const override { return m_spatialization; }
    void set_spatialization(SpatializationMode mode) override { m_spatialization = mode; }
    [[nodiscard]] void_math::Vec3 position() const override { return m_position; }
    void set_position(const void_math::Vec3& pos) override;
    [[nodiscard]] void_math::Vec3 velocity() const override { return m_velocity; }
    void set_velocity(const void_math::Vec3& vel) override;
    [[nodiscard]] void_math::Vec3 direction() const override { return m_direction; }
    void set_direction(const void_math::Vec3& dir) override;

    // Attenuation
    [[nodiscard]] AttenuationSettings attenuation() const override { return m_attenuation; }
    void set_attenuation(const AttenuationSettings& settings) override { m_attenuation = settings; }
    [[nodiscard]] float reference_distance() const override { return m_attenuation.reference_distance; }
    void set_reference_distance(float distance) override { m_attenuation.reference_distance = distance; }
    [[nodiscard]] float max_distance() const override { return m_attenuation.max_distance; }
    void set_max_distance(float distance) override { m_attenuation.max_distance = distance; }
    [[nodiscard]] float rolloff_factor() const override { return m_attenuation.rolloff_factor; }
    void set_rolloff_factor(float factor) override { m_attenuation.rolloff_factor = factor; }

    // Cone
    [[nodiscard]] ConeSettings cone() const override { return m_cone; }
    void set_cone(const ConeSettings& settings) override { m_cone = settings; }

    // Callbacks
    void on_finished(std::function<void(SourceId)> callback) override;
    void on_loop(std::function<void(SourceId, std::uint32_t)> callback) override;

    // Native handle
    [[nodiscard]] void* native_handle() const override { return m_native_handle; }
    void set_native_handle(void* handle) { m_native_handle = handle; }

    /// Update function (called by system each frame)
    void update(float dt);

    /// Fire finished callback
    void fire_finished_callback();

    /// Fire loop callback
    void fire_loop_callback();

    /// Get effective volume (with fading applied)
    [[nodiscard]] float effective_volume() const;

private:
    SourceId m_id{0};
    std::string m_name;
    void* m_user_data = nullptr;

    // Playback state
    AudioState m_state = AudioState::Initial;
    BufferId m_buffer;

    // Volume
    float m_volume = 1.0f;
    float m_pitch = 1.0f;
    float m_pan = 0.0f;
    bool m_muted = false;

    // Looping
    bool m_looping = false;
    std::uint32_t m_loop_count = 0;
    std::uint32_t m_current_loop = 0;

    // Time
    float m_playback_position = 0;
    std::uint64_t m_playback_sample = 0;
    float m_duration = 0;

    // Fading
    bool m_fading = false;
    float m_fade_start_volume = 0;
    float m_fade_target_volume = 0;
    float m_fade_duration = 0;
    float m_fade_time = 0;
    bool m_stop_after_fade = false;

    // Bus
    BusId m_output_bus;
    std::uint8_t m_priority = 128;

    // 3D audio
    SpatializationMode m_spatialization = SpatializationMode::None;
    void_math::Vec3 m_position = {0, 0, 0};
    void_math::Vec3 m_velocity = {0, 0, 0};
    void_math::Vec3 m_direction = {0, 0, -1};
    AttenuationSettings m_attenuation;
    ConeSettings m_cone;

    // Callbacks
    std::function<void(SourceId)> m_finished_callback;
    std::function<void(SourceId, std::uint32_t)> m_loop_callback;

    void* m_native_handle = nullptr;
};

// =============================================================================
// Audio Source Builder
// =============================================================================

/// Fluent builder for audio sources
class AudioSourceBuilder {
public:
    AudioSourceBuilder() = default;

    /// Set buffer
    AudioSourceBuilder& buffer(BufferId buf);

    /// Set output bus
    AudioSourceBuilder& bus(BusId bus);

    /// Set volume
    AudioSourceBuilder& volume(float vol);

    /// Set pitch
    AudioSourceBuilder& pitch(float p);

    /// Set pan
    AudioSourceBuilder& pan(float p);

    /// Enable looping
    AudioSourceBuilder& loop(bool enable = true);

    /// Set loop count
    AudioSourceBuilder& loop_count(std::uint32_t count);

    /// Play on create
    AudioSourceBuilder& play_on_create(bool enable = true);

    /// Set start position
    AudioSourceBuilder& start_at(float seconds);

    /// Set 3D position
    AudioSourceBuilder& position(float x, float y, float z);
    AudioSourceBuilder& position(const void_math::Vec3& pos);

    /// Set velocity
    AudioSourceBuilder& velocity(const void_math::Vec3& vel);

    /// Set direction
    AudioSourceBuilder& direction(const void_math::Vec3& dir);

    /// Enable 3D spatialization
    AudioSourceBuilder& spatial_3d();

    /// Enable HRTF
    AudioSourceBuilder& hrtf();

    /// Set attenuation
    AudioSourceBuilder& attenuation(const AttenuationSettings& settings);

    /// Set reference distance
    AudioSourceBuilder& reference_distance(float dist);

    /// Set max distance
    AudioSourceBuilder& max_distance(float dist);

    /// Set rolloff
    AudioSourceBuilder& rolloff(float factor);

    /// Set cone
    AudioSourceBuilder& cone(float inner_angle, float outer_angle, float outer_gain);

    /// Set priority
    AudioSourceBuilder& priority(std::uint8_t p);

    /// Set name
    AudioSourceBuilder& name(std::string_view n);

    /// Set user data
    AudioSourceBuilder& user_data(void* data);

    /// Build the configuration
    [[nodiscard]] AudioSourceConfig build() const { return m_config; }

private:
    AudioSourceConfig m_config;
};

// =============================================================================
// One-Shot Audio
// =============================================================================

/// Handle for one-shot audio playback
struct OneShotHandle {
    SourceId source_id;
    bool valid = false;

    explicit operator bool() const { return valid; }
};

/// Simple one-shot audio playback (fire and forget)
class OneShotPlayer {
public:
    explicit OneShotPlayer(class AudioSystem& system);

    /// Play a buffer at default settings
    OneShotHandle play(BufferId buffer);

    /// Play a buffer at volume
    OneShotHandle play(BufferId buffer, float volume);

    /// Play a buffer at position
    OneShotHandle play_3d(BufferId buffer, const void_math::Vec3& position, float volume = 1.0f);

    /// Play a buffer with full config
    OneShotHandle play(const AudioSourceConfig& config);

    /// Stop a one-shot
    void stop(OneShotHandle handle);

    /// Stop all one-shots
    void stop_all();

    /// Update (recycle finished sources)
    void update();

    /// Get number of active one-shots
    [[nodiscard]] std::size_t active_count() const;

private:
    class AudioSystem& m_system;
    std::vector<SourceId> m_active_sources;
    std::vector<SourceId> m_pool;
    std::size_t m_pool_size = 32;
};

} // namespace void_audio
