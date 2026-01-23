#pragma once

/// @file types.hpp
/// @brief Core types and enumerations for void_graph

#include "fwd.hpp"
#include <void_engine/core/error.hpp>

#include <any>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace void_graph {

// =============================================================================
// Enumerations
// =============================================================================

/// @brief Category of a pin (input or output)
enum class PinDirection : std::uint8_t {
    Input,      ///< Receives data or execution flow
    Output      ///< Sends data or execution flow
};

/// @brief Type of data a pin handles
enum class PinType : std::uint8_t {
    // Execution flow (white pins in Blueprint)
    Exec,           ///< Execution flow (no data)

    // Primitive types
    Bool,           ///< Boolean value
    Int,            ///< 32-bit signed integer
    Int64,          ///< 64-bit signed integer
    Float,          ///< 32-bit floating point
    Double,         ///< 64-bit floating point
    String,         ///< String value

    // Math types
    Vec2,           ///< 2D vector
    Vec3,           ///< 3D vector
    Vec4,           ///< 4D vector
    Quat,           ///< Quaternion
    Mat3,           ///< 3x3 matrix
    Mat4,           ///< 4x4 matrix
    Transform,      ///< Transform (position, rotation, scale)
    Color,          ///< RGBA color

    // Object types
    Object,         ///< Generic object reference
    Entity,         ///< ECS entity
    Component,      ///< ECS component
    Asset,          ///< Asset handle

    // Container types
    Array,          ///< Array of values
    Map,            ///< Key-value map
    Set,            ///< Unique set of values

    // Special types
    Any,            ///< Any type (wildcard)
    Struct,         ///< Custom struct
    Enum,           ///< Enumeration value
    Delegate,       ///< Function delegate
    Event,          ///< Event type

    // Flow control
    Branch,         ///< Conditional branch output
    Loop,           ///< Loop control

    Count
};

/// @brief Category of a node
enum class NodeCategory : std::uint8_t {
    Event,          ///< Event entry points (BeginPlay, Tick, etc.)
    Function,       ///< Pure or impure functions
    Variable,       ///< Get/Set variable nodes
    FlowControl,    ///< Branch, loop, sequence, etc.
    Math,           ///< Mathematical operations
    Conversion,     ///< Type conversion nodes
    Utility,        ///< Utility nodes (print, delay, etc.)
    Custom,         ///< User-defined nodes
    Comment,        ///< Comment/note nodes
    Reroute,        ///< Wire reroute nodes
    Subgraph,       ///< Collapsed subgraph
    Macro,          ///< Macro node

    Count
};

/// @brief Purity of a node (pure nodes have no side effects)
enum class NodePurity : std::uint8_t {
    Pure,           ///< No side effects, can be cached
    Impure,         ///< Has side effects, must execute
    Latent          ///< Asynchronous, may pause execution
};

/// @brief State of a node during execution
enum class NodeState : std::uint8_t {
    Idle,           ///< Not executing
    Pending,        ///< Waiting to execute
    Executing,      ///< Currently executing
    Suspended,      ///< Suspended (latent node)
    Completed,      ///< Execution completed
    Error           ///< Execution error
};

/// @brief State of graph execution
enum class ExecutionState : std::uint8_t {
    Idle,           ///< Not running
    Running,        ///< Actively executing
    Paused,         ///< Paused at breakpoint
    Suspended,      ///< Waiting for latent action
    Completed,      ///< Finished execution
    Aborted,        ///< Execution aborted
    Error           ///< Execution error
};

/// @brief Type of graph
enum class GraphType : std::uint8_t {
    Event,          ///< Event-driven graph (like Blueprint Event Graph)
    Function,       ///< Function graph (reusable function)
    Macro,          ///< Macro graph (inline expansion)
    AnimGraph,      ///< Animation graph
    State,          ///< State machine graph
    Material,       ///< Material/shader graph
    Custom          ///< Custom graph type
};

/// @brief Compilation optimization level
enum class OptimizationLevel : std::uint8_t {
    Debug,          ///< No optimization, full debug info
    Development,    ///< Some optimization, some debug info
    Shipping        ///< Full optimization, no debug info
};

// =============================================================================
// Value Types
// =============================================================================

/// @brief Runtime value that can be stored in pins
using PinValue = std::variant<
    std::monostate,             // Null/unset
    bool,                       // Bool
    std::int32_t,               // Int
    std::int64_t,               // Int64
    float,                      // Float
    double,                     // Double
    std::string,                // String
    std::array<float, 2>,       // Vec2
    std::array<float, 3>,       // Vec3
    std::array<float, 4>,       // Vec4
    std::array<float, 4>,       // Quat (stored as vec4)
    std::array<float, 16>,      // Mat4
    std::uint64_t,              // Entity/Handle
    std::vector<std::any>,      // Array
    std::any                    // Any/Object
>;

/// @brief Default value definition for a pin
struct PinDefault {
    PinType type = PinType::Any;
    PinValue value;
    std::string literal;        ///< String representation for serialization
    bool use_literal = false;   ///< Whether to use literal string
};

// =============================================================================
// Pin Definition
// =============================================================================

/// @brief Definition of a pin on a node
struct Pin {
    PinId id;
    NodeId owner;                   ///< Node this pin belongs to
    std::string name;               ///< Display name
    std::string tooltip;            ///< Hover tooltip
    PinDirection direction = PinDirection::Input;
    PinType type = PinType::Any;

    // For container types
    PinType inner_type = PinType::Any;  ///< Element type for arrays/sets
    PinType key_type = PinType::Any;    ///< Key type for maps

    // For struct/enum types
    std::string type_name;          ///< Struct/enum type name

    // State
    PinDefault default_value;       ///< Default value if not connected
    bool is_connected = false;      ///< Has active connection
    bool is_hidden = false;         ///< Hidden from UI
    bool is_advanced = false;       ///< Show in advanced section
    bool is_reference = false;      ///< Pass by reference
    bool is_const = false;          ///< Const reference

    // Visual
    std::uint32_t color = 0xFFFFFFFF;   ///< Pin color override
    float position_y = 0.0f;        ///< Y position offset

    /// @brief Check if this pin can connect to another
    [[nodiscard]] bool can_connect_to(const Pin& other) const;

    /// @brief Get the wire color for this pin type
    [[nodiscard]] std::uint32_t get_wire_color() const;
};

// =============================================================================
// Connection Definition
// =============================================================================

/// @brief A connection between two pins
struct Connection {
    ConnectionId id;
    PinId source;       ///< Output pin
    PinId target;       ///< Input pin
    NodeId source_node;
    NodeId target_node;

    // Visual
    std::vector<std::array<float, 2>> control_points;   ///< Bezier control points
    std::uint32_t color_override = 0;                   ///< 0 = use pin color
    float thickness = 2.0f;
};

// =============================================================================
// Node Template
// =============================================================================

/// @brief Template/definition for creating nodes
struct NodeTemplate {
    NodeTypeId id;
    std::string name;                   ///< Display name
    std::string category;               ///< Category path (e.g., "Math|Trig")
    std::string tooltip;                ///< Description
    std::string keywords;               ///< Search keywords

    NodeCategory node_category = NodeCategory::Function;
    NodePurity purity = NodePurity::Pure;

    // Pin templates
    std::vector<Pin> input_pins;
    std::vector<Pin> output_pins;

    // Behavior
    bool is_compact = false;            ///< Use compact display
    bool is_deprecated = false;
    std::string deprecated_message;
    bool is_development_only = false;

    // Visual
    std::uint32_t title_color = 0xFF333333;
    std::string icon;                   ///< Icon asset path
    float min_width = 100.0f;
    float min_height = 50.0f;

    // Factory
    std::function<std::unique_ptr<INode>()> create;
};

// =============================================================================
// Graph Variable
// =============================================================================

/// @brief A variable defined in a graph
struct GraphVariable {
    VariableId id;
    std::string name;
    std::string category;               ///< Variable category
    std::string tooltip;
    PinType type = PinType::Any;
    std::string type_name;              ///< For struct/enum types
    PinDefault default_value;

    bool is_public = false;             ///< Exposed to outside
    bool is_replicated = false;         ///< Network replicated
    bool is_save_game = false;          ///< Saved with game
    bool is_read_only = false;

    std::uint32_t instance_editable : 1 = 0;
    std::uint32_t blueprint_read_only : 1 = 0;
};

// =============================================================================
// Graph Metadata
// =============================================================================

/// @brief Metadata for a graph
struct GraphMetadata {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point modified;

    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> custom_data;
};

// =============================================================================
// Execution Types
// =============================================================================

/// @brief Context for graph execution
struct ExecutionContext {
    ExecutionId id;
    GraphId graph;

    // Current state
    NodeId current_node;
    PinId current_exec_pin;             ///< Current execution output
    ExecutionState state = ExecutionState::Idle;

    // Call stack
    std::vector<NodeId> call_stack;
    std::size_t max_call_depth = 1000;

    // Variables
    std::unordered_map<VariableId, PinValue> variables;
    std::unordered_map<PinId, PinValue> pin_values;

    // Timing
    float delta_time = 0.0f;
    float total_time = 0.0f;
    std::uint64_t frame_count = 0;

    // Debugging
    bool debug_enabled = false;
    std::vector<NodeId> breakpoints;
    std::function<void(NodeId)> on_breakpoint_hit;

    // Owner
    std::uint64_t owner_entity = 0;     ///< Entity that owns this execution
    void* owner_object = nullptr;       ///< Native object pointer
};

/// @brief Result of graph execution
struct ExecutionResult {
    ExecutionState final_state = ExecutionState::Completed;
    std::string error_message;
    NodeId error_node;
    std::size_t nodes_executed = 0;
    float execution_time_ms = 0.0f;

    std::unordered_map<PinId, PinValue> output_values;
};

// =============================================================================
// Error Types
// =============================================================================

/// @brief Graph-related errors
enum class GraphError {
    None = 0,
    InvalidGraph,
    InvalidNode,
    InvalidPin,
    InvalidConnection,
    TypeMismatch,
    CyclicConnection,
    MaxDepthExceeded,
    CompilationFailed,
    ExecutionFailed,
    BreakpointHit,
    NodeNotFound,
    PinNotFound,
    VariableNotFound,
    DuplicateName,
    InvalidOperation,
    SerializationError,
    VersionMismatch
};

/// @brief Convert error to string
[[nodiscard]] inline constexpr const char* to_string(GraphError error) {
    switch (error) {
        case GraphError::None: return "None";
        case GraphError::InvalidGraph: return "Invalid graph";
        case GraphError::InvalidNode: return "Invalid node";
        case GraphError::InvalidPin: return "Invalid pin";
        case GraphError::InvalidConnection: return "Invalid connection";
        case GraphError::TypeMismatch: return "Type mismatch";
        case GraphError::CyclicConnection: return "Cyclic connection";
        case GraphError::MaxDepthExceeded: return "Max depth exceeded";
        case GraphError::CompilationFailed: return "Compilation failed";
        case GraphError::ExecutionFailed: return "Execution failed";
        case GraphError::BreakpointHit: return "Breakpoint hit";
        case GraphError::NodeNotFound: return "Node not found";
        case GraphError::PinNotFound: return "Pin not found";
        case GraphError::VariableNotFound: return "Variable not found";
        case GraphError::DuplicateName: return "Duplicate name";
        case GraphError::InvalidOperation: return "Invalid operation";
        case GraphError::SerializationError: return "Serialization error";
        case GraphError::VersionMismatch: return "Version mismatch";
        default: return "Unknown error";
    }
}

/// @brief Result type for graph operations
template <typename T>
using GraphResult = void_core::Result<T, GraphError>;

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Get display name for a pin type
[[nodiscard]] constexpr const char* pin_type_name(PinType type) {
    switch (type) {
        case PinType::Exec: return "Exec";
        case PinType::Bool: return "Boolean";
        case PinType::Int: return "Integer";
        case PinType::Int64: return "Integer64";
        case PinType::Float: return "Float";
        case PinType::Double: return "Double";
        case PinType::String: return "String";
        case PinType::Vec2: return "Vector2";
        case PinType::Vec3: return "Vector3";
        case PinType::Vec4: return "Vector4";
        case PinType::Quat: return "Quaternion";
        case PinType::Mat3: return "Matrix3x3";
        case PinType::Mat4: return "Matrix4x4";
        case PinType::Transform: return "Transform";
        case PinType::Color: return "Color";
        case PinType::Object: return "Object";
        case PinType::Entity: return "Entity";
        case PinType::Component: return "Component";
        case PinType::Asset: return "Asset";
        case PinType::Array: return "Array";
        case PinType::Map: return "Map";
        case PinType::Set: return "Set";
        case PinType::Any: return "Any";
        case PinType::Struct: return "Struct";
        case PinType::Enum: return "Enum";
        case PinType::Delegate: return "Delegate";
        case PinType::Event: return "Event";
        case PinType::Branch: return "Branch";
        case PinType::Loop: return "Loop";
        default: return "Unknown";
    }
}

/// @brief Get default wire color for a pin type
[[nodiscard]] constexpr std::uint32_t pin_type_color(PinType type) {
    switch (type) {
        case PinType::Exec: return 0xFFFFFFFF;      // White
        case PinType::Bool: return 0xFF990000;      // Dark red
        case PinType::Int: return 0xFF00FFFF;       // Cyan
        case PinType::Int64: return 0xFF00DDDD;     // Dark cyan
        case PinType::Float: return 0xFF00FF00;     // Green
        case PinType::Double: return 0xFF00DD00;    // Dark green
        case PinType::String: return 0xFFFF00FF;    // Magenta
        case PinType::Vec2: return 0xFFFFCC00;      // Gold
        case PinType::Vec3: return 0xFFFFAA00;      // Orange
        case PinType::Vec4: return 0xFFFF8800;      // Dark orange
        case PinType::Quat: return 0xFF88CCFF;      // Light blue
        case PinType::Mat3: return 0xFF8888FF;      // Purple
        case PinType::Mat4: return 0xFF6666FF;      // Dark purple
        case PinType::Transform: return 0xFFFF6600; // Red-orange
        case PinType::Color: return 0xFF66FF66;     // Light green
        case PinType::Object: return 0xFF0088FF;    // Blue
        case PinType::Entity: return 0xFF00AAFF;    // Light blue
        case PinType::Component: return 0xFF00CCFF; // Cyan-blue
        case PinType::Asset: return 0xFFFFFF00;     // Yellow
        case PinType::Array: return 0xFFCC88FF;     // Light purple
        case PinType::Map: return 0xFFFF88CC;       // Pink
        case PinType::Set: return 0xFFFFAAFF;       // Light magenta
        case PinType::Any: return 0xFF888888;       // Gray
        case PinType::Struct: return 0xFF0000FF;    // Blue
        case PinType::Enum: return 0xFF00FF88;      // Teal
        case PinType::Delegate: return 0xFFFF0000;  // Red
        case PinType::Event: return 0xFFFF4444;     // Light red
        default: return 0xFFFFFFFF;
    }
}

/// @brief Check if a pin type is numeric
[[nodiscard]] constexpr bool is_numeric_type(PinType type) {
    return type == PinType::Int || type == PinType::Int64 ||
           type == PinType::Float || type == PinType::Double;
}

/// @brief Check if implicit conversion is allowed between types
[[nodiscard]] bool can_implicit_convert(PinType from, PinType to);

/// @brief Get the common type for two numeric types
[[nodiscard]] PinType common_numeric_type(PinType a, PinType b);

} // namespace void_graph
