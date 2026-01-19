//! # void_core - Void Engine Core
//!
//! Zero-dependency core primitives providing the foundational abstractions
//! for the entire engine. Everything is designed to be:
//! - **Dynamic**: Runtime registration and hot-reload support
//! - **Extensible**: Trait-based design for maximum flexibility
//! - **Lightweight**: Zero external dependencies
//!
//! ## Philosophy
//! "Everything is a Plugin" - The engine has no hardcoded systems.
//! All functionality is provided through the plugin system.

#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(not(feature = "std"))]
extern crate alloc;

#[cfg(feature = "std")]
extern crate std as alloc;

pub mod plugin;
pub mod handle;
pub mod type_registry;
pub mod version;
pub mod error;
pub mod id;

#[cfg(feature = "hot-reload")]
pub mod hot_reload;

pub use plugin::*;
pub use handle::*;
pub use type_registry::*;
pub use version::*;
pub use error::*;
pub use id::*;

/// Re-export commonly used types
pub mod prelude {
    pub use crate::plugin::{Plugin, PluginId, PluginContext, PluginState};
    pub use crate::handle::{Handle, HandleAllocator};
    pub use crate::type_registry::{TypeRegistry, TypeInfo, DynType};
    pub use crate::version::Version;
    pub use crate::error::{Error, Result};
    pub use crate::id::{Id, IdGenerator};

    #[cfg(feature = "hot-reload")]
    pub use crate::hot_reload::{HotReloadable, HotReloadManager};
}
