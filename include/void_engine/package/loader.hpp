#pragma once

/// @file loader.hpp
/// @brief Package loader interface and load context
///
/// PackageLoader is the base interface for type-specific package loaders.
/// LoadContext provides access to engine systems needed during loading.

#include "fwd.hpp"
#include "resolver.hpp"
#include <void_engine/core/error.hpp>

#include <memory>
#include <map>
#include <functional>
#include <typeindex>

// Forward declarations for engine systems
namespace void_ecs { class World; }
namespace void_event { class EventBus; }

namespace void_package {

// Forward declare for loader registration
class PackageLoader;

// =============================================================================
// LoadContext
// =============================================================================

/// Context provided to package loaders during load/unload operations
///
/// The LoadContext provides access to engine systems that packages may need:
/// - ECS World for component/system registration
/// - EventBus for event subscription
/// - AssetServer for asset loading (Phase 2)
/// - Other registered services
///
/// Thread-safety: LoadContext is NOT thread-safe. Package loading should
/// occur on a single thread (typically the main thread).
class LoadContext {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    LoadContext() = default;

    /// Construct with required systems
    explicit LoadContext(void_ecs::World* ecs_world,
                         void_event::EventBus* event_bus = nullptr);

    // Non-copyable, movable
    LoadContext(const LoadContext&) = delete;
    LoadContext& operator=(const LoadContext&) = delete;
    LoadContext(LoadContext&&) = default;
    LoadContext& operator=(LoadContext&&) = default;

    // =========================================================================
    // Core Systems Access
    // =========================================================================

    /// Get the ECS world (may be null if not set)
    [[nodiscard]] void_ecs::World* ecs_world() const noexcept { return m_ecs_world; }

    /// Get the event bus (may be null if not set)
    [[nodiscard]] void_event::EventBus* event_bus() const noexcept { return m_event_bus; }

    /// Set ECS world
    void set_ecs_world(void_ecs::World* world) noexcept { m_ecs_world = world; }

    /// Set event bus
    void set_event_bus(void_event::EventBus* bus) noexcept { m_event_bus = bus; }

    // =========================================================================
    // Loader Registration
    // =========================================================================

    /// Register a package loader for a specific type
    ///
    /// @param loader The loader to register (takes ownership)
    void register_loader(std::unique_ptr<PackageLoader> loader);

    /// Get loader for a package type
    ///
    /// @param type Package type
    /// @return Pointer to loader, or nullptr if not registered
    [[nodiscard]] PackageLoader* get_loader(PackageType type) const;

    /// Check if a loader is registered for a type
    [[nodiscard]] bool has_loader(PackageType type) const;

    // =========================================================================
    // Generic Service Registration
    // =========================================================================

    /// Register a service by type
    ///
    /// @tparam T Service type
    /// @param service Pointer to service (caller retains ownership)
    template<typename T>
    void register_service(T* service) {
        m_services[std::type_index(typeid(T))] = static_cast<void*>(service);
    }

    /// Get a registered service
    ///
    /// @tparam T Service type
    /// @return Pointer to service, or nullptr if not registered
    template<typename T>
    [[nodiscard]] T* get_service() const {
        auto it = m_services.find(std::type_index(typeid(T)));
        if (it == m_services.end()) {
            return nullptr;
        }
        return static_cast<T*>(it->second);
    }

    /// Check if a service is registered
    template<typename T>
    [[nodiscard]] bool has_service() const {
        return m_services.count(std::type_index(typeid(T))) > 0;
    }

    // =========================================================================
    // Load State
    // =========================================================================

    /// Mark a package as currently loading
    void begin_loading(const std::string& package_name);

    /// Mark a package as finished loading
    void end_loading(const std::string& package_name);

    /// Check if a package is currently being loaded
    [[nodiscard]] bool is_loading(const std::string& package_name) const;

    /// Get names of packages currently being loaded
    [[nodiscard]] const std::set<std::string>& loading_packages() const noexcept {
        return m_loading;
    }

private:
    // Core systems
    void_ecs::World* m_ecs_world = nullptr;
    void_event::EventBus* m_event_bus = nullptr;

    // Loaders by package type
    std::map<PackageType, std::unique_ptr<PackageLoader>> m_loaders;

    // Generic services
    std::map<std::type_index, void*> m_services;

    // Currently loading packages (for cycle detection)
    std::set<std::string> m_loading;
};

// =============================================================================
// PackageLoader
// =============================================================================

/// Abstract base class for package loaders
///
/// Each package type has its own loader implementation:
/// - AssetBundleLoader (Phase 2)
/// - PluginPackageLoader (Phase 3)
/// - WidgetPackageLoader (Phase 4)
/// - LayerPackageLoader (Phase 5)
/// - WorldPackageLoader (Phase 6)
///
/// Loaders are responsible for:
/// - Parsing type-specific manifest sections
/// - Loading package content into engine systems
/// - Unloading package content cleanly
/// - Supporting hot-reload where applicable
class PackageLoader {
public:
    virtual ~PackageLoader() = default;

    // =========================================================================
    // Type Information
    // =========================================================================

    /// Get the package type this loader handles
    [[nodiscard]] virtual PackageType supported_type() const = 0;

    /// Get loader name for debugging
    [[nodiscard]] virtual const char* name() const = 0;

    // =========================================================================
    // Loading
    // =========================================================================

    /// Load a package
    ///
    /// @param package Resolved package information
    /// @param ctx Load context with access to engine systems
    /// @return Ok on success, Error on failure
    [[nodiscard]] virtual void_core::Result<void> load(
        const ResolvedPackage& package,
        LoadContext& ctx) = 0;

    /// Unload a package
    ///
    /// @param package_name Name of package to unload
    /// @param ctx Load context with access to engine systems
    /// @return Ok on success, Error on failure
    [[nodiscard]] virtual void_core::Result<void> unload(
        const std::string& package_name,
        LoadContext& ctx) = 0;

    // =========================================================================
    // Hot-Reload Support
    // =========================================================================

    /// Check if this loader supports hot-reload
    [[nodiscard]] virtual bool supports_hot_reload() const { return false; }

    /// Reload a package
    ///
    /// Default implementation unloads then loads.
    ///
    /// @param package Resolved package information
    /// @param ctx Load context with access to engine systems
    /// @return Ok on success, Error on failure
    [[nodiscard]] virtual void_core::Result<void> reload(
        const ResolvedPackage& package,
        LoadContext& ctx);

    // =========================================================================
    // Queries
    // =========================================================================

    /// Check if a package is currently loaded by this loader
    [[nodiscard]] virtual bool is_loaded(const std::string& package_name) const = 0;

    /// Get names of all packages loaded by this loader
    [[nodiscard]] virtual std::vector<std::string> loaded_packages() const = 0;

protected:
    PackageLoader() = default;

    // Non-copyable
    PackageLoader(const PackageLoader&) = delete;
    PackageLoader& operator=(const PackageLoader&) = delete;
};

// =============================================================================
// Stub Loader
// =============================================================================

/// A stub loader that accepts but does nothing
///
/// Useful for testing or as a placeholder during development.
class StubPackageLoader : public PackageLoader {
public:
    explicit StubPackageLoader(PackageType type, const char* name = "StubLoader")
        : m_type(type), m_name(name) {}

    [[nodiscard]] PackageType supported_type() const override { return m_type; }
    [[nodiscard]] const char* name() const override { return m_name; }

    [[nodiscard]] void_core::Result<void> load(
        const ResolvedPackage& package,
        LoadContext& ctx) override;

    [[nodiscard]] void_core::Result<void> unload(
        const std::string& package_name,
        LoadContext& ctx) override;

    [[nodiscard]] bool is_loaded(const std::string& package_name) const override;
    [[nodiscard]] std::vector<std::string> loaded_packages() const override;

private:
    PackageType m_type;
    const char* m_name;
    std::set<std::string> m_loaded;
};

// =============================================================================
// Loader Factory Functions
// =============================================================================

/// Create a plugin package loader (Phase 3)
[[nodiscard]] std::unique_ptr<PackageLoader> create_plugin_package_loader();

/// Create a widget package loader (Phase 4)
[[nodiscard]] std::unique_ptr<PackageLoader> create_widget_package_loader();

/// Create a layer package loader (Phase 5)
[[nodiscard]] std::unique_ptr<PackageLoader> create_layer_package_loader();

/// Create a world package loader (Phase 6)
[[nodiscard]] std::unique_ptr<PackageLoader> create_world_package_loader();

} // namespace void_package
