#pragma once

/// @file value.hpp
/// @brief Dynamic value type for void_ir patches

#include "fwd.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <array>
#include <cmath>
#include <stdexcept>

namespace void_ir {

// =============================================================================
// Math Types (minimal for IR values)
// =============================================================================

/// 2D vector
struct Vec2 {
    float x = 0.0f, y = 0.0f;

    constexpr Vec2() noexcept = default;
    constexpr Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}

    constexpr bool operator==(const Vec2& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

/// 3D vector
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr Vec3() noexcept = default;
    constexpr Vec3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr bool operator==(const Vec3& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }
};

/// 4D vector
struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

    constexpr Vec4() noexcept = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) noexcept
        : x(x_), y(y_), z(z_), w(w_) {}

    constexpr bool operator==(const Vec4& other) const noexcept {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
};

/// 4x4 matrix (column-major)
struct Mat4 {
    std::array<float, 16> data = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    constexpr Mat4() noexcept = default;

    explicit Mat4(const std::array<float, 16>& d) : data(d) {}

    [[nodiscard]] static Mat4 identity() noexcept {
        return Mat4{};
    }

    bool operator==(const Mat4& other) const noexcept {
        return data == other.data;
    }
};

// =============================================================================
// ValueType
// =============================================================================

/// Value type discriminator
enum class ValueType : std::uint8_t {
    Null = 0,
    Bool,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Mat4,
    Array,
    Object,
    Bytes,
    EntityRef,
    AssetRef
};

/// Get string name for value type
[[nodiscard]] inline const char* value_type_name(ValueType type) noexcept {
    switch (type) {
        case ValueType::Null: return "Null";
        case ValueType::Bool: return "Bool";
        case ValueType::Int: return "Int";
        case ValueType::Float: return "Float";
        case ValueType::String: return "String";
        case ValueType::Vec2: return "Vec2";
        case ValueType::Vec3: return "Vec3";
        case ValueType::Vec4: return "Vec4";
        case ValueType::Mat4: return "Mat4";
        case ValueType::Array: return "Array";
        case ValueType::Object: return "Object";
        case ValueType::Bytes: return "Bytes";
        case ValueType::EntityRef: return "EntityRef";
        case ValueType::AssetRef: return "AssetRef";
        default: return "Unknown";
    }
}

// =============================================================================
// Value
// =============================================================================

/// Forward declare for recursive types
class Value;

/// Array of values
using ValueArray = std::vector<Value>;

/// Object (key-value map)
using ValueObject = std::unordered_map<std::string, Value>;

/// Binary data
using ValueBytes = std::vector<std::uint8_t>;

/// Entity reference (namespace + id)
struct ValueEntityRef {
    std::uint32_t namespace_id = UINT32_MAX;
    std::uint64_t entity_id = 0;

    constexpr bool operator==(const ValueEntityRef& other) const noexcept = default;
};

/// Asset reference
struct ValueAssetRef {
    std::string path;
    std::uint64_t uuid = 0;

    bool operator==(const ValueAssetRef& other) const noexcept {
        return path == other.path && uuid == other.uuid;
    }
};

/// Dynamic value type using std::variant
class Value {
public:
    using Variant = std::variant<
        std::monostate,      // Null
        bool,                // Bool
        std::int64_t,        // Int
        double,              // Float
        std::string,         // String
        Vec2,                // Vec2
        Vec3,                // Vec3
        Vec4,                // Vec4
        Mat4,                // Mat4
        ValueArray,          // Array
        ValueObject,         // Object
        ValueBytes,          // Bytes
        ValueEntityRef,      // EntityRef
        ValueAssetRef        // AssetRef
    >;

    /// Default constructor creates null
    Value() : m_data(std::monostate{}) {}

    /// Construct from bool
    Value(bool v) : m_data(v) {}

    /// Construct from integer types
    Value(int v) : m_data(static_cast<std::int64_t>(v)) {}
    Value(std::int64_t v) : m_data(v) {}
    Value(std::uint64_t v) : m_data(static_cast<std::int64_t>(v)) {}

    /// Construct from floating point
    Value(float v) : m_data(static_cast<double>(v)) {}
    Value(double v) : m_data(v) {}

    /// Construct from string
    Value(const char* v) : m_data(std::string(v)) {}
    Value(std::string v) : m_data(std::move(v)) {}
    Value(std::string_view v) : m_data(std::string(v)) {}

    /// Construct from math types
    Value(Vec2 v) : m_data(v) {}
    Value(Vec3 v) : m_data(v) {}
    Value(Vec4 v) : m_data(v) {}
    Value(Mat4 v) : m_data(std::move(v)) {}

    /// Construct from array
    Value(ValueArray v) : m_data(std::move(v)) {}

    /// Construct from object
    Value(ValueObject v) : m_data(std::move(v)) {}

    /// Construct from bytes
    Value(ValueBytes v) : m_data(std::move(v)) {}

    /// Construct from entity ref
    Value(ValueEntityRef v) : m_data(v) {}

    /// Construct from asset ref
    Value(ValueAssetRef v) : m_data(std::move(v)) {}

    // -------------------------------------------------------------------------
    // Factory methods
    // -------------------------------------------------------------------------

    /// Create null value
    [[nodiscard]] static Value null() { return Value{}; }

    /// Create from initializer list (array)
    [[nodiscard]] static Value array(std::initializer_list<Value> values) {
        return Value(ValueArray(values));
    }

    /// Create empty array
    [[nodiscard]] static Value empty_array() {
        return Value(ValueArray{});
    }

    /// Create empty object
    [[nodiscard]] static Value empty_object() {
        return Value(ValueObject{});
    }

    /// Create entity reference
    [[nodiscard]] static Value entity_ref(std::uint32_t ns, std::uint64_t id) {
        return Value(ValueEntityRef{ns, id});
    }

    /// Create asset reference from path
    [[nodiscard]] static Value asset_path(std::string path) {
        return Value(ValueAssetRef{std::move(path), 0});
    }

    /// Create asset reference from UUID
    [[nodiscard]] static Value asset_uuid(std::uint64_t uuid) {
        return Value(ValueAssetRef{"", uuid});
    }

    // -------------------------------------------------------------------------
    // Type checking
    // -------------------------------------------------------------------------

    /// Get value type
    [[nodiscard]] ValueType type() const noexcept {
        return static_cast<ValueType>(m_data.index());
    }

    /// Get type name
    [[nodiscard]] const char* type_name() const noexcept {
        return value_type_name(type());
    }

    /// Check if null
    [[nodiscard]] bool is_null() const noexcept {
        return std::holds_alternative<std::monostate>(m_data);
    }

    /// Check if bool
    [[nodiscard]] bool is_bool() const noexcept {
        return std::holds_alternative<bool>(m_data);
    }

    /// Check if integer
    [[nodiscard]] bool is_int() const noexcept {
        return std::holds_alternative<std::int64_t>(m_data);
    }

    /// Check if float
    [[nodiscard]] bool is_float() const noexcept {
        return std::holds_alternative<double>(m_data);
    }

    /// Check if numeric (int or float)
    [[nodiscard]] bool is_numeric() const noexcept {
        return is_int() || is_float();
    }

    /// Check if string
    [[nodiscard]] bool is_string() const noexcept {
        return std::holds_alternative<std::string>(m_data);
    }

    /// Check if Vec2
    [[nodiscard]] bool is_vec2() const noexcept {
        return std::holds_alternative<Vec2>(m_data);
    }

    /// Check if Vec3
    [[nodiscard]] bool is_vec3() const noexcept {
        return std::holds_alternative<Vec3>(m_data);
    }

    /// Check if Vec4
    [[nodiscard]] bool is_vec4() const noexcept {
        return std::holds_alternative<Vec4>(m_data);
    }

    /// Check if Mat4
    [[nodiscard]] bool is_mat4() const noexcept {
        return std::holds_alternative<Mat4>(m_data);
    }

    /// Check if array
    [[nodiscard]] bool is_array() const noexcept {
        return std::holds_alternative<ValueArray>(m_data);
    }

    /// Check if object
    [[nodiscard]] bool is_object() const noexcept {
        return std::holds_alternative<ValueObject>(m_data);
    }

    /// Check if bytes
    [[nodiscard]] bool is_bytes() const noexcept {
        return std::holds_alternative<ValueBytes>(m_data);
    }

    /// Check if entity reference
    [[nodiscard]] bool is_entity_ref() const noexcept {
        return std::holds_alternative<ValueEntityRef>(m_data);
    }

    /// Check if asset reference
    [[nodiscard]] bool is_asset_ref() const noexcept {
        return std::holds_alternative<ValueAssetRef>(m_data);
    }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// Get as bool (throws if wrong type)
    [[nodiscard]] bool as_bool() const {
        return std::get<bool>(m_data);
    }

    /// Get as int (throws if wrong type)
    [[nodiscard]] std::int64_t as_int() const {
        return std::get<std::int64_t>(m_data);
    }

    /// Get as float (throws if wrong type)
    [[nodiscard]] double as_float() const {
        return std::get<double>(m_data);
    }

    /// Get as numeric (converts int to float if needed)
    [[nodiscard]] double as_numeric() const {
        if (is_int()) {
            return static_cast<double>(as_int());
        }
        return as_float();
    }

    /// Get as string (throws if wrong type)
    [[nodiscard]] const std::string& as_string() const {
        return std::get<std::string>(m_data);
    }

    /// Get as Vec2 (throws if wrong type)
    [[nodiscard]] const Vec2& as_vec2() const {
        return std::get<Vec2>(m_data);
    }

    /// Get as Vec3 (throws if wrong type)
    [[nodiscard]] const Vec3& as_vec3() const {
        return std::get<Vec3>(m_data);
    }

    /// Get as Vec4 (throws if wrong type)
    [[nodiscard]] const Vec4& as_vec4() const {
        return std::get<Vec4>(m_data);
    }

    /// Get as Mat4 (throws if wrong type)
    [[nodiscard]] const Mat4& as_mat4() const {
        return std::get<Mat4>(m_data);
    }

    /// Get as array (throws if wrong type)
    [[nodiscard]] const ValueArray& as_array() const {
        return std::get<ValueArray>(m_data);
    }

    /// Get as mutable array (throws if wrong type)
    [[nodiscard]] ValueArray& as_array_mut() {
        return std::get<ValueArray>(m_data);
    }

    /// Get as object (throws if wrong type)
    [[nodiscard]] const ValueObject& as_object() const {
        return std::get<ValueObject>(m_data);
    }

    /// Get as mutable object (throws if wrong type)
    [[nodiscard]] ValueObject& as_object_mut() {
        return std::get<ValueObject>(m_data);
    }

    /// Get as bytes (throws if wrong type)
    [[nodiscard]] const ValueBytes& as_bytes() const {
        return std::get<ValueBytes>(m_data);
    }

    /// Get as entity ref (throws if wrong type)
    [[nodiscard]] const ValueEntityRef& as_entity_ref() const {
        return std::get<ValueEntityRef>(m_data);
    }

    /// Get as asset ref (throws if wrong type)
    [[nodiscard]] const ValueAssetRef& as_asset_ref() const {
        return std::get<ValueAssetRef>(m_data);
    }

    // -------------------------------------------------------------------------
    // Optional accessors (return nullopt on type mismatch)
    // -------------------------------------------------------------------------

    [[nodiscard]] std::optional<bool> try_bool() const noexcept {
        if (auto* p = std::get_if<bool>(&m_data)) return *p;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::int64_t> try_int() const noexcept {
        if (auto* p = std::get_if<std::int64_t>(&m_data)) return *p;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<double> try_float() const noexcept {
        if (auto* p = std::get_if<double>(&m_data)) return *p;
        return std::nullopt;
    }

    [[nodiscard]] const std::string* try_string() const noexcept {
        return std::get_if<std::string>(&m_data);
    }

    [[nodiscard]] const Vec2* try_vec2() const noexcept {
        return std::get_if<Vec2>(&m_data);
    }

    [[nodiscard]] const Vec3* try_vec3() const noexcept {
        return std::get_if<Vec3>(&m_data);
    }

    [[nodiscard]] const Vec4* try_vec4() const noexcept {
        return std::get_if<Vec4>(&m_data);
    }

    [[nodiscard]] const ValueArray* try_array() const noexcept {
        return std::get_if<ValueArray>(&m_data);
    }

    [[nodiscard]] const ValueObject* try_object() const noexcept {
        return std::get_if<ValueObject>(&m_data);
    }

    // -------------------------------------------------------------------------
    // Array operations
    // -------------------------------------------------------------------------

    /// Get array size (0 if not array)
    [[nodiscard]] std::size_t size() const noexcept {
        if (auto* arr = std::get_if<ValueArray>(&m_data)) {
            return arr->size();
        }
        if (auto* obj = std::get_if<ValueObject>(&m_data)) {
            return obj->size();
        }
        return 0;
    }

    /// Array index access (throws if not array or out of bounds)
    [[nodiscard]] const Value& operator[](std::size_t index) const {
        return std::get<ValueArray>(m_data).at(index);
    }

    /// Array index access mutable
    [[nodiscard]] Value& operator[](std::size_t index) {
        return std::get<ValueArray>(m_data).at(index);
    }

    /// Object key access (throws if not object)
    [[nodiscard]] const Value& operator[](const std::string& key) const {
        return std::get<ValueObject>(m_data).at(key);
    }

    /// Object key access mutable (inserts if not found)
    [[nodiscard]] Value& operator[](const std::string& key) {
        return std::get<ValueObject>(m_data)[key];
    }

    /// Check if object contains key
    [[nodiscard]] bool contains(const std::string& key) const {
        if (auto* obj = std::get_if<ValueObject>(&m_data)) {
            return obj->find(key) != obj->end();
        }
        return false;
    }

    /// Get value from object (returns null if not found)
    [[nodiscard]] const Value* get(const std::string& key) const {
        if (auto* obj = std::get_if<ValueObject>(&m_data)) {
            auto it = obj->find(key);
            if (it != obj->end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // Comparison
    // -------------------------------------------------------------------------

    bool operator==(const Value& other) const {
        return m_data == other.m_data;
    }

    bool operator!=(const Value& other) const {
        return m_data != other.m_data;
    }

    // -------------------------------------------------------------------------
    // Clone
    // -------------------------------------------------------------------------

    /// Deep clone the value
    [[nodiscard]] Value clone() const {
        return Value(m_data);
    }

private:
    /// Construct from variant (for cloning)
    explicit Value(Variant data) : m_data(std::move(data)) {}

    Variant m_data;
};

} // namespace void_ir
