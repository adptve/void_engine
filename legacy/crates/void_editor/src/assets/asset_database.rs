//! Asset database for tracking and managing project assets.

use std::collections::HashMap;
use std::path::PathBuf;

use crate::panels::AssetType;

/// Unique identifier for an asset.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct AssetGuid(pub u64);

impl AssetGuid {
    /// Generate a new GUID from a path.
    pub fn from_path(path: &PathBuf) -> Self {
        use std::hash::{Hash, Hasher};
        let mut hasher = std::collections::hash_map::DefaultHasher::new();
        path.hash(&mut hasher);
        Self(hasher.finish())
    }
}

/// Metadata for an indexed asset.
#[derive(Clone, Debug)]
pub struct AssetMetadata {
    pub guid: AssetGuid,
    pub path: PathBuf,
    pub asset_type: AssetType,
    pub dependencies: Vec<AssetGuid>,
    pub last_modified: std::time::SystemTime,
    pub thumbnail: Option<PathBuf>,
}

/// Database of project assets.
pub struct AssetDatabase {
    /// Asset root directories
    roots: Vec<PathBuf>,
    /// Indexed assets by GUID
    assets: HashMap<AssetGuid, AssetMetadata>,
    /// Path to GUID lookup
    path_to_guid: HashMap<PathBuf, AssetGuid>,
    /// Whether database needs refresh
    dirty: bool,
}

impl Default for AssetDatabase {
    fn default() -> Self {
        Self::new()
    }
}

impl AssetDatabase {
    pub fn new() -> Self {
        Self {
            roots: Vec::new(),
            assets: HashMap::new(),
            path_to_guid: HashMap::new(),
            dirty: true,
        }
    }

    /// Add an asset root directory.
    pub fn add_root(&mut self, path: PathBuf) {
        if path.is_dir() && !self.roots.contains(&path) {
            self.roots.push(path);
            self.dirty = true;
        }
    }

    /// Remove an asset root directory.
    pub fn remove_root(&mut self, path: &PathBuf) {
        self.roots.retain(|p| p != path);
        self.dirty = true;
    }

    /// Get all root directories.
    pub fn roots(&self) -> &[PathBuf] {
        &self.roots
    }

    /// Refresh the asset index.
    pub fn refresh(&mut self) {
        self.assets.clear();
        self.path_to_guid.clear();

        for root in &self.roots.clone() {
            self.index_directory(root);
        }

        self.dirty = false;
    }

    fn index_directory(&mut self, dir: &PathBuf) {
        if let Ok(entries) = std::fs::read_dir(dir) {
            for entry in entries.flatten() {
                let path = entry.path();

                if path.is_dir() {
                    self.index_directory(&path);
                } else if path.is_file() {
                    self.index_file(&path);
                }
            }
        }
    }

    fn index_file(&mut self, path: &PathBuf) {
        let name = path.file_name()
            .map(|n| n.to_string_lossy().to_string())
            .unwrap_or_default();

        // Skip hidden files
        if name.starts_with('.') {
            return;
        }

        let asset_type = crate::panels::AssetType::from_filename(&name);

        // Skip unknown types
        if asset_type == AssetType::Unknown {
            return;
        }

        let guid = AssetGuid::from_path(path);
        let last_modified = std::fs::metadata(path)
            .and_then(|m| m.modified())
            .unwrap_or(std::time::SystemTime::UNIX_EPOCH);

        let metadata = AssetMetadata {
            guid,
            path: path.clone(),
            asset_type,
            dependencies: Vec::new(),
            last_modified,
            thumbnail: None,
        };

        self.path_to_guid.insert(path.clone(), guid);
        self.assets.insert(guid, metadata);
    }

    /// Get asset by GUID.
    pub fn get(&self, guid: AssetGuid) -> Option<&AssetMetadata> {
        self.assets.get(&guid)
    }

    /// Get asset by path.
    pub fn get_by_path(&self, path: &PathBuf) -> Option<&AssetMetadata> {
        self.path_to_guid.get(path).and_then(|guid| self.assets.get(guid))
    }

    /// Find GUID for a path.
    pub fn find_guid(&self, path: &PathBuf) -> Option<AssetGuid> {
        self.path_to_guid.get(path).copied()
    }

    /// Get all assets of a type.
    pub fn assets_of_type(&self, asset_type: AssetType) -> impl Iterator<Item = &AssetMetadata> {
        self.assets.values().filter(move |a| a.asset_type == asset_type)
    }

    /// Get total asset count.
    pub fn count(&self) -> usize {
        self.assets.len()
    }

    /// Check if database needs refresh.
    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    /// Search assets by name.
    pub fn search(&self, query: &str) -> impl Iterator<Item = &AssetMetadata> {
        let query_lower = query.to_lowercase();
        self.assets.values().filter(move |a| {
            a.path
                .file_name()
                .map(|n| n.to_string_lossy().to_lowercase().contains(&query_lower))
                .unwrap_or(false)
        })
    }
}
