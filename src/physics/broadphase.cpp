/// @file broadphase.cpp
/// @brief Broad phase collision detection implementation
///
/// This file provides the compilation unit for BroadPhaseBvh.
/// The BVH implementation is primarily header-only for inlining performance,
/// but this file ensures proper compilation and linkage.

#include <void_engine/physics/broadphase.hpp>
#include <void_engine/physics/collision.hpp>

namespace void_physics {

// =============================================================================
// BroadPhaseBvh Non-Inline Methods
// =============================================================================

// The BroadPhaseBvh class is implemented inline in the header for performance.
// This file exists to:
// 1. Ensure the header compiles correctly as a standalone unit
// 2. Provide a location for any future non-inline implementations
// 3. Allow explicit template instantiations if needed

// Note: All methods are currently inline in broadphase.hpp for optimal
// performance in tight simulation loops.

} // namespace void_physics
