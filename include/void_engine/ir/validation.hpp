#pragma once

/// @file validation.hpp
/// @brief Schema-based validation for void_ir

#include "fwd.hpp"
#include "value.hpp"
#include "patch.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

namespace void_ir {

// =============================================================================
// FieldType
// =============================================================================

/// Field type discriminator for schema
enum class FieldType : std::uint8_t {
    Bool = 0,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Mat4,
    Array,
    Object,
    EntityRef,
    AssetRef,
    Enum,       // String from allowed set
    Any         // No type checking
};

/// Get field type name
[[nodiscard]] inline const char* field_type_name(FieldType type) noexcept {
    switch (type) {
        case FieldType::Bool: return "Bool";
        case FieldType::Int: return "Int";
        case FieldType::Float: return "Float";
        case FieldType::String: return "String";
        case FieldType::Vec2: return "Vec2";
        case FieldType::Vec3: return "Vec3";
        case FieldType::Vec4: return "Vec4";
        case FieldType::Mat4: return "Mat4";
        case FieldType::Array: return "Array";
        case FieldType::Object: return "Object";
        case FieldType::EntityRef: return "EntityRef";
        case FieldType::AssetRef: return "AssetRef";
        case FieldType::Enum: return "Enum";
        case FieldType::Any: return "Any";
        default: return "Unknown";
    }
}

/// Check if value type matches field type
[[nodiscard]] inline bool value_matches_field_type(const Value& value, FieldType field_type) {
    switch (field_type) {
        case FieldType::Bool: return value.is_bool();
        case FieldType::Int: return value.is_int();
        case FieldType::Float: return value.is_float() || value.is_int();
        case FieldType::String: return value.is_string();
        case FieldType::Vec2: return value.is_vec2();
        case FieldType::Vec3: return value.is_vec3();
        case FieldType::Vec4: return value.is_vec4();
        case FieldType::Mat4: return value.is_mat4();
        case FieldType::Array: return value.is_array();
        case FieldType::Object: return value.is_object();
        case FieldType::EntityRef: return value.is_entity_ref();
        case FieldType::AssetRef: return value.is_asset_ref();
        case FieldType::Enum: return value.is_string();
        case FieldType::Any: return true;
        default: return false;
    }
}

// =============================================================================
// FieldConstraint
// =============================================================================

/// Numeric range constraint
struct NumericRange {
    std::optional<double> min;
    std::optional<double> max;

    [[nodiscard]] bool check(double value) const {
        if (min && value < *min) return false;
        if (max && value > *max) return false;
        return true;
    }
};

/// String constraint
struct StringConstraint {
    std::optional<std::size_t> min_length;
    std::optional<std::size_t> max_length;
    std::optional<std::string> pattern;  // Regex pattern (optional, requires <regex>)

    [[nodiscard]] bool check(const std::string& value) const {
        if (min_length && value.length() < *min_length) return false;
        if (max_length && value.length() > *max_length) return false;
        return true;
    }
};

/// Array constraint
struct ArrayConstraint {
    std::optional<std::size_t> min_length;
    std::optional<std::size_t> max_length;
    std::optional<FieldType> element_type;

    [[nodiscard]] bool check_length(std::size_t length) const {
        if (min_length && length < *min_length) return false;
        if (max_length && length > *max_length) return false;
        return true;
    }
};

// =============================================================================
// FieldDescriptor
// =============================================================================

/// Describes a field in a component schema
struct FieldDescriptor {
    std::string name;
    FieldType type = FieldType::Any;
    bool required = true;
    bool nullable = false;
    Value default_value;

    // Constraints
    std::optional<NumericRange> numeric_range;
    std::optional<StringConstraint> string_constraint;
    std::optional<ArrayConstraint> array_constraint;
    std::vector<std::string> enum_values;  // For Enum type

    /// Create bool field
    [[nodiscard]] static FieldDescriptor boolean(std::string name, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::Bool;
        f.required = required;
        return f;
    }

    /// Create int field
    [[nodiscard]] static FieldDescriptor integer(std::string name, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::Int;
        f.required = required;
        return f;
    }

    /// Create int field with range
    [[nodiscard]] static FieldDescriptor integer_range(
        std::string name, std::int64_t min, std::int64_t max, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::Int;
        f.required = required;
        f.numeric_range = NumericRange{
            static_cast<double>(min),
            static_cast<double>(max)
        };
        return f;
    }

    /// Create float field
    [[nodiscard]] static FieldDescriptor floating(std::string name, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::Float;
        f.required = required;
        return f;
    }

    /// Create float field with range
    [[nodiscard]] static FieldDescriptor float_range(
        std::string name, double min, double max, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::Float;
        f.required = required;
        f.numeric_range = NumericRange{min, max};
        return f;
    }

    /// Create string field
    [[nodiscard]] static FieldDescriptor string(std::string name, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::String;
        f.required = required;
        return f;
    }

    /// Create Vec3 field
    [[nodiscard]] static FieldDescriptor vec3(std::string name, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::Vec3;
        f.required = required;
        return f;
    }

    /// Create Vec4 field
    [[nodiscard]] static FieldDescriptor vec4(std::string name, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::Vec4;
        f.required = required;
        return f;
    }

    /// Create enum field
    [[nodiscard]] static FieldDescriptor enumeration(
        std::string name, std::vector<std::string> values, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::Enum;
        f.required = required;
        f.enum_values = std::move(values);
        return f;
    }

    /// Create entity ref field
    [[nodiscard]] static FieldDescriptor entity_ref(std::string name, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::EntityRef;
        f.required = required;
        return f;
    }

    /// Create asset ref field
    [[nodiscard]] static FieldDescriptor asset_ref(std::string name, bool required = true) {
        FieldDescriptor f;
        f.name = std::move(name);
        f.type = FieldType::AssetRef;
        f.required = required;
        return f;
    }

    /// Set as optional with default
    FieldDescriptor& with_default(Value v) {
        required = false;
        default_value = std::move(v);
        return *this;
    }

    /// Set as nullable
    FieldDescriptor& make_nullable() {
        nullable = true;
        return *this;
    }
};

// =============================================================================
// ValidationError
// =============================================================================

/// Validation error detail
struct ValidationError {
    std::string field_path;
    std::string message;
    std::optional<Value> actual_value;
    std::optional<Value> expected_value;

    [[nodiscard]] std::string to_string() const {
        if (field_path.empty()) {
            return message;
        }
        return field_path + ": " + message;
    }
};

// =============================================================================
// ValidationResult
// =============================================================================

/// Result of validation
struct ValidationResult {
    bool valid = true;
    std::vector<ValidationError> errors;

    [[nodiscard]] static ValidationResult ok() {
        return ValidationResult{true, {}};
    }

    [[nodiscard]] static ValidationResult failed(std::string message) {
        ValidationResult r;
        r.valid = false;
        r.errors.push_back(ValidationError{"", std::move(message), std::nullopt, std::nullopt});
        return r;
    }

    [[nodiscard]] static ValidationResult field_error(
        std::string path, std::string message) {
        ValidationResult r;
        r.valid = false;
        r.errors.push_back(ValidationError{std::move(path), std::move(message),
                                           std::nullopt, std::nullopt});
        return r;
    }

    /// Merge another result
    void merge(const ValidationResult& other) {
        if (!other.valid) {
            valid = false;
        }
        errors.insert(errors.end(), other.errors.begin(), other.errors.end());
    }

    /// Add error
    void add_error(std::string path, std::string message) {
        valid = false;
        errors.push_back(ValidationError{std::move(path), std::move(message),
                                         std::nullopt, std::nullopt});
    }

    /// Get first error message
    [[nodiscard]] std::string first_error() const {
        if (errors.empty()) {
            return "";
        }
        return errors[0].to_string();
    }

    /// Get all error messages
    [[nodiscard]] std::vector<std::string> all_errors() const {
        std::vector<std::string> msgs;
        msgs.reserve(errors.size());
        for (const auto& e : errors) {
            msgs.push_back(e.to_string());
        }
        return msgs;
    }
};

// =============================================================================
// ComponentSchema
// =============================================================================

/// Schema for a component type
class ComponentSchema {
public:
    /// Construct with component type name
    explicit ComponentSchema(std::string type_name)
        : m_type_name(std::move(type_name)) {}

    /// Get type name
    [[nodiscard]] const std::string& type_name() const noexcept {
        return m_type_name;
    }

    /// Add field
    ComponentSchema& field(FieldDescriptor descriptor) {
        m_fields.push_back(std::move(descriptor));
        return *this;
    }

    /// Get fields
    [[nodiscard]] const std::vector<FieldDescriptor>& fields() const noexcept {
        return m_fields;
    }

    /// Find field by name
    [[nodiscard]] const FieldDescriptor* find_field(std::string_view name) const {
        for (const auto& f : m_fields) {
            if (f.name == name) {
                return &f;
            }
        }
        return nullptr;
    }

    /// Validate a value against this schema
    [[nodiscard]] ValidationResult validate(const Value& value) const {
        if (!value.is_object()) {
            return ValidationResult::failed("Expected object value for component");
        }

        ValidationResult result = ValidationResult::ok();
        const auto& obj = value.as_object();

        // Check required fields
        for (const auto& field : m_fields) {
            auto it = obj.find(field.name);

            if (it == obj.end()) {
                if (field.required) {
                    result.add_error(field.name, "Required field missing");
                }
                continue;
            }

            // Validate field value
            auto field_result = validate_field(field, it->second, field.name);
            result.merge(field_result);
        }

        // Check for unknown fields (optional, could be warning)
        for (const auto& [key, val] : obj) {
            if (!find_field(key)) {
                // Unknown field - could add warning
            }
        }

        return result;
    }

    /// Validate a single field
    [[nodiscard]] ValidationResult validate_field(
        const FieldDescriptor& field, const Value& value, const std::string& path) const {

        // Check null
        if (value.is_null()) {
            if (!field.nullable) {
                return ValidationResult::field_error(path, "Field cannot be null");
            }
            return ValidationResult::ok();
        }

        // Check type
        if (!value_matches_field_type(value, field.type)) {
            return ValidationResult::field_error(
                path,
                "Type mismatch: expected " + std::string(field_type_name(field.type))
            );
        }

        // Check constraints
        ValidationResult result = ValidationResult::ok();

        if (field.numeric_range && value.is_numeric()) {
            double num = value.as_numeric();
            if (!field.numeric_range->check(num)) {
                result.add_error(path, "Value out of range");
            }
        }

        if (field.string_constraint && value.is_string()) {
            if (!field.string_constraint->check(value.as_string())) {
                result.add_error(path, "String constraint violated");
            }
        }

        if (field.array_constraint && value.is_array()) {
            const auto& arr = value.as_array();
            if (!field.array_constraint->check_length(arr.size())) {
                result.add_error(path, "Array length constraint violated");
            }

            if (field.array_constraint->element_type) {
                for (std::size_t i = 0; i < arr.size(); ++i) {
                    if (!value_matches_field_type(arr[i], *field.array_constraint->element_type)) {
                        result.add_error(
                            path + "[" + std::to_string(i) + "]",
                            "Element type mismatch"
                        );
                    }
                }
            }
        }

        // Check enum values
        if (field.type == FieldType::Enum && value.is_string()) {
            const auto& str = value.as_string();
            bool found = false;
            for (const auto& allowed : field.enum_values) {
                if (allowed == str) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                result.add_error(path, "Invalid enum value: " + str);
            }
        }

        return result;
    }

private:
    std::string m_type_name;
    std::vector<FieldDescriptor> m_fields;
};

// =============================================================================
// SchemaRegistry
// =============================================================================

/// Registry of component schemas
class SchemaRegistry {
public:
    /// Register a schema
    void register_schema(ComponentSchema schema) {
        m_schemas.emplace(schema.type_name(), std::move(schema));
    }

    /// Get schema by type name
    [[nodiscard]] const ComponentSchema* get(std::string_view type_name) const {
        auto it = m_schemas.find(std::string(type_name));
        if (it == m_schemas.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Check if schema exists
    [[nodiscard]] bool has(std::string_view type_name) const {
        return m_schemas.find(std::string(type_name)) != m_schemas.end();
    }

    /// Validate a component patch
    [[nodiscard]] ValidationResult validate_patch(const ComponentPatch& patch) const {
        const ComponentSchema* schema = get(patch.component_type);

        if (!schema) {
            // No schema = no validation
            return ValidationResult::ok();
        }

        if (patch.operation == ComponentOp::Remove) {
            return ValidationResult::ok();
        }

        if (patch.operation == ComponentOp::SetField) {
            const FieldDescriptor* field = schema->find_field(patch.field_path);
            if (!field) {
                return ValidationResult::field_error(patch.field_path, "Unknown field");
            }
            return schema->validate_field(*field, patch.value, patch.field_path);
        }

        // Add or Set - validate entire value
        return schema->validate(patch.value);
    }

    /// Get all registered type names
    [[nodiscard]] std::vector<std::string> type_names() const {
        std::vector<std::string> names;
        names.reserve(m_schemas.size());
        for (const auto& [name, _] : m_schemas) {
            names.push_back(name);
        }
        return names;
    }

    /// Get schema count
    [[nodiscard]] std::size_t size() const noexcept {
        return m_schemas.size();
    }

    /// Clear all schemas
    void clear() {
        m_schemas.clear();
    }

private:
    std::unordered_map<std::string, ComponentSchema> m_schemas;
};

// =============================================================================
// PatchValidator
// =============================================================================

/// Validates patches against schemas and permissions
class PatchValidator {
public:
    /// Construct with schema registry
    explicit PatchValidator(const SchemaRegistry& schemas)
        : m_schemas(schemas) {}

    /// Validate a single patch
    [[nodiscard]] ValidationResult validate(
        const Patch& patch, const NamespacePermissions& permissions) const {

        ValidationResult result = ValidationResult::ok();

        // Check permissions based on patch type
        patch.visit([&](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, EntityPatch>) {
                if (p.operation == EntityOp::Create && !permissions.can_create_entities) {
                    result.add_error("", "Permission denied: cannot create entities");
                }
                if (p.operation == EntityOp::Delete && !permissions.can_delete_entities) {
                    result.add_error("", "Permission denied: cannot delete entities");
                }
            }
            else if constexpr (std::is_same_v<T, ComponentPatch>) {
                if (!permissions.can_modify_components) {
                    result.add_error("", "Permission denied: cannot modify components");
                }
                if (!permissions.is_component_allowed(p.component_type)) {
                    result.add_error("", "Permission denied: component type not allowed");
                }

                // Validate against schema
                auto schema_result = m_schemas.validate_patch(p);
                result.merge(schema_result);
            }
            else if constexpr (std::is_same_v<T, LayerPatch>) {
                if (!permissions.can_modify_layers) {
                    result.add_error("", "Permission denied: cannot modify layers");
                }
            }
            else if constexpr (std::is_same_v<T, HierarchyPatch>) {
                if (!permissions.can_modify_hierarchy) {
                    result.add_error("", "Permission denied: cannot modify hierarchy");
                }
            }
        });

        return result;
    }

    /// Validate a batch of patches
    [[nodiscard]] ValidationResult validate_batch(
        const PatchBatch& batch, const NamespacePermissions& permissions) const {

        ValidationResult result = ValidationResult::ok();

        for (std::size_t i = 0; i < batch.size(); ++i) {
            auto patch_result = validate(batch.patches()[i], permissions);
            if (!patch_result.valid) {
                for (auto& error : patch_result.errors) {
                    error.field_path = "[" + std::to_string(i) + "]." + error.field_path;
                }
            }
            result.merge(patch_result);
        }

        return result;
    }

private:
    const SchemaRegistry& m_schemas;
};

} // namespace void_ir
