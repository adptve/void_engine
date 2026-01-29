#pragma once

/// @file execution.hpp
/// @brief Graph execution engine and compiler

#include "graph.hpp"

#include <chrono>
#include <deque>
#include <memory>
#include <queue>
#include <vector>

namespace void_graph {

// =============================================================================
// Node Executor Interface
// =============================================================================

/// @brief Interface for custom node execution strategies
class INodeExecutor {
public:
    virtual ~INodeExecutor() = default;

    /// @brief Execute a node
    virtual PinId execute(INode& node, ExecutionContext& ctx) = 0;

    /// @brief Pre-execution hook
    virtual void pre_execute([[maybe_unused]] INode& node, [[maybe_unused]] ExecutionContext& ctx) {}

    /// @brief Post-execution hook
    virtual void post_execute([[maybe_unused]] INode& node, [[maybe_unused]] ExecutionContext& ctx) {}
};

/// @brief Default node executor
class DefaultNodeExecutor : public INodeExecutor {
public:
    PinId execute(INode& node, ExecutionContext& ctx) override;
};

// =============================================================================
// Execution Frame
// =============================================================================

/// @brief A frame in the execution stack
struct ExecutionFrame {
    NodeId node_id;
    PinId exec_pin;         ///< Which exec output to follow
    std::size_t sequence_index = 0;  ///< For sequence nodes
    bool is_resuming = false;
    std::chrono::steady_clock::time_point started_at;
};

// =============================================================================
// Latent Action
// =============================================================================

/// @brief A suspended latent action
struct LatentAction {
    ExecutionId execution_id;
    NodeId node_id;
    float remaining_time = 0.0f;
    std::function<bool()> completion_predicate;
    std::function<void()> on_complete;
    std::chrono::steady_clock::time_point started_at;
};

// =============================================================================
// Graph Executor
// =============================================================================

/// @brief Executes graphs at runtime
class GraphExecutor {
public:
    GraphExecutor();
    ~GraphExecutor();

    // ==========================================================================
    // Execution Control
    // ==========================================================================

    /// @brief Start executing from an event node
    /// @return Execution ID for tracking
    ExecutionId start(GraphInstance& instance, EventNode& event);

    /// @brief Start executing from a specific node
    ExecutionId start(GraphInstance& instance, NodeId start_node);

    /// @brief Update all running executions
    /// @param delta_time Time since last update
    void update(float delta_time);

    /// @brief Pause an execution
    void pause(ExecutionId id);

    /// @brief Resume a paused execution
    void resume(ExecutionId id);

    /// @brief Abort an execution
    void abort(ExecutionId id);

    /// @brief Check if execution is running
    [[nodiscard]] bool is_running(ExecutionId id) const;

    /// @brief Get execution state
    [[nodiscard]] ExecutionState get_state(ExecutionId id) const;

    /// @brief Get execution result (if completed)
    [[nodiscard]] const ExecutionResult* get_result(ExecutionId id) const;

    // ==========================================================================
    // Pin Values
    // ==========================================================================

    /// @brief Get the value of a pin
    template <typename T>
    [[nodiscard]] T get_pin_value(ExecutionContext& ctx, PinId pin) const;

    /// @brief Set the value of a pin
    template <typename T>
    void set_pin_value(ExecutionContext& ctx, PinId pin, T&& value);

    /// @brief Compute the value of an input pin (pulls from connected output)
    PinValue compute_input_value(ExecutionContext& ctx, const Pin& input_pin);

    // ==========================================================================
    // Custom Executors
    // ==========================================================================

    /// @brief Set custom node executor
    void set_node_executor(std::unique_ptr<INodeExecutor> executor);

    /// @brief Set node executor for specific node type
    void set_node_executor(NodeTypeId type_id, std::unique_ptr<INodeExecutor> executor);

    // ==========================================================================
    // Debugging
    // ==========================================================================

    /// @brief Enable debug mode
    void set_debug_enabled(bool enabled) { debug_enabled_ = enabled; }
    [[nodiscard]] bool debug_enabled() const { return debug_enabled_; }

    /// @brief Add breakpoint
    void add_breakpoint(GraphId graph, NodeId node);

    /// @brief Remove breakpoint
    void remove_breakpoint(GraphId graph, NodeId node);

    /// @brief Clear all breakpoints
    void clear_breakpoints();

    /// @brief Step to next node (while paused at breakpoint)
    void step_into(ExecutionId id);

    /// @brief Step over (skip into subgraphs)
    void step_over(ExecutionId id);

    /// @brief Step out of current subgraph
    void step_out(ExecutionId id);

    /// @brief Set breakpoint hit callback
    using BreakpointCallback = std::function<void(ExecutionId, NodeId)>;
    void set_breakpoint_callback(BreakpointCallback callback);

    // ==========================================================================
    // Statistics
    // ==========================================================================

    struct ExecutionStats {
        std::size_t total_nodes_executed = 0;
        std::size_t active_executions = 0;
        std::size_t latent_actions = 0;
        float average_execution_time_ms = 0.0f;
        std::size_t peak_call_depth = 0;
    };

    [[nodiscard]] ExecutionStats stats() const;

    // ==========================================================================
    // Latent Actions
    // ==========================================================================

    /// @brief Register a latent action
    void register_latent_action(ExecutionContext& ctx, float duration,
                                 std::function<void()> on_complete = nullptr);

    /// @brief Register a latent action with predicate
    void register_latent_action(ExecutionContext& ctx,
                                 std::function<bool()> completion_predicate,
                                 std::function<void()> on_complete = nullptr);

private:
    /// @brief Internal execution data
    struct ExecutionData {
        ExecutionId id;
        GraphInstance* instance = nullptr;
        ExecutionState state = ExecutionState::Idle;
        std::vector<ExecutionFrame> stack;
        ExecutionResult result;
        std::chrono::steady_clock::time_point started_at;
    };

    /// @brief Execute nodes until suspension or completion
    void run_execution(ExecutionData& data);

    /// @brief Execute a single node
    PinId execute_node(ExecutionData& data, INode& node);

    /// @brief Handle latent actions
    void update_latent_actions(float delta_time);

    /// @brief Check for breakpoint
    bool check_breakpoint(ExecutionData& data, NodeId node);

    std::unordered_map<ExecutionId, ExecutionData> executions_;
    std::deque<LatentAction> latent_actions_;

    std::unique_ptr<INodeExecutor> default_executor_;
    std::unordered_map<NodeTypeId, std::unique_ptr<INodeExecutor>> custom_executors_;

    std::unordered_map<GraphId, std::unordered_set<NodeId>> breakpoints_;
    BreakpointCallback breakpoint_callback_;

    bool debug_enabled_ = false;
    ExecutionStats stats_;

    inline static std::uint32_t next_execution_id_ = 1;
};

// =============================================================================
// Compiled Instruction
// =============================================================================

/// @brief A compiled instruction for the VM
struct CompiledInstruction {
    enum class OpCode : std::uint8_t {
        // Flow control
        Nop,            ///< No operation
        Jump,           ///< Jump to address
        JumpIf,         ///< Conditional jump
        JumpIfNot,      ///< Conditional jump (inverted)
        Call,           ///< Call subgraph
        Return,         ///< Return from subgraph

        // Node execution
        Execute,        ///< Execute node
        ExecutePure,    ///< Execute pure node (cacheable)

        // Value operations
        LoadConst,      ///< Load constant to register
        LoadVar,        ///< Load variable to register
        StoreVar,       ///< Store register to variable
        LoadPin,        ///< Load pin value
        StorePin,       ///< Store to pin value
        Copy,           ///< Copy register

        // Math
        Add,
        Sub,
        Mul,
        Div,
        Neg,
        And,
        Or,
        Not,
        Eq,
        Ne,
        Lt,
        Le,
        Gt,
        Ge,

        // Latent
        Suspend,        ///< Suspend execution
        WaitFrame,      ///< Wait for next frame
        WaitTime,       ///< Wait for duration

        // Debug
        Breakpoint,     ///< Breakpoint
        Trace           ///< Debug trace
    };

    OpCode op = OpCode::Nop;
    std::uint32_t arg1 = 0;
    std::uint32_t arg2 = 0;
    std::uint32_t arg3 = 0;

    // Extended data for complex instructions
    PinValue immediate;
};

// =============================================================================
// Compiled Graph
// =============================================================================

/// @brief A compiled graph ready for fast execution
class CompiledGraph {
public:
    CompiledGraph() = default;

    /// @brief Get instructions
    [[nodiscard]] std::span<const CompiledInstruction> instructions() const {
        return instructions_;
    }

    /// @brief Get entry point for an event
    [[nodiscard]] std::optional<std::size_t> get_entry_point(const std::string& event_name) const;

    /// @brief Get constant value
    [[nodiscard]] const PinValue& get_constant(std::size_t index) const;

    /// @brief Get required register count
    [[nodiscard]] std::size_t register_count() const { return register_count_; }

    /// @brief Get source graph ID
    [[nodiscard]] GraphId source_graph() const { return source_graph_; }

    /// @brief Validation info
    [[nodiscard]] bool is_valid() const { return is_valid_; }
    [[nodiscard]] const std::string& validation_error() const { return validation_error_; }

private:
    friend class GraphCompiler;

    GraphId source_graph_;
    std::vector<CompiledInstruction> instructions_;
    std::unordered_map<std::string, std::size_t> entry_points_;
    std::vector<PinValue> constants_;
    std::size_t register_count_ = 0;
    bool is_valid_ = true;
    std::string validation_error_;
};

// =============================================================================
// Graph Compiler
// =============================================================================

/// @brief Compiles graphs to bytecode for faster execution
class GraphCompiler {
public:
    /// @brief Compilation options
    struct Options {
        OptimizationLevel optimization = OptimizationLevel::Development;
        bool emit_debug_info = true;
        bool validate_types = true;
        bool fold_constants = true;
        bool eliminate_dead_code = true;
        bool inline_pure_nodes = true;
        std::size_t max_inline_depth = 3;
    };

    GraphCompiler();
    explicit GraphCompiler(const Options& options);

    /// @brief Compile a graph
    [[nodiscard]] GraphResult<CompiledGraph> compile(const Graph& graph);

    /// @brief Compile a graph with specific entry points
    [[nodiscard]] GraphResult<CompiledGraph> compile(const Graph& graph,
                                                       std::span<const std::string> events);

    /// @brief Get compilation errors
    [[nodiscard]] const std::vector<std::string>& errors() const { return errors_; }

    /// @brief Get compilation warnings
    [[nodiscard]] const std::vector<std::string>& warnings() const { return warnings_; }

    /// @brief Set options
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const { return options_; }

private:
    /// @brief Compile a node
    void compile_node(const Graph& graph, INode& node, CompiledGraph& output);

    /// @brief Compile flow from an event
    void compile_event(const Graph& graph, EventNode& event, CompiledGraph& output);

    /// @brief Allocate a register
    std::uint32_t allocate_register();

    /// @brief Emit an instruction
    void emit(CompiledGraph& output, CompiledInstruction instr);

    /// @brief Optimize the compiled graph
    void optimize(CompiledGraph& output);

    /// @brief Constant folding pass
    void fold_constants(CompiledGraph& output);

    /// @brief Dead code elimination pass
    void eliminate_dead_code(CompiledGraph& output);

    Options options_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
    std::uint32_t next_register_ = 0;
    std::unordered_map<NodeId, std::size_t> node_addresses_;
};

// =============================================================================
// Compiled Graph Executor
// =============================================================================

/// @brief Fast executor for compiled graphs
class CompiledGraphExecutor {
public:
    CompiledGraphExecutor();

    /// @brief Execute a compiled graph from an entry point
    ExecutionResult execute(const CompiledGraph& graph,
                            const std::string& entry_point,
                            ExecutionContext& ctx);

    /// @brief Update latent actions
    void update(float delta_time);

    /// @brief Set debug enabled
    void set_debug_enabled(bool enabled) { debug_enabled_ = enabled; }

private:
    /// @brief Execute instruction
    bool execute_instruction(const CompiledInstruction& instr,
                             ExecutionContext& ctx,
                             std::size_t& ip);

    std::vector<PinValue> registers_;
    std::deque<LatentAction> latent_actions_;
    bool debug_enabled_ = false;
};

} // namespace void_graph
