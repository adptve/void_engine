//! Dynamic value types for IR operations
//!
//! Values are serializable representations of component data that can
//! be transmitted over the patch bus without requiring concrete types.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// A dynamic value that can represent any component data
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum Value {
    /// Null/None value
    Null,
    /// Boolean value
    Bool(bool),
    /// Integer value (i64 for wide compatibility)
    Int(i64),
    /// Floating point value
    Float(f64),
    /// String value
    String(String),
    /// 2D vector
    Vec2([f64; 2]),
    /// 3D vector
    Vec3([f64; 3]),
    /// 4D vector / quaternion
    Vec4([f64; 4]),
    /// 4x4 matrix (column-major)
    Mat4([[f64; 4]; 4]),
    /// Array of values
    Array(Vec<Value>),
    /// Object/map of values
    Object(HashMap<String, Value>),
    /// Raw bytes (base64 encoded in JSON)
    #[serde(with = "base64_serde")]
    Bytes(Vec<u8>),
}

impl Value {
    /// Create a null value
    pub fn null() -> Self {
        Self::Null
    }

    /// Check if value is null
    pub fn is_null(&self) -> bool {
        matches!(self, Self::Null)
    }

    /// Try to get as bool
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Self::Bool(b) => Some(*b),
            _ => None,
        }
    }

    /// Try to get as i64
    pub fn as_int(&self) -> Option<i64> {
        match self {
            Self::Int(i) => Some(*i),
            Self::Float(f) => Some(*f as i64),
            _ => None,
        }
    }

    /// Try to get as f64
    pub fn as_float(&self) -> Option<f64> {
        match self {
            Self::Float(f) => Some(*f),
            Self::Int(i) => Some(*i as f64),
            _ => None,
        }
    }

    /// Try to get as string
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Self::String(s) => Some(s),
            _ => None,
        }
    }

    /// Try to get as vec3
    pub fn as_vec3(&self) -> Option<[f64; 3]> {
        match self {
            Self::Vec3(v) => Some(*v),
            Self::Array(arr) if arr.len() == 3 => {
                Some([
                    arr[0].as_float()?,
                    arr[1].as_float()?,
                    arr[2].as_float()?,
                ])
            }
            _ => None,
        }
    }

    /// Try to get as object
    pub fn as_object(&self) -> Option<&HashMap<String, Value>> {
        match self {
            Self::Object(o) => Some(o),
            _ => None,
        }
    }

    /// Try to get as mutable object
    pub fn as_object_mut(&mut self) -> Option<&mut HashMap<String, Value>> {
        match self {
            Self::Object(o) => Some(o),
            _ => None,
        }
    }

    /// Get a field from an object value
    pub fn get(&self, key: &str) -> Option<&Value> {
        self.as_object()?.get(key)
    }

    /// Set a field on an object value
    pub fn set(&mut self, key: impl Into<String>, value: Value) -> Option<()> {
        self.as_object_mut()?.insert(key.into(), value);
        Some(())
    }
}

impl Default for Value {
    fn default() -> Self {
        Self::Null
    }
}

impl From<bool> for Value {
    fn from(v: bool) -> Self {
        Self::Bool(v)
    }
}

impl From<i32> for Value {
    fn from(v: i32) -> Self {
        Self::Int(v as i64)
    }
}

impl From<i64> for Value {
    fn from(v: i64) -> Self {
        Self::Int(v)
    }
}

impl From<f32> for Value {
    fn from(v: f32) -> Self {
        Self::Float(v as f64)
    }
}

impl From<f64> for Value {
    fn from(v: f64) -> Self {
        Self::Float(v)
    }
}

impl From<String> for Value {
    fn from(v: String) -> Self {
        Self::String(v)
    }
}

impl From<&str> for Value {
    fn from(v: &str) -> Self {
        Self::String(v.to_string())
    }
}

impl From<[f32; 3]> for Value {
    fn from(v: [f32; 3]) -> Self {
        Self::Vec3([v[0] as f64, v[1] as f64, v[2] as f64])
    }
}

impl From<[f64; 3]> for Value {
    fn from(v: [f64; 3]) -> Self {
        Self::Vec3(v)
    }
}

impl From<[f32; 4]> for Value {
    fn from(v: [f32; 4]) -> Self {
        Self::Vec4([v[0] as f64, v[1] as f64, v[2] as f64, v[3] as f64])
    }
}

impl From<[f64; 4]> for Value {
    fn from(v: [f64; 4]) -> Self {
        Self::Vec4(v)
    }
}

impl<K: Into<String>, V: Into<Value>> FromIterator<(K, V)> for Value {
    fn from_iter<I: IntoIterator<Item = (K, V)>>(iter: I) -> Self {
        let map: HashMap<String, Value> = iter
            .into_iter()
            .map(|(k, v)| (k.into(), v.into()))
            .collect();
        Self::Object(map)
    }
}

/// Helper module for base64 serialization of bytes
mod base64_serde {
    use serde::{Deserialize, Deserializer, Serializer};

    pub fn serialize<S: Serializer>(bytes: &[u8], serializer: S) -> Result<S::Ok, S::Error> {
        use base64::Engine;
        let encoded = base64::engine::general_purpose::STANDARD.encode(bytes);
        serializer.serialize_str(&encoded)
    }

    pub fn deserialize<'de, D: Deserializer<'de>>(deserializer: D) -> Result<Vec<u8>, D::Error> {
        use base64::Engine;
        let s = String::deserialize(deserializer)?;
        base64::engine::general_purpose::STANDARD
            .decode(&s)
            .map_err(serde::de::Error::custom)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_value_types() {
        assert!(Value::null().is_null());
        assert_eq!(Value::from(42).as_int(), Some(42));
        assert_eq!(Value::from(3.14).as_float(), Some(3.14));
        assert_eq!(Value::from("hello").as_str(), Some("hello"));
    }

    #[test]
    fn test_object_value() {
        let obj: Value = [
            ("x", Value::from(1.0)),
            ("y", Value::from(2.0)),
            ("z", Value::from(3.0)),
        ]
        .into_iter()
        .collect();

        assert_eq!(obj.get("x").and_then(|v| v.as_float()), Some(1.0));
    }
}
