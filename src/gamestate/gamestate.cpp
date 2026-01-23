/// @file gamestate.cpp
/// @brief Implementation of main game state system for void_gamestate module

#include "void_engine/gamestate/gamestate.hpp"

#include <algorithm>

namespace void_gamestate {

// =============================================================================
// PhaseCondition
// =============================================================================

PhaseCondition PhaseCondition::variable_equals(VariableStore* store, std::string_view name, bool value) {
    std::string name_str(name);
    return PhaseCondition([store, name_str, value]() {
        return store && store->get_bool(name_str) == value;
    });
}

PhaseCondition PhaseCondition::variable_equals(VariableStore* store, std::string_view name, int value) {
    std::string name_str(name);
    return PhaseCondition([store, name_str, value]() {
        return store && store->get_int(name_str) == value;
    });
}

PhaseCondition PhaseCondition::timer_elapsed(float* timer, float duration) {
    return PhaseCondition([timer, duration]() {
        return timer && *timer >= duration;
    });
}

// =============================================================================
// GameStateMachine
// =============================================================================

GameStateMachine::GameStateMachine() = default;
GameStateMachine::~GameStateMachine() = default;

GamePhaseId GameStateMachine::register_phase(const GamePhase& phase) {
    return register_phase(phase, nullptr);
}

GamePhaseId GameStateMachine::register_phase(const GamePhase& phase, std::unique_ptr<IPhaseState> state) {
    // Check for name conflicts
    if (m_name_lookup.contains(phase.name)) {
        return m_name_lookup[phase.name];
    }

    GamePhaseId id{m_next_id++};
    GamePhase new_phase = phase;
    new_phase.id = id;

    m_phases[id] = std::move(new_phase);
    m_name_lookup[phase.name] = id;

    if (state) {
        m_states[id] = std::move(state);
    }

    // Set as current if first phase
    if (!m_current_phase) {
        m_current_phase = id;
    }

    return id;
}

bool GameStateMachine::unregister_phase(GamePhaseId id) {
    auto it = m_phases.find(id);
    if (it == m_phases.end()) return false;

    // Can't unregister current phase
    if (m_current_phase == id) return false;

    m_name_lookup.erase(it->second.name);
    m_states.erase(id);
    m_phases.erase(it);

    return true;
}

const GamePhase* GameStateMachine::get_phase(GamePhaseId id) const {
    auto it = m_phases.find(id);
    return it != m_phases.end() ? &it->second : nullptr;
}

GamePhase* GameStateMachine::get_phase_mut(GamePhaseId id) {
    auto it = m_phases.find(id);
    return it != m_phases.end() ? &it->second : nullptr;
}

GamePhaseId GameStateMachine::find_phase(std::string_view name) const {
    std::string name_str(name);
    auto it = m_name_lookup.find(name_str);
    return it != m_name_lookup.end() ? it->second : GamePhaseId{};
}

IPhaseState* GameStateMachine::get_state(GamePhaseId id) {
    auto it = m_states.find(id);
    return it != m_states.end() ? it->second.get() : nullptr;
}

const IPhaseState* GameStateMachine::get_state(GamePhaseId id) const {
    auto it = m_states.find(id);
    return it != m_states.end() ? it->second.get() : nullptr;
}

void GameStateMachine::register_transition(const PhaseTransition& transition) {
    m_transitions.push_back(transition);
}

void GameStateMachine::register_transition(GamePhaseId from, GamePhaseId to, PhaseCondition condition) {
    PhaseTransition trans{
        .from_phase = from,
        .to_phase = to,
        .type = TransitionType::Immediate,
        .duration = 0.0f,
        .condition = [cond = std::move(condition)]() { return cond.evaluate(); }
    };
    m_transitions.push_back(std::move(trans));
}

void GameStateMachine::clear_transitions() {
    m_transitions.clear();
}

bool GameStateMachine::change_phase(GamePhaseId new_phase) {
    auto phase = get_phase(new_phase);
    if (!phase) return false;

    return change_phase(new_phase, phase->enter_transition, phase->transition_duration);
}

bool GameStateMachine::change_phase(GamePhaseId new_phase, TransitionType transition, float duration) {
    if (!m_phases.contains(new_phase)) return false;
    if (m_transitioning) return false;

    if (transition == TransitionType::Immediate || duration <= 0) {
        // Immediate transition
        if (auto* state = get_state(m_current_phase)) {
            state->on_exit();
        }

        m_previous_phase = m_current_phase;
        m_current_phase = new_phase;
        m_history.push_back(new_phase);

        if (auto* state = get_state(m_current_phase)) {
            state->on_enter();
        }

        notify_change(m_previous_phase, m_current_phase, transition);
    } else {
        // Start animated transition
        start_transition(new_phase, transition, duration);
    }

    return true;
}

bool GameStateMachine::request_phase(GamePhaseId phase) {
    // Request to change - will be checked on next update
    m_target_phase = phase;
    return true;
}

void GameStateMachine::cancel_transition() {
    if (m_transitioning) {
        m_transitioning = false;
        m_transition_progress = 0;
    }
}

void GameStateMachine::start_transition(GamePhaseId to_phase, TransitionType type, float duration) {
    m_target_phase = to_phase;
    m_transition_type = type;
    m_transition_duration = duration;
    m_transition_progress = 0;
    m_transitioning = true;

    if (m_on_transition_start) {
        m_on_transition_start(0);
    }

    // Notify current state of transition out
    if (auto* state = get_state(m_current_phase)) {
        state->on_transition_out(0);
    }
}

void GameStateMachine::complete_transition() {
    // Exit old state
    if (auto* state = get_state(m_current_phase)) {
        state->on_exit();
    }

    m_previous_phase = m_current_phase;
    m_current_phase = m_target_phase;
    m_history.push_back(m_current_phase);

    // Enter new state
    if (auto* state = get_state(m_current_phase)) {
        state->on_enter();
    }

    m_transitioning = false;
    m_transition_progress = 0;
    m_target_phase = GamePhaseId{};

    notify_change(m_previous_phase, m_current_phase, m_transition_type);

    if (m_on_transition_end) {
        m_on_transition_end();
    }
}

void GameStateMachine::update_transition(float delta_time) {
    if (!m_transitioning) return;

    m_transition_progress += delta_time / m_transition_duration;

    if (m_transition_progress >= 1.0f) {
        m_transition_progress = 1.0f;
        complete_transition();
    } else {
        // Update transition callbacks
        if (m_on_transition_start) {
            m_on_transition_start(m_transition_progress);
        }

        // Notify states
        if (auto* state = get_state(m_current_phase)) {
            state->on_transition_out(m_transition_progress);
        }
        if (auto* state = get_state(m_target_phase)) {
            state->on_transition_in(m_transition_progress);
        }
    }
}

bool GameStateMachine::is_in_phase(PhaseType type) const {
    auto phase = get_phase(m_current_phase);
    return phase && phase->type == type;
}

bool GameStateMachine::is_gameplay() const {
    return is_in_phase(PhaseType::Gameplay);
}

bool GameStateMachine::is_paused() const {
    return is_in_phase(PhaseType::Pause);
}

bool GameStateMachine::is_menu() const {
    return is_in_phase(PhaseType::Menu);
}

bool GameStateMachine::is_loading() const {
    return is_in_phase(PhaseType::Loading);
}

bool GameStateMachine::allows_input() const {
    if (auto* state = get_state(m_current_phase)) {
        return state->allows_input();
    }
    auto phase = get_phase(m_current_phase);
    return phase ? phase->allow_input : true;
}

bool GameStateMachine::shows_hud() const {
    if (auto* state = get_state(m_current_phase)) {
        return state->shows_hud();
    }
    auto phase = get_phase(m_current_phase);
    return phase ? phase->show_hud : true;
}

bool GameStateMachine::allows_pause() const {
    auto phase = get_phase(m_current_phase);
    return phase ? phase->allow_pause : true;
}

void GameStateMachine::update(float delta_time) {
    // Update transition
    if (m_transitioning) {
        update_transition(delta_time);
        return;
    }

    // Check for requested phase change
    if (m_target_phase) {
        change_phase(m_target_phase);
        m_target_phase = GamePhaseId{};
    }

    // Check automatic transitions
    check_automatic_transitions();

    // Update current state
    if (auto* state = get_state(m_current_phase)) {
        state->on_update(delta_time);
    }
}

void GameStateMachine::check_automatic_transitions() {
    for (const auto& trans : m_transitions) {
        if (trans.from_phase == m_current_phase && trans.condition && trans.condition()) {
            if (trans.on_start) {
                trans.on_start();
            }

            change_phase(trans.to_phase, trans.type, trans.duration);

            if (trans.on_complete) {
                trans.on_complete();
            }
            break;
        }
    }
}

bool GameStateMachine::go_back() {
    if (m_history.size() < 2) return false;

    m_history.pop_back();
    GamePhaseId prev = m_history.back();
    m_history.pop_back(); // Will be re-added in change_phase

    return change_phase(prev);
}

void GameStateMachine::push_phase(GamePhaseId phase) {
    m_phase_stack.push_back(m_current_phase);
    change_phase(phase);
}

void GameStateMachine::pop_phase() {
    if (m_phase_stack.empty()) return;

    GamePhaseId prev = m_phase_stack.back();
    m_phase_stack.pop_back();
    change_phase(prev);
}

GamePhaseId GameStateMachine::peek_phase() const {
    return m_phase_stack.empty() ? GamePhaseId{} : m_phase_stack.back();
}

void GameStateMachine::notify_change(GamePhaseId old_phase, GamePhaseId new_phase, TransitionType transition) {
    if (m_on_change) {
        PhaseChangeEvent event{
            .old_phase = old_phase,
            .new_phase = new_phase,
            .transition = transition,
            .timestamp = 0 // Would be set from system time
        };
        m_on_change(event);
    }
}

GamePhaseId GameStateMachine::deserialize_phase(std::uint64_t value) const {
    GamePhaseId id{value};
    return m_phases.contains(id) ? id : GamePhaseId{};
}

// =============================================================================
// GameStateSystem
// =============================================================================

GameStateSystem::GameStateSystem()
    : m_globals(&m_variables)
    , m_auto_save(&m_save_manager)
    , m_checkpoints(&m_save_manager)
    , m_quests(&m_objectives) {
    setup_subsystems();
}

GameStateSystem::GameStateSystem(const GameStateConfig& config)
    : m_config(config)
    , m_globals(&m_variables)
    , m_save_manager(config)
    , m_auto_save(&m_save_manager)
    , m_checkpoints(&m_save_manager)
    , m_quests(&m_objectives) {
    setup_subsystems();
}

GameStateSystem::~GameStateSystem() {
    shutdown();
}

void GameStateSystem::initialize() {
    if (m_initialized) return;

    // Initialize save directory
    m_save_manager.set_save_directory(m_config.save_directory);
    m_save_manager.set_max_slots(m_config.max_save_slots);

    // Initialize auto-save
    m_auto_save.set_interval(m_config.auto_save_interval);
    m_auto_save.set_max_auto_saves(m_config.max_auto_saves);

    // Initialize checkpoints
    m_checkpoints.set_max_checkpoints(m_config.max_checkpoints);
    m_checkpoints.set_enabled(m_config.enable_checkpoints);

    // Initialize variables
    m_variables.set_track_history(m_config.track_variable_history);

    m_initialized = true;
}

void GameStateSystem::shutdown() {
    if (!m_initialized) return;

    m_auto_save.disable();
    m_initialized = false;
}

void GameStateSystem::set_config(const GameStateConfig& config) {
    m_config = config;

    if (m_initialized) {
        m_save_manager.set_save_directory(config.save_directory);
        m_save_manager.set_max_slots(config.max_save_slots);
        m_auto_save.set_interval(config.auto_save_interval);
        m_auto_save.set_max_auto_saves(config.max_auto_saves);
        m_checkpoints.set_max_checkpoints(config.max_checkpoints);
        m_checkpoints.set_enabled(config.enable_checkpoints);
        m_variables.set_track_history(config.track_variable_history);
    }
}

void GameStateSystem::setup_subsystems() {
    // m_globals and m_quests are already initialized in the constructor initializer list
    // with their required dependencies (&m_variables and &m_objectives respectively)

    // Connect save manager to subsystems (setters for runtime reconfiguration)
    m_auto_save.set_save_manager(&m_save_manager);
    m_checkpoints.set_save_manager(&m_save_manager);
}

void GameStateSystem::update(float delta_time) {
    if (!m_initialized) return;

    update_time(delta_time);

    // Update subsystems
    m_state_machine.update(delta_time);
    m_auto_save.update(delta_time);
    m_objectives.update(delta_time);
    m_quests.update(delta_time);
}

void GameStateSystem::update_time(float delta_time) {
    float scaled_delta = delta_time * m_time_scale;
    m_current_time += scaled_delta;

    // Only track play time during gameplay
    if (m_state_machine.is_gameplay()) {
        m_play_time += scaled_delta;
    }

    // Update time in subsystems
    m_variables.set_current_time(m_current_time);
    m_objectives.set_current_time(m_current_time);
    m_quests.set_current_time(m_current_time);
    m_checkpoints.set_current_time(m_current_time);
    m_save_manager.set_play_time(m_play_time);
}

void GameStateSystem::set_current_level(const std::string& level) {
    m_current_level = level;
    m_save_manager.set_current_level(level);
    m_checkpoints.set_current_level(level);
}

void GameStateSystem::on_level_load(const std::string& level) {
    set_current_level(level);

    // Reset level-scoped variables
    m_variables.reset_scope(VariableScope::Level);

    if (m_on_level_load) {
        m_on_level_load(level);
    }
}

void GameStateSystem::on_level_unload() {
    // Clear level-scoped variables
    m_variables.reset_scope(VariableScope::Level);

    // Clear level checkpoints
    auto checkpoints = m_checkpoints.get_checkpoints_in_level(m_current_level);
    for (const auto& cp : checkpoints) {
        m_checkpoints.delete_checkpoint(cp.id);
    }

    if (m_on_level_unload) {
        m_on_level_unload();
    }
}

void GameStateSystem::quick_save() {
    m_save_manager.quick_save();
}

void GameStateSystem::quick_load() {
    m_save_manager.quick_load();
}

void GameStateSystem::create_checkpoint(const std::string& name) {
    m_checkpoints.create_checkpoint(name);
}

void GameStateSystem::load_latest_checkpoint() {
    m_checkpoints.load_latest_checkpoint();
}

void GameStateSystem::reset_session() {
    // Reset session-scoped variables
    m_variables.reset_scope(VariableScope::Session);

    // Clear objectives
    m_objectives.reset_all();

    // Clear checkpoints
    m_checkpoints.clear_all_checkpoints();

    m_play_time = 0;
}

void GameStateSystem::reset_level() {
    m_variables.reset_scope(VariableScope::Level);

    // Load latest checkpoint if available
    if (m_checkpoints.checkpoint_count() > 0) {
        load_latest_checkpoint();
    }
}

void GameStateSystem::new_game() {
    m_is_new_game = true;

    // Reset all non-persistent variables
    m_variables.reset_scope(VariableScope::Global);
    m_variables.reset_scope(VariableScope::Level);
    m_variables.reset_scope(VariableScope::Session);

    // Clear entity variables
    m_entity_vars.clear_all();

    // Reset objectives and quests
    m_objectives.reset_all();

    // Clear checkpoints
    m_checkpoints.clear_all_checkpoints();

    // Reset time
    m_current_time = 0;
    m_play_time = 0;

    if (m_on_new_game) {
        m_on_new_game();
    }
}

bool GameStateSystem::has_save_data() const {
    return m_save_manager.get_latest_slot().value != 0;
}

} // namespace void_gamestate
