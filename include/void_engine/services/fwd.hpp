#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_services

#include <cstdint>
#include <string>

namespace void_services {

// Service types
class IService;
class ServiceRegistry;
struct ServiceId;
struct ServiceHealth;

// Session types
class Session;
class SessionManager;
struct SessionId;

// Network types
class ITransport;
class Connection;
struct ConnectionId;

// Replication types
struct ReplicatedEntity;
enum class Authority;
enum class ConsistencyLevel;

} // namespace void_services
