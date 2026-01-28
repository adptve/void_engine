/// @file commands.hpp
/// @brief Widget state modification commands
///
/// All widget state modifications go through commands.
/// This enables validation, undo/redo, and network replication.

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "widget_api.hpp"

#include <memory>
#include <string>

namespace void_widget {

// Forward declaration
class WidgetStateCore;

// =============================================================================
// Widget Lifecycle Commands
// =============================================================================

/// @brief Command to create a widget
class CreateWidgetCommand : public IWidgetCommand {
public:
    CreateWidgetCommand(std::string type, std::string name = "")
        : m_type(std::move(type)), m_name(std::move(name)) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "CreateWidget"; }

    WidgetId created_id() const { return m_created_id; }

private:
    std::string m_type;
    std::string m_name;
    WidgetId m_created_id{0};
};

/// @brief Command to destroy a widget
class DestroyWidgetCommand : public IWidgetCommand {
public:
    explicit DestroyWidgetCommand(WidgetId id) : m_id(id) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "DestroyWidget"; }

private:
    WidgetId m_id;
};

/// @brief Command to set widget parent
class SetParentCommand : public IWidgetCommand {
public:
    SetParentCommand(WidgetId child, WidgetId parent)
        : m_child(child), m_parent(parent) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetParent"; }

private:
    WidgetId m_child;
    WidgetId m_parent;
};

/// @brief Command to set widget layer
class SetLayerCommand : public IWidgetCommand {
public:
    SetLayerCommand(WidgetId widget, LayerId layer)
        : m_widget(widget), m_layer(layer) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetLayer"; }

private:
    WidgetId m_widget;
    LayerId m_layer;
};

// =============================================================================
// Layout Commands
// =============================================================================

/// @brief Command to set widget position
class SetPositionCommand : public IWidgetCommand {
public:
    SetPositionCommand(WidgetId id, Vec2 position)
        : m_id(id), m_position(position) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetPosition"; }

private:
    WidgetId m_id;
    Vec2 m_position;
};

/// @brief Command to set widget size
class SetSizeCommand : public IWidgetCommand {
public:
    SetSizeCommand(WidgetId id, Vec2 size)
        : m_id(id), m_size(size) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetSize"; }

private:
    WidgetId m_id;
    Vec2 m_size;
};

/// @brief Command to set widget anchor
class SetAnchorCommand : public IWidgetCommand {
public:
    SetAnchorCommand(WidgetId id, Vec2 anchor)
        : m_id(id), m_anchor(anchor) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetAnchor"; }

private:
    WidgetId m_id;
    Vec2 m_anchor;
};

/// @brief Command to set widget pivot
class SetPivotCommand : public IWidgetCommand {
public:
    SetPivotCommand(WidgetId id, Vec2 pivot)
        : m_id(id), m_pivot(pivot) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetPivot"; }

private:
    WidgetId m_id;
    Vec2 m_pivot;
};

/// @brief Command to set widget margin
class SetMarginCommand : public IWidgetCommand {
public:
    SetMarginCommand(WidgetId id, Insets margin)
        : m_id(id), m_margin(margin) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetMargin"; }

private:
    WidgetId m_id;
    Insets m_margin;
};

/// @brief Command to set widget padding
class SetPaddingCommand : public IWidgetCommand {
public:
    SetPaddingCommand(WidgetId id, Insets padding)
        : m_id(id), m_padding(padding) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetPadding"; }

private:
    WidgetId m_id;
    Insets m_padding;
};

/// @brief Command to set widget visibility
class SetVisibleCommand : public IWidgetCommand {
public:
    SetVisibleCommand(WidgetId id, bool visible)
        : m_id(id), m_visible(visible) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetVisible"; }

private:
    WidgetId m_id;
    bool m_visible;
};

/// @brief Command to set widget rotation
class SetRotationCommand : public IWidgetCommand {
public:
    SetRotationCommand(WidgetId id, float degrees)
        : m_id(id), m_degrees(degrees) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetRotation"; }

private:
    WidgetId m_id;
    float m_degrees;
};

/// @brief Command to set widget scale
class SetScaleCommand : public IWidgetCommand {
public:
    SetScaleCommand(WidgetId id, Vec2 scale)
        : m_id(id), m_scale(scale) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetScale"; }

private:
    WidgetId m_id;
    Vec2 m_scale;
};

// =============================================================================
// Style Commands
// =============================================================================

/// @brief Command to set style property
class SetStylePropertyCommand : public IWidgetCommand {
public:
    SetStylePropertyCommand(WidgetId id, std::string property, std::any value)
        : m_id(id), m_property(std::move(property)), m_value(std::move(value)) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetStyleProperty"; }

private:
    WidgetId m_id;
    std::string m_property;
    std::any m_value;
};

/// @brief Command to set opacity
class SetOpacityCommand : public IWidgetCommand {
public:
    SetOpacityCommand(WidgetId id, float opacity)
        : m_id(id), m_opacity(opacity) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetOpacity"; }

private:
    WidgetId m_id;
    float m_opacity;
};

/// @brief Command to apply theme
class ApplyThemeCommand : public IWidgetCommand {
public:
    explicit ApplyThemeCommand(std::string theme_name)
        : m_theme_name(std::move(theme_name)) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "ApplyTheme"; }

private:
    std::string m_theme_name;
};

// =============================================================================
// Animation Commands
// =============================================================================

/// @brief Command to play an animation
class PlayAnimationCommand : public IWidgetCommand {
public:
    PlayAnimationCommand(WidgetId id, std::string anim_name)
        : m_id(id), m_anim_name(std::move(anim_name)) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "PlayAnimation"; }

    AnimationId animation_id() const { return m_anim_id; }

private:
    WidgetId m_id;
    std::string m_anim_name;
    AnimationId m_anim_id{0};
};

/// @brief Command to stop an animation
class StopAnimationCommand : public IWidgetCommand {
public:
    StopAnimationCommand(WidgetId id, AnimationId anim = AnimationId{0})
        : m_id(id), m_anim(anim) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "StopAnimation"; }

private:
    WidgetId m_id;
    AnimationId m_anim;
};

/// @brief Command to animate a property
class AnimatePropertyCommand : public IWidgetCommand {
public:
    AnimatePropertyCommand(WidgetId id, std::string property, std::any target,
                           float duration, EasingType easing)
        : m_id(id), m_property(std::move(property)), m_target(std::move(target)),
          m_duration(duration), m_easing(easing) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "AnimateProperty"; }

    AnimationId animation_id() const { return m_anim_id; }

private:
    WidgetId m_id;
    std::string m_property;
    std::any m_target;
    float m_duration;
    EasingType m_easing;
    AnimationId m_anim_id{0};
};

// =============================================================================
// Binding Commands
// =============================================================================

/// @brief Command to create a data binding
class BindPropertyCommand : public IWidgetCommand {
public:
    BindPropertyCommand(WidgetId id, std::string property, std::string source_path,
                        BindingMode mode = BindingMode::OneWay)
        : m_id(id), m_property(std::move(property)),
          m_source_path(std::move(source_path)), m_mode(mode) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "BindProperty"; }

    BindingId binding_id() const { return m_binding_id; }

private:
    WidgetId m_id;
    std::string m_property;
    std::string m_source_path;
    BindingMode m_mode;
    BindingId m_binding_id{0};
};

/// @brief Command to remove a binding
class UnbindPropertyCommand : public IWidgetCommand {
public:
    explicit UnbindPropertyCommand(BindingId binding)
        : m_binding(binding) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "UnbindProperty"; }

private:
    BindingId m_binding;
};

// =============================================================================
// Widget Property Commands
// =============================================================================

/// @brief Command to set widget text
class SetTextCommand : public IWidgetCommand {
public:
    SetTextCommand(WidgetId id, std::string text)
        : m_id(id), m_text(std::move(text)) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetText"; }

private:
    WidgetId m_id;
    std::string m_text;
};

/// @brief Command to set widget value
class SetValueCommand : public IWidgetCommand {
public:
    SetValueCommand(WidgetId id, float value)
        : m_id(id), m_value(value) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetValue"; }

private:
    WidgetId m_id;
    float m_value;
};

/// @brief Command to set widget checked state
class SetCheckedCommand : public IWidgetCommand {
public:
    SetCheckedCommand(WidgetId id, bool checked)
        : m_id(id), m_checked(checked) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetChecked"; }

private:
    WidgetId m_id;
    bool m_checked;
};

/// @brief Command to set widget enabled state
class SetEnabledCommand : public IWidgetCommand {
public:
    SetEnabledCommand(WidgetId id, bool enabled)
        : m_id(id), m_enabled(enabled) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetEnabled"; }

private:
    WidgetId m_id;
    bool m_enabled;
};

/// @brief Command to set custom property
class SetPropertyCommand : public IWidgetCommand {
public:
    SetPropertyCommand(WidgetId id, std::string key, std::any value)
        : m_id(id), m_key(std::move(key)), m_value(std::move(value)) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetProperty"; }

private:
    WidgetId m_id;
    std::string m_key;
    std::any m_value;
};

// =============================================================================
// Focus Commands
// =============================================================================

/// @brief Command to set focused widget
class SetFocusCommand : public IWidgetCommand {
public:
    explicit SetFocusCommand(WidgetId id) : m_id(id) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetFocus"; }

private:
    WidgetId m_id;
};

/// @brief Command to clear focus
class ClearFocusCommand : public IWidgetCommand {
public:
    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "ClearFocus"; }
};

// =============================================================================
// Layer Commands
// =============================================================================

/// @brief Command to create a layer
class CreateLayerCommand : public IWidgetCommand {
public:
    CreateLayerCommand(std::string name, int z_order = 0)
        : m_name(std::move(name)), m_z_order(z_order) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "CreateLayer"; }

    LayerId layer_id() const { return m_layer_id; }

private:
    std::string m_name;
    int m_z_order;
    LayerId m_layer_id{0};
};

/// @brief Command to destroy a layer
class DestroyLayerCommand : public IWidgetCommand {
public:
    explicit DestroyLayerCommand(LayerId id) : m_id(id) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "DestroyLayer"; }

private:
    LayerId m_id;
};

/// @brief Command to set layer visibility
class SetLayerVisibleCommand : public IWidgetCommand {
public:
    SetLayerVisibleCommand(LayerId id, bool visible)
        : m_id(id), m_visible(visible) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetLayerVisible"; }

private:
    LayerId m_id;
    bool m_visible;
};

/// @brief Command to set layer opacity
class SetLayerOpacityCommand : public IWidgetCommand {
public:
    SetLayerOpacityCommand(LayerId id, float opacity)
        : m_id(id), m_opacity(opacity) {}

    void execute(WidgetStateCore& state) override;
    const char* name() const override { return "SetLayerOpacity"; }

private:
    LayerId m_id;
    float m_opacity;
};

// =============================================================================
// Command Factory
// =============================================================================

/// @brief Factory for creating widget commands
struct WidgetCommands {
    // Lifecycle
    static auto create(std::string type, std::string name = "") {
        return std::make_unique<CreateWidgetCommand>(std::move(type), std::move(name));
    }

    static auto destroy(WidgetId id) {
        return std::make_unique<DestroyWidgetCommand>(id);
    }

    static auto set_parent(WidgetId child, WidgetId parent) {
        return std::make_unique<SetParentCommand>(child, parent);
    }

    // Layout
    static auto set_position(WidgetId id, Vec2 pos) {
        return std::make_unique<SetPositionCommand>(id, pos);
    }

    static auto set_size(WidgetId id, Vec2 size) {
        return std::make_unique<SetSizeCommand>(id, size);
    }

    static auto set_visible(WidgetId id, bool visible) {
        return std::make_unique<SetVisibleCommand>(id, visible);
    }

    // Style
    static auto set_opacity(WidgetId id, float opacity) {
        return std::make_unique<SetOpacityCommand>(id, opacity);
    }

    static auto apply_theme(std::string name) {
        return std::make_unique<ApplyThemeCommand>(std::move(name));
    }

    // Animation
    static auto play_animation(WidgetId id, std::string name) {
        return std::make_unique<PlayAnimationCommand>(id, std::move(name));
    }

    static auto stop_animation(WidgetId id, AnimationId anim = AnimationId{0}) {
        return std::make_unique<StopAnimationCommand>(id, anim);
    }

    // Binding
    static auto bind(WidgetId id, std::string property, std::string source,
                     BindingMode mode = BindingMode::OneWay) {
        return std::make_unique<BindPropertyCommand>(id, std::move(property),
                                                      std::move(source), mode);
    }

    static auto unbind(BindingId id) {
        return std::make_unique<UnbindPropertyCommand>(id);
    }

    // Properties
    static auto set_text(WidgetId id, std::string text) {
        return std::make_unique<SetTextCommand>(id, std::move(text));
    }

    static auto set_value(WidgetId id, float value) {
        return std::make_unique<SetValueCommand>(id, value);
    }

    static auto set_checked(WidgetId id, bool checked) {
        return std::make_unique<SetCheckedCommand>(id, checked);
    }

    // Focus
    static auto set_focus(WidgetId id) {
        return std::make_unique<SetFocusCommand>(id);
    }

    static auto clear_focus() {
        return std::make_unique<ClearFocusCommand>();
    }
};

} // namespace void_widget
