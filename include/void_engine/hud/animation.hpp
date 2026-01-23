/// @file animation.hpp
/// @brief Animation system for void_hud module

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_hud {

// =============================================================================
// Easing Functions
// =============================================================================

/// @brief Easing function type
using EasingFunc = std::function<float(float)>;

/// @brief Collection of easing functions
class Easing {
public:
    static float linear(float t) { return t; }

    static float ease_in(float t) { return t * t; }
    static float ease_out(float t) { return t * (2 - t); }
    static float ease_in_out(float t) {
        return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
    }

    static float ease_in_quad(float t) { return t * t; }
    static float ease_out_quad(float t) { return t * (2 - t); }
    static float ease_in_out_quad(float t) {
        return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
    }

    static float ease_in_cubic(float t) { return t * t * t; }
    static float ease_out_cubic(float t) {
        float f = t - 1;
        return f * f * f + 1;
    }
    static float ease_in_out_cubic(float t) {
        return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
    }

    static float ease_in_elastic(float t);
    static float ease_out_elastic(float t);
    static float ease_in_out_elastic(float t);

    static float ease_in_bounce(float t);
    static float ease_out_bounce(float t);
    static float ease_in_out_bounce(float t);

    /// @brief Get easing function by type
    static EasingFunc get(EasingType type);
};

// =============================================================================
// IHudAnimation Interface
// =============================================================================

/// @brief Interface for HUD animations
class IHudAnimation {
public:
    virtual ~IHudAnimation() = default;

    /// @brief Get animation ID
    virtual HudAnimationId id() const = 0;

    /// @brief Get animation name
    virtual const std::string& name() const = 0;

    /// @brief Get current state
    virtual AnimationState state() const = 0;

    /// @brief Get duration
    virtual float duration() const = 0;

    /// @brief Get current time
    virtual float current_time() const = 0;

    /// @brief Get normalized progress (0-1)
    virtual float progress() const = 0;

    /// @brief Start/resume animation
    virtual void play() = 0;

    /// @brief Pause animation
    virtual void pause() = 0;

    /// @brief Stop and reset animation
    virtual void stop() = 0;

    /// @brief Reset to beginning
    virtual void reset() = 0;

    /// @brief Update animation
    virtual void update(float delta_time) = 0;

    /// @brief Apply animation to target
    virtual void apply(IHudElement* target) = 0;

    /// @brief Set callbacks
    virtual void set_on_start(AnimationCallback callback) = 0;
    virtual void set_on_complete(AnimationCallback callback) = 0;
    virtual void set_on_loop(AnimationCallback callback) = 0;
};

// =============================================================================
// PropertyAnimation
// =============================================================================

/// @brief Animates a single property using keyframes
class PropertyAnimation : public IHudAnimation {
public:
    PropertyAnimation();
    explicit PropertyAnimation(const AnimationDef& def);
    ~PropertyAnimation() override;

    // Identity
    HudAnimationId id() const override { return m_def.id; }
    const std::string& name() const override { return m_def.name; }

    // State
    AnimationState state() const override { return m_state; }
    float duration() const override { return m_def.duration; }
    float current_time() const override { return m_current_time; }
    float progress() const override;

    // Control
    void play() override;
    void pause() override;
    void stop() override;
    void reset() override;

    // Update
    void update(float delta_time) override;
    void apply(IHudElement* target) override;

    // Callbacks
    void set_on_start(AnimationCallback callback) override { m_on_start = std::move(callback); }
    void set_on_complete(AnimationCallback callback) override { m_on_complete = std::move(callback); }
    void set_on_loop(AnimationCallback callback) override { m_on_loop = std::move(callback); }

    // Configuration
    void set_definition(const AnimationDef& def) { m_def = def; }
    const AnimationDef& definition() const { return m_def; }

    void set_play_mode(PlayMode mode) { m_def.play_mode = mode; }
    PlayMode play_mode() const { return m_def.play_mode; }

    // Keyframes
    void add_keyframe(float time, float value, EasingType easing = EasingType::Linear);
    void clear_keyframes();

    // Internal
    void set_id(HudAnimationId id) { m_def.id = id; }

private:
    float evaluate_at(float time) const;
    void handle_loop();

    AnimationDef m_def;
    AnimationState m_state{AnimationState::Idle};
    float m_current_time{0};
    std::uint32_t m_loop_count{0};
    bool m_reverse{false};

    AnimationCallback m_on_start;
    AnimationCallback m_on_complete;
    AnimationCallback m_on_loop;
};

// =============================================================================
// HudAnimationSequence
// =============================================================================

/// @brief Sequence of animations that play in order
class HudAnimationSequence : public IHudAnimation {
public:
    HudAnimationSequence();
    explicit HudAnimationSequence(const std::string& name);
    ~HudAnimationSequence() override;

    // Identity
    HudAnimationId id() const override { return m_id; }
    const std::string& name() const override { return m_name; }

    // State
    AnimationState state() const override { return m_state; }
    float duration() const override;
    float current_time() const override { return m_current_time; }
    float progress() const override;

    // Control
    void play() override;
    void pause() override;
    void stop() override;
    void reset() override;

    // Update
    void update(float delta_time) override;
    void apply(IHudElement* target) override;

    // Callbacks
    void set_on_start(AnimationCallback callback) override { m_on_start = std::move(callback); }
    void set_on_complete(AnimationCallback callback) override { m_on_complete = std::move(callback); }
    void set_on_loop(AnimationCallback callback) override { m_on_loop = std::move(callback); }

    // Sequence building
    void add_animation(std::unique_ptr<IHudAnimation> anim);
    void add_animation(std::unique_ptr<IHudAnimation> anim, float start_time);
    void add_delay(float duration);
    void add_callback(std::function<void()> callback);

    // Query
    std::size_t animation_count() const { return m_animations.size(); }
    IHudAnimation* get_animation(std::size_t index);
    std::size_t current_index() const { return m_current_index; }

    // Internal
    void set_id(HudAnimationId id) { m_id = id; }

private:
    struct SequenceItem {
        std::unique_ptr<IHudAnimation> animation;
        float start_time{0};
        bool started{false};
    };

    HudAnimationId m_id;
    std::string m_name;
    std::vector<SequenceItem> m_animations;
    AnimationState m_state{AnimationState::Idle};
    float m_current_time{0};
    std::size_t m_current_index{0};

    AnimationCallback m_on_start;
    AnimationCallback m_on_complete;
    AnimationCallback m_on_loop;
};

// =============================================================================
// HudAnimationGroup
// =============================================================================

/// @brief Group of animations that play simultaneously
class HudAnimationGroup : public IHudAnimation {
public:
    HudAnimationGroup();
    explicit HudAnimationGroup(const std::string& name);
    ~HudAnimationGroup() override;

    // Identity
    HudAnimationId id() const override { return m_id; }
    const std::string& name() const override { return m_name; }

    // State
    AnimationState state() const override { return m_state; }
    float duration() const override;
    float current_time() const override { return m_current_time; }
    float progress() const override;

    // Control
    void play() override;
    void pause() override;
    void stop() override;
    void reset() override;

    // Update
    void update(float delta_time) override;
    void apply(IHudElement* target) override;

    // Callbacks
    void set_on_start(AnimationCallback callback) override { m_on_start = std::move(callback); }
    void set_on_complete(AnimationCallback callback) override { m_on_complete = std::move(callback); }
    void set_on_loop(AnimationCallback callback) override { m_on_loop = std::move(callback); }

    // Group building
    void add_animation(std::unique_ptr<IHudAnimation> anim);

    // Query
    std::size_t animation_count() const { return m_animations.size(); }
    IHudAnimation* get_animation(std::size_t index);

    // Internal
    void set_id(HudAnimationId id) { m_id = id; }

private:
    HudAnimationId m_id;
    std::string m_name;
    std::vector<std::unique_ptr<IHudAnimation>> m_animations;
    AnimationState m_state{AnimationState::Idle};
    float m_current_time{0};

    AnimationCallback m_on_start;
    AnimationCallback m_on_complete;
    AnimationCallback m_on_loop;
};

// =============================================================================
// HudTransition
// =============================================================================

/// @brief Quick transitions for property changes
class HudTransition {
public:
    HudTransition();
    explicit HudTransition(const TransitionDef& def);

    // Configuration
    void set_definition(const TransitionDef& def) { m_def = def; }
    const TransitionDef& definition() const { return m_def; }

    // Start transition
    void start(IHudElement* target, float from_value, float to_value);
    void start(IHudElement* target, const Color& from_color, const Color& to_color);

    // Update
    void update(float delta_time);
    bool is_complete() const { return m_complete; }
    bool is_active() const { return m_active; }

    // Cancel
    void cancel();

private:
    TransitionDef m_def;
    IHudElement* m_target{nullptr};
    float m_from_value{0};
    float m_to_value{0};
    Color m_from_color;
    Color m_to_color;
    bool m_is_color{false};
    float m_elapsed{0};
    bool m_active{false};
    bool m_complete{false};
};

// =============================================================================
// HudAnimator
// =============================================================================

/// @brief Manages animations for HUD elements
class HudAnimator {
public:
    HudAnimator();
    ~HudAnimator();

    // Animation registration
    HudAnimationId register_animation(std::unique_ptr<IHudAnimation> anim);
    HudAnimationId register_animation(const AnimationDef& def);
    bool unregister_animation(HudAnimationId id);

    // Animation lookup
    IHudAnimation* get_animation(HudAnimationId id);
    HudAnimationId find_animation(std::string_view name) const;

    // Play animations
    void play(HudAnimationId id, IHudElement* target);
    void play_sequence(const std::vector<HudAnimationId>& ids, IHudElement* target);
    void play_group(const std::vector<HudAnimationId>& ids, IHudElement* target);

    // Quick animations
    void fade_in(IHudElement* target, float duration = 0.3f);
    void fade_out(IHudElement* target, float duration = 0.3f);
    void slide_in(IHudElement* target, AnchorPoint from, float duration = 0.3f);
    void slide_out(IHudElement* target, AnchorPoint to, float duration = 0.3f);
    void scale_in(IHudElement* target, float duration = 0.3f);
    void scale_out(IHudElement* target, float duration = 0.3f);
    void pulse(IHudElement* target, float scale = 1.2f, float duration = 0.5f);
    void shake(IHudElement* target, float intensity = 5.0f, float duration = 0.3f);
    void bounce(IHudElement* target, float height = 20.0f, float duration = 0.5f);

    // Transitions
    void transition(IHudElement* target, AnimProperty property,
                    float to_value, float duration = 0.3f,
                    EasingType easing = EasingType::EaseOutQuad);
    void transition_color(IHudElement* target, const Color& to_color,
                          float duration = 0.3f,
                          EasingType easing = EasingType::EaseOutQuad);

    // Stop animations
    void stop(IHudElement* target);
    void stop(HudAnimationId id);
    void stop_all();

    // Pause/Resume
    void pause(IHudElement* target);
    void resume(IHudElement* target);
    void pause_all();
    void resume_all();

    // Update
    void update(float delta_time);

    // Query
    bool is_animating(IHudElement* target) const;
    std::vector<HudAnimationId> get_active_animations(IHudElement* target) const;

private:
    struct ActiveAnimation {
        IHudAnimation* animation;
        IHudElement* target;
        bool owned{false};
    };

    std::unordered_map<HudAnimationId, std::unique_ptr<IHudAnimation>> m_animations;
    std::vector<ActiveAnimation> m_active;
    std::vector<HudTransition> m_transitions;
    std::uint64_t m_next_id{1};
};

// =============================================================================
// AnimationBuilder
// =============================================================================

/// @brief Fluent builder for creating animations
class AnimationBuilder {
public:
    AnimationBuilder();
    explicit AnimationBuilder(const std::string& name);

    // Properties
    AnimationBuilder& property(AnimProperty prop);
    AnimationBuilder& duration(float dur);
    AnimationBuilder& delay(float del);
    AnimationBuilder& play_mode(PlayMode mode);
    AnimationBuilder& repeat(std::uint32_t count);
    AnimationBuilder& loop();

    // Keyframes
    AnimationBuilder& from(float value);
    AnimationBuilder& to(float value);
    AnimationBuilder& keyframe(float time, float value, EasingType easing = EasingType::Linear);

    // Easing
    AnimationBuilder& easing(EasingType type);
    AnimationBuilder& ease_in();
    AnimationBuilder& ease_out();
    AnimationBuilder& ease_in_out();

    // Callbacks
    AnimationBuilder& on_start(AnimationCallback callback);
    AnimationBuilder& on_complete(AnimationCallback callback);
    AnimationBuilder& on_loop(AnimationCallback callback);

    // Build
    std::unique_ptr<PropertyAnimation> build();
    AnimationDef build_def();

private:
    AnimationDef m_def;
    float m_from_value{0};
    float m_to_value{0};
    bool m_has_from{false};
    bool m_has_to{false};
    AnimationCallback m_on_start;
    AnimationCallback m_on_complete;
    AnimationCallback m_on_loop;
};

// =============================================================================
// Preset Animations
// =============================================================================

namespace presets {

/// @brief Fade in animation
inline std::unique_ptr<PropertyAnimation> fade_in(float duration = 0.3f) {
    return AnimationBuilder("FadeIn")
        .property(AnimProperty::Opacity)
        .from(0).to(1)
        .duration(duration)
        .ease_out()
        .build();
}

/// @brief Fade out animation
inline std::unique_ptr<PropertyAnimation> fade_out(float duration = 0.3f) {
    return AnimationBuilder("FadeOut")
        .property(AnimProperty::Opacity)
        .from(1).to(0)
        .duration(duration)
        .ease_in()
        .build();
}

/// @brief Scale up animation
inline std::unique_ptr<PropertyAnimation> scale_up(float duration = 0.3f) {
    return AnimationBuilder("ScaleUp")
        .property(AnimProperty::Scale)
        .from(0).to(1)
        .duration(duration)
        .ease_out()
        .build();
}

/// @brief Scale down animation
inline std::unique_ptr<PropertyAnimation> scale_down(float duration = 0.3f) {
    return AnimationBuilder("ScaleDown")
        .property(AnimProperty::Scale)
        .from(1).to(0)
        .duration(duration)
        .ease_in()
        .build();
}

/// @brief Pulse animation
inline std::unique_ptr<PropertyAnimation> pulse(float scale = 1.2f, float duration = 0.5f) {
    return AnimationBuilder("Pulse")
        .property(AnimProperty::Scale)
        .from(1).to(scale)
        .duration(duration)
        .play_mode(PlayMode::PingPong)
        .loop()
        .ease_in_out()
        .build();
}

/// @brief Bounce animation
inline std::unique_ptr<PropertyAnimation> bounce(float height = 20.0f, float duration = 0.5f) {
    auto anim = AnimationBuilder("Bounce")
        .property(AnimProperty::PositionY)
        .duration(duration)
        .build();
    anim->add_keyframe(0, 0, EasingType::EaseOutQuad);
    anim->add_keyframe(0.5f, -height, EasingType::EaseOutQuad);
    anim->add_keyframe(1.0f, 0, EasingType::EaseInQuad);
    return anim;
}

} // namespace presets

} // namespace void_hud
