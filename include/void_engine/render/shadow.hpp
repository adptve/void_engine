#pragma once

/// @file shadow.hpp
/// @brief Shadow mapping system for void_render

#include "fwd.hpp"
#include "camera.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <numbers>

namespace void_render {

// =============================================================================
// Shadow Constants
// =============================================================================

/// Maximum shadow cascades for directional lights
static constexpr std::size_t MAX_SHADOW_CASCADES = 4;

/// Maximum point light shadow maps
static constexpr std::size_t MAX_POINT_SHADOW_MAPS = 16;

/// Maximum spot light shadow maps
static constexpr std::size_t MAX_SPOT_SHADOW_MAPS = 32;

/// Default shadow map resolution
static constexpr std::uint32_t DEFAULT_SHADOW_MAP_SIZE = 2048;

/// Default shadow atlas size
static constexpr std::uint32_t DEFAULT_SHADOW_ATLAS_SIZE = 4096;

// =============================================================================
// ShadowQuality
// =============================================================================

/// Shadow quality presets
enum class ShadowQuality : std::uint8_t {
    Off = 0,
    Low = 1,       // 512x512
    Medium = 2,    // 1024x1024
    High = 3,      // 2048x2048
    Ultra = 4,     // 4096x4096
};

/// Get shadow map size for quality preset
[[nodiscard]] inline std::uint32_t shadow_quality_size(ShadowQuality quality) {
    switch (quality) {
        case ShadowQuality::Off: return 0;
        case ShadowQuality::Low: return 512;
        case ShadowQuality::Medium: return 1024;
        case ShadowQuality::High: return 2048;
        case ShadowQuality::Ultra: return 4096;
        default: return 2048;
    }
}

// =============================================================================
// ShadowFilterMode
// =============================================================================

/// Shadow filtering mode
enum class ShadowFilterMode : std::uint8_t {
    Hard = 0,          // No filtering, sharp shadows
    Pcf = 1,           // Percentage-closer filtering
    Pcss = 2,          // Percentage-closer soft shadows
    Vsm = 3,           // Variance shadow maps
    Esm = 4,           // Exponential shadow maps
};

// =============================================================================
// ShadowConfig
// =============================================================================

/// Shadow mapping configuration
struct ShadowConfig {
    ShadowQuality quality = ShadowQuality::High;
    ShadowFilterMode filter_mode = ShadowFilterMode::Pcf;

    // Cascade settings
    std::uint32_t cascade_count = 4;
    float cascade_split_lambda = 0.75f;  // PSSM split scheme (0=uniform, 1=logarithmic)
    float cascade_blend_distance = 5.0f;  // Blend distance between cascades

    // Shadow bias
    float depth_bias = 0.005f;
    float normal_bias = 0.02f;
    float slope_bias = 0.0f;

    // PCF settings
    std::uint32_t pcf_samples = 16;
    float pcf_radius = 1.5f;

    // PCSS settings
    float pcss_light_size = 0.5f;
    float pcss_blocker_search_samples = 16;

    // Atlas settings
    std::uint32_t atlas_size = DEFAULT_SHADOW_ATLAS_SIZE;
    std::uint32_t max_point_shadows = MAX_POINT_SHADOW_MAPS;
    std::uint32_t max_spot_shadows = MAX_SPOT_SHADOW_MAPS;

    // Performance
    bool stabilize_cascades = true;  // Reduces shimmering on camera movement
    bool cull_front_faces = false;   // Cull front faces for Peter Panning reduction

    /// Create low quality config
    [[nodiscard]] static ShadowConfig low() {
        ShadowConfig cfg;
        cfg.quality = ShadowQuality::Low;
        cfg.cascade_count = 2;
        cfg.pcf_samples = 4;
        cfg.max_point_shadows = 4;
        cfg.max_spot_shadows = 8;
        return cfg;
    }

    /// Create high quality config
    [[nodiscard]] static ShadowConfig high() {
        ShadowConfig cfg;
        cfg.quality = ShadowQuality::High;
        cfg.cascade_count = 4;
        cfg.filter_mode = ShadowFilterMode::Pcf;
        cfg.pcf_samples = 16;
        return cfg;
    }

    /// Create ultra quality config
    [[nodiscard]] static ShadowConfig ultra() {
        ShadowConfig cfg;
        cfg.quality = ShadowQuality::Ultra;
        cfg.cascade_count = 4;
        cfg.filter_mode = ShadowFilterMode::Pcss;
        cfg.pcf_samples = 32;
        cfg.pcss_blocker_search_samples = 32;
        return cfg;
    }
};

// =============================================================================
// CascadeData (GPU-ready)
// =============================================================================

/// Cascade shadow map data for GPU (128 bytes, aligned)
struct alignas(16) GpuCascadeData {
    std::array<std::array<float, 4>, 4> view_proj_matrix;  // 64 bytes
    std::array<float, 4> split_depths;                      // 16 bytes (near, far, unused, unused)
    std::array<float, 4> atlas_viewport;                    // 16 bytes (x, y, width, height in UV)
    std::array<float, 4> shadow_params;                     // 16 bytes (bias, normal_bias, unused, unused)
    std::array<float, 4> _pad;                              // 16 bytes padding

    /// Size in bytes
    static constexpr std::size_t SIZE = 128;

    GpuCascadeData() {
        view_proj_matrix = {{
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}
        }};
        split_depths = {0, 0, 0, 0};
        atlas_viewport = {0, 0, 1, 1};
        shadow_params = {0.005f, 0.02f, 0, 0};
        _pad = {0, 0, 0, 0};
    }
};

static_assert(sizeof(GpuCascadeData) == 128, "GpuCascadeData must be 128 bytes");

// =============================================================================
// GpuShadowData
// =============================================================================

/// Shadow data for GPU (1024 bytes = 4 cascades + params)
struct alignas(16) GpuShadowData {
    std::array<GpuCascadeData, MAX_SHADOW_CASCADES> cascades;  // 512 bytes

    // Global shadow params (16 bytes)
    std::array<float, 4> global_params;  // (cascade_count, filter_mode, pcf_radius, pcss_light_size)

    // Light direction for directional shadow (16 bytes)
    std::array<float, 3> light_direction;
    float _pad0;

    // Shadow color/ambient (16 bytes)
    std::array<float, 3> shadow_color;  // Color in shadow (for soft shadows)
    float shadow_strength;              // 0 = no shadow, 1 = full shadow

    // Reserved for future use (464 bytes)
    std::array<float, 116> _reserved;

    /// Size in bytes
    static constexpr std::size_t SIZE = 1024;

    GpuShadowData() : _reserved{} {
        global_params = {4.0f, 1.0f, 1.5f, 0.5f};  // 4 cascades, PCF, 1.5 radius, 0.5 light size
        light_direction = {0, -1, 0};
        _pad0 = 0;
        shadow_color = {0, 0, 0};
        shadow_strength = 1.0f;
    }
};

static_assert(sizeof(GpuShadowData) == 1024, "GpuShadowData must be 1024 bytes");

// =============================================================================
// ShadowAtlasEntry
// =============================================================================

/// Entry in shadow atlas
struct ShadowAtlasEntry {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t light_index = UINT32_MAX;
    bool in_use = false;

    /// Get UV viewport
    [[nodiscard]] std::array<float, 4> viewport_uv(std::uint32_t atlas_size) const {
        float inv_size = 1.0f / static_cast<float>(atlas_size);
        return {
            static_cast<float>(x) * inv_size,
            static_cast<float>(y) * inv_size,
            static_cast<float>(width) * inv_size,
            static_cast<float>(height) * inv_size
        };
    }
};

// =============================================================================
// ShadowAtlas
// =============================================================================

/// Shadow map atlas manager
class ShadowAtlas {
public:
    /// Construct with atlas size
    explicit ShadowAtlas(std::uint32_t size = DEFAULT_SHADOW_ATLAS_SIZE)
        : m_size(size) {}

    /// Get atlas size
    [[nodiscard]] std::uint32_t size() const noexcept { return m_size; }

    /// Resize atlas (clears all entries)
    void resize(std::uint32_t size) {
        m_size = size;
        clear();
    }

    /// Allocate entry in atlas
    [[nodiscard]] bool allocate(std::uint32_t width, std::uint32_t height, ShadowAtlasEntry& out_entry) {
        // Simple row-based allocation
        for (auto& entry : m_entries) {
            if (!entry.in_use && entry.width >= width && entry.height >= height) {
                // Found suitable free entry
                entry.in_use = true;
                out_entry = entry;
                out_entry.width = width;
                out_entry.height = height;
                return true;
            }
        }

        // Try to allocate new entry
        if (m_current_y + height <= m_size) {
            if (m_current_x + width <= m_size) {
                // Fits in current row
                ShadowAtlasEntry entry;
                entry.x = m_current_x;
                entry.y = m_current_y;
                entry.width = width;
                entry.height = height;
                entry.in_use = true;

                m_current_x += width;
                m_row_height = std::max(m_row_height, height);

                m_entries.push_back(entry);
                out_entry = entry;
                return true;
            } else {
                // Move to next row
                m_current_x = 0;
                m_current_y += m_row_height;
                m_row_height = 0;

                if (m_current_y + height <= m_size && width <= m_size) {
                    ShadowAtlasEntry entry;
                    entry.x = m_current_x;
                    entry.y = m_current_y;
                    entry.width = width;
                    entry.height = height;
                    entry.in_use = true;

                    m_current_x += width;
                    m_row_height = height;

                    m_entries.push_back(entry);
                    out_entry = entry;
                    return true;
                }
            }
        }

        return false;  // Atlas full
    }

    /// Free entry
    void free(std::uint32_t x, std::uint32_t y) {
        for (auto& entry : m_entries) {
            if (entry.x == x && entry.y == y) {
                entry.in_use = false;
                entry.light_index = UINT32_MAX;
                break;
            }
        }
    }

    /// Clear all entries
    void clear() {
        m_entries.clear();
        m_current_x = 0;
        m_current_y = 0;
        m_row_height = 0;
    }

    /// Get all entries
    [[nodiscard]] const std::vector<ShadowAtlasEntry>& entries() const noexcept {
        return m_entries;
    }

    /// Get used entry count
    [[nodiscard]] std::size_t used_count() const {
        std::size_t count = 0;
        for (const auto& entry : m_entries) {
            if (entry.in_use) count++;
        }
        return count;
    }

private:
    std::uint32_t m_size;
    std::vector<ShadowAtlasEntry> m_entries;
    std::uint32_t m_current_x = 0;
    std::uint32_t m_current_y = 0;
    std::uint32_t m_row_height = 0;
};

// =============================================================================
// CascadeSplitCalculator
// =============================================================================

/// Calculates cascade split distances
class CascadeSplitCalculator {
public:
    /// Calculate cascade splits using PSSM (Practical Split Scheme)
    /// @param near_plane Camera near plane
    /// @param far_plane Camera far plane
    /// @param cascade_count Number of cascades (1-4)
    /// @param lambda Blend factor (0=uniform, 1=logarithmic)
    [[nodiscard]] static std::array<float, MAX_SHADOW_CASCADES + 1> calculate_splits(
        float near_plane,
        float far_plane,
        std::uint32_t cascade_count,
        float lambda = 0.75f)
    {
        std::array<float, MAX_SHADOW_CASCADES + 1> splits = {};
        splits[0] = near_plane;

        cascade_count = std::min(cascade_count, static_cast<std::uint32_t>(MAX_SHADOW_CASCADES));

        for (std::uint32_t i = 1; i <= cascade_count; ++i) {
            float p = static_cast<float>(i) / static_cast<float>(cascade_count);

            // Logarithmic split
            float log_split = near_plane * std::pow(far_plane / near_plane, p);

            // Uniform split
            float uniform_split = near_plane + (far_plane - near_plane) * p;

            // Blend
            splits[i] = lambda * log_split + (1.0f - lambda) * uniform_split;
        }

        return splits;
    }

    /// Calculate tight cascade bounds for a frustum segment
    [[nodiscard]] static std::array<std::array<float, 3>, 8> calculate_frustum_corners(
        const Camera& camera,
        float near_dist,
        float far_dist)
    {
        // Get inverse view-projection for the segment
        PerspectiveProjection segment_proj = camera.perspective();
        segment_proj.near_plane = near_dist;
        segment_proj.far_plane = far_dist;

        std::array<std::array<float, 3>, 8> corners;

        // NDC corners
        constexpr std::array<std::array<float, 3>, 8> ndc_corners = {{
            {-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0},  // Near plane (reverse-Z: z=1 is near, z=0 is far)
            {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}   // Far plane
        }};

        // For simplicity, calculate corners using basic frustum math
        float aspect = segment_proj.aspect_ratio;
        float tan_half_fov = std::tan(segment_proj.fov_y / 2.0f);

        float near_h = tan_half_fov * near_dist;
        float near_w = near_h * aspect;
        float far_h = tan_half_fov * far_dist;
        float far_w = far_h * aspect;

        // Get camera basis vectors
        auto pos = camera.position();
        auto fwd = camera.forward();
        auto rgt = camera.right();
        auto up = camera.up();

        // Near plane center
        std::array<float, 3> near_center = {
            pos[0] + fwd[0] * near_dist,
            pos[1] + fwd[1] * near_dist,
            pos[2] + fwd[2] * near_dist
        };

        // Far plane center
        std::array<float, 3> far_center = {
            pos[0] + fwd[0] * far_dist,
            pos[1] + fwd[1] * far_dist,
            pos[2] + fwd[2] * far_dist
        };

        // Near plane corners
        corners[0] = {near_center[0] - rgt[0] * near_w - up[0] * near_h,
                      near_center[1] - rgt[1] * near_w - up[1] * near_h,
                      near_center[2] - rgt[2] * near_w - up[2] * near_h};
        corners[1] = {near_center[0] + rgt[0] * near_w - up[0] * near_h,
                      near_center[1] + rgt[1] * near_w - up[1] * near_h,
                      near_center[2] + rgt[2] * near_w - up[2] * near_h};
        corners[2] = {near_center[0] + rgt[0] * near_w + up[0] * near_h,
                      near_center[1] + rgt[1] * near_w + up[1] * near_h,
                      near_center[2] + rgt[2] * near_w + up[2] * near_h};
        corners[3] = {near_center[0] - rgt[0] * near_w + up[0] * near_h,
                      near_center[1] - rgt[1] * near_w + up[1] * near_h,
                      near_center[2] - rgt[2] * near_w + up[2] * near_h};

        // Far plane corners
        corners[4] = {far_center[0] - rgt[0] * far_w - up[0] * far_h,
                      far_center[1] - rgt[1] * far_w - up[1] * far_h,
                      far_center[2] - rgt[2] * far_w - up[2] * far_h};
        corners[5] = {far_center[0] + rgt[0] * far_w - up[0] * far_h,
                      far_center[1] + rgt[1] * far_w - up[1] * far_h,
                      far_center[2] + rgt[2] * far_w - up[2] * far_h};
        corners[6] = {far_center[0] + rgt[0] * far_w + up[0] * far_h,
                      far_center[1] + rgt[1] * far_w + up[1] * far_h,
                      far_center[2] + rgt[2] * far_w + up[2] * far_h};
        corners[7] = {far_center[0] - rgt[0] * far_w + up[0] * far_h,
                      far_center[1] - rgt[1] * far_w + up[1] * far_h,
                      far_center[2] - rgt[2] * far_w + up[2] * far_h};

        return corners;
    }
};

// =============================================================================
// CascadedShadowMap
// =============================================================================

/// Manages cascaded shadow maps for directional lights
class CascadedShadowMap {
public:
    /// Construct with config
    explicit CascadedShadowMap(const ShadowConfig& config = ShadowConfig{})
        : m_config(config) {
        m_cascade_count = std::min(config.cascade_count, static_cast<std::uint32_t>(MAX_SHADOW_CASCADES));
    }

    /// Update cascades for camera and light direction
    void update(const Camera& camera, const std::array<float, 3>& light_direction) {
        m_light_direction = light_direction;

        // Normalize light direction
        float len = std::sqrt(
            light_direction[0] * light_direction[0] +
            light_direction[1] * light_direction[1] +
            light_direction[2] * light_direction[2]
        );
        if (len > 1e-6f) {
            m_light_direction[0] /= len;
            m_light_direction[1] /= len;
            m_light_direction[2] /= len;
        }

        // Calculate splits
        float near_p = camera.perspective().near_plane;
        float far_p = camera.perspective().far_plane;
        auto splits = CascadeSplitCalculator::calculate_splits(
            near_p, far_p, m_cascade_count, m_config.cascade_split_lambda
        );

        // Update each cascade
        for (std::uint32_t i = 0; i < m_cascade_count; ++i) {
            update_cascade(camera, splits[i], splits[i + 1], i);
        }
    }

    /// Get cascade count
    [[nodiscard]] std::uint32_t cascade_count() const noexcept { return m_cascade_count; }

    /// Get cascade data
    [[nodiscard]] const GpuCascadeData& cascade(std::size_t index) const {
        return m_cascades[index];
    }

    /// Get all cascades
    [[nodiscard]] const std::array<GpuCascadeData, MAX_SHADOW_CASCADES>& cascades() const noexcept {
        return m_cascades;
    }

    /// Get config
    [[nodiscard]] const ShadowConfig& config() const noexcept { return m_config; }

    /// Set config
    void set_config(const ShadowConfig& config) {
        m_config = config;
        m_cascade_count = std::min(config.cascade_count, static_cast<std::uint32_t>(MAX_SHADOW_CASCADES));
    }

    /// Get GPU shadow data
    [[nodiscard]] GpuShadowData gpu_data() const {
        GpuShadowData data;
        data.cascades = m_cascades;
        data.global_params = {
            static_cast<float>(m_cascade_count),
            static_cast<float>(m_config.filter_mode),
            m_config.pcf_radius,
            m_config.pcss_light_size
        };
        data.light_direction = m_light_direction;
        data.shadow_color = {0, 0, 0};
        data.shadow_strength = 1.0f;
        return data;
    }

private:
    void update_cascade(const Camera& camera, float near_dist, float far_dist, std::uint32_t cascade_index) {
        // Get frustum corners for this cascade
        auto corners = CascadeSplitCalculator::calculate_frustum_corners(camera, near_dist, far_dist);

        // Calculate frustum center
        std::array<float, 3> center = {0, 0, 0};
        for (const auto& corner : corners) {
            center[0] += corner[0];
            center[1] += corner[1];
            center[2] += corner[2];
        }
        center[0] /= 8.0f;
        center[1] /= 8.0f;
        center[2] /= 8.0f;

        // Calculate light-space view matrix
        // Light looks in opposite direction (towards the scene)
        std::array<float, 3> light_pos = {
            center[0] - m_light_direction[0] * 100.0f,  // Back up along light direction
            center[1] - m_light_direction[1] * 100.0f,
            center[2] - m_light_direction[2] * 100.0f
        };

        // Calculate light basis
        std::array<float, 3> light_forward = m_light_direction;
        std::array<float, 3> world_up = {0, 1, 0};

        // Handle case where light is pointing straight up/down
        if (std::abs(light_forward[1]) > 0.99f) {
            world_up = {0, 0, 1};
        }

        // Right = forward × up
        std::array<float, 3> light_right = {
            light_forward[1] * world_up[2] - light_forward[2] * world_up[1],
            light_forward[2] * world_up[0] - light_forward[0] * world_up[2],
            light_forward[0] * world_up[1] - light_forward[1] * world_up[0]
        };
        float right_len = std::sqrt(light_right[0] * light_right[0] + light_right[1] * light_right[1] + light_right[2] * light_right[2]);
        if (right_len > 1e-6f) {
            light_right[0] /= right_len;
            light_right[1] /= right_len;
            light_right[2] /= right_len;
        }

        // Up = right × forward
        std::array<float, 3> light_up = {
            light_right[1] * light_forward[2] - light_right[2] * light_forward[1],
            light_right[2] * light_forward[0] - light_right[0] * light_forward[2],
            light_right[0] * light_forward[1] - light_right[1] * light_forward[0]
        };

        // Light view matrix
        std::array<std::array<float, 4>, 4> light_view;
        light_view[0] = {light_right[0], light_up[0], light_forward[0], 0};
        light_view[1] = {light_right[1], light_up[1], light_forward[1], 0};
        light_view[2] = {light_right[2], light_up[2], light_forward[2], 0};
        light_view[3] = {
            -(light_right[0] * light_pos[0] + light_right[1] * light_pos[1] + light_right[2] * light_pos[2]),
            -(light_up[0] * light_pos[0] + light_up[1] * light_pos[1] + light_up[2] * light_pos[2]),
            -(light_forward[0] * light_pos[0] + light_forward[1] * light_pos[1] + light_forward[2] * light_pos[2]),
            1
        };

        // Transform frustum corners to light space
        float min_x = FLT_MAX, max_x = -FLT_MAX;
        float min_y = FLT_MAX, max_y = -FLT_MAX;
        float min_z = FLT_MAX, max_z = -FLT_MAX;

        for (const auto& corner : corners) {
            // Transform to light space
            float lx = light_view[0][0] * corner[0] + light_view[1][0] * corner[1] + light_view[2][0] * corner[2] + light_view[3][0];
            float ly = light_view[0][1] * corner[0] + light_view[1][1] * corner[1] + light_view[2][1] * corner[2] + light_view[3][1];
            float lz = light_view[0][2] * corner[0] + light_view[1][2] * corner[1] + light_view[2][2] * corner[2] + light_view[3][2];

            min_x = std::min(min_x, lx);
            max_x = std::max(max_x, lx);
            min_y = std::min(min_y, ly);
            max_y = std::max(max_y, ly);
            min_z = std::min(min_z, lz);
            max_z = std::max(max_z, lz);
        }

        // Extend Z range for shadow casters behind camera
        min_z -= 200.0f;

        // Stabilize cascade if enabled (reduces shimmering)
        if (m_config.stabilize_cascades) {
            std::uint32_t shadow_size = shadow_quality_size(m_config.quality);
            float texel_size = (max_x - min_x) / static_cast<float>(shadow_size);

            min_x = std::floor(min_x / texel_size) * texel_size;
            max_x = std::ceil(max_x / texel_size) * texel_size;
            min_y = std::floor(min_y / texel_size) * texel_size;
            max_y = std::ceil(max_y / texel_size) * texel_size;
        }

        // Light orthographic projection
        std::array<std::array<float, 4>, 4> light_proj = {};
        light_proj[0][0] = 2.0f / (max_x - min_x);
        light_proj[1][1] = 2.0f / (max_y - min_y);
        light_proj[2][2] = 1.0f / (max_z - min_z);
        light_proj[3][0] = -(max_x + min_x) / (max_x - min_x);
        light_proj[3][1] = -(max_y + min_y) / (max_y - min_y);
        light_proj[3][2] = -min_z / (max_z - min_z);
        light_proj[3][3] = 1.0f;

        // Compute view-proj
        auto& cascade = m_cascades[cascade_index];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                cascade.view_proj_matrix[i][j] = 0;
                for (int k = 0; k < 4; ++k) {
                    cascade.view_proj_matrix[i][j] += light_proj[k][j] * light_view[i][k];
                }
            }
        }

        cascade.split_depths = {near_dist, far_dist, 0, 0};
        cascade.shadow_params = {m_config.depth_bias, m_config.normal_bias, 0, 0};

        // Atlas viewport (assuming cascades are arranged horizontally)
        float inv_cascade_count = 1.0f / static_cast<float>(m_cascade_count);
        cascade.atlas_viewport = {
            static_cast<float>(cascade_index) * inv_cascade_count,
            0.0f,
            inv_cascade_count,
            1.0f
        };
    }

private:
    ShadowConfig m_config;
    std::uint32_t m_cascade_count;
    std::array<float, 3> m_light_direction = {0, -1, 0};
    std::array<GpuCascadeData, MAX_SHADOW_CASCADES> m_cascades;
};

// =============================================================================
// PointLightShadowData
// =============================================================================

/// Point light shadow map data (cube map - 6 faces)
struct alignas(16) GpuPointShadowData {
    std::array<std::array<std::array<float, 4>, 4>, 6> face_matrices;  // 6 face view-proj matrices (384 bytes)
    std::array<float, 3> light_position;
    float light_range;
    std::array<float, 4> shadow_params;  // bias, normal_bias, unused, unused

    /// Size in bytes
    static constexpr std::size_t SIZE = 416;
};

// =============================================================================
// SpotLightShadowData
// =============================================================================

/// Spot light shadow map data
struct alignas(16) GpuSpotShadowData {
    std::array<std::array<float, 4>, 4> view_proj_matrix;  // 64 bytes
    std::array<float, 3> light_position;
    float light_range;
    std::array<float, 3> light_direction;
    float outer_angle;
    std::array<float, 4> atlas_viewport;  // x, y, width, height in UV
    std::array<float, 4> shadow_params;   // bias, normal_bias, unused, unused

    /// Size in bytes
    static constexpr std::size_t SIZE = 128;
};

static_assert(sizeof(GpuSpotShadowData) == 128, "GpuSpotShadowData must be 128 bytes");

// =============================================================================
// ShadowManager
// =============================================================================

/// Manages all shadow maps
class ShadowManager {
public:
    /// Construct with config
    explicit ShadowManager(const ShadowConfig& config = ShadowConfig{})
        : m_config(config)
        , m_cascaded_shadow(config)
        , m_atlas(config.atlas_size) {}

    /// Get config
    [[nodiscard]] const ShadowConfig& config() const noexcept { return m_config; }

    /// Set config
    void set_config(const ShadowConfig& config) {
        m_config = config;
        m_cascaded_shadow.set_config(config);
        m_atlas.resize(config.atlas_size);
    }

    /// Get cascaded shadow map
    [[nodiscard]] CascadedShadowMap& cascaded_shadow() noexcept { return m_cascaded_shadow; }
    [[nodiscard]] const CascadedShadowMap& cascaded_shadow() const noexcept { return m_cascaded_shadow; }

    /// Get shadow atlas
    [[nodiscard]] ShadowAtlas& atlas() noexcept { return m_atlas; }
    [[nodiscard]] const ShadowAtlas& atlas() const noexcept { return m_atlas; }

    /// Update directional shadow for camera
    void update_directional(const Camera& camera, const std::array<float, 3>& light_direction) {
        m_cascaded_shadow.update(camera, light_direction);
    }

    /// Begin frame (clear per-frame data)
    void begin_frame() {
        m_point_shadows.clear();
        m_spot_shadows.clear();
    }

    /// Add point light shadow
    bool add_point_shadow(const std::array<float, 3>& position, float range, std::uint32_t light_index) {
        if (m_point_shadows.size() >= m_config.max_point_shadows) {
            return false;
        }

        GpuPointShadowData data;
        data.light_position = position;
        data.light_range = range;
        data.shadow_params = {m_config.depth_bias, m_config.normal_bias, 0, 0};

        // Calculate 6 face matrices for cube map
        constexpr std::array<std::array<float, 3>, 6> face_dirs = {{
            {1, 0, 0}, {-1, 0, 0},  // +X, -X
            {0, 1, 0}, {0, -1, 0},  // +Y, -Y
            {0, 0, 1}, {0, 0, -1}   // +Z, -Z
        }};
        constexpr std::array<std::array<float, 3>, 6> face_ups = {{
            {0, -1, 0}, {0, -1, 0},
            {0, 0, 1}, {0, 0, -1},
            {0, -1, 0}, {0, -1, 0}
        }};

        float fov = std::numbers::pi_v<float> / 2.0f;  // 90 degrees
        float aspect = 1.0f;
        float near_p = 0.1f;

        for (std::size_t i = 0; i < 6; ++i) {
            // Calculate view matrix for this face
            // ... (simplified, would need proper look_at calculation)
            data.face_matrices[i] = {{
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0},
                {-position[0], -position[1], -position[2], 1}
            }};
        }

        (void)light_index;
        (void)fov;
        (void)aspect;
        (void)near_p;
        (void)face_dirs;
        (void)face_ups;

        m_point_shadows.push_back(data);
        return true;
    }

    /// Add spot light shadow
    bool add_spot_shadow(
        const std::array<float, 3>& position,
        const std::array<float, 3>& direction,
        float range,
        float outer_angle,
        std::uint32_t light_index)
    {
        if (m_spot_shadows.size() >= m_config.max_spot_shadows) {
            return false;
        }

        // Allocate atlas entry
        std::uint32_t shadow_size = shadow_quality_size(m_config.quality) / 2;  // Smaller for spot lights
        ShadowAtlasEntry entry;
        if (!m_atlas.allocate(shadow_size, shadow_size, entry)) {
            return false;
        }
        entry.light_index = light_index;

        GpuSpotShadowData data;
        data.light_position = position;
        data.light_range = range;
        data.light_direction = direction;
        data.outer_angle = outer_angle;
        data.atlas_viewport = entry.viewport_uv(m_atlas.size());
        data.shadow_params = {m_config.depth_bias, m_config.normal_bias, 0, 0};

        // Calculate view-proj matrix
        // ... (simplified)
        data.view_proj_matrix = {{
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}
        }};

        m_spot_shadows.push_back(data);
        return true;
    }

    /// Get point shadows
    [[nodiscard]] const std::vector<GpuPointShadowData>& point_shadows() const noexcept {
        return m_point_shadows;
    }

    /// Get spot shadows
    [[nodiscard]] const std::vector<GpuSpotShadowData>& spot_shadows() const noexcept {
        return m_spot_shadows;
    }

private:
    ShadowConfig m_config;
    CascadedShadowMap m_cascaded_shadow;
    ShadowAtlas m_atlas;
    std::vector<GpuPointShadowData> m_point_shadows;
    std::vector<GpuSpotShadowData> m_spot_shadows;
};

} // namespace void_render
