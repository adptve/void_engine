#pragma once

/// @file renderer.hpp
/// @brief UI Renderer interface
///
/// Provides GPU rendering abstraction for UI elements with:
/// - Backend-agnostic interface
/// - Null renderer for testing
/// - Shader source for wgpu/WebGPU/Vulkan implementations

#include "fwd.hpp"
#include "types.hpp"

#include <memory>
#include <string>

namespace void_ui {

// =============================================================================
// UI Shader Source
// =============================================================================

/// WGSL shader source for UI rendering
inline constexpr const char* UI_SHADER_WGSL = R"(
struct Uniforms {
    screen_size: vec2<f32>,
    _padding: vec2<f32>,
};

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

struct VertexInput {
    @location(0) position: vec2<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) color: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    // Convert pixel coordinates to clip space (-1 to 1)
    let x = (in.position.x / uniforms.screen_size.x) * 2.0 - 1.0;
    let y = 1.0 - (in.position.y / uniforms.screen_size.y) * 2.0;
    out.clip_position = vec4<f32>(x, y, 0.0, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return in.color;
}
)";

/// GLSL vertex shader source
inline constexpr const char* UI_SHADER_GLSL_VERT = R"(
#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    vec2 screen_size;
    vec2 _padding;
};

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;

void main() {
    float x = (position.x / screen_size.x) * 2.0 - 1.0;
    float y = 1.0 - (position.y / screen_size.y) * 2.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
    frag_uv = uv;
    frag_color = color;
}
)";

/// GLSL fragment shader source
inline constexpr const char* UI_SHADER_GLSL_FRAG = R"(
#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = frag_color;
}
)";

/// HLSL shader source (for D3D12)
inline constexpr const char* UI_SHADER_HLSL = R"(
cbuffer Uniforms : register(b0) {
    float2 screen_size;
    float2 _padding;
};

struct VSInput {
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    float x = (input.position.x / screen_size.x) * 2.0 - 1.0;
    float y = 1.0 - (input.position.y / screen_size.y) * 2.0;
    output.position = float4(x, y, 0.0, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}
)";

// =============================================================================
// Renderer Interface
// =============================================================================

/// Prepared GPU buffers for rendering
struct UiGpuBuffers {
    void* vertex_buffer = nullptr;
    void* index_buffer = nullptr;
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
};

/// UI Renderer interface
class IUiRenderer {
public:
    virtual ~IUiRenderer() = default;

    /// Set screen size for coordinate conversion
    virtual void set_screen_size(float width, float height) = 0;

    /// Get screen size
    [[nodiscard]] virtual Size screen_size() const = 0;

    /// Prepare draw data for rendering (creates GPU buffers)
    /// @return true if there is data to render
    virtual bool prepare(const UiDrawData& draw_data) = 0;

    /// Render the prepared UI
    /// @param render_pass Native render pass handle
    virtual void render(void* render_pass) = 0;

    /// Get native pipeline handle
    [[nodiscard]] virtual void* native_pipeline() const = 0;

    /// Get native bind group handle
    [[nodiscard]] virtual void* native_bind_group() const = 0;

    /// Check if renderer is valid
    [[nodiscard]] virtual bool is_valid() const = 0;
};

// =============================================================================
// Null Renderer (for testing)
// =============================================================================

/// Null UI renderer for testing
class NullUiRenderer : public IUiRenderer {
public:
    NullUiRenderer() = default;

    void set_screen_size(float width, float height) override {
        m_screen_size = {width, height};
    }

    [[nodiscard]] Size screen_size() const override {
        return m_screen_size;
    }

    bool prepare(const UiDrawData& draw_data) override {
        m_last_vertex_count = static_cast<std::uint32_t>(draw_data.vertices.size());
        m_last_index_count = static_cast<std::uint32_t>(draw_data.indices.size());
        return !draw_data.empty();
    }

    void render(void* /*render_pass*/) override {
        // No-op
    }

    [[nodiscard]] void* native_pipeline() const override { return nullptr; }
    [[nodiscard]] void* native_bind_group() const override { return nullptr; }
    [[nodiscard]] bool is_valid() const override { return true; }

    /// Get last prepared vertex count (for testing)
    [[nodiscard]] std::uint32_t last_vertex_count() const { return m_last_vertex_count; }

    /// Get last prepared index count (for testing)
    [[nodiscard]] std::uint32_t last_index_count() const { return m_last_index_count; }

private:
    Size m_screen_size{1280.0f, 720.0f};
    std::uint32_t m_last_vertex_count = 0;
    std::uint32_t m_last_index_count = 0;
};

// =============================================================================
// Renderer Factory
// =============================================================================

/// Create a null renderer for testing
[[nodiscard]] inline std::unique_ptr<IUiRenderer> create_null_renderer() {
    return std::make_unique<NullUiRenderer>();
}

// Note: GPU-specific renderers would be created by passing device/queue handles
// Example: create_wgpu_renderer(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat format)

} // namespace void_ui
