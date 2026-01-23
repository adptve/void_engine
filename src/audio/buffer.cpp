/// @file buffer.cpp
/// @brief Audio buffer implementations for void_audio

#include <void_engine/audio/buffer.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <random>

namespace void_audio {

// =============================================================================
// AudioBuffer Implementation
// =============================================================================

AudioBuffer::AudioBuffer(const AudioBufferDesc& desc) {
    load(desc);
}

float AudioBuffer::duration() const {
    if (m_sample_rate == 0) return 0;
    return static_cast<float>(m_sample_count) / static_cast<float>(m_sample_rate);
}

void_core::Result<void> AudioBuffer::load(const AudioBufferDesc& desc) {
    m_format = desc.format;
    m_sample_rate = desc.sample_rate;
    m_sample_count = desc.sample_count;
    m_name = desc.name;

    if (desc.data && desc.data_size > 0) {
        m_data.resize(desc.data_size);
        std::memcpy(m_data.data(), desc.data, desc.data_size);
    }

    return {};
}

void_core::Result<BufferPtr> AudioBuffer::load_from_file(const std::filesystem::path& path) {
    auto format = detect_audio_format(path);

    if (format == AudioFileFormat::WAV) {
        WavDecoder decoder(path);
        return decoder.decode();
    }

    // For other formats, return error for now
    return void_core::Error{void_core::ErrorCode::NotSupported,
        "Audio format not supported: " + path.extension().string()};
}

void_core::Result<BufferPtr> AudioBuffer::load_from_memory(
    std::span<const std::byte> data,
    const std::string& name) {

    auto format = detect_audio_format(data);

    if (format == AudioFileFormat::WAV) {
        WavDecoder decoder(data);
        auto result = decoder.decode();
        if (result) {
            // Set name if provided
            if (!name.empty()) {
                auto* buffer = static_cast<AudioBuffer*>(result->get());
                if (buffer) {
                    buffer->m_name = name;
                }
            }
        }
        return result;
    }

    return void_core::Error{void_core::ErrorCode::NotSupported, "Audio format not supported"};
}

BufferPtr AudioBuffer::create_silence(
    AudioFormat format,
    std::uint32_t sample_rate,
    float duration_seconds) {

    auto buffer = std::make_shared<AudioBuffer>();

    std::uint32_t sample_count = static_cast<std::uint32_t>(sample_rate * duration_seconds);
    std::size_t bytes_per_sample = get_bytes_per_sample(format);
    std::size_t total_bytes = sample_count * bytes_per_sample;

    buffer->m_format = format;
    buffer->m_sample_rate = sample_rate;
    buffer->m_sample_count = sample_count;
    buffer->m_name = "silence";
    buffer->m_data.resize(total_bytes, std::byte{0});

    return buffer;
}

BufferPtr AudioBuffer::create_sine_wave(
    float frequency,
    float amplitude,
    AudioFormat format,
    std::uint32_t sample_rate,
    float duration_seconds) {

    auto buffer = std::make_shared<AudioBuffer>();

    std::uint32_t sample_count = static_cast<std::uint32_t>(sample_rate * duration_seconds);
    std::uint32_t channels = get_channel_count(format);

    buffer->m_format = format;
    buffer->m_sample_rate = sample_rate;
    buffer->m_sample_count = sample_count;
    buffer->m_name = "sine_" + std::to_string(static_cast<int>(frequency)) + "hz";

    constexpr float pi = 3.14159265358979323846f;
    float angular_freq = 2.0f * pi * frequency / static_cast<float>(sample_rate);

    if (format == AudioFormat::Mono16 || format == AudioFormat::Stereo16) {
        std::size_t total_samples = sample_count * channels;
        buffer->m_data.resize(total_samples * 2);
        auto* data = reinterpret_cast<std::int16_t*>(buffer->m_data.data());

        for (std::uint32_t i = 0; i < sample_count; ++i) {
            float sample = amplitude * std::sin(angular_freq * static_cast<float>(i));
            std::int16_t value = static_cast<std::int16_t>(sample * 32767.0f);

            for (std::uint32_t c = 0; c < channels; ++c) {
                data[i * channels + c] = value;
            }
        }
    } else if (format == AudioFormat::MonoFloat || format == AudioFormat::StereoFloat) {
        std::size_t total_samples = sample_count * channels;
        buffer->m_data.resize(total_samples * 4);
        auto* data = reinterpret_cast<float*>(buffer->m_data.data());

        for (std::uint32_t i = 0; i < sample_count; ++i) {
            float sample = amplitude * std::sin(angular_freq * static_cast<float>(i));

            for (std::uint32_t c = 0; c < channels; ++c) {
                data[i * channels + c] = sample;
            }
        }
    }

    return buffer;
}

BufferPtr AudioBuffer::create_white_noise(
    float amplitude,
    AudioFormat format,
    std::uint32_t sample_rate,
    float duration_seconds) {

    auto buffer = std::make_shared<AudioBuffer>();

    std::uint32_t sample_count = static_cast<std::uint32_t>(sample_rate * duration_seconds);
    std::uint32_t channels = get_channel_count(format);

    buffer->m_format = format;
    buffer->m_sample_rate = sample_rate;
    buffer->m_sample_count = sample_count;
    buffer->m_name = "white_noise";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    if (format == AudioFormat::Mono16 || format == AudioFormat::Stereo16) {
        std::size_t total_samples = sample_count * channels;
        buffer->m_data.resize(total_samples * 2);
        auto* data = reinterpret_cast<std::int16_t*>(buffer->m_data.data());

        for (std::size_t i = 0; i < total_samples; ++i) {
            float sample = amplitude * dist(gen);
            data[i] = static_cast<std::int16_t>(sample * 32767.0f);
        }
    } else if (format == AudioFormat::MonoFloat || format == AudioFormat::StereoFloat) {
        std::size_t total_samples = sample_count * channels;
        buffer->m_data.resize(total_samples * 4);
        auto* data = reinterpret_cast<float*>(buffer->m_data.data());

        for (std::size_t i = 0; i < total_samples; ++i) {
            data[i] = amplitude * dist(gen);
        }
    }

    return buffer;
}

// =============================================================================
// StreamingBuffer Implementation
// =============================================================================

StreamingBuffer::StreamingBuffer(const AudioBufferDesc& desc, StreamCallback callback)
    : m_format(desc.format)
    , m_sample_rate(desc.sample_rate)
    , m_sample_count(desc.sample_count)
    , m_name(desc.name)
    , m_callback(std::move(callback)) {
}

float StreamingBuffer::duration() const {
    if (m_sample_rate == 0) return 0;
    return static_cast<float>(m_sample_count) / static_cast<float>(m_sample_rate);
}

std::size_t StreamingBuffer::size_bytes() const {
    return static_cast<std::size_t>(m_sample_count) * get_bytes_per_sample(m_format);
}

std::size_t StreamingBuffer::read(void* buffer, std::size_t bytes) {
    if (!m_callback) return 0;

    std::size_t read_bytes = m_callback(buffer, bytes);
    if (read_bytes == 0) {
        m_at_end = true;
    }

    std::size_t bytes_per_sample = get_bytes_per_sample(m_format);
    if (bytes_per_sample > 0) {
        m_position += read_bytes / bytes_per_sample;
    }

    return read_bytes;
}

bool StreamingBuffer::seek(std::uint64_t sample_position) {
    m_position = sample_position;
    m_at_end = false;
    return true;
}

void StreamingBuffer::reset() {
    m_position = 0;
    m_at_end = false;
}

void_core::Result<std::shared_ptr<StreamingBuffer>> StreamingBuffer::open_file(
    const std::filesystem::path& path) {

    auto format = detect_audio_format(path);
    if (format == AudioFileFormat::Unknown) {
        return void_core::Error{void_core::ErrorCode::NotSupported, "Unknown audio format"};
    }

    // Create decoder
    auto decoder = create_decoder(path);
    if (!decoder) {
        return void_core::Error{void_core::ErrorCode::ParseError, "Failed to create decoder"};
    }

    auto info = decoder->info();

    AudioBufferDesc desc;
    desc.format = info.audio_format;
    desc.sample_rate = info.sample_rate;
    desc.sample_count = info.sample_count;
    desc.name = path.filename().string();
    desc.streaming = true;

    // Capture decoder in callback
    auto decoder_ptr = std::shared_ptr<IAudioDecoder>(std::move(decoder));
    auto callback = [decoder_ptr](void* buffer, std::size_t bytes) -> std::size_t {
        return decoder_ptr->read(buffer, bytes);
    };

    return std::make_shared<StreamingBuffer>(desc, callback);
}

// =============================================================================
// Audio File Detection
// =============================================================================

AudioFileFormat detect_audio_format(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".wav" || ext == ".wave") return AudioFileFormat::WAV;
    if (ext == ".ogg" || ext == ".oga") return AudioFileFormat::OGG;
    if (ext == ".mp3") return AudioFileFormat::MP3;
    if (ext == ".flac") return AudioFileFormat::FLAC;
    if (ext == ".aiff" || ext == ".aif") return AudioFileFormat::AIFF;

    return AudioFileFormat::Unknown;
}

AudioFileFormat detect_audio_format(std::span<const std::byte> data) {
    if (data.size() < 12) return AudioFileFormat::Unknown;

    // Check magic numbers
    const auto* bytes = reinterpret_cast<const unsigned char*>(data.data());

    // WAV: "RIFF" + size + "WAVE"
    if (bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
        bytes[8] == 'W' && bytes[9] == 'A' && bytes[10] == 'V' && bytes[11] == 'E') {
        return AudioFileFormat::WAV;
    }

    // OGG: "OggS"
    if (bytes[0] == 'O' && bytes[1] == 'g' && bytes[2] == 'g' && bytes[3] == 'S') {
        return AudioFileFormat::OGG;
    }

    // MP3: ID3 tag or frame sync
    if ((bytes[0] == 'I' && bytes[1] == 'D' && bytes[2] == '3') ||
        (bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0)) {
        return AudioFileFormat::MP3;
    }

    // FLAC: "fLaC"
    if (bytes[0] == 'f' && bytes[1] == 'L' && bytes[2] == 'a' && bytes[3] == 'C') {
        return AudioFileFormat::FLAC;
    }

    // AIFF: "FORM" + size + "AIFF"
    if (bytes[0] == 'F' && bytes[1] == 'O' && bytes[2] == 'R' && bytes[3] == 'M' &&
        bytes[8] == 'A' && bytes[9] == 'I' && bytes[10] == 'F' && bytes[11] == 'F') {
        return AudioFileFormat::AIFF;
    }

    return AudioFileFormat::Unknown;
}

void_core::Result<AudioFileInfo> get_audio_file_info(const std::filesystem::path& path) {
    auto format = detect_audio_format(path);
    if (format == AudioFileFormat::Unknown) {
        return void_core::Error{void_core::ErrorCode::NotSupported, "Unknown audio format"};
    }

    auto decoder = create_decoder(path);
    if (!decoder) {
        return void_core::Error{void_core::ErrorCode::ParseError, "Failed to open file"};
    }

    return decoder->info();
}

std::unique_ptr<IAudioDecoder> create_decoder(const std::filesystem::path& path) {
    auto format = detect_audio_format(path);

    switch (format) {
        case AudioFileFormat::WAV:
            return std::make_unique<WavDecoder>(path);
        default:
            return nullptr;
    }
}

std::unique_ptr<IAudioDecoder> create_decoder(std::span<const std::byte> data) {
    auto format = detect_audio_format(data);

    switch (format) {
        case AudioFileFormat::WAV:
            return std::make_unique<WavDecoder>(data);
        default:
            return nullptr;
    }
}

// =============================================================================
// WAV Decoder Implementation
// =============================================================================

WavDecoder::WavDecoder(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return;

    std::size_t size = static_cast<std::size_t>(file.tellg());
    file.seekg(0);

    m_file_data.resize(size);
    file.read(reinterpret_cast<char*>(m_file_data.data()), static_cast<std::streamsize>(size));

    m_info.file_format = AudioFileFormat::WAV;
    m_info.file_size = size;

    parse_header();
}

WavDecoder::WavDecoder(std::span<const std::byte> data) {
    m_file_data.assign(data.begin(), data.end());
    m_info.file_format = AudioFileFormat::WAV;
    m_info.file_size = data.size();

    parse_header();
}

void_core::Result<void> WavDecoder::parse_header() {
    if (m_file_data.size() < 44) {
        return void_core::Error{void_core::ErrorCode::ParseError, "WAV file too small"};
    }

    const auto* data = reinterpret_cast<const unsigned char*>(m_file_data.data());

    // Verify RIFF header
    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F') {
        return void_core::Error{void_core::ErrorCode::ParseError, "Not a RIFF file"};
    }

    // Verify WAVE format
    if (data[8] != 'W' || data[9] != 'A' || data[10] != 'V' || data[11] != 'E') {
        return void_core::Error{void_core::ErrorCode::ParseError, "Not a WAVE file"};
    }

    // Find fmt chunk
    std::size_t pos = 12;
    while (pos + 8 <= m_file_data.size()) {
        char chunk_id[5] = {
            static_cast<char>(data[pos]),
            static_cast<char>(data[pos + 1]),
            static_cast<char>(data[pos + 2]),
            static_cast<char>(data[pos + 3]),
            0
        };

        std::uint32_t chunk_size = data[pos + 4] | (data[pos + 5] << 8) |
                                   (data[pos + 6] << 16) | (data[pos + 7] << 24);

        if (std::strcmp(chunk_id, "fmt ") == 0) {
            if (chunk_size < 16) {
                return void_core::Error{void_core::ErrorCode::ParseError, "Invalid fmt chunk"};
            }

            std::uint16_t format_tag = data[pos + 8] | (data[pos + 9] << 8);
            m_info.channels = data[pos + 10] | (data[pos + 11] << 8);
            m_info.sample_rate = data[pos + 12] | (data[pos + 13] << 8) |
                                 (data[pos + 14] << 16) | (data[pos + 15] << 24);
            m_info.bits_per_sample = data[pos + 22] | (data[pos + 23] << 8);

            // Determine format
            if (format_tag == 1) { // PCM
                if (m_info.channels == 1) {
                    m_info.audio_format = m_info.bits_per_sample == 8 ?
                        AudioFormat::Mono8 : AudioFormat::Mono16;
                } else {
                    m_info.audio_format = m_info.bits_per_sample == 8 ?
                        AudioFormat::Stereo8 : AudioFormat::Stereo16;
                }
            } else if (format_tag == 3) { // IEEE float
                m_info.audio_format = m_info.channels == 1 ?
                    AudioFormat::MonoFloat : AudioFormat::StereoFloat;
            }
        } else if (std::strcmp(chunk_id, "data") == 0) {
            m_data_offset = pos + 8;
            std::size_t data_size = chunk_size;

            std::size_t bytes_per_sample = (m_info.bits_per_sample / 8) * m_info.channels;
            if (bytes_per_sample > 0) {
                m_info.sample_count = static_cast<std::uint32_t>(data_size / bytes_per_sample);
            }

            m_info.duration = static_cast<float>(m_info.sample_count) /
                              static_cast<float>(m_info.sample_rate);
            break;
        }

        pos += 8 + chunk_size;
        if (chunk_size & 1) pos++; // Padding
    }

    return {};
}

void_core::Result<BufferPtr> WavDecoder::decode() {
    auto buffer = std::make_shared<AudioBuffer>();

    AudioBufferDesc desc;
    desc.format = m_info.audio_format;
    desc.sample_rate = m_info.sample_rate;
    desc.sample_count = m_info.sample_count;
    desc.data = m_file_data.data() + m_data_offset;
    desc.data_size = m_info.sample_count * (m_info.bits_per_sample / 8) * m_info.channels;

    auto result = buffer->load(desc);
    if (!result) {
        return void_core::Error{result.error().code(), result.error().message()};
    }

    return BufferPtr(buffer);
}

std::size_t WavDecoder::read(void* buffer, std::size_t bytes) {
    std::size_t bytes_per_sample = (m_info.bits_per_sample / 8) * m_info.channels;
    std::size_t current_byte = m_data_offset + m_position * bytes_per_sample;

    std::size_t available = m_file_data.size() - current_byte;
    std::size_t to_read = std::min(bytes, available);

    if (to_read > 0) {
        std::memcpy(buffer, m_file_data.data() + current_byte, to_read);
        m_position += to_read / bytes_per_sample;
    }

    return to_read;
}

bool WavDecoder::seek(std::uint64_t sample) {
    if (sample >= m_info.sample_count) return false;
    m_position = sample;
    return true;
}

// =============================================================================
// AudioBufferPool Implementation
// =============================================================================

AudioBufferPool::AudioBufferPool(std::size_t max_buffers)
    : m_max_buffers(max_buffers) {
}

void_core::Result<BufferPtr> AudioBufferPool::get(const std::filesystem::path& path) {
    std::string key = path.string();

    auto it = m_path_cache.find(key);
    if (it != m_path_cache.end()) {
        auto buf_it = m_buffers.find(it->second);
        if (buf_it != m_buffers.end()) {
            return buf_it->second;
        }
    }

    // Load buffer
    auto result = AudioBuffer::load_from_file(path);
    if (!result) {
        return result;
    }

    BufferId id{m_next_id++};
    auto* buffer = static_cast<AudioBuffer*>(result->get());
    buffer->set_id(id);

    m_buffers[id] = *result;
    m_path_cache[key] = id;

    return *result;
}

void_core::Result<BufferId> AudioBufferPool::load(const std::filesystem::path& path) {
    auto result = get(path);
    if (!result) {
        return void_core::Error{result.error().code(), result.error().message()};
    }
    return result->get()->id();
}

BufferId AudioBufferPool::create(const AudioBufferDesc& desc) {
    BufferId id{m_next_id++};
    auto buffer = std::make_shared<AudioBuffer>(desc);
    buffer->set_id(id);
    m_buffers[id] = buffer;
    return id;
}

BufferPtr AudioBufferPool::get(BufferId id) {
    auto it = m_buffers.find(id);
    return it != m_buffers.end() ? it->second : nullptr;
}

void AudioBufferPool::release(BufferId id) {
    m_buffers.erase(id);

    // Remove from path cache
    for (auto it = m_path_cache.begin(); it != m_path_cache.end(); ) {
        if (it->second == id) {
            it = m_path_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void AudioBufferPool::clear() {
    m_buffers.clear();
    m_path_cache.clear();
}

std::size_t AudioBufferPool::memory_usage() const {
    std::size_t total = 0;
    for (const auto& [id, buffer] : m_buffers) {
        if (buffer) {
            total += buffer->size_bytes();
        }
    }
    return total;
}

} // namespace void_audio
