/// @file supervisor.cpp
/// @brief Erlang-style supervision implementation

#include <void_engine/kernel/supervisor.hpp>

#include <algorithm>
#include <future>
#include <queue>
#include <unordered_set>

namespace void_kernel {

// =============================================================================
// ChildHandle Implementation
// =============================================================================

std::chrono::nanoseconds ChildHandle::uptime() const {
    if (m_state.load() != ChildState::Running) {
        return std::chrono::nanoseconds{0};
    }
    return std::chrono::steady_clock::now() - m_last_start_time;
}

// =============================================================================
// Supervisor Implementation
// =============================================================================

Supervisor::Supervisor(SupervisorConfig config)
    : m_config(std::move(config)) {
}

Supervisor::~Supervisor() {
    stop();
}

void Supervisor::set_config(const SupervisorConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void_core::Result<void> Supervisor::add_child(ChildSpec spec) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for duplicate
    if (m_children.find(spec.name) != m_children.end()) {
        return void_core::Error{"Child already exists: " + spec.name};
    }

    auto child = std::make_unique<ChildHandle>();
    child->m_name = spec.name;
    child->m_spec = std::move(spec);
    child->m_state.store(ChildState::Stopped);

    // Insert in priority order
    auto insert_pos = std::lower_bound(
        m_child_order.begin(),
        m_child_order.end(),
        child->m_spec.priority,
        [this](const std::string& name, std::uint32_t priority) {
            auto it = m_children.find(name);
            return it != m_children.end() && it->second->m_spec.priority < priority;
        }
    );
    m_child_order.insert(insert_pos, child->m_name);
    m_children[child->m_name] = std::move(child);

    return void_core::Ok();
}

void_core::Result<void> Supervisor::add_child(
    const std::string& name,
    std::function<void()> start_fn,
    std::function<void()> stop_fn,
    RestartStrategy restart) {

    ChildSpec spec;
    spec.name = name;
    spec.start_fn = std::move(start_fn);
    spec.stop_fn = std::move(stop_fn);
    spec.restart = restart;

    return add_child(std::move(spec));
}

void_core::Result<void> Supervisor::remove_child(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_children.find(name);
    if (it == m_children.end()) {
        return void_core::Error{"Child not found: " + name};
    }

    // Stop if running
    if (it->second->m_state.load() == ChildState::Running) {
        stop_child_internal(*it->second);
    }

    // Remove from order
    auto order_it = std::find(m_child_order.begin(), m_child_order.end(), name);
    if (order_it != m_child_order.end()) {
        m_child_order.erase(order_it);
    }

    m_children.erase(it);
    return void_core::Ok();
}

ChildHandle* Supervisor::get_child(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_children.find(name);
    if (it != m_children.end()) {
        return it->second.get();
    }
    return nullptr;
}

const ChildHandle* Supervisor::get_child(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_children.find(name);
    if (it != m_children.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::string> Supervisor::child_names() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_child_order;
}

std::size_t Supervisor::child_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_children.size();
}

std::size_t Supervisor::running_child_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t count = 0;
    for (const auto& [name, child] : m_children) {
        if (child->m_state.load() == ChildState::Running) {
            count++;
        }
    }
    return count;
}

void_core::Result<void> Supervisor::start() {
    if (m_state.load() != SupervisorState::Stopped) {
        return void_core::Error{"Supervisor already running"};
    }

    m_state.store(SupervisorState::Starting);

    // Start children in order
    auto order = get_start_order();
    for (const auto& name : order) {
        auto result = start_child(name);
        if (!result) {
            // Handle based on restart strategy
            if (m_config.strategy == RestartStrategy::OneForAll) {
                stop();
                return result;
            }
        }
    }

    m_state.store(SupervisorState::Running);

    // Start monitoring thread
    m_monitor_running.store(true);
    m_monitor_thread = std::thread([this]() {
        while (m_monitor_running.load()) {
            check_children();

            std::unique_lock<std::mutex> lock(m_mutex);
            m_monitor_cv.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                return !m_monitor_running.load();
            });
        }
    });

    return void_core::Ok();
}

void Supervisor::stop() {
    if (m_state.load() == SupervisorState::Stopped) {
        return;
    }

    m_state.store(SupervisorState::Stopping);

    // Stop monitoring thread
    m_monitor_running.store(false);
    m_monitor_cv.notify_all();
    if (m_monitor_thread.joinable()) {
        m_monitor_thread.join();
    }

    // Stop children in reverse order
    auto order = get_stop_order();
    for (const auto& name : order) {
        stop_child(name);
    }

    m_state.store(SupervisorState::Stopped);
}

void_core::Result<void> Supervisor::restart() {
    stop();
    return start();
}

void_core::Result<void> Supervisor::start_child(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_children.find(name);
    if (it == m_children.end()) {
        return void_core::Error{"Child not found: " + name};
    }

    start_child_internal(*it->second);
    return void_core::Ok();
}

void_core::Result<void> Supervisor::stop_child(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_children.find(name);
    if (it == m_children.end()) {
        return void_core::Error{"Child not found: " + name};
    }

    stop_child_internal(*it->second);
    return void_core::Ok();
}

void_core::Result<void> Supervisor::restart_child(const std::string& name) {
    auto stop_result = stop_child(name);
    if (!stop_result) {
        return stop_result;
    }
    return start_child(name);
}

void_core::Result<void> Supervisor::terminate_child(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_children.find(name);
    if (it == m_children.end()) {
        return void_core::Error{"Child not found: " + name};
    }

    stop_child_internal(*it->second);
    it->second->m_state.store(ChildState::Terminated);

    emit_event(ChildEvent{
        .supervisor_name = m_config.name,
        .child_name = name,
        .type = ChildEventType::TerminatedNormally,
    });

    return void_core::Ok();
}

void Supervisor::start_child_internal(ChildHandle& child) {
    if (child.m_state.load() == ChildState::Running) {
        return;
    }

    child.m_state.store(ChildState::Starting);
    child.m_last_start_time = std::chrono::steady_clock::now();
    child.m_should_stop.store(false);

    // Start in thread
    if (child.m_spec.start_fn) {
        child.m_thread = std::thread([this, &child]() {
            try {
                child.m_state.store(ChildState::Running);

                emit_event(ChildEvent{
                    .supervisor_name = m_config.name,
                    .child_name = child.m_name,
                    .type = ChildEventType::Started,
                });

                child.m_spec.start_fn();

                // Normal exit
                if (!child.m_should_stop.load()) {
                    child.m_state.store(ChildState::Terminated);
                    emit_event(ChildEvent{
                        .supervisor_name = m_config.name,
                        .child_name = child.m_name,
                        .type = ChildEventType::TerminatedNormally,
                    });
                }
            } catch (const std::exception& e) {
                child.m_last_error = e.what();
                child.m_last_failure_time = std::chrono::steady_clock::now();
                child.m_state.store(ChildState::Failed);
                report_failure(child.m_name, e.what());
            } catch (...) {
                child.m_last_error = "Unknown exception";
                child.m_last_failure_time = std::chrono::steady_clock::now();
                child.m_state.store(ChildState::Failed);
                report_failure(child.m_name, "Unknown exception");
            }
        });
    }
}

void Supervisor::stop_child_internal(ChildHandle& child) {
    if (child.m_state.load() == ChildState::Stopped ||
        child.m_state.load() == ChildState::Terminated) {
        return;
    }

    child.m_state.store(ChildState::Stopping);
    child.m_should_stop.store(true);

    // Call stop function if provided
    if (child.m_spec.stop_fn) {
        child.m_spec.stop_fn();
    }

    // Wait for thread to finish
    if (child.m_thread.joinable()) {
        // Wait with timeout
        auto future = std::async(std::launch::async, [&child]() {
            if (child.m_thread.joinable()) {
                child.m_thread.join();
            }
        });

        if (future.wait_for(child.m_spec.shutdown_timeout) == std::future_status::timeout) {
            // Thread didn't stop in time - detach it (not ideal, but prevents deadlock)
            child.m_thread.detach();
        }
    }

    child.m_state.store(ChildState::Stopped);

    emit_event(ChildEvent{
        .supervisor_name = m_config.name,
        .child_name = child.m_name,
        .type = ChildEventType::Stopped,
    });
}

void Supervisor::check_children() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [name, child] : m_children) {
        auto state = child->m_state.load();
        if (state == ChildState::Failed) {
            handle_child_failure(name);
        }
    }
}

void Supervisor::report_failure(const std::string& name, const std::string& error) {
    emit_event(ChildEvent{
        .supervisor_name = m_config.name,
        .child_name = name,
        .type = ChildEventType::Crashed,
        .error_message = error,
    });

    // Handle will be done in check_children
}

void Supervisor::handle_child_failure(const std::string& name) {
    auto it = m_children.find(name);
    if (it == m_children.end()) return;

    auto& child = *it->second;

    // Check restart strategy
    if (child.m_spec.restart == RestartStrategy::Temporary) {
        // Never restart
        child.m_state.store(ChildState::Terminated);
        return;
    }

    if (child.m_spec.restart == RestartStrategy::Transient) {
        // Only restart on abnormal termination (which this is)
        // Fall through to restart logic
    }

    // Track restart time
    auto now = std::chrono::steady_clock::now();
    m_restart_times.push_back(now);

    // Remove old restart times outside the window
    auto window_start = now - m_config.limits.time_window;
    m_restart_times.erase(
        std::remove_if(m_restart_times.begin(), m_restart_times.end(),
            [window_start](const auto& t) { return t < window_start; }),
        m_restart_times.end()
    );

    // Check restart limits
    if (!m_config.limits.allows_restart(static_cast<std::uint32_t>(m_restart_times.size()))) {
        m_state.store(SupervisorState::Failed);
        if (m_on_max_restarts) {
            m_on_max_restarts();
        }
        return;
    }

    // Apply restart strategy
    apply_restart_strategy(name);
}

void Supervisor::apply_restart_strategy(const std::string& failed_child) {
    switch (m_config.strategy) {
        case RestartStrategy::OneForOne: {
            // Only restart the failed child
            auto it = m_children.find(failed_child);
            if (it != m_children.end()) {
                auto& child = *it->second;
                child.m_restart_count++;
                m_total_restarts++;

                auto delay = calculate_restart_delay(child.m_restart_count);
                std::this_thread::sleep_for(delay);

                child.m_state.store(ChildState::Restarting);
                start_child_internal(child);

                emit_event(ChildEvent{
                    .supervisor_name = m_config.name,
                    .child_name = failed_child,
                    .type = ChildEventType::Restarted,
                    .restart_count = child.m_restart_count,
                });
            }
            break;
        }

        case RestartStrategy::OneForAll: {
            // Stop all children, then restart all
            auto order = get_stop_order();
            for (const auto& name : order) {
                auto it = m_children.find(name);
                if (it != m_children.end()) {
                    stop_child_internal(*it->second);
                }
            }

            m_total_restarts++;

            auto delay = calculate_restart_delay(m_total_restarts);
            std::this_thread::sleep_for(delay);

            auto start_order = get_start_order();
            for (const auto& name : start_order) {
                auto it = m_children.find(name);
                if (it != m_children.end()) {
                    it->second->m_restart_count++;
                    start_child_internal(*it->second);

                    emit_event(ChildEvent{
                        .supervisor_name = m_config.name,
                        .child_name = name,
                        .type = ChildEventType::Restarted,
                        .restart_count = it->second->m_restart_count,
                    });
                }
            }
            break;
        }

        case RestartStrategy::RestForOne: {
            // Stop the failed child and all children started after it, then restart
            bool found = false;
            std::vector<std::string> to_restart;

            for (const auto& name : m_child_order) {
                if (name == failed_child) {
                    found = true;
                }
                if (found) {
                    to_restart.push_back(name);
                }
            }

            // Stop in reverse order
            for (auto it = to_restart.rbegin(); it != to_restart.rend(); ++it) {
                auto child_it = m_children.find(*it);
                if (child_it != m_children.end()) {
                    stop_child_internal(*child_it->second);
                }
            }

            m_total_restarts++;

            auto delay = calculate_restart_delay(m_total_restarts);
            std::this_thread::sleep_for(delay);

            // Restart in order
            for (const auto& name : to_restart) {
                auto it = m_children.find(name);
                if (it != m_children.end()) {
                    it->second->m_restart_count++;
                    start_child_internal(*it->second);

                    emit_event(ChildEvent{
                        .supervisor_name = m_config.name,
                        .child_name = name,
                        .type = ChildEventType::Restarted,
                        .restart_count = it->second->m_restart_count,
                    });
                }
            }
            break;
        }

        case RestartStrategy::Temporary:
        case RestartStrategy::Transient:
            // Already handled above
            break;
    }
}

std::chrono::milliseconds Supervisor::calculate_restart_delay(std::uint32_t restart_count) const {
    auto delay = m_config.base_restart_delay;
    for (std::uint32_t i = 1; i < restart_count && delay < m_config.max_restart_delay; ++i) {
        delay = std::chrono::milliseconds(
            static_cast<long long>(delay.count() * m_config.restart_delay_multiplier)
        );
    }
    return std::min(delay, m_config.max_restart_delay);
}

std::vector<std::string> Supervisor::get_start_order() const {
    // Already in priority order
    return m_child_order;
}

std::vector<std::string> Supervisor::get_stop_order() const {
    // Reverse of start order
    std::vector<std::string> order = m_child_order;
    std::reverse(order.begin(), order.end());
    return order;
}

void Supervisor::emit_event(const ChildEvent& event) {
    if (m_event_callback) {
        m_event_callback(event);
    }
}

std::uint32_t Supervisor::get_restart_count(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_children.find(name);
    if (it != m_children.end()) {
        return it->second->m_restart_count;
    }
    return 0;
}

std::uint32_t Supervisor::total_restart_count() const {
    return m_total_restarts;
}

bool Supervisor::restart_limits_exceeded() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();
    auto window_start = now - m_config.limits.time_window;

    std::size_t count = 0;
    for (const auto& t : m_restart_times) {
        if (t >= window_start) count++;
    }

    return !m_config.limits.allows_restart(static_cast<std::uint32_t>(count));
}

void Supervisor::set_event_callback(ChildEventCallback callback) {
    m_event_callback = std::move(callback);
}

void Supervisor::set_on_max_restarts(std::function<void()> callback) {
    m_on_max_restarts = std::move(callback);
}

// =============================================================================
// SupervisorTree Implementation
// =============================================================================

SupervisorTree::SupervisorTree() = default;
SupervisorTree::~SupervisorTree() {
    stop();
}

void_core::Result<Supervisor*> SupervisorTree::create_root(SupervisorConfig config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_root_name.empty()) {
        return void_core::Error{"Root supervisor already exists"};
    }

    auto supervisor = std::make_unique<Supervisor>(std::move(config));
    Supervisor* ptr = supervisor.get();
    std::string name = supervisor->name();

    m_supervisors[name] = SupervisorNode{
        .supervisor = std::move(supervisor),
        .parent_name = "",
        .children = {},
    };
    m_root_name = name;

    return ptr;
}

void_core::Result<Supervisor*> SupervisorTree::create_supervisor(
    const std::string& parent_name,
    SupervisorConfig config) {

    std::lock_guard<std::mutex> lock(m_mutex);

    auto parent_it = m_supervisors.find(parent_name);
    if (parent_it == m_supervisors.end()) {
        return void_core::Error{"Parent supervisor not found: " + parent_name};
    }

    auto supervisor = std::make_unique<Supervisor>(std::move(config));
    Supervisor* ptr = supervisor.get();
    std::string name = supervisor->name();

    if (m_supervisors.find(name) != m_supervisors.end()) {
        return void_core::Error{"Supervisor already exists: " + name};
    }

    parent_it->second.children.push_back(name);

    m_supervisors[name] = SupervisorNode{
        .supervisor = std::move(supervisor),
        .parent_name = parent_name,
        .children = {},
    };

    return ptr;
}

Supervisor* SupervisorTree::get_supervisor(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_supervisors.find(name);
    if (it != m_supervisors.end()) {
        return it->second.supervisor.get();
    }
    return nullptr;
}

const Supervisor* SupervisorTree::get_supervisor(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_supervisors.find(name);
    if (it != m_supervisors.end()) {
        return it->second.supervisor.get();
    }
    return nullptr;
}

Supervisor* SupervisorTree::root() {
    return get_supervisor(m_root_name);
}

const Supervisor* SupervisorTree::root() const {
    return get_supervisor(m_root_name);
}

void_core::Result<void> SupervisorTree::remove_supervisor(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_supervisors.find(name);
    if (it == m_supervisors.end()) {
        return void_core::Error{"Supervisor not found: " + name};
    }

    // Stop the supervisor
    it->second.supervisor->stop();

    // Remove from parent's children list
    if (!it->second.parent_name.empty()) {
        auto parent_it = m_supervisors.find(it->second.parent_name);
        if (parent_it != m_supervisors.end()) {
            auto& children = parent_it->second.children;
            children.erase(
                std::remove(children.begin(), children.end(), name),
                children.end()
            );
        }
    }

    // Recursively remove children
    std::vector<std::string> children_to_remove = it->second.children;
    for (const auto& child : children_to_remove) {
        remove_supervisor(child);
    }

    // Remove this supervisor
    if (name == m_root_name) {
        m_root_name.clear();
    }
    m_supervisors.erase(it);

    return void_core::Ok();
}

std::vector<std::string> SupervisorTree::supervisor_names() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_supervisors.size());
    for (const auto& [name, node] : m_supervisors) {
        names.push_back(name);
    }
    return names;
}

void_core::Result<void> SupervisorTree::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_root_name.empty()) {
        return void_core::Error{"No root supervisor"};
    }

    // Start in BFS order (root first, then children)
    std::queue<std::string> queue;
    queue.push(m_root_name);

    while (!queue.empty()) {
        std::string name = queue.front();
        queue.pop();

        auto it = m_supervisors.find(name);
        if (it != m_supervisors.end()) {
            auto result = it->second.supervisor->start();
            if (!result) {
                return result;
            }

            for (const auto& child : it->second.children) {
                queue.push(child);
            }
        }
    }

    return void_core::Ok();
}

void SupervisorTree::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_root_name.empty()) return;

    // Stop in reverse BFS order (leaves first)
    std::vector<std::string> stop_order;
    std::queue<std::string> queue;
    queue.push(m_root_name);

    while (!queue.empty()) {
        std::string name = queue.front();
        queue.pop();
        stop_order.push_back(name);

        auto it = m_supervisors.find(name);
        if (it != m_supervisors.end()) {
            for (const auto& child : it->second.children) {
                queue.push(child);
            }
        }
    }

    // Reverse to stop leaves first
    std::reverse(stop_order.begin(), stop_order.end());

    for (const auto& name : stop_order) {
        auto it = m_supervisors.find(name);
        if (it != m_supervisors.end()) {
            it->second.supervisor->stop();
        }
    }
}

void SupervisorTree::check_all() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [name, node] : m_supervisors) {
        node.supervisor->check_children();
    }
}

std::size_t SupervisorTree::total_child_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t total = 0;
    for (const auto& [name, node] : m_supervisors) {
        total += node.supervisor->child_count();
    }
    return total;
}

std::size_t SupervisorTree::total_running_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t total = 0;
    for (const auto& [name, node] : m_supervisors) {
        total += node.supervisor->running_child_count();
    }
    return total;
}

std::uint32_t SupervisorTree::total_restart_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::uint32_t total = 0;
    for (const auto& [name, node] : m_supervisors) {
        total += node.supervisor->total_restart_count();
    }
    return total;
}

} // namespace void_kernel
