/// @file sandbox.cpp
/// @brief Sandbox implementation for isolated execution

#include <void_engine/kernel/sandbox.hpp>

#include <algorithm>
#include <regex>

#ifdef _WIN32
    // Windows doesn't have fnmatch, use simple pattern matching
    static int fnmatch(const char* pattern, const char* string, int) {
        // Very basic glob matching for Windows
        std::string pat(pattern);
        std::string str(string);

        // Convert glob to regex
        std::string regex_str;
        for (char c : pat) {
            switch (c) {
                case '*': regex_str += ".*"; break;
                case '?': regex_str += "."; break;
                case '.': regex_str += "\\."; break;
                default: regex_str += c; break;
            }
        }

        try {
            std::regex re(regex_str, std::regex::icase);
            return std::regex_match(str, re) ? 0 : 1;
        } catch (...) {
            return 1;
        }
    }
    #define FNM_PATHNAME 0
#else
    #include <fnmatch.h>
#endif

namespace void_kernel {

// =============================================================================
// Thread-local sandbox context
// =============================================================================

static thread_local Sandbox* t_current_sandbox = nullptr;

Sandbox* current_sandbox() {
    return t_current_sandbox;
}

void set_current_sandbox(Sandbox* sandbox) {
    t_current_sandbox = sandbox;
}

// =============================================================================
// SandboxGuard Implementation
// =============================================================================

SandboxGuard::SandboxGuard(Sandbox& sandbox) : m_previous(t_current_sandbox), m_current(&sandbox) {
    t_current_sandbox = m_current;
    m_current->enter();
}

SandboxGuard::~SandboxGuard() {
    m_current->exit();
    t_current_sandbox = m_previous;
}

// =============================================================================
// PermissionSet Implementation
// =============================================================================

PermissionSet::PermissionSet(Permission base_permissions) : m_permissions(base_permissions) {}

void PermissionSet::grant(Permission perm) {
    m_permissions |= perm;
}

void PermissionSet::revoke(Permission perm) {
    m_permissions = m_permissions & ~perm;
}

bool PermissionSet::has(Permission perm) const {
    return has_permission(m_permissions, perm);
}

bool PermissionSet::has_all(Permission perms) const {
    return (static_cast<std::uint32_t>(m_permissions) & static_cast<std::uint32_t>(perms)) ==
           static_cast<std::uint32_t>(perms);
}

bool PermissionSet::has_any(Permission perms) const {
    return (static_cast<std::uint32_t>(m_permissions) & static_cast<std::uint32_t>(perms)) != 0;
}

void PermissionSet::allow_path(const std::filesystem::path& path) {
    m_allowed_paths.push_back(std::filesystem::absolute(path));
}

void PermissionSet::allow_path_pattern(const std::string& pattern) {
    m_path_patterns.push_back(pattern);
}

bool PermissionSet::is_path_allowed(const std::filesystem::path& path) const {
    auto abs_path = std::filesystem::absolute(path);
    auto path_str = abs_path.string();

    // Check explicit paths
    for (const auto& allowed : m_allowed_paths) {
        auto allowed_str = allowed.string();

        // Check if path is under allowed directory
        if (path_str.find(allowed_str) == 0) {
            return true;
        }
    }

    // Check patterns
    for (const auto& pattern : m_path_patterns) {
        if (fnmatch(pattern.c_str(), path_str.c_str(), FNM_PATHNAME) == 0) {
            return true;
        }
    }

    return false;
}

const std::vector<std::filesystem::path>& PermissionSet::allowed_paths() const {
    return m_allowed_paths;
}

void PermissionSet::allow_host(const std::string& host) {
    m_allowed_hosts.push_back(host);
}

void PermissionSet::allow_host_pattern(const std::string& pattern) {
    m_host_patterns.push_back(pattern);
}

bool PermissionSet::is_host_allowed(const std::string& host) const {
    // Check explicit hosts
    for (const auto& allowed : m_allowed_hosts) {
        if (host == allowed) {
            return true;
        }
    }

    // Check patterns
    for (const auto& pattern : m_host_patterns) {
        if (fnmatch(pattern.c_str(), host.c_str(), 0) == 0) {
            return true;
        }
    }

    return false;
}

const std::vector<std::string>& PermissionSet::allowed_hosts() const {
    return m_allowed_hosts;
}

PermissionSet PermissionSet::minimal() {
    return PermissionSet{Permission::None};
}

PermissionSet PermissionSet::read_only() {
    return PermissionSet{
        Permission::FileRead |
        Permission::AssetRead |
        Permission::SystemInfo |
        Permission::TimeAccess |
        Permission::RandomAccess
    };
}

PermissionSet PermissionSet::full() {
    return PermissionSet{Permission::All};
}

PermissionSet PermissionSet::game_script() {
    return PermissionSet{
        Permission::AssetRead |
        Permission::EntityAll |
        Permission::ComponentAccess |
        Permission::EventAll |
        Permission::TimeAccess |
        Permission::RandomAccess
    };
}

PermissionSet PermissionSet::editor_plugin() {
    return PermissionSet{
        Permission::FileAll |
        Permission::AssetAll |
        Permission::EntityAll |
        Permission::ComponentAccess |
        Permission::EventAll |
        Permission::ServiceCall |
        Permission::SystemInfo |
        Permission::TimeAccess |
        Permission::RandomAccess |
        Permission::ThreadCreate
    };
}

// =============================================================================
// ResourceUsageTracker Implementation
// =============================================================================

ResourceUsageTracker::ResourceUsageTracker(const ResourceLimits& limits) : m_limits(limits) {}

bool ResourceUsageTracker::allocate(std::size_t bytes) {
    std::size_t current = m_memory_used.load();
    std::size_t new_value = current + bytes;

    if (m_limits.max_memory_bytes > 0 && new_value > m_limits.max_memory_bytes) {
        return false;
    }

    if (m_limits.max_allocations > 0 && m_allocation_count.load() >= m_limits.max_allocations) {
        return false;
    }

    m_memory_used.fetch_add(bytes);
    m_allocation_count.fetch_add(1);

    // Update peak
    std::size_t peak = m_memory_peak.load();
    while (new_value > peak && !m_memory_peak.compare_exchange_weak(peak, new_value)) {
        // Retry
    }

    return true;
}

void ResourceUsageTracker::deallocate(std::size_t bytes) {
    m_memory_used.fetch_sub(bytes);
}

bool ResourceUsageTracker::use_cpu_time(std::uint64_t microseconds) {
    if (m_limits.max_cpu_time_us > 0 &&
        m_cpu_time_used.load() + microseconds > m_limits.max_cpu_time_us) {
        return false;
    }

    m_cpu_time_used.fetch_add(microseconds);
    return true;
}

bool ResourceUsageTracker::execute_instructions(std::uint64_t count) {
    if (m_limits.max_instructions > 0 &&
        m_instructions.load() + count > m_limits.max_instructions) {
        return false;
    }

    m_instructions.fetch_add(count);
    return true;
}

bool ResourceUsageTracker::open_handle() {
    if (m_limits.max_file_handles > 0 &&
        m_open_handles.load() >= m_limits.max_file_handles) {
        return false;
    }

    m_open_handles.fetch_add(1);
    return true;
}

void ResourceUsageTracker::close_handle() {
    m_open_handles.fetch_sub(1);
}

bool ResourceUsageTracker::create_thread() {
    if (m_limits.max_threads > 0 &&
        m_active_threads.load() >= m_limits.max_threads) {
        return false;
    }

    m_active_threads.fetch_add(1);
    return true;
}

void ResourceUsageTracker::terminate_thread() {
    m_active_threads.fetch_sub(1);
}

bool ResourceUsageTracker::any_limit_exceeded() const {
    if (m_limits.max_memory_bytes > 0 && m_memory_used.load() > m_limits.max_memory_bytes) {
        return true;
    }
    if (m_limits.max_cpu_time_us > 0 && m_cpu_time_used.load() > m_limits.max_cpu_time_us) {
        return true;
    }
    if (m_limits.max_instructions > 0 && m_instructions.load() > m_limits.max_instructions) {
        return true;
    }
    if (m_limits.max_file_handles > 0 && m_open_handles.load() > m_limits.max_file_handles) {
        return true;
    }
    if (m_limits.max_threads > 0 && m_active_threads.load() > m_limits.max_threads) {
        return true;
    }
    if (m_limits.max_allocations > 0 && m_allocation_count.load() > m_limits.max_allocations) {
        return true;
    }
    return false;
}

std::vector<std::string> ResourceUsageTracker::exceeded_limits() const {
    std::vector<std::string> exceeded;

    if (m_limits.max_memory_bytes > 0 && m_memory_used.load() > m_limits.max_memory_bytes) {
        exceeded.push_back("memory");
    }
    if (m_limits.max_cpu_time_us > 0 && m_cpu_time_used.load() > m_limits.max_cpu_time_us) {
        exceeded.push_back("cpu_time");
    }
    if (m_limits.max_instructions > 0 && m_instructions.load() > m_limits.max_instructions) {
        exceeded.push_back("instructions");
    }
    if (m_limits.max_file_handles > 0 && m_open_handles.load() > m_limits.max_file_handles) {
        exceeded.push_back("file_handles");
    }
    if (m_limits.max_threads > 0 && m_active_threads.load() > m_limits.max_threads) {
        exceeded.push_back("threads");
    }
    if (m_limits.max_allocations > 0 && m_allocation_count.load() > m_limits.max_allocations) {
        exceeded.push_back("allocations");
    }

    return exceeded;
}

void ResourceUsageTracker::reset() {
    m_memory_used.store(0);
    m_memory_peak.store(0);
    m_allocation_count.store(0);
    m_cpu_time_used.store(0);
    m_instructions.store(0);
    m_open_handles.store(0);
    m_active_threads.store(0);
}

// =============================================================================
// Sandbox Implementation
// =============================================================================

Sandbox::Sandbox(SandboxConfig config)
    : m_config(std::move(config))
    , m_permissions(m_config.permissions)
    , m_resources(m_config.limits)
    , m_creation_time(std::chrono::steady_clock::now()) {

    // Add allowed paths from config
    for (const auto& path : m_config.allowed_paths) {
        m_permissions.allow_path(path);
    }

    // Add allowed hosts from config
    for (const auto& host : m_config.allowed_hosts) {
        m_permissions.allow_host(host);
    }
}

Sandbox::~Sandbox() {
    terminate();
}

void_core::Result<void> Sandbox::enter() {
    auto state = m_state.load();
    if (state == SandboxState::Terminated || state == SandboxState::Violated) {
        return void_core::Error{"Cannot enter terminated or violated sandbox"};
    }

    m_state.store(SandboxState::Running);
    m_enter_time = std::chrono::steady_clock::now();
    return void_core::Ok();
}

void Sandbox::exit() {
    if (m_state.load() == SandboxState::Running) {
        m_total_execution_time += std::chrono::steady_clock::now() - m_enter_time;
        m_state.store(SandboxState::Created);
    }
}

void Sandbox::suspend() {
    if (m_state.load() == SandboxState::Running) {
        m_total_execution_time += std::chrono::steady_clock::now() - m_enter_time;
        m_state.store(SandboxState::Suspended);
    }
}

void Sandbox::resume() {
    if (m_state.load() == SandboxState::Suspended) {
        m_enter_time = std::chrono::steady_clock::now();
        m_state.store(SandboxState::Running);
    }
}

void Sandbox::terminate() {
    m_state.store(SandboxState::Terminated);
}

bool Sandbox::check_permission(Permission perm) const {
    return m_permissions.has(perm);
}

bool Sandbox::check_file_access(const std::filesystem::path& path, Permission access_type) const {
    // Check basic file permission
    if (!m_permissions.has(access_type)) {
        return false;
    }

    // Check path allowlist
    return m_permissions.is_path_allowed(path);
}

bool Sandbox::check_network_access(const std::string& host, Permission access_type) const {
    // Check basic network permission
    if (!m_permissions.has(access_type)) {
        return false;
    }

    // Check host allowlist
    return m_permissions.is_host_allowed(host);
}

void_core::Result<void> Sandbox::request_permission(Permission perm) {
    // In a real implementation, this might prompt the user or check a policy
    // For now, just check if the permission is already granted
    if (m_permissions.has(perm)) {
        return void_core::Ok();
    }
    return void_core::Error{"Permission denied"};
}

void_core::Result<void> Sandbox::allocate_memory(std::size_t bytes) {
    if (!m_resources.allocate(bytes)) {
        handle_violation(Permission::None, "Memory limit exceeded");
        return void_core::Error{"Memory limit exceeded"};
    }
    return void_core::Ok();
}

void Sandbox::deallocate_memory(std::size_t bytes) {
    m_resources.deallocate(bytes);
}

void_core::Result<void> Sandbox::use_cpu_time(std::uint64_t microseconds) {
    if (!m_resources.use_cpu_time(microseconds)) {
        handle_violation(Permission::None, "CPU time limit exceeded");
        return void_core::Error{"CPU time limit exceeded"};
    }
    return void_core::Ok();
}

void_core::Result<void> Sandbox::execute_instructions(std::uint64_t count) {
    if (!m_resources.execute_instructions(count)) {
        handle_violation(Permission::None, "Instruction limit exceeded");
        return void_core::Error{"Instruction limit exceeded"};
    }
    return void_core::Ok();
}

void_core::Result<void> Sandbox::open_handle() {
    if (!m_resources.open_handle()) {
        handle_violation(Permission::FileRead, "File handle limit exceeded");
        return void_core::Error{"File handle limit exceeded"};
    }
    return void_core::Ok();
}

void Sandbox::close_handle() {
    m_resources.close_handle();
}

void_core::Result<void> Sandbox::create_thread() {
    if (!check_permission(Permission::ThreadCreate)) {
        handle_violation(Permission::ThreadCreate, "Thread creation not permitted");
        return void_core::Error{"Thread creation not permitted"};
    }
    if (!m_resources.create_thread()) {
        handle_violation(Permission::ThreadCreate, "Thread limit exceeded");
        return void_core::Error{"Thread limit exceeded"};
    }
    return void_core::Ok();
}

void Sandbox::terminate_thread() {
    m_resources.terminate_thread();
}

void Sandbox::report_violation(Permission attempted, const std::string& details) {
    handle_violation(attempted, details);
}

std::optional<SandboxViolationEvent> Sandbox::last_violation() const {
    std::lock_guard<std::mutex> lock(m_violation_mutex);
    return m_last_violation;
}

void Sandbox::set_violation_callback(ViolationCallback callback) {
    m_violation_callback = std::move(callback);
}

void Sandbox::handle_violation(Permission attempted, const std::string& details) {
    auto count = m_violation_count.fetch_add(1) + 1;

    SandboxViolationEvent event{
        .sandbox_name = m_config.name,
        .attempted_permission = attempted,
        .details = details,
        .timestamp = std::chrono::system_clock::now(),
    };

    {
        std::lock_guard<std::mutex> lock(m_violation_mutex);
        m_last_violation = event;
    }

    if (m_violation_callback) {
        m_violation_callback(event);
    }

    // Check if max violations exceeded
    if (m_max_violations > 0 && count >= m_max_violations) {
        m_state.store(SandboxState::Violated);
    }
}

std::chrono::nanoseconds Sandbox::uptime() const {
    return std::chrono::steady_clock::now() - m_creation_time;
}

std::chrono::nanoseconds Sandbox::execution_time() const {
    auto total = m_total_execution_time;
    if (m_state.load() == SandboxState::Running) {
        total += std::chrono::steady_clock::now() - m_enter_time;
    }
    return total;
}

// =============================================================================
// SandboxFactory Implementation
// =============================================================================

std::unique_ptr<Sandbox> SandboxFactory::create_trusted(const std::string& name) {
    return std::make_unique<Sandbox>(SandboxConfig::trusted(name));
}

std::unique_ptr<Sandbox> SandboxFactory::create_untrusted(const std::string& name) {
    return std::make_unique<Sandbox>(SandboxConfig::untrusted(name));
}

std::unique_ptr<Sandbox> SandboxFactory::create_for_script(const std::string& name) {
    SandboxConfig config;
    config.name = name;
    config.permissions = PermissionSet::game_script().raw();
    config.limits = ResourceLimits::defaults();
    config.limits.max_memory_bytes = 128 * 1024 * 1024;  // 128 MB for scripts
    config.limits.max_cpu_time_us = 10000000;  // 10 seconds
    return std::make_unique<Sandbox>(std::move(config));
}

std::unique_ptr<Sandbox> SandboxFactory::create_for_plugin(const std::string& name) {
    SandboxConfig config;
    config.name = name;
    config.permissions = PermissionSet::editor_plugin().raw();
    config.limits = ResourceLimits::defaults();
    config.inherit_environment = true;
    return std::make_unique<Sandbox>(std::move(config));
}

std::unique_ptr<Sandbox> SandboxFactory::create_custom(const SandboxConfig& config) {
    return std::make_unique<Sandbox>(config);
}

} // namespace void_kernel
