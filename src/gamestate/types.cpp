/// @file types.cpp
/// @brief Implementation of core types for void_gamestate module

#include "void_engine/gamestate/types.hpp"

#include <cstring>
#include <stdexcept>

namespace void_gamestate {

// =============================================================================
// VariableValue
// =============================================================================

bool VariableValue::as_bool() const {
    if (!value.has_value()) return false;

    switch (type) {
        case VariableType::Bool:
            return std::any_cast<bool>(value);
        case VariableType::Int:
            return std::any_cast<int>(value) != 0;
        case VariableType::Float:
            return std::any_cast<float>(value) != 0.0f;
        case VariableType::String:
            return !std::any_cast<std::string>(value).empty();
        default:
            return false;
    }
}

int VariableValue::as_int() const {
    if (!value.has_value()) return 0;

    switch (type) {
        case VariableType::Bool:
            return std::any_cast<bool>(value) ? 1 : 0;
        case VariableType::Int:
            return std::any_cast<int>(value);
        case VariableType::Float:
            return static_cast<int>(std::any_cast<float>(value));
        case VariableType::String:
            try {
                return std::stoi(std::any_cast<std::string>(value));
            } catch (...) {
                return 0;
            }
        default:
            return 0;
    }
}

float VariableValue::as_float() const {
    if (!value.has_value()) return 0.0f;

    switch (type) {
        case VariableType::Bool:
            return std::any_cast<bool>(value) ? 1.0f : 0.0f;
        case VariableType::Int:
            return static_cast<float>(std::any_cast<int>(value));
        case VariableType::Float:
            return std::any_cast<float>(value);
        case VariableType::String:
            try {
                return std::stof(std::any_cast<std::string>(value));
            } catch (...) {
                return 0.0f;
            }
        default:
            return 0.0f;
    }
}

std::string VariableValue::as_string() const {
    if (!value.has_value()) return "";

    switch (type) {
        case VariableType::Bool:
            return std::any_cast<bool>(value) ? "true" : "false";
        case VariableType::Int:
            return std::to_string(std::any_cast<int>(value));
        case VariableType::Float:
            return std::to_string(std::any_cast<float>(value));
        case VariableType::String:
            return std::any_cast<std::string>(value);
        case VariableType::Vector3: {
            auto v = std::any_cast<Vec3>(value);
            return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        }
        case VariableType::Color: {
            auto c = std::any_cast<Color>(value);
            return "(" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " +
                   std::to_string(c.b) + ", " + std::to_string(c.a) + ")";
        }
        case VariableType::EntityRef:
            return "Entity:" + std::to_string(std::any_cast<EntityId>(value).value);
        default:
            return "";
    }
}

Vec3 VariableValue::as_vector3() const {
    if (!value.has_value()) return Vec3{};

    if (type == VariableType::Vector3) {
        return std::any_cast<Vec3>(value);
    }
    return Vec3{};
}

Color VariableValue::as_color() const {
    if (!value.has_value()) return Color{};

    if (type == VariableType::Color) {
        return std::any_cast<Color>(value);
    }
    return Color{};
}

EntityId VariableValue::as_entity() const {
    if (!value.has_value()) return EntityId{};

    if (type == VariableType::EntityRef) {
        return std::any_cast<EntityId>(value);
    }
    return EntityId{};
}

bool VariableValue::operator==(const VariableValue& other) const {
    if (type != other.type) return false;
    if (!value.has_value() && !other.value.has_value()) return true;
    if (!value.has_value() || !other.value.has_value()) return false;

    switch (type) {
        case VariableType::Bool:
            return std::any_cast<bool>(value) == std::any_cast<bool>(other.value);
        case VariableType::Int:
            return std::any_cast<int>(value) == std::any_cast<int>(other.value);
        case VariableType::Float:
            return std::any_cast<float>(value) == std::any_cast<float>(other.value);
        case VariableType::String:
            return std::any_cast<std::string>(value) == std::any_cast<std::string>(other.value);
        case VariableType::Vector3: {
            auto a = std::any_cast<Vec3>(value);
            auto b = std::any_cast<Vec3>(other.value);
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
        case VariableType::Color: {
            auto a = std::any_cast<Color>(value);
            auto b = std::any_cast<Color>(other.value);
            return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        }
        case VariableType::EntityRef:
            return std::any_cast<EntityId>(value) == std::any_cast<EntityId>(other.value);
        default:
            return false;
    }
}

} // namespace void_gamestate
