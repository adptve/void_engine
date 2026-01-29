#include "registry.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace void_graph {

// =============================================================================
// NodeRegistry Implementation
// =============================================================================

NodeRegistry::NodeRegistry() = default;

NodeRegistry& NodeRegistry::instance() {
    static NodeRegistry registry;
    return registry;
}

NodeTypeId NodeRegistry::register_node(const NodeTemplate& tmpl) {
    NodeTypeId id = tmpl.id.is_valid() ? tmpl.id : NodeTypeId::from_bits(next_type_id_++);
    templates_[id] = tmpl;
    templates_[id].id = id;
    name_to_id_[tmpl.name] = id;
    return id;
}

void NodeRegistry::register_node(NodeTypeId id, const NodeTemplate& tmpl) {
    templates_[id] = tmpl;
    templates_[id].id = id;
    name_to_id_[tmpl.name] = id;
}

bool NodeRegistry::unregister_node(NodeTypeId id) {
    auto it = templates_.find(id);
    if (it == templates_.end()) return false;

    name_to_id_.erase(it->second.name);
    templates_.erase(it);
    return true;
}

bool NodeRegistry::has_node(NodeTypeId id) const {
    return templates_.count(id) > 0;
}

const NodeTemplate* NodeRegistry::get_template(NodeTypeId id) const {
    auto it = templates_.find(id);
    if (it == templates_.end()) return nullptr;
    return &it->second;
}

const NodeTemplate* NodeRegistry::find_template(const std::string& name) const {
    auto it = name_to_id_.find(name);
    if (it == name_to_id_.end()) return nullptr;
    return get_template(it->second);
}

std::unique_ptr<INode> NodeRegistry::create_node(NodeTypeId type_id, [[maybe_unused]] NodeId node_id) const {
    const NodeTemplate* tmpl = get_template(type_id);
    if (!tmpl || !tmpl->create) return nullptr;

    auto node = tmpl->create();
    // Note: The factory should create the node with the correct ID
    return node;
}

std::unique_ptr<INode> NodeRegistry::create_node(const std::string& name, NodeId node_id) const {
    const NodeTemplate* tmpl = find_template(name);
    if (!tmpl) return nullptr;
    return create_node(tmpl->id, node_id);
}

std::vector<const NodeTemplate*> NodeRegistry::all_templates() const {
    std::vector<const NodeTemplate*> result;
    result.reserve(templates_.size());
    for (const auto& [id, tmpl] : templates_) {
        result.push_back(&tmpl);
    }
    return result;
}

std::vector<const NodeTemplate*> NodeRegistry::templates_by_category(NodeCategory category) const {
    std::vector<const NodeTemplate*> result;
    for (const auto& [id, tmpl] : templates_) {
        if (tmpl.node_category == category) {
            result.push_back(&tmpl);
        }
    }
    return result;
}

std::vector<const NodeTemplate*> NodeRegistry::templates_by_path(const std::string& path) const {
    std::vector<const NodeTemplate*> result;
    for (const auto& [id, tmpl] : templates_) {
        if (tmpl.category.find(path) == 0) {
            result.push_back(&tmpl);
        }
    }
    return result;
}

std::vector<const NodeTemplate*> NodeRegistry::search(const std::string& query) const {
    std::vector<const NodeTemplate*> result;

    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& [id, tmpl] : templates_) {
        std::string lower_name = tmpl.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::string lower_keywords = tmpl.keywords;
        std::transform(lower_keywords.begin(), lower_keywords.end(), lower_keywords.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower_name.find(lower_query) != std::string::npos ||
            lower_keywords.find(lower_query) != std::string::npos) {
            result.push_back(&tmpl);
        }
    }

    return result;
}

std::vector<std::string> NodeRegistry::all_categories() const {
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;

    for (const auto& [id, tmpl] : templates_) {
        if (!tmpl.category.empty() && !seen.count(tmpl.category)) {
            result.push_back(tmpl.category);
            seen.insert(tmpl.category);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

void NodeRegistry::register_builtins() {
    register_event_nodes();
    register_flow_control_nodes();
    register_math_nodes();
    register_conversion_nodes();
    register_utility_nodes();
    register_string_nodes();
    register_array_nodes();
    register_debug_nodes();
    register_entity_nodes();
    register_physics_nodes();
    register_audio_nodes();
    register_combat_nodes();
    register_inventory_nodes();
    register_ai_nodes();
    register_input_nodes();
    register_ui_nodes();
    register_time_nodes();
}

void NodeRegistry::register_event_nodes() {
    static std::uint32_t next_id = 1;
    auto* counter = &next_id;

    // Event Begin Play
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::EventBeginPlay;
        tmpl.name = "Event Begin Play";
        tmpl.category = "Events";
        tmpl.tooltip = "Called when the graph starts";
        tmpl.node_category = NodeCategory::Event;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF880000;
        tmpl.keywords = "start init begin";

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            return std::make_unique<EventNode>(
                NodeId::from_bits((*counter)++), builtin::EventBeginPlay, "Event Begin Play");
        };

        register_node(tmpl);
    }

    // Event Tick
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::EventTick;
        tmpl.name = "Event Tick";
        tmpl.category = "Events";
        tmpl.tooltip = "Called every frame";
        tmpl.node_category = NodeCategory::Event;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF880000;
        tmpl.keywords = "update frame";

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        Pin delta_time;
        delta_time.name = "Delta Time";
        delta_time.direction = PinDirection::Output;
        delta_time.type = PinType::Float;
        tmpl.output_pins.push_back(std::move(delta_time));

        tmpl.create = [counter]() {
            auto node = std::make_unique<EventNode>(
                NodeId::from_bits((*counter)++), builtin::EventTick, "Event Tick");
            node->set_event_name("Tick");
            return node;
        };

        register_node(tmpl);
    }

    // Event End Play
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::EventEndPlay;
        tmpl.name = "Event End Play";
        tmpl.category = "Events";
        tmpl.tooltip = "Called when the graph stops";
        tmpl.node_category = NodeCategory::Event;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF880000;
        tmpl.keywords = "stop end shutdown";

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            return std::make_unique<EventNode>(
                NodeId::from_bits((*counter)++), builtin::EventEndPlay, "Event End Play");
        };

        register_node(tmpl);
    }
}

void NodeRegistry::register_flow_control_nodes() {
    static std::uint32_t next_id = 1000;
    auto* counter = &next_id;

    // Branch
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::Branch;
        tmpl.name = "Branch";
        tmpl.category = "Flow Control";
        tmpl.tooltip = "If/else branching";
        tmpl.node_category = NodeCategory::FlowControl;
        tmpl.purity = NodePurity::Impure;
        tmpl.is_compact = true;
        tmpl.keywords = "if else condition";

        tmpl.create = [counter]() {
            return std::make_unique<BranchNode>(NodeId::from_bits((*counter)++), builtin::Branch);
        };

        register_node(tmpl);
    }

    // Sequence
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::Sequence;
        tmpl.name = "Sequence";
        tmpl.category = "Flow Control";
        tmpl.tooltip = "Execute multiple outputs in order";
        tmpl.node_category = NodeCategory::FlowControl;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "order multiple";

        tmpl.create = [counter]() {
            return std::make_unique<SequenceNode>(NodeId::from_bits((*counter)++), builtin::Sequence, 2);
        };

        register_node(tmpl);
    }

    // For Loop
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ForLoop;
        tmpl.name = "For Loop";
        tmpl.category = "Flow Control";
        tmpl.tooltip = "Iterate from first to last index";
        tmpl.node_category = NodeCategory::FlowControl;
        tmpl.purity = NodePurity::Latent;
        tmpl.keywords = "iterate repeat count";

        tmpl.create = [counter]() {
            return std::make_unique<ForLoopNode>(NodeId::from_bits((*counter)++), builtin::ForLoop);
        };

        register_node(tmpl);
    }

    // While Loop
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::WhileLoop;
        tmpl.name = "While Loop";
        tmpl.category = "Flow Control";
        tmpl.tooltip = "Loop while condition is true";
        tmpl.node_category = NodeCategory::FlowControl;
        tmpl.purity = NodePurity::Latent;
        tmpl.keywords = "condition repeat";

        tmpl.create = [counter]() {
            return std::make_unique<WhileLoopNode>(NodeId::from_bits((*counter)++), builtin::WhileLoop);
        };

        register_node(tmpl);
    }

    // Delay
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::Delay;
        tmpl.name = "Delay";
        tmpl.category = "Flow Control";
        tmpl.tooltip = "Wait for a duration before continuing";
        tmpl.node_category = NodeCategory::FlowControl;
        tmpl.purity = NodePurity::Latent;
        tmpl.keywords = "wait time pause";

        tmpl.create = [counter]() {
            return std::make_unique<DelayNode>(NodeId::from_bits((*counter)++), builtin::Delay);
        };

        register_node(tmpl);
    }

    // Do Once
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::DoOnce;
        tmpl.name = "Do Once";
        tmpl.category = "Flow Control";
        tmpl.tooltip = "Execute only once until reset";
        tmpl.node_category = NodeCategory::FlowControl;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "single once";

        tmpl.create = [counter]() {
            return std::make_unique<DoOnceNode>(NodeId::from_bits((*counter)++), builtin::DoOnce);
        };

        register_node(tmpl);
    }

    // Flip Flop
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::FlipFlop;
        tmpl.name = "Flip Flop";
        tmpl.category = "Flow Control";
        tmpl.tooltip = "Alternate between two outputs";
        tmpl.node_category = NodeCategory::FlowControl;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "toggle alternate switch";

        tmpl.create = [counter]() {
            return std::make_unique<FlipFlopNode>(NodeId::from_bits((*counter)++), builtin::FlipFlop);
        };

        register_node(tmpl);
    }

    // Gate
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::Gate;
        tmpl.name = "Gate";
        tmpl.category = "Flow Control";
        tmpl.tooltip = "Gate that can be opened/closed";
        tmpl.node_category = NodeCategory::FlowControl;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "block allow pass";

        tmpl.create = [counter]() {
            return std::make_unique<GateNode>(NodeId::from_bits((*counter)++), builtin::Gate);
        };

        register_node(tmpl);
    }
}

void NodeRegistry::register_math_nodes() {
    static std::uint32_t next_id = 2000;
    auto* counter = &next_id;

    auto register_math_op = [&, counter](NodeTypeId id, const std::string& name,
                                 MathNode::Operation op, const std::string& keywords) {
        NodeTemplate tmpl;
        tmpl.id = id;
        tmpl.name = name;
        tmpl.category = "Math";
        tmpl.node_category = NodeCategory::Math;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.keywords = keywords;

        tmpl.create = [counter, op]() {
            return std::make_unique<MathNode>(NodeId::from_bits((*counter)++), builtin::MathAdd, op);
        };

        register_node(tmpl);
    };

    register_math_op(builtin::MathAdd, "Add", MathNode::Operation::Add, "+ plus");
    register_math_op(builtin::MathSubtract, "Subtract", MathNode::Operation::Subtract, "- minus");
    register_math_op(builtin::MathMultiply, "Multiply", MathNode::Operation::Multiply, "* times");
    register_math_op(builtin::MathDivide, "Divide", MathNode::Operation::Divide, "/ over");
    register_math_op(builtin::MathModulo, "Modulo", MathNode::Operation::Modulo, "% remainder");
    register_math_op(builtin::MathNegate, "Negate", MathNode::Operation::Negate, "negative -");
    register_math_op(builtin::MathAbs, "Abs", MathNode::Operation::Abs, "absolute");
    register_math_op(builtin::MathPower, "Power", MathNode::Operation::Power, "^ exponent pow");
    register_math_op(builtin::MathSqrt, "Sqrt", MathNode::Operation::Sqrt, "square root");

    register_math_op(builtin::MathSin, "Sin", MathNode::Operation::Sin, "sine trigonometry");
    register_math_op(builtin::MathCos, "Cos", MathNode::Operation::Cos, "cosine trigonometry");
    register_math_op(builtin::MathTan, "Tan", MathNode::Operation::Tan, "tangent trigonometry");
    register_math_op(builtin::MathAsin, "Asin", MathNode::Operation::Asin, "arcsine");
    register_math_op(builtin::MathAcos, "Acos", MathNode::Operation::Acos, "arccosine");
    register_math_op(builtin::MathAtan, "Atan", MathNode::Operation::Atan, "arctangent");
    register_math_op(builtin::MathAtan2, "Atan2", MathNode::Operation::Atan2, "arctangent2");

    register_math_op(builtin::MathMin, "Min", MathNode::Operation::Min, "minimum smaller");
    register_math_op(builtin::MathMax, "Max", MathNode::Operation::Max, "maximum larger");
    register_math_op(builtin::MathClamp, "Clamp", MathNode::Operation::Clamp, "limit range");
    register_math_op(builtin::MathLerp, "Lerp", MathNode::Operation::Lerp, "interpolate linear");
}

void NodeRegistry::register_conversion_nodes() {
    static std::uint32_t next_id = 3000;
    auto* counter = &next_id;

    auto register_conversion = [&, counter](NodeTypeId id, const std::string& name,
                                    PinType from, PinType to) {
        NodeTemplate tmpl;
        tmpl.id = id;
        tmpl.name = name;
        tmpl.category = "Conversion";
        tmpl.node_category = NodeCategory::Conversion;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;

        tmpl.create = [counter, from, to]() {
            return std::make_unique<ConversionNode>(
                NodeId::from_bits((*counter)++), builtin::ToFloat, from, to);
        };

        register_node(tmpl);
    };

    register_conversion(builtin::ToFloat, "To Float", PinType::Int, PinType::Float);
    register_conversion(builtin::ToInt, "To Int", PinType::Float, PinType::Int);
    register_conversion(builtin::ToString, "To String", PinType::Any, PinType::String);
    register_conversion(builtin::ToBool, "To Bool", PinType::Any, PinType::Bool);
}

void NodeRegistry::register_utility_nodes() {
    static std::uint32_t next_id = 4000;
    auto* counter = &next_id;

    // Comment
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::Comment;
        tmpl.name = "Comment";
        tmpl.category = "Utility";
        tmpl.tooltip = "Add a comment/note";
        tmpl.node_category = NodeCategory::Comment;

        tmpl.create = [counter]() {
            return std::make_unique<CommentNode>(NodeId::from_bits((*counter)++), builtin::Comment);
        };

        register_node(tmpl);
    }

    // Reroute
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::Reroute;
        tmpl.name = "Reroute";
        tmpl.category = "Utility";
        tmpl.tooltip = "Reroute wires for visual organization";
        tmpl.node_category = NodeCategory::Reroute;
        tmpl.is_compact = true;

        tmpl.create = [counter]() {
            return std::make_unique<RerouteNode>(NodeId::from_bits((*counter)++), builtin::Reroute);
        };

        register_node(tmpl);
    }
}

void NodeRegistry::register_string_nodes() {
    static std::uint32_t next_id = 6000;
    auto* counter = &next_id;

    // String Append
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::StringAppend;
        tmpl.name = "Append";
        tmpl.category = "String";
        tmpl.tooltip = "Concatenate two strings";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.keywords = "concat join + combine";

        Pin a;
        a.name = "A";
        a.direction = PinDirection::Input;
        a.type = PinType::String;
        tmpl.input_pins.push_back(std::move(a));

        Pin b;
        b.name = "B";
        b.direction = PinDirection::Input;
        b.type = PinType::String;
        tmpl.input_pins.push_back(std::move(b));

        Pin out;
        out.name = "Result";
        out.direction = PinDirection::Output;
        out.type = PinType::String;
        tmpl.output_pins.push_back(std::move(out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::StringAppend, "Append");
            node->set_pure(true);
            node->set_implementation([](ExecutionContext& ctx, const FunctionNode& n) -> PinId {
                const Pin* a_pin = n.find_pin_by_name("A");
                const Pin* b_pin = n.find_pin_by_name("B");
                const Pin* out_pin = n.find_pin_by_name("Result");

                std::string a_val, b_val;
                if (a_pin) {
                    auto it = ctx.pin_values.find(a_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::string>(it->second)) {
                        a_val = std::get<std::string>(it->second);
                    }
                }
                if (b_pin) {
                    auto it = ctx.pin_values.find(b_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::string>(it->second)) {
                        b_val = std::get<std::string>(it->second);
                    }
                }

                if (out_pin) {
                    ctx.pin_values[out_pin->id] = a_val + b_val;
                }
                return PinId{};
            });
            return node;
        };

        register_node(tmpl);
    }

    // String Length
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::StringLength;
        tmpl.name = "String Length";
        tmpl.category = "String";
        tmpl.tooltip = "Get the length of a string";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.keywords = "size count characters";

        Pin str;
        str.name = "String";
        str.direction = PinDirection::Input;
        str.type = PinType::String;
        tmpl.input_pins.push_back(std::move(str));

        Pin len;
        len.name = "Length";
        len.direction = PinDirection::Output;
        len.type = PinType::Int;
        tmpl.output_pins.push_back(std::move(len));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::StringLength, "String Length");
            node->set_pure(true);
            node->set_implementation([](ExecutionContext& ctx, const FunctionNode& n) -> PinId {
                const Pin* str_pin = n.find_pin_by_name("String");
                const Pin* len_pin = n.find_pin_by_name("Length");

                std::string str_val;
                if (str_pin) {
                    auto it = ctx.pin_values.find(str_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::string>(it->second)) {
                        str_val = std::get<std::string>(it->second);
                    }
                }

                if (len_pin) {
                    ctx.pin_values[len_pin->id] = static_cast<std::int32_t>(str_val.size());
                }
                return PinId{};
            });
            return node;
        };

        register_node(tmpl);
    }

    // String Contains
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::StringFind;
        tmpl.name = "Contains";
        tmpl.category = "String";
        tmpl.tooltip = "Check if a string contains a substring";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.keywords = "find search includes has";

        Pin str;
        str.name = "String";
        str.direction = PinDirection::Input;
        str.type = PinType::String;
        tmpl.input_pins.push_back(std::move(str));

        Pin sub;
        sub.name = "Substring";
        sub.direction = PinDirection::Input;
        sub.type = PinType::String;
        tmpl.input_pins.push_back(std::move(sub));

        Pin result;
        result.name = "Contains";
        result.direction = PinDirection::Output;
        result.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(result));

        Pin index;
        index.name = "Index";
        index.direction = PinDirection::Output;
        index.type = PinType::Int;
        tmpl.output_pins.push_back(std::move(index));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::StringFind, "Contains");
            node->set_pure(true);
            node->set_implementation([](ExecutionContext& ctx, const FunctionNode& n) -> PinId {
                const Pin* str_pin = n.find_pin_by_name("String");
                const Pin* sub_pin = n.find_pin_by_name("Substring");
                const Pin* contains_pin = n.find_pin_by_name("Contains");
                const Pin* index_pin = n.find_pin_by_name("Index");

                std::string str_val, sub_val;
                if (str_pin) {
                    auto it = ctx.pin_values.find(str_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::string>(it->second)) {
                        str_val = std::get<std::string>(it->second);
                    }
                }
                if (sub_pin) {
                    auto it = ctx.pin_values.find(sub_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::string>(it->second)) {
                        sub_val = std::get<std::string>(it->second);
                    }
                }

                auto pos = str_val.find(sub_val);
                bool found = pos != std::string::npos;

                if (contains_pin) {
                    ctx.pin_values[contains_pin->id] = found;
                }
                if (index_pin) {
                    ctx.pin_values[index_pin->id] = found ? static_cast<std::int32_t>(pos) : -1;
                }
                return PinId{};
            });
            return node;
        };

        register_node(tmpl);
    }

    // String Substring
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::StringSubstring;
        tmpl.name = "Substring";
        tmpl.category = "String";
        tmpl.tooltip = "Extract a portion of a string";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.keywords = "slice mid part extract";

        Pin str;
        str.name = "String";
        str.direction = PinDirection::Input;
        str.type = PinType::String;
        tmpl.input_pins.push_back(std::move(str));

        Pin start;
        start.name = "Start";
        start.direction = PinDirection::Input;
        start.type = PinType::Int;
        tmpl.input_pins.push_back(std::move(start));

        Pin length;
        length.name = "Length";
        length.direction = PinDirection::Input;
        length.type = PinType::Int;
        tmpl.input_pins.push_back(std::move(length));

        Pin result;
        result.name = "Result";
        result.direction = PinDirection::Output;
        result.type = PinType::String;
        tmpl.output_pins.push_back(std::move(result));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::StringSubstring, "Substring");
            node->set_pure(true);
            node->set_implementation([](ExecutionContext& ctx, const FunctionNode& n) -> PinId {
                const Pin* str_pin = n.find_pin_by_name("String");
                const Pin* start_pin = n.find_pin_by_name("Start");
                const Pin* len_pin = n.find_pin_by_name("Length");
                const Pin* result_pin = n.find_pin_by_name("Result");

                std::string str_val;
                std::int32_t start_val = 0, len_val = -1;

                if (str_pin) {
                    auto it = ctx.pin_values.find(str_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::string>(it->second)) {
                        str_val = std::get<std::string>(it->second);
                    }
                }
                if (start_pin) {
                    auto it = ctx.pin_values.find(start_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::int32_t>(it->second)) {
                        start_val = std::get<std::int32_t>(it->second);
                    }
                }
                if (len_pin) {
                    auto it = ctx.pin_values.find(len_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::int32_t>(it->second)) {
                        len_val = std::get<std::int32_t>(it->second);
                    }
                }

                std::string result;
                if (start_val >= 0 && static_cast<std::size_t>(start_val) < str_val.size()) {
                    if (len_val < 0) {
                        result = str_val.substr(static_cast<std::size_t>(start_val));
                    } else {
                        result = str_val.substr(static_cast<std::size_t>(start_val),
                                                static_cast<std::size_t>(len_val));
                    }
                }

                if (result_pin) {
                    ctx.pin_values[result_pin->id] = std::move(result);
                }
                return PinId{};
            });
            return node;
        };

        register_node(tmpl);
    }

    // String Replace
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::StringReplace;
        tmpl.name = "Replace";
        tmpl.category = "String";
        tmpl.tooltip = "Replace occurrences in a string";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.keywords = "substitute swap change";

        Pin str;
        str.name = "String";
        str.direction = PinDirection::Input;
        str.type = PinType::String;
        tmpl.input_pins.push_back(std::move(str));

        Pin from;
        from.name = "From";
        from.direction = PinDirection::Input;
        from.type = PinType::String;
        tmpl.input_pins.push_back(std::move(from));

        Pin to;
        to.name = "To";
        to.direction = PinDirection::Input;
        to.type = PinType::String;
        tmpl.input_pins.push_back(std::move(to));

        Pin result;
        result.name = "Result";
        result.direction = PinDirection::Output;
        result.type = PinType::String;
        tmpl.output_pins.push_back(std::move(result));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::StringReplace, "Replace");
            node->set_pure(true);
            node->set_implementation([](ExecutionContext& ctx, const FunctionNode& n) -> PinId {
                const Pin* str_pin = n.find_pin_by_name("String");
                const Pin* from_pin = n.find_pin_by_name("From");
                const Pin* to_pin = n.find_pin_by_name("To");
                const Pin* result_pin = n.find_pin_by_name("Result");

                std::string str_val, from_val, to_val;

                if (str_pin) {
                    auto it = ctx.pin_values.find(str_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::string>(it->second)) {
                        str_val = std::get<std::string>(it->second);
                    }
                }
                if (from_pin) {
                    auto it = ctx.pin_values.find(from_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::string>(it->second)) {
                        from_val = std::get<std::string>(it->second);
                    }
                }
                if (to_pin) {
                    auto it = ctx.pin_values.find(to_pin->id);
                    if (it != ctx.pin_values.end() && std::holds_alternative<std::string>(it->second)) {
                        to_val = std::get<std::string>(it->second);
                    }
                }

                // Replace all occurrences
                std::string result = str_val;
                if (!from_val.empty()) {
                    std::size_t pos = 0;
                    while ((pos = result.find(from_val, pos)) != std::string::npos) {
                        result.replace(pos, from_val.length(), to_val);
                        pos += to_val.length();
                    }
                }

                if (result_pin) {
                    ctx.pin_values[result_pin->id] = std::move(result);
                }
                return PinId{};
            });
            return node;
        };

        register_node(tmpl);
    }

    // String Split
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::StringSplit;
        tmpl.name = "Split";
        tmpl.category = "String";
        tmpl.tooltip = "Split a string by delimiter";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.keywords = "tokenize separate divide";

        Pin str;
        str.name = "String";
        str.direction = PinDirection::Input;
        str.type = PinType::String;
        tmpl.input_pins.push_back(std::move(str));

        Pin delim;
        delim.name = "Delimiter";
        delim.direction = PinDirection::Input;
        delim.type = PinType::String;
        tmpl.input_pins.push_back(std::move(delim));

        Pin result;
        result.name = "Result";
        result.direction = PinDirection::Output;
        result.type = PinType::Array;
        tmpl.output_pins.push_back(std::move(result));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::StringSplit, "Split");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // String Join
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::StringJoin;
        tmpl.name = "Join";
        tmpl.category = "String";
        tmpl.tooltip = "Join array elements with separator";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.keywords = "combine merge array";

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin sep;
        sep.name = "Separator";
        sep.direction = PinDirection::Input;
        sep.type = PinType::String;
        tmpl.input_pins.push_back(std::move(sep));

        Pin result;
        result.name = "Result";
        result.direction = PinDirection::Output;
        result.type = PinType::String;
        tmpl.output_pins.push_back(std::move(result));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::StringJoin, "Join");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // String Format
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::StringFormat;
        tmpl.name = "Format";
        tmpl.category = "String";
        tmpl.tooltip = "Format a string with placeholders";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.keywords = "template sprintf printf";

        Pin format;
        format.name = "Format";
        format.direction = PinDirection::Input;
        format.type = PinType::String;
        tmpl.input_pins.push_back(std::move(format));

        Pin args;
        args.name = "Args";
        args.direction = PinDirection::Input;
        args.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(args));

        Pin result;
        result.name = "Result";
        result.direction = PinDirection::Output;
        result.type = PinType::String;
        tmpl.output_pins.push_back(std::move(result));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::StringFormat, "Format");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }
}

void NodeRegistry::register_array_nodes() {
    static std::uint32_t next_id = 7000;
    auto* counter = &next_id;

    // Array Add
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ArrayAdd;
        tmpl.name = "Array Add";
        tmpl.category = "Array";
        tmpl.tooltip = "Add an element to an array";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "push append insert";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin element;
        element.name = "Element";
        element.direction = PinDirection::Input;
        element.type = PinType::Any;
        tmpl.input_pins.push_back(std::move(element));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        Pin new_size;
        new_size.name = "New Size";
        new_size.direction = PinDirection::Output;
        new_size.type = PinType::Int;
        tmpl.output_pins.push_back(std::move(new_size));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ArrayAdd, "Array Add");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Array Remove
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ArrayRemove;
        tmpl.name = "Array Remove";
        tmpl.category = "Array";
        tmpl.tooltip = "Remove an element from an array by index";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "delete pop erase";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin index;
        index.name = "Index";
        index.direction = PinDirection::Input;
        index.type = PinType::Int;
        tmpl.input_pins.push_back(std::move(index));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        Pin success;
        success.name = "Success";
        success.direction = PinDirection::Output;
        success.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(success));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ArrayRemove, "Array Remove");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Array Get
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ArrayGet;
        tmpl.name = "Array Get";
        tmpl.category = "Array";
        tmpl.tooltip = "Get an element from an array by index";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.keywords = "access index element at";

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin index;
        index.name = "Index";
        index.direction = PinDirection::Input;
        index.type = PinType::Int;
        tmpl.input_pins.push_back(std::move(index));

        Pin element;
        element.name = "Element";
        element.direction = PinDirection::Output;
        element.type = PinType::Any;
        tmpl.output_pins.push_back(std::move(element));

        Pin valid;
        valid.name = "Valid";
        valid.direction = PinDirection::Output;
        valid.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(valid));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ArrayGet, "Array Get");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Array Set
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ArraySet;
        tmpl.name = "Array Set";
        tmpl.category = "Array";
        tmpl.tooltip = "Set an element in an array by index";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "assign write update";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin index;
        index.name = "Index";
        index.direction = PinDirection::Input;
        index.type = PinType::Int;
        tmpl.input_pins.push_back(std::move(index));

        Pin value;
        value.name = "Value";
        value.direction = PinDirection::Input;
        value.type = PinType::Any;
        tmpl.input_pins.push_back(std::move(value));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        Pin success;
        success.name = "Success";
        success.direction = PinDirection::Output;
        success.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(success));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ArraySet, "Array Set");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Array Length
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ArrayLength;
        tmpl.name = "Array Length";
        tmpl.category = "Array";
        tmpl.tooltip = "Get the length of an array";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.keywords = "size count num";

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin len;
        len.name = "Length";
        len.direction = PinDirection::Output;
        len.type = PinType::Int;
        tmpl.output_pins.push_back(std::move(len));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ArrayLength, "Array Length");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Array Clear
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ArrayClear;
        tmpl.name = "Array Clear";
        tmpl.category = "Array";
        tmpl.tooltip = "Clear all elements from an array";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "empty reset remove all";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ArrayClear, "Array Clear");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Array Contains
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ArrayContains;
        tmpl.name = "Array Contains";
        tmpl.category = "Array";
        tmpl.tooltip = "Check if an array contains an element";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.keywords = "has includes find";

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin element;
        element.name = "Element";
        element.direction = PinDirection::Input;
        element.type = PinType::Any;
        tmpl.input_pins.push_back(std::move(element));

        Pin result;
        result.name = "Contains";
        result.direction = PinDirection::Output;
        result.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(result));

        Pin index;
        index.name = "Index";
        index.direction = PinDirection::Output;
        index.type = PinType::Int;
        tmpl.output_pins.push_back(std::move(index));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ArrayContains, "Array Contains");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Array Sort
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ArraySort;
        tmpl.name = "Array Sort";
        tmpl.category = "Array";
        tmpl.tooltip = "Sort an array";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "order arrange";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin ascending;
        ascending.name = "Ascending";
        ascending.direction = PinDirection::Input;
        ascending.type = PinType::Bool;
        ascending.default_value.value = true;
        tmpl.input_pins.push_back(std::move(ascending));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ArraySort, "Array Sort");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Array Reverse
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ArrayReverse;
        tmpl.name = "Array Reverse";
        tmpl.category = "Array";
        tmpl.tooltip = "Reverse the order of elements in an array";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "flip invert";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin arr;
        arr.name = "Array";
        arr.direction = PinDirection::Input;
        arr.type = PinType::Array;
        tmpl.input_pins.push_back(std::move(arr));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ArrayReverse, "Array Reverse");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }
}

void NodeRegistry::register_debug_nodes() {
    static std::uint32_t next_id = 5000;
    auto* counter = &next_id;

    // Print String
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::PrintString;
        tmpl.name = "Print String";
        tmpl.category = "Debug";
        tmpl.tooltip = "Print a string to the log";
        tmpl.node_category = NodeCategory::Utility;
        tmpl.purity = NodePurity::Impure;
        tmpl.keywords = "log output debug";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin text;
        text.name = "Text";
        text.direction = PinDirection::Input;
        text.type = PinType::String;
        tmpl.input_pins.push_back(std::move(text));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::PrintString, "Print String");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }
}

NodeTypeId NodeRegistry::type_id_from_name(const std::string& name) {
    // FNV-1a hash
    std::uint32_t hash = 2166136261u;
    for (char c : name) {
        hash ^= static_cast<std::uint32_t>(c);
        hash *= 16777619u;
    }
    return NodeTypeId::from_bits(hash);
}

// =============================================================================
// GraphLibrary Implementation
// =============================================================================

GraphLibrary::GraphLibrary() = default;

GraphLibrary& GraphLibrary::instance() {
    static GraphLibrary library;
    return library;
}

GraphId GraphLibrary::add_graph(std::unique_ptr<Graph> graph) {
    GraphId id = graph->id().is_valid() ? graph->id() : GraphId::from_bits(next_graph_id_++);
    graph_names_[graph->name()] = id;
    graphs_[id] = std::move(graph);
    return id;
}

bool GraphLibrary::remove_graph(GraphId id) {
    auto it = graphs_.find(id);
    if (it == graphs_.end()) return false;

    graph_names_.erase(it->second->name());
    graphs_.erase(it);
    return true;
}

Graph* GraphLibrary::get_graph(GraphId id) {
    auto it = graphs_.find(id);
    if (it == graphs_.end()) return nullptr;
    return it->second.get();
}

const Graph* GraphLibrary::get_graph(GraphId id) const {
    auto it = graphs_.find(id);
    if (it == graphs_.end()) return nullptr;
    return it->second.get();
}

Graph* GraphLibrary::find_graph(const std::string& name) {
    auto it = graph_names_.find(name);
    if (it == graph_names_.end()) return nullptr;
    return get_graph(it->second);
}

std::vector<Graph*> GraphLibrary::all_graphs() {
    std::vector<Graph*> result;
    result.reserve(graphs_.size());
    for (auto& [id, graph] : graphs_) {
        result.push_back(graph.get());
    }
    return result;
}

std::vector<const Graph*> GraphLibrary::all_graphs() const {
    std::vector<const Graph*> result;
    result.reserve(graphs_.size());
    for (const auto& [id, graph] : graphs_) {
        result.push_back(graph.get());
    }
    return result;
}

SubgraphId GraphLibrary::add_subgraph(std::unique_ptr<Subgraph> subgraph) {
    SubgraphId id = subgraph->subgraph_id().is_valid() ? subgraph->subgraph_id()
                                                        : SubgraphId::from_bits(next_subgraph_id_++);
    subgraph_names_[subgraph->name()] = id;
    subgraphs_[id] = std::move(subgraph);
    return id;
}

Subgraph* GraphLibrary::get_subgraph(SubgraphId id) {
    auto it = subgraphs_.find(id);
    if (it == subgraphs_.end()) return nullptr;
    return it->second.get();
}

const Subgraph* GraphLibrary::get_subgraph(SubgraphId id) const {
    auto it = subgraphs_.find(id);
    if (it == subgraphs_.end()) return nullptr;
    return it->second.get();
}

Subgraph* GraphLibrary::find_subgraph(const std::string& name) {
    auto it = subgraph_names_.find(name);
    if (it == subgraph_names_.end()) return nullptr;
    return get_subgraph(it->second);
}

std::vector<Subgraph*> GraphLibrary::all_subgraphs() {
    std::vector<Subgraph*> result;
    result.reserve(subgraphs_.size());
    for (auto& [id, subgraph] : subgraphs_) {
        result.push_back(subgraph.get());
    }
    return result;
}

GraphResult<Graph*> GraphLibrary::load_graph(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return GraphResult<Graph*>(GraphError::InvalidGraph);
    }

    auto graph = Graph::deserialize(file, NodeRegistry::instance());
    if (!graph) {
        return GraphResult<Graph*>(GraphError::SerializationError);
    }

    Graph* ptr = graph.get();
    add_graph(std::move(graph));
    return GraphResult<Graph*>(ptr);
}

GraphResult<void> GraphLibrary::save_graph(GraphId id, const std::filesystem::path& path) {
    const Graph* graph = get_graph(id);
    if (!graph) {
        return GraphResult<void>(GraphError::InvalidGraph);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return GraphResult<void>(GraphError::SerializationError);
    }

    graph->serialize(file);
    return GraphResult<void>::ok();
}

void GraphLibrary::load_directory(const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory)) return;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".vgraph") {
            (void)load_graph(entry.path());
        }
    }
}

std::string GraphLibrary::export_cpp(GraphId id) const {
    const Graph* graph = get_graph(id);
    if (!graph) return "";

    std::ostringstream ss;
    ss << "// Auto-generated from graph: " << graph->name() << "\n";
    ss << "#include <void_graph/graph.hpp>\n\n";
    ss << "void execute_" << graph->name() << "() {\n";
    ss << "    // TODO: Generate C++ code\n";
    ss << "}\n";

    return ss.str();
}

GraphResult<Graph*> GraphLibrary::import_json(const std::string& json) {
    auto graph = Graph::from_json(json, NodeRegistry::instance());
    if (!graph) {
        return GraphResult<Graph*>(GraphError::SerializationError);
    }

    Graph* ptr = graph.get();
    add_graph(std::move(graph));
    return GraphResult<Graph*>(ptr);
}

std::vector<Graph*> GraphLibrary::graphs_by_category(const std::string& category) {
    std::vector<Graph*> result;
    for (auto& [id, graph] : graphs_) {
        // Check metadata for category
        auto it = graph->metadata().custom_data.find("category");
        if (it != graph->metadata().custom_data.end() && it->second == category) {
            result.push_back(graph.get());
        }
    }
    return result;
}

std::vector<Graph*> GraphLibrary::search(const std::string& query) {
    std::vector<Graph*> result;

    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    for (auto& [id, graph] : graphs_) {
        std::string lower_name = graph->name();
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

        if (lower_name.find(lower_query) != std::string::npos) {
            result.push_back(graph.get());
        }
    }

    return result;
}

std::vector<GraphId> GraphLibrary::get_dependents(SubgraphId subgraph_id) const {
    std::vector<GraphId> result;
    for (const auto& [id, graph] : graphs_) {
        for (const auto& node : graph->nodes()) {
            if (node->category() == NodeCategory::Subgraph) {
                auto* subgraph_node = static_cast<const SubgraphNode*>(node.get());
                if (subgraph_node->subgraph_id() == subgraph_id) {
                    result.push_back(id);
                    break;
                }
            }
        }
    }
    return result;
}

bool GraphLibrary::validate_dependencies() const {
    for (const auto& [id, graph] : graphs_) {
        for (const auto& node : graph->nodes()) {
            if (node->category() == NodeCategory::Subgraph) {
                auto* subgraph_node = static_cast<const SubgraphNode*>(node.get());
                if (!get_subgraph(subgraph_node->subgraph_id())) {
                    return false;
                }
            }
        }
    }
    return true;
}

// =============================================================================
// Entity Nodes
// =============================================================================

void NodeRegistry::register_entity_nodes() {
    static std::uint32_t next_id = 10000;
    auto* counter = &next_id;

    // Spawn Entity
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::SpawnEntity;
        tmpl.name = "Spawn Entity";
        tmpl.category = "Entity";
        tmpl.tooltip = "Spawn a new entity in the world";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF2266AA;
        tmpl.keywords = "create instantiate new actor";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin type;
        type.name = "Type";
        type.direction = PinDirection::Input;
        type.type = PinType::String;
        tmpl.input_pins.push_back(std::move(type));

        Pin location;
        location.name = "Location";
        location.direction = PinDirection::Input;
        location.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(location));

        Pin rotation;
        rotation.name = "Rotation";
        rotation.direction = PinDirection::Input;
        rotation.type = PinType::Quat;
        tmpl.input_pins.push_back(std::move(rotation));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Output;
        entity.type = PinType::Entity;
        tmpl.output_pins.push_back(std::move(entity));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::SpawnEntity, "Spawn Entity");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Destroy Entity
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::DestroyEntity;
        tmpl.name = "Destroy Entity";
        tmpl.category = "Entity";
        tmpl.tooltip = "Destroy an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF2266AA;
        tmpl.keywords = "delete remove kill despawn";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::DestroyEntity, "Destroy Entity");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Get Entity Location
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetEntityLocation;
        tmpl.name = "Get Location";
        tmpl.category = "Entity";
        tmpl.tooltip = "Get the world location of an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFF2266AA;
        tmpl.keywords = "position transform world";

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin location;
        location.name = "Location";
        location.direction = PinDirection::Output;
        location.type = PinType::Vec3;
        tmpl.output_pins.push_back(std::move(location));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetEntityLocation, "Get Location");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Set Entity Location
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::SetEntityLocation;
        tmpl.name = "Set Location";
        tmpl.category = "Entity";
        tmpl.tooltip = "Set the world location of an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF2266AA;
        tmpl.keywords = "position transform move teleport";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin location;
        location.name = "Location";
        location.direction = PinDirection::Input;
        location.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(location));

        Pin sweep;
        sweep.name = "Sweep";
        sweep.direction = PinDirection::Input;
        sweep.type = PinType::Bool;
        sweep.default_value.value = false;
        tmpl.input_pins.push_back(std::move(sweep));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::SetEntityLocation, "Set Location");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Get Entity Rotation
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetEntityRotation;
        tmpl.name = "Get Rotation";
        tmpl.category = "Entity";
        tmpl.tooltip = "Get the world rotation of an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFF2266AA;
        tmpl.keywords = "orientation transform angle";

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin rotation;
        rotation.name = "Rotation";
        rotation.direction = PinDirection::Output;
        rotation.type = PinType::Quat;
        tmpl.output_pins.push_back(std::move(rotation));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetEntityRotation, "Get Rotation");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Set Entity Rotation
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::SetEntityRotation;
        tmpl.name = "Set Rotation";
        tmpl.category = "Entity";
        tmpl.tooltip = "Set the world rotation of an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF2266AA;
        tmpl.keywords = "orientation transform angle rotate";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin rotation;
        rotation.name = "Rotation";
        rotation.direction = PinDirection::Input;
        rotation.type = PinType::Quat;
        tmpl.input_pins.push_back(std::move(rotation));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::SetEntityRotation, "Set Rotation");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Get Component
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetComponent;
        tmpl.name = "Get Component";
        tmpl.category = "Entity";
        tmpl.tooltip = "Get a component from an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.title_color = 0xFF2266AA;
        tmpl.keywords = "find query";

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin comp_type;
        comp_type.name = "Component Type";
        comp_type.direction = PinDirection::Input;
        comp_type.type = PinType::String;
        tmpl.input_pins.push_back(std::move(comp_type));

        Pin component;
        component.name = "Component";
        component.direction = PinDirection::Output;
        component.type = PinType::Component;
        tmpl.output_pins.push_back(std::move(component));

        Pin valid;
        valid.name = "Valid";
        valid.direction = PinDirection::Output;
        valid.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(valid));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetComponent, "Get Component");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Has Component
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::HasComponent;
        tmpl.name = "Has Component";
        tmpl.category = "Entity";
        tmpl.tooltip = "Check if an entity has a component";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFF2266AA;
        tmpl.keywords = "check query";

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin comp_type;
        comp_type.name = "Component Type";
        comp_type.direction = PinDirection::Input;
        comp_type.type = PinType::String;
        tmpl.input_pins.push_back(std::move(comp_type));

        Pin result;
        result.name = "Has";
        result.direction = PinDirection::Output;
        result.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(result));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::HasComponent, "Has Component");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }
}

// =============================================================================
// Physics Nodes
// =============================================================================

void NodeRegistry::register_physics_nodes() {
    static std::uint32_t next_id = 11000;
    auto* counter = &next_id;

    // Add Force
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::AddForce;
        tmpl.name = "Add Force";
        tmpl.category = "Physics";
        tmpl.tooltip = "Add a force to a rigidbody";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF22AA66;
        tmpl.keywords = "push accelerate physics";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin force;
        force.name = "Force";
        force.direction = PinDirection::Input;
        force.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(force));

        Pin world_space;
        world_space.name = "World Space";
        world_space.direction = PinDirection::Input;
        world_space.type = PinType::Bool;
        world_space.default_value.value = true;
        tmpl.input_pins.push_back(std::move(world_space));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::AddForce, "Add Force");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Add Impulse
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::AddImpulse;
        tmpl.name = "Add Impulse";
        tmpl.category = "Physics";
        tmpl.tooltip = "Add an instant impulse to a rigidbody";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF22AA66;
        tmpl.keywords = "push instant physics";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin impulse;
        impulse.name = "Impulse";
        impulse.direction = PinDirection::Input;
        impulse.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(impulse));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::AddImpulse, "Add Impulse");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Set Velocity
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::SetVelocity;
        tmpl.name = "Set Velocity";
        tmpl.category = "Physics";
        tmpl.tooltip = "Set the linear velocity of a rigidbody";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF22AA66;
        tmpl.keywords = "speed physics";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin velocity;
        velocity.name = "Velocity";
        velocity.direction = PinDirection::Input;
        velocity.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(velocity));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::SetVelocity, "Set Velocity");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Get Velocity
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetVelocity;
        tmpl.name = "Get Velocity";
        tmpl.category = "Physics";
        tmpl.tooltip = "Get the linear velocity of a rigidbody";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFF22AA66;
        tmpl.keywords = "speed physics";

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin velocity;
        velocity.name = "Velocity";
        velocity.direction = PinDirection::Output;
        velocity.type = PinType::Vec3;
        tmpl.output_pins.push_back(std::move(velocity));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetVelocity, "Get Velocity");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Raycast
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::Raycast;
        tmpl.name = "Raycast";
        tmpl.category = "Physics";
        tmpl.tooltip = "Cast a ray and get the first hit";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.title_color = 0xFF22AA66;
        tmpl.keywords = "trace line cast collision";

        Pin origin;
        origin.name = "Origin";
        origin.direction = PinDirection::Input;
        origin.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(origin));

        Pin direction;
        direction.name = "Direction";
        direction.direction = PinDirection::Input;
        direction.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(direction));

        Pin max_dist;
        max_dist.name = "Max Distance";
        max_dist.direction = PinDirection::Input;
        max_dist.type = PinType::Float;
        max_dist.default_value.value = 1000.0f;
        tmpl.input_pins.push_back(std::move(max_dist));

        Pin hit;
        hit.name = "Hit";
        hit.direction = PinDirection::Output;
        hit.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(hit));

        Pin hit_location;
        hit_location.name = "Hit Location";
        hit_location.direction = PinDirection::Output;
        hit_location.type = PinType::Vec3;
        tmpl.output_pins.push_back(std::move(hit_location));

        Pin hit_normal;
        hit_normal.name = "Hit Normal";
        hit_normal.direction = PinDirection::Output;
        hit_normal.type = PinType::Vec3;
        tmpl.output_pins.push_back(std::move(hit_normal));

        Pin hit_entity;
        hit_entity.name = "Hit Entity";
        hit_entity.direction = PinDirection::Output;
        hit_entity.type = PinType::Entity;
        tmpl.output_pins.push_back(std::move(hit_entity));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::Raycast, "Raycast");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Overlap Sphere
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::OverlapSphere;
        tmpl.name = "Overlap Sphere";
        tmpl.category = "Physics";
        tmpl.tooltip = "Get all entities overlapping a sphere";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.title_color = 0xFF22AA66;
        tmpl.keywords = "collision detect area";

        Pin center;
        center.name = "Center";
        center.direction = PinDirection::Input;
        center.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(center));

        Pin radius;
        radius.name = "Radius";
        radius.direction = PinDirection::Input;
        radius.type = PinType::Float;
        tmpl.input_pins.push_back(std::move(radius));

        Pin results;
        results.name = "Entities";
        results.direction = PinDirection::Output;
        results.type = PinType::Array;
        tmpl.output_pins.push_back(std::move(results));

        Pin count;
        count.name = "Count";
        count.direction = PinDirection::Output;
        count.type = PinType::Int;
        tmpl.output_pins.push_back(std::move(count));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::OverlapSphere, "Overlap Sphere");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }
}

// =============================================================================
// Audio Nodes
// =============================================================================

void NodeRegistry::register_audio_nodes() {
    static std::uint32_t next_id = 12000;
    auto* counter = &next_id;

    // Play Sound
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::PlaySound;
        tmpl.name = "Play Sound";
        tmpl.category = "Audio";
        tmpl.tooltip = "Play a sound effect";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFFAA6622;
        tmpl.keywords = "audio sfx effect";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin sound;
        sound.name = "Sound";
        sound.direction = PinDirection::Input;
        sound.type = PinType::Asset;
        tmpl.input_pins.push_back(std::move(sound));

        Pin volume;
        volume.name = "Volume";
        volume.direction = PinDirection::Input;
        volume.type = PinType::Float;
        volume.default_value.value = 1.0f;
        tmpl.input_pins.push_back(std::move(volume));

        Pin pitch;
        pitch.name = "Pitch";
        pitch.direction = PinDirection::Input;
        pitch.type = PinType::Float;
        pitch.default_value.value = 1.0f;
        tmpl.input_pins.push_back(std::move(pitch));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::PlaySound, "Play Sound");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Play Sound At Location
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::PlaySoundAtLocation;
        tmpl.name = "Play Sound At Location";
        tmpl.category = "Audio";
        tmpl.tooltip = "Play a 3D sound at a location";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFFAA6622;
        tmpl.keywords = "audio sfx 3d spatial";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin sound;
        sound.name = "Sound";
        sound.direction = PinDirection::Input;
        sound.type = PinType::Asset;
        tmpl.input_pins.push_back(std::move(sound));

        Pin location;
        location.name = "Location";
        location.direction = PinDirection::Input;
        location.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(location));

        Pin volume;
        volume.name = "Volume";
        volume.direction = PinDirection::Input;
        volume.type = PinType::Float;
        volume.default_value.value = 1.0f;
        tmpl.input_pins.push_back(std::move(volume));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::PlaySoundAtLocation, "Play Sound At Location");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Play Music
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::PlayMusic;
        tmpl.name = "Play Music";
        tmpl.category = "Audio";
        tmpl.tooltip = "Play background music";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFFAA6622;
        tmpl.keywords = "audio bgm track";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin music;
        music.name = "Music";
        music.direction = PinDirection::Input;
        music.type = PinType::Asset;
        tmpl.input_pins.push_back(std::move(music));

        Pin loop;
        loop.name = "Loop";
        loop.direction = PinDirection::Input;
        loop.type = PinType::Bool;
        loop.default_value.value = true;
        tmpl.input_pins.push_back(std::move(loop));

        Pin fade_in;
        fade_in.name = "Fade In";
        fade_in.direction = PinDirection::Input;
        fade_in.type = PinType::Float;
        fade_in.default_value.value = 0.0f;
        tmpl.input_pins.push_back(std::move(fade_in));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::PlayMusic, "Play Music");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Stop Music
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::StopMusic;
        tmpl.name = "Stop Music";
        tmpl.category = "Audio";
        tmpl.tooltip = "Stop background music";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFFAA6622;
        tmpl.keywords = "audio bgm";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin fade_out;
        fade_out.name = "Fade Out";
        fade_out.direction = PinDirection::Input;
        fade_out.type = PinType::Float;
        fade_out.default_value.value = 0.0f;
        tmpl.input_pins.push_back(std::move(fade_out));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::StopMusic, "Stop Music");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }
}

// =============================================================================
// Combat Nodes
// =============================================================================

void NodeRegistry::register_combat_nodes() {
    static std::uint32_t next_id = 13000;
    auto* counter = &next_id;

    // Apply Damage
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ApplyDamage;
        tmpl.name = "Apply Damage";
        tmpl.category = "Combat";
        tmpl.tooltip = "Apply damage to an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFFAA2222;
        tmpl.keywords = "hurt health attack";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin target;
        target.name = "Target";
        target.direction = PinDirection::Input;
        target.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(target));

        Pin amount;
        amount.name = "Amount";
        amount.direction = PinDirection::Input;
        amount.type = PinType::Float;
        tmpl.input_pins.push_back(std::move(amount));

        Pin source;
        source.name = "Source";
        source.direction = PinDirection::Input;
        source.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(source));

        Pin damage_type;
        damage_type.name = "Damage Type";
        damage_type.direction = PinDirection::Input;
        damage_type.type = PinType::String;
        damage_type.default_value.value = std::string("Default");
        tmpl.input_pins.push_back(std::move(damage_type));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        Pin actual_damage;
        actual_damage.name = "Actual Damage";
        actual_damage.direction = PinDirection::Output;
        actual_damage.type = PinType::Float;
        tmpl.output_pins.push_back(std::move(actual_damage));

        Pin killed;
        killed.name = "Killed";
        killed.direction = PinDirection::Output;
        killed.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(killed));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ApplyDamage, "Apply Damage");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Get Health
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetHealth;
        tmpl.name = "Get Health";
        tmpl.category = "Combat";
        tmpl.tooltip = "Get the current health of an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFFAA2222;
        tmpl.keywords = "hp life";

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin health;
        health.name = "Health";
        health.direction = PinDirection::Output;
        health.type = PinType::Float;
        tmpl.output_pins.push_back(std::move(health));

        Pin max_health;
        max_health.name = "Max Health";
        max_health.direction = PinDirection::Output;
        max_health.type = PinType::Float;
        tmpl.output_pins.push_back(std::move(max_health));

        Pin percent;
        percent.name = "Percent";
        percent.direction = PinDirection::Output;
        percent.type = PinType::Float;
        tmpl.output_pins.push_back(std::move(percent));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetHealth, "Get Health");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Set Health
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::SetHealth;
        tmpl.name = "Set Health";
        tmpl.category = "Combat";
        tmpl.tooltip = "Set the health of an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFFAA2222;
        tmpl.keywords = "hp life";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin health;
        health.name = "Health";
        health.direction = PinDirection::Input;
        health.type = PinType::Float;
        tmpl.input_pins.push_back(std::move(health));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::SetHealth, "Set Health");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Heal
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::Heal;
        tmpl.name = "Heal";
        tmpl.category = "Combat";
        tmpl.tooltip = "Heal an entity";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF22AA22;
        tmpl.keywords = "restore hp life";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin amount;
        amount.name = "Amount";
        amount.direction = PinDirection::Input;
        amount.type = PinType::Float;
        tmpl.input_pins.push_back(std::move(amount));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        Pin actual_heal;
        actual_heal.name = "Actual Heal";
        actual_heal.direction = PinDirection::Output;
        actual_heal.type = PinType::Float;
        tmpl.output_pins.push_back(std::move(actual_heal));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::Heal, "Heal");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }
}

// =============================================================================
// Inventory Nodes
// =============================================================================

void NodeRegistry::register_inventory_nodes() {
    static std::uint32_t next_id = 14000;
    auto* counter = &next_id;

    // Add Item
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::AddItem;
        tmpl.name = "Add Item";
        tmpl.category = "Inventory";
        tmpl.tooltip = "Add an item to an inventory";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF66AA22;
        tmpl.keywords = "give pickup collect";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin item_id;
        item_id.name = "Item ID";
        item_id.direction = PinDirection::Input;
        item_id.type = PinType::String;
        tmpl.input_pins.push_back(std::move(item_id));

        Pin count;
        count.name = "Count";
        count.direction = PinDirection::Input;
        count.type = PinType::Int;
        count.default_value.value = std::int32_t(1);
        tmpl.input_pins.push_back(std::move(count));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        Pin success;
        success.name = "Success";
        success.direction = PinDirection::Output;
        success.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(success));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::AddItem, "Add Item");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Remove Item
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::RemoveItem;
        tmpl.name = "Remove Item";
        tmpl.category = "Inventory";
        tmpl.tooltip = "Remove an item from an inventory";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF66AA22;
        tmpl.keywords = "drop consume use";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin item_id;
        item_id.name = "Item ID";
        item_id.direction = PinDirection::Input;
        item_id.type = PinType::String;
        tmpl.input_pins.push_back(std::move(item_id));

        Pin count;
        count.name = "Count";
        count.direction = PinDirection::Input;
        count.type = PinType::Int;
        count.default_value.value = std::int32_t(1);
        tmpl.input_pins.push_back(std::move(count));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        Pin success;
        success.name = "Success";
        success.direction = PinDirection::Output;
        success.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(success));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::RemoveItem, "Remove Item");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Has Item
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::HasItem;
        tmpl.name = "Has Item";
        tmpl.category = "Inventory";
        tmpl.tooltip = "Check if an inventory has an item";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFF66AA22;
        tmpl.keywords = "check contains";

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin item_id;
        item_id.name = "Item ID";
        item_id.direction = PinDirection::Input;
        item_id.type = PinType::String;
        tmpl.input_pins.push_back(std::move(item_id));

        Pin has;
        has.name = "Has";
        has.direction = PinDirection::Output;
        has.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(has));

        Pin count;
        count.name = "Count";
        count.direction = PinDirection::Output;
        count.type = PinType::Int;
        tmpl.output_pins.push_back(std::move(count));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::HasItem, "Has Item");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }
}

// =============================================================================
// AI Nodes
// =============================================================================

void NodeRegistry::register_ai_nodes() {
    static std::uint32_t next_id = 15000;
    auto* counter = &next_id;

    // Move To
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::MoveTo;
        tmpl.name = "Move To";
        tmpl.category = "AI";
        tmpl.tooltip = "Move an AI to a location";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Latent;
        tmpl.title_color = 0xFF6622AA;
        tmpl.keywords = "pathfind navigate walk";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin destination;
        destination.name = "Destination";
        destination.direction = PinDirection::Input;
        destination.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(destination));

        Pin acceptance;
        acceptance.name = "Acceptance Radius";
        acceptance.direction = PinDirection::Input;
        acceptance.type = PinType::Float;
        acceptance.default_value.value = 1.0f;
        tmpl.input_pins.push_back(std::move(acceptance));

        Pin on_success;
        on_success.name = "On Success";
        on_success.direction = PinDirection::Output;
        on_success.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(on_success));

        Pin on_fail;
        on_fail.name = "On Fail";
        on_fail.direction = PinDirection::Output;
        on_fail.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(on_fail));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::MoveTo, "Move To");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Set AI State
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::SetAIState;
        tmpl.name = "Set AI State";
        tmpl.category = "AI";
        tmpl.tooltip = "Set the AI behavior state";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF6622AA;
        tmpl.keywords = "behavior mode";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin state;
        state.name = "State";
        state.direction = PinDirection::Input;
        state.type = PinType::String;
        tmpl.input_pins.push_back(std::move(state));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::SetAIState, "Set AI State");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Get AI State
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetAIState;
        tmpl.name = "Get AI State";
        tmpl.category = "AI";
        tmpl.tooltip = "Get the current AI state";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFF6622AA;
        tmpl.keywords = "behavior mode";

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin state;
        state.name = "State";
        state.direction = PinDirection::Output;
        state.type = PinType::String;
        tmpl.output_pins.push_back(std::move(state));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetAIState, "Get AI State");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Look At
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::LookAt;
        tmpl.name = "Look At";
        tmpl.category = "AI";
        tmpl.tooltip = "Make an entity look at a target";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF6622AA;
        tmpl.keywords = "face rotate turn";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin entity;
        entity.name = "Entity";
        entity.direction = PinDirection::Input;
        entity.type = PinType::Entity;
        tmpl.input_pins.push_back(std::move(entity));

        Pin target;
        target.name = "Target";
        target.direction = PinDirection::Input;
        target.type = PinType::Vec3;
        tmpl.input_pins.push_back(std::move(target));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::LookAt, "Look At");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }
}

// =============================================================================
// Input Nodes
// =============================================================================

void NodeRegistry::register_input_nodes() {
    static std::uint32_t next_id = 16000;
    auto* counter = &next_id;

    // Is Key Pressed
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::IsKeyPressed;
        tmpl.name = "Is Key Pressed";
        tmpl.category = "Input";
        tmpl.tooltip = "Check if a key is currently pressed";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFFAA22AA;
        tmpl.keywords = "keyboard held down";

        Pin key;
        key.name = "Key";
        key.direction = PinDirection::Input;
        key.type = PinType::String;
        tmpl.input_pins.push_back(std::move(key));

        Pin pressed;
        pressed.name = "Pressed";
        pressed.direction = PinDirection::Output;
        pressed.type = PinType::Bool;
        tmpl.output_pins.push_back(std::move(pressed));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::IsKeyPressed, "Is Key Pressed");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Get Mouse Position
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetMousePosition;
        tmpl.name = "Get Mouse Position";
        tmpl.category = "Input";
        tmpl.tooltip = "Get the current mouse position";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFFAA22AA;
        tmpl.keywords = "cursor screen";

        Pin position;
        position.name = "Position";
        position.direction = PinDirection::Output;
        position.type = PinType::Vec2;
        tmpl.output_pins.push_back(std::move(position));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetMousePosition, "Get Mouse Position");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }

    // Get Axis Value
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetAxisValue;
        tmpl.name = "Get Axis Value";
        tmpl.category = "Input";
        tmpl.tooltip = "Get the value of an input axis";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFFAA22AA;
        tmpl.keywords = "gamepad joystick analog";

        Pin axis;
        axis.name = "Axis";
        axis.direction = PinDirection::Input;
        axis.type = PinType::String;
        tmpl.input_pins.push_back(std::move(axis));

        Pin value;
        value.name = "Value";
        value.direction = PinDirection::Output;
        value.type = PinType::Float;
        tmpl.output_pins.push_back(std::move(value));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetAxisValue, "Get Axis Value");
            node->set_pure(true);
            return node;
        };

        register_node(tmpl);
    }
}

// =============================================================================
// UI Nodes
// =============================================================================

void NodeRegistry::register_ui_nodes() {
    static std::uint32_t next_id = 17000;
    auto* counter = &next_id;

    // Show Notification
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::ShowNotification;
        tmpl.name = "Show Notification";
        tmpl.category = "UI";
        tmpl.tooltip = "Show a notification message";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF22AAAA;
        tmpl.keywords = "message toast popup";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin text;
        text.name = "Text";
        text.direction = PinDirection::Input;
        text.type = PinType::String;
        tmpl.input_pins.push_back(std::move(text));

        Pin duration;
        duration.name = "Duration";
        duration.direction = PinDirection::Input;
        duration.type = PinType::Float;
        duration.default_value.value = 3.0f;
        tmpl.input_pins.push_back(std::move(duration));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::ShowNotification, "Show Notification");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Set Widget Text
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::SetWidgetText;
        tmpl.name = "Set Widget Text";
        tmpl.category = "UI";
        tmpl.tooltip = "Set the text of a UI widget";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF22AAAA;
        tmpl.keywords = "label update";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin widget;
        widget.name = "Widget";
        widget.direction = PinDirection::Input;
        widget.type = PinType::String;
        tmpl.input_pins.push_back(std::move(widget));

        Pin text;
        text.name = "Text";
        text.direction = PinDirection::Input;
        text.type = PinType::String;
        tmpl.input_pins.push_back(std::move(text));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::SetWidgetText, "Set Widget Text");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }

    // Set Progress Bar
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::SetProgressBar;
        tmpl.name = "Set Progress Bar";
        tmpl.category = "UI";
        tmpl.tooltip = "Set the progress of a progress bar";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFF22AAAA;
        tmpl.keywords = "health bar percent";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin widget;
        widget.name = "Widget";
        widget.direction = PinDirection::Input;
        widget.type = PinType::String;
        tmpl.input_pins.push_back(std::move(widget));

        Pin progress;
        progress.name = "Progress";
        progress.direction = PinDirection::Input;
        progress.type = PinType::Float;
        tmpl.input_pins.push_back(std::move(progress));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::SetProgressBar, "Set Progress Bar");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }
}

// =============================================================================
// Time Nodes
// =============================================================================

void NodeRegistry::register_time_nodes() {
    static std::uint32_t next_id = 18000;
    auto* counter = &next_id;

    // Get Delta Time
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetDeltaTime;
        tmpl.name = "Get Delta Time";
        tmpl.category = "Time";
        tmpl.tooltip = "Get the time since last frame";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFFAAAA22;
        tmpl.keywords = "frame dt";

        Pin delta;
        delta.name = "Delta Time";
        delta.direction = PinDirection::Output;
        delta.type = PinType::Float;
        tmpl.output_pins.push_back(std::move(delta));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetDeltaTime, "Get Delta Time");
            node->set_pure(true);
            node->set_implementation([](ExecutionContext& ctx, const FunctionNode& n) -> PinId {
                const Pin* out_pin = n.find_pin_by_name("Delta Time");
                if (out_pin) {
                    ctx.pin_values[out_pin->id] = ctx.delta_time;
                }
                return PinId{};
            });
            return node;
        };

        register_node(tmpl);
    }

    // Get Time Seconds
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::GetTimeSeconds;
        tmpl.name = "Get Time Seconds";
        tmpl.category = "Time";
        tmpl.tooltip = "Get the total time since start";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Pure;
        tmpl.is_compact = true;
        tmpl.title_color = 0xFFAAAA22;
        tmpl.keywords = "elapsed total";

        Pin time;
        time.name = "Time";
        time.direction = PinDirection::Output;
        time.type = PinType::Float;
        tmpl.output_pins.push_back(std::move(time));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::GetTimeSeconds, "Get Time Seconds");
            node->set_pure(true);
            node->set_implementation([](ExecutionContext& ctx, const FunctionNode& n) -> PinId {
                const Pin* out_pin = n.find_pin_by_name("Time");
                if (out_pin) {
                    ctx.pin_values[out_pin->id] = ctx.total_time;
                }
                return PinId{};
            });
            return node;
        };

        register_node(tmpl);
    }

    // Set Time Scale
    {
        NodeTemplate tmpl;
        tmpl.id = builtin::SetTimeScale;
        tmpl.name = "Set Time Scale";
        tmpl.category = "Time";
        tmpl.tooltip = "Set the game time scale";
        tmpl.node_category = NodeCategory::Function;
        tmpl.purity = NodePurity::Impure;
        tmpl.title_color = 0xFFAAAA22;
        tmpl.keywords = "slow motion speed";

        Pin exec_in;
        exec_in.direction = PinDirection::Input;
        exec_in.type = PinType::Exec;
        tmpl.input_pins.push_back(std::move(exec_in));

        Pin scale;
        scale.name = "Scale";
        scale.direction = PinDirection::Input;
        scale.type = PinType::Float;
        scale.default_value.value = 1.0f;
        tmpl.input_pins.push_back(std::move(scale));

        Pin exec_out;
        exec_out.direction = PinDirection::Output;
        exec_out.type = PinType::Exec;
        tmpl.output_pins.push_back(std::move(exec_out));

        tmpl.create = [counter]() {
            auto node = std::make_unique<FunctionNode>(
                NodeId::from_bits((*counter)++), builtin::SetTimeScale, "Set Time Scale");
            node->set_pure(false);
            return node;
        };

        register_node(tmpl);
    }
}

} // namespace void_graph
