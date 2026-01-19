//! # void_event - Lock-free Event System
//!
//! High-performance event system with:
//! - Lock-free event queues
//! - Priority-based delivery
//! - Typed and dynamic events
//! - Observer pattern support

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::vec::Vec;
use core::any::{Any, TypeId};
use void_core::Id;
use void_structures::LockFreeQueue;

/// Event priority
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Priority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3,
}

impl Default for Priority {
    fn default() -> Self {
        Self::Normal
    }
}

/// Event envelope containing metadata
pub struct EventEnvelope {
    /// Event type ID
    pub type_id: TypeId,
    /// Event data
    pub data: Box<dyn Any + Send + Sync>,
    /// Priority
    pub priority: Priority,
    /// Timestamp
    pub timestamp: u64,
    /// Source ID
    pub source: Option<Id>,
}

impl EventEnvelope {
    /// Create a new envelope
    pub fn new<E: Event>(event: E, priority: Priority, timestamp: u64) -> Self {
        Self {
            type_id: TypeId::of::<E>(),
            data: Box::new(event),
            priority,
            timestamp,
            source: None,
        }
    }

    /// Try to downcast to specific event type
    pub fn downcast_ref<E: Event>(&self) -> Option<&E> {
        self.data.downcast_ref::<E>()
    }

    /// Try to take the event data
    pub fn downcast<E: Event>(self) -> Option<E> {
        self.data.downcast::<E>().ok().map(|b| *b)
    }
}

/// Trait for events
pub trait Event: Send + Sync + 'static {}

// Blanket implementation
impl<T: Send + Sync + 'static> Event for T {}

/// Event handler function type
pub type EventHandler<E> = Box<dyn Fn(&E) + Send + Sync>;

/// Dynamic event handler
pub type DynamicHandler = Box<dyn Fn(&dyn Any) + Send + Sync>;

/// Subscriber ID
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct SubscriberId(pub u64);

/// Event bus for publishing and subscribing to events
pub struct EventBus {
    /// Event queue
    queue: LockFreeQueue<EventEnvelope>,
    /// Typed handlers
    handlers: BTreeMap<TypeId, Vec<(SubscriberId, Priority, DynamicHandler)>>,
    /// Next subscriber ID
    next_subscriber_id: u64,
    /// Event timestamp counter
    timestamp: u64,
}

impl EventBus {
    /// Create a new event bus
    pub fn new() -> Self {
        Self {
            queue: LockFreeQueue::new(),
            handlers: BTreeMap::new(),
            next_subscriber_id: 1,
            timestamp: 0,
        }
    }

    /// Publish an event
    pub fn publish<E: Event>(&self, event: E) {
        self.publish_with_priority(event, Priority::Normal);
    }

    /// Publish an event with priority
    pub fn publish_with_priority<E: Event>(&self, event: E, priority: Priority) {
        let envelope = EventEnvelope::new(event, priority, self.timestamp);
        self.queue.push(envelope);
    }

    /// Subscribe to an event type
    pub fn subscribe<E: Event, F>(&mut self, handler: F) -> SubscriberId
    where
        F: Fn(&E) + Send + Sync + 'static,
    {
        self.subscribe_with_priority::<E, F>(handler, Priority::Normal)
    }

    /// Subscribe with priority
    pub fn subscribe_with_priority<E: Event, F>(
        &mut self,
        handler: F,
        priority: Priority,
    ) -> SubscriberId
    where
        F: Fn(&E) + Send + Sync + 'static,
    {
        let id = SubscriberId(self.next_subscriber_id);
        self.next_subscriber_id += 1;

        let type_id = TypeId::of::<E>();
        let wrapped_handler: DynamicHandler = Box::new(move |any: &dyn Any| {
            if let Some(event) = any.downcast_ref::<E>() {
                handler(event);
            }
        });

        self.handlers
            .entry(type_id)
            .or_default()
            .push((id, priority, wrapped_handler));

        // Sort by priority (higher priority first)
        if let Some(handlers) = self.handlers.get_mut(&type_id) {
            handlers.sort_by(|a, b| b.1.cmp(&a.1));
        }

        id
    }

    /// Unsubscribe
    pub fn unsubscribe(&mut self, id: SubscriberId) {
        for handlers in self.handlers.values_mut() {
            handlers.retain(|(sub_id, _, _)| *sub_id != id);
        }
    }

    /// Process all pending events
    pub fn process(&mut self) {
        // Collect events and sort by priority
        let mut events = Vec::new();
        while let Some(envelope) = self.queue.pop() {
            events.push(envelope);
        }

        // Sort by priority (higher priority first)
        events.sort_by(|a, b| b.priority.cmp(&a.priority));

        // Dispatch events
        for envelope in events {
            if let Some(handlers) = self.handlers.get(&envelope.type_id) {
                for (_, _, handler) in handlers {
                    handler(envelope.data.as_ref());
                }
            }
        }

        self.timestamp += 1;
    }

    /// Clear all events without processing
    pub fn clear(&self) {
        while self.queue.pop().is_some() {}
    }

    /// Get pending event count
    pub fn pending_count(&self) -> usize {
        self.queue.len()
    }

    /// Check if there are pending events
    pub fn has_pending(&self) -> bool {
        !self.queue.is_empty()
    }
}

impl Default for EventBus {
    fn default() -> Self {
        Self::new()
    }
}

/// Channel for single-type events
pub struct EventChannel<E: Event> {
    queue: LockFreeQueue<E>,
}

impl<E: Event> EventChannel<E> {
    /// Create a new channel
    pub fn new() -> Self {
        Self {
            queue: LockFreeQueue::new(),
        }
    }

    /// Send an event
    pub fn send(&self, event: E) {
        self.queue.push(event);
    }

    /// Receive an event
    pub fn receive(&self) -> Option<E> {
        self.queue.pop()
    }

    /// Drain all events
    pub fn drain(&self) -> Vec<E> {
        let mut events = Vec::new();
        while let Some(event) = self.queue.pop() {
            events.push(event);
        }
        events
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.queue.is_empty()
    }

    /// Get pending count
    pub fn len(&self) -> usize {
        self.queue.len()
    }
}

impl<E: Event> Default for EventChannel<E> {
    fn default() -> Self {
        Self::new()
    }
}

/// Prelude
pub mod prelude {
    pub use crate::{Event, EventBus, EventChannel, EventEnvelope, Priority, SubscriberId};
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::sync::atomic::{AtomicU32, Ordering};
    use alloc::sync::Arc;

    struct TestEvent(i32);

    #[test]
    fn test_event_bus() {
        let mut bus = EventBus::new();
        let counter = Arc::new(AtomicU32::new(0));
        let counter_clone = counter.clone();

        bus.subscribe(move |_: &TestEvent| {
            counter_clone.fetch_add(1, Ordering::SeqCst);
        });

        bus.publish(TestEvent(42));
        bus.process();

        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    #[test]
    fn test_event_channel() {
        let channel: EventChannel<TestEvent> = EventChannel::new();

        channel.send(TestEvent(1));
        channel.send(TestEvent(2));
        channel.send(TestEvent(3));

        let events = channel.drain();
        assert_eq!(events.len(), 3);
        assert_eq!(events[0].0, 1);
        assert_eq!(events[1].0, 2);
        assert_eq!(events[2].0, 3);
    }

    #[test]
    fn test_priority() {
        let mut bus = EventBus::new();
        let order = Arc::new(parking_lot::Mutex::new(Vec::new()));
        let order1 = order.clone();
        let order2 = order.clone();

        bus.subscribe_with_priority(
            move |e: &TestEvent| {
                order1.lock().push(("high", e.0));
            },
            Priority::High,
        );

        bus.subscribe_with_priority(
            move |e: &TestEvent| {
                order2.lock().push(("low", e.0));
            },
            Priority::Low,
        );

        bus.publish(TestEvent(42));
        bus.process();

        let received = order.lock();
        // High priority should be first
        assert_eq!(received[0].0, "high");
        assert_eq!(received[1].0, "low");
    }
}
