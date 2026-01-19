//! Void Audio - Audio System
//!
//! This crate provides audio playback using rodio.
//!
//! # Features
//!
//! - Sound effect playback (one-shot and looping)
//! - Music system with crossfade
//! - 3D spatial audio
//! - Audio channels with independent volume
//! - Audio source component for entities
//!
//! # Example
//!
//! ```ignore
//! use void_audio::prelude::*;
//!
//! // Initialize audio system
//! let mut audio = AudioSystem::new()?;
//!
//! // Play a sound
//! audio.play_sound("explosion.wav", SoundSettings::default());
//!
//! // Play music
//! audio.play_music("background.mp3", MusicSettings::default().with_fade_in(2.0));
//! ```

pub mod channel;
pub mod settings;
pub mod source;
pub mod system;

pub mod prelude {
    pub use crate::channel::{AudioChannel, ChannelId};
    pub use crate::settings::{MusicSettings, SoundSettings, SpatialSettings};
    pub use crate::source::{AudioSourceComponent, PlaybackState};
    pub use crate::system::{AudioError, AudioSystem, SoundHandle};
}

pub use prelude::*;
