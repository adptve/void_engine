#include "execution.hpp"

#include <algorithm>

namespace void_graph {

// =============================================================================
// DefaultNodeExecutor Implementation
// =============================================================================

PinId DefaultNodeExecutor::execute(INode& node, ExecutionContext& ctx) {
    return node.execute(ctx);
}

// =============================================================================
// GraphExecutor Implementation
// =============================================================================

GraphExecutor::GraphExecutor()
    : default_executor_(std::make_unique<DefaultNodeExecutor>()) {}

GraphExecutor::~GraphExecutor() = default;

ExecutionId GraphExecutor::start(GraphInstance& instance, EventNode& event) {
    return start(instance, event.id());
}

ExecutionId GraphExecutor::start(GraphInstance& instance, NodeId start_node) {
    ExecutionId id = ExecutionId::from_bits(next_execution_id_++);

    ExecutionData data;
    data.id = id;
    data.instance = &instance;
    data.state = ExecutionState::Running;
    data.started_at = std::chrono::steady_clock::now();

    // Initialize first frame
    ExecutionFrame frame;
    frame.node_id = start_node;
    frame.started_at = data.started_at;
    data.stack.push_back(frame);

    executions_[id] = std::move(data);

    // Run execution
    run_execution(executions_[id]);

    return id;
}

void GraphExecutor::update(float delta_time) {
    // Update latent actions
    update_latent_actions(delta_time);

    // Update all running executions
    for (auto& [id, data] : executions_) {
        if (data.state == ExecutionState::Running ||
            data.state == ExecutionState::Suspended) {
            data.instance->context().delta_time = delta_time;
            data.instance->context().total_time += delta_time;
            ++data.instance->context().frame_count;

            if (data.state == ExecutionState::Running) {
                run_execution(data);
            }
        }
    }

    // Clean up completed executions
    std::erase_if(executions_, [](const auto& pair) {
        return pair.second.state == ExecutionState::Completed ||
               pair.second.state == ExecutionState::Aborted ||
               pair.second.state == ExecutionState::Error;
    });
}

void GraphExecutor::pause(ExecutionId id) {
    auto it = executions_.find(id);
    if (it != executions_.end() && it->second.state == ExecutionState::Running) {
        it->second.state = ExecutionState::Paused;
    }
}

void GraphExecutor::resume(ExecutionId id) {
    auto it = executions_.find(id);
    if (it != executions_.end() && it->second.state == ExecutionState::Paused) {
        it->second.state = ExecutionState::Running;
        run_execution(it->second);
    }
}

void GraphExecutor::abort(ExecutionId id) {
    auto it = executions_.find(id);
    if (it != executions_.end()) {
        it->second.state = ExecutionState::Aborted;
    }
}

bool GraphExecutor::is_running(ExecutionId id) const {
    auto it = executions_.find(id);
    return it != executions_.end() &&
           (it->second.state == ExecutionState::Running ||
            it->second.state == ExecutionState::Suspended);
}

ExecutionState GraphExecutor::get_state(ExecutionId id) const {
    auto it = executions_.find(id);
    if (it == executions_.end()) return ExecutionState::Idle;
    return it->second.state;
}

const ExecutionResult* GraphExecutor::get_result(ExecutionId id) const {
    auto it = executions_.find(id);
    if (it == executions_.end()) return nullptr;
    return &it->second.result;
}

void GraphExecutor::run_execution(ExecutionData& data) {
    const std::size_t max_iterations = 10000;  // Prevent infinite loops
    std::size_t iterations = 0;

    while (!data.stack.empty() && data.state == ExecutionState::Running) {
        if (++iterations > max_iterations) {
            data.state = ExecutionState::Error;
            data.result.error_message = "Max iterations exceeded";
            break;
        }

        ExecutionFrame& frame = data.stack.back();

        // Get the node
        INode* node = data.instance->context().graph.is_valid()
            ? const_cast<Graph&>(data.instance->graph()).get_node(frame.node_id)
            : nullptr;

        if (!node) {
            data.stack.pop_back();
            continue;
        }

        // Check breakpoint
        if (check_breakpoint(data, frame.node_id)) {
            data.state = ExecutionState::Paused;
            break;
        }

        // Execute or resume
        PinId next_exec;
        if (frame.is_resuming) {
            next_exec = node->resume(data.instance->context());
            frame.is_resuming = false;
        } else {
            next_exec = execute_node(data, *node);
        }

        // Handle result
        if (!next_exec.is_valid()) {
            // No more execution from this node
            if (node->state() == NodeState::Suspended) {
                // Latent node - wait
                data.state = ExecutionState::Suspended;
                frame.is_resuming = true;
                break;
            }

            // Pop this node and continue
            data.stack.pop_back();
        } else {
            // Follow the exec pin to next node
            const Graph& graph = data.instance->graph();
            std::vector<PinId> connected = graph.get_connected_inputs(next_exec);

            if (connected.empty()) {
                data.stack.pop_back();
            } else {
                // Find the node that owns the connected pin
                for (const auto& node_ptr : graph.nodes()) {
                    if (node_ptr->find_pin(connected[0])) {
                        frame.node_id = node_ptr->id();
                        frame.is_resuming = false;
                        break;
                    }
                }
            }
        }
    }

    if (data.stack.empty()) {
        data.state = ExecutionState::Completed;
        data.result.final_state = ExecutionState::Completed;

        auto end_time = std::chrono::steady_clock::now();
        data.result.execution_time_ms = std::chrono::duration<float, std::milli>(
            end_time - data.started_at).count();
    }
}

PinId GraphExecutor::execute_node(ExecutionData& data, INode& node) {
    node.set_state(NodeState::Executing);

    // Get executor
    INodeExecutor* executor = default_executor_.get();
    auto it = custom_executors_.find(node.type_id());
    if (it != custom_executors_.end()) {
        executor = it->second.get();
    }

    // Pre-execute
    executor->pre_execute(node, data.instance->context());

    // Pull input values for non-exec pins
    [[maybe_unused]] const Graph& graph = data.instance->graph();
    for (const auto& pin : node.input_pins()) {
        if (pin.type != PinType::Exec && pin.is_connected) {
            PinValue value = compute_input_value(data.instance->context(), pin);
            data.instance->context().pin_values[pin.id] = std::move(value);
        }
    }

    // Execute
    PinId result = executor->execute(node, data.instance->context());

    // Post-execute
    executor->post_execute(node, data.instance->context());

    if (node.state() != NodeState::Suspended) {
        node.set_state(NodeState::Completed);
    }

    ++data.result.nodes_executed;
    ++stats_.total_nodes_executed;

    return result;
}

PinValue GraphExecutor::compute_input_value([[maybe_unused]] ExecutionContext& ctx, const Pin& input_pin) {
    // This would need access to the graph to find connected output
    // For now, return default value
    return input_pin.default_value.value;
}

void GraphExecutor::set_node_executor(std::unique_ptr<INodeExecutor> executor) {
    default_executor_ = std::move(executor);
}

void GraphExecutor::set_node_executor(NodeTypeId type_id, std::unique_ptr<INodeExecutor> executor) {
    custom_executors_[type_id] = std::move(executor);
}

void GraphExecutor::add_breakpoint(GraphId graph, NodeId node) {
    breakpoints_[graph].insert(node);
}

void GraphExecutor::remove_breakpoint(GraphId graph, NodeId node) {
    auto it = breakpoints_.find(graph);
    if (it != breakpoints_.end()) {
        it->second.erase(node);
    }
}

void GraphExecutor::clear_breakpoints() {
    breakpoints_.clear();
}

void GraphExecutor::step_into(ExecutionId id) {
    auto it = executions_.find(id);
    if (it != executions_.end() && it->second.state == ExecutionState::Paused) {
        it->second.state = ExecutionState::Running;
        // Will pause at next node due to debug mode
    }
}

void GraphExecutor::step_over(ExecutionId id) {
    // Similar to step_into but skip subgraphs
    step_into(id);
}

void GraphExecutor::step_out(ExecutionId id) {
    // Continue until returning from current stack level
    step_into(id);
}

void GraphExecutor::set_breakpoint_callback(BreakpointCallback callback) {
    breakpoint_callback_ = std::move(callback);
}

bool GraphExecutor::check_breakpoint(ExecutionData& data, NodeId node) {
    if (!debug_enabled_) return false;

    auto it = breakpoints_.find(data.instance->graph().id());
    if (it == breakpoints_.end()) return false;

    if (it->second.count(node)) {
        if (breakpoint_callback_) {
            breakpoint_callback_(data.id, node);
        }
        return true;
    }

    return false;
}

GraphExecutor::ExecutionStats GraphExecutor::stats() const {
    ExecutionStats s = stats_;
    s.active_executions = executions_.size();
    s.latent_actions = latent_actions_.size();
    return s;
}

void GraphExecutor::register_latent_action(ExecutionContext& ctx, float duration,
                                            std::function<void()> on_complete) {
    LatentAction action;
    action.execution_id = ctx.id;
    action.node_id = ctx.current_node;
    action.remaining_time = duration;
    action.on_complete = std::move(on_complete);
    action.started_at = std::chrono::steady_clock::now();

    latent_actions_.push_back(std::move(action));
}

void GraphExecutor::register_latent_action(ExecutionContext& ctx,
                                            std::function<bool()> completion_predicate,
                                            std::function<void()> on_complete) {
    LatentAction action;
    action.execution_id = ctx.id;
    action.node_id = ctx.current_node;
    action.completion_predicate = std::move(completion_predicate);
    action.on_complete = std::move(on_complete);
    action.started_at = std::chrono::steady_clock::now();

    latent_actions_.push_back(std::move(action));
}

void GraphExecutor::update_latent_actions(float delta_time) {
    auto it = latent_actions_.begin();
    while (it != latent_actions_.end()) {
        bool complete = false;

        if (it->completion_predicate) {
            complete = it->completion_predicate();
        } else {
            it->remaining_time -= delta_time;
            complete = it->remaining_time <= 0.0f;
        }

        if (complete) {
            if (it->on_complete) {
                it->on_complete();
            }

            // Resume the execution
            auto exec_it = executions_.find(it->execution_id);
            if (exec_it != executions_.end() &&
                exec_it->second.state == ExecutionState::Suspended) {
                exec_it->second.state = ExecutionState::Running;
            }

            it = latent_actions_.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// CompiledGraph Implementation
// =============================================================================

std::optional<std::size_t> CompiledGraph::get_entry_point(const std::string& event_name) const {
    auto it = entry_points_.find(event_name);
    if (it == entry_points_.end()) return std::nullopt;
    return it->second;
}

const PinValue& CompiledGraph::get_constant(std::size_t index) const {
    static const PinValue empty;
    if (index >= constants_.size()) return empty;
    return constants_[index];
}

// =============================================================================
// GraphCompiler Implementation
// =============================================================================

GraphCompiler::GraphCompiler() = default;

GraphCompiler::GraphCompiler(const Options& options) : options_(options) {}

GraphResult<CompiledGraph> GraphCompiler::compile(const Graph& graph) {
    std::vector<std::string> event_names;
    for (const auto& event : graph.get_event_nodes()) {
        event_names.push_back(event->event_name());
    }
    return compile(graph, event_names);
}

GraphResult<CompiledGraph> GraphCompiler::compile(const Graph& graph,
                                                    [[maybe_unused]] std::span<const std::string> events) {
    errors_.clear();
    warnings_.clear();
    next_register_ = 0;
    node_addresses_.clear();

    CompiledGraph output;
    output.source_graph_ = graph.id();

    // Compile each event
    for (const auto& event_node : graph.get_event_nodes()) {
        compile_event(graph, *event_node, output);
    }

    output.register_count_ = next_register_;

    // Optimization passes
    if (options_.optimization != OptimizationLevel::Debug) {
        optimize(output);
    }

    output.is_valid_ = errors_.empty();
    if (!errors_.empty()) {
        output.validation_error_ = errors_[0];
    }

    return GraphResult<CompiledGraph>(std::move(output));
}

void GraphCompiler::compile_node([[maybe_unused]] const Graph& graph, INode& node, CompiledGraph& output) {
    node_addresses_[node.id()] = output.instructions_.size();

    CompiledInstruction instr;
    instr.op = CompiledInstruction::OpCode::Execute;
    instr.arg1 = node.id().to_bits();
    emit(output, instr);
}

void GraphCompiler::compile_event(const Graph& graph, EventNode& event, CompiledGraph& output) {
    output.entry_points_[event.event_name()] = output.instructions_.size();
    compile_node(graph, event, output);

    // Follow execution flow
    // This is simplified - full implementation would traverse the graph
}

std::uint32_t GraphCompiler::allocate_register() {
    return next_register_++;
}

void GraphCompiler::emit(CompiledGraph& output, CompiledInstruction instr) {
    output.instructions_.push_back(std::move(instr));
}

void GraphCompiler::optimize(CompiledGraph& output) {
    if (options_.fold_constants) {
        fold_constants(output);
    }
    if (options_.eliminate_dead_code) {
        eliminate_dead_code(output);
    }
}

void GraphCompiler::fold_constants([[maybe_unused]] CompiledGraph& output) {
    // Identify sequences of constant operations and fold them
    // This is a placeholder for the actual implementation
}

void GraphCompiler::eliminate_dead_code([[maybe_unused]] CompiledGraph& output) {
    // Remove unreachable instructions
    // This is a placeholder for the actual implementation
}

// =============================================================================
// CompiledGraphExecutor Implementation
// =============================================================================

CompiledGraphExecutor::CompiledGraphExecutor() = default;

ExecutionResult CompiledGraphExecutor::execute(const CompiledGraph& graph,
                                                const std::string& entry_point,
                                                ExecutionContext& ctx) {
    ExecutionResult result;

    auto entry = graph.get_entry_point(entry_point);
    if (!entry) {
        result.final_state = ExecutionState::Error;
        result.error_message = "Entry point not found: " + entry_point;
        return result;
    }

    std::size_t ip = *entry;
    registers_.resize(graph.register_count());

    auto start_time = std::chrono::steady_clock::now();

    while (ip < graph.instructions().size()) {
        if (!execute_instruction(graph.instructions()[ip], ctx, ip)) {
            break;
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    result.execution_time_ms = std::chrono::duration<float, std::milli>(
        end_time - start_time).count();

    result.final_state = ExecutionState::Completed;
    return result;
}

void CompiledGraphExecutor::update(float delta_time) {
    auto it = latent_actions_.begin();
    while (it != latent_actions_.end()) {
        it->remaining_time -= delta_time;
        if (it->remaining_time <= 0.0f) {
            if (it->on_complete) {
                it->on_complete();
            }
            it = latent_actions_.erase(it);
        } else {
            ++it;
        }
    }
}

bool CompiledGraphExecutor::execute_instruction(const CompiledInstruction& instr,
                                                  [[maybe_unused]] ExecutionContext& ctx,
                                                  std::size_t& ip) {
    switch (instr.op) {
        case CompiledInstruction::OpCode::Nop:
            ++ip;
            break;

        case CompiledInstruction::OpCode::Jump:
            ip = instr.arg1;
            break;

        case CompiledInstruction::OpCode::JumpIf:
            if (std::holds_alternative<bool>(registers_[instr.arg1]) &&
                std::get<bool>(registers_[instr.arg1])) {
                ip = instr.arg2;
            } else {
                ++ip;
            }
            break;

        case CompiledInstruction::OpCode::JumpIfNot:
            if (std::holds_alternative<bool>(registers_[instr.arg1]) &&
                !std::get<bool>(registers_[instr.arg1])) {
                ip = instr.arg2;
            } else {
                ++ip;
            }
            break;

        case CompiledInstruction::OpCode::Return:
            return false;

        case CompiledInstruction::OpCode::LoadConst:
            registers_[instr.arg1] = instr.immediate;
            ++ip;
            break;

        case CompiledInstruction::OpCode::Copy:
            registers_[instr.arg1] = registers_[instr.arg2];
            ++ip;
            break;

        case CompiledInstruction::OpCode::Add:
            if (std::holds_alternative<float>(registers_[instr.arg2]) &&
                std::holds_alternative<float>(registers_[instr.arg3])) {
                registers_[instr.arg1] = std::get<float>(registers_[instr.arg2]) +
                                          std::get<float>(registers_[instr.arg3]);
            }
            ++ip;
            break;

        case CompiledInstruction::OpCode::Sub:
            if (std::holds_alternative<float>(registers_[instr.arg2]) &&
                std::holds_alternative<float>(registers_[instr.arg3])) {
                registers_[instr.arg1] = std::get<float>(registers_[instr.arg2]) -
                                          std::get<float>(registers_[instr.arg3]);
            }
            ++ip;
            break;

        case CompiledInstruction::OpCode::Mul:
            if (std::holds_alternative<float>(registers_[instr.arg2]) &&
                std::holds_alternative<float>(registers_[instr.arg3])) {
                registers_[instr.arg1] = std::get<float>(registers_[instr.arg2]) *
                                          std::get<float>(registers_[instr.arg3]);
            }
            ++ip;
            break;

        case CompiledInstruction::OpCode::Div:
            if (std::holds_alternative<float>(registers_[instr.arg2]) &&
                std::holds_alternative<float>(registers_[instr.arg3])) {
                float divisor = std::get<float>(registers_[instr.arg3]);
                if (divisor != 0.0f) {
                    registers_[instr.arg1] = std::get<float>(registers_[instr.arg2]) / divisor;
                }
            }
            ++ip;
            break;

        case CompiledInstruction::OpCode::Eq: {
            // Handle equality comparison by checking types first
            // std::any is not comparable, so we need type-specific comparison
            const auto& lhs = registers_[instr.arg2];
            const auto& rhs = registers_[instr.arg3];
            bool result = false;

            if (lhs.index() == rhs.index()) {
                // Same type index - compare based on type
                if (std::holds_alternative<bool>(lhs)) {
                    result = std::get<bool>(lhs) == std::get<bool>(rhs);
                } else if (std::holds_alternative<std::int32_t>(lhs)) {
                    result = std::get<std::int32_t>(lhs) == std::get<std::int32_t>(rhs);
                } else if (std::holds_alternative<std::int64_t>(lhs)) {
                    result = std::get<std::int64_t>(lhs) == std::get<std::int64_t>(rhs);
                } else if (std::holds_alternative<float>(lhs)) {
                    result = std::get<float>(lhs) == std::get<float>(rhs);
                } else if (std::holds_alternative<double>(lhs)) {
                    result = std::get<double>(lhs) == std::get<double>(rhs);
                } else if (std::holds_alternative<std::string>(lhs)) {
                    result = std::get<std::string>(lhs) == std::get<std::string>(rhs);
                } else if (std::holds_alternative<std::uint64_t>(lhs)) {
                    result = std::get<std::uint64_t>(lhs) == std::get<std::uint64_t>(rhs);
                } else if (std::holds_alternative<std::monostate>(lhs)) {
                    result = true;  // Both are monostate
                }
                // For arrays, vectors, and std::any - we can't easily compare, treat as not equal
            }
            registers_[instr.arg1] = result;
            ++ip;
            break;
        }

        case CompiledInstruction::OpCode::Lt:
            if (std::holds_alternative<float>(registers_[instr.arg2]) &&
                std::holds_alternative<float>(registers_[instr.arg3])) {
                registers_[instr.arg1] = std::get<float>(registers_[instr.arg2]) <
                                          std::get<float>(registers_[instr.arg3]);
            }
            ++ip;
            break;

        case CompiledInstruction::OpCode::Suspend:
            return false;  // Suspend execution

        case CompiledInstruction::OpCode::Breakpoint:
            if (debug_enabled_) {
                return false;  // Pause at breakpoint
            }
            ++ip;
            break;

        default:
            ++ip;
            break;
    }

    return true;
}

} // namespace void_graph
