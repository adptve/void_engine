/// @file context.cpp
/// @brief Implementation of PluginContext for package-based plugins

#include <void_engine/plugin_api/context.hpp>
#include <void_engine/kernel/kernel.hpp>
#include <void_engine/render/components.hpp>
#include <void_engine/package/component_schema.hpp>

#include <spdlog/spdlog.h>

namespace void_plugin_api {

// Static member initialization
std::uint64_t PluginContext::s_next_subscription_id = 0;

// =============================================================================
// Constructor / Destructor
// =============================================================================

PluginContext::PluginContext(
    const std::string& plugin_id,
    void_ecs::World* world,
    void_kernel::IKernel* kernel,
    void_event::EventBus* events,
    void_package::ComponentSchemaRegistry* schema_registry)
    : m_plugin_id(plugin_id)
    , m_world(world)
    , m_kernel(kernel)
    , m_events(events)
    , m_schema_registry(schema_registry)
{
    spdlog::debug("[PluginContext] Created for plugin '{}' (schema_registry={})",
                  plugin_id, static_cast<void*>(schema_registry));
}

PluginContext::~PluginContext() {
    spdlog::debug("[PluginContext] Destroyed for plugin '{}'", m_plugin_id);
}

// =============================================================================
// Component Registration
// =============================================================================

std::optional<void_ecs::ComponentId> PluginContext::get_component_id(
    const std::string& name) const
{
    // First check the schema registry (the canonical source)
    if (m_schema_registry) {
        auto id = m_schema_registry->get_component_id(name);
        if (id) {
            return id;
        }
    }

    // Then check registered components from this plugin
    for (const auto& reg : m_registered_components) {
        if (reg.name == name) {
            return reg.comp_id;
        }
    }

    // Finally check ECS world's component registry by name
    if (m_world) {
        return m_world->component_id_by_name(name);
    }

    return std::nullopt;
}

void_core::Result<void> PluginContext::apply_component_from_json(
    const std::string& component_name,
    const nlohmann::json& json_data,
    void_ecs::Entity entity)
{
    if (!m_world) {
        return void_core::Error{"PluginContext: No world available"};
    }

    if (!m_schema_registry) {
        return void_core::Error{"PluginContext: No schema registry available"};
    }

    // Use the schema registry to apply the component
    return m_schema_registry->apply_to_entity(*m_world, entity, component_name, json_data);
}

// =============================================================================
// System Registration
// =============================================================================

void PluginContext::register_system(void_kernel::Stage stage,
                                     const std::string& name,
                                     void_kernel::SystemFunc func,
                                     std::int32_t priority)
{
    if (!m_kernel) {
        spdlog::warn("[PluginContext] Cannot register system '{}': no kernel", name);
        return;
    }

    m_kernel->register_system(stage, name, std::move(func), priority);

    m_registered_systems.push_back(SystemRegistration{
        .name = name,
        .plugin_id = m_plugin_id,
        .stage = stage,
        .priority = priority
    });

    spdlog::debug("[PluginContext] Plugin '{}' registered system '{}' in stage {} with priority {}",
                  m_plugin_id, name, void_kernel::to_string(stage), priority);
}

void PluginContext::unregister_system(void_kernel::Stage stage, const std::string& name) {
    if (!m_kernel) {
        return;
    }

    m_kernel->unregister_system(stage, name);

    // Remove from tracking
    m_registered_systems.erase(
        std::remove_if(m_registered_systems.begin(), m_registered_systems.end(),
            [&name](const SystemRegistration& reg) { return reg.name == name; }),
        m_registered_systems.end()
    );

    spdlog::debug("[PluginContext] Plugin '{}' unregistered system '{}'",
                  m_plugin_id, name);
}

void PluginContext::unregister_all_systems() {
    if (!m_kernel) {
        return;
    }

    for (const auto& sys : m_registered_systems) {
        m_kernel->unregister_system(sys.stage, sys.name);
        spdlog::debug("[PluginContext] Unregistered system '{}' (plugin unload)",
                      sys.name);
    }
    m_registered_systems.clear();
}

// =============================================================================
// Event Subscription
// =============================================================================

void PluginContext::unsubscribe(const SubscriptionHandle& handle) {
    if (!handle.is_valid()) {
        return;
    }

    // TODO: Integrate with void_event::EventBus
    // For now, just remove from tracking
    m_subscriptions.erase(
        std::remove_if(m_subscriptions.begin(), m_subscriptions.end(),
            [&handle](const SubscriptionHandle& h) { return h.id == handle.id; }),
        m_subscriptions.end()
    );

    spdlog::debug("[PluginContext] Plugin '{}' unsubscribed from event '{}'",
                  m_plugin_id, handle.event_name);
}

void PluginContext::unsubscribe_all() {
    // TODO: Integrate with void_event::EventBus
    for (const auto& sub : m_subscriptions) {
        spdlog::debug("[PluginContext] Unsubscribed from '{}' (plugin unload)",
                      sub.event_name);
    }
    m_subscriptions.clear();
}

// =============================================================================
// ECS Access
// =============================================================================

const void_ecs::World& PluginContext::world() const {
    if (!m_world) {
        spdlog::error("[PluginContext] Attempted to access null world");
        // This is a programming error - crash intentionally
        std::terminate();
    }
    return *m_world;
}

void_ecs::World& PluginContext::world_mut() {
    if (!m_world) {
        spdlog::error("[PluginContext] Attempted to access null world");
        std::terminate();
    }
    return *m_world;
}

// =============================================================================
// Render Contract
// =============================================================================

void_core::Result<void> PluginContext::make_renderable(
    void_ecs::Entity entity,
    const RenderableDesc& desc)
{
    if (!m_world) {
        return void_core::Error{"PluginContext: No world available"};
    }

    if (!m_world->is_alive(entity)) {
        return void_core::Error{"PluginContext: Entity is not alive"};
    }

    // Validate description
    if (!desc.is_valid()) {
        return void_core::Error{"PluginContext: Invalid RenderableDesc (no mesh and visible)"};
    }

    // Add or update TransformComponent
    if (desc.override_transform || !m_world->has_component<void_render::TransformComponent>(entity)) {
        void_render::TransformComponent transform;
        transform.position = desc.position;
        transform.rotation = desc.rotation;
        transform.scale = desc.scale;
        transform.dirty = true;
        m_world->add_component(entity, transform);
    }

    // Add MeshComponent if we have a mesh source
    if (desc.has_mesh()) {
        void_render::MeshComponent mesh;
        if (!desc.mesh_builtin.empty()) {
            mesh = void_render::MeshComponent::builtin(desc.mesh_builtin);
        } else {
            // For asset meshes, we create a component with path
            // The actual loading happens via ModelComponent/AssetSystem
            mesh.builtin_mesh = "";  // Clear builtin
            mesh.submesh_index = desc.submesh_index;
            // Note: mesh_handle will be set by asset loading system
        }
        m_world->add_component(entity, mesh);
    }

    // Add MaterialComponent
    {
        void_render::MaterialComponent mat;
        mat.albedo = desc.material.albedo;
        mat.metallic_value = desc.material.metallic;
        mat.roughness_value = desc.material.roughness;
        mat.ao_value = desc.material.ao;
        mat.emissive = desc.material.emissive;
        mat.emissive_strength = desc.material.emissive_strength;
        mat.double_sided = desc.material.double_sided;
        mat.alpha_blend = desc.material.alpha_blend;
        mat.alpha_cutoff = desc.material.alpha_cutoff;
        // Note: texture handles will be set by asset loading system
        m_world->add_component(entity, mat);
    }

    // Add RenderableTag
    {
        void_render::RenderableTag tag;
        tag.visible = desc.visible;
        tag.layer_mask = desc.layer_mask;
        tag.render_order = desc.render_order;
        m_world->add_component(entity, tag);
    }

    spdlog::trace("[PluginContext] Made entity {} renderable with mesh '{}'",
                  entity.index,
                  desc.mesh_builtin.empty() ? desc.mesh_asset : desc.mesh_builtin);

    return void_core::Ok();
}

} // namespace void_plugin_api
