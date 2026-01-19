//! Animation Blending System
//!
//! Multi-layer animation blending with blend trees and state machines.
//!
//! # Features
//!
//! - Animation layers with override/additive blending
//! - Bone masks for partial body animation
//! - 1D and 2D blend trees with parameter control
//! - Animation state machines with conditions
//! - Hot-reload serialization support
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::animation_blending::*;
//!
//! // Create layered animation
//! let mut layers = AnimationLayers::default();
//!
//! // Add upper body additive layer
//! let upper = layers.add_layer("UpperBody", LayerBlendMode::Additive);
//! layers.set_mask(upper, BoneMask::new(&["spine", "arm_l", "arm_r"]));
//! layers.play_on_layer(upper, "wave");
//!
//! // Create a 1D blend tree for locomotion
//! let tree = BlendTree::blend_1d("speed", vec![
//!     ("idle", 0.0),
//!     ("walk", 1.0),
//!     ("run", 2.0),
//! ]);
//! ```

use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::collections::BTreeSet;
use alloc::string::String;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use crate::animation::{
    AnimationClip, AnimationPose, AnimationState, KeyframeValue, LoopMode, lerp_value, slerp,
};

// ============================================================================
// Bone Mask
// ============================================================================

/// Mask for selective bone blending
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct BoneMask {
    /// Bone names included in mask
    pub bones: BTreeSet<String>,
    /// Include children of masked bones
    pub include_children: bool,
    /// Per-bone weights (optional)
    pub weights: Option<BTreeMap<String, f32>>,
}

impl BoneMask {
    /// Create a new bone mask
    pub fn new(bones: &[&str]) -> Self {
        Self {
            bones: bones.iter().map(|s| (*s).to_string()).collect(),
            include_children: true,
            weights: None,
        }
    }

    /// Create mask with specific weights
    pub fn with_weights(bones: &[(&str, f32)]) -> Self {
        let bone_set: BTreeSet<String> = bones.iter().map(|(s, _)| (*s).to_string()).collect();
        let weights: BTreeMap<String, f32> = bones
            .iter()
            .map(|(s, w)| ((*s).to_string(), *w))
            .collect();

        Self {
            bones: bone_set,
            include_children: true,
            weights: Some(weights),
        }
    }

    /// Check if bone is in mask
    pub fn contains(&self, bone: &str) -> bool {
        self.bones.contains(bone)
    }

    /// Get weight for bone
    pub fn weight(&self, bone: &str) -> f32 {
        if !self.contains(bone) {
            return 0.0;
        }
        self.weights
            .as_ref()
            .and_then(|w| w.get(bone))
            .copied()
            .unwrap_or(1.0)
    }

    /// Add bones to mask
    pub fn add_bones(&mut self, bones: &[&str]) {
        for bone in bones {
            self.bones.insert((*bone).to_string());
        }
    }
}

// ============================================================================
// Layer Blend Mode
// ============================================================================

/// Blend mode for animation layers
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum LayerBlendMode {
    /// Replace lower layers
    #[default]
    Override,
    /// Add to lower layers
    Additive,
}

// ============================================================================
// Layer State
// ============================================================================

/// State of an animation layer
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum LayerState {
    /// Single animation
    Single(AnimationState),
    /// Blend tree
    BlendTree(BlendTree),
    /// State machine
    StateMachine(AnimationStateMachine),
}

impl Default for LayerState {
    fn default() -> Self {
        Self::Single(AnimationState::default())
    }
}

// ============================================================================
// Animation Layer
// ============================================================================

/// Single animation layer
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

impl Default for AnimationLayer {
    fn default() -> Self {
        Self {
            name: "Base".to_string(),
            weight: 1.0,
            blend_mode: LayerBlendMode::Override,
            mask: None,
            state: LayerState::Single(AnimationState::default()),
            enabled: true,
        }
    }
}

impl AnimationLayer {
    /// Create a new layer
    pub fn new(name: impl Into<String>, blend_mode: LayerBlendMode) -> Self {
        Self {
            name: name.into(),
            blend_mode,
            ..Default::default()
        }
    }

    /// Evaluate the layer to get a pose
    pub fn evaluate(&self, clips: &BTreeMap<String, AnimationClip>) -> AnimationPose {
        match &self.state {
            LayerState::Single(state) => {
                if let Some(clip) = clips.get(&state.clip) {
                    clip.sample(state.time)
                } else {
                    AnimationPose::default()
                }
            }
            LayerState::BlendTree(tree) => tree.evaluate(clips),
            LayerState::StateMachine(sm) => sm.evaluate(clips),
        }
    }
}

// ============================================================================
// Animation Layers Component
// ============================================================================

/// Multi-layer animation blending component
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationLayers {
    /// Ordered layers (index 0 = base layer)
    pub layers: Vec<AnimationLayer>,
    /// Final blended pose (computed each frame)
    #[serde(skip)]
    pub final_pose: AnimationPose,
}

impl Default for AnimationLayers {
    fn default() -> Self {
        Self {
            layers: vec![AnimationLayer::default()],
            final_pose: AnimationPose::default(),
        }
    }
}

impl AnimationLayers {
    /// Create new animation layers with base layer
    pub fn new() -> Self {
        Self::default()
    }

    /// Add a new layer
    pub fn add_layer(&mut self, name: impl Into<String>, blend_mode: LayerBlendMode) -> usize {
        let index = self.layers.len();
        self.layers.push(AnimationLayer::new(name, blend_mode));
        index
    }

    /// Get layer by index
    pub fn get(&self, index: usize) -> Option<&AnimationLayer> {
        self.layers.get(index)
    }

    /// Get mutable layer by index
    pub fn get_mut(&mut self, index: usize) -> Option<&mut AnimationLayer> {
        self.layers.get_mut(index)
    }

    /// Set layer weight
    pub fn set_weight(&mut self, layer: usize, weight: f32) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.weight = weight.clamp(0.0, 1.0);
        }
    }

    /// Set layer enabled state
    pub fn set_enabled(&mut self, layer: usize, enabled: bool) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.enabled = enabled;
        }
    }

    /// Set bone mask for layer
    pub fn set_mask(&mut self, layer: usize, mask: BoneMask) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.mask = Some(mask);
        }
    }

    /// Clear bone mask for layer
    pub fn clear_mask(&mut self, layer: usize) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.mask = None;
        }
    }

    /// Play animation on layer
    pub fn play_on_layer(&mut self, layer: usize, clip: impl Into<String>) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.state = LayerState::Single(AnimationState::new(clip));
        }
    }

    /// Play looped animation on layer
    pub fn play_looped_on_layer(&mut self, layer: usize, clip: impl Into<String>) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.state = LayerState::Single(AnimationState::new(clip).with_loop_mode(LoopMode::Loop));
        }
    }

    /// Set blend tree on layer
    pub fn set_blend_tree(&mut self, layer: usize, tree: BlendTree) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.state = LayerState::BlendTree(tree);
        }
    }

    /// Set state machine on layer
    pub fn set_state_machine(&mut self, layer: usize, sm: AnimationStateMachine) {
        if let Some(l) = self.layers.get_mut(layer) {
            l.state = LayerState::StateMachine(sm);
        }
    }

    /// Evaluate all layers and compute final pose
    pub fn evaluate(&mut self, clips: &BTreeMap<String, AnimationClip>) {
        let mut result = AnimationPose::default();

        for layer in &self.layers {
            if !layer.enabled || layer.weight <= 0.0 {
                continue;
            }

            let pose = layer.evaluate(clips);

            // Apply bone mask if present
            let pose = if let Some(ref mask) = layer.mask {
                apply_bone_mask(&pose, mask)
            } else {
                pose
            };

            // Blend with accumulated result
            result = match layer.blend_mode {
                LayerBlendMode::Override => blend_poses(&result, &pose, layer.weight),
                LayerBlendMode::Additive => apply_additive(&result, &pose, layer.weight),
            };
        }

        self.final_pose = result;
    }

    /// Get layer count
    pub fn len(&self) -> usize {
        self.layers.len()
    }

    /// Check if empty (no layers)
    pub fn is_empty(&self) -> bool {
        self.layers.is_empty()
    }

    /// Find layer by name
    pub fn find_layer(&self, name: &str) -> Option<usize> {
        self.layers.iter().position(|l| l.name == name)
    }
}

// ============================================================================
// Blend Tree
// ============================================================================

/// Blend tree for parameter-driven animation blending
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlendTree {
    /// Root node
    pub root: BlendNode,
    /// Parameter values
    pub parameters: BTreeMap<String, f32>,
    /// Current playback time
    pub time: f32,
    /// Playback speed
    pub speed: f32,
    /// Is playing
    pub playing: bool,
}

impl Default for BlendTree {
    fn default() -> Self {
        Self {
            root: BlendNode::Clip {
                clip: String::new(),
                speed: 1.0,
            },
            parameters: BTreeMap::new(),
            time: 0.0,
            speed: 1.0,
            playing: true,
        }
    }
}

impl BlendTree {
    /// Create a simple 1D blend tree
    pub fn blend_1d(parameter: impl Into<String>, clips: Vec<(&str, f32)>) -> Self {
        let children = clips
            .into_iter()
            .map(|(clip, threshold)| BlendChild1D {
                node: Box::new(BlendNode::Clip {
                    clip: clip.to_string(),
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
            parameters: BTreeMap::new(),
            time: 0.0,
            speed: 1.0,
            playing: true,
        }
    }

    /// Create a 2D blend tree
    pub fn blend_2d(
        param_x: impl Into<String>,
        param_y: impl Into<String>,
        clips: Vec<(&str, [f32; 2])>,
    ) -> Self {
        let children = clips
            .into_iter()
            .map(|(clip, position)| BlendChild2D {
                node: Box::new(BlendNode::Clip {
                    clip: clip.to_string(),
                    speed: 1.0,
                }),
                position,
            })
            .collect();

        Self {
            root: BlendNode::Blend2D {
                parameter_x: param_x.into(),
                parameter_y: param_y.into(),
                children,
                blend_type: Blend2DType::SimpleDirectional,
            },
            parameters: BTreeMap::new(),
            time: 0.0,
            speed: 1.0,
            playing: true,
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

    /// Update blend tree time
    pub fn update(&mut self, dt: f32) {
        if self.playing {
            self.time += dt * self.speed;
        }
    }

    /// Evaluate blend tree at current parameter values
    pub fn evaluate(&self, clips: &BTreeMap<String, AnimationClip>) -> AnimationPose {
        self.evaluate_node(&self.root, clips)
    }

    fn evaluate_node(
        &self,
        node: &BlendNode,
        clips: &BTreeMap<String, AnimationClip>,
    ) -> AnimationPose {
        match node {
            BlendNode::Clip { clip, speed } => {
                if let Some(anim_clip) = clips.get(clip) {
                    let time = self.time * speed;
                    anim_clip.sample(time)
                } else {
                    AnimationPose::default()
                }
            }

            BlendNode::Blend1D {
                parameter,
                children,
            } => {
                let value = self.get_parameter(parameter);
                self.blend_1d_children(children, value, clips)
            }

            BlendNode::Blend2D {
                parameter_x,
                parameter_y,
                children,
                blend_type,
            } => {
                let x = self.get_parameter(parameter_x);
                let y = self.get_parameter(parameter_y);
                self.blend_2d_children(children, [x, y], *blend_type, clips)
            }

            BlendNode::Direct { children } => self.blend_direct_children(children, clips),

            BlendNode::Additive {
                base,
                additive,
                weight,
            } => {
                let base_pose = self.evaluate_node(base, clips);
                let add_pose = self.evaluate_node(additive, clips);
                apply_additive(&base_pose, &add_pose, *weight)
            }
        }
    }

    fn blend_1d_children(
        &self,
        children: &[BlendChild1D],
        value: f32,
        clips: &BTreeMap<String, AnimationClip>,
    ) -> AnimationPose {
        if children.is_empty() {
            return AnimationPose::default();
        }

        if children.len() == 1 {
            return self.evaluate_node(&children[0].node, clips);
        }

        // Sort by threshold
        let mut sorted: Vec<_> = children.iter().collect();
        sorted.sort_by(|a, b| a.threshold.partial_cmp(&b.threshold).unwrap());

        // Find surrounding thresholds
        let mut lower = sorted[0];
        let mut upper = sorted[0];

        for child in &sorted {
            if child.threshold <= value {
                lower = child;
            }
            if child.threshold >= value && upper.threshold <= value {
                upper = child;
            }
        }

        // Clamp to bounds
        if value <= sorted[0].threshold {
            return self.evaluate_node(&sorted[0].node, clips);
        }
        if value >= sorted[sorted.len() - 1].threshold {
            return self.evaluate_node(&sorted[sorted.len() - 1].node, clips);
        }

        // Calculate blend factor
        let range = upper.threshold - lower.threshold;
        let t = if range.abs() < 0.0001 {
            0.0
        } else {
            ((value - lower.threshold) / range).clamp(0.0, 1.0)
        };

        // Blend poses
        let pose_a = self.evaluate_node(&lower.node, clips);
        let pose_b = self.evaluate_node(&upper.node, clips);

        blend_poses(&pose_a, &pose_b, t)
    }

    fn blend_2d_children(
        &self,
        children: &[BlendChild2D],
        position: [f32; 2],
        blend_type: Blend2DType,
        clips: &BTreeMap<String, AnimationClip>,
    ) -> AnimationPose {
        if children.is_empty() {
            return AnimationPose::default();
        }

        if children.len() == 1 {
            return self.evaluate_node(&children[0].node, clips);
        }

        // Calculate weights based on blend type
        let weights = match blend_type {
            Blend2DType::SimpleDirectional | Blend2DType::FreeformDirectional => {
                self.directional_weights(children, position)
            }
            Blend2DType::FreeformCartesian => self.cartesian_weights(children, position),
        };

        // Blend all poses
        let mut result = AnimationPose::default();
        let mut total_weight = 0.0;

        for (i, child) in children.iter().enumerate() {
            if weights[i] > 0.0001 {
                let pose = self.evaluate_node(&child.node, clips);
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

    fn directional_weights(&self, children: &[BlendChild2D], pos: [f32; 2]) -> Vec<f32> {
        // Inverse distance weighting
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
        if total > 0.0 {
            for w in &mut weights {
                *w /= total;
            }
        }

        weights
    }

    fn cartesian_weights(&self, children: &[BlendChild2D], pos: [f32; 2]) -> Vec<f32> {
        // Same as directional for simplicity
        self.directional_weights(children, pos)
    }

    fn blend_direct_children(
        &self,
        children: &[BlendChildDirect],
        clips: &BTreeMap<String, AnimationClip>,
    ) -> AnimationPose {
        let mut result = AnimationPose::default();
        let mut total_weight = 0.0;

        for child in children {
            let weight = self.get_parameter(&child.weight_parameter);
            if weight > 0.0001 {
                let pose = self.evaluate_node(&child.node, clips);
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
}

/// Blend tree node
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum BlendNode {
    /// Single animation clip
    Clip {
        clip: String,
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

/// Child node for 1D blend
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlendChild1D {
    pub node: Box<BlendNode>,
    pub threshold: f32,
}

/// Child node for 2D blend
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlendChild2D {
    pub node: Box<BlendNode>,
    pub position: [f32; 2],
}

/// Child node for direct blend
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlendChildDirect {
    pub node: Box<BlendNode>,
    pub weight_parameter: String,
}

/// 2D blend type
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum Blend2DType {
    #[default]
    SimpleDirectional,
    FreeformDirectional,
    FreeformCartesian,
}

// ============================================================================
// Animation State Machine
// ============================================================================

/// State machine for animation control
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationStateMachine {
    /// All states
    pub states: BTreeMap<String, AnimState>,
    /// All transitions
    pub transitions: Vec<AnimTransition>,
    /// Current state name
    pub current_state: String,
    /// Active transition (if any)
    pub active_transition: Option<ActiveTransition>,
    /// Parameters for conditions
    pub parameters: BTreeMap<String, AnimParameter>,
    /// Current time in state
    pub state_time: f32,
}

impl Default for AnimationStateMachine {
    fn default() -> Self {
        Self::new("idle")
    }
}

impl AnimationStateMachine {
    /// Create a new state machine
    pub fn new(initial_state: impl Into<String>) -> Self {
        Self {
            states: BTreeMap::new(),
            transitions: Vec::new(),
            current_state: initial_state.into(),
            active_transition: None,
            parameters: BTreeMap::new(),
            state_time: 0.0,
        }
    }

    /// Add a state
    pub fn add_state(&mut self, name: impl Into<String>, clip: impl Into<String>) {
        let name = name.into();
        self.states.insert(
            name.clone(),
            AnimState {
                name,
                motion: AnimMotion::Clip(clip.into()),
                speed: 1.0,
                loop_mode: LoopMode::Loop,
            },
        );
    }

    /// Add a state with blend tree
    pub fn add_state_with_tree(&mut self, name: impl Into<String>, tree: BlendTree) {
        let name = name.into();
        self.states.insert(
            name.clone(),
            AnimState {
                name,
                motion: AnimMotion::BlendTree(tree),
                speed: 1.0,
                loop_mode: LoopMode::Loop,
            },
        );
    }

    /// Add a transition
    pub fn add_transition(
        &mut self,
        from: impl Into<String>,
        to: impl Into<String>,
        duration: f32,
    ) -> usize {
        let index = self.transitions.len();
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
        index
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

    /// Add float condition to last transition
    pub fn with_float_condition(
        &mut self,
        parameter: impl Into<String>,
        comparison: Comparison,
        value: f32,
    ) {
        if let Some(t) = self.transitions.last_mut() {
            t.conditions.push(TransitionCondition::Float {
                parameter: parameter.into(),
                comparison,
                value,
            });
        }
    }

    /// Add trigger condition to last transition
    pub fn with_trigger_condition(&mut self, parameter: impl Into<String>) {
        if let Some(t) = self.transitions.last_mut() {
            t.conditions
                .push(TransitionCondition::Trigger { parameter: parameter.into() });
        }
    }

    /// Set exit time for last transition
    pub fn with_exit_time(&mut self, exit_time: f32) {
        if let Some(t) = self.transitions.last_mut() {
            t.has_exit_time = true;
            t.exit_time = exit_time;
        }
    }

    /// Set parameter
    pub fn set_bool(&mut self, name: impl Into<String>, value: bool) {
        self.parameters.insert(name.into(), AnimParameter::Bool(value));
    }

    pub fn set_float(&mut self, name: impl Into<String>, value: f32) {
        self.parameters.insert(name.into(), AnimParameter::Float(value));
    }

    pub fn set_int(&mut self, name: impl Into<String>, value: i32) {
        self.parameters.insert(name.into(), AnimParameter::Int(value));
    }

    pub fn set_trigger(&mut self, name: impl Into<String>) {
        self.parameters.insert(name.into(), AnimParameter::Trigger(true));
    }

    /// Get parameter value
    pub fn get_bool(&self, name: &str) -> bool {
        self.parameters
            .get(name)
            .and_then(|p| {
                if let AnimParameter::Bool(v) = p {
                    Some(*v)
                } else {
                    None
                }
            })
            .unwrap_or(false)
    }

    pub fn get_float(&self, name: &str) -> f32 {
        self.parameters
            .get(name)
            .and_then(|p| {
                if let AnimParameter::Float(v) = p {
                    Some(*v)
                } else {
                    None
                }
            })
            .unwrap_or(0.0)
    }

    /// Update state machine
    pub fn update(&mut self, dt: f32, clips: &BTreeMap<String, AnimationClip>) {
        // Get current state duration
        let current_duration = self
            .states
            .get(&self.current_state)
            .and_then(|s| match &s.motion {
                AnimMotion::Clip(name) => clips.get(name).map(|c| c.duration),
                AnimMotion::BlendTree(_) => Some(1.0), // Blend trees loop
            })
            .unwrap_or(1.0);

        // Update state time
        if self.active_transition.is_none() {
            self.state_time += dt;
        }

        // Check for transitions from current state
        if self.active_transition.is_none() {
            for (i, transition) in self.transitions.iter().enumerate() {
                if transition.from != "*" && transition.from != self.current_state {
                    continue;
                }

                // Check exit time
                if transition.has_exit_time && current_duration > 0.0 {
                    let normalized_time = self.state_time / current_duration;
                    if normalized_time < transition.exit_time {
                        continue;
                    }
                }

                // Check conditions
                if self.check_conditions(&transition.conditions) {
                    self.active_transition = Some(ActiveTransition {
                        transition_index: i,
                        elapsed: 0.0,
                        from_time: self.state_time,
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
                self.state_time = transition.offset;
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
        if conditions.is_empty() {
            return true; // No conditions = always transition
        }

        for condition in conditions {
            let passed = match condition {
                TransitionCondition::Bool { parameter, value } => self
                    .parameters
                    .get(parameter)
                    .map(|p| matches!(p, AnimParameter::Bool(v) if v == value))
                    .unwrap_or(false),
                TransitionCondition::Float {
                    parameter,
                    comparison,
                    value,
                } => self
                    .parameters
                    .get(parameter)
                    .and_then(|p| {
                        if let AnimParameter::Float(v) = p {
                            Some(*v)
                        } else {
                            None
                        }
                    })
                    .map(|v| Self::compare(v, *comparison, *value))
                    .unwrap_or(false),
                TransitionCondition::Int {
                    parameter,
                    comparison,
                    value,
                } => self
                    .parameters
                    .get(parameter)
                    .and_then(|p| {
                        if let AnimParameter::Int(v) = p {
                            Some(*v)
                        } else {
                            None
                        }
                    })
                    .map(|v| Self::compare(v as f32, *comparison, *value as f32))
                    .unwrap_or(false),
                TransitionCondition::Trigger { parameter } => self
                    .parameters
                    .get(parameter)
                    .map(|p| matches!(p, AnimParameter::Trigger(true)))
                    .unwrap_or(false),
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
            if transition.duration <= 0.0 {
                1.0
            } else {
                (active.elapsed / transition.duration).clamp(0.0, 1.0)
            }
        } else {
            0.0
        }
    }

    /// Check if currently transitioning
    pub fn is_transitioning(&self) -> bool {
        self.active_transition.is_some()
    }

    /// Get current state
    pub fn current_state(&self) -> &str {
        &self.current_state
    }

    /// Evaluate state machine to get current pose
    pub fn evaluate(&self, clips: &BTreeMap<String, AnimationClip>) -> AnimationPose {
        let current_pose = self.evaluate_state(&self.current_state, self.state_time, clips);

        if let Some(ref active) = self.active_transition {
            let transition = &self.transitions[active.transition_index];
            let target_pose = self.evaluate_state(&transition.to, active.elapsed + transition.offset, clips);
            let t = self.get_transition_weight();
            blend_poses(&current_pose, &target_pose, t)
        } else {
            current_pose
        }
    }

    fn evaluate_state(
        &self,
        state_name: &str,
        time: f32,
        clips: &BTreeMap<String, AnimationClip>,
    ) -> AnimationPose {
        if let Some(state) = self.states.get(state_name) {
            match &state.motion {
                AnimMotion::Clip(clip_name) => {
                    if let Some(clip) = clips.get(clip_name) {
                        clip.sample(time * state.speed)
                    } else {
                        AnimationPose::default()
                    }
                }
                AnimMotion::BlendTree(tree) => tree.evaluate(clips),
            }
        } else {
            AnimationPose::default()
        }
    }

    /// Force transition to state
    pub fn go_to_state(&mut self, state: impl Into<String>) {
        let state = state.into();
        if self.states.contains_key(&state) {
            self.current_state = state;
            self.active_transition = None;
            self.state_time = 0.0;
        }
    }
}

/// Animation state
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimState {
    pub name: String,
    pub motion: AnimMotion,
    pub speed: f32,
    pub loop_mode: LoopMode,
}

/// Motion type for a state
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum AnimMotion {
    Clip(String),
    BlendTree(BlendTree),
}

/// Transition between states
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

/// Transition condition
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum TransitionCondition {
    Bool { parameter: String, value: bool },
    Float { parameter: String, comparison: Comparison, value: f32 },
    Int { parameter: String, comparison: Comparison, value: i32 },
    Trigger { parameter: String },
}

/// Comparison operator
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum Comparison {
    Greater,
    Less,
    Equal,
    NotEqual,
}

/// Interruption source for transitions
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum InterruptionSource {
    #[default]
    None,
    CurrentState,
    NextState,
    CurrentThenNext,
    NextThenCurrent,
}

/// Active transition state
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ActiveTransition {
    pub transition_index: usize,
    pub elapsed: f32,
    pub from_time: f32,
}

/// Animation parameter
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum AnimParameter {
    Bool(bool),
    Float(f32),
    Int(i32),
    Trigger(bool),
}

// ============================================================================
// Blending Functions
// ============================================================================

/// Blend two poses together
pub fn blend_poses(a: &AnimationPose, b: &AnimationPose, t: f32) -> AnimationPose {
    AnimationPose::blend(a, b, t)
}

/// Apply additive pose on top of base
pub fn apply_additive(base: &AnimationPose, additive: &AnimationPose, weight: f32) -> AnimationPose {
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
            (Some(KeyframeValue::Float(b)), KeyframeValue::Float(a)) => {
                KeyframeValue::Float(b + a * weight)
            }
            (Some(KeyframeValue::Quat(b)), KeyframeValue::Quat(a)) => {
                // Additive quaternion: result = base * (identity.lerp(additive, weight))
                let identity = [0.0, 0.0, 0.0, 1.0];
                let scaled_add = slerp(identity, *a, weight);
                KeyframeValue::Quat(multiply_quat(*b, scaled_add))
            }
            (None, v) => {
                // Apply additive value directly with weight
                match v {
                    KeyframeValue::Vec3(a) => {
                        KeyframeValue::Vec3([a[0] * weight, a[1] * weight, a[2] * weight])
                    }
                    KeyframeValue::Float(a) => KeyframeValue::Float(a * weight),
                    KeyframeValue::Quat(a) => {
                        let identity = [0.0, 0.0, 0.0, 1.0];
                        KeyframeValue::Quat(slerp(identity, *a, weight))
                    }
                    KeyframeValue::Weights(w) => {
                        KeyframeValue::Weights(w.iter().map(|v| v * weight).collect())
                    }
                }
            }
            _ => continue,
        };

        result.channels.insert(target.clone(), new_value);
    }

    result
}

/// Apply bone mask to pose
pub fn apply_bone_mask(pose: &AnimationPose, mask: &BoneMask) -> AnimationPose {
    let mut result = AnimationPose::new();

    for (target, value) in &pose.channels {
        // Extract bone name from target path (e.g., "root/spine/arm" -> check each part)
        let bone_name = target.split('/').last().unwrap_or(target);

        if mask.contains(bone_name) {
            let weight = mask.weight(bone_name);
            if weight >= 1.0 {
                result.channels.insert(target.clone(), value.clone());
            } else {
                // Blend with identity based on weight
                let blended = match value {
                    KeyframeValue::Vec3(v) => {
                        KeyframeValue::Vec3([v[0] * weight, v[1] * weight, v[2] * weight])
                    }
                    KeyframeValue::Float(v) => KeyframeValue::Float(v * weight),
                    KeyframeValue::Quat(v) => {
                        let identity = [0.0, 0.0, 0.0, 1.0];
                        KeyframeValue::Quat(slerp(identity, *v, weight))
                    }
                    KeyframeValue::Weights(w) => {
                        KeyframeValue::Weights(w.iter().map(|v| v * weight).collect())
                    }
                };
                result.channels.insert(target.clone(), blended);
            }
        }
    }

    result
}

/// Multiply quaternions
pub fn multiply_quat(a: [f32; 4], b: [f32; 4]) -> [f32; 4] {
    [
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    ]
}

// ============================================================================
// Hot-Reload State
// ============================================================================

/// Animation layers state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationLayersState {
    pub layers: Vec<AnimationLayer>,
}

impl AnimationLayers {
    /// Save state for hot-reload
    pub fn save_state(&self) -> AnimationLayersState {
        AnimationLayersState {
            layers: self.layers.clone(),
        }
    }

    /// Restore state from hot-reload
    pub fn restore_state(&mut self, state: AnimationLayersState) {
        self.layers = state.layers;
    }
}

/// Blend tree state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlendTreeState {
    pub root: BlendNode,
    pub parameters: BTreeMap<String, f32>,
    pub time: f32,
    pub speed: f32,
    pub playing: bool,
}

impl BlendTree {
    /// Save state for hot-reload
    pub fn save_state(&self) -> BlendTreeState {
        BlendTreeState {
            root: self.root.clone(),
            parameters: self.parameters.clone(),
            time: self.time,
            speed: self.speed,
            playing: self.playing,
        }
    }

    /// Restore state from hot-reload
    pub fn restore_state(&mut self, state: BlendTreeState) {
        self.root = state.root;
        self.parameters = state.parameters;
        self.time = state.time;
        self.speed = state.speed;
        self.playing = state.playing;
    }
}

/// State machine state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct StateMachineState {
    pub states: BTreeMap<String, AnimState>,
    pub transitions: Vec<AnimTransition>,
    pub current_state: String,
    pub active_transition: Option<ActiveTransition>,
    pub parameters: BTreeMap<String, AnimParameter>,
    pub state_time: f32,
}

impl AnimationStateMachine {
    /// Save state for hot-reload
    pub fn save_state(&self) -> StateMachineState {
        StateMachineState {
            states: self.states.clone(),
            transitions: self.transitions.clone(),
            current_state: self.current_state.clone(),
            active_transition: self.active_transition.clone(),
            parameters: self.parameters.clone(),
            state_time: self.state_time,
        }
    }

    /// Restore state from hot-reload
    pub fn restore_state(&mut self, state: StateMachineState) {
        self.states = state.states;
        self.transitions = state.transitions;
        self.parameters = state.parameters;
        self.state_time = state.state_time;

        // Validate current state still exists
        if self.states.contains_key(&state.current_state) {
            self.current_state = state.current_state;
            self.active_transition = state.active_transition;
        } else {
            // Reset to first available state
            if let Some(first) = self.states.keys().next() {
                self.current_state = first.clone();
            }
            self.active_transition = None;
        }
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::animation::{AnimationTrack, AnimatedProperty, Keyframe, Interpolation};

    fn create_test_clip(name: &str, duration: f32, value: f32) -> AnimationClip {
        let mut clip = AnimationClip::new(name, duration);
        clip.add_track(
            "root/translation",
            AnimationTrack {
                target: "root".to_string(),
                property: AnimatedProperty::Translation,
                keyframes: vec![
                    Keyframe::vec3(0.0, [0.0, value, 0.0]),
                    Keyframe::vec3(duration, [0.0, value, 0.0]),
                ],
                interpolation: Interpolation::Linear,
            },
        );
        clip
    }

    fn create_test_clips() -> BTreeMap<String, AnimationClip> {
        let mut clips = BTreeMap::new();
        clips.insert("idle".to_string(), create_test_clip("idle", 1.0, 0.0));
        clips.insert("walk".to_string(), create_test_clip("walk", 1.0, 1.0));
        clips.insert("run".to_string(), create_test_clip("run", 1.0, 2.0));
        clips
    }

    #[test]
    fn test_bone_mask() {
        let mask = BoneMask::new(&["spine", "arm_l", "arm_r"]);

        assert!(mask.contains("spine"));
        assert!(mask.contains("arm_l"));
        assert!(!mask.contains("leg_l"));

        assert_eq!(mask.weight("spine"), 1.0);
        assert_eq!(mask.weight("leg_l"), 0.0);
    }

    #[test]
    fn test_bone_mask_with_weights() {
        let mask = BoneMask::with_weights(&[("spine", 1.0), ("arm_l", 0.5), ("arm_r", 0.5)]);

        assert_eq!(mask.weight("spine"), 1.0);
        assert_eq!(mask.weight("arm_l"), 0.5);
    }

    #[test]
    fn test_animation_layers_default() {
        let layers = AnimationLayers::default();

        assert_eq!(layers.len(), 1);
        assert_eq!(layers.layers[0].name, "Base");
        assert_eq!(layers.layers[0].blend_mode, LayerBlendMode::Override);
    }

    #[test]
    fn test_animation_layers_add() {
        let mut layers = AnimationLayers::new();

        let idx = layers.add_layer("UpperBody", LayerBlendMode::Additive);
        assert_eq!(idx, 1);
        assert_eq!(layers.len(), 2);
        assert_eq!(layers.layers[1].blend_mode, LayerBlendMode::Additive);
    }

    #[test]
    fn test_blend_tree_1d() {
        let clips = create_test_clips();
        let mut tree = BlendTree::blend_1d("speed", vec![
            ("idle", 0.0),
            ("walk", 1.0),
            ("run", 2.0),
        ]);

        // At speed=0, should be idle (y=0)
        tree.set_parameter("speed", 0.0);
        let pose = tree.evaluate(&clips);
        if let Some(KeyframeValue::Vec3(v)) = pose.get("root/translation") {
            assert!((v[1] - 0.0).abs() < 0.01);
        }

        // At speed=1, should be walk (y=1)
        tree.set_parameter("speed", 1.0);
        let pose = tree.evaluate(&clips);
        if let Some(KeyframeValue::Vec3(v)) = pose.get("root/translation") {
            assert!((v[1] - 1.0).abs() < 0.01);
        }

        // At speed=0.5, should be blend of idle and walk (y=0.5)
        tree.set_parameter("speed", 0.5);
        let pose = tree.evaluate(&clips);
        if let Some(KeyframeValue::Vec3(v)) = pose.get("root/translation") {
            assert!((v[1] - 0.5).abs() < 0.01);
        }
    }

    #[test]
    fn test_blend_tree_2d() {
        let mut clips = BTreeMap::new();
        clips.insert("center".to_string(), create_test_clip("center", 1.0, 0.0));
        clips.insert("forward".to_string(), create_test_clip("forward", 1.0, 1.0));
        clips.insert("back".to_string(), create_test_clip("back", 1.0, -1.0));

        let mut tree = BlendTree::blend_2d("x", "y", vec![
            ("center", [0.0, 0.0]),
            ("forward", [0.0, 1.0]),
            ("back", [0.0, -1.0]),
        ]);

        // At center
        tree.set_parameter("x", 0.0);
        tree.set_parameter("y", 0.0);
        let pose = tree.evaluate(&clips);
        assert!(!pose.channels.is_empty());
    }

    #[test]
    fn test_state_machine_basic() {
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle_clip");
        sm.add_state("walk", "walk_clip");

        assert_eq!(sm.current_state(), "idle");
        assert!(!sm.is_transitioning());
    }

    #[test]
    fn test_state_machine_transition() {
        let clips = create_test_clips();
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle");
        sm.add_state("walk", "walk");
        sm.add_transition("idle", "walk", 0.2);
        sm.with_bool_condition("walking", true);

        // Trigger transition
        sm.set_bool("walking", true);
        sm.update(0.0, &clips);

        assert!(sm.is_transitioning());

        // Complete transition
        sm.update(0.3, &clips);
        assert!(!sm.is_transitioning());
        assert_eq!(sm.current_state(), "walk");
    }

    #[test]
    fn test_state_machine_exit_time() {
        let clips = create_test_clips();
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle");
        sm.add_state("walk", "walk");
        sm.add_transition("idle", "walk", 0.2);
        sm.with_exit_time(0.5);

        // Update to time 0.3 (before exit time)
        sm.state_time = 0.3;
        sm.update(0.0, &clips);
        assert!(!sm.is_transitioning());

        // Update to time 0.6 (after exit time)
        sm.state_time = 0.6;
        sm.update(0.0, &clips);
        assert!(sm.is_transitioning());
    }

    #[test]
    fn test_state_machine_trigger() {
        let clips = create_test_clips();
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle");
        sm.add_state("jump", "walk");
        sm.add_transition("idle", "jump", 0.1);
        sm.with_trigger_condition("jump");

        // Set trigger and update
        sm.set_trigger("jump");
        sm.update(0.0, &clips);
        assert!(sm.is_transitioning());

        // Trigger should be reset
        sm.update(0.0, &clips);
        // Trigger is now false, no new transition would start
    }

    #[test]
    fn test_state_machine_float_condition() {
        let clips = create_test_clips();
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle");
        sm.add_state("run", "run");
        sm.add_transition("idle", "run", 0.2);
        sm.with_float_condition("speed", Comparison::Greater, 1.5);

        // Speed below threshold
        sm.set_float("speed", 1.0);
        sm.update(0.0, &clips);
        assert!(!sm.is_transitioning());

        // Speed above threshold
        sm.set_float("speed", 2.0);
        sm.update(0.0, &clips);
        assert!(sm.is_transitioning());
    }

    #[test]
    fn test_additive_blend() {
        let mut base = AnimationPose::new();
        base.set("root", KeyframeValue::Vec3([0.0, 1.0, 0.0]));

        let mut additive = AnimationPose::new();
        additive.set("root", KeyframeValue::Vec3([0.5, 0.0, 0.0]));

        let result = apply_additive(&base, &additive, 1.0);

        if let Some(KeyframeValue::Vec3(v)) = result.get("root") {
            assert!((v[0] - 0.5).abs() < 0.01);
            assert!((v[1] - 1.0).abs() < 0.01);
        }
    }

    #[test]
    fn test_additive_blend_with_weight() {
        let mut base = AnimationPose::new();
        base.set("root", KeyframeValue::Vec3([0.0, 1.0, 0.0]));

        let mut additive = AnimationPose::new();
        additive.set("root", KeyframeValue::Vec3([1.0, 0.0, 0.0]));

        let result = apply_additive(&base, &additive, 0.5);

        if let Some(KeyframeValue::Vec3(v)) = result.get("root") {
            assert!((v[0] - 0.5).abs() < 0.01);
            assert!((v[1] - 1.0).abs() < 0.01);
        }
    }

    #[test]
    fn test_layers_serialization() {
        let mut layers = AnimationLayers::default();
        layers.add_layer("UpperBody", LayerBlendMode::Additive);
        layers.set_weight(1, 0.75);

        let saved = layers.save_state();
        let json = serde_json::to_string(&saved).unwrap();
        let restored: AnimationLayersState = serde_json::from_str(&json).unwrap();

        let mut new_layers = AnimationLayers::default();
        new_layers.restore_state(restored);

        assert_eq!(new_layers.len(), 2);
        assert_eq!(new_layers.layers[1].weight, 0.75);
    }

    #[test]
    fn test_blend_tree_serialization() {
        let mut tree = BlendTree::blend_1d("speed", vec![
            ("idle", 0.0),
            ("walk", 1.0),
        ]);
        tree.set_parameter("speed", 0.5);
        tree.time = 1.5;

        let saved = tree.save_state();
        let json = serde_json::to_string(&saved).unwrap();
        let restored: BlendTreeState = serde_json::from_str(&json).unwrap();

        let mut new_tree = BlendTree::default();
        new_tree.restore_state(restored);

        assert_eq!(new_tree.get_parameter("speed"), 0.5);
        assert_eq!(new_tree.time, 1.5);
    }

    #[test]
    fn test_state_machine_serialization() {
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle_clip");
        sm.add_state("walk", "walk_clip");
        sm.current_state = "walk".to_string();
        sm.set_float("speed", 1.5);
        sm.state_time = 0.75;

        let saved = sm.save_state();
        let json = serde_json::to_string(&saved).unwrap();
        let restored: StateMachineState = serde_json::from_str(&json).unwrap();

        let mut new_sm = AnimationStateMachine::new("idle");
        new_sm.restore_state(restored);

        assert_eq!(new_sm.current_state(), "walk");
        assert_eq!(new_sm.get_float("speed"), 1.5);
        assert_eq!(new_sm.state_time, 0.75);
    }

    #[test]
    fn test_state_machine_restore_validates_state() {
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle_clip");
        sm.add_state("walk", "walk_clip");

        let mut saved = sm.save_state();
        saved.current_state = "deleted_state".to_string();

        let mut new_sm = AnimationStateMachine::new("idle");
        new_sm.add_state("idle", "idle_clip");
        new_sm.restore_state(saved);

        // Should reset to valid state
        assert!(new_sm.states.contains_key(&new_sm.current_state));
    }

    #[test]
    fn test_go_to_state() {
        let mut sm = AnimationStateMachine::new("idle");
        sm.add_state("idle", "idle_clip");
        sm.add_state("walk", "walk_clip");
        sm.state_time = 1.0;

        sm.go_to_state("walk");

        assert_eq!(sm.current_state(), "walk");
        assert_eq!(sm.state_time, 0.0);
        assert!(!sm.is_transitioning());
    }

    #[test]
    fn test_multiply_quat() {
        let identity = [0.0, 0.0, 0.0, 1.0];
        let result = multiply_quat(identity, identity);

        assert!((result[0] - 0.0).abs() < 0.001);
        assert!((result[1] - 0.0).abs() < 0.001);
        assert!((result[2] - 0.0).abs() < 0.001);
        assert!((result[3] - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_find_layer() {
        let mut layers = AnimationLayers::new();
        layers.add_layer("UpperBody", LayerBlendMode::Additive);
        layers.add_layer("Face", LayerBlendMode::Override);

        assert_eq!(layers.find_layer("Base"), Some(0));
        assert_eq!(layers.find_layer("UpperBody"), Some(1));
        assert_eq!(layers.find_layer("Face"), Some(2));
        assert_eq!(layers.find_layer("Unknown"), None);
    }

    #[test]
    fn test_comparison_operators() {
        assert!(AnimationStateMachine::compare(2.0, Comparison::Greater, 1.0));
        assert!(!AnimationStateMachine::compare(1.0, Comparison::Greater, 2.0));
        assert!(AnimationStateMachine::compare(1.0, Comparison::Less, 2.0));
        assert!(AnimationStateMachine::compare(1.0, Comparison::Equal, 1.0));
        assert!(AnimationStateMachine::compare(1.0, Comparison::NotEqual, 2.0));
    }
}
