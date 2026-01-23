// ============================================================================
// SYNTHWAVE SUN SHADER
// ============================================================================
// The iconic retro sun with:
// - Gradient from yellow to hot pink
// - Horizontal scanline gaps
// - Soft glow halo
// - Pulsing animation
// ============================================================================

struct SunUniforms {
    color_top: vec4<f32>,
    color_bottom: vec4<f32>,
    scanline_count: f32,
    scanline_offset: f32,
    glow_intensity: f32,
    time: f32,
}

@group(0) @binding(0) var<uniform> sun: SunUniforms;

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    var output: VertexOutput;

    // Circle vertices (generated)
    let angle = f32(vertex_index) / 64.0 * 6.28318;
    let x = cos(angle);
    let y = sin(angle);

    output.clip_position = vec4<f32>(x * 0.3, y * 0.3 + 0.2, 0.5, 1.0);
    output.uv = vec2<f32>(x, y) * 0.5 + 0.5;

    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let centered_uv = input.uv * 2.0 - 1.0;
    let dist = length(centered_uv);

    // Circle mask
    if dist > 1.0 {
        // Outer glow
        let glow_dist = dist - 1.0;
        let glow = exp(-glow_dist * 3.0) * sun.glow_intensity * 0.5;
        let glow_color = mix(sun.color_bottom.rgb, sun.color_top.rgb, 0.5);
        return vec4<f32>(glow_color * glow, glow);
    }

    // Vertical gradient (top to bottom)
    let gradient_t = (centered_uv.y + 1.0) * 0.5;
    var color = mix(sun.color_bottom.rgb, sun.color_top.rgb, gradient_t);

    // Scanline gaps (horizontal stripes that cut through lower half)
    let scanline_y = (1.0 - gradient_t) * sun.scanline_count + sun.scanline_offset;
    let scanline_wave = sin(scanline_y * 3.14159);

    // Only apply scanlines to bottom half
    if gradient_t < 0.5 {
        let scanline_strength = (0.5 - gradient_t) * 2.0;
        let scanline_mask = step(0.3, scanline_wave);

        // Cut out the scanline gaps
        if scanline_mask < 0.5 && scanline_strength > 0.3 {
            discard;
        }
    }

    // Edge glow
    let edge = smoothstep(0.9, 1.0, dist);
    color += sun.color_bottom.rgb * edge * 0.5;

    // Center brightness boost
    let center_boost = (1.0 - dist) * 0.3;
    color += vec3<f32>(1.0, 1.0, 0.8) * center_boost;

    // HDR output for bloom
    return vec4<f32>(color * 1.5, 1.0);
}
