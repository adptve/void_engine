#pragma once

/// @file light.hpp
/// @brief Lighting system for void_render

#include "fwd.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace void_render {

// =============================================================================
// Light Limits
// =============================================================================

/// Maximum directional lights (uniform buffer)
static constexpr std::size_t MAX_DIRECTIONAL_LIGHTS = 4;

/// Maximum point lights (storage buffer)
static constexpr std::size_t MAX_POINT_LIGHTS = 256;

/// Maximum spot lights (storage buffer)
static constexpr std::size_t MAX_SPOT_LIGHTS = 128;

// =============================================================================
// GpuDirectionalLight (GPU-ready)
// =============================================================================

/// Directional light data for GPU (64 bytes, aligned)
struct alignas(16) GpuDirectionalLight {
    std::array<float, 3> direction;     // 12 bytes (normalized, world space)
    float _pad0 = 0.0f;                 // 4 bytes
    std::array<float, 3> color;         // 12 bytes (linear RGB)
    float intensity = 1.0f;             // 4 bytes (lux)
    std::array<std::array<float, 4>, 4> shadow_matrix;  // 64 bytes (view-projection)
    std::int32_t shadow_map_index = -1; // 4 bytes (-1 = no shadow)
    std::array<float, 3> _pad1;         // 12 bytes

    /// Size in bytes
    static constexpr std::size_t SIZE = 112;

    /// Default constructor
    GpuDirectionalLight()
        : direction{0, -1, 0}
        , color{1, 1, 1}
        , shadow_matrix{{
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}
        }}
        , _pad1{0, 0, 0} {}

    /// Construct with direction and color
    GpuDirectionalLight(std::array<float, 3> dir, std::array<float, 3> col, float intens = 1.0f)
        : direction(dir)
        , color(col)
        , intensity(intens)
        , shadow_matrix{{
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}
        }}
        , _pad1{0, 0, 0} {
        normalize_direction();
    }

    /// Normalize direction vector
    void normalize_direction() {
        float len = std::sqrt(direction[0] * direction[0] +
                              direction[1] * direction[1] +
                              direction[2] * direction[2]);
        if (len > 1e-6f) {
            direction[0] /= len;
            direction[1] /= len;
            direction[2] /= len;
        }
    }
};

// =============================================================================
// GpuPointLight (GPU-ready)
// =============================================================================

/// Point light data for GPU (48 bytes, aligned)
struct alignas(16) GpuPointLight {
    std::array<float, 3> position;      // 12 bytes (world space)
    float range = 10.0f;                // 4 bytes (max attenuation distance)
    std::array<float, 3> color;         // 12 bytes (linear RGB)
    float intensity = 1.0f;             // 4 bytes (lumens)
    std::array<float, 3> attenuation;   // 12 bytes [constant, linear, quadratic]
    std::int32_t shadow_map_index = -1; // 4 bytes (cubemap index, -1 = no shadow)

    /// Size in bytes
    static constexpr std::size_t SIZE = 48;

    /// Default constructor
    GpuPointLight()
        : position{0, 0, 0}
        , color{1, 1, 1}
        , attenuation{1, 0.09f, 0.032f} {}

    /// Construct with position and color
    GpuPointLight(std::array<float, 3> pos, std::array<float, 3> col,
                  float intens = 1.0f, float r = 10.0f)
        : position(pos)
        , range(r)
        , color(col)
        , intensity(intens)
        , attenuation{1, 0.09f, 0.032f} {}

    /// Set attenuation factors
    void set_attenuation(float constant, float linear, float quadratic) {
        attenuation = {constant, linear, quadratic};
    }

    /// Calculate attenuation at distance
    [[nodiscard]] float calculate_attenuation(float distance) const {
        return 1.0f / (attenuation[0] +
                       attenuation[1] * distance +
                       attenuation[2] * distance * distance);
    }
};

// =============================================================================
// GpuSpotLight (GPU-ready)
// =============================================================================

/// Spot light data for GPU (96 bytes, aligned)
struct alignas(16) GpuSpotLight {
    std::array<float, 3> position;      // 12 bytes
    float range = 10.0f;                // 4 bytes
    std::array<float, 3> direction;     // 12 bytes (normalized)
    float inner_cos = 0.95f;            // 4 bytes (cosine of inner cone angle)
    std::array<float, 3> color;         // 12 bytes (linear RGB)
    float outer_cos = 0.9f;             // 4 bytes (cosine of outer cone angle)
    std::array<float, 3> attenuation;   // 12 bytes
    float intensity = 1.0f;             // 4 bytes (lumens)
    std::array<std::array<float, 4>, 4> shadow_matrix;  // 64 bytes
    std::int32_t shadow_map_index = -1; // 4 bytes
    std::array<float, 3> _pad;          // 12 bytes

    /// Size in bytes
    static constexpr std::size_t SIZE = 144;

    /// Default constructor
    GpuSpotLight()
        : position{0, 0, 0}
        , direction{0, -1, 0}
        , color{1, 1, 1}
        , attenuation{1, 0.09f, 0.032f}
        , shadow_matrix{{
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}
        }}
        , _pad{0, 0, 0} {}

    /// Construct with position, direction, and color
    GpuSpotLight(std::array<float, 3> pos, std::array<float, 3> dir,
                 std::array<float, 3> col, float inner_angle_deg = 30.0f,
                 float outer_angle_deg = 45.0f, float intens = 1.0f, float r = 10.0f)
        : position(pos)
        , range(r)
        , direction(dir)
        , color(col)
        , intensity(intens)
        , attenuation{1, 0.09f, 0.032f}
        , shadow_matrix{{
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}
        }}
        , _pad{0, 0, 0} {
        set_cone_angles(inner_angle_deg, outer_angle_deg);
        normalize_direction();
    }

    /// Set cone angles in degrees
    void set_cone_angles(float inner_deg, float outer_deg) {
        const float pi = 3.14159265358979323846f;
        inner_cos = std::cos(inner_deg * pi / 180.0f);
        outer_cos = std::cos(outer_deg * pi / 180.0f);
    }

    /// Normalize direction vector
    void normalize_direction() {
        float len = std::sqrt(direction[0] * direction[0] +
                              direction[1] * direction[1] +
                              direction[2] * direction[2]);
        if (len > 1e-6f) {
            direction[0] /= len;
            direction[1] /= len;
            direction[2] /= len;
        }
    }
};

// =============================================================================
// LightCounts
// =============================================================================

/// Light counts for shader uniform
struct LightCounts {
    std::uint32_t directional = 0;
    std::uint32_t point = 0;
    std::uint32_t spot = 0;
    std::uint32_t _pad = 0;
};

// =============================================================================
// LightBuffer
// =============================================================================

/// Buffer for all scene lights
class LightBuffer {
public:
    /// Default constructor
    LightBuffer() = default;

    /// Begin frame (clear all lights)
    void begin_frame() {
        m_directional_lights.clear();
        m_point_lights.clear();
        m_spot_lights.clear();
        m_counts = LightCounts{};
    }

    /// Add directional light
    /// Returns true if added, false if at capacity
    bool add_directional(const GpuDirectionalLight& light) {
        if (m_directional_lights.size() >= MAX_DIRECTIONAL_LIGHTS) {
            return false;
        }
        m_directional_lights.push_back(light);
        m_counts.directional = static_cast<std::uint32_t>(m_directional_lights.size());
        return true;
    }

    /// Add point light
    bool add_point(const GpuPointLight& light) {
        if (m_point_lights.size() >= MAX_POINT_LIGHTS) {
            return false;
        }
        m_point_lights.push_back(light);
        m_counts.point = static_cast<std::uint32_t>(m_point_lights.size());
        return true;
    }

    /// Add spot light
    bool add_spot(const GpuSpotLight& light) {
        if (m_spot_lights.size() >= MAX_SPOT_LIGHTS) {
            return false;
        }
        m_spot_lights.push_back(light);
        m_counts.spot = static_cast<std::uint32_t>(m_spot_lights.size());
        return true;
    }

    /// Get directional lights
    [[nodiscard]] const std::vector<GpuDirectionalLight>& directional_lights() const noexcept {
        return m_directional_lights;
    }

    /// Get point lights
    [[nodiscard]] const std::vector<GpuPointLight>& point_lights() const noexcept {
        return m_point_lights;
    }

    /// Get spot lights
    [[nodiscard]] const std::vector<GpuSpotLight>& spot_lights() const noexcept {
        return m_spot_lights;
    }

    /// Get light counts
    [[nodiscard]] const LightCounts& counts() const noexcept {
        return m_counts;
    }

    /// Get directional light count
    [[nodiscard]] std::size_t directional_count() const noexcept {
        return m_directional_lights.size();
    }

    /// Get point light count
    [[nodiscard]] std::size_t point_count() const noexcept {
        return m_point_lights.size();
    }

    /// Get spot light count
    [[nodiscard]] std::size_t spot_count() const noexcept {
        return m_spot_lights.size();
    }

    /// Get total light count
    [[nodiscard]] std::size_t total_count() const noexcept {
        return m_directional_lights.size() + m_point_lights.size() + m_spot_lights.size();
    }

    /// Sort point lights by distance to camera (nearest first)
    void sort_point_lights_by_distance(std::array<float, 3> camera_pos) {
        std::sort(m_point_lights.begin(), m_point_lights.end(),
            [&camera_pos](const GpuPointLight& a, const GpuPointLight& b) {
                float da = (a.position[0] - camera_pos[0]) * (a.position[0] - camera_pos[0]) +
                           (a.position[1] - camera_pos[1]) * (a.position[1] - camera_pos[1]) +
                           (a.position[2] - camera_pos[2]) * (a.position[2] - camera_pos[2]);
                float db = (b.position[0] - camera_pos[0]) * (b.position[0] - camera_pos[0]) +
                           (b.position[1] - camera_pos[1]) * (b.position[1] - camera_pos[1]) +
                           (b.position[2] - camera_pos[2]) * (b.position[2] - camera_pos[2]);
                return da < db;
            });
    }

    /// Sort spot lights by distance to camera
    void sort_spot_lights_by_distance(std::array<float, 3> camera_pos) {
        std::sort(m_spot_lights.begin(), m_spot_lights.end(),
            [&camera_pos](const GpuSpotLight& a, const GpuSpotLight& b) {
                float da = (a.position[0] - camera_pos[0]) * (a.position[0] - camera_pos[0]) +
                           (a.position[1] - camera_pos[1]) * (a.position[1] - camera_pos[1]) +
                           (a.position[2] - camera_pos[2]) * (a.position[2] - camera_pos[2]);
                float db = (b.position[0] - camera_pos[0]) * (b.position[0] - camera_pos[0]) +
                           (b.position[1] - camera_pos[1]) * (b.position[1] - camera_pos[1]) +
                           (b.position[2] - camera_pos[2]) * (b.position[2] - camera_pos[2]);
                return da < db;
            });
    }

    /// Get raw bytes for directional lights
    [[nodiscard]] const void* directional_data() const noexcept {
        return m_directional_lights.data();
    }

    /// Get directional data size in bytes
    [[nodiscard]] std::size_t directional_data_size() const noexcept {
        return m_directional_lights.size() * sizeof(GpuDirectionalLight);
    }

    /// Get raw bytes for point lights
    [[nodiscard]] const void* point_data() const noexcept {
        return m_point_lights.data();
    }

    /// Get point data size in bytes
    [[nodiscard]] std::size_t point_data_size() const noexcept {
        return m_point_lights.size() * sizeof(GpuPointLight);
    }

    /// Get raw bytes for spot lights
    [[nodiscard]] const void* spot_data() const noexcept {
        return m_spot_lights.data();
    }

    /// Get spot data size in bytes
    [[nodiscard]] std::size_t spot_data_size() const noexcept {
        return m_spot_lights.size() * sizeof(GpuSpotLight);
    }

private:
    std::vector<GpuDirectionalLight> m_directional_lights;
    std::vector<GpuPointLight> m_point_lights;
    std::vector<GpuSpotLight> m_spot_lights;
    LightCounts m_counts;
};

// =============================================================================
// LightExtractionConfig
// =============================================================================

/// Configuration for light extraction
struct LightExtractionConfig {
    bool distance_culling = true;
    float culling_distance_multiplier = 1.5f;  // Multiplier on light range
    bool sort_by_importance = true;
    std::size_t max_lights_per_frame = 256;

    [[nodiscard]] static LightExtractionConfig defaults() {
        return LightExtractionConfig{};
    }
};

// =============================================================================
// LightExtractionStats
// =============================================================================

/// Statistics for light extraction
struct LightExtractionStats {
    std::size_t lights_processed = 0;
    std::size_t lights_culled = 0;
    std::size_t directional_added = 0;
    std::size_t point_added = 0;
    std::size_t spot_added = 0;

    void reset() {
        lights_processed = 0;
        lights_culled = 0;
        directional_added = 0;
        point_added = 0;
        spot_added = 0;
    }
};

// =============================================================================
// LightExtractor
// =============================================================================

/// Extracts and filters lights from scene data
class LightExtractor {
public:
    /// Default constructor
    LightExtractor() = default;

    /// Construct with config
    explicit LightExtractor(LightExtractionConfig config)
        : m_config(config) {}

    /// Begin frame
    void begin_frame(LightBuffer& buffer) {
        buffer.begin_frame();
        m_stats.reset();
    }

    /// Extract directional light
    bool extract_directional(LightBuffer& buffer,
                              std::array<float, 3> direction,
                              std::array<float, 3> color,
                              float intensity) {
        m_stats.lights_processed++;

        GpuDirectionalLight light(direction, color, intensity);
        if (buffer.add_directional(light)) {
            m_stats.directional_added++;
            return true;
        }
        return false;
    }

    /// Extract point light with culling
    bool extract_point(LightBuffer& buffer,
                        std::array<float, 3> position,
                        float range,
                        std::array<float, 3> color,
                        float intensity,
                        std::array<float, 3> attenuation,
                        std::array<float, 3> camera_pos) {
        m_stats.lights_processed++;

        // Distance culling
        if (m_config.distance_culling) {
            float dx = position[0] - camera_pos[0];
            float dy = position[1] - camera_pos[1];
            float dz = position[2] - camera_pos[2];
            float dist_sq = dx * dx + dy * dy + dz * dz;
            float cull_dist = range * m_config.culling_distance_multiplier;

            if (dist_sq > cull_dist * cull_dist) {
                m_stats.lights_culled++;
                return false;
            }
        }

        GpuPointLight light(position, color, intensity, range);
        light.set_attenuation(attenuation[0], attenuation[1], attenuation[2]);

        if (buffer.add_point(light)) {
            m_stats.point_added++;
            return true;
        }
        return false;
    }

    /// Extract spot light with culling
    bool extract_spot(LightBuffer& buffer,
                       std::array<float, 3> position,
                       std::array<float, 3> direction,
                       float range,
                       float inner_angle,
                       float outer_angle,
                       std::array<float, 3> color,
                       float intensity,
                       std::array<float, 3> camera_pos) {
        m_stats.lights_processed++;

        // Distance culling
        if (m_config.distance_culling) {
            float dx = position[0] - camera_pos[0];
            float dy = position[1] - camera_pos[1];
            float dz = position[2] - camera_pos[2];
            float dist_sq = dx * dx + dy * dy + dz * dz;
            float cull_dist = range * m_config.culling_distance_multiplier;

            if (dist_sq > cull_dist * cull_dist) {
                m_stats.lights_culled++;
                return false;
            }
        }

        GpuSpotLight light(position, direction, color, inner_angle, outer_angle, intensity, range);

        if (buffer.add_spot(light)) {
            m_stats.spot_added++;
            return true;
        }
        return false;
    }

    /// Get statistics
    [[nodiscard]] const LightExtractionStats& stats() const noexcept {
        return m_stats;
    }

    /// Get configuration
    [[nodiscard]] const LightExtractionConfig& config() const noexcept {
        return m_config;
    }

    /// Set configuration
    void set_config(LightExtractionConfig config) {
        m_config = config;
    }

private:
    LightExtractionConfig m_config;
    LightExtractionStats m_stats;
};

} // namespace void_render
