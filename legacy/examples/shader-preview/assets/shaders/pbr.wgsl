// ============================================================================
// PBR (Physically Based Rendering) Shader
// ============================================================================
// Implements a metallic-roughness PBR workflow with:
// - Base color (albedo) with optional texture
// - Metallic-roughness (packed or separate)
// - Normal mapping
// - Ambient occlusion
// - Emissive
// - IBL (Image-Based Lighting) approximation
// - Point/directional lights
// ============================================================================

// Constants
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
}

struct ModelUniforms {
    model: mat4x4<f32>,
    normal_matrix: mat4x4<f32>,
}

struct MaterialUniforms {
    base_color_factor: vec4<f32>,
    emissive_factor: vec3<f32>,
    metallic_factor: f32,
    roughness_factor: f32,
    normal_scale: f32,
    occlusion_strength: f32,
    alpha_cutoff: f32,
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
@group(2) @binding(0) var<uniform> material: MaterialUniforms;
@group(2) @binding(1) var base_color_texture: texture_2d<f32>;
@group(2) @binding(2) var base_color_sampler: sampler;
@group(2) @binding(3) var normal_texture: texture_2d<f32>;
@group(2) @binding(4) var normal_sampler: sampler;
@group(2) @binding(5) var metallic_roughness_texture: texture_2d<f32>;
@group(2) @binding(6) var metallic_roughness_sampler: sampler;
@group(3) @binding(0) var<uniform> light: LightUniforms;

// ============================================================================
// Vertex I/O
// ============================================================================

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) tangent: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_position: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) world_tangent: vec3<f32>,
    @location(4) world_bitangent: vec3<f32>,
}

// ============================================================================
// Vertex Shader
// ============================================================================

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    let world_pos = model.model * vec4<f32>(input.position, 1.0);
    output.world_position = world_pos.xyz;
    output.clip_position = camera.view_proj * world_pos;

    // Transform normal to world space
    output.world_normal = normalize((model.normal_matrix * vec4<f32>(input.normal, 0.0)).xyz);

    // Transform tangent to world space
    output.world_tangent = normalize((model.model * vec4<f32>(input.tangent.xyz, 0.0)).xyz);

    // Calculate bitangent with handedness
    output.world_bitangent = cross(output.world_normal, output.world_tangent) * input.tangent.w;

    output.uv = input.uv;

    return output;
}

// ============================================================================
// PBR Functions
// ============================================================================

// Fresnel-Schlick approximation
fn fresnel_schlick(cos_theta: f32, f0: vec3<f32>) -> vec3<f32> {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness for IBL
fn fresnel_schlick_roughness(cos_theta: f32, f0: vec3<f32>, roughness: f32) -> vec3<f32> {
    return f0 + (max(vec3<f32>(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// GGX/Trowbridge-Reitz normal distribution function
fn distribution_ggx(n_dot_h: f32, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let n_dot_h2 = n_dot_h * n_dot_h;

    let num = a2;
    var denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Smith's Schlick-GGX geometry function
fn geometry_schlick_ggx(n_dot_v: f32, roughness: f32) -> f32 {
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;

    let num = n_dot_v;
    let denom = n_dot_v * (1.0 - k) + k;

    return num / denom;
}

// Smith's geometry function for both view and light directions
fn geometry_smith(n_dot_v: f32, n_dot_l: f32, roughness: f32) -> f32 {
    let ggx1 = geometry_schlick_ggx(n_dot_v, roughness);
    let ggx2 = geometry_schlick_ggx(n_dot_l, roughness);
    return ggx1 * ggx2;
}

// ============================================================================
// Fragment Shader
// ============================================================================

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // Sample textures
    let base_color_sample = textureSample(base_color_texture, base_color_sampler, input.uv);
    let base_color = base_color_sample.rgb * material.base_color_factor.rgb;
    let alpha = base_color_sample.a * material.base_color_factor.a;

    // Alpha test
    if alpha < material.alpha_cutoff {
        discard;
    }

    // Sample metallic-roughness (G = roughness, B = metallic)
    let mr_sample = textureSample(metallic_roughness_texture, metallic_roughness_sampler, input.uv);
    let metallic = mr_sample.b * material.metallic_factor;
    let roughness = max(mr_sample.g * material.roughness_factor, MIN_ROUGHNESS);

    // Sample and transform normal
    let normal_sample = textureSample(normal_texture, normal_sampler, input.uv).xyz;
    let tangent_normal = normalize(normal_sample * 2.0 - 1.0) * vec3<f32>(material.normal_scale, material.normal_scale, 1.0);

    // TBN matrix
    let tbn = mat3x3<f32>(
        normalize(input.world_tangent),
        normalize(input.world_bitangent),
        normalize(input.world_normal)
    );
    let n = normalize(tbn * tangent_normal);

    // View direction
    let v = normalize(camera.camera_pos - input.world_position);

    // Light direction (directional light)
    let l = normalize(-light.direction);

    // Half vector
    let h = normalize(v + l);

    // Dot products
    let n_dot_v = max(dot(n, v), 0.001);
    let n_dot_l = max(dot(n, l), 0.0);
    let n_dot_h = max(dot(n, h), 0.0);
    let h_dot_v = max(dot(h, v), 0.0);

    // Calculate F0 (reflectance at normal incidence)
    // For dielectrics, use 0.04; for metals, use base color
    let f0 = mix(vec3<f32>(0.04), base_color, metallic);

    // Cook-Torrance BRDF
    let d = distribution_ggx(n_dot_h, roughness);
    let g = geometry_smith(n_dot_v, n_dot_l, roughness);
    let f = fresnel_schlick(h_dot_v, f0);

    // Specular component
    let numerator = d * g * f;
    let denominator = 4.0 * n_dot_v * n_dot_l + 0.0001;
    let specular = numerator / denominator;

    // Energy conservation: specular + diffuse = 1
    let ks = f;
    var kd = vec3<f32>(1.0) - ks;
    // Metals have no diffuse
    kd = kd * (1.0 - metallic);

    // Lambertian diffuse
    let diffuse = kd * base_color / PI;

    // Direct lighting contribution
    let radiance = light.color * light.intensity;
    let direct = (diffuse + specular) * radiance * n_dot_l;

    // Ambient/IBL approximation
    let f_ambient = fresnel_schlick_roughness(n_dot_v, f0, roughness);
    let kd_ambient = (1.0 - f_ambient) * (1.0 - metallic);
    let ambient_diffuse = kd_ambient * base_color * light.ambient_color * light.ambient_intensity;

    // Simple ambient specular approximation
    let ambient_specular = f_ambient * light.ambient_color * light.ambient_intensity * 0.3;

    let ambient = ambient_diffuse + ambient_specular;

    // Emissive
    let emissive = material.emissive_factor;

    // Final color
    var color = direct + ambient + emissive;

    // Tone mapping (Reinhard)
    color = color / (color + vec3<f32>(1.0));

    // Gamma correction
    color = pow(color, vec3<f32>(1.0 / 2.2));

    return vec4<f32>(color, alpha);
}

// ============================================================================
// Simple PBR Fragment Shader (no textures, just factors)
// ============================================================================

@fragment
fn fs_simple(input: VertexOutput) -> @location(0) vec4<f32> {
    let base_color = material.base_color_factor.rgb;
    let alpha = material.base_color_factor.a;
    let metallic = material.metallic_factor;
    let roughness = max(material.roughness_factor, MIN_ROUGHNESS);

    let n = normalize(input.world_normal);
    let v = normalize(camera.camera_pos - input.world_position);
    let l = normalize(-light.direction);
    let h = normalize(v + l);

    let n_dot_v = max(dot(n, v), 0.001);
    let n_dot_l = max(dot(n, l), 0.0);
    let n_dot_h = max(dot(n, h), 0.0);
    let h_dot_v = max(dot(h, v), 0.0);

    let f0 = mix(vec3<f32>(0.04), base_color, metallic);

    let d = distribution_ggx(n_dot_h, roughness);
    let g = geometry_smith(n_dot_v, n_dot_l, roughness);
    let f = fresnel_schlick(h_dot_v, f0);

    let specular = (d * g * f) / (4.0 * n_dot_v * n_dot_l + 0.0001);

    let ks = f;
    let kd = (vec3<f32>(1.0) - ks) * (1.0 - metallic);
    let diffuse = kd * base_color / PI;

    let radiance = light.color * light.intensity;
    let direct = (diffuse + specular) * radiance * n_dot_l;

    let ambient = base_color * light.ambient_color * light.ambient_intensity * (1.0 - metallic * 0.5);
    let emissive = material.emissive_factor;

    var color = direct + ambient + emissive;
    color = color / (color + vec3<f32>(1.0));
    color = pow(color, vec3<f32>(1.0 / 2.2));

    return vec4<f32>(color, alpha);
}
