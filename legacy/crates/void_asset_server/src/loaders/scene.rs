//! Scene loader for JSON scene definitions

use serde::{Deserialize, Serialize};

/// Scene definition with entities and their components
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SceneAsset {
    /// Scene name
    pub name: String,
    /// Entities in the scene
    #[serde(default)]
    pub entities: Vec<EntityDef>,
    /// Referenced assets (for preloading)
    #[serde(default)]
    pub assets: SceneAssets,
}

/// Entity definition
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EntityDef {
    /// Entity name
    pub name: String,
    /// Transform
    #[serde(default)]
    pub transform: TransformDef,
    /// Components
    #[serde(default)]
    pub components: Vec<ComponentDef>,
    /// Behavior plugin path (e.g., "plugins/gun.wasm")
    #[serde(default)]
    pub plugin: Option<String>,
    /// Plugin configuration (passed to plugin on spawn)
    #[serde(default)]
    pub plugin_config: Option<serde_json::Value>,
    /// Child entities
    #[serde(default)]
    pub children: Vec<EntityDef>,
}

/// Transform definition
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct TransformDef {
    /// Position
    #[serde(default)]
    pub position: [f32; 3],
    /// Rotation (euler angles in radians, or degrees if < 2*PI)
    #[serde(default)]
    pub rotation: [f32; 3],
    /// Scale
    #[serde(default = "default_scale")]
    pub scale: [f32; 3],
}

fn default_scale() -> [f32; 3] {
    [1.0, 1.0, 1.0]
}

impl Default for TransformDef {
    fn default() -> Self {
        Self {
            position: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0],
            scale: default_scale(),
        }
    }
}

/// Component definitions (tagged enum for polymorphism)
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum ComponentDef {
    /// Mesh renderer component
    MeshRenderer {
        /// Mesh asset path (e.g., "meshes/cube.obj") or primitive name ("cube", "sphere")
        mesh: String,
        /// Optional material path
        #[serde(default)]
        material: Option<String>,
        /// Optional color override
        #[serde(default)]
        color: Option<[f32; 4]>,
    },
    /// Light component
    Light {
        /// Light type: "directional", "point", "spot"
        kind: String,
        /// Light color RGB
        color: [f32; 3],
        /// Light intensity
        intensity: f32,
        /// Range (for point/spot lights)
        #[serde(default)]
        range: Option<f32>,
    },
    /// Camera component
    Camera {
        /// Field of view in degrees
        fov: f32,
        /// Near clip plane
        near: f32,
        /// Far clip plane
        far: f32,
    },
    /// Spinning behavior
    Spinning {
        /// Rotation speed per axis (radians/sec)
        speed: [f32; 3],
    },
    /// Behavior plugin component
    BehaviorPlugin {
        /// Path to WASM plugin file
        path: String,
        /// Configuration passed to plugin
        #[serde(default)]
        config: Option<serde_json::Value>,
    },
    /// Physics rigidbody component
    RigidBody {
        /// Body type: "static", "dynamic", "kinematic"
        #[serde(default = "default_rigid_body_type")]
        body_type: String,
        /// Mass in kg
        #[serde(default = "default_mass")]
        mass: f32,
        /// Enable gravity
        #[serde(default = "default_true")]
        gravity: bool,
    },
    /// Collider component
    Collider {
        /// Shape type: "box", "sphere", "capsule", "mesh"
        shape: String,
        /// Size/dimensions depending on shape
        #[serde(default)]
        size: Option<[f32; 3]>,
        /// Radius for sphere/capsule
        #[serde(default)]
        radius: Option<f32>,
        /// Height for capsule
        #[serde(default)]
        height: Option<f32>,
        /// Is trigger (no physics response, just events)
        #[serde(default)]
        trigger: bool,
    },
    /// Custom component (catch-all)
    Custom {
        /// Component type name
        name: String,
        /// Arbitrary JSON data
        data: serde_json::Value,
    },
}

fn default_rigid_body_type() -> String {
    "dynamic".to_string()
}

fn default_mass() -> f32 {
    1.0
}

fn default_true() -> bool {
    true
}

/// Assets referenced by the scene
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct SceneAssets {
    /// Mesh asset paths
    #[serde(default)]
    pub meshes: Vec<String>,
    /// Texture asset paths
    #[serde(default)]
    pub textures: Vec<String>,
    /// Shader asset paths
    #[serde(default)]
    pub shaders: Vec<String>,
    /// Material asset paths
    #[serde(default)]
    pub materials: Vec<String>,
    /// Behavior plugin paths
    #[serde(default)]
    pub plugins: Vec<String>,
}

/// Loader for scene files
pub struct SceneLoader;

impl SceneLoader {
    /// Load a scene from JSON bytes
    pub fn load(data: &[u8], path: &str) -> Result<SceneAsset, String> {
        let text = std::str::from_utf8(data)
            .map_err(|e| format!("Invalid UTF-8 in scene {}: {}", path, e))?;

        serde_json::from_str(text)
            .map_err(|e| format!("Failed to parse scene JSON {}: {}", path, e))
    }

    /// Create an empty scene
    pub fn empty(name: &str) -> SceneAsset {
        SceneAsset {
            name: name.to_string(),
            entities: Vec::new(),
            assets: SceneAssets::default(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_scene() {
        let json = r#"{
            "name": "Test Scene",
            "entities": [
                {
                    "name": "Cube",
                    "transform": {
                        "position": [0, 1, 0],
                        "scale": [2, 2, 2]
                    },
                    "components": [
                        { "type": "MeshRenderer", "mesh": "cube", "color": [1, 0, 0, 1] },
                        { "type": "Spinning", "speed": [0, 1, 0] }
                    ]
                }
            ]
        }"#;

        let scene = SceneLoader::load(json.as_bytes(), "test.scene.json").unwrap();
        assert_eq!(scene.name, "Test Scene");
        assert_eq!(scene.entities.len(), 1);
        assert_eq!(scene.entities[0].name, "Cube");
        assert_eq!(scene.entities[0].transform.position, [0.0, 1.0, 0.0]);
        assert_eq!(scene.entities[0].components.len(), 2);
    }
}
