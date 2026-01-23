# void_engine Migration Completion Plan

> A prioritized, dependency-aware plan to complete the Rust → C++ migration.

---

## Overview

This plan is organized into **6 phases** based on dependencies. Each phase unlocks functionality for subsequent phases.

**Estimated Total Effort:** 8-12 weeks (1 developer, full-time)

---

## Phase 1: Asset Loading (CRITICAL)

**Why First:** Nothing else works without loading textures, models, and sounds from disk.

**Duration:** 1-2 weeks

### 1.1 void_asset - Local File Loaders

**Legacy Reference:** `legacy/crates/void_asset/src/`

| Task | Legacy File | Priority |
|------|-------------|----------|
| Image loader (PNG, JPG, TGA) | `image.rs` | P0 |
| HDR loader (Radiance .hdr) | `hdr.rs` | P0 |
| KTX2/DDS compressed textures | `ktx2.rs` | P1 |
| glTF 2.0 loader | `gltf.rs` | P0 |
| OBJ loader (fallback) | `obj.rs` | P2 |
| Audio loader (WAV, OGG, MP3) | `audio.rs` | P1 |
| Font loader (TTF/OTF) | `font.rs` | P2 |
| Asset cache with LRU eviction | `cache.rs` | P1 |
| Async loading with callbacks | `loader.rs` | P1 |

**Implementation Notes:**
- Use `stb_image.h` for PNG/JPG/TGA (single header, public domain)
- Use `cgltf.h` or `tinygltf` for glTF (single header)
- Use `stb_vorbis.c` for OGG audio
- Use `dr_wav.h`, `dr_mp3.h` for WAV/MP3

**Files to Create/Modify:**
```
src/asset/
  ├── image_loader.cpp      # NEW - stb_image wrapper
  ├── hdr_loader.cpp        # NEW - HDR/Radiance loader
  ├── gltf_loader.cpp       # NEW - glTF 2.0 parser
  ├── audio_loader.cpp      # NEW - WAV/OGG/MP3
  ├── asset_cache.cpp       # NEW - LRU cache
  └── asset_manager.cpp     # MODIFY - integrate loaders
```

**Deliverable:** Can load textures and models from disk.

---

## Phase 2: Complete Rendering Pipeline

**Why Second:** With assets loading, we can now render real content.

**Duration:** 2-3 weeks

### 2.1 void_render - Texture Management

**Legacy Reference:** `legacy/crates/void_runtime/src/scene_renderer.rs` (TextureManager)

| Task | Legacy Location | Priority |
|------|-----------------|----------|
| GPU texture upload | `scene_renderer.rs:TextureManager` | P0 |
| Texture sampling/filtering | `scene_renderer.rs` | P0 |
| Texture bind groups | `scene_renderer.rs` | P0 |
| Cubemap loading (skybox) | `scene_renderer.rs:load_environment` | P0 |
| Texture arrays | `scene_renderer.rs` | P2 |

**Files to Create/Modify:**
```
src/render/
  ├── texture_manager.cpp   # NEW - GPU texture management
  ├── gl_renderer.cpp       # MODIFY - integrate textures
  └── environment.cpp       # NEW - skybox/IBL
```

### 2.2 void_render - Mesh Loading

**Legacy Reference:** `legacy/crates/void_runtime/src/scene_renderer.rs` (load_gltf)

| Task | Priority |
|------|----------|
| Upload glTF meshes to GPU | P0 |
| Vertex attributes (position, normal, UV, tangent) | P0 |
| Index buffers | P0 |
| Multiple mesh primitives | P1 |
| Skinned meshes (skeleton) | P2 |
| Morph targets | P3 |

**Files to Modify:**
```
src/render/
  └── gl_renderer.cpp       # MODIFY - load_gltf(), mesh upload
```

### 2.3 void_render - Shadow Mapping

**Legacy Reference:** `legacy/crates/void_runtime/src/scene_renderer.rs` (ShadowConfig, render_shadow_pass)

| Task | Legacy Location | Priority |
|------|-----------------|----------|
| Shadow map FBO creation | `scene_renderer.rs:create_shadow_resources` | P0 |
| Depth-only shadow pass | `scene_renderer.rs:render_shadow_pass` | P0 |
| Shadow sampling in PBR shader | `pbr.wgsl` | P0 |
| Cascade shadow maps (CSM) | `scene_renderer.rs:ShadowCascade` | P1 |
| PCF soft shadows | `pbr.wgsl` | P1 |
| Contact hardening (PCSS) | P2 |

**Files to Create/Modify:**
```
src/render/
  ├── shadow_map.cpp        # NEW - shadow map rendering
  └── gl_renderer.cpp       # MODIFY - integrate shadow pass
```

### 2.4 void_render - Environment & Sky

**Legacy Reference:** `legacy/crates/void_runtime/src/scene_renderer.rs` (SkyUniforms, render_sky)

| Task | Priority |
|------|----------|
| Procedural sky gradient | P0 |
| HDR environment cubemap | P1 |
| IBL diffuse irradiance | P1 |
| IBL specular pre-filtered | P1 |
| BRDF LUT | P1 |
| Atmospheric scattering | P3 |

### 2.5 void_render - Particle System

**Legacy Reference:** `legacy/crates/void_runtime/src/scene_renderer.rs` (ParticleSystem, ParticleEmitter)

| Task | Legacy Location | Priority |
|------|-----------------|----------|
| Particle emitter spawning | `scene_renderer.rs:ParticleEmitter` | P1 |
| Particle update (CPU) | `scene_renderer.rs:update_particles` | P1 |
| Billboard rendering | `scene_renderer.rs:render_particles` | P1 |
| GPU particle simulation | P2 |
| Particle textures/atlases | P2 |

### 2.6 void_shader - Shader Compilation (Optional)

**Legacy Reference:** `legacy/crates/void_shader/src/`

| Task | Priority |
|------|----------|
| Runtime GLSL compilation | P1 |
| Shader hot-reload from files | P1 |
| SPIR-V cross-compilation | P2 |
| Shader reflection | P2 |

**Note:** Current hardcoded shaders work. This is optimization.

**Deliverable:** Full PBR rendering with textures, models, shadows, skybox, particles.

---

## Phase 3: ECS Execution Engine

**Why Third:** Game logic requires entity queries and systems.

**Duration:** 1-2 weeks

### 3.1 void_ecs - Core Execution

**Legacy Reference:** `legacy/crates/void_ecs/src/`

| Task | Legacy File | Priority |
|------|-------------|----------|
| Entity allocator with generations | `entity.rs` | P0 |
| Component storage (SoA) | `storage.rs` | P0 |
| Archetype management | `archetype.rs` | P0 |
| Query iteration | `query.rs` | P0 |
| System scheduling | `system.rs` | P1 |
| Change detection | `change.rs` | P2 |
| Entity commands (deferred) | `commands.rs` | P1 |
| Relationships/hierarchy | `hierarchy.rs` | P2 |

**Files to Create:**
```
src/ecs/
  ├── entity_allocator.cpp  # NEW - generational entity IDs
  ├── component_storage.cpp # NEW - SoA storage
  ├── archetype.cpp         # NEW - archetype tables
  ├── query.cpp             # NEW - query execution
  ├── system.cpp            # NEW - system scheduler
  └── world.cpp             # MODIFY - integrate all
```

**Deliverable:** Can spawn entities, add components, run queries, execute systems.

---

## Phase 4: Physics Simulation

**Why Fourth:** Many games need collision and physics.

**Duration:** 1-2 weeks

### 4.1 void_physics - Integration

**Legacy Reference:** `legacy/crates/void_physics/src/`

**Recommended Backend:** Jolt Physics (modern, performant, MIT license)

| Task | Priority |
|------|----------|
| Jolt integration setup | P0 |
| Rigidbody creation | P0 |
| Collider shapes (box, sphere, capsule, mesh) | P0 |
| Collision detection callbacks | P0 |
| Raycasting | P0 |
| Joints (hinge, ball, fixed) | P1 |
| Character controller | P1 |
| Trigger volumes | P1 |

**Files to Create:**
```
src/physics/
  ├── jolt_backend.cpp      # NEW - Jolt Physics wrapper
  ├── rigidbody.cpp         # NEW - rigidbody management
  ├── collider.cpp          # NEW - collider shapes
  ├── raycast.cpp           # NEW - raycasting
  └── physics_world.cpp     # MODIFY - integrate backend
```

**Deliverable:** Objects fall, collide, respond to forces.

---

## Phase 5: Audio Playback

**Why Fifth:** Audio is important but not blocking.

**Duration:** 1 week

### 5.1 void_audio - Playback Engine

**Legacy Reference:** `legacy/crates/void_audio/src/`

**Recommended Backend:** miniaudio (single header, public domain)

| Task | Priority |
|------|----------|
| miniaudio integration | P0 |
| Sound playback (fire and forget) | P0 |
| Looping sounds | P0 |
| Volume/pitch control | P0 |
| 3D spatial audio | P1 |
| Audio groups/mixer | P1 |
| Effects (reverb, delay) | P2 |
| Streaming large files | P2 |

**Files to Create:**
```
src/audio/
  ├── miniaudio_backend.cpp # NEW - miniaudio wrapper
  ├── sound.cpp             # NEW - sound playback
  ├── listener.cpp          # NEW - 3D listener
  └── audio_system.cpp      # MODIFY - integrate backend
```

**Deliverable:** Can play sounds, music, 3D audio.

---

## Phase 6: Gameplay Systems & Polish

**Why Last:** These build on everything above.

**Duration:** 2-3 weeks

### 6.1 Complete Gameplay Modules

Each of these needs their execution logic implemented:

| Module | Key Missing Functionality |
|--------|--------------------------|
| void_combat | Damage calculation, hit detection |
| void_inventory | Item operations, container logic |
| void_triggers | Trigger evaluation, action execution |
| void_gamestate | Save/load persistence, serialization |
| void_hud | HUD rendering, input handling |
| void_ai | NavMesh generation, pathfinding (Recast/Detour) |

### 6.2 void_graph - Visual Scripting

**Legacy Reference:** `legacy/crates/void_graph/src/`

| Task | Priority |
|------|----------|
| Node execution engine | P2 |
| Graph compiler | P2 |
| Editor integration | P3 |

### 6.3 void_presenter - GPU Backends (Optional)

Only needed if switching from OpenGL:

| Task | Priority |
|------|----------|
| Vulkan backend | P3 |
| WebGPU backend | P3 |
| Metal backend | P3 |

### 6.4 void_xr - VR/AR Support (Optional)

| Task | Priority |
|------|----------|
| OpenXR integration | P3 |
| Hand tracking | P3 |
| Passthrough AR | P3 |

---

## Recommended Order (Summary)

```
Week 1-2:   Phase 1 - Asset Loading (textures, models)
Week 3-5:   Phase 2 - Rendering (textures, shadows, sky, particles)
Week 6-7:   Phase 3 - ECS Execution
Week 8-9:   Phase 4 - Physics
Week 10:    Phase 5 - Audio
Week 11-12: Phase 6 - Gameplay Systems
```

---

## Dependencies Between Phases

```
Phase 1 (Assets) ─────┬────► Phase 2 (Rendering)
                      │
                      └────► Phase 5 (Audio)

Phase 2 (Rendering) ──────► Phase 6 (HUD, Particles)

Phase 3 (ECS) ────────┬────► Phase 4 (Physics)
                      │
                      └────► Phase 6 (Gameplay)

Phase 4 (Physics) ────────► Phase 6 (Combat, Triggers)
```

---

## Quick Wins (Can Do Immediately)

These require minimal effort and provide visible progress:

1. **Texture loading with stb_image** (~2 hours)
   - Drop in `stb_image.h`
   - Add `ImageLoader::load()`
   - Upload to GPU as GL texture

2. **glTF loading with cgltf** (~4 hours)
   - Drop in `cgltf.h`
   - Parse mesh data
   - Upload vertices/indices to GPU

3. **Basic shadow map** (~4 hours)
   - Create depth FBO
   - Render scene from light POV
   - Sample in PBR shader

4. **Procedural skybox** (~2 hours)
   - Already have sky gradient in scene.toml
   - Just need to render fullscreen quad with gradient

---

## Third-Party Libraries to Add

| Library | Purpose | License | Header-Only |
|---------|---------|---------|-------------|
| stb_image.h | Image loading | Public Domain | Yes |
| stb_image_write.h | Image saving | Public Domain | Yes |
| cgltf.h | glTF parsing | MIT | Yes |
| miniaudio.h | Audio playback | Public Domain | Yes |
| Jolt Physics | Physics engine | MIT | No (static lib) |
| Recast/Detour | NavMesh/Pathfinding | zlib | No (static lib) |

---

## Validation Checklist

After each phase, verify against legacy:

### Phase 1 Complete When:
- [ ] Can load PNG/JPG textures
- [ ] Can load HDR environment maps
- [ ] Can load glTF models
- [ ] Can load WAV/OGG audio files

### Phase 2 Complete When:
- [ ] Textured models render correctly
- [ ] PBR materials match legacy appearance
- [ ] Shadows render from directional lights
- [ ] Skybox/environment renders
- [ ] Particles spawn and render

### Phase 3 Complete When:
- [ ] Can spawn 10,000+ entities
- [ ] Queries return correct components
- [ ] Systems execute in order
- [ ] Entity hierarchy works

### Phase 4 Complete When:
- [ ] Objects fall with gravity
- [ ] Collisions are detected
- [ ] Raycasts work
- [ ] Character controller moves

### Phase 5 Complete When:
- [ ] Sounds play on events
- [ ] 3D audio positioning works
- [ ] Music loops correctly
- [ ] Volume control works

### Phase 6 Complete When:
- [ ] Full legacy model-viewer example runs identically
- [ ] All scene.toml features work
- [ ] Hot-reload works for all asset types

---

## Final Target

**The migration is complete when:**

```
.\void_runtime.exe examples\model-viewer
```

Produces **identical visual and behavioral output** to:

```
cargo run -p void_runtime -- examples/model-viewer
```

Including:
- All entities with correct textures and materials
- All animations playing
- All lights with shadows
- Environment/skybox
- Particles
- Camera controls
- Hot-reload of scene.toml, shaders, and assets
