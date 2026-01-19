//! Interpreter for VoidScript
//!
//! Executes AST nodes and manages runtime state.

use std::collections::HashMap;
use std::sync::Arc;
use thiserror::Error;

use crate::ast::{Expr, Stmt, Program, BinaryOp, UnaryOp};
use crate::value::{Value, Function, NativeFunction};
use crate::environment::Environment;
use crate::builtins;

/// Runtime errors
#[derive(Debug, Error)]
pub enum RuntimeError {
    #[error("Undefined variable: {0}")]
    UndefinedVariable(String),

    #[error("Type error: {0}")]
    TypeError(String),

    #[error("Arity error: expected {expected} arguments, got {got}")]
    ArityError { expected: usize, got: usize },

    #[error("Not callable: {0}")]
    NotCallable(String),

    #[error("Division by zero")]
    DivisionByZero,

    #[error("Index out of bounds: {index} (length {length})")]
    IndexOutOfBounds { index: i64, length: usize },

    #[error("Property not found: {0}")]
    PropertyNotFound(String),

    #[error("Native function error: {0}")]
    NativeError(String),

    #[error("Break outside loop")]
    BreakOutsideLoop,

    #[error("Continue outside loop")]
    ContinueOutsideLoop,

    #[error("Invalid operation: {0}")]
    InvalidOperation(String),
}

/// VoidScript interpreter
pub struct Interpreter {
    /// Variable environment
    environment: Environment,
    /// Native functions
    natives: HashMap<String, NativeFunction>,
}

impl Interpreter {
    /// Create a new interpreter
    pub fn new() -> Self {
        let mut interpreter = Self {
            environment: Environment::new(),
            natives: HashMap::new(),
        };

        // Register built-in functions
        builtins::register_builtins(&mut interpreter);

        interpreter
    }

    /// Execute a program
    pub fn execute(&mut self, program: &Program) -> Result<Value, RuntimeError> {
        let mut result = Value::Null;

        for stmt in &program.statements {
            result = self.execute_stmt(stmt)?;
        }

        Ok(result)
    }

    /// Evaluate an expression (public for REPL)
    pub fn eval_expr(&mut self, expr: &Expr) -> Result<Value, RuntimeError> {
        self.evaluate(expr)
    }

    /// Get a variable value
    pub fn get_var(&self, name: &str) -> Option<Value> {
        // Check natives first
        if let Some(native) = self.natives.get(name) {
            return Some(Value::Native(native.clone()));
        }
        self.environment.get(name)
    }

    /// Set a variable value
    pub fn set_var(&mut self, name: &str, value: Value) {
        if !self.environment.set(name, value.clone()) {
            self.environment.define(name, value);
        }
    }

    /// Register a native function
    pub fn register_native<F>(&mut self, name: &str, func: F)
    where
        F: Fn(Vec<Value>) -> Result<Value, String> + Send + Sync + 'static,
    {
        let native = NativeFunction::new(name, -1, func);
        self.natives.insert(name.to_string(), native.clone());
        self.environment.define_global(name, Value::Native(native));
    }

    /// Register a native function with known arity
    pub fn register_native_with_arity<F>(&mut self, name: &str, arity: i32, func: F)
    where
        F: Fn(Vec<Value>) -> Result<Value, String> + Send + Sync + 'static,
    {
        let native = NativeFunction::new(name, arity, func);
        self.natives.insert(name.to_string(), native.clone());
        self.environment.define_global(name, Value::Native(native));
    }

    /// Get the environment
    pub fn environment(&self) -> &Environment {
        &self.environment
    }

    // === Statement execution ===

    fn execute_stmt(&mut self, stmt: &Stmt) -> Result<Value, RuntimeError> {
        match stmt {
            Stmt::Expr(expr) => self.evaluate(expr),

            Stmt::Let { name, value } => {
                let val = self.evaluate(value)?;
                self.environment.define(name, val);
                Ok(Value::Null)
            }

            Stmt::Assign { name, value } => {
                let val = self.evaluate(value)?;
                if !self.environment.set(name, val) {
                    return Err(RuntimeError::UndefinedVariable(name.clone()));
                }
                Ok(Value::Null)
            }

            Stmt::Block(statements) => {
                self.execute_block(statements)
            }

            Stmt::If { condition, then_branch, else_branch } => {
                let cond_value = self.evaluate(condition)?;
                if cond_value.is_truthy() {
                    self.execute_stmt(then_branch)
                } else if let Some(else_br) = else_branch {
                    self.execute_stmt(else_br)
                } else {
                    Ok(Value::Null)
                }
            }

            Stmt::While { condition, body } => {
                while self.evaluate(condition)?.is_truthy() {
                    match self.execute_stmt(body) {
                        Ok(_) => {}
                        Err(RuntimeError::BreakOutsideLoop) => break,
                        Err(RuntimeError::ContinueOutsideLoop) => continue,
                        Err(e) => return Err(e),
                    }
                }
                Ok(Value::Null)
            }

            Stmt::For { variable, iterable, body } => {
                let iter_value = self.evaluate(iterable)?;
                let items = match iter_value {
                    Value::Array(arr) => arr,
                    Value::String(s) => {
                        s.chars().map(|c| Value::String(c.to_string())).collect()
                    }
                    _ => {
                        return Err(RuntimeError::TypeError(
                            format!("Cannot iterate over {}", iter_value.type_name())
                        ));
                    }
                };

                self.environment.push_scope();
                for item in items {
                    self.environment.define(variable, item);
                    match self.execute_stmt(body) {
                        Ok(_) => {}
                        Err(RuntimeError::BreakOutsideLoop) => break,
                        Err(RuntimeError::ContinueOutsideLoop) => continue,
                        Err(e) => {
                            self.environment.pop_scope();
                            return Err(e);
                        }
                    }
                }
                self.environment.pop_scope();
                Ok(Value::Null)
            }

            Stmt::Function { name, params, body } => {
                let func = Function::new(name.clone(), params.clone(), body.clone());
                self.environment.define(name, Value::Function(func));
                Ok(Value::Null)
            }

            Stmt::Return(expr) => {
                let value = if let Some(e) = expr {
                    self.evaluate(e)?
                } else {
                    Value::Null
                };
                Err(RuntimeError::InvalidOperation(format!("RETURN:{}",
                    serde_json::to_string(&crate::value::SerializableValue::from(&value))
                        .unwrap_or_else(|_| "null".to_string())
                )))
            }

            Stmt::Break => Err(RuntimeError::BreakOutsideLoop),

            Stmt::Continue => Err(RuntimeError::ContinueOutsideLoop),
        }
    }

    fn execute_block(&mut self, statements: &[Stmt]) -> Result<Value, RuntimeError> {
        self.environment.push_scope();
        let mut result = Value::Null;

        for stmt in statements {
            match self.execute_stmt(stmt) {
                Ok(val) => result = val,
                Err(e) => {
                    self.environment.pop_scope();
                    return Err(e);
                }
            }
        }

        self.environment.pop_scope();
        Ok(result)
    }

    // === Expression evaluation ===

    fn evaluate(&mut self, expr: &Expr) -> Result<Value, RuntimeError> {
        match expr {
            Expr::Literal(value) => Ok(value.clone()),

            Expr::Ident(name) => {
                // Check natives first
                if let Some(native) = self.natives.get(name) {
                    return Ok(Value::Native(native.clone()));
                }
                self.environment.get(name)
                    .ok_or_else(|| RuntimeError::UndefinedVariable(name.clone()))
            }

            Expr::Binary { left, op, right } => {
                let lhs = self.evaluate(left)?;
                let rhs = self.evaluate(right)?;
                self.eval_binary(lhs, *op, rhs)
            }

            Expr::Unary { op, expr } => {
                let value = self.evaluate(expr)?;
                self.eval_unary(*op, value)
            }

            Expr::Call { callee, args } => {
                let func = self.evaluate(callee)?;
                let mut arg_values = Vec::with_capacity(args.len());
                for arg in args {
                    arg_values.push(self.evaluate(arg)?);
                }
                self.call_function(func, arg_values)
            }

            Expr::Index { object, index } => {
                let obj = self.evaluate(object)?;
                let idx = self.evaluate(index)?;
                self.eval_index(obj, idx)
            }

            Expr::Member { object, member } => {
                let obj = self.evaluate(object)?;
                self.eval_member(obj, member)
            }

            Expr::Array(elements) => {
                let mut values = Vec::with_capacity(elements.len());
                for elem in elements {
                    values.push(self.evaluate(elem)?);
                }
                Ok(Value::Array(values))
            }

            Expr::Object(pairs) => {
                let mut map = HashMap::new();
                for (key, value_expr) in pairs {
                    let value = self.evaluate(value_expr)?;
                    map.insert(key.clone(), value);
                }
                Ok(Value::Object(map))
            }

            Expr::Ternary { condition, then_expr, else_expr } => {
                let cond = self.evaluate(condition)?;
                if cond.is_truthy() {
                    self.evaluate(then_expr)
                } else {
                    self.evaluate(else_expr)
                }
            }

            Expr::Lambda { params, body } => {
                // Create a closure capturing current scope as HashMap
                let captured = self.environment.snapshot();
                let captured_map = Arc::new(parking_lot::RwLock::new(captured));

                // Lambda body is a single expression, wrap in return
                let func = Function::with_closure(
                    "<lambda>".to_string(),
                    params.clone(),
                    vec![Stmt::Return(Some((**body).clone()))],
                    captured_map,
                );
                Ok(Value::Function(func))
            }
        }
    }

    fn eval_binary(&self, lhs: Value, op: BinaryOp, rhs: Value) -> Result<Value, RuntimeError> {
        match op {
            BinaryOp::Add => {
                match (&lhs, &rhs) {
                    (Value::Int(a), Value::Int(b)) => Ok(Value::Int(a + b)),
                    (Value::Float(a), Value::Float(b)) => Ok(Value::Float(a + b)),
                    (Value::Int(a), Value::Float(b)) => Ok(Value::Float(*a as f64 + b)),
                    (Value::Float(a), Value::Int(b)) => Ok(Value::Float(a + *b as f64)),
                    (Value::String(a), Value::String(b)) => Ok(Value::String(format!("{}{}", a, b))),
                    (Value::String(a), b) => Ok(Value::String(format!("{}{}", a, b.to_string_value()))),
                    (a, Value::String(b)) => Ok(Value::String(format!("{}{}", a.to_string_value(), b))),
                    (Value::Array(a), Value::Array(b)) => {
                        let mut result = a.clone();
                        result.extend(b.clone());
                        Ok(Value::Array(result))
                    }
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot add {} and {}", lhs.type_name(), rhs.type_name()
                    ))),
                }
            }

            BinaryOp::Sub => {
                match (&lhs, &rhs) {
                    (Value::Int(a), Value::Int(b)) => Ok(Value::Int(a - b)),
                    (Value::Float(a), Value::Float(b)) => Ok(Value::Float(a - b)),
                    (Value::Int(a), Value::Float(b)) => Ok(Value::Float(*a as f64 - b)),
                    (Value::Float(a), Value::Int(b)) => Ok(Value::Float(a - *b as f64)),
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot subtract {} from {}", rhs.type_name(), lhs.type_name()
                    ))),
                }
            }

            BinaryOp::Mul => {
                match (&lhs, &rhs) {
                    (Value::Int(a), Value::Int(b)) => Ok(Value::Int(a * b)),
                    (Value::Float(a), Value::Float(b)) => Ok(Value::Float(a * b)),
                    (Value::Int(a), Value::Float(b)) => Ok(Value::Float(*a as f64 * b)),
                    (Value::Float(a), Value::Int(b)) => Ok(Value::Float(a * *b as f64)),
                    (Value::String(s), Value::Int(n)) | (Value::Int(n), Value::String(s)) => {
                        Ok(Value::String(s.repeat(*n as usize)))
                    }
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot multiply {} and {}", lhs.type_name(), rhs.type_name()
                    ))),
                }
            }

            BinaryOp::Div => {
                match (&lhs, &rhs) {
                    (Value::Int(a), Value::Int(b)) => {
                        if *b == 0 {
                            return Err(RuntimeError::DivisionByZero);
                        }
                        Ok(Value::Int(a / b))
                    }
                    (Value::Float(a), Value::Float(b)) => {
                        if *b == 0.0 {
                            return Err(RuntimeError::DivisionByZero);
                        }
                        Ok(Value::Float(a / b))
                    }
                    (Value::Int(a), Value::Float(b)) => {
                        if *b == 0.0 {
                            return Err(RuntimeError::DivisionByZero);
                        }
                        Ok(Value::Float(*a as f64 / b))
                    }
                    (Value::Float(a), Value::Int(b)) => {
                        if *b == 0 {
                            return Err(RuntimeError::DivisionByZero);
                        }
                        Ok(Value::Float(a / *b as f64))
                    }
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot divide {} by {}", lhs.type_name(), rhs.type_name()
                    ))),
                }
            }

            BinaryOp::Mod => {
                match (&lhs, &rhs) {
                    (Value::Int(a), Value::Int(b)) => {
                        if *b == 0 {
                            return Err(RuntimeError::DivisionByZero);
                        }
                        Ok(Value::Int(a % b))
                    }
                    (Value::Float(a), Value::Float(b)) => {
                        if *b == 0.0 {
                            return Err(RuntimeError::DivisionByZero);
                        }
                        Ok(Value::Float(a % b))
                    }
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot modulo {} by {}", lhs.type_name(), rhs.type_name()
                    ))),
                }
            }

            BinaryOp::Eq => Ok(Value::Bool(lhs == rhs)),
            BinaryOp::Ne => Ok(Value::Bool(lhs != rhs)),

            BinaryOp::Lt => {
                match (&lhs, &rhs) {
                    (Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a < b)),
                    (Value::Float(a), Value::Float(b)) => Ok(Value::Bool(a < b)),
                    (Value::Int(a), Value::Float(b)) => Ok(Value::Bool((*a as f64) < *b)),
                    (Value::Float(a), Value::Int(b)) => Ok(Value::Bool(*a < (*b as f64))),
                    (Value::String(a), Value::String(b)) => Ok(Value::Bool(a < b)),
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot compare {} and {}", lhs.type_name(), rhs.type_name()
                    ))),
                }
            }

            BinaryOp::Le => {
                match (&lhs, &rhs) {
                    (Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a <= b)),
                    (Value::Float(a), Value::Float(b)) => Ok(Value::Bool(a <= b)),
                    (Value::Int(a), Value::Float(b)) => Ok(Value::Bool((*a as f64) <= *b)),
                    (Value::Float(a), Value::Int(b)) => Ok(Value::Bool(*a <= (*b as f64))),
                    (Value::String(a), Value::String(b)) => Ok(Value::Bool(a <= b)),
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot compare {} and {}", lhs.type_name(), rhs.type_name()
                    ))),
                }
            }

            BinaryOp::Gt => {
                match (&lhs, &rhs) {
                    (Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a > b)),
                    (Value::Float(a), Value::Float(b)) => Ok(Value::Bool(a > b)),
                    (Value::Int(a), Value::Float(b)) => Ok(Value::Bool((*a as f64) > *b)),
                    (Value::Float(a), Value::Int(b)) => Ok(Value::Bool(*a > (*b as f64))),
                    (Value::String(a), Value::String(b)) => Ok(Value::Bool(a > b)),
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot compare {} and {}", lhs.type_name(), rhs.type_name()
                    ))),
                }
            }

            BinaryOp::Ge => {
                match (&lhs, &rhs) {
                    (Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a >= b)),
                    (Value::Float(a), Value::Float(b)) => Ok(Value::Bool(a >= b)),
                    (Value::Int(a), Value::Float(b)) => Ok(Value::Bool((*a as f64) >= *b)),
                    (Value::Float(a), Value::Int(b)) => Ok(Value::Bool(*a >= (*b as f64))),
                    (Value::String(a), Value::String(b)) => Ok(Value::Bool(a >= b)),
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot compare {} and {}", lhs.type_name(), rhs.type_name()
                    ))),
                }
            }

            BinaryOp::And => {
                Ok(Value::Bool(lhs.is_truthy() && rhs.is_truthy()))
            }

            BinaryOp::Or => {
                Ok(Value::Bool(lhs.is_truthy() || rhs.is_truthy()))
            }
        }
    }

    fn eval_unary(&self, op: UnaryOp, value: Value) -> Result<Value, RuntimeError> {
        match op {
            UnaryOp::Neg => {
                match value {
                    Value::Int(n) => Ok(Value::Int(-n)),
                    Value::Float(f) => Ok(Value::Float(-f)),
                    _ => Err(RuntimeError::TypeError(format!(
                        "Cannot negate {}", value.type_name()
                    ))),
                }
            }
            UnaryOp::Not => {
                Ok(Value::Bool(!value.is_truthy()))
            }
        }
    }

    fn eval_index(&self, object: Value, index: Value) -> Result<Value, RuntimeError> {
        match object {
            Value::Array(arr) => {
                let idx = match index {
                    Value::Int(i) => i,
                    _ => return Err(RuntimeError::TypeError(
                        "Array index must be an integer".to_string()
                    )),
                };

                let actual_idx = if idx < 0 {
                    (arr.len() as i64 + idx) as usize
                } else {
                    idx as usize
                };

                arr.get(actual_idx)
                    .cloned()
                    .ok_or(RuntimeError::IndexOutOfBounds {
                        index: idx,
                        length: arr.len(),
                    })
            }

            Value::String(s) => {
                let idx = match index {
                    Value::Int(i) => i,
                    _ => return Err(RuntimeError::TypeError(
                        "String index must be an integer".to_string()
                    )),
                };

                let chars: Vec<char> = s.chars().collect();
                let actual_idx = if idx < 0 {
                    (chars.len() as i64 + idx) as usize
                } else {
                    idx as usize
                };

                chars.get(actual_idx)
                    .map(|c| Value::String(c.to_string()))
                    .ok_or(RuntimeError::IndexOutOfBounds {
                        index: idx,
                        length: chars.len(),
                    })
            }

            Value::Object(obj) => {
                let key = match index {
                    Value::String(s) => s,
                    _ => return Err(RuntimeError::TypeError(
                        "Object key must be a string".to_string()
                    )),
                };

                obj.get(&key)
                    .cloned()
                    .ok_or(RuntimeError::PropertyNotFound(key))
            }

            _ => Err(RuntimeError::TypeError(format!(
                "Cannot index {}", object.type_name()
            ))),
        }
    }

    fn eval_member(&self, object: Value, member: &str) -> Result<Value, RuntimeError> {
        match object {
            Value::Object(obj) => {
                obj.get(member)
                    .cloned()
                    .ok_or(RuntimeError::PropertyNotFound(member.to_string()))
            }

            Value::Array(arr) => {
                match member {
                    "length" | "len" => Ok(Value::Int(arr.len() as i64)),
                    _ => Err(RuntimeError::PropertyNotFound(member.to_string())),
                }
            }

            Value::String(s) => {
                match member {
                    "length" | "len" => Ok(Value::Int(s.len() as i64)),
                    _ => Err(RuntimeError::PropertyNotFound(member.to_string())),
                }
            }

            _ => Err(RuntimeError::TypeError(format!(
                "Cannot access member on {}", object.type_name()
            ))),
        }
    }

    fn call_function(&mut self, func: Value, args: Vec<Value>) -> Result<Value, RuntimeError> {
        match func {
            Value::Function(f) => {
                if args.len() != f.params.len() {
                    return Err(RuntimeError::ArityError {
                        expected: f.params.len(),
                        got: args.len(),
                    });
                }

                // Create new environment for function
                self.environment.push_scope();

                // Bind parameters
                for (param, arg) in f.params.iter().zip(args) {
                    self.environment.define(param, arg);
                }

                // If function has closure, merge it in
                if let Some(closure) = &f.closure {
                    let captured = closure.read();
                    for (name, value) in captured.iter() {
                        if !self.environment.contains(name) {
                            self.environment.define(name.clone(), value.clone());
                        }
                    }
                }

                // Execute body
                let mut result = Value::Null;
                for stmt in &f.body {
                    match self.execute_stmt(stmt) {
                        Ok(val) => result = val,
                        Err(RuntimeError::InvalidOperation(msg)) if msg.starts_with("RETURN:") => {
                            // Handle return
                            let json = &msg[7..];
                            let ser: crate::value::SerializableValue =
                                serde_json::from_str(json).unwrap_or(crate::value::SerializableValue::Null);
                            result = Value::from(ser);
                            break;
                        }
                        Err(e) => {
                            self.environment.pop_scope();
                            return Err(e);
                        }
                    }
                }

                self.environment.pop_scope();
                Ok(result)
            }

            Value::Native(native) => {
                native.call(args).map_err(RuntimeError::NativeError)
            }

            _ => Err(RuntimeError::NotCallable(func.type_name().to_string())),
        }
    }
}

impl Default for Interpreter {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::Lexer;
    use crate::parser::Parser;

    fn run(source: &str) -> Result<Value, RuntimeError> {
        let tokens = Lexer::new(source).tokenize().unwrap();
        let program = Parser::new(tokens).parse().unwrap();
        let mut interpreter = Interpreter::new();
        interpreter.execute(&program)
    }

    fn eval(source: &str) -> Result<Value, RuntimeError> {
        let tokens = Lexer::new(source).tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        let expr = parser.parse_expression().unwrap();
        let mut interpreter = Interpreter::new();
        interpreter.eval_expr(&expr)
    }

    #[test]
    fn test_arithmetic() {
        assert_eq!(eval("2 + 3").unwrap(), Value::Int(5));
        assert_eq!(eval("10 - 4").unwrap(), Value::Int(6));
        assert_eq!(eval("3 * 4").unwrap(), Value::Int(12));
        assert_eq!(eval("15 / 3").unwrap(), Value::Int(5));
        assert_eq!(eval("17 % 5").unwrap(), Value::Int(2));
    }

    #[test]
    fn test_precedence() {
        assert_eq!(eval("2 + 3 * 4").unwrap(), Value::Int(14));
        assert_eq!(eval("(2 + 3) * 4").unwrap(), Value::Int(20));
    }

    #[test]
    fn test_comparison() {
        assert_eq!(eval("5 > 3").unwrap(), Value::Bool(true));
        assert_eq!(eval("5 < 3").unwrap(), Value::Bool(false));
        assert_eq!(eval("5 == 5").unwrap(), Value::Bool(true));
        assert_eq!(eval("5 != 5").unwrap(), Value::Bool(false));
    }

    #[test]
    fn test_logical() {
        assert_eq!(eval("true && true").unwrap(), Value::Bool(true));
        assert_eq!(eval("true && false").unwrap(), Value::Bool(false));
        assert_eq!(eval("false || true").unwrap(), Value::Bool(true));
        assert_eq!(eval("!true").unwrap(), Value::Bool(false));
    }

    #[test]
    fn test_string_concat() {
        assert_eq!(
            eval(r#""hello" + " " + "world""#).unwrap(),
            Value::String("hello world".to_string())
        );
    }

    #[test]
    fn test_variables() {
        run("let x = 10;").unwrap();
        // New interpreter for each test
        let mut int = Interpreter::new();
        let tokens = Lexer::new("let x = 10; x + 5;").tokenize().unwrap();
        let program = Parser::new(tokens).parse().unwrap();
        int.execute(&program).unwrap();
        assert_eq!(int.get_var("x"), Some(Value::Int(10)));
    }

    #[test]
    fn test_function_call() {
        let code = r#"
            fn double(x) {
                return x * 2;
            }
            double(5);
        "#;
        let result = run(code).unwrap();
        assert_eq!(result, Value::Int(10));
    }

    #[test]
    fn test_while_loop() {
        let code = r#"
            let sum = 0;
            let i = 1;
            while i <= 5 {
                sum = sum + i;
                i = i + 1;
            }
            sum;
        "#;
        let result = run(code).unwrap();
        assert_eq!(result, Value::Int(15));
    }

    #[test]
    fn test_array() {
        assert_eq!(eval("[1, 2, 3][1]").unwrap(), Value::Int(2));
        assert_eq!(eval("[1, 2, 3].len").unwrap(), Value::Int(3));
    }

    #[test]
    fn test_object() {
        assert_eq!(
            eval("{ x: 10, y: 20 }.x").unwrap(),
            Value::Int(10)
        );
    }

    #[test]
    fn test_native_function() {
        let mut interpreter = Interpreter::new();
        interpreter.register_native("triple", |args| {
            match args.first() {
                Some(Value::Int(n)) => Ok(Value::Int(n * 3)),
                _ => Err("Expected integer".to_string()),
            }
        });

        let tokens = Lexer::new("triple(7)").tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        let expr = parser.parse_expression().unwrap();
        let result = interpreter.eval_expr(&expr).unwrap();
        assert_eq!(result, Value::Int(21));
    }

    #[test]
    fn test_division_by_zero() {
        let result = eval("10 / 0");
        assert!(matches!(result, Err(RuntimeError::DivisionByZero)));
    }

    #[test]
    fn test_undefined_variable() {
        let result = eval("undefined_var");
        assert!(matches!(result, Err(RuntimeError::UndefinedVariable(_))));
    }
}
