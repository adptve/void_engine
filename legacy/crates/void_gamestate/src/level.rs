//! Level state management

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Level completion status
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum LevelStatus {
    /// Level not yet visited
    Locked,
    /// Level unlocked but not started
    Unlocked,
    /// Level in progress
    InProgress,
    /// Level completed
    Completed,
    /// Level completed with all objectives
    Mastered,
}

impl Default for LevelStatus {
    fn default() -> Self {
        Self::Locked
    }
}

impl LevelStatus {
    /// Check if level is playable
    pub fn is_playable(&self) -> bool {
        !matches!(self, Self::Locked)
    }

    /// Check if level has been completed
    pub fn is_completed(&self) -> bool {
        matches!(self, Self::Completed | Self::Mastered)
    }
}

/// Level objective
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LevelObjective {
    /// Objective ID
    pub id: String,
    /// Display name
    pub name: String,
    /// Description
    pub description: String,
    /// Whether this is a main objective
    pub required: bool,
    /// Whether objective is complete
    pub completed: bool,
    /// Progress (0.0 - 1.0 for progress-based objectives)
    pub progress: f32,
    /// Target value (for counting objectives)
    pub target: u32,
    /// Current value
    pub current: u32,
}

impl LevelObjective {
    /// Create a new objective
    pub fn new(id: impl Into<String>, name: impl Into<String>) -> Self {
        Self {
            id: id.into(),
            name: name.into(),
            description: String::new(),
            required: true,
            completed: false,
            progress: 0.0,
            target: 1,
            current: 0,
        }
    }

    /// Set as optional
    pub fn optional(mut self) -> Self {
        self.required = false;
        self
    }

    /// Set description
    pub fn with_description(mut self, desc: impl Into<String>) -> Self {
        self.description = desc.into();
        self
    }

    /// Set target count
    pub fn with_target(mut self, target: u32) -> Self {
        self.target = target;
        self
    }

    /// Update progress
    pub fn update(&mut self, current: u32) {
        self.current = current.min(self.target);
        self.progress = if self.target > 0 {
            self.current as f32 / self.target as f32
        } else {
            1.0
        };
        self.completed = self.current >= self.target;
    }

    /// Complete the objective
    pub fn complete(&mut self) {
        self.current = self.target;
        self.progress = 1.0;
        self.completed = true;
    }
}

/// State of a level/area
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LevelState {
    /// Level identifier
    pub id: String,
    /// Display name
    pub name: String,
    /// Current status
    pub status: LevelStatus,
    /// Objectives
    pub objectives: HashMap<String, LevelObjective>,
    /// Best completion time (seconds)
    pub best_time: Option<f32>,
    /// Number of times completed
    pub completions: u32,
    /// Collectibles found / total
    pub collectibles: (u32, u32),
    /// Secrets found / total
    pub secrets: (u32, u32),
    /// Star rating (0-3)
    pub stars: u8,
    /// Custom data
    pub data: HashMap<String, serde_json::Value>,
}

impl LevelState {
    /// Create a new level state
    pub fn new(id: impl Into<String>) -> Self {
        Self {
            id: id.into(),
            name: String::new(),
            status: LevelStatus::Locked,
            objectives: HashMap::new(),
            best_time: None,
            completions: 0,
            collectibles: (0, 0),
            secrets: (0, 0),
            stars: 0,
            data: HashMap::new(),
        }
    }

    /// Set display name
    pub fn with_name(mut self, name: impl Into<String>) -> Self {
        self.name = name.into();
        self
    }

    /// Set initial status
    pub fn with_status(mut self, status: LevelStatus) -> Self {
        self.status = status;
        self
    }

    /// Set collectible count
    pub fn with_collectibles(mut self, total: u32) -> Self {
        self.collectibles.1 = total;
        self
    }

    /// Set secret count
    pub fn with_secrets(mut self, total: u32) -> Self {
        self.secrets.1 = total;
        self
    }

    /// Add objective
    pub fn with_objective(mut self, objective: LevelObjective) -> Self {
        self.objectives.insert(objective.id.clone(), objective);
        self
    }

    /// Unlock the level
    pub fn unlock(&mut self) {
        if self.status == LevelStatus::Locked {
            self.status = LevelStatus::Unlocked;
        }
    }

    /// Start the level
    pub fn start(&mut self) {
        if self.status.is_playable() {
            self.status = LevelStatus::InProgress;
        }
    }

    /// Complete the level
    pub fn complete(&mut self, time: f32) {
        self.completions += 1;

        if self.best_time.map(|t| time < t).unwrap_or(true) {
            self.best_time = Some(time);
        }

        // Check if all objectives are complete
        let all_complete = self.objectives.values().all(|o| !o.required || o.completed);
        let all_optional = self.objectives.values().all(|o| o.completed);

        if all_optional {
            self.status = LevelStatus::Mastered;
        } else if all_complete {
            self.status = LevelStatus::Completed;
        }

        // Calculate stars
        self.calculate_stars();
    }

    /// Calculate star rating
    fn calculate_stars(&mut self) {
        let mut stars = 0u8;

        // Star for completion
        if self.status.is_completed() {
            stars += 1;
        }

        // Star for all collectibles
        if self.collectibles.0 >= self.collectibles.1 && self.collectibles.1 > 0 {
            stars += 1;
        }

        // Star for all objectives
        if self.objectives.values().all(|o| o.completed) {
            stars += 1;
        }

        self.stars = stars;
    }

    /// Update objective
    pub fn update_objective(&mut self, id: &str, current: u32) {
        if let Some(objective) = self.objectives.get_mut(id) {
            objective.update(current);
        }
    }

    /// Complete objective
    pub fn complete_objective(&mut self, id: &str) {
        if let Some(objective) = self.objectives.get_mut(id) {
            objective.complete();
        }
    }

    /// Add collectible
    pub fn collect(&mut self) {
        if self.collectibles.0 < self.collectibles.1 {
            self.collectibles.0 += 1;
        }
    }

    /// Find secret
    pub fn find_secret(&mut self) {
        if self.secrets.0 < self.secrets.1 {
            self.secrets.0 += 1;
        }
    }

    /// Get completion percentage
    pub fn completion_percent(&self) -> f32 {
        let mut total = 0;
        let mut done = 0;

        // Objectives
        for obj in self.objectives.values() {
            total += 1;
            if obj.completed {
                done += 1;
            }
        }

        // Collectibles
        if self.collectibles.1 > 0 {
            total += 1;
            if self.collectibles.0 >= self.collectibles.1 {
                done += 1;
            }
        }

        // Secrets
        if self.secrets.1 > 0 {
            total += 1;
            if self.secrets.0 >= self.secrets.1 {
                done += 1;
            }
        }

        if total == 0 {
            0.0
        } else {
            done as f32 / total as f32 * 100.0
        }
    }
}

impl Default for LevelState {
    fn default() -> Self {
        Self::new("default")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_level_state() {
        let mut level = LevelState::new("level1")
            .with_name("Forest")
            .with_status(LevelStatus::Unlocked)
            .with_collectibles(10)
            .with_objective(LevelObjective::new("defeat_boss", "Defeat the Boss"));

        assert!(level.status.is_playable());
        assert!(!level.status.is_completed());

        level.start();
        assert_eq!(level.status, LevelStatus::InProgress);

        level.complete_objective("defeat_boss");
        level.complete(120.0);

        assert!(level.status.is_completed());
        assert_eq!(level.completions, 1);
        assert_eq!(level.best_time, Some(120.0));
    }

    #[test]
    fn test_objective() {
        let mut objective = LevelObjective::new("collect_coins", "Collect 10 Coins")
            .with_target(10);

        assert!(!objective.completed);

        objective.update(5);
        assert_eq!(objective.progress, 0.5);
        assert!(!objective.completed);

        objective.update(10);
        assert!(objective.completed);
        assert_eq!(objective.progress, 1.0);
    }

    #[test]
    fn test_collectibles() {
        let mut level = LevelState::new("test")
            .with_collectibles(5)
            .with_secrets(3);

        level.collect();
        level.collect();
        assert_eq!(level.collectibles.0, 2);

        level.find_secret();
        assert_eq!(level.secrets.0, 1);
    }

    #[test]
    fn test_stars() {
        let mut level = LevelState::new("test")
            .with_status(LevelStatus::Unlocked)
            .with_collectibles(3)
            .with_objective(LevelObjective::new("main", "Complete"));

        level.complete_objective("main");

        // Collect all
        level.collectibles.0 = 3;

        level.complete(60.0);

        assert_eq!(level.stars, 3);
    }
}
