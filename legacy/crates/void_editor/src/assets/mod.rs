//! Asset management and importing.
//!
//! Handles asset discovery, indexing, thumbnails, and hot-reload.

mod asset_database;
mod thumbnail_cache;
mod prefab;

pub use asset_database::{AssetDatabase, AssetMetadata, AssetGuid};
pub use thumbnail_cache::{ThumbnailCache, Thumbnail, ThumbnailState, THUMBNAIL_SIZE};
pub use prefab::{Prefab, PrefabEntity, PrefabTransform, PrefabMesh, PrefabMetadata, PrefabLibrary, PrefabError};
