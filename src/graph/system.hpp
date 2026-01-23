#pragma once

/// @file system.hpp
/// @brief Main graph system integrating all components

#include "registry.hpp"

#include <void_engine/ecs/ecs.hpp>
#include <void_engine/event/event.hpp>

namespace void_graph {

// =============================================================================
// Graph Component
// =============================================================================

/// @brief ECS component for entities with graph execution
struct GraphComponent {
    GraphId graph_id;
    std::unique_ptr<GraphInstance> instance;
    bool auto_tick = true;
    bool enabled = true;

    // Event bindings
    std::unordered_map<std::string, EventNode*> event_bindings;

    // Active executions
    std::vector<ExecutionId> active_executions;
};

// =============================================================================
// Graph Events
// =============================================================================

/// @brief Event: Graph execution started
struct GraphExecutionStartedEvent {
    GraphId graph_id;
    ExecutionId execution_id;
    std::uint64_t entity_id;
};

/// @brief Event: Graph execution completed
struct GraphExecutionCompletedEvent {
    GraphId graph_id;
    ExecutionId execution_id;
    std::uint64_t entity_id;
    ExecutionState final_state;
};

/// @brief Event: Graph breakpoint hit
struct GraphBreakpointEvent {
    GraphId graph_id;
    ExecutionId execution_id;
    NodeId node_id;
};

/// @brief Event: Node executed (for debugging)
struct NodeExecutedEvent {
    GraphId graph_id;
    NodeId node_id;
    float execution_time_ms;
};

// =============================================================================
// Graph System
// =============================================================================

/// @brief Main system for visual scripting
class GraphSystem {
public:
    GraphSystem();
    ~GraphSystem();

    // Singleton access
    [[nodiscard]] static GraphSystem& instance();
    [[nodiscard]] static GraphSystem* instance_ptr();

    // ==========================================================================
    // Initialization
    // ==========================================================================

    /// @brief Initialize the graph system
    void initialize();

    /// @brief Shutdown the graph system
    void shutdown();

    /// @brief Check if initialized
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    // ==========================================================================
    // Subsystems
    // ==========================================================================

    /// @brief Get the node registry
    [[nodiscard]] NodeRegistry& registry() { return registry_; }
    [[nodiscard]] const NodeRegistry& registry() const { return registry_; }

    /// @brief Get the graph library
    [[nodiscard]] GraphLibrary& library() { return library_; }
    [[nodiscard]] const GraphLibrary& library() const { return library_; }

    /// @brief Get the graph executor
    [[nodiscard]] GraphExecutor& executor() { return executor_; }
    [[nodiscard]] const GraphExecutor& executor() const { return executor_; }

    /// @brief Get the graph compiler
    [[nodiscard]] GraphCompiler& compiler() { return compiler_; }
    [[nodiscard]] const GraphCompiler& compiler() const { return compiler_; }

    // ==========================================================================
    // Graph Management
    // ==========================================================================

    /// @brief Create a new graph
    [[nodiscard]] Graph* create_graph(const std::string& name);

    /// @brief Create a new graph builder
    [[nodiscard]] GraphBuilder create_graph_builder(const std::string& name = "");

    /// @brief Load a graph from file
    [[nodiscard]] Graph* load_graph(const std::filesystem::path& path);

    /// @brief Save a graph to file
    bool save_graph(GraphId id, const std::filesystem::path& path);

    /// @brief Get a graph
    [[nodiscard]] Graph* get_graph(GraphId id);

    /// @brief Delete a graph
    bool delete_graph(GraphId id);

    // ==========================================================================
    // Entity Integration
    // ==========================================================================

    /// @brief Attach a graph to an entity
    GraphComponent* attach_graph(std::uint64_t entity_id, GraphId graph_id);

    /// @brief Detach a graph from an entity
    void detach_graph(std::uint64_t entity_id);

    /// @brief Get the graph component for an entity
    [[nodiscard]] GraphComponent* get_component(std::uint64_t entity_id);

    /// @brief Trigger an event on an entity's graph
    ExecutionId trigger_event(std::uint64_t entity_id, const std::string& event_name);

    // ==========================================================================
    // Execution
    // ==========================================================================

    /// @brief Update all graph executions
    void update(float delta_time);

    /// @brief Execute a graph immediately
    ExecutionResult execute_sync(GraphId graph_id, const std::string& entry_point);

    /// @brief Start async execution
    ExecutionId execute_async(GraphId graph_id, const std::string& entry_point);

    /// @brief Compile a graph for faster execution
    [[nodiscard]] CompiledGraph* compile_graph(GraphId id);

    // ==========================================================================
    // Debugging
    // ==========================================================================

    /// @brief Enable debug mode for all executions
    void set_debug_mode(bool enabled);
    [[nodiscard]] bool debug_mode() const { return debug_mode_; }

    /// @brief Toggle breakpoint
    void toggle_breakpoint(GraphId graph, NodeId node);

    /// @brief Step through execution
    void step(ExecutionId id);

    /// @brief Continue execution after breakpoint
    void continue_execution(ExecutionId id);

    /// @brief Get execution call stack
    [[nodiscard]] std::vector<NodeId> get_call_stack(ExecutionId id) const;

    /// @brief Get active executions
    [[nodiscard]] std::vector<ExecutionId> get_active_executions() const;

    // ==========================================================================
    // Hot Reload
    // ==========================================================================

    /// @brief Reload a graph from file
    bool hot_reload(GraphId id);

    /// @brief Enable hot reload watching
    void enable_hot_reload(bool enabled);

    /// @brief Check for file changes
    void check_hot_reload();

    // ==========================================================================
    // Events
    // ==========================================================================

    /// @brief Set event bus for graph events
    void set_event_bus(void_event::EventBus* bus) { event_bus_ = bus; }

    /// @brief Get event bus
    [[nodiscard]] void_event::EventBus* event_bus() const { return event_bus_; }

    // ==========================================================================
    // Statistics
    // ==========================================================================

    struct Stats {
        std::size_t total_graphs = 0;
        std::size_t total_nodes = 0;
        std::size_t active_executions = 0;
        std::size_t compiled_graphs = 0;
        float avg_execution_time_ms = 0.0f;
    };

    [[nodiscard]] Stats stats() const;

private:
    NodeRegistry registry_;
    GraphLibrary library_;
    GraphExecutor executor_;
    GraphCompiler compiler_;

    std::unordered_map<GraphId, std::unique_ptr<CompiledGraph>> compiled_graphs_;
    std::unordered_map<std::uint64_t, GraphComponent> entity_components_;

    void_event::EventBus* event_bus_ = nullptr;

    bool initialized_ = false;
    bool debug_mode_ = false;
    bool hot_reload_enabled_ = false;

    std::unordered_map<GraphId, std::filesystem::path> graph_paths_;
    std::unordered_map<GraphId, std::filesystem::file_time_type> graph_timestamps_;
};

// =============================================================================
// Prelude Namespace
// =============================================================================

/// @brief Convenient imports for common usage
namespace prelude {

using void_graph::Graph;
using void_graph::GraphBuilder;
using void_graph::GraphId;
using void_graph::GraphSystem;
using void_graph::NodeId;
using void_graph::PinId;
using void_graph::PinType;

using void_graph::INode;
using void_graph::EventNode;
using void_graph::FunctionNode;
using void_graph::BranchNode;
using void_graph::SequenceNode;
using void_graph::ForLoopNode;
using void_graph::DelayNode;
using void_graph::MathNode;

using void_graph::NodeBuilder;
using void_graph::NodeRegistry;
using void_graph::GraphLibrary;

using void_graph::ExecutionContext;
using void_graph::ExecutionResult;
using void_graph::GraphExecutor;

namespace builtin = void_graph::builtin;

} // namespace prelude

} // namespace void_graph
