/// @file component_schema.cpp
/// @brief Implementation of component schema registry for JSON -> component conversion

#include <void_engine/package/component_schema.hpp>
#include <void_engine/ecs/world.hpp>

#include <sstream>
#include <cstring>

namespace void_package {

// =============================================================================
// FieldType Utilities
// =============================================================================

const char* field_type_to_string(FieldType type) noexcept {
    switch (type) {
        case FieldType::Bool: return "bool";
        case FieldType::Int32: return "i32";
        case FieldType::Int64: return "i64";
        case FieldType::UInt32: return "u32";
        case FieldType::UInt64: return "u64";
        case FieldType::Float32: return "f32";
        case FieldType::Float64: return "f64";
        case FieldType::String: return "string";
        case FieldType::Vec2: return "vec2";
        case FieldType::Vec3: return "vec3";
        case FieldType::Vec4: return "vec4";
        case FieldType::Quat: return "quat";
        case FieldType::Mat4: return "mat4";
        case FieldType::Entity: return "Entity";
        case FieldType::Array: return "array";
        case FieldType::Object: return "object";
        case FieldType::Any: return "any";
        default: return "unknown";
    }
}

bool field_type_from_string(const std::string& str, FieldType& out) noexcept {
    if (str == "bool") { out = FieldType::Bool; return true; }
    if (str == "i32" || str == "int" || str == "int32") { out = FieldType::Int32; return true; }
    if (str == "i64" || str == "int64") { out = FieldType::Int64; return true; }
    if (str == "u32" || str == "uint" || str == "uint32") { out = FieldType::UInt32; return true; }
    if (str == "u64" || str == "uint64") { out = FieldType::UInt64; return true; }
    if (str == "f32" || str == "float" || str == "float32") { out = FieldType::Float32; return true; }
    if (str == "f64" || str == "double" || str == "float64") { out = FieldType::Float64; return true; }
    if (str == "string" || str == "String") { out = FieldType::String; return true; }
    if (str == "vec2" || str == "Vec2") { out = FieldType::Vec2; return true; }
    if (str == "vec3" || str == "Vec3") { out = FieldType::Vec3; return true; }
    if (str == "vec4" || str == "Vec4") { out = FieldType::Vec4; return true; }
    if (str == "quat" || str == "Quat" || str == "quaternion") { out = FieldType::Quat; return true; }
    if (str == "mat4" || str == "Mat4" || str == "matrix4") { out = FieldType::Mat4; return true; }
    if (str == "Entity" || str == "entity") { out = FieldType::Entity; return true; }
    if (str == "array") { out = FieldType::Array; return true; }
    if (str == "object") { out = FieldType::Object; return true; }
    if (str == "any" || str == "json") { out = FieldType::Any; return true; }

    // Handle array<T> syntax
    if (str.size() > 6 && str.substr(0, 6) == "array<") {
        out = FieldType::Array;
        return true;
    }

    return false;
}

std::size_t field_type_size(FieldType type) noexcept {
    switch (type) {
        case FieldType::Bool: return sizeof(bool);
        case FieldType::Int32: return sizeof(std::int32_t);
        case FieldType::Int64: return sizeof(std::int64_t);
        case FieldType::UInt32: return sizeof(std::uint32_t);
        case FieldType::UInt64: return sizeof(std::uint64_t);
        case FieldType::Float32: return sizeof(float);
        case FieldType::Float64: return sizeof(double);
        case FieldType::Vec2: return sizeof(float) * 2;
        case FieldType::Vec3: return sizeof(float) * 3;
        case FieldType::Vec4: return sizeof(float) * 4;
        case FieldType::Quat: return sizeof(float) * 4;
        case FieldType::Mat4: return sizeof(float) * 16;
        case FieldType::Entity: return sizeof(std::uint64_t);  // Entity storage
        // Variable-size types
        case FieldType::String: return 0;
        case FieldType::Array: return 0;
        case FieldType::Object: return 0;
        case FieldType::Any: return 0;
        default: return 0;
    }
}

// =============================================================================
// FieldSchema
// =============================================================================

void_core::Result<FieldSchema> FieldSchema::from_json(const nlohmann::json& j) {
    FieldSchema schema;

    // Name can be at the top level or we might be parsing inline
    if (j.contains("name") && j["name"].is_string()) {
        schema.name = j["name"].get<std::string>();
    }

    // Type parsing
    if (j.contains("type") && j["type"].is_string()) {
        std::string type_str = j["type"].get<std::string>();

        // Parse array<T> syntax
        if (type_str.size() > 6 && type_str.substr(0, 6) == "array<") {
            schema.type = FieldType::Array;
            // Extract element type
            std::size_t end = type_str.find('>');
            if (end != std::string::npos) {
                std::string elem_type_str = type_str.substr(6, end - 6);
                FieldType elem_type;
                if (field_type_from_string(elem_type_str, elem_type)) {
                    schema.array_element_type = elem_type;
                }
            }
        } else {
            if (!field_type_from_string(type_str, schema.type)) {
                return void_core::Err<FieldSchema>("Unknown field type: " + type_str);
            }
        }
    }

    // Capacity for arrays
    if (j.contains("capacity") && j["capacity"].is_number_unsigned()) {
        schema.array_capacity = j["capacity"].get<std::size_t>();
    }

    // Default value
    if (j.contains("default")) {
        schema.default_value = j["default"];
    }

    // Required flag
    if (j.contains("required") && j["required"].is_boolean()) {
        schema.required = j["required"].get<bool>();
    }

    // Description
    if (j.contains("description") && j["description"].is_string()) {
        schema.description = j["description"].get<std::string>();
    }

    return void_core::Ok(std::move(schema));
}

nlohmann::json FieldSchema::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = field_type_to_string(type);

    if (type == FieldType::Array && array_element_type) {
        j["type"] = std::string("array<") + field_type_to_string(*array_element_type) + ">";
    }

    if (array_capacity) {
        j["capacity"] = *array_capacity;
    }

    if (default_value) {
        j["default"] = *default_value;
    }

    if (required) {
        j["required"] = required;
    }

    if (!description.empty()) {
        j["description"] = description;
    }

    return j;
}

void_core::Result<void> FieldSchema::validate(const nlohmann::json& value) const {
    switch (type) {
        case FieldType::Bool:
            if (!value.is_boolean()) {
                return void_core::Err("Field '" + name + "': expected bool");
            }
            break;

        case FieldType::Int32:
        case FieldType::Int64:
        case FieldType::UInt32:
        case FieldType::UInt64:
            if (!value.is_number_integer()) {
                return void_core::Err("Field '" + name + "': expected integer");
            }
            break;

        case FieldType::Float32:
        case FieldType::Float64:
            if (!value.is_number()) {
                return void_core::Err("Field '" + name + "': expected number");
            }
            break;

        case FieldType::String:
            if (!value.is_string()) {
                return void_core::Err("Field '" + name + "': expected string");
            }
            break;

        case FieldType::Vec2:
            if (!value.is_array() || value.size() < 2) {
                return void_core::Err("Field '" + name + "': expected array of 2 numbers");
            }
            break;

        case FieldType::Vec3:
            if (!value.is_array() || value.size() < 3) {
                return void_core::Err("Field '" + name + "': expected array of 3 numbers");
            }
            break;

        case FieldType::Vec4:
        case FieldType::Quat:
            if (!value.is_array() || value.size() < 4) {
                return void_core::Err("Field '" + name + "': expected array of 4 numbers");
            }
            break;

        case FieldType::Mat4:
            if (!value.is_array() || value.size() < 16) {
                return void_core::Err("Field '" + name + "': expected array of 16 numbers");
            }
            break;

        case FieldType::Array:
            if (!value.is_array()) {
                return void_core::Err("Field '" + name + "': expected array");
            }
            break;

        case FieldType::Object:
            if (!value.is_object()) {
                return void_core::Err("Field '" + name + "': expected object");
            }
            break;

        case FieldType::Entity:
        case FieldType::Any:
            // Accept anything
            break;
    }

    return void_core::Ok();
}

// =============================================================================
// ComponentSchema
// =============================================================================

void_core::Result<ComponentSchema> ComponentSchema::from_json(const nlohmann::json& j) {
    ComponentSchema schema;

    if (!j.contains("name") || !j["name"].is_string()) {
        return void_core::Err<ComponentSchema>("ComponentSchema: missing 'name' field");
    }
    schema.name = j["name"].get<std::string>();

    // Parse fields
    if (j.contains("fields") && j["fields"].is_object()) {
        for (auto& [field_name, field_def] : j["fields"].items()) {
            auto result = FieldSchema::from_json(field_def);
            if (!result) {
                return void_core::Err<ComponentSchema>("ComponentSchema '" + schema.name + "', field '" +
                                       field_name + "': " + result.error().message());
            }
            result->name = field_name;
            schema.fields.push_back(std::move(*result));
        }
    }

    // Check if this is a tag (no fields)
    if (j.contains("is_tag") && j["is_tag"].is_boolean()) {
        schema.is_tag = j["is_tag"].get<bool>();
    } else {
        schema.is_tag = schema.fields.empty();
    }

    // Calculate layout
    schema.calculate_layout();

    return void_core::Ok(std::move(schema));
}

nlohmann::json ComponentSchema::to_json() const {
    nlohmann::json j;
    j["name"] = name;

    if (!fields.empty()) {
        j["fields"] = nlohmann::json::object();
        for (const auto& field : fields) {
            j["fields"][field.name] = field.to_json();
        }
    }

    if (is_tag) {
        j["is_tag"] = true;
    }

    return j;
}

void_core::Result<void> ComponentSchema::validate(const nlohmann::json& data) const {
    if (is_tag) {
        // Tags shouldn't have data
        return void_core::Ok();
    }

    if (!data.is_object()) {
        return void_core::Err("Component data must be an object");
    }

    // Check required fields
    for (const auto& field : fields) {
        if (field.required && !data.contains(field.name)) {
            return void_core::Err("Missing required field: " + field.name);
        }

        if (data.contains(field.name)) {
            auto result = field.validate(data[field.name]);
            if (!result) {
                return result;
            }
        }
    }

    return void_core::Ok();
}

const FieldSchema* ComponentSchema::get_field(const std::string& field_name) const {
    for (const auto& field : fields) {
        if (field.name == field_name) {
            return &field;
        }
    }
    return nullptr;
}

void ComponentSchema::calculate_layout() {
    if (is_tag) {
        size = 0;
        alignment = 1;
        return;
    }

    // Simple layout calculation - fixed-size fields only for now
    std::size_t offset = 0;
    std::size_t max_align = 1;

    for (const auto& field : fields) {
        std::size_t field_size = field_type_size(field.type);
        std::size_t field_align = field_size > 0 ? field_size : sizeof(void*);

        if (field_align > max_align) {
            max_align = field_align;
        }

        // Align offset
        offset = (offset + field_align - 1) & ~(field_align - 1);
        offset += field_size;
    }

    // Final alignment
    size = (offset + max_align - 1) & ~(max_align - 1);
    alignment = max_align;

    // If we have variable-size fields, mark size as 0 (runtime-determined)
    for (const auto& field : fields) {
        if (field_type_size(field.type) == 0) {
            size = 0;
            break;
        }
    }
}

// =============================================================================
// ComponentSchemaRegistry
// =============================================================================

void_core::Result<void_ecs::ComponentId> ComponentSchemaRegistry::register_schema(
    ComponentSchema schema)
{
    // Create default factory and applier
    auto factory = create_default_factory(schema);

    // We need an ECS registry to allocate component IDs
    if (!m_ecs_registry) {
        return void_core::Err<void_ecs::ComponentId>("ComponentSchemaRegistry: ECS registry not set");
    }

    // Register with ECS as a dynamic component
    void_ecs::ComponentInfo info;
    info.name = schema.name;
    info.size = schema.size;
    info.align = schema.alignment;
    info.type_id = std::type_index(typeid(void));  // Dynamic component

    // For now, just use memcpy for move/drop since we don't have RAII fields
    info.move_fn = []([[maybe_unused]] void* src, [[maybe_unused]] void* dst) {
        // Will be set based on size
    };

    void_ecs::ComponentId comp_id = m_ecs_registry->register_dynamic(std::move(info));

    auto applier = create_default_applier(schema, comp_id);

    // Store registration
    RegisteredSchema reg;
    reg.schema = std::move(schema);
    reg.component_id = comp_id;
    reg.factory = std::move(factory);
    reg.applier = std::move(applier);

    std::string name = reg.schema.name;
    m_schemas[name] = std::move(reg);
    m_id_to_name[comp_id] = name;

    return void_core::Ok(comp_id);
}

void_core::Result<void_ecs::ComponentId> ComponentSchemaRegistry::register_schema_with_factory(
    ComponentSchema schema,
    ComponentFactory factory,
    ComponentApplier applier)
{
    if (!m_ecs_registry) {
        return void_core::Err<void_ecs::ComponentId>("ComponentSchemaRegistry: ECS registry not set");
    }

    void_ecs::ComponentInfo info;
    info.name = schema.name;
    info.size = schema.size;
    info.align = schema.alignment;
    info.type_id = std::type_index(typeid(void));

    void_ecs::ComponentId comp_id = m_ecs_registry->register_dynamic(std::move(info));

    RegisteredSchema reg;
    reg.schema = std::move(schema);
    reg.component_id = comp_id;
    reg.factory = std::move(factory);
    reg.applier = std::move(applier);

    std::string name = reg.schema.name;
    m_schemas[name] = std::move(reg);
    m_id_to_name[comp_id] = name;

    return void_core::Ok(comp_id);
}

bool ComponentSchemaRegistry::unregister_schema(const std::string& name) {
    auto it = m_schemas.find(name);
    if (it == m_schemas.end()) {
        return false;
    }

    m_id_to_name.erase(it->second.component_id);
    m_schemas.erase(it);
    return true;
}

void ComponentSchemaRegistry::clear() {
    m_schemas.clear();
    m_id_to_name.clear();
}

const ComponentSchema* ComponentSchemaRegistry::get_schema(const std::string& name) const {
    auto it = m_schemas.find(name);
    return it != m_schemas.end() ? &it->second.schema : nullptr;
}

std::optional<void_ecs::ComponentId> ComponentSchemaRegistry::get_component_id(
    const std::string& name) const
{
    auto it = m_schemas.find(name);
    return it != m_schemas.end() ? std::optional{it->second.component_id} : std::nullopt;
}

bool ComponentSchemaRegistry::has_schema(const std::string& name) const {
    return m_schemas.count(name) > 0;
}

std::vector<std::string> ComponentSchemaRegistry::all_schema_names() const {
    std::vector<std::string> names;
    names.reserve(m_schemas.size());
    for (const auto& [name, _] : m_schemas) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> ComponentSchemaRegistry::schemas_from_plugin(
    const std::string& plugin_name) const
{
    std::vector<std::string> names;
    for (const auto& [name, reg] : m_schemas) {
        if (reg.schema.source_plugin == plugin_name) {
            names.push_back(name);
        }
    }
    return names;
}

void_core::Result<std::vector<std::byte>> ComponentSchemaRegistry::create_instance(
    const std::string& name,
    const nlohmann::json& data) const
{
    auto it = m_schemas.find(name);
    if (it == m_schemas.end()) {
        return void_core::Err<std::vector<std::byte>>("Unknown component schema: " + name);
    }

    // Validate data against schema
    auto valid = it->second.schema.validate(data);
    if (!valid) {
        return void_core::Err<std::vector<std::byte>>(valid.error());
    }

    // Use factory to create bytes
    return it->second.factory(data);
}

void_core::Result<void> ComponentSchemaRegistry::apply_to_entity(
    void_ecs::World& world,
    void_ecs::Entity entity,
    const std::string& name,
    const nlohmann::json& data) const
{
    auto it = m_schemas.find(name);
    if (it == m_schemas.end()) {
        return void_core::Err("Unknown component schema: " + name);
    }

    // Validate data
    auto valid = it->second.schema.validate(data);
    if (!valid) {
        return void_core::Err(valid.error());
    }

    // Use applier
    return it->second.applier(world, entity, data);
}

void_core::Result<std::vector<std::byte>> ComponentSchemaRegistry::create_default(
    const std::string& name) const
{
    auto it = m_schemas.find(name);
    if (it == m_schemas.end()) {
        return void_core::Err<std::vector<std::byte>>("Unknown component schema: " + name);
    }

    // Build default JSON from schema defaults
    nlohmann::json defaults = nlohmann::json::object();
    for (const auto& field : it->second.schema.fields) {
        if (field.default_value) {
            defaults[field.name] = *field.default_value;
        }
    }

    return it->second.factory(defaults);
}

void_core::Result<void> ComponentSchemaRegistry::validate(
    const std::string& name,
    const nlohmann::json& data) const
{
    auto it = m_schemas.find(name);
    if (it == m_schemas.end()) {
        return void_core::Err("Unknown component schema: " + name);
    }
    return it->second.schema.validate(data);
}

std::string ComponentSchemaRegistry::format_state() const {
    std::ostringstream ss;
    ss << "ComponentSchemaRegistry:\n";
    ss << "  Schemas: " << m_schemas.size() << "\n";
    ss << "  ECS registry: " << (m_ecs_registry ? "set" : "not set") << "\n";

    if (!m_schemas.empty()) {
        ss << "\n  Registered schemas:\n";
        for (const auto& [name, reg] : m_schemas) {
            ss << "    - " << name;
            if (reg.schema.is_tag) {
                ss << " (tag)";
            } else {
                ss << " (" << reg.schema.fields.size() << " fields, "
                   << reg.schema.size << " bytes)";
            }
            ss << "\n";
        }
    }

    return ss.str();
}

ComponentFactory ComponentSchemaRegistry::create_default_factory(const ComponentSchema& schema) {
    // For tag components
    if (schema.is_tag || schema.size == 0) {
        return [](const nlohmann::json&) -> void_core::Result<std::vector<std::byte>> {
            return void_core::Ok(std::vector<std::byte>{});
        };
    }

    // For fixed-size components, create a simple binary factory
    std::size_t comp_size = schema.size;
    std::vector<FieldSchema> fields = schema.fields;

    return [comp_size, fields]([[maybe_unused]] const nlohmann::json& data) -> void_core::Result<std::vector<std::byte>> {
        std::vector<std::byte> bytes(comp_size, std::byte{0});

        // This is a simplified implementation - in production you'd want proper field layout
        // For now, just validate and return zeroed bytes
        // Real implementation would parse each field and place at correct offset

        return void_core::Ok(std::move(bytes));
    };
}

ComponentApplier ComponentSchemaRegistry::create_default_applier(
    const ComponentSchema& schema,
    void_ecs::ComponentId comp_id)
{
    std::size_t comp_size = schema.size;

    return [comp_id, comp_size](void_ecs::World& world,
                                 void_ecs::Entity entity,
                                 [[maybe_unused]] const nlohmann::json& data) -> void_core::Result<void> {
        // Create component bytes
        std::vector<std::byte> bytes(comp_size, std::byte{0});

        // Add to entity using raw API
        if (!world.add_component_raw(entity, comp_id, bytes.data(), bytes.size())) {
            return void_core::Err("Failed to add component to entity");
        }

        return void_core::Ok();
    };
}

// =============================================================================
// Utility Functions
// =============================================================================

void_core::Result<std::vector<std::byte>> parse_field_value(
    const nlohmann::json& value,
    FieldType type)
{
    std::vector<std::byte> result;

    auto push_value = [&result](const auto& v) {
        const std::byte* ptr = reinterpret_cast<const std::byte*>(&v);
        result.insert(result.end(), ptr, ptr + sizeof(v));
    };

    switch (type) {
        case FieldType::Bool: {
            bool v = value.get<bool>();
            push_value(v);
            break;
        }
        case FieldType::Int32: {
            std::int32_t v = value.get<std::int32_t>();
            push_value(v);
            break;
        }
        case FieldType::Int64: {
            std::int64_t v = value.get<std::int64_t>();
            push_value(v);
            break;
        }
        case FieldType::UInt32: {
            std::uint32_t v = value.get<std::uint32_t>();
            push_value(v);
            break;
        }
        case FieldType::UInt64: {
            std::uint64_t v = value.get<std::uint64_t>();
            push_value(v);
            break;
        }
        case FieldType::Float32: {
            float v = value.get<float>();
            push_value(v);
            break;
        }
        case FieldType::Float64: {
            double v = value.get<double>();
            push_value(v);
            break;
        }
        case FieldType::Vec2: {
            float x = value[0].get<float>();
            float y = value[1].get<float>();
            push_value(x);
            push_value(y);
            break;
        }
        case FieldType::Vec3: {
            float x = value[0].get<float>();
            float y = value[1].get<float>();
            float z = value[2].get<float>();
            push_value(x);
            push_value(y);
            push_value(z);
            break;
        }
        case FieldType::Vec4:
        case FieldType::Quat: {
            float x = value[0].get<float>();
            float y = value[1].get<float>();
            float z = value[2].get<float>();
            float w = value[3].get<float>();
            push_value(x);
            push_value(y);
            push_value(z);
            push_value(w);
            break;
        }
        case FieldType::Mat4: {
            for (int i = 0; i < 16; ++i) {
                float v = value[i].get<float>();
                push_value(v);
            }
            break;
        }
        default:
            return void_core::Err<std::vector<std::byte>>("Unsupported field type for binary conversion");
    }

    return void_core::Ok(std::move(result));
}

nlohmann::json serialize_field_value(
    const std::byte* data,
    [[maybe_unused]] std::size_t size,
    FieldType type)
{
    switch (type) {
        case FieldType::Bool:
            return *reinterpret_cast<const bool*>(data);
        case FieldType::Int32:
            return *reinterpret_cast<const std::int32_t*>(data);
        case FieldType::Int64:
            return *reinterpret_cast<const std::int64_t*>(data);
        case FieldType::UInt32:
            return *reinterpret_cast<const std::uint32_t*>(data);
        case FieldType::UInt64:
            return *reinterpret_cast<const std::uint64_t*>(data);
        case FieldType::Float32:
            return *reinterpret_cast<const float*>(data);
        case FieldType::Float64:
            return *reinterpret_cast<const double*>(data);
        case FieldType::Vec2: {
            const float* f = reinterpret_cast<const float*>(data);
            return nlohmann::json::array({f[0], f[1]});
        }
        case FieldType::Vec3: {
            const float* f = reinterpret_cast<const float*>(data);
            return nlohmann::json::array({f[0], f[1], f[2]});
        }
        case FieldType::Vec4:
        case FieldType::Quat: {
            const float* f = reinterpret_cast<const float*>(data);
            return nlohmann::json::array({f[0], f[1], f[2], f[3]});
        }
        case FieldType::Mat4: {
            const float* f = reinterpret_cast<const float*>(data);
            nlohmann::json arr = nlohmann::json::array();
            for (int i = 0; i < 16; ++i) {
                arr.push_back(f[i]);
            }
            return arr;
        }
        default:
            return nullptr;
    }
}

} // namespace void_package
