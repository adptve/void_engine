//! Asset browser panel.
//!
//! File-based asset management with filtering and preview.

use std::path::PathBuf;
use std::time::SystemTime;

/// Type of asset based on file extension.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AssetType {
    Scene,
    Mesh,
    Texture,
    Shader,
    Audio,
    Script,
    Unknown,
}

impl AssetType {
    /// Determine asset type from file extension.
    pub fn from_extension(ext: &str) -> Self {
        match ext.to_lowercase().as_str() {
            // Scene formats
            "scene.json" | "toml" | "ron" => AssetType::Scene,

            // Mesh formats
            "obj" | "gltf" | "glb" | "fbx" | "dae" => AssetType::Mesh,

            // Texture formats
            "png" | "jpg" | "jpeg" | "bmp" | "tga" | "dds" | "hdr" | "exr" => AssetType::Texture,

            // Shader formats
            "wgsl" | "glsl" | "vert" | "frag" | "comp" | "spv" | "hlsl" => AssetType::Shader,

            // Audio formats
            "wav" | "mp3" | "ogg" | "flac" => AssetType::Audio,

            // Script formats
            "lua" | "js" | "wasm" => AssetType::Script,

            _ => AssetType::Unknown,
        }
    }

    /// Determine asset type from filename (handles compound extensions).
    pub fn from_filename(filename: &str) -> Self {
        // Check for compound extensions first
        if filename.ends_with(".scene.json") {
            return AssetType::Scene;
        }
        if filename.ends_with(".scene.toml") {
            return AssetType::Scene;
        }

        // Fall back to simple extension
        if let Some(ext) = std::path::Path::new(filename).extension() {
            Self::from_extension(ext.to_str().unwrap_or(""))
        } else {
            AssetType::Unknown
        }
    }

    /// Get icon for this asset type.
    pub fn icon(&self) -> &'static str {
        match self {
            AssetType::Scene => "[S]",
            AssetType::Mesh => "[M]",
            AssetType::Texture => "[T]",
            AssetType::Shader => "[#]",
            AssetType::Audio => "[A]",
            AssetType::Script => "[L]",
            AssetType::Unknown => "[?]",
        }
    }

    /// Get display name for this asset type.
    pub fn name(&self) -> &'static str {
        match self {
            AssetType::Scene => "Scene",
            AssetType::Mesh => "Mesh",
            AssetType::Texture => "Texture",
            AssetType::Shader => "Shader",
            AssetType::Audio => "Audio",
            AssetType::Script => "Script",
            AssetType::Unknown => "Unknown",
        }
    }
}

/// An asset file entry.
#[derive(Clone, Debug)]
pub struct AssetEntry {
    pub name: String,
    pub path: PathBuf,
    pub asset_type: AssetType,
    pub size_bytes: u64,
    pub modified: Option<SystemTime>,
    pub is_directory: bool,
}

impl AssetEntry {
    pub fn from_path(path: PathBuf) -> Option<Self> {
        let metadata = std::fs::metadata(&path).ok()?;
        let name = path.file_name()?.to_string_lossy().to_string();
        let is_directory = metadata.is_dir();

        let asset_type = if is_directory {
            AssetType::Unknown
        } else {
            AssetType::from_filename(&name)
        };

        Some(Self {
            name,
            path,
            asset_type,
            size_bytes: if is_directory { 0 } else { metadata.len() },
            modified: metadata.modified().ok(),
            is_directory,
        })
    }

    /// Format file size for display.
    pub fn size_display(&self) -> String {
        if self.is_directory {
            return String::new();
        }

        let bytes = self.size_bytes;
        if bytes < 1024 {
            format!("{} B", bytes)
        } else if bytes < 1024 * 1024 {
            format!("{:.1} KB", bytes as f64 / 1024.0)
        } else if bytes < 1024 * 1024 * 1024 {
            format!("{:.1} MB", bytes as f64 / (1024.0 * 1024.0))
        } else {
            format!("{:.1} GB", bytes as f64 / (1024.0 * 1024.0 * 1024.0))
        }
    }
}

/// Asset browser panel state.
#[derive(Debug)]
pub struct AssetBrowser {
    pub current_path: PathBuf,
    pub entries: Vec<AssetEntry>,
    pub selected_asset: Option<usize>,
    pub filter_text: String,

    // Type filters
    pub show_scenes: bool,
    pub show_meshes: bool,
    pub show_textures: bool,
    pub show_shaders: bool,
    pub show_audio: bool,
    pub show_scripts: bool,
    pub show_unknown: bool,
    pub show_directories: bool,

    // View options
    pub show_hidden: bool,
    pub sort_by: SortBy,
    pub sort_ascending: bool,

    // Navigation history
    history: Vec<PathBuf>,
    history_index: usize,
}

/// Sort order for asset list.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SortBy {
    Name,
    Type,
    Size,
    Modified,
}

impl Default for AssetBrowser {
    fn default() -> Self {
        Self {
            current_path: std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")),
            entries: Vec::new(),
            selected_asset: None,
            filter_text: String::new(),
            show_scenes: true,
            show_meshes: true,
            show_textures: true,
            show_shaders: true,
            show_audio: true,
            show_scripts: true,
            show_unknown: false,
            show_directories: true,
            show_hidden: false,
            sort_by: SortBy::Name,
            sort_ascending: true,
            history: Vec::new(),
            history_index: 0,
        }
    }
}

impl AssetBrowser {
    pub fn new() -> Self {
        Self::default()
    }

    /// Refresh the file list for current directory.
    pub fn refresh(&mut self) {
        self.entries.clear();
        self.selected_asset = None;

        if let Ok(read_dir) = std::fs::read_dir(&self.current_path) {
            for entry in read_dir.flatten() {
                let path = entry.path();

                // Skip hidden files unless enabled
                if !self.show_hidden {
                    if let Some(name) = path.file_name() {
                        if name.to_string_lossy().starts_with('.') {
                            continue;
                        }
                    }
                }

                if let Some(asset_entry) = AssetEntry::from_path(path) {
                    self.entries.push(asset_entry);
                }
            }
        }

        self.sort_entries();
    }

    /// Sort entries based on current settings.
    fn sort_entries(&mut self) {
        // Directories first, then by selected criteria
        self.entries.sort_by(|a, b| {
            // Directories always first
            match (a.is_directory, b.is_directory) {
                (true, false) => return std::cmp::Ordering::Less,
                (false, true) => return std::cmp::Ordering::Greater,
                _ => {}
            }

            let cmp = match self.sort_by {
                SortBy::Name => a.name.to_lowercase().cmp(&b.name.to_lowercase()),
                SortBy::Type => a.asset_type.name().cmp(b.asset_type.name()),
                SortBy::Size => a.size_bytes.cmp(&b.size_bytes),
                SortBy::Modified => a.modified.cmp(&b.modified),
            };

            if self.sort_ascending {
                cmp
            } else {
                cmp.reverse()
            }
        });
    }

    /// Navigate to a directory.
    pub fn navigate_to(&mut self, path: PathBuf) {
        if path.is_dir() {
            // Add to history
            if self.history_index < self.history.len() {
                self.history.truncate(self.history_index);
            }
            self.history.push(self.current_path.clone());
            self.history_index = self.history.len();

            self.current_path = path;
            self.refresh();
        }
    }

    /// Navigate to parent directory.
    pub fn navigate_up(&mut self) {
        if let Some(parent) = self.current_path.parent() {
            self.navigate_to(parent.to_path_buf());
        }
    }

    /// Go back in navigation history.
    pub fn navigate_back(&mut self) {
        if self.history_index > 0 {
            self.history_index -= 1;
            self.current_path = self.history[self.history_index].clone();
            self.refresh();
        }
    }

    /// Go forward in navigation history.
    pub fn navigate_forward(&mut self) {
        if self.history_index < self.history.len() - 1 {
            self.history_index += 1;
            self.current_path = self.history[self.history_index].clone();
            self.refresh();
        }
    }

    /// Check if back navigation is available.
    pub fn can_go_back(&self) -> bool {
        self.history_index > 0
    }

    /// Check if forward navigation is available.
    pub fn can_go_forward(&self) -> bool {
        self.history_index < self.history.len().saturating_sub(1)
    }

    /// Check if an entry should be shown based on current filters.
    pub fn should_show(&self, entry: &AssetEntry) -> bool {
        // Directory filter
        if entry.is_directory {
            return self.show_directories;
        }

        // Type filter
        let type_ok = match entry.asset_type {
            AssetType::Scene => self.show_scenes,
            AssetType::Mesh => self.show_meshes,
            AssetType::Texture => self.show_textures,
            AssetType::Shader => self.show_shaders,
            AssetType::Audio => self.show_audio,
            AssetType::Script => self.show_scripts,
            AssetType::Unknown => self.show_unknown,
        };

        // Text filter
        let filter_ok = self.filter_text.is_empty()
            || entry.name.to_lowercase().contains(&self.filter_text.to_lowercase());

        type_ok && filter_ok
    }

    /// Get filtered entries.
    pub fn filtered_entries(&self) -> impl Iterator<Item = (usize, &AssetEntry)> {
        self.entries
            .iter()
            .enumerate()
            .filter(|(_, e)| self.should_show(e))
    }

    /// Get selected entry.
    pub fn selected_entry(&self) -> Option<&AssetEntry> {
        self.selected_asset.and_then(|idx| self.entries.get(idx))
    }

    /// Select entry by index.
    pub fn select(&mut self, index: usize) {
        if index < self.entries.len() {
            self.selected_asset = Some(index);
        }
    }

    /// Clear selection.
    pub fn clear_selection(&mut self) {
        self.selected_asset = None;
    }
}
