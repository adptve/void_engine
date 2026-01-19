//! Property system for C++ classes
//!
//! Properties allow configuration values to be passed from TOML to C++ classes.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// A property value that can be passed to C++
///
/// Note: We don't use `#[serde(untagged)]` because bincode doesn't support it.
/// For TOML parsing, the toml crate handles this at a higher level.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum CppPropertyValue {
    /// Boolean value
    Bool(bool),
    /// Integer value
    Int(i64),
    /// Float value
    Float(f64),
    /// String value
    String(String),
    /// Vector2 value
    Vec2([f32; 2]),
    /// Vector3 value
    Vec3([f32; 3]),
    /// Vector4 value
    Vec4([f32; 4]),
    /// Color value (RGBA)
    Color([f32; 4]),
    /// Entity reference (by name)
    Entity(String),
    /// Asset path
    Asset(String),
    /// Array of values
    Array(Vec<CppPropertyValue>),
    /// Nested object
    Object(HashMap<String, CppPropertyValue>),
}

impl CppPropertyValue {
    /// Get as bool
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            CppPropertyValue::Bool(v) => Some(*v),
            _ => None,
        }
    }

    /// Get as integer
    pub fn as_int(&self) -> Option<i64> {
        match self {
            CppPropertyValue::Int(v) => Some(*v),
            CppPropertyValue::Float(v) => Some(*v as i64),
            _ => None,
        }
    }

    /// Get as float
    pub fn as_float(&self) -> Option<f64> {
        match self {
            CppPropertyValue::Float(v) => Some(*v),
            CppPropertyValue::Int(v) => Some(*v as f64),
            _ => None,
        }
    }

    /// Get as string
    pub fn as_string(&self) -> Option<&str> {
        match self {
            CppPropertyValue::String(v) => Some(v),
            CppPropertyValue::Entity(v) => Some(v),
            CppPropertyValue::Asset(v) => Some(v),
            _ => None,
        }
    }

    /// Get as vec3
    pub fn as_vec3(&self) -> Option<[f32; 3]> {
        match self {
            CppPropertyValue::Vec3(v) => Some(*v),
            _ => None,
        }
    }

    /// Get as vec4/color
    pub fn as_vec4(&self) -> Option<[f32; 4]> {
        match self {
            CppPropertyValue::Vec4(v) | CppPropertyValue::Color(v) => Some(*v),
            _ => None,
        }
    }

    /// Get type name for debugging
    pub fn type_name(&self) -> &'static str {
        match self {
            CppPropertyValue::Bool(_) => "bool",
            CppPropertyValue::Int(_) => "int",
            CppPropertyValue::Float(_) => "float",
            CppPropertyValue::String(_) => "string",
            CppPropertyValue::Vec2(_) => "vec2",
            CppPropertyValue::Vec3(_) => "vec3",
            CppPropertyValue::Vec4(_) => "vec4",
            CppPropertyValue::Color(_) => "color",
            CppPropertyValue::Entity(_) => "entity",
            CppPropertyValue::Asset(_) => "asset",
            CppPropertyValue::Array(_) => "array",
            CppPropertyValue::Object(_) => "object",
        }
    }
}

/// Property metadata for editor/reflection
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CppProperty {
    /// Property name
    pub name: String,
    /// Property type
    pub property_type: PropertyType,
    /// Default value
    pub default: CppPropertyValue,
    /// Category for editor grouping
    pub category: Option<String>,
    /// Tooltip/description
    pub description: Option<String>,
    /// Minimum value (for numeric types)
    pub min: Option<f64>,
    /// Maximum value (for numeric types)
    pub max: Option<f64>,
    /// Is this property editable in editor
    pub editable: bool,
    /// Is this property visible in editor
    pub visible: bool,
}

/// Property type enum
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum PropertyType {
    Bool,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Entity,
    Asset,
    Array,
    Object,
}

impl Default for PropertyType {
    fn default() -> Self {
        PropertyType::String
    }
}

/// Map of property name to value
pub type PropertyMap = HashMap<String, CppPropertyValue>;

/// Extension trait for PropertyMap
pub trait PropertyMapExt {
    /// Get a bool property
    fn get_bool(&self, key: &str) -> Option<bool>;
    /// Get an int property
    fn get_int(&self, key: &str) -> Option<i64>;
    /// Get a float property
    fn get_float(&self, key: &str) -> Option<f64>;
    /// Get a string property
    fn get_string(&self, key: &str) -> Option<&str>;
    /// Get a vec3 property
    fn get_vec3(&self, key: &str) -> Option<[f32; 3]>;
}

impl PropertyMapExt for PropertyMap {
    fn get_bool(&self, key: &str) -> Option<bool> {
        self.get(key).and_then(|v| v.as_bool())
    }

    fn get_int(&self, key: &str) -> Option<i64> {
        self.get(key).and_then(|v| v.as_int())
    }

    fn get_float(&self, key: &str) -> Option<f64> {
        self.get(key).and_then(|v| v.as_float())
    }

    fn get_string(&self, key: &str) -> Option<&str> {
        self.get(key).and_then(|v| v.as_string())
    }

    fn get_vec3(&self, key: &str) -> Option<[f32; 3]> {
        self.get(key).and_then(|v| v.as_vec3())
    }
}

/// Serialize properties to a byte buffer for FFI transfer
pub fn serialize_properties(properties: &PropertyMap) -> Result<Vec<u8>, bincode::Error> {
    bincode::serialize(properties)
}

/// Deserialize properties from a byte buffer
pub fn deserialize_properties(data: &[u8]) -> Result<PropertyMap, bincode::Error> {
    bincode::deserialize(data)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_property_value_types() {
        let bool_val = CppPropertyValue::Bool(true);
        assert_eq!(bool_val.as_bool(), Some(true));
        assert_eq!(bool_val.type_name(), "bool");

        let int_val = CppPropertyValue::Int(42);
        assert_eq!(int_val.as_int(), Some(42));
        assert_eq!(int_val.as_float(), Some(42.0));

        let float_val = CppPropertyValue::Float(3.14);
        assert_eq!(float_val.as_float(), Some(3.14));

        let string_val = CppPropertyValue::String("hello".to_string());
        assert_eq!(string_val.as_string(), Some("hello"));

        let vec3_val = CppPropertyValue::Vec3([1.0, 2.0, 3.0]);
        assert_eq!(vec3_val.as_vec3(), Some([1.0, 2.0, 3.0]));
    }

    #[test]
    fn test_property_map() {
        let mut map = PropertyMap::new();
        map.insert("health".to_string(), CppPropertyValue::Float(100.0));
        map.insert("name".to_string(), CppPropertyValue::String("Player".to_string()));
        map.insert("enabled".to_string(), CppPropertyValue::Bool(true));

        assert_eq!(map.get_float("health"), Some(100.0));
        assert_eq!(map.get_string("name"), Some("Player"));
        assert_eq!(map.get_bool("enabled"), Some(true));
        assert_eq!(map.get_bool("missing"), None);
    }

    #[test]
    fn test_serialization() {
        let mut map = PropertyMap::new();
        map.insert("value".to_string(), CppPropertyValue::Int(42));

        let bytes = serialize_properties(&map).unwrap();
        let restored = deserialize_properties(&bytes).unwrap();

        assert_eq!(restored.get_int("value"), Some(42));
    }
}
