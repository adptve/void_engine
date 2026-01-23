#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_memory

namespace void_memory {

// Allocators
class Arena;
class Pool;
class StackAllocator;
class FreeList;

// Typed wrappers
template<typename T> class TypedPool;

// Helpers
struct ArenaState;
struct StackMarker;
struct PoolStats;
struct FreeListStats;

// Scopes
template<typename Allocator> class AllocatorScope;

// Placement policies
enum class PlacementPolicy;

} // namespace void_memory
