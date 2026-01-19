//! Audio channels for mixing

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Channel identifier
pub type ChannelId = String;

/// Audio channel for grouping and mixing sounds
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioChannel {
    /// Channel name/ID
    pub name: String,
    /// Volume (0.0 - 1.0)
    pub volume: f32,
    /// Whether channel is muted
    pub muted: bool,
    /// Whether channel is paused
    pub paused: bool,
    /// Parent channel (for hierarchical mixing)
    pub parent: Option<ChannelId>,
}

impl AudioChannel {
    /// Create a new channel
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            volume: 1.0,
            muted: false,
            paused: false,
            parent: None,
        }
    }

    /// Set volume
    pub fn with_volume(mut self, volume: f32) -> Self {
        self.volume = volume.clamp(0.0, 1.0);
        self
    }

    /// Set parent channel
    pub fn with_parent(mut self, parent: impl Into<ChannelId>) -> Self {
        self.parent = Some(parent.into());
        self
    }

    /// Get effective volume (considering mute)
    pub fn effective_volume(&self) -> f32 {
        if self.muted {
            0.0
        } else {
            self.volume
        }
    }
}

impl Default for AudioChannel {
    fn default() -> Self {
        Self::new("default")
    }
}

/// Channel manager
#[derive(Debug, Default)]
pub struct ChannelManager {
    /// Registered channels
    channels: HashMap<ChannelId, AudioChannel>,
    /// Master volume
    master_volume: f32,
    /// Master mute
    master_muted: bool,
}

impl ChannelManager {
    /// Create a new channel manager
    pub fn new() -> Self {
        let mut manager = Self {
            channels: HashMap::new(),
            master_volume: 1.0,
            master_muted: false,
        };

        // Create default channels
        manager.register(AudioChannel::new("master"));
        manager.register(AudioChannel::new("sfx").with_parent("master"));
        manager.register(AudioChannel::new("music").with_parent("master"));
        manager.register(AudioChannel::new("voice").with_parent("master"));
        manager.register(AudioChannel::new("ambient").with_parent("master"));
        manager.register(AudioChannel::new("ui").with_parent("master"));

        manager
    }

    /// Register a channel
    pub fn register(&mut self, channel: AudioChannel) {
        self.channels.insert(channel.name.clone(), channel);
    }

    /// Get a channel
    pub fn get(&self, name: &str) -> Option<&AudioChannel> {
        self.channels.get(name)
    }

    /// Get mutable channel
    pub fn get_mut(&mut self, name: &str) -> Option<&mut AudioChannel> {
        self.channels.get_mut(name)
    }

    /// Set channel volume
    pub fn set_volume(&mut self, name: &str, volume: f32) {
        if let Some(channel) = self.channels.get_mut(name) {
            channel.volume = volume.clamp(0.0, 1.0);
        }
    }

    /// Set channel mute state
    pub fn set_muted(&mut self, name: &str, muted: bool) {
        if let Some(channel) = self.channels.get_mut(name) {
            channel.muted = muted;
        }
    }

    /// Set channel paused state
    pub fn set_paused(&mut self, name: &str, paused: bool) {
        if let Some(channel) = self.channels.get_mut(name) {
            channel.paused = paused;
        }
    }

    /// Set master volume
    pub fn set_master_volume(&mut self, volume: f32) {
        self.master_volume = volume.clamp(0.0, 1.0);
    }

    /// Get master volume
    pub fn master_volume(&self) -> f32 {
        self.master_volume
    }

    /// Set master mute
    pub fn set_master_muted(&mut self, muted: bool) {
        self.master_muted = muted;
    }

    /// Check if master is muted
    pub fn is_master_muted(&self) -> bool {
        self.master_muted
    }

    /// Calculate final volume for a channel (including parent chain)
    pub fn calculate_volume(&self, channel_name: &str) -> f32 {
        if self.master_muted {
            return 0.0;
        }

        let mut volume = self.master_volume;

        if let Some(channel) = self.channels.get(channel_name) {
            volume *= self.calculate_channel_volume(channel);
        }

        volume
    }

    /// Calculate channel volume recursively through parent chain
    fn calculate_channel_volume(&self, channel: &AudioChannel) -> f32 {
        let mut volume = channel.effective_volume();

        if let Some(ref parent_name) = channel.parent {
            if let Some(parent) = self.channels.get(parent_name) {
                volume *= self.calculate_channel_volume(parent);
            }
        }

        volume
    }

    /// Check if channel is paused (including parent chain)
    pub fn is_paused(&self, channel_name: &str) -> bool {
        if let Some(channel) = self.channels.get(channel_name) {
            if channel.paused {
                return true;
            }
            if let Some(ref parent_name) = channel.parent {
                return self.is_paused(parent_name);
            }
        }
        false
    }

    /// Get all channel names
    pub fn channel_names(&self) -> impl Iterator<Item = &str> {
        self.channels.keys().map(|s| s.as_str())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_channel_creation() {
        let channel = AudioChannel::new("effects").with_volume(0.8);

        assert_eq!(channel.name, "effects");
        assert_eq!(channel.volume, 0.8);
        assert!(!channel.muted);
    }

    #[test]
    fn test_channel_manager() {
        let manager = ChannelManager::new();

        assert!(manager.get("master").is_some());
        assert!(manager.get("sfx").is_some());
        assert!(manager.get("music").is_some());
    }

    #[test]
    fn test_volume_calculation() {
        let mut manager = ChannelManager::new();

        manager.set_master_volume(0.5);
        manager.set_volume("sfx", 0.8);

        let volume = manager.calculate_volume("sfx");
        // master (0.5) * master channel (1.0) * sfx (0.8)
        assert!((volume - 0.4).abs() < 0.001);
    }

    #[test]
    fn test_muting() {
        let mut manager = ChannelManager::new();

        manager.set_muted("sfx", true);
        let volume = manager.calculate_volume("sfx");
        assert_eq!(volume, 0.0);

        manager.set_master_muted(true);
        let volume = manager.calculate_volume("music");
        assert_eq!(volume, 0.0);
    }

    #[test]
    fn test_pause_hierarchy() {
        let mut manager = ChannelManager::new();

        manager.set_paused("master", true);
        assert!(manager.is_paused("sfx")); // Child is also paused
        assert!(manager.is_paused("music"));
    }
}
