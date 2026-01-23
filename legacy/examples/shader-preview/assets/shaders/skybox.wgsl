// ============================================================================
// Gradient Skybox Shader
// ============================================================================
// Renders a procedural gradient skybox with optional sun

struct SkyboxUniforms {
    view_proj_inverse: mat4x4<f32>,
    camera_pos: vec3<f32>,
    time: f32,
    sun_direction: vec3<f32>,
    sun_intensity: f32,
    sky_color_top: vec3<f32>,
    _pad1: f32,
    sky_color_horizon: vec3<f32>,
    _pad2: f32,
    ground_color: vec3<f32>,
    _pad3: f32,
}

@group(0) @binding(0) var<uniform> sky: SkyboxUniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) view_ray: vec3<f32>,
}

// Fullscreen triangle
@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    var output: VertexOutput;

    // Fullscreen triangle coordinates
    let x = f32((vertex_index << 1u) & 2u) * 2.0 - 1.0;
    let y = f32(vertex_index & 2u) * 2.0 - 1.0;

    output.position = vec4<f32>(x, y, 0.9999, 1.0);

    // Calculate view ray by unprojecting clip space position
    let clip_pos = vec4<f32>(x, y, 1.0, 1.0);
    let world_pos = sky.view_proj_inverse * clip_pos;
    output.view_ray = normalize(world_pos.xyz / world_pos.w - sky.camera_pos);

    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let ray = normalize(input.view_ray);

    // Vertical blend factor
    let y = ray.y;

    // Sky gradient
    var color: vec3<f32>;
    if y > 0.0 {
        // Above horizon: blend from horizon to top
        let t = pow(y, 0.5);
        color = mix(sky.sky_color_horizon, sky.sky_color_top, t);
    } else {
        // Below horizon: darker ground color
        let t = pow(-y, 0.8);
        color = mix(sky.sky_color_horizon * 0.5, sky.ground_color, t);
    }

    // Sun
    let sun_dot = max(dot(ray, normalize(-sky.sun_direction)), 0.0);

    // Sun disk
    let sun_disk = smoothstep(0.9995, 0.9999, sun_dot);
    color = mix(color, vec3<f32>(1.0, 0.95, 0.8), sun_disk * sky.sun_intensity);

    // Sun glow
    let sun_glow = pow(sun_dot, 64.0) * 0.5 + pow(sun_dot, 8.0) * 0.2;
    color += vec3<f32>(1.0, 0.7, 0.4) * sun_glow * sky.sun_intensity;

    // Atmospheric scattering approximation
    let scatter = pow(max(sun_dot, 0.0), 2.0) * 0.1;
    color += vec3<f32>(1.0, 0.8, 0.6) * scatter * sky.sun_intensity * (1.0 - abs(y));

    return vec4<f32>(color, 1.0);
}
