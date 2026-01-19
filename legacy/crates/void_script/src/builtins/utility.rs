//! Utility built-in functions for VoidScript
//!
//! Provides miscellaneous utility functions:
//! - assert, panic: Assertions and error handling
//! - clone: Deep clone values
//! - json_parse, json_stringify: JSON handling
//! - trace: Debug tracing
//! - hash: Value hashing
//! - typeof: Type checking (alias for type)
//! - default: Default value handling

use crate::interpreter::Interpreter;
use crate::value::Value;
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

/// Register utility functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    register_assert(interpreter);
    register_panic(interpreter);
    register_clone(interpreter);
    register_json_parse(interpreter);
    register_json_stringify(interpreter);
    register_trace(interpreter);
    register_hash(interpreter);
    register_default(interpreter);
    register_coalesce(interpreter);
    register_identity(interpreter);
    register_constant(interpreter);
    register_noop(interpreter);
    register_compare(interpreter);
    register_equals(interpreter);
    register_to_json(interpreter);
    register_from_json(interpreter);
    register_env(interpreter);
}

/// assert(condition, message?) - Assert a condition is true
fn register_assert(interpreter: &mut Interpreter) {
    interpreter.register_native("assert", |args| {
        if args.is_empty() {
            return Err("assert() requires at least one argument".to_string());
        }

        if !args[0].is_truthy() {
            let message = if args.len() > 1 {
                args[1].to_string_value()
            } else {
                "Assertion failed".to_string()
            };
            return Err(message);
        }

        Ok(Value::Null)
    });
}

/// panic(message) - Abort execution with error
fn register_panic(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("panic", 1, |args| {
        Err(args[0].to_string_value())
    });
}

/// clone(value) - Deep clone a value
fn register_clone(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("clone", 1, |args| {
        // Value already implements Clone, and we do deep clone through the enum
        Ok(args[0].clone())
    });
}

/// json_parse(string) - Parse JSON string to value
fn register_json_parse(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("json_parse", 1, |args| {
        match &args[0] {
            Value::String(s) => {
                let parsed: serde_json::Value = serde_json::from_str(s)
                    .map_err(|e| format!("JSON parse error: {}", e))?;
                Ok(json_to_value(parsed))
            }
            _ => Err("json_parse() expects a string".to_string()),
        }
    });
}

/// json_stringify(value, pretty?) - Convert value to JSON string
fn register_json_stringify(interpreter: &mut Interpreter) {
    interpreter.register_native("json_stringify", |args| {
        if args.is_empty() {
            return Err("json_stringify() requires a value".to_string());
        }

        let pretty = if args.len() > 1 {
            args[1].is_truthy()
        } else {
            false
        };

        let ser = crate::value::SerializableValue::from(&args[0]);
        let json = if pretty {
            serde_json::to_string_pretty(&ser)
        } else {
            serde_json::to_string(&ser)
        };

        json.map(Value::String)
            .map_err(|e| format!("JSON stringify error: {}", e))
    });
}

/// trace(...args) - Print debug trace with file/line info
fn register_trace(interpreter: &mut Interpreter) {
    interpreter.register_native("trace", |args| {
        let output: Vec<String> = args.iter()
            .map(|v| format!("{:?}", v))
            .collect();
        eprintln!("[TRACE] {}", output.join(" "));
        Ok(Value::Null)
    });
}

/// hash(value) - Get hash of a value (for comparison/deduplication)
fn register_hash(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("hash", 1, |args| {
        let hash = compute_hash(&args[0]);
        Ok(Value::Int(hash as i64))
    });
}

/// Helper to compute hash of a value
fn compute_hash(value: &Value) -> u64 {
    let mut hasher = DefaultHasher::new();
    match value {
        Value::Null => 0_u64.hash(&mut hasher),
        Value::Bool(b) => b.hash(&mut hasher),
        Value::Int(n) => n.hash(&mut hasher),
        Value::Float(f) => f.to_bits().hash(&mut hasher),
        Value::String(s) => s.hash(&mut hasher),
        Value::Array(arr) => {
            for item in arr {
                compute_hash(item).hash(&mut hasher);
            }
        }
        Value::Object(obj) => {
            // Sort keys for consistent hashing
            let mut keys: Vec<_> = obj.keys().collect();
            keys.sort();
            for key in keys {
                key.hash(&mut hasher);
                compute_hash(obj.get(key).unwrap()).hash(&mut hasher);
            }
        }
        Value::Function(f) => {
            f.name.hash(&mut hasher);
        }
        Value::Native(n) => {
            n.name.hash(&mut hasher);
        }
    }
    hasher.finish()
}

/// default(value, default_value) - Return default if value is null
fn register_default(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("default", 2, |args| {
        if args[0].is_null() {
            Ok(args[1].clone())
        } else {
            Ok(args[0].clone())
        }
    });
}

/// coalesce(...args) - Return first non-null value
fn register_coalesce(interpreter: &mut Interpreter) {
    interpreter.register_native("coalesce", |args| {
        for arg in args {
            if !arg.is_null() {
                return Ok(arg);
            }
        }
        Ok(Value::Null)
    });
}

/// identity(value) - Return the value unchanged
fn register_identity(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("identity", 1, |args| {
        Ok(args[0].clone())
    });
}

/// constant(value) - Return a function that always returns the value
fn register_constant(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("constant", 1, |args| {
        // In a real implementation, this would return a closure
        // For now, just return the value wrapped in a way that indicates it's a constant
        let mut obj = std::collections::HashMap::new();
        obj.insert("type".to_string(), Value::String("constant".to_string()));
        obj.insert("value".to_string(), args[0].clone());
        Ok(Value::Object(obj))
    });
}

/// noop() - Do nothing, return null
fn register_noop(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("noop", 0, |_| {
        Ok(Value::Null)
    });
}

/// compare(a, b) - Compare two values, return -1, 0, or 1
fn register_compare(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("compare", 2, |args| {
        let result = match (&args[0], &args[1]) {
            (Value::Int(a), Value::Int(b)) => a.cmp(b),
            (Value::Float(a), Value::Float(b)) => {
                a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal)
            }
            (Value::Int(a), Value::Float(b)) => {
                (*a as f64).partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal)
            }
            (Value::Float(a), Value::Int(b)) => {
                a.partial_cmp(&(*b as f64)).unwrap_or(std::cmp::Ordering::Equal)
            }
            (Value::String(a), Value::String(b)) => a.cmp(b),
            (Value::Bool(a), Value::Bool(b)) => a.cmp(b),
            _ => return Err("compare() expects comparable values".to_string()),
        };

        Ok(Value::Int(match result {
            std::cmp::Ordering::Less => -1,
            std::cmp::Ordering::Equal => 0,
            std::cmp::Ordering::Greater => 1,
        }))
    });
}

/// equals(a, b) - Deep equality check
fn register_equals(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("equals", 2, |args| {
        Ok(Value::Bool(deep_equals(&args[0], &args[1])))
    });
}

/// Helper for deep equality
fn deep_equals(a: &Value, b: &Value) -> bool {
    match (a, b) {
        (Value::Null, Value::Null) => true,
        (Value::Bool(a), Value::Bool(b)) => a == b,
        (Value::Int(a), Value::Int(b)) => a == b,
        (Value::Float(a), Value::Float(b)) => (a - b).abs() < f64::EPSILON,
        (Value::Int(a), Value::Float(b)) | (Value::Float(b), Value::Int(a)) => {
            (*a as f64 - b).abs() < f64::EPSILON
        }
        (Value::String(a), Value::String(b)) => a == b,
        (Value::Array(a), Value::Array(b)) => {
            a.len() == b.len() && a.iter().zip(b.iter()).all(|(x, y)| deep_equals(x, y))
        }
        (Value::Object(a), Value::Object(b)) => {
            if a.len() != b.len() {
                return false;
            }
            a.iter().all(|(k, v)| b.get(k).map_or(false, |bv| deep_equals(v, bv)))
        }
        _ => false,
    }
}

/// to_json(value) - Alias for json_stringify
fn register_to_json(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("to_json", 1, |args| {
        let ser = crate::value::SerializableValue::from(&args[0]);
        serde_json::to_string(&ser)
            .map(Value::String)
            .map_err(|e| format!("JSON stringify error: {}", e))
    });
}

/// from_json(string) - Alias for json_parse
fn register_from_json(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("from_json", 1, |args| {
        match &args[0] {
            Value::String(s) => {
                let parsed: serde_json::Value = serde_json::from_str(s)
                    .map_err(|e| format!("JSON parse error: {}", e))?;
                Ok(json_to_value(parsed))
            }
            _ => Err("from_json() expects a string".to_string()),
        }
    });
}

/// env(name, default?) - Get environment variable
fn register_env(interpreter: &mut Interpreter) {
    interpreter.register_native("env", |args| {
        if args.is_empty() {
            return Err("env() requires a variable name".to_string());
        }

        let name = match &args[0] {
            Value::String(s) => s.clone(),
            _ => return Err("env() expects a string variable name".to_string()),
        };

        let default = if args.len() > 1 {
            args[1].clone()
        } else {
            Value::Null
        };

        match std::env::var(&name) {
            Ok(value) => Ok(Value::String(value)),
            Err(_) => Ok(default),
        }
    });
}

/// Convert serde_json::Value to VoidScript Value
fn json_to_value(json: serde_json::Value) -> Value {
    match json {
        serde_json::Value::Null => Value::Null,
        serde_json::Value::Bool(b) => Value::Bool(b),
        serde_json::Value::Number(n) => {
            if let Some(i) = n.as_i64() {
                Value::Int(i)
            } else if let Some(f) = n.as_f64() {
                Value::Float(f)
            } else {
                Value::Null
            }
        }
        serde_json::Value::String(s) => Value::String(s),
        serde_json::Value::Array(arr) => {
            Value::Array(arr.into_iter().map(json_to_value).collect())
        }
        serde_json::Value::Object(obj) => {
            Value::Object(obj.into_iter().map(|(k, v)| (k, json_to_value(v))).collect())
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::VoidScript;
    use crate::value::Value;

    fn run(code: &str) -> Value {
        let mut vs = VoidScript::new();
        vs.execute(code).unwrap()
    }

    fn run_err(code: &str) -> String {
        let mut vs = VoidScript::new();
        match vs.execute(code) {
            Err(e) => e.to_string(),
            Ok(_) => panic!("Expected error"),
        }
    }

    #[test]
    fn test_assert_success() {
        let result = run("assert(true);");
        assert_eq!(result, Value::Null);

        let result = run("assert(1 + 1 == 2);");
        assert_eq!(result, Value::Null);
    }

    #[test]
    fn test_assert_failure() {
        let err = run_err("assert(false);");
        assert!(err.contains("Assertion failed"));

        let err = run_err(r#"assert(false, "Custom message");"#);
        assert!(err.contains("Custom message"));
    }

    #[test]
    fn test_panic() {
        let err = run_err(r#"panic("Something went wrong");"#);
        assert!(err.contains("Something went wrong"));
    }

    #[test]
    fn test_clone() {
        let result = run("clone([1, 2, 3]);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 3);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_json_parse() {
        let result = run(r#"json_parse("{\"x\": 1, \"y\": 2}");"#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("x"), Some(&Value::Int(1)));
            assert_eq!(obj.get("y"), Some(&Value::Int(2)));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_json_stringify() {
        let result = run("json_stringify({x: 1, y: 2});");
        if let Value::String(s) = result {
            assert!(s.contains("\"x\""));
            assert!(s.contains("\"y\""));
        } else {
            panic!("Expected string");
        }
    }

    #[test]
    fn test_json_stringify_pretty() {
        let result = run("json_stringify({x: 1}, true);");
        if let Value::String(s) = result {
            assert!(s.contains("\n")); // Pretty formatting includes newlines
        } else {
            panic!("Expected string");
        }
    }

    #[test]
    fn test_trace() {
        // Just verify it doesn't error
        let result = run("trace(1, 2, 3);");
        assert_eq!(result, Value::Null);
    }

    #[test]
    fn test_hash() {
        let result = run(r#"hash("test");"#);
        assert!(matches!(result, Value::Int(_)));

        // Same values should have same hash
        let h1 = run(r#"hash("hello");"#);
        let h2 = run(r#"hash("hello");"#);
        assert_eq!(h1, h2);

        // Different values should (usually) have different hashes
        let h3 = run(r#"hash("world");"#);
        assert_ne!(h1, h3);
    }

    #[test]
    fn test_default() {
        assert_eq!(run("default(null, 42);"), Value::Int(42));
        assert_eq!(run("default(10, 42);"), Value::Int(10));
    }

    #[test]
    fn test_coalesce() {
        assert_eq!(run("coalesce(null, null, 42, 100);"), Value::Int(42));
        assert_eq!(run("coalesce(1, 2, 3);"), Value::Int(1));
        assert_eq!(run("coalesce(null, null);"), Value::Null);
    }

    #[test]
    fn test_identity() {
        assert_eq!(run("identity(42);"), Value::Int(42));
        assert_eq!(run(r#"identity("hello");"#), Value::String("hello".to_string()));
    }

    #[test]
    fn test_noop() {
        assert_eq!(run("noop();"), Value::Null);
    }

    #[test]
    fn test_compare() {
        assert_eq!(run("compare(1, 2);"), Value::Int(-1));
        assert_eq!(run("compare(2, 2);"), Value::Int(0));
        assert_eq!(run("compare(3, 2);"), Value::Int(1));
        assert_eq!(run(r#"compare("a", "b");"#), Value::Int(-1));
    }

    #[test]
    fn test_equals() {
        assert_eq!(run("equals(1, 1);"), Value::Bool(true));
        assert_eq!(run("equals(1, 2);"), Value::Bool(false));
        assert_eq!(run("equals([1, 2], [1, 2]);"), Value::Bool(true));
        assert_eq!(run("equals([1, 2], [1, 3]);"), Value::Bool(false));
        assert_eq!(run("equals({a: 1}, {a: 1});"), Value::Bool(true));
    }

    #[test]
    fn test_to_from_json() {
        let result = run("to_json({x: 1});");
        assert!(matches!(result, Value::String(_)));

        let result = run(r#"from_json("{\"x\": 1}");"#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("x"), Some(&Value::Int(1)));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_env() {
        // Test with a default value for a variable that likely doesn't exist
        let result = run(r#"env("UNLIKELY_VAR_NAME_12345", "default");"#);
        assert_eq!(result, Value::String("default".to_string()));
    }

    #[test]
    fn test_constant() {
        let result = run("constant(42);");
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("type"), Some(&Value::String("constant".to_string())));
            assert_eq!(obj.get("value"), Some(&Value::Int(42)));
        } else {
            panic!("Expected object");
        }
    }
}
