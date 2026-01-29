#pragma once

/// @file definition_registry.hpp
/// @brief Generic definition registry for data-driven content
///
/// The DefinitionRegistry stores arbitrary JSON definitions organized by
/// registry type and ID. This enables:
///
/// 1. Plugins defining registry types (weapons, auras, abilities)
/// 2. Asset bundles providing definitions for those registries
/// 3. Systems querying definitions by type and ID at runtime
///
/// Example flow:
/// - Plugin "gameplay.combat" declares it uses "weapons" registry
/// - Asset bundle provides weapon definitions with damage, fire_rate, etc.
/// - CombatSystem queries weapons registry to get weapon stats by ID
///
/// The engine has no compile-time knowledge of what fields definitions contain.

#include "fwd.hpp"
#include <void_engine/core/error.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <functional>

namespace void_package {

// =============================================================================
// CollisionPolicy
// =============================================================================

/// Policy for handling definition ID collisions
enum class CollisionPolicy : std::uint8_t {
    Error,      ///< Fail if definition ID already exists
    FirstWins,  ///< Keep the first definition, ignore subsequent
    LastWins,   ///< Replace with the latest definition (higher layer wins)
    Merge       ///< Attempt to merge definitions (for compatible types)
};

/// Convert collision policy to string
[[nodiscard]] const char* collision_policy_to_string(CollisionPolicy policy) noexcept;

/// Parse collision policy from string
[[nodiscard]] bool collision_policy_from_string(const std::string& str, CollisionPolicy& out) noexcept;

// =============================================================================
// DefinitionSource
// =============================================================================

/// Information about where a definition came from
struct DefinitionSource {
    std::string bundle_name;                     ///< Bundle that provided the definition
    std::string file_path;                       ///< Path to the definition file
    int priority = 0;                            ///< Priority for collision resolution

    /// Compare by priority (higher is better)
    [[nodiscard]] bool operator<(const DefinitionSource& other) const {
        return priority < other.priority;
    }
};

// =============================================================================
// StoredDefinition
// =============================================================================

/// A definition stored in the registry
struct StoredDefinition {
    std::string id;                              ///< Definition ID
    nlohmann::json data;                         ///< The actual definition data
    DefinitionSource source;                     ///< Where it came from
};

// =============================================================================
// RegistryTypeConfig
// =============================================================================

/// Configuration for a registry type
struct RegistryTypeConfig {
    std::string name;                            ///< Registry type name
    CollisionPolicy collision_policy = CollisionPolicy::Error;
    std::optional<std::string> schema_path;      ///< Optional JSON schema for validation
    bool allow_dynamic_fields = true;            ///< Allow fields not in schema

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<RegistryTypeConfig> from_json(const nlohmann::json& j);
};

// =============================================================================
// DefinitionRegistry
// =============================================================================

/// Generic registry for storing definitions by type and ID
///
/// Organized as: registry_type -> id -> definition_data
///
/// Example:
/// ```cpp
/// registry.register_definition("weapons", "plasma_rifle", {
///     {"damage", 45},
///     {"fire_rate", 8},
///     {"mesh", "plasma_rifle_mesh"}
/// });
///
/// auto def = registry.get_definition("weapons", "plasma_rifle");
/// if (def) {
///     int damage = (*def)["damage"].get<int>();
/// }
/// ```
class DefinitionRegistry {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    DefinitionRegistry() = default;

    // Non-copyable, movable
    DefinitionRegistry(const DefinitionRegistry&) = delete;
    DefinitionRegistry& operator=(const DefinitionRegistry&) = delete;
    DefinitionRegistry(DefinitionRegistry&&) = default;
    DefinitionRegistry& operator=(DefinitionRegistry&&) = default;

    // =========================================================================
    // Registry Type Configuration
    // =========================================================================

    /// Configure a registry type
    ///
    /// @param type_name The registry type (e.g., "weapons", "auras")
    /// @param config Configuration for this registry type
    void configure_type(const std::string& type_name, RegistryTypeConfig config);

    /// Set collision policy for a registry type
    void set_collision_policy(const std::string& type_name, CollisionPolicy policy);

    /// Get collision policy for a registry type
    [[nodiscard]] CollisionPolicy get_collision_policy(const std::string& type_name) const;

    /// Get default collision policy
    [[nodiscard]] CollisionPolicy default_collision_policy() const {
        return m_default_policy;
    }

    /// Set default collision policy
    void set_default_collision_policy(CollisionPolicy policy) {
        m_default_policy = policy;
    }

    // =========================================================================
    // Registration
    // =========================================================================

    /// Register a definition
    ///
    /// @param registry_type The registry type (e.g., "weapons")
    /// @param id The definition ID (e.g., "plasma_rifle")
    /// @param data The definition data (arbitrary JSON)
    /// @param source Where this definition came from
    /// @return Ok on success, Error if collision policy forbids
    [[nodiscard]] void_core::Result<void> register_definition(
        const std::string& registry_type,
        const std::string& id,
        nlohmann::json data,
        DefinitionSource source = {});

    /// Register a definition (simplified overload)
    [[nodiscard]] void_core::Result<void> register_definition(
        const std::string& registry_type,
        const std::string& id,
        nlohmann::json data,
        const std::string& bundle_name);

    /// Register multiple definitions for a registry type
    [[nodiscard]] void_core::Result<void> register_definitions(
        const std::string& registry_type,
        const std::vector<std::pair<std::string, nlohmann::json>>& definitions,
        const std::string& bundle_name);

    /// Unregister a definition
    ///
    /// @param registry_type The registry type
    /// @param id The definition ID
    /// @return true if definition was found and removed
    bool unregister_definition(const std::string& registry_type, const std::string& id);

    /// Unregister all definitions from a bundle
    ///
    /// @param bundle_name The bundle name
    /// @return Number of definitions removed
    std::size_t unregister_bundle(const std::string& bundle_name);

    /// Unregister all definitions of a registry type
    ///
    /// @param registry_type The registry type
    /// @return Number of definitions removed
    std::size_t unregister_type(const std::string& registry_type);

    /// Clear all definitions
    void clear();

    // =========================================================================
    // Queries
    // =========================================================================

    /// Get a definition by type and ID
    ///
    /// @param registry_type The registry type
    /// @param id The definition ID
    /// @return The definition data, or nullopt if not found
    [[nodiscard]] std::optional<nlohmann::json> get_definition(
        const std::string& registry_type,
        const std::string& id) const;

    /// Get a definition with source info
    [[nodiscard]] const StoredDefinition* get_definition_full(
        const std::string& registry_type,
        const std::string& id) const;

    /// Check if a definition exists
    [[nodiscard]] bool has_definition(
        const std::string& registry_type,
        const std::string& id) const;

    /// List all definition IDs for a registry type
    [[nodiscard]] std::vector<std::string> list_definitions(
        const std::string& registry_type) const;

    /// List all registry types
    [[nodiscard]] std::vector<std::string> list_registry_types() const;

    /// Get all definitions for a registry type
    [[nodiscard]] std::vector<StoredDefinition> all_definitions(
        const std::string& registry_type) const;

    /// Get definition count for a registry type
    [[nodiscard]] std::size_t definition_count(const std::string& registry_type) const;

    /// Get total definition count across all types
    [[nodiscard]] std::size_t total_definition_count() const;

    /// Check if a registry type exists (has any definitions)
    [[nodiscard]] bool has_registry_type(const std::string& registry_type) const;

    // =========================================================================
    // Typed Access
    // =========================================================================

    /// Get a definition field with type conversion
    ///
    /// @tparam T The type to convert to
    /// @param registry_type The registry type
    /// @param id The definition ID
    /// @param field The field name
    /// @return The field value, or nullopt if not found or wrong type
    template<typename T>
    [[nodiscard]] std::optional<T> get_field(
        const std::string& registry_type,
        const std::string& id,
        const std::string& field) const;

    /// Get a definition field with default value
    template<typename T>
    [[nodiscard]] T get_field_or(
        const std::string& registry_type,
        const std::string& id,
        const std::string& field,
        T default_value) const;

    // =========================================================================
    // Iteration
    // =========================================================================

    /// Iterate over all definitions of a type
    ///
    /// @param registry_type The registry type
    /// @param callback Called for each definition (id, data)
    void for_each(
        const std::string& registry_type,
        const std::function<void(const std::string& id, const nlohmann::json& data)>& callback) const;

    /// Iterate over all definitions of all types
    void for_each_all(
        const std::function<void(const std::string& type, const std::string& id,
                                  const nlohmann::json& data)>& callback) const;

    // =========================================================================
    // Debugging
    // =========================================================================

    /// Format registry state for debugging
    [[nodiscard]] std::string format_state() const;

    /// Get statistics
    struct Stats {
        std::size_t total_definitions = 0;
        std::size_t registry_types = 0;
        std::size_t bundles = 0;
        std::map<std::string, std::size_t> definitions_per_type;
    };
    [[nodiscard]] Stats get_stats() const;

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct RegistryData {
        std::map<std::string, StoredDefinition> definitions;
        RegistryTypeConfig config;
    };

    // =========================================================================
    // Data Members
    // =========================================================================

    std::map<std::string, RegistryData> m_registries;
    CollisionPolicy m_default_policy = CollisionPolicy::Error;
    std::set<std::string> m_known_bundles;
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename T>
std::optional<T> DefinitionRegistry::get_field(
    const std::string& registry_type,
    const std::string& id,
    const std::string& field) const
{
    auto def = get_definition(registry_type, id);
    if (!def) {
        return std::nullopt;
    }

    auto it = def->find(field);
    if (it == def->end()) {
        return std::nullopt;
    }

    try {
        return it->get<T>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

template<typename T>
T DefinitionRegistry::get_field_or(
    const std::string& registry_type,
    const std::string& id,
    const std::string& field,
    T default_value) const
{
    auto result = get_field<T>(registry_type, id, field);
    return result.value_or(std::move(default_value));
}

} // namespace void_package
