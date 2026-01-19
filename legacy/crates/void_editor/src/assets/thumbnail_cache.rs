//! Thumbnail generation and caching for asset previews.

use std::collections::HashMap;
use std::path::PathBuf;
use crate::panels::AssetType;
use super::AssetGuid;

/// Size of generated thumbnails.
pub const THUMBNAIL_SIZE: u32 = 64;

/// A cached thumbnail.
#[derive(Clone, Debug)]
pub struct Thumbnail {
    /// RGBA pixel data
    pub data: Vec<u8>,
    /// Width in pixels
    pub width: u32,
    /// Height in pixels
    pub height: u32,
}

impl Thumbnail {
    /// Create a solid color thumbnail.
    pub fn solid_color(r: u8, g: u8, b: u8) -> Self {
        let size = THUMBNAIL_SIZE as usize;
        let mut data = Vec::with_capacity(size * size * 4);
        for _ in 0..size * size {
            data.push(r);
            data.push(g);
            data.push(b);
            data.push(255);
        }
        Self {
            data,
            width: THUMBNAIL_SIZE,
            height: THUMBNAIL_SIZE,
        }
    }

    /// Create a placeholder thumbnail for an asset type.
    pub fn placeholder(asset_type: AssetType) -> Self {
        match asset_type {
            AssetType::Texture => Self::checker_pattern(180, 180, 200, 140, 140, 160),
            AssetType::Mesh => Self::solid_color(100, 150, 200),
            AssetType::Scene => Self::solid_color(200, 150, 100),
            AssetType::Shader => Self::solid_color(150, 200, 100),
            AssetType::Audio => Self::solid_color(200, 100, 150),
            AssetType::Script => Self::solid_color(150, 100, 200),
            AssetType::Unknown => Self::solid_color(128, 128, 128),
        }
    }

    /// Create a checkerboard pattern thumbnail.
    pub fn checker_pattern(r1: u8, g1: u8, b1: u8, r2: u8, g2: u8, b2: u8) -> Self {
        let size = THUMBNAIL_SIZE as usize;
        let tile_size = 8;
        let mut data = Vec::with_capacity(size * size * 4);

        for y in 0..size {
            for x in 0..size {
                let checker = ((x / tile_size) + (y / tile_size)) % 2 == 0;
                if checker {
                    data.push(r1);
                    data.push(g1);
                    data.push(b1);
                } else {
                    data.push(r2);
                    data.push(g2);
                    data.push(b2);
                }
                data.push(255);
            }
        }

        Self {
            data,
            width: THUMBNAIL_SIZE,
            height: THUMBNAIL_SIZE,
        }
    }
}

/// State of thumbnail generation.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ThumbnailState {
    /// Not yet requested
    NotGenerated,
    /// Currently being generated
    Generating,
    /// Generated successfully
    Ready,
    /// Generation failed
    Failed,
}

/// Entry in the thumbnail cache.
struct CacheEntry {
    thumbnail: Option<Thumbnail>,
    state: ThumbnailState,
}

/// Cache for asset thumbnails.
pub struct ThumbnailCache {
    /// Cached thumbnails by asset GUID
    cache: HashMap<AssetGuid, CacheEntry>,
    /// Cache directory for persistent storage
    cache_dir: Option<PathBuf>,
    /// Queue of assets waiting for thumbnail generation
    pending: Vec<(AssetGuid, PathBuf, AssetType)>,
    /// Maximum cache size
    max_size: usize,
}

impl Default for ThumbnailCache {
    fn default() -> Self {
        Self::new()
    }
}

impl ThumbnailCache {
    pub fn new() -> Self {
        Self {
            cache: HashMap::new(),
            cache_dir: None,
            pending: Vec::new(),
            max_size: 1000,
        }
    }

    /// Set the cache directory for persistent storage.
    pub fn set_cache_dir(&mut self, dir: PathBuf) {
        if !dir.exists() {
            let _ = std::fs::create_dir_all(&dir);
        }
        self.cache_dir = Some(dir);
    }

    /// Get a thumbnail for an asset.
    pub fn get(&self, guid: AssetGuid) -> Option<&Thumbnail> {
        self.cache.get(&guid).and_then(|e| e.thumbnail.as_ref())
    }

    /// Get the state of a thumbnail.
    pub fn state(&self, guid: AssetGuid) -> ThumbnailState {
        self.cache.get(&guid).map(|e| e.state).unwrap_or(ThumbnailState::NotGenerated)
    }

    /// Request thumbnail generation for an asset.
    pub fn request(&mut self, guid: AssetGuid, path: PathBuf, asset_type: AssetType) {
        if !self.cache.contains_key(&guid) {
            self.cache.insert(guid, CacheEntry {
                thumbnail: None,
                state: ThumbnailState::Generating,
            });
            self.pending.push((guid, path, asset_type));
        }
    }

    /// Process pending thumbnail generation (call each frame).
    /// Returns number of thumbnails generated this frame.
    pub fn process_pending(&mut self, max_per_frame: usize) -> usize {
        let mut generated = 0;

        while generated < max_per_frame && !self.pending.is_empty() {
            let (guid, path, asset_type) = self.pending.remove(0);

            let thumbnail = match asset_type {
                AssetType::Texture => self.generate_texture_thumbnail(&path),
                _ => Some(Thumbnail::placeholder(asset_type)),
            };

            if let Some(entry) = self.cache.get_mut(&guid) {
                if thumbnail.is_some() {
                    entry.thumbnail = thumbnail;
                    entry.state = ThumbnailState::Ready;
                } else {
                    entry.thumbnail = Some(Thumbnail::placeholder(asset_type));
                    entry.state = ThumbnailState::Failed;
                }
            }

            generated += 1;
        }

        generated
    }

    /// Generate thumbnail for a texture file.
    fn generate_texture_thumbnail(&self, path: &PathBuf) -> Option<Thumbnail> {
        // Try to load and resize the image
        let data = std::fs::read(path).ok()?;

        // Try to decode as image
        let img = image::load_from_memory(&data).ok()?;

        // Resize to thumbnail size
        let resized = img.resize_exact(
            THUMBNAIL_SIZE,
            THUMBNAIL_SIZE,
            image::imageops::FilterType::Triangle,
        );

        // Convert to RGBA
        let rgba = resized.to_rgba8();

        Some(Thumbnail {
            data: rgba.into_raw(),
            width: THUMBNAIL_SIZE,
            height: THUMBNAIL_SIZE,
        })
    }

    /// Invalidate a thumbnail (e.g., when asset changes).
    pub fn invalidate(&mut self, guid: AssetGuid) {
        self.cache.remove(&guid);
    }

    /// Clear all cached thumbnails.
    pub fn clear(&mut self) {
        self.cache.clear();
        self.pending.clear();
    }

    /// Get number of cached thumbnails.
    pub fn cached_count(&self) -> usize {
        self.cache.len()
    }

    /// Get number of pending thumbnails.
    pub fn pending_count(&self) -> usize {
        self.pending.len()
    }

    /// Evict oldest entries if cache is too large.
    pub fn evict_if_needed(&mut self) {
        // Simple eviction: remove entries until under limit
        // A more sophisticated approach would use LRU
        while self.cache.len() > self.max_size {
            if let Some(key) = self.cache.keys().next().copied() {
                self.cache.remove(&key);
            } else {
                break;
            }
        }
    }
}
