// ============================================================================
// Shadow Map Depth Shader
// ============================================================================
// Renders scene depth from light's perspective for shadow mapping
// Only outputs depth - no color output needed
// ============================================================================

struct LightSpaceUniforms {
    light_view_proj: mat4x4<f32>,
};

struct ModelUniforms {
    model: mat4x4<f32>,
    normal_matrix: mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> light_space: LightSpaceUniforms;
@group(1) @binding(0) var<uniform> model: ModelUniforms;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    let world_pos = model.model * vec4<f32>(in.position, 1.0);
    output.position = light_space.light_view_proj * world_pos;

    return output;
}

// Fragment shader is minimal - we only care about depth
// The depth is automatically written by the GPU
@fragment
fn fs_main(in: VertexOutput) {
    // No color output - depth-only pass
    // GPU automatically writes depth to shadow map
}
