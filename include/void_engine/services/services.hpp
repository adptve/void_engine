#pragma once

/// @file services.hpp
/// @brief Main include header for void_services
///
/// void_services provides service lifecycle management:
/// - IService interface for implementing services
/// - ServiceRegistry for registration and discovery
/// - Health monitoring with auto-restart
/// - Session management with permissions
///
/// ## Quick Start
///
/// ### Implementing a Service
/// ```cpp
/// class MyService : public void_services::ServiceBase {
/// public:
///     MyService() : ServiceBase("my_service") {}
///
/// protected:
///     bool on_start() override {
///         // Initialize service
///         return true;
///     }
///
///     void on_stop() override {
///         // Cleanup
///     }
///
///     float on_check_health() override {
///         // Return 0.0-1.0 health score
///         return 1.0f;
///     }
/// };
/// ```
///
/// ### Using the Registry
/// ```cpp
/// void_services::ServiceRegistry registry;
///
/// // Register services
/// auto my_service = registry.register_service<MyService>();
///
/// // Start all services
/// registry.start_all();
///
/// // Start health monitoring
/// registry.start_health_monitor();
///
/// // Get a service
/// auto service = registry.get_typed<MyService>("my_service");
///
/// // Stop all services
/// registry.stop_all();
/// ```
///
/// ### Session Management
/// ```cpp
/// void_services::SessionManager sessions;
///
/// // Create a session
/// auto session = sessions.create_session();
/// session->activate();
///
/// // Authenticate
/// session->set_user_id("user123");
///
/// // Check permissions
/// session->grant_permission("assets.read");
/// if (session->has_permission("assets.read")) {
///     // Access allowed
/// }
///
/// // Store session data
/// session->set("last_scene", std::string("level1"));
/// auto scene = session->get<std::string>("last_scene");
/// ```

#include "fwd.hpp"
#include "service.hpp"
#include "registry.hpp"
#include "session.hpp"

namespace void_services {

/// Prelude - commonly used types
namespace prelude {
    using void_services::IService;
    using void_services::ServiceBase;
    using void_services::ServiceId;
    using void_services::ServiceState;
    using void_services::ServiceHealth;
    using void_services::ServiceConfig;
    using void_services::ServiceRegistry;
    using void_services::ServiceEvent;
    using void_services::ServiceEventType;
    using void_services::Session;
    using void_services::SessionManager;
    using void_services::SessionId;
    using void_services::SessionState;
} // namespace prelude

} // namespace void_services
