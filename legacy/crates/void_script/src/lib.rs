//! # VoidScript
//!
//! A simple scripting language for Void Engine providing:
//! - Variable bindings
//! - Functions and closures
//! - Control flow (if/else, loops)
//! - Built-in functions for engine interaction
//! - REPL integration
//! - Kernel integration via ScriptContext
//!
//! ## Syntax Overview
//!
//! ```text
//! // Variables
//! let x = 10;
//! let name = "hello";
//!
//! // Functions
//! fn add(a, b) {
//!     return a + b;
//! }
//!
//! // Control flow
//! if x > 5 {
//!     print("big");
//! } else {
//!     print("small");
//! }
//!
//! // Loops
//! while x > 0 {
//!     x = x - 1;
//! }
//!
//! // Built-ins (emit real patches when context is live)
//! let player = spawn("player");
//! set_position(player, 10, 0, 5);
//! ```
//!
//! ## Kernel Integration
//!
//! When a ScriptContext with a NamespaceHandle is provided, builtins
//! emit real IR patches to the kernel:
//!
//! ```ignore
//! use void_script::{VoidScript, ScriptContext};
//! use void_ir::NamespaceHandle;
//!
//! let context = ScriptContext::new(handle);
//! let mut vs = VoidScript::with_context(context);
//! vs.execute("spawn(\"player\")")?; // Emits EntityPatch::Create
//! ```

pub mod lexer;
pub mod parser;
pub mod ast;
pub mod value;
pub mod interpreter;
pub mod builtins;
pub mod environment;
pub mod context;

pub use lexer::{Lexer, Token, TokenKind};
pub use parser::{Parser, ParseError};
pub use ast::{Expr, Stmt, Program};
pub use value::Value;
pub use interpreter::{Interpreter, RuntimeError};
pub use environment::Environment;
pub use context::{ScriptContext, script_to_ir_value, ir_to_script_value};

use thiserror::Error;

/// VoidScript errors
#[derive(Debug, Error)]
pub enum ScriptError {
    #[error("Lexer error: {0}")]
    LexerError(String),

    #[error("Parse error: {0}")]
    ParseError(#[from] ParseError),

    #[error("Runtime error: {0}")]
    RuntimeError(#[from] RuntimeError),

    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
}

/// Script execution result
pub type ScriptResult<T> = Result<T, ScriptError>;

/// VoidScript engine
pub struct VoidScript {
    interpreter: Interpreter,
    context: ScriptContext,
}

impl VoidScript {
    /// Create a new VoidScript engine with mock context
    pub fn new() -> Self {
        let context = ScriptContext::mock();
        let mut interpreter = Interpreter::new();
        builtins::register_context_builtins(&mut interpreter, context.clone());
        Self {
            interpreter,
            context,
        }
    }

    /// Create a VoidScript engine with a live context (connected to kernel)
    pub fn with_context(context: ScriptContext) -> Self {
        let mut interpreter = Interpreter::new();
        builtins::register_context_builtins(&mut interpreter, context.clone());
        Self {
            interpreter,
            context,
        }
    }

    /// Get the script context
    pub fn context(&self) -> &ScriptContext {
        &self.context
    }

    /// Check if connected to kernel (live mode)
    pub fn is_live(&self) -> bool {
        self.context.is_live()
    }

    /// Execute a script and return the result
    pub fn execute(&mut self, source: &str) -> ScriptResult<Value> {
        let tokens = Lexer::new(source).tokenize()?;
        let program = Parser::new(tokens).parse()?;
        let result = self.interpreter.execute(&program)?;
        Ok(result)
    }

    /// Execute a single expression and return the result
    pub fn eval(&mut self, source: &str) -> ScriptResult<Value> {
        let tokens = Lexer::new(source).tokenize()?;
        let mut parser = Parser::new(tokens);
        let expr = parser.parse_expression()?;
        let result = self.interpreter.eval_expr(&expr)?;
        Ok(result)
    }

    /// Get a variable value
    pub fn get_var(&self, name: &str) -> Option<Value> {
        self.interpreter.get_var(name)
    }

    /// Set a variable value
    pub fn set_var(&mut self, name: &str, value: Value) {
        self.interpreter.set_var(name, value);
    }

    /// Register a native function
    pub fn register_fn<F>(&mut self, name: &str, func: F)
    where
        F: Fn(Vec<Value>) -> Result<Value, String> + Send + Sync + 'static,
    {
        self.interpreter.register_native(name, func);
    }

    /// Get the interpreter environment
    pub fn environment(&self) -> &Environment {
        self.interpreter.environment()
    }

    /// Reset the interpreter state (keeps context)
    pub fn reset(&mut self) {
        self.interpreter = Interpreter::new();
        builtins::register_context_builtins(&mut self.interpreter, self.context.clone());
    }
}

impl Default for VoidScript {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_basic_arithmetic() {
        let mut vs = VoidScript::new();
        let result = vs.eval("1 + 2 * 3").unwrap();
        assert_eq!(result, Value::Int(7));
    }

    #[test]
    fn test_variables() {
        let mut vs = VoidScript::new();
        vs.execute("let x = 10;").unwrap();
        let result = vs.eval("x + 5").unwrap();
        assert_eq!(result, Value::Int(15));
    }

    #[test]
    fn test_strings() {
        let mut vs = VoidScript::new();
        let result = vs.eval(r#""hello" + " world""#).unwrap();
        assert_eq!(result, Value::String("hello world".to_string()));
    }

    #[test]
    fn test_comparison() {
        let mut vs = VoidScript::new();
        assert_eq!(vs.eval("5 > 3").unwrap(), Value::Bool(true));
        assert_eq!(vs.eval("5 < 3").unwrap(), Value::Bool(false));
        assert_eq!(vs.eval("5 == 5").unwrap(), Value::Bool(true));
    }

    #[test]
    fn test_if_else() {
        let mut vs = VoidScript::new();
        vs.execute(r#"
            let result = 0;
            if 5 > 3 {
                result = 1;
            } else {
                result = 2;
            }
        "#).unwrap();
        assert_eq!(vs.get_var("result"), Some(Value::Int(1)));
    }

    #[test]
    fn test_while_loop() {
        let mut vs = VoidScript::new();
        vs.execute(r#"
            let sum = 0;
            let i = 1;
            while i <= 5 {
                sum = sum + i;
                i = i + 1;
            }
        "#).unwrap();
        assert_eq!(vs.get_var("sum"), Some(Value::Int(15)));
    }

    #[test]
    fn test_function() {
        let mut vs = VoidScript::new();
        vs.execute(r#"
            fn add(a, b) {
                return a + b;
            }
            let result = add(3, 4);
        "#).unwrap();
        assert_eq!(vs.get_var("result"), Some(Value::Int(7)));
    }

    #[test]
    fn test_native_function() {
        let mut vs = VoidScript::new();
        vs.register_fn("double", |args| {
            if let Some(Value::Int(n)) = args.first() {
                Ok(Value::Int(n * 2))
            } else {
                Err("Expected integer argument".to_string())
            }
        });
        let result = vs.eval("double(21)").unwrap();
        assert_eq!(result, Value::Int(42));
    }
}
