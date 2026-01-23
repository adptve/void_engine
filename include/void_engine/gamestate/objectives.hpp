/// @file objectives.hpp
/// @brief Objective and quest system for void_gamestate module

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace void_gamestate {

// =============================================================================
// ObjectiveTracker
// =============================================================================

/// @brief Tracks progress on objectives
class ObjectiveTracker {
public:
    ObjectiveTracker();
    ~ObjectiveTracker();

    // Objective registration
    ObjectiveId register_objective(const ObjectiveDef& def);
    bool unregister_objective(ObjectiveId id);
    const ObjectiveDef* get_definition(ObjectiveId id) const;
    ObjectiveDef* get_definition_mut(ObjectiveId id);

    // Lookup
    ObjectiveId find(std::string_view name) const;
    bool exists(ObjectiveId id) const;

    // State management
    ObjectiveState get_state(ObjectiveId id) const;
    bool set_state(ObjectiveId id, ObjectiveState state);

    // State transitions
    bool reveal(ObjectiveId id);
    bool activate(ObjectiveId id);
    bool complete(ObjectiveId id);
    bool fail(ObjectiveId id);
    bool abandon(ObjectiveId id);
    bool reset(ObjectiveId id);

    // Progress tracking
    ObjectiveProgress get_progress(ObjectiveId id) const;
    bool increment_progress(ObjectiveId id, std::uint32_t amount = 1);
    bool set_progress(ObjectiveId id, std::uint32_t count);
    bool add_completed_step(ObjectiveId id, const std::string& step);
    bool is_complete(ObjectiveId id) const;
    float get_completion_percentage(ObjectiveId id) const;

    // Time tracking
    void update(float delta_time);
    float get_time_remaining(ObjectiveId id) const;
    bool is_timed(ObjectiveId id) const;
    bool is_expired(ObjectiveId id) const;

    // Queries
    std::vector<ObjectiveId> get_all() const;
    std::vector<ObjectiveId> get_by_state(ObjectiveState state) const;
    std::vector<ObjectiveId> get_by_type(ObjectiveType type) const;
    std::vector<ObjectiveId> get_active() const;
    std::vector<ObjectiveId> get_completed() const;
    std::vector<ObjectiveId> get_failed() const;
    std::vector<ObjectiveId> get_tracked() const;

    // Tracking
    void set_tracked(ObjectiveId id, bool tracked);
    bool is_tracked(ObjectiveId id) const;
    ObjectiveId get_primary_tracked() const;
    void set_primary_tracked(ObjectiveId id);

    // Prerequisites
    bool check_prerequisites(ObjectiveId id) const;
    std::vector<ObjectiveId> get_available_objectives() const;

    // Callbacks
    void set_on_state_change(ObjectiveCallback callback) { m_on_state_change = std::move(callback); }
    void set_on_progress(std::function<void(ObjectiveId, std::uint32_t, std::uint32_t)> callback) {
        m_on_progress = std::move(callback);
    }

    // Serialization
    struct SerializedObjective {
        std::uint64_t id;
        std::uint8_t state;
        std::uint32_t current_count;
        float time_elapsed;
        double started_time;
        double completed_time;
        std::vector<std::string> completed_steps;
        bool tracked;
    };

    std::vector<SerializedObjective> serialize() const;
    void deserialize(const std::vector<SerializedObjective>& data);

    // Clear
    void clear();
    void reset_all();

    // Time
    void set_current_time(double time) { m_current_time = time; }

private:
    void notify_state_change(ObjectiveId id, ObjectiveState old_state, ObjectiveState new_state);
    void notify_progress(ObjectiveId id, std::uint32_t old_count, std::uint32_t new_count);
    void check_auto_complete(ObjectiveId id);

    std::unordered_map<ObjectiveId, ObjectiveDef> m_definitions;
    std::unordered_map<ObjectiveId, ObjectiveProgress> m_progress;
    std::unordered_map<std::string, ObjectiveId> m_name_lookup;
    std::unordered_set<ObjectiveId> m_tracked;
    ObjectiveId m_primary_tracked;

    ObjectiveCallback m_on_state_change;
    std::function<void(ObjectiveId, std::uint32_t, std::uint32_t)> m_on_progress;

    double m_current_time{0};
    std::uint64_t m_next_id{1};
};

// =============================================================================
// Quest
// =============================================================================

/// @brief A quest containing multiple objectives
struct Quest {
    std::uint64_t id{0};
    std::string name;
    std::string description;
    std::string giver_name;
    std::string completion_text;
    std::string failure_text;

    // Type
    bool is_main_quest{false};
    bool is_repeatable{false};
    bool is_hidden{false};

    // Requirements
    std::uint32_t required_level{0};
    std::vector<std::uint64_t> required_quests;
    std::vector<std::string> required_items;

    // Objectives
    std::vector<ObjectiveId> objectives;
    bool sequential{false}; // Must complete in order

    // Rewards
    std::uint32_t experience_reward{0};
    std::uint32_t currency_reward{0};
    std::vector<std::string> item_rewards;
    std::string reward_description;

    // Time
    float time_limit{0}; // 0 = no limit
    double accepted_time{0};
    double completed_time{0};

    // State
    ObjectiveState state{ObjectiveState::Hidden};

    // Metadata
    std::string category;
    std::vector<std::string> tags;
    std::string icon_path;
    std::unordered_map<std::string, std::string> custom_data;
};

// =============================================================================
// QuestSystem
// =============================================================================

/// @brief Manages quests and their objectives
class QuestSystem {
public:
    QuestSystem();
    explicit QuestSystem(ObjectiveTracker* tracker);
    ~QuestSystem();

    // Configuration
    void set_objective_tracker(ObjectiveTracker* tracker) { m_tracker = tracker; }
    ObjectiveTracker* tracker() { return m_tracker; }
    const ObjectiveTracker* tracker() const { return m_tracker; }

    // Quest registration
    std::uint64_t register_quest(const Quest& quest);
    bool unregister_quest(std::uint64_t id);
    const Quest* get_quest(std::uint64_t id) const;
    Quest* get_quest_mut(std::uint64_t id);

    // Lookup
    std::uint64_t find_quest(std::string_view name) const;
    bool quest_exists(std::uint64_t id) const;

    // Quest state
    ObjectiveState get_quest_state(std::uint64_t id) const;
    bool accept_quest(std::uint64_t id);
    bool abandon_quest(std::uint64_t id);
    bool complete_quest(std::uint64_t id);
    bool fail_quest(std::uint64_t id);

    // Progress
    float get_quest_progress(std::uint64_t id) const;
    std::uint32_t get_completed_objectives(std::uint64_t id) const;
    std::uint32_t get_total_objectives(std::uint64_t id) const;
    ObjectiveId get_current_objective(std::uint64_t id) const;

    // Queries
    std::vector<std::uint64_t> get_all_quests() const;
    std::vector<std::uint64_t> get_quests_by_state(ObjectiveState state) const;
    std::vector<std::uint64_t> get_available_quests() const;
    std::vector<std::uint64_t> get_active_quests() const;
    std::vector<std::uint64_t> get_completed_quests() const;
    std::vector<std::uint64_t> get_failed_quests() const;
    std::vector<std::uint64_t> get_main_quests() const;
    std::vector<std::uint64_t> get_side_quests() const;
    std::vector<std::uint64_t> get_quests_by_category(const std::string& category) const;
    std::vector<std::uint64_t> get_quests_by_giver(const std::string& giver) const;

    // Requirements
    bool can_accept_quest(std::uint64_t id) const;
    bool meets_level_requirement(std::uint64_t id, std::uint32_t player_level) const;
    bool has_required_quests(std::uint64_t id) const;
    bool has_required_items(std::uint64_t id, const std::vector<std::string>& inventory) const;

    // Tracking
    void set_tracked_quest(std::uint64_t id);
    std::uint64_t get_tracked_quest() const { return m_tracked_quest; }
    void clear_tracked_quest();

    // Update
    void update(float delta_time);

    // Callbacks
    void set_on_quest_accepted(std::function<void(std::uint64_t)> callback) {
        m_on_accepted = std::move(callback);
    }
    void set_on_quest_completed(std::function<void(std::uint64_t)> callback) {
        m_on_completed = std::move(callback);
    }
    void set_on_quest_failed(std::function<void(std::uint64_t)> callback) {
        m_on_failed = std::move(callback);
    }
    void set_on_quest_abandoned(std::function<void(std::uint64_t)> callback) {
        m_on_abandoned = std::move(callback);
    }
    void set_on_objective_completed(std::function<void(std::uint64_t, ObjectiveId)> callback) {
        m_on_objective_completed = std::move(callback);
    }

    // Serialization
    struct SerializedQuest {
        std::uint64_t id;
        std::uint8_t state;
        double accepted_time;
        double completed_time;
    };

    std::vector<SerializedQuest> serialize() const;
    void deserialize(const std::vector<SerializedQuest>& data);

    // Clear
    void clear();

    // Time
    void set_current_time(double time) { m_current_time = time; }
    void set_player_level(std::uint32_t level) { m_player_level = level; }

private:
    void update_quest_state(std::uint64_t id);
    void on_objective_state_changed(const ObjectiveEvent& event);

    ObjectiveTracker* m_tracker{nullptr};
    std::unordered_map<std::uint64_t, Quest> m_quests;
    std::unordered_map<std::string, std::uint64_t> m_name_lookup;
    std::unordered_map<ObjectiveId, std::uint64_t> m_objective_to_quest;

    std::uint64_t m_tracked_quest{0};
    double m_current_time{0};
    std::uint32_t m_player_level{1};
    std::uint64_t m_next_id{1};

    std::function<void(std::uint64_t)> m_on_accepted;
    std::function<void(std::uint64_t)> m_on_completed;
    std::function<void(std::uint64_t)> m_on_failed;
    std::function<void(std::uint64_t)> m_on_abandoned;
    std::function<void(std::uint64_t, ObjectiveId)> m_on_objective_completed;
};

// =============================================================================
// ObjectiveBuilder
// =============================================================================

/// @brief Fluent builder for objectives
class ObjectiveBuilder {
public:
    ObjectiveBuilder() = default;

    ObjectiveBuilder& name(const std::string& name) {
        m_def.name = name;
        return *this;
    }

    ObjectiveBuilder& description(const std::string& desc) {
        m_def.description = desc;
        return *this;
    }

    ObjectiveBuilder& hint(const std::string& hint) {
        m_def.hint = hint;
        return *this;
    }

    ObjectiveBuilder& type(ObjectiveType type) {
        m_def.type = type;
        return *this;
    }

    ObjectiveBuilder& primary() {
        m_def.type = ObjectiveType::Primary;
        return *this;
    }

    ObjectiveBuilder& secondary() {
        m_def.type = ObjectiveType::Secondary;
        return *this;
    }

    ObjectiveBuilder& optional() {
        m_def.type = ObjectiveType::Optional;
        return *this;
    }

    ObjectiveBuilder& hidden() {
        m_def.type = ObjectiveType::Hidden;
        return *this;
    }

    ObjectiveBuilder& timed(float seconds) {
        m_def.type = ObjectiveType::Timed;
        m_def.time_limit = seconds;
        return *this;
    }

    ObjectiveBuilder& repeatable() {
        m_def.type = ObjectiveType::Repeatable;
        return *this;
    }

    ObjectiveBuilder& count(std::uint32_t required) {
        m_def.required_count = required;
        return *this;
    }

    ObjectiveBuilder& trackable(bool value = true) {
        m_def.trackable = value;
        return *this;
    }

    ObjectiveBuilder& prerequisite(ObjectiveId id) {
        m_def.prerequisites.push_back(id);
        return *this;
    }

    ObjectiveBuilder& conflicts_with(ObjectiveId id) {
        m_def.conflicts.push_back(id);
        return *this;
    }

    ObjectiveBuilder& reward(const std::string& desc) {
        m_def.reward_description = desc;
        return *this;
    }

    ObjectiveBuilder& icon(const std::string& path) {
        m_def.icon_path = path;
        return *this;
    }

    ObjectiveBuilder& marker(const std::string& path) {
        m_def.marker_path = path;
        return *this;
    }

    ObjectiveBuilder& target_position(const Vec3& pos) {
        m_def.target_position = pos;
        return *this;
    }

    ObjectiveBuilder& target_entity(EntityId entity) {
        m_def.target_entity = entity;
        return *this;
    }

    ObjectiveDef build() const { return m_def; }

private:
    ObjectiveDef m_def;
};

// =============================================================================
// QuestBuilder
// =============================================================================

/// @brief Fluent builder for quests
class QuestBuilder {
public:
    QuestBuilder() = default;

    QuestBuilder& name(const std::string& name) {
        m_quest.name = name;
        return *this;
    }

    QuestBuilder& description(const std::string& desc) {
        m_quest.description = desc;
        return *this;
    }

    QuestBuilder& giver(const std::string& name) {
        m_quest.giver_name = name;
        return *this;
    }

    QuestBuilder& completion_text(const std::string& text) {
        m_quest.completion_text = text;
        return *this;
    }

    QuestBuilder& failure_text(const std::string& text) {
        m_quest.failure_text = text;
        return *this;
    }

    QuestBuilder& main_quest(bool value = true) {
        m_quest.is_main_quest = value;
        return *this;
    }

    QuestBuilder& repeatable(bool value = true) {
        m_quest.is_repeatable = value;
        return *this;
    }

    QuestBuilder& hidden(bool value = true) {
        m_quest.is_hidden = value;
        return *this;
    }

    QuestBuilder& required_level(std::uint32_t level) {
        m_quest.required_level = level;
        return *this;
    }

    QuestBuilder& requires_quest(std::uint64_t quest_id) {
        m_quest.required_quests.push_back(quest_id);
        return *this;
    }

    QuestBuilder& requires_item(const std::string& item) {
        m_quest.required_items.push_back(item);
        return *this;
    }

    QuestBuilder& objective(ObjectiveId id) {
        m_quest.objectives.push_back(id);
        return *this;
    }

    QuestBuilder& sequential(bool value = true) {
        m_quest.sequential = value;
        return *this;
    }

    QuestBuilder& experience_reward(std::uint32_t xp) {
        m_quest.experience_reward = xp;
        return *this;
    }

    QuestBuilder& currency_reward(std::uint32_t amount) {
        m_quest.currency_reward = amount;
        return *this;
    }

    QuestBuilder& item_reward(const std::string& item) {
        m_quest.item_rewards.push_back(item);
        return *this;
    }

    QuestBuilder& reward_description(const std::string& desc) {
        m_quest.reward_description = desc;
        return *this;
    }

    QuestBuilder& time_limit(float seconds) {
        m_quest.time_limit = seconds;
        return *this;
    }

    QuestBuilder& category(const std::string& cat) {
        m_quest.category = cat;
        return *this;
    }

    QuestBuilder& tag(const std::string& tag) {
        m_quest.tags.push_back(tag);
        return *this;
    }

    QuestBuilder& icon(const std::string& path) {
        m_quest.icon_path = path;
        return *this;
    }

    QuestBuilder& custom_data(const std::string& key, const std::string& value) {
        m_quest.custom_data[key] = value;
        return *this;
    }

    Quest build() const { return m_quest; }

private:
    Quest m_quest;
};

} // namespace void_gamestate
