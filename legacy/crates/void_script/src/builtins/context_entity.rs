//! Context-aware entity built-in functions
//!
//! These functions use ScriptContext to emit real IR patches when connected
//! to the kernel. They override the stub implementations in entity.rs.

use crate::context::{ScriptContext, script_to_ir_value};
use crate::interpreter::Interpreter;
use crate::value::Value;
use std::collections::HashMap;

/// Register context-aware entity functions
pub fn register(interpreter: &mut Interpreter, context: ScriptContext) {
    register_spawn(interpreter, context.clone());
    register_destroy(interpreter, context.clone());
    register_set_component(interpreter, context.clone());
    register_set_position(interpreter, context.clone());
    register_set_rotation(interpreter, context.clone());
    register_set_scale(interpreter, context);
}

/// spawn(name) or spawn(name, components) - Create a new entity via IR patch
fn register_spawn(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native("spawn", move |args| {
        if args.is_empty() {
            return Err("spawn() requires at least a name argument".to_string());
        }

        let name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("spawn() expects a string name".to_string()),
        };

        // Extract components if provided
        let components = if args.len() > 1 {
            match &args[1] {
                Value::Object(obj) => {
                    obj.iter()
                        .map(|(k, v)| (k.clone(), script_to_ir_value(v)))
                        .collect()
                }
                _ => HashMap::new(),
            }
        } else {
            HashMap::new()
        };

        // Use context to spawn (emits real patch if live)
        let entity_id = context.spawn_entity(&name, None, components)?;

        // Return entity object with ID
        let mut entity = HashMap::new();
        entity.insert("id".to_string(), Value::Int(entity_id as i64));
        entity.insert("name".to_string(), Value::String(name));
        entity.insert("type".to_string(), Value::String("entity".to_string()));

        Ok(Value::Object(entity))
    });
}

/// destroy(entity) - Destroy an entity via IR patch
fn register_destroy(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native_with_arity("destroy", 1, move |args| {
        let entity_id = extract_entity_id(&args[0])?;
        context.destroy_entity(entity_id)?;
        Ok(Value::Bool(true))
    });
}

/// set_component(entity, component_name, data) - Set component via IR patch
fn register_set_component(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native_with_arity("set_component", 3, move |args| {
        let entity_id = extract_entity_id(&args[0])?;
        let component_name = match &args[1] {
            Value::String(s) => s.clone(),
            _ => return Err("set_component() expects a string component name".to_string()),
        };
        let value = script_to_ir_value(&args[2]);

        context.set_component(entity_id, &component_name, value)?;
        Ok(Value::Bool(true))
    });
}

/// set_position(entity, x, y, z) or set_position(entity, vec3) - Set position via IR patch
fn register_set_position(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native("set_position", move |args| {
        if args.len() < 2 {
            return Err("set_position() requires entity and position".to_string());
        }

        let entity_id = extract_entity_id(&args[0])?;
        let (x, y, z) = extract_vec3(&args[1..])?;

        context.set_position(entity_id, x, y, z)?;
        Ok(Value::Bool(true))
    });
}

/// set_rotation(entity, x, y, z) or set_rotation(entity, vec3) - Set rotation via IR patch
fn register_set_rotation(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native("set_rotation", move |args| {
        if args.len() < 2 {
            return Err("set_rotation() requires entity and rotation".to_string());
        }

        let entity_id = extract_entity_id(&args[0])?;
        let (x, y, z) = extract_vec3(&args[1..])?;

        context.set_rotation(entity_id, x, y, z)?;
        Ok(Value::Bool(true))
    });
}

/// set_scale(entity, x, y, z) or set_scale(entity, vec3) or set_scale(entity, uniform)
fn register_set_scale(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native("set_scale", move |args| {
        if args.len() < 2 {
            return Err("set_scale() requires entity and scale".to_string());
        }

        let entity_id = extract_entity_id(&args[0])?;

        // Handle uniform scale
        if args.len() == 2 {
            if let Some(uniform) = args[1].to_float() {
                context.set_scale(entity_id, uniform, uniform, uniform)?;
                return Ok(Value::Bool(true));
            }
        }

        let (x, y, z) = extract_vec3(&args[1..])?;
        context.set_scale(entity_id, x, y, z)?;
        Ok(Value::Bool(true))
    });
}

/// Extract entity ID from a Value (either object with id field or raw int)
fn extract_entity_id(value: &Value) -> Result<u64, String> {
    match value {
        Value::Object(obj) => {
            if let Some(Value::Int(id)) = obj.get("id") {
                Ok(*id as u64)
            } else {
                Err("Entity object missing 'id' field".to_string())
            }
        }
        Value::Int(id) => Ok(*id as u64),
        _ => Err("Expected entity or entity ID".to_string()),
    }
}

/// Extract vec3 from args (either 3 numbers or a vec3 object/array)
fn extract_vec3(args: &[Value]) -> Result<(f64, f64, f64), String> {
    if args.len() >= 3 {
        // (x, y, z) format
        let x = args[0].to_float().ok_or("Expected number for x")?;
        let y = args[1].to_float().ok_or("Expected number for y")?;
        let z = args[2].to_float().ok_or("Expected number for z")?;
        Ok((x, y, z))
    } else if args.len() == 1 {
        // vec3 object or array
        match &args[0] {
            Value::Object(obj) => {
                let x = obj.get("x").and_then(|v| v.to_float()).ok_or("Missing x")?;
                let y = obj.get("y").and_then(|v| v.to_float()).ok_or("Missing y")?;
                let z = obj.get("z").and_then(|v| v.to_float()).ok_or("Missing z")?;
                Ok((x, y, z))
            }
            Value::Array(arr) if arr.len() >= 3 => {
                let x = arr[0].to_float().ok_or("Expected number for x")?;
                let y = arr[1].to_float().ok_or("Expected number for y")?;
                let z = arr[2].to_float().ok_or("Expected number for z")?;
                Ok((x, y, z))
            }
            _ => Err("Expected vec3 object, array, or x,y,z coordinates".to_string()),
        }
    } else {
        Err("Expected vec3 or x,y,z coordinates".to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_extract_entity_id_from_object() {
        let mut obj = HashMap::new();
        obj.insert("id".to_string(), Value::Int(42));
        obj.insert("name".to_string(), Value::String("test".to_string()));

        let id = extract_entity_id(&Value::Object(obj)).unwrap();
        assert_eq!(id, 42);
    }

    #[test]
    fn test_extract_entity_id_from_int() {
        let id = extract_entity_id(&Value::Int(123)).unwrap();
        assert_eq!(id, 123);
    }

    #[test]
    fn test_extract_vec3_from_args() {
        let args = vec![Value::Float(1.0), Value::Float(2.0), Value::Float(3.0)];
        let (x, y, z) = extract_vec3(&args).unwrap();
        assert_eq!((x, y, z), (1.0, 2.0, 3.0));
    }

    #[test]
    fn test_extract_vec3_from_object() {
        let mut obj = HashMap::new();
        obj.insert("x".to_string(), Value::Float(1.0));
        obj.insert("y".to_string(), Value::Float(2.0));
        obj.insert("z".to_string(), Value::Float(3.0));

        let args = vec![Value::Object(obj)];
        let (x, y, z) = extract_vec3(&args).unwrap();
        assert_eq!((x, y, z), (1.0, 2.0, 3.0));
    }

    #[test]
    fn test_extract_vec3_from_array() {
        let arr = vec![Value::Float(1.0), Value::Float(2.0), Value::Float(3.0)];
        let args = vec![Value::Array(arr)];
        let (x, y, z) = extract_vec3(&args).unwrap();
        assert_eq!((x, y, z), (1.0, 2.0, 3.0));
    }
}
