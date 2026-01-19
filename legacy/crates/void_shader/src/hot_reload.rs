//! Hot-reload support for shaders
//!
//! Watches shader files for changes and triggers recompilation.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::Arc;
use std::time::{Duration, Instant};

use notify::{Config, Event, EventKind, RecommendedWatcher, RecursiveMode, Watcher};
use parking_lot::Mutex;

use crate::ShaderError;

/// Shader file change event
#[derive(Debug, Clone)]
pub enum ShaderFileEvent {
    /// File was created
    Created(PathBuf),
    /// File was modified
    Modified(PathBuf),
    /// File was deleted
    Deleted(PathBuf),
    /// File was renamed (old, new)
    Renamed(PathBuf, PathBuf),
}

/// Shader file watcher for hot-reload
pub struct ShaderWatcher {
    /// The underlying file watcher
    _watcher: RecommendedWatcher,
    /// Channel receiver for events
    receiver: Receiver<ShaderFileEvent>,
    /// Watched directories
    watched_dirs: Vec<PathBuf>,
    /// Debounce tracking
    debounce: Arc<Mutex<DebounceState>>,
}

/// Debounce state to prevent rapid-fire reloads
struct DebounceState {
    /// Last event time per file
    last_event: HashMap<PathBuf, Instant>,
    /// Debounce duration
    debounce_duration: Duration,
}

impl DebounceState {
    fn new(debounce_duration: Duration) -> Self {
        Self {
            last_event: HashMap::new(),
            debounce_duration,
        }
    }

    fn should_trigger(&mut self, path: &Path) -> bool {
        let now = Instant::now();
        if let Some(last) = self.last_event.get(path) {
            if now.duration_since(*last) < self.debounce_duration {
                return false;
            }
        }
        self.last_event.insert(path.to_path_buf(), now);
        true
    }
}

impl ShaderWatcher {
    /// Create a new shader watcher for the given directory
    pub fn new(shader_dir: &Path) -> Result<Self, ShaderError> {
        Self::with_debounce(shader_dir, Duration::from_millis(100))
    }

    /// Create a watcher with custom debounce duration
    pub fn with_debounce(shader_dir: &Path, debounce: Duration) -> Result<Self, ShaderError> {
        let (tx, rx) = mpsc::channel();
        let debounce_state = Arc::new(Mutex::new(DebounceState::new(debounce)));
        let debounce_clone = debounce_state.clone();

        let sender: Sender<ShaderFileEvent> = tx;

        let mut watcher = notify::recommended_watcher(move |result: Result<Event, notify::Error>| {
            if let Ok(event) = result {
                Self::handle_event(event, &sender, &debounce_clone);
            }
        })
        .map_err(|e| ShaderError::HotReloadError(format!("Failed to create watcher: {}", e)))?;

        // Watch the shader directory
        watcher
            .watch(shader_dir, RecursiveMode::Recursive)
            .map_err(|e| ShaderError::HotReloadError(format!("Failed to watch directory: {}", e)))?;

        log::info!("Started watching shader directory: {:?}", shader_dir);

        Ok(Self {
            _watcher: watcher,
            receiver: rx,
            watched_dirs: vec![shader_dir.to_path_buf()],
            debounce: debounce_state,
        })
    }

    /// Handle a notify event
    fn handle_event(event: Event, sender: &Sender<ShaderFileEvent>, debounce: &Arc<Mutex<DebounceState>>) {
        // Only care about .wgsl files
        let wgsl_paths: Vec<_> = event
            .paths
            .iter()
            .filter(|p| {
                p.extension()
                    .map(|ext| ext == "wgsl")
                    .unwrap_or(false)
            })
            .collect();

        if wgsl_paths.is_empty() {
            return;
        }

        let mut debounce = debounce.lock();

        match event.kind {
            EventKind::Create(_) => {
                for path in wgsl_paths {
                    if debounce.should_trigger(path) {
                        let _ = sender.send(ShaderFileEvent::Created(path.clone()));
                    }
                }
            }
            EventKind::Modify(_) => {
                for path in wgsl_paths {
                    if debounce.should_trigger(path) {
                        let _ = sender.send(ShaderFileEvent::Modified(path.clone()));
                    }
                }
            }
            EventKind::Remove(_) => {
                for path in wgsl_paths {
                    if debounce.should_trigger(path) {
                        let _ = sender.send(ShaderFileEvent::Deleted(path.clone()));
                    }
                }
            }
            _ => {}
        }
    }

    /// Watch an additional directory
    pub fn watch_dir(&mut self, dir: &Path) -> Result<(), ShaderError> {
        // Note: We can't add to the watcher after creation with this design
        // This would need to be refactored to store the watcher mutably
        Err(ShaderError::HotReloadError(
            "Cannot add directories after creation".to_string(),
        ))
    }

    /// Poll for file changes (non-blocking)
    pub fn poll_changes(&mut self) -> Vec<PathBuf> {
        let mut changed = Vec::new();
        while let Ok(event) = self.receiver.try_recv() {
            match event {
                ShaderFileEvent::Created(path) | ShaderFileEvent::Modified(path) => {
                    changed.push(path);
                }
                ShaderFileEvent::Deleted(path) => {
                    log::debug!("Shader deleted: {:?}", path);
                }
                ShaderFileEvent::Renamed(old, new) => {
                    log::debug!("Shader renamed: {:?} -> {:?}", old, new);
                    changed.push(new);
                }
            }
        }
        changed
    }

    /// Wait for the next file change (blocking)
    pub fn wait_for_change(&mut self) -> Option<PathBuf> {
        loop {
            match self.receiver.recv() {
                Ok(ShaderFileEvent::Created(path)) | Ok(ShaderFileEvent::Modified(path)) => {
                    return Some(path);
                }
                Ok(ShaderFileEvent::Deleted(_)) => continue,
                Ok(ShaderFileEvent::Renamed(_, new)) => return Some(new),
                Err(_) => return None,
            }
        }
    }

    /// Wait for the next change with timeout
    pub fn wait_for_change_timeout(&mut self, timeout: Duration) -> Option<PathBuf> {
        let deadline = Instant::now() + timeout;
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                return None;
            }

            match self.receiver.recv_timeout(remaining) {
                Ok(ShaderFileEvent::Created(path)) | Ok(ShaderFileEvent::Modified(path)) => {
                    return Some(path);
                }
                Ok(ShaderFileEvent::Deleted(_)) => continue,
                Ok(ShaderFileEvent::Renamed(_, new)) => return Some(new),
                Err(_) => return None,
            }
        }
    }

    /// Get watched directories
    pub fn watched_dirs(&self) -> &[PathBuf] {
        &self.watched_dirs
    }
}

/// Hot-reload manager that integrates with ShaderPipeline
pub struct HotReloadManager {
    /// The file watcher
    watcher: ShaderWatcher,
    /// Path to shader ID mapping
    path_to_shader: HashMap<PathBuf, crate::ShaderId>,
    /// Pending reloads
    pending_reloads: Vec<PathBuf>,
}

impl HotReloadManager {
    /// Create a new hot-reload manager
    pub fn new(shader_dir: &Path) -> Result<Self, ShaderError> {
        Ok(Self {
            watcher: ShaderWatcher::new(shader_dir)?,
            path_to_shader: HashMap::new(),
            pending_reloads: Vec::new(),
        })
    }

    /// Register a shader path
    pub fn register_path(&mut self, path: PathBuf, id: crate::ShaderId) {
        self.path_to_shader.insert(path, id);
    }

    /// Unregister a shader path
    pub fn unregister_path(&mut self, path: &Path) {
        self.path_to_shader.remove(path);
    }

    /// Poll for changes and return shader IDs that need reloading
    pub fn poll(&mut self) -> Vec<crate::ShaderId> {
        let changes = self.watcher.poll_changes();
        let mut to_reload = Vec::new();

        for path in changes {
            if let Some(id) = self.path_to_shader.get(&path) {
                to_reload.push(*id);
            } else {
                // Unknown shader - might be new
                self.pending_reloads.push(path);
            }
        }

        to_reload
    }

    /// Get paths of new shaders that were added
    pub fn take_new_shaders(&mut self) -> Vec<PathBuf> {
        std::mem::take(&mut self.pending_reloads)
    }

    /// Check if any shaders need reloading
    pub fn has_pending_changes(&self) -> bool {
        self.watcher.receiver.try_iter().count() > 0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::tempdir;

    #[test]
    fn test_debounce_state() {
        let mut state = DebounceState::new(Duration::from_millis(100));
        let path = PathBuf::from("/test/shader.wgsl");

        // First trigger should work
        assert!(state.should_trigger(&path));

        // Immediate second trigger should be blocked
        assert!(!state.should_trigger(&path));
    }

    #[test]
    fn test_watcher_creation() {
        let dir = tempdir().unwrap();
        let watcher = ShaderWatcher::new(dir.path());
        assert!(watcher.is_ok());
    }

    // Note: Full file watching tests require actual file system operations
    // and proper timing, so they're typically done as integration tests
}
