/// @file types.cpp
/// @brief Core type implementations for void_audio

#include <void_engine/audio/types.hpp>

namespace void_audio {

// =============================================================================
// String Conversions
// =============================================================================

const char* to_string(AudioBackend backend) {
    switch (backend) {
        case AudioBackend::Null: return "Null";
        case AudioBackend::OpenAL: return "OpenAL";
        case AudioBackend::XAudio2: return "XAudio2";
        case AudioBackend::CoreAudio: return "CoreAudio";
        case AudioBackend::FMOD: return "FMOD";
        case AudioBackend::Wwise: return "Wwise";
        case AudioBackend::Custom: return "Custom";
    }
    return "Unknown";
}

std::uint32_t get_channel_count(AudioFormat format) {
    switch (format) {
        case AudioFormat::Unknown: return 0;
        case AudioFormat::Mono8:
        case AudioFormat::Mono16:
        case AudioFormat::MonoFloat: return 1;
        case AudioFormat::Stereo8:
        case AudioFormat::Stereo16:
        case AudioFormat::StereoFloat: return 2;
        case AudioFormat::Surround51: return 6;
        case AudioFormat::Surround71: return 8;
    }
    return 0;
}

std::uint32_t get_bytes_per_sample(AudioFormat format) {
    switch (format) {
        case AudioFormat::Unknown: return 0;
        case AudioFormat::Mono8: return 1;
        case AudioFormat::Mono16: return 2;
        case AudioFormat::MonoFloat: return 4;
        case AudioFormat::Stereo8: return 2;
        case AudioFormat::Stereo16: return 4;
        case AudioFormat::StereoFloat: return 8;
        case AudioFormat::Surround51: return 12; // 6 * 16-bit
        case AudioFormat::Surround71: return 16; // 8 * 16-bit
    }
    return 0;
}

const char* to_string(AudioFormat format) {
    switch (format) {
        case AudioFormat::Unknown: return "Unknown";
        case AudioFormat::Mono8: return "Mono8";
        case AudioFormat::Mono16: return "Mono16";
        case AudioFormat::MonoFloat: return "MonoFloat";
        case AudioFormat::Stereo8: return "Stereo8";
        case AudioFormat::Stereo16: return "Stereo16";
        case AudioFormat::StereoFloat: return "StereoFloat";
        case AudioFormat::Surround51: return "Surround51";
        case AudioFormat::Surround71: return "Surround71";
    }
    return "Unknown";
}

const char* to_string(AudioState state) {
    switch (state) {
        case AudioState::Initial: return "Initial";
        case AudioState::Playing: return "Playing";
        case AudioState::Paused: return "Paused";
        case AudioState::Stopped: return "Stopped";
    }
    return "Unknown";
}

const char* to_string(AttenuationModel model) {
    switch (model) {
        case AttenuationModel::None: return "None";
        case AttenuationModel::InverseDistance: return "InverseDistance";
        case AttenuationModel::InverseDistanceClamped: return "InverseDistanceClamped";
        case AttenuationModel::LinearDistance: return "LinearDistance";
        case AttenuationModel::LinearDistanceClamped: return "LinearDistanceClamped";
        case AttenuationModel::ExponentialDistance: return "ExponentialDistance";
        case AttenuationModel::ExponentialDistanceClamped: return "ExponentialDistanceClamped";
        case AttenuationModel::Custom: return "Custom";
    }
    return "Unknown";
}

const char* to_string(EffectType type) {
    switch (type) {
        case EffectType::None: return "None";
        case EffectType::Reverb: return "Reverb";
        case EffectType::Delay: return "Delay";
        case EffectType::LowPassFilter: return "LowPassFilter";
        case EffectType::HighPassFilter: return "HighPassFilter";
        case EffectType::BandPassFilter: return "BandPassFilter";
        case EffectType::Compressor: return "Compressor";
        case EffectType::Limiter: return "Limiter";
        case EffectType::Distortion: return "Distortion";
        case EffectType::Chorus: return "Chorus";
        case EffectType::Flanger: return "Flanger";
        case EffectType::Phaser: return "Phaser";
        case EffectType::Equalizer: return "Equalizer";
        case EffectType::Pitch: return "Pitch";
        case EffectType::Custom: return "Custom";
    }
    return "Unknown";
}

// =============================================================================
// Reverb Presets
// =============================================================================

ReverbConfig ReverbConfig::small_room() {
    ReverbConfig config;
    config.room_size = 0.2f;
    config.damping = 0.7f;
    config.decay_time = 0.4f;
    config.pre_delay = 0.01f;
    config.early_reflections = 0.7f;
    config.late_reflections = 0.3f;
    config.diffusion = 0.8f;
    config.density = 0.9f;
    config.mix = 0.3f;
    return config;
}

ReverbConfig ReverbConfig::medium_room() {
    ReverbConfig config;
    config.room_size = 0.5f;
    config.damping = 0.5f;
    config.decay_time = 1.0f;
    config.pre_delay = 0.02f;
    config.early_reflections = 0.5f;
    config.late_reflections = 0.5f;
    config.diffusion = 0.7f;
    config.density = 0.7f;
    config.mix = 0.4f;
    return config;
}

ReverbConfig ReverbConfig::large_hall() {
    ReverbConfig config;
    config.room_size = 0.8f;
    config.damping = 0.3f;
    config.decay_time = 2.5f;
    config.pre_delay = 0.04f;
    config.early_reflections = 0.3f;
    config.late_reflections = 0.7f;
    config.diffusion = 0.5f;
    config.density = 0.5f;
    config.mix = 0.5f;
    return config;
}

ReverbConfig ReverbConfig::cathedral() {
    ReverbConfig config;
    config.room_size = 1.0f;
    config.damping = 0.2f;
    config.decay_time = 4.0f;
    config.pre_delay = 0.06f;
    config.early_reflections = 0.2f;
    config.late_reflections = 0.8f;
    config.diffusion = 0.4f;
    config.density = 0.3f;
    config.mix = 0.6f;
    return config;
}

ReverbConfig ReverbConfig::outdoor() {
    ReverbConfig config;
    config.room_size = 0.6f;
    config.damping = 0.8f;
    config.decay_time = 0.8f;
    config.pre_delay = 0.005f;
    config.early_reflections = 0.8f;
    config.late_reflections = 0.2f;
    config.diffusion = 0.9f;
    config.density = 0.2f;
    config.mix = 0.2f;
    return config;
}

ReverbConfig ReverbConfig::underwater() {
    ReverbConfig config;
    config.room_size = 0.9f;
    config.damping = 0.1f;
    config.decay_time = 3.0f;
    config.pre_delay = 0.03f;
    config.early_reflections = 0.4f;
    config.late_reflections = 0.6f;
    config.diffusion = 0.3f;
    config.density = 0.8f;
    config.hf_reference = 2000.0f;
    config.mix = 0.7f;
    return config;
}

// =============================================================================
// EQ Presets
// =============================================================================

EQConfig EQConfig::three_band(float low_gain, float mid_gain, float high_gain) {
    EQConfig config;
    config.bands = {
        {EQBand::Type::LowShelf, 200.0f, low_gain, 0.7f, true},
        {EQBand::Type::Peak, 1000.0f, mid_gain, 1.0f, true},
        {EQBand::Type::HighShelf, 4000.0f, high_gain, 0.7f, true}
    };
    return config;
}

EQConfig EQConfig::five_band() {
    EQConfig config;
    config.bands = {
        {EQBand::Type::LowShelf, 80.0f, 0.0f, 0.7f, true},
        {EQBand::Type::Peak, 250.0f, 0.0f, 1.0f, true},
        {EQBand::Type::Peak, 1000.0f, 0.0f, 1.0f, true},
        {EQBand::Type::Peak, 4000.0f, 0.0f, 1.0f, true},
        {EQBand::Type::HighShelf, 12000.0f, 0.0f, 0.7f, true}
    };
    return config;
}

EQConfig EQConfig::graphic_10band() {
    EQConfig config;
    float frequencies[] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    for (float freq : frequencies) {
        config.bands.push_back({EQBand::Type::Peak, freq, 0.0f, 2.0f, true});
    }
    return config;
}

// =============================================================================
// AudioConfig Presets
// =============================================================================

AudioConfig AudioConfig::defaults() {
    return AudioConfig{};
}

AudioConfig AudioConfig::low_latency() {
    AudioConfig config;
    config.buffer_size = 256;
    config.buffer_count = 2;
    config.enable_async_loading = false;
    return config;
}

AudioConfig AudioConfig::high_quality() {
    AudioConfig config;
    config.sample_rate = 48000;
    config.buffer_size = 2048;
    config.buffer_count = 4;
    config.max_sources = 128;
    config.max_virtual_voices = 256;
    return config;
}

} // namespace void_audio
