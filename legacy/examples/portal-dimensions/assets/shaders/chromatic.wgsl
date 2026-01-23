// ============================================================================
// CHROMATIC ABERRATION SHADER
// ============================================================================
// Separates color channels for a reality-distortion effect
// ============================================================================

struct ChromaticUniforms {
    intensity: f32,
    center: vec2<f32>,
    time: f32,
}

@group(0) @binding(0) var<uniform> chromatic: ChromaticUniforms;
@group(0) @binding(1) var source_texture: texture_2d<f32>;
@group(0) @binding(2) var texture_sampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    var output: VertexOutput;

    let x = f32((vertex_index << 1u) & 2u);
    let y = f32(vertex_index & 2u);

    output.position = vec4<f32>(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0);
    output.uv = vec2<f32>(x, 1.0 - y);

    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let center = chromatic.center;
    let to_center = input.uv - center;
    let dist = length(to_center);

    // Chromatic offset increases toward edges
    let offset = to_center * chromatic.intensity * dist * 0.02;

    // Sample each channel with different offsets
    let r = textureSample(source_texture, texture_sampler, input.uv + offset).r;
    let g = textureSample(source_texture, texture_sampler, input.uv).g;
    let b = textureSample(source_texture, texture_sampler, input.uv - offset).b;

    // Original alpha
    let a = textureSample(source_texture, texture_sampler, input.uv).a;

    return vec4<f32>(r, g, b, a);
}
