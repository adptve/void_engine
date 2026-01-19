//! Save/load system

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use thiserror::Error;

/// Save system errors
#[derive(Debug, Error)]
pub enum SaveError {
    /// File I/O error
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    /// Serialization error
    #[error("Serialization error: {0}")]
    Serialization(String),
    /// Deserialization error
    #[error("Deserialization error: {0}")]
    Deserialization(String),
    /// Version mismatch
    #[error("Version mismatch: save version {0}, current version {1}")]
    VersionMismatch(u32, u32),
    /// Slot not found
    #[error("Save slot not found: {0}")]
    SlotNotFound(String),
    /// Corrupted save
    #[error("Corrupted save data")]
    Corrupted,
}

/// Save file format
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SaveFormat {
    /// JSON (human readable)
    Json,
    /// Binary (compact)
    Binary,
}

impl Default for SaveFormat {
    fn default() -> Self {
        Self::Binary
    }
}

/// Save data header
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SaveHeader {
    /// Save format version
    pub version: u32,
    /// Game version
    pub game_version: String,
    /// Save name/title
    pub name: String,
    /// Save timestamp (Unix timestamp)
    pub timestamp: u64,
    /// Play time in seconds
    pub play_time: f64,
    /// Current level/area
    pub level: String,
    /// Preview image path (optional)
    pub screenshot: Option<String>,
}

impl SaveHeader {
    /// Create a new header
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            version: 1,
            game_version: String::new(),
            name: name.into(),
            timestamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_secs())
                .unwrap_or(0),
            play_time: 0.0,
            level: String::new(),
            screenshot: None,
        }
    }

    /// Set game version
    pub fn with_game_version(mut self, version: impl Into<String>) -> Self {
        self.game_version = version.into();
        self
    }

    /// Set play time
    pub fn with_play_time(mut self, time: f64) -> Self {
        self.play_time = time;
        self
    }

    /// Set current level
    pub fn with_level(mut self, level: impl Into<String>) -> Self {
        self.level = level.into();
        self
    }
}

/// Complete save data
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SaveData {
    /// Save header
    pub header: SaveHeader,
    /// Current checkpoint
    pub checkpoint: Option<String>,
    /// Game state data (serialized)
    pub state_data: HashMap<String, Vec<u8>>,
    /// Player data
    pub player_data: HashMap<String, serde_json::Value>,
    /// World/level data
    pub world_data: HashMap<String, serde_json::Value>,
    /// Custom data
    pub custom_data: HashMap<String, serde_json::Value>,
}

impl SaveData {
    /// Create new save data
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            header: SaveHeader::new(name),
            checkpoint: None,
            state_data: HashMap::new(),
            player_data: HashMap::new(),
            world_data: HashMap::new(),
            custom_data: HashMap::new(),
        }
    }

    /// Set checkpoint
    pub fn with_checkpoint(mut self, checkpoint: impl Into<String>) -> Self {
        self.checkpoint = Some(checkpoint.into());
        self
    }

    /// Set player data
    pub fn with_player_data(mut self, key: impl Into<String>, value: serde_json::Value) -> Self {
        self.player_data.insert(key.into(), value);
        self
    }

    /// Set world data
    pub fn with_world_data(mut self, key: impl Into<String>, value: serde_json::Value) -> Self {
        self.world_data.insert(key.into(), value);
        self
    }

    /// Set custom data
    pub fn with_custom_data(mut self, key: impl Into<String>, value: serde_json::Value) -> Self {
        self.custom_data.insert(key.into(), value);
        self
    }

    /// Store binary state data
    pub fn store_state<T: Serialize>(&mut self, key: impl Into<String>, data: &T) -> Result<(), SaveError> {
        let bytes = bincode::serialize(data)
            .map_err(|e| SaveError::Serialization(e.to_string()))?;
        self.state_data.insert(key.into(), bytes);
        Ok(())
    }

    /// Load binary state data
    pub fn load_state<T: for<'de> Deserialize<'de>>(&self, key: &str) -> Result<T, SaveError> {
        let bytes = self.state_data.get(key).ok_or_else(|| {
            SaveError::Deserialization(format!("Key not found: {}", key))
        })?;
        bincode::deserialize(bytes).map_err(|e| SaveError::Deserialization(e.to_string()))
    }
}

/// Save slot info (for displaying in UI)
#[derive(Debug, Clone)]
pub struct SaveSlot {
    /// Slot identifier
    pub id: String,
    /// Save header (or None if empty)
    pub header: Option<SaveHeader>,
    /// File path
    pub path: PathBuf,
    /// Whether slot is occupied
    pub occupied: bool,
}

impl SaveSlot {
    /// Create an empty slot
    pub fn empty(id: impl Into<String>, path: impl Into<PathBuf>) -> Self {
        Self {
            id: id.into(),
            header: None,
            path: path.into(),
            occupied: false,
        }
    }

    /// Create an occupied slot
    pub fn occupied(id: impl Into<String>, path: impl Into<PathBuf>, header: SaveHeader) -> Self {
        Self {
            id: id.into(),
            header: Some(header),
            path: path.into(),
            occupied: true,
        }
    }
}

/// Save manager
pub struct SaveManager {
    /// Base save directory
    save_dir: PathBuf,
    /// Save file format
    format: SaveFormat,
    /// Current save version
    version: u32,
    /// Cached slot info
    slots: HashMap<String, SaveSlot>,
    /// Maximum number of auto-saves
    max_autosaves: usize,
}

impl SaveManager {
    /// Create a new save manager
    pub fn new(save_dir: impl Into<PathBuf>) -> Self {
        Self {
            save_dir: save_dir.into(),
            format: SaveFormat::Binary,
            version: 1,
            slots: HashMap::new(),
            max_autosaves: 3,
        }
    }

    /// Set save format
    pub fn with_format(mut self, format: SaveFormat) -> Self {
        self.format = format;
        self
    }

    /// Set version
    pub fn with_version(mut self, version: u32) -> Self {
        self.version = version;
        self
    }

    /// Set max autosaves
    pub fn with_max_autosaves(mut self, max: usize) -> Self {
        self.max_autosaves = max;
        self
    }

    /// Ensure save directory exists
    pub fn ensure_dir(&self) -> Result<(), SaveError> {
        fs::create_dir_all(&self.save_dir)?;
        Ok(())
    }

    /// Get save file path for a slot
    fn slot_path(&self, slot: &str) -> PathBuf {
        let ext = match self.format {
            SaveFormat::Json => "json",
            SaveFormat::Binary => "sav",
        };
        self.save_dir.join(format!("{}.{}", slot, ext))
    }

    /// Save game to slot
    pub fn save(&mut self, slot: &str, data: &SaveData) -> Result<(), SaveError> {
        self.ensure_dir()?;

        let path = self.slot_path(slot);

        let bytes = match self.format {
            SaveFormat::Json => serde_json::to_vec_pretty(data)
                .map_err(|e| SaveError::Serialization(e.to_string()))?,
            SaveFormat::Binary => bincode::serialize(data)
                .map_err(|e| SaveError::Serialization(e.to_string()))?,
        };

        fs::write(&path, bytes)?;

        // Update cache
        self.slots.insert(
            slot.to_string(),
            SaveSlot::occupied(slot, path, data.header.clone()),
        );

        Ok(())
    }

    /// Load game from slot
    pub fn load(&self, slot: &str) -> Result<SaveData, SaveError> {
        let path = self.slot_path(slot);

        if !path.exists() {
            return Err(SaveError::SlotNotFound(slot.to_string()));
        }

        let bytes = fs::read(&path)?;

        let data: SaveData = match self.format {
            SaveFormat::Json => serde_json::from_slice(&bytes)
                .map_err(|e| SaveError::Deserialization(e.to_string()))?,
            SaveFormat::Binary => bincode::deserialize(&bytes)
                .map_err(|e| SaveError::Deserialization(e.to_string()))?,
        };

        // Check version
        if data.header.version > self.version {
            return Err(SaveError::VersionMismatch(data.header.version, self.version));
        }

        Ok(data)
    }

    /// Delete a save slot
    pub fn delete(&mut self, slot: &str) -> Result<(), SaveError> {
        let path = self.slot_path(slot);

        if path.exists() {
            fs::remove_file(&path)?;
        }

        self.slots.remove(slot);
        Ok(())
    }

    /// Check if slot exists
    pub fn exists(&self, slot: &str) -> bool {
        self.slot_path(slot).exists()
    }

    /// Get slot info
    pub fn get_slot(&mut self, slot: &str) -> Result<SaveSlot, SaveError> {
        if let Some(cached) = self.slots.get(slot) {
            return Ok(cached.clone());
        }

        let path = self.slot_path(slot);

        if !path.exists() {
            let slot_info = SaveSlot::empty(slot, path);
            self.slots.insert(slot.to_string(), slot_info.clone());
            return Ok(slot_info);
        }

        // Read header only
        let data = self.load(slot)?;
        let slot_info = SaveSlot::occupied(slot, path, data.header);
        self.slots.insert(slot.to_string(), slot_info.clone());

        Ok(slot_info)
    }

    /// List all save slots
    pub fn list_slots(&mut self) -> Result<Vec<SaveSlot>, SaveError> {
        self.ensure_dir()?;

        let mut slots = Vec::new();
        let ext = match self.format {
            SaveFormat::Json => "json",
            SaveFormat::Binary => "sav",
        };

        for entry in fs::read_dir(&self.save_dir)? {
            let entry = entry?;
            let path = entry.path();

            if path.extension().map(|e| e == ext).unwrap_or(false) {
                if let Some(stem) = path.file_stem().and_then(|s| s.to_str()) {
                    if let Ok(slot) = self.get_slot(stem) {
                        slots.push(slot);
                    }
                }
            }
        }

        // Sort by timestamp (newest first)
        slots.sort_by(|a, b| {
            let ts_a = a.header.as_ref().map(|h| h.timestamp).unwrap_or(0);
            let ts_b = b.header.as_ref().map(|h| h.timestamp).unwrap_or(0);
            ts_b.cmp(&ts_a)
        });

        Ok(slots)
    }

    /// Auto-save with rotation
    pub fn autosave(&mut self, data: &SaveData) -> Result<String, SaveError> {
        // Find oldest autosave or empty slot
        let mut autosaves: Vec<_> = (0..self.max_autosaves)
            .map(|i| format!("autosave_{}", i))
            .collect();

        // Sort by timestamp (oldest first)
        autosaves.sort_by(|a, b| {
            let ts_a = self.slots.get(a).and_then(|s| s.header.as_ref()).map(|h| h.timestamp).unwrap_or(0);
            let ts_b = self.slots.get(b).and_then(|s| s.header.as_ref()).map(|h| h.timestamp).unwrap_or(0);
            ts_a.cmp(&ts_b)
        });

        let slot = autosaves.first().cloned().unwrap_or_else(|| "autosave_0".to_string());
        self.save(&slot, data)?;

        Ok(slot)
    }

    /// Quick save
    pub fn quicksave(&mut self, data: &SaveData) -> Result<(), SaveError> {
        self.save("quicksave", data)
    }

    /// Quick load
    pub fn quickload(&self) -> Result<SaveData, SaveError> {
        self.load("quicksave")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env::temp_dir;

    #[test]
    fn test_save_data() {
        let save = SaveData::new("Test Save")
            .with_checkpoint("level1")
            .with_player_data("health", serde_json::json!(100))
            .with_world_data("time", serde_json::json!(12.5));

        assert_eq!(save.header.name, "Test Save");
        assert_eq!(save.checkpoint, Some("level1".to_string()));
        assert!(save.player_data.contains_key("health"));
    }

    #[test]
    fn test_save_header() {
        let header = SaveHeader::new("My Save")
            .with_game_version("1.0.0")
            .with_play_time(3600.0)
            .with_level("forest");

        assert_eq!(header.name, "My Save");
        assert_eq!(header.game_version, "1.0.0");
        assert_eq!(header.play_time, 3600.0);
    }

    #[test]
    fn test_state_serialization() {
        #[derive(Serialize, Deserialize, PartialEq, Debug)]
        struct TestState {
            value: i32,
        }

        let mut save = SaveData::new("Test");
        let state = TestState { value: 42 };

        save.store_state("test", &state).unwrap();
        let loaded: TestState = save.load_state("test").unwrap();

        assert_eq!(state, loaded);
    }

    #[test]
    fn test_save_manager() {
        let save_dir = temp_dir().join("void_test_saves");
        let _ = fs::remove_dir_all(&save_dir); // Clean up

        let mut manager = SaveManager::new(&save_dir)
            .with_format(SaveFormat::Json);

        let save = SaveData::new("Test Save")
            .with_checkpoint("start");

        manager.save("slot1", &save).unwrap();
        assert!(manager.exists("slot1"));

        let loaded = manager.load("slot1").unwrap();
        assert_eq!(loaded.header.name, "Test Save");

        manager.delete("slot1").unwrap();
        assert!(!manager.exists("slot1"));

        let _ = fs::remove_dir_all(&save_dir); // Clean up
    }
}
