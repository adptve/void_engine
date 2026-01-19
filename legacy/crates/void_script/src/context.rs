//! Script Context - Bridge between VoidScript and the Kernel
//!
//! The ScriptContext provides the connection from script builtins to the
//! kernel's IR patch system. When a script calls `spawn()` or `set_position()`,
//! the builtin uses this context to emit real patches to the PatchBus.
//!
//! # Architecture
//!
//! ```text
//! VoidScript Code
//!      │
//!      ▼
//! Builtin Function (e.g., spawn)
//!      │
//!      ▼
//! ScriptContext.emit_patch()
//!      │
//!      ▼
//! NamespaceHandle.submit()
//!      │
//!      ▼
//! PatchBus → Kernel → ECS World
//! ```
//!
//! # Usage
//!
//! ```ignore
//! use void_script::{VoidScript, ScriptContext};
//! use void_ir::{PatchBus, Namespace};
//!
//! // Create patch bus and namespace
//! let bus = PatchBus::default();
//! let namespace = Namespace::new("my_app");
//! let handle = bus.register_namespace(namespace);
//!
//! // Create context and script engine
//! let context = ScriptContext::new(handle);
//! let mut vs = VoidScript::with_context(context);
//!
//! // Now builtins emit real patches
//! vs.execute(r#"
//!     let player = spawn("player");
//!     set_position(player, 10, 0, 5);
//! "#)?;
//! ```

use parking_lot::RwLock;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;

use void_ir::{
    NamespaceHandle, NamespaceId, Patch, PatchKind, TransactionBuilder,
    patch::{EntityPatch, ComponentPatch, LayerPatch, EntityRef, LayerType},
    Value as IrValue,
};

use crate::value::Value;

/// Context for script execution providing kernel integration
#[derive(Clone)]
pub struct ScriptContext {
    /// Handle to the app's namespace for submitting patches
    pub handle: Arc<RwLock<Option<NamespaceHandle>>>,
    /// Local entity ID counter
    entity_counter: Arc<AtomicU64>,
    /// Map from script entity IDs to namespace entity refs
    entity_map: Arc<RwLock<HashMap<u64, EntityRef>>>,
    /// Whether to actually emit patches (false for testing/mock mode)
    live_mode: Arc<RwLock<bool>>,
}

impl ScriptContext {
    /// Create a new script context with a namespace handle
    pub fn new(handle: NamespaceHandle) -> Self {
        Self {
            handle: Arc::new(RwLock::new(Some(handle))),
            entity_counter: Arc::new(AtomicU64::new(1)),
            entity_map: Arc::new(RwLock::new(HashMap::new())),
            live_mode: Arc::new(RwLock::new(true)),
        }
    }

    /// Create a mock context (builtins return mock data, no patches emitted)
    pub fn mock() -> Self {
        Self {
            handle: Arc::new(RwLock::new(None)),
            entity_counter: Arc::new(AtomicU64::new(1)),
            entity_map: Arc::new(RwLock::new(HashMap::new())),
            live_mode: Arc::new(RwLock::new(false)),
        }
    }

    /// Check if context is in live mode (connected to kernel)
    pub fn is_live(&self) -> bool {
        *self.live_mode.read() && self.handle.read().is_some()
    }

    /// Get the namespace ID
    pub fn namespace_id(&self) -> Option<NamespaceId> {
        self.handle.read().as_ref().map(|h| h.id())
    }

    /// Generate a new local entity ID
    pub fn next_entity_id(&self) -> u64 {
        self.entity_counter.fetch_add(1, Ordering::SeqCst)
    }

    /// Register an entity mapping (script ID -> entity ref)
    pub fn register_entity(&self, script_id: u64, entity_ref: EntityRef) {
        self.entity_map.write().insert(script_id, entity_ref);
    }

    /// Look up entity ref from script ID
    pub fn get_entity_ref(&self, script_id: u64) -> Option<EntityRef> {
        self.entity_map.read().get(&script_id).copied()
    }

    /// Spawn an entity and emit the create patch
    ///
    /// Returns the script-side entity ID
    pub fn spawn_entity(
        &self,
        name: &str,
        archetype: Option<&str>,
        components: HashMap<String, IrValue>,
    ) -> Result<u64, String> {
        let script_id = self.next_entity_id();

        if let Some(handle) = self.handle.read().as_ref() {
            let ns_id = handle.id();
            let entity_ref = EntityRef::new(ns_id, script_id);

            // Create entity patch
            let mut patch = EntityPatch::create(ns_id, script_id);
            if let Some(arch) = archetype {
                patch = patch.with_archetype(arch);
            }
            for (comp_name, value) in components {
                patch = patch.with_component(comp_name, value);
            }

            // Submit transaction
            let tx = TransactionBuilder::new(ns_id)
                .patch(Patch::new(ns_id, PatchKind::Entity(patch)))
                .build();

            handle.submit(tx).map_err(|e| e.to_string())?;

            // Register mapping
            self.register_entity(script_id, entity_ref);

            log::debug!("Spawned entity {} ({}) via patch", name, script_id);
        }

        Ok(script_id)
    }

    /// Destroy an entity
    pub fn destroy_entity(&self, script_id: u64) -> Result<(), String> {
        let entity_ref = self.get_entity_ref(script_id)
            .ok_or_else(|| format!("Entity {} not found", script_id))?;

        if let Some(handle) = self.handle.read().as_ref() {
            let ns_id = handle.id();
            let patch = EntityPatch::destroy(entity_ref);

            let tx = TransactionBuilder::new(ns_id)
                .patch(Patch::new(ns_id, PatchKind::Entity(patch)))
                .build();

            handle.submit(tx).map_err(|e| e.to_string())?;

            // Remove from map
            self.entity_map.write().remove(&script_id);

            log::debug!("Destroyed entity {}", script_id);
        }

        Ok(())
    }

    /// Set a component on an entity
    pub fn set_component(
        &self,
        script_id: u64,
        component_name: &str,
        value: IrValue,
    ) -> Result<(), String> {
        let entity_ref = self.get_entity_ref(script_id)
            .ok_or_else(|| format!("Entity {} not found", script_id))?;

        if let Some(handle) = self.handle.read().as_ref() {
            let ns_id = handle.id();
            let patch = ComponentPatch::set(entity_ref, component_name, value);

            let tx = TransactionBuilder::new(ns_id)
                .patch(Patch::new(ns_id, PatchKind::Component(patch)))
                .build();

            handle.submit(tx).map_err(|e| e.to_string())?;

            log::debug!("Set component {} on entity {}", component_name, script_id);
        }

        Ok(())
    }

    /// Create a layer
    pub fn create_layer(
        &self,
        name: &str,
        layer_type: LayerType,
        priority: i32,
    ) -> Result<(), String> {
        if let Some(handle) = self.handle.read().as_ref() {
            let ns_id = handle.id();
            let patch = LayerPatch::create(name, layer_type, priority);

            let tx = TransactionBuilder::new(ns_id)
                .patch(Patch::new(ns_id, PatchKind::Layer(patch)))
                .build();

            handle.submit(tx).map_err(|e| e.to_string())?;

            log::debug!("Created layer {} (type: {:?}, priority: {})", name, layer_type, priority);
        }

        Ok(())
    }

    /// Set entity position (convenience for Transform component)
    pub fn set_position(&self, script_id: u64, x: f64, y: f64, z: f64) -> Result<(), String> {
        let position = IrValue::from([x, y, z]);
        let mut transform = std::collections::HashMap::new();
        transform.insert("position".to_string(), position);
        self.set_component(script_id, "Transform", IrValue::Object(transform))
    }

    /// Set entity rotation (convenience for Transform component)
    pub fn set_rotation(&self, script_id: u64, x: f64, y: f64, z: f64) -> Result<(), String> {
        let rotation = IrValue::from([x, y, z]);
        let mut transform = std::collections::HashMap::new();
        transform.insert("rotation".to_string(), rotation);
        self.set_component(script_id, "Transform", IrValue::Object(transform))
    }

    /// Set entity scale (convenience for Transform component)
    pub fn set_scale(&self, script_id: u64, x: f64, y: f64, z: f64) -> Result<(), String> {
        let scale = IrValue::from([x, y, z]);
        let mut transform = std::collections::HashMap::new();
        transform.insert("scale".to_string(), scale);
        self.set_component(script_id, "Transform", IrValue::Object(transform))
    }
}

impl Default for ScriptContext {
    fn default() -> Self {
        Self::mock()
    }
}

/// Convert VoidScript Value to IR Value
pub fn script_to_ir_value(value: &Value) -> IrValue {
    match value {
        Value::Null => IrValue::Null,
        Value::Bool(b) => IrValue::Bool(*b),
        Value::Int(i) => IrValue::Int(*i),
        Value::Float(f) => IrValue::Float(*f),
        Value::String(s) => IrValue::String(s.clone()),
        Value::Array(arr) => {
            IrValue::Array(arr.iter().map(script_to_ir_value).collect())
        }
        Value::Object(obj) => {
            IrValue::Object(obj.iter().map(|(k, v)| (k.clone(), script_to_ir_value(v))).collect())
        }
        Value::Function(_) | Value::Native(_) => IrValue::Null, // Functions can't be serialized
    }
}

/// Convert IR Value to VoidScript Value
pub fn ir_to_script_value(value: &IrValue) -> Value {
    match value {
        IrValue::Null => Value::Null,
        IrValue::Bool(b) => Value::Bool(*b),
        IrValue::Int(i) => Value::Int(*i),
        IrValue::Float(f) => Value::Float(*f),
        IrValue::String(s) => Value::String(s.clone()),
        IrValue::Vec2(v) => {
            let mut obj = HashMap::new();
            obj.insert("x".to_string(), Value::Float(v[0]));
            obj.insert("y".to_string(), Value::Float(v[1]));
            Value::Object(obj)
        }
        IrValue::Vec3(v) => {
            let mut obj = HashMap::new();
            obj.insert("x".to_string(), Value::Float(v[0]));
            obj.insert("y".to_string(), Value::Float(v[1]));
            obj.insert("z".to_string(), Value::Float(v[2]));
            Value::Object(obj)
        }
        IrValue::Vec4(v) => {
            let mut obj = HashMap::new();
            obj.insert("x".to_string(), Value::Float(v[0]));
            obj.insert("y".to_string(), Value::Float(v[1]));
            obj.insert("z".to_string(), Value::Float(v[2]));
            obj.insert("w".to_string(), Value::Float(v[3]));
            Value::Object(obj)
        }
        IrValue::Mat4(m) => {
            // Convert 4x4 matrix to array of arrays
            let cols: Vec<Value> = m.iter()
                .map(|col| Value::Array(col.iter().map(|v| Value::Float(*v)).collect()))
                .collect();
            Value::Array(cols)
        }
        IrValue::Array(arr) => {
            Value::Array(arr.iter().map(ir_to_script_value).collect())
        }
        IrValue::Object(obj) => {
            Value::Object(obj.iter().map(|(k, v)| (k.clone(), ir_to_script_value(v))).collect())
        }
        IrValue::Bytes(bytes) => {
            // Convert bytes to array of integers
            Value::Array(bytes.iter().map(|b| Value::Int(*b as i64)).collect())
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mock_context() {
        let ctx = ScriptContext::mock();
        assert!(!ctx.is_live());

        let id = ctx.next_entity_id();
        assert!(id > 0);
    }

    #[test]
    fn test_value_conversion() {
        let script_val = Value::Object({
            let mut m = HashMap::new();
            m.insert("x".to_string(), Value::Float(1.0));
            m.insert("y".to_string(), Value::Float(2.0));
            m
        });

        let ir_val = script_to_ir_value(&script_val);
        let back = ir_to_script_value(&ir_val);

        assert_eq!(script_val, back);
    }
}
