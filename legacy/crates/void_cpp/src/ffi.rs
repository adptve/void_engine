//! FFI types and function signatures for C++ interop
//!
//! This module defines the C ABI types used to communicate between
//! Rust and C++ code. All types use `#[repr(C)]` for ABI compatibility.

use std::ffi::{c_char, c_void};
use std::os::raw::c_int;

/// API version for compatibility checking
pub const VOID_CPP_API_VERSION: u32 = 1;

/// Opaque handle to a C++ class instance
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct CppHandle {
    /// Pointer to the C++ object
    pub ptr: *mut c_void,
}

impl CppHandle {
    /// Create a null handle
    pub const fn null() -> Self {
        Self { ptr: std::ptr::null_mut() }
    }

    /// Check if handle is null
    pub fn is_null(&self) -> bool {
        self.ptr.is_null()
    }
}

unsafe impl Send for CppHandle {}
unsafe impl Sync for CppHandle {}

/// Vector3 for FFI (matches C++ FVector)
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct FfiVec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

impl FfiVec3 {
    pub const ZERO: Self = Self { x: 0.0, y: 0.0, z: 0.0 };

    pub fn new(x: f32, y: f32, z: f32) -> Self {
        Self { x, y, z }
    }
}

impl From<void_math::prelude::Vec3> for FfiVec3 {
    fn from(v: void_math::prelude::Vec3) -> Self {
        Self { x: v.x, y: v.y, z: v.z }
    }
}

impl From<FfiVec3> for void_math::prelude::Vec3 {
    fn from(v: FfiVec3) -> Self {
        void_math::prelude::Vec3::new(v.x, v.y, v.z)
    }
}

/// Quaternion for FFI (matches C++ FQuat)
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiQuat {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

impl FfiQuat {
    pub const IDENTITY: Self = Self { x: 0.0, y: 0.0, z: 0.0, w: 1.0 };

    pub fn new(x: f32, y: f32, z: f32, w: f32) -> Self {
        Self { x, y, z, w }
    }
}

impl Default for FfiQuat {
    fn default() -> Self {
        Self::IDENTITY
    }
}

/// Transform for FFI
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct FfiTransform {
    pub position: FfiVec3,
    pub rotation: FfiQuat,
    pub scale: FfiVec3,
}

impl FfiTransform {
    pub fn identity() -> Self {
        Self {
            position: FfiVec3::ZERO,
            rotation: FfiQuat::IDENTITY,
            scale: FfiVec3::new(1.0, 1.0, 1.0),
        }
    }
}

/// Entity ID for FFI
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct FfiEntityId {
    pub index: u32,
    pub generation: u32,
}

impl Default for FfiEntityId {
    fn default() -> Self {
        Self::INVALID
    }
}

impl FfiEntityId {
    pub const INVALID: Self = Self { index: u32::MAX, generation: 0 };

    pub fn new(index: u32, generation: u32) -> Self {
        Self { index, generation }
    }

    pub fn is_valid(&self) -> bool {
        self.index != u32::MAX
    }
}

impl From<void_ecs::prelude::Entity> for FfiEntityId {
    fn from(e: void_ecs::prelude::Entity) -> Self {
        Self::new(e.index(), e.generation())
    }
}

impl From<FfiEntityId> for void_ecs::prelude::Entity {
    fn from(id: FfiEntityId) -> Self {
        void_ecs::prelude::Entity::new(id.index, id.generation)
    }
}

/// Hit result for collision/raycast callbacks
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct FfiHitResult {
    pub hit: bool,
    pub point: FfiVec3,
    pub normal: FfiVec3,
    pub distance: f32,
    pub entity: FfiEntityId,
}

/// Damage info passed to OnDamage callback
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiDamageInfo {
    pub amount: f32,
    pub damage_type: c_int,
    pub source: FfiEntityId,
    pub hit_point: FfiVec3,
    pub hit_normal: FfiVec3,
    pub is_critical: bool,
}

impl Default for FfiDamageInfo {
    fn default() -> Self {
        Self {
            amount: 0.0,
            damage_type: 0,
            source: FfiEntityId::INVALID,
            hit_point: FfiVec3::ZERO,
            hit_normal: FfiVec3::ZERO,
            is_critical: false,
        }
    }
}

/// Input action info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiInputAction {
    pub action_name: *const c_char,
    pub value: f32,
    pub pressed: bool,
}

/// Class metadata exported by C++ libraries
#[repr(C)]
pub struct FfiClassInfo {
    /// Class name (null-terminated)
    pub name: *const c_char,
    /// Size in bytes
    pub size: usize,
    /// Alignment requirement
    pub alignment: usize,
    /// API version the class was built with
    pub api_version: u32,
    /// Constructor function
    pub create_fn: Option<extern "C" fn() -> CppHandle>,
    /// Destructor function
    pub destroy_fn: Option<extern "C" fn(CppHandle)>,
}

/// Function pointer types for class lifecycle methods
pub type BeginPlayFn = extern "C" fn(CppHandle);
pub type TickFn = extern "C" fn(CppHandle, f32);
pub type EndPlayFn = extern "C" fn(CppHandle);
pub type FixedTickFn = extern "C" fn(CppHandle, f32);

/// Function pointer types for event callbacks
pub type OnCollisionEnterFn = extern "C" fn(CppHandle, FfiEntityId, FfiHitResult);
pub type OnCollisionExitFn = extern "C" fn(CppHandle, FfiEntityId);
pub type OnTriggerEnterFn = extern "C" fn(CppHandle, FfiEntityId);
pub type OnTriggerExitFn = extern "C" fn(CppHandle, FfiEntityId);
pub type OnDamageFn = extern "C" fn(CppHandle, FfiDamageInfo);
pub type OnDeathFn = extern "C" fn(CppHandle, FfiEntityId);
pub type OnInteractFn = extern "C" fn(CppHandle, FfiEntityId);
pub type OnInputActionFn = extern "C" fn(CppHandle, FfiInputAction);

/// Virtual table for C++ class methods
#[repr(C)]
pub struct FfiClassVTable {
    // Lifecycle
    pub begin_play: Option<BeginPlayFn>,
    pub tick: Option<TickFn>,
    pub end_play: Option<EndPlayFn>,
    pub fixed_tick: Option<FixedTickFn>,

    // Collision events
    pub on_collision_enter: Option<OnCollisionEnterFn>,
    pub on_collision_exit: Option<OnCollisionExitFn>,
    pub on_trigger_enter: Option<OnTriggerEnterFn>,
    pub on_trigger_exit: Option<OnTriggerExitFn>,

    // Combat events
    pub on_damage: Option<OnDamageFn>,
    pub on_death: Option<OnDeathFn>,

    // Interaction
    pub on_interact: Option<OnInteractFn>,
    pub on_input_action: Option<OnInputActionFn>,

    // Serialization for hot-reload
    pub serialize: Option<extern "C" fn(CppHandle, *mut u8, usize) -> usize>,
    pub deserialize: Option<extern "C" fn(CppHandle, *const u8, usize) -> bool>,
    pub get_serialized_size: Option<extern "C" fn(CppHandle) -> usize>,
}

impl Default for FfiClassVTable {
    fn default() -> Self {
        Self {
            begin_play: None,
            tick: None,
            end_play: None,
            fixed_tick: None,
            on_collision_enter: None,
            on_collision_exit: None,
            on_trigger_enter: None,
            on_trigger_exit: None,
            on_damage: None,
            on_death: None,
            on_interact: None,
            on_input_action: None,
            serialize: None,
            deserialize: None,
            get_serialized_size: None,
        }
    }
}

/// Library info returned by void_get_library_info
#[repr(C)]
pub struct FfiLibraryInfo {
    /// API version
    pub api_version: u32,
    /// Number of classes in the library
    pub class_count: u32,
    /// Library name (null-terminated)
    pub name: *const c_char,
    /// Library version string (null-terminated)
    pub version: *const c_char,
}

/// Type alias for the library info function
pub type GetLibraryInfoFn = extern "C" fn() -> FfiLibraryInfo;

/// Type alias for getting class info by index
pub type GetClassInfoFn = extern "C" fn(u32) -> *const FfiClassInfo;

/// Type alias for getting class vtable
pub type GetClassVTableFn = extern "C" fn(*const c_char) -> *const FfiClassVTable;

/// World context passed to C++ for engine access
#[repr(C)]
pub struct FfiWorldContext {
    /// Opaque pointer to Rust world
    pub world_ptr: *mut c_void,

    // Entity operations
    pub spawn_entity: Option<extern "C" fn(*mut c_void, *const c_char) -> FfiEntityId>,
    pub destroy_entity: Option<extern "C" fn(*mut c_void, FfiEntityId)>,
    pub get_entity_position: Option<extern "C" fn(*mut c_void, FfiEntityId) -> FfiVec3>,
    pub set_entity_position: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiVec3)>,
    pub get_entity_rotation: Option<extern "C" fn(*mut c_void, FfiEntityId) -> FfiQuat>,
    pub set_entity_rotation: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiQuat)>,

    // Physics
    pub apply_force: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiVec3)>,
    pub apply_impulse: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiVec3)>,
    pub set_velocity: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiVec3)>,
    pub raycast: Option<extern "C" fn(*mut c_void, FfiVec3, FfiVec3, f32) -> FfiHitResult>,

    // Audio
    pub play_sound: Option<extern "C" fn(*mut c_void, *const c_char)>,
    pub play_sound_at: Option<extern "C" fn(*mut c_void, *const c_char, FfiVec3)>,

    // Logging
    pub log_message: Option<extern "C" fn(*mut c_void, c_int, *const c_char)>,
}

impl Default for FfiWorldContext {
    fn default() -> Self {
        Self {
            world_ptr: std::ptr::null_mut(),
            spawn_entity: None,
            destroy_entity: None,
            get_entity_position: None,
            set_entity_position: None,
            get_entity_rotation: None,
            set_entity_rotation: None,
            apply_force: None,
            apply_impulse: None,
            set_velocity: None,
            raycast: None,
            play_sound: None,
            play_sound_at: None,
            log_message: None,
        }
    }
}

/// Set the world context for a C++ instance
pub type SetWorldContextFn = extern "C" fn(CppHandle, *const FfiWorldContext);

/// Set the entity ID for a C++ instance
pub type SetEntityIdFn = extern "C" fn(CppHandle, FfiEntityId);

// ============================================================================
// Extended FFI Types for Game Systems (docs 21-29)
// ============================================================================

/// Vector2 for FFI (UI coordinates, etc.)
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct FfiVec2 {
    pub x: f32,
    pub y: f32,
}

impl FfiVec2 {
    pub const ZERO: Self = Self { x: 0.0, y: 0.0 };

    pub fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }
}

/// Color for FFI (RGBA)
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiColor {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

impl FfiColor {
    pub const WHITE: Self = Self { r: 1.0, g: 1.0, b: 1.0, a: 1.0 };
    pub const BLACK: Self = Self { r: 0.0, g: 0.0, b: 0.0, a: 1.0 };
    pub const RED: Self = Self { r: 1.0, g: 0.0, b: 0.0, a: 1.0 };
    pub const GREEN: Self = Self { r: 0.0, g: 1.0, b: 0.0, a: 1.0 };
    pub const BLUE: Self = Self { r: 0.0, g: 0.0, b: 1.0, a: 1.0 };

    pub fn new(r: f32, g: f32, b: f32, a: f32) -> Self {
        Self { r, g, b, a }
    }
}

impl Default for FfiColor {
    fn default() -> Self {
        Self::WHITE
    }
}

// ============================================================================
// Combat System FFI Types (doc 24)
// ============================================================================

/// Health info for queries
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiHealthInfo {
    pub current: f32,
    pub max: f32,
    pub is_alive: bool,
    pub is_invulnerable: bool,
    pub regen_rate: f32,
}

impl Default for FfiHealthInfo {
    fn default() -> Self {
        Self {
            current: 100.0,
            max: 100.0,
            is_alive: true,
            is_invulnerable: false,
            regen_rate: 0.0,
        }
    }
}

/// Status effect info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiStatusEffect {
    pub effect_id: u32,
    pub stacks: u32,
    pub remaining_duration: f32,
    pub source: FfiEntityId,
}

impl Default for FfiStatusEffect {
    fn default() -> Self {
        Self {
            effect_id: 0,
            stacks: 0,
            remaining_duration: 0.0,
            source: FfiEntityId::INVALID,
        }
    }
}

/// Weapon state info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiWeaponInfo {
    pub weapon_id: u32,
    pub ammo_current: u32,
    pub ammo_reserve: u32,
    pub ammo_max_magazine: u32,
    pub is_reloading: bool,
    pub fire_rate: f32,
    pub damage: f32,
}

impl Default for FfiWeaponInfo {
    fn default() -> Self {
        Self {
            weapon_id: 0,
            ammo_current: 0,
            ammo_reserve: 0,
            ammo_max_magazine: 0,
            is_reloading: false,
            fire_rate: 1.0,
            damage: 10.0,
        }
    }
}

// ============================================================================
// Inventory System FFI Types (doc 25)
// ============================================================================

/// Item instance info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiItemInfo {
    pub item_id: u32,
    pub slot_index: u32,
    pub count: u32,
    pub max_stack: u32,
    pub weight: f32,
    pub is_equipped: bool,
}

impl Default for FfiItemInfo {
    fn default() -> Self {
        Self {
            item_id: 0,
            slot_index: u32::MAX,
            count: 0,
            max_stack: 1,
            weight: 0.0,
            is_equipped: false,
        }
    }
}

/// Inventory info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiInventoryInfo {
    pub total_slots: u32,
    pub used_slots: u32,
    pub current_weight: f32,
    pub max_weight: f32,
}

impl Default for FfiInventoryInfo {
    fn default() -> Self {
        Self {
            total_slots: 20,
            used_slots: 0,
            current_weight: 0.0,
            max_weight: f32::MAX,
        }
    }
}

// ============================================================================
// AI/Navigation System FFI Types (doc 29)
// ============================================================================

/// AI state info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiAiState {
    pub current_state: u32,
    pub target_entity: FfiEntityId,
    pub target_position: FfiVec3,
    pub alert_level: f32,
    pub has_target: bool,
    pub can_see_target: bool,
    pub can_hear_target: bool,
}

impl Default for FfiAiState {
    fn default() -> Self {
        Self {
            current_state: 0,
            target_entity: FfiEntityId::INVALID,
            target_position: FfiVec3::ZERO,
            alert_level: 0.0,
            has_target: false,
            can_see_target: false,
            can_hear_target: false,
        }
    }
}

/// Navigation path info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiNavPath {
    pub is_valid: bool,
    pub is_partial: bool,
    pub path_length: f32,
    pub waypoint_count: u32,
}

impl Default for FfiNavPath {
    fn default() -> Self {
        Self {
            is_valid: false,
            is_partial: false,
            path_length: 0.0,
            waypoint_count: 0,
        }
    }
}

/// Cover point info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiCoverPoint {
    pub position: FfiVec3,
    pub facing: FfiVec3,
    pub is_occupied: bool,
    pub cover_type: c_int, // 0=full, 1=half, 2=lean
}

impl Default for FfiCoverPoint {
    fn default() -> Self {
        Self {
            position: FfiVec3::ZERO,
            facing: FfiVec3::ZERO,
            is_occupied: false,
            cover_type: 0,
        }
    }
}

// ============================================================================
// State System FFI Types (doc 27)
// ============================================================================

/// Quest status
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FfiQuestStatus {
    NotStarted = 0,
    InProgress = 1,
    Completed = 2,
    Failed = 3,
}

impl Default for FfiQuestStatus {
    fn default() -> Self {
        Self::NotStarted
    }
}

/// Quest info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiQuestInfo {
    pub quest_id: u32,
    pub status: FfiQuestStatus,
    pub current_objective: u32,
    pub total_objectives: u32,
}

impl Default for FfiQuestInfo {
    fn default() -> Self {
        Self {
            quest_id: 0,
            status: FfiQuestStatus::NotStarted,
            current_objective: 0,
            total_objectives: 0,
        }
    }
}

/// Objective progress info
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiObjectiveInfo {
    pub objective_id: u32,
    pub current_count: u32,
    pub required_count: u32,
    pub is_complete: bool,
    pub is_optional: bool,
}

impl Default for FfiObjectiveInfo {
    fn default() -> Self {
        Self {
            objective_id: 0,
            current_count: 0,
            required_count: 1,
            is_complete: false,
            is_optional: false,
        }
    }
}

// ============================================================================
// UI/HUD System FFI Types (doc 28)
// ============================================================================

/// Anchor point for UI positioning
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FfiAnchorPoint {
    TopLeft = 0,
    TopCenter = 1,
    TopRight = 2,
    MiddleLeft = 3,
    Center = 4,
    MiddleRight = 5,
    BottomLeft = 6,
    BottomCenter = 7,
    BottomRight = 8,
}

impl Default for FfiAnchorPoint {
    fn default() -> Self {
        Self::TopLeft
    }
}

/// HUD element visibility state
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiHudElement {
    pub element_id: u32,
    pub is_visible: bool,
    pub position: FfiVec2,
    pub size: FfiVec2,
    pub opacity: f32,
}

impl Default for FfiHudElement {
    fn default() -> Self {
        Self {
            element_id: 0,
            is_visible: true,
            position: FfiVec2::ZERO,
            size: FfiVec2::ZERO,
            opacity: 1.0,
        }
    }
}

// ============================================================================
// Audio System FFI Types (doc 26)
// ============================================================================

/// Sound handle for managing playing sounds
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct FfiSoundHandle {
    pub id: u64,
}

impl FfiSoundHandle {
    pub const INVALID: Self = Self { id: 0 };

    pub fn is_valid(&self) -> bool {
        self.id != 0
    }
}

impl Default for FfiSoundHandle {
    fn default() -> Self {
        Self::INVALID
    }
}

/// Audio playback parameters
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FfiAudioParams {
    pub volume: f32,
    pub pitch: f32,
    pub is_looping: bool,
    pub is_3d: bool,
    pub position: FfiVec3,
    pub min_distance: f32,
    pub max_distance: f32,
}

impl Default for FfiAudioParams {
    fn default() -> Self {
        Self {
            volume: 1.0,
            pitch: 1.0,
            is_looping: false,
            is_3d: false,
            position: FfiVec3::ZERO,
            min_distance: 1.0,
            max_distance: 100.0,
        }
    }
}

// ============================================================================
// Extended Event Callback Types
// ============================================================================

/// Callback for AI state changes
pub type OnAiStateChangeFn = extern "C" fn(CppHandle, old_state: u32, new_state: u32);

/// Callback for when AI detects a target
pub type OnAiTargetAcquiredFn = extern "C" fn(CppHandle, target: FfiEntityId);

/// Callback for when AI loses a target
pub type OnAiTargetLostFn = extern "C" fn(CppHandle);

/// Callback for inventory changes
pub type OnInventoryChangeFn = extern "C" fn(CppHandle, item_id: u32, old_count: u32, new_count: u32);

/// Callback for item pickup
pub type OnItemPickupFn = extern "C" fn(CppHandle, item_id: u32, count: u32);

/// Callback for item use
pub type OnItemUseFn = extern "C" fn(CppHandle, item_id: u32);

/// Callback for weapon fire
pub type OnWeaponFireFn = extern "C" fn(CppHandle);

/// Callback for weapon reload
pub type OnWeaponReloadFn = extern "C" fn(CppHandle);

/// Callback for status effect applied
pub type OnStatusEffectAppliedFn = extern "C" fn(CppHandle, effect_id: u32, source: FfiEntityId);

/// Callback for status effect removed
pub type OnStatusEffectRemovedFn = extern "C" fn(CppHandle, effect_id: u32);

/// Callback for heal received
pub type OnHealFn = extern "C" fn(CppHandle, amount: f32, source: FfiEntityId);

/// Callback for quest started
pub type OnQuestStartFn = extern "C" fn(CppHandle, quest_id: u32);

/// Callback for quest objective progress
pub type OnQuestProgressFn = extern "C" fn(CppHandle, quest_id: u32, objective_id: u32, progress: u32);

/// Callback for quest completed
pub type OnQuestCompleteFn = extern "C" fn(CppHandle, quest_id: u32);

/// Callback for achievement unlocked
pub type OnAchievementUnlockedFn = extern "C" fn(CppHandle, achievement_id: u32);

/// Callback for dialogue started
pub type OnDialogueStartFn = extern "C" fn(CppHandle, dialogue_id: u32, speaker: FfiEntityId);

/// Callback for dialogue choice made
pub type OnDialogueChoiceFn = extern "C" fn(CppHandle, dialogue_id: u32, choice_id: u32);

/// Callback for sound finished playing
pub type OnSoundFinishedFn = extern "C" fn(CppHandle, sound_handle: FfiSoundHandle);

// ============================================================================
// Extended VTable for Game Systems
// ============================================================================

/// Extended virtual table with all game system callbacks
#[repr(C)]
pub struct FfiExtendedVTable {
    // Base vtable functions (inherit from FfiClassVTable)
    pub base: FfiClassVTable,

    // AI callbacks
    pub on_ai_state_change: Option<OnAiStateChangeFn>,
    pub on_ai_target_acquired: Option<OnAiTargetAcquiredFn>,
    pub on_ai_target_lost: Option<OnAiTargetLostFn>,

    // Inventory callbacks
    pub on_inventory_change: Option<OnInventoryChangeFn>,
    pub on_item_pickup: Option<OnItemPickupFn>,
    pub on_item_use: Option<OnItemUseFn>,

    // Weapon callbacks
    pub on_weapon_fire: Option<OnWeaponFireFn>,
    pub on_weapon_reload: Option<OnWeaponReloadFn>,

    // Combat callbacks
    pub on_status_effect_applied: Option<OnStatusEffectAppliedFn>,
    pub on_status_effect_removed: Option<OnStatusEffectRemovedFn>,
    pub on_heal: Option<OnHealFn>,

    // Quest callbacks
    pub on_quest_start: Option<OnQuestStartFn>,
    pub on_quest_progress: Option<OnQuestProgressFn>,
    pub on_quest_complete: Option<OnQuestCompleteFn>,
    pub on_achievement_unlocked: Option<OnAchievementUnlockedFn>,

    // Dialogue callbacks
    pub on_dialogue_start: Option<OnDialogueStartFn>,
    pub on_dialogue_choice: Option<OnDialogueChoiceFn>,

    // Audio callbacks
    pub on_sound_finished: Option<OnSoundFinishedFn>,
}

impl Default for FfiExtendedVTable {
    fn default() -> Self {
        Self {
            base: FfiClassVTable::default(),
            on_ai_state_change: None,
            on_ai_target_acquired: None,
            on_ai_target_lost: None,
            on_inventory_change: None,
            on_item_pickup: None,
            on_item_use: None,
            on_weapon_fire: None,
            on_weapon_reload: None,
            on_status_effect_applied: None,
            on_status_effect_removed: None,
            on_heal: None,
            on_quest_start: None,
            on_quest_progress: None,
            on_quest_complete: None,
            on_achievement_unlocked: None,
            on_dialogue_start: None,
            on_dialogue_choice: None,
            on_sound_finished: None,
        }
    }
}

// ============================================================================
// Extended World Context with Full Game Systems API
// ============================================================================

/// Extended world context with all game system functions
#[repr(C)]
pub struct FfiExtendedWorldContext {
    // Base context
    pub base: FfiWorldContext,

    // ========== Combat System (doc 24) ==========
    /// Get health info for an entity
    pub get_health_info: Option<extern "C" fn(*mut c_void, FfiEntityId) -> FfiHealthInfo>,
    /// Apply damage to an entity
    pub apply_damage: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiDamageInfo)>,
    /// Heal an entity
    pub heal_entity: Option<extern "C" fn(*mut c_void, FfiEntityId, f32, FfiEntityId)>,
    /// Set invulnerability
    pub set_invulnerable: Option<extern "C" fn(*mut c_void, FfiEntityId, bool)>,
    /// Apply status effect
    pub apply_status_effect: Option<extern "C" fn(*mut c_void, FfiEntityId, u32, f32, FfiEntityId)>,
    /// Remove status effect
    pub remove_status_effect: Option<extern "C" fn(*mut c_void, FfiEntityId, u32)>,
    /// Check if entity has status effect
    pub has_status_effect: Option<extern "C" fn(*mut c_void, FfiEntityId, u32) -> bool>,
    /// Get weapon info for an entity
    pub get_weapon_info: Option<extern "C" fn(*mut c_void, FfiEntityId) -> FfiWeaponInfo>,
    /// Fire weapon
    pub fire_weapon: Option<extern "C" fn(*mut c_void, FfiEntityId)>,
    /// Reload weapon
    pub reload_weapon: Option<extern "C" fn(*mut c_void, FfiEntityId)>,

    // ========== Inventory System (doc 25) ==========
    /// Get inventory info
    pub get_inventory_info: Option<extern "C" fn(*mut c_void, FfiEntityId) -> FfiInventoryInfo>,
    /// Add item to inventory
    pub add_item: Option<extern "C" fn(*mut c_void, FfiEntityId, u32, u32) -> bool>,
    /// Remove item from inventory
    pub remove_item: Option<extern "C" fn(*mut c_void, FfiEntityId, u32, u32) -> bool>,
    /// Check if has item
    pub has_item: Option<extern "C" fn(*mut c_void, FfiEntityId, u32, u32) -> bool>,
    /// Get item count
    pub get_item_count: Option<extern "C" fn(*mut c_void, FfiEntityId, u32) -> u32>,
    /// Equip item
    pub equip_item: Option<extern "C" fn(*mut c_void, FfiEntityId, u32) -> bool>,
    /// Unequip item
    pub unequip_item: Option<extern "C" fn(*mut c_void, FfiEntityId, u32)>,
    /// Use item
    pub use_item: Option<extern "C" fn(*mut c_void, FfiEntityId, u32) -> bool>,
    /// Drop item in world
    pub drop_item: Option<extern "C" fn(*mut c_void, FfiEntityId, u32, u32, FfiVec3) -> FfiEntityId>,

    // ========== AI/Navigation System (doc 29) ==========
    /// Get AI state
    pub get_ai_state: Option<extern "C" fn(*mut c_void, FfiEntityId) -> FfiAiState>,
    /// Set AI state
    pub set_ai_state: Option<extern "C" fn(*mut c_void, FfiEntityId, u32)>,
    /// Set AI target entity
    pub set_ai_target: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiEntityId)>,
    /// Set AI target position
    pub set_ai_target_position: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiVec3)>,
    /// Clear AI target
    pub clear_ai_target: Option<extern "C" fn(*mut c_void, FfiEntityId)>,
    /// Find path to position
    pub find_path: Option<extern "C" fn(*mut c_void, FfiVec3, FfiVec3) -> FfiNavPath>,
    /// Move AI to position
    pub ai_move_to: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiVec3) -> bool>,
    /// Stop AI movement
    pub ai_stop: Option<extern "C" fn(*mut c_void, FfiEntityId)>,
    /// Find nearest cover point
    pub find_cover: Option<extern "C" fn(*mut c_void, FfiVec3, FfiVec3, f32) -> FfiCoverPoint>,
    /// Alert nearby AI
    pub alert_nearby: Option<extern "C" fn(*mut c_void, FfiVec3, f32, FfiEntityId)>,

    // ========== State System (doc 27) ==========
    /// Get integer state variable
    pub get_state_int: Option<extern "C" fn(*mut c_void, *const c_char) -> i64>,
    /// Set integer state variable
    pub set_state_int: Option<extern "C" fn(*mut c_void, *const c_char, i64)>,
    /// Get float state variable
    pub get_state_float: Option<extern "C" fn(*mut c_void, *const c_char) -> f64>,
    /// Set float state variable
    pub set_state_float: Option<extern "C" fn(*mut c_void, *const c_char, f64)>,
    /// Get bool state variable
    pub get_state_bool: Option<extern "C" fn(*mut c_void, *const c_char) -> bool>,
    /// Set bool state variable
    pub set_state_bool: Option<extern "C" fn(*mut c_void, *const c_char, bool)>,
    /// Save game
    pub save_game: Option<extern "C" fn(*mut c_void, *const c_char) -> bool>,
    /// Load game
    pub load_game: Option<extern "C" fn(*mut c_void, *const c_char) -> bool>,
    /// Get quest info
    pub get_quest_info: Option<extern "C" fn(*mut c_void, u32) -> FfiQuestInfo>,
    /// Start quest
    pub start_quest: Option<extern "C" fn(*mut c_void, u32)>,
    /// Complete quest
    pub complete_quest: Option<extern "C" fn(*mut c_void, u32)>,
    /// Fail quest
    pub fail_quest: Option<extern "C" fn(*mut c_void, u32)>,
    /// Update objective progress
    pub update_objective: Option<extern "C" fn(*mut c_void, u32, u32, u32)>,
    /// Complete objective
    pub complete_objective: Option<extern "C" fn(*mut c_void, u32, u32)>,
    /// Unlock achievement
    pub unlock_achievement: Option<extern "C" fn(*mut c_void, u32)>,
    /// Check if achievement unlocked
    pub is_achievement_unlocked: Option<extern "C" fn(*mut c_void, u32) -> bool>,

    // ========== UI/HUD System (doc 28) ==========
    /// Show HUD element
    pub show_hud_element: Option<extern "C" fn(*mut c_void, *const c_char)>,
    /// Hide HUD element
    pub hide_hud_element: Option<extern "C" fn(*mut c_void, *const c_char)>,
    /// Set HUD element visibility
    pub set_hud_visibility: Option<extern "C" fn(*mut c_void, *const c_char, bool)>,
    /// Update HUD element value (for progress bars, etc.)
    pub set_hud_value: Option<extern "C" fn(*mut c_void, *const c_char, f32)>,
    /// Update HUD element text
    pub set_hud_text: Option<extern "C" fn(*mut c_void, *const c_char, *const c_char)>,
    /// Show notification
    pub show_notification: Option<extern "C" fn(*mut c_void, *const c_char, f32)>,
    /// Show damage number
    pub show_damage_number: Option<extern "C" fn(*mut c_void, FfiVec3, f32, bool, FfiColor)>,
    /// Show interaction prompt
    pub show_interaction_prompt: Option<extern "C" fn(*mut c_void, *const c_char, *const c_char)>,
    /// Hide interaction prompt
    pub hide_interaction_prompt: Option<extern "C" fn(*mut c_void)>,
    /// Open menu
    pub open_menu: Option<extern "C" fn(*mut c_void, *const c_char)>,
    /// Close menu
    pub close_menu: Option<extern "C" fn(*mut c_void, *const c_char)>,
    /// Start dialogue
    pub start_dialogue: Option<extern "C" fn(*mut c_void, *const c_char, FfiEntityId)>,
    /// End dialogue
    pub end_dialogue: Option<extern "C" fn(*mut c_void)>,

    // ========== Audio System (doc 26) ==========
    /// Play sound with parameters (returns handle)
    pub play_sound_ex: Option<extern "C" fn(*mut c_void, *const c_char, FfiAudioParams) -> FfiSoundHandle>,
    /// Stop sound
    pub stop_sound: Option<extern "C" fn(*mut c_void, FfiSoundHandle)>,
    /// Set sound volume
    pub set_sound_volume: Option<extern "C" fn(*mut c_void, FfiSoundHandle, f32)>,
    /// Set sound pitch
    pub set_sound_pitch: Option<extern "C" fn(*mut c_void, FfiSoundHandle, f32)>,
    /// Set sound position (for 3D sounds)
    pub set_sound_position: Option<extern "C" fn(*mut c_void, FfiSoundHandle, FfiVec3)>,
    /// Play music track
    pub play_music: Option<extern "C" fn(*mut c_void, *const c_char, f32)>,
    /// Stop music
    pub stop_music: Option<extern "C" fn(*mut c_void)>,
    /// Set music volume
    pub set_music_volume: Option<extern "C" fn(*mut c_void, f32)>,
    /// Crossfade to new music
    pub crossfade_music: Option<extern "C" fn(*mut c_void, *const c_char, f32)>,

    // ========== Physics Extended (doc 23) ==========
    /// Sphere cast
    pub sphere_cast: Option<extern "C" fn(*mut c_void, FfiVec3, FfiVec3, f32, f32) -> FfiHitResult>,
    /// Box cast
    pub box_cast: Option<extern "C" fn(*mut c_void, FfiVec3, FfiVec3, FfiVec3, FfiQuat, f32) -> FfiHitResult>,
    /// Overlap sphere (returns count, fills array)
    pub overlap_sphere: Option<extern "C" fn(*mut c_void, FfiVec3, f32, *mut FfiEntityId, u32) -> u32>,
    /// Set entity velocity
    pub get_velocity: Option<extern "C" fn(*mut c_void, FfiEntityId) -> FfiVec3>,
    /// Apply force at position
    pub apply_force_at: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiVec3, FfiVec3)>,
    /// Set gravity scale
    pub set_gravity_scale: Option<extern "C" fn(*mut c_void, FfiEntityId, f32)>,
    /// Set physics enabled
    pub set_physics_enabled: Option<extern "C" fn(*mut c_void, FfiEntityId, bool)>,

    // ========== Triggers Extended (doc 22) ==========
    /// Check if entity is in trigger
    pub is_entity_in_trigger: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiEntityId) -> bool>,
    /// Get entities in trigger (returns count, fills array)
    pub get_entities_in_trigger: Option<extern "C" fn(*mut c_void, FfiEntityId, *mut FfiEntityId, u32) -> u32>,
    /// Enable trigger
    pub enable_trigger: Option<extern "C" fn(*mut c_void, FfiEntityId)>,
    /// Disable trigger
    pub disable_trigger: Option<extern "C" fn(*mut c_void, FfiEntityId)>,
    /// Reset trigger (for one-shot triggers)
    pub reset_trigger: Option<extern "C" fn(*mut c_void, FfiEntityId)>,

    // ========== Entity Extended ==========
    /// Get entity by name
    pub get_entity_by_name: Option<extern "C" fn(*mut c_void, *const c_char) -> FfiEntityId>,
    /// Get entity name
    pub get_entity_name: Option<extern "C" fn(*mut c_void, FfiEntityId, *mut c_char, u32) -> u32>,
    /// Check if entity has tag
    pub entity_has_tag: Option<extern "C" fn(*mut c_void, FfiEntityId, *const c_char) -> bool>,
    /// Get entities with tag (returns count, fills array)
    pub get_entities_with_tag: Option<extern "C" fn(*mut c_void, *const c_char, *mut FfiEntityId, u32) -> u32>,
    /// Get distance between entities
    pub get_distance: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiEntityId) -> f32>,
    /// Check line of sight
    pub has_line_of_sight: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiEntityId) -> bool>,
    /// Get entity transform
    pub get_entity_transform: Option<extern "C" fn(*mut c_void, FfiEntityId) -> FfiTransform>,
    /// Set entity transform
    pub set_entity_transform: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiTransform)>,
    /// Get entity scale
    pub get_entity_scale: Option<extern "C" fn(*mut c_void, FfiEntityId) -> FfiVec3>,
    /// Set entity scale
    pub set_entity_scale: Option<extern "C" fn(*mut c_void, FfiEntityId, FfiVec3)>,
    /// Set entity enabled
    pub set_entity_enabled: Option<extern "C" fn(*mut c_void, FfiEntityId, bool)>,
    /// Is entity enabled
    pub is_entity_enabled: Option<extern "C" fn(*mut c_void, FfiEntityId) -> bool>,

    // ========== Time/Game ==========
    /// Get delta time
    pub get_delta_time: Option<extern "C" fn(*mut c_void) -> f32>,
    /// Get total time
    pub get_total_time: Option<extern "C" fn(*mut c_void) -> f64>,
    /// Get time scale
    pub get_time_scale: Option<extern "C" fn(*mut c_void) -> f32>,
    /// Set time scale
    pub set_time_scale: Option<extern "C" fn(*mut c_void, f32)>,
    /// Load scene
    pub load_scene: Option<extern "C" fn(*mut c_void, *const c_char)>,
}

impl Default for FfiExtendedWorldContext {
    fn default() -> Self {
        Self {
            base: FfiWorldContext::default(),
            // Combat
            get_health_info: None,
            apply_damage: None,
            heal_entity: None,
            set_invulnerable: None,
            apply_status_effect: None,
            remove_status_effect: None,
            has_status_effect: None,
            get_weapon_info: None,
            fire_weapon: None,
            reload_weapon: None,
            // Inventory
            get_inventory_info: None,
            add_item: None,
            remove_item: None,
            has_item: None,
            get_item_count: None,
            equip_item: None,
            unequip_item: None,
            use_item: None,
            drop_item: None,
            // AI/Navigation
            get_ai_state: None,
            set_ai_state: None,
            set_ai_target: None,
            set_ai_target_position: None,
            clear_ai_target: None,
            find_path: None,
            ai_move_to: None,
            ai_stop: None,
            find_cover: None,
            alert_nearby: None,
            // State
            get_state_int: None,
            set_state_int: None,
            get_state_float: None,
            set_state_float: None,
            get_state_bool: None,
            set_state_bool: None,
            save_game: None,
            load_game: None,
            get_quest_info: None,
            start_quest: None,
            complete_quest: None,
            fail_quest: None,
            update_objective: None,
            complete_objective: None,
            unlock_achievement: None,
            is_achievement_unlocked: None,
            // UI/HUD
            show_hud_element: None,
            hide_hud_element: None,
            set_hud_visibility: None,
            set_hud_value: None,
            set_hud_text: None,
            show_notification: None,
            show_damage_number: None,
            show_interaction_prompt: None,
            hide_interaction_prompt: None,
            open_menu: None,
            close_menu: None,
            start_dialogue: None,
            end_dialogue: None,
            // Audio
            play_sound_ex: None,
            stop_sound: None,
            set_sound_volume: None,
            set_sound_pitch: None,
            set_sound_position: None,
            play_music: None,
            stop_music: None,
            set_music_volume: None,
            crossfade_music: None,
            // Physics Extended
            sphere_cast: None,
            box_cast: None,
            overlap_sphere: None,
            get_velocity: None,
            apply_force_at: None,
            set_gravity_scale: None,
            set_physics_enabled: None,
            // Triggers Extended
            is_entity_in_trigger: None,
            get_entities_in_trigger: None,
            enable_trigger: None,
            disable_trigger: None,
            reset_trigger: None,
            // Entity Extended
            get_entity_by_name: None,
            get_entity_name: None,
            entity_has_tag: None,
            get_entities_with_tag: None,
            get_distance: None,
            has_line_of_sight: None,
            get_entity_transform: None,
            set_entity_transform: None,
            get_entity_scale: None,
            set_entity_scale: None,
            set_entity_enabled: None,
            is_entity_enabled: None,
            // Time/Game
            get_delta_time: None,
            get_total_time: None,
            get_time_scale: None,
            set_time_scale: None,
            load_scene: None,
        }
    }
}

/// Type alias for getting extended vtable
pub type GetExtendedVTableFn = extern "C" fn(*const c_char) -> *const FfiExtendedVTable;

/// Set the extended world context for a C++ instance
pub type SetExtendedWorldContextFn = extern "C" fn(CppHandle, *const FfiExtendedWorldContext);
