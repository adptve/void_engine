// ============================================================================
// CRT EFFECT SHADER
// ============================================================================
// Retro CRT monitor simulation with:
// - Screen curvature
// - Scanlines
// - Vignette
// - RGB pixel separation
// - Subtle flicker
// ============================================================================

struct CRTUniforms {
    scanline_intensity: f32,
    curvature: f32,
    vignette: f32,
    time: f32,
    resolution: vec2<f32>,
}

@group(0) @binding(0) var<uniform> crt: CRTUniforms;
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

// Apply barrel distortion for CRT curve
fn curve_uv(uv: vec2<f32>, curvature: f32) -> vec2<f32> {
    let centered = uv * 2.0 - 1.0;
    let offset = centered.yx * centered.yx * curvature;
    let curved = centered + centered * offset;
    return curved * 0.5 + 0.5;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // Apply screen curvature
    let curved_uv = curve_uv(input.uv, crt.curvature);

    // Check if we're outside the curved screen
    if curved_uv.x < 0.0 || curved_uv.x > 1.0 || curved_uv.y < 0.0 || curved_uv.y > 1.0 {
        return vec4<f32>(0.0, 0.0, 0.0, 1.0);
    }

    // Sample the source
    var color = textureSample(source_texture, texture_sampler, curved_uv).rgb;

    // RGB pixel separation (subtle)
    let pixel_offset = 0.001;
    let r = textureSample(source_texture, texture_sampler, curved_uv + vec2<f32>(pixel_offset, 0.0)).r;
    let b = textureSample(source_texture, texture_sampler, curved_uv - vec2<f32>(pixel_offset, 0.0)).b;
    color.r = mix(color.r, r, 0.3);
    color.b = mix(color.b, b, 0.3);

    // Scanlines
    let scanline_y = curved_uv.y * crt.resolution.y;
    let scanline = sin(scanline_y * 3.14159 * 2.0);
    let scanline_factor = 1.0 - crt.scanline_intensity * (scanline * 0.5 + 0.5) * 0.5;
    color *= scanline_factor;

    // Horizontal scanline (less intense)
    let h_scanline = sin(curved_uv.x * crt.resolution.x * 3.14159);
    color *= 1.0 - crt.scanline_intensity * 0.1 * (h_scanline * 0.5 + 0.5);

    // Vignette
    let centered = curved_uv * 2.0 - 1.0;
    let vignette = 1.0 - dot(centered, centered) * crt.vignette;
    color *= vignette;

    // Subtle flicker
    let flicker = 1.0 + sin(crt.time * 60.0) * 0.005;
    color *= flicker;

    // Slight color bleeding / phosphor glow
    let glow = color * 0.1;
    color += glow;

    return vec4<f32>(color, 1.0);
}
