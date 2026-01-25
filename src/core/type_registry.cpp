/// @file type_registry.cpp
/// @brief Type registry implementation for void_core
///
/// The type registry is primarily template-based and header-only.
/// This file provides:
/// - Global type registry instance
/// - Type serialization utilities
/// - Built-in type registrations
/// - Hot-reload safe type management

#include <void_engine/core/type_registry.hpp>
#include <void_engine/core/id.hpp>
#include <void_engine/core/version.hpp>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace void_core {

// =============================================================================
// Global Type Registry
// =============================================================================

namespace {

/// Global type registry instance
TypeRegistry s_global_registry;

/// Mutex for thread-safe access during registration
std::mutex s_registry_mutex;

/// Flag indicating if built-in types have been registered
bool s_builtins_registered = false;

} // anonymous namespace

/// Get the global type registry
TypeRegistry& global_type_registry() {
    return s_global_registry;
}

/// Thread-safe type registration
template<typename T>
void register_global_type() {
    std::lock_guard<std::mutex> lock(s_registry_mutex);
    s_global_registry.register_type<T>();
}

/// Thread-safe type registration with name
template<typename T>
void register_global_type_with_name(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_registry_mutex);
    s_global_registry.register_with_name<T>(name);
}

// =============================================================================
// Built-in Type Registration
// =============================================================================

/// Register all built-in primitive types
void register_builtin_types() {
    std::lock_guard<std::mutex> lock(s_registry_mutex);

    if (s_builtins_registered) {
        return;
    }

    // Primitive types
    s_global_registry.register_with_name<bool>("bool");
    s_global_registry.register_with_name<std::int8_t>("i8");
    s_global_registry.register_with_name<std::int16_t>("i16");
    s_global_registry.register_with_name<std::int32_t>("i32");
    s_global_registry.register_with_name<std::int64_t>("i64");
    s_global_registry.register_with_name<std::uint8_t>("u8");
    s_global_registry.register_with_name<std::uint16_t>("u16");
    s_global_registry.register_with_name<std::uint32_t>("u32");
    s_global_registry.register_with_name<std::uint64_t>("u64");
    s_global_registry.register_with_name<float>("f32");
    s_global_registry.register_with_name<double>("f64");
    s_global_registry.register_with_name<char>("char");
    s_global_registry.register_with_name<std::string>("string");

    // Core types
    s_global_registry.register_with_name<Id>("Id");
    s_global_registry.register_with_name<NamedId>("NamedId");
    s_global_registry.register_with_name<Version>("Version");

    s_builtins_registered = true;
}

/// Check if built-in types are registered
bool are_builtins_registered() {
    return s_builtins_registered;
}

// =============================================================================
// Type Schema Builders
// =============================================================================

namespace schema {

/// Build schema for primitive types
TypeSchema build_primitive_schema(PrimitiveType type) {
    return TypeSchema::primitive(type);
}

/// Build schema for a struct with named fields
class StructSchemaBuilder {
public:
    StructSchemaBuilder() = default;

    /// Add a field to the struct
    template<typename T>
    StructSchemaBuilder& field(const std::string& name, std::size_t offset) {
        auto field_schema = std::make_shared<TypeSchema>(TypeSchema::opaque());

        // Try to get schema from global registry
        const TypeInfo* info = s_global_registry.get<T>();
        if (info && info->schema.has_value()) {
            field_schema = std::make_shared<TypeSchema>(info->schema.value());
        }

        m_fields.emplace_back(name, offset, field_schema);
        return *this;
    }

    /// Build the final schema
    TypeSchema build() {
        return TypeSchema::structure(std::move(m_fields));
    }

private:
    std::vector<FieldInfo> m_fields;
};

/// Build schema for an enum with variants
class EnumSchemaBuilder {
public:
    EnumSchemaBuilder() = default;

    /// Add a variant to the enum
    EnumSchemaBuilder& variant(const std::string& name, std::int64_t discriminant) {
        m_variants.emplace_back(name, discriminant);
        return *this;
    }

    /// Build the final schema
    TypeSchema build() {
        return TypeSchema::enumeration(std::move(m_variants));
    }

private:
    std::vector<VariantInfo> m_variants;
};

/// Create an array schema
TypeSchema array_of(const TypeSchema& element) {
    return TypeSchema::array(std::make_shared<TypeSchema>(element));
}

/// Create an optional schema
TypeSchema optional_of(const TypeSchema& inner) {
    return TypeSchema::optional(std::make_shared<TypeSchema>(inner));
}

/// Create a map schema
TypeSchema map_of(const TypeSchema& key, const TypeSchema& value) {
    return TypeSchema::map(
        std::make_shared<TypeSchema>(key),
        std::make_shared<TypeSchema>(value));
}

} // namespace schema

// =============================================================================
// Type Info Formatting
// =============================================================================

namespace debug {

/// Format TypeInfo for debugging
std::string format_type_info(const TypeInfo& info) {
    std::ostringstream oss;
    oss << "TypeInfo {\n"
        << "  name: \"" << info.name << "\",\n"
        << "  size: " << info.size << ",\n"
        << "  align: " << info.align << ",\n"
        << "  needs_drop: " << (info.needs_drop ? "true" : "false") << ",\n"
        << "  has_schema: " << (info.schema.has_value() ? "true" : "false") << "\n"
        << "}";
    return oss.str();
}

/// Format TypeSchema for debugging
std::string format_type_schema(const TypeSchema& schema, int indent) {
    std::string pad(indent * 2, ' ');
    std::ostringstream oss;

    switch (schema.kind) {
        case TypeSchema::Kind::Primitive:
            oss << pad << "Primitive(" << primitive_type_name(schema.primitive_type) << ")";
            break;

        case TypeSchema::Kind::Struct:
            oss << pad << "Struct {\n";
            for (const auto& field : schema.fields) {
                oss << pad << "  " << field.name << " @" << field.offset << ": ";
                if (field.schema) {
                    oss << "\n" << format_type_schema(*field.schema, indent + 2);
                } else {
                    oss << "opaque";
                }
                oss << ",\n";
            }
            oss << pad << "}";
            break;

        case TypeSchema::Kind::Enum:
            oss << pad << "Enum {\n";
            for (const auto& variant : schema.variants) {
                oss << pad << "  " << variant.name << " = " << variant.discriminant << ",\n";
            }
            oss << pad << "}";
            break;

        case TypeSchema::Kind::Array:
            oss << pad << "Array<";
            if (schema.element_type) {
                oss << "\n" << format_type_schema(*schema.element_type, indent + 1) << "\n" << pad;
            } else {
                oss << "?";
            }
            oss << ">";
            break;

        case TypeSchema::Kind::Optional:
            oss << pad << "Optional<";
            if (schema.element_type) {
                oss << "\n" << format_type_schema(*schema.element_type, indent + 1) << "\n" << pad;
            } else {
                oss << "?";
            }
            oss << ">";
            break;

        case TypeSchema::Kind::Map:
            oss << pad << "Map<";
            if (schema.key_type && schema.value_type) {
                oss << "\n" << format_type_schema(*schema.key_type, indent + 1) << ",\n"
                    << format_type_schema(*schema.value_type, indent + 1) << "\n" << pad;
            } else {
                oss << "?, ?";
            }
            oss << ">";
            break;

        case TypeSchema::Kind::Tuple:
            oss << pad << "Tuple(";
            for (std::size_t i = 0; i < schema.tuple_elements.size(); ++i) {
                if (i > 0) oss << ", ";
                if (schema.tuple_elements[i]) {
                    oss << "\n" << format_type_schema(*schema.tuple_elements[i], indent + 1);
                } else {
                    oss << "?";
                }
            }
            oss << "\n" << pad << ")";
            break;

        case TypeSchema::Kind::Opaque:
            oss << pad << "Opaque";
            break;
    }

    return oss.str();
}

/// Dump all registered types
std::string dump_type_registry() {
    std::ostringstream oss;
    oss << "TypeRegistry (" << s_global_registry.len() << " types):\n";

    s_global_registry.for_each([&oss](const TypeInfo& info) {
        oss << "  - " << info.name << " (" << info.size << " bytes)\n";
    });

    return oss.str();
}

} // namespace debug

// =============================================================================
// Type Serialization Support
// =============================================================================

namespace serialization {

/// Binary serialization constants for TypeInfo
namespace type_binary {
    constexpr std::uint32_t MAGIC = 0x54595045;  // "TYPE"
    constexpr std::uint32_t VERSION = 1;
}

/// Serialize TypeInfo to binary (metadata only, not the actual type)
std::vector<std::uint8_t> serialize_type_info(const TypeInfo& info) {
    std::vector<std::uint8_t> data;
    data.reserve(128);

    // Helper to append data
    auto append = [&data](const void* ptr, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(ptr);
        data.insert(data.end(), bytes, bytes + size);
    };

    // Magic
    append(&type_binary::MAGIC, sizeof(std::uint32_t));

    // Version
    append(&type_binary::VERSION, sizeof(std::uint32_t));

    // Name length and data
    std::uint32_t name_len = static_cast<std::uint32_t>(info.name.size());
    append(&name_len, sizeof(std::uint32_t));
    append(info.name.data(), name_len);

    // Size and alignment
    std::uint64_t size = info.size;
    std::uint64_t align = info.align;
    append(&size, sizeof(std::uint64_t));
    append(&align, sizeof(std::uint64_t));

    // Flags
    std::uint8_t flags = (info.needs_drop ? 1 : 0) | (info.schema.has_value() ? 2 : 0);
    append(&flags, sizeof(std::uint8_t));

    return data;
}

/// Deserialize TypeInfo from binary
Result<TypeInfo> deserialize_type_info(const std::vector<std::uint8_t>& data) {
    if (data.size() < sizeof(std::uint32_t) * 3) {
        return Err<TypeInfo>(Error(ErrorCode::ParseError, "TypeInfo data too short"));
    }

    const auto* ptr = data.data();

    // Verify magic
    std::uint32_t magic;
    std::memcpy(&magic, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (magic != type_binary::MAGIC) {
        return Err<TypeInfo>(Error(ErrorCode::ParseError, "Invalid TypeInfo magic"));
    }

    // Verify version
    std::uint32_t version;
    std::memcpy(&version, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (version != type_binary::VERSION) {
        return Err<TypeInfo>(Error(ErrorCode::IncompatibleVersion, "Unsupported TypeInfo version"));
    }

    // Read name
    std::uint32_t name_len;
    std::memcpy(&name_len, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    std::string name(reinterpret_cast<const char*>(ptr), name_len);
    ptr += name_len;

    // Read size and alignment
    std::uint64_t size, align;
    std::memcpy(&size, ptr, sizeof(std::uint64_t));
    ptr += sizeof(std::uint64_t);
    std::memcpy(&align, ptr, sizeof(std::uint64_t));
    ptr += sizeof(std::uint64_t);

    // Read flags
    std::uint8_t flags;
    std::memcpy(&flags, ptr, sizeof(std::uint8_t));
    bool needs_drop = (flags & 1) != 0;

    TypeInfo info;
    info.type_id = std::type_index(typeid(void));  // Placeholder - actual type unknown at deserialization
    info.name = std::move(name);
    info.size = static_cast<std::size_t>(size);
    info.align = static_cast<std::size_t>(align);
    info.needs_drop = needs_drop;

    return Ok(std::move(info));
}

} // namespace serialization

// =============================================================================
// Hot-Reload Support for Type Registry
// =============================================================================

/// Snapshot the type registry for hot-reload
std::vector<std::uint8_t> snapshot_type_registry() {
    std::lock_guard<std::mutex> lock(s_registry_mutex);

    std::vector<std::uint8_t> data;

    // Header: magic + count
    constexpr std::uint32_t REGISTRY_MAGIC = 0x54524547;  // "TREG"
    data.resize(sizeof(std::uint32_t) * 2);
    std::memcpy(data.data(), &REGISTRY_MAGIC, sizeof(std::uint32_t));
    std::uint32_t count = static_cast<std::uint32_t>(s_global_registry.len());
    std::memcpy(data.data() + sizeof(std::uint32_t), &count, sizeof(std::uint32_t));

    // Serialize each type's info
    s_global_registry.for_each([&data](const TypeInfo& info) {
        auto type_data = serialization::serialize_type_info(info);
        std::uint32_t type_len = static_cast<std::uint32_t>(type_data.size());
        data.insert(data.end(),
            reinterpret_cast<const std::uint8_t*>(&type_len),
            reinterpret_cast<const std::uint8_t*>(&type_len) + sizeof(std::uint32_t));
        data.insert(data.end(), type_data.begin(), type_data.end());
    });

    return data;
}

/// Verify type registry compatibility after hot-reload
Result<void> verify_type_registry_compatibility(const std::vector<std::uint8_t>& snapshot) {
    if (snapshot.size() < sizeof(std::uint32_t) * 2) {
        return Err(Error(ErrorCode::ParseError, "Type registry snapshot too short"));
    }

    // Verify magic
    constexpr std::uint32_t REGISTRY_MAGIC = 0x54524547;  // "TREG"
    std::uint32_t magic;
    std::memcpy(&magic, snapshot.data(), sizeof(std::uint32_t));

    if (magic != REGISTRY_MAGIC) {
        return Err(Error(ErrorCode::ParseError, "Invalid type registry snapshot magic"));
    }

    // Get count
    std::uint32_t count;
    std::memcpy(&count, snapshot.data() + sizeof(std::uint32_t), sizeof(std::uint32_t));

    // Verify we have at least as many types registered
    if (s_global_registry.len() < count) {
        return Err(Error(ErrorCode::IncompatibleVersion,
            "Type registry has fewer types after reload"));
    }

    return Ok();
}

// =============================================================================
// Explicit Template Instantiations
// =============================================================================

// Common type instantiations for global registration
template void register_global_type<bool>();
template void register_global_type<int>();
template void register_global_type<float>();
template void register_global_type<double>();
template void register_global_type<std::string>();

template void register_global_type_with_name<bool>(const std::string&);
template void register_global_type_with_name<int>(const std::string&);
template void register_global_type_with_name<float>(const std::string&);
template void register_global_type_with_name<double>(const std::string&);
template void register_global_type_with_name<std::string>(const std::string&);

} // namespace void_core
