/// @file world_composer.cpp
/// @brief World composer implementation

#include <void_engine/package/world_composer.hpp>
#include <void_engine/package/registry.hpp>
#include <void_engine/package/loader.hpp>
#include <void_engine/package/prefab_registry.hpp>
#include <void_engine/package/component_schema.hpp>
#include <void_engine/package/definition_registry.hpp>
#include <void_engine/package/widget_manager.hpp>
#include <void_engine/package/layer_applier.hpp>

#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/system.hpp>

#include <sstream>
#include <random>
#include <algorithm>

namespace void_package {

// =============================================================================
// WorldComposer Construction
// =============================================================================

WorldComposer::WorldComposer() = default;

WorldComposer::~WorldComposer() {
    // Ensure world is unloaded
    if (has_world()) {
        WorldUnloadOptions options;
        options.force = true;
        options.emit_events = false;
        static_cast<void>(unload_world(options));
    }
}

WorldComposer::WorldComposer(WorldComposer&&) noexcept = default;
WorldComposer& WorldComposer::operator=(WorldComposer&&) noexcept = default;

// =============================================================================
// Resource Schema Registration
// =============================================================================

void WorldComposer::register_resource_schema(
    const std::string& name,
    std::function<void_core::Result<void>(void_ecs::World&, const nlohmann::json&)> creator) {

    m_resource_schemas[name] = std::move(creator);
}

bool WorldComposer::has_resource_schema(const std::string& name) const {
    return m_resource_schemas.count(name) > 0;
}

// =============================================================================
// World Loading
// =============================================================================

void_core::Result<void> WorldComposer::load_world(
    const std::string& world_package_name,
    const WorldLoadOptions& options) {

    // Ensure no world is currently loaded
    if (has_world()) {
        return void_core::Err("A world is already loaded. Unload it first or use switch_world().");
    }

    // Validate required systems
    if (!m_package_registry) {
        return void_core::Err("Package registry not set");
    }
    if (!m_load_context) {
        return void_core::Err("Load context not set");
    }
    if (!m_load_context->ecs_world()) {
        return void_core::Err("ECS world not set in load context");
    }

    // Initialize world info
    LoadedWorldInfo info;
    info.name = world_package_name;
    info.state = WorldState::Loading;
    info.load_started = std::chrono::steady_clock::now();

    // Step 1: Load the world package to get the manifest
    auto load_result = m_package_registry->load_package(world_package_name, *m_load_context);
    if (!load_result) {
        info.state = WorldState::Failed;
        return void_core::Err("Failed to load world package: " + load_result.error().message());
    }

    // Get the loaded package to retrieve its path
    auto* loaded = m_package_registry->get_loaded(world_package_name);
    if (!loaded) {
        info.state = WorldState::Failed;
        return void_core::Err("Package '" + world_package_name + "' not found in registry after loading");
    }

    // Load manifest directly from the package path
    auto manifest_result = WorldPackageManifest::load(loaded->resolved.path);
    if (!manifest_result) {
        info.state = WorldState::Failed;
        return void_core::Err("Failed to parse world manifest: " + manifest_result.error().message());
    }
    info.manifest = std::move(*manifest_result);

    // Execute boot sequence
    auto boot_result = execute_boot_sequence(info, options);
    if (!boot_result) {
        info.state = WorldState::Failed;
        // Cleanup on failure
        cleanup_partial_load(info);
        return boot_result;
    }

    // Mark as ready
    info.state = WorldState::Ready;
    info.load_finished = std::chrono::steady_clock::now();

    // Store as current world
    m_current_world = std::move(info);

    // Emit load event
    if (options.emit_events) {
        emit_world_loaded(*m_current_world);
    }

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::load_world_from_manifest(
    WorldPackageManifest manifest,
    const WorldLoadOptions& options) {

    // Ensure no world is currently loaded
    if (has_world()) {
        return void_core::Err("A world is already loaded. Unload it first or use switch_world().");
    }

    // Validate required systems
    if (!m_load_context) {
        return void_core::Err("Load context not set");
    }
    if (!m_load_context->ecs_world()) {
        return void_core::Err("ECS world not set in load context");
    }

    // Initialize world info
    LoadedWorldInfo info;
    info.name = manifest.base.name;
    info.manifest = std::move(manifest);
    info.state = WorldState::Loading;
    info.load_started = std::chrono::steady_clock::now();

    // Execute boot sequence
    auto boot_result = execute_boot_sequence(info, options);
    if (!boot_result) {
        info.state = WorldState::Failed;
        cleanup_partial_load(info);
        return boot_result;
    }

    // Mark as ready
    info.state = WorldState::Ready;
    info.load_finished = std::chrono::steady_clock::now();

    // Store as current world
    m_current_world = std::move(info);

    // Emit load event
    if (options.emit_events) {
        emit_world_loaded(*m_current_world);
    }

    return void_core::Ok();
}

// =============================================================================
// Boot Sequence Execution
// =============================================================================

void_core::Result<void> WorldComposer::execute_boot_sequence(
    LoadedWorldInfo& info,
    const WorldLoadOptions& options) {

    // Step 1: Resolve and load dependencies
    auto deps_result = resolve_dependencies(info.manifest);
    if (!deps_result) {
        return deps_result;
    }

    // Step 2: Load asset bundles
    auto assets_result = load_assets(info.manifest, info);
    if (!assets_result) {
        return assets_result;
    }

    // Step 3: Load plugins
    auto plugins_result = load_plugins(info.manifest, info);
    if (!plugins_result) {
        return plugins_result;
    }

    // Step 4: Load widgets
    auto widgets_result = load_widgets(info.manifest, options, info);
    if (!widgets_result) {
        return widgets_result;
    }

    // Step 5: Stage layers
    auto layers_stage_result = stage_layers(info.manifest, info);
    if (!layers_stage_result) {
        return layers_stage_result;
    }

    // Step 6: Instantiate root scene
    auto scene_result = instantiate_root_scene(info.manifest, info);
    if (!scene_result) {
        return scene_result;
    }

    // Step 7: Apply layers
    if (options.apply_layers) {
        auto layers_apply_result = apply_layers(info.manifest, options, info);
        if (!layers_apply_result) {
            return layers_apply_result;
        }
    }

    // Step 8: Initialize ECS resources
    if (options.initialize_resources) {
        auto resources_result = initialize_ecs_resources(info.manifest, info);
        if (!resources_result) {
            return resources_result;
        }
    }

    // Step 9: Configure environment
    auto env_result = configure_environment(info.manifest, info);
    if (!env_result) {
        return env_result;
    }

    // Step 10: Spawn player
    if (options.spawn_player && info.manifest.has_player_spawn()) {
        auto player_result = spawn_player_internal(info.manifest, options, info);
        if (!player_result) {
            return player_result;
        }
    }

    // Step 11: Start scheduler
    if (options.start_scheduler) {
        auto scheduler_result = start_scheduler(options, info);
        if (!scheduler_result) {
            return scheduler_result;
        }
    }

    return void_core::Ok();
}

void WorldComposer::cleanup_partial_load(LoadedWorldInfo& info) {
    // Despawn any created entities
    if (m_load_context && m_load_context->ecs_world()) {
        auto* world = m_load_context->ecs_world();
        for (auto entity : info.scene_entities) {
            if (world->is_alive(entity)) {
                world->despawn(entity);
            }
        }
        if (info.player_entity.has_value() && world->is_alive(*info.player_entity)) {
            world->despawn(*info.player_entity);
        }
    }

    // Unapply layers
    if (m_layer_applier) {
        for (const auto& layer_name : info.applied_layers) {
            static_cast<void>(m_layer_applier->unapply(layer_name, *m_load_context->ecs_world()));
        }
    }

    // Clear tracking
    info.scene_entities.clear();
    info.player_entity.reset();
    info.applied_layers.clear();
}

// =============================================================================
// World Unloading
// =============================================================================

void_core::Result<void> WorldComposer::unload_world(
    const WorldUnloadOptions& options) {

    if (!m_current_world) {
        return void_core::Err("No world is loaded");
    }

    m_current_world->state = WorldState::Unloading;

    // Emit unloading event
    if (options.emit_events) {
        emit_world_unloading(*m_current_world);
    }

    // Stop scheduler
    stop_scheduler();

    // Unapply all layers
    unapply_all_layers(*m_current_world);

    // Despawn all entities (except player if preserving)
    if (!options.preserve_player) {
        despawn_all_entities(*m_current_world);
    } else {
        // Despawn non-player entities
        if (m_load_context && m_load_context->ecs_world()) {
            auto* world = m_load_context->ecs_world();
            for (auto entity : m_current_world->scene_entities) {
                if (world->is_alive(entity)) {
                    world->despawn(entity);
                }
            }
        }
    }

    // Unload widgets
    unload_widgets(*m_current_world);

    // Unload plugins
    unload_plugins(*m_current_world);

    // Unload assets
    unload_assets(*m_current_world);

    // Clear ECS resources
    clear_resources(*m_current_world);

    std::string world_name = m_current_world->name;

    // Clear current world
    m_current_world.reset();

    // Emit unloaded event
    if (options.emit_events) {
        emit_world_unloaded(world_name);
    }

    return void_core::Ok();
}

// =============================================================================
// World Switching
// =============================================================================

void_core::Result<void> WorldComposer::switch_world(
    const std::string& new_world_name,
    const WorldSwitchOptions& options) {

    std::optional<void_ecs::Entity> preserved_player;

    // If transferring player, preserve it
    if (options.transfer_player && m_current_world && m_current_world->player_entity) {
        preserved_player = m_current_world->player_entity;
    }

    // Emit switch event
    if (options.emit_events && m_event_bus) {
        std::string from_world = m_current_world ? m_current_world->name : "";
        // Would emit WorldSwitchEvent here
    }

    // Unload current world
    if (has_world()) {
        WorldUnloadOptions unload_opts = options.unload_options;
        unload_opts.preserve_player = options.transfer_player;
        auto unload_result = unload_world(unload_opts);
        if (!unload_result && !options.unload_options.force) {
            return unload_result;
        }
    }

    // Load new world
    WorldLoadOptions load_opts = options.load_options;
    if (options.transfer_player && preserved_player) {
        load_opts.spawn_player = false;  // We'll transfer the existing player
    }

    auto load_result = load_world(new_world_name, load_opts);
    if (!load_result) {
        return load_result;
    }

    // Transfer player if preserved
    if (options.transfer_player && preserved_player && m_current_world) {
        m_current_world->player_entity = preserved_player;
        // TODO: Move player to new spawn point if needed
    }

    return void_core::Ok();
}

// =============================================================================
// Internal Loading Steps Implementation
// =============================================================================

void_core::Result<void> WorldComposer::resolve_dependencies(
    const WorldPackageManifest& manifest) {

    // Dependencies are resolved when we load packages through the registry
    // The PackageRegistry handles dependency resolution via PackageResolver
    return void_core::Ok();
}

void_core::Result<void> WorldComposer::load_assets(
    const WorldPackageManifest& manifest,
    LoadedWorldInfo& info) {

    if (!m_package_registry || !m_load_context) {
        return void_core::Ok();  // Nothing to load
    }

    // Load asset bundle dependencies
    for (const auto& dep : manifest.base.asset_deps) {
        auto result = m_package_registry->load_package(dep.name, *m_load_context);
        if (!result) {
            if (!dep.optional) {
                return void_core::Err("Failed to load asset bundle '" + dep.name + "': " +
                                      result.error().message());
            }
            // Optional dependency failed, continue
            continue;
        }
        info.loaded_assets.push_back(dep.name);
    }

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::load_plugins(
    const WorldPackageManifest& manifest,
    LoadedWorldInfo& info) {

    if (!m_package_registry || !m_load_context) {
        return void_core::Ok();
    }

    // Load plugin dependencies
    for (const auto& dep : manifest.base.plugin_deps) {
        auto result = m_package_registry->load_package(dep.name, *m_load_context);
        if (!result) {
            if (!dep.optional) {
                return void_core::Err("Failed to load plugin '" + dep.name + "': " +
                                      result.error().message());
            }
            continue;
        }
        info.loaded_plugins.push_back(dep.name);
    }

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::load_widgets(
    const WorldPackageManifest& manifest,
    const WorldLoadOptions& options,
    LoadedWorldInfo& info) {

    if (!m_package_registry || !m_load_context) {
        return void_core::Ok();
    }

    // Load widget dependencies
    for (const auto& dep : manifest.base.widget_deps) {
        auto result = m_package_registry->load_package(dep.name, *m_load_context);
        if (!result) {
            if (!dep.optional) {
                return void_core::Err("Failed to load widget '" + dep.name + "': " +
                                      result.error().message());
            }
            continue;
        }
        info.loaded_widgets.push_back(dep.name);
    }

    // Activate widgets specified in manifest
    if (m_widget_manager) {
        auto widget_list = manifest.all_widgets(options.include_dev_widgets);
        for (const auto& widget_name : widget_list) {
            // TODO: Activate widget through WidgetManager
        }
    }

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::stage_layers(
    const WorldPackageManifest& manifest,
    LoadedWorldInfo& info) {

    if (!m_package_registry || !m_load_context || !m_layer_applier) {
        return void_core::Ok();
    }

    // Load and stage layer dependencies
    for (const auto& dep : manifest.base.layer_deps) {
        auto result = m_package_registry->load_package(dep.name, *m_load_context);
        if (!result) {
            if (!dep.optional) {
                return void_core::Err("Failed to load layer '" + dep.name + "': " +
                                      result.error().message());
            }
            continue;
        }
        // Layer is now staged (parsed but not applied)
    }

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::instantiate_root_scene(
    const WorldPackageManifest& manifest,
    LoadedWorldInfo& info) {

    if (!m_load_context || !m_load_context->ecs_world()) {
        return void_core::Err("No ECS world available");
    }

    auto* world = m_load_context->ecs_world();

    // Resolve scene path
    auto scene_path = manifest.resolve_scene_path(manifest.root_scene.path);

    // TODO: Use SceneInstantiator to load the scene
    // For now, just create a placeholder entity to indicate scene is loaded
    auto root_entity = world->spawn();
    info.scene_entities.push_back(root_entity);

    // TODO: Instantiate all entities from scene file
    // This would use PrefabRegistry for prefab-based entities

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::apply_layers(
    const WorldPackageManifest& manifest,
    const WorldLoadOptions& options,
    LoadedWorldInfo& info) {

    if (!m_layer_applier || !m_load_context || !m_load_context->ecs_world()) {
        return void_core::Ok();
    }

    auto* world = m_load_context->ecs_world();

    // Apply layers in order
    for (const auto& layer_name : manifest.layers) {
        if (m_layer_applier->is_staged(layer_name)) {
            auto result = m_layer_applier->apply(layer_name, *world);
            if (!result) {
                return void_core::Err("Failed to apply layer '" + layer_name + "': " +
                                      result.error().message());
            }
            info.applied_layers.push_back(layer_name);
        }
    }

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::initialize_ecs_resources(
    const WorldPackageManifest& manifest,
    LoadedWorldInfo& info) {

    if (!m_load_context || !m_load_context->ecs_world()) {
        return void_core::Ok();
    }

    auto* world = m_load_context->ecs_world();

    // Initialize resources from manifest
    for (const auto& [resource_name, data] : manifest.ecs_resources) {
        // Look up resource schema
        auto it = m_resource_schemas.find(resource_name);
        if (it == m_resource_schemas.end()) {
            // If component schema registry is available, try dynamic creation
            if (m_schema_registry) {
                auto* schema = m_schema_registry->get_schema(resource_name);
                if (!schema) {
                    return void_core::Err("Unknown resource type: " + resource_name +
                                          " (no schema registered)");
                }
                // TODO: Create resource from schema
                continue;
            }
            return void_core::Err("Unknown resource type: " + resource_name);
        }

        // Create resource using registered schema
        auto result = it->second(*world, data);
        if (!result) {
            return void_core::Err("Failed to initialize resource '" + resource_name + "': " +
                                  result.error().message());
        }
    }

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::configure_environment(
    const WorldPackageManifest& manifest,
    LoadedWorldInfo& info) {

    // TODO: Configure environment settings
    // - Set time of day
    // - Configure skybox
    // - Set up weather
    // - Configure post-processing

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::start_scheduler(
    const WorldLoadOptions& options,
    LoadedWorldInfo& info) {

    if (!m_load_context || !m_load_context->ecs_world()) {
        return void_core::Ok();
    }

    // TODO: Start the ECS system scheduler
    // world->scheduler().start();

    return void_core::Ok();
}

void_core::Result<void> WorldComposer::spawn_player_internal(
    const WorldPackageManifest& manifest,
    const WorldLoadOptions& options,
    LoadedWorldInfo& info) {

    if (!manifest.player_spawn.has_value()) {
        return void_core::Ok();
    }

    auto spawn_result = spawn_player(options.player_spawn_override);
    if (!spawn_result) {
        return void_core::Err("Failed to spawn player: " + spawn_result.error().message());
    }

    info.player_entity = *spawn_result;

    return void_core::Ok();
}

void WorldComposer::emit_world_loaded(const LoadedWorldInfo& info) {
    // TODO: Emit WorldLoadedEvent through EventBus
    // if (m_event_bus) {
    //     m_event_bus->publish(WorldLoadedEvent{info.name, info.manifest.base.source_path.string()});
    // }
}

// =============================================================================
// Internal Unloading Steps Implementation
// =============================================================================

void WorldComposer::stop_scheduler() {
    // TODO: Stop the ECS system scheduler
    // if (m_load_context && m_load_context->ecs_world()) {
    //     m_load_context->ecs_world()->scheduler().stop();
    // }
}

void WorldComposer::emit_world_unloading(const LoadedWorldInfo& info) {
    // TODO: Emit WorldUnloadingEvent through EventBus
}

void WorldComposer::unapply_all_layers(LoadedWorldInfo& info) {
    if (!m_layer_applier || !m_load_context || !m_load_context->ecs_world()) {
        return;
    }

    // Unapply in reverse order
    for (auto it = info.applied_layers.rbegin(); it != info.applied_layers.rend(); ++it) {
        static_cast<void>(m_layer_applier->unapply(*it, *m_load_context->ecs_world()));
    }
    info.applied_layers.clear();
}

void WorldComposer::despawn_all_entities(LoadedWorldInfo& info) {
    if (!m_load_context || !m_load_context->ecs_world()) {
        return;
    }

    auto* world = m_load_context->ecs_world();

    // Despawn scene entities
    for (auto entity : info.scene_entities) {
        if (world->is_alive(entity)) {
            world->despawn(entity);
        }
    }
    info.scene_entities.clear();

    // Despawn player
    if (info.player_entity.has_value() && world->is_alive(*info.player_entity)) {
        world->despawn(*info.player_entity);
    }
    info.player_entity.reset();
}

void WorldComposer::unload_widgets(LoadedWorldInfo& info) {
    if (!m_widget_manager) {
        return;
    }

    // TODO: Deactivate widgets
    info.loaded_widgets.clear();
}

void WorldComposer::unload_plugins(LoadedWorldInfo& info) {
    // Plugins are unloaded through PackageRegistry
    // We track what was loaded but let registry manage unload
    info.loaded_plugins.clear();
}

void WorldComposer::unload_assets(LoadedWorldInfo& info) {
    // Assets are unloaded through PackageRegistry
    info.loaded_assets.clear();
}

void WorldComposer::clear_resources(LoadedWorldInfo& info) {
    // TODO: Clear ECS resources that were initialized
}

void WorldComposer::emit_world_unloaded(const std::string& world_name) {
    // TODO: Emit WorldUnloadedEvent through EventBus
}

// =============================================================================
// Layer Control
// =============================================================================

void_core::Result<void> WorldComposer::apply_layer(const std::string& layer_name) {
    if (!has_world()) {
        return void_core::Err("No world is loaded");
    }
    if (!m_layer_applier || !m_load_context || !m_load_context->ecs_world()) {
        return void_core::Err("Layer applier not configured");
    }

    auto result = m_layer_applier->apply(layer_name, *m_load_context->ecs_world());
    if (result) {
        m_current_world->applied_layers.push_back(layer_name);
    }
    return result;
}

void_core::Result<void> WorldComposer::unapply_layer(const std::string& layer_name) {
    if (!has_world()) {
        return void_core::Err("No world is loaded");
    }
    if (!m_layer_applier || !m_load_context || !m_load_context->ecs_world()) {
        return void_core::Err("Layer applier not configured");
    }

    auto result = m_layer_applier->unapply(layer_name, *m_load_context->ecs_world());
    if (result) {
        auto& layers = m_current_world->applied_layers;
        layers.erase(std::remove(layers.begin(), layers.end(), layer_name), layers.end());
    }
    return result;
}

std::vector<std::string> WorldComposer::applied_layers() const {
    if (!m_current_world) {
        return {};
    }
    return m_current_world->applied_layers;
}

// =============================================================================
// Player Management
// =============================================================================

void_core::Result<void_ecs::Entity> WorldComposer::spawn_player(
    const std::optional<TransformData>& position_override) {

    if (!has_world()) {
        return void_core::Err<void_ecs::Entity>("No world is loaded");
    }
    if (!m_current_world->manifest.player_spawn.has_value()) {
        return void_core::Err<void_ecs::Entity>("No player spawn configuration in world");
    }
    if (!m_prefab_registry) {
        return void_core::Err<void_ecs::Entity>("Prefab registry not configured");
    }
    if (!m_load_context || !m_load_context->ecs_world()) {
        return void_core::Err<void_ecs::Entity>("ECS world not available");
    }

    const auto& spawn_config = *m_current_world->manifest.player_spawn;

    // Determine spawn position
    std::optional<TransformData> spawn_pos = position_override;
    if (!spawn_pos) {
        spawn_pos = get_next_spawn_point(m_current_world->manifest);
    }

    // Instantiate player prefab
    auto result = m_prefab_registry->instantiate(
        spawn_config.prefab,
        *m_load_context->ecs_world(),
        spawn_pos);

    if (!result) {
        return void_core::Err<void_ecs::Entity>("Failed to instantiate player prefab: " + result.error().message());
    }

    auto player = *result;

    // Apply initial inventory if configured
    if (spawn_config.has_initial_inventory()) {
        auto inv_result = apply_initial_inventory(player, *spawn_config.initial_inventory);
        if (!inv_result) {
            // Log warning but don't fail
        }
    }

    // Apply initial stats if configured
    if (spawn_config.has_initial_stats()) {
        auto stats_result = apply_initial_stats(player, *spawn_config.initial_stats);
        if (!stats_result) {
            // Log warning but don't fail
        }
    }

    m_current_world->player_entity = player;

    return void_core::Ok(player);
}

void WorldComposer::despawn_player() {
    if (!has_world() || !m_current_world->player_entity) {
        return;
    }
    if (!m_load_context || !m_load_context->ecs_world()) {
        return;
    }

    auto* world = m_load_context->ecs_world();
    if (world->is_alive(*m_current_world->player_entity)) {
        world->despawn(*m_current_world->player_entity);
    }
    m_current_world->player_entity.reset();
}

void_core::Result<void> WorldComposer::respawn_player() {
    if (!has_world()) {
        return void_core::Err("No world is loaded");
    }

    // Despawn current player if exists
    despawn_player();

    // Spawn new player
    auto result = spawn_player();
    if (!result) {
        return void_core::Err(result.error().message());
    }

    return void_core::Ok();
}

// =============================================================================
// Legacy Compatibility
// =============================================================================

void_core::Result<void> WorldComposer::load_legacy_scene(
    const std::filesystem::path& scene_path) {

    // Create a minimal world manifest for the legacy scene
    WorldPackageManifest manifest;
    manifest.base.name = "legacy." + scene_path.stem().string();
    manifest.base.type = PackageType::World;
    manifest.base.version = SemanticVersion{1, 0, 0};
    manifest.base.source_path = scene_path;
    manifest.base.base_path = scene_path.parent_path();

    manifest.root_scene.path = scene_path.filename().string();

    // Load as a manifest-based world
    return load_world_from_manifest(std::move(manifest));
}

// =============================================================================
// Helper Methods
// =============================================================================

std::optional<TransformData> WorldComposer::get_next_spawn_point(
    const WorldPackageManifest& manifest) const {

    const auto& spawn_points = manifest.root_scene.spawn_points;
    if (spawn_points.empty()) {
        return std::nullopt;
    }

    std::size_t index = 0;
    const auto& spawn_config = manifest.player_spawn;

    if (spawn_config.has_value()) {
        switch (spawn_config->spawn_selection) {
            case SpawnSelection::RoundRobin:
                index = m_spawn_point_index % spawn_points.size();
                m_spawn_point_index++;
                break;

            case SpawnSelection::Random: {
                static std::mt19937 gen(std::random_device{}());
                std::uniform_int_distribution<std::size_t> dist(0, spawn_points.size() - 1);
                index = dist(gen);
                break;
            }

            case SpawnSelection::Fixed:
                index = 0;
                break;

            case SpawnSelection::Weighted:
                // TODO: Implement weighted selection
                index = 0;
                break;
        }
    }

    // TODO: Look up spawn point entity and get its transform
    // For now, return a default position
    TransformData transform;
    return transform;
}

void_core::Result<void> WorldComposer::apply_initial_inventory(
    void_ecs::Entity player,
    const nlohmann::json& inventory) {

    // TODO: Apply inventory through inventory system if loaded
    // This would iterate over inventory slots and add items
    return void_core::Ok();
}

void_core::Result<void> WorldComposer::apply_initial_stats(
    void_ecs::Entity player,
    const nlohmann::json& stats) {

    // TODO: Apply stats to player entity components
    // This would set Health, Armor, Stamina, etc.
    return void_core::Ok();
}

// =============================================================================
// Debugging
// =============================================================================

std::string WorldComposer::format_state() const {
    std::ostringstream oss;
    oss << "WorldComposer State:\n";

    if (!m_current_world) {
        oss << "  No world loaded\n";
    } else {
        oss << "  World: " << m_current_world->name << "\n";
        oss << "  State: " << world_state_to_string(m_current_world->state) << "\n";
        oss << "  Scene entities: " << m_current_world->scene_entities.size() << "\n";
        oss << "  Player: " << (m_current_world->player_entity ? "spawned" : "none") << "\n";
        oss << "  Loaded assets: " << m_current_world->loaded_assets.size() << "\n";
        oss << "  Loaded plugins: " << m_current_world->loaded_plugins.size() << "\n";
        oss << "  Applied layers: " << m_current_world->applied_layers.size() << "\n";

        if (m_current_world->state == WorldState::Ready) {
            oss << "  Load time: " << m_current_world->load_duration_ms() << " ms\n";
        }
    }

    oss << "  Resource schemas: " << m_resource_schemas.size() << "\n";

    return oss.str();
}

// =============================================================================
// ECS World Access
// =============================================================================

void_ecs::World* WorldComposer::ecs_world() const noexcept {
    if (m_load_context) {
        return m_load_context->ecs_world();
    }
    return nullptr;
}

// =============================================================================
// Frame Update
// =============================================================================

void WorldComposer::update(float /*dt*/) {
    // Run ECS systems if a world is loaded
    if (m_load_context && m_load_context->ecs_world() && has_world()) {
        m_load_context->ecs_world()->run_systems();
    }
}

// =============================================================================
// Factory Functions
// =============================================================================

std::unique_ptr<WorldComposer> create_world_composer() {
    return std::make_unique<WorldComposer>();
}

} // namespace void_package
