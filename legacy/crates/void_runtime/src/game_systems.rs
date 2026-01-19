//! Game Systems Integration
//!
//! Integrates void_physics and void_triggers into the runtime for
//! physics-based gameplay with trigger volumes.

use void_physics::prelude::*;
use void_triggers::prelude::*;
use void_triggers::system::TriggerEntity;
use void_math::Vec3;
use std::collections::HashMap;

/// Player controller state
#[derive(Debug, Clone)]
pub struct PlayerController {
    /// Player entity ID
    pub entity_id: u64,
    /// Physics body handle
    pub body_handle: Option<RigidBodyHandle>,
    /// Player collider handle (for raycast exclusion)
    pub collider_handle: Option<ColliderHandle>,
    /// Movement speed
    pub move_speed: f32,
    /// Jump velocity
    pub jump_velocity: f32,
    /// Is player grounded
    pub grounded: bool,
    /// Ground check distance
    pub ground_check_distance: f32,
    /// Player height (for camera offset)
    pub height: f32,
    /// Current velocity for smoothing
    pub velocity: Vec3,
    /// Manually tracked position (bypasses kinematic body issues)
    pub position: Vec3,
}

impl Default for PlayerController {
    fn default() -> Self {
        Self {
            entity_id: 0,
            body_handle: None,
            collider_handle: None,
            move_speed: 5.0,
            jump_velocity: 8.0,
            grounded: false,
            ground_check_distance: 0.5,
            height: 1.8,
            velocity: Vec3::ZERO,
            position: Vec3::ZERO,
        }
    }
}

/// Game world containing physics and triggers
pub struct GameWorld {
    /// Physics simulation
    pub physics: PhysicsWorld,
    /// Trigger system
    pub triggers: TriggerSystem,
    /// Player controller
    pub player: PlayerController,
    /// Entity to physics body mapping
    entity_bodies: HashMap<u64, RigidBodyHandle>,
    /// Trigger entity transforms (id, position, scale)
    trigger_transforms: Vec<(u64, [f32; 3], [f32; 3])>,
    /// All entity positions for trigger checks
    entity_positions: Vec<TriggerEntity>,
    /// Collected trigger events
    pending_trigger_events: Vec<TriggerEvent>,
    /// Whether game systems are enabled
    pub enabled: bool,
}

impl GameWorld {
    /// Create a new game world
    pub fn new() -> Self {
        // Use default config with standard gravity
        let config = PhysicsConfig::default();

        Self {
            physics: PhysicsWorld::new(config),
            triggers: TriggerSystem::new(),
            player: PlayerController::default(),
            entity_bodies: HashMap::new(),
            trigger_transforms: Vec::new(),
            entity_positions: Vec::new(),
            pending_trigger_events: Vec::new(),
            enabled: false,
        }
    }

    /// Enable game systems
    pub fn enable(&mut self) {
        self.enabled = true;
        // Sync query pipeline so raycasts work immediately
        self.physics.sync_query_pipeline();
        log::info!("Game systems enabled");
    }

    /// Create a static physics body (floor, walls)
    pub fn create_static_body(
        &mut self,
        entity_id: u64,
        position: [f32; 3],
        shape: ColliderShape,
    ) -> RigidBodyHandle {
        let body = self.physics.create_rigid_body_for_entity(
            entity_id as u128,
            RigidBodyDesc::fixed().with_position(position[0], position[1], position[2]),
        );

        let material = PhysicsMaterial {
            friction: 0.8,
            restitution: 0.0,
            ..Default::default()
        };

        self.physics.create_collider_for_entity(
            entity_id as u128,
            ColliderDesc::new(shape).with_material(material),
            Some(body),
        );

        self.entity_bodies.insert(entity_id, body);
        body
    }

    /// Create a dynamic physics body (player, enemies, objects)
    pub fn create_dynamic_body(
        &mut self,
        entity_id: u64,
        position: [f32; 3],
        shape: ColliderShape,
        mass: f32,
    ) -> RigidBodyHandle {
        let body = self.physics.create_rigid_body_for_entity(
            entity_id as u128,
            RigidBodyDesc::dynamic()
                .with_position(position[0], position[1], position[2])
                .with_mass(mass),
        );

        let material = PhysicsMaterial {
            friction: 0.5,
            restitution: 0.0,
            density: mass,
            ..Default::default()
        };

        self.physics.create_collider_for_entity(
            entity_id as u128,
            ColliderDesc::new(shape).with_material(material),
            Some(body),
        );

        self.entity_bodies.insert(entity_id, body);
        body
    }

    /// Create a kinematic body for the player (character controller style)
    pub fn create_player_body(
        &mut self,
        entity_id: u64,
        position: [f32; 3],
    ) -> RigidBodyHandle {
        // Use kinematic for player to avoid getting pushed around
        let body = self.physics.create_rigid_body_for_entity(
            entity_id as u128,
            RigidBodyDesc::kinematic()
                .with_position(position[0], position[1], position[2]),
        );

        let material = PhysicsMaterial {
            friction: 0.0,
            restitution: 0.0,
            ..Default::default()
        };

        // Capsule collider for player (height 1.8, radius 0.4)
        let collider = self.physics.create_collider_for_entity(
            entity_id as u128,
            ColliderDesc::new(ColliderShape::CapsuleY {
                half_height: 0.7,
                radius: 0.4,
            })
            .with_material(material),
            Some(body),
        );

        self.entity_bodies.insert(entity_id, body);
        self.player.entity_id = entity_id;
        self.player.body_handle = Some(body);
        self.player.collider_handle = Some(collider);
        self.player.position = Vec3::new(position[0], position[1], position[2]);

        body
    }

    /// Create a trigger volume
    pub fn create_trigger(
        &mut self,
        entity_id: u64,
        position: [f32; 3],
        scale: [f32; 3],
        volume: TriggerVolume,
    ) {
        let trigger = TriggerComponent::new(volume);
        self.triggers.register_trigger(entity_id, trigger);
        self.trigger_transforms.push((entity_id, position, scale));
    }

    /// Update player movement based on input
    pub fn update_player_movement(
        &mut self,
        move_forward: f32,
        move_right: f32,
        jump: bool,
        yaw: f32,
        delta_time: f32,
    ) {
        if !self.enabled {
            return;
        }

        // Use manually tracked position instead of physics body
        let pos = self.player.position;

        // Calculate movement direction based on yaw
        let forward = Vec3::new(-yaw.sin(), 0.0, -yaw.cos());
        let right = Vec3::new(-forward.z, 0.0, forward.x);

        // Calculate desired velocity
        let move_dir = forward * move_forward + right * move_right;
        let move_speed = self.player.move_speed;

        let mut new_velocity = if move_dir.length_squared() > 0.01 {
            move_dir.normalize() * move_speed
        } else {
            Vec3::ZERO
        };

        // Ground check: cast ray from player center downward
        // Capsule: half_height=0.7, radius=0.4, so bottom is 1.1 below center
        let capsule_half_extent = 0.7 + 0.4;

        // Ray starts from player center
        let ray_origin = [pos.x, pos.y, pos.z];

        // Max distance: from center to ground, plus ground check margin
        let mut raycast_options = void_physics::query::RaycastOptions {
            max_distance: capsule_half_extent + self.player.ground_check_distance,
            solid_only: true,
            ..Default::default()
        };

        // Exclude player's own collider from raycast
        if let Some(player_collider) = self.player.collider_handle {
            raycast_options.exclude.push(player_collider);
        }

        let ground_check = self.physics.query().raycast(
            ray_origin,
            [0.0, -1.0, 0.0],
            &raycast_options,
        );

        // Determine ground state and calculate target Y position
        let mut target_y = pos.y;

        if let Some(hit) = &ground_check {
            // Ground detected - hit.distance is from ray_origin to ground surface
            // Player center should be at: ground_y + capsule_half_extent
            let ground_y = hit.point[1];
            let ideal_y = ground_y + capsule_half_extent;

            // How far is the capsule bottom from the ground?
            let capsule_bottom_y = pos.y - capsule_half_extent;
            let gap = capsule_bottom_y - ground_y;

            if gap < self.player.ground_check_distance && gap > -0.3 {
                // Close to ground - snap to it
                self.player.grounded = true;
                target_y = ideal_y;
                new_velocity.y = 0.0;

                // Jump if grounded
                if jump {
                    new_velocity.y = self.player.jump_velocity;
                    self.player.grounded = false;
                    target_y = pos.y; // Don't snap when jumping
                }
            } else if gap < 0.0 {
                // Below ground - push up
                self.player.grounded = true;
                target_y = ideal_y;
                new_velocity.y = 0.0;
            } else {
                // Above ground but not close enough - falling
                self.player.grounded = false;
                new_velocity.y = self.player.velocity.y - 9.81 * delta_time;
                target_y = pos.y + new_velocity.y * delta_time;
            }
        } else {
            // No ground detected - falling
            self.player.grounded = false;
            new_velocity.y = self.player.velocity.y - 9.81 * delta_time;
            target_y = pos.y + new_velocity.y * delta_time;
        }

        self.player.velocity = new_velocity;

        // Update manually tracked position
        self.player.position = Vec3::new(
            pos.x + new_velocity.x * delta_time,
            target_y,
            pos.z + new_velocity.z * delta_time,
        );
    }

    /// Get player position for camera
    pub fn get_player_position(&self) -> Option<[f32; 3]> {
        if self.player.body_handle.is_some() {
            Some([self.player.position.x, self.player.position.y, self.player.position.z])
        } else {
            None
        }
    }

    /// Get player eye position (for first-person camera)
    pub fn get_player_eye_position(&self) -> Option<[f32; 3]> {
        let mut pos = self.get_player_position()?;
        // Eye height is near top of capsule
        pos[1] += self.player.height / 2.0 - 0.2;
        Some(pos)
    }

    /// Step the physics simulation
    pub fn step(&mut self, delta_time: f32) {
        if !self.enabled {
            return;
        }

        // Step physics
        self.physics.step(delta_time);

        // Update entity positions for trigger checks
        self.entity_positions.clear();
        for (&entity_id, &body_handle) in &self.entity_bodies {
            if let Ok(pos) = self.physics.get_body_position(body_handle) {
                self.entity_positions.push(TriggerEntity::new(entity_id).with_position(pos));
            }
        }

        // Update triggers
        self.triggers.update(delta_time, &self.trigger_transforms, &self.entity_positions);

        // Collect trigger events
        self.pending_trigger_events = self.triggers.drain_events();

        // Process trigger events
        for event in &self.pending_trigger_events {
            match event.event_type {
                TriggerEventType::Enter => {
                    log::info!("Entity {} entered trigger {}", event.other_entity, event.trigger_entity);
                }
                TriggerEventType::Exit => {
                    log::info!("Entity {} exited trigger {}", event.other_entity, event.trigger_entity);
                }
                TriggerEventType::Stay => {
                    // Don't log stay events - too noisy
                }
            }
        }

        // Process physics collision events
        for event in self.physics.collision_events() {
            if event.is_sensor {
                // Sensor events are handled by triggers
                continue;
            }

            if event.is_started() {
                log::debug!(
                    "Collision between entities {} and {}",
                    event.user_data1,
                    event.user_data2
                );
            }
        }
    }

    /// Get body position for an entity
    pub fn get_entity_position(&self, entity_id: u64) -> Option<[f32; 3]> {
        let body_handle = self.entity_bodies.get(&entity_id)?;
        self.physics.get_body_position(*body_handle).ok()
    }

    /// Get pending trigger events
    pub fn trigger_events(&self) -> &[TriggerEvent] {
        &self.pending_trigger_events
    }

    /// Check if player is in a specific trigger
    pub fn is_player_in_trigger(&self, trigger_id: u64) -> bool {
        if let Some(trigger) = self.triggers.get_trigger(trigger_id) {
            trigger.is_inside(self.player.entity_id)
        } else {
            false
        }
    }

    /// Get physics debug info
    pub fn debug_info(&self) -> String {
        format!(
            "Physics: {} bodies, {} colliders | Triggers: {} | Player grounded: {}",
            self.physics.body_count(),
            self.physics.collider_count(),
            self.triggers.trigger_count(),
            self.player.grounded
        )
    }
}

impl Default for GameWorld {
    fn default() -> Self {
        Self::new()
    }
}

// ============================================================================
// TOML Configuration Structures for Game Systems (docs 21-29)
// ============================================================================

use serde::{Deserialize, Serialize};

// ============================================================================
// Scripting System (doc 21)
// ============================================================================

/// C++ class definition for scripting
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct CppClassDef {
    /// Class name (must match C++ class)
    pub class: String,
    /// Optional library to load from
    #[serde(default)]
    pub library: Option<String>,
    /// Constructor arguments
    #[serde(default)]
    pub args: HashMap<String, ScriptValue>,
    /// Property overrides
    #[serde(default)]
    pub properties: HashMap<String, ScriptValue>,
}

/// Blueprint definition for visual scripting
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct BlueprintDef {
    /// Blueprint asset path
    pub path: String,
    /// Variable overrides
    #[serde(default)]
    pub variables: HashMap<String, ScriptValue>,
}

/// Script value - can be various types
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(untagged)]
pub enum ScriptValue {
    Bool(bool),
    Int(i64),
    Float(f64),
    String(String),
    Vec2([f64; 2]),
    Vec3([f64; 3]),
    Vec4([f64; 4]),
    Array(Vec<ScriptValue>),
    Map(HashMap<String, ScriptValue>),
}

impl Default for ScriptValue {
    fn default() -> Self {
        ScriptValue::Int(0)
    }
}

/// Event binding definition
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct EventBindingDef {
    /// Event name to bind
    pub event: String,
    /// Function to call
    pub function: String,
    /// Target entity (if different from self)
    #[serde(default)]
    pub target: Option<String>,
}

// ============================================================================
// Enhanced Trigger System (doc 22)
// ============================================================================

/// Full trigger definition matching doc 22
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct TriggerDefinition {
    /// Trigger volume shape
    pub volume: TriggerVolumeDef,
    /// Trigger mode
    #[serde(default)]
    pub mode: TriggerModeDef,
    /// Filter configuration
    #[serde(default)]
    pub filter: TriggerFilterDef,
    /// Actions to execute
    #[serde(default)]
    pub actions: Vec<TriggerActionDef>,
    /// Whether trigger is initially enabled
    #[serde(default = "default_true")]
    pub enabled: bool,
    /// Cooldown between activations
    #[serde(default)]
    pub cooldown: f32,
    /// Delay before action executes
    #[serde(default)]
    pub delay: f32,
    /// Debug visualization
    #[serde(default)]
    pub debug_draw: bool,
}

impl Default for TriggerDefinition {
    fn default() -> Self {
        Self {
            volume: TriggerVolumeDef::default(),
            mode: TriggerModeDef::Repeatable,
            filter: TriggerFilterDef::default(),
            actions: Vec::new(),
            enabled: true,
            cooldown: 0.0,
            delay: 0.0,
            debug_draw: false,
        }
    }
}

/// Trigger volume shapes
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "lowercase")]
pub enum TriggerVolumeDef {
    Box {
        #[serde(default = "default_trigger_size")]
        size: [f32; 3],
    },
    Sphere {
        #[serde(default = "default_trigger_radius")]
        radius: f32,
    },
    Capsule {
        #[serde(default = "default_trigger_radius")]
        radius: f32,
        #[serde(default = "default_trigger_height")]
        height: f32,
    },
    Cylinder {
        #[serde(default = "default_trigger_radius")]
        radius: f32,
        #[serde(default = "default_trigger_height")]
        height: f32,
    },
    Mesh {
        path: String,
    },
}

fn default_trigger_size() -> [f32; 3] { [2.0, 2.0, 2.0] }
fn default_trigger_radius() -> f32 { 1.0 }
fn default_trigger_height() -> f32 { 2.0 }

impl Default for TriggerVolumeDef {
    fn default() -> Self {
        Self::Box { size: default_trigger_size() }
    }
}

/// Trigger mode
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum TriggerModeDef {
    /// Fire once ever
    Once,
    /// Fire once per entity
    OncePerEntity,
    /// Fire every time
    Repeatable,
    /// Fire while inside
    WhileInside,
    /// Fire on exit only
    OnExit,
}

impl Default for TriggerModeDef {
    fn default() -> Self {
        Self::Repeatable
    }
}

/// Trigger filter configuration
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct TriggerFilterDef {
    /// Required tags (any of)
    #[serde(default)]
    pub tags: Vec<String>,
    /// Required layers (bitmask or names)
    #[serde(default)]
    pub layers: Vec<String>,
    /// Require player specifically
    #[serde(default)]
    pub player_only: bool,
    /// Entity types to include
    #[serde(default)]
    pub entity_types: Vec<String>,
    /// Entities to exclude by name
    #[serde(default)]
    pub exclude: Vec<String>,
}

/// Trigger action definition
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum TriggerActionDef {
    /// Call a script function
    CallFunction {
        target: String,
        function: String,
        #[serde(default)]
        args: Vec<ScriptValue>,
    },
    /// Set a state variable
    SetState {
        variable: String,
        value: ScriptValue,
    },
    /// Play a sound
    PlaySound {
        sound: String,
        #[serde(default)]
        volume: Option<f32>,
    },
    /// Spawn an entity
    SpawnEntity {
        prefab: String,
        #[serde(default)]
        position: Option<[f32; 3]>,
        #[serde(default)]
        relative: bool,
    },
    /// Enable/disable an entity
    SetEnabled {
        target: String,
        enabled: bool,
    },
    /// Load a new scene
    LoadScene {
        scene: String,
        #[serde(default)]
        spawn_point: Option<String>,
    },
    /// Show UI element
    ShowUI {
        element: String,
        #[serde(default)]
        data: HashMap<String, ScriptValue>,
    },
    /// Apply damage
    ApplyDamage {
        amount: f32,
        #[serde(default)]
        damage_type: String,
    },
    /// Add item to inventory
    AddItem {
        item: String,
        #[serde(default = "default_one")]
        count: u32,
    },
    /// Start quest
    StartQuest {
        quest: String,
    },
    /// Complete objective
    CompleteObjective {
        quest: String,
        objective: String,
    },
    /// Teleport entity
    Teleport {
        #[serde(default)]
        target: Option<String>,
        destination: [f32; 3],
    },
    /// Custom event
    SendEvent {
        event: String,
        #[serde(default)]
        data: HashMap<String, ScriptValue>,
    },
}

fn default_one() -> u32 { 1 }

// ============================================================================
// Full Physics System (doc 23)
// ============================================================================

/// Complete physics definition matching doc 23
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct FullPhysicsDef {
    /// Body type
    #[serde(default)]
    pub body_type: PhysicsBodyType,
    /// Collider configuration
    #[serde(default)]
    pub collider: ColliderDef,
    /// Mass and inertia
    #[serde(default = "default_physics_mass")]
    pub mass: f32,
    /// Linear damping
    #[serde(default)]
    pub linear_damping: f32,
    /// Angular damping
    #[serde(default)]
    pub angular_damping: f32,
    /// Gravity scale
    #[serde(default = "default_one_f32")]
    pub gravity_scale: f32,
    /// Can rotate
    #[serde(default = "default_true")]
    pub can_rotate: bool,
    /// Lock rotation axes
    #[serde(default)]
    pub lock_rotation: [bool; 3],
    /// Lock position axes
    #[serde(default)]
    pub lock_position: [bool; 3],
    /// Is initially asleep
    #[serde(default)]
    pub start_asleep: bool,
    /// CCD (continuous collision detection)
    #[serde(default)]
    pub ccd: bool,
    /// Collision groups
    #[serde(default)]
    pub collision_groups: CollisionGroupsDef,
}

fn default_physics_mass() -> f32 { 1.0 }
fn default_one_f32() -> f32 { 1.0 }
fn default_true() -> bool { true }

impl Default for FullPhysicsDef {
    fn default() -> Self {
        Self {
            body_type: PhysicsBodyType::Dynamic,
            collider: ColliderDef::default(),
            mass: default_physics_mass(),
            linear_damping: 0.0,
            angular_damping: 0.05,
            gravity_scale: 1.0,
            can_rotate: true,
            lock_rotation: [false; 3],
            lock_position: [false; 3],
            start_asleep: false,
            ccd: false,
            collision_groups: CollisionGroupsDef::default(),
        }
    }
}

/// Physics body type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum PhysicsBodyType {
    Static,
    Dynamic,
    Kinematic,
}

impl Default for PhysicsBodyType {
    fn default() -> Self {
        Self::Dynamic
    }
}

/// Collider definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ColliderDef {
    /// Shape
    #[serde(default)]
    pub shape: ColliderShapeDef,
    /// Material properties
    #[serde(default)]
    pub material: PhysicsMaterialDef,
    /// Is sensor (no physics response)
    #[serde(default)]
    pub is_sensor: bool,
    /// Offset from entity center
    #[serde(default)]
    pub offset: [f32; 3],
    /// Rotation offset (euler degrees)
    #[serde(default)]
    pub rotation: [f32; 3],
}

impl Default for ColliderDef {
    fn default() -> Self {
        Self {
            shape: ColliderShapeDef::default(),
            material: PhysicsMaterialDef::default(),
            is_sensor: false,
            offset: [0.0; 3],
            rotation: [0.0; 3],
        }
    }
}

/// Collider shape definition
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "lowercase")]
pub enum ColliderShapeDef {
    Box {
        #[serde(default = "default_collider_size")]
        size: [f32; 3],
    },
    Sphere {
        #[serde(default = "default_collider_radius")]
        radius: f32,
    },
    Capsule {
        #[serde(default = "default_collider_radius")]
        radius: f32,
        #[serde(default = "default_collider_height")]
        height: f32,
        #[serde(default)]
        axis: CapsuleAxis,
    },
    Cylinder {
        #[serde(default = "default_collider_radius")]
        radius: f32,
        #[serde(default = "default_collider_height")]
        height: f32,
    },
    Cone {
        #[serde(default = "default_collider_radius")]
        radius: f32,
        #[serde(default = "default_collider_height")]
        height: f32,
    },
    ConvexHull {
        mesh: String,
    },
    TriMesh {
        mesh: String,
    },
    Compound {
        shapes: Vec<CompoundShapePart>,
    },
}

fn default_collider_size() -> [f32; 3] { [1.0, 1.0, 1.0] }
fn default_collider_radius() -> f32 { 0.5 }
fn default_collider_height() -> f32 { 1.0 }

impl Default for ColliderShapeDef {
    fn default() -> Self {
        Self::Box { size: default_collider_size() }
    }
}

/// Capsule axis orientation
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum CapsuleAxis {
    X,
    Y,
    Z,
}

impl Default for CapsuleAxis {
    fn default() -> Self {
        Self::Y
    }
}

/// Compound shape part
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CompoundShapePart {
    pub shape: ColliderShapeDef,
    #[serde(default)]
    pub offset: [f32; 3],
    #[serde(default)]
    pub rotation: [f32; 3],
}

/// Physics material definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PhysicsMaterialDef {
    #[serde(default = "default_friction")]
    pub friction: f32,
    #[serde(default)]
    pub restitution: f32,
    #[serde(default)]
    pub friction_combine: CombineMode,
    #[serde(default)]
    pub restitution_combine: CombineMode,
}

fn default_friction() -> f32 { 0.5 }

impl Default for PhysicsMaterialDef {
    fn default() -> Self {
        Self {
            friction: default_friction(),
            restitution: 0.0,
            friction_combine: CombineMode::Average,
            restitution_combine: CombineMode::Average,
        }
    }
}

/// Combine mode for physics materials
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum CombineMode {
    Average,
    Min,
    Max,
    Multiply,
}

impl Default for CombineMode {
    fn default() -> Self {
        Self::Average
    }
}

/// Collision groups definition
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct CollisionGroupsDef {
    #[serde(default)]
    pub membership: Vec<String>,
    #[serde(default)]
    pub filter: Vec<String>,
}

/// Joint definition
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum JointDef {
    Fixed {
        target: String,
        #[serde(default)]
        anchor: [f32; 3],
    },
    Revolute {
        target: String,
        axis: [f32; 3],
        #[serde(default)]
        anchor: [f32; 3],
        #[serde(default)]
        limits: Option<[f32; 2]>,
        #[serde(default)]
        motor: Option<MotorDef>,
    },
    Prismatic {
        target: String,
        axis: [f32; 3],
        #[serde(default)]
        anchor: [f32; 3],
        #[serde(default)]
        limits: Option<[f32; 2]>,
        #[serde(default)]
        motor: Option<MotorDef>,
    },
    Spherical {
        target: String,
        #[serde(default)]
        anchor: [f32; 3],
        #[serde(default)]
        swing_limit: Option<f32>,
        #[serde(default)]
        twist_limit: Option<[f32; 2]>,
    },
    Distance {
        target: String,
        #[serde(default)]
        min_distance: f32,
        #[serde(default)]
        max_distance: f32,
        #[serde(default)]
        stiffness: f32,
        #[serde(default)]
        damping: f32,
    },
    Rope {
        target: String,
        max_length: f32,
    },
}

/// Motor definition for joints
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct MotorDef {
    #[serde(default)]
    pub target_velocity: f32,
    #[serde(default)]
    pub max_force: f32,
    #[serde(default)]
    pub stiffness: f32,
    #[serde(default)]
    pub damping: f32,
}

/// Character controller definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CharacterControllerDef {
    #[serde(default = "default_char_height")]
    pub height: f32,
    #[serde(default = "default_char_radius")]
    pub radius: f32,
    #[serde(default = "default_step_height")]
    pub step_height: f32,
    #[serde(default = "default_slope_limit")]
    pub max_slope: f32,
    #[serde(default)]
    pub skin_width: f32,
    #[serde(default = "default_move_speed")]
    pub move_speed: f32,
    #[serde(default = "default_jump_velocity")]
    pub jump_velocity: f32,
    #[serde(default = "default_gravity")]
    pub gravity: f32,
}

fn default_char_height() -> f32 { 1.8 }
fn default_char_radius() -> f32 { 0.4 }
fn default_step_height() -> f32 { 0.3 }
fn default_slope_limit() -> f32 { 45.0 }
fn default_move_speed() -> f32 { 5.0 }
fn default_jump_velocity() -> f32 { 8.0 }
fn default_gravity() -> f32 { 20.0 }

impl Default for CharacterControllerDef {
    fn default() -> Self {
        Self {
            height: default_char_height(),
            radius: default_char_radius(),
            step_height: default_step_height(),
            max_slope: default_slope_limit(),
            skin_width: 0.01,
            move_speed: default_move_speed(),
            jump_velocity: default_jump_velocity(),
            gravity: default_gravity(),
        }
    }
}

// ============================================================================
// Combat System (doc 24)
// ============================================================================

/// Health component definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct HealthDef {
    #[serde(default = "default_max_health")]
    pub max_health: f32,
    #[serde(default)]
    pub current_health: Option<f32>,
    #[serde(default)]
    pub regeneration: f32,
    #[serde(default)]
    pub regen_delay: f32,
    #[serde(default)]
    pub invulnerable: bool,
    #[serde(default)]
    pub damage_multipliers: HashMap<String, f32>,
    #[serde(default)]
    pub on_damage: Option<String>,
    #[serde(default)]
    pub on_death: Option<String>,
    #[serde(default)]
    pub on_heal: Option<String>,
}

fn default_max_health() -> f32 { 100.0 }

impl Default for HealthDef {
    fn default() -> Self {
        Self {
            max_health: default_max_health(),
            current_health: None,
            regeneration: 0.0,
            regen_delay: 3.0,
            invulnerable: false,
            damage_multipliers: HashMap::new(),
            on_damage: None,
            on_death: None,
            on_heal: None,
        }
    }
}

/// Weapon definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct WeaponDef {
    pub name: String,
    #[serde(default)]
    pub weapon_type: WeaponType,
    #[serde(default = "default_damage")]
    pub damage: f32,
    #[serde(default)]
    pub damage_type: String,
    #[serde(default = "default_fire_rate")]
    pub fire_rate: f32,
    #[serde(default)]
    pub automatic: bool,
    #[serde(default)]
    pub hitscan: Option<HitscanDef>,
    #[serde(default)]
    pub projectile: Option<ProjectileDef>,
    #[serde(default)]
    pub melee: Option<MeleeDef>,
    #[serde(default)]
    pub ammo: Option<AmmoDef>,
    #[serde(default)]
    pub spread: f32,
    #[serde(default)]
    pub recoil: Option<RecoilDef>,
    #[serde(default)]
    pub sounds: WeaponSoundsDef,
    #[serde(default)]
    pub effects: WeaponEffectsDef,
}

fn default_damage() -> f32 { 10.0 }
fn default_fire_rate() -> f32 { 1.0 }

impl Default for WeaponDef {
    fn default() -> Self {
        Self {
            name: "Weapon".to_string(),
            weapon_type: WeaponType::Hitscan,
            damage: default_damage(),
            damage_type: "physical".to_string(),
            fire_rate: default_fire_rate(),
            automatic: false,
            hitscan: None,
            projectile: None,
            melee: None,
            ammo: None,
            spread: 0.0,
            recoil: None,
            sounds: WeaponSoundsDef::default(),
            effects: WeaponEffectsDef::default(),
        }
    }
}

/// Weapon type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum WeaponType {
    Hitscan,
    Projectile,
    Melee,
}

impl Default for WeaponType {
    fn default() -> Self {
        Self::Hitscan
    }
}

/// Hitscan weapon configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct HitscanDef {
    #[serde(default = "default_range")]
    pub range: f32,
    #[serde(default = "default_one")]
    pub pellets: u32,
    #[serde(default)]
    pub penetration: f32,
    #[serde(default)]
    pub falloff: Option<FalloffDef>,
}

fn default_range() -> f32 { 100.0 }

impl Default for HitscanDef {
    fn default() -> Self {
        Self {
            range: default_range(),
            pellets: 1,
            penetration: 0.0,
            falloff: None,
        }
    }
}

/// Damage falloff configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct FalloffDef {
    pub start_distance: f32,
    pub end_distance: f32,
    #[serde(default = "default_min_damage_mult")]
    pub min_damage_mult: f32,
}

fn default_min_damage_mult() -> f32 { 0.5 }

/// Projectile weapon configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ProjectileDef {
    pub prefab: String,
    #[serde(default = "default_projectile_speed")]
    pub speed: f32,
    #[serde(default)]
    pub gravity: f32,
    #[serde(default = "default_projectile_lifetime")]
    pub lifetime: f32,
    #[serde(default)]
    pub homing: Option<HomingDef>,
    #[serde(default)]
    pub explosion: Option<ExplosionDef>,
}

fn default_projectile_speed() -> f32 { 50.0 }
fn default_projectile_lifetime() -> f32 { 5.0 }

impl Default for ProjectileDef {
    fn default() -> Self {
        Self {
            prefab: "projectile".to_string(),
            speed: default_projectile_speed(),
            gravity: 0.0,
            lifetime: default_projectile_lifetime(),
            homing: None,
            explosion: None,
        }
    }
}

/// Homing projectile configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct HomingDef {
    #[serde(default = "default_turn_rate")]
    pub turn_rate: f32,
    #[serde(default = "default_lock_range")]
    pub lock_range: f32,
    #[serde(default = "default_lock_angle")]
    pub lock_angle: f32,
}

fn default_turn_rate() -> f32 { 90.0 }
fn default_lock_range() -> f32 { 50.0 }
fn default_lock_angle() -> f32 { 30.0 }

/// Explosion configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ExplosionDef {
    #[serde(default = "default_explosion_radius")]
    pub radius: f32,
    #[serde(default = "default_explosion_damage")]
    pub damage: f32,
    #[serde(default)]
    pub falloff: bool,
    #[serde(default)]
    pub force: f32,
    #[serde(default)]
    pub effect: Option<String>,
    #[serde(default)]
    pub sound: Option<String>,
}

fn default_explosion_radius() -> f32 { 5.0 }
fn default_explosion_damage() -> f32 { 50.0 }

impl Default for ExplosionDef {
    fn default() -> Self {
        Self {
            radius: default_explosion_radius(),
            damage: default_explosion_damage(),
            falloff: true,
            force: 500.0,
            effect: None,
            sound: None,
        }
    }
}

/// Melee weapon configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct MeleeDef {
    #[serde(default = "default_melee_range")]
    pub range: f32,
    #[serde(default = "default_melee_angle")]
    pub angle: f32,
    #[serde(default)]
    pub combo: Vec<ComboAttackDef>,
    #[serde(default)]
    pub hit_multiple: bool,
}

fn default_melee_range() -> f32 { 2.0 }
fn default_melee_angle() -> f32 { 60.0 }

impl Default for MeleeDef {
    fn default() -> Self {
        Self {
            range: default_melee_range(),
            angle: default_melee_angle(),
            combo: Vec::new(),
            hit_multiple: false,
        }
    }
}

/// Combo attack definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ComboAttackDef {
    pub animation: String,
    #[serde(default = "default_damage")]
    pub damage: f32,
    #[serde(default)]
    pub damage_multiplier: f32,
    #[serde(default)]
    pub hit_time: f32,
    #[serde(default)]
    pub recovery_time: f32,
}

/// Ammo configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AmmoDef {
    pub ammo_type: String,
    #[serde(default = "default_mag_size")]
    pub magazine_size: u32,
    #[serde(default = "default_reload_time")]
    pub reload_time: f32,
    #[serde(default)]
    pub reserve_max: Option<u32>,
    #[serde(default)]
    pub chambered: bool,
}

fn default_mag_size() -> u32 { 30 }
fn default_reload_time() -> f32 { 2.0 }

/// Recoil configuration
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct RecoilDef {
    #[serde(default)]
    pub vertical: f32,
    #[serde(default)]
    pub horizontal: f32,
    #[serde(default)]
    pub recovery_speed: f32,
    #[serde(default)]
    pub pattern: Vec<[f32; 2]>,
}

/// Weapon sounds
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct WeaponSoundsDef {
    #[serde(default)]
    pub fire: Option<String>,
    #[serde(default)]
    pub reload: Option<String>,
    #[serde(default)]
    pub empty: Option<String>,
    #[serde(default)]
    pub equip: Option<String>,
}

/// Weapon effects
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct WeaponEffectsDef {
    #[serde(default)]
    pub muzzle_flash: Option<String>,
    #[serde(default)]
    pub impact: Option<String>,
    #[serde(default)]
    pub tracer: Option<String>,
    #[serde(default)]
    pub shell_eject: Option<String>,
}

/// Status effect definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct StatusEffectDef {
    pub name: String,
    #[serde(default)]
    pub duration: f32,
    #[serde(default)]
    pub stacks: bool,
    #[serde(default = "default_max_stacks")]
    pub max_stacks: u32,
    #[serde(default)]
    pub tick_rate: f32,
    #[serde(default)]
    pub effects: Vec<StatusEffectType>,
    #[serde(default)]
    pub on_apply: Option<String>,
    #[serde(default)]
    pub on_tick: Option<String>,
    #[serde(default)]
    pub on_expire: Option<String>,
}

fn default_max_stacks() -> u32 { 1 }

/// Status effect type
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum StatusEffectType {
    DamageOverTime { damage: f32, damage_type: String },
    HealOverTime { amount: f32 },
    SpeedModifier { multiplier: f32 },
    DamageModifier { multiplier: f32 },
    Stun,
    Root,
    Silence,
    Invulnerable,
    Shield { amount: f32 },
}

// ============================================================================
// Inventory System (doc 25)
// ============================================================================

/// Item definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ItemDef {
    pub id: String,
    pub name: String,
    #[serde(default)]
    pub description: String,
    #[serde(default)]
    pub icon: Option<String>,
    #[serde(default)]
    pub mesh: Option<String>,
    #[serde(default)]
    pub item_type: ItemType,
    #[serde(default = "default_max_stack")]
    pub max_stack: u32,
    #[serde(default)]
    pub weight: f32,
    #[serde(default)]
    pub value: u32,
    #[serde(default)]
    pub rarity: ItemRarity,
    #[serde(default)]
    pub consumable: Option<ConsumableDef>,
    #[serde(default)]
    pub equipment: Option<EquipmentDef>,
    #[serde(default)]
    pub weapon: Option<WeaponDef>,
    #[serde(default)]
    pub usable: Option<UsableDef>,
    #[serde(default)]
    pub tags: Vec<String>,
}

fn default_max_stack() -> u32 { 99 }

impl Default for ItemDef {
    fn default() -> Self {
        Self {
            id: "item".to_string(),
            name: "Item".to_string(),
            description: String::new(),
            icon: None,
            mesh: None,
            item_type: ItemType::Misc,
            max_stack: default_max_stack(),
            weight: 0.0,
            value: 0,
            rarity: ItemRarity::Common,
            consumable: None,
            equipment: None,
            weapon: None,
            usable: None,
            tags: Vec::new(),
        }
    }
}

/// Item type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum ItemType {
    Weapon,
    Armor,
    Consumable,
    Key,
    Quest,
    Ammo,
    Material,
    Misc,
}

impl Default for ItemType {
    fn default() -> Self {
        Self::Misc
    }
}

/// Item rarity
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum ItemRarity {
    Common,
    Uncommon,
    Rare,
    Epic,
    Legendary,
}

impl Default for ItemRarity {
    fn default() -> Self {
        Self::Common
    }
}

/// Consumable item properties
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ConsumableDef {
    #[serde(default)]
    pub heal_amount: f32,
    #[serde(default)]
    pub restore_stamina: f32,
    #[serde(default)]
    pub restore_mana: f32,
    #[serde(default)]
    pub apply_effects: Vec<String>,
    #[serde(default)]
    pub use_time: f32,
    #[serde(default)]
    pub cooldown: f32,
    #[serde(default)]
    pub destroy_on_use: bool,
}

impl Default for ConsumableDef {
    fn default() -> Self {
        Self {
            heal_amount: 0.0,
            restore_stamina: 0.0,
            restore_mana: 0.0,
            apply_effects: Vec::new(),
            use_time: 0.0,
            cooldown: 0.0,
            destroy_on_use: true,
        }
    }
}

/// Equipment item properties
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct EquipmentDef {
    pub slot: EquipmentSlot,
    #[serde(default)]
    pub armor: f32,
    #[serde(default)]
    pub stat_bonuses: HashMap<String, f32>,
    #[serde(default)]
    pub required_level: u32,
    #[serde(default)]
    pub required_class: Option<String>,
    #[serde(default)]
    pub set_bonus: Option<String>,
}

/// Equipment slot
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum EquipmentSlot {
    Head,
    Chest,
    Legs,
    Feet,
    Hands,
    MainHand,
    OffHand,
    Ring,
    Amulet,
    Back,
}

impl Default for EquipmentSlot {
    fn default() -> Self {
        Self::MainHand
    }
}

/// Usable item properties
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct UsableDef {
    #[serde(default)]
    pub on_use: Option<String>,
    #[serde(default)]
    pub use_range: f32,
    #[serde(default)]
    pub use_time: f32,
    #[serde(default)]
    pub charges: Option<u32>,
    #[serde(default)]
    pub cooldown: f32,
}

/// Inventory component definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct InventoryDef {
    #[serde(default = "default_inventory_slots")]
    pub slots: u32,
    #[serde(default)]
    pub max_weight: Option<f32>,
    #[serde(default)]
    pub allowed_types: Vec<ItemType>,
    #[serde(default)]
    pub starting_items: Vec<StartingItemDef>,
    #[serde(default)]
    pub equipment_slots: Vec<EquipmentSlot>,
}

fn default_inventory_slots() -> u32 { 20 }

impl Default for InventoryDef {
    fn default() -> Self {
        Self {
            slots: default_inventory_slots(),
            max_weight: None,
            allowed_types: Vec::new(),
            starting_items: Vec::new(),
            equipment_slots: Vec::new(),
        }
    }
}

/// Starting item definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct StartingItemDef {
    pub item: String,
    #[serde(default = "default_one")]
    pub count: u32,
    #[serde(default)]
    pub equipped: bool,
}

/// Pickup item definition (world item)
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PickupDef {
    pub item: String,
    #[serde(default = "default_one")]
    pub count: u32,
    #[serde(default)]
    pub respawn: bool,
    #[serde(default)]
    pub respawn_time: f32,
    #[serde(default)]
    pub interaction_prompt: Option<String>,
    #[serde(default = "default_pickup_range")]
    pub pickup_range: f32,
    #[serde(default)]
    pub auto_pickup: bool,
}

fn default_pickup_range() -> f32 { 2.0 }

impl Default for PickupDef {
    fn default() -> Self {
        Self {
            item: String::new(),
            count: 1,
            respawn: false,
            respawn_time: 60.0,
            interaction_prompt: None,
            pickup_range: default_pickup_range(),
            auto_pickup: false,
        }
    }
}

/// Loot table definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct LootTableDef {
    pub id: String,
    pub entries: Vec<LootEntryDef>,
    #[serde(default = "default_loot_rolls")]
    pub rolls: u32,
    #[serde(default)]
    pub guaranteed: Vec<String>,
}

fn default_loot_rolls() -> u32 { 1 }

/// Loot entry
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct LootEntryDef {
    pub item: String,
    #[serde(default = "default_loot_weight")]
    pub weight: f32,
    #[serde(default = "default_one")]
    pub min_count: u32,
    #[serde(default = "default_one")]
    pub max_count: u32,
}

fn default_loot_weight() -> f32 { 1.0 }

/// Vendor/shop definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct VendorDef {
    pub name: String,
    pub items: Vec<VendorItemDef>,
    #[serde(default = "default_buy_multiplier")]
    pub buy_multiplier: f32,
    #[serde(default = "default_sell_multiplier")]
    pub sell_multiplier: f32,
    #[serde(default)]
    pub restock_time: Option<f32>,
    #[serde(default)]
    pub currency: String,
}

fn default_buy_multiplier() -> f32 { 1.0 }
fn default_sell_multiplier() -> f32 { 0.5 }

/// Vendor item
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct VendorItemDef {
    pub item: String,
    #[serde(default)]
    pub price_override: Option<u32>,
    #[serde(default)]
    pub stock: Option<u32>,
    #[serde(default)]
    pub required_reputation: Option<i32>,
}

// ============================================================================
// Audio System (doc 26)
// ============================================================================

/// Audio configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AudioConfigDef {
    #[serde(default)]
    pub ambient: Vec<AmbientSoundDef>,
    #[serde(default)]
    pub music: Option<MusicDef>,
    #[serde(default)]
    pub reverb_zones: Vec<ReverbZoneDef>,
    #[serde(default)]
    pub sound_banks: Vec<String>,
}

impl Default for AudioConfigDef {
    fn default() -> Self {
        Self {
            ambient: Vec::new(),
            music: None,
            reverb_zones: Vec::new(),
            sound_banks: Vec::new(),
        }
    }
}

/// Ambient sound definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AmbientSoundDef {
    pub sound: String,
    #[serde(default = "default_volume")]
    pub volume: f32,
    #[serde(default = "default_true")]
    pub loop_sound: bool,
    #[serde(default)]
    pub position: Option<[f32; 3]>,
    #[serde(default)]
    pub radius: Option<f32>,
    #[serde(default)]
    pub falloff: Option<f32>,
    #[serde(default)]
    pub time_of_day: Option<[f32; 2]>,
}

fn default_volume() -> f32 { 1.0 }

/// Music configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct MusicDef {
    pub tracks: Vec<MusicTrackDef>,
    #[serde(default)]
    pub shuffle: bool,
    #[serde(default = "default_crossfade")]
    pub crossfade: f32,
    #[serde(default)]
    pub combat_music: Option<String>,
    #[serde(default)]
    pub exploration_music: Option<String>,
}

fn default_crossfade() -> f32 { 2.0 }

/// Music track
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct MusicTrackDef {
    pub path: String,
    #[serde(default = "default_volume")]
    pub volume: f32,
    #[serde(default)]
    pub intro: Option<String>,
    #[serde(default)]
    pub loop_start: Option<f32>,
}

/// Reverb zone definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ReverbZoneDef {
    pub position: [f32; 3],
    pub size: [f32; 3],
    pub preset: ReverbPreset,
    #[serde(default = "default_reverb_blend")]
    pub blend: f32,
}

fn default_reverb_blend() -> f32 { 1.0 }

/// Reverb preset
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum ReverbPreset {
    None,
    Room,
    Hall,
    Cave,
    Outdoor,
    Underwater,
    Custom,
}

impl Default for ReverbPreset {
    fn default() -> Self {
        Self::None
    }
}

/// Entity audio component
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct EntityAudioDef {
    #[serde(default)]
    pub footsteps: Option<FootstepsDef>,
    #[serde(default)]
    pub voice: Option<VoiceDef>,
    #[serde(default)]
    pub sounds: HashMap<String, String>,
}

/// Footsteps configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct FootstepsDef {
    #[serde(default)]
    pub surfaces: HashMap<String, Vec<String>>,
    #[serde(default = "default_footstep_volume")]
    pub volume: f32,
    #[serde(default)]
    pub pitch_variation: f32,
}

fn default_footstep_volume() -> f32 { 0.5 }

impl Default for FootstepsDef {
    fn default() -> Self {
        Self {
            surfaces: HashMap::new(),
            volume: default_footstep_volume(),
            pitch_variation: 0.1,
        }
    }
}

/// Voice/dialogue audio
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct VoiceDef {
    #[serde(default)]
    pub lines: HashMap<String, String>,
    #[serde(default)]
    pub barks: Vec<String>,
    #[serde(default)]
    pub death_sounds: Vec<String>,
    #[serde(default)]
    pub hurt_sounds: Vec<String>,
}

impl Default for VoiceDef {
    fn default() -> Self {
        Self {
            lines: HashMap::new(),
            barks: Vec::new(),
            death_sounds: Vec::new(),
            hurt_sounds: Vec::new(),
        }
    }
}

// ============================================================================
// State System (doc 27)
// ============================================================================

/// State configuration
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct StateConfigDef {
    #[serde(default)]
    pub variables: HashMap<String, StateVariableDef>,
    #[serde(default)]
    pub save: Option<SaveConfigDef>,
    #[serde(default)]
    pub checkpoints: Vec<CheckpointDef>,
}

/// State variable definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct StateVariableDef {
    #[serde(rename = "type")]
    pub var_type: StateVarType,
    pub default: ScriptValue,
    #[serde(default)]
    pub persistent: bool,
    #[serde(default)]
    pub sync_network: bool,
    #[serde(default)]
    pub replicate: bool,
    #[serde(default)]
    pub on_change: Option<String>,
}

/// State variable type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum StateVarType {
    Bool,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Entity,
    Array,
    Map,
}

impl Default for StateVarType {
    fn default() -> Self {
        Self::Int
    }
}

/// Save configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct SaveConfigDef {
    #[serde(default = "default_true")]
    pub auto_save: bool,
    #[serde(default = "default_auto_save_interval")]
    pub auto_save_interval: f32,
    #[serde(default = "default_max_saves")]
    pub max_saves: u32,
    #[serde(default)]
    pub save_on_quit: bool,
    #[serde(default)]
    pub save_components: Vec<String>,
    #[serde(default)]
    pub exclude_entities: Vec<String>,
}

fn default_auto_save_interval() -> f32 { 300.0 }
fn default_max_saves() -> u32 { 10 }

impl Default for SaveConfigDef {
    fn default() -> Self {
        Self {
            auto_save: true,
            auto_save_interval: default_auto_save_interval(),
            max_saves: default_max_saves(),
            save_on_quit: true,
            save_components: Vec::new(),
            exclude_entities: Vec::new(),
        }
    }
}

/// Checkpoint definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CheckpointDef {
    pub id: String,
    pub position: [f32; 3],
    #[serde(default)]
    pub rotation: [f32; 3],
    #[serde(default)]
    pub trigger: Option<String>,
    #[serde(default = "default_true")]
    pub auto_activate: bool,
}

/// Quest definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct QuestDef {
    pub id: String,
    pub name: String,
    #[serde(default)]
    pub description: String,
    pub objectives: Vec<QuestObjectiveDef>,
    #[serde(default)]
    pub rewards: Vec<QuestRewardDef>,
    #[serde(default)]
    pub prerequisites: Vec<String>,
    #[serde(default)]
    pub on_start: Option<String>,
    #[serde(default)]
    pub on_complete: Option<String>,
    #[serde(default)]
    pub repeatable: bool,
    #[serde(default)]
    pub hidden: bool,
    #[serde(default)]
    pub category: String,
}

/// Quest objective
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct QuestObjectiveDef {
    pub id: String,
    pub description: String,
    #[serde(default)]
    pub objective_type: ObjectiveType,
    #[serde(default)]
    pub target: Option<String>,
    #[serde(default = "default_one")]
    pub count: u32,
    #[serde(default)]
    pub optional: bool,
    #[serde(default)]
    pub hidden: bool,
    #[serde(default)]
    pub on_progress: Option<String>,
    #[serde(default)]
    pub on_complete: Option<String>,
}

/// Objective type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum ObjectiveType {
    Kill,
    Collect,
    Interact,
    Reach,
    Escort,
    Defend,
    Talk,
    Custom,
}

impl Default for ObjectiveType {
    fn default() -> Self {
        Self::Custom
    }
}

/// Quest reward
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum QuestRewardDef {
    Item { item: String, count: u32 },
    Currency { currency: String, amount: u32 },
    Experience { amount: u32 },
    Reputation { faction: String, amount: i32 },
    Unlock { unlock: String },
}

/// Achievement definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AchievementDef {
    pub id: String,
    pub name: String,
    #[serde(default)]
    pub description: String,
    #[serde(default)]
    pub icon: Option<String>,
    #[serde(default)]
    pub hidden: bool,
    #[serde(default)]
    pub points: u32,
    #[serde(default)]
    pub condition: Option<AchievementConditionDef>,
    #[serde(default)]
    pub rewards: Vec<QuestRewardDef>,
}

/// Achievement condition
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum AchievementConditionDef {
    KillCount { enemy_type: Option<String>, count: u32 },
    CollectItems { item: Option<String>, count: u32 },
    CompleteQuests { quests: Vec<String> },
    ReachLevel { level: u32 },
    DiscoverLocations { locations: Vec<String> },
    Variable { variable: String, value: ScriptValue },
    Custom { function: String },
}

// ============================================================================
// UI/HUD System (doc 28)
// ============================================================================

/// UI configuration
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct UiConfigDef {
    #[serde(default)]
    pub hud: Option<HudConfigDef>,
    #[serde(default)]
    pub menus: HashMap<String, MenuDef>,
    #[serde(default)]
    pub dialogs: HashMap<String, DialogDef>,
    #[serde(default)]
    pub notifications: NotificationConfigDef,
}

/// HUD configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct HudConfigDef {
    #[serde(default)]
    pub elements: Vec<HudElementDef>,
    #[serde(default)]
    pub damage_numbers: Option<DamageNumbersDef>,
    #[serde(default)]
    pub interaction_prompt: Option<InteractionPromptDef>,
    #[serde(default)]
    pub nameplates: Option<NameplatesDef>,
    #[serde(default)]
    pub crosshair: Option<CrosshairDef>,
}

impl Default for HudConfigDef {
    fn default() -> Self {
        Self {
            elements: Vec::new(),
            damage_numbers: None,
            interaction_prompt: None,
            nameplates: None,
            crosshair: None,
        }
    }
}

/// HUD element definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct HudElementDef {
    pub id: String,
    #[serde(rename = "type")]
    pub element_type: HudElementType,
    pub anchor: AnchorPoint,
    #[serde(default)]
    pub offset: [f32; 2],
    #[serde(default)]
    pub size: Option<[f32; 2]>,
    #[serde(default)]
    pub binding: Option<String>,
    #[serde(default = "default_true")]
    pub visible: bool,
    #[serde(default)]
    pub style: HudStyleDef,
}

/// HUD element type
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum HudElementType {
    ProgressBar {
        #[serde(default)]
        direction: ProgressDirection,
        #[serde(default)]
        show_text: bool,
        #[serde(default)]
        fill_color: Option<[f32; 4]>,
        #[serde(default)]
        background_color: Option<[f32; 4]>,
    },
    Text {
        #[serde(default)]
        format: Option<String>,
        #[serde(default)]
        font_size: Option<f32>,
    },
    Image {
        #[serde(default)]
        texture: Option<String>,
        #[serde(default)]
        tint: Option<[f32; 4]>,
    },
    Panel {
        #[serde(default)]
        children: Vec<HudElementDef>,
    },
    IconGrid {
        columns: u32,
        #[serde(default)]
        icon_size: f32,
        #[serde(default)]
        spacing: f32,
    },
    Minimap {
        #[serde(default = "default_minimap_size")]
        size: f32,
        #[serde(default)]
        zoom: f32,
        #[serde(default)]
        rotation: bool,
        #[serde(default)]
        icons: Vec<MinimapIconDef>,
    },
    Compass {
        #[serde(default)]
        markers: Vec<CompassMarkerDef>,
    },
    StatusIcons {
        #[serde(default)]
        icon_size: f32,
        #[serde(default)]
        max_icons: u32,
    },
}

fn default_minimap_size() -> f32 { 200.0 }

/// Progress bar direction
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum ProgressDirection {
    LeftToRight,
    RightToLeft,
    BottomToTop,
    TopToBottom,
}

impl Default for ProgressDirection {
    fn default() -> Self {
        Self::LeftToRight
    }
}

/// Anchor point for UI elements
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum AnchorPoint {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    Center,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
}

impl Default for AnchorPoint {
    fn default() -> Self {
        Self::TopLeft
    }
}

/// HUD style definition
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct HudStyleDef {
    #[serde(default)]
    pub background: Option<[f32; 4]>,
    #[serde(default)]
    pub border: Option<BorderDef>,
    #[serde(default)]
    pub padding: Option<[f32; 4]>,
    #[serde(default)]
    pub font: Option<String>,
    #[serde(default)]
    pub text_color: Option<[f32; 4]>,
    #[serde(default)]
    pub opacity: Option<f32>,
}

/// Border definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct BorderDef {
    #[serde(default = "default_border_width")]
    pub width: f32,
    #[serde(default)]
    pub color: [f32; 4],
    #[serde(default)]
    pub radius: f32,
}

fn default_border_width() -> f32 { 1.0 }

impl Default for BorderDef {
    fn default() -> Self {
        Self {
            width: default_border_width(),
            color: [1.0, 1.0, 1.0, 1.0],
            radius: 0.0,
        }
    }
}

/// Minimap icon definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct MinimapIconDef {
    pub tag: String,
    pub icon: String,
    #[serde(default)]
    pub color: Option<[f32; 4]>,
    #[serde(default)]
    pub size: Option<f32>,
}

/// Compass marker definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CompassMarkerDef {
    pub tag: String,
    pub icon: String,
    #[serde(default)]
    pub label: Option<String>,
}

/// Damage numbers configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DamageNumbersDef {
    #[serde(default = "default_true")]
    pub enabled: bool,
    #[serde(default = "default_font_size_damage")]
    pub font_size: f32,
    #[serde(default)]
    pub normal_color: [f32; 4],
    #[serde(default)]
    pub critical_color: [f32; 4],
    #[serde(default)]
    pub heal_color: [f32; 4],
    #[serde(default = "default_damage_float_speed")]
    pub float_speed: f32,
    #[serde(default = "default_damage_lifetime")]
    pub lifetime: f32,
}

fn default_font_size_damage() -> f32 { 24.0 }
fn default_damage_float_speed() -> f32 { 50.0 }
fn default_damage_lifetime() -> f32 { 1.0 }

impl Default for DamageNumbersDef {
    fn default() -> Self {
        Self {
            enabled: true,
            font_size: default_font_size_damage(),
            normal_color: [1.0, 1.0, 1.0, 1.0],
            critical_color: [1.0, 0.8, 0.0, 1.0],
            heal_color: [0.0, 1.0, 0.0, 1.0],
            float_speed: default_damage_float_speed(),
            lifetime: default_damage_lifetime(),
        }
    }
}

/// Interaction prompt configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct InteractionPromptDef {
    #[serde(default)]
    pub offset: [f32; 2],
    #[serde(default)]
    pub show_key: bool,
    #[serde(default)]
    pub style: HudStyleDef,
}

impl Default for InteractionPromptDef {
    fn default() -> Self {
        Self {
            offset: [0.0, -50.0],
            show_key: true,
            style: HudStyleDef::default(),
        }
    }
}

/// Nameplates configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct NameplatesDef {
    #[serde(default = "default_true")]
    pub enabled: bool,
    #[serde(default = "default_nameplate_distance")]
    pub max_distance: f32,
    #[serde(default)]
    pub show_health: bool,
    #[serde(default)]
    pub show_level: bool,
    #[serde(default)]
    pub friendly_color: [f32; 4],
    #[serde(default)]
    pub enemy_color: [f32; 4],
    #[serde(default)]
    pub neutral_color: [f32; 4],
}

fn default_nameplate_distance() -> f32 { 30.0 }

impl Default for NameplatesDef {
    fn default() -> Self {
        Self {
            enabled: true,
            max_distance: default_nameplate_distance(),
            show_health: true,
            show_level: false,
            friendly_color: [0.0, 1.0, 0.0, 1.0],
            enemy_color: [1.0, 0.0, 0.0, 1.0],
            neutral_color: [1.0, 1.0, 0.0, 1.0],
        }
    }
}

/// Crosshair configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CrosshairDef {
    #[serde(default)]
    pub texture: Option<String>,
    #[serde(default = "default_crosshair_size")]
    pub size: f32,
    #[serde(default)]
    pub color: [f32; 4],
    #[serde(default)]
    pub dynamic: bool,
    #[serde(default)]
    pub hit_marker: Option<HitMarkerDef>,
}

fn default_crosshair_size() -> f32 { 32.0 }

impl Default for CrosshairDef {
    fn default() -> Self {
        Self {
            texture: None,
            size: default_crosshair_size(),
            color: [1.0, 1.0, 1.0, 1.0],
            dynamic: false,
            hit_marker: None,
        }
    }
}

/// Hit marker configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct HitMarkerDef {
    #[serde(default)]
    pub texture: Option<String>,
    #[serde(default)]
    pub color: [f32; 4],
    #[serde(default = "default_hitmarker_duration")]
    pub duration: f32,
}

fn default_hitmarker_duration() -> f32 { 0.2 }

/// Menu definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct MenuDef {
    pub layout: MenuLayoutDef,
    #[serde(default)]
    pub items: Vec<MenuItemDef>,
    #[serde(default)]
    pub style: HudStyleDef,
    #[serde(default)]
    pub on_open: Option<String>,
    #[serde(default)]
    pub on_close: Option<String>,
    #[serde(default)]
    pub pause_game: bool,
}

/// Menu layout
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum MenuLayoutDef {
    Vertical,
    Horizontal,
    Grid { columns: u32 },
    Custom { template: String },
}

impl Default for MenuLayoutDef {
    fn default() -> Self {
        Self::Vertical
    }
}

/// Menu item definition
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum MenuItemDef {
    Button {
        label: String,
        #[serde(default)]
        action: Option<String>,
        #[serde(default)]
        enabled: Option<String>,
    },
    Slider {
        label: String,
        binding: String,
        min: f32,
        max: f32,
        #[serde(default)]
        step: Option<f32>,
    },
    Toggle {
        label: String,
        binding: String,
    },
    Dropdown {
        label: String,
        binding: String,
        options: Vec<String>,
    },
    Submenu {
        label: String,
        menu: String,
    },
    Separator,
    Label {
        text: String,
    },
}

/// Dialog definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DialogDef {
    pub title: String,
    #[serde(default)]
    pub message: Option<String>,
    #[serde(default)]
    pub portrait: Option<String>,
    #[serde(default)]
    pub speaker: Option<String>,
    #[serde(default)]
    pub choices: Vec<DialogChoiceDef>,
    #[serde(default)]
    pub on_show: Option<String>,
    #[serde(default)]
    pub auto_advance: Option<f32>,
    #[serde(default)]
    pub style: HudStyleDef,
}

/// Dialog choice
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DialogChoiceDef {
    pub text: String,
    #[serde(default)]
    pub action: Option<String>,
    #[serde(default)]
    pub next_dialog: Option<String>,
    #[serde(default)]
    pub condition: Option<String>,
    #[serde(default)]
    pub disabled_text: Option<String>,
}

/// Notification configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct NotificationConfigDef {
    pub anchor: AnchorPoint,
    #[serde(default)]
    pub offset: [f32; 2],
    #[serde(default = "default_max_notifications")]
    pub max_visible: u32,
    #[serde(default = "default_notification_duration")]
    pub default_duration: f32,
    #[serde(default)]
    pub style: HudStyleDef,
}

fn default_max_notifications() -> u32 { 5 }
fn default_notification_duration() -> f32 { 3.0 }

impl Default for NotificationConfigDef {
    fn default() -> Self {
        Self {
            anchor: AnchorPoint::TopRight,
            offset: [-20.0, 20.0],
            max_visible: default_max_notifications(),
            default_duration: default_notification_duration(),
            style: HudStyleDef::default(),
        }
    }
}

// ============================================================================
// AI/Navigation System (doc 29)
// ============================================================================

/// Navigation configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct NavigationConfigDef {
    #[serde(default)]
    pub navmesh: Option<NavMeshConfigDef>,
    #[serde(default)]
    pub waypoint_paths: Vec<WaypointPathDef>,
    #[serde(default)]
    pub cover_points: Vec<CoverPointDef>,
    #[serde(default)]
    pub ai_groups: Vec<AiGroupDef>,
}

impl Default for NavigationConfigDef {
    fn default() -> Self {
        Self {
            navmesh: None,
            waypoint_paths: Vec::new(),
            cover_points: Vec::new(),
            ai_groups: Vec::new(),
        }
    }
}

/// NavMesh configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct NavMeshConfigDef {
    #[serde(default)]
    pub source: NavMeshSource,
    #[serde(default)]
    pub agent_radius: f32,
    #[serde(default)]
    pub agent_height: f32,
    #[serde(default)]
    pub max_slope: f32,
    #[serde(default)]
    pub step_height: f32,
    #[serde(default)]
    pub cell_size: f32,
    #[serde(default)]
    pub cell_height: f32,
    #[serde(default)]
    pub areas: Vec<NavAreaDef>,
}

impl Default for NavMeshConfigDef {
    fn default() -> Self {
        Self {
            source: NavMeshSource::Generate,
            agent_radius: 0.4,
            agent_height: 1.8,
            max_slope: 45.0,
            step_height: 0.3,
            cell_size: 0.3,
            cell_height: 0.2,
            areas: Vec::new(),
        }
    }
}

/// NavMesh source
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum NavMeshSource {
    Generate,
    File { path: String },
}

impl Default for NavMeshSource {
    fn default() -> Self {
        Self::Generate
    }
}

/// Navigation area definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct NavAreaDef {
    pub name: String,
    #[serde(default = "default_nav_cost")]
    pub cost: f32,
    #[serde(default)]
    pub flags: Vec<String>,
}

fn default_nav_cost() -> f32 { 1.0 }

/// AI component definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiComponentDef {
    #[serde(flatten)]
    pub ai_type: AiTypeDef,
    #[serde(default)]
    pub senses: AiSensesDef,
    #[serde(default)]
    pub movement: AiMovementDef,
    #[serde(default)]
    pub faction: Option<String>,
    #[serde(default)]
    pub aggro_range: Option<f32>,
    #[serde(default)]
    pub leash_range: Option<f32>,
}

/// AI type - state machine, behavior tree, or utility
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum AiTypeDef {
    StateMachine {
        initial_state: String,
        states: Vec<AiStateDef>,
    },
    BehaviorTree {
        tree: String,
        #[serde(default)]
        blackboard: HashMap<String, ScriptValue>,
    },
    Utility {
        considerations: Vec<AiConsiderationDef>,
        #[serde(default = "default_decision_interval")]
        decision_interval: f32,
    },
}

fn default_decision_interval() -> f32 { 0.5 }

/// AI state definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiStateDef {
    pub name: String,
    #[serde(default)]
    pub on_enter: Option<String>,
    #[serde(default)]
    pub on_update: Option<String>,
    #[serde(default)]
    pub on_exit: Option<String>,
    #[serde(default)]
    pub transitions: Vec<AiTransitionDef>,
    #[serde(default)]
    pub actions: Vec<AiActionDef>,
}

/// AI transition definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiTransitionDef {
    pub to: String,
    pub condition: AiConditionDef,
    #[serde(default)]
    pub priority: i32,
}

/// AI condition definition
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum AiConditionDef {
    CanSeePlayer,
    CanHearPlayer,
    HealthBelow { percent: f32 },
    HealthAbove { percent: f32 },
    DistanceToPlayer { less_than: Option<f32>, greater_than: Option<f32> },
    HasTarget,
    TargetDead,
    TimeSinceLastSaw { seconds: f32 },
    Variable { name: String, equals: ScriptValue },
    And { conditions: Vec<AiConditionDef> },
    Or { conditions: Vec<AiConditionDef> },
    Not { condition: Box<AiConditionDef> },
    Custom { function: String },
}

/// AI action definition
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum AiActionDef {
    MoveTo { target: AiTargetDef },
    Attack { target: AiTargetDef },
    Patrol { path: String },
    Wait { duration: f32 },
    PlayAnimation { animation: String },
    PlaySound { sound: String },
    Flee { from: AiTargetDef },
    TakeCover,
    Investigate { location: AiTargetDef },
    Alert { radius: f32 },
    CallFunction { function: String, args: Vec<ScriptValue> },
}

/// AI target definition
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum AiTargetDef {
    Player,
    Entity { name: String },
    Position { position: [f32; 3] },
    LastKnownPosition,
    Random { radius: f32 },
    Waypoint { path: String, index: u32 },
}

/// AI consideration for utility AI
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiConsiderationDef {
    pub name: String,
    pub action: AiActionDef,
    pub evaluators: Vec<AiEvaluatorDef>,
    #[serde(default = "default_one_f32")]
    pub weight: f32,
}

/// AI evaluator for utility AI
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum AiEvaluatorDef {
    DistanceToTarget {
        target: AiTargetDef,
        curve: UtilityCurve,
    },
    Health {
        curve: UtilityCurve,
    },
    Ammo {
        curve: UtilityCurve,
    },
    ThreatLevel {
        curve: UtilityCurve,
    },
    TimeSinceAction {
        action: String,
        curve: UtilityCurve,
    },
    Variable {
        name: String,
        curve: UtilityCurve,
    },
    Custom {
        function: String,
    },
}

/// Utility curve for AI scoring
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum UtilityCurve {
    Linear { min: f32, max: f32, invert: bool },
    Quadratic { min: f32, max: f32, exponent: f32 },
    Logistic { midpoint: f32, steepness: f32 },
    Step { threshold: f32, below: f32, above: f32 },
}

/// AI senses configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiSensesDef {
    #[serde(default)]
    pub sight: Option<AiSightDef>,
    #[serde(default)]
    pub hearing: Option<AiHearingDef>,
    #[serde(default)]
    pub damage: Option<AiDamageSenseDef>,
    #[serde(default)]
    pub touch: Option<AiTouchSenseDef>,
}

impl Default for AiSensesDef {
    fn default() -> Self {
        Self {
            sight: Some(AiSightDef::default()),
            hearing: Some(AiHearingDef::default()),
            damage: Some(AiDamageSenseDef::default()),
            touch: None,
        }
    }
}

/// AI sight sense
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiSightDef {
    #[serde(default = "default_sight_range")]
    pub range: f32,
    #[serde(default = "default_sight_angle")]
    pub angle: f32,
    #[serde(default)]
    pub eye_offset: [f32; 3],
    #[serde(default)]
    pub peripheral_range: Option<f32>,
    #[serde(default)]
    pub peripheral_angle: Option<f32>,
    #[serde(default = "default_true")]
    pub requires_los: bool,
    #[serde(default = "default_recognition_time")]
    pub recognition_time: f32,
    #[serde(default = "default_lose_sight_time")]
    pub lose_sight_time: f32,
}

fn default_sight_range() -> f32 { 30.0 }
fn default_sight_angle() -> f32 { 120.0 }
fn default_recognition_time() -> f32 { 0.2 }
fn default_lose_sight_time() -> f32 { 2.0 }

impl Default for AiSightDef {
    fn default() -> Self {
        Self {
            range: default_sight_range(),
            angle: default_sight_angle(),
            eye_offset: [0.0, 1.6, 0.0],
            peripheral_range: None,
            peripheral_angle: None,
            requires_los: true,
            recognition_time: default_recognition_time(),
            lose_sight_time: default_lose_sight_time(),
        }
    }
}

/// AI hearing sense
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiHearingDef {
    #[serde(default = "default_hearing_range")]
    pub range: f32,
    #[serde(default = "default_hearing_threshold")]
    pub threshold: f32,
    #[serde(default)]
    pub sound_types: Vec<String>,
}

fn default_hearing_range() -> f32 { 20.0 }
fn default_hearing_threshold() -> f32 { 0.5 }

impl Default for AiHearingDef {
    fn default() -> Self {
        Self {
            range: default_hearing_range(),
            threshold: default_hearing_threshold(),
            sound_types: Vec::new(),
        }
    }
}

/// AI damage sense
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiDamageSenseDef {
    #[serde(default = "default_true")]
    pub detect_attacker: bool,
    #[serde(default = "default_damage_memory")]
    pub memory_time: f32,
}

fn default_damage_memory() -> f32 { 10.0 }

impl Default for AiDamageSenseDef {
    fn default() -> Self {
        Self {
            detect_attacker: true,
            memory_time: default_damage_memory(),
        }
    }
}

/// AI touch sense
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiTouchSenseDef {
    #[serde(default = "default_touch_radius")]
    pub radius: f32,
}

fn default_touch_radius() -> f32 { 1.0 }

/// AI movement configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiMovementDef {
    #[serde(default = "default_walk_speed")]
    pub walk_speed: f32,
    #[serde(default = "default_run_speed")]
    pub run_speed: f32,
    #[serde(default = "default_turn_speed")]
    pub turn_speed: f32,
    #[serde(default = "default_acceleration")]
    pub acceleration: f32,
    #[serde(default)]
    pub can_jump: bool,
    #[serde(default)]
    pub can_strafe: bool,
    #[serde(default)]
    pub avoidance_radius: f32,
}

fn default_walk_speed() -> f32 { 2.0 }
fn default_run_speed() -> f32 { 5.0 }
fn default_turn_speed() -> f32 { 360.0 }
fn default_acceleration() -> f32 { 10.0 }

impl Default for AiMovementDef {
    fn default() -> Self {
        Self {
            walk_speed: default_walk_speed(),
            run_speed: default_run_speed(),
            turn_speed: default_turn_speed(),
            acceleration: default_acceleration(),
            can_jump: false,
            can_strafe: true,
            avoidance_radius: 0.5,
        }
    }
}

/// Waypoint path definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct WaypointPathDef {
    pub name: String,
    pub points: Vec<WaypointDef>,
    #[serde(default = "default_true")]
    pub loop_path: bool,
    #[serde(default)]
    pub bidirectional: bool,
}

/// Waypoint definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct WaypointDef {
    pub position: [f32; 3],
    #[serde(default)]
    pub wait_time: f32,
    #[serde(default)]
    pub action: Option<String>,
    #[serde(default)]
    pub radius: f32,
}

/// Cover point definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CoverPointDef {
    pub position: [f32; 3],
    pub facing: [f32; 3],
    #[serde(default)]
    pub cover_type: CoverType,
    #[serde(default)]
    pub crouch: bool,
}

/// Cover type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum CoverType {
    Full,
    Half,
    Lean,
}

impl Default for CoverType {
    fn default() -> Self {
        Self::Full
    }
}

/// AI group definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AiGroupDef {
    pub name: String,
    #[serde(default)]
    pub members: Vec<String>,
    #[serde(default)]
    pub leader: Option<String>,
    #[serde(default)]
    pub behavior: GroupBehavior,
    #[serde(default)]
    pub formation: Option<FormationDef>,
}

/// Group behavior
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum GroupBehavior {
    Individual,
    FollowLeader,
    Coordinated,
    Swarm,
}

impl Default for GroupBehavior {
    fn default() -> Self {
        Self::Individual
    }
}

/// Formation definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct FormationDef {
    #[serde(rename = "type")]
    pub formation_type: FormationType,
    #[serde(default = "default_formation_spacing")]
    pub spacing: f32,
}

fn default_formation_spacing() -> f32 { 2.0 }

/// Formation type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum FormationType {
    Line,
    Column,
    Wedge,
    Circle,
    Custom,
}

impl Default for FormationType {
    fn default() -> Self {
        Self::Line
    }
}

// ============================================================================
// Game Scene Definition - Combines all systems
// ============================================================================

/// Extended scene definition with all game systems
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct GameSceneDefinition {
    /// Scene-level scripting
    #[serde(default)]
    pub scripting: Option<SceneScriptingDef>,
    /// Scene-level physics configuration
    #[serde(default)]
    pub physics_config: Option<PhysicsConfigDef>,
    /// Scene-level audio
    #[serde(default)]
    pub audio: Option<AudioConfigDef>,
    /// Scene-level state
    #[serde(default)]
    pub state: Option<StateConfigDef>,
    /// Scene-level UI
    #[serde(default)]
    pub ui: Option<UiConfigDef>,
    /// Scene-level navigation
    #[serde(default)]
    pub navigation: Option<NavigationConfigDef>,
    /// Item definitions (scene-local)
    #[serde(default)]
    pub items: Vec<ItemDef>,
    /// Loot tables
    #[serde(default)]
    pub loot_tables: Vec<LootTableDef>,
    /// Quest definitions
    #[serde(default)]
    pub quests: Vec<QuestDef>,
    /// Achievements
    #[serde(default)]
    pub achievements: Vec<AchievementDef>,
}

/// Scene-level scripting configuration
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct SceneScriptingDef {
    /// Libraries to load
    #[serde(default)]
    pub libraries: Vec<String>,
    /// Global variables
    #[serde(default)]
    pub globals: HashMap<String, ScriptValue>,
    /// Scene-level event handlers
    #[serde(default)]
    pub events: HashMap<String, String>,
}

/// Scene-level physics configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PhysicsConfigDef {
    #[serde(default)]
    pub gravity: [f32; 3],
    #[serde(default = "default_physics_substeps")]
    pub substeps: u32,
    #[serde(default)]
    pub collision_layers: HashMap<String, u32>,
    #[serde(default)]
    pub layer_collisions: Vec<[String; 2]>,
}

fn default_physics_substeps() -> u32 { 1 }

impl Default for PhysicsConfigDef {
    fn default() -> Self {
        Self {
            gravity: [0.0, -9.81, 0.0],
            substeps: default_physics_substeps(),
            collision_layers: HashMap::new(),
            layer_collisions: Vec::new(),
        }
    }
}

/// Entity-level game components
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct GameEntityComponents {
    /// C++ class script
    #[serde(default)]
    pub cpp_class: Option<CppClassDef>,
    /// Blueprint
    #[serde(default)]
    pub blueprint: Option<BlueprintDef>,
    /// Event bindings
    #[serde(default)]
    pub events: Vec<EventBindingDef>,
    /// Full trigger (replaces basic trigger)
    #[serde(default)]
    pub trigger: Option<TriggerDefinition>,
    /// Full physics (replaces basic physics)
    #[serde(default)]
    pub physics: Option<FullPhysicsDef>,
    /// Character controller
    #[serde(default)]
    pub character_controller: Option<CharacterControllerDef>,
    /// Joints
    #[serde(default)]
    pub joints: Vec<JointDef>,
    /// Health component
    #[serde(default)]
    pub health: Option<HealthDef>,
    /// Weapon component
    #[serde(default)]
    pub weapon: Option<WeaponDef>,
    /// Inventory component
    #[serde(default)]
    pub inventory: Option<InventoryDef>,
    /// Pickup component
    #[serde(default)]
    pub pickup: Option<PickupDef>,
    /// Vendor component
    #[serde(default)]
    pub vendor: Option<VendorDef>,
    /// Audio component
    #[serde(default)]
    pub audio: Option<EntityAudioDef>,
    /// AI component
    #[serde(default)]
    pub ai: Option<AiComponentDef>,
    /// Tags for filtering
    #[serde(default)]
    pub tags: Vec<String>,
    /// Custom properties
    #[serde(default)]
    pub properties: HashMap<String, ScriptValue>,
}

// ============================================================================
// RUNTIME GAME SYSTEMS - Process entity components at runtime
// ============================================================================

/// Runtime health component - tracks current health state
#[derive(Debug, Clone)]
pub struct HealthComponent {
    pub entity_id: u64,
    pub max_health: f32,
    pub current_health: f32,
    pub regeneration: f32,
    pub regen_delay: f32,
    pub time_since_damage: f32,
    pub invulnerable: bool,
    pub damage_multipliers: HashMap<String, f32>,
    pub on_damage: Option<String>,
    pub on_death: Option<String>,
    pub on_heal: Option<String>,
    pub is_dead: bool,
}

impl HealthComponent {
    pub fn from_def(entity_id: u64, def: &HealthDef) -> Self {
        let current = def.current_health.unwrap_or(def.max_health);
        Self {
            entity_id,
            max_health: def.max_health,
            current_health: current,
            regeneration: def.regeneration,
            regen_delay: def.regen_delay,
            time_since_damage: def.regen_delay, // Start ready to regen
            invulnerable: def.invulnerable,
            damage_multipliers: def.damage_multipliers.clone(),
            on_damage: def.on_damage.clone(),
            on_death: def.on_death.clone(),
            on_heal: def.on_heal.clone(),
            is_dead: false,
        }
    }

    /// Apply damage, returns actual damage dealt
    pub fn apply_damage(&mut self, amount: f32, damage_type: &str) -> f32 {
        if self.invulnerable || self.is_dead {
            return 0.0;
        }

        let multiplier = self.damage_multipliers.get(damage_type).copied().unwrap_or(1.0);
        let actual_damage = amount * multiplier;

        self.current_health = (self.current_health - actual_damage).max(0.0);
        self.time_since_damage = 0.0;

        if self.current_health <= 0.0 {
            self.is_dead = true;
        }

        actual_damage
    }

    /// Heal the entity, returns actual healing done
    pub fn heal(&mut self, amount: f32) -> f32 {
        if self.is_dead {
            return 0.0;
        }

        let old_health = self.current_health;
        self.current_health = (self.current_health + amount).min(self.max_health);
        self.current_health - old_health
    }

    /// Update health regeneration
    pub fn update(&mut self, delta_time: f32) {
        if self.is_dead || self.regeneration <= 0.0 {
            return;
        }

        self.time_since_damage += delta_time;

        if self.time_since_damage >= self.regen_delay {
            self.heal(self.regeneration * delta_time);
        }
    }

    /// Revive with optional health amount
    pub fn revive(&mut self, health: Option<f32>) {
        self.is_dead = false;
        self.current_health = health.unwrap_or(self.max_health);
    }

    /// Get health percentage (0.0 to 1.0)
    pub fn health_percent(&self) -> f32 {
        self.current_health / self.max_health
    }
}

/// Runtime weapon component - tracks firing state, ammo, cooldowns
#[derive(Debug, Clone)]
pub struct WeaponComponent {
    pub entity_id: u64,
    pub def: WeaponDef,
    pub current_ammo: u32,
    pub reserve_ammo: u32,
    pub fire_cooldown: f32,
    pub reload_timer: f32,
    pub is_reloading: bool,
    pub combo_index: u32,
    pub last_fire_time: f32,
}

impl WeaponComponent {
    pub fn from_def(entity_id: u64, def: &WeaponDef) -> Self {
        let current_ammo = def.ammo.as_ref().map(|a| a.magazine_size).unwrap_or(u32::MAX);
        let reserve_ammo = def.ammo.as_ref().and_then(|a| a.reserve_max).unwrap_or(u32::MAX);
        Self {
            entity_id,
            def: def.clone(),
            current_ammo,
            reserve_ammo,
            fire_cooldown: 0.0,
            reload_timer: 0.0,
            is_reloading: false,
            combo_index: 0,
            last_fire_time: 0.0,
        }
    }

    /// Check if weapon can fire
    pub fn can_fire(&self) -> bool {
        !self.is_reloading && self.fire_cooldown <= 0.0 && self.current_ammo > 0
    }

    /// Fire the weapon, returns true if fired
    pub fn fire(&mut self) -> bool {
        if !self.can_fire() {
            return false;
        }

        if self.current_ammo != u32::MAX {
            self.current_ammo -= 1;
        }
        self.fire_cooldown = 1.0 / self.def.fire_rate;

        // Update combo for melee
        if let Some(melee) = &self.def.melee {
            if !melee.combo.is_empty() {
                self.combo_index = (self.combo_index + 1) % melee.combo.len() as u32;
            }
        }

        true
    }

    /// Start reloading
    pub fn reload(&mut self) {
        if self.is_reloading {
            return;
        }
        if let Some(ammo) = &self.def.ammo {
            if self.current_ammo < ammo.magazine_size && self.reserve_ammo > 0 {
                self.is_reloading = true;
                self.reload_timer = ammo.reload_time;
            }
        }
    }

    /// Update weapon timers
    pub fn update(&mut self, delta_time: f32) {
        self.fire_cooldown = (self.fire_cooldown - delta_time).max(0.0);

        if self.is_reloading {
            self.reload_timer -= delta_time;
            if self.reload_timer <= 0.0 {
                self.finish_reload();
            }
        }
    }

    fn finish_reload(&mut self) {
        if let Some(ammo) = &self.def.ammo {
            let needed = ammo.magazine_size - self.current_ammo;
            let available = needed.min(self.reserve_ammo);
            self.current_ammo += available;
            if self.reserve_ammo != u32::MAX {
                self.reserve_ammo -= available;
            }
        }
        self.is_reloading = false;
        self.reload_timer = 0.0;
    }
}

/// Runtime inventory item instance
#[derive(Debug, Clone)]
pub struct ItemInstance {
    pub item_id: String,
    pub quantity: u32,
    pub equipped: bool,
    pub slot: Option<EquipmentSlot>,
    pub durability: Option<f32>,
    pub custom_data: HashMap<String, ScriptValue>,
}

/// Runtime inventory component
#[derive(Debug, Clone)]
pub struct InventoryComponent {
    pub entity_id: u64,
    pub max_slots: u32,
    pub max_weight: Option<f32>,
    pub items: Vec<ItemInstance>,
    pub equipped: HashMap<EquipmentSlot, usize>,
    pub currency: HashMap<String, i64>,
    pub current_weight: f32,
}

impl InventoryComponent {
    pub fn from_def(entity_id: u64, def: &InventoryDef) -> Self {
        let mut inv = Self {
            entity_id,
            max_slots: def.slots,
            max_weight: def.max_weight,
            items: Vec::new(),
            equipped: HashMap::new(),
            currency: HashMap::new(), // starting_currency not in InventoryDef, use empty
            current_weight: 0.0,
        };

        // Add starting items
        for start_item in &def.starting_items {
            inv.add_item(&start_item.item, start_item.count, None);
        }

        inv
    }

    /// Add item to inventory, returns slot index or None if full
    pub fn add_item(&mut self, item_id: &str, quantity: u32, item_defs: Option<&[ItemDef]>) -> Option<usize> {
        // Find item weight from definitions if provided
        let item_weight = item_defs
            .and_then(|defs| defs.iter().find(|d| d.id == item_id))
            .map(|d| d.weight)
            .unwrap_or(0.0);

        let max_stack = item_defs
            .and_then(|defs| defs.iter().find(|d| d.id == item_id))
            .map(|d| d.max_stack)
            .unwrap_or(99);

        // Check weight limit
        if let Some(max_w) = self.max_weight {
            if self.current_weight + (item_weight * quantity as f32) > max_w {
                return None;
            }
        }

        // Try to stack with existing
        for (idx, existing) in self.items.iter_mut().enumerate() {
            if existing.item_id == item_id && existing.quantity < max_stack {
                let can_add = (max_stack - existing.quantity).min(quantity);
                existing.quantity += can_add;
                self.current_weight += item_weight * can_add as f32;
                if can_add == quantity {
                    return Some(idx);
                }
                // Remaining quantity - continue to add new stack
            }
        }

        // Check slot limit
        if self.items.len() >= self.max_slots as usize {
            return None;
        }

        // Add new stack
        let idx = self.items.len();
        self.items.push(ItemInstance {
            item_id: item_id.to_string(),
            quantity,
            equipped: false,
            slot: None,
            durability: None,
            custom_data: HashMap::new(),
        });
        self.current_weight += item_weight * quantity as f32;
        Some(idx)
    }

    /// Remove item from inventory
    pub fn remove_item(&mut self, item_id: &str, quantity: u32) -> bool {
        let mut remaining = quantity;
        let mut indices_to_remove = Vec::new();

        for (idx, item) in self.items.iter_mut().enumerate() {
            if item.item_id == item_id && remaining > 0 {
                let take = item.quantity.min(remaining);
                item.quantity -= take;
                remaining -= take;
                if item.quantity == 0 {
                    indices_to_remove.push(idx);
                }
            }
        }

        // Remove empty slots (in reverse order to maintain indices)
        for idx in indices_to_remove.into_iter().rev() {
            self.items.remove(idx);
        }

        remaining == 0
    }

    /// Get item count
    pub fn item_count(&self, item_id: &str) -> u32 {
        self.items
            .iter()
            .filter(|i| i.item_id == item_id)
            .map(|i| i.quantity)
            .sum()
    }

    /// Check if inventory has item
    pub fn has_item(&self, item_id: &str, count: u32) -> bool {
        self.item_count(item_id) >= count
    }

    /// Equip item at slot index
    pub fn equip(&mut self, slot_idx: usize, equipment_slot: EquipmentSlot) -> bool {
        if slot_idx >= self.items.len() {
            return false;
        }

        // Unequip current item in that slot
        if let Some(&current_idx) = self.equipped.get(&equipment_slot) {
            if current_idx < self.items.len() {
                self.items[current_idx].equipped = false;
                self.items[current_idx].slot = None;
            }
        }

        // Equip new item
        self.items[slot_idx].equipped = true;
        self.items[slot_idx].slot = Some(equipment_slot.clone());
        self.equipped.insert(equipment_slot, slot_idx);
        true
    }

    /// Modify currency
    pub fn modify_currency(&mut self, currency_type: &str, amount: i64) -> bool {
        let current = self.currency.entry(currency_type.to_string()).or_insert(0);
        let new_amount = *current + amount;
        if new_amount < 0 {
            return false;
        }
        *current = new_amount;
        true
    }
}

/// Runtime AI state
#[derive(Debug, Clone)]
pub struct AiState {
    pub entity_id: u64,
    pub current_state: String,
    pub target_entity: Option<u64>,
    pub target_position: Option<[f32; 3]>,
    pub last_known_position: Option<[f32; 3]>,
    pub alert_level: f32,
    pub state_time: f32,
    pub path: Vec<[f32; 3]>,
    pub path_index: usize,
    pub waypoint_path: Option<String>,
    pub waypoint_index: usize,
    pub variables: HashMap<String, ScriptValue>,
}

impl AiState {
    pub fn from_def(entity_id: u64, def: &AiComponentDef) -> Self {
        let initial_state = match &def.ai_type {
            AiTypeDef::StateMachine { initial_state, .. } => {
                initial_state.clone()
            }
            _ => "idle".to_string(),
        };

        Self {
            entity_id,
            current_state: initial_state,
            target_entity: None,
            target_position: None,
            last_known_position: None,
            alert_level: 0.0,
            state_time: 0.0,
            path: Vec::new(),
            path_index: 0,
            waypoint_path: None,
            waypoint_index: 0,
            variables: HashMap::new(),
        }
    }

    /// Transition to a new state
    pub fn transition(&mut self, new_state: &str) {
        if self.current_state != new_state {
            log::debug!("AI {} transitioning from {} to {}", self.entity_id, self.current_state, new_state);
            self.current_state = new_state.to_string();
            self.state_time = 0.0;
        }
    }

    /// Set AI target
    pub fn set_target(&mut self, target: Option<u64>, position: Option<[f32; 3]>) {
        self.target_entity = target;
        if let Some(pos) = position {
            self.target_position = Some(pos);
            self.last_known_position = Some(pos);
        }
    }

    /// Update alert level
    pub fn update_alert(&mut self, delta: f32, decay_rate: f32) {
        self.alert_level = (self.alert_level - decay_rate * delta).max(0.0);
    }

    /// Alert the AI
    pub fn alert(&mut self, level: f32, source_position: Option<[f32; 3]>) {
        self.alert_level = self.alert_level.max(level).min(1.0);
        if let Some(pos) = source_position {
            self.last_known_position = Some(pos);
        }
    }
}

/// Active status effect on an entity
#[derive(Debug, Clone)]
pub struct ActiveStatusEffect {
    pub effect_id: String,
    pub remaining_time: f32,
    pub stacks: u32,
    pub tick_timer: f32,
    pub source_entity: Option<u64>,
}

/// Quest progress tracking
#[derive(Debug, Clone)]
pub struct QuestProgress {
    pub quest_id: String,
    pub started: bool,
    pub completed: bool,
    pub failed: bool,
    pub objectives: HashMap<String, u32>, // objective_id -> progress count
}

impl QuestProgress {
    pub fn new(quest_id: &str) -> Self {
        Self {
            quest_id: quest_id.to_string(),
            started: false,
            completed: false,
            failed: false,
            objectives: HashMap::new(),
        }
    }
}

/// Game state manager - handles variables, quests, achievements
#[derive(Debug, Clone, Default)]
pub struct GameStateManager {
    pub variables_int: HashMap<String, i64>,
    pub variables_float: HashMap<String, f64>,
    pub variables_bool: HashMap<String, bool>,
    pub variables_string: HashMap<String, String>,
    pub quests: HashMap<String, QuestProgress>,
    pub achievements: HashMap<String, bool>,
    pub checkpoints: Vec<String>,
    pub current_checkpoint: Option<String>,
}

impl GameStateManager {
    pub fn new() -> Self {
        Self::default()
    }

    // Integer variables
    pub fn get_int(&self, name: &str) -> i64 {
        self.variables_int.get(name).copied().unwrap_or(0)
    }

    pub fn set_int(&mut self, name: &str, value: i64) {
        self.variables_int.insert(name.to_string(), value);
    }

    pub fn add_int(&mut self, name: &str, delta: i64) {
        let current = self.get_int(name);
        self.set_int(name, current + delta);
    }

    // Float variables
    pub fn get_float(&self, name: &str) -> f64 {
        self.variables_float.get(name).copied().unwrap_or(0.0)
    }

    pub fn set_float(&mut self, name: &str, value: f64) {
        self.variables_float.insert(name.to_string(), value);
    }

    // Bool variables
    pub fn get_bool(&self, name: &str) -> bool {
        self.variables_bool.get(name).copied().unwrap_or(false)
    }

    pub fn set_bool(&mut self, name: &str, value: bool) {
        self.variables_bool.insert(name.to_string(), value);
    }

    // String variables
    pub fn get_string(&self, name: &str) -> &str {
        self.variables_string.get(name).map(|s| s.as_str()).unwrap_or("")
    }

    pub fn set_string(&mut self, name: &str, value: &str) {
        self.variables_string.insert(name.to_string(), value.to_string());
    }

    // Quest management
    pub fn start_quest(&mut self, quest_id: &str) {
        let quest = self.quests.entry(quest_id.to_string())
            .or_insert_with(|| QuestProgress::new(quest_id));
        quest.started = true;
        log::info!("Quest started: {}", quest_id);
    }

    pub fn complete_quest(&mut self, quest_id: &str) {
        if let Some(quest) = self.quests.get_mut(quest_id) {
            quest.completed = true;
            log::info!("Quest completed: {}", quest_id);
        }
    }

    pub fn fail_quest(&mut self, quest_id: &str) {
        if let Some(quest) = self.quests.get_mut(quest_id) {
            quest.failed = true;
            log::info!("Quest failed: {}", quest_id);
        }
    }

    pub fn update_objective(&mut self, quest_id: &str, objective_id: &str, progress: u32) {
        if let Some(quest) = self.quests.get_mut(quest_id) {
            quest.objectives.insert(objective_id.to_string(), progress);
        }
    }

    pub fn is_quest_active(&self, quest_id: &str) -> bool {
        self.quests.get(quest_id)
            .map(|q| q.started && !q.completed && !q.failed)
            .unwrap_or(false)
    }

    pub fn is_quest_complete(&self, quest_id: &str) -> bool {
        self.quests.get(quest_id).map(|q| q.completed).unwrap_or(false)
    }

    // Achievement management
    pub fn unlock_achievement(&mut self, achievement_id: &str) {
        if !self.achievements.get(achievement_id).copied().unwrap_or(false) {
            self.achievements.insert(achievement_id.to_string(), true);
            log::info!("Achievement unlocked: {}", achievement_id);
        }
    }

    pub fn has_achievement(&self, achievement_id: &str) -> bool {
        self.achievements.get(achievement_id).copied().unwrap_or(false)
    }

    // Checkpoint management
    pub fn set_checkpoint(&mut self, checkpoint_id: &str) {
        self.current_checkpoint = Some(checkpoint_id.to_string());
        if !self.checkpoints.contains(&checkpoint_id.to_string()) {
            self.checkpoints.push(checkpoint_id.to_string());
        }
        log::info!("Checkpoint reached: {}", checkpoint_id);
    }
}

/// Pending game event for processing
#[derive(Debug, Clone)]
pub enum GameEvent {
    Damage {
        target: u64,
        source: Option<u64>,
        amount: f32,
        damage_type: String,
    },
    Death {
        entity: u64,
        killer: Option<u64>,
    },
    Heal {
        target: u64,
        amount: f32,
    },
    ItemPickup {
        entity: u64,
        item_id: String,
        quantity: u32,
    },
    TriggerEnter {
        trigger: u64,
        entity: u64,
    },
    TriggerExit {
        trigger: u64,
        entity: u64,
    },
    QuestStarted {
        quest_id: String,
    },
    QuestCompleted {
        quest_id: String,
    },
    ObjectiveComplete {
        quest_id: String,
        objective_id: String,
    },
    Custom {
        event_name: String,
        data: HashMap<String, ScriptValue>,
    },
}

/// Comprehensive game systems manager
/// Coordinates all runtime game systems
pub struct GameSystemsManager {
    /// Health components by entity ID
    pub health_components: HashMap<u64, HealthComponent>,
    /// Weapon components by entity ID
    pub weapon_components: HashMap<u64, WeaponComponent>,
    /// Inventory components by entity ID
    pub inventory_components: HashMap<u64, InventoryComponent>,
    /// AI states by entity ID
    pub ai_states: HashMap<u64, AiState>,
    /// Active status effects by entity ID
    pub status_effects: HashMap<u64, Vec<ActiveStatusEffect>>,
    /// Global game state
    pub state: GameStateManager,
    /// Pending events to process
    pub pending_events: Vec<GameEvent>,
    /// Item definitions lookup
    pub item_definitions: HashMap<String, ItemDef>,
    /// Status effect definitions lookup
    pub status_effect_definitions: HashMap<String, StatusEffectDef>,
    /// Quest definitions lookup
    pub quest_definitions: HashMap<String, QuestDef>,
    /// Entity names lookup
    pub entity_names: HashMap<u64, String>,
    /// Entity positions (synced from physics or transforms)
    pub entity_positions: HashMap<u64, [f32; 3]>,
    /// Player entity ID
    pub player_entity: Option<u64>,
}

impl Default for GameSystemsManager {
    fn default() -> Self {
        Self::new()
    }
}

impl GameSystemsManager {
    pub fn new() -> Self {
        Self {
            health_components: HashMap::new(),
            weapon_components: HashMap::new(),
            inventory_components: HashMap::new(),
            ai_states: HashMap::new(),
            status_effects: HashMap::new(),
            state: GameStateManager::new(),
            pending_events: Vec::new(),
            item_definitions: HashMap::new(),
            status_effect_definitions: HashMap::new(),
            quest_definitions: HashMap::new(),
            entity_names: HashMap::new(),
            entity_positions: HashMap::new(),
            player_entity: None,
        }
    }

    /// Register item definitions from scene
    pub fn register_items(&mut self, items: &[ItemDef]) {
        for item in items {
            self.item_definitions.insert(item.id.clone(), item.clone());
        }
    }

    /// Register status effect definitions
    pub fn register_status_effects(&mut self, effects: &[StatusEffectDef]) {
        for effect in effects {
            self.status_effect_definitions.insert(effect.name.clone(), effect.clone());
        }
    }

    /// Register quest definitions
    pub fn register_quests(&mut self, quests: &[QuestDef]) {
        for quest in quests {
            self.quest_definitions.insert(quest.id.clone(), quest.clone());
        }
    }

    /// Create health component for entity
    pub fn create_health(&mut self, entity_id: u64, def: &HealthDef) {
        let component = HealthComponent::from_def(entity_id, def);
        self.health_components.insert(entity_id, component);
    }

    /// Create weapon component for entity
    pub fn create_weapon(&mut self, entity_id: u64, def: &WeaponDef) {
        let component = WeaponComponent::from_def(entity_id, def);
        self.weapon_components.insert(entity_id, component);
    }

    /// Create inventory component for entity
    pub fn create_inventory(&mut self, entity_id: u64, def: &InventoryDef) {
        let component = InventoryComponent::from_def(entity_id, def);
        self.inventory_components.insert(entity_id, component);
    }

    /// Create AI state for entity
    pub fn create_ai(&mut self, entity_id: u64, def: &AiComponentDef) {
        let state = AiState::from_def(entity_id, def);
        self.ai_states.insert(entity_id, state);
    }

    /// Apply damage to entity
    pub fn apply_damage(&mut self, target: u64, source: Option<u64>, amount: f32, damage_type: &str) {
        self.pending_events.push(GameEvent::Damage {
            target,
            source,
            amount,
            damage_type: damage_type.to_string(),
        });
    }

    /// Heal entity
    pub fn heal_entity(&mut self, target: u64, amount: f32) {
        self.pending_events.push(GameEvent::Heal { target, amount });
    }

    /// Fire weapon for entity
    pub fn fire_weapon(&mut self, entity_id: u64) -> Option<WeaponFireResult> {
        let weapon = self.weapon_components.get_mut(&entity_id)?;

        if !weapon.fire() {
            return None;
        }

        let position = self.entity_positions.get(&entity_id).copied().unwrap_or([0.0; 3]);

        Some(WeaponFireResult {
            weapon_type: weapon.def.weapon_type,
            damage: weapon.def.damage,
            damage_type: weapon.def.damage_type.clone(),
            position,
            spread: weapon.def.spread,
            hitscan: weapon.def.hitscan.clone(),
            projectile: weapon.def.projectile.clone(),
            melee: weapon.def.melee.clone(),
        })
    }

    /// Add item to entity's inventory
    pub fn add_item(&mut self, entity_id: u64, item_id: &str, quantity: u32) -> bool {
        let items: Vec<ItemDef> = self.item_definitions.values().cloned().collect();
        if let Some(inv) = self.inventory_components.get_mut(&entity_id) {
            inv.add_item(item_id, quantity, Some(&items)).is_some()
        } else {
            false
        }
    }

    /// Remove item from entity's inventory
    pub fn remove_item(&mut self, entity_id: u64, item_id: &str, quantity: u32) -> bool {
        if let Some(inv) = self.inventory_components.get_mut(&entity_id) {
            inv.remove_item(item_id, quantity)
        } else {
            false
        }
    }

    /// Apply status effect to entity
    pub fn apply_status_effect(&mut self, target: u64, effect_id: &str, source: Option<u64>) {
        let Some(def) = self.status_effect_definitions.get(effect_id).cloned() else {
            log::warn!("Unknown status effect: {}", effect_id);
            return;
        };

        let effects = self.status_effects.entry(target).or_default();

        // Check for existing effect
        if let Some(existing) = effects.iter_mut().find(|e| e.effect_id == effect_id) {
            if def.stacks && existing.stacks < def.max_stacks {
                existing.stacks += 1;
                existing.remaining_time = def.duration;
            } else if !def.stacks {
                existing.remaining_time = def.duration;
            }
        } else {
            effects.push(ActiveStatusEffect {
                effect_id: effect_id.to_string(),
                remaining_time: def.duration,
                stacks: 1,
                tick_timer: 0.0,
                source_entity: source,
            });
        }

        log::debug!("Applied status effect {} to entity {}", effect_id, target);
    }

    /// Set AI state for entity
    pub fn set_ai_state(&mut self, entity_id: u64, state: &str) {
        if let Some(ai) = self.ai_states.get_mut(&entity_id) {
            ai.transition(state);
        }
    }

    /// Set AI target for entity
    pub fn set_ai_target(&mut self, entity_id: u64, target: Option<u64>) {
        if let Some(ai) = self.ai_states.get_mut(&entity_id) {
            let position = target.and_then(|t| self.entity_positions.get(&t).copied());
            ai.set_target(target, position);
        }
    }

    /// Update all systems for one frame
    pub fn update(&mut self, delta_time: f32) {
        // Process pending events
        self.process_events();

        // Update health components (regeneration)
        for health in self.health_components.values_mut() {
            health.update(delta_time);
        }

        // Update weapon components (cooldowns)
        for weapon in self.weapon_components.values_mut() {
            weapon.update(delta_time);
        }

        // Update status effects
        self.update_status_effects(delta_time);

        // Update AI states
        self.update_ai(delta_time);
    }

    fn process_events(&mut self) {
        let events: Vec<GameEvent> = self.pending_events.drain(..).collect();

        for event in events {
            match event {
                GameEvent::Damage { target, source, amount, damage_type } => {
                    if let Some(health) = self.health_components.get_mut(&target) {
                        let actual = health.apply_damage(amount, &damage_type);
                        if actual > 0.0 {
                            log::debug!("Entity {} took {} {} damage", target, actual, damage_type);

                            if health.is_dead {
                                self.pending_events.push(GameEvent::Death {
                                    entity: target,
                                    killer: source,
                                });
                            }
                        }
                    }
                }
                GameEvent::Death { entity, killer } => {
                    log::info!("Entity {} died (killed by {:?})", entity, killer);
                    // Additional death handling could go here
                }
                GameEvent::Heal { target, amount } => {
                    if let Some(health) = self.health_components.get_mut(&target) {
                        let actual = health.heal(amount);
                        if actual > 0.0 {
                            log::debug!("Entity {} healed for {}", target, actual);
                        }
                    }
                }
                GameEvent::ItemPickup { entity, item_id, quantity } => {
                    if self.add_item(entity, &item_id, quantity) {
                        log::info!("Entity {} picked up {}x {}", entity, quantity, item_id);
                    }
                }
                GameEvent::TriggerEnter { trigger, entity } => {
                    log::debug!("Entity {} entered trigger {}", entity, trigger);
                    // Trigger action processing would be handled by TriggerSystem
                }
                GameEvent::TriggerExit { trigger, entity } => {
                    log::debug!("Entity {} exited trigger {}", entity, trigger);
                }
                GameEvent::QuestStarted { quest_id } => {
                    self.state.start_quest(&quest_id);
                }
                GameEvent::QuestCompleted { quest_id } => {
                    self.state.complete_quest(&quest_id);
                }
                GameEvent::ObjectiveComplete { quest_id, objective_id } => {
                    if let Some(quest_def) = self.quest_definitions.get(&quest_id) {
                        if let Some(obj) = quest_def.objectives.iter().find(|o| o.id == objective_id) {
                            let current = self.state.quests.get(&quest_id)
                                .and_then(|q| q.objectives.get(&objective_id))
                                .copied()
                                .unwrap_or(0);
                            self.state.update_objective(&quest_id, &objective_id, current + 1);

                            // Check if quest is complete
                            if let Some(progress) = self.state.quests.get(&quest_id) {
                                let all_complete = quest_def.objectives.iter().all(|o| {
                                    progress.objectives.get(&o.id).copied().unwrap_or(0) >= o.count
                                });
                                if all_complete {
                                    self.pending_events.push(GameEvent::QuestCompleted {
                                        quest_id: quest_id.clone(),
                                    });
                                }
                            }
                        }
                    }
                }
                GameEvent::Custom { event_name, data } => {
                    log::debug!("Custom event: {} with {} data entries", event_name, data.len());
                }
            }
        }
    }

    fn update_status_effects(&mut self, delta_time: f32) {
        let mut expired: Vec<(u64, String)> = Vec::new();
        let mut tick_damage: Vec<(u64, f32, String)> = Vec::new();
        let mut tick_heal: Vec<(u64, f32)> = Vec::new();

        for (&entity_id, effects) in &mut self.status_effects {
            for effect in effects.iter_mut() {
                effect.remaining_time -= delta_time;

                if effect.remaining_time <= 0.0 {
                    expired.push((entity_id, effect.effect_id.clone()));
                    continue;
                }

                // Process ticks
                if let Some(def) = self.status_effect_definitions.get(&effect.effect_id) {
                    if def.tick_rate > 0.0 {
                        effect.tick_timer += delta_time;
                        let tick_interval = 1.0 / def.tick_rate;

                        while effect.tick_timer >= tick_interval {
                            effect.tick_timer -= tick_interval;

                            for effect_type in &def.effects {
                                match effect_type {
                                    StatusEffectType::DamageOverTime { damage, damage_type } => {
                                        tick_damage.push((
                                            entity_id,
                                            *damage * effect.stacks as f32,
                                            damage_type.clone(),
                                        ));
                                    }
                                    StatusEffectType::HealOverTime { amount } => {
                                        tick_heal.push((entity_id, *amount * effect.stacks as f32));
                                    }
                                    _ => {}
                                }
                            }
                        }
                    }
                }
            }
        }

        // Remove expired effects
        for (entity_id, effect_id) in expired {
            if let Some(effects) = self.status_effects.get_mut(&entity_id) {
                effects.retain(|e| e.effect_id != effect_id);
            }
        }

        // Apply tick damage/heal
        for (entity, damage, damage_type) in tick_damage {
            self.apply_damage(entity, None, damage, &damage_type);
        }
        for (entity, amount) in tick_heal {
            self.heal_entity(entity, amount);
        }
    }

    fn update_ai(&mut self, delta_time: f32) {
        for ai in self.ai_states.values_mut() {
            ai.state_time += delta_time;
            ai.update_alert(delta_time, 0.1); // Decay alert over time
        }
    }

    /// Get health info for entity
    pub fn get_health(&self, entity_id: u64) -> Option<&HealthComponent> {
        self.health_components.get(&entity_id)
    }

    /// Get inventory for entity
    pub fn get_inventory(&self, entity_id: u64) -> Option<&InventoryComponent> {
        self.inventory_components.get(&entity_id)
    }

    /// Get AI state for entity
    pub fn get_ai_state(&self, entity_id: u64) -> Option<&AiState> {
        self.ai_states.get(&entity_id)
    }

    /// Check if entity is alive
    pub fn is_alive(&self, entity_id: u64) -> bool {
        self.health_components.get(&entity_id)
            .map(|h| !h.is_dead)
            .unwrap_or(true) // Entities without health are considered alive
    }

    /// Get debug info string
    pub fn debug_info(&self) -> String {
        format!(
            "GameSystems: {} health, {} weapons, {} inventories, {} AI, {} effects | State: {} vars, {} quests",
            self.health_components.len(),
            self.weapon_components.len(),
            self.inventory_components.len(),
            self.ai_states.len(),
            self.status_effects.values().map(|v| v.len()).sum::<usize>(),
            self.state.variables_int.len() + self.state.variables_float.len() +
                self.state.variables_bool.len() + self.state.variables_string.len(),
            self.state.quests.len(),
        )
    }
}

/// Result of firing a weapon
#[derive(Debug, Clone)]
pub struct WeaponFireResult {
    pub weapon_type: WeaponType,
    pub damage: f32,
    pub damage_type: String,
    pub position: [f32; 3],
    pub spread: f32,
    pub hitscan: Option<HitscanDef>,
    pub projectile: Option<ProjectileDef>,
    pub melee: Option<MeleeDef>,
}
