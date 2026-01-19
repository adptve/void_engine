//! Plugin system - the heart of engine extensibility
//!
//! Everything in the Void Engine is a plugin. This provides:
//! - Maximum extensibility
//! - Hot-reload support
//! - Clean separation of concerns
//! - Dynamic loading/unloading

use core::any::Any;
use alloc::boxed::Box;
use alloc::vec::Vec;
use alloc::string::String;
use alloc::string::ToString;
use alloc::collections::BTreeMap;

use crate::version::Version;
use crate::error::{PluginError, Result};
use crate::type_registry::TypeRegistry;
use crate::id::NamedId;

/// Unique identifier for a plugin
#[derive(Clone, PartialEq, Eq, Hash, Debug)]
pub struct PluginId(NamedId);

impl PluginId {
    /// Create a new plugin ID
    pub fn new(name: &str) -> Self {
        Self(NamedId::new(name))
    }

    /// Get the plugin name
    pub fn name(&self) -> &str {
        self.0.name()
    }
}

impl From<&str> for PluginId {
    fn from(s: &str) -> Self {
        Self::new(s)
    }
}

/// State that can be preserved across hot-reloads
pub struct PluginState {
    /// Serialized state data
    pub data: Vec<u8>,
    /// Type identifier for validation
    pub type_name: String,
    /// Version at the time of snapshot
    pub version: Version,
}

impl PluginState {
    /// Create a new plugin state
    pub fn new(data: Vec<u8>, type_name: &str, version: Version) -> Self {
        Self {
            data,
            type_name: type_name.into(),
            version,
        }
    }

    /// Create an empty state
    pub fn empty() -> Self {
        Self {
            data: Vec::new(),
            type_name: String::new(),
            version: Version::ZERO,
        }
    }
}

/// The current state of a plugin
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PluginStatus {
    /// Plugin is registered but not loaded
    Registered,
    /// Plugin is currently loading
    Loading,
    /// Plugin is loaded and active
    Active,
    /// Plugin is being unloaded
    Unloading,
    /// Plugin failed to load
    Failed,
    /// Plugin is disabled
    Disabled,
}

/// Context provided to plugins during lifecycle events
pub struct PluginContext<'a> {
    /// Type registry for registering new types
    pub types: &'a mut TypeRegistry,
    /// Access to other plugins (read-only)
    pub plugins: &'a PluginRegistry,
    /// Custom data storage
    pub data: &'a mut BTreeMap<String, Box<dyn Any + Send + Sync>>,
}

impl<'a> PluginContext<'a> {
    /// Store custom data
    pub fn insert<T: Any + Send + Sync>(&mut self, key: &str, value: T) {
        self.data.insert(key.into(), Box::new(value));
    }

    /// Retrieve custom data
    pub fn get<T: Any + Send + Sync>(&self, key: &str) -> Option<&T> {
        self.data.get(key)?.downcast_ref()
    }

    /// Retrieve mutable custom data
    pub fn get_mut<T: Any + Send + Sync>(&mut self, key: &str) -> Option<&mut T> {
        self.data.get_mut(key)?.downcast_mut()
    }
}

/// The core plugin trait - everything implements this
pub trait Plugin: Send + Sync {
    /// Unique identifier for this plugin
    fn id(&self) -> PluginId;

    /// Plugin version for compatibility checking
    fn version(&self) -> Version {
        Version::new(0, 1, 0)
    }

    /// Dependencies on other plugins (by ID)
    fn dependencies(&self) -> Vec<PluginId> {
        Vec::new()
    }

    /// Called when the plugin is first loaded
    fn on_load(&mut self, ctx: &mut PluginContext) -> Result<()> {
        let _ = ctx;
        Ok(())
    }

    /// Called every frame while active
    fn on_update(&mut self, _dt: f32) {}

    /// Called when about to be unloaded - return state to preserve
    fn on_unload(&mut self, _ctx: &mut PluginContext) -> Result<PluginState> {
        Ok(PluginState::empty())
    }

    /// Called after hot-reload with preserved state
    fn on_reload(&mut self, ctx: &mut PluginContext, state: PluginState) -> Result<()> {
        let _ = (ctx, state);
        Ok(())
    }

    /// Register types this plugin provides
    fn register_types(&self, registry: &mut TypeRegistry) {
        let _ = registry;
    }

    /// Whether this plugin supports hot-reload
    fn supports_hot_reload(&self) -> bool {
        false
    }
}

/// Metadata about a registered plugin
pub struct PluginInfo {
    pub id: PluginId,
    pub version: Version,
    pub dependencies: Vec<PluginId>,
    pub status: PluginStatus,
    pub supports_hot_reload: bool,
}

/// Central registry for all plugins
pub struct PluginRegistry {
    plugins: BTreeMap<String, Box<dyn Plugin>>,
    info: BTreeMap<String, PluginInfo>,
    load_order: Vec<PluginId>,
}

impl PluginRegistry {
    /// Create a new plugin registry
    pub fn new() -> Self {
        Self {
            plugins: BTreeMap::new(),
            info: BTreeMap::new(),
            load_order: Vec::new(),
        }
    }

    /// Register a plugin without loading it
    pub fn register<P: Plugin + 'static>(&mut self, plugin: P) -> Result<()> {
        let id = plugin.id();
        let name = id.name().to_string();

        if self.plugins.contains_key(&name) {
            return Err(PluginError::AlreadyRegistered(name.into()).into());
        }

        let info = PluginInfo {
            id: id.clone(),
            version: plugin.version(),
            dependencies: plugin.dependencies(),
            status: PluginStatus::Registered,
            supports_hot_reload: plugin.supports_hot_reload(),
        };

        self.plugins.insert(name.clone(), Box::new(plugin));
        self.info.insert(name, info);
        Ok(())
    }

    /// Load a registered plugin
    pub fn load(
        &mut self,
        id: &PluginId,
        types: &mut TypeRegistry,
        data: &mut BTreeMap<String, Box<dyn Any + Send + Sync>>,
    ) -> Result<()> {
        let name = id.name().to_string();

        // Check if registered and get dependencies
        let dependencies = {
            let info = self.info.get(&name)
                .ok_or_else(|| PluginError::NotFound(name.clone().into()))?;
            info.dependencies.clone()
        };

        // Check dependencies
        for dep_id in &dependencies {
            let dep_name = dep_id.name();
            let dep_info = self.info.get(dep_name)
                .ok_or_else(|| PluginError::MissingDependency {
                    plugin: name.clone().into(),
                    dependency: dep_name.into(),
                })?;

            if dep_info.status != PluginStatus::Active {
                return Err(PluginError::MissingDependency {
                    plugin: name.clone().into(),
                    dependency: dep_name.into(),
                }.into());
            }
        }

        // Set status to loading
        self.info.get_mut(&name).unwrap().status = PluginStatus::Loading;

        // Take the plugin out temporarily to avoid borrow conflicts
        let mut plugin = self.plugins.remove(&name).unwrap();

        // Register types first
        plugin.register_types(types);

        // Create context and call on_load
        let mut ctx = PluginContext {
            types,
            plugins: self,
            data,
        };

        let result = plugin.on_load(&mut ctx);

        // Put the plugin back
        self.plugins.insert(name.clone(), plugin);

        if let Err(e) = result {
            self.info.get_mut(&name).unwrap().status = PluginStatus::Failed;
            return Err(e);
        }

        self.info.get_mut(&name).unwrap().status = PluginStatus::Active;
        self.load_order.push(id.clone());
        Ok(())
    }

    /// Unload a plugin
    pub fn unload(
        &mut self,
        id: &PluginId,
        types: &mut TypeRegistry,
        data: &mut BTreeMap<String, Box<dyn Any + Send + Sync>>,
    ) -> Result<PluginState> {
        let name = id.name().to_string();

        // Check status first
        {
            let info = self.info.get(&name)
                .ok_or_else(|| PluginError::NotFound(name.clone().into()))?;

            if info.status != PluginStatus::Active {
                return Err(PluginError::InvalidState(
                    "Plugin is not active".into()
                ).into());
            }
        }

        self.info.get_mut(&name).unwrap().status = PluginStatus::Unloading;

        // Take the plugin out temporarily to avoid borrow conflicts
        let mut plugin = self.plugins.remove(&name).unwrap();

        let mut ctx = PluginContext {
            types,
            plugins: self,
            data,
        };

        let state = plugin.on_unload(&mut ctx)?;

        // Put the plugin back
        self.plugins.insert(name.clone(), plugin);

        self.info.get_mut(&name).unwrap().status = PluginStatus::Registered;
        self.load_order.retain(|i| i.name() != name);

        Ok(state)
    }

    /// Hot-reload a plugin
    pub fn hot_reload(
        &mut self,
        id: &PluginId,
        new_plugin: Box<dyn Plugin>,
        types: &mut TypeRegistry,
        data: &mut BTreeMap<String, Box<dyn Any + Send + Sync>>,
    ) -> Result<()> {
        let name = id.name().to_string();

        // Check if the plugin supports hot-reload
        {
            let info = self.info.get(&name)
                .ok_or_else(|| PluginError::NotFound(name.clone().into()))?;

            if !info.supports_hot_reload {
                return Err(PluginError::InvalidState(
                    "Plugin does not support hot-reload".into()
                ).into());
            }
        }

        // Snapshot current state
        let state = self.unload(id, types, data)?;

        // Replace plugin
        self.plugins.insert(name.clone(), new_plugin);

        // Reload with state
        self.load(id, types, data)?;

        // Take the plugin out temporarily to avoid borrow conflicts
        let mut plugin = self.plugins.remove(&name).unwrap();

        let mut ctx = PluginContext {
            types,
            plugins: self,
            data,
        };

        let result = plugin.on_reload(&mut ctx, state);

        // Put the plugin back
        self.plugins.insert(name.clone(), plugin);

        result
    }

    /// Get a plugin by ID
    pub fn get(&self, id: &PluginId) -> Option<&dyn Plugin> {
        self.plugins.get(id.name()).map(|p| p.as_ref())
    }

    /// Get a mutable plugin by ID
    pub fn get_mut(&mut self, id: &PluginId) -> Option<&mut dyn Plugin> {
        match self.plugins.get_mut(id.name()) {
            Some(p) => Some(p.as_mut()),
            None => None,
        }
    }

    /// Get plugin info
    pub fn info(&self, id: &PluginId) -> Option<&PluginInfo> {
        self.info.get(id.name())
    }

    /// Iterate over all active plugins in load order
    pub fn active_plugins(&self) -> impl Iterator<Item = &dyn Plugin> {
        self.load_order.iter().filter_map(|id| self.get(id))
    }

    /// Update all active plugins
    pub fn update_all(&mut self, dt: f32) {
        for id in &self.load_order.clone() {
            if let Some(plugin) = self.plugins.get_mut(id.name()) {
                plugin.on_update(dt);
            }
        }
    }

    /// Get the number of registered plugins
    pub fn len(&self) -> usize {
        self.plugins.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.plugins.is_empty()
    }

    /// Get the number of active plugins
    pub fn active_count(&self) -> usize {
        self.load_order.len()
    }
}

impl Default for PluginRegistry {
    fn default() -> Self {
        Self::new()
    }
}

/// Marker trait for plugins that provide a specific capability
pub trait ProvidesCap<C: ?Sized>: Plugin {
    /// Get the capability
    fn provide(&self) -> &C;
    /// Get the capability mutably
    fn provide_mut(&mut self) -> &mut C;
}

/// Helper macro for defining plugins
#[macro_export]
macro_rules! define_plugin {
    ($name:ident, $id:literal, $version:expr) => {
        impl $crate::Plugin for $name {
            fn id(&self) -> $crate::PluginId {
                $crate::PluginId::new($id)
            }

            fn version(&self) -> $crate::Version {
                $version
            }
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestPlugin {
        loaded: bool,
    }

    impl Plugin for TestPlugin {
        fn id(&self) -> PluginId {
            PluginId::new("test_plugin")
        }

        fn version(&self) -> Version {
            Version::new(1, 0, 0)
        }

        fn on_load(&mut self, _ctx: &mut PluginContext) -> Result<()> {
            self.loaded = true;
            Ok(())
        }
    }

    #[test]
    fn test_plugin_registration() {
        let mut registry = PluginRegistry::new();
        let plugin = TestPlugin { loaded: false };

        registry.register(plugin).unwrap();
        assert!(registry.info(&PluginId::new("test_plugin")).is_some());
    }
}
