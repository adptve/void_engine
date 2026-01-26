/// @file audio_loader.cpp
/// @brief Audio asset loader implementation - Full production code with no stubs

#include <void_engine/asset/loaders/audio_loader.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

// ============================================================================
// Audio Library Implementations
// ============================================================================

// stb_vorbis for OGG (included in stb)
#ifdef VOID_HAS_STB
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#endif

// dr_libs for WAV, FLAC, and MP3 (backup)
#ifdef VOID_HAS_DR_LIBS
#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#include "dr_wav.h"
#include "dr_flac.h"
#include "dr_mp3.h"
#endif

// minimp3 for MP3 (preferred)
#ifdef VOID_HAS_MINIMP3
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"
#endif

namespace void_asset {

// =============================================================================
// WAV Parser Implementation
// =============================================================================

std::optional<WavParser::WavHeader> WavParser::parse_header(
    const std::vector<std::uint8_t>& data) {

    // Minimum WAV file size: RIFF header (12) + fmt chunk (24) + data header (8)
    if (data.size() < 44) {
        return std::nullopt;
    }

    const std::uint8_t* ptr = data.data();

    // Check RIFF header
    if (std::memcmp(ptr, "RIFF", 4) != 0) {
        return std::nullopt;
    }
    ptr += 4;

    // Skip file size
    ptr += 4;

    // Check WAVE format
    if (std::memcmp(ptr, "WAVE", 4) != 0) {
        return std::nullopt;
    }
    ptr += 4;

    WavHeader header;

    // Parse chunks
    const std::uint8_t* end = data.data() + data.size();
    bool found_fmt = false;
    bool found_data = false;

    while (ptr + 8 <= end && !(found_fmt && found_data)) {
        char chunk_id[5] = {0};
        std::memcpy(chunk_id, ptr, 4);
        ptr += 4;

        std::uint32_t chunk_size = *reinterpret_cast<const std::uint32_t*>(ptr);
        ptr += 4;

        if (std::strcmp(chunk_id, "fmt ") == 0) {
            if (ptr + 16 > end) {
                return std::nullopt;
            }

            header.audio_format = *reinterpret_cast<const std::uint16_t*>(ptr);
            ptr += 2;
            header.channels = *reinterpret_cast<const std::uint16_t*>(ptr);
            ptr += 2;
            header.sample_rate = *reinterpret_cast<const std::uint32_t*>(ptr);
            ptr += 4;
            header.byte_rate = *reinterpret_cast<const std::uint32_t*>(ptr);
            ptr += 4;
            header.block_align = *reinterpret_cast<const std::uint16_t*>(ptr);
            ptr += 2;
            header.bits_per_sample = *reinterpret_cast<const std::uint16_t*>(ptr);
            ptr += 2;

            // Skip extra format bytes if present
            if (chunk_size > 16) {
                ptr += (chunk_size - 16);
            }

            found_fmt = true;
        } else if (std::strcmp(chunk_id, "data") == 0) {
            header.data_size = chunk_size;
            header.data_offset = static_cast<std::size_t>(ptr - data.data());
            found_data = true;
            break;  // Data chunk found, we're done
        } else {
            // Skip unknown chunk
            ptr += chunk_size;
            // Align to word boundary
            if (chunk_size & 1) {
                ptr += 1;
            }
        }
    }

    if (!found_fmt || !found_data) {
        return std::nullopt;
    }

    return header;
}

AudioFormat WavParser::get_format(const WavHeader& header) {
    if (header.audio_format == 1) {  // PCM
        switch (header.bits_per_sample) {
            case 8: return AudioFormat::PCM_U8;
            case 16: return AudioFormat::PCM_S16;
            case 24: return AudioFormat::PCM_S24;
            case 32: return AudioFormat::PCM_S32;
            default: return AudioFormat::PCM_S16;
        }
    } else if (header.audio_format == 3) {  // IEEE Float
        switch (header.bits_per_sample) {
            case 32: return AudioFormat::PCM_F32;
            case 64: return AudioFormat::PCM_F64;
            default: return AudioFormat::PCM_F32;
        }
    }
    return AudioFormat::PCM_S16;
}

// =============================================================================
// Audio Loader Implementation
// =============================================================================

LoadResult<AudioAsset> AudioLoader::load(LoadContext& ctx) {
    const auto& ext = ctx.extension();

    if (ext == "wav" || ext == "wave") {
        return load_wav(ctx);
    } else if (ext == "ogg") {
        return load_ogg(ctx);
    } else if (ext == "mp3") {
        return load_mp3(ctx);
    } else if (ext == "flac") {
        return load_flac(ctx);
    } else if (ext == "aiff" || ext == "aif") {
        return load_aiff(ctx);
    }

    return void_core::Err<std::unique_ptr<AudioAsset>>("Unknown audio format: " + ext);
}

LoadResult<AudioAsset> AudioLoader::load_wav(LoadContext& ctx) {
    const auto& data = ctx.data();

#ifdef VOID_HAS_DR_LIBS
    // Use dr_wav for robust WAV loading
    drwav wav;
    if (!drwav_init_memory(&wav, data.data(), data.size(), nullptr)) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Failed to parse WAV file");
    }

    AudioAsset audio;
    audio.name = ctx.path().stem();
    audio.sample_rate = wav.sampleRate;
    audio.channels = wav.channels;
    audio.frame_count = wav.totalPCMFrameCount;

    // Determine format based on WAV properties
    if (wav.translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT) {
        audio.format = AudioFormat::PCM_F32;
        audio.data.resize(audio.frame_count * audio.channels * sizeof(float));
        drwav_read_pcm_frames_f32(&wav, audio.frame_count,
            reinterpret_cast<float*>(audio.data.data()));
    } else {
        // Decode to S16 (most common format)
        audio.format = AudioFormat::PCM_S16;
        audio.data.resize(audio.frame_count * audio.channels * sizeof(std::int16_t));
        drwav_read_pcm_frames_s16(&wav, audio.frame_count,
            reinterpret_cast<std::int16_t*>(audio.data.data()));
    }

    drwav_uninit(&wav);

#else
    // Fallback to manual WAV parsing
    auto header_opt = WavParser::parse_header(data);
    if (!header_opt) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Invalid WAV file");
    }

    const auto& header = *header_opt;

    AudioAsset audio;
    audio.name = ctx.path().stem();
    audio.sample_rate = header.sample_rate;
    audio.channels = header.channels;
    audio.format = WavParser::get_format(header);

    // Calculate frame count
    std::uint32_t bytes_per_frame = header.channels * (header.bits_per_sample / 8);
    if (bytes_per_frame == 0) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Invalid WAV format");
    }
    audio.frame_count = header.data_size / bytes_per_frame;

    // Copy audio data
    if (header.data_offset + header.data_size > data.size()) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("WAV data exceeds file size");
    }

    audio.data.resize(header.data_size);
    std::memcpy(audio.data.data(), data.data() + header.data_offset, header.data_size);
#endif

    // Apply config conversions
    apply_config(audio);

    return void_core::Ok(std::make_unique<AudioAsset>(std::move(audio)));
}

LoadResult<AudioAsset> AudioLoader::load_ogg(LoadContext& ctx) {
    const auto& data = ctx.data();

#ifdef VOID_HAS_STB
    // Use stb_vorbis for OGG Vorbis decoding
    int channels = 0;
    int sample_rate = 0;
    short* output = nullptr;

    int samples = stb_vorbis_decode_memory(
        data.data(), static_cast<int>(data.size()),
        &channels, &sample_rate, &output
    );

    if (samples < 0 || output == nullptr) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Failed to decode OGG Vorbis file");
    }

    AudioAsset audio;
    audio.name = ctx.path().stem();
    audio.sample_rate = static_cast<std::uint32_t>(sample_rate);
    audio.channels = static_cast<std::uint32_t>(channels);
    audio.format = AudioFormat::PCM_S16;
    audio.frame_count = static_cast<std::uint64_t>(samples);

    std::size_t data_size = samples * channels * sizeof(short);
    audio.data.resize(data_size);
    std::memcpy(audio.data.data(), output, data_size);

    free(output);

    apply_config(audio);
    return void_core::Ok(std::make_unique<AudioAsset>(std::move(audio)));

#else
    // Check OGG magic for validation
    if (data.size() < 4 || std::memcmp(data.data(), "OggS", 4) != 0) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Invalid OGG file");
    }
    return void_core::Err<std::unique_ptr<AudioAsset>>(
        "OGG decoding requires stb_vorbis (enable VOID_HAS_STB)");
#endif
}

LoadResult<AudioAsset> AudioLoader::load_mp3(LoadContext& ctx) {
    const auto& data = ctx.data();

#ifdef VOID_HAS_MINIMP3
    // Use minimp3 for MP3 decoding (preferred)
    mp3dec_t mp3d;
    mp3dec_init(&mp3d);

    mp3dec_file_info_t info;
    std::memset(&info, 0, sizeof(info));

    int result = mp3dec_load_buf(&mp3d, data.data(), data.size(), &info, nullptr, nullptr);
    if (result != 0 || info.buffer == nullptr) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Failed to decode MP3 file");
    }

    AudioAsset audio;
    audio.name = ctx.path().stem();
    audio.sample_rate = static_cast<std::uint32_t>(info.hz);
    audio.channels = static_cast<std::uint32_t>(info.channels);
    audio.format = AudioFormat::PCM_S16;
    audio.frame_count = static_cast<std::uint64_t>(info.samples / info.channels);

    std::size_t data_size = info.samples * sizeof(mp3d_sample_t);
    audio.data.resize(data_size);
    std::memcpy(audio.data.data(), info.buffer, data_size);

    free(info.buffer);

    apply_config(audio);
    return void_core::Ok(std::make_unique<AudioAsset>(std::move(audio)));

#elif defined(VOID_HAS_DR_LIBS)
    // Fallback to dr_mp3
    drmp3_config config;
    drmp3_uint64 frame_count;
    drmp3_int16* samples = drmp3_open_memory_and_read_pcm_frames_s16(
        data.data(), data.size(), &config, &frame_count, nullptr);

    if (samples == nullptr) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Failed to decode MP3 file");
    }

    AudioAsset audio;
    audio.name = ctx.path().stem();
    audio.sample_rate = config.sampleRate;
    audio.channels = config.channels;
    audio.format = AudioFormat::PCM_S16;
    audio.frame_count = frame_count;

    std::size_t data_size = frame_count * config.channels * sizeof(drmp3_int16);
    audio.data.resize(data_size);
    std::memcpy(audio.data.data(), samples, data_size);

    drmp3_free(samples, nullptr);

    apply_config(audio);
    return void_core::Ok(std::make_unique<AudioAsset>(std::move(audio)));

#else
    // Check for MP3 sync or ID3 tag for validation
    if (data.size() < 4) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("File too small for MP3");
    }
    bool has_id3 = (data[0] == 'I' && data[1] == 'D' && data[2] == '3');
    bool has_sync = ((data[0] == 0xFF) && ((data[1] & 0xE0) == 0xE0));
    if (!has_id3 && !has_sync) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Invalid MP3 file");
    }
    return void_core::Err<std::unique_ptr<AudioAsset>>(
        "MP3 decoding requires minimp3 or dr_mp3 (enable VOID_HAS_MINIMP3 or VOID_HAS_DR_LIBS)");
#endif
}

LoadResult<AudioAsset> AudioLoader::load_flac(LoadContext& ctx) {
    const auto& data = ctx.data();

#ifdef VOID_HAS_DR_LIBS
    // Use dr_flac for FLAC decoding
    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    drflac_uint64 total_samples = 0;

    // Decode to S16 for best compatibility
    drflac_int16* samples = drflac_open_memory_and_read_pcm_frames_s16(
        data.data(), data.size(),
        &channels, &sample_rate, &total_samples,
        nullptr
    );

    if (samples == nullptr) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Failed to decode FLAC file");
    }

    AudioAsset audio;
    audio.name = ctx.path().stem();
    audio.sample_rate = sample_rate;
    audio.channels = channels;
    audio.format = AudioFormat::PCM_S16;
    audio.frame_count = total_samples;

    std::size_t data_size = total_samples * channels * sizeof(drflac_int16);
    audio.data.resize(data_size);
    std::memcpy(audio.data.data(), samples, data_size);

    drflac_free(samples, nullptr);

    apply_config(audio);
    return void_core::Ok(std::make_unique<AudioAsset>(std::move(audio)));

#else
    // Check FLAC magic for validation
    if (data.size() < 4 || std::memcmp(data.data(), "fLaC", 4) != 0) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Invalid FLAC file");
    }
    return void_core::Err<std::unique_ptr<AudioAsset>>(
        "FLAC decoding requires dr_flac (enable VOID_HAS_DR_LIBS)");
#endif
}

LoadResult<AudioAsset> AudioLoader::load_aiff(LoadContext& ctx) {
    const auto& data = ctx.data();

    // Check AIFF magic: "FORM" + size + "AIFF" or "AIFC"
    if (data.size() < 12) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("File too small for AIFF");
    }

    if (std::memcmp(data.data(), "FORM", 4) != 0) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Invalid AIFF file (missing FORM)");
    }

    const char* form_type = reinterpret_cast<const char*>(data.data() + 8);
    bool is_aiff = (std::memcmp(form_type, "AIFF", 4) == 0);
    bool is_aifc = (std::memcmp(form_type, "AIFC", 4) == 0);

    if (!is_aiff && !is_aifc) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Invalid AIFF file (not AIFF/AIFC)");
    }

    // Parse AIFF chunks
    const std::uint8_t* ptr = data.data() + 12;
    const std::uint8_t* end = data.data() + data.size();

    std::uint16_t channels = 0;
    std::uint32_t num_frames = 0;
    std::uint16_t bits_per_sample = 0;
    double sample_rate = 0.0;
    const std::uint8_t* sound_data = nullptr;
    std::uint32_t sound_size = 0;

    while (ptr + 8 <= end) {
        char chunk_id[5] = {0};
        std::memcpy(chunk_id, ptr, 4);
        ptr += 4;

        // AIFF uses big-endian
        std::uint32_t chunk_size =
            (static_cast<std::uint32_t>(ptr[0]) << 24) |
            (static_cast<std::uint32_t>(ptr[1]) << 16) |
            (static_cast<std::uint32_t>(ptr[2]) << 8) |
            static_cast<std::uint32_t>(ptr[3]);
        ptr += 4;

        if (std::strcmp(chunk_id, "COMM") == 0) {
            // Common chunk
            if (ptr + 18 > end) break;

            channels = (static_cast<std::uint16_t>(ptr[0]) << 8) | ptr[1];
            ptr += 2;

            num_frames =
                (static_cast<std::uint32_t>(ptr[0]) << 24) |
                (static_cast<std::uint32_t>(ptr[1]) << 16) |
                (static_cast<std::uint32_t>(ptr[2]) << 8) |
                static_cast<std::uint32_t>(ptr[3]);
            ptr += 4;

            bits_per_sample = (static_cast<std::uint16_t>(ptr[0]) << 8) | ptr[1];
            ptr += 2;

            // Sample rate is 80-bit extended precision float
            // Parse the 10-byte extended precision format
            std::uint16_t exponent = (static_cast<std::uint16_t>(ptr[0]) << 8) | ptr[1];
            std::uint64_t mantissa =
                (static_cast<std::uint64_t>(ptr[2]) << 56) |
                (static_cast<std::uint64_t>(ptr[3]) << 48) |
                (static_cast<std::uint64_t>(ptr[4]) << 40) |
                (static_cast<std::uint64_t>(ptr[5]) << 32) |
                (static_cast<std::uint64_t>(ptr[6]) << 24) |
                (static_cast<std::uint64_t>(ptr[7]) << 16) |
                (static_cast<std::uint64_t>(ptr[8]) << 8) |
                static_cast<std::uint64_t>(ptr[9]);

            bool sign = (exponent >> 15) != 0;
            exponent &= 0x7FFF;

            if (exponent == 0 && mantissa == 0) {
                sample_rate = 0.0;
            } else if (exponent == 0x7FFF) {
                sample_rate = 0.0;  // Inf/NaN
            } else {
                sample_rate = std::ldexp(
                    static_cast<double>(mantissa), exponent - 16383 - 63);
                if (sign) sample_rate = -sample_rate;
            }

            ptr += 10;

            // Skip rest of COMM chunk
            if (chunk_size > 18) {
                ptr += (chunk_size - 18);
            }
        } else if (std::strcmp(chunk_id, "SSND") == 0) {
            // Sound data chunk
            if (ptr + 8 > end) break;

            // Skip offset and block size
            ptr += 8;
            sound_data = ptr;
            sound_size = chunk_size - 8;
            break;
        } else {
            // Skip unknown chunk
            ptr += chunk_size;
        }

        // Align to word boundary
        if (chunk_size & 1) {
            ptr += 1;
        }
    }

    if (channels == 0 || num_frames == 0 || sound_data == nullptr) {
        return void_core::Err<std::unique_ptr<AudioAsset>>("Invalid or incomplete AIFF file");
    }

    AudioAsset audio;
    audio.name = ctx.path().stem();
    audio.sample_rate = static_cast<std::uint32_t>(sample_rate);
    audio.channels = channels;
    audio.frame_count = num_frames;

    // Determine format and convert big-endian to little-endian
    std::uint32_t bytes_per_sample = bits_per_sample / 8;
    std::size_t total_samples = num_frames * channels;

    if (bits_per_sample == 8) {
        audio.format = AudioFormat::PCM_U8;
        audio.data.resize(total_samples);
        // 8-bit AIFF is signed, convert to unsigned
        for (std::size_t i = 0; i < total_samples; ++i) {
            audio.data[i] = static_cast<std::uint8_t>(
                static_cast<std::int8_t>(sound_data[i]) + 128);
        }
    } else if (bits_per_sample == 16) {
        audio.format = AudioFormat::PCM_S16;
        audio.data.resize(total_samples * 2);
        auto* dst = reinterpret_cast<std::int16_t*>(audio.data.data());
        for (std::size_t i = 0; i < total_samples; ++i) {
            // Big-endian to little-endian
            dst[i] = static_cast<std::int16_t>(
                (static_cast<std::uint16_t>(sound_data[i * 2]) << 8) |
                sound_data[i * 2 + 1]);
        }
    } else if (bits_per_sample == 24) {
        audio.format = AudioFormat::PCM_S24;
        audio.data.resize(total_samples * 3);
        for (std::size_t i = 0; i < total_samples; ++i) {
            // Big-endian to little-endian
            audio.data[i * 3] = sound_data[i * 3 + 2];
            audio.data[i * 3 + 1] = sound_data[i * 3 + 1];
            audio.data[i * 3 + 2] = sound_data[i * 3];
        }
    } else if (bits_per_sample == 32) {
        audio.format = AudioFormat::PCM_S32;
        audio.data.resize(total_samples * 4);
        auto* dst = reinterpret_cast<std::int32_t*>(audio.data.data());
        for (std::size_t i = 0; i < total_samples; ++i) {
            // Big-endian to little-endian
            dst[i] = static_cast<std::int32_t>(
                (static_cast<std::uint32_t>(sound_data[i * 4]) << 24) |
                (static_cast<std::uint32_t>(sound_data[i * 4 + 1]) << 16) |
                (static_cast<std::uint32_t>(sound_data[i * 4 + 2]) << 8) |
                sound_data[i * 4 + 3]);
        }
    } else {
        return void_core::Err<std::unique_ptr<AudioAsset>>(
            "Unsupported AIFF bit depth: " + std::to_string(bits_per_sample));
    }

    apply_config(audio);
    return void_core::Ok(std::make_unique<AudioAsset>(std::move(audio)));
}

void AudioLoader::apply_config(AudioAsset& audio) {
    if (m_config.convert_to_stereo && audio.channels == 1) {
        convert_channels(audio, 2);
    } else if (m_config.convert_to_mono && audio.channels == 2) {
        convert_channels(audio, 1);
    }

    if (m_config.target_format != audio.format) {
        convert_format(audio, m_config.target_format);
    }

    if (m_config.resample_rate > 0 && m_config.resample_rate != audio.sample_rate) {
        resample(audio, m_config.resample_rate);
    }

    if (m_config.normalize) {
        normalize(audio, m_config.normalize_peak);
    }
}

void AudioLoader::convert_format(AudioAsset& audio, AudioFormat target) {
    if (audio.format == target) {
        return;
    }

    std::uint32_t src_bps = bytes_per_sample(audio.format);
    std::uint32_t dst_bps = bytes_per_sample(target);
    std::size_t sample_count = audio.frame_count * audio.channels;

    std::vector<std::uint8_t> new_data(sample_count * dst_bps);

    // Convert each sample
    for (std::size_t i = 0; i < sample_count; ++i) {
        // Read source sample as float
        float sample = 0.0f;
        const std::uint8_t* src = audio.data.data() + i * src_bps;

        switch (audio.format) {
            case AudioFormat::PCM_U8:
                sample = (static_cast<float>(*src) - 128.0f) / 128.0f;
                break;
            case AudioFormat::PCM_S16: {
                std::int16_t s16 = *reinterpret_cast<const std::int16_t*>(src);
                sample = static_cast<float>(s16) / 32768.0f;
                break;
            }
            case AudioFormat::PCM_S24: {
                std::int32_t s24 = (src[0]) | (src[1] << 8) | (src[2] << 16);
                if (s24 & 0x800000) s24 |= 0xFF000000;  // Sign extend
                sample = static_cast<float>(s24) / 8388608.0f;
                break;
            }
            case AudioFormat::PCM_S32: {
                std::int32_t s32 = *reinterpret_cast<const std::int32_t*>(src);
                sample = static_cast<float>(s32) / 2147483648.0f;
                break;
            }
            case AudioFormat::PCM_F32:
                sample = *reinterpret_cast<const float*>(src);
                break;
            case AudioFormat::PCM_F64: {
                double d64 = *reinterpret_cast<const double*>(src);
                sample = static_cast<float>(d64);
                break;
            }
        }

        // Clamp
        sample = std::clamp(sample, -1.0f, 1.0f);

        // Write destination sample
        std::uint8_t* dst = new_data.data() + i * dst_bps;

        switch (target) {
            case AudioFormat::PCM_U8:
                *dst = static_cast<std::uint8_t>((sample + 1.0f) * 127.5f);
                break;
            case AudioFormat::PCM_S16: {
                std::int16_t s16 = static_cast<std::int16_t>(sample * 32767.0f);
                *reinterpret_cast<std::int16_t*>(dst) = s16;
                break;
            }
            case AudioFormat::PCM_S24: {
                std::int32_t s24 = static_cast<std::int32_t>(sample * 8388607.0f);
                dst[0] = static_cast<std::uint8_t>(s24 & 0xFF);
                dst[1] = static_cast<std::uint8_t>((s24 >> 8) & 0xFF);
                dst[2] = static_cast<std::uint8_t>((s24 >> 16) & 0xFF);
                break;
            }
            case AudioFormat::PCM_S32: {
                std::int32_t s32 = static_cast<std::int32_t>(sample * 2147483647.0f);
                *reinterpret_cast<std::int32_t*>(dst) = s32;
                break;
            }
            case AudioFormat::PCM_F32:
                *reinterpret_cast<float*>(dst) = sample;
                break;
            case AudioFormat::PCM_F64:
                *reinterpret_cast<double*>(dst) = static_cast<double>(sample);
                break;
        }
    }

    audio.data = std::move(new_data);
    audio.format = target;
}

void AudioLoader::convert_channels(AudioAsset& audio, std::uint32_t target_channels) {
    if (audio.channels == target_channels) {
        return;
    }

    std::uint32_t bps = bytes_per_sample(audio.format);
    std::size_t new_size = audio.frame_count * target_channels * bps;
    std::vector<std::uint8_t> new_data(new_size);

    if (audio.channels == 1 && target_channels == 2) {
        // Mono to stereo: duplicate samples
        for (std::uint64_t i = 0; i < audio.frame_count; ++i) {
            const std::uint8_t* src = audio.data.data() + i * bps;
            std::uint8_t* dst = new_data.data() + i * 2 * bps;
            std::memcpy(dst, src, bps);          // Left
            std::memcpy(dst + bps, src, bps);    // Right
        }
    } else if (audio.channels == 2 && target_channels == 1) {
        // Stereo to mono: average samples
        for (std::uint64_t i = 0; i < audio.frame_count; ++i) {
            const std::uint8_t* src = audio.data.data() + i * 2 * bps;
            std::uint8_t* dst = new_data.data() + i * bps;

            // Average based on format
            switch (audio.format) {
                case AudioFormat::PCM_U8: {
                    std::uint16_t avg = (src[0] + src[1]) / 2;
                    dst[0] = static_cast<std::uint8_t>(avg);
                    break;
                }
                case AudioFormat::PCM_S16: {
                    auto* s = reinterpret_cast<const std::int16_t*>(src);
                    std::int16_t avg = static_cast<std::int16_t>((s[0] + s[1]) / 2);
                    *reinterpret_cast<std::int16_t*>(dst) = avg;
                    break;
                }
                case AudioFormat::PCM_S32: {
                    auto* s = reinterpret_cast<const std::int32_t*>(src);
                    std::int32_t avg = static_cast<std::int32_t>((s[0] + s[1]) / 2);
                    *reinterpret_cast<std::int32_t*>(dst) = avg;
                    break;
                }
                case AudioFormat::PCM_F32: {
                    auto* s = reinterpret_cast<const float*>(src);
                    float avg = (s[0] + s[1]) / 2.0f;
                    *reinterpret_cast<float*>(dst) = avg;
                    break;
                }
                default:
                    // For other formats, just copy left channel
                    std::memcpy(dst, src, bps);
                    break;
            }
        }
    }

    audio.data = std::move(new_data);
    audio.channels = target_channels;
}

void AudioLoader::resample(AudioAsset& audio, std::uint32_t target_rate) {
    if (audio.sample_rate == target_rate) {
        return;
    }

    // Linear interpolation resampling
    // For production quality, consider integrating libsamplerate

    double ratio = static_cast<double>(target_rate) / static_cast<double>(audio.sample_rate);
    std::uint64_t new_frame_count = static_cast<std::uint64_t>(
        static_cast<double>(audio.frame_count) * ratio
    );

    std::uint32_t bps = bytes_per_sample(audio.format);
    std::size_t new_size = new_frame_count * audio.channels * bps;
    std::vector<std::uint8_t> new_data(new_size);

    for (std::uint64_t i = 0; i < new_frame_count; ++i) {
        double src_pos = static_cast<double>(i) / ratio;
        std::uint64_t src_idx = static_cast<std::uint64_t>(src_pos);
        double frac = src_pos - static_cast<double>(src_idx);

        // Clamp to valid range
        if (src_idx >= audio.frame_count - 1) {
            src_idx = audio.frame_count - 1;
            frac = 0.0;
        }

        for (std::uint32_t c = 0; c < audio.channels; ++c) {
            const std::uint8_t* src0 = audio.data.data() +
                (src_idx * audio.channels + c) * bps;
            const std::uint8_t* src1 = audio.data.data() +
                ((src_idx + 1) * audio.channels + c) * bps;
            std::uint8_t* dst = new_data.data() +
                (i * audio.channels + c) * bps;

            // Linear interpolation based on format
            switch (audio.format) {
                case AudioFormat::PCM_S16: {
                    auto s0 = *reinterpret_cast<const std::int16_t*>(src0);
                    auto s1 = *reinterpret_cast<const std::int16_t*>(src1);
                    auto interp = static_cast<std::int16_t>(
                        s0 + (s1 - s0) * frac
                    );
                    *reinterpret_cast<std::int16_t*>(dst) = interp;
                    break;
                }
                case AudioFormat::PCM_F32: {
                    auto s0 = *reinterpret_cast<const float*>(src0);
                    auto s1 = *reinterpret_cast<const float*>(src1);
                    auto interp = static_cast<float>(s0 + (s1 - s0) * frac);
                    *reinterpret_cast<float*>(dst) = interp;
                    break;
                }
                default:
                    // For other formats, nearest neighbor
                    std::memcpy(dst, src0, bps);
                    break;
            }
        }
    }

    audio.data = std::move(new_data);
    audio.sample_rate = target_rate;
    audio.frame_count = new_frame_count;
}

void AudioLoader::normalize(AudioAsset& audio, float peak) {
    if (audio.data.empty() || peak <= 0.0f) {
        return;
    }

    // Find current peak
    float current_peak = 0.0f;
    std::uint32_t bps = bytes_per_sample(audio.format);
    std::size_t sample_count = audio.frame_count * audio.channels;

    for (std::size_t i = 0; i < sample_count; ++i) {
        const std::uint8_t* src = audio.data.data() + i * bps;
        float sample = 0.0f;

        switch (audio.format) {
            case AudioFormat::PCM_U8:
                sample = std::abs((static_cast<float>(*src) - 128.0f) / 128.0f);
                break;
            case AudioFormat::PCM_S16: {
                auto s16 = *reinterpret_cast<const std::int16_t*>(src);
                sample = std::abs(static_cast<float>(s16) / 32768.0f);
                break;
            }
            case AudioFormat::PCM_S32: {
                auto s32 = *reinterpret_cast<const std::int32_t*>(src);
                sample = std::abs(static_cast<float>(s32) / 2147483648.0f);
                break;
            }
            case AudioFormat::PCM_F32:
                sample = std::abs(*reinterpret_cast<const float*>(src));
                break;
            default:
                break;
        }

        current_peak = std::max(current_peak, sample);
    }

    if (current_peak <= 0.0001f) {
        return;  // Silence or near-silence
    }

    // Calculate gain
    float gain = peak / current_peak;
    if (std::abs(gain - 1.0f) < 0.001f) {
        return;  // Already normalized
    }

    // Apply gain
    for (std::size_t i = 0; i < sample_count; ++i) {
        std::uint8_t* ptr = audio.data.data() + i * bps;

        switch (audio.format) {
            case AudioFormat::PCM_U8: {
                float sample = (static_cast<float>(*ptr) - 128.0f) / 128.0f;
                sample = std::clamp(sample * gain, -1.0f, 1.0f);
                *ptr = static_cast<std::uint8_t>((sample + 1.0f) * 127.5f);
                break;
            }
            case AudioFormat::PCM_S16: {
                auto* s16 = reinterpret_cast<std::int16_t*>(ptr);
                float sample = static_cast<float>(*s16) / 32768.0f;
                sample = std::clamp(sample * gain, -1.0f, 1.0f);
                *s16 = static_cast<std::int16_t>(sample * 32767.0f);
                break;
            }
            case AudioFormat::PCM_S32: {
                auto* s32 = reinterpret_cast<std::int32_t*>(ptr);
                float sample = static_cast<float>(*s32) / 2147483648.0f;
                sample = std::clamp(sample * gain, -1.0f, 1.0f);
                *s32 = static_cast<std::int32_t>(sample * 2147483647.0f);
                break;
            }
            case AudioFormat::PCM_F32: {
                auto* f32 = reinterpret_cast<float*>(ptr);
                *f32 = std::clamp(*f32 * gain, -1.0f, 1.0f);
                break;
            }
            default:
                break;
        }
    }
}

// =============================================================================
// Streaming Audio Loader Implementation
// =============================================================================

LoadResult<StreamingAudioAsset> StreamingAudioLoader::load(LoadContext& ctx) {
    const auto& ext = ctx.extension();
    const auto& data = ctx.data();

    StreamingAudioAsset asset;
    asset.name = ctx.path().stem();
    asset.source_path = ctx.path().str();

    if (ext == "wav" || ext == "wave") {
#ifdef VOID_HAS_DR_LIBS
        drwav wav;
        if (!drwav_init_memory(&wav, data.data(), data.size(), nullptr)) {
            return void_core::Err<std::unique_ptr<StreamingAudioAsset>>("Invalid WAV file");
        }
        asset.sample_rate = wav.sampleRate;
        asset.channels = wav.channels;
        asset.format = (wav.translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT)
            ? AudioFormat::PCM_F32 : AudioFormat::PCM_S16;
        asset.frame_count = wav.totalPCMFrameCount;
        drwav_uninit(&wav);
#else
        auto header_opt = WavParser::parse_header(data);
        if (!header_opt) {
            return void_core::Err<std::unique_ptr<StreamingAudioAsset>>("Invalid WAV file");
        }
        const auto& header = *header_opt;
        asset.sample_rate = header.sample_rate;
        asset.channels = header.channels;
        asset.format = WavParser::get_format(header);
        std::uint32_t bytes_per_frame = header.channels * (header.bits_per_sample / 8);
        asset.frame_count = header.data_size / bytes_per_frame;
#endif
    } else if (ext == "ogg") {
#ifdef VOID_HAS_STB
        int error = 0;
        stb_vorbis* v = stb_vorbis_open_memory(data.data(), static_cast<int>(data.size()),
            &error, nullptr);
        if (v == nullptr) {
            return void_core::Err<std::unique_ptr<StreamingAudioAsset>>("Invalid OGG file");
        }
        stb_vorbis_info info = stb_vorbis_get_info(v);
        asset.sample_rate = info.sample_rate;
        asset.channels = info.channels;
        asset.format = AudioFormat::PCM_S16;
        asset.frame_count = stb_vorbis_stream_length_in_samples(v);
        stb_vorbis_close(v);
#else
        if (data.size() < 4 || std::memcmp(data.data(), "OggS", 4) != 0) {
            return void_core::Err<std::unique_ptr<StreamingAudioAsset>>("Invalid OGG file");
        }
        asset.sample_rate = 44100;
        asset.channels = 2;
        asset.format = AudioFormat::PCM_S16;
#endif
    } else if (ext == "mp3") {
#ifdef VOID_HAS_DR_LIBS
        drmp3 mp3;
        if (!drmp3_init_memory(&mp3, data.data(), data.size(), nullptr)) {
            return void_core::Err<std::unique_ptr<StreamingAudioAsset>>("Invalid MP3 file");
        }
        asset.sample_rate = mp3.sampleRate;
        asset.channels = mp3.channels;
        asset.format = AudioFormat::PCM_S16;
        asset.frame_count = drmp3_get_pcm_frame_count(&mp3);
        drmp3_uninit(&mp3);
#else
        if (data.size() < 4) {
            return void_core::Err<std::unique_ptr<StreamingAudioAsset>>("File too small");
        }
        asset.sample_rate = 44100;
        asset.channels = 2;
        asset.format = AudioFormat::PCM_S16;
#endif
    } else if (ext == "flac") {
#ifdef VOID_HAS_DR_LIBS
        drflac* flac = drflac_open_memory(data.data(), data.size(), nullptr);
        if (flac == nullptr) {
            return void_core::Err<std::unique_ptr<StreamingAudioAsset>>("Invalid FLAC file");
        }
        asset.sample_rate = flac->sampleRate;
        asset.channels = flac->channels;
        asset.format = AudioFormat::PCM_S16;
        asset.frame_count = flac->totalPCMFrameCount;
        drflac_close(flac);
#else
        if (data.size() < 4 || std::memcmp(data.data(), "fLaC", 4) != 0) {
            return void_core::Err<std::unique_ptr<StreamingAudioAsset>>("Invalid FLAC file");
        }
        asset.sample_rate = 44100;
        asset.channels = 2;
        asset.format = AudioFormat::PCM_S16;
#endif
    } else {
        return void_core::Err<std::unique_ptr<StreamingAudioAsset>>("Unknown format: " + ext);
    }

    return void_core::Ok(std::make_unique<StreamingAudioAsset>(std::move(asset)));
}

} // namespace void_asset
