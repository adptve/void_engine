//! C++ class instance management
//!
//! Each entity with a C++ class gets an instance that wraps the native object.

use crate::error::{CppError, Result};
use crate::ffi::*;
use crate::library::CppLibrary;
use crate::properties::PropertyMap;
use std::sync::Arc;
use void_ecs::prelude::Entity;

/// Unique identifier for a C++ class instance
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct InstanceId(pub u64);

impl InstanceId {
    /// Create a new instance ID
    pub fn new(id: u64) -> Self {
        Self(id)
    }
}

/// A live instance of a C++ class
pub struct CppClassInstance {
    /// Unique instance ID
    pub id: InstanceId,
    /// Class name
    pub class_name: String,
    /// Handle to the C++ object
    handle: CppHandle,
    /// Reference to the library (keeps it loaded)
    library: Arc<CppLibrary>,
    /// Cached vtable pointer
    vtable: *const FfiClassVTable,
    /// Associated entity
    entity: Entity,
    /// Whether BeginPlay has been called
    begun: bool,
    /// Properties
    properties: PropertyMap,
}

// Safety: The C++ object is managed by the library which is Send + Sync
unsafe impl Send for CppClassInstance {}
unsafe impl Sync for CppClassInstance {}

impl CppClassInstance {
    /// Create a new instance
    pub(crate) fn new(
        id: InstanceId,
        class_name: String,
        handle: CppHandle,
        library: Arc<CppLibrary>,
        vtable: *const FfiClassVTable,
        entity: Entity,
        properties: PropertyMap,
    ) -> Self {
        Self {
            id,
            class_name,
            handle,
            library,
            vtable,
            entity,
            begun: false,
            properties,
        }
    }

    /// Get the instance ID
    pub fn id(&self) -> InstanceId {
        self.id
    }

    /// Get the class name
    pub fn class_name(&self) -> &str {
        &self.class_name
    }

    /// Get the associated entity
    pub fn entity(&self) -> Entity {
        self.entity
    }

    /// Check if BeginPlay has been called
    pub fn has_begun(&self) -> bool {
        self.begun
    }

    /// Get the handle (for advanced FFI usage)
    pub fn handle(&self) -> CppHandle {
        self.handle
    }

    /// Get the properties
    pub fn properties(&self) -> &PropertyMap {
        &self.properties
    }

    /// Get mutable properties
    pub fn properties_mut(&mut self) -> &mut PropertyMap {
        &mut self.properties
    }

    /// Set the entity ID on the C++ side
    pub fn set_entity_id(&self) -> Result<()> {
        if self.handle.is_null() {
            return Err(CppError::InvalidState("Null handle".into()));
        }

        // Try to get set_entity_id function
        let set_entity_id: Option<SetEntityIdFn> = unsafe {
            self.library.get_symbol::<SetEntityIdFn>(b"void_set_entity_id\0")
                .map(|s| *s)
        };

        if let Some(func) = set_entity_id {
            func(self.handle, self.entity.into());
        }

        Ok(())
    }

    /// Set the world context on the C++ side
    pub fn set_world_context(&self, context: &FfiWorldContext) -> Result<()> {
        if self.handle.is_null() {
            return Err(CppError::InvalidState("Null handle".into()));
        }

        // Try to get set_world_context function
        let set_context: Option<SetWorldContextFn> = unsafe {
            self.library.get_symbol::<SetWorldContextFn>(b"void_set_world_context\0")
                .map(|s| *s)
        };

        if let Some(func) = set_context {
            func(self.handle, context);
        }

        Ok(())
    }

    // ========== Lifecycle Methods ==========

    /// Call BeginPlay on the C++ object
    pub fn begin_play(&mut self) {
        if self.handle.is_null() || self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(begin_play) = vtable.begin_play {
            begin_play(self.handle);
        }
        self.begun = true;
    }

    /// Call Tick on the C++ object
    pub fn tick(&self, delta_time: f32) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(tick) = vtable.tick {
            tick(self.handle, delta_time);
        }
    }

    /// Call FixedTick on the C++ object
    pub fn fixed_tick(&self, delta_time: f32) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(fixed_tick) = vtable.fixed_tick {
            fixed_tick(self.handle, delta_time);
        }
    }

    /// Call EndPlay on the C++ object
    pub fn end_play(&mut self) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(end_play) = vtable.end_play {
            end_play(self.handle);
        }
        self.begun = false;
    }

    // ========== Event Methods ==========

    /// Call OnCollisionEnter
    pub fn on_collision_enter(&self, other: Entity, hit: FfiHitResult) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(on_collision_enter) = vtable.on_collision_enter {
            on_collision_enter(self.handle, other.into(), hit);
        }
    }

    /// Call OnCollisionExit
    pub fn on_collision_exit(&self, other: Entity) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(on_collision_exit) = vtable.on_collision_exit {
            on_collision_exit(self.handle, other.into());
        }
    }

    /// Call OnTriggerEnter
    pub fn on_trigger_enter(&self, other: Entity) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(on_trigger_enter) = vtable.on_trigger_enter {
            on_trigger_enter(self.handle, other.into());
        }
    }

    /// Call OnTriggerExit
    pub fn on_trigger_exit(&self, other: Entity) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(on_trigger_exit) = vtable.on_trigger_exit {
            on_trigger_exit(self.handle, other.into());
        }
    }

    /// Call OnDamage
    pub fn on_damage(&self, damage_info: FfiDamageInfo) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(on_damage) = vtable.on_damage {
            on_damage(self.handle, damage_info);
        }
    }

    /// Call OnDeath
    pub fn on_death(&self, killer: Entity) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(on_death) = vtable.on_death {
            on_death(self.handle, killer.into());
        }
    }

    /// Call OnInteract
    pub fn on_interact(&self, interactor: Entity) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(on_interact) = vtable.on_interact {
            on_interact(self.handle, interactor.into());
        }
    }

    /// Call OnInputAction
    pub fn on_input_action(&self, action: FfiInputAction) {
        if self.handle.is_null() || !self.begun {
            return;
        }

        let vtable = unsafe { &*self.vtable };
        if let Some(on_input_action) = vtable.on_input_action {
            on_input_action(self.handle, action);
        }
    }

    // ========== Serialization for Hot-Reload ==========

    /// Serialize the C++ object's state
    pub fn serialize(&self) -> Result<Vec<u8>> {
        if self.handle.is_null() {
            return Ok(Vec::new());
        }

        let vtable = unsafe { &*self.vtable };

        // Check if serialization is supported
        let (get_size, serialize) = match (vtable.get_serialized_size, vtable.serialize) {
            (Some(gs), Some(s)) => (gs, s),
            _ => return Ok(Vec::new()), // No serialization support
        };

        // Get required size
        let size = get_size(self.handle);
        if size == 0 {
            return Ok(Vec::new());
        }

        // Allocate buffer and serialize
        let mut buffer = vec![0u8; size];
        let written = serialize(self.handle, buffer.as_mut_ptr(), buffer.len());
        buffer.truncate(written);

        Ok(buffer)
    }

    /// Deserialize state into the C++ object
    pub fn deserialize(&self, data: &[u8]) -> Result<bool> {
        if self.handle.is_null() || data.is_empty() {
            return Ok(false);
        }

        let vtable = unsafe { &*self.vtable };

        let deserialize = match vtable.deserialize {
            Some(d) => d,
            None => return Ok(false),
        };

        let success = deserialize(self.handle, data.as_ptr(), data.len());
        Ok(success)
    }

    /// Check if this instance supports serialization
    pub fn supports_serialization(&self) -> bool {
        let vtable = unsafe { &*self.vtable };
        vtable.serialize.is_some() && vtable.deserialize.is_some()
    }
}

impl Drop for CppClassInstance {
    fn drop(&mut self) {
        // Call EndPlay if needed
        if self.begun {
            self.end_play();
        }

        // Destroy the C++ object
        if !self.handle.is_null() {
            if let Err(e) = self.library.destroy_instance(&self.class_name, self.handle) {
                log::error!("Failed to destroy C++ instance: {}", e);
            }
        }
    }
}
