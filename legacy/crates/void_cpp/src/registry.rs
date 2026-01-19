//! C++ class registry
//!
//! Central registry that manages loaded libraries and class instances.

use crate::error::{CppError, Result};
use crate::ffi::*;
use crate::instance::{CppClassInstance, InstanceId};
use crate::library::CppLibrary;
use crate::properties::PropertyMap;
use parking_lot::RwLock;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use void_ecs::prelude::Entity;

/// Central registry for C++ classes and instances
pub struct CppClassRegistry {
    /// Loaded libraries by path
    libraries: RwLock<HashMap<PathBuf, Arc<CppLibrary>>>,
    /// Class name to library mapping
    class_to_library: RwLock<HashMap<String, PathBuf>>,
    /// Active instances by ID
    instances: RwLock<HashMap<InstanceId, CppClassInstance>>,
    /// Entity to instance mapping
    entity_to_instance: RwLock<HashMap<Entity, InstanceId>>,
    /// Next instance ID
    next_instance_id: AtomicU64,
    /// World context for C++ objects
    world_context: RwLock<FfiWorldContext>,
}

impl CppClassRegistry {
    /// Create a new empty registry
    pub fn new() -> Self {
        Self {
            libraries: RwLock::new(HashMap::new()),
            class_to_library: RwLock::new(HashMap::new()),
            instances: RwLock::new(HashMap::new()),
            entity_to_instance: RwLock::new(HashMap::new()),
            next_instance_id: AtomicU64::new(1),
            world_context: RwLock::new(FfiWorldContext::default()),
        }
    }

    /// Set the world context that will be passed to C++ objects
    pub fn set_world_context(&self, context: FfiWorldContext) {
        *self.world_context.write() = context;
    }

    // ========== Library Management ==========

    /// Load a C++ library
    pub fn load_library(&self, path: impl AsRef<Path>) -> Result<()> {
        let path = path.as_ref().to_path_buf();

        // Check if already loaded
        if self.libraries.read().contains_key(&path) {
            return Err(CppError::LibraryAlreadyLoaded(path));
        }

        // Load the library
        let library = Arc::new(CppLibrary::load(&path)?);

        // Register all classes
        {
            let mut class_map = self.class_to_library.write();
            for class_info in &library.info.classes {
                if class_map.contains_key(&class_info.name) {
                    log::warn!(
                        "Class '{}' already registered from another library",
                        class_info.name
                    );
                }
                class_map.insert(class_info.name.clone(), path.clone());
            }
        }

        // Store the library
        self.libraries.write().insert(path, library);

        Ok(())
    }

    /// Unload a library (and all its instances)
    pub fn unload_library(&self, path: impl AsRef<Path>) -> Result<()> {
        let path = path.as_ref().to_path_buf();

        // Get the library
        let library = self.libraries.write().remove(&path);
        let library = match library {
            Some(lib) => lib,
            None => return Ok(()), // Not loaded
        };

        // Remove class mappings
        {
            let mut class_map = self.class_to_library.write();
            class_map.retain(|_, lib_path| *lib_path != path);
        }

        // Destroy instances from this library
        let class_names: Vec<String> = library.class_names().iter().map(|s| s.to_string()).collect();
        {
            let mut instances = self.instances.write();
            let mut entity_map = self.entity_to_instance.write();

            // Find instances to remove
            let to_remove: Vec<InstanceId> = instances
                .iter()
                .filter(|(_, inst)| class_names.contains(&inst.class_name))
                .map(|(id, _)| *id)
                .collect();

            // Remove them
            for id in to_remove {
                if let Some(instance) = instances.remove(&id) {
                    entity_map.remove(&instance.entity());
                }
            }
        }

        log::info!("Unloaded C++ library '{}'", library.name());
        Ok(())
    }

    /// Check if a library is loaded
    pub fn is_library_loaded(&self, path: impl AsRef<Path>) -> bool {
        self.libraries.read().contains_key(path.as_ref())
    }

    /// Get list of loaded library paths
    pub fn loaded_libraries(&self) -> Vec<PathBuf> {
        self.libraries.read().keys().cloned().collect()
    }

    // ========== Class Information ==========

    /// Check if a class is registered
    pub fn has_class(&self, class_name: &str) -> bool {
        self.class_to_library.read().contains_key(class_name)
    }

    /// Get all registered class names
    pub fn class_names(&self) -> Vec<String> {
        self.class_to_library.read().keys().cloned().collect()
    }

    /// Get the library that contains a class
    fn get_library_for_class(&self, class_name: &str) -> Result<Arc<CppLibrary>> {
        let path = self.class_to_library.read()
            .get(class_name)
            .cloned()
            .ok_or_else(|| CppError::ClassNotFound(class_name.to_string()))?;

        self.libraries.read()
            .get(&path)
            .cloned()
            .ok_or_else(|| CppError::ClassNotFound(class_name.to_string()))
    }

    // ========== Instance Management ==========

    /// Create a new instance of a C++ class for an entity
    pub fn create_instance(
        &self,
        class_name: &str,
        entity: Entity,
        properties: PropertyMap,
    ) -> Result<InstanceId> {
        // Check if entity already has an instance
        if self.entity_to_instance.read().contains_key(&entity) {
            return Err(CppError::InvalidState(
                format!("Entity {:?} already has a C++ instance", entity)
            ));
        }

        // Get the library
        let library = self.get_library_for_class(class_name)?;

        // Create the handle
        let handle = library.create_instance(class_name)?;

        // Get vtable
        let vtable = library.get_class_vtable(class_name)?;

        // Allocate instance ID
        let id = InstanceId::new(self.next_instance_id.fetch_add(1, Ordering::Relaxed));

        // Create instance
        let instance = CppClassInstance::new(
            id,
            class_name.to_string(),
            handle,
            library,
            vtable,
            entity,
            properties,
        );

        // Set entity ID on C++ side
        instance.set_entity_id()?;

        // Set world context
        instance.set_world_context(&self.world_context.read())?;

        // Store instance
        self.instances.write().insert(id, instance);
        self.entity_to_instance.write().insert(entity, id);

        log::debug!("Created C++ instance {} of class '{}' for entity {:?}", id.0, class_name, entity);

        Ok(id)
    }

    /// Destroy an instance
    pub fn destroy_instance(&self, id: InstanceId) -> Result<()> {
        let instance = self.instances.write().remove(&id);

        if let Some(instance) = instance {
            self.entity_to_instance.write().remove(&instance.entity());
            log::debug!("Destroyed C++ instance {} of class '{}'", id.0, instance.class_name());
        }

        Ok(())
    }

    /// Destroy instance by entity
    pub fn destroy_instance_for_entity(&self, entity: Entity) -> Result<()> {
        let id = self.entity_to_instance.write().remove(&entity);

        if let Some(id) = id {
            self.instances.write().remove(&id);
        }

        Ok(())
    }

    /// Get an instance by ID
    pub fn get_instance(&self, id: InstanceId) -> Option<impl std::ops::Deref<Target = CppClassInstance> + '_> {
        let instances = self.instances.read();
        if instances.contains_key(&id) {
            Some(parking_lot::RwLockReadGuard::map(instances, |m| m.get(&id).unwrap()))
        } else {
            None
        }
    }

    /// Get mutable instance by ID
    pub fn get_instance_mut(&self, id: InstanceId) -> Option<impl std::ops::DerefMut<Target = CppClassInstance> + '_> {
        let instances = self.instances.write();
        if instances.contains_key(&id) {
            Some(parking_lot::RwLockWriteGuard::map(instances, |m| m.get_mut(&id).unwrap()))
        } else {
            None
        }
    }

    /// Get instance ID for an entity
    pub fn get_instance_for_entity(&self, entity: Entity) -> Option<InstanceId> {
        self.entity_to_instance.read().get(&entity).copied()
    }

    /// Get number of active instances
    pub fn instance_count(&self) -> usize {
        self.instances.read().len()
    }

    // ========== Lifecycle Methods ==========

    /// Call BeginPlay on all instances that haven't started
    pub fn begin_play_all(&self) {
        let mut instances = self.instances.write();
        for instance in instances.values_mut() {
            if !instance.has_begun() {
                instance.begin_play();
            }
        }
    }

    /// Call Tick on all instances
    pub fn tick_all(&self, delta_time: f32) {
        let instances = self.instances.read();
        for instance in instances.values() {
            instance.tick(delta_time);
        }
    }

    /// Call FixedTick on all instances
    pub fn fixed_tick_all(&self, delta_time: f32) {
        let instances = self.instances.read();
        for instance in instances.values() {
            instance.fixed_tick(delta_time);
        }
    }

    /// Call EndPlay on all instances
    pub fn end_play_all(&self) {
        let mut instances = self.instances.write();
        for instance in instances.values_mut() {
            instance.end_play();
        }
    }

    // ========== Hot-Reload Support ==========

    /// Prepare for hot-reload of a library
    /// Returns serialized state of all instances from the library
    pub fn prepare_reload(&self, path: impl AsRef<Path>) -> Result<HashMap<InstanceId, (Entity, String, PropertyMap, Vec<u8>)>> {
        let path = path.as_ref();

        let library = self.libraries.read()
            .get(path)
            .cloned()
            .ok_or_else(|| CppError::LoadError {
                path: path.to_path_buf(),
                message: "Library not loaded".into(),
            })?;

        let class_names: Vec<String> = library.class_names().iter().map(|s| s.to_string()).collect();

        let instances = self.instances.read();
        let mut states = HashMap::new();

        for (id, instance) in instances.iter() {
            if class_names.contains(&instance.class_name) {
                let state = instance.serialize()?;
                states.insert(
                    *id,
                    (
                        instance.entity(),
                        instance.class_name().to_string(),
                        instance.properties().clone(),
                        state,
                    ),
                );
            }
        }

        Ok(states)
    }

    /// Complete hot-reload: recreate instances with saved state
    pub fn complete_reload(
        &self,
        path: impl AsRef<Path>,
        states: HashMap<InstanceId, (Entity, String, PropertyMap, Vec<u8>)>,
    ) -> Result<()> {
        let path = path.as_ref();

        // Unload old library
        self.unload_library(path)?;

        // Load new library
        self.load_library(path)?;

        // Recreate instances
        for (old_id, (entity, class_name, properties, state)) in states {
            match self.create_instance(&class_name, entity, properties) {
                Ok(new_id) => {
                    // Restore state
                    if let Some(mut instance) = self.get_instance_mut(new_id) {
                        if let Err(e) = instance.deserialize(&state) {
                            log::error!("Failed to restore state for instance {}: {}", old_id.0, e);
                        }
                        // Call BeginPlay
                        instance.begin_play();
                    }
                }
                Err(e) => {
                    log::error!(
                        "Failed to recreate instance {} of class '{}': {}",
                        old_id.0,
                        class_name,
                        e
                    );
                }
            }
        }

        Ok(())
    }
}

impl Default for CppClassRegistry {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_registry_creation() {
        let registry = CppClassRegistry::new();
        assert_eq!(registry.instance_count(), 0);
        assert!(registry.loaded_libraries().is_empty());
        assert!(registry.class_names().is_empty());
    }
}
