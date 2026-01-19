// ============================================================================
// PBR Shader with Texture Support
// ============================================================================
// Full PBR with:
// - Albedo/Base Color texture
// - Normal map (tangent space)
// - Metallic-Roughness texture (G=roughness, B=metallic)
// - Ambient Occlusion texture
// - Emissive texture
// - Fallback to uniform values when textures unavailable
// ============================================================================

const PI: f32 = 3.14159265359;
const MIN_ROUGHNESS: f32 = 0.04;

// ============================================================================
// Uniform Structures
// ============================================================================

struct CameraUniforms {
    view_proj: mat4x4<f32>,
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    camera_pos: vec3<f32>,
    _padding: f32,
};

struct ModelUniforms {
    model: mat4x4<f32>,
    normal_matrix: mat4x4<f32>,
};

struct MaterialUniforms {
    base_color: vec4<f32>,
    metallic: f32,
    roughness: f32,
    emissive_r: f32,
    emissive_g: f32,
    emissive_b: f32,
    ao_strength: f32,
    normal_scale: f32,
    // Texture flags: bit 0=albedo, 1=normal, 2=metallic_roughness, 3=ao, 4=emissive
    texture_flags: u32,
};

struct LightUniforms {
    direction: vec3<f32>,
    _pad1: f32,
    color: vec3<f32>,
    intensity: f32,
    ambient_color: vec3<f32>,
    ambient_intensity: f32,
};

// Uniform bindings
@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(1) @binding(0) var<uniform> model: ModelUniforms;
@group(2) @binding(0) var<uniform> material: MaterialUniforms;
@group(3) @binding(0) var<uniform> light: LightUniforms;

// Texture bindings (group 4)
@group(4) @binding(0) var albedo_texture: texture_2d<f32>;
@group(4) @binding(1) var albedo_sampler: sampler;
@group(4) @binding(2) var normal_texture: texture_2d<f32>;
@group(4) @binding(3) var normal_sampler: sampler;
@group(4) @binding(4) var metallic_roughness_texture: texture_2d<f32>;
@group(4) @binding(5) var metallic_roughness_sampler: sampler;
@group(4) @binding(6) var ao_texture: texture_2d<f32>;
@group(4) @binding(7) var ao_sampler: sampler;
@group(4) @binding(8) var emissive_texture: texture_2d<f32>;
@group(4) @binding(9) var emissive_sampler: sampler;

// ============================================================================
// Vertex I/O
// ============================================================================

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) tangent: vec4<f32>,  // xyz=tangent, w=handedness
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_position: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) world_tangent: vec3<f32>,
    @location(4) world_bitangent: vec3<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;

    let world_pos = model.model * vec4<f32>(in.position, 1.0);
    out.world_position = world_pos.xyz;
    out.clip_position = camera.view_proj * world_pos;

    // Transform normal and tangent to world space
    let world_normal = normalize((model.normal_matrix * vec4<f32>(in.normal, 0.0)).xyz);
    let world_tangent = normalize((model.model * vec4<f32>(in.tangent.xyz, 0.0)).xyz);

    // Compute bitangent with handedness
    let world_bitangent = cross(world_normal, world_tangent) * in.tangent.w;

    out.world_normal = world_normal;
    out.world_tangent = world_tangent;
    out.world_bitangent = world_bitangent;
    out.uv = in.uv;

    return out;
}

// ============================================================================
// Helper Functions
// ============================================================================

fn has_texture(flag: u32) -> bool {
    return (material.texture_flags & flag) != 0u;
}

const TEX_ALBEDO: u32 = 1u;
const TEX_NORMAL: u32 = 2u;
const TEX_METALLIC_ROUGHNESS: u32 = 4u;
const TEX_AO: u32 = 8u;
const TEX_EMISSIVE: u32 = 16u;

// ============================================================================
// PBR Functions
// ============================================================================

fn fresnel_schlick(cos_theta: f32, f0: vec3<f32>) -> vec3<f32> {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

fn fresnel_schlick_roughness(cos_theta: f32, f0: vec3<f32>, roughness: f32) -> vec3<f32> {
    let max_reflectance = max(vec3<f32>(1.0 - roughness), f0);
    return f0 + (max_reflectance - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

fn distribution_ggx(n_dot_h: f32, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let n_dot_h2 = n_dot_h * n_dot_h;

    let num = a2;
    var denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0001);
}

fn geometry_schlick_ggx(n_dot_v: f32, roughness: f32) -> f32 {
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;
    return n_dot_v / (n_dot_v * (1.0 - k) + k);
}

fn geometry_smith(n_dot_v: f32, n_dot_l: f32, roughness: f32) -> f32 {
    return geometry_schlick_ggx(n_dot_v, roughness) * geometry_schlick_ggx(n_dot_l, roughness);
}

// ============================================================================
// Environment Sampling (procedural HDR sky)
// ============================================================================

fn sample_environment_lod(dir: vec3<f32>, roughness: f32) -> vec3<f32> {
    let y = dir.y;
    let blur = roughness * roughness;

    let sky_zenith = vec3<f32>(0.2, 0.4, 0.9);
    let sky_horizon = vec3<f32>(0.7, 0.8, 1.0);
    let ground = vec3<f32>(0.15, 0.12, 0.1);

    var env: vec3<f32>;

    if y > 0.0 {
        let t = pow(y, 0.4);
        env = mix(sky_horizon, sky_zenith, t);

        let sun_dir = normalize(vec3<f32>(0.5, 0.7, -0.5));
        let sun_dot = max(dot(dir, sun_dir), 0.0);

        let sun_sharpness = mix(512.0, 4.0, blur);
        let sun_intensity = mix(12.0, 1.5, blur);
        env += vec3<f32>(1.0, 0.98, 0.95) * pow(sun_dot, sun_sharpness) * sun_intensity;

        let corona_sharpness = mix(64.0, 4.0, blur);
        env += vec3<f32>(1.0, 0.95, 0.85) * pow(sun_dot, corona_sharpness) * mix(2.0, 0.5, blur);

        let fill_dir = normalize(vec3<f32>(-0.7, 0.5, 0.5));
        let fill_dot = max(dot(dir, fill_dir), 0.0);
        env += vec3<f32>(0.6, 0.7, 1.0) * pow(fill_dot, mix(32.0, 4.0, blur)) * mix(2.0, 0.5, blur);
    } else {
        let t = pow(-y, 0.5);
        env = mix(sky_horizon * 0.5, ground, t);
    }

    env += vec3<f32>(1.0, 0.95, 0.85) * pow(1.0 - abs(y), 6.0) * 0.4;

    return env;
}

// ============================================================================
// Fragment Shader
// ============================================================================

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Sample textures or use uniform values
    var base_color: vec3<f32>;
    var alpha: f32;
    if has_texture(TEX_ALBEDO) {
        let albedo_sample = textureSample(albedo_texture, albedo_sampler, in.uv);
        base_color = albedo_sample.rgb * material.base_color.rgb;
        alpha = albedo_sample.a * material.base_color.a;
    } else {
        base_color = material.base_color.rgb;
        alpha = material.base_color.a;
    }

    var metallic: f32;
    var roughness: f32;
    if has_texture(TEX_METALLIC_ROUGHNESS) {
        // glTF standard: G=roughness, B=metallic
        let mr_sample = textureSample(metallic_roughness_texture, metallic_roughness_sampler, in.uv);
        roughness = mr_sample.g * material.roughness;
        metallic = mr_sample.b * material.metallic;
    } else {
        metallic = material.metallic;
        roughness = material.roughness;
    }
    roughness = max(roughness, MIN_ROUGHNESS);

    var ao: f32 = 1.0;
    if has_texture(TEX_AO) {
        ao = textureSample(ao_texture, ao_sampler, in.uv).r;
        ao = mix(1.0, ao, material.ao_strength);
    }

    var emissive: vec3<f32>;
    if has_texture(TEX_EMISSIVE) {
        emissive = textureSample(emissive_texture, emissive_sampler, in.uv).rgb;
        emissive *= vec3<f32>(material.emissive_r, material.emissive_g, material.emissive_b);
    } else {
        emissive = vec3<f32>(material.emissive_r, material.emissive_g, material.emissive_b);
    }

    // Normal mapping
    var n: vec3<f32>;
    if has_texture(TEX_NORMAL) {
        // Sample normal map (stored as RGB where 0.5 = zero)
        let normal_sample = textureSample(normal_texture, normal_sampler, in.uv).rgb;
        var tangent_normal = normal_sample * 2.0 - 1.0;
        tangent_normal.xy *= material.normal_scale;
        tangent_normal = normalize(tangent_normal);

        // Construct TBN matrix and transform to world space
        let tbn = mat3x3<f32>(
            normalize(in.world_tangent),
            normalize(in.world_bitangent),
            normalize(in.world_normal)
        );
        n = normalize(tbn * tangent_normal);
    } else {
        n = normalize(in.world_normal);
    }

    let v = normalize(camera.camera_pos - in.world_position);
    let l = normalize(-light.direction);
    let h = normalize(v + l);
    let r = reflect(-v, n);

    // Dot products
    let n_dot_v = max(dot(n, v), 0.001);
    let n_dot_l = max(dot(n, l), 0.0);
    let n_dot_h = max(dot(n, h), 0.0);
    let h_dot_v = max(dot(h, v), 0.0);

    // F0 - reflectance at normal incidence
    let f0 = mix(vec3<f32>(0.04), base_color, metallic);

    // Cook-Torrance BRDF
    let d = distribution_ggx(n_dot_h, roughness);
    let g = geometry_smith(n_dot_v, n_dot_l, roughness);
    let f = fresnel_schlick(h_dot_v, f0);

    let numerator = d * g * f;
    let denominator = 4.0 * n_dot_v * n_dot_l + 0.0001;
    let specular = numerator / denominator;

    // Energy conservation
    let ks = f;
    let kd = (vec3<f32>(1.0) - ks) * (1.0 - metallic);
    let diffuse = kd * base_color / PI;

    // ========== DIRECT LIGHTING ==========

    let radiance = light.color * light.intensity;
    var direct = (diffuse + specular) * radiance * n_dot_l;

    // Fill light
    let fill_dir = normalize(vec3<f32>(-0.6, 0.4, 0.7));
    let fill_n_dot_l = max(dot(n, fill_dir), 0.0);
    let fill_radiance = vec3<f32>(0.5, 0.6, 0.8) * light.intensity * 0.3;
    direct += (diffuse + specular * 0.3) * fill_radiance * fill_n_dot_l;

    // Back light
    let back_dir = normalize(vec3<f32>(0.0, 0.3, -1.0));
    let back_n_dot_l = max(dot(n, back_dir), 0.0);
    direct += diffuse * vec3<f32>(0.4, 0.45, 0.6) * light.intensity * back_n_dot_l * 0.25;

    // ========== ENVIRONMENT / IBL ==========

    let env = sample_environment_lod(r, roughness);
    let fresnel_env = fresnel_schlick_roughness(n_dot_v, f0, roughness);
    let metal_tint = mix(vec3<f32>(1.0), base_color, metallic);
    let env_strength = mix(0.4, 1.0, metallic);
    let env_reflection = env * fresnel_env * metal_tint * env_strength;

    // Diffuse IBL
    let hemisphere_blend = n.y * 0.5 + 0.5;
    let sky_irradiance = vec3<f32>(0.6, 0.7, 0.9);
    let ground_irradiance = vec3<f32>(0.25, 0.22, 0.2);
    let diffuse_ibl = mix(ground_irradiance, sky_irradiance, hemisphere_blend);
    let ambient_diffuse = kd * base_color * diffuse_ibl * light.ambient_intensity;

    let ambient = (ambient_diffuse + env_reflection) * ao;

    // ========== RIM LIGHT ==========

    let rim = pow(1.0 - n_dot_v, 3.0) * 0.6;
    let rim_color = mix(vec3<f32>(0.5, 0.6, 0.8), light.color, 0.4);
    let rim_light = rim_color * rim;

    // ========== FINAL COMBINATION ==========

    var color = direct + ambient + rim_light + emissive;

    // ACES tone mapping
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d2 = 0.59;
    let e = 0.14;
    color = clamp((color * (a * color + b)) / (color * (c * color + d2) + e), vec3<f32>(0.0), vec3<f32>(1.0));

    return vec4<f32>(color, alpha);
}
