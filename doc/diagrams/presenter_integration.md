# void_presenter Integration Architecture

## Overview

void_presenter provides a unified presentation layer that abstracts over multiple GPU backends and handles frame delivery to various output targets including windows, XR headsets, and offscreen buffers.

## Architecture Diagram

```
                              Application Layer
                                     │
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        MultiBackendPresenter                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │ Frame Timing    │  │ Statistics      │  │ Hot-Reload      │          │
│  │ & Pacing        │  │ Tracking        │  │ Support         │          │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘          │
└─────────────────────────────────────────────────────────────────────────┘
                                     │
                    ┌────────────────┼────────────────┐
                    ▼                ▼                ▼
          ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
          │  Window Target  │ │ Offscreen       │ │  XR Target      │
          │                 │ │ Target          │ │  (Stereo)       │
          └────────┬────────┘ └────────┬────────┘ └────────┬────────┘
                   │                   │                   │
                   └───────────────────┼───────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          ManagedSwapchain                                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │ Triple          │  │ Auto-Resize     │  │ Present Mode    │          │
│  │ Buffering       │  │ Handling        │  │ Switching       │          │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘          │
└─────────────────────────────────────────────────────────────────────────┘
                                     │
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                           IBackend Interface                             │
└─────────────────────────────────────────────────────────────────────────┘
          │                │                │                │
          ▼                ▼                ▼                ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │   Null   │    │   WGPU   │    │  OpenGL  │    │  OpenXR  │
    │ Backend  │    │ Backend  │    │ Backend  │    │ Backend  │
    └──────────┘    └────┬─────┘    └──────────┘    └──────────┘
                         │
         ┌───────────────┼───────────────┐
         ▼               ▼               ▼
    ┌──────────┐   ┌──────────┐   ┌──────────┐
    │  Vulkan  │   │  D3D12   │   │  Metal   │
    └──────────┘   └──────────┘   └──────────┘
```

## Component Relationships

### 1. MultiBackendPresenter
The main entry point that coordinates:
- Backend selection and switching
- Output target management
- Frame lifecycle
- Statistics collection

```cpp
// Usage example
MultiBackendPresenterConfig config;
config.backend_config.preferred_type = BackendType::Wgpu;
config.target_fps = 60;

MultiBackendPresenter presenter(config);
presenter.initialize();

// Create window output
auto target = presenter.create_output_target(window, target_config);

// Frame loop
while (running) {
    if (auto frame = presenter.begin_frame()) {
        // Render...
        presenter.end_frame(*frame);
    }
}
```

### 2. ManagedSwapchain
Wraps backend swapchains with:
- Automatic resize handling
- Triple buffering by default
- Frame pacing integration
- Statistics tracking

### 3. Backend Abstraction

| Backend | Platform | Notes |
|---------|----------|-------|
| Null | All | Testing, headless |
| WGPU | All | Recommended, auto-selects best API |
| OpenGL | All | Fallback |
| OpenXR | Desktop | VR/XR support |

### 4. Hot-Reload Support

```
┌─────────────────────────────────────────┐
│          Hot-Reload Lifecycle            │
├─────────────────────────────────────────┤
│ 1. prepare_reload()                     │
│    └─ Dehydrate state to RehydrationState│
│    └─ Release GPU resources             │
│                                         │
│ 2. [DLL/SO reload happens]              │
│                                         │
│ 3. finish_reload()                      │
│    └─ Recreate GPU resources            │
│    └─ Rehydrate from saved state        │
└─────────────────────────────────────────┘
```

## Data Flow

### Frame Lifecycle

```
begin_frame()
    │
    ├─► Poll backend events
    ├─► Update frame timing
    ├─► Acquire swapchain images
    │
    ▼
[Application renders to images]
    │
    ▼
end_frame()
    │
    ├─► Submit to GPU
    ├─► Present to display
    ├─► Update statistics
    │
    ▼
[Wait for next frame / pacing]
```

### Backend Switch Flow

```
switch_backend(new_type, reason)
    │
    ├─► Dehydrate current state
    ├─► Wait for GPU idle
    ├─► Destroy old backend
    ├─► Create new backend
    ├─► Recreate surfaces/swapchains
    ├─► Rehydrate state
    │
    ▼
[Emit BackendSwitchEvent]
```

## Integration Points

### With void_render
- Provides window/surface creation via GLFW
- Shares OpenGL context management
- Coordinates GPU resource handles

### With void_core
- Uses Result/Error types for error handling
- Implements HotReloadable interface
- Uses Version for compatibility checks

### With void_compositor
- Provides frame output targets
- Coordinates render-to-texture operations
- Manages layer composition timing

## Performance Considerations

1. **Frame Pacing**: Uses high-resolution clock with spin-wait for sub-millisecond precision
2. **Triple Buffering**: Reduces latency while preventing tearing
3. **No Present-Path Allocations**: All frame buffers pre-allocated
4. **Async GPU Work**: Overlapped CPU/GPU execution via frame-in-flight tracking

## Configuration Reference

### SwapchainConfig
```cpp
struct SwapchainConfig {
    uint32_t width;
    uint32_t height;
    SurfaceFormat format;      // Default: Bgra8UnormSrgb
    PresentMode present_mode;  // Default: Fifo (VSync)
    AlphaMode alpha_mode;      // Default: Opaque
    uint32_t image_count;      // Default: 3 (triple buffer)
    bool enable_hdr;           // Default: false
};
```

### MultiBackendPresenterConfig
```cpp
struct MultiBackendPresenterConfig {
    BackendConfig backend_config;
    uint32_t target_fps;           // Default: 60
    bool enable_frame_pacing;      // Default: true
    bool track_detailed_stats;     // Default: true
    size_t stats_history_size;     // Default: 300
    bool enable_hot_swap;          // Default: true
    optional<XrSessionConfig> xr_config;
};
```
