//! Prefab system for saving and instantiating entity templates.

use std::path::PathBuf;
use serde::{Deserialize, Serialize};

use crate::core::{EntityId, MeshType, Transform};

/// A prefab is a saved entity template that can be instantiated.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Prefab {
    /// Name of the prefab
    pub name: String,
    /// Root entity data
    pub root: PrefabEntity,
    /// Child entities (for hierarchical prefabs)
    pub children: Vec<PrefabEntity>,
    /// Prefab metadata
    pub metadata: PrefabMetadata,
}

/// Entity data stored in a prefab.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PrefabEntity {
    /// Entity name
    pub name: String,
    /// Transform data
    pub transform: PrefabTransform,
    /// Mesh component (if any)
    pub mesh: Option<PrefabMesh>,
    /// Custom properties
    pub properties: std::collections::HashMap<String, serde_json::Value>,
}

/// Transform data for prefab entities.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PrefabTransform {
    pub position: [f32; 3],
    pub rotation: [f32; 3],
    pub scale: [f32; 3],
}

impl Default for PrefabTransform {
    fn default() -> Self {
        Self {
            position: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0],
            scale: [1.0, 1.0, 1.0],
        }
    }
}

impl From<Transform> for PrefabTransform {
    fn from(t: Transform) -> Self {
        Self {
            position: t.position,
            rotation: t.rotation,
            scale: t.scale,
        }
    }
}

impl From<PrefabTransform> for Transform {
    fn from(t: PrefabTransform) -> Self {
        Self {
            position: t.position,
            rotation: t.rotation,
            scale: t.scale,
        }
    }
}

/// Mesh component data for prefabs.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PrefabMesh {
    /// Mesh type (primitive)
    pub mesh_type: String,
    /// Color
    pub color: [f32; 3],
    /// External mesh asset path (if any)
    pub asset_path: Option<PathBuf>,
}

impl PrefabMesh {
    pub fn from_primitive(mesh_type: MeshType, color: [f32; 3]) -> Self {
        Self {
            mesh_type: mesh_type.name().to_string(),
            color,
            asset_path: None,
        }
    }

    pub fn to_mesh_type(&self) -> MeshType {
        match self.mesh_type.to_lowercase().as_str() {
            "cube" => MeshType::Cube,
            "sphere" => MeshType::Sphere,
            "cylinder" => MeshType::Cylinder,
            "plane" => MeshType::Plane,
            "torus" => MeshType::Torus,
            "diamond" => MeshType::Diamond,
            _ => MeshType::Cube,
        }
    }
}

/// Prefab metadata.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PrefabMetadata {
    /// Prefab version
    pub version: u32,
    /// Creation timestamp
    pub created: Option<String>,
    /// Author/source
    pub author: Option<String>,
    /// Description
    pub description: Option<String>,
    /// Tags for categorization
    pub tags: Vec<String>,
}

impl Default for PrefabMetadata {
    fn default() -> Self {
        Self {
            version: 1,
            created: None,
            author: None,
            description: None,
            tags: Vec::new(),
        }
    }
}

impl Prefab {
    /// Create a new prefab from an entity name and transform.
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            root: PrefabEntity {
                name: String::new(),
                transform: PrefabTransform::default(),
                mesh: None,
                properties: std::collections::HashMap::new(),
            },
            children: Vec::new(),
            metadata: PrefabMetadata::default(),
        }
    }

    /// Save prefab to a JSON file.
    pub fn save(&self, path: &PathBuf) -> Result<(), PrefabError> {
        let json = serde_json::to_string_pretty(self)
            .map_err(|e| PrefabError::SerializationError(e.to_string()))?;

        std::fs::write(path, json)
            .map_err(|e| PrefabError::IoError(e.to_string()))?;

        Ok(())
    }

    /// Load prefab from a JSON file.
    pub fn load(path: &PathBuf) -> Result<Self, PrefabError> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| PrefabError::IoError(e.to_string()))?;

        let prefab: Self = serde_json::from_str(&content)
            .map_err(|e| PrefabError::DeserializationError(e.to_string()))?;

        Ok(prefab)
    }
}

/// Errors that can occur with prefabs.
#[derive(Clone, Debug)]
pub enum PrefabError {
    IoError(String),
    SerializationError(String),
    DeserializationError(String),
    InvalidPrefab(String),
}

impl std::fmt::Display for PrefabError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PrefabError::IoError(e) => write!(f, "IO error: {}", e),
            PrefabError::SerializationError(e) => write!(f, "Serialization error: {}", e),
            PrefabError::DeserializationError(e) => write!(f, "Deserialization error: {}", e),
            PrefabError::InvalidPrefab(e) => write!(f, "Invalid prefab: {}", e),
        }
    }
}

impl std::error::Error for PrefabError {}

/// Library of loaded prefabs.
#[derive(Default)]
pub struct PrefabLibrary {
    prefabs: std::collections::HashMap<String, Prefab>,
    prefab_dir: Option<PathBuf>,
}

impl PrefabLibrary {
    pub fn new() -> Self {
        Self::default()
    }

    /// Set the prefab directory.
    pub fn set_prefab_dir(&mut self, dir: PathBuf) {
        if !dir.exists() {
            let _ = std::fs::create_dir_all(&dir);
        }
        self.prefab_dir = Some(dir);
    }

    /// Load all prefabs from the prefab directory.
    pub fn load_all(&mut self) -> usize {
        let Some(dir) = &self.prefab_dir else {
            return 0;
        };

        let mut count = 0;

        if let Ok(entries) = std::fs::read_dir(dir) {
            for entry in entries.flatten() {
                let path = entry.path();
                if path.extension().map(|e| e == "prefab" || e == "json").unwrap_or(false) {
                    if let Ok(prefab) = Prefab::load(&path) {
                        self.prefabs.insert(prefab.name.clone(), prefab);
                        count += 1;
                    }
                }
            }
        }

        count
    }

    /// Get a prefab by name.
    pub fn get(&self, name: &str) -> Option<&Prefab> {
        self.prefabs.get(name)
    }

    /// Add a prefab to the library.
    pub fn add(&mut self, prefab: Prefab) {
        self.prefabs.insert(prefab.name.clone(), prefab);
    }

    /// Save a prefab to disk.
    pub fn save(&self, name: &str) -> Result<(), PrefabError> {
        let prefab = self.prefabs.get(name)
            .ok_or_else(|| PrefabError::InvalidPrefab("Prefab not found".to_string()))?;

        let dir = self.prefab_dir.as_ref()
            .ok_or_else(|| PrefabError::IoError("Prefab directory not set".to_string()))?;

        let path = dir.join(format!("{}.prefab", name));
        prefab.save(&path)
    }

    /// List all prefab names.
    pub fn list(&self) -> impl Iterator<Item = &str> {
        self.prefabs.keys().map(|s| s.as_str())
    }

    /// Get prefab count.
    pub fn count(&self) -> usize {
        self.prefabs.len()
    }
}
