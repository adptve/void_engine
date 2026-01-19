//! Asset Injector - unified asset loading and hot-reload pipeline

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::fs;

use parking_lot::RwLock;

use crate::watcher::{FileWatcher, FileChange, FileChangeKind};
use crate::loaders::{
    ShaderAsset, ShaderLoader,
    TextureAsset, TextureLoader,
    MeshAsset, MeshLoader,
    SceneAsset, SceneLoader,
};

/// Asset identifier (simple incrementing counter)
pub type AssetId = u64;

/// Asset kind/type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum AssetKind {
    Shader,
    Texture,
    Mesh,
    Scene,
    Unknown,
}

impl AssetKind {
    /// Determine asset kind from file extension
    pub fn from_path(path: &Path) -> Self {
        let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
        match ext.to_lowercase().as_str() {
            "wgsl" | "glsl" | "spv" => AssetKind::Shader,
            "png" | "jpg" | "jpeg" | "bmp" => AssetKind::Texture,
            "obj" | "gltf" | "glb" => AssetKind::Mesh,
            "json" => {
                // Check if it's a scene file
                if path.to_string_lossy().contains(".scene.") {
                    AssetKind::Scene
                } else {
                    AssetKind::Scene // Default JSON to scene
                }
            }
            _ => AssetKind::Unknown,
        }
    }
}

/// Events emitted by the asset injector
#[derive(Debug, Clone)]
pub enum InjectionEvent {
    /// Asset was loaded successfully
    Loaded {
        id: AssetId,
        path: String,
        kind: AssetKind,
    },
    /// Asset was reloaded (hot-reload)
    Reloaded {
        id: AssetId,
        path: String,
        kind: AssetKind,
        generation: u32,
    },
    /// Asset loading failed
    Failed {
        path: String,
        error: String,
    },
    /// Asset was removed
    Removed {
        id: AssetId,
        path: String,
    },
}

/// Loaded asset data (type-erased)
#[derive(Debug)]
pub enum LoadedAsset {
    Shader(ShaderAsset),
    Texture(TextureAsset),
    Mesh(MeshAsset),
    Scene(SceneAsset),
}

/// Asset entry in storage
struct AssetEntry {
    id: AssetId,
    path: String,
    kind: AssetKind,
    generation: u32,
    asset: LoadedAsset,
}

/// Configuration for the asset injector
#[derive(Debug, Clone)]
pub struct InjectorConfig {
    /// Directories to watch for changes
    pub watch_dirs: Vec<PathBuf>,
    /// Enable hot-reload file watching
    pub hot_reload: bool,
    /// Base asset directory
    pub asset_dir: PathBuf,
}

impl Default for InjectorConfig {
    fn default() -> Self {
        Self {
            watch_dirs: vec![PathBuf::from("assets")],
            hot_reload: true,
            asset_dir: PathBuf::from("assets"),
        }
    }
}

/// The main asset injector
pub struct AssetInjector {
    config: InjectorConfig,

    /// File watcher for hot-reload
    watcher: Option<FileWatcher>,

    /// Asset storage
    assets: RwLock<HashMap<AssetId, AssetEntry>>,

    /// Path to ID mapping
    path_to_id: RwLock<HashMap<String, AssetId>>,

    /// Next asset ID
    next_id: RwLock<AssetId>,

    /// Pending events
    events: RwLock<Vec<InjectionEvent>>,

    /// Texture loader configuration
    texture_loader: TextureLoader,
}

impl AssetInjector {
    /// Create a new asset injector
    pub fn new(config: InjectorConfig) -> Self {
        Self {
            config,
            watcher: None,
            assets: RwLock::new(HashMap::new()),
            path_to_id: RwLock::new(HashMap::new()),
            next_id: RwLock::new(1),
            events: RwLock::new(Vec::new()),
            texture_loader: TextureLoader::default(),
        }
    }

    /// Start the injector (begins file watching)
    pub fn start(&mut self) -> Result<(), String> {
        if self.config.hot_reload {
            let mut watcher = FileWatcher::new()?;

            for dir in &self.config.watch_dirs {
                // Make path absolute if relative
                let abs_path = if dir.is_relative() {
                    std::env::current_dir()
                        .map(|cwd| cwd.join(dir))
                        .unwrap_or_else(|_| dir.clone())
                } else {
                    dir.clone()
                };

                if let Err(e) = watcher.watch(&abs_path) {
                    log::warn!("Failed to watch {:?}: {}", abs_path, e);
                }
            }

            self.watcher = Some(watcher);
            log::info!("Asset injector started with hot-reload");
        } else {
            log::info!("Asset injector started (hot-reload disabled)");
        }

        Ok(())
    }

    /// Update the injector - poll for file changes
    pub fn update(&mut self) -> Vec<InjectionEvent> {
        // Poll file watcher
        if let Some(ref mut watcher) = self.watcher {
            for change in watcher.poll() {
                self.handle_file_change(change);
            }
        }

        // Drain and return events
        std::mem::take(&mut *self.events.write())
    }

    /// Handle a file change event
    fn handle_file_change(&self, change: FileChange) {
        let path_str = change.path.to_string_lossy().to_string();

        match change.kind {
            FileChangeKind::Created | FileChangeKind::Modified => {
                // Load or reload the asset
                if let Err(e) = self.load_file(&change.path) {
                    self.events.write().push(InjectionEvent::Failed {
                        path: path_str,
                        error: e,
                    });
                }
            }
            FileChangeKind::Deleted => {
                // Remove the asset
                if let Some(id) = self.path_to_id.write().remove(&path_str) {
                    self.assets.write().remove(&id);
                    self.events.write().push(InjectionEvent::Removed {
                        id,
                        path: path_str,
                    });
                }
            }
        }
    }

    /// Load an asset file
    pub fn load_file(&self, path: &Path) -> Result<AssetId, String> {
        let data = fs::read(path)
            .map_err(|e| format!("Failed to read {:?}: {}", path, e))?;

        let path_str = path.to_string_lossy().to_string();
        self.load_bytes(&path_str, &data)
    }

    /// Load an asset from a path string (relative to asset dir)
    pub fn load(&self, path: &str) -> Result<AssetId, String> {
        let full_path = self.config.asset_dir.join(path);
        self.load_file(&full_path)
    }

    /// Load an asset from bytes
    pub fn load_bytes(&self, path: &str, data: &[u8]) -> Result<AssetId, String> {
        let kind = AssetKind::from_path(Path::new(path));

        // Parse the asset
        let asset = match kind {
            AssetKind::Shader => {
                LoadedAsset::Shader(ShaderLoader::load(data, path)?)
            }
            AssetKind::Texture => {
                LoadedAsset::Texture(self.texture_loader.load(data, path)?)
            }
            AssetKind::Mesh => {
                LoadedAsset::Mesh(MeshLoader::load(data, path)?)
            }
            AssetKind::Scene => {
                LoadedAsset::Scene(SceneLoader::load(data, path)?)
            }
            AssetKind::Unknown => {
                return Err(format!("Unknown asset type for: {}", path));
            }
        };

        // Check if this is a reload
        let mut path_to_id = self.path_to_id.write();
        let mut assets = self.assets.write();

        if let Some(&existing_id) = path_to_id.get(path) {
            // Reload existing asset
            if let Some(entry) = assets.get_mut(&existing_id) {
                entry.generation += 1;
                entry.asset = asset;

                let generation = entry.generation;
                drop(assets);
                drop(path_to_id);

                self.events.write().push(InjectionEvent::Reloaded {
                    id: existing_id,
                    path: path.to_string(),
                    kind,
                    generation,
                });

                log::info!("Reloaded asset: {} (gen {})", path, generation);
                return Ok(existing_id);
            }
        }

        // New asset
        let id = {
            let mut next_id = self.next_id.write();
            let id = *next_id;
            *next_id += 1;
            id
        };

        path_to_id.insert(path.to_string(), id);
        assets.insert(id, AssetEntry {
            id,
            path: path.to_string(),
            kind,
            generation: 1,
            asset,
        });

        drop(assets);
        drop(path_to_id);

        self.events.write().push(InjectionEvent::Loaded {
            id,
            path: path.to_string(),
            kind,
        });

        log::info!("Loaded asset: {} (id {})", path, id);
        Ok(id)
    }

    /// Get an asset by ID
    pub fn get(&self, id: AssetId) -> Option<AssetRef<'_>> {
        let assets = self.assets.read();
        if assets.contains_key(&id) {
            Some(AssetRef {
                assets,
                id,
            })
        } else {
            None
        }
    }

    /// Get an asset by path
    pub fn get_by_path(&self, path: &str) -> Option<AssetId> {
        self.path_to_id.read().get(path).copied()
    }

    /// Get a shader asset
    pub fn get_shader(&self, id: AssetId) -> Option<ShaderAsset> {
        let assets = self.assets.read();
        assets.get(&id).and_then(|e| {
            if let LoadedAsset::Shader(s) = &e.asset {
                Some(s.clone())
            } else {
                None
            }
        })
    }

    /// Get a texture asset
    pub fn get_texture(&self, id: AssetId) -> Option<TextureAsset> {
        let assets = self.assets.read();
        assets.get(&id).and_then(|e| {
            if let LoadedAsset::Texture(t) = &e.asset {
                Some(t.clone())
            } else {
                None
            }
        })
    }

    /// Get a mesh asset
    pub fn get_mesh(&self, id: AssetId) -> Option<MeshAsset> {
        let assets = self.assets.read();
        assets.get(&id).and_then(|e| {
            if let LoadedAsset::Mesh(m) = &e.asset {
                Some(m.clone())
            } else {
                None
            }
        })
    }

    /// Get a scene asset
    pub fn get_scene(&self, id: AssetId) -> Option<SceneAsset> {
        let assets = self.assets.read();
        assets.get(&id).and_then(|e| {
            if let LoadedAsset::Scene(s) = &e.asset {
                Some(s.clone())
            } else {
                None
            }
        })
    }

    /// Get asset generation (for cache invalidation)
    pub fn get_generation(&self, id: AssetId) -> Option<u32> {
        self.assets.read().get(&id).map(|e| e.generation)
    }

    /// List all loaded assets
    pub fn list_assets(&self) -> Vec<(AssetId, String, AssetKind)> {
        self.assets
            .read()
            .values()
            .map(|e| (e.id, e.path.clone(), e.kind))
            .collect()
    }

    /// Preload all assets from a directory
    pub fn preload_directory(&self, dir: &Path) -> Result<Vec<AssetId>, String> {
        let mut ids = Vec::new();

        if !dir.exists() {
            return Err(format!("Directory does not exist: {:?}", dir));
        }

        for entry in fs::read_dir(dir).map_err(|e| format!("Failed to read dir: {}", e))? {
            let entry = entry.map_err(|e| format!("Failed to read entry: {}", e))?;
            let path = entry.path();

            if path.is_file() {
                let kind = AssetKind::from_path(&path);
                if kind != AssetKind::Unknown {
                    match self.load_file(&path) {
                        Ok(id) => ids.push(id),
                        Err(e) => log::warn!("Failed to preload {:?}: {}", path, e),
                    }
                }
            } else if path.is_dir() {
                // Recursively preload subdirectories
                match self.preload_directory(&path) {
                    Ok(sub_ids) => ids.extend(sub_ids),
                    Err(e) => log::warn!("Failed to preload {:?}: {}", path, e),
                }
            }
        }

        Ok(ids)
    }
}

/// Reference to a loaded asset
pub struct AssetRef<'a> {
    assets: parking_lot::RwLockReadGuard<'a, HashMap<AssetId, AssetEntry>>,
    id: AssetId,
}

impl<'a> AssetRef<'a> {
    /// Get the asset data
    pub fn data(&self) -> Option<&LoadedAsset> {
        self.assets.get(&self.id).map(|e| &e.asset)
    }

    /// Get the asset kind
    pub fn kind(&self) -> Option<AssetKind> {
        self.assets.get(&self.id).map(|e| e.kind)
    }

    /// Get the asset path
    pub fn path(&self) -> Option<&str> {
        self.assets.get(&self.id).map(|e| e.path.as_str())
    }

    /// Get the generation
    pub fn generation(&self) -> Option<u32> {
        self.assets.get(&self.id).map(|e| e.generation)
    }
}
