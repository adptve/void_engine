//! Shader variant demonstration
//!
//! Shows how to create shader permutations with different feature flags.

use void_shader::{
    ShaderCompiler, ShaderVariantCollection, VariantBuilder, ShaderVariant, ShaderDefine,
    CompileTarget,
};

fn main() {
    println!("Shader Variant Demo");
    println!("===================\n");

    let compiler = ShaderCompiler::new();

    // Base shader source (without defines)
    let base_shader = r#"
struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    #ifdef USE_TEXTURE
    @location(2) uv: vec2<f32>,
    #endif
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_normal: vec3<f32>,
    #ifdef USE_TEXTURE
    @location(1) uv: vec2<f32>,
    #endif
}

#ifdef USE_LIGHTING
@group(0) @binding(0)
var<uniform> light_dir: vec3<f32>;
#endif

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.clip_position = vec4<f32>(in.position, 1.0);
    out.world_normal = in.normal;

    #ifdef USE_TEXTURE
    out.uv = in.uv;
    #endif

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var color = vec3<f32>(1.0, 1.0, 1.0);

    #ifdef USE_LIGHTING
    let ndotl = max(dot(in.world_normal, light_dir), 0.0);
    color = color * ndotl;
    #endif

    return vec4<f32>(color, 1.0);
}
"#;

    // Create variant collection
    let mut collection = ShaderVariantCollection::new(base_shader.to_string());

    // Build variants using the builder
    println!("Building shader variants...\n");

    let variants = VariantBuilder::new("material")
        .with_feature("USE_TEXTURE")
        .with_feature("USE_LIGHTING")
        .build();

    println!("Generated {} variants:", variants.len());
    for variant in &variants {
        println!("  - {}", variant.name);
        collection.add_variant(variant.clone());
    }

    // Compile all variants
    println!("\nCompiling variants for multiple backends...");
    let targets = vec![
        CompileTarget::SpirV,
        CompileTarget::Wgsl,
        CompileTarget::GlslEs300,
    ];

    match collection.compile_all(&compiler, &targets) {
        Ok(()) => {
            println!("✓ All variants compiled successfully\n");

            // Show compilation stats
            println!("Compilation Statistics:");
            println!("  Total shaders compiled: {}", collection.compiled_count());
            println!("  Variants: {}", variants.len());
            println!("  Targets per variant: {}", targets.len());
            println!("  Total combinations: {}", variants.len() * targets.len());

            // Show some compiled output
            println!("\nSample Variant Output:");
            for variant_name in collection.variant_names() {
                println!("\n{} (SPIR-V):", variant_name);
                if let Some(compiled) = collection.get_variant(&variant_name, CompileTarget::SpirV)
                {
                    if let Some(spirv) = compiled.as_spirv() {
                        println!("  SPIR-V size: {} bytes", spirv.len() * 4);
                    }
                }
            }
        }
        Err(e) => {
            eprintln!("❌ Compilation failed: {:?}", e);
        }
    }

    // Manual variant creation
    println!("\n\nManual Variant Creation:");
    let custom_variant = ShaderVariant::new("custom_high_quality")
        .with_define(ShaderDefine::new("USE_TEXTURE"))
        .with_define(ShaderDefine::new("USE_LIGHTING"))
        .with_define(ShaderDefine::with_value("MAX_LIGHTS", "8"))
        .with_define(ShaderDefine::with_value("SHADOW_QUALITY", "HIGH"));

    println!("Preprocessor header for '{}':", custom_variant.name);
    println!("{}", custom_variant.generate_header());
}
