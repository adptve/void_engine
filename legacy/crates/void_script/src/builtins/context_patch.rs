//! Context-aware patch built-in functions
//!
//! These functions provide direct access to the IR patch system, allowing
//! scripts to emit arbitrary patches to the kernel.
//!
//! # Functions
//!
//! - `emit_patch(patch_object)` - Emit an IR patch to the kernel
//! - `get_namespace()` - Get the current app's namespace ID
//!
//! # Patch Format
//!
//! ```javascript
//! emit_patch({
//!     type: "entity",           // "entity", "component", "layer", "asset"
//!     entity: {                  // For entity/component patches
//!         namespace: get_namespace(),
//!         local_id: 10
//!     },
//!     op: "create",              // Operation type
//!     archetype: "PBRModel",     // Optional archetype
//!     components: {              // Component data
//!         "Transform": { position: [0, 0, 0], ... },
//!         "Material": { shader: "pbr.wgsl", ... }
//!     }
//! });
//! ```

use crate::context::{ScriptContext, script_to_ir_value};
use crate::interpreter::Interpreter;
use crate::value::Value;
use std::collections::HashMap;

use void_ir::{
    NamespaceId, Patch, PatchKind, TransactionBuilder,
    patch::{EntityPatch, ComponentPatch, LayerPatch, EntityRef, LayerType},
    Value as IrValue,
};

/// Helper to extract string from Value
fn value_as_string(v: &Value) -> Option<String> {
    match v {
        Value::String(s) => Some(s.clone()),
        _ => None,
    }
}

/// Helper to extract bool from Value
fn value_as_bool(v: &Value) -> Option<bool> {
    match v {
        Value::Bool(b) => Some(*b),
        _ => None,
    }
}

/// Register context-aware patch functions
pub fn register(interpreter: &mut Interpreter, context: ScriptContext) {
    register_emit_patch(interpreter, context.clone());
    register_get_namespace(interpreter, context.clone());
    register_get_keyboard_state(interpreter);
    register_get_mouse_state(interpreter);
}

/// emit_patch(patch_object) - Emit an IR patch directly
fn register_emit_patch(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native("emit_patch", move |args| {
        if args.is_empty() {
            return Err("emit_patch() requires a patch object".to_string());
        }

        let patch_obj = match &args[0] {
            Value::Object(obj) => obj,
            _ => return Err("emit_patch() expects an object".to_string()),
        };

        // Parse patch type
        let patch_type = patch_obj.get("type")
            .and_then(value_as_string)
            .ok_or("Patch missing 'type' field")?;

        // Get namespace from context
        let ns_id = context.namespace_id()
            .ok_or("No namespace available")?;

        // Build the patch based on type
        let patch_kind = match patch_type.as_str() {
            "entity" => parse_entity_patch(patch_obj, ns_id)?,
            "component" => parse_component_patch(patch_obj, ns_id)?,
            "layer" => parse_layer_patch(patch_obj)?,
            _ => return Err(format!("Unknown patch type: {}", patch_type)),
        };

        // Submit the patch
        if context.is_live() {
            // Access handle through the context
            let handle_guard = context.handle.read();
            if let Some(handle) = handle_guard.as_ref() {
                let tx = TransactionBuilder::new(ns_id)
                    .patch(Patch::new(ns_id, patch_kind))
                    .build();

                handle.submit(tx).map_err(|e| e.to_string())?;
            }
        }

        Ok(Value::Bool(true))
    });
}

/// get_namespace() - Get current namespace ID
fn register_get_namespace(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native("get_namespace", move |_args| {
        if let Some(ns_id) = context.namespace_id() {
            Ok(Value::Int(ns_id.as_u64() as i64))
        } else {
            Ok(Value::Int(0))
        }
    });
}

/// get_keyboard_state() - Get current keyboard state (stub for now)
fn register_get_keyboard_state(interpreter: &mut Interpreter) {
    interpreter.register_native("get_keyboard_state", move |_args| {
        // Return a keyboard state object (stub)
        let mut state = HashMap::new();
        state.insert("key_1".to_string(), Value::Bool(false));
        state.insert("key_2".to_string(), Value::Bool(false));
        state.insert("key_3".to_string(), Value::Bool(false));
        state.insert("key_4".to_string(), Value::Bool(false));
        state.insert("tab_pressed".to_string(), Value::Bool(false));
        state.insert("escape".to_string(), Value::Bool(false));
        state.insert("space".to_string(), Value::Bool(false));
        Ok(Value::Object(state))
    });
}

/// get_mouse_state() - Get current mouse state (stub for now)
fn register_get_mouse_state(interpreter: &mut Interpreter) {
    interpreter.register_native("get_mouse_state", move |_args| {
        // Return a mouse state object (stub)
        let mut state = HashMap::new();
        state.insert("x".to_string(), Value::Float(0.0));
        state.insert("y".to_string(), Value::Float(0.0));
        state.insert("left_button".to_string(), Value::Bool(false));
        state.insert("right_button".to_string(), Value::Bool(false));
        state.insert("scroll_delta".to_string(), Value::Float(0.0));
        Ok(Value::Object(state))
    });
}

/// Parse entity patch from script object
fn parse_entity_patch(obj: &HashMap<String, Value>, ns_id: NamespaceId) -> Result<PatchKind, String> {
    let op = obj.get("op")
        .and_then(value_as_string)
        .ok_or("Entity patch missing 'op' field")?;

    // Get entity reference
    let entity_ref = parse_entity_ref(obj, ns_id)?;

    match op.as_str() {
        "create" => {
            let archetype = obj.get("archetype")
                .and_then(value_as_string);

            let mut patch = EntityPatch::create(ns_id, entity_ref.local_id);

            if let Some(arch) = archetype {
                patch = patch.with_archetype(&arch);
            }

            // Add components
            if let Some(Value::Object(components)) = obj.get("components") {
                for (comp_name, comp_value) in components {
                    let ir_value = script_to_ir_value(comp_value);
                    patch = patch.with_component(comp_name.clone(), ir_value);
                }
            }

            Ok(PatchKind::Entity(patch))
        }
        "destroy" => {
            let patch = EntityPatch::destroy(entity_ref);
            Ok(PatchKind::Entity(patch))
        }
        // TODO: Add enable/disable when EntityPatch supports them
        _ => Err(format!("Unknown entity op: {}", op)),
    }
}

/// Parse component patch from script object
fn parse_component_patch(obj: &HashMap<String, Value>, ns_id: NamespaceId) -> Result<PatchKind, String> {
    let op = obj.get("op")
        .and_then(value_as_string)
        .ok_or("Component patch missing 'op' field")?;

    let entity_ref = parse_entity_ref(obj, ns_id)?;

    let component = obj.get("component")
        .and_then(value_as_string)
        .ok_or("Component patch missing 'component' field")?;

    match op.as_str() {
        "set" => {
            let data = obj.get("data")
                .ok_or("Component set missing 'data' field")?;
            let ir_value = script_to_ir_value(data);
            let patch = ComponentPatch::set(entity_ref, &component, ir_value);
            Ok(PatchKind::Component(patch))
        }
        "update" => {
            let fields = obj.get("fields")
                .ok_or("Component update missing 'fields' field")?;
            let ir_value = script_to_ir_value(fields);

            // Extract fields from the object
            let fields_map = match ir_value {
                IrValue::Object(map) => map,
                _ => return Err("Component update 'fields' must be an object".to_string()),
            };

            let patch = ComponentPatch::update(entity_ref, &component, fields_map);
            Ok(PatchKind::Component(patch))
        }
        "remove" => {
            let patch = ComponentPatch::remove(entity_ref, &component);
            Ok(PatchKind::Component(patch))
        }
        _ => Err(format!("Unknown component op: {}", op)),
    }
}

/// Parse layer patch from script object
fn parse_layer_patch(obj: &HashMap<String, Value>) -> Result<PatchKind, String> {
    let op = obj.get("op")
        .and_then(value_as_string)
        .ok_or("Layer patch missing 'op' field")?;

    let name = obj.get("name")
        .and_then(value_as_string)
        .ok_or("Layer patch missing 'name' field")?;

    match op.as_str() {
        "create" => {
            let layer_type_str = obj.get("layer_type")
                .and_then(value_as_string);
            let layer_type = layer_type_str
                .as_ref()
                .map(|s| parse_layer_type(s))
                .unwrap_or(LayerType::Content);

            let priority = obj.get("priority")
                .and_then(|v| v.to_int())
                .unwrap_or(0) as i32;

            let patch = LayerPatch::create(&name, layer_type, priority);
            Ok(PatchKind::Layer(patch))
        }
        "destroy" => {
            let patch = LayerPatch::destroy(&name);
            Ok(PatchKind::Layer(patch))
        }
        // Note: set_visible not implemented in LayerPatch yet
        _ => Err(format!("Unknown layer op: {}", op)),
    }
}

/// Parse entity reference from patch object
fn parse_entity_ref(obj: &HashMap<String, Value>, default_ns: NamespaceId) -> Result<EntityRef, String> {
    if let Some(Value::Object(entity_obj)) = obj.get("entity") {
        let ns = entity_obj.get("namespace")
            .and_then(|v| v.to_int())
            .map(|n| NamespaceId::from_raw(n as u64))
            .unwrap_or(default_ns);

        let local_id = entity_obj.get("local_id")
            .and_then(|v| v.to_int())
            .ok_or("Entity missing 'local_id'")?;

        Ok(EntityRef::new(ns, local_id as u64))
    } else {
        Err("Patch missing 'entity' field".to_string())
    }
}

/// Parse layer type from string
fn parse_layer_type(s: &str) -> LayerType {
    match s.to_lowercase().as_str() {
        "content" => LayerType::Content,
        "effect" => LayerType::Effect,
        "overlay" => LayerType::Overlay,
        "portal" => LayerType::Portal,
        _ => LayerType::Content,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_layer_type() {
        assert!(matches!(parse_layer_type("content"), LayerType::Content));
        assert!(matches!(parse_layer_type("effect"), LayerType::Effect));
        assert!(matches!(parse_layer_type("overlay"), LayerType::Overlay));
        assert!(matches!(parse_layer_type("portal"), LayerType::Portal));
        assert!(matches!(parse_layer_type("unknown"), LayerType::Content));
    }
}
