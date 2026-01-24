#pragma once

/// @file graph.hpp
/// @brief Public API for void_graph visual scripting system
///
/// This header provides the public interface for the void_graph module,
/// a Blueprint-style visual scripting system for game logic.
///
/// Key components:
/// - GraphSystem: Main system singleton for managing graphs
/// - Graph: Container for nodes and connections
/// - INode: Interface for all graph nodes
/// - NodeRegistry: Registry of available node types
/// - GraphExecutor: Runtime execution engine
///
/// Example usage:
/// @code
/// #include <void_engine/graph/graph.hpp>
///
/// void setup_graph() {
///     auto& system = void_graph::GraphSystem::instance();
///     system.initialize();
///
///     // Create a graph
///     auto* graph = system.create_graph("MyGraph");
///
///     // Create nodes
///     auto* event = graph->create_node(void_graph::builtin::EventBeginPlay);
///     auto* print = graph->create_node(void_graph::builtin::PrintString);
///
///     // Connect them
///     graph->connect(event->output_pins()[0].id, print->input_pins()[0].id);
///
///     // Execute
///     system.execute_sync(graph->id(), "BeginPlay");
/// }
/// @endcode

// Forward declarations
namespace void_graph {
    class Graph;
    class GraphBuilder;
    class GraphInstance;
    class GraphSystem;
    class GraphExecutor;
    class GraphCompiler;
    class CompiledGraph;
    class NodeRegistry;
    class GraphLibrary;
    class INode;
    class NodeBase;
    class EventNode;
    class FunctionNode;
    class VariableNode;
    class BranchNode;
    class SequenceNode;
    class ForLoopNode;
    class DelayNode;
    class MathNode;
    class Subgraph;
    class SubgraphNode;
    struct Pin;
    struct Connection;
    struct NodeTemplate;
    struct GraphVariable;
    struct GraphMetadata;
    struct ExecutionContext;
    struct ExecutionResult;
}

// Include the full implementation headers
// Note: These are internal headers in src/graph/
// The public API is defined through forward declarations

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace void_graph {

// =============================================================================
// Handle Types (from fwd.hpp)
// =============================================================================

/// @brief Strong type wrapper for graph-related IDs
template <typename Tag>
struct StrongId {
    std::uint32_t value = 0;

    [[nodiscard]] constexpr bool is_valid() const { return value != 0; }
    [[nodiscard]] constexpr std::uint32_t to_bits() const { return value; }
    [[nodiscard]] static constexpr StrongId from_bits(std::uint32_t v) { return StrongId{v}; }
    [[nodiscard]] static constexpr StrongId null() { return StrongId{0}; }
    [[nodiscard]] static constexpr StrongId create(std::uint32_t index, std::uint32_t gen) {
        return StrongId{(gen << 20) | (index & 0xFFFFF)};
    }

    constexpr bool operator==(const StrongId& other) const { return value == other.value; }
    constexpr bool operator!=(const StrongId& other) const { return value != other.value; }
    constexpr bool operator<(const StrongId& other) const { return value < other.value; }
};

struct GraphIdTag {};
struct NodeIdTag {};
struct PinIdTag {};
struct ConnectionIdTag {};
struct NodeTypeIdTag {};
struct VariableIdTag {};
struct SubgraphIdTag {};
struct ExecutionIdTag {};

using GraphId = StrongId<GraphIdTag>;
using NodeId = StrongId<NodeIdTag>;
using PinId = StrongId<PinIdTag>;
using ConnectionId = StrongId<ConnectionIdTag>;
using NodeTypeId = StrongId<NodeTypeIdTag>;
using VariableId = StrongId<VariableIdTag>;
using SubgraphId = StrongId<SubgraphIdTag>;
using ExecutionId = StrongId<ExecutionIdTag>;

// =============================================================================
// Enums (from types.hpp)
// =============================================================================

/// @brief Pin direction
enum class PinDirection : std::uint8_t {
    Input,
    Output
};

/// @brief Pin data types
enum class PinType : std::uint8_t {
    Exec,       ///< Execution flow (white)
    Bool,       ///< Boolean (red)
    Int,        ///< Integer (cyan)
    Float,      ///< Float (green)
    String,     ///< String (magenta)
    Vec2,       ///< 2D vector (gold)
    Vec3,       ///< 3D vector (yellow)
    Vec4,       ///< 4D vector (orange)
    Quat,       ///< Quaternion (purple)
    Mat3,       ///< 3x3 matrix
    Mat4,       ///< 4x4 matrix
    Transform,  ///< Full transform
    Color,      ///< RGBA color
    Object,     ///< Generic object reference
    Entity,     ///< ECS entity
    Component,  ///< ECS component
    Asset,      ///< Asset reference
    Array,      ///< Dynamic array
    Map,        ///< Key-value map
    Set,        ///< Unique set
    Any,        ///< Wildcard type
    Struct,     ///< Custom struct
    Enum,       ///< Enum value
    Delegate,   ///< Function delegate
    Event,      ///< Event dispatcher
    Branch,     ///< Branch condition
    Loop        ///< Loop control
};

/// @brief Node category for organization
enum class NodeCategory : std::uint8_t {
    Event,
    Function,
    Variable,
    FlowControl,
    Math,
    Conversion,
    Utility,
    Custom,
    Comment,
    Reroute,
    Subgraph,
    Macro
};

/// @brief Node purity (affects caching and execution)
enum class NodePurity : std::uint8_t {
    Pure,       ///< No side effects, can cache
    Impure,     ///< Has side effects
    Latent      ///< Suspends execution
};

/// @brief Node execution state
enum class NodeState : std::uint8_t {
    Idle,
    Pending,
    Executing,
    Suspended,
    Completed,
    Error
};

/// @brief Graph execution state
enum class ExecutionState : std::uint8_t {
    Idle,
    Running,
    Paused,
    Suspended,
    Completed,
    Aborted,
    Error
};

/// @brief Graph type
enum class GraphType : std::uint8_t {
    Event,      ///< Event-driven graph
    Function,   ///< Callable function
    Macro,      ///< Inline expansion
    AnimGraph,  ///< Animation graph
    State,      ///< State machine
    Material,   ///< Material graph
    Custom
};

/// @brief Graph errors
enum class GraphError : std::uint8_t {
    None,
    InvalidNode,
    InvalidPin,
    InvalidConnection,
    TypeMismatch,
    CyclicConnection,
    InvalidGraph,
    ExecutionError,
    CompilationError,
    SerializationError,
    NotFound,
    AlreadyExists,
    InvalidOperation,
    OutOfMemory,
    Timeout,
    Interrupted,
    PermissionDenied,
    Unknown
};

// =============================================================================
// Result Type
// =============================================================================

/// @brief Result type for graph operations
template <typename T>
class GraphResult {
public:
    GraphResult() : error_(GraphError::None) {}
    explicit GraphResult(T&& value) : value_(std::move(value)), error_(GraphError::None) {}
    explicit GraphResult(const T& value) : value_(value), error_(GraphError::None) {}
    explicit GraphResult(GraphError error) : error_(error) {}

    [[nodiscard]] bool ok() const { return error_ == GraphError::None; }
    [[nodiscard]] explicit operator bool() const { return ok(); }
    [[nodiscard]] GraphError error() const { return error_; }
    [[nodiscard]] T& value() { return value_; }
    [[nodiscard]] const T& value() const { return value_; }
    [[nodiscard]] T* operator->() { return &value_; }
    [[nodiscard]] const T* operator->() const { return &value_; }

    static GraphResult ok() { return GraphResult(); }

private:
    T value_{};
    GraphError error_;
};

template <>
class GraphResult<void> {
public:
    GraphResult() : error_(GraphError::None) {}
    explicit GraphResult(GraphError error) : error_(error) {}

    [[nodiscard]] bool is_ok() const { return error_ == GraphError::None; }
    [[nodiscard]] explicit operator bool() const { return is_ok(); }
    [[nodiscard]] GraphError error() const { return error_; }

    static GraphResult ok() { return GraphResult(); }

private:
    GraphError error_;
};

// =============================================================================
// Pin Value Type
// =============================================================================

/// @brief Value that can be stored in a pin
using PinValue = std::variant<
    std::monostate,         // null/empty
    bool,
    std::int32_t,
    std::int64_t,
    float,
    double,
    std::string,
    std::uint64_t,          // Entity ID
    std::vector<PinValue>,  // Array
    std::any                // Custom types
>;

} // namespace void_graph

// Hash specializations for ID types
namespace std {
    template <typename Tag>
    struct hash<void_graph::StrongId<Tag>> {
        std::size_t operator()(const void_graph::StrongId<Tag>& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
}

namespace void_graph {

// =============================================================================
// Built-in Node Type IDs
// =============================================================================

/// @brief Built-in node type identifiers
namespace builtin {

// Event nodes
inline const NodeTypeId EventBeginPlay = NodeTypeId::create(1, 0);
inline const NodeTypeId EventTick = NodeTypeId::create(2, 0);
inline const NodeTypeId EventEndPlay = NodeTypeId::create(3, 0);

// Flow control
inline const NodeTypeId Branch = NodeTypeId::create(100, 0);
inline const NodeTypeId Sequence = NodeTypeId::create(101, 0);
inline const NodeTypeId ForLoop = NodeTypeId::create(102, 0);
inline const NodeTypeId Delay = NodeTypeId::create(105, 0);

// Math
inline const NodeTypeId MathAdd = NodeTypeId::create(200, 0);
inline const NodeTypeId MathSubtract = NodeTypeId::create(201, 0);
inline const NodeTypeId MathMultiply = NodeTypeId::create(202, 0);
inline const NodeTypeId MathDivide = NodeTypeId::create(203, 0);

// Debug
inline const NodeTypeId PrintString = NodeTypeId::create(700, 0);

// Entity
inline const NodeTypeId SpawnEntity = NodeTypeId::create(1000, 0);
inline const NodeTypeId DestroyEntity = NodeTypeId::create(1001, 0);
inline const NodeTypeId GetEntityLocation = NodeTypeId::create(1002, 0);
inline const NodeTypeId SetEntityLocation = NodeTypeId::create(1003, 0);

// Physics
inline const NodeTypeId AddForce = NodeTypeId::create(1100, 0);
inline const NodeTypeId Raycast = NodeTypeId::create(1106, 0);

// Audio
inline const NodeTypeId PlaySound = NodeTypeId::create(1200, 0);
inline const NodeTypeId PlayMusic = NodeTypeId::create(1205, 0);

// Combat
inline const NodeTypeId ApplyDamage = NodeTypeId::create(1300, 0);
inline const NodeTypeId GetHealth = NodeTypeId::create(1301, 0);

} // namespace builtin

} // namespace void_graph
