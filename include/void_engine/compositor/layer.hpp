#pragma once

/// @file layer.hpp
/// @brief Layer compositing system
///
/// Provides priority-based layer composition with blend modes:
/// - Normal: Standard alpha blending
/// - Additive: Add colors together (good for glow effects)
/// - Multiply: Multiply colors (good for shadows)
/// - Screen: Inverse multiply (good for highlights)
/// - Replace: No blending, complete replacement
///
/// Layers are composited from lowest priority to highest, with
/// higher priority layers rendered on top.

#include "fwd.hpp"
#include "types.hpp"
#include "rehydration.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_compositor {

// =============================================================================
// Layer ID
// =============================================================================

/// Unique layer identifier
struct LayerId {
    std::uint64_t id = 0;

    [[nodiscard]] bool operator==(const LayerId& other) const { return id == other.id; }
    [[nodiscard]] bool operator!=(const LayerId& other) const { return id != other.id; }
    [[nodiscard]] bool operator<(const LayerId& other) const { return id < other.id; }

    [[nodiscard]] bool is_valid() const { return id != 0; }

    [[nodiscard]] static LayerId invalid() { return LayerId{0}; }
};

} // namespace void_compositor

// Hash for LayerId
template<>
struct std::hash<void_compositor::LayerId> {
    std::size_t operator()(const void_compositor::LayerId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.id);
    }
};

namespace void_compositor {

// =============================================================================
// Blend Mode
// =============================================================================

/// Blend mode for layer compositing
enum class BlendMode : std::uint8_t {
    /// Standard alpha blending: result = src * alpha + dst * (1 - alpha)
    Normal,
    /// Additive blending: result = src + dst
    Additive,
    /// Multiplicative blending: result = src * dst
    Multiply,
    /// Screen blending: result = 1 - (1 - src) * (1 - dst)
    Screen,
    /// Replace: result = src (no blending)
    Replace,
    /// Overlay: combination of multiply and screen
    Overlay,
    /// Soft light: softer version of overlay
    SoftLight,
    /// Hard light: hard version of overlay
    HardLight,
    /// Difference: result = abs(src - dst)
    Difference,
    /// Exclusion: result = src + dst - 2 * src * dst
    Exclusion,
};

/// Get blend mode name
[[nodiscard]] inline const char* to_string(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal: return "Normal";
        case BlendMode::Additive: return "Additive";
        case BlendMode::Multiply: return "Multiply";
        case BlendMode::Screen: return "Screen";
        case BlendMode::Replace: return "Replace";
        case BlendMode::Overlay: return "Overlay";
        case BlendMode::SoftLight: return "SoftLight";
        case BlendMode::HardLight: return "HardLight";
        case BlendMode::Difference: return "Difference";
        case BlendMode::Exclusion: return "Exclusion";
    }
    return "Unknown";
}

// =============================================================================
// Layer Config
// =============================================================================

/// Layer configuration
struct LayerConfig {
    /// Layer name (for debugging)
    std::string name;
    /// Priority (lower = rendered first, higher = on top)
    std::int32_t priority = 0;
    /// Blend mode
    BlendMode blend_mode = BlendMode::Normal;
    /// Opacity (0.0 = transparent, 1.0 = opaque)
    float opacity = 1.0f;
    /// Is layer visible?
    bool visible = true;
    /// Clip to parent bounds?
    bool clip_to_bounds = false;
    /// Mask layer ID (optional)
    std::optional<LayerId> mask_layer;

    /// Create a default layer config
    static LayerConfig create(const std::string& layer_name, std::int32_t layer_priority = 0) {
        return LayerConfig{
            .name = layer_name,
            .priority = layer_priority,
        };
    }

    /// Builder pattern: set priority
    [[nodiscard]] LayerConfig& with_priority(std::int32_t p) {
        priority = p;
        return *this;
    }

    /// Builder pattern: set blend mode
    [[nodiscard]] LayerConfig& with_blend_mode(BlendMode mode) {
        blend_mode = mode;
        return *this;
    }

    /// Builder pattern: set opacity
    [[nodiscard]] LayerConfig& with_opacity(float o) {
        opacity = std::clamp(o, 0.0f, 1.0f);
        return *this;
    }

    /// Builder pattern: set visibility
    [[nodiscard]] LayerConfig& with_visible(bool v) {
        visible = v;
        return *this;
    }

    /// Builder pattern: set clip to bounds
    [[nodiscard]] LayerConfig& with_clip_to_bounds(bool clip) {
        clip_to_bounds = clip;
        return *this;
    }
};

// =============================================================================
// Layer Bounds
// =============================================================================

/// Layer bounds (position and size)
struct LayerBounds {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    /// Create bounds
    static LayerBounds create(float x, float y, float w, float h) {
        return LayerBounds{x, y, w, h};
    }

    /// Create bounds from size (position at origin)
    static LayerBounds from_size(float w, float h) {
        return LayerBounds{0.0f, 0.0f, w, h};
    }

    /// Check if point is inside bounds
    [[nodiscard]] bool contains(float px, float py) const {
        return px >= x && px < x + width &&
               py >= y && py < y + height;
    }

    /// Check if bounds intersect
    [[nodiscard]] bool intersects(const LayerBounds& other) const {
        return x < other.x + other.width &&
               x + width > other.x &&
               y < other.y + other.height &&
               y + height > other.y;
    }

    /// Get intersection with another bounds
    [[nodiscard]] std::optional<LayerBounds> intersection(const LayerBounds& other) const {
        float ix = std::max(x, other.x);
        float iy = std::max(y, other.y);
        float iw = std::min(x + width, other.x + other.width) - ix;
        float ih = std::min(y + height, other.y + other.height) - iy;

        if (iw > 0 && ih > 0) {
            return LayerBounds{ix, iy, iw, ih};
        }
        return std::nullopt;
    }

    /// Get union with another bounds
    [[nodiscard]] LayerBounds union_with(const LayerBounds& other) const {
        float ux = std::min(x, other.x);
        float uy = std::min(y, other.y);
        float uw = std::max(x + width, other.x + other.width) - ux;
        float uh = std::max(y + height, other.y + other.height) - uy;
        return LayerBounds{ux, uy, uw, uh};
    }

    /// Get area
    [[nodiscard]] float area() const {
        return width * height;
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const {
        return width <= 0 || height <= 0;
    }
};

// =============================================================================
// Layer Transform
// =============================================================================

/// Layer transform (2D affine transform)
struct LayerTransform {
    /// Translation
    float translate_x = 0.0f;
    float translate_y = 0.0f;

    /// Scale
    float scale_x = 1.0f;
    float scale_y = 1.0f;

    /// Rotation (radians)
    float rotation = 0.0f;

    /// Anchor point (normalized, 0-1)
    float anchor_x = 0.0f;
    float anchor_y = 0.0f;

    /// Create identity transform
    static LayerTransform identity() {
        return LayerTransform{};
    }

    /// Check if identity
    [[nodiscard]] bool is_identity() const {
        return translate_x == 0.0f && translate_y == 0.0f &&
               scale_x == 1.0f && scale_y == 1.0f &&
               rotation == 0.0f;
    }

    /// Builder pattern: set translation
    [[nodiscard]] LayerTransform& with_translation(float x, float y) {
        translate_x = x;
        translate_y = y;
        return *this;
    }

    /// Builder pattern: set scale
    [[nodiscard]] LayerTransform& with_scale(float x, float y) {
        scale_x = x;
        scale_y = y;
        return *this;
    }

    /// Builder pattern: set uniform scale
    [[nodiscard]] LayerTransform& with_uniform_scale(float s) {
        scale_x = s;
        scale_y = s;
        return *this;
    }

    /// Builder pattern: set rotation
    [[nodiscard]] LayerTransform& with_rotation(float r) {
        rotation = r;
        return *this;
    }

    /// Builder pattern: set anchor point
    [[nodiscard]] LayerTransform& with_anchor(float x, float y) {
        anchor_x = x;
        anchor_y = y;
        return *this;
    }
};

// =============================================================================
// Layer Content
// =============================================================================

/// Layer content type
enum class LayerContentType : std::uint8_t {
    /// No content (container layer)
    Empty,
    /// Solid color fill
    SolidColor,
    /// Texture/image reference
    Texture,
    /// Render target (dynamic content)
    RenderTarget,
    /// Sub-compositor (nested layers)
    SubCompositor,
};

/// Layer content
struct LayerContent {
    LayerContentType type = LayerContentType::Empty;

    /// Color (for SolidColor type)
    struct Color {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    } color;

    /// Texture handle (platform-specific)
    void* texture_handle = nullptr;
    std::uint32_t texture_width = 0;
    std::uint32_t texture_height = 0;

    /// Texture UV coordinates
    float uv_min_x = 0.0f;
    float uv_min_y = 0.0f;
    float uv_max_x = 1.0f;
    float uv_max_y = 1.0f;

    /// Create empty content
    static LayerContent empty() {
        return LayerContent{.type = LayerContentType::Empty};
    }

    /// Create solid color content
    static LayerContent solid_color(float r, float g, float b, float a = 1.0f) {
        LayerContent content;
        content.type = LayerContentType::SolidColor;
        content.color = {r, g, b, a};
        return content;
    }

    /// Create texture content
    static LayerContent texture(void* handle, std::uint32_t w, std::uint32_t h) {
        LayerContent content;
        content.type = LayerContentType::Texture;
        content.texture_handle = handle;
        content.texture_width = w;
        content.texture_height = h;
        return content;
    }

    /// Create render target content
    static LayerContent render_target(void* handle, std::uint32_t w, std::uint32_t h) {
        LayerContent content;
        content.type = LayerContentType::RenderTarget;
        content.texture_handle = handle;
        content.texture_width = w;
        content.texture_height = h;
        return content;
    }
};

// =============================================================================
// Layer
// =============================================================================

/// A compositing layer
class Layer {
public:
    Layer(LayerId id, const LayerConfig& config)
        : m_id(id)
        , m_config(config)
    {}

    /// Get layer ID
    [[nodiscard]] LayerId id() const { return m_id; }

    /// Get configuration
    [[nodiscard]] const LayerConfig& config() const { return m_config; }

    /// Get mutable configuration
    [[nodiscard]] LayerConfig& config() { return m_config; }

    /// Get bounds
    [[nodiscard]] const LayerBounds& bounds() const { return m_bounds; }

    /// Set bounds
    void set_bounds(const LayerBounds& bounds) { m_bounds = bounds; }

    /// Get transform
    [[nodiscard]] const LayerTransform& transform() const { return m_transform; }

    /// Set transform
    void set_transform(const LayerTransform& transform) { m_transform = transform; }

    /// Get content
    [[nodiscard]] const LayerContent& content() const { return m_content; }

    /// Set content
    void set_content(const LayerContent& content) { m_content = content; }

    /// Check if visible (considers opacity and visibility flag)
    [[nodiscard]] bool is_visible() const {
        return m_config.visible && m_config.opacity > 0.0f;
    }

    /// Check if layer needs compositing (has non-trivial blend mode)
    [[nodiscard]] bool needs_compositing() const {
        return m_config.blend_mode != BlendMode::Normal ||
               m_config.opacity < 1.0f ||
               m_config.mask_layer.has_value();
    }

    /// Get parent layer ID
    [[nodiscard]] std::optional<LayerId> parent() const { return m_parent; }

    /// Set parent layer
    void set_parent(std::optional<LayerId> parent) { m_parent = parent; }

    /// Get child layer IDs
    [[nodiscard]] const std::vector<LayerId>& children() const { return m_children; }

    /// Add child layer
    void add_child(LayerId child) {
        m_children.push_back(child);
    }

    /// Remove child layer
    bool remove_child(LayerId child) {
        auto it = std::find(m_children.begin(), m_children.end(), child);
        if (it != m_children.end()) {
            m_children.erase(it);
            return true;
        }
        return false;
    }

    /// Clear children
    void clear_children() {
        m_children.clear();
    }

    /// Check if dirty (needs re-render)
    [[nodiscard]] bool is_dirty() const { return m_dirty; }

    /// Mark as dirty
    void mark_dirty() { m_dirty = true; }

    /// Clear dirty flag
    void clear_dirty() { m_dirty = false; }

private:
    LayerId m_id;
    LayerConfig m_config;
    LayerBounds m_bounds;
    LayerTransform m_transform;
    LayerContent m_content;
    std::optional<LayerId> m_parent;
    std::vector<LayerId> m_children;
    bool m_dirty = true;
};

// =============================================================================
// Layer Manager
// =============================================================================

/// Manages all layers and their hierarchy
class LayerManager : public IRehydratable {
public:
    LayerManager() = default;

    // =========================================================================
    // Layer Creation
    // =========================================================================

    /// Create a new layer
    [[nodiscard]] LayerId create_layer(const LayerConfig& config) {
        std::unique_lock lock(m_mutex);
        LayerId id{++m_next_id};
        m_layers.emplace(id, std::make_unique<Layer>(id, config));
        m_sorted_dirty = true;
        return id;
    }

    /// Create a child layer
    [[nodiscard]] LayerId create_child_layer(LayerId parent, const LayerConfig& config) {
        std::unique_lock lock(m_mutex);

        auto parent_it = m_layers.find(parent);
        if (parent_it == m_layers.end()) {
            return LayerId::invalid();
        }

        LayerId id{++m_next_id};
        auto layer = std::make_unique<Layer>(id, config);
        layer->set_parent(parent);
        parent_it->second->add_child(id);
        m_layers.emplace(id, std::move(layer));
        m_sorted_dirty = true;
        return id;
    }

    /// Destroy a layer
    bool destroy_layer(LayerId id) {
        std::unique_lock lock(m_mutex);

        auto it = m_layers.find(id);
        if (it == m_layers.end()) {
            return false;
        }

        // Remove from parent
        if (auto parent = it->second->parent()) {
            if (auto parent_it = m_layers.find(*parent); parent_it != m_layers.end()) {
                parent_it->second->remove_child(id);
            }
        }

        // Orphan children (they become root layers)
        for (LayerId child : it->second->children()) {
            if (auto child_it = m_layers.find(child); child_it != m_layers.end()) {
                child_it->second->set_parent(std::nullopt);
            }
        }

        m_layers.erase(it);
        m_sorted_dirty = true;
        return true;
    }

    // =========================================================================
    // Layer Access
    // =========================================================================

    /// Get layer by ID
    [[nodiscard]] Layer* get(LayerId id) {
        std::shared_lock lock(m_mutex);
        auto it = m_layers.find(id);
        return it != m_layers.end() ? it->second.get() : nullptr;
    }

    /// Get layer by ID (const)
    [[nodiscard]] const Layer* get(LayerId id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_layers.find(id);
        return it != m_layers.end() ? it->second.get() : nullptr;
    }

    /// Get layer by name
    [[nodiscard]] Layer* find_by_name(const std::string& name) {
        std::shared_lock lock(m_mutex);
        for (auto& [_, layer] : m_layers) {
            if (layer->config().name == name) {
                return layer.get();
            }
        }
        return nullptr;
    }

    /// Get all layers sorted by priority
    [[nodiscard]] std::vector<Layer*> get_sorted_layers() {
        std::unique_lock lock(m_mutex);
        update_sorted_list();
        return m_sorted_layers;
    }

    /// Get root layers (no parent)
    [[nodiscard]] std::vector<Layer*> get_root_layers() {
        std::shared_lock lock(m_mutex);
        std::vector<Layer*> roots;
        for (auto& [_, layer] : m_layers) {
            if (!layer->parent()) {
                roots.push_back(layer.get());
            }
        }
        std::sort(roots.begin(), roots.end(), [](Layer* a, Layer* b) {
            return a->config().priority < b->config().priority;
        });
        return roots;
    }

    /// Get layer count
    [[nodiscard]] std::size_t count() const {
        std::shared_lock lock(m_mutex);
        return m_layers.size();
    }

    /// Check if layer exists
    [[nodiscard]] bool exists(LayerId id) const {
        std::shared_lock lock(m_mutex);
        return m_layers.count(id) > 0;
    }

    // =========================================================================
    // Layer Modification
    // =========================================================================

    /// Set layer priority
    void set_priority(LayerId id, std::int32_t priority) {
        std::unique_lock lock(m_mutex);
        if (auto it = m_layers.find(id); it != m_layers.end()) {
            it->second->config().priority = priority;
            m_sorted_dirty = true;
        }
    }

    /// Set layer visibility
    void set_visible(LayerId id, bool visible) {
        std::shared_lock lock(m_mutex);
        if (auto it = m_layers.find(id); it != m_layers.end()) {
            it->second->config().visible = visible;
            it->second->mark_dirty();
        }
    }

    /// Set layer opacity
    void set_opacity(LayerId id, float opacity) {
        std::shared_lock lock(m_mutex);
        if (auto it = m_layers.find(id); it != m_layers.end()) {
            it->second->config().opacity = std::clamp(opacity, 0.0f, 1.0f);
            it->second->mark_dirty();
        }
    }

    /// Set layer blend mode
    void set_blend_mode(LayerId id, BlendMode mode) {
        std::shared_lock lock(m_mutex);
        if (auto it = m_layers.find(id); it != m_layers.end()) {
            it->second->config().blend_mode = mode;
            it->second->mark_dirty();
        }
    }

    /// Set layer bounds
    void set_bounds(LayerId id, const LayerBounds& bounds) {
        std::shared_lock lock(m_mutex);
        if (auto it = m_layers.find(id); it != m_layers.end()) {
            it->second->set_bounds(bounds);
            it->second->mark_dirty();
        }
    }

    /// Set layer transform
    void set_transform(LayerId id, const LayerTransform& transform) {
        std::shared_lock lock(m_mutex);
        if (auto it = m_layers.find(id); it != m_layers.end()) {
            it->second->set_transform(transform);
            it->second->mark_dirty();
        }
    }

    /// Set layer content
    void set_content(LayerId id, const LayerContent& content) {
        std::shared_lock lock(m_mutex);
        if (auto it = m_layers.find(id); it != m_layers.end()) {
            it->second->set_content(content);
            it->second->mark_dirty();
        }
    }

    // =========================================================================
    // Hierarchy
    // =========================================================================

    /// Move layer to new parent
    bool reparent(LayerId layer, std::optional<LayerId> new_parent) {
        std::unique_lock lock(m_mutex);

        auto layer_it = m_layers.find(layer);
        if (layer_it == m_layers.end()) {
            return false;
        }

        // Validate new parent
        if (new_parent) {
            if (m_layers.find(*new_parent) == m_layers.end()) {
                return false;
            }
            // Prevent cycles
            if (*new_parent == layer) {
                return false;
            }
        }

        // Remove from current parent
        if (auto current_parent = layer_it->second->parent()) {
            if (auto parent_it = m_layers.find(*current_parent); parent_it != m_layers.end()) {
                parent_it->second->remove_child(layer);
            }
        }

        // Add to new parent
        layer_it->second->set_parent(new_parent);
        if (new_parent) {
            if (auto parent_it = m_layers.find(*new_parent); parent_it != m_layers.end()) {
                parent_it->second->add_child(layer);
            }
        }

        m_sorted_dirty = true;
        return true;
    }

    /// Move layer to front (highest priority)
    void bring_to_front(LayerId id) {
        std::unique_lock lock(m_mutex);
        std::int32_t max_priority = 0;
        for (auto& [_, layer] : m_layers) {
            max_priority = std::max(max_priority, layer->config().priority);
        }
        if (auto it = m_layers.find(id); it != m_layers.end()) {
            it->second->config().priority = max_priority + 1;
            m_sorted_dirty = true;
        }
    }

    /// Move layer to back (lowest priority)
    void send_to_back(LayerId id) {
        std::unique_lock lock(m_mutex);
        std::int32_t min_priority = 0;
        for (auto& [_, layer] : m_layers) {
            min_priority = std::min(min_priority, layer->config().priority);
        }
        if (auto it = m_layers.find(id); it != m_layers.end()) {
            it->second->config().priority = min_priority - 1;
            m_sorted_dirty = true;
        }
    }

    // =========================================================================
    // Dirty Tracking
    // =========================================================================

    /// Check if any layer is dirty
    [[nodiscard]] bool has_dirty_layers() const {
        std::shared_lock lock(m_mutex);
        for (const auto& [_, layer] : m_layers) {
            if (layer->is_dirty()) {
                return true;
            }
        }
        return false;
    }

    /// Get dirty layers
    [[nodiscard]] std::vector<Layer*> get_dirty_layers() {
        std::shared_lock lock(m_mutex);
        std::vector<Layer*> dirty;
        for (auto& [_, layer] : m_layers) {
            if (layer->is_dirty()) {
                dirty.push_back(layer.get());
            }
        }
        return dirty;
    }

    /// Clear all dirty flags
    void clear_all_dirty() {
        std::shared_lock lock(m_mutex);
        for (auto& [_, layer] : m_layers) {
            layer->clear_dirty();
        }
    }

    /// Mark all layers dirty
    void mark_all_dirty() {
        std::shared_lock lock(m_mutex);
        for (auto& [_, layer] : m_layers) {
            layer->mark_dirty();
        }
    }

    // =========================================================================
    // Iteration
    // =========================================================================

    /// Iterate over all layers
    template<typename Fn>
    void for_each(Fn&& fn) {
        std::shared_lock lock(m_mutex);
        for (auto& [_, layer] : m_layers) {
            fn(*layer);
        }
    }

    /// Iterate over all layers (const)
    template<typename Fn>
    void for_each(Fn&& fn) const {
        std::shared_lock lock(m_mutex);
        for (const auto& [_, layer] : m_layers) {
            fn(*layer);
        }
    }

    // =========================================================================
    // Hot-Reload Support
    // =========================================================================

    /// Dehydrate for hot-reload
    [[nodiscard]] RehydrationState dehydrate() const override {
        std::shared_lock lock(m_mutex);
        RehydrationState state;

        state.set_uint("layer_count", m_layers.size());
        state.set_uint("next_id", m_next_id);

        // Serialize each layer
        std::size_t idx = 0;
        for (const auto& [id, layer] : m_layers) {
            RehydrationState layer_state;
            layer_state.set_uint("id", id.id);
            layer_state.set_string("name", layer->config().name);
            layer_state.set_int("priority", layer->config().priority);
            layer_state.set_int("blend_mode", static_cast<std::int64_t>(layer->config().blend_mode));
            layer_state.set_float("opacity", layer->config().opacity);
            layer_state.set_bool("visible", layer->config().visible);

            // Bounds
            layer_state.set_float("bounds_x", layer->bounds().x);
            layer_state.set_float("bounds_y", layer->bounds().y);
            layer_state.set_float("bounds_w", layer->bounds().width);
            layer_state.set_float("bounds_h", layer->bounds().height);

            // Parent
            if (layer->parent()) {
                layer_state.set_uint("parent_id", layer->parent()->id);
            }

            state.set_nested("layer_" + std::to_string(idx++), std::move(layer_state));
        }

        return state;
    }

    /// Rehydrate from hot-reload state
    bool rehydrate(const RehydrationState& state) override {
        std::unique_lock lock(m_mutex);

        auto layer_count = state.get_uint("layer_count");
        auto next_id = state.get_uint("next_id");

        if (!layer_count || !next_id) {
            return false;
        }

        m_layers.clear();
        m_next_id = *next_id;

        // First pass: create all layers
        for (std::size_t i = 0; i < *layer_count; ++i) {
            auto layer_state = state.get_nested("layer_" + std::to_string(i));
            if (!layer_state) continue;

            auto id = layer_state->get_uint("id");
            auto name = layer_state->get_string("name");
            auto priority = layer_state->get_int("priority");
            auto blend_mode = layer_state->get_int("blend_mode");
            auto opacity = layer_state->get_float("opacity");
            auto visible = layer_state->get_bool("visible");

            if (!id || !name || !priority || !blend_mode || !opacity || !visible) {
                continue;
            }

            LayerConfig config;
            config.name = *name;
            config.priority = static_cast<std::int32_t>(*priority);
            config.blend_mode = static_cast<BlendMode>(*blend_mode);
            config.opacity = static_cast<float>(*opacity);
            config.visible = *visible;

            LayerId layer_id{*id};
            auto layer = std::make_unique<Layer>(layer_id, config);

            // Bounds
            if (auto bx = layer_state->get_float("bounds_x")) {
                LayerBounds bounds;
                bounds.x = static_cast<float>(*bx);
                if (auto by = layer_state->get_float("bounds_y")) bounds.y = static_cast<float>(*by);
                if (auto bw = layer_state->get_float("bounds_w")) bounds.width = static_cast<float>(*bw);
                if (auto bh = layer_state->get_float("bounds_h")) bounds.height = static_cast<float>(*bh);
                layer->set_bounds(bounds);
            }

            m_layers.emplace(layer_id, std::move(layer));
        }

        // Second pass: restore parent relationships
        for (std::size_t i = 0; i < *layer_count; ++i) {
            auto layer_state = state.get_nested("layer_" + std::to_string(i));
            if (!layer_state) continue;

            auto id = layer_state->get_uint("id");
            auto parent_id = layer_state->get_uint("parent_id");

            if (id && parent_id) {
                LayerId layer_id{*id};
                LayerId parent_layer_id{*parent_id};

                if (auto it = m_layers.find(layer_id); it != m_layers.end()) {
                    it->second->set_parent(parent_layer_id);
                    if (auto parent_it = m_layers.find(parent_layer_id); parent_it != m_layers.end()) {
                        parent_it->second->add_child(layer_id);
                    }
                }
            }
        }

        m_sorted_dirty = true;
        return true;
    }

    /// Clear all layers
    void clear() {
        std::unique_lock lock(m_mutex);
        m_layers.clear();
        m_sorted_layers.clear();
        m_sorted_dirty = true;
    }

private:
    /// Update the sorted layer list
    void update_sorted_list() {
        if (!m_sorted_dirty) {
            return;
        }

        m_sorted_layers.clear();
        m_sorted_layers.reserve(m_layers.size());

        for (auto& [_, layer] : m_layers) {
            m_sorted_layers.push_back(layer.get());
        }

        std::sort(m_sorted_layers.begin(), m_sorted_layers.end(), [](Layer* a, Layer* b) {
            return a->config().priority < b->config().priority;
        });

        m_sorted_dirty = false;
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<LayerId, std::unique_ptr<Layer>> m_layers;
    std::vector<Layer*> m_sorted_layers;
    std::uint64_t m_next_id = 0;
    bool m_sorted_dirty = true;
};

} // namespace void_compositor
