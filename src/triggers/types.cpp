/// @file types.cpp
/// @brief Core types implementation for void_triggers module

#include <void_engine/triggers/types.hpp>

#include <cmath>

namespace void_triggers {

bool VariableValue::compare(const VariableValue& other, CompareOp op) const {
    switch (op) {
        case CompareOp::Equal:
            if (type == VariableType::Bool) return as_bool() == other.as_bool();
            if (type == VariableType::Int) return as_int() == other.as_int();
            if (type == VariableType::Float) return std::abs(as_float() - other.as_float()) < 0.0001f;
            if (type == VariableType::String) return as_string() == other.as_string();
            return false;

        case CompareOp::NotEqual:
            return !compare(other, CompareOp::Equal);

        case CompareOp::Less:
            if (type == VariableType::Int) return as_int() < other.as_int();
            if (type == VariableType::Float) return as_float() < other.as_float();
            return false;

        case CompareOp::LessEqual:
            if (type == VariableType::Int) return as_int() <= other.as_int();
            if (type == VariableType::Float) return as_float() <= other.as_float();
            return false;

        case CompareOp::Greater:
            if (type == VariableType::Int) return as_int() > other.as_int();
            if (type == VariableType::Float) return as_float() > other.as_float();
            return false;

        case CompareOp::GreaterEqual:
            if (type == VariableType::Int) return as_int() >= other.as_int();
            if (type == VariableType::Float) return as_float() >= other.as_float();
            return false;

        case CompareOp::Contains:
            if (type == VariableType::String) {
                return as_string().find(other.as_string()) != std::string::npos;
            }
            return false;

        case CompareOp::NotContains:
            return !compare(other, CompareOp::Contains);

        default:
            return false;
    }
}

} // namespace void_triggers
