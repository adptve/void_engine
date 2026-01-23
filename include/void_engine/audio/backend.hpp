/// @file backend.hpp
/// @brief Audio backend abstraction for void_audio

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "buffer.hpp"
#include "source.hpp"
#include "listener.hpp"
#include "mixer.hpp"
#include "effects.hpp"

#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_audio {

// =============================================================================
// Backend Capabilities
// =============================================================================

/// Backend capability flags
enum class AudioCapability : std::uint32_t {
    None                = 0,
    Playback            = 1 << 0,
    Recording           = 1 << 1,
    Streaming           = 1 << 2,
    Spatialization3D    = 1 << 3,
    HRTF                = 1 << 4,
    Effects             = 1 << 5,
    Reverb              = 1 << 6,
    EQ                  = 1 << 7,
    Compression         = 1 << 8,
    LowLatency          = 1 << 9,
    Multithreaded       = 1 << 10,
    HotReload           = 1 << 11,

    Standard = Playback | Streaming | Spatialization3D | Effects,
    Full = 0xFFFFFFFF
};

inline AudioCapability operator|(AudioCapability a, AudioCapability b) {
    return static_cast<AudioCapability>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline AudioCapability operator&(AudioCapability a, AudioCapability b) {
    return static_cast<AudioCapability>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline bool has_capability(AudioCapability caps, AudioCapability check) {
    return (caps & check) == check;
}

// =============================================================================
// Backend Information
// =============================================================================

/// Information about an audio backend
struct AudioBackendInfo {
    AudioBackend type = AudioBackend::Null;
    std::string name;
    std::string version;
    std::string vendor;
    AudioCapability capabilities = AudioCapability::None;

    struct Limits {
        std::uint32_t max_sources = 256;
        std::uint32_t max_buffers = 1024;
        std::uint32_t max_effects = 64;
        std::uint32_t max_buses = 32;
        std::uint32_t max_sample_rate = 192000;
    } limits;

    struct Performance {
        float latency_ms = 10.0f;
        bool hardware_accelerated = false;
        bool simd_optimized = false;
    } performance;
};

// =============================================================================
// Audio Backend Interface
// =============================================================================

/// Audio backend interface
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    /// Get backend information
    [[nodiscard]] virtual AudioBackendInfo info() const = 0;

    /// Get backend type
    [[nodiscard]] virtual AudioBackend type() const = 0;

    /// Initialize the backend
    [[nodiscard]] virtual void_core::Result<void> initialize(const AudioConfig& config) = 0;

    /// Shutdown the backend
    virtual void shutdown() = 0;

    /// Check if initialized
    [[nodiscard]] virtual bool is_initialized() const = 0;

    // =========================================================================
    // Buffer Management
    // =========================================================================

    /// Create a buffer
    [[nodiscard]] virtual void_core::Result<BufferId> create_buffer(const AudioBufferDesc& desc) = 0;

    /// Destroy a buffer
    virtual void destroy_buffer(BufferId id) = 0;

    /// Get buffer
    [[nodiscard]] virtual IAudioBuffer* get_buffer(BufferId id) = 0;

    // =========================================================================
    // Source Management
    // =========================================================================

    /// Create a source
    [[nodiscard]] virtual void_core::Result<SourceId> create_source(const AudioSourceConfig& config) = 0;

    /// Destroy a source
    virtual void destroy_source(SourceId id) = 0;

    /// Get source
    [[nodiscard]] virtual IAudioSource* get_source(SourceId id) = 0;

    // =========================================================================
    // Listener
    // =========================================================================

    /// Get the listener
    [[nodiscard]] virtual IAudioListener* listener() = 0;

    // =========================================================================
    // Effects
    // =========================================================================

    /// Create an effect
    [[nodiscard]] virtual void_core::Result<EffectId> create_effect(const EffectConfig& config) = 0;

    /// Destroy an effect
    virtual void destroy_effect(EffectId id) = 0;

    /// Get effect
    [[nodiscard]] virtual IAudioEffect* get_effect(EffectId id) = 0;

    // =========================================================================
    // Update
    // =========================================================================

    /// Update audio system (call each frame)
    virtual void update(float dt) = 0;

    /// Process audio (for software mixing)
    virtual void process() = 0;

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get statistics
    [[nodiscard]] virtual AudioStats stats() const = 0;

    /// Reset statistics
    virtual void reset_stats() = 0;

    // =========================================================================
    // Capability Check
    // =========================================================================

    /// Check if capability is supported
    [[nodiscard]] virtual bool supports(AudioCapability cap) const {
        return has_capability(info().capabilities, cap);
    }
};

// =============================================================================
// Null Audio Backend
// =============================================================================

/// Null audio backend (silent, for testing)
class NullAudioBackend : public IAudioBackend {
public:
    [[nodiscard]] AudioBackendInfo info() const override;
    [[nodiscard]] AudioBackend type() const override { return AudioBackend::Null; }
    [[nodiscard]] void_core::Result<void> initialize(const AudioConfig& config) override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const override { return m_initialized; }

    [[nodiscard]] void_core::Result<BufferId> create_buffer(const AudioBufferDesc& desc) override;
    void destroy_buffer(BufferId id) override;
    [[nodiscard]] IAudioBuffer* get_buffer(BufferId id) override;

    [[nodiscard]] void_core::Result<SourceId> create_source(const AudioSourceConfig& config) override;
    void destroy_source(SourceId id) override;
    [[nodiscard]] IAudioSource* get_source(SourceId id) override;

    [[nodiscard]] IAudioListener* listener() override { return &m_listener; }

    [[nodiscard]] void_core::Result<EffectId> create_effect(const EffectConfig& config) override;
    void destroy_effect(EffectId id) override;
    [[nodiscard]] IAudioEffect* get_effect(EffectId id) override;

    void update(float dt) override;
    void process() override {}

    [[nodiscard]] AudioStats stats() const override { return m_stats; }
    void reset_stats() override { m_stats = {}; }

private:
    bool m_initialized = false;
    AudioConfig m_config;
    AudioStats m_stats;
    AudioListener m_listener;

    std::unordered_map<BufferId, std::unique_ptr<AudioBuffer>> m_buffers;
    std::unordered_map<SourceId, std::unique_ptr<AudioSource>> m_sources;
    std::unordered_map<EffectId, EffectPtr> m_effects;

    std::uint32_t m_next_buffer_id = 1;
    std::uint32_t m_next_source_id = 1;
    std::uint32_t m_next_effect_id = 1;
};

// =============================================================================
// OpenAL Backend
// =============================================================================

/// OpenAL audio backend
class OpenALBackend : public IAudioBackend {
public:
    [[nodiscard]] AudioBackendInfo info() const override;
    [[nodiscard]] AudioBackend type() const override { return AudioBackend::OpenAL; }
    [[nodiscard]] void_core::Result<void> initialize(const AudioConfig& config) override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const override;

    [[nodiscard]] void_core::Result<BufferId> create_buffer(const AudioBufferDesc& desc) override;
    void destroy_buffer(BufferId id) override;
    [[nodiscard]] IAudioBuffer* get_buffer(BufferId id) override;

    [[nodiscard]] void_core::Result<SourceId> create_source(const AudioSourceConfig& config) override;
    void destroy_source(SourceId id) override;
    [[nodiscard]] IAudioSource* get_source(SourceId id) override;

    [[nodiscard]] IAudioListener* listener() override;

    [[nodiscard]] void_core::Result<EffectId> create_effect(const EffectConfig& config) override;
    void destroy_effect(EffectId id) override;
    [[nodiscard]] IAudioEffect* get_effect(EffectId id) override;

    void update(float dt) override;
    void process() override;

    [[nodiscard]] AudioStats stats() const override;
    void reset_stats() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// Audio Backend Factory
// =============================================================================

/// Factory for creating audio backends
class AudioBackendFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<IAudioBackend>()>;

    /// Get singleton instance
    [[nodiscard]] static AudioBackendFactory& instance();

    /// Register a backend creator
    void register_backend(AudioBackend type, CreatorFunc creator);

    /// Unregister a backend
    void unregister_backend(AudioBackend type);

    /// Check if backend is available
    [[nodiscard]] bool is_available(AudioBackend type) const;

    /// Get available backends
    [[nodiscard]] std::vector<AudioBackend> available_backends() const;

    /// Create backend instance
    [[nodiscard]] std::unique_ptr<IAudioBackend> create(AudioBackend type) const;

    /// Create best available backend
    [[nodiscard]] std::unique_ptr<IAudioBackend> create_best() const;

    /// Register built-in backends
    void register_builtins();

private:
    AudioBackendFactory() = default;

    struct RegisteredBackend {
        CreatorFunc creator;
        AudioBackendInfo info;
    };

    std::unordered_map<AudioBackend, RegisteredBackend> m_backends;
};

// =============================================================================
// Audio System
// =============================================================================

/// High-level audio system
class AudioSystem {
public:
    /// Create audio system with specified backend
    explicit AudioSystem(AudioBackend backend = AudioBackend::OpenAL);
    ~AudioSystem();

    // Non-copyable
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    /// Initialize the system
    [[nodiscard]] void_core::Result<void> initialize(const AudioConfig& config);

    /// Shutdown the system
    void shutdown();

    /// Check if initialized
    [[nodiscard]] bool is_initialized() const { return m_initialized; }

    // =========================================================================
    // Backend Access
    // =========================================================================

    /// Get backend
    [[nodiscard]] IAudioBackend* backend() { return m_backend.get(); }
    [[nodiscard]] const IAudioBackend* backend() const { return m_backend.get(); }

    // =========================================================================
    // Buffer Management
    // =========================================================================

    /// Load buffer from file
    [[nodiscard]] void_core::Result<BufferId> load_buffer(const std::filesystem::path& path);

    /// Create buffer from description
    [[nodiscard]] void_core::Result<BufferId> create_buffer(const AudioBufferDesc& desc);

    /// Get buffer
    [[nodiscard]] IAudioBuffer* get_buffer(BufferId id);

    /// Destroy buffer
    void destroy_buffer(BufferId id);

    // =========================================================================
    // Source Management
    // =========================================================================

    /// Create source
    [[nodiscard]] void_core::Result<SourceId> create_source(const AudioSourceConfig& config);

    /// Create source with builder
    [[nodiscard]] void_core::Result<SourceId> create_source(const AudioSourceBuilder& builder);

    /// Get source
    [[nodiscard]] IAudioSource* get_source(SourceId id);

    /// Destroy source
    void destroy_source(SourceId id);

    /// Play source
    void play(SourceId id);

    /// Pause source
    void pause(SourceId id);

    /// Stop source
    void stop(SourceId id);

    /// Stop all sources
    void stop_all();

    // =========================================================================
    // Listener
    // =========================================================================

    /// Get listener
    [[nodiscard]] IAudioListener* listener();

    // =========================================================================
    // Mixer
    // =========================================================================

    /// Get mixer
    [[nodiscard]] AudioMixer& mixer() { return m_mixer; }
    [[nodiscard]] const AudioMixer& mixer() const { return m_mixer; }

    // =========================================================================
    // One-Shot Playback
    // =========================================================================

    /// Play a one-shot sound
    OneShotHandle play_one_shot(BufferId buffer, float volume = 1.0f);

    /// Play a one-shot sound at position
    OneShotHandle play_one_shot_3d(BufferId buffer, const void_math::Vec3& position, float volume = 1.0f);

    /// Get one-shot player
    [[nodiscard]] OneShotPlayer& one_shot() { return m_one_shot; }

    // =========================================================================
    // Music
    // =========================================================================

    /// Play music
    void play_music(BufferId buffer, const MusicConfig& config = MusicConfig{});

    /// Stop music
    void stop_music(float fade_time = 0.0f);

    /// Pause music
    void pause_music();

    /// Resume music
    void resume_music();

    /// Cross-fade to new music
    void crossfade_music(BufferId buffer, float fade_time = 1.0f);

    /// Get current music source
    [[nodiscard]] SourceId current_music() const { return m_current_music; }

    /// Get music volume
    [[nodiscard]] float music_volume() const;

    /// Set music volume
    void set_music_volume(float volume);

    // =========================================================================
    // Update
    // =========================================================================

    /// Update audio system (call each frame)
    void update(float dt);

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get statistics
    [[nodiscard]] AudioStats stats() const;

    // =========================================================================
    // Hot Reload
    // =========================================================================

    /// Take snapshot for hot reload
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() const;

    /// Restore from snapshot
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot);

private:
    std::unique_ptr<IAudioBackend> m_backend;
    AudioConfig m_config;
    bool m_initialized = false;

    AudioMixer m_mixer;
    OneShotPlayer m_one_shot;

    // Music playback
    SourceId m_current_music{0};
    SourceId m_next_music{0};
    MusicConfig m_music_config;
    bool m_music_crossfading = false;
    float m_music_fade_time = 0;
};

} // namespace void_audio
