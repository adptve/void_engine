/// @file plugin_package_loader.cpp
/// @brief Plugin package loader implementation
///
/// Loads plugin packages by:
/// 1. Parsing the plugin manifest
/// 2. Registering components to the ECS
/// 3. Loading dynamic libraries
/// 4. Registering systems from loaded libraries (legacy manifest-based)
/// 5. Instantiating IPlugin if library exports plugin_create (new Phase 3)
/// 6. Setting up event handlers
/// 7. Configuring data registries
///
/// Two loading modes are supported:
/// - Legacy: Systems declared in JSON manifest with entry points
/// - IPlugin: DLL exports plugin_create(), engine calls on_load(PluginContext)

#include <void_engine/package/loader.hpp>
#include <void_engine/package/plugin_package.hpp>
#include <void_engine/package/dynamic_library.hpp>
#include <void_engine/package/component_schema.hpp>
#include <void_engine/package/definition_registry.hpp>

// IPlugin interface, PluginContext, and PluginRegistry from Phase 1 & 4
#include <void_engine/plugin_api/plugin.hpp>
#include <void_engine/plugin_api/context.hpp>
#include <void_engine/plugin_api/state.hpp>

#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/component.hpp>
#include <void_engine/ecs/system.hpp>
#include <void_engine/kernel/kernel.hpp>

#include <spdlog/spdlog.h>

#include <map>
#include <set>

namespace void_package {

// =============================================================================
// LoadedPluginState
// =============================================================================

/// State for a loaded plugin, used for unloading and hot-reload
struct LoadedPluginState {
    std::string name;
    PluginPackageManifest manifest;
    std::vector<void_ecs::ComponentId> registered_components;
    std::vector<std::string> registered_systems;
    std::vector<std::string> registered_event_handlers;
    std::vector<std::string> configured_registries;
    std::vector<std::filesystem::path> loaded_libraries;

    // IPlugin-based plugin support (Phase 3)
    void_plugin_api::IPlugin* iplugin = nullptr;           ///< IPlugin instance (if DLL-based)
    std::unique_ptr<void_plugin_api::PluginContext> context; ///< PluginContext for this plugin
    std::filesystem::path main_library_path;               ///< Path to main DLL (for hot-reload)
    bool uses_iplugin = false;                             ///< True if using IPlugin interface

    /// Check if this plugin supports hot-reload
    [[nodiscard]] bool supports_hot_reload() const {
        return uses_iplugin && iplugin && iplugin->supports_hot_reload();
    }
};

// =============================================================================
// PluginPackageLoader
// =============================================================================

/// Loader for plugin.package files
class PluginPackageLoader : public PackageLoader {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    PluginPackageLoader() = default;

    // =========================================================================
    // PackageLoader Interface
    // =========================================================================

    [[nodiscard]] PackageType supported_type() const override {
        return PackageType::Plugin;
    }

    [[nodiscard]] const char* name() const override {
        return "PluginPackageLoader";
    }

    [[nodiscard]] void_core::Result<void> load(
        const ResolvedPackage& package,
        LoadContext& ctx) override;

    [[nodiscard]] void_core::Result<void> unload(
        const std::string& package_name,
        LoadContext& ctx) override;

    [[nodiscard]] bool supports_hot_reload() const override { return true; }

    [[nodiscard]] void_core::Result<void> reload(
        const ResolvedPackage& package,
        LoadContext& ctx) override;

    [[nodiscard]] bool is_loaded(const std::string& package_name) const override {
        return m_loaded_plugins.find(package_name) != m_loaded_plugins.end();
    }

    [[nodiscard]] std::vector<std::string> loaded_packages() const override {
        std::vector<std::string> names;
        names.reserve(m_loaded_plugins.size());
        for (const auto& [name, _] : m_loaded_plugins) {
            names.push_back(name);
        }
        return names;
    }

    // =========================================================================
    // Component Schema Registry
    // =========================================================================

    /// Get the component schema registry (returns external if set, otherwise internal)
    [[nodiscard]] ComponentSchemaRegistry& schema_registry() {
        return m_external_schema_registry ? *m_external_schema_registry : m_schema_registry;
    }
    [[nodiscard]] const ComponentSchemaRegistry& schema_registry() const {
        return m_external_schema_registry ? *m_external_schema_registry : m_schema_registry;
    }

    /// Set an external component schema registry to use instead of the internal one.
    /// This allows sharing a schema registry across the entire package system.
    /// The external registry MUST outlive this loader.
    void set_external_schema_registry(ComponentSchemaRegistry* registry) {
        m_external_schema_registry = registry;
    }

    /// Set the component schema registry's ECS registry (for backwards compatibility)
    void set_ecs_registry(void_ecs::ComponentRegistry* registry) {
        m_schema_registry.set_ecs_registry(registry);
        // If using external registry, also set on that
        if (m_external_schema_registry) {
            m_external_schema_registry->set_ecs_registry(registry);
        }
    }

    // =========================================================================
    // Definition Registry
    // =========================================================================

    /// Get the definition registry for data-driven content
    [[nodiscard]] DefinitionRegistry& definition_registry() { return m_definition_registry; }
    [[nodiscard]] const DefinitionRegistry& definition_registry() const { return m_definition_registry; }

    // =========================================================================
    // Library Cache
    // =========================================================================

    /// Get the dynamic library cache
    [[nodiscard]] DynamicLibraryCache& library_cache() { return m_library_cache; }
    [[nodiscard]] const DynamicLibraryCache& library_cache() const { return m_library_cache; }

private:
    // =========================================================================
    // Loading Steps
    // =========================================================================

    /// Register all components from a plugin manifest
    [[nodiscard]] void_core::Result<std::vector<void_ecs::ComponentId>> register_components(
        const PluginPackageManifest& manifest,
        LoadContext& ctx);

    /// Load all required dynamic libraries
    [[nodiscard]] void_core::Result<std::vector<std::filesystem::path>> load_libraries(
        const PluginPackageManifest& manifest);

    /// Register all systems from a plugin manifest
    [[nodiscard]] void_core::Result<std::vector<std::string>> register_systems(
        const PluginPackageManifest& manifest,
        LoadContext& ctx);

    /// Register all event handlers from a plugin manifest
    [[nodiscard]] void_core::Result<std::vector<std::string>> register_event_handlers(
        const PluginPackageManifest& manifest,
        LoadContext& ctx);

    /// Configure all data registries from a plugin manifest
    [[nodiscard]] void_core::Result<std::vector<std::string>> configure_registries(
        const PluginPackageManifest& manifest);

    // =========================================================================
    // System Creation
    // =========================================================================

    /// Create a system wrapper from a plugin system declaration
    [[nodiscard]] void_core::Result<std::unique_ptr<void_ecs::System>> create_plugin_system(
        const SystemDeclaration& decl,
        const PluginPackageManifest& manifest,
        LoadContext& ctx);

    // =========================================================================
    // IPlugin Lifecycle (Phase 3)
    // =========================================================================

    /// Try to load an IPlugin from a library
    /// Returns the IPlugin* if library exports plugin_create, nullptr otherwise
    [[nodiscard]] void_plugin_api::IPlugin* try_create_iplugin(DynamicLibrary* lib);

    /// Destroy an IPlugin instance via plugin_destroy
    void destroy_iplugin(DynamicLibrary* lib, void_plugin_api::IPlugin* plugin);

    /// Create PluginContext for an IPlugin
    [[nodiscard]] std::unique_ptr<void_plugin_api::PluginContext> create_plugin_context(
        const std::string& plugin_id,
        LoadContext& ctx);

    /// Populate RenderComponentIds from schema registry
    void populate_render_component_ids(void_plugin_api::PluginContext& ctx);

    /// Hot-reload a single plugin
    [[nodiscard]] void_core::Result<void> hot_reload_plugin(
        const std::string& package_name,
        LoadContext& ctx);

    // =========================================================================
    // Plugin State Tracking (Phase 4)
    // =========================================================================

    /// Get PluginRegistry from ECS world (may return nullptr if not available)
    [[nodiscard]] void_plugin_api::PluginRegistry* get_plugin_registry(LoadContext& ctx);

    /// Update PluginRegistry when a plugin starts loading
    void track_plugin_loading(LoadContext& ctx, const std::string& plugin_id,
                              const void_core::Version& version);

    /// Update PluginRegistry when a plugin finishes loading successfully
    void track_plugin_loaded(LoadContext& ctx, const std::string& plugin_id,
                             const LoadedPluginState& state);

    /// Update PluginRegistry when a plugin fails to load
    void track_plugin_failed(LoadContext& ctx, const std::string& plugin_id,
                             const std::string& error);

    /// Update PluginRegistry when a plugin is unloaded
    void track_plugin_unloaded(LoadContext& ctx, const std::string& plugin_id);

    /// Update PluginRegistry for hot-reload status
    void track_plugin_reloading(LoadContext& ctx, const std::string& plugin_id);
    void track_plugin_reloaded(LoadContext& ctx, const std::string& plugin_id);

    // =========================================================================
    // Data Members
    // =========================================================================

    std::map<std::string, LoadedPluginState> m_loaded_plugins;
    ComponentSchemaRegistry m_schema_registry;           ///< Internal fallback registry
    ComponentSchemaRegistry* m_external_schema_registry = nullptr;  ///< External shared registry (if set)
    DefinitionRegistry m_definition_registry;
    DynamicLibraryCache m_library_cache;
    void_kernel::Kernel* m_kernel = nullptr;             ///< Kernel for system registration (optional)

public:
    /// Set the kernel for IPlugin system registration
    void set_kernel(void_kernel::Kernel* kernel) { m_kernel = kernel; }
};

// =============================================================================
// PluginSystem - Wrapper for dynamically loaded systems
// =============================================================================

/// System implementation that wraps a dynamically loaded function
class PluginSystem : public void_ecs::System {
public:
    PluginSystem(void_ecs::SystemDescriptor desc, PluginSystemFn fn)
        : m_descriptor(std::move(desc))
        , m_function(fn)
    {}

    [[nodiscard]] const void_ecs::SystemDescriptor& descriptor() const override {
        return m_descriptor;
    }

    void run(void_ecs::World& world) override {
        if (m_function) {
            m_function(world);
        }
    }

private:
    void_ecs::SystemDescriptor m_descriptor;
    PluginSystemFn m_function;
};

// =============================================================================
// PluginPackageLoader Implementation
// =============================================================================

void_core::Result<void> PluginPackageLoader::load(
    const ResolvedPackage& package,
    LoadContext& ctx)
{
    // Check if already loaded
    if (is_loaded(package.manifest.name)) {
        return void_core::Error("Plugin already loaded: " + package.manifest.name);
    }

    // Load and parse the manifest from the package path
    auto manifest_result = PluginPackageManifest::load(package.manifest.source_path);
    if (!manifest_result) {
        return void_core::Error("Failed to load plugin manifest: " + manifest_result.error().message());
    }

    auto& manifest = *manifest_result;

    // Validate the manifest
    auto validate_result = manifest.validate();
    if (!validate_result) {
        return void_core::Error("Plugin validation failed: " + validate_result.error().message());
    }

    spdlog::info("[PluginPackageLoader] Loading plugin: {}", package.manifest.name);

    // Track loading in PluginRegistry (Phase 4)
    // Convert SemanticVersion to void_core::Version
    void_core::Version core_version{
        static_cast<std::uint16_t>(manifest.base.version.major),
        static_cast<std::uint16_t>(manifest.base.version.minor),
        static_cast<std::uint16_t>(manifest.base.version.patch)
    };
    track_plugin_loading(ctx, package.manifest.name, core_version);

    // Initialize state tracking
    LoadedPluginState state;
    state.name = package.manifest.name;
    state.manifest = manifest;

    // Step 1: Register components (from manifest - for both legacy and IPlugin modes)
    auto comp_result = register_components(manifest, ctx);
    if (!comp_result) {
        std::string error = "Failed to register components: " + comp_result.error().message();
        track_plugin_failed(ctx, package.manifest.name, error);
        return void_core::Error(error);
    }
    state.registered_components = std::move(*comp_result);

    // Step 2: Load dynamic libraries
    auto lib_result = load_libraries(manifest);
    if (!lib_result) {
        std::string error = "Failed to load libraries: " + lib_result.error().message();
        track_plugin_failed(ctx, package.manifest.name, error);
        return void_core::Error(error);
    }
    state.loaded_libraries = std::move(*lib_result);

    // Step 3: Check for IPlugin interface in loaded libraries
    // Try each library to see if it exports plugin_create
    void_plugin_api::IPlugin* iplugin = nullptr;
    DynamicLibrary* main_lib = nullptr;

    for (const auto& lib_path : state.loaded_libraries) {
        auto* lib = m_library_cache.get(lib_path);
        if (lib && lib->has_symbol("plugin_create")) {
            iplugin = try_create_iplugin(lib);
            if (iplugin) {
                main_lib = lib;
                state.main_library_path = lib_path;
                break;
            }
        }
    }

    if (iplugin) {
        // IPlugin-based loading (Phase 3)
        spdlog::info("[PluginPackageLoader] Plugin '{}' exports IPlugin interface (id: {}, version: {}.{}.{})",
                     package.manifest.name,
                     iplugin->id(),
                     iplugin->version().major,
                     iplugin->version().minor,
                     iplugin->version().patch);

        state.uses_iplugin = true;
        state.iplugin = iplugin;

        // Create PluginContext for the plugin
        state.context = create_plugin_context(iplugin->id(), ctx);
        if (!state.context) {
            destroy_iplugin(main_lib, iplugin);
            std::string error = "Failed to create PluginContext for plugin: " + package.manifest.name;
            track_plugin_failed(ctx, package.manifest.name, error);
            return void_core::Error(error);
        }

        // Populate render component IDs so plugin can use make_renderable()
        populate_render_component_ids(*state.context);

        // Call on_load() - this is where the plugin registers its components, systems, etc.
        auto load_result = iplugin->on_load(*state.context);
        if (!load_result) {
            spdlog::error("[PluginPackageLoader] Plugin '{}' on_load() failed: {}",
                         package.manifest.name, load_result.error().message());
            destroy_iplugin(main_lib, iplugin);
            state.iplugin = nullptr;
            state.context.reset();
            std::string error = "Plugin on_load() failed: " + load_result.error().message();
            track_plugin_failed(ctx, package.manifest.name, error);
            return void_core::Error(error);
        }

        spdlog::info("[PluginPackageLoader] Plugin '{}' loaded via IPlugin interface", package.manifest.name);

    } else {
        // Legacy manifest-based loading
        spdlog::info("[PluginPackageLoader] Plugin '{}' using manifest-based loading (no IPlugin)",
                     package.manifest.name);

        // Step 3 (legacy): Register systems from manifest
        auto sys_result = register_systems(manifest, ctx);
        if (!sys_result) {
            std::string error = "Failed to register systems: " + sys_result.error().message();
            track_plugin_failed(ctx, package.manifest.name, error);
            return void_core::Error(error);
        }
        state.registered_systems = std::move(*sys_result);

        // Step 4 (legacy): Register event handlers from manifest
        auto handler_result = register_event_handlers(manifest, ctx);
        if (!handler_result) {
            std::string error = "Failed to register event handlers: " + handler_result.error().message();
            track_plugin_failed(ctx, package.manifest.name, error);
            return void_core::Error(error);
        }
        state.registered_event_handlers = std::move(*handler_result);
    }

    // Step 5: Configure registries (both modes)
    auto reg_result = configure_registries(manifest);
    if (!reg_result) {
        std::string error = "Failed to configure registries: " + reg_result.error().message();
        track_plugin_failed(ctx, package.manifest.name, error);
        return void_core::Error(error);
    }
    state.configured_registries = std::move(*reg_result);

    // Store loaded plugin state
    m_loaded_plugins[package.manifest.name] = std::move(state);

    // Track successful load in PluginRegistry (Phase 4)
    track_plugin_loaded(ctx, package.manifest.name, m_loaded_plugins[package.manifest.name]);

    spdlog::info("[PluginPackageLoader] Plugin '{}' loaded successfully", package.manifest.name);
    return void_core::Ok();
}

void_core::Result<void> PluginPackageLoader::unload(
    const std::string& package_name,
    LoadContext& ctx)
{
    auto it = m_loaded_plugins.find(package_name);
    if (it == m_loaded_plugins.end()) {
        return void_core::Error("Plugin not loaded: " + package_name);
    }

    auto& state = it->second;

    spdlog::info("[PluginPackageLoader] Unloading plugin: {}", package_name);

    // Unload in reverse order of loading:

    if (state.uses_iplugin && state.iplugin && state.context) {
        // IPlugin-based unloading

        // 1. Unregister all systems registered by the plugin
        state.context->unregister_all_systems();

        // 2. Unsubscribe from all events
        state.context->unsubscribe_all();

        // 3. Call on_unload() - plugin cleans up its resources
        auto unload_result = state.iplugin->on_unload(*state.context);
        if (!unload_result) {
            spdlog::warn("[PluginPackageLoader] Plugin '{}' on_unload() failed: {} (continuing cleanup)",
                        package_name, unload_result.error().message());
            // Continue cleanup even if on_unload fails
        }

        // 4. Destroy the IPlugin instance via plugin_destroy
        auto* lib = m_library_cache.get(state.main_library_path);
        if (lib) {
            destroy_iplugin(lib, state.iplugin);
        }
        state.iplugin = nullptr;

        // 5. Release the context
        state.context.reset();

    } else {
        // Legacy manifest-based unloading

        // 1. Event handlers - would need event bus access to unregister
        // (Not fully implemented - requires event system integration)

        // 2. Systems - would need to remove from scheduler
        // (Not fully implemented - scheduler doesn't support removal)
    }

    // Common cleanup for both modes:

    // Unload libraries from cache (must happen AFTER plugin_destroy)
    for (const auto& lib_path : state.loaded_libraries) {
        m_library_cache.unload(lib_path);
    }

    // Unregister component schemas from the shared registry
    for (const auto& comp : state.manifest.components) {
        (void)schema_registry().unregister_schema(comp.name);
    }

    // Track unload in PluginRegistry (Phase 4)
    track_plugin_unloaded(ctx, package_name);

    // Remove from loaded plugins
    m_loaded_plugins.erase(it);

    spdlog::info("[PluginPackageLoader] Plugin '{}' unloaded", package_name);
    return void_core::Ok();
}

void_core::Result<std::vector<void_ecs::ComponentId>> PluginPackageLoader::register_components(
    const PluginPackageManifest& manifest,
    LoadContext& ctx)
{
    std::vector<void_ecs::ComponentId> registered;

    spdlog::info("[PluginPackageLoader] Registering {} components from plugin '{}'",
                 manifest.components.size(), manifest.base.name);

    // Diagnostic: Check if using external or internal registry
    spdlog::info("[PluginPackageLoader] Using {} schema registry (address: {})",
                 m_external_schema_registry ? "EXTERNAL" : "internal",
                 m_external_schema_registry ? static_cast<void*>(m_external_schema_registry)
                                            : static_cast<void*>(&m_schema_registry));

    // Set up ECS registry if we have a world (needed for component ID allocation)
    if (ctx.ecs_world()) {
        spdlog::info("[PluginPackageLoader] Setting ECS registry from context's world");
        schema_registry().set_ecs_registry(&ctx.ecs_world()->component_registry_mut());
    } else {
        spdlog::warn("[PluginPackageLoader] No ECS world in context - component IDs may not allocate properly");
    }

    for (const auto& comp_decl : manifest.components) {
        // Convert declaration to schema
        auto schema_result = comp_decl.to_component_schema(manifest.base.name);
        if (!schema_result) {
            spdlog::error("[PluginPackageLoader] Failed to create schema for component '{}': {}",
                         comp_decl.name, schema_result.error().message());
            return void_core::Error("Component '" + comp_decl.name + "': " + schema_result.error().message());
        }

        spdlog::info("[PluginPackageLoader] Registering component schema '{}' ({} fields, {} bytes)",
                     comp_decl.name, schema_result->fields.size(), schema_result->size);

        // Register schema to the shared registry (external if set, internal otherwise)
        auto id_result = schema_registry().register_schema(std::move(*schema_result));
        if (!id_result) {
            spdlog::error("[PluginPackageLoader] Failed to register component '{}': {}",
                         comp_decl.name, id_result.error().message());
            return void_core::Error("Failed to register component '" + comp_decl.name + "': " +
                id_result.error().message());
        }

        spdlog::info("[PluginPackageLoader] Successfully registered component '{}' with ID {}",
                     comp_decl.name, id_result->value());
        registered.push_back(*id_result);
    }

    // Diagnostic: List what's now in the schema registry
    auto all_schemas = schema_registry().all_schema_names();
    spdlog::info("[PluginPackageLoader] Schema registry now has {} schemas: {}",
                 all_schemas.size(),
                 [&all_schemas]() {
                     std::string list;
                     for (std::size_t i = 0; i < all_schemas.size() && i < 10; ++i) {
                         if (i > 0) list += ", ";
                         list += all_schemas[i];
                     }
                     if (all_schemas.size() > 10) list += "...";
                     return list;
                 }());

    return registered;
}

void_core::Result<std::vector<std::filesystem::path>> PluginPackageLoader::load_libraries(
    const PluginPackageManifest& manifest)
{
    std::vector<std::filesystem::path> loaded;

    // Use manifest.libraries which includes main_library (if specified) plus
    // all libraries from systems and event_handlers
    for (const auto& lib_path : manifest.libraries) {
        // Ensure correct extension
        auto full_path = with_library_extension(lib_path);

        // Check if file exists
        if (!std::filesystem::exists(full_path)) {
            return void_core::Error("Library not found: " + full_path.string());
        }

        // Load into cache
        auto lib_result = m_library_cache.get_or_load(full_path);
        if (!lib_result) {
            return void_core::Error("Failed to load library: " + lib_result.error().message());
        }

        loaded.push_back(full_path);
    }

    return loaded;
}

void_core::Result<std::vector<std::string>> PluginPackageLoader::register_systems(
    const PluginPackageManifest& manifest,
    LoadContext& ctx)
{
    std::vector<std::string> registered;

    if (!ctx.ecs_world()) {
        // No world available - skip system registration but succeed
        return registered;
    }

    for (const auto& sys_decl : manifest.systems) {
        auto system_result = create_plugin_system(sys_decl, manifest, ctx);
        if (!system_result) {
            return void_core::Error("System '" + sys_decl.name + "': " + system_result.error().message());
        }

        ctx.ecs_world()->add_system(std::move(*system_result));
        registered.push_back(sys_decl.name);
    }

    return registered;
}

void_core::Result<std::unique_ptr<void_ecs::System>> PluginPackageLoader::create_plugin_system(
    const SystemDeclaration& decl,
    const PluginPackageManifest& manifest,
    LoadContext& ctx)
{
    // Resolve library path
    auto lib_path = with_library_extension(manifest.resolve_library_path(decl.library));

    // Get library from cache
    auto* lib = m_library_cache.get(lib_path);
    if (!lib) {
        return void_core::Error("Library not loaded: " + lib_path.string());
    }

    // Look up entry point function
    auto fn_result = lib->get_function<PluginSystemFn>(decl.entry_point);
    if (!fn_result) {
        return void_core::Error("Entry point not found: " + decl.entry_point +
            " in " + lib_path.string());
    }

    // Get system stage
    auto stage_result = decl.get_stage();
    if (!stage_result) {
        return void_core::Error(stage_result.error().message());
    }

    // Build system descriptor
    void_ecs::SystemDescriptor desc(decl.name);
    desc.set_stage(*stage_result);

    if (decl.exclusive) {
        desc.set_exclusive();
    }

    // Build query from component names
    void_ecs::QueryDescriptor query_desc;

    for (const auto& comp_name : decl.query) {
        // Look up component ID by name
        auto comp_id = m_schema_registry.get_component_id(comp_name);
        if (!comp_id) {
            // Try ECS registry directly
            if (ctx.ecs_world()) {
                comp_id = ctx.ecs_world()->component_registry().get_id_by_name(comp_name);
            }
        }

        if (!comp_id) {
            return void_core::Error("Unknown component in query: " + comp_name);
        }

        query_desc.write(*comp_id);  // Default to write access
    }

    for (const auto& comp_name : decl.exclude) {
        auto comp_id = m_schema_registry.get_component_id(comp_name);
        if (!comp_id) {
            if (ctx.ecs_world()) {
                comp_id = ctx.ecs_world()->component_registry().get_id_by_name(comp_name);
            }
        }

        if (!comp_id) {
            return void_core::Error("Unknown component in exclude: " + comp_name);
        }

        query_desc.without(*comp_id);
    }

    desc.add_query(std::move(query_desc));

    // Add ordering constraints
    for (const auto& after_name : decl.run_after) {
        desc.after(void_ecs::SystemId::from_name(after_name));
    }
    for (const auto& before_name : decl.run_before) {
        desc.before(void_ecs::SystemId::from_name(before_name));
    }

    return std::unique_ptr<void_ecs::System>(
        std::make_unique<PluginSystem>(std::move(desc), *fn_result));
}

void_core::Result<std::vector<std::string>> PluginPackageLoader::register_event_handlers(
    const PluginPackageManifest& manifest,
    LoadContext& ctx)
{
    std::vector<std::string> registered;

    // Event handler registration requires event bus
    if (!ctx.event_bus()) {
        // No event bus - skip but succeed
        return registered;
    }

    for (const auto& handler_decl : manifest.event_handlers) {
        // Resolve library
        auto lib_path = with_library_extension(manifest.resolve_library_path(handler_decl.library));
        auto* lib = m_library_cache.get(lib_path);
        if (!lib) {
            return void_core::Error("Library not loaded for event handler: " + lib_path.string());
        }

        // Look up handler function
        auto fn_result = lib->get_function<PluginEventHandlerFn>(handler_decl.handler);
        if (!fn_result) {
            return void_core::Error("Handler not found: " + handler_decl.handler +
                " in " + lib_path.string());
        }

        // Event bus registration would happen here
        // ctx.event_bus()->subscribe(handler_decl.event, *fn_result, handler_decl.priority);

        registered.push_back(handler_decl.event + ":" + handler_decl.handler);
    }

    return registered;
}

void_core::Result<std::vector<std::string>> PluginPackageLoader::configure_registries(
    const PluginPackageManifest& manifest)
{
    std::vector<std::string> configured;

    for (const auto& reg_decl : manifest.registries) {
        auto config_result = reg_decl.to_registry_config();
        if (!config_result) {
            return void_core::Error("Registry '" + reg_decl.name + "': " + config_result.error().message());
        }

        m_definition_registry.configure_type(reg_decl.name, std::move(*config_result));
        configured.push_back(reg_decl.name);
    }

    return configured;
}

// =============================================================================
// IPlugin Lifecycle Implementation (Phase 3)
// =============================================================================

void_plugin_api::IPlugin* PluginPackageLoader::try_create_iplugin(DynamicLibrary* lib) {
    if (!lib) return nullptr;

    // Try to get plugin_create function
    auto create_result = lib->get_function<void_plugin_api::PluginCreateFunc>("plugin_create");
    if (!create_result) {
        return nullptr;
    }

    // Call plugin_create to instantiate the plugin
    auto create_fn = *create_result;
    void_plugin_api::IPlugin* plugin = create_fn();

    if (!plugin) {
        spdlog::warn("[PluginPackageLoader] plugin_create() returned nullptr");
        return nullptr;
    }

    // Optionally check API version compatibility
    auto version_result = lib->get_function<void_plugin_api::PluginApiVersionFunc>("plugin_api_version");
    if (version_result) {
        const char* api_version = (*version_result)();
        spdlog::debug("[PluginPackageLoader] Plugin API version: {}", api_version ? api_version : "unknown");
    }

    return plugin;
}

void PluginPackageLoader::destroy_iplugin(DynamicLibrary* lib, void_plugin_api::IPlugin* plugin) {
    if (!lib || !plugin) return;

    auto destroy_result = lib->get_function<void_plugin_api::PluginDestroyFunc>("plugin_destroy");
    if (destroy_result) {
        (*destroy_result)(plugin);
    } else {
        // Fallback: delete directly (not recommended but better than leaking)
        spdlog::warn("[PluginPackageLoader] plugin_destroy not found, using delete");
        delete plugin;
    }
}

std::unique_ptr<void_plugin_api::PluginContext> PluginPackageLoader::create_plugin_context(
    const std::string& plugin_id,
    LoadContext& ctx)
{
    // Get required references from LoadContext
    void_ecs::World* world = ctx.ecs_world();
    void_event::EventBus* events = ctx.event_bus();

    // Kernel is needed for system registration
    // Try to get from LoadContext if it provides one, otherwise use stored kernel
    void_kernel::IKernel* kernel = m_kernel;

    // Get the schema registry (external or internal)
    void_package::ComponentSchemaRegistry* schema = m_external_schema_registry
        ? m_external_schema_registry
        : &m_schema_registry;

    // Create the context
    auto context = std::make_unique<void_plugin_api::PluginContext>(
        plugin_id,
        world,
        kernel,
        events,
        schema
    );

    return context;
}

void PluginPackageLoader::populate_render_component_ids(void_plugin_api::PluginContext& ctx) {
    void_plugin_api::RenderComponentIds ids;

    // Look up engine render component IDs from the schema registry
    auto& registry = schema_registry();

    auto transform_id = registry.get_component_id("Transform");
    if (transform_id) ids.transform = *transform_id;

    auto mesh_id = registry.get_component_id("Mesh");
    if (mesh_id) ids.mesh = *mesh_id;

    auto material_id = registry.get_component_id("Material");
    if (material_id) ids.material = *material_id;

    auto light_id = registry.get_component_id("Light");
    if (light_id) ids.light = *light_id;

    auto camera_id = registry.get_component_id("Camera");
    if (camera_id) ids.camera = *camera_id;

    auto renderable_id = registry.get_component_id("Renderable");
    if (renderable_id) ids.renderable_tag = *renderable_id;

    auto hierarchy_id = registry.get_component_id("Hierarchy");
    if (hierarchy_id) ids.hierarchy = *hierarchy_id;

    ctx.set_render_component_ids(ids);

    if (!ids.is_complete()) {
        spdlog::warn("[PluginPackageLoader] Not all render components are registered. "
                     "make_renderable() may not work correctly.");
    }
}

void_core::Result<void> PluginPackageLoader::hot_reload_plugin(
    const std::string& package_name,
    LoadContext& ctx)
{
    auto it = m_loaded_plugins.find(package_name);
    if (it == m_loaded_plugins.end()) {
        return void_core::Error("Plugin not loaded: " + package_name);
    }

    auto& state = it->second;

    // Only IPlugin-based plugins support hot-reload
    if (!state.uses_iplugin || !state.iplugin) {
        return void_core::Error("Plugin does not support hot-reload: " + package_name);
    }

    if (!state.iplugin->supports_hot_reload()) {
        return void_core::Error("Plugin explicitly disabled hot-reload: " + package_name);
    }

    spdlog::info("[PluginPackageLoader] Hot-reloading plugin: {}", package_name);

    // Track reloading status (Phase 4)
    track_plugin_reloading(ctx, package_name);

    // Step 1: Capture state snapshot
    auto snapshot = state.iplugin->snapshot();
    spdlog::debug("[PluginPackageLoader] Captured snapshot ({} bytes, type: {})",
                  snapshot.data.size(), snapshot.type_name);

    // Step 2: Call on_unload()
    state.context->unregister_all_systems();
    state.context->unsubscribe_all();

    auto unload_result = state.iplugin->on_unload(*state.context);
    if (!unload_result) {
        spdlog::warn("[PluginPackageLoader] on_unload() failed during hot-reload: {}",
                    unload_result.error().message());
        // Continue with reload attempt
    }

    // Step 3: Destroy old plugin instance
    auto* lib = m_library_cache.get(state.main_library_path);
    if (lib) {
        destroy_iplugin(lib, state.iplugin);
    }
    state.iplugin = nullptr;
    state.context.reset();

    // Step 4: Unload old DLL
    m_library_cache.unload(state.main_library_path);

    // Step 5: Load new DLL
    auto lib_result = m_library_cache.get_or_load(state.main_library_path);
    if (!lib_result) {
        spdlog::error("[PluginPackageLoader] Failed to reload library: {}",
                     lib_result.error().message());
        // TODO: Attempt rollback
        return void_core::Error("Failed to reload library: " + lib_result.error().message());
    }
    lib = *lib_result;

    // Step 6: Create new plugin instance
    state.iplugin = try_create_iplugin(lib);
    if (!state.iplugin) {
        spdlog::error("[PluginPackageLoader] plugin_create() failed after hot-reload");
        return void_core::Error("plugin_create() failed after hot-reload");
    }

    // Step 7: Create new context and call on_load()
    state.context = create_plugin_context(state.iplugin->id(), ctx);
    if (!state.context) {
        destroy_iplugin(lib, state.iplugin);
        state.iplugin = nullptr;
        return void_core::Error("Failed to create PluginContext after hot-reload");
    }

    populate_render_component_ids(*state.context);

    auto load_result = state.iplugin->on_load(*state.context);
    if (!load_result) {
        spdlog::error("[PluginPackageLoader] on_load() failed after hot-reload: {}",
                     load_result.error().message());
        destroy_iplugin(lib, state.iplugin);
        state.iplugin = nullptr;
        state.context.reset();
        return void_core::Error("on_load() failed after hot-reload: " + load_result.error().message());
    }

    // Step 8: Restore state from snapshot
    auto restore_result = state.iplugin->restore(snapshot);
    if (!restore_result) {
        spdlog::warn("[PluginPackageLoader] restore() failed: {} (plugin will start fresh)",
                    restore_result.error().message());
        // Continue - plugin will just start with fresh state
    }

    // Step 9: Notify plugin that reload completed
    state.iplugin->on_reloaded();

    // Track successful reload (Phase 4)
    track_plugin_reloaded(ctx, package_name);

    spdlog::info("[PluginPackageLoader] Hot-reload complete for plugin: {}", package_name);
    return void_core::Ok();
}

// =============================================================================
// Hot-Reload Interface
// =============================================================================

void_core::Result<void> PluginPackageLoader::reload(
    const ResolvedPackage& package,
    LoadContext& ctx)
{
    return hot_reload_plugin(package.manifest.name, ctx);
}

// =============================================================================
// Plugin State Tracking (Phase 4)
// =============================================================================

void_plugin_api::PluginRegistry* PluginPackageLoader::get_plugin_registry(LoadContext& ctx) {
    if (!ctx.ecs_world()) {
        return nullptr;
    }
    return ctx.ecs_world()->resource<void_plugin_api::PluginRegistry>();
}

void PluginPackageLoader::track_plugin_loading(LoadContext& ctx, const std::string& plugin_id,
                                                const void_core::Version& version) {
    auto* registry = get_plugin_registry(ctx);
    if (!registry) {
        spdlog::debug("[PluginPackageLoader] No PluginRegistry available for tracking");
        return;
    }

    // Create initial state
    auto state = void_plugin_api::PluginState::loading(plugin_id, version);
    registry->add(std::move(state));

    spdlog::debug("[PluginPackageLoader] Tracking plugin '{}' as Loading", plugin_id);
}

void PluginPackageLoader::track_plugin_loaded(LoadContext& ctx, const std::string& plugin_id,
                                               const LoadedPluginState& loaded_state) {
    auto* registry = get_plugin_registry(ctx);
    if (!registry) return;

    auto* state = registry->get(plugin_id);
    if (!state) {
        spdlog::warn("[PluginPackageLoader] Cannot track load: plugin '{}' not in registry", plugin_id);
        return;
    }

    // Update state with registration info
    state->status = void_plugin_api::PluginStatus::Active;
    state->library_path = loaded_state.main_library_path.string();

    // Copy registration lists from manifest
    for (const auto& comp : loaded_state.manifest.components) {
        state->registered_components.push_back(comp.name);
    }
    for (const auto& sys : loaded_state.manifest.systems) {
        state->registered_systems.push_back(sys.name);
    }

    // If using IPlugin, also get info from the plugin itself
    if (loaded_state.uses_iplugin && loaded_state.iplugin) {
        state->description = loaded_state.iplugin->description();
        state->author = loaded_state.iplugin->author();

        // Get dependencies from IPlugin
        for (const auto& dep : loaded_state.iplugin->dependencies()) {
            state->dependencies.push_back(dep.name);
        }

        // Override component/system lists with what plugin reports
        auto plugin_components = loaded_state.iplugin->component_names();
        if (!plugin_components.empty()) {
            state->registered_components = std::move(plugin_components);
        }

        auto plugin_systems = loaded_state.iplugin->system_names();
        if (!plugin_systems.empty()) {
            state->registered_systems = std::move(plugin_systems);
        }
    }

    // Rebuild dependents graph
    registry->rebuild_dependents();

    spdlog::debug("[PluginPackageLoader] Plugin '{}' tracked as Active ({} components, {} systems)",
                  plugin_id, state->registered_components.size(), state->registered_systems.size());
}

void PluginPackageLoader::track_plugin_failed(LoadContext& ctx, const std::string& plugin_id,
                                               const std::string& error) {
    auto* registry = get_plugin_registry(ctx);
    if (!registry) return;

    registry->set_failed(plugin_id, error);
    spdlog::debug("[PluginPackageLoader] Plugin '{}' tracked as Failed: {}", plugin_id, error);
}

void PluginPackageLoader::track_plugin_unloaded(LoadContext& ctx, const std::string& plugin_id) {
    auto* registry = get_plugin_registry(ctx);
    if (!registry) return;

    registry->remove(plugin_id);
    spdlog::debug("[PluginPackageLoader] Plugin '{}' removed from registry", plugin_id);
}

void PluginPackageLoader::track_plugin_reloading(LoadContext& ctx, const std::string& plugin_id) {
    auto* registry = get_plugin_registry(ctx);
    if (!registry) return;

    registry->set_status(plugin_id, void_plugin_api::PluginStatus::Reloading);
    spdlog::debug("[PluginPackageLoader] Plugin '{}' tracked as Reloading", plugin_id);
}

void PluginPackageLoader::track_plugin_reloaded(LoadContext& ctx, const std::string& plugin_id) {
    auto* registry = get_plugin_registry(ctx);
    if (!registry) return;

    registry->mark_reloaded(plugin_id);
    spdlog::debug("[PluginPackageLoader] Plugin '{}' tracked as Reloaded", plugin_id);
}

// =============================================================================
// Factory Function
// =============================================================================

/// Create a plugin package loader with no external registry (uses internal)
std::unique_ptr<PackageLoader> create_plugin_package_loader() {
    return std::make_unique<PluginPackageLoader>();
}

/// Create a plugin package loader with an external schema registry.
/// This allows sharing a component schema registry across the package system.
std::unique_ptr<PackageLoader> create_plugin_package_loader(ComponentSchemaRegistry* schema_registry) {
    auto loader = std::make_unique<PluginPackageLoader>();
    if (schema_registry) {
        loader->set_external_schema_registry(schema_registry);
    }
    return loader;
}

/// Create a plugin package loader with external schema registry and kernel.
/// Full-featured factory for IPlugin support with system registration.
std::unique_ptr<PackageLoader> create_plugin_package_loader(
    ComponentSchemaRegistry* schema_registry,
    void_kernel::Kernel* kernel)
{
    auto loader = std::make_unique<PluginPackageLoader>();
    if (schema_registry) {
        loader->set_external_schema_registry(schema_registry);
    }
    if (kernel) {
        loader->set_kernel(kernel);
    }
    return loader;
}

} // namespace void_package
