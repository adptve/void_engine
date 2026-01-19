//! Spatial Acceleration Structures
//!
//! This module provides spatial data structures for efficient culling and queries:
//! - BVH (Bounding Volume Hierarchy) for spatial acceleration
//! - Frustum culling support
//! - Ray queries
//! - AABB queries
//!
//! # Example
//!
//! ```ignore
//! use void_render::spatial::{BVH, SpatialIndexSystem, SpatialQueryConfig};
//! use void_math::{AABB, Vec3, Ray};
//! use void_ecs::Entity;
//!
//! // Build BVH from entities
//! let mut bvh = BVH::new();
//! bvh.build(&[
//!     (entity1, AABB::new(Vec3::ZERO, Vec3::ONE)),
//!     (entity2, AABB::new(Vec3::splat(5.0), Vec3::splat(6.0))),
//! ]);
//!
//! // Query with AABB
//! let hits = bvh.query_aabb(&AABB::new(Vec3::ZERO, Vec3::splat(2.0)));
//!
//! // Query with ray
//! let ray = Ray::new(Vec3::new(-5.0, 0.5, 0.5), Vec3::X);
//! let ray_hits = bvh.query_ray(&ray, 100.0);
//! ```

mod bvh;

pub use bvh::*;
