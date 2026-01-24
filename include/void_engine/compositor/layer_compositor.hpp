#pragma once

/// @file layer_compositor.hpp
/// @brief Layer compositor for rendering layers with blend modes
///
/// Provides the rendering side of layer compositing, integrating with
/// void_render for GPU-based layer composition.

#include "fwd.hpp"
#include "layer.hpp"
#include "compositor.hpp"
#include "rehydration.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace void_compositor {

// =============================================================================
// Layer Compositor Config
// =============================================================================

/// Layer compositor configuration
struct LayerCompositorConfig {
    /// Default background color (RGBA)
    float background_r = 0.0f;
    float background_g = 0.0f;
    float background_b = 0.0f;
    float background_a = 1.0f;

    /// Use GPU acceleration
    bool use_gpu = true;

    /// Enable caching for static layers
    bool enable_caching = true;

    /// Maximum cache size (in textures)
    std::uint32_t max_cache_size = 32;

    /// Enable debug visualization
    bool debug_overlay = false;

    /// Create default config
    static LayerCompositorConfig create() {
        return LayerCompositorConfig{};
    }

    /// Builder: set background color
    [[nodiscard]] LayerCompositorConfig& with_background(float r, float g, float b, float a = 1.0f) {
        background_r = r;
        background_g = g;
        background_b = b;
        background_a = a;
        return *this;
    }

    /// Builder: enable/disable GPU
    [[nodiscard]] LayerCompositorConfig& with_gpu(bool enable) {
        use_gpu = enable;
        return *this;
    }

    /// Builder: enable/disable caching
    [[nodiscard]] LayerCompositorConfig& with_caching(bool enable) {
        enable_caching = enable;
        return *this;
    }

    /// Builder: enable/disable debug overlay
    [[nodiscard]] LayerCompositorConfig& with_debug(bool enable) {
        debug_overlay = enable;
        return *this;
    }
};

// =============================================================================
// Layer Render Callback
// =============================================================================

/// Callback for rendering layer content
/// @param layer The layer to render
/// @param target The render target handle
/// @param width Target width
/// @param height Target height
/// @return true if rendering succeeded
using LayerRenderCallback = std::function<bool(
    const Layer& layer,
    void* target,
    std::uint32_t width,
    std::uint32_t height
)>;

// =============================================================================
// Layer Compositor Statistics
// =============================================================================

/// Statistics for layer compositor performance
struct LayerCompositorStats {
    /// Number of layers rendered this frame
    std::uint32_t layers_rendered = 0;

    /// Number of layers skipped (invisible/culled)
    std::uint32_t layers_skipped = 0;

    /// Number of cache hits
    std::uint32_t cache_hits = 0;

    /// Number of cache misses
    std::uint32_t cache_misses = 0;

    /// Number of blend operations
    std::uint32_t blend_operations = 0;

    /// Frame render time (nanoseconds)
    std::uint64_t render_time_ns = 0;

    /// Reset statistics
    void reset() {
        layers_rendered = 0;
        layers_skipped = 0;
        cache_hits = 0;
        cache_misses = 0;
        blend_operations = 0;
        render_time_ns = 0;
    }
};

// =============================================================================
// Layer Compositor Interface
// =============================================================================

/// Interface for layer compositor implementations
class ILayerCompositor {
public:
    virtual ~ILayerCompositor() = default;

    /// Initialize the compositor
    /// @param width Output width
    /// @param height Output height
    /// @return true on success
    virtual bool initialize(std::uint32_t width, std::uint32_t height) = 0;

    /// Shutdown the compositor
    virtual void shutdown() = 0;

    /// Check if initialized
    [[nodiscard]] virtual bool is_initialized() const = 0;

    /// Resize the output
    /// @param width New width
    /// @param height New height
    /// @return true on success
    virtual bool resize(std::uint32_t width, std::uint32_t height) = 0;

    /// Get current output size
    [[nodiscard]] virtual std::pair<std::uint32_t, std::uint32_t> size() const = 0;

    /// Begin frame rendering
    virtual void begin_frame() = 0;

    /// Composite all layers from the layer manager
    /// @param layers Layer manager containing layers to composite
    /// @param render_callback Callback to render dynamic layer content
    virtual void composite(LayerManager& layers, LayerRenderCallback render_callback) = 0;

    /// End frame rendering
    virtual void end_frame() = 0;

    /// Get the final composited output texture handle
    [[nodiscard]] virtual void* output_texture() const = 0;

    /// Get statistics
    [[nodiscard]] virtual const LayerCompositorStats& stats() const = 0;

    /// Get configuration
    [[nodiscard]] virtual const LayerCompositorConfig& config() const = 0;

    /// Set debug overlay enabled
    virtual void set_debug_overlay(bool enabled) = 0;

    /// Clear the cache
    virtual void clear_cache() = 0;
};

// =============================================================================
// Null Layer Compositor (for testing)
// =============================================================================

/// Null layer compositor implementation for testing
class NullLayerCompositor : public ILayerCompositor {
public:
    explicit NullLayerCompositor(const LayerCompositorConfig& config = LayerCompositorConfig{})
        : m_config(config)
    {}

    bool initialize(std::uint32_t width, std::uint32_t height) override {
        m_width = width;
        m_height = height;
        m_initialized = true;
        return true;
    }

    void shutdown() override {
        m_initialized = false;
    }

    [[nodiscard]] bool is_initialized() const override {
        return m_initialized;
    }

    bool resize(std::uint32_t width, std::uint32_t height) override {
        m_width = width;
        m_height = height;
        return true;
    }

    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const override {
        return {m_width, m_height};
    }

    void begin_frame() override {
        m_stats.reset();
    }

    void composite(LayerManager& layers, LayerRenderCallback /*render_callback*/) override {
        auto sorted = layers.get_sorted_layers();

        for (Layer* layer : sorted) {
            if (!layer->is_visible()) {
                ++m_stats.layers_skipped;
                continue;
            }

            ++m_stats.layers_rendered;

            if (layer->needs_compositing()) {
                ++m_stats.blend_operations;
            }
        }
    }

    void end_frame() override {
        layers.clear_all_dirty();
    }

    [[nodiscard]] void* output_texture() const override {
        return nullptr;
    }

    [[nodiscard]] const LayerCompositorStats& stats() const override {
        return m_stats;
    }

    [[nodiscard]] const LayerCompositorConfig& config() const override {
        return m_config;
    }

    void set_debug_overlay(bool enabled) override {
        m_config.debug_overlay = enabled;
    }

    void clear_cache() override {
        // No-op for null compositor
    }

private:
    LayerCompositorConfig m_config;
    LayerCompositorStats m_stats;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    bool m_initialized = false;
    LayerManager layers; // Dummy for end_frame
};

// =============================================================================
// Software Layer Compositor
// =============================================================================

/// Software (CPU-based) layer compositor for fallback
class SoftwareLayerCompositor : public ILayerCompositor {
public:
    explicit SoftwareLayerCompositor(const LayerCompositorConfig& config = LayerCompositorConfig{})
        : m_config(config)
    {
        m_config.use_gpu = false;
    }

    bool initialize(std::uint32_t width, std::uint32_t height) override {
        m_width = width;
        m_height = height;

        // Allocate output buffer (RGBA8)
        m_output_buffer.resize(width * height * 4);
        std::fill(m_output_buffer.begin(), m_output_buffer.end(), 0);

        m_initialized = true;
        return true;
    }

    void shutdown() override {
        m_output_buffer.clear();
        m_initialized = false;
    }

    [[nodiscard]] bool is_initialized() const override {
        return m_initialized;
    }

    bool resize(std::uint32_t width, std::uint32_t height) override {
        m_width = width;
        m_height = height;
        m_output_buffer.resize(width * height * 4);
        std::fill(m_output_buffer.begin(), m_output_buffer.end(), 0);
        return true;
    }

    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const override {
        return {m_width, m_height};
    }

    void begin_frame() override {
        m_stats.reset();
        m_frame_start = std::chrono::steady_clock::now();

        // Clear to background color
        std::uint8_t r = static_cast<std::uint8_t>(m_config.background_r * 255.0f);
        std::uint8_t g = static_cast<std::uint8_t>(m_config.background_g * 255.0f);
        std::uint8_t b = static_cast<std::uint8_t>(m_config.background_b * 255.0f);
        std::uint8_t a = static_cast<std::uint8_t>(m_config.background_a * 255.0f);

        for (std::size_t i = 0; i < m_output_buffer.size(); i += 4) {
            m_output_buffer[i + 0] = r;
            m_output_buffer[i + 1] = g;
            m_output_buffer[i + 2] = b;
            m_output_buffer[i + 3] = a;
        }
    }

    void composite(LayerManager& layers, LayerRenderCallback render_callback) override {
        auto sorted = layers.get_sorted_layers();

        for (Layer* layer : sorted) {
            if (!layer->is_visible()) {
                ++m_stats.layers_skipped;
                continue;
            }

            const auto& bounds = layer->bounds();
            if (bounds.is_empty()) {
                ++m_stats.layers_skipped;
                continue;
            }

            // Render layer content if it has a render callback
            if (layer->content().type == LayerContentType::RenderTarget ||
                layer->content().type == LayerContentType::Empty) {
                if (render_callback) {
                    render_callback(*layer, m_output_buffer.data(), m_width, m_height);
                }
            }

            // Composite based on content type
            composite_layer(*layer);
            ++m_stats.layers_rendered;
        }
    }

    void end_frame() override {
        auto elapsed = std::chrono::steady_clock::now() - m_frame_start;
        m_stats.render_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    }

    [[nodiscard]] void* output_texture() const override {
        return const_cast<std::uint8_t*>(m_output_buffer.data());
    }

    [[nodiscard]] const LayerCompositorStats& stats() const override {
        return m_stats;
    }

    [[nodiscard]] const LayerCompositorConfig& config() const override {
        return m_config;
    }

    void set_debug_overlay(bool enabled) override {
        m_config.debug_overlay = enabled;
    }

    void clear_cache() override {
        // No caching in software compositor
    }

    /// Get the output buffer
    [[nodiscard]] const std::vector<std::uint8_t>& output_buffer() const {
        return m_output_buffer;
    }

private:
    /// Composite a single layer
    void composite_layer(const Layer& layer) {
        const auto& bounds = layer.bounds();
        const auto& content = layer.content();
        const auto& config = layer.config();

        // Calculate pixel bounds
        std::int32_t x0 = static_cast<std::int32_t>(bounds.x);
        std::int32_t y0 = static_cast<std::int32_t>(bounds.y);
        std::int32_t x1 = static_cast<std::int32_t>(bounds.x + bounds.width);
        std::int32_t y1 = static_cast<std::int32_t>(bounds.y + bounds.height);

        // Clamp to output bounds
        x0 = std::max(0, std::min(x0, static_cast<std::int32_t>(m_width)));
        y0 = std::max(0, std::min(y0, static_cast<std::int32_t>(m_height)));
        x1 = std::max(0, std::min(x1, static_cast<std::int32_t>(m_width)));
        y1 = std::max(0, std::min(y1, static_cast<std::int32_t>(m_height)));

        if (x0 >= x1 || y0 >= y1) {
            return;
        }

        // Get source color
        float src_r = 1.0f, src_g = 1.0f, src_b = 1.0f, src_a = 1.0f;

        if (content.type == LayerContentType::SolidColor) {
            src_r = content.color.r;
            src_g = content.color.g;
            src_b = content.color.b;
            src_a = content.color.a;
        }

        // Apply layer opacity
        src_a *= config.opacity;

        // Composite each pixel
        for (std::int32_t y = y0; y < y1; ++y) {
            for (std::int32_t x = x0; x < x1; ++x) {
                std::size_t idx = (y * m_width + x) * 4;

                // Get destination color
                float dst_r = m_output_buffer[idx + 0] / 255.0f;
                float dst_g = m_output_buffer[idx + 1] / 255.0f;
                float dst_b = m_output_buffer[idx + 2] / 255.0f;
                float dst_a = m_output_buffer[idx + 3] / 255.0f;

                // Blend
                float out_r, out_g, out_b, out_a;
                blend(src_r, src_g, src_b, src_a,
                      dst_r, dst_g, dst_b, dst_a,
                      out_r, out_g, out_b, out_a,
                      config.blend_mode);

                // Write back
                m_output_buffer[idx + 0] = static_cast<std::uint8_t>(std::clamp(out_r * 255.0f, 0.0f, 255.0f));
                m_output_buffer[idx + 1] = static_cast<std::uint8_t>(std::clamp(out_g * 255.0f, 0.0f, 255.0f));
                m_output_buffer[idx + 2] = static_cast<std::uint8_t>(std::clamp(out_b * 255.0f, 0.0f, 255.0f));
                m_output_buffer[idx + 3] = static_cast<std::uint8_t>(std::clamp(out_a * 255.0f, 0.0f, 255.0f));
            }
        }

        ++m_stats.blend_operations;
    }

    /// Apply blend mode
    void blend(float src_r, float src_g, float src_b, float src_a,
               float dst_r, float dst_g, float dst_b, float dst_a,
               float& out_r, float& out_g, float& out_b, float& out_a,
               BlendMode mode) {

        switch (mode) {
            case BlendMode::Normal:
                // Porter-Duff over
                out_a = src_a + dst_a * (1.0f - src_a);
                if (out_a > 0.0f) {
                    out_r = (src_r * src_a + dst_r * dst_a * (1.0f - src_a)) / out_a;
                    out_g = (src_g * src_a + dst_g * dst_a * (1.0f - src_a)) / out_a;
                    out_b = (src_b * src_a + dst_b * dst_a * (1.0f - src_a)) / out_a;
                } else {
                    out_r = out_g = out_b = 0.0f;
                }
                break;

            case BlendMode::Additive:
                out_r = std::min(1.0f, src_r * src_a + dst_r);
                out_g = std::min(1.0f, src_g * src_a + dst_g);
                out_b = std::min(1.0f, src_b * src_a + dst_b);
                out_a = std::min(1.0f, src_a + dst_a);
                break;

            case BlendMode::Multiply:
                out_r = src_r * dst_r;
                out_g = src_g * dst_g;
                out_b = src_b * dst_b;
                out_a = src_a * dst_a;
                break;

            case BlendMode::Screen:
                out_r = 1.0f - (1.0f - src_r) * (1.0f - dst_r);
                out_g = 1.0f - (1.0f - src_g) * (1.0f - dst_g);
                out_b = 1.0f - (1.0f - src_b) * (1.0f - dst_b);
                out_a = 1.0f - (1.0f - src_a) * (1.0f - dst_a);
                break;

            case BlendMode::Replace:
                out_r = src_r;
                out_g = src_g;
                out_b = src_b;
                out_a = src_a;
                break;

            case BlendMode::Overlay: {
                auto overlay_channel = [](float s, float d) {
                    if (d < 0.5f) {
                        return 2.0f * s * d;
                    } else {
                        return 1.0f - 2.0f * (1.0f - s) * (1.0f - d);
                    }
                };
                out_r = overlay_channel(src_r, dst_r);
                out_g = overlay_channel(src_g, dst_g);
                out_b = overlay_channel(src_b, dst_b);
                out_a = src_a + dst_a * (1.0f - src_a);
                break;
            }

            case BlendMode::SoftLight: {
                auto softlight_channel = [](float s, float d) {
                    if (s < 0.5f) {
                        return d - (1.0f - 2.0f * s) * d * (1.0f - d);
                    } else {
                        float g = (d <= 0.25f) ? ((16.0f * d - 12.0f) * d + 4.0f) * d : std::sqrt(d);
                        return d + (2.0f * s - 1.0f) * (g - d);
                    }
                };
                out_r = softlight_channel(src_r, dst_r);
                out_g = softlight_channel(src_g, dst_g);
                out_b = softlight_channel(src_b, dst_b);
                out_a = src_a + dst_a * (1.0f - src_a);
                break;
            }

            case BlendMode::HardLight: {
                auto hardlight_channel = [](float s, float d) {
                    if (s < 0.5f) {
                        return 2.0f * s * d;
                    } else {
                        return 1.0f - 2.0f * (1.0f - s) * (1.0f - d);
                    }
                };
                out_r = hardlight_channel(src_r, dst_r);
                out_g = hardlight_channel(src_g, dst_g);
                out_b = hardlight_channel(src_b, dst_b);
                out_a = src_a + dst_a * (1.0f - src_a);
                break;
            }

            case BlendMode::Difference:
                out_r = std::abs(src_r - dst_r);
                out_g = std::abs(src_g - dst_g);
                out_b = std::abs(src_b - dst_b);
                out_a = src_a + dst_a * (1.0f - src_a);
                break;

            case BlendMode::Exclusion:
                out_r = src_r + dst_r - 2.0f * src_r * dst_r;
                out_g = src_g + dst_g - 2.0f * src_g * dst_g;
                out_b = src_b + dst_b - 2.0f * src_b * dst_b;
                out_a = src_a + dst_a * (1.0f - src_a);
                break;

            default:
                // Fallback to normal
                out_a = src_a + dst_a * (1.0f - src_a);
                if (out_a > 0.0f) {
                    out_r = (src_r * src_a + dst_r * dst_a * (1.0f - src_a)) / out_a;
                    out_g = (src_g * src_a + dst_g * dst_a * (1.0f - src_a)) / out_a;
                    out_b = (src_b * src_a + dst_b * dst_a * (1.0f - src_a)) / out_a;
                } else {
                    out_r = out_g = out_b = 0.0f;
                }
                break;
        }
    }

private:
    LayerCompositorConfig m_config;
    LayerCompositorStats m_stats;
    std::vector<std::uint8_t> m_output_buffer;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    bool m_initialized = false;
    std::chrono::steady_clock::time_point m_frame_start;
};

// =============================================================================
// Layer Compositor Factory
// =============================================================================

/// Factory for creating layer compositor instances
class LayerCompositorFactory {
public:
    /// Create a layer compositor
    /// @param config Configuration
    /// @return Layer compositor instance
    static std::unique_ptr<ILayerCompositor> create(
        const LayerCompositorConfig& config = LayerCompositorConfig{}) {

        if (!config.use_gpu) {
            return std::make_unique<SoftwareLayerCompositor>(config);
        }

        // GPU implementation would go here (OpenGL, Vulkan, etc.)
        // For now, fall back to software
        auto sw_config = config;
        sw_config.use_gpu = false;
        return std::make_unique<SoftwareLayerCompositor>(sw_config);
    }

    /// Create a null layer compositor for testing
    static std::unique_ptr<ILayerCompositor> create_null(
        const LayerCompositorConfig& config = LayerCompositorConfig{}) {
        return std::make_unique<NullLayerCompositor>(config);
    }

    /// Create a software layer compositor
    static std::unique_ptr<ILayerCompositor> create_software(
        const LayerCompositorConfig& config = LayerCompositorConfig{}) {
        return std::make_unique<SoftwareLayerCompositor>(config);
    }
};

} // namespace void_compositor
