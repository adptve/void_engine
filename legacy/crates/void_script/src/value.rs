//! Runtime values for VoidScript
//!
//! Defines the value types that can be stored and manipulated at runtime.

use std::collections::HashMap;
use std::fmt;
use std::sync::Arc;
use parking_lot::RwLock;
use serde::{Serialize, Deserialize};

use crate::ast::Stmt;

/// Runtime value type
#[derive(Debug, Clone)]
pub enum Value {
    /// Null/unit value
    Null,
    /// Boolean
    Bool(bool),
    /// Integer (64-bit signed)
    Int(i64),
    /// Float (64-bit)
    Float(f64),
    /// String
    String(String),
    /// Array of values
    Array(Vec<Value>),
    /// Object/map
    Object(HashMap<String, Value>),
    /// User-defined function
    Function(Function),
    /// Native/built-in function
    Native(NativeFunction),
}

impl Value {
    /// Check if value is truthy
    pub fn is_truthy(&self) -> bool {
        match self {
            Self::Null => false,
            Self::Bool(b) => *b,
            Self::Int(n) => *n != 0,
            Self::Float(f) => *f != 0.0,
            Self::String(s) => !s.is_empty(),
            Self::Array(arr) => !arr.is_empty(),
            Self::Object(obj) => !obj.is_empty(),
            Self::Function(_) | Self::Native(_) => true,
        }
    }

    /// Get type name
    pub fn type_name(&self) -> &'static str {
        match self {
            Self::Null => "null",
            Self::Bool(_) => "bool",
            Self::Int(_) => "int",
            Self::Float(_) => "float",
            Self::String(_) => "string",
            Self::Array(_) => "array",
            Self::Object(_) => "object",
            Self::Function(_) => "function",
            Self::Native(_) => "native",
        }
    }

    /// Try to convert to int
    pub fn to_int(&self) -> Option<i64> {
        match self {
            Self::Int(n) => Some(*n),
            Self::Float(f) => Some(*f as i64),
            Self::Bool(b) => Some(if *b { 1 } else { 0 }),
            Self::String(s) => s.parse().ok(),
            _ => None,
        }
    }

    /// Try to convert to float
    pub fn to_float(&self) -> Option<f64> {
        match self {
            Self::Int(n) => Some(*n as f64),
            Self::Float(f) => Some(*f),
            Self::Bool(b) => Some(if *b { 1.0 } else { 0.0 }),
            Self::String(s) => s.parse().ok(),
            _ => None,
        }
    }

    /// Try to convert to string
    pub fn to_string_value(&self) -> String {
        match self {
            Self::Null => "null".to_string(),
            Self::Bool(b) => b.to_string(),
            Self::Int(n) => n.to_string(),
            Self::Float(f) => f.to_string(),
            Self::String(s) => s.clone(),
            Self::Array(arr) => {
                let items: Vec<String> = arr.iter()
                    .map(|v| v.to_string_value())
                    .collect();
                format!("[{}]", items.join(", "))
            }
            Self::Object(obj) => {
                let items: Vec<String> = obj.iter()
                    .map(|(k, v)| format!("{}: {}", k, v.to_string_value()))
                    .collect();
                format!("{{{}}}", items.join(", "))
            }
            Self::Function(f) => format!("<function {}>", f.name),
            Self::Native(n) => format!("<native {}>", n.name),
        }
    }

    /// Check if value is null
    pub fn is_null(&self) -> bool {
        matches!(self, Self::Null)
    }

    /// Check if value is a number (int or float)
    pub fn is_number(&self) -> bool {
        matches!(self, Self::Int(_) | Self::Float(_))
    }

    /// Check if value is callable
    pub fn is_callable(&self) -> bool {
        matches!(self, Self::Function(_) | Self::Native(_))
    }
}

impl PartialEq for Value {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Null, Self::Null) => true,
            (Self::Bool(a), Self::Bool(b)) => a == b,
            (Self::Int(a), Self::Int(b)) => a == b,
            (Self::Float(a), Self::Float(b)) => (a - b).abs() < f64::EPSILON,
            (Self::Int(a), Self::Float(b)) | (Self::Float(b), Self::Int(a)) => {
                (*a as f64 - b).abs() < f64::EPSILON
            }
            (Self::String(a), Self::String(b)) => a == b,
            (Self::Array(a), Self::Array(b)) => a == b,
            _ => false,
        }
    }
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.to_string_value())
    }
}

/// User-defined function
#[derive(Debug, Clone)]
pub struct Function {
    /// Function name
    pub name: String,
    /// Parameter names
    pub params: Vec<String>,
    /// Function body
    pub body: Vec<Stmt>,
    /// Closure environment (captured variables)
    pub closure: Option<Arc<RwLock<HashMap<String, Value>>>>,
}

impl Function {
    /// Create a new function
    pub fn new(name: String, params: Vec<String>, body: Vec<Stmt>) -> Self {
        Self {
            name,
            params,
            body,
            closure: None,
        }
    }

    /// Create a function with closure
    pub fn with_closure(
        name: String,
        params: Vec<String>,
        body: Vec<Stmt>,
        closure: Arc<RwLock<HashMap<String, Value>>>,
    ) -> Self {
        Self {
            name,
            params,
            body,
            closure: Some(closure),
        }
    }

    /// Get arity (number of parameters)
    pub fn arity(&self) -> usize {
        self.params.len()
    }
}

/// Native function type
pub type NativeFn = Arc<dyn Fn(Vec<Value>) -> Result<Value, String> + Send + Sync>;

/// Native/built-in function
#[derive(Clone)]
pub struct NativeFunction {
    /// Function name
    pub name: String,
    /// Expected number of arguments (-1 for variadic)
    pub arity: i32,
    /// The function implementation
    pub func: NativeFn,
}

impl NativeFunction {
    /// Create a new native function
    pub fn new<F>(name: impl Into<String>, arity: i32, func: F) -> Self
    where
        F: Fn(Vec<Value>) -> Result<Value, String> + Send + Sync + 'static,
    {
        Self {
            name: name.into(),
            arity,
            func: Arc::new(func),
        }
    }

    /// Call the native function
    pub fn call(&self, args: Vec<Value>) -> Result<Value, String> {
        if self.arity >= 0 && args.len() != self.arity as usize {
            return Err(format!(
                "Function '{}' expected {} arguments, got {}",
                self.name, self.arity, args.len()
            ));
        }
        (self.func)(args)
    }
}

impl fmt::Debug for NativeFunction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("NativeFunction")
            .field("name", &self.name)
            .field("arity", &self.arity)
            .finish()
    }
}

/// Serializable value (for persistence/network)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum SerializableValue {
    Null,
    Bool(bool),
    Int(i64),
    Float(f64),
    String(String),
    Array(Vec<SerializableValue>),
    Object(HashMap<String, SerializableValue>),
}

impl From<&Value> for SerializableValue {
    fn from(value: &Value) -> Self {
        match value {
            Value::Null => Self::Null,
            Value::Bool(b) => Self::Bool(*b),
            Value::Int(n) => Self::Int(*n),
            Value::Float(f) => Self::Float(*f),
            Value::String(s) => Self::String(s.clone()),
            Value::Array(arr) => {
                Self::Array(arr.iter().map(SerializableValue::from).collect())
            }
            Value::Object(obj) => {
                Self::Object(obj.iter()
                    .map(|(k, v)| (k.clone(), SerializableValue::from(v)))
                    .collect())
            }
            Value::Function(_) | Value::Native(_) => Self::Null,
        }
    }
}

impl From<SerializableValue> for Value {
    fn from(value: SerializableValue) -> Self {
        match value {
            SerializableValue::Null => Self::Null,
            SerializableValue::Bool(b) => Self::Bool(b),
            SerializableValue::Int(n) => Self::Int(n),
            SerializableValue::Float(f) => Self::Float(f),
            SerializableValue::String(s) => Self::String(s),
            SerializableValue::Array(arr) => {
                Self::Array(arr.into_iter().map(Value::from).collect())
            }
            SerializableValue::Object(obj) => {
                Self::Object(obj.into_iter()
                    .map(|(k, v)| (k, Value::from(v)))
                    .collect())
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_value_truthiness() {
        assert!(!Value::Null.is_truthy());
        assert!(!Value::Bool(false).is_truthy());
        assert!(Value::Bool(true).is_truthy());
        assert!(!Value::Int(0).is_truthy());
        assert!(Value::Int(1).is_truthy());
        assert!(!Value::String(String::new()).is_truthy());
        assert!(Value::String("hello".to_string()).is_truthy());
    }

    #[test]
    fn test_value_equality() {
        assert_eq!(Value::Int(5), Value::Int(5));
        assert_eq!(Value::Int(5), Value::Float(5.0));
        assert_ne!(Value::Int(5), Value::Int(6));
        assert_eq!(Value::String("a".to_string()), Value::String("a".to_string()));
    }

    #[test]
    fn test_value_conversions() {
        assert_eq!(Value::Int(42).to_float(), Some(42.0));
        assert_eq!(Value::Float(3.14).to_int(), Some(3));
        assert_eq!(Value::String("123".to_string()).to_int(), Some(123));
    }

    #[test]
    fn test_value_display() {
        assert_eq!(Value::Null.to_string(), "null");
        assert_eq!(Value::Bool(true).to_string(), "true");
        assert_eq!(Value::Int(42).to_string(), "42");
        assert_eq!(Value::String("hello".to_string()).to_string(), "hello");
    }

    #[test]
    fn test_native_function() {
        let func = NativeFunction::new("add", 2, |args| {
            match (&args[0], &args[1]) {
                (Value::Int(a), Value::Int(b)) => Ok(Value::Int(a + b)),
                _ => Err("Expected integers".to_string()),
            }
        });

        assert_eq!(func.arity, 2);
        let result = func.call(vec![Value::Int(2), Value::Int(3)]).unwrap();
        assert_eq!(result, Value::Int(5));
    }

    #[test]
    fn test_serializable_roundtrip() {
        let value = Value::Object(HashMap::from([
            ("name".to_string(), Value::String("test".to_string())),
            ("count".to_string(), Value::Int(42)),
        ]));

        let serializable = SerializableValue::from(&value);
        let json = serde_json::to_string(&serializable).unwrap();
        let restored: SerializableValue = serde_json::from_str(&json).unwrap();
        let back: Value = Value::from(restored);

        match back {
            Value::Object(obj) => {
                assert_eq!(obj.get("name"), Some(&Value::String("test".to_string())));
                assert_eq!(obj.get("count"), Some(&Value::Int(42)));
            }
            _ => panic!("Expected object"),
        }
    }
}
