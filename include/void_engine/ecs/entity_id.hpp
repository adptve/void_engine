/// @file entity_id.hpp
/// @brief Conversion utilities between void_ecs::Entity and void_core::EntityId
///
/// This header provides seamless conversion between ECS entity handles and the
/// canonical EntityId type used across all engine modules.
///
/// Architecture:
/// - void_ecs::Entity is the ECS-native entity handle (index + generation)
/// - void_core::EntityId is the canonical ID for cross-module communication
/// - Both use the same bit layout: [Generation(32 bits) | Index(32 bits)]
/// - Conversion is zero-cost (just bit reinterpretation)

#pragma once

#include <void_engine/core/id.hpp>
#include <void_engine/ecs/entity.hpp>

namespace void_ecs {

// =============================================================================
// Entity <-> EntityId Conversion
// =============================================================================

/// Convert ECS Entity to canonical EntityId
/// @param entity The ECS entity handle
/// @return Canonical EntityId for cross-module use
[[nodiscard]] inline constexpr void_core::EntityId to_entity_id(Entity entity) noexcept {
    return void_core::EntityId(entity.to_bits());
}

/// Convert canonical EntityId to ECS Entity
/// @param id The canonical entity ID
/// @return ECS entity handle
[[nodiscard]] inline constexpr Entity from_entity_id(void_core::EntityId id) noexcept {
    return Entity::from_bits(id.to_bits());
}

/// Convert ECS Entity to void_core::Id
/// @param entity The ECS entity handle
/// @return Generic ID for systems that use void_core::Id
[[nodiscard]] inline constexpr void_core::Id to_core_id(Entity entity) noexcept {
    return void_core::Id(entity.to_bits());
}

/// Convert void_core::Id to ECS Entity
/// @param id The generic ID
/// @return ECS entity handle
[[nodiscard]] inline constexpr Entity from_core_id(void_core::Id id) noexcept {
    return Entity::from_bits(id.to_bits());
}

// =============================================================================
// Entity Extensions (for convenience)
// =============================================================================

/// Extension trait providing EntityId conversion on Entity
/// Usage: entity.to_entity_id() via ADL
struct EntityIdConversion {
    /// Convert to canonical EntityId
    [[nodiscard]] static constexpr void_core::EntityId convert(Entity entity) noexcept {
        return to_entity_id(entity);
    }
};

} // namespace void_ecs

namespace void_core {

// =============================================================================
// EntityId Extensions
// =============================================================================

/// Convert EntityId to ECS Entity
/// Convenience function in void_core namespace for symmetry
[[nodiscard]] inline constexpr void_ecs::Entity to_ecs_entity(EntityId id) noexcept {
    return void_ecs::from_entity_id(id);
}

/// Create EntityId from ECS Entity
/// Convenience function in void_core namespace for symmetry
[[nodiscard]] inline constexpr EntityId from_ecs_entity(void_ecs::Entity entity) noexcept {
    return void_ecs::to_entity_id(entity);
}

} // namespace void_core
