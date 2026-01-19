//! Error types for the physics system

use thiserror::Error;

/// Physics system errors
#[derive(Debug, Error)]
pub enum PhysicsError {
    /// Rigid body not found
    #[error("Rigid body not found: {0:?}")]
    BodyNotFound(crate::body::RigidBodyHandle),

    /// Collider not found
    #[error("Collider not found: {0:?}")]
    ColliderNotFound(crate::collider::ColliderHandle),

    /// Invalid configuration
    #[error("Invalid physics configuration: {0}")]
    InvalidConfig(String),

    /// Entity not found in physics world
    #[error("Entity has no physics body")]
    EntityNotInPhysics,

    /// Invalid collision layer
    #[error("Invalid collision layer: {0}")]
    InvalidLayer(String),

    /// Shape creation failed
    #[error("Failed to create collision shape: {0}")]
    ShapeCreationFailed(String),
}

/// Result type for physics operations
pub type Result<T> = std::result::Result<T, PhysicsError>;
