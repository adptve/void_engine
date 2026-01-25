#pragma once

/// @file compositor.hpp
/// @brief Compositor and layer system for void_render

#include "fwd.hpp"
#include "pass.hpp"
#include "camera.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <algorithm>

namespace void_render {

// =============================================================================
// LayerId
// =============================================================================

/// Layer identifier (bitmask-compatible)
struct LayerId {
    std::uint32_t value = 0;

    constexpr LayerId() noexcept = default;
    constexpr explicit LayerId(std::uint32_t v) noexcept : value(v) {}

    /// Get layer bit (0-31)
    [[nodiscard]] static constexpr LayerId from_bit(std::uint32_t bit) noexcept {
        return LayerId(1u << bit);
    }

    /// Check if this layer matches a mask
    [[nodiscard]] constexpr bool matches(std::uint32_t mask) const noexcept {
        return (value & mask) != 0;
    }

    /// Combine layers
    [[nodiscard]] constexpr LayerId operator|(LayerId other) const noexcept {
        return LayerId(value | other.value);
    }

    constexpr LayerId& operator|=(LayerId other) noexcept {
        value |= other.value;
        return *this;
    }

    constexpr bool operator==(const LayerId& other) const noexcept = default;
};

/// Predefined layers
namespace layers {
    constexpr LayerId Default      = LayerId::from_bit(0);   // Default render layer
    constexpr LayerId Transparent  = LayerId::from_bit(1);   // Transparent objects
    constexpr LayerId UI           = LayerId::from_bit(2);   // UI elements
    constexpr LayerId Debug        = LayerId::from_bit(3);   // Debug visualization
    constexpr LayerId PostProcess  = LayerId::from_bit(4);   // Post-process only
    constexpr LayerId Shadow       = LayerId::from_bit(5);   // Shadow casters
    constexpr LayerId Reflection   = LayerId::from_bit(6);   // Reflection capture
    constexpr LayerId Decal        = LayerId::from_bit(7);   // Decals
    constexpr LayerId Particle     = LayerId::from_bit(8);   // Particles
    constexpr LayerId Sky          = LayerId::from_bit(9);   // Skybox/atmosphere
    constexpr LayerId Terrain      = LayerId::from_bit(10);  // Terrain
    constexpr LayerId Water        = LayerId::from_bit(11);  // Water surfaces
    constexpr LayerId Foliage      = LayerId::from_bit(12);  // Grass, trees
    constexpr LayerId Character    = LayerId::from_bit(13);  // Characters/NPCs
    constexpr LayerId Prop         = LayerId::from_bit(14);  // Props/items
    constexpr LayerId Effect       = LayerId::from_bit(15);  // Visual effects

    constexpr std::uint32_t All          = 0xFFFFFFFF;
    constexpr std::uint32_t Opaque       = Default.value | Terrain.value | Character.value | Prop.value;
    constexpr std::uint32_t ShadowCasters = Default.value | Terrain.value | Character.value | Prop.value | Foliage.value;
}

// =============================================================================
// LayerFlags
// =============================================================================

/// Layer behavior flags
enum class LayerFlags : std::uint32_t {
    None            = 0,
    Visible         = 1 << 0,   // Layer is rendered
    CastsShadows    = 1 << 1,   // Objects cast shadows
    ReceivesShadows = 1 << 2,   // Objects receive shadows
    Reflective      = 1 << 3,   // Included in reflections
    Pickable        = 1 << 4,   // Included in picking/raycasts
    DepthWrite      = 1 << 5,   // Writes to depth buffer
    DepthTest       = 1 << 6,   // Tests against depth buffer
    Instanced       = 1 << 7,   // Uses GPU instancing
    Static          = 1 << 8,   // Static objects (BVH optimized)
    Dynamic         = 1 << 9,   // Dynamic objects
    Culled          = 1 << 10,  // Frustum culled
};

[[nodiscard]] constexpr LayerFlags operator|(LayerFlags a, LayerFlags b) noexcept {
    return static_cast<LayerFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

[[nodiscard]] constexpr LayerFlags operator&(LayerFlags a, LayerFlags b) noexcept {
    return static_cast<LayerFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

[[nodiscard]] constexpr bool has_flag(LayerFlags flags, LayerFlags flag) noexcept {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0;
}

/// Default layer flags
namespace layer_flags {
    constexpr LayerFlags Default = LayerFlags::Visible | LayerFlags::CastsShadows |
                                   LayerFlags::ReceivesShadows | LayerFlags::Pickable |
                                   LayerFlags::DepthWrite | LayerFlags::DepthTest |
                                   LayerFlags::Culled;

    constexpr LayerFlags Transparent = LayerFlags::Visible | LayerFlags::ReceivesShadows |
                                       LayerFlags::Pickable | LayerFlags::DepthTest |
                                       LayerFlags::Culled;

    constexpr LayerFlags UI = LayerFlags::Visible | LayerFlags::Pickable;

    constexpr LayerFlags Debug = LayerFlags::Visible;

    constexpr LayerFlags StaticGeometry = Default | LayerFlags::Static;

    constexpr LayerFlags DynamicGeometry = Default | LayerFlags::Dynamic;
}

// =============================================================================
// RenderLayer
// =============================================================================

/// Configuration for a render layer
struct RenderLayer {
    std::string name;
    LayerId id;
    LayerFlags flags = layer_flags::Default;
    std::int32_t sort_order = 0;         // Lower = rendered first

    // Culling
    float cull_distance = 1000.0f;       // Max render distance
    float lod_bias = 0.0f;               // LOD selection bias

    // Blending (for transparent layers)
    BlendMode blend_mode = BlendMode::Opaque;

    // Stencil
    std::uint8_t stencil_ref = 0;
    std::uint8_t stencil_mask = 0xFF;

    /// Create default layer
    [[nodiscard]] static RenderLayer create_default(const std::string& name, std::uint32_t bit) {
        RenderLayer layer;
        layer.name = name;
        layer.id = LayerId::from_bit(bit);
        layer.flags = layer_flags::Default;
        return layer;
    }

    /// Create transparent layer
    [[nodiscard]] static RenderLayer create_transparent(const std::string& name, std::uint32_t bit) {
        RenderLayer layer;
        layer.name = name;
        layer.id = LayerId::from_bit(bit);
        layer.flags = layer_flags::Transparent;
        layer.blend_mode = BlendMode::AlphaBlend;
        layer.sort_order = 100;  // Render after opaque
        return layer;
    }

    /// Create UI layer
    [[nodiscard]] static RenderLayer create_ui(const std::string& name, std::uint32_t bit) {
        RenderLayer layer;
        layer.name = name;
        layer.id = LayerId::from_bit(bit);
        layer.flags = layer_flags::UI;
        layer.blend_mode = BlendMode::AlphaBlend;
        layer.sort_order = 200;  // Render last
        return layer;
    }

    /// Builder pattern
    RenderLayer& with_flags(LayerFlags f) { flags = f; return *this; }
    RenderLayer& with_sort_order(std::int32_t order) { sort_order = order; return *this; }
    RenderLayer& with_cull_distance(float dist) { cull_distance = dist; return *this; }
    RenderLayer& with_blend(BlendMode mode) { blend_mode = mode; return *this; }
};

// =============================================================================
// LayerManager
// =============================================================================

/// Manages render layers
class LayerManager {
public:
    /// Constructor
    LayerManager() {
        // Create default layers
        add(RenderLayer::create_default("default", 0));
        add(RenderLayer::create_transparent("transparent", 1));
        add(RenderLayer::create_ui("ui", 2));
    }

    /// Add layer
    void add(const RenderLayer& layer) {
        m_layers.push_back(layer);
        m_name_to_index[layer.name] = m_layers.size() - 1;
        m_id_to_index[layer.id.value] = m_layers.size() - 1;
        m_sorted = false;
    }

    /// Get layer by name
    [[nodiscard]] RenderLayer* get(const std::string& name) {
        auto it = m_name_to_index.find(name);
        if (it == m_name_to_index.end()) return nullptr;
        return &m_layers[it->second];
    }

    [[nodiscard]] const RenderLayer* get(const std::string& name) const {
        auto it = m_name_to_index.find(name);
        if (it == m_name_to_index.end()) return nullptr;
        return &m_layers[it->second];
    }

    /// Get layer by ID
    [[nodiscard]] RenderLayer* get(LayerId id) {
        auto it = m_id_to_index.find(id.value);
        if (it == m_id_to_index.end()) return nullptr;
        return &m_layers[it->second];
    }

    /// Get layer by ID (const)
    [[nodiscard]] const RenderLayer* get(LayerId id) const {
        auto it = m_id_to_index.find(id.value);
        if (it == m_id_to_index.end()) return nullptr;
        return &m_layers[it->second];
    }

    /// Get all layers
    [[nodiscard]] const std::vector<RenderLayer>& layers() const noexcept {
        return m_layers;
    }

    /// Get sorted layers (by sort_order)
    [[nodiscard]] const std::vector<std::size_t>& sorted_indices() {
        if (!m_sorted) {
            m_sorted_indices.resize(m_layers.size());
            for (std::size_t i = 0; i < m_layers.size(); ++i) {
                m_sorted_indices[i] = i;
            }
            std::sort(m_sorted_indices.begin(), m_sorted_indices.end(),
                [this](std::size_t a, std::size_t b) {
                    return m_layers[a].sort_order < m_layers[b].sort_order;
                });
            m_sorted = true;
        }
        return m_sorted_indices;
    }

    /// Set layer visibility
    void set_visible(LayerId id, bool visible) {
        if (auto* layer = get(id)) {
            if (visible) {
                layer->flags = layer->flags | LayerFlags::Visible;
            } else {
                layer->flags = static_cast<LayerFlags>(
                    static_cast<std::uint32_t>(layer->flags) &
                    ~static_cast<std::uint32_t>(LayerFlags::Visible));
            }
        }
    }

    /// Check if layer is visible
    [[nodiscard]] bool is_visible(LayerId id) const {
        if (const auto* layer = get(LayerId(id))) {
            return has_flag(layer->flags, LayerFlags::Visible);
        }
        return false;
    }

    /// Get visible layer mask
    [[nodiscard]] std::uint32_t visible_mask() const {
        std::uint32_t mask = 0;
        for (const auto& layer : m_layers) {
            if (has_flag(layer.flags, LayerFlags::Visible)) {
                mask |= layer.id.value;
            }
        }
        return mask;
    }

    /// Get shadow caster mask
    [[nodiscard]] std::uint32_t shadow_caster_mask() const {
        std::uint32_t mask = 0;
        for (const auto& layer : m_layers) {
            if (has_flag(layer.flags, LayerFlags::CastsShadows) &&
                has_flag(layer.flags, LayerFlags::Visible)) {
                mask |= layer.id.value;
            }
        }
        return mask;
    }

    /// Clear all layers
    void clear() {
        m_layers.clear();
        m_name_to_index.clear();
        m_id_to_index.clear();
        m_sorted_indices.clear();
        m_sorted = false;
    }

private:
    std::vector<RenderLayer> m_layers;
    std::unordered_map<std::string, std::size_t> m_name_to_index;
    std::unordered_map<std::uint32_t, std::size_t> m_id_to_index;
    std::vector<std::size_t> m_sorted_indices;
    bool m_sorted = false;
};

// =============================================================================
// ViewportConfig
// =============================================================================

/// Viewport configuration
struct ViewportConfig {
    std::array<float, 2> offset = {0.0f, 0.0f};      // Normalized offset (0-1)
    std::array<float, 2> size = {1.0f, 1.0f};        // Normalized size (0-1)
    std::array<float, 2> depth_range = {0.0f, 1.0f}; // Near/far depth range
    std::uint32_t scissor_x = 0;
    std::uint32_t scissor_y = 0;
    std::uint32_t scissor_width = 0;                  // 0 = full viewport
    std::uint32_t scissor_height = 0;

    /// Create fullscreen viewport
    [[nodiscard]] static ViewportConfig fullscreen() {
        return ViewportConfig{};
    }

    /// Create split-screen viewport
    [[nodiscard]] static ViewportConfig split_horizontal(std::uint32_t index, std::uint32_t count) {
        ViewportConfig cfg;
        float h = 1.0f / static_cast<float>(count);
        cfg.offset = {0.0f, h * static_cast<float>(index)};
        cfg.size = {1.0f, h};
        return cfg;
    }

    [[nodiscard]] static ViewportConfig split_vertical(std::uint32_t index, std::uint32_t count) {
        ViewportConfig cfg;
        float w = 1.0f / static_cast<float>(count);
        cfg.offset = {w * static_cast<float>(index), 0.0f};
        cfg.size = {w, 1.0f};
        return cfg;
    }

    /// Get pixel rect
    [[nodiscard]] std::array<std::uint32_t, 4> pixel_rect(std::uint32_t render_width, std::uint32_t render_height) const {
        return {
            static_cast<std::uint32_t>(offset[0] * static_cast<float>(render_width)),
            static_cast<std::uint32_t>(offset[1] * static_cast<float>(render_height)),
            static_cast<std::uint32_t>(size[0] * static_cast<float>(render_width)),
            static_cast<std::uint32_t>(size[1] * static_cast<float>(render_height))
        };
    }
};

// =============================================================================
// View
// =============================================================================

/// A view represents a camera + viewport + layer mask combination
struct View {
    std::string name;
    Camera* camera = nullptr;
    ViewportConfig viewport;
    std::uint32_t layer_mask = layers::All;
    std::int32_t priority = 0;            // Lower = rendered first
    bool enabled = true;

    // Clear settings
    std::array<float, 4> clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    float clear_depth = 0.0f;             // Reverse-Z: 0 is far

    /// Create main view
    [[nodiscard]] static View create_main(const std::string& name, Camera* cam) {
        View view;
        view.name = name;
        view.camera = cam;
        view.priority = 0;
        return view;
    }

    /// Create shadow view
    [[nodiscard]] static View create_shadow(const std::string& name, Camera* cam) {
        View view;
        view.name = name;
        view.camera = cam;
        view.layer_mask = layers::ShadowCasters;
        view.priority = -100;  // Render before main
        return view;
    }

    /// Create reflection view
    [[nodiscard]] static View create_reflection(const std::string& name, Camera* cam) {
        View view;
        view.name = name;
        view.camera = cam;
        view.layer_mask = layers::All & ~layers::UI.value & ~layers::Debug.value;
        view.priority = -50;
        return view;
    }
};

// =============================================================================
// CompositorNode
// =============================================================================

/// Node in the compositor graph
struct CompositorNode {
    std::string name;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::function<void(const PassContext&)> execute;
    std::int32_t priority = 0;
    bool enabled = true;
};

// =============================================================================
// CompositorConfig
// =============================================================================

/// Compositor configuration
struct CompositorConfig {
    // Resolution
    std::uint32_t render_width = 1920;
    std::uint32_t render_height = 1080;
    float render_scale = 1.0f;            // Internal render resolution scale

    // Anti-aliasing
    std::uint32_t msaa_samples = 1;       // 1 = disabled
    bool use_fxaa = true;
    bool use_taa = false;

    // HDR
    bool hdr_enabled = true;
    float exposure = 1.0f;
    float gamma = 2.2f;

    // Post-processing
    bool bloom_enabled = true;
    float bloom_intensity = 0.5f;
    float bloom_threshold = 1.0f;

    bool ssao_enabled = true;
    float ssao_radius = 0.5f;
    float ssao_intensity = 1.0f;

    bool dof_enabled = false;
    float dof_focus_distance = 10.0f;
    float dof_aperture = 0.1f;

    bool motion_blur_enabled = false;
    float motion_blur_intensity = 0.5f;

    // Shadows
    bool shadows_enabled = true;

    // Debug
    bool debug_wireframe = false;
    bool debug_normals = false;
    bool debug_depth = false;
    bool debug_gbuffer = false;

    /// Get actual render size (after scale)
    [[nodiscard]] std::array<std::uint32_t, 2> scaled_size() const {
        return {
            static_cast<std::uint32_t>(static_cast<float>(render_width) * render_scale),
            static_cast<std::uint32_t>(static_cast<float>(render_height) * render_scale)
        };
    }
};

// =============================================================================
// CompositorStats
// =============================================================================

/// Statistics for compositor
struct CompositorStats {
    float frame_time_ms = 0.0f;
    float gpu_time_ms = 0.0f;
    float cpu_time_ms = 0.0f;

    std::uint32_t total_draw_calls = 0;
    std::uint32_t total_triangles = 0;
    std::uint32_t total_instances = 0;

    std::uint32_t visible_objects = 0;
    std::uint32_t culled_objects = 0;

    std::uint32_t pass_count = 0;
    std::uint32_t view_count = 0;

    void reset() {
        frame_time_ms = 0.0f;
        gpu_time_ms = 0.0f;
        cpu_time_ms = 0.0f;
        total_draw_calls = 0;
        total_triangles = 0;
        total_instances = 0;
        visible_objects = 0;
        culled_objects = 0;
        pass_count = 0;
        view_count = 0;
    }
};

// =============================================================================
// Compositor
// =============================================================================

/// Main compositor class - manages views, layers, and render passes
class Compositor {
public:
    /// Constructor
    explicit Compositor(const CompositorConfig& config = CompositorConfig{})
        : m_config(config) {}

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /// Get config
    [[nodiscard]] CompositorConfig& config() noexcept { return m_config; }
    [[nodiscard]] const CompositorConfig& config() const noexcept { return m_config; }

    /// Set render size
    void set_render_size(std::uint32_t width, std::uint32_t height) {
        m_config.render_width = width;
        m_config.render_height = height;
        m_passes.resize_all(width, height);
    }

    /// Set render scale
    void set_render_scale(float scale) {
        m_config.render_scale = scale;
        auto [w, h] = m_config.scaled_size();
        m_passes.resize_all(w, h);
    }

    // -------------------------------------------------------------------------
    // Views
    // -------------------------------------------------------------------------

    /// Add view
    void add_view(const View& view) {
        m_views.push_back(view);
        m_views_sorted = false;
    }

    /// Get view by name
    [[nodiscard]] View* get_view(const std::string& name) {
        for (auto& view : m_views) {
            if (view.name == name) return &view;
        }
        return nullptr;
    }

    /// Get all views
    [[nodiscard]] std::vector<View>& views() noexcept { return m_views; }
    [[nodiscard]] const std::vector<View>& views() const noexcept { return m_views; }

    /// Remove view
    bool remove_view(const std::string& name) {
        auto it = std::find_if(m_views.begin(), m_views.end(),
            [&](const View& v) { return v.name == name; });
        if (it != m_views.end()) {
            m_views.erase(it);
            m_views_sorted = false;
            return true;
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Layers
    // -------------------------------------------------------------------------

    /// Get layer manager
    [[nodiscard]] LayerManager& layers() noexcept { return m_layers; }
    [[nodiscard]] const LayerManager& layers() const noexcept { return m_layers; }

    // -------------------------------------------------------------------------
    // Passes
    // -------------------------------------------------------------------------

    /// Get pass registry
    [[nodiscard]] PassRegistry& passes() noexcept { return m_passes; }
    [[nodiscard]] const PassRegistry& passes() const noexcept { return m_passes; }

    /// Add built-in pass
    PassId add_builtin_pass(PassType type) {
        switch (type) {
            case PassType::DepthPrePass:
                return m_passes.add_callback(builtin_passes::depth_prepass(), [](const PassContext&) {});
            case PassType::ShadowMap:
                return m_passes.add_callback(builtin_passes::shadow_map(), [](const PassContext&) {});
            case PassType::GBuffer:
                return m_passes.add_callback(builtin_passes::gbuffer(), [](const PassContext&) {});
            case PassType::Lighting:
                return m_passes.add_callback(builtin_passes::deferred_lighting(), [](const PassContext&) {});
            case PassType::Forward:
                return m_passes.add_callback(builtin_passes::forward(), [](const PassContext&) {});
            case PassType::ForwardTransparent:
                return m_passes.add_callback(builtin_passes::forward_transparent(), [](const PassContext&) {});
            case PassType::Sky:
                return m_passes.add_callback(builtin_passes::sky(), [](const PassContext&) {});
            case PassType::Ssao:
                return m_passes.add_callback(builtin_passes::ssao(), [](const PassContext&) {});
            case PassType::Bloom:
                return m_passes.add_callback(builtin_passes::bloom(), [](const PassContext&) {});
            case PassType::Tonemapping:
                return m_passes.add_callback(builtin_passes::tonemapping(), [](const PassContext&) {});
            case PassType::Fxaa:
                return m_passes.add_callback(builtin_passes::fxaa(), [](const PassContext&) {});
            case PassType::Debug:
                return m_passes.add_callback(builtin_passes::debug_overlay(), [](const PassContext&) {});
            case PassType::Ui:
                return m_passes.add_callback(builtin_passes::ui(), [](const PassContext&) {});
            default:
                return PassId::invalid();
        }
    }

    /// Setup default forward pipeline
    void setup_forward_pipeline() {
        m_passes.clear();
        add_builtin_pass(PassType::DepthPrePass);
        add_builtin_pass(PassType::ShadowMap);
        add_builtin_pass(PassType::Forward);
        add_builtin_pass(PassType::ForwardTransparent);
        add_builtin_pass(PassType::Sky);
        add_builtin_pass(PassType::Ssao);
        add_builtin_pass(PassType::Bloom);
        add_builtin_pass(PassType::Tonemapping);
        add_builtin_pass(PassType::Fxaa);
        add_builtin_pass(PassType::Debug);
        add_builtin_pass(PassType::Ui);
    }

    /// Setup default deferred pipeline
    void setup_deferred_pipeline() {
        m_passes.clear();
        add_builtin_pass(PassType::DepthPrePass);
        add_builtin_pass(PassType::ShadowMap);
        add_builtin_pass(PassType::GBuffer);
        add_builtin_pass(PassType::Lighting);
        add_builtin_pass(PassType::ForwardTransparent);
        add_builtin_pass(PassType::Sky);
        add_builtin_pass(PassType::Ssao);
        add_builtin_pass(PassType::Bloom);
        add_builtin_pass(PassType::Tonemapping);
        add_builtin_pass(PassType::Fxaa);
        add_builtin_pass(PassType::Debug);
        add_builtin_pass(PassType::Ui);
    }

    // -------------------------------------------------------------------------
    // Frame Execution
    // -------------------------------------------------------------------------

    /// Begin frame
    void begin_frame() {
        m_stats.reset();
        m_frame_index++;
    }

    /// Execute all views and passes
    void execute(float delta_time) {
        sort_views();

        auto [width, height] = m_config.scaled_size();

        for (const auto& view : m_views) {
            if (!view.enabled || !view.camera) continue;

            PassContext ctx;
            ctx.frame_index = m_frame_index;
            ctx.delta_time = delta_time;
            ctx.render_size = {width, height};

            auto rect = view.viewport.pixel_rect(width, height);
            ctx.viewport_offset = {rect[0], rect[1]};
            ctx.viewport_size = {rect[2], rect[3]};

            m_passes.execute_all(ctx);

            m_stats.view_count++;
        }

        m_stats.pass_count = static_cast<std::uint32_t>(m_passes.count());
    }

    /// End frame
    void end_frame() {
        // Compute final stats
    }

    // -------------------------------------------------------------------------
    // Stats
    // -------------------------------------------------------------------------

    /// Get stats
    [[nodiscard]] const CompositorStats& stats() const noexcept { return m_stats; }

    /// Get frame index
    [[nodiscard]] std::uint32_t frame_index() const noexcept { return m_frame_index; }

private:
    void sort_views() {
        if (m_views_sorted) return;
        std::sort(m_views.begin(), m_views.end(),
            [](const View& a, const View& b) { return a.priority < b.priority; });
        m_views_sorted = true;
    }

private:
    CompositorConfig m_config;
    LayerManager m_layers;
    PassRegistry m_passes;
    std::vector<View> m_views;
    bool m_views_sorted = false;

    CompositorStats m_stats;
    std::uint32_t m_frame_index = 0;
};

// =============================================================================
// RenderQueue
// =============================================================================

/// Sort key for render queue
struct RenderSortKey {
    std::uint64_t value = 0;

    /// Create sort key from components
    [[nodiscard]] static RenderSortKey create(
        std::uint8_t pass,           // 8 bits - pass index
        std::uint8_t layer,          // 8 bits - layer
        std::uint16_t material,      // 16 bits - material/shader
        std::uint32_t depth)         // 32 bits - depth (front-to-back or back-to-front)
    {
        RenderSortKey key;
        key.value = (static_cast<std::uint64_t>(pass) << 56) |
                    (static_cast<std::uint64_t>(layer) << 48) |
                    (static_cast<std::uint64_t>(material) << 32) |
                    static_cast<std::uint64_t>(depth);
        return key;
    }

    bool operator<(const RenderSortKey& other) const noexcept {
        return value < other.value;
    }
};

/// Item in render queue
struct RenderItem {
    RenderSortKey sort_key;
    std::uint64_t mesh_id = 0;
    std::uint64_t material_id = 0;
    std::uint32_t instance_offset = 0;
    std::uint32_t instance_count = 1;

    bool operator<(const RenderItem& other) const noexcept {
        return sort_key < other.sort_key;
    }
};

/// Render queue for sorting draw calls
class RenderQueue {
public:
    /// Reserve capacity
    void reserve(std::size_t capacity) {
        m_items.reserve(capacity);
    }

    /// Add item
    void add(const RenderItem& item) {
        m_items.push_back(item);
    }

    /// Sort queue
    void sort() {
        std::sort(m_items.begin(), m_items.end());
    }

    /// Get items
    [[nodiscard]] const std::vector<RenderItem>& items() const noexcept {
        return m_items;
    }

    /// Get item count
    [[nodiscard]] std::size_t size() const noexcept {
        return m_items.size();
    }

    /// Clear queue
    void clear() {
        m_items.clear();
    }

    /// Iterate items
    template<typename F>
    void for_each(F&& callback) const {
        for (const auto& item : m_items) {
            callback(item);
        }
    }

private:
    std::vector<RenderItem> m_items;
};

} // namespace void_render
