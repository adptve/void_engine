//! Physics world - main simulation container

use crate::body::{RigidBodyDesc, RigidBodyHandle};
use crate::collider::{ColliderDesc, ColliderHandle};
use crate::config::PhysicsConfig;
use crate::error::{PhysicsError, Result};
use crate::events::{CollisionEvent, CollisionEventType, ContactData, EventCollector};
use crate::query::PhysicsQuery;
use rapier3d::prelude as rapier;
use rapier3d::na::{UnitQuaternion, Quaternion};
use std::collections::HashMap;
use std::num::NonZeroUsize;

/// The main physics world containing all simulation state
pub struct PhysicsWorld {
    /// Configuration
    config: PhysicsConfig,

    /// Rapier physics pipeline
    pipeline: rapier::PhysicsPipeline,

    /// Gravity
    gravity: rapier::Vector<f32>,

    /// Integration parameters
    integration_params: rapier::IntegrationParameters,

    /// Island manager
    islands: rapier::IslandManager,

    /// Broad phase
    broad_phase: rapier::DefaultBroadPhase,

    /// Narrow phase
    narrow_phase: rapier::NarrowPhase,

    /// Impulse joint set
    impulse_joints: rapier::ImpulseJointSet,

    /// Multibody joint set
    multibody_joints: rapier::MultibodyJointSet,

    /// CCD solver
    ccd_solver: rapier::CCDSolver,

    /// Query pipeline
    query_pipeline: rapier::QueryPipeline,

    /// Rigid body set
    bodies: rapier::RigidBodySet,

    /// Collider set
    colliders: rapier::ColliderSet,

    /// Event collector
    events: EventCollector,

    /// Mapping from entity user_data to body handles
    entity_to_body: HashMap<u128, RigidBodyHandle>,

    /// Mapping from entity user_data to collider handles
    entity_to_collider: HashMap<u128, Vec<ColliderHandle>>,

    /// Accumulated time for fixed timestep
    accumulated_time: f32,
}

impl PhysicsWorld {
    /// Create a new physics world
    pub fn new(config: PhysicsConfig) -> Self {
        let gravity = rapier::Vector::new(config.gravity[0], config.gravity[1], config.gravity[2]);

        let mut integration_params = rapier::IntegrationParameters::default();
        integration_params.dt = config.timestep;
        integration_params.num_solver_iterations = NonZeroUsize::new(config.velocity_iterations).unwrap();

        Self {
            config,
            pipeline: rapier::PhysicsPipeline::new(),
            gravity,
            integration_params,
            islands: rapier::IslandManager::new(),
            broad_phase: rapier::DefaultBroadPhase::new(),
            narrow_phase: rapier::NarrowPhase::new(),
            impulse_joints: rapier::ImpulseJointSet::new(),
            multibody_joints: rapier::MultibodyJointSet::new(),
            ccd_solver: rapier::CCDSolver::new(),
            query_pipeline: rapier::QueryPipeline::new(),
            bodies: rapier::RigidBodySet::new(),
            colliders: rapier::ColliderSet::new(),
            events: EventCollector::new(),
            entity_to_body: HashMap::new(),
            entity_to_collider: HashMap::new(),
            accumulated_time: 0.0,
        }
    }

    /// Get the physics configuration
    pub fn config(&self) -> &PhysicsConfig {
        &self.config
    }

    /// Set gravity
    pub fn set_gravity(&mut self, x: f32, y: f32, z: f32) {
        self.gravity = rapier::Vector::new(x, y, z);
    }

    /// Get gravity
    pub fn gravity(&self) -> [f32; 3] {
        [self.gravity.x, self.gravity.y, self.gravity.z]
    }

    // ==================== Rigid Bodies ====================

    /// Create a rigid body
    pub fn create_rigid_body(&mut self, desc: RigidBodyDesc) -> RigidBodyHandle {
        let builder = desc.to_rapier_builder();
        let handle = self.bodies.insert(builder);
        RigidBodyHandle(handle)
    }

    /// Create a rigid body associated with an entity
    pub fn create_rigid_body_for_entity(&mut self, entity_id: u128, desc: RigidBodyDesc) -> RigidBodyHandle {
        let handle = self.create_rigid_body(desc);
        self.entity_to_body.insert(entity_id, handle);
        handle
    }

    /// Remove a rigid body
    pub fn remove_rigid_body(&mut self, handle: RigidBodyHandle) {
        self.bodies.remove(
            handle.0,
            &mut self.islands,
            &mut self.colliders,
            &mut self.impulse_joints,
            &mut self.multibody_joints,
            true, // Remove attached colliders
        );

        // Clean up entity mapping
        self.entity_to_body.retain(|_, h| *h != handle);
    }

    /// Get rigid body position
    pub fn get_body_position(&self, handle: RigidBodyHandle) -> Result<[f32; 3]> {
        self.bodies
            .get(handle.0)
            .map(|b| {
                let pos = b.translation();
                [pos.x, pos.y, pos.z]
            })
            .ok_or(PhysicsError::BodyNotFound(handle))
    }

    /// Set rigid body position
    pub fn set_body_position(&mut self, handle: RigidBodyHandle, x: f32, y: f32, z: f32) -> Result<()> {
        self.bodies
            .get_mut(handle.0)
            .map(|b| {
                if b.body_type() == rapier::RigidBodyType::KinematicPositionBased {
                    // For kinematic position-based: set next position preserving rotation
                    let current_rot = *b.rotation();
                    let new_pos = rapier::Isometry::from_parts(
                        rapier::Translation::new(x, y, z),
                        current_rot,
                    );
                    b.set_next_kinematic_position(new_pos);
                } else {
                    b.set_translation(rapier::Vector::new(x, y, z), true);
                }
            })
            .ok_or(PhysicsError::BodyNotFound(handle))
    }

    /// Get rigid body rotation (quaternion)
    pub fn get_body_rotation(&self, handle: RigidBodyHandle) -> Result<[f32; 4]> {
        self.bodies
            .get(handle.0)
            .map(|b| {
                let rot = b.rotation();
                [rot.i, rot.j, rot.k, rot.w]
            })
            .ok_or(PhysicsError::BodyNotFound(handle))
    }

    /// Set rigid body rotation
    pub fn set_body_rotation(&mut self, handle: RigidBodyHandle, x: f32, y: f32, z: f32, w: f32) -> Result<()> {
        self.bodies
            .get_mut(handle.0)
            .map(|b| {
                b.set_rotation(
                    UnitQuaternion::from_quaternion(Quaternion::new(w, x, y, z)),
                    true,
                );
            })
            .ok_or(PhysicsError::BodyNotFound(handle))
    }

    /// Get rigid body linear velocity
    pub fn get_body_linear_velocity(&self, handle: RigidBodyHandle) -> Result<[f32; 3]> {
        self.bodies
            .get(handle.0)
            .map(|b| {
                let vel = b.linvel();
                [vel.x, vel.y, vel.z]
            })
            .ok_or(PhysicsError::BodyNotFound(handle))
    }

    /// Set rigid body linear velocity
    pub fn set_body_linear_velocity(&mut self, handle: RigidBodyHandle, x: f32, y: f32, z: f32) -> Result<()> {
        self.bodies
            .get_mut(handle.0)
            .map(|b| {
                b.set_linvel(rapier::Vector::new(x, y, z), true);
            })
            .ok_or(PhysicsError::BodyNotFound(handle))
    }

    /// Apply a force to a rigid body
    pub fn apply_force(&mut self, handle: RigidBodyHandle, force: [f32; 3]) -> Result<()> {
        self.bodies
            .get_mut(handle.0)
            .map(|b| {
                b.add_force(rapier::Vector::new(force[0], force[1], force[2]), true);
            })
            .ok_or(PhysicsError::BodyNotFound(handle))
    }

    /// Apply an impulse to a rigid body
    pub fn apply_impulse(&mut self, handle: RigidBodyHandle, impulse: [f32; 3]) -> Result<()> {
        self.bodies
            .get_mut(handle.0)
            .map(|b| {
                b.apply_impulse(rapier::Vector::new(impulse[0], impulse[1], impulse[2]), true);
            })
            .ok_or(PhysicsError::BodyNotFound(handle))
    }

    /// Apply an impulse at a world point
    pub fn apply_impulse_at_point(
        &mut self,
        handle: RigidBodyHandle,
        impulse: [f32; 3],
        point: [f32; 3],
    ) -> Result<()> {
        self.bodies
            .get_mut(handle.0)
            .map(|b| {
                b.apply_impulse_at_point(
                    rapier::Vector::new(impulse[0], impulse[1], impulse[2]),
                    rapier::Point::new(point[0], point[1], point[2]),
                    true,
                );
            })
            .ok_or(PhysicsError::BodyNotFound(handle))
    }

    // ==================== Colliders ====================

    /// Create a collider attached to a rigid body
    pub fn create_collider(&mut self, desc: ColliderDesc, parent: Option<RigidBodyHandle>) -> ColliderHandle {
        let builder = desc.to_rapier_builder();
        let handle = match parent {
            Some(body) => self.colliders.insert_with_parent(builder, body.0, &mut self.bodies),
            None => self.colliders.insert(builder),
        };
        ColliderHandle(handle)
    }

    /// Create a collider for an entity
    pub fn create_collider_for_entity(
        &mut self,
        entity_id: u128,
        mut desc: ColliderDesc,
        parent: Option<RigidBodyHandle>,
    ) -> ColliderHandle {
        desc.user_data = entity_id;
        let handle = self.create_collider(desc, parent);

        self.entity_to_collider
            .entry(entity_id)
            .or_insert_with(Vec::new)
            .push(handle);

        handle
    }

    /// Remove a collider
    pub fn remove_collider(&mut self, handle: ColliderHandle) {
        self.colliders.remove(handle.0, &mut self.islands, &mut self.bodies, true);

        // Clean up entity mapping
        for colliders in self.entity_to_collider.values_mut() {
            colliders.retain(|h| *h != handle);
        }
    }

    /// Set collider as sensor
    pub fn set_collider_sensor(&mut self, handle: ColliderHandle, is_sensor: bool) -> Result<()> {
        self.colliders
            .get_mut(handle.0)
            .map(|c| c.set_sensor(is_sensor))
            .ok_or(PhysicsError::ColliderNotFound(handle))
    }

    // ==================== Entity Helpers ====================

    /// Get the rigid body handle for an entity
    pub fn get_body_for_entity(&self, entity_id: u128) -> Option<RigidBodyHandle> {
        self.entity_to_body.get(&entity_id).copied()
    }

    /// Get collider handles for an entity
    pub fn get_colliders_for_entity(&self, entity_id: u128) -> Option<&Vec<ColliderHandle>> {
        self.entity_to_collider.get(&entity_id)
    }

    /// Remove all physics objects for an entity
    pub fn remove_entity(&mut self, entity_id: u128) {
        // Remove colliders first
        if let Some(colliders) = self.entity_to_collider.remove(&entity_id) {
            for handle in colliders {
                self.colliders.remove(handle.0, &mut self.islands, &mut self.bodies, true);
            }
        }

        // Remove body
        if let Some(handle) = self.entity_to_body.remove(&entity_id) {
            self.bodies.remove(
                handle.0,
                &mut self.islands,
                &mut self.colliders,
                &mut self.impulse_joints,
                &mut self.multibody_joints,
                true,
            );
        }
    }

    // ==================== Simulation ====================

    /// Step the physics simulation with fixed timestep
    pub fn step(&mut self, delta_time: f32) {
        self.accumulated_time += delta_time;

        let mut steps = 0;
        while self.accumulated_time >= self.config.timestep && steps < self.config.max_substeps {
            self.step_internal();
            self.accumulated_time -= self.config.timestep;
            steps += 1;
        }

        // Update query pipeline after stepping
        self.query_pipeline.update(&self.colliders);
    }

    /// Manually sync the query pipeline with current colliders.
    /// Call this after adding colliders if you need to query before the first step().
    pub fn sync_query_pipeline(&mut self) {
        self.query_pipeline.update(&self.colliders);
    }

    /// Internal fixed timestep
    fn step_internal(&mut self) {
        // Clear previous events
        self.events.clear();

        // Collect collision events
        let (collision_send, collision_recv) = crossbeam_channel::unbounded();
        let (contact_force_send, _contact_force_recv) = crossbeam_channel::unbounded();

        let event_handler = ChannelEventCollector {
            collision_events: collision_send,
            contact_force_events: contact_force_send,
        };

        // Step simulation
        self.pipeline.step(
            &self.gravity,
            &self.integration_params,
            &mut self.islands,
            &mut self.broad_phase,
            &mut self.narrow_phase,
            &mut self.bodies,
            &mut self.colliders,
            &mut self.impulse_joints,
            &mut self.multibody_joints,
            &mut self.ccd_solver,
            None,
            &(),
            &event_handler,
        );

        // Process collision events
        while let Ok(event) = collision_recv.try_recv() {
            let (h1, h2, started) = match event {
                rapier::CollisionEvent::Started(h1, h2, _) => (h1, h2, true),
                rapier::CollisionEvent::Stopped(h1, h2, _) => (h1, h2, false),
            };

            let c1 = self.colliders.get(h1);
            let c2 = self.colliders.get(h2);

            let is_sensor = c1.map(|c| c.is_sensor()).unwrap_or(false)
                || c2.map(|c| c.is_sensor()).unwrap_or(false);

            // Get contacts for started collisions
            let contacts = if started && !is_sensor {
                self.get_contacts(h1, h2)
            } else {
                Vec::new()
            };

            self.events.collision_events.push(CollisionEvent {
                collider1: ColliderHandle(h1),
                collider2: ColliderHandle(h2),
                event_type: if started {
                    CollisionEventType::Started
                } else {
                    CollisionEventType::Stopped
                },
                is_sensor,
                contacts,
                user_data1: c1.map(|c| c.user_data).unwrap_or(0),
                user_data2: c2.map(|c| c.user_data).unwrap_or(0),
            });
        }
    }

    /// Get contact points between two colliders
    fn get_contacts(&self, h1: rapier::ColliderHandle, h2: rapier::ColliderHandle) -> Vec<ContactData> {
        let mut contacts = Vec::new();

        if let Some(contact_pair) = self.narrow_phase.contact_pair(h1, h2) {
            for manifold in &contact_pair.manifolds {
                for point in &manifold.points {
                    contacts.push(ContactData {
                        point: [
                            manifold.local_n1.x * point.dist + point.local_p1.x,
                            manifold.local_n1.y * point.dist + point.local_p1.y,
                            manifold.local_n1.z * point.dist + point.local_p1.z,
                        ],
                        normal: [manifold.local_n1.x, manifold.local_n1.y, manifold.local_n1.z],
                        depth: point.dist,
                        impulse: point.data.impulse,
                    });
                }
            }
        }

        contacts
    }

    // ==================== Queries ====================

    /// Get a query interface for raycasting and shape queries
    pub fn query(&self) -> PhysicsQuery<'_> {
        PhysicsQuery {
            query_pipeline: &self.query_pipeline,
            colliders: &self.colliders,
            bodies: &self.bodies,
        }
    }

    // ==================== Events ====================

    /// Get collision events from the last step
    pub fn collision_events(&self) -> &[CollisionEvent] {
        &self.events.collision_events
    }

    /// Get collision start events
    pub fn collision_started(&self) -> impl Iterator<Item = &CollisionEvent> {
        self.events.started_collisions()
    }

    /// Get collision end events
    pub fn collision_stopped(&self) -> impl Iterator<Item = &CollisionEvent> {
        self.events.stopped_collisions()
    }

    /// Get trigger enter events
    pub fn trigger_enters(&self) -> impl Iterator<Item = &CollisionEvent> {
        self.events.trigger_enters()
    }

    /// Get trigger exit events
    pub fn trigger_exits(&self) -> impl Iterator<Item = &CollisionEvent> {
        self.events.trigger_exits()
    }

    // ==================== Debug ====================

    /// Get number of rigid bodies
    pub fn body_count(&self) -> usize {
        self.bodies.len()
    }

    /// Get number of colliders
    pub fn collider_count(&self) -> usize {
        self.colliders.len()
    }

    /// Get number of active (awake) bodies
    pub fn active_body_count(&self) -> usize {
        self.islands.active_dynamic_bodies().len()
    }
}

/// Contact force event data
struct ContactForceData {
    collider1: rapier::ColliderHandle,
    collider2: rapier::ColliderHandle,
    total_force: f32,
}

/// Channel-based event collector for Rapier
struct ChannelEventCollector {
    collision_events: crossbeam_channel::Sender<rapier::CollisionEvent>,
    contact_force_events: crossbeam_channel::Sender<ContactForceData>,
}

impl rapier::EventHandler for ChannelEventCollector {
    fn handle_collision_event(
        &self,
        _bodies: &rapier::RigidBodySet,
        _colliders: &rapier::ColliderSet,
        event: rapier::CollisionEvent,
        _contact_pair: Option<&rapier::ContactPair>,
    ) {
        let _ = self.collision_events.send(event);
    }

    fn handle_contact_force_event(
        &self,
        _dt: f32,
        _bodies: &rapier::RigidBodySet,
        _colliders: &rapier::ColliderSet,
        contact_pair: &rapier::ContactPair,
        total_force_magnitude: f32,
    ) {
        let _ = self.contact_force_events.send(ContactForceData {
            collider1: contact_pair.collider1,
            collider2: contact_pair.collider2,
            total_force: total_force_magnitude,
        });
    }
}

impl Default for PhysicsWorld {
    fn default() -> Self {
        Self::new(PhysicsConfig::default())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::collider::ColliderShape;

    #[test]
    fn test_create_world() {
        let world = PhysicsWorld::new(PhysicsConfig::default());
        assert_eq!(world.body_count(), 0);
        assert_eq!(world.collider_count(), 0);
    }

    #[test]
    fn test_create_body_and_collider() {
        let mut world = PhysicsWorld::new(PhysicsConfig::default());

        let body = world.create_rigid_body(RigidBodyDesc::dynamic().with_position(0.0, 10.0, 0.0));
        let _collider = world.create_collider(
            ColliderDesc::new(ColliderShape::sphere(1.0)),
            Some(body),
        );

        assert_eq!(world.body_count(), 1);
        assert_eq!(world.collider_count(), 1);
    }

    #[test]
    fn test_gravity_fall() {
        let mut world = PhysicsWorld::new(PhysicsConfig::default());

        let body = world.create_rigid_body(RigidBodyDesc::dynamic().with_position(0.0, 10.0, 0.0));
        world.create_collider(ColliderDesc::new(ColliderShape::sphere(1.0)), Some(body));

        let initial_y = world.get_body_position(body).unwrap()[1];

        // Step simulation
        for _ in 0..60 {
            world.step(1.0 / 60.0);
        }

        let final_y = world.get_body_position(body).unwrap()[1];
        assert!(final_y < initial_y, "Body should fall due to gravity");
    }
}
