/// @file source.cpp
/// @brief Audio source implementations for void_audio

#include <void_engine/audio/source.hpp>
#include <void_engine/audio/backend.hpp>

#include <algorithm>
#include <cmath>

namespace void_audio {

// =============================================================================
// AudioSource Implementation
// =============================================================================

AudioSource::AudioSource(const AudioSourceConfig& config)
    : m_name(config.name)
    , m_user_data(config.user_data)
    , m_buffer(config.buffer)
    , m_volume(config.volume)
    , m_pitch(config.pitch)
    , m_pan(config.pan)
    , m_looping(config.loop)
    , m_output_bus(config.output_bus)
    , m_priority(config.priority)
    , m_spatialization(config.spatialization)
    , m_position(config.position)
    , m_velocity(config.velocity)
    , m_direction(config.direction)
    , m_attenuation(config.attenuation)
    , m_cone(config.cone) {

    if (config.start_time > 0) {
        m_playback_position = config.start_time;
    }
}

void AudioSource::play() {
    m_state = AudioState::Playing;
}

void AudioSource::pause() {
    if (m_state == AudioState::Playing) {
        m_state = AudioState::Paused;
    }
}

void AudioSource::stop() {
    m_state = AudioState::Stopped;
    m_playback_position = 0;
    m_playback_sample = 0;
    m_current_loop = 0;
}

void AudioSource::set_volume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioSource::set_pitch(float pitch) {
    m_pitch = std::clamp(pitch, 0.01f, 10.0f);
}

void AudioSource::set_pan(float pan) {
    m_pan = std::clamp(pan, -1.0f, 1.0f);
}

void AudioSource::set_playback_position(float seconds) {
    m_playback_position = std::max(0.0f, seconds);
    // Update sample position based on buffer sample rate (if known)
}

void AudioSource::set_playback_sample(std::uint64_t sample) {
    m_playback_sample = sample;
    // Update time position based on buffer sample rate (if known)
}

void AudioSource::fade_to(float target_volume, float duration_seconds) {
    if (duration_seconds <= 0) {
        m_volume = target_volume;
        return;
    }

    m_fading = true;
    m_fade_start_volume = m_volume;
    m_fade_target_volume = std::clamp(target_volume, 0.0f, 1.0f);
    m_fade_duration = duration_seconds;
    m_fade_time = 0;
    m_stop_after_fade = false;
}

void AudioSource::fade_in(float duration_seconds) {
    float current_vol = m_volume;
    m_volume = 0;
    fade_to(current_vol > 0 ? current_vol : 1.0f, duration_seconds);
}

void AudioSource::fade_out(float duration_seconds) {
    fade_to(0, duration_seconds);
}

void AudioSource::fade_out_and_stop(float duration_seconds) {
    fade_to(0, duration_seconds);
    m_stop_after_fade = true;
}

void AudioSource::set_position(const void_math::Vec3& pos) {
    m_position = pos;
}

void AudioSource::set_velocity(const void_math::Vec3& vel) {
    m_velocity = vel;
}

void AudioSource::set_direction(const void_math::Vec3& dir) {
    // Normalize direction
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 1e-6f) {
        m_direction = {dir.x / len, dir.y / len, dir.z / len};
    }
}

void AudioSource::on_finished(std::function<void(SourceId)> callback) {
    m_finished_callback = std::move(callback);
}

void AudioSource::on_loop(std::function<void(SourceId, std::uint32_t)> callback) {
    m_loop_callback = std::move(callback);
}

void AudioSource::update(float dt) {
    if (m_state != AudioState::Playing) return;

    // Update playback position
    m_playback_position += dt * m_pitch;

    // Handle fading
    if (m_fading) {
        m_fade_time += dt;

        if (m_fade_time >= m_fade_duration) {
            m_volume = m_fade_target_volume;
            m_fading = false;

            if (m_stop_after_fade) {
                stop();
                fire_finished_callback();
            }
        } else {
            float t = m_fade_time / m_fade_duration;
            // Smooth fade curve
            t = t * t * (3.0f - 2.0f * t);
            m_volume = m_fade_start_volume + (m_fade_target_volume - m_fade_start_volume) * t;
        }
    }

    // Check for playback end
    if (m_duration > 0 && m_playback_position >= m_duration) {
        if (m_looping && (m_loop_count == 0 || m_current_loop < m_loop_count - 1)) {
            m_playback_position = 0;
            m_current_loop++;
            fire_loop_callback();
        } else {
            stop();
            fire_finished_callback();
        }
    }
}

void AudioSource::fire_finished_callback() {
    if (m_finished_callback) {
        m_finished_callback(m_id);
    }
}

void AudioSource::fire_loop_callback() {
    if (m_loop_callback) {
        m_loop_callback(m_id, m_current_loop);
    }
}

float AudioSource::effective_volume() const {
    if (m_muted) return 0;
    return m_volume;
}

// =============================================================================
// AudioSourceBuilder Implementation
// =============================================================================

AudioSourceBuilder& AudioSourceBuilder::buffer(BufferId buf) {
    m_config.buffer = buf;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::bus(BusId bus) {
    m_config.output_bus = bus;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::volume(float vol) {
    m_config.volume = std::clamp(vol, 0.0f, 1.0f);
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::pitch(float p) {
    m_config.pitch = std::clamp(p, 0.01f, 10.0f);
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::pan(float p) {
    m_config.pan = std::clamp(p, -1.0f, 1.0f);
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::loop(bool enable) {
    m_config.loop = enable;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::loop_count(std::uint32_t count) {
    m_config.loop = count > 0;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::play_on_create(bool enable) {
    m_config.play_on_create = enable;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::start_at(float seconds) {
    m_config.start_time = seconds;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::position(float x, float y, float z) {
    m_config.position = {x, y, z};
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::position(const void_math::Vec3& pos) {
    m_config.position = pos;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::velocity(const void_math::Vec3& vel) {
    m_config.velocity = vel;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::direction(const void_math::Vec3& dir) {
    m_config.direction = dir;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::spatial_3d() {
    m_config.spatialization = SpatializationMode::Positional;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::hrtf() {
    m_config.spatialization = SpatializationMode::HRTF;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::attenuation(const AttenuationSettings& settings) {
    m_config.attenuation = settings;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::reference_distance(float dist) {
    m_config.attenuation.reference_distance = dist;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::max_distance(float dist) {
    m_config.attenuation.max_distance = dist;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::rolloff(float factor) {
    m_config.attenuation.rolloff_factor = factor;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::cone(float inner_angle, float outer_angle, float outer_gain) {
    m_config.cone.inner_angle = inner_angle;
    m_config.cone.outer_angle = outer_angle;
    m_config.cone.outer_gain = outer_gain;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::priority(std::uint8_t p) {
    m_config.priority = p;
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::name(std::string_view n) {
    m_config.name = std::string(n);
    return *this;
}

AudioSourceBuilder& AudioSourceBuilder::user_data(void* data) {
    m_config.user_data = data;
    return *this;
}

// =============================================================================
// OneShotPlayer Implementation
// =============================================================================

OneShotPlayer::OneShotPlayer(AudioSystem& system)
    : m_system(system) {
}

OneShotHandle OneShotPlayer::play(BufferId buffer) {
    return play(buffer, 1.0f);
}

OneShotHandle OneShotPlayer::play(BufferId buffer, float volume) {
    AudioSourceConfig config;
    config.buffer = buffer;
    config.volume = volume;
    config.play_on_create = true;
    return play(config);
}

OneShotHandle OneShotPlayer::play_3d(BufferId buffer, const void_math::Vec3& position, float volume) {
    AudioSourceConfig config;
    config.buffer = buffer;
    config.volume = volume;
    config.play_on_create = true;
    config.spatialization = SpatializationMode::Positional;
    config.position = position;
    return play(config);
}

OneShotHandle OneShotPlayer::play(const AudioSourceConfig& config) {
    OneShotHandle handle;

    // Try to reuse from pool
    SourceId source_id{0};
    if (!m_pool.empty()) {
        source_id = m_pool.back();
        m_pool.pop_back();

        // Reconfigure existing source
        if (auto* source = m_system.get_source(source_id)) {
            source->set_buffer(config.buffer);
            source->set_volume(config.volume);
            source->set_pitch(config.pitch);
            source->set_looping(false);
            source->set_spatialization(config.spatialization);
            source->set_position(config.position);
            source->play();
        }
    } else {
        // Create new source
        auto result = m_system.create_source(config);
        if (result) {
            source_id = *result;
        }
    }

    if (source_id) {
        m_active_sources.push_back(source_id);
        handle.source_id = source_id;
        handle.valid = true;
    }

    return handle;
}

void OneShotPlayer::stop(OneShotHandle handle) {
    if (!handle.valid) return;

    auto it = std::find(m_active_sources.begin(), m_active_sources.end(), handle.source_id);
    if (it != m_active_sources.end()) {
        m_system.stop(handle.source_id);
        m_active_sources.erase(it);
        m_pool.push_back(handle.source_id);
    }
}

void OneShotPlayer::stop_all() {
    for (auto source_id : m_active_sources) {
        m_system.stop(source_id);
        m_pool.push_back(source_id);
    }
    m_active_sources.clear();
}

void OneShotPlayer::update() {
    // Check for finished sources
    auto it = m_active_sources.begin();
    while (it != m_active_sources.end()) {
        auto* source = m_system.get_source(*it);
        if (!source || source->is_stopped()) {
            if (m_pool.size() < m_pool_size) {
                m_pool.push_back(*it);
            } else {
                m_system.destroy_source(*it);
            }
            it = m_active_sources.erase(it);
        } else {
            ++it;
        }
    }
}

std::size_t OneShotPlayer::active_count() const {
    return m_active_sources.size();
}

} // namespace void_audio
