/// @file types.hpp
/// @brief Core type definitions for void_audio

#pragma once

#include "fwd.hpp"

#include <void_engine/math/math.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace void_audio {

// =============================================================================
// Audio Backend
// =============================================================================

/// Supported audio backends
enum class AudioBackend : std::uint8_t {
    Null,       ///< Null backend (no audio)
    OpenAL,     ///< OpenAL Soft
    XAudio2,    ///< XAudio2 (Windows)
    CoreAudio,  ///< Core Audio (macOS/iOS)
    FMOD,       ///< FMOD Studio
    Wwise,      ///< Audiokinetic Wwise
    Custom      ///< Custom backend
};

/// Convert backend to string
const char* to_string(AudioBackend backend);

// =============================================================================
// Audio Format
// =============================================================================

/// Audio sample format
enum class AudioFormat : std::uint8_t {
    Unknown,
    Mono8,          ///< 8-bit mono
    Mono16,         ///< 16-bit mono
    MonoFloat,      ///< 32-bit float mono
    Stereo8,        ///< 8-bit stereo
    Stereo16,       ///< 16-bit stereo
    StereoFloat,    ///< 32-bit float stereo
    Surround51,     ///< 5.1 surround (6 channels)
    Surround71      ///< 7.1 surround (8 channels)
};

/// Get number of channels for format
std::uint32_t get_channel_count(AudioFormat format);

/// Get bytes per sample for format
std::uint32_t get_bytes_per_sample(AudioFormat format);

/// Convert format to string
const char* to_string(AudioFormat format);

// =============================================================================
// Audio State
// =============================================================================

/// Audio source playback state
enum class AudioState : std::uint8_t {
    Initial,    ///< Source has not been played yet
    Playing,    ///< Source is currently playing
    Paused,     ///< Source is paused
    Stopped     ///< Source has stopped
};

/// Convert state to string
const char* to_string(AudioState state);

// =============================================================================
// Spatialization
// =============================================================================

/// 3D audio spatialization mode
enum class SpatializationMode : std::uint8_t {
    None,       ///< No spatialization (2D audio)
    Positional, ///< Position-based spatialization
    HRTF        ///< Head-related transfer function (binaural)
};

/// Distance attenuation model
enum class AttenuationModel : std::uint8_t {
    None,               ///< No attenuation
    InverseDistance,    ///< 1/distance falloff
    InverseDistanceClamped,
    LinearDistance,     ///< Linear falloff
    LinearDistanceClamped,
    ExponentialDistance,
    ExponentialDistanceClamped,
    Custom              ///< Custom attenuation curve
};

/// Convert model to string
const char* to_string(AttenuationModel model);

// =============================================================================
// Effect Types
// =============================================================================

/// Audio effect types
enum class EffectType : std::uint8_t {
    None,
    Reverb,
    Delay,
    LowPassFilter,
    HighPassFilter,
    BandPassFilter,
    Compressor,
    Limiter,
    Distortion,
    Chorus,
    Flanger,
    Phaser,
    Equalizer,
    Pitch,
    Custom
};

/// Convert effect type to string
const char* to_string(EffectType type);

// =============================================================================
// Audio Buffer Description
// =============================================================================

/// Description for creating an audio buffer
struct AudioBufferDesc {
    AudioFormat format = AudioFormat::Mono16;
    std::uint32_t sample_rate = 44100;
    std::uint32_t sample_count = 0;
    const void* data = nullptr;
    std::size_t data_size = 0;
    std::string name;
    bool streaming = false;         ///< Use streaming for large files

    /// Calculate buffer duration in seconds
    [[nodiscard]] float duration() const {
        if (sample_rate == 0) return 0;
        return static_cast<float>(sample_count) / static_cast<float>(sample_rate);
    }
};

// =============================================================================
// Audio Source Configuration
// =============================================================================

/// Distance attenuation settings
struct AttenuationSettings {
    AttenuationModel model = AttenuationModel::InverseDistanceClamped;
    float reference_distance = 1.0f;    ///< Distance at which volume is 100%
    float max_distance = 100.0f;        ///< Distance at which volume reaches minimum
    float rolloff_factor = 1.0f;        ///< Attenuation rate
    float min_gain = 0.0f;              ///< Minimum volume at max distance
    float max_gain = 1.0f;              ///< Maximum volume

    /// Custom attenuation curve (normalized distance -> gain)
    std::function<float(float)> custom_curve;
};

/// Cone settings for directional audio
struct ConeSettings {
    float inner_angle = 360.0f;         ///< Full volume cone angle (degrees)
    float outer_angle = 360.0f;         ///< Outer cone angle (degrees)
    float outer_gain = 0.0f;            ///< Volume outside outer cone
};

/// Configuration for an audio source
struct AudioSourceConfig {
    BufferId buffer;
    BusId output_bus;

    // Volume and pan
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;               ///< -1 (left) to 1 (right)

    // Playback
    bool loop = false;
    bool play_on_create = false;
    float start_time = 0.0f;        ///< Start position in seconds

    // 3D audio
    SpatializationMode spatialization = SpatializationMode::None;
    void_math::Vec3 position = {0, 0, 0};
    void_math::Vec3 velocity = {0, 0, 0};
    void_math::Vec3 direction = {0, 0, -1};

    AttenuationSettings attenuation;
    ConeSettings cone;

    // Priority (lower = higher priority, 0 = highest)
    std::uint8_t priority = 128;

    std::string name;
    void* user_data = nullptr;
};

// =============================================================================
// Listener Configuration
// =============================================================================

/// Configuration for the audio listener
struct ListenerConfig {
    void_math::Vec3 position = {0, 0, 0};
    void_math::Vec3 velocity = {0, 0, 0};
    void_math::Vec3 forward = {0, 0, -1};
    void_math::Vec3 up = {0, 1, 0};

    float master_volume = 1.0f;
    float doppler_factor = 1.0f;
    float speed_of_sound = 343.3f;  ///< In meters/second
};

// =============================================================================
// Bus Configuration
// =============================================================================

/// Configuration for an audio bus
struct BusConfig {
    std::string name;
    BusId parent;                   ///< Parent bus (0 = master)

    float volume = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;

    std::vector<EffectId> effects;  ///< Effect chain

    void* user_data = nullptr;
};

// =============================================================================
// Effect Configurations
// =============================================================================

/// Base effect configuration
struct EffectConfig {
    EffectType type = EffectType::None;
    bool enabled = true;
    float mix = 1.0f;               ///< Wet/dry mix (0 = dry, 1 = wet)
};

/// Reverb effect configuration
struct ReverbConfig : EffectConfig {
    ReverbConfig() { type = EffectType::Reverb; }

    float room_size = 0.5f;         ///< 0 to 1
    float damping = 0.5f;           ///< High frequency damping 0 to 1
    float decay_time = 1.5f;        ///< Decay time in seconds
    float pre_delay = 0.02f;        ///< Pre-delay in seconds
    float early_reflections = 0.5f;
    float late_reflections = 0.5f;
    float diffusion = 0.5f;
    float density = 0.5f;
    float hf_reference = 5000.0f;   ///< High frequency reference
    float lf_reference = 250.0f;    ///< Low frequency reference

    /// Presets
    static ReverbConfig small_room();
    static ReverbConfig medium_room();
    static ReverbConfig large_hall();
    static ReverbConfig cathedral();
    static ReverbConfig outdoor();
    static ReverbConfig underwater();
};

/// Delay effect configuration
struct DelayConfig : EffectConfig {
    DelayConfig() { type = EffectType::Delay; }

    float delay_time = 0.5f;        ///< Delay time in seconds
    float feedback = 0.3f;          ///< Feedback amount 0 to 1
    bool ping_pong = false;         ///< Ping-pong stereo delay
    float stereo_spread = 0.5f;     ///< Stereo spread for ping-pong
    bool tempo_sync = false;        ///< Sync to tempo
    float tempo_division = 0.25f;   ///< Division of tempo (0.25 = quarter note)
};

/// Filter effect configuration
struct FilterConfig : EffectConfig {
    FilterConfig(EffectType filter_type = EffectType::LowPassFilter) {
        type = filter_type;
    }

    float cutoff = 1000.0f;         ///< Cutoff frequency in Hz
    float resonance = 0.707f;       ///< Q factor
    float gain = 0.0f;              ///< Gain in dB (for shelf filters)
};

/// Compressor effect configuration
struct CompressorConfig : EffectConfig {
    CompressorConfig() { type = EffectType::Compressor; }

    float threshold = -10.0f;       ///< Threshold in dB
    float ratio = 4.0f;             ///< Compression ratio (4:1)
    float attack = 0.01f;           ///< Attack time in seconds
    float release = 0.1f;           ///< Release time in seconds
    float knee = 0.0f;              ///< Soft knee in dB
    float makeup_gain = 0.0f;       ///< Makeup gain in dB
    bool auto_makeup = false;       ///< Automatic makeup gain
};

/// Distortion effect configuration
struct DistortionConfig : EffectConfig {
    DistortionConfig() { type = EffectType::Distortion; }

    float drive = 0.5f;             ///< Drive amount 0 to 1
    float tone = 0.5f;              ///< Tone control 0 to 1
    float output = 1.0f;            ///< Output level 0 to 1

    enum class Mode {
        SoftClip,
        HardClip,
        Tube,
        Fuzz,
        Bitcrush
    } mode = Mode::SoftClip;

    // Bitcrush settings
    std::uint8_t bit_depth = 8;     ///< Bit depth for bitcrush
    float sample_rate_reduction = 1.0f;
};

/// Chorus effect configuration
struct ChorusConfig : EffectConfig {
    ChorusConfig() { type = EffectType::Chorus; }

    float rate = 1.0f;              ///< LFO rate in Hz
    float depth = 0.5f;             ///< Modulation depth 0 to 1
    float delay = 0.02f;            ///< Base delay in seconds
    float feedback = 0.0f;          ///< Feedback amount
    std::uint8_t voices = 2;        ///< Number of voices
    float stereo_width = 1.0f;      ///< Stereo spread
};

/// Equalizer band configuration
struct EQBand {
    enum class Type {
        LowShelf,
        HighShelf,
        Peak,
        LowPass,
        HighPass,
        Notch
    } type = Type::Peak;

    float frequency = 1000.0f;      ///< Center frequency
    float gain = 0.0f;              ///< Gain in dB
    float q = 1.0f;                 ///< Q factor / bandwidth
    bool enabled = true;
};

/// Equalizer effect configuration
struct EQConfig : EffectConfig {
    EQConfig() { type = EffectType::Equalizer; }

    std::vector<EQBand> bands;

    /// Create 3-band EQ
    static EQConfig three_band(float low_gain, float mid_gain, float high_gain);
    /// Create 5-band EQ
    static EQConfig five_band();
    /// Create 10-band graphic EQ
    static EQConfig graphic_10band();
};

// =============================================================================
// Audio System Configuration
// =============================================================================

/// Configuration for the audio system
struct AudioConfig {
    AudioBackend backend = AudioBackend::OpenAL;

    std::uint32_t sample_rate = 44100;
    std::uint32_t buffer_size = 1024;       ///< Samples per buffer
    std::uint32_t buffer_count = 4;         ///< Number of buffers for streaming
    std::uint32_t max_sources = 64;         ///< Maximum concurrent sources
    std::uint32_t max_buffers = 256;        ///< Maximum loaded buffers

    // 3D audio
    float doppler_factor = 1.0f;
    float speed_of_sound = 343.3f;
    float distance_model = 1.0f;

    // Virtual voices (for voice limiting)
    std::uint32_t max_virtual_voices = 128;
    bool enable_voice_stealing = true;

    // Threading
    bool enable_async_loading = true;
    std::uint32_t audio_thread_count = 1;

    /// Default configuration
    static AudioConfig defaults();

    /// Low latency configuration
    static AudioConfig low_latency();

    /// High quality configuration
    static AudioConfig high_quality();
};

// =============================================================================
// Audio Statistics
// =============================================================================

/// Audio system statistics
struct AudioStats {
    std::uint32_t active_sources = 0;
    std::uint32_t virtual_sources = 0;
    std::uint32_t loaded_buffers = 0;
    std::uint32_t streaming_buffers = 0;

    std::uint64_t total_samples_played = 0;
    std::uint64_t total_bytes_streamed = 0;

    float cpu_usage = 0.0f;             ///< Audio thread CPU usage
    float buffer_underruns = 0;         ///< Number of underruns
    float latency_ms = 0.0f;            ///< Current latency

    // Per-frame stats
    std::uint32_t sources_started = 0;
    std::uint32_t sources_stopped = 0;
    std::uint32_t voices_stolen = 0;
};

// =============================================================================
// Music Playback
// =============================================================================

/// Music track transition types
enum class MusicTransition : std::uint8_t {
    Immediate,          ///< Instant switch
    Crossfade,          ///< Crossfade between tracks
    FadeOutFadeIn,      ///< Fade out current, then fade in new
    BeatSync            ///< Switch on next beat
};

/// Music playback configuration
struct MusicConfig {
    SourceId source;
    float fade_time = 1.0f;             ///< Default fade duration
    MusicTransition transition = MusicTransition::Crossfade;

    bool loop = true;
    float intro_length = 0.0f;          ///< Non-looping intro section
    float loop_start = 0.0f;            ///< Loop start point
    float loop_end = 0.0f;              ///< Loop end point (0 = end of track)

    // Beat sync
    float bpm = 120.0f;
    std::uint32_t beats_per_bar = 4;
    std::uint32_t bars_to_wait = 1;     ///< Bars to wait before transition
};

// =============================================================================
// Audio Events
// =============================================================================

/// Event fired when a source finishes playing
struct SourceEndedEvent {
    SourceId source_id;
    bool naturally_ended;               ///< True if played to end, false if stopped
};

/// Event fired when a source loops
struct SourceLoopedEvent {
    SourceId source_id;
    std::uint32_t loop_count;
};

/// Event fired when music transitions
struct MusicTransitionEvent {
    SourceId from_source;
    SourceId to_source;
    MusicTransition transition_type;
};

} // namespace void_audio
