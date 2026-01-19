//! Steering behaviors

use serde::{Deserialize, Serialize};

/// Output of a steering behavior
#[derive(Debug, Clone, Copy, Default, Serialize, Deserialize)]
pub struct SteeringOutput {
    /// Linear acceleration
    pub linear: [f32; 3],
    /// Angular acceleration (rotation around Y axis for 2D-like steering)
    pub angular: f32,
}

impl SteeringOutput {
    /// Zero output
    pub fn zero() -> Self {
        Self::default()
    }

    /// Create from linear velocity
    pub fn linear(x: f32, y: f32, z: f32) -> Self {
        Self {
            linear: [x, y, z],
            angular: 0.0,
        }
    }

    /// Add another steering output
    pub fn add(&mut self, other: &SteeringOutput) {
        self.linear[0] += other.linear[0];
        self.linear[1] += other.linear[1];
        self.linear[2] += other.linear[2];
        self.angular += other.angular;
    }

    /// Scale the output
    pub fn scale(&mut self, factor: f32) {
        self.linear[0] *= factor;
        self.linear[1] *= factor;
        self.linear[2] *= factor;
        self.angular *= factor;
    }

    /// Get magnitude of linear component
    pub fn magnitude(&self) -> f32 {
        (self.linear[0].powi(2) + self.linear[1].powi(2) + self.linear[2].powi(2)).sqrt()
    }

    /// Normalize linear component
    pub fn normalize(&mut self) {
        let mag = self.magnitude();
        if mag > 0.0 {
            self.linear[0] /= mag;
            self.linear[1] /= mag;
            self.linear[2] /= mag;
        }
    }

    /// Limit magnitude
    pub fn limit(&mut self, max: f32) {
        let mag = self.magnitude();
        if mag > max {
            let scale = max / mag;
            self.linear[0] *= scale;
            self.linear[1] *= scale;
            self.linear[2] *= scale;
        }
    }
}

/// Steering behavior type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SteeringBehaviorType {
    Seek,
    Flee,
    Arrive,
    Pursue,
    Evade,
    Wander,
    ObstacleAvoidance,
    Separation,
    Alignment,
    Cohesion,
}

/// Steering behavior configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SteeringBehavior {
    /// Behavior type
    pub behavior_type: SteeringBehaviorType,
    /// Weight for blending
    pub weight: f32,
    /// Maximum acceleration
    pub max_acceleration: f32,
    /// Arrive slowing radius
    pub arrive_radius: f32,
    /// Wander radius
    pub wander_radius: f32,
    /// Wander distance
    pub wander_distance: f32,
    /// Separation radius
    pub separation_radius: f32,
    /// Alignment radius
    pub alignment_radius: f32,
    /// Cohesion radius
    pub cohesion_radius: f32,
    /// Obstacle avoidance look-ahead distance
    pub look_ahead: f32,
}

impl Default for SteeringBehavior {
    fn default() -> Self {
        Self {
            behavior_type: SteeringBehaviorType::Seek,
            weight: 1.0,
            max_acceleration: 10.0,
            arrive_radius: 5.0,
            wander_radius: 2.0,
            wander_distance: 5.0,
            separation_radius: 2.0,
            alignment_radius: 5.0,
            cohesion_radius: 10.0,
            look_ahead: 5.0,
        }
    }
}

impl SteeringBehavior {
    /// Create a seek behavior
    pub fn seek() -> Self {
        Self {
            behavior_type: SteeringBehaviorType::Seek,
            ..Default::default()
        }
    }

    /// Create a flee behavior
    pub fn flee() -> Self {
        Self {
            behavior_type: SteeringBehaviorType::Flee,
            ..Default::default()
        }
    }

    /// Create an arrive behavior
    pub fn arrive(radius: f32) -> Self {
        Self {
            behavior_type: SteeringBehaviorType::Arrive,
            arrive_radius: radius,
            ..Default::default()
        }
    }

    /// Create a wander behavior
    pub fn wander() -> Self {
        Self {
            behavior_type: SteeringBehaviorType::Wander,
            ..Default::default()
        }
    }

    /// Create a separation behavior
    pub fn separation(radius: f32) -> Self {
        Self {
            behavior_type: SteeringBehaviorType::Separation,
            separation_radius: radius,
            ..Default::default()
        }
    }

    /// Set weight
    pub fn with_weight(mut self, weight: f32) -> Self {
        self.weight = weight;
        self
    }

    /// Set max acceleration
    pub fn with_max_acceleration(mut self, max: f32) -> Self {
        self.max_acceleration = max;
        self
    }

    /// Calculate steering output
    pub fn calculate(
        &self,
        position: [f32; 3],
        velocity: [f32; 3],
        target: [f32; 3],
        max_speed: f32,
    ) -> SteeringOutput {
        match self.behavior_type {
            SteeringBehaviorType::Seek => self.seek_impl(position, velocity, target, max_speed),
            SteeringBehaviorType::Flee => self.flee_impl(position, velocity, target, max_speed),
            SteeringBehaviorType::Arrive => self.arrive_impl(position, velocity, target, max_speed),
            SteeringBehaviorType::Wander => self.wander_impl(position, velocity, max_speed),
            _ => SteeringOutput::zero(),
        }
    }

    fn seek_impl(
        &self,
        position: [f32; 3],
        _velocity: [f32; 3],
        target: [f32; 3],
        max_speed: f32,
    ) -> SteeringOutput {
        let desired = [
            target[0] - position[0],
            target[1] - position[1],
            target[2] - position[2],
        ];

        let mut output = SteeringOutput::linear(desired[0], desired[1], desired[2]);
        output.normalize();
        output.scale(max_speed);
        output.limit(self.max_acceleration);
        output.scale(self.weight);
        output
    }

    fn flee_impl(
        &self,
        position: [f32; 3],
        velocity: [f32; 3],
        target: [f32; 3],
        max_speed: f32,
    ) -> SteeringOutput {
        let mut output = self.seek_impl(position, velocity, target, max_speed);
        output.linear[0] = -output.linear[0];
        output.linear[1] = -output.linear[1];
        output.linear[2] = -output.linear[2];
        output
    }

    fn arrive_impl(
        &self,
        position: [f32; 3],
        _velocity: [f32; 3],
        target: [f32; 3],
        max_speed: f32,
    ) -> SteeringOutput {
        let to_target = [
            target[0] - position[0],
            target[1] - position[1],
            target[2] - position[2],
        ];

        let distance =
            (to_target[0].powi(2) + to_target[1].powi(2) + to_target[2].powi(2)).sqrt();

        if distance < 0.01 {
            return SteeringOutput::zero();
        }

        let target_speed = if distance < self.arrive_radius {
            max_speed * (distance / self.arrive_radius)
        } else {
            max_speed
        };

        let mut output = SteeringOutput::linear(
            to_target[0] / distance * target_speed,
            to_target[1] / distance * target_speed,
            to_target[2] / distance * target_speed,
        );

        output.limit(self.max_acceleration);
        output.scale(self.weight);
        output
    }

    fn wander_impl(
        &self,
        _position: [f32; 3],
        velocity: [f32; 3],
        max_speed: f32,
    ) -> SteeringOutput {
        // Simple wander - add random offset to current direction
        let random_angle = rand_float() * std::f32::consts::TAU;

        let forward = normalize_vec3(velocity);
        let wander_x = forward[0] * self.wander_distance + random_angle.cos() * self.wander_radius;
        let wander_z = forward[2] * self.wander_distance + random_angle.sin() * self.wander_radius;

        let mut output = SteeringOutput::linear(wander_x, 0.0, wander_z);
        output.normalize();
        output.scale(max_speed);
        output.limit(self.max_acceleration);
        output.scale(self.weight);
        output
    }
}

/// Normalize a 3D vector
fn normalize_vec3(v: [f32; 3]) -> [f32; 3] {
    let mag = (v[0].powi(2) + v[1].powi(2) + v[2].powi(2)).sqrt();
    if mag > 0.0 {
        [v[0] / mag, v[1] / mag, v[2] / mag]
    } else {
        [0.0, 0.0, 1.0] // Default forward
    }
}

/// Simple random float (0.0 - 1.0)
fn rand_float() -> f32 {
    use std::time::{SystemTime, UNIX_EPOCH};
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .subsec_nanos();
    (nanos % 1000) as f32 / 1000.0
}

/// Steering agent with combined behaviors
#[derive(Debug, Clone, Default)]
pub struct SteeringAgent {
    /// Active behaviors
    pub behaviors: Vec<SteeringBehavior>,
    /// Maximum speed
    pub max_speed: f32,
    /// Maximum force
    pub max_force: f32,
}

impl SteeringAgent {
    /// Create a new steering agent
    pub fn new(max_speed: f32, max_force: f32) -> Self {
        Self {
            behaviors: Vec::new(),
            max_speed,
            max_force,
        }
    }

    /// Add a behavior
    pub fn add_behavior(&mut self, behavior: SteeringBehavior) {
        self.behaviors.push(behavior);
    }

    /// Calculate combined steering
    pub fn calculate(&self, position: [f32; 3], velocity: [f32; 3], target: [f32; 3]) -> SteeringOutput {
        let mut total = SteeringOutput::zero();

        for behavior in &self.behaviors {
            let output = behavior.calculate(position, velocity, target, self.max_speed);
            total.add(&output);
        }

        total.limit(self.max_force);
        total
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_steering_output() {
        let mut output = SteeringOutput::linear(3.0, 0.0, 4.0);
        assert_eq!(output.magnitude(), 5.0);

        output.normalize();
        assert!((output.magnitude() - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_seek() {
        let behavior = SteeringBehavior::seek();
        let output = behavior.calculate(
            [0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0],
            [10.0, 0.0, 0.0],
            5.0,
        );

        assert!(output.linear[0] > 0.0); // Moving toward target
    }

    #[test]
    fn test_flee() {
        let behavior = SteeringBehavior::flee();
        let output = behavior.calculate(
            [0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0],
            [10.0, 0.0, 0.0],
            5.0,
        );

        assert!(output.linear[0] < 0.0); // Moving away from target
    }

    #[test]
    fn test_arrive() {
        let behavior = SteeringBehavior::arrive(5.0);

        // Far from target
        let output_far = behavior.calculate(
            [0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0],
            [20.0, 0.0, 0.0],
            10.0,
        );

        // Close to target
        let output_close = behavior.calculate(
            [0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0],
            [2.0, 0.0, 0.0],
            10.0,
        );

        // Should slow down when close
        assert!(output_close.magnitude() < output_far.magnitude());
    }

    #[test]
    fn test_steering_agent() {
        let mut agent = SteeringAgent::new(10.0, 5.0);
        agent.add_behavior(SteeringBehavior::seek().with_weight(1.0));

        let output = agent.calculate([0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [10.0, 0.0, 0.0]);
        assert!(output.linear[0] > 0.0);
    }
}
