# void_services Integration Diagram

## Module Overview

void_services provides service lifecycle management, event-driven communication, and session handling for the Void Engine.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           void_services                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐      │
│  │  ServiceRegistry │  │    EventBus      │  │  SessionManager  │      │
│  │                  │  │                  │  │                  │      │
│  │  • register()    │  │  • subscribe()   │  │  • create()      │      │
│  │  • start_all()   │  │  • publish()     │  │  • authenticate()│      │
│  │  • stop_all()    │  │  • process()     │  │  • terminate()   │      │
│  │  • health_check()│  │  • queue()       │  │  • permissions   │      │
│  └────────┬─────────┘  └────────┬─────────┘  └────────┬─────────┘      │
│           │                     │                     │                 │
│           └─────────────────────┼─────────────────────┘                 │
│                                 │                                        │
│                    ┌────────────▼────────────┐                          │
│                    │    Hot-Reload Support    │                          │
│                    │                          │                          │
│                    │  • take_snapshot()       │                          │
│                    │  • restore_snapshot()    │                          │
│                    │  • binary serialization  │                          │
│                    └────────────────────────┘                          │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

## Component Relationships

### ServiceRegistry

Central service management hub that handles:
- Service registration and discovery
- Lifecycle management (start/stop/restart)
- Health monitoring with auto-restart
- Dependency ordering for startup/shutdown

```
┌─────────────────────────────────────────────────────┐
│                  ServiceRegistry                     │
├─────────────────────────────────────────────────────┤
│                                                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐ │
│  │  Service A  │  │  Service B  │  │  Service C  │ │
│  │  (Running)  │  │  (Stopped)  │  │  (Degraded) │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘ │
│         │                │                │         │
│         └────────────────┼────────────────┘         │
│                          │                          │
│              ┌───────────▼───────────┐             │
│              │   Health Monitor      │             │
│              │   (background thread) │             │
│              └───────────────────────┘             │
│                                                      │
└─────────────────────────────────────────────────────┘
```

### EventBus

Publish-subscribe event system with:
- Type-safe event publishing
- Priority-based processing
- Category wildcards
- Thread-safe access

```
Publishers                        Subscribers
    │                                 ▲
    │  ┌─────────────────────────┐   │
    └──►    Type Handlers        ├───┘
       │  (TypedEventHandler<T>) │
       └─────────────────────────┘
                  ▲
    │  ┌─────────────────────────┐   │
    └──►  Category Handlers      ├───┘
       │  (wildcard matching)    │
       └─────────────────────────┘
                  ▲
       ┌─────────────────────────┐
       │   Priority Queues       │
       │  Critical > High >      │
       │  Normal > Low           │
       └─────────────────────────┘
```

### SessionManager

Client session management with:
- Session creation and lifecycle
- Authentication state tracking
- Hierarchical permissions
- Session variables (key-value storage)
- Automatic expiry and cleanup

```
┌─────────────────────────────────────────────────────┐
│                  SessionManager                      │
├─────────────────────────────────────────────────────┤
│                                                      │
│  ┌─────────────────────────────────────────────┐   │
│  │              Active Sessions                 │   │
│  │                                              │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐    │   │
│  │  │ Session1 │ │ Session2 │ │ Session3 │    │   │
│  │  │ Auth:Yes │ │ Auth:No  │ │ Auth:Yes │    │   │
│  │  │ User:abc │ │ Anon     │ │ User:def │    │   │
│  │  └──────────┘ └──────────┘ └──────────┘    │   │
│  │                                              │   │
│  └─────────────────────────────────────────────┘   │
│                                                      │
│  ┌─────────────────────────────────────────────┐   │
│  │         Cleanup Thread                       │   │
│  │   • Expire inactive sessions                 │   │
│  │   • Remove terminated sessions               │   │
│  └─────────────────────────────────────────────┘   │
│                                                      │
└─────────────────────────────────────────────────────┘
```

## Integration with Other Modules

### Dependencies

```
void_services
    │
    ├── void_core
    │   ├── Error types
    │   ├── Result<T>
    │   └── Version info
    │
    └── void_event
        └── Event dispatching (optional integration)
```

### Users

```
void_engine
    │
    ├── void_audio (as a service)
    ├── void_asset (as a service)
    ├── void_physics (as a service)
    └── void_scripting (as a service)
```

## Service Lifecycle

```
        ┌─────────────┐
        │   Stopped   │◄────────────────────────┐
        └──────┬──────┘                         │
               │ start()                        │
               ▼                                │
        ┌─────────────┐                         │
        │  Starting   │                         │
        └──────┬──────┘                         │
               │                                │
    ┌──────────┴──────────┐                    │
    │                     │                    │
    ▼                     ▼                    │
┌─────────┐         ┌─────────┐               │
│ Running │◄───────►│ Degraded│               │
└────┬────┘         └────┬────┘               │
     │ stop()            │                    │
     └─────────┬─────────┘                    │
               ▼                               │
        ┌─────────────┐                        │
        │  Stopping   │                        │
        └──────┬──────┘                        │
               │                               │
               └───────────────────────────────┘
                      │
                      ▼ (on error)
               ┌─────────────┐
               │   Failed    │
               └─────────────┘
```

## Hot-Reload Support

void_services provides full hot-reload support:

1. **Snapshot Capture**: Serialize registry, sessions, and event bus state
2. **Binary Format**: Compact, versioned binary serialization
3. **State Restoration**: Restore service states after reload
4. **Session Preservation**: Maintain active sessions across reloads

```
Before Reload                    After Reload
┌────────────────┐              ┌────────────────┐
│ ServiceRegistry│   snapshot   │ ServiceRegistry│
│   3 services   │ ──────────► │   3 services   │
│   2 running    │              │   2 running    │
└────────────────┘              └────────────────┘

┌────────────────┐              ┌────────────────┐
│ SessionManager │   snapshot   │ SessionManager │
│   5 sessions   │ ──────────► │   5 sessions   │
│   3 auth'd     │              │   3 auth'd     │
└────────────────┘              └────────────────┘

┌────────────────┐              ┌────────────────┐
│   EventBus     │   snapshot   │   EventBus     │
│   enabled:true │ ──────────► │   enabled:true │
│   (subs reset) │              │   (re-sub)     │
└────────────────┘              └────────────────┘
```

## Thread Safety

All components are designed for concurrent access:

| Component | Thread Safety | Lock Type |
|-----------|--------------|-----------|
| ServiceRegistry | Thread-safe | shared_mutex |
| EventBus | Thread-safe | shared_mutex + mutex |
| SessionManager | Thread-safe | shared_mutex |
| Session | Thread-safe | shared_mutex |

## File Structure

```
include/void_engine/services/
├── fwd.hpp           # Forward declarations
├── service.hpp       # IService, ServiceBase, ServiceConfig
├── registry.hpp      # ServiceRegistry
├── session.hpp       # Session, SessionManager
├── event_bus.hpp     # EventBus, TypedEvent
├── snapshot.hpp      # Hot-reload serialization
└── services.hpp      # Main include (prelude)

src/services/
├── CMakeLists.txt
├── service.cpp       # State utilities, dependency resolution
├── event_bus.cpp     # Statistics, category matching
└── services.cpp      # Module init, global instances
```
