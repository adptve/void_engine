//! Audio playback settings

use serde::{Deserialize, Serialize};

/// Settings for sound effect playback
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SoundSettings {
    /// Volume (0.0 - 1.0)
    pub volume: f32,
    /// Playback speed multiplier
    pub speed: f32,
    /// Whether to loop the sound
    pub looping: bool,
    /// Start position in seconds
    pub start_position: f32,
    /// Spatial settings (None for 2D/non-spatial)
    pub spatial: Option<SpatialSettings>,
    /// Channel to play on (None for default)
    pub channel: Option<String>,
    /// Priority (higher = more important, won't be culled)
    pub priority: u8,
}

impl SoundSettings {
    /// Create default settings
    pub fn new() -> Self {
        Self::default()
    }

    /// Set volume
    pub fn with_volume(mut self, volume: f32) -> Self {
        self.volume = volume.clamp(0.0, 1.0);
        self
    }

    /// Set playback speed
    pub fn with_speed(mut self, speed: f32) -> Self {
        self.speed = speed.max(0.1);
        self
    }

    /// Enable looping
    pub fn with_looping(mut self) -> Self {
        self.looping = true;
        self
    }

    /// Set start position
    pub fn with_start(mut self, position: f32) -> Self {
        self.start_position = position.max(0.0);
        self
    }

    /// Set spatial settings for 3D audio
    pub fn with_spatial(mut self, settings: SpatialSettings) -> Self {
        self.spatial = Some(settings);
        self
    }

    /// Set channel
    pub fn with_channel(mut self, channel: impl Into<String>) -> Self {
        self.channel = Some(channel.into());
        self
    }

    /// Set priority
    pub fn with_priority(mut self, priority: u8) -> Self {
        self.priority = priority;
        self
    }
}

impl Default for SoundSettings {
    fn default() -> Self {
        Self {
            volume: 1.0,
            speed: 1.0,
            looping: false,
            start_position: 0.0,
            spatial: None,
            channel: None,
            priority: 128,
        }
    }
}

/// Settings for music playback
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MusicSettings {
    /// Volume (0.0 - 1.0)
    pub volume: f32,
    /// Fade in duration in seconds
    pub fade_in: f32,
    /// Fade out duration for previous track
    pub fade_out: f32,
    /// Whether to loop the music
    pub looping: bool,
    /// Start position in seconds
    pub start_position: f32,
    /// Crossfade with previous track
    pub crossfade: bool,
}

impl MusicSettings {
    /// Create default settings
    pub fn new() -> Self {
        Self::default()
    }

    /// Set volume
    pub fn with_volume(mut self, volume: f32) -> Self {
        self.volume = volume.clamp(0.0, 1.0);
        self
    }

    /// Set fade in duration
    pub fn with_fade_in(mut self, duration: f32) -> Self {
        self.fade_in = duration.max(0.0);
        self
    }

    /// Set fade out duration
    pub fn with_fade_out(mut self, duration: f32) -> Self {
        self.fade_out = duration.max(0.0);
        self
    }

    /// Enable looping
    pub fn with_looping(mut self) -> Self {
        self.looping = true;
        self
    }

    /// Set start position
    pub fn with_start(mut self, position: f32) -> Self {
        self.start_position = position.max(0.0);
        self
    }

    /// Enable crossfade with previous track
    pub fn with_crossfade(mut self) -> Self {
        self.crossfade = true;
        self
    }
}

impl Default for MusicSettings {
    fn default() -> Self {
        Self {
            volume: 1.0,
            fade_in: 0.0,
            fade_out: 0.0,
            looping: true,
            start_position: 0.0,
            crossfade: false,
        }
    }
}

/// Spatial audio settings for 3D sound
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SpatialSettings {
    /// Position in world space
    pub position: [f32; 3],
    /// Velocity for doppler effect
    pub velocity: [f32; 3],
    /// Minimum distance (full volume)
    pub min_distance: f32,
    /// Maximum distance (sound cutoff)
    pub max_distance: f32,
    /// Distance attenuation model
    pub attenuation: AttenuationModel,
    /// Cone for directional sounds
    pub cone: Option<AudioCone>,
}

impl SpatialSettings {
    /// Create spatial settings at a position
    pub fn at(position: [f32; 3]) -> Self {
        Self {
            position,
            velocity: [0.0, 0.0, 0.0],
            min_distance: 1.0,
            max_distance: 100.0,
            attenuation: AttenuationModel::InverseDistance,
            cone: None,
        }
    }

    /// Set velocity
    pub fn with_velocity(mut self, velocity: [f32; 3]) -> Self {
        self.velocity = velocity;
        self
    }

    /// Set distance range
    pub fn with_distance(mut self, min: f32, max: f32) -> Self {
        self.min_distance = min.max(0.1);
        self.max_distance = max.max(min + 0.1);
        self
    }

    /// Set attenuation model
    pub fn with_attenuation(mut self, model: AttenuationModel) -> Self {
        self.attenuation = model;
        self
    }

    /// Set directional cone
    pub fn with_cone(mut self, cone: AudioCone) -> Self {
        self.cone = Some(cone);
        self
    }

    /// Calculate volume at given listener position
    pub fn calculate_volume(&self, listener_pos: [f32; 3]) -> f32 {
        let dx = self.position[0] - listener_pos[0];
        let dy = self.position[1] - listener_pos[1];
        let dz = self.position[2] - listener_pos[2];
        let distance = (dx * dx + dy * dy + dz * dz).sqrt();

        if distance <= self.min_distance {
            return 1.0;
        }
        if distance >= self.max_distance {
            return 0.0;
        }

        let t = (distance - self.min_distance) / (self.max_distance - self.min_distance);

        match self.attenuation {
            AttenuationModel::Linear => 1.0 - t,
            AttenuationModel::InverseDistance => self.min_distance / distance,
            AttenuationModel::ExponentialDistance => {
                let rolloff = 1.0; // Could be configurable
                (self.min_distance / distance).powf(rolloff)
            }
        }
    }

    /// Calculate stereo panning (-1 left, +1 right)
    pub fn calculate_pan(&self, listener_pos: [f32; 3], listener_forward: [f32; 3]) -> f32 {
        let dx = self.position[0] - listener_pos[0];
        let dz = self.position[2] - listener_pos[2];
        let dist = (dx * dx + dz * dz).sqrt();

        if dist < 0.01 {
            return 0.0;
        }

        // Calculate right vector (cross with up)
        let right_x = listener_forward[2];
        let right_z = -listener_forward[0];

        // Dot product with direction to source
        let to_source_x = dx / dist;
        let to_source_z = dz / dist;
        let pan = right_x * to_source_x + right_z * to_source_z;

        pan.clamp(-1.0, 1.0)
    }
}

impl Default for SpatialSettings {
    fn default() -> Self {
        Self::at([0.0, 0.0, 0.0])
    }
}

/// Distance attenuation model
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum AttenuationModel {
    /// Linear falloff
    Linear,
    /// Inverse distance falloff (realistic)
    InverseDistance,
    /// Exponential falloff
    ExponentialDistance,
}

impl Default for AttenuationModel {
    fn default() -> Self {
        Self::InverseDistance
    }
}

/// Directional audio cone
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioCone {
    /// Direction the cone points
    pub direction: [f32; 3],
    /// Inner cone angle (full volume) in degrees
    pub inner_angle: f32,
    /// Outer cone angle (attenuated) in degrees
    pub outer_angle: f32,
    /// Volume multiplier at outer angle
    pub outer_gain: f32,
}

impl AudioCone {
    /// Create a new audio cone
    pub fn new(direction: [f32; 3], inner_angle: f32, outer_angle: f32) -> Self {
        Self {
            direction,
            inner_angle,
            outer_angle,
            outer_gain: 0.0,
        }
    }

    /// Set outer gain
    pub fn with_outer_gain(mut self, gain: f32) -> Self {
        self.outer_gain = gain.clamp(0.0, 1.0);
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sound_settings() {
        let settings = SoundSettings::new()
            .with_volume(0.5)
            .with_looping()
            .with_priority(255);

        assert_eq!(settings.volume, 0.5);
        assert!(settings.looping);
        assert_eq!(settings.priority, 255);
    }

    #[test]
    fn test_music_settings() {
        let settings = MusicSettings::new()
            .with_fade_in(2.0)
            .with_crossfade();

        assert_eq!(settings.fade_in, 2.0);
        assert!(settings.crossfade);
    }

    #[test]
    fn test_spatial_volume() {
        let spatial = SpatialSettings::at([10.0, 0.0, 0.0]).with_distance(1.0, 100.0);

        // At source position
        let vol = spatial.calculate_volume([10.0, 0.0, 0.0]);
        assert_eq!(vol, 1.0);

        // At min distance
        let vol = spatial.calculate_volume([11.0, 0.0, 0.0]);
        assert_eq!(vol, 1.0);

        // Far away
        let vol = spatial.calculate_volume([110.0, 0.0, 0.0]);
        assert_eq!(vol, 0.0);
    }

    #[test]
    fn test_spatial_pan() {
        let spatial = SpatialSettings::at([10.0, 0.0, 0.0]);

        // Sound to the right
        let pan = spatial.calculate_pan([0.0, 0.0, 0.0], [0.0, 0.0, 1.0]);
        assert!(pan > 0.0);

        // Sound to the left
        let pan = spatial.calculate_pan([0.0, 0.0, 0.0], [0.0, 0.0, -1.0]);
        assert!(pan < 0.0);
    }
}
