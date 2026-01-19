//! # Void Asset Server
//!
//! Real-time asset injection and hot-reload system for the Void Engine.
//!
//! ## Features
//!
//! - **File Watching**: Automatically detects changes to asset files
//! - **Hot-Reload**: Swap shaders, textures, meshes at runtime
//! - **Multiple Loaders**: Support for WGSL shaders, PNG/JPG textures, OBJ meshes, JSON scenes
//! - **Remote Source**: Connect to void-assets server for cloud-based asset management
//!
//! ## Local File Watching Example
//!
//! ```ignore
//! use void_asset_server::{AssetInjector, InjectorConfig};
//!
//! let config = InjectorConfig::default();
//! let mut injector = AssetInjector::new(config);
//! injector.start().expect("Failed to start injector");
//!
//! // In your game loop:
//! for event in injector.update() {
//!     match event {
//!         InjectionEvent::Loaded { path, .. } => println!("Loaded: {}", path),
//!         InjectionEvent::Reloaded { path, .. } => println!("Hot-reloaded: {}", path),
//!         _ => {}
//!     }
//! }
//! ```
//!
//! ## Remote Asset Source Example
//!
//! ```ignore
//! use void_asset_server::remote::{RemoteAssetSource, RemoteConfig, RemoteEvent};
//!
//! let config = RemoteConfig {
//!     base_url: "http://localhost:3001".to_string(),
//!     project_id: "my-project".to_string(),
//!     ..Default::default()
//! };
//!
//! let mut remote = RemoteAssetSource::new(config);
//! remote.connect().expect("Failed to connect");
//!
//! // In your game loop:
//! for event in remote.poll() {
//!     match event {
//!         RemoteEvent::SceneUpdated { scene_id, version } => {
//!             println!("Scene {} updated to v{}", scene_id, version);
//!             let scene = remote.fetch_scene(&scene_id).unwrap();
//!             // Reload the scene...
//!         }
//!         RemoteEvent::AssetUploaded { filename, .. } => {
//!             println!("New asset uploaded: {}", filename);
//!         }
//!         _ => {}
//!     }
//! }
//! ```

pub mod loaders;
pub mod watcher;
pub mod injector;

#[cfg(feature = "remote")]
pub mod remote;

pub use loaders::{
    ShaderAsset, ShaderLoader,
    TextureAsset, TextureLoader,
    MeshAsset, MeshLoader,
    SceneAsset, SceneLoader, EntityDef, TransformDef, ComponentDef,
};

pub use watcher::{FileWatcher, FileChange, FileChangeKind};
pub use injector::{AssetInjector, InjectorConfig, InjectionEvent, AssetKind};

#[cfg(feature = "remote")]
pub use remote::{RemoteAssetSource, RemoteConfig, RemoteEvent, SceneInfo, AssetInfo};
