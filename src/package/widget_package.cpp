/// @file widget_package.cpp
/// @brief Widget package manifest implementation

#include <void_engine/package/widget_package.hpp>

#include <algorithm>
#include <fstream>
#include <set>

namespace void_package {

// =============================================================================
// BuildType Utilities
// =============================================================================

const char* build_type_to_string(BuildType type) noexcept {
    switch (type) {
        case BuildType::Debug:       return "debug";
        case BuildType::Development: return "development";
        case BuildType::Profile:     return "profile";
        case BuildType::Release:     return "release";
        default:                     return "unknown";
    }
}

bool build_type_from_string(const std::string& str, BuildType& out) noexcept {
    if (str == "debug" || str == "Debug") {
        out = BuildType::Debug;
        return true;
    }
    if (str == "development" || str == "Development" || str == "dev") {
        out = BuildType::Development;
        return true;
    }
    if (str == "profile" || str == "Profile") {
        out = BuildType::Profile;
        return true;
    }
    if (str == "release" || str == "Release" || str == "shipping") {
        out = BuildType::Release;
        return true;
    }
    return false;
}

// =============================================================================
// BindingType Utilities
// =============================================================================

const char* binding_type_to_string(BindingType type) noexcept {
    switch (type) {
        case BindingType::Query:    return "query";
        case BindingType::Resource: return "resource";
        case BindingType::Event:    return "event";
        default:                    return "unknown";
    }
}

bool binding_type_from_string(const std::string& str, BindingType& out) noexcept {
    if (str == "query" || str == "Query") {
        out = BindingType::Query;
        return true;
    }
    if (str == "resource" || str == "Resource") {
        out = BindingType::Resource;
        return true;
    }
    if (str == "event" || str == "Event") {
        out = BindingType::Event;
        return true;
    }
    return false;
}

// =============================================================================
// WidgetDeclaration
// =============================================================================

void_core::Result<WidgetDeclaration> WidgetDeclaration::from_json(const nlohmann::json& j) {
    WidgetDeclaration decl;

    // Required: id
    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Error("Widget declaration missing required 'id' field");
    }
    decl.id = j["id"].get<std::string>();

    // Required: type
    if (!j.contains("type") || !j["type"].is_string()) {
        return void_core::Error("Widget '" + decl.id + "' missing required 'type' field");
    }
    decl.type = j["type"].get<std::string>();

    // Optional: enabled_in_builds (defaults to all builds)
    if (j.contains("enabled_in_builds")) {
        if (!j["enabled_in_builds"].is_array()) {
            return void_core::Error("Widget '" + decl.id + "': enabled_in_builds must be an array");
        }
        for (const auto& build_json : j["enabled_in_builds"]) {
            if (!build_json.is_string()) {
                return void_core::Error("Widget '" + decl.id + "': enabled_in_builds must contain strings");
            }
            BuildType build;
            if (!build_type_from_string(build_json.get<std::string>(), build)) {
                return void_core::Error("Widget '" + decl.id + "': unknown build type: " +
                    build_json.get<std::string>());
            }
            decl.enabled_in_builds.push_back(build);
        }
    } else {
        // Default: enabled in all builds
        decl.enabled_in_builds = {
            BuildType::Debug,
            BuildType::Development,
            BuildType::Profile,
            BuildType::Release
        };
    }

    // Optional: toggle_key
    if (j.contains("toggle_key") && j["toggle_key"].is_string()) {
        decl.toggle_key = j["toggle_key"].get<std::string>();
    }

    // Optional: config (defaults to empty object)
    if (j.contains("config")) {
        decl.config = j["config"];
    } else {
        decl.config = nlohmann::json::object();
    }

    // Optional: initially_visible (defaults to true)
    if (j.contains("initially_visible") && j["initially_visible"].is_boolean()) {
        decl.initially_visible = j["initially_visible"].get<bool>();
    }

    // Optional: description
    if (j.contains("description") && j["description"].is_string()) {
        decl.description = j["description"].get<std::string>();
    }

    return decl;
}

nlohmann::json WidgetDeclaration::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["type"] = type;

    nlohmann::json builds = nlohmann::json::array();
    for (const auto& build : enabled_in_builds) {
        builds.push_back(build_type_to_string(build));
    }
    j["enabled_in_builds"] = builds;

    if (toggle_key.has_value()) {
        j["toggle_key"] = *toggle_key;
    }
    j["config"] = config;
    j["initially_visible"] = initially_visible;

    if (!description.empty()) {
        j["description"] = description;
    }

    return j;
}

bool WidgetDeclaration::is_enabled_in_current_build() const noexcept {
    return is_enabled_for_build(current_build_type());
}

bool WidgetDeclaration::is_enabled_for_build(BuildType build) const noexcept {
    return std::find(enabled_in_builds.begin(), enabled_in_builds.end(), build)
           != enabled_in_builds.end();
}

// =============================================================================
// WidgetBinding
// =============================================================================

void_core::Result<WidgetBinding> WidgetBinding::from_json(const nlohmann::json& j) {
    WidgetBinding binding;

    // Required: widget_id
    if (!j.contains("widget_id") || !j["widget_id"].is_string()) {
        return void_core::Error("Widget binding missing required 'widget_id' field");
    }
    binding.widget_id = j["widget_id"].get<std::string>();

    // Required: data_source
    if (!j.contains("data_source") || !j["data_source"].is_string()) {
        return void_core::Error("Widget binding for '" + binding.widget_id +
            "' missing required 'data_source' field");
    }
    binding.data_source = j["data_source"].get<std::string>();

    // Required: binding_type
    if (!j.contains("binding_type") || !j["binding_type"].is_string()) {
        return void_core::Error("Widget binding for '" + binding.widget_id +
            "' missing required 'binding_type' field");
    }
    if (!binding_type_from_string(j["binding_type"].get<std::string>(), binding.binding_type)) {
        return void_core::Error("Widget binding for '" + binding.widget_id +
            "': unknown binding_type: " + j["binding_type"].get<std::string>());
    }

    // Optional: read_only (defaults to true)
    if (j.contains("read_only") && j["read_only"].is_boolean()) {
        binding.read_only = j["read_only"].get<bool>();
    }

    // Optional: alias
    if (j.contains("alias") && j["alias"].is_string()) {
        binding.alias = j["alias"].get<std::string>();
    }

    return binding;
}

nlohmann::json WidgetBinding::to_json() const {
    nlohmann::json j;
    j["widget_id"] = widget_id;
    j["data_source"] = data_source;
    j["binding_type"] = binding_type_to_string(binding_type);
    j["read_only"] = read_only;
    if (!alias.empty()) {
        j["alias"] = alias;
    }
    return j;
}

std::vector<std::string> WidgetBinding::parse_query_components() const {
    std::vector<std::string> components;

    // Format: "query:Component1,Component2,Component3"
    if (binding_type != BindingType::Query) {
        return components;
    }

    // Find the colon separator
    auto colon_pos = data_source.find(':');
    if (colon_pos == std::string::npos || colon_pos + 1 >= data_source.size()) {
        return components;
    }

    // Parse comma-separated component names
    std::string components_str = data_source.substr(colon_pos + 1);
    std::string::size_type start = 0;
    std::string::size_type end = 0;

    while ((end = components_str.find(',', start)) != std::string::npos) {
        std::string comp = components_str.substr(start, end - start);
        // Trim whitespace
        auto comp_start = comp.find_first_not_of(" \t");
        auto comp_end = comp.find_last_not_of(" \t");
        if (comp_start != std::string::npos) {
            components.push_back(comp.substr(comp_start, comp_end - comp_start + 1));
        }
        start = end + 1;
    }

    // Last component (or only component if no commas)
    std::string last = components_str.substr(start);
    auto comp_start = last.find_first_not_of(" \t");
    auto comp_end = last.find_last_not_of(" \t");
    if (comp_start != std::string::npos) {
        components.push_back(last.substr(comp_start, comp_end - comp_start + 1));
    }

    return components;
}

std::string WidgetBinding::parse_resource_name() const {
    // Format: "resource:ResourceName"
    if (binding_type != BindingType::Resource) {
        return {};
    }

    auto colon_pos = data_source.find(':');
    if (colon_pos == std::string::npos || colon_pos + 1 >= data_source.size()) {
        return {};
    }

    std::string name = data_source.substr(colon_pos + 1);
    // Trim whitespace
    auto start = name.find_first_not_of(" \t");
    auto end = name.find_last_not_of(" \t");
    if (start != std::string::npos) {
        return name.substr(start, end - start + 1);
    }
    return {};
}

std::string WidgetBinding::parse_event_name() const {
    // Format: "event:EventName"
    if (binding_type != BindingType::Event) {
        return {};
    }

    auto colon_pos = data_source.find(':');
    if (colon_pos == std::string::npos || colon_pos + 1 >= data_source.size()) {
        return {};
    }

    std::string name = data_source.substr(colon_pos + 1);
    // Trim whitespace
    auto start = name.find_first_not_of(" \t");
    auto end = name.find_last_not_of(" \t");
    if (start != std::string::npos) {
        return name.substr(start, end - start + 1);
    }
    return {};
}

// =============================================================================
// WidgetLibraryDeclaration
// =============================================================================

void_core::Result<WidgetLibraryDeclaration> WidgetLibraryDeclaration::from_json(const nlohmann::json& j) {
    WidgetLibraryDeclaration decl;

    // Required: type_name
    if (!j.contains("type_name") || !j["type_name"].is_string()) {
        return void_core::Error("Widget library declaration missing 'type_name'");
    }
    decl.type_name = j["type_name"].get<std::string>();

    // Required: library
    if (!j.contains("library") || !j["library"].is_string()) {
        return void_core::Error("Widget library '" + decl.type_name + "' missing 'library' path");
    }
    decl.library = j["library"].get<std::string>();

    // Required: factory
    if (!j.contains("factory") || !j["factory"].is_string()) {
        return void_core::Error("Widget library '" + decl.type_name + "' missing 'factory' function name");
    }
    decl.factory = j["factory"].get<std::string>();

    return decl;
}

nlohmann::json WidgetLibraryDeclaration::to_json() const {
    return nlohmann::json{
        {"type_name", type_name},
        {"library", library},
        {"factory", factory}
    };
}

// =============================================================================
// WidgetPackageManifest
// =============================================================================

void_core::Result<WidgetPackageManifest> WidgetPackageManifest::load(
    const std::filesystem::path& path)
{
    // Read file
    std::ifstream file(path);
    if (!file.is_open()) {
        return void_core::Error("Failed to open widget manifest: " + path.string());
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    return from_json_string(content, path);
}

void_core::Result<WidgetPackageManifest> WidgetPackageManifest::from_json_string(
    const std::string& json_str,
    const std::filesystem::path& source_path)
{
    // Parse JSON
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::parse_error& e) {
        return void_core::Error("JSON parse error: " + std::string(e.what()));
    }

    // Parse base manifest first
    auto base_result = PackageManifest::from_json_string(json_str, source_path);
    if (!base_result) {
        return void_core::Error("Failed to parse base manifest: " + base_result.error().message());
    }

    return from_json(j, std::move(*base_result));
}

void_core::Result<WidgetPackageManifest> WidgetPackageManifest::from_json(
    const nlohmann::json& j,
    PackageManifest base_manifest)
{
    WidgetPackageManifest manifest;
    manifest.base = std::move(base_manifest);

    // Validate type
    if (manifest.base.type != PackageType::Widget) {
        return void_core::Error("Package type must be 'widget', got: " +
            std::string(package_type_to_string(manifest.base.type)));
    }

    // Parse widgets array
    if (j.contains("widgets")) {
        if (!j["widgets"].is_array()) {
            return void_core::Error("'widgets' must be an array");
        }
        for (const auto& widget_json : j["widgets"]) {
            auto decl_result = WidgetDeclaration::from_json(widget_json);
            if (!decl_result) {
                return void_core::Error("Widget declaration error: " + decl_result.error().message());
            }
            manifest.widgets.push_back(std::move(*decl_result));
        }
    }

    // Parse bindings array
    if (j.contains("bindings")) {
        if (!j["bindings"].is_array()) {
            return void_core::Error("'bindings' must be an array");
        }
        for (const auto& binding_json : j["bindings"]) {
            auto binding_result = WidgetBinding::from_json(binding_json);
            if (!binding_result) {
                return void_core::Error("Binding error: " + binding_result.error().message());
            }
            manifest.bindings.push_back(std::move(*binding_result));
        }
    }

    // Parse widget_types array (custom widget types from libraries)
    if (j.contains("widget_types")) {
        if (!j["widget_types"].is_array()) {
            return void_core::Error("'widget_types' must be an array");
        }
        for (const auto& type_json : j["widget_types"]) {
            auto type_result = WidgetLibraryDeclaration::from_json(type_json);
            if (!type_result) {
                return void_core::Error("Widget type error: " + type_result.error().message());
            }
            manifest.widget_types.push_back(std::move(*type_result));
        }
    }

    return manifest;
}

nlohmann::json WidgetPackageManifest::to_json() const {
    nlohmann::json j;

    // Package metadata
    j["package"] = {
        {"name", base.name},
        {"type", package_type_to_string(base.type)},
        {"version", base.version.to_string()}
    };

    // Widgets
    nlohmann::json widgets_json = nlohmann::json::array();
    for (const auto& widget : widgets) {
        widgets_json.push_back(widget.to_json());
    }
    j["widgets"] = widgets_json;

    // Bindings
    nlohmann::json bindings_json = nlohmann::json::array();
    for (const auto& binding : bindings) {
        bindings_json.push_back(binding.to_json());
    }
    j["bindings"] = bindings_json;

    // Widget types
    if (!widget_types.empty()) {
        nlohmann::json types_json = nlohmann::json::array();
        for (const auto& type : widget_types) {
            types_json.push_back(type.to_json());
        }
        j["widget_types"] = types_json;
    }

    return j;
}

void_core::Result<void> WidgetPackageManifest::validate() const {
    // Validate base manifest
    auto base_result = base.validate();
    if (!base_result) {
        return base_result;
    }

    // Validate widget IDs are unique
    std::set<std::string> widget_ids;
    for (const auto& widget : widgets) {
        if (widget_ids.count(widget.id) > 0) {
            return void_core::Error("Duplicate widget ID: " + widget.id);
        }
        widget_ids.insert(widget.id);
    }

    // Validate bindings reference existing widgets
    for (const auto& binding : bindings) {
        if (widget_ids.count(binding.widget_id) == 0) {
            return void_core::Error("Binding references unknown widget: " + binding.widget_id);
        }
    }

    // Validate widget type names are unique
    std::set<std::string> type_names;
    for (const auto& type : widget_types) {
        if (type_names.count(type.type_name) > 0) {
            return void_core::Error("Duplicate widget type name: " + type.type_name);
        }
        type_names.insert(type.type_name);
    }

    return void_core::Ok();
}

bool WidgetPackageManifest::has_widget(const std::string& widget_id) const {
    return std::any_of(widgets.begin(), widgets.end(),
        [&widget_id](const WidgetDeclaration& w) { return w.id == widget_id; });
}

const WidgetDeclaration* WidgetPackageManifest::get_widget(const std::string& widget_id) const {
    auto it = std::find_if(widgets.begin(), widgets.end(),
        [&widget_id](const WidgetDeclaration& w) { return w.id == widget_id; });
    return it != widgets.end() ? &(*it) : nullptr;
}

std::vector<const WidgetBinding*> WidgetPackageManifest::get_bindings_for(
    const std::string& widget_id) const
{
    std::vector<const WidgetBinding*> result;
    for (const auto& binding : bindings) {
        if (binding.widget_id == widget_id) {
            result.push_back(&binding);
        }
    }
    return result;
}

std::vector<const WidgetDeclaration*> WidgetPackageManifest::widgets_for_current_build() const {
    return widgets_for_build(current_build_type());
}

std::vector<const WidgetDeclaration*> WidgetPackageManifest::widgets_for_build(BuildType build) const {
    std::vector<const WidgetDeclaration*> result;
    for (const auto& widget : widgets) {
        if (widget.is_enabled_for_build(build)) {
            result.push_back(&widget);
        }
    }
    return result;
}

std::vector<std::filesystem::path> WidgetPackageManifest::collect_library_paths() const {
    std::set<std::string> unique_paths;
    std::vector<std::filesystem::path> result;

    for (const auto& type : widget_types) {
        if (unique_paths.insert(type.library).second) {
            result.push_back(resolve_library_path(type.library));
        }
    }

    return result;
}

std::filesystem::path WidgetPackageManifest::resolve_library_path(
    const std::string& lib_path) const
{
    std::filesystem::path path(lib_path);
    if (path.is_absolute()) {
        return path;
    }
    return base.base_path / path;
}

} // namespace void_package
