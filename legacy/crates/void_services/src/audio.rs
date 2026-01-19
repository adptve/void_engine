//! Audio service
//!
//! Manages audio playback, mixing, and spatial audio.

use std::any::Any;
use std::collections::HashMap;
use thiserror::Error;
use serde::{Serialize, Deserialize};

use crate::service::{Service, ServiceId, ServiceState, ServiceHealth, ServiceConfig, ServiceResult};

/// Audio handle - reference to an audio source
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct AudioHandle(u64);

impl AudioHandle {
    /// Create a new audio handle
    pub fn new(id: u64) -> Self {
        Self(id)
    }

    /// Get the raw ID
    pub fn id(&self) -> u64 {
        self.0
    }
}

/// Audio source state
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum AudioState {
    /// Stopped
    Stopped,
    /// Playing
    Playing,
    /// Paused
    Paused,
    /// Loading
    Loading,
    /// Error state
    Error,
}

/// Audio source type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum AudioSourceType {
    /// Sound effect (short, one-shot)
    Effect,
    /// Music (long, streamed)
    Music,
    /// Ambient (looping background)
    Ambient,
    /// Voice/speech
    Voice,
    /// UI sounds
    Ui,
}

/// Audio source properties
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioSource {
    /// Source handle
    pub handle: AudioHandle,
    /// Source type
    pub source_type: AudioSourceType,
    /// Current state
    pub state: AudioState,
    /// Volume (0.0 - 1.0)
    pub volume: f32,
    /// Pan (-1.0 left, 0.0 center, 1.0 right)
    pub pan: f32,
    /// Pitch multiplier
    pub pitch: f32,
    /// Whether to loop
    pub looping: bool,
    /// 3D position (if spatial audio)
    pub position: Option<[f32; 3]>,
    /// Playback progress (0.0 - 1.0)
    pub progress: f32,
    /// Duration in seconds (if known)
    pub duration: Option<f32>,
}

impl Default for AudioSource {
    fn default() -> Self {
        Self {
            handle: AudioHandle::new(0),
            source_type: AudioSourceType::Effect,
            state: AudioState::Stopped,
            volume: 1.0,
            pan: 0.0,
            pitch: 1.0,
            looping: false,
            position: None,
            progress: 0.0,
            duration: None,
        }
    }
}

/// Audio channel/bus for mixing
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioChannel {
    /// Channel name
    pub name: String,
    /// Master volume
    pub volume: f32,
    /// Muted
    pub muted: bool,
    /// Solo (only this channel plays)
    pub solo: bool,
    /// Active source count
    pub source_count: usize,
}

impl AudioChannel {
    /// Create a new channel
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            volume: 1.0,
            muted: false,
            solo: false,
            source_count: 0,
        }
    }

    /// Get effective volume
    pub fn effective_volume(&self) -> f32 {
        if self.muted { 0.0 } else { self.volume }
    }
}

/// Audio service configuration
#[derive(Debug, Clone)]
pub struct AudioServiceConfig {
    /// Base service config
    pub service: ServiceConfig,
    /// Master volume
    pub master_volume: f32,
    /// Maximum concurrent audio sources
    pub max_sources: usize,
    /// Enable spatial audio
    pub spatial_audio: bool,
    /// Listener position
    pub listener_position: [f32; 3],
}

impl Default for AudioServiceConfig {
    fn default() -> Self {
        Self {
            service: ServiceConfig::new("audio"),
            master_volume: 1.0,
            max_sources: 64,
            spatial_audio: true,
            listener_position: [0.0, 0.0, 0.0],
        }
    }
}

/// Audio errors
#[derive(Debug, Error)]
pub enum AudioError {
    #[error("Audio source not found: {0}")]
    SourceNotFound(u64),

    #[error("Channel not found: {0}")]
    ChannelNotFound(String),

    #[error("Maximum sources exceeded")]
    MaxSources,

    #[error("Audio backend error: {0}")]
    BackendError(String),

    #[error("Invalid parameter: {0}")]
    InvalidParameter(String),
}

/// Audio statistics
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct AudioStats {
    /// Currently playing sources
    pub playing_sources: usize,
    /// Total sources created
    pub total_sources: u64,
    /// CPU usage percentage
    pub cpu_usage: f32,
}

/// Audio service - manages audio playback
pub struct AudioService {
    /// Configuration
    config: AudioServiceConfig,
    /// Current state
    state: ServiceState,
    /// Next handle ID
    next_handle: u64,
    /// Active audio sources
    sources: HashMap<AudioHandle, AudioSource>,
    /// Audio channels
    channels: HashMap<String, AudioChannel>,
    /// Statistics
    stats: AudioStats,
}

impl AudioService {
    /// Create a new audio service
    pub fn new(config: AudioServiceConfig) -> Self {
        let mut service = Self {
            config,
            state: ServiceState::Stopped,
            next_handle: 1,
            sources: HashMap::new(),
            channels: HashMap::new(),
            stats: AudioStats::default(),
        };

        // Create default channels
        service.create_channel("master");
        service.create_channel("music");
        service.create_channel("effects");
        service.create_channel("voice");
        service.create_channel("ambient");
        service.create_channel("ui");

        service
    }

    /// Create with default configuration
    pub fn with_defaults() -> Self {
        Self::new(AudioServiceConfig::default())
    }

    /// Create a new audio source
    pub fn create_source(&mut self, source_type: AudioSourceType) -> Result<AudioHandle, AudioError> {
        if self.sources.len() >= self.config.max_sources {
            return Err(AudioError::MaxSources);
        }

        let handle = AudioHandle::new(self.next_handle);
        self.next_handle += 1;

        let source = AudioSource {
            handle,
            source_type,
            ..Default::default()
        };

        self.sources.insert(handle, source);
        self.stats.total_sources += 1;

        Ok(handle)
    }

    /// Get an audio source
    pub fn get_source(&self, handle: AudioHandle) -> Option<&AudioSource> {
        self.sources.get(&handle)
    }

    /// Get a mutable audio source
    pub fn get_source_mut(&mut self, handle: AudioHandle) -> Option<&mut AudioSource> {
        self.sources.get_mut(&handle)
    }

    /// Play an audio source
    pub fn play(&mut self, handle: AudioHandle) -> Result<(), AudioError> {
        let source = self.sources.get_mut(&handle)
            .ok_or(AudioError::SourceNotFound(handle.id()))?;

        source.state = AudioState::Playing;
        self.update_stats();
        Ok(())
    }

    /// Pause an audio source
    pub fn pause(&mut self, handle: AudioHandle) -> Result<(), AudioError> {
        let source = self.sources.get_mut(&handle)
            .ok_or(AudioError::SourceNotFound(handle.id()))?;

        if source.state == AudioState::Playing {
            source.state = AudioState::Paused;
        }
        self.update_stats();
        Ok(())
    }

    /// Stop an audio source
    pub fn stop_source(&mut self, handle: AudioHandle) -> Result<(), AudioError> {
        let source = self.sources.get_mut(&handle)
            .ok_or(AudioError::SourceNotFound(handle.id()))?;

        source.state = AudioState::Stopped;
        source.progress = 0.0;
        self.update_stats();
        Ok(())
    }

    /// Remove an audio source
    pub fn remove_source(&mut self, handle: AudioHandle) -> Result<(), AudioError> {
        self.sources.remove(&handle)
            .ok_or(AudioError::SourceNotFound(handle.id()))?;
        self.update_stats();
        Ok(())
    }

    /// Set source volume
    pub fn set_volume(&mut self, handle: AudioHandle, volume: f32) -> Result<(), AudioError> {
        let source = self.sources.get_mut(&handle)
            .ok_or(AudioError::SourceNotFound(handle.id()))?;

        source.volume = volume.clamp(0.0, 1.0);
        Ok(())
    }

    /// Set source pan
    pub fn set_pan(&mut self, handle: AudioHandle, pan: f32) -> Result<(), AudioError> {
        let source = self.sources.get_mut(&handle)
            .ok_or(AudioError::SourceNotFound(handle.id()))?;

        source.pan = pan.clamp(-1.0, 1.0);
        Ok(())
    }

    /// Set source pitch
    pub fn set_pitch(&mut self, handle: AudioHandle, pitch: f32) -> Result<(), AudioError> {
        let source = self.sources.get_mut(&handle)
            .ok_or(AudioError::SourceNotFound(handle.id()))?;

        if pitch <= 0.0 {
            return Err(AudioError::InvalidParameter("pitch must be positive".to_string()));
        }
        source.pitch = pitch;
        Ok(())
    }

    /// Set source looping
    pub fn set_looping(&mut self, handle: AudioHandle, looping: bool) -> Result<(), AudioError> {
        let source = self.sources.get_mut(&handle)
            .ok_or(AudioError::SourceNotFound(handle.id()))?;

        source.looping = looping;
        Ok(())
    }

    /// Set source 3D position
    pub fn set_position(&mut self, handle: AudioHandle, position: [f32; 3]) -> Result<(), AudioError> {
        let source = self.sources.get_mut(&handle)
            .ok_or(AudioError::SourceNotFound(handle.id()))?;

        source.position = Some(position);
        Ok(())
    }

    /// Create an audio channel
    pub fn create_channel(&mut self, name: impl Into<String>) -> &mut AudioChannel {
        let name = name.into();
        self.channels.entry(name.clone())
            .or_insert_with(|| AudioChannel::new(name))
    }

    /// Get a channel
    pub fn get_channel(&self, name: &str) -> Option<&AudioChannel> {
        self.channels.get(name)
    }

    /// Get a mutable channel
    pub fn get_channel_mut(&mut self, name: &str) -> Option<&mut AudioChannel> {
        self.channels.get_mut(name)
    }

    /// Set channel volume
    pub fn set_channel_volume(&mut self, name: &str, volume: f32) -> Result<(), AudioError> {
        let channel = self.channels.get_mut(name)
            .ok_or_else(|| AudioError::ChannelNotFound(name.to_string()))?;

        channel.volume = volume.clamp(0.0, 1.0);
        Ok(())
    }

    /// Mute a channel
    pub fn mute_channel(&mut self, name: &str, muted: bool) -> Result<(), AudioError> {
        let channel = self.channels.get_mut(name)
            .ok_or_else(|| AudioError::ChannelNotFound(name.to_string()))?;

        channel.muted = muted;
        Ok(())
    }

    /// Set master volume
    pub fn set_master_volume(&mut self, volume: f32) {
        self.config.master_volume = volume.clamp(0.0, 1.0);
    }

    /// Get master volume
    pub fn master_volume(&self) -> f32 {
        self.config.master_volume
    }

    /// Set listener position (for spatial audio)
    pub fn set_listener_position(&mut self, position: [f32; 3]) {
        self.config.listener_position = position;
    }

    /// Get statistics
    pub fn stats(&self) -> &AudioStats {
        &self.stats
    }

    /// Stop all audio
    pub fn stop_all(&mut self) {
        for source in self.sources.values_mut() {
            source.state = AudioState::Stopped;
            source.progress = 0.0;
        }
        self.update_stats();
    }

    /// Update statistics
    fn update_stats(&mut self) {
        self.stats.playing_sources = self.sources.values()
            .filter(|s| s.state == AudioState::Playing)
            .count();
    }

    /// Get source count
    pub fn source_count(&self) -> usize {
        self.sources.len()
    }

    /// Get playing source count
    pub fn playing_count(&self) -> usize {
        self.stats.playing_sources
    }
}

impl Service for AudioService {
    fn id(&self) -> &ServiceId {
        &self.config.service.id
    }

    fn state(&self) -> ServiceState {
        self.state
    }

    fn health(&self) -> ServiceHealth {
        if self.state == ServiceState::Running {
            let mut health = ServiceHealth::healthy();
            health.metrics.insert("sources".to_string(), self.sources.len() as f64);
            health.metrics.insert("playing".to_string(), self.stats.playing_sources as f64);
            health.metrics.insert("master_volume".to_string(), self.config.master_volume as f64);
            health
        } else {
            ServiceHealth::default()
        }
    }

    fn config(&self) -> &ServiceConfig {
        &self.config.service
    }

    fn start(&mut self) -> ServiceResult<()> {
        self.state = ServiceState::Running;
        Ok(())
    }

    fn stop(&mut self) -> ServiceResult<()> {
        self.stop_all();
        self.sources.clear();
        self.state = ServiceState::Stopped;
        Ok(())
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_audio_handle() {
        let h1 = AudioHandle::new(1);
        let h2 = AudioHandle::new(2);
        assert_eq!(h1.id(), 1);
        assert_ne!(h1, h2);
    }

    #[test]
    fn test_create_source() {
        let mut service = AudioService::with_defaults();
        service.start().unwrap();

        let handle = service.create_source(AudioSourceType::Effect).unwrap();
        assert!(service.get_source(handle).is_some());

        let source = service.get_source(handle).unwrap();
        assert_eq!(source.source_type, AudioSourceType::Effect);
        assert_eq!(source.state, AudioState::Stopped);
    }

    #[test]
    fn test_play_pause_stop() {
        let mut service = AudioService::with_defaults();
        service.start().unwrap();

        let handle = service.create_source(AudioSourceType::Music).unwrap();

        service.play(handle).unwrap();
        assert_eq!(service.get_source(handle).unwrap().state, AudioState::Playing);

        service.pause(handle).unwrap();
        assert_eq!(service.get_source(handle).unwrap().state, AudioState::Paused);

        service.stop_source(handle).unwrap();
        assert_eq!(service.get_source(handle).unwrap().state, AudioState::Stopped);
    }

    #[test]
    fn test_source_properties() {
        let mut service = AudioService::with_defaults();
        service.start().unwrap();

        let handle = service.create_source(AudioSourceType::Effect).unwrap();

        service.set_volume(handle, 0.5).unwrap();
        assert!((service.get_source(handle).unwrap().volume - 0.5).abs() < 0.001);

        service.set_pan(handle, -0.5).unwrap();
        assert!((service.get_source(handle).unwrap().pan - (-0.5)).abs() < 0.001);

        service.set_pitch(handle, 1.5).unwrap();
        assert!((service.get_source(handle).unwrap().pitch - 1.5).abs() < 0.001);

        service.set_looping(handle, true).unwrap();
        assert!(service.get_source(handle).unwrap().looping);

        service.set_position(handle, [1.0, 2.0, 3.0]).unwrap();
        assert_eq!(service.get_source(handle).unwrap().position, Some([1.0, 2.0, 3.0]));
    }

    #[test]
    fn test_volume_clamping() {
        let mut service = AudioService::with_defaults();
        service.start().unwrap();

        let handle = service.create_source(AudioSourceType::Effect).unwrap();

        service.set_volume(handle, 2.0).unwrap();
        assert!((service.get_source(handle).unwrap().volume - 1.0).abs() < 0.001);

        service.set_volume(handle, -1.0).unwrap();
        assert!(service.get_source(handle).unwrap().volume.abs() < 0.001);
    }

    #[test]
    fn test_channels() {
        let mut service = AudioService::with_defaults();
        service.start().unwrap();

        // Default channels exist
        assert!(service.get_channel("master").is_some());
        assert!(service.get_channel("music").is_some());

        // Create custom channel
        service.create_channel("custom");
        assert!(service.get_channel("custom").is_some());

        // Set channel volume
        service.set_channel_volume("custom", 0.5).unwrap();
        assert!((service.get_channel("custom").unwrap().volume - 0.5).abs() < 0.001);

        // Mute channel
        service.mute_channel("music", true).unwrap();
        assert!(service.get_channel("music").unwrap().muted);
    }

    #[test]
    fn test_master_volume() {
        let mut service = AudioService::with_defaults();

        service.set_master_volume(0.7);
        assert!((service.master_volume() - 0.7).abs() < 0.001);

        // Clamping
        service.set_master_volume(2.0);
        assert!((service.master_volume() - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_stop_all() {
        let mut service = AudioService::with_defaults();
        service.start().unwrap();

        let h1 = service.create_source(AudioSourceType::Effect).unwrap();
        let h2 = service.create_source(AudioSourceType::Music).unwrap();

        service.play(h1).unwrap();
        service.play(h2).unwrap();
        assert_eq!(service.playing_count(), 2);

        service.stop_all();
        assert_eq!(service.playing_count(), 0);
    }

    #[test]
    fn test_max_sources() {
        let config = AudioServiceConfig {
            max_sources: 2,
            ..Default::default()
        };

        let mut service = AudioService::new(config);
        service.start().unwrap();

        service.create_source(AudioSourceType::Effect).unwrap();
        service.create_source(AudioSourceType::Effect).unwrap();

        let result = service.create_source(AudioSourceType::Effect);
        assert!(matches!(result, Err(AudioError::MaxSources)));
    }

    #[test]
    fn test_service_lifecycle() {
        let mut service = AudioService::with_defaults();

        assert_eq!(service.state(), ServiceState::Stopped);

        service.start().unwrap();
        assert_eq!(service.state(), ServiceState::Running);

        let handle = service.create_source(AudioSourceType::Effect).unwrap();
        service.play(handle).unwrap();

        service.stop().unwrap();
        assert_eq!(service.state(), ServiceState::Stopped);
        assert_eq!(service.source_count(), 0);
    }
}
