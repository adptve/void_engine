//! Type built-in functions for VoidScript
//!
//! Provides functions for type inspection and conversion:
//! - type: Get the type name of a value
//! - str: Convert to string
//! - int: Convert to integer
//! - float: Convert to float
//! - bool: Convert to boolean
//! - is_null, is_number, is_string, is_array, is_object, is_function: Type checks

use crate::interpreter::Interpreter;
use crate::value::Value;

/// Register type functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    register_type(interpreter);
    register_str(interpreter);
    register_int(interpreter);
    register_float(interpreter);
    register_bool(interpreter);
    register_is_null(interpreter);
    register_is_number(interpreter);
    register_is_string(interpreter);
    register_is_array(interpreter);
    register_is_object(interpreter);
    register_is_function(interpreter);
    register_is_bool(interpreter);
    register_is_int(interpreter);
    register_is_float(interpreter);
    register_typeof(interpreter);
}

/// type(value) - Get the type name of a value
fn register_type(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("type", 1, |args| {
        Ok(Value::String(args[0].type_name().to_string()))
    });
}

/// typeof(value) - Alias for type()
fn register_typeof(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("typeof", 1, |args| {
        Ok(Value::String(args[0].type_name().to_string()))
    });
}

/// str(value) - Convert value to string
fn register_str(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("str", 1, |args| {
        Ok(Value::String(args[0].to_string_value()))
    });
}

/// int(value) - Convert value to integer
fn register_int(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("int", 1, |args| {
        match args[0].to_int() {
            Some(n) => Ok(Value::Int(n)),
            None => Err(format!("Cannot convert {} to int", args[0].type_name())),
        }
    });
}

/// float(value) - Convert value to float
fn register_float(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("float", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f)),
            None => Err(format!("Cannot convert {} to float", args[0].type_name())),
        }
    });
}

/// bool(value) - Convert value to boolean based on truthiness
fn register_bool(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("bool", 1, |args| {
        Ok(Value::Bool(args[0].is_truthy()))
    });
}

/// is_null(value) - Check if value is null
fn register_is_null(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_null", 1, |args| {
        Ok(Value::Bool(args[0].is_null()))
    });
}

/// is_number(value) - Check if value is a number (int or float)
fn register_is_number(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_number", 1, |args| {
        Ok(Value::Bool(args[0].is_number()))
    });
}

/// is_string(value) - Check if value is a string
fn register_is_string(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_string", 1, |args| {
        Ok(Value::Bool(matches!(args[0], Value::String(_))))
    });
}

/// is_array(value) - Check if value is an array
fn register_is_array(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_array", 1, |args| {
        Ok(Value::Bool(matches!(args[0], Value::Array(_))))
    });
}

/// is_object(value) - Check if value is an object
fn register_is_object(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_object", 1, |args| {
        Ok(Value::Bool(matches!(args[0], Value::Object(_))))
    });
}

/// is_function(value) - Check if value is callable (function or native)
fn register_is_function(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_function", 1, |args| {
        Ok(Value::Bool(args[0].is_callable()))
    });
}

/// is_bool(value) - Check if value is a boolean
fn register_is_bool(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_bool", 1, |args| {
        Ok(Value::Bool(matches!(args[0], Value::Bool(_))))
    });
}

/// is_int(value) - Check if value is an integer
fn register_is_int(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_int", 1, |args| {
        Ok(Value::Bool(matches!(args[0], Value::Int(_))))
    });
}

/// is_float(value) - Check if value is a float
fn register_is_float(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_float", 1, |args| {
        Ok(Value::Bool(matches!(args[0], Value::Float(_))))
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
    fn test_type() {
        assert_eq!(run("type(42);"), Value::String("int".to_string()));
        assert_eq!(run("type(3.14);"), Value::String("float".to_string()));
        assert_eq!(run(r#"type("hello");"#), Value::String("string".to_string()));
        assert_eq!(run("type(true);"), Value::String("bool".to_string()));
        assert_eq!(run("type(null);"), Value::String("null".to_string()));
        assert_eq!(run("type([1,2,3]);"), Value::String("array".to_string()));
        assert_eq!(run("type({x: 1});"), Value::String("object".to_string()));
    }

    #[test]
    fn test_typeof() {
        assert_eq!(run("typeof(42);"), Value::String("int".to_string()));
    }

    #[test]
    fn test_str() {
        assert_eq!(run("str(42);"), Value::String("42".to_string()));
        assert_eq!(run("str(3.14);"), Value::String("3.14".to_string()));
        assert_eq!(run("str(true);"), Value::String("true".to_string()));
        assert_eq!(run("str(null);"), Value::String("null".to_string()));
    }

    #[test]
    fn test_int() {
        assert_eq!(run("int(3.7);"), Value::Int(3));
        assert_eq!(run(r#"int("42");"#), Value::Int(42));
        assert_eq!(run("int(true);"), Value::Int(1));
        assert_eq!(run("int(false);"), Value::Int(0));
    }

    #[test]
    fn test_float() {
        assert_eq!(run("float(42);"), Value::Float(42.0));
        assert_eq!(run(r#"float("3.14");"#), Value::Float(3.14));
    }

    #[test]
    fn test_bool() {
        assert_eq!(run("bool(0);"), Value::Bool(false));
        assert_eq!(run("bool(1);"), Value::Bool(true));
        assert_eq!(run(r#"bool("");"#), Value::Bool(false));
        assert_eq!(run(r#"bool("hello");"#), Value::Bool(true));
        assert_eq!(run("bool([]);"), Value::Bool(false));
        assert_eq!(run("bool([1]);"), Value::Bool(true));
    }

    #[test]
    fn test_is_null() {
        assert_eq!(run("is_null(null);"), Value::Bool(true));
        assert_eq!(run("is_null(0);"), Value::Bool(false));
    }

    #[test]
    fn test_is_number() {
        assert_eq!(run("is_number(42);"), Value::Bool(true));
        assert_eq!(run("is_number(3.14);"), Value::Bool(true));
        assert_eq!(run(r#"is_number("42");"#), Value::Bool(false));
    }

    #[test]
    fn test_is_string() {
        assert_eq!(run(r#"is_string("hello");"#), Value::Bool(true));
        assert_eq!(run("is_string(42);"), Value::Bool(false));
    }

    #[test]
    fn test_is_array() {
        assert_eq!(run("is_array([1, 2, 3]);"), Value::Bool(true));
        assert_eq!(run("is_array({});"), Value::Bool(false));
    }

    #[test]
    fn test_is_object() {
        assert_eq!(run("is_object({x: 1});"), Value::Bool(true));
        assert_eq!(run("is_object([]);"), Value::Bool(false));
    }

    #[test]
    fn test_is_function() {
        let mut vs = VoidScript::new();
        vs.execute("fn foo() { return 1; }").unwrap();
        let result = vs.execute("is_function(foo);").unwrap();
        assert_eq!(result, Value::Bool(true));

        assert_eq!(run("is_function(42);"), Value::Bool(false));
        // Built-in functions are also callable
        assert_eq!(run("is_function(print);"), Value::Bool(true));
    }

    #[test]
    fn test_is_bool() {
        assert_eq!(run("is_bool(true);"), Value::Bool(true));
        assert_eq!(run("is_bool(false);"), Value::Bool(true));
        assert_eq!(run("is_bool(1);"), Value::Bool(false));
    }

    #[test]
    fn test_is_int() {
        assert_eq!(run("is_int(42);"), Value::Bool(true));
        assert_eq!(run("is_int(3.14);"), Value::Bool(false));
    }

    #[test]
    fn test_is_float() {
        assert_eq!(run("is_float(3.14);"), Value::Bool(true));
        assert_eq!(run("is_float(42);"), Value::Bool(false));
    }
}
