#include "graph.hpp"
#include "registry.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <queue>
#include <sstream>
#include <unordered_map>

namespace void_graph {

// =============================================================================
// Graph Implementation
// =============================================================================

Graph::Graph(GraphId id, const std::string& name)
    : id_(id.is_valid() ? id : GraphId::from_bits(next_node_id_++)), name_(name) {}

INode* Graph::add_node(std::unique_ptr<INode> node) {
    if (!node) return nullptr;

    NodeId id = node->id();
    INode* ptr = node.get();

    node_index_[id] = nodes_.size();
    nodes_.push_back(std::move(node));

    if (on_node_added_) {
        on_node_added_(ptr);
    }

    return ptr;
}

INode* Graph::create_node(const NodeTemplate& tmpl, float x, float y) {
    if (!tmpl.create) return nullptr;

    auto node = tmpl.create();
    if (!node) return nullptr;

    node->set_position(x, y);
    return add_node(std::move(node));
}

INode* Graph::create_node(NodeTypeId type_id, float x, float y) {
    const NodeTemplate* tmpl = NodeRegistry::instance().get_template(type_id);
    if (!tmpl) return nullptr;
    return create_node(*tmpl, x, y);
}

bool Graph::remove_node(NodeId id) {
    auto it = node_index_.find(id);
    if (it == node_index_.end()) return false;

    std::size_t index = it->second;
    INode* node = nodes_[index].get();

    // Remove all connections
    disconnect_node(id);

    // Callback before removal
    if (on_node_removed_) {
        on_node_removed_(node);
    }

    // Swap and pop
    if (index < nodes_.size() - 1) {
        node_index_[nodes_.back()->id()] = index;
        std::swap(nodes_[index], nodes_.back());
    }
    nodes_.pop_back();
    node_index_.erase(id);

    return true;
}

INode* Graph::get_node(NodeId id) {
    auto it = node_index_.find(id);
    if (it == node_index_.end()) return nullptr;
    return nodes_[it->second].get();
}

const INode* Graph::get_node(NodeId id) const {
    auto it = node_index_.find(id);
    if (it == node_index_.end()) return nullptr;
    return nodes_[it->second].get();
}

std::span<const std::unique_ptr<INode>> Graph::nodes() const {
    return nodes_;
}

std::vector<INode*> Graph::find_nodes_by_category(NodeCategory category) const {
    std::vector<INode*> result;
    for (const auto& node : nodes_) {
        if (node->category() == category) {
            result.push_back(node.get());
        }
    }
    return result;
}

std::vector<INode*> Graph::find_nodes_by_type(NodeTypeId type_id) const {
    std::vector<INode*> result;
    for (const auto& node : nodes_) {
        if (node->type_id() == type_id) {
            result.push_back(node.get());
        }
    }
    return result;
}

std::vector<EventNode*> Graph::get_event_nodes() const {
    std::vector<EventNode*> result;
    for (const auto& node : nodes_) {
        if (node->category() == NodeCategory::Event) {
            result.push_back(static_cast<EventNode*>(node.get()));
        }
    }
    return result;
}

// =============================================================================
// Connection Management
// =============================================================================

GraphResult<ConnectionId> Graph::connect(PinId source, PinId target) {
    auto validation = can_connect(source, target);
    if (!validation) {
        return GraphResult<ConnectionId>(validation.error());
    }

    // Find source and target pins
    INode* source_node = find_pin_owner(source);
    INode* target_node = find_pin_owner(target);

    if (!source_node || !target_node) {
        return GraphResult<ConnectionId>(GraphError::InvalidPin);
    }

    const Pin* source_pin = source_node->find_pin(source);
    const Pin* target_pin = target_node->find_pin(target);

    // Ensure source is output and target is input
    if (source_pin->direction != PinDirection::Output ||
        target_pin->direction != PinDirection::Input) {
        // Swap if needed
        if (source_pin->direction == PinDirection::Input &&
            target_pin->direction == PinDirection::Output) {
            std::swap(source, target);
            std::swap(source_node, target_node);
            std::swap(source_pin, target_pin);
        } else {
            return GraphResult<ConnectionId>(GraphError::InvalidConnection);
        }
    }

    // For input pins, disconnect existing connection (single input rule)
    if (target_pin->type != PinType::Exec) {
        disconnect_pin(target);
    }

    // Create connection
    Connection conn;
    conn.id = ConnectionId::from_bits(next_connection_id_++);
    conn.source = source;
    conn.target = target;
    conn.source_node = source_node->id();
    conn.target_node = target_node->id();

    connection_index_[conn.id] = connections_.size();
    connections_.push_back(conn);

    // Track pin connections
    pin_connections_[source].push_back(conn.id);
    pin_connections_[target].push_back(conn.id);

    if (on_connection_added_) {
        on_connection_added_(conn);
    }

    return GraphResult<ConnectionId>(conn.id);
}

bool Graph::disconnect(ConnectionId id) {
    auto it = connection_index_.find(id);
    if (it == connection_index_.end()) return false;

    std::size_t index = it->second;
    const Connection& conn = connections_[index];

    // Remove from pin tracking
    auto& source_conns = pin_connections_[conn.source];
    source_conns.erase(std::remove(source_conns.begin(), source_conns.end(), id), source_conns.end());

    auto& target_conns = pin_connections_[conn.target];
    target_conns.erase(std::remove(target_conns.begin(), target_conns.end(), id), target_conns.end());

    if (on_connection_removed_) {
        on_connection_removed_(conn);
    }

    // Swap and pop
    if (index < connections_.size() - 1) {
        connection_index_[connections_.back().id] = index;
        std::swap(connections_[index], connections_.back());
    }
    connections_.pop_back();
    connection_index_.erase(id);

    return true;
}

void Graph::disconnect_pin(PinId id) {
    auto it = pin_connections_.find(id);
    if (it == pin_connections_.end()) return;

    auto conn_ids = it->second;  // Copy to avoid iterator invalidation
    for (ConnectionId conn_id : conn_ids) {
        disconnect(conn_id);
    }
}

void Graph::disconnect_node(NodeId id) {
    const INode* node = get_node(id);
    if (!node) return;

    for (const auto& pin : node->input_pins()) {
        disconnect_pin(pin.id);
    }
    for (const auto& pin : node->output_pins()) {
        disconnect_pin(pin.id);
    }
}

GraphResult<void> Graph::can_connect(PinId source, PinId target) const {
    INode* source_node = find_pin_owner(source);
    INode* target_node = find_pin_owner(target);

    if (!source_node || !target_node) {
        return GraphResult<void>(GraphError::InvalidPin);
    }

    // Cannot connect to self
    if (source_node == target_node) {
        return GraphResult<void>(GraphError::InvalidConnection);
    }

    const Pin* source_pin = source_node->find_pin(source);
    const Pin* target_pin = target_node->find_pin(target);

    if (!source_pin || !target_pin) {
        return GraphResult<void>(GraphError::InvalidPin);
    }

    // Check type compatibility
    if (!source_pin->can_connect_to(*target_pin)) {
        return GraphResult<void>(GraphError::TypeMismatch);
    }

    // Check for cycles (for exec connections)
    if (source_pin->type == PinType::Exec && would_create_cycle(source, target)) {
        return GraphResult<void>(GraphError::CyclicConnection);
    }

    return GraphResult<void>::ok();
}

const Connection* Graph::get_connection(ConnectionId id) const {
    auto it = connection_index_.find(id);
    if (it == connection_index_.end()) return nullptr;
    return &connections_[it->second];
}

std::span<const Connection> Graph::connections() const {
    return connections_;
}

std::vector<const Connection*> Graph::get_connections_for_pin(PinId id) const {
    std::vector<const Connection*> result;
    auto it = pin_connections_.find(id);
    if (it != pin_connections_.end()) {
        for (ConnectionId conn_id : it->second) {
            if (const Connection* conn = get_connection(conn_id)) {
                result.push_back(conn);
            }
        }
    }
    return result;
}

std::vector<const Connection*> Graph::get_connections_for_node(NodeId id) const {
    std::vector<const Connection*> result;
    const INode* node = get_node(id);
    if (!node) return result;

    for (const auto& pin : node->input_pins()) {
        for (const Connection* conn : get_connections_for_pin(pin.id)) {
            result.push_back(conn);
        }
    }
    for (const auto& pin : node->output_pins()) {
        for (const Connection* conn : get_connections_for_pin(pin.id)) {
            result.push_back(conn);
        }
    }

    return result;
}

PinId Graph::get_connected_output(PinId input_pin) const {
    auto it = pin_connections_.find(input_pin);
    if (it == pin_connections_.end() || it->second.empty()) {
        return PinId::null();
    }

    const Connection* conn = get_connection(it->second[0]);
    if (!conn) return PinId::null();

    return conn->target == input_pin ? conn->source : PinId::null();
}

std::vector<PinId> Graph::get_connected_inputs(PinId output_pin) const {
    std::vector<PinId> result;
    auto it = pin_connections_.find(output_pin);
    if (it != pin_connections_.end()) {
        for (ConnectionId conn_id : it->second) {
            if (const Connection* conn = get_connection(conn_id)) {
                if (conn->source == output_pin) {
                    result.push_back(conn->target);
                }
            }
        }
    }
    return result;
}

bool Graph::would_create_cycle(PinId source, PinId target) const {
    INode* source_node = find_pin_owner(source);
    INode* target_node = find_pin_owner(target);

    if (!source_node || !target_node) return false;

    std::unordered_set<NodeId> visited;
    return has_cycle_dfs(target_node->id(), source_node->id(), visited);
}

bool Graph::has_cycle_dfs(NodeId start, NodeId target, std::unordered_set<NodeId>& visited) const {
    if (start == target) return true;
    if (visited.count(start)) return false;

    visited.insert(start);

    const INode* node = get_node(start);
    if (!node) return false;

    // Follow output connections
    for (const auto& pin : node->output_pins()) {
        if (pin.type == PinType::Exec) {
            for (PinId connected : get_connected_inputs(pin.id)) {
                INode* connected_node = find_pin_owner(connected);
                if (connected_node && has_cycle_dfs(connected_node->id(), target, visited)) {
                    return true;
                }
            }
        }
    }

    return false;
}

INode* Graph::find_pin_owner(PinId pin_id) const {
    for (const auto& node : nodes_) {
        if (node->find_pin(pin_id)) {
            return node.get();
        }
    }
    return nullptr;
}

// =============================================================================
// Variable Management
// =============================================================================

VariableId Graph::add_variable(const GraphVariable& var) {
    VariableId id = var.id.is_valid() ? var.id : VariableId::from_bits(next_variable_id_++);

    GraphVariable v = var;
    v.id = id;

    variable_index_[id] = variables_.size();
    variables_.push_back(std::move(v));

    return id;
}

bool Graph::remove_variable(VariableId id) {
    auto it = variable_index_.find(id);
    if (it == variable_index_.end()) return false;

    std::size_t index = it->second;

    if (index < variables_.size() - 1) {
        variable_index_[variables_.back().id] = index;
        std::swap(variables_[index], variables_.back());
    }
    variables_.pop_back();
    variable_index_.erase(id);

    return true;
}

GraphVariable* Graph::get_variable(VariableId id) {
    auto it = variable_index_.find(id);
    if (it == variable_index_.end()) return nullptr;
    return &variables_[it->second];
}

const GraphVariable* Graph::get_variable(VariableId id) const {
    auto it = variable_index_.find(id);
    if (it == variable_index_.end()) return nullptr;
    return &variables_[it->second];
}

GraphVariable* Graph::find_variable(const std::string& name) {
    for (auto& var : variables_) {
        if (var.name == name) return &var;
    }
    return nullptr;
}

std::span<const GraphVariable> Graph::variables() const {
    return variables_;
}

VariableNode* Graph::create_getter(VariableId var_id, float x, float y) {
    GraphVariable* var = get_variable(var_id);
    if (!var) return nullptr;

    auto node = std::make_unique<VariableNode>(
        NodeId::from_bits(next_node_id_++),
        builtin::GetVariable,
        "Get " + var->name,
        VariableNode::Mode::Get
    );
    node->set_variable_id(var_id);
    node->set_variable_type(var->type);
    node->set_position(x, y);

    return static_cast<VariableNode*>(add_node(std::move(node)));
}

VariableNode* Graph::create_setter(VariableId var_id, float x, float y) {
    GraphVariable* var = get_variable(var_id);
    if (!var) return nullptr;

    auto node = std::make_unique<VariableNode>(
        NodeId::from_bits(next_node_id_++),
        builtin::SetVariable,
        "Set " + var->name,
        VariableNode::Mode::Set
    );
    node->set_variable_id(var_id);
    node->set_variable_type(var->type);
    node->set_position(x, y);

    return static_cast<VariableNode*>(add_node(std::move(node)));
}

// =============================================================================
// Interface Pins
// =============================================================================

void Graph::add_interface_input(const std::string& name, PinType type) {
    Pin pin;
    pin.name = name;
    pin.direction = PinDirection::Input;
    pin.type = type;
    pin.color = pin_type_color(type);
    interface_inputs_.push_back(std::move(pin));
}

void Graph::add_interface_output(const std::string& name, PinType type) {
    Pin pin;
    pin.name = name;
    pin.direction = PinDirection::Output;
    pin.type = type;
    pin.color = pin_type_color(type);
    interface_outputs_.push_back(std::move(pin));
}

// =============================================================================
// Utility
// =============================================================================

void Graph::clear() {
    nodes_.clear();
    node_index_.clear();
    connections_.clear();
    connection_index_.clear();
    pin_connections_.clear();
    variables_.clear();
    variable_index_.clear();
    interface_inputs_.clear();
    interface_outputs_.clear();
}

GraphResult<void> Graph::validate() const {
    // Check for disconnected exec outputs
    for (const auto& node : nodes_) {
        if (node->is_disabled()) continue;

        // Event nodes must have at least one connection
        if (node->category() == NodeCategory::Event) {
            bool has_connection = false;
            for (const auto& pin : node->output_pins()) {
                if (pin.type == PinType::Exec && !get_connected_inputs(pin.id).empty()) {
                    has_connection = true;
                    break;
                }
            }
            if (!has_connection) {
                return GraphResult<void>(GraphError::InvalidGraph);
            }
        }
    }

    return GraphResult<void>::ok();
}

std::vector<NodeId> Graph::get_orphaned_nodes() const {
    std::vector<NodeId> result;
    for (const auto& node : nodes_) {
        if (node->category() == NodeCategory::Comment) continue;
        if (node->category() == NodeCategory::Reroute) continue;

        bool has_connection = false;
        for (const auto& pin : node->input_pins()) {
            if (!get_connections_for_pin(pin.id).empty()) {
                has_connection = true;
                break;
            }
        }
        if (!has_connection) {
            for (const auto& pin : node->output_pins()) {
                if (!get_connections_for_pin(pin.id).empty()) {
                    has_connection = true;
                    break;
                }
            }
        }

        if (!has_connection && node->category() != NodeCategory::Event) {
            result.push_back(node->id());
        }
    }
    return result;
}

std::vector<NodeId> Graph::compute_execution_order() const {
    std::vector<NodeId> result;
    std::unordered_set<NodeId> visited;
    std::unordered_map<NodeId, std::size_t> in_degree;

    // Build in-degree map
    for (const auto& node : nodes_) {
        in_degree[node->id()] = 0;
    }

    for (const auto& conn : connections_) {
        ++in_degree[conn.target_node];
    }

    // Start with nodes that have no incoming connections
    std::queue<NodeId> queue;
    for (const auto& [id, degree] : in_degree) {
        if (degree == 0) {
            queue.push(id);
        }
    }

    // Topological sort
    while (!queue.empty()) {
        NodeId id = queue.front();
        queue.pop();

        if (visited.count(id)) continue;
        visited.insert(id);
        result.push_back(id);

        const INode* node = get_node(id);
        if (!node) continue;

        for (const auto& pin : node->output_pins()) {
            for (const Connection* conn : get_connections_for_pin(pin.id)) {
                if (--in_degree[conn->target_node] == 0) {
                    queue.push(conn->target_node);
                }
            }
        }
    }

    return result;
}

std::unique_ptr<Graph> Graph::clone() const {
    auto copy = std::make_unique<Graph>(GraphId{}, name_);
    copy->type_ = type_;
    copy->metadata_ = metadata_;

    // TODO: Deep clone nodes and rebuild connections
    // This requires serialization/deserialization or manual cloning

    return copy;
}

// =============================================================================
// Serialization
// =============================================================================

std::string Graph::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << name_ << "\",\n";
    ss << "  \"type\": " << static_cast<int>(type_) << ",\n";
    ss << "  \"nodes\": [\n";

    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        ss << "    {\n";
        ss << "      \"id\": " << node->id().to_bits() << ",\n";
        ss << "      \"type_id\": " << node->type_id().to_bits() << ",\n";
        ss << "      \"name\": \"" << node->name() << "\",\n";
        ss << "      \"x\": " << node->x() << ",\n";
        ss << "      \"y\": " << node->y() << "\n";
        ss << "    }";
        if (i < nodes_.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "  ],\n";
    ss << "  \"connections\": [\n";

    for (std::size_t i = 0; i < connections_.size(); ++i) {
        const auto& conn = connections_[i];
        ss << "    {\n";
        ss << "      \"source\": " << conn.source.to_bits() << ",\n";
        ss << "      \"target\": " << conn.target.to_bits() << "\n";
        ss << "    }";
        if (i < connections_.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";

    return ss.str();
}

namespace {
    // Simple JSON tokenizer for graph parsing
    enum class JsonTokenType { Object, Array, String, Number, Bool, Null, Colon, Comma, End };

    struct JsonToken {
        JsonTokenType type;
        std::string value;
    };

    class SimpleJsonParser {
    public:
        explicit SimpleJsonParser(const std::string& json) : json_(json), pos_(0) {}

        bool parse_graph(Graph& graph, const NodeRegistry& registry) {
            skip_whitespace();
            if (!expect('{')) return false;

            while (pos_ < json_.size()) {
                skip_whitespace();
                if (peek() == '}') { ++pos_; break; }

                std::string key = parse_string();
                skip_whitespace();
                if (!expect(':')) return false;
                skip_whitespace();

                if (key == "name") {
                    graph.set_name(parse_string());
                } else if (key == "type") {
                    graph.set_type(static_cast<GraphType>(parse_int()));
                } else if (key == "nodes") {
                    parse_nodes(graph, registry);
                } else if (key == "connections") {
                    parse_connections(graph);
                } else if (key == "variables") {
                    parse_variables(graph);
                } else {
                    skip_value();
                }

                skip_whitespace();
                if (peek() == ',') ++pos_;
            }
            return true;
        }

    private:
        void skip_whitespace() {
            while (pos_ < json_.size() && std::isspace(json_[pos_])) ++pos_;
        }

        char peek() const { return pos_ < json_.size() ? json_[pos_] : '\0'; }
        bool expect(char c) { if (peek() == c) { ++pos_; return true; } return false; }

        std::string parse_string() {
            skip_whitespace();
            if (!expect('"')) return "";
            std::string result;
            while (pos_ < json_.size() && json_[pos_] != '"') {
                if (json_[pos_] == '\\' && pos_ + 1 < json_.size()) {
                    ++pos_;
                    switch (json_[pos_]) {
                        case 'n': result += '\n'; break;
                        case 't': result += '\t'; break;
                        case 'r': result += '\r'; break;
                        case '"': result += '"'; break;
                        case '\\': result += '\\'; break;
                        default: result += json_[pos_]; break;
                    }
                } else {
                    result += json_[pos_];
                }
                ++pos_;
            }
            expect('"');
            return result;
        }

        int parse_int() {
            skip_whitespace();
            int result = 0;
            bool negative = false;
            if (peek() == '-') { negative = true; ++pos_; }
            while (pos_ < json_.size() && std::isdigit(json_[pos_])) {
                result = result * 10 + (json_[pos_] - '0');
                ++pos_;
            }
            return negative ? -result : result;
        }

        float parse_float() {
            skip_whitespace();
            std::string num_str;
            while (pos_ < json_.size() && (std::isdigit(json_[pos_]) ||
                   json_[pos_] == '.' || json_[pos_] == '-' || json_[pos_] == 'e' || json_[pos_] == 'E')) {
                num_str += json_[pos_++];
            }
            return num_str.empty() ? 0.0f : std::stof(num_str);
        }

        void skip_value() {
            skip_whitespace();
            char c = peek();
            if (c == '"') {
                parse_string();
            } else if (c == '{') {
                skip_object();
            } else if (c == '[') {
                skip_array();
            } else {
                while (pos_ < json_.size() && json_[pos_] != ',' && json_[pos_] != '}' && json_[pos_] != ']') {
                    ++pos_;
                }
            }
        }

        void skip_object() {
            expect('{');
            int depth = 1;
            while (pos_ < json_.size() && depth > 0) {
                if (json_[pos_] == '{') ++depth;
                else if (json_[pos_] == '}') --depth;
                else if (json_[pos_] == '"') {
                    ++pos_;
                    while (pos_ < json_.size() && json_[pos_] != '"') {
                        if (json_[pos_] == '\\') ++pos_;
                        ++pos_;
                    }
                }
                ++pos_;
            }
        }

        void skip_array() {
            expect('[');
            int depth = 1;
            while (pos_ < json_.size() && depth > 0) {
                if (json_[pos_] == '[') ++depth;
                else if (json_[pos_] == ']') --depth;
                else if (json_[pos_] == '"') {
                    ++pos_;
                    while (pos_ < json_.size() && json_[pos_] != '"') {
                        if (json_[pos_] == '\\') ++pos_;
                        ++pos_;
                    }
                }
                ++pos_;
            }
        }

        void parse_nodes(Graph& graph, const NodeRegistry& registry) {
            skip_whitespace();
            if (!expect('[')) return;

            while (pos_ < json_.size()) {
                skip_whitespace();
                if (peek() == ']') { ++pos_; break; }

                if (expect('{')) {
                    std::uint32_t id = 0, type_id = 0;
                    float x = 0, y = 0;
                    std::string name;

                    while (pos_ < json_.size()) {
                        skip_whitespace();
                        if (peek() == '}') { ++pos_; break; }

                        std::string key = parse_string();
                        skip_whitespace();
                        expect(':');
                        skip_whitespace();

                        if (key == "id") id = static_cast<std::uint32_t>(parse_int());
                        else if (key == "type_id") type_id = static_cast<std::uint32_t>(parse_int());
                        else if (key == "name") name = parse_string();
                        else if (key == "x") x = parse_float();
                        else if (key == "y") y = parse_float();
                        else skip_value();

                        skip_whitespace();
                        if (peek() == ',') ++pos_;
                    }

                    auto node = registry.create_node(NodeTypeId::from_bits(type_id), NodeId::from_bits(id));
                    if (node) {
                        node->set_position(x, y);
                        graph.add_node(std::move(node));
                    }
                }

                skip_whitespace();
                if (peek() == ',') ++pos_;
            }
        }

        void parse_connections(Graph& graph) {
            skip_whitespace();
            if (!expect('[')) return;

            while (pos_ < json_.size()) {
                skip_whitespace();
                if (peek() == ']') { ++pos_; break; }

                if (expect('{')) {
                    std::uint32_t source = 0, target = 0;

                    while (pos_ < json_.size()) {
                        skip_whitespace();
                        if (peek() == '}') { ++pos_; break; }

                        std::string key = parse_string();
                        skip_whitespace();
                        expect(':');
                        skip_whitespace();

                        if (key == "source") source = static_cast<std::uint32_t>(parse_int());
                        else if (key == "target") target = static_cast<std::uint32_t>(parse_int());
                        else skip_value();

                        skip_whitespace();
                        if (peek() == ',') ++pos_;
                    }

                    graph.connect(PinId::from_bits(source), PinId::from_bits(target));
                }

                skip_whitespace();
                if (peek() == ',') ++pos_;
            }
        }

        void parse_variables(Graph& graph) {
            skip_whitespace();
            if (!expect('[')) return;

            while (pos_ < json_.size()) {
                skip_whitespace();
                if (peek() == ']') { ++pos_; break; }

                if (expect('{')) {
                    GraphVariable var;

                    while (pos_ < json_.size()) {
                        skip_whitespace();
                        if (peek() == '}') { ++pos_; break; }

                        std::string key = parse_string();
                        skip_whitespace();
                        expect(':');
                        skip_whitespace();

                        if (key == "name") var.name = parse_string();
                        else if (key == "type") var.type = static_cast<PinType>(parse_int());
                        else skip_value();

                        skip_whitespace();
                        if (peek() == ',') ++pos_;
                    }

                    graph.add_variable(var);
                }

                skip_whitespace();
                if (peek() == ',') ++pos_;
            }
        }

        const std::string& json_;
        std::size_t pos_;
    };

    // Simple TOML parser for blueprint files
    class SimpleTomlParser {
    public:
        explicit SimpleTomlParser(const std::string& toml) : toml_(toml), pos_(0), line_(1) {}

        bool parse_graph(Graph& graph, const NodeRegistry& registry) {
            std::unordered_map<std::uint32_t, NodeId> node_id_map;

            while (pos_ < toml_.size()) {
                skip_whitespace_and_comments();
                if (pos_ >= toml_.size()) break;

                if (peek() == '[') {
                    std::string section = parse_section();

                    if (section == "blueprint") {
                        parse_blueprint_section(graph);
                    } else if (section == "variables") {
                        parse_variable(graph);
                    } else if (section == "nodes") {
                        parse_node(graph, registry, node_id_map);
                    } else if (section == "connections") {
                        parse_connection(graph, node_id_map);
                    }
                } else {
                    skip_line();
                }
            }
            return true;
        }

    private:
        void skip_whitespace() {
            while (pos_ < toml_.size() && (toml_[pos_] == ' ' || toml_[pos_] == '\t')) ++pos_;
        }

        void skip_whitespace_and_comments() {
            while (pos_ < toml_.size()) {
                if (toml_[pos_] == ' ' || toml_[pos_] == '\t') {
                    ++pos_;
                } else if (toml_[pos_] == '\n' || toml_[pos_] == '\r') {
                    if (toml_[pos_] == '\r' && pos_ + 1 < toml_.size() && toml_[pos_ + 1] == '\n') ++pos_;
                    ++pos_;
                    ++line_;
                } else if (toml_[pos_] == '#') {
                    skip_line();
                } else {
                    break;
                }
            }
        }

        void skip_line() {
            while (pos_ < toml_.size() && toml_[pos_] != '\n' && toml_[pos_] != '\r') ++pos_;
            if (pos_ < toml_.size()) {
                if (toml_[pos_] == '\r' && pos_ + 1 < toml_.size() && toml_[pos_ + 1] == '\n') ++pos_;
                ++pos_;
                ++line_;
            }
        }

        char peek() const { return pos_ < toml_.size() ? toml_[pos_] : '\0'; }
        bool expect(char c) { if (peek() == c) { ++pos_; return true; } return false; }

        std::string parse_section() {
            std::string section;
            bool is_array = false;

            expect('[');
            if (peek() == '[') { expect('['); is_array = true; }

            while (pos_ < toml_.size() && toml_[pos_] != ']' && toml_[pos_] != '\n') {
                section += toml_[pos_++];
            }

            expect(']');
            if (is_array) expect(']');
            skip_line();

            return section;
        }

        std::string parse_key() {
            skip_whitespace();
            std::string key;
            while (pos_ < toml_.size() && toml_[pos_] != '=' && toml_[pos_] != ' ' &&
                   toml_[pos_] != '\t' && toml_[pos_] != '\n') {
                key += toml_[pos_++];
            }
            return key;
        }

        std::string parse_string_value() {
            skip_whitespace();
            if (!expect('"')) return "";
            std::string result;
            while (pos_ < toml_.size() && toml_[pos_] != '"') {
                if (toml_[pos_] == '\\' && pos_ + 1 < toml_.size()) {
                    ++pos_;
                    switch (toml_[pos_]) {
                        case 'n': result += '\n'; break;
                        case 't': result += '\t'; break;
                        case 'r': result += '\r'; break;
                        default: result += toml_[pos_]; break;
                    }
                } else {
                    result += toml_[pos_];
                }
                ++pos_;
            }
            expect('"');
            return result;
        }

        int parse_int_value() {
            skip_whitespace();
            std::string num;
            if (peek() == '-' || peek() == '+') num += toml_[pos_++];
            while (pos_ < toml_.size() && std::isdigit(toml_[pos_])) {
                num += toml_[pos_++];
            }
            return num.empty() ? 0 : std::stoi(num);
        }

        float parse_float_value() {
            skip_whitespace();
            std::string num;
            while (pos_ < toml_.size() && (std::isdigit(toml_[pos_]) ||
                   toml_[pos_] == '.' || toml_[pos_] == '-' || toml_[pos_] == '+' ||
                   toml_[pos_] == 'e' || toml_[pos_] == 'E')) {
                num += toml_[pos_++];
            }
            return num.empty() ? 0.0f : std::stof(num);
        }

        bool parse_bool_value() {
            skip_whitespace();
            if (toml_.substr(pos_, 4) == "true") { pos_ += 4; return true; }
            if (toml_.substr(pos_, 5) == "false") { pos_ += 5; return false; }
            return false;
        }

        void parse_blueprint_section(Graph& graph) {
            while (pos_ < toml_.size() && peek() != '[') {
                skip_whitespace_and_comments();
                if (peek() == '[') break;

                std::string key = parse_key();
                if (key.empty()) { skip_line(); continue; }

                skip_whitespace();
                if (!expect('=')) { skip_line(); continue; }

                if (key == "name") {
                    graph.set_name(parse_string_value());
                } else if (key == "version") {
                    parse_string_value(); // Skip version
                }
                skip_line();
            }
        }

        void parse_variable(Graph& graph) {
            GraphVariable var;

            while (pos_ < toml_.size() && peek() != '[') {
                skip_whitespace_and_comments();
                if (peek() == '[') break;

                std::string key = parse_key();
                if (key.empty()) { skip_line(); continue; }

                skip_whitespace();
                if (!expect('=')) { skip_line(); continue; }

                if (key == "name") {
                    var.name = parse_string_value();
                } else if (key == "type") {
                    std::string type_str = parse_string_value();
                    if (type_str == "bool") var.type = PinType::Bool;
                    else if (type_str == "int") var.type = PinType::Int;
                    else if (type_str == "float") var.type = PinType::Float;
                    else if (type_str == "string") var.type = PinType::String;
                    else if (type_str == "vec3") var.type = PinType::Vec3;
                } else if (key == "default") {
                    // Parse based on type
                    skip_whitespace();
                    if (peek() == '"') {
                        var.default_value.value = parse_string_value();
                    } else if (peek() == 't' || peek() == 'f') {
                        var.default_value.value = parse_bool_value();
                    } else {
                        var.default_value.value = parse_float_value();
                    }
                } else if (key == "exposed") {
                    var.is_exposed = parse_bool_value();
                }
                skip_line();
            }

            if (!var.name.empty()) {
                graph.add_variable(var);
            }
        }

        void parse_node(Graph& graph, const NodeRegistry& registry,
                       std::unordered_map<std::uint32_t, NodeId>& node_id_map) {
            std::uint32_t id = 0;
            std::string type_name;
            float x = 0, y = 0;

            while (pos_ < toml_.size() && peek() != '[') {
                skip_whitespace_and_comments();
                if (peek() == '[') break;

                std::string key = parse_key();
                if (key.empty()) { skip_line(); continue; }

                skip_whitespace();
                if (!expect('=')) { skip_line(); continue; }

                if (key == "id") {
                    id = static_cast<std::uint32_t>(parse_int_value());
                } else if (key == "type") {
                    type_name = parse_string_value();
                } else if (key == "position") {
                    // Parse [x, y] array
                    skip_whitespace();
                    if (expect('[')) {
                        x = parse_float_value();
                        skip_whitespace();
                        expect(',');
                        y = parse_float_value();
                        expect(']');
                    }
                }
                skip_line();
            }

            if (id > 0 && !type_name.empty()) {
                auto node = registry.create_node(type_name, NodeId::from_bits(id));
                if (node) {
                    node->set_position(x, y);
                    node_id_map[id] = node->id();
                    graph.add_node(std::move(node));
                }
            }
        }

        void parse_connection(Graph& graph, const std::unordered_map<std::uint32_t, NodeId>& node_id_map) {
            std::uint32_t from_node = 0, to_node = 0;
            std::string from_pin, to_pin;

            while (pos_ < toml_.size() && peek() != '[') {
                skip_whitespace_and_comments();
                if (peek() == '[') break;

                std::string key = parse_key();
                if (key.empty()) { skip_line(); continue; }

                skip_whitespace();
                if (!expect('=')) { skip_line(); continue; }

                // Handle inline tables: from = { node = 1, pin = "exec_out" }
                if (key == "from" || key == "to") {
                    skip_whitespace();
                    if (expect('{')) {
                        std::uint32_t* node_ptr = (key == "from") ? &from_node : &to_node;
                        std::string* pin_ptr = (key == "from") ? &from_pin : &to_pin;

                        while (peek() != '}' && pos_ < toml_.size()) {
                            skip_whitespace();
                            std::string subkey = parse_key();
                            skip_whitespace();
                            expect('=');
                            skip_whitespace();

                            if (subkey == "node") {
                                *node_ptr = static_cast<std::uint32_t>(parse_int_value());
                            } else if (subkey == "pin") {
                                *pin_ptr = parse_string_value();
                            }

                            skip_whitespace();
                            if (peek() == ',') ++pos_;
                        }
                        expect('}');
                    }
                }
                skip_line();
            }

            // Create connection using node IDs and pin names
            // This is simplified - full implementation would look up pins by name
            if (from_node > 0 && to_node > 0) {
                auto from_it = node_id_map.find(from_node);
                auto to_it = node_id_map.find(to_node);

                if (from_it != node_id_map.end() && to_it != node_id_map.end()) {
                    INode* from_node_ptr = graph.get_node(from_it->second);
                    INode* to_node_ptr = graph.get_node(to_it->second);

                    if (from_node_ptr && to_node_ptr) {
                        const Pin* from_pin_ptr = from_node_ptr->find_pin_by_name(from_pin);
                        const Pin* to_pin_ptr = to_node_ptr->find_pin_by_name(to_pin);

                        if (from_pin_ptr && to_pin_ptr) {
                            graph.connect(from_pin_ptr->id, to_pin_ptr->id);
                        }
                    }
                }
            }
        }

        const std::string& toml_;
        std::size_t pos_;
        std::size_t line_;
    };
} // anonymous namespace

std::unique_ptr<Graph> Graph::from_json(const std::string& json, const NodeRegistry& registry) {
    auto graph = std::make_unique<Graph>();
    SimpleJsonParser parser(json);

    if (!parser.parse_graph(*graph, registry)) {
        return nullptr;
    }

    return graph;
}

std::unique_ptr<Graph> Graph::from_toml(const std::string& toml, const NodeRegistry& registry) {
    auto graph = std::make_unique<Graph>();
    SimpleTomlParser parser(toml);

    if (!parser.parse_graph(*graph, registry)) {
        return nullptr;
    }

    return graph;
}

std::unique_ptr<Graph> Graph::load(const std::filesystem::path& path, const NodeRegistry& registry) {
    std::ifstream file(path);
    if (!file) return nullptr;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string ext = path.extension().string();
    if (ext == ".json") {
        return from_json(content, registry);
    } else if (ext == ".toml" || ext == ".bp") {
        return from_toml(content, registry);
    } else if (ext == ".vgraph") {
        file.seekg(0);
        return deserialize(file, registry);
    }

    // Try to auto-detect format
    if (content.find('{') == 0) {
        return from_json(content, registry);
    } else if (content.find('[') != std::string::npos || content.find('=') != std::string::npos) {
        return from_toml(content, registry);
    }

    return nullptr;
}

void Graph::serialize(std::ostream& out) const {
    // Version
    std::uint32_t version = 1;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Name
    std::uint32_t name_len = static_cast<std::uint32_t>(name_.size());
    out.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
    out.write(name_.data(), name_len);

    // Type
    out.write(reinterpret_cast<const char*>(&type_), sizeof(type_));

    // Node count
    std::uint32_t node_count = static_cast<std::uint32_t>(nodes_.size());
    out.write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));

    // Nodes
    for (const auto& node : nodes_) {
        std::uint32_t id = node->id().to_bits();
        std::uint32_t type_id = node->type_id().to_bits();
        float x = node->x();
        float y = node->y();

        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        out.write(reinterpret_cast<const char*>(&type_id), sizeof(type_id));
        out.write(reinterpret_cast<const char*>(&x), sizeof(x));
        out.write(reinterpret_cast<const char*>(&y), sizeof(y));

        node->serialize(out);
    }

    // Connection count
    std::uint32_t conn_count = static_cast<std::uint32_t>(connections_.size());
    out.write(reinterpret_cast<const char*>(&conn_count), sizeof(conn_count));

    // Connections
    for (const auto& conn : connections_) {
        std::uint32_t source = conn.source.to_bits();
        std::uint32_t target = conn.target.to_bits();

        out.write(reinterpret_cast<const char*>(&source), sizeof(source));
        out.write(reinterpret_cast<const char*>(&target), sizeof(target));
    }
}

std::unique_ptr<Graph> Graph::deserialize(std::istream& in, const NodeRegistry& registry) {
    auto graph = std::make_unique<Graph>();

    // Version
    std::uint32_t version;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));

    // Name
    std::uint32_t name_len;
    in.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
    graph->name_.resize(name_len);
    in.read(graph->name_.data(), name_len);

    // Type
    in.read(reinterpret_cast<char*>(&graph->type_), sizeof(graph->type_));

    // Node count
    std::uint32_t node_count;
    in.read(reinterpret_cast<char*>(&node_count), sizeof(node_count));

    // Nodes
    for (std::uint32_t i = 0; i < node_count; ++i) {
        std::uint32_t id, type_id;
        float x, y;

        in.read(reinterpret_cast<char*>(&id), sizeof(id));
        in.read(reinterpret_cast<char*>(&type_id), sizeof(type_id));
        in.read(reinterpret_cast<char*>(&x), sizeof(x));
        in.read(reinterpret_cast<char*>(&y), sizeof(y));

        auto node = registry.create_node(NodeTypeId::from_bits(type_id), NodeId::from_bits(id));
        if (node) {
            node->set_position(x, y);
            node->deserialize(in);
            graph->add_node(std::move(node));
        }
    }

    // Connection count
    std::uint32_t conn_count;
    in.read(reinterpret_cast<char*>(&conn_count), sizeof(conn_count));

    // Connections
    for (std::uint32_t i = 0; i < conn_count; ++i) {
        std::uint32_t source, target;

        in.read(reinterpret_cast<char*>(&source), sizeof(source));
        in.read(reinterpret_cast<char*>(&target), sizeof(target));

        graph->connect(PinId::from_bits(source), PinId::from_bits(target));
    }

    return graph;
}

// =============================================================================
// Subgraph Implementation
// =============================================================================

Subgraph::Subgraph(SubgraphId id, const std::string& name)
    : Graph(GraphId::from_bits(id.to_bits()), name), subgraph_id_(id) {
    set_type(GraphType::Function);
}

// =============================================================================
// GraphInstance Implementation
// =============================================================================

GraphInstance::GraphInstance(const Graph& graph, std::uint64_t owner_entity)
    : graph_(graph) {
    context_.graph = graph.id();
    context_.owner_entity = owner_entity;

    // Initialize variables with defaults
    reset_variables();
}

void GraphInstance::reset_variables() {
    context_.variables.clear();
    for (const auto& var : graph_.variables()) {
        context_.variables[var.id] = var.default_value.value;
    }
}

void GraphInstance::reset_execution() {
    context_.state = ExecutionState::Idle;
    context_.current_node = NodeId{};
    context_.current_exec_pin = PinId{};
    context_.call_stack.clear();
    context_.pin_values.clear();
}

// =============================================================================
// GraphBuilder Implementation
// =============================================================================

GraphBuilder::GraphBuilder()
    : graph_(std::make_unique<Graph>()) {}

GraphBuilder::GraphBuilder(const std::string& name)
    : graph_(std::make_unique<Graph>(GraphId{}, name)) {}

GraphBuilder& GraphBuilder::name(const std::string& n) {
    graph_->set_name(n);
    return *this;
}

GraphBuilder& GraphBuilder::type(GraphType t) {
    graph_->set_type(t);
    return *this;
}

GraphBuilder& GraphBuilder::node(std::unique_ptr<INode> n) {
    graph_->add_node(std::move(n));
    return *this;
}

GraphBuilder& GraphBuilder::connect(PinId source, PinId target) {
    pending_connections_.emplace_back(source, target);
    return *this;
}

GraphBuilder& GraphBuilder::variable(const std::string& n, PinType type) {
    GraphVariable var;
    var.name = n;
    var.type = type;
    graph_->add_variable(var);
    return *this;
}

GraphBuilder& GraphBuilder::input(const std::string& n, PinType type) {
    graph_->add_interface_input(n, type);
    return *this;
}

GraphBuilder& GraphBuilder::output(const std::string& n, PinType type) {
    graph_->add_interface_output(n, type);
    return *this;
}

std::unique_ptr<Graph> GraphBuilder::build() {
    // Create pending connections
    for (const auto& [source, target] : pending_connections_) {
        graph_->connect(source, target);
    }
    pending_connections_.clear();

    return std::move(graph_);
}

} // namespace void_graph
