# void_shader

Shader compilation and hot-reload pipeline for Void Engine.

## Features

- **Multi-Backend Compilation**: SPIR-V (Vulkan), WGSL (WebGPU), GLSL (OpenGL)
- **Hot-Reload**: Automatic shader recompilation on file changes
- **Rollback**: Automatic rollback to previous version on compilation failure
- **Version Tracking**: Track shader versions and maintain history
- **Shader Variants**: Define-based permutations for feature flags
- **Reflection**: Extract binding information from shaders
- **Validation**: Security and conformance validation

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  1. IMPORT          Raw shader source arrives               │
│  2. PARSE+REFLECT   Analyze with naga, extract bindings     │
│  3. NORMALIZE       Map to engine binding schema            │
│  4. COMPILE         Generate backend variants               │
│  5. VALIDATE        Security checks, capability gates       │
│  6. PUBLISH         Stage → Activate → Retain for rollback  │
└─────────────────────────────────────────────────────────────┘
```

## Quick Start

```rust
use void_shader::{ShaderPipeline, ShaderPipelineConfig, CompileTarget};
use std::path::PathBuf;

// Configure pipeline
let config = ShaderPipelineConfig {
    shader_base_path: PathBuf::from("shaders"),
    validate: true,
    default_targets: vec![
        CompileTarget::SpirV,
        CompileTarget::Wgsl,
        CompileTarget::GlslEs300,
    ],
    max_cached_shaders: 256,
    hot_reload: true,
};

let mut pipeline = ShaderPipeline::new(config);

// Load a shader
let shader_id = pipeline.load_shader("my_shader.wgsl")?;

// Get compiled shader
let entry = pipeline.get_shader(shader_id).unwrap();
println!("Shader '{}' version {}", entry.name, entry.version.raw());

// Access compiled SPIR-V
if let Some(compiled) = entry.get_compiled(CompileTarget::SpirV) {
    if let Some(spirv) = compiled.as_spirv() {
        println!("SPIR-V size: {} bytes", spirv.len() * 4);
    }
}
```

## Hot-Reload

Enable hot-reload with the `hot-reload` feature:

```toml
void_shader = { version = "0.1", features = ["hot-reload"] }
```

```rust
// Start watching for file changes
pipeline.start_watching()?;

// Poll for changes in your main loop
loop {
    let changes = pipeline.poll_changes();

    for (path, shader_id, result) in changes {
        match result {
            Ok(()) => println!("✓ Reloaded: {:?}", path),
            Err(e) => println!("❌ Failed, rolled back: {:?}", e),
        }
    }

    // ... render frame
}
```

## Shader Variants

Create permutations with different feature flags:

```rust
use void_shader::{VariantBuilder, ShaderVariantCollection};

let base_shader = r#"
    #ifdef USE_TEXTURE
    @location(1) uv: vec2<f32>,
    #endif

    #ifdef USE_LIGHTING
    // lighting code
    #endif
"#;

// Build all permutations
let variants = VariantBuilder::new("material")
    .with_feature("USE_TEXTURE")
    .with_feature("USE_LIGHTING")
    .build();

// Generates: material, material_use_texture, material_use_lighting, material_use_texture_use_lighting

let mut collection = ShaderVariantCollection::new(base_shader.to_string());
for variant in variants {
    collection.add_variant(variant);
}

collection.compile_all(&compiler, &targets)?;
```

## Version Tracking & Rollback

```rust
// Shader versions increment on reload
let id = pipeline.compile_shader("test", source)?;
assert_eq!(pipeline.registry().version(id).unwrap().raw(), 1);

pipeline.reload_shader(id)?;
assert_eq!(pipeline.registry().version(id).unwrap().raw(), 2);

// Rollback to previous version
pipeline.registry().rollback(id)?;
assert_eq!(pipeline.registry().version(id).unwrap().raw(), 1);

// Check history depth
let depth = pipeline.registry().history_depth(id);
println!("History depth: {}", depth);
```

## Listeners

Subscribe to shader updates:

```rust
pipeline.registry().add_listener(|shader_id, version| {
    println!("Shader {:?} updated to version {}", shader_id, version.raw());
});
```

## Registry Stats

```rust
let stats = pipeline.registry().stats();
println!("Shaders: {}", stats.total_shaders);
println!("Compilations: {}", stats.total_compilations);
println!("Source bytes: {}", stats.total_source_bytes);
println!("Compiled bytes: {}", stats.total_compiled_bytes);
```

## Shader Reflection

Extract binding information:

```rust
let entry = pipeline.get_shader(shader_id).unwrap();
let reflection = &entry.reflection;

// Inspect bind groups
for (group_id, layout) in &reflection.bind_groups {
    println!("Group {}:", group_id);
    for binding in &layout.bindings {
        println!("  Binding {}: {:?}", binding.binding, binding.binding_type);
    }
}

// Check vertex inputs
for input in &reflection.vertex_inputs {
    println!("Input @location({}): {:?}", input.location, input.format);
}
```

## Engine Binding Schema

Shaders should follow the engine's binding schema:

```wgsl
// Group 0: Global (camera, time, environment)
@group(0) @binding(0) var<uniform> camera: CameraUniform;
@group(0) @binding(1) var<uniform> time: TimeUniform;

// Group 1: Material (per-shader custom)
@group(1) @binding(0) var<uniform> material: MaterialUniform;
@group(1) @binding(1) var albedo_texture: texture_2d<f32>;

// Group 2: Object (per-instance)
@group(2) @binding(0) var<uniform> transform: mat4x4<f32>;

// Group 3: Custom (app-specific)
```

## Examples

Run examples:

```bash
# Hot-reload demo
cargo run --example hot_reload_demo --features hot-reload

# Variant compilation
cargo run --example variant_demo
```

## Testing

```bash
cargo test
cargo test --features hot-reload
```

## Dependencies

- **naga**: Shader parsing and cross-compilation
- **notify**: File watching for hot-reload (optional)
- **parking_lot**: High-performance synchronization
- **thiserror**: Error handling

## Safety & Validation

The shader pipeline enforces:

- **No frame-owning shaders**: Shaders never control the swapchain
- **Backend portability**: All shaders compile for all enabled backends
- **Atomic activation**: Shaders activate at frame boundaries
- **Mandatory rollback**: Previous version retained for recovery
- **Security validation**: Loop bounds, resource limits checked

## Integration with Void Kernel

Shaders integrate with the kernel via:

```rust
// Shader updates can be sent via IR patches
let patch = ShaderUpdatePatch {
    shader_id,
    new_version,
};

kernel.apply_patch(patch)?;
```

## License

MIT OR Apache-2.0
