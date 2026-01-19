# Input Formats Reference

Documentation for all configuration files, assets, and input formats accepted by void_engine.

## Document Index

| Document | Description | Use When |
|----------|-------------|----------|
| [manifest.md](./manifest.md) | Package manifest structure | Creating new applications |
| [scene.md](./scene.md) | Scene file structure | Defining 3D scenes |
| [materials.md](./materials.md) | PBR material properties | Texturing objects |

## File Formats

| Format | Extension | Purpose |
|--------|-----------|---------|
| TOML | `.toml` | Configuration files |
| PNG/JPG | `.png`, `.jpg` | Textures |
| HDR | `.hdr` | Environment maps |
| GLTF/GLB | `.gltf`, `.glb` | 3D models |
| WGSL | `.wgsl` | Shaders |
| C++ | `.cpp`, `.hpp` | Native scripts |

## Application Structure

```
my_app/
├── manifest.toml        # Required: app metadata
├── scene.toml           # Required: initial scene
├── materials/           # Optional: material definitions
│   ├── metal.toml
│   └── glass.toml
├── shaders/             # Optional: custom shaders
│   └── custom.wgsl
└── assets/              # Optional: textures, models
    ├── textures/
    │   ├── albedo.png
    │   └── normal.png
    └── models/
        └── character.glb
```

## Configuration Hierarchy

```
manifest.toml (app configuration)
    └── scene.toml (scene definition)
            ├── entities (scene objects)
            ├── cameras (viewpoints)
            ├── lights (illumination)
            └── materials (inline or referenced)
```

## Quick Start

### Minimal Application

**manifest.toml**
```toml
[package]
name = "my-app"
version = "1.0.0"

[app]
scene = "scene.toml"
```

**scene.toml**
```toml
[scene]
name = "My Scene"

[[entities]]
name = "cube"
mesh = "cube"

[entities.material]
albedo = [1.0, 0.0, 0.0, 1.0]
```

### With Custom Materials

**manifest.toml**
```toml
[package]
name = "material-demo"
version = "1.0.0"

[app]
scene = "scene.toml"

[[app.layers]]
name = "world"
type = "content"
priority = 10
```

**materials/gold.toml**
```toml
[material]
name = "Gold"

[material.properties]
albedo = [1.0, 0.766, 0.336, 1.0]
metallic = 1.0
roughness = 0.3
```

**scene.toml**
```toml
[scene]
name = "Material Demo"

[[entities]]
name = "golden_sphere"
mesh = "sphere"
material = "materials/gold.toml"

[entities.transform]
position = [0, 1, 0]
```

## Data Types

| Type | Format | Example |
|------|--------|---------|
| Position | `[f32; 3]` | `[1.0, 2.0, 3.0]` |
| Color RGB | `[f32; 3]` | `[1.0, 0.5, 0.0]` |
| Color RGBA | `[f32; 4]` | `[1.0, 0.5, 0.0, 1.0]` |
| Rotation | `[f32; 3]` | `[0, 45, 0]` (degrees) |
| Quaternion | `[f32; 4]` | `[0, 0, 0, 1]` (xyzw) |
| Scale | `f32` or `[f32; 3]` | `2.0` or `[1, 2, 1]` |
| Bool | `bool` | `true`, `false` |
| String | `"string"` | `"my_name"` |
| Path | `"string"` | `"assets/texture.png"` |

## Path Resolution

All paths are relative to the application directory (containing `manifest.toml`):

```toml
# Correct path references
mesh = "assets/models/character.glb"
albedo = { texture = "assets/textures/wood.png" }
material = "materials/metal.toml"

# Paths are NOT absolute
# mesh = "/home/user/models/char.glb"  # WRONG
```

## Validation

The engine validates all input files:

- **Manifest**: Required fields, valid names, resource limits
- **Scene**: Entity names unique, valid references
- **Materials**: Valid property values, texture paths exist

Validation errors are reported with file path and line number when possible.

## Hot-Reload Support

All format files support hot-reloading during development:

```cpp
// Enable hot-reload
engine.enable_hot_reload(true);

// Files are automatically watched and reloaded
// - scene.toml changes reload scene
// - material.toml changes update materials
// - shader.wgsl changes recompile shaders
```

## C++ Loading API

```cpp
#include <void_engine/app/manifest.hpp>
#include <void_engine/scene/loader.hpp>
#include <void_engine/render/material.hpp>

// Load manifest
auto manifest = Manifest::load("manifest.toml").unwrap();

// Load scene
auto scene = SceneLoader::load(manifest.app().scene).unwrap();

// Load material
auto material = MaterialLoader::load("materials/gold.toml").unwrap();
```
