#include "system.hpp"

#include <algorithm>

namespace void_graph {

// =============================================================================
// GraphSystem Implementation
// =============================================================================

namespace {
    GraphSystem* g_instance = nullptr;
}

GraphSystem::GraphSystem() {
    g_instance = this;
}

GraphSystem::~GraphSystem() {
    if (g_instance == this) {
        g_instance = nullptr;
    }
}

GraphSystem& GraphSystem::instance() {
    if (!g_instance) {
        static GraphSystem default_instance;
        return default_instance;
    }
    return *g_instance;
}

GraphSystem* GraphSystem::instance_ptr() {
    return g_instance;
}

void GraphSystem::initialize() {
    if (initialized_) return;

    // Register built-in nodes
    registry_.register_builtins();

    initialized_ = true;
}

void GraphSystem::shutdown() {
    if (!initialized_) return;

    // Clear all data
    compiled_graphs_.clear();
    entity_components_.clear();
    graph_paths_.clear();
    graph_timestamps_.clear();

    initialized_ = false;
}

Graph* GraphSystem::create_graph(const std::string& name) {
    auto graph = std::make_unique<Graph>(GraphId{}, name);
    Graph* ptr = graph.get();
    library_.add_graph(std::move(graph));
    return ptr;
}

GraphBuilder GraphSystem::create_graph_builder(const std::string& name) {
    return GraphBuilder(name);
}

Graph* GraphSystem::load_graph(const std::filesystem::path& path) {
    auto result = library_.load_graph(path);
    if (!result) return nullptr;

    Graph* graph = result.value();
    graph_paths_[graph->id()] = path;
    graph_timestamps_[graph->id()] = std::filesystem::last_write_time(path);

    return graph;
}

bool GraphSystem::save_graph(GraphId id, const std::filesystem::path& path) {
    auto result = library_.save_graph(id, path);
    if (!result) return false;

    graph_paths_[id] = path;
    graph_timestamps_[id] = std::filesystem::last_write_time(path);

    return true;
}

Graph* GraphSystem::get_graph(GraphId id) {
    return library_.get_graph(id);
}

bool GraphSystem::delete_graph(GraphId id) {
    // Remove compiled version
    compiled_graphs_.erase(id);

    // Remove path tracking
    graph_paths_.erase(id);
    graph_timestamps_.erase(id);

    return library_.remove_graph(id);
}

GraphComponent* GraphSystem::attach_graph(std::uint64_t entity_id, GraphId graph_id) {
    Graph* graph = library_.get_graph(graph_id);
    if (!graph) return nullptr;

    GraphComponent& comp = entity_components_[entity_id];
    comp.graph_id = graph_id;
    comp.instance = std::make_unique<GraphInstance>(*graph, entity_id);
    comp.enabled = true;
    comp.auto_tick = true;

    // Bind events
    for (EventNode* event : graph->get_event_nodes()) {
        comp.event_bindings[event->event_name()] = event;
    }

    return &comp;
}

void GraphSystem::detach_graph(std::uint64_t entity_id) {
    auto it = entity_components_.find(entity_id);
    if (it == entity_components_.end()) return;

    // Abort any active executions
    for (ExecutionId exec_id : it->second.active_executions) {
        executor_.abort(exec_id);
    }

    entity_components_.erase(it);
}

GraphComponent* GraphSystem::get_component(std::uint64_t entity_id) {
    auto it = entity_components_.find(entity_id);
    if (it == entity_components_.end()) return nullptr;
    return &it->second;
}

ExecutionId GraphSystem::trigger_event(std::uint64_t entity_id, const std::string& event_name) {
    GraphComponent* comp = get_component(entity_id);
    if (!comp || !comp->enabled || !comp->instance) {
        return ExecutionId{};
    }

    auto it = comp->event_bindings.find(event_name);
    if (it == comp->event_bindings.end()) {
        return ExecutionId{};
    }

    ExecutionId id = executor_.start(*comp->instance, *it->second);
    comp->active_executions.push_back(id);

    // Emit event
    if (event_bus_) {
        GraphExecutionStartedEvent event;
        event.graph_id = comp->graph_id;
        event.execution_id = id;
        event.entity_id = entity_id;
        event_bus_->publish(event);
    }

    return id;
}

void GraphSystem::update(float delta_time) {
    // Check for hot reload
    if (hot_reload_enabled_) {
        check_hot_reload();
    }

    // Tick all entities with auto_tick
    for (auto& [entity_id, comp] : entity_components_) {
        if (comp.enabled && comp.auto_tick) {
            trigger_event(entity_id, "Tick");
        }
    }

    // Update executor
    executor_.update(delta_time);

    // Clean up completed executions
    for (auto& [entity_id, comp] : entity_components_) {
        std::erase_if(comp.active_executions, [this](ExecutionId id) {
            return !executor_.is_running(id);
        });
    }
}

ExecutionResult GraphSystem::execute_sync(GraphId graph_id, const std::string& entry_point) {
    Graph* graph = library_.get_graph(graph_id);
    if (!graph) {
        ExecutionResult result;
        result.final_state = ExecutionState::Error;
        result.error_message = "Graph not found";
        return result;
    }

    // Try compiled execution first
    CompiledGraph* compiled = compile_graph(graph_id);
    if (compiled && compiled->is_valid()) {
        ExecutionContext ctx;
        ctx.graph = graph_id;
        CompiledGraphExecutor compiled_executor;
        return compiled_executor.execute(*compiled, entry_point, ctx);
    }

    // Fall back to interpreter
    GraphInstance instance(*graph);

    // Find entry event
    for (EventNode* event : graph->get_event_nodes()) {
        if (event->event_name() == entry_point) {
            ExecutionId id = executor_.start(instance, *event);

            // Wait for completion
            while (executor_.is_running(id)) {
                executor_.update(0.016f);  // ~60 FPS
            }

            if (const ExecutionResult* result = executor_.get_result(id)) {
                return *result;
            }
            break;
        }
    }

    ExecutionResult result;
    result.final_state = ExecutionState::Error;
    result.error_message = "Entry point not found";
    return result;
}

ExecutionId GraphSystem::execute_async(GraphId graph_id, const std::string& entry_point) {
    Graph* graph = library_.get_graph(graph_id);
    if (!graph) {
        return ExecutionId{};
    }

    // Create temporary instance
    auto instance = std::make_unique<GraphInstance>(*graph);

    // Find entry event
    for (EventNode* event : graph->get_event_nodes()) {
        if (event->event_name() == entry_point) {
            return executor_.start(*instance, *event);
        }
    }

    return ExecutionId{};
}

CompiledGraph* GraphSystem::compile_graph(GraphId id) {
    auto it = compiled_graphs_.find(id);
    if (it != compiled_graphs_.end()) {
        return it->second.get();
    }

    Graph* graph = library_.get_graph(id);
    if (!graph) return nullptr;

    auto result = compiler_.compile(*graph);
    if (!result) return nullptr;

    auto compiled = std::make_unique<CompiledGraph>(std::move(result.value()));
    CompiledGraph* ptr = compiled.get();
    compiled_graphs_[id] = std::move(compiled);

    return ptr;
}

void GraphSystem::set_debug_mode(bool enabled) {
    debug_mode_ = enabled;
    executor_.set_debug_enabled(enabled);
}

void GraphSystem::toggle_breakpoint(GraphId graph, NodeId node) {
    // Check if breakpoint exists
    // This is simplified - would need proper tracking
    executor_.add_breakpoint(graph, node);
}

void GraphSystem::step(ExecutionId id) {
    executor_.step_into(id);
}

void GraphSystem::continue_execution(ExecutionId id) {
    executor_.resume(id);
}

std::vector<NodeId> GraphSystem::get_call_stack(ExecutionId id) const {
    // Would need access to executor internals
    return {};
}

std::vector<ExecutionId> GraphSystem::get_active_executions() const {
    std::vector<ExecutionId> result;
    for (const auto& [entity_id, comp] : entity_components_) {
        for (ExecutionId id : comp.active_executions) {
            result.push_back(id);
        }
    }
    return result;
}

bool GraphSystem::hot_reload(GraphId id) {
    auto path_it = graph_paths_.find(id);
    if (path_it == graph_paths_.end()) return false;

    // Remove old graph
    compiled_graphs_.erase(id);

    // Reload
    Graph* graph = load_graph(path_it->second);
    if (!graph) return false;

    // Re-attach to entities using this graph
    for (auto& [entity_id, comp] : entity_components_) {
        if (comp.graph_id == id) {
            comp.instance = std::make_unique<GraphInstance>(*graph, entity_id);

            // Rebuild event bindings
            comp.event_bindings.clear();
            for (EventNode* event : graph->get_event_nodes()) {
                comp.event_bindings[event->event_name()] = event;
            }
        }
    }

    return true;
}

void GraphSystem::enable_hot_reload(bool enabled) {
    hot_reload_enabled_ = enabled;
}

void GraphSystem::check_hot_reload() {
    for (auto& [id, path] : graph_paths_) {
        if (!std::filesystem::exists(path)) continue;

        auto current_time = std::filesystem::last_write_time(path);
        auto it = graph_timestamps_.find(id);

        if (it == graph_timestamps_.end() || current_time > it->second) {
            hot_reload(id);
            graph_timestamps_[id] = current_time;
        }
    }
}

GraphSystem::Stats GraphSystem::stats() const {
    Stats s;
    s.total_graphs = library_.all_graphs().size();
    s.compiled_graphs = compiled_graphs_.size();
    s.active_executions = get_active_executions().size();

    for (const Graph* graph : library_.all_graphs()) {
        s.total_nodes += graph->node_count();
    }

    auto exec_stats = executor_.stats();
    s.avg_execution_time_ms = exec_stats.average_execution_time_ms;

    return s;
}

} // namespace void_graph
