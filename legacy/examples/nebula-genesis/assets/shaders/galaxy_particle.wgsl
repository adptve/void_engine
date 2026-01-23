// ============================================================================
// GALAXY PARTICLE SHADER
// ============================================================================
// Renders millions of stars with:
// - Soft circular falloff for glow effect
// - Color temperature based on star properties
// - Distance-based size attenuation
// - Twinkle animation
// - Chromatic aberration at edges
// ============================================================================

struct Uniforms {
    view_matrix: mat4x4<f32>,
    projection_matrix: mat4x4<f32>,
    camera_position: vec3<f32>,
    time: f32,
    core_brightness: f32,
    viewport_size: vec2<f32>,
}

struct ParticleInput {
    @location(0) quad_pos: vec2<f32>,      // Quad vertex (-1 to 1)
    @location(1) instance_pos: vec3<f32>,  // World position
    @location(2) instance_color: vec4<f32>, // RGBA color
    @location(3) instance_size: f32,        // Particle size
    @location(4) instance_id: u32,          // For variation
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec4<f32>,
    @location(2) world_pos: vec3<f32>,
    @location(3) size: f32,
    @location(4) twinkle: f32,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

// ============================================================================
// VERTEX SHADER
// ============================================================================

@vertex
fn vs_main(input: ParticleInput) -> VertexOutput {
    var output: VertexOutput;

    // Billboard: face the camera
    let to_camera = normalize(uniforms.camera_position - input.instance_pos);
    let right = normalize(cross(vec3<f32>(0.0, 1.0, 0.0), to_camera));
    let up = cross(to_camera, right);

    // Calculate twinkle based on instance ID and time
    let twinkle_phase = f32(input.instance_id) * 0.1 + uniforms.time * 2.0;
    let twinkle = 0.7 + 0.3 * sin(twinkle_phase) * sin(twinkle_phase * 1.7);

    // Scale quad
    let size = input.instance_size * twinkle;
    let offset = right * input.quad_pos.x * size + up * input.quad_pos.y * size;
    let world_pos = input.instance_pos + offset;

    // Transform to clip space
    let view_pos = uniforms.view_matrix * vec4<f32>(world_pos, 1.0);
    output.clip_position = uniforms.projection_matrix * view_pos;

    output.uv = input.quad_pos * 0.5 + 0.5;
    output.color = input.instance_color;
    output.world_pos = world_pos;
    output.size = size;
    output.twinkle = twinkle;

    return output;
}

// ============================================================================
// FRAGMENT SHADER
// ============================================================================

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // Centered UV
    let centered_uv = input.uv * 2.0 - 1.0;
    let dist = length(centered_uv);

    // Soft circular falloff with glow
    let core_radius = 0.15;
    let glow_radius = 1.0;

    // Core: bright center
    let core = smoothstep(core_radius, 0.0, dist);

    // Glow: soft falloff
    let glow = exp(-dist * dist * 3.0);

    // Combine
    let intensity = core * 2.0 + glow * 0.5;

    // Apply color with HDR values
    var color = input.color.rgb * intensity;

    // Add slight chromatic aberration for stars near edge of view
    let screen_pos = input.clip_position.xy / uniforms.viewport_size;
    let edge_dist = length(screen_pos - 0.5) * 2.0;
    let chromatic = edge_dist * 0.02;

    color.r *= 1.0 + chromatic;
    color.b *= 1.0 - chromatic;

    // Boost brightness for core stars
    let distance_to_center = length(input.world_pos);
    let core_boost = exp(-distance_to_center * 0.05) * uniforms.core_brightness;
    color += input.color.rgb * core_boost * core;

    // Apply twinkle to alpha
    let alpha = intensity * input.color.a * input.twinkle;

    // Discard nearly transparent pixels
    if alpha < 0.01 {
        discard;
    }

    return vec4<f32>(color, alpha);
}
