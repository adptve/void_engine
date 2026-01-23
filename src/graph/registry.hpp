#pragma once

/// @file registry.hpp
/// @brief Node registry and graph library

#include "execution.hpp"

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

namespace void_graph {

// =============================================================================
// Node Registry
// =============================================================================

/// @brief Registry of available node types
class NodeRegistry {
public:
    NodeRegistry();
    ~NodeRegistry() = default;

    // Singleton access
    [[nodiscard]] static NodeRegistry& instance();

    // ==========================================================================
    // Registration
    // ==========================================================================

    /// @brief Register a node template
    NodeTypeId register_node(const NodeTemplate& tmpl);

    /// @brief Register a node template with explicit type ID
    void register_node(NodeTypeId id, const NodeTemplate& tmpl);

    /// @brief Register a factory function
    template <typename T>
    NodeTypeId register_node(const std::string& name);

    /// @brief Unregister a node type
    bool unregister_node(NodeTypeId id);

    /// @brief Check if a node type exists
    [[nodiscard]] bool has_node(NodeTypeId id) const;

    /// @brief Get a node template
    [[nodiscard]] const NodeTemplate* get_template(NodeTypeId id) const;

    /// @brief Find template by name
    [[nodiscard]] const NodeTemplate* find_template(const std::string& name) const;

    // ==========================================================================
    // Node Creation
    // ==========================================================================

    /// @brief Create a node instance
    [[nodiscard]] std::unique_ptr<INode> create_node(NodeTypeId type_id, NodeId node_id) const;

    /// @brief Create a node by name
    [[nodiscard]] std::unique_ptr<INode> create_node(const std::string& name, NodeId node_id) const;

    // ==========================================================================
    // Queries
    // ==========================================================================

    /// @brief Get all registered templates
    [[nodiscard]] std::vector<const NodeTemplate*> all_templates() const;

    /// @brief Get templates by category
    [[nodiscard]] std::vector<const NodeTemplate*> templates_by_category(NodeCategory category) const;

    /// @brief Get templates by category path
    [[nodiscard]] std::vector<const NodeTemplate*> templates_by_path(const std::string& path) const;

    /// @brief Search templates by name/keywords
    [[nodiscard]] std::vector<const NodeTemplate*> search(const std::string& query) const;

    /// @brief Get all category paths
    [[nodiscard]] std::vector<std::string> all_categories() const;

    // ==========================================================================
    // Built-in Nodes
    // ==========================================================================

    /// @brief Register all built-in node types
    void register_builtins();

    /// @brief Register event nodes (BeginPlay, Tick, etc.)
    void register_event_nodes();

    /// @brief Register flow control nodes
    void register_flow_control_nodes();

    /// @brief Register math nodes
    void register_math_nodes();

    /// @brief Register conversion nodes
    void register_conversion_nodes();

    /// @brief Register utility nodes
    void register_utility_nodes();

    /// @brief Register string nodes
    void register_string_nodes();

    /// @brief Register array nodes
    void register_array_nodes();

    /// @brief Register debug nodes
    void register_debug_nodes();

    // ==========================================================================
    // Type ID Generation
    // ==========================================================================

    /// @brief Generate a type ID from a name
    [[nodiscard]] static NodeTypeId type_id_from_name(const std::string& name);

private:
    std::unordered_map<NodeTypeId, NodeTemplate> templates_;
    std::unordered_map<std::string, NodeTypeId> name_to_id_;
    inline static std::uint32_t next_type_id_ = 1;
};

// =============================================================================
// Graph Library
// =============================================================================

/// @brief Library of reusable graphs/functions
class GraphLibrary {
public:
    GraphLibrary();
    ~GraphLibrary() = default;

    // Singleton access
    [[nodiscard]] static GraphLibrary& instance();

    // ==========================================================================
    // Graph Management
    // ==========================================================================

    /// @brief Add a graph to the library
    GraphId add_graph(std::unique_ptr<Graph> graph);

    /// @brief Remove a graph from the library
    bool remove_graph(GraphId id);

    /// @brief Get a graph by ID
    [[nodiscard]] Graph* get_graph(GraphId id);
    [[nodiscard]] const Graph* get_graph(GraphId id) const;

    /// @brief Find graph by name
    [[nodiscard]] Graph* find_graph(const std::string& name);

    /// @brief Get all graphs
    [[nodiscard]] std::vector<Graph*> all_graphs();
    [[nodiscard]] std::vector<const Graph*> all_graphs() const;

    // ==========================================================================
    // Subgraph Management
    // ==========================================================================

    /// @brief Add a subgraph (reusable function)
    SubgraphId add_subgraph(std::unique_ptr<Subgraph> subgraph);

    /// @brief Get a subgraph
    [[nodiscard]] Subgraph* get_subgraph(SubgraphId id);
    [[nodiscard]] const Subgraph* get_subgraph(SubgraphId id) const;

    /// @brief Find subgraph by name
    [[nodiscard]] Subgraph* find_subgraph(const std::string& name);

    /// @brief Get all subgraphs
    [[nodiscard]] std::vector<Subgraph*> all_subgraphs();

    // ==========================================================================
    // Loading/Saving
    // ==========================================================================

    /// @brief Load a graph from file
    [[nodiscard]] GraphResult<Graph*> load_graph(const std::filesystem::path& path);

    /// @brief Save a graph to file
    [[nodiscard]] GraphResult<void> save_graph(GraphId id, const std::filesystem::path& path);

    /// @brief Load all graphs from a directory
    void load_directory(const std::filesystem::path& directory);

    /// @brief Export graph as C++ code
    [[nodiscard]] std::string export_cpp(GraphId id) const;

    /// @brief Import graph from JSON
    [[nodiscard]] GraphResult<Graph*> import_json(const std::string& json);

    // ==========================================================================
    // Categories
    // ==========================================================================

    /// @brief Get graphs by category
    [[nodiscard]] std::vector<Graph*> graphs_by_category(const std::string& category);

    /// @brief Search graphs
    [[nodiscard]] std::vector<Graph*> search(const std::string& query);

    // ==========================================================================
    // Dependencies
    // ==========================================================================

    /// @brief Get all graphs that depend on a subgraph
    [[nodiscard]] std::vector<GraphId> get_dependents(SubgraphId subgraph_id) const;

    /// @brief Validate all dependencies
    [[nodiscard]] bool validate_dependencies() const;

private:
    std::unordered_map<GraphId, std::unique_ptr<Graph>> graphs_;
    std::unordered_map<SubgraphId, std::unique_ptr<Subgraph>> subgraphs_;
    std::unordered_map<std::string, GraphId> graph_names_;
    std::unordered_map<std::string, SubgraphId> subgraph_names_;

    inline static std::uint32_t next_graph_id_ = 1;
    inline static std::uint32_t next_subgraph_id_ = 1;
};

// =============================================================================
// Built-in Node Type IDs
// =============================================================================

namespace builtin {

// Event nodes
inline const NodeTypeId EventBeginPlay = NodeTypeId::create(1, 0);
inline const NodeTypeId EventTick = NodeTypeId::create(2, 0);
inline const NodeTypeId EventEndPlay = NodeTypeId::create(3, 0);
inline const NodeTypeId EventOnOverlapBegin = NodeTypeId::create(4, 0);
inline const NodeTypeId EventOnOverlapEnd = NodeTypeId::create(5, 0);
inline const NodeTypeId EventOnHit = NodeTypeId::create(6, 0);
inline const NodeTypeId EventCustom = NodeTypeId::create(7, 0);

// Flow control
inline const NodeTypeId Branch = NodeTypeId::create(100, 0);
inline const NodeTypeId Sequence = NodeTypeId::create(101, 0);
inline const NodeTypeId ForLoop = NodeTypeId::create(102, 0);
inline const NodeTypeId WhileLoop = NodeTypeId::create(103, 0);
inline const NodeTypeId ForEachLoop = NodeTypeId::create(104, 0);
inline const NodeTypeId Delay = NodeTypeId::create(105, 0);
inline const NodeTypeId DoOnce = NodeTypeId::create(106, 0);
inline const NodeTypeId FlipFlop = NodeTypeId::create(107, 0);
inline const NodeTypeId Gate = NodeTypeId::create(108, 0);
inline const NodeTypeId DoN = NodeTypeId::create(109, 0);
inline const NodeTypeId Switch = NodeTypeId::create(110, 0);

// Math - Arithmetic
inline const NodeTypeId MathAdd = NodeTypeId::create(200, 0);
inline const NodeTypeId MathSubtract = NodeTypeId::create(201, 0);
inline const NodeTypeId MathMultiply = NodeTypeId::create(202, 0);
inline const NodeTypeId MathDivide = NodeTypeId::create(203, 0);
inline const NodeTypeId MathModulo = NodeTypeId::create(204, 0);
inline const NodeTypeId MathNegate = NodeTypeId::create(205, 0);
inline const NodeTypeId MathAbs = NodeTypeId::create(206, 0);
inline const NodeTypeId MathPower = NodeTypeId::create(207, 0);
inline const NodeTypeId MathSqrt = NodeTypeId::create(208, 0);

// Math - Trigonometry
inline const NodeTypeId MathSin = NodeTypeId::create(220, 0);
inline const NodeTypeId MathCos = NodeTypeId::create(221, 0);
inline const NodeTypeId MathTan = NodeTypeId::create(222, 0);
inline const NodeTypeId MathAsin = NodeTypeId::create(223, 0);
inline const NodeTypeId MathAcos = NodeTypeId::create(224, 0);
inline const NodeTypeId MathAtan = NodeTypeId::create(225, 0);
inline const NodeTypeId MathAtan2 = NodeTypeId::create(226, 0);

// Math - Comparison
inline const NodeTypeId MathMin = NodeTypeId::create(240, 0);
inline const NodeTypeId MathMax = NodeTypeId::create(241, 0);
inline const NodeTypeId MathClamp = NodeTypeId::create(242, 0);
inline const NodeTypeId MathLerp = NodeTypeId::create(243, 0);
inline const NodeTypeId MathInverseLerp = NodeTypeId::create(244, 0);
inline const NodeTypeId MathMapRange = NodeTypeId::create(245, 0);

// Math - Vector
inline const NodeTypeId VectorAdd = NodeTypeId::create(260, 0);
inline const NodeTypeId VectorSubtract = NodeTypeId::create(261, 0);
inline const NodeTypeId VectorMultiply = NodeTypeId::create(262, 0);
inline const NodeTypeId VectorDot = NodeTypeId::create(263, 0);
inline const NodeTypeId VectorCross = NodeTypeId::create(264, 0);
inline const NodeTypeId VectorNormalize = NodeTypeId::create(265, 0);
inline const NodeTypeId VectorLength = NodeTypeId::create(266, 0);
inline const NodeTypeId VectorDistance = NodeTypeId::create(267, 0);

// Logic
inline const NodeTypeId LogicAnd = NodeTypeId::create(300, 0);
inline const NodeTypeId LogicOr = NodeTypeId::create(301, 0);
inline const NodeTypeId LogicNot = NodeTypeId::create(302, 0);
inline const NodeTypeId LogicXor = NodeTypeId::create(303, 0);
inline const NodeTypeId CompareEqual = NodeTypeId::create(310, 0);
inline const NodeTypeId CompareNotEqual = NodeTypeId::create(311, 0);
inline const NodeTypeId CompareLess = NodeTypeId::create(312, 0);
inline const NodeTypeId CompareLessEqual = NodeTypeId::create(313, 0);
inline const NodeTypeId CompareGreater = NodeTypeId::create(314, 0);
inline const NodeTypeId CompareGreaterEqual = NodeTypeId::create(315, 0);

// Conversion
inline const NodeTypeId ToFloat = NodeTypeId::create(400, 0);
inline const NodeTypeId ToInt = NodeTypeId::create(401, 0);
inline const NodeTypeId ToString = NodeTypeId::create(402, 0);
inline const NodeTypeId ToBool = NodeTypeId::create(403, 0);
inline const NodeTypeId ToVector3 = NodeTypeId::create(404, 0);

// String
inline const NodeTypeId StringAppend = NodeTypeId::create(500, 0);
inline const NodeTypeId StringFormat = NodeTypeId::create(501, 0);
inline const NodeTypeId StringLength = NodeTypeId::create(502, 0);
inline const NodeTypeId StringSubstring = NodeTypeId::create(503, 0);
inline const NodeTypeId StringFind = NodeTypeId::create(504, 0);
inline const NodeTypeId StringReplace = NodeTypeId::create(505, 0);
inline const NodeTypeId StringSplit = NodeTypeId::create(506, 0);
inline const NodeTypeId StringJoin = NodeTypeId::create(507, 0);

// Array
inline const NodeTypeId ArrayAdd = NodeTypeId::create(600, 0);
inline const NodeTypeId ArrayRemove = NodeTypeId::create(601, 0);
inline const NodeTypeId ArrayGet = NodeTypeId::create(602, 0);
inline const NodeTypeId ArraySet = NodeTypeId::create(603, 0);
inline const NodeTypeId ArrayLength = NodeTypeId::create(604, 0);
inline const NodeTypeId ArrayClear = NodeTypeId::create(605, 0);
inline const NodeTypeId ArrayContains = NodeTypeId::create(606, 0);
inline const NodeTypeId ArrayFind = NodeTypeId::create(607, 0);
inline const NodeTypeId ArraySort = NodeTypeId::create(608, 0);
inline const NodeTypeId ArrayReverse = NodeTypeId::create(609, 0);

// Debug
inline const NodeTypeId PrintString = NodeTypeId::create(700, 0);
inline const NodeTypeId PrintValue = NodeTypeId::create(701, 0);
inline const NodeTypeId DrawDebugLine = NodeTypeId::create(702, 0);
inline const NodeTypeId DrawDebugSphere = NodeTypeId::create(703, 0);
inline const NodeTypeId DrawDebugBox = NodeTypeId::create(704, 0);

// Utility
inline const NodeTypeId MakeVector2 = NodeTypeId::create(800, 0);
inline const NodeTypeId MakeVector3 = NodeTypeId::create(801, 0);
inline const NodeTypeId MakeVector4 = NodeTypeId::create(802, 0);
inline const NodeTypeId MakeRotator = NodeTypeId::create(803, 0);
inline const NodeTypeId MakeTransform = NodeTypeId::create(804, 0);
inline const NodeTypeId MakeColor = NodeTypeId::create(805, 0);
inline const NodeTypeId BreakVector2 = NodeTypeId::create(810, 0);
inline const NodeTypeId BreakVector3 = NodeTypeId::create(811, 0);
inline const NodeTypeId BreakVector4 = NodeTypeId::create(812, 0);
inline const NodeTypeId BreakRotator = NodeTypeId::create(813, 0);
inline const NodeTypeId BreakTransform = NodeTypeId::create(814, 0);
inline const NodeTypeId BreakColor = NodeTypeId::create(815, 0);

// Special
inline const NodeTypeId Comment = NodeTypeId::create(900, 0);
inline const NodeTypeId Reroute = NodeTypeId::create(901, 0);
inline const NodeTypeId GetVariable = NodeTypeId::create(902, 0);
inline const NodeTypeId SetVariable = NodeTypeId::create(903, 0);
inline const NodeTypeId CallFunction = NodeTypeId::create(904, 0);
inline const NodeTypeId Return = NodeTypeId::create(905, 0);

} // namespace builtin

} // namespace void_graph
