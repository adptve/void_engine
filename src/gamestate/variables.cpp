/// @file variables.cpp
/// @brief Implementation of variable system for void_gamestate module

#include "void_engine/gamestate/variables.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace void_gamestate {

// =============================================================================
// VariableStore
// =============================================================================

VariableStore::VariableStore() = default;
VariableStore::~VariableStore() = default;

VariableId VariableStore::register_variable(const GameVariable& var) {
    // Check for name conflicts
    if (m_name_lookup.contains(var.name)) {
        return m_name_lookup[var.name];
    }

    VariableId id{m_next_id++};
    GameVariable new_var = var;
    new_var.id = id;
    new_var.current_value = var.default_value;
    new_var.last_modified = m_current_time;

    m_variables[id] = std::move(new_var);
    m_name_lookup[var.name] = id;

    return id;
}

bool VariableStore::unregister_variable(VariableId id) {
    auto it = m_variables.find(id);
    if (it == m_variables.end()) return false;

    m_name_lookup.erase(it->second.name);
    m_history.erase(id);
    m_variables.erase(it);
    return true;
}

const GameVariable* VariableStore::get_variable(VariableId id) const {
    auto it = m_variables.find(id);
    return it != m_variables.end() ? &it->second : nullptr;
}

GameVariable* VariableStore::get_variable_mut(VariableId id) {
    auto it = m_variables.find(id);
    return it != m_variables.end() ? &it->second : nullptr;
}

VariableId VariableStore::find(std::string_view name) const {
    std::string name_str(name);
    auto it = m_name_lookup.find(name_str);
    return it != m_name_lookup.end() ? it->second : VariableId{};
}

bool VariableStore::exists(VariableId id) const {
    return m_variables.contains(id);
}

bool VariableStore::exists(std::string_view name) const {
    return m_name_lookup.contains(std::string(name));
}

VariableValue VariableStore::get(VariableId id) const {
    auto var = get_variable(id);
    return var ? var->current_value : VariableValue{};
}

VariableValue VariableStore::get(std::string_view name) const {
    return get(find(name));
}

bool VariableStore::set(VariableId id, const VariableValue& value) {
    auto* var = get_variable_mut(id);
    if (!var) return false;

    // Type checking
    if (var->type != value.type) {
        // Allow some implicit conversions
        VariableValue converted = value;
        converted.type = var->type;

        switch (var->type) {
            case VariableType::Bool:
                converted.value = value.as_bool();
                break;
            case VariableType::Int:
                converted.value = value.as_int();
                break;
            case VariableType::Float:
                converted.value = value.as_float();
                break;
            case VariableType::String:
                converted.value = value.as_string();
                break;
            default:
                return false;
        }

        return set(id, converted);
    }

    // Constraint checking
    if (var->type == VariableType::Int || var->type == VariableType::Float) {
        float val = value.as_float();
        if (var->has_min && val < var->min_value) return false;
        if (var->has_max && val > var->max_value) return false;
    }

    if (var->type == VariableType::String && !var->allowed_values.empty()) {
        std::string str_val = value.as_string();
        bool found = false;
        for (const auto& allowed : var->allowed_values) {
            if (allowed == str_val) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    VariableValue old_value = var->current_value;
    var->current_value = value;
    var->last_modified = m_current_time;

    notify_change(id, old_value, value);
    return true;
}

bool VariableStore::set(std::string_view name, const VariableValue& value) {
    return set(find(name), value);
}

bool VariableStore::get_bool(VariableId id, bool default_value) const {
    auto var = get_variable(id);
    return var ? var->current_value.as_bool() : default_value;
}

int VariableStore::get_int(VariableId id, int default_value) const {
    auto var = get_variable(id);
    return var ? var->current_value.as_int() : default_value;
}

float VariableStore::get_float(VariableId id, float default_value) const {
    auto var = get_variable(id);
    return var ? var->current_value.as_float() : default_value;
}

std::string VariableStore::get_string(VariableId id, const std::string& default_value) const {
    auto var = get_variable(id);
    return var ? var->current_value.as_string() : default_value;
}

void VariableStore::set_bool(VariableId id, bool value) {
    set(id, VariableValue(value));
}

void VariableStore::set_int(VariableId id, int value) {
    set(id, VariableValue(value));
}

void VariableStore::set_float(VariableId id, float value) {
    set(id, VariableValue(value));
}

void VariableStore::set_string(VariableId id, const std::string& value) {
    set(id, VariableValue(value));
}

bool VariableStore::get_bool(std::string_view name, bool default_value) const {
    return get_bool(find(name), default_value);
}

int VariableStore::get_int(std::string_view name, int default_value) const {
    return get_int(find(name), default_value);
}

float VariableStore::get_float(std::string_view name, float default_value) const {
    return get_float(find(name), default_value);
}

std::string VariableStore::get_string(std::string_view name, const std::string& default_value) const {
    return get_string(find(name), default_value);
}

void VariableStore::set_bool(std::string_view name, bool value) {
    set(find(name), VariableValue(value));
}

void VariableStore::set_int(std::string_view name, int value) {
    set(find(name), VariableValue(value));
}

void VariableStore::set_float(std::string_view name, float value) {
    set(find(name), VariableValue(value));
}

void VariableStore::set_string(std::string_view name, const std::string& value) {
    set(find(name), VariableValue(value));
}

std::vector<VariableId> VariableStore::all_variables() const {
    std::vector<VariableId> result;
    result.reserve(m_variables.size());
    for (const auto& [id, _] : m_variables) {
        result.push_back(id);
    }
    return result;
}

std::vector<VariableId> VariableStore::get_by_scope(VariableScope scope) const {
    std::vector<VariableId> result;
    for (const auto& [id, var] : m_variables) {
        if (var.scope == scope) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<VariableId> VariableStore::get_by_type(VariableType type) const {
    std::vector<VariableId> result;
    for (const auto& [id, var] : m_variables) {
        if (var.type == type) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<VariableId> VariableStore::get_by_category(const std::string& category) const {
    std::vector<VariableId> result;
    for (const auto& [id, var] : m_variables) {
        if (var.category == category) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<VariableId> VariableStore::get_by_tag(const std::string& tag) const {
    std::vector<VariableId> result;
    for (const auto& [id, var] : m_variables) {
        for (const auto& t : var.tags) {
            if (t == tag) {
                result.push_back(id);
                break;
            }
        }
    }
    return result;
}

std::vector<VariableId> VariableStore::get_persistent() const {
    std::vector<VariableId> result;
    for (const auto& [id, var] : m_variables) {
        if (has_flag(var.persistence, PersistenceFlags::SaveToFile)) {
            result.push_back(id);
        }
    }
    return result;
}

void VariableStore::reset_all() {
    for (auto& [id, var] : m_variables) {
        VariableValue old_value = var.current_value;
        var.current_value = var.default_value;
        var.last_modified = m_current_time;
        notify_change(id, old_value, var.current_value);
    }
}

void VariableStore::reset_scope(VariableScope scope) {
    for (auto& [id, var] : m_variables) {
        if (var.scope == scope) {
            VariableValue old_value = var.current_value;
            var.current_value = var.default_value;
            var.last_modified = m_current_time;
            notify_change(id, old_value, var.current_value);
        }
    }
}

void VariableStore::clear() {
    m_variables.clear();
    m_name_lookup.clear();
    m_history.clear();
}

void VariableStore::notify_change(VariableId id, const VariableValue& old_value, const VariableValue& new_value) {
    auto* var = get_variable(id);
    if (!var) return;

    VariableChangeEvent event{
        .variable = id,
        .name = var->name,
        .old_value = old_value,
        .new_value = new_value,
        .timestamp = m_current_time,
        .source_entity = EntityId{}
    };

    if (m_track_history) {
        m_history[id].push_back(event);
    }

    if (m_on_change) {
        m_on_change(event);
    }
}

std::vector<VariableChangeEvent> VariableStore::get_history(VariableId id) const {
    auto it = m_history.find(id);
    return it != m_history.end() ? it->second : std::vector<VariableChangeEvent>{};
}

void VariableStore::clear_history() {
    m_history.clear();
}

std::vector<std::uint8_t> VariableStore::serialize_value(const VariableValue& value) const {
    std::vector<std::uint8_t> data;

    switch (value.type) {
        case VariableType::Bool: {
            data.push_back(value.as_bool() ? 1 : 0);
            break;
        }
        case VariableType::Int: {
            int val = value.as_int();
            data.resize(sizeof(int));
            std::memcpy(data.data(), &val, sizeof(int));
            break;
        }
        case VariableType::Float: {
            float val = value.as_float();
            data.resize(sizeof(float));
            std::memcpy(data.data(), &val, sizeof(float));
            break;
        }
        case VariableType::String: {
            std::string str = value.as_string();
            std::uint32_t len = static_cast<std::uint32_t>(str.size());
            data.resize(sizeof(std::uint32_t) + len);
            std::memcpy(data.data(), &len, sizeof(std::uint32_t));
            std::memcpy(data.data() + sizeof(std::uint32_t), str.data(), len);
            break;
        }
        case VariableType::Vector3: {
            Vec3 val = value.as_vector3();
            data.resize(sizeof(Vec3));
            std::memcpy(data.data(), &val, sizeof(Vec3));
            break;
        }
        case VariableType::Color: {
            Color val = value.as_color();
            data.resize(sizeof(Color));
            std::memcpy(data.data(), &val, sizeof(Color));
            break;
        }
        case VariableType::EntityRef: {
            EntityId val = value.as_entity();
            data.resize(sizeof(std::uint64_t));
            std::memcpy(data.data(), &val.value, sizeof(std::uint64_t));
            break;
        }
        default:
            break;
    }

    return data;
}

VariableValue VariableStore::deserialize_value(VariableType type, const std::vector<std::uint8_t>& data) const {
    VariableValue value;
    value.type = type;

    switch (type) {
        case VariableType::Bool: {
            if (data.size() >= 1) {
                value.value = data[0] != 0;
            }
            break;
        }
        case VariableType::Int: {
            if (data.size() >= sizeof(int)) {
                int val;
                std::memcpy(&val, data.data(), sizeof(int));
                value.value = val;
            }
            break;
        }
        case VariableType::Float: {
            if (data.size() >= sizeof(float)) {
                float val;
                std::memcpy(&val, data.data(), sizeof(float));
                value.value = val;
            }
            break;
        }
        case VariableType::String: {
            if (data.size() >= sizeof(std::uint32_t)) {
                std::uint32_t len;
                std::memcpy(&len, data.data(), sizeof(std::uint32_t));
                if (data.size() >= sizeof(std::uint32_t) + len) {
                    std::string str(reinterpret_cast<const char*>(data.data() + sizeof(std::uint32_t)), len);
                    value.value = std::move(str);
                }
            }
            break;
        }
        case VariableType::Vector3: {
            if (data.size() >= sizeof(Vec3)) {
                Vec3 val;
                std::memcpy(&val, data.data(), sizeof(Vec3));
                value.value = val;
            }
            break;
        }
        case VariableType::Color: {
            if (data.size() >= sizeof(Color)) {
                Color val;
                std::memcpy(&val, data.data(), sizeof(Color));
                value.value = val;
            }
            break;
        }
        case VariableType::EntityRef: {
            if (data.size() >= sizeof(std::uint64_t)) {
                std::uint64_t val;
                std::memcpy(&val, data.data(), sizeof(std::uint64_t));
                value.value = EntityId{val};
            }
            break;
        }
        default:
            break;
    }

    return value;
}

std::vector<VariableStore::SerializedVariable> VariableStore::serialize() const {
    std::vector<SerializedVariable> result;
    result.reserve(m_variables.size());

    for (const auto& [id, var] : m_variables) {
        if (!has_flag(var.persistence, PersistenceFlags::SaveToFile)) continue;

        SerializedVariable sv{
            .id = id.value,
            .name = var.name,
            .type = static_cast<std::uint8_t>(var.type),
            .scope = static_cast<std::uint8_t>(var.scope),
            .value_data = serialize_value(var.current_value)
        };
        result.push_back(std::move(sv));
    }

    return result;
}

void VariableStore::deserialize(const std::vector<SerializedVariable>& data) {
    for (const auto& sv : data) {
        VariableId id{sv.id};

        // Find by name first
        auto it = m_name_lookup.find(sv.name);
        if (it != m_name_lookup.end()) {
            id = it->second;
        }

        auto* var = get_variable_mut(id);
        if (var) {
            auto type = static_cast<VariableType>(sv.type);
            var->current_value = deserialize_value(type, sv.value_data);
            var->last_modified = m_current_time;
        }
    }
}

// =============================================================================
// GlobalVariables
// =============================================================================

GlobalVariables* GlobalVariables::s_instance = nullptr;

GlobalVariables::GlobalVariables()
    : m_owned_store(std::make_unique<VariableStore>())
    , m_store(m_owned_store.get()) {
    if (!s_instance) {
        s_instance = this;
    }
}

GlobalVariables::GlobalVariables(VariableStore* store)
    : m_store(store) {
    if (!s_instance) {
        s_instance = this;
    }
}

GlobalVariables::~GlobalVariables() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

GlobalVariables& GlobalVariables::instance() {
    if (!s_instance) {
        static GlobalVariables default_instance;
    }
    return *s_instance;
}

VariableValue GlobalVariables::operator[](std::string_view name) const {
    return m_store->get(name);
}

VariableId GlobalVariables::register_bool(const std::string& name, bool default_value) {
    GameVariable var{
        .name = name,
        .type = VariableType::Bool,
        .scope = VariableScope::Global,
        .default_value = VariableValue(default_value)
    };
    return m_store->register_variable(var);
}

VariableId GlobalVariables::register_int(const std::string& name, int default_value, int min, int max) {
    GameVariable var{
        .name = name,
        .type = VariableType::Int,
        .scope = VariableScope::Global,
        .default_value = VariableValue(default_value),
        .has_min = (min != max || min != 0),
        .has_max = (min != max || max != 0),
        .min_value = static_cast<float>(min),
        .max_value = static_cast<float>(max)
    };
    return m_store->register_variable(var);
}

VariableId GlobalVariables::register_float(const std::string& name, float default_value, float min, float max) {
    GameVariable var{
        .name = name,
        .type = VariableType::Float,
        .scope = VariableScope::Global,
        .default_value = VariableValue(default_value),
        .has_min = (min != max || min != 0),
        .has_max = (min != max || max != 0),
        .min_value = min,
        .max_value = max
    };
    return m_store->register_variable(var);
}

VariableId GlobalVariables::register_string(const std::string& name, const std::string& default_value) {
    GameVariable var{
        .name = name,
        .type = VariableType::String,
        .scope = VariableScope::Global,
        .default_value = VariableValue(default_value)
    };
    return m_store->register_variable(var);
}

// =============================================================================
// EntityVariables
// =============================================================================

EntityVariables::EntityVariables() = default;
EntityVariables::~EntityVariables() = default;

void EntityVariables::create_entity(EntityId entity) {
    if (!m_entity_vars.contains(entity)) {
        m_entity_vars[entity] = {};
    }
}

void EntityVariables::remove_entity(EntityId entity) {
    m_entity_vars.erase(entity);
}

bool EntityVariables::has_entity(EntityId entity) const {
    return m_entity_vars.contains(entity);
}

VariableValue EntityVariables::get(EntityId entity, std::string_view name) const {
    auto it = m_entity_vars.find(entity);
    if (it == m_entity_vars.end()) return VariableValue{};

    std::string name_str(name);
    auto var_it = it->second.find(name_str);
    return var_it != it->second.end() ? var_it->second : VariableValue{};
}

void EntityVariables::set(EntityId entity, std::string_view name, const VariableValue& value) {
    create_entity(entity);
    m_entity_vars[entity][std::string(name)] = value;
}

bool EntityVariables::get_bool(EntityId entity, std::string_view name, bool default_value) const {
    auto val = get(entity, name);
    return val.value.has_value() ? val.as_bool() : default_value;
}

int EntityVariables::get_int(EntityId entity, std::string_view name, int default_value) const {
    auto val = get(entity, name);
    return val.value.has_value() ? val.as_int() : default_value;
}

float EntityVariables::get_float(EntityId entity, std::string_view name, float default_value) const {
    auto val = get(entity, name);
    return val.value.has_value() ? val.as_float() : default_value;
}

std::string EntityVariables::get_string(EntityId entity, std::string_view name, const std::string& default_value) const {
    auto val = get(entity, name);
    return val.value.has_value() ? val.as_string() : default_value;
}

void EntityVariables::set_bool(EntityId entity, std::string_view name, bool value) {
    set(entity, name, VariableValue(value));
}

void EntityVariables::set_int(EntityId entity, std::string_view name, int value) {
    set(entity, name, VariableValue(value));
}

void EntityVariables::set_float(EntityId entity, std::string_view name, float value) {
    set(entity, name, VariableValue(value));
}

void EntityVariables::set_string(EntityId entity, std::string_view name, const std::string& value) {
    set(entity, name, VariableValue(value));
}

void EntityVariables::register_variable(EntityId entity, const std::string& name, const VariableValue& default_value) {
    create_entity(entity);
    if (!m_entity_vars[entity].contains(name)) {
        m_entity_vars[entity][name] = default_value;
    }
}

std::unordered_map<std::string, VariableValue> EntityVariables::get_all(EntityId entity) const {
    auto it = m_entity_vars.find(entity);
    return it != m_entity_vars.end() ? it->second : std::unordered_map<std::string, VariableValue>{};
}

void EntityVariables::set_all(EntityId entity, const std::unordered_map<std::string, VariableValue>& values) {
    m_entity_vars[entity] = values;
}

void EntityVariables::clear_entity(EntityId entity) {
    auto it = m_entity_vars.find(entity);
    if (it != m_entity_vars.end()) {
        it->second.clear();
    }
}

void EntityVariables::clear_all() {
    m_entity_vars.clear();
}

std::vector<EntityVariables::SerializedEntityVars> EntityVariables::serialize() const {
    std::vector<SerializedEntityVars> result;
    // Implementation would serialize entity variables
    return result;
}

void EntityVariables::deserialize(const std::vector<SerializedEntityVars>& data) {
    // Implementation would deserialize entity variables
}

// =============================================================================
// VariableExpression
// =============================================================================

VariableExpression::VariableExpression() = default;

VariableExpression::VariableExpression(const std::string& expression)
    : m_expression(expression) {
    parse();
}

VariableExpression::~VariableExpression() = default;

void VariableExpression::set_expression(const std::string& expression) {
    m_expression = expression;
    m_tokens.clear();
    m_referenced.clear();
    m_valid = false;
    m_error.clear();
    parse();
}

void VariableExpression::parse() {
    m_tokens.clear();
    m_referenced.clear();
    m_valid = true;
    m_error.clear();

    if (m_expression.empty()) {
        m_valid = false;
        m_error = "Empty expression";
        return;
    }

    std::size_t pos = 0;
    while (pos < m_expression.size()) {
        char c = m_expression[pos];

        // Skip whitespace
        if (std::isspace(c)) {
            ++pos;
            continue;
        }

        // Number
        if (std::isdigit(c) || (c == '.' && pos + 1 < m_expression.size() && std::isdigit(m_expression[pos + 1]))) {
            std::size_t start = pos;
            while (pos < m_expression.size() && (std::isdigit(m_expression[pos]) || m_expression[pos] == '.')) {
                ++pos;
            }
            m_tokens.push_back({TokenType::Number, m_expression.substr(start, pos - start)});
            continue;
        }

        // String literal
        if (c == '"' || c == '\'') {
            char quote = c;
            std::size_t start = ++pos;
            while (pos < m_expression.size() && m_expression[pos] != quote) {
                if (m_expression[pos] == '\\' && pos + 1 < m_expression.size()) {
                    ++pos;
                }
                ++pos;
            }
            if (pos >= m_expression.size()) {
                m_valid = false;
                m_error = "Unterminated string";
                return;
            }
            m_tokens.push_back({TokenType::String, m_expression.substr(start, pos - start)});
            ++pos;
            continue;
        }

        // Variable (starts with $)
        if (c == '$') {
            std::size_t start = ++pos;
            while (pos < m_expression.size() && (std::isalnum(m_expression[pos]) || m_expression[pos] == '_')) {
                ++pos;
            }
            std::string var_name = m_expression.substr(start, pos - start);
            m_tokens.push_back({TokenType::Variable, var_name});
            m_referenced.push_back(var_name);
            continue;
        }

        // Operators
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '<' || c == '>' || c == '=' || c == '!' || c == '&' || c == '|') {
            std::string op(1, c);
            ++pos;
            // Check for two-character operators
            if (pos < m_expression.size()) {
                char next = m_expression[pos];
                if ((c == '=' && next == '=') || (c == '!' && next == '=') ||
                    (c == '<' && next == '=') || (c == '>' && next == '=') ||
                    (c == '&' && next == '&') || (c == '|' && next == '|')) {
                    op += next;
                    ++pos;
                }
            }
            m_tokens.push_back({TokenType::Operator, op});
            continue;
        }

        // Parentheses
        if (c == '(') {
            m_tokens.push_back({TokenType::LeftParen, "("});
            ++pos;
            continue;
        }
        if (c == ')') {
            m_tokens.push_back({TokenType::RightParen, ")"});
            ++pos;
            continue;
        }

        // Comma
        if (c == ',') {
            m_tokens.push_back({TokenType::Comma, ","});
            ++pos;
            continue;
        }

        // Identifier (for functions or boolean literals)
        if (std::isalpha(c) || c == '_') {
            std::size_t start = pos;
            while (pos < m_expression.size() && (std::isalnum(m_expression[pos]) || m_expression[pos] == '_')) {
                ++pos;
            }
            std::string ident = m_expression.substr(start, pos - start);
            if (ident == "true" || ident == "false") {
                m_tokens.push_back({TokenType::Number, ident == "true" ? "1" : "0"});
            } else {
                // Treat as variable without $
                m_tokens.push_back({TokenType::Variable, ident});
                m_referenced.push_back(ident);
            }
            continue;
        }

        // Unknown character
        m_valid = false;
        m_error = "Unexpected character: " + std::string(1, c);
        return;
    }

    m_tokens.push_back({TokenType::End, ""});
}

VariableValue VariableExpression::evaluate(VariableStore* store) const {
    if (!m_valid || !store) {
        return VariableValue{};
    }

    // Simple expression evaluator
    // For now, handle single variable or literal
    if (m_tokens.size() == 2) { // Token + End
        const auto& token = m_tokens[0];
        switch (token.type) {
            case TokenType::Number: {
                if (token.value.find('.') != std::string::npos) {
                    return VariableValue(std::stof(token.value));
                }
                return VariableValue(std::stoi(token.value));
            }
            case TokenType::String:
                return VariableValue(token.value);
            case TokenType::Variable:
                return store->get(token.value);
            default:
                break;
        }
    }

    // More complex expressions would need proper parsing
    return VariableValue{};
}

bool VariableExpression::evaluate_bool(VariableStore* store) const {
    return evaluate(store).as_bool();
}

int VariableExpression::evaluate_int(VariableStore* store) const {
    return evaluate(store).as_int();
}

float VariableExpression::evaluate_float(VariableStore* store) const {
    return evaluate(store).as_float();
}

std::string VariableExpression::evaluate_string(VariableStore* store) const {
    return evaluate(store).as_string();
}

std::vector<std::string> VariableExpression::referenced_variables() const {
    return m_referenced;
}

VariableValue VariableExpression::evaluate_node(VariableStore* store, std::size_t& pos) const {
    // Implementation for complex expression evaluation
    return VariableValue{};
}

} // namespace void_gamestate
