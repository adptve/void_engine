/// @file world.cpp
/// @brief World implementation - unified scene/world concept

#include <void_engine/scene/world.hpp>
#include <void_engine/event/event_bus.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace void_scene {

// =============================================================================
// Constructor / Destructor
// =============================================================================

World::World(std::string world_id)
    : m_world_id(std::move(world_id)) {
}

World::~World() {
    if (m_initialized) {
        clear();
    }
}

World::World(World&& other) noexcept
    : m_world_id(std::move(other.m_world_id))
    , m_ecs(std::move(other.m_ecs))
    , m_event_bus(other.m_event_bus)
    , m_initialized(other.m_initialized)
    , m_active_layers(std::move(other.m_active_layers))
    , m_active_plugins(std::move(other.m_active_plugins))
    , m_active_widget_sets(std::move(other.m_active_widget_sets))
    , m_spatial_context(std::move(other.m_spatial_context))
    , m_on_layer_activated(std::move(other.m_on_layer_activated))
    , m_on_layer_deactivated(std::move(other.m_on_layer_deactivated))
    , m_on_plugin_activated(std::move(other.m_on_plugin_activated))
    , m_on_plugin_deactivated(std::move(other.m_on_plugin_deactivated)) {
    other.m_event_bus = nullptr;
    other.m_initialized = false;
}

World& World::operator=(World&& other) noexcept {
    if (this != &other) {
        if (m_initialized) {
            clear();
        }

        m_world_id = std::move(other.m_world_id);
        m_ecs = std::move(other.m_ecs);
        m_event_bus = other.m_event_bus;
        m_initialized = other.m_initialized;
        m_active_layers = std::move(other.m_active_layers);
        m_active_plugins = std::move(other.m_active_plugins);
        m_active_widget_sets = std::move(other.m_active_widget_sets);
        m_spatial_context = std::move(other.m_spatial_context);
        m_on_layer_activated = std::move(other.m_on_layer_activated);
        m_on_layer_deactivated = std::move(other.m_on_layer_deactivated);
        m_on_plugin_activated = std::move(other.m_on_plugin_activated);
        m_on_plugin_deactivated = std::move(other.m_on_plugin_deactivated);

        other.m_event_bus = nullptr;
        other.m_initialized = false;
    }
    return *this;
}

// =============================================================================
// Lifecycle
// =============================================================================

void_core::Result<void> World::initialize(void_event::EventBus* event_bus) {
    if (m_initialized) {
        return void_core::Error("World already initialized");
    }

    m_event_bus = event_bus;
    m_initialized = true;

    spdlog::info("World '{}' initialized", m_world_id);

    // Publish creation event
    if (m_event_bus) {
        m_event_bus->publish(WorldCreatedEvent{m_world_id, this});
    }

    return void_core::Ok();
}

void World::update(float dt) {
    (void)dt;
    // World update logic
    // ECS systems are executed via Kernel stages, not here
    // This is for world-level coordination if needed
}

void World::clear() {
    spdlog::info("Clearing world '{}'", m_world_id);

    // Deactivate all widget sets
    for (auto it = m_active_widget_sets.begin(); it != m_active_widget_sets.end(); ) {
        std::string widget_set_id = *it;
        it = m_active_widget_sets.erase(it);
        spdlog::debug("  Deactivated widget set: {}", widget_set_id);
    }

    // Deactivate all plugins
    for (auto it = m_active_plugins.begin(); it != m_active_plugins.end(); ) {
        std::string plugin_id = *it;
        it = m_active_plugins.erase(it);

        if (m_on_plugin_deactivated) {
            m_on_plugin_deactivated(plugin_id);
        }

        if (m_event_bus) {
            m_event_bus->publish(WorldPluginDeactivatedEvent{m_world_id, plugin_id});
        }

        spdlog::debug("  Deactivated plugin: {}", plugin_id);
    }

    // Deactivate all layers
    while (!m_active_layers.empty()) {
        std::string layer_id = m_active_layers.back().layer_id;
        m_active_layers.pop_back();

        if (m_on_layer_deactivated) {
            m_on_layer_deactivated(layer_id);
        }

        if (m_event_bus) {
            m_event_bus->publish(LayerDeactivatedEvent{m_world_id, layer_id});
        }

        spdlog::debug("  Deactivated layer: {}", layer_id);
    }

    // Clear ECS world
    m_ecs.clear();

    spdlog::info("World '{}' cleared", m_world_id);
}

// =============================================================================
// Layer Management
// =============================================================================

void_core::Result<void> World::activate_layer(const std::string& layer_id, int priority) {
    if (is_layer_active(layer_id)) {
        return void_core::Error("Layer '" + layer_id + "' is already active");
    }

    ActiveLayer layer{layer_id, priority, true};
    m_active_layers.push_back(layer);

    // Sort by priority (lower priority first)
    std::sort(m_active_layers.begin(), m_active_layers.end(),
              [](const ActiveLayer& a, const ActiveLayer& b) {
                  return a.priority < b.priority;
              });

    spdlog::info("Activated layer '{}' with priority {} in world '{}'",
                 layer_id, priority, m_world_id);

    if (m_on_layer_activated) {
        m_on_layer_activated(layer_id);
    }

    if (m_event_bus) {
        m_event_bus->publish(LayerActivatedEvent{m_world_id, layer_id, priority});
    }

    return void_core::Ok();
}

void World::deactivate_layer(const std::string& layer_id) {
    auto it = std::find_if(m_active_layers.begin(), m_active_layers.end(),
                           [&layer_id](const ActiveLayer& layer) {
                               return layer.layer_id == layer_id;
                           });

    if (it == m_active_layers.end()) {
        return;
    }

    m_active_layers.erase(it);

    spdlog::info("Deactivated layer '{}' in world '{}'", layer_id, m_world_id);

    if (m_on_layer_deactivated) {
        m_on_layer_deactivated(layer_id);
    }

    if (m_event_bus) {
        m_event_bus->publish(LayerDeactivatedEvent{m_world_id, layer_id});
    }
}

bool World::is_layer_active(const std::string& layer_id) const {
    return std::any_of(m_active_layers.begin(), m_active_layers.end(),
                       [&layer_id](const ActiveLayer& layer) {
                           return layer.layer_id == layer_id;
                       });
}

// =============================================================================
// Plugin Management
// =============================================================================

void_core::Result<void> World::activate_plugin(const std::string& plugin_id) {
    if (is_plugin_active(plugin_id)) {
        return void_core::Error("Plugin '" + plugin_id + "' is already active");
    }

    m_active_plugins.insert(plugin_id);

    spdlog::info("Activated plugin '{}' for world '{}'", plugin_id, m_world_id);

    if (m_on_plugin_activated) {
        m_on_plugin_activated(plugin_id);
    }

    if (m_event_bus) {
        m_event_bus->publish(WorldPluginActivatedEvent{m_world_id, plugin_id});
    }

    return void_core::Ok();
}

void World::deactivate_plugin(const std::string& plugin_id) {
    if (!is_plugin_active(plugin_id)) {
        return;
    }

    m_active_plugins.erase(plugin_id);

    spdlog::info("Deactivated plugin '{}' for world '{}'", plugin_id, m_world_id);

    if (m_on_plugin_deactivated) {
        m_on_plugin_deactivated(plugin_id);
    }

    if (m_event_bus) {
        m_event_bus->publish(WorldPluginDeactivatedEvent{m_world_id, plugin_id});
    }
}

bool World::is_plugin_active(const std::string& plugin_id) const {
    return m_active_plugins.count(plugin_id) > 0;
}

// =============================================================================
// Widget Management
// =============================================================================

void_core::Result<void> World::activate_widget_set(const std::string& widget_set_id) {
    if (is_widget_set_active(widget_set_id)) {
        return void_core::Error("Widget set '" + widget_set_id + "' is already active");
    }

    m_active_widget_sets.insert(widget_set_id);

    spdlog::info("Activated widget set '{}' for world '{}'", widget_set_id, m_world_id);

    return void_core::Ok();
}

void World::deactivate_widget_set(const std::string& widget_set_id) {
    if (!is_widget_set_active(widget_set_id)) {
        return;
    }

    m_active_widget_sets.erase(widget_set_id);

    spdlog::info("Deactivated widget set '{}' for world '{}'", widget_set_id, m_world_id);
}

bool World::is_widget_set_active(const std::string& widget_set_id) const {
    return m_active_widget_sets.count(widget_set_id) > 0;
}

} // namespace void_scene
