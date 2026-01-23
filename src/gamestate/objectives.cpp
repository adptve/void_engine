/// @file objectives.cpp
/// @brief Implementation of objective and quest system for void_gamestate module

#include "void_engine/gamestate/objectives.hpp"

#include <algorithm>

namespace void_gamestate {

// =============================================================================
// ObjectiveTracker
// =============================================================================

ObjectiveTracker::ObjectiveTracker() = default;
ObjectiveTracker::~ObjectiveTracker() = default;

ObjectiveId ObjectiveTracker::register_objective(const ObjectiveDef& def) {
    // Check for name conflicts
    if (m_name_lookup.contains(def.name)) {
        return m_name_lookup[def.name];
    }

    ObjectiveId id{m_next_id++};
    ObjectiveDef new_def = def;
    new_def.id = id;

    m_definitions[id] = std::move(new_def);
    m_name_lookup[def.name] = id;

    // Initialize progress
    ObjectiveProgress progress;
    progress.objective_id = id;
    progress.state = ObjectiveState::Hidden;
    m_progress[id] = progress;

    return id;
}

bool ObjectiveTracker::unregister_objective(ObjectiveId id) {
    auto it = m_definitions.find(id);
    if (it == m_definitions.end()) return false;

    m_name_lookup.erase(it->second.name);
    m_progress.erase(id);
    m_tracked.erase(id);
    m_definitions.erase(it);

    if (m_primary_tracked == id) {
        m_primary_tracked = ObjectiveId{};
    }

    return true;
}

const ObjectiveDef* ObjectiveTracker::get_definition(ObjectiveId id) const {
    auto it = m_definitions.find(id);
    return it != m_definitions.end() ? &it->second : nullptr;
}

ObjectiveDef* ObjectiveTracker::get_definition_mut(ObjectiveId id) {
    auto it = m_definitions.find(id);
    return it != m_definitions.end() ? &it->second : nullptr;
}

ObjectiveId ObjectiveTracker::find(std::string_view name) const {
    std::string name_str(name);
    auto it = m_name_lookup.find(name_str);
    return it != m_name_lookup.end() ? it->second : ObjectiveId{};
}

bool ObjectiveTracker::exists(ObjectiveId id) const {
    return m_definitions.contains(id);
}

ObjectiveState ObjectiveTracker::get_state(ObjectiveId id) const {
    auto it = m_progress.find(id);
    return it != m_progress.end() ? it->second.state : ObjectiveState::Hidden;
}

bool ObjectiveTracker::set_state(ObjectiveId id, ObjectiveState state) {
    auto it = m_progress.find(id);
    if (it == m_progress.end()) return false;

    ObjectiveState old_state = it->second.state;
    if (old_state == state) return true;

    it->second.state = state;

    // Update timestamps
    if (state == ObjectiveState::Active && it->second.started_time == 0) {
        it->second.started_time = m_current_time;
    }
    if (state == ObjectiveState::Completed || state == ObjectiveState::Failed) {
        it->second.completed_time = m_current_time;
    }

    notify_state_change(id, old_state, state);
    return true;
}

bool ObjectiveTracker::reveal(ObjectiveId id) {
    auto state = get_state(id);
    if (state != ObjectiveState::Hidden) return false;
    return set_state(id, ObjectiveState::Inactive);
}

bool ObjectiveTracker::activate(ObjectiveId id) {
    auto state = get_state(id);
    if (state != ObjectiveState::Hidden && state != ObjectiveState::Inactive) return false;
    if (!check_prerequisites(id)) return false;
    return set_state(id, ObjectiveState::Active);
}

bool ObjectiveTracker::complete(ObjectiveId id) {
    auto state = get_state(id);
    if (state != ObjectiveState::Active) return false;
    return set_state(id, ObjectiveState::Completed);
}

bool ObjectiveTracker::fail(ObjectiveId id) {
    auto state = get_state(id);
    if (state != ObjectiveState::Active) return false;
    return set_state(id, ObjectiveState::Failed);
}

bool ObjectiveTracker::abandon(ObjectiveId id) {
    auto state = get_state(id);
    if (state != ObjectiveState::Active && state != ObjectiveState::Inactive) return false;
    return set_state(id, ObjectiveState::Abandoned);
}

bool ObjectiveTracker::reset(ObjectiveId id) {
    auto it = m_progress.find(id);
    if (it == m_progress.end()) return false;

    ObjectiveState old_state = it->second.state;
    it->second.state = ObjectiveState::Hidden;
    it->second.current_count = 0;
    it->second.time_elapsed = 0;
    it->second.started_time = 0;
    it->second.completed_time = 0;
    it->second.completed_steps.clear();

    notify_state_change(id, old_state, ObjectiveState::Hidden);
    return true;
}

ObjectiveProgress ObjectiveTracker::get_progress(ObjectiveId id) const {
    auto it = m_progress.find(id);
    return it != m_progress.end() ? it->second : ObjectiveProgress{};
}

bool ObjectiveTracker::increment_progress(ObjectiveId id, std::uint32_t amount) {
    auto it = m_progress.find(id);
    if (it == m_progress.end()) return false;
    if (it->second.state != ObjectiveState::Active) return false;

    auto def = get_definition(id);
    if (!def) return false;

    std::uint32_t old_count = it->second.current_count;
    it->second.current_count = std::min(
        it->second.current_count + amount,
        def->required_count);

    notify_progress(id, old_count, it->second.current_count);
    check_auto_complete(id);

    return true;
}

bool ObjectiveTracker::set_progress(ObjectiveId id, std::uint32_t count) {
    auto it = m_progress.find(id);
    if (it == m_progress.end()) return false;

    auto def = get_definition(id);
    if (!def) return false;

    std::uint32_t old_count = it->second.current_count;
    it->second.current_count = std::min(count, def->required_count);

    notify_progress(id, old_count, it->second.current_count);
    check_auto_complete(id);

    return true;
}

bool ObjectiveTracker::add_completed_step(ObjectiveId id, const std::string& step) {
    auto it = m_progress.find(id);
    if (it == m_progress.end()) return false;

    it->second.completed_steps.push_back(step);
    return true;
}

bool ObjectiveTracker::is_complete(ObjectiveId id) const {
    auto state = get_state(id);
    if (state == ObjectiveState::Completed) return true;

    auto def = get_definition(id);
    if (!def) return false;

    auto progress = get_progress(id);
    return progress.current_count >= def->required_count;
}

float ObjectiveTracker::get_completion_percentage(ObjectiveId id) const {
    auto def = get_definition(id);
    if (!def || def->required_count == 0) return 0.0f;

    auto progress = get_progress(id);
    return static_cast<float>(progress.current_count) / static_cast<float>(def->required_count);
}

void ObjectiveTracker::update(float delta_time) {
    for (auto& [id, progress] : m_progress) {
        if (progress.state != ObjectiveState::Active) continue;

        auto def = get_definition(id);
        if (!def) continue;

        // Update time for timed objectives
        if (def->type == ObjectiveType::Timed && def->time_limit > 0) {
            progress.time_elapsed += delta_time;

            if (progress.time_elapsed >= def->time_limit) {
                fail(id);
            }
        }
    }
}

float ObjectiveTracker::get_time_remaining(ObjectiveId id) const {
    auto def = get_definition(id);
    if (!def || def->time_limit <= 0) return -1.0f;

    auto progress = get_progress(id);
    return std::max(0.0f, def->time_limit - progress.time_elapsed);
}

bool ObjectiveTracker::is_timed(ObjectiveId id) const {
    auto def = get_definition(id);
    return def && def->type == ObjectiveType::Timed && def->time_limit > 0;
}

bool ObjectiveTracker::is_expired(ObjectiveId id) const {
    return is_timed(id) && get_time_remaining(id) <= 0;
}

std::vector<ObjectiveId> ObjectiveTracker::get_all() const {
    std::vector<ObjectiveId> result;
    result.reserve(m_definitions.size());
    for (const auto& [id, _] : m_definitions) {
        result.push_back(id);
    }
    return result;
}

std::vector<ObjectiveId> ObjectiveTracker::get_by_state(ObjectiveState state) const {
    std::vector<ObjectiveId> result;
    for (const auto& [id, progress] : m_progress) {
        if (progress.state == state) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<ObjectiveId> ObjectiveTracker::get_by_type(ObjectiveType type) const {
    std::vector<ObjectiveId> result;
    for (const auto& [id, def] : m_definitions) {
        if (def.type == type) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<ObjectiveId> ObjectiveTracker::get_active() const {
    return get_by_state(ObjectiveState::Active);
}

std::vector<ObjectiveId> ObjectiveTracker::get_completed() const {
    return get_by_state(ObjectiveState::Completed);
}

std::vector<ObjectiveId> ObjectiveTracker::get_failed() const {
    return get_by_state(ObjectiveState::Failed);
}

std::vector<ObjectiveId> ObjectiveTracker::get_tracked() const {
    return std::vector<ObjectiveId>(m_tracked.begin(), m_tracked.end());
}

void ObjectiveTracker::set_tracked(ObjectiveId id, bool tracked) {
    if (tracked) {
        m_tracked.insert(id);
    } else {
        m_tracked.erase(id);
        if (m_primary_tracked == id) {
            m_primary_tracked = ObjectiveId{};
        }
    }
}

bool ObjectiveTracker::is_tracked(ObjectiveId id) const {
    return m_tracked.contains(id);
}

ObjectiveId ObjectiveTracker::get_primary_tracked() const {
    return m_primary_tracked;
}

void ObjectiveTracker::set_primary_tracked(ObjectiveId id) {
    m_primary_tracked = id;
    if (id) {
        m_tracked.insert(id);
    }
}

bool ObjectiveTracker::check_prerequisites(ObjectiveId id) const {
    auto def = get_definition(id);
    if (!def) return false;

    for (ObjectiveId prereq : def->prerequisites) {
        if (get_state(prereq) != ObjectiveState::Completed) {
            return false;
        }
    }

    // Check conflicts
    for (ObjectiveId conflict : def->conflicts) {
        auto state = get_state(conflict);
        if (state == ObjectiveState::Active || state == ObjectiveState::Completed) {
            return false;
        }
    }

    return true;
}

std::vector<ObjectiveId> ObjectiveTracker::get_available_objectives() const {
    std::vector<ObjectiveId> result;

    for (const auto& [id, def] : m_definitions) {
        auto state = get_state(id);
        if (state == ObjectiveState::Hidden || state == ObjectiveState::Inactive) {
            if (check_prerequisites(id)) {
                result.push_back(id);
            }
        }
    }

    return result;
}

void ObjectiveTracker::notify_state_change(ObjectiveId id, ObjectiveState old_state, ObjectiveState new_state) {
    if (m_on_state_change) {
        ObjectiveEvent event{
            .objective = id,
            .old_state = old_state,
            .new_state = new_state,
            .timestamp = m_current_time
        };
        m_on_state_change(event);
    }
}

void ObjectiveTracker::notify_progress(ObjectiveId id, std::uint32_t old_count, std::uint32_t new_count) {
    if (m_on_progress && old_count != new_count) {
        m_on_progress(id, old_count, new_count);
    }
}

void ObjectiveTracker::check_auto_complete(ObjectiveId id) {
    auto def = get_definition(id);
    if (!def) return;

    auto progress = get_progress(id);
    if (progress.current_count >= def->required_count) {
        complete(id);
    }
}

std::vector<ObjectiveTracker::SerializedObjective> ObjectiveTracker::serialize() const {
    std::vector<SerializedObjective> result;
    result.reserve(m_progress.size());

    for (const auto& [id, progress] : m_progress) {
        SerializedObjective so{
            .id = id.value,
            .state = static_cast<std::uint8_t>(progress.state),
            .current_count = progress.current_count,
            .time_elapsed = progress.time_elapsed,
            .started_time = progress.started_time,
            .completed_time = progress.completed_time,
            .completed_steps = progress.completed_steps,
            .tracked = m_tracked.contains(id)
        };
        result.push_back(std::move(so));
    }

    return result;
}

void ObjectiveTracker::deserialize(const std::vector<SerializedObjective>& data) {
    for (const auto& so : data) {
        ObjectiveId id{so.id};
        auto it = m_progress.find(id);
        if (it != m_progress.end()) {
            it->second.state = static_cast<ObjectiveState>(so.state);
            it->second.current_count = so.current_count;
            it->second.time_elapsed = so.time_elapsed;
            it->second.started_time = so.started_time;
            it->second.completed_time = so.completed_time;
            it->second.completed_steps = so.completed_steps;

            if (so.tracked) {
                m_tracked.insert(id);
            }
        }
    }
}

void ObjectiveTracker::clear() {
    m_definitions.clear();
    m_progress.clear();
    m_name_lookup.clear();
    m_tracked.clear();
    m_primary_tracked = ObjectiveId{};
}

void ObjectiveTracker::reset_all() {
    for (auto& [id, progress] : m_progress) {
        progress.state = ObjectiveState::Hidden;
        progress.current_count = 0;
        progress.time_elapsed = 0;
        progress.started_time = 0;
        progress.completed_time = 0;
        progress.completed_steps.clear();
    }
    m_tracked.clear();
    m_primary_tracked = ObjectiveId{};
}

// =============================================================================
// QuestSystem
// =============================================================================

QuestSystem::QuestSystem() = default;

QuestSystem::QuestSystem(ObjectiveTracker* tracker)
    : m_tracker(tracker) {
    if (m_tracker) {
        m_tracker->set_on_state_change([this](const ObjectiveEvent& e) {
            on_objective_state_changed(e);
        });
    }
}

QuestSystem::~QuestSystem() = default;

std::uint64_t QuestSystem::register_quest(const Quest& quest) {
    // Check for name conflicts
    if (m_name_lookup.contains(quest.name)) {
        return m_name_lookup[quest.name];
    }

    std::uint64_t id = m_next_id++;
    Quest new_quest = quest;
    new_quest.id = id;

    m_quests[id] = std::move(new_quest);
    m_name_lookup[quest.name] = id;

    // Map objectives to this quest
    for (ObjectiveId obj_id : quest.objectives) {
        m_objective_to_quest[obj_id] = id;
    }

    return id;
}

bool QuestSystem::unregister_quest(std::uint64_t id) {
    auto it = m_quests.find(id);
    if (it == m_quests.end()) return false;

    // Remove objective mappings
    for (ObjectiveId obj_id : it->second.objectives) {
        m_objective_to_quest.erase(obj_id);
    }

    m_name_lookup.erase(it->second.name);
    m_quests.erase(it);

    if (m_tracked_quest == id) {
        m_tracked_quest = 0;
    }

    return true;
}

const Quest* QuestSystem::get_quest(std::uint64_t id) const {
    auto it = m_quests.find(id);
    return it != m_quests.end() ? &it->second : nullptr;
}

Quest* QuestSystem::get_quest_mut(std::uint64_t id) {
    auto it = m_quests.find(id);
    return it != m_quests.end() ? &it->second : nullptr;
}

std::uint64_t QuestSystem::find_quest(std::string_view name) const {
    std::string name_str(name);
    auto it = m_name_lookup.find(name_str);
    return it != m_name_lookup.end() ? it->second : 0;
}

bool QuestSystem::quest_exists(std::uint64_t id) const {
    return m_quests.contains(id);
}

ObjectiveState QuestSystem::get_quest_state(std::uint64_t id) const {
    auto quest = get_quest(id);
    return quest ? quest->state : ObjectiveState::Hidden;
}

bool QuestSystem::accept_quest(std::uint64_t id) {
    auto quest = get_quest_mut(id);
    if (!quest) return false;

    if (quest->state != ObjectiveState::Hidden && quest->state != ObjectiveState::Inactive) {
        return false;
    }

    if (!can_accept_quest(id)) {
        return false;
    }

    quest->state = ObjectiveState::Active;
    quest->accepted_time = m_current_time;

    // Activate first objective (or all if not sequential)
    if (m_tracker) {
        if (quest->sequential && !quest->objectives.empty()) {
            m_tracker->activate(quest->objectives[0]);
        } else {
            for (ObjectiveId obj_id : quest->objectives) {
                m_tracker->activate(obj_id);
            }
        }
    }

    if (m_on_accepted) {
        m_on_accepted(id);
    }

    return true;
}

bool QuestSystem::abandon_quest(std::uint64_t id) {
    auto quest = get_quest_mut(id);
    if (!quest) return false;

    if (quest->state != ObjectiveState::Active) {
        return false;
    }

    quest->state = ObjectiveState::Abandoned;

    // Abandon all objectives
    if (m_tracker) {
        for (ObjectiveId obj_id : quest->objectives) {
            m_tracker->abandon(obj_id);
        }
    }

    if (m_on_abandoned) {
        m_on_abandoned(id);
    }

    return true;
}

bool QuestSystem::complete_quest(std::uint64_t id) {
    auto quest = get_quest_mut(id);
    if (!quest) return false;

    if (quest->state != ObjectiveState::Active) {
        return false;
    }

    quest->state = ObjectiveState::Completed;
    quest->completed_time = m_current_time;

    if (m_on_completed) {
        m_on_completed(id);
    }

    return true;
}

bool QuestSystem::fail_quest(std::uint64_t id) {
    auto quest = get_quest_mut(id);
    if (!quest) return false;

    if (quest->state != ObjectiveState::Active) {
        return false;
    }

    quest->state = ObjectiveState::Failed;
    quest->completed_time = m_current_time;

    if (m_on_failed) {
        m_on_failed(id);
    }

    return true;
}

float QuestSystem::get_quest_progress(std::uint64_t id) const {
    auto quest = get_quest(id);
    if (!quest || quest->objectives.empty()) return 0.0f;

    std::uint32_t completed = get_completed_objectives(id);
    return static_cast<float>(completed) / static_cast<float>(quest->objectives.size());
}

std::uint32_t QuestSystem::get_completed_objectives(std::uint64_t id) const {
    auto quest = get_quest(id);
    if (!quest || !m_tracker) return 0;

    std::uint32_t count = 0;
    for (ObjectiveId obj_id : quest->objectives) {
        if (m_tracker->get_state(obj_id) == ObjectiveState::Completed) {
            ++count;
        }
    }
    return count;
}

std::uint32_t QuestSystem::get_total_objectives(std::uint64_t id) const {
    auto quest = get_quest(id);
    return quest ? static_cast<std::uint32_t>(quest->objectives.size()) : 0;
}

ObjectiveId QuestSystem::get_current_objective(std::uint64_t id) const {
    auto quest = get_quest(id);
    if (!quest || !m_tracker) return ObjectiveId{};

    for (ObjectiveId obj_id : quest->objectives) {
        auto state = m_tracker->get_state(obj_id);
        if (state == ObjectiveState::Active) {
            return obj_id;
        }
    }

    return ObjectiveId{};
}

std::vector<std::uint64_t> QuestSystem::get_all_quests() const {
    std::vector<std::uint64_t> result;
    result.reserve(m_quests.size());
    for (const auto& [id, _] : m_quests) {
        result.push_back(id);
    }
    return result;
}

std::vector<std::uint64_t> QuestSystem::get_quests_by_state(ObjectiveState state) const {
    std::vector<std::uint64_t> result;
    for (const auto& [id, quest] : m_quests) {
        if (quest.state == state) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::uint64_t> QuestSystem::get_available_quests() const {
    std::vector<std::uint64_t> result;
    for (const auto& [id, quest] : m_quests) {
        if ((quest.state == ObjectiveState::Hidden || quest.state == ObjectiveState::Inactive) &&
            can_accept_quest(id)) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::uint64_t> QuestSystem::get_active_quests() const {
    return get_quests_by_state(ObjectiveState::Active);
}

std::vector<std::uint64_t> QuestSystem::get_completed_quests() const {
    return get_quests_by_state(ObjectiveState::Completed);
}

std::vector<std::uint64_t> QuestSystem::get_failed_quests() const {
    return get_quests_by_state(ObjectiveState::Failed);
}

std::vector<std::uint64_t> QuestSystem::get_main_quests() const {
    std::vector<std::uint64_t> result;
    for (const auto& [id, quest] : m_quests) {
        if (quest.is_main_quest) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::uint64_t> QuestSystem::get_side_quests() const {
    std::vector<std::uint64_t> result;
    for (const auto& [id, quest] : m_quests) {
        if (!quest.is_main_quest) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::uint64_t> QuestSystem::get_quests_by_category(const std::string& category) const {
    std::vector<std::uint64_t> result;
    for (const auto& [id, quest] : m_quests) {
        if (quest.category == category) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::uint64_t> QuestSystem::get_quests_by_giver(const std::string& giver) const {
    std::vector<std::uint64_t> result;
    for (const auto& [id, quest] : m_quests) {
        if (quest.giver_name == giver) {
            result.push_back(id);
        }
    }
    return result;
}

bool QuestSystem::can_accept_quest(std::uint64_t id) const {
    auto quest = get_quest(id);
    if (!quest) return false;

    // Check level requirement
    if (quest->required_level > m_player_level) {
        return false;
    }

    // Check required quests
    if (!has_required_quests(id)) {
        return false;
    }

    return true;
}

bool QuestSystem::meets_level_requirement(std::uint64_t id, std::uint32_t player_level) const {
    auto quest = get_quest(id);
    return quest && quest->required_level <= player_level;
}

bool QuestSystem::has_required_quests(std::uint64_t id) const {
    auto quest = get_quest(id);
    if (!quest) return false;

    for (std::uint64_t req_id : quest->required_quests) {
        auto req_quest = get_quest(req_id);
        if (!req_quest || req_quest->state != ObjectiveState::Completed) {
            return false;
        }
    }

    return true;
}

bool QuestSystem::has_required_items(std::uint64_t id, const std::vector<std::string>& inventory) const {
    auto quest = get_quest(id);
    if (!quest) return false;

    for (const auto& item : quest->required_items) {
        bool found = false;
        for (const auto& inv_item : inventory) {
            if (inv_item == item) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

void QuestSystem::set_tracked_quest(std::uint64_t id) {
    m_tracked_quest = id;

    // Also track objectives
    if (m_tracker) {
        auto quest = get_quest(id);
        if (quest) {
            for (ObjectiveId obj_id : quest->objectives) {
                m_tracker->set_tracked(obj_id, true);
            }
        }
    }
}

void QuestSystem::clear_tracked_quest() {
    m_tracked_quest = 0;
}

void QuestSystem::update(float delta_time) {
    // Update timed quests
    for (auto& [id, quest] : m_quests) {
        if (quest.state != ObjectiveState::Active) continue;

        if (quest.time_limit > 0) {
            float elapsed = static_cast<float>(m_current_time - quest.accepted_time);
            if (elapsed >= quest.time_limit) {
                fail_quest(id);
            }
        }
    }
}

void QuestSystem::on_objective_state_changed(const ObjectiveEvent& event) {
    // Find quest for this objective
    auto it = m_objective_to_quest.find(event.objective);
    if (it == m_objective_to_quest.end()) return;

    std::uint64_t quest_id = it->second;
    auto quest = get_quest_mut(quest_id);
    if (!quest || quest->state != ObjectiveState::Active) return;

    if (event.new_state == ObjectiveState::Completed) {
        if (m_on_objective_completed) {
            m_on_objective_completed(quest_id, event.objective);
        }

        // Check if quest is complete
        bool all_complete = true;
        for (ObjectiveId obj_id : quest->objectives) {
            if (m_tracker->get_state(obj_id) != ObjectiveState::Completed) {
                all_complete = false;
                break;
            }
        }

        if (all_complete) {
            complete_quest(quest_id);
        } else if (quest->sequential && m_tracker) {
            // Activate next objective
            bool found_completed = false;
            for (ObjectiveId obj_id : quest->objectives) {
                if (obj_id == event.objective) {
                    found_completed = true;
                } else if (found_completed) {
                    auto state = m_tracker->get_state(obj_id);
                    if (state == ObjectiveState::Hidden || state == ObjectiveState::Inactive) {
                        m_tracker->activate(obj_id);
                        break;
                    }
                }
            }
        }
    } else if (event.new_state == ObjectiveState::Failed) {
        // Check if this causes quest failure
        auto def = m_tracker->get_definition(event.objective);
        if (def && def->type == ObjectiveType::Primary) {
            fail_quest(quest_id);
        }
    }
}

std::vector<QuestSystem::SerializedQuest> QuestSystem::serialize() const {
    std::vector<SerializedQuest> result;
    result.reserve(m_quests.size());

    for (const auto& [id, quest] : m_quests) {
        SerializedQuest sq{
            .id = id,
            .state = static_cast<std::uint8_t>(quest.state),
            .accepted_time = quest.accepted_time,
            .completed_time = quest.completed_time
        };
        result.push_back(sq);
    }

    return result;
}

void QuestSystem::deserialize(const std::vector<SerializedQuest>& data) {
    for (const auto& sq : data) {
        auto it = m_quests.find(sq.id);
        if (it != m_quests.end()) {
            it->second.state = static_cast<ObjectiveState>(sq.state);
            it->second.accepted_time = sq.accepted_time;
            it->second.completed_time = sq.completed_time;
        }
    }
}

void QuestSystem::clear() {
    m_quests.clear();
    m_name_lookup.clear();
    m_objective_to_quest.clear();
    m_tracked_quest = 0;
}

} // namespace void_gamestate
