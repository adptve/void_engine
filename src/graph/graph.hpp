#pragma once

/// @file graph.hpp
/// @brief Graph container and management

#include "node.hpp"

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace void_graph {

// =============================================================================
// Graph
// =============================================================================

/// @brief Container for nodes and connections
class Graph {
public:
    explicit Graph(GraphId id = GraphId{}, const std::string& name = "Graph");
    ~Graph() = default;

    // Non-copyable, movable
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;
    Graph(Graph&&) = default;
    Graph& operator=(Graph&&) = default;

    // Identity
    [[nodiscard]] GraphId id() const { return id_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }
    [[nodiscard]] GraphType type() const { return type_; }
    void set_type(GraphType type) { type_ = type; }

    // Metadata
    [[nodiscard]] GraphMetadata& metadata() { return metadata_; }
    [[nodiscard]] const GraphMetadata& metadata() const { return metadata_; }

    // ==========================================================================
    // Node Management
    // ==========================================================================

    /// @brief Add a node to the graph
    /// @return The added node
    INode* add_node(std::unique_ptr<INode> node);

    /// @brief Create a node from a template
    /// @return The created node or nullptr
    INode* create_node(const NodeTemplate& tmpl, float x = 0.0f, float y = 0.0f);

    /// @brief Create a node by type ID
    INode* create_node(NodeTypeId type_id, float x = 0.0f, float y = 0.0f);

    /// @brief Remove a node (and all its connections)
    bool remove_node(NodeId id);

    /// @brief Get a node by ID
    [[nodiscard]] INode* get_node(NodeId id);
    [[nodiscard]] const INode* get_node(NodeId id) const;

    /// @brief Get all nodes
    [[nodiscard]] std::span<const std::unique_ptr<INode>> nodes() const;

    /// @brief Get node count
    [[nodiscard]] std::size_t node_count() const { return nodes_.size(); }

    /// @brief Find nodes by category
    [[nodiscard]] std::vector<INode*> find_nodes_by_category(NodeCategory category) const;

    /// @brief Find nodes by type ID
    [[nodiscard]] std::vector<INode*> find_nodes_by_type(NodeTypeId type_id) const;

    /// @brief Get all event nodes
    [[nodiscard]] std::vector<EventNode*> get_event_nodes() const;

    // ==========================================================================
    // Connection Management
    // ==========================================================================

    /// @brief Connect two pins
    /// @return Connection ID or error
    GraphResult<ConnectionId> connect(PinId source, PinId target);

    /// @brief Disconnect a connection
    bool disconnect(ConnectionId id);

    /// @brief Disconnect all connections for a pin
    void disconnect_pin(PinId id);

    /// @brief Disconnect all connections for a node
    void disconnect_node(NodeId id);

    /// @brief Check if a connection would be valid
    [[nodiscard]] GraphResult<void> can_connect(PinId source, PinId target) const;

    /// @brief Get a connection by ID
    [[nodiscard]] const Connection* get_connection(ConnectionId id) const;

    /// @brief Get all connections
    [[nodiscard]] std::span<const Connection> connections() const;

    /// @brief Get connections for a pin
    [[nodiscard]] std::vector<const Connection*> get_connections_for_pin(PinId id) const;

    /// @brief Get connections for a node
    [[nodiscard]] std::vector<const Connection*> get_connections_for_node(NodeId id) const;

    /// @brief Get the connected output pin for an input pin
    [[nodiscard]] PinId get_connected_output(PinId input_pin) const;

    /// @brief Get all connected input pins for an output pin
    [[nodiscard]] std::vector<PinId> get_connected_inputs(PinId output_pin) const;

    /// @brief Check for cycles (returns true if adding connection would create cycle)
    [[nodiscard]] bool would_create_cycle(PinId source, PinId target) const;

    // ==========================================================================
    // Variable Management
    // ==========================================================================

    /// @brief Add a variable
    VariableId add_variable(const GraphVariable& var);

    /// @brief Remove a variable
    bool remove_variable(VariableId id);

    /// @brief Get a variable
    [[nodiscard]] GraphVariable* get_variable(VariableId id);
    [[nodiscard]] const GraphVariable* get_variable(VariableId id) const;

    /// @brief Find variable by name
    [[nodiscard]] GraphVariable* find_variable(const std::string& name);

    /// @brief Get all variables
    [[nodiscard]] std::span<const GraphVariable> variables() const;

    /// @brief Create getter node for variable
    VariableNode* create_getter(VariableId var_id, float x = 0.0f, float y = 0.0f);

    /// @brief Create setter node for variable
    VariableNode* create_setter(VariableId var_id, float x = 0.0f, float y = 0.0f);

    // ==========================================================================
    // Interface Pins (for subgraphs/functions)
    // ==========================================================================

    /// @brief Add an interface input (exposed as pin when used as subgraph)
    void add_interface_input(const std::string& name, PinType type);

    /// @brief Add an interface output
    void add_interface_output(const std::string& name, PinType type);

    /// @brief Get interface inputs
    [[nodiscard]] std::span<const Pin> interface_inputs() const { return interface_inputs_; }

    /// @brief Get interface outputs
    [[nodiscard]] std::span<const Pin> interface_outputs() const { return interface_outputs_; }

    // ==========================================================================
    // Utility
    // ==========================================================================

    /// @brief Clear the graph
    void clear();

    /// @brief Validate the graph
    [[nodiscard]] GraphResult<void> validate() const;

    /// @brief Get all orphaned nodes (no connections)
    [[nodiscard]] std::vector<NodeId> get_orphaned_nodes() const;

    /// @brief Compute execution order for all nodes
    [[nodiscard]] std::vector<NodeId> compute_execution_order() const;

    /// @brief Clone the graph
    [[nodiscard]] std::unique_ptr<Graph> clone() const;

    // ==========================================================================
    // Serialization
    // ==========================================================================

    /// @brief Serialize to JSON
    [[nodiscard]] std::string to_json() const;

    /// @brief Deserialize from JSON
    static std::unique_ptr<Graph> from_json(const std::string& json, const NodeRegistry& registry);

    /// @brief Load from TOML blueprint file
    static std::unique_ptr<Graph> from_toml(const std::string& toml, const NodeRegistry& registry);

    /// @brief Load from file (auto-detects format)
    static std::unique_ptr<Graph> load(const std::filesystem::path& path, const NodeRegistry& registry);

    /// @brief Serialize to binary
    void serialize(std::ostream& out) const;

    /// @brief Deserialize from binary
    static std::unique_ptr<Graph> deserialize(std::istream& in, const NodeRegistry& registry);

    // ==========================================================================
    // Event Callbacks
    // ==========================================================================

    using NodeCallback = std::function<void(INode*)>;
    using ConnectionCallback = std::function<void(const Connection&)>;

    void set_on_node_added(NodeCallback cb) { on_node_added_ = std::move(cb); }
    void set_on_node_removed(NodeCallback cb) { on_node_removed_ = std::move(cb); }
    void set_on_connection_added(ConnectionCallback cb) { on_connection_added_ = std::move(cb); }
    void set_on_connection_removed(ConnectionCallback cb) { on_connection_removed_ = std::move(cb); }

private:
    /// @brief Find which node owns a pin
    [[nodiscard]] INode* find_pin_owner(PinId pin_id) const;

    /// @brief Check cycle detection using DFS
    [[nodiscard]] bool has_cycle_dfs(NodeId start, NodeId target,
                                      std::unordered_set<NodeId>& visited) const;

    GraphId id_;
    std::string name_;
    GraphType type_ = GraphType::Event;
    GraphMetadata metadata_;

    std::vector<std::unique_ptr<INode>> nodes_;
    std::unordered_map<NodeId, std::size_t> node_index_;

    std::vector<Connection> connections_;
    std::unordered_map<ConnectionId, std::size_t> connection_index_;
    std::unordered_map<PinId, std::vector<ConnectionId>> pin_connections_;

    std::vector<GraphVariable> variables_;
    std::unordered_map<VariableId, std::size_t> variable_index_;

    std::vector<Pin> interface_inputs_;
    std::vector<Pin> interface_outputs_;

    NodeCallback on_node_added_;
    NodeCallback on_node_removed_;
    ConnectionCallback on_connection_added_;
    ConnectionCallback on_connection_removed_;

    inline static std::uint32_t next_node_id_ = 1;
    inline static std::uint32_t next_connection_id_ = 1;
    inline static std::uint32_t next_variable_id_ = 1;
};

// =============================================================================
// Subgraph
// =============================================================================

/// @brief A reusable subgraph that can be embedded in other graphs
class Subgraph : public Graph {
public:
    Subgraph(SubgraphId id, const std::string& name);

    [[nodiscard]] SubgraphId subgraph_id() const { return subgraph_id_; }

    /// @brief Entry node for the subgraph
    [[nodiscard]] EventNode* entry_node() const { return entry_node_; }

    /// @brief Exit node for the subgraph
    [[nodiscard]] INode* exit_node() const { return exit_node_; }

private:
    SubgraphId subgraph_id_;
    EventNode* entry_node_ = nullptr;
    INode* exit_node_ = nullptr;
};

// =============================================================================
// Graph Instance
// =============================================================================

/// @brief A runtime instance of a graph
class GraphInstance {
public:
    GraphInstance(const Graph& graph, std::uint64_t owner_entity = 0);

    /// @brief Get the source graph
    [[nodiscard]] const Graph& graph() const { return graph_; }

    /// @brief Get the execution context
    [[nodiscard]] ExecutionContext& context() { return context_; }
    [[nodiscard]] const ExecutionContext& context() const { return context_; }

    /// @brief Get a variable value
    template <typename T>
    [[nodiscard]] T get_variable(VariableId id) const;

    /// @brief Set a variable value
    template <typename T>
    void set_variable(VariableId id, T&& value);

    /// @brief Reset all variables to defaults
    void reset_variables();

    /// @brief Reset execution state
    void reset_execution();

private:
    const Graph& graph_;
    ExecutionContext context_;
};

// =============================================================================
// Graph Builder
// =============================================================================

/// @brief Fluent builder for creating graphs
class GraphBuilder {
public:
    GraphBuilder();
    explicit GraphBuilder(const std::string& name);

    /// @brief Set graph name
    GraphBuilder& name(const std::string& name);

    /// @brief Set graph type
    GraphBuilder& type(GraphType type);

    /// @brief Add a node
    GraphBuilder& node(std::unique_ptr<INode> node);

    /// @brief Add a node and get reference
    template <typename T, typename... Args>
    GraphBuilder& node(T*& out_ref, Args&&... args);

    /// @brief Connect pins
    GraphBuilder& connect(PinId source, PinId target);

    /// @brief Add a variable
    GraphBuilder& variable(const std::string& name, PinType type);

    /// @brief Add a variable with default value
    template <typename T>
    GraphBuilder& variable(const std::string& name, PinType type, T&& default_value);

    /// @brief Add interface input
    GraphBuilder& input(const std::string& name, PinType type);

    /// @brief Add interface output
    GraphBuilder& output(const std::string& name, PinType type);

    /// @brief Build the graph
    [[nodiscard]] std::unique_ptr<Graph> build();

private:
    std::unique_ptr<Graph> graph_;
    std::vector<std::pair<PinId, PinId>> pending_connections_;
};

} // namespace void_graph
