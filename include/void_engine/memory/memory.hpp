#pragma once

/// @file memory.hpp
/// @brief Main header for void_memory - custom memory allocators
///
/// High-performance memory allocators for game engine use cases:
/// - Arena: Linear allocation, bulk deallocation
/// - Pool: Fixed-size block allocation
/// - StackAllocator: LIFO allocation with markers
/// - FreeList: General-purpose with fragmentation management

#include "fwd.hpp"
#include "allocator.hpp"
#include "arena.hpp"
#include "pool.hpp"
#include "stack.hpp"
#include "free_list.hpp"

namespace void_memory {

/// Prelude namespace for commonly used types
namespace prelude {
    using void_memory::IAllocator;
    using void_memory::Arena;
    using void_memory::ArenaScope;
    using void_memory::Pool;
    using void_memory::TypedPool;
    using void_memory::StackAllocator;
    using void_memory::StackScope;
    using void_memory::FreeList;
    using void_memory::PlacementPolicy;
    using void_memory::align_up;
    using void_memory::align_down;
    using void_memory::is_aligned;
}

} // namespace void_memory
