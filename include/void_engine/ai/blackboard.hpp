/// @file blackboard.hpp
/// @brief Blackboard system for AI data sharing

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_ai {

// =============================================================================
// Blackboard Key
// =============================================================================

/// @brief Typed blackboard key for compile-time type safety
template<typename T>
struct BlackboardKey {
    std::string name;

    explicit BlackboardKey(std::string_view n) : name(n) {}
    BlackboardKey(const char* n) : name(n) {}
};

// =============================================================================
// Blackboard Interface
// =============================================================================

/// @brief Interface for blackboard data storage
class IBlackboard {
public:
    virtual ~IBlackboard() = default;

    // Type-safe accessors
    template<typename T>
    void set(const BlackboardKey<T>& key, const T& value);

    template<typename T>
    bool get(const BlackboardKey<T>& key, T& out_value) const;

    template<typename T>
    T get_or_default(const BlackboardKey<T>& key, const T& default_value = T{}) const;

    template<typename T>
    bool has(const BlackboardKey<T>& key) const;

    template<typename T>
    void remove(const BlackboardKey<T>& key);

    // String-based accessors
    virtual void set_value(std::string_view key, const BlackboardValue& value) = 0;
    virtual bool get_value(std::string_view key, BlackboardValue& out_value) const = 0;
    virtual bool has_key(std::string_view key) const = 0;
    virtual void remove_key(std::string_view key) = 0;
    virtual void clear() = 0;

    // Convenience setters
    virtual void set_bool(std::string_view key, bool value) = 0;
    virtual void set_int(std::string_view key, int value) = 0;
    virtual void set_float(std::string_view key, float value) = 0;
    virtual void set_string(std::string_view key, std::string_view value) = 0;
    virtual void set_vec3(std::string_view key, const void_math::Vec3& value) = 0;

    // Convenience getters
    virtual bool get_bool(std::string_view key, bool default_value = false) const = 0;
    virtual int get_int(std::string_view key, int default_value = 0) const = 0;
    virtual float get_float(std::string_view key, float default_value = 0) const = 0;
    virtual std::string get_string(std::string_view key, std::string_view default_value = "") const = 0;
    virtual void_math::Vec3 get_vec3(std::string_view key, const void_math::Vec3& default_value = {}) const = 0;

    // Observation
    using ChangeCallback = std::function<void(std::string_view key, const BlackboardValue& value)>;
    virtual void observe(std::string_view key, ChangeCallback callback) = 0;
    virtual void unobserve(std::string_view key) = 0;

    // Scoping
    virtual IBlackboard* parent() const = 0;
    virtual void set_parent(IBlackboard* parent) = 0;

    // Serialization
    virtual std::vector<std::pair<std::string, BlackboardValue>> get_all() const = 0;
    virtual void merge(const IBlackboard& other) = 0;
};

// =============================================================================
// Blackboard Implementation
// =============================================================================

/// @brief Standard blackboard implementation
class Blackboard : public IBlackboard {
public:
    Blackboard() = default;
    explicit Blackboard(IBlackboard* parent);
    ~Blackboard() override = default;

    // IBlackboard interface
    void set_value(std::string_view key, const BlackboardValue& value) override;
    bool get_value(std::string_view key, BlackboardValue& out_value) const override;
    bool has_key(std::string_view key) const override;
    void remove_key(std::string_view key) override;
    void clear() override;

    void set_bool(std::string_view key, bool value) override;
    void set_int(std::string_view key, int value) override;
    void set_float(std::string_view key, float value) override;
    void set_string(std::string_view key, std::string_view value) override;
    void set_vec3(std::string_view key, const void_math::Vec3& value) override;

    bool get_bool(std::string_view key, bool default_value = false) const override;
    int get_int(std::string_view key, int default_value = 0) const override;
    float get_float(std::string_view key, float default_value = 0) const override;
    std::string get_string(std::string_view key, std::string_view default_value = "") const override;
    void_math::Vec3 get_vec3(std::string_view key, const void_math::Vec3& default_value = {}) const override;

    void observe(std::string_view key, ChangeCallback callback) override;
    void unobserve(std::string_view key) override;

    IBlackboard* parent() const override { return m_parent; }
    void set_parent(IBlackboard* parent) override { m_parent = parent; }

    std::vector<std::pair<std::string, BlackboardValue>> get_all() const override;
    void merge(const IBlackboard& other) override;

private:
    void notify_observers(std::string_view key, const BlackboardValue& value);

    std::unordered_map<std::string, BlackboardValue> m_data;
    std::unordered_map<std::string, std::vector<ChangeCallback>> m_observers;
    IBlackboard* m_parent{nullptr};
};

// =============================================================================
// Scoped Blackboard
// =============================================================================

/// @brief RAII scope that creates a child blackboard
class BlackboardScope {
public:
    explicit BlackboardScope(IBlackboard& parent);
    ~BlackboardScope();

    BlackboardScope(const BlackboardScope&) = delete;
    BlackboardScope& operator=(const BlackboardScope&) = delete;

    IBlackboard* operator->() { return m_scoped.get(); }
    IBlackboard& operator*() { return *m_scoped; }
    IBlackboard* get() { return m_scoped.get(); }

private:
    std::unique_ptr<Blackboard> m_scoped;
};

// =============================================================================
// Template Implementations
// =============================================================================

template<typename T>
void IBlackboard::set(const BlackboardKey<T>& key, const T& value) {
    if constexpr (std::is_same_v<T, bool>) {
        set_bool(key.name, value);
    } else if constexpr (std::is_same_v<T, int>) {
        set_int(key.name, value);
    } else if constexpr (std::is_same_v<T, float>) {
        set_float(key.name, value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        set_string(key.name, value);
    } else if constexpr (std::is_same_v<T, void_math::Vec3>) {
        set_vec3(key.name, value);
    } else {
        set_value(key.name, std::any{value});
    }
}

template<typename T>
bool IBlackboard::get(const BlackboardKey<T>& key, T& out_value) const {
    if constexpr (std::is_same_v<T, bool>) {
        if (has_key(key.name)) {
            out_value = get_bool(key.name);
            return true;
        }
    } else if constexpr (std::is_same_v<T, int>) {
        if (has_key(key.name)) {
            out_value = get_int(key.name);
            return true;
        }
    } else if constexpr (std::is_same_v<T, float>) {
        if (has_key(key.name)) {
            out_value = get_float(key.name);
            return true;
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (has_key(key.name)) {
            out_value = get_string(key.name);
            return true;
        }
    } else if constexpr (std::is_same_v<T, void_math::Vec3>) {
        if (has_key(key.name)) {
            out_value = get_vec3(key.name);
            return true;
        }
    } else {
        BlackboardValue val;
        if (get_value(key.name, val)) {
            if (auto* any_val = std::get_if<std::any>(&val)) {
                if (any_val->type() == typeid(T)) {
                    out_value = std::any_cast<T>(*any_val);
                    return true;
                }
            }
        }
    }
    return false;
}

template<typename T>
T IBlackboard::get_or_default(const BlackboardKey<T>& key, const T& default_value) const {
    T value;
    if (get(key, value)) {
        return value;
    }
    return default_value;
}

template<typename T>
bool IBlackboard::has(const BlackboardKey<T>& key) const {
    return has_key(key.name);
}

template<typename T>
void IBlackboard::remove(const BlackboardKey<T>& key) {
    remove_key(key.name);
}

// =============================================================================
// Common Blackboard Keys
// =============================================================================

namespace bb_keys {
    // Target-related
    inline const BlackboardKey<bool> has_target{"has_target"};
    inline const BlackboardKey<void_math::Vec3> target_position{"target_position"};
    inline const BlackboardKey<void_math::Vec3> target_velocity{"target_velocity"};
    inline const BlackboardKey<float> target_distance{"target_distance"};
    inline const BlackboardKey<bool> can_see_target{"can_see_target"};

    // Self-related
    inline const BlackboardKey<void_math::Vec3> self_position{"self_position"};
    inline const BlackboardKey<float> health_percent{"health_percent"};
    inline const BlackboardKey<bool> is_in_combat{"is_in_combat"};

    // Movement
    inline const BlackboardKey<void_math::Vec3> move_destination{"move_destination"};
    inline const BlackboardKey<bool> path_valid{"path_valid"};
    inline const BlackboardKey<float> path_progress{"path_progress"};

    // Combat
    inline const BlackboardKey<float> last_damage_time{"last_damage_time"};
    inline const BlackboardKey<void_math::Vec3> last_damage_direction{"last_damage_direction"};
    inline const BlackboardKey<bool> weapon_ready{"weapon_ready"};

    // State
    inline const BlackboardKey<int> current_state{"current_state"};
    inline const BlackboardKey<float> state_time{"state_time"};
}

} // namespace void_ai
