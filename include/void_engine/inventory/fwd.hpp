/// @file fwd.hpp
/// @brief Forward declarations for void_inventory module

#pragma once

#include <cstdint>
#include <functional>

namespace void_inventory {

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Unique identifier for item definitions
struct ItemDefId {
    std::uint64_t value{0};
    bool operator==(const ItemDefId&) const = default;
    bool operator!=(const ItemDefId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for item instances
struct ItemInstanceId {
    std::uint64_t value{0};
    bool operator==(const ItemInstanceId&) const = default;
    bool operator!=(const ItemInstanceId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for containers
struct ContainerId {
    std::uint64_t value{0};
    bool operator==(const ContainerId&) const = default;
    bool operator!=(const ContainerId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for equipment slots
struct EquipmentSlotId {
    std::uint64_t value{0};
    bool operator==(const EquipmentSlotId&) const = default;
    bool operator!=(const EquipmentSlotId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for recipes
struct RecipeId {
    std::uint64_t value{0};
    bool operator==(const RecipeId&) const = default;
    bool operator!=(const RecipeId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for crafting stations
struct CraftingStationId {
    std::uint64_t value{0};
    bool operator==(const CraftingStationId&) const = default;
    bool operator!=(const CraftingStationId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Entity identifier (from ECS)
struct EntityId {
    std::uint64_t value{0};
    bool operator==(const EntityId&) const = default;
    bool operator!=(const EntityId&) const = default;
    explicit operator bool() const { return value != 0; }
};

// =============================================================================
// Forward Declarations - Items
// =============================================================================

struct ItemDef;
struct ItemInstance;
struct ItemStack;
struct ItemModifier;
struct ItemProperty;
class ItemRegistry;
class ItemFactory;

// =============================================================================
// Forward Declarations - Containers
// =============================================================================

struct SlotConfig;
struct SlotState;
class IContainer;
class Container;
class GridContainer;
class WeightedContainer;
class FilteredContainer;

// =============================================================================
// Forward Declarations - Equipment
// =============================================================================

struct EquipmentSlotDef;
struct EquippedItem;
struct StatModifier;
class EquipmentComponent;
class EquipmentSet;
class EquipmentSetRegistry;

// =============================================================================
// Forward Declarations - Crafting
// =============================================================================

struct RecipeIngredient;
struct RecipeOutput;
struct Recipe;
struct CraftingProgress;
class RecipeRegistry;
class CraftingStation;
class CraftingComponent;

// =============================================================================
// Forward Declarations - System
// =============================================================================

struct InventoryConfig;
struct InventoryTransaction;
class InventorySystem;

} // namespace void_inventory

// =============================================================================
// Hash Specializations
// =============================================================================

namespace std {

template<>
struct hash<void_inventory::ItemDefId> {
    std::size_t operator()(const void_inventory::ItemDefId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_inventory::ItemInstanceId> {
    std::size_t operator()(const void_inventory::ItemInstanceId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_inventory::ContainerId> {
    std::size_t operator()(const void_inventory::ContainerId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_inventory::EquipmentSlotId> {
    std::size_t operator()(const void_inventory::EquipmentSlotId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_inventory::RecipeId> {
    std::size_t operator()(const void_inventory::RecipeId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_inventory::CraftingStationId> {
    std::size_t operator()(const void_inventory::CraftingStationId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_inventory::EntityId> {
    std::size_t operator()(const void_inventory::EntityId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace std
