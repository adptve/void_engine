#pragma once

/// @file prefab_registry.hpp
/// @brief Prefab registry for runtime entity instantiation
///
/// The PrefabRegistry stores entity templates (prefabs) with their component
/// data as JSON. Components are resolved by NAME at instantiation time, not
/// at compile time. This enables:
///
/// 1. External packages defining entities with components the engine doesn't know
/// 2. Plugins registering new component types at runtime
/// 3. Mods adding prefabs with custom components
///
/// CRITICAL: Component resolution happens at instantiate() time via
/// ComponentRegistry::get_id_by_name(). If a component name isn't registered,
/// instantiation fails with a clear error.

#include "fwd.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>
#include <void_engine/ecs/entity.hpp>
#include <void_engine/ecs/world.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <memory>

namespace void_package {

// Forward declarations
class ComponentSchemaRegistry;

// =============================================================================
// PrefabDefinition
// =============================================================================

/// A prefab definition storing component data as JSON
///
/// Components are stored by NAME, not by type. Resolution to ComponentId
/// happens at instantiation time.
struct PrefabDefinition {
    std::string id;                              ///< Unique prefab identifier
    std::string source_bundle;                   ///< Bundle that provided this prefab
    std::map<std::string, nlohmann::json> components;  ///< Component name -> component data
    std::vector<std::string> tags;               ///< Entity tags

    /// Check if prefab has a specific component
    [[nodiscard]] bool has_component(const std::string& name) const {
        return components.count(name) > 0;
    }

    /// Get component data (returns nullptr if not found)
    [[nodiscard]] const nlohmann::json* get_component_data(const std::string& name) const {
        auto it = components.find(name);
        return it != components.end() ? &it->second : nullptr;
    }

    /// Get number of components
    [[nodiscard]] std::size_t component_count() const { return components.size(); }
};

// =============================================================================
// TransformData
// =============================================================================

/// Transform data for overriding prefab position/rotation/scale
struct TransformData {
    std::array<float, 3> position = {0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 1.0f};  // Quaternion (x,y,z,w)
    std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};

    /// Create from JSON
    [[nodiscard]] static std::optional<TransformData> from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// InstantiationContext
// =============================================================================

/// Context provided during prefab instantiation
///
/// Contains everything needed to resolve component names to types and
/// create component instances from JSON.
struct InstantiationContext {
    void_ecs::World* world = nullptr;            ///< ECS world to spawn into
    ComponentSchemaRegistry* schema_registry = nullptr;  ///< For JSON -> bytes conversion
    std::optional<TransformData> transform_override;  ///< Optional transform override

    /// Validate context is properly configured
    [[nodiscard]] void_core::Result<void> validate() const;
};

// =============================================================================
// InstantiationResult
// =============================================================================

/// Result of prefab instantiation
struct InstantiationResult {
    void_ecs::Entity entity;                     ///< The spawned entity
    std::vector<std::string> applied_components; ///< Components that were applied
    std::vector<std::string> skipped_components; ///< Components skipped (deferred)

    /// Check if instantiation was complete (no skipped components)
    [[nodiscard]] bool is_complete() const { return skipped_components.empty(); }
};

// =============================================================================
// ComponentInstantiator
// =============================================================================

/// Function type for creating a component instance from JSON
///
/// @param data The JSON component data
/// @param world The ECS world
/// @param entity The target entity
/// @return Ok() on success, Error on failure
using ComponentInstantiator = std::function<void_core::Result<void>(
    const nlohmann::json& data,
    void_ecs::World& world,
    void_ecs::Entity entity)>;

// =============================================================================
// PrefabRegistry
// =============================================================================

/// Registry for prefab definitions
///
/// Stores prefab templates and provides runtime instantiation.
/// Components are resolved by name at instantiation time.
class PrefabRegistry {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    PrefabRegistry() = default;

    // Non-copyable, movable
    PrefabRegistry(const PrefabRegistry&) = delete;
    PrefabRegistry& operator=(const PrefabRegistry&) = delete;
    PrefabRegistry(PrefabRegistry&&) = default;
    PrefabRegistry& operator=(PrefabRegistry&&) = default;

    // =========================================================================
    // Registration
    // =========================================================================

    /// Register a prefab definition
    ///
    /// @param definition The prefab to register
    /// @return Ok if registered, Error if ID already exists (unless overwrite is allowed)
    [[nodiscard]] void_core::Result<void> register_prefab(PrefabDefinition definition);

    /// Register a prefab, allowing overwrite of existing
    ///
    /// @param definition The prefab to register
    void register_prefab_overwrite(PrefabDefinition definition);

    /// Unregister a prefab
    ///
    /// @param prefab_id The prefab ID to remove
    /// @return true if prefab was found and removed
    bool unregister_prefab(const std::string& prefab_id);

    /// Unregister all prefabs from a specific bundle
    ///
    /// @param bundle_name The bundle name
    /// @return Number of prefabs removed
    std::size_t unregister_bundle(const std::string& bundle_name);

    /// Clear all prefabs
    void clear();

    // =========================================================================
    // Component Instantiator Registration
    // =========================================================================

    /// Register a component instantiator by name
    ///
    /// This allows type-safe instantiation of known component types.
    /// The instantiator receives JSON data and creates the component.
    ///
    /// @param component_name The component name (must match prefab component keys)
    /// @param instantiator Function to create component from JSON
    void register_instantiator(const std::string& component_name,
                                ComponentInstantiator instantiator);

    /// Register a typed component instantiator
    ///
    /// Creates an instantiator that parses JSON into type T and adds to entity.
    ///
    /// @tparam T The component type
    /// @param component_name The component name
    template<typename T>
    void register_typed_instantiator(const std::string& component_name);

    /// Check if an instantiator is registered
    [[nodiscard]] bool has_instantiator(const std::string& component_name) const;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Get a prefab by ID
    ///
    /// @param prefab_id The prefab ID
    /// @return Pointer to prefab, or nullptr if not found
    [[nodiscard]] const PrefabDefinition* get(const std::string& prefab_id) const;

    /// Check if prefab exists
    [[nodiscard]] bool contains(const std::string& prefab_id) const;

    /// Get all prefab IDs
    [[nodiscard]] std::vector<std::string> all_prefab_ids() const;

    /// Get prefab IDs from a specific bundle
    [[nodiscard]] std::vector<std::string> prefabs_from_bundle(
        const std::string& bundle_name) const;

    /// Get total number of registered prefabs
    [[nodiscard]] std::size_t size() const { return m_prefabs.size(); }

    /// Check if registry is empty
    [[nodiscard]] bool empty() const { return m_prefabs.empty(); }

    // =========================================================================
    // Instantiation
    // =========================================================================

    /// Instantiate a prefab into the ECS world
    ///
    /// Creates an entity and adds all components defined in the prefab.
    /// Components are resolved by name at runtime.
    ///
    /// @param prefab_id The prefab to instantiate
    /// @param world The ECS world to spawn into
    /// @param transform_override Optional transform to override prefab default
    /// @return The spawned entity, or Error if instantiation fails
    ///
    /// Errors:
    /// - Prefab not found
    /// - Unknown component name (not registered with ECS or no instantiator)
    /// - Component data doesn't match schema
    [[nodiscard]] void_core::Result<void_ecs::Entity> instantiate(
        const std::string& prefab_id,
        void_ecs::World& world,
        const std::optional<TransformData>& transform_override = std::nullopt);

    /// Instantiate with full context (for advanced use)
    [[nodiscard]] void_core::Result<InstantiationResult> instantiate_with_context(
        const std::string& prefab_id,
        InstantiationContext& ctx);

    /// Instantiate a prefab definition directly (without lookup)
    [[nodiscard]] void_core::Result<void_ecs::Entity> instantiate_definition(
        const PrefabDefinition& definition,
        void_ecs::World& world,
        const std::optional<TransformData>& transform_override = std::nullopt);

    // =========================================================================
    // Deferred Component Handling
    // =========================================================================

    /// Policy for handling unknown components
    enum class UnknownComponentPolicy {
        Error,      ///< Fail instantiation if any component is unknown
        Skip,       ///< Skip unknown components, continue with known ones
        Defer       ///< Mark as deferred for later application
    };

    /// Set policy for unknown components
    void set_unknown_component_policy(UnknownComponentPolicy policy) {
        m_unknown_policy = policy;
    }

    /// Get current policy
    [[nodiscard]] UnknownComponentPolicy unknown_component_policy() const {
        return m_unknown_policy;
    }

    // =========================================================================
    // Schema Registry Integration
    // =========================================================================

    /// Set the component schema registry for JSON->bytes conversion
    void set_schema_registry(ComponentSchemaRegistry* registry) {
        m_schema_registry = registry;
    }

    /// Get the schema registry
    [[nodiscard]] ComponentSchemaRegistry* schema_registry() const {
        return m_schema_registry;
    }

    // =========================================================================
    // Debugging
    // =========================================================================

    /// Format registry state for debugging
    [[nodiscard]] std::string format_state() const;

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// Apply a single component to an entity
    [[nodiscard]] void_core::Result<bool> apply_component(
        void_ecs::World& world,
        void_ecs::Entity entity,
        const std::string& component_name,
        const nlohmann::json& component_data);

    // =========================================================================
    // Data Members
    // =========================================================================

    std::map<std::string, PrefabDefinition> m_prefabs;
    std::map<std::string, ComponentInstantiator> m_instantiators;
    ComponentSchemaRegistry* m_schema_registry = nullptr;
    UnknownComponentPolicy m_unknown_policy = UnknownComponentPolicy::Error;
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename T>
void PrefabRegistry::register_typed_instantiator(const std::string& component_name) {
    register_instantiator(component_name, [](const nlohmann::json& data,
                                              void_ecs::World& world,
                                              void_ecs::Entity entity) -> void_core::Result<void> {
        try {
            T component = data.get<T>();
            if (!world.add_component(entity, std::move(component))) {
                return void_core::Err("Failed to add component " + std::string(typeid(T).name()));
            }
            return void_core::Ok();
        } catch (const nlohmann::json::exception& e) {
            return void_core::Err(std::string("JSON parse error: ") + e.what());
        }
    });
}

} // namespace void_package
