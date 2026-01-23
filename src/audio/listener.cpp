/// @file listener.cpp
/// @brief Audio listener implementations for void_audio

#include <void_engine/audio/listener.hpp>

#include <algorithm>
#include <cmath>

namespace void_audio {

// =============================================================================
// AudioListener Implementation
// =============================================================================

AudioListener::AudioListener(const ListenerConfig& config)
    : m_position(config.position)
    , m_velocity(config.velocity)
    , m_forward(config.forward)
    , m_up(config.up)
    , m_master_volume(config.master_volume)
    , m_doppler_factor(config.doppler_factor)
    , m_speed_of_sound(config.speed_of_sound) {
}

void AudioListener::set_position(const void_math::Vec3& pos) {
    m_position = pos;
    m_dirty = true;
}

void AudioListener::set_velocity(const void_math::Vec3& vel) {
    m_velocity = vel;
    m_dirty = true;
}

void AudioListener::set_orientation(const void_math::Vec3& forward, const void_math::Vec3& up) {
    // Normalize vectors
    auto normalize = [](const void_math::Vec3& v) -> void_math::Vec3 {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len > 1e-6f) {
            return {v.x / len, v.y / len, v.z / len};
        }
        return v;
    };

    m_forward = normalize(forward);
    m_up = normalize(up);
    m_dirty = true;
}

void AudioListener::set_transform(const void_math::Transform& transform) {
    m_position = transform.position;

    // Extract forward and up from quaternion
    const auto& q = transform.rotation;

    // Forward is -Z axis rotated by quaternion
    void_math::Vec3 fwd = {0, 0, -1};
    void_math::Vec3 up_dir = {0, 1, 0};

    // Rotate by quaternion
    auto rotate = [](const void_math::Vec3& v, const void_math::Quat& q) -> void_math::Vec3 {
        float qx = q.x, qy = q.y, qz = q.z, qw = q.w;

        float cx = qy * v.z - qz * v.y;
        float cy = qz * v.x - qx * v.z;
        float cz = qx * v.y - qy * v.x;

        float ccx = qy * cz - qz * cy;
        float ccy = qz * cx - qx * cz;
        float ccz = qx * cy - qy * cx;

        return {
            v.x + 2.0f * (qw * cx + ccx),
            v.y + 2.0f * (qw * cy + ccy),
            v.z + 2.0f * (qw * cz + ccz)
        };
    };

    m_forward = rotate(fwd, q);
    m_up = rotate(up_dir, q);
    m_dirty = true;
}

void AudioListener::set_master_volume(float volume) {
    m_master_volume = std::clamp(volume, 0.0f, 1.0f);
    m_dirty = true;
}

void AudioListener::set_doppler_factor(float factor) {
    m_doppler_factor = std::max(0.0f, factor);
    m_dirty = true;
}

void AudioListener::set_speed_of_sound(float speed) {
    m_speed_of_sound = std::max(1.0f, speed);
    m_dirty = true;
}

// =============================================================================
// Listener Utilities
// =============================================================================

float calculate_attenuation(
    const void_math::Vec3& source_pos,
    const void_math::Vec3& listener_pos,
    const AttenuationSettings& settings) {

    // Calculate distance
    float dx = source_pos.x - listener_pos.x;
    float dy = source_pos.y - listener_pos.y;
    float dz = source_pos.z - listener_pos.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    float ref_dist = settings.reference_distance;
    float max_dist = settings.max_distance;
    float rolloff = settings.rolloff_factor;

    float gain = 1.0f;

    switch (settings.model) {
        case AttenuationModel::None:
            gain = 1.0f;
            break;

        case AttenuationModel::InverseDistance:
            gain = ref_dist / (ref_dist + rolloff * (distance - ref_dist));
            break;

        case AttenuationModel::InverseDistanceClamped:
            distance = std::clamp(distance, ref_dist, max_dist);
            gain = ref_dist / (ref_dist + rolloff * (distance - ref_dist));
            break;

        case AttenuationModel::LinearDistance:
            gain = 1.0f - rolloff * (distance - ref_dist) / (max_dist - ref_dist);
            break;

        case AttenuationModel::LinearDistanceClamped:
            distance = std::clamp(distance, ref_dist, max_dist);
            gain = 1.0f - rolloff * (distance - ref_dist) / (max_dist - ref_dist);
            break;

        case AttenuationModel::ExponentialDistance:
            gain = std::pow(distance / ref_dist, -rolloff);
            break;

        case AttenuationModel::ExponentialDistanceClamped:
            distance = std::clamp(distance, ref_dist, max_dist);
            gain = std::pow(distance / ref_dist, -rolloff);
            break;

        case AttenuationModel::Custom:
            if (settings.custom_curve) {
                float normalized_dist = (distance - ref_dist) / (max_dist - ref_dist);
                normalized_dist = std::clamp(normalized_dist, 0.0f, 1.0f);
                gain = settings.custom_curve(normalized_dist);
            }
            break;
    }

    return std::clamp(gain, settings.min_gain, settings.max_gain);
}

float calculate_doppler_pitch(
    const void_math::Vec3& source_pos,
    const void_math::Vec3& source_vel,
    const void_math::Vec3& listener_pos,
    const void_math::Vec3& listener_vel,
    float speed_of_sound,
    float doppler_factor) {

    if (doppler_factor <= 0) return 1.0f;

    // Direction from listener to source
    float dx = source_pos.x - listener_pos.x;
    float dy = source_pos.y - listener_pos.y;
    float dz = source_pos.z - listener_pos.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist < 1e-6f) return 1.0f;

    // Normalize direction
    dx /= dist;
    dy /= dist;
    dz /= dist;

    // Relative velocities along direction
    float v_listener = listener_vel.x * dx + listener_vel.y * dy + listener_vel.z * dz;
    float v_source = source_vel.x * dx + source_vel.y * dy + source_vel.z * dz;

    // Clamp velocities to prevent division by zero or negative values
    float max_vel = speed_of_sound * 0.9f;
    v_listener = std::clamp(v_listener, -max_vel, max_vel);
    v_source = std::clamp(v_source, -max_vel, max_vel);

    // Doppler formula: f' = f * (c + v_listener) / (c + v_source)
    float pitch = (speed_of_sound + v_listener * doppler_factor) /
                  (speed_of_sound + v_source * doppler_factor);

    // Clamp to reasonable range
    return std::clamp(pitch, 0.5f, 2.0f);
}

float calculate_cone_attenuation(
    const void_math::Vec3& source_pos,
    const void_math::Vec3& source_dir,
    const void_math::Vec3& listener_pos,
    const ConeSettings& cone) {

    if (cone.inner_angle >= 360.0f || cone.outer_angle >= 360.0f) {
        return 1.0f; // No cone attenuation
    }

    // Direction from source to listener
    float dx = listener_pos.x - source_pos.x;
    float dy = listener_pos.y - source_pos.y;
    float dz = listener_pos.z - source_pos.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist < 1e-6f) return 1.0f;

    dx /= dist;
    dy /= dist;
    dz /= dist;

    // Angle between source direction and direction to listener
    float dot = source_dir.x * dx + source_dir.y * dy + source_dir.z * dz;
    float angle = std::acos(std::clamp(dot, -1.0f, 1.0f)) * (180.0f / 3.14159265358979323846f);

    float inner_half = cone.inner_angle * 0.5f;
    float outer_half = cone.outer_angle * 0.5f;

    if (angle <= inner_half) {
        return 1.0f;
    } else if (angle >= outer_half) {
        return cone.outer_gain;
    } else {
        // Interpolate between inner and outer
        float t = (angle - inner_half) / (outer_half - inner_half);
        return 1.0f + (cone.outer_gain - 1.0f) * t;
    }
}

void calculate_stereo_pan(
    const void_math::Vec3& source_pos,
    const void_math::Vec3& listener_pos,
    const void_math::Vec3& listener_forward,
    const void_math::Vec3& listener_up,
    float& left_gain,
    float& right_gain) {

    // Calculate direction from listener to source
    float dx = source_pos.x - listener_pos.x;
    float dy = source_pos.y - listener_pos.y;
    float dz = source_pos.z - listener_pos.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist < 1e-6f) {
        left_gain = 0.707f;
        right_gain = 0.707f;
        return;
    }

    dx /= dist;
    dy /= dist;
    dz /= dist;

    // Calculate right vector (cross product of forward and up)
    void_math::Vec3 right = {
        listener_forward.y * listener_up.z - listener_forward.z * listener_up.y,
        listener_forward.z * listener_up.x - listener_forward.x * listener_up.z,
        listener_forward.x * listener_up.y - listener_forward.y * listener_up.x
    };

    // Dot product with right vector gives left-right pan (-1 to 1)
    float pan = dx * right.x + dy * right.y + dz * right.z;

    // Convert to gains using constant power panning
    constexpr float pi = 3.14159265358979323846f;
    float angle = (pan + 1.0f) * 0.25f * pi; // 0 to pi/2

    left_gain = std::cos(angle);
    right_gain = std::sin(angle);
}

} // namespace void_audio
