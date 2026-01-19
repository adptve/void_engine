//! Service registry
//!
//! Manages service lifecycle and provides service discovery.

use std::collections::HashMap;
use std::sync::Arc;
use parking_lot::RwLock;

use crate::service::{Service, ServiceId, ServiceState, ServiceHealth, ServiceError, ServiceResult};

/// Service wrapper with metadata
struct ServiceEntry {
    /// The service instance
    service: Box<dyn Service>,
    /// Restart count
    restart_count: u32,
    /// Registration order (for ordered startup/shutdown)
    order: usize,
}

/// Service registry - manages all system services
pub struct ServiceRegistry {
    /// Registered services
    services: HashMap<ServiceId, ServiceEntry>,
    /// Registration counter
    next_order: usize,
    /// Global enabled flag
    enabled: bool,
}

impl ServiceRegistry {
    /// Create a new empty registry
    pub fn new() -> Self {
        Self {
            services: HashMap::new(),
            next_order: 0,
            enabled: true,
        }
    }

    /// Register a service
    pub fn register(&mut self, service: Box<dyn Service>) -> ServiceResult<()> {
        let id = service.id().clone();

        if self.services.contains_key(&id) {
            return Err(ServiceError::AlreadyExists(id.to_string()));
        }

        let entry = ServiceEntry {
            service,
            restart_count: 0,
            order: self.next_order,
        };
        self.next_order += 1;

        self.services.insert(id, entry);
        Ok(())
    }

    /// Unregister a service
    pub fn unregister(&mut self, id: &ServiceId) -> ServiceResult<Box<dyn Service>> {
        let mut entry = self.services.remove(id)
            .ok_or_else(|| ServiceError::NotFound(id.to_string()))?;

        // Stop if running
        if entry.service.state().is_operational() {
            let _ = entry.service.stop();
        }

        Ok(entry.service)
    }

    /// Get a service by ID (immutable)
    pub fn get(&self, id: &ServiceId) -> Option<&dyn Service> {
        self.services.get(id).map(|e| e.service.as_ref())
    }

    /// Get a service by ID (mutable)
    pub fn get_mut(&mut self, id: &ServiceId) -> Option<&mut Box<dyn Service>> {
        self.services.get_mut(id).map(|e| &mut e.service)
    }

    /// Get a typed service reference
    pub fn get_typed<T: Service + 'static>(&self, id: &ServiceId) -> Option<&T> {
        self.services.get(id)
            .and_then(|e| e.service.as_any().downcast_ref::<T>())
    }

    /// Get a typed service reference (mutable)
    pub fn get_typed_mut<T: Service + 'static>(&mut self, id: &ServiceId) -> Option<&mut T> {
        self.services.get_mut(id)
            .and_then(|e| e.service.as_any_mut().downcast_mut::<T>())
    }

    /// Start a specific service
    pub fn start(&mut self, id: &ServiceId) -> ServiceResult<()> {
        let entry = self.services.get_mut(id)
            .ok_or_else(|| ServiceError::NotFound(id.to_string()))?;

        entry.service.start()
    }

    /// Stop a specific service
    pub fn stop(&mut self, id: &ServiceId) -> ServiceResult<()> {
        let entry = self.services.get_mut(id)
            .ok_or_else(|| ServiceError::NotFound(id.to_string()))?;

        entry.service.stop()
    }

    /// Start all registered services (in registration order)
    pub fn start_all(&mut self) -> ServiceResult<()> {
        if !self.enabled {
            return Ok(());
        }

        // Get IDs sorted by registration order
        let mut ordered: Vec<_> = self.services.iter()
            .map(|(id, entry)| (id.clone(), entry.order))
            .collect();
        ordered.sort_by_key(|(_, order)| *order);

        // Start in order
        for (id, _) in ordered {
            if let Some(entry) = self.services.get_mut(&id) {
                if entry.service.state() == ServiceState::Stopped {
                    entry.service.start()?;
                }
            }
        }

        Ok(())
    }

    /// Stop all registered services (in reverse registration order)
    pub fn stop_all(&mut self) -> ServiceResult<()> {
        // Get IDs sorted by registration order (reversed)
        let mut ordered: Vec<_> = self.services.iter()
            .map(|(id, entry)| (id.clone(), entry.order))
            .collect();
        ordered.sort_by_key(|(_, order)| std::cmp::Reverse(*order));

        // Stop in reverse order
        for (id, _) in ordered {
            if let Some(entry) = self.services.get_mut(&id) {
                if entry.service.state().is_operational() {
                    let _ = entry.service.stop();
                }
            }
        }

        Ok(())
    }

    /// Restart a specific service
    pub fn restart(&mut self, id: &ServiceId) -> ServiceResult<()> {
        let entry = self.services.get_mut(id)
            .ok_or_else(|| ServiceError::NotFound(id.to_string()))?;

        entry.restart_count += 1;
        entry.service.restart()
    }

    /// Get health status of all services
    pub fn health_all(&self) -> HashMap<ServiceId, ServiceHealth> {
        self.services.iter()
            .map(|(id, entry)| (id.clone(), entry.service.health()))
            .collect()
    }

    /// Get health status of a specific service
    pub fn health(&self, id: &ServiceId) -> Option<ServiceHealth> {
        self.services.get(id).map(|e| e.service.health())
    }

    /// Get all service IDs
    pub fn service_ids(&self) -> Vec<ServiceId> {
        self.services.keys().cloned().collect()
    }

    /// Get number of registered services
    pub fn len(&self) -> usize {
        self.services.len()
    }

    /// Check if registry is empty
    pub fn is_empty(&self) -> bool {
        self.services.is_empty()
    }

    /// Check if a service exists
    pub fn contains(&self, id: &ServiceId) -> bool {
        self.services.contains_key(id)
    }

    /// Enable/disable the registry
    pub fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    /// Check if registry is enabled
    pub fn is_enabled(&self) -> bool {
        self.enabled
    }

    /// Get restart count for a service
    pub fn restart_count(&self, id: &ServiceId) -> Option<u32> {
        self.services.get(id).map(|e| e.restart_count)
    }

    /// Check health and restart failed services if configured
    pub fn check_and_restart_failed(&mut self) {
        let failed: Vec<ServiceId> = self.services.iter()
            .filter(|(_, entry)| {
                entry.service.state() == ServiceState::Failed
                    && entry.service.config().auto_restart
                    && entry.restart_count < entry.service.config().max_restarts
            })
            .map(|(id, _)| id.clone())
            .collect();

        for id in failed {
            log::info!("Auto-restarting failed service: {}", id);
            if let Err(e) = self.restart(&id) {
                log::error!("Failed to restart service {}: {}", id, e);
            }
        }
    }
}

impl Default for ServiceRegistry {
    fn default() -> Self {
        Self::new()
    }
}

/// Thread-safe service registry wrapper
pub struct SharedServiceRegistry {
    inner: Arc<RwLock<ServiceRegistry>>,
}

impl SharedServiceRegistry {
    /// Create a new shared registry
    pub fn new() -> Self {
        Self {
            inner: Arc::new(RwLock::new(ServiceRegistry::new())),
        }
    }

    /// Get a read lock
    pub fn read(&self) -> parking_lot::RwLockReadGuard<'_, ServiceRegistry> {
        self.inner.read()
    }

    /// Get a write lock
    pub fn write(&self) -> parking_lot::RwLockWriteGuard<'_, ServiceRegistry> {
        self.inner.write()
    }

    /// Clone the Arc
    pub fn clone_arc(&self) -> Self {
        Self {
            inner: Arc::clone(&self.inner),
        }
    }
}

impl Default for SharedServiceRegistry {
    fn default() -> Self {
        Self::new()
    }
}

impl Clone for SharedServiceRegistry {
    fn clone(&self) -> Self {
        self.clone_arc()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::any::Any;
    use crate::service::{ServiceConfig, ServiceHealth};

    struct TestService {
        config: ServiceConfig,
        state: ServiceState,
    }

    impl TestService {
        fn new(id: &str) -> Self {
            Self {
                config: ServiceConfig::new(id),
                state: ServiceState::Stopped,
            }
        }
    }

    impl Service for TestService {
        fn id(&self) -> &ServiceId {
            &self.config.id
        }

        fn state(&self) -> ServiceState {
            self.state
        }

        fn health(&self) -> ServiceHealth {
            if self.state.is_operational() {
                ServiceHealth::healthy()
            } else {
                ServiceHealth::default()
            }
        }

        fn config(&self) -> &ServiceConfig {
            &self.config
        }

        fn start(&mut self) -> ServiceResult<()> {
            self.state = ServiceState::Running;
            Ok(())
        }

        fn stop(&mut self) -> ServiceResult<()> {
            self.state = ServiceState::Stopped;
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
    fn test_register_service() {
        let mut registry = ServiceRegistry::new();
        let service = Box::new(TestService::new("test"));

        assert!(registry.register(service).is_ok());
        assert!(registry.contains(&ServiceId::new("test")));
        assert_eq!(registry.len(), 1);
    }

    #[test]
    fn test_register_duplicate() {
        let mut registry = ServiceRegistry::new();
        registry.register(Box::new(TestService::new("test"))).unwrap();

        let result = registry.register(Box::new(TestService::new("test")));
        assert!(matches!(result, Err(ServiceError::AlreadyExists(_))));
    }

    #[test]
    fn test_start_stop_service() {
        let mut registry = ServiceRegistry::new();
        registry.register(Box::new(TestService::new("test"))).unwrap();

        let id = ServiceId::new("test");

        registry.start(&id).unwrap();
        assert_eq!(registry.get(&id).unwrap().state(), ServiceState::Running);

        registry.stop(&id).unwrap();
        assert_eq!(registry.get(&id).unwrap().state(), ServiceState::Stopped);
    }

    #[test]
    fn test_start_stop_all() {
        let mut registry = ServiceRegistry::new();
        registry.register(Box::new(TestService::new("a"))).unwrap();
        registry.register(Box::new(TestService::new("b"))).unwrap();
        registry.register(Box::new(TestService::new("c"))).unwrap();

        registry.start_all().unwrap();

        let health = registry.health_all();
        assert!(health.values().all(|h| h.state == ServiceState::Running));

        registry.stop_all().unwrap();

        let health = registry.health_all();
        assert!(health.values().all(|h| h.state == ServiceState::Stopped));
    }

    #[test]
    fn test_unregister() {
        let mut registry = ServiceRegistry::new();
        registry.register(Box::new(TestService::new("test"))).unwrap();

        let id = ServiceId::new("test");
        let service = registry.unregister(&id).unwrap();
        assert_eq!(service.id().name(), "test");
        assert!(!registry.contains(&id));
    }

    #[test]
    fn test_get_typed() {
        let mut registry = ServiceRegistry::new();
        registry.register(Box::new(TestService::new("test"))).unwrap();

        let id = ServiceId::new("test");
        let typed = registry.get_typed::<TestService>(&id);
        assert!(typed.is_some());
    }

    #[test]
    fn test_shared_registry() {
        let shared = SharedServiceRegistry::new();

        {
            let mut reg = shared.write();
            reg.register(Box::new(TestService::new("test"))).unwrap();
        }

        {
            let reg = shared.read();
            assert!(reg.contains(&ServiceId::new("test")));
        }
    }
}
