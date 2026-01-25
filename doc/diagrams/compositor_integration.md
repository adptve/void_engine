# Compositor Integration Architecture

## Overview

The void_compositor module provides post-processing, HDR, VRR, and layer-based composition for Void Engine. This document describes the integration architecture and data flow.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           void_engine Runtime                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐ │
│  │  ECS World   │──▶│   Physics    │──▶│   Renderer   │──▶│  Compositor  │ │
│  └──────────────┘   └──────────────┘   └──────────────┘   └──────────────┘ │
│         │                  │                  │                  │          │
│         │                  │                  ▼                  ▼          │
│         │                  │          ┌──────────────┐   ┌──────────────┐  │
│         │                  │          │   Render     │   │   Layer      │  │
│         │                  │          │   Target     │──▶│  Compositor  │  │
│         │                  │          └──────────────┘   └──────────────┘  │
│         │                  │                                    │          │
│         │                  │                                    ▼          │
│         │                  │                             ┌──────────────┐  │
│         └──────────────────┴─────────────────────────────│   Swapchain  │  │
│                                                          │   (GLFW)     │  │
│                                                          └──────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Frame Pipeline

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Frame N Pipeline                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  1. Frame Start                                                              │
│     ├── FrameTiming.begin_frame()                                           │
│     ├── glfwPollEvents()                                                    │
│     └── EventBus.process()                                                  │
│                                                                              │
│  2. Update Phase                                                             │
│     ├── AssetServer.process()                                               │
│     ├── PhysicsWorld.step(dt)                                               │
│     ├── LiveSceneManager.update(dt)                                         │
│     └── AnimationSystem.update()                                            │
│                                                                              │
│  3. Render Phase                                                             │
│     ├── Renderer.update(dt)                                                 │
│     └── Renderer.render()  ──────────────┐                                  │
│                                          │                                  │
│  4. Compositor Phase                     │                                  │
│     ├── Compositor.begin_frame()         │                                  │
│     │   ├── Dispatch VRR events          │                                  │
│     │   └── Begin layer frame            ▼                                  │
│     ├── Compositor.apply_post_processing(render_texture)                    │
│     │   ├── Create/update layer from renderer output                        │
│     │   ├── Apply tone mapping (HDR → SDR if needed)                        │
│     │   └── Composite layers                                                │
│     └── Compositor.end_frame()                                              │
│         ├── Finalize composition                                            │
│         └── Update VRR content velocity                                     │
│                                                                              │
│  5. Present                                                                  │
│     └── glfwSwapBuffers()                                                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Component Interaction

### Compositor Facade

The `Compositor` class provides a simplified high-level interface:

```cpp
// Configuration
CompositorFacadeConfig config;
config.enable_hdr = false;
config.enable_vrr = false;
config.output_width = 1920;
config.output_height = 1080;

// Initialization
Compositor compositor(config);
compositor.initialize();

// Per-frame usage
compositor.begin_frame();
compositor.apply_post_processing(renderer_output);
compositor.end_frame();

// Cleanup
compositor.shutdown();
```

### Internal Components

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Compositor Facade                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────┐   ┌──────────────────┐   ┌──────────────────┐        │
│  │   ICompositor    │   │  LayerManager    │   │ ILayerCompositor │        │
│  │  (Factory-based) │   │  (Layer storage) │   │  (Composition)   │        │
│  └────────┬─────────┘   └────────┬─────────┘   └────────┬─────────┘        │
│           │                      │                      │                   │
│           ▼                      ▼                      ▼                   │
│  ┌──────────────────┐   ┌──────────────────┐   ┌──────────────────┐        │
│  │ FrameScheduler   │   │      Layer       │   │ SoftwareLayer    │        │
│  │   (VRR timing)   │   │   (Properties)   │   │   Compositor     │        │
│  └──────────────────┘   └──────────────────┘   └──────────────────┘        │
│           │                                                                  │
│           ▼                                                                  │
│  ┌──────────────────┐   ┌──────────────────┐                               │
│  │   VrrConfig      │   │    HdrConfig     │                               │
│  │  (FreeSync/etc)  │   │  (HDR10/HLG)     │                               │
│  └──────────────────┘   └──────────────────┘                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Feature Modules

### HDR (High Dynamic Range)

```
Input (Linear HDR)
      │
      ▼
┌─────────────────┐
│  Color Space    │  Rec.709 → Rec.2020
│  Conversion     │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Tone Mapping   │  ACES / Reinhard / Uncharted2
│                 │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Transfer Fn    │  PQ (ST 2084) / HLG / sRGB
│  Application    │
└────────┬────────┘
         │
         ▼
Output (Display)
```

### VRR (Variable Refresh Rate)

```
Frame Time History
      │
      ▼
┌─────────────────┐
│  Content        │  Motion analysis
│  Velocity       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Refresh Rate   │  Map velocity to Hz
│  Calculation    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  LFC (Low       │  Frame multiplication
│  Framerate      │  when below min VRR
│  Compensation)  │
└────────┬────────┘
         │
         ▼
Display Refresh
```

### Layer Composition

```
┌─────────────────────────────────────────────────────────────────┐
│                    Layer Stack (Bottom to Top)                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Layer 0 (Priority: -1000) ──► Background                       │
│      │                                                           │
│      ▼                                                           │
│  Layer 1 (Priority: 0)     ──► Scene Render (from Renderer)     │
│      │                                                           │
│      ▼                                                           │
│  Layer 2 (Priority: 100)   ──► Post-Processing Effects          │
│      │                                                           │
│      ▼                                                           │
│  Layer 3 (Priority: 500)   ──► UI Overlay                       │
│      │                                                           │
│      ▼                                                           │
│  Layer 4 (Priority: 1000)  ──► Debug Overlay                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Hot-Reload Support

The compositor implements `void_core::HotReloadable`:

```cpp
// Snapshot captures:
// - Configuration (HDR, VRR, output dimensions)
// - Frame number
// - Layer manager state
// - VRR config

// Restore reinitializes:
// - Compositor with saved config
// - Layer manager state
// - Maintains frame continuity
```

## Data Flow Summary

```
[Input Events] ──► [Event Bus] ──► [Services]
                       │
                       ▼
[Scene] ──► [ECS] ──► [Physics] ──► [Animation]
                                        │
                                        ▼
                               [Renderer] ──► [GPU Commands]
                                        │
                                        ▼
                               [Compositor]
                                   │
                        ┌──────────┼──────────┐
                        ▼          ▼          ▼
                    [HDR]      [Layers]    [VRR]
                        │          │          │
                        └──────────┼──────────┘
                                   │
                                   ▼
                              [Present]
                                   │
                                   ▼
                              [Display]
```

## Performance Considerations

1. **No allocations in hot path**: Layer content updates use pre-allocated buffers
2. **Dirty region tracking**: Only recomposite changed layers
3. **VRR optimization**: Content velocity drives refresh rate
4. **GPU acceleration**: Layer compositor can use GPU when available

## Integration Points

| System | Integration | Direction |
|--------|-------------|-----------|
| Renderer | Output texture | Renderer → Compositor |
| GLFW | Window/VSync | Bidirectional |
| Event Bus | Frame events | Compositor → Bus |
| Hot-Reload | State serialization | Bidirectional |
| Asset Server | Texture loading | Asset → Compositor |
