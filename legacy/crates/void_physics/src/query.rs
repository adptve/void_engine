//! Physics queries (raycasting, shape casting)

use crate::collider::{ColliderHandle, ColliderShape};
use crate::layers::CollisionGroups;
use rapier3d::prelude as rapier;
use rapier3d::na::{UnitQuaternion, Quaternion};
use rapier3d::parry::query::ShapeCastOptions as RapierShapeCastOptions;

/// Result of a raycast query
#[derive(Debug, Clone, Copy)]
pub struct RaycastHit {
    /// The collider that was hit
    pub collider: ColliderHandle,
    /// Hit point in world space
    pub point: [f32; 3],
    /// Surface normal at hit point
    pub normal: [f32; 3],
    /// Distance from ray origin
    pub distance: f32,
    /// User data from the collider
    pub user_data: u128,
}

/// Result of a shape cast query
#[derive(Debug, Clone, Copy)]
pub struct ShapeCastHit {
    /// The collider that was hit
    pub collider: ColliderHandle,
    /// Hit point in world space
    pub point: [f32; 3],
    /// Surface normal at hit point
    pub normal: [f32; 3],
    /// Time of impact (0-1 along the cast path)
    pub toi: f32,
    /// User data from the collider
    pub user_data: u128,
}

/// Options for raycast queries
#[derive(Debug, Clone)]
pub struct RaycastOptions {
    /// Maximum distance for the ray
    pub max_distance: f32,
    /// Only hit solid colliders (not sensors)
    pub solid_only: bool,
    /// Collision groups filter
    pub filter: CollisionGroups,
    /// Colliders to exclude
    pub exclude: Vec<ColliderHandle>,
}

impl Default for RaycastOptions {
    fn default() -> Self {
        Self {
            max_distance: f32::MAX,
            solid_only: true,
            filter: CollisionGroups::ALL,
            exclude: Vec::new(),
        }
    }
}

impl RaycastOptions {
    /// Set maximum distance
    pub fn with_max_distance(mut self, distance: f32) -> Self {
        self.max_distance = distance;
        self
    }

    /// Set whether to hit sensors
    pub fn with_sensors(mut self, include_sensors: bool) -> Self {
        self.solid_only = !include_sensors;
        self
    }

    /// Set collision filter
    pub fn with_filter(mut self, filter: CollisionGroups) -> Self {
        self.filter = filter;
        self
    }

    /// Add a collider to exclude
    pub fn exclude(mut self, collider: ColliderHandle) -> Self {
        self.exclude.push(collider);
        self
    }
}

/// Options for shape cast queries
#[derive(Debug, Clone)]
pub struct ShapeCastOptions {
    /// Maximum distance for the cast
    pub max_toi: f32,
    /// Only hit solid colliders
    pub solid_only: bool,
    /// Collision groups filter
    pub filter: CollisionGroups,
    /// Colliders to exclude
    pub exclude: Vec<ColliderHandle>,
}

impl Default for ShapeCastOptions {
    fn default() -> Self {
        Self {
            max_toi: 1.0,
            solid_only: true,
            filter: CollisionGroups::ALL,
            exclude: Vec::new(),
        }
    }
}

/// Query interface for physics world
pub struct PhysicsQuery<'a> {
    pub(crate) query_pipeline: &'a rapier::QueryPipeline,
    pub(crate) colliders: &'a rapier::ColliderSet,
    pub(crate) bodies: &'a rapier::RigidBodySet,
}

impl<'a> PhysicsQuery<'a> {
    /// Cast a ray and get the first hit
    pub fn raycast(
        &self,
        origin: [f32; 3],
        direction: [f32; 3],
        options: &RaycastOptions,
    ) -> Option<RaycastHit> {
        let ray = rapier::Ray::new(
            rapier::Point::new(origin[0], origin[1], origin[2]),
            rapier::Vector::new(direction[0], direction[1], direction[2]),
        );

        let mut filter = rapier::QueryFilter::new()
            .groups(rapier::InteractionGroups::new(
                rapier::Group::from_bits_truncate(options.filter.memberships),
                rapier::Group::from_bits_truncate(options.filter.filter),
            ));

        if options.solid_only {
            filter = filter.exclude_sensors();
        }

        // Exclude specified colliders
        for excluded in &options.exclude {
            filter = filter.exclude_collider(excluded.0);
        }

        self.query_pipeline
            .cast_ray(
                self.bodies,
                self.colliders,
                &ray,
                options.max_distance,
                options.solid_only,
                filter,
            )
            .map(|(handle, toi)| {
                let point = ray.point_at(toi);
                let collider = self.colliders.get(handle).unwrap();

                // Get normal at hit point (approximate using direction)
                let dir = rapier::Vector::new(direction[0], direction[1], direction[2]);
                let normal = -dir.normalize();

                RaycastHit {
                    collider: ColliderHandle(handle),
                    point: [point.x, point.y, point.z],
                    normal: [normal.x, normal.y, normal.z],
                    distance: toi,
                    user_data: collider.user_data,
                }
            })
    }

    /// Cast a ray and get all hits
    pub fn raycast_all(
        &self,
        origin: [f32; 3],
        direction: [f32; 3],
        options: &RaycastOptions,
    ) -> Vec<RaycastHit> {
        let ray = rapier::Ray::new(
            rapier::Point::new(origin[0], origin[1], origin[2]),
            rapier::Vector::new(direction[0], direction[1], direction[2]),
        );

        let filter = rapier::QueryFilter::new()
            .groups(rapier::InteractionGroups::new(
                rapier::Group::from_bits_truncate(options.filter.memberships),
                rapier::Group::from_bits_truncate(options.filter.filter),
            ));

        let filter = if options.solid_only {
            filter.exclude_sensors()
        } else {
            filter
        };

        let mut hits = Vec::new();

        self.query_pipeline.intersections_with_ray(
            self.bodies,
            self.colliders,
            &ray,
            options.max_distance,
            options.solid_only,
            filter,
            |handle, intersection| {
                let point = ray.point_at(intersection.time_of_impact);
                let collider = self.colliders.get(handle).unwrap();

                hits.push(RaycastHit {
                    collider: ColliderHandle(handle),
                    point: [point.x, point.y, point.z],
                    normal: [
                        intersection.normal.x,
                        intersection.normal.y,
                        intersection.normal.z,
                    ],
                    distance: intersection.time_of_impact,
                    user_data: collider.user_data,
                });

                true // Continue searching
            },
        );

        // Sort by distance
        hits.sort_by(|a, b| a.distance.partial_cmp(&b.distance).unwrap());
        hits
    }

    /// Check if a point is inside any collider
    pub fn point_inside(&self, point: [f32; 3], filter: CollisionGroups) -> Option<ColliderHandle> {
        let point = rapier::Point::new(point[0], point[1], point[2]);
        let filter = rapier::QueryFilter::new().groups(rapier::InteractionGroups::new(
            rapier::Group::from_bits_truncate(filter.memberships),
            rapier::Group::from_bits_truncate(filter.filter),
        ));

        let mut result = None;
        self.query_pipeline.intersections_with_point(
            self.bodies,
            self.colliders,
            &point,
            filter,
            |handle| {
                result = Some(ColliderHandle(handle));
                false // Stop at first hit
            },
        );
        result
    }

    /// Get all colliders that overlap with a shape at a position
    pub fn overlap_shape(
        &self,
        shape: &ColliderShape,
        position: [f32; 3],
        rotation: [f32; 4],
        filter: CollisionGroups,
    ) -> Vec<ColliderHandle> {
        let rapier_shape = shape.to_rapier();
        let pos = rapier::Isometry::from_parts(
            rapier::Translation::new(position[0], position[1], position[2]),
            UnitQuaternion::from_quaternion(Quaternion::new(
                rotation[3],
                rotation[0],
                rotation[1],
                rotation[2],
            )),
        );

        let filter = rapier::QueryFilter::new().groups(rapier::InteractionGroups::new(
            rapier::Group::from_bits_truncate(filter.memberships),
            rapier::Group::from_bits_truncate(filter.filter),
        ));

        let mut results = Vec::new();

        self.query_pipeline.intersections_with_shape(
            self.bodies,
            self.colliders,
            &pos,
            rapier_shape.as_ref(),
            filter,
            |handle| {
                results.push(ColliderHandle(handle));
                true // Continue
            },
        );

        results
    }

    /// Cast a shape and get the first hit
    pub fn shapecast(
        &self,
        shape: &ColliderShape,
        origin: [f32; 3],
        rotation: [f32; 4],
        direction: [f32; 3],
        options: &ShapeCastOptions,
    ) -> Option<ShapeCastHit> {
        let rapier_shape = shape.to_rapier();
        let pos = rapier::Isometry::from_parts(
            rapier::Translation::new(origin[0], origin[1], origin[2]),
            UnitQuaternion::from_quaternion(Quaternion::new(
                rotation[3],
                rotation[0],
                rotation[1],
                rotation[2],
            )),
        );
        let vel = rapier::Vector::new(direction[0], direction[1], direction[2]);

        let filter = rapier::QueryFilter::new().groups(rapier::InteractionGroups::new(
            rapier::Group::from_bits_truncate(options.filter.memberships),
            rapier::Group::from_bits_truncate(options.filter.filter),
        ));

        let filter = if options.solid_only {
            filter.exclude_sensors()
        } else {
            filter
        };

        let shape_cast_options = RapierShapeCastOptions {
            max_time_of_impact: options.max_toi,
            stop_at_penetration: true,
            ..Default::default()
        };

        self.query_pipeline
            .cast_shape(
                self.bodies,
                self.colliders,
                &pos,
                &vel,
                rapier_shape.as_ref(),
                shape_cast_options,
                filter,
            )
            .map(|(handle, hit)| {
                let collider = self.colliders.get(handle).unwrap();
                let point = pos.translation.vector + vel * hit.time_of_impact;

                ShapeCastHit {
                    collider: ColliderHandle(handle),
                    point: [point.x, point.y, point.z],
                    normal: [hit.normal1.x, hit.normal1.y, hit.normal1.z],
                    toi: hit.time_of_impact,
                    user_data: collider.user_data,
                }
            })
    }
}
