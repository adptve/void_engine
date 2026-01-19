//! Checkpoint system

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// A checkpoint in the game
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Checkpoint {
    /// Unique identifier
    pub id: String,
    /// Display name
    pub name: String,
    /// Level/area this checkpoint is in
    pub level: String,
    /// Spawn position
    pub position: [f32; 3],
    /// Spawn rotation (yaw in degrees)
    pub rotation: f32,
    /// Whether this checkpoint has been activated
    pub activated: bool,
    /// Time when activated (Unix timestamp)
    pub activated_time: Option<u64>,
    /// Associated data
    pub data: HashMap<String, serde_json::Value>,
}

impl Checkpoint {
    /// Create a new checkpoint
    pub fn new(id: impl Into<String>, level: impl Into<String>) -> Self {
        Self {
            id: id.into(),
            name: String::new(),
            level: level.into(),
            position: [0.0, 0.0, 0.0],
            rotation: 0.0,
            activated: false,
            activated_time: None,
            data: HashMap::new(),
        }
    }

    /// Set display name
    pub fn with_name(mut self, name: impl Into<String>) -> Self {
        self.name = name.into();
        self
    }

    /// Set spawn position
    pub fn with_position(mut self, position: [f32; 3]) -> Self {
        self.position = position;
        self
    }

    /// Set spawn rotation
    pub fn with_rotation(mut self, rotation: f32) -> Self {
        self.rotation = rotation;
        self
    }

    /// Set data
    pub fn with_data(mut self, key: impl Into<String>, value: serde_json::Value) -> Self {
        self.data.insert(key.into(), value);
        self
    }

    /// Activate this checkpoint
    pub fn activate(&mut self) {
        self.activated = true;
        self.activated_time = Some(
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_secs())
                .unwrap_or(0),
        );
    }

    /// Deactivate this checkpoint
    pub fn deactivate(&mut self) {
        self.activated = false;
    }
}

impl Default for Checkpoint {
    fn default() -> Self {
        Self::new("default", "default")
    }
}

/// Checkpoint manager
#[derive(Debug, Default)]
pub struct CheckpointManager {
    /// All checkpoints
    checkpoints: HashMap<String, Checkpoint>,
    /// Current active checkpoint
    current: Option<String>,
    /// Level checkpoints grouped by level
    level_checkpoints: HashMap<String, Vec<String>>,
}

impl CheckpointManager {
    /// Create a new checkpoint manager
    pub fn new() -> Self {
        Self::default()
    }

    /// Register a checkpoint
    pub fn register(&mut self, checkpoint: Checkpoint) {
        let level = checkpoint.level.clone();
        let id = checkpoint.id.clone();

        self.checkpoints.insert(id.clone(), checkpoint);

        self.level_checkpoints
            .entry(level)
            .or_default()
            .push(id);
    }

    /// Get a checkpoint
    pub fn get(&self, id: &str) -> Option<&Checkpoint> {
        self.checkpoints.get(id)
    }

    /// Get mutable checkpoint
    pub fn get_mut(&mut self, id: &str) -> Option<&mut Checkpoint> {
        self.checkpoints.get_mut(id)
    }

    /// Activate a checkpoint
    pub fn activate(&mut self, id: &str) -> bool {
        if let Some(checkpoint) = self.checkpoints.get_mut(id) {
            checkpoint.activate();
            self.current = Some(id.to_string());
            true
        } else {
            false
        }
    }

    /// Get current checkpoint
    pub fn current(&self) -> Option<&Checkpoint> {
        self.current.as_ref().and_then(|id| self.checkpoints.get(id))
    }

    /// Get current checkpoint ID
    pub fn current_id(&self) -> Option<&str> {
        self.current.as_deref()
    }

    /// Get all checkpoints for a level
    pub fn level_checkpoints(&self, level: &str) -> Vec<&Checkpoint> {
        self.level_checkpoints
            .get(level)
            .map(|ids| {
                ids.iter()
                    .filter_map(|id| self.checkpoints.get(id))
                    .collect()
            })
            .unwrap_or_default()
    }

    /// Get all activated checkpoints
    pub fn activated_checkpoints(&self) -> Vec<&Checkpoint> {
        self.checkpoints
            .values()
            .filter(|c| c.activated)
            .collect()
    }

    /// Get activated checkpoints for a level
    pub fn activated_in_level(&self, level: &str) -> Vec<&Checkpoint> {
        self.level_checkpoints(level)
            .into_iter()
            .filter(|c| c.activated)
            .collect()
    }

    /// Reset all checkpoints
    pub fn reset_all(&mut self) {
        for checkpoint in self.checkpoints.values_mut() {
            checkpoint.deactivate();
        }
        self.current = None;
    }

    /// Reset checkpoints for a specific level
    pub fn reset_level(&mut self, level: &str) {
        if let Some(ids) = self.level_checkpoints.get(level) {
            for id in ids {
                if let Some(checkpoint) = self.checkpoints.get_mut(id) {
                    checkpoint.deactivate();
                    if self.current.as_ref() == Some(id) {
                        self.current = None;
                    }
                }
            }
        }
    }

    /// Get spawn info from current checkpoint
    pub fn get_spawn(&self) -> Option<([f32; 3], f32)> {
        self.current().map(|c| (c.position, c.rotation))
    }

    /// Get all checkpoint IDs
    pub fn all_ids(&self) -> impl Iterator<Item = &str> {
        self.checkpoints.keys().map(|s| s.as_str())
    }

    /// Get checkpoint count
    pub fn count(&self) -> usize {
        self.checkpoints.len()
    }

    /// Get activated count
    pub fn activated_count(&self) -> usize {
        self.checkpoints.values().filter(|c| c.activated).count()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_checkpoint() {
        let mut checkpoint = Checkpoint::new("cp1", "level1")
            .with_name("Forest Entrance")
            .with_position([10.0, 0.0, 20.0]);

        assert_eq!(checkpoint.id, "cp1");
        assert!(!checkpoint.activated);

        checkpoint.activate();
        assert!(checkpoint.activated);
        assert!(checkpoint.activated_time.is_some());
    }

    #[test]
    fn test_checkpoint_manager() {
        let mut manager = CheckpointManager::new();

        manager.register(
            Checkpoint::new("l1_start", "level1")
                .with_position([0.0, 0.0, 0.0]),
        );
        manager.register(
            Checkpoint::new("l1_mid", "level1")
                .with_position([50.0, 0.0, 0.0]),
        );
        manager.register(
            Checkpoint::new("l2_start", "level2")
                .with_position([0.0, 0.0, 0.0]),
        );

        assert_eq!(manager.count(), 3);
        assert_eq!(manager.level_checkpoints("level1").len(), 2);

        manager.activate("l1_mid");
        assert_eq!(manager.current_id(), Some("l1_mid"));

        let spawn = manager.get_spawn();
        assert!(spawn.is_some());
        assert_eq!(spawn.unwrap().0, [50.0, 0.0, 0.0]);
    }

    #[test]
    fn test_reset() {
        let mut manager = CheckpointManager::new();

        manager.register(Checkpoint::new("cp1", "level1"));
        manager.register(Checkpoint::new("cp2", "level1"));

        manager.activate("cp1");
        manager.activate("cp2");

        assert_eq!(manager.activated_count(), 2);

        manager.reset_level("level1");
        assert_eq!(manager.activated_count(), 0);
        assert!(manager.current_id().is_none());
    }
}
