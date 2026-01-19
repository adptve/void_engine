//! Audio system using rodio

use crate::channel::ChannelManager;
use crate::settings::{MusicSettings, SoundSettings};
use parking_lot::RwLock;
use rodio::{Decoder, OutputStream, OutputStreamHandle, Sink};
use std::collections::HashMap;
use std::fs::File;
use std::io::BufReader;
use std::path::Path;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;

/// Audio error types
#[derive(Debug)]
pub enum AudioError {
    /// Failed to initialize audio device
    DeviceInit(String),
    /// Failed to load audio file
    LoadError(String),
    /// Invalid handle
    InvalidHandle,
    /// Channel not found
    ChannelNotFound(String),
    /// File not found
    FileNotFound(String),
}

impl std::fmt::Display for AudioError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::DeviceInit(msg) => write!(f, "Failed to initialize audio device: {}", msg),
            Self::LoadError(msg) => write!(f, "Failed to load audio: {}", msg),
            Self::InvalidHandle => write!(f, "Invalid sound handle"),
            Self::ChannelNotFound(name) => write!(f, "Channel not found: {}", name),
            Self::FileNotFound(path) => write!(f, "Audio file not found: {}", path),
        }
    }
}

impl std::error::Error for AudioError {}

/// Handle to a playing sound
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SoundHandle(pub u64);

impl SoundHandle {
    /// Invalid handle constant
    pub const INVALID: SoundHandle = SoundHandle(0);
}

/// Internal sound data
struct PlayingSound {
    sink: Sink,
    channel: String,
    path: String,
    looping: bool,
}

/// The audio system
pub struct AudioSystem {
    /// Output stream (must be kept alive)
    _stream: OutputStream,
    /// Stream handle for creating sinks
    stream_handle: OutputStreamHandle,
    /// Channel manager
    channels: Arc<RwLock<ChannelManager>>,
    /// Currently playing sounds
    sounds: HashMap<u64, PlayingSound>,
    /// Next handle ID
    next_handle: AtomicU64,
    /// Current music track
    music_sink: Option<Sink>,
    /// Current music path
    music_path: Option<String>,
    /// Global master volume
    master_volume: f32,
    /// Base path for audio files
    audio_path: String,
}

impl AudioSystem {
    /// Create a new audio system
    pub fn new() -> Result<Self, AudioError> {
        let (stream, stream_handle) = OutputStream::try_default()
            .map_err(|e| AudioError::DeviceInit(e.to_string()))?;

        Ok(Self {
            _stream: stream,
            stream_handle,
            channels: Arc::new(RwLock::new(ChannelManager::new())),
            sounds: HashMap::new(),
            next_handle: AtomicU64::new(1),
            music_sink: None,
            music_path: None,
            master_volume: 1.0,
            audio_path: String::new(),
        })
    }

    /// Set the base path for audio files
    pub fn set_audio_path(&mut self, path: impl Into<String>) {
        self.audio_path = path.into();
    }

    /// Get full path for an audio file
    fn resolve_path(&self, path: &str) -> String {
        if self.audio_path.is_empty() || Path::new(path).is_absolute() {
            path.to_string()
        } else {
            format!("{}/{}", self.audio_path, path)
        }
    }

    /// Get channel manager
    pub fn channels(&self) -> &Arc<RwLock<ChannelManager>> {
        &self.channels
    }

    /// Set master volume
    pub fn set_master_volume(&mut self, volume: f32) {
        self.master_volume = volume.clamp(0.0, 1.0);
        self.channels.write().set_master_volume(volume);
        self.update_all_volumes();
    }

    /// Get master volume
    pub fn master_volume(&self) -> f32 {
        self.master_volume
    }

    /// Set channel volume
    pub fn set_channel_volume(&mut self, channel: &str, volume: f32) {
        self.channels.write().set_volume(channel, volume);
        self.update_channel_volumes(channel);
    }

    /// Mute/unmute a channel
    pub fn set_channel_muted(&mut self, channel: &str, muted: bool) {
        self.channels.write().set_muted(channel, muted);
        self.update_channel_volumes(channel);
    }

    /// Play a sound effect
    pub fn play_sound(
        &mut self,
        path: &str,
        settings: SoundSettings,
    ) -> Result<SoundHandle, AudioError> {
        let full_path = self.resolve_path(path);

        let file = File::open(&full_path)
            .map_err(|_| AudioError::FileNotFound(full_path.clone()))?;

        let source = Decoder::new(BufReader::new(file))
            .map_err(|e| AudioError::LoadError(e.to_string()))?;

        let sink = Sink::try_new(&self.stream_handle)
            .map_err(|e| AudioError::DeviceInit(e.to_string()))?;

        // Calculate volume with channel
        let channel = settings.channel.clone().unwrap_or_else(|| "sfx".to_string());
        let channel_volume = self.channels.read().calculate_volume(&channel);
        let final_volume = settings.volume * channel_volume * self.master_volume;

        sink.set_volume(final_volume);
        sink.set_speed(settings.speed);

        if settings.looping {
            sink.append(rodio::source::Source::repeat_infinite(source));
        } else {
            sink.append(source);
        }

        let handle_id = self.next_handle.fetch_add(1, Ordering::SeqCst);

        self.sounds.insert(
            handle_id,
            PlayingSound {
                sink,
                channel,
                path: full_path,
                looping: settings.looping,
            },
        );

        Ok(SoundHandle(handle_id))
    }

    /// Play music
    pub fn play_music(
        &mut self,
        path: &str,
        settings: MusicSettings,
    ) -> Result<(), AudioError> {
        let full_path = self.resolve_path(path);

        // Stop current music
        if let Some(sink) = self.music_sink.take() {
            if settings.crossfade && settings.fade_out > 0.0 {
                // TODO: Implement proper crossfade with timing
                sink.stop();
            } else {
                sink.stop();
            }
        }

        let file = File::open(&full_path)
            .map_err(|_| AudioError::FileNotFound(full_path.clone()))?;

        let source = Decoder::new(BufReader::new(file))
            .map_err(|e| AudioError::LoadError(e.to_string()))?;

        let sink = Sink::try_new(&self.stream_handle)
            .map_err(|e| AudioError::DeviceInit(e.to_string()))?;

        let channel_volume = self.channels.read().calculate_volume("music");
        let final_volume = settings.volume * channel_volume * self.master_volume;

        sink.set_volume(if settings.fade_in > 0.0 { 0.0 } else { final_volume });

        if settings.looping {
            sink.append(rodio::source::Source::repeat_infinite(source));
        } else {
            sink.append(source);
        }

        self.music_sink = Some(sink);
        self.music_path = Some(full_path);

        Ok(())
    }

    /// Stop music
    pub fn stop_music(&mut self) {
        if let Some(sink) = self.music_sink.take() {
            sink.stop();
        }
        self.music_path = None;
    }

    /// Pause music
    pub fn pause_music(&mut self) {
        if let Some(ref sink) = self.music_sink {
            sink.pause();
        }
    }

    /// Resume music
    pub fn resume_music(&mut self) {
        if let Some(ref sink) = self.music_sink {
            sink.play();
        }
    }

    /// Stop a sound by handle
    pub fn stop_sound(&mut self, handle: SoundHandle) {
        if let Some(sound) = self.sounds.remove(&handle.0) {
            sound.sink.stop();
        }
    }

    /// Pause a sound by handle
    pub fn pause_sound(&mut self, handle: SoundHandle) {
        if let Some(sound) = self.sounds.get(&handle.0) {
            sound.sink.pause();
        }
    }

    /// Resume a sound by handle
    pub fn resume_sound(&mut self, handle: SoundHandle) {
        if let Some(sound) = self.sounds.get(&handle.0) {
            sound.sink.play();
        }
    }

    /// Set volume for a sound
    pub fn set_sound_volume(&mut self, handle: SoundHandle, volume: f32) {
        if let Some(sound) = self.sounds.get(&handle.0) {
            let channel_volume = self.channels.read().calculate_volume(&sound.channel);
            sound.sink.set_volume(volume * channel_volume * self.master_volume);
        }
    }

    /// Check if a sound is playing
    pub fn is_playing(&self, handle: SoundHandle) -> bool {
        if let Some(sound) = self.sounds.get(&handle.0) {
            !sound.sink.empty()
        } else {
            false
        }
    }

    /// Stop all sounds
    pub fn stop_all(&mut self) {
        for (_, sound) in self.sounds.drain() {
            sound.sink.stop();
        }
    }

    /// Stop all sounds in a channel
    pub fn stop_channel(&mut self, channel: &str) {
        let to_remove: Vec<_> = self
            .sounds
            .iter()
            .filter(|(_, s)| s.channel == channel)
            .map(|(&id, _)| id)
            .collect();

        for id in to_remove {
            if let Some(sound) = self.sounds.remove(&id) {
                sound.sink.stop();
            }
        }
    }

    /// Pause all sounds in a channel
    pub fn pause_channel(&mut self, channel: &str) {
        for sound in self.sounds.values() {
            if sound.channel == channel {
                sound.sink.pause();
            }
        }
    }

    /// Resume all sounds in a channel
    pub fn resume_channel(&mut self, channel: &str) {
        for sound in self.sounds.values() {
            if sound.channel == channel {
                sound.sink.play();
            }
        }
    }

    /// Update the audio system (cleanup finished sounds)
    pub fn update(&mut self, _delta_time: f32) {
        // Remove finished sounds
        self.sounds.retain(|_, sound| {
            if sound.looping {
                true
            } else {
                !sound.sink.empty()
            }
        });
    }

    /// Update volumes for all sounds
    fn update_all_volumes(&mut self) {
        let channels = self.channels.read();
        for sound in self.sounds.values() {
            let channel_volume = channels.calculate_volume(&sound.channel);
            // Note: We don't know the original volume, so this is an approximation
            sound.sink.set_volume(channel_volume * self.master_volume);
        }

        if let Some(ref sink) = self.music_sink {
            let channel_volume = channels.calculate_volume("music");
            sink.set_volume(channel_volume * self.master_volume);
        }
    }

    /// Update volumes for sounds in a specific channel
    fn update_channel_volumes(&mut self, channel: &str) {
        let channel_volume = self.channels.read().calculate_volume(channel);
        for sound in self.sounds.values() {
            if sound.channel == channel {
                sound.sink.set_volume(channel_volume * self.master_volume);
            }
        }

        if channel == "music" {
            if let Some(ref sink) = self.music_sink {
                sink.set_volume(channel_volume * self.master_volume);
            }
        }
    }

    /// Get number of active sounds
    pub fn active_sound_count(&self) -> usize {
        self.sounds.len()
    }

    /// Check if music is playing
    pub fn is_music_playing(&self) -> bool {
        self.music_sink
            .as_ref()
            .map(|s| !s.empty() && !s.is_paused())
            .unwrap_or(false)
    }

    /// Get current music path
    pub fn current_music(&self) -> Option<&str> {
        self.music_path.as_deref()
    }
}

// Note: AudioSystem can't implement Default because it needs to initialize the audio device
// which can fail

#[cfg(test)]
mod tests {
    use super::*;

    // Note: Audio tests require audio hardware and are typically run manually
    // These tests just verify the basic API compiles

    #[test]
    fn test_sound_handle() {
        let handle = SoundHandle(123);
        assert_eq!(handle.0, 123);
        assert_ne!(handle, SoundHandle::INVALID);
    }

    #[test]
    fn test_audio_error_display() {
        let err = AudioError::FileNotFound("test.wav".to_string());
        assert!(err.to_string().contains("test.wav"));
    }
}
