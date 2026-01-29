#pragma once

/// @file node.hpp
/// @brief Node interface and base implementations

#include "types.hpp"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace void_graph {

// =============================================================================
// Node Interface
// =============================================================================

/// @brief Interface for all graph nodes
class INode {
public:
    virtual ~INode() = default;

    // Identity
    [[nodiscard]] virtual NodeId id() const = 0;
    [[nodiscard]] virtual NodeTypeId type_id() const = 0;
    [[nodiscard]] virtual const std::string& name() const = 0;
    [[nodiscard]] virtual const std::string& title() const = 0;
    [[nodiscard]] virtual NodeCategory category() const = 0;
    [[nodiscard]] virtual NodePurity purity() const = 0;

    // Pins
    [[nodiscard]] virtual std::span<const Pin> input_pins() const = 0;
    [[nodiscard]] virtual std::span<const Pin> output_pins() const = 0;
    [[nodiscard]] virtual const Pin* find_pin(PinId id) const = 0;
    [[nodiscard]] virtual const Pin* find_pin_by_name(const std::string& name) const = 0;

    // Execution
    [[nodiscard]] virtual NodeState state() const = 0;
    virtual void set_state(NodeState state) = 0;

    /// @brief Execute the node
    /// @param ctx Execution context
    /// @return Output execution pin to follow (null for end of flow)
    virtual PinId execute(ExecutionContext& ctx) = 0;

    /// @brief Resume a suspended latent node
    virtual PinId resume([[maybe_unused]] ExecutionContext& ctx) { return PinId{}; }

    /// @brief Called when a connection is made/broken
    virtual void on_connection_changed([[maybe_unused]] PinId pin, [[maybe_unused]] bool connected) {}

    /// @brief Called when an input pin value changes
    virtual void on_input_changed([[maybe_unused]] PinId pin) {}

    // Lifecycle
    virtual void initialize() {}
    virtual void shutdown() {}

    // Serialization
    virtual void serialize([[maybe_unused]] std::ostream& out) const {}
    virtual void deserialize([[maybe_unused]] std::istream& in) {}

    // Visual
    [[nodiscard]] virtual float x() const = 0;
    [[nodiscard]] virtual float y() const = 0;
    virtual void set_position(float x, float y) = 0;
    [[nodiscard]] virtual float width() const = 0;
    [[nodiscard]] virtual float height() const = 0;
    [[nodiscard]] virtual std::uint32_t title_color() const = 0;
    [[nodiscard]] virtual bool is_compact() const = 0;
    [[nodiscard]] virtual const std::string& comment() const = 0;
    virtual void set_comment(const std::string& comment) = 0;

    // State queries
    [[nodiscard]] virtual bool is_breakpoint() const = 0;
    virtual void set_breakpoint(bool enabled) = 0;
    [[nodiscard]] virtual bool is_disabled() const = 0;
    virtual void set_disabled(bool disabled) = 0;
};

// =============================================================================
// Node Base Implementation
// =============================================================================

/// @brief Base implementation of INode with common functionality
class NodeBase : public INode {
public:
    NodeBase(NodeId id, NodeTypeId type_id, const std::string& name);
    ~NodeBase() override = default;

    // Identity
    [[nodiscard]] NodeId id() const override { return id_; }
    [[nodiscard]] NodeTypeId type_id() const override { return type_id_; }
    [[nodiscard]] const std::string& name() const override { return name_; }
    [[nodiscard]] const std::string& title() const override { return title_.empty() ? name_ : title_; }
    [[nodiscard]] NodeCategory category() const override { return category_; }
    [[nodiscard]] NodePurity purity() const override { return purity_; }

    // Pins
    [[nodiscard]] std::span<const Pin> input_pins() const override { return input_pins_; }
    [[nodiscard]] std::span<const Pin> output_pins() const override { return output_pins_; }
    [[nodiscard]] const Pin* find_pin(PinId id) const override;
    [[nodiscard]] const Pin* find_pin_by_name(const std::string& name) const override;

    // Execution state
    [[nodiscard]] NodeState state() const override { return state_; }
    void set_state(NodeState state) override { state_ = state; }

    // Visual
    [[nodiscard]] float x() const override { return x_; }
    [[nodiscard]] float y() const override { return y_; }
    void set_position(float x, float y) override { x_ = x; y_ = y; }
    [[nodiscard]] float width() const override { return width_; }
    [[nodiscard]] float height() const override { return height_; }
    [[nodiscard]] std::uint32_t title_color() const override { return title_color_; }
    [[nodiscard]] bool is_compact() const override { return is_compact_; }
    [[nodiscard]] const std::string& comment() const override { return comment_; }
    void set_comment(const std::string& comment) override { comment_ = comment; }

    // State
    [[nodiscard]] bool is_breakpoint() const override { return is_breakpoint_; }
    void set_breakpoint(bool enabled) override { is_breakpoint_ = enabled; }
    [[nodiscard]] bool is_disabled() const override { return is_disabled_; }
    void set_disabled(bool disabled) override { is_disabled_ = disabled; }

protected:
    /// @brief Add an input pin
    Pin& add_input_pin(const std::string& name, PinType type);

    /// @brief Add an output pin
    Pin& add_output_pin(const std::string& name, PinType type);

    /// @brief Add an execution input pin
    Pin& add_exec_input(const std::string& name = "");

    /// @brief Add an execution output pin
    Pin& add_exec_output(const std::string& name = "");

    /// @brief Get a mutable pin
    Pin* get_mutable_pin(PinId id);

    /// @brief Get input pin value from context
    template <typename T>
    T get_input(ExecutionContext& ctx, const std::string& pin_name) const;

    /// @brief Set output pin value in context
    template <typename T>
    void set_output(ExecutionContext& ctx, const std::string& pin_name, T&& value) const;

    /// @brief Get the first exec output pin
    [[nodiscard]] PinId first_exec_output() const;

    // Members
    NodeId id_;
    NodeTypeId type_id_;
    std::string name_;
    std::string title_;
    std::string comment_;
    NodeCategory category_ = NodeCategory::Function;
    NodePurity purity_ = NodePurity::Pure;
    NodeState state_ = NodeState::Idle;

    std::vector<Pin> input_pins_;
    std::vector<Pin> output_pins_;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float width_ = 150.0f;
    float height_ = 100.0f;
    std::uint32_t title_color_ = 0xFF333333;
    bool is_compact_ = false;
    bool is_breakpoint_ = false;
    bool is_disabled_ = false;

    inline static std::uint32_t next_pin_id_ = 1;
};

// =============================================================================
// Event Node
// =============================================================================

/// @brief Entry point node for events (BeginPlay, Tick, etc.)
class EventNode : public NodeBase {
public:
    using EventCallback = std::function<void(ExecutionContext&)>;

    EventNode(NodeId id, NodeTypeId type_id, const std::string& name);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::Event; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Impure; }

    PinId execute(ExecutionContext& ctx) override;

    void set_event_callback(EventCallback callback) { callback_ = std::move(callback); }

    /// @brief Get the event name
    [[nodiscard]] const std::string& event_name() const { return event_name_; }
    void set_event_name(const std::string& name) { event_name_ = name; }

protected:
    std::string event_name_;
    EventCallback callback_;
};

// =============================================================================
// Function Node
// =============================================================================

/// @brief Node that executes a function
class FunctionNode : public NodeBase {
public:
    using FunctionImpl = std::function<PinId(ExecutionContext&, const FunctionNode&)>;

    FunctionNode(NodeId id, NodeTypeId type_id, const std::string& name);

    PinId execute(ExecutionContext& ctx) override;

    void set_implementation(FunctionImpl impl) { impl_ = std::move(impl); }

    /// @brief Set whether this function is pure
    void set_pure(bool pure) { purity_ = pure ? NodePurity::Pure : NodePurity::Impure; }

protected:
    FunctionImpl impl_;
};

// =============================================================================
// Variable Node
// =============================================================================

/// @brief Get or Set variable node
class VariableNode : public NodeBase {
public:
    enum class Mode { Get, Set };

    VariableNode(NodeId id, NodeTypeId type_id, const std::string& name, Mode mode);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::Variable; }
    [[nodiscard]] NodePurity purity() const override {
        return mode_ == Mode::Get ? NodePurity::Pure : NodePurity::Impure;
    }

    PinId execute(ExecutionContext& ctx) override;

    [[nodiscard]] Mode mode() const { return mode_; }
    [[nodiscard]] VariableId variable_id() const { return variable_id_; }
    void set_variable_id(VariableId id) { variable_id_ = id; }

    [[nodiscard]] PinType variable_type() const { return variable_type_; }
    void set_variable_type(PinType type);

protected:
    Mode mode_;
    VariableId variable_id_;
    PinType variable_type_ = PinType::Any;
};

// =============================================================================
// Flow Control Nodes
// =============================================================================

/// @brief Branch (if/else) node
class BranchNode : public NodeBase {
public:
    BranchNode(NodeId id, NodeTypeId type_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::FlowControl; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Impure; }

    PinId execute(ExecutionContext& ctx) override;
};

/// @brief Sequence node (executes multiple outputs in order)
class SequenceNode : public NodeBase {
public:
    SequenceNode(NodeId id, NodeTypeId type_id, std::size_t output_count = 2);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::FlowControl; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Impure; }

    PinId execute(ExecutionContext& ctx) override;

    /// @brief Add another output
    void add_output();

private:
    std::size_t current_output_ = 0;
};

/// @brief ForLoop node
class ForLoopNode : public NodeBase {
public:
    ForLoopNode(NodeId id, NodeTypeId type_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::FlowControl; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Latent; }

    PinId execute(ExecutionContext& ctx) override;
    PinId resume(ExecutionContext& ctx) override;

private:
    std::int32_t first_index_ = 0;
    std::int32_t last_index_ = 0;
    std::int32_t current_index_ = 0;
};

/// @brief WhileLoop node
class WhileLoopNode : public NodeBase {
public:
    WhileLoopNode(NodeId id, NodeTypeId type_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::FlowControl; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Latent; }

    PinId execute(ExecutionContext& ctx) override;
    PinId resume(ExecutionContext& ctx) override;
};

/// @brief ForEachLoop node
class ForEachLoopNode : public NodeBase {
public:
    ForEachLoopNode(NodeId id, NodeTypeId type_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::FlowControl; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Latent; }

    PinId execute(ExecutionContext& ctx) override;
    PinId resume(ExecutionContext& ctx) override;

private:
    std::size_t current_index_ = 0;
    std::size_t array_size_ = 0;
};

/// @brief Delay node (latent action)
class DelayNode : public NodeBase {
public:
    DelayNode(NodeId id, NodeTypeId type_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::FlowControl; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Latent; }

    PinId execute(ExecutionContext& ctx) override;
    PinId resume(ExecutionContext& ctx) override;

private:
    float delay_seconds_ = 0.0f;
    float elapsed_time_ = 0.0f;
};

/// @brief DoOnce node (executes only once until reset)
class DoOnceNode : public NodeBase {
public:
    DoOnceNode(NodeId id, NodeTypeId type_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::FlowControl; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Impure; }

    PinId execute(ExecutionContext& ctx) override;

    void reset() { has_executed_ = false; }

private:
    bool has_executed_ = false;
};

/// @brief FlipFlop node (alternates between two outputs)
class FlipFlopNode : public NodeBase {
public:
    FlipFlopNode(NodeId id, NodeTypeId type_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::FlowControl; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Impure; }

    PinId execute(ExecutionContext& ctx) override;

private:
    bool is_a_ = true;
};

/// @brief Gate node (can be opened/closed)
class GateNode : public NodeBase {
public:
    GateNode(NodeId id, NodeTypeId type_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::FlowControl; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Impure; }

    PinId execute(ExecutionContext& ctx) override;

    void set_open(bool open) { is_open_ = open; }
    [[nodiscard]] bool is_open() const { return is_open_; }

private:
    bool is_open_ = true;
};

// =============================================================================
// Math Node
// =============================================================================

/// @brief Generic math operation node
class MathNode : public NodeBase {
public:
    enum class Operation {
        // Arithmetic
        Add, Subtract, Multiply, Divide, Modulo, Negate, Abs,
        // Powers
        Power, Sqrt, Exp, Log, Log10,
        // Trigonometry
        Sin, Cos, Tan, Asin, Acos, Atan, Atan2,
        // Rounding
        Floor, Ceil, Round, Truncate,
        // Comparison
        Min, Max, Clamp, Lerp,
        // Vector
        Dot, Cross, Normalize, Length, Distance,
        // Other
        Sign, Frac, Step, SmoothStep
    };

    MathNode(NodeId id, NodeTypeId type_id, Operation op);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::Math; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Pure; }

    PinId execute(ExecutionContext& ctx) override;

    [[nodiscard]] Operation operation() const { return operation_; }

private:
    void setup_pins();
    Operation operation_;
};

// =============================================================================
// Conversion Node
// =============================================================================

/// @brief Type conversion node
class ConversionNode : public NodeBase {
public:
    ConversionNode(NodeId id, NodeTypeId type_id, PinType from_type, PinType to_type);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::Conversion; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Pure; }

    PinId execute(ExecutionContext& ctx) override;

private:
    PinType from_type_;
    PinType to_type_;
};

// =============================================================================
// Comment Node
// =============================================================================

/// @brief Visual comment/note node (no execution)
class CommentNode : public NodeBase {
public:
    CommentNode(NodeId id, NodeTypeId type_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::Comment; }

    PinId execute([[maybe_unused]] ExecutionContext& ctx) override { return PinId{}; }

    [[nodiscard]] const std::string& text() const { return text_; }
    void set_text(const std::string& text) { text_ = text; }

    [[nodiscard]] std::uint32_t background_color() const { return bg_color_; }
    void set_background_color(std::uint32_t color) { bg_color_ = color; }

    [[nodiscard]] bool is_bubble() const { return is_bubble_; }
    void set_bubble(bool bubble) { is_bubble_ = bubble; }

private:
    std::string text_;
    std::uint32_t bg_color_ = 0x44FFFFFF;
    bool is_bubble_ = false;
};

// =============================================================================
// Reroute Node
// =============================================================================

/// @brief Wire reroute node for visual organization
class RerouteNode : public NodeBase {
public:
    RerouteNode(NodeId id, NodeTypeId type_id, PinType pin_type = PinType::Any);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::Reroute; }
    [[nodiscard]] NodePurity purity() const override { return NodePurity::Pure; }
    [[nodiscard]] bool is_compact() const override { return true; }

    PinId execute(ExecutionContext& ctx) override;

    void set_pin_type(PinType type);

private:
    PinType pin_type_;
};

// =============================================================================
// Subgraph Node
// =============================================================================

/// @brief Node that executes a subgraph
class SubgraphNode : public NodeBase {
public:
    SubgraphNode(NodeId id, NodeTypeId type_id, SubgraphId subgraph_id);

    [[nodiscard]] NodeCategory category() const override { return NodeCategory::Subgraph; }

    PinId execute(ExecutionContext& ctx) override;

    [[nodiscard]] SubgraphId subgraph_id() const { return subgraph_id_; }

    /// @brief Update pins based on subgraph interface
    void sync_pins(const class Graph& subgraph);

private:
    SubgraphId subgraph_id_;
};

// =============================================================================
// Node Builder
// =============================================================================

/// @brief Fluent builder for creating nodes
class NodeBuilder {
public:
    explicit NodeBuilder(NodeTypeId type_id);

    /// @brief Set node name
    NodeBuilder& name(const std::string& name);

    /// @brief Set display title
    NodeBuilder& title(const std::string& title);

    /// @brief Set category path
    NodeBuilder& category(const std::string& category);

    /// @brief Set category enum
    NodeBuilder& category(NodeCategory category);

    /// @brief Set tooltip
    NodeBuilder& tooltip(const std::string& tooltip);

    /// @brief Set search keywords
    NodeBuilder& keywords(const std::string& keywords);

    /// @brief Set purity
    NodeBuilder& purity(NodePurity purity);
    NodeBuilder& pure() { return purity(NodePurity::Pure); }
    NodeBuilder& impure() { return purity(NodePurity::Impure); }
    NodeBuilder& latent() { return purity(NodePurity::Latent); }

    /// @brief Set compact mode
    NodeBuilder& compact(bool enabled = true);

    /// @brief Set title bar color
    NodeBuilder& color(std::uint32_t color);

    /// @brief Add exec input
    NodeBuilder& exec_in(const std::string& name = "");

    /// @brief Add exec output
    NodeBuilder& exec_out(const std::string& name = "");

    /// @brief Add input pin
    NodeBuilder& input(const std::string& name, PinType type);

    /// @brief Add output pin
    NodeBuilder& output(const std::string& name, PinType type);

    /// @brief Add input with default value
    template <typename T>
    NodeBuilder& input(const std::string& name, PinType type, T&& default_value);

    /// @brief Build the node template
    [[nodiscard]] NodeTemplate build() const;

    /// @brief Build and register with the registry
    NodeTypeId build_and_register(NodeRegistry& registry);

private:
    NodeTemplate template_;
    std::vector<std::pair<std::string, PinType>> pending_inputs_;
    std::vector<std::pair<std::string, PinType>> pending_outputs_;
    std::size_t exec_in_count_ = 0;
    std::size_t exec_out_count_ = 0;
};

} // namespace void_graph
