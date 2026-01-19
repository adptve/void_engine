//! App Loader - Loads apps from directories and integrates all systems
//!
//! The AppLoader is responsible for:
//! - Loading and parsing app manifests
//! - Creating namespaces and script contexts
//! - Loading app shaders
//! - Executing entry scripts
//! - Managing app lifecycles
//!
//! # Architecture
//!
//! ```text
//! App Directory
//!     │
//!     ├── manifest.toml ──► ManifestParser ──► AppManifest
//!     │
//!     ├── scripts/
//!     │   └── main.vs ──► VoidScript ──► ScriptContext ──► PatchBus
//!     │
//!     └── assets/shaders/
//!         └── *.wgsl ──► ShaderPipeline ──► ShaderRegistry
//! ```
//!
//! # Usage
//!
//! ```ignore
//! use void_runtime::app_loader::AppLoader;
//! use void_kernel::Kernel;
//!
//! let mut kernel = Kernel::new(Default::default());
//! let mut loader = AppLoader::new(kernel.patch_bus().clone());
//!
//! // Load an app from directory
//! let app_id = loader.load_app("examples/synthwave-dreamscape")?;
//!
//! // The app is now running with:
//! // - Manifest parsed and registered
//! // - Shaders compiled and available
//! // - Entry script executed
//! // - Patches flowing through PatchBus
//! ```

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::Arc;

use parking_lot::RwLock;
use thiserror::Error;

use void_ir::{NamespaceHandle, PatchBus};
use void_kernel::{
    app::{AppId, AppManifest},
    manifest::{self, ExtendedManifest, ManifestError},
};
use void_script::{ScriptContext, VoidScript, ScriptError};
use void_shader::{ShaderPipeline, ShaderPipelineConfig, ShaderError, ShaderId};

/// Errors from app loading
#[derive(Debug, Error)]
pub enum AppLoadError {
    #[error("Manifest error: {0}")]
    Manifest(#[from] ManifestError),

    #[error("Script error: {0}")]
    Script(#[from] ScriptError),

    #[error("Shader error: {0}")]
    Shader(#[from] ShaderError),

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("App directory not found: {0}")]
    DirectoryNotFound(PathBuf),

    #[error("Entry script not found: {0}")]
    EntryScriptNotFound(PathBuf),

    #[error("App not found: {0:?}")]
    AppNotFound(AppId),
}

/// Result type for app loading operations
pub type AppLoadResult<T> = Result<T, AppLoadError>;

/// A loaded app instance
pub struct LoadedApp {
    /// App ID
    pub id: AppId,
    /// Extended manifest
    pub manifest: ExtendedManifest,
    /// Namespace handle for emitting patches
    pub handle: NamespaceHandle,
    /// Script engine instance
    pub script: VoidScript,
    /// Loaded shader IDs
    pub shaders: Vec<ShaderId>,
    /// App directory
    pub app_dir: PathBuf,
}

impl LoadedApp {
    /// Execute a script in this app's context
    pub fn execute_script(&mut self, script: &str) -> Result<void_script::Value, ScriptError> {
        self.script.execute(script)
    }

    /// Get the app's namespace handle
    pub fn handle(&self) -> &NamespaceHandle {
        &self.handle
    }
}

/// Configuration for the app loader
#[derive(Debug, Clone)]
pub struct AppLoaderConfig {
    /// Enable shader hot-reload
    pub shader_hot_reload: bool,
    /// Enable script hot-reload
    pub script_hot_reload: bool,
    /// Maximum loaded apps
    pub max_apps: usize,
}

impl Default for AppLoaderConfig {
    fn default() -> Self {
        Self {
            shader_hot_reload: true,
            script_hot_reload: true,
            max_apps: 64,
        }
    }
}

/// App loader for loading and managing app instances
pub struct AppLoader {
    config: AppLoaderConfig,
    /// Reference to the patch bus
    patch_bus: Arc<PatchBus>,
    /// Loaded apps
    apps: HashMap<AppId, LoadedApp>,
    /// Shader pipeline
    shader_pipeline: ShaderPipeline,
}

impl AppLoader {
    /// Create a new app loader
    pub fn new(patch_bus: Arc<PatchBus>) -> Self {
        Self::with_config(patch_bus, AppLoaderConfig::default())
    }

    /// Create a new app loader with configuration
    pub fn with_config(patch_bus: Arc<PatchBus>, config: AppLoaderConfig) -> Self {
        let shader_config = ShaderPipelineConfig {
            validate: true,
            hot_reload: config.shader_hot_reload,
            ..Default::default()
        };

        Self {
            config,
            patch_bus,
            apps: HashMap::new(),
            shader_pipeline: ShaderPipeline::new(shader_config),
        }
    }

    /// Load an app from a directory
    pub fn load_app(&mut self, app_path: impl AsRef<Path>) -> AppLoadResult<AppId> {
        let app_dir = app_path.as_ref().to_path_buf();

        if !app_dir.exists() {
            return Err(AppLoadError::DirectoryNotFound(app_dir));
        }

        // Parse manifest
        let manifest_path = app_dir.join("manifest.toml");
        let extended_manifest = manifest::load_extended_manifest(&manifest_path)?;

        log::info!(
            "Loading app '{}' v{} from {:?}",
            extended_manifest.manifest.name,
            extended_manifest.manifest.version,
            app_dir
        );

        // Create namespace for this app
        let namespace = void_ir::Namespace::new(&extended_manifest.manifest.name);
        let handle = self.patch_bus.register_namespace(namespace);
        let app_id = AppId::new();

        // Create script context with namespace handle
        let context = ScriptContext::new(handle.clone());
        let mut script = VoidScript::with_context(context);

        // Load shaders
        let shaders = self.load_app_shaders(&app_dir)?;

        // Execute entry script if specified
        if let Some(entry_script) = &extended_manifest.entry_script {
            let script_path = app_dir.join(entry_script);
            if script_path.exists() {
                log::debug!("Executing entry script: {:?}", script_path);
                let source = std::fs::read_to_string(&script_path)?;
                script.execute(&source)?;
            } else {
                log::warn!("Entry script not found: {:?}", script_path);
            }
        }

        // Create layers from manifest
        for layer_req in &extended_manifest.manifest.layers {
            // Emit layer creation patch through the handle
            let tx = void_ir::TransactionBuilder::new(handle.id())
                .patch(void_ir::Patch::new(
                    handle.id(),
                    void_ir::PatchKind::Layer(void_ir::patch::LayerPatch::create(
                        &layer_req.name,
                        layer_req.layer_type.clone(),
                        layer_req.priority,
                    )),
                ))
                .build();

            if let Err(e) = handle.submit(tx) {
                log::error!("Failed to create layer '{}': {:?}", layer_req.name, e);
            }
        }

        // Store loaded app
        let loaded_app = LoadedApp {
            id: app_id,
            manifest: extended_manifest,
            handle,
            script,
            shaders,
            app_dir,
        };

        self.apps.insert(app_id, loaded_app);

        log::info!("App loaded: {:?}", app_id);
        Ok(app_id)
    }

    /// Load shaders from an app's assets/shaders directory
    fn load_app_shaders(&mut self, app_dir: &Path) -> AppLoadResult<Vec<ShaderId>> {
        let shaders_dir = app_dir.join("assets").join("shaders");
        let mut shader_ids = Vec::new();

        if !shaders_dir.exists() {
            log::debug!("No shaders directory at {:?}", shaders_dir);
            return Ok(shader_ids);
        }

        // Find all .wgsl files
        for entry in std::fs::read_dir(&shaders_dir)? {
            let entry = entry?;
            let path = entry.path();

            if path.extension().map(|e| e == "wgsl").unwrap_or(false) {
                log::debug!("Loading shader: {:?}", path);

                let source = std::fs::read_to_string(&path)?;
                let name = path.file_stem()
                    .and_then(|s| s.to_str())
                    .unwrap_or("unnamed");

                match self.shader_pipeline.compile_shader(name, &source) {
                    Ok(id) => {
                        shader_ids.push(id);
                        log::info!("Compiled shader: {} -> {:?}", name, id);
                    }
                    Err(e) => {
                        log::error!("Failed to compile shader '{}': {:?}", name, e);
                        // Continue loading other shaders
                    }
                }
            }
        }

        Ok(shader_ids)
    }

    /// Unload an app
    pub fn unload_app(&mut self, id: AppId) -> AppLoadResult<()> {
        let app = self.apps.remove(&id)
            .ok_or(AppLoadError::AppNotFound(id))?;

        log::info!("Unloaded app '{}' ({:?})", app.manifest.manifest.name, id);
        Ok(())
    }

    /// Get a loaded app
    pub fn get_app(&self, id: AppId) -> Option<&LoadedApp> {
        self.apps.get(&id)
    }

    /// Get a mutable loaded app
    pub fn get_app_mut(&mut self, id: AppId) -> Option<&mut LoadedApp> {
        self.apps.get_mut(&id)
    }

    /// Get all loaded app IDs
    pub fn loaded_apps(&self) -> Vec<AppId> {
        self.apps.keys().copied().collect()
    }

    /// Get the shader pipeline
    pub fn shader_pipeline(&self) -> &ShaderPipeline {
        &self.shader_pipeline
    }

    /// Get mutable shader pipeline
    pub fn shader_pipeline_mut(&mut self) -> &mut ShaderPipeline {
        &mut self.shader_pipeline
    }

    /// Poll for shader hot-reload changes
    #[cfg(feature = "hot-reload")]
    pub fn poll_shader_changes(&mut self) {
        let results = self.shader_pipeline.poll_changes();
        for (path, id, result) in results {
            match result {
                Ok(_) => log::info!("Hot-reloaded shader: {:?}", path),
                Err(e) => log::error!("Failed to hot-reload shader {:?}: {:?}", path, e),
            }
        }
    }

    /// Update all loaded apps (call each frame)
    pub fn update(&mut self, delta: f32) {
        // Future: could run app update scripts here
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_app_loader_config_default() {
        let config = AppLoaderConfig::default();
        assert!(config.shader_hot_reload);
        assert!(config.script_hot_reload);
        assert_eq!(config.max_apps, 64);
    }
}
