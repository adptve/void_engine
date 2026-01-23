#pragma once

/// @file audio_loader.hpp
/// @brief Audio asset loader for WAV, OGG, MP3, and FLAC formats

#include <void_engine/asset/loader.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace void_asset {

// =============================================================================
// Audio Asset Types
// =============================================================================

/// Audio sample format
enum class AudioFormat : std::uint8_t {
    PCM_U8,      // Unsigned 8-bit
    PCM_S16,     // Signed 16-bit (most common)
    PCM_S24,     // Signed 24-bit
    PCM_S32,     // Signed 32-bit
    PCM_F32,     // 32-bit float
    PCM_F64,     // 64-bit float
};

/// Get bytes per sample for format
[[nodiscard]] inline std::uint32_t bytes_per_sample(AudioFormat format) {
    switch (format) {
        case AudioFormat::PCM_U8: return 1;
        case AudioFormat::PCM_S16: return 2;
        case AudioFormat::PCM_S24: return 3;
        case AudioFormat::PCM_S32: return 4;
        case AudioFormat::PCM_F32: return 4;
        case AudioFormat::PCM_F64: return 8;
    }
    return 2;
}

/// Audio asset
struct AudioAsset {
    std::string name;
    std::vector<std::uint8_t> data;  // Raw PCM samples
    std::uint32_t sample_rate = 44100;
    std::uint32_t channels = 2;
    AudioFormat format = AudioFormat::PCM_S16;
    std::uint64_t frame_count = 0;

    // Metadata
    std::string title;
    std::string artist;
    std::string album;

    /// Get duration in seconds
    [[nodiscard]] double duration() const {
        if (sample_rate == 0) return 0.0;
        return static_cast<double>(frame_count) / static_cast<double>(sample_rate);
    }

    /// Get data size in bytes
    [[nodiscard]] std::size_t data_size() const {
        return frame_count * channels * bytes_per_sample(format);
    }

    /// Get bytes per frame
    [[nodiscard]] std::uint32_t bytes_per_frame() const {
        return channels * bytes_per_sample(format);
    }

    /// Check if stereo
    [[nodiscard]] bool is_stereo() const { return channels == 2; }

    /// Check if mono
    [[nodiscard]] bool is_mono() const { return channels == 1; }
};

/// Streaming audio asset (for large files)
struct StreamingAudioAsset {
    std::string name;
    std::string source_path;
    std::uint32_t sample_rate = 44100;
    std::uint32_t channels = 2;
    AudioFormat format = AudioFormat::PCM_S16;
    std::uint64_t frame_count = 0;

    // For streaming, we don't store all data
    // Instead, we provide methods to read chunks

    /// Get duration in seconds
    [[nodiscard]] double duration() const {
        if (sample_rate == 0) return 0.0;
        return static_cast<double>(frame_count) / static_cast<double>(sample_rate);
    }
};

// =============================================================================
// Audio Loader
// =============================================================================

/// Configuration for audio loading
struct AudioLoadConfig {
    bool convert_to_stereo = false;
    bool convert_to_mono = false;
    std::uint32_t resample_rate = 0;  // 0 = keep original
    AudioFormat target_format = AudioFormat::PCM_S16;
    bool normalize = false;
    float normalize_peak = 0.95f;
};

/// Loads audio assets
class AudioLoader : public AssetLoader<AudioAsset> {
public:
    AudioLoader() = default;
    explicit AudioLoader(AudioLoadConfig config) : m_config(std::move(config)) {}

    [[nodiscard]] std::vector<std::string> extensions() const override {
        return {"wav", "wave", "ogg", "mp3", "flac", "aiff", "aif"};
    }

    [[nodiscard]] LoadResult<AudioAsset> load(LoadContext& ctx) override;

    [[nodiscard]] std::string type_name() const override {
        return "AudioAsset";
    }

    /// Set load config
    void set_config(AudioLoadConfig config) { m_config = std::move(config); }

private:
    LoadResult<AudioAsset> load_wav(LoadContext& ctx);
    LoadResult<AudioAsset> load_ogg(LoadContext& ctx);
    LoadResult<AudioAsset> load_mp3(LoadContext& ctx);
    LoadResult<AudioAsset> load_flac(LoadContext& ctx);
    LoadResult<AudioAsset> load_aiff(LoadContext& ctx);

    void apply_config(AudioAsset& audio);
    void convert_format(AudioAsset& audio, AudioFormat target);
    void convert_channels(AudioAsset& audio, std::uint32_t target_channels);
    void resample(AudioAsset& audio, std::uint32_t target_rate);
    void normalize(AudioAsset& audio, float peak);

    AudioLoadConfig m_config;
};

// =============================================================================
// Streaming Audio Loader
// =============================================================================

/// Loads audio for streaming (doesn't decode entire file)
class StreamingAudioLoader : public AssetLoader<StreamingAudioAsset> {
public:
    [[nodiscard]] std::vector<std::string> extensions() const override {
        return {"wav", "ogg", "mp3", "flac"};
    }

    [[nodiscard]] LoadResult<StreamingAudioAsset> load(LoadContext& ctx) override;

    [[nodiscard]] std::string type_name() const override {
        return "StreamingAudioAsset";
    }
};

// =============================================================================
// WAV Parser
// =============================================================================

/// Parses WAV file format
class WavParser {
public:
    struct WavHeader {
        std::uint16_t audio_format = 1;  // 1 = PCM, 3 = Float
        std::uint16_t channels = 2;
        std::uint32_t sample_rate = 44100;
        std::uint32_t byte_rate = 0;
        std::uint16_t block_align = 0;
        std::uint16_t bits_per_sample = 16;
        std::uint32_t data_size = 0;
        std::size_t data_offset = 0;
    };

    /// Parse WAV header
    static std::optional<WavHeader> parse_header(const std::vector<std::uint8_t>& data);

    /// Get audio format from header
    static AudioFormat get_format(const WavHeader& header);
};

} // namespace void_asset
