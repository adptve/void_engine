/// @file blackboard.cpp
/// @brief Blackboard implementation for void_ai module

#include <void_engine/ai/blackboard.hpp>

namespace void_ai {

// =============================================================================
// Blackboard Implementation
// =============================================================================

Blackboard::Blackboard(IBlackboard* parent)
    : m_parent(parent) {
}

void Blackboard::set_value(std::string_view key, const BlackboardValue& value) {
    std::string key_str(key);
    m_data[key_str] = value;
    notify_observers(key, value);
}

bool Blackboard::get_value(std::string_view key, BlackboardValue& out_value) const {
    std::string key_str(key);
    auto it = m_data.find(key_str);
    if (it != m_data.end()) {
        out_value = it->second;
        return true;
    }

    // Check parent blackboard
    if (m_parent) {
        return m_parent->get_value(key, out_value);
    }

    return false;
}

bool Blackboard::has_key(std::string_view key) const {
    std::string key_str(key);
    if (m_data.find(key_str) != m_data.end()) {
        return true;
    }

    if (m_parent) {
        return m_parent->has_key(key);
    }

    return false;
}

void Blackboard::remove_key(std::string_view key) {
    m_data.erase(std::string(key));
}

void Blackboard::clear() {
    m_data.clear();
}

void Blackboard::set_bool(std::string_view key, bool value) {
    set_value(key, BlackboardValue{value});
}

void Blackboard::set_int(std::string_view key, int value) {
    set_value(key, BlackboardValue{value});
}

void Blackboard::set_float(std::string_view key, float value) {
    set_value(key, BlackboardValue{value});
}

void Blackboard::set_string(std::string_view key, std::string_view value) {
    set_value(key, BlackboardValue{std::string(value)});
}

void Blackboard::set_vec3(std::string_view key, const void_math::Vec3& value) {
    set_value(key, BlackboardValue{value});
}

bool Blackboard::get_bool(std::string_view key, bool default_value) const {
    BlackboardValue value;
    if (get_value(key, value)) {
        if (auto* v = std::get_if<bool>(&value)) {
            return *v;
        }
    }
    return default_value;
}

int Blackboard::get_int(std::string_view key, int default_value) const {
    BlackboardValue value;
    if (get_value(key, value)) {
        if (auto* v = std::get_if<int>(&value)) {
            return *v;
        }
    }
    return default_value;
}

float Blackboard::get_float(std::string_view key, float default_value) const {
    BlackboardValue value;
    if (get_value(key, value)) {
        if (auto* v = std::get_if<float>(&value)) {
            return *v;
        }
        // Also check double and convert
        if (auto* v = std::get_if<double>(&value)) {
            return static_cast<float>(*v);
        }
    }
    return default_value;
}

std::string Blackboard::get_string(std::string_view key, std::string_view default_value) const {
    BlackboardValue value;
    if (get_value(key, value)) {
        if (auto* v = std::get_if<std::string>(&value)) {
            return *v;
        }
    }
    return std::string(default_value);
}

void_math::Vec3 Blackboard::get_vec3(std::string_view key, const void_math::Vec3& default_value) const {
    BlackboardValue value;
    if (get_value(key, value)) {
        if (auto* v = std::get_if<void_math::Vec3>(&value)) {
            return *v;
        }
    }
    return default_value;
}

void Blackboard::observe(std::string_view key, ChangeCallback callback) {
    m_observers[std::string(key)].push_back(std::move(callback));
}

void Blackboard::unobserve(std::string_view key) {
    m_observers.erase(std::string(key));
}

std::vector<std::pair<std::string, BlackboardValue>> Blackboard::get_all() const {
    std::vector<std::pair<std::string, BlackboardValue>> result;
    result.reserve(m_data.size());
    for (const auto& [key, value] : m_data) {
        result.emplace_back(key, value);
    }
    return result;
}

void Blackboard::merge(const IBlackboard& other) {
    auto all_data = other.get_all();
    for (const auto& [key, value] : all_data) {
        set_value(key, value);
    }
}

void Blackboard::notify_observers(std::string_view key, const BlackboardValue& value) {
    std::string key_str(key);
    auto it = m_observers.find(key_str);
    if (it != m_observers.end()) {
        for (const auto& callback : it->second) {
            callback(key, value);
        }
    }
}

// =============================================================================
// BlackboardScope Implementation
// =============================================================================

BlackboardScope::BlackboardScope(IBlackboard& parent)
    : m_scoped(std::make_unique<Blackboard>(&parent)) {
}

BlackboardScope::~BlackboardScope() = default;

} // namespace void_ai
