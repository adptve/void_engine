# App Runtime Interface Specification

> **Purpose**: This document defines the complete runtime interface that apps expect from the Void Engine runtime. It is derived from exhaustive analysis of the three example applications: `nebula-genesis`, `portal-dimensions`, and `synthwave-dreamscape`.

---

## Executive Summary

The example apps expect a runtime that provides:
- **VoidScript interpreter** with lifecycle hooks and built-in functions
- **Patch-based state management** for entity/component manipulation
- **Multi-layer compositing** with blend modes and priorities
- **WGSL shader pipeline** with uniform binding support
- **Input handling** for mouse and keyboard
- **Built-in mesh primitives** for rendering

---

## 1. VoidScript Execution Environment

### 1.1 Lifecycle Hooks

Apps implement these functions that the runtime calls:

| Hook | Signature | When Called |
|------|-----------|-------------|
| `on_init()` | `fn on_init()` | Once when app starts |
| `on_update(dt)` | `fn on_update(dt: f32)` | Every frame with delta time in seconds |
| `on_focus()` | `fn on_focus()` | When app gains focus |
| `on_blur()` | `fn on_blur()` | When app loses focus |
| `on_shutdown()` | `fn on_shutdown()` | When app is being unloaded |

**Runtime Contract**:
```
on_init()           -> called once at startup
loop {
    on_update(dt)   -> called every frame (~60fps target)
}
on_shutdown()       -> called once at termination
```

### 1.2 Module System

Apps use imports to include other VoidScript files:

```voidscript
import "systems/particle_system.vs";
import "systems/camera_controller.vs";
```

**Runtime Requirements**:
- Resolve paths relative to app's script directory
- Support circular import detection
- Merge imported symbols into caller's scope

### 1.3 Built-in Constants

| Constant | Value | Usage |
|----------|-------|-------|
| `TAU` | 6.283185307179586 | Full circle radians |
| `PI` | 3.141592653589793 | Half circle radians |

*Note: Apps currently define these manually. Runtime should provide them.*

### 1.4 Built-in Functions

#### Core Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `print(msg)` | `fn print(msg: any)` | Output to console |
| `str(value)` | `fn str(value: any) -> string` | Convert to string |
| `len(array)` | `fn len(arr: array) -> u32` | Array/string length |

#### Math Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sin(x)` | `fn sin(x: f32) -> f32` | Sine |
| `cos(x)` | `fn cos(x: f32) -> f32` | Cosine |
| `tan(x)` | `fn tan(x: f32) -> f32` | Tangent |
| `acos(x)` | `fn acos(x: f32) -> f32` | Arc cosine |
| `atan2(y, x)` | `fn atan2(y: f32, x: f32) -> f32` | Two-argument arctangent |
| `sqrt(x)` | `fn sqrt(x: f32) -> f32` | Square root |
| `pow(base, exp)` | `fn pow(base: f32, exp: f32) -> f32` | Power |
| `exp(x)` | `fn exp(x: f32) -> f32` | e^x |
| `abs(x)` | `fn abs(x: f32) -> f32` | Absolute value |
| `floor(x)` | `fn floor(x: f32) -> f32` | Floor |
| `max(a, b)` | `fn max(a: f32, b: f32) -> f32` | Maximum |
| `min(a, b)` | `fn min(a: f32, b: f32) -> f32` | Minimum |
| `clamp(v, min, max)` | `fn clamp(v: f32, min: f32, max: f32) -> f32` | Clamp value |
| `fmod(a, b)` | `fn fmod(a: f32, b: f32) -> f32` | Float modulo |

#### Random Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `random(min, max)` | `fn random(min: f32, max: f32) -> f32` | Random float in range |

#### Runtime Query Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_namespace()` | `fn get_namespace() -> NamespaceId` | Get app's namespace |
| `get_fps()` | `fn get_fps() -> f32` | Current framerate |
| `get_viewport_aspect()` | `fn get_viewport_aspect() -> f32` | Width/height ratio |

### 1.5 Input System Functions

#### Mouse State

```voidscript
let mouse = get_mouse_state();
// Returns:
// {
//     x: f32,              // Screen X position
//     y: f32,              // Screen Y position
//     left_button: bool,   // Left button pressed
//     right_button: bool,  // Right button pressed
//     scroll_delta: f32    // Scroll wheel delta
// }
```

#### Keyboard State

```voidscript
let keyboard = get_keyboard_state();
// Returns:
// {
//     space: bool,
//     up_arrow: bool,
//     down_arrow: bool,
//     left_arrow: bool,
//     right_arrow: bool,
//     key_1: bool,
//     key_2: bool,
//     key_3: bool,
//     key_4: bool,
//     key_5: bool,
//     // ... other keys as needed
// }
```

### 1.6 Iteration Constructs

```voidscript
// Range-based for loop
for i in range(0, count) {
    // i iterates from 0 to count-1
}

// Collection iteration
for item in collection {
    // item is each element
}
```

---

## 2. Patch System (emit_patch)

The patch system is the **sole mechanism** for apps to modify runtime state. Apps emit declarative patches; the runtime applies them atomically.

### 2.1 Patch Function

```voidscript
emit_patch(patch: PatchObject)
```

### 2.2 Entity Patches

#### Create Entity

```voidscript
emit_patch({
    type: "entity",
    entity: { namespace: get_namespace(), local_id: <u64> },
    op: "create",
    archetype: "<ArchetypeName>",
    components: {
        "<ComponentName>": { /* component data */ },
        // ... more components
    }
});
```

#### Destroy Entity

```voidscript
emit_patch({
    type: "entity",
    entity: { namespace: get_namespace(), local_id: <u64> },
    op: "destroy"
});
```

### 2.3 Component Patches

#### Update Component (partial)

```voidscript
emit_patch({
    type: "component",
    entity: { namespace: get_namespace(), local_id: <u64> },
    component: "<ComponentName>",
    op: "update",
    fields: {
        field_name: new_value,
        // only specified fields are updated
    }
});
```

#### Set Component (full replacement)

```voidscript
emit_patch({
    type: "component",
    entity: { namespace: get_namespace(), local_id: <u64> },
    component: "<ComponentName>",
    op: "set",
    data: {
        // complete component data
    }
});
```

### 2.4 Entity Reference Format

All entity references use:
```voidscript
{
    namespace: NamespaceId,  // From get_namespace()
    local_id: u64            // App-chosen unique ID
}
```

---

## 3. Component Types Required

### 3.1 Transform

Universal spatial component.

```rust
struct Transform {
    position: [f32; 3],    // World position [x, y, z]
    rotation: [f32; 4],    // Quaternion [x, y, z, w]
    scale: [f32; 3],       // Scale factors [x, y, z]
}
```

**Used By**: All apps, every renderable entity.

### 3.2 Material

Shader and rendering configuration.

```rust
struct Material {
    shader: String,                    // Path to WGSL shader
    blend_mode: BlendMode,             // "additive" | "normal" | "replace" | "multiply"
    depth_write: Option<bool>,         // Write to depth buffer
    depth_test: Option<bool>,          // Test against depth buffer
    double_sided: Option<bool>,        // Render both faces
    color: Option<[f32; 4]>,           // Base color
    uniforms: HashMap<String, Value>,  // Shader uniforms
}

enum BlendMode {
    Replace,    // dst = src
    Normal,     // Standard alpha blending
    Additive,   // dst = dst + src
    Multiply,   // dst = dst * src
}
```

**Uniform Value Types**:
- `f32`
- `vec2<f32>` as `[f32; 2]`
- `vec3<f32>` as `[f32; 3]`
- `vec4<f32>` as `[f32; 4]`
- `mat4x4<f32>` as `[[f32; 4]; 4]`
- `u32`

### 3.3 Renderable

Mesh and layer assignment.

```rust
struct Renderable {
    mesh: String,             // Mesh identifier
    instance_count: Option<u32>,  // For instanced rendering
    layer: String,            // Layer name to render to
}
```

### 3.4 ParticleBuffer

GPU particle data for instanced rendering.

```rust
struct ParticleBuffer {
    positions: Vec<[f32; 3]>,      // World positions
    velocities: Option<Vec<[f32; 3]>>,  // Velocities (optional)
    colors: Vec<[f32; 4]>,         // RGBA colors
    sizes: Vec<f32>,               // Per-particle sizes
    count: u32,                    // Active particle count
}
```

**Used By**: `nebula-genesis`, `portal-dimensions`, `synthwave-dreamscape`

### 3.5 BloomEffect

Post-process bloom configuration.

```rust
struct BloomEffect {
    intensity: f32,        // Bloom strength
    threshold: f32,        // Brightness threshold
    blur_passes: Option<u32>,  // Number of blur iterations
    blur_scale: Option<f32>,   // Blur kernel scale
}
```

**Used By**: `nebula-genesis`, `synthwave-dreamscape`

### 3.6 UIText

Text overlay rendering.

```rust
struct UIText {
    text: String,          // Text content
    position: [f32; 2],    // Screen position [x, y]
    font_size: f32,        // Font size in pixels
    color: [f32; 4],       // RGBA color
    layer: String,         // Layer name
}
```

**Used By**: All apps for debug/UI text

### 3.7 Portal (portal-dimensions only)

Portal rendering configuration.

```rust
struct Portal {
    target_dimension: u32,         // Target dimension ID
    recursion_depth: u32,          // Max recursion levels
    distortion: f32,               // Edge distortion strength
    virtual_camera: Option<Camera>, // Computed virtual camera
    recursion_level: Option<u32>,  // Current recursion depth
    render_target: Option<String>, // Render target name
}
```

### 3.8 Environment

Global environment settings.

```rust
struct Environment {
    sky_color: [f32; 4],       // Sky/clear color
    ambient_color: [f32; 3],   // Ambient light color
}
```

**Used By**: `portal-dimensions`

### 3.9 DistortionEffect

Screen-space distortion.

```rust
struct DistortionEffect {
    strength: f32,      // Distortion amount
    frequency: f32,     // Wave frequency
    time: f32,          // Animation time
}
```

**Used By**: `portal-dimensions`

### 3.10 ChromaticAberration

Color channel separation effect.

```rust
struct ChromaticAberration {
    intensity: f32,     // Separation amount
}
```

**Used By**: `portal-dimensions`

### 3.11 CRTEffect

CRT monitor simulation.

```rust
struct CRTEffect {
    scanline_intensity: f32,  // Scanline darkness
    curvature: f32,           // Screen curve amount
    vignette: f32,            // Edge darkening
    time: f32,                // Animation time
}
```

**Used By**: `synthwave-dreamscape`

### 3.12 FlashEffect

Screen flash overlay.

```rust
struct FlashEffect {
    color: [f32; 4],    // Flash color
    intensity: f32,     // Flash brightness
    duration: f32,      // Flash duration in seconds
}
```

**Used By**: `portal-dimensions`

### 3.13 Animator

Automatic animation.

```rust
struct Animator {
    rotation_speed: [f32; 3],  // Rotation per second [x, y, z]
    bob_amplitude: f32,        // Vertical bob amount
    bob_speed: f32,            // Bob frequency
}
```

**Used By**: `portal-dimensions`

---

## 4. Archetype Catalog

Archetypes are templates defining which components an entity requires.

| Archetype | Required Components | Optional Components |
|-----------|--------------------|--------------------|
| `ParticleSystem` | Transform, ParticleBuffer, Material, Renderable | - |
| `StaticParticles` | Transform, ParticleBuffer, Material, Renderable | - |
| `PostProcess` | Material, Renderable | BloomEffect, CRTEffect, DistortionEffect |
| `Portal` | Transform, Portal, Material, Renderable | - |
| `StaticMesh` | Transform, Material, Renderable | Animator, Environment |
| `FullscreenQuad` | Transform, Material, Renderable | - |
| `SynthwaveSun` | Transform, Material, Renderable | - |
| `MountainLayer` | Transform, Material, Renderable | - |
| `InfiniteGrid` | Transform, Material, Renderable | - |

---

## 5. Layer System Requirements

### 5.1 Layer Types

| Type | Purpose | Blend Default |
|------|---------|--------------|
| `content` | World geometry, particles, objects | varies |
| `effect` | Post-processing passes | additive/normal |
| `overlay` | UI elements on top | normal |
| `portal` | Render-to-texture for portals | replace |

### 5.2 Layer Declarations (from manifests)

#### nebula-genesis
```toml
[[app.layers]]
name = "deep_space"    # type: content, priority: -100, blend: replace
name = "nebula_clouds" # type: content, priority: -50, blend: additive
name = "star_field"    # type: content, priority: 0, blend: additive
name = "galaxy_core"   # type: content, priority: 50, blend: additive
name = "bloom"         # type: effect, priority: 100, blend: additive
name = "ui"            # type: overlay, priority: 200, blend: normal
```

#### portal-dimensions
```toml
[[app.layers]]
name = "world"         # type: content, priority: 0, blend: replace
name = "portal_a"      # type: portal, priority: 10, blend: replace
name = "portal_b"      # type: portal, priority: 20, blend: replace
name = "distortion"    # type: effect, priority: 50, blend: normal
name = "chromatic"     # type: effect, priority: 60, blend: normal
name = "ui"            # type: overlay, priority: 100, blend: normal
```

#### synthwave-dreamscape
```toml
[[app.layers]]
name = "sky"           # type: content, priority: -100, blend: replace
name = "sun"           # type: content, priority: -50, blend: additive
name = "mountains"     # type: content, priority: 0, blend: normal
name = "grid"          # type: content, priority: 10, blend: additive
name = "particles"     # type: content, priority: 20, blend: additive
name = "glow"          # type: effect, priority: 100, blend: additive
name = "scanlines"     # type: effect, priority: 110, blend: multiply
name = "ui"            # type: overlay, priority: 200, blend: normal
```

### 5.3 Layer Compositing Order

1. Sort layers by priority (ascending)
2. Render content layers to their render targets
3. Apply effect layers as post-process passes
4. Composite overlay layers on top
5. Final output to screen

---

## 6. Mesh Primitives Required

| Mesh ID | Description | Vertex Data |
|---------|-------------|-------------|
| `quad` | Billboard quad for particles | 4 vertices, -1 to 1, with UVs |
| `point` | Point sprite | Single vertex |
| `fullscreen_quad` | Screen-space quad for post-process | Covers NDC -1 to 1 |
| `cube` | Unit cube | 36 vertices with normals |
| `plane` | Flat plane | Subdivided grid |
| `circle` | Disc/circle mesh | Radial vertices |
| `portal_frame` | Portal surface | Quad with border |
| `mountain_strip` | Triangle strip for mountains | Procedural height strip |

---

## 7. Shader Pipeline Requirements

### 7.1 Uniform Binding Convention

All shaders use bind group 0 for uniforms:

```wgsl
@group(0) @binding(0) var<uniform> uniforms: UniformStruct;
@group(0) @binding(1) var source_texture: texture_2d<f32>;
@group(0) @binding(2) var texture_sampler: sampler;
```

### 7.2 Common Uniform Types

#### Camera/Transform Uniforms
```wgsl
struct CameraUniforms {
    view_matrix: mat4x4<f32>,
    projection_matrix: mat4x4<f32>,
    camera_position: vec3<f32>,
    time: f32,
}
```

#### Post-Process Uniforms
```wgsl
struct PostProcessUniforms {
    // Effect-specific parameters
    intensity: f32,
    time: f32,
    resolution: vec2<f32>,
    // Textures bound separately
}
```

### 7.3 Vertex Input Conventions

#### Standard Mesh
```wgsl
struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
}
```

#### Instanced Particles
```wgsl
struct ParticleInput {
    @location(0) quad_pos: vec2<f32>,      // Per-vertex
    @location(1) instance_pos: vec3<f32>,  // Per-instance
    @location(2) instance_color: vec4<f32>,
    @location(3) instance_size: f32,
    @location(4) instance_id: u32,
}
```

#### Fullscreen (Generated)
```wgsl
@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    // Generate fullscreen triangle from vertex_index
}
```

### 7.4 Fragment Output Convention

```wgsl
@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // Return RGBA color
}
```

### 7.5 Shader Files Referenced by Apps

| App | Shaders Used |
|-----|-------------|
| nebula-genesis | `galaxy_particle.wgsl`, `bloom.wgsl`, `nebula_cloud.wgsl`, `static_star.wgsl` |
| portal-dimensions | `portal_surface.wgsl`, `chromatic.wgsl`, `grid_floor.wgsl`, `holographic.wgsl`, `distortion.wgsl` |
| synthwave-dreamscape | `neon_grid.wgsl`, `synthwave_sun.wgsl`, `crt_effect.wgsl`, `synthwave_sky.wgsl`, `mountain_silhouette.wgsl`, `bloom.wgsl` |

---

## 8. Platform Feature Requirements

From manifest `required_features`:

| Feature | Required By | Description |
|---------|-------------|-------------|
| `webgpu` | All apps | Core rendering API |
| `compute` | nebula-genesis | Compute shaders for physics |
| `render_targets` | portal-dimensions | Render-to-texture support |

---

## 9. Resource Limits (from manifests)

| Resource | nebula-genesis | portal-dimensions | synthwave-dreamscape |
|----------|---------------|------------------|---------------------|
| max_entities | 50,000 | 10,000 | 20,000 |
| max_memory | 256 MB | 256 MB | 256 MB |
| max_layers | 8 | 8 | 10 |
| max_cpu_ms | 16.0 | 16.0 | 16.0 |

---

## 10. Minimal Viable Runtime

To run at least ONE of these apps, the minimal runtime needs:

### For `synthwave-dreamscape` (Simplest)

**Required Capabilities**:

1. **VoidScript Interpreter**
   - Lifecycle hooks: `on_init()`, `on_update(dt)`, `on_shutdown()`
   - Math functions: sin, cos, sqrt, random, floor, min, max
   - `emit_patch()` function
   - `get_namespace()`, `get_keyboard_state()`
   - Object/array literals, for loops, if statements

2. **Patch Processor**
   - Entity create/destroy
   - Component set/update
   - Support for: Transform, Material, Renderable, ParticleBuffer, UIText, BloomEffect, CRTEffect

3. **Layer System**
   - 8 layers with priority sorting
   - Blend modes: replace, additive, normal, multiply
   - Content and effect layer types

4. **Renderer**
   - WGSL shader compilation
   - Uniform buffer updates
   - Meshes: fullscreen_quad, plane, circle
   - Instanced rendering for particles
   - Multi-pass rendering (content -> effects)

5. **Input**
   - Keyboard state query (arrow keys, number keys)

**NOT Required for synthwave-dreamscape**:
- Mouse input
- Portal/render-to-texture
- Compute shaders
- Module imports (could inline)

### Implementation Priority Order

```
Phase 1: Static Rendering
├── Parse manifest.toml
├── Execute on_init()
├── Process create entity patches
├── Render single frame
└── Display output

Phase 2: Animation Loop
├── Call on_update(dt) each frame
├── Process component update patches
├── Re-render with updated uniforms
└── Handle keyboard input

Phase 3: Multi-Layer Compositing
├── Create multiple render targets
├── Priority-based layer sorting
├── Blend mode implementation
└── Post-process effects chain

Phase 4: Particles
├── ParticleBuffer component
├── Instanced rendering
├── GPU buffer updates per frame
└── Large particle counts (10k+)
```

---

## 11. Data Flow Summary

```
┌─────────────────────────────────────────────────────────────────┐
│                        VoidScript App                           │
├─────────────────────────────────────────────────────────────────┤
│  on_init()                                                      │
│    └─► emit_patch({ type: "entity", op: "create", ... })       │
│                                                                 │
│  on_update(dt)                                                  │
│    ├─► get_keyboard_state() ─────────────────┐                 │
│    ├─► get_mouse_state() ────────────────────┤                 │
│    ├─► emit_patch({ type: "component", ... })│                 │
│    └─► emit_patch({ type: "component", ... })│                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                       Patch Processor                           │
├─────────────────────────────────────────────────────────────────┤
│  Validate patches against capabilities                          │
│  Queue patches for atomic application                           │
│  Apply to ECS world                                             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                          ECS World                              │
├─────────────────────────────────────────────────────────────────┤
│  Entities with Components:                                      │
│  ├── Entity(1): Transform, Material, Renderable                 │
│  ├── Entity(2): Transform, ParticleBuffer, Material, Renderable│
│  └── Entity(3): BloomEffect, Material, Renderable              │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Render Pipeline                            │
├─────────────────────────────────────────────────────────────────┤
│  1. Sort entities by layer priority                             │
│  2. For each layer:                                             │
│     ├── Set render target                                       │
│     ├── Upload uniforms from Material.uniforms                  │
│     ├── Bind shader from Material.shader                        │
│     ├── Draw mesh from Renderable.mesh                          │
│     └── Apply blend mode                                        │
│  3. Composite all layers to screen                              │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                         ┌─────────┐
                         │ Display │
                         └─────────┘
```

---

## 12. Cross-Reference: Scripts to Runtime Calls

### nebula-genesis/main.vs

| Line | Runtime Call | Purpose |
|------|--------------|---------|
| 54 | `create_orbit_camera({...})` | User-defined, calls runtime internally |
| 66 | `create_input_handler()` | User-defined |
| 69 | `generate_galaxy(settings)` | User-defined, emits patches |
| 72 | `create_background_stars(5000)` | User-defined, emits patches |
| 75 | `setup_bloom_pass(...)` | User-defined, emits patches |
| 129 | `range(0, config.particle_count)` | **Runtime: range iterator** |
| 135 | `random(0.0, 1.0)` | **Runtime: random** |
| 145 | `cos(spiral_angle)`, `sin(spiral_angle)` | **Runtime: trig functions** |
| 152 | `sqrt(...)`, `max(r, 0.1)` | **Runtime: math functions** |
| 205-235 | `emit_patch({...})` | **Runtime: patch system** |
| 207 | `get_namespace()` | **Runtime: namespace query** |
| 433 | `get_fps()` | **Runtime: performance query** |

### portal-dimensions/main.vs

| Line | Runtime Call | Purpose |
|------|--------------|---------|
| 161-187 | `emit_patch({...})` | **Runtime: entity creation** |
| 169 | `euler_to_quat(...)` | User-defined helper |
| 314-337 | Collision detection | Pure math, no runtime calls |
| 525 | `get_keyboard_state()` | **Runtime: input query** |

### synthwave-dreamscape/main.vs

| Line | Runtime Call | Purpose |
|------|--------------|---------|
| 129-154 | `emit_patch({...})` | **Runtime: entity creation** |
| 371 | `random(-20.0, 20.0)` | **Runtime: random** |
| 525 | `get_keyboard_state()` | **Runtime: input query** |
| 628 | `str(floor(speed * 100))` | **Runtime: string/math** |

---

## 13. Implementation Checklist

### VoidScript Interpreter
- [ ] Tokenizer and parser for VoidScript syntax
- [ ] Variable scoping (let, global)
- [ ] Object literals `{ key: value }`
- [ ] Array literals `[a, b, c]`
- [ ] Array access `arr[i]`
- [ ] Object access `obj.field`
- [ ] Function definitions `fn name(args) { body }`
- [ ] For loops `for i in range(...)` and `for item in collection`
- [ ] If/else statements
- [ ] Import system
- [ ] Built-in math functions
- [ ] Built-in `emit_patch()` function

### Patch System
- [ ] Patch validation
- [ ] Entity lifecycle (create, destroy)
- [ ] Component operations (set, update)
- [ ] Entity ID generation and tracking
- [ ] Namespace isolation

### Components
- [ ] Transform
- [ ] Material (with uniform support)
- [ ] Renderable
- [ ] ParticleBuffer
- [ ] BloomEffect
- [ ] CRTEffect
- [ ] UIText
- [ ] Environment
- [ ] Portal (advanced)
- [ ] DistortionEffect
- [ ] ChromaticAberration
- [ ] FlashEffect
- [ ] Animator

### Rendering
- [ ] WGSL shader compilation
- [ ] Uniform buffer management
- [ ] Instanced rendering
- [ ] Multi-layer compositing
- [ ] Blend mode support
- [ ] Post-process pipeline
- [ ] Render-to-texture (for portals)

### Input
- [ ] Keyboard state polling
- [ ] Mouse state polling
- [ ] Scroll wheel support

---

## References

- **Example Apps**: `examples/nebula-genesis/`, `examples/portal-dimensions/`, `examples/synthwave-dreamscape/`
- **Architecture**: `docs/architecture/apps.md`
- **Manifest Format**: See example `manifest.toml` files
