/// @file variables.hpp
/// @brief Game variable system for void_gamestate module

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_gamestate {

// =============================================================================
// VariableStore
// =============================================================================

/// @brief Storage for game variables
class VariableStore {
public:
    VariableStore();
    ~VariableStore();

    // Registration
    /// @brief Register a new variable
    VariableId register_variable(const GameVariable& var);

    /// @brief Unregister a variable
    bool unregister_variable(VariableId id);

    /// @brief Get variable definition
    const GameVariable* get_variable(VariableId id) const;
    GameVariable* get_variable_mut(VariableId id);

    /// @brief Find variable by name
    VariableId find(std::string_view name) const;

    /// @brief Check if variable exists
    bool exists(VariableId id) const;
    bool exists(std::string_view name) const;

    // Value access
    /// @brief Get variable value
    VariableValue get(VariableId id) const;
    VariableValue get(std::string_view name) const;

    /// @brief Set variable value
    bool set(VariableId id, const VariableValue& value);
    bool set(std::string_view name, const VariableValue& value);

    // Type-safe access
    bool get_bool(VariableId id, bool default_value = false) const;
    int get_int(VariableId id, int default_value = 0) const;
    float get_float(VariableId id, float default_value = 0) const;
    std::string get_string(VariableId id, const std::string& default_value = "") const;

    void set_bool(VariableId id, bool value);
    void set_int(VariableId id, int value);
    void set_float(VariableId id, float value);
    void set_string(VariableId id, const std::string& value);

    // Convenience
    bool get_bool(std::string_view name, bool default_value = false) const;
    int get_int(std::string_view name, int default_value = 0) const;
    float get_float(std::string_view name, float default_value = 0) const;
    std::string get_string(std::string_view name, const std::string& default_value = "") const;

    void set_bool(std::string_view name, bool value);
    void set_int(std::string_view name, int value);
    void set_float(std::string_view name, float value);
    void set_string(std::string_view name, const std::string& value);

    // Queries
    /// @brief Get all variables
    std::vector<VariableId> all_variables() const;

    /// @brief Get variables by scope
    std::vector<VariableId> get_by_scope(VariableScope scope) const;

    /// @brief Get variables by type
    std::vector<VariableId> get_by_type(VariableType type) const;

    /// @brief Get variables by category
    std::vector<VariableId> get_by_category(const std::string& category) const;

    /// @brief Get variables by tag
    std::vector<VariableId> get_by_tag(const std::string& tag) const;

    /// @brief Get persistent variables
    std::vector<VariableId> get_persistent() const;

    // Batch operations
    /// @brief Reset all variables to defaults
    void reset_all();

    /// @brief Reset variables by scope
    void reset_scope(VariableScope scope);

    /// @brief Clear all variables
    void clear();

    /// @brief Get variable count
    std::size_t count() const { return m_variables.size(); }

    // Change tracking
    /// @brief Set change callback
    void set_on_change(VariableChangeCallback callback) { m_on_change = std::move(callback); }

    /// @brief Get change history for variable
    std::vector<VariableChangeEvent> get_history(VariableId id) const;

    /// @brief Clear change history
    void clear_history();

    /// @brief Enable/disable history tracking
    void set_track_history(bool track) { m_track_history = track; }

    // Serialization
    struct SerializedVariable {
        std::uint64_t id;
        std::string name;
        std::uint8_t type;
        std::uint8_t scope;
        std::vector<std::uint8_t> value_data;
    };

    std::vector<SerializedVariable> serialize() const;
    void deserialize(const std::vector<SerializedVariable>& data);

    // Time
    void set_current_time(double time) { m_current_time = time; }

private:
    void notify_change(VariableId id, const VariableValue& old_value, const VariableValue& new_value);
    std::vector<std::uint8_t> serialize_value(const VariableValue& value) const;
    VariableValue deserialize_value(VariableType type, const std::vector<std::uint8_t>& data) const;

    std::unordered_map<VariableId, GameVariable> m_variables;
    std::unordered_map<std::string, VariableId> m_name_lookup;
    std::unordered_map<VariableId, std::vector<VariableChangeEvent>> m_history;

    VariableChangeCallback m_on_change;
    bool m_track_history{true};
    double m_current_time{0};
    std::uint64_t m_next_id{1};
};

// =============================================================================
// GlobalVariables
// =============================================================================

/// @brief Singleton-like access to global variables
class GlobalVariables {
public:
    GlobalVariables();
    explicit GlobalVariables(VariableStore* store);
    ~GlobalVariables();

    // Quick access
    static GlobalVariables& instance();

    // Store access
    VariableStore& store() { return *m_store; }
    const VariableStore& store() const { return *m_store; }

    // Shorthand access
    VariableValue operator[](std::string_view name) const;

    template<typename T>
    T get(std::string_view name, const T& default_value = T{}) const;

    template<typename T>
    void set(std::string_view name, const T& value);

    // Quick registration helpers
    VariableId register_bool(const std::string& name, bool default_value);
    VariableId register_int(const std::string& name, int default_value, int min = 0, int max = 0);
    VariableId register_float(const std::string& name, float default_value, float min = 0, float max = 0);
    VariableId register_string(const std::string& name, const std::string& default_value);

private:
    VariableStore* m_store{nullptr};
    std::unique_ptr<VariableStore> m_owned_store;
    static GlobalVariables* s_instance;
};

// =============================================================================
// EntityVariables
// =============================================================================

/// @brief Per-entity variable storage
class EntityVariables {
public:
    EntityVariables();
    ~EntityVariables();

    // Entity management
    /// @brief Create variable store for entity
    void create_entity(EntityId entity);

    /// @brief Remove entity and its variables
    void remove_entity(EntityId entity);

    /// @brief Check if entity has variables
    bool has_entity(EntityId entity) const;

    // Variable access
    VariableValue get(EntityId entity, std::string_view name) const;
    void set(EntityId entity, std::string_view name, const VariableValue& value);

    // Type-safe access
    bool get_bool(EntityId entity, std::string_view name, bool default_value = false) const;
    int get_int(EntityId entity, std::string_view name, int default_value = 0) const;
    float get_float(EntityId entity, std::string_view name, float default_value = 0) const;
    std::string get_string(EntityId entity, std::string_view name, const std::string& default_value = "") const;

    void set_bool(EntityId entity, std::string_view name, bool value);
    void set_int(EntityId entity, std::string_view name, int value);
    void set_float(EntityId entity, std::string_view name, float value);
    void set_string(EntityId entity, std::string_view name, const std::string& value);

    // Registration with entity
    void register_variable(EntityId entity, const std::string& name, const VariableValue& default_value);

    // Bulk operations
    std::unordered_map<std::string, VariableValue> get_all(EntityId entity) const;
    void set_all(EntityId entity, const std::unordered_map<std::string, VariableValue>& values);

    // Clear
    void clear_entity(EntityId entity);
    void clear_all();

    // Serialization
    struct SerializedEntityVars {
        std::uint64_t entity_id;
        std::unordered_map<std::string, std::vector<std::uint8_t>> variables;
    };

    std::vector<SerializedEntityVars> serialize() const;
    void deserialize(const std::vector<SerializedEntityVars>& data);

private:
    std::unordered_map<EntityId, std::unordered_map<std::string, VariableValue>> m_entity_vars;
};

// =============================================================================
// VariableExpression
// =============================================================================

/// @brief Evaluates expressions with variables
class VariableExpression {
public:
    VariableExpression();
    explicit VariableExpression(const std::string& expression);
    ~VariableExpression();

    /// @brief Set expression
    void set_expression(const std::string& expression);

    /// @brief Evaluate expression
    VariableValue evaluate(VariableStore* store) const;

    /// @brief Evaluate to specific type
    bool evaluate_bool(VariableStore* store) const;
    int evaluate_int(VariableStore* store) const;
    float evaluate_float(VariableStore* store) const;
    std::string evaluate_string(VariableStore* store) const;

    /// @brief Check if expression is valid
    bool is_valid() const { return m_valid; }

    /// @brief Get error message
    const std::string& error() const { return m_error; }

    /// @brief Get referenced variables
    std::vector<std::string> referenced_variables() const;

private:
    enum class TokenType {
        Number,
        String,
        Variable,
        Operator,
        LeftParen,
        RightParen,
        Comma,
        End
    };

    struct Token {
        TokenType type;
        std::string value;
    };

    void parse();
    VariableValue evaluate_node(VariableStore* store, std::size_t& pos) const;

    std::string m_expression;
    std::vector<Token> m_tokens;
    std::vector<std::string> m_referenced;
    bool m_valid{false};
    std::string m_error;
};

// =============================================================================
// Template implementations
// =============================================================================

template<typename T>
T GlobalVariables::get(std::string_view name, const T& default_value) const {
    VariableValue value = m_store->get(name);
    if (!value.value.has_value()) {
        return default_value;
    }

    if constexpr (std::is_same_v<T, bool>) {
        return value.as_bool();
    } else if constexpr (std::is_same_v<T, int>) {
        return value.as_int();
    } else if constexpr (std::is_same_v<T, float>) {
        return value.as_float();
    } else if constexpr (std::is_same_v<T, std::string>) {
        return value.as_string();
    } else {
        return default_value;
    }
}

template<typename T>
void GlobalVariables::set(std::string_view name, const T& value) {
    m_store->set(name, VariableValue(value));
}

} // namespace void_gamestate
