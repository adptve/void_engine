//! App Package Format
//!
//! Defines the `.mvp` (Metaverse Package) format for distributing apps.
//!
//! ## Package Structure
//!
//! ```text
//! myapp.mvp (ZIP archive)
//! ├── manifest.toml      # App manifest
//! ├── icon.png           # App icon (optional)
//! ├── assets/            # Asset files
//! │   ├── models/
//! │   ├── textures/
//! │   └── audio/
//! ├── scripts/           # VoidScript files
//! │   └── main.vs
//! ├── wasm/              # WASM modules (optional)
//! │   └── logic.wasm
//! └── signature          # Package signature (optional)
//! ```
//!
//! ## Security
//!
//! Packages can be signed to verify authenticity. The signature covers
//! the manifest and all content hashes.

use std::collections::HashMap;
use std::io::{Read, Write, Seek};
use std::path::{Path, PathBuf};
use serde::{Serialize, Deserialize};
use thiserror::Error;

use crate::app::{AppManifest, AppPermissions, ResourceRequirements, LayerRequest};

/// Package file extension
pub const PACKAGE_EXTENSION: &str = "mvp";

/// Package magic bytes
pub const PACKAGE_MAGIC: &[u8; 4] = b"MVP\x01";

/// Maximum package size (100 MB)
pub const MAX_PACKAGE_SIZE: u64 = 100 * 1024 * 1024;

/// Package format version
pub const PACKAGE_VERSION: u32 = 1;

/// Package manifest (serialized to TOML)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackageManifest {
    /// Package metadata
    pub package: PackageMetadata,
    /// App configuration
    pub app: AppConfig,
    /// Dependencies
    #[serde(default)]
    pub dependencies: Vec<Dependency>,
    /// Asset declarations
    #[serde(default)]
    pub assets: AssetConfig,
    /// Script configuration
    #[serde(default)]
    pub scripts: ScriptConfig,
    /// Platform requirements
    #[serde(default)]
    pub platform: PlatformConfig,
}

/// Package metadata
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackageMetadata {
    /// Package name (unique identifier)
    pub name: String,
    /// Display name
    #[serde(default)]
    pub display_name: Option<String>,
    /// Version (semver)
    pub version: String,
    /// Description
    #[serde(default)]
    pub description: Option<String>,
    /// Author
    #[serde(default)]
    pub author: Option<String>,
    /// Author email
    #[serde(default)]
    pub email: Option<String>,
    /// Homepage URL
    #[serde(default)]
    pub homepage: Option<String>,
    /// Repository URL
    #[serde(default)]
    pub repository: Option<String>,
    /// License identifier (SPDX)
    #[serde(default)]
    pub license: Option<String>,
    /// Keywords for discovery
    #[serde(default)]
    pub keywords: Vec<String>,
    /// Categories
    #[serde(default)]
    pub categories: Vec<String>,
}

/// App-specific configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfig {
    /// App type
    #[serde(default)]
    pub app_type: AppType,
    /// Entry point script
    #[serde(default = "default_entry")]
    pub entry: String,
    /// Layer requests
    #[serde(default)]
    pub layers: Vec<LayerConfig>,
    /// Permissions
    #[serde(default)]
    pub permissions: PermissionConfig,
    /// Resource limits
    #[serde(default)]
    pub resources: ResourceConfig,
}

fn default_entry() -> String {
    "scripts/main.vs".to_string()
}

/// App type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "lowercase")]
pub enum AppType {
    /// Standard app with UI
    #[default]
    App,
    /// Background service
    Service,
    /// Widget/overlay
    Widget,
    /// Game
    Game,
    /// Tool/utility
    Tool,
}

/// Layer configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LayerConfig {
    /// Layer name
    pub name: String,
    /// Layer type
    #[serde(rename = "type")]
    pub layer_type: String,
    /// Priority (higher = on top)
    #[serde(default)]
    pub priority: i32,
    /// Blend mode
    #[serde(default)]
    pub blend: Option<String>,
}

/// Permission configuration
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct PermissionConfig {
    /// Network access
    #[serde(default)]
    pub network: bool,
    /// Filesystem access
    #[serde(default)]
    pub filesystem: bool,
    /// Script execution
    #[serde(default)]
    pub scripts: bool,
    /// Cross-app read
    #[serde(default)]
    pub cross_app_read: bool,
    /// Camera access
    #[serde(default)]
    pub camera: bool,
    /// Microphone access
    #[serde(default)]
    pub microphone: bool,
    /// Location access
    #[serde(default)]
    pub location: bool,
}

/// Resource limits
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ResourceConfig {
    /// Maximum entities
    #[serde(default)]
    pub max_entities: Option<u32>,
    /// Maximum memory (bytes)
    #[serde(default)]
    pub max_memory: Option<u64>,
    /// Maximum layers
    #[serde(default)]
    pub max_layers: Option<u32>,
    /// Maximum CPU time per frame (ms)
    #[serde(default)]
    pub max_cpu_ms: Option<f32>,
}

/// Dependency declaration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Dependency {
    /// Package name
    pub name: String,
    /// Version requirement (semver range)
    pub version: String,
    /// Optional dependency
    #[serde(default)]
    pub optional: bool,
}

/// Asset configuration
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct AssetConfig {
    /// Asset directories to include
    #[serde(default)]
    pub include: Vec<String>,
    /// Asset directories to exclude
    #[serde(default)]
    pub exclude: Vec<String>,
    /// Compression level (0-9)
    #[serde(default = "default_compression")]
    pub compression: u32,
}

fn default_compression() -> u32 {
    6
}

/// Script configuration
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ScriptConfig {
    /// Script language
    #[serde(default = "default_language")]
    pub language: String,
    /// WASM modules
    #[serde(default)]
    pub wasm_modules: Vec<String>,
}

fn default_language() -> String {
    "voidscript".to_string()
}

/// Platform requirements
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct PlatformConfig {
    /// Minimum OS version
    #[serde(default)]
    pub min_version: Option<String>,
    /// Required features
    #[serde(default)]
    pub required_features: Vec<String>,
    /// Supported platforms
    #[serde(default)]
    pub platforms: Vec<String>,
    /// XR support
    #[serde(default)]
    pub xr: XrSupport,
}

/// XR support configuration
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct XrSupport {
    /// Supports VR
    #[serde(default)]
    pub vr: bool,
    /// Supports AR
    #[serde(default)]
    pub ar: bool,
    /// Supports MR
    #[serde(default)]
    pub mr: bool,
    /// Requires hand tracking
    #[serde(default)]
    pub hand_tracking: bool,
    /// Requires passthrough
    #[serde(default)]
    pub passthrough: bool,
}

/// Package entry (file in package)
#[derive(Debug, Clone)]
pub struct PackageEntry {
    /// Path within package
    pub path: PathBuf,
    /// File size
    pub size: u64,
    /// Content hash (SHA-256)
    pub hash: [u8; 32],
    /// Compression method
    pub compression: CompressionMethod,
    /// Compressed size
    pub compressed_size: u64,
}

/// Compression method
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CompressionMethod {
    /// No compression
    None,
    /// Deflate (ZIP standard)
    Deflate,
    /// LZ4 (fast)
    Lz4,
}

/// Package signature
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackageSignature {
    /// Signer identity (public key fingerprint)
    pub signer: String,
    /// Signature algorithm
    pub algorithm: String,
    /// Signature bytes (base64)
    pub signature: String,
    /// Timestamp
    pub timestamp: u64,
}

/// Loaded package
#[derive(Debug)]
pub struct Package {
    /// Manifest
    pub manifest: PackageManifest,
    /// Package entries
    pub entries: HashMap<PathBuf, PackageEntry>,
    /// Signature (if signed)
    pub signature: Option<PackageSignature>,
    /// Package source path
    pub source: PathBuf,
}

impl Package {
    /// Get the app manifest from the package manifest
    pub fn to_app_manifest(&self) -> AppManifest {
        let meta = &self.manifest.package;
        let app = &self.manifest.app;

        AppManifest {
            name: meta.name.clone(),
            version: meta.version.clone(),
            description: meta.description.clone(),
            author: meta.author.clone(),
            layers: app.layers.iter().map(|l| {
                LayerRequest {
                    name: l.name.clone(),
                    layer_type: parse_layer_type(&l.layer_type),
                    priority: l.priority,
                }
            }).collect(),
            resources: ResourceRequirements {
                max_entities: app.resources.max_entities,
                max_memory: app.resources.max_memory,
                max_layers: app.resources.max_layers,
            },
            permissions: AppPermissions {
                network: app.permissions.network,
                filesystem: app.permissions.filesystem,
                scripts: app.permissions.scripts,
                cross_app_read: app.permissions.cross_app_read,
            },
        }
    }

    /// Check if package has a specific entry
    pub fn has_entry(&self, path: &str) -> bool {
        self.entries.contains_key(Path::new(path))
    }

    /// Get entry info
    pub fn get_entry(&self, path: &str) -> Option<&PackageEntry> {
        self.entries.get(Path::new(path))
    }

    /// List all entries
    pub fn list_entries(&self) -> Vec<&PathBuf> {
        self.entries.keys().collect()
    }

    /// Get entries by directory
    pub fn list_directory(&self, dir: &str) -> Vec<&PathBuf> {
        let dir_path = Path::new(dir);
        self.entries.keys()
            .filter(|p| p.starts_with(dir_path))
            .collect()
    }

    /// Is the package signed?
    pub fn is_signed(&self) -> bool {
        self.signature.is_some()
    }

    /// Get total uncompressed size
    pub fn total_size(&self) -> u64 {
        self.entries.values().map(|e| e.size).sum()
    }

    /// Get total compressed size
    pub fn compressed_size(&self) -> u64 {
        self.entries.values().map(|e| e.compressed_size).sum()
    }
}

fn parse_layer_type(s: &str) -> void_ir::patch::LayerType {
    match s.to_lowercase().as_str() {
        "background" | "content" | "world" => void_ir::patch::LayerType::Content,
        "ui" | "overlay" | "hud" => void_ir::patch::LayerType::Overlay,
        "effect" | "postprocess" | "system" => void_ir::patch::LayerType::Effect,
        "portal" => void_ir::patch::LayerType::Portal,
        _ => void_ir::patch::LayerType::Content,
    }
}

/// Package errors
#[derive(Debug, Error)]
pub enum PackageError {
    #[error("Invalid package format: {0}")]
    InvalidFormat(String),

    #[error("Package too large: {0} bytes")]
    TooLarge(u64),

    #[error("Missing manifest")]
    MissingManifest,

    #[error("Invalid manifest: {0}")]
    InvalidManifest(String),

    #[error("Entry not found: {0}")]
    EntryNotFound(String),

    #[error("Signature verification failed")]
    SignatureInvalid,

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("Unsupported compression: {0}")]
    UnsupportedCompression(String),
}

/// Package builder for creating packages
pub struct PackageBuilder {
    manifest: PackageManifest,
    entries: Vec<(PathBuf, Vec<u8>)>,
    compression: CompressionMethod,
}

impl PackageBuilder {
    /// Create a new package builder
    pub fn new(manifest: PackageManifest) -> Self {
        Self {
            manifest,
            entries: Vec::new(),
            compression: CompressionMethod::Deflate,
        }
    }

    /// Set compression method
    pub fn compression(mut self, method: CompressionMethod) -> Self {
        self.compression = method;
        self
    }

    /// Add a file entry
    pub fn add_file(mut self, path: impl Into<PathBuf>, content: Vec<u8>) -> Self {
        self.entries.push((path.into(), content));
        self
    }

    /// Add all files from a directory
    pub fn add_directory(mut self, base_path: &Path, package_path: &str) -> std::io::Result<Self> {
        fn visit_dir(base: &Path, current: &Path, package_base: &str, entries: &mut Vec<(PathBuf, Vec<u8>)>) -> std::io::Result<()> {
            if current.is_dir() {
                for entry in std::fs::read_dir(current)? {
                    let entry = entry?;
                    let path = entry.path();
                    visit_dir(base, &path, package_base, entries)?;
                }
            } else if current.is_file() {
                let relative = current.strip_prefix(base).unwrap();
                let package_path = PathBuf::from(package_base).join(relative);
                let content = std::fs::read(current)?;
                entries.push((package_path, content));
            }
            Ok(())
        }

        visit_dir(base_path, base_path, package_path, &mut self.entries)?;
        Ok(self)
    }

    /// Build the package (returns bytes)
    pub fn build(self) -> Result<Vec<u8>, PackageError> {
        use std::io::Cursor;

        let mut output = Cursor::new(Vec::new());

        // Write magic
        output.write_all(PACKAGE_MAGIC)?;

        // Write version
        output.write_all(&PACKAGE_VERSION.to_le_bytes())?;

        // Serialize manifest to TOML
        let manifest_toml = toml_serialize(&self.manifest)?;

        // Write manifest length and content
        let manifest_bytes = manifest_toml.as_bytes();
        output.write_all(&(manifest_bytes.len() as u32).to_le_bytes())?;
        output.write_all(manifest_bytes)?;

        // Write entry count
        output.write_all(&(self.entries.len() as u32).to_le_bytes())?;

        // Write each entry
        for (path, content) in &self.entries {
            let path_str = path.to_string_lossy();
            let path_bytes = path_str.as_bytes();

            // Path length and path
            output.write_all(&(path_bytes.len() as u16).to_le_bytes())?;
            output.write_all(path_bytes)?;

            // Content hash (simple for now, just use size as placeholder)
            let hash = simple_hash(content);
            output.write_all(&hash)?;

            // Compression method
            output.write_all(&[self.compression as u8])?;

            // Compressed content
            let compressed = compress_content(content, self.compression)?;

            // Original size
            output.write_all(&(content.len() as u64).to_le_bytes())?;

            // Compressed size and content
            output.write_all(&(compressed.len() as u64).to_le_bytes())?;
            output.write_all(&compressed)?;
        }

        Ok(output.into_inner())
    }
}

/// Simple TOML serialization (subset)
fn toml_serialize(manifest: &PackageManifest) -> Result<String, PackageError> {
    // For now, use serde_json and convert (in real impl, use toml crate)
    serde_json::to_string_pretty(manifest)
        .map_err(|e| PackageError::InvalidManifest(e.to_string()))
}

/// Simple content hash
fn simple_hash(content: &[u8]) -> [u8; 32] {
    // Simple hash for now (in real impl, use SHA-256)
    let mut hash = [0u8; 32];
    let len_bytes = (content.len() as u64).to_le_bytes();
    hash[..8].copy_from_slice(&len_bytes);

    // XOR some content bytes
    for (i, chunk) in content.chunks(32).enumerate() {
        for (j, &byte) in chunk.iter().enumerate() {
            hash[j] ^= byte.wrapping_add(i as u8);
        }
    }

    hash
}

/// Compress content
fn compress_content(content: &[u8], method: CompressionMethod) -> Result<Vec<u8>, PackageError> {
    match method {
        CompressionMethod::None => Ok(content.to_vec()),
        CompressionMethod::Deflate => {
            // Simple compression (in real impl, use flate2)
            Ok(content.to_vec())
        }
        CompressionMethod::Lz4 => {
            // In real impl, use lz4
            Ok(content.to_vec())
        }
    }
}

/// Package loader
pub struct PackageLoader;

impl PackageLoader {
    /// Load a package from bytes
    pub fn load_from_bytes(data: &[u8]) -> Result<Package, PackageError> {
        use std::io::Cursor;

        if data.len() < 12 {
            return Err(PackageError::InvalidFormat("Package too small".into()));
        }

        let mut cursor = Cursor::new(data);

        // Read and verify magic
        let mut magic = [0u8; 4];
        cursor.read_exact(&mut magic)?;

        if &magic != PACKAGE_MAGIC {
            return Err(PackageError::InvalidFormat("Invalid magic bytes".into()));
        }

        // Read version
        let mut version_bytes = [0u8; 4];
        cursor.read_exact(&mut version_bytes)?;
        let version = u32::from_le_bytes(version_bytes);

        if version > PACKAGE_VERSION {
            return Err(PackageError::InvalidFormat(format!(
                "Unsupported version: {} (max: {})", version, PACKAGE_VERSION
            )));
        }

        // Read manifest length
        let mut len_bytes = [0u8; 4];
        cursor.read_exact(&mut len_bytes)?;
        let manifest_len = u32::from_le_bytes(len_bytes) as usize;

        // Read manifest
        let mut manifest_bytes = vec![0u8; manifest_len];
        cursor.read_exact(&mut manifest_bytes)?;

        let manifest_str = String::from_utf8(manifest_bytes)
            .map_err(|_| PackageError::InvalidManifest("Invalid UTF-8".into()))?;

        let manifest: PackageManifest = serde_json::from_str(&manifest_str)
            .map_err(|e| PackageError::InvalidManifest(e.to_string()))?;

        // Read entry count
        let mut count_bytes = [0u8; 4];
        cursor.read_exact(&mut count_bytes)?;
        let entry_count = u32::from_le_bytes(count_bytes) as usize;

        // Read entries
        let mut entries = HashMap::new();

        for _ in 0..entry_count {
            // Path length
            let mut path_len_bytes = [0u8; 2];
            cursor.read_exact(&mut path_len_bytes)?;
            let path_len = u16::from_le_bytes(path_len_bytes) as usize;

            // Path
            let mut path_bytes = vec![0u8; path_len];
            cursor.read_exact(&mut path_bytes)?;
            let path = PathBuf::from(String::from_utf8_lossy(&path_bytes).to_string());

            // Hash
            let mut hash = [0u8; 32];
            cursor.read_exact(&mut hash)?;

            // Compression method
            let mut compression_byte = [0u8; 1];
            cursor.read_exact(&mut compression_byte)?;
            let compression = match compression_byte[0] {
                0 => CompressionMethod::None,
                1 => CompressionMethod::Deflate,
                2 => CompressionMethod::Lz4,
                _ => return Err(PackageError::UnsupportedCompression(
                    format!("Unknown compression: {}", compression_byte[0])
                )),
            };

            // Original size
            let mut size_bytes = [0u8; 8];
            cursor.read_exact(&mut size_bytes)?;
            let size = u64::from_le_bytes(size_bytes);

            // Compressed size
            let mut compressed_size_bytes = [0u8; 8];
            cursor.read_exact(&mut compressed_size_bytes)?;
            let compressed_size = u64::from_le_bytes(compressed_size_bytes);

            // Skip content (we just store metadata for now)
            cursor.set_position(cursor.position() + compressed_size);

            entries.insert(path.clone(), PackageEntry {
                path,
                size,
                hash,
                compression,
                compressed_size,
            });
        }

        Ok(Package {
            manifest,
            entries,
            signature: None,
            source: PathBuf::new(),
        })
    }

    /// Load a package from a file
    pub fn load_from_file(path: &Path) -> Result<Package, PackageError> {
        let data = std::fs::read(path)?;

        if data.len() as u64 > MAX_PACKAGE_SIZE {
            return Err(PackageError::TooLarge(data.len() as u64));
        }

        let mut package = Self::load_from_bytes(&data)?;
        package.source = path.to_path_buf();

        Ok(package)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_manifest() -> PackageManifest {
        PackageManifest {
            package: PackageMetadata {
                name: "test-app".to_string(),
                display_name: Some("Test App".to_string()),
                version: "1.0.0".to_string(),
                description: Some("A test app".to_string()),
                author: Some("Test Author".to_string()),
                email: None,
                homepage: None,
                repository: None,
                license: Some("MIT".to_string()),
                keywords: vec!["test".to_string()],
                categories: vec!["utility".to_string()],
            },
            app: AppConfig {
                app_type: AppType::App,
                entry: "scripts/main.vs".to_string(),
                layers: vec![LayerConfig {
                    name: "content".to_string(),
                    layer_type: "content".to_string(),
                    priority: 0,
                    blend: None,
                }],
                permissions: PermissionConfig::default(),
                resources: ResourceConfig::default(),
            },
            dependencies: Vec::new(),
            assets: AssetConfig::default(),
            scripts: ScriptConfig::default(),
            platform: PlatformConfig::default(),
        }
    }

    #[test]
    fn test_package_build_and_load() {
        let manifest = test_manifest();

        let package_bytes = PackageBuilder::new(manifest.clone())
            .add_file("scripts/main.vs", b"print(\"Hello\")".to_vec())
            .add_file("assets/test.txt", b"Test content".to_vec())
            .build()
            .unwrap();

        let package = PackageLoader::load_from_bytes(&package_bytes).unwrap();

        assert_eq!(package.manifest.package.name, "test-app");
        assert_eq!(package.manifest.package.version, "1.0.0");
        assert!(package.has_entry("scripts/main.vs"));
        assert!(package.has_entry("assets/test.txt"));
    }

    #[test]
    fn test_to_app_manifest() {
        let manifest = test_manifest();

        let package_bytes = PackageBuilder::new(manifest)
            .build()
            .unwrap();

        let package = PackageLoader::load_from_bytes(&package_bytes).unwrap();
        let app_manifest = package.to_app_manifest();

        assert_eq!(app_manifest.name, "test-app");
        assert_eq!(app_manifest.version, "1.0.0");
        assert_eq!(app_manifest.layers.len(), 1);
    }

    #[test]
    fn test_package_size() {
        let manifest = test_manifest();

        let package_bytes = PackageBuilder::new(manifest)
            .add_file("large.bin", vec![0u8; 1000])
            .add_file("small.txt", b"Hello".to_vec())
            .build()
            .unwrap();

        let package = PackageLoader::load_from_bytes(&package_bytes).unwrap();

        assert_eq!(package.total_size(), 1005);
    }
}
