/// @file context.hpp
/// @brief Plugin context providing engine access for package-based plugins
///
/// PluginContext is passed to IPlugin::on_load() and on_unload(). It provides
/// all the APIs a plugin needs to integrate with the engine:
/// - Component registration with the package ComponentSchemaRegistry
/// - System registration with kernel stages
/// - Event subscription
/// - ECS World access (read and write)
/// - Resource access
/// - Render contract (make_renderable)
///
/// IMPORTANT: Component registration uses void_package::ComponentSchemaRegistry
/// which is the canonical way to register components in the package-driven architecture.
/// The ECS itself is void_ecs, but component schemas and JSON factories go through
/// the package system.

#pragma once

#include "renderable.hpp"

#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/entity.hpp>
#include <void_engine/kernel/types.hpp>
#include <void_engine/package/component_schema.hpp>
#include <void_engine/core/error.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

// Forward declarations
namespace void_kernel {
class IKernel;
}

namespace void_event {
class EventBus;
}

namespace void_ecs {
class World;
}

namespace void_package {
class ComponentSchemaRegistry;
}

namespace void_plugin_api {

// =============================================================================
// Component Registration (uses void_package::ComponentSchemaRegistry)
// =============================================================================

/// @brief Component registration info tracked by PluginContext
///
/// Note: The actual JSON→component conversion is handled by
/// void_package::ComponentSchemaRegistry. This struct tracks what
/// this plugin has registered for cleanup on unload.
struct ComponentRegistration {
    std::string name;               ///< Component name (e.g., "Health")
    std::string plugin_id;          ///< Plugin that registered it
    std::type_index type;           ///< C++ type_index for the component
    void_ecs::ComponentId comp_id;  ///< ECS component ID
};

// =============================================================================
// System Registration
// =============================================================================

/// @brief System registration info tracked by PluginContext
struct SystemRegistration {
    std::string name;               ///< System name (e.g., "HealthRegenSystem")
    std::string plugin_id;          ///< Plugin that registered it
    void_kernel::Stage stage;       ///< Kernel stage to run in
    std::int32_t priority;          ///< Priority within stage
};

// =============================================================================
// Event Subscription
// =============================================================================

/// @brief Subscription handle for unsubscribing
struct SubscriptionHandle {
    std::uint64_t id{0};
    std::string event_name;
    std::string plugin_id;

    [[nodiscard]] bool is_valid() const { return id != 0; }
};

// =============================================================================
// Engine Render Component IDs
// =============================================================================

/// @brief IDs for engine render components
///
/// Plugins use these to add render components to entities or check
/// if entities have render components.
struct RenderComponentIds {
    void_ecs::ComponentId transform{0};
    void_ecs::ComponentId mesh{0};
    void_ecs::ComponentId material{0};
    void_ecs::ComponentId light{0};
    void_ecs::ComponentId camera{0};
    void_ecs::ComponentId renderable_tag{0};
    void_ecs::ComponentId hierarchy{0};

    /// Check if all render components are registered
    [[nodiscard]] bool is_complete() const {
        return transform.value() != 0 &&
               mesh.value() != 0 &&
               material.value() != 0 &&
               renderable_tag.value() != 0;
    }
};

// =============================================================================
// PluginContext
// =============================================================================

/// @brief Context providing engine access for plugins
///
/// Plugins receive a PluginContext in on_load() and on_unload().
/// This is the ONLY way plugins should interact with the engine.
///
/// Thread Safety:
/// - All registration methods must be called from the main thread
/// - world() and world_mut() return references that must not be stored
/// - System functions are called from the main thread
///
/// Lifetime:
/// - The context is valid only during on_load() and on_unload()
/// - Plugins must NOT store pointers to the context
class PluginContext {
public:
    /// @brief Construct context with engine references
    PluginContext(
        const std::string& plugin_id,
        void_ecs::World* world,
        void_kernel::IKernel* kernel,
        void_event::EventBus* events,
        void_package::ComponentSchemaRegistry* schema_registry);

    ~PluginContext();

    // Non-copyable, non-movable (reference semantics)
    PluginContext(const PluginContext&) = delete;
    PluginContext& operator=(const PluginContext&) = delete;
    PluginContext(PluginContext&&) = delete;
    PluginContext& operator=(PluginContext&&) = delete;

    // =========================================================================
    // Component Registration (via void_package::ComponentSchemaRegistry)
    // =========================================================================

    /// @brief Register a component type with the package schema registry
    ///
    /// This registers the component with void_package::ComponentSchemaRegistry,
    /// which is the canonical way to register components in the package-driven
    /// architecture. The schema registry handles JSON→component conversion.
    ///
    /// @tparam T Component type (must be default-constructible for ECS)
    /// @param name Component name used in JSON (e.g., "Health")
    /// @param applier Function to apply JSON data to entity
    /// @return Component ID for the registered component
    ///
    /// @code
    /// ctx.register_component<Health>("Health", [](void_ecs::World& world,
    ///                                              void_ecs::Entity entity,
    ///                                              const nlohmann::json& data) {
    ///     Health h;
    ///     h.current = data.value("current", 100.0f);
    ///     h.max = data.value("max", 100.0f);
    ///     world.add_component(entity, h);
    ///     return void_core::Ok();
    /// });
    /// @endcode
    template<typename T>
    void_ecs::ComponentId register_component(const std::string& name,
                                              void_package::ComponentApplier applier);

    /// @brief Register a simple component without JSON applier
    ///
    /// Use this for components that don't need JSON deserialization
    /// (e.g., tag components or components only added programmatically).
    template<typename T>
    void_ecs::ComponentId register_component(const std::string& name);

    /// @brief Get component ID by name
    ///
    /// Returns nullopt if component is not registered.
    [[nodiscard]] std::optional<void_ecs::ComponentId> get_component_id(
        const std::string& name) const;

    /// @brief Apply component to entity from JSON using schema registry
    ///
    /// @param component_name Name of the component type
    /// @param json_data JSON object with component data
    /// @param entity Target entity
    /// @return Ok() on success, Error if applier fails or component not found
    [[nodiscard]] void_core::Result<void> apply_component_from_json(
        const std::string& component_name,
        const nlohmann::json& json_data,
        void_ecs::Entity entity);

    // =========================================================================
    // System Registration
    // =========================================================================

    /// @brief Register a system to run in a kernel stage
    ///
    /// @param stage Execution stage (Update, FixedUpdate, Render, etc.)
    /// @param name Unique system name (convention: "plugin_id.SystemName")
    /// @param func System function called each frame
    /// @param priority Execution order within stage (lower = earlier, default 0)
    ///
    /// @code
    /// ctx.register_system(
    ///     void_kernel::Stage::Update,
    ///     "base.health.HealthRegenSystem",
    ///     [this](float dt) { run_health_regen(dt); },
    ///     10  // Run after default priority
    /// );
    /// @endcode
    void register_system(void_kernel::Stage stage,
                         const std::string& name,
                         void_kernel::SystemFunc func,
                         std::int32_t priority = 0);

    /// @brief Unregister a previously registered system
    ///
    /// @note Systems are automatically unregistered when the plugin unloads,
    /// but this can be used to dynamically disable systems.
    void unregister_system(void_kernel::Stage stage, const std::string& name);

    // =========================================================================
    // Event Subscription
    // =========================================================================

    /// @brief Subscribe to an event type
    ///
    /// @tparam E Event type (must be registered with EventBus)
    /// @param handler Function called when event is dispatched
    /// @return Handle for unsubscribing
    ///
    /// @code
    /// auto handle = ctx.subscribe<DamageEvent>([this](const DamageEvent& e) {
    ///     handle_damage(e);
    /// });
    /// @endcode
    template<typename E>
    SubscriptionHandle subscribe(std::function<void(const E&)> handler);

    /// @brief Unsubscribe from an event
    ///
    /// @note Subscriptions are automatically removed when the plugin unloads.
    void unsubscribe(const SubscriptionHandle& handle);

    // =========================================================================
    // ECS Access
    // =========================================================================

    /// @brief Get read-only access to the ECS world
    [[nodiscard]] const void_ecs::World& world() const;

    /// @brief Get mutable access to the ECS world
    [[nodiscard]] void_ecs::World& world_mut();

    // =========================================================================
    // Resource Access
    // =========================================================================

    /// @brief Get read-only access to an ECS resource
    ///
    /// @tparam R Resource type
    /// @return Pointer to resource, or nullptr if not found
    template<typename R>
    [[nodiscard]] const R* resource() const;

    /// @brief Get mutable access to an ECS resource
    ///
    /// @tparam R Resource type
    /// @return Pointer to resource, or nullptr if not found
    template<typename R>
    [[nodiscard]] R* resource_mut();

    // =========================================================================
    // Engine Services
    // =========================================================================

    /// @brief Get the event bus for publishing events
    [[nodiscard]] void_event::EventBus* events() const { return m_events; }

    /// @brief Get the kernel for advanced operations
    [[nodiscard]] void_kernel::IKernel* kernel() const { return m_kernel; }

    // =========================================================================
    // Render Contract
    // =========================================================================

    /// @brief Get IDs of engine render components
    ///
    /// Use these to check if entities have render components or to add
    /// render components programmatically.
    [[nodiscard]] const RenderComponentIds& render_components() const {
        return m_render_ids;
    }

    /// @brief Make an entity renderable using a high-level description
    ///
    /// This is the PLUGIN RENDER CONTRACT. Plugins describe WHAT they want
    /// rendered (mesh, material, visibility), and the engine handles HOW
    /// (adds correct engine components, handles batching, etc.).
    ///
    /// @param entity The entity to make renderable
    /// @param desc Description of render properties
    /// @return Ok() on success, Error if entity doesn't exist
    ///
    /// @code
    /// RenderableDesc desc;
    /// desc.mesh_builtin = "sphere";
    /// desc.material.albedo = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
    /// desc.material.metallic = 0.0f;
    /// desc.material.roughness = 0.8f;
    /// ctx.make_renderable(enemy_entity, desc);
    /// @endcode
    [[nodiscard]] void_core::Result<void> make_renderable(
        void_ecs::Entity entity,
        const RenderableDesc& desc);

    // =========================================================================
    // Plugin Information
    // =========================================================================

    /// @brief Get the ID of the plugin this context belongs to
    [[nodiscard]] const std::string& plugin_id() const { return m_plugin_id; }

    /// @brief Get list of components registered by this plugin
    [[nodiscard]] const std::vector<ComponentRegistration>& registered_components() const {
        return m_registered_components;
    }

    /// @brief Get list of systems registered by this plugin
    [[nodiscard]] const std::vector<SystemRegistration>& registered_systems() const {
        return m_registered_systems;
    }

    /// @brief Get list of active event subscriptions
    [[nodiscard]] const std::vector<SubscriptionHandle>& subscriptions() const {
        return m_subscriptions;
    }

    // =========================================================================
    // Internal (called by PluginPackageLoader)
    // =========================================================================

    /// @brief Set render component IDs (called during engine init)
    void set_render_component_ids(const RenderComponentIds& ids) {
        m_render_ids = ids;
    }

    /// @brief Unregister all systems registered by this plugin
    void unregister_all_systems();

    /// @brief Unsubscribe from all events
    void unsubscribe_all();

    /// @brief Get the component schema registry
    [[nodiscard]] void_package::ComponentSchemaRegistry* schema_registry() const {
        return m_schema_registry;
    }

private:
    std::string m_plugin_id;
    void_ecs::World* m_world;
    void_kernel::IKernel* m_kernel;
    void_event::EventBus* m_events;
    void_package::ComponentSchemaRegistry* m_schema_registry;

    RenderComponentIds m_render_ids;

    // Tracking for cleanup
    std::vector<ComponentRegistration> m_registered_components;
    std::vector<SystemRegistration> m_registered_systems;
    std::vector<SubscriptionHandle> m_subscriptions;

    // Subscription ID counter
    static std::uint64_t s_next_subscription_id;
};

// =============================================================================
// Template Implementations
// =============================================================================

template<typename T>
void_ecs::ComponentId PluginContext::register_component(const std::string& name,
                                                         void_package::ComponentApplier applier) {
    if (!m_world || !m_schema_registry) {
        return void_ecs::ComponentId{0};
    }

    // Register with ECS first to get the component ID
    void_ecs::ComponentId comp_id = m_world->register_component<T>();

    // Create a schema for this component
    void_package::ComponentSchema schema;
    schema.name = name;
    schema.source_plugin = m_plugin_id;
    schema.size = sizeof(T);
    schema.alignment = alignof(T);
    schema.is_tag = (sizeof(T) == 1);  // Assume 1-byte components are tags

    // Register with schema registry
    if (applier) {
        auto result = m_schema_registry->register_schema_with_factory(
            std::move(schema),
            nullptr,  // No byte factory needed
            std::move(applier)
        );
        if (!result) {
            // Schema registration failed, but ECS component is registered
            // This is acceptable - just means JSON loading won't work
        }
    } else {
        auto result = m_schema_registry->register_schema(std::move(schema));
        if (!result) {
            // Schema registration failed
        }
    }

    // Track registration
    m_registered_components.push_back(ComponentRegistration{
        .name = name,
        .plugin_id = m_plugin_id,
        .type = std::type_index(typeid(T)),
        .comp_id = comp_id
    });

    return comp_id;
}

template<typename T>
void_ecs::ComponentId PluginContext::register_component(const std::string& name) {
    // Register without applier - components must be added programmatically
    return register_component<T>(name, nullptr);
}

template<typename R>
const R* PluginContext::resource() const {
    if (!m_world) return nullptr;
    return m_world->resource<R>();
}

template<typename R>
R* PluginContext::resource_mut() {
    if (!m_world) return nullptr;
    return m_world->resource<R>();
}

template<typename E>
SubscriptionHandle PluginContext::subscribe(std::function<void(const E&)> handler) {
    SubscriptionHandle handle;
    handle.id = ++s_next_subscription_id;
    handle.event_name = typeid(E).name();
    handle.plugin_id = m_plugin_id;

    // TODO: Integrate with void_event::EventBus when event system is available
    // For now, just track the subscription
    m_subscriptions.push_back(handle);

    return handle;
}

} // namespace void_plugin_api
