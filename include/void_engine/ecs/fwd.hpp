#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_ecs
///
/// All ECS types are declared here for header dependency management.

#include <cstdint>
#include <cstddef>

namespace void_ecs {

// =============================================================================
// Core Types
// =============================================================================

/// Entity with generational index
struct Entity;

/// Entity allocation and lifetime management
class EntityAllocator;

/// Entity location within archetype storage
struct EntityLocation;

// =============================================================================
// Component Types
// =============================================================================

/// Unique component type identifier
struct ComponentId;

/// Component metadata (size, alignment, drop function)
struct ComponentInfo;

/// Registry of all component types
class ComponentRegistry;

/// Type-erased component storage
class ComponentStorage;

// =============================================================================
// Archetype Types
// =============================================================================

/// Unique archetype identifier
struct ArchetypeId;

/// Graph edge for archetype transitions
struct ArchetypeEdge;

/// Container for entities with identical component sets
class Archetype;

/// Manager for all archetypes
class Archetypes;

// =============================================================================
// Query Types
// =============================================================================

/// Component access mode
enum class Access : uint8_t;

/// Single component access requirement
struct ComponentAccess;

/// Query descriptor (builder pattern)
class QueryDescriptor;

/// Cached query state
class QueryState;

/// Iterator over archetype rows
template<bool IsConst>
class ArchetypeQueryIter;

/// Iterator over multiple archetypes
class QueryIter;

// =============================================================================
// System Types
// =============================================================================

/// Unique system identifier
struct SystemId;

/// Execution stage for systems
enum class SystemStage : uint8_t;

/// Resource access declaration
struct ResourceAccess;

/// System metadata
class SystemDescriptor;

/// System interface
class System;

/// System execution scheduler
class SystemScheduler;

/// Batch of parallel-safe systems
struct SystemBatch;

// =============================================================================
// World
// =============================================================================

/// The main ECS container
class World;

/// Fluent entity construction
template<typename WorldT>
class EntityBuilder;

// =============================================================================
// Common Type Aliases
// =============================================================================

using EntityIndex = std::uint32_t;
using Generation = std::uint32_t;

} // namespace void_ecs
