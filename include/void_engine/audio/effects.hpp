/// @file effects.hpp
/// @brief Audio effects for void_audio

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace void_audio {

// =============================================================================
// Audio Effect Interface
// =============================================================================

/// Interface for audio effects
class IAudioEffect {
public:
    virtual ~IAudioEffect() = default;

    /// Get effect ID
    [[nodiscard]] virtual EffectId id() const = 0;

    /// Get effect type
    [[nodiscard]] virtual EffectType type() const = 0;

    /// Get effect name
    [[nodiscard]] virtual const std::string& name() const = 0;

    /// Check if enabled
    [[nodiscard]] virtual bool is_enabled() const = 0;

    /// Set enabled
    virtual void set_enabled(bool enabled) = 0;

    /// Get wet/dry mix (0 = dry, 1 = wet)
    [[nodiscard]] virtual float mix() const = 0;

    /// Set wet/dry mix
    virtual void set_mix(float mix) = 0;

    /// Process audio (in-place)
    virtual void process(float* samples, std::size_t sample_count, std::uint32_t channels) = 0;

    /// Reset effect state
    virtual void reset() = 0;

    /// Get native handle (backend-specific)
    [[nodiscard]] virtual void* native_handle() const = 0;
};

// =============================================================================
// Base Effect Implementation
// =============================================================================

/// Base class for effect implementations
class AudioEffectBase : public IAudioEffect {
public:
    [[nodiscard]] EffectId id() const override { return m_id; }
    [[nodiscard]] EffectType type() const override { return m_type; }
    [[nodiscard]] const std::string& name() const override { return m_name; }
    [[nodiscard]] bool is_enabled() const override { return m_enabled; }
    void set_enabled(bool enabled) override { m_enabled = enabled; }
    [[nodiscard]] float mix() const override { return m_mix; }
    void set_mix(float mix) override { m_mix = std::clamp(mix, 0.0f, 1.0f); }
    [[nodiscard]] void* native_handle() const override { return m_native_handle; }

    void set_id(EffectId id) { m_id = id; }
    void set_native_handle(void* handle) { m_native_handle = handle; }

protected:
    EffectId m_id{0};
    EffectType m_type = EffectType::None;
    std::string m_name;
    bool m_enabled = true;
    float m_mix = 1.0f;
    void* m_native_handle = nullptr;

    /// Apply wet/dry mix
    void apply_mix(const float* dry, float* wet, std::size_t count);
};

// =============================================================================
// Reverb Effect
// =============================================================================

/// Reverb effect implementation
class ReverbEffect : public AudioEffectBase {
public:
    ReverbEffect();
    explicit ReverbEffect(const ReverbConfig& config);

    void process(float* samples, std::size_t sample_count, std::uint32_t channels) override;
    void reset() override;

    // Configuration
    void set_config(const ReverbConfig& config);
    [[nodiscard]] const ReverbConfig& config() const { return m_config; }

    // Individual parameters
    void set_room_size(float size);
    void set_damping(float damping);
    void set_decay_time(float time);
    void set_pre_delay(float delay);

private:
    ReverbConfig m_config;

    // Internal state for reverb processing
    static constexpr std::size_t NUM_COMBS = 8;
    static constexpr std::size_t NUM_ALLPASS = 4;

    struct CombFilter {
        std::vector<float> buffer;
        std::size_t index = 0;
        float feedback = 0.5f;
        float damp = 0.5f;
        float last = 0;
    };

    struct AllpassFilter {
        std::vector<float> buffer;
        std::size_t index = 0;
        float feedback = 0.5f;
    };

    std::array<CombFilter, NUM_COMBS> m_combs_l;
    std::array<CombFilter, NUM_COMBS> m_combs_r;
    std::array<AllpassFilter, NUM_ALLPASS> m_allpass_l;
    std::array<AllpassFilter, NUM_ALLPASS> m_allpass_r;

    std::vector<float> m_pre_delay_buffer;
    std::size_t m_pre_delay_index = 0;
    std::size_t m_pre_delay_samples = 0;

    void update_parameters();
    float process_comb(CombFilter& comb, float input);
    float process_allpass(AllpassFilter& allpass, float input);
};

// =============================================================================
// Delay Effect
// =============================================================================

/// Delay effect implementation
class DelayEffect : public AudioEffectBase {
public:
    DelayEffect();
    explicit DelayEffect(const DelayConfig& config);

    void process(float* samples, std::size_t sample_count, std::uint32_t channels) override;
    void reset() override;

    void set_config(const DelayConfig& config);
    [[nodiscard]] const DelayConfig& config() const { return m_config; }

    void set_delay_time(float seconds);
    void set_feedback(float feedback);
    void set_ping_pong(bool enabled);

private:
    DelayConfig m_config;

    std::vector<float> m_buffer_l;
    std::vector<float> m_buffer_r;
    std::size_t m_write_index = 0;
    std::size_t m_delay_samples = 0;
    std::uint32_t m_sample_rate = 44100;
};

// =============================================================================
// Filter Effect
// =============================================================================

/// Filter effect implementation (low-pass, high-pass, band-pass)
class FilterEffect : public AudioEffectBase {
public:
    FilterEffect();
    explicit FilterEffect(const FilterConfig& config);

    void process(float* samples, std::size_t sample_count, std::uint32_t channels) override;
    void reset() override;

    void set_config(const FilterConfig& config);
    [[nodiscard]] const FilterConfig& config() const { return m_config; }

    void set_cutoff(float hz);
    void set_resonance(float q);
    void set_filter_type(EffectType type);

private:
    FilterConfig m_config;

    // Biquad filter coefficients
    float m_a0 = 1, m_a1 = 0, m_a2 = 0;
    float m_b0 = 1, m_b1 = 0, m_b2 = 0;

    // Filter state (per channel)
    std::array<float, 2> m_x1 = {0, 0};
    std::array<float, 2> m_x2 = {0, 0};
    std::array<float, 2> m_y1 = {0, 0};
    std::array<float, 2> m_y2 = {0, 0};

    std::uint32_t m_sample_rate = 44100;

    void calculate_coefficients();
};

// =============================================================================
// Compressor Effect
// =============================================================================

/// Compressor/limiter effect implementation
class CompressorEffect : public AudioEffectBase {
public:
    CompressorEffect();
    explicit CompressorEffect(const CompressorConfig& config);

    void process(float* samples, std::size_t sample_count, std::uint32_t channels) override;
    void reset() override;

    void set_config(const CompressorConfig& config);
    [[nodiscard]] const CompressorConfig& config() const { return m_config; }

    void set_threshold(float db);
    void set_ratio(float ratio);
    void set_attack(float seconds);
    void set_release(float seconds);

    /// Get current gain reduction in dB
    [[nodiscard]] float gain_reduction() const { return m_gain_reduction; }

private:
    CompressorConfig m_config;

    float m_envelope = 0;
    float m_gain_reduction = 0;
    std::uint32_t m_sample_rate = 44100;

    float compute_gain(float input_db);
};

// =============================================================================
// Distortion Effect
// =============================================================================

/// Distortion effect implementation
class DistortionEffect : public AudioEffectBase {
public:
    DistortionEffect();
    explicit DistortionEffect(const DistortionConfig& config);

    void process(float* samples, std::size_t sample_count, std::uint32_t channels) override;
    void reset() override;

    void set_config(const DistortionConfig& config);
    [[nodiscard]] const DistortionConfig& config() const { return m_config; }

    void set_drive(float drive);
    void set_mode(DistortionConfig::Mode mode);

private:
    DistortionConfig m_config;

    // Bitcrush state
    float m_bitcrush_hold = 0;
    std::uint32_t m_bitcrush_counter = 0;

    float distort_sample(float sample);
};

// =============================================================================
// Chorus Effect
// =============================================================================

/// Chorus effect implementation
class ChorusEffect : public AudioEffectBase {
public:
    ChorusEffect();
    explicit ChorusEffect(const ChorusConfig& config);

    void process(float* samples, std::size_t sample_count, std::uint32_t channels) override;
    void reset() override;

    void set_config(const ChorusConfig& config);
    [[nodiscard]] const ChorusConfig& config() const { return m_config; }

    void set_rate(float hz);
    void set_depth(float depth);
    void set_voices(std::uint8_t count);

private:
    ChorusConfig m_config;

    std::vector<float> m_delay_buffer_l;
    std::vector<float> m_delay_buffer_r;
    std::size_t m_write_index = 0;

    std::vector<float> m_lfo_phases;
    std::uint32_t m_sample_rate = 44100;

    float read_delay(const std::vector<float>& buffer, float delay_samples);
};

// =============================================================================
// Equalizer Effect
// =============================================================================

/// Parametric equalizer effect
class EQEffect : public AudioEffectBase {
public:
    EQEffect();
    explicit EQEffect(const EQConfig& config);

    void process(float* samples, std::size_t sample_count, std::uint32_t channels) override;
    void reset() override;

    void set_config(const EQConfig& config);
    [[nodiscard]] const EQConfig& config() const { return m_config; }

    void set_band(std::size_t index, const EQBand& band);
    void set_band_gain(std::size_t index, float db);
    void set_band_frequency(std::size_t index, float hz);
    void set_band_q(std::size_t index, float q);

private:
    EQConfig m_config;

    // Biquad state per band, per channel
    struct BandState {
        float a0 = 1, a1 = 0, a2 = 0;
        float b0 = 1, b1 = 0, b2 = 0;
        std::array<float, 2> x1 = {0, 0};
        std::array<float, 2> x2 = {0, 0};
        std::array<float, 2> y1 = {0, 0};
        std::array<float, 2> y2 = {0, 0};
    };

    std::vector<BandState> m_band_states;
    std::uint32_t m_sample_rate = 44100;

    void calculate_band_coefficients(std::size_t index);
};

// =============================================================================
// Effect Factory
// =============================================================================

/// Factory for creating audio effects
class AudioEffectFactory {
public:
    /// Create effect from type
    static EffectPtr create(EffectType type);

    /// Create reverb effect
    static EffectPtr create_reverb(const ReverbConfig& config = ReverbConfig{});

    /// Create delay effect
    static EffectPtr create_delay(const DelayConfig& config = DelayConfig{});

    /// Create filter effect
    static EffectPtr create_filter(const FilterConfig& config = FilterConfig{});

    /// Create compressor effect
    static EffectPtr create_compressor(const CompressorConfig& config = CompressorConfig{});

    /// Create distortion effect
    static EffectPtr create_distortion(const DistortionConfig& config = DistortionConfig{});

    /// Create chorus effect
    static EffectPtr create_chorus(const ChorusConfig& config = ChorusConfig{});

    /// Create EQ effect
    static EffectPtr create_eq(const EQConfig& config = EQConfig{});
};

// =============================================================================
// Effect Chain
// =============================================================================

/// Chain of effects to process in sequence
class EffectChain {
public:
    /// Add effect to end of chain
    void add(EffectPtr effect);

    /// Insert effect at position
    void insert(std::size_t index, EffectPtr effect);

    /// Remove effect by ID
    void remove(EffectId id);

    /// Remove effect at index
    void remove_at(std::size_t index);

    /// Clear all effects
    void clear();

    /// Get effect by ID
    [[nodiscard]] IAudioEffect* get(EffectId id);
    [[nodiscard]] const IAudioEffect* get(EffectId id) const;

    /// Get effect at index
    [[nodiscard]] IAudioEffect* at(std::size_t index);
    [[nodiscard]] const IAudioEffect* at(std::size_t index) const;

    /// Get number of effects
    [[nodiscard]] std::size_t size() const { return m_effects.size(); }

    /// Check if empty
    [[nodiscard]] bool empty() const { return m_effects.empty(); }

    /// Process audio through chain
    void process(float* samples, std::size_t sample_count, std::uint32_t channels);

    /// Reset all effects
    void reset();

    /// Get all effects
    [[nodiscard]] const std::vector<EffectPtr>& effects() const { return m_effects; }

private:
    std::vector<EffectPtr> m_effects;
};

} // namespace void_audio
