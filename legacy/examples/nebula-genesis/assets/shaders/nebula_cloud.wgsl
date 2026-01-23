// ============================================================================
// NEBULA CLOUD SHADER
// ============================================================================
// Volumetric nebula clouds using raymarching:
// - Procedural noise for cloud density
// - Multiple octaves of fractal brownian motion
// - Emission and absorption
// - Animated flow
// ============================================================================

struct CloudUniforms {
    view_matrix: mat4x4<f32>,
    projection_matrix: mat4x4<f32>,
    inverse_view_projection: mat4x4<f32>,
    camera_position: vec3<f32>,
    time: f32,
    cloud_density: f32,
    cloud_color_1: vec3<f32>,
    cloud_color_2: vec3<f32>,
    absorption: f32,
}

@group(0) @binding(0) var<uniform> cloud: CloudUniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) ray_dir: vec3<f32>,
}

// ============================================================================
// NOISE FUNCTIONS
// ============================================================================

fn hash33(p: vec3<f32>) -> vec3<f32> {
    var q = fract(p * vec3<f32>(0.1031, 0.1030, 0.0973));
    q += dot(q, q.yxz + 33.33);
    return fract((q.xxy + q.yxx) * q.zyx);
}

fn noise3d(p: vec3<f32>) -> f32 {
    let i = floor(p);
    let f = fract(p);
    let u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(
            mix(dot(hash33(i + vec3<f32>(0.0, 0.0, 0.0)) * 2.0 - 1.0, f - vec3<f32>(0.0, 0.0, 0.0)),
                dot(hash33(i + vec3<f32>(1.0, 0.0, 0.0)) * 2.0 - 1.0, f - vec3<f32>(1.0, 0.0, 0.0)), u.x),
            mix(dot(hash33(i + vec3<f32>(0.0, 1.0, 0.0)) * 2.0 - 1.0, f - vec3<f32>(0.0, 1.0, 0.0)),
                dot(hash33(i + vec3<f32>(1.0, 1.0, 0.0)) * 2.0 - 1.0, f - vec3<f32>(1.0, 1.0, 0.0)), u.x), u.y),
        mix(
            mix(dot(hash33(i + vec3<f32>(0.0, 0.0, 1.0)) * 2.0 - 1.0, f - vec3<f32>(0.0, 0.0, 1.0)),
                dot(hash33(i + vec3<f32>(1.0, 0.0, 1.0)) * 2.0 - 1.0, f - vec3<f32>(1.0, 0.0, 1.0)), u.x),
            mix(dot(hash33(i + vec3<f32>(0.0, 1.0, 1.0)) * 2.0 - 1.0, f - vec3<f32>(0.0, 1.0, 1.0)),
                dot(hash33(i + vec3<f32>(1.0, 1.0, 1.0)) * 2.0 - 1.0, f - vec3<f32>(1.0, 1.0, 1.0)), u.x), u.y),
        u.z
    );
}

fn fbm(p: vec3<f32>, octaves: i32) -> f32 {
    var value = 0.0;
    var amplitude = 0.5;
    var frequency = 1.0;
    var pos = p;

    for (var i = 0; i < octaves; i++) {
        value += amplitude * noise3d(pos * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
        // Rotate for less axis-aligned artifacts
        pos = vec3<f32>(
            pos.y * 0.8 + pos.z * 0.6,
            pos.z * 0.8 - pos.x * 0.6,
            pos.x * 0.8 + pos.y * 0.6
        );
    }

    return value;
}

// ============================================================================
// NEBULA DENSITY
// ============================================================================

fn nebula_density(p: vec3<f32>) -> f32 {
    // Animated position
    let flow = vec3<f32>(cloud.time * 0.02, cloud.time * 0.01, cloud.time * 0.015);
    let pos = p + flow;

    // Base shape: disk with spiral arms
    let r = length(p.xz);
    let angle = atan2(p.z, p.x);
    let spiral = sin(angle * 4.0 - r * 0.3) * 0.5 + 0.5;
    let disk = exp(-abs(p.y) * 0.5) * exp(-r * 0.02);

    // Detail noise
    let detail = fbm(pos * 0.1, 5) * 0.5 + 0.5;

    // Combine
    var density = disk * spiral * detail * cloud.cloud_density;

    // Core glow
    let core_dist = length(p);
    density += exp(-core_dist * 0.1) * 0.5;

    return max(density, 0.0);
}

// ============================================================================
// RAYMARCHING
// ============================================================================

fn raymarch_nebula(ray_origin: vec3<f32>, ray_dir: vec3<f32>) -> vec4<f32> {
    let MAX_STEPS: i32 = 64;
    let MAX_DIST: f32 = 200.0;
    let STEP_SIZE: f32 = 2.0;

    var accumulated_color = vec3<f32>(0.0);
    var accumulated_alpha = 0.0;
    var t = 0.0;

    for (var i = 0; i < MAX_STEPS; i++) {
        if accumulated_alpha > 0.95 || t > MAX_DIST {
            break;
        }

        let pos = ray_origin + ray_dir * t;
        let density = nebula_density(pos);

        if density > 0.001 {
            // Color based on position and density
            let color_mix = smoothstep(0.0, 30.0, length(pos.xz));
            let color = mix(cloud.cloud_color_1, cloud.cloud_color_2, color_mix);

            // Emission
            let emission = color * density * 2.0;

            // Absorption
            let alpha = 1.0 - exp(-density * STEP_SIZE * cloud.absorption);

            // Front-to-back compositing
            accumulated_color += emission * (1.0 - accumulated_alpha);
            accumulated_alpha += alpha * (1.0 - accumulated_alpha);
        }

        t += STEP_SIZE;
    }

    return vec4<f32>(accumulated_color, accumulated_alpha);
}

// ============================================================================
// VERTEX SHADER
// ============================================================================

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    var output: VertexOutput;

    // Fullscreen triangle
    let x = f32((vertex_index << 1u) & 2u);
    let y = f32(vertex_index & 2u);

    output.position = vec4<f32>(x * 2.0 - 1.0, 1.0 - y * 2.0, 0.0, 1.0);

    // Calculate ray direction
    let clip_pos = vec4<f32>(x * 2.0 - 1.0, 1.0 - y * 2.0, 1.0, 1.0);
    let world_pos = cloud.inverse_view_projection * clip_pos;
    output.ray_dir = normalize(world_pos.xyz / world_pos.w - cloud.camera_position);

    return output;
}

// ============================================================================
// FRAGMENT SHADER
// ============================================================================

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let result = raymarch_nebula(cloud.camera_position, normalize(input.ray_dir));

    // Pre-multiply alpha for additive blending compatibility
    return vec4<f32>(result.rgb * result.a, result.a);
}
