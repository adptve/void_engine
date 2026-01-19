//! Integration tests for shader pipeline
//!
//! Tests the complete hot-reload workflow including:
//! - Shader compilation
//! - Version tracking
//! - Rollback on failure
//! - Variant compilation

use void_shader::{
    ShaderPipeline, ShaderPipelineConfig, CompileTarget, ShaderVariantCollection, VariantBuilder,
};
use std::path::PathBuf;

#[test]
fn test_shader_compilation() {
    let config = ShaderPipelineConfig {
        shader_base_path: PathBuf::from("tests/shaders"),
        validate: true,
        default_targets: vec![CompileTarget::SpirV, CompileTarget::Wgsl],
        max_cached_shaders: 10,
        hot_reload: false,
    };

    let mut pipeline = ShaderPipeline::new(config);

    let shader_source = r#"
        @vertex
        fn vs_main(@location(0) pos: vec3<f32>) -> @builtin(position) vec4<f32> {
            return vec4<f32>(pos, 1.0);
        }

        @fragment
        fn fs_main() -> @location(0) vec4<f32> {
            return vec4<f32>(1.0, 0.0, 0.0, 1.0);
        }
    "#;

    let shader_id = pipeline
        .compile_shader("test", shader_source)
        .expect("Should compile valid shader");

    let entry = pipeline.get_shader(shader_id).expect("Should get shader");

    assert_eq!(entry.name, "test");
    assert_eq!(entry.version.raw(), 1);
    assert!(entry.has_target(CompileTarget::SpirV));
    assert!(entry.has_target(CompileTarget::Wgsl));
}

#[test]
fn test_shader_reload() {
    let mut pipeline = ShaderPipeline::default();

    let shader_source = r#"
        @vertex
        fn vs_main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0);
        }
    "#;

    let shader_id = pipeline
        .compile_shader("test", shader_source)
        .expect("Should compile");

    // Verify initial version
    let entry = pipeline.get_shader(shader_id).unwrap();
    assert_eq!(entry.version.raw(), 1);

    // Reload shader
    pipeline.reload_shader(shader_id).expect("Should reload");

    // Check version incremented
    let entry = pipeline.get_shader(shader_id).unwrap();
    assert_eq!(entry.version.raw(), 2);
}

#[test]
fn test_shader_rollback() {
    let mut pipeline = ShaderPipeline::default();

    let valid_shader = r#"
        @vertex
        fn vs_main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0);
        }
    "#;

    let shader_id = pipeline
        .compile_shader("test", valid_shader)
        .expect("Should compile");

    // Reload once to create history
    pipeline.reload_shader(shader_id).expect("Should reload");

    let entry = pipeline.get_shader(shader_id).unwrap();
    let version_before = entry.version;

    // Manually rollback
    pipeline
        .registry()
        .rollback(shader_id)
        .expect("Should rollback");

    let entry = pipeline.get_shader(shader_id).unwrap();
    assert!(entry.version < version_before);
}

#[test]
fn test_version_tracking() {
    let mut pipeline = ShaderPipeline::default();

    let shader = r#"
        @vertex
        fn main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0);
        }
    "#;

    let id = pipeline.compile_shader("versioned", shader).unwrap();

    // Initial version
    let v1 = pipeline.registry().version(id).unwrap();
    assert_eq!(v1.raw(), 1);

    // Reload
    pipeline.reload_shader(id).unwrap();
    let v2 = pipeline.registry().version(id).unwrap();
    assert_eq!(v2.raw(), 2);

    // Reload again
    pipeline.reload_shader(id).unwrap();
    let v3 = pipeline.registry().version(id).unwrap();
    assert_eq!(v3.raw(), 3);

    // Check history
    assert!(pipeline.registry().history_depth(id) >= 2);
}

#[test]
fn test_shader_variants() {
    let compiler = void_shader::ShaderCompiler::new();

    let base_shader = r#"
        @vertex
        fn vs_main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0);
        }

        @fragment
        fn fs_main() -> @location(0) vec4<f32> {
            return vec4<f32>(1.0);
        }
    "#;

    let mut collection = ShaderVariantCollection::new(base_shader.to_string());

    let variants = VariantBuilder::new("test")
        .with_feature("FEATURE_A")
        .with_feature("FEATURE_B")
        .build();

    assert_eq!(variants.len(), 4); // base, a, b, a+b

    for variant in variants {
        collection.add_variant(variant);
    }

    let targets = vec![CompileTarget::SpirV];
    collection
        .compile_all(&compiler, &targets)
        .expect("Should compile all variants");

    assert_eq!(collection.compiled_count(), 4);
}

#[test]
fn test_listener_notifications() {
    use std::sync::{Arc, Mutex};

    let mut pipeline = ShaderPipeline::default();

    let notifications = Arc::new(Mutex::new(Vec::new()));
    let notifications_clone = notifications.clone();

    pipeline.registry().add_listener(move |id, version| {
        notifications_clone.lock().unwrap().push((id, version));
    });

    let shader = r#"
        @vertex
        fn main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0);
        }
    "#;

    let id = pipeline.compile_shader("test", shader).unwrap();

    // Should have received notification for initial compilation
    {
        let notifs = notifications.lock().unwrap();
        assert_eq!(notifs.len(), 1);
        assert_eq!(notifs[0].0, id);
    }

    // Reload should trigger another notification
    pipeline.reload_shader(id).unwrap();

    {
        let notifs = notifications.lock().unwrap();
        assert_eq!(notifs.len(), 2);
    }
}

#[test]
fn test_registry_stats() {
    let mut pipeline = ShaderPipeline::default();

    let shader = r#"
        @vertex
        fn main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0);
        }
    "#;

    pipeline.compile_shader("test1", shader).unwrap();
    pipeline.compile_shader("test2", shader).unwrap();

    let stats = pipeline.registry().stats();
    assert_eq!(stats.total_shaders, 2);
    assert!(stats.total_compilations >= 2);
}

#[test]
fn test_invalid_shader_compilation() {
    let mut pipeline = ShaderPipeline::default();

    let invalid_shader = "this is not valid WGSL";

    let result = pipeline.compile_shader("invalid", invalid_shader);
    assert!(result.is_err());
}

#[test]
fn test_backend_compilation() {
    let compiler = void_shader::ShaderCompiler::new();

    let shader = r#"
        @vertex
        fn main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0);
        }
    "#;

    let module = compiler.parse_wgsl(shader).expect("Should parse");

    // Test SPIR-V
    let spirv = compiler
        .compile(&module, CompileTarget::SpirV)
        .expect("Should compile to SPIR-V");
    assert!(spirv.as_spirv().is_some());

    // Test WGSL
    let wgsl = compiler
        .compile(&module, CompileTarget::Wgsl)
        .expect("Should compile to WGSL");
    assert!(wgsl.as_wgsl().is_some());

    // Test GLSL
    let glsl = compiler
        .compile(&module, CompileTarget::GlslEs300)
        .expect("Should compile to GLSL");
    assert!(glsl.as_glsl().is_some());
}
