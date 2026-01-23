#pragma once

/// @file event.hpp
/// @brief Main include header for void_event
///
/// void_event provides a high-performance event system with:
/// - Lock-free event queues (using void_structures::LockFreeQueue)
/// - Priority-based delivery
/// - Typed EventChannel for single-type events
/// - Dynamic EventBus for multi-type events
/// - Broadcast channels for fan-out delivery
///
/// ## Quick Start
///
/// ### EventBus (dynamic, multi-type events)
/// ```cpp
/// void_event::EventBus bus;
///
/// // Subscribe to events
/// auto sub_id = bus.subscribe<MyEvent>([](const MyEvent& e) {
///     // Handle event
/// });
///
/// // Publish events
/// bus.publish(MyEvent{...});
///
/// // Process all pending events
/// bus.process();
///
/// // Unsubscribe
/// bus.unsubscribe(sub_id);
/// ```
///
/// ### EventChannel (typed, single-type events)
/// ```cpp
/// void_event::EventChannel<MyEvent> channel;
///
/// // Send events
/// channel.send(MyEvent{...});
///
/// // Receive events
/// while (auto event = channel.receive()) {
///     // Handle *event
/// }
///
/// // Or drain all at once
/// auto events = channel.drain();
/// ```
///
/// ### BroadcastChannel (fan-out to multiple receivers)
/// ```cpp
/// void_event::BroadcastChannel<MyEvent> broadcast;
///
/// // Create receivers
/// auto recv1 = broadcast.create_receiver();
/// auto recv2 = broadcast.create_receiver();
///
/// // Send to all receivers
/// broadcast.send(MyEvent{...});
///
/// // Each receiver gets its own copy
/// auto e1 = recv1->receive();
/// auto e2 = recv2->receive();
/// ```

#include "fwd.hpp"
#include "event_bus.hpp"
#include "channel.hpp"

namespace void_event {

/// Prelude - commonly used types
namespace prelude {
    using void_event::Priority;
    using void_event::SubscriberId;
    using void_event::EventEnvelope;
    using void_event::EventBus;
    using void_event::EventChannel;
    using void_event::BroadcastChannel;
} // namespace prelude

} // namespace void_event
