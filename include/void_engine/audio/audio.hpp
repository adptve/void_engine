/// @file audio.hpp
/// @brief Main include header for void_audio
///
/// void_audio provides comprehensive audio functionality:
/// - Multi-backend support (OpenAL, XAudio2, FMOD, Wwise)
/// - 3D spatialization with HRTF support
/// - Audio bus hierarchy for mixing
/// - DSP effects chain (reverb, delay, filter, compressor, etc.)
/// - Music system with crossfading
/// - Streaming for large audio files
/// - Voice limiting and prioritization
/// - Hot-reload support
///
/// ## Quick Start
///
/// ### Creating an Audio System
/// ```cpp
/// #include <void_engine/audio/audio.hpp>
///
/// // Create and initialize audio system
/// auto audio = std::make_unique<void_audio::AudioSystem>(void_audio::AudioBackend::OpenAL);
/// audio->initialize(void_audio::AudioConfig::defaults());
///
/// // Create default buses (Master, Music, SFX, Voice, Ambient)
/// audio->mixer().create_default_buses();
/// ```
///
/// ### Loading and Playing Sounds
/// ```cpp
/// // Load a sound effect
/// auto buffer_result = audio->load_buffer("sounds/explosion.wav");
/// if (!buffer_result) {
///     // Handle error
/// }
/// auto buffer_id = *buffer_result;
///
/// // Play as one-shot (fire and forget)
/// audio->play_one_shot(buffer_id, 0.8f); // 80% volume
///
/// // Play 3D positioned sound
/// audio->play_one_shot_3d(buffer_id, {10.0f, 0.0f, 5.0f});
/// ```
///
/// ### Creating Audio Sources
/// ```cpp
/// // Create a looping ambient source
/// auto source_result = audio->create_source(
///     void_audio::AudioSourceBuilder()
///         .buffer(buffer_id)
///         .bus(audio->mixer().ambient_bus())
///         .volume(0.5f)
///         .loop()
///         .play_on_create()
///         .build()
/// );
///
/// // Create a 3D positional source
/// auto spatial_source = audio->create_source(
///     void_audio::AudioSourceBuilder()
///         .buffer(footstep_buffer)
///         .spatial_3d()
///         .position(5.0f, 0.0f, 3.0f)
///         .reference_distance(1.0f)
///         .max_distance(50.0f)
///         .build()
/// );
///
/// // Control playback
/// if (auto* source = audio->get_source(*source_result)) {
///     source->play();
///     source->fade_to(0.3f, 2.0f);  // Fade to 30% over 2 seconds
/// }
/// ```
///
/// ### Listener Setup
/// ```cpp
/// // Update listener position (usually follows camera)
/// auto* listener = audio->listener();
/// listener->set_position(camera.position());
/// listener->set_orientation(camera.forward(), camera.up());
/// listener->set_velocity(player.velocity());  // For Doppler effect
/// ```
///
/// ### Audio Mixing
/// ```cpp
/// // Get mixer and buses
/// auto& mixer = audio->mixer();
///
/// // Adjust bus volumes
/// mixer.get_bus(mixer.sfx_bus())->set_volume(0.8f);
/// mixer.get_bus(mixer.music_bus())->set_volume(0.6f);
///
/// // Mute voice
/// mixer.get_bus(mixer.voice_bus())->set_muted(true);
///
/// // Create a snapshot
/// auto snapshot = mixer.take_snapshot("gameplay");
///
/// // Later, restore with blend
/// mixer.apply_snapshot(snapshot, 1.0f);  // Blend over 1 second
/// ```
///
/// ### Music System
/// ```cpp
/// // Load music
/// auto music_buffer = *audio->load_buffer("music/battle.ogg");
///
/// // Play with configuration
/// void_audio::MusicConfig music_config;
/// music_config.loop = true;
/// music_config.fade_time = 2.0f;
/// audio->play_music(music_buffer, music_config);
///
/// // Crossfade to new track
/// auto exploration_music = *audio->load_buffer("music/exploration.ogg");
/// audio->crossfade_music(exploration_music, 3.0f);
///
/// // Control music volume
/// audio->set_music_volume(0.7f);
/// ```
///
/// ### Audio Effects
/// ```cpp
/// // Create reverb effect
/// void_audio::ReverbConfig reverb = void_audio::ReverbConfig::large_hall();
/// auto effect = void_audio::AudioEffectFactory::create_reverb(reverb);
///
/// // Add to bus
/// mixer.get_bus(mixer.sfx_bus())->add_effect(effect->id());
///
/// // Adjust effect in real-time
/// auto* reverb_effect = static_cast<void_audio::ReverbEffect*>(
///     audio->backend()->get_effect(effect->id())
/// );
/// reverb_effect->set_decay_time(2.5f);
/// reverb_effect->set_mix(0.3f);  // 30% wet
/// ```
///
/// ### Update Loop
/// ```cpp
/// void game_loop(float dt) {
///     // Update audio system
///     audio->update(dt);
///
///     // Update listener
///     audio->listener()->set_position(camera.position());
///     audio->listener()->set_orientation(camera.forward(), camera.up());
/// }
/// ```

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "buffer.hpp"
#include "source.hpp"
#include "listener.hpp"
#include "mixer.hpp"
#include "effects.hpp"
#include "backend.hpp"

namespace void_audio {

/// Prelude - commonly used types
namespace prelude {
    using void_audio::AudioSystem;
    using void_audio::AudioConfig;
    using void_audio::AudioStats;
    using void_audio::AudioBackend;

    using void_audio::IAudioBuffer;
    using void_audio::AudioBuffer;
    using void_audio::StreamingBuffer;
    using void_audio::AudioBufferDesc;
    using void_audio::BufferId;

    using void_audio::IAudioSource;
    using void_audio::AudioSource;
    using void_audio::AudioSourceConfig;
    using void_audio::AudioSourceBuilder;
    using void_audio::SourceId;
    using void_audio::AudioState;

    using void_audio::IAudioListener;
    using void_audio::AudioListener;
    using void_audio::ListenerConfig;

    using void_audio::IAudioBus;
    using void_audio::AudioBus;
    using void_audio::AudioMixer;
    using void_audio::BusConfig;
    using void_audio::BusId;

    using void_audio::IAudioEffect;
    using void_audio::ReverbEffect;
    using void_audio::DelayEffect;
    using void_audio::FilterEffect;
    using void_audio::CompressorEffect;
    using void_audio::DistortionEffect;
    using void_audio::ChorusEffect;
    using void_audio::EQEffect;
    using void_audio::AudioEffectFactory;
    using void_audio::EffectChain;
    using void_audio::EffectId;
    using void_audio::EffectType;

    using void_audio::ReverbConfig;
    using void_audio::DelayConfig;
    using void_audio::FilterConfig;
    using void_audio::CompressorConfig;
    using void_audio::DistortionConfig;
    using void_audio::ChorusConfig;
    using void_audio::EQConfig;
    using void_audio::EQBand;

    using void_audio::AudioFormat;
    using void_audio::SpatializationMode;
    using void_audio::AttenuationModel;
    using void_audio::AttenuationSettings;
    using void_audio::ConeSettings;

    using void_audio::OneShotHandle;
    using void_audio::OneShotPlayer;

    using void_audio::MusicConfig;
    using void_audio::MusicTransition;

    using void_audio::VoiceLimiter;
    using void_audio::AudioDucker;
    using void_audio::DuckConfig;

    using void_audio::IAudioBackend;
    using void_audio::AudioBackendFactory;
} // namespace prelude

} // namespace void_audio
