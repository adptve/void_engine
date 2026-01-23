/// @file lifecycle.cpp
/// @brief Lifecycle management implementation for void_engine

#include <void_engine/engine/lifecycle.hpp>
#include <void_engine/engine/engine.hpp>

#include <algorithm>

namespace void_engine {

// =============================================================================
// LifecycleManager
// =============================================================================

void LifecycleManager::register_hook(LifecycleHook hook) {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    m_hooks[hook.name] = std::move(hook);
}

void LifecycleManager::register_hooks(std::vector<LifecycleHook> hooks) {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    for (auto& hook : hooks) {
        m_hooks[hook.name] = std::move(hook);
    }
}

bool LifecycleManager::unregister_hook(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    return m_hooks.erase(name) > 0;
}

void LifecycleManager::set_hook_enabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    auto it = m_hooks.find(name);
    if (it != m_hooks.end()) {
        it->second.enabled = enabled;
    }
}

bool LifecycleManager::has_hook(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    return m_hooks.find(name) != m_hooks.end();
}

void LifecycleManager::on_init(const std::string& name, LifecycleCallback callback, HookPriority priority) {
    register_hook(LifecycleHook::create(name, LifecyclePhase::CoreInit, std::move(callback), priority));
}

void LifecycleManager::on_ready(const std::string& name, LifecycleCallback callback, HookPriority priority) {
    register_hook(LifecycleHook::create(name, LifecyclePhase::Ready, std::move(callback), priority));
}

void LifecycleManager::on_shutdown(const std::string& name, LifecycleCallback callback, HookPriority priority) {
    register_hook(LifecycleHook::create(name, LifecyclePhase::CoreShutdown, std::move(callback), priority));
}

void LifecycleManager::on_pre_update(const std::string& name, LifecycleCallback callback, HookPriority priority) {
    // Special handling for pre-update hooks
    auto hook = LifecycleHook::create(name, LifecyclePhase::Running, std::move(callback), priority);

    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    m_hooks[name] = std::move(hook);
    m_pre_update_hooks.push_back(name);

    // Sort by priority
    std::sort(m_pre_update_hooks.begin(), m_pre_update_hooks.end(),
        [this](const std::string& a, const std::string& b) {
            auto it_a = m_hooks.find(a);
            auto it_b = m_hooks.find(b);
            if (it_a == m_hooks.end() || it_b == m_hooks.end()) return false;
            return static_cast<int>(it_a->second.priority) < static_cast<int>(it_b->second.priority);
        });
}

void LifecycleManager::on_post_update(const std::string& name, LifecycleCallback callback, HookPriority priority) {
    // Special handling for post-update hooks
    auto hook = LifecycleHook::create(name, LifecyclePhase::Running, std::move(callback), priority);

    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    m_hooks[name] = std::move(hook);
    m_post_update_hooks.push_back(name);

    // Sort by priority
    std::sort(m_post_update_hooks.begin(), m_post_update_hooks.end(),
        [this](const std::string& a, const std::string& b) {
            auto it_a = m_hooks.find(a);
            auto it_b = m_hooks.find(b);
            if (it_a == m_hooks.end() || it_b == m_hooks.end()) return false;
            return static_cast<int>(it_a->second.priority) < static_cast<int>(it_b->second.priority);
        });
}

void_core::Result<void> LifecycleManager::transition_to(LifecyclePhase phase, Engine& engine) {
    LifecyclePhase old_phase = m_current_phase;

    // Record timing
    auto now = std::chrono::steady_clock::now();
    if (old_phase != LifecyclePhase::PreInit) {
        auto duration = now - m_phase_start_time;
        m_phase_durations[old_phase] = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    }
    m_phase_start_time = now;

    // Execute hooks for new phase
    auto result = execute_phase(phase, engine);
    if (!result) {
        return result;
    }

    // Update current phase
    m_current_phase = phase;

    // Record transition
    record_transition(old_phase, phase);

    return void_core::Ok();
}

void_core::Result<void> LifecycleManager::execute_current_phase(Engine& engine) {
    return execute_phase(m_current_phase, engine);
}

void_core::Result<void> LifecycleManager::execute_phase(LifecyclePhase phase, Engine& engine) {
    auto hooks = get_hooks_for_phase(phase);

    std::vector<std::string> to_remove;

    for (auto* hook : hooks) {
        if (!hook->enabled) continue;

        auto result = hook->callback(engine);
        if (!result) {
            return void_core::Error{"Lifecycle hook '" + hook->name + "' failed: " + result.error().message()};
        }

        if (hook->once) {
            to_remove.push_back(hook->name);
        }
    }

    // Remove one-shot hooks
    {
        std::lock_guard<std::mutex> lock(m_hooks_mutex);
        for (const auto& name : to_remove) {
            m_hooks.erase(name);
        }
    }

    return void_core::Ok();
}

void_core::Result<void> LifecycleManager::pre_update(Engine& engine) {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);

    for (const auto& name : m_pre_update_hooks) {
        auto it = m_hooks.find(name);
        if (it == m_hooks.end() || !it->second.enabled) continue;

        auto result = it->second.callback(engine);
        if (!result) {
            return result;
        }
    }

    return void_core::Ok();
}

void_core::Result<void> LifecycleManager::post_update(Engine& engine) {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);

    for (const auto& name : m_post_update_hooks) {
        auto it = m_hooks.find(name);
        if (it == m_hooks.end() || !it->second.enabled) continue;

        auto result = it->second.callback(engine);
        if (!result) {
            return result;
        }
    }

    return void_core::Ok();
}

void LifecycleManager::set_on_phase_change(std::function<void(const LifecycleEvent&)> callback) {
    m_on_phase_change = std::move(callback);
}

std::chrono::nanoseconds LifecycleManager::phase_duration(LifecyclePhase phase) const {
    auto it = m_phase_durations.find(phase);
    if (it != m_phase_durations.end()) {
        return it->second;
    }
    return std::chrono::nanoseconds{0};
}

std::chrono::nanoseconds LifecycleManager::total_init_time() const {
    std::chrono::nanoseconds total{0};

    // Sum initialization phases
    total += phase_duration(LifecyclePhase::PreInit);
    total += phase_duration(LifecyclePhase::CoreInit);
    total += phase_duration(LifecyclePhase::SubsystemInit);
    total += phase_duration(LifecyclePhase::AppInit);
    total += phase_duration(LifecyclePhase::Ready);

    return total;
}

std::size_t LifecycleManager::hook_count() const {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    return m_hooks.size();
}

std::size_t LifecycleManager::hook_count(LifecyclePhase phase) const {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);

    std::size_t count = 0;
    for (const auto& [name, hook] : m_hooks) {
        if (hook.phase == phase) {
            count++;
        }
    }
    return count;
}

std::vector<LifecycleHook*> LifecycleManager::get_hooks_for_phase(LifecyclePhase phase) {
    std::lock_guard<std::mutex> lock(m_hooks_mutex);

    std::vector<LifecycleHook*> hooks;
    for (auto& [name, hook] : m_hooks) {
        if (hook.phase == phase) {
            hooks.push_back(&hook);
        }
    }

    // Sort by priority (lower = earlier)
    std::sort(hooks.begin(), hooks.end(),
        [](const LifecycleHook* a, const LifecycleHook* b) {
            return static_cast<int>(a->priority) < static_cast<int>(b->priority);
        });

    return hooks;
}

void LifecycleManager::record_transition(LifecyclePhase old_phase, LifecyclePhase new_phase) {
    LifecycleEvent event{
        .old_phase = old_phase,
        .new_phase = new_phase,
        .timestamp = std::chrono::system_clock::now(),
        .details = ""
    };

    m_events.push_back(event);

    if (m_on_phase_change) {
        m_on_phase_change(event);
    }
}

} // namespace void_engine
