# Materials Format

Materials define surface appearance using physically-based rendering (PBR) with the metallic-roughness workflow.

## File Formats

Materials can be defined in two ways:

1. **Inline**: Within entity definitions in scene files
2. **Standalone**: Separate `.toml` material files for reuse

## Basic PBR Properties

```toml
[material]
# Base color (required for visible surfaces)
albedo = [1.0, 0.0, 0.0, 1.0]     # RGBA color
# OR texture
albedo = { texture = "path.png" }

# Surface properties
metallic = 0.0                     # 0.0 = dielectric, 1.0 = metal
roughness = 0.5                    # 0.0 = mirror, 1.0 = rough

# Optional texture maps
normal_map = "normal.png"          # Normal/bump map
ao_map = "ao.png"                  # Ambient occlusion
emissive = [0, 0, 0]               # Self-illumination RGB
```

## Color Values

Colors use 0.0-1.0 range (not 0-255):

```toml
# Pure red
albedo = [1.0, 0.0, 0.0, 1.0]

# 50% gray
albedo = [0.5, 0.5, 0.5, 1.0]

# Orange with 80% opacity
albedo = [1.0, 0.5, 0.0, 0.8]
```

## Metallic-Roughness Guidelines

| Material | Metallic | Roughness |
|----------|----------|-----------|
| Polished metal | 1.0 | 0.1 |
| Brushed metal | 1.0 | 0.4 |
| Plastic | 0.0 | 0.3 |
| Rubber | 0.0 | 0.9 |
| Wood | 0.0 | 0.7 |
| Marble | 0.0 | 0.2 |
| Skin | 0.0 | 0.5 |

## Texture-Based Properties

Any property can use a texture instead of a constant value:

```toml
[material]
# Color from texture
albedo = { texture = "assets/textures/brick_color.png" }

# Metallic from texture (grayscale: black=0, white=1)
metallic = { texture = "assets/textures/brick_metallic.png" }

# Roughness from texture
roughness = { texture = "assets/textures/brick_roughness.png" }
```

## Common Material Presets

### Chrome

```toml
[material]
albedo = [0.95, 0.95, 0.95, 1.0]
metallic = 1.0
roughness = 0.05
```

### Gold

```toml
[material]
albedo = [1.0, 0.766, 0.336, 1.0]
metallic = 1.0
roughness = 0.3
```

### Copper

```toml
[material]
albedo = [0.955, 0.637, 0.538, 1.0]
metallic = 1.0
roughness = 0.4
```

### Plastic (Red)

```toml
[material]
albedo = [0.8, 0.1, 0.1, 1.0]
metallic = 0.0
roughness = 0.3
```

### Concrete

```toml
[material]
albedo = [0.5, 0.5, 0.5, 1.0]
metallic = 0.0
roughness = 0.9
```

## Advanced Materials

### Transmission (Glass/Liquids)

For transparent materials with refraction:

```toml
[material]
albedo = [1.0, 1.0, 1.0, 1.0]
metallic = 0.0
roughness = 0.0

[material.transmission]
factor = 1.0                      # 0 = opaque, 1 = fully transmissive
ior = 1.5                         # Index of refraction
thickness = 0.1                   # Volume thickness (meters)
attenuation_color = [1, 1, 1]     # Color absorption
attenuation_distance = 1.0        # Distance for full absorption
```

#### Common IOR Values

| Material | IOR |
|----------|-----|
| Air | 1.0 |
| Water | 1.33 |
| Glass | 1.5 |
| Crystal | 2.0 |
| Diamond | 2.42 |

### Sheen (Fabric/Velvet)

Soft highlight for cloth-like materials:

```toml
[material]
albedo = [0.3, 0.1, 0.4, 1.0]     # Purple velvet
metallic = 0.0
roughness = 0.8

[material.sheen]
color = [1.0, 1.0, 1.0]           # Highlight color
roughness = 0.5                    # Sheen roughness
```

### Clearcoat (Car Paint/Lacquer)

Additional specular layer on top of base material:

```toml
[material]
albedo = [0.8, 0.1, 0.1, 1.0]     # Red car paint
metallic = 0.9
roughness = 0.4

[material.clearcoat]
intensity = 1.0                    # 0 = no coat, 1 = full coat
roughness = 0.1                    # Coat roughness
normal_map = "clearcoat_normal.png"  # Optional separate normal
```

### Anisotropy (Brushed Metal/Hair)

Directional roughness for brushed surfaces:

```toml
[material]
albedo = [0.9, 0.9, 0.9, 1.0]
metallic = 1.0
roughness = 0.3

[material.anisotropy]
strength = 0.8                     # -1 to 1 (0 = isotropic)
rotation = 0.0                     # Radians
direction_map = "aniso_dir.png"    # Optional direction texture
```

### Subsurface Scattering (Skin/Wax)

Light penetrating and scattering inside material:

```toml
[material]
albedo = [0.9, 0.7, 0.6, 1.0]     # Skin tone
metallic = 0.0
roughness = 0.5

[material.subsurface]
color = [1.0, 0.4, 0.3]           # Scattered light color
radius = [1.0, 0.2, 0.1]          # Scatter distance per RGB channel
factor = 0.5                       # SSS intensity
```

### Iridescence (Soap Bubbles/Oil)

Thin-film interference effects:

```toml
[material]
albedo = [0.1, 0.1, 0.1, 1.0]
metallic = 0.0
roughness = 0.0

[material.iridescence]
factor = 1.0                       # Effect intensity
ior = 1.3                          # Thin-film IOR
thickness_range = [100, 400]       # Thickness in nanometers
```

## Standalone Material Files

Create reusable material definitions in separate files:

### materials/metal_gold.toml

```toml
[material]
name = "Gold Metal"
shader = "shaders/pbr.wgsl"
blend_mode = "opaque"

[material.properties]
albedo = [1.0, 0.766, 0.336, 1.0]
metallic = 1.0
roughness = 0.3

[material.textures]
# Optional texture overrides
```

### Reference in Scene

```toml
[[entities]]
name = "golden_statue"
mesh = "assets/models/statue.glb"
material = "materials/metal_gold.toml"
```

## Blend Modes

```toml
[material]
blend_mode = "opaque"    # Default, no transparency
# OR
blend_mode = "alpha"     # Standard alpha blending
# OR
blend_mode = "additive"  # Additive blending (glow)
# OR
blend_mode = "masked"    # Alpha cutoff (foliage)
alpha_cutoff = 0.5       # For masked mode
```

## Double-Sided Rendering

```toml
[material]
double_sided = true      # Render both faces
cull_mode = "none"       # none, front, back
```

## Combined Material Example

### Pearlescent Car Paint

```toml
[material]
name = "Pearlescent White"
shader = "shaders/pbr_advanced.wgsl"
blend_mode = "opaque"

[material.properties]
albedo = [0.9, 0.9, 0.95, 1.0]
metallic = 0.7
roughness = 0.3

[material.clearcoat]
intensity = 1.0
roughness = 0.05

[material.iridescence]
factor = 0.3
ior = 1.8
thickness_range = [200, 400]
```

### Tinted Glass

```toml
[material]
name = "Green Tinted Glass"
shader = "shaders/pbr_transmission.wgsl"
blend_mode = "alpha"

[material.properties]
albedo = [1.0, 1.0, 1.0, 0.1]
metallic = 0.0
roughness = 0.0

[material.transmission]
factor = 0.95
ior = 1.5
thickness = 0.02
attenuation_color = [0.2, 0.8, 0.3]
attenuation_distance = 0.5
```

### Human Skin

```toml
[material]
name = "Skin"
shader = "shaders/pbr_subsurface.wgsl"
blend_mode = "opaque"

[material.properties]
albedo = { texture = "skin_albedo.png" }
metallic = 0.0
roughness = { texture = "skin_roughness.png" }
normal_map = "skin_normal.png"

[material.subsurface]
color = [1.0, 0.4, 0.3]
radius = [1.0, 0.2, 0.1]
factor = 0.5
```

## C++ Material API

```cpp
#include <void_engine/render/material.hpp>

using namespace void_engine::render;

// Create material from file
auto result = MaterialLoader::load("materials/gold.toml");
MaterialHandle gold = result.unwrap();

// Create material programmatically
Material material;
material.set_property("albedo", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
material.set_property("metallic", 0.0f);
material.set_property("roughness", 0.5f);
material.set_texture("normal_map", normal_texture);

MaterialHandle handle = material_cache.create("red_plastic", material);

// Apply to entity
renderer.submit(mesh, handle, transform, layer);
```

## Hot-Reload Support

Materials support hot-reloading:

```cpp
// Material files are automatically watched
material_cache.enable_hot_reload(true);

// Manual reload
material_cache.reload(handle);

// Shader dependency tracking
// When a shader changes, all materials using it are invalidated
shader_manager.on_shader_changed([&](ShaderId id) {
    material_cache.invalidate_by_shader(id);
});
```
