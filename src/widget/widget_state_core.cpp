/// @file widget_state_core.cpp
/// @brief Implementation of WidgetStateCore and WidgetAPIImpl

#include <void_engine/widget/widget_state_core.hpp>
#include <void_engine/widget/commands.hpp>
#include <void_engine/plugin_api/dynamic_library.hpp>
#include <void_engine/gamestate/gamestate_core.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace void_widget {

// =============================================================================
// WidgetStateCore Implementation
// =============================================================================

WidgetStateCore::WidgetStateCore() : m_config{} {}

WidgetStateCore::WidgetStateCore(const WidgetStateCoreConfig& config) : m_config(config) {}

WidgetStateCore::~WidgetStateCore() {
    if (m_initialized) {
        shutdown();
    }
}

void WidgetStateCore::initialize() {
    if (m_initialized) return;

    // Set up screen metrics
    m_layout_state.screen_width = m_config.screen_width;
    m_layout_state.screen_height = m_config.screen_height;
    m_layout_state.ui_scale = m_config.ui_scale;

    // Set up default themes
    setup_default_themes();

    // Set up default layer
    setup_default_layer();

    // Create the widget API
    m_widget_api = std::make_unique<WidgetAPIImpl>(this, m_game_state);

    m_initialized = true;
}

void WidgetStateCore::shutdown() {
    if (!m_initialized) return;

    // Stop watching
    stop_watching();

    // Unload all widget plugins
    for (auto& [name, loaded] : m_loaded_widgets) {
        if (loaded->widget) {
            loaded->widget->on_widget_unload();
        }
    }
    m_loaded_widgets.clear();
    m_widget_type_to_plugin.clear();

    // Clear all state
    m_widget_registry = WidgetRegistry{};
    m_layout_state = LayoutState{};
    m_style_state = StyleState{};
    m_interaction_state = InteractionState{};
    m_animation_state = AnimationState{};
    m_binding_state = BindingState{};
    m_render_state = RenderState{};

    m_widget_api.reset();
    m_initialized = false;
}

IWidgetAPI* WidgetStateCore::widget_api() {
    return m_widget_api.get();
}

void WidgetStateCore::setup_default_themes() {
    m_style_state.themes["dark"] = Theme::dark();
    m_style_state.themes["light"] = Theme::light();
    m_style_state.themes["high_contrast"] = Theme::high_contrast();
    m_style_state.active_theme = "dark";
}

void WidgetStateCore::setup_default_layer() {
    // Create the default layer
    WidgetLayer default_layer;
    default_layer.id = LayerId{m_widget_registry.next_layer_id++};
    default_layer.name = "default";
    default_layer.z_order = 0;
    default_layer.visible = true;
    default_layer.interactive = true;
    m_widget_registry.layers.push_back(default_layer);
}

// -----------------------------------------------------------------------------
// Update Loop
// -----------------------------------------------------------------------------

void WidgetStateCore::begin_frame(float dt) {
    m_delta_time = dt;
    m_current_time += dt;
    m_frame_number++;

    // Clear render state
    m_render_state.clear();

    // Save previous input state
    m_interaction_state.mouse_position_prev = m_interaction_state.mouse_position;
    m_interaction_state.mouse_buttons_prev = m_interaction_state.mouse_buttons;
    m_interaction_state.keys_prev = m_interaction_state.keys;
}

void WidgetStateCore::process_input() {
    // Calculate mouse delta
    m_interaction_state.mouse_delta = m_interaction_state.mouse_position -
                                       m_interaction_state.mouse_position_prev;

    // Hit test to find hovered widget
    WidgetId new_hovered = hit_test(m_interaction_state.mouse_position);

    // Handle hover transitions
    if (new_hovered != m_interaction_state.hovered_widget) {
        // Exit old widget
        if (m_interaction_state.hovered_widget) {
            auto* widget = m_widget_registry.get(m_interaction_state.hovered_widget);
            if (widget) {
                // Find plugin and call on_hover_exit
                auto it = m_widget_type_to_plugin.find(widget->type);
                if (it != m_widget_type_to_plugin.end()) {
                    it->second->on_hover_exit(m_interaction_state.hovered_widget, *widget);
                }
            }
        }

        // Enter new widget
        m_interaction_state.hovered_widget = new_hovered;
        if (new_hovered) {
            auto* widget = m_widget_registry.get(new_hovered);
            if (widget) {
                auto it = m_widget_type_to_plugin.find(widget->type);
                if (it != m_widget_type_to_plugin.end()) {
                    it->second->on_hover_enter(new_hovered, *widget);
                }
            }
        }
    }

    // Handle mouse press
    if (m_interaction_state.is_mouse_pressed(0)) {
        m_interaction_state.pressed_widget = new_hovered;

        if (new_hovered) {
            // Check for double click
            double now = m_current_time;
            if (new_hovered == m_interaction_state.last_clicked_widget &&
                now - m_interaction_state.last_click_time < m_interaction_state.double_click_time) {
                m_interaction_state.click_count++;
            } else {
                m_interaction_state.click_count = 1;
            }
            m_interaction_state.last_clicked_widget = new_hovered;
            m_interaction_state.last_click_time = now;

            // Set focus
            if (m_interaction_state.focused_widget != new_hovered) {
                // Blur old
                if (m_interaction_state.focused_widget) {
                    auto* old_widget = m_widget_registry.get(m_interaction_state.focused_widget);
                    if (old_widget) {
                        auto it = m_widget_type_to_plugin.find(old_widget->type);
                        if (it != m_widget_type_to_plugin.end()) {
                            it->second->on_blur(m_interaction_state.focused_widget, *old_widget);
                        }
                    }
                }

                // Focus new
                m_interaction_state.focused_widget = new_hovered;
                auto* widget = m_widget_registry.get(new_hovered);
                if (widget) {
                    auto it = m_widget_type_to_plugin.find(widget->type);
                    if (it != m_widget_type_to_plugin.end()) {
                        it->second->on_focus(new_hovered, *widget);
                    }
                }
            }

            // Store drag start
            m_interaction_state.drag_start = m_interaction_state.mouse_position;
        }
    }

    // Handle mouse release
    if (m_interaction_state.is_mouse_released(0)) {
        if (m_interaction_state.pressed_widget && m_interaction_state.pressed_widget == new_hovered) {
            // This is a click
            auto* widget = m_widget_registry.get(m_interaction_state.pressed_widget);
            if (widget) {
                auto it = m_widget_type_to_plugin.find(widget->type);
                if (it != m_widget_type_to_plugin.end()) {
                    if (m_interaction_state.click_count >= 2) {
                        it->second->on_double_click(m_interaction_state.pressed_widget, *widget,
                                                     m_interaction_state.mouse_position);
                    } else {
                        it->second->on_click(m_interaction_state.pressed_widget, *widget,
                                              m_interaction_state.mouse_position);
                    }
                }

                // Dispatch event
                WidgetEvent event;
                event.type = m_interaction_state.click_count >= 2 ?
                             WidgetEventType::DoubleClick : WidgetEventType::Click;
                event.widget = m_interaction_state.pressed_widget;
                event.position = m_interaction_state.mouse_position;
                dispatch_event(m_interaction_state.pressed_widget, event);
            }
        }

        // End drag
        if (m_interaction_state.drag_active) {
            m_interaction_state.drag_active = false;
            auto* widget = m_widget_registry.get(m_interaction_state.dragging_widget);
            if (widget) {
                auto it = m_widget_type_to_plugin.find(widget->type);
                if (it != m_widget_type_to_plugin.end()) {
                    it->second->on_drag_end(m_interaction_state.dragging_widget, *widget,
                                             m_interaction_state.mouse_position);
                }
            }
            m_interaction_state.dragging_widget = WidgetId{0};
        }

        m_interaction_state.pressed_widget = WidgetId{0};
    }

    // Handle drag
    if (m_interaction_state.is_mouse_down(0) && m_interaction_state.pressed_widget) {
        Vec2 delta = m_interaction_state.mouse_position - m_interaction_state.drag_start;
        float threshold = 5.0f;

        if (!m_interaction_state.drag_active &&
            (std::abs(delta.x) > threshold || std::abs(delta.y) > threshold)) {
            // Start drag
            m_interaction_state.drag_active = true;
            m_interaction_state.dragging_widget = m_interaction_state.pressed_widget;

            auto* widget = m_widget_registry.get(m_interaction_state.dragging_widget);
            if (widget) {
                auto it = m_widget_type_to_plugin.find(widget->type);
                if (it != m_widget_type_to_plugin.end()) {
                    it->second->on_drag_start(m_interaction_state.dragging_widget, *widget,
                                               m_interaction_state.drag_start);
                }
            }
        }

        if (m_interaction_state.drag_active && m_interaction_state.mouse_delta.x != 0 ||
            m_interaction_state.mouse_delta.y != 0) {
            auto* widget = m_widget_registry.get(m_interaction_state.dragging_widget);
            if (widget) {
                auto it = m_widget_type_to_plugin.find(widget->type);
                if (it != m_widget_type_to_plugin.end()) {
                    it->second->on_drag(m_interaction_state.dragging_widget, *widget,
                                         m_interaction_state.mouse_delta);
                }
            }
        }
    }

    // Handle keyboard input for focused widget
    if (m_interaction_state.focused_widget) {
        auto* widget = m_widget_registry.get(m_interaction_state.focused_widget);
        if (widget) {
            auto it = m_widget_type_to_plugin.find(widget->type);
            if (it != m_widget_type_to_plugin.end()) {
                // Key presses
                for (int key = 0; key < 512; ++key) {
                    if (m_interaction_state.is_key_pressed(key)) {
                        it->second->on_key_press(m_interaction_state.focused_widget, *widget,
                                                  key, m_interaction_state.modifiers);
                    }
                    if (m_interaction_state.is_key_released(key)) {
                        it->second->on_key_release(m_interaction_state.focused_widget, *widget,
                                                    key, m_interaction_state.modifiers);
                    }
                }

                // Text input
                if (!m_interaction_state.text_input_buffer.empty()) {
                    it->second->on_text_input(m_interaction_state.focused_widget, *widget,
                                               m_interaction_state.text_input_buffer);
                }
            }
        }
    }

    // Clear text input
    m_interaction_state.text_input_buffer.clear();
}

void WidgetStateCore::update(float dt) {
    // Process queued commands
    process_commands();

    // Update animations
    update_animations(dt);

    // Update data bindings
    update_bindings();

    // Update all widget plugins
    for (auto& [name, loaded] : m_loaded_widgets) {
        if (loaded->widget) {
            loaded->widget->update(dt);
        }
    }
}

void WidgetStateCore::layout() {
    // Compute layout for dirty widgets
    if (m_layout_state.has_dirty()) {
        // Start from roots
        for (WidgetId root : m_widget_registry.roots) {
            compute_layout(root);
        }
        m_layout_state.clear_dirty();
    }
}

void WidgetStateCore::render() {
    // Sort layers by z-order
    std::vector<WidgetLayer*> sorted_layers;
    for (auto& layer : m_widget_registry.layers) {
        if (layer.visible) {
            sorted_layers.push_back(&layer);
        }
    }
    std::sort(sorted_layers.begin(), sorted_layers.end(),
              [](const WidgetLayer* a, const WidgetLayer* b) {
                  return a->z_order < b->z_order;
              });

    // Render each layer
    for (WidgetLayer* layer : sorted_layers) {
        m_render_state.current_layer = layer->id;
        m_render_state.current_opacity = layer->opacity;

        // Render widgets in this layer
        for (WidgetId widget_id : layer->widgets) {
            render_widget(widget_id);
        }
    }
}

void WidgetStateCore::end_frame() {
    // Nothing specific needed here currently
}

// -----------------------------------------------------------------------------
// Input Handling
// -----------------------------------------------------------------------------

void WidgetStateCore::set_mouse_position(float x, float y) {
    m_interaction_state.mouse_position = {x, y};
}

void WidgetStateCore::set_mouse_button(int button, bool pressed) {
    if (button >= 0 && button < 8) {
        m_interaction_state.mouse_buttons[button] = pressed;
    }
}

void WidgetStateCore::set_scroll(float delta) {
    m_interaction_state.scroll_delta = delta;

    // Dispatch to hovered widget
    if (m_interaction_state.hovered_widget && delta != 0) {
        auto* widget = m_widget_registry.get(m_interaction_state.hovered_widget);
        if (widget) {
            auto it = m_widget_type_to_plugin.find(widget->type);
            if (it != m_widget_type_to_plugin.end()) {
                it->second->on_scroll(m_interaction_state.hovered_widget, *widget, delta);
            }
        }
    }
}

void WidgetStateCore::set_key(int key, bool pressed) {
    if (key >= 0 && key < 512) {
        m_interaction_state.keys[key] = pressed;
    }
}

void WidgetStateCore::set_modifiers(std::uint32_t mods) {
    m_interaction_state.modifiers = mods;
}

void WidgetStateCore::add_text_input(const std::string& text) {
    m_interaction_state.text_input_buffer += text;
}

void WidgetStateCore::clear_text_input() {
    m_interaction_state.text_input_buffer.clear();
}

// -----------------------------------------------------------------------------
// Screen Management
// -----------------------------------------------------------------------------

void WidgetStateCore::set_screen_size(float width, float height) {
    if (width != m_layout_state.screen_width || height != m_layout_state.screen_height) {
        m_layout_state.screen_width = width;
        m_layout_state.screen_height = height;

        // Mark all widgets as needing layout recalculation
        for (const auto& [id, widget] : m_widget_registry.widgets) {
            m_layout_state.mark_dirty(id);
        }
    }
}

void WidgetStateCore::set_ui_scale(float scale) {
    if (scale != m_layout_state.ui_scale) {
        m_layout_state.ui_scale = scale;

        // Mark all widgets as dirty
        for (const auto& [id, widget] : m_widget_registry.widgets) {
            m_layout_state.mark_dirty(id);
        }
    }
}

Vec2 WidgetStateCore::screen_size() const {
    return m_layout_state.screen_size();
}

// -----------------------------------------------------------------------------
// Widget Management
// -----------------------------------------------------------------------------

WidgetId WidgetStateCore::create_widget(std::string_view type, std::string_view name) {
    WidgetId id{m_widget_registry.next_widget_id++};

    WidgetInstance widget;
    widget.id = id;
    widget.type = std::string(type);
    widget.name = std::string(name);
    widget.visibility = Visibility::Visible;
    widget.state = WidgetState::None;
    widget.interactive = true;

    // Assign to default layer
    if (!m_widget_registry.layers.empty()) {
        widget.layer = m_widget_registry.layers[0].id;
        m_widget_registry.layers[0].widgets.push_back(id);
        m_widget_registry.widget_layer[id] = widget.layer;
    }

    m_widget_registry.widgets[id] = std::move(widget);

    // Add to roots (no parent initially)
    m_widget_registry.roots.push_back(id);

    // Register name if provided
    if (!name.empty()) {
        m_widget_registry.named_widgets[std::string(name)] = id;
    }

    // Create default layout
    LayoutData layout;
    layout.position = {0, 0};
    layout.size = {100, 100};
    m_layout_state.layouts[id] = layout;
    m_layout_state.mark_dirty(id);

    return id;
}

void WidgetStateCore::destroy_widget(WidgetId id) {
    auto* widget = m_widget_registry.get(id);
    if (!widget) return;

    // Destroy children first (recursive)
    auto children = m_widget_registry.get_children(id);
    for (WidgetId child : children) {
        destroy_widget(child);
    }

    // Remove from parent's children list
    WidgetId parent_id = m_widget_registry.get_parent(id);
    if (parent_id) {
        auto& parent_children = m_widget_registry.children[parent_id];
        parent_children.erase(
            std::remove(parent_children.begin(), parent_children.end(), id),
            parent_children.end());
    } else {
        // Remove from roots
        m_widget_registry.roots.erase(
            std::remove(m_widget_registry.roots.begin(), m_widget_registry.roots.end(), id),
            m_widget_registry.roots.end());
    }

    // Remove from layer
    if (widget->layer) {
        for (auto& layer : m_widget_registry.layers) {
            if (layer.id == widget->layer) {
                layer.widgets.erase(
                    std::remove(layer.widgets.begin(), layer.widgets.end(), id),
                    layer.widgets.end());
                break;
            }
        }
    }

    // Remove name mapping
    if (!widget->name.empty()) {
        m_widget_registry.named_widgets.erase(widget->name);
    }

    // Clear interaction state if this widget had it
    if (m_interaction_state.hovered_widget == id) m_interaction_state.hovered_widget = WidgetId{0};
    if (m_interaction_state.pressed_widget == id) m_interaction_state.pressed_widget = WidgetId{0};
    if (m_interaction_state.focused_widget == id) m_interaction_state.focused_widget = WidgetId{0};
    if (m_interaction_state.dragging_widget == id) m_interaction_state.dragging_widget = WidgetId{0};

    // Remove from all state stores
    m_widget_registry.widgets.erase(id);
    m_widget_registry.children.erase(id);
    m_widget_registry.parent.erase(id);
    m_widget_registry.widget_layer.erase(id);
    m_layout_state.layouts.erase(id);
    m_layout_state.computed_bounds.erase(id);
    m_layout_state.constraints.erase(id);
    m_style_state.overrides.erase(id);
    m_style_state.computed_styles.erase(id);
    m_animation_state.animations.erase(id);
    m_animation_state.transitions.erase(id);
    m_binding_state.bindings.erase(id);
}

void WidgetStateCore::set_parent(WidgetId child, WidgetId parent) {
    if (!m_widget_registry.exists(child)) return;

    // Remove from old parent or roots
    WidgetId old_parent = m_widget_registry.get_parent(child);
    if (old_parent) {
        auto& old_children = m_widget_registry.children[old_parent];
        old_children.erase(
            std::remove(old_children.begin(), old_children.end(), child),
            old_children.end());
    } else {
        m_widget_registry.roots.erase(
            std::remove(m_widget_registry.roots.begin(), m_widget_registry.roots.end(), child),
            m_widget_registry.roots.end());
    }

    // Set new parent
    if (parent && m_widget_registry.exists(parent)) {
        m_widget_registry.parent[child] = parent;
        m_widget_registry.children[parent].push_back(child);
    } else {
        m_widget_registry.parent.erase(child);
        m_widget_registry.roots.push_back(child);
    }

    m_layout_state.mark_dirty(child);
}

const WidgetInstance* WidgetStateCore::get_widget(WidgetId id) const {
    return m_widget_registry.get(id);
}

WidgetId WidgetStateCore::find_widget(std::string_view name) const {
    return m_widget_registry.find_by_name(name);
}

// -----------------------------------------------------------------------------
// Layer Management
// -----------------------------------------------------------------------------

LayerId WidgetStateCore::create_layer(std::string_view name, int z_order) {
    LayerId id{m_widget_registry.next_layer_id++};

    WidgetLayer layer;
    layer.id = id;
    layer.name = std::string(name);
    layer.z_order = z_order;
    layer.visible = true;
    layer.interactive = true;

    m_widget_registry.layers.push_back(layer);
    return id;
}

void WidgetStateCore::destroy_layer(LayerId id) {
    // Move widgets to default layer
    for (auto& layer : m_widget_registry.layers) {
        if (layer.id == id) {
            if (!m_widget_registry.layers.empty()) {
                LayerId default_layer = m_widget_registry.layers[0].id;
                for (WidgetId widget_id : layer.widgets) {
                    m_widget_registry.widget_layer[widget_id] = default_layer;
                    m_widget_registry.layers[0].widgets.push_back(widget_id);
                }
            }
            break;
        }
    }

    // Remove layer
    m_widget_registry.layers.erase(
        std::remove_if(m_widget_registry.layers.begin(), m_widget_registry.layers.end(),
                       [id](const WidgetLayer& l) { return l.id == id; }),
        m_widget_registry.layers.end());
}

const WidgetLayer* WidgetStateCore::get_layer(LayerId id) const {
    for (const auto& layer : m_widget_registry.layers) {
        if (layer.id == id) return &layer;
    }
    return nullptr;
}

// -----------------------------------------------------------------------------
// Theme Management
// -----------------------------------------------------------------------------

void WidgetStateCore::register_theme(const Theme& theme) {
    m_style_state.themes[theme.name] = theme;
}

void WidgetStateCore::apply_theme(std::string_view name) {
    if (m_style_state.themes.count(std::string(name))) {
        m_style_state.active_theme = std::string(name);

        // Mark all widgets as needing style recomputation
        for (const auto& [id, widget] : m_widget_registry.widgets) {
            m_style_state.computed_styles.erase(id);
        }
    }
}

const Theme* WidgetStateCore::current_theme() const {
    return m_style_state.current_theme();
}

// -----------------------------------------------------------------------------
// Data Source Management
// -----------------------------------------------------------------------------

void WidgetStateCore::register_data_source(std::string_view name, IDataSource* source) {
    m_binding_state.sources[std::string(name)] = source;
}

void WidgetStateCore::register_data_source(std::string_view name, DataSourceCallback callback) {
    m_data_source_callbacks[std::string(name)] = std::move(callback);
}

void WidgetStateCore::unregister_data_source(std::string_view name) {
    m_binding_state.sources.erase(std::string(name));
    m_data_source_callbacks.erase(std::string(name));
}

void WidgetStateCore::set_game_state(void_gamestate::GameStateCore* game_state) {
    m_game_state = game_state;
}

bool WidgetStateCore::load_widget_plugin(const std::filesystem::path& path) {
    return watcher_load_widget(path);
}

void WidgetStateCore::configure_watcher(const WidgetWatcherConfig& config) {
    m_watcher_config = config;
    m_watch_paths = config.watch_paths;
}

// -----------------------------------------------------------------------------
// Widget Plugin Management
// -----------------------------------------------------------------------------

bool WidgetStateCore::watcher_load_widget(const std::filesystem::path& path) {
    // Extract plugin name from path
    std::string filename = path.stem().string();

    // Check if already loaded
    if (m_loaded_widgets.count(filename)) {
        return false;
    }

    auto loaded = std::make_unique<LoadedWidget>();
    loaded->name = filename;
    loaded->library = std::make_unique<void_plugin_api::DynamicLibrary>();

    if (!loaded->library->load(path)) {
        spdlog::error("Failed to load widget library {}: {}", path.string(), loaded->library->error());
        return false;
    }

    // Get factory functions
    auto create_func = loaded->library->get_function<CreateWidgetFunc>("create_widget");
    auto destroy_func = loaded->library->get_function<DestroyWidgetFunc>("destroy_widget");

    if (!create_func || !destroy_func) {
        spdlog::error("Widget {} missing factory functions", filename);
        return false;
    }

    // Create the widget plugin instance
    loaded->widget = create_func();
    loaded->destroy_func = destroy_func;

    if (!loaded->widget) {
        spdlog::error("Failed to create widget instance: {}", filename);
        return false;
    }

    // Initialize the widget plugin
    auto result = loaded->widget->on_widget_load();
    if (!result) {
        spdlog::error("Widget {} on_widget_load failed", filename);
        return false;
    }

    // Register widget types provided by this plugin
    for (const auto& widget_type : loaded->widget->provided_widgets()) {
        m_widget_type_to_plugin[widget_type] = loaded->widget;
    }

    m_loaded_widgets[filename] = std::move(loaded);

    spdlog::info("Widget plugin loaded: {} (provides: {})", filename,
                 m_loaded_widgets[filename]->widget->provided_widgets().size());
    return true;
}

bool WidgetStateCore::watcher_unload_widget(const std::string& name) {
    auto it = m_loaded_widgets.find(name);
    if (it == m_loaded_widgets.end()) return false;

    // Remove widget type mappings
    if (it->second->widget) {
        for (const auto& widget_type : it->second->widget->provided_widgets()) {
            m_widget_type_to_plugin.erase(widget_type);
        }
        it->second->widget->on_widget_unload();
    }

    m_loaded_widgets.erase(it);
    return true;
}

bool WidgetStateCore::watcher_hot_reload_widget(const std::string& name,
                                                 const std::filesystem::path& new_path) {
    // Unload old
    watcher_unload_widget(name);

    // Load new
    return watcher_load_widget(new_path);
}

bool WidgetStateCore::watcher_is_widget_loaded(const std::string& name) const {
    return m_loaded_widgets.count(name) > 0;
}

std::vector<std::string> WidgetStateCore::watcher_loaded_widgets() const {
    std::vector<std::string> names;
    names.reserve(m_loaded_widgets.size());
    for (const auto& [name, loaded] : m_loaded_widgets) {
        names.push_back(name);
    }
    return names;
}

std::size_t WidgetStateCore::active_widget_count() const {
    return m_widget_registry.widgets.size();
}

std::size_t WidgetStateCore::active_plugin_count() const {
    return m_loaded_widgets.size();
}

Widget* WidgetStateCore::get_widget_plugin(std::string_view name) const {
    auto it = m_loaded_widgets.find(std::string(name));
    return it != m_loaded_widgets.end() ? it->second->widget : nullptr;
}

// -----------------------------------------------------------------------------
// Widget Watcher
// -----------------------------------------------------------------------------

void WidgetStateCore::start_watching(const std::vector<std::filesystem::path>& paths) {
    if (m_watching) return;

    m_watch_paths = paths;
    if (m_watch_paths.empty()) {
        m_watch_paths.push_back(std::filesystem::current_path() / "widgets");
    }

    // Scan for existing widgets
    for (const auto& watch_path : m_watch_paths) {
        if (!std::filesystem::exists(watch_path)) continue;

        for (const auto& entry : std::filesystem::directory_iterator(watch_path)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::string expected_ext = void_plugin_api::native_plugin_extension();

            if (ext == expected_ext) {
                watcher_load_widget(entry.path());
            }
        }
    }

    m_watching = true;
}

void WidgetStateCore::stop_watching() {
    m_watching = false;
}

bool WidgetStateCore::is_watching() const {
    return m_watching;
}

// -----------------------------------------------------------------------------
// Statistics
// -----------------------------------------------------------------------------

WidgetStateCore::Stats WidgetStateCore::stats() const {
    Stats s;
    s.total_widgets = m_widget_registry.widgets.size();
    s.layers = m_widget_registry.layers.size();
    s.active_plugins = m_loaded_widgets.size();

    // Count visible widgets
    for (const auto& [id, widget] : m_widget_registry.widgets) {
        if (widget.visibility == Visibility::Visible) {
            s.visible_widgets++;
        }
    }

    // Count active animations
    for (const auto& [id, anims] : m_animation_state.animations) {
        for (const auto& anim : anims) {
            if (anim.state == AnimState::Playing) {
                s.active_animations++;
            }
        }
    }

    // Count active bindings
    for (const auto& [id, bindings] : m_binding_state.bindings) {
        s.active_bindings += bindings.size();
    }

    s.draw_calls = m_render_state.draw_calls;
    s.triangles = m_render_state.triangles;

    return s;
}

// -----------------------------------------------------------------------------
// Private Helpers
// -----------------------------------------------------------------------------

void WidgetStateCore::process_commands() {
    for (auto& cmd : m_command_queue) {
        cmd->execute(*this);
    }
    m_command_queue.clear();
}

void WidgetStateCore::update_animations(float dt) {
    m_animation_state.current_time += dt;

    for (auto& [widget_id, anims] : m_animation_state.animations) {
        for (auto& anim : anims) {
            if (anim.state != AnimState::Playing) continue;

            anim.elapsed += dt * anim.play_direction;

            if (anim.elapsed >= anim.duration) {
                // Animation finished
                switch (anim.play_mode) {
                    case PlayMode::Once:
                        anim.state = AnimState::Finished;
                        break;
                    case PlayMode::Loop:
                        anim.elapsed = 0;
                        anim.loops_completed++;
                        break;
                    case PlayMode::PingPong:
                        anim.play_direction = -1;
                        anim.elapsed = anim.duration;
                        break;
                    case PlayMode::Reverse:
                        anim.state = AnimState::Finished;
                        break;
                }
            } else if (anim.elapsed <= 0 && anim.play_direction < 0) {
                // Ping-pong back
                anim.play_direction = 1;
                anim.elapsed = 0;
                anim.loops_completed++;

                if (anim.max_loops > 0 && anim.loops_completed >= anim.max_loops) {
                    anim.state = AnimState::Finished;
                }
            }

            // Apply animation value
            // TODO: Interpolate keyframes and apply to widget property
        }
    }

    // Clean up finished animations
    for (auto& [widget_id, anims] : m_animation_state.animations) {
        anims.erase(
            std::remove_if(anims.begin(), anims.end(),
                           [](const ActiveAnimation& a) { return a.state == AnimState::Finished; }),
            anims.end());
    }
}

void WidgetStateCore::update_bindings() {
    // Process pending binding updates
    for (const auto& update : m_binding_state.pending_updates) {
        auto* binding = m_binding_state.get_binding(update.binding);
        if (!binding || !binding->enabled) continue;

        // Apply new value to widget property
        auto* widget = m_widget_registry.get_mut(binding->widget);
        if (widget) {
            widget->set_property(binding->target_property, update.new_value);
        }
    }
    m_binding_state.pending_updates.clear();

    // Update bindings from sources
    for (auto& [widget_id, bindings] : m_binding_state.bindings) {
        for (auto& binding : bindings) {
            if (!binding.enabled) continue;

            // Parse source path (e.g., "player.health" -> source="player", path="health")
            auto dot_pos = binding.source_path.find('.');
            std::string source_name = dot_pos != std::string::npos ?
                                      binding.source_path.substr(0, dot_pos) : binding.source_path;
            std::string path = dot_pos != std::string::npos ?
                               binding.source_path.substr(dot_pos + 1) : "";

            auto* source = m_binding_state.get_source(source_name);
            if (!source) continue;

            std::any value = source->get_value(path);
            if (value.has_value()) {
                auto* widget = m_widget_registry.get_mut(widget_id);
                if (widget) {
                    widget->set_property(binding.target_property, value);
                }
            }
        }
    }
}

void WidgetStateCore::compute_layout(WidgetId id) {
    auto* widget = m_widget_registry.get(id);
    auto* layout = m_layout_state.get(id);
    if (!widget || !layout) return;

    // Get parent bounds
    Rect parent_bounds{0, 0, m_layout_state.screen_width, m_layout_state.screen_height};
    WidgetId parent_id = m_widget_registry.get_parent(id);
    if (parent_id) {
        parent_bounds = m_layout_state.get_bounds(parent_id);
    }

    // Compute position
    float x = layout->position.x;
    float y = layout->position.y;

    switch (layout->position_mode) {
        case PositionMode::Absolute:
            // Position is in screen coordinates
            break;
        case PositionMode::Relative:
            // Position is relative to parent
            x += parent_bounds.x;
            y += parent_bounds.y;
            break;
        case PositionMode::Anchored:
            // Position from anchor point
            x = parent_bounds.x + parent_bounds.width * layout->anchor.x + layout->position.x;
            y = parent_bounds.y + parent_bounds.height * layout->anchor.y + layout->position.y;
            break;
        case PositionMode::WorldSpace:
            // Would need 3D->2D projection
            break;
    }

    // Compute size
    float width = layout->size.x;
    float height = layout->size.y;

    if (layout->width_mode == SizeMode::Relative) {
        width = parent_bounds.width * layout->size.x;
    } else if (layout->width_mode == SizeMode::Fill) {
        width = parent_bounds.width - layout->margin.horizontal();
    } else if (layout->width_mode == SizeMode::FitContent) {
        // Ask plugin for content size
        auto it = m_widget_type_to_plugin.find(widget->type);
        if (it != m_widget_type_to_plugin.end()) {
            Vec2 content_size = it->second->measure_widget(id, *widget, {parent_bounds.width, parent_bounds.height});
            width = content_size.x;
        }
    }

    if (layout->height_mode == SizeMode::Relative) {
        height = parent_bounds.height * layout->size.y;
    } else if (layout->height_mode == SizeMode::Fill) {
        height = parent_bounds.height - layout->margin.vertical();
    } else if (layout->height_mode == SizeMode::FitContent) {
        auto it = m_widget_type_to_plugin.find(widget->type);
        if (it != m_widget_type_to_plugin.end()) {
            Vec2 content_size = it->second->measure_widget(id, *widget, {parent_bounds.width, parent_bounds.height});
            height = content_size.y;
        }
    }

    // Apply constraints
    if (layout->min_width > 0) width = std::max(width, layout->min_width);
    if (layout->min_height > 0) height = std::max(height, layout->min_height);
    if (layout->max_width > 0) width = std::min(width, layout->max_width);
    if (layout->max_height > 0) height = std::min(height, layout->max_height);

    // Apply pivot offset
    x -= width * layout->pivot.x;
    y -= height * layout->pivot.y;

    // Apply margins
    x += layout->margin.left;
    y += layout->margin.top;

    // Apply UI scale
    x *= m_layout_state.ui_scale;
    y *= m_layout_state.ui_scale;
    width *= m_layout_state.ui_scale;
    height *= m_layout_state.ui_scale;

    // Store computed bounds
    m_layout_state.computed_bounds[id] = {x, y, width, height};

    // Recursively compute children
    for (WidgetId child : m_widget_registry.get_children(id)) {
        compute_layout(child);
    }
}

void WidgetStateCore::render_widget(WidgetId id) {
    auto* widget = m_widget_registry.get(id);
    if (!widget || widget->visibility != Visibility::Visible) return;

    // Find the plugin that handles this widget type
    auto it = m_widget_type_to_plugin.find(widget->type);
    if (it != m_widget_type_to_plugin.end()) {
        // Apply clipping if needed
        if (widget->clip_children) {
            Rect bounds = m_layout_state.get_bounds(id);
            m_render_state.scissor_stack.push_back(bounds);
        }

        // Render the widget
        it->second->render_widget(id, *widget);
        m_render_state.widgets_rendered++;

        // Render children
        for (WidgetId child : m_widget_registry.get_children(id)) {
            render_widget(child);
        }

        // Pop clipping
        if (widget->clip_children && !m_render_state.scissor_stack.empty()) {
            m_render_state.scissor_stack.pop_back();
        }
    }
}

WidgetId WidgetStateCore::hit_test(Vec2 point) const {
    // Test from top to bottom (reverse z-order)
    std::vector<const WidgetLayer*> sorted_layers;
    for (const auto& layer : m_widget_registry.layers) {
        if (layer.visible && layer.interactive) {
            sorted_layers.push_back(&layer);
        }
    }
    std::sort(sorted_layers.begin(), sorted_layers.end(),
              [](const WidgetLayer* a, const WidgetLayer* b) {
                  return a->z_order > b->z_order;  // Reverse order
              });

    for (const WidgetLayer* layer : sorted_layers) {
        // Test widgets in reverse order (last drawn = on top)
        for (auto it = layer->widgets.rbegin(); it != layer->widgets.rend(); ++it) {
            WidgetId widget_id = *it;
            auto* widget = m_widget_registry.get(widget_id);
            if (!widget || widget->visibility != Visibility::Visible || !widget->interactive) continue;
            if (has_state(widget->state, WidgetState::Disabled)) continue;

            Rect bounds = m_layout_state.get_bounds(widget_id);
            if (bounds.contains(point)) {
                // Check children first (they're on top)
                auto children = m_widget_registry.get_children(widget_id);
                for (auto child_it = children.rbegin(); child_it != children.rend(); ++child_it) {
                    WidgetId child_hit = hit_test_recursive(*child_it, point);
                    if (child_hit) return child_hit;
                }
                return widget_id;
            }
        }
    }

    return WidgetId{0};
}

WidgetId WidgetStateCore::hit_test_recursive(WidgetId id, Vec2 point) const {
    auto* widget = m_widget_registry.get(id);
    if (!widget || widget->visibility != Visibility::Visible || !widget->interactive) {
        return WidgetId{0};
    }
    if (has_state(widget->state, WidgetState::Disabled)) return WidgetId{0};

    Rect bounds = m_layout_state.get_bounds(id);
    if (!bounds.contains(point)) return WidgetId{0};

    // Check children first
    auto children = m_widget_registry.get_children(id);
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        WidgetId child_hit = hit_test_recursive(*it, point);
        if (child_hit) return child_hit;
    }

    return id;
}

void WidgetStateCore::dispatch_event(WidgetId id, const WidgetEvent& event) {
    if (m_on_click && event.type == WidgetEventType::Click) {
        m_on_click(id, event);
    }
    // TODO: Dispatch to registered callbacks in WidgetAPIImpl
}

// =============================================================================
// WidgetAPIImpl Implementation
// =============================================================================

WidgetAPIImpl::WidgetAPIImpl(WidgetStateCore* core, void_gamestate::GameStateCore* game_state)
    : m_core(core), m_game_state(game_state) {}

WidgetAPIImpl::~WidgetAPIImpl() = default;

const WidgetRegistry& WidgetAPIImpl::registry() const { return m_core->widget_registry(); }
const LayoutState& WidgetAPIImpl::layout() const { return m_core->layout_state(); }
const StyleState& WidgetAPIImpl::style() const { return m_core->style_state(); }
const InteractionState& WidgetAPIImpl::interaction() const { return m_core->interaction_state(); }
const AnimationState& WidgetAPIImpl::animation() const { return m_core->animation_state(); }
const BindingState& WidgetAPIImpl::bindings() const { return m_core->binding_state(); }

const WidgetInstance* WidgetAPIImpl::get_widget(WidgetId id) const {
    return m_core->get_widget(id);
}

WidgetId WidgetAPIImpl::find_widget(std::string_view name) const {
    return m_core->find_widget(name);
}

std::vector<WidgetId> WidgetAPIImpl::find_widgets_by_type(std::string_view type) const {
    return m_core->widget_registry().find_by_type(type);
}

std::vector<WidgetId> WidgetAPIImpl::get_children(WidgetId parent) const {
    return m_core->widget_registry().get_children(parent);
}

WidgetId WidgetAPIImpl::get_parent(WidgetId child) const {
    return m_core->widget_registry().get_parent(child);
}

Rect WidgetAPIImpl::get_bounds(WidgetId id) const {
    return m_core->layout_state().get_bounds(id);
}

ComputedStyle WidgetAPIImpl::get_computed_style(WidgetId id) const {
    // Check cache first
    const auto* cached = m_core->style_state().get_computed(id);
    if (cached) return *cached;

    // Compute from theme + overrides
    ComputedStyle computed;
    const Theme* theme = m_core->current_theme();
    if (theme) {
        computed.background_color = theme->panel_background;
        computed.border_color = theme->panel_border;
        computed.text_color = theme->text_primary;
        computed.border_width = theme->border_width;
        computed.border_radius = theme->border_radius;
        computed.font_size = theme->text_size;
    }

    // Apply overrides
    const auto* overrides = m_core->style_state().get_overrides(id);
    if (overrides) {
        if (overrides->background_color) computed.background_color = *overrides->background_color;
        if (overrides->border_color) computed.border_color = *overrides->border_color;
        if (overrides->text_color) computed.text_color = *overrides->text_color;
        if (overrides->border_width) computed.border_width = *overrides->border_width;
        if (overrides->border_radius) computed.border_radius = *overrides->border_radius;
        if (overrides->opacity) computed.opacity = *overrides->opacity;
        if (overrides->font) computed.font = *overrides->font;
        if (overrides->font_size) computed.font_size = *overrides->font_size;
    }

    return computed;
}

bool WidgetAPIImpl::is_hovered(WidgetId id) const {
    return m_core->interaction_state().is_hovered(id);
}

bool WidgetAPIImpl::is_pressed(WidgetId id) const {
    return m_core->interaction_state().is_pressed(id);
}

bool WidgetAPIImpl::is_focused(WidgetId id) const {
    return m_core->interaction_state().is_focused(id);
}

bool WidgetAPIImpl::is_visible(WidgetId id) const {
    const auto* widget = m_core->get_widget(id);
    return widget && widget->visibility == Visibility::Visible;
}

bool WidgetAPIImpl::hit_test(WidgetId id, Vec2 point) const {
    Rect bounds = get_bounds(id);
    return bounds.contains(point);
}

void WidgetAPIImpl::submit_command(std::unique_ptr<IWidgetCommand> cmd) {
    // Execute immediately for now
    // TODO: Queue for batch processing if needed
    cmd->execute(*m_core);
}

WidgetId WidgetAPIImpl::create_widget(std::string_view type, std::string_view name) {
    return m_core->create_widget(type, name);
}

WidgetId WidgetAPIImpl::create_from_template(std::string_view template_name, std::string_view name) {
    // TODO: Implement template instantiation
    return WidgetId{0};
}

void WidgetAPIImpl::destroy_widget(WidgetId id) {
    m_core->destroy_widget(id);
}

void WidgetAPIImpl::set_parent(WidgetId child, WidgetId parent) {
    m_core->set_parent(child, parent);
}

void WidgetAPIImpl::set_layer(WidgetId id, LayerId layer) {
    auto cmd = std::make_unique<SetLayerCommand>(id, layer);
    cmd->execute(*m_core);
}

void WidgetAPIImpl::set_position(WidgetId id, Vec2 pos) {
    auto* layout = m_core->layout_state().get_mut(id);
    if (layout) {
        layout->position = pos;
        m_core->layout_state().mark_dirty(id);
    }
}

void WidgetAPIImpl::set_size(WidgetId id, Vec2 size) {
    auto* layout = m_core->layout_state().get_mut(id);
    if (layout) {
        layout->size = size;
        m_core->layout_state().mark_dirty(id);
    }
}

void WidgetAPIImpl::set_anchor(WidgetId id, Vec2 anchor) {
    auto* layout = m_core->layout_state().get_mut(id);
    if (layout) {
        layout->anchor = anchor;
        m_core->layout_state().mark_dirty(id);
    }
}

void WidgetAPIImpl::set_anchor(WidgetId id, Anchor anchor) {
    set_anchor(id, anchor_to_vec2(anchor));
}

void WidgetAPIImpl::set_pivot(WidgetId id, Vec2 pivot) {
    auto* layout = m_core->layout_state().get_mut(id);
    if (layout) {
        layout->pivot = pivot;
        m_core->layout_state().mark_dirty(id);
    }
}

void WidgetAPIImpl::set_margin(WidgetId id, Insets margin) {
    auto* layout = m_core->layout_state().get_mut(id);
    if (layout) {
        layout->margin = margin;
        m_core->layout_state().mark_dirty(id);
    }
}

void WidgetAPIImpl::set_padding(WidgetId id, Insets padding) {
    auto* layout = m_core->layout_state().get_mut(id);
    if (layout) {
        layout->padding = padding;
        m_core->layout_state().mark_dirty(id);
    }
}

void WidgetAPIImpl::set_visible(WidgetId id, bool visible) {
    auto* widget = m_core->widget_registry().get_mut(id);
    if (widget) {
        widget->visibility = visible ? Visibility::Visible : Visibility::Hidden;
    }
}

void WidgetAPIImpl::set_rotation(WidgetId id, float degrees) {
    auto* layout = m_core->layout_state().get_mut(id);
    if (layout) {
        layout->rotation = degrees;
        m_core->layout_state().mark_dirty(id);
    }
}

void WidgetAPIImpl::set_scale(WidgetId id, Vec2 scale) {
    auto* layout = m_core->layout_state().get_mut(id);
    if (layout) {
        layout->scale = scale;
        m_core->layout_state().mark_dirty(id);
    }
}

void WidgetAPIImpl::set_style(WidgetId id, std::string_view property, const std::any& value) {
    auto cmd = std::make_unique<SetStylePropertyCommand>(id, std::string(property), value);
    cmd->execute(*m_core);
}

void WidgetAPIImpl::set_background_color(WidgetId id, const Color& color) {
    m_core->style_state().overrides[id].background_color = color;
    m_core->style_state().computed_styles.erase(id);
}

void WidgetAPIImpl::set_border_color(WidgetId id, const Color& color) {
    m_core->style_state().overrides[id].border_color = color;
    m_core->style_state().computed_styles.erase(id);
}

void WidgetAPIImpl::set_text_color(WidgetId id, const Color& color) {
    m_core->style_state().overrides[id].text_color = color;
    m_core->style_state().computed_styles.erase(id);
}

void WidgetAPIImpl::set_opacity(WidgetId id, float opacity) {
    m_core->style_state().overrides[id].opacity = opacity;
    m_core->style_state().computed_styles.erase(id);
}

void WidgetAPIImpl::apply_theme(std::string_view theme_name) {
    m_core->apply_theme(theme_name);
}

AnimationId WidgetAPIImpl::play_animation(WidgetId id, std::string_view anim_name) {
    auto cmd = std::make_unique<PlayAnimationCommand>(id, std::string(anim_name));
    cmd->execute(*m_core);
    return cmd->animation_id();
}

void WidgetAPIImpl::stop_animation(WidgetId id, AnimationId anim) {
    auto cmd = std::make_unique<StopAnimationCommand>(id, anim);
    cmd->execute(*m_core);
}

void WidgetAPIImpl::stop_all_animations(WidgetId id) {
    stop_animation(id, AnimationId{0});
}

AnimationId WidgetAPIImpl::animate_property(WidgetId id, std::string_view property,
                                             const std::any& target, float duration,
                                             EasingType easing) {
    auto cmd = std::make_unique<AnimatePropertyCommand>(id, std::string(property), target, duration, easing);
    cmd->execute(*m_core);
    return cmd->animation_id();
}

AnimationId WidgetAPIImpl::fade_in(WidgetId id, float duration) {
    return animate_property(id, "opacity", 1.0f, duration, EasingType::EaseOutQuad);
}

AnimationId WidgetAPIImpl::fade_out(WidgetId id, float duration) {
    return animate_property(id, "opacity", 0.0f, duration, EasingType::EaseOutQuad);
}

AnimationId WidgetAPIImpl::slide_in(WidgetId id, Vec2 from, float duration) {
    auto* layout = m_core->layout_state().get_mut(id);
    if (!layout) return AnimationId{0};

    Vec2 target = layout->position;
    layout->position = from;
    return animate_property(id, "position", target, duration, EasingType::EaseOutCubic);
}

AnimationId WidgetAPIImpl::slide_out(WidgetId id, Vec2 to, float duration) {
    return animate_property(id, "position", to, duration, EasingType::EaseInCubic);
}

BindingId WidgetAPIImpl::bind(WidgetId id, std::string_view property,
                               std::string_view source_path, BindingMode mode) {
    auto cmd = std::make_unique<BindPropertyCommand>(id, std::string(property),
                                                      std::string(source_path), mode);
    cmd->execute(*m_core);
    return cmd->binding_id();
}

void WidgetAPIImpl::unbind(BindingId binding) {
    auto cmd = std::make_unique<UnbindPropertyCommand>(binding);
    cmd->execute(*m_core);
}

void WidgetAPIImpl::unbind_all(WidgetId id) {
    m_core->binding_state().bindings.erase(id);
}

void WidgetAPIImpl::set_text(WidgetId id, std::string_view text) {
    auto* widget = m_core->widget_registry().get_mut(id);
    if (widget) {
        widget->set_property("text", std::string(text));
    }
}

std::string WidgetAPIImpl::get_text(WidgetId id) const {
    const auto* widget = m_core->get_widget(id);
    if (widget) {
        return widget->get_property<std::string>("text", "");
    }
    return "";
}

void WidgetAPIImpl::set_value(WidgetId id, float value) {
    auto* widget = m_core->widget_registry().get_mut(id);
    if (widget) {
        widget->set_property("value", value);
    }
}

float WidgetAPIImpl::get_value(WidgetId id) const {
    const auto* widget = m_core->get_widget(id);
    if (widget) {
        return widget->get_property<float>("value", 0.0f);
    }
    return 0.0f;
}

void WidgetAPIImpl::set_checked(WidgetId id, bool checked) {
    auto* widget = m_core->widget_registry().get_mut(id);
    if (widget) {
        if (checked) {
            widget->state = widget->state | WidgetState::Checked;
        } else {
            widget->state = static_cast<WidgetState>(
                static_cast<uint32_t>(widget->state) & ~static_cast<uint32_t>(WidgetState::Checked));
        }
    }
}

bool WidgetAPIImpl::is_checked(WidgetId id) const {
    const auto* widget = m_core->get_widget(id);
    return widget && has_state(widget->state, WidgetState::Checked);
}

void WidgetAPIImpl::set_enabled(WidgetId id, bool enabled) {
    auto* widget = m_core->widget_registry().get_mut(id);
    if (widget) {
        if (!enabled) {
            widget->state = widget->state | WidgetState::Disabled;
        } else {
            widget->state = static_cast<WidgetState>(
                static_cast<uint32_t>(widget->state) & ~static_cast<uint32_t>(WidgetState::Disabled));
        }
    }
}

bool WidgetAPIImpl::is_enabled(WidgetId id) const {
    const auto* widget = m_core->get_widget(id);
    return widget && !has_state(widget->state, WidgetState::Disabled);
}

void WidgetAPIImpl::set_property(WidgetId id, std::string_view key, const std::any& value) {
    auto* widget = m_core->widget_registry().get_mut(id);
    if (widget) {
        widget->properties[std::string(key)] = value;
    }
}

std::any WidgetAPIImpl::get_property(WidgetId id, std::string_view key) const {
    const auto* widget = m_core->get_widget(id);
    if (widget) {
        auto it = widget->properties.find(std::string(key));
        if (it != widget->properties.end()) {
            return it->second;
        }
    }
    return {};
}

// Drawing API - these add commands to the render state
void WidgetAPIImpl::draw_rect(const Rect& rect, const Color& color) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Rect;
    cmd.bounds = rect;
    cmd.color = color;
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
}

void WidgetAPIImpl::draw_rect_outline(const Rect& rect, const Color& color, float width) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::RectOutline;
    cmd.bounds = rect;
    cmd.color = color;
    cmd.param1 = width;
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
}

void WidgetAPIImpl::draw_rounded_rect(const Rect& rect, const Color& color, float radius) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::RoundedRect;
    cmd.bounds = rect;
    cmd.color = color;
    cmd.param1 = radius;
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
}

void WidgetAPIImpl::draw_rounded_rect_outline(const Rect& rect, const Color& color,
                                               float radius, float width) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::RoundedRect;
    cmd.bounds = rect;
    cmd.color = color;
    cmd.param1 = radius;
    cmd.param2 = width;
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
}

void WidgetAPIImpl::draw_text(const std::string& text, Vec2 pos, const Color& color,
                               float size, std::string_view font) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Text;
    cmd.bounds = {pos.x, pos.y, 0, 0};
    cmd.color = color;
    cmd.text = text;
    cmd.param1 = size;
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
}

void WidgetAPIImpl::draw_text_aligned(const std::string& text, const Rect& rect,
                                       TextAlign h_align, VerticalAlign v_align,
                                       const Color& color, float size) {
    // Calculate position based on alignment
    // For now, just draw at rect position
    draw_text(text, rect.position(), color, size, "default");
}

void WidgetAPIImpl::draw_icon(std::string_view icon, Vec2 pos, Vec2 size, const Color& tint) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Texture;
    cmd.bounds = {pos.x, pos.y, size.x, size.y};
    cmd.color = tint;
    cmd.texture = std::string(icon);
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
}

void WidgetAPIImpl::draw_line(Vec2 from, Vec2 to, const Color& color, float width) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Line;
    cmd.bounds = {from.x, from.y, to.x - from.x, to.y - from.y};
    cmd.color = color;
    cmd.param1 = width;
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
}

void WidgetAPIImpl::draw_texture(std::string_view texture, const Rect& dest,
                                  const Rect& src, const Color& tint) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Texture;
    cmd.bounds = dest;
    cmd.texture_rect = src;
    cmd.color = tint;
    cmd.texture = std::string(texture);
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
}

void WidgetAPIImpl::draw_circle(Vec2 center, float radius, const Color& color) {
    // Approximate with rounded rect for now
    draw_rounded_rect({center.x - radius, center.y - radius, radius * 2, radius * 2}, color, radius);
}

void WidgetAPIImpl::draw_circle_outline(Vec2 center, float radius, const Color& color, float width) {
    draw_rounded_rect_outline({center.x - radius, center.y - radius, radius * 2, radius * 2},
                               color, radius, width);
}

void WidgetAPIImpl::push_scissor(const Rect& rect) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Scissor;
    cmd.bounds = rect;
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
    m_core->render_state().scissor_stack.push_back(rect);
}

void WidgetAPIImpl::pop_scissor() {
    DrawCommand cmd;
    cmd.type = DrawCommandType::ScissorPop;
    m_core->render_state().layer_commands[m_core->render_state().current_layer].commands.push_back(cmd);
    if (!m_core->render_state().scissor_stack.empty()) {
        m_core->render_state().scissor_stack.pop_back();
    }
}

Vec2 WidgetAPIImpl::mouse_position() const {
    return m_core->interaction_state().mouse_position;
}

Vec2 WidgetAPIImpl::mouse_delta() const {
    return m_core->interaction_state().mouse_delta;
}

bool WidgetAPIImpl::is_mouse_down(int button) const {
    return m_core->interaction_state().is_mouse_down(button);
}

bool WidgetAPIImpl::is_mouse_pressed(int button) const {
    return m_core->interaction_state().is_mouse_pressed(button);
}

bool WidgetAPIImpl::is_mouse_released(int button) const {
    return m_core->interaction_state().is_mouse_released(button);
}

bool WidgetAPIImpl::is_key_down(int key) const {
    return m_core->interaction_state().is_key_down(key);
}

bool WidgetAPIImpl::is_key_pressed(int key) const {
    return m_core->interaction_state().is_key_pressed(key);
}

const std::string& WidgetAPIImpl::text_input() const {
    return m_core->interaction_state().text_input_buffer;
}

void WidgetAPIImpl::set_focus(WidgetId id) {
    auto cmd = std::make_unique<SetFocusCommand>(id);
    cmd->execute(*m_core);
}

void WidgetAPIImpl::clear_focus() {
    auto cmd = std::make_unique<ClearFocusCommand>();
    cmd->execute(*m_core);
}

void WidgetAPIImpl::focus_next() {
    // TODO: Implement focus chain navigation
}

void WidgetAPIImpl::focus_prev() {
    // TODO: Implement focus chain navigation
}

void WidgetAPIImpl::subscribe(WidgetId id, WidgetEventType event, WidgetEventCallback callback) {
    m_subscriptions.push_back({id, event, std::move(callback)});
}

void WidgetAPIImpl::unsubscribe(WidgetId id, WidgetEventType event) {
    m_subscriptions.erase(
        std::remove_if(m_subscriptions.begin(), m_subscriptions.end(),
                       [id, event](const Subscription& s) {
                           return s.widget == id && s.event == event;
                       }),
        m_subscriptions.end());
}

LayerId WidgetAPIImpl::create_layer(std::string_view name, int z_order) {
    return m_core->create_layer(name, z_order);
}

void WidgetAPIImpl::destroy_layer(LayerId id) {
    m_core->destroy_layer(id);
}

void WidgetAPIImpl::set_layer_visible(LayerId id, bool visible) {
    for (auto& layer : m_core->widget_registry().layers) {
        if (layer.id == id) {
            layer.visible = visible;
            break;
        }
    }
}

void WidgetAPIImpl::set_layer_opacity(LayerId id, float opacity) {
    for (auto& layer : m_core->widget_registry().layers) {
        if (layer.id == id) {
            layer.opacity = opacity;
            break;
        }
    }
}

float WidgetAPIImpl::delta_time() const {
    return m_core->delta_time();
}

double WidgetAPIImpl::current_time() const {
    return m_core->current_time();
}

Vec2 WidgetAPIImpl::screen_size() const {
    return m_core->screen_size();
}

float WidgetAPIImpl::ui_scale() const {
    return m_core->layout_state().ui_scale;
}

const void_gamestate::GameStateCore* WidgetAPIImpl::game_state() const {
    return m_game_state;
}

} // namespace void_widget
