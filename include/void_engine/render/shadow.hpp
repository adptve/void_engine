#pragma once

/// @file shadow.hpp
/// @brief Shadow mapping system for void_render
///
/// Provides cascaded shadow maps for directional lights, shadow atlases for
/// point/spot lights, and optional ray-traced shadows with temporal accumulation.

#include "fwd.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <optional>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace void_render {

// Forward declaration for ShaderProgram (defined in gl_renderer.hpp)
class ShaderProgram;

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
    // Enable/disable
    bool enabled = true;

    // Quality preset
    ShadowQuality quality = ShadowQuality::High;
    ShadowFilterMode filter_mode = ShadowFilterMode::Pcf;

    // Cascade settings
    std::uint32_t cascade_count = 4;
    std::uint32_t resolution = 2048;
    float cascade_split_lambda = 0.75f;  // PSSM split scheme (0=uniform, 1=logarithmic)
    float cascade_blend_distance = 5.0f;  // Blend distance between cascades
    float shadow_distance = 100.0f;       // Maximum shadow distance

    // Shadow bias
    float depth_bias = 0.0005f;
    float normal_bias = 0.02f;
    float slope_bias = 0.0f;

    // PCF settings
    std::uint32_t pcf_samples = 16;
    std::uint32_t pcf_radius = 1;

    // PCSS settings
    float pcss_light_size = 0.5f;
    float pcss_blocker_search_samples = 16;

    // Atlas settings
    std::uint32_t atlas_size = DEFAULT_SHADOW_ATLAS_SIZE;
    std::uint32_t max_point_shadows = MAX_POINT_SHADOW_MAPS;
    std::uint32_t max_spot_shadows = MAX_SPOT_SHADOW_MAPS;

    // Performance/debug
    bool stabilize_cascades = true;       // Reduces shimmering on camera movement
    bool cull_front_faces = false;        // Cull front faces for Peter Panning reduction
    bool blend_cascade_regions = true;    // Smooth cascade transitions
    bool visualize_cascades = false;      // Debug cascade visualization

    /// Factory: default configuration
    [[nodiscard]] static ShadowConfig default_config();

    /// Factory: high quality configuration
    [[nodiscard]] static ShadowConfig high_quality();

    /// Factory: performance-focused configuration
    [[nodiscard]] static ShadowConfig performance();

    /// Factory: low quality (alias for performance)
    [[nodiscard]] static ShadowConfig low() { return performance(); }

    /// Factory: high (alias for high_quality)
    [[nodiscard]] static ShadowConfig high() { return high_quality(); }

    /// Factory: ultra quality
    [[nodiscard]] static ShadowConfig ultra() {
        ShadowConfig cfg = high_quality();
        cfg.quality = ShadowQuality::Ultra;
        cfg.resolution = 4096;
        cfg.filter_mode = ShadowFilterMode::Pcss;
        cfg.pcf_samples = 32;
        cfg.pcss_blocker_search_samples = 32;
        return cfg;
    }
};

// =============================================================================
// CascadeData (GPU-ready)
// =============================================================================

/// Per-cascade shadow data
struct CascadeData {
    glm::mat4 view_projection;    // Light space view-projection matrix
    float split_depth = 0.0f;     // Split distance from camera
    float texel_size = 0.0f;      // Texel size for bias calculation
    std::uint32_t cascade_index = 0;
    float _pad = 0.0f;
};

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
// CascadedShadowMap
// =============================================================================

/// Manages cascaded shadow maps for directional lights
class CascadedShadowMap {
public:
    CascadedShadowMap();
    ~CascadedShadowMap();

    // Non-copyable
    CascadedShadowMap(const CascadedShadowMap&) = delete;
    CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;

    // Movable
    CascadedShadowMap(CascadedShadowMap&&) noexcept = default;
    CascadedShadowMap& operator=(CascadedShadowMap&&) noexcept = default;

    /// Initialize GPU resources
    [[nodiscard]] bool initialize(const ShadowConfig& config);

    /// Release GPU resources
    void destroy();

    /// Update cascades for camera and light
    void update(const glm::mat4& view, const glm::mat4& projection,
                float near_plane, float far_plane,
                const glm::vec3& light_direction);

    /// Begin rendering to a cascade
    void begin_shadow_pass(std::uint32_t cascade_index);

    /// End shadow pass
    void end_shadow_pass();

    /// Bind shadow map texture
    void bind_shadow_map(std::uint32_t texture_unit) const;

    /// Get cascade count
    [[nodiscard]] std::uint32_t cascade_count() const noexcept {
        return m_config.cascade_count;
    }

    /// Get cascade data for shaders
    [[nodiscard]] const std::vector<CascadeData>& cascade_data() const noexcept {
        return m_cascade_data;
    }

    /// Get shadow map texture handle
    [[nodiscard]] std::uint32_t shadow_map_texture() const noexcept {
        return m_shadow_map;
    }

    /// Get config
    [[nodiscard]] const ShadowConfig& config() const noexcept { return m_config; }

private:
    void calculate_cascade_splits(float near_plane, float far_plane);

    [[nodiscard]] std::array<glm::vec3, 8> get_frustum_corners_world_space(
        const glm::mat4& view, const glm::mat4& projection,
        float near_plane, float far_plane) const;

private:
    ShadowConfig m_config;
    std::uint32_t m_shadow_map = 0;                    // GL texture array
    std::vector<std::uint32_t> m_framebuffers;         // Per-cascade framebuffers
    std::vector<CascadeData> m_cascade_data;           // Per-cascade matrices/data
    std::vector<float> m_cascade_splits;               // Split distances
};

// =============================================================================
// ShadowAtlas
// =============================================================================

/// Shadow map atlas for point and spot lights
class ShadowAtlas {
public:
    /// Tile allocation in the atlas
    struct Allocation {
        bool allocated = false;
        std::uint32_t light_id = 0;
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        glm::vec4 uv_rect{0, 0, 0, 0};  // UV coordinates in atlas
    };

    ShadowAtlas();
    ~ShadowAtlas();

    // Non-copyable
    ShadowAtlas(const ShadowAtlas&) = delete;
    ShadowAtlas& operator=(const ShadowAtlas&) = delete;

    /// Initialize atlas
    [[nodiscard]] bool initialize(std::uint32_t size, std::uint32_t max_lights);

    /// Release GPU resources
    void destroy();

    /// Allocate a tile for a light
    [[nodiscard]] std::optional<Allocation> allocate(std::uint32_t light_id);

    /// Release a light's allocation
    void release(std::uint32_t light_id);

    /// Begin rendering to an allocation
    void begin_render(const Allocation& alloc);

    /// End rendering
    void end_render();

    /// Bind atlas texture
    void bind(std::uint32_t texture_unit) const;

    /// Get atlas size
    [[nodiscard]] std::uint32_t size() const noexcept { return m_atlas_size; }

    /// Get tile size
    [[nodiscard]] std::uint32_t tile_size() const noexcept { return m_tile_size; }

private:
    std::uint32_t m_atlas_size = 0;
    std::uint32_t m_max_lights = 0;
    std::uint32_t m_tile_size = 0;
    std::uint32_t m_atlas_texture = 0;
    std::uint32_t m_framebuffer = 0;
    std::vector<Allocation> m_allocations;
};

// =============================================================================
// ShadowManager
// =============================================================================

/// Unified shadow management (cascaded + atlas + ray-traced)
class ShadowManager {
public:
    ShadowManager();
    ~ShadowManager();

    // Non-copyable
    ShadowManager(const ShadowManager&) = delete;
    ShadowManager& operator=(const ShadowManager&) = delete;

    /// Initialize shadow system
    [[nodiscard]] bool initialize(const ShadowConfig& config);

    /// Shutdown shadow system
    void shutdown();

    /// Update shadow maps for current frame
    void update(const glm::mat4& camera_view,
                const glm::mat4& camera_projection,
                float near_plane, float far_plane,
                const glm::vec3& sun_direction);

    /// Begin directional light shadow pass for cascade
    void begin_directional_shadow_pass(std::uint32_t cascade);

    /// End directional light shadow pass
    void end_directional_shadow_pass();

    /// Get cascade view-projection matrix
    [[nodiscard]] glm::mat4 get_cascade_view_projection(std::uint32_t cascade) const;

    /// Bind shadow maps for rendering
    void bind_shadow_maps(std::uint32_t cascade_unit, std::uint32_t atlas_unit) const;

    /// Get packed cascade data for shader uniforms
    [[nodiscard]] std::vector<glm::vec4> get_cascade_data_packed() const;

    /// Get config
    [[nodiscard]] const ShadowConfig& config() const noexcept { return m_config; }

    /// Get cascaded shadows
    [[nodiscard]] CascadedShadowMap& cascaded_shadows() noexcept {
        return m_cascaded_shadows;
    }
    [[nodiscard]] const CascadedShadowMap& cascaded_shadows() const noexcept {
        return m_cascaded_shadows;
    }

    /// Get shadow atlas
    [[nodiscard]] ShadowAtlas& atlas() noexcept { return m_shadow_atlas; }
    [[nodiscard]] const ShadowAtlas& atlas() const noexcept { return m_shadow_atlas; }

private:
    bool create_depth_shader();

private:
    ShadowConfig m_config;
    CascadedShadowMap m_cascaded_shadows;
    ShadowAtlas m_shadow_atlas;
    std::unique_ptr<ShaderProgram> m_depth_shader;
};

// =============================================================================
// Ray-Traced Shadows (Optional)
// =============================================================================

/// Ray-traced shadow configuration
struct RayTracedShadowConfig {
    bool enabled = false;
    std::uint32_t rays_per_pixel = 1;           // SPP for soft shadows
    float max_ray_distance = 1000.0f;           // Maximum shadow ray length
    float shadow_bias = 0.001f;                 // Ray origin offset
    float soft_shadow_radius = 0.1f;            // Light source radius for soft shadows
    bool use_blue_noise = true;                 // Blue noise sampling
    bool temporal_accumulation = true;          // Accumulate across frames
    std::uint32_t denoiser_iterations = 2;      // Shadow denoiser passes
};

/// Ray structure for ray tracing
struct ShadowRay {
    glm::vec3 origin;
    float t_min;
    glm::vec3 direction;
    float t_max;
};

/// BLAS (Bottom-Level Acceleration Structure) handle
struct BlasHandle {
    std::uint64_t id = 0;
    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
};

/// TLAS (Top-Level Acceleration Structure) handle
struct TlasHandle {
    std::uint64_t id = 0;
    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
};

/// Acceleration structure geometry description
struct AccelerationStructureGeometry {
    const float* vertices = nullptr;
    std::uint32_t vertex_count = 0;
    std::uint32_t vertex_stride = 0;
    const std::uint32_t* indices = nullptr;
    std::uint32_t index_count = 0;
    bool opaque = true;
};

/// Instance for TLAS
struct AccelerationStructureInstance {
    BlasHandle blas;
    glm::mat4 transform;
    std::uint32_t instance_id = 0;
    std::uint32_t mask = 0xFF;
    bool visible = true;
};

/// Ray-traced shadow renderer (RTX/DXR support)
class RayTracedShadowRenderer {
public:
    RayTracedShadowRenderer();
    ~RayTracedShadowRenderer();

    // Non-copyable
    RayTracedShadowRenderer(const RayTracedShadowRenderer&) = delete;
    RayTracedShadowRenderer& operator=(const RayTracedShadowRenderer&) = delete;

    /// Initialize ray-traced shadows
    [[nodiscard]] bool initialize(const RayTracedShadowConfig& config,
                                   std::uint32_t width, std::uint32_t height);

    /// Shutdown and release resources
    void shutdown();

    /// Check if ray tracing is supported
    [[nodiscard]] bool is_supported() const noexcept { return m_rt_supported; }

    /// Build BLAS for mesh geometry
    [[nodiscard]] BlasHandle build_blas(const AccelerationStructureGeometry& geometry);

    /// Destroy BLAS
    void destroy_blas(BlasHandle handle);

    /// Build TLAS from instances
    [[nodiscard]] bool build_tlas(const std::vector<AccelerationStructureInstance>& instances);

    /// Update TLAS (for dynamic scenes)
    void update_tlas();

    /// Trace shadow rays for directional light
    void trace_directional_shadows(const glm::vec3& light_direction,
                                    const glm::mat4& view_projection,
                                    std::uint32_t depth_texture);

    /// Trace shadow rays for point light
    void trace_point_shadows(const glm::vec3& light_position,
                              float light_radius,
                              const glm::mat4& view_projection,
                              std::uint32_t depth_texture);

    /// Get shadow output texture
    [[nodiscard]] std::uint32_t shadow_texture() const noexcept {
        return m_shadow_texture;
    }

    /// Get config
    [[nodiscard]] const RayTracedShadowConfig& config() const noexcept {
        return m_config;
    }

private:
    bool check_raytracing_support();
    bool create_shadow_texture();
    void destroy_shadow_texture();
    bool create_rt_pipeline();
    void destroy_rt_pipeline();
    void create_blue_noise_texture();
    void create_temporal_resources();
    void destroy_temporal_resources();
    void destroy_acceleration_structures();

    void bind_rt_pipeline();
    void set_light_direction(const glm::vec3& dir);
    void set_light_position(const glm::vec3& pos, float radius);
    void set_view_projection(const glm::mat4& vp);
    void bind_depth_texture(std::uint32_t tex);
    void dispatch_rays(std::uint32_t width, std::uint32_t height, std::uint32_t depth);
    void apply_temporal_filter();
    void apply_denoiser();
    void denoise_pass(std::uint32_t iteration);

private:
    struct BlasData {
        std::uint32_t vertex_count = 0;
        std::uint32_t index_count = 0;
        bool opaque = true;
    };

    RayTracedShadowConfig m_config;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    bool m_rt_supported = false;
    bool m_tlas_dirty = false;

    std::uint32_t m_shadow_texture = 0;
    std::uint32_t m_history_texture = 0;
    std::uint32_t m_blue_noise_texture = 0;

    std::unordered_map<std::uint64_t, BlasData> m_blas_map;
    std::vector<AccelerationStructureInstance> m_instances;
    std::uint64_t m_next_blas_id = 0;
    std::uint64_t m_frame_count = 0;
};

// =============================================================================
// Point Light Shadow Data (for GPU)
// =============================================================================

/// Point light shadow map data (cube map - 6 faces)
struct alignas(16) GpuPointShadowData {
    std::array<std::array<std::array<float, 4>, 4>, 6> face_matrices;  // 6 face view-proj matrices
    std::array<float, 3> light_position;
    float light_range;
    std::array<float, 4> shadow_params;  // bias, normal_bias, unused, unused

    /// Size in bytes
    static constexpr std::size_t SIZE = 416;
};

// =============================================================================
// Spot Light Shadow Data (for GPU)
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

} // namespace void_render
