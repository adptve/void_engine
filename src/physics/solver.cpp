/// @file solver.cpp
/// @brief Constraint solver implementation
///
/// This file provides the compilation unit for ConstraintSolver and related classes.
/// The sequential impulse solver is primarily header-only for inlining performance,
/// but this file ensures proper compilation and linkage.

#include <void_engine/physics/solver.hpp>
#include <void_engine/physics/body.hpp>

namespace void_physics {

// =============================================================================
// ConstraintSolver Non-Inline Methods
// =============================================================================

// The ConstraintSolver and joint constraint classes are implemented inline in
// the header for performance. Sequential impulse solving benefits from inlining
// due to the iterative nature and frequent vector/matrix operations.

// This file exists to:
// 1. Ensure the header compiles correctly as a standalone unit
// 2. Provide a location for any future non-inline implementations
// 3. Allow explicit template instantiations if needed

// Joint constraint implementations:
// - FixedJointConstraint: Locks relative position and orientation
// - DistanceJointConstraint: Maintains distance with optional spring
// - SpringJointConstraint: Spring force between anchor points
// - BallJointConstraint: Free rotation at anchor point
// - HingeJointConstraint: Rotation around single axis with motor support

// Note: All methods are currently inline in solver.hpp for optimal
// performance in tight simulation loops.

} // namespace void_physics
