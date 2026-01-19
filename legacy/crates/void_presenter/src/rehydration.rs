//! Rehydration support for hot-swap
//!
//! Enables state restoration without restart.

use std::collections::HashMap;
use serde::{Serialize, Deserialize};
use parking_lot::RwLock;

/// Rehydration state container
#[derive(Debug, Clone, Default)]
pub struct RehydrationState {
    /// String values
    string_values: HashMap<String, String>,
    /// Integer values
    int_values: HashMap<String, i64>,
    /// Float values
    float_values: HashMap<String, f64>,
    /// Boolean values
    bool_values: HashMap<String, bool>,
    /// Binary data
    binary_values: HashMap<String, Vec<u8>>,
    /// Nested states
    nested_states: HashMap<String, RehydrationState>,
}

impl RehydrationState {
    /// Create empty state
    pub fn new() -> Self {
        Self::default()
    }

    /// Add a typed value
    pub fn with_value<T: RehydrationValue>(mut self, key: &str, value: T) -> Self {
        value.store(&mut self, key);
        self
    }

    /// Set a typed value
    pub fn set_value<T: RehydrationValue>(&mut self, key: &str, value: T) {
        value.store(self, key);
    }

    /// Get a typed value
    pub fn get_value<T: RehydrationValue>(&self, key: &str) -> Option<T> {
        T::load(self, key)
    }

    /// Add string value
    pub fn set_string(&mut self, key: &str, value: String) {
        self.string_values.insert(key.to_string(), value);
    }

    /// Get string value
    pub fn get_string(&self, key: &str) -> Option<&String> {
        self.string_values.get(key)
    }

    /// Add int value
    pub fn set_int(&mut self, key: &str, value: i64) {
        self.int_values.insert(key.to_string(), value);
    }

    /// Get int value
    pub fn get_int(&self, key: &str) -> Option<i64> {
        self.int_values.get(key).copied()
    }

    /// Add float value
    pub fn set_float(&mut self, key: &str, value: f64) {
        self.float_values.insert(key.to_string(), value);
    }

    /// Get float value
    pub fn get_float(&self, key: &str) -> Option<f64> {
        self.float_values.get(key).copied()
    }

    /// Add bool value
    pub fn set_bool(&mut self, key: &str, value: bool) {
        self.bool_values.insert(key.to_string(), value);
    }

    /// Get bool value
    pub fn get_bool(&self, key: &str) -> Option<bool> {
        self.bool_values.get(key).copied()
    }

    /// Add binary value
    pub fn set_binary(&mut self, key: &str, value: Vec<u8>) {
        self.binary_values.insert(key.to_string(), value);
    }

    /// Get binary value
    pub fn get_binary(&self, key: &str) -> Option<&Vec<u8>> {
        self.binary_values.get(key)
    }

    /// Add nested state
    pub fn set_nested(&mut self, key: &str, state: RehydrationState) {
        self.nested_states.insert(key.to_string(), state);
    }

    /// Get nested state
    pub fn get_nested(&self, key: &str) -> Option<&RehydrationState> {
        self.nested_states.get(key)
    }

    /// Get mutable nested state
    pub fn get_nested_mut(&mut self, key: &str) -> Option<&mut RehydrationState> {
        self.nested_states.get_mut(key)
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.string_values.is_empty()
            && self.int_values.is_empty()
            && self.float_values.is_empty()
            && self.bool_values.is_empty()
            && self.binary_values.is_empty()
            && self.nested_states.is_empty()
    }

    /// Clear all values
    pub fn clear(&mut self) {
        self.string_values.clear();
        self.int_values.clear();
        self.float_values.clear();
        self.bool_values.clear();
        self.binary_values.clear();
        self.nested_states.clear();
    }

    /// Merge another state into this one
    pub fn merge(&mut self, other: RehydrationState) {
        self.string_values.extend(other.string_values);
        self.int_values.extend(other.int_values);
        self.float_values.extend(other.float_values);
        self.bool_values.extend(other.bool_values);
        self.binary_values.extend(other.binary_values);
        self.nested_states.extend(other.nested_states);
    }

    /// Serialize to JSON
    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string(&self.to_serializable())
    }

    /// Deserialize from JSON
    pub fn from_json(json: &str) -> Result<Self, serde_json::Error> {
        let serializable: SerializableState = serde_json::from_str(json)?;
        Ok(Self::from_serializable(serializable))
    }

    fn to_serializable(&self) -> SerializableState {
        SerializableState {
            strings: self.string_values.clone(),
            ints: self.int_values.clone(),
            floats: self.float_values.clone(),
            bools: self.bool_values.clone(),
            binaries: self.binary_values.iter()
                .map(|(k, v)| (k.clone(), base64::encode(v)))
                .collect(),
            nested: self.nested_states.iter()
                .map(|(k, v)| (k.clone(), v.to_serializable()))
                .collect(),
        }
    }

    fn from_serializable(s: SerializableState) -> Self {
        Self {
            string_values: s.strings,
            int_values: s.ints,
            float_values: s.floats,
            bool_values: s.bools,
            binary_values: s.binaries.into_iter()
                .filter_map(|(k, v)| base64::decode(&v).ok().map(|d| (k, d)))
                .collect(),
            nested_states: s.nested.into_iter()
                .map(|(k, v)| (k, Self::from_serializable(v)))
                .collect(),
        }
    }
}

/// Simple base64 encoding/decoding
mod base64 {
    const CHARS: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    pub fn encode(data: &[u8]) -> String {
        let mut result = String::new();
        for chunk in data.chunks(3) {
            let b = match chunk.len() {
                1 => [chunk[0], 0, 0],
                2 => [chunk[0], chunk[1], 0],
                _ => [chunk[0], chunk[1], chunk[2]],
            };

            let n = ((b[0] as u32) << 16) | ((b[1] as u32) << 8) | (b[2] as u32);
            result.push(CHARS[((n >> 18) & 63) as usize] as char);
            result.push(CHARS[((n >> 12) & 63) as usize] as char);

            if chunk.len() > 1 {
                result.push(CHARS[((n >> 6) & 63) as usize] as char);
            } else {
                result.push('=');
            }

            if chunk.len() > 2 {
                result.push(CHARS[(n & 63) as usize] as char);
            } else {
                result.push('=');
            }
        }
        result
    }

    pub fn decode(s: &str) -> Result<Vec<u8>, ()> {
        let mut result = Vec::new();
        let chars: Vec<u8> = s.bytes().filter(|&b| b != b'=').collect();

        for chunk in chars.chunks(4) {
            if chunk.len() < 2 {
                break;
            }

            let decode_char = |c: u8| -> Result<u32, ()> {
                match c {
                    b'A'..=b'Z' => Ok((c - b'A') as u32),
                    b'a'..=b'z' => Ok((c - b'a' + 26) as u32),
                    b'0'..=b'9' => Ok((c - b'0' + 52) as u32),
                    b'+' => Ok(62),
                    b'/' => Ok(63),
                    _ => Err(()),
                }
            };

            let a = decode_char(chunk[0])?;
            let b = decode_char(chunk[1])?;
            let c = chunk.get(2).map(|&x| decode_char(x)).transpose()?.unwrap_or(0);
            let d = chunk.get(3).map(|&x| decode_char(x)).transpose()?.unwrap_or(0);

            let n = (a << 18) | (b << 12) | (c << 6) | d;

            result.push((n >> 16) as u8);
            if chunk.len() > 2 {
                result.push((n >> 8) as u8);
            }
            if chunk.len() > 3 {
                result.push(n as u8);
            }
        }

        Ok(result)
    }
}

/// Serializable state for JSON
#[derive(Debug, Clone, Serialize, Deserialize)]
struct SerializableState {
    #[serde(default)]
    strings: HashMap<String, String>,
    #[serde(default)]
    ints: HashMap<String, i64>,
    #[serde(default)]
    floats: HashMap<String, f64>,
    #[serde(default)]
    bools: HashMap<String, bool>,
    #[serde(default)]
    binaries: HashMap<String, String>,
    #[serde(default)]
    nested: HashMap<String, SerializableState>,
}

/// Trait for types that can be stored in rehydration state
pub trait RehydrationValue: Sized {
    fn store(&self, state: &mut RehydrationState, key: &str);
    fn load(state: &RehydrationState, key: &str) -> Option<Self>;
}

impl RehydrationValue for String {
    fn store(&self, state: &mut RehydrationState, key: &str) {
        state.set_string(key, self.clone());
    }

    fn load(state: &RehydrationState, key: &str) -> Option<Self> {
        state.get_string(key).cloned()
    }
}

impl RehydrationValue for i64 {
    fn store(&self, state: &mut RehydrationState, key: &str) {
        state.set_int(key, *self);
    }

    fn load(state: &RehydrationState, key: &str) -> Option<Self> {
        state.get_int(key)
    }
}

impl RehydrationValue for u64 {
    fn store(&self, state: &mut RehydrationState, key: &str) {
        state.set_int(key, *self as i64);
    }

    fn load(state: &RehydrationState, key: &str) -> Option<Self> {
        state.get_int(key).map(|v| v as u64)
    }
}

impl RehydrationValue for f64 {
    fn store(&self, state: &mut RehydrationState, key: &str) {
        state.set_float(key, *self);
    }

    fn load(state: &RehydrationState, key: &str) -> Option<Self> {
        state.get_float(key)
    }
}

impl RehydrationValue for bool {
    fn store(&self, state: &mut RehydrationState, key: &str) {
        state.set_bool(key, *self);
    }

    fn load(state: &RehydrationState, key: &str) -> Option<Self> {
        state.get_bool(key)
    }
}

impl RehydrationValue for Vec<u8> {
    fn store(&self, state: &mut RehydrationState, key: &str) {
        state.set_binary(key, self.clone());
    }

    fn load(state: &RehydrationState, key: &str) -> Option<Self> {
        state.get_binary(key).cloned()
    }
}

/// Trait for types that can be rehydrated
pub trait Rehydratable {
    /// Get current state for rehydration
    fn dehydrate(&self) -> RehydrationState;

    /// Restore from rehydration state
    fn rehydrate(&mut self, state: RehydrationState) -> Result<(), RehydrationError>;
}

/// Rehydration error
#[derive(Debug, thiserror::Error)]
pub enum RehydrationError {
    #[error("Missing required field: {0}")]
    MissingField(String),

    #[error("Invalid data: {0}")]
    InvalidData(String),

    #[error("Version mismatch: expected {expected}, got {actual}")]
    VersionMismatch { expected: String, actual: String },

    #[error("Serialization error: {0}")]
    SerializationError(String),
}

/// Rehydration store for managing multiple states
pub struct RehydrationStore {
    states: RwLock<HashMap<String, RehydrationState>>,
}

impl RehydrationStore {
    /// Create empty store
    pub fn new() -> Self {
        Self {
            states: RwLock::new(HashMap::new()),
        }
    }

    /// Store state
    pub fn store(&self, key: &str, state: RehydrationState) {
        self.states.write().insert(key.to_string(), state);
    }

    /// Retrieve state
    pub fn retrieve(&self, key: &str) -> Option<RehydrationState> {
        self.states.read().get(key).cloned()
    }

    /// Remove state
    pub fn remove(&self, key: &str) -> Option<RehydrationState> {
        self.states.write().remove(key)
    }

    /// Check if state exists
    pub fn contains(&self, key: &str) -> bool {
        self.states.read().contains_key(key)
    }

    /// Get all keys
    pub fn keys(&self) -> Vec<String> {
        self.states.read().keys().cloned().collect()
    }

    /// Clear all states
    pub fn clear(&self) {
        self.states.write().clear();
    }

    /// Serialize all states to JSON
    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        let states: HashMap<String, SerializableState> = self.states.read()
            .iter()
            .map(|(k, v)| (k.clone(), v.to_serializable()))
            .collect();
        serde_json::to_string(&states)
    }

    /// Deserialize from JSON
    pub fn from_json(json: &str) -> Result<Self, serde_json::Error> {
        let states: HashMap<String, SerializableState> = serde_json::from_str(json)?;
        Ok(Self {
            states: RwLock::new(
                states.into_iter()
                    .map(|(k, v)| (k, RehydrationState::from_serializable(v)))
                    .collect()
            ),
        })
    }
}

impl Default for RehydrationStore {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_rehydration_state() {
        let state = RehydrationState::new()
            .with_value("count", 42i64)
            .with_value("name", "test".to_string())
            .with_value("enabled", true);

        assert_eq!(state.get_value::<i64>("count"), Some(42));
        assert_eq!(state.get_value::<String>("name"), Some("test".to_string()));
        assert_eq!(state.get_value::<bool>("enabled"), Some(true));
    }

    #[test]
    fn test_nested_state() {
        let mut state = RehydrationState::new();

        let inner = RehydrationState::new()
            .with_value("x", 10i64)
            .with_value("y", 20i64);

        state.set_nested("position", inner);

        let retrieved = state.get_nested("position").unwrap();
        assert_eq!(retrieved.get_value::<i64>("x"), Some(10));
        assert_eq!(retrieved.get_value::<i64>("y"), Some(20));
    }

    #[test]
    fn test_json_serialization() {
        let state = RehydrationState::new()
            .with_value("count", 42i64)
            .with_value("name", "test".to_string());

        let json = state.to_json().unwrap();
        let restored = RehydrationState::from_json(&json).unwrap();

        assert_eq!(restored.get_value::<i64>("count"), Some(42));
        assert_eq!(restored.get_value::<String>("name"), Some("test".to_string()));
    }

    #[test]
    fn test_binary_values() {
        let state = RehydrationState::new()
            .with_value("data", vec![1u8, 2, 3, 4, 5]);

        let json = state.to_json().unwrap();
        let restored = RehydrationState::from_json(&json).unwrap();

        assert_eq!(restored.get_value::<Vec<u8>>("data"), Some(vec![1, 2, 3, 4, 5]));
    }

    #[test]
    fn test_rehydration_store() {
        let store = RehydrationStore::new();

        let state = RehydrationState::new()
            .with_value("version", 1i64);

        store.store("presenter_1", state);

        assert!(store.contains("presenter_1"));
        let retrieved = store.retrieve("presenter_1").unwrap();
        assert_eq!(retrieved.get_value::<i64>("version"), Some(1));
    }

    #[test]
    fn test_base64() {
        let data = b"Hello, World!";
        let encoded = base64::encode(data);
        let decoded = base64::decode(&encoded).unwrap();
        assert_eq!(decoded, data);
    }
}
