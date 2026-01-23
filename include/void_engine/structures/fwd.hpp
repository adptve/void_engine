#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_structures types

#include <cstdint>
#include <cstddef>

namespace void_structures {

// =============================================================================
// Forward Declarations
// =============================================================================

/// Generational key for SlotMap
template<typename T>
struct SlotKey;

/// Generational index-based storage
template<typename T>
class SlotMap;

/// Cache-friendly sparse set with stable indices
template<typename T>
class SparseSet;

/// Efficient bit-level storage
class BitSet;

/// Unbounded lock-free multi-producer multi-consumer queue
template<typename T>
class LockFreeQueue;

/// Fixed-capacity lock-free ring buffer queue
template<typename T>
class BoundedQueue;

} // namespace void_structures
