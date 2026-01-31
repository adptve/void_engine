# ECS Renderer Flow (Code-Verified)

This document summarizes the ECS renderer pipeline and includes a Mermaid diagram showing how runtime initialization and kernel stages drive rendering. It is based on the current runtime wiring and render system implementations.

## Overview

- `Runtime::init_render()` initializes the ECS render context and registers the ECS render systems with the kernel stages.【F:src/runtime/runtime.cpp†L622-L695】【F:src/runtime/runtime.cpp†L1464-L1537】
- `RenderContext::initialize()` loads OpenGL function pointers and initializes the render asset manager, which builds the default shader and built-in meshes used by the ECS renderer.【F:src/render/render_systems.cpp†L58-L81】【F:src/render/render_assets.cpp†L520-L546】
- The kernel executes render systems across stages (Update → RenderPrepare → Render), building a render queue and issuing GPU draw calls via `RenderSystem::run()`.【F:src/runtime/runtime.cpp†L884-L914】【F:src/render/render_systems.cpp†L520-L671】

## Mermaid Diagram

```mermaid
flowchart TD
    A[Runtime::init_render] --> B[init_render_context]
    B --> C[RenderContext::initialize
(load_opengl_functions + RenderAssetManager::initialize)]
    A --> D[register_engine_render_systems]

    D --> E[Stage::Update
TransformSystem]
    D --> F[Stage::RenderPrepare
CameraSystem → LightSystem → RenderPrepareSystem]
    D --> G[Stage::Render
RenderSystem]
    D --> H[Stage::HotReloadPoll
RenderAssetManager::poll_hot_reload]

    F --> I[RenderQueue built from
MeshComponent/MaterialComponent]
    I --> G
    G --> J[GPU draw calls
(default PBR shader)]
```

## Notes

- Built-in meshes (sphere, cube, etc.) and the default PBR shader are now created in the render asset manager during render context initialization so ECS mesh references resolve at runtime without the legacy SceneRenderer.【F:src/render/render_assets.cpp†L520-L546】【F:src/render/render_assets.cpp†L272-L454】
