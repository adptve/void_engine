/// @file conditions.cpp
/// @brief Condition system implementation for void_triggers module

#include <void_engine/triggers/conditions.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace void_triggers {

// =============================================================================
// ConditionGroup Implementation
// =============================================================================

ConditionGroup::ConditionGroup() = default;

ConditionGroup::ConditionGroup(LogicalOp op)
    : m_operator(op) {
}

ConditionGroup::~ConditionGroup() = default;

bool ConditionGroup::evaluate(const TriggerEvent& event) const {
    if (m_conditions.empty()) {
        return true;
    }

    switch (m_operator) {
        case LogicalOp::And:
            for (const auto& cond : m_conditions) {
                if (!cond->evaluate(event)) {
                    return false;
                }
            }
            return true;

        case LogicalOp::Or:
            for (const auto& cond : m_conditions) {
                if (cond->evaluate(event)) {
                    return true;
                }
            }
            return false;

        case LogicalOp::Not:
            if (!m_conditions.empty()) {
                return !m_conditions[0]->evaluate(event);
            }
            return true;

        case LogicalOp::Xor: {
            int true_count = 0;
            for (const auto& cond : m_conditions) {
                if (cond->evaluate(event)) {
                    true_count++;
                }
            }
            return true_count == 1;
        }

        case LogicalOp::Nand:
            for (const auto& cond : m_conditions) {
                if (!cond->evaluate(event)) {
                    return true;
                }
            }
            return false;

        case LogicalOp::Nor:
            for (const auto& cond : m_conditions) {
                if (cond->evaluate(event)) {
                    return false;
                }
            }
            return true;
    }

    return false;
}

std::string ConditionGroup::description() const {
    std::string op_str;
    switch (m_operator) {
        case LogicalOp::And: op_str = "AND"; break;
        case LogicalOp::Or: op_str = "OR"; break;
        case LogicalOp::Not: op_str = "NOT"; break;
        case LogicalOp::Xor: op_str = "XOR"; break;
        case LogicalOp::Nand: op_str = "NAND"; break;
        case LogicalOp::Nor: op_str = "NOR"; break;
    }
    return "Group(" + op_str + ", " + std::to_string(m_conditions.size()) + " conditions)";
}

std::unique_ptr<ICondition> ConditionGroup::clone() const {
    auto result = std::make_unique<ConditionGroup>(m_operator);
    for (const auto& cond : m_conditions) {
        result->add(cond->clone());
    }
    return result;
}

void ConditionGroup::add(std::unique_ptr<ICondition> condition) {
    m_conditions.push_back(std::move(condition));
}

void ConditionGroup::add_all(std::vector<std::unique_ptr<ICondition>> conditions) {
    for (auto& cond : conditions) {
        m_conditions.push_back(std::move(cond));
    }
}

void ConditionGroup::clear() {
    m_conditions.clear();
}

// =============================================================================
// VariableCondition Implementation
// =============================================================================

VariableCondition::VariableCondition() = default;

VariableCondition::VariableCondition(const std::string& variable, CompareOp op, const VariableValue& value)
    : m_variable(variable)
    , m_operator(op)
    , m_value(value) {
}

VariableCondition::~VariableCondition() = default;

bool VariableCondition::evaluate(const TriggerEvent& /*event*/) const {
    if (!m_getter) {
        return false;
    }

    VariableValue current = m_getter(m_variable);
    return current.compare(m_value, m_operator);
}

std::string VariableCondition::description() const {
    std::string op_str;
    switch (m_operator) {
        case CompareOp::Equal: op_str = "=="; break;
        case CompareOp::NotEqual: op_str = "!="; break;
        case CompareOp::Less: op_str = "<"; break;
        case CompareOp::LessEqual: op_str = "<="; break;
        case CompareOp::Greater: op_str = ">"; break;
        case CompareOp::GreaterEqual: op_str = ">="; break;
        case CompareOp::Contains: op_str = "contains"; break;
        case CompareOp::NotContains: op_str = "!contains"; break;
    }
    return m_variable + " " + op_str + " ?";
}

std::unique_ptr<ICondition> VariableCondition::clone() const {
    auto result = std::make_unique<VariableCondition>(m_variable, m_operator, m_value);
    result->set_variable_getter(m_getter);
    return result;
}

// =============================================================================
// EntityCondition Implementation
// =============================================================================

EntityCondition::EntityCondition() = default;

EntityCondition::EntityCondition(Property property, const std::string& param)
    : m_property(property)
    , m_parameter(param) {
}

EntityCondition::~EntityCondition() = default;

bool EntityCondition::evaluate(const TriggerEvent& event) const {
    if (!m_getter) {
        return false;
    }

    EntityId entity = m_use_event_entity ? event.entity : m_entity;
    bool result = m_getter(entity, m_property, m_parameter);
    return m_inverted ? !result : result;
}

std::string EntityCondition::description() const {
    std::string prop_str;
    switch (m_property) {
        case Property::Exists: prop_str = "Exists"; break;
        case Property::Alive: prop_str = "Alive"; break;
        case Property::HasTag: prop_str = "HasTag(" + m_parameter + ")"; break;
        case Property::HasComponent: prop_str = "HasComponent(" + m_parameter + ")"; break;
        case Property::InZone: prop_str = "InZone(" + m_parameter + ")"; break;
        case Property::Custom: prop_str = "Custom"; break;
    }
    return "Entity " + prop_str;
}

std::unique_ptr<ICondition> EntityCondition::clone() const {
    auto result = std::make_unique<EntityCondition>(m_property, m_parameter);
    result->set_entity(m_entity);
    result->set_use_event_entity(m_use_event_entity);
    result->set_property_getter(m_getter);
    result->set_inverted(m_inverted);
    return result;
}

// =============================================================================
// TimerCondition Implementation
// =============================================================================

TimerCondition::TimerCondition() = default;

TimerCondition::TimerCondition(float threshold, CompareOp op, TimeSource source)
    : m_threshold(threshold)
    , m_operator(op)
    , m_source(source) {
}

TimerCondition::~TimerCondition() = default;

bool TimerCondition::evaluate(const TriggerEvent& event) const {
    double current_time;

    if (m_time_getter) {
        current_time = m_time_getter(m_source);
    } else if (m_source == TimeSource::EventTime) {
        current_time = event.timestamp;
    } else {
        return false;
    }

    VariableValue current(static_cast<float>(current_time));
    VariableValue threshold(m_threshold);
    return current.compare(threshold, m_operator);
}

std::string TimerCondition::description() const {
    std::string source_str;
    switch (m_source) {
        case TimeSource::GameTime: source_str = "GameTime"; break;
        case TimeSource::RealTime: source_str = "RealTime"; break;
        case TimeSource::TriggerTime: source_str = "TriggerTime"; break;
        case TimeSource::EventTime: source_str = "EventTime"; break;
    }
    return "Timer(" + source_str + " ? " + std::to_string(m_threshold) + ")";
}

std::unique_ptr<ICondition> TimerCondition::clone() const {
    auto result = std::make_unique<TimerCondition>(m_threshold, m_operator, m_source);
    result->set_time_getter(m_time_getter);
    return result;
}

// =============================================================================
// CountCondition Implementation
// =============================================================================

CountCondition::CountCondition() = default;

CountCondition::CountCondition(std::uint32_t threshold, CompareOp op)
    : m_threshold(threshold)
    , m_operator(op) {
}

CountCondition::~CountCondition() = default;

bool CountCondition::evaluate(const TriggerEvent& event) const {
    if (!m_count_getter) {
        return false;
    }

    std::uint32_t count = m_count_getter(event.trigger);
    VariableValue current(static_cast<int>(count));
    VariableValue threshold(static_cast<int>(m_threshold));
    return current.compare(threshold, m_operator);
}

std::string CountCondition::description() const {
    return "Count ? " + std::to_string(m_threshold);
}

std::unique_ptr<ICondition> CountCondition::clone() const {
    auto result = std::make_unique<CountCondition>(m_threshold, m_operator);
    result->set_count_getter(m_count_getter);
    return result;
}

// =============================================================================
// RandomCondition Implementation
// =============================================================================

RandomCondition::RandomCondition() = default;

RandomCondition::RandomCondition(float probability)
    : m_probability(probability) {
}

RandomCondition::~RandomCondition() = default;

bool RandomCondition::evaluate(const TriggerEvent& /*event*/) const {
    // Simple xorshift random
    m_state ^= m_state << 13;
    m_state ^= m_state >> 17;
    m_state ^= m_state << 5;
    float random = static_cast<float>(m_state & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
    return random <= m_probability;
}

std::string RandomCondition::description() const {
    return "Random(" + std::to_string(static_cast<int>(m_probability * 100)) + "%)";
}

std::unique_ptr<ICondition> RandomCondition::clone() const {
    auto result = std::make_unique<RandomCondition>(m_probability);
    result->set_seed(m_state);
    return result;
}

void RandomCondition::set_seed(std::uint64_t seed) {
    m_state = seed;
}

// =============================================================================
// DistanceCondition Implementation
// =============================================================================

DistanceCondition::DistanceCondition() = default;

DistanceCondition::DistanceCondition(float threshold, CompareOp op)
    : m_threshold(threshold)
    , m_operator(op) {
}

DistanceCondition::~DistanceCondition() = default;

bool DistanceCondition::evaluate(const TriggerEvent& event) const {
    Vec3 entity_pos = event.position;

    Vec3 target_pos;
    if (m_use_entity && m_position_getter) {
        target_pos = m_position_getter(m_target_entity);
    } else {
        target_pos = m_target_point;
    }

    float dx = entity_pos.x - target_pos.x;
    float dy = m_use_2d ? 0 : entity_pos.y - target_pos.y;
    float dz = entity_pos.z - target_pos.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    VariableValue current(distance);
    VariableValue threshold(m_threshold);
    return current.compare(threshold, m_operator);
}

std::string DistanceCondition::description() const {
    return "Distance ? " + std::to_string(m_threshold);
}

std::unique_ptr<ICondition> DistanceCondition::clone() const {
    auto result = std::make_unique<DistanceCondition>(m_threshold, m_operator);
    if (m_use_entity) {
        result->set_target_entity(m_target_entity);
    } else {
        result->set_target_point(m_target_point);
    }
    result->set_use_2d(m_use_2d);
    result->set_position_getter(m_position_getter);
    return result;
}

// =============================================================================
// TagCondition Implementation
// =============================================================================

TagCondition::TagCondition() = default;

TagCondition::TagCondition(const std::string& tag, bool require_all)
    : m_tags({tag})
    , m_require_all(require_all) {
}

TagCondition::TagCondition(const std::vector<std::string>& tags, bool require_all)
    : m_tags(tags)
    , m_require_all(require_all) {
}

TagCondition::~TagCondition() = default;

bool TagCondition::evaluate(const TriggerEvent& event) const {
    if (!m_tags_getter) {
        return false;
    }

    std::vector<std::string> entity_tags = m_tags_getter(event.entity);

    bool result;
    if (m_require_all) {
        result = true;
        for (const auto& tag : m_tags) {
            bool found = std::find(entity_tags.begin(), entity_tags.end(), tag) != entity_tags.end();
            if (!found) {
                result = false;
                break;
            }
        }
    } else {
        result = false;
        for (const auto& tag : m_tags) {
            bool found = std::find(entity_tags.begin(), entity_tags.end(), tag) != entity_tags.end();
            if (found) {
                result = true;
                break;
            }
        }
    }

    return m_inverted ? !result : result;
}

std::string TagCondition::description() const {
    std::string tags_str;
    for (std::size_t i = 0; i < m_tags.size(); ++i) {
        if (i > 0) tags_str += ", ";
        tags_str += m_tags[i];
    }
    return "Tags(" + tags_str + ")";
}

std::unique_ptr<ICondition> TagCondition::clone() const {
    auto result = std::make_unique<TagCondition>(m_tags, m_require_all);
    result->set_inverted(m_inverted);
    result->set_tags_getter(m_tags_getter);
    return result;
}

// =============================================================================
// CallbackCondition Implementation
// =============================================================================

CallbackCondition::CallbackCondition() = default;

CallbackCondition::CallbackCondition(ConditionCallback callback, const std::string& desc)
    : m_callback(std::move(callback))
    , m_description(desc) {
}

CallbackCondition::~CallbackCondition() = default;

bool CallbackCondition::evaluate(const TriggerEvent& event) const {
    if (m_callback) {
        return m_callback(event);
    }
    return false;
}

std::string CallbackCondition::description() const {
    return m_description;
}

std::unique_ptr<ICondition> CallbackCondition::clone() const {
    return std::make_unique<CallbackCondition>(m_callback, m_description);
}

// =============================================================================
// ConditionBuilder Implementation
// =============================================================================

std::unique_ptr<VariableCondition> ConditionBuilder::variable(const std::string& name) {
    auto cond = std::make_unique<VariableCondition>();
    cond->set_variable(name);
    return cond;
}

std::unique_ptr<EntityCondition> ConditionBuilder::entity(EntityCondition::Property prop) {
    return std::make_unique<EntityCondition>(prop);
}

std::unique_ptr<TimerCondition> ConditionBuilder::timer(float seconds) {
    return std::make_unique<TimerCondition>(seconds, CompareOp::GreaterEqual);
}

std::unique_ptr<CountCondition> ConditionBuilder::count(std::uint32_t threshold) {
    return std::make_unique<CountCondition>(threshold, CompareOp::Less);
}

std::unique_ptr<RandomCondition> ConditionBuilder::random(float probability) {
    return std::make_unique<RandomCondition>(probability);
}

std::unique_ptr<DistanceCondition> ConditionBuilder::distance(float threshold) {
    return std::make_unique<DistanceCondition>(threshold, CompareOp::LessEqual);
}

std::unique_ptr<TagCondition> ConditionBuilder::tag(const std::string& tag) {
    return std::make_unique<TagCondition>(tag);
}

std::unique_ptr<ConditionGroup> ConditionBuilder::all() {
    return std::make_unique<ConditionGroup>(LogicalOp::And);
}

std::unique_ptr<ConditionGroup> ConditionBuilder::any() {
    return std::make_unique<ConditionGroup>(LogicalOp::Or);
}

std::unique_ptr<CallbackCondition> ConditionBuilder::custom(ConditionCallback callback) {
    return std::make_unique<CallbackCondition>(std::move(callback));
}

} // namespace void_triggers
