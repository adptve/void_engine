//! File system watcher for hot-reload

use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};
use std::collections::HashMap;

#[cfg(feature = "file-watcher")]
use notify::{Event, EventKind, RecursiveMode, Watcher};

/// A file change event
#[derive(Debug, Clone)]
pub struct FileChange {
    /// Path to the changed file
    pub path: PathBuf,
    /// Type of change
    pub kind: FileChangeKind,
}

/// Type of file change
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FileChangeKind {
    /// File was created
    Created,
    /// File was modified
    Modified,
    /// File was deleted
    Deleted,
}

/// File watcher that monitors directories for changes
pub struct FileWatcher {
    #[cfg(feature = "file-watcher")]
    watcher: notify::RecommendedWatcher,
    #[cfg(feature = "file-watcher")]
    rx: crossbeam_channel::Receiver<notify::Result<Event>>,

    /// Debounce tracking - prevents duplicate events
    debounce: HashMap<PathBuf, Instant>,
    /// Debounce duration
    debounce_duration: Duration,
    /// Watched directories
    watch_dirs: Vec<PathBuf>,
}

impl FileWatcher {
    /// Create a new file watcher
    #[cfg(feature = "file-watcher")]
    pub fn new() -> Result<Self, String> {
        let (tx, rx) = crossbeam_channel::unbounded();

        let watcher = notify::recommended_watcher(move |res| {
            let _ = tx.send(res);
        })
        .map_err(|e| format!("Failed to create file watcher: {}", e))?;

        Ok(Self {
            watcher,
            rx,
            debounce: HashMap::new(),
            debounce_duration: Duration::from_millis(100),
            watch_dirs: Vec::new(),
        })
    }

    /// Create a new file watcher (no-op without file-watcher feature)
    #[cfg(not(feature = "file-watcher"))]
    pub fn new() -> Result<Self, String> {
        Ok(Self {
            debounce: HashMap::new(),
            debounce_duration: Duration::from_millis(100),
            watch_dirs: Vec::new(),
        })
    }

    /// Watch a directory for changes
    #[cfg(feature = "file-watcher")]
    pub fn watch(&mut self, dir: impl AsRef<Path>) -> Result<(), String> {
        let path = dir.as_ref().to_path_buf();

        if !path.exists() {
            // Try to create the directory
            std::fs::create_dir_all(&path)
                .map_err(|e| format!("Failed to create watch directory {:?}: {}", path, e))?;
        }

        self.watcher
            .watch(&path, RecursiveMode::Recursive)
            .map_err(|e| format!("Failed to watch {:?}: {}", path, e))?;

        log::info!("Watching directory: {:?}", path);
        self.watch_dirs.push(path);
        Ok(())
    }

    /// Watch a directory (no-op without file-watcher feature)
    #[cfg(not(feature = "file-watcher"))]
    pub fn watch(&mut self, dir: impl AsRef<Path>) -> Result<(), String> {
        self.watch_dirs.push(dir.as_ref().to_path_buf());
        Ok(())
    }

    /// Poll for file changes
    #[cfg(feature = "file-watcher")]
    pub fn poll(&mut self) -> Vec<FileChange> {
        let mut changes = Vec::new();
        let now = Instant::now();

        // Drain all pending events
        while let Ok(result) = self.rx.try_recv() {
            if let Ok(event) = result {
                for path in event.paths {
                    // Skip directories
                    if path.is_dir() {
                        continue;
                    }

                    // Skip non-asset files
                    if !Self::is_asset_file(&path) {
                        continue;
                    }

                    // Check debounce
                    if let Some(last) = self.debounce.get(&path) {
                        if now.duration_since(*last) < self.debounce_duration {
                            continue;
                        }
                    }

                    let kind = match event.kind {
                        EventKind::Create(_) => FileChangeKind::Created,
                        EventKind::Modify(_) => FileChangeKind::Modified,
                        EventKind::Remove(_) => FileChangeKind::Deleted,
                        _ => continue,
                    };

                    self.debounce.insert(path.clone(), now);
                    changes.push(FileChange { path, kind });
                }
            }
        }

        // Clean up old debounce entries
        self.debounce.retain(|_, time| now.duration_since(*time) < Duration::from_secs(5));

        changes
    }

    /// Poll for file changes (no-op without file-watcher feature)
    #[cfg(not(feature = "file-watcher"))]
    pub fn poll(&mut self) -> Vec<FileChange> {
        Vec::new()
    }

    /// Check if a file is a recognized asset type
    fn is_asset_file(path: &Path) -> bool {
        let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
        matches!(
            ext.to_lowercase().as_str(),
            "wgsl" | "glsl" | "spv" |           // Shaders
            "png" | "jpg" | "jpeg" | "bmp" |   // Textures
            "obj" | "gltf" | "glb" |           // Meshes
            "json"                              // Scenes, configs
        )
    }

    /// Get the list of watched directories
    pub fn watch_dirs(&self) -> &[PathBuf] {
        &self.watch_dirs
    }

    /// Set debounce duration
    pub fn set_debounce(&mut self, duration: Duration) {
        self.debounce_duration = duration;
    }
}

impl Default for FileWatcher {
    fn default() -> Self {
        Self::new().expect("Failed to create file watcher")
    }
}
