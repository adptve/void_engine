#pragma once

/// @file resource.hpp
/// @brief GPU resource types and descriptors for void_render

#include "fwd.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>
#include <array>
#include <functional>
#include <atomic>

namespace void_render {

// =============================================================================
// ResourceId
// =============================================================================

/// Unique resource identifier
struct ResourceId {
    std::uint64_t value = UINT64_MAX;

    constexpr ResourceId() noexcept = default;
    constexpr explicit ResourceId(std::uint64_t v) noexcept : value(v) {}

    /// Create from name (deterministic hash) - alias for from_name
    [[nodiscard]] static ResourceId from_hash(std::string_view name) noexcept {
        // FNV-1a hash
        std::uint64_t hash = 14695981039346656037ULL;
        for (char c : name) {
            hash ^= static_cast<std::uint64_t>(c);
            hash *= 1099511628211ULL;
        }
        return ResourceId(hash);
    }

    /// Create from name (deterministic hash)
    [[nodiscard]] static ResourceId from_name(std::string_view name) noexcept {
        return from_hash(name);
    }

    /// Generate sequential IDs (thread-safe)
    [[nodiscard]] static ResourceId sequential() noexcept {
        static std::atomic<std::uint64_t> counter{0};
        return ResourceId(counter.fetch_add(1, std::memory_order_relaxed));
    }

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value != UINT64_MAX;
    }

    [[nodiscard]] static constexpr ResourceId invalid() noexcept {
        return ResourceId{};
    }

    constexpr bool operator==(const ResourceId& other) const noexcept = default;
    constexpr bool operator<(const ResourceId& other) const noexcept {
        return value < other.value;
    }
};

// =============================================================================
// TextureFormat - 45 format variants
// =============================================================================

/// Texture format enumeration
enum class TextureFormat : std::uint8_t {
    // 8-bit formats
    R8Unorm = 0,
    R8Snorm,
    R8Uint,
    R8Sint,

    // 16-bit formats
    R16Uint,
    R16Sint,
    R16Float,
    Rg8Unorm,
    Rg8Snorm,
    Rg8Uint,
    Rg8Sint,

    // 32-bit formats
    R32Uint,
    R32Sint,
    R32Float,
    Rg16Uint,
    Rg16Sint,
    Rg16Float,
    Rgba8Unorm,
    Rgba8UnormSrgb,
    Rgba8Snorm,
    Rgba8Uint,
    Rgba8Sint,
    Bgra8Unorm,
    Bgra8UnormSrgb,

    // 64-bit formats
    Rg32Uint,
    Rg32Sint,
    Rg32Float,
    Rgba16Uint,
    Rgba16Sint,
    Rgba16Float,

    // 128-bit formats
    Rgba32Uint,
    Rgba32Sint,
    Rgba32Float,

    // Depth/Stencil formats
    Depth16Unorm,
    Depth24Plus,
    Depth24PlusStencil8,
    Depth32Float,
    Depth32FloatStencil8,

    // Compressed formats (BC/DXT)
    Bc1RgbaUnorm,      // DXT1
    Bc1RgbaUnormSrgb,
    Bc2RgbaUnorm,      // DXT3
    Bc2RgbaUnormSrgb,
    Bc3RgbaUnorm,      // DXT5
    Bc3RgbaUnormSrgb,
    Bc4RUnorm,
    Bc4RSnorm,
    Bc5RgUnorm,
    Bc5RgSnorm,
    Bc6hRgbUfloat,
    Bc6hRgbSfloat,
    Bc7RgbaUnorm,
    Bc7RgbaUnormSrgb,

    // Count
    Count
};

/// Check if format is a depth format
[[nodiscard]] constexpr bool is_depth_format(TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::Depth16Unorm:
        case TextureFormat::Depth24Plus:
        case TextureFormat::Depth24PlusStencil8:
        case TextureFormat::Depth32Float:
        case TextureFormat::Depth32FloatStencil8:
            return true;
        default:
            return false;
    }
}

/// Check if format has stencil
[[nodiscard]] constexpr bool has_stencil(TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::Depth24PlusStencil8:
        case TextureFormat::Depth32FloatStencil8:
            return true;
        default:
            return false;
    }
}

/// Check if format is sRGB
[[nodiscard]] constexpr bool is_srgb_format(TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::Rgba8UnormSrgb:
        case TextureFormat::Bgra8UnormSrgb:
        case TextureFormat::Bc1RgbaUnormSrgb:
        case TextureFormat::Bc2RgbaUnormSrgb:
        case TextureFormat::Bc3RgbaUnormSrgb:
        case TextureFormat::Bc7RgbaUnormSrgb:
            return true;
        default:
            return false;
    }
}

/// Check if format is compressed
[[nodiscard]] constexpr bool is_compressed_format(TextureFormat format) noexcept {
    return format >= TextureFormat::Bc1RgbaUnorm && format <= TextureFormat::Bc7RgbaUnormSrgb;
}

/// Get bytes per pixel (0 for compressed formats)
[[nodiscard]] constexpr std::size_t bytes_per_pixel(TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::R8Unorm:
        case TextureFormat::R8Snorm:
        case TextureFormat::R8Uint:
        case TextureFormat::R8Sint:
            return 1;

        case TextureFormat::R16Uint:
        case TextureFormat::R16Sint:
        case TextureFormat::R16Float:
        case TextureFormat::Rg8Unorm:
        case TextureFormat::Rg8Snorm:
        case TextureFormat::Rg8Uint:
        case TextureFormat::Rg8Sint:
        case TextureFormat::Depth16Unorm:
            return 2;

        case TextureFormat::R32Uint:
        case TextureFormat::R32Sint:
        case TextureFormat::R32Float:
        case TextureFormat::Rg16Uint:
        case TextureFormat::Rg16Sint:
        case TextureFormat::Rg16Float:
        case TextureFormat::Rgba8Unorm:
        case TextureFormat::Rgba8UnormSrgb:
        case TextureFormat::Rgba8Snorm:
        case TextureFormat::Rgba8Uint:
        case TextureFormat::Rgba8Sint:
        case TextureFormat::Bgra8Unorm:
        case TextureFormat::Bgra8UnormSrgb:
        case TextureFormat::Depth24Plus:
        case TextureFormat::Depth24PlusStencil8:
        case TextureFormat::Depth32Float:
            return 4;

        case TextureFormat::Rg32Uint:
        case TextureFormat::Rg32Sint:
        case TextureFormat::Rg32Float:
        case TextureFormat::Rgba16Uint:
        case TextureFormat::Rgba16Sint:
        case TextureFormat::Rgba16Float:
        case TextureFormat::Depth32FloatStencil8:
            return 8;

        case TextureFormat::Rgba32Uint:
        case TextureFormat::Rgba32Sint:
        case TextureFormat::Rgba32Float:
            return 16;

        default:
            return 0;  // Compressed formats
    }
}

/// Alias for bytes_per_pixel
[[nodiscard]] constexpr std::size_t texture_format_bytes(TextureFormat format) noexcept {
    return bytes_per_pixel(format);
}

/// Check if format is a stencil format
[[nodiscard]] constexpr bool is_stencil_format(TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::Depth24PlusStencil8:
        case TextureFormat::Depth32FloatStencil8:
            return true;
        default:
            return false;
    }
}

/// Get format name string
[[nodiscard]] inline const char* texture_format_name(TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::R8Unorm: return "R8Unorm";
        case TextureFormat::R8Snorm: return "R8Snorm";
        case TextureFormat::R8Uint: return "R8Uint";
        case TextureFormat::R8Sint: return "R8Sint";
        case TextureFormat::R16Uint: return "R16Uint";
        case TextureFormat::R16Sint: return "R16Sint";
        case TextureFormat::R16Float: return "R16Float";
        case TextureFormat::Rg8Unorm: return "Rg8Unorm";
        case TextureFormat::Rg8Snorm: return "Rg8Snorm";
        case TextureFormat::Rg8Uint: return "Rg8Uint";
        case TextureFormat::Rg8Sint: return "Rg8Sint";
        case TextureFormat::R32Uint: return "R32Uint";
        case TextureFormat::R32Sint: return "R32Sint";
        case TextureFormat::R32Float: return "R32Float";
        case TextureFormat::Rg16Uint: return "Rg16Uint";
        case TextureFormat::Rg16Sint: return "Rg16Sint";
        case TextureFormat::Rg16Float: return "Rg16Float";
        case TextureFormat::Rgba8Unorm: return "Rgba8Unorm";
        case TextureFormat::Rgba8UnormSrgb: return "Rgba8UnormSrgb";
        case TextureFormat::Rgba8Snorm: return "Rgba8Snorm";
        case TextureFormat::Rgba8Uint: return "Rgba8Uint";
        case TextureFormat::Rgba8Sint: return "Rgba8Sint";
        case TextureFormat::Bgra8Unorm: return "Bgra8Unorm";
        case TextureFormat::Bgra8UnormSrgb: return "Bgra8UnormSrgb";
        case TextureFormat::Rg32Uint: return "Rg32Uint";
        case TextureFormat::Rg32Sint: return "Rg32Sint";
        case TextureFormat::Rg32Float: return "Rg32Float";
        case TextureFormat::Rgba16Uint: return "Rgba16Uint";
        case TextureFormat::Rgba16Sint: return "Rgba16Sint";
        case TextureFormat::Rgba16Float: return "Rgba16Float";
        case TextureFormat::Rgba32Uint: return "Rgba32Uint";
        case TextureFormat::Rgba32Sint: return "Rgba32Sint";
        case TextureFormat::Rgba32Float: return "Rgba32Float";
        case TextureFormat::Depth16Unorm: return "Depth16Unorm";
        case TextureFormat::Depth24Plus: return "Depth24Plus";
        case TextureFormat::Depth24PlusStencil8: return "Depth24PlusStencil8";
        case TextureFormat::Depth32Float: return "Depth32Float";
        case TextureFormat::Depth32FloatStencil8: return "Depth32FloatStencil8";
        case TextureFormat::Bc1RgbaUnorm: return "Bc1RgbaUnorm";
        case TextureFormat::Bc1RgbaUnormSrgb: return "Bc1RgbaUnormSrgb";
        case TextureFormat::Bc2RgbaUnorm: return "Bc2RgbaUnorm";
        case TextureFormat::Bc2RgbaUnormSrgb: return "Bc2RgbaUnormSrgb";
        case TextureFormat::Bc3RgbaUnorm: return "Bc3RgbaUnorm";
        case TextureFormat::Bc3RgbaUnormSrgb: return "Bc3RgbaUnormSrgb";
        case TextureFormat::Bc4RUnorm: return "Bc4RUnorm";
        case TextureFormat::Bc4RSnorm: return "Bc4RSnorm";
        case TextureFormat::Bc5RgUnorm: return "Bc5RgUnorm";
        case TextureFormat::Bc5RgSnorm: return "Bc5RgSnorm";
        case TextureFormat::Bc6hRgbUfloat: return "Bc6hRgbUfloat";
        case TextureFormat::Bc6hRgbSfloat: return "Bc6hRgbSfloat";
        case TextureFormat::Bc7RgbaUnorm: return "Bc7RgbaUnorm";
        case TextureFormat::Bc7RgbaUnormSrgb: return "Bc7RgbaUnormSrgb";
        default: return "Unknown";
    }
}

// =============================================================================
// TextureDimension
// =============================================================================

/// Texture dimension
enum class TextureDimension : std::uint8_t {
    D1 = 0,
    D2,
    D3
};

// =============================================================================
// TextureUsage (bitflags)
// =============================================================================

/// Texture usage flags
enum class TextureUsage : std::uint32_t {
    None            = 0,
    CopySrc         = 1 << 0,
    CopyDst         = 1 << 1,
    TextureBinding  = 1 << 2,
    StorageBinding  = 1 << 3,
    RenderAttachment = 1 << 4
};

/// Bitwise OR for TextureUsage
[[nodiscard]] constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) noexcept {
    return static_cast<TextureUsage>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b)
    );
}

/// Bitwise AND for TextureUsage
[[nodiscard]] constexpr TextureUsage operator&(TextureUsage a, TextureUsage b) noexcept {
    return static_cast<TextureUsage>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)
    );
}

/// Bitwise OR assignment
constexpr TextureUsage& operator|=(TextureUsage& a, TextureUsage b) noexcept {
    return a = a | b;
}

/// Check if flag is set
[[nodiscard]] constexpr bool has_flag(TextureUsage flags, TextureUsage flag) noexcept {
    return (flags & flag) == flag;
}

// =============================================================================
// TextureDesc
// =============================================================================

/// Texture descriptor
struct TextureDesc {
    std::string label;
    std::array<std::uint32_t, 3> size = {1, 1, 1};
    std::uint32_t mip_level_count = 1;
    std::uint32_t sample_count = 1;  // MSAA
    TextureDimension dimension = TextureDimension::D2;
    TextureFormat format = TextureFormat::Rgba8Unorm;
    TextureUsage usage = TextureUsage::TextureBinding;

    /// Create 2D texture descriptor
    [[nodiscard]] static TextureDesc texture_2d(
        std::uint32_t width, std::uint32_t height,
        TextureFormat fmt, TextureUsage use = TextureUsage::TextureBinding) {
        TextureDesc desc;
        desc.size = {width, height, 1};
        desc.dimension = TextureDimension::D2;
        desc.format = fmt;
        desc.usage = use;
        return desc;
    }

    /// Create render target descriptor
    [[nodiscard]] static TextureDesc render_target(
        std::uint32_t width, std::uint32_t height,
        TextureFormat fmt, std::uint32_t samples = 1) {
        TextureDesc desc;
        desc.size = {width, height, 1};
        desc.dimension = TextureDimension::D2;
        desc.format = fmt;
        desc.sample_count = samples;
        desc.usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding;
        return desc;
    }

    /// Create depth buffer descriptor
    [[nodiscard]] static TextureDesc depth_buffer(
        std::uint32_t width, std::uint32_t height,
        TextureFormat fmt = TextureFormat::Depth32Float,
        std::uint32_t samples = 1) {
        TextureDesc desc;
        desc.size = {width, height, 1};
        desc.dimension = TextureDimension::D2;
        desc.format = fmt;
        desc.sample_count = samples;
        desc.usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding;
        return desc;
    }

    /// Get width
    [[nodiscard]] std::uint32_t width() const noexcept { return size[0]; }

    /// Get height
    [[nodiscard]] std::uint32_t height() const noexcept { return size[1]; }

    /// Get depth (for 3D textures)
    [[nodiscard]] std::uint32_t depth() const noexcept { return size[2]; }
};

// =============================================================================
// BufferUsage (bitflags)
// =============================================================================

/// Buffer usage flags
enum class BufferUsage : std::uint32_t {
    None      = 0,
    MapRead   = 1 << 0,
    MapWrite  = 1 << 1,
    CopySrc   = 1 << 2,
    CopyDst   = 1 << 3,
    Index     = 1 << 4,
    Vertex    = 1 << 5,
    Uniform   = 1 << 6,
    Storage   = 1 << 7,
    Indirect  = 1 << 8
};

/// Bitwise OR for BufferUsage
[[nodiscard]] constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) noexcept {
    return static_cast<BufferUsage>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b)
    );
}

/// Bitwise AND for BufferUsage
[[nodiscard]] constexpr BufferUsage operator&(BufferUsage a, BufferUsage b) noexcept {
    return static_cast<BufferUsage>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)
    );
}

/// Bitwise OR assignment
constexpr BufferUsage& operator|=(BufferUsage& a, BufferUsage b) noexcept {
    return a = a | b;
}

/// Check if flag is set
[[nodiscard]] constexpr bool has_flag(BufferUsage flags, BufferUsage flag) noexcept {
    return (flags & flag) == flag;
}

// =============================================================================
// BufferDesc
// =============================================================================

/// Buffer descriptor
struct BufferDesc {
    std::string label;
    std::uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    bool mapped_at_creation = false;

    /// Create vertex buffer descriptor
    [[nodiscard]] static BufferDesc vertex_buffer(std::uint64_t bytes) {
        BufferDesc desc;
        desc.size = bytes;
        desc.usage = BufferUsage::Vertex | BufferUsage::CopyDst;
        return desc;
    }

    /// Create index buffer descriptor
    [[nodiscard]] static BufferDesc index_buffer(std::uint64_t bytes) {
        BufferDesc desc;
        desc.size = bytes;
        desc.usage = BufferUsage::Index | BufferUsage::CopyDst;
        return desc;
    }

    /// Create uniform buffer descriptor
    [[nodiscard]] static BufferDesc uniform_buffer(std::uint64_t bytes) {
        BufferDesc desc;
        desc.size = bytes;
        desc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
        return desc;
    }

    /// Create storage buffer descriptor
    [[nodiscard]] static BufferDesc storage_buffer(std::uint64_t bytes) {
        BufferDesc desc;
        desc.size = bytes;
        desc.usage = BufferUsage::Storage | BufferUsage::CopyDst;
        return desc;
    }
};

// =============================================================================
// Sampler Types
// =============================================================================

/// Filter mode
enum class FilterMode : std::uint8_t {
    Nearest = 0,
    Linear
};

/// Address mode
enum class AddressMode : std::uint8_t {
    ClampToEdge = 0,
    Repeat,
    MirrorRepeat,
    ClampToBorder
};

/// Compare function
enum class CompareFunction : std::uint8_t {
    Never = 0,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

/// Sampler descriptor
struct SamplerDesc {
    std::string label;
    AddressMode address_mode_u = AddressMode::ClampToEdge;
    AddressMode address_mode_v = AddressMode::ClampToEdge;
    AddressMode address_mode_w = AddressMode::ClampToEdge;
    FilterMode mag_filter = FilterMode::Linear;
    FilterMode min_filter = FilterMode::Linear;
    FilterMode mipmap_filter = FilterMode::Linear;
    float lod_min_clamp = 0.0f;
    float lod_max_clamp = 1000.0f;
    std::optional<CompareFunction> compare;
    std::uint16_t anisotropy_clamp = 1;

    /// Create linear sampler
    [[nodiscard]] static SamplerDesc linear() {
        SamplerDesc desc;
        desc.mag_filter = FilterMode::Linear;
        desc.min_filter = FilterMode::Linear;
        desc.mipmap_filter = FilterMode::Linear;
        return desc;
    }

    /// Create nearest (point) sampler
    [[nodiscard]] static SamplerDesc nearest() {
        SamplerDesc desc;
        desc.mag_filter = FilterMode::Nearest;
        desc.min_filter = FilterMode::Nearest;
        desc.mipmap_filter = FilterMode::Nearest;
        return desc;
    }

    /// Create repeating sampler
    [[nodiscard]] static SamplerDesc repeating() {
        SamplerDesc desc;
        desc.address_mode_u = AddressMode::Repeat;
        desc.address_mode_v = AddressMode::Repeat;
        desc.address_mode_w = AddressMode::Repeat;
        return desc;
    }

    /// Create shadow sampler
    [[nodiscard]] static SamplerDesc shadow() {
        SamplerDesc desc;
        desc.mag_filter = FilterMode::Linear;
        desc.min_filter = FilterMode::Linear;
        desc.compare = CompareFunction::LessEqual;
        return desc;
    }

    /// Set anisotropic filtering
    SamplerDesc& anisotropic(std::uint16_t level) {
        anisotropy_clamp = level;
        return *this;
    }
};

// =============================================================================
// Attachment Types
// =============================================================================

/// Load operation
enum class LoadOp : std::uint8_t {
    Clear = 0,
    Load,
    DontCare
};

/// Store operation
enum class StoreOp : std::uint8_t {
    Store = 0,
    Discard
};

/// Clear value
struct ClearValue {
    enum class Type : std::uint8_t {
        Color,
        Depth,
        Stencil,
        DepthStencil
    };

    Type type = Type::Color;
    union {
        std::array<float, 4> color;
        float depth;
        std::uint32_t stencil;
        struct {
            float depth_value;
            std::uint32_t stencil_value;
        } depth_stencil;
    };

    /// Default constructor (black color)
    ClearValue() : type(Type::Color), color{0.0f, 0.0f, 0.0f, 1.0f} {}

    /// Create color clear value
    [[nodiscard]] static ClearValue with_color(float r, float g, float b, float a = 1.0f) {
        ClearValue cv;
        cv.type = Type::Color;
        cv.color = {r, g, b, a};
        return cv;
    }

    /// Create depth clear value
    [[nodiscard]] static ClearValue depth_value(float d) {
        ClearValue cv;
        cv.type = Type::Depth;
        cv.depth = d;
        return cv;
    }

    /// Create stencil clear value
    [[nodiscard]] static ClearValue stencil_value(std::uint32_t s) {
        ClearValue cv;
        cv.type = Type::Stencil;
        cv.stencil = s;
        return cv;
    }

    /// Create depth/stencil clear value
    [[nodiscard]] static ClearValue depth_stencil_value(float d, std::uint32_t s) {
        ClearValue cv;
        cv.type = Type::DepthStencil;
        cv.depth_stencil = {d, s};
        return cv;
    }
};

/// Attachment descriptor
struct AttachmentDesc {
    TextureFormat format = TextureFormat::Rgba8Unorm;
    std::uint32_t samples = 1;
    LoadOp load_op = LoadOp::Clear;
    StoreOp store_op = StoreOp::Store;

    /// Create color attachment
    [[nodiscard]] static AttachmentDesc color(
        TextureFormat fmt = TextureFormat::Rgba8Unorm,
        LoadOp load = LoadOp::Clear, StoreOp store = StoreOp::Store) {
        return AttachmentDesc{fmt, 1, load, store};
    }

    /// Create depth attachment
    [[nodiscard]] static AttachmentDesc depth(
        TextureFormat fmt = TextureFormat::Depth32Float,
        LoadOp load = LoadOp::Clear, StoreOp store = StoreOp::Store) {
        return AttachmentDesc{fmt, 1, load, store};
    }
};

} // namespace void_render

// Hash specializations
template<>
struct std::hash<void_render::ResourceId> {
    std::size_t operator()(const void_render::ResourceId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
