#pragma once

/// @file state_machine.hpp
/// @brief Finite State Machine (FSM) implementation for void_ai
///
/// Provides a generic, type-safe FSM with:
/// - State lifecycle hooks (on_enter, on_exit, on_update)
/// - Priority-based transitions
/// - Global transitions (from any state)
/// - Hot-reload support via snapshots

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_ai {

// =============================================================================
// State Interface
// =============================================================================

/// State interface for FSM states
/// @tparam Context The context type passed to state methods
template<typename Context>
class IState {
public:
    virtual ~IState() = default;

    /// Called when entering this state
    virtual void on_enter([[maybe_unused]] Context& ctx) {}

    /// Called when exiting this state
    virtual void on_exit([[maybe_unused]] Context& ctx) {}

    /// Called every update while in this state
    /// @return true if state should continue, false to trigger exit
    virtual bool on_update([[maybe_unused]] Context& ctx, [[maybe_unused]] float dt) { return true; }

    /// Get state name for debugging/serialization
    [[nodiscard]] virtual std::string_view name() const = 0;
};

// =============================================================================
// Transition
// =============================================================================

/// Transition condition function type
template<typename Context>
using TransitionCondition = std::function<bool(const Context&)>;

/// A state transition with condition and priority
template<typename StateId, typename Context>
struct Transition {
    StateId to_state;                          ///< Target state
    TransitionCondition<Context> condition;    ///< Condition function
    std::int32_t priority = 0;                 ///< Higher = checked first

    /// Create a transition
    static Transition create(StateId to, TransitionCondition<Context> cond, std::int32_t prio = 0) {
        return Transition{to, std::move(cond), prio};
    }

    /// Check if transition should occur
    [[nodiscard]] bool should_transition(const Context& ctx) const {
        return condition && condition(ctx);
    }
};

// =============================================================================
// State Machine
// =============================================================================

/// Generic Finite State Machine
/// @tparam StateId Type used to identify states (enum, string, int, etc.)
/// @tparam Context Context type passed to states and conditions
template<typename StateId, typename Context>
class StateMachine {
public:
    using StatePtr = std::unique_ptr<IState<Context>>;
    using TransitionType = Transition<StateId, Context>;

    /// Create a state machine with initial state
    explicit StateMachine(StateId initial_state)
        : m_current_state(initial_state)
        , m_initial_state(initial_state) {
    }

    /// Register a state
    void register_state(StateId id, StatePtr state) {
        m_states[id] = std::move(state);
    }

    /// Register a state with a factory function
    template<typename StateType, typename... Args>
    void emplace_state(StateId id, Args&&... args) {
        m_states[id] = std::make_unique<StateType>(std::forward<Args>(args)...);
    }

    /// Add a transition from one state to another
    void add_transition(StateId from, StateId to, TransitionCondition<Context> condition) {
        m_transitions[from].push_back(TransitionType::create(to, std::move(condition)));
    }

    /// Add a transition with priority
    void add_transition_priority(StateId from, StateId to, TransitionCondition<Context> condition, std::int32_t priority) {
        m_transitions[from].push_back(TransitionType::create(to, std::move(condition), priority));
    }

    /// Add a global transition (checked from any state)
    void add_global_transition(StateId to, TransitionCondition<Context> condition) {
        m_global_transitions.push_back(TransitionType::create(to, std::move(condition)));
    }

    /// Add a global transition with priority
    void add_global_transition_priority(StateId to, TransitionCondition<Context> condition, std::int32_t priority) {
        m_global_transitions.push_back(TransitionType::create(to, std::move(condition), priority));
    }

    /// Start the state machine (enters initial state)
    void start(Context& ctx) {
        if (!m_started) {
            m_started = true;
            enter_state(m_current_state, ctx);
        }
    }

    /// Update the state machine
    void update(Context& ctx, float dt) {
        if (!m_started) {
            start(ctx);
        }

        // Update current state
        if (auto* state = get_state(m_current_state)) {
            if (!state->on_update(ctx, dt)) {
                // State signaled it wants to exit - check transitions
            }
        }

        // Check global transitions first (sorted by priority)
        for (const auto& transition : m_sorted_global_transitions) {
            if (m_current_state != transition.to_state && transition.should_transition(ctx)) {
                force_transition(transition.to_state, ctx);
                return;
            }
        }

        // Check state-specific transitions
        auto it = m_transitions.find(m_current_state);
        if (it != m_transitions.end()) {
            // Sort transitions by priority (higher first)
            auto sorted = it->second;
            std::sort(sorted.begin(), sorted.end(),
                [](const TransitionType& a, const TransitionType& b) {
                    return a.priority > b.priority;
                });

            for (const auto& transition : sorted) {
                if (transition.should_transition(ctx)) {
                    force_transition(transition.to_state, ctx);
                    return;
                }
            }
        }
    }

    /// Force transition to a state (bypasses conditions)
    void force_transition(StateId to, Context& ctx) {
        if (!m_started) {
            m_current_state = to;
            start(ctx);
            return;
        }

        // Exit current state
        if (auto* state = get_state(m_current_state)) {
            state->on_exit(ctx);
        }

        // Update history
        m_previous_state = m_current_state;
        m_current_state = to;

        // Enter new state
        enter_state(to, ctx);
    }

    /// Reset to initial state
    void reset(Context& ctx) {
        if (m_started && m_current_state != m_initial_state) {
            force_transition(m_initial_state, ctx);
        }
        m_previous_state = std::nullopt;
    }

    /// Get current state ID
    [[nodiscard]] StateId current_state() const { return m_current_state; }

    /// Get previous state ID (if any)
    [[nodiscard]] std::optional<StateId> previous_state() const { return m_previous_state; }

    /// Check if in a specific state
    [[nodiscard]] bool is_in_state(StateId state) const {
        return m_current_state == state;
    }

    /// Get current state object
    [[nodiscard]] IState<Context>* current() {
        return get_state(m_current_state);
    }

    [[nodiscard]] const IState<Context>* current() const {
        return get_state(m_current_state);
    }

    /// Get state object by ID
    [[nodiscard]] IState<Context>* get_state(StateId id) {
        auto it = m_states.find(id);
        return it != m_states.end() ? it->second.get() : nullptr;
    }

    [[nodiscard]] const IState<Context>* get_state(StateId id) const {
        auto it = m_states.find(id);
        return it != m_states.end() ? it->second.get() : nullptr;
    }

    /// Check if started
    [[nodiscard]] bool is_started() const { return m_started; }

    // =========================================================================
    // Hot-Reload Support
    // =========================================================================

    /// Snapshot for hot-reload
    struct Snapshot {
        StateId current_state;
        std::optional<StateId> previous_state;
        bool started = false;
    };

    /// Take a snapshot of current state
    [[nodiscard]] Snapshot take_snapshot() const {
        return Snapshot{m_current_state, m_previous_state, m_started};
    }

    /// Restore from a snapshot
    void apply_snapshot(const Snapshot& snapshot, Context& ctx) {
        // Exit current state if running
        if (m_started && m_current_state != snapshot.current_state) {
            if (auto* state = get_state(m_current_state)) {
                state->on_exit(ctx);
            }
        }

        m_current_state = snapshot.current_state;
        m_previous_state = snapshot.previous_state;
        m_started = snapshot.started;

        // Enter restored state if was running
        if (m_started) {
            enter_state(m_current_state, ctx);
        }
    }

private:
    void enter_state(StateId id, Context& ctx) {
        if (auto* state = get_state(id)) {
            state->on_enter(ctx);
        }
    }

    void sort_global_transitions() {
        m_sorted_global_transitions = m_global_transitions;
        std::sort(m_sorted_global_transitions.begin(), m_sorted_global_transitions.end(),
            [](const TransitionType& a, const TransitionType& b) {
                return a.priority > b.priority;
            });
    }

    StateId m_current_state;
    StateId m_initial_state;
    std::optional<StateId> m_previous_state;
    bool m_started = false;

    std::unordered_map<StateId, StatePtr> m_states;
    std::unordered_map<StateId, std::vector<TransitionType>> m_transitions;
    std::vector<TransitionType> m_global_transitions;
    std::vector<TransitionType> m_sorted_global_transitions;
};

// =============================================================================
// Simple State Implementation
// =============================================================================

/// Simple state enum for basic use cases
enum class SimpleStateId : std::uint32_t {
    Idle = 0,
    Active,
    Paused,
    Custom1,
    Custom2,
    Custom3,
    Custom4
};

/// Lambda-based state for quick prototyping
template<typename Context>
class LambdaState : public IState<Context> {
public:
    using EnterFn = std::function<void(Context&)>;
    using ExitFn = std::function<void(Context&)>;
    using UpdateFn = std::function<bool(Context&, float)>;

    explicit LambdaState(std::string name)
        : m_name(std::move(name)) {}

    LambdaState(std::string name, EnterFn on_enter, ExitFn on_exit, UpdateFn on_update)
        : m_name(std::move(name))
        , m_on_enter(std::move(on_enter))
        , m_on_exit(std::move(on_exit))
        , m_on_update(std::move(on_update)) {}

    void on_enter(Context& ctx) override {
        if (m_on_enter) m_on_enter(ctx);
    }

    void on_exit(Context& ctx) override {
        if (m_on_exit) m_on_exit(ctx);
    }

    bool on_update(Context& ctx, float dt) override {
        return m_on_update ? m_on_update(ctx, dt) : true;
    }

    [[nodiscard]] std::string_view name() const override { return m_name; }

    /// Set enter callback
    LambdaState& set_on_enter(EnterFn fn) { m_on_enter = std::move(fn); return *this; }

    /// Set exit callback
    LambdaState& set_on_exit(ExitFn fn) { m_on_exit = std::move(fn); return *this; }

    /// Set update callback
    LambdaState& set_on_update(UpdateFn fn) { m_on_update = std::move(fn); return *this; }

private:
    std::string m_name;
    EnterFn m_on_enter;
    ExitFn m_on_exit;
    UpdateFn m_on_update;
};

// =============================================================================
// State Machine Builder
// =============================================================================

/// Fluent builder for state machines
template<typename StateId, typename Context>
class StateMachineBuilder {
public:
    using Machine = StateMachine<StateId, Context>;
    using StatePtr = typename Machine::StatePtr;

    explicit StateMachineBuilder(StateId initial)
        : m_machine(std::make_unique<Machine>(initial))
        , m_current_state(initial) {}

    /// Add a state
    StateMachineBuilder& state(StateId id, StatePtr state) {
        m_machine->register_state(id, std::move(state));
        m_current_state = id;
        return *this;
    }

    /// Add a lambda state
    StateMachineBuilder& lambda_state(StateId id, const std::string& name) {
        m_machine->register_state(id, std::make_unique<LambdaState<Context>>(name));
        m_current_state = id;
        return *this;
    }

    /// Add transition from current state
    StateMachineBuilder& transition_to(StateId to, TransitionCondition<Context> condition) {
        m_machine->add_transition(m_current_state, to, std::move(condition));
        return *this;
    }

    /// Add transition with priority from current state
    StateMachineBuilder& transition_to_priority(StateId to, TransitionCondition<Context> condition, std::int32_t priority) {
        m_machine->add_transition_priority(m_current_state, to, std::move(condition), priority);
        return *this;
    }

    /// Add transition between specific states
    StateMachineBuilder& transition(StateId from, StateId to, TransitionCondition<Context> condition) {
        m_machine->add_transition(from, to, std::move(condition));
        return *this;
    }

    /// Add global transition
    StateMachineBuilder& global_transition(StateId to, TransitionCondition<Context> condition) {
        m_machine->add_global_transition(to, std::move(condition));
        return *this;
    }

    /// Build the state machine
    [[nodiscard]] std::unique_ptr<Machine> build() {
        return std::move(m_machine);
    }

private:
    std::unique_ptr<Machine> m_machine;
    StateId m_current_state;
};

/// Create a state machine builder
template<typename StateId, typename Context>
StateMachineBuilder<StateId, Context> make_state_machine(StateId initial) {
    return StateMachineBuilder<StateId, Context>(initial);
}

// =============================================================================
// String-based State Machine (for data-driven FSM)
// =============================================================================

/// String-identified state for data-driven FSMs
using StringStateId = std::string;

/// Data-driven state that can be configured from TOML/JSON
template<typename Context>
class DataDrivenState : public IState<Context> {
public:
    explicit DataDrivenState(std::string name)
        : m_name(std::move(name)) {}

    [[nodiscard]] std::string_view name() const override { return m_name; }

    /// State configuration
    struct Config {
        std::string animation;
        float duration = 0;
        float speed = 1.0f;
        std::unordered_map<std::string, std::string> properties;
    };

    void set_config(const Config& config) { m_config = config; }
    [[nodiscard]] const Config& config() const { return m_config; }

    void on_enter(Context& ctx) override {
        m_time_in_state = 0;
        if (m_enter_callback) m_enter_callback(ctx, m_config);
    }

    void on_exit(Context& ctx) override {
        if (m_exit_callback) m_exit_callback(ctx, m_config);
    }

    bool on_update(Context& ctx, float dt) override {
        m_time_in_state += dt;
        if (m_update_callback) return m_update_callback(ctx, m_config, dt);
        return true;
    }

    [[nodiscard]] float time_in_state() const { return m_time_in_state; }

    using EnterCallback = std::function<void(Context&, const Config&)>;
    using ExitCallback = std::function<void(Context&, const Config&)>;
    using UpdateCallback = std::function<bool(Context&, const Config&, float)>;

    void set_enter_callback(EnterCallback cb) { m_enter_callback = std::move(cb); }
    void set_exit_callback(ExitCallback cb) { m_exit_callback = std::move(cb); }
    void set_update_callback(UpdateCallback cb) { m_update_callback = std::move(cb); }

private:
    std::string m_name;
    Config m_config;
    float m_time_in_state = 0;

    EnterCallback m_enter_callback;
    ExitCallback m_exit_callback;
    UpdateCallback m_update_callback;
};

/// Convenience alias for string-based state machines
template<typename Context>
using StringStateMachine = StateMachine<StringStateId, Context>;

} // namespace void_ai
