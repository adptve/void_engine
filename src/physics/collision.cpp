/// @file collision.cpp
/// @brief Narrow phase collision detection implementation
///
/// This file provides the compilation unit for CollisionDetector.
/// The GJK/EPA implementation is primarily header-only for inlining performance,
/// but this file ensures proper compilation and linkage.

#include <void_engine/physics/collision.hpp>
#include <void_engine/physics/shape.hpp>

namespace void_physics {

// =============================================================================
// CollisionDetector Non-Inline Methods
// =============================================================================

// The CollisionDetector class is implemented inline in the header for performance.
// GJK and EPA algorithms benefit significantly from inlining due to the tight
// loops and frequent vector operations.

// This file exists to:
// 1. Ensure the header compiles correctly as a standalone unit
// 2. Provide a location for any future non-inline implementations
// 3. Allow explicit template instantiations if needed

// Note: All methods are currently static and inline in collision.hpp for optimal
// performance in tight simulation loops.

} // namespace void_physics
