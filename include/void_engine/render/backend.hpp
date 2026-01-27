#pragma once

/// @file backend.hpp
/// @brief Multi-backend GPU abstraction layer for void_render
///
/// Provides a unified interface for multiple graphics APIs:
/// - Vulkan (primary, high-performance)
/// - OpenGL (fallback, compatibility)
/// - Metal (macOS native)
/// - Direct3D 12 (Windows native)
/// - WebGPU (cross-platform, WASM)
///
/// Architecture:
/// - void_render:: contains top-level enums (GpuBackend, DisplayBackend) and BackendManager
/// - void_render::gpu:: contains low-level RHI types (handles, descriptors, interfaces)
///
/// This separation allows coexistence with higher-level types in resource.hpp/texture.hpp
///
/// Based on legacy Rust void_presenter/void_compositor architecture

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <optional>
#include <functional>
#include <variant>
#include <unordered_map>

namespace void_render {

// =============================================================================
// Top-Level Backend Enums (void_render namespace)
// =============================================================================

/// Graphics API backend type
enum class GpuBackend : std::uint8_t {
    Auto = 0,       // Auto-select best available
    Vulkan,         // Vulkan 1.2+ (primary)
    OpenGL,         // OpenGL 4.5+ / OpenGL ES 3.2
    Metal,          // Metal 2.0+ (macOS/iOS)
    Direct3D12,     // D3D12 (Windows 10+)
    WebGPU,         // WebGPU (WASM, cross-platform)
    Null            // Null backend (headless/testing)
};

/// Display/window backend type
enum class DisplayBackend : std::uint8_t {
    Auto = 0,       // Auto-select best available
    Drm,            // DRM/KMS direct GPU access (Linux)
    Wayland,        // Wayland compositor (Linux)
    X11,            // X11 display server (Linux)
    Win32,          // Win32 window (Windows)
    Cocoa,          // Cocoa/AppKit (macOS)
    Web,            // HTML5 Canvas (WASM)
    Headless        // No display output
};

/// Backend selection strategy
enum class BackendSelector : std::uint8_t {
    Auto = 0,       // Auto-detect and select best
    Prefer,         // Prefer specified, fallback if unavailable
    Require         // Require specified or fail
};

/// Convert backend to string
[[nodiscard]] inline const char* gpu_backend_name(GpuBackend backend) noexcept {
    switch (backend) {
        case GpuBackend::Auto: return "Auto";
        case GpuBackend::Vulkan: return "Vulkan";
        case GpuBackend::OpenGL: return "OpenGL";
        case GpuBackend::Metal: return "Metal";
        case GpuBackend::Direct3D12: return "Direct3D12";
        case GpuBackend::WebGPU: return "WebGPU";
        case GpuBackend::Null: return "Null";
        default: return "Unknown";
    }
}

[[nodiscard]] inline const char* display_backend_name(DisplayBackend backend) noexcept {
    switch (backend) {
        case DisplayBackend::Auto: return "Auto";
        case DisplayBackend::Drm: return "DRM/KMS";
        case DisplayBackend::Wayland: return "Wayland";
        case DisplayBackend::X11: return "X11";
        case DisplayBackend::Win32: return "Win32";
        case DisplayBackend::Cocoa: return "Cocoa";
        case DisplayBackend::Web: return "Web";
        case DisplayBackend::Headless: return "Headless";
        default: return "Unknown";
    }
}

// =============================================================================
// Low-Level GPU Abstraction Types (void_render::gpu namespace)
// =============================================================================

/// Low-level GPU abstraction types (RHI-equivalent)
/// Use void_render::gpu:: types for direct GPU resource management
namespace gpu {

// =============================================================================
// Backend Capabilities
// =============================================================================

/// GPU feature flags
struct GpuFeatures {
    bool compute_shaders = false;
    bool tessellation = false;
    bool geometry_shaders = false;
    bool ray_tracing = false;
    bool mesh_shaders = false;
    bool variable_rate_shading = false;
    bool bindless_resources = false;
    bool sparse_textures = false;
    bool multi_draw_indirect = false;
    bool sampler_anisotropy = false;
    bool texture_compression_bc = false;
    bool texture_compression_astc = false;
    bool depth_clamp = false;
    bool fill_mode_non_solid = false;
    bool wide_lines = false;
    bool large_points = false;
    bool multi_viewport = false;
    bool sampler_mirror_clamp = false;
    bool shader_float64 = false;
    bool shader_int64 = false;
    bool shader_int16 = false;
    bool descriptor_indexing = false;
    bool buffer_device_address = false;
    bool timeline_semaphores = false;
    bool dynamic_rendering = false;
    bool maintenance4 = false;
};

/// GPU limits
struct GpuLimits {
    std::uint32_t max_texture_size_1d = 16384;
    std::uint32_t max_texture_size_2d = 16384;
    std::uint32_t max_texture_size_3d = 2048;
    std::uint32_t max_texture_size_cube = 16384;
    std::uint32_t max_texture_array_layers = 2048;
    std::uint32_t max_uniform_buffer_size = 65536;
    std::uint32_t max_storage_buffer_size = 134217728;
    std::uint32_t max_push_constant_size = 256;
    std::uint32_t max_bind_groups = 4;
    std::uint32_t max_bindings_per_group = 1000;
    std::uint32_t max_vertex_attributes = 16;
    std::uint32_t max_vertex_buffers = 8;
    std::uint32_t max_vertex_buffer_stride = 2048;
    std::uint32_t max_color_attachments = 8;
    std::uint32_t max_compute_workgroup_size_x = 1024;
    std::uint32_t max_compute_workgroup_size_y = 1024;
    std::uint32_t max_compute_workgroup_size_z = 64;
    std::uint32_t max_compute_workgroups_per_dimension = 65535;
    std::uint32_t max_sampled_textures_per_stage = 16;
    std::uint32_t max_samplers_per_stage = 16;
    std::uint32_t max_storage_textures_per_stage = 8;
    std::uint32_t max_storage_buffers_per_stage = 8;
    std::uint32_t max_uniform_buffers_per_stage = 12;
    float max_sampler_anisotropy = 16.0f;
    std::uint64_t max_buffer_size = 268435456;
};

/// Display capabilities
struct DisplayCapabilities {
    DisplayBackend backend_type = DisplayBackend::Headless;
    bool vrr_supported = false;           // Variable Refresh Rate
    bool hdr_supported = false;           // HDR output
    bool multi_output = false;            // Multiple displays
    bool hardware_cursor = false;         // Hardware cursor support
    bool direct_scanout = false;          // Direct GPU-to-display
    bool fullscreen_exclusive = false;    // Exclusive fullscreen
    std::uint32_t max_refresh_rate = 60;
    std::uint32_t max_width = 7680;       // 8K
    std::uint32_t max_height = 4320;
    std::vector<std::array<std::uint32_t, 2>> supported_resolutions;
    std::vector<std::uint32_t> supported_refresh_rates;
};

/// Combined backend capabilities
struct BackendCapabilities {
    GpuBackend gpu_backend = GpuBackend::Null;
    DisplayBackend display_backend = DisplayBackend::Headless;
    std::string device_name;
    std::string driver_version;
    std::uint32_t vendor_id = 0;
    std::uint32_t device_id = 0;
    GpuFeatures features;
    GpuLimits limits;
    DisplayCapabilities display;
};

// =============================================================================
// Backend Configuration
// =============================================================================

/// Backend initialization configuration
struct BackendConfig {
    GpuBackend preferred_gpu_backend = GpuBackend::Auto;
    DisplayBackend preferred_display_backend = DisplayBackend::Auto;
    BackendSelector gpu_selector = BackendSelector::Auto;
    BackendSelector display_selector = BackendSelector::Auto;

    bool enable_validation = false;       // GPU validation/debug layers
    bool enable_gpu_profiling = false;    // GPU profiler support
    bool enable_api_capture = false;      // RenderDoc/PIX capture
    bool power_preference_low = false;    // Prefer integrated GPU
    bool require_discrete_gpu = false;    // Require dedicated GPU

    std::uint32_t initial_width = 1920;
    std::uint32_t initial_height = 1080;
    std::uint32_t target_refresh_rate = 60;
    bool vsync = true;
    bool hdr_enabled = false;
    bool vrr_enabled = false;

    std::string window_title = "void_engine";
    bool resizable = true;
    bool fullscreen = false;
    bool borderless = false;
};

// =============================================================================
// GPU Resource Handles
// =============================================================================

/// Opaque handle for GPU resources
template<typename Tag>
struct GpuHandle {
    std::uint64_t id = 0;

    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
    [[nodiscard]] static GpuHandle invalid() noexcept { return GpuHandle{0}; }

    bool operator==(const GpuHandle& other) const noexcept = default;
};

// Handle types
struct BufferTag {};
struct TextureTag {};
struct SamplerTag {};
struct ShaderModuleTag {};
struct PipelineTag {};
struct BindGroupTag {};
struct BindGroupLayoutTag {};
struct RenderPassTag {};
struct CommandBufferTag {};
struct FenceTag {};
struct SemaphoreTag {};
struct QueryPoolTag {};

using BufferHandle = GpuHandle<BufferTag>;
using TextureHandle = GpuHandle<TextureTag>;
using SamplerHandle = GpuHandle<SamplerTag>;
using ShaderModuleHandle = GpuHandle<ShaderModuleTag>;
using PipelineHandle = GpuHandle<PipelineTag>;
using BindGroupHandle = GpuHandle<BindGroupTag>;
using BindGroupLayoutHandle = GpuHandle<BindGroupLayoutTag>;
using RenderPassHandle = GpuHandle<RenderPassTag>;
using CommandBufferHandle = GpuHandle<CommandBufferTag>;
using FenceHandle = GpuHandle<FenceTag>;
using SemaphoreHandle = GpuHandle<SemaphoreTag>;
using QueryPoolHandle = GpuHandle<QueryPoolTag>;

// =============================================================================
// Resource Descriptions
// =============================================================================

/// Buffer usage flags
enum class BufferUsage : std::uint32_t {
    None = 0,
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    Indirect = 1 << 4,
    TransferSrc = 1 << 5,
    TransferDst = 1 << 6,
    QueryResolve = 1 << 7
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline bool operator&(BufferUsage a, BufferUsage b) {
    return (static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)) != 0;
}

/// Texture usage flags
enum class TextureUsage : std::uint32_t {
    None = 0,
    Sampled = 1 << 0,
    Storage = 1 << 1,
    RenderAttachment = 1 << 2,
    TransferSrc = 1 << 3,
    TransferDst = 1 << 4
};

inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

/// Texture format
enum class TextureFormat : std::uint16_t {
    // 8-bit formats
    R8Unorm, R8Snorm, R8Uint, R8Sint,
    // 16-bit formats
    R16Uint, R16Sint, R16Float,
    Rg8Unorm, Rg8Snorm, Rg8Uint, Rg8Sint,
    // 32-bit formats
    R32Uint, R32Sint, R32Float,
    Rg16Uint, Rg16Sint, Rg16Float,
    Rgba8Unorm, Rgba8UnormSrgb, Rgba8Snorm, Rgba8Uint, Rgba8Sint,
    Bgra8Unorm, Bgra8UnormSrgb,
    Rgb10a2Unorm,
    Rg11b10Float,
    // 64-bit formats
    Rg32Uint, Rg32Sint, Rg32Float,
    Rgba16Uint, Rgba16Sint, Rgba16Float,
    // 128-bit formats
    Rgba32Uint, Rgba32Sint, Rgba32Float,
    // Depth/stencil formats
    Depth16Unorm,
    Depth24Plus,
    Depth24PlusStencil8,
    Depth32Float,
    Depth32FloatStencil8,
    Stencil8,
    // Compressed formats (BC)
    Bc1RgbaUnorm, Bc1RgbaUnormSrgb,
    Bc2RgbaUnorm, Bc2RgbaUnormSrgb,
    Bc3RgbaUnorm, Bc3RgbaUnormSrgb,
    Bc4RUnorm, Bc4RSnorm,
    Bc5RgUnorm, Bc5RgSnorm,
    Bc6hRgbUfloat, Bc6hRgbFloat,
    Bc7RgbaUnorm, Bc7RgbaUnormSrgb,
    // Compressed formats (ASTC)
    Astc4x4Unorm, Astc4x4UnormSrgb,
    Astc5x4Unorm, Astc5x4UnormSrgb,
    Astc5x5Unorm, Astc5x5UnormSrgb,
    Astc6x5Unorm, Astc6x5UnormSrgb,
    Astc6x6Unorm, Astc6x6UnormSrgb,
    Astc8x5Unorm, Astc8x5UnormSrgb,
    Astc8x6Unorm, Astc8x6UnormSrgb,
    Astc8x8Unorm, Astc8x8UnormSrgb,
    Astc10x5Unorm, Astc10x5UnormSrgb,
    Astc10x6Unorm, Astc10x6UnormSrgb,
    Astc10x8Unorm, Astc10x8UnormSrgb,
    Astc10x10Unorm, Astc10x10UnormSrgb,
    Astc12x10Unorm, Astc12x10UnormSrgb,
    Astc12x12Unorm, Astc12x12UnormSrgb
};

/// Texture dimension
enum class TextureDimension : std::uint8_t {
    D1,
    D2,
    D3,
    Cube,
    D2Array,
    CubeArray
};

/// Buffer description
struct BufferDesc {
    std::uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    bool mapped_at_creation = false;
    std::string label;
};

/// Texture description
struct TextureDesc {
    std::uint32_t width = 1;
    std::uint32_t height = 1;
    std::uint32_t depth_or_layers = 1;
    std::uint32_t mip_levels = 1;
    std::uint32_t sample_count = 1;
    TextureDimension dimension = TextureDimension::D2;
    TextureFormat format = TextureFormat::Rgba8Unorm;
    TextureUsage usage = TextureUsage::Sampled;
    std::string label;
};

/// Sampler description
struct SamplerDesc {
    enum class Filter : std::uint8_t { Nearest, Linear };
    enum class AddressMode : std::uint8_t { Repeat, MirrorRepeat, ClampToEdge, ClampToBorder };
    enum class CompareFunction : std::uint8_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };

    Filter min_filter = Filter::Linear;
    Filter mag_filter = Filter::Linear;
    Filter mipmap_filter = Filter::Linear;
    AddressMode address_mode_u = AddressMode::Repeat;
    AddressMode address_mode_v = AddressMode::Repeat;
    AddressMode address_mode_w = AddressMode::Repeat;
    float lod_min_clamp = 0.0f;
    float lod_max_clamp = 1000.0f;
    float max_anisotropy = 1.0f;
    std::optional<CompareFunction> compare;
    std::string label;
};

// =============================================================================
// Shader & Pipeline
// =============================================================================

/// Shader stage
enum class ShaderStage : std::uint8_t {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessControl,
    TessEvaluation,
    Mesh,
    Task,
    RayGeneration,
    RayMiss,
    RayClosestHit,
    RayAnyHit,
    RayIntersection
};

/// Shader module description
struct ShaderModuleDesc {
    std::vector<std::uint32_t> spirv;  // SPIR-V bytecode
    std::string entry_point = "main";
    ShaderStage stage = ShaderStage::Vertex;
    std::string label;
};

/// Vertex attribute format
enum class VertexFormat : std::uint8_t {
    Float32, Float32x2, Float32x3, Float32x4,
    Sint32, Sint32x2, Sint32x3, Sint32x4,
    Uint32, Uint32x2, Uint32x3, Uint32x4,
    Float16x2, Float16x4,
    Sint16x2, Sint16x4,
    Uint16x2, Uint16x4,
    Snorm16x2, Snorm16x4,
    Unorm16x2, Unorm16x4,
    Sint8x2, Sint8x4,
    Uint8x2, Uint8x4,
    Snorm8x2, Snorm8x4,
    Unorm8x2, Unorm8x4,
    Unorm10_10_10_2
};

/// Vertex buffer layout
struct VertexBufferLayout {
    std::uint32_t stride = 0;
    bool instanced = false;
    struct Attribute {
        VertexFormat format;
        std::uint32_t offset;
        std::uint32_t shader_location;
    };
    std::vector<Attribute> attributes;
};

/// Primitive topology
enum class PrimitiveTopology : std::uint8_t {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip
};

/// Front face winding
enum class FrontFace : std::uint8_t {
    Ccw,  // Counter-clockwise
    Cw    // Clockwise
};

/// Cull mode
enum class CullMode : std::uint8_t {
    None,
    Front,
    Back
};

/// Polygon mode
enum class PolygonMode : std::uint8_t {
    Fill,
    Line,
    Point
};

/// Blend factor
enum class BlendFactor : std::uint8_t {
    Zero, One,
    Src, OneMinusSrc, SrcAlpha, OneMinusSrcAlpha,
    Dst, OneMinusDst, DstAlpha, OneMinusDstAlpha,
    SrcAlphaSaturated,
    Constant, OneMinusConstant
};

/// Blend operation
enum class BlendOp : std::uint8_t {
    Add, Subtract, ReverseSubtract, Min, Max
};

/// Compare function
enum class CompareOp : std::uint8_t {
    Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always
};

/// Stencil operation
enum class StencilOp : std::uint8_t {
    Keep, Zero, Replace, IncrementClamp, DecrementClamp, Invert, IncrementWrap, DecrementWrap
};

/// Blend state
struct BlendState {
    BlendFactor src_factor = BlendFactor::One;
    BlendFactor dst_factor = BlendFactor::Zero;
    BlendOp operation = BlendOp::Add;
};

/// Color target state
struct ColorTargetState {
    TextureFormat format = TextureFormat::Bgra8Unorm;
    std::optional<BlendState> blend_color;
    std::optional<BlendState> blend_alpha;
    std::uint8_t write_mask = 0xF;  // RGBA
};

/// Depth stencil state
struct DepthStencilState {
    TextureFormat format = TextureFormat::Depth24Plus;
    bool depth_write_enabled = true;
    CompareOp depth_compare = CompareOp::Less;

    struct StencilFaceState {
        CompareOp compare = CompareOp::Always;
        StencilOp fail_op = StencilOp::Keep;
        StencilOp depth_fail_op = StencilOp::Keep;
        StencilOp pass_op = StencilOp::Keep;
    };

    StencilFaceState stencil_front;
    StencilFaceState stencil_back;
    std::uint32_t stencil_read_mask = 0xFFFFFFFF;
    std::uint32_t stencil_write_mask = 0xFFFFFFFF;
    std::int32_t depth_bias = 0;
    float depth_bias_slope_scale = 0.0f;
    float depth_bias_clamp = 0.0f;
};

/// Multisample state
struct MultisampleState {
    std::uint32_t count = 1;
    std::uint32_t mask = 0xFFFFFFFF;
    bool alpha_to_coverage_enabled = false;
};

/// Render pipeline description
struct RenderPipelineDesc {
    ShaderModuleHandle vertex_shader;
    ShaderModuleHandle fragment_shader;
    std::vector<VertexBufferLayout> vertex_buffers;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    FrontFace front_face = FrontFace::Ccw;
    CullMode cull_mode = CullMode::Back;
    PolygonMode polygon_mode = PolygonMode::Fill;
    bool unclipped_depth = false;
    bool conservative_rasterization = false;
    std::optional<DepthStencilState> depth_stencil;
    MultisampleState multisample;
    std::vector<ColorTargetState> color_targets;
    BindGroupLayoutHandle bind_group_layouts[4] = {};
    std::string label;
};

/// Compute pipeline description
struct ComputePipelineDesc {
    ShaderModuleHandle compute_shader;
    BindGroupLayoutHandle bind_group_layouts[4] = {};
    std::string label;
};

// =============================================================================
// Rehydration State (for hot-swapping)
// =============================================================================

/// Serializable presenter state for hot-reload
struct RehydrationState {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool fullscreen = false;
    bool vsync = true;
    std::array<float, 4> clear_color = {0, 0, 0, 1};
    std::uint64_t frame_count = 0;

    // Opaque backend-specific state
    std::vector<std::uint8_t> backend_data;
};

// =============================================================================
// Backend Interface
// =============================================================================

/// Frame timing info
struct FrameTiming {
    double gpu_time_ms = 0;
    double cpu_time_ms = 0;
    double present_time_ms = 0;
    std::uint64_t frame_number = 0;
};

/// Backend error codes
enum class BackendError {
    None = 0,
    NotInitialized,
    AlreadyInitialized,
    UnsupportedBackend,
    DeviceLost,
    OutOfMemory,
    InvalidHandle,
    InvalidParameter,
    ShaderCompilationFailed,
    PipelineCreationFailed,
    SurfaceLost,
    Timeout,
    Unknown
};

/// GPU Backend interface - abstract base for all backends
class IGpuBackend {
public:
    virtual ~IGpuBackend() = default;

    // Lifecycle
    virtual BackendError init(const BackendConfig& config) = 0;
    virtual void shutdown() = 0;
    [[nodiscard]] virtual bool is_initialized() const = 0;

    // Capabilities
    [[nodiscard]] virtual GpuBackend backend_type() const = 0;
    [[nodiscard]] virtual const BackendCapabilities& capabilities() const = 0;

    // Resource creation
    [[nodiscard]] virtual BufferHandle create_buffer(const BufferDesc& desc) = 0;
    [[nodiscard]] virtual TextureHandle create_texture(const TextureDesc& desc) = 0;
    [[nodiscard]] virtual SamplerHandle create_sampler(const SamplerDesc& desc) = 0;
    [[nodiscard]] virtual ShaderModuleHandle create_shader_module(const ShaderModuleDesc& desc) = 0;
    [[nodiscard]] virtual PipelineHandle create_render_pipeline(const RenderPipelineDesc& desc) = 0;
    [[nodiscard]] virtual PipelineHandle create_compute_pipeline(const ComputePipelineDesc& desc) = 0;

    // Resource destruction
    virtual void destroy_buffer(BufferHandle handle) = 0;
    virtual void destroy_texture(TextureHandle handle) = 0;
    virtual void destroy_sampler(SamplerHandle handle) = 0;
    virtual void destroy_shader_module(ShaderModuleHandle handle) = 0;
    virtual void destroy_pipeline(PipelineHandle handle) = 0;

    // Buffer operations
    virtual void write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) = 0;
    [[nodiscard]] virtual void* map_buffer(BufferHandle handle, std::size_t offset, std::size_t size) = 0;
    virtual void unmap_buffer(BufferHandle handle) = 0;

    // Texture operations
    virtual void write_texture(TextureHandle handle, const void* data, std::size_t size,
                               std::uint32_t mip_level = 0, std::uint32_t array_layer = 0) = 0;
    virtual void generate_mipmaps(TextureHandle handle) = 0;

    // Frame management
    virtual BackendError begin_frame() = 0;
    virtual BackendError end_frame() = 0;
    virtual void present() = 0;
    virtual void wait_idle() = 0;

    // Resize
    virtual void resize(std::uint32_t width, std::uint32_t height) = 0;

    // Hot-reload support (SACRED patterns)
    [[nodiscard]] virtual RehydrationState get_rehydration_state() const = 0;
    virtual BackendError rehydrate(const RehydrationState& state) = 0;

    // Statistics
    [[nodiscard]] virtual FrameTiming get_frame_timing() const = 0;
    [[nodiscard]] virtual std::uint64_t get_allocated_memory() const = 0;
};

// =============================================================================
// Presenter Interface (display output abstraction)
// =============================================================================

using PresenterId = std::uint32_t;

/// Presenter capabilities
struct PresenterCapabilities {
    DisplayBackend backend = DisplayBackend::Headless;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t refresh_rate = 60;
    TextureFormat surface_format = TextureFormat::Bgra8Unorm;
    bool vrr = false;
    bool hdr = false;
    bool supports_resize = true;
    bool supports_fullscreen = true;
};

/// Presenter interface - manages display surface
class IPresenter {
public:
    virtual ~IPresenter() = default;

    [[nodiscard]] virtual PresenterId id() const = 0;
    [[nodiscard]] virtual const PresenterCapabilities& capabilities() const = 0;

    virtual BackendError resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual BackendError set_fullscreen(bool fullscreen) = 0;
    virtual BackendError set_vsync(bool vsync) = 0;

    [[nodiscard]] virtual TextureHandle acquire_next_texture() = 0;
    virtual void present(TextureHandle texture) = 0;

    // Hot-reload support (SACRED patterns)
    [[nodiscard]] virtual RehydrationState get_rehydration_state() const = 0;
    virtual BackendError rehydrate(const RehydrationState& state) = 0;
};

// =============================================================================
// Backend Factory Functions
// =============================================================================

/// Backend detection result
struct BackendAvailability {
    GpuBackend gpu_backend;
    bool available = false;
    std::string reason;  // Empty if available, error message if not
};

/// Detect available backends
[[nodiscard]] std::vector<BackendAvailability> detect_available_backends();

/// Select best backend based on config
[[nodiscard]] GpuBackend select_gpu_backend(const BackendConfig& config,
                                             const std::vector<BackendAvailability>& available);

/// Create backend instance
[[nodiscard]] std::unique_ptr<IGpuBackend> create_backend(GpuBackend backend);

/// Create presenter instance
[[nodiscard]] std::unique_ptr<IPresenter> create_presenter(DisplayBackend backend,
                                                            IGpuBackend* gpu_backend,
                                                            const BackendConfig& config);

} // namespace gpu

// =============================================================================
// Backend Manager (void_render namespace - coordinates GPU backend + presenters)
// =============================================================================

/// BackendManager coordinates GPU backends and presenters with hot-swap support
///
/// Features:
/// - Runtime backend switching (Vulkan <-> OpenGL <-> D3D12)
/// - Multi-display presenter management
/// - State preservation during hot-swap via RehydrationState
/// - SACRED hot-reload pattern support
class BackendManager {
public:
    BackendManager() = default;
    ~BackendManager();

    // Non-copyable
    BackendManager(const BackendManager&) = delete;
    BackendManager& operator=(const BackendManager&) = delete;

    /// Initialize with configuration
    gpu::BackendError init(const gpu::BackendConfig& config);

    /// Shutdown and release all resources
    void shutdown();

    /// Check if initialized
    [[nodiscard]] bool is_initialized() const noexcept { return m_gpu_backend != nullptr; }

    /// Get GPU backend
    [[nodiscard]] gpu::IGpuBackend* gpu() const noexcept { return m_gpu_backend.get(); }

    /// Get primary presenter
    [[nodiscard]] gpu::IPresenter* presenter() const noexcept {
        return m_presenters.empty() ? nullptr : m_presenters[0].get();
    }

    /// Get presenter by ID
    [[nodiscard]] gpu::IPresenter* get_presenter(gpu::PresenterId id) const;

    /// Add additional presenter (multi-display)
    [[nodiscard]] gpu::PresenterId add_presenter(DisplayBackend backend);

    /// Remove presenter
    void remove_presenter(gpu::PresenterId id);

    /// Get capabilities
    [[nodiscard]] const gpu::BackendCapabilities& capabilities() const;

    /// Begin frame (all presenters)
    gpu::BackendError begin_frame();

    /// End frame (all presenters)
    gpu::BackendError end_frame();

    /// Hot-swap GPU backend at runtime (preserves state)
    /// This is a SACRED operation - state is captured before swap and restored after
    gpu::BackendError hot_swap_backend(GpuBackend new_backend);

    /// Hot-swap presenter (e.g., switch display outputs)
    gpu::BackendError hot_swap_presenter(gpu::PresenterId id, DisplayBackend new_backend);

    // SACRED hot-reload patterns
    [[nodiscard]] gpu::RehydrationState snapshot() const;
    gpu::BackendError restore(const gpu::RehydrationState& state);

private:
    std::unique_ptr<gpu::IGpuBackend> m_gpu_backend;
    std::vector<std::unique_ptr<gpu::IPresenter>> m_presenters;
    gpu::BackendConfig m_config;
    gpu::PresenterId m_next_presenter_id = 1;
};

} // namespace void_render

// Hash specializations
template<typename Tag>
struct std::hash<void_render::gpu::GpuHandle<Tag>> {
    std::size_t operator()(const void_render::gpu::GpuHandle<Tag>& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.id);
    }
};
