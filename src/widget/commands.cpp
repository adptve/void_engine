/// @file commands.cpp
/// @brief Implementation of widget commands

#include <void_engine/widget/commands.hpp>
#include <void_engine/widget/widget_state_core.hpp>

namespace void_widget {

// =============================================================================
// Widget Lifecycle Commands
// =============================================================================

void CreateWidgetCommand::execute(WidgetStateCore& state) {
    m_created_id = state.create_widget(m_type, m_name);
}

void DestroyWidgetCommand::execute(WidgetStateCore& state) {
    state.destroy_widget(m_id);
}

void SetParentCommand::execute(WidgetStateCore& state) {
    state.set_parent(m_child, m_parent);
}

void SetLayerCommand::execute(WidgetStateCore& state) {
    auto* widget = state.widget_registry().get_mut(m_widget);
    if (!widget) return;

    // Remove from old layer
    LayerId old_layer = widget->layer;
    for (auto& layer : state.widget_registry().layers) {
        if (layer.id == old_layer) {
            layer.widgets.erase(
                std::remove(layer.widgets.begin(), layer.widgets.end(), m_widget),
                layer.widgets.end());
            break;
        }
    }

    // Add to new layer
    widget->layer = m_layer;
    state.widget_registry().widget_layer[m_widget] = m_layer;
    for (auto& layer : state.widget_registry().layers) {
        if (layer.id == m_layer) {
            layer.widgets.push_back(m_widget);
            break;
        }
    }
}

// =============================================================================
// Layout Commands
// =============================================================================

void SetPositionCommand::execute(WidgetStateCore& state) {
    auto* layout = state.layout_state().get_mut(m_id);
    if (layout) {
        layout->position = m_position;
        state.layout_state().mark_dirty(m_id);
    }
}

void SetSizeCommand::execute(WidgetStateCore& state) {
    auto* layout = state.layout_state().get_mut(m_id);
    if (layout) {
        layout->size = m_size;
        state.layout_state().mark_dirty(m_id);
    }
}

void SetAnchorCommand::execute(WidgetStateCore& state) {
    auto* layout = state.layout_state().get_mut(m_id);
    if (layout) {
        layout->anchor = m_anchor;
        state.layout_state().mark_dirty(m_id);
    }
}

void SetPivotCommand::execute(WidgetStateCore& state) {
    auto* layout = state.layout_state().get_mut(m_id);
    if (layout) {
        layout->pivot = m_pivot;
        state.layout_state().mark_dirty(m_id);
    }
}

void SetMarginCommand::execute(WidgetStateCore& state) {
    auto* layout = state.layout_state().get_mut(m_id);
    if (layout) {
        layout->margin = m_margin;
        state.layout_state().mark_dirty(m_id);
    }
}

void SetPaddingCommand::execute(WidgetStateCore& state) {
    auto* layout = state.layout_state().get_mut(m_id);
    if (layout) {
        layout->padding = m_padding;
        state.layout_state().mark_dirty(m_id);
    }
}

void SetVisibleCommand::execute(WidgetStateCore& state) {
    auto* widget = state.widget_registry().get_mut(m_id);
    if (widget) {
        widget->visibility = m_visible ? Visibility::Visible : Visibility::Hidden;
    }
}

void SetRotationCommand::execute(WidgetStateCore& state) {
    auto* layout = state.layout_state().get_mut(m_id);
    if (layout) {
        layout->rotation = m_degrees;
        state.layout_state().mark_dirty(m_id);
    }
}

void SetScaleCommand::execute(WidgetStateCore& state) {
    auto* layout = state.layout_state().get_mut(m_id);
    if (layout) {
        layout->scale = m_scale;
        state.layout_state().mark_dirty(m_id);
    }
}

// =============================================================================
// Style Commands
// =============================================================================

void SetStylePropertyCommand::execute(WidgetStateCore& state) {
    auto& overrides = state.style_state().overrides[m_id];

    if (m_property == "background_color") {
        overrides.background_color = std::any_cast<Color>(m_value);
    } else if (m_property == "border_color") {
        overrides.border_color = std::any_cast<Color>(m_value);
    } else if (m_property == "text_color") {
        overrides.text_color = std::any_cast<Color>(m_value);
    } else if (m_property == "border_width") {
        overrides.border_width = std::any_cast<float>(m_value);
    } else if (m_property == "border_radius") {
        overrides.border_radius = std::any_cast<float>(m_value);
    } else if (m_property == "opacity") {
        overrides.opacity = std::any_cast<float>(m_value);
    } else if (m_property == "font") {
        overrides.font = std::any_cast<std::string>(m_value);
    } else if (m_property == "font_size") {
        overrides.font_size = std::any_cast<float>(m_value);
    }

    // Invalidate computed style cache
    state.style_state().computed_styles.erase(m_id);
}

void SetOpacityCommand::execute(WidgetStateCore& state) {
    state.style_state().overrides[m_id].opacity = m_opacity;
    state.style_state().computed_styles.erase(m_id);
}

void ApplyThemeCommand::execute(WidgetStateCore& state) {
    state.apply_theme(m_theme_name);
}

// =============================================================================
// Animation Commands
// =============================================================================

void PlayAnimationCommand::execute(WidgetStateCore& state) {
    const auto* def = state.animation_state().get_definition(m_anim_name);
    if (!def) return;

    ActiveAnimation anim;
    anim.id = AnimationId{state.animation_state().next_animation_id++};
    anim.definition_name = m_anim_name;
    anim.target_property = def->target_property;
    anim.duration = def->duration;
    anim.play_mode = def->play_mode;
    anim.keyframes = def->keyframes;
    anim.state = AnimState::Playing;
    anim.max_loops = def->repeat_count;

    state.animation_state().animations[m_id].push_back(anim);
    m_anim_id = anim.id;
}

void StopAnimationCommand::execute(WidgetStateCore& state) {
    auto it = state.animation_state().animations.find(m_id);
    if (it == state.animation_state().animations.end()) return;

    if (m_anim.value == 0) {
        // Stop all animations
        it->second.clear();
    } else {
        // Stop specific animation
        it->second.erase(
            std::remove_if(it->second.begin(), it->second.end(),
                           [this](const ActiveAnimation& a) { return a.id == m_anim; }),
            it->second.end());
    }
}

void AnimatePropertyCommand::execute(WidgetStateCore& state) {
    ActiveAnimation anim;
    anim.id = AnimationId{state.animation_state().next_animation_id++};
    anim.target_property = m_property;
    anim.duration = m_duration;
    anim.play_mode = PlayMode::Once;
    anim.state = AnimState::Playing;

    // Create keyframes from current value to target
    // Get current value from widget
    const auto* widget = state.widget_registry().get(m_id);
    if (widget) {
        Keyframe start;
        start.time = 0;
        // Use find() since properties is const
        auto it = widget->properties.find(m_property);
        if (it != widget->properties.end()) {
            start.value = it->second;
        } else {
            start.value = m_target;  // If no current value, start from target
        }
        start.easing = m_easing;

        Keyframe end;
        end.time = m_duration;
        end.value = m_target;
        end.easing = EasingType::Linear;

        anim.keyframes = {start, end};
    }

    state.animation_state().animations[m_id].push_back(anim);
    m_anim_id = anim.id;
}

// =============================================================================
// Binding Commands
// =============================================================================

void BindPropertyCommand::execute(WidgetStateCore& state) {
    DataBinding binding;
    binding.id = BindingId{state.binding_state().next_binding_id++};
    binding.widget = m_id;
    binding.target_property = m_property;
    binding.source_path = m_source_path;
    binding.mode = m_mode;
    binding.enabled = true;

    state.binding_state().bindings[m_id].push_back(binding);
    state.binding_state().bindings_by_id[binding.id] = &state.binding_state().bindings[m_id].back();

    m_binding_id = binding.id;
}

void UnbindPropertyCommand::execute(WidgetStateCore& state) {
    // Find and remove the binding
    for (auto& [widget_id, bindings] : state.binding_state().bindings) {
        bindings.erase(
            std::remove_if(bindings.begin(), bindings.end(),
                           [this](const DataBinding& b) { return b.id == m_binding; }),
            bindings.end());
    }
    state.binding_state().bindings_by_id.erase(m_binding);
}

// =============================================================================
// Widget Property Commands
// =============================================================================

void SetTextCommand::execute(WidgetStateCore& state) {
    auto* widget = state.widget_registry().get_mut(m_id);
    if (widget) {
        widget->set_property("text", m_text);
    }
}

void SetValueCommand::execute(WidgetStateCore& state) {
    auto* widget = state.widget_registry().get_mut(m_id);
    if (widget) {
        widget->set_property("value", m_value);
    }
}

void SetCheckedCommand::execute(WidgetStateCore& state) {
    auto* widget = state.widget_registry().get_mut(m_id);
    if (widget) {
        if (m_checked) {
            widget->state = widget->state | WidgetState::Checked;
        } else {
            widget->state = static_cast<WidgetState>(
                static_cast<uint32_t>(widget->state) & ~static_cast<uint32_t>(WidgetState::Checked));
        }
    }
}

void SetEnabledCommand::execute(WidgetStateCore& state) {
    auto* widget = state.widget_registry().get_mut(m_id);
    if (widget) {
        if (!m_enabled) {
            widget->state = widget->state | WidgetState::Disabled;
        } else {
            widget->state = static_cast<WidgetState>(
                static_cast<uint32_t>(widget->state) & ~static_cast<uint32_t>(WidgetState::Disabled));
        }
    }
}

void SetPropertyCommand::execute(WidgetStateCore& state) {
    auto* widget = state.widget_registry().get_mut(m_id);
    if (widget) {
        widget->properties[m_key] = m_value;
    }
}

// =============================================================================
// Focus Commands
// =============================================================================

void SetFocusCommand::execute(WidgetStateCore& state) {
    state.interaction_state().focused_widget = m_id;
}

void ClearFocusCommand::execute(WidgetStateCore& state) {
    state.interaction_state().focused_widget = WidgetId{0};
}

// =============================================================================
// Layer Commands
// =============================================================================

void CreateLayerCommand::execute(WidgetStateCore& state) {
    m_layer_id = state.create_layer(m_name, m_z_order);
}

void DestroyLayerCommand::execute(WidgetStateCore& state) {
    state.destroy_layer(m_id);
}

void SetLayerVisibleCommand::execute(WidgetStateCore& state) {
    for (auto& layer : state.widget_registry().layers) {
        if (layer.id == m_id) {
            layer.visible = m_visible;
            break;
        }
    }
}

void SetLayerOpacityCommand::execute(WidgetStateCore& state) {
    for (auto& layer : state.widget_registry().layers) {
        if (layer.id == m_id) {
            layer.opacity = m_opacity;
            break;
        }
    }
}

} // namespace void_widget
