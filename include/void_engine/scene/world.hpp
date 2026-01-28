/// @file world.hpp
/// @brief World - The unified scene/world concept
///
/// Architecture (from doc/review):
/// - Scene == World
/// - World owns one ECS world (entities + components)
/// - World manages active layers, plugins, widgets
/// - World holds spatial context (XR anchors, VR origin)
/// - ECS is authoritative; graphs are derived views
///
/// A World is the live runtime representation of a loaded scene.
/// SceneData is the serialization format; World is the runtime state.

#pragma once

#include <void_engine/ecs/world.hpp>
#include <void_engine/core/error.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>

namespace void_event { class EventBus; }

namespace void_scene {

// =============================================================================
// Spatial Context
// =============================================================================

/// @brief Spatial context for XR and world-space anchoring
struct SpatialContext {
    /// XR reference space type (local, stage, unbounded)
    std::string reference_space{"local"};

    /// World origin offset (for large world coordinates)
    std::array<double, 3> world_origin{0.0, 0.0, 0.0};

    /// VR play area bounds (if applicable)
    std::array<float, 2> play_area_bounds{2.0f, 2.0f};

    /// Is the spatial context anchored to a physical location?
    bool is_anchored{false};

    /// Anchor ID for persistent spatial anchors
    std::string anchor_id;
};

// =============================================================================
// Layer Reference
// =============================================================================

/// @brief Reference to an active layer in the world
struct ActiveLayer {
    std::string layer_id;
    int priority{0};
    bool enabled{true};
};

// =============================================================================
// World
// =============================================================================

/// @brief The unified scene/world concept
///
/// World is the live runtime representation of a loaded scene.
/// It owns:
/// - The ECS world (authoritative entity/component storage)
/// - Active layers (additive patches to the world)
/// - Active plugins (gameplay systems)
/// - Active widgets (reactive UI views)
/// - Spatial context (XR anchoring)
///
/// Usage:
/// ```cpp
/// World world("main_menu");
/// world.initialize(event_bus);
///
/// // ECS is authoritative
/// auto entity = world.ecs().spawn();
/// world.ecs().add_component(entity, Position{0, 0, 0});
///
/// // Activate layers/plugins/widgets
/// world.activate_layer("lighting_layer");
/// world.activate_plugin("combat_plugin");
/// world.activate_widget_set("hud_widgets");
///
/// // Spatial context for XR
/// world.set_spatial_context(SpatialContext{.reference_space = "stage"});
/// ```
class World {
public:
    using LayerActivatedCallback = std::function<void(const std::string& layer_id)>;
    using LayerDeactivatedCallback = std::function<void(const std::string& layer_id)>;
    using PluginActivatedCallback = std::function<void(const std::string& plugin_id)>;
    using PluginDeactivatedCallback = std::function<void(const std::string& plugin_id)>;

    /// @brief Construct a world with the given ID
    explicit World(std::string world_id);

    ~World();

    // Non-copyable
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Movable
    World(World&&) noexcept;
    World& operator=(World&&) noexcept;

    // -------------------------------------------------------------------------
    // Identity
    // -------------------------------------------------------------------------

    /// @brief Get the world ID
    [[nodiscard]] const std::string& id() const noexcept { return m_world_id; }

    // -------------------------------------------------------------------------
    // ECS Access (Authoritative)
    // -------------------------------------------------------------------------

    /// @brief Get the ECS world (read-only)
    [[nodiscard]] const void_ecs::World& ecs() const noexcept { return m_ecs; }

    /// @brief Get the ECS world (mutable)
    [[nodiscard]] void_ecs::World& ecs() noexcept { return m_ecs; }

    /// @brief Get entity count
    [[nodiscard]] std::size_t entity_count() const noexcept { return m_ecs.entity_count(); }

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// @brief Initialize the world
    [[nodiscard]] void_core::Result<void> initialize(void_event::EventBus* event_bus);

    /// @brief Update the world (called each frame)
    void update(float dt);

    /// @brief Clear all entities and reset world state
    void clear();

    // -------------------------------------------------------------------------
    // Layer Management
    // -------------------------------------------------------------------------

    /// @brief Activate a layer by ID
    /// Layers are additive patches - they add/modify entities but don't own them
    [[nodiscard]] void_core::Result<void> activate_layer(const std::string& layer_id, int priority = 0);

    /// @brief Deactivate a layer by ID
    void deactivate_layer(const std::string& layer_id);

    /// @brief Check if a layer is active
    [[nodiscard]] bool is_layer_active(const std::string& layer_id) const;

    /// @brief Get all active layers
    [[nodiscard]] const std::vector<ActiveLayer>& active_layers() const noexcept { return m_active_layers; }

    /// @brief Set callback for layer activation
    void on_layer_activated(LayerActivatedCallback callback) { m_on_layer_activated = std::move(callback); }

    /// @brief Set callback for layer deactivation
    void on_layer_deactivated(LayerDeactivatedCallback callback) { m_on_layer_deactivated = std::move(callback); }

    // -------------------------------------------------------------------------
    // Plugin Management
    // -------------------------------------------------------------------------

    /// @brief Activate a plugin for this world
    [[nodiscard]] void_core::Result<void> activate_plugin(const std::string& plugin_id);

    /// @brief Deactivate a plugin for this world
    void deactivate_plugin(const std::string& plugin_id);

    /// @brief Check if a plugin is active
    [[nodiscard]] bool is_plugin_active(const std::string& plugin_id) const;

    /// @brief Get all active plugins
    [[nodiscard]] const std::unordered_set<std::string>& active_plugins() const noexcept { return m_active_plugins; }

    /// @brief Set callback for plugin activation
    void on_plugin_activated(PluginActivatedCallback callback) { m_on_plugin_activated = std::move(callback); }

    /// @brief Set callback for plugin deactivation
    void on_plugin_deactivated(PluginDeactivatedCallback callback) { m_on_plugin_deactivated = std::move(callback); }

    // -------------------------------------------------------------------------
    // Widget Management
    // -------------------------------------------------------------------------

    /// @brief Activate a widget set for this world
    [[nodiscard]] void_core::Result<void> activate_widget_set(const std::string& widget_set_id);

    /// @brief Deactivate a widget set for this world
    void deactivate_widget_set(const std::string& widget_set_id);

    /// @brief Check if a widget set is active
    [[nodiscard]] bool is_widget_set_active(const std::string& widget_set_id) const;

    /// @brief Get all active widget sets
    [[nodiscard]] const std::unordered_set<std::string>& active_widget_sets() const noexcept { return m_active_widget_sets; }

    // -------------------------------------------------------------------------
    // Spatial Context
    // -------------------------------------------------------------------------

    /// @brief Set the spatial context
    void set_spatial_context(SpatialContext context) { m_spatial_context = std::move(context); }

    /// @brief Get the spatial context
    [[nodiscard]] const SpatialContext& spatial_context() const noexcept { return m_spatial_context; }

    /// @brief Get mutable spatial context
    [[nodiscard]] SpatialContext& spatial_context() noexcept { return m_spatial_context; }

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    /// @brief Check if world is initialized
    [[nodiscard]] bool is_initialized() const noexcept { return m_initialized; }

    /// @brief Get the event bus
    [[nodiscard]] void_event::EventBus* event_bus() const noexcept { return m_event_bus; }

private:
    std::string m_world_id;
    void_ecs::World m_ecs;
    void_event::EventBus* m_event_bus{nullptr};
    bool m_initialized{false};

    // Active layers (sorted by priority)
    std::vector<ActiveLayer> m_active_layers;

    // Active plugins
    std::unordered_set<std::string> m_active_plugins;

    // Active widget sets
    std::unordered_set<std::string> m_active_widget_sets;

    // Spatial context
    SpatialContext m_spatial_context;

    // Callbacks
    LayerActivatedCallback m_on_layer_activated;
    LayerDeactivatedCallback m_on_layer_deactivated;
    PluginActivatedCallback m_on_plugin_activated;
    PluginDeactivatedCallback m_on_plugin_deactivated;
};

// =============================================================================
// World Events
// =============================================================================

/// @brief Event published when a world is created
struct WorldCreatedEvent {
    std::string world_id;
    World* world;
};

/// @brief Event published when a world is destroyed
struct WorldDestroyedEvent {
    std::string world_id;
};

/// @brief Event published when a layer is activated
struct LayerActivatedEvent {
    std::string world_id;
    std::string layer_id;
    int priority;
};

/// @brief Event published when a layer is deactivated
struct LayerDeactivatedEvent {
    std::string world_id;
    std::string layer_id;
};

/// @brief Event published when a plugin is activated for a world
struct WorldPluginActivatedEvent {
    std::string world_id;
    std::string plugin_id;
};

/// @brief Event published when a plugin is deactivated for a world
struct WorldPluginDeactivatedEvent {
    std::string world_id;
    std::string plugin_id;
};

} // namespace void_scene
