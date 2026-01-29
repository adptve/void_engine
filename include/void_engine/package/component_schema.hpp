#pragma once

/// @file component_schema.hpp
/// @brief Component schema registry for runtime JSON -> component conversion
///
/// The ComponentSchemaRegistry bridges JSON component data (from prefabs and
/// external packages) to ECS component instances. It enables:
///
/// 1. Runtime registration of component types with JSON schemas
/// 2. Validation of component data against schemas
/// 3. Creation of component instances from JSON
/// 4. Dynamic component registration for mods/plugins
///
/// This is CRITICAL for supporting external packages that define components
/// the engine has never seen at compile time.

#include "fwd.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>
#include <void_engine/ecs/component.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <memory>

namespace void_package {

// =============================================================================
// FieldType
// =============================================================================

/// Supported field types for component schemas
enum class FieldType : std::uint8_t {
    Bool,       ///< Boolean
    Int32,      ///< 32-bit signed integer
    Int64,      ///< 64-bit signed integer
    UInt32,     ///< 32-bit unsigned integer
    UInt64,     ///< 64-bit unsigned integer
    Float32,    ///< 32-bit float
    Float64,    ///< 64-bit float
    String,     ///< std::string
    Vec2,       ///< 2D vector [x, y]
    Vec3,       ///< 3D vector [x, y, z]
    Vec4,       ///< 4D vector [x, y, z, w]
    Quat,       ///< Quaternion [x, y, z, w]
    Mat4,       ///< 4x4 matrix
    Entity,     ///< Entity reference
    Array,      ///< Array of another type
    Object,     ///< Nested object
    Any         ///< Arbitrary JSON (stored as nlohmann::json)
};

/// Convert FieldType to string
[[nodiscard]] const char* field_type_to_string(FieldType type) noexcept;

/// Parse FieldType from string (e.g., "f32", "vec3", "string")
[[nodiscard]] bool field_type_from_string(const std::string& str, FieldType& out) noexcept;

/// Get size in bytes for a field type (0 for variable-size types)
[[nodiscard]] std::size_t field_type_size(FieldType type) noexcept;

// =============================================================================
// FieldSchema
// =============================================================================

/// Schema for a single component field
struct FieldSchema {
    std::string name;                            ///< Field name
    FieldType type = FieldType::Any;             ///< Field type
    std::optional<FieldType> array_element_type; ///< For Array type, element type
    std::optional<std::size_t> array_capacity;   ///< For Array type, max capacity
    std::optional<nlohmann::json> default_value; ///< Default value if not specified
    bool required = false;                       ///< Whether field is required
    std::string description;                     ///< Documentation

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<FieldSchema> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Validate a value against this schema
    [[nodiscard]] void_core::Result<void> validate(const nlohmann::json& value) const;
};

// =============================================================================
// ComponentSchema
// =============================================================================

/// Schema for a complete component type
struct ComponentSchema {
    std::string name;                            ///< Component name
    std::vector<FieldSchema> fields;             ///< Field definitions
    std::size_t size = 0;                        ///< Total size in bytes
    std::size_t alignment = 1;                   ///< Required alignment
    std::string source_plugin;                   ///< Plugin that defined this
    bool is_tag = false;                         ///< Tag component (no data)

    /// Parse from JSON (plugin.package component declaration)
    [[nodiscard]] static void_core::Result<ComponentSchema> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Validate component data against schema
    [[nodiscard]] void_core::Result<void> validate(const nlohmann::json& data) const;

    /// Get field by name
    [[nodiscard]] const FieldSchema* get_field(const std::string& field_name) const;

    /// Calculate required size for storage
    void calculate_layout();
};

// =============================================================================
// ComponentFactory
// =============================================================================

/// Function type for creating component bytes from JSON
using ComponentFactory = std::function<void_core::Result<std::vector<std::byte>>(
    const nlohmann::json& data)>;

/// Function type for applying component to entity from JSON
using ComponentApplier = std::function<void_core::Result<void>(
    void_ecs::World& world,
    void_ecs::Entity entity,
    const nlohmann::json& data)>;

// =============================================================================
// ComponentSchemaRegistry
// =============================================================================

/// Registry for component schemas enabling JSON -> component conversion
///
/// This is the bridge between:
/// - Plugin-defined component schemas (JSON declarations)
/// - Asset bundle prefabs (component data as JSON)
/// - ECS world (actual component instances)
class ComponentSchemaRegistry {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    ComponentSchemaRegistry() = default;

    // Non-copyable, movable
    ComponentSchemaRegistry(const ComponentSchemaRegistry&) = delete;
    ComponentSchemaRegistry& operator=(const ComponentSchemaRegistry&) = delete;
    ComponentSchemaRegistry(ComponentSchemaRegistry&&) = default;
    ComponentSchemaRegistry& operator=(ComponentSchemaRegistry&&) = default;

    // =========================================================================
    // Schema Registration
    // =========================================================================

    /// Register a component schema
    ///
    /// @param schema The component schema
    /// @return The assigned ComponentId, or Error
    [[nodiscard]] void_core::Result<void_ecs::ComponentId> register_schema(
        ComponentSchema schema);

    /// Register a schema with custom factory
    ///
    /// @param schema The component schema
    /// @param factory Function to create component bytes from JSON
    /// @param applier Function to apply component to entity from JSON
    [[nodiscard]] void_core::Result<void_ecs::ComponentId> register_schema_with_factory(
        ComponentSchema schema,
        ComponentFactory factory,
        ComponentApplier applier);

    /// Register a typed component with automatic schema generation
    ///
    /// @tparam T The component type
    /// @param name Component name for lookup
    template<typename T>
    void_core::Result<void_ecs::ComponentId> register_typed(const std::string& name);

    /// Unregister a schema by name
    [[nodiscard]] bool unregister_schema(const std::string& name);

    /// Clear all schemas
    void clear();

    // =========================================================================
    // Queries
    // =========================================================================

    /// Get schema by name
    [[nodiscard]] const ComponentSchema* get_schema(const std::string& name) const;

    /// Get ComponentId by name
    [[nodiscard]] std::optional<void_ecs::ComponentId> get_component_id(
        const std::string& name) const;

    /// Check if schema exists
    [[nodiscard]] bool has_schema(const std::string& name) const;

    /// Get all registered schema names
    [[nodiscard]] std::vector<std::string> all_schema_names() const;

    /// Get schemas from a specific plugin
    [[nodiscard]] std::vector<std::string> schemas_from_plugin(
        const std::string& plugin_name) const;

    /// Get number of registered schemas
    [[nodiscard]] std::size_t size() const { return m_schemas.size(); }

    // =========================================================================
    // Instance Creation
    // =========================================================================

    /// Create component bytes from JSON
    ///
    /// @param name Component name
    /// @param data Component data as JSON
    /// @return Component bytes, or Error
    [[nodiscard]] void_core::Result<std::vector<std::byte>> create_instance(
        const std::string& name,
        const nlohmann::json& data) const;

    /// Apply component to entity from JSON
    ///
    /// @param world The ECS world
    /// @param entity The target entity
    /// @param name Component name
    /// @param data Component data as JSON
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> apply_to_entity(
        void_ecs::World& world,
        void_ecs::Entity entity,
        const std::string& name,
        const nlohmann::json& data) const;

    /// Create default instance of component
    [[nodiscard]] void_core::Result<std::vector<std::byte>> create_default(
        const std::string& name) const;

    // =========================================================================
    // Validation
    // =========================================================================

    /// Validate component data against schema
    [[nodiscard]] void_core::Result<void> validate(
        const std::string& name,
        const nlohmann::json& data) const;

    // =========================================================================
    // ECS Integration
    // =========================================================================

    /// Set the ECS component registry for ID allocation
    void set_ecs_registry(void_ecs::ComponentRegistry* registry) {
        m_ecs_registry = registry;
    }

    /// Get the ECS component registry
    [[nodiscard]] void_ecs::ComponentRegistry* ecs_registry() const {
        return m_ecs_registry;
    }

    // =========================================================================
    // Debugging
    // =========================================================================

    /// Format registry state
    [[nodiscard]] std::string format_state() const;

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct RegisteredSchema {
        ComponentSchema schema;
        void_ecs::ComponentId component_id;
        ComponentFactory factory;
        ComponentApplier applier;
    };

    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// Create default factory for a schema
    [[nodiscard]] ComponentFactory create_default_factory(const ComponentSchema& schema);

    /// Create default applier for a schema
    [[nodiscard]] ComponentApplier create_default_applier(
        const ComponentSchema& schema,
        void_ecs::ComponentId comp_id);

    // =========================================================================
    // Data Members
    // =========================================================================

    std::map<std::string, RegisteredSchema> m_schemas;
    std::map<void_ecs::ComponentId, std::string> m_id_to_name;
    void_ecs::ComponentRegistry* m_ecs_registry = nullptr;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// Parse a value from JSON according to field type
[[nodiscard]] void_core::Result<std::vector<std::byte>> parse_field_value(
    const nlohmann::json& value,
    FieldType type);

/// Serialize a field value to JSON
[[nodiscard]] nlohmann::json serialize_field_value(
    const std::byte* data,
    std::size_t size,
    FieldType type);

} // namespace void_package
