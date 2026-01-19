// ============================================================================
// PBR Shader - Full Featured with HDR Environment
// ============================================================================
// Based on research from LearnOpenGL, Filament, and Three.js
// - Cook-Torrance BRDF with GGX/Smith
// - HDR procedural environment (bright sun for specular)
// - Fresnel-Schlick with roughness for IBL
// - Multi-light setup (key + fill + back)
// - ACES tone mapping
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
    normal_scale: f32,
    ao_strength: f32,
    // Texture flags: bit 0=albedo, 1=normal, 2=metallic_roughness
    texture_flags: u32,
};

// Texture flag constants
const TEX_ALBEDO: u32 = 1u;
const TEX_NORMAL: u32 = 2u;
const TEX_METALLIC_ROUGHNESS: u32 = 4u;

struct LightUniforms {
    direction: vec3<f32>,
    _pad1: f32,
    color: vec3<f32>,
    intensity: f32,
    ambient_color: vec3<f32>,
    ambient_intensity: f32,
};

// Shadow uniforms (light space matrix + config)
struct ShadowUniforms {
    light_view_proj: mat4x4<f32>,
    // Shadow config: x=bias, y=normal_bias, z=softness, w=enabled
    config: vec4<f32>,
};

// Bind group layout:
// Group 0: Camera + Environment (per-frame)
// Group 1: Model (per-object)
// Group 2: Material + Textures (per-object, combined to stay within 4 group limit)
// Group 3: Light + Shadow (per-frame, combined for 4-group limit)

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(0) @binding(1) var env_texture: texture_2d<f32>;
@group(0) @binding(2) var env_sampler: sampler;

@group(1) @binding(0) var<uniform> model: ModelUniforms;

// Material uniforms and textures combined in group 2
@group(2) @binding(0) var<uniform> material: MaterialUniforms;
@group(2) @binding(1) var albedo_texture: texture_2d<f32>;
@group(2) @binding(2) var albedo_sampler: sampler;
@group(2) @binding(3) var normal_texture: texture_2d<f32>;
@group(2) @binding(4) var normal_sampler: sampler;
@group(2) @binding(5) var metallic_roughness_texture: texture_2d<f32>;
@group(2) @binding(6) var metallic_roughness_sampler: sampler;

// Light + Shadow combined in group 3
@group(3) @binding(0) var<uniform> light: LightUniforms;
@group(3) @binding(1) var shadow_map: texture_depth_2d;
@group(3) @binding(2) var shadow_sampler: sampler_comparison;
@group(3) @binding(3) var<uniform> shadow: ShadowUniforms;

// ============================================================================
// Vertex I/O
// ============================================================================

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_position: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) shadow_coord: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;

    let world_pos = model.model * vec4<f32>(in.position, 1.0);
    out.world_position = world_pos.xyz;

    // Calculate shadow coordinates (light space position)
    out.shadow_coord = shadow.light_view_proj * world_pos;
    out.clip_position = camera.view_proj * world_pos;
    out.world_normal = normalize((model.normal_matrix * vec4<f32>(in.normal, 0.0)).xyz);
    out.uv = in.uv;

    return out;
}

// ============================================================================
// PBR Functions
// ============================================================================

// Standard Fresnel-Schlick
fn fresnel_schlick(cos_theta: f32, f0: vec3<f32>) -> vec3<f32> {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness for IBL (important for metals!)
fn fresnel_schlick_roughness(cos_theta: f32, f0: vec3<f32>, roughness: f32) -> vec3<f32> {
    let max_reflectance = max(vec3<f32>(1.0 - roughness), f0);
    return f0 + (max_reflectance - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// GGX/Trowbridge-Reitz normal distribution
fn distribution_ggx(n_dot_h: f32, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let n_dot_h2 = n_dot_h * n_dot_h;

    let num = a2;
    var denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0001);
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

// ============================================================================
// HDR Environment / IBL
// ============================================================================

// Sample equirectangular environment map
fn sample_equirectangular(dir: vec3<f32>) -> vec3<f32> {
    // Convert direction to UV coordinates
    // phi = atan2(z, x), theta = acos(y)
    let phi = atan2(dir.z, dir.x);
    let theta = acos(clamp(dir.y, -1.0, 1.0));

    // Map to [0, 1] UV range
    let u = (phi + PI) / (2.0 * PI);
    let v = theta / PI;

    return textureSample(env_texture, env_sampler, vec2<f32>(u, v)).rgb;
}

// Procedural HDR environment with BRIGHT sun for specular highlights (fallback)
fn sample_environment_procedural(dir: vec3<f32>) -> vec3<f32> {
    let y = dir.y;

    // Sky gradient - more saturated colors
    let sky_zenith = vec3<f32>(0.2, 0.4, 0.9);    // Deep blue
    let sky_horizon = vec3<f32>(0.7, 0.8, 1.0);   // Light blue
    let ground = vec3<f32>(0.15, 0.12, 0.1);      // Warm brown

    var env: vec3<f32>;

    if y > 0.0 {
        let t = pow(y, 0.4);
        env = mix(sky_horizon, sky_zenith, t);

        // BRIGHT HDR SUN - critical for metallic specular!
        let sun_dir = normalize(vec3<f32>(0.5, 0.7, -0.5));
        let sun_dot = max(dot(dir, sun_dir), 0.0);

        // Sun disk - HDR but controlled
        let sun_disk = pow(sun_dot, 512.0);
        env += vec3<f32>(1.0, 0.98, 0.95) * sun_disk * 12.0;

        // Sun corona
        let sun_corona = pow(sun_dot, 64.0);
        env += vec3<f32>(1.0, 0.95, 0.85) * sun_corona * 2.0;

        // Soft glow
        let sun_glow = pow(sun_dot, 8.0);
        env += vec3<f32>(1.0, 0.9, 0.7) * sun_glow * 0.5;

        // Secondary light source (opposite side)
        let fill_dir = normalize(vec3<f32>(-0.7, 0.5, 0.5));
        let fill_dot = max(dot(dir, fill_dir), 0.0);
        env += vec3<f32>(0.6, 0.7, 1.0) * pow(fill_dot, 32.0) * 2.0;

    } else {
        // Ground reflection
        let t = pow(-y, 0.5);
        env = mix(sky_horizon * 0.5, ground, t);
    }

    // Horizon glow (atmospheric scattering)
    let horizon_factor = 1.0 - abs(y);
    env += vec3<f32>(1.0, 0.95, 0.85) * pow(horizon_factor, 6.0) * 0.5;

    return env;
}

// Sample environment with roughness-based blur (procedural fallback)
fn sample_environment_lod_procedural(dir: vec3<f32>, roughness: f32) -> vec3<f32> {
    let y = dir.y;
    let blur = roughness * roughness;  // Perceptual roughness squared

    // Sky gradient (always visible, even at high roughness)
    let sky_zenith = vec3<f32>(0.2, 0.4, 0.9);
    let sky_horizon = vec3<f32>(0.7, 0.8, 1.0);
    let ground = vec3<f32>(0.15, 0.12, 0.1);

    var env: vec3<f32>;

    if y > 0.0 {
        let t = pow(y, 0.4);
        env = mix(sky_horizon, sky_zenith, t);

        // Sun - sharpness decreases with roughness, but still contributes!
        let sun_dir = normalize(vec3<f32>(0.5, 0.7, -0.5));
        let sun_dot = max(dot(dir, sun_dir), 0.0);

        // Sharper sun for smooth surfaces, wider/softer for rough
        let sun_sharpness = mix(512.0, 4.0, blur);
        let sun_intensity = mix(12.0, 1.5, blur);
        env += vec3<f32>(1.0, 0.98, 0.95) * pow(sun_dot, sun_sharpness) * sun_intensity;

        // Corona - always present, just wider
        let corona_sharpness = mix(64.0, 4.0, blur);
        env += vec3<f32>(1.0, 0.95, 0.85) * pow(sun_dot, corona_sharpness) * mix(2.0, 0.5, blur);

        // Fill light
        let fill_dir = normalize(vec3<f32>(-0.7, 0.5, 0.5));
        let fill_dot = max(dot(dir, fill_dir), 0.0);
        env += vec3<f32>(0.6, 0.7, 1.0) * pow(fill_dot, mix(32.0, 4.0, blur)) * mix(2.0, 0.5, blur);

    } else {
        let t = pow(-y, 0.5);
        env = mix(sky_horizon * 0.5, ground, t);
    }

    // Horizon glow
    env += vec3<f32>(1.0, 0.95, 0.85) * pow(1.0 - abs(y), 6.0) * 0.4;

    return env;
}

// Main environment sampling function - uses HDR texture
fn sample_environment(dir: vec3<f32>) -> vec3<f32> {
    return sample_equirectangular(dir);
}

// Sample environment with roughness (blur approximation)
// Without mipmaps, we approximate blur by mixing with a diffuse average
fn sample_environment_lod(dir: vec3<f32>, roughness: f32) -> vec3<f32> {
    let sharp = sample_equirectangular(dir);

    // For rough surfaces, blend towards a diffuse hemisphere average
    // This simulates the effect of pre-filtered environment maps
    let blur = roughness * roughness;

    // Sample several directions for rough approximation
    if blur > 0.1 {
        // Approximate diffuse irradiance by sampling multiple directions
        let up = vec3<f32>(0.0, 1.0, 0.0);
        let right = normalize(cross(up, dir));
        let forward = cross(dir, right);

        // Sample in a cone around the direction
        let offset = blur * 0.5;
        let s1 = sample_equirectangular(normalize(dir + right * offset));
        let s2 = sample_equirectangular(normalize(dir - right * offset));
        let s3 = sample_equirectangular(normalize(dir + forward * offset));
        let s4 = sample_equirectangular(normalize(dir - forward * offset));

        let blurred = (sharp + s1 + s2 + s3 + s4) / 5.0;
        return mix(sharp, blurred, blur);
    }

    return sharp;
}

// ============================================================================
// Shadow Mapping
// ============================================================================

// Calculate shadow factor with PCF (Percentage Closer Filtering)
fn calculate_shadow(shadow_coord: vec4<f32>, normal: vec3<f32>, light_dir: vec3<f32>) -> f32 {
    // Check if shadows are enabled (w component of config)
    if shadow.config.w < 0.5 {
        return 1.0; // No shadow
    }

    // Perspective divide
    let proj_coords = shadow_coord.xyz / shadow_coord.w;

    // Transform to [0, 1] range (NDC is [-1, 1], depth is [0, 1])
    let shadow_uv = proj_coords.xy * 0.5 + 0.5;
    // Flip Y for texture coordinates
    let uv = vec2<f32>(shadow_uv.x, 1.0 - shadow_uv.y);

    // Out of shadow map bounds check
    if uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || proj_coords.z > 1.0 {
        return 1.0; // Outside shadow map, no shadow
    }

    // Get bias values
    let bias = shadow.config.x;
    let normal_bias = shadow.config.y;
    let softness = shadow.config.z;

    // Calculate bias based on surface angle
    let n_dot_l = max(dot(normal, -light_dir), 0.0);
    let slope_bias = max(bias * (1.0 - n_dot_l), bias * 0.1);
    let current_depth = proj_coords.z - slope_bias - normal_bias * (1.0 - n_dot_l);

    // PCF (3x3 kernel for soft shadows)
    let texel_size = 1.0 / 2048.0; // Shadow map resolution
    var shadow_sum = 0.0;
    let pcf_range = 1; // -1 to 1 = 3x3

    for (var x = -pcf_range; x <= pcf_range; x++) {
        for (var y = -pcf_range; y <= pcf_range; y++) {
            let offset = vec2<f32>(f32(x), f32(y)) * texel_size * softness;
            shadow_sum += textureSampleCompare(
                shadow_map,
                shadow_sampler,
                uv + offset,
                current_depth
            );
        }
    }

    // Average the samples (9 samples for 3x3)
    let shadow_factor = shadow_sum / 9.0;

    // Don't make shadows completely black - allow some ambient light through
    // This creates more realistic shadows that aren't pitch black
    let min_shadow = 0.3;
    return mix(min_shadow, 1.0, shadow_factor);
}

// ============================================================================
// Fragment Shader
// ============================================================================

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Get texture flags
    let tex_flags = material.texture_flags;

    // Sample textures or use uniform values
    var base_color: vec3<f32>;
    var alpha: f32;
    if (tex_flags & TEX_ALBEDO) != 0u {
        let tex_color = textureSample(albedo_texture, albedo_sampler, in.uv);
        base_color = tex_color.rgb * material.base_color.rgb;
        // Force alpha=1.0 for textured materials (metal/PBR textures are opaque)
        alpha = material.base_color.a;
    } else {
        base_color = material.base_color.rgb;
        alpha = material.base_color.a;
    }

    var metallic: f32;
    var roughness: f32;
    if (tex_flags & TEX_METALLIC_ROUGHNESS) != 0u {
        // glTF standard: G=roughness, B=metallic
        // For separate roughness textures, R channel contains roughness
        let mr = textureSample(metallic_roughness_texture, metallic_roughness_sampler, in.uv);
        roughness = mr.r * material.roughness;  // Use R for grayscale roughness textures
        metallic = material.metallic;  // Keep uniform metallic (no metalness texture loaded)
    } else {
        metallic = material.metallic;
        roughness = material.roughness;
    }
    roughness = max(roughness, MIN_ROUGHNESS);

    let emissive = vec3<f32>(material.emissive_r, material.emissive_g, material.emissive_b);

    // Normal mapping (simplified - world space perturbation)
    var n: vec3<f32>;
    if (tex_flags & TEX_NORMAL) != 0u {
        let normal_sample = textureSample(normal_texture, normal_sampler, in.uv).rgb;
        let tangent_normal = normalize(normal_sample * 2.0 - 1.0);
        // Simplified normal mapping without tangent vectors
        // Just perturb the normal slightly based on the normal map
        n = normalize(in.world_normal + tangent_normal.x * 0.5 + tangent_normal.y * 0.5);
    } else {
        n = normalize(in.world_normal);
    }
    let v = normalize(camera.camera_pos - in.world_position);
    let l = normalize(-light.direction);
    let h = normalize(v + l);
    let r = reflect(-v, n);  // Reflection direction

    // Dot products
    let n_dot_v = max(dot(n, v), 0.001);
    let n_dot_l = max(dot(n, l), 0.0);
    let n_dot_h = max(dot(n, h), 0.0);
    let h_dot_v = max(dot(h, v), 0.0);

    // F0 - reflectance at normal incidence
    // Dielectric: 0.04, Metal: base_color
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

    // ========== SHADOW CALCULATION ==========
    let shadow_factor = calculate_shadow(in.shadow_coord, n, light.direction);

    // ========== DIRECT LIGHTING ==========

    // Key light (main directional) - affected by shadows
    let radiance = light.color * light.intensity;
    var direct = (diffuse + specular) * radiance * n_dot_l * shadow_factor;

    // Fill light (opposite side, cooler) - not shadowed (simulates bounce light)
    // Reduced intensity for better contrast
    let fill_dir = normalize(vec3<f32>(-0.6, 0.4, 0.7));
    let fill_n_dot_l = max(dot(n, fill_dir), 0.0);
    let fill_radiance = vec3<f32>(0.4, 0.5, 0.7) * light.intensity * 0.15;
    direct += (diffuse + specular * 0.2) * fill_radiance * fill_n_dot_l;

    // Back light (rim separation) - subtle
    let back_dir = normalize(vec3<f32>(0.0, 0.3, -1.0));
    let back_n_dot_l = max(dot(n, back_dir), 0.0);
    direct += diffuse * vec3<f32>(0.3, 0.35, 0.5) * light.intensity * back_n_dot_l * 0.1;

    // ========== ENVIRONMENT / IBL ==========

    // Sample environment for reflection
    let env = sample_environment_lod(r, roughness);

    // Use Fresnel with roughness for environment reflection
    let fresnel_env = fresnel_schlick_roughness(n_dot_v, f0, roughness);

    // For metals, the base_color IS the reflection color
    // Boost the metal tint so gold looks gold, copper looks copper, etc.
    let metal_tint = mix(vec3<f32>(1.0), base_color, metallic);

    // Metals get strong environment reflection, dielectrics get less
    // Reduced from 1.0 to 0.7 for metals to avoid over-brightness
    let env_strength = mix(0.25, 0.7, metallic);
    let env_reflection = env * fresnel_env * metal_tint * env_strength;

    // Diffuse IBL (hemisphere sampling approximation)
    // Reduced irradiance values for better contrast
    let hemisphere_blend = n.y * 0.5 + 0.5;
    let sky_irradiance = vec3<f32>(0.4, 0.5, 0.7);
    let ground_irradiance = vec3<f32>(0.15, 0.12, 0.1);
    let diffuse_ibl = mix(ground_irradiance, sky_irradiance, hemisphere_blend);

    // Ambient diffuse (only for non-metals)
    let ambient_diffuse = kd * base_color * diffuse_ibl * light.ambient_intensity;

    // Combine ambient: diffuse + specular reflection
    let ambient = ambient_diffuse + env_reflection;

    // ========== RIM LIGHT ==========
    // Subtle rim for edge definition, not too bright
    let rim = pow(1.0 - n_dot_v, 4.0) * 0.25;
    let rim_color = mix(vec3<f32>(0.4, 0.5, 0.7), light.color, 0.3);
    let rim_light = rim_color * rim;

    // ========== FINAL COMBINATION ==========

    var color = direct + ambient + rim_light + emissive;

    // ACES tone mapping (filmic, preserves bright specular)
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d2 = 0.59;
    let e = 0.14;
    color = clamp((color * (a * color + b)) / (color * (c * color + d2) + e), vec3<f32>(0.0), vec3<f32>(1.0));

    // Note: sRGB surface (Bgra8UnormSrgb) handles gamma correction automatically
    // Do NOT apply manual pow(1/2.2) here - it would double-correct

    return vec4<f32>(color, alpha);
}
