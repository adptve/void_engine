# void_presenter

Multi-platform presenter abstraction for the Void Engine / Metaverse OS.

## Overview

The presenter abstraction layer provides a unified interface for presenting frames across different output targets:

- **Desktop Presenter**: winit/wgpu windowed and fullscreen (Windows/macOS/Linux)
- **WebXR Presenter**: Browser-based VR/AR via WebXR API (future)
- **PC Link Presenter**: Streaming to VR headsets (future)
- **Bare Metal Presenter**: DRM/KMS direct framebuffer (Linux) (future)
- **Presenter Manager**: Multiple simultaneous presenters

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  KERNEL (immortal)                                           │
│                                                              │
│       ┌───────────┬───────────┬───────────┐                 │
│       ▼           ▼           ▼           ▼                 │
│   ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐              │
│   │Desktop │ │ WebXR  │ │PC Link │ │  Bare  │              │
│   │(active)│ │(active)│ │ (idle) │ │ Metal  │              │
│   └────────┘ └────────┘ └────────┘ └────────┘              │
│                                                              │
│   Presenters connect/disconnect. Kernel continues.          │
└──────────────────────────────────────────────────────────────┘
```

## Core Principles

1. **XR is a Presenter, Not a Kernel**: Presenters are transient, kernel is immortal
2. **Multiple Active Presenters**: Same kernel serves desktop AND WebXR simultaneously
3. **Rehydration First**: Sessions disconnect and reconnect; state must survive
4. **No Platform Lock-In**: No assumptions about windowing, shells, or OS UI

## Features

- **desktop** - Desktop windowed/fullscreen presenter (winit/wgpu)
- **drm-backend** - Linux DRM/KMS bare metal presenter
- **webgpu** - WebGPU backend support
- **webxr** - WebXR VR/AR support (future)

## Usage

### Basic Desktop Presenter

```rust
use void_presenter::{DesktopPresenter, PresenterId, PresenterConfig};
use winit::window::Window;
use std::sync::Arc;

// Create window
let window = Arc::new(window_builder.build(&event_loop)?);

// Create presenter
let id = PresenterId::new(1);
let mut presenter = DesktopPresenter::new(id, window).await?;

// Render loop
loop {
    // Begin frame
    let mut frame = presenter.begin_frame()?;

    // ... render to frame ...

    // Present
    presenter.present(frame)?;
}
```

### With Presenter Manager

```rust
use void_presenter::{PresenterManager, DesktopPresenter};

let manager = PresenterManager::new();

// Register desktop presenter
let id = manager.allocate_id();
let presenter = Box::new(DesktopPresenter::new(id, window).await?);
manager.register(presenter);

// Begin frames on all presenters
let frames = manager.begin_all_frames();

// Render to each frame...

// Present all
manager.present_all(frames);
```

### Rehydration (Hot-Swap)

```rust
// Capture state before disconnect
let state = presenter.rehydration_state();

// ... presenter disconnects ...

// Create new presenter and restore state
let mut new_presenter = DesktopPresenter::new(id, new_window).await?;
new_presenter.rehydrate(state)?;

// Frame numbers and configuration preserved!
```

## Implementation Status

| Presenter | Status | Platform | Notes |
|-----------|--------|----------|-------|
| Desktop Windowed | ✅ Complete | Win/Mac/Linux | Production ready |
| Desktop Fullscreen | ✅ Complete | Win/Mac/Linux | Via winit |
| WebXR Immersive | 🚧 Planned | Browser | WebXR API |
| WebXR Inline | 🚧 Planned | Browser | AR overlay |
| PC Link | 🚧 Planned | Windows | Quest streaming |
| Native XR | 🚧 Planned | Standalone | OpenXR |
| Bare Metal | 🚧 Planned | Linux | DRM/KMS |
| Headless | ✅ Complete | Any | Testing |

## Desktop Presenter Details

### Surface Management

- Automatic surface configuration on creation
- Proper resize handling with surface reconfiguration
- Surface loss recovery (automatic recreation)
- Surface outdated handling (automatic reconfiguration)

### Present Modes

| Mode | Behavior | Use Case |
|------|----------|----------|
| `Immediate` | No VSync, may tear | Lowest latency, benchmarks |
| `Mailbox` | VSync, may drop frames | Low latency, no tearing |
| `Fifo` | VSync, no drops | Guaranteed no tearing, higher latency |
| `FifoRelaxed` | VSync normally, tear when late | Adaptive VSync |

### Rehydration

The desktop presenter preserves:
- Frame number (for continuity)
- Surface size (width/height)
- Surface format
- Configuration settings

## Building

```bash
# Desktop presenter only
cargo build -p void_presenter --features desktop

# With DRM backend (Linux)
cargo build -p void_presenter --features drm-backend

# All features
cargo build -p void_presenter --all-features
```

## Testing

```bash
# Run tests
cargo test -p void_presenter --features desktop

# Run with logging
RUST_LOG=debug cargo test -p void_presenter --features desktop -- --nocapture
```

## Dependencies

- `winit` - Cross-platform windowing (desktop feature)
- `wgpu` - Modern cross-platform graphics API
- `parking_lot` - High-performance locks
- `serde` - Serialization
- `thiserror` - Error handling
- `glam` - Math library

## Integration with void_runtime

The desktop presenter is used by `void_runtime` for desktop mode:

```rust
// In void_runtime
use void_presenter::{DesktopPresenter, PresenterManager};

let presenter_manager = PresenterManager::new();
let id = presenter_manager.allocate_id();

let presenter = Box::new(
    DesktopPresenter::new(id, window).await?
);

presenter_manager.register(presenter);
```

## Architecture Compliance

This implementation follows the presenter abstraction architecture:

- ✅ Implements full `Presenter` trait
- ✅ Does NOT crash kernel on disconnect
- ✅ Supports rehydration for hot-swap
- ✅ Can run alongside other presenters
- ✅ Backend-agnostic (works with Vulkan/Metal/DX12/WebGPU)

## License

MIT OR Apache-2.0
