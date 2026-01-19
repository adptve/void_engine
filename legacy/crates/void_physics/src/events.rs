//! Physics events (collisions, triggers)

use crate::collider::ColliderHandle;
use rapier3d::prelude as rapier;

/// Contact data from a collision
#[derive(Debug, Clone, Copy)]
pub struct ContactData {
    /// Contact point in world space
    pub point: [f32; 3],
    /// Contact normal (pointing from collider1 to collider2)
    pub normal: [f32; 3],
    /// Penetration depth
    pub depth: f32,
    /// Impulse applied at this contact
    pub impulse: f32,
}

/// Type of collision event
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CollisionEventType {
    /// Collision started
    Started,
    /// Collision ended
    Stopped,
}

/// A collision event between two colliders
#[derive(Debug, Clone)]
pub struct CollisionEvent {
    /// First collider
    pub collider1: ColliderHandle,
    /// Second collider
    pub collider2: ColliderHandle,
    /// Event type
    pub event_type: CollisionEventType,
    /// Whether this is a sensor event (trigger)
    pub is_sensor: bool,
    /// Contact points (empty for sensor events or stopped events)
    pub contacts: Vec<ContactData>,
    /// User data from collider 1
    pub user_data1: u128,
    /// User data from collider 2
    pub user_data2: u128,
}

impl CollisionEvent {
    /// Check if this is a start event
    pub fn is_started(&self) -> bool {
        self.event_type == CollisionEventType::Started
    }

    /// Check if this is a stop event
    pub fn is_stopped(&self) -> bool {
        self.event_type == CollisionEventType::Stopped
    }

    /// Get the average contact point
    pub fn average_contact_point(&self) -> Option<[f32; 3]> {
        if self.contacts.is_empty() {
            return None;
        }
        let mut sum = [0.0f32; 3];
        for contact in &self.contacts {
            sum[0] += contact.point[0];
            sum[1] += contact.point[1];
            sum[2] += contact.point[2];
        }
        let n = self.contacts.len() as f32;
        Some([sum[0] / n, sum[1] / n, sum[2] / n])
    }

    /// Get the average contact normal
    pub fn average_normal(&self) -> Option<[f32; 3]> {
        if self.contacts.is_empty() {
            return None;
        }
        let mut sum = [0.0f32; 3];
        for contact in &self.contacts {
            sum[0] += contact.normal[0];
            sum[1] += contact.normal[1];
            sum[2] += contact.normal[2];
        }
        // Normalize
        let len = (sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]).sqrt();
        if len > 0.0001 {
            Some([sum[0] / len, sum[1] / len, sum[2] / len])
        } else {
            None
        }
    }

    /// Get total impulse from all contacts
    pub fn total_impulse(&self) -> f32 {
        self.contacts.iter().map(|c| c.impulse).sum()
    }
}

/// Handler trait for physics events
pub trait PhysicsEventHandler: Send + Sync {
    /// Called when a collision starts or ends
    fn on_collision(&mut self, event: &CollisionEvent);

    /// Called for contact force events (high-impact collisions)
    fn on_contact_force(&mut self, _collider1: ColliderHandle, _collider2: ColliderHandle, _total_force: f32) {}
}

/// Default event handler that collects events into a buffer
#[derive(Default)]
pub struct EventCollector {
    /// Collision events this frame
    pub collision_events: Vec<CollisionEvent>,
    /// Contact force events this frame
    pub force_events: Vec<(ColliderHandle, ColliderHandle, f32)>,
}

impl EventCollector {
    /// Create a new event collector
    pub fn new() -> Self {
        Self::default()
    }

    /// Clear all collected events
    pub fn clear(&mut self) {
        self.collision_events.clear();
        self.force_events.clear();
    }

    /// Get collision start events
    pub fn started_collisions(&self) -> impl Iterator<Item = &CollisionEvent> {
        self.collision_events.iter().filter(|e| e.is_started())
    }

    /// Get collision end events
    pub fn stopped_collisions(&self) -> impl Iterator<Item = &CollisionEvent> {
        self.collision_events.iter().filter(|e| e.is_stopped())
    }

    /// Get sensor/trigger start events
    pub fn trigger_enters(&self) -> impl Iterator<Item = &CollisionEvent> {
        self.collision_events
            .iter()
            .filter(|e| e.is_sensor && e.is_started())
    }

    /// Get sensor/trigger end events
    pub fn trigger_exits(&self) -> impl Iterator<Item = &CollisionEvent> {
        self.collision_events
            .iter()
            .filter(|e| e.is_sensor && e.is_stopped())
    }
}

impl PhysicsEventHandler for EventCollector {
    fn on_collision(&mut self, event: &CollisionEvent) {
        self.collision_events.push(event.clone());
    }

    fn on_contact_force(&mut self, collider1: ColliderHandle, collider2: ColliderHandle, total_force: f32) {
        self.force_events.push((collider1, collider2, total_force));
    }
}

/// Internal Rapier event handler
pub(crate) struct RapierEventHandler<'a> {
    pub(crate) handler: &'a mut dyn PhysicsEventHandler,
    pub(crate) colliders: &'a rapier::ColliderSet,
}

impl<'a> rapier::EventHandler for RapierEventHandler<'a> {
    fn handle_collision_event(
        &self,
        _bodies: &rapier::RigidBodySet,
        _colliders: &rapier::ColliderSet,
        event: rapier::CollisionEvent,
        _contact_pair: Option<&rapier::ContactPair>,
    ) {
        let (h1, h2, started) = match event {
            rapier::CollisionEvent::Started(h1, h2, _) => (h1, h2, true),
            rapier::CollisionEvent::Stopped(h1, h2, _) => (h1, h2, false),
        };

        let c1 = self.colliders.get(h1);
        let c2 = self.colliders.get(h2);

        let is_sensor = c1.map(|c| c.is_sensor()).unwrap_or(false)
            || c2.map(|c| c.is_sensor()).unwrap_or(false);

        let collision_event = CollisionEvent {
            collider1: ColliderHandle(h1),
            collider2: ColliderHandle(h2),
            event_type: if started {
                CollisionEventType::Started
            } else {
                CollisionEventType::Stopped
            },
            is_sensor,
            contacts: Vec::new(), // Contacts would need contact_pair processing
            user_data1: c1.map(|c| c.user_data).unwrap_or(0),
            user_data2: c2.map(|c| c.user_data).unwrap_or(0),
        };

        // Note: We can't call handler here due to borrow checker
        // Events are collected differently in practice
        let _ = collision_event;
    }

    fn handle_contact_force_event(
        &self,
        _dt: f32,
        _bodies: &rapier::RigidBodySet,
        _colliders: &rapier::ColliderSet,
        _contact_pair: &rapier::ContactPair,
        _total_force_magnitude: f32,
    ) {
        // Contact force events handled separately
    }
}
