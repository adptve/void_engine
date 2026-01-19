//! # void_asset - Hot-Reloadable Asset System
//!
//! Dynamic asset loading with:
//! - Pluggable loaders (register at runtime)
//! - Hot-reload support
//! - Reference-counted handles
//! - Dependency tracking
//!
//! ## Example
//!
//! ```ignore
//! use void_asset::prelude::*;
//!
//! // Create asset server
//! let server = AssetServer::default_config();
//!
//! // Register a custom loader
//! server.register_loader(MyTextureLoader);
//!
//! // Load an asset (async)
//! let handle: Handle<Texture> = server.load("textures/player.png");
//!
//! // Check if loaded
//! if handle.is_loaded() {
//!     // Use the asset
//! }
//!
//! // Process loads each frame
//! server.process(|path| std::fs::read(path).ok());
//! ```

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

pub mod handle;
pub mod loader;
pub mod storage;
pub mod server;

pub use handle::{AssetId, Handle, WeakHandle, LoadState, HandleData, UntypedHandle};
pub use loader::{AssetLoader, LoadContext, LoadError, LoadResult, LoaderRegistry, ErasedLoader};
pub use storage::AssetStorage;
pub use server::{AssetServer, AssetServerConfig, AssetEvent, AssetMeta, AssetPath};

#[cfg(feature = "hot-reload")]
pub use server::FileWatcher;

/// Prelude - commonly used types
pub mod prelude {
    pub use crate::handle::{AssetId, Handle, WeakHandle, LoadState};
    pub use crate::loader::{AssetLoader, LoadContext, LoadError, LoadResult};
    pub use crate::storage::AssetStorage;
    pub use crate::server::{AssetServer, AssetServerConfig, AssetEvent};
}

/// Built-in asset loaders for common types
pub mod loaders {
    use super::*;
    use alloc::string::String;
    use alloc::string::ToString;
    use alloc::vec::Vec;

    /// Raw bytes asset
    pub struct Bytes(pub Vec<u8>);

    /// Text asset
    pub struct Text(pub String);

    /// Bytes loader
    pub struct BytesLoader;

    impl AssetLoader for BytesLoader {
        type Asset = Bytes;

        fn extensions(&self) -> &[&str] {
            &["bin", "dat"]
        }

        fn load(&self, ctx: &mut LoadContext) -> LoadResult<Self::Asset> {
            Ok(Bytes(ctx.data.to_vec()))
        }
    }

    /// Text loader
    pub struct TextLoader;

    impl AssetLoader for TextLoader {
        type Asset = Text;

        fn extensions(&self) -> &[&str] {
            &["txt", "text", "md", "json", "toml", "yaml", "yml", "xml"]
        }

        fn load(&self, ctx: &mut LoadContext) -> LoadResult<Self::Asset> {
            let text = ctx.read_string()?;
            Ok(Text(text.to_string()))
        }
    }
}

/// Asset reference for embedding in components
#[derive(Clone)]
pub struct AssetRef<T: Send + Sync + 'static> {
    handle: Option<Handle<T>>,
    path: Option<alloc::string::String>,
}

impl<T: Send + Sync + 'static> AssetRef<T> {
    /// Create an empty asset reference
    pub fn empty() -> Self {
        Self {
            handle: None,
            path: None,
        }
    }

    /// Create from a path (not yet loaded)
    pub fn from_path(path: impl Into<alloc::string::String>) -> Self {
        Self {
            handle: None,
            path: Some(path.into()),
        }
    }

    /// Create from a handle
    pub fn from_handle(handle: Handle<T>) -> Self {
        Self {
            handle: Some(handle),
            path: None,
        }
    }

    /// Get the path
    pub fn path(&self) -> Option<&str> {
        self.path.as_deref()
    }

    /// Get the handle
    pub fn handle(&self) -> Option<&Handle<T>> {
        self.handle.as_ref()
    }

    /// Set the handle
    pub fn set_handle(&mut self, handle: Handle<T>) {
        self.handle = Some(handle);
    }

    /// Check if loaded
    pub fn is_loaded(&self) -> bool {
        self.handle.as_ref().map(|h| h.is_loaded()).unwrap_or(false)
    }

    /// Load using an asset server
    pub fn load(&mut self, server: &AssetServer) -> Option<&Handle<T>> {
        if self.handle.is_none() {
            if let Some(path) = &self.path {
                self.handle = Some(server.load(path.clone()));
            }
        }
        self.handle.as_ref()
    }
}

impl<T: Send + Sync + 'static> Default for AssetRef<T> {
    fn default() -> Self {
        Self::empty()
    }
}

impl<T: Send + Sync + 'static> core::fmt::Debug for AssetRef<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("AssetRef")
            .field("path", &self.path)
            .field("loaded", &self.is_loaded())
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bytes_loader() {
        let loader = loaders::BytesLoader;

        let data = b"test data";
        let mut ctx = LoadContext::new("test.bin", data, AssetId::new(1));

        let result = loader.load(&mut ctx);
        assert!(result.is_ok());

        let bytes = result.unwrap();
        assert_eq!(bytes.0, b"test data");
    }

    #[test]
    fn test_text_loader() {
        let loader = loaders::TextLoader;

        let data = b"Hello, World!";
        let mut ctx = LoadContext::new("test.txt", data, AssetId::new(1));

        let result = loader.load(&mut ctx);
        assert!(result.is_ok());

        let text = result.unwrap();
        assert_eq!(text.0, "Hello, World!");
    }

    #[test]
    fn test_asset_ref() {
        let asset_ref: AssetRef<loaders::Text> = AssetRef::from_path("test.txt");
        assert!(!asset_ref.is_loaded());
        assert_eq!(asset_ref.path(), Some("test.txt"));
    }
}
