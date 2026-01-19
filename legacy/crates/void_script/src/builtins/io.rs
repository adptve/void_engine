//! I/O built-in functions for VoidScript
//!
//! Provides functions for input/output operations:
//! - print: Output values to console
//! - println: Output values with newline (alias for print)
//! - debug: Output debug representation
//! - input: Read line from stdin

use crate::interpreter::Interpreter;
use crate::value::Value;

/// Register I/O functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    register_print(interpreter);
    register_println(interpreter);
    register_debug(interpreter);
    register_input(interpreter);
    register_eprint(interpreter);
    register_eprintln(interpreter);
}

/// print(...args) - Output values to console, space-separated
fn register_print(interpreter: &mut Interpreter) {
    interpreter.register_native("print", |args| {
        let output: Vec<String> = args.iter()
            .map(|v| v.to_string_value())
            .collect();
        println!("{}", output.join(" "));
        Ok(Value::Null)
    });
}

/// println(...args) - Output values with newline (same as print currently)
fn register_println(interpreter: &mut Interpreter) {
    interpreter.register_native("println", |args| {
        let output: Vec<String> = args.iter()
            .map(|v| v.to_string_value())
            .collect();
        println!("{}", output.join(" "));
        Ok(Value::Null)
    });
}

/// debug(...args) - Print debug representation of values
fn register_debug(interpreter: &mut Interpreter) {
    interpreter.register_native("debug", |args| {
        for arg in &args {
            println!("{:?}", arg);
        }
        Ok(Value::Null)
    });
}

/// input() - Read a line from stdin
fn register_input(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("input", 0, |_| {
        let mut line = String::new();
        match std::io::stdin().read_line(&mut line) {
            Ok(_) => Ok(Value::String(line.trim_end().to_string())),
            Err(e) => Err(format!("Failed to read input: {}", e)),
        }
    });
}

/// eprint(...args) - Output to stderr
fn register_eprint(interpreter: &mut Interpreter) {
    interpreter.register_native("eprint", |args| {
        let output: Vec<String> = args.iter()
            .map(|v| v.to_string_value())
            .collect();
        eprintln!("{}", output.join(" "));
        Ok(Value::Null)
    });
}

/// eprintln(...args) - Output to stderr with newline
fn register_eprintln(interpreter: &mut Interpreter) {
    interpreter.register_native("eprintln", |args| {
        let output: Vec<String> = args.iter()
            .map(|v| v.to_string_value())
            .collect();
        eprintln!("{}", output.join(" "));
        Ok(Value::Null)
    });
}

#[cfg(test)]
mod tests {
    use crate::VoidScript;
    use crate::value::Value;

    #[test]
    fn test_print_no_error() {
        let mut vs = VoidScript::new();
        // Just verify print doesn't error
        let result = vs.execute("print(1, 2, 3);");
        assert!(result.is_ok());
    }

    #[test]
    fn test_println_no_error() {
        let mut vs = VoidScript::new();
        let result = vs.execute(r#"println("hello", "world");"#);
        assert!(result.is_ok());
    }

    #[test]
    fn test_debug_no_error() {
        let mut vs = VoidScript::new();
        let result = vs.execute("debug([1, 2, 3]);");
        assert!(result.is_ok());
    }

    #[test]
    fn test_print_returns_null() {
        let mut vs = VoidScript::new();
        let result = vs.execute("print(42);").unwrap();
        assert_eq!(result, Value::Null);
    }

    #[test]
    fn test_eprint_no_error() {
        let mut vs = VoidScript::new();
        let result = vs.execute(r#"eprint("error message");"#);
        assert!(result.is_ok());
    }
}
