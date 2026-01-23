/// @file mixer.hpp
/// @brief Audio mixing and bus system for void_audio

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_audio {

// =============================================================================
// Audio Bus Interface
// =============================================================================

/// Interface for audio bus (mixing group)
class IAudioBus {
public:
    virtual ~IAudioBus() = default;

    /// Get bus ID
    [[nodiscard]] virtual BusId id() const = 0;

    /// Get bus name
    [[nodiscard]] virtual const std::string& name() const = 0;

    /// Get parent bus
    [[nodiscard]] virtual BusId parent() const = 0;

    // =========================================================================
    // Volume Control
    // =========================================================================

    /// Get volume (0 to 1)
    [[nodiscard]] virtual float volume() const = 0;

    /// Set volume
    virtual void set_volume(float volume) = 0;

    /// Get effective volume (including parent attenuation)
    [[nodiscard]] virtual float effective_volume() const = 0;

    /// Get pan (-1 left to 1 right)
    [[nodiscard]] virtual float pan() const = 0;

    /// Set pan
    virtual void set_pan(float pan) = 0;

    // =========================================================================
    // Mute/Solo
    // =========================================================================

    /// Check if muted
    [[nodiscard]] virtual bool is_muted() const = 0;

    /// Set muted
    virtual void set_muted(bool muted) = 0;

    /// Check if solo
    [[nodiscard]] virtual bool is_solo() const = 0;

    /// Set solo
    virtual void set_solo(bool solo) = 0;

    /// Check if effectively muted (own mute or parent mute)
    [[nodiscard]] virtual bool is_effectively_muted() const = 0;

    // =========================================================================
    // Effects Chain
    // =========================================================================

    /// Get effect chain
    [[nodiscard]] virtual const std::vector<EffectId>& effects() const = 0;

    /// Add effect to chain
    virtual void add_effect(EffectId effect) = 0;

    /// Remove effect from chain
    virtual void remove_effect(EffectId effect) = 0;

    /// Clear all effects
    virtual void clear_effects() = 0;

    /// Reorder effect in chain
    virtual void move_effect(EffectId effect, std::size_t new_index) = 0;

    // =========================================================================
    // Children
    // =========================================================================

    /// Get child buses
    [[nodiscard]] virtual const std::vector<BusId>& children() const = 0;

    // =========================================================================
    // Native Handle
    // =========================================================================

    /// Get native handle (backend-specific)
    [[nodiscard]] virtual void* native_handle() const = 0;
};

// =============================================================================
// Audio Bus Implementation
// =============================================================================

/// Standard audio bus implementation
class AudioBus : public IAudioBus {
public:
    AudioBus() = default;
    explicit AudioBus(const BusConfig& config);

    // Identity
    [[nodiscard]] BusId id() const override { return m_id; }
    [[nodiscard]] const std::string& name() const override { return m_name; }
    [[nodiscard]] BusId parent() const override { return m_parent; }

    /// Set bus ID
    void set_id(BusId id) { m_id = id; }

    /// Set parent bus
    void set_parent(BusId parent) { m_parent = parent; }

    // Volume
    [[nodiscard]] float volume() const override { return m_volume; }
    void set_volume(float volume) override;
    [[nodiscard]] float effective_volume() const override { return m_effective_volume; }
    [[nodiscard]] float pan() const override { return m_pan; }
    void set_pan(float pan) override;

    /// Update effective volume (called by mixer)
    void update_effective_volume(float parent_volume);

    // Mute/Solo
    [[nodiscard]] bool is_muted() const override { return m_muted; }
    void set_muted(bool muted) override { m_muted = muted; }
    [[nodiscard]] bool is_solo() const override { return m_solo; }
    void set_solo(bool solo) override { m_solo = solo; }
    [[nodiscard]] bool is_effectively_muted() const override { return m_effectively_muted; }

    /// Update effective mute state
    void update_effective_mute(bool parent_muted, bool any_solo);

    // Effects
    [[nodiscard]] const std::vector<EffectId>& effects() const override { return m_effects; }
    void add_effect(EffectId effect) override;
    void remove_effect(EffectId effect) override;
    void clear_effects() override { m_effects.clear(); }
    void move_effect(EffectId effect, std::size_t new_index) override;

    // Children
    [[nodiscard]] const std::vector<BusId>& children() const override { return m_children; }

    /// Add child bus
    void add_child(BusId child);

    /// Remove child bus
    void remove_child(BusId child);

    // Native handle
    [[nodiscard]] void* native_handle() const override { return m_native_handle; }
    void set_native_handle(void* handle) { m_native_handle = handle; }

private:
    BusId m_id{0};
    std::string m_name;
    BusId m_parent{0};

    float m_volume = 1.0f;
    float m_effective_volume = 1.0f;
    float m_pan = 0.0f;
    bool m_muted = false;
    bool m_solo = false;
    bool m_effectively_muted = false;

    std::vector<EffectId> m_effects;
    std::vector<BusId> m_children;

    void* m_native_handle = nullptr;
};

// =============================================================================
// Audio Mixer
// =============================================================================

/// Audio mixer managing buses and routing
class AudioMixer {
public:
    AudioMixer();
    ~AudioMixer();

    // =========================================================================
    // Bus Management
    // =========================================================================

    /// Create a new bus
    BusId create_bus(const BusConfig& config);

    /// Create a new bus with name
    BusId create_bus(std::string_view name, BusId parent = BusId{0});

    /// Get bus by ID
    [[nodiscard]] IAudioBus* get_bus(BusId id);
    [[nodiscard]] const IAudioBus* get_bus(BusId id) const;

    /// Get bus by name
    [[nodiscard]] IAudioBus* find_bus(std::string_view name);
    [[nodiscard]] const IAudioBus* find_bus(std::string_view name) const;

    /// Destroy a bus
    void destroy_bus(BusId id);

    /// Get master bus
    [[nodiscard]] IAudioBus* master_bus();
    [[nodiscard]] const IAudioBus* master_bus() const;

    /// Get master bus ID
    [[nodiscard]] BusId master_bus_id() const { return m_master_bus; }

    /// Get all buses
    [[nodiscard]] const std::unordered_map<BusId, std::unique_ptr<AudioBus>>& buses() const {
        return m_buses;
    }

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    /// Set master volume
    void set_master_volume(float volume);

    /// Get master volume
    [[nodiscard]] float master_volume() const;

    /// Mute master
    void set_master_muted(bool muted);

    /// Check if master is muted
    [[nodiscard]] bool is_master_muted() const;

    // =========================================================================
    // Snapshots
    // =========================================================================

    /// Snapshot of mixer state
    struct Snapshot {
        std::string name;
        std::unordered_map<BusId, float> volumes;
        std::unordered_map<BusId, bool> mutes;
    };

    /// Take a snapshot
    [[nodiscard]] Snapshot take_snapshot(std::string_view name = "") const;

    /// Apply a snapshot
    void apply_snapshot(const Snapshot& snapshot, float blend_time = 0.0f);

    /// Blend between snapshots
    void blend_snapshots(const Snapshot& from, const Snapshot& to, float t);

    // =========================================================================
    // Update
    // =========================================================================

    /// Update mixer state
    void update();

    /// Recalculate bus hierarchies
    void recalculate_buses();

    // =========================================================================
    // Presets
    // =========================================================================

    /// Create common bus structure (Master, Music, SFX, Voice, Ambient)
    void create_default_buses();

    /// Get SFX bus (if created with defaults)
    [[nodiscard]] BusId sfx_bus() const { return m_sfx_bus; }

    /// Get Music bus (if created with defaults)
    [[nodiscard]] BusId music_bus() const { return m_music_bus; }

    /// Get Voice bus (if created with defaults)
    [[nodiscard]] BusId voice_bus() const { return m_voice_bus; }

    /// Get Ambient bus (if created with defaults)
    [[nodiscard]] BusId ambient_bus() const { return m_ambient_bus; }

private:
    void update_bus_recursive(AudioBus* bus, float parent_volume, bool parent_muted, bool any_solo);
    bool has_solo_bus() const;

    std::unordered_map<BusId, std::unique_ptr<AudioBus>> m_buses;
    std::unordered_map<std::string, BusId> m_bus_names;
    std::uint32_t m_next_bus_id = 1;

    BusId m_master_bus{0};
    BusId m_sfx_bus{0};
    BusId m_music_bus{0};
    BusId m_voice_bus{0};
    BusId m_ambient_bus{0};

    // Snapshot blending
    bool m_blending = false;
    Snapshot m_blend_from;
    Snapshot m_blend_to;
    float m_blend_time = 0;
    float m_blend_duration = 0;
};

// =============================================================================
// Voice Limiter
// =============================================================================

/// Manages voice limiting and prioritization
class VoiceLimiter {
public:
    explicit VoiceLimiter(std::uint32_t max_voices = 64);

    /// Request a voice for a source
    /// Returns true if voice was granted, false if source should be virtualized
    bool request_voice(SourceId source, std::uint8_t priority, float audibility);

    /// Release a voice
    void release_voice(SourceId source);

    /// Update voice assignments based on audibility
    void update(const std::function<float(SourceId)>& get_audibility);

    /// Get number of active voices
    [[nodiscard]] std::uint32_t active_voice_count() const { return m_active_count; }

    /// Get number of virtual voices
    [[nodiscard]] std::uint32_t virtual_voice_count() const { return m_virtual_count; }

    /// Check if source has an active voice
    [[nodiscard]] bool has_voice(SourceId source) const;

    /// Set maximum voices
    void set_max_voices(std::uint32_t max);

    /// Get maximum voices
    [[nodiscard]] std::uint32_t max_voices() const { return m_max_voices; }

private:
    struct VoiceInfo {
        SourceId source;
        std::uint8_t priority;
        float audibility;
        bool active;
    };

    std::vector<VoiceInfo> m_voices;
    std::uint32_t m_max_voices;
    std::uint32_t m_active_count = 0;
    std::uint32_t m_virtual_count = 0;
};

// =============================================================================
// Duck/Sidechain
// =============================================================================

/// Ducking configuration
struct DuckConfig {
    BusId trigger_bus;          ///< Bus that triggers ducking
    BusId target_bus;           ///< Bus to duck
    float threshold = -20.0f;   ///< Trigger threshold in dB
    float duck_amount = 0.5f;   ///< Amount to duck (0-1)
    float attack = 0.01f;       ///< Attack time in seconds
    float release = 0.1f;       ///< Release time in seconds
};

/// Manages audio ducking (e.g., duck music when voice plays)
class AudioDucker {
public:
    /// Add a duck relationship
    void add_duck(const DuckConfig& config);

    /// Remove a duck relationship
    void remove_duck(BusId trigger, BusId target);

    /// Update ducking (call each frame)
    void update(float dt, const std::function<float(BusId)>& get_bus_level);

    /// Get current duck amount for a bus (0-1, where 1 = no duck)
    [[nodiscard]] float get_duck_amount(BusId bus) const;

private:
    struct DuckState {
        DuckConfig config;
        float current_duck = 1.0f;  // 1 = no duck
    };

    std::vector<DuckState> m_ducks;
    std::unordered_map<BusId, float> m_duck_amounts;
};

} // namespace void_audio
