//! Scene serialization and management.
//!
//! Handles saving and loading editor scenes in TOML format.

mod serializer;

pub use serializer::{SceneSerializer, SceneData, EntityData, TransformData, SceneError};
