/// @file manifest.cpp
/// @brief Package manifest implementation

#include <void_engine/package/manifest.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace void_package {

// =============================================================================
// JSON Parsing Helpers
// =============================================================================

namespace {

/// Parse a single dependency from JSON
void_core::Result<PackageDependency> parse_dependency(const nlohmann::json& j) {
    PackageDependency dep;

    // Name is required
    if (!j.contains("name") || !j["name"].is_string()) {
        return void_core::Err<PackageDependency>(
            void_core::Error(void_core::ErrorCode::ParseError,
                "Dependency missing 'name' field"));
    }
    dep.name = j["name"].get<std::string>();

    // Version constraint (optional, defaults to any)
    if (j.contains("version") && j["version"].is_string()) {
        auto constraint_result = VersionConstraint::parse(j["version"].get<std::string>());
        if (!constraint_result) {
            return void_core::Err<PackageDependency>(
                void_core::Error(void_core::ErrorCode::ParseError,
                    "Invalid version constraint for dependency '" + dep.name + "': " +
                    constraint_result.error().message()));
        }
        dep.constraint = std::move(*constraint_result);
    }

    // Optional flag
    if (j.contains("optional") && j["optional"].is_boolean()) {
        dep.optional = j["optional"].get<bool>();
    }

    // Reason (optional)
    if (j.contains("reason") && j["reason"].is_string()) {
        dep.reason = j["reason"].get<std::string>();
    }

    return void_core::Ok(dep);
}

/// Parse dependencies array from JSON
void_core::Result<std::vector<PackageDependency>> parse_dependencies_array(
    const nlohmann::json& arr, const char* type_name) {

    std::vector<PackageDependency> deps;
    deps.reserve(arr.size());

    for (const auto& item : arr) {
        auto dep_result = parse_dependency(item);
        if (!dep_result) {
            return void_core::Err<std::vector<PackageDependency>>(
                void_core::Error(void_core::ErrorCode::ParseError,
                    std::string("Error parsing ") + type_name + " dependency: " +
                    dep_result.error().message()));
        }
        deps.push_back(std::move(*dep_result));
    }

    return void_core::Ok(std::move(deps));
}

} // anonymous namespace

// =============================================================================
// PackageManifest Implementation
// =============================================================================

void_core::Result<PackageManifest> PackageManifest::load(const std::filesystem::path& path) {
    // Check file exists
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::NotFound,
                "Manifest file not found: " + path.string()));
    }

    // Read file contents
    std::ifstream file(path);
    if (!file.is_open()) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::IOError,
                "Failed to open manifest file: " + path.string()));
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_str = buffer.str();

    // Parse with source path for error messages
    auto result = from_json_string(json_str, path);
    if (!result) {
        return result;
    }

    // Set path information
    result->source_path = path;
    result->base_path = path.parent_path();

    return result;
}

void_core::Result<PackageManifest> PackageManifest::from_json_string(
    const std::string& json_str,
    const std::filesystem::path& source_path) {

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::parse_error& e) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::ParseError,
                "JSON parse error in " + source_path.string() + ": " + e.what()));
    }

    PackageManifest manifest;

    // Parse "package" section (required)
    if (!j.contains("package") || !j["package"].is_object()) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::ParseError,
                "Missing 'package' section in manifest"));
    }

    const auto& pkg = j["package"];

    // Name (required)
    if (!pkg.contains("name") || !pkg["name"].is_string()) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::ParseError,
                "Missing 'package.name' in manifest"));
    }
    manifest.name = pkg["name"].get<std::string>();

    // Validate package name
    if (!is_valid_package_name(manifest.name)) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::ValidationError,
                "Invalid package name: " + manifest.name));
    }

    // Type (required)
    if (!pkg.contains("type") || !pkg["type"].is_string()) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::ParseError,
                "Missing 'package.type' in manifest"));
    }
    std::string type_str = pkg["type"].get<std::string>();
    if (!package_type_from_string(type_str, manifest.type)) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::ParseError,
                "Invalid package type: " + type_str));
    }

    // Version (required)
    if (!pkg.contains("version") || !pkg["version"].is_string()) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::ParseError,
                "Missing 'package.version' in manifest"));
    }
    auto ver_result = SemanticVersion::parse(pkg["version"].get<std::string>());
    if (!ver_result) {
        return void_core::Err<PackageManifest>(
            void_core::Error(void_core::ErrorCode::ParseError,
                "Invalid package version: " + ver_result.error().message()));
    }
    manifest.version = std::move(*ver_result);

    // Optional metadata fields
    if (pkg.contains("display_name") && pkg["display_name"].is_string()) {
        manifest.display_name = pkg["display_name"].get<std::string>();
    }
    if (pkg.contains("description") && pkg["description"].is_string()) {
        manifest.description = pkg["description"].get<std::string>();
    }
    if (pkg.contains("author") && pkg["author"].is_string()) {
        manifest.author = pkg["author"].get<std::string>();
    }
    if (pkg.contains("license") && pkg["license"].is_string()) {
        manifest.license = pkg["license"].get<std::string>();
    }
    if (pkg.contains("homepage") && pkg["homepage"].is_string()) {
        manifest.homepage = pkg["homepage"].get<std::string>();
    }
    if (pkg.contains("repository") && pkg["repository"].is_string()) {
        manifest.repository = pkg["repository"].get<std::string>();
    }

    // Engine version constraint (optional)
    if (j.contains("engine") && j["engine"].is_object()) {
        const auto& eng = j["engine"];
        if (eng.contains("version") && eng["version"].is_string()) {
            auto constraint_result = VersionConstraint::parse(eng["version"].get<std::string>());
            if (!constraint_result) {
                return void_core::Err<PackageManifest>(
                    void_core::Error(void_core::ErrorCode::ParseError,
                        "Invalid engine version constraint: " + constraint_result.error().message()));
            }
            manifest.engine_version = std::move(*constraint_result);
        }
        // Also support min/max format
        if (eng.contains("min") && eng["min"].is_string()) {
            auto min_ver = SemanticVersion::parse(eng["min"].get<std::string>());
            if (min_ver) {
                auto max_str = eng.contains("max") && eng["max"].is_string()
                    ? eng["max"].get<std::string>() : "";

                if (!max_str.empty()) {
                    auto max_ver = SemanticVersion::parse(max_str);
                    if (max_ver) {
                        manifest.engine_version = VersionConstraint::range(
                            *min_ver, true, *max_ver, true);
                    }
                } else {
                    manifest.engine_version = VersionConstraint::greater_equal(*min_ver);
                }
            }
        }
    }

    // Parse dependencies section
    if (j.contains("dependencies") && j["dependencies"].is_object()) {
        const auto& deps = j["dependencies"];

        // Plugin dependencies
        if (deps.contains("plugins") && deps["plugins"].is_array()) {
            auto result = parse_dependencies_array(deps["plugins"], "plugin");
            if (!result) {
                return void_core::Err<PackageManifest>(result.error());
            }
            manifest.plugin_deps = std::move(*result);
        }

        // Widget dependencies
        if (deps.contains("widgets") && deps["widgets"].is_array()) {
            auto result = parse_dependencies_array(deps["widgets"], "widget");
            if (!result) {
                return void_core::Err<PackageManifest>(result.error());
            }
            manifest.widget_deps = std::move(*result);
        }

        // Layer dependencies
        if (deps.contains("layers") && deps["layers"].is_array()) {
            auto result = parse_dependencies_array(deps["layers"], "layer");
            if (!result) {
                return void_core::Err<PackageManifest>(result.error());
            }
            manifest.layer_deps = std::move(*result);
        }

        // Asset dependencies
        if (deps.contains("assets") && deps["assets"].is_array()) {
            auto result = parse_dependencies_array(deps["assets"], "asset");
            if (!result) {
                return void_core::Err<PackageManifest>(result.error());
            }
            manifest.asset_deps = std::move(*result);
        }
    }

    return void_core::Ok(std::move(manifest));
}

void_core::Result<void> PackageManifest::validate() const {
    // Check required fields
    if (name.empty()) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::ValidationError,
            "Package name cannot be empty"));
    }

    // Validate package name format
    if (!is_valid_package_name(name)) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::ValidationError,
            "Invalid package name format: " + name));
    }

    // Check for self-dependency
    auto all_deps = all_dependencies();
    for (const auto& dep : all_deps) {
        if (dep.name == name) {
            return void_core::Err(void_core::Error(void_core::ErrorCode::ValidationError,
                "Package cannot depend on itself: " + name));
        }
    }

    // Validate dependency type rules
    // Check plugin dependencies
    if (!plugin_deps.empty() && !may_depend_on(PackageType::Plugin)) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::ValidationError,
            std::string(package_type_to_string(type)) + " packages cannot depend on plugins"));
    }

    // Check widget dependencies
    if (!widget_deps.empty() && !may_depend_on(PackageType::Widget)) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::ValidationError,
            std::string(package_type_to_string(type)) + " packages cannot depend on widgets"));
    }

    // Check layer dependencies
    if (!layer_deps.empty() && !may_depend_on(PackageType::Layer)) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::ValidationError,
            std::string(package_type_to_string(type)) + " packages cannot depend on layers"));
    }

    // For plugins, validate layer hierarchy
    if (type == PackageType::Plugin) {
        int my_layer = plugin_layer_level();
        for (const auto& dep : plugin_deps) {
            int dep_layer = get_plugin_layer_level(dep.name);
            if (dep_layer > my_layer && my_layer >= 0 && dep_layer >= 0) {
                return void_core::Err(void_core::Error(void_core::ErrorCode::ValidationError,
                    "Plugin '" + name + "' (layer " + std::to_string(my_layer) +
                    ") cannot depend on '" + dep.name + "' (layer " + std::to_string(dep_layer) + ")"));
            }
        }
    }

    return void_core::Ok();
}

bool PackageManifest::may_depend_on(PackageType other_type) const noexcept {
    switch (type) {
        case PackageType::World:
            // World can depend on everything
            return true;

        case PackageType::Layer:
            // Layer can depend on plugin, widget, asset
            return other_type == PackageType::Plugin ||
                   other_type == PackageType::Widget ||
                   other_type == PackageType::Asset;

        case PackageType::Plugin:
            // Plugin can depend on plugin (lower layer) or asset
            return other_type == PackageType::Plugin ||
                   other_type == PackageType::Asset;

        case PackageType::Widget:
            // Widget can depend on plugin or asset
            return other_type == PackageType::Plugin ||
                   other_type == PackageType::Asset;

        case PackageType::Asset:
            // Asset can only depend on other assets (preferably none)
            return other_type == PackageType::Asset;

        default:
            return false;
    }
}

std::vector<PackageDependency> PackageManifest::all_dependencies() const {
    std::vector<PackageDependency> all;
    all.reserve(plugin_deps.size() + widget_deps.size() +
                layer_deps.size() + asset_deps.size());

    all.insert(all.end(), plugin_deps.begin(), plugin_deps.end());
    all.insert(all.end(), widget_deps.begin(), widget_deps.end());
    all.insert(all.end(), layer_deps.begin(), layer_deps.end());
    all.insert(all.end(), asset_deps.begin(), asset_deps.end());

    return all;
}

std::vector<PackageDependency> PackageManifest::required_dependencies() const {
    std::vector<PackageDependency> required;
    auto all = all_dependencies();
    required.reserve(all.size());

    for (auto& dep : all) {
        if (!dep.optional) {
            required.push_back(std::move(dep));
        }
    }

    return required;
}

std::string PackageManifest::namespace_prefix() const {
    auto prefix = get_namespace_prefix(name);
    return std::string(prefix);
}

std::string PackageManifest::short_name() const {
    auto dot_pos = name.find('.');
    if (dot_pos == std::string::npos) {
        return name;
    }
    return name.substr(dot_pos + 1);
}

int PackageManifest::plugin_layer_level() const noexcept {
    return get_plugin_layer_level(name);
}

bool PackageManifest::respects_plugin_layers(
    [[maybe_unused]] const std::string& dep_name, int dep_layer) const noexcept {

    int my_layer = plugin_layer_level();

    // Unknown layers always pass (don't enforce for custom namespaces)
    if (my_layer < 0 || dep_layer < 0) {
        return true;
    }

    // Cannot depend on higher layer
    return dep_layer <= my_layer;
}

// =============================================================================
// Package Name Utilities
// =============================================================================

bool is_valid_package_name(std::string_view name) noexcept {
    if (name.empty()) return false;

    // Must contain at least one dot
    if (name.find('.') == std::string_view::npos) return false;

    // Cannot start or end with dot
    if (name.front() == '.' || name.back() == '.') return false;

    bool prev_was_dot = false;
    for (char c : name) {
        if (c == '.') {
            // Cannot have consecutive dots
            if (prev_was_dot) return false;
            prev_was_dot = true;
        } else {
            prev_was_dot = false;
            // Only lowercase alphanumeric and underscore
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                return false;
            }
            if (std::isupper(static_cast<unsigned char>(c))) {
                return false;  // Must be lowercase
            }
        }
    }

    return true;
}

int get_plugin_layer_level(std::string_view name) noexcept {
    auto prefix = get_namespace_prefix(name);

    if (prefix == "core") return 0;
    if (prefix == "engine") return 1;
    if (prefix == "gameplay") return 2;
    if (prefix == "feature") return 3;
    if (prefix == "mod") return 4;

    return -1;  // Unknown namespace
}

std::string_view get_namespace_prefix(std::string_view name) noexcept {
    auto dot_pos = name.find('.');
    if (dot_pos == std::string_view::npos) {
        return name;
    }
    return name.substr(0, dot_pos);
}

bool has_namespace_prefix(std::string_view name, std::string_view prefix) noexcept {
    return get_namespace_prefix(name) == prefix;
}

} // namespace void_package
