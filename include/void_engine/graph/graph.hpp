#pragma once

/// @file graph.hpp
/// @brief Public API for void_graph visual scripting system
///
/// This header provides forward declarations for the void_graph module,
/// a Blueprint-style visual scripting system for game logic.
///
/// Key components:
/// - GraphSystem: Main system singleton for managing graphs
/// - Graph: Container for nodes and connections
/// - INode: Interface for all graph nodes
/// - NodeRegistry: Registry of available node types
/// - GraphExecutor: Runtime execution engine
///
/// Note: Full type definitions are in the internal headers (src/graph/).
/// This public header provides forward declarations for external use.
/// For built-in node type IDs, include the internal registry.hpp.

#include <void_engine/core/handle.hpp>
#include <cstdint>

namespace void_graph {

// =============================================================================
// Handle Types (using void_core::Handle for type safety)
// =============================================================================

// Tag types are only defined if not already defined by internal fwd.hpp
#ifndef VOID_GRAPH_TAG_TYPES_DEFINED
#define VOID_GRAPH_TAG_TYPES_DEFINED
struct GraphIdTag {};
struct NodeIdTag {};
struct PinIdTag {};
struct ConnectionIdTag {};
struct NodeTypeIdTag {};
struct VariableIdTag {};
struct SubgraphIdTag {};
struct ExecutionIdTag {};
#endif

using GraphId = void_core::Handle<GraphIdTag>;
using NodeId = void_core::Handle<NodeIdTag>;
using PinId = void_core::Handle<PinIdTag>;
using ConnectionId = void_core::Handle<ConnectionIdTag>;
using NodeTypeId = void_core::Handle<NodeTypeIdTag>;
using VariableId = void_core::Handle<VariableIdTag>;
using SubgraphId = void_core::Handle<SubgraphIdTag>;
using ExecutionId = void_core::Handle<ExecutionIdTag>;

// =============================================================================
// Forward Declarations (Classes and Structs only)
// =============================================================================

// Core types (defined in src/graph/types.hpp)
struct Pin;
struct Connection;
struct NodeTemplate;
struct GraphVariable;
struct GraphMetadata;
struct ExecutionContext;
struct ExecutionResult;

// Node classes (defined in src/graph/node.hpp)
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

// Graph classes (defined in src/graph/graph.hpp internal)
class Graph;
class Subgraph;
class GraphInstance;

// Execution (defined in src/graph/execution.hpp)
class INodeExecutor;
class GraphExecutor;
class GraphCompiler;
class CompiledGraph;

// Registry (defined in src/graph/registry.hpp)
class NodeRegistry;
class GraphLibrary;

// Builders
class NodeBuilder;
class GraphBuilder;
class PinBuilder;

// System (defined in src/graph/system.hpp)
class GraphSystem;

// Note: Enum forward declarations removed - enums must be fully defined.
// Use the internal types.hpp for enum definitions.
// Note: builtin namespace removed - defined in internal registry.hpp.

} // namespace void_graph
