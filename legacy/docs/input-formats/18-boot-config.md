# Boot Configuration

System-level startup configuration for the Void GUI runtime.

## Configuration Sources

Boot configuration is loaded from multiple sources (priority order):

1. **Command Line**: `--backend=smithay`
2. **Environment Variables**: `METAVERSE_BACKEND=smithay`
3. **Config File**: `boot.toml` or `/etc/metaverse/boot.conf`
4. **Auto-detection**: Based on available hardware

## Backend Selection

```toml
[boot]
backend = "winit"              # Primary display backend
fallback = "cli"               # Fallback if primary fails
debug = false                  # Enable debug logging
```

### Available Backends

| Backend | Platform | Description |
|---------|----------|-------------|
| `winit` | All | Window on existing display server |
| `smithay` | Linux | Full Wayland compositor (DRM/KMS) |
| `xr` | All | OpenXR VR/AR headset |
| `cli` | All | Command-line only, no graphics |
| `auto` | All | Auto-detect best option |

### Backend Selection Logic (Auto)

1. If XR hardware detected → `xr`
2. If Linux without display server → `smithay`
3. If display server available → `winit`
4. Fallback → `cli`

## Display Configuration

```toml
[display]
resolution = "1920x1080"       # Resolution or null for auto
refresh_rate = 60              # Hz or null for auto
vrr = true                     # Variable Refresh Rate
hdr = false                    # HDR output
scale = 1.0                    # UI scale factor
```

### Resolution Formats

```toml
resolution = "1920x1080"       # Exact resolution
resolution = "4k"              # Preset (3840x2160)
resolution = "1080p"           # Preset (1920x1080)
resolution = null              # Auto-detect
```

### Common Presets

| Preset | Resolution |
|--------|------------|
| `720p` | 1280x720 |
| `1080p` | 1920x1080 |
| `1440p` | 2560x1440 |
| `4k` | 3840x2160 |

## XR Configuration

```toml
[xr]
enabled = true                 # Enable XR support
mode = "vr"                    # XR mode
supersampling = 1.0            # Resolution multiplier
hand_tracking = true           # Enable hand tracking
passthrough = false            # Camera passthrough (AR)
```

### XR Modes

| Mode | Description |
|------|-------------|
| `vr` | Virtual Reality (full immersion) |
| `ar` | Augmented Reality (overlay on real world) |
| `mr` | Mixed Reality (virtual + real blending) |
| `desktop` | Flat screen in VR space |

### Supersampling

| Value | Quality | Performance |
|-------|---------|-------------|
| `0.8` | Lower | Better FPS |
| `1.0` | Native | Baseline |
| `1.2` | Higher | Reduced FPS |
| `1.5` | Very high | Demanding |

## Command Line Arguments

```bash
# Backend selection
cargo run -p void_runtime -- --backend winit
cargo run -p void_runtime -- --backend smithay
cargo run -p void_runtime -- --backend cli

# Combined options
cargo run -p void_runtime -- --backend winit --debug

# Load specific app
cargo run -p void_runtime -- --app examples/model-viewer
```

## Environment Variables

```bash
# Backend
export METAVERSE_BACKEND=winit
export METAVERSE_FALLBACK=cli

# Debug
export METAVERSE_DEBUG=true

# Display
export METAVERSE_RESOLUTION=1920x1080
export METAVERSE_VRR=true

# XR
export METAVERSE_XR_MODE=vr
export METAVERSE_XR_SUPERSAMPLING=1.2
```

## Boot Configuration Presets

### Desktop Development

```toml
[boot]
backend = "winit"
fallback = "cli"
debug = true

[display]
resolution = null              # Use window size
vrr = false
hdr = false
scale = 1.0

[xr]
enabled = false
```

### Linux Compositor

```toml
[boot]
backend = "smithay"
fallback = "winit"
debug = false

[display]
resolution = "1920x1080"
refresh_rate = 60
vrr = true
hdr = false
scale = 1.0

[xr]
enabled = false
```

### VR Mode

```toml
[boot]
backend = "xr"
fallback = "winit"
debug = false

[display]
vrr = true
hdr = true

[xr]
enabled = true
mode = "vr"
supersampling = 1.2
hand_tracking = true
passthrough = false
```

### AR Mode

```toml
[boot]
backend = "xr"
fallback = "winit"
debug = false

[xr]
enabled = true
mode = "ar"
supersampling = 1.0
hand_tracking = true
passthrough = true
```

### Headless/CI

```toml
[boot]
backend = "cli"
fallback = "cli"
debug = true

[display]
resolution = null

[xr]
enabled = false
```

## Complete Boot Configuration

```toml
[boot]
backend = "auto"
fallback = "cli"
debug = false

[display]
resolution = null
refresh_rate = null
vrr = true
hdr = false
scale = 1.0

[xr]
enabled = true
mode = "vr"
supersampling = 1.0
hand_tracking = true
passthrough = false
```

## Startup App

Specify an app to load automatically:

```toml
[boot]
startup_app = "examples/model-viewer"
```

Or via command line:

```bash
cargo run -p void_runtime -- --app examples/model-viewer
```

## Config File Locations

The runtime searches for boot configuration in:

1. `./boot.toml` (current directory)
2. `~/.config/void/boot.toml` (user config)
3. `/etc/metaverse/boot.conf` (system config)
