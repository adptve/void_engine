/// @file buffer.hpp
/// @brief Audio buffer interface and implementations for void_audio

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/core/error.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace void_audio {

// =============================================================================
// Audio Buffer Interface
// =============================================================================

/// Interface for audio buffers
class IAudioBuffer {
public:
    virtual ~IAudioBuffer() = default;

    /// Get buffer ID
    [[nodiscard]] virtual BufferId id() const = 0;

    /// Get buffer name
    [[nodiscard]] virtual const std::string& name() const = 0;

    /// Get audio format
    [[nodiscard]] virtual AudioFormat format() const = 0;

    /// Get sample rate
    [[nodiscard]] virtual std::uint32_t sample_rate() const = 0;

    /// Get number of samples
    [[nodiscard]] virtual std::uint32_t sample_count() const = 0;

    /// Get duration in seconds
    [[nodiscard]] virtual float duration() const = 0;

    /// Get raw buffer size in bytes
    [[nodiscard]] virtual std::size_t size_bytes() const = 0;

    /// Check if buffer is streaming
    [[nodiscard]] virtual bool is_streaming() const = 0;

    /// Check if buffer is loaded
    [[nodiscard]] virtual bool is_loaded() const = 0;

    /// Get raw data pointer (may be null for streaming buffers)
    [[nodiscard]] virtual const void* data() const = 0;

    /// Get native handle (backend-specific)
    [[nodiscard]] virtual void* native_handle() const = 0;
};

// =============================================================================
// Static Audio Buffer
// =============================================================================

/// Static audio buffer that holds all data in memory
class AudioBuffer : public IAudioBuffer {
public:
    AudioBuffer() = default;
    explicit AudioBuffer(const AudioBufferDesc& desc);

    // IAudioBuffer interface
    [[nodiscard]] BufferId id() const override { return m_id; }
    [[nodiscard]] const std::string& name() const override { return m_name; }
    [[nodiscard]] AudioFormat format() const override { return m_format; }
    [[nodiscard]] std::uint32_t sample_rate() const override { return m_sample_rate; }
    [[nodiscard]] std::uint32_t sample_count() const override { return m_sample_count; }
    [[nodiscard]] float duration() const override;
    [[nodiscard]] std::size_t size_bytes() const override { return m_data.size(); }
    [[nodiscard]] bool is_streaming() const override { return false; }
    [[nodiscard]] bool is_loaded() const override { return !m_data.empty(); }
    [[nodiscard]] const void* data() const override { return m_data.data(); }
    [[nodiscard]] void* native_handle() const override { return m_native_handle; }

    /// Set native handle
    void set_native_handle(void* handle) { m_native_handle = handle; }

    /// Set buffer ID
    void set_id(BufferId id) { m_id = id; }

    /// Get mutable data
    std::vector<std::byte>& mutable_data() { return m_data; }

    /// Load from raw data
    void_core::Result<void> load(const AudioBufferDesc& desc);

    /// Load from file
    static void_core::Result<BufferPtr> load_from_file(const std::filesystem::path& path);

    /// Load from memory
    static void_core::Result<BufferPtr> load_from_memory(
        std::span<const std::byte> data,
        const std::string& name = "");

    /// Create silence buffer
    static BufferPtr create_silence(
        AudioFormat format,
        std::uint32_t sample_rate,
        float duration_seconds);

    /// Create sine wave buffer
    static BufferPtr create_sine_wave(
        float frequency,
        float amplitude,
        AudioFormat format,
        std::uint32_t sample_rate,
        float duration_seconds);

    /// Create white noise buffer
    static BufferPtr create_white_noise(
        float amplitude,
        AudioFormat format,
        std::uint32_t sample_rate,
        float duration_seconds);

private:
    BufferId m_id{0};
    std::string m_name;
    AudioFormat m_format = AudioFormat::Unknown;
    std::uint32_t m_sample_rate = 0;
    std::uint32_t m_sample_count = 0;
    std::vector<std::byte> m_data;
    void* m_native_handle = nullptr;
};

// =============================================================================
// Streaming Audio Buffer
// =============================================================================

/// Callback for providing streaming audio data
using StreamCallback = std::function<std::size_t(void* buffer, std::size_t bytes)>;

/// Streaming audio buffer for large files
class StreamingBuffer : public IAudioBuffer {
public:
    StreamingBuffer() = default;
    StreamingBuffer(const AudioBufferDesc& desc, StreamCallback callback);

    // IAudioBuffer interface
    [[nodiscard]] BufferId id() const override { return m_id; }
    [[nodiscard]] const std::string& name() const override { return m_name; }
    [[nodiscard]] AudioFormat format() const override { return m_format; }
    [[nodiscard]] std::uint32_t sample_rate() const override { return m_sample_rate; }
    [[nodiscard]] std::uint32_t sample_count() const override { return m_sample_count; }
    [[nodiscard]] float duration() const override;
    [[nodiscard]] std::size_t size_bytes() const override;
    [[nodiscard]] bool is_streaming() const override { return true; }
    [[nodiscard]] bool is_loaded() const override { return m_callback != nullptr; }
    [[nodiscard]] const void* data() const override { return nullptr; }
    [[nodiscard]] void* native_handle() const override { return m_native_handle; }

    /// Set buffer ID
    void set_id(BufferId id) { m_id = id; }

    /// Set native handle
    void set_native_handle(void* handle) { m_native_handle = handle; }

    /// Read data into buffer
    std::size_t read(void* buffer, std::size_t bytes);

    /// Seek to position (in samples)
    bool seek(std::uint64_t sample_position);

    /// Get current position (in samples)
    [[nodiscard]] std::uint64_t position() const { return m_position; }

    /// Check if at end of stream
    [[nodiscard]] bool at_end() const { return m_at_end; }

    /// Reset to beginning
    void reset();

    /// Open a file for streaming
    static void_core::Result<std::shared_ptr<StreamingBuffer>> open_file(
        const std::filesystem::path& path);

private:
    BufferId m_id{0};
    std::string m_name;
    AudioFormat m_format = AudioFormat::Unknown;
    std::uint32_t m_sample_rate = 0;
    std::uint32_t m_sample_count = 0;

    StreamCallback m_callback;
    std::uint64_t m_position = 0;
    bool m_at_end = false;

    void* m_native_handle = nullptr;

    // File handle for file-based streaming
    struct FileStream;
    std::shared_ptr<FileStream> m_file_stream;
};

// =============================================================================
// Audio File Loading
// =============================================================================

/// Supported audio file formats
enum class AudioFileFormat {
    Unknown,
    WAV,
    OGG,
    MP3,
    FLAC,
    AIFF
};

/// Detect audio file format from path or data
AudioFileFormat detect_audio_format(const std::filesystem::path& path);
AudioFileFormat detect_audio_format(std::span<const std::byte> data);

/// Audio file information
struct AudioFileInfo {
    AudioFileFormat file_format = AudioFileFormat::Unknown;
    AudioFormat audio_format = AudioFormat::Unknown;
    std::uint32_t sample_rate = 0;
    std::uint32_t sample_count = 0;
    std::uint32_t channels = 0;
    std::uint32_t bits_per_sample = 0;
    float duration = 0.0f;
    std::uint64_t file_size = 0;
};

/// Get information about an audio file
void_core::Result<AudioFileInfo> get_audio_file_info(const std::filesystem::path& path);

/// Audio decoder interface
class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;

    /// Get file info
    [[nodiscard]] virtual AudioFileInfo info() const = 0;

    /// Decode entire file to buffer
    virtual void_core::Result<BufferPtr> decode() = 0;

    /// Read samples (for streaming)
    virtual std::size_t read(void* buffer, std::size_t bytes) = 0;

    /// Seek to position (in samples)
    virtual bool seek(std::uint64_t sample) = 0;

    /// Get current position (in samples)
    [[nodiscard]] virtual std::uint64_t position() const = 0;
};

/// Create decoder for file
std::unique_ptr<IAudioDecoder> create_decoder(const std::filesystem::path& path);

/// Create decoder for memory
std::unique_ptr<IAudioDecoder> create_decoder(std::span<const std::byte> data);

// =============================================================================
// WAV File Support
// =============================================================================

/// WAV file decoder
class WavDecoder : public IAudioDecoder {
public:
    explicit WavDecoder(const std::filesystem::path& path);
    explicit WavDecoder(std::span<const std::byte> data);

    [[nodiscard]] AudioFileInfo info() const override { return m_info; }
    void_core::Result<BufferPtr> decode() override;
    std::size_t read(void* buffer, std::size_t bytes) override;
    bool seek(std::uint64_t sample) override;
    [[nodiscard]] std::uint64_t position() const override { return m_position; }

private:
    void_core::Result<void> parse_header();

    AudioFileInfo m_info;
    std::vector<std::byte> m_file_data;
    std::size_t m_data_offset = 0;
    std::uint64_t m_position = 0;
};

// =============================================================================
// Audio Buffer Pool
// =============================================================================

/// Pool for managing audio buffers
class AudioBufferPool {
public:
    explicit AudioBufferPool(std::size_t max_buffers = 256);

    /// Get or load a buffer by path
    void_core::Result<BufferPtr> get(const std::filesystem::path& path);

    /// Load a buffer
    void_core::Result<BufferId> load(const std::filesystem::path& path);

    /// Create a buffer from description
    BufferId create(const AudioBufferDesc& desc);

    /// Get buffer by ID
    BufferPtr get(BufferId id);

    /// Release a buffer
    void release(BufferId id);

    /// Clear all buffers
    void clear();

    /// Get number of loaded buffers
    [[nodiscard]] std::size_t count() const { return m_buffers.size(); }

    /// Get total memory usage
    [[nodiscard]] std::size_t memory_usage() const;

private:
    std::size_t m_max_buffers;
    std::unordered_map<BufferId, BufferPtr> m_buffers;
    std::unordered_map<std::string, BufferId> m_path_cache;
    std::uint32_t m_next_id = 1;
};

} // namespace void_audio
