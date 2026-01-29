/// @file plugin_package_loader.cpp
/// @brief Plugin package loader implementation
///
/// Loads plugin packages by:
/// 1. Parsing the plugin manifest
/// 2. Registering components to the ECS
/// 3. Loading dynamic libraries
/// 4. Registering systems from loaded libraries
/// 5. Setting up event handlers
/// 6. Configuring data registries

#include <void_engine/package/loader.hpp>
#include <void_engine/package/plugin_package.hpp>
#include <void_engine/package/dynamic_library.hpp>
#include <void_engine/package/component_schema.hpp>
#include <void_engine/package/definition_registry.hpp>

#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/component.hpp>
#include <void_engine/ecs/system.hpp>

#include <map>
#include <set>

namespace void_package {

// =============================================================================
// LoadedPluginState
// =============================================================================

/// State for a loaded plugin, used for unloading
struct LoadedPluginState {
    std::string name;
    PluginPackageManifest manifest;
    std::vector<void_ecs::ComponentId> registered_components;
    std::vector<std::string> registered_systems;
    std::vector<std::string> registered_event_handlers;
    std::vector<std::string> configured_registries;
    std::vector<std::filesystem::path> loaded_libraries;
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

    /// Get the component schema registry
    [[nodiscard]] ComponentSchemaRegistry& schema_registry() { return m_schema_registry; }
    [[nodiscard]] const ComponentSchemaRegistry& schema_registry() const { return m_schema_registry; }

    /// Set the component schema registry's ECS registry
    void set_ecs_registry(void_ecs::ComponentRegistry* registry) {
        m_schema_registry.set_ecs_registry(registry);
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
    // Data Members
    // =========================================================================

    std::map<std::string, LoadedPluginState> m_loaded_plugins;
    ComponentSchemaRegistry m_schema_registry;
    DefinitionRegistry m_definition_registry;
    DynamicLibraryCache m_library_cache;
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

    // Initialize state tracking
    LoadedPluginState state;
    state.name = package.manifest.name;
    state.manifest = manifest;

    // Step 1: Register components
    auto comp_result = register_components(manifest, ctx);
    if (!comp_result) {
        return void_core::Error("Failed to register components: " + comp_result.error().message());
    }
    state.registered_components = std::move(*comp_result);

    // Step 2: Load dynamic libraries
    auto lib_result = load_libraries(manifest);
    if (!lib_result) {
        // Cleanup on failure
        // Components are difficult to unregister, but we track what we registered
        return void_core::Error("Failed to load libraries: " + lib_result.error().message());
    }
    state.loaded_libraries = std::move(*lib_result);

    // Step 3: Register systems
    auto sys_result = register_systems(manifest, ctx);
    if (!sys_result) {
        return void_core::Error("Failed to register systems: " + sys_result.error().message());
    }
    state.registered_systems = std::move(*sys_result);

    // Step 4: Register event handlers
    auto handler_result = register_event_handlers(manifest, ctx);
    if (!handler_result) {
        return void_core::Error("Failed to register event handlers: " + handler_result.error().message());
    }
    state.registered_event_handlers = std::move(*handler_result);

    // Step 5: Configure registries
    auto reg_result = configure_registries(manifest);
    if (!reg_result) {
        return void_core::Error("Failed to configure registries: " + reg_result.error().message());
    }
    state.configured_registries = std::move(*reg_result);

    // Store loaded plugin state
    m_loaded_plugins[package.manifest.name] = std::move(state);

    return void_core::Ok();
}

void_core::Result<void> PluginPackageLoader::unload(
    const std::string& package_name,
    LoadContext& /*ctx*/)
{
    auto it = m_loaded_plugins.find(package_name);
    if (it == m_loaded_plugins.end()) {
        return void_core::Error("Plugin not loaded: " + package_name);
    }

    const auto& state = it->second;

    // Unload in reverse order of loading:
    // 1. Configured registries - just clear configuration, definitions remain
    // (Registry data persists for now - full cleanup would require tracking)

    // 2. Event handlers - would need event bus access to unregister
    // (Not implemented yet - requires event system integration)

    // 3. Systems - would need to remove from scheduler
    // (Not implemented yet - scheduler doesn't support removal)

    // 4. Libraries - unload from cache
    for (const auto& lib_path : state.loaded_libraries) {
        m_library_cache.unload(lib_path);
    }

    // 5. Components - schemas can be unregistered
    for (const auto& comp : state.manifest.components) {
        (void)m_schema_registry.unregister_schema(comp.name);
    }

    // Remove from loaded plugins
    m_loaded_plugins.erase(it);

    return void_core::Ok();
}

void_core::Result<std::vector<void_ecs::ComponentId>> PluginPackageLoader::register_components(
    const PluginPackageManifest& manifest,
    LoadContext& ctx)
{
    std::vector<void_ecs::ComponentId> registered;

    // Set up ECS registry if we have a world
    if (ctx.ecs_world()) {
        m_schema_registry.set_ecs_registry(&ctx.ecs_world()->component_registry_mut());
    }

    for (const auto& comp_decl : manifest.components) {
        // Convert declaration to schema
        auto schema_result = comp_decl.to_component_schema(manifest.base.name);
        if (!schema_result) {
            return void_core::Error("Component '" + comp_decl.name + "': " + schema_result.error().message());
        }

        // Register schema
        auto id_result = m_schema_registry.register_schema(std::move(*schema_result));
        if (!id_result) {
            return void_core::Error("Failed to register component '" + comp_decl.name + "': " +
                id_result.error().message());
        }

        registered.push_back(*id_result);
    }

    return registered;
}

void_core::Result<std::vector<std::filesystem::path>> PluginPackageLoader::load_libraries(
    const PluginPackageManifest& manifest)
{
    std::vector<std::filesystem::path> loaded;

    auto lib_paths = manifest.collect_library_paths();
    for (const auto& lib_path : lib_paths) {
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
// Factory Function
// =============================================================================

/// Create a plugin package loader
std::unique_ptr<PackageLoader> create_plugin_package_loader() {
    return std::make_unique<PluginPackageLoader>();
}

} // namespace void_package
