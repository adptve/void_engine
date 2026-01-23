/// @file mixer.cpp
/// @brief Audio mixer implementations for void_audio

#include <void_engine/audio/mixer.hpp>

#include <algorithm>
#include <cmath>

namespace void_audio {

// =============================================================================
// AudioBus Implementation
// =============================================================================

AudioBus::AudioBus(const BusConfig& config)
    : m_name(config.name)
    , m_parent(config.parent)
    , m_volume(config.volume)
    , m_pan(config.pan)
    , m_muted(config.muted)
    , m_solo(config.solo)
    , m_effects(config.effects) {
}

void AudioBus::set_volume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioBus::set_pan(float pan) {
    m_pan = std::clamp(pan, -1.0f, 1.0f);
}

void AudioBus::update_effective_volume(float parent_volume) {
    m_effective_volume = m_volume * parent_volume;
}

void AudioBus::update_effective_mute(bool parent_muted, bool any_solo) {
    if (parent_muted) {
        m_effectively_muted = true;
    } else if (any_solo) {
        // If any bus has solo, only solo buses (and their children) play
        m_effectively_muted = !m_solo;
    } else {
        m_effectively_muted = m_muted;
    }
}

void AudioBus::add_effect(EffectId effect) {
    if (std::find(m_effects.begin(), m_effects.end(), effect) == m_effects.end()) {
        m_effects.push_back(effect);
    }
}

void AudioBus::remove_effect(EffectId effect) {
    auto it = std::find(m_effects.begin(), m_effects.end(), effect);
    if (it != m_effects.end()) {
        m_effects.erase(it);
    }
}

void AudioBus::move_effect(EffectId effect, std::size_t new_index) {
    auto it = std::find(m_effects.begin(), m_effects.end(), effect);
    if (it == m_effects.end()) return;

    std::size_t current_index = std::distance(m_effects.begin(), it);
    if (current_index == new_index) return;

    m_effects.erase(it);
    if (new_index >= m_effects.size()) {
        m_effects.push_back(effect);
    } else {
        m_effects.insert(m_effects.begin() + static_cast<std::ptrdiff_t>(new_index), effect);
    }
}

void AudioBus::add_child(BusId child) {
    if (std::find(m_children.begin(), m_children.end(), child) == m_children.end()) {
        m_children.push_back(child);
    }
}

void AudioBus::remove_child(BusId child) {
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

// =============================================================================
// AudioMixer Implementation
// =============================================================================

AudioMixer::AudioMixer() {
    // Create master bus
    BusConfig master_config;
    master_config.name = "Master";
    m_master_bus = create_bus(master_config);
}

AudioMixer::~AudioMixer() = default;

BusId AudioMixer::create_bus(const BusConfig& config) {
    BusId id{m_next_bus_id++};
    auto bus = std::make_unique<AudioBus>(config);
    bus->set_id(id);

    // Add to parent's children
    if (config.parent) {
        if (auto* parent = get_bus(config.parent)) {
            auto* parent_bus = static_cast<AudioBus*>(parent);
            parent_bus->add_child(id);
        }
    }

    if (!config.name.empty()) {
        m_bus_names[config.name] = id;
    }

    m_buses[id] = std::move(bus);
    return id;
}

BusId AudioMixer::create_bus(std::string_view name, BusId parent) {
    BusConfig config;
    config.name = std::string(name);
    config.parent = parent;
    return create_bus(config);
}

IAudioBus* AudioMixer::get_bus(BusId id) {
    auto it = m_buses.find(id);
    return it != m_buses.end() ? it->second.get() : nullptr;
}

const IAudioBus* AudioMixer::get_bus(BusId id) const {
    auto it = m_buses.find(id);
    return it != m_buses.end() ? it->second.get() : nullptr;
}

IAudioBus* AudioMixer::find_bus(std::string_view name) {
    auto it = m_bus_names.find(std::string(name));
    if (it != m_bus_names.end()) {
        return get_bus(it->second);
    }
    return nullptr;
}

const IAudioBus* AudioMixer::find_bus(std::string_view name) const {
    auto it = m_bus_names.find(std::string(name));
    if (it != m_bus_names.end()) {
        return get_bus(it->second);
    }
    return nullptr;
}

void AudioMixer::destroy_bus(BusId id) {
    if (id == m_master_bus) return; // Can't destroy master

    auto it = m_buses.find(id);
    if (it == m_buses.end()) return;

    // Remove from parent's children
    BusId parent_id = it->second->parent();
    if (parent_id) {
        if (auto* parent = get_bus(parent_id)) {
            static_cast<AudioBus*>(parent)->remove_child(id);
        }
    }

    // Remove from name map
    for (auto name_it = m_bus_names.begin(); name_it != m_bus_names.end(); ) {
        if (name_it->second == id) {
            name_it = m_bus_names.erase(name_it);
        } else {
            ++name_it;
        }
    }

    m_buses.erase(it);
}

IAudioBus* AudioMixer::master_bus() {
    return get_bus(m_master_bus);
}

const IAudioBus* AudioMixer::master_bus() const {
    return get_bus(m_master_bus);
}

void AudioMixer::set_master_volume(float volume) {
    if (auto* bus = master_bus()) {
        bus->set_volume(volume);
    }
}

float AudioMixer::master_volume() const {
    if (const auto* bus = master_bus()) {
        return bus->volume();
    }
    return 1.0f;
}

void AudioMixer::set_master_muted(bool muted) {
    if (auto* bus = master_bus()) {
        bus->set_muted(muted);
    }
}

bool AudioMixer::is_master_muted() const {
    if (const auto* bus = master_bus()) {
        return bus->is_muted();
    }
    return false;
}

AudioMixer::Snapshot AudioMixer::take_snapshot(std::string_view name) const {
    Snapshot snapshot;
    snapshot.name = std::string(name);

    for (const auto& [id, bus] : m_buses) {
        snapshot.volumes[id] = bus->volume();
        snapshot.mutes[id] = bus->is_muted();
    }

    return snapshot;
}

void AudioMixer::apply_snapshot(const Snapshot& snapshot, float blend_time) {
    if (blend_time <= 0) {
        // Immediate apply
        for (const auto& [id, volume] : snapshot.volumes) {
            if (auto* bus = get_bus(id)) {
                bus->set_volume(volume);
            }
        }
        for (const auto& [id, muted] : snapshot.mutes) {
            if (auto* bus = get_bus(id)) {
                bus->set_muted(muted);
            }
        }
    } else {
        // Start blend
        m_blend_from = take_snapshot("blend_from");
        m_blend_to = snapshot;
        m_blend_time = 0;
        m_blend_duration = blend_time;
        m_blending = true;
    }
}

void AudioMixer::blend_snapshots(const Snapshot& from, const Snapshot& to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    for (const auto& [id, to_volume] : to.volumes) {
        auto from_it = from.volumes.find(id);
        float from_volume = from_it != from.volumes.end() ? from_it->second : to_volume;

        float blended = from_volume + (to_volume - from_volume) * t;
        if (auto* bus = get_bus(id)) {
            bus->set_volume(blended);
        }
    }

    // Mutes snap at 50%
    for (const auto& [id, to_muted] : to.mutes) {
        auto from_it = from.mutes.find(id);
        bool from_muted = from_it != from.mutes.end() ? from_it->second : to_muted;

        bool muted = t < 0.5f ? from_muted : to_muted;
        if (auto* bus = get_bus(id)) {
            bus->set_muted(muted);
        }
    }
}

void AudioMixer::update() {
    // Handle snapshot blending
    if (m_blending) {
        m_blend_time += 1.0f / 60.0f; // Assume 60fps, should pass dt
        float t = m_blend_time / m_blend_duration;

        if (t >= 1.0f) {
            t = 1.0f;
            m_blending = false;
        }

        // Smooth interpolation
        t = t * t * (3.0f - 2.0f * t);
        blend_snapshots(m_blend_from, m_blend_to, t);
    }

    // Recalculate bus hierarchies
    recalculate_buses();
}

void AudioMixer::recalculate_buses() {
    bool any_solo = has_solo_bus();

    // Start from master and propagate down
    if (auto* master = static_cast<AudioBus*>(get_bus(m_master_bus))) {
        update_bus_recursive(master, 1.0f, false, any_solo);
    }
}

void AudioMixer::update_bus_recursive(AudioBus* bus, float parent_volume, bool parent_muted, bool any_solo) {
    bus->update_effective_volume(parent_volume);
    bus->update_effective_mute(parent_muted, any_solo);

    float my_volume = bus->is_effectively_muted() ? 0.0f : bus->effective_volume();
    bool my_muted = bus->is_effectively_muted();

    for (BusId child_id : bus->children()) {
        if (auto* child = static_cast<AudioBus*>(get_bus(child_id))) {
            update_bus_recursive(child, my_volume, my_muted, any_solo);
        }
    }
}

bool AudioMixer::has_solo_bus() const {
    for (const auto& [id, bus] : m_buses) {
        if (bus->is_solo()) return true;
    }
    return false;
}

void AudioMixer::create_default_buses() {
    // Create standard bus structure
    m_sfx_bus = create_bus("SFX", m_master_bus);
    m_music_bus = create_bus("Music", m_master_bus);
    m_voice_bus = create_bus("Voice", m_master_bus);
    m_ambient_bus = create_bus("Ambient", m_master_bus);

    // Sub-buses for SFX
    create_bus("UI", m_sfx_bus);
    create_bus("Weapons", m_sfx_bus);
    create_bus("Footsteps", m_sfx_bus);
}

// =============================================================================
// VoiceLimiter Implementation
// =============================================================================

VoiceLimiter::VoiceLimiter(std::uint32_t max_voices)
    : m_max_voices(max_voices) {
}

bool VoiceLimiter::request_voice(SourceId source, std::uint8_t priority, float audibility) {
    // Check if source already has a voice
    for (auto& voice : m_voices) {
        if (voice.source == source) {
            voice.priority = priority;
            voice.audibility = audibility;
            return voice.active;
        }
    }

    // Add new voice info
    VoiceInfo info;
    info.source = source;
    info.priority = priority;
    info.audibility = audibility;
    info.active = false;

    // Check if we have room
    if (m_active_count < m_max_voices) {
        info.active = true;
        m_active_count++;
    } else {
        // Need to potentially steal a voice
        // Find lowest priority/audibility voice
        VoiceInfo* worst = nullptr;
        for (auto& voice : m_voices) {
            if (voice.active) {
                if (!worst ||
                    voice.priority > worst->priority ||
                    (voice.priority == worst->priority && voice.audibility < worst->audibility)) {
                    worst = &voice;
                }
            }
        }

        if (worst && (priority < worst->priority ||
                      (priority == worst->priority && audibility > worst->audibility))) {
            // Steal the voice
            worst->active = false;
            info.active = true;
        } else {
            m_virtual_count++;
        }
    }

    m_voices.push_back(info);
    return info.active;
}

void VoiceLimiter::release_voice(SourceId source) {
    auto it = std::find_if(m_voices.begin(), m_voices.end(),
        [source](const VoiceInfo& v) { return v.source == source; });

    if (it != m_voices.end()) {
        if (it->active) {
            m_active_count--;
        } else {
            m_virtual_count--;
        }
        m_voices.erase(it);
    }
}

void VoiceLimiter::update(const std::function<float(SourceId)>& get_audibility) {
    // Update audibility values
    for (auto& voice : m_voices) {
        voice.audibility = get_audibility(voice.source);
    }

    // Sort by priority and audibility
    std::sort(m_voices.begin(), m_voices.end(),
        [](const VoiceInfo& a, const VoiceInfo& b) {
            if (a.priority != b.priority) return a.priority < b.priority;
            return a.audibility > b.audibility;
        });

    // Assign voices to top entries
    m_active_count = 0;
    m_virtual_count = 0;

    for (auto& voice : m_voices) {
        if (m_active_count < m_max_voices) {
            voice.active = true;
            m_active_count++;
        } else {
            voice.active = false;
            m_virtual_count++;
        }
    }
}

bool VoiceLimiter::has_voice(SourceId source) const {
    auto it = std::find_if(m_voices.begin(), m_voices.end(),
        [source](const VoiceInfo& v) { return v.source == source; });
    return it != m_voices.end() && it->active;
}

void VoiceLimiter::set_max_voices(std::uint32_t max) {
    m_max_voices = max;
}

// =============================================================================
// AudioDucker Implementation
// =============================================================================

void AudioDucker::add_duck(const DuckConfig& config) {
    DuckState state;
    state.config = config;
    state.current_duck = 1.0f;
    m_ducks.push_back(state);
}

void AudioDucker::remove_duck(BusId trigger, BusId target) {
    auto it = std::remove_if(m_ducks.begin(), m_ducks.end(),
        [trigger, target](const DuckState& s) {
            return s.config.trigger_bus == trigger && s.config.target_bus == target;
        });
    m_ducks.erase(it, m_ducks.end());
}

void AudioDucker::update(float dt, const std::function<float(BusId)>& get_bus_level) {
    m_duck_amounts.clear();

    for (auto& duck : m_ducks) {
        float trigger_level = get_bus_level(duck.config.trigger_bus);

        // Convert to dB
        float trigger_db = 20.0f * std::log10(std::max(trigger_level, 1e-6f));

        bool should_duck = trigger_db > duck.config.threshold;
        float target_duck = should_duck ? (1.0f - duck.config.duck_amount) : 1.0f;

        // Smooth transition
        float rate = should_duck ?
            (1.0f / duck.config.attack) :
            (1.0f / duck.config.release);

        if (duck.current_duck < target_duck) {
            duck.current_duck = std::min(duck.current_duck + rate * dt, target_duck);
        } else {
            duck.current_duck = std::max(duck.current_duck - rate * dt, target_duck);
        }

        // Accumulate duck amounts (multiply if multiple ducks affect same bus)
        auto it = m_duck_amounts.find(duck.config.target_bus);
        if (it != m_duck_amounts.end()) {
            it->second *= duck.current_duck;
        } else {
            m_duck_amounts[duck.config.target_bus] = duck.current_duck;
        }
    }
}

float AudioDucker::get_duck_amount(BusId bus) const {
    auto it = m_duck_amounts.find(bus);
    return it != m_duck_amounts.end() ? it->second : 1.0f;
}

} // namespace void_audio
