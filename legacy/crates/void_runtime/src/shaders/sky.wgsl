// ============================================================================
// Sky Shader with HDR Environment Map and Procedural Fallback
// ============================================================================
// Renders sky from:
// 1. Equirectangular HDR environment map (when available)
// 2. Procedural sky with configurable colors and sun (fallback)
// Should be rendered AFTER objects with depth test LessEqual
// ============================================================================

const PI: f32 = 3.14159265359;

struct CameraUniforms {
    view_proj: mat4x4<f32>,
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    camera_pos: vec3<f32>,
    _padding: f32,
};

// Sky configuration uniforms (must match SkyUniforms in Rust)
struct SkyUniforms {
    zenith_color: vec3<f32>,
    _pad0: f32,
    horizon_color: vec3<f32>,
    _pad1: f32,
    ground_color: vec3<f32>,
    _pad2: f32,
    sun_direction: vec3<f32>,
    sun_size: f32,
    sun_intensity: f32,
    sun_falloff: f32,
    fog_density: f32,
    use_environment_map: f32,  // 1.0 = use env map, 0.0 = procedural
};

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(0) @binding(1) var env_texture: texture_2d<f32>;
@group(0) @binding(2) var env_sampler: sampler;
@group(0) @binding(3) var<uniform> sky: SkyUniforms;

// Sample equirectangular environment map
fn sample_equirectangular(dir: vec3<f32>) -> vec3<f32> {
    let phi = atan2(dir.z, dir.x);
    let theta = acos(clamp(dir.y, -1.0, 1.0));
    let u = (phi + PI) / (2.0 * PI);
    let v = theta / PI;
    return textureSample(env_texture, env_sampler, vec2<f32>(u, v)).rgb;
}

// Procedural sky gradient
fn procedural_sky(dir: vec3<f32>) -> vec3<f32> {
    let up = dir.y;

    // Sky gradient: zenith -> horizon -> ground
    var color: vec3<f32>;
    if up > 0.0 {
        // Above horizon: blend zenith to horizon
        let t = pow(1.0 - up, 2.0);  // More horizon near the horizon
        color = mix(sky.zenith_color, sky.horizon_color, t);
    } else {
        // Below horizon: blend horizon to ground
        let t = pow(-up, 0.5);  // Faster transition to ground
        color = mix(sky.horizon_color, sky.ground_color, t);
    }

    // Add sun disc and glow
    if sky.sun_size > 0.0 {
        let sun_dir = normalize(sky.sun_direction);
        let sun_dot = dot(dir, -sun_dir);  // Negative because light comes FROM sun direction

        // Sun disc (hard edge)
        let sun_angular_radius = sky.sun_size;
        if sun_dot > cos(sun_angular_radius) {
            // Inside sun disc - very bright
            color = color + vec3<f32>(1.0, 0.95, 0.9) * sky.sun_intensity;
        } else {
            // Sun glow/halo (exponential falloff)
            let glow_amount = pow(max(sun_dot, 0.0), sky.sun_falloff * 10.0);
            color = color + vec3<f32>(1.0, 0.9, 0.7) * glow_amount * sky.sun_intensity * 0.3;
        }
    }

    return color;
}

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) view_dir: vec3<f32>,
};

// Generate fullscreen triangle and compute view direction
@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> VertexOutput {
    var output: VertexOutput;

    // Fullscreen triangle trick: 3 vertices cover the screen
    let x = f32((idx << 1u) & 2u);
    let y = f32(idx & 2u);

    // Position at far depth (will pass depth test with LessEqual)
    output.position = vec4<f32>(x * 2.0 - 1.0, 1.0 - y * 2.0, 0.9999, 1.0);

    // Compute view direction from inverse projection
    let clip_pos = vec4<f32>(x * 2.0 - 1.0, 1.0 - y * 2.0, 1.0, 1.0);

    // Simple approximation: treat clip xy as view angles
    let fov_scale = 1.2;  // Approximate field of view
    let view_local = vec3<f32>(
        (x * 2.0 - 1.0) * fov_scale,
        (1.0 - y * 2.0) * fov_scale,
        -1.0
    );

    // Transform to world space using inverse view matrix
    let inv_view = transpose(mat3x3<f32>(
        camera.view[0].xyz,
        camera.view[1].xyz,
        camera.view[2].xyz
    ));

    output.view_dir = normalize(inv_view * view_local);

    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let dir = normalize(input.view_dir);

    var color: vec3<f32>;

    // Choose between environment map and procedural sky
    if sky.use_environment_map > 0.5 {
        // Sample HDR environment map
        color = sample_equirectangular(dir);

        // Apply exposure (HDR images can be quite bright)
        let exposure = 1.0;
        color = color * exposure;
    } else {
        // Use procedural sky
        color = procedural_sky(dir);
    }

    // Apply fog near horizon (optional)
    if sky.fog_density > 0.0 {
        let horizon_factor = 1.0 - abs(dir.y);
        let fog_factor = pow(horizon_factor, 4.0) * sky.fog_density;
        color = mix(color, sky.horizon_color, fog_factor);
    }

    // ACES tone mapping (filmic, preserves bright details)
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    color = clamp((color * (a * color + b)) / (color * (c * color + d) + e), vec3<f32>(0.0), vec3<f32>(1.0));

    return vec4<f32>(color, 1.0);
}
