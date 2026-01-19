# Layer Compositor System

## Overview

The void_render layer compositor provides complete layer-based composition with shader failure isolation. Each application renders to its own isolated layers, and the kernel composites them into the final frame.

## Core Principle: Shaders Never Compose With Shaders

```
┌──────────────────────────────────────────────────────────────┐
│  WRONG:  Shader A → Shader B → Shader C → Final             │
│          (Cascade failure. Undefined composition.)           │
│                                                              │
│  RIGHT:  Shader A → Layer A ─┐                               │
│          Shader B → Layer B ──┼──► Compositor ──► Final     │
│          Shader C → Layer C ─┘                               │
│          (Isolated. Failed shader = black layer.)            │
└──────────────────────────────────────────────────────────────┘
```

## Architecture

### Module Structure

```
void_render/
├── layer.rs          # Layer types, health, viewport, z-ordering
├── blend.rs          # Blend mode shaders and GPU configurations
├── compositor.rs     # Compositor with layer composition
├── graph.rs          # Render graph infrastructure
└── resource.rs       # GPU resource descriptors
```

### Key Types

#### Layer

```rust
pub struct Layer {
    id: LayerId,
    config: LayerConfig,
    health: LayerHealth,
    color_target: Option<GraphHandle>,
    depth_target: Option<GraphHandle>,
    last_rendered_frame: u64,
    dirty: bool,
}
```

#### LayerHealth

```rust
pub enum LayerHealth {
    Healthy,
    ShaderFailed { error: String },       // Renders black, doesn't crash
    ResourceFailed { error: String },     // Skipped entirely
    Skipped { reason: String },           // Budget/perf skipped
}
```

#### BlendMode

```rust
pub enum BlendMode {
    Normal,      // Standard alpha blending
    Additive,    // Light/glow effects
    Multiply,    // Shadows/darkening
    Screen,      // Brightening
    Overlay,     // Contrast boost
    Darken,      // Min blend
    Lighten,     // Max blend
    ColorDodge,  // Brightening with color dodge
    ColorBurn,   // Darkening with color burn
    HardLight,   // Strong contrast
    SoftLight,   // Soft contrast
    Difference,  // Absolute difference
    Exclusion,   // Inverted difference
    Replace,     // No blending
}
```

Each blend mode has:
- **WGSL shader code** for GPU execution
- **GPU blend config** (for hardware-accelerated modes)
- **Needs destination** flag (for optimization)

#### LayerViewport

```rust
pub struct LayerViewport {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}
```

Supports both normalized (0-1) and pixel coordinates. Automatically converts based on output size.

## Composition Pipeline

### Frame Flow

```
1. begin_frame()
   ├─ Increment frame index
   └─ Clear render graph

2. build_graph()
   ├─ Sort layers by z_order (ascending)
   ├─ Filter visible layers
   ├─ Create render targets for each layer
   └─ Compile render graph

3. composite_layers()
   ├─ For each visible layer (sorted):
   │  ├─ If Healthy: composite_layer_normal()
   │  ├─ If ShaderFailed: composite_layer_black()
   │  ├─ If ResourceFailed: skip
   │  └─ If Skipped: skip
   └─ Apply blend modes and viewports

4. execute(backend_data)
   └─ Execute compiled render graph

5. end_frame()
   └─ Mark layers as rendered
```

### Shader Failure Handling

When a shader fails during layer rendering:

1. **Detection**: Backend detects shader compile/execution error
2. **Isolation**: Layer health set to `ShaderFailed`
3. **Fallback**: Compositor renders black rectangle for that layer
4. **Continue**: Other layers render normally
5. **Recovery**: `attempt_layer_recovery()` can try to fix

**Critical**: The frame NEVER crashes. Failed shader = black layer.

### Blend Mode Application

Simple modes (Normal, Additive, Multiply, Replace) use **hardware blending**:

```rust
// GPU blend config for Normal mode
BlendConfig {
    src_color_factor: BlendFactor::SrcAlpha,
    dst_color_factor: BlendFactor::OneMinusSrcAlpha,
    color_operation: BlendOperation::Add,
    ...
}
```

Complex modes (Overlay, SoftLight, etc.) use **shader-based blending**:

```rust
// Generated WGSL shader
let blended = mix(
    2.0 * src.rgb * dst.rgb,
    vec3<f32>(1.0) - 2.0 * (vec3<f32>(1.0) - src.rgb) * (vec3<f32>(1.0) - dst.rgb),
    step(0.5, dst.rgb)
);
```

## Usage Examples

### Basic Layer Creation

```rust
use void_render::prelude::*;

let mut compositor = Compositor::new();
compositor.set_output_size(1920, 1080);

// Create background layer
let mut bg_config = LayerConfig::default();
bg_config.name = "background".into();
bg_config.z_order = 0;
bg_config.clear_color = Some([0.1, 0.1, 0.1, 1.0]);

let bg_layer = compositor.create_layer("background", bg_config);

// Create main content layer
let mut main_config = LayerConfig::default();
main_config.name = "main".into();
main_config.z_order = 10;
main_config.blend_mode = BlendMode::Normal;
main_config.opacity = 1.0;

let main_layer = compositor.create_layer("main", main_config);

// Create UI overlay
let mut ui_config = LayerConfig::default();
ui_config.name = "ui".into();
ui_config.z_order = 100;
ui_config.blend_mode = BlendMode::Normal;
ui_config.opacity = 0.9;

let ui_layer = compositor.create_layer("ui", ui_config);
```

### Windowed Layer

```rust
// Create a layer that renders to a specific viewport
let mut windowed_config = LayerConfig::default();
windowed_config.name = "window".into();
windowed_config.z_order = 50;
windowed_config.viewport = Some(LayerViewport::from_pixels(
    100.0, 100.0,  // x, y position
    800.0, 600.0,  // width, height
));

let windowed_layer = compositor.create_layer("window", windowed_config);
```

### Handling Shader Failure

```rust
// Simulate a shader failure
if let Some(layer) = compositor.layer_mut(layer_id) {
    layer.set_health(LayerHealth::ShaderFailed {
        error: "Vertex shader compilation failed".into(),
    });
}

// Compositor will render this layer as black instead of crashing
compositor.composite_layers()?;

// Later, attempt recovery
compositor.attempt_layer_recovery();
```

### Custom Blend Modes

```rust
// Use additive blending for glow effects
let mut glow_config = LayerConfig::default();
glow_config.name = "glow".into();
glow_config.z_order = 20;
glow_config.blend_mode = BlendMode::Additive;
glow_config.opacity = 0.5;

let glow_layer = compositor.create_layer("glow", glow_config);

// Use multiply for shadows
let mut shadow_config = LayerConfig::default();
shadow_config.name = "shadow".into();
shadow_config.z_order = 5;
shadow_config.blend_mode = BlendMode::Multiply;

let shadow_layer = compositor.create_layer("shadow", shadow_config);
```

### Frame Loop

```rust
loop {
    // Begin frame
    compositor.begin_frame();

    // Build render graph
    compositor.build_graph();

    // Apps render to their layers here
    // ...

    // Composite all layers
    compositor.composite_layers()?;

    // Execute GPU commands
    compositor.execute(&mut backend_data);

    // End frame
    compositor.end_frame();
}
```

## Performance Optimizations

### Dirty Tracking

Layers track whether they've changed since last render:

```rust
// Check which layers need re-rendering
let dirty_layers = compositor.collect_dirty_layers();

// Only re-render dirty layers
for layer_id in dirty_layers {
    if let Some(layer) = compositor.layer_mut(layer_id) {
        // Render this layer
        render_layer(layer);
    }
}
```

### Render Scale

Reduce memory/bandwidth for effects layers:

```rust
let mut effect_config = LayerConfig::default();
effect_config.name = "blur".into();
effect_config.z_order = 15;
effect_config.render_scale = 0.5; // Half resolution
```

### Viewport Culling

Skip layers outside the visible area:

```rust
// Layer completely outside viewport will be skipped
let mut offscreen_config = LayerConfig::default();
offscreen_config.viewport = Some(LayerViewport::from_pixels(
    -1000.0, -1000.0,  // Off screen
    100.0, 100.0,
));
offscreen_config.visible = false;
```

## Shader Generation

The `blend` module generates WGSL shaders for composition:

```rust
use void_render::blend::*;

// Generate composite shader for a blend mode
let shader_source = generate_composite_shader(
    BlendMode::Overlay,
    true,  // has_opacity
);

// Generate simple blit shader
let blit_shader = generate_blit_shader();

// Generate solid color shader (for failed shaders)
let solid_color_shader = generate_solid_color_shader();
```

## Integration with Kernel

The compositor integrates with `void_kernel` for app isolation:

```rust
// In void_kernel:
use void_render::{Compositor, LayerConfig, BlendMode};

// Each app gets its own layer
for app in apps {
    let mut config = LayerConfig::default();
    config.name = app.name().into();
    config.z_order = app.z_order();
    config.blend_mode = BlendMode::Normal;

    let layer_id = compositor.create_layer(&app.name(), config);
    app.set_layer_id(layer_id);
}

// Render all apps to their layers
for app in apps {
    match app.render() {
        Ok(_) => {
            // Layer remains healthy
        }
        Err(e) => {
            // Mark layer as failed
            if let Some(layer) = compositor.layer_mut(app.layer_id()) {
                layer.set_health(LayerHealth::ShaderFailed {
                    error: e.to_string(),
                });
            }
        }
    }
}

// Composite all layers (failed shaders render as black)
compositor.composite_layers()?;
```

## Testing

Run the compositor tests:

```bash
cargo test -p void_render --lib compositor
```

Run layer tests:

```bash
cargo test -p void_render --lib layer
```

Run blend mode tests:

```bash
cargo test -p void_render --lib blend
```

## Implementation Checklist

- [x] LayerHealth tracking (Healthy, ShaderFailed, ResourceFailed, Skipped)
- [x] Layer z-ordering by priority
- [x] Blend modes (14 modes supported)
- [x] Viewport management (fullscreen and windowed)
- [x] Shader failure isolation (black layer fallback)
- [x] Dirty tracking for performance
- [x] Render scale support
- [x] WGSL shader generation
- [x] GPU blend configuration
- [x] Composition pipeline
- [x] Layer recovery mechanism
- [ ] Actual GPU command generation (TODO: backend-specific)
- [ ] Layer caching for static content
- [ ] Budget enforcement (max layers, max pixels)
- [ ] Dirty region tracking (sub-layer optimization)

## Future Enhancements

### Layer Caching

```rust
pub struct LayerCache {
    cached: HashMap<LayerId, CachedLayer>,
    dirty_layers: HashSet<LayerId>,
}

impl LayerCache {
    pub fn can_use_cache(&self, layer_id: LayerId, current_frame: u64, max_age: u64) -> bool {
        // Check if layer output can be reused
    }
}
```

### Budget Enforcement

```rust
pub struct LayerBudget {
    max_layers: usize,
    max_combined_pixels: u64,
    max_render_time_us: u64,
}

impl Compositor {
    pub fn collect_within_budget(&mut self, budget: &LayerBudget) -> Vec<LayerId> {
        // Skip low-priority layers if over budget
    }
}
```

### Dirty Regions

```rust
pub struct DirtyRegion {
    x: u32,
    y: u32,
    width: u32,
    height: u32,
}

impl Layer {
    pub fn dirty_region(&self) -> Option<DirtyRegion> {
        // Only re-render changed regions
    }
}
```

## References

- [Architecture Documentation](../../docs/architecture/03-LAYER-COMPOSITOR.md)
- [Vision Document](../../docs/VISION.md)
- [void_kernel Layer Manager](../void_kernel/src/layer.rs)
