//! # void_cpp - C++ Native Scripting
//!
//! Provides C++ native code integration for high-performance game logic,
//! following the Unreal Engine model of C++ classes with Blueprint support.
//!
//! ## Overview
//!
//! C++ classes are compiled into dynamic libraries (DLL/SO/DYLIB) that can
//! be loaded at runtime. The system supports hot-reload: recompile the DLL
//! and the engine will automatically reload it, preserving object state.
//!
//! ## Architecture
//!
//! ```text
//! ┌─────────────────┐     ┌─────────────────┐
//! │   C++ Classes   │────▶│  Dynamic Lib    │
//! │  (GameLogic.cpp)│     │  (game.dll)     │
//! └─────────────────┘     └────────┬────────┘
//!                                  │
//!                                  ▼
//! ┌─────────────────┐     ┌─────────────────┐
//! │   CppLibrary    │◀────│   libloading    │
//! │   (Rust FFI)    │     │                 │
//! └────────┬────────┘     └─────────────────┘
//!          │
//!          ▼
//! ┌─────────────────┐     ┌─────────────────┐
//! │ CppClassRegistry│────▶│ CppClassInstance│
//! │ (manages libs)  │     │ (per entity)    │
//! └────────┬────────┘     └─────────────────┘
//!          │
//!          ▼
//! ┌─────────────────┐
//! │  CppComponent   │ ◀── ECS integration
//! │  (per entity)   │
//! └─────────────────┘
//! ```
//!
//! ## Example
//!
//! ```ignore
//! use void_cpp::{CppClassRegistry, CppLibrary};
//!
//! // Load a C++ library
//! let mut registry = CppClassRegistry::new();
//! registry.load_library("game_logic.dll")?;
//!
//! // Create an instance of a C++ class
//! let instance = registry.create_instance("PlayerController", entity_id)?;
//!
//! // Call lifecycle methods
//! instance.begin_play();
//! instance.tick(delta_time);
//! ```
//!
//! ## C++ Side
//!
//! Include `void_api.h` and implement your classes:
//!
//! ```cpp
//! #include "void_api.h"
//!
//! class PlayerController : public VoidActor {
//! public:
//!     void BeginPlay() override {
//!         // Initialization
//!     }
//!
//!     void Tick(float DeltaTime) override {
//!         // Per-frame logic
//!     }
//! };
//!
//! // Export the class
//! VOID_EXPORT_CLASS(PlayerController)
//! ```

mod error;
mod ffi;
mod library;
mod registry;
mod instance;
mod component;
mod properties;

#[cfg(feature = "hot-reload")]
mod hot_reload;

pub use error::{CppError, Result};
pub use ffi::*;
pub use library::{CppLibrary, LibraryInfo};
pub use registry::CppClassRegistry;
pub use instance::{CppClassInstance, InstanceId};
pub use component::CppClassComponent;
pub use properties::{CppProperty, CppPropertyValue, PropertyMap};

#[cfg(feature = "hot-reload")]
pub use hot_reload::CppHotReloadManager;

/// Re-export commonly used types
pub mod prelude {
    pub use crate::error::{CppError, Result};
    pub use crate::library::CppLibrary;
    pub use crate::registry::CppClassRegistry;
    pub use crate::instance::{CppClassInstance, InstanceId};
    pub use crate::component::CppClassComponent;
    pub use crate::properties::{CppProperty, CppPropertyValue, PropertyMap};

    #[cfg(feature = "hot-reload")]
    pub use crate::hot_reload::CppHotReloadManager;
}
