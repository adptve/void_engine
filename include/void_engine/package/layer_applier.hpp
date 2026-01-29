#pragma once

/// @file layer_applier.hpp
/// @brief Layer staging, application, and rollback
///
/// The LayerApplier manages the lifecycle of layers:
/// - stage(): Parse and validate a layer without applying it
/// - apply(): Apply a staged layer to the ECS world
/// - unapply(): Clean rollback of all layer modifications
///
/// CRITICAL: All entities and resources modified by a layer are tracked
/// for clean rollback. When a layer is unapplied:
/// - All spawned entities are despawned
/// - All modified resources are reverted to pre-layer values
/// - All spawners are stopped and cleaned up
///
/// Layers can be applied/unapplied at runtime while the game is running.

#include "layer_package.hpp"
#include "prefab_registry.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>
#include <void_engine/ecs/entity.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include <optional>

namespace void_package {

// Forward declarations
class PrefabRegistry;
class ComponentSchemaRegistry;

// =============================================================================
// SpawnerState
// =============================================================================

/// Runtime state for an active spawner
struct SpawnerState {
    std::string id;                           ///< Spawner ID
    const SpawnerEntry* entry = nullptr;      ///< Reference to spawner config
    std::vector<void_ecs::Entity> spawned;    ///< Currently active spawned entities
    float time_since_last_spawn = 0.0f;       ///< Accumulator for spawn timing
    bool initial_spawn_done = false;          ///< Whether initial spawn has occurred

    /// Get count of currently active entities
    [[nodiscard]] std::size_t active_count() const { return spawned.size(); }

    /// Check if spawner can spawn (under max_active)
    [[nodiscard]] bool can_spawn() const {
        return entry && static_cast<int>(spawned.size()) < entry->max_active;
    }
};

// =============================================================================
// ModifierOriginalValue
// =============================================================================

/// Stored original value for a modifier (for rollback)
struct ModifierOriginalValue {
    std::string path;
    nlohmann::json original_value;
    bool was_present = true;  ///< False if the resource didn't exist before
};

// =============================================================================
// LightingOriginalState
// =============================================================================

/// Original lighting state for rollback
struct LightingOriginalState {
    std::optional<nlohmann::json> sun_state;
    std::optional<nlohmann::json> ambient_state;
    std::vector<void_ecs::Entity> created_lights;
};

// =============================================================================
// StagedLayer
// =============================================================================

/// A parsed and validated layer ready for application
///
/// Contains the manifest plus tracking data for application/unapplication.
struct StagedLayer {
    std::string name;                          ///< Layer name (from manifest)
    LayerPackageManifest manifest;             ///< The layer manifest
    std::filesystem::path source_path;         ///< Path to the layer file

    /// Check if layer is valid for application
    [[nodiscard]] bool is_valid() const { return !name.empty(); }
};

// =============================================================================
// AppliedLayerState
// =============================================================================

/// State tracking for an applied layer (for clean unapply)
struct AppliedLayerState {
    std::string name;
    LayerPackageManifest manifest;

    // Entities created by this layer
    std::vector<void_ecs::Entity> spawned_entities;
    std::vector<void_ecs::Entity> objective_entities;
    std::vector<void_ecs::Entity> weather_entities;

    // Spawner runtime state
    std::map<std::string, SpawnerState> spawner_states;

    // Original values for rollback
    std::vector<ModifierOriginalValue> modifier_originals;
    LightingOriginalState lighting_original;
    std::optional<nlohmann::json> weather_original;

    // Timestamps
    std::chrono::steady_clock::time_point applied_at;

    /// Get total entity count created by this layer
    [[nodiscard]] std::size_t total_entity_count() const;

    /// Get list of all entities owned by this layer
    [[nodiscard]] std::vector<void_ecs::Entity> all_entities() const;
};

// =============================================================================
// LayerApplier
// =============================================================================

/// Manages layer staging, application, and rollback
///
/// Key responsibilities:
/// - Staging layers (parse without apply)
/// - Applying layers to ECS world
/// - Clean unapplication with full rollback
/// - Spawner management (continuous spawning)
/// - Modifier application and reversion
///
/// Thread-safety: NOT thread-safe. Must be accessed from main thread.
class LayerApplier {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    LayerApplier();
    ~LayerApplier();

    // Non-copyable
    LayerApplier(const LayerApplier&) = delete;
    LayerApplier& operator=(const LayerApplier&) = delete;

    // Movable
    LayerApplier(LayerApplier&&) noexcept;
    LayerApplier& operator=(LayerApplier&&) noexcept;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Set the prefab registry for entity instantiation
    void set_prefab_registry(PrefabRegistry* registry) noexcept {
        m_prefab_registry = registry;
    }

    /// Set the component schema registry for modifier application
    void set_schema_registry(ComponentSchemaRegistry* registry) noexcept {
        m_schema_registry = registry;
    }

    // =========================================================================
    // Staging
    // =========================================================================

    /// Stage a layer package (parse and validate without applying)
    ///
    /// @param package Resolved package information
    /// @return Staged layer ready for application, or Error
    [[nodiscard]] void_core::Result<StagedLayer> stage(const ResolvedPackage& package);

    /// Stage a layer from a manifest directly
    [[nodiscard]] void_core::Result<StagedLayer> stage(LayerPackageManifest manifest);

    /// Check if a layer is staged
    [[nodiscard]] bool is_staged(const std::string& layer_name) const;

    /// Get a staged layer
    [[nodiscard]] const StagedLayer* get_staged(const std::string& layer_name) const;

    /// Remove a staged layer (without applying)
    void unstage(const std::string& layer_name);

    /// Get all staged layer names
    [[nodiscard]] std::vector<std::string> staged_layer_names() const;

    // =========================================================================
    // Application
    // =========================================================================

    /// Apply a staged layer to the ECS world
    ///
    /// Performs:
    /// 1. Instantiate additive scene entities
    /// 2. Create spawner entities
    /// 3. Apply lighting overrides
    /// 4. Apply weather overrides
    /// 5. Create objective entities
    /// 6. Apply modifiers (save originals for rollback)
    ///
    /// @param layer_name Name of the staged layer to apply
    /// @param world ECS world to apply to
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> apply(
        const std::string& layer_name,
        void_ecs::World& world);

    /// Apply a staged layer directly
    [[nodiscard]] void_core::Result<void> apply(
        const StagedLayer& layer,
        void_ecs::World& world);

    // =========================================================================
    // Unapplication
    // =========================================================================

    /// Unapply a layer from the ECS world
    ///
    /// Performs:
    /// 1. Despawn all entities created by this layer
    /// 2. Stop and clean up all spawners
    /// 3. Revert lighting to pre-layer state
    /// 4. Revert weather to pre-layer state
    /// 5. Revert all modifiers to original values
    ///
    /// @param layer_name Name of the layer to unapply
    /// @param world ECS world to unapply from
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> unapply(
        const std::string& layer_name,
        void_ecs::World& world);

    /// Unapply all layers
    void unapply_all(void_ecs::World& world);

    // =========================================================================
    // Queries
    // =========================================================================

    /// Check if a layer is currently applied
    [[nodiscard]] bool is_applied(const std::string& layer_name) const;

    /// Get state for an applied layer
    [[nodiscard]] const AppliedLayerState* get_applied_state(
        const std::string& layer_name) const;

    /// Get all applied layer names (in application order)
    [[nodiscard]] std::vector<std::string> applied_layer_names() const;

    /// Get applied layer count
    [[nodiscard]] std::size_t applied_layer_count() const noexcept {
        return m_applied_layers.size();
    }

    // =========================================================================
    // Spawner Management
    // =========================================================================

    /// Update all spawners (call each frame)
    ///
    /// Spawners continuously spawn entities up to their max_active limit.
    ///
    /// @param world ECS world to spawn into
    /// @param dt Delta time in seconds
    void update_spawners(void_ecs::World& world, float dt);

    /// Force spawn from a specific spawner
    ///
    /// @param layer_name Layer containing the spawner
    /// @param spawner_id Spawner ID
    /// @param world ECS world
    /// @return Spawned entity, or Error
    [[nodiscard]] void_core::Result<void_ecs::Entity> force_spawn(
        const std::string& layer_name,
        const std::string& spawner_id,
        void_ecs::World& world);

    /// Clear dead entities from spawner tracking
    ///
    /// Call this to clean up entities that have been despawned externally.
    void cleanup_dead_entities(void_ecs::World& world);

    // =========================================================================
    // Modifier System
    // =========================================================================

    /// Callback type for getting a resource value by path
    using ResourceGetter = std::function<nlohmann::json(const std::string& path)>;

    /// Callback type for setting a resource value by path
    using ResourceSetter = std::function<bool(const std::string& path, const nlohmann::json& value)>;

    /// Set the resource getter callback
    void set_resource_getter(ResourceGetter getter) {
        m_resource_getter = std::move(getter);
    }

    /// Set the resource setter callback
    void set_resource_setter(ResourceSetter setter) {
        m_resource_setter = std::move(setter);
    }

    // =========================================================================
    // Layer Ordering
    // =========================================================================

    /// Get layers sorted by priority (lowest first)
    [[nodiscard]] std::vector<std::string> layers_by_priority() const;

    /// Reorder applied layers by priority (reapply if needed)
    ///
    /// Use when priorities change or layers need reordering.
    [[nodiscard]] void_core::Result<void> reorder_layers(void_ecs::World& world);

    // =========================================================================
    // Debugging
    // =========================================================================

    /// Get applier state as formatted string
    [[nodiscard]] std::string format_state() const;

private:
    // =========================================================================
    // Internal Application Methods
    // =========================================================================

    /// Apply additive scenes
    [[nodiscard]] void_core::Result<void> apply_additive_scenes(
        const LayerPackageManifest& manifest,
        void_ecs::World& world,
        AppliedLayerState& state);

    /// Create spawners
    [[nodiscard]] void_core::Result<void> create_spawners(
        const LayerPackageManifest& manifest,
        void_ecs::World& world,
        AppliedLayerState& state);

    /// Apply lighting overrides
    [[nodiscard]] void_core::Result<void> apply_lighting(
        const LayerPackageManifest& manifest,
        void_ecs::World& world,
        AppliedLayerState& state);

    /// Apply weather overrides
    [[nodiscard]] void_core::Result<void> apply_weather(
        const LayerPackageManifest& manifest,
        void_ecs::World& world,
        AppliedLayerState& state);

    /// Create objectives
    [[nodiscard]] void_core::Result<void> apply_objectives(
        const LayerPackageManifest& manifest,
        void_ecs::World& world,
        AppliedLayerState& state);

    /// Apply modifiers
    [[nodiscard]] void_core::Result<void> apply_modifiers(
        const LayerPackageManifest& manifest,
        void_ecs::World& world,
        AppliedLayerState& state);

    // =========================================================================
    // Internal Unapplication Methods
    // =========================================================================

    /// Despawn all entities from a layer
    void despawn_entities(AppliedLayerState& state, void_ecs::World& world);

    /// Revert lighting to original state
    void revert_lighting(AppliedLayerState& state, void_ecs::World& world);

    /// Revert weather to original state
    void revert_weather(AppliedLayerState& state, void_ecs::World& world);

    /// Revert all modifiers
    void revert_modifiers(AppliedLayerState& state, void_ecs::World& world);

    // =========================================================================
    // Spawner Helpers
    // =========================================================================

    /// Spawn a single entity from a spawner
    [[nodiscard]] void_core::Result<void_ecs::Entity> spawn_from_spawner(
        const SpawnerEntry& spawner,
        void_ecs::World& world);

    /// Get random position within spawner volume
    [[nodiscard]] std::array<float, 3> get_spawn_position(const SpawnerVolume& volume);

    // =========================================================================
    // Data Members
    // =========================================================================

    // Staged layers (parsed but not applied)
    std::map<std::string, StagedLayer> m_staged_layers;

    // Applied layers (active in the world)
    std::map<std::string, AppliedLayerState> m_applied_layers;

    // Application order (for priority-based ordering)
    std::vector<std::string> m_application_order;

    // External dependencies
    PrefabRegistry* m_prefab_registry = nullptr;
    ComponentSchemaRegistry* m_schema_registry = nullptr;

    // Modifier callbacks
    ResourceGetter m_resource_getter;
    ResourceSetter m_resource_setter;
};

// =============================================================================
// Factory Function
// =============================================================================

/// Create a layer applier
[[nodiscard]] std::unique_ptr<LayerApplier> create_layer_applier();

} // namespace void_package
