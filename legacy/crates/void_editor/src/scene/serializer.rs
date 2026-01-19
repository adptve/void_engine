//! Scene serialization to/from TOML format.

use std::path::PathBuf;
use serde::{Deserialize, Serialize};

use crate::core::{EditorState, EntityId, MeshType, SceneEntity, Transform};

/// Scene file data structure.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SceneData {
    /// Scene metadata
    pub scene: SceneMetadata,
    /// Environment settings (optional)
    #[serde(default)]
    pub environment: Option<EnvironmentData>,
    /// All entities in the scene
    #[serde(default)]
    pub entities: Vec<EntityData>,
}

/// Scene metadata.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SceneMetadata {
    pub name: String,
    #[serde(default)]
    pub description: String,
    #[serde(default = "default_version")]
    pub version: String,
}

fn default_version() -> String {
    "1.0.0".to_string()
}

/// Environment settings.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EnvironmentData {
    #[serde(default)]
    pub light_direction: Option<[f32; 3]>,
    #[serde(default)]
    pub light_color: Option<[f32; 3]>,
    #[serde(default)]
    pub light_intensity: Option<f32>,
    #[serde(default)]
    pub ambient_intensity: Option<f32>,
}

/// Entity data for serialization.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EntityData {
    pub name: String,
    pub mesh: String,
    #[serde(default = "default_layer")]
    pub layer: String,
    #[serde(default = "default_visible")]
    pub visible: bool,
    pub transform: TransformData,
    #[serde(default)]
    pub material: Option<MaterialData>,
}

fn default_layer() -> String {
    "world".to_string()
}

fn default_visible() -> bool {
    true
}

/// Transform data for serialization.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct TransformData {
    #[serde(default)]
    pub position: [f32; 3],
    #[serde(default)]
    pub rotation: Option<[f32; 3]>,
    #[serde(default = "default_scale")]
    pub scale: ScaleData,
}

fn default_scale() -> ScaleData {
    ScaleData::Uniform(1.0)
}

/// Scale can be uniform or per-axis.
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(untagged)]
pub enum ScaleData {
    Uniform(f32),
    PerAxis([f32; 3]),
}

impl ScaleData {
    pub fn to_array(&self) -> [f32; 3] {
        match self {
            ScaleData::Uniform(s) => [*s, *s, *s],
            ScaleData::PerAxis(arr) => *arr,
        }
    }
}

/// Material data for serialization.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MaterialData {
    #[serde(default)]
    pub albedo: Option<ColorOrTexture>,
    #[serde(default)]
    pub metallic: Option<f32>,
    #[serde(default)]
    pub roughness: Option<f32>,
    #[serde(default)]
    pub emissive: Option<[f32; 3]>,
}

/// Color value or texture reference.
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(untagged)]
pub enum ColorOrTexture {
    Color([f32; 3]),
    Texture { texture: String },
}

/// Scene serialization errors.
#[derive(Clone, Debug)]
pub enum SceneError {
    IoError(String),
    ParseError(String),
    SerializeError(String),
}

impl std::fmt::Display for SceneError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SceneError::IoError(e) => write!(f, "IO error: {}", e),
            SceneError::ParseError(e) => write!(f, "Parse error: {}", e),
            SceneError::SerializeError(e) => write!(f, "Serialize error: {}", e),
        }
    }
}

impl std::error::Error for SceneError {}

/// Scene serializer for save/load operations.
pub struct SceneSerializer;

impl SceneSerializer {
    /// Save editor state to a TOML scene file.
    pub fn save(state: &EditorState, path: &PathBuf) -> Result<(), SceneError> {
        let scene_data = Self::state_to_scene_data(state);

        let toml_string = toml::to_string_pretty(&scene_data)
            .map_err(|e| SceneError::SerializeError(e.to_string()))?;

        std::fs::write(path, toml_string)
            .map_err(|e| SceneError::IoError(e.to_string()))?;

        Ok(())
    }

    /// Load a TOML scene file into editor state.
    pub fn load(state: &mut EditorState, path: &PathBuf) -> Result<(), SceneError> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| SceneError::IoError(e.to_string()))?;

        let scene_data: SceneData = toml::from_str(&content)
            .map_err(|e| SceneError::ParseError(e.to_string()))?;

        Self::apply_scene_data(state, &scene_data);

        Ok(())
    }

    /// Convert editor state to scene data structure.
    fn state_to_scene_data(state: &EditorState) -> SceneData {
        let entities: Vec<EntityData> = state.entities.iter().map(|entity| {
            EntityData {
                name: entity.name.clone(),
                mesh: entity.mesh_type.name().to_lowercase(),
                layer: "world".to_string(),
                visible: true,
                transform: TransformData {
                    position: entity.transform.position,
                    rotation: Some(entity.transform.rotation),
                    scale: if entity.transform.scale[0] == entity.transform.scale[1]
                           && entity.transform.scale[1] == entity.transform.scale[2] {
                        ScaleData::Uniform(entity.transform.scale[0])
                    } else {
                        ScaleData::PerAxis(entity.transform.scale)
                    },
                },
                material: Some(MaterialData {
                    albedo: Some(ColorOrTexture::Color(entity.color)),
                    metallic: Some(0.0),
                    roughness: Some(0.5),
                    emissive: None,
                }),
            }
        }).collect();

        SceneData {
            scene: SceneMetadata {
                name: state.scene_path.as_ref()
                    .and_then(|p| p.file_stem())
                    .map(|s| s.to_string_lossy().to_string())
                    .unwrap_or_else(|| "Untitled Scene".to_string()),
                description: String::new(),
                version: "1.0.0".to_string(),
            },
            environment: None,
            entities,
        }
    }

    /// Apply scene data to editor state.
    fn apply_scene_data(state: &mut EditorState, scene_data: &SceneData) {
        // Clear existing entities
        state.entities.clear();
        state.selection.clear();

        // Create entities from scene data
        for entity_data in &scene_data.entities {
            let mesh_type = Self::parse_mesh_type(&entity_data.mesh);

            let id = state.create_entity(entity_data.name.clone(), mesh_type);

            // Apply transform
            if let Some(entity) = state.get_entity_mut(id) {
                entity.transform.position = entity_data.transform.position;
                if let Some(rotation) = entity_data.transform.rotation {
                    entity.transform.rotation = rotation;
                }
                entity.transform.scale = entity_data.transform.scale.to_array();

                // Apply material color if specified
                if let Some(material) = &entity_data.material {
                    if let Some(ColorOrTexture::Color(color)) = &material.albedo {
                        entity.color = *color;
                    }
                }
            }
        }

        state.scene_modified = false;
    }

    /// Parse mesh type from string.
    fn parse_mesh_type(s: &str) -> MeshType {
        match s.to_lowercase().as_str() {
            "cube" => MeshType::Cube,
            "sphere" => MeshType::Sphere,
            "cylinder" => MeshType::Cylinder,
            "plane" => MeshType::Plane,
            "torus" => MeshType::Torus,
            "diamond" => MeshType::Diamond,
            _ => MeshType::Cube, // Default
        }
    }
}
