/// @file plugin_package.cpp
/// @brief Plugin package manifest implementation

#include <void_engine/package/plugin_package.hpp>
#include <void_engine/package/definition_registry.hpp>

#include <fstream>
#include <algorithm>
#include <cctype>
#include <set>

namespace void_package {

// =============================================================================
// System Stage Utilities
// =============================================================================

const char* system_stage_to_string(void_ecs::SystemStage stage) noexcept {
    switch (stage) {
        case void_ecs::SystemStage::First:      return "first";
        case void_ecs::SystemStage::PreUpdate:  return "pre_update";
        case void_ecs::SystemStage::Update:     return "update";
        case void_ecs::SystemStage::PostUpdate: return "post_update";
        case void_ecs::SystemStage::PreRender:  return "pre_render";
        case void_ecs::SystemStage::Render:     return "render";
        case void_ecs::SystemStage::PostRender: return "post_render";
        case void_ecs::SystemStage::Last:       return "last";
        default: return "unknown";
    }
}

bool system_stage_from_string(const std::string& str, void_ecs::SystemStage& out) noexcept {
    // Convert to lowercase for case-insensitive comparison
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Handle both underscore and no-underscore variants
    if (lower == "first") {
        out = void_ecs::SystemStage::First;
        return true;
    }
    if (lower == "pre_update" || lower == "preupdate") {
        out = void_ecs::SystemStage::PreUpdate;
        return true;
    }
    if (lower == "update") {
        out = void_ecs::SystemStage::Update;
        return true;
    }
    if (lower == "post_update" || lower == "postupdate") {
        out = void_ecs::SystemStage::PostUpdate;
        return true;
    }
    if (lower == "pre_render" || lower == "prerender") {
        out = void_ecs::SystemStage::PreRender;
        return true;
    }
    if (lower == "render") {
        out = void_ecs::SystemStage::Render;
        return true;
    }
    if (lower == "post_render" || lower == "postrender") {
        out = void_ecs::SystemStage::PostRender;
        return true;
    }
    if (lower == "last") {
        out = void_ecs::SystemStage::Last;
        return true;
    }

    return false;
}

// =============================================================================
// FieldDeclaration
// =============================================================================

void_core::Result<FieldDeclaration> FieldDeclaration::from_json(
    const std::string& field_name,
    const nlohmann::json& j)
{
    FieldDeclaration decl;
    decl.name = field_name;

    // Handle shorthand: "field_name": "f32" (type string only)
    if (j.is_string()) {
        decl.type = j.get<std::string>();
        return decl;
    }

    // Handle full object form
    if (!j.is_object()) {
        return void_core::Error("Field declaration must be string or object");
    }

    // Type is required
    if (!j.contains("type")) {
        return void_core::Error("Field '" + field_name + "' missing 'type'");
    }
    decl.type = j["type"].get<std::string>();

    // Optional fields
    if (j.contains("default")) {
        decl.default_value = j["default"];
    }
    if (j.contains("required")) {
        decl.required = j["required"].get<bool>();
    }
    if (j.contains("description")) {
        decl.description = j["description"].get<std::string>();
    }

    return decl;
}

void_core::Result<FieldSchema> FieldDeclaration::to_field_schema() const {
    FieldSchema schema;
    schema.name = name;
    schema.required = required;
    schema.description = description;
    schema.default_value = default_value;

    // Parse type string to FieldType
    FieldType field_type;
    if (!field_type_from_string(type, field_type)) {
        return void_core::Error("Unknown field type: " + type);
    }
    schema.type = field_type;

    // Handle array types like "array<f32>"
    if (type.starts_with("array<") && type.ends_with(">")) {
        schema.type = FieldType::Array;
        std::string element_type = type.substr(6, type.length() - 7);
        FieldType elem_ft;
        if (!field_type_from_string(element_type, elem_ft)) {
            return void_core::Error("Unknown array element type: " + element_type);
        }
        schema.array_element_type = elem_ft;
    }

    return schema;
}

// =============================================================================
// ComponentDeclaration
// =============================================================================

void_core::Result<ComponentDeclaration> ComponentDeclaration::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return void_core::Error("Component declaration must be an object");
    }

    ComponentDeclaration decl;

    // Name is required
    if (!j.contains("name")) {
        return void_core::Error("Component declaration missing 'name'");
    }
    decl.name = j["name"].get<std::string>();

    // Check for tag component
    if (j.contains("is_tag") && j["is_tag"].get<bool>()) {
        decl.is_tag = true;
        // Tag components have no fields
        return decl;
    }

    // Parse fields
    if (j.contains("fields")) {
        const auto& fields_json = j["fields"];
        if (!fields_json.is_object()) {
            return void_core::Error("Component '" + decl.name + "' fields must be an object");
        }

        for (auto& [field_name, field_json] : fields_json.items()) {
            auto field_result = FieldDeclaration::from_json(field_name, field_json);
            if (!field_result) {
                return void_core::Error("Component '" + decl.name + "': " + field_result.error().message());
            }
            decl.fields[field_name] = std::move(*field_result);
        }
    }

    // Optional metadata
    if (j.contains("description")) {
        decl.description = j["description"].get<std::string>();
    }

    return decl;
}

nlohmann::json ComponentDeclaration::to_json() const {
    nlohmann::json j;
    j["name"] = name;

    if (is_tag) {
        j["is_tag"] = true;
    } else {
        nlohmann::json fields_json = nlohmann::json::object();
        for (const auto& [field_name, field] : fields) {
            nlohmann::json field_json;
            field_json["type"] = field.type;
            if (field.default_value) {
                field_json["default"] = *field.default_value;
            }
            if (field.required) {
                field_json["required"] = true;
            }
            if (!field.description.empty()) {
                field_json["description"] = field.description;
            }
            fields_json[field_name] = field_json;
        }
        j["fields"] = fields_json;
    }

    if (!description.empty()) {
        j["description"] = description;
    }

    return j;
}

void_core::Result<ComponentSchema> ComponentDeclaration::to_component_schema(
    const std::string& plugin_name) const
{
    ComponentSchema schema;
    schema.name = name;
    schema.source_plugin = plugin_name;
    schema.is_tag = is_tag;

    // Convert all fields
    for (const auto& [field_name, field_decl] : fields) {
        auto field_schema_result = field_decl.to_field_schema();
        if (!field_schema_result) {
            return void_core::Error("Component '" + name + "' field '" + field_name + "': " +
                field_schema_result.error().message());
        }
        schema.fields.push_back(std::move(*field_schema_result));
    }

    // Calculate layout
    schema.calculate_layout();

    return schema;
}

// =============================================================================
// SystemDeclaration
// =============================================================================

void_core::Result<SystemDeclaration> SystemDeclaration::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return void_core::Error("System declaration must be an object");
    }

    SystemDeclaration decl;

    // Name is required
    if (!j.contains("name")) {
        return void_core::Error("System declaration missing 'name'");
    }
    decl.name = j["name"].get<std::string>();

    // Stage (default to "update")
    if (j.contains("stage")) {
        decl.stage = j["stage"].get<std::string>();
    } else {
        decl.stage = "update";
    }

    // Validate stage
    void_ecs::SystemStage stage_enum;
    if (!system_stage_from_string(decl.stage, stage_enum)) {
        return void_core::Error("System '" + decl.name + "' has invalid stage: " + decl.stage);
    }

    // Query components (required)
    if (j.contains("query")) {
        if (!j["query"].is_array()) {
            return void_core::Error("System '" + decl.name + "' query must be an array");
        }
        for (const auto& comp : j["query"]) {
            if (!comp.is_string()) {
                return void_core::Error("System '" + decl.name + "' query elements must be strings");
            }
            decl.query.push_back(comp.get<std::string>());
        }
    }

    // Exclude components (optional)
    if (j.contains("exclude")) {
        if (!j["exclude"].is_array()) {
            return void_core::Error("System '" + decl.name + "' exclude must be an array");
        }
        for (const auto& comp : j["exclude"]) {
            if (!comp.is_string()) {
                return void_core::Error("System '" + decl.name + "' exclude elements must be strings");
            }
            decl.exclude.push_back(comp.get<std::string>());
        }
    }

    // Library path (required)
    if (!j.contains("library")) {
        return void_core::Error("System '" + decl.name + "' missing 'library'");
    }
    decl.library = j["library"].get<std::string>();

    // Entry point (default to system name + "_run")
    if (j.contains("entry_point")) {
        decl.entry_point = j["entry_point"].get<std::string>();
    } else {
        // Convert name to snake_case and add _run
        std::string entry = decl.name;
        // Simple conversion: insert underscore before uppercase letters
        std::string result;
        for (size_t i = 0; i < entry.size(); ++i) {
            char c = entry[i];
            if (std::isupper(c) && i > 0) {
                result += '_';
            }
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        decl.entry_point = result + "_run";
    }

    // Ordering constraints
    if (j.contains("run_after")) {
        if (!j["run_after"].is_array()) {
            return void_core::Error("System '" + decl.name + "' run_after must be an array");
        }
        for (const auto& sys : j["run_after"]) {
            decl.run_after.push_back(sys.get<std::string>());
        }
    }
    if (j.contains("run_before")) {
        if (!j["run_before"].is_array()) {
            return void_core::Error("System '" + decl.name + "' run_before must be an array");
        }
        for (const auto& sys : j["run_before"]) {
            decl.run_before.push_back(sys.get<std::string>());
        }
    }

    // Exclusive flag
    if (j.contains("exclusive")) {
        decl.exclusive = j["exclusive"].get<bool>();
    }

    // Description
    if (j.contains("description")) {
        decl.description = j["description"].get<std::string>();
    }

    return decl;
}

nlohmann::json SystemDeclaration::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["stage"] = stage;

    if (!query.empty()) {
        j["query"] = query;
    }
    if (!exclude.empty()) {
        j["exclude"] = exclude;
    }

    j["library"] = library;
    j["entry_point"] = entry_point;

    if (!run_after.empty()) {
        j["run_after"] = run_after;
    }
    if (!run_before.empty()) {
        j["run_before"] = run_before;
    }
    if (exclusive) {
        j["exclusive"] = true;
    }
    if (!description.empty()) {
        j["description"] = description;
    }

    return j;
}

void_core::Result<void_ecs::SystemStage> SystemDeclaration::parse_stage(const std::string& stage_str) {
    void_ecs::SystemStage stage;
    if (!system_stage_from_string(stage_str, stage)) {
        return void_core::Error("Invalid system stage: " + stage_str);
    }
    return stage;
}

void_core::Result<void_ecs::SystemStage> SystemDeclaration::get_stage() const {
    return parse_stage(stage);
}

// =============================================================================
// EventHandlerDeclaration
// =============================================================================

void_core::Result<EventHandlerDeclaration> EventHandlerDeclaration::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return void_core::Error("Event handler declaration must be an object");
    }

    EventHandlerDeclaration decl;

    // Event name is required
    if (!j.contains("event")) {
        return void_core::Error("Event handler missing 'event'");
    }
    decl.event = j["event"].get<std::string>();

    // Handler function name is required
    if (!j.contains("handler")) {
        return void_core::Error("Event handler for '" + decl.event + "' missing 'handler'");
    }
    decl.handler = j["handler"].get<std::string>();

    // Library path is required
    if (!j.contains("library")) {
        return void_core::Error("Event handler for '" + decl.event + "' missing 'library'");
    }
    decl.library = j["library"].get<std::string>();

    // Optional priority
    if (j.contains("priority")) {
        decl.priority = j["priority"].get<int>();
    }

    // Optional description
    if (j.contains("description")) {
        decl.description = j["description"].get<std::string>();
    }

    return decl;
}

nlohmann::json EventHandlerDeclaration::to_json() const {
    nlohmann::json j;
    j["event"] = event;
    j["handler"] = handler;
    j["library"] = library;

    if (priority != 0) {
        j["priority"] = priority;
    }
    if (!description.empty()) {
        j["description"] = description;
    }

    return j;
}

// =============================================================================
// RegistryDeclaration
// =============================================================================

void_core::Result<RegistryDeclaration> RegistryDeclaration::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return void_core::Error("Registry declaration must be an object");
    }

    RegistryDeclaration decl;

    // Name is required
    if (!j.contains("name")) {
        return void_core::Error("Registry declaration missing 'name'");
    }
    decl.name = j["name"].get<std::string>();

    // Collision policy (default to "error")
    if (j.contains("collision_policy")) {
        decl.collision_policy = j["collision_policy"].get<std::string>();
    } else {
        decl.collision_policy = "error";
    }

    // Validate collision policy
    CollisionPolicy policy;
    if (!collision_policy_from_string(decl.collision_policy, policy)) {
        return void_core::Error("Registry '" + decl.name + "' has invalid collision_policy: " +
            decl.collision_policy);
    }

    // Optional schema path
    if (j.contains("schema")) {
        decl.schema_path = j["schema"].get<std::string>();
    }

    // Allow dynamic fields (default true)
    if (j.contains("allow_dynamic_fields")) {
        decl.allow_dynamic_fields = j["allow_dynamic_fields"].get<bool>();
    }

    // Description
    if (j.contains("description")) {
        decl.description = j["description"].get<std::string>();
    }

    return decl;
}

nlohmann::json RegistryDeclaration::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["collision_policy"] = collision_policy;

    if (schema_path) {
        j["schema"] = *schema_path;
    }
    if (!allow_dynamic_fields) {
        j["allow_dynamic_fields"] = false;
    }
    if (!description.empty()) {
        j["description"] = description;
    }

    return j;
}

void_core::Result<RegistryTypeConfig> RegistryDeclaration::to_registry_config() const {
    RegistryTypeConfig config;
    config.name = name;
    config.allow_dynamic_fields = allow_dynamic_fields;
    config.schema_path = schema_path;

    CollisionPolicy policy;
    if (!collision_policy_from_string(collision_policy, policy)) {
        return void_core::Error("Invalid collision policy: " + collision_policy);
    }
    config.collision_policy = policy;

    return config;
}

// =============================================================================
// PluginPackageManifest
// =============================================================================

void_core::Result<PluginPackageManifest> PluginPackageManifest::load(
    const std::filesystem::path& path)
{
    // Read file
    std::ifstream file(path);
    if (!file) {
        return void_core::Error("Failed to open plugin manifest: " + path.string());
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    return from_json_string(content, path);
}

void_core::Result<PluginPackageManifest> PluginPackageManifest::from_json_string(
    const std::string& json_str,
    const std::filesystem::path& source_path)
{
    // Parse JSON
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::exception& e) {
        return void_core::Error("Failed to parse plugin manifest JSON: " + std::string(e.what()));
    }

    // First parse the base manifest
    auto base_result = PackageManifest::from_json_string(json_str, source_path);
    if (!base_result) {
        return void_core::Error("Failed to parse base manifest: " + base_result.error().message());
    }

    // Validate it's a plugin type
    if (base_result->type != PackageType::Plugin) {
        return void_core::Error("Expected plugin package type, got: " +
            std::string(package_type_to_string(base_result->type)));
    }

    return from_json(j, std::move(*base_result));
}

void_core::Result<PluginPackageManifest> PluginPackageManifest::from_json(
    const nlohmann::json& j,
    PackageManifest base_manifest)
{
    PluginPackageManifest manifest;
    manifest.base = std::move(base_manifest);

    // Parse components
    if (j.contains("components")) {
        const auto& comps = j["components"];
        if (!comps.is_array()) {
            return void_core::Error("Plugin 'components' must be an array");
        }

        for (const auto& comp_json : comps) {
            auto comp_result = ComponentDeclaration::from_json(comp_json);
            if (!comp_result) {
                return void_core::Error("Failed to parse component: " + comp_result.error().message());
            }
            manifest.components.push_back(std::move(*comp_result));
        }
    }

    // Parse systems
    if (j.contains("systems")) {
        const auto& systems = j["systems"];
        if (!systems.is_array()) {
            return void_core::Error("Plugin 'systems' must be an array");
        }

        for (const auto& sys_json : systems) {
            auto sys_result = SystemDeclaration::from_json(sys_json);
            if (!sys_result) {
                return void_core::Error("Failed to parse system: " + sys_result.error().message());
            }
            manifest.systems.push_back(std::move(*sys_result));
        }
    }

    // Parse event handlers
    if (j.contains("event_handlers")) {
        const auto& handlers = j["event_handlers"];
        if (!handlers.is_array()) {
            return void_core::Error("Plugin 'event_handlers' must be an array");
        }

        for (const auto& handler_json : handlers) {
            auto handler_result = EventHandlerDeclaration::from_json(handler_json);
            if (!handler_result) {
                return void_core::Error("Failed to parse event handler: " + handler_result.error().message());
            }
            manifest.event_handlers.push_back(std::move(*handler_result));
        }
    }

    // Parse registries
    if (j.contains("registries")) {
        const auto& regs = j["registries"];
        if (!regs.is_array()) {
            return void_core::Error("Plugin 'registries' must be an array");
        }

        for (const auto& reg_json : regs) {
            auto reg_result = RegistryDeclaration::from_json(reg_json);
            if (!reg_result) {
                return void_core::Error("Failed to parse registry: " + reg_result.error().message());
            }
            manifest.registries.push_back(std::move(*reg_result));
        }
    }

    // Collect library paths
    manifest.libraries = manifest.collect_library_paths();

    return manifest;
}

nlohmann::json PluginPackageManifest::to_json() const {
    // Start with base manifest fields
    nlohmann::json j;

    // Package info
    j["package"]["name"] = base.name;
    j["package"]["type"] = "plugin";
    j["package"]["version"] = base.version.to_string();

    // Dependencies
    if (!base.plugin_deps.empty()) {
        nlohmann::json deps = nlohmann::json::array();
        for (const auto& dep : base.plugin_deps) {
            deps.push_back(dep.name);
        }
        j["dependencies"]["plugins"] = deps;
    }

    // Components
    if (!components.empty()) {
        nlohmann::json comps = nlohmann::json::array();
        for (const auto& comp : components) {
            comps.push_back(comp.to_json());
        }
        j["components"] = comps;
    }

    // Systems
    if (!systems.empty()) {
        nlohmann::json sys = nlohmann::json::array();
        for (const auto& s : systems) {
            sys.push_back(s.to_json());
        }
        j["systems"] = sys;
    }

    // Event handlers
    if (!event_handlers.empty()) {
        nlohmann::json handlers = nlohmann::json::array();
        for (const auto& h : event_handlers) {
            handlers.push_back(h.to_json());
        }
        j["event_handlers"] = handlers;
    }

    // Registries
    if (!registries.empty()) {
        nlohmann::json regs = nlohmann::json::array();
        for (const auto& r : registries) {
            regs.push_back(r.to_json());
        }
        j["registries"] = regs;
    }

    return j;
}

void_core::Result<void> PluginPackageManifest::validate() const {
    // Validate base manifest first
    auto base_result = base.validate();
    if (!base_result) {
        return base_result;
    }

    // Check for duplicate component names
    std::set<std::string> comp_names;
    for (const auto& comp : components) {
        if (!comp_names.insert(comp.name).second) {
            return void_core::Error("Duplicate component name: " + comp.name);
        }
    }

    // Check for duplicate system names
    std::set<std::string> sys_names;
    for (const auto& sys : systems) {
        if (!sys_names.insert(sys.name).second) {
            return void_core::Error("Duplicate system name: " + sys.name);
        }
    }

    // Validate system stages
    for (const auto& sys : systems) {
        auto stage_result = sys.get_stage();
        if (!stage_result) {
            return void_core::Error("System '" + sys.name + "': " + stage_result.error().message());
        }
    }

    // Check for duplicate registry names
    std::set<std::string> reg_names;
    for (const auto& reg : registries) {
        if (!reg_names.insert(reg.name).second) {
            return void_core::Error("Duplicate registry name: " + reg.name);
        }
    }

    return void_core::Ok();
}

bool PluginPackageManifest::has_component(const std::string& name) const {
    return std::any_of(components.begin(), components.end(),
        [&name](const ComponentDeclaration& c) { return c.name == name; });
}

bool PluginPackageManifest::has_system(const std::string& name) const {
    return std::any_of(systems.begin(), systems.end(),
        [&name](const SystemDeclaration& s) { return s.name == name; });
}

const ComponentDeclaration* PluginPackageManifest::get_component(const std::string& name) const {
    auto it = std::find_if(components.begin(), components.end(),
        [&name](const ComponentDeclaration& c) { return c.name == name; });
    return it != components.end() ? &(*it) : nullptr;
}

const SystemDeclaration* PluginPackageManifest::get_system(const std::string& name) const {
    auto it = std::find_if(systems.begin(), systems.end(),
        [&name](const SystemDeclaration& s) { return s.name == name; });
    return it != systems.end() ? &(*it) : nullptr;
}

std::vector<std::filesystem::path> PluginPackageManifest::collect_library_paths() const {
    std::set<std::filesystem::path> unique_paths;

    for (const auto& sys : systems) {
        unique_paths.insert(resolve_library_path(sys.library));
    }

    for (const auto& handler : event_handlers) {
        unique_paths.insert(resolve_library_path(handler.library));
    }

    return std::vector<std::filesystem::path>(unique_paths.begin(), unique_paths.end());
}

std::filesystem::path PluginPackageManifest::resolve_library_path(
    const std::string& lib_path) const
{
    std::filesystem::path path(lib_path);

    // If already absolute, return as-is
    if (path.is_absolute()) {
        return path;
    }

    // Resolve relative to package base path
    return base.base_path / path;
}

} // namespace void_package
