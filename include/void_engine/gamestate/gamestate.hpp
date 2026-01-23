/// @file gamestate.hpp
/// @brief Main game state system for void_gamestate module

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "variables.hpp"
#include "saveload.hpp"
#include "objectives.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_gamestate {

// =============================================================================
// IPhaseState Interface
// =============================================================================

/// @brief Interface for phase state implementations
class IPhaseState {
public:
    virtual ~IPhaseState() = default;

    /// @brief Called when entering this phase
    virtual void on_enter() = 0;

    /// @brief Called when exiting this phase
    virtual void on_exit() = 0;

    /// @brief Called every frame while in this phase
    virtual void on_update(float delta_time) = 0;

    /// @brief Called during transition into this phase
    virtual void on_transition_in(float progress) {}

    /// @brief Called during transition out of this phase
    virtual void on_transition_out(float progress) {}

    /// @brief Get phase definition
    virtual const GamePhase& phase() const = 0;

    /// @brief Check if this phase allows input
    virtual bool allows_input() const { return phase().allow_input; }

    /// @brief Check if this phase shows HUD
    virtual bool shows_hud() const { return phase().show_hud; }

    /// @brief Check if this phase pauses the game
    virtual bool pauses_game() const { return phase().pause_game; }
};

// =============================================================================
// PhaseCondition
// =============================================================================

/// @brief Condition for automatic phase transitions
class PhaseCondition {
public:
    using ConditionFunc = std::function<bool()>;

    PhaseCondition() = default;
    explicit PhaseCondition(ConditionFunc func) : m_func(std::move(func)) {}

    bool evaluate() const { return m_func ? m_func() : false; }
    bool is_valid() const { return m_func != nullptr; }

    // Factory methods
    static PhaseCondition always() { return PhaseCondition([]{ return true; }); }
    static PhaseCondition never() { return PhaseCondition([]{ return false; }); }
    static PhaseCondition variable_equals(VariableStore* store, std::string_view name, bool value);
    static PhaseCondition variable_equals(VariableStore* store, std::string_view name, int value);
    static PhaseCondition timer_elapsed(float* timer, float duration);

private:
    ConditionFunc m_func;
};

// =============================================================================
// GameStateMachine
// =============================================================================

/// @brief Manages game phase transitions
class GameStateMachine {
public:
    GameStateMachine();
    ~GameStateMachine();

    // Phase registration
    GamePhaseId register_phase(const GamePhase& phase);
    GamePhaseId register_phase(const GamePhase& phase, std::unique_ptr<IPhaseState> state);
    bool unregister_phase(GamePhaseId id);

    // Phase lookup
    const GamePhase* get_phase(GamePhaseId id) const;
    GamePhase* get_phase_mut(GamePhaseId id);
    GamePhaseId find_phase(std::string_view name) const;
    IPhaseState* get_state(GamePhaseId id);
    const IPhaseState* get_state(GamePhaseId id) const;

    // Transition registration
    void register_transition(const PhaseTransition& transition);
    void register_transition(GamePhaseId from, GamePhaseId to, PhaseCondition condition);
    void clear_transitions();

    // State management
    GamePhaseId current_phase() const { return m_current_phase; }
    GamePhaseId previous_phase() const { return m_previous_phase; }
    bool is_in_transition() const { return m_transitioning; }
    float transition_progress() const { return m_transition_progress; }

    // Phase changes
    bool change_phase(GamePhaseId new_phase);
    bool change_phase(GamePhaseId new_phase, TransitionType transition, float duration = 0.5f);
    bool request_phase(GamePhaseId phase);
    void cancel_transition();

    // Quick accessors
    bool is_in_phase(GamePhaseId id) const { return m_current_phase == id; }
    bool is_in_phase(PhaseType type) const;
    bool is_gameplay() const;
    bool is_paused() const;
    bool is_menu() const;
    bool is_loading() const;

    // Input/HUD queries
    bool allows_input() const;
    bool shows_hud() const;
    bool allows_pause() const;

    // Update
    void update(float delta_time);

    // History
    std::vector<GamePhaseId> get_phase_history() const { return m_history; }
    void clear_history() { m_history.clear(); }
    bool can_go_back() const { return m_history.size() > 1; }
    bool go_back();

    // Stack-based phases (for menus, dialogs, etc.)
    void push_phase(GamePhaseId phase);
    void pop_phase();
    GamePhaseId peek_phase() const;
    std::size_t stack_depth() const { return m_phase_stack.size(); }

    // Callbacks
    void set_on_phase_change(PhaseChangeCallback callback) { m_on_change = std::move(callback); }
    void set_on_transition_start(TransitionCallback callback) { m_on_transition_start = std::move(callback); }
    void set_on_transition_end(std::function<void()> callback) { m_on_transition_end = std::move(callback); }

    // Serialization
    GamePhaseId deserialize_phase(std::uint64_t value) const;

private:
    void start_transition(GamePhaseId to_phase, TransitionType type, float duration);
    void complete_transition();
    void update_transition(float delta_time);
    void check_automatic_transitions();
    void notify_change(GamePhaseId old_phase, GamePhaseId new_phase, TransitionType transition);

    std::unordered_map<GamePhaseId, GamePhase> m_phases;
    std::unordered_map<GamePhaseId, std::unique_ptr<IPhaseState>> m_states;
    std::unordered_map<std::string, GamePhaseId> m_name_lookup;
    std::vector<PhaseTransition> m_transitions;

    GamePhaseId m_current_phase;
    GamePhaseId m_previous_phase;
    GamePhaseId m_target_phase;

    bool m_transitioning{false};
    float m_transition_progress{0};
    float m_transition_duration{0};
    TransitionType m_transition_type{TransitionType::Immediate};

    std::vector<GamePhaseId> m_history;
    std::vector<GamePhaseId> m_phase_stack;
    std::uint64_t m_next_id{1};

    PhaseChangeCallback m_on_change;
    TransitionCallback m_on_transition_start;
    std::function<void()> m_on_transition_end;
};

// =============================================================================
// GameStateSystem
// =============================================================================

/// @brief Main game state management system
class GameStateSystem {
public:
    GameStateSystem();
    explicit GameStateSystem(const GameStateConfig& config);
    ~GameStateSystem();

    // Initialization
    void initialize();
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Configuration
    const GameStateConfig& config() const { return m_config; }
    void set_config(const GameStateConfig& config);

    // Subsystem access
    VariableStore& variables() { return m_variables; }
    const VariableStore& variables() const { return m_variables; }

    GlobalVariables& globals() { return m_globals; }
    const GlobalVariables& globals() const { return m_globals; }

    EntityVariables& entity_variables() { return m_entity_vars; }
    const EntityVariables& entity_variables() const { return m_entity_vars; }

    GameStateMachine& state_machine() { return m_state_machine; }
    const GameStateMachine& state_machine() const { return m_state_machine; }

    SaveManager& save_manager() { return m_save_manager; }
    const SaveManager& save_manager() const { return m_save_manager; }

    AutoSaveManager& auto_save() { return m_auto_save; }
    const AutoSaveManager& auto_save() const { return m_auto_save; }

    CheckpointManager& checkpoints() { return m_checkpoints; }
    const CheckpointManager& checkpoints() const { return m_checkpoints; }

    ObjectiveTracker& objectives() { return m_objectives; }
    const ObjectiveTracker& objectives() const { return m_objectives; }

    QuestSystem& quests() { return m_quests; }
    const QuestSystem& quests() const { return m_quests; }

    // Update
    void update(float delta_time);

    // Time
    double current_time() const { return m_current_time; }
    double play_time() const { return m_play_time; }
    void set_time_scale(float scale) { m_time_scale = scale; }
    float time_scale() const { return m_time_scale; }

    // Level management
    void set_current_level(const std::string& level);
    const std::string& current_level() const { return m_current_level; }
    void on_level_load(const std::string& level);
    void on_level_unload();

    // Quick save/load
    void quick_save();
    void quick_load();

    // Checkpoint
    void create_checkpoint(const std::string& name = "");
    void load_latest_checkpoint();

    // Reset
    void reset_session();
    void reset_level();
    void new_game();

    // State queries
    bool is_new_game() const { return m_is_new_game; }
    bool has_save_data() const;

    // Callbacks
    void set_on_level_load(std::function<void(const std::string&)> callback) {
        m_on_level_load = std::move(callback);
    }
    void set_on_level_unload(std::function<void()> callback) {
        m_on_level_unload = std::move(callback);
    }
    void set_on_new_game(std::function<void()> callback) {
        m_on_new_game = std::move(callback);
    }

private:
    void setup_subsystems();
    void update_time(float delta_time);

    GameStateConfig m_config;
    bool m_initialized{false};

    // Subsystems
    VariableStore m_variables;
    GlobalVariables m_globals;
    EntityVariables m_entity_vars;
    GameStateMachine m_state_machine;
    SaveManager m_save_manager;
    AutoSaveManager m_auto_save;
    CheckpointManager m_checkpoints;
    ObjectiveTracker m_objectives;
    QuestSystem m_quests;

    // Time tracking
    double m_current_time{0};
    double m_play_time{0};
    float m_time_scale{1.0f};

    // State
    std::string m_current_level;
    bool m_is_new_game{true};

    // Callbacks
    std::function<void(const std::string&)> m_on_level_load;
    std::function<void()> m_on_level_unload;
    std::function<void()> m_on_new_game;
};

// =============================================================================
// PhaseBuilder
// =============================================================================

/// @brief Fluent builder for game phases
class PhaseBuilder {
public:
    PhaseBuilder() = default;

    PhaseBuilder& name(const std::string& name) {
        m_phase.name = name;
        return *this;
    }

    PhaseBuilder& type(PhaseType type) {
        m_phase.type = type;
        return *this;
    }

    PhaseBuilder& menu() {
        m_phase.type = PhaseType::Menu;
        m_phase.pause_game = true;
        m_phase.show_hud = false;
        return *this;
    }

    PhaseBuilder& gameplay() {
        m_phase.type = PhaseType::Gameplay;
        m_phase.pause_game = false;
        m_phase.show_hud = true;
        m_phase.allow_input = true;
        m_phase.allow_pause = true;
        return *this;
    }

    PhaseBuilder& pause() {
        m_phase.type = PhaseType::Pause;
        m_phase.pause_game = true;
        m_phase.show_hud = true;
        return *this;
    }

    PhaseBuilder& loading() {
        m_phase.type = PhaseType::Loading;
        m_phase.pause_game = true;
        m_phase.show_hud = false;
        m_phase.allow_input = false;
        m_phase.allow_pause = false;
        return *this;
    }

    PhaseBuilder& cutscene() {
        m_phase.type = PhaseType::Cutscene;
        m_phase.pause_game = false;
        m_phase.show_hud = false;
        m_phase.allow_input = false;
        m_phase.allow_pause = true;
        return *this;
    }

    PhaseBuilder& dialog() {
        m_phase.type = PhaseType::Dialog;
        m_phase.pause_game = true;
        m_phase.show_hud = true;
        m_phase.allow_input = true;
        return *this;
    }

    PhaseBuilder& enter_transition(TransitionType type, float duration = 0.5f) {
        m_phase.enter_transition = type;
        m_phase.transition_duration = duration;
        return *this;
    }

    PhaseBuilder& exit_transition(TransitionType type, float duration = 0.5f) {
        m_phase.exit_transition = type;
        m_phase.transition_duration = duration;
        return *this;
    }

    PhaseBuilder& pause_game(bool value) {
        m_phase.pause_game = value;
        return *this;
    }

    PhaseBuilder& show_hud(bool value) {
        m_phase.show_hud = value;
        return *this;
    }

    PhaseBuilder& allow_input(bool value) {
        m_phase.allow_input = value;
        return *this;
    }

    PhaseBuilder& allow_pause(bool value) {
        m_phase.allow_pause = value;
        return *this;
    }

    PhaseBuilder& scene(const std::string& name) {
        m_phase.scene_name = name;
        return *this;
    }

    PhaseBuilder& music(const std::string& track) {
        m_phase.music_track = track;
        return *this;
    }

    PhaseBuilder& custom_data(const std::string& key, const std::string& value) {
        m_phase.custom_data[key] = value;
        return *this;
    }

    GamePhase build() const { return m_phase; }

private:
    GamePhase m_phase;
};

// =============================================================================
// Preset Phases
// =============================================================================

namespace presets {

/// @brief Create standard main menu phase
inline GamePhase main_menu_phase() {
    return PhaseBuilder()
        .name("MainMenu")
        .menu()
        .enter_transition(TransitionType::FadeIn)
        .exit_transition(TransitionType::FadeOut)
        .build();
}

/// @brief Create standard gameplay phase
inline GamePhase gameplay_phase() {
    return PhaseBuilder()
        .name("Gameplay")
        .gameplay()
        .enter_transition(TransitionType::FadeIn)
        .exit_transition(TransitionType::FadeOut)
        .build();
}

/// @brief Create standard pause phase
inline GamePhase pause_phase() {
    return PhaseBuilder()
        .name("Pause")
        .pause()
        .enter_transition(TransitionType::Immediate)
        .exit_transition(TransitionType::Immediate)
        .build();
}

/// @brief Create standard loading phase
inline GamePhase loading_phase() {
    return PhaseBuilder()
        .name("Loading")
        .loading()
        .enter_transition(TransitionType::FadeOut)
        .exit_transition(TransitionType::FadeIn)
        .build();
}

/// @brief Create standard cutscene phase
inline GamePhase cutscene_phase() {
    return PhaseBuilder()
        .name("Cutscene")
        .cutscene()
        .enter_transition(TransitionType::FadeOut)
        .exit_transition(TransitionType::FadeIn)
        .build();
}

/// @brief Create standard dialog phase
inline GamePhase dialog_phase() {
    return PhaseBuilder()
        .name("Dialog")
        .dialog()
        .enter_transition(TransitionType::Immediate)
        .exit_transition(TransitionType::Immediate)
        .build();
}

/// @brief Create game over phase
inline GamePhase game_over_phase() {
    return PhaseBuilder()
        .name("GameOver")
        .type(PhaseType::GameOver)
        .pause_game(true)
        .show_hud(false)
        .allow_input(true)
        .allow_pause(false)
        .enter_transition(TransitionType::FadeOut, 1.0f)
        .build();
}

/// @brief Create victory phase
inline GamePhase victory_phase() {
    return PhaseBuilder()
        .name("Victory")
        .type(PhaseType::Victory)
        .pause_game(true)
        .show_hud(false)
        .allow_input(true)
        .allow_pause(false)
        .enter_transition(TransitionType::FadeOut, 1.0f)
        .build();
}

/// @brief Create inventory phase
inline GamePhase inventory_phase() {
    return PhaseBuilder()
        .name("Inventory")
        .type(PhaseType::Inventory)
        .pause_game(true)
        .show_hud(true)
        .allow_input(true)
        .allow_pause(false)
        .enter_transition(TransitionType::Immediate)
        .exit_transition(TransitionType::Immediate)
        .build();
}

/// @brief Create combat phase
inline GamePhase combat_phase() {
    return PhaseBuilder()
        .name("Combat")
        .type(PhaseType::Combat)
        .pause_game(false)
        .show_hud(true)
        .allow_input(true)
        .allow_pause(true)
        .enter_transition(TransitionType::Immediate)
        .exit_transition(TransitionType::Immediate)
        .build();
}

} // namespace presets

// =============================================================================
// Prelude - Convenience namespace
// =============================================================================

namespace prelude {

using void_gamestate::VariableType;
using void_gamestate::VariableScope;
using void_gamestate::PersistenceFlags;
using void_gamestate::PhaseType;
using void_gamestate::TransitionType;
using void_gamestate::SaveType;
using void_gamestate::SaveResult;
using void_gamestate::LoadResult;
using void_gamestate::ObjectiveState;
using void_gamestate::ObjectiveType;

using void_gamestate::VariableId;
using void_gamestate::SaveSlotId;
using void_gamestate::CheckpointId;
using void_gamestate::GamePhaseId;
using void_gamestate::ObjectiveId;

using void_gamestate::Vec3;
using void_gamestate::Color;
using void_gamestate::VariableValue;
using void_gamestate::GameVariable;
using void_gamestate::VariableBinding;
using void_gamestate::SaveMetadata;
using void_gamestate::SaveData;
using void_gamestate::SaveSlot;
using void_gamestate::ObjectiveDef;
using void_gamestate::ObjectiveProgress;
using void_gamestate::GamePhase;
using void_gamestate::PhaseTransition;
using void_gamestate::GameStateConfig;

using void_gamestate::VariableStore;
using void_gamestate::GlobalVariables;
using void_gamestate::EntityVariables;
using void_gamestate::VariableExpression;

using void_gamestate::ISaveable;
using void_gamestate::SaveSerializer;
using void_gamestate::SaveManager;
using void_gamestate::AutoSaveManager;
using void_gamestate::CheckpointManager;
using void_gamestate::SaveStateSnapshot;
using void_gamestate::SaveMigrator;

using void_gamestate::ObjectiveTracker;
using void_gamestate::Quest;
using void_gamestate::QuestSystem;
using void_gamestate::ObjectiveBuilder;
using void_gamestate::QuestBuilder;

using void_gamestate::IPhaseState;
using void_gamestate::PhaseCondition;
using void_gamestate::GameStateMachine;
using void_gamestate::GameStateSystem;
using void_gamestate::PhaseBuilder;

namespace presets = void_gamestate::presets;

} // namespace prelude

} // namespace void_gamestate
