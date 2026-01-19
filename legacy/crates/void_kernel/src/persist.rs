//! State Persistence for Crash Recovery
//!
//! The kernel daemon persists critical state to disk periodically, enabling
//! recovery after crashes. This module handles serialization, atomic writes,
//! and state restoration.
//!
//! # Persistence Strategy
//!
//! - State is persisted to `/var/metaverse/state/`
//! - Writes are atomic (write to temp file, then rename)
//! - Multiple versions are kept for rollback
//! - Only critical state is persisted (not full world state)
//!
//! # What Gets Persisted
//!
//! - Frame counter
//! - App registrations and their namespaces
//! - Capability grants
//! - Layer configuration
//! - Asset registry metadata

use serde::{Deserialize, Serialize};
use std::fs::{self, File};
use std::io::{BufReader, BufWriter, Read, Write};
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use void_ir::NamespaceId;

use crate::app::AppId;
use crate::capability::CapabilityId;
use crate::layer::LayerId;
use crate::KernelState;

/// Configuration for the state persister
#[derive(Debug, Clone)]
pub struct PersistConfig {
    /// Base directory for state files
    pub state_dir: PathBuf,
    /// How often to persist state (in frames)
    pub persist_interval_frames: u64,
    /// How often to persist state (in seconds) - takes precedence
    pub persist_interval_secs: f64,
    /// Number of state versions to keep
    pub max_versions: usize,
    /// Whether to compress state files
    pub compress: bool,
}

impl Default for PersistConfig {
    fn default() -> Self {
        Self {
            state_dir: PathBuf::from("/var/metaverse/state"),
            persist_interval_frames: 600, // Every 10 seconds at 60 FPS
            persist_interval_secs: 10.0,
            max_versions: 5,
            compress: false,
        }
    }
}

impl PersistConfig {
    /// Create config for development (uses local directory)
    pub fn development() -> Self {
        Self {
            state_dir: PathBuf::from("./state"),
            persist_interval_frames: 60,
            persist_interval_secs: 1.0,
            max_versions: 3,
            compress: false,
        }
    }

    /// Create config for testing (uses temp directory)
    pub fn testing() -> Self {
        Self {
            state_dir: std::env::temp_dir().join("metaverse_test_state"),
            persist_interval_frames: 1,
            persist_interval_secs: 0.1,
            max_versions: 2,
            compress: false,
        }
    }
}

/// Persisted kernel state
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PersistedState {
    /// Version of the persistence format
    pub version: u32,
    /// When this state was saved
    pub saved_at: u64,
    /// Frame number at save time
    pub frame: u64,
    /// Uptime when saved (seconds)
    pub uptime_secs: f32,
    /// Kernel lifecycle state
    pub kernel_state: KernelStateSerializable,
    /// Persisted apps
    pub apps: Vec<PersistedApp>,
    /// Persisted layers
    pub layers: Vec<PersistedLayer>,
    /// Persisted capabilities
    pub capabilities: Vec<PersistedCapability>,
    /// Backend name
    pub backend: String,
    /// Custom metadata
    pub metadata: std::collections::HashMap<String, String>,
}

impl PersistedState {
    /// Current persistence format version
    pub const VERSION: u32 = 1;

    /// Create a new empty state
    pub fn new(frame: u64, uptime_secs: f32) -> Self {
        Self {
            version: Self::VERSION,
            saved_at: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map(|d| d.as_secs())
                .unwrap_or(0),
            frame,
            uptime_secs,
            kernel_state: KernelStateSerializable::Running,
            apps: Vec::new(),
            layers: Vec::new(),
            capabilities: Vec::new(),
            backend: "unknown".to_string(),
            metadata: std::collections::HashMap::new(),
        }
    }
}

/// Serializable version of KernelState
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum KernelStateSerializable {
    Initializing,
    Running,
    Paused,
    ShuttingDown,
    Stopped,
}

impl From<KernelState> for KernelStateSerializable {
    fn from(state: KernelState) -> Self {
        match state {
            KernelState::Initializing => Self::Initializing,
            KernelState::Running => Self::Running,
            KernelState::Paused => Self::Paused,
            KernelState::ShuttingDown => Self::ShuttingDown,
            KernelState::Stopped => Self::Stopped,
        }
    }
}

impl From<KernelStateSerializable> for KernelState {
    fn from(state: KernelStateSerializable) -> Self {
        match state {
            KernelStateSerializable::Initializing => Self::Initializing,
            KernelStateSerializable::Running => Self::Running,
            KernelStateSerializable::Paused => Self::Paused,
            KernelStateSerializable::ShuttingDown => Self::ShuttingDown,
            KernelStateSerializable::Stopped => Self::Stopped,
        }
    }
}

/// Persisted app information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PersistedApp {
    /// App ID
    pub id: u64,
    /// Namespace ID
    pub namespace_id: u64,
    /// App name
    pub name: String,
    /// App version
    pub version: String,
    /// Was the app running when state was saved
    pub was_running: bool,
    /// App's layers
    pub layers: Vec<u64>,
}

/// Persisted layer information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PersistedLayer {
    /// Layer ID
    pub id: u64,
    /// Layer name
    pub name: String,
    /// Owner namespace
    pub owner: u64,
    /// Z-order priority
    pub priority: i32,
    /// Was visible
    pub visible: bool,
}

/// Persisted capability information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PersistedCapability {
    /// Capability ID
    pub id: u64,
    /// Kind name
    pub kind: String,
    /// Holder namespace
    pub holder: u64,
    /// Grantor namespace
    pub grantor: u64,
    /// Whether delegable
    pub delegable: bool,
}

/// State persister manages saving and loading kernel state
pub struct StatePersister {
    /// Configuration
    config: PersistConfig,
    /// Last persist time
    last_persist: Instant,
    /// Last persist frame
    last_persist_frame: u64,
    /// Whether state directory exists and is writable
    initialized: bool,
    /// Statistics
    stats: PersistStats,
}

/// Persistence statistics
#[derive(Debug, Clone, Default)]
pub struct PersistStats {
    /// Total persists
    pub persists: u64,
    /// Total loads
    pub loads: u64,
    /// Failed persists
    pub persist_failures: u64,
    /// Failed loads
    pub load_failures: u64,
    /// Last persist duration
    pub last_persist_ms: f32,
    /// Average persist duration
    pub avg_persist_ms: f32,
    /// Last state size in bytes
    pub last_state_bytes: u64,
}

/// Errors from persistence operations
#[derive(Debug)]
pub enum PersistError {
    /// IO error
    Io(std::io::Error),
    /// Serialization error
    Serialization(String),
    /// Deserialization error
    Deserialization(String),
    /// Version mismatch
    VersionMismatch { expected: u32, got: u32 },
    /// State directory not initialized
    NotInitialized,
    /// No state found to load
    NoState,
    /// State corrupted
    Corrupted(String),
}

impl std::fmt::Display for PersistError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io(e) => write!(f, "IO error: {}", e),
            Self::Serialization(e) => write!(f, "Serialization error: {}", e),
            Self::Deserialization(e) => write!(f, "Deserialization error: {}", e),
            Self::VersionMismatch { expected, got } => {
                write!(f, "Version mismatch: expected {}, got {}", expected, got)
            }
            Self::NotInitialized => write!(f, "State directory not initialized"),
            Self::NoState => write!(f, "No persisted state found"),
            Self::Corrupted(e) => write!(f, "State corrupted: {}", e),
        }
    }
}

impl std::error::Error for PersistError {}

impl From<std::io::Error> for PersistError {
    fn from(e: std::io::Error) -> Self {
        Self::Io(e)
    }
}

impl StatePersister {
    /// Create a new state persister
    pub fn new(config: PersistConfig) -> Self {
        Self {
            config,
            last_persist: Instant::now(),
            last_persist_frame: 0,
            initialized: false,
            stats: PersistStats::default(),
        }
    }

    /// Initialize the state directory
    pub fn init(&mut self) -> Result<(), PersistError> {
        // Create state directory if it doesn't exist
        fs::create_dir_all(&self.config.state_dir)?;

        // Test write access
        let test_file = self.config.state_dir.join(".write_test");
        fs::write(&test_file, b"test")?;
        fs::remove_file(&test_file)?;

        self.initialized = true;
        log::info!("State persister initialized at {:?}", self.config.state_dir);

        Ok(())
    }

    /// Check if we should persist now
    pub fn should_persist(&self, current_frame: u64, delta_time: f32) -> bool {
        if !self.initialized {
            return false;
        }

        // Check time-based interval
        let elapsed = self.last_persist.elapsed().as_secs_f64();
        if elapsed >= self.config.persist_interval_secs {
            return true;
        }

        // Check frame-based interval
        let frames_since = current_frame.saturating_sub(self.last_persist_frame);
        if frames_since >= self.config.persist_interval_frames {
            return true;
        }

        false
    }

    /// Persist kernel state to disk
    pub fn persist(&mut self, state: &PersistedState) -> Result<(), PersistError> {
        if !self.initialized {
            return Err(PersistError::NotInitialized);
        }

        let start = Instant::now();

        // Generate filename with timestamp
        let filename = format!("state_{}.bin", state.saved_at);
        let temp_path = self.config.state_dir.join(format!(".{}.tmp", filename));
        let final_path = self.config.state_dir.join(&filename);

        // Serialize state
        let data = bincode::serialize(state)
            .map_err(|e| PersistError::Serialization(e.to_string()))?;

        // Write to temp file
        {
            let file = File::create(&temp_path)?;
            let mut writer = BufWriter::new(file);
            writer.write_all(&data)?;
            writer.flush()?;
        }

        // Atomic rename
        fs::rename(&temp_path, &final_path)?;

        // Update "latest" symlink/file
        let latest_path = self.config.state_dir.join("latest");
        let _ = fs::remove_file(&latest_path); // Ignore error if doesn't exist

        // On Windows, we can't create symlinks easily, so write the filename
        fs::write(&latest_path, filename.as_bytes())?;

        // Clean up old versions
        self.cleanup_old_versions()?;

        // Update stats
        let duration = start.elapsed();
        self.stats.persists += 1;
        self.stats.last_persist_ms = duration.as_secs_f32() * 1000.0;
        self.stats.last_state_bytes = data.len() as u64;

        // Update average
        let n = self.stats.persists as f32;
        self.stats.avg_persist_ms =
            (self.stats.avg_persist_ms * (n - 1.0) + self.stats.last_persist_ms) / n;

        self.last_persist = Instant::now();
        self.last_persist_frame = state.frame;

        log::debug!(
            "Persisted state: {} bytes, {:.2}ms",
            data.len(),
            self.stats.last_persist_ms
        );

        Ok(())
    }

    /// Load the most recent persisted state
    pub fn load(&mut self) -> Result<PersistedState, PersistError> {
        if !self.initialized {
            // Try to initialize
            self.init()?;
        }

        // Find the latest state file
        let latest_path = self.config.state_dir.join("latest");

        let state_filename = if latest_path.exists() {
            // Read the filename from the latest marker
            let filename = fs::read_to_string(&latest_path)?;
            filename.trim().to_string()
        } else {
            // Find the most recent state file
            self.find_latest_state_file()?
        };

        let state_path = self.config.state_dir.join(&state_filename);

        if !state_path.exists() {
            return Err(PersistError::NoState);
        }

        // Read and deserialize
        let file = File::open(&state_path)?;
        let mut reader = BufReader::new(file);
        let mut data = Vec::new();
        reader.read_to_end(&mut data)?;

        let state: PersistedState = bincode::deserialize(&data)
            .map_err(|e| PersistError::Deserialization(e.to_string()))?;

        // Validate version
        if state.version != PersistedState::VERSION {
            return Err(PersistError::VersionMismatch {
                expected: PersistedState::VERSION,
                got: state.version,
            });
        }

        self.stats.loads += 1;

        log::info!(
            "Loaded state from {:?}: frame={}, saved_at={}",
            state_path,
            state.frame,
            state.saved_at
        );

        Ok(state)
    }

    /// Load a specific version of the state
    pub fn load_version(&mut self, saved_at: u64) -> Result<PersistedState, PersistError> {
        if !self.initialized {
            self.init()?;
        }

        let filename = format!("state_{}.bin", saved_at);
        let state_path = self.config.state_dir.join(&filename);

        if !state_path.exists() {
            return Err(PersistError::NoState);
        }

        let file = File::open(&state_path)?;
        let mut reader = BufReader::new(file);
        let mut data = Vec::new();
        reader.read_to_end(&mut data)?;

        let state: PersistedState = bincode::deserialize(&data)
            .map_err(|e| PersistError::Deserialization(e.to_string()))?;

        if state.version != PersistedState::VERSION {
            return Err(PersistError::VersionMismatch {
                expected: PersistedState::VERSION,
                got: state.version,
            });
        }

        self.stats.loads += 1;
        Ok(state)
    }

    /// List available state versions
    pub fn list_versions(&self) -> Result<Vec<StateVersion>, PersistError> {
        if !self.initialized {
            return Err(PersistError::NotInitialized);
        }

        let mut versions = Vec::new();

        for entry in fs::read_dir(&self.config.state_dir)? {
            let entry = entry?;
            let name = entry.file_name();
            let name_str = name.to_string_lossy();

            if name_str.starts_with("state_") && name_str.ends_with(".bin") {
                // Parse timestamp from filename
                if let Some(timestamp_str) = name_str
                    .strip_prefix("state_")
                    .and_then(|s| s.strip_suffix(".bin"))
                {
                    if let Ok(timestamp) = timestamp_str.parse::<u64>() {
                        let metadata = entry.metadata()?;
                        versions.push(StateVersion {
                            saved_at: timestamp,
                            size_bytes: metadata.len(),
                        });
                    }
                }
            }
        }

        // Sort by timestamp, newest first
        versions.sort_by(|a, b| b.saved_at.cmp(&a.saved_at));

        Ok(versions)
    }

    /// Find the latest state file by scanning the directory
    fn find_latest_state_file(&self) -> Result<String, PersistError> {
        let mut latest: Option<(u64, String)> = None;

        for entry in fs::read_dir(&self.config.state_dir)? {
            let entry = entry?;
            let name = entry.file_name();
            let name_str = name.to_string_lossy();

            if name_str.starts_with("state_") && name_str.ends_with(".bin") {
                if let Some(timestamp_str) = name_str
                    .strip_prefix("state_")
                    .and_then(|s| s.strip_suffix(".bin"))
                {
                    if let Ok(timestamp) = timestamp_str.parse::<u64>() {
                        match &latest {
                            Some((t, _)) if timestamp > *t => {
                                latest = Some((timestamp, name_str.to_string()));
                            }
                            None => {
                                latest = Some((timestamp, name_str.to_string()));
                            }
                            _ => {}
                        }
                    }
                }
            }
        }

        latest.map(|(_, f)| f).ok_or(PersistError::NoState)
    }

    /// Clean up old state versions
    fn cleanup_old_versions(&self) -> Result<(), PersistError> {
        let versions = self.list_versions()?;

        // Keep only max_versions
        if versions.len() > self.config.max_versions {
            for version in versions.iter().skip(self.config.max_versions) {
                let filename = format!("state_{}.bin", version.saved_at);
                let path = self.config.state_dir.join(&filename);
                if let Err(e) = fs::remove_file(&path) {
                    log::warn!("Failed to remove old state file {:?}: {}", path, e);
                } else {
                    log::debug!("Removed old state file: {}", filename);
                }
            }
        }

        Ok(())
    }

    /// Force an immediate persist
    pub fn force_persist(&mut self, state: &PersistedState) -> Result<(), PersistError> {
        self.persist(state)
    }

    /// Get persist statistics
    pub fn stats(&self) -> &PersistStats {
        &self.stats
    }

    /// Delete all persisted state (for testing)
    pub fn clear(&mut self) -> Result<(), PersistError> {
        if !self.initialized {
            return Ok(());
        }

        for entry in fs::read_dir(&self.config.state_dir)? {
            let entry = entry?;
            let path = entry.path();
            if path.is_file() {
                fs::remove_file(&path)?;
            }
        }

        log::info!("Cleared all persisted state");
        Ok(())
    }

    /// Check if any persisted state exists
    pub fn has_state(&self) -> bool {
        if !self.initialized {
            return false;
        }

        let latest_path = self.config.state_dir.join("latest");
        if latest_path.exists() {
            return true;
        }

        // Check for any state files
        if let Ok(entries) = fs::read_dir(&self.config.state_dir) {
            for entry in entries.flatten() {
                let name = entry.file_name();
                let name_str = name.to_string_lossy();
                if name_str.starts_with("state_") && name_str.ends_with(".bin") {
                    return true;
                }
            }
        }

        false
    }
}

/// Information about a state version
#[derive(Debug, Clone)]
pub struct StateVersion {
    /// When this state was saved (Unix timestamp)
    pub saved_at: u64,
    /// Size of the state file in bytes
    pub size_bytes: u64,
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    fn test_config() -> PersistConfig {
        let dir = env::temp_dir().join(format!(
            "metaverse_persist_test_{}",
            std::process::id()
        ));
        PersistConfig {
            state_dir: dir,
            persist_interval_frames: 1,
            persist_interval_secs: 0.0,
            max_versions: 3,
            compress: false,
        }
    }

    fn cleanup_test_dir(config: &PersistConfig) {
        let _ = fs::remove_dir_all(&config.state_dir);
    }

    #[test]
    fn test_init() {
        let config = test_config();
        let mut persister = StatePersister::new(config.clone());

        assert!(!persister.initialized);
        persister.init().unwrap();
        assert!(persister.initialized);

        cleanup_test_dir(&config);
    }

    #[test]
    fn test_persist_and_load() {
        let config = test_config();
        let mut persister = StatePersister::new(config.clone());
        persister.init().unwrap();

        let state = PersistedState::new(100, 10.5);
        persister.persist(&state).unwrap();

        let loaded = persister.load().unwrap();
        assert_eq!(loaded.frame, 100);
        assert_eq!(loaded.version, PersistedState::VERSION);

        cleanup_test_dir(&config);
    }

    #[test]
    fn test_multiple_versions() {
        let config = test_config();
        let mut persister = StatePersister::new(config.clone());
        persister.init().unwrap();

        // Save multiple states
        for i in 0..5 {
            let mut state = PersistedState::new(i * 100, i as f32);
            // Add small delay to get different timestamps
            std::thread::sleep(std::time::Duration::from_millis(10));
            state.saved_at += i; // Ensure unique timestamps
            persister.persist(&state).unwrap();
        }

        // Should only keep max_versions (3)
        let versions = persister.list_versions().unwrap();
        assert!(versions.len() <= 3);

        cleanup_test_dir(&config);
    }

    #[test]
    fn test_should_persist() {
        let config = PersistConfig {
            persist_interval_secs: 0.1,
            persist_interval_frames: 100,
            ..test_config()
        };
        let mut persister = StatePersister::new(config.clone());
        persister.init().unwrap();

        // Should persist after init
        assert!(persister.should_persist(0, 0.016));

        // Persist to reset timer
        persister.persist(&PersistedState::new(0, 0.0)).unwrap();

        // Shouldn't persist immediately after
        assert!(!persister.should_persist(1, 0.016));

        // Should persist after interval
        std::thread::sleep(std::time::Duration::from_millis(150));
        assert!(persister.should_persist(2, 0.016));

        cleanup_test_dir(&config);
    }

    #[test]
    fn test_has_state() {
        let config = test_config();
        let mut persister = StatePersister::new(config.clone());
        persister.init().unwrap();

        assert!(!persister.has_state());

        persister.persist(&PersistedState::new(0, 0.0)).unwrap();
        assert!(persister.has_state());

        cleanup_test_dir(&config);
    }

    #[test]
    fn test_clear() {
        let config = test_config();
        let mut persister = StatePersister::new(config.clone());
        persister.init().unwrap();

        persister.persist(&PersistedState::new(0, 0.0)).unwrap();
        assert!(persister.has_state());

        persister.clear().unwrap();
        assert!(!persister.has_state());

        cleanup_test_dir(&config);
    }

    #[test]
    fn test_stats() {
        let config = test_config();
        let mut persister = StatePersister::new(config.clone());
        persister.init().unwrap();

        assert_eq!(persister.stats().persists, 0);

        persister.persist(&PersistedState::new(0, 0.0)).unwrap();
        assert_eq!(persister.stats().persists, 1);
        assert!(persister.stats().last_state_bytes > 0);

        persister.load().unwrap();
        assert_eq!(persister.stats().loads, 1);

        cleanup_test_dir(&config);
    }
}
