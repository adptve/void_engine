# Phase 8: Keyframe Animation System

## Status: Not Started

## User Story

> As a scene author, I want to play authored animations instead of only procedural motion.

## Requirements Checklist

- [ ] Support animation clips
- [ ] Support keyframe tracks for: Translation, Rotation, Scale
- [ ] Allow multiple clips per entity
- [ ] Support looping and one-shot playback
- [ ] Support animation time scaling
- [ ] Allow animation binding to imported meshes

## Current State Analysis

### Existing Implementation

The codebase currently has animation configuration in TOML scene format but no runtime animation system.

**void_runtime config:**
```toml
[[animations]]
entity = "floating_cube"
property = "transform.position.y"
keyframes = [
    { time = 0.0, value = 0.0, easing = "linear" },
    { time = 2.0, value = 2.0, easing = "ease_in_out" },
    { time = 4.0, value = 0.0, easing = "linear" },
]
loop_mode = "repeat"
```

### Gaps
1. No animation player component
2. No animation clip asset type
3. No skeletal animation support
4. No animation events
5. No animation graph/state machine
6. No root motion support
7. No animation compression

## Implementation Specification

### 1. Animation Clip Asset

```rust
// crates/void_asset/src/animation.rs (NEW FILE)

use std::collections::HashMap;

/// Animation clip asset
#[derive(Clone, Debug)]
pub struct AnimationClip {
    /// Unique asset identifier
    pub id: AssetId,

    /// Asset path
    pub path: String,

    /// Clip name
    pub name: String,

    /// Duration in seconds
    pub duration: f32,

    /// Animation tracks (target path -> track)
    pub tracks: HashMap<String, AnimationTrack>,

    /// Root motion enabled
    pub root_motion: bool,

    /// Animation events
    pub events: Vec<AnimationEvent>,
}

/// Animation track for a single property
#[derive(Clone, Debug)]
pub struct AnimationTrack {
    /// Target entity path (e.g., "armature/spine/head")
    pub target: String,

    /// Property being animated
    pub property: AnimatedProperty,

    /// Keyframes
    pub keyframes: Vec<Keyframe>,

    /// Interpolation mode
    pub interpolation: Interpolation,
}

#[derive(Clone, Copy, Debug)]
pub enum AnimatedProperty {
    Translation,
    Rotation,
    Scale,
    Weights,  // Morph targets
    Custom(u32),  // Custom property index
}

/// Single keyframe
#[derive(Clone, Debug)]
pub struct Keyframe {
    /// Time in seconds
    pub time: f32,

    /// Value (interpretation depends on property)
    pub value: KeyframeValue,

    /// Tangent for cubic interpolation
    pub in_tangent: Option<KeyframeValue>,
    pub out_tangent: Option<KeyframeValue>,
}

#[derive(Clone, Debug)]
pub enum KeyframeValue {
    Float(f32),
    Vec3([f32; 3]),
    Quat([f32; 4]),
    Weights(Vec<f32>),
}

#[derive(Clone, Copy, Debug, Default)]
pub enum Interpolation {
    Step,
    #[default]
    Linear,
    CubicSpline,
}

/// Animation event fired at specific time
#[derive(Clone, Debug)]
pub struct AnimationEvent {
    pub time: f32,
    pub name: String,
    pub data: Option<String>,
}

impl AnimationClip {
    /// Sample animation at given time
    pub fn sample(&self, time: f32) -> AnimationPose {
        let time = time % self.duration;  // Wrap for looping

        let mut pose = AnimationPose::default();

        for (target, track) in &self.tracks {
            let value = track.sample(time);
            pose.channels.insert(target.clone(), value);
        }

        pose
    }
}

impl AnimationTrack {
    /// Sample track at given time
    pub fn sample(&self, time: f32) -> KeyframeValue {
        // Find surrounding keyframes
        let (prev, next, t) = self.find_keyframes(time);

        match self.interpolation {
            Interpolation::Step => prev.value.clone(),
            Interpolation::Linear => Self::lerp(&prev.value, &next.value, t),
            Interpolation::CubicSpline => Self::cubic_spline(prev, next, t),
        }
    }

    fn find_keyframes(&self, time: f32) -> (&Keyframe, &Keyframe, f32) {
        // Binary search for keyframes
        let idx = self.keyframes.partition_point(|k| k.time <= time);

        let prev_idx = idx.saturating_sub(1);
        let next_idx = (idx).min(self.keyframes.len() - 1);

        let prev = &self.keyframes[prev_idx];
        let next = &self.keyframes[next_idx];

        let t = if (next.time - prev.time).abs() < 0.0001 {
            0.0
        } else {
            (time - prev.time) / (next.time - prev.time)
        };

        (prev, next, t)
    }

    fn lerp(a: &KeyframeValue, b: &KeyframeValue, t: f32) -> KeyframeValue {
        match (a, b) {
            (KeyframeValue::Float(a), KeyframeValue::Float(b)) => {
                KeyframeValue::Float(a + (b - a) * t)
            }
            (KeyframeValue::Vec3(a), KeyframeValue::Vec3(b)) => {
                KeyframeValue::Vec3([
                    a[0] + (b[0] - a[0]) * t,
                    a[1] + (b[1] - a[1]) * t,
                    a[2] + (b[2] - a[2]) * t,
                ])
            }
            (KeyframeValue::Quat(a), KeyframeValue::Quat(b)) => {
                // Spherical linear interpolation
                KeyframeValue::Quat(slerp(*a, *b, t))
            }
            (KeyframeValue::Weights(a), KeyframeValue::Weights(b)) => {
                KeyframeValue::Weights(
                    a.iter().zip(b.iter())
                        .map(|(a, b)| a + (b - a) * t)
                        .collect()
                )
            }
            _ => a.clone(),
        }
    }

    fn cubic_spline(prev: &Keyframe, next: &Keyframe, t: f32) -> KeyframeValue {
        // Hermite spline interpolation
        let t2 = t * t;
        let t3 = t2 * t;

        let h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        let h10 = t3 - 2.0 * t2 + t;
        let h01 = -2.0 * t3 + 3.0 * t2;
        let h11 = t3 - t2;

        let dt = next.time - prev.time;

        // For Vec3 values (simplified)
        if let (KeyframeValue::Vec3(p0), KeyframeValue::Vec3(p1)) = (&prev.value, &next.value) {
            let m0 = prev.out_tangent.as_ref()
                .and_then(|t| if let KeyframeValue::Vec3(v) = t { Some(*v) } else { None })
                .unwrap_or([0.0; 3]);
            let m1 = next.in_tangent.as_ref()
                .and_then(|t| if let KeyframeValue::Vec3(v) = t { Some(*v) } else { None })
                .unwrap_or([0.0; 3]);

            let result = [
                h00 * p0[0] + h10 * dt * m0[0] + h01 * p1[0] + h11 * dt * m1[0],
                h00 * p0[1] + h10 * dt * m0[1] + h01 * p1[1] + h11 * dt * m1[1],
                h00 * p0[2] + h10 * dt * m0[2] + h01 * p1[2] + h11 * dt * m1[2],
            ];

            return KeyframeValue::Vec3(result);
        }

        // Fallback to linear
        Self::lerp(&prev.value, &next.value, t)
    }
}

/// Sampled animation pose
#[derive(Clone, Debug, Default)]
pub struct AnimationPose {
    pub channels: HashMap<String, KeyframeValue>,
}

fn slerp(a: [f32; 4], b: [f32; 4], t: f32) -> [f32; 4] {
    // Quaternion spherical linear interpolation
    let dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];

    let (b, dot) = if dot < 0.0 {
        ([-b[0], -b[1], -b[2], -b[3]], -dot)
    } else {
        (b, dot)
    };

    if dot > 0.9995 {
        // Linear interpolation for nearly identical quaternions
        let result = [
            a[0] + t * (b[0] - a[0]),
            a[1] + t * (b[1] - a[1]),
            a[2] + t * (b[2] - a[2]),
            a[3] + t * (b[3] - a[3]),
        ];
        normalize_quat(result)
    } else {
        let theta_0 = dot.acos();
        let theta = theta_0 * t;
        let sin_theta = theta.sin();
        let sin_theta_0 = theta_0.sin();

        let s0 = (theta_0 - theta).cos() - dot * sin_theta / sin_theta_0;
        let s1 = sin_theta / sin_theta_0;

        [
            s0 * a[0] + s1 * b[0],
            s0 * a[1] + s1 * b[1],
            s0 * a[2] + s1 * b[2],
            s0 * a[3] + s1 * b[3],
        ]
    }
}

fn normalize_quat(q: [f32; 4]) -> [f32; 4] {
    let len = (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]).sqrt();
    [q[0] / len, q[1] / len, q[2] / len, q[3] / len]
}
```

### 2. Animation Player Component

```rust
// crates/void_ecs/src/components/animation.rs (NEW FILE)

use std::collections::HashMap;

/// Controls animation playback on an entity
#[derive(Clone, Debug)]
pub struct AnimationPlayer {
    /// Currently playing animations (slot -> state)
    pub animations: HashMap<u32, AnimationState>,

    /// Default slot for simple playback
    pub active_slot: u32,

    /// Playback speed multiplier
    pub speed: f32,

    /// Paused state
    pub paused: bool,
}

#[derive(Clone, Debug)]
pub struct AnimationState {
    /// Animation clip asset path
    pub clip: String,

    /// Cached clip ID
    pub clip_id: Option<AssetId>,

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

    /// Play direction
    pub reversed: bool,

    /// Blend target (for transitions)
    pub blend_to: Option<Box<AnimationState>>,
    pub blend_duration: f32,
    pub blend_time: f32,
}

#[derive(Clone, Copy, Debug, Default)]
pub enum LoopMode {
    #[default]
    Once,
    Loop,
    PingPong,
    ClampForever,
}

impl Default for AnimationPlayer {
    fn default() -> Self {
        Self {
            animations: HashMap::new(),
            active_slot: 0,
            speed: 1.0,
            paused: false,
        }
    }
}

impl AnimationPlayer {
    /// Play animation in default slot
    pub fn play(&mut self, clip: impl Into<String>) {
        self.play_in_slot(self.active_slot, clip);
    }

    /// Play animation in specific slot
    pub fn play_in_slot(&mut self, slot: u32, clip: impl Into<String>) {
        self.animations.insert(slot, AnimationState {
            clip: clip.into(),
            clip_id: None,
            time: 0.0,
            speed: 1.0,
            loop_mode: LoopMode::Once,
            weight: 1.0,
            playing: true,
            reversed: false,
            blend_to: None,
            blend_duration: 0.0,
            blend_time: 0.0,
        });
    }

    /// Play with looping
    pub fn play_looped(&mut self, clip: impl Into<String>) {
        self.play(clip);
        if let Some(state) = self.animations.get_mut(&self.active_slot) {
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

    /// Pause/resume
    pub fn set_paused(&mut self, paused: bool) {
        self.paused = paused;
    }

    /// Seek to time
    pub fn seek(&mut self, slot: u32, time: f32) {
        if let Some(state) = self.animations.get_mut(&slot) {
            state.time = time;
        }
    }

    /// Set playback speed
    pub fn set_speed(&mut self, speed: f32) {
        self.speed = speed;
    }

    /// Blend to new animation
    pub fn blend_to(&mut self, slot: u32, clip: impl Into<String>, duration: f32) {
        if let Some(current) = self.animations.get_mut(&slot) {
            current.blend_to = Some(Box::new(AnimationState {
                clip: clip.into(),
                clip_id: None,
                time: 0.0,
                speed: 1.0,
                loop_mode: current.loop_mode,
                weight: 0.0,
                playing: true,
                reversed: false,
                blend_to: None,
                blend_duration: 0.0,
                blend_time: 0.0,
            }));
            current.blend_duration = duration;
            current.blend_time = 0.0;
        }
    }

    /// Check if animation is playing
    pub fn is_playing(&self, slot: u32) -> bool {
        self.animations.get(&slot).map(|s| s.playing).unwrap_or(false)
    }

    /// Get current time
    pub fn current_time(&self, slot: u32) -> Option<f32> {
        self.animations.get(&slot).map(|s| s.time)
    }
}
```

### 3. Animation System

```rust
// crates/void_ecs/src/systems/animation_system.rs (NEW FILE)

use crate::{World, Entity};
use crate::components::{AnimationPlayer, LocalTransform};
use void_asset::animation::{AnimationClip, AnimationPose, KeyframeValue};

/// Updates animation playback and applies poses
pub struct AnimationSystem;

impl AnimationSystem {
    pub fn update(
        world: &mut World,
        dt: f32,
        clip_cache: &HashMap<AssetId, AnimationClip>,
    ) {
        // Collect entities with animation players
        let entities: Vec<Entity> = world.query::<&AnimationPlayer>()
            .map(|(e, _)| e)
            .collect();

        for entity in entities {
            let player = world.get::<AnimationPlayer>(entity).unwrap().clone();

            if player.paused {
                continue;
            }

            // Update each animation slot
            for (slot, state) in &player.animations {
                if !state.playing {
                    continue;
                }

                // Get clip
                let clip = match state.clip_id.and_then(|id| clip_cache.get(&id)) {
                    Some(c) => c,
                    None => continue,
                };

                // Update time
                let speed = player.speed * state.speed * if state.reversed { -1.0 } else { 1.0 };
                let new_time = state.time + dt * speed;

                // Handle loop modes
                let (new_time, finished) = match state.loop_mode {
                    LoopMode::Once => {
                        if new_time >= clip.duration {
                            (clip.duration, true)
                        } else if new_time < 0.0 {
                            (0.0, true)
                        } else {
                            (new_time, false)
                        }
                    }
                    LoopMode::Loop => {
                        (new_time.rem_euclid(clip.duration), false)
                    }
                    LoopMode::PingPong => {
                        let cycle = new_time / clip.duration;
                        let in_reverse = (cycle as i32) % 2 == 1;
                        let t = new_time.rem_euclid(clip.duration);
                        (if in_reverse { clip.duration - t } else { t }, false)
                    }
                    LoopMode::ClampForever => {
                        (new_time.clamp(0.0, clip.duration), false)
                    }
                };

                // Sample pose
                let pose = clip.sample(new_time);

                // Handle blending
                let final_pose = if let Some(blend_to) = &state.blend_to {
                    let blend_clip = blend_to.clip_id
                        .and_then(|id| clip_cache.get(&id));

                    if let Some(blend_clip) = blend_clip {
                        let blend_pose = blend_clip.sample(blend_to.time);
                        let t = state.blend_time / state.blend_duration;
                        Self::blend_poses(&pose, &blend_pose, t.min(1.0))
                    } else {
                        pose
                    }
                } else {
                    pose
                };

                // Apply pose to entity hierarchy
                Self::apply_pose(world, entity, &final_pose, state.weight);

                // Update state
                if let Some(player) = world.get_mut::<AnimationPlayer>(entity) {
                    if let Some(state) = player.animations.get_mut(slot) {
                        state.time = new_time;

                        if finished {
                            state.playing = false;
                        }

                        // Update blend
                        if state.blend_to.is_some() {
                            state.blend_time += dt;

                            if state.blend_time >= state.blend_duration {
                                // Transition complete
                                if let Some(mut blend_to) = state.blend_to.take() {
                                    *state = *blend_to;
                                }
                            } else if let Some(blend_to) = &mut state.blend_to {
                                blend_to.time += dt * player.speed * blend_to.speed;
                            }
                        }
                    }
                }

                // Fire animation events
                Self::fire_events(world, entity, clip, state.time, new_time);
            }
        }
    }

    fn blend_poses(a: &AnimationPose, b: &AnimationPose, t: f32) -> AnimationPose {
        let mut result = AnimationPose::default();

        // Blend matching channels
        for (target, value_a) in &a.channels {
            if let Some(value_b) = b.channels.get(target) {
                result.channels.insert(target.clone(), Self::blend_values(value_a, value_b, t));
            } else {
                result.channels.insert(target.clone(), value_a.clone());
            }
        }

        // Add channels only in B
        for (target, value_b) in &b.channels {
            if !a.channels.contains_key(target) {
                result.channels.insert(target.clone(), value_b.clone());
            }
        }

        result
    }

    fn blend_values(a: &KeyframeValue, b: &KeyframeValue, t: f32) -> KeyframeValue {
        match (a, b) {
            (KeyframeValue::Vec3(a), KeyframeValue::Vec3(b)) => {
                KeyframeValue::Vec3([
                    a[0] + (b[0] - a[0]) * t,
                    a[1] + (b[1] - a[1]) * t,
                    a[2] + (b[2] - a[2]) * t,
                ])
            }
            (KeyframeValue::Quat(a), KeyframeValue::Quat(b)) => {
                KeyframeValue::Quat(slerp(*a, *b, t))
            }
            _ => if t < 0.5 { a.clone() } else { b.clone() }
        }
    }

    fn apply_pose(world: &mut World, root: Entity, pose: &AnimationPose, weight: f32) {
        for (target, value) in &pose.channels {
            // Find target entity in hierarchy
            let target_entity = Self::find_child_by_path(world, root, target);

            if let Some(entity) = target_entity {
                if let Some(mut transform) = world.get_mut::<LocalTransform>(entity) {
                    match value {
                        KeyframeValue::Vec3(v) if target.ends_with("/translation") => {
                            if weight >= 1.0 {
                                transform.translation = *v;
                            } else {
                                // Blend with current
                                transform.translation = [
                                    transform.translation[0] + (v[0] - transform.translation[0]) * weight,
                                    transform.translation[1] + (v[1] - transform.translation[1]) * weight,
                                    transform.translation[2] + (v[2] - transform.translation[2]) * weight,
                                ];
                            }
                        }
                        KeyframeValue::Quat(q) if target.ends_with("/rotation") => {
                            if weight >= 1.0 {
                                transform.rotation = *q;
                            } else {
                                transform.rotation = slerp(transform.rotation, *q, weight);
                            }
                        }
                        KeyframeValue::Vec3(v) if target.ends_with("/scale") => {
                            if weight >= 1.0 {
                                transform.scale = *v;
                            } else {
                                transform.scale = [
                                    transform.scale[0] + (v[0] - transform.scale[0]) * weight,
                                    transform.scale[1] + (v[1] - transform.scale[1]) * weight,
                                    transform.scale[2] + (v[2] - transform.scale[2]) * weight,
                                ];
                            }
                        }
                        _ => {}
                    }
                }
            }
        }
    }

    fn find_child_by_path(world: &World, root: Entity, path: &str) -> Option<Entity> {
        // Parse path like "armature/spine/head"
        let parts: Vec<&str> = path.split('/').collect();
        let mut current = root;

        for part in parts {
            // Find child with matching name
            if let Some(children) = world.get::<Children>(current) {
                let mut found = false;
                for child in children.iter() {
                    if let Some(name) = world.get::<Name>(*child) {
                        if name.0 == part {
                            current = *child;
                            found = true;
                            break;
                        }
                    }
                }
                if !found {
                    return None;
                }
            } else {
                return None;
            }
        }

        Some(current)
    }

    fn fire_events(world: &World, entity: Entity, clip: &AnimationClip, prev_time: f32, new_time: f32) {
        for event in &clip.events {
            // Check if event time was crossed
            let crossed = (prev_time <= event.time && new_time > event.time) ||
                         (prev_time >= event.time && new_time < event.time);

            if crossed {
                // Fire event (through event system)
                log::debug!("Animation event: {} on {:?}", event.name, entity);
                // TODO: Dispatch through void_event
            }
        }
    }
}
```

### 4. glTF Animation Loader

```rust
// crates/void_asset_server/src/loaders/gltf_animation.rs (NEW FILE)

use gltf::animation::Interpolation as GltfInterpolation;
use void_asset::animation::*;

impl GltfLoader {
    pub fn load_animations(document: &gltf::Document, buffers: &[gltf::buffer::Data])
        -> Vec<AnimationClip>
    {
        let mut clips = Vec::new();

        for animation in document.animations() {
            let mut tracks = HashMap::new();
            let mut max_time = 0.0f32;

            for channel in animation.channels() {
                let reader = channel.reader(|buffer| Some(&buffers[buffer.index()]));

                let sampler = channel.sampler();
                let target = channel.target();

                // Get target node name
                let node_name = target.node().name()
                    .unwrap_or(&format!("node_{}", target.node().index()))
                    .to_string();

                // Get times
                let times: Vec<f32> = reader.read_inputs()
                    .map(|i| i.collect())
                    .unwrap_or_default();

                if let Some(last) = times.last() {
                    max_time = max_time.max(*last);
                }

                // Get property and values
                let (property, keyframes) = match target.property() {
                    gltf::animation::Property::Translation => {
                        let values: Vec<[f32; 3]> = reader.read_outputs()
                            .map(|o| match o {
                                gltf::animation::util::ReadOutputs::Translations(t) =>
                                    t.collect(),
                                _ => vec![],
                            })
                            .unwrap_or_default();

                        let keyframes = Self::build_keyframes(&times, &values, sampler.interpolation());
                        (AnimatedProperty::Translation, keyframes)
                    }
                    gltf::animation::Property::Rotation => {
                        let values: Vec<[f32; 4]> = reader.read_outputs()
                            .map(|o| match o {
                                gltf::animation::util::ReadOutputs::Rotations(r) =>
                                    r.into_f32().collect(),
                                _ => vec![],
                            })
                            .unwrap_or_default();

                        let keyframes: Vec<Keyframe> = times.iter().zip(values.iter())
                            .map(|(t, v)| Keyframe {
                                time: *t,
                                value: KeyframeValue::Quat(*v),
                                in_tangent: None,
                                out_tangent: None,
                            })
                            .collect();

                        (AnimatedProperty::Rotation, keyframes)
                    }
                    gltf::animation::Property::Scale => {
                        let values: Vec<[f32; 3]> = reader.read_outputs()
                            .map(|o| match o {
                                gltf::animation::util::ReadOutputs::Scales(s) =>
                                    s.collect(),
                                _ => vec![],
                            })
                            .unwrap_or_default();

                        let keyframes = Self::build_keyframes(&times, &values, sampler.interpolation());
                        (AnimatedProperty::Scale, keyframes)
                    }
                    gltf::animation::Property::MorphTargetWeights => {
                        // Morph targets - more complex
                        let values: Vec<f32> = reader.read_outputs()
                            .map(|o| match o {
                                gltf::animation::util::ReadOutputs::MorphTargetWeights(w) =>
                                    w.collect(),
                                _ => vec![],
                            })
                            .unwrap_or_default();

                        // Group by keyframe
                        let morph_count = if times.is_empty() { 0 } else { values.len() / times.len() };
                        let keyframes: Vec<Keyframe> = times.iter().enumerate()
                            .map(|(i, t)| {
                                let start = i * morph_count;
                                let end = start + morph_count;
                                Keyframe {
                                    time: *t,
                                    value: KeyframeValue::Weights(values[start..end].to_vec()),
                                    in_tangent: None,
                                    out_tangent: None,
                                }
                            })
                            .collect();

                        (AnimatedProperty::Weights, keyframes)
                    }
                };

                let interpolation = match sampler.interpolation() {
                    GltfInterpolation::Step => Interpolation::Step,
                    GltfInterpolation::Linear => Interpolation::Linear,
                    GltfInterpolation::CubicSpline => Interpolation::CubicSpline,
                };

                let track_key = format!("{}/{:?}", node_name, property);
                tracks.insert(track_key, AnimationTrack {
                    target: node_name.clone(),
                    property,
                    keyframes,
                    interpolation,
                });
            }

            clips.push(AnimationClip {
                id: AssetId::new(),
                path: String::new(),
                name: animation.name().unwrap_or("unnamed").to_string(),
                duration: max_time,
                tracks,
                root_motion: false,
                events: Vec::new(),
            });
        }

        clips
    }

    fn build_keyframes(
        times: &[f32],
        values: &[[f32; 3]],
        interpolation: GltfInterpolation,
    ) -> Vec<Keyframe> {
        if interpolation == GltfInterpolation::CubicSpline {
            // Cubic spline has 3 values per keyframe: in_tangent, value, out_tangent
            times.iter().enumerate()
                .map(|(i, t)| {
                    let base = i * 3;
                    Keyframe {
                        time: *t,
                        value: KeyframeValue::Vec3(values[base + 1]),
                        in_tangent: Some(KeyframeValue::Vec3(values[base])),
                        out_tangent: Some(KeyframeValue::Vec3(values[base + 2])),
                    }
                })
                .collect()
        } else {
            times.iter().zip(values.iter())
                .map(|(t, v)| Keyframe {
                    time: *t,
                    value: KeyframeValue::Vec3(*v),
                    in_tangent: None,
                    out_tangent: None,
                })
                .collect()
        }
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_asset/src/animation.rs` | CREATE | Animation clip asset |
| `void_ecs/src/components/animation.rs` | CREATE | Animation player component |
| `void_ecs/src/systems/animation_system.rs` | CREATE | Animation update system |
| `void_asset_server/src/loaders/gltf_animation.rs` | CREATE | glTF animation loader |
| `void_ecs/src/lib.rs` | MODIFY | Export animation modules |
| `void_engine/src/lib.rs` | MODIFY | Register animation system |
| `void_editor/src/panels/inspector.rs` | MODIFY | Animation player UI |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_linear_interpolation() {
    let track = AnimationTrack {
        keyframes: vec![
            Keyframe { time: 0.0, value: KeyframeValue::Vec3([0.0, 0.0, 0.0]), .. },
            Keyframe { time: 1.0, value: KeyframeValue::Vec3([1.0, 1.0, 1.0]), .. },
        ],
        interpolation: Interpolation::Linear,
        ..
    };

    let value = track.sample(0.5);
    if let KeyframeValue::Vec3(v) = value {
        assert!((v[0] - 0.5).abs() < 0.01);
    }
}

#[test]
fn test_quaternion_slerp() {
    let a = [0.0, 0.0, 0.0, 1.0];  // Identity
    let b = [0.0, 0.707, 0.0, 0.707];  // 90 deg Y

    let mid = slerp(a, b, 0.5);

    // Should be ~45 degrees
    assert!((mid[1] - 0.383).abs() < 0.01);
}

#[test]
fn test_loop_mode() {
    let clip = AnimationClip { duration: 1.0, .. };
    let mut state = AnimationState { loop_mode: LoopMode::Loop, time: 0.0, .. };

    // Advance past duration
    state.time = 2.5;

    // Should wrap
    let wrapped = state.time.rem_euclid(clip.duration);
    assert!((wrapped - 0.5).abs() < 0.01);
}
```

## Hot-Swap Support

### Serialization

All animation components derive Serde traits for state persistence during hot-reload:

```rust
use serde::{Serialize, Deserialize};

/// Animation clip asset - serializable for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationClip {
    /// Unique asset identifier
    #[serde(skip)]
    pub id: AssetId,

    /// Asset path (used to re-resolve id after reload)
    pub path: String,

    /// Clip name
    pub name: String,

    /// Duration in seconds
    pub duration: f32,

    /// Animation tracks (target path -> track)
    pub tracks: HashMap<String, AnimationTrack>,

    /// Root motion enabled
    pub root_motion: bool,

    /// Animation events
    pub events: Vec<AnimationEvent>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationTrack {
    pub target: String,
    pub property: AnimatedProperty,
    pub keyframes: Vec<Keyframe>,
    pub interpolation: Interpolation,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum AnimatedProperty {
    Translation,
    Rotation,
    Scale,
    Weights,
    Custom(u32),
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Keyframe {
    pub time: f32,
    pub value: KeyframeValue,
    pub in_tangent: Option<KeyframeValue>,
    pub out_tangent: Option<KeyframeValue>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum KeyframeValue {
    Float(f32),
    Vec3([f32; 3]),
    Quat([f32; 4]),
    Weights(Vec<f32>),
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum Interpolation {
    Step,
    #[default]
    Linear,
    CubicSpline,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationEvent {
    pub time: f32,
    pub name: String,
    pub data: Option<String>,
}

/// Animation player component - preserves playback state during hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationPlayer {
    /// Currently playing animations (slot -> state)
    pub animations: HashMap<u32, AnimationState>,

    /// Default slot for simple playback
    pub active_slot: u32,

    /// Playback speed multiplier
    pub speed: f32,

    /// Paused state
    pub paused: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationState {
    /// Animation clip asset path
    pub clip: String,

    /// Cached clip ID (re-resolved after reload)
    #[serde(skip)]
    pub clip_id: Option<AssetId>,

    /// Current playback time - PRESERVED during hot-reload
    pub time: f32,

    /// Playback speed for this animation
    pub speed: f32,

    /// Loop mode
    pub loop_mode: LoopMode,

    /// Blend weight (0-1)
    pub weight: f32,

    /// Is playing
    pub playing: bool,

    /// Play direction
    pub reversed: bool,

    /// Blend target (for transitions)
    pub blend_to: Option<Box<AnimationState>>,
    pub blend_duration: f32,
    pub blend_time: f32,
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum LoopMode {
    #[default]
    Once,
    Loop,
    PingPong,
    ClampForever,
}
```

### HotReloadable Implementation

```rust
use void_core::hot_reload::{HotReloadable, HotReloadContext};

impl HotReloadable for AnimationClip {
    fn type_name() -> &'static str {
        "AnimationClip"
    }

    fn version() -> u32 {
        1
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(data: &[u8]) -> Result<Self, HotReloadError> {
        bincode::deserialize(data).map_err(|e| HotReloadError::Deserialization(e.to_string()))
    }

    fn on_reload(&mut self, ctx: &HotReloadContext) {
        // Re-resolve asset ID from path
        if let Some(id) = ctx.asset_server().resolve_id(&self.path) {
            self.id = id;
        }

        // Notify animation system of clip change
        ctx.notify_system::<AnimationSystem>("clip_reloaded", &self.path);
    }
}

impl HotReloadable for AnimationPlayer {
    fn type_name() -> &'static str {
        "AnimationPlayer"
    }

    fn version() -> u32 {
        1
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(data: &[u8]) -> Result<Self, HotReloadError> {
        bincode::deserialize(data).map_err(|e| HotReloadError::Deserialization(e.to_string()))
    }

    fn on_reload(&mut self, ctx: &HotReloadContext) {
        // Re-resolve clip IDs for all animation states
        for state in self.animations.values_mut() {
            state.clip_id = ctx.asset_server().resolve_id(&state.clip);

            // Also resolve blend_to if present
            if let Some(ref mut blend_to) = state.blend_to {
                blend_to.clip_id = ctx.asset_server().resolve_id(&blend_to.clip);
            }
        }

        log::debug!(
            "AnimationPlayer reloaded: {} active animations, time preserved",
            self.animations.len()
        );
    }
}
```

### Asset Dependencies

AnimationClip tracks its source file for hot-reload:

```rust
use void_asset::AssetDependent;

impl AssetDependent for AnimationClip {
    fn get_dependencies(&self) -> Vec<AssetPath> {
        vec![AssetPath::new(&self.path)]
    }

    fn on_dependency_changed(&mut self, path: &AssetPath, ctx: &AssetContext) {
        if path.as_str() == self.path {
            // Reload clip data from file
            if let Ok(new_clip) = ctx.asset_server().load::<AnimationClip>(&self.path) {
                // Preserve ID but update all other data
                let old_id = self.id;
                *self = new_clip;
                self.id = old_id;

                log::info!("Animation clip '{}' hot-reloaded", self.name);
            }
        }
    }
}

impl AssetDependent for AnimationPlayer {
    fn get_dependencies(&self) -> Vec<AssetPath> {
        // Depend on all referenced clips
        self.animations.values()
            .map(|state| AssetPath::new(&state.clip))
            .chain(
                self.animations.values()
                    .filter_map(|state| state.blend_to.as_ref())
                    .map(|blend| AssetPath::new(&blend.clip))
            )
            .collect()
    }

    fn on_dependency_changed(&mut self, path: &AssetPath, ctx: &AssetContext) {
        // Re-resolve affected clip IDs
        for state in self.animations.values_mut() {
            if state.clip == path.as_str() {
                state.clip_id = ctx.asset_server().resolve_id(&state.clip);
            }
            if let Some(ref mut blend_to) = state.blend_to {
                if blend_to.clip == path.as_str() {
                    blend_to.clip_id = ctx.asset_server().resolve_id(&blend_to.clip);
                }
            }
        }
    }
}
```

### Frame-Boundary Updates

Animation state updates respect frame boundaries:

```rust
// crates/void_ecs/src/systems/animation_system.rs

pub struct AnimationSystemState {
    /// Pending clip updates from hot-reload
    pending_clip_updates: VecDeque<ClipUpdate>,

    /// Clip cache (rebuilt on hot-reload)
    clip_cache: HashMap<AssetId, AnimationClip>,

    /// Dirty players that need clip re-resolution
    dirty_players: HashSet<Entity>,
}

impl AnimationSystemState {
    /// Queue a clip update (called during hot-reload)
    pub fn queue_clip_update(&mut self, clip_path: String, new_clip: AnimationClip) {
        self.pending_clip_updates.push_back(ClipUpdate {
            path: clip_path,
            clip: new_clip,
        });
    }

    /// Mark a player as needing clip re-resolution
    pub fn mark_player_dirty(&mut self, entity: Entity) {
        self.dirty_players.insert(entity);
    }

    /// Apply pending updates at frame boundary (before animation update)
    pub fn apply_frame_updates(&mut self, world: &mut World) {
        // Apply clip updates
        for update in self.pending_clip_updates.drain(..) {
            self.clip_cache.insert(update.clip.id, update.clip);
            log::debug!("Applied hot-reload for clip: {}", update.path);
        }

        // Re-resolve dirty player clip IDs
        for entity in self.dirty_players.drain() {
            if let Some(player) = world.get_mut::<AnimationPlayer>(entity) {
                for state in player.animations.values_mut() {
                    // Find clip by path
                    state.clip_id = self.clip_cache.iter()
                        .find(|(_, clip)| clip.path == state.clip)
                        .map(|(id, _)| *id);
                }
            }
        }
    }
}

struct ClipUpdate {
    path: String,
    clip: AnimationClip,
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_animation_clip_serialization_roundtrip() {
        let mut clip = AnimationClip {
            id: AssetId::new(),
            path: "animations/walk.gltf#walk".to_string(),
            name: "walk".to_string(),
            duration: 1.5,
            tracks: HashMap::new(),
            root_motion: false,
            events: vec![
                AnimationEvent {
                    time: 0.5,
                    name: "footstep".to_string(),
                    data: Some("left".to_string()),
                },
            ],
        };

        // Add a track
        clip.tracks.insert("spine/translation".to_string(), AnimationTrack {
            target: "spine".to_string(),
            property: AnimatedProperty::Translation,
            keyframes: vec![
                Keyframe {
                    time: 0.0,
                    value: KeyframeValue::Vec3([0.0, 0.0, 0.0]),
                    in_tangent: None,
                    out_tangent: None,
                },
                Keyframe {
                    time: 1.5,
                    value: KeyframeValue::Vec3([0.0, 1.0, 0.0]),
                    in_tangent: None,
                    out_tangent: None,
                },
            ],
            interpolation: Interpolation::Linear,
        });

        let serialized = clip.serialize_state().unwrap();
        let deserialized = AnimationClip::deserialize_state(&serialized).unwrap();

        assert_eq!(clip.name, deserialized.name);
        assert_eq!(clip.duration, deserialized.duration);
        assert_eq!(clip.tracks.len(), deserialized.tracks.len());
        assert_eq!(clip.events.len(), deserialized.events.len());
    }

    #[test]
    fn test_animation_player_state_preservation() {
        let mut player = AnimationPlayer::default();
        player.play_looped("walk");

        // Simulate time passing
        if let Some(state) = player.animations.get_mut(&0) {
            state.time = 0.75;  // Mid-animation
        }

        player.speed = 1.5;
        player.paused = false;

        let serialized = player.serialize_state().unwrap();
        let deserialized = AnimationPlayer::deserialize_state(&serialized).unwrap();

        // Verify playback state is preserved
        assert_eq!(player.speed, deserialized.speed);
        assert_eq!(player.paused, deserialized.paused);
        assert_eq!(player.animations.len(), deserialized.animations.len());

        let orig_state = player.animations.get(&0).unwrap();
        let new_state = deserialized.animations.get(&0).unwrap();

        assert_eq!(orig_state.time, new_state.time);  // Time preserved!
        assert_eq!(orig_state.clip, new_state.clip);
        assert_eq!(orig_state.playing, new_state.playing);
    }

    #[test]
    fn test_animation_blend_state_preservation() {
        let mut player = AnimationPlayer::default();
        player.play("idle");
        player.blend_to(0, "walk", 0.3);

        // Advance blend partially
        if let Some(state) = player.animations.get_mut(&0) {
            state.blend_time = 0.15;  // 50% through blend
            if let Some(ref mut blend_to) = state.blend_to {
                blend_to.time = 0.15;
            }
        }

        let serialized = player.serialize_state().unwrap();
        let deserialized = AnimationPlayer::deserialize_state(&serialized).unwrap();

        let state = deserialized.animations.get(&0).unwrap();
        assert!(state.blend_to.is_some());
        assert_eq!(state.blend_time, 0.15);

        let blend_to = state.blend_to.as_ref().unwrap();
        assert_eq!(blend_to.clip, "walk");
        assert_eq!(blend_to.time, 0.15);
    }

    #[test]
    fn test_animation_player_dependency_tracking() {
        let mut player = AnimationPlayer::default();
        player.play_in_slot(0, "animations/idle.gltf#idle");
        player.play_in_slot(1, "animations/walk.gltf#walk");
        player.blend_to(0, "animations/run.gltf#run", 0.5);

        let deps = player.get_dependencies();

        assert_eq!(deps.len(), 3);
        assert!(deps.iter().any(|p| p.as_str() == "animations/idle.gltf#idle"));
        assert!(deps.iter().any(|p| p.as_str() == "animations/walk.gltf#walk"));
        assert!(deps.iter().any(|p| p.as_str() == "animations/run.gltf#run"));
    }

    #[test]
    fn test_clip_hot_reload_preserves_playback() {
        let mut state = AnimationSystemState::default();
        let mut world = World::new();

        // Create entity with animation player
        let entity = world.spawn();
        let mut player = AnimationPlayer::default();
        player.play_looped("test_clip");
        player.animations.get_mut(&0).unwrap().time = 0.5;
        world.insert(entity, player);

        // Queue a clip update (simulating file change)
        let new_clip = AnimationClip {
            id: AssetId::new(),
            path: "test_clip".to_string(),
            name: "test".to_string(),
            duration: 2.0,  // Duration changed
            tracks: HashMap::new(),
            root_motion: false,
            events: Vec::new(),
        };

        state.queue_clip_update("test_clip".to_string(), new_clip);
        state.mark_player_dirty(entity);

        // Apply at frame boundary
        state.apply_frame_updates(&mut world);

        // Verify playback time preserved
        let player = world.get::<AnimationPlayer>(entity).unwrap();
        assert_eq!(player.animations.get(&0).unwrap().time, 0.5);
    }
}
```

## Fault Tolerance

### Animation Clip Loading with Fallback

```rust
impl AnimationClip {
    /// Load animation clip with fallback on failure
    pub fn load_with_fallback(path: &str, asset_server: &AssetServer) -> Self {
        std::panic::catch_unwind(|| {
            asset_server.load::<AnimationClip>(path)
        })
        .unwrap_or_else(|e| {
            log::error!("Failed to load animation clip '{}': {:?}, using empty clip", path, e);
            Self::empty_clip(path)
        })
    }

    /// Empty fallback clip
    pub fn empty_clip(path: &str) -> Self {
        Self {
            id: AssetId::new(),
            path: path.to_string(),
            name: "empty_fallback".to_string(),
            duration: 0.0,
            tracks: HashMap::new(),
            root_motion: false,
            events: Vec::new(),
        }
    }
}
```

### Safe Animation Sampling

```rust
impl AnimationClip {
    /// Sample animation with panic protection
    pub fn sample_safe(&self, time: f32) -> AnimationPose {
        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.sample(time)
        }))
        .unwrap_or_else(|e| {
            log::error!(
                "Panic while sampling animation '{}' at time {}: {:?}",
                self.name, time, e
            );
            AnimationPose::default()
        })
    }
}

impl AnimationTrack {
    /// Sample track with bounds checking
    pub fn sample_safe(&self, time: f32) -> KeyframeValue {
        if self.keyframes.is_empty() {
            return KeyframeValue::Float(0.0);
        }

        // Clamp time to valid range
        let clamped_time = time.clamp(0.0, self.keyframes.last().map(|k| k.time).unwrap_or(0.0));

        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.sample(clamped_time)
        }))
        .unwrap_or_else(|_| {
            log::warn!("Track sampling failed, using first keyframe");
            self.keyframes.first()
                .map(|k| k.value.clone())
                .unwrap_or(KeyframeValue::Float(0.0))
        })
    }
}
```

### Animation System Resilience

```rust
impl AnimationSystem {
    /// Update with fault isolation per entity
    pub fn update_safe(
        world: &mut World,
        dt: f32,
        clip_cache: &HashMap<AssetId, AnimationClip>,
    ) {
        let entities: Vec<Entity> = world.query::<&AnimationPlayer>()
            .map(|(e, _)| e)
            .collect();

        for entity in entities {
            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                Self::update_entity(world, entity, dt, clip_cache)
            }));

            if let Err(e) = result {
                log::error!(
                    "Animation update panic for entity {:?}: {:?}, skipping this frame",
                    entity, e
                );

                // Optionally pause the problematic player
                if let Some(mut player) = world.get_mut::<AnimationPlayer>(entity) {
                    player.paused = true;
                    log::warn!("Auto-paused animation player for entity {:?}", entity);
                }
            }
        }
    }

    /// Update single entity (isolated for fault tolerance)
    fn update_entity(
        world: &mut World,
        entity: Entity,
        dt: f32,
        clip_cache: &HashMap<AssetId, AnimationClip>,
    ) {
        let Some(player) = world.get::<AnimationPlayer>(entity).cloned() else {
            return;
        };

        if player.paused {
            return;
        }

        // Process each animation slot
        for (slot, state) in &player.animations {
            if !state.playing {
                continue;
            }

            // Get clip safely
            let Some(clip) = state.clip_id.and_then(|id| clip_cache.get(&id)) else {
                log::debug!("Clip not found for slot {} on entity {:?}", slot, entity);
                continue;
            };

            // Sample with protection
            let pose = clip.sample_safe(state.time);

            // Apply pose with protection
            if let Err(e) = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                Self::apply_pose(world, entity, &pose, state.weight)
            })) {
                log::error!("Failed to apply pose for entity {:?}: {:?}", entity, e);
            }
        }

        // Update player state
        Self::advance_player_time(world, entity, dt, clip_cache);
    }
}
```

### Quaternion Math Safety

```rust
/// Safe slerp with NaN/infinity protection
fn slerp_safe(a: [f32; 4], b: [f32; 4], t: f32) -> [f32; 4] {
    // Validate inputs
    let a_valid = a.iter().all(|v| v.is_finite());
    let b_valid = b.iter().all(|v| v.is_finite());
    let t_valid = t.is_finite();

    if !a_valid || !b_valid || !t_valid {
        log::warn!("Invalid quaternion slerp inputs, returning identity");
        return [0.0, 0.0, 0.0, 1.0];
    }

    let result = slerp(a, b, t.clamp(0.0, 1.0));

    // Validate output
    if result.iter().any(|v| !v.is_finite()) {
        log::warn!("Quaternion slerp produced invalid result, returning identity");
        return [0.0, 0.0, 0.0, 1.0];
    }

    normalize_quat(result)
}
```

## Acceptance Criteria

### Functional

- [ ] Animation clips load from glTF files
- [ ] Translation, rotation, scale tracks work
- [ ] Linear and cubic interpolation work
- [ ] Loop modes work (once, loop, ping-pong)
- [ ] Animation speed scaling works
- [ ] Multiple animations can play simultaneously
- [ ] Animation blending works
- [ ] Seek/pause/resume work
- [ ] Editor shows animation controls
- [ ] Performance: 100 animated entities at 60 FPS

### Hot-Swap Compliance

- [ ] AnimationClip derives `Serialize` and `Deserialize`
- [ ] AnimationPlayer derives `Serialize` and `Deserialize`
- [ ] AnimationState derives `Serialize` and `Deserialize`
- [ ] AnimationClip implements `HotReloadable` trait
- [ ] AnimationPlayer implements `HotReloadable` trait
- [ ] Playback time (`state.time`) preserved across hot-reload
- [ ] Blend state preserved across hot-reload
- [ ] Clip IDs re-resolved after hot-reload
- [ ] AnimationClip implements `AssetDependent` for file tracking
- [ ] AnimationPlayer implements `AssetDependent` for clip tracking
- [ ] Frame-boundary update queue processes clip changes safely
- [ ] Serialization roundtrip preserves all animation state
- [ ] Hot-swap tests pass for all animation components

## Dependencies

- **Phase 1: Scene Graph** - Animations target hierarchy
- **Phase 3: Mesh Import** - glTF contains animations

## Dependents

- **Phase 9: Animation Blending** - Building on this system

---

**Estimated Complexity**: High
**Primary Crates**: void_asset, void_ecs
**Reviewer Notes**: Quaternion math is tricky - verify slerp implementation
