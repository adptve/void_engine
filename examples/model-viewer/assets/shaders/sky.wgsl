// ============================================================================
// Sky Shader with Sun
// ============================================================================

struct SkyUniforms {
    sky_zenith: vec3<f32>,
    _pad1: f32,
    sky_horizon: vec3<f32>,
    _pad2: f32,
    ground_color: vec3<f32>,
    _pad3: f32,
    sun_direction: vec3<f32>,
    sun_intensity: f32,
    time: f32,
    _pad4: f32,
    _pad5: f32,
    _pad6: f32,
}

@group(0) @binding(0) var<uniform> sky: SkyUniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> VertexOutput {
    var output: VertexOutput;
    let x = f32((idx << 1u) & 2u);
    let y = f32(idx & 2u);
    output.position = vec4<f32>(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.9999, 1.0);
    output.uv = vec2<f32>(x, y);
    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let uv = input.uv;
    let y = 1.0 - uv.y;

    var color: vec3<f32>;

    if y > 0.5 {
        // Sky
        let t = (y - 0.5) * 2.0;
        color = mix(sky.sky_horizon, sky.sky_zenith, pow(t, 0.4));

        // Sun
        let sun_uv = vec2<f32>(0.75, 0.8);
        let sun_dist = length(uv - sun_uv);

        // Sun disk
        let sun_disk = smoothstep(0.05, 0.02, sun_dist);
        color = mix(color, vec3<f32>(1.0, 0.95, 0.85), sun_disk * sky.sun_intensity);

        // Sun glow
        color += vec3<f32>(1.0, 0.8, 0.4) * exp(-sun_dist * 6.0) * 0.5 * sky.sun_intensity;
        color += vec3<f32>(1.0, 0.9, 0.6) * exp(-sun_dist * 2.5) * 0.25 * sky.sun_intensity;

    } else {
        // Ground
        let t = (0.5 - y) * 2.0;
        color = mix(sky.sky_horizon * 0.4, sky.ground_color, pow(t, 0.6));
    }

    // Horizon glow
    let h = 1.0 - abs(y - 0.5) * 2.0;
    color += vec3<f32>(1.0, 0.85, 0.6) * pow(max(h, 0.0), 3.0) * 0.25;

    // Tone mapping
    color = color / (color + 0.5);

    return vec4<f32>(color, 1.0);
}
