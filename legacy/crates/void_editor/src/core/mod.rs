//! Core editor types and state management.
//!
//! This module contains the central `EditorState` and supporting types
//! that form the foundation of the editor.

pub mod editor_state;
mod selection;
mod history;
mod preferences;

pub use editor_state::{
    EditorState,
    MeshType,
    Transform,
    SceneEntity,
    NameComponent,
    TransformComponent,
    MeshComponent,
    DraggedAsset,
};
pub use selection::{SelectionManager, SelectionMode};
pub use history::{UndoHistory, Transaction};
pub use preferences::EditorPreferences;

use std::path::PathBuf;

/// Entity identifier used throughout the editor.
/// Maps to void_ecs Entity internally.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct EntityId(pub u32);

impl std::fmt::Display for EntityId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Entity({})", self.0)
    }
}

/// Recent files list with LRU behavior.
#[derive(Clone, Debug, Default)]
pub struct RecentFiles {
    files: std::collections::VecDeque<PathBuf>,
    max_entries: usize,
}

impl RecentFiles {
    const DEFAULT_MAX: usize = 10;

    pub fn new() -> Self {
        Self {
            files: std::collections::VecDeque::new(),
            max_entries: Self::DEFAULT_MAX,
        }
    }

    pub fn with_capacity(max: usize) -> Self {
        Self {
            files: std::collections::VecDeque::new(),
            max_entries: max,
        }
    }

    pub fn add(&mut self, path: PathBuf) {
        // Remove if already exists (move to front)
        self.files.retain(|p| p != &path);
        // Add to front
        self.files.push_front(path);
        // Limit size
        while self.files.len() > self.max_entries {
            self.files.pop_back();
        }
    }

    pub fn clear(&mut self) {
        self.files.clear();
    }

    pub fn files(&self) -> impl Iterator<Item = &PathBuf> {
        self.files.iter()
    }

    pub fn is_empty(&self) -> bool {
        self.files.is_empty()
    }

    pub fn len(&self) -> usize {
        self.files.len()
    }
}
