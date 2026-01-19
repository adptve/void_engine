//! Audio source component for entities

use crate::settings::{SoundSettings, SpatialSettings};
use serde::{Deserialize, Serialize};

/// Playback state
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum PlaybackState {
    /// Not playing
    Stopped,
    /// Currently playing
    Playing,
    /// Paused
    Paused,
    /// Fading in
    FadingIn,
    /// Fading out
    FadingOut,
}

impl Default for PlaybackState {
    fn default() -> Self {
        Self::Stopped
    }
}

/// Audio source component for entities that emit sound
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioSourceComponent {
    /// Sound file path
    pub clip: String,
    /// Sound settings
    pub settings: SoundSettings,
    /// Current playback state
    #[serde(skip)]
    pub state: PlaybackState,
    /// Whether to play on spawn
    pub play_on_awake: bool,
    /// Current playback position in seconds
    #[serde(skip)]
    pub position: f32,
    /// Duration of the clip in seconds
    #[serde(skip)]
    pub duration: f32,
    /// Internal handle ID
    #[serde(skip)]
    pub handle_id: Option<u64>,
    /// Fade progress (0.0 - 1.0)
    #[serde(skip)]
    pub fade_progress: f32,
    /// Target volume for fade
    #[serde(skip)]
    pub fade_target: f32,
    /// Fade duration
    #[serde(skip)]
    pub fade_duration: f32,
}

impl AudioSourceComponent {
    /// Create a new audio source
    pub fn new(clip: impl Into<String>) -> Self {
        Self {
            clip: clip.into(),
            settings: SoundSettings::default(),
            state: PlaybackState::Stopped,
            play_on_awake: false,
            position: 0.0,
            duration: 0.0,
            handle_id: None,
            fade_progress: 1.0,
            fade_target: 1.0,
            fade_duration: 0.0,
        }
    }

    /// Set settings
    pub fn with_settings(mut self, settings: SoundSettings) -> Self {
        self.settings = settings;
        self
    }

    /// Set volume
    pub fn with_volume(mut self, volume: f32) -> Self {
        self.settings.volume = volume.clamp(0.0, 1.0);
        self
    }

    /// Enable looping
    pub fn with_looping(mut self) -> Self {
        self.settings.looping = true;
        self
    }

    /// Enable play on awake
    pub fn play_on_awake(mut self) -> Self {
        self.play_on_awake = true;
        self
    }

    /// Set spatial audio
    pub fn with_spatial(mut self, position: [f32; 3]) -> Self {
        self.settings.spatial = Some(SpatialSettings::at(position));
        self
    }

    /// Set spatial settings
    pub fn with_spatial_settings(mut self, spatial: SpatialSettings) -> Self {
        self.settings.spatial = Some(spatial);
        self
    }

    /// Set channel
    pub fn with_channel(mut self, channel: impl Into<String>) -> Self {
        self.settings.channel = Some(channel.into());
        self
    }

    /// Check if playing
    pub fn is_playing(&self) -> bool {
        matches!(
            self.state,
            PlaybackState::Playing | PlaybackState::FadingIn | PlaybackState::FadingOut
        )
    }

    /// Check if stopped
    pub fn is_stopped(&self) -> bool {
        self.state == PlaybackState::Stopped
    }

    /// Check if paused
    pub fn is_paused(&self) -> bool {
        self.state == PlaybackState::Paused
    }

    /// Get playback progress (0.0 - 1.0)
    pub fn progress(&self) -> f32 {
        if self.duration <= 0.0 {
            0.0
        } else {
            (self.position / self.duration).clamp(0.0, 1.0)
        }
    }

    /// Update position (for 3D audio)
    pub fn set_position(&mut self, position: [f32; 3]) {
        if let Some(ref mut spatial) = self.settings.spatial {
            spatial.position = position;
        }
    }

    /// Update velocity (for doppler effect)
    pub fn set_velocity(&mut self, velocity: [f32; 3]) {
        if let Some(ref mut spatial) = self.settings.spatial {
            spatial.velocity = velocity;
        }
    }

    /// Start fade in
    pub fn fade_in(&mut self, duration: f32) {
        self.fade_progress = 0.0;
        self.fade_target = self.settings.volume;
        self.fade_duration = duration;
        self.state = PlaybackState::FadingIn;
    }

    /// Start fade out
    pub fn fade_out(&mut self, duration: f32) {
        self.fade_progress = 0.0;
        self.fade_target = 0.0;
        self.fade_duration = duration;
        self.state = PlaybackState::FadingOut;
    }

    /// Update fade (returns current volume multiplier)
    pub fn update_fade(&mut self, delta_time: f32) -> f32 {
        if self.fade_duration <= 0.0 {
            return 1.0;
        }

        self.fade_progress += delta_time / self.fade_duration;

        if self.fade_progress >= 1.0 {
            self.fade_progress = 1.0;

            match self.state {
                PlaybackState::FadingIn => {
                    self.state = PlaybackState::Playing;
                }
                PlaybackState::FadingOut => {
                    self.state = PlaybackState::Stopped;
                }
                _ => {}
            }

            return self.fade_target / self.settings.volume.max(0.001);
        }

        match self.state {
            PlaybackState::FadingIn => self.fade_progress,
            PlaybackState::FadingOut => 1.0 - self.fade_progress,
            _ => 1.0,
        }
    }

    /// Get effective volume (considering fades)
    pub fn effective_volume(&self) -> f32 {
        let fade_mult = match self.state {
            PlaybackState::FadingIn => self.fade_progress,
            PlaybackState::FadingOut => 1.0 - self.fade_progress,
            _ => 1.0,
        };
        self.settings.volume * fade_mult
    }
}

impl Default for AudioSourceComponent {
    fn default() -> Self {
        Self::new("")
    }
}

/// Audio listener component (typically attached to camera/player)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioListenerComponent {
    /// Whether this is the active listener
    pub active: bool,
    /// Position in world space
    #[serde(skip)]
    pub position: [f32; 3],
    /// Forward direction
    #[serde(skip)]
    pub forward: [f32; 3],
    /// Up direction
    #[serde(skip)]
    pub up: [f32; 3],
    /// Velocity for doppler
    #[serde(skip)]
    pub velocity: [f32; 3],
}

impl AudioListenerComponent {
    /// Create a new audio listener
    pub fn new() -> Self {
        Self {
            active: true,
            position: [0.0, 0.0, 0.0],
            forward: [0.0, 0.0, 1.0],
            up: [0.0, 1.0, 0.0],
            velocity: [0.0, 0.0, 0.0],
        }
    }

    /// Update transform
    pub fn set_transform(
        &mut self,
        position: [f32; 3],
        forward: [f32; 3],
        up: [f32; 3],
    ) {
        self.position = position;
        self.forward = forward;
        self.up = up;
    }

    /// Set velocity
    pub fn set_velocity(&mut self, velocity: [f32; 3]) {
        self.velocity = velocity;
    }
}

impl Default for AudioListenerComponent {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_audio_source() {
        let source = AudioSourceComponent::new("explosion.wav")
            .with_volume(0.8)
            .with_looping()
            .play_on_awake();

        assert_eq!(source.clip, "explosion.wav");
        assert_eq!(source.settings.volume, 0.8);
        assert!(source.settings.looping);
        assert!(source.play_on_awake);
    }

    #[test]
    fn test_spatial_source() {
        let mut source = AudioSourceComponent::new("footstep.wav")
            .with_spatial([10.0, 0.0, 0.0]);

        assert!(source.settings.spatial.is_some());

        source.set_position([20.0, 0.0, 0.0]);
        assert_eq!(source.settings.spatial.as_ref().unwrap().position[0], 20.0);
    }

    #[test]
    fn test_fade() {
        let mut source = AudioSourceComponent::new("music.mp3")
            .with_volume(1.0);

        source.fade_in(1.0);
        assert_eq!(source.state, PlaybackState::FadingIn);

        // Simulate half fade
        source.fade_progress = 0.5;
        assert!((source.effective_volume() - 0.5).abs() < 0.001);
    }

    #[test]
    fn test_listener() {
        let mut listener = AudioListenerComponent::new();

        listener.set_transform([1.0, 2.0, 3.0], [0.0, 0.0, 1.0], [0.0, 1.0, 0.0]);

        assert_eq!(listener.position, [1.0, 2.0, 3.0]);
        assert_eq!(listener.forward, [0.0, 0.0, 1.0]);
    }
}
