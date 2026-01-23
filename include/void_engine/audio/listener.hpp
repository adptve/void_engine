/// @file listener.hpp
/// @brief Audio listener interface for void_audio

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/math/math.hpp>

namespace void_audio {

// =============================================================================
// Audio Listener Interface
// =============================================================================

/// Interface for audio listener (the "ear" in 3D audio)
class IAudioListener {
public:
    virtual ~IAudioListener() = default;

    // =========================================================================
    // Position and Orientation
    // =========================================================================

    /// Get listener position
    [[nodiscard]] virtual void_math::Vec3 position() const = 0;

    /// Set listener position
    virtual void set_position(const void_math::Vec3& pos) = 0;

    /// Get listener velocity (for Doppler effect)
    [[nodiscard]] virtual void_math::Vec3 velocity() const = 0;

    /// Set listener velocity
    virtual void set_velocity(const void_math::Vec3& vel) = 0;

    /// Get forward direction
    [[nodiscard]] virtual void_math::Vec3 forward() const = 0;

    /// Get up direction
    [[nodiscard]] virtual void_math::Vec3 up() const = 0;

    /// Set orientation
    virtual void set_orientation(const void_math::Vec3& forward, const void_math::Vec3& up) = 0;

    /// Set transform from matrix
    virtual void set_transform(const void_math::Transform& transform) = 0;

    // =========================================================================
    // Volume
    // =========================================================================

    /// Get master volume
    [[nodiscard]] virtual float master_volume() const = 0;

    /// Set master volume
    virtual void set_master_volume(float volume) = 0;

    // =========================================================================
    // Doppler Effect
    // =========================================================================

    /// Get Doppler factor
    [[nodiscard]] virtual float doppler_factor() const = 0;

    /// Set Doppler factor (0 = no Doppler, 1 = normal, >1 = exaggerated)
    virtual void set_doppler_factor(float factor) = 0;

    /// Get speed of sound
    [[nodiscard]] virtual float speed_of_sound() const = 0;

    /// Set speed of sound (default 343.3 m/s)
    virtual void set_speed_of_sound(float speed) = 0;

    // =========================================================================
    // Native Handle
    // =========================================================================

    /// Get native handle (backend-specific)
    [[nodiscard]] virtual void* native_handle() const = 0;
};

// =============================================================================
// Audio Listener Implementation
// =============================================================================

/// Standard audio listener implementation
class AudioListener : public IAudioListener {
public:
    AudioListener() = default;
    explicit AudioListener(const ListenerConfig& config);

    // Position and orientation
    [[nodiscard]] void_math::Vec3 position() const override { return m_position; }
    void set_position(const void_math::Vec3& pos) override;
    [[nodiscard]] void_math::Vec3 velocity() const override { return m_velocity; }
    void set_velocity(const void_math::Vec3& vel) override;
    [[nodiscard]] void_math::Vec3 forward() const override { return m_forward; }
    [[nodiscard]] void_math::Vec3 up() const override { return m_up; }
    void set_orientation(const void_math::Vec3& forward, const void_math::Vec3& up) override;
    void set_transform(const void_math::Transform& transform) override;

    // Volume
    [[nodiscard]] float master_volume() const override { return m_master_volume; }
    void set_master_volume(float volume) override;

    // Doppler
    [[nodiscard]] float doppler_factor() const override { return m_doppler_factor; }
    void set_doppler_factor(float factor) override;
    [[nodiscard]] float speed_of_sound() const override { return m_speed_of_sound; }
    void set_speed_of_sound(float speed) override;

    // Native handle
    [[nodiscard]] void* native_handle() const override { return m_native_handle; }
    void set_native_handle(void* handle) { m_native_handle = handle; }

    /// Mark as dirty (needs backend update)
    void mark_dirty() { m_dirty = true; }

    /// Check if dirty
    [[nodiscard]] bool is_dirty() const { return m_dirty; }

    /// Clear dirty flag
    void clear_dirty() { m_dirty = false; }

private:
    void_math::Vec3 m_position = {0, 0, 0};
    void_math::Vec3 m_velocity = {0, 0, 0};
    void_math::Vec3 m_forward = {0, 0, -1};
    void_math::Vec3 m_up = {0, 1, 0};

    float m_master_volume = 1.0f;
    float m_doppler_factor = 1.0f;
    float m_speed_of_sound = 343.3f;

    void* m_native_handle = nullptr;
    bool m_dirty = true;
};

// =============================================================================
// Listener Utilities
// =============================================================================

/// Calculate distance attenuation
float calculate_attenuation(
    const void_math::Vec3& source_pos,
    const void_math::Vec3& listener_pos,
    const AttenuationSettings& settings);

/// Calculate Doppler pitch shift
float calculate_doppler_pitch(
    const void_math::Vec3& source_pos,
    const void_math::Vec3& source_vel,
    const void_math::Vec3& listener_pos,
    const void_math::Vec3& listener_vel,
    float speed_of_sound,
    float doppler_factor);

/// Calculate cone attenuation
float calculate_cone_attenuation(
    const void_math::Vec3& source_pos,
    const void_math::Vec3& source_dir,
    const void_math::Vec3& listener_pos,
    const ConeSettings& cone);

/// Calculate panning for stereo output
void calculate_stereo_pan(
    const void_math::Vec3& source_pos,
    const void_math::Vec3& listener_pos,
    const void_math::Vec3& listener_forward,
    const void_math::Vec3& listener_up,
    float& left_gain,
    float& right_gain);

} // namespace void_audio
