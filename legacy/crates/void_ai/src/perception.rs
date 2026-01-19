//! Perception and sensing system

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Type of stimulus that can be perceived
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum StimulusType {
    /// Visual stimulus (sight)
    Visual,
    /// Audio stimulus (sound)
    Audio,
    /// Damage stimulus (being hit)
    Damage,
    /// Touch/collision stimulus
    Touch,
    /// Custom stimulus type
    Custom(u32),
}

/// A stimulus that can be perceived by AI agents
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Stimulus {
    /// Type of stimulus
    pub stimulus_type: StimulusType,
    /// Position of the stimulus
    pub position: [f32; 3],
    /// Intensity/strength of the stimulus
    pub intensity: f32,
    /// Source entity ID (if any)
    pub source: Option<u64>,
    /// Time when stimulus was created
    pub timestamp: f64,
    /// How long this stimulus lasts
    pub duration: f32,
    /// Additional data
    pub data: Option<StimulusData>,
}

/// Additional data for specific stimulus types
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum StimulusData {
    /// Damage amount
    Damage(f32),
    /// Sound name/type
    Sound(String),
    /// Visual target entity
    Target(u64),
    /// Custom data
    Custom(String),
}

impl Stimulus {
    /// Create a new visual stimulus
    pub fn visual(position: [f32; 3], target: u64, intensity: f32) -> Self {
        Self {
            stimulus_type: StimulusType::Visual,
            position,
            intensity,
            source: Some(target),
            timestamp: 0.0, // Will be set by perception system
            duration: 0.0,  // Visual stimuli are instant
            data: Some(StimulusData::Target(target)),
        }
    }

    /// Create a new audio stimulus
    pub fn audio(position: [f32; 3], intensity: f32, duration: f32) -> Self {
        Self {
            stimulus_type: StimulusType::Audio,
            position,
            intensity,
            source: None,
            timestamp: 0.0,
            duration,
            data: None,
        }
    }

    /// Create a damage stimulus
    pub fn damage(position: [f32; 3], source: u64, amount: f32) -> Self {
        Self {
            stimulus_type: StimulusType::Damage,
            position,
            intensity: amount,
            source: Some(source),
            timestamp: 0.0,
            duration: 0.0,
            data: Some(StimulusData::Damage(amount)),
        }
    }

    /// Check if stimulus has expired
    pub fn is_expired(&self, current_time: f64) -> bool {
        if self.duration <= 0.0 {
            return false; // Instant stimuli don't expire this way
        }
        current_time - self.timestamp > self.duration as f64
    }

    /// Get distance from a position
    pub fn distance_from(&self, position: [f32; 3]) -> f32 {
        let dx = self.position[0] - position[0];
        let dy = self.position[1] - position[1];
        let dz = self.position[2] - position[2];
        (dx * dx + dy * dy + dz * dz).sqrt()
    }
}

/// Perception sense configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SenseConfig {
    /// Maximum range of this sense
    pub range: f32,
    /// Field of view angle (radians, 0 = all around)
    pub fov: f32,
    /// Sensitivity multiplier
    pub sensitivity: f32,
    /// Whether this sense is enabled
    pub enabled: bool,
    /// Stimulus types this sense can detect
    pub detects: Vec<StimulusType>,
}

impl Default for SenseConfig {
    fn default() -> Self {
        Self {
            range: 20.0,
            fov: 0.0, // 360 degrees
            sensitivity: 1.0,
            enabled: true,
            detects: vec![StimulusType::Visual, StimulusType::Audio],
        }
    }
}

impl SenseConfig {
    /// Create a sight sense
    pub fn sight(range: f32, fov: f32) -> Self {
        Self {
            range,
            fov,
            sensitivity: 1.0,
            enabled: true,
            detects: vec![StimulusType::Visual],
        }
    }

    /// Create a hearing sense
    pub fn hearing(range: f32) -> Self {
        Self {
            range,
            fov: 0.0, // 360 degree hearing
            sensitivity: 1.0,
            enabled: true,
            detects: vec![StimulusType::Audio],
        }
    }

    /// Create a damage sense (always on)
    pub fn damage() -> Self {
        Self {
            range: f32::MAX,
            fov: 0.0,
            sensitivity: 1.0,
            enabled: true,
            detects: vec![StimulusType::Damage],
        }
    }
}

/// Memory of a perceived stimulus
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PerceptionMemory {
    /// The original stimulus
    pub stimulus: Stimulus,
    /// When it was perceived
    pub perceived_at: f64,
    /// Last known position (may be updated)
    pub last_known_position: [f32; 3],
    /// Confidence in this memory (0.0 - 1.0)
    pub confidence: f32,
    /// Number of times this target was perceived
    pub perception_count: u32,
}

impl PerceptionMemory {
    /// Create from a stimulus
    pub fn from_stimulus(stimulus: Stimulus, time: f64) -> Self {
        Self {
            last_known_position: stimulus.position,
            perceived_at: time,
            stimulus,
            confidence: 1.0,
            perception_count: 1,
        }
    }

    /// Update with new perception
    pub fn update(&mut self, new_position: [f32; 3], time: f64) {
        self.last_known_position = new_position;
        self.perceived_at = time;
        self.confidence = 1.0;
        self.perception_count += 1;
    }

    /// Decay confidence over time
    pub fn decay(&mut self, current_time: f64, decay_rate: f32) {
        let elapsed = (current_time - self.perceived_at) as f32;
        self.confidence = (1.0 - elapsed * decay_rate).max(0.0);
    }
}

/// Perception component for an AI agent
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct PerceptionComponent {
    /// Configured senses
    pub senses: Vec<SenseConfig>,
    /// Current stimuli being perceived
    pub current_stimuli: Vec<Stimulus>,
    /// Memory of past perceptions (keyed by source entity)
    pub memory: HashMap<u64, PerceptionMemory>,
    /// Agent's position
    pub position: [f32; 3],
    /// Agent's forward direction
    pub forward: [f32; 3],
    /// Memory decay rate (per second)
    pub memory_decay_rate: f32,
    /// Maximum memory entries
    pub max_memory: usize,
}

impl PerceptionComponent {
    /// Create a new perception component
    pub fn new() -> Self {
        Self {
            senses: vec![SenseConfig::default()],
            current_stimuli: Vec::new(),
            memory: HashMap::new(),
            position: [0.0, 0.0, 0.0],
            forward: [0.0, 0.0, 1.0],
            memory_decay_rate: 0.1,
            max_memory: 10,
        }
    }

    /// Add a sense
    pub fn with_sense(mut self, sense: SenseConfig) -> Self {
        self.senses.push(sense);
        self
    }

    /// Set position
    pub fn set_position(&mut self, position: [f32; 3]) {
        self.position = position;
    }

    /// Set forward direction
    pub fn set_forward(&mut self, forward: [f32; 3]) {
        self.forward = forward;
    }

    /// Check if a stimulus can be perceived
    pub fn can_perceive(&self, stimulus: &Stimulus) -> bool {
        for sense in &self.senses {
            if !sense.enabled {
                continue;
            }

            if !sense.detects.contains(&stimulus.stimulus_type) {
                continue;
            }

            let distance = stimulus.distance_from(self.position);
            if distance > sense.range {
                continue;
            }

            // Check FOV if not 360
            if sense.fov > 0.0 {
                let to_stimulus = [
                    stimulus.position[0] - self.position[0],
                    stimulus.position[1] - self.position[1],
                    stimulus.position[2] - self.position[2],
                ];
                let angle = angle_between(self.forward, to_stimulus);
                if angle > sense.fov / 2.0 {
                    continue;
                }
            }

            // Calculate effective intensity
            let distance_factor = 1.0 - (distance / sense.range);
            let effective_intensity = stimulus.intensity * distance_factor * sense.sensitivity;

            if effective_intensity > 0.1 {
                return true;
            }
        }

        false
    }

    /// Process a stimulus
    pub fn process_stimulus(&mut self, stimulus: Stimulus, current_time: f64) {
        if !self.can_perceive(&stimulus) {
            return;
        }

        // Add to current stimuli
        self.current_stimuli.push(stimulus.clone());

        // Update memory if has source
        if let Some(source) = stimulus.source {
            if let Some(memory) = self.memory.get_mut(&source) {
                memory.update(stimulus.position, current_time);
            } else {
                if self.memory.len() >= self.max_memory {
                    // Remove oldest/lowest confidence memory
                    let oldest = self
                        .memory
                        .iter()
                        .min_by(|a, b| {
                            a.1.confidence
                                .partial_cmp(&b.1.confidence)
                                .unwrap_or(std::cmp::Ordering::Equal)
                        })
                        .map(|(k, _)| *k);
                    if let Some(key) = oldest {
                        self.memory.remove(&key);
                    }
                }
                self.memory
                    .insert(source, PerceptionMemory::from_stimulus(stimulus, current_time));
            }
        }
    }

    /// Update perception (call each frame)
    pub fn update(&mut self, current_time: f64) {
        // Clear current stimuli
        self.current_stimuli.clear();

        // Decay memories
        for memory in self.memory.values_mut() {
            memory.decay(current_time, self.memory_decay_rate);
        }

        // Remove forgotten memories
        self.memory.retain(|_, m| m.confidence > 0.0);
    }

    /// Get the highest priority target
    pub fn get_primary_target(&self) -> Option<&PerceptionMemory> {
        self.memory
            .values()
            .filter(|m| m.confidence > 0.5)
            .max_by(|a, b| {
                // Prioritize by confidence, then recency
                a.confidence
                    .partial_cmp(&b.confidence)
                    .unwrap_or(std::cmp::Ordering::Equal)
            })
    }

    /// Check if any target is currently perceived
    pub fn has_target(&self) -> bool {
        self.memory.values().any(|m| m.confidence > 0.5)
    }

    /// Get remembered position of a target
    pub fn get_last_known_position(&self, target: u64) -> Option<[f32; 3]> {
        self.memory.get(&target).map(|m| m.last_known_position)
    }

    /// Check if a specific entity is being perceived
    pub fn is_perceiving(&self, entity: u64) -> bool {
        self.current_stimuli
            .iter()
            .any(|s| s.source == Some(entity))
    }
}

/// Calculate angle between two vectors (in radians)
fn angle_between(a: [f32; 3], b: [f32; 3]) -> f32 {
    let mag_a = (a[0] * a[0] + a[1] * a[1] + a[2] * a[2]).sqrt();
    let mag_b = (b[0] * b[0] + b[1] * b[1] + b[2] * b[2]).sqrt();

    if mag_a == 0.0 || mag_b == 0.0 {
        return 0.0;
    }

    let dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    let cos_angle = (dot / (mag_a * mag_b)).clamp(-1.0, 1.0);
    cos_angle.acos()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_stimulus_creation() {
        let visual = Stimulus::visual([10.0, 0.0, 0.0], 1, 1.0);
        assert_eq!(visual.stimulus_type, StimulusType::Visual);
        assert_eq!(visual.source, Some(1));

        let audio = Stimulus::audio([5.0, 0.0, 0.0], 0.5, 2.0);
        assert_eq!(audio.stimulus_type, StimulusType::Audio);
        assert_eq!(audio.duration, 2.0);
    }

    #[test]
    fn test_stimulus_distance() {
        let stimulus = Stimulus::visual([3.0, 4.0, 0.0], 1, 1.0);
        let distance = stimulus.distance_from([0.0, 0.0, 0.0]);
        assert!((distance - 5.0).abs() < 0.001);
    }

    #[test]
    fn test_perception_component() {
        let mut perception = PerceptionComponent::new();
        perception.set_position([0.0, 0.0, 0.0]);
        perception.set_forward([0.0, 0.0, 1.0]);

        // Add sight sense
        perception.senses = vec![SenseConfig::sight(50.0, 0.0)]; // 360 degree vision

        let stimulus = Stimulus::visual([10.0, 0.0, 0.0], 1, 1.0);
        assert!(perception.can_perceive(&stimulus));
    }

    #[test]
    fn test_perception_out_of_range() {
        let mut perception = PerceptionComponent::new();
        perception.set_position([0.0, 0.0, 0.0]);
        perception.senses = vec![SenseConfig::sight(10.0, 0.0)];

        let stimulus = Stimulus::visual([100.0, 0.0, 0.0], 1, 1.0);
        assert!(!perception.can_perceive(&stimulus));
    }

    #[test]
    fn test_perception_fov() {
        let mut perception = PerceptionComponent::new();
        perception.set_position([0.0, 0.0, 0.0]);
        perception.set_forward([1.0, 0.0, 0.0]); // Looking right
        perception.senses = vec![SenseConfig::sight(50.0, std::f32::consts::PI / 2.0)]; // 90 degree FOV

        // In front
        let in_front = Stimulus::visual([10.0, 0.0, 0.0], 1, 1.0);
        assert!(perception.can_perceive(&in_front));

        // Behind (should not be visible with 90 degree FOV)
        let behind = Stimulus::visual([-10.0, 0.0, 0.0], 2, 1.0);
        assert!(!perception.can_perceive(&behind));
    }

    #[test]
    fn test_process_stimulus() {
        let mut perception = PerceptionComponent::new();
        perception.set_position([0.0, 0.0, 0.0]);
        perception.senses = vec![SenseConfig::sight(50.0, 0.0)];

        let stimulus = Stimulus::visual([10.0, 0.0, 0.0], 123, 1.0);
        perception.process_stimulus(stimulus, 0.0);

        assert!(perception.memory.contains_key(&123));
        assert!(perception.has_target());
    }

    #[test]
    fn test_memory_decay() {
        let mut perception = PerceptionComponent::new();
        perception.memory_decay_rate = 0.5;

        perception.memory.insert(
            1,
            PerceptionMemory {
                stimulus: Stimulus::visual([10.0, 0.0, 0.0], 1, 1.0),
                perceived_at: 0.0,
                last_known_position: [10.0, 0.0, 0.0],
                confidence: 1.0,
                perception_count: 1,
            },
        );

        perception.update(1.0); // 1 second later (0.5 decay, so confidence = 0.5)
        assert!(perception.memory.get(&1).unwrap().confidence < 1.0);
        assert!(perception.memory.get(&1).unwrap().confidence > 0.0);
    }

    #[test]
    fn test_get_primary_target() {
        let mut perception = PerceptionComponent::new();

        perception.memory.insert(
            1,
            PerceptionMemory {
                stimulus: Stimulus::visual([10.0, 0.0, 0.0], 1, 1.0),
                perceived_at: 0.0,
                last_known_position: [10.0, 0.0, 0.0],
                confidence: 0.8,
                perception_count: 1,
            },
        );

        perception.memory.insert(
            2,
            PerceptionMemory {
                stimulus: Stimulus::visual([20.0, 0.0, 0.0], 2, 1.0),
                perceived_at: 0.0,
                last_known_position: [20.0, 0.0, 0.0],
                confidence: 0.9,
                perception_count: 1,
            },
        );

        let target = perception.get_primary_target().unwrap();
        assert_eq!(target.stimulus.source, Some(2)); // Higher confidence
    }

    #[test]
    fn test_hearing_sense() {
        let mut perception = PerceptionComponent::new();
        perception.set_position([0.0, 0.0, 0.0]);
        perception.senses = vec![SenseConfig::hearing(30.0)];

        // Can hear audio
        let audio = Stimulus::audio([10.0, 0.0, 0.0], 1.0, 1.0);
        assert!(perception.can_perceive(&audio));

        // Cannot see visual with only hearing
        let visual = Stimulus::visual([10.0, 0.0, 0.0], 1, 1.0);
        assert!(!perception.can_perceive(&visual));
    }

    #[test]
    fn test_damage_sense() {
        let mut perception = PerceptionComponent::new();
        perception.set_position([0.0, 0.0, 0.0]);
        perception.senses = vec![SenseConfig::damage()];

        // Always perceives damage regardless of distance
        let damage = Stimulus::damage([1000.0, 0.0, 0.0], 1, 50.0);
        assert!(perception.can_perceive(&damage));
    }

    #[test]
    fn test_max_memory() {
        let mut perception = PerceptionComponent::new();
        perception.set_position([0.0, 0.0, 0.0]);
        perception.senses = vec![SenseConfig::sight(100.0, 0.0)];
        perception.max_memory = 2;

        // Add 3 stimuli
        perception.process_stimulus(Stimulus::visual([10.0, 0.0, 0.0], 1, 1.0), 0.0);
        perception.process_stimulus(Stimulus::visual([20.0, 0.0, 0.0], 2, 1.0), 1.0);
        perception.process_stimulus(Stimulus::visual([30.0, 0.0, 0.0], 3, 1.0), 2.0);

        // Should only keep 2
        assert_eq!(perception.memory.len(), 2);
    }
}
