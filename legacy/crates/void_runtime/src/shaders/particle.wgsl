// ============================================================================
// PARTICLE SHADER - Billboarded quad particles with glow
// ============================================================================

// Must match the Rust CameraUniforms struct layout!
struct CameraUniforms {
    view_projection: mat4x4<f32>,  // view_proj comes first
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    camera_position: vec3<f32>,
    _padding: f32,
}

struct ParticleVertex {
    @location(0) quad_pos: vec2<f32>,       // Quad vertex position (-1 to 1)
    @location(1) particle_pos: vec3<f32>,   // World position (instanced)
    @location(2) particle_color: vec4<f32>, // RGBA color (instanced)
    @location(3) particle_size: f32,        // Size (instanced)
    @location(4) particle_age: f32,         // Age 0-1 (instanced)
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec4<f32>,
    @location(2) age: f32,
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;

// ============================================================================
// VERTEX SHADER - Billboard facing camera
// ============================================================================

@vertex
fn vs_main(input: ParticleVertex) -> VertexOutput {
    var output: VertexOutput;

    // Billboard: make quad face the camera
    let to_camera = normalize(camera.camera_position - input.particle_pos);
    let world_up = vec3<f32>(0.0, 1.0, 0.0);
    var right = normalize(cross(world_up, to_camera));
    var up = cross(to_camera, right);

    // Handle case when looking straight up/down
    if length(right) < 0.001 {
        right = vec3<f32>(1.0, 0.0, 0.0);
        up = vec3<f32>(0.0, 0.0, 1.0);
    }

    // Scale quad by particle size
    let size = input.particle_size;
    let offset = right * input.quad_pos.x * size + up * input.quad_pos.y * size;
    let world_pos = input.particle_pos + offset;

    // Transform to clip space
    output.clip_position = camera.view_projection * vec4<f32>(world_pos, 1.0);
    output.uv = input.quad_pos * 0.5 + 0.5;
    output.color = input.particle_color;
    output.age = input.particle_age;

    return output;
}

// ============================================================================
// FRAGMENT SHADER - Soft circle with glow
// ============================================================================

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // Centered UV for circular falloff
    let centered = input.uv * 2.0 - 1.0;
    let dist = length(centered);

    // Soft circular falloff
    let core_radius = 0.3;
    let glow_falloff = 2.5;

    // Core: bright center
    let core = smoothstep(core_radius, 0.0, dist);

    // Glow: exponential falloff
    let glow = exp(-dist * dist * glow_falloff);

    // Combine core and glow
    let intensity = core * 1.5 + glow * 0.5;

    // Fade out based on age (particles fade as they die)
    let age_fade = 1.0 - input.age * input.age;

    // Final color
    var color = input.color.rgb * intensity;
    let alpha = intensity * input.color.a * age_fade;

    // Discard nearly transparent pixels
    if alpha < 0.01 {
        discard;
    }

    return vec4<f32>(color, alpha);
}
