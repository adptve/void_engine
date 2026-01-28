/// @file fwd.hpp
/// @brief Forward declarations for void_combat module

#pragma once

#include <void_engine/core/id.hpp>
#include <cstdint>
#include <memory>

namespace void_combat {

// Use canonical EntityId from void_core
using EntityId = void_core::EntityId;

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Strongly-typed weapon ID
struct WeaponId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const WeaponId&) const = default;
    auto operator<=>(const WeaponId&) const = default;
};

/// @brief Strongly-typed damage type ID
struct DamageTypeId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const DamageTypeId&) const = default;
    auto operator<=>(const DamageTypeId&) const = default;
};

/// @brief Strongly-typed status effect ID
struct StatusEffectId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const StatusEffectId&) const = default;
    auto operator<=>(const StatusEffectId&) const = default;
};

/// @brief Strongly-typed projectile ID
struct ProjectileId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const ProjectileId&) const = default;
    auto operator<=>(const ProjectileId&) const = default;
};


// =============================================================================
// Forward Declarations - Health
// =============================================================================

class IHealthComponent;
class HealthComponent;
class ShieldComponent;
class ArmorComponent;

// =============================================================================
// Forward Declarations - Damage
// =============================================================================

class DamageType;
class DamageTypeRegistry;
struct DamageInfo;
struct DamageResult;
class DamageProcessor;

// =============================================================================
// Forward Declarations - Weapons
// =============================================================================

class IWeapon;
class Weapon;
class WeaponRegistry;
class ProjectileWeapon;
class HitscanWeapon;
class MeleeWeapon;
class AreaWeapon;

// =============================================================================
// Forward Declarations - Projectiles
// =============================================================================

class IProjectile;
class Projectile;
class ProjectilePool;
class ProjectileSystem;

// =============================================================================
// Forward Declarations - Status Effects
// =============================================================================

class IStatusEffect;
class StatusEffect;
class StatusEffectRegistry;
class StatusEffectComponent;

// =============================================================================
// Forward Declarations - Combat System
// =============================================================================

class CombatSystem;
class HitDetection;
class KillTracker;

// =============================================================================
// Smart Pointer Aliases
// =============================================================================

using WeaponPtr = std::unique_ptr<IWeapon>;
using StatusEffectPtr = std::unique_ptr<IStatusEffect>;
using ProjectilePtr = std::unique_ptr<IProjectile>;

} // namespace void_combat

// Hash specializations
namespace std {
    template<> struct hash<void_combat::WeaponId> {
        std::size_t operator()(const void_combat::WeaponId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_combat::DamageTypeId> {
        std::size_t operator()(const void_combat::DamageTypeId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_combat::StatusEffectId> {
        std::size_t operator()(const void_combat::StatusEffectId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_combat::ProjectileId> {
        std::size_t operator()(const void_combat::ProjectileId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_combat::EntityId> {
        std::size_t operator()(const void_combat::EntityId& id) const noexcept {
            return std::hash<std::uint64_t>{}(id.value);
        }
    };
}
