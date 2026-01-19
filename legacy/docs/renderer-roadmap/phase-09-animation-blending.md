# Phase 9: Animation Blending

## Status: Not Started

## User Story

> As a scene author, I want to blend between animations smoothly.

## Requirements Checklist

- [ ] Support blending between two or more animation clips
- [ ] Configurable blend durations
- [ ] Support additive animation layers
- [ ] Deterministic blending behavior

## Current State Analysis

Phase 8 implements basic animation playback with simple crossfade blending. This phase extends it with:
- Blend trees
- Additive layers
- Masked blending
- 1D/2D blend spaces

## Implementation Specification

### 1. Animation Layer System

```rust
// crates/void_ecs/src/components/animation_layers.rs (NEW FILE)

/// Multi-layer animation blending
#[derive(Clone, Debug)]
pub struct AnimationLayers {
    /// Ordered layers (index 0 = base layer)
    pub layers: Vec<AnimationLayer>,

    /// Final blended pose (computed each frame)
    pub final_pose: AnimationPose,
}

#[derive(Clone, Debug)]
pub struct AnimationLayer {
    /// Layer name
    pub name: String,

    /// Layer weight (0-1)
    pub weight: f32,

    /// Blend mode
    pub blend_mode: LayerBlendMode,

    /// Bone mask (None = all bones)
    pub mask: Option<BoneMask>,

    /// Active animation state
    pub state: LayerState,

    /// Is layer enabled
    pub enabled: bool,
}

#[derive(Clone, Copy, Debug, Default)]
pub enum LayerBlendMode {
    /// Replace lower layers
    #[default]
    Override,

    /// Add to lower layers
    Additive,
}

/// Mask for selective bone blending
#[derive(Clone, Debug)]
pub struct BoneMask {
    /// Bone names included in mask
    pub bones: HashSet<String>,

    /// Include children of masked bones
    pub include_children: bool,

    /// Per-bone weights (optional)
    pub weights: Option<HashMap<String, f32>>,
}

#[derive(Clone, Debug)]
pub enum LayerState {
    /// Single animation
    Single(AnimationState),

    /// Blend tree
    BlendTree(BlendTree),

    /// State machine
    StateMachine(AnimationStateMachine),
}

impl Default for AnimationLayers {
    fn default() -> Self {
        Self {
            layers: vec![AnimationLayer {
                name: "Base".to_string(),
                weight: 1.0,
                blend_mode: LayerBlendMode::Override,
                mask: None,
                state: LayerState::Single(AnimationState::default()),
                enabled: true,
            }],
            final_pose: AnimationPose::default(),
        }
    }
}

impl AnimationLayers {
    /// Add a new layer
    pub fn add_layer(&mut self, name: impl Into<String>, blend_mode: LayerBlendMode) -> usize {
        let index = self.layers.len();
        self.layers.push(AnimationLayer {
            name: name.into(),
            weight: 1.0,
            blend_mode,
            mask: None,
            state: LayerState::Single(AnimationState::default()),
            enabled: true,
        });
        index
    }

    /// Set layer weight
    pub fn set_weight(&mut self, layer: usize, weight: f32) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.weight = weight.clamp(0.0, 1.0);
        }
    }

    /// Set bone mask for layer
    pub fn set_mask(&mut self, layer: usize, mask: BoneMask) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.mask = Some(mask);
        }
    }

    /// Play animation on layer
    pub fn play_on_layer(&mut self, layer: usize, clip: impl Into<String>) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.state = LayerState::Single(AnimationState {
                clip: clip.into(),
                playing: true,
                ..Default::default()
            });
        }
    }
}
```

### 2. Blend Trees

```rust
// crates/void_ecs/src/components/blend_tree.rs (NEW FILE)

/// Blend tree for parameter-driven animation blending
#[derive(Clone, Debug)]
pub struct BlendTree {
    /// Root node
    pub root: BlendNode,

    /// Parameter values
    pub parameters: HashMap<String, f32>,
}

#[derive(Clone, Debug)]
pub enum BlendNode {
    /// Single animation clip
    Clip {
        clip: String,
        clip_id: Option<AssetId>,
        speed: f32,
    },

    /// 1D blend between animations
    Blend1D {
        parameter: String,
        children: Vec<BlendChild1D>,
    },

    /// 2D blend using two parameters
    Blend2D {
        parameter_x: String,
        parameter_y: String,
        children: Vec<BlendChild2D>,
        blend_type: Blend2DType,
    },

    /// Direct blend with weights
    Direct {
        children: Vec<BlendChildDirect>,
    },

    /// Additive blend
    Additive {
        base: Box<BlendNode>,
        additive: Box<BlendNode>,
        weight: f32,
    },
}

#[derive(Clone, Debug)]
pub struct BlendChild1D {
    pub node: Box<BlendNode>,
    pub threshold: f32,  // Parameter value where this is 100%
}

#[derive(Clone, Debug)]
pub struct BlendChild2D {
    pub node: Box<BlendNode>,
    pub position: [f32; 2],  // Position in 2D parameter space
}

#[derive(Clone, Debug)]
pub struct BlendChildDirect {
    pub node: Box<BlendNode>,
    pub weight_parameter: String,
}

#[derive(Clone, Copy, Debug, Default)]
pub enum Blend2DType {
    #[default]
    SimpleDirectional,
    FreeformDirectional,
    FreeformCartesian,
}

impl BlendTree {
    /// Create a simple 1D blend tree
    pub fn blend_1d(parameter: impl Into<String>, clips: Vec<(String, f32)>) -> Self {
        let children = clips.into_iter()
            .map(|(clip, threshold)| BlendChild1D {
                node: Box::new(BlendNode::Clip {
                    clip,
                    clip_id: None,
                    speed: 1.0,
                }),
                threshold,
            })
            .collect();

        Self {
            root: BlendNode::Blend1D {
                parameter: parameter.into(),
                children,
            },
            parameters: HashMap::new(),
        }
    }

    /// Set parameter value
    pub fn set_parameter(&mut self, name: impl Into<String>, value: f32) {
        self.parameters.insert(name.into(), value);
    }

    /// Get parameter value
    pub fn get_parameter(&self, name: &str) -> f32 {
        self.parameters.get(name).copied().unwrap_or(0.0)
    }

    /// Evaluate blend tree at current parameter values
    pub fn evaluate(
        &self,
        clip_cache: &HashMap<AssetId, AnimationClip>,
        time: f32,
    ) -> AnimationPose {
        self.evaluate_node(&self.root, clip_cache, time)
    }

    fn evaluate_node(
        &self,
        node: &BlendNode,
        clip_cache: &HashMap<AssetId, AnimationClip>,
        time: f32,
    ) -> AnimationPose {
        match node {
            BlendNode::Clip { clip_id, .. } => {
                if let Some(id) = clip_id {
                    if let Some(clip) = clip_cache.get(id) {
                        return clip.sample(time);
                    }
                }
                AnimationPose::default()
            }

            BlendNode::Blend1D { parameter, children } => {
                let value = self.get_parameter(parameter);
                self.blend_1d_children(children, value, clip_cache, time)
            }

            BlendNode::Blend2D { parameter_x, parameter_y, children, blend_type } => {
                let x = self.get_parameter(parameter_x);
                let y = self.get_parameter(parameter_y);
                self.blend_2d_children(children, [x, y], *blend_type, clip_cache, time)
            }

            BlendNode::Direct { children } => {
                self.blend_direct_children(children, clip_cache, time)
            }

            BlendNode::Additive { base, additive, weight } => {
                let base_pose = self.evaluate_node(base, clip_cache, time);
                let add_pose = self.evaluate_node(additive, clip_cache, time);
                self.apply_additive(&base_pose, &add_pose, *weight)
            }
        }
    }

    fn blend_1d_children(
        &self,
        children: &[BlendChild1D],
        value: f32,
        clip_cache: &HashMap<AssetId, AnimationClip>,
        time: f32,
    ) -> AnimationPose {
        if children.is_empty() {
            return AnimationPose::default();
        }

        // Find two nearest children to blend
        let mut sorted: Vec<_> = children.iter().collect();
        sorted.sort_by(|a, b| a.threshold.partial_cmp(&b.threshold).unwrap());

        // Find surrounding thresholds
        let mut lower = &sorted[0];
        let mut upper = &sorted[0];

        for child in &sorted {
            if child.threshold <= value {
                lower = child;
            }
            if child.threshold >= value && upper.threshold <= value {
                upper = child;
            }
        }

        // Calculate blend factor
        let range = upper.threshold - lower.threshold;
        let t = if range.abs() < 0.0001 {
            0.0
        } else {
            ((value - lower.threshold) / range).clamp(0.0, 1.0)
        };

        // Blend poses
        let pose_a = self.evaluate_node(&lower.node, clip_cache, time);
        let pose_b = self.evaluate_node(&upper.node, clip_cache, time);

        blend_poses(&pose_a, &pose_b, t)
    }

    fn blend_2d_children(
        &self,
        children: &[BlendChild2D],
        position: [f32; 2],
        blend_type: Blend2DType,
        clip_cache: &HashMap<AssetId, AnimationClip>,
        time: f32,
    ) -> AnimationPose {
        if children.is_empty() {
            return AnimationPose::default();
        }

        // Calculate weights based on blend type
        let weights = match blend_type {
            Blend2DType::SimpleDirectional => {
                self.simple_directional_weights(children, position)
            }
            Blend2DType::FreeformDirectional | Blend2DType::FreeformCartesian => {
                self.gradient_band_weights(children, position)
            }
        };

        // Blend all poses
        let mut result = AnimationPose::default();
        let mut total_weight = 0.0;

        for (i, child) in children.iter().enumerate() {
            if weights[i] > 0.0001 {
                let pose = self.evaluate_node(&child.node, clip_cache, time);
                result = if total_weight < 0.0001 {
                    pose
                } else {
                    let t = weights[i] / (total_weight + weights[i]);
                    blend_poses(&result, &pose, t)
                };
                total_weight += weights[i];
            }
        }

        result
    }

    fn simple_directional_weights(&self, children: &[BlendChild2D], pos: [f32; 2]) -> Vec<f32> {
        // Simple inverse distance weighting
        let mut weights = vec![0.0; children.len()];
        let mut total = 0.0;

        for (i, child) in children.iter().enumerate() {
            let dx = pos[0] - child.position[0];
            let dy = pos[1] - child.position[1];
            let dist_sq = dx * dx + dy * dy;

            if dist_sq < 0.0001 {
                // Very close, use this one
                weights[i] = 1.0;
                return weights;
            }

            weights[i] = 1.0 / dist_sq;
            total += weights[i];
        }

        // Normalize
        for w in &mut weights {
            *w /= total;
        }

        weights
    }

    fn gradient_band_weights(&self, children: &[BlendChild2D], pos: [f32; 2]) -> Vec<f32> {
        // More sophisticated gradient band interpolation
        // Simplified implementation - real Unity uses proper gradient bands
        self.simple_directional_weights(children, pos)
    }

    fn blend_direct_children(
        &self,
        children: &[BlendChildDirect],
        clip_cache: &HashMap<AssetId, AnimationClip>,
        time: f32,
    ) -> AnimationPose {
        let mut result = AnimationPose::default();
        let mut total_weight = 0.0;

        for child in children {
            let weight = self.get_parameter(&child.weight_parameter);
            if weight > 0.0001 {
                let pose = self.evaluate_node(&child.node, clip_cache, time);
                result = if total_weight < 0.0001 {
                    pose
                } else {
                    let t = weight / (total_weight + weight);
                    blend_poses(&result, &pose, t)
                };
                total_weight += weight;
            }
        }

        result
    }

    fn apply_additive(
        &self,
        base: &AnimationPose,
        additive: &AnimationPose,
        weight: f32,
    ) -> AnimationPose {
        let mut result = base.clone();

        for (target, add_value) in &additive.channels {
            let base_value = result.channels.get(target);

            let new_value = match (base_value, add_value) {
                (Some(KeyframeValue::Vec3(b)), KeyframeValue::Vec3(a)) => {
                    KeyframeValue::Vec3([
                        b[0] + a[0] * weight,
                        b[1] + a[1] * weight,
                        b[2] + a[2] * weight,
                    ])
                }
                (Some(KeyframeValue::Quat(b)), KeyframeValue::Quat(a)) => {
                    // Additive quaternion: result = base * (identity.lerp(additive, weight))
                    let identity = [0.0, 0.0, 0.0, 1.0];
                    let scaled_add = slerp(identity, *a, weight);
                    KeyframeValue::Quat(multiply_quat(*b, scaled_add))
                }
                _ => continue,
            };

            result.channels.insert(target.clone(), new_value);
        }

        result
    }
}

fn blend_poses(a: &AnimationPose, b: &AnimationPose, t: f32) -> AnimationPose {
    let mut result = AnimationPose::default();

    for (target, value_a) in &a.channels {
        if let Some(value_b) = b.channels.get(target) {
            result.channels.insert(target.clone(), blend_values(value_a, value_b, t));
        } else {
            result.channels.insert(target.clone(), value_a.clone());
        }
    }

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

fn multiply_quat(a: [f32; 4], b: [f32; 4]) -> [f32; 4] {
    [
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    ]
}
```

### 3. Animation State Machine

```rust
// crates/void_ecs/src/components/animation_state_machine.rs (NEW FILE)

/// State machine for animation control
#[derive(Clone, Debug)]
pub struct AnimationStateMachine {
    /// All states
    pub states: HashMap<String, AnimState>,

    /// All transitions
    pub transitions: Vec<AnimTransition>,

    /// Current state name
    pub current_state: String,

    /// Active transition (if any)
    pub active_transition: Option<ActiveTransition>,

    /// Parameters for conditions
    pub parameters: HashMap<String, AnimParameter>,
}

#[derive(Clone, Debug)]
pub struct AnimState {
    pub name: String,
    pub motion: AnimMotion,
    pub speed: f32,
    pub loop_mode: LoopMode,
}

#[derive(Clone, Debug)]
pub enum AnimMotion {
    Clip(String),
    BlendTree(BlendTree),
}

#[derive(Clone, Debug)]
pub struct AnimTransition {
    pub from: String,  // Source state ("*" = any)
    pub to: String,    // Destination state
    pub conditions: Vec<TransitionCondition>,
    pub duration: f32,
    pub offset: f32,   // Start time in destination
    pub interruption: InterruptionSource,
    pub has_exit_time: bool,
    pub exit_time: f32,  // 0-1 normalized
}

#[derive(Clone, Debug)]
pub enum TransitionCondition {
    Bool { parameter: String, value: bool },
    Float { parameter: String, comparison: Comparison, value: f32 },
    Int { parameter: String, comparison: Comparison, value: i32 },
    Trigger { parameter: String },
}

#[derive(Clone, Copy, Debug)]
pub enum Comparison {
    Greater,
    Less,
    Equal,
    NotEqual,
}

#[derive(Clone, Copy, Debug, Default)]
pub enum InterruptionSource {
    #[default]
    None,
    CurrentState,
    NextState,
    CurrentThenNext,
    NextThenCurrent,
}

#[derive(Clone, Debug)]
pub struct ActiveTransition {
    pub transition_index: usize,
    pub elapsed: f32,
    pub from_time: f32,
}

#[derive(Clone, Debug)]
pub enum AnimParameter {
    Bool(bool),
    Float(f32),
    Int(i32),
    Trigger(bool),
}

impl AnimationStateMachine {
    pub fn new(initial_state: impl Into<String>) -> Self {
        Self {
            states: HashMap::new(),
            transitions: Vec::new(),
            current_state: initial_state.into(),
            active_transition: None,
            parameters: HashMap::new(),
        }
    }

    /// Add a state
    pub fn add_state(&mut self, name: impl Into<String>, clip: impl Into<String>) {
        let name = name.into();
        self.states.insert(name.clone(), AnimState {
            name,
            motion: AnimMotion::Clip(clip.into()),
            speed: 1.0,
            loop_mode: LoopMode::Loop,
        });
    }

    /// Add a transition
    pub fn add_transition(&mut self, from: impl Into<String>, to: impl Into<String>, duration: f32) {
        self.transitions.push(AnimTransition {
            from: from.into(),
            to: to.into(),
            conditions: Vec::new(),
            duration,
            offset: 0.0,
            interruption: InterruptionSource::None,
            has_exit_time: false,
            exit_time: 1.0,
        });
    }

    /// Add condition to last transition
    pub fn with_bool_condition(&mut self, parameter: impl Into<String>, value: bool) {
        if let Some(t) = self.transitions.last_mut() {
            t.conditions.push(TransitionCondition::Bool {
                parameter: parameter.into(),
                value,
            });
        }
    }

    /// Set parameter
    pub fn set_bool(&mut self, name: impl Into<String>, value: bool) {
        self.parameters.insert(name.into(), AnimParameter::Bool(value));
    }

    pub fn set_float(&mut self, name: impl Into<String>, value: f32) {
        self.parameters.insert(name.into(), AnimParameter::Float(value));
    }

    pub fn set_trigger(&mut self, name: impl Into<String>) {
        self.parameters.insert(name.into(), AnimParameter::Trigger(true));
    }

    /// Update state machine
    pub fn update(&mut self, dt: f32, current_time: f32, current_duration: f32) {
        // Check for transitions from current state
        if self.active_transition.is_none() {
            for (i, transition) in self.transitions.iter().enumerate() {
                if transition.from != "*" && transition.from != self.current_state {
                    continue;
                }

                // Check exit time
                if transition.has_exit_time {
                    let normalized_time = current_time / current_duration;
                    if normalized_time < transition.exit_time {
                        continue;
                    }
                }

                // Check conditions
                if self.check_conditions(&transition.conditions) {
                    self.active_transition = Some(ActiveTransition {
                        transition_index: i,
                        elapsed: 0.0,
                        from_time: current_time,
                    });
                    break;
                }
            }
        }

        // Update active transition
        if let Some(ref mut active) = self.active_transition {
            active.elapsed += dt;

            let transition = &self.transitions[active.transition_index];
            if active.elapsed >= transition.duration {
                // Transition complete
                self.current_state = transition.to.clone();
                self.active_transition = None;
            }
        }

        // Reset triggers
        for param in self.parameters.values_mut() {
            if let AnimParameter::Trigger(_) = param {
                *param = AnimParameter::Trigger(false);
            }
        }
    }

    fn check_conditions(&self, conditions: &[TransitionCondition]) -> bool {
        for condition in conditions {
            let passed = match condition {
                TransitionCondition::Bool { parameter, value } => {
                    self.parameters.get(parameter)
                        .map(|p| matches!(p, AnimParameter::Bool(v) if v == value))
                        .unwrap_or(false)
                }
                TransitionCondition::Float { parameter, comparison, value } => {
                    self.parameters.get(parameter)
                        .and_then(|p| if let AnimParameter::Float(v) = p { Some(*v) } else { None })
                        .map(|v| Self::compare(v, *comparison, *value))
                        .unwrap_or(false)
                }
                TransitionCondition::Int { parameter, comparison, value } => {
                    self.parameters.get(parameter)
                        .and_then(|p| if let AnimParameter::Int(v) = p { Some(*v) } else { None })
                        .map(|v| Self::compare(v as f32, *comparison, *value as f32))
                        .unwrap_or(false)
                }
                TransitionCondition::Trigger { parameter } => {
                    self.parameters.get(parameter)
                        .map(|p| matches!(p, AnimParameter::Trigger(true)))
                        .unwrap_or(false)
                }
            };

            if !passed {
                return false;
            }
        }

        true
    }

    fn compare(a: f32, cmp: Comparison, b: f32) -> bool {
        match cmp {
            Comparison::Greater => a > b,
            Comparison::Less => a < b,
            Comparison::Equal => (a - b).abs() < 0.0001,
            Comparison::NotEqual => (a - b).abs() >= 0.0001,
        }
    }

    /// Get blend weight for transition (0 = fully in from, 1 = fully in to)
    pub fn get_transition_weight(&self) -> f32 {
        if let Some(ref active) = self.active_transition {
            let transition = &self.transitions[active.transition_index];
            (active.elapsed / transition.duration).clamp(0.0, 1.0)
        } else {
            0.0
        }
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_ecs/src/components/animation_layers.rs` | CREATE | Layer system |
| `void_ecs/src/components/blend_tree.rs` | CREATE | Blend tree |
| `void_ecs/src/components/animation_state_machine.rs` | CREATE | State machine |
| `void_ecs/src/systems/animation_system.rs` | MODIFY | Support layers/blending |
| `void_editor/src/panels/animator.rs` | CREATE | Animator editor panel |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_1d_blend() {
    let tree = BlendTree::blend_1d("speed", vec![
        ("idle".into(), 0.0),
        ("walk".into(), 1.0),
        ("run".into(), 2.0),
    ]);

    tree.set_parameter("speed", 0.5);
    // Should blend 50% idle, 50% walk
}

#[test]
fn test_additive_layer() {
    let mut layers = AnimationLayers::default();
    let layer_idx = layers.add_layer("Additive", LayerBlendMode::Additive);
    layers.play_on_layer(layer_idx, "breathing");
    // Should add breathing on top of base animation
}

#[test]
fn test_state_machine_transition() {
    let mut sm = AnimationStateMachine::new("idle");
    sm.add_state("idle", "idle_clip");
    sm.add_state("walk", "walk_clip");
    sm.add_transition("idle", "walk", 0.2);
    sm.with_bool_condition("walking", true);

    sm.set_bool("walking", true);
    sm.update(0.0, 0.0, 1.0);

    assert!(sm.active_transition.is_some());
}
```

## Hot-Swap Support

### Serialization

All animation blending components derive Serde traits for state persistence during hot-reload:

```rust
use serde::{Serialize, Deserialize};

/// Multi-layer animation blending - serializable for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationLayers {
    /// Ordered layers (index 0 = base layer)
    pub layers: Vec<AnimationLayer>,

    /// Final blended pose (computed each frame, not serialized)
    #[serde(skip)]
    pub final_pose: AnimationPose,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationLayer {
    /// Layer name
    pub name: String,

    /// Layer weight (0-1)
    pub weight: f32,

    /// Blend mode
    pub blend_mode: LayerBlendMode,

    /// Bone mask (None = all bones)
    pub mask: Option<BoneMask>,

    /// Active animation state
    pub state: LayerState,

    /// Is layer enabled
    pub enabled: bool,
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum LayerBlendMode {
    #[default]
    Override,
    Additive,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BoneMask {
    pub bones: HashSet<String>,
    pub include_children: bool,
    pub weights: Option<HashMap<String, f32>>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum LayerState {
    Single(AnimationState),
    BlendTree(BlendTree),
    StateMachine(AnimationStateMachine),
}

/// Blend tree for parameter-driven animation blending
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlendTree {
    /// Root node
    pub root: BlendNode,

    /// Parameter values - PRESERVED during hot-reload
    pub parameters: HashMap<String, f32>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum BlendNode {
    Clip {
        clip: String,
        #[serde(skip)]
        clip_id: Option<AssetId>,
        speed: f32,
    },

    Blend1D {
        parameter: String,
        children: Vec<BlendChild1D>,
    },

    Blend2D {
        parameter_x: String,
        parameter_y: String,
        children: Vec<BlendChild2D>,
        blend_type: Blend2DType,
    },

    Direct {
        children: Vec<BlendChildDirect>,
    },

    Additive {
        base: Box<BlendNode>,
        additive: Box<BlendNode>,
        weight: f32,
    },
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlendChild1D {
    pub node: Box<BlendNode>,
    pub threshold: f32,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlendChild2D {
    pub node: Box<BlendNode>,
    pub position: [f32; 2],
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlendChildDirect {
    pub node: Box<BlendNode>,
    pub weight_parameter: String,
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum Blend2DType {
    #[default]
    SimpleDirectional,
    FreeformDirectional,
    FreeformCartesian,
}

/// State machine for animation control
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationStateMachine {
    /// All states
    pub states: HashMap<String, AnimState>,

    /// All transitions
    pub transitions: Vec<AnimTransition>,

    /// Current state name - PRESERVED during hot-reload
    pub current_state: String,

    /// Active transition (if any) - PRESERVED during hot-reload
    pub active_transition: Option<ActiveTransition>,

    /// Parameters for conditions - PRESERVED during hot-reload
    pub parameters: HashMap<String, AnimParameter>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimState {
    pub name: String,
    pub motion: AnimMotion,
    pub speed: f32,
    pub loop_mode: LoopMode,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum AnimMotion {
    Clip(String),
    BlendTree(BlendTree),
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimTransition {
    pub from: String,
    pub to: String,
    pub conditions: Vec<TransitionCondition>,
    pub duration: f32,
    pub offset: f32,
    pub interruption: InterruptionSource,
    pub has_exit_time: bool,
    pub exit_time: f32,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum TransitionCondition {
    Bool { parameter: String, value: bool },
    Float { parameter: String, comparison: Comparison, value: f32 },
    Int { parameter: String, comparison: Comparison, value: i32 },
    Trigger { parameter: String },
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum Comparison {
    Greater,
    Less,
    Equal,
    NotEqual,
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum InterruptionSource {
    #[default]
    None,
    CurrentState,
    NextState,
    CurrentThenNext,
    NextThenCurrent,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ActiveTransition {
    pub transition_index: usize,
    pub elapsed: f32,
    pub from_time: f32,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum AnimParameter {
    Bool(bool),
    Float(f32),
    Int(i32),
    Trigger(bool),
}
```

### HotReloadable Implementation

```rust
use void_core::hot_reload::{HotReloadable, HotReloadContext};

impl HotReloadable for AnimationLayers {
    fn type_name() -> &'static str {
        "AnimationLayers"
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
        // Re-resolve clip IDs in all layers
        for layer in &mut self.layers {
            layer.resolve_clip_ids(ctx.asset_server());
        }

        log::debug!(
            "AnimationLayers reloaded: {} layers, weights preserved",
            self.layers.len()
        );
    }
}

impl HotReloadable for BlendTree {
    fn type_name() -> &'static str {
        "BlendTree"
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
        // Re-resolve clip IDs in blend tree
        self.resolve_clip_ids_recursive(&mut self.root, ctx.asset_server());

        log::debug!(
            "BlendTree reloaded: {} parameters preserved",
            self.parameters.len()
        );
    }
}

impl HotReloadable for AnimationStateMachine {
    fn type_name() -> &'static str {
        "AnimationStateMachine"
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
        // Re-resolve clip IDs in all states
        for state in self.states.values_mut() {
            state.resolve_clip_ids(ctx.asset_server());
        }

        // Validate current state still exists
        if !self.states.contains_key(&self.current_state) {
            log::warn!(
                "State '{}' no longer exists after reload, resetting to first state",
                self.current_state
            );
            if let Some(first) = self.states.keys().next() {
                self.current_state = first.clone();
            }
            self.active_transition = None;
        }

        // Validate active transition
        if let Some(ref active) = self.active_transition {
            if active.transition_index >= self.transitions.len() {
                log::warn!("Active transition index invalid after reload, clearing");
                self.active_transition = None;
            }
        }

        log::debug!(
            "AnimationStateMachine reloaded: current_state='{}', {} parameters preserved",
            self.current_state,
            self.parameters.len()
        );
    }
}
```

### Asset Dependencies

Blend trees and state machines track their clip dependencies:

```rust
use void_asset::AssetDependent;

impl AssetDependent for AnimationLayers {
    fn get_dependencies(&self) -> Vec<AssetPath> {
        self.layers.iter()
            .flat_map(|layer| layer.get_clip_paths())
            .map(|path| AssetPath::new(path))
            .collect()
    }

    fn on_dependency_changed(&mut self, path: &AssetPath, ctx: &AssetContext) {
        for layer in &mut self.layers {
            layer.on_clip_changed(path.as_str(), ctx.asset_server());
        }
    }
}

impl AssetDependent for BlendTree {
    fn get_dependencies(&self) -> Vec<AssetPath> {
        let mut paths = Vec::new();
        self.collect_clip_paths(&self.root, &mut paths);
        paths.into_iter().map(|p| AssetPath::new(p)).collect()
    }

    fn on_dependency_changed(&mut self, path: &AssetPath, ctx: &AssetContext) {
        self.on_clip_changed(&mut self.root, path.as_str(), ctx.asset_server());
    }
}

impl AssetDependent for AnimationStateMachine {
    fn get_dependencies(&self) -> Vec<AssetPath> {
        self.states.values()
            .flat_map(|state| state.get_clip_paths())
            .map(|path| AssetPath::new(path))
            .collect()
    }

    fn on_dependency_changed(&mut self, path: &AssetPath, ctx: &AssetContext) {
        for state in self.states.values_mut() {
            state.on_clip_changed(path.as_str(), ctx.asset_server());
        }
    }
}

// Helper implementations
impl AnimationLayer {
    fn get_clip_paths(&self) -> Vec<String> {
        match &self.state {
            LayerState::Single(state) => vec![state.clip.clone()],
            LayerState::BlendTree(tree) => {
                let mut paths = Vec::new();
                tree.collect_clip_paths(&tree.root, &mut paths);
                paths
            }
            LayerState::StateMachine(sm) => {
                sm.states.values()
                    .flat_map(|s| s.get_clip_paths())
                    .collect()
            }
        }
    }

    fn resolve_clip_ids(&mut self, asset_server: &AssetServer) {
        match &mut self.state {
            LayerState::Single(state) => {
                state.clip_id = asset_server.resolve_id(&state.clip);
            }
            LayerState::BlendTree(tree) => {
                tree.resolve_clip_ids_recursive(&mut tree.root, asset_server);
            }
            LayerState::StateMachine(sm) => {
                for state in sm.states.values_mut() {
                    state.resolve_clip_ids(asset_server);
                }
            }
        }
    }
}

impl BlendTree {
    fn collect_clip_paths(&self, node: &BlendNode, paths: &mut Vec<String>) {
        match node {
            BlendNode::Clip { clip, .. } => {
                paths.push(clip.clone());
            }
            BlendNode::Blend1D { children, .. } => {
                for child in children {
                    self.collect_clip_paths(&child.node, paths);
                }
            }
            BlendNode::Blend2D { children, .. } => {
                for child in children {
                    self.collect_clip_paths(&child.node, paths);
                }
            }
            BlendNode::Direct { children } => {
                for child in children {
                    self.collect_clip_paths(&child.node, paths);
                }
            }
            BlendNode::Additive { base, additive, .. } => {
                self.collect_clip_paths(base, paths);
                self.collect_clip_paths(additive, paths);
            }
        }
    }

    fn resolve_clip_ids_recursive(&self, node: &mut BlendNode, asset_server: &AssetServer) {
        match node {
            BlendNode::Clip { clip, clip_id, .. } => {
                *clip_id = asset_server.resolve_id(clip);
            }
            BlendNode::Blend1D { children, .. } => {
                for child in children {
                    self.resolve_clip_ids_recursive(&mut child.node, asset_server);
                }
            }
            BlendNode::Blend2D { children, .. } => {
                for child in children {
                    self.resolve_clip_ids_recursive(&mut child.node, asset_server);
                }
            }
            BlendNode::Direct { children } => {
                for child in children {
                    self.resolve_clip_ids_recursive(&mut child.node, asset_server);
                }
            }
            BlendNode::Additive { base, additive, .. } => {
                self.resolve_clip_ids_recursive(base, asset_server);
                self.resolve_clip_ids_recursive(additive, asset_server);
            }
        }
    }
}

impl AnimState {
    fn get_clip_paths(&self) -> Vec<String> {
        match &self.motion {
            AnimMotion::Clip(clip) => vec![clip.clone()],
            AnimMotion::BlendTree(tree) => {
                let mut paths = Vec::new();
                tree.collect_clip_paths(&tree.root, &mut paths);
                paths
            }
        }
    }

    fn resolve_clip_ids(&mut self, asset_server: &AssetServer) {
        match &mut self.motion {
            AnimMotion::Clip(_) => {
                // Clip references resolved at runtime
            }
            AnimMotion::BlendTree(tree) => {
                tree.resolve_clip_ids_recursive(&mut tree.root, asset_server);
            }
        }
    }
}
```

### Frame-Boundary Updates

State machine and blend tree updates at frame boundaries:

```rust
// crates/void_ecs/src/systems/animation_blending_system.rs

pub struct AnimationBlendingSystemState {
    /// Pending layer configuration updates
    pending_layer_updates: VecDeque<LayerUpdate>,

    /// Pending state machine definition updates
    pending_state_machine_updates: VecDeque<StateMachineUpdate>,

    /// Entities needing clip re-resolution
    dirty_entities: HashSet<Entity>,
}

impl AnimationBlendingSystemState {
    /// Queue a layer configuration update
    pub fn queue_layer_update(&mut self, entity: Entity, layer_index: usize, config: AnimationLayer) {
        self.pending_layer_updates.push_back(LayerUpdate {
            entity,
            layer_index,
            config,
        });
    }

    /// Queue a state machine definition update
    pub fn queue_state_machine_update(&mut self, entity: Entity, new_sm: AnimationStateMachine) {
        self.pending_state_machine_updates.push_back(StateMachineUpdate {
            entity,
            state_machine: new_sm,
        });
    }

    /// Apply pending updates at frame boundary
    pub fn apply_frame_updates(&mut self, world: &mut World, asset_server: &AssetServer) {
        // Apply layer updates
        for update in self.pending_layer_updates.drain(..) {
            if let Some(layers) = world.get_mut::<AnimationLayers>(update.entity) {
                if update.layer_index < layers.layers.len() {
                    // Preserve runtime state from old layer
                    let old_state = &layers.layers[update.layer_index].state;
                    let mut new_config = update.config;

                    // Merge preserved state
                    match (&old_state, &mut new_config.state) {
                        (LayerState::StateMachine(old), LayerState::StateMachine(new)) => {
                            // Preserve current state and parameters
                            new.current_state = old.current_state.clone();
                            new.parameters = old.parameters.clone();
                            new.active_transition = old.active_transition.clone();
                        }
                        (LayerState::BlendTree(old), LayerState::BlendTree(new)) => {
                            // Preserve parameters
                            new.parameters = old.parameters.clone();
                        }
                        _ => {}
                    }

                    layers.layers[update.layer_index] = new_config;
                }
            }
        }

        // Apply state machine updates
        for update in self.pending_state_machine_updates.drain(..) {
            if let Some(layers) = world.get_mut::<AnimationLayers>(update.entity) {
                for layer in &mut layers.layers {
                    if let LayerState::StateMachine(ref mut sm) = layer.state {
                        // Preserve current state and parameters
                        let old_state = sm.current_state.clone();
                        let old_params = sm.parameters.clone();
                        let old_transition = sm.active_transition.clone();

                        *sm = update.state_machine.clone();

                        // Restore if valid
                        if sm.states.contains_key(&old_state) {
                            sm.current_state = old_state;
                        }
                        sm.parameters = old_params;
                        if old_transition.as_ref().map(|t| t.transition_index < sm.transitions.len()).unwrap_or(false) {
                            sm.active_transition = old_transition;
                        }
                    }
                }
            }
        }

        // Re-resolve clip IDs for dirty entities
        for entity in self.dirty_entities.drain() {
            if let Some(layers) = world.get_mut::<AnimationLayers>(entity) {
                for layer in &mut layers.layers {
                    layer.resolve_clip_ids(asset_server);
                }
            }
        }
    }
}

struct LayerUpdate {
    entity: Entity,
    layer_index: usize,
    config: AnimationLayer,
}

struct StateMachineUpdate {
    entity: Entity,
    state_machine: AnimationStateMachine,
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_animation_layers_serialization_roundtrip() {
        let mut layers = AnimationLayers::default();
        layers.add_layer("Upper Body", LayerBlendMode::Additive);
        layers.set_weight(1, 0.75);
        layers.set_mask(1, BoneMask {
            bones: ["spine", "arm_l", "arm_r"].iter().map(|s| s.to_string()).collect(),
            include_children: true,
            weights: None,
        });

        let serialized = layers.serialize_state().unwrap();
        let deserialized = AnimationLayers::deserialize_state(&serialized).unwrap();

        assert_eq!(layers.layers.len(), deserialized.layers.len());
        assert_eq!(layers.layers[1].name, deserialized.layers[1].name);
        assert_eq!(layers.layers[1].weight, deserialized.layers[1].weight);
        assert!(deserialized.layers[1].mask.is_some());
    }

    #[test]
    fn test_blend_tree_parameter_preservation() {
        let mut tree = BlendTree::blend_1d("speed", vec![
            ("idle".into(), 0.0),
            ("walk".into(), 1.0),
            ("run".into(), 2.0),
        ]);

        // Set runtime parameters
        tree.set_parameter("speed", 1.5);
        tree.set_parameter("direction", 0.3);

        let serialized = tree.serialize_state().unwrap();
        let deserialized = BlendTree::deserialize_state(&serialized).unwrap();

        assert_eq!(tree.parameters.len(), deserialized.parameters.len());
        assert_eq!(tree.get_parameter("speed"), deserialized.get_parameter("speed"));
        assert_eq!(tree.get_parameter("direction"), deserialized.get_parameter("direction"));
    }

    #[test]
    fn test_state_machine_state_preservation() {
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle_clip");
        sm.add_state("walk", "walk_clip");
        sm.add_state("run", "run_clip");

        sm.add_transition("idle", "walk", 0.2);
        sm.add_transition("walk", "run", 0.3);

        // Simulate runtime state
        sm.current_state = "walk".to_string();
        sm.set_float("speed", 1.5);
        sm.set_bool("grounded", true);

        // Start a transition
        sm.active_transition = Some(ActiveTransition {
            transition_index: 1,
            elapsed: 0.15,
            from_time: 0.5,
        });

        let serialized = sm.serialize_state().unwrap();
        let deserialized = AnimationStateMachine::deserialize_state(&serialized).unwrap();

        assert_eq!(sm.current_state, deserialized.current_state);
        assert_eq!(sm.parameters.len(), deserialized.parameters.len());
        assert!(deserialized.active_transition.is_some());

        let active = deserialized.active_transition.unwrap();
        assert_eq!(active.transition_index, 1);
        assert_eq!(active.elapsed, 0.15);
    }

    #[test]
    fn test_blend_tree_dependency_tracking() {
        let tree = BlendTree {
            root: BlendNode::Blend2D {
                parameter_x: "move_x".to_string(),
                parameter_y: "move_y".to_string(),
                children: vec![
                    BlendChild2D {
                        node: Box::new(BlendNode::Clip {
                            clip: "idle".to_string(),
                            clip_id: None,
                            speed: 1.0,
                        }),
                        position: [0.0, 0.0],
                    },
                    BlendChild2D {
                        node: Box::new(BlendNode::Clip {
                            clip: "walk_forward".to_string(),
                            clip_id: None,
                            speed: 1.0,
                        }),
                        position: [0.0, 1.0],
                    },
                    BlendChild2D {
                        node: Box::new(BlendNode::Clip {
                            clip: "walk_back".to_string(),
                            clip_id: None,
                            speed: 1.0,
                        }),
                        position: [0.0, -1.0],
                    },
                ],
                blend_type: Blend2DType::SimpleDirectional,
            },
            parameters: HashMap::new(),
        };

        let deps = tree.get_dependencies();

        assert_eq!(deps.len(), 3);
        assert!(deps.iter().any(|p| p.as_str() == "idle"));
        assert!(deps.iter().any(|p| p.as_str() == "walk_forward"));
        assert!(deps.iter().any(|p| p.as_str() == "walk_back"));
    }

    #[test]
    fn test_state_machine_reload_validation() {
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle_clip");
        sm.add_state("walk", "walk_clip");
        sm.current_state = "walk".to_string();

        // Simulate a reload that removes the "walk" state
        let mut reloaded_sm = AnimationStateMachine::new("idle");
        reloaded_sm.add_state("idle", "idle_clip");
        reloaded_sm.add_state("run", "run_clip");  // walk removed, run added

        // Preserve state from original
        reloaded_sm.current_state = sm.current_state.clone();

        // Simulate on_reload validation
        let ctx = HotReloadContext::new_test();
        reloaded_sm.on_reload(&ctx);

        // Should have reset to a valid state since "walk" no longer exists
        assert!(reloaded_sm.states.contains_key(&reloaded_sm.current_state));
    }

    #[test]
    fn test_additive_layer_serialization() {
        let mut layers = AnimationLayers::default();

        // Base layer with blend tree
        layers.layers[0].state = LayerState::BlendTree(BlendTree::blend_1d("speed", vec![
            ("idle".into(), 0.0),
            ("walk".into(), 1.0),
        ]));

        // Additive layer
        let additive_idx = layers.add_layer("Breathing", LayerBlendMode::Additive);
        layers.layers[additive_idx].state = LayerState::Single(AnimationState {
            clip: "breathing_additive".to_string(),
            clip_id: None,
            time: 0.5,
            speed: 1.0,
            loop_mode: LoopMode::Loop,
            weight: 1.0,
            playing: true,
            reversed: false,
            blend_to: None,
            blend_duration: 0.0,
            blend_time: 0.0,
        });

        let serialized = layers.serialize_state().unwrap();
        let deserialized = AnimationLayers::deserialize_state(&serialized).unwrap();

        assert_eq!(deserialized.layers.len(), 2);
        assert_eq!(deserialized.layers[1].blend_mode, LayerBlendMode::Additive);

        if let LayerState::Single(state) = &deserialized.layers[1].state {
            assert_eq!(state.time, 0.5);  // Time preserved
            assert_eq!(state.clip, "breathing_additive");
        } else {
            panic!("Expected Single state");
        }
    }

    #[test]
    fn test_frame_boundary_state_preservation() {
        let mut system_state = AnimationBlendingSystemState::default();
        let mut world = World::new();

        // Create entity with state machine
        let entity = world.spawn();
        let mut layers = AnimationLayers::default();

        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle_clip");
        sm.add_state("walk", "walk_clip");
        sm.current_state = "walk".to_string();
        sm.set_float("speed", 2.5);

        layers.layers[0].state = LayerState::StateMachine(sm);
        world.insert(entity, layers);

        // Queue a state machine update with new definition
        let mut new_sm = AnimationStateMachine::new("idle");
        new_sm.add_state("idle", "new_idle_clip");
        new_sm.add_state("walk", "new_walk_clip");
        new_sm.add_state("sprint", "sprint_clip");

        system_state.queue_state_machine_update(entity, new_sm);

        // Apply at frame boundary
        let asset_server = AssetServer::new_test();
        system_state.apply_frame_updates(&mut world, &asset_server);

        // Verify state preserved
        let layers = world.get::<AnimationLayers>(entity).unwrap();
        if let LayerState::StateMachine(sm) = &layers.layers[0].state {
            assert_eq!(sm.current_state, "walk");  // Preserved
            assert_eq!(sm.get_float("speed"), 2.5);  // Preserved
            assert!(sm.states.contains_key("sprint"));  // New state added
        } else {
            panic!("Expected StateMachine");
        }
    }
}
```

## Fault Tolerance

### Blend Tree Evaluation with Fallback

```rust
impl BlendTree {
    /// Evaluate blend tree with panic protection
    pub fn evaluate_safe(
        &self,
        clip_cache: &HashMap<AssetId, AnimationClip>,
        time: f32,
    ) -> AnimationPose {
        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.evaluate(clip_cache, time)
        }))
        .unwrap_or_else(|e| {
            log::error!("Blend tree evaluation panic: {:?}, returning identity pose", e);
            AnimationPose::default()
        })
    }

    /// Evaluate node with bounds checking
    fn evaluate_node_safe(
        &self,
        node: &BlendNode,
        clip_cache: &HashMap<AssetId, AnimationClip>,
        time: f32,
    ) -> AnimationPose {
        match node {
            BlendNode::Clip { clip_id, .. } => {
                if let Some(id) = clip_id {
                    if let Some(clip) = clip_cache.get(id) {
                        return clip.sample_safe(time);
                    }
                }
                log::debug!("Clip not found in blend tree, using identity");
                AnimationPose::default()
            }

            BlendNode::Blend1D { parameter, children } => {
                if children.is_empty() {
                    return AnimationPose::default();
                }

                let value = self.get_parameter(parameter);

                // Clamp to valid range
                let min_threshold = children.iter().map(|c| c.threshold).fold(f32::INFINITY, f32::min);
                let max_threshold = children.iter().map(|c| c.threshold).fold(f32::NEG_INFINITY, f32::max);
                let clamped_value = value.clamp(min_threshold, max_threshold);

                self.blend_1d_children(children, clamped_value, clip_cache, time)
            }

            // ... other node types with similar protection
            _ => self.evaluate_node(node, clip_cache, time),
        }
    }
}
```

### State Machine with Recovery

```rust
impl AnimationStateMachine {
    /// Update state machine with fault tolerance
    pub fn update_safe(&mut self, dt: f32, current_time: f32, current_duration: f32) {
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.update(dt, current_time, current_duration)
        }));

        if let Err(e) = result {
            log::error!("State machine update panic: {:?}, resetting state", e);

            // Reset to safe state
            self.active_transition = None;

            // Find first available state
            if let Some(first_state) = self.states.keys().next().cloned() {
                if !self.states.contains_key(&self.current_state) {
                    self.current_state = first_state;
                }
            }

            // Clear triggers to prevent repeated panics
            for param in self.parameters.values_mut() {
                if let AnimParameter::Trigger(_) = param {
                    *param = AnimParameter::Trigger(false);
                }
            }
        }
    }

    /// Validate state machine consistency
    pub fn validate(&self) -> Result<(), StateMachineError> {
        // Check current state exists
        if !self.states.contains_key(&self.current_state) {
            return Err(StateMachineError::InvalidCurrentState(self.current_state.clone()));
        }

        // Check all transitions reference valid states
        for (i, transition) in self.transitions.iter().enumerate() {
            if transition.from != "*" && !self.states.contains_key(&transition.from) {
                return Err(StateMachineError::InvalidTransitionSource(i, transition.from.clone()));
            }
            if !self.states.contains_key(&transition.to) {
                return Err(StateMachineError::InvalidTransitionTarget(i, transition.to.clone()));
            }
        }

        // Validate active transition
        if let Some(ref active) = self.active_transition {
            if active.transition_index >= self.transitions.len() {
                return Err(StateMachineError::InvalidActiveTransition(active.transition_index));
            }
        }

        Ok(())
    }
}

#[derive(Debug)]
pub enum StateMachineError {
    InvalidCurrentState(String),
    InvalidTransitionSource(usize, String),
    InvalidTransitionTarget(usize, String),
    InvalidActiveTransition(usize),
}
```

### Animation Layer Fault Isolation

```rust
impl AnimationLayers {
    /// Evaluate all layers with fault isolation
    pub fn evaluate_safe(&mut self, clip_cache: &HashMap<AssetId, AnimationClip>, time: f32) {
        let mut accumulated_pose = AnimationPose::default();

        for (i, layer) in self.layers.iter_mut().enumerate() {
            if !layer.enabled || layer.weight <= 0.0 {
                continue;
            }

            let layer_result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                layer.evaluate(clip_cache, time)
            }));

            match layer_result {
                Ok(pose) => {
                    accumulated_pose = match layer.blend_mode {
                        LayerBlendMode::Override => {
                            blend_poses(&accumulated_pose, &pose, layer.weight)
                        }
                        LayerBlendMode::Additive => {
                            apply_additive(&accumulated_pose, &pose, layer.weight)
                        }
                    };
                }
                Err(e) => {
                    log::error!("Layer {} ('{}') evaluation panic: {:?}, skipping", i, layer.name, e);

                    // Optionally disable the problematic layer
                    layer.enabled = false;
                    log::warn!("Auto-disabled layer '{}' due to evaluation error", layer.name);
                }
            }
        }

        self.final_pose = accumulated_pose;
    }
}
```

### Parameter Validation

```rust
impl BlendTree {
    /// Set parameter with validation
    pub fn set_parameter_safe(&mut self, name: impl Into<String>, value: f32) {
        let name = name.into();

        // Validate value
        if !value.is_finite() {
            log::warn!("Attempted to set parameter '{}' to non-finite value {}, ignoring", name, value);
            return;
        }

        self.parameters.insert(name, value);
    }
}

impl AnimationStateMachine {
    /// Set float parameter with validation
    pub fn set_float_safe(&mut self, name: impl Into<String>, value: f32) {
        let name = name.into();

        if !value.is_finite() {
            log::warn!("Attempted to set parameter '{}' to non-finite value {}, ignoring", name, value);
            return;
        }

        self.parameters.insert(name, AnimParameter::Float(value));
    }
}
```

## Acceptance Criteria

### Functional

- [ ] Animation layers work with override mode
- [ ] Additive layers add motion correctly
- [ ] Bone masks work for partial body animation
- [ ] 1D blend trees work with parameters
- [ ] 2D blend trees work with two parameters
- [ ] State machines transition correctly
- [ ] Transition conditions work (bool, float, trigger)
- [ ] Exit time conditions work
- [ ] Blending is deterministic (same input = same output)
- [ ] Editor shows blend tree visualization

### Hot-Swap Compliance

- [ ] AnimationLayers derives `Serialize` and `Deserialize`
- [ ] BlendTree derives `Serialize` and `Deserialize`
- [ ] AnimationStateMachine derives `Serialize` and `Deserialize`
- [ ] All enum variants derive `Serialize` and `Deserialize`
- [ ] AnimationLayers implements `HotReloadable` trait
- [ ] BlendTree implements `HotReloadable` trait
- [ ] AnimationStateMachine implements `HotReloadable` trait
- [ ] Blend tree parameters preserved across hot-reload
- [ ] State machine current_state preserved across hot-reload
- [ ] State machine parameters preserved across hot-reload
- [ ] Active transition state preserved across hot-reload
- [ ] Clip IDs re-resolved recursively in blend trees
- [ ] State machine validates state existence after reload
- [ ] Frame-boundary updates preserve runtime state
- [ ] Serialization roundtrip preserves all blending state
- [ ] Hot-swap tests pass for all blending components

## Dependencies

- **Phase 8: Keyframe Animation** - Base animation system

## Dependents

- None (terminal feature)

---

**Estimated Complexity**: High
**Primary Crates**: void_ecs
**Reviewer Notes**: Ensure quaternion blending is correct for additive layers
