# Quick Start Guide - Desktop Presenter

Get started with the desktop presenter in 5 minutes.

## Installation

Add to `Cargo.toml`:

```toml
[dependencies]
void_presenter = { path = "../void_presenter", features = ["desktop"] }
winit = "0.29"
pollster = "0.3"  # For async executor
```

## Minimal Example

```rust
use void_presenter::{DesktopPresenter, Presenter, PresenterId};
use winit::{
    application::ApplicationHandler,
    event::WindowEvent,
    event_loop::{ControlFlow, EventLoop},
    window::Window,
};
use std::sync::Arc;

struct App {
    window: Option<Arc<Window>>,
    presenter: Option<DesktopPresenter>,
}

impl ApplicationHandler for App {
    fn resumed(&mut self, event_loop: &winit::event_loop::ActiveEventLoop) {
        let window = Arc::new(event_loop.create_window(Default::default()).unwrap());
        let id = PresenterId::new(1);
        let presenter = pollster::block_on(async {
            DesktopPresenter::new(id, window.clone()).await.unwrap()
        });

        self.window = Some(window);
        self.presenter = Some(presenter);
    }

    fn window_event(
        &mut self,
        _: &winit::event_loop::ActiveEventLoop,
        _: winit::window::WindowId,
        event: WindowEvent,
    ) {
        match event {
            WindowEvent::RedrawRequested => {
                if let Some(presenter) = &mut self.presenter {
                    let mut frame = presenter.begin_frame().unwrap();
                    frame.begin_render();
                    // Render here...
                    frame.end_render();
                    presenter.present(frame).unwrap();
                }
                if let Some(window) = &self.window {
                    window.request_redraw();
                }
            }
            WindowEvent::Resized(size) => {
                if let Some(presenter) = &mut self.presenter {
                    presenter.resize(size.width, size.height).unwrap();
                }
            }
            _ => {}
        }
    }
}

fn main() {
    let event_loop = EventLoop::new().unwrap();
    event_loop.set_control_flow(ControlFlow::Poll);
    let mut app = App { window: None, presenter: None };
    event_loop.run_app(&mut app).unwrap();
}
```

## Common Patterns

### With Configuration

```rust
use void_presenter::{PresenterConfig, PresentMode, SurfaceFormat};

let config = PresenterConfig {
    format: SurfaceFormat::Bgra8UnormSrgb,
    present_mode: PresentMode::Fifo,  // VSync on
    size: (1920, 1080),
    target_frame_rate: 60,
    ..Default::default()
};

let presenter = DesktopPresenter::with_config(id, window, config).await?;
```

### Render to Surface

```rust
// Get wgpu device and queue
let device = presenter.device();
let queue = presenter.queue();

// Begin frame
let frame = presenter.begin_frame()?;

// Create command encoder
let mut encoder = device.create_command_encoder(&Default::default());

// Get surface texture view
// Note: In real usage, you'd get this from the frame's user data
// or create a view from the acquired texture

// Render pass
{
    let render_pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
        label: Some("Main Render Pass"),
        // ... configure render pass
    });
    // Draw calls...
}

// Submit commands
queue.submit(std::iter::once(encoder.finish()));

// Present
presenter.present(frame)?;
```

### Handle Errors

```rust
match presenter.begin_frame() {
    Ok(frame) => {
        // Render...
        presenter.present(frame)?;
    }
    Err(PresenterError::SurfaceLost) => {
        log::warn!("Surface lost, will recreate on next frame");
    }
    Err(e) => {
        log::error!("Failed to begin frame: {:?}", e);
    }
}
```

### Multiple Presenters

```rust
use void_presenter::PresenterManager;

let manager = PresenterManager::new();

// Create presenters for each window
for window in windows {
    let id = manager.allocate_id();
    let presenter = Box::new(DesktopPresenter::new(id, window).await?);
    manager.register(presenter);
}

// Render loop
loop {
    let frames = manager.begin_all_frames();

    for (id, frame) in frames {
        // Render to each frame...
    }

    manager.present_all(frames);
}
```

### Rehydration

```rust
// Save state
let state = presenter.rehydration_state();
save_to_disk(&state)?;

// Later, restore state
let state = load_from_disk()?;
presenter.rehydrate(state)?;
// Frame numbers and configuration preserved!
```

## Key Concepts

### PresenterId

Each presenter needs a unique ID:

```rust
let id = PresenterId::new(1);  // Manual
// Or
let id = manager.allocate_id();  // From manager
```

### Frame Lifecycle

```rust
// 1. Begin
let mut frame = presenter.begin_frame()?;

// 2. Render phases (optional, for timing)
frame.begin_render();
// ... render ...
frame.end_render();

// 3. Present
presenter.present(frame)?;
```

### Present Modes

```rust
PresentMode::Immediate  // No VSync, lowest latency
PresentMode::Mailbox    // VSync, may drop frames
PresentMode::Fifo       // VSync, no drops (default)
PresentMode::FifoRelaxed // Adaptive VSync
```

## Troubleshooting

### "No suitable adapter found"

Make sure you have a GPU available:

```rust
// Check available backends
log::info!("wgpu backends: {:?}", wgpu::Backends::all());
```

### "Surface lost"

Happens on GPU reset or minimize - handled automatically:

```rust
// The presenter will recreate the surface automatically
let frame = presenter.begin_frame()?; // Will succeed after recreation
```

### Window resize not working

Always handle resize events:

```rust
WindowEvent::Resized(size) => {
    if size.width > 0 && size.height > 0 {
        presenter.resize(size.width, size.height)?;
    }
}
```

## Next Steps

- See `examples/desktop_basic.rs` for a complete example
- See `examples/multi_presenter.rs` for multi-window usage
- Read `README.md` for detailed documentation
- Check `MIGRATION.md` if migrating from old presenter

## Resources

- [wgpu Tutorial](https://sotrh.github.io/learn-wgpu/)
- [winit Documentation](https://docs.rs/winit/)
- [Presenter Architecture](../../docs/architecture/05-PRESENTER-ABSTRACTION.md)
