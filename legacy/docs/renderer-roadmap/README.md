# Void GUI v2 - AAA Renderer Implementation Roadmap

## Overview

This roadmap defines the implementation phases required to bring the Void GUI v2 renderer to AAA-quality feature parity with three.js and beyond. Each phase is a self-contained feature set with clear requirements, acceptance criteria, and implementation guidance.

## Architecture Principles

1. **ECS-First**: All features integrate with `void_ecs` using proper components and systems
2. **IR Patches**: State changes flow through `void_ir` for atomic, rollback-capable updates
3. **Hot-Swappable**: Every feature supports runtime modification without restart (see [Hot-Swap Compliance](#hot-swap-compliance))
4. **GPU-Agnostic**: Render graph abstraction in `void_render` with wgpu backend
5. **Fault-Tolerant**: Failed features degrade gracefully (shader failure = black layer)

## Hot-Swap Compliance

**CRITICAL**: All phases must adhere to the project's core philosophy: "Everything is Hot-Swappable". See `docs/project-summary.md` Section 2 for the full specification.

### Required Hot-Swap Mechanisms

| Component Type | Mechanism | Implementation |
|---------------|-----------|----------------|
| **Components** | `HotReloadable` trait | Serialize state, survive reload |
| **Assets** | File watch + atomic swap | via `void_asset_server` |
| **Shaders** | Naga recompilation | Frame-boundary swap |
| **GPU Buffers** | Double-buffering | No frame stutter |
| **Systems** | Plugin lifecycle | State preserved |

### Per-Phase Hot-Swap Requirements

Each phase document MUST include a "Hot-Swap Support" section specifying:

1. **Serialization**: All components derive `Serialize`/`Deserialize`
2. **State Preservation**: How state survives module hot-reload
3. **Asset Dependencies**: What happens when dependent assets reload
4. **Frame-Boundary Updates**: Changes apply atomically between frames
5. **Rollback Support**: Failed updates can be reverted
6. **Graceful Degradation**: Missing resources show placeholder, not crash

### Hot-Swap State Flow

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Snapshot   │────▶│  Serialize  │────▶│   Unload    │
│  Old State  │     │   to Bytes  │     │  Old Module │
└─────────────┘     └─────────────┘     └─────────────┘
                                               │
                                               ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Restore   │◀────│ Deserialize │◀────│    Load     │
│  New State  │     │ from Bytes  │     │  New Module │
└─────────────┘     └─────────────┘     └─────────────┘
                           │
                    ┌──────┴──────┐
                    ▼             ▼
              [Success]      [Failure]
                    │             │
                    │      ┌──────┴──────┐
                    │      │  Rollback   │
                    │      │ to Snapshot │
                    │      └─────────────┘
                    ▼
             [Frame Continues]
```

### Component Hot-Swap Template

Every renderer component MUST implement:

```rust
use serde::{Serialize, Deserialize};
use void_core::HotReloadable;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MyComponent {
    // All fields must be serializable
    pub data: SomeData,

    // Transient state marked with skip
    #[serde(skip)]
    pub cached_value: Option<CachedData>,
}

impl HotReloadable for MyComponent {
    fn snapshot(&self) -> Vec<u8> {
        bincode::serialize(self).unwrap()
    }

    fn restore(bytes: &[u8]) -> Result<Self, HotReloadError> {
        bincode::deserialize(bytes).map_err(|e| HotReloadError::Deserialize(e.to_string()))
    }

    fn on_reload(&mut self) {
        // Rebuild transient state
        self.cached_value = None;
    }
}
```

### Asset Hot-Reload Integration

All asset-dependent components must handle:

```rust
impl AssetDependent for MeshRenderer {
    fn on_asset_changed(&mut self, asset_id: AssetId, new_version: u64) {
        if self.mesh_id == Some(asset_id) {
            // Mark for GPU buffer rebuild
            self.needs_rebuild = true;
        }
    }

    fn asset_dependencies(&self) -> Vec<AssetId> {
        self.mesh_id.into_iter().collect()
    }
}
```

### Shader Hot-Reload

All render passes must support shader hot-reload:

```rust
impl ShaderHotReload for LightingPass {
    fn shader_paths(&self) -> Vec<&str> {
        vec!["shaders/lighting.wgsl", "shaders/pbr.wgsl"]
    }

    fn on_shader_changed(&mut self, path: &str, new_module: &wgpu::ShaderModule) {
        // Rebuild pipeline at frame boundary
        self.pipeline_dirty = true;
        self.pending_shader = Some(new_module.clone());
    }

    fn apply_pending(&mut self, device: &wgpu::Device) {
        if let Some(shader) = self.pending_shader.take() {
            self.rebuild_pipeline(device, &shader);
        }
    }
}
```

### Acceptance Criteria Addendum

Every phase's acceptance criteria MUST include:

- [ ] All components implement `Serialize`/`Deserialize`
- [ ] All components implement `HotReloadable`
- [ ] Asset-dependent components implement `AssetDependent`
- [ ] Shader-using passes implement `ShaderHotReload`
- [ ] Changes apply at frame boundary (no mid-frame corruption)
- [ ] Failed reloads rollback cleanly
- [ ] Test: hot-reload preserves visual state

## Phase Summary

| Phase | Feature | Priority | Complexity | Dependencies |
|-------|---------|----------|------------|--------------|
| 1 | Scene Graph | Critical | Medium | void_ecs, void_math |
| 2 | Camera System | Critical | Medium | Phase 1 |
| 3 | Mesh Import | Critical | High | void_asset, gltf |
| 4 | Instancing | High | Medium | Phase 3 |
| 5 | Lighting | Critical | High | Phase 1, 2 |
| 6 | Shadow Mapping | High | Very High | Phase 5 |
| 7 | Advanced Materials | High | High | Phase 3 |
| 8 | Keyframe Animation | High | High | Phase 3 |
| 9 | Animation Blending | Medium | High | Phase 8 |
| 10 | Picking & Raycasting | High | Medium | Phase 1, 14 |
| 11 | Entity Input Events | Medium | Medium | Phase 10 |
| 12 | Multi-Pass Entities | High | Medium | void_render |
| 13 | Custom Render Passes | Medium | High | Phase 12 |
| 14 | Spatial Queries | Critical | Medium | Phase 1 |
| 15 | LOD System | Medium | Medium | Phase 3, 14 |
| 16 | Scene Streaming | Medium | High | Phase 1 |
| 17 | Precision Management | Low | High | Phase 1, 2 |
| 18 | Debug Hooks | High | Low | All phases |

## Implementation Order (Recommended)

### Tier 1: Foundation (Weeks 1-4)
1. **Phase 1: Scene Graph** - Everything else depends on hierarchy
2. **Phase 14: Spatial Queries** - Required for culling and picking
3. **Phase 2: Camera System** - Must render something

### Tier 2: Core Rendering (Weeks 5-10)
4. **Phase 3: Mesh Import** - Real geometry
5. **Phase 5: Lighting** - Proper shading
6. **Phase 12: Multi-Pass Entities** - Render pipeline structure
7. **Phase 7: Advanced Materials** - Visual quality

### Tier 3: Shadows & Animation (Weeks 11-16)
8. **Phase 6: Shadow Mapping** - Depth and realism
9. **Phase 8: Keyframe Animation** - Motion
10. **Phase 9: Animation Blending** - Smooth transitions

### Tier 4: Interaction & Polish (Weeks 17-22)
11. **Phase 10: Picking & Raycasting** - User interaction
12. **Phase 11: Entity Input Events** - Event-driven interaction
13. **Phase 4: Instancing** - Performance optimization
14. **Phase 15: LOD System** - Distance-based optimization

### Tier 5: Advanced (Weeks 23+)
15. **Phase 13: Custom Render Passes** - Extensibility
16. **Phase 16: Scene Streaming** - Large worlds
17. **Phase 17: Precision Management** - Massive scale
18. **Phase 18: Debug Hooks** - Development tooling

## Directory Structure

```
docs/renderer-roadmap/
├── README.md                      # This file
├── hot-swap-addendum.md           # Hot-swap compliance requirements
├── phase-01-scene-graph.md
├── phase-02-camera-system.md
├── phase-03-mesh-import.md
├── phase-04-instancing.md
├── phase-05-lighting.md
├── phase-06-shadow-mapping.md
├── phase-07-advanced-materials.md
├── phase-08-keyframe-animation.md
├── phase-09-animation-blending.md
├── phase-10-picking-raycasting.md
├── phase-11-entity-input-events.md
├── phase-12-multi-pass-entities.md
├── phase-13-custom-render-passes.md
├── phase-14-spatial-queries.md
├── phase-15-lod-system.md
├── phase-16-scene-streaming.md
├── phase-17-precision-management.md
└── phase-18-debug-hooks.md
```

## Crate Modifications by Phase

| Crate | Phases Affected |
|-------|-----------------|
| void_ecs | 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 14, 15, 16 |
| void_render | 4, 5, 6, 7, 12, 13, 18 |
| void_math | 1, 8, 9, 10, 14, 17 |
| void_shader | 5, 6, 7 |
| void_asset | 3, 8 |
| void_asset_server | 3, 8 |
| void_ir | 1, 11, 16 |
| void_engine | 10, 11, 18 |
| void_kernel | 16, 18 |
| void_editor | All phases (UI integration) |

## Quality Standards

### Code Requirements
- All public APIs documented with rustdoc
- Unit tests for core logic (>80% coverage)
- Integration tests for system interactions
- Benchmark tests for performance-critical paths

### Performance Targets
- 60 FPS at 1080p with 10,000 entities
- 144 FPS at 1080p with 1,000 entities
- Shadow maps: <2ms per light
- Frustum culling: <0.5ms for 100,000 entities

### Compatibility
- wgpu backends: Vulkan, Metal, DX12, WebGPU
- Minimum GPU: GTX 1060 / RX 580 / Apple M1
- WebGPU support for browser deployment

## Claude Agent Integration

Each phase has an associated Claude skill for implementation assistance:

```
/renderer-phase-01  # Scene Graph implementation
/renderer-phase-02  # Camera System implementation
...
/renderer-phase-18  # Debug Hooks implementation
```

Use `/renderer-status` to check overall progress.

## Getting Started

1. Read the phase document for your target feature
2. Check dependencies are implemented
3. Create a feature branch: `feature/renderer-phase-XX-name`
4. Implement following the specification
5. Run tests: `cargo test -p void_ecs -p void_render`
6. Submit PR with phase checklist completed

---

**Last Updated**: 2026-01-11
**Status**: Planning Complete, Implementation Pending
