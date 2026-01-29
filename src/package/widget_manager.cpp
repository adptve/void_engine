/// @file widget_manager.cpp
/// @brief Widget manager implementation

#include <void_engine/package/widget_manager.hpp>
#include <void_engine/package/widget.hpp>
#include <void_engine/package/widget_package.hpp>
#include <void_engine/package/component_schema.hpp>
#include <void_engine/package/dynamic_library.hpp>

#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/component.hpp>
#include <void_engine/ecs/query.hpp>

#include <sstream>
#include <algorithm>

namespace void_package {

// =============================================================================
// WidgetContext Implementation
// =============================================================================

void_ecs::QueryState* WidgetContext::get_bound_query(const std::string& name) const {
    auto it = m_bound_queries.find(name);
    return it != m_bound_queries.end() ? it->second : nullptr;
}

const nlohmann::json* WidgetContext::get_resource(const std::string& name) const {
    auto it = m_bound_resources.find(name);
    return it != m_bound_resources.end() ? it->second : nullptr;
}

void WidgetContext::add_bound_query(const std::string& name, void_ecs::QueryState* query) {
    m_bound_queries[name] = query;
}

void WidgetContext::add_resource_binding(const std::string& name, const nlohmann::json* resource) {
    m_bound_resources[name] = resource;
}

// =============================================================================
// WidgetTypeRegistry Implementation
// =============================================================================

WidgetTypeRegistry::WidgetTypeRegistry() {
    register_builtins();
}

void WidgetTypeRegistry::register_type(const std::string& type_name, WidgetFactory factory) {
    m_factories[type_name] = std::move(factory);
}

bool WidgetTypeRegistry::has_type(const std::string& type_name) const {
    return m_factories.count(type_name) > 0;
}

std::unique_ptr<Widget> WidgetTypeRegistry::create(
    const std::string& type_name,
    const nlohmann::json& config) const
{
    auto it = m_factories.find(type_name);
    if (it == m_factories.end()) {
        return nullptr;
    }
    return it->second(config);
}

std::vector<std::string> WidgetTypeRegistry::registered_types() const {
    std::vector<std::string> types;
    types.reserve(m_factories.size());
    for (const auto& [name, _] : m_factories) {
        types.push_back(name);
    }
    return types;
}

void WidgetTypeRegistry::register_builtins() {
    // Debug HUD
    register_type("debug_hud", [](const nlohmann::json& config) -> std::unique_ptr<Widget> {
        return std::make_unique<DebugHudWidget>(config);
    });

    // Console
    register_type("console", [](const nlohmann::json& config) -> std::unique_ptr<Widget> {
        return std::make_unique<ConsoleWidget>(config);
    });

    // Inspector
    register_type("inspector", [](const nlohmann::json& config) -> std::unique_ptr<Widget> {
        return std::make_unique<InspectorWidget>(config);
    });
}

// =============================================================================
// Built-in Widgets Implementation
// =============================================================================

// --- DebugHudWidget ---

DebugHudWidget::DebugHudWidget() = default;

DebugHudWidget::DebugHudWidget(const nlohmann::json& config) {
    if (config.contains("id") && config["id"].is_string()) {
        m_id = config["id"].get<std::string>();
    }
    if (config.contains("show_fps") && config["show_fps"].is_boolean()) {
        m_show_fps = config["show_fps"].get<bool>();
    }
    if (config.contains("show_frame_time") && config["show_frame_time"].is_boolean()) {
        m_show_frame_time = config["show_frame_time"].get<bool>();
    }
    if (config.contains("show_entity_count") && config["show_entity_count"].is_boolean()) {
        m_show_entity_count = config["show_entity_count"].get<bool>();
    }
    if (config.contains("show_memory") && config["show_memory"].is_boolean()) {
        m_show_memory = config["show_memory"].get<bool>();
    }
}

void_core::Result<void> DebugHudWidget::init(WidgetContext& /*ctx*/) {
    m_fps = 0.0f;
    m_frame_time_ms = 0.0f;
    m_entity_count = 0;
    m_fps_accumulator = 0.0f;
    m_fps_sample_count = 0;
    return void_core::Ok();
}

void DebugHudWidget::update(WidgetContext& ctx, float dt) {
    // Update FPS
    if (dt > 0.0f) {
        m_fps_accumulator += 1.0f / dt;
        m_fps_sample_count++;

        if (m_fps_sample_count >= kFpsSampleWindow) {
            m_fps = m_fps_accumulator / static_cast<float>(m_fps_sample_count);
            m_frame_time_ms = 1000.0f / m_fps;
            m_fps_accumulator = 0.0f;
            m_fps_sample_count = 0;
        }
    }

    // Update entity count
    if (ctx.world()) {
        m_entity_count = ctx.world()->entity_count();
    }
}

void DebugHudWidget::render(WidgetContext& /*ctx*/) {
    // Rendering would be done by the UI system
    // This is a placeholder for the interface
    // In a real implementation, this would use ImGui or similar
}

void DebugHudWidget::shutdown(WidgetContext& /*ctx*/) {
    // Nothing to clean up
}

// --- ConsoleWidget ---

ConsoleWidget::ConsoleWidget() {
    setup_default_commands();
}

ConsoleWidget::ConsoleWidget(const nlohmann::json& config) {
    setup_default_commands();

    if (config.contains("id") && config["id"].is_string()) {
        m_id = config["id"].get<std::string>();
    }
    if (config.contains("history_size") && config["history_size"].is_number_unsigned()) {
        m_max_history = config["history_size"].get<std::size_t>();
    }
    if (config.contains("log_filter") && config["log_filter"].is_string()) {
        m_log_filter = config["log_filter"].get<std::string>();
    }
}

void_core::Result<void> ConsoleWidget::init(WidgetContext& /*ctx*/) {
    m_log_messages.clear();
    m_history.clear();
    m_input_buffer.clear();
    m_history_index = 0;
    return void_core::Ok();
}

void ConsoleWidget::update(WidgetContext& /*ctx*/, float /*dt*/) {
    // Console doesn't need per-frame updates
}

void ConsoleWidget::render(WidgetContext& /*ctx*/) {
    // Rendering would be done by the UI system
}

void ConsoleWidget::shutdown(WidgetContext& /*ctx*/) {
    m_log_messages.clear();
    m_history.clear();
    m_commands.clear();
}

void ConsoleWidget::log(const std::string& message) {
    m_log_messages.push_back(message);
    m_scroll_to_bottom = true;
}

void_core::Result<void> ConsoleWidget::execute_command(const std::string& command) {
    if (command.empty()) {
        return void_core::Ok();
    }

    // Add to history
    m_history.push_back(command);
    if (m_history.size() > m_max_history) {
        m_history.erase(m_history.begin());
    }
    m_history_index = m_history.size();

    // Parse command and args
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string token;
    while (iss >> token) {
        args.push_back(token);
    }

    if (args.empty()) {
        return void_core::Ok();
    }

    const std::string& cmd_name = args[0];
    args.erase(args.begin()); // Remove command name, leaving only args

    auto it = m_commands.find(cmd_name);
    if (it == m_commands.end()) {
        log("[Error] Unknown command: " + cmd_name);
        return void_core::Error("Unknown command: " + cmd_name);
    }

    auto result = it->second(args);
    if (!result) {
        log("[Error] " + result.error().message());
    }
    return result;
}

void ConsoleWidget::register_command(
    const std::string& name,
    std::function<void_core::Result<void>(const std::vector<std::string>&)> handler)
{
    m_commands[name] = std::move(handler);
}

void ConsoleWidget::setup_default_commands() {
    register_command("help", [this](const std::vector<std::string>& /*args*/) {
        log("Available commands:");
        for (const auto& [name, _] : m_commands) {
            log("  " + name);
        }
        return void_core::Ok();
    });

    register_command("clear", [this](const std::vector<std::string>& /*args*/) {
        m_log_messages.clear();
        return void_core::Ok();
    });

    register_command("echo", [this](const std::vector<std::string>& args) {
        std::string message;
        for (const auto& arg : args) {
            if (!message.empty()) message += " ";
            message += arg;
        }
        log(message);
        return void_core::Ok();
    });
}

// --- InspectorWidget ---

InspectorWidget::InspectorWidget() = default;

InspectorWidget::InspectorWidget(const nlohmann::json& config) {
    if (config.contains("id") && config["id"].is_string()) {
        m_id = config["id"].get<std::string>();
    }
    if (config.contains("allow_edit") && config["allow_edit"].is_boolean()) {
        m_allow_edit = config["allow_edit"].get<bool>();
    }
    if (config.contains("show_hidden") && config["show_hidden"].is_boolean()) {
        m_show_hidden = config["show_hidden"].get<bool>();
    }
}

void_core::Result<void> InspectorWidget::init(WidgetContext& /*ctx*/) {
    m_selected_entity = std::nullopt;
    return void_core::Ok();
}

void InspectorWidget::update(WidgetContext& ctx, float /*dt*/) {
    // Validate selected entity still exists
    if (m_selected_entity.has_value() && ctx.world()) {
        if (!ctx.world()->is_alive(*m_selected_entity)) {
            m_selected_entity = std::nullopt;
        }
    }
}

void InspectorWidget::render(WidgetContext& /*ctx*/) {
    // Rendering would be done by the UI system
}

void InspectorWidget::shutdown(WidgetContext& /*ctx*/) {
    m_selected_entity = std::nullopt;
}

void InspectorWidget::select_entity(void_ecs::Entity entity) {
    m_selected_entity = entity;
}

void InspectorWidget::clear_selection() {
    m_selected_entity = std::nullopt;
}

std::optional<void_ecs::Entity> InspectorWidget::selected_entity() const {
    return m_selected_entity;
}

// =============================================================================
// WidgetManager Implementation
// =============================================================================

WidgetManager::WidgetManager() = default;

WidgetManager::~WidgetManager() {
    shutdown_all();
}

WidgetManager::WidgetManager(WidgetManager&&) noexcept = default;
WidgetManager& WidgetManager::operator=(WidgetManager&&) noexcept = default;

// --- Widget Registration ---

void_core::Result<WidgetHandle> WidgetManager::register_widget(
    const WidgetDeclaration& decl,
    const std::string& source_package)
{
    // Check for duplicate ID
    if (m_id_to_handle.count(decl.id) > 0) {
        return void_core::Error("Widget already registered: " + decl.id);
    }

    // Create the widget using the type registry
    auto widget = m_type_registry.create(decl.type, decl.config);
    if (!widget) {
        return void_core::Error("Unknown widget type: " + decl.type);
    }

    // Allocate handle
    WidgetHandle handle = allocate_handle();

    // Create instance
    auto instance = std::make_unique<WidgetInstance>();
    instance->handle = handle;
    instance->id = decl.id;
    instance->type = decl.type;
    instance->source_package = source_package;
    instance->widget = std::move(widget);
    instance->context.set_world(m_ecs_world);
    instance->context.set_config(decl.config);
    instance->widget->set_visible(decl.initially_visible);

    // Store
    if (handle.index >= m_widgets.size()) {
        m_widgets.resize(handle.index + 1);
    }
    m_widgets[handle.index] = std::move(instance);
    m_id_to_handle[decl.id] = handle;

    // Register toggle key if specified
    if (decl.toggle_key.has_value()) {
        m_toggle_key_to_widget[*decl.toggle_key] = decl.id;
    }

    return handle;
}

void_core::Result<WidgetHandle> WidgetManager::register_and_init_widget(
    const WidgetDeclaration& decl,
    const std::string& source_package)
{
    auto handle_result = register_widget(decl, source_package);
    if (!handle_result) {
        return handle_result;
    }

    auto init_result = init_widget(*handle_result);
    if (!init_result) {
        // Remove the widget on init failure (ignore destroy result, already failing)
        (void)destroy_widget(*handle_result);
        return void_core::Error("Widget init failed: " + init_result.error().message());
    }

    return handle_result;
}

// --- Widget Creation ---

void_core::Result<WidgetHandle> WidgetManager::create_widget(const std::string& id) {
    // Look up existing registration
    auto it = m_id_to_handle.find(id);
    if (it != m_id_to_handle.end()) {
        // Already registered, return existing handle
        return it->second;
    }

    return void_core::Error("Widget not registered: " + id);
}

void_core::Result<WidgetHandle> WidgetManager::create_widget(
    const std::string& type,
    const nlohmann::json& config)
{
    // Generate a unique ID if not provided
    std::string id;
    if (config.contains("id") && config["id"].is_string()) {
        id = config["id"].get<std::string>();
    } else {
        id = type + "_" + std::to_string(m_next_generation);
    }

    WidgetDeclaration decl;
    decl.id = id;
    decl.type = type;
    decl.config = config;
    decl.enabled_in_builds = {
        BuildType::Debug,
        BuildType::Development,
        BuildType::Profile,
        BuildType::Release
    };

    return register_and_init_widget(decl);
}

// --- Widget Destruction ---

void_core::Result<void> WidgetManager::destroy_widget(WidgetHandle handle) {
    auto* instance = get_instance(handle);
    if (!instance) {
        return void_core::Error("Invalid widget handle");
    }

    // Shutdown if initialized
    if (instance->initialized && instance->widget) {
        instance->widget->shutdown(instance->context);
    }

    // Remove from lookup tables
    m_id_to_handle.erase(instance->id);

    // Remove toggle key mapping
    for (auto it = m_toggle_key_to_widget.begin(); it != m_toggle_key_to_widget.end(); ) {
        if (it->second == instance->id) {
            it = m_toggle_key_to_widget.erase(it);
        } else {
            ++it;
        }
    }

    // Free the slot
    m_widgets[handle.index].reset();
    m_free_indices.push_back(handle.index);

    return void_core::Ok();
}

void_core::Result<void> WidgetManager::destroy_widget(const std::string& id) {
    auto it = m_id_to_handle.find(id);
    if (it == m_id_to_handle.end()) {
        return void_core::Error("Widget not found: " + id);
    }
    return destroy_widget(it->second);
}

void WidgetManager::destroy_widgets_from_package(const std::string& package_name) {
    std::vector<WidgetHandle> to_destroy;
    for (const auto& instance : m_widgets) {
        if (instance && instance->source_package == package_name) {
            to_destroy.push_back(instance->handle);
        }
    }
    for (const auto& handle : to_destroy) {
        (void)destroy_widget(handle);
    }
}

void WidgetManager::destroy_all_widgets() {
    shutdown_all();

    m_widgets.clear();
    m_free_indices.clear();
    m_id_to_handle.clear();
    m_toggle_key_to_widget.clear();
}

// --- ECS Binding ---

void_core::Result<void> WidgetManager::bind_to_query(
    const std::string& widget_id,
    void_ecs::QueryDescriptor query_descriptor,
    const std::string& binding_name)
{
    auto* instance = get_instance_by_id(widget_id);
    if (!instance) {
        return void_core::Error("Widget not found: " + widget_id);
    }

    if (!m_ecs_world) {
        return void_core::Error("No ECS world set");
    }

    // Build the query descriptor and create query state
    query_descriptor.build();
    auto query_state = std::make_unique<void_ecs::QueryState>(std::move(query_descriptor));
    query_state->update(m_ecs_world->archetypes());

    // Add to context
    instance->context.add_bound_query(binding_name, query_state.get());

    // Transfer ownership
    instance->owned_queries.push_back(std::move(query_state));

    return void_core::Ok();
}

void_core::Result<void> WidgetManager::bind_to_query_by_names(
    const std::string& widget_id,
    const std::vector<std::string>& component_names,
    const std::string& binding_name)
{
    if (!m_schema_registry) {
        return void_core::Error("No component schema registry set");
    }

    if (!m_ecs_world) {
        return void_core::Error("No ECS world set");
    }

    // Build query descriptor from component names
    void_ecs::QueryDescriptor query;

    for (const auto& name : component_names) {
        // Try schema registry first
        auto comp_id = m_schema_registry->get_component_id(name);
        if (!comp_id) {
            // Fall back to ECS registry
            comp_id = m_ecs_world->component_registry().get_id_by_name(name);
        }

        if (!comp_id) {
            return void_core::Error("Unknown component for widget binding: " + name);
        }

        // Widgets typically read-only
        query.read(*comp_id);
    }

    return bind_to_query(widget_id, std::move(query), binding_name);
}

void_core::Result<void> WidgetManager::bind_to_resource(
    const std::string& widget_id,
    const std::string& resource_name)
{
    auto* instance = get_instance_by_id(widget_id);
    if (!instance) {
        return void_core::Error("Widget not found: " + widget_id);
    }

    // Resource binding is deferred - the resource may not exist yet
    // We store the name and resolve it during update
    // For now, just record the binding intent
    // Actual resource lookup would be done via ECS resource system

    // Placeholder: resources would be stored in ECS world
    instance->context.add_resource_binding(resource_name, nullptr);

    return void_core::Ok();
}

void_core::Result<void> WidgetManager::apply_binding(const WidgetBinding& binding) {
    switch (binding.binding_type) {
        case BindingType::Query: {
            auto components = binding.parse_query_components();
            if (components.empty()) {
                return void_core::Error("Invalid query binding: " + binding.data_source);
            }
            std::string alias = binding.alias.empty() ? "default" : binding.alias;
            return bind_to_query_by_names(binding.widget_id, components, alias);
        }

        case BindingType::Resource: {
            std::string resource_name = binding.parse_resource_name();
            if (resource_name.empty()) {
                return void_core::Error("Invalid resource binding: " + binding.data_source);
            }
            return bind_to_resource(binding.widget_id, resource_name);
        }

        case BindingType::Event:
            // Event bindings would require event bus integration
            // Not implemented in this phase
            return void_core::Ok();
    }

    return void_core::Ok();
}

// --- Widget Lifecycle ---

void_core::Result<void> WidgetManager::init_widget(WidgetHandle handle) {
    auto* instance = get_instance(handle);
    if (!instance) {
        return void_core::Error("Invalid widget handle");
    }

    if (instance->initialized) {
        return void_core::Ok(); // Already initialized
    }

    if (!instance->widget) {
        return void_core::Error("Widget has no implementation");
    }

    // Ensure context has current world
    instance->context.set_world(m_ecs_world);

    auto result = instance->widget->init(instance->context);
    if (!result) {
        return result;
    }

    instance->initialized = true;
    return void_core::Ok();
}

void_core::Result<void> WidgetManager::init_widget(const std::string& id) {
    auto handle = get_handle(id);
    if (!handle.has_value()) {
        return void_core::Error("Widget not found: " + id);
    }
    return init_widget(*handle);
}

void_core::Result<void> WidgetManager::init_all() {
    for (const auto& instance : m_widgets) {
        if (instance && !instance->initialized) {
            auto result = init_widget(instance->handle);
            if (!result) {
                return result;
            }
        }
    }
    return void_core::Ok();
}

void_core::Result<void> WidgetManager::shutdown_widget(WidgetHandle handle) {
    auto* instance = get_instance(handle);
    if (!instance) {
        return void_core::Error("Invalid widget handle");
    }

    if (!instance->initialized) {
        return void_core::Ok();
    }

    if (instance->widget) {
        instance->widget->shutdown(instance->context);
    }

    instance->initialized = false;
    return void_core::Ok();
}

void WidgetManager::shutdown_all() {
    for (const auto& instance : m_widgets) {
        if (instance && instance->initialized && instance->widget) {
            instance->widget->shutdown(instance->context);
            instance->initialized = false;
        }
    }
}

// --- Frame Update ---

void WidgetManager::update_all(float dt) {
    for (const auto& instance : m_widgets) {
        if (instance && instance->initialized && instance->widget) {
            if (instance->widget->is_enabled()) {
                instance->widget->update(instance->context, dt);
            }
        }
    }
}

void WidgetManager::render_all() {
    for (const auto& instance : m_widgets) {
        if (instance && instance->initialized && instance->widget) {
            if (instance->widget->is_visible()) {
                instance->widget->render(instance->context);
            }
        }
    }
}

// --- Widget Access ---

Widget* WidgetManager::get_widget(WidgetHandle handle) {
    auto* instance = get_instance(handle);
    return instance ? instance->widget.get() : nullptr;
}

const Widget* WidgetManager::get_widget(WidgetHandle handle) const {
    auto* instance = get_instance(handle);
    return instance ? instance->widget.get() : nullptr;
}

Widget* WidgetManager::get_widget(const std::string& id) {
    auto* instance = get_instance_by_id(id);
    return instance ? instance->widget.get() : nullptr;
}

const Widget* WidgetManager::get_widget(const std::string& id) const {
    auto* instance = get_instance_by_id(id);
    return instance ? instance->widget.get() : nullptr;
}

std::optional<WidgetHandle> WidgetManager::get_handle(const std::string& id) const {
    auto it = m_id_to_handle.find(id);
    if (it != m_id_to_handle.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool WidgetManager::has_widget(const std::string& id) const {
    return m_id_to_handle.count(id) > 0;
}

bool WidgetManager::is_valid_handle(WidgetHandle handle) const {
    if (handle.index >= m_widgets.size()) {
        return false;
    }
    const auto& instance = m_widgets[handle.index];
    return instance && instance->handle.generation == handle.generation;
}

std::vector<std::string> WidgetManager::all_widget_ids() const {
    std::vector<std::string> ids;
    ids.reserve(m_widgets.size());
    for (const auto& instance : m_widgets) {
        if (instance) {
            ids.push_back(instance->id);
        }
    }
    return ids;
}

std::vector<std::string> WidgetManager::widgets_from_package(const std::string& package_name) const {
    std::vector<std::string> ids;
    for (const auto& instance : m_widgets) {
        if (instance && instance->source_package == package_name) {
            ids.push_back(instance->id);
        }
    }
    return ids;
}

// --- Widget Type Registration ---

void_core::Result<void> WidgetManager::register_widget_type_from_library(
    const WidgetLibraryDeclaration& decl)
{
    if (!m_library_cache) {
        return void_core::Error("No library cache set");
    }

    // Load the library
    std::filesystem::path lib_path = with_library_extension(decl.library);
    auto lib_result = m_library_cache->get_or_load(lib_path);
    if (!lib_result) {
        return void_core::Error("Failed to load widget library: " + lib_result.error().message());
    }

    auto* lib = *lib_result;

    // Look up the factory function
    // Factory signature: Widget* create_widget(const char* config_json)
    using WidgetCreateFn = Widget*(*)(const char* config_json);

    auto fn_result = lib->get_function<WidgetCreateFn>(decl.factory);
    if (!fn_result) {
        return void_core::Error("Factory function not found: " + decl.factory);
    }

    WidgetCreateFn create_fn = *fn_result;

    // Register the type
    m_type_registry.register_type(decl.type_name,
        [create_fn](const nlohmann::json& config) -> std::unique_ptr<Widget> {
            std::string config_str = config.dump();
            Widget* widget = create_fn(config_str.c_str());
            return std::unique_ptr<Widget>(widget);
        });

    return void_core::Ok();
}

// --- Visibility Control ---

void WidgetManager::toggle_widget(const std::string& id) {
    auto* widget = get_widget(id);
    if (widget) {
        widget->toggle_visible();
    }
}

void WidgetManager::set_widget_visible(const std::string& id, bool visible) {
    auto* widget = get_widget(id);
    if (widget) {
        widget->set_visible(visible);
    }
}

void WidgetManager::handle_toggle_key(const std::string& key_name) {
    auto it = m_toggle_key_to_widget.find(key_name);
    if (it != m_toggle_key_to_widget.end()) {
        toggle_widget(it->second);
    }
}

void WidgetManager::register_toggle_key(const std::string& key_name, const std::string& widget_id) {
    m_toggle_key_to_widget[key_name] = widget_id;
}

// --- Debugging ---

std::string WidgetManager::format_state() const {
    std::ostringstream oss;
    oss << "WidgetManager State:\n";
    oss << "  Widgets: " << widget_count() << "\n";
    oss << "  Types: " << m_type_registry.registered_types().size() << "\n";
    oss << "\nRegistered Widgets:\n";
    for (const auto& instance : m_widgets) {
        if (instance) {
            oss << "  - " << instance->id << " (" << instance->type << ")";
            if (!instance->source_package.empty()) {
                oss << " from " << instance->source_package;
            }
            oss << " [" << (instance->initialized ? "init" : "not-init") << "]";
            oss << " [" << (instance->widget && instance->widget->is_visible() ? "visible" : "hidden") << "]";
            oss << "\n";
        }
    }
    return oss.str();
}

// --- Internal Methods ---

WidgetHandle WidgetManager::allocate_handle() {
    WidgetHandle handle;

    if (!m_free_indices.empty()) {
        handle.index = m_free_indices.back();
        m_free_indices.pop_back();
    } else {
        handle.index = static_cast<std::uint32_t>(m_widgets.size());
    }

    handle.generation = m_next_generation++;

    return handle;
}

WidgetInstance* WidgetManager::get_instance(WidgetHandle handle) {
    if (handle.index >= m_widgets.size()) {
        return nullptr;
    }
    auto& instance = m_widgets[handle.index];
    if (!instance || instance->handle.generation != handle.generation) {
        return nullptr;
    }
    return instance.get();
}

const WidgetInstance* WidgetManager::get_instance(WidgetHandle handle) const {
    if (handle.index >= m_widgets.size()) {
        return nullptr;
    }
    const auto& instance = m_widgets[handle.index];
    if (!instance || instance->handle.generation != handle.generation) {
        return nullptr;
    }
    return instance.get();
}

WidgetInstance* WidgetManager::get_instance_by_id(const std::string& id) {
    auto handle = get_handle(id);
    if (!handle.has_value()) {
        return nullptr;
    }
    return get_instance(*handle);
}

const WidgetInstance* WidgetManager::get_instance_by_id(const std::string& id) const {
    auto handle = get_handle(id);
    if (!handle.has_value()) {
        return nullptr;
    }
    return get_instance(*handle);
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<WidgetManager> create_widget_manager() {
    return std::make_unique<WidgetManager>();
}

} // namespace void_package
