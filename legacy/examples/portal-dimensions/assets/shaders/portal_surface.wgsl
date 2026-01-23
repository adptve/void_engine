// ============================================================================
// PORTAL SURFACE SHADER
// ============================================================================
// Creates the mesmerizing portal effect with:
// - Swirling vortex pattern
// - Edge glow and Fresnel
// - Dimension-specific coloring
// - Reality distortion ripples
// - Recursive depth indication
// ============================================================================

struct PortalUniforms {
    model_matrix: mat4x4<f32>,
    view_matrix: mat4x4<f32>,
    projection_matrix: mat4x4<f32>,
    time: f32,
    pulse: f32,
    swirl_angle: f32,
    dimension_color: vec4<f32>,
    distortion: f32,
    recursion_level: u32,
}

@group(0) @binding(0) var<uniform> portal: PortalUniforms;
@group(0) @binding(1) var portal_texture: texture_2d<f32>;
@group(0) @binding(2) var portal_sampler: sampler;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_position: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) view_dir: vec3<f32>,
}

// ============================================================================
// VERTEX SHADER
// ============================================================================

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    let world_pos = (portal.model_matrix * vec4<f32>(input.position, 1.0)).xyz;
    output.world_position = world_pos;
    output.world_normal = normalize((portal.model_matrix * vec4<f32>(input.normal, 0.0)).xyz);
    output.uv = input.uv;

    // View direction for Fresnel
    let camera_pos = -portal.view_matrix[3].xyz;
    output.view_dir = normalize(camera_pos - world_pos);

    output.clip_position = portal.projection_matrix * portal.view_matrix * vec4<f32>(world_pos, 1.0);

    return output;
}

// ============================================================================
// NOISE FUNCTIONS
// ============================================================================

fn hash21(p: vec2<f32>) -> f32 {
    let p3 = fract(vec3<f32>(p.x, p.y, p.x) * 0.1031);
    let p3_shifted = p3 + dot(p3, p3.yzx + 33.33);
    return fract((p3_shifted.x + p3_shifted.y) * p3_shifted.z);
}

fn noise2d(p: vec2<f32>) -> f32 {
    let i = floor(p);
    let f = fract(p);
    let u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(hash21(i + vec2<f32>(0.0, 0.0)), hash21(i + vec2<f32>(1.0, 0.0)), u.x),
        mix(hash21(i + vec2<f32>(0.0, 1.0)), hash21(i + vec2<f32>(1.0, 1.0)), u.x),
        u.y
    );
}

fn fbm(p: vec2<f32>, octaves: i32) -> f32 {
    var value = 0.0;
    var amplitude = 0.5;
    var pos = p;

    for (var i = 0; i < octaves; i++) {
        value += amplitude * noise2d(pos);
        pos *= 2.0;
        amplitude *= 0.5;
    }

    return value;
}

// ============================================================================
// FRAGMENT SHADER
// ============================================================================

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // Centered UV (-1 to 1)
    let centered_uv = input.uv * 2.0 - 1.0;
    let radius = length(centered_uv);
    let angle = atan2(centered_uv.y, centered_uv.x);

    // ========== PORTAL BOUNDARY ==========
    // Elliptical portal shape
    let portal_edge = smoothstep(1.0, 0.95, radius);

    if portal_edge < 0.01 {
        discard;
    }

    // ========== SWIRLING VORTEX ==========
    // Spiral distortion
    let spiral_factor = radius * 3.0;
    let spiral_angle = angle + portal.swirl_angle * (1.0 - radius);

    // Animated noise for organic movement
    let noise_uv = vec2<f32>(
        cos(spiral_angle) * radius * 2.0 + portal.time * 0.2,
        sin(spiral_angle) * radius * 2.0 + portal.time * 0.15
    );
    let swirl_noise = fbm(noise_uv, 4);

    // ========== DEPTH RINGS ==========
    let ring_count = 8.0;
    let ring_speed = portal.time * 2.0;
    let rings = sin((radius * ring_count - ring_speed) * 3.14159) * 0.5 + 0.5;
    let ring_intensity = rings * (1.0 - radius) * 0.5;

    // ========== ENERGY TENDRILS ==========
    let tendril_count = 6.0;
    let tendril_angle = angle * tendril_count + portal.time;
    let tendrils = pow(abs(sin(tendril_angle)), 8.0) * (1.0 - radius * 0.5);

    // ========== CENTER GLOW ==========
    let center_glow = exp(-radius * 3.0);

    // ========== EDGE FRESNEL ==========
    let fresnel = pow(1.0 - abs(dot(input.world_normal, input.view_dir)), 3.0);
    let edge_glow = fresnel * smoothstep(0.7, 1.0, radius);

    // ========== COMBINE PATTERNS ==========
    var intensity = 0.0;
    intensity += center_glow * 2.0;
    intensity += ring_intensity;
    intensity += tendrils * 0.3;
    intensity += swirl_noise * 0.4 * (1.0 - radius);
    intensity += edge_glow * 0.5;

    // Pulse animation
    intensity *= 0.8 + portal.pulse * 0.4;

    // ========== COLOR ==========
    let base_color = portal.dimension_color.rgb;

    // Color variation based on radius
    let inner_color = base_color * 1.5;
    let outer_color = base_color * 0.5 + vec3<f32>(0.1, 0.0, 0.2);
    var final_color = mix(inner_color, outer_color, radius);

    // Add white core
    final_color = mix(final_color, vec3<f32>(1.0, 1.0, 1.0), center_glow * 0.5);

    // Apply intensity
    final_color *= intensity;

    // ========== DISTORTION OVERLAY ==========
    // Sample what's "through" the portal with distortion
    let distort_uv = input.uv + vec2<f32>(
        sin(input.uv.y * 10.0 + portal.time) * portal.distortion * 0.1,
        cos(input.uv.x * 10.0 + portal.time) * portal.distortion * 0.1
    );

    let through_color = textureSample(portal_texture, portal_sampler, distort_uv);
    final_color = mix(through_color.rgb, final_color, 0.3 + center_glow * 0.7);

    // ========== RECURSION DEPTH INDICATOR ==========
    // Darker for deeper recursion levels
    let depth_darken = 1.0 - f32(portal.recursion_level) * 0.15;
    final_color *= depth_darken;

    // ========== FINAL ALPHA ==========
    let alpha = portal_edge * (0.7 + intensity * 0.3);

    // HDR values for bloom pickup
    return vec4<f32>(final_color * 1.5, alpha);
}
