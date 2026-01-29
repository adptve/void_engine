/// @file layer_package_loader.cpp
/// @brief Layer package loader implementation
///
/// Loads layer packages by:
/// 1. Parsing the layer manifest
/// 2. Staging the layer (parse but don't apply)
/// 3. Application is deferred - the World decides when to apply/unapply
///
/// Layers are designed for runtime toggling - apply/unapply while game runs.

#include <void_engine/package/loader.hpp>
#include <void_engine/package/layer_package.hpp>
#include <void_engine/package/layer_applier.hpp>
#include <void_engine/package/prefab_registry.hpp>
#include <void_engine/package/component_schema.hpp>

#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/entity.hpp>

#include <map>
#include <set>
#include <random>
#include <cmath>
#include <sstream>
#include <algorithm>

namespace void_package {

// =============================================================================
// AppliedLayerState Implementation
// =============================================================================

std::size_t AppliedLayerState::total_entity_count() const {
    std::size_t count = spawned_entities.size() +
                        objective_entities.size() +
                        weather_entities.size() +
                        lighting_original.created_lights.size();

    for (const auto& [_, state] : spawner_states) {
        count += state.spawned.size();
    }

    return count;
}

std::vector<void_ecs::Entity> AppliedLayerState::all_entities() const {
    std::vector<void_ecs::Entity> entities;
    entities.reserve(total_entity_count());

    entities.insert(entities.end(), spawned_entities.begin(), spawned_entities.end());
    entities.insert(entities.end(), objective_entities.begin(), objective_entities.end());
    entities.insert(entities.end(), weather_entities.begin(), weather_entities.end());
    entities.insert(entities.end(),
        lighting_original.created_lights.begin(),
        lighting_original.created_lights.end());

    for (const auto& [_, state] : spawner_states) {
        entities.insert(entities.end(), state.spawned.begin(), state.spawned.end());
    }

    return entities;
}

// =============================================================================
// LayerApplier Implementation
// =============================================================================

LayerApplier::LayerApplier() = default;
LayerApplier::~LayerApplier() = default;
LayerApplier::LayerApplier(LayerApplier&&) noexcept = default;
LayerApplier& LayerApplier::operator=(LayerApplier&&) noexcept = default;

// =============================================================================
// Staging
// =============================================================================

void_core::Result<StagedLayer> LayerApplier::stage(const ResolvedPackage& package) {
    // Load and parse the manifest
    auto manifest_result = LayerPackageManifest::load(package.manifest.source_path);
    if (!manifest_result) {
        return void_core::Err<StagedLayer>("Failed to load layer manifest: " +
            manifest_result.error().message());
    }

    return stage(std::move(*manifest_result));
}

void_core::Result<StagedLayer> LayerApplier::stage(LayerPackageManifest manifest) {
    // Validate the manifest
    auto validate_result = manifest.validate();
    if (!validate_result) {
        return void_core::Err<StagedLayer>("Layer manifest validation failed: " +
            validate_result.error().message());
    }

    // Create staged layer
    StagedLayer staged;
    staged.name = manifest.base.name;
    staged.source_path = manifest.base.source_path;
    staged.manifest = std::move(manifest);

    // Store in staged layers
    m_staged_layers[staged.name] = staged;

    return staged;
}

bool LayerApplier::is_staged(const std::string& layer_name) const {
    return m_staged_layers.find(layer_name) != m_staged_layers.end();
}

const StagedLayer* LayerApplier::get_staged(const std::string& layer_name) const {
    auto it = m_staged_layers.find(layer_name);
    return it != m_staged_layers.end() ? &it->second : nullptr;
}

void LayerApplier::unstage(const std::string& layer_name) {
    m_staged_layers.erase(layer_name);
}

std::vector<std::string> LayerApplier::staged_layer_names() const {
    std::vector<std::string> names;
    names.reserve(m_staged_layers.size());
    for (const auto& [name, _] : m_staged_layers) {
        names.push_back(name);
    }
    return names;
}

// =============================================================================
// Application
// =============================================================================

void_core::Result<void> LayerApplier::apply(
    const std::string& layer_name,
    void_ecs::World& world)
{
    // Get staged layer
    auto it = m_staged_layers.find(layer_name);
    if (it == m_staged_layers.end()) {
        return void_core::Err("Layer not staged: " + layer_name);
    }

    return apply(it->second, world);
}

void_core::Result<void> LayerApplier::apply(
    const StagedLayer& layer,
    void_ecs::World& world)
{
    // Check if already applied
    if (is_applied(layer.name)) {
        return void_core::Err("Layer already applied: " + layer.name);
    }

    // Initialize state tracking
    AppliedLayerState state;
    state.name = layer.name;
    state.manifest = layer.manifest;
    state.applied_at = std::chrono::steady_clock::now();

    const auto& manifest = layer.manifest;

    // Step 1: Apply additive scenes
    auto scenes_result = apply_additive_scenes(manifest, world, state);
    if (!scenes_result) {
        // Rollback what we've done so far
        despawn_entities(state, world);
        return void_core::Err("Failed to apply additive scenes: " +
            scenes_result.error().message());
    }

    // Step 2: Create spawners
    auto spawners_result = create_spawners(manifest, world, state);
    if (!spawners_result) {
        despawn_entities(state, world);
        return void_core::Err("Failed to create spawners: " +
            spawners_result.error().message());
    }

    // Step 3: Apply lighting overrides
    auto lighting_result = apply_lighting(manifest, world, state);
    if (!lighting_result) {
        despawn_entities(state, world);
        return void_core::Err("Failed to apply lighting: " +
            lighting_result.error().message());
    }

    // Step 4: Apply weather overrides
    auto weather_result = apply_weather(manifest, world, state);
    if (!weather_result) {
        despawn_entities(state, world);
        revert_lighting(state, world);
        return void_core::Err("Failed to apply weather: " +
            weather_result.error().message());
    }

    // Step 5: Create objectives
    auto objectives_result = apply_objectives(manifest, world, state);
    if (!objectives_result) {
        despawn_entities(state, world);
        revert_lighting(state, world);
        revert_weather(state, world);
        return void_core::Err("Failed to apply objectives: " +
            objectives_result.error().message());
    }

    // Step 6: Apply modifiers (save originals for rollback)
    auto modifiers_result = apply_modifiers(manifest, world, state);
    if (!modifiers_result) {
        despawn_entities(state, world);
        revert_lighting(state, world);
        revert_weather(state, world);
        return void_core::Err("Failed to apply modifiers: " +
            modifiers_result.error().message());
    }

    // Store state and add to application order
    m_applied_layers[layer.name] = std::move(state);
    m_application_order.push_back(layer.name);

    return void_core::Ok();
}

// =============================================================================
// Unapplication
// =============================================================================

void_core::Result<void> LayerApplier::unapply(
    const std::string& layer_name,
    void_ecs::World& world)
{
    auto it = m_applied_layers.find(layer_name);
    if (it == m_applied_layers.end()) {
        return void_core::Err("Layer not applied: " + layer_name);
    }

    auto& state = it->second;

    // Revert in reverse order of application

    // Step 1: Revert modifiers
    revert_modifiers(state, world);

    // Step 2: Revert weather
    revert_weather(state, world);

    // Step 3: Revert lighting
    revert_lighting(state, world);

    // Step 4: Despawn all entities (including spawner entities, objectives)
    despawn_entities(state, world);

    // Remove from tracking
    m_applied_layers.erase(it);

    // Remove from application order
    auto order_it = std::find(m_application_order.begin(),
                               m_application_order.end(),
                               layer_name);
    if (order_it != m_application_order.end()) {
        m_application_order.erase(order_it);
    }

    return void_core::Ok();
}

void LayerApplier::unapply_all(void_ecs::World& world) {
    // Unapply in reverse order
    auto names = m_application_order;
    std::reverse(names.begin(), names.end());

    for (const auto& name : names) {
        auto result = unapply(name, world);
        // Log but continue on error
        (void)result;
    }
}

// =============================================================================
// Queries
// =============================================================================

bool LayerApplier::is_applied(const std::string& layer_name) const {
    return m_applied_layers.find(layer_name) != m_applied_layers.end();
}

const AppliedLayerState* LayerApplier::get_applied_state(
    const std::string& layer_name) const
{
    auto it = m_applied_layers.find(layer_name);
    return it != m_applied_layers.end() ? &it->second : nullptr;
}

std::vector<std::string> LayerApplier::applied_layer_names() const {
    return m_application_order;
}

// =============================================================================
// Spawner Management
// =============================================================================

void LayerApplier::update_spawners(void_ecs::World& world, float dt) {
    for (auto& [layer_name, state] : m_applied_layers) {
        for (auto& [spawner_id, spawner_state] : state.spawner_states) {
            if (!spawner_state.entry) continue;

            // Skip if at max
            if (!spawner_state.can_spawn()) continue;

            // Initial delay check
            if (!spawner_state.initial_spawn_done) {
                spawner_state.time_since_last_spawn += dt;
                if (spawner_state.time_since_last_spawn < spawner_state.entry->initial_delay) {
                    continue;
                }
                spawner_state.initial_spawn_done = true;
                spawner_state.time_since_last_spawn = 0.0f;
            }

            // Accumulate time
            spawner_state.time_since_last_spawn += dt;

            // Calculate spawn interval
            float spawn_interval = 1.0f / spawner_state.entry->spawn_rate;

            // Spawn if enough time has passed
            while (spawner_state.time_since_last_spawn >= spawn_interval &&
                   spawner_state.can_spawn())
            {
                spawner_state.time_since_last_spawn -= spawn_interval;

                auto spawn_result = spawn_from_spawner(*spawner_state.entry, world);
                if (spawn_result) {
                    spawner_state.spawned.push_back(*spawn_result);
                }
            }
        }
    }
}

void_core::Result<void_ecs::Entity> LayerApplier::force_spawn(
    const std::string& layer_name,
    const std::string& spawner_id,
    void_ecs::World& world)
{
    auto layer_it = m_applied_layers.find(layer_name);
    if (layer_it == m_applied_layers.end()) {
        return void_core::Err<void_ecs::Entity>("Layer not applied: " + layer_name);
    }

    auto spawner_it = layer_it->second.spawner_states.find(spawner_id);
    if (spawner_it == layer_it->second.spawner_states.end()) {
        return void_core::Err<void_ecs::Entity>("Spawner not found: " + spawner_id);
    }

    auto& spawner_state = spawner_it->second;
    if (!spawner_state.entry) {
        return void_core::Err<void_ecs::Entity>("Spawner has no entry");
    }

    auto spawn_result = spawn_from_spawner(*spawner_state.entry, world);
    if (!spawn_result) {
        return spawn_result;
    }

    spawner_state.spawned.push_back(*spawn_result);
    return spawn_result;
}

void LayerApplier::cleanup_dead_entities(void_ecs::World& world) {
    for (auto& [layer_name, state] : m_applied_layers) {
        // Clean up spawner entities
        for (auto& [spawner_id, spawner_state] : state.spawner_states) {
            auto& spawned = spawner_state.spawned;
            spawned.erase(
                std::remove_if(spawned.begin(), spawned.end(),
                    [&world](void_ecs::Entity e) {
                        return !world.is_alive(e);
                    }),
                spawned.end()
            );
        }

        // Clean up spawned entities list
        state.spawned_entities.erase(
            std::remove_if(state.spawned_entities.begin(), state.spawned_entities.end(),
                [&world](void_ecs::Entity e) {
                    return !world.is_alive(e);
                }),
            state.spawned_entities.end()
        );
    }
}

// =============================================================================
// Layer Ordering
// =============================================================================

std::vector<std::string> LayerApplier::layers_by_priority() const {
    std::vector<std::pair<std::string, int>> layers_with_priority;
    layers_with_priority.reserve(m_applied_layers.size());

    for (const auto& [name, state] : m_applied_layers) {
        layers_with_priority.emplace_back(name, state.manifest.priority);
    }

    // Sort by priority (lowest first)
    std::sort(layers_with_priority.begin(), layers_with_priority.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

    std::vector<std::string> result;
    result.reserve(layers_with_priority.size());
    for (const auto& [name, _] : layers_with_priority) {
        result.push_back(name);
    }

    return result;
}

void_core::Result<void> LayerApplier::reorder_layers(void_ecs::World& world) {
    // Get current order by priority
    auto priority_order = layers_by_priority();

    // If already in correct order, nothing to do
    if (priority_order == m_application_order) {
        return void_core::Ok();
    }

    // Store current states
    std::map<std::string, LayerPackageManifest> manifests;
    for (const auto& [name, state] : m_applied_layers) {
        manifests[name] = state.manifest;
    }

    // Unapply all in reverse current order
    unapply_all(world);

    // Reapply in priority order
    for (const auto& name : priority_order) {
        StagedLayer staged;
        staged.name = name;
        staged.manifest = manifests[name];

        auto result = apply(staged, world);
        if (!result) {
            return result;
        }
    }

    return void_core::Ok();
}

// =============================================================================
// Debugging
// =============================================================================

std::string LayerApplier::format_state() const {
    std::stringstream ss;

    ss << "LayerApplier State:\n";
    ss << "  Staged layers: " << m_staged_layers.size() << "\n";
    for (const auto& [name, layer] : m_staged_layers) {
        ss << "    - " << name << "\n";
    }

    ss << "  Applied layers: " << m_applied_layers.size() << "\n";
    for (const auto& name : m_application_order) {
        auto it = m_applied_layers.find(name);
        if (it != m_applied_layers.end()) {
            const auto& state = it->second;
            ss << "    - " << name
               << " (priority: " << state.manifest.priority
               << ", entities: " << state.total_entity_count()
               << ", spawners: " << state.spawner_states.size()
               << ")\n";
        }
    }

    return ss.str();
}

// =============================================================================
// Internal Application Methods
// =============================================================================

void_core::Result<void> LayerApplier::apply_additive_scenes(
    const LayerPackageManifest& manifest,
    void_ecs::World& /*world*/,
    AppliedLayerState& /*state*/)
{
    if (!m_prefab_registry) {
        // No prefab registry - can't spawn scenes
        // This is okay if there are no additive scenes
        if (manifest.additive_scenes.empty()) {
            return void_core::Ok();
        }
        return void_core::Err("PrefabRegistry required for additive scenes");
    }

    for (const auto& scene : manifest.additive_scenes) {
        if (scene.spawn_mode != SpawnMode::Immediate) {
            continue; // Skip deferred scenes
        }

        // Scene files would typically contain a list of prefab instances
        // For now, we treat the scene path as a prefab ID
        // A full implementation would parse the scene file

        [[maybe_unused]] auto scene_path = manifest.resolve_scene_path(scene.path);

        // TODO: Implement full scene file parsing
        // For now, skip scene loading - this would require SceneLoader integration
    }

    return void_core::Ok();
}

void_core::Result<void> LayerApplier::create_spawners(
    const LayerPackageManifest& manifest,
    void_ecs::World& world,
    AppliedLayerState& state)
{
    for (const auto& spawner : manifest.spawners) {
        SpawnerState spawner_state;
        spawner_state.id = spawner.id;
        spawner_state.entry = &spawner;
        spawner_state.time_since_last_spawn = 0.0f;
        spawner_state.initial_spawn_done = spawner.initial_delay <= 0.0f;

        // Initial spawn if requested
        if (spawner.spawn_on_apply && m_prefab_registry) {
            auto spawn_result = spawn_from_spawner(spawner, world);
            if (spawn_result) {
                spawner_state.spawned.push_back(*spawn_result);
            }
        }

        state.spawner_states[spawner.id] = std::move(spawner_state);
    }

    return void_core::Ok();
}

void_core::Result<void> LayerApplier::apply_lighting(
    const LayerPackageManifest& manifest,
    void_ecs::World& /*world*/,
    AppliedLayerState& /*state*/)
{
    if (!manifest.lighting.has_value() || !manifest.lighting->has_overrides()) {
        return void_core::Ok();
    }

    [[maybe_unused]] const auto& lighting = *manifest.lighting;

    // TODO: Implement actual lighting modification
    // This would:
    // 1. Find the sun entity and store its current state
    // 2. Apply sun override
    // 3. Find ambient settings and store current state
    // 4. Apply ambient override
    // 5. Spawn additional light entities

    // For additional lights, we would create entities
    // state.lighting_original.created_lights would store them

    return void_core::Ok();
}

void_core::Result<void> LayerApplier::apply_weather(
    const LayerPackageManifest& manifest,
    void_ecs::World& /*world*/,
    AppliedLayerState& /*state*/)
{
    if (!manifest.weather.has_value() || !manifest.weather->has_overrides()) {
        return void_core::Ok();
    }

    // TODO: Implement actual weather modification
    // This would interact with the weather system to apply:
    // - Fog settings
    // - Precipitation
    // - Wind zones

    return void_core::Ok();
}

void_core::Result<void> LayerApplier::apply_objectives(
    const LayerPackageManifest& manifest,
    void_ecs::World& /*world*/,
    AppliedLayerState& /*state*/)
{
    if (!m_prefab_registry) {
        if (manifest.objectives.empty()) {
            return void_core::Ok();
        }
        // Objectives typically need prefabs for visualization
        // Could also be handled by a dedicated objective system
    }

    for ([[maybe_unused]] const auto& objective : manifest.objectives) {
        // Objectives would be created as entities with specific components
        // The actual implementation depends on the game's objective system

        // For now, just track that we have objectives
        // A full implementation would:
        // 1. Create objective entity
        // 2. Add objective components based on type
        // 3. Position at objective.position
    }

    return void_core::Ok();
}

void_core::Result<void> LayerApplier::apply_modifiers(
    const LayerPackageManifest& manifest,
    void_ecs::World& /*world*/,
    AppliedLayerState& state)
{
    if (!m_resource_getter || !m_resource_setter) {
        if (manifest.modifiers.empty()) {
            return void_core::Ok();
        }
        return void_core::Err("Resource getter/setter required for modifiers");
    }

    for (const auto& mod : manifest.modifiers) {
        // Store original value for rollback
        ModifierOriginalValue original;
        original.path = mod.path;

        try {
            original.original_value = m_resource_getter(mod.path);
            original.was_present = true;
        } catch (...) {
            original.was_present = false;
        }

        state.modifier_originals.push_back(original);

        // Apply new value
        if (!m_resource_setter(mod.path, mod.value)) {
            return void_core::Err("Failed to set modifier: " + mod.path);
        }
    }

    return void_core::Ok();
}

// =============================================================================
// Internal Unapplication Methods
// =============================================================================

void LayerApplier::despawn_entities(AppliedLayerState& state, void_ecs::World& world) {
    auto all = state.all_entities();
    for (auto entity : all) {
        if (world.is_alive(entity)) {
            world.despawn(entity);
        }
    }

    state.spawned_entities.clear();
    state.objective_entities.clear();
    state.weather_entities.clear();
    state.lighting_original.created_lights.clear();
    state.spawner_states.clear();
}

void LayerApplier::revert_lighting(AppliedLayerState& /*state*/, void_ecs::World& /*world*/) {
    // TODO: Restore original sun and ambient settings
    // This would read from state.lighting_original.sun_state and ambient_state
    // and apply them back to the world
}

void LayerApplier::revert_weather(AppliedLayerState& /*state*/, void_ecs::World& /*world*/) {
    // TODO: Restore original weather settings
    // This would read from state.weather_original and apply it back
}

void LayerApplier::revert_modifiers(AppliedLayerState& state, void_ecs::World& /*world*/) {
    if (!m_resource_setter) return;

    // Revert in reverse order
    for (auto it = state.modifier_originals.rbegin();
         it != state.modifier_originals.rend();
         ++it)
    {
        const auto& original = *it;

        if (original.was_present) {
            m_resource_setter(original.path, original.original_value);
        } else {
            // Resource didn't exist before - ideally we'd remove it
            // For now, just set it to null/default
            m_resource_setter(original.path, nlohmann::json());
        }
    }

    state.modifier_originals.clear();
}

// =============================================================================
// Spawner Helpers
// =============================================================================

void_core::Result<void_ecs::Entity> LayerApplier::spawn_from_spawner(
    const SpawnerEntry& spawner,
    void_ecs::World& world)
{
    if (!m_prefab_registry) {
        return void_core::Err<void_ecs::Entity>("PrefabRegistry not set");
    }

    // Get spawn position
    auto position = get_spawn_position(spawner.volume);

    // Create transform override
    TransformData transform;
    transform.position = position;

    // Instantiate prefab
    auto result = m_prefab_registry->instantiate(spawner.prefab, world, transform);
    if (!result) {
        return void_core::Err<void_ecs::Entity>(
            "Failed to spawn from " + spawner.id + ": " + result.error().message());
    }

    return result;
}

std::array<float, 3> LayerApplier::get_spawn_position(const SpawnerVolume& volume) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    if (volume.type == SpawnerVolume::Type::Sphere) {
        // Random point in sphere
        std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159265f);
        std::uniform_real_distribution<float> radius_dist(0.0f, 1.0f);

        float theta = angle_dist(gen);
        float phi = std::acos(2.0f * radius_dist(gen) - 1.0f);
        float r = volume.radius * std::cbrt(radius_dist(gen));

        return {
            volume.center[0] + r * std::sin(phi) * std::cos(theta),
            volume.center[1] + r * std::cos(phi),
            volume.center[2] + r * std::sin(phi) * std::sin(theta)
        };
    } else {
        // Random point in box
        std::uniform_real_distribution<float> x_dist(volume.min[0], volume.max[0]);
        std::uniform_real_distribution<float> y_dist(volume.min[1], volume.max[1]);
        std::uniform_real_distribution<float> z_dist(volume.min[2], volume.max[2]);

        return {x_dist(gen), y_dist(gen), z_dist(gen)};
    }
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<LayerApplier> create_layer_applier() {
    return std::make_unique<LayerApplier>();
}

// =============================================================================
// LayerPackageLoader
// =============================================================================

/// Loader for layer.package files
///
/// Layers are staged on load but not applied automatically.
/// The World/Runtime decides when to apply/unapply layers.
class LayerPackageLoader : public PackageLoader {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    LayerPackageLoader() : m_applier(create_layer_applier()) {}

    // =========================================================================
    // PackageLoader Interface
    // =========================================================================

    [[nodiscard]] PackageType supported_type() const override {
        return PackageType::Layer;
    }

    [[nodiscard]] const char* name() const override {
        return "LayerPackageLoader";
    }

    [[nodiscard]] void_core::Result<void> load(
        const ResolvedPackage& package,
        LoadContext& ctx) override
    {
        // Check if already loaded
        if (is_loaded(package.manifest.name)) {
            return void_core::Err("Layer package already loaded: " + package.manifest.name);
        }

        // Configure applier with services from context
        if (auto* prefab_reg = ctx.get_service<PrefabRegistry>()) {
            m_applier->set_prefab_registry(prefab_reg);
        }
        if (auto* schema_reg = ctx.get_service<ComponentSchemaRegistry>()) {
            m_applier->set_schema_registry(schema_reg);
        }

        // Stage the layer (parse but don't apply)
        auto stage_result = m_applier->stage(package);
        if (!stage_result) {
            return void_core::Err("Failed to stage layer: " + stage_result.error().message());
        }

        m_loaded_packages.insert(package.manifest.name);

        return void_core::Ok();
    }

    [[nodiscard]] void_core::Result<void> unload(
        const std::string& package_name,
        LoadContext& ctx) override
    {
        auto it = m_loaded_packages.find(package_name);
        if (it == m_loaded_packages.end()) {
            return void_core::Err("Layer package not loaded: " + package_name);
        }

        // If layer is applied, unapply it first
        if (m_applier->is_applied(package_name)) {
            auto* world = ctx.ecs_world();
            if (world) {
                auto unapply_result = m_applier->unapply(package_name, *world);
                if (!unapply_result) {
                    return void_core::Err("Failed to unapply layer: " +
                        unapply_result.error().message());
                }
            }
        }

        // Unstage the layer
        m_applier->unstage(package_name);

        m_loaded_packages.erase(it);

        return void_core::Ok();
    }

    [[nodiscard]] bool supports_hot_reload() const override { return true; }

    [[nodiscard]] bool is_loaded(const std::string& package_name) const override {
        return m_loaded_packages.find(package_name) != m_loaded_packages.end();
    }

    [[nodiscard]] std::vector<std::string> loaded_packages() const override {
        return {m_loaded_packages.begin(), m_loaded_packages.end()};
    }

    // =========================================================================
    // Layer-Specific API
    // =========================================================================

    /// Get the layer applier
    [[nodiscard]] LayerApplier& applier() { return *m_applier; }
    [[nodiscard]] const LayerApplier& applier() const { return *m_applier; }

    /// Apply a loaded layer to the world
    [[nodiscard]] void_core::Result<void> apply_layer(
        const std::string& layer_name,
        void_ecs::World& world)
    {
        return m_applier->apply(layer_name, world);
    }

    /// Unapply a layer from the world
    [[nodiscard]] void_core::Result<void> unapply_layer(
        const std::string& layer_name,
        void_ecs::World& world)
    {
        return m_applier->unapply(layer_name, world);
    }

    /// Check if a layer is currently applied
    [[nodiscard]] bool is_layer_applied(const std::string& layer_name) const {
        return m_applier->is_applied(layer_name);
    }

    /// Get names of all applied layers
    [[nodiscard]] std::vector<std::string> applied_layers() const {
        return m_applier->applied_layer_names();
    }

    /// Update all spawners (call each frame)
    void update_spawners(void_ecs::World& world, float dt) {
        m_applier->update_spawners(world, dt);
    }

private:
    std::unique_ptr<LayerApplier> m_applier;
    std::set<std::string> m_loaded_packages;
};

// =============================================================================
// Factory Function
// =============================================================================

/// Create a layer package loader
std::unique_ptr<PackageLoader> create_layer_package_loader() {
    return std::make_unique<LayerPackageLoader>();
}

} // namespace void_package
