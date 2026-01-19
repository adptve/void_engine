//! Void GameState - Game State Management
//!
//! This crate provides game state management and save/load functionality.
//!
//! # Features
//!
//! - Game state machine (menu, playing, paused, etc.)
//! - Save/load system with versioning
//! - Checkpoint system
//! - Game progress tracking
//! - Level state management
//!
//! # Example
//!
//! ```ignore
//! use void_gamestate::prelude::*;
//!
//! // Create game state manager
//! let mut state = GameStateManager::new();
//! state.push_state(GameState::MainMenu);
//!
//! // Save game
//! let save = SaveData::new("My Save")
//!     .with_checkpoint("level1_start");
//! save_manager.save("slot1", &save)?;
//! ```

pub mod checkpoint;
pub mod level;
pub mod progress;
pub mod save;
pub mod state;

pub mod prelude {
    pub use crate::checkpoint::{Checkpoint, CheckpointManager};
    pub use crate::level::{LevelState, LevelStatus};
    pub use crate::progress::{Achievement, ProgressTracker};
    pub use crate::save::{SaveData, SaveError, SaveManager, SaveSlot};
    pub use crate::state::{GameState, GameStateManager, StateTransition};
}

pub use prelude::*;
