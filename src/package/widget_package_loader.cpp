/// @file widget_package_loader.cpp
/// @brief Widget package loader implementation
///
/// Loads widget packages by:
/// 1. Parsing the widget manifest
/// 2. Registering custom widget types from libraries
/// 3. Creating widgets based on type (builtin or library)
/// 4. Setting up ECS bindings (queries from component names)
/// 5. Filtering by build type (debug/development/release)

#include <void_engine/package/loader.hpp>
#include <void_engine/package/widget_package.hpp>
#include <void_engine/package/widget_manager.hpp>
#include <void_engine/package/widget.hpp>
#include <void_engine/package/component_schema.hpp>
#include <void_engine/package/dynamic_library.hpp>

#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/component.hpp>

#include <map>
#include <set>

namespace void_package {

// =============================================================================
// LoadedWidgetState
// =============================================================================

/// State for a loaded widget package, used for unloading
struct LoadedWidgetState {
    std::string name;
    WidgetPackageManifest manifest;
    std::vector<std::string> registered_widgets;
    std::vector<std::string> registered_types;
    std::vector<std::filesystem::path> loaded_libraries;
};

// =============================================================================
// WidgetPackageLoader
// =============================================================================

/// Loader for widget.package files
class WidgetPackageLoader : public PackageLoader {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    WidgetPackageLoader() = default;

    // =========================================================================
    // PackageLoader Interface
    // =========================================================================

    [[nodiscard]] PackageType supported_type() const override {
        return PackageType::Widget;
    }

    [[nodiscard]] const char* name() const override {
        return "WidgetPackageLoader";
    }

    [[nodiscard]] void_core::Result<void> load(
        const ResolvedPackage& package,
        LoadContext& ctx) override;

    [[nodiscard]] void_core::Result<void> unload(
        const std::string& package_name,
        LoadContext& ctx) override;

    [[nodiscard]] bool supports_hot_reload() const override { return true; }

    [[nodiscard]] bool is_loaded(const std::string& package_name) const override {
        return m_loaded_packages.find(package_name) != m_loaded_packages.end();
    }

    [[nodiscard]] std::vector<std::string> loaded_packages() const override {
        std::vector<std::string> names;
        names.reserve(m_loaded_packages.size());
        for (const auto& [name, _] : m_loaded_packages) {
            names.push_back(name);
        }
        return names;
    }

    // =========================================================================
    // Widget Manager Access
    // =========================================================================

    /// Get the widget manager
    [[nodiscard]] WidgetManager& widget_manager() { return m_widget_manager; }
    [[nodiscard]] const WidgetManager& widget_manager() const { return m_widget_manager; }

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

    /// Load custom widget types from libraries
    [[nodiscard]] void_core::Result<std::vector<std::string>> load_widget_types(
        const WidgetPackageManifest& manifest);

    /// Load and register widgets
    [[nodiscard]] void_core::Result<std::vector<std::string>> load_widgets(
        const WidgetPackageManifest& manifest,
        const std::string& package_name,
        LoadContext& ctx);

    /// Setup ECS bindings for widgets
    [[nodiscard]] void_core::Result<void> setup_bindings(
        const WidgetPackageManifest& manifest,
        LoadContext& ctx);

    // =========================================================================
    // Data Members
    // =========================================================================

    std::map<std::string, LoadedWidgetState> m_loaded_packages;
    WidgetManager m_widget_manager;
    DynamicLibraryCache m_library_cache;
};

// =============================================================================
// WidgetPackageLoader Implementation
// =============================================================================

void_core::Result<void> WidgetPackageLoader::load(
    const ResolvedPackage& package,
    LoadContext& ctx)
{
    // Check if already loaded
    if (is_loaded(package.manifest.name)) {
        return void_core::Error("Widget package already loaded: " + package.manifest.name);
    }

    // Load and parse the manifest
    auto manifest_result = WidgetPackageManifest::load(package.manifest.source_path);
    if (!manifest_result) {
        return void_core::Error("Failed to load widget manifest: " + manifest_result.error().message());
    }

    auto& manifest = *manifest_result;

    // Validate the manifest
    auto validate_result = manifest.validate();
    if (!validate_result) {
        return void_core::Error("Widget manifest validation failed: " + validate_result.error().message());
    }

    // Initialize state tracking
    LoadedWidgetState state;
    state.name = package.manifest.name;
    state.manifest = manifest;

    // Configure widget manager with services from context
    if (ctx.ecs_world()) {
        m_widget_manager.set_ecs_world(ctx.ecs_world());
    }

    // Get schema registry from plugin loader if available
    auto* schema_registry = ctx.get_service<ComponentSchemaRegistry>();
    if (schema_registry) {
        m_widget_manager.set_schema_registry(schema_registry);
    }

    m_widget_manager.set_library_cache(&m_library_cache);

    // Step 1: Load custom widget types from libraries
    auto types_result = load_widget_types(manifest);
    if (!types_result) {
        return void_core::Error("Failed to load widget types: " + types_result.error().message());
    }
    state.registered_types = std::move(*types_result);

    // Step 2: Load and register widgets (filtered by build type)
    auto widgets_result = load_widgets(manifest, package.manifest.name, ctx);
    if (!widgets_result) {
        return void_core::Error("Failed to load widgets: " + widgets_result.error().message());
    }
    state.registered_widgets = std::move(*widgets_result);

    // Step 3: Setup ECS bindings
    auto bindings_result = setup_bindings(manifest, ctx);
    if (!bindings_result) {
        return void_core::Error("Failed to setup bindings: " + bindings_result.error().message());
    }

    // Step 4: Initialize all widgets
    auto init_result = m_widget_manager.init_all();
    if (!init_result) {
        return void_core::Error("Failed to initialize widgets: " + init_result.error().message());
    }

    // Store loaded state
    state.loaded_libraries = manifest.collect_library_paths();
    m_loaded_packages[package.manifest.name] = std::move(state);

    return void_core::Ok();
}

void_core::Result<void> WidgetPackageLoader::unload(
    const std::string& package_name,
    LoadContext& /*ctx*/)
{
    auto it = m_loaded_packages.find(package_name);
    if (it == m_loaded_packages.end()) {
        return void_core::Error("Widget package not loaded: " + package_name);
    }

    const auto& state = it->second;

    // Destroy all widgets from this package
    m_widget_manager.destroy_widgets_from_package(package_name);

    // Unload libraries
    for (const auto& lib_path : state.loaded_libraries) {
        m_library_cache.unload(lib_path);
    }

    // Remove from loaded packages
    m_loaded_packages.erase(it);

    return void_core::Ok();
}

void_core::Result<std::vector<std::string>> WidgetPackageLoader::load_widget_types(
    const WidgetPackageManifest& manifest)
{
    std::vector<std::string> registered;

    for (const auto& type_decl : manifest.widget_types) {
        // Load the library first
        auto lib_path = with_library_extension(manifest.resolve_library_path(type_decl.library));

        if (!std::filesystem::exists(lib_path)) {
            return void_core::Error("Widget library not found: " + lib_path.string());
        }

        auto lib_result = m_library_cache.get_or_load(lib_path);
        if (!lib_result) {
            return void_core::Error("Failed to load widget library: " + lib_result.error().message());
        }

        // Register the widget type
        auto reg_result = m_widget_manager.register_widget_type_from_library(type_decl);
        if (!reg_result) {
            return void_core::Error("Failed to register widget type '" + type_decl.type_name +
                "': " + reg_result.error().message());
        }

        registered.push_back(type_decl.type_name);
    }

    return registered;
}

void_core::Result<std::vector<std::string>> WidgetPackageLoader::load_widgets(
    const WidgetPackageManifest& manifest,
    const std::string& package_name,
    LoadContext& /*ctx*/)
{
    std::vector<std::string> registered;

    // Get widgets enabled for current build
    auto enabled_widgets = manifest.widgets_for_current_build();

    for (const auto* widget_decl : enabled_widgets) {
        // Check if the widget type exists
        if (!m_widget_manager.type_registry().has_type(widget_decl->type)) {
            return void_core::Error("Unknown widget type: " + widget_decl->type +
                " for widget: " + widget_decl->id);
        }

        // Register the widget
        auto handle_result = m_widget_manager.register_widget(*widget_decl, package_name);
        if (!handle_result) {
            return void_core::Error("Failed to register widget '" + widget_decl->id +
                "': " + handle_result.error().message());
        }

        registered.push_back(widget_decl->id);
    }

    return registered;
}

void_core::Result<void> WidgetPackageLoader::setup_bindings(
    const WidgetPackageManifest& manifest,
    LoadContext& /*ctx*/)
{
    for (const auto& binding : manifest.bindings) {
        // Check if the widget exists (may have been filtered by build type)
        if (!m_widget_manager.has_widget(binding.widget_id)) {
            // Skip bindings for widgets that weren't loaded
            continue;
        }

        auto result = m_widget_manager.apply_binding(binding);
        if (!result) {
            return void_core::Error("Failed to apply binding for widget '" +
                binding.widget_id + "': " + result.error().message());
        }
    }

    return void_core::Ok();
}

// =============================================================================
// Factory Function
// =============================================================================

/// Create a widget package loader
std::unique_ptr<PackageLoader> create_widget_package_loader() {
    return std::make_unique<WidgetPackageLoader>();
}

} // namespace void_package
