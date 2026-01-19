//! Asset loaders for various file formats

mod shader;
mod texture;
pub mod mesh;
pub mod mesh_validator;
mod scene;
pub mod gltf;

pub use shader::{ShaderAsset, ShaderLoader, ShaderStage, ShaderEntryPoint};
pub use texture::{TextureAsset, TextureLoader};
pub use mesh::{
    MeshAsset, MeshLoader, MeshPrimitive, SubMesh, Bounds, Vertex,
    SkinnedVertex, PrimitiveTopology, Skeleton, MorphTarget, LegacyMeshAsset,
};
pub use mesh_validator::{
    MeshValidator, MeshValidationError, ValidationOptions, ValidationResult, MeshStats,
    compute_tangents,
};
pub use scene::{SceneAsset, SceneLoader, EntityDef, TransformDef, ComponentDef, SceneAssets};
pub use gltf::{
    GltfAsset, GltfLoader, GltfMesh, GltfPrimitive, GltfNode,
    PbrVertex, PbrMaterial, AlphaMode, compute_tangents as compute_pbr_tangents,
};
