#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_graph module

#include <void_engine/core/handle.hpp>
#include <cstdint>

namespace void_graph {

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Unique identifier for a graph
struct GraphIdTag {};
using GraphId = void_core::Handle<GraphIdTag>;

/// @brief Unique identifier for a node within a graph
struct NodeIdTag {};
using NodeId = void_core::Handle<NodeIdTag>;

/// @brief Unique identifier for a pin on a node
struct PinIdTag {};
using PinId = void_core::Handle<PinIdTag>;

/// @brief Unique identifier for a connection between pins
struct ConnectionIdTag {};
using ConnectionId = void_core::Handle<ConnectionIdTag>;

/// @brief Unique identifier for a node template/type
struct NodeTypeIdTag {};
using NodeTypeId = void_core::Handle<NodeTypeIdTag>;

/// @brief Unique identifier for a variable in the graph
struct VariableIdTag {};
using VariableId = void_core::Handle<VariableIdTag>;

/// @brief Unique identifier for a subgraph/function
struct SubgraphIdTag {};
using SubgraphId = void_core::Handle<SubgraphIdTag>;

/// @brief Unique identifier for an execution context
struct ExecutionIdTag {};
using ExecutionId = void_core::Handle<ExecutionIdTag>;

// =============================================================================
// Forward Declarations
// =============================================================================

// Core types
struct Pin;
struct Connection;
struct NodeTemplate;
struct GraphVariable;
struct GraphMetadata;
struct ExecutionContext;
struct ExecutionResult;

// Node types
class INode;
class NodeBase;
class EventNode;
class FunctionNode;
class VariableNode;
class FlowControlNode;
class MathNode;
class ConversionNode;
class CommentNode;
class RerouteNode;

// Graph types
class Graph;
class Subgraph;
class GraphInstance;

// Execution
class INodeExecutor;
class GraphExecutor;
class GraphCompiler;
class CompiledGraph;

// Registry
class NodeRegistry;
class GraphLibrary;

// Builders
class NodeBuilder;
class GraphBuilder;
class PinBuilder;

// System
class GraphSystem;

} // namespace void_graph
