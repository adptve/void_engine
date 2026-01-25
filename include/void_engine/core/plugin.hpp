#pragma once

/// @file plugin.hpp
/// @brief Plugin system for void_core

#include "fwd.hpp"
#include "error.hpp"
#include "version.hpp"
#include "id.hpp"
#include "type_registry.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <any>
#include <functional>

namespace void_core {

// =============================================================================
// PluginId
// =============================================================================

/// Plugin identifier
struct PluginId {
    NamedId id;

    /// Default constructor
    PluginId() = default;

    /// Construct from name
    explicit PluginId(const std::string& name) : id(name) {}
    explicit PluginId(const char* name) : id(name) {}

    /// Get name
    [[nodiscard]] const std::string& name() const noexcept { return id.name; }

    /// Get hash
    [[nodiscard]] std::uint64_t hash() const noexcept { return id.hash; }

    /// Comparison
    bool operator==(const PluginId& other) const noexcept { return id == other.id; }
    bool operator!=(const PluginId& other) const noexcept { return id != other.id; }
    bool operator<(const PluginId& other) const noexcept { return id < other.id; }

    /// String conversion
    [[nodiscard]] explicit operator std::string() const { return id.name; }
};

// =============================================================================
// PluginStatus
// =============================================================================

/// Plugin lifecycle status
enum class PluginStatus : std::uint8_t {
    Registered,   // Registered but not loaded
    Loading,      // Currently loading
    Active,       // Loaded and active
    Unloading,    // Being unloaded
    Unloaded,     // Unloaded (was active, now unloaded)
    Failed,       // Load failed
    Disabled,     // Disabled
};

/// Get status name
[[nodiscard]] inline const char* plugin_status_name(PluginStatus status) {
    switch (status) {
        case PluginStatus::Registered: return "Registered";
        case PluginStatus::Loading: return "Loading";
        case PluginStatus::Active: return "Active";
        case PluginStatus::Unloading: return "Unloading";
        case PluginStatus::Unloaded: return "Unloaded";
        case PluginStatus::Failed: return "Failed";
        case PluginStatus::Disabled: return "Disabled";
        default: return "Unknown";
    }
}

// =============================================================================
// PluginState
// =============================================================================

/// State preservation across hot-reloads
struct PluginState {
    std::vector<std::uint8_t> data;  // Serialized state
    std::string type_name;            // Type validation
    Version version;                  // Version snapshot

    /// Default constructor (empty state)
    PluginState() = default;

    /// Construct with data
    PluginState(std::vector<std::uint8_t> d, const std::string& tn, Version v)
        : data(std::move(d)), type_name(tn), version(v) {}

    /// Create empty state
    [[nodiscard]] static PluginState empty() {
        return PluginState{};
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return data.empty();
    }
};

// =============================================================================
// PluginContext
// =============================================================================

/// Context passed to plugin lifecycle methods
struct PluginContext {
    TypeRegistry* types = nullptr;
    class PluginRegistry* plugins = nullptr;
    std::map<std::string, std::any> data;

    /// Insert data
    template<typename T>
    void insert(const std::string& key, T value) {
        data[key] = std::move(value);
    }

    /// Get data (const)
    template<typename T>
    [[nodiscard]] const T* get(const std::string& key) const {
        auto it = data.find(key);
        if (it == data.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&it->second);
    }

    /// Get data (mutable)
    template<typename T>
    [[nodiscard]] T* get_mut(const std::string& key) {
        auto it = data.find(key);
        if (it == data.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&it->second);
    }

    /// Remove data
    bool remove(const std::string& key) {
        return data.erase(key) > 0;
    }

    /// Check if key exists
    [[nodiscard]] bool contains(const std::string& key) const {
        return data.find(key) != data.end();
    }
};

// =============================================================================
// PluginInfo
// =============================================================================

/// Plugin metadata
struct PluginInfo {
    PluginId id;
    Version version;
    std::vector<PluginId> dependencies;
    PluginStatus status = PluginStatus::Registered;
    bool supports_hot_reload = false;

    PluginInfo() = default;

    PluginInfo(PluginId pid, Version v, std::vector<PluginId> deps, bool hot_reload)
        : id(std::move(pid))
        , version(v)
        , dependencies(std::move(deps))
        , supports_hot_reload(hot_reload) {}
};

// =============================================================================
// Plugin (Base Class)
// =============================================================================

/// Base class for all plugins
class Plugin {
public:
    virtual ~Plugin() = default;

    /// Get plugin ID
    [[nodiscard]] virtual PluginId id() const = 0;

    /// Get plugin version
    [[nodiscard]] virtual Version version() const {
        return Version{0, 1, 0};
    }

    /// Get dependencies
    [[nodiscard]] virtual std::vector<PluginId> dependencies() const {
        return {};
    }

    /// Called when plugin is loaded
    virtual Result<void> on_load(PluginContext& ctx) = 0;

    /// Called every frame
    virtual void on_update(float dt) {
        (void)dt;
    }

    /// Called when plugin is unloaded
    /// Returns state for potential hot-reload
    virtual Result<PluginState> on_unload(PluginContext& ctx) = 0;

    /// Called when plugin is reloaded with previous state
    virtual Result<void> on_reload(PluginContext& ctx, PluginState state) {
        (void)state;
        return on_load(ctx);
    }

    /// Register types with the type registry
    virtual void register_types(TypeRegistry& registry) {
        (void)registry;
    }

    /// Check if plugin supports hot-reload
    [[nodiscard]] virtual bool supports_hot_reload() const {
        return false;
    }

    /// Get plugin info
    [[nodiscard]] PluginInfo info() const {
        return PluginInfo{
            id(),
            version(),
            dependencies(),
            supports_hot_reload()
        };
    }
};

// =============================================================================
// PluginRegistry
// =============================================================================

/// Central plugin management system
class PluginRegistry {
public:
    /// Constructor
    PluginRegistry() = default;

    /// Register plugin
    Result<void> register_plugin(std::unique_ptr<Plugin> plugin) {
        if (!plugin) {
            return Err(Error("Cannot register null plugin"));
        }

        PluginId pid = plugin->id();
        std::string name = pid.name();

        if (m_plugins.find(name) != m_plugins.end()) {
            return Err(PluginError::already_registered(name));
        }

        PluginInfo info = plugin->info();
        info.status = PluginStatus::Registered;

        m_plugins[name] = std::move(plugin);
        m_info[name] = info;

        return Ok();
    }

    /// Load plugin by ID
    Result<void> load(const PluginId& id, TypeRegistry& types) {
        std::string name = id.name();

        auto plugin_it = m_plugins.find(name);
        if (plugin_it == m_plugins.end()) {
            return Err(PluginError::not_found(name));
        }

        auto info_it = m_info.find(name);
        if (info_it == m_info.end()) {
            return Err(PluginError::not_found(name));
        }

        // Check dependencies
        for (const auto& dep : info_it->second.dependencies) {
            if (!is_active(dep)) {
                return Err(PluginError::missing_dependency(name, dep.name()));
            }
        }

        // Set status to loading
        info_it->second.status = PluginStatus::Loading;

        // Register types
        plugin_it->second->register_types(types);

        // Create context
        PluginContext ctx;
        ctx.types = &types;
        ctx.plugins = this;

        // Call on_load
        auto result = plugin_it->second->on_load(ctx);
        if (!result) {
            info_it->second.status = PluginStatus::Failed;
            return Err(PluginError::init_failed(name, result.error().message()));
        }

        // Set status to active
        info_it->second.status = PluginStatus::Active;
        m_load_order.push_back(id);

        return Ok();
    }

    /// Unload plugin by ID
    Result<PluginState> unload(const PluginId& id, TypeRegistry& types) {
        std::string name = id.name();

        auto plugin_it = m_plugins.find(name);
        if (plugin_it == m_plugins.end()) {
            return Err<PluginState>(PluginError::not_found(name));
        }

        auto info_it = m_info.find(name);
        if (info_it == m_info.end()) {
            return Err<PluginState>(PluginError::not_found(name));
        }

        if (info_it->second.status != PluginStatus::Active) {
            return Err<PluginState>(PluginError::invalid_state(name, "Plugin is not active"));
        }

        // Set status to unloading
        info_it->second.status = PluginStatus::Unloading;

        // Create context
        PluginContext ctx;
        ctx.types = &types;
        ctx.plugins = this;

        // Call on_unload
        auto result = plugin_it->second->on_unload(ctx);
        if (!result) {
            info_it->second.status = PluginStatus::Active;  // Revert
            return Err<PluginState>(result.error());
        }

        // Set status to registered
        info_it->second.status = PluginStatus::Registered;

        // Remove from load order
        m_load_order.erase(
            std::remove(m_load_order.begin(), m_load_order.end(), id),
            m_load_order.end());

        return Ok(std::move(result).value());
    }

    /// Hot-reload plugin with new implementation
    Result<void> hot_reload(
        const PluginId& id,
        std::unique_ptr<Plugin> new_plugin,
        TypeRegistry& types)
    {
        std::string name = id.name();

        if (!new_plugin) {
            return Err(Error("Cannot hot-reload with null plugin"));
        }

        auto info_it = m_info.find(name);
        if (info_it == m_info.end()) {
            return Err(PluginError::not_found(name));
        }

        if (!info_it->second.supports_hot_reload) {
            return Err(PluginError::invalid_state(name, "Plugin does not support hot-reload"));
        }

        // Unload current
        auto unload_result = unload(id, types);
        if (!unload_result) {
            return Err(unload_result.error());
        }

        PluginState state = std::move(unload_result).value();

        // Replace plugin
        m_plugins[name] = std::move(new_plugin);
        auto& plugin = m_plugins[name];

        // Update info
        m_info[name] = plugin->info();
        m_info[name].status = PluginStatus::Loading;

        // Register types
        plugin->register_types(types);

        // Create context
        PluginContext ctx;
        ctx.types = &types;
        ctx.plugins = this;

        // Call on_reload with state
        auto reload_result = plugin->on_reload(ctx, std::move(state));
        if (!reload_result) {
            m_info[name].status = PluginStatus::Failed;
            return Err(reload_result.error());
        }

        m_info[name].status = PluginStatus::Active;
        m_load_order.push_back(id);

        return Ok();
    }

    /// Get plugin by ID
    [[nodiscard]] Plugin* get(const PluginId& id) {
        auto it = m_plugins.find(id.name());
        return it != m_plugins.end() ? it->second.get() : nullptr;
    }

    [[nodiscard]] const Plugin* get(const PluginId& id) const {
        auto it = m_plugins.find(id.name());
        return it != m_plugins.end() ? it->second.get() : nullptr;
    }

    /// Get plugin info
    [[nodiscard]] const PluginInfo* info(const PluginId& id) const {
        auto it = m_info.find(id.name());
        return it != m_info.end() ? &it->second : nullptr;
    }

    /// Check if plugin is active
    [[nodiscard]] bool is_active(const PluginId& id) const {
        auto info_ptr = info(id);
        return info_ptr && info_ptr->status == PluginStatus::Active;
    }

    /// Update all active plugins
    void update_all(float dt) {
        for (const auto& id : m_load_order) {
            auto* plugin = get(id);
            if (plugin && is_active(id)) {
                plugin->on_update(dt);
            }
        }
    }

    /// Get count
    [[nodiscard]] std::size_t len() const noexcept {
        return m_plugins.size();
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return m_plugins.empty();
    }

    /// Get active count
    [[nodiscard]] std::size_t active_count() const {
        std::size_t count = 0;
        for (const auto& [name, info] : m_info) {
            if (info.status == PluginStatus::Active) {
                count++;
            }
        }
        return count;
    }

    /// Get load order
    [[nodiscard]] const std::vector<PluginId>& load_order() const noexcept {
        return m_load_order;
    }

    /// Iterate active plugins
    template<typename F>
    void for_each_active(F&& func) {
        for (const auto& id : m_load_order) {
            auto* plugin = get(id);
            if (plugin && is_active(id)) {
                func(*plugin);
            }
        }
    }

    template<typename F>
    void for_each_active(F&& func) const {
        for (const auto& id : m_load_order) {
            const auto* plugin = get(id);
            if (plugin && is_active(id)) {
                func(*plugin);
            }
        }
    }

    /// Unload all plugins in reverse order (with TypeRegistry)
    void unload_all(TypeRegistry& types) {
        // Unload in reverse order
        for (auto it = m_load_order.rbegin(); it != m_load_order.rend(); ++it) {
            auto* plugin = get(*it);
            if (plugin) {
                auto info_it = m_info.find(it->name());
                if (info_it != m_info.end()) {
                    info_it->second.status = PluginStatus::Unloading;
                }

                // Create context for unload
                PluginContext ctx;
                ctx.types = &types;
                ctx.plugins = this;

                plugin->on_unload(ctx);

                if (info_it != m_info.end()) {
                    info_it->second.status = PluginStatus::Unloaded;
                }
            }
        }
        m_load_order.clear();
        m_plugins.clear();
        m_info.clear();
    }

    /// Unload all plugins in reverse order (no TypeRegistry - for shutdown)
    void unload_all() {
        // Unload in reverse order without calling on_unload
        // Used during shutdown when TypeRegistry may not be available
        for (auto it = m_load_order.rbegin(); it != m_load_order.rend(); ++it) {
            auto info_it = m_info.find(it->name());
            if (info_it != m_info.end()) {
                info_it->second.status = PluginStatus::Unloaded;
            }
        }
        m_load_order.clear();
        m_plugins.clear();
        m_info.clear();
    }

private:
    std::map<std::string, std::unique_ptr<Plugin>> m_plugins;
    std::map<std::string, PluginInfo> m_info;
    std::vector<PluginId> m_load_order;
};

// =============================================================================
// Helper Macros
// =============================================================================

/// Define plugin ID and version
#define VOID_DEFINE_PLUGIN(ClassName, PluginName, MajorVer, MinorVer, PatchVer) \
    [[nodiscard]] ::void_core::PluginId id() const override { \
        return ::void_core::PluginId(PluginName); \
    } \
    [[nodiscard]] ::void_core::Version version() const override { \
        return ::void_core::Version{MajorVer, MinorVer, PatchVer}; \
    }

// =============================================================================
// Plugin State Serialization (Implemented in plugin.cpp)
// =============================================================================

namespace serialization {

/// Serialize a PluginState to binary
std::vector<std::uint8_t> serialize_plugin_state(const PluginState& state);

/// Deserialize a PluginState from binary
Result<PluginState> deserialize_plugin_state(const std::vector<std::uint8_t>& data);

} // namespace serialization

// =============================================================================
// Plugin Dependency Resolution (Implemented in plugin.cpp)
// =============================================================================

/// Topologically sort plugins by dependencies
Result<std::vector<PluginId>> resolve_load_order(
    const std::vector<PluginId>& plugins,
    const std::function<std::vector<PluginId>(const PluginId&)>& get_dependencies);

/// Check if all dependencies are satisfied
Result<void> check_dependencies(
    const PluginId& plugin,
    const std::vector<PluginId>& dependencies,
    const std::function<bool(const PluginId&)>& is_loaded);

// =============================================================================
// Plugin Validation (Implemented in plugin.cpp)
// =============================================================================

/// Validate a plugin before registration
Result<void> validate_plugin(const Plugin& plugin);

/// Validate plugin state before restore
Result<void> validate_plugin_state(const PluginState& state, const Plugin& plugin);

// =============================================================================
// Plugin Statistics (Implemented in plugin.cpp)
// =============================================================================

/// Plugin statistics
struct PluginStatistics {
    std::uint64_t total_loads = 0;
    std::uint64_t total_unloads = 0;
    std::uint64_t total_hot_reloads = 0;
    std::uint64_t failed_loads = 0;
    std::uint64_t failed_hot_reloads = 0;
};

/// Record a plugin load
void record_plugin_load(bool success);

/// Record a plugin unload
void record_plugin_unload();

/// Record a plugin hot-reload
void record_plugin_hot_reload(bool success);

/// Get plugin statistics
PluginStatistics get_plugin_statistics();

/// Reset plugin statistics
void reset_plugin_statistics();

/// Format plugin statistics
std::string format_plugin_statistics();

// =============================================================================
// Global Plugin Registry (Implemented in plugin.cpp)
// =============================================================================

/// Get or create the global plugin registry
PluginRegistry& global_plugin_registry();

/// Shutdown the global plugin registry
void shutdown_plugin_registry();

// =============================================================================
// Debug Utilities (Implemented in plugin.cpp)
// =============================================================================

namespace debug {

/// Format a PluginId for debugging
std::string format_plugin_id(const PluginId& id);

/// Format a PluginInfo for debugging
std::string format_plugin_info(const PluginInfo& info);

/// Format PluginRegistry state for debugging
std::string format_registry_state(const PluginRegistry& registry);

/// Format a PluginState for debugging
std::string format_plugin_state(const PluginState& state);

} // namespace debug

} // namespace void_core

/// Hash specialization
template<>
struct std::hash<void_core::PluginId> {
    std::size_t operator()(const void_core::PluginId& id) const noexcept {
        return static_cast<std::size_t>(id.hash());
    }
};
