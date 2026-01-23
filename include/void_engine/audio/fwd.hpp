/// @file fwd.hpp
/// @brief Forward declarations for void_audio

#pragma once

#include <cstdint>
#include <memory>

namespace void_audio {

// =============================================================================
// Handle Types
// =============================================================================

/// Unique identifier for audio buffers
struct BufferId {
    std::uint32_t value = 0;
    bool operator==(const BufferId& other) const { return value == other.value; }
    bool operator!=(const BufferId& other) const { return value != other.value; }
    bool operator<(const BufferId& other) const { return value < other.value; }
    explicit operator bool() const { return value != 0; }
};

/// Unique identifier for audio sources
struct SourceId {
    std::uint32_t value = 0;
    bool operator==(const SourceId& other) const { return value == other.value; }
    bool operator!=(const SourceId& other) const { return value != other.value; }
    bool operator<(const SourceId& other) const { return value < other.value; }
    explicit operator bool() const { return value != 0; }
};

/// Unique identifier for audio buses
struct BusId {
    std::uint32_t value = 0;
    bool operator==(const BusId& other) const { return value == other.value; }
    bool operator!=(const BusId& other) const { return value != other.value; }
    bool operator<(const BusId& other) const { return value < other.value; }
    explicit operator bool() const { return value != 0; }
};

/// Unique identifier for effects
struct EffectId {
    std::uint32_t value = 0;
    bool operator==(const EffectId& other) const { return value == other.value; }
    bool operator!=(const EffectId& other) const { return value != other.value; }
    bool operator<(const EffectId& other) const { return value < other.value; }
    explicit operator bool() const { return value != 0; }
};

// =============================================================================
// Enumerations
// =============================================================================

enum class AudioBackend : std::uint8_t;
enum class AudioFormat : std::uint8_t;
enum class AudioState : std::uint8_t;
enum class SpatializationMode : std::uint8_t;
enum class AttenuationModel : std::uint8_t;
enum class EffectType : std::uint8_t;

// =============================================================================
// Core Types
// =============================================================================

struct AudioConfig;
struct AudioStats;
struct AudioBufferDesc;
struct AudioSourceConfig;
struct ListenerConfig;
struct BusConfig;
struct EffectConfig;

// =============================================================================
// Classes
// =============================================================================

// Buffer
class IAudioBuffer;
class AudioBuffer;
class StreamingBuffer;

// Source
class IAudioSource;
class AudioSource;
class AudioSource3D;

// Listener
class IAudioListener;
class AudioListener;

// Mixer
class IAudioBus;
class AudioBus;
class AudioMixer;

// Effects
class IAudioEffect;
class ReverbEffect;
class DelayEffect;
class FilterEffect;
class CompressorEffect;
class DistortionEffect;
class ChorusEffect;
class EQEffect;

// Backend
class IAudioBackend;
class NullAudioBackend;
class OpenALBackend;
class AudioBackendFactory;

// System
class AudioSystem;

// =============================================================================
// Smart Pointers
// =============================================================================

using BufferPtr = std::shared_ptr<IAudioBuffer>;
using SourcePtr = std::shared_ptr<IAudioSource>;
using ListenerPtr = std::shared_ptr<IAudioListener>;
using BusPtr = std::shared_ptr<IAudioBus>;
using EffectPtr = std::shared_ptr<IAudioEffect>;
using BackendPtr = std::unique_ptr<IAudioBackend>;

} // namespace void_audio

// Hash specializations for use in unordered containers
namespace std {
    template<> struct hash<void_audio::BufferId> {
        std::size_t operator()(const void_audio::BufferId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };

    template<> struct hash<void_audio::SourceId> {
        std::size_t operator()(const void_audio::SourceId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };

    template<> struct hash<void_audio::BusId> {
        std::size_t operator()(const void_audio::BusId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };

    template<> struct hash<void_audio::EffectId> {
        std::size_t operator()(const void_audio::EffectId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
}
