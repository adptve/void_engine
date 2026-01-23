#pragma once

/// @file type_registry.hpp
/// @brief Runtime type information and registry for void_core

#include "fwd.hpp"
#include "error.hpp"
#include <cstdint>
#include <string>
#include <typeinfo>
#include <typeindex>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <any>
#include <optional>

namespace void_core {

// =============================================================================
// PrimitiveType
// =============================================================================

/// Primitive type enumeration
enum class PrimitiveType : std::uint8_t {
    Bool,
    I8, I16, I32, I64,
    U8, U16, U32, U64,
    F32, F64,
    Char,
    String,
};

/// Get primitive type name
[[nodiscard]] inline const char* primitive_type_name(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::Bool: return "bool";
        case PrimitiveType::I8: return "i8";
        case PrimitiveType::I16: return "i16";
        case PrimitiveType::I32: return "i32";
        case PrimitiveType::I64: return "i64";
        case PrimitiveType::U8: return "u8";
        case PrimitiveType::U16: return "u16";
        case PrimitiveType::U32: return "u32";
        case PrimitiveType::U64: return "u64";
        case PrimitiveType::F32: return "f32";
        case PrimitiveType::F64: return "f64";
        case PrimitiveType::Char: return "char";
        case PrimitiveType::String: return "string";
        default: return "unknown";
    }
}

// =============================================================================
// TypeSchema
// =============================================================================

/// Field information for struct types
struct FieldInfo {
    std::string name;
    std::size_t offset = 0;
    std::shared_ptr<struct TypeSchema> schema;

    FieldInfo() = default;
    FieldInfo(std::string n, std::size_t off, std::shared_ptr<TypeSchema> sch)
        : name(std::move(n)), offset(off), schema(std::move(sch)) {}
};

/// Variant information for enum types
struct VariantInfo {
    std::string name;
    std::int64_t discriminant = 0;
    std::vector<FieldInfo> fields;

    VariantInfo() = default;
    VariantInfo(std::string n, std::int64_t disc)
        : name(std::move(n)), discriminant(disc) {}
};

/// Type schema for serialization
struct TypeSchema {
    enum class Kind : std::uint8_t {
        Primitive,
        Struct,
        Enum,
        Array,
        Optional,
        Map,
        Tuple,
        Opaque,
    };

    Kind kind = Kind::Opaque;

    // For Primitive
    PrimitiveType primitive_type = PrimitiveType::Bool;

    // For Struct
    std::vector<FieldInfo> fields;

    // For Enum
    std::vector<VariantInfo> variants;

    // For Array, Optional
    std::shared_ptr<TypeSchema> element_type;

    // For Map
    std::shared_ptr<TypeSchema> key_type;
    std::shared_ptr<TypeSchema> value_type;

    // For Tuple
    std::vector<std::shared_ptr<TypeSchema>> tuple_elements;

    /// Factory methods
    [[nodiscard]] static TypeSchema primitive(PrimitiveType ptype) {
        TypeSchema s;
        s.kind = Kind::Primitive;
        s.primitive_type = ptype;
        return s;
    }

    [[nodiscard]] static TypeSchema structure(std::vector<FieldInfo> f) {
        TypeSchema s;
        s.kind = Kind::Struct;
        s.fields = std::move(f);
        return s;
    }

    [[nodiscard]] static TypeSchema enumeration(std::vector<VariantInfo> v) {
        TypeSchema s;
        s.kind = Kind::Enum;
        s.variants = std::move(v);
        return s;
    }

    [[nodiscard]] static TypeSchema array(std::shared_ptr<TypeSchema> elem) {
        TypeSchema s;
        s.kind = Kind::Array;
        s.element_type = std::move(elem);
        return s;
    }

    [[nodiscard]] static TypeSchema optional(std::shared_ptr<TypeSchema> inner) {
        TypeSchema s;
        s.kind = Kind::Optional;
        s.element_type = std::move(inner);
        return s;
    }

    [[nodiscard]] static TypeSchema map(std::shared_ptr<TypeSchema> key, std::shared_ptr<TypeSchema> value) {
        TypeSchema s;
        s.kind = Kind::Map;
        s.key_type = std::move(key);
        s.value_type = std::move(value);
        return s;
    }

    [[nodiscard]] static TypeSchema opaque() {
        TypeSchema s;
        s.kind = Kind::Opaque;
        return s;
    }
};

// =============================================================================
// TypeInfo
// =============================================================================

/// Runtime type information
struct TypeInfo {
    std::type_index type_id;
    std::string name;
    std::size_t size = 0;
    std::size_t align = 0;
    bool needs_drop = false;
    std::optional<TypeSchema> schema;

    TypeInfo() : type_id(typeid(void)) {}

    TypeInfo(std::type_index tid, std::string n, std::size_t sz, std::size_t al, bool drop)
        : type_id(tid), name(std::move(n)), size(sz), align(al), needs_drop(drop) {}

    /// Create TypeInfo for type T
    template<typename T>
    [[nodiscard]] static TypeInfo of() {
        TypeInfo info;
        info.type_id = std::type_index(typeid(T));
        info.name = typeid(T).name();  // Compiler-specific mangled name
        info.size = sizeof(T);
        info.align = alignof(T);
        info.needs_drop = !std::is_trivially_destructible_v<T>;
        return info;
    }

    /// Add schema
    TypeInfo& with_schema(TypeSchema s) {
        schema = std::move(s);
        return *this;
    }

    /// Add readable name
    TypeInfo& with_name(const std::string& readable_name) {
        name = readable_name;
        return *this;
    }
};

// =============================================================================
// DynType (Dynamic Type Interface)
// =============================================================================

/// Interface for dynamically-typed objects
class DynType {
public:
    virtual ~DynType() = default;

    /// Get type information
    [[nodiscard]] virtual TypeInfo type_info() const = 0;

    /// Clone to heap
    [[nodiscard]] virtual std::unique_ptr<DynType> clone_box() const = 0;

    /// Get as std::any reference
    [[nodiscard]] virtual std::any as_any() const = 0;

    /// Serialize to bytes (optional)
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> to_bytes() const {
        return std::nullopt;
    }

    /// Deserialize from bytes (optional)
    [[nodiscard]] virtual bool from_bytes(const std::vector<std::uint8_t>& /*bytes*/) {
        return false;
    }

    /// Downcast to concrete type
    template<typename T>
    [[nodiscard]] T* downcast() {
        if (type_info().type_id == std::type_index(typeid(T))) {
            return static_cast<T*>(get_raw_ptr());
        }
        return nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* downcast() const {
        if (type_info().type_id == std::type_index(typeid(T))) {
            return static_cast<const T*>(get_raw_ptr());
        }
        return nullptr;
    }

protected:
    /// Get raw pointer to underlying value
    [[nodiscard]] virtual void* get_raw_ptr() = 0;
    [[nodiscard]] virtual const void* get_raw_ptr() const = 0;
};

/// Concrete wrapper for any type
template<typename T>
class DynTypeImpl : public DynType {
public:
    explicit DynTypeImpl(T value) : m_value(std::move(value)) {}

    [[nodiscard]] TypeInfo type_info() const override {
        return TypeInfo::of<T>();
    }

    [[nodiscard]] std::unique_ptr<DynType> clone_box() const override {
        if constexpr (std::is_copy_constructible_v<T>) {
            return std::make_unique<DynTypeImpl<T>>(m_value);
        } else {
            return nullptr;
        }
    }

    [[nodiscard]] std::any as_any() const override {
        return m_value;
    }

    /// Access the value
    [[nodiscard]] T& value() { return m_value; }
    [[nodiscard]] const T& value() const { return m_value; }

protected:
    [[nodiscard]] void* get_raw_ptr() override { return &m_value; }
    [[nodiscard]] const void* get_raw_ptr() const override { return &m_value; }

private:
    T m_value;
};

/// Create DynType from value
template<typename T>
[[nodiscard]] std::unique_ptr<DynType> make_dyn(T value) {
    return std::make_unique<DynTypeImpl<T>>(std::move(value));
}

// =============================================================================
// TypeRegistry
// =============================================================================

/// Central type registration and instantiation system
class TypeRegistry {
public:
    using Constructor = std::function<std::unique_ptr<DynType>()>;

    /// Constructor
    TypeRegistry() = default;

    /// Register type with default constructor
    template<typename T>
    TypeRegistry& register_type() {
        static_assert(std::is_default_constructible_v<T>,
            "Type must be default constructible for registration with constructor");

        auto type_id = std::type_index(typeid(T));
        TypeInfo info = TypeInfo::of<T>();

        m_by_id[type_id] = info;
        m_by_name.insert_or_assign(info.name, type_id);
        m_constructors[type_id] = []() -> std::unique_ptr<DynType> {
            return std::make_unique<DynTypeImpl<T>>(T{});
        };

        return *this;
    }

    /// Register type with custom info (no constructor)
    TypeRegistry& register_with_info(TypeInfo info) {
        m_by_id[info.type_id] = info;
        m_by_name.insert_or_assign(info.name, info.type_id);
        return *this;
    }

    /// Register type with name and constructor
    template<typename T>
    TypeRegistry& register_with_name(const std::string& name) {
        auto type_id = std::type_index(typeid(T));
        TypeInfo info = TypeInfo::of<T>().with_name(name);

        m_by_id[type_id] = info;
        m_by_name.insert_or_assign(name, type_id);

        if constexpr (std::is_default_constructible_v<T>) {
            m_constructors[type_id] = []() -> std::unique_ptr<DynType> {
                return std::make_unique<DynTypeImpl<T>>(T{});
            };
        }

        return *this;
    }

    /// Get type info by type
    template<typename T>
    [[nodiscard]] const TypeInfo* get() const {
        return get(std::type_index(typeid(T)));
    }

    /// Get type info by type_index
    [[nodiscard]] const TypeInfo* get(std::type_index type_id) const {
        auto it = m_by_id.find(type_id);
        return it != m_by_id.end() ? &it->second : nullptr;
    }

    /// Get type info by name
    [[nodiscard]] const TypeInfo* get_by_name(const std::string& name) const {
        auto it = m_by_name.find(name);
        if (it == m_by_name.end()) {
            return nullptr;
        }
        return get(it->second);
    }

    /// Check if type is registered
    template<typename T>
    [[nodiscard]] bool contains() const {
        return contains(std::type_index(typeid(T)));
    }

    [[nodiscard]] bool contains(std::type_index type_id) const {
        return m_by_id.find(type_id) != m_by_id.end();
    }

    [[nodiscard]] bool contains_name(const std::string& name) const {
        return m_by_name.find(name) != m_by_name.end();
    }

    /// Create instance by type
    template<typename T>
    [[nodiscard]] std::unique_ptr<DynType> create() const {
        return create(std::type_index(typeid(T)));
    }

    /// Create instance by type_index
    [[nodiscard]] std::unique_ptr<DynType> create(std::type_index type_id) const {
        auto it = m_constructors.find(type_id);
        if (it == m_constructors.end()) {
            return nullptr;
        }
        return it->second();
    }

    /// Create instance by name
    [[nodiscard]] std::unique_ptr<DynType> create_by_name(const std::string& name) const {
        auto it = m_by_name.find(name);
        if (it == m_by_name.end()) {
            return nullptr;
        }
        return create(it->second);
    }

    /// Get type count
    [[nodiscard]] std::size_t len() const noexcept {
        return m_by_id.size();
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return m_by_id.empty();
    }

    /// Clear all registrations
    void clear() {
        m_by_id.clear();
        m_by_name.clear();
        m_constructors.clear();
    }

    /// Iterate over all types
    template<typename F>
    void for_each(F&& func) const {
        for (const auto& [id, info] : m_by_id) {
            func(info);
        }
    }

    /// Get result wrapper with error handling
    [[nodiscard]] Result<std::reference_wrapper<const TypeInfo>> get_result(std::type_index type_id) const {
        const TypeInfo* info = get(type_id);
        if (!info) {
            // Use type name from type_index
            return Err<std::reference_wrapper<const TypeInfo>>(
                TypeRegistryError::not_registered(type_id.name()));
        }
        return Ok(std::cref(*info));
    }

    [[nodiscard]] Result<std::reference_wrapper<const TypeInfo>> get_result_by_name(const std::string& name) const {
        const TypeInfo* info = get_by_name(name);
        if (!info) {
            return Err<std::reference_wrapper<const TypeInfo>>(
                TypeRegistryError::not_registered(name));
        }
        return Ok(std::cref(*info));
    }

private:
    std::map<std::type_index, TypeInfo> m_by_id;
    std::map<std::string, std::type_index> m_by_name;
    std::map<std::type_index, Constructor> m_constructors;
};

} // namespace void_core
