//! Value - Runtime values for visual scripting
//!
//! Values are the data that flows through the graph.

use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use core::any::{Any, TypeId};
use core::fmt;

/// Type information for a value
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum ValueType {
    /// No value (void/unit)
    None,
    /// Boolean
    Bool,
    /// 32-bit integer
    Int,
    /// 32-bit float
    Float,
    /// String
    String,
    /// 2D vector
    Vec2,
    /// 3D vector
    Vec3,
    /// 4D vector
    Vec4,
    /// Quaternion
    Quat,
    /// Transform
    Transform,
    /// Color (RGBA)
    Color,
    /// Entity reference
    Entity,
    /// Asset reference
    Asset,
    /// Array of values
    Array(Box<ValueType>),
    /// Custom/user-defined type
    Custom(String),
    /// Any type (wildcard)
    Any,
    /// Execution flow (not a value, just control flow)
    Exec,
}

impl ValueType {
    /// Check if this type is compatible with another
    pub fn is_compatible_with(&self, other: &ValueType) -> bool {
        match (self, other) {
            (ValueType::Any, _) | (_, ValueType::Any) => true,
            (ValueType::None, ValueType::None) => true,
            (ValueType::Bool, ValueType::Bool) => true,
            (ValueType::Int, ValueType::Int) => true,
            (ValueType::Float, ValueType::Float) => true,
            (ValueType::Int, ValueType::Float) | (ValueType::Float, ValueType::Int) => true, // Implicit conversion
            (ValueType::String, ValueType::String) => true,
            (ValueType::Vec2, ValueType::Vec2) => true,
            (ValueType::Vec3, ValueType::Vec3) => true,
            (ValueType::Vec4, ValueType::Vec4) => true,
            (ValueType::Quat, ValueType::Quat) => true,
            (ValueType::Transform, ValueType::Transform) => true,
            (ValueType::Color, ValueType::Color) => true,
            (ValueType::Entity, ValueType::Entity) => true,
            (ValueType::Asset, ValueType::Asset) => true,
            (ValueType::Array(a), ValueType::Array(b)) => a.is_compatible_with(b),
            (ValueType::Custom(a), ValueType::Custom(b)) => a == b,
            (ValueType::Exec, ValueType::Exec) => true,
            _ => false,
        }
    }

    /// Check if this is an execution pin type
    pub fn is_exec(&self) -> bool {
        matches!(self, ValueType::Exec)
    }

    /// Get a default value for this type
    pub fn default_value(&self) -> Value {
        match self {
            ValueType::None => Value::None,
            ValueType::Bool => Value::Bool(false),
            ValueType::Int => Value::Int(0),
            ValueType::Float => Value::Float(0.0),
            ValueType::String => Value::String(String::new()),
            ValueType::Vec2 => Value::Vec2([0.0, 0.0]),
            ValueType::Vec3 => Value::Vec3([0.0, 0.0, 0.0]),
            ValueType::Vec4 => Value::Vec4([0.0, 0.0, 0.0, 0.0]),
            ValueType::Quat => Value::Quat([0.0, 0.0, 0.0, 1.0]),
            ValueType::Transform => Value::Transform {
                position: [0.0, 0.0, 0.0],
                rotation: [0.0, 0.0, 0.0, 1.0],
                scale: [1.0, 1.0, 1.0],
            },
            ValueType::Color => Value::Color([1.0, 1.0, 1.0, 1.0]),
            ValueType::Entity => Value::Entity(0),
            ValueType::Asset => Value::Asset(0),
            ValueType::Array(_) => Value::Array(Vec::new()),
            ValueType::Custom(_) => Value::None,
            ValueType::Any => Value::None,
            ValueType::Exec => Value::None,
        }
    }
}

impl Default for ValueType {
    fn default() -> Self {
        Self::None
    }
}

/// Runtime value
#[derive(Clone, Debug)]
pub enum Value {
    /// No value
    None,
    /// Boolean
    Bool(bool),
    /// Integer
    Int(i32),
    /// Float
    Float(f32),
    /// String
    String(String),
    /// 2D vector
    Vec2([f32; 2]),
    /// 3D vector
    Vec3([f32; 3]),
    /// 4D vector
    Vec4([f32; 4]),
    /// Quaternion
    Quat([f32; 4]),
    /// Transform
    Transform {
        position: [f32; 3],
        rotation: [f32; 4],
        scale: [f32; 3],
    },
    /// Color (RGBA)
    Color([f32; 4]),
    /// Entity reference (entity ID as u64)
    Entity(u64),
    /// Asset reference (asset ID as u64)
    Asset(u64),
    /// Array of values
    Array(Vec<Value>),
    /// Custom boxed value
    Custom(Box<dyn CustomValue>),
}

impl Value {
    /// Get the type of this value
    pub fn value_type(&self) -> ValueType {
        match self {
            Value::None => ValueType::None,
            Value::Bool(_) => ValueType::Bool,
            Value::Int(_) => ValueType::Int,
            Value::Float(_) => ValueType::Float,
            Value::String(_) => ValueType::String,
            Value::Vec2(_) => ValueType::Vec2,
            Value::Vec3(_) => ValueType::Vec3,
            Value::Vec4(_) => ValueType::Vec4,
            Value::Quat(_) => ValueType::Quat,
            Value::Transform { .. } => ValueType::Transform,
            Value::Color(_) => ValueType::Color,
            Value::Entity(_) => ValueType::Entity,
            Value::Asset(_) => ValueType::Asset,
            Value::Array(arr) => {
                let elem_type = arr.first()
                    .map(|v| v.value_type())
                    .unwrap_or(ValueType::Any);
                ValueType::Array(Box::new(elem_type))
            }
            Value::Custom(c) => ValueType::Custom(c.type_name().to_string()),
        }
    }

    /// Try to convert to bool
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Value::Bool(b) => Some(*b),
            Value::Int(i) => Some(*i != 0),
            Value::Float(f) => Some(*f != 0.0),
            _ => None,
        }
    }

    /// Try to convert to int
    pub fn as_int(&self) -> Option<i32> {
        match self {
            Value::Int(i) => Some(*i),
            Value::Float(f) => Some(*f as i32),
            Value::Bool(b) => Some(if *b { 1 } else { 0 }),
            _ => None,
        }
    }

    /// Try to convert to float
    pub fn as_float(&self) -> Option<f32> {
        match self {
            Value::Float(f) => Some(*f),
            Value::Int(i) => Some(*i as f32),
            _ => None,
        }
    }

    /// Try to get as string
    pub fn as_string(&self) -> Option<&str> {
        match self {
            Value::String(s) => Some(s),
            _ => None,
        }
    }

    /// Try to get as vec3
    pub fn as_vec3(&self) -> Option<[f32; 3]> {
        match self {
            Value::Vec3(v) => Some(*v),
            Value::Vec4(v) => Some([v[0], v[1], v[2]]),
            _ => None,
        }
    }

    /// Check if the value is "truthy"
    pub fn is_truthy(&self) -> bool {
        match self {
            Value::None => false,
            Value::Bool(b) => *b,
            Value::Int(i) => *i != 0,
            Value::Float(f) => *f != 0.0,
            Value::String(s) => !s.is_empty(),
            Value::Array(a) => !a.is_empty(),
            _ => true,
        }
    }
}

impl Default for Value {
    fn default() -> Self {
        Self::None
    }
}

impl PartialEq for Value {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Value::None, Value::None) => true,
            (Value::Bool(a), Value::Bool(b)) => a == b,
            (Value::Int(a), Value::Int(b)) => a == b,
            (Value::Float(a), Value::Float(b)) => (a - b).abs() < f32::EPSILON,
            (Value::String(a), Value::String(b)) => a == b,
            (Value::Vec2(a), Value::Vec2(b)) => a == b,
            (Value::Vec3(a), Value::Vec3(b)) => a == b,
            (Value::Vec4(a), Value::Vec4(b)) => a == b,
            (Value::Quat(a), Value::Quat(b)) => a == b,
            (Value::Entity(a), Value::Entity(b)) => a == b,
            (Value::Asset(a), Value::Asset(b)) => a == b,
            (Value::Array(a), Value::Array(b)) => a == b,
            _ => false,
        }
    }
}

/// Trait for custom value types
pub trait CustomValue: Send + Sync {
    /// Get the type name
    fn type_name(&self) -> &'static str;

    /// Clone the value
    fn clone_box(&self) -> Box<dyn CustomValue>;

    /// Debug representation
    fn debug(&self) -> String;

    /// Get as Any for downcasting
    fn as_any(&self) -> &dyn Any;
}

impl Clone for Box<dyn CustomValue> {
    fn clone(&self) -> Self {
        self.clone_box()
    }
}

impl fmt::Debug for Box<dyn CustomValue> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.debug())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_value_type_compatibility() {
        assert!(ValueType::Int.is_compatible_with(&ValueType::Float));
        assert!(ValueType::Any.is_compatible_with(&ValueType::String));
        assert!(!ValueType::String.is_compatible_with(&ValueType::Int));
    }

    #[test]
    fn test_value_conversions() {
        let int_val = Value::Int(42);
        assert_eq!(int_val.as_int(), Some(42));
        assert_eq!(int_val.as_float(), Some(42.0));
        assert_eq!(int_val.as_bool(), Some(true));

        let float_val = Value::Float(3.14);
        assert_eq!(float_val.as_int(), Some(3));
        assert!((float_val.as_float().unwrap() - 3.14).abs() < 0.01);
    }

    #[test]
    fn test_truthy() {
        assert!(!Value::None.is_truthy());
        assert!(!Value::Bool(false).is_truthy());
        assert!(Value::Bool(true).is_truthy());
        assert!(!Value::Int(0).is_truthy());
        assert!(Value::Int(1).is_truthy());
        assert!(Value::String("hello".into()).is_truthy());
        assert!(!Value::String(String::new()).is_truthy());
    }
}
