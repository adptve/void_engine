//! Trigger events

use serde::{Deserialize, Serialize};

/// Type of trigger event
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum TriggerEventType {
    /// Entity entered the trigger volume
    Enter,
    /// Entity exited the trigger volume
    Exit,
    /// Entity is inside the trigger volume (every frame)
    Stay,
}

/// A trigger event
#[derive(Debug, Clone)]
pub struct TriggerEvent {
    /// Type of event
    pub event_type: TriggerEventType,
    /// The trigger entity
    pub trigger_entity: u64,
    /// The entity that triggered the event
    pub other_entity: u64,
    /// World position of the trigger
    pub trigger_position: [f32; 3],
    /// Position of the other entity
    pub other_position: [f32; 3],
    /// Time in the trigger (for Stay events)
    pub time_in_trigger: f32,
}

impl TriggerEvent {
    /// Create an enter event
    pub fn enter(trigger: u64, other: u64) -> Self {
        Self {
            event_type: TriggerEventType::Enter,
            trigger_entity: trigger,
            other_entity: other,
            trigger_position: [0.0, 0.0, 0.0],
            other_position: [0.0, 0.0, 0.0],
            time_in_trigger: 0.0,
        }
    }

    /// Create an exit event
    pub fn exit(trigger: u64, other: u64, time_spent: f32) -> Self {
        Self {
            event_type: TriggerEventType::Exit,
            trigger_entity: trigger,
            other_entity: other,
            trigger_position: [0.0, 0.0, 0.0],
            other_position: [0.0, 0.0, 0.0],
            time_in_trigger: time_spent,
        }
    }

    /// Create a stay event
    pub fn stay(trigger: u64, other: u64, time_spent: f32) -> Self {
        Self {
            event_type: TriggerEventType::Stay,
            trigger_entity: trigger,
            other_entity: other,
            trigger_position: [0.0, 0.0, 0.0],
            other_position: [0.0, 0.0, 0.0],
            time_in_trigger: time_spent,
        }
    }

    /// Set trigger position
    pub fn with_trigger_position(mut self, pos: [f32; 3]) -> Self {
        self.trigger_position = pos;
        self
    }

    /// Set other entity position
    pub fn with_other_position(mut self, pos: [f32; 3]) -> Self {
        self.other_position = pos;
        self
    }

    /// Check if this is an enter event
    pub fn is_enter(&self) -> bool {
        self.event_type == TriggerEventType::Enter
    }

    /// Check if this is an exit event
    pub fn is_exit(&self) -> bool {
        self.event_type == TriggerEventType::Exit
    }

    /// Check if this is a stay event
    pub fn is_stay(&self) -> bool {
        self.event_type == TriggerEventType::Stay
    }
}

/// Callback type for trigger events
pub type TriggerCallback = Box<dyn Fn(&TriggerEvent) + Send + Sync>;

/// Builder for trigger event handlers
pub struct TriggerHandler {
    /// Callback for enter events
    pub on_enter: Option<TriggerCallback>,
    /// Callback for exit events
    pub on_exit: Option<TriggerCallback>,
    /// Callback for stay events
    pub on_stay: Option<TriggerCallback>,
}

impl TriggerHandler {
    /// Create a new empty handler
    pub fn new() -> Self {
        Self {
            on_enter: None,
            on_exit: None,
            on_stay: None,
        }
    }

    /// Set enter callback
    pub fn on_enter<F>(mut self, f: F) -> Self
    where
        F: Fn(&TriggerEvent) + Send + Sync + 'static,
    {
        self.on_enter = Some(Box::new(f));
        self
    }

    /// Set exit callback
    pub fn on_exit<F>(mut self, f: F) -> Self
    where
        F: Fn(&TriggerEvent) + Send + Sync + 'static,
    {
        self.on_exit = Some(Box::new(f));
        self
    }

    /// Set stay callback
    pub fn on_stay<F>(mut self, f: F) -> Self
    where
        F: Fn(&TriggerEvent) + Send + Sync + 'static,
    {
        self.on_stay = Some(Box::new(f));
        self
    }

    /// Handle an event
    pub fn handle(&self, event: &TriggerEvent) {
        match event.event_type {
            TriggerEventType::Enter => {
                if let Some(ref callback) = self.on_enter {
                    callback(event);
                }
            }
            TriggerEventType::Exit => {
                if let Some(ref callback) = self.on_exit {
                    callback(event);
                }
            }
            TriggerEventType::Stay => {
                if let Some(ref callback) = self.on_stay {
                    callback(event);
                }
            }
        }
    }
}

impl Default for TriggerHandler {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU32, Ordering};
    use std::sync::Arc;

    #[test]
    fn test_event_creation() {
        let event = TriggerEvent::enter(1, 2)
            .with_trigger_position([1.0, 2.0, 3.0])
            .with_other_position([4.0, 5.0, 6.0]);

        assert!(event.is_enter());
        assert_eq!(event.trigger_entity, 1);
        assert_eq!(event.other_entity, 2);
    }

    #[test]
    fn test_handler_callbacks() {
        let enter_count = Arc::new(AtomicU32::new(0));
        let exit_count = Arc::new(AtomicU32::new(0));

        let enter_count_clone = enter_count.clone();
        let exit_count_clone = exit_count.clone();

        let handler = TriggerHandler::new()
            .on_enter(move |_| {
                enter_count_clone.fetch_add(1, Ordering::SeqCst);
            })
            .on_exit(move |_| {
                exit_count_clone.fetch_add(1, Ordering::SeqCst);
            });

        handler.handle(&TriggerEvent::enter(1, 2));
        handler.handle(&TriggerEvent::enter(1, 3));
        handler.handle(&TriggerEvent::exit(1, 2, 1.0));

        assert_eq!(enter_count.load(Ordering::SeqCst), 2);
        assert_eq!(exit_count.load(Ordering::SeqCst), 1);
    }
}
