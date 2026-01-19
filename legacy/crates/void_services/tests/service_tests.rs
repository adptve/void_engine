//! Integration tests for void_services

use void_services::*;
use std::any::Any;
use std::collections::HashMap;

// Mock service for testing
struct MockService {
    config: service::ServiceConfig,
    state: service::ServiceState,
    start_count: usize,
    stop_count: usize,
}

impl MockService {
    fn new(id: &str) -> Self {
        Self {
            config: service::ServiceConfig::new(id),
            state: service::ServiceState::Stopped,
            start_count: 0,
            stop_count: 0,
        }
    }

    fn with_auto_restart(mut self, enabled: bool) -> Self {
        self.config.auto_restart = enabled;
        self
    }
}

impl Service for MockService {
    fn id(&self) -> &ServiceId {
        &self.config.id
    }

    fn state(&self) -> ServiceState {
        self.state
    }

    fn health(&self) -> service::ServiceHealth {
        match self.state {
            ServiceState::Running => service::ServiceHealth::healthy(),
            ServiceState::Failed => service::ServiceHealth::failed("Mock failure"),
            ServiceState::Degraded => service::ServiceHealth::degraded("Mock degradation"),
            _ => service::ServiceHealth::default(),
        }
    }

    fn config(&self) -> &service::ServiceConfig {
        &self.config
    }

    fn start(&mut self) -> ServiceResult<()> {
        if self.state == ServiceState::Running {
            return Err(ServiceError::Internal("Already running".to_string()));
        }
        self.state = ServiceState::Running;
        self.start_count += 1;
        Ok(())
    }

    fn stop(&mut self) -> ServiceResult<()> {
        if self.state == ServiceState::Stopped {
            return Err(ServiceError::Internal("Already stopped".to_string()));
        }
        self.state = ServiceState::Stopped;
        self.stop_count += 1;
        Ok(())
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

#[test]
fn test_service_state_checks() {
    assert!(ServiceState::Running.is_operational());
    assert!(ServiceState::Degraded.is_operational());
    assert!(!ServiceState::Stopped.is_operational());
    assert!(!ServiceState::Failed.is_operational());

    assert!(ServiceState::Starting.is_transitioning());
    assert!(ServiceState::Stopping.is_transitioning());
    assert!(!ServiceState::Running.is_transitioning());
}

#[test]
fn test_service_health_creation() {
    let healthy = service::ServiceHealth::healthy();
    assert_eq!(healthy.state, ServiceState::Running);
    assert!((healthy.health_score - 1.0).abs() < 0.001);

    let degraded = service::ServiceHealth::degraded("Low memory");
    assert_eq!(degraded.state, ServiceState::Degraded);
    assert_eq!(degraded.last_error.as_deref(), Some("Low memory"));

    let failed = service::ServiceHealth::failed("Crash");
    assert_eq!(failed.state, ServiceState::Failed);
    assert!((failed.health_score - 0.0).abs() < 0.001);
}

#[test]
fn test_registry_register_unregister() {
    let mut registry = ServiceRegistry::new();

    let service = Box::new(MockService::new("test"));
    registry.register(service).unwrap();

    assert!(registry.contains(&ServiceId::new("test")));
    assert_eq!(registry.len(), 1);

    let _removed = registry.unregister(&ServiceId::new("test")).unwrap();
    assert!(!registry.contains(&ServiceId::new("test")));
    assert_eq!(registry.len(), 0);
}

#[test]
fn test_registry_duplicate_registration() {
    let mut registry = ServiceRegistry::new();

    registry.register(Box::new(MockService::new("test"))).unwrap();
    let result = registry.register(Box::new(MockService::new("test")));

    assert!(matches!(result, Err(ServiceError::AlreadyExists(_))));
}

#[test]
fn test_service_start_stop() {
    let mut registry = ServiceRegistry::new();
    registry.register(Box::new(MockService::new("test"))).unwrap();

    let id = ServiceId::new("test");

    // Start service
    registry.start(&id).unwrap();
    let service = registry.get(&id).unwrap();
    assert_eq!(service.state(), ServiceState::Running);

    // Stop service
    registry.stop(&id).unwrap();
    let service = registry.get(&id).unwrap();
    assert_eq!(service.state(), ServiceState::Stopped);
}

#[test]
fn test_start_all_services() {
    let mut registry = ServiceRegistry::new();

    registry.register(Box::new(MockService::new("service_a"))).unwrap();
    registry.register(Box::new(MockService::new("service_b"))).unwrap();
    registry.register(Box::new(MockService::new("service_c"))).unwrap();

    registry.start_all().unwrap();

    // All services should be running
    let health = registry.health_all();
    assert_eq!(health.len(), 3);
    assert!(health.values().all(|h| h.state == ServiceState::Running));
}

#[test]
fn test_stop_all_services() {
    let mut registry = ServiceRegistry::new();

    registry.register(Box::new(MockService::new("service_a"))).unwrap();
    registry.register(Box::new(MockService::new("service_b"))).unwrap();

    registry.start_all().unwrap();
    registry.stop_all().unwrap();

    // All services should be stopped
    let health = registry.health_all();
    assert!(health.values().all(|h| h.state == ServiceState::Stopped));
}

#[test]
fn test_service_restart() {
    let mut registry = ServiceRegistry::new();
    registry.register(Box::new(MockService::new("test"))).unwrap();

    let id = ServiceId::new("test");

    registry.start(&id).unwrap();
    registry.restart(&id).unwrap();

    let service = registry.get(&id).unwrap();
    assert_eq!(service.state(), ServiceState::Running);

    // Check restart count
    assert_eq!(registry.restart_count(&id), Some(1));
}

#[test]
fn test_typed_service_access() {
    let mut registry = ServiceRegistry::new();
    registry.register(Box::new(MockService::new("test"))).unwrap();

    let id = ServiceId::new("test");

    // Downcast to concrete type
    let typed = registry.get_typed::<MockService>(&id);
    assert!(typed.is_some());
    assert_eq!(typed.unwrap().start_count, 0);

    // Start and check count
    registry.start(&id).unwrap();
    let typed = registry.get_typed::<MockService>(&id);
    assert_eq!(typed.unwrap().start_count, 1);
}

#[test]
fn test_service_health_check() {
    let mut registry = ServiceRegistry::new();
    registry.register(Box::new(MockService::new("test"))).unwrap();

    let id = ServiceId::new("test");

    // Initial health
    let health = registry.health(&id).unwrap();
    assert_eq!(health.state, ServiceState::Stopped);

    // Start and check health
    registry.start(&id).unwrap();
    let health = registry.health(&id).unwrap();
    assert_eq!(health.state, ServiceState::Running);
    assert!((health.health_score - 1.0).abs() < 0.001);
}

#[test]
fn test_registry_enabled_flag() {
    let mut registry = ServiceRegistry::new();
    assert!(registry.is_enabled());

    registry.set_enabled(false);
    assert!(!registry.is_enabled());

    // start_all should be no-op when disabled
    registry.register(Box::new(MockService::new("test"))).unwrap();
    registry.start_all().unwrap();

    let id = ServiceId::new("test");
    let service = registry.get(&id).unwrap();
    assert_eq!(service.state(), ServiceState::Stopped);
}

#[test]
fn test_service_ids() {
    let mut registry = ServiceRegistry::new();

    registry.register(Box::new(MockService::new("a"))).unwrap();
    registry.register(Box::new(MockService::new("b"))).unwrap();
    registry.register(Box::new(MockService::new("c"))).unwrap();

    let ids = registry.service_ids();
    assert_eq!(ids.len(), 3);
    assert!(ids.contains(&ServiceId::new("a")));
    assert!(ids.contains(&ServiceId::new("b")));
    assert!(ids.contains(&ServiceId::new("c")));
}

#[test]
fn test_shared_registry() {
    use void_services::registry::SharedServiceRegistry;

    let shared = SharedServiceRegistry::new();

    // Write lock to add service
    {
        let mut registry = shared.write();
        registry.register(Box::new(MockService::new("test"))).unwrap();
    }

    // Read lock to check
    {
        let registry = shared.read();
        assert!(registry.contains(&ServiceId::new("test")));
    }

    // Clone the shared registry
    let cloned = shared.clone();
    {
        let registry = cloned.read();
        assert!(registry.contains(&ServiceId::new("test")));
    }
}

#[test]
fn test_service_dependency() {
    let required = service::ServiceDependency::required("database");
    assert!(required.required);
    assert_eq!(required.service_id.name(), "database");

    let optional = service::ServiceDependency::optional("cache");
    assert!(!optional.required);
    assert_eq!(optional.service_id.name(), "cache");
}

#[test]
fn test_service_config_defaults() {
    let config = service::ServiceConfig::new("my-service");

    assert_eq!(config.id.name(), "my-service");
    assert!(config.auto_restart);
    assert_eq!(config.max_restarts, 3);
    assert!(config.health_check_interval_ms > 0);
}

#[test]
fn test_event_bus_creation() {
    let bus = EventBus::new();
    assert_eq!(bus.pending_count(), 0);
}

#[test]
fn test_event_bus_publish_subscribe() {
    let mut bus = EventBus::new();

    let events_received = std::sync::Arc::new(std::sync::Mutex::new(Vec::new()));
    let events_clone = events_received.clone();

    bus.subscribe("test_event", Box::new(move |event| {
        events_clone.lock().unwrap().push(event.name.clone());
    }));

    bus.publish(Event::new("test_event", serde_json::json!({"data": 42})));
    bus.dispatch();

    let received = events_received.lock().unwrap();
    assert_eq!(received.len(), 1);
    assert_eq!(received[0], "test_event");
}

#[test]
fn test_unregister_running_service() {
    let mut registry = ServiceRegistry::new();
    registry.register(Box::new(MockService::new("test"))).unwrap();

    let id = ServiceId::new("test");
    registry.start(&id).unwrap();

    // Unregister should stop the service first
    let service = registry.unregister(&id).unwrap();
    assert_eq!(service.state(), ServiceState::Stopped);
}

#[test]
fn test_service_not_found_errors() {
    let mut registry = ServiceRegistry::new();
    let id = ServiceId::new("nonexistent");

    assert!(matches!(
        registry.start(&id),
        Err(ServiceError::NotFound(_))
    ));

    assert!(matches!(
        registry.stop(&id),
        Err(ServiceError::NotFound(_))
    ));

    assert!(matches!(
        registry.restart(&id),
        Err(ServiceError::NotFound(_))
    ));

    assert!(matches!(
        registry.unregister(&id),
        Err(ServiceError::NotFound(_))
    ));
}

#[test]
fn test_auto_restart_failed_services() {
    let mut registry = ServiceRegistry::new();

    // Create service with auto-restart enabled
    let service = Box::new(MockService::new("auto_restart").with_auto_restart(true));
    registry.register(service).unwrap();

    let id = ServiceId::new("auto_restart");

    // Start service
    registry.start(&id).unwrap();

    // Simulate failure
    if let Some(service) = registry.get_typed_mut::<MockService>(&id) {
        service.state = ServiceState::Failed;
    }

    // Check and restart failed services
    registry.check_and_restart_failed();

    // Service should be running again
    let health = registry.health(&id).unwrap();
    assert_eq!(health.state, ServiceState::Running);
}

#[test]
fn test_registration_order_preserved() {
    let mut registry = ServiceRegistry::new();

    registry.register(Box::new(MockService::new("first"))).unwrap();
    registry.register(Box::new(MockService::new("second"))).unwrap();
    registry.register(Box::new(MockService::new("third"))).unwrap();

    // Services should start in registration order
    registry.start_all().unwrap();

    // Check start counts match registration order
    let first = registry.get_typed::<MockService>(&ServiceId::new("first")).unwrap();
    let second = registry.get_typed::<MockService>(&ServiceId::new("second")).unwrap();
    let third = registry.get_typed::<MockService>(&ServiceId::new("third")).unwrap();

    assert_eq!(first.start_count, 1);
    assert_eq!(second.start_count, 1);
    assert_eq!(third.start_count, 1);
}
