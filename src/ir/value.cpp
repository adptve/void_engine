/// @file value.cpp
/// @brief Value serialization implementation for void_ir

#include <void_engine/ir/value.hpp>
#include <void_engine/ir/snapshot.hpp>

namespace void_ir {

namespace {
    constexpr std::uint32_t VALUE_MAGIC = 0x56414C55;  // "VALU"
    constexpr std::uint32_t VALUE_VERSION = 1;
}

std::vector<std::uint8_t> serialize_value_binary(const Value& value) {
    BinaryWriter writer;

    writer.write_u32(VALUE_MAGIC);
    writer.write_u32(VALUE_VERSION);

    serialize_value(writer, value);

    return writer.take();
}

std::optional<Value> deserialize_value_binary(const std::vector<std::uint8_t>& data) {
    if (data.size() < 8) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != VALUE_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != VALUE_VERSION) {
        return std::nullopt;
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return deserialize_value(reader);
}

std::size_t estimate_value_size(const Value& value) {
    std::size_t size = 1;

    switch (value.type()) {
        case ValueType::Null:
            break;
        case ValueType::Bool:
            size += 1;
            break;
        case ValueType::Int:
            size += 8;
            break;
        case ValueType::Float:
            size += 8;
            break;
        case ValueType::String:
            size += 4 + value.as_string().size();
            break;
        case ValueType::Vec2:
            size += 16;
            break;
        case ValueType::Vec3:
            size += 24;
            break;
        case ValueType::Vec4:
            size += 32;
            break;
        case ValueType::Mat4:
            size += 128;
            break;
        case ValueType::Array:
            size += 4;
            for (const auto& elem : value.as_array()) {
                size += estimate_value_size(elem);
            }
            break;
        case ValueType::Object:
            size += 4;
            for (const auto& [key, val] : value.as_object()) {
                size += 4 + key.size();
                size += estimate_value_size(val);
            }
            break;
        case ValueType::Bytes:
            size += 4 + value.as_bytes().size();
            break;
        case ValueType::EntityRef:
            size += 12;
            break;
        case ValueType::AssetRef:
            size += 4 + value.as_asset_ref().path.size() + 8;
            break;
    }

    return size;
}

bool values_equal_epsilon(const Value& a, const Value& b, double epsilon) {
    if (a.type() != b.type()) {
        return false;
    }

    switch (a.type()) {
        case ValueType::Null:
            return true;
        case ValueType::Bool:
            return a.as_bool() == b.as_bool();
        case ValueType::Int:
            return a.as_int() == b.as_int();
        case ValueType::Float:
            {
                double diff = std::abs(a.as_float() - b.as_float());
                return diff <= epsilon;
            }
        case ValueType::String:
            return a.as_string() == b.as_string();
        case ValueType::Vec2:
            {
                const auto& va = a.as_vec2();
                const auto& vb = b.as_vec2();
                return std::abs(va.x - vb.x) <= epsilon &&
                       std::abs(va.y - vb.y) <= epsilon;
            }
        case ValueType::Vec3:
            {
                const auto& va = a.as_vec3();
                const auto& vb = b.as_vec3();
                return std::abs(va.x - vb.x) <= epsilon &&
                       std::abs(va.y - vb.y) <= epsilon &&
                       std::abs(va.z - vb.z) <= epsilon;
            }
        case ValueType::Vec4:
            {
                const auto& va = a.as_vec4();
                const auto& vb = b.as_vec4();
                return std::abs(va.x - vb.x) <= epsilon &&
                       std::abs(va.y - vb.y) <= epsilon &&
                       std::abs(va.z - vb.z) <= epsilon &&
                       std::abs(va.w - vb.w) <= epsilon;
            }
        case ValueType::Mat4:
            {
                const auto& ma = a.as_mat4();
                const auto& mb = b.as_mat4();
                for (int i = 0; i < 16; ++i) {
                    if (std::abs(ma.data[i] - mb.data[i]) > epsilon) {
                        return false;
                    }
                }
                return true;
            }
        case ValueType::Array:
            {
                const auto& aa = a.as_array();
                const auto& ab = b.as_array();
                if (aa.size() != ab.size()) {
                    return false;
                }
                for (std::size_t i = 0; i < aa.size(); ++i) {
                    if (!values_equal_epsilon(aa[i], ab[i], epsilon)) {
                        return false;
                    }
                }
                return true;
            }
        case ValueType::Object:
            {
                const auto& oa = a.as_object();
                const auto& ob = b.as_object();
                if (oa.size() != ob.size()) {
                    return false;
                }
                for (const auto& [key, val] : oa) {
                    auto it = ob.find(key);
                    if (it == ob.end()) {
                        return false;
                    }
                    if (!values_equal_epsilon(val, it->second, epsilon)) {
                        return false;
                    }
                }
                return true;
            }
        case ValueType::Bytes:
            return a.as_bytes() == b.as_bytes();
        case ValueType::EntityRef:
            return a.as_entity_ref() == b.as_entity_ref();
        case ValueType::AssetRef:
            return a.as_asset_ref() == b.as_asset_ref();
    }

    return false;
}

} // namespace void_ir
