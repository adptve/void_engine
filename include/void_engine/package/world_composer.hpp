#pragma once

/// @file world_composer.hpp
/// @brief World composition and orchestration
///
/// The WorldComposer manages the complete lifecycle of a world:
/// - load_world(): Full boot sequence from world package
/// - unload_world(): Clean shutdown with state preservation option
/// - switch_world(): Atomic transition between worlds
///
/// BOOT SEQUENCE (load_world):
/// 1. PackageRegistry::scan_directory(content_path)
/// 2. WorldComposer::load_world("arena_deathmatch")
///    a. Resolver determines load order
///    b. Load asset.bundles (PrefabRegistry, DefinitionRegistry populated)
///    c. Load plugin.packages (components, systems registered)
///    d. Load widget.packages (UI created)
///    e. Stage layer.packages
///    f. Parse world manifest
///    g. Instantiate root scene (using PrefabRegistry)
///    h. Apply active layers
///    i. Initialize ECS resources from manifest
///    j. Configure environment
///    k. Start scheduler
///    l. Emit WorldLoadedEvent
/// 3. Game loop runs
///
/// CRITICAL: WorldComposer is the single point of entry for world loading.
/// It orchestrates ALL other loaders and ensures proper ordering.

#include "fwd.hpp"
#include "world_package.hpp"
#include "layer_applier.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>
#include <void_engine/ecs/entity.hpp>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <functional>
#include <chrono>

// Forward declarations
namespace void_event { class EventBus; }

namespace void_package {

// Forward declarations
class PackageRegistry;
class LoadContext;
class PrefabRegistry;
class ComponentSchemaRegistry;
class DefinitionRegistry;
class WidgetManager;

// =============================================================================
// WorldState
// =============================================================================

/// Current state of a loaded world
enum class WorldState : std::uint8_t {
    Unloaded,       ///< No world loaded
    Loading,        ///< World is currently loading
    Ready,          ///< World is loaded and ready
    Unloading,      ///< World is currently unloading
    Failed          ///< World load failed
};

/// Convert WorldState to string
[[nodiscard]] const char* world_state_to_string(WorldState state) noexcept;

// =============================================================================
// LoadedWorldInfo
// =============================================================================

/// Information about a loaded world
struct LoadedWorldInfo {
    std::string name;                              ///< World package name
    WorldPackageManifest manifest;                 ///< Parsed manifest
    WorldState state = WorldState::Unloaded;       ///< Current state

    // Loaded content tracking
    std::vector<std::string> loaded_assets;        ///< Asset bundles loaded
    std::vector<std::string> loaded_plugins;       ///< Plugins loaded
    std::vector<std::string> loaded_widgets;       ///< Widgets loaded
    std::vector<std::string> applied_layers;       ///< Layers applied

    // Scene tracking
    std::vector<void_ecs::Entity> scene_entities;  ///< Root scene entities
    std::optional<void_ecs::Entity> player_entity; ///< Spawned player (if any)

    // Timing
    std::chrono::steady_clock::time_point load_started;
    std::chrono::steady_clock::time_point load_finished;

    /// Get load duration in milliseconds
    [[nodiscard]] double load_duration_ms() const;

    /// Get total entity count
    [[nodiscard]] std::size_t total_entity_count() const;
};

// =============================================================================
// WorldLoadOptions
// =============================================================================

/// Options for world loading
struct WorldLoadOptions {
    bool spawn_player = true;                      ///< Auto-spawn player entity
    bool apply_layers = true;                      ///< Apply world's default layers
    bool initialize_resources = true;              ///< Initialize ECS resources
    bool start_scheduler = true;                   ///< Start system scheduler
    bool emit_events = true;                       ///< Emit world events
    bool include_dev_widgets = false;              ///< Include dev-only widgets
    std::optional<TransformData> player_spawn_override;  ///< Override player spawn pos
};

// =============================================================================
// WorldUnloadOptions
// =============================================================================

/// Options for world unloading
struct WorldUnloadOptions {
    bool preserve_player = false;                  ///< Keep player entity for transfer
    bool emit_events = true;                       ///< Emit unload events
    bool force = false;                            ///< Force unload even if errors
};

// =============================================================================
// WorldSwitchOptions
// =============================================================================

/// Options for world switching
struct WorldSwitchOptions {
    bool transfer_player = false;                  ///< Transfer player to new world
    bool emit_events = true;                       ///< Emit switch events
    WorldLoadOptions load_options;                 ///< Options for new world
    WorldUnloadOptions unload_options;             ///< Options for old world
};

// =============================================================================
// ResourceSchemaEntry
// =============================================================================

/// Schema for a dynamic ECS resource
struct ResourceSchemaEntry {
    std::string name;                              ///< Resource type name
    std::function<void_core::Result<void>(void_ecs::World&, const nlohmann::json&)> creator;
};

// =============================================================================
// WorldComposer
// =============================================================================

/// Orchestrates world loading, unloading, and switching
///
/// The WorldComposer is the primary interface for world lifecycle management.
/// It coordinates all package loaders and ensures proper ordering.
///
/// Usage:
/// ```cpp
/// WorldComposer composer;
/// composer.set_package_registry(&registry);
/// composer.set_load_context(&ctx);
///
/// // Load a world
/// auto result = composer.load_world("arena_deathmatch");
/// if (!result) {
///     log_error("Failed to load world: {}", result.error().message());
/// }
///
/// // Later, switch to another world
/// composer.switch_world("capture_the_flag");
///
/// // Unload when done
/// composer.unload_world();
/// ```
class WorldComposer {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    WorldComposer();
    ~WorldComposer();

    // Non-copyable
    WorldComposer(const WorldComposer&) = delete;
    WorldComposer& operator=(const WorldComposer&) = delete;

    // Movable
    WorldComposer(WorldComposer&&) noexcept;
    WorldComposer& operator=(WorldComposer&&) noexcept;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Set the package registry
    void set_package_registry(PackageRegistry* registry) noexcept {
        m_package_registry = registry;
    }

    /// Set the load context
    void set_load_context(LoadContext* ctx) noexcept {
        m_load_context = ctx;
    }

    /// Set the prefab registry
    void set_prefab_registry(PrefabRegistry* registry) noexcept {
        m_prefab_registry = registry;
    }

    /// Set the component schema registry
    void set_schema_registry(ComponentSchemaRegistry* registry) noexcept {
        m_schema_registry = registry;
    }

    /// Set the definition registry
    void set_definition_registry(DefinitionRegistry* registry) noexcept {
        m_definition_registry = registry;
    }

    /// Set the widget manager
    void set_widget_manager(WidgetManager* manager) noexcept {
        m_widget_manager = manager;
    }

    /// Set the event bus
    void set_event_bus(void_event::EventBus* bus) noexcept {
        m_event_bus = bus;
    }

    /// Set the layer applier
    void set_layer_applier(LayerApplier* applier) noexcept {
        m_layer_applier = applier;
    }

    // =========================================================================
    // Resource Schema Registration
    // =========================================================================

    /// Register a resource schema for dynamic resource creation
    ///
    /// This allows world manifests to initialize ECS resources by name.
    ///
    /// @param name Resource type name (must match manifest ecs_resources keys)
    /// @param creator Function to create resource from JSON data
    void register_resource_schema(
        const std::string& name,
        std::function<void_core::Result<void>(void_ecs::World&, const nlohmann::json&)> creator);

    /// Check if a resource schema is registered
    [[nodiscard]] bool has_resource_schema(const std::string& name) const;

    // =========================================================================
    // World Loading
    // =========================================================================

    /// Load a world from a package
    ///
    /// Executes the full boot sequence:
    /// 1. Resolve all dependencies
    /// 2. Load asset bundles (PrefabRegistry, DefinitionRegistry populated)
    /// 3. Load plugins (components, systems registered)
    /// 4. Load widgets (UI created)
    /// 5. Stage layers
    /// 6. Parse world manifest
    /// 7. Instantiate root scene (using PrefabRegistry)
    /// 8. Apply active layers
    /// 9. Initialize ECS resources from manifest
    /// 10. Configure environment
    /// 11. Start scheduler
    /// 12. Emit WorldLoadedEvent
    ///
    /// @param world_package_name Name of the world package to load
    /// @param options Load options
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> load_world(
        const std::string& world_package_name,
        const WorldLoadOptions& options = {});

    /// Load a world from a manifest directly (for testing or embedded worlds)
    [[nodiscard]] void_core::Result<void> load_world_from_manifest(
        WorldPackageManifest manifest,
        const WorldLoadOptions& options = {});

    // =========================================================================
    // World Unloading
    // =========================================================================

    /// Unload the current world
    ///
    /// Performs:
    /// 1. Stop scheduler (if running)
    /// 2. Emit WorldUnloadingEvent
    /// 3. Unapply all layers
    /// 4. Despawn all world entities
    /// 5. Unload widgets
    /// 6. Unload plugins
    /// 7. Unload assets
    /// 8. Clear ECS resources
    /// 9. Emit WorldUnloadedEvent
    ///
    /// @param options Unload options
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> unload_world(
        const WorldUnloadOptions& options = {});

    // =========================================================================
    // World Switching
    // =========================================================================

    /// Switch from current world to a new world
    ///
    /// Atomic transition:
    /// 1. Unload current world
    /// 2. Load new world
    ///
    /// If new world fails to load, returns error (old world is still unloaded).
    ///
    /// @param new_world_name Name of the world package to switch to
    /// @param options Switch options
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> switch_world(
        const std::string& new_world_name,
        const WorldSwitchOptions& options = {});

    // =========================================================================
    // Queries
    // =========================================================================

    /// Check if a world is loaded
    [[nodiscard]] bool has_world() const noexcept {
        return m_current_world.has_value() && m_current_world->state == WorldState::Ready;
    }

    /// Get current world state
    [[nodiscard]] WorldState current_state() const noexcept {
        return m_current_world ? m_current_world->state : WorldState::Unloaded;
    }

    /// Get current world name
    [[nodiscard]] std::string current_world_name() const {
        return m_current_world ? m_current_world->name : "";
    }

    /// Get current world info
    [[nodiscard]] const LoadedWorldInfo* current_world_info() const {
        return m_current_world ? &*m_current_world : nullptr;
    }

    /// Get current world manifest
    [[nodiscard]] const WorldPackageManifest* current_manifest() const {
        return m_current_world ? &m_current_world->manifest : nullptr;
    }

    /// Get player entity (if spawned)
    [[nodiscard]] std::optional<void_ecs::Entity> player_entity() const {
        return m_current_world ? m_current_world->player_entity : std::nullopt;
    }

    /// Get the ECS world (if a world is loaded)
    [[nodiscard]] void_ecs::World* ecs_world() const noexcept;

    // =========================================================================
    // Frame Update
    // =========================================================================

    /// Update the loaded world (called each frame)
    ///
    /// This progresses the ECS world and any active systems.
    ///
    /// @param dt Delta time since last frame
    void update(float dt);

    // =========================================================================
    // Layer Control
    // =========================================================================

    /// Apply an additional layer to the current world
    [[nodiscard]] void_core::Result<void> apply_layer(const std::string& layer_name);

    /// Unapply a layer from the current world
    [[nodiscard]] void_core::Result<void> unapply_layer(const std::string& layer_name);

    /// Get list of applied layers
    [[nodiscard]] std::vector<std::string> applied_layers() const;

    // =========================================================================
    // Player Management
    // =========================================================================

    /// Spawn the player entity using world's player_spawn configuration
    [[nodiscard]] void_core::Result<void_ecs::Entity> spawn_player(
        const std::optional<TransformData>& position_override = std::nullopt);

    /// Despawn the current player entity
    void despawn_player();

    /// Respawn the player at a spawn point
    [[nodiscard]] void_core::Result<void> respawn_player();

    // =========================================================================
    // Legacy Compatibility
    // =========================================================================

    /// Load a legacy scene.json file (without package system)
    ///
    /// For backwards compatibility during migration.
    /// Legacy scenes are treated as worlds without dependencies.
    ///
    /// @param scene_path Path to scene.json file
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> load_legacy_scene(
        const std::filesystem::path& scene_path);

    // =========================================================================
    // Debugging
    // =========================================================================

    /// Get composer state as formatted string
    [[nodiscard]] std::string format_state() const;

private:
    // =========================================================================
    // Boot Sequence
    // =========================================================================

    /// Execute the full boot sequence for a world
    [[nodiscard]] void_core::Result<void> execute_boot_sequence(
        LoadedWorldInfo& info,
        const WorldLoadOptions& options);

    /// Cleanup after a partial/failed load
    void cleanup_partial_load(LoadedWorldInfo& info);

    // =========================================================================
    // Internal Loading Steps
    // =========================================================================

    /// Step 1: Resolve dependencies
    [[nodiscard]] void_core::Result<void> resolve_dependencies(
        const WorldPackageManifest& manifest);

    /// Step 2: Load asset bundles
    [[nodiscard]] void_core::Result<void> load_assets(
        const WorldPackageManifest& manifest,
        LoadedWorldInfo& info);

    /// Step 3: Load plugins
    [[nodiscard]] void_core::Result<void> load_plugins(
        const WorldPackageManifest& manifest,
        LoadedWorldInfo& info);

    /// Step 4: Load widgets
    [[nodiscard]] void_core::Result<void> load_widgets(
        const WorldPackageManifest& manifest,
        const WorldLoadOptions& options,
        LoadedWorldInfo& info);

    /// Step 5: Stage layers
    [[nodiscard]] void_core::Result<void> stage_layers(
        const WorldPackageManifest& manifest,
        LoadedWorldInfo& info);

    /// Step 6: Instantiate root scene
    [[nodiscard]] void_core::Result<void> instantiate_root_scene(
        const WorldPackageManifest& manifest,
        LoadedWorldInfo& info);

    /// Step 7: Apply layers
    [[nodiscard]] void_core::Result<void> apply_layers(
        const WorldPackageManifest& manifest,
        const WorldLoadOptions& options,
        LoadedWorldInfo& info);

    /// Step 8: Initialize ECS resources
    [[nodiscard]] void_core::Result<void> initialize_ecs_resources(
        const WorldPackageManifest& manifest,
        LoadedWorldInfo& info);

    /// Step 9: Configure environment
    [[nodiscard]] void_core::Result<void> configure_environment(
        const WorldPackageManifest& manifest,
        LoadedWorldInfo& info);

    /// Step 10: Start scheduler
    [[nodiscard]] void_core::Result<void> start_scheduler(
        const WorldLoadOptions& options,
        LoadedWorldInfo& info);

    /// Step 11: Spawn player
    [[nodiscard]] void_core::Result<void> spawn_player_internal(
        const WorldPackageManifest& manifest,
        const WorldLoadOptions& options,
        LoadedWorldInfo& info);

    /// Step 12: Emit WorldLoadedEvent
    void emit_world_loaded(const LoadedWorldInfo& info);

    // =========================================================================
    // Internal Unloading Steps
    // =========================================================================

    /// Stop scheduler
    void stop_scheduler();

    /// Emit WorldUnloadingEvent
    void emit_world_unloading(const LoadedWorldInfo& info);

    /// Unapply all layers
    void unapply_all_layers(LoadedWorldInfo& info);

    /// Despawn all world entities
    void despawn_all_entities(LoadedWorldInfo& info);

    /// Unload widgets
    void unload_widgets(LoadedWorldInfo& info);

    /// Unload plugins
    void unload_plugins(LoadedWorldInfo& info);

    /// Unload assets
    void unload_assets(LoadedWorldInfo& info);

    /// Clear ECS resources
    void clear_resources(LoadedWorldInfo& info);

    /// Emit WorldUnloadedEvent
    void emit_world_unloaded(const std::string& world_name);

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /// Get next spawn point based on selection method
    [[nodiscard]] std::optional<TransformData> get_next_spawn_point(
        const WorldPackageManifest& manifest) const;

    /// Apply initial inventory to player
    [[nodiscard]] void_core::Result<void> apply_initial_inventory(
        void_ecs::Entity player,
        const nlohmann::json& inventory);

    /// Apply initial stats to player
    [[nodiscard]] void_core::Result<void> apply_initial_stats(
        void_ecs::Entity player,
        const nlohmann::json& stats);

    // =========================================================================
    // Data Members
    // =========================================================================

    // External dependencies (not owned)
    PackageRegistry* m_package_registry = nullptr;
    LoadContext* m_load_context = nullptr;
    PrefabRegistry* m_prefab_registry = nullptr;
    ComponentSchemaRegistry* m_schema_registry = nullptr;
    DefinitionRegistry* m_definition_registry = nullptr;
    WidgetManager* m_widget_manager = nullptr;
    void_event::EventBus* m_event_bus = nullptr;
    LayerApplier* m_layer_applier = nullptr;

    // Resource schemas for dynamic resource creation
    std::map<std::string, std::function<void_core::Result<void>(void_ecs::World&, const nlohmann::json&)>> m_resource_schemas;

    // Current world state
    std::optional<LoadedWorldInfo> m_current_world;

    // Spawn point tracking
    mutable std::size_t m_spawn_point_index = 0;
};

// =============================================================================
// Factory Functions
// =============================================================================

/// Create a world composer
[[nodiscard]] std::unique_ptr<WorldComposer> create_world_composer();

/// Create a world package loader (delegates to WorldComposer)
[[nodiscard]] std::unique_ptr<PackageLoader> create_world_package_loader();

} // namespace void_package
