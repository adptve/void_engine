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

std::unique_ptr<INode> NodeRegistry::create_node(NodeTypeId type_id, NodeId node_id) const {
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
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    for (const auto& [id, tmpl] : templates_) {
        std::string lower_name = tmpl.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

        std::string lower_keywords = tmpl.keywords;
        std::transform(lower_keywords.begin(), lower_keywords.end(), lower_keywords.begin(), ::tolower);

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
    // String operations would be registered here
    // Similar pattern to math nodes
}

void NodeRegistry::register_array_nodes() {
    // Array operations would be registered here
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
            load_graph(entry.path());
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

} // namespace void_graph
