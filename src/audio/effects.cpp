/// @file effects.cpp
/// @brief Audio effect implementations for void_audio

#include <void_engine/audio/effects.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace void_audio {

// =============================================================================
// AudioEffectBase Implementation
// =============================================================================

void AudioEffectBase::apply_mix(const float* dry, float* wet, std::size_t count) {
    if (m_mix >= 1.0f) return; // Full wet
    if (m_mix <= 0.0f) {
        // Full dry
        std::memcpy(wet, dry, count * sizeof(float));
        return;
    }

    float wet_amount = m_mix;
    float dry_amount = 1.0f - m_mix;

    for (std::size_t i = 0; i < count; ++i) {
        wet[i] = dry[i] * dry_amount + wet[i] * wet_amount;
    }
}

// =============================================================================
// ReverbEffect Implementation
// =============================================================================

ReverbEffect::ReverbEffect() {
    m_type = EffectType::Reverb;
    m_name = "Reverb";
    update_parameters();
}

ReverbEffect::ReverbEffect(const ReverbConfig& config)
    : m_config(config) {
    m_type = EffectType::Reverb;
    m_name = "Reverb";
    m_mix = config.mix;
    m_enabled = config.enabled;
    update_parameters();
}

void ReverbEffect::update_parameters() {
    // Initialize comb filters with different delays
    constexpr std::size_t comb_delays[] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
    constexpr std::size_t allpass_delays[] = {225, 556, 441, 341};

    for (std::size_t i = 0; i < NUM_COMBS; ++i) {
        std::size_t delay = static_cast<std::size_t>(comb_delays[i] * m_config.room_size);
        m_combs_l[i].buffer.resize(delay, 0.0f);
        m_combs_r[i].buffer.resize(delay, 0.0f);
        m_combs_l[i].index = 0;
        m_combs_r[i].index = 0;
        m_combs_l[i].feedback = std::pow(0.001f, static_cast<float>(delay) / (m_config.decay_time * 44100.0f));
        m_combs_r[i].feedback = m_combs_l[i].feedback;
        m_combs_l[i].damp = m_config.damping;
        m_combs_r[i].damp = m_config.damping;
    }

    for (std::size_t i = 0; i < NUM_ALLPASS; ++i) {
        m_allpass_l[i].buffer.resize(allpass_delays[i], 0.0f);
        m_allpass_r[i].buffer.resize(allpass_delays[i], 0.0f);
        m_allpass_l[i].index = 0;
        m_allpass_r[i].index = 0;
        m_allpass_l[i].feedback = 0.5f;
        m_allpass_r[i].feedback = 0.5f;
    }

    // Pre-delay buffer
    std::size_t pre_delay_samples = static_cast<std::size_t>(m_config.pre_delay * 44100.0f);
    m_pre_delay_buffer.resize(std::max(pre_delay_samples, std::size_t(1)), 0.0f);
    m_pre_delay_samples = pre_delay_samples;
}

float ReverbEffect::process_comb(CombFilter& comb, float input) {
    float output = comb.buffer[comb.index];

    // Low-pass filter in feedback path
    comb.last = output * (1.0f - comb.damp) + comb.last * comb.damp;

    comb.buffer[comb.index] = input + comb.last * comb.feedback;

    comb.index++;
    if (comb.index >= comb.buffer.size()) {
        comb.index = 0;
    }

    return output;
}

float ReverbEffect::process_allpass(AllpassFilter& allpass, float input) {
    float buffered = allpass.buffer[allpass.index];
    float output = -input + buffered;

    allpass.buffer[allpass.index] = input + buffered * allpass.feedback;

    allpass.index++;
    if (allpass.index >= allpass.buffer.size()) {
        allpass.index = 0;
    }

    return output;
}

void ReverbEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    for (std::size_t i = 0; i < sample_count; ++i) {
        float in_l = samples[i * channels];
        float in_r = channels > 1 ? samples[i * channels + 1] : in_l;

        // Pre-delay
        float delayed = m_pre_delay_buffer[m_pre_delay_index];
        m_pre_delay_buffer[m_pre_delay_index] = (in_l + in_r) * 0.5f;
        m_pre_delay_index++;
        if (m_pre_delay_index >= m_pre_delay_samples) {
            m_pre_delay_index = 0;
        }

        // Parallel comb filters
        float out_l = 0, out_r = 0;
        for (std::size_t c = 0; c < NUM_COMBS; ++c) {
            out_l += process_comb(m_combs_l[c], delayed);
            out_r += process_comb(m_combs_r[c], delayed);
        }

        // Series allpass filters
        for (std::size_t a = 0; a < NUM_ALLPASS; ++a) {
            out_l = process_allpass(m_allpass_l[a], out_l);
            out_r = process_allpass(m_allpass_r[a], out_r);
        }

        samples[i * channels] = out_l * 0.25f;
        if (channels > 1) {
            samples[i * channels + 1] = out_r * 0.25f;
        }
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void ReverbEffect::reset() {
    for (auto& comb : m_combs_l) {
        std::fill(comb.buffer.begin(), comb.buffer.end(), 0.0f);
        comb.index = 0;
        comb.last = 0;
    }
    for (auto& comb : m_combs_r) {
        std::fill(comb.buffer.begin(), comb.buffer.end(), 0.0f);
        comb.index = 0;
        comb.last = 0;
    }
    for (auto& ap : m_allpass_l) {
        std::fill(ap.buffer.begin(), ap.buffer.end(), 0.0f);
        ap.index = 0;
    }
    for (auto& ap : m_allpass_r) {
        std::fill(ap.buffer.begin(), ap.buffer.end(), 0.0f);
        ap.index = 0;
    }
    std::fill(m_pre_delay_buffer.begin(), m_pre_delay_buffer.end(), 0.0f);
}

void ReverbEffect::set_config(const ReverbConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;
    update_parameters();
}

void ReverbEffect::set_room_size(float size) {
    m_config.room_size = std::clamp(size, 0.0f, 1.0f);
    update_parameters();
}

void ReverbEffect::set_damping(float damping) {
    m_config.damping = std::clamp(damping, 0.0f, 1.0f);
    for (auto& comb : m_combs_l) comb.damp = m_config.damping;
    for (auto& comb : m_combs_r) comb.damp = m_config.damping;
}

void ReverbEffect::set_decay_time(float time) {
    m_config.decay_time = std::max(0.1f, time);
    update_parameters();
}

void ReverbEffect::set_pre_delay(float delay) {
    m_config.pre_delay = std::max(0.0f, delay);
    update_parameters();
}

// =============================================================================
// DelayEffect Implementation
// =============================================================================

DelayEffect::DelayEffect() {
    m_type = EffectType::Delay;
    m_name = "Delay";

    // Default 1 second buffer
    m_delay_samples = static_cast<std::size_t>(m_config.delay_time * m_sample_rate);
    m_buffer_l.resize(m_sample_rate, 0.0f);
    m_buffer_r.resize(m_sample_rate, 0.0f);
}

DelayEffect::DelayEffect(const DelayConfig& config)
    : m_config(config) {
    m_type = EffectType::Delay;
    m_name = "Delay";
    m_mix = config.mix;
    m_enabled = config.enabled;

    m_delay_samples = static_cast<std::size_t>(config.delay_time * m_sample_rate);
    m_buffer_l.resize(m_sample_rate * 2, 0.0f); // 2 second max
    m_buffer_r.resize(m_sample_rate * 2, 0.0f);
}

void DelayEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    for (std::size_t i = 0; i < sample_count; ++i) {
        // Read from delay buffer
        std::size_t read_index = (m_write_index + m_buffer_l.size() - m_delay_samples) % m_buffer_l.size();

        float delayed_l = m_buffer_l[read_index];
        float delayed_r = m_config.ping_pong ? m_buffer_r[read_index] : delayed_l;

        // Write to delay buffer (input + feedback)
        float in_l = samples[i * channels];
        float in_r = channels > 1 ? samples[i * channels + 1] : in_l;

        if (m_config.ping_pong) {
            m_buffer_l[m_write_index] = in_l + delayed_r * m_config.feedback;
            m_buffer_r[m_write_index] = in_r + delayed_l * m_config.feedback;
        } else {
            m_buffer_l[m_write_index] = in_l + delayed_l * m_config.feedback;
            m_buffer_r[m_write_index] = in_r + delayed_r * m_config.feedback;
        }

        // Output
        samples[i * channels] = delayed_l;
        if (channels > 1) {
            samples[i * channels + 1] = delayed_r;
        }

        m_write_index++;
        if (m_write_index >= m_buffer_l.size()) {
            m_write_index = 0;
        }
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void DelayEffect::reset() {
    std::fill(m_buffer_l.begin(), m_buffer_l.end(), 0.0f);
    std::fill(m_buffer_r.begin(), m_buffer_r.end(), 0.0f);
    m_write_index = 0;
}

void DelayEffect::set_config(const DelayConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;
    m_delay_samples = static_cast<std::size_t>(config.delay_time * m_sample_rate);
}

void DelayEffect::set_delay_time(float seconds) {
    m_config.delay_time = std::clamp(seconds, 0.001f, 2.0f);
    m_delay_samples = static_cast<std::size_t>(m_config.delay_time * m_sample_rate);
}

void DelayEffect::set_feedback(float feedback) {
    m_config.feedback = std::clamp(feedback, 0.0f, 0.99f);
}

void DelayEffect::set_ping_pong(bool enabled) {
    m_config.ping_pong = enabled;
}

// =============================================================================
// FilterEffect Implementation
// =============================================================================

FilterEffect::FilterEffect() {
    m_type = EffectType::LowPassFilter;
    m_name = "Filter";
    calculate_coefficients();
}

FilterEffect::FilterEffect(const FilterConfig& config)
    : m_config(config) {
    m_type = config.type;
    m_name = "Filter";
    m_mix = config.mix;
    m_enabled = config.enabled;
    calculate_coefficients();
}

void FilterEffect::calculate_coefficients() {
    constexpr float pi = 3.14159265358979323846f;
    float omega = 2.0f * pi * m_config.cutoff / static_cast<float>(m_sample_rate);
    float sin_omega = std::sin(omega);
    float cos_omega = std::cos(omega);
    float alpha = sin_omega / (2.0f * m_config.resonance);

    switch (m_type) {
        case EffectType::LowPassFilter:
            m_b0 = (1.0f - cos_omega) / 2.0f;
            m_b1 = 1.0f - cos_omega;
            m_b2 = (1.0f - cos_omega) / 2.0f;
            m_a0 = 1.0f + alpha;
            m_a1 = -2.0f * cos_omega;
            m_a2 = 1.0f - alpha;
            break;

        case EffectType::HighPassFilter:
            m_b0 = (1.0f + cos_omega) / 2.0f;
            m_b1 = -(1.0f + cos_omega);
            m_b2 = (1.0f + cos_omega) / 2.0f;
            m_a0 = 1.0f + alpha;
            m_a1 = -2.0f * cos_omega;
            m_a2 = 1.0f - alpha;
            break;

        case EffectType::BandPassFilter:
            m_b0 = alpha;
            m_b1 = 0;
            m_b2 = -alpha;
            m_a0 = 1.0f + alpha;
            m_a1 = -2.0f * cos_omega;
            m_a2 = 1.0f - alpha;
            break;

        default:
            // Unity filter
            m_b0 = 1;
            m_b1 = 0;
            m_b2 = 0;
            m_a0 = 1;
            m_a1 = 0;
            m_a2 = 0;
            break;
    }

    // Normalize
    m_b0 /= m_a0;
    m_b1 /= m_a0;
    m_b2 /= m_a0;
    m_a1 /= m_a0;
    m_a2 /= m_a0;
}

void FilterEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    for (std::size_t i = 0; i < sample_count; ++i) {
        for (std::uint32_t c = 0; c < std::min(channels, 2u); ++c) {
            float x = samples[i * channels + c];

            // Biquad filter
            float y = m_b0 * x + m_b1 * m_x1[c] + m_b2 * m_x2[c]
                    - m_a1 * m_y1[c] - m_a2 * m_y2[c];

            m_x2[c] = m_x1[c];
            m_x1[c] = x;
            m_y2[c] = m_y1[c];
            m_y1[c] = y;

            samples[i * channels + c] = y;
        }
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void FilterEffect::reset() {
    m_x1 = {0, 0};
    m_x2 = {0, 0};
    m_y1 = {0, 0};
    m_y2 = {0, 0};
}

void FilterEffect::set_config(const FilterConfig& config) {
    m_config = config;
    m_type = config.type;
    m_mix = config.mix;
    m_enabled = config.enabled;
    calculate_coefficients();
}

void FilterEffect::set_cutoff(float hz) {
    m_config.cutoff = std::clamp(hz, 20.0f, 20000.0f);
    calculate_coefficients();
}

void FilterEffect::set_resonance(float q) {
    m_config.resonance = std::clamp(q, 0.1f, 10.0f);
    calculate_coefficients();
}

void FilterEffect::set_filter_type(EffectType type) {
    m_type = type;
    m_config.type = type;
    calculate_coefficients();
}

// =============================================================================
// CompressorEffect Implementation
// =============================================================================

CompressorEffect::CompressorEffect() {
    m_type = EffectType::Compressor;
    m_name = "Compressor";
}

CompressorEffect::CompressorEffect(const CompressorConfig& config)
    : m_config(config) {
    m_type = EffectType::Compressor;
    m_name = "Compressor";
    m_mix = config.mix;
    m_enabled = config.enabled;
}

float CompressorEffect::compute_gain(float input_db) {
    float threshold = m_config.threshold;
    float ratio = m_config.ratio;
    float knee = m_config.knee;

    if (knee > 0) {
        // Soft knee
        float knee_start = threshold - knee / 2.0f;
        float knee_end = threshold + knee / 2.0f;

        if (input_db < knee_start) {
            return 0; // No compression
        } else if (input_db > knee_end) {
            return (threshold - input_db) * (1.0f - 1.0f / ratio);
        } else {
            // In knee region
            float x = input_db - knee_start;
            float a = 1.0f / ratio - 1.0f;
            return a * x * x / (2.0f * knee);
        }
    } else {
        // Hard knee
        if (input_db < threshold) {
            return 0;
        }
        return (threshold - input_db) * (1.0f - 1.0f / ratio);
    }
}

void CompressorEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    float attack_coeff = std::exp(-1.0f / (m_config.attack * static_cast<float>(m_sample_rate)));
    float release_coeff = std::exp(-1.0f / (m_config.release * static_cast<float>(m_sample_rate)));

    for (std::size_t i = 0; i < sample_count; ++i) {
        // Get peak level across channels
        float peak = 0;
        for (std::uint32_t c = 0; c < channels; ++c) {
            peak = std::max(peak, std::abs(samples[i * channels + c]));
        }

        // Convert to dB
        float input_db = 20.0f * std::log10(std::max(peak, 1e-6f));

        // Compute gain reduction
        float target_gr = compute_gain(input_db);

        // Smooth envelope
        float coeff = target_gr < m_envelope ? attack_coeff : release_coeff;
        m_envelope = m_envelope * coeff + target_gr * (1.0f - coeff);

        // Apply gain (including makeup)
        float gain_db = m_envelope + m_config.makeup_gain;
        float gain_linear = std::pow(10.0f, gain_db / 20.0f);

        m_gain_reduction = -m_envelope;

        for (std::uint32_t c = 0; c < channels; ++c) {
            samples[i * channels + c] *= gain_linear;
        }
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void CompressorEffect::reset() {
    m_envelope = 0;
    m_gain_reduction = 0;
}

void CompressorEffect::set_config(const CompressorConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;
}

void CompressorEffect::set_threshold(float db) {
    m_config.threshold = std::clamp(db, -60.0f, 0.0f);
}

void CompressorEffect::set_ratio(float ratio) {
    m_config.ratio = std::clamp(ratio, 1.0f, 100.0f);
}

void CompressorEffect::set_attack(float seconds) {
    m_config.attack = std::clamp(seconds, 0.0001f, 1.0f);
}

void CompressorEffect::set_release(float seconds) {
    m_config.release = std::clamp(seconds, 0.001f, 5.0f);
}

// =============================================================================
// DistortionEffect Implementation
// =============================================================================

DistortionEffect::DistortionEffect() {
    m_type = EffectType::Distortion;
    m_name = "Distortion";
}

DistortionEffect::DistortionEffect(const DistortionConfig& config)
    : m_config(config) {
    m_type = EffectType::Distortion;
    m_name = "Distortion";
    m_mix = config.mix;
    m_enabled = config.enabled;
}

float DistortionEffect::distort_sample(float sample) {
    float drive = 1.0f + m_config.drive * 10.0f;
    sample *= drive;

    switch (m_config.mode) {
        case DistortionConfig::Mode::SoftClip:
            return std::tanh(sample);

        case DistortionConfig::Mode::HardClip:
            return std::clamp(sample, -1.0f, 1.0f);

        case DistortionConfig::Mode::Tube: {
            float x = sample;
            if (x > 0) {
                return 1.0f - std::exp(-x);
            } else {
                return -1.0f + std::exp(x);
            }
        }

        case DistortionConfig::Mode::Fuzz: {
            float sign = sample >= 0 ? 1.0f : -1.0f;
            float x = std::abs(sample);
            return sign * (1.0f - std::pow(1.0f - std::min(x, 1.0f), 3.0f));
        }

        case DistortionConfig::Mode::Bitcrush: {
            // Reduce bit depth
            float scale = std::pow(2.0f, m_config.bit_depth - 1.0f);
            return std::round(sample * scale) / scale;
        }
    }

    return sample;
}

void DistortionEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    for (std::size_t i = 0; i < sample_count * channels; ++i) {
        samples[i] = distort_sample(samples[i]) * m_config.output;
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void DistortionEffect::reset() {
    m_bitcrush_hold = 0;
    m_bitcrush_counter = 0;
}

void DistortionEffect::set_config(const DistortionConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;
}

void DistortionEffect::set_drive(float drive) {
    m_config.drive = std::clamp(drive, 0.0f, 1.0f);
}

void DistortionEffect::set_mode(DistortionConfig::Mode mode) {
    m_config.mode = mode;
}

// =============================================================================
// ChorusEffect Implementation
// =============================================================================

ChorusEffect::ChorusEffect() {
    m_type = EffectType::Chorus;
    m_name = "Chorus";

    // Initialize delay buffers
    std::size_t buffer_size = static_cast<std::size_t>(0.1f * m_sample_rate);
    m_delay_buffer_l.resize(buffer_size, 0.0f);
    m_delay_buffer_r.resize(buffer_size, 0.0f);

    // Initialize LFO phases
    m_lfo_phases.resize(m_config.voices, 0.0f);
    for (std::uint8_t i = 0; i < m_config.voices; ++i) {
        m_lfo_phases[i] = static_cast<float>(i) / static_cast<float>(m_config.voices);
    }
}

ChorusEffect::ChorusEffect(const ChorusConfig& config)
    : m_config(config) {
    m_type = EffectType::Chorus;
    m_name = "Chorus";
    m_mix = config.mix;
    m_enabled = config.enabled;

    std::size_t buffer_size = static_cast<std::size_t>(0.1f * m_sample_rate);
    m_delay_buffer_l.resize(buffer_size, 0.0f);
    m_delay_buffer_r.resize(buffer_size, 0.0f);

    m_lfo_phases.resize(config.voices, 0.0f);
    for (std::uint8_t i = 0; i < config.voices; ++i) {
        m_lfo_phases[i] = static_cast<float>(i) / static_cast<float>(config.voices);
    }
}

float ChorusEffect::read_delay(const std::vector<float>& buffer, float delay_samples) {
    // Linear interpolation
    std::size_t index = static_cast<std::size_t>(m_write_index + buffer.size() - delay_samples) % buffer.size();
    std::size_t next_index = (index + 1) % buffer.size();
    float frac = delay_samples - std::floor(delay_samples);

    return buffer[index] * (1.0f - frac) + buffer[next_index] * frac;
}

void ChorusEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    constexpr float pi = 3.14159265358979323846f;
    float lfo_increment = m_config.rate / static_cast<float>(m_sample_rate);

    for (std::size_t i = 0; i < sample_count; ++i) {
        float in_l = samples[i * channels];
        float in_r = channels > 1 ? samples[i * channels + 1] : in_l;

        // Write to delay buffer
        m_delay_buffer_l[m_write_index] = in_l;
        m_delay_buffer_r[m_write_index] = in_r;

        float out_l = 0, out_r = 0;

        // Sum all voices
        for (std::uint8_t v = 0; v < m_config.voices; ++v) {
            // LFO modulates delay time
            float lfo = std::sin(2.0f * pi * m_lfo_phases[v]);
            float delay_ms = m_config.delay * 1000.0f + lfo * m_config.depth * 10.0f;
            float delay_samples = delay_ms * static_cast<float>(m_sample_rate) / 1000.0f;

            // Stereo spread
            float pan = (static_cast<float>(v) / static_cast<float>(m_config.voices) - 0.5f) *
                       m_config.stereo_width;

            float voice_l = read_delay(m_delay_buffer_l, delay_samples);
            float voice_r = read_delay(m_delay_buffer_r, delay_samples);

            out_l += voice_l * (0.5f - pan * 0.5f);
            out_r += voice_r * (0.5f + pan * 0.5f);

            // Update LFO phase
            m_lfo_phases[v] += lfo_increment;
            if (m_lfo_phases[v] >= 1.0f) m_lfo_phases[v] -= 1.0f;
        }

        samples[i * channels] = out_l / static_cast<float>(m_config.voices);
        if (channels > 1) {
            samples[i * channels + 1] = out_r / static_cast<float>(m_config.voices);
        }

        m_write_index++;
        if (m_write_index >= m_delay_buffer_l.size()) {
            m_write_index = 0;
        }
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void ChorusEffect::reset() {
    std::fill(m_delay_buffer_l.begin(), m_delay_buffer_l.end(), 0.0f);
    std::fill(m_delay_buffer_r.begin(), m_delay_buffer_r.end(), 0.0f);
    m_write_index = 0;
    for (auto& phase : m_lfo_phases) {
        phase = 0;
    }
}

void ChorusEffect::set_config(const ChorusConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;

    m_lfo_phases.resize(config.voices, 0.0f);
}

void ChorusEffect::set_rate(float hz) {
    m_config.rate = std::clamp(hz, 0.01f, 10.0f);
}

void ChorusEffect::set_depth(float depth) {
    m_config.depth = std::clamp(depth, 0.0f, 1.0f);
}

void ChorusEffect::set_voices(std::uint8_t count) {
    m_config.voices = std::max(std::uint8_t(1), count);
    m_lfo_phases.resize(m_config.voices, 0.0f);
}

// =============================================================================
// EQEffect Implementation
// =============================================================================

EQEffect::EQEffect() {
    m_type = EffectType::Equalizer;
    m_name = "EQ";
}

EQEffect::EQEffect(const EQConfig& config)
    : m_config(config) {
    m_type = EffectType::Equalizer;
    m_name = "EQ";
    m_mix = config.mix;
    m_enabled = config.enabled;

    m_band_states.resize(config.bands.size());
    for (std::size_t i = 0; i < config.bands.size(); ++i) {
        calculate_band_coefficients(i);
    }
}

void EQEffect::calculate_band_coefficients(std::size_t index) {
    if (index >= m_config.bands.size()) return;

    const auto& band = m_config.bands[index];
    auto& state = m_band_states[index];

    constexpr float pi = 3.14159265358979323846f;
    float omega = 2.0f * pi * band.frequency / static_cast<float>(m_sample_rate);
    float sin_omega = std::sin(omega);
    float cos_omega = std::cos(omega);
    float A = std::pow(10.0f, band.gain / 40.0f);
    float alpha = sin_omega / (2.0f * band.q);

    switch (band.type) {
        case EQBand::Type::Peak:
            state.b0 = 1.0f + alpha * A;
            state.b1 = -2.0f * cos_omega;
            state.b2 = 1.0f - alpha * A;
            state.a0 = 1.0f + alpha / A;
            state.a1 = -2.0f * cos_omega;
            state.a2 = 1.0f - alpha / A;
            break;

        case EQBand::Type::LowShelf: {
            float sqrtA = std::sqrt(A);
            state.b0 = A * ((A + 1) - (A - 1) * cos_omega + 2 * sqrtA * alpha);
            state.b1 = 2 * A * ((A - 1) - (A + 1) * cos_omega);
            state.b2 = A * ((A + 1) - (A - 1) * cos_omega - 2 * sqrtA * alpha);
            state.a0 = (A + 1) + (A - 1) * cos_omega + 2 * sqrtA * alpha;
            state.a1 = -2 * ((A - 1) + (A + 1) * cos_omega);
            state.a2 = (A + 1) + (A - 1) * cos_omega - 2 * sqrtA * alpha;
            break;
        }

        case EQBand::Type::HighShelf: {
            float sqrtA = std::sqrt(A);
            state.b0 = A * ((A + 1) + (A - 1) * cos_omega + 2 * sqrtA * alpha);
            state.b1 = -2 * A * ((A - 1) + (A + 1) * cos_omega);
            state.b2 = A * ((A + 1) + (A - 1) * cos_omega - 2 * sqrtA * alpha);
            state.a0 = (A + 1) - (A - 1) * cos_omega + 2 * sqrtA * alpha;
            state.a1 = 2 * ((A - 1) - (A + 1) * cos_omega);
            state.a2 = (A + 1) - (A - 1) * cos_omega - 2 * sqrtA * alpha;
            break;
        }

        default:
            state.b0 = 1;
            state.b1 = 0;
            state.b2 = 0;
            state.a0 = 1;
            state.a1 = 0;
            state.a2 = 0;
            break;
    }

    // Normalize
    state.b0 /= state.a0;
    state.b1 /= state.a0;
    state.b2 /= state.a0;
    state.a1 /= state.a0;
    state.a2 /= state.a0;
}

void EQEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    // Process each band in series
    for (std::size_t b = 0; b < m_config.bands.size(); ++b) {
        if (!m_config.bands[b].enabled) continue;

        auto& state = m_band_states[b];

        for (std::size_t i = 0; i < sample_count; ++i) {
            for (std::uint32_t c = 0; c < std::min(channels, 2u); ++c) {
                float x = samples[i * channels + c];

                float y = state.b0 * x + state.b1 * state.x1[c] + state.b2 * state.x2[c]
                        - state.a1 * state.y1[c] - state.a2 * state.y2[c];

                state.x2[c] = state.x1[c];
                state.x1[c] = x;
                state.y2[c] = state.y1[c];
                state.y1[c] = y;

                samples[i * channels + c] = y;
            }
        }
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void EQEffect::reset() {
    for (auto& state : m_band_states) {
        state.x1 = {0, 0};
        state.x2 = {0, 0};
        state.y1 = {0, 0};
        state.y2 = {0, 0};
    }
}

void EQEffect::set_config(const EQConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;

    m_band_states.resize(config.bands.size());
    for (std::size_t i = 0; i < config.bands.size(); ++i) {
        calculate_band_coefficients(i);
    }
}

void EQEffect::set_band(std::size_t index, const EQBand& band) {
    if (index >= m_config.bands.size()) return;
    m_config.bands[index] = band;
    calculate_band_coefficients(index);
}

void EQEffect::set_band_gain(std::size_t index, float db) {
    if (index >= m_config.bands.size()) return;
    m_config.bands[index].gain = std::clamp(db, -24.0f, 24.0f);
    calculate_band_coefficients(index);
}

void EQEffect::set_band_frequency(std::size_t index, float hz) {
    if (index >= m_config.bands.size()) return;
    m_config.bands[index].frequency = std::clamp(hz, 20.0f, 20000.0f);
    calculate_band_coefficients(index);
}

void EQEffect::set_band_q(std::size_t index, float q) {
    if (index >= m_config.bands.size()) return;
    m_config.bands[index].q = std::clamp(q, 0.1f, 10.0f);
    calculate_band_coefficients(index);
}

// =============================================================================
// LimiterEffect Implementation
// =============================================================================

LimiterEffect::LimiterEffect() {
    m_type = EffectType::Limiter;
    m_name = "Limiter";
    m_lookahead_buffer.resize(LOOKAHEAD_SAMPLES * 2, 0.0f);
}

LimiterEffect::LimiterEffect(const LimiterConfig& config)
    : m_config(config) {
    m_type = EffectType::Limiter;
    m_name = "Limiter";
    m_mix = config.mix;
    m_enabled = config.enabled;
    m_lookahead_buffer.resize(LOOKAHEAD_SAMPLES * 2, 0.0f);
}

void LimiterEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    float threshold_linear = std::pow(10.0f, m_config.threshold / 20.0f);
    float ceiling_linear = std::pow(10.0f, m_config.ceiling / 20.0f);
    float release_coeff = std::exp(-1.0f / (m_config.release * static_cast<float>(m_sample_rate)));

    for (std::size_t i = 0; i < sample_count; ++i) {
        // Get peak level across channels
        float peak = 0;
        for (std::uint32_t c = 0; c < channels; ++c) {
            peak = std::max(peak, std::abs(samples[i * channels + c]));
        }

        // Compute required gain reduction
        float target_gain = 1.0f;
        if (peak > threshold_linear) {
            if (m_config.soft_knee) {
                // Soft knee limiting
                float knee_width = 0.1f * threshold_linear;
                if (peak < threshold_linear + knee_width) {
                    float x = (peak - threshold_linear) / knee_width;
                    target_gain = 1.0f - x * x * 0.5f * (1.0f - threshold_linear / peak);
                } else {
                    target_gain = threshold_linear / peak;
                }
            } else {
                target_gain = threshold_linear / peak;
            }
        }

        // Smooth envelope (instant attack, smooth release)
        if (target_gain < m_envelope) {
            m_envelope = target_gain; // Instant attack
        } else {
            m_envelope = m_envelope * release_coeff + target_gain * (1.0f - release_coeff);
        }

        // Apply ceiling
        float gain = m_envelope * ceiling_linear / threshold_linear;
        gain = std::min(gain, ceiling_linear);

        m_gain_reduction = 20.0f * std::log10(std::max(m_envelope, 1e-6f));

        // Apply gain
        for (std::uint32_t c = 0; c < channels; ++c) {
            samples[i * channels + c] *= gain;
            // Hard clip at ceiling as safety
            samples[i * channels + c] = std::clamp(samples[i * channels + c], -ceiling_linear, ceiling_linear);
        }
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void LimiterEffect::reset() {
    m_envelope = 0;
    m_gain_reduction = 0;
    std::fill(m_lookahead_buffer.begin(), m_lookahead_buffer.end(), 0.0f);
    m_lookahead_index = 0;
}

void LimiterEffect::set_config(const LimiterConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;
}

void LimiterEffect::set_threshold(float db) {
    m_config.threshold = std::clamp(db, -30.0f, 0.0f);
}

void LimiterEffect::set_release(float seconds) {
    m_config.release = std::clamp(seconds, 0.001f, 1.0f);
}

void LimiterEffect::set_ceiling(float db) {
    m_config.ceiling = std::clamp(db, -10.0f, 0.0f);
}

// =============================================================================
// FlangerEffect Implementation
// =============================================================================

FlangerEffect::FlangerEffect() {
    m_type = EffectType::Flanger;
    m_name = "Flanger";

    // Flanger uses short delays (1-10ms), allocate 50ms buffer
    std::size_t buffer_size = static_cast<std::size_t>(0.05f * m_sample_rate);
    m_delay_buffer_l.resize(buffer_size, 0.0f);
    m_delay_buffer_r.resize(buffer_size, 0.0f);
}

FlangerEffect::FlangerEffect(const FlangerConfig& config)
    : m_config(config) {
    m_type = EffectType::Flanger;
    m_name = "Flanger";
    m_mix = config.mix;
    m_enabled = config.enabled;

    std::size_t buffer_size = static_cast<std::size_t>(0.05f * m_sample_rate);
    m_delay_buffer_l.resize(buffer_size, 0.0f);
    m_delay_buffer_r.resize(buffer_size, 0.0f);
}

float FlangerEffect::read_delay_interpolated(const std::vector<float>& buffer, float delay_samples) {
    float read_pos = static_cast<float>(m_write_index) - delay_samples;
    if (read_pos < 0) read_pos += static_cast<float>(buffer.size());

    std::size_t index = static_cast<std::size_t>(read_pos) % buffer.size();
    std::size_t next_index = (index + 1) % buffer.size();
    float frac = read_pos - std::floor(read_pos);

    return buffer[index] * (1.0f - frac) + buffer[next_index] * frac;
}

void FlangerEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    constexpr float pi = 3.14159265358979323846f;
    float lfo_increment = m_config.rate / static_cast<float>(m_sample_rate);

    for (std::size_t i = 0; i < sample_count; ++i) {
        float in_l = samples[i * channels];
        float in_r = channels > 1 ? samples[i * channels + 1] : in_l;

        // LFO modulation (with stereo phase offset)
        float lfo_l = std::sin(2.0f * pi * m_lfo_phase);
        float lfo_r = std::sin(2.0f * pi * (m_lfo_phase + m_config.stereo_phase));

        // Calculate delay times (1-10ms range modulated by LFO)
        float base_delay_samples = m_config.delay * static_cast<float>(m_sample_rate);
        float mod_range = base_delay_samples * m_config.depth;

        float delay_samples_l = base_delay_samples + lfo_l * mod_range;
        float delay_samples_r = base_delay_samples + lfo_r * mod_range;

        // Clamp delay to valid range
        delay_samples_l = std::clamp(delay_samples_l, 1.0f, static_cast<float>(m_delay_buffer_l.size() - 1));
        delay_samples_r = std::clamp(delay_samples_r, 1.0f, static_cast<float>(m_delay_buffer_r.size() - 1));

        // Read from delay lines
        float delayed_l = read_delay_interpolated(m_delay_buffer_l, delay_samples_l);
        float delayed_r = read_delay_interpolated(m_delay_buffer_r, delay_samples_r);

        // Write to delay lines (input + feedback)
        m_delay_buffer_l[m_write_index] = in_l + delayed_l * m_config.feedback;
        m_delay_buffer_r[m_write_index] = in_r + delayed_r * m_config.feedback;

        // Output
        samples[i * channels] = delayed_l;
        if (channels > 1) {
            samples[i * channels + 1] = delayed_r;
        }

        // Advance write position and LFO
        m_write_index++;
        if (m_write_index >= m_delay_buffer_l.size()) {
            m_write_index = 0;
        }

        m_lfo_phase += lfo_increment;
        if (m_lfo_phase >= 1.0f) m_lfo_phase -= 1.0f;
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void FlangerEffect::reset() {
    std::fill(m_delay_buffer_l.begin(), m_delay_buffer_l.end(), 0.0f);
    std::fill(m_delay_buffer_r.begin(), m_delay_buffer_r.end(), 0.0f);
    m_write_index = 0;
    m_lfo_phase = 0;
}

void FlangerEffect::set_config(const FlangerConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;
}

void FlangerEffect::set_rate(float hz) {
    m_config.rate = std::clamp(hz, 0.01f, 5.0f);
}

void FlangerEffect::set_depth(float depth) {
    m_config.depth = std::clamp(depth, 0.0f, 1.0f);
}

void FlangerEffect::set_feedback(float feedback) {
    m_config.feedback = std::clamp(feedback, -0.99f, 0.99f);
}

// =============================================================================
// PhaserEffect Implementation
// =============================================================================

PhaserEffect::PhaserEffect() {
    m_type = EffectType::Phaser;
    m_name = "Phaser";
}

PhaserEffect::PhaserEffect(const PhaserConfig& config)
    : m_config(config) {
    m_type = EffectType::Phaser;
    m_name = "Phaser";
    m_mix = config.mix;
    m_enabled = config.enabled;
}

void PhaserEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    constexpr float pi = 3.14159265358979323846f;
    float lfo_increment = m_config.rate / static_cast<float>(m_sample_rate);

    std::uint8_t num_stages = std::min(m_config.stages, static_cast<std::uint8_t>(MAX_STAGES));

    for (std::size_t i = 0; i < sample_count; ++i) {
        // LFO for frequency sweep (with stereo phase)
        float lfo_l = (std::sin(2.0f * pi * m_lfo_phase) + 1.0f) * 0.5f;
        float lfo_r = (std::sin(2.0f * pi * (m_lfo_phase + m_config.stereo_phase)) + 1.0f) * 0.5f;

        // Map LFO to frequency range (exponential)
        float freq_l = m_config.min_freq * std::pow(m_config.max_freq / m_config.min_freq, lfo_l * m_config.depth);
        float freq_r = m_config.min_freq * std::pow(m_config.max_freq / m_config.min_freq, lfo_r * m_config.depth);

        // Calculate allpass coefficients
        float a1_l = (std::tan(pi * freq_l / static_cast<float>(m_sample_rate)) - 1.0f) /
                     (std::tan(pi * freq_l / static_cast<float>(m_sample_rate)) + 1.0f);
        float a1_r = (std::tan(pi * freq_r / static_cast<float>(m_sample_rate)) - 1.0f) /
                     (std::tan(pi * freq_r / static_cast<float>(m_sample_rate)) + 1.0f);

        // Input with feedback
        float in_l = samples[i * channels] + m_feedback_l * m_config.feedback;
        float in_r = channels > 1 ? samples[i * channels + 1] + m_feedback_r * m_config.feedback : in_l;

        // Process through allpass stages
        float out_l = in_l;
        float out_r = in_r;

        for (std::uint8_t s = 0; s < num_stages; ++s) {
            m_stages[s].a1 = a1_l;

            // Left channel allpass
            float tmp_l = m_stages[s].a1 * out_l + m_stages[s].z1_l;
            m_stages[s].z1_l = out_l - m_stages[s].a1 * tmp_l;
            out_l = tmp_l;

            // Right channel allpass
            m_stages[s].a1 = a1_r;
            float tmp_r = m_stages[s].a1 * out_r + m_stages[s].z1_r;
            m_stages[s].z1_r = out_r - m_stages[s].a1 * tmp_r;
            out_r = tmp_r;
        }

        // Store for feedback
        m_feedback_l = out_l;
        m_feedback_r = out_r;

        // Output (mix of dry and wet creates notch)
        samples[i * channels] = out_l;
        if (channels > 1) {
            samples[i * channels + 1] = out_r;
        }

        // Advance LFO
        m_lfo_phase += lfo_increment;
        if (m_lfo_phase >= 1.0f) m_lfo_phase -= 1.0f;
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void PhaserEffect::reset() {
    for (auto& stage : m_stages) {
        stage.z1_l = 0;
        stage.z1_r = 0;
    }
    m_lfo_phase = 0;
    m_feedback_l = 0;
    m_feedback_r = 0;
}

void PhaserEffect::set_config(const PhaserConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;
}

void PhaserEffect::set_rate(float hz) {
    m_config.rate = std::clamp(hz, 0.01f, 5.0f);
}

void PhaserEffect::set_depth(float depth) {
    m_config.depth = std::clamp(depth, 0.0f, 1.0f);
}

void PhaserEffect::set_stages(std::uint8_t count) {
    m_config.stages = std::clamp(count, std::uint8_t(2), std::uint8_t(MAX_STAGES));
}

// =============================================================================
// PitchShifterEffect Implementation
// =============================================================================

PitchShifterEffect::PitchShifterEffect() {
    m_type = EffectType::Pitch;
    m_name = "Pitch Shifter";
    update_parameters();
}

PitchShifterEffect::PitchShifterEffect(const PitchConfig& config)
    : m_config(config) {
    m_type = EffectType::Pitch;
    m_name = "Pitch Shifter";
    m_mix = config.mix;
    m_enabled = config.enabled;
    update_parameters();
}

void PitchShifterEffect::update_parameters() {
    // Calculate window size in samples
    m_window_samples = static_cast<std::size_t>(m_config.window_size * m_sample_rate);
    m_window_samples = std::max(m_window_samples, std::size_t(256));

    // Input buffer needs to hold multiple windows
    std::size_t buffer_size = m_window_samples * 4;
    m_input_buffer_l.resize(buffer_size, 0.0f);
    m_input_buffer_r.resize(buffer_size, 0.0f);

    // Calculate grain spacing based on overlap
    m_grain_spacing = m_window_samples * (1.0f - m_config.overlap);
    m_grain_spacing = std::max(m_grain_spacing, 1.0f);
}

float PitchShifterEffect::pitch_ratio() const {
    float semitones = m_config.semitones + m_config.cents / 100.0f;
    return std::pow(2.0f, semitones / 12.0f);
}

float PitchShifterEffect::read_with_window(const std::vector<float>& buffer, float position, float window_pos) {
    // Wrap position
    while (position < 0) position += static_cast<float>(buffer.size());
    while (position >= static_cast<float>(buffer.size())) position -= static_cast<float>(buffer.size());

    // Linear interpolation for read
    std::size_t index = static_cast<std::size_t>(position) % buffer.size();
    std::size_t next = (index + 1) % buffer.size();
    float frac = position - std::floor(position);
    float sample = buffer[index] * (1.0f - frac) + buffer[next] * frac;

    // Hann window
    constexpr float pi = 3.14159265358979323846f;
    float window = 0.5f * (1.0f - std::cos(2.0f * pi * window_pos));

    return sample * window;
}

void PitchShifterEffect::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    if (!m_enabled || channels == 0) return;

    std::vector<float> dry(sample_count * channels);
    std::memcpy(dry.data(), samples, sample_count * channels * sizeof(float));

    float ratio = pitch_ratio();
    float read_increment = ratio;

    for (std::size_t i = 0; i < sample_count; ++i) {
        float in_l = samples[i * channels];
        float in_r = channels > 1 ? samples[i * channels + 1] : in_l;

        // Write to input buffer
        m_input_buffer_l[m_input_write_index] = in_l;
        m_input_buffer_r[m_input_write_index] = in_r;

        // Start new grain if needed
        m_samples_since_grain++;
        if (m_samples_since_grain >= static_cast<std::size_t>(m_grain_spacing)) {
            m_samples_since_grain = 0;

            // Find an inactive grain
            for (std::size_t g = 0; g < NUM_GRAINS; ++g) {
                std::size_t idx = (m_next_grain + g) % NUM_GRAINS;
                if (!m_grains[idx].active) {
                    m_grains[idx].active = true;
                    m_grains[idx].position = static_cast<float>(m_input_write_index);
                    m_grains[idx].window_pos = 0;
                    m_next_grain = (idx + 1) % NUM_GRAINS;
                    break;
                }
            }
        }

        // Mix all active grains
        float out_l = 0, out_r = 0;
        int active_grains = 0;

        for (auto& grain : m_grains) {
            if (!grain.active) continue;

            out_l += read_with_window(m_input_buffer_l, grain.position, grain.window_pos);
            out_r += read_with_window(m_input_buffer_r, grain.position, grain.window_pos);
            active_grains++;

            // Advance grain
            grain.position += read_increment;
            grain.window_pos += 1.0f / static_cast<float>(m_window_samples);

            // Deactivate finished grains
            if (grain.window_pos >= 1.0f) {
                grain.active = false;
            }
        }

        // Normalize output
        if (active_grains > 0) {
            float norm = 1.0f / std::sqrt(static_cast<float>(active_grains));
            out_l *= norm;
            out_r *= norm;
        }

        samples[i * channels] = out_l;
        if (channels > 1) {
            samples[i * channels + 1] = out_r;
        }

        // Advance write position
        m_input_write_index++;
        if (m_input_write_index >= m_input_buffer_l.size()) {
            m_input_write_index = 0;
        }
    }

    apply_mix(dry.data(), samples, sample_count * channels);
}

void PitchShifterEffect::reset() {
    std::fill(m_input_buffer_l.begin(), m_input_buffer_l.end(), 0.0f);
    std::fill(m_input_buffer_r.begin(), m_input_buffer_r.end(), 0.0f);
    m_input_write_index = 0;
    m_samples_since_grain = 0;

    for (auto& grain : m_grains) {
        grain.active = false;
        grain.position = 0;
        grain.window_pos = 0;
    }
}

void PitchShifterEffect::set_config(const PitchConfig& config) {
    m_config = config;
    m_mix = config.mix;
    m_enabled = config.enabled;
    update_parameters();
}

void PitchShifterEffect::set_semitones(float semitones) {
    m_config.semitones = std::clamp(semitones, -24.0f, 24.0f);
}

void PitchShifterEffect::set_cents(float cents) {
    m_config.cents = std::clamp(cents, -100.0f, 100.0f);
}

// =============================================================================
// AudioEffectFactory Implementation
// =============================================================================

EffectPtr AudioEffectFactory::create(EffectType type) {
    switch (type) {
        case EffectType::Reverb:
            return create_reverb();
        case EffectType::Delay:
            return create_delay();
        case EffectType::LowPassFilter:
        case EffectType::HighPassFilter:
        case EffectType::BandPassFilter:
            return create_filter(FilterConfig{type});
        case EffectType::Compressor:
            return create_compressor();
        case EffectType::Limiter:
            return create_limiter();
        case EffectType::Distortion:
            return create_distortion();
        case EffectType::Chorus:
            return create_chorus();
        case EffectType::Flanger:
            return create_flanger();
        case EffectType::Phaser:
            return create_phaser();
        case EffectType::Equalizer:
            return create_eq();
        case EffectType::Pitch:
            return create_pitch();
        default:
            return nullptr;
    }
}

EffectPtr AudioEffectFactory::create_reverb(const ReverbConfig& config) {
    return std::make_shared<ReverbEffect>(config);
}

EffectPtr AudioEffectFactory::create_delay(const DelayConfig& config) {
    return std::make_shared<DelayEffect>(config);
}

EffectPtr AudioEffectFactory::create_filter(const FilterConfig& config) {
    return std::make_shared<FilterEffect>(config);
}

EffectPtr AudioEffectFactory::create_compressor(const CompressorConfig& config) {
    return std::make_shared<CompressorEffect>(config);
}

EffectPtr AudioEffectFactory::create_distortion(const DistortionConfig& config) {
    return std::make_shared<DistortionEffect>(config);
}

EffectPtr AudioEffectFactory::create_chorus(const ChorusConfig& config) {
    return std::make_shared<ChorusEffect>(config);
}

EffectPtr AudioEffectFactory::create_eq(const EQConfig& config) {
    return std::make_shared<EQEffect>(config);
}

EffectPtr AudioEffectFactory::create_limiter(const LimiterConfig& config) {
    return std::make_shared<LimiterEffect>(config);
}

EffectPtr AudioEffectFactory::create_flanger(const FlangerConfig& config) {
    return std::make_shared<FlangerEffect>(config);
}

EffectPtr AudioEffectFactory::create_phaser(const PhaserConfig& config) {
    return std::make_shared<PhaserEffect>(config);
}

EffectPtr AudioEffectFactory::create_pitch(const PitchConfig& config) {
    return std::make_shared<PitchShifterEffect>(config);
}

// =============================================================================
// EffectChain Implementation
// =============================================================================

void EffectChain::add(EffectPtr effect) {
    m_effects.push_back(std::move(effect));
}

void EffectChain::insert(std::size_t index, EffectPtr effect) {
    if (index >= m_effects.size()) {
        m_effects.push_back(std::move(effect));
    } else {
        m_effects.insert(m_effects.begin() + static_cast<std::ptrdiff_t>(index), std::move(effect));
    }
}

void EffectChain::remove(EffectId id) {
    auto it = std::remove_if(m_effects.begin(), m_effects.end(),
        [id](const EffectPtr& e) { return e && e->id() == id; });
    m_effects.erase(it, m_effects.end());
}

void EffectChain::remove_at(std::size_t index) {
    if (index < m_effects.size()) {
        m_effects.erase(m_effects.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void EffectChain::clear() {
    m_effects.clear();
}

IAudioEffect* EffectChain::get(EffectId id) {
    for (auto& effect : m_effects) {
        if (effect && effect->id() == id) {
            return effect.get();
        }
    }
    return nullptr;
}

const IAudioEffect* EffectChain::get(EffectId id) const {
    for (const auto& effect : m_effects) {
        if (effect && effect->id() == id) {
            return effect.get();
        }
    }
    return nullptr;
}

IAudioEffect* EffectChain::at(std::size_t index) {
    return index < m_effects.size() ? m_effects[index].get() : nullptr;
}

const IAudioEffect* EffectChain::at(std::size_t index) const {
    return index < m_effects.size() ? m_effects[index].get() : nullptr;
}

void EffectChain::process(float* samples, std::size_t sample_count, std::uint32_t channels) {
    for (auto& effect : m_effects) {
        if (effect && effect->is_enabled()) {
            effect->process(samples, sample_count, channels);
        }
    }
}

void EffectChain::reset() {
    for (auto& effect : m_effects) {
        if (effect) {
            effect->reset();
        }
    }
}

} // namespace void_audio
