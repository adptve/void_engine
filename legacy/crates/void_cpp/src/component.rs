//! ECS component for C++ class attachment
//!
//! This component links an entity to a C++ class instance.

use crate::instance::InstanceId;
use crate::properties::PropertyMap;
use serde::{Deserialize, Serialize};
use std::path::PathBuf;

/// Component that attaches a C++ class to an entity
///
/// When this component is added to an entity, the CppSystem will
/// create a corresponding C++ class instance.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CppClassComponent {
    /// Path to the C++ library
    pub library: PathBuf,
    /// Name of the C++ class
    pub class_name: String,
    /// Whether hot-reload is enabled for this instance
    pub hot_reload: bool,
    /// Initial properties for the class
    pub properties: PropertyMap,
    /// Runtime: the instance ID (set by the system)
    #[serde(skip)]
    pub instance_id: Option<InstanceId>,
    /// Runtime: whether the instance has been created
    #[serde(skip)]
    pub initialized: bool,
}

impl CppClassComponent {
    /// Create a new C++ class component
    pub fn new(library: impl Into<PathBuf>, class_name: impl Into<String>) -> Self {
        Self {
            library: library.into(),
            class_name: class_name.into(),
            hot_reload: true,
            properties: PropertyMap::new(),
            instance_id: None,
            initialized: false,
        }
    }

    /// Set whether hot-reload is enabled
    pub fn with_hot_reload(mut self, enabled: bool) -> Self {
        self.hot_reload = enabled;
        self
    }

    /// Add a property
    pub fn with_property(mut self, key: impl Into<String>, value: crate::properties::CppPropertyValue) -> Self {
        self.properties.insert(key.into(), value);
        self
    }

    /// Set all properties at once
    pub fn with_properties(mut self, properties: PropertyMap) -> Self {
        self.properties = properties;
        self
    }

    /// Check if the instance has been created
    pub fn is_initialized(&self) -> bool {
        self.initialized && self.instance_id.is_some()
    }

    /// Get the instance ID if available
    pub fn instance_id(&self) -> Option<InstanceId> {
        self.instance_id
    }
}

impl Default for CppClassComponent {
    fn default() -> Self {
        Self {
            library: PathBuf::new(),
            class_name: String::new(),
            hot_reload: true,
            properties: PropertyMap::new(),
            instance_id: None,
            initialized: false,
        }
    }
}

/// Configuration for parsing C++ class from TOML
#[derive(Debug, Clone, Deserialize)]
pub struct CppClassConfig {
    /// Library path
    pub library: String,
    /// Class name
    pub class: String,
    /// Hot-reload enabled
    #[serde(default = "default_true")]
    pub hot_reload: bool,
    /// Properties
    #[serde(default)]
    pub properties: PropertyMap,
}

fn default_true() -> bool {
    true
}

impl From<CppClassConfig> for CppClassComponent {
    fn from(config: CppClassConfig) -> Self {
        Self {
            library: PathBuf::from(config.library),
            class_name: config.class,
            hot_reload: config.hot_reload,
            properties: config.properties,
            instance_id: None,
            initialized: false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::properties::CppPropertyValue;

    #[test]
    fn test_component_creation() {
        let component = CppClassComponent::new("game.dll", "PlayerController")
            .with_hot_reload(true)
            .with_property("max_health", CppPropertyValue::Float(100.0))
            .with_property("name", CppPropertyValue::String("Player".to_string()));

        assert_eq!(component.class_name, "PlayerController");
        assert!(component.hot_reload);
        assert!(!component.is_initialized());
        assert_eq!(component.properties.len(), 2);
    }

    #[test]
    fn test_config_parsing() {
        // Note: Properties use tagged enum format for bincode compatibility
        let toml = r#"
            library = "game.dll"
            class = "EnemyAI"
            hot_reload = false
        "#;

        let config: CppClassConfig = toml::from_str(toml).unwrap();
        let component = CppClassComponent::from(config);

        assert_eq!(component.class_name, "EnemyAI");
        assert!(!component.hot_reload);
        assert!(component.properties.is_empty());
    }
}
