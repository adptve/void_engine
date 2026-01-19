# Migration Guide: Desktop Presenter to void_presenter

This guide helps migrate from the old `void_runtime::presenter::DesktopPresenter` to the new `void_presenter::DesktopPresenter`.

## Overview

The desktop presenter has been moved from `void_runtime` to the dedicated `void_presenter` crate to:

1. **Separation of Concerns**: Presenters are now a standalone abstraction
2. **Multi-Platform Support**: Easier to add WebXR, PC Link, bare metal presenters
3. **Rehydration**: Built-in support for hot-swap and session recovery
4. **Multi-Presenter**: Support for multiple simultaneous output targets

## Key Changes

### 1. Import Changes

**Before** (void_runtime):
```rust
use crate::presenter::DesktopPresenter;
```

**After** (void_presenter):
```rust
use void_presenter::{DesktopPresenter, PresenterId, PresenterConfig};
```

### 2. Presenter Creation

**Before**:
```rust
let presenter = DesktopPresenter::new(window).await;
```

**After**:
```rust
let id = PresenterId::new(1); // Or allocate from PresenterManager
let presenter = DesktopPresenter::new(id, window).await?;

// Or with configuration:
let config = PresenterConfig {
    format: SurfaceFormat::Bgra8UnormSrgb,
    present_mode: PresentMode::Fifo,
    size: (1920, 1080),
    target_frame_rate: 60,
    ..Default::default()
};
let presenter = DesktopPresenter::with_config(id, window, config).await?;
```

### 3. Frame Acquisition and Presentation

**Before**:
```rust
let texture = presenter.acquire_frame()?;
// ... render to texture ...
// (texture was auto-presented)
```

**After**:
```rust
// Begin frame
let mut frame = presenter.begin_frame()?;

// Access frame info
let size = frame.size();
let number = frame.number();

// Mark render phases
frame.begin_render();
// ... render ...
frame.end_render();

// Present
presenter.present(frame)?;
```

### 4. Presenter Trait Implementation

The new presenter implements the `Presenter` trait:

```rust
pub trait Presenter: Send + Sync {
    fn id(&self) -> PresenterId;
    fn capabilities(&self) -> &PresenterCapabilities;
    fn config(&self) -> &PresenterConfig;
    fn reconfigure(&mut self, config: PresenterConfig) -> Result<(), PresenterError>;
    fn resize(&mut self, width: u32, height: u32) -> Result<(), PresenterError>;
    fn begin_frame(&mut self) -> Result<Frame, PresenterError>;
    fn present(&mut self, frame: Frame) -> Result<(), PresenterError>;
    fn size(&self) -> (u32, u32);
    fn is_valid(&self) -> bool;
    fn rehydration_state(&self) -> RehydrationState;
    fn rehydrate(&mut self, state: RehydrationState) -> Result<(), PresenterError>;
}
```

### 5. Resize Handling

**Before**:
```rust
presenter.resize((width, height));
```

**After**:
```rust
presenter.resize(width, height)?;
```

### 6. Device and Queue Access

**Before**:
```rust
let device = presenter.device();
let queue = presenter.queue();
```

**After** (same):
```rust
let device = presenter.device();
let queue = presenter.queue();
```

## New Features

### Rehydration Support

Capture and restore presenter state for hot-swap:

```rust
// Before disconnect/restart
let state = presenter.rehydration_state();

// After reconnect
presenter.rehydrate(state)?;
```

### Multi-Presenter Management

Use `PresenterManager` to manage multiple presenters:

```rust
let manager = PresenterManager::new();

// Create and register presenters
let id1 = manager.allocate_id();
let presenter1 = Box::new(DesktopPresenter::new(id1, window1).await?);
manager.register(presenter1);

let id2 = manager.allocate_id();
let presenter2 = Box::new(DesktopPresenter::new(id2, window2).await?);
manager.register(presenter2);

// Present to all
let frames = manager.begin_all_frames();
// ... render to each frame ...
let results = manager.present_all(frames);
```

### Capabilities Inspection

```rust
let caps = presenter.capabilities();
println!("Supported formats: {:?}", caps.formats);
println!("Supported present modes: {:?}", caps.present_modes);
println!("Max resolution: {:?}", caps.max_resolution);
println!("HDR support: {}", caps.hdr_support);
```

### Frame Timing

```rust
let frame = presenter.begin_frame()?;

// Check deadline
if let Some(deadline) = frame.deadline() {
    println!("Frame deadline: {:?}", deadline);
}

// Render...
frame.begin_render();
// ...
frame.end_render();

// Check render duration
if let Some(duration) = frame.render_duration() {
    println!("Render took: {:?}", duration);
}

presenter.present(frame)?;
```

## Step-by-Step Migration

### Step 1: Update Cargo.toml

Add void_presenter dependency:

```toml
[dependencies]
void_presenter = { path = "../void_presenter", features = ["desktop"] }
```

### Step 2: Update Imports

```rust
// Remove old import
// use crate::presenter::DesktopPresenter;

// Add new imports
use void_presenter::{
    DesktopPresenter,
    Presenter,
    PresenterId,
    PresenterConfig,
    PresenterError,
    Frame,
};
```

### Step 3: Update Presenter Creation

```rust
// Old:
// let presenter = DesktopPresenter::new(window).await;

// New:
let id = PresenterId::new(1);
let presenter = DesktopPresenter::new(id, window).await
    .expect("Failed to create presenter");
```

### Step 4: Update Render Loop

```rust
// Old:
// let texture = presenter.acquire_frame()?;
// // render to texture
// // (auto-presented)

// New:
let mut frame = presenter.begin_frame()?;

frame.begin_render();
// Render to surface via presenter.device() and presenter.queue()
frame.end_render();

presenter.present(frame)?;
```

### Step 5: Update Resize Handling

```rust
// Old:
// presenter.resize((width, height));

// New:
if let Err(e) = presenter.resize(width, height) {
    log::error!("Failed to resize: {:?}", e);
}
```

## Example: Complete Migration

### Before (void_runtime)

```rust
use crate::presenter::DesktopPresenter;

async fn create_presenter(window: Arc<Window>) -> DesktopPresenter {
    DesktopPresenter::new(window).await
}

fn render_frame(presenter: &mut DesktopPresenter) {
    let texture = presenter.acquire_frame().unwrap();
    // Render...
    // Auto-presented
}

fn handle_resize(presenter: &mut DesktopPresenter, width: u32, height: u32) {
    presenter.resize((width, height));
}
```

### After (void_presenter)

```rust
use void_presenter::{DesktopPresenter, PresenterId, Presenter, PresenterConfig};
use std::sync::Arc;

async fn create_presenter(window: Arc<Window>) -> Result<DesktopPresenter, PresenterError> {
    let id = PresenterId::new(1);
    DesktopPresenter::new(id, window).await
}

fn render_frame(presenter: &mut DesktopPresenter) -> Result<(), PresenterError> {
    let mut frame = presenter.begin_frame()?;

    frame.begin_render();
    // Render using presenter.device() and presenter.queue()
    frame.end_render();

    presenter.present(frame)?;
    Ok(())
}

fn handle_resize(presenter: &mut DesktopPresenter, width: u32, height: u32) -> Result<(), PresenterError> {
    presenter.resize(width, height)
}
```

## Benefits of Migration

1. **Better Architecture**: Separation of presentation from runtime
2. **Multi-Platform**: Easy to add WebXR, bare metal, etc.
3. **Hot-Swap**: Rehydration support for zero-downtime updates
4. **Multi-Presenter**: Support multiple outputs simultaneously
5. **Frame Tracking**: Built-in frame timing and statistics
6. **Error Handling**: Proper error types and recovery

## Troubleshooting

### Error: "No suitable adapter found"

**Solution**: Ensure wgpu backends are available:

```rust
let instance = Instance::new(InstanceDescriptor {
    backends: Backends::all(),
    ..Default::default()
});
```

### Error: "Surface lost"

**Solution**: The new presenter automatically handles surface loss:

```rust
// Surface loss is handled internally
let frame = presenter.begin_frame()?; // Will recreate if needed
```

### Error: "Frame acquisition failed"

**Solution**: Check window size is not zero:

```rust
if size.width > 0 && size.height > 0 {
    presenter.resize(size.width, size.height)?;
}
```

## Support

For issues or questions about migration:

1. Check the examples in `crates/void_presenter/examples/`
2. Review the presenter documentation
3. See `docs/architecture/05-PRESENTER-ABSTRACTION.md`
