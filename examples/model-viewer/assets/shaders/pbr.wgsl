// ============================================================================
// PBR (Physically Based Rendering) Shader
// ============================================================================
// Metallic-roughness workflow with:
// - Cook-Torrance BRDF
// - GGX normal distribution
// - Schlick-GGX geometry function
// - Fresnel-Schlick approximation
// - Environment reflection approximation
// - PBR Texture support (albedo, normal, roughness)
// ============================================================================

const PI: f32 = 3.14159265359;

// Texture flags (must match Rust PbrTextures::get_flags())
const HAS_ALBEDO_TEX: u32 = 1u;
const HAS_NORMAL_TEX: u32 = 2u;
const HAS_ROUGHNESS_TEX: u32 = 4u;
const HAS_AO_TEX: u32 = 8u;
const HAS_EMISSIVE_TEX: u32 = 16u;

struct CameraUniforms {
    view_proj: mat4x4<f32>,
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    camera_pos: vec3<f32>,
    _padding: f32,
}

struct ModelUniforms {
    model: mat4x4<f32>,
    normal_matrix: mat4x4<f32>,
}

struct MaterialUniforms {
    base_color: vec4<f32>,
    metallic: f32,
    roughness: f32,
    emissive_r: f32,
    emissive_g: f32,
    emissive_b: f32,
    normal_scale: f32,
    ao_strength: f32,
    texture_flags: u32,
}

struct LightUniforms {
    direction: vec3<f32>,
    _pad1: f32,
    color: vec3<f32>,
    intensity: f32,
    ambient_color: vec3<f32>,
    ambient_intensity: f32,
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(1) @binding(0) var<uniform> model: ModelUniforms;

// Group 2: Material uniforms + textures
@group(2) @binding(0) var<uniform> material: MaterialUniforms;
@group(2) @binding(1) var t_albedo: texture_2d<f32>;
@group(2) @binding(2) var s_albedo: sampler;
@group(2) @binding(3) var t_normal: texture_2d<f32>;
@group(2) @binding(4) var s_normal: sampler;
@group(2) @binding(5) var t_roughness: texture_2d<f32>;
@group(2) @binding(6) var s_roughness: sampler;

@group(3) @binding(0) var<uniform> light: LightUniforms;

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
}

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    let world_pos = model.model * vec4<f32>(input.position, 1.0);
    output.world_position = world_pos.xyz;
    output.clip_position = camera.view_proj * world_pos;
    output.world_normal = normalize((model.normal_matrix * vec4<f32>(input.normal, 0.0)).xyz);
    output.uv = input.uv;
    return output;
}

// Fresnel-Schlick approximation
fn fresnel_schlick(cos_theta: f32, f0: vec3<f32>) -> vec3<f32> {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// GGX/Trowbridge-Reitz normal distribution
fn distribution_ggx(n_dot_h: f32, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let n_dot_h2 = n_dot_h * n_dot_h;
    var denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / denom;
}

// Schlick-GGX geometry function
fn geometry_schlick_ggx(n_dot_v: f32, roughness: f32) -> f32 {
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;
    return n_dot_v / (n_dot_v * (1.0 - k) + k);
}

// Smith's geometry function
fn geometry_smith(n_dot_v: f32, n_dot_l: f32, roughness: f32) -> f32 {
    return geometry_schlick_ggx(n_dot_v, roughness) * geometry_schlick_ggx(n_dot_l, roughness);
}

// Environment color approximation
fn sample_environment(dir: vec3<f32>) -> vec3<f32> {
    let y = dir.y;

    let sky_zenith = vec3<f32>(0.1, 0.3, 0.7);
    let sky_horizon = vec3<f32>(0.5, 0.7, 0.9);
    let ground = vec3<f32>(0.15, 0.12, 0.1);

    if y > 0.0 {
        let t = pow(y, 0.3);
        var env = mix(sky_horizon, sky_zenith, t);

        // Sun highlight
        let sun_dir = normalize(-light.direction);
        let sun_dot = max(dot(dir, sun_dir), 0.0);
        env += vec3<f32>(1.0, 0.9, 0.7) * pow(sun_dot, 64.0) * 2.0;
        env += vec3<f32>(1.0, 0.8, 0.5) * pow(sun_dot, 8.0) * 0.5;

        return env;
    } else {
        return mix(sky_horizon * 0.4, ground, pow(-y, 0.5));
    }
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let flags = material.texture_flags;

    // Sample albedo: use texture if available, otherwise uniform color
    var base_color: vec3<f32>;
    if (flags & HAS_ALBEDO_TEX) != 0u {
        base_color = textureSample(t_albedo, s_albedo, input.uv).rgb;
    } else {
        base_color = material.base_color.rgb;
    }

    // Sample roughness: use texture if available, otherwise uniform
    var roughness: f32;
    if (flags & HAS_ROUGHNESS_TEX) != 0u {
        roughness = textureSample(t_roughness, s_roughness, input.uv).r;
    } else {
        roughness = material.roughness;
    }
    roughness = max(roughness, 0.04);

    let metallic = material.metallic;

    // Normal mapping
    var n: vec3<f32>;
    if (flags & HAS_NORMAL_TEX) != 0u {
        let tangent_normal = textureSample(t_normal, s_normal, input.uv).rgb * 2.0 - 1.0;
        // Simple normal perturbation (proper TBN would be better)
        let world_normal = normalize(input.world_normal);
        n = normalize(world_normal + tangent_normal * material.normal_scale * 0.5);
    } else {
        n = normalize(input.world_normal);
    }

    let v = normalize(camera.camera_pos - input.world_position);
    let l = normalize(-light.direction);
    let h = normalize(v + l);
    let r = reflect(-v, n);

    let n_dot_v = max(dot(n, v), 0.001);
    let n_dot_l = max(dot(n, l), 0.0);
    let n_dot_h = max(dot(n, h), 0.0);
    let h_dot_v = max(dot(h, v), 0.0);

    // F0: reflectance at normal incidence
    let f0 = mix(vec3<f32>(0.04), base_color, metallic);

    // Cook-Torrance BRDF
    let d = distribution_ggx(n_dot_h, roughness);
    let g = geometry_smith(n_dot_v, n_dot_l, roughness);
    let f = fresnel_schlick(h_dot_v, f0);

    let specular = (d * g * f) / (4.0 * n_dot_v * n_dot_l + 0.0001);

    let ks = f;
    let kd = (vec3<f32>(1.0) - ks) * (1.0 - metallic);
    let diffuse = kd * base_color / PI;

    // Direct lighting
    let radiance = light.color * light.intensity;
    var direct = (diffuse + specular) * radiance * n_dot_l;

    // Fill light
    let fill_dir = normalize(vec3<f32>(-0.6, 0.4, 0.7));
    let fill_n_dot_l = max(dot(n, fill_dir), 0.0);
    direct += diffuse * vec3<f32>(0.4, 0.45, 0.6) * fill_n_dot_l * 0.3;

    // Environment reflection
    let env = sample_environment(r);
    let fresnel_env = fresnel_schlick(n_dot_v, f0);
    let env_strength = mix(0.2, 0.8, metallic);
    let env_reflection = env * fresnel_env * env_strength * (1.0 - roughness * 0.7);

    // Ambient diffuse
    let hemisphere = n.y * 0.5 + 0.5;
    let ambient_diffuse = kd * base_color * mix(
        vec3<f32>(0.2, 0.18, 0.15),
        vec3<f32>(0.5, 0.55, 0.7),
        hemisphere
    ) * light.ambient_intensity;

    // Combine
    let ambient = ambient_diffuse + env_reflection;

    // Rim light
    let rim = pow(1.0 - n_dot_v, 3.0) * 0.3;
    let rim_light = vec3<f32>(0.4, 0.5, 0.7) * rim;

    let emissive = vec3<f32>(material.emissive_r, material.emissive_g, material.emissive_b);
    var color = direct + ambient + rim_light + emissive;

    // ACES tone mapping
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d2 = 0.59;
    let e = 0.14;
    color = clamp((color * (a * color + b)) / (color * (c * color + d2) + e), vec3<f32>(0.0), vec3<f32>(1.0));

    // Gamma correction
    color = pow(color, vec3<f32>(1.0 / 2.2));

    return vec4<f32>(color, material.base_color.a);
}
