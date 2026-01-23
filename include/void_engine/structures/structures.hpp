#pragma once

/// @file structures.hpp
/// @brief Main include for void_structures module
///
/// This header includes all void_structures components:
/// - SlotMap<T> / SlotKey<T>: Generational index arena
/// - SparseSet<T>: Cache-friendly component storage
/// - BitSet: Compact bit-level storage
/// - LockFreeQueue<T>: Unbounded MPMC queue
/// - BoundedQueue<T>: Fixed-capacity ring buffer
///
/// @example Basic usage:
/// @code
/// #include <void_engine/structures/structures.hpp>
///
/// using namespace void_structures;
///
/// // Generational arena for stable handles
/// SlotMap<Entity> entities;
/// SlotKey<Entity> key = entities.insert({});
///
/// // Cache-friendly component storage
/// SparseSet<Position> positions;
/// positions.insert(entity_id, {0.0f, 0.0f, 0.0f});
///
/// // Compact bit flags
/// BitSet components(64);
/// components.set(0);  // Entity has component 0
///
/// // Thread-safe job queue
/// LockFreeQueue<Job> jobs;
/// jobs.push(my_job);
/// @endcode

#include "fwd.hpp"
#include "slot_map.hpp"
#include "sparse_set.hpp"
#include "bitset.hpp"
#include "lock_free_queue.hpp"
#include "bounded_queue.hpp"

namespace void_structures {

/// Version information
struct Version {
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;
    static constexpr int PATCH = 0;
};

} // namespace void_structures
