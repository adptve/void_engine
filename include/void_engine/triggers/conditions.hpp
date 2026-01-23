/// @file conditions.hpp
/// @brief Condition system for void_triggers module

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace void_triggers {

// =============================================================================
// ICondition Interface
// =============================================================================

/// @brief Interface for trigger conditions
class ICondition {
public:
    virtual ~ICondition() = default;

    /// @brief Evaluate the condition
    virtual bool evaluate(const TriggerEvent& event) const = 0;

    /// @brief Get condition description
    virtual std::string description() const = 0;

    /// @brief Clone the condition
    virtual std::unique_ptr<ICondition> clone() const = 0;
};

// =============================================================================
// ConditionGroup
// =============================================================================

/// @brief Group of conditions combined with logical operator
class ConditionGroup : public ICondition {
public:
    ConditionGroup();
    explicit ConditionGroup(LogicalOp op);
    ~ConditionGroup() override;

    bool evaluate(const TriggerEvent& event) const override;
    std::string description() const override;
    std::unique_ptr<ICondition> clone() const override;

    /// @brief Add a condition to the group
    void add(std::unique_ptr<ICondition> condition);

    /// @brief Add multiple conditions
    void add_all(std::vector<std::unique_ptr<ICondition>> conditions);

    /// @brief Clear all conditions
    void clear();

    /// @brief Get condition count
    std::size_t count() const { return m_conditions.size(); }

    /// @brief Set logical operator
    void set_operator(LogicalOp op) { m_operator = op; }
    LogicalOp get_operator() const { return m_operator; }

    // Fluent builder
    ConditionGroup& with_op(LogicalOp op) { m_operator = op; return *this; }
    ConditionGroup& with(std::unique_ptr<ICondition> condition) {
        add(std::move(condition));
        return *this;
    }

private:
    std::vector<std::unique_ptr<ICondition>> m_conditions;
    LogicalOp m_operator{LogicalOp::And};
};

// =============================================================================
// VariableCondition
// =============================================================================

/// @brief Condition based on variable comparison
class VariableCondition : public ICondition {
public:
    using VariableGetter = std::function<VariableValue(const std::string&)>;

    VariableCondition();
    VariableCondition(const std::string& variable, CompareOp op, const VariableValue& value);
    ~VariableCondition() override;

    bool evaluate(const TriggerEvent& event) const override;
    std::string description() const override;
    std::unique_ptr<ICondition> clone() const override;

    void set_variable(const std::string& name) { m_variable = name; }
    void set_operator(CompareOp op) { m_operator = op; }
    void set_value(const VariableValue& value) { m_value = value; }
    void set_variable_getter(VariableGetter getter) { m_getter = std::move(getter); }

    // Fluent builder
    VariableCondition& var(const std::string& name) { m_variable = name; return *this; }
    VariableCondition& op(CompareOp o) { m_operator = o; return *this; }
    VariableCondition& val(const VariableValue& v) { m_value = v; return *this; }

private:
    std::string m_variable;
    CompareOp m_operator{CompareOp::Equal};
    VariableValue m_value;
    VariableGetter m_getter;
};

// =============================================================================
// EntityCondition
// =============================================================================

/// @brief Condition based on entity properties
class EntityCondition : public ICondition {
public:
    enum class Property {
        Exists,
        Alive,
        HasTag,
        HasComponent,
        InZone,
        Custom
    };

    using PropertyGetter = std::function<bool(EntityId, Property, const std::string&)>;

    EntityCondition();
    EntityCondition(Property property, const std::string& param = "");
    ~EntityCondition() override;

    bool evaluate(const TriggerEvent& event) const override;
    std::string description() const override;
    std::unique_ptr<ICondition> clone() const override;

    void set_property(Property prop) { m_property = prop; }
    void set_parameter(const std::string& param) { m_parameter = param; }
    void set_entity(EntityId entity) { m_entity = entity; }
    void set_use_event_entity(bool use) { m_use_event_entity = use; }
    void set_property_getter(PropertyGetter getter) { m_getter = std::move(getter); }
    void set_inverted(bool inverted) { m_inverted = inverted; }

private:
    Property m_property{Property::Exists};
    std::string m_parameter;
    EntityId m_entity;
    bool m_use_event_entity{true};
    bool m_inverted{false};
    PropertyGetter m_getter;
};

// =============================================================================
// TimerCondition
// =============================================================================

/// @brief Condition based on time
class TimerCondition : public ICondition {
public:
    enum class TimeSource {
        GameTime,
        RealTime,
        TriggerTime,    ///< Time since trigger created
        EventTime       ///< Time of current event
    };

    TimerCondition();
    TimerCondition(float threshold, CompareOp op, TimeSource source = TimeSource::GameTime);
    ~TimerCondition() override;

    bool evaluate(const TriggerEvent& event) const override;
    std::string description() const override;
    std::unique_ptr<ICondition> clone() const override;

    void set_threshold(float threshold) { m_threshold = threshold; }
    void set_operator(CompareOp op) { m_operator = op; }
    void set_time_source(TimeSource source) { m_source = source; }

    using TimeGetter = std::function<double(TimeSource)>;
    void set_time_getter(TimeGetter getter) { m_time_getter = std::move(getter); }

private:
    float m_threshold{0};
    CompareOp m_operator{CompareOp::GreaterEqual};
    TimeSource m_source{TimeSource::GameTime};
    TimeGetter m_time_getter;
};

// =============================================================================
// CountCondition
// =============================================================================

/// @brief Condition based on activation count
class CountCondition : public ICondition {
public:
    CountCondition();
    CountCondition(std::uint32_t threshold, CompareOp op);
    ~CountCondition() override;

    bool evaluate(const TriggerEvent& event) const override;
    std::string description() const override;
    std::unique_ptr<ICondition> clone() const override;

    void set_threshold(std::uint32_t threshold) { m_threshold = threshold; }
    void set_operator(CompareOp op) { m_operator = op; }

    using CountGetter = std::function<std::uint32_t(TriggerId)>;
    void set_count_getter(CountGetter getter) { m_count_getter = std::move(getter); }

private:
    std::uint32_t m_threshold{1};
    CompareOp m_operator{CompareOp::Less};
    CountGetter m_count_getter;
};

// =============================================================================
// RandomCondition
// =============================================================================

/// @brief Condition based on random chance
class RandomCondition : public ICondition {
public:
    RandomCondition();
    explicit RandomCondition(float probability);
    ~RandomCondition() override;

    bool evaluate(const TriggerEvent& event) const override;
    std::string description() const override;
    std::unique_ptr<ICondition> clone() const override;

    void set_probability(float prob) { m_probability = prob; }
    float probability() const { return m_probability; }

    /// @brief Set seed for deterministic results
    void set_seed(std::uint64_t seed);

private:
    float m_probability{0.5f};
    mutable std::uint64_t m_state{12345};
};

// =============================================================================
// DistanceCondition
// =============================================================================

/// @brief Condition based on distance between entities/points
class DistanceCondition : public ICondition {
public:
    DistanceCondition();
    DistanceCondition(float threshold, CompareOp op);
    ~DistanceCondition() override;

    bool evaluate(const TriggerEvent& event) const override;
    std::string description() const override;
    std::unique_ptr<ICondition> clone() const override;

    void set_threshold(float threshold) { m_threshold = threshold; }
    void set_operator(CompareOp op) { m_operator = op; }
    void set_target_point(const Vec3& point) { m_target_point = point; m_use_entity = false; }
    void set_target_entity(EntityId entity) { m_target_entity = entity; m_use_entity = true; }
    void set_use_2d(bool use_2d) { m_use_2d = use_2d; }

    void set_position_getter(EntityPositionCallback getter) { m_position_getter = std::move(getter); }

private:
    float m_threshold{5.0f};
    CompareOp m_operator{CompareOp::LessEqual};
    Vec3 m_target_point;
    EntityId m_target_entity;
    bool m_use_entity{false};
    bool m_use_2d{false};
    EntityPositionCallback m_position_getter;
};

// =============================================================================
// TagCondition
// =============================================================================

/// @brief Condition based on entity tags
class TagCondition : public ICondition {
public:
    TagCondition();
    TagCondition(const std::string& tag, bool require_all = false);
    TagCondition(const std::vector<std::string>& tags, bool require_all = false);
    ~TagCondition() override;

    bool evaluate(const TriggerEvent& event) const override;
    std::string description() const override;
    std::unique_ptr<ICondition> clone() const override;

    void set_tags(const std::vector<std::string>& tags) { m_tags = tags; }
    void add_tag(const std::string& tag) { m_tags.push_back(tag); }
    void set_require_all(bool require) { m_require_all = require; }
    void set_inverted(bool inverted) { m_inverted = inverted; }

    void set_tags_getter(EntityTagsCallback getter) { m_tags_getter = std::move(getter); }

private:
    std::vector<std::string> m_tags;
    bool m_require_all{false};
    bool m_inverted{false};
    EntityTagsCallback m_tags_getter;
};

// =============================================================================
// CallbackCondition
// =============================================================================

/// @brief Custom callback-based condition
class CallbackCondition : public ICondition {
public:
    CallbackCondition();
    explicit CallbackCondition(ConditionCallback callback, const std::string& desc = "Custom");
    ~CallbackCondition() override;

    bool evaluate(const TriggerEvent& event) const override;
    std::string description() const override;
    std::unique_ptr<ICondition> clone() const override;

    void set_callback(ConditionCallback callback) { m_callback = std::move(callback); }
    void set_description(const std::string& desc) { m_description = desc; }

private:
    ConditionCallback m_callback;
    std::string m_description{"Custom"};
};

// =============================================================================
// Condition Builder
// =============================================================================

/// @brief Fluent builder for conditions
class ConditionBuilder {
public:
    ConditionBuilder() = default;

    /// @brief Create a variable condition
    static std::unique_ptr<VariableCondition> variable(const std::string& name);

    /// @brief Create an entity condition
    static std::unique_ptr<EntityCondition> entity(EntityCondition::Property prop);

    /// @brief Create a timer condition
    static std::unique_ptr<TimerCondition> timer(float seconds);

    /// @brief Create a count condition
    static std::unique_ptr<CountCondition> count(std::uint32_t threshold);

    /// @brief Create a random condition
    static std::unique_ptr<RandomCondition> random(float probability);

    /// @brief Create a distance condition
    static std::unique_ptr<DistanceCondition> distance(float threshold);

    /// @brief Create a tag condition
    static std::unique_ptr<TagCondition> tag(const std::string& tag);

    /// @brief Create an AND group
    static std::unique_ptr<ConditionGroup> all();

    /// @brief Create an OR group
    static std::unique_ptr<ConditionGroup> any();

    /// @brief Create a custom callback condition
    static std::unique_ptr<CallbackCondition> custom(ConditionCallback callback);
};

} // namespace void_triggers
