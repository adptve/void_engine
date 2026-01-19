//! Hot-reload demonstration
//!
//! This example shows:
//! - Loading shaders from files
//! - Automatic hot-reload on file changes
//! - Rollback on compilation failure
//! - Version tracking
//! - Subscriber notifications

use void_shader::{ShaderPipeline, ShaderPipelineConfig, CompileTarget, ShaderId, ShaderVersion};
use std::path::PathBuf;
use std::time::Duration;

fn main() {
    // Initialize logging
    env_logger::init();

    println!("Shader Hot-Reload Demo");
    println!("======================\n");

    // Configure pipeline
    let config = ShaderPipelineConfig {
        shader_base_path: PathBuf::from("examples/shaders"),
        validate: true,
        default_targets: vec![
            CompileTarget::SpirV,
            CompileTarget::Wgsl,
            CompileTarget::GlslEs300,
        ],
        max_cached_shaders: 10,
        hot_reload: true,
    };

    let mut pipeline = ShaderPipeline::new(config);

    // Add a listener for shader version changes
    pipeline.registry().add_listener(|id, version| {
        println!("🔔 Shader {:?} updated to version {}", id, version.raw());
    });

    println!("✓ Pipeline configured with hot-reload enabled\n");

    // Create a test shader file
    create_test_shader();

    // Load shader
    println!("📂 Loading test shader...");
    let shader_id = match pipeline.load_shader("test.wgsl") {
        Ok(id) => {
            println!("✓ Shader loaded with ID {:?}\n", id);
            id
        }
        Err(e) => {
            eprintln!("❌ Failed to load shader: {:?}", e);
            return;
        }
    };

    // Start watching for changes
    #[cfg(feature = "hot-reload")]
    {
        if let Err(e) = pipeline.start_watching() {
            eprintln!("❌ Failed to start file watcher: {:?}", e);
            return;
        }
        println!("👁️  File watcher started\n");

        println!("Instructions:");
        println!("1. Edit examples/shaders/test.wgsl");
        println!("2. Watch for automatic reload");
        println!("3. Try introducing a syntax error to see rollback");
        println!("4. Press Ctrl+C to exit\n");

        // Poll for changes
        loop {
            let changes = pipeline.poll_changes();

            for (path, id, result) in changes {
                match result {
                    Ok(()) => {
                        println!("✓ Hot-reloaded: {:?}", path);

                        // Show version history
                        let depth = pipeline.registry().history_depth(id);
                        println!("  History depth: {}", depth);

                        if let Some(entry) = pipeline.get_shader(id) {
                            println!("  Current version: {}", entry.version.raw());
                            println!("  Compiled for {} targets", entry.compiled.len());
                        }
                    }
                    Err(e) => {
                        println!("❌ Hot-reload failed: {:?}", e);
                        println!("  Rolled back to previous version");
                    }
                }
            }

            std::thread::sleep(Duration::from_millis(100));
        }
    }

    #[cfg(not(feature = "hot-reload"))]
    {
        println!("⚠️  Hot-reload feature not enabled");
        println!("   Run with: cargo run --example hot_reload_demo --features hot-reload");
    }
}

/// Create a test shader file
fn create_test_shader() {
    use std::fs;

    let dir = PathBuf::from("examples/shaders");
    if !dir.exists() {
        fs::create_dir_all(&dir).expect("Failed to create shader directory");
    }

    let shader_path = dir.join("test.wgsl");
    let shader_source = r#"
@vertex
fn vs_main(@location(0) position: vec3<f32>) -> @builtin(position) vec4<f32> {
    return vec4<f32>(position, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
    return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
"#;

    if !shader_path.exists() {
        fs::write(&shader_path, shader_source).expect("Failed to write test shader");
        println!("📝 Created test shader at {:?}\n", shader_path);
    }
}
