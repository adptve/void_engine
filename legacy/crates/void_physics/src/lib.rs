//! Void Physics - Rapier 3D Integration
//!
//! This crate provides physics simulation for the Void Engine using Rapier 3D.
//!
//! # Features
//!
//! - Rigid body dynamics (static, dynamic, kinematic)
//! - Collision detection with various shapes
//! - Trigger volumes (sensors)
//! - Raycasting and shape casting
//! - Collision layers and filtering
//! - Physics materials (friction, restitution)
//!
//! # Architecture
//!
//! ```text
//! ┌─────────────────────────────────────────────────┐
//! │                 PhysicsWorld                     │
//! │  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
//! │  │ RigidBodySet│  │ ColliderSet │  │ Queries │ │
//! │  └─────────────┘  └─────────────┘  └─────────┘ │
//! │  ┌─────────────────────────────────────────────┐│
//! │  │           PhysicsPipeline                   ││
//! │  │  (integration, collision, solver)          ││
//! │  └─────────────────────────────────────────────┘│
//! └─────────────────────────────────────────────────┘
//!                        │
//!         ┌──────────────┼──────────────┐
//!         ▼              ▼              ▼
//!    ┌─────────┐   ┌──────────┐   ┌──────────┐
//!    │RigidBody│   │ Collider │   │ Trigger  │
//!    │Component│   │Component │   │Component │
//!    └─────────┘   └──────────┘   └──────────┘
//! ```
//!
//! # Example
//!
//! ```ignore
//! use void_physics::prelude::*;
//!
//! // Create physics world
//! let mut physics = PhysicsWorld::new(PhysicsConfig::default());
//!
//! // Add a dynamic rigid body
//! let body_handle = physics.create_rigid_body(RigidBodyDesc {
//!     body_type: RigidBodyType::Dynamic,
//!     position: Vec3::new(0.0, 10.0, 0.0),
//!     ..Default::default()
//! });
//!
//! // Add a box collider
//! physics.create_collider(ColliderDesc {
//!     shape: ColliderShape::Box { half_extents: Vec3::new(0.5, 0.5, 0.5) },
//!     parent: Some(body_handle),
//!     ..Default::default()
//! });
//!
//! // Step simulation
//! physics.step(1.0 / 60.0);
//! ```

pub mod body;
pub mod collider;
pub mod config;
pub mod error;
pub mod events;
pub mod layers;
pub mod material;
pub mod query;
pub mod world;

pub mod prelude {
    //! Common imports for physics functionality
    pub use crate::body::{RigidBodyComponent, RigidBodyDesc, RigidBodyHandle, RigidBodyType};
    pub use crate::collider::{ColliderComponent, ColliderDesc, ColliderHandle, ColliderShape};
    pub use crate::config::PhysicsConfig;
    pub use crate::error::{PhysicsError, Result};
    pub use crate::events::{CollisionEvent, ContactData, PhysicsEventHandler};
    pub use crate::layers::{CollisionLayer, CollisionMatrix};
    pub use crate::material::PhysicsMaterial;
    pub use crate::query::{RaycastHit, ShapeCastHit};
    pub use crate::world::PhysicsWorld;
}

pub use prelude::*;
