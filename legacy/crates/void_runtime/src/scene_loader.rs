//! Scene Loader - Parses declarative scene.toml files
//!
//! This module enables hot-swappable scene content by loading entities,
//! materials, particle emitters, and other scene elements from TOML files
//! instead of hardcoded Rust.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::Path;

// Re-export game systems types for scene definitions
pub use crate::game_systems::{
    // Scripting (doc 21)
    CppClassDef, BlueprintDef, ScriptValue, EventBindingDef,
    // Triggers (doc 22)
    TriggerDefinition, TriggerVolumeDef, TriggerModeDef, TriggerFilterDef, TriggerActionDef,
    // Physics (doc 23)
    FullPhysicsDef, PhysicsBodyType, ColliderDef, ColliderShapeDef, CapsuleAxis,
    CompoundShapePart, PhysicsMaterialDef, CombineMode, CollisionGroupsDef, JointDef,
    MotorDef, CharacterControllerDef,
    // Combat (doc 24)
    HealthDef, WeaponDef, WeaponType, HitscanDef, FalloffDef, ProjectileDef, HomingDef,
    ExplosionDef, MeleeDef, ComboAttackDef, AmmoDef, RecoilDef, WeaponSoundsDef,
    WeaponEffectsDef, StatusEffectDef, StatusEffectType,
    // Inventory (doc 25)
    ItemDef, ItemType, ItemRarity, ConsumableDef, EquipmentDef, EquipmentSlot,
    UsableDef, InventoryDef, StartingItemDef, PickupDef, LootTableDef, LootEntryDef,
    VendorDef, VendorItemDef,
    // Audio (doc 26)
    AudioConfigDef, AmbientSoundDef, MusicDef, MusicTrackDef, ReverbZoneDef,
    ReverbPreset, EntityAudioDef, FootstepsDef, VoiceDef,
    // State (doc 27)
    StateConfigDef, StateVariableDef, StateVarType, SaveConfigDef, CheckpointDef,
    QuestDef, QuestObjectiveDef, ObjectiveType, QuestRewardDef, AchievementDef,
    AchievementConditionDef,
    // UI/HUD (doc 28)
    UiConfigDef, HudConfigDef, HudElementDef, HudElementType, ProgressDirection,
    AnchorPoint, HudStyleDef, BorderDef, MinimapIconDef, CompassMarkerDef,
    DamageNumbersDef, InteractionPromptDef, NameplatesDef, CrosshairDef, HitMarkerDef,
    MenuDef, MenuLayoutDef, MenuItemDef, DialogDef, DialogChoiceDef, NotificationConfigDef,
    // AI/Navigation (doc 29)
    NavigationConfigDef, NavMeshConfigDef, NavMeshSource, NavAreaDef, AiComponentDef,
    AiTypeDef, AiStateDef, AiTransitionDef, AiConditionDef, AiActionDef, AiTargetDef,
    AiConsiderationDef, AiEvaluatorDef, UtilityCurve, AiSensesDef, AiSightDef,
    AiHearingDef, AiDamageSenseDef, AiTouchSenseDef, AiMovementDef, WaypointPathDef,
    WaypointDef, CoverPointDef, CoverType, AiGroupDef, GroupBehavior, FormationDef,
    FormationType,
    // Aggregate types
    GameSceneDefinition, SceneScriptingDef, PhysicsConfigDef, GameEntityComponents,
};

// ============================================================================
// Scene Definition Structures
// ============================================================================

/// Root scene definition loaded from scene.toml
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct SceneDefinition {
    /// Scene metadata
    #[serde(default)]
    pub scene: SceneMetadata,

    /// Camera definitions
    #[serde(default)]
    pub cameras: Vec<CameraDef>,

    /// Light definitions
    #[serde(default)]
    pub lights: Vec<LightDef>,

    /// Shadow configuration
    #[serde(default)]
    pub shadows: ShadowsDef,

    /// Environment settings (sky, lighting, etc.)
    #[serde(default)]
    pub environment: EnvironmentDef,

    /// Picking configuration
    #[serde(default)]
    pub picking: PickingDef,

    /// Spatial query configuration
    #[serde(default)]
    pub spatial: SpatialDef,

    /// Debug configuration
    #[serde(default)]
    pub debug: DebugDef,

    /// Entity definitions
    #[serde(default)]
    pub entities: Vec<EntityDef>,

    /// Particle emitter definitions
    #[serde(default)]
    pub particle_emitters: Vec<ParticleEmitterDef>,

    /// Texture definitions (for preloading)
    #[serde(default)]
    pub textures: Vec<TextureDef>,

    /// Input configuration
    #[serde(default)]
    pub input: InputConfig,

    // ========== Game Systems (docs 21-29) ==========

    /// Scene-level scripting configuration (doc 21)
    #[serde(default)]
    pub scripting: Option<SceneScriptingDef>,

    /// Scene-level physics configuration (doc 23)
    #[serde(default)]
    pub physics_config: Option<PhysicsConfigDef>,

    /// Scene-level audio configuration (doc 26)
    #[serde(default)]
    pub audio: Option<AudioConfigDef>,

    /// Scene-level state configuration (doc 27)
    #[serde(default)]
    pub state: Option<StateConfigDef>,

    /// Scene-level UI configuration (doc 28)
    #[serde(default)]
    pub ui: Option<UiConfigDef>,

    /// Scene-level navigation configuration (doc 29)
    #[serde(default)]
    pub navigation: Option<NavigationConfigDef>,

    /// Item definitions (doc 25)
    #[serde(default)]
    pub items: Vec<ItemDef>,

    /// Loot table definitions (doc 25)
    #[serde(default)]
    pub loot_tables: Vec<LootTableDef>,

    /// Quest definitions (doc 27)
    #[serde(default)]
    pub quests: Vec<QuestDef>,

    /// Achievement definitions (doc 27)
    #[serde(default)]
    pub achievements: Vec<AchievementDef>,

    /// Status effect definitions (doc 24)
    #[serde(default)]
    pub status_effects: Vec<StatusEffectDef>,
}

/// Scene metadata
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct SceneMetadata {
    pub name: String,
    #[serde(default)]
    pub description: String,
    #[serde(default)]
    pub version: String,
}

impl Default for SceneMetadata {
    fn default() -> Self {
        Self {
            name: "Untitled Scene".to_string(),
            description: String::new(),
            version: "1.0.0".to_string(),
        }
    }
}

// ============================================================================
// Camera System (Phase 2)
// ============================================================================

/// Camera definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CameraDef {
    /// Camera name/ID
    pub name: String,

    /// Whether this camera is active
    #[serde(default)]
    pub active: bool,

    /// Camera type
    #[serde(rename = "type", default)]
    pub camera_type: CameraType,

    /// Camera control mode (fps, orbit, fly)
    #[serde(default)]
    pub control_mode: CameraControlMode,

    /// Camera transform
    #[serde(default)]
    pub transform: CameraTransformDef,

    /// Perspective projection settings
    #[serde(default)]
    pub perspective: Option<PerspectiveDef>,

    /// Orthographic projection settings
    #[serde(default)]
    pub orthographic: Option<OrthographicDef>,
}

/// Camera control mode
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum CameraControlMode {
    /// First-person shooter style (WASD + mouse look)
    Fps,
    /// Orbit around a target point (default for editors/viewers)
    #[default]
    Orbit,
    /// Free fly mode (6DOF)
    Fly,
}

/// Camera type
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum CameraType {
    #[default]
    Perspective,
    Orthographic,
}

/// Camera transform
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CameraTransformDef {
    /// Camera position [x, y, z]
    #[serde(default = "default_camera_position")]
    pub position: [f32; 3],

    /// Look-at target [x, y, z]
    #[serde(default)]
    pub target: [f32; 3],

    /// Up vector [x, y, z]
    #[serde(default = "default_up_vector")]
    pub up: [f32; 3],
}

fn default_camera_position() -> [f32; 3] { [0.0, 2.0, 5.0] }
fn default_up_vector() -> [f32; 3] { [0.0, 1.0, 0.0] }

impl Default for CameraTransformDef {
    fn default() -> Self {
        Self {
            position: default_camera_position(),
            target: [0.0, 0.0, 0.0],
            up: default_up_vector(),
        }
    }
}

/// Perspective projection settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PerspectiveDef {
    /// Field of view in degrees
    #[serde(default = "default_fov")]
    pub fov: f32,

    /// Near clip plane
    #[serde(default = "default_near")]
    pub near: f32,

    /// Far clip plane
    #[serde(default = "default_far")]
    pub far: f32,

    /// Aspect ratio ("auto" or a number)
    #[serde(default)]
    pub aspect: AspectRatio,
}

fn default_fov() -> f32 { 60.0 }
fn default_near() -> f32 { 0.1 }
fn default_far() -> f32 { 1000.0 }

impl Default for PerspectiveDef {
    fn default() -> Self {
        Self {
            fov: default_fov(),
            near: default_near(),
            far: default_far(),
            aspect: AspectRatio::Auto,
        }
    }
}

/// Aspect ratio - auto or fixed
#[derive(Debug, Clone, Serialize)]
pub enum AspectRatio {
    Auto,
    Fixed(f32),
}

impl Default for AspectRatio {
    fn default() -> Self {
        AspectRatio::Auto
    }
}

impl<'de> serde::Deserialize<'de> for AspectRatio {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use serde::de::{self, Visitor};

        struct AspectRatioVisitor;

        impl<'de> Visitor<'de> for AspectRatioVisitor {
            type Value = AspectRatio;

            fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
                formatter.write_str("\"auto\" or a number")
            }

            fn visit_str<E>(self, value: &str) -> Result<AspectRatio, E>
            where
                E: de::Error,
            {
                if value.eq_ignore_ascii_case("auto") {
                    Ok(AspectRatio::Auto)
                } else {
                    // Try to parse as number
                    value.parse::<f32>()
                        .map(AspectRatio::Fixed)
                        .map_err(|_| de::Error::custom(format!("invalid aspect ratio: {}", value)))
                }
            }

            fn visit_f64<E>(self, value: f64) -> Result<AspectRatio, E>
            where
                E: de::Error,
            {
                Ok(AspectRatio::Fixed(value as f32))
            }

            fn visit_i64<E>(self, value: i64) -> Result<AspectRatio, E>
            where
                E: de::Error,
            {
                Ok(AspectRatio::Fixed(value as f32))
            }

            fn visit_u64<E>(self, value: u64) -> Result<AspectRatio, E>
            where
                E: de::Error,
            {
                Ok(AspectRatio::Fixed(value as f32))
            }
        }

        deserializer.deserialize_any(AspectRatioVisitor)
    }
}

/// Orthographic projection settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct OrthographicDef {
    pub left: f32,
    pub right: f32,
    pub bottom: f32,
    pub top: f32,
    #[serde(default = "default_near")]
    pub near: f32,
    #[serde(default = "default_far")]
    pub far: f32,
}

impl Default for OrthographicDef {
    fn default() -> Self {
        Self {
            left: -10.0,
            right: 10.0,
            bottom: -10.0,
            top: 10.0,
            near: 0.1,
            far: 100.0,
        }
    }
}

// ============================================================================
// Lighting System (Phase 5)
// ============================================================================

/// Light definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct LightDef {
    /// Light name
    pub name: String,

    /// Light type
    #[serde(rename = "type")]
    pub light_type: LightType,

    /// Whether light is enabled
    #[serde(default = "default_true")]
    pub enabled: bool,

    /// Directional light settings
    #[serde(default)]
    pub directional: Option<DirectionalLightDef>,

    /// Point light settings
    #[serde(default)]
    pub point: Option<PointLightDef>,

    /// Spot light settings
    #[serde(default)]
    pub spot: Option<SpotLightDef>,
}

/// Light type
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum LightType {
    Directional,
    Point,
    Spot,
    Area,
}

/// Directional light settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DirectionalLightDef {
    /// Light direction [x, y, z]
    #[serde(default = "default_light_direction")]
    pub direction: [f32; 3],

    /// Light color [r, g, b]
    #[serde(default = "default_white")]
    pub color: [f32; 3],

    /// Light intensity
    #[serde(default = "default_light_intensity")]
    pub intensity: f32,

    /// Whether this light casts shadows
    #[serde(default)]
    pub cast_shadows: bool,
}

fn default_white() -> [f32; 3] { [1.0, 1.0, 1.0] }

impl Default for DirectionalLightDef {
    fn default() -> Self {
        Self {
            direction: default_light_direction(),
            color: default_white(),
            intensity: 1.0,
            cast_shadows: false,
        }
    }
}

/// Point light settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PointLightDef {
    /// Light position [x, y, z]
    #[serde(default)]
    pub position: [f32; 3],

    /// Light color [r, g, b]
    #[serde(default = "default_white")]
    pub color: [f32; 3],

    /// Light intensity
    #[serde(default = "default_point_intensity")]
    pub intensity: f32,

    /// Light range/radius
    #[serde(default = "default_light_range")]
    pub range: f32,

    /// Whether this light casts shadows
    #[serde(default)]
    pub cast_shadows: bool,

    /// Attenuation settings
    #[serde(default)]
    pub attenuation: AttenuationDef,
}

fn default_point_intensity() -> f32 { 100.0 }
fn default_light_range() -> f32 { 10.0 }

impl Default for PointLightDef {
    fn default() -> Self {
        Self {
            position: [0.0, 2.0, 0.0],
            color: default_white(),
            intensity: default_point_intensity(),
            range: default_light_range(),
            cast_shadows: false,
            attenuation: AttenuationDef::default(),
        }
    }
}

/// Light attenuation parameters
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AttenuationDef {
    pub constant: f32,
    pub linear: f32,
    pub quadratic: f32,
}

impl Default for AttenuationDef {
    fn default() -> Self {
        Self {
            constant: 1.0,
            linear: 0.09,
            quadratic: 0.032,
        }
    }
}

/// Spot light settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct SpotLightDef {
    /// Light position [x, y, z]
    #[serde(default)]
    pub position: [f32; 3],

    /// Light direction [x, y, z]
    #[serde(default = "default_spot_direction")]
    pub direction: [f32; 3],

    /// Light color [r, g, b]
    #[serde(default = "default_white")]
    pub color: [f32; 3],

    /// Light intensity
    #[serde(default = "default_spot_intensity")]
    pub intensity: f32,

    /// Light range
    #[serde(default = "default_light_range")]
    pub range: f32,

    /// Inner cone angle in degrees
    #[serde(default = "default_inner_angle")]
    pub inner_angle: f32,

    /// Outer cone angle in degrees
    #[serde(default = "default_outer_angle")]
    pub outer_angle: f32,

    /// Whether this light casts shadows
    #[serde(default)]
    pub cast_shadows: bool,
}

fn default_spot_direction() -> [f32; 3] { [0.0, -1.0, 0.0] }
fn default_spot_intensity() -> f32 { 200.0 }
fn default_inner_angle() -> f32 { 20.0 }
fn default_outer_angle() -> f32 { 35.0 }

impl Default for SpotLightDef {
    fn default() -> Self {
        Self {
            position: [0.0, 5.0, 0.0],
            direction: default_spot_direction(),
            color: default_white(),
            intensity: default_spot_intensity(),
            range: default_light_range(),
            inner_angle: default_inner_angle(),
            outer_angle: default_outer_angle(),
            cast_shadows: false,
        }
    }
}

// ============================================================================
// Shadow System (Phase 6)
// ============================================================================

/// Shadow configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ShadowsDef {
    /// Enable shadows
    #[serde(default = "default_true")]
    pub enabled: bool,

    /// Shadow atlas size
    #[serde(default = "default_atlas_size")]
    pub atlas_size: u32,

    /// Maximum shadow distance
    #[serde(default = "default_shadow_distance")]
    pub max_shadow_distance: f32,

    /// Shadow fade distance
    #[serde(default = "default_shadow_fade")]
    pub shadow_fade_distance: f32,

    /// Cascade settings
    #[serde(default)]
    pub cascades: CascadesDef,

    /// Shadow filtering settings
    #[serde(default)]
    pub filtering: ShadowFilteringDef,
}

fn default_atlas_size() -> u32 { 4096 }
fn default_shadow_distance() -> f32 { 50.0 }
fn default_shadow_fade() -> f32 { 5.0 }

impl Default for ShadowsDef {
    fn default() -> Self {
        Self {
            enabled: true,
            atlas_size: default_atlas_size(),
            max_shadow_distance: default_shadow_distance(),
            shadow_fade_distance: default_shadow_fade(),
            cascades: CascadesDef::default(),
            filtering: ShadowFilteringDef::default(),
        }
    }
}

/// Cascade shadow map settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CascadesDef {
    /// Number of cascades
    #[serde(default = "default_cascade_count")]
    pub count: u32,

    /// Split scheme
    #[serde(default)]
    pub split_scheme: CascadeSplitScheme,

    /// Lambda for practical split scheme
    #[serde(default = "default_lambda")]
    pub lambda: f32,

    /// Individual cascade levels
    #[serde(default)]
    pub levels: Vec<CascadeLevelDef>,
}

fn default_cascade_count() -> u32 { 3 }
fn default_lambda() -> f32 { 0.5 }

impl Default for CascadesDef {
    fn default() -> Self {
        Self {
            count: default_cascade_count(),
            split_scheme: CascadeSplitScheme::Practical,
            lambda: default_lambda(),
            levels: vec![],
        }
    }
}

/// Cascade split scheme
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum CascadeSplitScheme {
    /// Linear split
    Linear,
    /// Logarithmic split
    Logarithmic,
    /// Practical split (blend of linear and log)
    #[default]
    Practical,
}

/// Individual cascade level settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CascadeLevelDef {
    /// Resolution for this cascade
    #[serde(default = "default_cascade_resolution")]
    pub resolution: u32,

    /// Distance at which this cascade ends
    pub distance: f32,

    /// Shadow bias for this cascade
    #[serde(default = "default_shadow_bias")]
    pub bias: f32,
}

fn default_cascade_resolution() -> u32 { 1024 }
fn default_shadow_bias() -> f32 { 0.001 }

/// Shadow filtering settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ShadowFilteringDef {
    /// Filtering method
    #[serde(default)]
    pub method: ShadowFilterMethod,

    /// PCF sample count
    #[serde(default = "default_pcf_samples")]
    pub pcf_samples: u32,

    /// PCF radius
    #[serde(default = "default_pcf_radius")]
    pub pcf_radius: f32,

    /// Enable soft shadows
    #[serde(default)]
    pub soft_shadows: bool,

    /// Enable contact hardening (PCSS)
    #[serde(default)]
    pub contact_hardening: bool,
}

fn default_pcf_samples() -> u32 { 16 }
fn default_pcf_radius() -> f32 { 1.5 }

impl Default for ShadowFilteringDef {
    fn default() -> Self {
        Self {
            method: ShadowFilterMethod::Pcf,
            pcf_samples: default_pcf_samples(),
            pcf_radius: default_pcf_radius(),
            soft_shadows: true,
            contact_hardening: false,
        }
    }
}

/// Shadow filtering method
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum ShadowFilterMethod {
    /// No filtering (hard shadows)
    Hard,
    /// Percentage closer filtering
    #[default]
    Pcf,
    /// Variance shadow maps
    Vsm,
    /// Exponential shadow maps
    Esm,
}

// ============================================================================
// Picking System (Phase 10)
// ============================================================================

/// Picking configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PickingDef {
    /// Enable picking
    #[serde(default = "default_true")]
    pub enabled: bool,

    /// Picking method
    #[serde(default)]
    pub method: PickingMethod,

    /// Maximum picking distance
    #[serde(default = "default_pick_distance")]
    pub max_distance: f32,

    /// Layer mask for picking
    #[serde(default)]
    pub layer_mask: Vec<String>,

    /// GPU picking settings
    #[serde(default)]
    pub gpu: GpuPickingDef,
}

fn default_pick_distance() -> f32 { 100.0 }

impl Default for PickingDef {
    fn default() -> Self {
        Self {
            enabled: true,
            method: PickingMethod::Gpu,
            max_distance: default_pick_distance(),
            layer_mask: vec!["world".to_string()],
            gpu: GpuPickingDef::default(),
        }
    }
}

/// Picking method
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum PickingMethod {
    /// CPU ray casting
    Cpu,
    /// GPU-based picking (render entity IDs)
    #[default]
    Gpu,
    /// Hybrid approach
    Hybrid,
}

/// GPU picking settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct GpuPickingDef {
    /// Picking buffer size [width, height]
    #[serde(default = "default_pick_buffer_size")]
    pub buffer_size: [u32; 2],

    /// Frames to delay readback
    #[serde(default = "default_readback_delay")]
    pub readback_delay: u32,
}

fn default_pick_buffer_size() -> [u32; 2] { [256, 256] }
fn default_readback_delay() -> u32 { 1 }

impl Default for GpuPickingDef {
    fn default() -> Self {
        Self {
            buffer_size: default_pick_buffer_size(),
            readback_delay: default_readback_delay(),
        }
    }
}

// ============================================================================
// Spatial Query System (Phase 14)
// ============================================================================

/// Spatial query configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct SpatialDef {
    /// Spatial structure type
    #[serde(default)]
    pub structure: SpatialStructure,

    /// Auto-rebuild on changes
    #[serde(default = "default_true")]
    pub auto_rebuild: bool,

    /// Rebuild threshold (fraction of changed entities)
    #[serde(default = "default_rebuild_threshold")]
    pub rebuild_threshold: f32,

    /// BVH settings
    #[serde(default)]
    pub bvh: BvhDef,

    /// Query settings
    #[serde(default)]
    pub queries: SpatialQueriesDef,
}

fn default_rebuild_threshold() -> f32 { 0.3 }

impl Default for SpatialDef {
    fn default() -> Self {
        Self {
            structure: SpatialStructure::Bvh,
            auto_rebuild: true,
            rebuild_threshold: default_rebuild_threshold(),
            bvh: BvhDef::default(),
            queries: SpatialQueriesDef::default(),
        }
    }
}

/// Spatial structure type
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum SpatialStructure {
    /// Bounding Volume Hierarchy
    #[default]
    Bvh,
    /// Octree
    Octree,
    /// Grid
    Grid,
}

/// BVH settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct BvhDef {
    /// Maximum objects per leaf
    #[serde(default = "default_max_leaf_size")]
    pub max_leaf_size: u32,

    /// Build quality
    #[serde(default)]
    pub build_quality: BvhBuildQuality,
}

fn default_max_leaf_size() -> u32 { 4 }

impl Default for BvhDef {
    fn default() -> Self {
        Self {
            max_leaf_size: default_max_leaf_size(),
            build_quality: BvhBuildQuality::Medium,
        }
    }
}

/// BVH build quality
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum BvhBuildQuality {
    /// Fast build, lower query performance
    Fast,
    /// Balanced
    #[default]
    Medium,
    /// Slow build, best query performance
    High,
}

/// Spatial query settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct SpatialQueriesDef {
    /// Enable frustum culling
    #[serde(default = "default_true")]
    pub frustum_culling: bool,

    /// Enable occlusion culling
    #[serde(default)]
    pub occlusion_culling: bool,

    /// Maximum query results
    #[serde(default = "default_max_query_results")]
    pub max_query_results: u32,
}

fn default_max_query_results() -> u32 { 500 }

impl Default for SpatialQueriesDef {
    fn default() -> Self {
        Self {
            frustum_culling: true,
            occlusion_culling: false,
            max_query_results: default_max_query_results(),
        }
    }
}

// ============================================================================
// Debug System (Phase 18)
// ============================================================================

/// Debug configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DebugDef {
    /// Enable debug features
    #[serde(default)]
    pub enabled: bool,

    /// Stats overlay settings
    #[serde(default)]
    pub stats: DebugStatsDef,

    /// Visualization settings
    #[serde(default)]
    pub visualization: DebugVisualizationDef,

    /// Debug controls
    #[serde(default)]
    pub controls: DebugControlsDef,
}

impl Default for DebugDef {
    fn default() -> Self {
        Self {
            enabled: false,
            stats: DebugStatsDef::default(),
            visualization: DebugVisualizationDef::default(),
            controls: DebugControlsDef::default(),
        }
    }
}

/// Debug stats overlay
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DebugStatsDef {
    /// Enable stats display
    #[serde(default)]
    pub enabled: bool,

    /// Position on screen
    #[serde(default)]
    pub position: DebugPosition,

    /// Font size
    #[serde(default = "default_font_size")]
    pub font_size: u32,

    /// Background alpha
    #[serde(default = "default_bg_alpha")]
    pub background_alpha: f32,

    /// Which stats to display
    #[serde(default)]
    pub display: StatsDisplayDef,
}

fn default_font_size() -> u32 { 14 }
fn default_bg_alpha() -> f32 { 0.7 }

impl Default for DebugStatsDef {
    fn default() -> Self {
        Self {
            enabled: false,
            position: DebugPosition::TopLeft,
            font_size: default_font_size(),
            background_alpha: default_bg_alpha(),
            display: StatsDisplayDef::default(),
        }
    }
}

/// Debug overlay position
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum DebugPosition {
    #[default]
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
}

/// Which stats to display
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct StatsDisplayDef {
    #[serde(default = "default_true")]
    pub fps: bool,
    #[serde(default = "default_true")]
    pub frame_time: bool,
    #[serde(default = "default_true")]
    pub draw_calls: bool,
    #[serde(default = "default_true")]
    pub triangles: bool,
    #[serde(default = "default_true")]
    pub entities_total: bool,
    #[serde(default = "default_true")]
    pub entities_visible: bool,
    #[serde(default)]
    pub gpu_memory: bool,
    #[serde(default = "default_true")]
    pub cpu_time: bool,
}

impl Default for StatsDisplayDef {
    fn default() -> Self {
        Self {
            fps: true,
            frame_time: true,
            draw_calls: true,
            triangles: true,
            entities_total: true,
            entities_visible: true,
            gpu_memory: false,
            cpu_time: true,
        }
    }
}

/// Debug visualization settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DebugVisualizationDef {
    /// Enable visualization overlays
    #[serde(default)]
    pub enabled: bool,

    /// Show bounding boxes
    #[serde(default)]
    pub bounds: bool,

    /// Wireframe mode
    #[serde(default)]
    pub wireframe: bool,

    /// Show normals
    #[serde(default)]
    pub normals: bool,

    /// Show light volumes
    #[serde(default)]
    pub light_volumes: bool,

    /// Show shadow cascades
    #[serde(default)]
    pub shadow_cascades: bool,

    /// Show LOD levels
    #[serde(default)]
    pub lod_levels: bool,

    /// Show skeleton/bones
    #[serde(default)]
    pub skeleton: bool,

    /// Appearance settings
    #[serde(default)]
    pub appearance: DebugAppearanceDef,
}

impl Default for DebugVisualizationDef {
    fn default() -> Self {
        Self {
            enabled: false,
            bounds: false,
            wireframe: false,
            normals: false,
            light_volumes: false,
            shadow_cascades: false,
            lod_levels: false,
            skeleton: false,
            appearance: DebugAppearanceDef::default(),
        }
    }
}

/// Debug visualization appearance
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DebugAppearanceDef {
    /// Bounds color [r, g, b, a]
    #[serde(default = "default_bounds_color")]
    pub bounds_color: [f32; 4],

    /// Normal color [r, g, b, a]
    #[serde(default = "default_normal_color")]
    pub normal_color: [f32; 4],

    /// Normal visualization length
    #[serde(default = "default_normal_length")]
    pub normal_length: f32,

    /// Line width
    #[serde(default = "default_line_width")]
    pub line_width: f32,
}

fn default_bounds_color() -> [f32; 4] { [0.0, 1.0, 0.0, 0.5] }
fn default_normal_color() -> [f32; 4] { [0.0, 0.5, 1.0, 1.0] }
fn default_normal_length() -> f32 { 0.1 }
fn default_line_width() -> f32 { 1.0 }

impl Default for DebugAppearanceDef {
    fn default() -> Self {
        Self {
            bounds_color: default_bounds_color(),
            normal_color: default_normal_color(),
            normal_length: default_normal_length(),
            line_width: default_line_width(),
        }
    }
}

/// Debug control keys
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DebugControlsDef {
    /// Key to toggle debug overlay
    #[serde(default = "default_toggle_key")]
    pub toggle_key: String,

    /// Key to cycle debug mode
    #[serde(default = "default_cycle_key")]
    pub cycle_mode_key: String,

    /// Key to reload shaders
    #[serde(default = "default_reload_key")]
    pub reload_shaders_key: String,
}

fn default_toggle_key() -> String { "F3".to_string() }
fn default_cycle_key() -> String { "F4".to_string() }
fn default_reload_key() -> String { "F5".to_string() }

impl Default for DebugControlsDef {
    fn default() -> Self {
        Self {
            toggle_key: default_toggle_key(),
            cycle_mode_key: default_cycle_key(),
            reload_shaders_key: default_reload_key(),
        }
    }
}

/// Environment/lighting settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct EnvironmentDef {
    /// HDR environment map path (relative to app assets)
    #[serde(default)]
    pub environment_map: Option<String>,

    /// Sun/directional light direction [x, y, z]
    #[serde(default = "default_light_direction")]
    pub light_direction: [f32; 3],

    /// Light color [r, g, b]
    #[serde(default = "default_light_color")]
    pub light_color: [f32; 3],

    /// Light intensity
    #[serde(default = "default_light_intensity")]
    pub light_intensity: f32,

    /// Ambient light intensity
    #[serde(default = "default_ambient_intensity")]
    pub ambient_intensity: f32,

    /// Sky settings (procedural sky when no environment map)
    #[serde(default)]
    pub sky: SkyDef,
}

fn default_light_direction() -> [f32; 3] { [0.5, -0.7, 0.5] }
fn default_light_color() -> [f32; 3] { [1.0, 0.95, 0.85] }
fn default_light_intensity() -> f32 { 3.0 }
fn default_ambient_intensity() -> f32 { 0.25 }

impl Default for EnvironmentDef {
    fn default() -> Self {
        Self {
            environment_map: None,
            light_direction: default_light_direction(),
            light_color: default_light_color(),
            light_intensity: default_light_intensity(),
            ambient_intensity: default_ambient_intensity(),
            sky: SkyDef::default(),
        }
    }
}

/// Sky configuration for procedural sky rendering
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct SkyDef {
    /// Sky color at zenith (top) [r, g, b]
    #[serde(default = "default_sky_zenith")]
    pub zenith_color: [f32; 3],

    /// Sky color at horizon [r, g, b]
    #[serde(default = "default_sky_horizon")]
    pub horizon_color: [f32; 3],

    /// Ground color (below horizon) [r, g, b]
    #[serde(default = "default_ground_color")]
    pub ground_color: [f32; 3],

    /// Sun disc size (angular radius, 0.0 = no sun disc)
    #[serde(default = "default_sun_size")]
    pub sun_size: f32,

    /// Sun disc intensity multiplier
    #[serde(default = "default_sun_intensity")]
    pub sun_intensity: f32,

    /// Sun halo/glow falloff
    #[serde(default = "default_sun_falloff")]
    pub sun_falloff: f32,

    /// Cloud coverage (0.0 = clear, 1.0 = overcast) - future use
    #[serde(default)]
    pub cloud_coverage: f32,

    /// Fog density at horizon
    #[serde(default)]
    pub fog_density: f32,

    /// Fog color [r, g, b] - defaults to horizon color if not set
    #[serde(default)]
    pub fog_color: Option<[f32; 3]>,
}

fn default_sky_zenith() -> [f32; 3] { [0.1, 0.3, 0.6] }
fn default_sky_horizon() -> [f32; 3] { [0.5, 0.7, 0.9] }
fn default_ground_color() -> [f32; 3] { [0.15, 0.12, 0.1] }
fn default_sun_size() -> f32 { 0.03 }
fn default_sun_intensity() -> f32 { 50.0 }
fn default_sun_falloff() -> f32 { 3.0 }

impl Default for SkyDef {
    fn default() -> Self {
        Self {
            zenith_color: default_sky_zenith(),
            horizon_color: default_sky_horizon(),
            ground_color: default_ground_color(),
            sun_size: default_sun_size(),
            sun_intensity: default_sun_intensity(),
            sun_falloff: default_sun_falloff(),
            cloud_coverage: 0.0,
            fog_density: 0.0,
            fog_color: None,
        }
    }
}

// ============================================================================
// Input Configuration
// ============================================================================

/// Input configuration - allows external configuration of controls
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct InputConfig {
    /// Camera control settings
    #[serde(default)]
    pub camera: CameraInputConfig,

    /// Key bindings
    #[serde(default)]
    pub bindings: HashMap<String, KeyBinding>,
}

impl Default for InputConfig {
    fn default() -> Self {
        let mut bindings = HashMap::new();
        // Default bindings
        bindings.insert("next_material".to_string(), KeyBinding::Key("Tab".to_string()));
        bindings.insert("prev_material".to_string(), KeyBinding::Key("Shift+Tab".to_string()));
        bindings.insert("model_sphere".to_string(), KeyBinding::Key("1".to_string()));
        bindings.insert("model_cube".to_string(), KeyBinding::Key("2".to_string()));
        bindings.insert("model_torus".to_string(), KeyBinding::Key("3".to_string()));
        bindings.insert("model_diamond".to_string(), KeyBinding::Key("4".to_string()));
        bindings.insert("reset_camera".to_string(), KeyBinding::Key("R".to_string()));
        bindings.insert("toggle_wireframe".to_string(), KeyBinding::Key("F".to_string()));
        bindings.insert("toggle_grid".to_string(), KeyBinding::Key("G".to_string()));
        bindings.insert("screenshot".to_string(), KeyBinding::Key("F12".to_string()));

        Self {
            camera: CameraInputConfig::default(),
            bindings,
        }
    }
}

/// Camera input settings
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct CameraInputConfig {
    /// Mouse button for orbit (rotating around target)
    #[serde(default = "default_orbit_button")]
    pub orbit_button: MouseButton,

    /// Mouse button for panning
    #[serde(default = "default_pan_button")]
    pub pan_button: MouseButton,

    /// Enable scroll wheel zoom
    #[serde(default = "default_true")]
    pub zoom_scroll: bool,

    /// Orbit sensitivity (radians per pixel)
    #[serde(default = "default_orbit_sensitivity")]
    pub orbit_sensitivity: f32,

    /// Pan sensitivity (units per pixel)
    #[serde(default = "default_pan_sensitivity")]
    pub pan_sensitivity: f32,

    /// Zoom sensitivity (multiplier per scroll unit)
    #[serde(default = "default_zoom_sensitivity")]
    pub zoom_sensitivity: f32,

    /// Invert Y axis for orbit
    #[serde(default)]
    pub invert_y: bool,

    /// Invert X axis for orbit
    #[serde(default)]
    pub invert_x: bool,

    /// Minimum zoom distance
    #[serde(default = "default_min_distance")]
    pub min_distance: f32,

    /// Maximum zoom distance
    #[serde(default = "default_max_distance")]
    pub max_distance: f32,
}

fn default_orbit_button() -> MouseButton { MouseButton::Left }
fn default_pan_button() -> MouseButton { MouseButton::Middle }
fn default_orbit_sensitivity() -> f32 { 0.005 }
fn default_pan_sensitivity() -> f32 { 0.01 }
fn default_zoom_sensitivity() -> f32 { 0.1 }
fn default_min_distance() -> f32 { 0.5 }
fn default_max_distance() -> f32 { 50.0 }

impl Default for CameraInputConfig {
    fn default() -> Self {
        Self {
            orbit_button: default_orbit_button(),
            pan_button: default_pan_button(),
            zoom_scroll: true,
            orbit_sensitivity: default_orbit_sensitivity(),
            pan_sensitivity: default_pan_sensitivity(),
            zoom_sensitivity: default_zoom_sensitivity(),
            invert_y: false,
            invert_x: false,
            min_distance: default_min_distance(),
            max_distance: default_max_distance(),
        }
    }
}

/// Mouse button enum
#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum MouseButton {
    Left,
    Right,
    Middle,
}

/// Key binding - can be a single key or key combination
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(untagged)]
pub enum KeyBinding {
    Key(String),
    Combo { key: String, modifiers: Vec<String> },
}

impl KeyBinding {
    /// Check if this binding matches the given key and modifiers
    pub fn matches(&self, key: &str, shift: bool, ctrl: bool, alt: bool) -> bool {
        match self {
            KeyBinding::Key(k) => {
                // Parse simple format like "Shift+Tab"
                let parts: Vec<&str> = k.split('+').collect();
                if parts.len() == 1 {
                    k.eq_ignore_ascii_case(key) && !shift && !ctrl && !alt
                } else {
                    let key_part = parts.last().unwrap();
                    let has_shift = parts.iter().any(|p| p.eq_ignore_ascii_case("shift"));
                    let has_ctrl = parts.iter().any(|p| p.eq_ignore_ascii_case("ctrl"));
                    let has_alt = parts.iter().any(|p| p.eq_ignore_ascii_case("alt"));
                    key_part.eq_ignore_ascii_case(key)
                        && shift == has_shift
                        && ctrl == has_ctrl
                        && alt == has_alt
                }
            }
            KeyBinding::Combo { key: k, modifiers } => {
                let has_shift = modifiers.iter().any(|m| m.eq_ignore_ascii_case("shift"));
                let has_ctrl = modifiers.iter().any(|m| m.eq_ignore_ascii_case("ctrl"));
                let has_alt = modifiers.iter().any(|m| m.eq_ignore_ascii_case("alt"));
                k.eq_ignore_ascii_case(key)
                    && shift == has_shift
                    && ctrl == has_ctrl
                    && alt == has_alt
            }
        }
    }
}

/// Entity definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct EntityDef {
    /// Entity name/ID
    pub name: String,

    /// Mesh type or path
    pub mesh: MeshDef,

    /// Transform
    #[serde(default)]
    pub transform: TransformDef,

    /// Material properties
    #[serde(default)]
    pub material: MaterialDef,

    /// Layer name
    #[serde(default = "default_layer")]
    pub layer: String,

    /// Whether entity is visible
    #[serde(default = "default_true")]
    pub visible: bool,

    /// Animation/motion configuration
    #[serde(default)]
    pub animation: Option<AnimationDef>,

    // ========== Picking & Events (Phase 10-11) ==========

    /// Pickable component configuration
    #[serde(default)]
    pub pickable: Option<PickableDef>,

    /// Input event handlers
    #[serde(default)]
    pub input_events: Option<InputEventsDef>,

    // ========== LOD System (Phase 15) ==========

    /// LOD (Level of Detail) configuration
    #[serde(default)]
    pub lod: Option<LodDef>,

    // ========== Render Pass (Phase 12) ==========

    /// Custom render pass flags
    #[serde(default)]
    pub render_pass: Option<RenderPassDef>,

    // ========== Basic Physics (legacy) ==========

    /// Physics body configuration (legacy - use game_physics for full features)
    #[serde(default)]
    pub physics: Option<PhysicsDef>,

    /// Trigger volume configuration (legacy - use game_trigger for full features)
    #[serde(default)]
    pub trigger: Option<TriggerDef>,

    // ========== Game Systems (docs 21-29) ==========

    /// C++ class script component (doc 21)
    #[serde(default)]
    pub cpp_class: Option<CppClassDef>,

    /// Blueprint component (doc 21)
    #[serde(default)]
    pub blueprint: Option<BlueprintDef>,

    /// Event bindings (doc 21)
    #[serde(default)]
    pub events: Vec<EventBindingDef>,

    /// Full trigger definition (doc 22) - replaces basic trigger when present
    #[serde(default)]
    pub game_trigger: Option<TriggerDefinition>,

    /// Full physics definition (doc 23) - replaces basic physics when present
    #[serde(default)]
    pub game_physics: Option<FullPhysicsDef>,

    /// Character controller (doc 23)
    #[serde(default)]
    pub character_controller: Option<CharacterControllerDef>,

    /// Physics joints (doc 23)
    #[serde(default)]
    pub joints: Vec<JointDef>,

    /// Health component (doc 24)
    #[serde(default)]
    pub health: Option<HealthDef>,

    /// Weapon component (doc 24)
    #[serde(default)]
    pub weapon: Option<WeaponDef>,

    /// Inventory component (doc 25)
    #[serde(default)]
    pub inventory: Option<InventoryDef>,

    /// Pickup component - makes this entity a world pickup (doc 25)
    #[serde(default)]
    pub pickup: Option<PickupDef>,

    /// Vendor/shop component (doc 25)
    #[serde(default)]
    pub vendor: Option<VendorDef>,

    /// Entity audio component (doc 26)
    #[serde(default)]
    pub entity_audio: Option<EntityAudioDef>,

    /// AI component (doc 29)
    #[serde(default)]
    pub ai: Option<AiComponentDef>,

    /// Entity tags for filtering and identification
    #[serde(default)]
    pub tags: Vec<String>,

    /// Custom properties for scripting
    #[serde(default)]
    pub properties: HashMap<String, ScriptValue>,
}

// ============================================================================
// Picking & Input Events (Phase 10-11)
// ============================================================================

/// Pickable component for entities that can be selected/clicked
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PickableDef {
    /// Enable picking for this entity
    #[serde(default = "default_true")]
    pub enabled: bool,

    /// Picking priority (higher = picked first when overlapping)
    #[serde(default)]
    pub priority: i32,

    /// Bounds type for picking
    #[serde(default)]
    pub bounds: PickBoundsType,

    /// Highlight on hover
    #[serde(default)]
    pub highlight_on_hover: bool,

    /// Custom pick mask/group
    #[serde(default)]
    pub group: Option<String>,
}

impl Default for PickableDef {
    fn default() -> Self {
        Self {
            enabled: true,
            priority: 0,
            bounds: PickBoundsType::Mesh,
            highlight_on_hover: false,
            group: None,
        }
    }
}

/// Pick bounds type
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum PickBoundsType {
    /// Use mesh geometry (precise)
    #[default]
    Mesh,
    /// Use axis-aligned bounding box (fast)
    Aabb,
    /// Use sphere bounds (fastest)
    Sphere,
    /// Custom bounds
    Custom,
}

/// Input event handlers for an entity
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct InputEventsDef {
    /// Handler for click events
    #[serde(default)]
    pub on_click: Option<String>,

    /// Handler for double-click
    #[serde(default)]
    pub on_double_click: Option<String>,

    /// Handler for pointer enter (hover start)
    #[serde(default)]
    pub on_pointer_enter: Option<String>,

    /// Handler for pointer exit (hover end)
    #[serde(default)]
    pub on_pointer_exit: Option<String>,

    /// Handler for drag start
    #[serde(default)]
    pub on_drag_start: Option<String>,

    /// Handler for drag
    #[serde(default)]
    pub on_drag: Option<String>,

    /// Handler for drag end
    #[serde(default)]
    pub on_drag_end: Option<String>,

    /// Handler for right-click
    #[serde(default)]
    pub on_context_menu: Option<String>,
}

// ============================================================================
// LOD System (Phase 15)
// ============================================================================

/// LOD (Level of Detail) configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct LodDef {
    /// LOD levels (distance thresholds)
    #[serde(default)]
    pub levels: Vec<LodLevelDef>,

    /// LOD bias (positive = use higher detail, negative = use lower)
    #[serde(default)]
    pub bias: f32,

    /// Fade between LOD levels
    #[serde(default)]
    pub fade_transition: bool,

    /// Fade duration in seconds
    #[serde(default = "default_lod_fade_duration")]
    pub fade_duration: f32,
}

fn default_lod_fade_duration() -> f32 { 0.2 }

impl Default for LodDef {
    fn default() -> Self {
        Self {
            levels: vec![],
            bias: 0.0,
            fade_transition: false,
            fade_duration: default_lod_fade_duration(),
        }
    }
}

/// Individual LOD level
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct LodLevelDef {
    /// Distance at which this LOD becomes active
    pub distance: f32,

    /// Mesh to use at this LOD (or "hide" to cull)
    pub mesh: Option<String>,

    /// Screen coverage threshold (alternative to distance)
    #[serde(default)]
    pub screen_coverage: Option<f32>,
}

// ============================================================================
// Render Pass (Phase 12)
// ============================================================================

/// Render pass configuration for an entity
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct RenderPassDef {
    /// Include in shadow pass
    #[serde(default = "default_true")]
    pub cast_shadows: bool,

    /// Receive shadows
    #[serde(default = "default_true")]
    pub receive_shadows: bool,

    /// Include in reflection pass
    #[serde(default = "default_true")]
    pub reflect: bool,

    /// Include in refraction pass
    #[serde(default)]
    pub refract: bool,

    /// Custom pass names to include this entity in
    #[serde(default)]
    pub custom_passes: Vec<String>,

    /// Render order within layer (lower = rendered first)
    #[serde(default)]
    pub order: i32,
}

// ============================================================================
// Physics System (Game Logic)
// ============================================================================

/// Physics body configuration for an entity
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PhysicsDef {
    /// Body type: "static", "dynamic", "kinematic", "player"
    #[serde(default = "default_physics_type")]
    pub body_type: String,

    /// Collider shape
    #[serde(default)]
    pub shape: PhysicsShapeDef,

    /// Mass (for dynamic bodies)
    #[serde(default = "default_mass")]
    pub mass: f32,

    /// Friction coefficient
    #[serde(default = "default_friction")]
    pub friction: f32,

    /// Restitution (bounciness)
    #[serde(default)]
    pub restitution: f32,

    /// Is this a sensor/trigger collider (no physics response)
    #[serde(default)]
    pub is_sensor: bool,
}

fn default_physics_type() -> String { "static".to_string() }
fn default_mass() -> f32 { 1.0 }
fn default_friction() -> f32 { 0.5 }

impl Default for PhysicsDef {
    fn default() -> Self {
        Self {
            body_type: default_physics_type(),
            shape: PhysicsShapeDef::default(),
            mass: default_mass(),
            friction: default_friction(),
            restitution: 0.0,
            is_sensor: false,
        }
    }
}

/// Physics collider shape definition
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "lowercase")]
pub enum PhysicsShapeDef {
    /// Box collider with half extents
    Box {
        #[serde(default = "default_half_extents")]
        half_extents: [f32; 3],
    },
    /// Sphere collider
    Sphere {
        #[serde(default = "default_radius")]
        radius: f32,
    },
    /// Capsule collider (cylinder with hemisphere caps)
    Capsule {
        #[serde(default = "default_half_height")]
        half_height: f32,
        #[serde(default = "default_radius")]
        radius: f32,
    },
    /// Cylinder collider
    Cylinder {
        #[serde(default = "default_half_height")]
        half_height: f32,
        #[serde(default = "default_radius")]
        radius: f32,
    },
    /// Use mesh bounds as box collider
    MeshBounds,
}

fn default_half_extents() -> [f32; 3] { [0.5, 0.5, 0.5] }
fn default_radius() -> f32 { 0.5 }
fn default_half_height() -> f32 { 0.5 }

impl Default for PhysicsShapeDef {
    fn default() -> Self {
        Self::Box { half_extents: default_half_extents() }
    }
}

/// Trigger volume configuration
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct TriggerDef {
    /// Trigger volume shape
    #[serde(default)]
    pub shape: TriggerShapeDef,

    /// Trigger mode: "once", "repeatable"
    #[serde(default = "default_trigger_mode")]
    pub mode: String,

    /// Cooldown between triggers (seconds)
    #[serde(default)]
    pub cooldown: f32,

    /// Event name to fire when triggered
    #[serde(default)]
    pub event: Option<String>,

    /// Layer filter (which layers can trigger this)
    #[serde(default)]
    pub filter_layers: Vec<String>,
}

fn default_trigger_mode() -> String { "repeatable".to_string() }

impl Default for TriggerDef {
    fn default() -> Self {
        Self {
            shape: TriggerShapeDef::default(),
            mode: default_trigger_mode(),
            cooldown: 0.0,
            event: None,
            filter_layers: Vec::new(),
        }
    }
}

/// Trigger volume shape
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "lowercase")]
pub enum TriggerShapeDef {
    /// Box trigger
    Box {
        #[serde(default = "default_half_extents")]
        half_extents: [f32; 3],
    },
    /// Sphere trigger
    Sphere {
        #[serde(default = "default_radius")]
        radius: f32,
    },
}

impl Default for TriggerShapeDef {
    fn default() -> Self {
        Self::Box { half_extents: default_half_extents() }
    }
}

// ============================================================================
// Animation System
// ============================================================================

/// Animation definition - controls how an entity moves/rotates over time
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum AnimationDef {
    /// Continuous rotation around an axis
    Rotate(RotateAnimation),
    /// Oscillate back and forth (like a pendulum or bobbing)
    Oscillate(OscillateAnimation),
    /// Follow a path of waypoints
    Path(PathAnimation),
    /// Orbit around a center point
    Orbit(OrbitAnimation),
    /// Scale pulsing (breathing effect)
    Pulse(PulseAnimation),
}

/// Rotation animation - continuously rotate around an axis
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct RotateAnimation {
    /// Rotation axis [x, y, z] (will be normalized)
    #[serde(default = "default_y_axis")]
    pub axis: [f32; 3],

    /// Rotation speed in radians per second
    #[serde(default = "default_rotation_speed")]
    pub speed: f32,
}

fn default_y_axis() -> [f32; 3] { [0.0, 1.0, 0.0] }
fn default_rotation_speed() -> f32 { 1.0 }

/// Oscillate animation - move back and forth
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct OscillateAnimation {
    /// Oscillation axis/direction [x, y, z]
    #[serde(default = "default_y_axis")]
    pub axis: [f32; 3],

    /// Amplitude (max displacement from origin)
    #[serde(default = "default_amplitude")]
    pub amplitude: f32,

    /// Frequency (oscillations per second)
    #[serde(default = "default_frequency")]
    pub frequency: f32,

    /// Phase offset (0-1, fraction of cycle)
    #[serde(default)]
    pub phase: f32,

    /// Oscillate rotation instead of position
    #[serde(default)]
    pub rotate: bool,
}

fn default_amplitude() -> f32 { 0.5 }
fn default_frequency() -> f32 { 1.0 }

/// Path animation - follow waypoints
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PathAnimation {
    /// List of waypoint positions [[x,y,z], [x,y,z], ...]
    pub points: Vec<[f32; 3]>,

    /// Total time to complete the path (seconds)
    #[serde(default = "default_duration")]
    pub duration: f32,

    /// Loop the animation
    #[serde(default = "default_true")]
    pub loop_animation: bool,

    /// Ping-pong (reverse at end instead of jumping back)
    #[serde(default)]
    pub ping_pong: bool,

    /// Interpolation mode
    #[serde(default)]
    pub interpolation: InterpolationMode,

    /// Orient entity to face movement direction
    #[serde(default)]
    pub orient_to_path: bool,

    /// Easing function
    #[serde(default)]
    pub easing: EasingFunction,
}

fn default_duration() -> f32 { 5.0 }

/// Orbit animation - orbit around a point
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct OrbitAnimation {
    /// Center point to orbit around [x, y, z]
    #[serde(default)]
    pub center: [f32; 3],

    /// Orbit radius
    #[serde(default = "default_orbit_radius")]
    pub radius: f32,

    /// Orbit speed (radians per second)
    #[serde(default = "default_rotation_speed")]
    pub speed: f32,

    /// Orbit axis (normal to orbit plane) [x, y, z]
    #[serde(default = "default_y_axis")]
    pub axis: [f32; 3],

    /// Starting angle (radians)
    #[serde(default)]
    pub start_angle: f32,

    /// Keep entity facing the center
    #[serde(default)]
    pub face_center: bool,
}

fn default_orbit_radius() -> f32 { 2.0 }

/// Pulse animation - scale oscillation
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PulseAnimation {
    /// Minimum scale multiplier
    #[serde(default = "default_pulse_min")]
    pub min_scale: f32,

    /// Maximum scale multiplier
    #[serde(default = "default_pulse_max")]
    pub max_scale: f32,

    /// Pulse frequency (cycles per second)
    #[serde(default = "default_frequency")]
    pub frequency: f32,

    /// Phase offset (0-1)
    #[serde(default)]
    pub phase: f32,
}

fn default_pulse_min() -> f32 { 0.9 }
fn default_pulse_max() -> f32 { 1.1 }

/// Interpolation mode for path animation
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum InterpolationMode {
    /// Linear interpolation between points
    #[default]
    Linear,
    /// Smooth curve through points (Catmull-Rom)
    CatmullRom,
    /// Cubic bezier (requires control points)
    Bezier,
    /// Step to next point (no interpolation)
    Step,
}

/// Easing functions for animations
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum EasingFunction {
    /// Linear (no easing)
    #[default]
    Linear,
    /// Smooth start and end
    SmoothStep,
    /// Slow start
    EaseIn,
    /// Slow end
    EaseOut,
    /// Slow start and end
    EaseInOut,
    /// Bouncy
    Bounce,
    /// Overshoot and settle
    Elastic,
}

impl EasingFunction {
    /// Apply easing to a 0-1 parameter
    pub fn apply(&self, t: f32) -> f32 {
        match self {
            EasingFunction::Linear => t,
            EasingFunction::SmoothStep => t * t * (3.0 - 2.0 * t),
            EasingFunction::EaseIn => t * t,
            EasingFunction::EaseOut => 1.0 - (1.0 - t) * (1.0 - t),
            EasingFunction::EaseInOut => {
                if t < 0.5 {
                    2.0 * t * t
                } else {
                    1.0 - (-2.0 * t + 2.0).powi(2) / 2.0
                }
            }
            EasingFunction::Bounce => {
                let n1 = 7.5625;
                let d1 = 2.75;
                let mut t = t;
                if t < 1.0 / d1 {
                    n1 * t * t
                } else if t < 2.0 / d1 {
                    t -= 1.5 / d1;
                    n1 * t * t + 0.75
                } else if t < 2.5 / d1 {
                    t -= 2.25 / d1;
                    n1 * t * t + 0.9375
                } else {
                    t -= 2.625 / d1;
                    n1 * t * t + 0.984375
                }
            }
            EasingFunction::Elastic => {
                if t == 0.0 || t == 1.0 {
                    t
                } else {
                    let p = 0.3;
                    let s = p / 4.0;
                    2.0_f32.powf(-10.0 * t) * ((t - s) * std::f32::consts::TAU / p).sin() + 1.0
                }
            }
        }
    }
}

fn default_layer() -> String { "world".to_string() }
fn default_true() -> bool { true }

/// Mesh definition - either a primitive or a file path
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(untagged)]
pub enum MeshDef {
    /// Primitive mesh type
    Primitive(PrimitiveMesh),
    /// Path to mesh file (glTF, OBJ, etc.)
    File { path: String },
}

/// Built-in primitive mesh types
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum PrimitiveMesh {
    Sphere,
    Cube,
    Plane,
    Torus,
    Diamond,
    Cylinder,
    Cone,
}

/// Transform definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct TransformDef {
    /// Position [x, y, z]
    #[serde(default)]
    pub position: [f32; 3],

    /// Rotation in euler angles (degrees) [x, y, z]
    #[serde(default)]
    pub rotation: [f32; 3],

    /// Scale [x, y, z] or uniform scale
    #[serde(default = "default_scale")]
    pub scale: ScaleDef,
}

fn default_scale() -> ScaleDef { ScaleDef::Uniform(1.0) }

impl Default for TransformDef {
    fn default() -> Self {
        Self {
            position: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0],
            scale: ScaleDef::Uniform(1.0),
        }
    }
}

/// Scale can be uniform or per-axis
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(untagged)]
pub enum ScaleDef {
    Uniform(f32),
    NonUniform([f32; 3]),
}

impl ScaleDef {
    pub fn to_array(&self) -> [f32; 3] {
        match self {
            ScaleDef::Uniform(s) => [*s, *s, *s],
            ScaleDef::NonUniform(arr) => *arr,
        }
    }
}

/// Material definition
#[derive(Debug, Clone, Deserialize, Serialize, Default)]
pub struct MaterialDef {
    /// Base color [r, g, b, a] or texture path
    #[serde(default)]
    pub albedo: ColorOrTexture,

    /// Normal map texture path
    #[serde(default)]
    pub normal_map: Option<String>,

    /// Metallic value (0-1) or texture path
    #[serde(default)]
    pub metallic: ValueOrTexture,

    /// Roughness value (0-1) or texture path
    #[serde(default = "default_roughness")]
    pub roughness: ValueOrTexture,

    /// Ambient occlusion texture path
    #[serde(default)]
    pub ao_map: Option<String>,

    /// Emissive color [r, g, b]
    #[serde(default)]
    pub emissive: [f32; 3],

    // ========== Advanced Materials (Phase 7) ==========

    /// Transmission properties (glass, water, etc.)
    #[serde(default)]
    pub transmission: Option<TransmissionDef>,

    /// Sheen properties (velvet, fabric)
    #[serde(default)]
    pub sheen: Option<SheenDef>,

    /// Clearcoat properties (car paint, lacquered surfaces)
    #[serde(default)]
    pub clearcoat: Option<ClearcoatDef>,

    /// Anisotropy properties (brushed metal, hair)
    #[serde(default)]
    pub anisotropy: Option<AnisotropyDef>,

    /// Subsurface scattering (skin, wax, marble)
    #[serde(default)]
    pub subsurface: Option<SubsurfaceDef>,

    /// Iridescence (soap bubbles, oil slicks)
    #[serde(default)]
    pub iridescence: Option<IridescenceDef>,
}

// ============================================================================
// Advanced Material Properties (Phase 7)
// ============================================================================

/// Transmission properties for transparent/translucent materials
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct TransmissionDef {
    /// Transmission factor (0 = opaque, 1 = fully transmissive)
    #[serde(default = "default_transmission_factor")]
    pub factor: f32,

    /// Index of refraction
    #[serde(default = "default_ior")]
    pub ior: f32,

    /// Thickness for volume absorption
    #[serde(default = "default_thickness")]
    pub thickness: f32,

    /// Attenuation color for volume absorption
    #[serde(default = "default_white")]
    pub attenuation_color: [f32; 3],

    /// Attenuation distance
    #[serde(default = "default_attenuation_distance")]
    pub attenuation_distance: f32,
}

fn default_transmission_factor() -> f32 { 1.0 }
fn default_ior() -> f32 { 1.5 }
fn default_thickness() -> f32 { 0.0 }
fn default_attenuation_distance() -> f32 { f32::INFINITY }

impl Default for TransmissionDef {
    fn default() -> Self {
        Self {
            factor: default_transmission_factor(),
            ior: default_ior(),
            thickness: default_thickness(),
            attenuation_color: default_white(),
            attenuation_distance: default_attenuation_distance(),
        }
    }
}

/// Sheen properties for fabric-like materials
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct SheenDef {
    /// Sheen color
    #[serde(default = "default_white")]
    pub color: [f32; 3],

    /// Sheen roughness
    #[serde(default = "default_sheen_roughness")]
    pub roughness: f32,
}

fn default_sheen_roughness() -> f32 { 0.5 }

impl Default for SheenDef {
    fn default() -> Self {
        Self {
            color: default_white(),
            roughness: default_sheen_roughness(),
        }
    }
}

/// Clearcoat properties for layered surfaces
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ClearcoatDef {
    /// Clearcoat intensity (0-1)
    #[serde(default = "default_clearcoat_intensity")]
    pub intensity: f32,

    /// Clearcoat roughness
    #[serde(default = "default_clearcoat_roughness")]
    pub roughness: f32,

    /// Clearcoat normal map
    #[serde(default)]
    pub normal_map: Option<String>,
}

fn default_clearcoat_intensity() -> f32 { 1.0 }
fn default_clearcoat_roughness() -> f32 { 0.1 }

impl Default for ClearcoatDef {
    fn default() -> Self {
        Self {
            intensity: default_clearcoat_intensity(),
            roughness: default_clearcoat_roughness(),
            normal_map: None,
        }
    }
}

/// Anisotropy properties for directional reflections
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct AnisotropyDef {
    /// Anisotropy strength (-1 to 1)
    #[serde(default = "default_anisotropy_strength")]
    pub strength: f32,

    /// Anisotropy rotation in radians
    #[serde(default)]
    pub rotation: f32,

    /// Anisotropy direction texture
    #[serde(default)]
    pub direction_map: Option<String>,
}

fn default_anisotropy_strength() -> f32 { 0.5 }

impl Default for AnisotropyDef {
    fn default() -> Self {
        Self {
            strength: default_anisotropy_strength(),
            rotation: 0.0,
            direction_map: None,
        }
    }
}

/// Subsurface scattering properties
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct SubsurfaceDef {
    /// Subsurface color
    #[serde(default = "default_subsurface_color")]
    pub color: [f32; 3],

    /// Subsurface radius (scattering distance)
    #[serde(default = "default_subsurface_radius")]
    pub radius: [f32; 3],

    /// Subsurface factor
    #[serde(default = "default_subsurface_factor")]
    pub factor: f32,
}

fn default_subsurface_color() -> [f32; 3] { [1.0, 0.8, 0.6] }
fn default_subsurface_radius() -> [f32; 3] { [1.0, 0.2, 0.1] }
fn default_subsurface_factor() -> f32 { 0.5 }

impl Default for SubsurfaceDef {
    fn default() -> Self {
        Self {
            color: default_subsurface_color(),
            radius: default_subsurface_radius(),
            factor: default_subsurface_factor(),
        }
    }
}

/// Iridescence properties (thin-film interference)
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct IridescenceDef {
    /// Iridescence factor (0-1)
    #[serde(default = "default_iridescence_factor")]
    pub factor: f32,

    /// Index of refraction for the thin film
    #[serde(default = "default_iridescence_ior")]
    pub ior: f32,

    /// Thickness range [min, max] in nanometers
    #[serde(default = "default_iridescence_thickness")]
    pub thickness_range: [f32; 2],
}

fn default_iridescence_factor() -> f32 { 1.0 }
fn default_iridescence_ior() -> f32 { 1.3 }
fn default_iridescence_thickness() -> [f32; 2] { [100.0, 400.0] }

impl Default for IridescenceDef {
    fn default() -> Self {
        Self {
            factor: default_iridescence_factor(),
            ior: default_iridescence_ior(),
            thickness_range: default_iridescence_thickness(),
        }
    }
}

fn default_roughness() -> ValueOrTexture { ValueOrTexture::Value(0.5) }

/// Color can be a solid color or a texture
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(untagged)]
pub enum ColorOrTexture {
    Color([f32; 4]),
    Color3([f32; 3]),  // Will be converted to [r, g, b, 1.0]
    Texture { texture: String },
}

impl Default for ColorOrTexture {
    fn default() -> Self {
        ColorOrTexture::Color([0.8, 0.8, 0.8, 1.0])
    }
}

impl ColorOrTexture {
    pub fn to_color(&self) -> [f32; 4] {
        match self {
            ColorOrTexture::Color(c) => *c,
            ColorOrTexture::Color3(c) => [c[0], c[1], c[2], 1.0],
            ColorOrTexture::Texture { .. } => [1.0, 1.0, 1.0, 1.0], // White if textured
        }
    }

    pub fn texture_path(&self) -> Option<&str> {
        match self {
            ColorOrTexture::Texture { texture } => Some(texture),
            _ => None,
        }
    }
}

/// Value can be a float or a texture
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(untagged)]
pub enum ValueOrTexture {
    Value(f32),
    Texture { texture: String },
}

impl Default for ValueOrTexture {
    fn default() -> Self {
        ValueOrTexture::Value(0.0)
    }
}

impl ValueOrTexture {
    pub fn to_value(&self) -> f32 {
        match self {
            ValueOrTexture::Value(v) => *v,
            ValueOrTexture::Texture { .. } => 1.0, // Full value if textured
        }
    }

    pub fn texture_path(&self) -> Option<&str> {
        match self {
            ValueOrTexture::Texture { texture } => Some(texture),
            _ => None,
        }
    }
}

/// Particle emitter definition
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ParticleEmitterDef {
    /// Emitter name
    pub name: String,

    /// Position [x, y, z]
    #[serde(default)]
    pub position: [f32; 3],

    /// Particles per second
    #[serde(default = "default_emit_rate")]
    pub emit_rate: f32,

    /// Maximum particles
    #[serde(default = "default_max_particles")]
    pub max_particles: usize,

    /// Particle lifetime range [min, max] seconds
    #[serde(default = "default_lifetime")]
    pub lifetime: [f32; 2],

    /// Particle speed range [min, max]
    #[serde(default = "default_speed")]
    pub speed: [f32; 2],

    /// Particle size range [min, max]
    #[serde(default = "default_size")]
    pub size: [f32; 2],

    /// Start color [r, g, b, a]
    #[serde(default = "default_color_start")]
    pub color_start: [f32; 4],

    /// End color [r, g, b, a]
    #[serde(default = "default_color_end")]
    pub color_end: [f32; 4],

    /// Gravity/acceleration [x, y, z]
    #[serde(default)]
    pub gravity: [f32; 3],

    /// Emission cone spread (radians)
    #[serde(default = "default_spread")]
    pub spread: f32,

    /// Emission direction [x, y, z]
    #[serde(default = "default_direction")]
    pub direction: [f32; 3],

    /// Whether emitter is enabled
    #[serde(default = "default_true")]
    pub enabled: bool,
}

fn default_emit_rate() -> f32 { 50.0 }
fn default_max_particles() -> usize { 1000 }
fn default_lifetime() -> [f32; 2] { [1.0, 3.0] }
fn default_speed() -> [f32; 2] { [1.0, 3.0] }
fn default_size() -> [f32; 2] { [0.05, 0.15] }
fn default_color_start() -> [f32; 4] { [1.0, 0.8, 0.2, 1.0] }
fn default_color_end() -> [f32; 4] { [1.0, 0.2, 0.0, 0.0] }
fn default_spread() -> f32 { 0.5 }
fn default_direction() -> [f32; 3] { [0.0, 1.0, 0.0] }

/// Texture definition for preloading
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct TextureDef {
    /// Texture name/ID
    pub name: String,
    /// Path relative to app assets
    pub path: String,
    /// Whether this is an HDR texture
    #[serde(default)]
    pub hdr: bool,
    /// sRGB (color) or linear (data like normal maps)
    #[serde(default = "default_true")]
    pub srgb: bool,
}

// ============================================================================
// Scene Loader
// ============================================================================

/// Error type for scene loading
#[derive(Debug)]
pub enum SceneLoadError {
    IoError(std::io::Error),
    ParseError(toml::de::Error),
    InvalidPath(String),
}

impl std::fmt::Display for SceneLoadError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SceneLoadError::IoError(e) => write!(f, "IO error: {}", e),
            SceneLoadError::ParseError(e) => write!(f, "Parse error: {}", e),
            SceneLoadError::InvalidPath(p) => write!(f, "Invalid path: {}", p),
        }
    }
}

impl std::error::Error for SceneLoadError {}

impl From<std::io::Error> for SceneLoadError {
    fn from(e: std::io::Error) -> Self {
        SceneLoadError::IoError(e)
    }
}

impl From<toml::de::Error> for SceneLoadError {
    fn from(e: toml::de::Error) -> Self {
        SceneLoadError::ParseError(e)
    }
}

/// Load a scene definition from a TOML file
pub fn load_scene<P: AsRef<Path>>(path: P) -> Result<SceneDefinition, SceneLoadError> {
    let content = std::fs::read_to_string(path)?;
    let scene: SceneDefinition = toml::from_str(&content)?;
    Ok(scene)
}

/// Load a scene definition from a TOML string
pub fn load_scene_from_str(content: &str) -> Result<SceneDefinition, SceneLoadError> {
    let scene: SceneDefinition = toml::from_str(content)?;
    Ok(scene)
}

// ============================================================================
// Conversion helpers for SceneRenderer
// ============================================================================

impl EntityDef {
    /// Convert to the mesh type enum used by SceneRenderer
    pub fn mesh_type(&self) -> &str {
        match &self.mesh {
            MeshDef::Primitive(p) => match p {
                PrimitiveMesh::Sphere => "sphere",
                PrimitiveMesh::Cube => "cube",
                PrimitiveMesh::Plane => "plane",
                PrimitiveMesh::Torus => "torus",
                PrimitiveMesh::Diamond => "diamond",
                PrimitiveMesh::Cylinder => "cylinder",
                PrimitiveMesh::Cone => "cone",
            },
            MeshDef::File { path } => path,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_simple_scene() {
        let toml = r#"
[scene]
name = "Test Scene"

[[entities]]
name = "sphere"
mesh = "sphere"

[entities.transform]
position = [0, 1, 0]

[entities.material]
metallic = 1.0
roughness = 0.2
"#;
        let scene = load_scene_from_str(toml).unwrap();
        assert_eq!(scene.scene.name, "Test Scene");
        assert_eq!(scene.entities.len(), 1);
    }

    #[test]
    fn test_parse_particle_emitter() {
        let toml = r#"
[[particle_emitters]]
name = "fire"
position = [0, 0, 0]
emit_rate = 100
color_start = [1.0, 0.7, 0.2, 1.0]
color_end = [1.0, 0.1, 0.0, 0.0]
"#;
        let scene = load_scene_from_str(toml).unwrap();
        assert_eq!(scene.particle_emitters.len(), 1);
        assert_eq!(scene.particle_emitters[0].emit_rate, 100.0);
    }
}
