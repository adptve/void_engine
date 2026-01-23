/// @file hud.cpp
/// @brief Implementation of main HUD system for void_hud module

#include "void_engine/hud/hud.hpp"

#include <algorithm>

namespace void_hud {

// =============================================================================
// HudLayer
// =============================================================================

HudLayer::HudLayer() = default;

HudLayer::HudLayer(const std::string& name, std::int32_t z_order)
    : m_name(name), m_z_order(z_order) {}

HudLayer::~HudLayer() = default;

void HudLayer::add_element(IHudElement* element) {
    if (!element) return;
    m_elements.push_back(element);
    m_needs_sort = true;
}

void HudLayer::remove_element(IHudElement* element) {
    m_elements.erase(
        std::remove(m_elements.begin(), m_elements.end(), element),
        m_elements.end());
}

void HudLayer::remove_element(HudElementId id) {
    m_elements.erase(
        std::remove_if(m_elements.begin(), m_elements.end(),
                       [id](IHudElement* e) { return e && e->id() == id; }),
        m_elements.end());
}

IHudElement* HudLayer::find_element(HudElementId id) const {
    for (auto* element : m_elements) {
        if (element && element->id() == id) {
            return element;
        }
    }
    return nullptr;
}

IHudElement* HudLayer::find_element(std::string_view name) const {
    for (auto* element : m_elements) {
        if (element && element->name() == name) {
            return element;
        }
    }
    return nullptr;
}

void HudLayer::update(float delta_time) {
    if (!m_visible) return;

    for (auto* element : m_elements) {
        if (element && element->is_visible()) {
            element->update(delta_time);
        }
    }
}

void HudLayer::render() {
    if (!m_visible) return;

    if (m_needs_sort) {
        sort_elements();
    }

    for (auto* element : m_elements) {
        if (element && element->is_visible()) {
            element->render();
        }
    }
}

void HudLayer::sort_elements() {
    std::stable_sort(m_elements.begin(), m_elements.end(),
                     [](IHudElement* a, IHudElement* b) {
                         return a->properties().z_order < b->properties().z_order;
                     });
    m_needs_sort = false;
}

void HudLayer::clear() {
    m_elements.clear();
}

// =============================================================================
// HudManager
// =============================================================================

HudManager::HudManager() {
    m_default_layer = create_layer("Default", 0);
    m_notification_layer = create_layer("Notifications", 100);
    m_tooltip_layer = create_layer("Tooltips", 200);
}

HudManager::HudManager(const HudConfig& config)
    : m_config(config) {
    m_screen_size = {config.reference_width, config.reference_height};
    m_default_layer = create_layer("Default", 0);
    m_notification_layer = create_layer("Notifications", 100);
    m_tooltip_layer = create_layer("Tooltips", 200);
}

HudManager::~HudManager() = default;

void HudManager::set_config(const HudConfig& config) {
    m_config = config;
    update_scale();
}

void HudManager::set_screen_size(float width, float height) {
    m_screen_size = {width, height};
    update_scale();
}

void HudManager::update_scale() {
    if (m_config.scale_with_screen) {
        float scale_x = m_screen_size.x / m_config.reference_width;
        float scale_y = m_screen_size.y / m_config.reference_height;
        m_scale_factor = std::min(scale_x, scale_y);
        m_scale_factor = std::clamp(m_scale_factor, m_config.min_scale, m_config.max_scale);
    } else {
        m_scale_factor = 1.0f;
    }
}

HudLayerId HudManager::create_layer(const std::string& name, std::int32_t z_order) {
    HudLayerId id{m_next_layer_id++};
    auto layer = std::make_unique<HudLayer>(name, z_order);
    layer->set_id(id);
    m_layers.push_back(std::move(layer));
    sort_layers();
    return id;
}

void HudManager::remove_layer(HudLayerId id) {
    // Don't remove default layers
    if (id == m_default_layer || id == m_notification_layer || id == m_tooltip_layer) {
        return;
    }

    m_layers.erase(
        std::remove_if(m_layers.begin(), m_layers.end(),
                       [id](const auto& l) { return l->id() == id; }),
        m_layers.end());
}

HudLayer* HudManager::get_layer(HudLayerId id) {
    for (auto& layer : m_layers) {
        if (layer->id() == id) {
            return layer.get();
        }
    }
    return nullptr;
}

HudLayer* HudManager::get_layer(std::string_view name) {
    for (auto& layer : m_layers) {
        if (layer->name() == name) {
            return layer.get();
        }
    }
    return nullptr;
}

void HudManager::set_layer_visible(HudLayerId id, bool visible) {
    if (auto* layer = get_layer(id)) {
        layer->set_visible(visible);
    }
}

void HudManager::sort_layers() {
    std::stable_sort(m_layers.begin(), m_layers.end(),
                     [](const auto& a, const auto& b) {
                         return a->z_order() < b->z_order();
                     });
}

void HudManager::add_element(IHudElement* element, HudLayerId layer) {
    if (!element) return;

    HudLayerId target = layer ? layer : m_default_layer;
    if (auto* l = get_layer(target)) {
        l->add_element(element);
    }
}

void HudManager::remove_element(HudElementId id) {
    for (auto& layer : m_layers) {
        layer->remove_element(id);
    }
    m_elements.erase(id);
}

void HudManager::remove_element(IHudElement* element) {
    if (!element) return;
    remove_element(element->id());
}

IHudElement* HudManager::find_element(HudElementId id) const {
    auto it = m_elements.find(id);
    return it != m_elements.end() ? it->second.get() : nullptr;
}

IHudElement* HudManager::find_element(std::string_view name) const {
    for (const auto& [_, element] : m_elements) {
        if (element && element->name() == name) {
            return element.get();
        }
        // Check children recursively
        if (auto* found = element->find_child_recursive(name)) {
            return found;
        }
    }
    return nullptr;
}

void HudManager::set_parent(IHudElement* child, IHudElement* parent) {
    if (!child) return;

    if (parent) {
        parent->add_child(child);
    }
}

void HudManager::unparent(IHudElement* element) {
    if (!element || !element->parent()) return;

    element->parent()->remove_child(element);
}

void HudManager::show(IHudElement* element) {
    if (element) {
        element->set_visible(true);
    }
}

void HudManager::hide(IHudElement* element) {
    if (element) {
        element->set_visible(false);
    }
}

void HudManager::set_visible(IHudElement* element, bool visible) {
    if (element) {
        element->set_visible(visible);
    }
}

void HudManager::update(float delta_time) {
    // Update layers
    for (auto& layer : m_layers) {
        layer->update(delta_time);
    }

    // Update animations
    m_animator.update(delta_time);

    // Update bindings
    m_bindings.update();

    // Cleanup expired notifications
    cleanup_expired_notifications();
}

void HudManager::render() {
    for (auto& layer : m_layers) {
        layer->render();
    }
}

bool HudManager::handle_pointer_move(const Vec2& position) {
    auto* element = hit_test(position);

    if (element != m_hovered_element) {
        if (m_hovered_element) {
            m_hovered_element->on_pointer_exit();
        }
        m_hovered_element = element;
        if (m_hovered_element) {
            m_hovered_element->on_pointer_enter();
        }
    }

    return element != nullptr;
}

bool HudManager::handle_pointer_down(const Vec2& position) {
    auto* element = hit_test(position);

    if (element) {
        m_pressed_element = element;
        element->on_pointer_down(position);
        return true;
    }

    return false;
}

bool HudManager::handle_pointer_up(const Vec2& position) {
    if (m_pressed_element) {
        m_pressed_element->on_pointer_up(position);

        // Check for click
        if (m_pressed_element->hit_test(position)) {
            m_pressed_element->on_click();
        }

        m_pressed_element = nullptr;
        return true;
    }

    return false;
}

IHudElement* HudManager::hit_test(const Vec2& position) const {
    // Test in reverse order (top layers first)
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
        if (!(*it)->is_visible()) continue;

        // Test elements in reverse z-order
        const auto& elements = (*it)->elements();
        for (auto eit = elements.rbegin(); eit != elements.rend(); ++eit) {
            if (*eit && (*eit)->hit_test(position)) {
                return *eit;
            }
        }
    }
    return nullptr;
}

void HudManager::show_notification(const NotificationDef& def) {
    auto* notification = create_element_in_layer<HudNotification>(m_notification_layer, def);

    // Position based on config
    float x = 0, y = 0;
    switch (m_config.notification_position) {
        case NotificationPosition::TopLeft:
            x = 10;
            y = 10 + m_active_notifications.size() * (50 + m_config.notification_spacing);
            break;
        case NotificationPosition::TopCenter:
            x = m_screen_size.x / 2 - 150;
            y = 10 + m_active_notifications.size() * (50 + m_config.notification_spacing);
            break;
        case NotificationPosition::TopRight:
            x = m_screen_size.x - 310;
            y = 10 + m_active_notifications.size() * (50 + m_config.notification_spacing);
            break;
        case NotificationPosition::BottomLeft:
            x = 10;
            y = m_screen_size.y - 60 - m_active_notifications.size() * (50 + m_config.notification_spacing);
            break;
        case NotificationPosition::BottomCenter:
            x = m_screen_size.x / 2 - 150;
            y = m_screen_size.y - 60 - m_active_notifications.size() * (50 + m_config.notification_spacing);
            break;
        case NotificationPosition::BottomRight:
            x = m_screen_size.x - 310;
            y = m_screen_size.y - 60 - m_active_notifications.size() * (50 + m_config.notification_spacing);
            break;
        case NotificationPosition::Center:
            x = m_screen_size.x / 2 - 150;
            y = m_screen_size.y / 2 - 25;
            break;
    }

    notification->set_position({x, y});
    notification->set_size({300, 50});

    // Animate in
    m_animator.fade_in(notification);

    m_active_notifications.push_back(notification);

    // Remove oldest if at max
    while (m_active_notifications.size() > m_config.max_notifications) {
        dismiss_notification(m_active_notifications.front()->id());
    }
}

void HudManager::dismiss_notification(HudElementId id) {
    for (auto it = m_active_notifications.begin(); it != m_active_notifications.end(); ++it) {
        if ((*it)->id() == id) {
            auto* notification = *it;
            m_animator.fade_out(notification, 0.2f);
            // Would need delayed removal
            m_active_notifications.erase(it);
            break;
        }
    }
}

void HudManager::clear_notifications() {
    for (auto* notification : m_active_notifications) {
        remove_element(notification);
    }
    m_active_notifications.clear();
}

void HudManager::cleanup_expired_notifications() {
    for (auto it = m_active_notifications.begin(); it != m_active_notifications.end();) {
        if ((*it)->is_expired()) {
            auto* notification = *it;
            m_animator.fade_out(notification, 0.2f);
            it = m_active_notifications.erase(it);
        } else {
            ++it;
        }
    }
}

void HudManager::show_tooltip(const Vec2& position, const std::string& text) {
    if (!m_active_tooltip) {
        m_active_tooltip = create_element_in_layer<HudTooltip>(m_tooltip_layer);
    }

    m_active_tooltip->set_title("");
    m_active_tooltip->set_description(text);
    m_active_tooltip->clear_stats();
    m_active_tooltip->show_at(position + m_config.tooltip_offset);
}

void HudManager::show_tooltip(IHudElement* element, const std::string& text) {
    if (!element) return;

    Rect bounds = element->global_bounds();
    Vec2 position{bounds.x + bounds.width, bounds.y};
    show_tooltip(position, text);
}

void HudManager::hide_tooltip() {
    if (m_active_tooltip) {
        m_active_tooltip->hide();
    }
}

void HudManager::update_world_markers(const std::function<Vec2(const Vec3&, bool&)>& project_func) {
    for (auto& [_, element] : m_elements) {
        if (auto* marker = dynamic_cast<HudObjectiveMarker*>(element.get())) {
            bool on_screen = true;
            Vec2 screen_pos = project_func(marker->world_position(), on_screen);
            marker->update_screen_position(screen_pos, on_screen);
        }
    }
}

void HudManager::clear() {
    for (auto& layer : m_layers) {
        layer->clear();
    }
    m_elements.clear();
    m_active_notifications.clear();
    m_active_tooltip = nullptr;
    m_hovered_element = nullptr;
    m_pressed_element = nullptr;
}

HudLayer* HudManager::get_default_layer() {
    return get_layer(m_default_layer);
}

// =============================================================================
// HudSystem
// =============================================================================

HudSystem::HudSystem()
    : m_manager(m_config) {}

HudSystem::HudSystem(const HudConfig& config)
    : m_config(config), m_manager(config) {}

HudSystem::~HudSystem() {
    shutdown();
}

void HudSystem::initialize() {
    if (m_initialized) return;
    m_initialized = true;
}

void HudSystem::shutdown() {
    if (!m_initialized) return;
    m_manager.clear();
    m_initialized = false;
}

void HudSystem::update(float delta_time) {
    if (!m_initialized) return;
    m_manager.update(delta_time);
}

void HudSystem::render() {
    if (!m_initialized) return;
    m_manager.render();
}

bool HudSystem::on_pointer_move(float x, float y) {
    return m_manager.handle_pointer_move({x, y});
}

bool HudSystem::on_pointer_down(float x, float y) {
    return m_manager.handle_pointer_down({x, y});
}

bool HudSystem::on_pointer_up(float x, float y) {
    return m_manager.handle_pointer_up({x, y});
}

void HudSystem::notify(const std::string& title, const std::string& message, NotificationType type) {
    NotificationDef def{
        .title = title,
        .message = message,
        .type = type,
        .duration = 3.0f
    };
    m_manager.show_notification(def);
}

HudProgressBar* HudSystem::create_health_bar(float x, float y, float width, float height) {
    ProgressBarProperties props;
    props.value = 1.0f;
    props.fill_color = Color::green();
    props.background_color = Color(0.2f, 0.2f, 0.2f, 0.8f);
    props.border_color = Color::white();
    props.border_width = 1.0f;
    props.show_text = false;

    auto* bar = m_manager.create_element<HudProgressBar>(props);
    bar->set_position({x, y});
    bar->set_size({width, height});
    bar->properties_mut().name = "HealthBar";
    return bar;
}

HudProgressBar* HudSystem::create_ammo_bar(float x, float y, float width, float height) {
    ProgressBarProperties props;
    props.value = 1.0f;
    props.style = ProgressStyle::Segmented;
    props.segments = 30;
    props.segment_gap = 1.0f;
    props.fill_color = Color::yellow();
    props.background_color = Color(0.2f, 0.2f, 0.2f, 0.5f);
    props.show_text = false;

    auto* bar = m_manager.create_element<HudProgressBar>(props);
    bar->set_position({x, y});
    bar->set_size({width, height});
    bar->properties_mut().name = "AmmoBar";
    return bar;
}

HudText* HudSystem::create_score_text(float x, float y) {
    TextProperties props;
    props.font_size = 24.0f;
    props.text_color = Color::white();
    props.has_shadow = true;

    auto* text = m_manager.create_element<HudText>("0", props);
    text->set_position({x, y});
    text->properties_mut().name = "ScoreText";
    text->set_format("Score: {}");
    return text;
}

HudMinimap* HudSystem::create_minimap(float x, float y, float size) {
    auto* minimap = m_manager.create_element<HudMinimap>(size);
    minimap->set_position({x, y});
    minimap->properties_mut().name = "Minimap";
    minimap->set_circular(true);
    return minimap;
}

HudCrosshair* HudSystem::create_crosshair() {
    auto* crosshair = m_manager.create_element<HudCrosshair>();
    crosshair->set_position({m_manager.screen_size().x / 2, m_manager.screen_size().y / 2});
    crosshair->properties_mut().name = "Crosshair";
    crosshair->properties_mut().anchor = AnchorPoint::MiddleCenter;
    return crosshair;
}

HudCompass* HudSystem::create_compass(float y) {
    auto* compass = m_manager.create_element<HudCompass>();
    compass->set_position({m_manager.screen_size().x / 2, y});
    compass->set_size({400, 40});
    compass->properties_mut().name = "Compass";
    compass->properties_mut().anchor = AnchorPoint::TopCenter;
    return compass;
}

} // namespace void_hud
