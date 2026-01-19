//! Layer built-in functions for VoidScript
//!
//! Provides functions for layer management in the compositor:
//! - create_layer, destroy_layer: Layer lifecycle
//! - set_layer_visible, get_layer_visible: Visibility control
//! - set_layer_opacity, get_layer_opacity: Opacity control
//! - set_layer_order, get_layer_order: Z-order management
//! - set_layer_transform: Transform manipulation
//! - get_layers: List all layers
//!
//! NOTE: These are stub implementations that return mock data.
//! In a real implementation, these would interface with the
//! compositor through the kernel's patch bus.

use crate::interpreter::Interpreter;
use crate::value::Value;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};

// Global layer ID counter for mock layers
static LAYER_COUNTER: AtomicU64 = AtomicU64::new(1);

/// Register layer functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    register_create_layer(interpreter);
    register_destroy_layer(interpreter);
    register_set_layer_visible(interpreter);
    register_get_layer_visible(interpreter);
    register_set_layer_opacity(interpreter);
    register_get_layer_opacity(interpreter);
    register_set_layer_order(interpreter);
    register_get_layer_order(interpreter);
    register_set_layer_transform(interpreter);
    register_get_layer_transform(interpreter);
    register_set_layer_size(interpreter);
    register_get_layer_size(interpreter);
    register_set_layer_position(interpreter);
    register_get_layer_position(interpreter);
    register_get_layers(interpreter);
    register_get_layer(interpreter);
    register_set_layer_blend_mode(interpreter);
    register_get_layer_blend_mode(interpreter);
    register_bring_to_front(interpreter);
    register_send_to_back(interpreter);
}

/// Helper to create a layer object
fn make_layer(id: u64, name: &str, layer_type: &str) -> Value {
    let mut obj = HashMap::new();
    obj.insert("id".to_string(), Value::Int(id as i64));
    obj.insert("name".to_string(), Value::String(name.to_string()));
    obj.insert("layer_type".to_string(), Value::String(layer_type.to_string()));
    obj.insert("type".to_string(), Value::String("layer".to_string()));
    obj.insert("visible".to_string(), Value::Bool(true));
    obj.insert("opacity".to_string(), Value::Float(1.0));
    obj.insert("order".to_string(), Value::Int(id as i64)); // Default order = id
    Value::Object(obj)
}

/// create_layer(name, type) - Create a new layer
/// Types: "background", "content", "overlay", "ui", "debug"
fn register_create_layer(interpreter: &mut Interpreter) {
    interpreter.register_native("create_layer", |args| {
        if args.is_empty() {
            return Err("create_layer() requires at least a name argument".to_string());
        }

        let name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("create_layer() expects a string name".to_string()),
        };

        let layer_type = if args.len() > 1 {
            match &args[1] {
                Value::String(s) => s.clone(),
                _ => return Err("create_layer() expects a string type".to_string()),
            }
        } else {
            "content".to_string()
        };

        // Validate layer type
        let valid_types = ["background", "content", "overlay", "ui", "debug"];
        if !valid_types.contains(&layer_type.as_str()) {
            return Err(format!(
                "Invalid layer type '{}'. Valid types: {:?}",
                layer_type, valid_types
            ));
        }

        let id = LAYER_COUNTER.fetch_add(1, Ordering::SeqCst);

        // In a real implementation, this would submit a patch to create the layer
        Ok(make_layer(id, &name, &layer_type))
    });
}

/// destroy_layer(layer) - Destroy a layer
fn register_destroy_layer(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("destroy_layer", 1, |args| {
        match &args[0] {
            Value::Object(obj) => {
                if obj.get("type") == Some(&Value::String("layer".to_string())) {
                    Ok(Value::Bool(true))
                } else {
                    Err("destroy_layer() expects a layer".to_string())
                }
            }
            Value::Int(_) => Ok(Value::Bool(true)), // Layer ID
            _ => Err("destroy_layer() expects a layer".to_string()),
        }
    });
}

/// set_layer_visible(layer, visible) - Set layer visibility
fn register_set_layer_visible(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set_layer_visible", 2, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_layer_visible() expects a layer".to_string()),
        };

        match &args[1] {
            Value::Bool(_) => Ok(Value::Bool(true)),
            _ => Err("set_layer_visible() expects a boolean".to_string()),
        }
    });
}

/// get_layer_visible(layer) - Get layer visibility
fn register_get_layer_visible(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_layer_visible", 1, |args| {
        match &args[0] {
            Value::Object(obj) => {
                Ok(obj.get("visible").cloned().unwrap_or(Value::Bool(true)))
            }
            Value::Int(_) => Ok(Value::Bool(true)), // Mock: always visible
            _ => Err("get_layer_visible() expects a layer".to_string()),
        }
    });
}

/// set_layer_opacity(layer, opacity) - Set layer opacity (0.0 to 1.0)
fn register_set_layer_opacity(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set_layer_opacity", 2, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_layer_opacity() expects a layer".to_string()),
        };

        match args[1].to_float() {
            Some(opacity) => {
                if opacity < 0.0 || opacity > 1.0 {
                    Err("set_layer_opacity() expects opacity between 0.0 and 1.0".to_string())
                } else {
                    Ok(Value::Bool(true))
                }
            }
            None => Err("set_layer_opacity() expects a number".to_string()),
        }
    });
}

/// get_layer_opacity(layer) - Get layer opacity
fn register_get_layer_opacity(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_layer_opacity", 1, |args| {
        match &args[0] {
            Value::Object(obj) => {
                Ok(obj.get("opacity").cloned().unwrap_or(Value::Float(1.0)))
            }
            Value::Int(_) => Ok(Value::Float(1.0)),
            _ => Err("get_layer_opacity() expects a layer".to_string()),
        }
    });
}

/// set_layer_order(layer, order) - Set layer z-order
fn register_set_layer_order(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set_layer_order", 2, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_layer_order() expects a layer".to_string()),
        };

        match &args[1] {
            Value::Int(_) => Ok(Value::Bool(true)),
            _ => Err("set_layer_order() expects an integer".to_string()),
        }
    });
}

/// get_layer_order(layer) - Get layer z-order
fn register_get_layer_order(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_layer_order", 1, |args| {
        match &args[0] {
            Value::Object(obj) => {
                Ok(obj.get("order").cloned().unwrap_or(Value::Int(0)))
            }
            Value::Int(id) => Ok(Value::Int(*id)), // Mock: order = id
            _ => Err("get_layer_order() expects a layer".to_string()),
        }
    });
}

/// set_layer_transform(layer, transform) - Set layer transform matrix
fn register_set_layer_transform(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set_layer_transform", 2, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_layer_transform() expects a layer".to_string()),
        };

        match &args[1] {
            Value::Object(_) | Value::Array(_) => Ok(Value::Bool(true)),
            _ => Err("set_layer_transform() expects a transform object or matrix".to_string()),
        }
    });
}

/// get_layer_transform(layer) - Get layer transform
fn register_get_layer_transform(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_layer_transform", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {
                // Return identity transform
                let mut transform = HashMap::new();
                transform.insert("x".to_string(), Value::Float(0.0));
                transform.insert("y".to_string(), Value::Float(0.0));
                transform.insert("rotation".to_string(), Value::Float(0.0));
                transform.insert("scale_x".to_string(), Value::Float(1.0));
                transform.insert("scale_y".to_string(), Value::Float(1.0));
                Ok(Value::Object(transform))
            }
            _ => Err("get_layer_transform() expects a layer".to_string()),
        }
    });
}

/// set_layer_size(layer, width, height) - Set layer size
fn register_set_layer_size(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set_layer_size", 3, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_layer_size() expects a layer".to_string()),
        };

        match (&args[1], &args[2]) {
            (Value::Int(_) | Value::Float(_), Value::Int(_) | Value::Float(_)) => {
                Ok(Value::Bool(true))
            }
            _ => Err("set_layer_size() expects numeric width and height".to_string()),
        }
    });
}

/// get_layer_size(layer) - Get layer size
fn register_get_layer_size(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_layer_size", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {
                let mut size = HashMap::new();
                size.insert("width".to_string(), Value::Int(1920));
                size.insert("height".to_string(), Value::Int(1080));
                Ok(Value::Object(size))
            }
            _ => Err("get_layer_size() expects a layer".to_string()),
        }
    });
}

/// set_layer_position(layer, x, y) - Set layer position
fn register_set_layer_position(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set_layer_position", 3, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_layer_position() expects a layer".to_string()),
        };

        match (&args[1], &args[2]) {
            (Value::Int(_) | Value::Float(_), Value::Int(_) | Value::Float(_)) => {
                Ok(Value::Bool(true))
            }
            _ => Err("set_layer_position() expects numeric x and y".to_string()),
        }
    });
}

/// get_layer_position(layer) - Get layer position
fn register_get_layer_position(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_layer_position", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {
                let mut pos = HashMap::new();
                pos.insert("x".to_string(), Value::Int(0));
                pos.insert("y".to_string(), Value::Int(0));
                Ok(Value::Object(pos))
            }
            _ => Err("get_layer_position() expects a layer".to_string()),
        }
    });
}

/// get_layers() - Get all layers
fn register_get_layers(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_layers", 0, |_| {
        // Return empty array (mock)
        Ok(Value::Array(Vec::new()))
    });
}

/// get_layer(name) - Get layer by name
fn register_get_layer(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_layer", 1, |args| {
        let name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("get_layer() expects a string name".to_string()),
        };

        // Return mock layer
        Ok(make_layer(1, &name, "content"))
    });
}

/// set_layer_blend_mode(layer, mode) - Set blend mode
/// Modes: "normal", "multiply", "screen", "overlay", "add", "subtract"
fn register_set_layer_blend_mode(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set_layer_blend_mode", 2, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => {}
            _ => return Err("set_layer_blend_mode() expects a layer".to_string()),
        };

        let mode = match &args[1] {
            Value::String(s) => s.clone(),
            _ => return Err("set_layer_blend_mode() expects a string mode".to_string()),
        };

        let valid_modes = ["normal", "multiply", "screen", "overlay", "add", "subtract"];
        if !valid_modes.contains(&mode.as_str()) {
            return Err(format!(
                "Invalid blend mode '{}'. Valid modes: {:?}",
                mode, valid_modes
            ));
        }

        Ok(Value::Bool(true))
    });
}

/// get_layer_blend_mode(layer) - Get blend mode
fn register_get_layer_blend_mode(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("get_layer_blend_mode", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => Ok(Value::String("normal".to_string())),
            _ => Err("get_layer_blend_mode() expects a layer".to_string()),
        }
    });
}

/// bring_to_front(layer) - Bring layer to front
fn register_bring_to_front(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("bring_to_front", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => Ok(Value::Bool(true)),
            _ => Err("bring_to_front() expects a layer".to_string()),
        }
    });
}

/// send_to_back(layer) - Send layer to back
fn register_send_to_back(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("send_to_back", 1, |args| {
        match &args[0] {
            Value::Object(_) | Value::Int(_) => Ok(Value::Bool(true)),
            _ => Err("send_to_back() expects a layer".to_string()),
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
    fn test_create_layer() {
        let result = run(r#"create_layer("ui_layer", "ui");"#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("id"));
            assert_eq!(obj.get("name"), Some(&Value::String("ui_layer".to_string())));
            assert_eq!(obj.get("layer_type"), Some(&Value::String("ui".to_string())));
            assert_eq!(obj.get("type"), Some(&Value::String("layer".to_string())));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_create_layer_default_type() {
        let result = run(r#"create_layer("my_layer");"#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("layer_type"), Some(&Value::String("content".to_string())));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_destroy_layer() {
        let result = run(r#"
            let l = create_layer("temp");
            destroy_layer(l);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_visibility() {
        let result = run(r#"
            let l = create_layer("test");
            get_layer_visible(l);
        "#);
        assert_eq!(result, Value::Bool(true));

        let result = run(r#"
            let l = create_layer("test");
            set_layer_visible(l, false);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_opacity() {
        let result = run(r#"
            let l = create_layer("test");
            get_layer_opacity(l);
        "#);
        assert_eq!(result, Value::Float(1.0));

        let result = run(r#"
            let l = create_layer("test");
            set_layer_opacity(l, 0.5);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_order() {
        let result = run(r#"
            let l = create_layer("test");
            set_layer_order(l, 10);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_transform() {
        let result = run(r#"
            let l = create_layer("test");
            get_layer_transform(l);
        "#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("x"));
            assert!(obj.contains_key("y"));
            assert!(obj.contains_key("rotation"));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_size() {
        let result = run(r#"
            let l = create_layer("test");
            get_layer_size(l);
        "#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("width"));
            assert!(obj.contains_key("height"));
        } else {
            panic!("Expected object");
        }

        let result = run(r#"
            let l = create_layer("test");
            set_layer_size(l, 800, 600);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_position() {
        let result = run(r#"
            let l = create_layer("test");
            get_layer_position(l);
        "#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("x"));
            assert!(obj.contains_key("y"));
        } else {
            panic!("Expected object");
        }

        let result = run(r#"
            let l = create_layer("test");
            set_layer_position(l, 100, 200);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_get_layers() {
        let result = run("get_layers();");
        assert!(matches!(result, Value::Array(_)));
    }

    #[test]
    fn test_get_layer() {
        let result = run(r#"get_layer("test");"#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("name"), Some(&Value::String("test".to_string())));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_blend_mode() {
        let result = run(r#"
            let l = create_layer("test");
            set_layer_blend_mode(l, "multiply");
        "#);
        assert_eq!(result, Value::Bool(true));

        let result = run(r#"
            let l = create_layer("test");
            get_layer_blend_mode(l);
        "#);
        assert_eq!(result, Value::String("normal".to_string()));
    }

    #[test]
    fn test_ordering() {
        let result = run(r#"
            let l = create_layer("test");
            bring_to_front(l);
        "#);
        assert_eq!(result, Value::Bool(true));

        let result = run(r#"
            let l = create_layer("test");
            send_to_back(l);
        "#);
        assert_eq!(result, Value::Bool(true));
    }
}
