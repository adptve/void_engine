#include "types.hpp"

namespace void_graph {

bool Pin::can_connect_to(const Pin& other) const {
    // Cannot connect same direction
    if (direction == other.direction) {
        return false;
    }

    // Exec pins can only connect to exec pins
    if (type == PinType::Exec || other.type == PinType::Exec) {
        return type == PinType::Exec && other.type == PinType::Exec;
    }

    // Any type can connect to anything
    if (type == PinType::Any || other.type == PinType::Any) {
        return true;
    }

    // Same type always works
    if (type == other.type) {
        // For container types, check inner types
        if (type == PinType::Array || type == PinType::Set) {
            return inner_type == PinType::Any ||
                   other.inner_type == PinType::Any ||
                   inner_type == other.inner_type;
        }
        if (type == PinType::Map) {
            return (key_type == PinType::Any || other.key_type == PinType::Any || key_type == other.key_type) &&
                   (inner_type == PinType::Any || other.inner_type == PinType::Any || inner_type == other.inner_type);
        }
        if (type == PinType::Struct || type == PinType::Enum) {
            return type_name.empty() || other.type_name.empty() || type_name == other.type_name;
        }
        return true;
    }

    // Implicit conversions
    return can_implicit_convert(direction == PinDirection::Output ? type : other.type,
                                 direction == PinDirection::Input ? type : other.type);
}

std::uint32_t Pin::get_wire_color() const {
    if (color != 0xFFFFFFFF) {
        return color;
    }
    return pin_type_color(type);
}

bool can_implicit_convert(PinType from, PinType to) {
    // Numeric conversions
    if (is_numeric_type(from) && is_numeric_type(to)) {
        return true;
    }

    // Bool can be converted from numeric
    if (to == PinType::Bool && is_numeric_type(from)) {
        return true;
    }

    // Anything can be converted to string
    if (to == PinType::String) {
        return true;
    }

    // Vector conversions
    if (from == PinType::Vec2 && to == PinType::Vec3) return true;
    if (from == PinType::Vec3 && to == PinType::Vec4) return true;
    if (from == PinType::Vec3 && to == PinType::Vec2) return true;
    if (from == PinType::Vec4 && to == PinType::Vec3) return true;

    // Color <-> Vec4
    if ((from == PinType::Color && to == PinType::Vec4) ||
        (from == PinType::Vec4 && to == PinType::Color)) {
        return true;
    }

    // Object hierarchy
    if (from == PinType::Entity && to == PinType::Object) return true;
    if (from == PinType::Component && to == PinType::Object) return true;
    if (from == PinType::Asset && to == PinType::Object) return true;

    return false;
}

PinType common_numeric_type(PinType a, PinType b) {
    if (!is_numeric_type(a) || !is_numeric_type(b)) {
        return PinType::Any;
    }

    // Priority: Double > Float > Int64 > Int
    if (a == PinType::Double || b == PinType::Double) return PinType::Double;
    if (a == PinType::Float || b == PinType::Float) return PinType::Float;
    if (a == PinType::Int64 || b == PinType::Int64) return PinType::Int64;
    return PinType::Int;
}

} // namespace void_graph
