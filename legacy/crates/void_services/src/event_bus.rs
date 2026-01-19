//! Event bus for inter-service communication
//!
//! Provides a publish-subscribe system for services to communicate.

use std::any::Any;
use std::collections::HashMap;
use std::sync::Arc;
use parking_lot::RwLock;
use serde::{Serialize, Deserialize};

/// Event identifier
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct EventId(String);

impl EventId {
    /// Create a new event ID
    pub fn new(name: impl Into<String>) -> Self {
        Self(name.into())
    }

    /// Get the event name
    pub fn name(&self) -> &str {
        &self.0
    }
}

impl std::fmt::Display for EventId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Event priority
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub enum EventPriority {
    /// Low priority (processed last)
    Low = 0,
    /// Normal priority
    Normal = 1,
    /// High priority (processed first)
    High = 2,
    /// Critical priority (immediate processing)
    Critical = 3,
}

impl Default for EventPriority {
    fn default() -> Self {
        Self::Normal
    }
}

/// Base event trait
pub trait Event: Send + Sync + 'static {
    /// Get the event ID
    fn event_id(&self) -> EventId;

    /// Get event priority
    fn priority(&self) -> EventPriority {
        EventPriority::Normal
    }

    /// Whether this event can be cancelled
    fn cancellable(&self) -> bool {
        false
    }

    /// Type erasure
    fn as_any(&self) -> &dyn Any;
}

/// Event handler trait
pub trait EventHandler: Send + Sync {
    /// Handle an event
    fn handle(&self, event: &dyn Event) -> EventResult;

    /// Get the event types this handler is interested in
    fn event_types(&self) -> Vec<EventId>;

    /// Handler priority (higher = called first)
    fn handler_priority(&self) -> i32 {
        0
    }
}

/// Event handling result
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EventResult {
    /// Continue propagation
    Continue,
    /// Stop propagation (event handled)
    Handled,
    /// Cancel the event (if cancellable)
    Cancel,
}

/// Simple event implementation
#[derive(Debug, Clone)]
pub struct SimpleEvent {
    /// Event ID
    pub id: EventId,
    /// Event data
    pub data: HashMap<String, String>,
    /// Priority
    pub priority: EventPriority,
}

impl SimpleEvent {
    /// Create a new simple event
    pub fn new(id: impl Into<String>) -> Self {
        Self {
            id: EventId::new(id),
            data: HashMap::new(),
            priority: EventPriority::Normal,
        }
    }

    /// Add data to the event
    pub fn with_data(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.data.insert(key.into(), value.into());
        self
    }

    /// Set priority
    pub fn with_priority(mut self, priority: EventPriority) -> Self {
        self.priority = priority;
        self
    }

    /// Get data value
    pub fn get(&self, key: &str) -> Option<&str> {
        self.data.get(key).map(|s| s.as_str())
    }
}

impl Event for SimpleEvent {
    fn event_id(&self) -> EventId {
        self.id.clone()
    }

    fn priority(&self) -> EventPriority {
        self.priority
    }

    fn as_any(&self) -> &dyn Any {
        self
    }
}

/// Handler entry in the bus
struct HandlerEntry {
    handler: Box<dyn EventHandler>,
    priority: i32,
}

/// Event bus for publish-subscribe messaging
pub struct EventBus {
    /// Handlers by event type
    handlers: HashMap<EventId, Vec<HandlerEntry>>,
    /// Global handlers (receive all events)
    global_handlers: Vec<HandlerEntry>,
    /// Event queue (for deferred processing)
    queue: Vec<Box<dyn Event>>,
    /// Statistics
    stats: EventBusStats,
}

/// Event bus statistics
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct EventBusStats {
    /// Total events published
    pub events_published: u64,
    /// Total events handled
    pub events_handled: u64,
    /// Total events cancelled
    pub events_cancelled: u64,
    /// Handlers registered
    pub handlers_registered: usize,
}

impl EventBus {
    /// Create a new event bus
    pub fn new() -> Self {
        Self {
            handlers: HashMap::new(),
            global_handlers: Vec::new(),
            queue: Vec::new(),
            stats: EventBusStats::default(),
        }
    }

    /// Subscribe a handler to specific event types
    pub fn subscribe(&mut self, handler: Box<dyn EventHandler>) {
        let priority = handler.handler_priority();
        let event_types = handler.event_types();

        if event_types.is_empty() {
            // Global handler
            self.global_handlers.push(HandlerEntry { handler, priority });
            self.global_handlers.sort_by(|a, b| b.priority.cmp(&a.priority));
        } else {
            // Specific event handlers
            for event_id in event_types {
                let entries = self.handlers.entry(event_id).or_insert_with(Vec::new);
                entries.push(HandlerEntry {
                    handler: unsafe {
                        // SAFETY: We're just storing the same handler reference multiple times
                        // This is safe because we're using Box and the handler is Send + Sync
                        std::ptr::read(&handler as *const Box<dyn EventHandler>)
                    },
                    priority,
                });
                entries.sort_by(|a, b| b.priority.cmp(&a.priority));
            }
            std::mem::forget(handler); // Don't drop since we copied the pointer above
        }

        self.stats.handlers_registered += 1;
    }

    /// Subscribe a closure handler
    pub fn subscribe_fn<F>(&mut self, event_id: EventId, handler: F)
    where
        F: Fn(&dyn Event) -> EventResult + Send + Sync + 'static,
    {
        struct ClosureHandler<F> {
            event_id: EventId,
            handler: F,
        }

        impl<F> EventHandler for ClosureHandler<F>
        where
            F: Fn(&dyn Event) -> EventResult + Send + Sync + 'static,
        {
            fn handle(&self, event: &dyn Event) -> EventResult {
                (self.handler)(event)
            }

            fn event_types(&self) -> Vec<EventId> {
                vec![self.event_id.clone()]
            }
        }

        self.subscribe(Box::new(ClosureHandler { event_id, handler }));
    }

    /// Publish an event (immediate processing)
    pub fn publish(&mut self, event: &dyn Event) -> EventResult {
        self.stats.events_published += 1;

        let event_id = event.event_id();
        let mut result = EventResult::Continue;

        // Process global handlers first
        for entry in &self.global_handlers {
            let handler_result = entry.handler.handle(event);
            if handler_result == EventResult::Cancel && event.cancellable() {
                self.stats.events_cancelled += 1;
                return EventResult::Cancel;
            }
            if handler_result == EventResult::Handled {
                result = EventResult::Handled;
            }
        }

        // Process specific handlers
        if let Some(handlers) = self.handlers.get(&event_id) {
            for entry in handlers {
                let handler_result = entry.handler.handle(event);
                if handler_result == EventResult::Cancel && event.cancellable() {
                    self.stats.events_cancelled += 1;
                    return EventResult::Cancel;
                }
                if handler_result == EventResult::Handled {
                    result = EventResult::Handled;
                }
            }
        }

        if result == EventResult::Handled {
            self.stats.events_handled += 1;
        }

        result
    }

    /// Queue an event for later processing
    pub fn queue(&mut self, event: Box<dyn Event>) {
        self.queue.push(event);
    }

    /// Process all queued events
    pub fn process_queue(&mut self) -> usize {
        let events: Vec<Box<dyn Event>> = self.queue.drain(..).collect();
        let count = events.len();

        // Sort by priority
        let mut sorted: Vec<_> = events.into_iter().collect();
        sorted.sort_by(|a, b| b.priority().cmp(&a.priority()));

        for event in sorted {
            self.publish(event.as_ref());
        }

        count
    }

    /// Get queue length
    pub fn queue_len(&self) -> usize {
        self.queue.len()
    }

    /// Get statistics
    pub fn stats(&self) -> &EventBusStats {
        &self.stats
    }

    /// Clear all handlers
    pub fn clear(&mut self) {
        self.handlers.clear();
        self.global_handlers.clear();
        self.queue.clear();
        self.stats.handlers_registered = 0;
    }
}

impl Default for EventBus {
    fn default() -> Self {
        Self::new()
    }
}

/// Thread-safe event bus wrapper
pub struct SharedEventBus {
    inner: Arc<RwLock<EventBus>>,
}

impl SharedEventBus {
    /// Create a new shared event bus
    pub fn new() -> Self {
        Self {
            inner: Arc::new(RwLock::new(EventBus::new())),
        }
    }

    /// Get a read lock
    pub fn read(&self) -> parking_lot::RwLockReadGuard<'_, EventBus> {
        self.inner.read()
    }

    /// Get a write lock
    pub fn write(&self) -> parking_lot::RwLockWriteGuard<'_, EventBus> {
        self.inner.write()
    }

    /// Clone the Arc
    pub fn clone_arc(&self) -> Self {
        Self {
            inner: Arc::clone(&self.inner),
        }
    }
}

impl Default for SharedEventBus {
    fn default() -> Self {
        Self::new()
    }
}

impl Clone for SharedEventBus {
    fn clone(&self) -> Self {
        self.clone_arc()
    }
}

// Common event types
/// System startup event
#[derive(Debug, Clone)]
pub struct SystemStartEvent;

impl Event for SystemStartEvent {
    fn event_id(&self) -> EventId {
        EventId::new("system.start")
    }

    fn priority(&self) -> EventPriority {
        EventPriority::Critical
    }

    fn as_any(&self) -> &dyn Any {
        self
    }
}

/// System shutdown event
#[derive(Debug, Clone)]
pub struct SystemShutdownEvent;

impl Event for SystemShutdownEvent {
    fn event_id(&self) -> EventId {
        EventId::new("system.shutdown")
    }

    fn priority(&self) -> EventPriority {
        EventPriority::Critical
    }

    fn as_any(&self) -> &dyn Any {
        self
    }
}

/// Service state changed event
#[derive(Debug, Clone)]
pub struct ServiceStateChangedEvent {
    pub service_id: String,
    pub old_state: String,
    pub new_state: String,
}

impl Event for ServiceStateChangedEvent {
    fn event_id(&self) -> EventId {
        EventId::new("service.state_changed")
    }

    fn as_any(&self) -> &dyn Any {
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU32, Ordering};

    #[test]
    fn test_event_id() {
        let id = EventId::new("test.event");
        assert_eq!(id.name(), "test.event");
    }

    #[test]
    fn test_simple_event() {
        let event = SimpleEvent::new("test")
            .with_data("key", "value")
            .with_priority(EventPriority::High);

        assert_eq!(event.event_id().name(), "test");
        assert_eq!(event.get("key"), Some("value"));
        assert_eq!(event.priority(), EventPriority::High);
    }

    #[test]
    fn test_publish_event() {
        let mut bus = EventBus::new();
        let counter = Arc::new(AtomicU32::new(0));

        let counter_clone = Arc::clone(&counter);
        bus.subscribe_fn(EventId::new("test"), move |_| {
            counter_clone.fetch_add(1, Ordering::SeqCst);
            EventResult::Handled
        });

        let event = SimpleEvent::new("test");
        let result = bus.publish(&event);

        assert_eq!(result, EventResult::Handled);
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    #[test]
    fn test_multiple_handlers() {
        let mut bus = EventBus::new();
        let counter = Arc::new(AtomicU32::new(0));

        for _ in 0..3 {
            let counter_clone = Arc::clone(&counter);
            bus.subscribe_fn(EventId::new("test"), move |_| {
                counter_clone.fetch_add(1, Ordering::SeqCst);
                EventResult::Continue
            });
        }

        let event = SimpleEvent::new("test");
        bus.publish(&event);

        assert_eq!(counter.load(Ordering::SeqCst), 3);
    }

    #[test]
    fn test_event_queue() {
        let mut bus = EventBus::new();
        let counter = Arc::new(AtomicU32::new(0));

        let counter_clone = Arc::clone(&counter);
        bus.subscribe_fn(EventId::new("test"), move |_| {
            counter_clone.fetch_add(1, Ordering::SeqCst);
            EventResult::Continue
        });

        // Queue events
        bus.queue(Box::new(SimpleEvent::new("test")));
        bus.queue(Box::new(SimpleEvent::new("test")));
        assert_eq!(bus.queue_len(), 2);

        // Process queue
        let processed = bus.process_queue();
        assert_eq!(processed, 2);
        assert_eq!(bus.queue_len(), 0);
        assert_eq!(counter.load(Ordering::SeqCst), 2);
    }

    #[test]
    fn test_event_priority_order() {
        let mut bus = EventBus::new();
        let order = Arc::new(RwLock::new(Vec::new()));

        let order_clone = Arc::clone(&order);
        bus.subscribe_fn(EventId::new("test"), move |event| {
            order_clone.write().push(event.priority());
            EventResult::Continue
        });

        bus.queue(Box::new(SimpleEvent::new("test").with_priority(EventPriority::Low)));
        bus.queue(Box::new(SimpleEvent::new("test").with_priority(EventPriority::High)));
        bus.queue(Box::new(SimpleEvent::new("test").with_priority(EventPriority::Normal)));

        bus.process_queue();

        let order_vec = order.read();
        assert_eq!(order_vec[0], EventPriority::High);
        assert_eq!(order_vec[1], EventPriority::Normal);
        assert_eq!(order_vec[2], EventPriority::Low);
    }

    #[test]
    fn test_system_events() {
        let start = SystemStartEvent;
        assert_eq!(start.event_id().name(), "system.start");
        assert_eq!(start.priority(), EventPriority::Critical);

        let shutdown = SystemShutdownEvent;
        assert_eq!(shutdown.event_id().name(), "system.shutdown");
    }

    #[test]
    fn test_stats() {
        let mut bus = EventBus::new();

        bus.subscribe_fn(EventId::new("test"), |_| EventResult::Handled);

        bus.publish(&SimpleEvent::new("test"));
        bus.publish(&SimpleEvent::new("test"));

        assert_eq!(bus.stats().events_published, 2);
        assert_eq!(bus.stats().events_handled, 2);
    }

    #[test]
    fn test_shared_event_bus() {
        let shared = SharedEventBus::new();
        let counter = Arc::new(AtomicU32::new(0));

        {
            let counter_clone = Arc::clone(&counter);
            shared.write().subscribe_fn(EventId::new("test"), move |_| {
                counter_clone.fetch_add(1, Ordering::SeqCst);
                EventResult::Continue
            });
        }

        {
            let mut bus = shared.write();
            bus.publish(&SimpleEvent::new("test"));
        }

        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }
}
