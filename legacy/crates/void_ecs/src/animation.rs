//! Keyframe Animation System
//!
//! Backend-agnostic animation infrastructure for transform animations.
//!
//! # Features
//!
//! - Animation clips with keyframe tracks
//! - Linear, step, and cubic spline interpolation
//! - Loop modes (once, loop, ping-pong, clamp)
//! - Animation blending and transitions
//! - Animation events
//! - Hot-reload serialization support
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::animation::*;
//!
//! // Create an animation clip
//! let mut clip = AnimationClip::new("walk", 2.0);
//! clip.add_track("root/translation", AnimationTrack {
//!     target: "root".to_string(),
//!     property: AnimatedProperty::Translation,
//!     keyframes: vec![
//!         Keyframe::vec3(0.0, [0.0, 0.0, 0.0]),
//!         Keyframe::vec3(1.0, [0.0, 1.0, 0.0]),
//!         Keyframe::vec3(2.0, [0.0, 0.0, 0.0]),
//!     ],
//!     interpolation: Interpolation::Linear,
//! });
//!
//! // Create an animation player
//! let mut player = AnimationPlayer::new();
//! player.play("walk");
//!
//! // Sample the animation
//! let pose = clip.sample(0.5);
//! ```

use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

/// Keyframe value types
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub enum KeyframeValue {
    /// Single float value
    Float(f32),
    /// 3D vector (translation, scale)
    Vec3([f32; 3]),
    /// Quaternion (rotation) [x, y, z, w]
    Quat([f32; 4]),
    /// Morph target weights
    Weights(Vec<f32>),
}

impl KeyframeValue {
    /// Create a float keyframe value
    pub fn float(v: f32) -> Self {
        Self::Float(v)
    }

    /// Create a vec3 keyframe value
    pub fn vec3(v: [f32; 3]) -> Self {
        Self::Vec3(v)
    }

    /// Create a quaternion keyframe value
    pub fn quat(v: [f32; 4]) -> Self {
        Self::Quat(v)
    }

    /// Create identity quaternion
    pub fn quat_identity() -> Self {
        Self::Quat([0.0, 0.0, 0.0, 1.0])
    }

    /// Create weights keyframe value
    pub fn weights(v: Vec<f32>) -> Self {
        Self::Weights(v)
    }
}

impl Default for KeyframeValue {
    fn default() -> Self {
        Self::Float(0.0)
    }
}

/// Interpolation mode
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum Interpolation {
    /// Constant value until next keyframe
    Step,
    /// Linear interpolation
    #[default]
    Linear,
    /// Cubic spline (Hermite) interpolation
    CubicSpline,
}

/// Single keyframe
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Keyframe {
    /// Time in seconds
    pub time: f32,
    /// Keyframe value
    pub value: KeyframeValue,
    /// In-tangent for cubic spline interpolation
    pub in_tangent: Option<KeyframeValue>,
    /// Out-tangent for cubic spline interpolation
    pub out_tangent: Option<KeyframeValue>,
}

impl Keyframe {
    /// Create a new keyframe
    pub fn new(time: f32, value: KeyframeValue) -> Self {
        Self {
            time,
            value,
            in_tangent: None,
            out_tangent: None,
        }
    }

    /// Create a float keyframe
    pub fn float(time: f32, value: f32) -> Self {
        Self::new(time, KeyframeValue::Float(value))
    }

    /// Create a vec3 keyframe
    pub fn vec3(time: f32, value: [f32; 3]) -> Self {
        Self::new(time, KeyframeValue::Vec3(value))
    }

    /// Create a quaternion keyframe
    pub fn quat(time: f32, value: [f32; 4]) -> Self {
        Self::new(time, KeyframeValue::Quat(value))
    }

    /// Create a keyframe with tangents for cubic spline
    pub fn with_tangents(
        time: f32,
        value: KeyframeValue,
        in_tangent: KeyframeValue,
        out_tangent: KeyframeValue,
    ) -> Self {
        Self {
            time,
            value,
            in_tangent: Some(in_tangent),
            out_tangent: Some(out_tangent),
        }
    }
}

/// Animated property type
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum AnimatedProperty {
    /// Translation (Vec3)
    Translation,
    /// Rotation (Quaternion)
    Rotation,
    /// Scale (Vec3)
    Scale,
    /// Morph target weights
    Weights,
    /// Custom property by index
    Custom(u32),
}

/// Animation track for a single property
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationTrack {
    /// Target entity path (e.g., "armature/spine/head")
    pub target: String,
    /// Property being animated
    pub property: AnimatedProperty,
    /// Keyframes (sorted by time)
    pub keyframes: Vec<Keyframe>,
    /// Interpolation mode
    pub interpolation: Interpolation,
}

impl AnimationTrack {
    /// Create a new animation track
    pub fn new(target: impl Into<String>, property: AnimatedProperty) -> Self {
        Self {
            target: target.into(),
            property,
            keyframes: Vec::new(),
            interpolation: Interpolation::Linear,
        }
    }

    /// Add a keyframe
    pub fn add_keyframe(&mut self, keyframe: Keyframe) {
        self.keyframes.push(keyframe);
        self.keyframes.sort_by(|a, b| a.time.partial_cmp(&b.time).unwrap());
    }

    /// Get track duration
    pub fn duration(&self) -> f32 {
        self.keyframes.last().map(|k| k.time).unwrap_or(0.0)
    }

    /// Sample the track at a given time
    pub fn sample(&self, time: f32) -> KeyframeValue {
        if self.keyframes.is_empty() {
            return KeyframeValue::Float(0.0);
        }

        if self.keyframes.len() == 1 {
            return self.keyframes[0].value.clone();
        }

        // Find surrounding keyframes
        let (prev, next, t) = self.find_keyframes(time);

        match self.interpolation {
            Interpolation::Step => prev.value.clone(),
            Interpolation::Linear => lerp_value(&prev.value, &next.value, t),
            Interpolation::CubicSpline => self.cubic_spline(prev, next, t),
        }
    }

    /// Find keyframes surrounding the given time
    fn find_keyframes(&self, time: f32) -> (&Keyframe, &Keyframe, f32) {
        // Binary search for the first keyframe with time > given time
        let idx = self.keyframes.partition_point(|k| k.time <= time);

        let prev_idx = idx.saturating_sub(1);
        let next_idx = idx.min(self.keyframes.len() - 1);

        let prev = &self.keyframes[prev_idx];
        let next = &self.keyframes[next_idx];

        let t = if (next.time - prev.time).abs() < 0.0001 {
            0.0
        } else {
            ((time - prev.time) / (next.time - prev.time)).clamp(0.0, 1.0)
        };

        (prev, next, t)
    }

    /// Cubic spline interpolation
    fn cubic_spline(&self, prev: &Keyframe, next: &Keyframe, t: f32) -> KeyframeValue {
        // Hermite basis functions
        let t2 = t * t;
        let t3 = t2 * t;

        let h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        let h10 = t3 - 2.0 * t2 + t;
        let h01 = -2.0 * t3 + 3.0 * t2;
        let h11 = t3 - t2;

        let dt = next.time - prev.time;

        match (&prev.value, &next.value) {
            (KeyframeValue::Vec3(p0), KeyframeValue::Vec3(p1)) => {
                let m0 = prev
                    .out_tangent
                    .as_ref()
                    .and_then(|t| {
                        if let KeyframeValue::Vec3(v) = t {
                            Some(*v)
                        } else {
                            None
                        }
                    })
                    .unwrap_or([0.0; 3]);
                let m1 = next
                    .in_tangent
                    .as_ref()
                    .and_then(|t| {
                        if let KeyframeValue::Vec3(v) = t {
                            Some(*v)
                        } else {
                            None
                        }
                    })
                    .unwrap_or([0.0; 3]);

                KeyframeValue::Vec3([
                    h00 * p0[0] + h10 * dt * m0[0] + h01 * p1[0] + h11 * dt * m1[0],
                    h00 * p0[1] + h10 * dt * m0[1] + h01 * p1[1] + h11 * dt * m1[1],
                    h00 * p0[2] + h10 * dt * m0[2] + h01 * p1[2] + h11 * dt * m1[2],
                ])
            }
            (KeyframeValue::Float(p0), KeyframeValue::Float(p1)) => {
                let m0 = prev
                    .out_tangent
                    .as_ref()
                    .and_then(|t| {
                        if let KeyframeValue::Float(v) = t {
                            Some(*v)
                        } else {
                            None
                        }
                    })
                    .unwrap_or(0.0);
                let m1 = next
                    .in_tangent
                    .as_ref()
                    .and_then(|t| {
                        if let KeyframeValue::Float(v) = t {
                            Some(*v)
                        } else {
                            None
                        }
                    })
                    .unwrap_or(0.0);

                KeyframeValue::Float(h00 * p0 + h10 * dt * m0 + h01 * p1 + h11 * dt * m1)
            }
            // For quaternions, fall back to slerp
            (KeyframeValue::Quat(a), KeyframeValue::Quat(b)) => KeyframeValue::Quat(slerp(*a, *b, t)),
            // Fallback
            _ => lerp_value(&prev.value, &next.value, t),
        }
    }
}

/// Animation event fired at specific time
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationEvent {
    /// Time in seconds
    pub time: f32,
    /// Event name
    pub name: String,
    /// Optional event data
    pub data: Option<String>,
}

impl AnimationEvent {
    /// Create a new animation event
    pub fn new(time: f32, name: impl Into<String>) -> Self {
        Self {
            time,
            name: name.into(),
            data: None,
        }
    }

    /// Create an event with data
    pub fn with_data(time: f32, name: impl Into<String>, data: impl Into<String>) -> Self {
        Self {
            time,
            name: name.into(),
            data: Some(data.into()),
        }
    }
}

/// Sampled animation pose
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct AnimationPose {
    /// Channel values (target path -> value)
    pub channels: BTreeMap<String, KeyframeValue>,
}

impl AnimationPose {
    /// Create an empty pose
    pub fn new() -> Self {
        Self::default()
    }

    /// Set a channel value
    pub fn set(&mut self, target: impl Into<String>, value: KeyframeValue) {
        self.channels.insert(target.into(), value);
    }

    /// Get a channel value
    pub fn get(&self, target: &str) -> Option<&KeyframeValue> {
        self.channels.get(target)
    }

    /// Blend two poses
    pub fn blend(a: &AnimationPose, b: &AnimationPose, t: f32) -> AnimationPose {
        let mut result = AnimationPose::new();

        // Blend matching channels
        for (target, value_a) in &a.channels {
            if let Some(value_b) = b.channels.get(target) {
                result.channels.insert(target.clone(), lerp_value(value_a, value_b, t));
            } else {
                result.channels.insert(target.clone(), value_a.clone());
            }
        }

        // Add channels only in b
        for (target, value_b) in &b.channels {
            if !a.channels.contains_key(target) {
                result.channels.insert(target.clone(), value_b.clone());
            }
        }

        result
    }
}

/// Animation clip asset
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationClip {
    /// Clip name
    pub name: String,
    /// Duration in seconds
    pub duration: f32,
    /// Animation tracks (target path -> track)
    pub tracks: BTreeMap<String, AnimationTrack>,
    /// Root motion enabled
    pub root_motion: bool,
    /// Animation events
    pub events: Vec<AnimationEvent>,
}

impl AnimationClip {
    /// Create a new animation clip
    pub fn new(name: impl Into<String>, duration: f32) -> Self {
        Self {
            name: name.into(),
            duration,
            tracks: BTreeMap::new(),
            root_motion: false,
            events: Vec::new(),
        }
    }

    /// Create an empty fallback clip
    pub fn empty() -> Self {
        Self::new("empty", 0.0)
    }

    /// Add a track to the clip
    pub fn add_track(&mut self, key: impl Into<String>, track: AnimationTrack) {
        self.tracks.insert(key.into(), track);
    }

    /// Add an event
    pub fn add_event(&mut self, event: AnimationEvent) {
        self.events.push(event);
        self.events.sort_by(|a, b| a.time.partial_cmp(&b.time).unwrap());
    }

    /// Sample the animation at a given time
    pub fn sample(&self, time: f32) -> AnimationPose {
        let time = if self.duration > 0.0 {
            time % self.duration
        } else {
            0.0
        };

        let mut pose = AnimationPose::new();

        for (target, track) in &self.tracks {
            let value = track.sample(time);
            pose.channels.insert(target.clone(), value);
        }

        pose
    }

    /// Get events in a time range
    pub fn events_in_range(&self, start: f32, end: f32) -> Vec<&AnimationEvent> {
        self.events
            .iter()
            .filter(|e| {
                if start <= end {
                    e.time >= start && e.time < end
                } else {
                    // Wrapped (for looping)
                    e.time >= start || e.time < end
                }
            })
            .collect()
    }
}

impl Default for AnimationClip {
    fn default() -> Self {
        Self::empty()
    }
}

/// Loop mode for animation playback
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum LoopMode {
    /// Play once and stop
    #[default]
    Once,
    /// Loop forever
    Loop,
    /// Play forward then backward
    PingPong,
    /// Play once and hold last frame
    ClampForever,
}

/// Animation playback state
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationState {
    /// Animation clip name/path
    pub clip: String,
    /// Current playback time
    pub time: f32,
    /// Playback speed for this animation
    pub speed: f32,
    /// Loop mode
    pub loop_mode: LoopMode,
    /// Blend weight (0-1)
    pub weight: f32,
    /// Is playing
    pub playing: bool,
    /// Play direction (true = reversed)
    pub reversed: bool,
    /// Blend target (for transitions)
    pub blend_to: Option<Box<AnimationState>>,
    /// Blend duration
    pub blend_duration: f32,
    /// Current blend time
    pub blend_time: f32,
}

impl AnimationState {
    /// Create a new animation state
    pub fn new(clip: impl Into<String>) -> Self {
        Self {
            clip: clip.into(),
            time: 0.0,
            speed: 1.0,
            loop_mode: LoopMode::Once,
            weight: 1.0,
            playing: true,
            reversed: false,
            blend_to: None,
            blend_duration: 0.0,
            blend_time: 0.0,
        }
    }

    /// Set loop mode
    pub fn with_loop_mode(mut self, mode: LoopMode) -> Self {
        self.loop_mode = mode;
        self
    }

    /// Set speed
    pub fn with_speed(mut self, speed: f32) -> Self {
        self.speed = speed;
        self
    }

    /// Set weight
    pub fn with_weight(mut self, weight: f32) -> Self {
        self.weight = weight;
        self
    }

    /// Check if blending
    pub fn is_blending(&self) -> bool {
        self.blend_to.is_some()
    }

    /// Update time based on loop mode and clip duration
    pub fn update_time(&mut self, dt: f32, clip_duration: f32) -> bool {
        if !self.playing || clip_duration <= 0.0 {
            return false;
        }

        let direction = if self.reversed { -1.0 } else { 1.0 };
        let new_time = self.time + dt * self.speed * direction;

        match self.loop_mode {
            LoopMode::Once => {
                if new_time >= clip_duration {
                    self.time = clip_duration;
                    self.playing = false;
                    true
                } else if new_time < 0.0 {
                    self.time = 0.0;
                    self.playing = false;
                    true
                } else {
                    self.time = new_time;
                    false
                }
            }
            LoopMode::Loop => {
                self.time = new_time.rem_euclid(clip_duration);
                false
            }
            LoopMode::PingPong => {
                let cycle = new_time / clip_duration;
                let in_reverse = (cycle as i32) % 2 == 1;
                let t = new_time.rem_euclid(clip_duration);
                self.time = if in_reverse { clip_duration - t } else { t };
                false
            }
            LoopMode::ClampForever => {
                self.time = new_time.clamp(0.0, clip_duration);
                false
            }
        }
    }

    /// Update blend state
    pub fn update_blend(&mut self, dt: f32, clip_duration: f32) -> bool {
        if let Some(ref mut blend_to) = self.blend_to {
            self.blend_time += dt;
            blend_to.update_time(dt * self.speed, clip_duration);

            if self.blend_time >= self.blend_duration {
                return true; // Blend complete
            }
        }
        false
    }

    /// Get blend progress (0-1)
    pub fn blend_progress(&self) -> f32 {
        if self.blend_duration <= 0.0 {
            1.0
        } else {
            (self.blend_time / self.blend_duration).clamp(0.0, 1.0)
        }
    }
}

impl Default for AnimationState {
    fn default() -> Self {
        Self::new("")
    }
}

/// Animation player component
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationPlayer {
    /// Currently playing animations (slot -> state)
    pub animations: BTreeMap<u32, AnimationState>,
    /// Default slot for simple playback
    pub active_slot: u32,
    /// Global playback speed multiplier
    pub speed: f32,
    /// Paused state
    pub paused: bool,
}

impl Default for AnimationPlayer {
    fn default() -> Self {
        Self {
            animations: BTreeMap::new(),
            active_slot: 0,
            speed: 1.0,
            paused: false,
        }
    }
}

impl AnimationPlayer {
    /// Create a new animation player
    pub fn new() -> Self {
        Self::default()
    }

    /// Play animation in default slot
    pub fn play(&mut self, clip: impl Into<String>) {
        self.play_in_slot(self.active_slot, clip);
    }

    /// Play animation in specific slot
    pub fn play_in_slot(&mut self, slot: u32, clip: impl Into<String>) {
        self.animations.insert(slot, AnimationState::new(clip));
    }

    /// Play with looping
    pub fn play_looped(&mut self, clip: impl Into<String>) {
        self.play(clip);
        if let Some(state) = self.animations.get_mut(&self.active_slot) {
            state.loop_mode = LoopMode::Loop;
        }
    }

    /// Play looped in specific slot
    pub fn play_looped_in_slot(&mut self, slot: u32, clip: impl Into<String>) {
        self.play_in_slot(slot, clip);
        if let Some(state) = self.animations.get_mut(&slot) {
            state.loop_mode = LoopMode::Loop;
        }
    }

    /// Stop animation in slot
    pub fn stop(&mut self, slot: u32) {
        if let Some(state) = self.animations.get_mut(&slot) {
            state.playing = false;
        }
    }

    /// Stop all animations
    pub fn stop_all(&mut self) {
        for state in self.animations.values_mut() {
            state.playing = false;
        }
    }

    /// Remove animation from slot
    pub fn remove(&mut self, slot: u32) {
        self.animations.remove(&slot);
    }

    /// Clear all animations
    pub fn clear(&mut self) {
        self.animations.clear();
    }

    /// Pause/resume playback
    pub fn set_paused(&mut self, paused: bool) {
        self.paused = paused;
    }

    /// Toggle pause
    pub fn toggle_pause(&mut self) {
        self.paused = !self.paused;
    }

    /// Seek to time in slot
    pub fn seek(&mut self, slot: u32, time: f32) {
        if let Some(state) = self.animations.get_mut(&slot) {
            state.time = time.max(0.0);
        }
    }

    /// Set global playback speed
    pub fn set_speed(&mut self, speed: f32) {
        self.speed = speed;
    }

    /// Set slot-specific speed
    pub fn set_slot_speed(&mut self, slot: u32, speed: f32) {
        if let Some(state) = self.animations.get_mut(&slot) {
            state.speed = speed;
        }
    }

    /// Set loop mode for slot
    pub fn set_loop_mode(&mut self, slot: u32, mode: LoopMode) {
        if let Some(state) = self.animations.get_mut(&slot) {
            state.loop_mode = mode;
        }
    }

    /// Blend to new animation
    pub fn blend_to(&mut self, slot: u32, clip: impl Into<String>, duration: f32) {
        if let Some(current) = self.animations.get_mut(&slot) {
            current.blend_to = Some(Box::new(
                AnimationState::new(clip).with_loop_mode(current.loop_mode),
            ));
            current.blend_duration = duration;
            current.blend_time = 0.0;
        } else {
            // No current animation, just play normally
            self.play_in_slot(slot, clip);
        }
    }

    /// Cross-fade between animations
    pub fn crossfade(&mut self, clip: impl Into<String>, duration: f32) {
        self.blend_to(self.active_slot, clip, duration);
    }

    /// Check if animation is playing in slot
    pub fn is_playing(&self, slot: u32) -> bool {
        self.animations
            .get(&slot)
            .map(|s| s.playing && !self.paused)
            .unwrap_or(false)
    }

    /// Check if any animation is playing
    pub fn is_any_playing(&self) -> bool {
        !self.paused && self.animations.values().any(|s| s.playing)
    }

    /// Get current time in slot
    pub fn current_time(&self, slot: u32) -> Option<f32> {
        self.animations.get(&slot).map(|s| s.time)
    }

    /// Get animation state for slot
    pub fn get_state(&self, slot: u32) -> Option<&AnimationState> {
        self.animations.get(&slot)
    }

    /// Get mutable animation state for slot
    pub fn get_state_mut(&mut self, slot: u32) -> Option<&mut AnimationState> {
        self.animations.get_mut(&slot)
    }

    /// Get number of active animations
    pub fn animation_count(&self) -> usize {
        self.animations.len()
    }

    /// Get all active slots
    pub fn active_slots(&self) -> impl Iterator<Item = u32> + '_ {
        self.animations.keys().copied()
    }
}

// ============================================================================
// Interpolation functions
// ============================================================================

/// Linear interpolation between two keyframe values
pub fn lerp_value(a: &KeyframeValue, b: &KeyframeValue, t: f32) -> KeyframeValue {
    match (a, b) {
        (KeyframeValue::Float(a), KeyframeValue::Float(b)) => {
            KeyframeValue::Float(a + (b - a) * t)
        }
        (KeyframeValue::Vec3(a), KeyframeValue::Vec3(b)) => KeyframeValue::Vec3([
            a[0] + (b[0] - a[0]) * t,
            a[1] + (b[1] - a[1]) * t,
            a[2] + (b[2] - a[2]) * t,
        ]),
        (KeyframeValue::Quat(a), KeyframeValue::Quat(b)) => KeyframeValue::Quat(slerp(*a, *b, t)),
        (KeyframeValue::Weights(a), KeyframeValue::Weights(b)) => {
            KeyframeValue::Weights(a.iter().zip(b.iter()).map(|(a, b)| a + (b - a) * t).collect())
        }
        // Fallback: return a if types don't match
        _ => a.clone(),
    }
}

/// Spherical linear interpolation for quaternions
pub fn slerp(a: [f32; 4], b: [f32; 4], t: f32) -> [f32; 4] {
    // Compute dot product
    let dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];

    // If dot is negative, negate b to take shorter path
    let (b, dot) = if dot < 0.0 {
        ([-b[0], -b[1], -b[2], -b[3]], -dot)
    } else {
        (b, dot)
    };

    // Clamp dot to valid range for acos
    let dot = dot.clamp(-1.0, 1.0);

    // If quaternions are very close, use linear interpolation
    if dot > 0.9995 {
        let result = [
            a[0] + t * (b[0] - a[0]),
            a[1] + t * (b[1] - a[1]),
            a[2] + t * (b[2] - a[2]),
            a[3] + t * (b[3] - a[3]),
        ];
        return normalize_quat(result);
    }

    // Standard slerp formula
    let theta_0 = dot.acos();
    let theta = theta_0 * t;
    let sin_theta = theta.sin();
    let sin_theta_0 = theta_0.sin();

    let s0 = theta.cos() - dot * sin_theta / sin_theta_0;
    let s1 = sin_theta / sin_theta_0;

    normalize_quat([
        s0 * a[0] + s1 * b[0],
        s0 * a[1] + s1 * b[1],
        s0 * a[2] + s1 * b[2],
        s0 * a[3] + s1 * b[3],
    ])
}

/// Normalize a quaternion
pub fn normalize_quat(q: [f32; 4]) -> [f32; 4] {
    let len_sq = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    if len_sq < 0.0001 {
        return [0.0, 0.0, 0.0, 1.0]; // Return identity if near-zero
    }
    let len = len_sq.sqrt();
    [q[0] / len, q[1] / len, q[2] / len, q[3] / len]
}

// ============================================================================
// Hot-reload state
// ============================================================================

/// Animation player state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationPlayerState {
    /// Animation states
    pub animations: BTreeMap<u32, AnimationState>,
    /// Active slot
    pub active_slot: u32,
    /// Speed
    pub speed: f32,
    /// Paused
    pub paused: bool,
}

impl AnimationPlayer {
    /// Save state for hot-reload
    pub fn save_state(&self) -> AnimationPlayerState {
        AnimationPlayerState {
            animations: self.animations.clone(),
            active_slot: self.active_slot,
            speed: self.speed,
            paused: self.paused,
        }
    }

    /// Restore state from hot-reload
    pub fn restore_state(&mut self, state: AnimationPlayerState) {
        self.animations = state.animations;
        self.active_slot = state.active_slot;
        self.speed = state.speed;
        self.paused = state.paused;
    }
}

/// Animation clip state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationClipState {
    /// Clip name
    pub name: String,
    /// Duration
    pub duration: f32,
    /// Tracks
    pub tracks: BTreeMap<String, AnimationTrack>,
    /// Root motion
    pub root_motion: bool,
    /// Events
    pub events: Vec<AnimationEvent>,
}

impl AnimationClip {
    /// Save state for hot-reload
    pub fn save_state(&self) -> AnimationClipState {
        AnimationClipState {
            name: self.name.clone(),
            duration: self.duration,
            tracks: self.tracks.clone(),
            root_motion: self.root_motion,
            events: self.events.clone(),
        }
    }

    /// Restore state from hot-reload
    pub fn restore_state(&mut self, state: AnimationClipState) {
        self.name = state.name;
        self.duration = state.duration;
        self.tracks = state.tracks;
        self.root_motion = state.root_motion;
        self.events = state.events;
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_keyframe_value_types() {
        let float = KeyframeValue::float(1.5);
        assert!(matches!(float, KeyframeValue::Float(v) if (v - 1.5).abs() < 0.001));

        let vec3 = KeyframeValue::vec3([1.0, 2.0, 3.0]);
        assert!(matches!(vec3, KeyframeValue::Vec3([1.0, 2.0, 3.0])));

        let quat = KeyframeValue::quat_identity();
        assert!(matches!(quat, KeyframeValue::Quat([0.0, 0.0, 0.0, 1.0])));
    }

    #[test]
    fn test_linear_interpolation_float() {
        let track = AnimationTrack {
            target: "test".to_string(),
            property: AnimatedProperty::Translation,
            keyframes: vec![Keyframe::float(0.0, 0.0), Keyframe::float(1.0, 10.0)],
            interpolation: Interpolation::Linear,
        };

        let value = track.sample(0.5);
        if let KeyframeValue::Float(v) = value {
            assert!((v - 5.0).abs() < 0.01);
        } else {
            panic!("Expected float value");
        }
    }

    #[test]
    fn test_linear_interpolation_vec3() {
        let track = AnimationTrack {
            target: "test".to_string(),
            property: AnimatedProperty::Translation,
            keyframes: vec![
                Keyframe::vec3(0.0, [0.0, 0.0, 0.0]),
                Keyframe::vec3(1.0, [10.0, 20.0, 30.0]),
            ],
            interpolation: Interpolation::Linear,
        };

        let value = track.sample(0.5);
        if let KeyframeValue::Vec3(v) = value {
            assert!((v[0] - 5.0).abs() < 0.01);
            assert!((v[1] - 10.0).abs() < 0.01);
            assert!((v[2] - 15.0).abs() < 0.01);
        } else {
            panic!("Expected Vec3 value");
        }
    }

    #[test]
    fn test_step_interpolation() {
        let track = AnimationTrack {
            target: "test".to_string(),
            property: AnimatedProperty::Translation,
            keyframes: vec![
                Keyframe::float(0.0, 0.0),
                Keyframe::float(0.5, 5.0),
                Keyframe::float(1.0, 10.0),
            ],
            interpolation: Interpolation::Step,
        };

        // Should return previous keyframe value
        let value = track.sample(0.25);
        if let KeyframeValue::Float(v) = value {
            assert!((v - 0.0).abs() < 0.01);
        }

        let value = track.sample(0.75);
        if let KeyframeValue::Float(v) = value {
            assert!((v - 5.0).abs() < 0.01);
        }
    }

    #[test]
    fn test_quaternion_slerp() {
        // Identity quaternion
        let a = [0.0, 0.0, 0.0, 1.0];
        // 90 degree rotation around Y
        let b = [0.0, 0.707, 0.0, 0.707];

        let mid = slerp(a, b, 0.5);

        // Should be approximately 45 degrees around Y
        assert!((mid[1] - 0.383).abs() < 0.01);
        assert!((mid[3] - 0.924).abs() < 0.01);
    }

    #[test]
    fn test_animation_clip_sample() {
        let mut clip = AnimationClip::new("test", 2.0);
        clip.add_track(
            "root/translation",
            AnimationTrack {
                target: "root".to_string(),
                property: AnimatedProperty::Translation,
                keyframes: vec![
                    Keyframe::vec3(0.0, [0.0, 0.0, 0.0]),
                    Keyframe::vec3(1.0, [0.0, 5.0, 0.0]),
                    Keyframe::vec3(2.0, [0.0, 0.0, 0.0]),
                ],
                interpolation: Interpolation::Linear,
            },
        );

        let pose = clip.sample(0.5);
        let value = pose.get("root/translation").unwrap();
        if let KeyframeValue::Vec3(v) = value {
            assert!((v[1] - 2.5).abs() < 0.01);
        }
    }

    #[test]
    fn test_loop_mode_once() {
        let mut state = AnimationState::new("test");
        state.loop_mode = LoopMode::Once;

        let finished = state.update_time(0.5, 1.0);
        assert!(!finished);
        assert!((state.time - 0.5).abs() < 0.01);

        let finished = state.update_time(0.6, 1.0);
        assert!(finished);
        assert!((state.time - 1.0).abs() < 0.01);
        assert!(!state.playing);
    }

    #[test]
    fn test_loop_mode_loop() {
        let mut state = AnimationState::new("test");
        state.loop_mode = LoopMode::Loop;

        state.update_time(0.5, 1.0);
        assert!((state.time - 0.5).abs() < 0.01);

        state.update_time(0.7, 1.0);
        // Should wrap to 0.2
        assert!((state.time - 0.2).abs() < 0.01);
    }

    #[test]
    fn test_loop_mode_pingpong() {
        let mut state = AnimationState::new("test");
        state.loop_mode = LoopMode::PingPong;

        state.update_time(0.75, 1.0);
        assert!((state.time - 0.75).abs() < 0.01);

        state.update_time(0.5, 1.0);
        // Time = 1.25, should be playing backwards at 0.75
        assert!((state.time - 0.75).abs() < 0.01);
    }

    #[test]
    fn test_animation_player() {
        let mut player = AnimationPlayer::new();

        player.play_looped("walk");
        assert!(player.is_playing(0));

        player.set_paused(true);
        assert!(!player.is_playing(0));

        player.set_paused(false);
        player.seek(0, 0.5);
        assert!((player.current_time(0).unwrap() - 0.5).abs() < 0.01);

        player.stop(0);
        assert!(!player.is_playing(0));
    }

    #[test]
    fn test_animation_blending() {
        let mut player = AnimationPlayer::new();
        player.play("idle");
        player.blend_to(0, "walk", 0.5);

        let state = player.get_state(0).unwrap();
        assert!(state.is_blending());
        assert!(state.blend_to.is_some());
    }

    #[test]
    fn test_pose_blending() {
        let mut pose_a = AnimationPose::new();
        pose_a.set("root", KeyframeValue::Vec3([0.0, 0.0, 0.0]));

        let mut pose_b = AnimationPose::new();
        pose_b.set("root", KeyframeValue::Vec3([10.0, 0.0, 0.0]));

        let blended = AnimationPose::blend(&pose_a, &pose_b, 0.5);
        if let Some(KeyframeValue::Vec3(v)) = blended.get("root") {
            assert!((v[0] - 5.0).abs() < 0.01);
        }
    }

    #[test]
    fn test_animation_events() {
        let mut clip = AnimationClip::new("test", 2.0);
        clip.add_event(AnimationEvent::new(0.5, "footstep"));
        clip.add_event(AnimationEvent::new(1.5, "footstep"));

        let events = clip.events_in_range(0.4, 0.6);
        assert_eq!(events.len(), 1);
        assert_eq!(events[0].name, "footstep");
    }

    #[test]
    fn test_animation_player_serialization() {
        let mut player = AnimationPlayer::new();
        player.play_looped("walk");
        player.speed = 1.5;

        if let Some(state) = player.get_state_mut(0) {
            state.time = 0.75;
        }

        let saved = player.save_state();
        let json = serde_json::to_string(&saved).unwrap();
        let restored: AnimationPlayerState = serde_json::from_str(&json).unwrap();

        let mut new_player = AnimationPlayer::new();
        new_player.restore_state(restored);

        assert_eq!(new_player.speed, 1.5);
        assert!((new_player.current_time(0).unwrap() - 0.75).abs() < 0.01);
    }

    #[test]
    fn test_animation_clip_serialization() {
        let mut clip = AnimationClip::new("walk", 2.0);
        clip.add_track(
            "root/translation",
            AnimationTrack {
                target: "root".to_string(),
                property: AnimatedProperty::Translation,
                keyframes: vec![
                    Keyframe::vec3(0.0, [0.0, 0.0, 0.0]),
                    Keyframe::vec3(2.0, [0.0, 1.0, 0.0]),
                ],
                interpolation: Interpolation::Linear,
            },
        );

        let saved = clip.save_state();
        let json = serde_json::to_string(&saved).unwrap();
        let restored: AnimationClipState = serde_json::from_str(&json).unwrap();

        let mut new_clip = AnimationClip::empty();
        new_clip.restore_state(restored);

        assert_eq!(new_clip.name, "walk");
        assert_eq!(new_clip.duration, 2.0);
        assert_eq!(new_clip.tracks.len(), 1);
    }

    #[test]
    fn test_multiple_animation_slots() {
        let mut player = AnimationPlayer::new();

        player.play_in_slot(0, "idle");
        player.play_looped_in_slot(1, "breathing");
        player.play_in_slot(2, "blink");

        assert_eq!(player.animation_count(), 3);
        assert!(player.is_playing(0));
        assert!(player.is_playing(1));
        assert!(player.is_playing(2));

        player.stop(0);
        assert!(!player.is_playing(0));
        assert!(player.is_playing(1));

        player.stop_all();
        assert!(!player.is_any_playing());
    }

    #[test]
    fn test_track_duration() {
        let track = AnimationTrack {
            target: "test".to_string(),
            property: AnimatedProperty::Translation,
            keyframes: vec![
                Keyframe::float(0.0, 0.0),
                Keyframe::float(1.5, 5.0),
                Keyframe::float(3.0, 10.0),
            ],
            interpolation: Interpolation::Linear,
        };

        assert!((track.duration() - 3.0).abs() < 0.01);
    }

    #[test]
    fn test_empty_track() {
        let track = AnimationTrack::new("test", AnimatedProperty::Translation);
        let value = track.sample(0.5);
        assert!(matches!(value, KeyframeValue::Float(0.0)));
    }

    #[test]
    fn test_single_keyframe() {
        let track = AnimationTrack {
            target: "test".to_string(),
            property: AnimatedProperty::Translation,
            keyframes: vec![Keyframe::vec3(0.0, [1.0, 2.0, 3.0])],
            interpolation: Interpolation::Linear,
        };

        let value = track.sample(0.5);
        if let KeyframeValue::Vec3(v) = value {
            assert!((v[0] - 1.0).abs() < 0.01);
        }
    }
}
