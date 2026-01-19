//! Progress tracking and achievements

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Achievement definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Achievement {
    /// Unique identifier
    pub id: String,
    /// Display name
    pub name: String,
    /// Description
    pub description: String,
    /// Icon path
    pub icon: String,
    /// Whether hidden until unlocked
    pub hidden: bool,
    /// Points/score value
    pub points: u32,
    /// Whether unlocked
    pub unlocked: bool,
    /// Unlock timestamp
    pub unlocked_time: Option<u64>,
    /// Progress (for progressive achievements)
    pub progress: f32,
    /// Target value
    pub target: u32,
    /// Current value
    pub current: u32,
}

impl Achievement {
    /// Create a new achievement
    pub fn new(id: impl Into<String>, name: impl Into<String>) -> Self {
        Self {
            id: id.into(),
            name: name.into(),
            description: String::new(),
            icon: String::new(),
            hidden: false,
            points: 10,
            unlocked: false,
            unlocked_time: None,
            progress: 0.0,
            target: 1,
            current: 0,
        }
    }

    /// Set description
    pub fn with_description(mut self, desc: impl Into<String>) -> Self {
        self.description = desc.into();
        self
    }

    /// Set icon
    pub fn with_icon(mut self, icon: impl Into<String>) -> Self {
        self.icon = icon.into();
        self
    }

    /// Set as hidden
    pub fn hidden(mut self) -> Self {
        self.hidden = true;
        self
    }

    /// Set points
    pub fn with_points(mut self, points: u32) -> Self {
        self.points = points;
        self
    }

    /// Set target for progressive achievement
    pub fn with_target(mut self, target: u32) -> Self {
        self.target = target;
        self
    }

    /// Update progress
    pub fn update(&mut self, current: u32) {
        self.current = current;
        self.progress = if self.target > 0 {
            (self.current as f32 / self.target as f32).min(1.0)
        } else {
            1.0
        };

        if self.current >= self.target && !self.unlocked {
            self.unlock();
        }
    }

    /// Increment progress
    pub fn increment(&mut self, amount: u32) {
        self.update(self.current.saturating_add(amount));
    }

    /// Unlock the achievement
    pub fn unlock(&mut self) {
        if !self.unlocked {
            self.unlocked = true;
            self.current = self.target;
            self.progress = 1.0;
            self.unlocked_time = Some(
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .map(|d| d.as_secs())
                    .unwrap_or(0),
            );
        }
    }
}

impl Default for Achievement {
    fn default() -> Self {
        Self::new("unknown", "Unknown Achievement")
    }
}

/// Stat types for tracking
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum StatType {
    /// Total play time (seconds)
    PlayTime,
    /// Enemies killed
    Kills,
    /// Deaths
    Deaths,
    /// Distance traveled
    Distance,
    /// Jumps
    Jumps,
    /// Items collected
    ItemsCollected,
    /// Secrets found
    SecretsFound,
    /// Levels completed
    LevelsCompleted,
    /// Custom stat
    Custom(u32),
}

/// Progress tracker for statistics and achievements
#[derive(Debug, Default, Serialize, Deserialize)]
pub struct ProgressTracker {
    /// Achievements
    achievements: HashMap<String, Achievement>,
    /// Statistics (integer values)
    stats_int: HashMap<String, i64>,
    /// Statistics (float values)
    stats_float: HashMap<String, f64>,
    /// Flags (boolean values)
    flags: HashMap<String, bool>,
    /// Total achievement points
    total_points: u32,
    /// Unlocked achievement points
    unlocked_points: u32,
}

impl ProgressTracker {
    /// Create a new progress tracker
    pub fn new() -> Self {
        Self::default()
    }

    // Achievement methods

    /// Register an achievement
    pub fn register_achievement(&mut self, achievement: Achievement) {
        self.total_points += achievement.points;
        if achievement.unlocked {
            self.unlocked_points += achievement.points;
        }
        self.achievements.insert(achievement.id.clone(), achievement);
    }

    /// Get an achievement
    pub fn get_achievement(&self, id: &str) -> Option<&Achievement> {
        self.achievements.get(id)
    }

    /// Get mutable achievement
    pub fn get_achievement_mut(&mut self, id: &str) -> Option<&mut Achievement> {
        self.achievements.get_mut(id)
    }

    /// Update achievement progress
    pub fn update_achievement(&mut self, id: &str, current: u32) -> bool {
        if let Some(achievement) = self.achievements.get_mut(id) {
            let was_unlocked = achievement.unlocked;
            achievement.update(current);

            if !was_unlocked && achievement.unlocked {
                self.unlocked_points += achievement.points;
                return true; // Just unlocked
            }
        }
        false
    }

    /// Increment achievement progress
    pub fn increment_achievement(&mut self, id: &str, amount: u32) -> bool {
        if let Some(achievement) = self.achievements.get_mut(id) {
            let was_unlocked = achievement.unlocked;
            achievement.increment(amount);

            if !was_unlocked && achievement.unlocked {
                self.unlocked_points += achievement.points;
                return true;
            }
        }
        false
    }

    /// Unlock an achievement
    pub fn unlock_achievement(&mut self, id: &str) -> bool {
        if let Some(achievement) = self.achievements.get_mut(id) {
            if !achievement.unlocked {
                achievement.unlock();
                self.unlocked_points += achievement.points;
                return true;
            }
        }
        false
    }

    /// Get all achievements
    pub fn achievements(&self) -> impl Iterator<Item = &Achievement> {
        self.achievements.values()
    }

    /// Get unlocked achievements
    pub fn unlocked_achievements(&self) -> impl Iterator<Item = &Achievement> {
        self.achievements.values().filter(|a| a.unlocked)
    }

    /// Get locked achievements (visible ones only)
    pub fn locked_achievements(&self) -> impl Iterator<Item = &Achievement> {
        self.achievements.values().filter(|a| !a.unlocked && !a.hidden)
    }

    /// Get achievement completion percentage
    pub fn achievement_percent(&self) -> f32 {
        if self.achievements.is_empty() {
            return 0.0;
        }
        let unlocked = self.achievements.values().filter(|a| a.unlocked).count();
        unlocked as f32 / self.achievements.len() as f32 * 100.0
    }

    /// Get total/unlocked points
    pub fn points(&self) -> (u32, u32) {
        (self.unlocked_points, self.total_points)
    }

    // Statistics methods

    /// Set integer stat
    pub fn set_stat_int(&mut self, name: impl Into<String>, value: i64) {
        self.stats_int.insert(name.into(), value);
    }

    /// Get integer stat
    pub fn get_stat_int(&self, name: &str) -> i64 {
        *self.stats_int.get(name).unwrap_or(&0)
    }

    /// Add to integer stat
    pub fn add_stat_int(&mut self, name: &str, amount: i64) {
        *self.stats_int.entry(name.to_string()).or_insert(0) += amount;
    }

    /// Set float stat
    pub fn set_stat_float(&mut self, name: impl Into<String>, value: f64) {
        self.stats_float.insert(name.into(), value);
    }

    /// Get float stat
    pub fn get_stat_float(&self, name: &str) -> f64 {
        *self.stats_float.get(name).unwrap_or(&0.0)
    }

    /// Add to float stat
    pub fn add_stat_float(&mut self, name: &str, amount: f64) {
        *self.stats_float.entry(name.to_string()).or_insert(0.0) += amount;
    }

    /// Set max float stat (only updates if higher)
    pub fn set_stat_max(&mut self, name: &str, value: f64) {
        let entry = self.stats_float.entry(name.to_string()).or_insert(0.0);
        if value > *entry {
            *entry = value;
        }
    }

    // Flag methods

    /// Set a flag
    pub fn set_flag(&mut self, name: impl Into<String>, value: bool) {
        self.flags.insert(name.into(), value);
    }

    /// Get a flag
    pub fn get_flag(&self, name: &str) -> bool {
        *self.flags.get(name).unwrap_or(&false)
    }

    /// Toggle a flag
    pub fn toggle_flag(&mut self, name: &str) -> bool {
        let entry = self.flags.entry(name.to_string()).or_insert(false);
        *entry = !*entry;
        *entry
    }

    // Common stat helpers

    /// Add play time
    pub fn add_play_time(&mut self, seconds: f64) {
        self.add_stat_float("play_time", seconds);
    }

    /// Get play time
    pub fn play_time(&self) -> f64 {
        self.get_stat_float("play_time")
    }

    /// Increment kill count
    pub fn add_kill(&mut self) {
        self.add_stat_int("kills", 1);
    }

    /// Get kill count
    pub fn kills(&self) -> i64 {
        self.get_stat_int("kills")
    }

    /// Increment death count
    pub fn add_death(&mut self) {
        self.add_stat_int("deaths", 1);
    }

    /// Get death count
    pub fn deaths(&self) -> i64 {
        self.get_stat_int("deaths")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_achievement() {
        let mut achievement = Achievement::new("first_kill", "First Blood")
            .with_description("Defeat your first enemy")
            .with_points(10);

        assert!(!achievement.unlocked);

        achievement.unlock();
        assert!(achievement.unlocked);
        assert!(achievement.unlocked_time.is_some());
    }

    #[test]
    fn test_progressive_achievement() {
        let mut achievement = Achievement::new("kill_100", "Centurion")
            .with_target(100);

        achievement.update(50);
        assert_eq!(achievement.progress, 0.5);
        assert!(!achievement.unlocked);

        achievement.update(100);
        assert!(achievement.unlocked);
    }

    #[test]
    fn test_progress_tracker() {
        let mut tracker = ProgressTracker::new();

        tracker.register_achievement(
            Achievement::new("test", "Test Achievement").with_points(50),
        );

        assert_eq!(tracker.points(), (0, 50));

        tracker.unlock_achievement("test");
        assert_eq!(tracker.points(), (50, 50));

        assert_eq!(tracker.achievement_percent(), 100.0);
    }

    #[test]
    fn test_stats() {
        let mut tracker = ProgressTracker::new();

        tracker.add_kill();
        tracker.add_kill();
        assert_eq!(tracker.kills(), 2);

        tracker.add_play_time(3600.0);
        assert_eq!(tracker.play_time(), 3600.0);
    }

    #[test]
    fn test_flags() {
        let mut tracker = ProgressTracker::new();

        assert!(!tracker.get_flag("tutorial_complete"));

        tracker.set_flag("tutorial_complete", true);
        assert!(tracker.get_flag("tutorial_complete"));

        tracker.toggle_flag("tutorial_complete");
        assert!(!tracker.get_flag("tutorial_complete"));
    }
}
