// ============================================================================
// BLOOM POST-PROCESSING SHADER
// ============================================================================
// Multi-pass bloom effect:
// 1. Extract bright pixels (threshold)
// 2. Gaussian blur (horizontal + vertical)
// 3. Combine with original
// ============================================================================

struct BloomUniforms {
    intensity: f32,
    threshold: f32,
    blur_scale: f32,
    pass: u32,  // 0=extract, 1=blur_h, 2=blur_v, 3=combine
    texel_size: vec2<f32>,
}

@group(0) @binding(0) var<uniform> bloom: BloomUniforms;
@group(0) @binding(1) var source_texture: texture_2d<f32>;
@group(0) @binding(2) var bloom_texture: texture_2d<f32>;
@group(0) @binding(3) var texture_sampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

// Fullscreen triangle
@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    var output: VertexOutput;

    // Generate fullscreen triangle
    let x = f32((vertex_index << 1u) & 2u);
    let y = f32(vertex_index & 2u);

    output.position = vec4<f32>(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0);
    output.uv = vec2<f32>(x, 1.0 - y);

    return output;
}

// ============================================================================
// PASS 0: BRIGHTNESS EXTRACTION
// ============================================================================

fn extract_bright(uv: vec2<f32>) -> vec4<f32> {
    let color = textureSample(source_texture, texture_sampler, uv);

    // Calculate luminance
    let luminance = dot(color.rgb, vec3<f32>(0.2126, 0.7152, 0.0722));

    // Soft threshold
    let soft_threshold = bloom.threshold * 0.9;
    let brightness = smoothstep(soft_threshold, bloom.threshold, luminance);

    return vec4<f32>(color.rgb * brightness, 1.0);
}

// ============================================================================
// PASS 1 & 2: GAUSSIAN BLUR
// ============================================================================

// 9-tap Gaussian weights
const WEIGHTS: array<f32, 5> = array<f32, 5>(
    0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216
);

fn blur_horizontal(uv: vec2<f32>) -> vec4<f32> {
    var result = textureSample(bloom_texture, texture_sampler, uv) * WEIGHTS[0];

    let offset = bloom.texel_size.x * bloom.blur_scale;

    for (var i = 1; i < 5; i++) {
        let sample_offset = offset * f32(i);
        result += textureSample(bloom_texture, texture_sampler, uv + vec2<f32>(sample_offset, 0.0)) * WEIGHTS[i];
        result += textureSample(bloom_texture, texture_sampler, uv - vec2<f32>(sample_offset, 0.0)) * WEIGHTS[i];
    }

    return result;
}

fn blur_vertical(uv: vec2<f32>) -> vec4<f32> {
    var result = textureSample(bloom_texture, texture_sampler, uv) * WEIGHTS[0];

    let offset = bloom.texel_size.y * bloom.blur_scale;

    for (var i = 1; i < 5; i++) {
        let sample_offset = offset * f32(i);
        result += textureSample(bloom_texture, texture_sampler, uv + vec2<f32>(0.0, sample_offset)) * WEIGHTS[i];
        result += textureSample(bloom_texture, texture_sampler, uv - vec2<f32>(0.0, sample_offset)) * WEIGHTS[i];
    }

    return result;
}

// ============================================================================
// PASS 3: COMBINE
// ============================================================================

fn combine(uv: vec2<f32>) -> vec4<f32> {
    let original = textureSample(source_texture, texture_sampler, uv);
    let bloom_color = textureSample(bloom_texture, texture_sampler, uv);

    // Additive blend with intensity
    var result = original.rgb + bloom_color.rgb * bloom.intensity;

    // Subtle vignette
    let centered = uv * 2.0 - 1.0;
    let vignette = 1.0 - dot(centered, centered) * 0.3;
    result *= vignette;

    // Tone mapping (simple Reinhard)
    result = result / (result + vec3<f32>(1.0));

    // Gamma correction
    result = pow(result, vec3<f32>(1.0 / 2.2));

    return vec4<f32>(result, 1.0);
}

// ============================================================================
// FRAGMENT SHADER
// ============================================================================

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    switch bloom.pass {
        case 0u: { return extract_bright(input.uv); }
        case 1u: { return blur_horizontal(input.uv); }
        case 2u: { return blur_vertical(input.uv); }
        case 3u: { return combine(input.uv); }
        default: { return vec4<f32>(1.0, 0.0, 1.0, 1.0); } // Error: magenta
    }
}
