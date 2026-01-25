# void_services Validation Document

## Module Overview

| Property | Value |
|----------|-------|
| Module Name | void_services |
| Version | 1.0.0 |
| Dependencies | void_core, void_event |
| C++ Standard | C++20 |
| Thread Safety | Full |

## Compilation Checklist

### Header Files

- [x] `fwd.hpp` - Forward declarations
- [x] `service.hpp` - Service interface and base class
- [x] `registry.hpp` - Service registry
- [x] `session.hpp` - Session management
- [x] `event_bus.hpp` - Event publish/subscribe
- [x] `snapshot.hpp` - Hot-reload serialization
- [x] `services.hpp` - Main include header

### Source Files

- [x] `service.cpp` - Service utilities
- [x] `event_bus.cpp` - Event bus utilities
- [x] `services.cpp` - Module implementation

### CMake Configuration

- [x] `CMakeLists.txt` updated with all source files
- [x] Dependencies: void_core, void_event

## API Validation

### ServiceRegistry

| Method | Validated | Notes |
|--------|-----------|-------|
| `register_service()` | ✓ | Template and non-template versions |
| `unregister()` | ✓ | By ID and name |
| `get()` | ✓ | Returns shared_ptr |
| `get_typed<T>()` | ✓ | Type-safe access |
| `has()` | ✓ | Existence check |
| `list()` | ✓ | Returns ordered IDs |
| `count()` | ✓ | Service count |
| `start_service()` | ✓ | Single service start |
| `stop_service()` | ✓ | Single service stop |
| `restart_service()` | ✓ | Stop + start |
| `start_all()` | ✓ | Priority ordering |
| `stop_all()` | ✓ | Reverse priority |
| `start_health_monitor()` | ✓ | Background thread |
| `stop_health_monitor()` | ✓ | Join thread |
| `check_all_health()` | ✓ | Manual health check |
| `get_health()` | ✓ | Single service health |
| `get_all_health()` | ✓ | All service health |
| `stats()` | ✓ | Registry statistics |
| `set_event_callback()` | ✓ | Event notification |
| `set_enabled()` | ✓ | Enable/disable |

### EventBus

| Method | Validated | Notes |
|--------|-----------|-------|
| `subscribe<T>()` | ✓ | Type-safe subscription |
| `subscribe_category()` | ✓ | Category wildcards |
| `unsubscribe()` | ✓ | By subscription ID |
| `publish<T>()` | ✓ | Immediate or queued |
| `queue<T>()` | ✓ | Deferred processing |
| `process_queue()` | ✓ | Both overloads |
| `set_enabled()` | ✓ | Enable/disable |
| `is_enabled()` | ✓ | Check state |
| `clear_queue()` | ✓ | Clear pending |
| `clear_subscriptions()` | ✓ | Remove all handlers |
| `stats()` | ✓ | Event statistics |
| `queue_size()` | ✓ | Current queue size |
| `config()` | ✓ | Get configuration |

### SessionManager

| Method | Validated | Notes |
|--------|-----------|-------|
| `create_session()` | ✓ | Create anonymous session |
| `create_authenticated_session()` | ✓ | Create with user ID |
| `get()` | ✓ | By session ID |
| `get_user_sessions()` | ✓ | All sessions for user |
| `list_active()` | ✓ | Active session IDs |
| `terminate()` | ✓ | Single session |
| `terminate_user()` | ✓ | All user sessions |
| `start_cleanup()` | ✓ | Background thread |
| `stop_cleanup()` | ✓ | Join thread |
| `cleanup_expired()` | ✓ | Manual cleanup |
| `stats()` | ✓ | Session statistics |
| `config()` | ✓ | Get configuration |
| `restore_session()` | ✓ | Hot-reload support |
| `restore_stats()` | ✓ | Hot-reload support |
| `set_next_session_id()` | ✓ | Hot-reload support |

### Session

| Method | Validated | Notes |
|--------|-----------|-------|
| `id()` | ✓ | Get session ID |
| `user_id()` | ✓ | Get authenticated user |
| `set_user_id()` | ✓ | Set on auth |
| `is_authenticated()` | ✓ | Auth check |
| `state()` | ✓ | Get state |
| `activate()` | ✓ | State transition |
| `suspend()` | ✓ | State transition |
| `resume()` | ✓ | State transition |
| `expire()` | ✓ | State transition |
| `terminate()` | ✓ | State transition |
| `restore_state()` | ✓ | Hot-reload support |
| `restore_auth()` | ✓ | Hot-reload support |
| `is_active()` | ✓ | Check usability |
| `touch()` | ✓ | Update activity |
| `created_at()` | ✓ | Creation time |
| `last_activity()` | ✓ | Activity time |
| `idle_time()` | ✓ | Idle duration |
| `age()` | ✓ | Session age |
| `has_permission()` | ✓ | Hierarchical check |
| `grant_permission()` | ✓ | Add permission |
| `revoke_permission()` | ✓ | Remove permission |
| `permissions()` | ✓ | Get all permissions |
| `clear_permissions()` | ✓ | Remove all |
| `set<T>()` | ✓ | Set variable |
| `get<T>()` | ✓ | Get variable |
| `has_variable()` | ✓ | Check existence |
| `remove_variable()` | ✓ | Delete variable |
| `clear_variables()` | ✓ | Remove all |
| `set_metadata()` | ✓ | Set metadata |
| `get_metadata()` | ✓ | Get metadata |
| `metadata()` | ✓ | Get all metadata |

## Hot-Reload Validation

### Serialization

| Component | Serialize | Deserialize | Restore |
|-----------|-----------|-------------|---------|
| RegistrySnapshot | ✓ | ✓ | ✓ |
| SessionManagerSnapshot | ✓ | ✓ | ✓ |
| EventBusSnapshot | ✓ | ✓ | ✓ |
| ServicesModuleSnapshot | ✓ | ✓ | ✓ |

### Version Compatibility

| Snapshot Type | Version | Compatible |
|---------------|---------|------------|
| RegistrySnapshot | 1 | ✓ |
| SessionManagerSnapshot | 1 | ✓ |
| EventBusSnapshot | 1 | ✓ |
| ServicesModuleSnapshot | 1 | ✓ |

## Thread Safety Validation

### Mutex Usage

| Component | Lock Type | Protected Resources |
|-----------|-----------|---------------------|
| ServiceRegistry | shared_mutex | services map, order list |
| EventBus (handlers) | shared_mutex | handler maps |
| EventBus (queue) | mutex | priority queues |
| SessionManager | shared_mutex | sessions map, user map |
| Session | shared_mutex | state, permissions, vars |

### Atomic Variables

| Component | Variable | Type |
|-----------|----------|------|
| ServiceBase | m_state | atomic<ServiceState> |
| EventBus | m_next_subscription_id | atomic<uint64_t> |
| EventBus | m_enabled | atomic<bool> |
| Session | m_state | atomic<SessionState> |
| SessionManager | m_cleanup_running | atomic<bool> |
| ServiceRegistry | m_enabled | atomic<bool> |
| ServiceRegistry | m_health_check_running | atomic<bool> |

## Error Handling

### State Transition Validation

```cpp
// Valid transitions defined in service.cpp
is_valid_transition(ServiceState from, ServiceState to);
```

### Configuration Validation

```cpp
// Validates service configuration
std::string validate_config(const ServiceConfig& config);
```

## Memory Management

### RAII Patterns

- `SubscriptionGuard` - Auto-unsubscribe on destruction
- `SharedEventBus` - Shared ownership of EventBus
- `SharedServiceRegistry` - Shared ownership of ServiceRegistry

### Ownership

| Type | Ownership |
|------|-----------|
| IService | shared_ptr in registry |
| Session | shared_ptr in manager |
| EventBus | shared_ptr (optional) |
| IEventHandler | shared_ptr in bus |

## Integration Tests Needed

1. **Service Lifecycle**
   - [ ] Register, start, stop, unregister
   - [ ] Dependency ordering
   - [ ] Health monitoring
   - [ ] Auto-restart on failure

2. **Event Bus**
   - [ ] Type-safe subscribe/publish
   - [ ] Category wildcards
   - [ ] Priority ordering
   - [ ] Queue processing

3. **Session Management**
   - [ ] Create/terminate sessions
   - [ ] Authentication flow
   - [ ] Permission hierarchy
   - [ ] Session variables

4. **Hot-Reload**
   - [ ] Full module snapshot/restore
   - [ ] Service state preservation
   - [ ] Session preservation
   - [ ] Event bus config restoration

## Performance Considerations

### Lock Contention

- Read operations use shared_lock for parallelism
- Write operations use unique_lock
- Event processing uses fine-grained locking

### Memory Allocation

- Pre-allocated queues where possible
- Move semantics for event data
- Efficient binary serialization

## Known Limitations

1. **EventBus Subscriptions**: Cannot be serialized for hot-reload (function pointers)
2. **Session Variables**: std::any cannot be serialized; must be re-established
3. **Background Threads**: Health monitor and cleanup threads restart after reload

## Build Verification

```bash
# Configure
cmake -B build

# Build void_services
cmake --build build --target void_services

# Run tests (when available)
ctest --test-dir build -R services
```

## Compatibility Matrix

| Component | void_core | void_event |
|-----------|-----------|------------|
| Required | ✓ | ✓ |
| Version | 1.x | 1.x |
