//! Entity built-in functions for VoidScript
//!
//! Provides functions for entity manipulation in the metaverse:
//! - spawn, destroy: Entity lifecycle
//! - get_component, set_component: Component access
//! - get_position, set_position: Position manipulation
//! - get_rotation, set_rotation: Rotation manipulation
//! - get_scale, set_scale: Scale manipulation
//! - has_component, remove_component: Component management
//!
//! NOTE: These are stub implementations that return mock data.
//! In a real implementation, these would interface with the kernel's
//! patch bus to submit IR patches for entity manipulation.

use crate::interpreter::Interpreter;
use crate::value::Value;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};

// Global entity ID counter for mock entities
static ENTITY_COUNTER: AtomicU64 = AtomicU64::new(1);

/// Register entity functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    register_spawn(interpreter);
    register_destroy(interpreter);
    register_get_component(interpreter);
    register_set_component(interpreter);
    register_has_component(interpreter);
    register_remove_component(interpreter);
    register_get_position(interpreter);
    register_set_position(interpreter);
    register_get_rotation(interpreter);
    register_set_rotation(interpreter);
    register_get_scale(interpreter);
    register_set_scale(interpreter);
    register_get_entity(interpreter);
    register_find_entities(interpreter);
    register_get_children(interpreter);
    register_get_parent(interpreter);
    register_set_parent(interpreter);
    register_clone_entity(interpreter);
    register_entity_exists(interpreter);
}

/// Helper to create an entity object
fn make_entity(id: u64, name: &str) -> Value {
    let mut obj = HashMap::new();
    obj.insert("id".to_string(), Value::Int(id as i64));
    obj.insert("name".to_string(), Value::String(name.to_string()));
    obj.insert("type".to_string(), Value::String("entity".to_string()));
    Value::Object(obj)
}

/// Helper to create a vec3 object
fn make_vec3(x: f64, y: f64, z: f64) -> Value {
    let mut obj = HashMap::new();
    obj.insert("x".to_string(), Value::Float(x));
    obj.insert("y".to_string(), Value::Float(y));
    obj.insert("z".to_string(), Value::Float(z));
    Value::Object(obj)
}

/// spawn(name) or spawn(name, components) - Create a new entity
fn register_spawn(interpreter: &mut Interpreter) {
    interpreter.register_native("spawn", |args| {
        if args.is_empty() {
            return Err("spawn() requires at least a name argument".to_string());
        }

        let name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("spawn() expects a string name".to_string()),
        };

        // Generate a new entity ID
        let id = ENTITY_COUNTER.fetch_add(1, Ordering::SeqCst);

        // In a real implementation, this would:
        // 1. Check capability "entity.spawn"
        // 2. Create an IR patch for entity creation
        // 3. Submit to patch bus
        // 4. Return entity reference

        // For now, return a mock entity object
        let mut entity = HashMap::new();
        entity.insert("id".to_string(), Value::Int(id as i64));
        entity.insert("name".to_string(), Value::String(name));
        entity.insert("type".to_string(), Value::String("entity".to_string()));

        // If components were provided, add them
        if args.len() > 1 {
            if let Value::Object(components) = &args[1] {
                entity.insert("components".to_string(), Value::Object(components.clone()));
            }
        }

        Ok(Value::Object(entity))
    });
}

/// destroy(entity) - Destroy an entity
fn register_destroy(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("destroy", 1, |args| {
        match &args[0] {
            Value::Object(obj) => {
                if obj.get("type") == Some(&Value::String("entity".to_string())) {
                    // In a real implementation, this would submit a destroy patch
                    Ok(Value::Bool(true))
                } else {
                    Err("destroy() expects an entity".to_string())
                }
            }
            Value::Int(_) => {
                // Entity ID passed directly
                Ok(Value::Bool(true))
            }
            _ => Err("destroy() expects an entity or entity ID".to_string()),
        }
    });
}

/// get_component(entity, component_name) - Get a component from an entity
fn register_get_component(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_component", 2, |args| {
        let entity = match &args[0] {
            Value::Object(obj) => obj.clone(),
            Value::Int(id) => {
                let mut obj = HashMap::new();
                obj.insert("id".to_string(), Value::Int(*id));
                obj
            }
            _ => return Err("get_component() expects an entity".to_string()),
        };

        let component_name = match &args[1] {
            Value::String(s) => s.clone(),
            _ => return Err("get_component() expects a string component name".to_string()),
        };

        // Check if entity has components stored
        if let Some(Value::Object(components)) = entity.get("components") {
            if let Some(component) = components.get(&component_name) {
                return Ok(component.clone());
            }
        }

        // Return mock component data based on common component types
        match component_name.as_str() {
            "Position" | "Transform" => Ok(make_vec3(0.0, 0.0, 0.0)),
            "Rotation" => Ok(make_vec3(0.0, 0.0, 0.0)),
            "Scale" => Ok(make_vec3(1.0, 1.0, 1.0)),
            "Health" => {
                let mut health = HashMap::new();
                health.insert("current".to_string(), Value::Int(100));
                health.insert("max".to_string(), Value::Int(100));
                Ok(Value::Object(health))
            }
            "Velocity" => Ok(make_vec3(0.0, 0.0, 0.0)),
            _ => Ok(Value::Null), // Component not found
        }
    });
}

/// set_component(entity, component_name, data) - Set a component on an entity
fn register_set_component(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set_component", 3, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_component() expects an entity".to_string()),
        };

        match &args[1] {
            Value::String(_) => {}
            _ => return Err("set_component() expects a string component name".to_string()),
        };

        // In a real implementation, this would:
        // 1. Check capability "entity.modify"
        // 2. Create an IR patch for component modification
        // 3. Submit to patch bus

        Ok(Value::Bool(true))
    });
}

/// has_component(entity, component_name) - Check if entity has a component
fn register_has_component(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("has_component", 2, |args| {
        let entity = match &args[0] {
            Value::Object(obj) => obj.clone(),
            Value::Int(id) => {
                let mut obj = HashMap::new();
                obj.insert("id".to_string(), Value::Int(*id));
                obj
            }
            _ => return Err("has_component() expects an entity".to_string()),
        };

        let component_name = match &args[1] {
            Value::String(s) => s.clone(),
            _ => return Err("has_component() expects a string component name".to_string()),
        };

        // Check if entity has the component
        if let Some(Value::Object(components)) = entity.get("components") {
            return Ok(Value::Bool(components.contains_key(&component_name)));
        }

        // Return mock result for common components
        let has = matches!(
            component_name.as_str(),
            "Position" | "Transform" | "Rotation" | "Scale"
        );
        Ok(Value::Bool(has))
    });
}

/// remove_component(entity, component_name) - Remove a component from an entity
fn register_remove_component(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("remove_component", 2, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("remove_component() expects an entity".to_string()),
        };

        match &args[1] {
            Value::String(_) => {}
            _ => return Err("remove_component() expects a string component name".to_string()),
        };

        Ok(Value::Bool(true))
    });
}

/// get_position(entity) - Get entity position
fn register_get_position(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_position", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("get_position() expects an entity".to_string()),
        };

        // Return mock position
        Ok(make_vec3(0.0, 0.0, 0.0))
    });
}

/// set_position(entity, x, y, z) or set_position(entity, vec3) - Set entity position
fn register_set_position(interpreter: &mut Interpreter) {
    interpreter.register_native("set_position", |args| {
        if args.len() < 2 {
            return Err("set_position() requires entity and position".to_string());
        }

        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_position() expects an entity".to_string()),
        };

        // Accept either (entity, x, y, z) or (entity, vec3)
        if args.len() == 4 {
            // (entity, x, y, z)
            for i in 1..4 {
                if !args[i].is_number() {
                    return Err("set_position() expects numeric coordinates".to_string());
                }
            }
        } else if args.len() == 2 {
            // (entity, vec3)
            match &args[1] {
                Value::Object(_) | Value::Array(_) => {}
                _ => return Err("set_position() expects a vec3 or coordinates".to_string()),
            }
        } else {
            return Err("set_position() expects (entity, x, y, z) or (entity, vec3)".to_string());
        }

        Ok(Value::Bool(true))
    });
}

/// get_rotation(entity) - Get entity rotation (Euler angles in degrees)
fn register_get_rotation(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_rotation", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("get_rotation() expects an entity".to_string()),
        };

        Ok(make_vec3(0.0, 0.0, 0.0))
    });
}

/// set_rotation(entity, x, y, z) or set_rotation(entity, vec3) - Set entity rotation
fn register_set_rotation(interpreter: &mut Interpreter) {
    interpreter.register_native("set_rotation", |args| {
        if args.len() < 2 {
            return Err("set_rotation() requires entity and rotation".to_string());
        }

        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_rotation() expects an entity".to_string()),
        };

        if args.len() == 4 {
            for i in 1..4 {
                if !args[i].is_number() {
                    return Err("set_rotation() expects numeric angles".to_string());
                }
            }
        } else if args.len() == 2 {
            match &args[1] {
                Value::Object(_) | Value::Array(_) => {}
                _ => return Err("set_rotation() expects a vec3 or angles".to_string()),
            }
        } else {
            return Err("set_rotation() expects (entity, x, y, z) or (entity, vec3)".to_string());
        }

        Ok(Value::Bool(true))
    });
}

/// get_scale(entity) - Get entity scale
fn register_get_scale(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_scale", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("get_scale() expects an entity".to_string()),
        };

        Ok(make_vec3(1.0, 1.0, 1.0))
    });
}

/// set_scale(entity, x, y, z) or set_scale(entity, vec3) or set_scale(entity, uniform)
fn register_set_scale(interpreter: &mut Interpreter) {
    interpreter.register_native("set_scale", |args| {
        if args.len() < 2 {
            return Err("set_scale() requires entity and scale".to_string());
        }

        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_scale() expects an entity".to_string()),
        };

        if args.len() == 4 {
            for i in 1..4 {
                if !args[i].is_number() {
                    return Err("set_scale() expects numeric scale values".to_string());
                }
            }
        } else if args.len() == 2 {
            match &args[1] {
                Value::Object(_) | Value::Array(_) | Value::Int(_) | Value::Float(_) => {}
                _ => return Err("set_scale() expects a vec3 or number".to_string()),
            }
        } else {
            return Err("set_scale() expects (entity, x, y, z), (entity, vec3), or (entity, uniform)".to_string());
        }

        Ok(Value::Bool(true))
    });
}

/// get_entity(name) - Find entity by name
fn register_get_entity(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_entity", 1, |args| {
        let name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("get_entity() expects a string name".to_string()),
        };

        // Return mock entity
        Ok(make_entity(1, &name))
    });
}

/// find_entities(query) - Find entities matching query
fn register_find_entities(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("find_entities", 1, |args| {
        match &args[0] {
            Value::String(_) | Value::Object(_) => {}
            _ => return Err("find_entities() expects a string or query object".to_string()),
        };

        // Return empty array (mock)
        Ok(Value::Array(Vec::new()))
    });
}

/// get_children(entity) - Get child entities
fn register_get_children(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_children", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("get_children() expects an entity".to_string()),
        };

        Ok(Value::Array(Vec::new()))
    });
}

/// get_parent(entity) - Get parent entity
fn register_get_parent(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_parent", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("get_parent() expects an entity".to_string()),
        };

        Ok(Value::Null)
    });
}

/// set_parent(entity, parent) - Set parent entity (null to unparent)
fn register_set_parent(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set_parent", 2, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_parent() expects an entity".to_string()),
        };

        match &args[1] {
            Value::Object(_) | Value::Int(_) | Value::Null => {}
            _ => return Err("set_parent() expects an entity or null".to_string()),
        };

        Ok(Value::Bool(true))
    });
}

/// clone_entity(entity) - Clone an entity
fn register_clone_entity(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("clone_entity", 1, |args| {
        let (id, name) = match &args[0] {
            Value::Object(obj) => {
                let id = obj.get("id").and_then(|v| v.to_int()).unwrap_or(0);
                let name = match obj.get("name") {
                    Some(Value::String(s)) => format!("{}_clone", s),
                    _ => "clone".to_string(),
                };
                (id, name)
            }
            Value::Int(id) => (*id, format!("entity_{}_clone", id)),
            _ => return Err("clone_entity() expects an entity".to_string()),
        };

        let new_id = ENTITY_COUNTER.fetch_add(1, Ordering::SeqCst);
        let _ = id; // Original ID not used in mock

        Ok(make_entity(new_id, &name))
    });
}

/// entity_exists(entity) - Check if entity exists
fn register_entity_exists(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("entity_exists", 1, |args| {
        match &args[0] {
            Value::Object(obj) => {
                Ok(Value::Bool(obj.get("type") == Some(&Value::String("entity".to_string()))))
            }
            Value::Int(_) => {
                // Assume entity exists (mock)
                Ok(Value::Bool(true))
            }
            Value::Null => Ok(Value::Bool(false)),
            _ => Err("entity_exists() expects an entity".to_string()),
        }
    });
}

#[cfg(test)]
mod tests {
    use crate::VoidScript;
    use crate::value::Value;

    fn run(code: &str) -> Value {
        let mut vs = VoidScript::new();
        vs.execute(code).unwrap()
    }

    #[test]
    fn test_spawn() {
        let result = run(r#"spawn("player");"#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("id"));
            assert_eq!(obj.get("name"), Some(&Value::String("player".to_string())));
            assert_eq!(obj.get("type"), Some(&Value::String("entity".to_string())));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_spawn_with_components() {
        let result = run(r#"spawn("player", {Health: {current: 100, max: 100}});"#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("components"));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_destroy() {
        let result = run(r#"
            let e = spawn("temp");
            destroy(e);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_get_component() {
        let result = run(r#"
            let e = spawn("player");
            get_component(e, "Position");
        "#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("x"));
            assert!(obj.contains_key("y"));
            assert!(obj.contains_key("z"));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_set_component() {
        let result = run(r#"
            let e = spawn("player");
            set_component(e, "Health", {current: 50, max: 100});
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_has_component() {
        let result = run(r#"
            let e = spawn("player");
            has_component(e, "Position");
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_position() {
        let result = run(r#"
            let e = spawn("player");
            get_position(e);
        "#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("x"));
        } else {
            panic!("Expected object");
        }

        let result = run(r#"
            let e = spawn("player");
            set_position(e, 10, 20, 30);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_rotation() {
        let result = run(r#"
            let e = spawn("player");
            get_rotation(e);
        "#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("x"));
        } else {
            panic!("Expected object");
        }

        let result = run(r#"
            let e = spawn("player");
            set_rotation(e, 0, 90, 0);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_scale() {
        let result = run(r#"
            let e = spawn("player");
            get_scale(e);
        "#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("x"), Some(&Value::Float(1.0)));
        } else {
            panic!("Expected object");
        }

        let result = run(r#"
            let e = spawn("player");
            set_scale(e, 2, 2, 2);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_get_entity() {
        let result = run(r#"get_entity("player");"#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("name"), Some(&Value::String("player".to_string())));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_find_entities() {
        let result = run(r#"find_entities("player");"#);
        assert!(matches!(result, Value::Array(_)));
    }

    #[test]
    fn test_clone_entity() {
        let result = run(r#"
            let e = spawn("player");
            clone_entity(e);
        "#);
        if let Value::Object(obj) = result {
            assert!(obj.get("name").unwrap().to_string_value().contains("clone"));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_entity_exists() {
        let result = run(r#"
            let e = spawn("player");
            entity_exists(e);
        "#);
        assert_eq!(result, Value::Bool(true));

        assert_eq!(run("entity_exists(null);"), Value::Bool(false));
    }

    #[test]
    fn test_hierarchy() {
        let result = run(r#"
            let e = spawn("player");
            get_children(e);
        "#);
        assert!(matches!(result, Value::Array(_)));

        let result = run(r#"
            let e = spawn("player");
            get_parent(e);
        "#);
        assert_eq!(result, Value::Null);

        let result = run(r#"
            let parent = spawn("parent");
            let child = spawn("child");
            set_parent(child, parent);
        "#);
        assert_eq!(result, Value::Bool(true));
    }
}
