//! Event built-in functions for VoidScript
//!
//! Provides functions for event handling:
//! - on: Register event listener
//! - emit: Emit event
//! - off: Remove event listener
//! - once: Register one-time listener
//! - listeners: Get event listeners
//! - has_listeners: Check if event has listeners
//!
//! NOTE: These are stub implementations that track event registrations
//! but don't actually dispatch events. In a real implementation, these
//! would interface with the kernel's event bus.

use crate::interpreter::Interpreter;
use crate::value::Value;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};

// Global listener ID counter
static LISTENER_COUNTER: AtomicU64 = AtomicU64::new(1);

/// Register event functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    register_on(interpreter);
    register_once(interpreter);
    register_off(interpreter);
    register_emit(interpreter);
    register_listeners(interpreter);
    register_has_listeners(interpreter);
    register_clear_listeners(interpreter);
    register_event_names(interpreter);
}

/// Helper to create a listener handle
fn make_listener_handle(id: u64, event_name: &str) -> Value {
    let mut obj = HashMap::new();
    obj.insert("id".to_string(), Value::Int(id as i64));
    obj.insert("event".to_string(), Value::String(event_name.to_string()));
    obj.insert("type".to_string(), Value::String("listener".to_string()));
    Value::Object(obj)
}

/// on(event_name, callback) - Register event listener
/// Returns a listener handle that can be used with off()
fn register_on(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("on", 2, |args| {
        let event_name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("on() expects a string event name".to_string()),
        };

        match &args[1] {
            Value::Function(_) | Value::Native(_) => {}
            _ => return Err("on() expects a function callback".to_string()),
        };

        // In a real implementation, this would:
        // 1. Register the callback with the event bus
        // 2. Store the callback for later invocation
        // For now, we just create a listener handle

        let id = LISTENER_COUNTER.fetch_add(1, Ordering::SeqCst);
        Ok(make_listener_handle(id, &event_name))
    });
}

/// once(event_name, callback) - Register one-time event listener
fn register_once(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("once", 2, |args| {
        let event_name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("once() expects a string event name".to_string()),
        };

        match &args[1] {
            Value::Function(_) | Value::Native(_) => {}
            _ => return Err("once() expects a function callback".to_string()),
        };

        let id = LISTENER_COUNTER.fetch_add(1, Ordering::SeqCst);
        let mut handle = HashMap::new();
        handle.insert("id".to_string(), Value::Int(id as i64));
        handle.insert("event".to_string(), Value::String(event_name));
        handle.insert("type".to_string(), Value::String("listener".to_string()));
        handle.insert("once".to_string(), Value::Bool(true));
        Ok(Value::Object(handle))
    });
}

/// off(handle) or off(event_name, callback?) - Remove event listener
fn register_off(interpreter: &mut Interpreter) {
    interpreter.register_native("off", |args| {
        if args.is_empty() {
            return Err("off() requires at least one argument".to_string());
        }

        match &args[0] {
            // off(handle) - Remove by handle
            Value::Object(obj) => {
                if obj.get("type") == Some(&Value::String("listener".to_string())) {
                    Ok(Value::Bool(true))
                } else {
                    Err("off() expects a listener handle".to_string())
                }
            }
            // off(event_name) - Remove all listeners for event
            // off(event_name, callback) - Remove specific callback
            Value::String(_event_name) => {
                // In a real implementation, this would remove listeners
                Ok(Value::Bool(true))
            }
            _ => Err("off() expects a listener handle or event name".to_string()),
        }
    });
}

/// emit(event_name, data?) - Emit an event
fn register_emit(interpreter: &mut Interpreter) {
    interpreter.register_native("emit", |args| {
        if args.is_empty() {
            return Err("emit() requires at least an event name".to_string());
        }

        let event_name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("emit() expects a string event name".to_string()),
        };

        let event_data = if args.len() > 1 {
            args[1].clone()
        } else {
            Value::Null
        };

        // In a real implementation, this would:
        // 1. Find all registered listeners for this event
        // 2. Create an event object with the data
        // 3. Invoke each listener with the event object
        // 4. Handle any errors from listeners
        // 5. Remove one-time listeners after invocation

        // Create event object that would be passed to listeners
        let mut event = HashMap::new();
        event.insert("name".to_string(), Value::String(event_name));
        event.insert("data".to_string(), event_data);
        event.insert(
            "timestamp".to_string(),
            Value::Int(
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap_or_default()
                    .as_millis() as i64,
            ),
        );

        Ok(Value::Object(event))
    });
}

/// listeners(event_name) - Get all listeners for an event
fn register_listeners(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("listeners", 1, |args| {
        match &args[0] {
            Value::String(_) => {
                // In a real implementation, this would return registered listeners
                Ok(Value::Array(Vec::new()))
            }
            _ => Err("listeners() expects a string event name".to_string()),
        }
    });
}

/// has_listeners(event_name) - Check if event has any listeners
fn register_has_listeners(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("has_listeners", 1, |args| {
        match &args[0] {
            Value::String(_) => {
                // In a real implementation, this would check registered listeners
                Ok(Value::Bool(false))
            }
            _ => Err("has_listeners() expects a string event name".to_string()),
        }
    });
}

/// clear_listeners(event_name?) - Remove all listeners, optionally for specific event
fn register_clear_listeners(interpreter: &mut Interpreter) {
    interpreter.register_native("clear_listeners", |args| {
        if !args.is_empty() {
            match &args[0] {
                Value::String(_) => {}
                _ => return Err("clear_listeners() expects a string event name".to_string()),
            };
        }
        // In a real implementation, this would clear registered listeners
        Ok(Value::Bool(true))
    });
}

/// event_names() - Get all event names with registered listeners
fn register_event_names(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("event_names", 0, |_| {
        // In a real implementation, this would return event names with listeners
        Ok(Value::Array(Vec::new()))
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
    fn test_on() {
        let result = run(r#"
            fn handler(event) {
                print(event);
            }
            on("player_damage", handler);
        "#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("id"));
            assert_eq!(obj.get("event"), Some(&Value::String("player_damage".to_string())));
            assert_eq!(obj.get("type"), Some(&Value::String("listener".to_string())));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_once() {
        let result = run(r#"
            fn handler(event) {
                print(event);
            }
            once("game_start", handler);
        "#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("once"), Some(&Value::Bool(true)));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_off_with_handle() {
        let result = run(r#"
            fn handler(event) {}
            let h = on("test", handler);
            off(h);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_off_with_event_name() {
        let result = run(r#"off("test");"#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_emit() {
        let result = run(r#"emit("player_damage", {damage: 10, source: "enemy"});"#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("name"), Some(&Value::String("player_damage".to_string())));
            assert!(obj.contains_key("data"));
            assert!(obj.contains_key("timestamp"));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_emit_no_data() {
        let result = run(r#"emit("game_start");"#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("data"), Some(&Value::Null));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_listeners() {
        let result = run(r#"listeners("test");"#);
        assert!(matches!(result, Value::Array(_)));
    }

    #[test]
    fn test_has_listeners() {
        let result = run(r#"has_listeners("test");"#);
        assert_eq!(result, Value::Bool(false));
    }

    #[test]
    fn test_clear_listeners() {
        let result = run(r#"clear_listeners("test");"#);
        assert_eq!(result, Value::Bool(true));

        let result = run("clear_listeners();");
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_event_names() {
        let result = run("event_names();");
        assert!(matches!(result, Value::Array(_)));
    }

    #[test]
    fn test_lambda_handler() {
        // Test using lambda as event handler
        let result = run(r#"
            let h = on("test", |event| {
                return event.data;
            });
            h;
        "#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("event"), Some(&Value::String("test".to_string())));
        } else {
            panic!("Expected object");
        }
    }
}
