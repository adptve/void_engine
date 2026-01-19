//! Manifest parser - TOML to AppManifest conversion
//!
//! Parses app manifest files (manifest.toml) into AppManifest structs
//! that the kernel can use to load and initialize apps.
//!
//! # Manifest Format
//!
//! ```toml
//! [package]
//! name = "my-app"
//! version = "1.0.0"
//! description = "My awesome app"
//! author = "Developer"
//!
//! [app]
//! entry_script = "scripts/main.vs"
//!
//! [[app.layers]]
//! name = "background"
//! type = "content"
//! priority = 0
//!
//! [[app.layers]]
//! name = "foreground"
//! type = "overlay"
//! priority = 100
//!
//! [resources]
//! max_entities = 10000
//! max_memory = 104857600  # 100MB
//! max_layers = 10
//!
//! [permissions]
//! network = false
//! filesystem = false
//! scripts = true
//! cross_app_read = false
//! ```

use crate::app::{AppManifest, AppPermissions, LayerRequest, ResourceRequirements};
use serde::Deserialize;
use std::path::Path;
use thiserror::Error;
use void_ir::patch::LayerType;

/// Errors from manifest parsing
#[derive(Debug, Error)]
pub enum ManifestError {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("TOML parse error: {0}")]
    Parse(#[from] toml::de::Error),

    #[error("Missing required field: {0}")]
    MissingField(String),

    #[error("Invalid layer type: {0}")]
    InvalidLayerType(String),

    #[error("Validation error: {0}")]
    Validation(String),
}

/// Result type for manifest operations
pub type ManifestResult<T> = Result<T, ManifestError>;

/// Raw TOML structure for package section
#[derive(Debug, Deserialize)]
struct PackageToml {
    name: String,
    version: String,
    description: Option<String>,
    author: Option<String>,
}

/// Raw TOML structure for layer request
#[derive(Debug, Deserialize)]
struct LayerToml {
    name: String,
    #[serde(rename = "type")]
    layer_type: String,
    #[serde(default)]
    priority: i32,
}

/// Raw TOML structure for app section
#[derive(Debug, Deserialize)]
struct AppToml {
    entry_script: Option<String>,
    #[serde(default)]
    layers: Vec<LayerToml>,
}

/// Raw TOML structure for resources section
#[derive(Debug, Deserialize, Default)]
struct ResourcesToml {
    max_entities: Option<u32>,
    max_memory: Option<u64>,
    max_layers: Option<u32>,
}

/// Raw TOML structure for permissions section
#[derive(Debug, Deserialize, Default)]
struct PermissionsToml {
    #[serde(default)]
    network: bool,
    #[serde(default)]
    filesystem: bool,
    #[serde(default)]
    scripts: bool,
    #[serde(default)]
    cross_app_read: bool,
}

/// Root TOML structure
#[derive(Debug, Deserialize)]
struct ManifestToml {
    package: PackageToml,
    #[serde(default)]
    app: Option<AppToml>,
    #[serde(default)]
    resources: Option<ResourcesToml>,
    #[serde(default)]
    permissions: Option<PermissionsToml>,
}

/// Parse layer type string to LayerType enum
fn parse_layer_type(s: &str) -> ManifestResult<LayerType> {
    match s.to_lowercase().as_str() {
        "content" => Ok(LayerType::Content),
        "effect" => Ok(LayerType::Effect),
        "overlay" => Ok(LayerType::Overlay),
        "portal" => Ok(LayerType::Portal),
        _ => Err(ManifestError::InvalidLayerType(s.to_string())),
    }
}

/// Parse a manifest from TOML string
pub fn parse_manifest(content: &str) -> ManifestResult<AppManifest> {
    let raw: ManifestToml = toml::from_str(content)?;

    // Convert layers
    let layers = if let Some(app) = &raw.app {
        app.layers
            .iter()
            .map(|l| {
                Ok(LayerRequest {
                    name: l.name.clone(),
                    layer_type: parse_layer_type(&l.layer_type)?,
                    priority: l.priority,
                })
            })
            .collect::<ManifestResult<Vec<_>>>()?
    } else {
        Vec::new()
    };

    // Convert resources
    let resources = raw.resources.map(|r| ResourceRequirements {
        max_entities: r.max_entities,
        max_memory: r.max_memory,
        max_layers: r.max_layers,
    }).unwrap_or_default();

    // Convert permissions
    let permissions = raw.permissions.map(|p| AppPermissions {
        network: p.network,
        filesystem: p.filesystem,
        scripts: p.scripts,
        cross_app_read: p.cross_app_read,
    }).unwrap_or_default();

    Ok(AppManifest {
        name: raw.package.name,
        version: raw.package.version,
        description: raw.package.description,
        author: raw.package.author,
        layers,
        resources,
        permissions,
    })
}

/// Load and parse a manifest from a file
pub fn load_manifest(path: impl AsRef<Path>) -> ManifestResult<AppManifest> {
    let content = std::fs::read_to_string(path)?;
    parse_manifest(&content)
}

/// Load manifest from an app directory (looks for manifest.toml)
pub fn load_app_manifest(app_dir: impl AsRef<Path>) -> ManifestResult<AppManifest> {
    let manifest_path = app_dir.as_ref().join("manifest.toml");
    load_manifest(&manifest_path)
}

/// Extended manifest with additional metadata not in AppManifest
#[derive(Debug, Clone)]
pub struct ExtendedManifest {
    /// Core manifest data
    pub manifest: AppManifest,
    /// Entry script path (relative to app directory)
    pub entry_script: Option<String>,
    /// App directory path
    pub app_dir: Option<std::path::PathBuf>,
}

/// Parse extended manifest from TOML with additional fields
pub fn parse_extended_manifest(content: &str, app_dir: Option<std::path::PathBuf>) -> ManifestResult<ExtendedManifest> {
    let raw: ManifestToml = toml::from_str(content)?;
    let manifest = parse_manifest(content)?;

    let entry_script = raw.app.as_ref().and_then(|a| a.entry_script.clone());

    Ok(ExtendedManifest {
        manifest,
        entry_script,
        app_dir,
    })
}

/// Load extended manifest from a file
pub fn load_extended_manifest(path: impl AsRef<Path>) -> ManifestResult<ExtendedManifest> {
    let path = path.as_ref();
    let content = std::fs::read_to_string(path)?;
    let app_dir = path.parent().map(|p| p.to_path_buf());
    parse_extended_manifest(&content, app_dir)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_minimal_manifest() {
        let content = r#"
[package]
name = "test-app"
version = "1.0.0"
"#;

        let manifest = parse_manifest(content).unwrap();
        assert_eq!(manifest.name, "test-app");
        assert_eq!(manifest.version, "1.0.0");
        assert!(manifest.description.is_none());
        assert!(manifest.layers.is_empty());
    }

    #[test]
    fn test_parse_full_manifest() {
        let content = r#"
[package]
name = "synthwave-dreamscape"
version = "1.0.0"
description = "A synthwave aesthetic experience"
author = "Void Engine Contributors"

[app]
entry_script = "scripts/main.vs"

[[app.layers]]
name = "sky"
type = "content"
priority = 0

[[app.layers]]
name = "sun"
type = "content"
priority = 10

[[app.layers]]
name = "grid"
type = "content"
priority = 20

[[app.layers]]
name = "scanlines"
type = "effect"
priority = 100

[resources]
max_entities = 1000
max_memory = 52428800
max_layers = 10

[permissions]
network = false
filesystem = false
scripts = true
cross_app_read = false
"#;

        let manifest = parse_manifest(content).unwrap();

        assert_eq!(manifest.name, "synthwave-dreamscape");
        assert_eq!(manifest.version, "1.0.0");
        assert_eq!(manifest.description.as_deref(), Some("A synthwave aesthetic experience"));
        assert_eq!(manifest.author.as_deref(), Some("Void Engine Contributors"));

        assert_eq!(manifest.layers.len(), 4);
        assert_eq!(manifest.layers[0].name, "sky");
        assert_eq!(manifest.layers[0].layer_type, LayerType::Content);
        assert_eq!(manifest.layers[0].priority, 0);

        assert_eq!(manifest.layers[3].name, "scanlines");
        assert_eq!(manifest.layers[3].layer_type, LayerType::Effect);

        assert_eq!(manifest.resources.max_entities, Some(1000));
        assert_eq!(manifest.resources.max_memory, Some(52428800));

        assert!(!manifest.permissions.network);
        assert!(manifest.permissions.scripts);
    }

    #[test]
    fn test_parse_layer_types() {
        assert_eq!(parse_layer_type("content").unwrap(), LayerType::Content);
        assert_eq!(parse_layer_type("Content").unwrap(), LayerType::Content);
        assert_eq!(parse_layer_type("CONTENT").unwrap(), LayerType::Content);
        assert_eq!(parse_layer_type("effect").unwrap(), LayerType::Effect);
        assert_eq!(parse_layer_type("overlay").unwrap(), LayerType::Overlay);
        assert_eq!(parse_layer_type("portal").unwrap(), LayerType::Portal);

        assert!(parse_layer_type("invalid").is_err());
    }

    #[test]
    fn test_parse_extended_manifest() {
        let content = r#"
[package]
name = "test-app"
version = "1.0.0"

[app]
entry_script = "scripts/main.vs"

[[app.layers]]
name = "content"
type = "content"
priority = 0
"#;

        let ext = parse_extended_manifest(content, Some(std::path::PathBuf::from("/apps/test"))).unwrap();

        assert_eq!(ext.manifest.name, "test-app");
        assert_eq!(ext.entry_script.as_deref(), Some("scripts/main.vs"));
        assert_eq!(ext.app_dir.as_ref().unwrap().to_str().unwrap(), "/apps/test");
    }
}
