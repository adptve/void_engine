# Void GUI v2 Input Formats Reference

Complete documentation for all configuration files, assets, and input formats accepted by the Void GUI v2 metaverse operating system.

## Document Index

| Document | Description | Use When |
|----------|-------------|----------|
| [01-manifest-toml](./01-manifest-toml.md) | Package manifest structure | Creating new apps |
| [02-scene-basics](./02-scene-basics.md) | Scene file structure | Starting a new scene |
| [03-entities](./03-entities.md) | Entity definitions | Adding objects to scene |
| [04-materials](./04-materials.md) | PBR material properties | Texturing objects |
| [05-advanced-materials](./05-advanced-materials.md) | Glass, fabric, skin effects | Special material effects |
| [06-animations](./06-animations.md) | Animation types | Making things move |
| [07-cameras](./07-cameras.md) | Camera configuration | Setting up viewpoints |
| [08-lights](./08-lights.md) | Lighting system | Illuminating scenes |
| [09-shadows](./09-shadows.md) | Shadow configuration | Adding shadows |
| [10-environment](./10-environment.md) | Sky and ambient settings | Scene atmosphere |
| [11-particles](./11-particles.md) | Particle emitters | Fire, smoke, sparks |
| [12-textures](./12-textures.md) | Texture loading | Using image assets |
| [13-input-config](./13-input-config.md) | Input bindings | Controls and keybinds |
| [14-picking](./14-picking.md) | Object selection | Click-to-select |
| [15-lod](./15-lod.md) | Level of detail | Performance optimization |
| [16-debug](./16-debug.md) | Debug visualization | Development tools |
| [17-spatial](./17-spatial.md) | Spatial queries | Collision and queries |
| [18-boot-config](./18-boot-config.md) | Boot configuration | System startup |
| [19-layers](./19-layers.md) | Render layers | Compositing |
| [20-meshes](./20-meshes.md) | Mesh types and models | 3D geometry |

### Game Logic (21-29)

| Document | Description | Use When |
|----------|-------------|----------|
| [21-scripting](./21-scripting.md) | C++ and Blueprint scripting | Adding game logic |
| [22-triggers](./22-triggers.md) | Trigger volumes | Collision events, zones |
| [23-physics](./23-physics.md) | Physics and rigidbodies | Dynamic objects |
| [24-combat](./24-combat.md) | Health, damage, weapons | Combat systems |
| [25-inventory](./25-inventory.md) | Items and pickups | Collectibles, equipment |
| [26-audio](./26-audio.md) | Sound and music | Audio playback |
| [27-state](./27-state.md) | Game state and saves | Persistence, progress |
| [28-ui-hud](./28-ui-hud.md) | UI and HUD elements | Player interface |
| [29-ai-navigation](./29-ai-navigation.md) | AI and pathfinding | NPCs, enemies |

## Quick Start

### Minimal App Structure

```
my-app/
├── manifest.toml    # Required: app metadata
├── scene.toml       # Required: scene definition
└── assets/          # Optional: textures, models
    ├── textures/
    └── models/
```

### Minimal manifest.toml

```toml
[package]
name = "my-app"
version = "1.0.0"

[app]
scene = "scene.toml"
```

### Minimal scene.toml

```toml
[scene]
name = "My Scene"

[[entities]]
name = "cube"
mesh = "cube"
```

## File Formats

| Format | Extension | Purpose |
|--------|-----------|---------|
| TOML | `.toml` | Configuration files |
| PNG/JPG | `.png`, `.jpg` | Textures |
| HDR | `.hdr` | Environment maps |
| GLTF/GLB | `.gltf`, `.glb` | 3D models |
| WGSL | `.wgsl` | Shaders |
| Blueprint | `.bp` | Visual scripts |
| C++ | `.cpp`, `.h` | Native scripts |
| NavMesh | `.nav` | Navigation mesh |
| Behavior Tree | `.bt` | AI behavior |

## Configuration Hierarchy

```
boot.toml (system)
    └── manifest.toml (app)
            └── scene.toml (scene)
                    └── assets (resources)
```

## Source Code References

- Scene loader: `crates/void_runtime/src/scene_loader.rs`
- Texture manager: `crates/void_runtime/src/texture_manager.rs`
- Boot config: `crates/void_runtime/src/boot_config.rs`
- ECS components: `crates/void_ecs/src/render_components.rs`
