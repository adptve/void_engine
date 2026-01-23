/// @file animation.cpp
/// @brief Implementation of animation system for void_hud module

#include "void_engine/hud/animation.hpp"
#include "void_engine/hud/elements.hpp"

#include <algorithm>
#include <cmath>

namespace void_hud {

// =============================================================================
// Easing Functions
// =============================================================================

float Easing::ease_in_elastic(float t) {
    if (t == 0 || t == 1) return t;
    const float c4 = (2 * 3.14159f) / 3;
    return -std::pow(2.0f, 10 * t - 10) * std::sin((t * 10 - 10.75f) * c4);
}

float Easing::ease_out_elastic(float t) {
    if (t == 0 || t == 1) return t;
    const float c4 = (2 * 3.14159f) / 3;
    return std::pow(2.0f, -10 * t) * std::sin((t * 10 - 0.75f) * c4) + 1;
}

float Easing::ease_in_out_elastic(float t) {
    if (t == 0 || t == 1) return t;
    const float c5 = (2 * 3.14159f) / 4.5f;
    if (t < 0.5f) {
        return -(std::pow(2.0f, 20 * t - 10) * std::sin((20 * t - 11.125f) * c5)) / 2;
    }
    return (std::pow(2.0f, -20 * t + 10) * std::sin((20 * t - 11.125f) * c5)) / 2 + 1;
}

float Easing::ease_out_bounce(float t) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;

    if (t < 1 / d1) {
        return n1 * t * t;
    } else if (t < 2 / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

float Easing::ease_in_bounce(float t) {
    return 1 - ease_out_bounce(1 - t);
}

float Easing::ease_in_out_bounce(float t) {
    return t < 0.5f
        ? (1 - ease_out_bounce(1 - 2 * t)) / 2
        : (1 + ease_out_bounce(2 * t - 1)) / 2;
}

EasingFunc Easing::get(EasingType type) {
    switch (type) {
        case EasingType::Linear: return linear;
        case EasingType::EaseIn: return ease_in;
        case EasingType::EaseOut: return ease_out;
        case EasingType::EaseInOut: return ease_in_out;
        case EasingType::EaseInQuad: return ease_in_quad;
        case EasingType::EaseOutQuad: return ease_out_quad;
        case EasingType::EaseInOutQuad: return ease_in_out_quad;
        case EasingType::EaseInCubic: return ease_in_cubic;
        case EasingType::EaseOutCubic: return ease_out_cubic;
        case EasingType::EaseInOutCubic: return ease_in_out_cubic;
        case EasingType::EaseInElastic: return ease_in_elastic;
        case EasingType::EaseOutElastic: return ease_out_elastic;
        case EasingType::EaseInOutElastic: return ease_in_out_elastic;
        case EasingType::EaseInBounce: return ease_in_bounce;
        case EasingType::EaseOutBounce: return ease_out_bounce;
        case EasingType::EaseInOutBounce: return ease_in_out_bounce;
        default: return linear;
    }
}

// =============================================================================
// PropertyAnimation
// =============================================================================

PropertyAnimation::PropertyAnimation() = default;

PropertyAnimation::PropertyAnimation(const AnimationDef& def)
    : m_def(def) {}

PropertyAnimation::~PropertyAnimation() = default;

float PropertyAnimation::progress() const {
    if (m_def.duration <= 0) return 0;
    return m_current_time / m_def.duration;
}

void PropertyAnimation::play() {
    if (m_state == AnimationState::Idle) {
        m_current_time = 0;
        m_loop_count = 0;
        m_reverse = false;

        if (m_on_start) {
            m_on_start(m_def.id);
        }
    }
    m_state = AnimationState::Playing;
}

void PropertyAnimation::pause() {
    if (m_state == AnimationState::Playing) {
        m_state = AnimationState::Paused;
    }
}

void PropertyAnimation::stop() {
    m_state = AnimationState::Idle;
    m_current_time = 0;
    m_loop_count = 0;
    m_reverse = false;
}

void PropertyAnimation::reset() {
    m_current_time = 0;
    m_loop_count = 0;
    m_reverse = false;
}

void PropertyAnimation::update(float delta_time) {
    if (m_state != AnimationState::Playing) return;

    // Handle delay
    if (m_def.delay > 0 && m_current_time < m_def.delay) {
        m_current_time += delta_time;
        if (m_current_time < m_def.delay) return;
        delta_time = m_current_time - m_def.delay;
        m_current_time = m_def.delay;
    }

    // Update time based on play mode
    if (m_reverse) {
        m_current_time -= delta_time;
    } else {
        m_current_time += delta_time;
    }

    // Check completion
    if (m_current_time >= m_def.duration || m_current_time <= 0) {
        handle_loop();
    }
}

void PropertyAnimation::apply(IHudElement* target) {
    if (!target || m_def.keyframes.empty()) return;

    float value = evaluate_at(m_current_time - m_def.delay);

    switch (m_def.property) {
        case AnimProperty::PositionX: {
            Vec2 pos = target->position();
            pos.x = value;
            target->set_position(pos);
            break;
        }
        case AnimProperty::PositionY: {
            Vec2 pos = target->position();
            pos.y = value;
            target->set_position(pos);
            break;
        }
        case AnimProperty::Width: {
            Vec2 size = target->size();
            size.x = value;
            target->set_size(size);
            break;
        }
        case AnimProperty::Height: {
            Vec2 size = target->size();
            size.y = value;
            target->set_size(size);
            break;
        }
        case AnimProperty::Opacity:
            target->set_opacity(value);
            break;
        case AnimProperty::Rotation:
            target->set_rotation(value);
            break;
        case AnimProperty::Scale:
        case AnimProperty::ScaleX:
        case AnimProperty::ScaleY: {
            Vec2 scale = target->scale();
            if (m_def.property == AnimProperty::Scale) {
                scale.x = scale.y = value;
            } else if (m_def.property == AnimProperty::ScaleX) {
                scale.x = value;
            } else {
                scale.y = value;
            }
            target->set_scale(scale);
            break;
        }
        default:
            break;
    }
}

void PropertyAnimation::add_keyframe(float time, float value, EasingType easing) {
    m_def.keyframes.push_back({time, value, easing});

    // Sort by time
    std::sort(m_def.keyframes.begin(), m_def.keyframes.end(),
              [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
}

void PropertyAnimation::clear_keyframes() {
    m_def.keyframes.clear();
}

float PropertyAnimation::evaluate_at(float time) const {
    if (m_def.keyframes.empty()) return 0;
    if (m_def.keyframes.size() == 1) return m_def.keyframes[0].value;

    time = std::clamp(time, 0.0f, m_def.duration);
    float normalized_time = time / m_def.duration;

    // Find surrounding keyframes
    const Keyframe* prev = &m_def.keyframes[0];
    const Keyframe* next = &m_def.keyframes[0];

    for (std::size_t i = 0; i < m_def.keyframes.size(); ++i) {
        if (m_def.keyframes[i].time <= normalized_time) {
            prev = &m_def.keyframes[i];
        }
        if (m_def.keyframes[i].time >= normalized_time) {
            next = &m_def.keyframes[i];
            break;
        }
    }

    if (prev == next) return prev->value;

    // Interpolate
    float segment_time = next->time - prev->time;
    if (segment_time <= 0) return prev->value;

    float t = (normalized_time - prev->time) / segment_time;
    auto easing = Easing::get(prev->easing);
    t = easing(t);

    return prev->value + (next->value - prev->value) * t;
}

void PropertyAnimation::handle_loop() {
    switch (m_def.play_mode) {
        case PlayMode::Once:
            m_state = AnimationState::Finished;
            m_current_time = m_def.duration;
            if (m_on_complete) {
                m_on_complete(m_def.id);
            }
            break;

        case PlayMode::Loop:
            ++m_loop_count;
            if (m_def.repeat_count > 0 && m_loop_count >= m_def.repeat_count) {
                m_state = AnimationState::Finished;
                if (m_on_complete) {
                    m_on_complete(m_def.id);
                }
            } else {
                m_current_time = m_def.delay;
                if (m_on_loop) {
                    m_on_loop(m_def.id);
                }
            }
            break;

        case PlayMode::PingPong:
            m_reverse = !m_reverse;
            if (!m_reverse) {
                ++m_loop_count;
                if (m_def.repeat_count > 0 && m_loop_count >= m_def.repeat_count) {
                    m_state = AnimationState::Finished;
                    if (m_on_complete) {
                        m_on_complete(m_def.id);
                    }
                } else if (m_on_loop) {
                    m_on_loop(m_def.id);
                }
            }
            m_current_time = m_reverse ? m_def.duration + m_def.delay : m_def.delay;
            break;

        case PlayMode::Reverse:
            m_state = AnimationState::Finished;
            m_current_time = 0;
            if (m_on_complete) {
                m_on_complete(m_def.id);
            }
            break;
    }
}

// =============================================================================
// HudAnimationSequence
// =============================================================================

HudAnimationSequence::HudAnimationSequence() = default;

HudAnimationSequence::HudAnimationSequence(const std::string& name)
    : m_name(name) {}

HudAnimationSequence::~HudAnimationSequence() = default;

float HudAnimationSequence::duration() const {
    float total = 0;
    for (const auto& item : m_animations) {
        if (item.animation) {
            total = std::max(total, item.start_time + item.animation->duration());
        }
    }
    return total;
}

float HudAnimationSequence::progress() const {
    float dur = duration();
    if (dur <= 0) return 0;
    return m_current_time / dur;
}

void HudAnimationSequence::play() {
    if (m_state == AnimationState::Idle) {
        m_current_time = 0;
        m_current_index = 0;
        for (auto& item : m_animations) {
            item.started = false;
        }
        if (m_on_start) {
            m_on_start(m_id);
        }
    }
    m_state = AnimationState::Playing;
}

void HudAnimationSequence::pause() {
    if (m_state == AnimationState::Playing) {
        m_state = AnimationState::Paused;
        for (auto& item : m_animations) {
            if (item.animation && item.started) {
                item.animation->pause();
            }
        }
    }
}

void HudAnimationSequence::stop() {
    m_state = AnimationState::Idle;
    m_current_time = 0;
    for (auto& item : m_animations) {
        if (item.animation) {
            item.animation->stop();
        }
        item.started = false;
    }
}

void HudAnimationSequence::reset() {
    m_current_time = 0;
    m_current_index = 0;
    for (auto& item : m_animations) {
        if (item.animation) {
            item.animation->reset();
        }
        item.started = false;
    }
}

void HudAnimationSequence::update(float delta_time) {
    if (m_state != AnimationState::Playing) return;

    m_current_time += delta_time;

    bool all_complete = true;
    for (auto& item : m_animations) {
        if (!item.animation) continue;

        // Start animations that should start
        if (!item.started && m_current_time >= item.start_time) {
            item.started = true;
            item.animation->play();
        }

        // Update active animations
        if (item.started && item.animation->state() == AnimationState::Playing) {
            item.animation->update(delta_time);
        }

        if (item.animation->state() != AnimationState::Finished) {
            all_complete = false;
        }
    }

    if (all_complete) {
        m_state = AnimationState::Finished;
        if (m_on_complete) {
            m_on_complete(m_id);
        }
    }
}

void HudAnimationSequence::apply(IHudElement* target) {
    for (auto& item : m_animations) {
        if (item.animation && item.started) {
            item.animation->apply(target);
        }
    }
}

void HudAnimationSequence::add_animation(std::unique_ptr<IHudAnimation> anim) {
    float start_time = 0;
    if (!m_animations.empty()) {
        auto& last = m_animations.back();
        if (last.animation) {
            start_time = last.start_time + last.animation->duration();
        }
    }
    add_animation(std::move(anim), start_time);
}

void HudAnimationSequence::add_animation(std::unique_ptr<IHudAnimation> anim, float start_time) {
    m_animations.push_back({std::move(anim), start_time, false});
}

void HudAnimationSequence::add_delay(float dur) {
    // Add an empty animation for delay
    auto dummy = std::make_unique<PropertyAnimation>();
    AnimationDef def;
    def.duration = dur;
    dummy->set_definition(def);
    add_animation(std::move(dummy));
}

void HudAnimationSequence::add_callback(std::function<void()> callback) {
    // Callbacks would be handled differently
}

IHudAnimation* HudAnimationSequence::get_animation(std::size_t index) {
    if (index >= m_animations.size()) return nullptr;
    return m_animations[index].animation.get();
}

// =============================================================================
// HudAnimationGroup
// =============================================================================

HudAnimationGroup::HudAnimationGroup() = default;

HudAnimationGroup::HudAnimationGroup(const std::string& name)
    : m_name(name) {}

HudAnimationGroup::~HudAnimationGroup() = default;

float HudAnimationGroup::duration() const {
    float max_dur = 0;
    for (const auto& anim : m_animations) {
        if (anim) {
            max_dur = std::max(max_dur, anim->duration());
        }
    }
    return max_dur;
}

float HudAnimationGroup::progress() const {
    float dur = duration();
    if (dur <= 0) return 0;
    return m_current_time / dur;
}

void HudAnimationGroup::play() {
    if (m_state == AnimationState::Idle) {
        m_current_time = 0;
        if (m_on_start) {
            m_on_start(m_id);
        }
    }
    m_state = AnimationState::Playing;
    for (auto& anim : m_animations) {
        if (anim) {
            anim->play();
        }
    }
}

void HudAnimationGroup::pause() {
    if (m_state == AnimationState::Playing) {
        m_state = AnimationState::Paused;
        for (auto& anim : m_animations) {
            if (anim) {
                anim->pause();
            }
        }
    }
}

void HudAnimationGroup::stop() {
    m_state = AnimationState::Idle;
    m_current_time = 0;
    for (auto& anim : m_animations) {
        if (anim) {
            anim->stop();
        }
    }
}

void HudAnimationGroup::reset() {
    m_current_time = 0;
    for (auto& anim : m_animations) {
        if (anim) {
            anim->reset();
        }
    }
}

void HudAnimationGroup::update(float delta_time) {
    if (m_state != AnimationState::Playing) return;

    m_current_time += delta_time;

    bool all_complete = true;
    for (auto& anim : m_animations) {
        if (anim) {
            anim->update(delta_time);
            if (anim->state() != AnimationState::Finished) {
                all_complete = false;
            }
        }
    }

    if (all_complete) {
        m_state = AnimationState::Finished;
        if (m_on_complete) {
            m_on_complete(m_id);
        }
    }
}

void HudAnimationGroup::apply(IHudElement* target) {
    for (auto& anim : m_animations) {
        if (anim) {
            anim->apply(target);
        }
    }
}

void HudAnimationGroup::add_animation(std::unique_ptr<IHudAnimation> anim) {
    m_animations.push_back(std::move(anim));
}

IHudAnimation* HudAnimationGroup::get_animation(std::size_t index) {
    if (index >= m_animations.size()) return nullptr;
    return m_animations[index].get();
}

// =============================================================================
// HudTransition
// =============================================================================

HudTransition::HudTransition() = default;

HudTransition::HudTransition(const TransitionDef& def)
    : m_def(def) {}

void HudTransition::start(IHudElement* target, float from_value, float to_value) {
    m_target = target;
    m_from_value = from_value;
    m_to_value = to_value;
    m_is_color = false;
    m_elapsed = 0;
    m_active = true;
    m_complete = false;
}

void HudTransition::start(IHudElement* target, const Color& from_color, const Color& to_color) {
    m_target = target;
    m_from_color = from_color;
    m_to_color = to_color;
    m_is_color = true;
    m_elapsed = 0;
    m_active = true;
    m_complete = false;
}

void HudTransition::update(float delta_time) {
    if (!m_active || m_complete) return;

    m_elapsed += delta_time;

    float t = std::min(1.0f, m_elapsed / m_def.duration);
    auto easing = Easing::get(m_def.easing);
    t = easing(t);

    if (m_target) {
        if (m_is_color) {
            Color color = m_from_color.lerp(m_to_color, t);
            m_target->properties_mut().color = color;
        } else {
            float value = m_from_value + (m_to_value - m_from_value) * t;

            switch (m_def.property) {
                case AnimProperty::Opacity:
                    m_target->set_opacity(value);
                    break;
                case AnimProperty::Scale:
                    m_target->set_scale({value, value});
                    break;
                case AnimProperty::PositionX: {
                    Vec2 pos = m_target->position();
                    pos.x = value;
                    m_target->set_position(pos);
                    break;
                }
                case AnimProperty::PositionY: {
                    Vec2 pos = m_target->position();
                    pos.y = value;
                    m_target->set_position(pos);
                    break;
                }
                default:
                    break;
            }
        }
    }

    if (m_elapsed >= m_def.duration) {
        m_complete = true;
        m_active = false;
    }
}

void HudTransition::cancel() {
    m_active = false;
    m_complete = true;
}

// =============================================================================
// HudAnimator
// =============================================================================

HudAnimator::HudAnimator() = default;
HudAnimator::~HudAnimator() = default;

HudAnimationId HudAnimator::register_animation(std::unique_ptr<IHudAnimation> anim) {
    HudAnimationId id{m_next_id++};
    if (auto* prop = dynamic_cast<PropertyAnimation*>(anim.get())) {
        prop->set_id(id);
    } else if (auto* seq = dynamic_cast<HudAnimationSequence*>(anim.get())) {
        seq->set_id(id);
    } else if (auto* group = dynamic_cast<HudAnimationGroup*>(anim.get())) {
        group->set_id(id);
    }
    m_animations[id] = std::move(anim);
    return id;
}

HudAnimationId HudAnimator::register_animation(const AnimationDef& def) {
    auto anim = std::make_unique<PropertyAnimation>(def);
    return register_animation(std::move(anim));
}

bool HudAnimator::unregister_animation(HudAnimationId id) {
    return m_animations.erase(id) > 0;
}

IHudAnimation* HudAnimator::get_animation(HudAnimationId id) {
    auto it = m_animations.find(id);
    return it != m_animations.end() ? it->second.get() : nullptr;
}

HudAnimationId HudAnimator::find_animation(std::string_view name) const {
    for (const auto& [id, anim] : m_animations) {
        if (anim && anim->name() == name) {
            return id;
        }
    }
    return HudAnimationId{};
}

void HudAnimator::play(HudAnimationId id, IHudElement* target) {
    auto* anim = get_animation(id);
    if (!anim || !target) return;

    anim->reset();
    anim->play();
    m_active.push_back({anim, target, false});
}

void HudAnimator::play_sequence(const std::vector<HudAnimationId>& ids, IHudElement* target) {
    auto seq = std::make_unique<HudAnimationSequence>("Sequence");
    for (auto id : ids) {
        if (auto* anim = get_animation(id)) {
            // Would need to clone animations
        }
    }
    // seq->play();
}

void HudAnimator::play_group(const std::vector<HudAnimationId>& ids, IHudElement* target) {
    auto group = std::make_unique<HudAnimationGroup>("Group");
    for (auto id : ids) {
        if (auto* anim = get_animation(id)) {
            // Would need to clone animations
        }
    }
    // group->play();
}

void HudAnimator::fade_in(IHudElement* target, float duration) {
    transition(target, AnimProperty::Opacity, 1.0f, duration, EasingType::EaseOut);
    target->set_opacity(0);
}

void HudAnimator::fade_out(IHudElement* target, float duration) {
    transition(target, AnimProperty::Opacity, 0.0f, duration, EasingType::EaseIn);
}

void HudAnimator::slide_in(IHudElement* target, AnchorPoint from, float duration) {
    // Implementation depends on screen size
}

void HudAnimator::slide_out(IHudElement* target, AnchorPoint to, float duration) {
    // Implementation depends on screen size
}

void HudAnimator::scale_in(IHudElement* target, float duration) {
    target->set_scale({0, 0});
    transition(target, AnimProperty::Scale, 1.0f, duration, EasingType::EaseOutElastic);
}

void HudAnimator::scale_out(IHudElement* target, float duration) {
    transition(target, AnimProperty::Scale, 0.0f, duration, EasingType::EaseIn);
}

void HudAnimator::pulse(IHudElement* target, float scale, float duration) {
    auto anim = presets::pulse(scale, duration);
    auto id = register_animation(std::move(anim));
    play(id, target);
}

void HudAnimator::shake(IHudElement* target, float intensity, float duration) {
    // Would need special shake animation
}

void HudAnimator::bounce(IHudElement* target, float height, float duration) {
    auto anim = presets::bounce(height, duration);
    auto id = register_animation(std::move(anim));
    play(id, target);
}

void HudAnimator::transition(IHudElement* target, AnimProperty property,
                              float to_value, float duration, EasingType easing) {
    TransitionDef def{property, duration, easing};
    HudTransition trans(def);

    float from_value = 0;
    switch (property) {
        case AnimProperty::Opacity:
            from_value = target->opacity();
            break;
        case AnimProperty::Scale:
            from_value = target->scale().x;
            break;
        default:
            break;
    }

    trans.start(target, from_value, to_value);
    m_transitions.push_back(trans);
}

void HudAnimator::transition_color(IHudElement* target, const Color& to_color,
                                    float duration, EasingType easing) {
    TransitionDef def{AnimProperty::Color, duration, easing};
    HudTransition trans(def);
    trans.start(target, target->properties().color, to_color);
    m_transitions.push_back(trans);
}

void HudAnimator::stop(IHudElement* target) {
    m_active.erase(
        std::remove_if(m_active.begin(), m_active.end(),
                       [target](const ActiveAnimation& a) { return a.target == target; }),
        m_active.end());
}

void HudAnimator::stop(HudAnimationId id) {
    auto* anim = get_animation(id);
    if (anim) {
        anim->stop();
    }
    m_active.erase(
        std::remove_if(m_active.begin(), m_active.end(),
                       [&anim](const ActiveAnimation& a) { return a.animation == anim; }),
        m_active.end());
}

void HudAnimator::stop_all() {
    for (auto& [id, anim] : m_animations) {
        if (anim) {
            anim->stop();
        }
    }
    m_active.clear();
    m_transitions.clear();
}

void HudAnimator::pause(IHudElement* target) {
    for (auto& active : m_active) {
        if (active.target == target && active.animation) {
            active.animation->pause();
        }
    }
}

void HudAnimator::resume(IHudElement* target) {
    for (auto& active : m_active) {
        if (active.target == target && active.animation) {
            active.animation->play();
        }
    }
}

void HudAnimator::pause_all() {
    for (auto& active : m_active) {
        if (active.animation) {
            active.animation->pause();
        }
    }
}

void HudAnimator::resume_all() {
    for (auto& active : m_active) {
        if (active.animation) {
            active.animation->play();
        }
    }
}

void HudAnimator::update(float delta_time) {
    // Update active animations
    for (auto it = m_active.begin(); it != m_active.end();) {
        if (it->animation) {
            it->animation->update(delta_time);
            it->animation->apply(it->target);

            if (it->animation->state() == AnimationState::Finished) {
                it = m_active.erase(it);
            } else {
                ++it;
            }
        } else {
            it = m_active.erase(it);
        }
    }

    // Update transitions
    for (auto it = m_transitions.begin(); it != m_transitions.end();) {
        it->update(delta_time);
        if (it->is_complete()) {
            it = m_transitions.erase(it);
        } else {
            ++it;
        }
    }
}

bool HudAnimator::is_animating(IHudElement* target) const {
    for (const auto& active : m_active) {
        if (active.target == target) {
            return true;
        }
    }
    return false;
}

std::vector<HudAnimationId> HudAnimator::get_active_animations(IHudElement* target) const {
    std::vector<HudAnimationId> result;
    for (const auto& active : m_active) {
        if (active.target == target && active.animation) {
            result.push_back(active.animation->id());
        }
    }
    return result;
}

// =============================================================================
// AnimationBuilder
// =============================================================================

AnimationBuilder::AnimationBuilder() {
    m_def.name = "Animation";
    m_def.duration = 1.0f;
}

AnimationBuilder::AnimationBuilder(const std::string& name) {
    m_def.name = name;
    m_def.duration = 1.0f;
}

AnimationBuilder& AnimationBuilder::property(AnimProperty prop) {
    m_def.property = prop;
    return *this;
}

AnimationBuilder& AnimationBuilder::duration(float dur) {
    m_def.duration = dur;
    return *this;
}

AnimationBuilder& AnimationBuilder::delay(float del) {
    m_def.delay = del;
    return *this;
}

AnimationBuilder& AnimationBuilder::play_mode(PlayMode mode) {
    m_def.play_mode = mode;
    return *this;
}

AnimationBuilder& AnimationBuilder::repeat(std::uint32_t count) {
    m_def.repeat_count = count;
    return *this;
}

AnimationBuilder& AnimationBuilder::loop() {
    m_def.repeat_count = 0;
    m_def.play_mode = PlayMode::Loop;
    return *this;
}

AnimationBuilder& AnimationBuilder::from(float value) {
    m_from_value = value;
    m_has_from = true;
    return *this;
}

AnimationBuilder& AnimationBuilder::to(float value) {
    m_to_value = value;
    m_has_to = true;
    return *this;
}

AnimationBuilder& AnimationBuilder::keyframe(float time, float value, EasingType easing) {
    m_def.keyframes.push_back({time, value, easing});
    return *this;
}

AnimationBuilder& AnimationBuilder::easing(EasingType type) {
    if (!m_def.keyframes.empty()) {
        m_def.keyframes[0].easing = type;
    }
    return *this;
}

AnimationBuilder& AnimationBuilder::ease_in() {
    return easing(EasingType::EaseIn);
}

AnimationBuilder& AnimationBuilder::ease_out() {
    return easing(EasingType::EaseOut);
}

AnimationBuilder& AnimationBuilder::ease_in_out() {
    return easing(EasingType::EaseInOut);
}

AnimationBuilder& AnimationBuilder::on_start(AnimationCallback callback) {
    m_on_start = std::move(callback);
    return *this;
}

AnimationBuilder& AnimationBuilder::on_complete(AnimationCallback callback) {
    m_on_complete = std::move(callback);
    return *this;
}

AnimationBuilder& AnimationBuilder::on_loop(AnimationCallback callback) {
    m_on_loop = std::move(callback);
    return *this;
}

std::unique_ptr<PropertyAnimation> AnimationBuilder::build() {
    // Add from/to as keyframes if specified
    if (m_has_from && m_has_to && m_def.keyframes.empty()) {
        m_def.keyframes.push_back({0, m_from_value, EasingType::Linear});
        m_def.keyframes.push_back({1.0f, m_to_value, EasingType::Linear});
    }

    auto anim = std::make_unique<PropertyAnimation>(m_def);

    if (m_on_start) anim->set_on_start(m_on_start);
    if (m_on_complete) anim->set_on_complete(m_on_complete);
    if (m_on_loop) anim->set_on_loop(m_on_loop);

    return anim;
}

AnimationDef AnimationBuilder::build_def() {
    if (m_has_from && m_has_to && m_def.keyframes.empty()) {
        m_def.keyframes.push_back({0, m_from_value, EasingType::Linear});
        m_def.keyframes.push_back({1.0f, m_to_value, EasingType::Linear});
    }
    return m_def;
}

} // namespace void_hud
