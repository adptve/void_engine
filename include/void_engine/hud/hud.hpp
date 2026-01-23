/// @file hud.hpp
/// @brief Main HUD system for void_hud module

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "elements.hpp"
#include "binding.hpp"
#include "animation.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_hud {

// =============================================================================
// HudLayer
// =============================================================================

/// @brief Layer containing HUD elements at a specific z-depth
class HudLayer {
public:
    HudLayer();
    explicit HudLayer(const std::string& name, std::int32_t z_order = 0);
    ~HudLayer();

    // Identity
    HudLayerId id() const { return m_id; }
    const std::string& name() const { return m_name; }

    // Z-order
    void set_z_order(std::int32_t z) { m_z_order = z; }
    std::int32_t z_order() const { return m_z_order; }

    // Elements
    void add_element(IHudElement* element);
    void remove_element(IHudElement* element);
    void remove_element(HudElementId id);
    IHudElement* find_element(HudElementId id) const;
    IHudElement* find_element(std::string_view name) const;
    const std::vector<IHudElement*>& elements() const { return m_elements; }

    // Visibility
    void set_visible(bool visible) { m_visible = visible; }
    bool is_visible() const { return m_visible; }

    void set_opacity(float opacity) { m_opacity = opacity; }
    float opacity() const { return m_opacity; }

    // Update/Render
    void update(float delta_time);
    void render();

    // Sorting
    void sort_elements();

    // Clear
    void clear();

    // Internal
    void set_id(HudLayerId id) { m_id = id; }

private:
    HudLayerId m_id;
    std::string m_name;
    std::int32_t m_z_order{0};
    std::vector<IHudElement*> m_elements;
    bool m_visible{true};
    float m_opacity{1.0f};
    bool m_needs_sort{false};
};

// =============================================================================
// HudManager
// =============================================================================

/// @brief Manages HUD elements, layers, and rendering
class HudManager {
public:
    HudManager();
    explicit HudManager(const HudConfig& config);
    ~HudManager();

    // Configuration
    const HudConfig& config() const { return m_config; }
    void set_config(const HudConfig& config);

    // Screen
    void set_screen_size(float width, float height);
    Vec2 screen_size() const { return m_screen_size; }
    float scale_factor() const { return m_scale_factor; }

    // Layer management
    HudLayerId create_layer(const std::string& name, std::int32_t z_order = 0);
    void remove_layer(HudLayerId id);
    HudLayer* get_layer(HudLayerId id);
    HudLayer* get_layer(std::string_view name);
    void set_layer_visible(HudLayerId id, bool visible);
    void sort_layers();

    // Element creation
    template<typename T, typename... Args>
    T* create_element(Args&&... args);

    template<typename T, typename... Args>
    T* create_element_in_layer(HudLayerId layer, Args&&... args);

    // Element management
    void add_element(IHudElement* element, HudLayerId layer = HudLayerId{});
    void remove_element(HudElementId id);
    void remove_element(IHudElement* element);
    IHudElement* find_element(HudElementId id) const;
    IHudElement* find_element(std::string_view name) const;

    // Parenting
    void set_parent(IHudElement* child, IHudElement* parent);
    void unparent(IHudElement* element);

    // Visibility
    void show(IHudElement* element);
    void hide(IHudElement* element);
    void set_visible(IHudElement* element, bool visible);

    // Update/Render
    void update(float delta_time);
    void render();

    // Input handling
    bool handle_pointer_move(const Vec2& position);
    bool handle_pointer_down(const Vec2& position);
    bool handle_pointer_up(const Vec2& position);

    // Hit testing
    IHudElement* hit_test(const Vec2& position) const;

    // Notifications
    void show_notification(const NotificationDef& def);
    void dismiss_notification(HudElementId id);
    void clear_notifications();

    // Tooltips
    void show_tooltip(const Vec2& position, const std::string& text);
    void show_tooltip(IHudElement* element, const std::string& text);
    void hide_tooltip();

    // Animation
    HudAnimator& animator() { return m_animator; }
    const HudAnimator& animator() const { return m_animator; }

    // Binding
    DataBindingManager& bindings() { return m_bindings; }
    const DataBindingManager& bindings() const { return m_bindings; }

    // World-space markers
    void update_world_markers(const std::function<Vec2(const Vec3&, bool&)>& project_func);

    // Clear
    void clear();

    // Debug
    void set_debug_draw(bool enabled) { m_debug_draw = enabled; }
    bool debug_draw() const { return m_debug_draw; }

private:
    void update_scale();
    void cleanup_expired_notifications();
    HudLayer* get_default_layer();

    HudConfig m_config;
    Vec2 m_screen_size{1920, 1080};
    float m_scale_factor{1.0f};

    std::vector<std::unique_ptr<HudLayer>> m_layers;
    std::unordered_map<HudElementId, std::unique_ptr<IHudElement>> m_elements;
    std::uint64_t m_next_layer_id{1};
    std::uint64_t m_next_element_id{1};

    HudLayerId m_default_layer;
    HudLayerId m_notification_layer;
    HudLayerId m_tooltip_layer;

    HudAnimator m_animator;
    DataBindingManager m_bindings;

    std::vector<HudNotification*> m_active_notifications;
    HudTooltip* m_active_tooltip{nullptr};

    IHudElement* m_hovered_element{nullptr};
    IHudElement* m_pressed_element{nullptr};

    bool m_debug_draw{false};
};

// =============================================================================
// HudSystem
// =============================================================================

/// @brief Main HUD system interface
class HudSystem {
public:
    HudSystem();
    explicit HudSystem(const HudConfig& config);
    ~HudSystem();

    // Initialization
    void initialize();
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Manager access
    HudManager& manager() { return m_manager; }
    const HudManager& manager() const { return m_manager; }

    HudAnimator& animator() { return m_manager.animator(); }
    DataBindingManager& bindings() { return m_manager.bindings(); }

    // Convenience methods
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        return m_manager.create_element<T>(std::forward<Args>(args)...);
    }

    void add(IHudElement* element) { m_manager.add_element(element); }
    void remove(IHudElement* element) { m_manager.remove_element(element); }
    IHudElement* find(std::string_view name) { return m_manager.find_element(name); }

    // Screen
    void set_screen_size(float width, float height) { m_manager.set_screen_size(width, height); }

    // Update/Render
    void update(float delta_time);
    void render();

    // Input
    bool on_pointer_move(float x, float y);
    bool on_pointer_down(float x, float y);
    bool on_pointer_up(float x, float y);

    // Notifications
    void notify(const std::string& title, const std::string& message,
                NotificationType type = NotificationType::Info);

    // Presets
    HudProgressBar* create_health_bar(float x, float y, float width, float height);
    HudProgressBar* create_ammo_bar(float x, float y, float width, float height);
    HudText* create_score_text(float x, float y);
    HudMinimap* create_minimap(float x, float y, float size);
    HudCrosshair* create_crosshair();
    HudCompass* create_compass(float y);

private:
    HudConfig m_config;
    HudManager m_manager;
    bool m_initialized{false};
};

// =============================================================================
// Template Implementations
// =============================================================================

template<typename T, typename... Args>
T* HudManager::create_element(Args&&... args) {
    return create_element_in_layer<T>(m_default_layer, std::forward<Args>(args)...);
}

template<typename T, typename... Args>
T* HudManager::create_element_in_layer(HudLayerId layer, Args&&... args) {
    auto element = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = element.get();

    HudElementId id{m_next_element_id++};
    ptr->set_id(id);

    m_elements[id] = std::move(element);

    if (auto* l = get_layer(layer)) {
        l->add_element(ptr);
    } else if (auto* def = get_default_layer()) {
        def->add_element(ptr);
    }

    return ptr;
}

// =============================================================================
// ElementBuilder
// =============================================================================

/// @brief Fluent builder for creating HUD elements
template<typename T>
class ElementBuilder {
public:
    explicit ElementBuilder(HudManager* manager)
        : m_manager(manager), m_element(std::make_unique<T>()) {}

    ElementBuilder& name(const std::string& name) {
        m_element->properties_mut().name = name;
        return *this;
    }

    ElementBuilder& position(float x, float y) {
        m_element->set_position({x, y});
        return *this;
    }

    ElementBuilder& size(float w, float h) {
        m_element->set_size({w, h});
        return *this;
    }

    ElementBuilder& anchor(AnchorPoint point) {
        m_element->properties_mut().anchor = point;
        return *this;
    }

    ElementBuilder& color(const Color& c) {
        m_element->properties_mut().color = c;
        return *this;
    }

    ElementBuilder& opacity(float o) {
        m_element->set_opacity(o);
        return *this;
    }

    ElementBuilder& z_order(std::int32_t z) {
        m_element->properties_mut().z_order = z;
        return *this;
    }

    ElementBuilder& visible(bool v) {
        m_element->set_visible(v);
        return *this;
    }

    ElementBuilder& interactive(bool i) {
        m_element->properties_mut().interactive = i;
        return *this;
    }

    ElementBuilder& in_layer(HudLayerId layer) {
        m_layer = layer;
        return *this;
    }

    T* build() {
        T* ptr = m_element.get();
        HudElementId id{1}; // Manager will assign proper ID
        m_manager->add_element(m_element.release(), m_layer);
        return ptr;
    }

private:
    HudManager* m_manager;
    std::unique_ptr<T> m_element;
    HudLayerId m_layer;
};

// =============================================================================
// Prelude - Convenience namespace
// =============================================================================

namespace prelude {

using void_hud::HudElementType;
using void_hud::AnchorPoint;
using void_hud::PositionMode;
using void_hud::SizeMode;
using void_hud::Visibility;
using void_hud::AnimProperty;
using void_hud::EasingType;
using void_hud::AnimationState;
using void_hud::PlayMode;
using void_hud::BindingMode;
using void_hud::ConverterType;
using void_hud::ProgressStyle;
using void_hud::FillDirection;
using void_hud::NotificationType;
using void_hud::NotificationPosition;

using void_hud::HudElementId;
using void_hud::HudLayerId;
using void_hud::BindingId;
using void_hud::HudAnimationId;

using void_hud::Vec2;
using void_hud::Vec3;
using void_hud::Rect;
using void_hud::Color;
using void_hud::Insets;
using void_hud::ElementProperties;
using void_hud::TextProperties;
using void_hud::ProgressBarProperties;
using void_hud::IconProperties;
using void_hud::Keyframe;
using void_hud::AnimationDef;
using void_hud::TransitionDef;
using void_hud::NotificationDef;
using void_hud::DamageIndicatorDef;
using void_hud::ObjectiveMarkerDef;
using void_hud::HudConfig;

using void_hud::IHudElement;
using void_hud::HudElementBase;
using void_hud::HudPanel;
using void_hud::HudText;
using void_hud::HudProgressBar;
using void_hud::HudIcon;
using void_hud::HudMinimap;
using void_hud::HudCrosshair;
using void_hud::HudCompass;
using void_hud::HudObjectiveMarker;
using void_hud::HudDamageIndicator;
using void_hud::HudNotification;
using void_hud::HudTooltip;

using void_hud::IDataSource;
using void_hud::SimpleDataSource;
using void_hud::IValueConverter;
using void_hud::PropertyBinding;
using void_hud::BindingContext;
using void_hud::DataBindingManager;
using void_hud::BindingBuilder;

using void_hud::Easing;
using void_hud::IHudAnimation;
using void_hud::PropertyAnimation;
using void_hud::HudAnimationSequence;
using void_hud::HudAnimationGroup;
using void_hud::HudTransition;
using void_hud::HudAnimator;
using void_hud::AnimationBuilder;

using void_hud::HudLayer;
using void_hud::HudManager;
using void_hud::HudSystem;

namespace presets = void_hud::presets;

} // namespace prelude

} // namespace void_hud
