//! Audio Backend
//!
//! Real audio playback using rodio/cpal.
//! This module is only compiled when the `audio-backend` feature is enabled.

use std::collections::HashMap;
use std::io::{BufReader, Cursor};
use std::sync::Arc;
use parking_lot::RwLock;
use thiserror::Error;

use rodio::{Decoder, OutputStream, OutputStreamHandle, Sink, Source};

use crate::audio::{AudioHandle, AudioState, AudioSourceType};

/// Audio backend errors
#[derive(Debug, Error)]
pub enum BackendError {
    #[error("Failed to initialize audio output: {0}")]
    OutputInit(String),

    #[error("Failed to decode audio: {0}")]
    Decode(String),

    #[error("Source not found: {0}")]
    SourceNotFound(u64),

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
}

/// Audio playback sink wrapper
struct PlaybackSink {
    sink: Sink,
    source_type: AudioSourceType,
    handle: AudioHandle,
    volume: f32,
    speed: f32,
}

impl PlaybackSink {
    fn new(sink: Sink, handle: AudioHandle, source_type: AudioSourceType) -> Self {
        Self {
            sink,
            source_type,
            handle,
            volume: 1.0,
            speed: 1.0,
        }
    }

    fn state(&self) -> AudioState {
        if self.sink.empty() {
            AudioState::Stopped
        } else if self.sink.is_paused() {
            AudioState::Paused
        } else {
            AudioState::Playing
        }
    }
}

/// Audio backend - manages real audio playback
pub struct AudioBackend {
    /// Output stream (must be kept alive)
    _stream: OutputStream,
    /// Stream handle for creating sinks
    stream_handle: OutputStreamHandle,
    /// Active playback sinks
    sinks: HashMap<AudioHandle, PlaybackSink>,
    /// Loaded audio data cache
    audio_cache: Arc<RwLock<HashMap<String, Vec<u8>>>>,
    /// Master volume
    master_volume: f32,
    /// Listener position for spatial audio
    listener_position: [f32; 3],
}

impl AudioBackend {
    /// Create a new audio backend
    pub fn new() -> Result<Self, BackendError> {
        let (stream, stream_handle) = OutputStream::try_default()
            .map_err(|e| BackendError::OutputInit(e.to_string()))?;

        Ok(Self {
            _stream: stream,
            stream_handle,
            sinks: HashMap::new(),
            audio_cache: Arc::new(RwLock::new(HashMap::new())),
            master_volume: 1.0,
            listener_position: [0.0, 0.0, 0.0],
        })
    }

    /// Load audio data from bytes
    pub fn load_from_bytes(&self, path: &str, data: Vec<u8>) {
        self.audio_cache.write().insert(path.to_string(), data);
    }

    /// Play audio from cached data
    pub fn play_cached(
        &mut self,
        handle: AudioHandle,
        path: &str,
        source_type: AudioSourceType,
        looping: bool,
    ) -> Result<(), BackendError> {
        let data = {
            let cache = self.audio_cache.read();
            cache.get(path).cloned()
                .ok_or_else(|| BackendError::Decode(format!("Audio not cached: {}", path)))?
        };

        self.play_from_bytes(handle, &data, source_type, looping)
    }

    /// Play audio from bytes
    pub fn play_from_bytes(
        &mut self,
        handle: AudioHandle,
        data: &[u8],
        source_type: AudioSourceType,
        looping: bool,
    ) -> Result<(), BackendError> {
        let cursor = Cursor::new(data.to_vec());
        let source = Decoder::new(BufReader::new(cursor))
            .map_err(|e| BackendError::Decode(e.to_string()))?;

        let sink = Sink::try_new(&self.stream_handle)
            .map_err(|e| BackendError::OutputInit(e.to_string()))?;

        if looping {
            sink.append(source.repeat_infinite());
        } else {
            sink.append(source);
        }

        sink.set_volume(self.master_volume);
        sink.play();

        let playback = PlaybackSink::new(sink, handle, source_type);
        self.sinks.insert(handle, playback);

        Ok(())
    }

    /// Play a simple test tone
    pub fn play_test_tone(&mut self, handle: AudioHandle, frequency: f32, duration_secs: f32) -> Result<(), BackendError> {
        let sink = Sink::try_new(&self.stream_handle)
            .map_err(|e| BackendError::OutputInit(e.to_string()))?;

        // Generate sine wave
        let sample_rate = 44100u32;
        let num_samples = (sample_rate as f32 * duration_secs) as usize;
        let samples: Vec<f32> = (0..num_samples)
            .map(|i| {
                let t = i as f32 / sample_rate as f32;
                (t * frequency * 2.0 * std::f32::consts::PI).sin() * 0.3
            })
            .collect();

        let source = rodio::buffer::SamplesBuffer::new(1, sample_rate, samples);
        sink.append(source);
        sink.set_volume(self.master_volume);
        sink.play();

        let playback = PlaybackSink::new(sink, handle, AudioSourceType::Effect);
        self.sinks.insert(handle, playback);

        Ok(())
    }

    /// Pause playback
    pub fn pause(&mut self, handle: AudioHandle) -> Result<(), BackendError> {
        let sink = self.sinks.get(&handle)
            .ok_or(BackendError::SourceNotFound(handle.id()))?;
        sink.sink.pause();
        Ok(())
    }

    /// Resume playback
    pub fn resume(&mut self, handle: AudioHandle) -> Result<(), BackendError> {
        let sink = self.sinks.get(&handle)
            .ok_or(BackendError::SourceNotFound(handle.id()))?;
        sink.sink.play();
        Ok(())
    }

    /// Stop and remove playback
    pub fn stop(&mut self, handle: AudioHandle) -> Result<(), BackendError> {
        if let Some(sink) = self.sinks.remove(&handle) {
            sink.sink.stop();
        }
        Ok(())
    }

    /// Set volume for a source
    pub fn set_volume(&mut self, handle: AudioHandle, volume: f32) -> Result<(), BackendError> {
        let sink = self.sinks.get_mut(&handle)
            .ok_or(BackendError::SourceNotFound(handle.id()))?;
        sink.volume = volume;
        sink.sink.set_volume(volume * self.master_volume);
        Ok(())
    }

    /// Set playback speed
    pub fn set_speed(&mut self, handle: AudioHandle, speed: f32) -> Result<(), BackendError> {
        let sink = self.sinks.get_mut(&handle)
            .ok_or(BackendError::SourceNotFound(handle.id()))?;
        sink.speed = speed;
        sink.sink.set_speed(speed);
        Ok(())
    }

    /// Set master volume
    pub fn set_master_volume(&mut self, volume: f32) {
        self.master_volume = volume.clamp(0.0, 1.0);

        // Update all active sinks
        for sink in self.sinks.values() {
            sink.sink.set_volume(sink.volume * self.master_volume);
        }
    }

    /// Get playback state
    pub fn get_state(&self, handle: AudioHandle) -> Option<AudioState> {
        self.sinks.get(&handle).map(|s| s.state())
    }

    /// Check if source is playing
    pub fn is_playing(&self, handle: AudioHandle) -> bool {
        self.sinks.get(&handle)
            .map(|s| !s.sink.empty() && !s.sink.is_paused())
            .unwrap_or(false)
    }

    /// Get number of active sources
    pub fn active_count(&self) -> usize {
        self.sinks.values().filter(|s| !s.sink.empty()).count()
    }

    /// Stop all playback
    pub fn stop_all(&mut self) {
        for sink in self.sinks.values() {
            sink.sink.stop();
        }
        self.sinks.clear();
    }

    /// Clean up finished sources
    pub fn cleanup_finished(&mut self) {
        self.sinks.retain(|_, s| !s.sink.empty());
    }

    /// Set listener position for spatial audio
    pub fn set_listener_position(&mut self, position: [f32; 3]) {
        self.listener_position = position;
        // Note: Rodio doesn't have built-in spatial audio,
        // but this sets up the infrastructure for a custom spatial audio system
    }

    /// Get listener position
    pub fn listener_position(&self) -> [f32; 3] {
        self.listener_position
    }
}

impl Default for AudioBackend {
    fn default() -> Self {
        Self::new().expect("Failed to initialize audio backend")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_backend_creation() {
        // This test may fail if no audio device is available
        let result = AudioBackend::new();
        if result.is_err() {
            println!("Audio backend not available: {:?}", result.err());
            return;
        }
        let backend = result.unwrap();
        assert_eq!(backend.active_count(), 0);
    }

    #[test]
    fn test_master_volume() {
        let result = AudioBackend::new();
        if result.is_err() {
            return;
        }
        let mut backend = result.unwrap();

        backend.set_master_volume(0.5);
        assert!((backend.master_volume - 0.5).abs() < 0.001);

        // Test clamping
        backend.set_master_volume(2.0);
        assert!((backend.master_volume - 1.0).abs() < 0.001);

        backend.set_master_volume(-1.0);
        assert!(backend.master_volume.abs() < 0.001);
    }
}
