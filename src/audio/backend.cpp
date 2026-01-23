/// @file backend.cpp
/// @brief Audio backend implementations for void_audio

#include <void_engine/audio/backend.hpp>

#include <cstring>

namespace void_audio {

// =============================================================================
// NullAudioBackend Implementation
// =============================================================================

AudioBackendInfo NullAudioBackend::info() const {
    AudioBackendInfo info;
    info.type = AudioBackend::Null;
    info.name = "Null Audio";
    info.version = "1.0.0";
    info.vendor = "void_engine";
    info.capabilities = AudioCapability::Playback |
                        AudioCapability::Streaming |
                        AudioCapability::Spatialization3D |
                        AudioCapability::Effects;

    info.limits.max_sources = 256;
    info.limits.max_buffers = 1024;
    info.limits.max_effects = 64;

    info.performance.latency_ms = 0;
    info.performance.hardware_accelerated = false;

    return info;
}

void_core::Result<void> NullAudioBackend::initialize(const AudioConfig& config) {
    m_config = config;
    m_initialized = true;
    return {};
}

void NullAudioBackend::shutdown() {
    m_buffers.clear();
    m_sources.clear();
    m_effects.clear();
    m_initialized = false;
}

void_core::Result<BufferId> NullAudioBackend::create_buffer(const AudioBufferDesc& desc) {
    BufferId id{m_next_buffer_id++};

    auto buffer = std::make_unique<AudioBuffer>(desc);
    buffer->set_id(id);

    m_buffers[id] = std::move(buffer);
    m_stats.loaded_buffers++;

    return id;
}

void NullAudioBackend::destroy_buffer(BufferId id) {
    if (m_buffers.erase(id) > 0) {
        m_stats.loaded_buffers--;
    }
}

IAudioBuffer* NullAudioBackend::get_buffer(BufferId id) {
    auto it = m_buffers.find(id);
    return it != m_buffers.end() ? it->second.get() : nullptr;
}

void_core::Result<SourceId> NullAudioBackend::create_source(const AudioSourceConfig& config) {
    SourceId id{m_next_source_id++};

    auto source = std::make_unique<AudioSource>(config);
    source->set_id(id);

    // Get duration from buffer
    if (auto* buffer = get_buffer(config.buffer)) {
        source->set_duration(buffer->duration());
    }

    m_sources[id] = std::move(source);

    if (config.play_on_create) {
        m_sources[id]->play();
    }

    return id;
}

void NullAudioBackend::destroy_source(SourceId id) {
    m_sources.erase(id);
}

IAudioSource* NullAudioBackend::get_source(SourceId id) {
    auto it = m_sources.find(id);
    return it != m_sources.end() ? it->second.get() : nullptr;
}

void_core::Result<EffectId> NullAudioBackend::create_effect(const EffectConfig& config) {
    EffectId id{m_next_effect_id++};

    auto effect = AudioEffectFactory::create(config.type);
    if (!effect) {
        return void_core::Error{void_core::ErrorCode::NotSupported, "Effect type not supported"};
    }

    auto* effect_base = static_cast<AudioEffectBase*>(effect.get());
    effect_base->set_id(id);

    m_effects[id] = std::move(effect);

    return id;
}

void NullAudioBackend::destroy_effect(EffectId id) {
    m_effects.erase(id);
}

IAudioEffect* NullAudioBackend::get_effect(EffectId id) {
    auto it = m_effects.find(id);
    return it != m_effects.end() ? it->second.get() : nullptr;
}

void NullAudioBackend::update(float dt) {
    // Update all playing sources
    m_stats.active_sources = 0;

    for (auto& [id, source] : m_sources) {
        if (source && source->is_playing()) {
            source->update(dt);
            m_stats.active_sources++;
        }
    }
}

// =============================================================================
// OpenALBackend Implementation
// =============================================================================

struct OpenALBackend::Impl {
    bool initialized = false;
    AudioConfig config;
    AudioStats stats;
    AudioListener listener;

    std::unordered_map<BufferId, std::unique_ptr<AudioBuffer>> buffers;
    std::unordered_map<SourceId, std::unique_ptr<AudioSource>> sources;
    std::unordered_map<EffectId, EffectPtr> effects;

    std::uint32_t next_buffer_id = 1;
    std::uint32_t next_source_id = 1;
    std::uint32_t next_effect_id = 1;

    // OpenAL handles would go here
    // ALCdevice* device = nullptr;
    // ALCcontext* context = nullptr;
};

AudioBackendInfo OpenALBackend::info() const {
    AudioBackendInfo info;
    info.type = AudioBackend::OpenAL;
    info.name = "OpenAL Soft";
    info.version = "1.23.1";
    info.vendor = "OpenAL";
    info.capabilities = AudioCapability::Standard |
                        AudioCapability::HRTF |
                        AudioCapability::Effects |
                        AudioCapability::Reverb |
                        AudioCapability::EQ;

    info.limits.max_sources = 256;
    info.limits.max_buffers = 4096;
    info.limits.max_effects = 128;
    info.limits.max_sample_rate = 192000;

    info.performance.latency_ms = 20.0f;
    info.performance.hardware_accelerated = true;
    info.performance.simd_optimized = true;

    return info;
}

void_core::Result<void> OpenALBackend::initialize(const AudioConfig& config) {
    m_impl = std::make_unique<Impl>();
    m_impl->config = config;

    // In a real implementation:
    /*
    // Open device
    m_impl->device = alcOpenDevice(nullptr);
    if (!m_impl->device) {
        return void_core::Error{void_core::ErrorCode::InitializationFailed, "Failed to open audio device"};
    }

    // Create context
    ALCint attrs[] = {
        ALC_FREQUENCY, static_cast<ALCint>(config.sample_rate),
        ALC_MONO_SOURCES, static_cast<ALCint>(config.max_sources),
        ALC_STEREO_SOURCES, static_cast<ALCint>(config.max_sources / 4),
        0
    };

    m_impl->context = alcCreateContext(m_impl->device, attrs);
    if (!m_impl->context) {
        alcCloseDevice(m_impl->device);
        return void_core::Error{void_core::ErrorCode::InitializationFailed, "Failed to create audio context"};
    }

    alcMakeContextCurrent(m_impl->context);

    // Check for EFX extension (effects)
    if (alcIsExtensionPresent(m_impl->device, "ALC_EXT_EFX")) {
        // Initialize EFX
    }
    */

    m_impl->initialized = true;
    return {};
}

void OpenALBackend::shutdown() {
    if (!m_impl) return;

    // In a real implementation:
    /*
    if (m_impl->context) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_impl->context);
    }
    if (m_impl->device) {
        alcCloseDevice(m_impl->device);
    }
    */

    m_impl->buffers.clear();
    m_impl->sources.clear();
    m_impl->effects.clear();
    m_impl->initialized = false;
    m_impl.reset();
}

bool OpenALBackend::is_initialized() const {
    return m_impl && m_impl->initialized;
}

void_core::Result<BufferId> OpenALBackend::create_buffer(const AudioBufferDesc& desc) {
    if (!m_impl) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Backend not initialized"};
    }

    BufferId id{m_impl->next_buffer_id++};

    auto buffer = std::make_unique<AudioBuffer>(desc);
    buffer->set_id(id);

    // In a real implementation:
    /*
    ALuint al_buffer;
    alGenBuffers(1, &al_buffer);

    ALenum format = AL_FORMAT_MONO16; // Determine from desc.format
    alBufferData(al_buffer, format, desc.data, desc.data_size, desc.sample_rate);

    buffer->set_native_handle(reinterpret_cast<void*>(static_cast<uintptr_t>(al_buffer)));
    */

    m_impl->buffers[id] = std::move(buffer);
    m_impl->stats.loaded_buffers++;

    return id;
}

void OpenALBackend::destroy_buffer(BufferId id) {
    if (!m_impl) return;

    auto it = m_impl->buffers.find(id);
    if (it != m_impl->buffers.end()) {
        // In a real implementation:
        /*
        ALuint al_buffer = static_cast<ALuint>(reinterpret_cast<uintptr_t>(it->second->native_handle()));
        alDeleteBuffers(1, &al_buffer);
        */

        m_impl->buffers.erase(it);
        m_impl->stats.loaded_buffers--;
    }
}

IAudioBuffer* OpenALBackend::get_buffer(BufferId id) {
    if (!m_impl) return nullptr;
    auto it = m_impl->buffers.find(id);
    return it != m_impl->buffers.end() ? it->second.get() : nullptr;
}

void_core::Result<SourceId> OpenALBackend::create_source(const AudioSourceConfig& config) {
    if (!m_impl) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Backend not initialized"};
    }

    SourceId id{m_impl->next_source_id++};

    auto source = std::make_unique<AudioSource>(config);
    source->set_id(id);

    // Get duration from buffer
    if (auto* buffer = get_buffer(config.buffer)) {
        source->set_duration(buffer->duration());
    }

    // In a real implementation:
    /*
    ALuint al_source;
    alGenSources(1, &al_source);

    // Attach buffer
    auto* buffer = get_buffer(config.buffer);
    if (buffer) {
        ALuint al_buffer = static_cast<ALuint>(reinterpret_cast<uintptr_t>(buffer->native_handle()));
        alSourcei(al_source, AL_BUFFER, al_buffer);
    }

    // Set properties
    alSourcef(al_source, AL_GAIN, config.volume);
    alSourcef(al_source, AL_PITCH, config.pitch);
    alSource3f(al_source, AL_POSITION, config.position.x, config.position.y, config.position.z);
    alSource3f(al_source, AL_VELOCITY, config.velocity.x, config.velocity.y, config.velocity.z);
    alSourcei(al_source, AL_LOOPING, config.loop ? AL_TRUE : AL_FALSE);

    if (config.spatialization == SpatializationMode::None) {
        alSourcei(al_source, AL_SOURCE_RELATIVE, AL_TRUE);
        alSource3f(al_source, AL_POSITION, 0, 0, 0);
    }

    source->set_native_handle(reinterpret_cast<void*>(static_cast<uintptr_t>(al_source)));
    */

    m_impl->sources[id] = std::move(source);

    if (config.play_on_create) {
        m_impl->sources[id]->play();
    }

    return id;
}

void OpenALBackend::destroy_source(SourceId id) {
    if (!m_impl) return;

    auto it = m_impl->sources.find(id);
    if (it != m_impl->sources.end()) {
        // In a real implementation:
        /*
        ALuint al_source = static_cast<ALuint>(reinterpret_cast<uintptr_t>(it->second->native_handle()));
        alDeleteSources(1, &al_source);
        */

        m_impl->sources.erase(it);
    }
}

IAudioSource* OpenALBackend::get_source(SourceId id) {
    if (!m_impl) return nullptr;
    auto it = m_impl->sources.find(id);
    return it != m_impl->sources.end() ? it->second.get() : nullptr;
}

IAudioListener* OpenALBackend::listener() {
    if (!m_impl) return nullptr;
    return &m_impl->listener;
}

void_core::Result<EffectId> OpenALBackend::create_effect(const EffectConfig& config) {
    if (!m_impl) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Backend not initialized"};
    }

    EffectId id{m_impl->next_effect_id++};

    auto effect = AudioEffectFactory::create(config.type);
    if (!effect) {
        return void_core::Error{void_core::ErrorCode::NotSupported, "Effect type not supported"};
    }

    auto* effect_base = static_cast<AudioEffectBase*>(effect.get());
    effect_base->set_id(id);

    // In a real implementation with EFX:
    /*
    ALuint al_effect;
    alGenEffects(1, &al_effect);

    // Configure effect based on type
    switch (config.type) {
        case EffectType::Reverb:
            alEffecti(al_effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
            // Set reverb parameters...
            break;
        // ... other effect types
    }

    effect_base->set_native_handle(reinterpret_cast<void*>(static_cast<uintptr_t>(al_effect)));
    */

    m_impl->effects[id] = std::move(effect);

    return id;
}

void OpenALBackend::destroy_effect(EffectId id) {
    if (!m_impl) return;

    auto it = m_impl->effects.find(id);
    if (it != m_impl->effects.end()) {
        // In a real implementation:
        /*
        ALuint al_effect = static_cast<ALuint>(reinterpret_cast<uintptr_t>(it->second->native_handle()));
        alDeleteEffects(1, &al_effect);
        */

        m_impl->effects.erase(it);
    }
}

IAudioEffect* OpenALBackend::get_effect(EffectId id) {
    if (!m_impl) return nullptr;
    auto it = m_impl->effects.find(id);
    return it != m_impl->effects.end() ? it->second.get() : nullptr;
}

void OpenALBackend::update(float dt) {
    if (!m_impl) return;

    // Update listener
    if (m_impl->listener.is_dirty()) {
        // In a real implementation:
        /*
        auto pos = m_impl->listener.position();
        auto vel = m_impl->listener.velocity();
        auto fwd = m_impl->listener.forward();
        auto up = m_impl->listener.up();

        alListener3f(AL_POSITION, pos.x, pos.y, pos.z);
        alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);

        float orientation[] = {fwd.x, fwd.y, fwd.z, up.x, up.y, up.z};
        alListenerfv(AL_ORIENTATION, orientation);

        alListenerf(AL_GAIN, m_impl->listener.master_volume());
        alSpeedOfSound(m_impl->listener.speed_of_sound());
        alDopplerFactor(m_impl->listener.doppler_factor());
        */

        m_impl->listener.clear_dirty();
    }

    // Update sources
    m_impl->stats.active_sources = 0;

    for (auto& [id, source] : m_impl->sources) {
        if (!source) continue;

        source->update(dt);

        if (source->is_playing()) {
            m_impl->stats.active_sources++;
        }
    }
}

void OpenALBackend::process() {
    // OpenAL handles audio processing in its own thread
    // This function is for software mixing backends
}

AudioStats OpenALBackend::stats() const {
    if (!m_impl) return {};
    return m_impl->stats;
}

void OpenALBackend::reset_stats() {
    if (m_impl) {
        m_impl->stats = {};
        m_impl->stats.loaded_buffers = static_cast<std::uint32_t>(m_impl->buffers.size());
    }
}

// =============================================================================
// AudioBackendFactory Implementation
// =============================================================================

AudioBackendFactory& AudioBackendFactory::instance() {
    static AudioBackendFactory factory;
    return factory;
}

void AudioBackendFactory::register_backend(AudioBackend type, CreatorFunc creator) {
    auto backend = creator();
    RegisteredBackend entry;
    entry.creator = std::move(creator);
    entry.info = backend->info();
    m_backends[type] = std::move(entry);
}

void AudioBackendFactory::unregister_backend(AudioBackend type) {
    m_backends.erase(type);
}

bool AudioBackendFactory::is_available(AudioBackend type) const {
    return m_backends.find(type) != m_backends.end();
}

std::vector<AudioBackend> AudioBackendFactory::available_backends() const {
    std::vector<AudioBackend> result;
    result.reserve(m_backends.size());
    for (const auto& [type, entry] : m_backends) {
        result.push_back(type);
    }
    return result;
}

std::unique_ptr<IAudioBackend> AudioBackendFactory::create(AudioBackend type) const {
    auto it = m_backends.find(type);
    if (it == m_backends.end()) {
        return nullptr;
    }
    return it->second.creator();
}

std::unique_ptr<IAudioBackend> AudioBackendFactory::create_best() const {
    // Priority: OpenAL > XAudio2 > CoreAudio > Null
    static const AudioBackend priority[] = {
        AudioBackend::OpenAL,
        AudioBackend::XAudio2,
        AudioBackend::CoreAudio,
        AudioBackend::FMOD,
        AudioBackend::Wwise,
        AudioBackend::Null
    };

    for (auto type : priority) {
        if (is_available(type)) {
            return create(type);
        }
    }

    return std::make_unique<NullAudioBackend>();
}

void AudioBackendFactory::register_builtins() {
    register_backend(AudioBackend::Null, []() {
        return std::make_unique<NullAudioBackend>();
    });

    register_backend(AudioBackend::OpenAL, []() {
        return std::make_unique<OpenALBackend>();
    });

    // Other backends would be conditionally registered based on platform/availability
    #ifdef VOID_HAS_XAUDIO2
    // register_backend(AudioBackend::XAudio2, ...);
    #endif

    #ifdef VOID_HAS_COREAUDIO
    // register_backend(AudioBackend::CoreAudio, ...);
    #endif
}

// =============================================================================
// AudioSystem Implementation
// =============================================================================

AudioSystem::AudioSystem(AudioBackend backend)
    : m_one_shot(*this) {

    AudioBackendFactory::instance().register_builtins();
    m_backend = AudioBackendFactory::instance().create(backend);
    if (!m_backend) {
        m_backend = std::make_unique<NullAudioBackend>();
    }
}

AudioSystem::~AudioSystem() {
    shutdown();
}

void_core::Result<void> AudioSystem::initialize(const AudioConfig& config) {
    if (m_initialized) {
        return void_core::Error{void_core::ErrorCode::AlreadyExists, "Audio system already initialized"};
    }

    m_config = config;

    auto result = m_backend->initialize(config);
    if (!result) {
        return result;
    }

    // Create default mixer buses
    m_mixer.create_default_buses();

    m_initialized = true;
    return {};
}

void AudioSystem::shutdown() {
    if (!m_initialized) return;

    m_one_shot.stop_all();

    if (m_backend) {
        m_backend->shutdown();
    }

    m_initialized = false;
}

void_core::Result<BufferId> AudioSystem::load_buffer(const std::filesystem::path& path) {
    auto buffer_result = AudioBuffer::load_from_file(path);
    if (!buffer_result) {
        return void_core::Error{buffer_result.error().code(), buffer_result.error().message()};
    }

    AudioBufferDesc desc;
    desc.format = (*buffer_result)->format();
    desc.sample_rate = (*buffer_result)->sample_rate();
    desc.sample_count = (*buffer_result)->sample_count();
    desc.data = (*buffer_result)->data();
    desc.data_size = (*buffer_result)->size_bytes();
    desc.name = path.filename().string();

    return m_backend->create_buffer(desc);
}

void_core::Result<BufferId> AudioSystem::create_buffer(const AudioBufferDesc& desc) {
    return m_backend->create_buffer(desc);
}

IAudioBuffer* AudioSystem::get_buffer(BufferId id) {
    return m_backend->get_buffer(id);
}

void AudioSystem::destroy_buffer(BufferId id) {
    m_backend->destroy_buffer(id);
}

void_core::Result<SourceId> AudioSystem::create_source(const AudioSourceConfig& config) {
    return m_backend->create_source(config);
}

void_core::Result<SourceId> AudioSystem::create_source(const AudioSourceBuilder& builder) {
    return create_source(builder.build());
}

IAudioSource* AudioSystem::get_source(SourceId id) {
    return m_backend->get_source(id);
}

void AudioSystem::destroy_source(SourceId id) {
    m_backend->destroy_source(id);
}

void AudioSystem::play(SourceId id) {
    if (auto* source = get_source(id)) {
        source->play();
    }
}

void AudioSystem::pause(SourceId id) {
    if (auto* source = get_source(id)) {
        source->pause();
    }
}

void AudioSystem::stop(SourceId id) {
    if (auto* source = get_source(id)) {
        source->stop();
    }
}

void AudioSystem::stop_all() {
    // This would iterate all sources and stop them
    // In a real implementation, track all source IDs
}

IAudioListener* AudioSystem::listener() {
    return m_backend->listener();
}

OneShotHandle AudioSystem::play_one_shot(BufferId buffer, float volume) {
    return m_one_shot.play(buffer, volume);
}

OneShotHandle AudioSystem::play_one_shot_3d(BufferId buffer, const void_math::Vec3& position, float volume) {
    return m_one_shot.play_3d(buffer, position, volume);
}

void AudioSystem::play_music(BufferId buffer, const MusicConfig& config) {
    // Stop current music
    if (m_current_music) {
        if (auto* source = get_source(m_current_music)) {
            source->fade_out_and_stop(config.fade_time);
        }
    }

    // Create new music source
    AudioSourceConfig source_config;
    source_config.buffer = buffer;
    source_config.output_bus = m_mixer.music_bus();
    source_config.loop = config.loop;
    source_config.play_on_create = true;

    auto result = create_source(source_config);
    if (result) {
        m_current_music = *result;
        m_music_config = config;

        if (auto* source = get_source(m_current_music)) {
            source->fade_in(config.fade_time);
        }
    }
}

void AudioSystem::stop_music(float fade_time) {
    if (m_current_music) {
        if (auto* source = get_source(m_current_music)) {
            source->fade_out_and_stop(fade_time);
        }
        m_current_music = SourceId{0};
    }
}

void AudioSystem::pause_music() {
    if (m_current_music) {
        pause(m_current_music);
    }
}

void AudioSystem::resume_music() {
    if (m_current_music) {
        play(m_current_music);
    }
}

void AudioSystem::crossfade_music(BufferId buffer, float fade_time) {
    MusicConfig config = m_music_config;
    config.fade_time = fade_time;
    config.transition = MusicTransition::Crossfade;
    play_music(buffer, config);
}

float AudioSystem::music_volume() const {
    if (const auto* bus = m_mixer.get_bus(m_mixer.music_bus())) {
        return bus->volume();
    }
    return 1.0f;
}

void AudioSystem::set_music_volume(float volume) {
    if (auto* bus = m_mixer.get_bus(m_mixer.music_bus())) {
        bus->set_volume(volume);
    }
}

void AudioSystem::update(float dt) {
    if (!m_initialized) return;

    // Update backend
    m_backend->update(dt);

    // Update mixer
    m_mixer.update();

    // Update one-shot player
    m_one_shot.update();
}

AudioStats AudioSystem::stats() const {
    if (m_backend) {
        return m_backend->stats();
    }
    return {};
}

void_core::Result<void_core::HotReloadSnapshot> AudioSystem::snapshot() const {
    void_core::HotReloadSnapshot snap;
    snap.type_name = "audio_system";

    // Save mixer state
    auto mixer_snap = m_mixer.take_snapshot("hot_reload");

    // Serialize all data into the snapshot data vector
    // Format: [num_buses][bus_id, volume]...[music_id]
    size_t data_size = sizeof(std::uint32_t) + // num_buses
                       mixer_snap.volumes.size() * (sizeof(std::uint32_t) + sizeof(float)) + // bus volumes
                       sizeof(std::uint32_t); // music_id
    snap.data.resize(data_size);

    std::uint8_t* ptr = snap.data.data();

    // Write number of buses
    std::uint32_t num_buses = static_cast<std::uint32_t>(mixer_snap.volumes.size());
    std::memcpy(ptr, &num_buses, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Write bus volumes
    for (const auto& [id, volume] : mixer_snap.volumes) {
        std::memcpy(ptr, &id.value, sizeof(std::uint32_t));
        ptr += sizeof(std::uint32_t);
        std::memcpy(ptr, &volume, sizeof(float));
        ptr += sizeof(float);
    }

    // Write current music
    std::memcpy(ptr, &m_current_music.value, sizeof(std::uint32_t));

    return snap;
}

void_core::Result<void> AudioSystem::restore(void_core::HotReloadSnapshot snapshot) {
    if (snapshot.type_name != "audio_system") {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid snapshot type"};
    }

    if (snapshot.data.empty()) {
        return {};
    }

    // Deserialize data from the snapshot
    // Format: [num_buses][bus_id, volume]...[music_id]
    const std::uint8_t* ptr = snapshot.data.data();
    const std::uint8_t* end = ptr + snapshot.data.size();

    // Read number of buses
    if (ptr + sizeof(std::uint32_t) > end) return {};
    std::uint32_t num_buses;
    std::memcpy(&num_buses, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Read bus volumes
    for (std::uint32_t i = 0; i < num_buses && ptr + sizeof(std::uint32_t) + sizeof(float) <= end; ++i) {
        std::uint32_t id_val;
        std::memcpy(&id_val, ptr, sizeof(std::uint32_t));
        ptr += sizeof(std::uint32_t);

        float volume;
        std::memcpy(&volume, ptr, sizeof(float));
        ptr += sizeof(float);

        BusId id{id_val};
        if (auto* bus = m_mixer.get_bus(id)) {
            bus->set_volume(volume);
        }
    }

    return {};
}

} // namespace void_audio
