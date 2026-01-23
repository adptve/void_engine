#pragma once

/// @file binding.hpp
/// @brief Shader binding information and reflection for void_shader

#include "fwd.hpp"
#include "types.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace void_shader {

// =============================================================================
// BindingType
// =============================================================================

/// Types of shader bindings
enum class BindingType : std::uint8_t {
    UniformBuffer,
    StorageBuffer,
    ReadOnlyStorageBuffer,
    Sampler,
    SampledTexture,
    StorageTexture,
    ReadOnlyStorageTexture,
    CombinedImageSampler,
};

/// Get binding type name
[[nodiscard]] inline const char* binding_type_name(BindingType type) {
    switch (type) {
        case BindingType::UniformBuffer: return "UniformBuffer";
        case BindingType::StorageBuffer: return "StorageBuffer";
        case BindingType::ReadOnlyStorageBuffer: return "ReadOnlyStorageBuffer";
        case BindingType::Sampler: return "Sampler";
        case BindingType::SampledTexture: return "SampledTexture";
        case BindingType::StorageTexture: return "StorageTexture";
        case BindingType::ReadOnlyStorageTexture: return "ReadOnlyStorageTexture";
        case BindingType::CombinedImageSampler: return "CombinedImageSampler";
        default: return "Unknown";
    }
}

// =============================================================================
// TextureFormat
// =============================================================================

/// Texture format for reflection
enum class TextureFormat : std::uint8_t {
    Unknown,
    R8Unorm,
    R8Snorm,
    R8Uint,
    R8Sint,
    R16Uint,
    R16Sint,
    R16Float,
    Rg8Unorm,
    Rg8Snorm,
    Rg8Uint,
    Rg8Sint,
    R32Uint,
    R32Sint,
    R32Float,
    Rg16Uint,
    Rg16Sint,
    Rg16Float,
    Rgba8Unorm,
    Rgba8Snorm,
    Rgba8Uint,
    Rgba8Sint,
    Bgra8Unorm,
    Rgb10a2Unorm,
    Rg32Uint,
    Rg32Sint,
    Rg32Float,
    Rgba16Uint,
    Rgba16Sint,
    Rgba16Float,
    Rgba32Uint,
    Rgba32Sint,
    Rgba32Float,
    Depth16Unorm,
    Depth24Plus,
    Depth24PlusStencil8,
    Depth32Float,
    Depth32FloatStencil8,
};

// =============================================================================
// TextureDimension
// =============================================================================

/// Texture dimension
enum class TextureDimension : std::uint8_t {
    Texture1D,
    Texture2D,
    Texture2DArray,
    Texture3D,
    TextureCube,
    TextureCubeArray,
    Multisampled2D,
};

// =============================================================================
// VertexFormat
// =============================================================================

/// Vertex attribute format
enum class VertexFormat : std::uint8_t {
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Sint8x2,
    Sint8x4,
    Uint8x2,
    Uint8x4,
    Snorm8x2,
    Snorm8x4,
    Unorm8x2,
    Unorm8x4,
    Sint16x2,
    Sint16x4,
    Uint16x2,
    Uint16x4,
    Snorm16x2,
    Snorm16x4,
    Unorm16x2,
    Unorm16x4,
    Float16x2,
    Float16x4,
    Sint32,
    Sint32x2,
    Sint32x3,
    Sint32x4,
    Uint32,
    Uint32x2,
    Uint32x3,
    Uint32x4,
};

/// Get vertex format size in bytes
[[nodiscard]] inline std::size_t vertex_format_size(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float32: return 4;
        case VertexFormat::Float32x2: return 8;
        case VertexFormat::Float32x3: return 12;
        case VertexFormat::Float32x4: return 16;
        case VertexFormat::Sint8x2:
        case VertexFormat::Uint8x2:
        case VertexFormat::Snorm8x2:
        case VertexFormat::Unorm8x2: return 2;
        case VertexFormat::Sint8x4:
        case VertexFormat::Uint8x4:
        case VertexFormat::Snorm8x4:
        case VertexFormat::Unorm8x4: return 4;
        case VertexFormat::Sint16x2:
        case VertexFormat::Uint16x2:
        case VertexFormat::Snorm16x2:
        case VertexFormat::Unorm16x2:
        case VertexFormat::Float16x2: return 4;
        case VertexFormat::Sint16x4:
        case VertexFormat::Uint16x4:
        case VertexFormat::Snorm16x4:
        case VertexFormat::Unorm16x4:
        case VertexFormat::Float16x4: return 8;
        case VertexFormat::Sint32:
        case VertexFormat::Uint32: return 4;
        case VertexFormat::Sint32x2:
        case VertexFormat::Uint32x2: return 8;
        case VertexFormat::Sint32x3:
        case VertexFormat::Uint32x3: return 12;
        case VertexFormat::Sint32x4:
        case VertexFormat::Uint32x4: return 16;
        default: return 0;
    }
}

// =============================================================================
// BindingInfo
// =============================================================================

/// Information about a single binding
struct BindingInfo {
    std::uint32_t group = 0;
    std::uint32_t binding = 0;
    BindingType type = BindingType::UniformBuffer;
    std::optional<std::string> name;

    // For buffers
    std::size_t min_binding_size = 0;
    bool has_dynamic_offset = false;

    // For textures
    TextureDimension texture_dimension = TextureDimension::Texture2D;
    TextureFormat texture_format = TextureFormat::Unknown;
    bool multisampled = false;

    /// Default constructor
    BindingInfo() = default;

    /// Construct uniform buffer binding
    static BindingInfo uniform_buffer(
        std::uint32_t grp,
        std::uint32_t bind,
        std::size_t size,
        const std::string& n = "")
    {
        BindingInfo info;
        info.group = grp;
        info.binding = bind;
        info.type = BindingType::UniformBuffer;
        info.min_binding_size = size;
        if (!n.empty()) info.name = n;
        return info;
    }

    /// Construct storage buffer binding
    static BindingInfo storage_buffer(
        std::uint32_t grp,
        std::uint32_t bind,
        bool read_only = false,
        const std::string& n = "")
    {
        BindingInfo info;
        info.group = grp;
        info.binding = bind;
        info.type = read_only ? BindingType::ReadOnlyStorageBuffer : BindingType::StorageBuffer;
        if (!n.empty()) info.name = n;
        return info;
    }

    /// Construct sampler binding
    static BindingInfo sampler(std::uint32_t grp, std::uint32_t bind, const std::string& n = "") {
        BindingInfo info;
        info.group = grp;
        info.binding = bind;
        info.type = BindingType::Sampler;
        if (!n.empty()) info.name = n;
        return info;
    }

    /// Construct texture binding
    static BindingInfo texture(
        std::uint32_t grp,
        std::uint32_t bind,
        TextureDimension dim = TextureDimension::Texture2D,
        const std::string& n = "")
    {
        BindingInfo info;
        info.group = grp;
        info.binding = bind;
        info.type = BindingType::SampledTexture;
        info.texture_dimension = dim;
        if (!n.empty()) info.name = n;
        return info;
    }
};

// =============================================================================
// BindGroupLayout
// =============================================================================

/// Layout of a bind group
struct BindGroupLayout {
    std::uint32_t group = 0;
    std::vector<BindingInfo> bindings;

    /// Default constructor
    BindGroupLayout() = default;

    /// Construct with group index
    explicit BindGroupLayout(std::uint32_t grp) : group(grp) {}

    /// Add binding
    BindGroupLayout& with_binding(BindingInfo info) {
        bindings.push_back(std::move(info));
        return *this;
    }

    /// Get binding by index
    [[nodiscard]] const BindingInfo* get_binding(std::uint32_t binding_index) const {
        for (const auto& b : bindings) {
            if (b.binding == binding_index) {
                return &b;
            }
        }
        return nullptr;
    }

    /// Check if has binding
    [[nodiscard]] bool has_binding(std::uint32_t binding_index) const {
        return get_binding(binding_index) != nullptr;
    }

    /// Get binding count
    [[nodiscard]] std::size_t binding_count() const noexcept {
        return bindings.size();
    }

    /// Sort bindings by binding index
    void sort_bindings() {
        std::sort(bindings.begin(), bindings.end(),
            [](const BindingInfo& a, const BindingInfo& b) {
                return a.binding < b.binding;
            });
    }
};

// =============================================================================
// VertexInput
// =============================================================================

/// Vertex shader input attribute
struct VertexInput {
    std::uint32_t location = 0;
    VertexFormat format = VertexFormat::Float32x4;
    std::optional<std::string> name;

    /// Default constructor
    VertexInput() = default;

    /// Construct with location and format
    VertexInput(std::uint32_t loc, VertexFormat fmt, const std::string& n = "")
        : location(loc), format(fmt)
    {
        if (!n.empty()) name = n;
    }

    /// Get size in bytes
    [[nodiscard]] std::size_t size() const noexcept {
        return vertex_format_size(format);
    }
};

// =============================================================================
// FragmentOutput
// =============================================================================

/// Fragment shader output
struct FragmentOutput {
    std::uint32_t location = 0;
    VertexFormat format = VertexFormat::Float32x4;
    std::optional<std::string> name;

    /// Default constructor
    FragmentOutput() = default;

    /// Construct with location and format
    FragmentOutput(std::uint32_t loc, VertexFormat fmt, const std::string& n = "")
        : location(loc), format(fmt)
    {
        if (!n.empty()) name = n;
    }
};

// =============================================================================
// PushConstantRange
// =============================================================================

/// Push constant / root constant range
struct PushConstantRange {
    ShaderStage stages = ShaderStage::Vertex;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;

    /// Default constructor
    PushConstantRange() = default;

    /// Construct with values
    PushConstantRange(ShaderStage s, std::uint32_t off, std::uint32_t sz)
        : stages(s), offset(off), size(sz) {}
};

// =============================================================================
// ShaderReflection
// =============================================================================

/// Complete reflection information for a shader
struct ShaderReflection {
    std::map<std::uint32_t, BindGroupLayout> bind_groups;
    std::vector<VertexInput> vertex_inputs;
    std::vector<FragmentOutput> fragment_outputs;
    std::optional<std::array<std::uint32_t, 3>> workgroup_size;
    std::optional<PushConstantRange> push_constants;
    std::vector<std::string> entry_points;

    /// Default constructor
    ShaderReflection() = default;

    /// Get bind group layout
    [[nodiscard]] const BindGroupLayout* get_bind_group(std::uint32_t group) const {
        auto it = bind_groups.find(group);
        return it != bind_groups.end() ? &it->second : nullptr;
    }

    /// Check if has bind group
    [[nodiscard]] bool has_bind_group(std::uint32_t group) const {
        return bind_groups.find(group) != bind_groups.end();
    }

    /// Get total binding count across all groups
    [[nodiscard]] std::size_t total_binding_count() const {
        std::size_t count = 0;
        for (const auto& [grp, layout] : bind_groups) {
            count += layout.binding_count();
        }
        return count;
    }

    /// Get vertex input by location
    [[nodiscard]] const VertexInput* get_vertex_input(std::uint32_t location) const {
        for (const auto& input : vertex_inputs) {
            if (input.location == location) {
                return &input;
            }
        }
        return nullptr;
    }

    /// Check if has entry point
    [[nodiscard]] bool has_entry_point(const std::string& name) const {
        for (const auto& ep : entry_points) {
            if (ep == name) return true;
        }
        return false;
    }

    /// Get max bind group index used
    [[nodiscard]] std::uint32_t max_bind_group() const {
        std::uint32_t max = 0;
        for (const auto& [grp, layout] : bind_groups) {
            if (grp > max) max = grp;
        }
        return max;
    }

    /// Calculate total vertex input stride
    [[nodiscard]] std::size_t vertex_stride() const {
        std::size_t stride = 0;
        for (const auto& input : vertex_inputs) {
            stride += input.size();
        }
        return stride;
    }

    /// Check if this is a compute shader
    [[nodiscard]] bool is_compute() const noexcept {
        return workgroup_size.has_value();
    }

    /// Merge with another reflection (for combined shaders)
    void merge(const ShaderReflection& other) {
        for (const auto& [grp, layout] : other.bind_groups) {
            if (bind_groups.find(grp) == bind_groups.end()) {
                bind_groups[grp] = layout;
            } else {
                // Merge bindings
                for (const auto& binding : layout.bindings) {
                    if (!bind_groups[grp].has_binding(binding.binding)) {
                        bind_groups[grp].bindings.push_back(binding);
                    }
                }
            }
        }

        // Vertex inputs only from vertex shader
        if (vertex_inputs.empty()) {
            vertex_inputs = other.vertex_inputs;
        }

        // Fragment outputs only from fragment shader
        if (fragment_outputs.empty()) {
            fragment_outputs = other.fragment_outputs;
        }

        // Entry points
        for (const auto& ep : other.entry_points) {
            if (!has_entry_point(ep)) {
                entry_points.push_back(ep);
            }
        }
    }
};

// =============================================================================
// Standard Bind Group Indices
// =============================================================================

namespace bind_group {
    /// Global data (camera, time, environment)
    constexpr std::uint32_t GLOBAL = 0;

    /// Material data (per-shader custom data)
    constexpr std::uint32_t MATERIAL = 1;

    /// Object/instance data (transforms)
    constexpr std::uint32_t OBJECT = 2;

    /// Custom/application-specific
    constexpr std::uint32_t CUSTOM = 3;
}

} // namespace void_shader
