#include "node.hpp"
#include "graph.hpp"
#include "registry.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace void_graph {

// =============================================================================
// NodeBase Implementation
// =============================================================================

NodeBase::NodeBase(NodeId id, NodeTypeId type_id, const std::string& name)
    : id_(id), type_id_(type_id), name_(name) {}

const Pin* NodeBase::find_pin(PinId id) const {
    for (const auto& pin : input_pins_) {
        if (pin.id == id) return &pin;
    }
    for (const auto& pin : output_pins_) {
        if (pin.id == id) return &pin;
    }
    return nullptr;
}

const Pin* NodeBase::find_pin_by_name(const std::string& name) const {
    for (const auto& pin : input_pins_) {
        if (pin.name == name) return &pin;
    }
    for (const auto& pin : output_pins_) {
        if (pin.name == name) return &pin;
    }
    return nullptr;
}

Pin& NodeBase::add_input_pin(const std::string& name, PinType type) {
    Pin pin;
    pin.id = PinId::create(next_pin_id_++, 0);
    pin.owner = id_;
    pin.name = name;
    pin.direction = PinDirection::Input;
    pin.type = type;
    pin.color = pin_type_color(type);
    input_pins_.push_back(std::move(pin));
    return input_pins_.back();
}

Pin& NodeBase::add_output_pin(const std::string& name, PinType type) {
    Pin pin;
    pin.id = PinId::create(next_pin_id_++, 0);
    pin.owner = id_;
    pin.name = name;
    pin.direction = PinDirection::Output;
    pin.type = type;
    pin.color = pin_type_color(type);
    output_pins_.push_back(std::move(pin));
    return output_pins_.back();
}

Pin& NodeBase::add_exec_input(const std::string& name) {
    return add_input_pin(name.empty() ? "" : name, PinType::Exec);
}

Pin& NodeBase::add_exec_output(const std::string& name) {
    return add_output_pin(name.empty() ? "" : name, PinType::Exec);
}

Pin* NodeBase::get_mutable_pin(PinId id) {
    for (auto& pin : input_pins_) {
        if (pin.id == id) return &pin;
    }
    for (auto& pin : output_pins_) {
        if (pin.id == id) return &pin;
    }
    return nullptr;
}

PinId NodeBase::first_exec_output() const {
    for (const auto& pin : output_pins_) {
        if (pin.type == PinType::Exec) {
            return pin.id;
        }
    }
    return PinId{};
}

// =============================================================================
// EventNode Implementation
// =============================================================================

EventNode::EventNode(NodeId id, NodeTypeId type_id, const std::string& name)
    : NodeBase(id, type_id, name), event_name_(name) {
    title_color_ = 0xFF880000;  // Red for events
    add_exec_output();
}

PinId EventNode::execute(ExecutionContext& ctx) {
    if (callback_) {
        callback_(ctx);
    }
    return first_exec_output();
}

// =============================================================================
// FunctionNode Implementation
// =============================================================================

FunctionNode::FunctionNode(NodeId id, NodeTypeId type_id, const std::string& name)
    : NodeBase(id, type_id, name) {}

PinId FunctionNode::execute(ExecutionContext& ctx) {
    if (impl_) {
        return impl_(ctx, *this);
    }
    return first_exec_output();
}

// =============================================================================
// VariableNode Implementation
// =============================================================================

VariableNode::VariableNode(NodeId id, NodeTypeId type_id, const std::string& name, Mode mode)
    : NodeBase(id, type_id, name), mode_(mode) {
    title_color_ = mode == Mode::Get ? 0xFF006600 : 0xFF660000;

    if (mode == Mode::Get) {
        add_output_pin("Value", PinType::Any);
    } else {
        add_exec_input();
        add_input_pin("Value", PinType::Any);
        add_exec_output();
    }
}

PinId VariableNode::execute(ExecutionContext& ctx) {
    if (mode_ == Mode::Get) {
        // Get variable value and set output
        auto it = ctx.variables.find(variable_id_);
        if (it != ctx.variables.end()) {
            ctx.pin_values[output_pins_[0].id] = it->second;
        }
        return PinId{};  // Pure node, no exec flow
    } else {
        // Set variable value from input
        auto it = ctx.pin_values.find(input_pins_[1].id);  // Index 1 is Value pin
        if (it != ctx.pin_values.end()) {
            ctx.variables[variable_id_] = it->second;
        }
        return first_exec_output();
    }
}

void VariableNode::set_variable_type(PinType type) {
    variable_type_ = type;
    if (mode_ == Mode::Get) {
        if (!output_pins_.empty()) {
            output_pins_[0].type = type;
            output_pins_[0].color = pin_type_color(type);
        }
    } else {
        if (input_pins_.size() > 1) {
            input_pins_[1].type = type;
            input_pins_[1].color = pin_type_color(type);
        }
    }
}

// =============================================================================
// Flow Control Nodes
// =============================================================================

BranchNode::BranchNode(NodeId id, NodeTypeId type_id)
    : NodeBase(id, type_id, "Branch") {
    category_ = NodeCategory::FlowControl;
    purity_ = NodePurity::Impure;
    title_color_ = 0xFF444444;
    is_compact_ = true;

    add_exec_input();
    add_input_pin("Condition", PinType::Bool);
    add_exec_output("True");
    add_exec_output("False");
}

PinId BranchNode::execute(ExecutionContext& ctx) {
    auto it = ctx.pin_values.find(input_pins_[1].id);
    bool condition = false;
    if (it != ctx.pin_values.end()) {
        if (std::holds_alternative<bool>(it->second)) {
            condition = std::get<bool>(it->second);
        }
    }
    return condition ? output_pins_[0].id : output_pins_[1].id;
}

SequenceNode::SequenceNode(NodeId id, NodeTypeId type_id, std::size_t output_count)
    : NodeBase(id, type_id, "Sequence") {
    category_ = NodeCategory::FlowControl;
    purity_ = NodePurity::Impure;
    title_color_ = 0xFF444444;

    add_exec_input();
    for (std::size_t i = 0; i < output_count; ++i) {
        add_exec_output("Then " + std::to_string(i));
    }
}

PinId SequenceNode::execute([[maybe_unused]] ExecutionContext& ctx) {
    if (current_output_ < output_pins_.size()) {
        return output_pins_[current_output_++].id;
    }
    current_output_ = 0;
    return PinId{};
}

void SequenceNode::add_output() {
    add_exec_output("Then " + std::to_string(output_pins_.size()));
}

ForLoopNode::ForLoopNode(NodeId id, NodeTypeId type_id)
    : NodeBase(id, type_id, "For Loop") {
    category_ = NodeCategory::FlowControl;
    purity_ = NodePurity::Latent;
    title_color_ = 0xFF444444;

    add_exec_input();
    add_input_pin("First Index", PinType::Int);
    add_input_pin("Last Index", PinType::Int);
    add_exec_output("Loop Body");
    add_output_pin("Index", PinType::Int);
    add_exec_output("Completed");
}

PinId ForLoopNode::execute(ExecutionContext& ctx) {
    // Get first and last index
    auto first_it = ctx.pin_values.find(input_pins_[1].id);
    auto last_it = ctx.pin_values.find(input_pins_[2].id);

    first_index_ = 0;
    last_index_ = 0;

    if (first_it != ctx.pin_values.end() && std::holds_alternative<std::int32_t>(first_it->second)) {
        first_index_ = std::get<std::int32_t>(first_it->second);
    }
    if (last_it != ctx.pin_values.end() && std::holds_alternative<std::int32_t>(last_it->second)) {
        last_index_ = std::get<std::int32_t>(last_it->second);
    }

    current_index_ = first_index_;

    if (current_index_ <= last_index_) {
        ctx.pin_values[output_pins_[1].id] = current_index_;
        return output_pins_[0].id;  // Loop Body
    }
    return output_pins_[2].id;  // Completed
}

PinId ForLoopNode::resume(ExecutionContext& ctx) {
    ++current_index_;
    if (current_index_ <= last_index_) {
        ctx.pin_values[output_pins_[1].id] = current_index_;
        return output_pins_[0].id;  // Loop Body
    }
    return output_pins_[2].id;  // Completed
}

WhileLoopNode::WhileLoopNode(NodeId id, NodeTypeId type_id)
    : NodeBase(id, type_id, "While Loop") {
    category_ = NodeCategory::FlowControl;
    purity_ = NodePurity::Latent;
    title_color_ = 0xFF444444;

    add_exec_input();
    add_input_pin("Condition", PinType::Bool);
    add_exec_output("Loop Body");
    add_exec_output("Completed");
}

PinId WhileLoopNode::execute(ExecutionContext& ctx) {
    auto it = ctx.pin_values.find(input_pins_[1].id);
    bool condition = false;
    if (it != ctx.pin_values.end() && std::holds_alternative<bool>(it->second)) {
        condition = std::get<bool>(it->second);
    }
    return condition ? output_pins_[0].id : output_pins_[1].id;
}

PinId WhileLoopNode::resume(ExecutionContext& ctx) {
    return execute(ctx);
}

ForEachLoopNode::ForEachLoopNode(NodeId id, NodeTypeId type_id)
    : NodeBase(id, type_id, "For Each Loop") {
    category_ = NodeCategory::FlowControl;
    purity_ = NodePurity::Latent;
    title_color_ = 0xFF444444;

    add_exec_input();
    add_input_pin("Array", PinType::Array);
    add_exec_output("Loop Body");
    add_output_pin("Element", PinType::Any);
    add_output_pin("Index", PinType::Int);
    add_exec_output("Completed");
}

PinId ForEachLoopNode::execute(ExecutionContext& ctx) {
    auto it = ctx.pin_values.find(input_pins_[1].id);
    if (it == ctx.pin_values.end()) {
        return output_pins_[3].id;  // Completed
    }

    if (std::holds_alternative<std::vector<std::any>>(it->second)) {
        const auto& arr = std::get<std::vector<std::any>>(it->second);
        array_size_ = arr.size();
        current_index_ = 0;

        if (!arr.empty()) {
            ctx.pin_values[output_pins_[1].id] = arr[0];
            ctx.pin_values[output_pins_[2].id] = static_cast<std::int32_t>(0);
            return output_pins_[0].id;  // Loop Body
        }
    }

    return output_pins_[3].id;  // Completed
}

PinId ForEachLoopNode::resume(ExecutionContext& ctx) {
    ++current_index_;

    auto it = ctx.pin_values.find(input_pins_[1].id);
    if (it != ctx.pin_values.end() && std::holds_alternative<std::vector<std::any>>(it->second)) {
        const auto& arr = std::get<std::vector<std::any>>(it->second);
        if (current_index_ < arr.size()) {
            ctx.pin_values[output_pins_[1].id] = arr[current_index_];
            ctx.pin_values[output_pins_[2].id] = static_cast<std::int32_t>(current_index_);
            return output_pins_[0].id;  // Loop Body
        }
    }

    return output_pins_[3].id;  // Completed
}

DelayNode::DelayNode(NodeId id, NodeTypeId type_id)
    : NodeBase(id, type_id, "Delay") {
    category_ = NodeCategory::FlowControl;
    purity_ = NodePurity::Latent;
    title_color_ = 0xFF444444;

    add_exec_input();
    add_input_pin("Duration", PinType::Float);
    add_exec_output("Completed");
}

PinId DelayNode::execute(ExecutionContext& ctx) {
    auto it = ctx.pin_values.find(input_pins_[1].id);
    delay_seconds_ = 0.0f;
    if (it != ctx.pin_values.end() && std::holds_alternative<float>(it->second)) {
        delay_seconds_ = std::get<float>(it->second);
    }
    elapsed_time_ = 0.0f;
    state_ = NodeState::Suspended;
    return PinId{};  // Suspend
}

PinId DelayNode::resume(ExecutionContext& ctx) {
    elapsed_time_ += ctx.delta_time;
    if (elapsed_time_ >= delay_seconds_) {
        state_ = NodeState::Completed;
        return output_pins_[0].id;
    }
    return PinId{};  // Still waiting
}

DoOnceNode::DoOnceNode(NodeId id, NodeTypeId type_id)
    : NodeBase(id, type_id, "Do Once") {
    category_ = NodeCategory::FlowControl;
    purity_ = NodePurity::Impure;
    title_color_ = 0xFF444444;

    add_exec_input();
    add_exec_input("Reset");
    add_exec_output("Completed");
}

PinId DoOnceNode::execute(ExecutionContext& ctx) {
    // Check which exec input was triggered
    if (ctx.current_exec_pin == input_pins_[1].id) {
        // Reset input
        has_executed_ = false;
        return PinId{};
    }

    if (!has_executed_) {
        has_executed_ = true;
        return output_pins_[0].id;
    }
    return PinId{};
}

FlipFlopNode::FlipFlopNode(NodeId id, NodeTypeId type_id)
    : NodeBase(id, type_id, "Flip Flop") {
    category_ = NodeCategory::FlowControl;
    purity_ = NodePurity::Impure;
    title_color_ = 0xFF444444;

    add_exec_input();
    add_exec_output("A");
    add_exec_output("B");
    add_output_pin("Is A", PinType::Bool);
}

PinId FlipFlopNode::execute(ExecutionContext& ctx) {
    is_a_ = !is_a_;
    ctx.pin_values[output_pins_[2].id] = is_a_;
    return is_a_ ? output_pins_[0].id : output_pins_[1].id;
}

GateNode::GateNode(NodeId id, NodeTypeId type_id)
    : NodeBase(id, type_id, "Gate") {
    category_ = NodeCategory::FlowControl;
    purity_ = NodePurity::Impure;
    title_color_ = 0xFF444444;

    add_exec_input("Enter");
    add_exec_input("Open");
    add_exec_input("Close");
    add_exec_input("Toggle");
    add_input_pin("Start Closed", PinType::Bool);
    add_exec_output("Exit");
}

PinId GateNode::execute(ExecutionContext& ctx) {
    if (ctx.current_exec_pin == input_pins_[1].id) {
        is_open_ = true;
        return PinId{};
    }
    if (ctx.current_exec_pin == input_pins_[2].id) {
        is_open_ = false;
        return PinId{};
    }
    if (ctx.current_exec_pin == input_pins_[3].id) {
        is_open_ = !is_open_;
        return PinId{};
    }
    // Enter
    return is_open_ ? output_pins_[0].id : PinId{};
}

// =============================================================================
// MathNode Implementation
// =============================================================================

MathNode::MathNode(NodeId id, NodeTypeId type_id, Operation op)
    : NodeBase(id, type_id, "Math"), operation_(op) {
    category_ = NodeCategory::Math;
    purity_ = NodePurity::Pure;
    title_color_ = 0xFF005500;
    is_compact_ = true;

    setup_pins();
}

void MathNode::setup_pins() {
    switch (operation_) {
        // Binary arithmetic
        case Operation::Add:
        case Operation::Subtract:
        case Operation::Multiply:
        case Operation::Divide:
        case Operation::Modulo:
        case Operation::Power:
        case Operation::Min:
        case Operation::Max:
        case Operation::Atan2:
            add_input_pin("A", PinType::Float);
            add_input_pin("B", PinType::Float);
            add_output_pin("Result", PinType::Float);
            break;

        // Unary
        case Operation::Negate:
        case Operation::Abs:
        case Operation::Sqrt:
        case Operation::Exp:
        case Operation::Log:
        case Operation::Log10:
        case Operation::Sin:
        case Operation::Cos:
        case Operation::Tan:
        case Operation::Asin:
        case Operation::Acos:
        case Operation::Atan:
        case Operation::Floor:
        case Operation::Ceil:
        case Operation::Round:
        case Operation::Truncate:
        case Operation::Sign:
        case Operation::Frac:
            add_input_pin("Value", PinType::Float);
            add_output_pin("Result", PinType::Float);
            break;

        // Ternary
        case Operation::Clamp:
            add_input_pin("Value", PinType::Float);
            add_input_pin("Min", PinType::Float);
            add_input_pin("Max", PinType::Float);
            add_output_pin("Result", PinType::Float);
            break;

        case Operation::Lerp:
            add_input_pin("A", PinType::Float);
            add_input_pin("B", PinType::Float);
            add_input_pin("Alpha", PinType::Float);
            add_output_pin("Result", PinType::Float);
            break;

        case Operation::Step:
            add_input_pin("Edge", PinType::Float);
            add_input_pin("X", PinType::Float);
            add_output_pin("Result", PinType::Float);
            break;

        case Operation::SmoothStep:
            add_input_pin("Edge0", PinType::Float);
            add_input_pin("Edge1", PinType::Float);
            add_input_pin("X", PinType::Float);
            add_output_pin("Result", PinType::Float);
            break;

        // Vector operations
        case Operation::Dot:
        case Operation::Distance:
            add_input_pin("A", PinType::Vec3);
            add_input_pin("B", PinType::Vec3);
            add_output_pin("Result", PinType::Float);
            break;

        case Operation::Cross:
            add_input_pin("A", PinType::Vec3);
            add_input_pin("B", PinType::Vec3);
            add_output_pin("Result", PinType::Vec3);
            break;

        case Operation::Normalize:
            add_input_pin("Vector", PinType::Vec3);
            add_output_pin("Result", PinType::Vec3);
            break;

        case Operation::Length:
            add_input_pin("Vector", PinType::Vec3);
            add_output_pin("Result", PinType::Float);
            break;
    }
}

PinId MathNode::execute(ExecutionContext& ctx) {
    auto get_float = [&](std::size_t pin_index) -> float {
        auto it = ctx.pin_values.find(input_pins_[pin_index].id);
        if (it != ctx.pin_values.end()) {
            if (std::holds_alternative<float>(it->second)) {
                return std::get<float>(it->second);
            }
            if (std::holds_alternative<std::int32_t>(it->second)) {
                return static_cast<float>(std::get<std::int32_t>(it->second));
            }
            if (std::holds_alternative<double>(it->second)) {
                return static_cast<float>(std::get<double>(it->second));
            }
        }
        return 0.0f;
    };

    float result = 0.0f;

    switch (operation_) {
        case Operation::Add: result = get_float(0) + get_float(1); break;
        case Operation::Subtract: result = get_float(0) - get_float(1); break;
        case Operation::Multiply: result = get_float(0) * get_float(1); break;
        case Operation::Divide: {
            float b = get_float(1);
            result = b != 0.0f ? get_float(0) / b : 0.0f;
            break;
        }
        case Operation::Modulo: {
            float b = get_float(1);
            result = b != 0.0f ? std::fmod(get_float(0), b) : 0.0f;
            break;
        }
        case Operation::Negate: result = -get_float(0); break;
        case Operation::Abs: result = std::abs(get_float(0)); break;
        case Operation::Power: result = std::pow(get_float(0), get_float(1)); break;
        case Operation::Sqrt: result = std::sqrt(get_float(0)); break;
        case Operation::Exp: result = std::exp(get_float(0)); break;
        case Operation::Log: result = std::log(get_float(0)); break;
        case Operation::Log10: result = std::log10(get_float(0)); break;
        case Operation::Sin: result = std::sin(get_float(0)); break;
        case Operation::Cos: result = std::cos(get_float(0)); break;
        case Operation::Tan: result = std::tan(get_float(0)); break;
        case Operation::Asin: result = std::asin(get_float(0)); break;
        case Operation::Acos: result = std::acos(get_float(0)); break;
        case Operation::Atan: result = std::atan(get_float(0)); break;
        case Operation::Atan2: result = std::atan2(get_float(0), get_float(1)); break;
        case Operation::Floor: result = std::floor(get_float(0)); break;
        case Operation::Ceil: result = std::ceil(get_float(0)); break;
        case Operation::Round: result = std::round(get_float(0)); break;
        case Operation::Truncate: result = std::trunc(get_float(0)); break;
        case Operation::Min: result = std::min(get_float(0), get_float(1)); break;
        case Operation::Max: result = std::max(get_float(0), get_float(1)); break;
        case Operation::Clamp: result = std::clamp(get_float(0), get_float(1), get_float(2)); break;
        case Operation::Lerp: {
            float a = get_float(0), b = get_float(1), t = get_float(2);
            result = a + (b - a) * t;
            break;
        }
        case Operation::Sign: result = static_cast<float>((get_float(0) > 0.0f) - (get_float(0) < 0.0f)); break;
        case Operation::Frac: {
            float v = get_float(0);
            result = v - std::floor(v);
            break;
        }
        case Operation::Step: result = get_float(1) >= get_float(0) ? 1.0f : 0.0f; break;
        case Operation::SmoothStep: {
            float e0 = get_float(0), e1 = get_float(1), x = get_float(2);
            float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
            result = t * t * (3.0f - 2.0f * t);
            break;
        }
        default: break;
    }

    ctx.pin_values[output_pins_[0].id] = result;
    return PinId{};  // Pure node
}

// =============================================================================
// ConversionNode Implementation
// =============================================================================

ConversionNode::ConversionNode(NodeId id, NodeTypeId type_id, PinType from_type, PinType to_type)
    : NodeBase(id, type_id, std::string(pin_type_name(from_type)) + " to " + pin_type_name(to_type)),
      from_type_(from_type), to_type_(to_type) {
    category_ = NodeCategory::Conversion;
    purity_ = NodePurity::Pure;
    title_color_ = 0xFF555500;
    is_compact_ = true;

    add_input_pin("Value", from_type);
    add_output_pin("Result", to_type);
}

PinId ConversionNode::execute(ExecutionContext& ctx) {
    auto it = ctx.pin_values.find(input_pins_[0].id);
    if (it == ctx.pin_values.end()) {
        return PinId{};
    }

    const PinValue& input = it->second;
    PinValue output;

    // Conversion logic
    switch (to_type_) {
        case PinType::String: {
            std::ostringstream ss;
            if (std::holds_alternative<bool>(input)) ss << (std::get<bool>(input) ? "true" : "false");
            else if (std::holds_alternative<std::int32_t>(input)) ss << std::get<std::int32_t>(input);
            else if (std::holds_alternative<std::int64_t>(input)) ss << std::get<std::int64_t>(input);
            else if (std::holds_alternative<float>(input)) ss << std::get<float>(input);
            else if (std::holds_alternative<double>(input)) ss << std::get<double>(input);
            else if (std::holds_alternative<std::string>(input)) ss << std::get<std::string>(input);
            output = ss.str();
            break;
        }
        case PinType::Float: {
            if (std::holds_alternative<std::int32_t>(input)) output = static_cast<float>(std::get<std::int32_t>(input));
            else if (std::holds_alternative<std::int64_t>(input)) output = static_cast<float>(std::get<std::int64_t>(input));
            else if (std::holds_alternative<double>(input)) output = static_cast<float>(std::get<double>(input));
            else if (std::holds_alternative<bool>(input)) output = std::get<bool>(input) ? 1.0f : 0.0f;
            else if (std::holds_alternative<float>(input)) output = std::get<float>(input);
            break;
        }
        case PinType::Int: {
            if (std::holds_alternative<float>(input)) output = static_cast<std::int32_t>(std::get<float>(input));
            else if (std::holds_alternative<double>(input)) output = static_cast<std::int32_t>(std::get<double>(input));
            else if (std::holds_alternative<std::int64_t>(input)) output = static_cast<std::int32_t>(std::get<std::int64_t>(input));
            else if (std::holds_alternative<bool>(input)) output = std::get<bool>(input) ? 1 : 0;
            else if (std::holds_alternative<std::int32_t>(input)) output = std::get<std::int32_t>(input);
            break;
        }
        case PinType::Bool: {
            if (std::holds_alternative<float>(input)) output = std::get<float>(input) != 0.0f;
            else if (std::holds_alternative<double>(input)) output = std::get<double>(input) != 0.0;
            else if (std::holds_alternative<std::int32_t>(input)) output = std::get<std::int32_t>(input) != 0;
            else if (std::holds_alternative<std::int64_t>(input)) output = std::get<std::int64_t>(input) != 0;
            else if (std::holds_alternative<std::string>(input)) output = !std::get<std::string>(input).empty();
            else if (std::holds_alternative<bool>(input)) output = std::get<bool>(input);
            break;
        }
        default:
            output = input;
            break;
    }

    ctx.pin_values[output_pins_[0].id] = std::move(output);
    return PinId{};
}

// =============================================================================
// CommentNode Implementation
// =============================================================================

CommentNode::CommentNode(NodeId id, NodeTypeId type_id)
    : NodeBase(id, type_id, "Comment") {
    category_ = NodeCategory::Comment;
    title_color_ = 0x00000000;  // No title bar
    width_ = 200.0f;
    height_ = 100.0f;
}

// =============================================================================
// RerouteNode Implementation
// =============================================================================

RerouteNode::RerouteNode(NodeId id, NodeTypeId type_id, PinType pin_type)
    : NodeBase(id, type_id, "Reroute"), pin_type_(pin_type) {
    category_ = NodeCategory::Reroute;
    width_ = 24.0f;
    height_ = 24.0f;

    add_input_pin("", pin_type);
    add_output_pin("", pin_type);
}

PinId RerouteNode::execute(ExecutionContext& ctx) {
    // Pass through
    auto it = ctx.pin_values.find(input_pins_[0].id);
    if (it != ctx.pin_values.end()) {
        ctx.pin_values[output_pins_[0].id] = it->second;
    }
    return pin_type_ == PinType::Exec ? output_pins_[0].id : PinId{};
}

void RerouteNode::set_pin_type(PinType type) {
    pin_type_ = type;
    if (!input_pins_.empty()) {
        input_pins_[0].type = type;
        input_pins_[0].color = pin_type_color(type);
    }
    if (!output_pins_.empty()) {
        output_pins_[0].type = type;
        output_pins_[0].color = pin_type_color(type);
    }
}

// =============================================================================
// SubgraphNode Implementation
// =============================================================================

SubgraphNode::SubgraphNode(NodeId id, NodeTypeId type_id, SubgraphId subgraph_id)
    : NodeBase(id, type_id, "Subgraph"), subgraph_id_(subgraph_id) {
    category_ = NodeCategory::Subgraph;
    title_color_ = 0xFF0066AA;
}

PinId SubgraphNode::execute([[maybe_unused]] ExecutionContext& ctx) {
    // Subgraph execution is handled by the executor
    return first_exec_output();
}

void SubgraphNode::sync_pins(const Graph& subgraph) {
    input_pins_.clear();
    output_pins_.clear();

    // Add exec pins
    if (!subgraph.interface_inputs().empty()) {
        add_exec_input();
    }
    if (!subgraph.interface_outputs().empty()) {
        add_exec_output();
    }

    // Copy interface pins
    for (const auto& pin : subgraph.interface_inputs()) {
        add_input_pin(pin.name, pin.type);
    }
    for (const auto& pin : subgraph.interface_outputs()) {
        add_output_pin(pin.name, pin.type);
    }

    name_ = subgraph.name();
}

// =============================================================================
// NodeBuilder Implementation
// =============================================================================

NodeBuilder::NodeBuilder(NodeTypeId type_id) {
    template_.id = type_id;
}

NodeBuilder& NodeBuilder::name(const std::string& n) {
    template_.name = n;
    return *this;
}

NodeBuilder& NodeBuilder::title([[maybe_unused]] const std::string& t) {
    // Title is stored in name for templates
    return *this;
}

NodeBuilder& NodeBuilder::category(const std::string& c) {
    template_.category = c;
    return *this;
}

NodeBuilder& NodeBuilder::category(NodeCategory c) {
    template_.node_category = c;
    return *this;
}

NodeBuilder& NodeBuilder::tooltip(const std::string& t) {
    template_.tooltip = t;
    return *this;
}

NodeBuilder& NodeBuilder::keywords(const std::string& k) {
    template_.keywords = k;
    return *this;
}

NodeBuilder& NodeBuilder::purity(NodePurity p) {
    template_.purity = p;
    return *this;
}

NodeBuilder& NodeBuilder::compact(bool enabled) {
    template_.is_compact = enabled;
    return *this;
}

NodeBuilder& NodeBuilder::color(std::uint32_t c) {
    template_.title_color = c;
    return *this;
}

NodeBuilder& NodeBuilder::exec_in(const std::string& n) {
    Pin pin;
    pin.name = n;
    pin.direction = PinDirection::Input;
    pin.type = PinType::Exec;
    template_.input_pins.push_back(std::move(pin));
    ++exec_in_count_;
    return *this;
}

NodeBuilder& NodeBuilder::exec_out(const std::string& n) {
    Pin pin;
    pin.name = n;
    pin.direction = PinDirection::Output;
    pin.type = PinType::Exec;
    template_.output_pins.push_back(std::move(pin));
    ++exec_out_count_;
    return *this;
}

NodeBuilder& NodeBuilder::input(const std::string& n, PinType type) {
    Pin pin;
    pin.name = n;
    pin.direction = PinDirection::Input;
    pin.type = type;
    pin.color = pin_type_color(type);
    template_.input_pins.push_back(std::move(pin));
    return *this;
}

NodeBuilder& NodeBuilder::output(const std::string& n, PinType type) {
    Pin pin;
    pin.name = n;
    pin.direction = PinDirection::Output;
    pin.type = type;
    pin.color = pin_type_color(type);
    template_.output_pins.push_back(std::move(pin));
    return *this;
}

NodeTemplate NodeBuilder::build() const {
    return template_;
}

NodeTypeId NodeBuilder::build_and_register(NodeRegistry& registry) {
    return registry.register_node(template_);
}

} // namespace void_graph
