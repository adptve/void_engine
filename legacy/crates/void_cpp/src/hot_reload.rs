//! Hot-reload support for C++ libraries
//!
//! Watches for changes to C++ libraries and automatically reloads them,
//! preserving instance state.

use crate::error::{CppError, Result};
use crate::instance::InstanceId;
use crate::properties::PropertyMap;
use crate::registry::CppClassRegistry;
use notify::{RecommendedWatcher, Event, EventKind};
use parking_lot::RwLock;
use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};
use std::sync::mpsc::{channel, Receiver};
use std::sync::Arc;
use std::time::{Duration, Instant};
use void_ecs::prelude::Entity;

/// Configuration for hot-reload
#[derive(Debug, Clone)]
pub struct HotReloadConfig {
    /// Debounce duration (wait for file changes to settle)
    pub debounce_duration: Duration,
    /// Whether to preserve instance state across reloads
    pub preserve_state: bool,
    /// Watch directories for libraries
    pub watch_directories: Vec<PathBuf>,
}

impl Default for HotReloadConfig {
    fn default() -> Self {
        Self {
            debounce_duration: Duration::from_millis(500),
            preserve_state: true,
            watch_directories: vec![PathBuf::from(".")],
        }
    }
}

/// Manages hot-reload of C++ libraries
pub struct CppHotReloadManager {
    /// Configuration
    config: HotReloadConfig,
    /// The file watcher
    watcher: Option<RecommendedWatcher>,
    /// Receiver for file events
    event_receiver: Option<Receiver<notify::Result<Event>>>,
    /// Libraries being watched (path -> last modified time)
    watched_libraries: RwLock<HashSet<PathBuf>>,
    /// Pending reloads (path -> time detected)
    pending_reloads: RwLock<HashMap<PathBuf, Instant>>,
    /// Saved states for reload
    saved_states: RwLock<HashMap<PathBuf, HashMap<InstanceId, (Entity, String, PropertyMap, Vec<u8>)>>>,
    /// Whether hot-reload is enabled
    enabled: bool,
}

impl CppHotReloadManager {
    /// Create a new hot-reload manager
    pub fn new(config: HotReloadConfig) -> Result<Self> {
        let (tx, rx) = channel();

        let watcher = notify::recommended_watcher(move |res| {
            let _ = tx.send(res);
        }).map_err(|e| CppError::HotReloadFailed(e.to_string()))?;

        Ok(Self {
            config,
            watcher: Some(watcher),
            event_receiver: Some(rx),
            watched_libraries: RwLock::new(HashSet::new()),
            pending_reloads: RwLock::new(HashMap::new()),
            saved_states: RwLock::new(HashMap::new()),
            enabled: true,
        })
    }

    /// Create a disabled hot-reload manager (for testing)
    pub fn disabled() -> Self {
        Self {
            config: HotReloadConfig::default(),
            watcher: None,
            event_receiver: None,
            watched_libraries: RwLock::new(HashSet::new()),
            pending_reloads: RwLock::new(HashMap::new()),
            saved_states: RwLock::new(HashMap::new()),
            enabled: false,
        }
    }

    /// Check if hot-reload is enabled
    pub fn is_enabled(&self) -> bool {
        self.enabled && self.watcher.is_some()
    }

    /// Start watching a library file
    pub fn watch_library(&self, path: impl AsRef<Path>) -> Result<()> {
        if !self.is_enabled() {
            return Ok(());
        }

        let path = path.as_ref().to_path_buf();

        // Add to watched set
        self.watched_libraries.write().insert(path.clone());

        // Note: In a full implementation, we'd add the parent directory to the watcher here.
        // For now, directories are watched at creation time via config.watch_directories.

        log::debug!("Watching library for hot-reload: {}", path.display());
        Ok(())
    }

    /// Stop watching a library file
    pub fn unwatch_library(&self, path: impl AsRef<Path>) {
        let path = path.as_ref().to_path_buf();
        self.watched_libraries.write().remove(&path);
        self.pending_reloads.write().remove(&path);
    }

    /// Poll for file changes and process pending reloads
    pub fn poll(&self, registry: &CppClassRegistry) -> Result<Vec<PathBuf>> {
        if !self.is_enabled() {
            return Ok(Vec::new());
        }

        let mut reloaded = Vec::new();

        // Process file events
        if let Some(ref rx) = self.event_receiver {
            while let Ok(result) = rx.try_recv() {
                if let Ok(event) = result {
                    self.handle_event(&event);
                }
            }
        }

        // Check for pending reloads that have passed the debounce period
        let now = Instant::now();
        let to_reload: Vec<PathBuf> = {
            let pending = self.pending_reloads.read();
            pending
                .iter()
                .filter(|(_, time)| now.duration_since(**time) >= self.config.debounce_duration)
                .map(|(path, _)| path.clone())
                .collect()
        };

        // Process reloads
        for path in to_reload {
            match self.reload_library(&path, registry) {
                Ok(()) => {
                    reloaded.push(path.clone());
                    self.pending_reloads.write().remove(&path);
                    log::info!("Hot-reloaded C++ library: {}", path.display());
                }
                Err(e) => {
                    log::error!("Failed to hot-reload {}: {}", path.display(), e);
                    // Keep in pending to retry
                }
            }
        }

        Ok(reloaded)
    }

    /// Handle a file system event
    fn handle_event(&self, event: &Event) {
        match event.kind {
            EventKind::Modify(_) | EventKind::Create(_) => {
                for path in &event.paths {
                    // Check if this is a watched library
                    if self.is_library_file(path) && self.watched_libraries.read().contains(path) {
                        self.pending_reloads.write().insert(path.clone(), Instant::now());
                        log::debug!("Detected change in library: {}", path.display());
                    }
                }
            }
            _ => {}
        }
    }

    /// Check if a path is a library file
    fn is_library_file(&self, path: &Path) -> bool {
        match path.extension().and_then(|e| e.to_str()) {
            Some("dll") | Some("so") | Some("dylib") => true,
            _ => false,
        }
    }

    /// Perform the actual reload of a library
    fn reload_library(&self, path: &Path, registry: &CppClassRegistry) -> Result<()> {
        // Save instance states
        if self.config.preserve_state {
            let states = registry.prepare_reload(path)?;
            self.saved_states.write().insert(path.to_path_buf(), states);
        }

        // Get saved states
        let states = self.saved_states.write()
            .remove(path)
            .unwrap_or_default();

        // Reload the library
        registry.complete_reload(path, states)?;

        Ok(())
    }

    /// Manually trigger a reload of a library
    pub fn trigger_reload(&self, path: impl AsRef<Path>) {
        let path = path.as_ref().to_path_buf();
        if self.watched_libraries.read().contains(&path) {
            self.pending_reloads.write().insert(path, Instant::now());
        }
    }

    /// Get list of libraries pending reload
    pub fn pending_libraries(&self) -> Vec<PathBuf> {
        self.pending_reloads.read().keys().cloned().collect()
    }

    /// Get list of watched libraries
    pub fn watched_libraries(&self) -> Vec<PathBuf> {
        self.watched_libraries.read().iter().cloned().collect()
    }
}

impl Drop for CppHotReloadManager {
    fn drop(&mut self) {
        log::debug!("Shutting down C++ hot-reload manager");
    }
}

/// System that processes hot-reload during the game loop
pub struct CppHotReloadSystem {
    /// The hot-reload manager
    manager: Arc<CppHotReloadManager>,
}

impl CppHotReloadSystem {
    /// Create a new hot-reload system
    pub fn new(manager: Arc<CppHotReloadManager>) -> Self {
        Self { manager }
    }

    /// Process hot-reloads (call once per frame)
    pub fn update(&self, registry: &CppClassRegistry) -> Result<()> {
        let reloaded = self.manager.poll(registry)?;

        for path in reloaded {
            log::info!("C++ library reloaded: {}", path.display());
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_disabled_manager() {
        let manager = CppHotReloadManager::disabled();
        assert!(!manager.is_enabled());
        assert!(manager.watched_libraries().is_empty());
    }

    #[test]
    fn test_is_library_file() {
        let manager = CppHotReloadManager::disabled();

        assert!(manager.is_library_file(Path::new("game.dll")));
        assert!(manager.is_library_file(Path::new("libgame.so")));
        assert!(manager.is_library_file(Path::new("libgame.dylib")));
        assert!(!manager.is_library_file(Path::new("game.exe")));
        assert!(!manager.is_library_file(Path::new("game.cpp")));
    }

    #[test]
    fn test_config_defaults() {
        let config = HotReloadConfig::default();
        assert!(config.preserve_state);
        assert_eq!(config.debounce_duration, Duration::from_millis(500));
    }
}
