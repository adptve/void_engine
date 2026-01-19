//! Context-aware layer built-in functions
//!
//! These functions use ScriptContext to emit real IR patches for layer
//! operations when connected to the kernel.

use crate::context::ScriptContext;
use crate::interpreter::Interpreter;
use crate::value::Value;
use void_ir::patch::LayerType;

/// Register context-aware layer functions
pub fn register(interpreter: &mut Interpreter, context: ScriptContext) {
    register_create_layer(interpreter, context.clone());
    register_request_layer(interpreter, context);
}

/// create_layer(name, type, priority) - Create a layer via IR patch
fn register_create_layer(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native("create_layer", move |args| {
        if args.len() < 2 {
            return Err("create_layer() requires name and type".to_string());
        }

        let name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("create_layer() expects a string name".to_string()),
        };

        let layer_type = match &args[1] {
            Value::String(s) => parse_layer_type(s)?,
            _ => return Err("create_layer() expects a string layer type".to_string()),
        };

        let priority = if args.len() > 2 {
            args[2].to_int().unwrap_or(0) as i32
        } else {
            0
        };

        context.create_layer(&name, layer_type, priority)?;

        Ok(Value::String(name))
    });
}

/// request_layer(config) - Request a layer with full configuration
fn register_request_layer(interpreter: &mut Interpreter, context: ScriptContext) {
    interpreter.register_native_with_arity("request_layer", 1, move |args| {
        let config = match &args[0] {
            Value::Object(obj) => obj.clone(),
            _ => return Err("request_layer() expects a configuration object".to_string()),
        };

        let name = config.get("name")
            .and_then(|v| match v { Value::String(s) => Some(s.clone()), _ => None })
            .ok_or("request_layer() config missing 'name'")?;

        let layer_type_str = config.get("type")
            .and_then(|v| match v { Value::String(s) => Some(s.clone()), _ => None })
            .unwrap_or_else(|| "content".to_string());

        let layer_type = parse_layer_type(&layer_type_str)?;

        let priority = config.get("priority")
            .and_then(|v| v.to_int())
            .unwrap_or(0) as i32;

        context.create_layer(&name, layer_type, priority)?;

        Ok(Value::String(name))
    });
}

/// Parse layer type string
fn parse_layer_type(s: &str) -> Result<LayerType, String> {
    match s.to_lowercase().as_str() {
        "content" => Ok(LayerType::Content),
        "effect" => Ok(LayerType::Effect),
        "overlay" => Ok(LayerType::Overlay),
        "portal" => Ok(LayerType::Portal),
        _ => Err(format!("Unknown layer type: {}", s)),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_layer_type() {
        assert_eq!(parse_layer_type("content").unwrap(), LayerType::Content);
        assert_eq!(parse_layer_type("Content").unwrap(), LayerType::Content);
        assert_eq!(parse_layer_type("CONTENT").unwrap(), LayerType::Content);
        assert_eq!(parse_layer_type("effect").unwrap(), LayerType::Effect);
        assert_eq!(parse_layer_type("overlay").unwrap(), LayerType::Overlay);
        assert_eq!(parse_layer_type("portal").unwrap(), LayerType::Portal);
        assert!(parse_layer_type("invalid").is_err());
    }
}
