# VRR and HDR Support in void_compositor

This document describes the Variable Refresh Rate (VRR) and High Dynamic Range (HDR) support in the Metaverse OS compositor.

## Overview

void_compositor provides comprehensive support for:

- **VRR (Variable Refresh Rate)**: FreeSync, G-Sync, VESA AdaptiveSync
- **HDR (High Dynamic Range)**: HDR10, HLG, wide color gamut

These features enable:
- Lower input latency (VRR)
- Smoother gameplay with variable framerates (VRR)
- Increased visual fidelity with higher brightness and contrast (HDR)
- Wide color gamut for more accurate color reproduction

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Application Layer                                          │
│  - Requests VRR/HDR                                         │
│  - Provides content metadata                                │
├─────────────────────────────────────────────────────────────┤
│  Compositor (void_compositor)                               │
│  ├─ VRR Management                                          │
│  │  ├─ Capability detection (vrr_capable property)         │
│  │  ├─ Dynamic refresh rate adaptation                     │
│  │  └─ Frame scheduler integration                         │
│  ├─ HDR Management                                          │
│  │  ├─ Capability detection (HDR_OUTPUT_METADATA)          │
│  │  ├─ Color space conversion                              │
│  │  └─ Metadata blob creation                              │
│  └─ FrameScheduler                                          │
│     ├─ VRR-aware timing                                     │
│     └─ Content velocity tracking                            │
├─────────────────────────────────────────────────────────────┤
│  DRM/KMS (Linux Kernel)                                     │
│  ├─ VRR enablement (drm_connector_set_vrr_enabled)          │
│  └─ HDR metadata (hdr_output_metadata property)             │
└─────────────────────────────────────────────────────────────┘
```

## VRR (Variable Refresh Rate)

### Detection

VRR capability is detected via the DRM `vrr_capable` connector property:

```rust
use void_compositor::{Compositor, CompositorConfig};

let compositor = Compositor::new(CompositorConfig::default())?;

// Check if VRR is supported
let caps = compositor.capabilities();
if caps.vrr_supported {
    println!("VRR is supported!");

    // Get detailed capability info
    if let Some(vrr_cap) = compositor.vrr_capability() {
        println!("VRR range: {}-{} Hz",
            vrr_cap.min_refresh_rate.unwrap_or(0),
            vrr_cap.max_refresh_rate.unwrap_or(0)
        );
    }
}
```

### Modes

VRR supports multiple operating modes:

| Mode | Description | Use Case |
|------|-------------|----------|
| `Disabled` | Fixed refresh rate | Default, maximum compatibility |
| `Auto` | Adaptive based on content | Games, dynamic content |
| `MaximumPerformance` | Always max refresh | Fast-paced games, competitive |
| `PowerSaving` | Prefer lower refresh | Battery conservation, static content |

### Enabling VRR

```rust
use void_compositor::VrrMode;

// Enable VRR in auto mode
compositor.enable_vrr(VrrMode::Auto)?;

// Check active configuration
if let Some(vrr_config) = compositor.vrr_config() {
    println!("Current refresh: {} Hz", vrr_config.current_refresh_rate);
}
```

### Content Velocity Adaptation

The compositor automatically adapts refresh rate based on content changes:

```rust
// Update content velocity (0.0 = static, 1.0 = fast motion)
let velocity = calculate_scene_motion(); // Application-defined
compositor.frame_scheduler_mut().update_content_velocity(velocity);

// Compositor adapts refresh rate:
// - High velocity (>0.5) -> max refresh rate
// - Low velocity (<0.1) -> min refresh rate
// - Medium velocity -> interpolated
```

### Disabling VRR

```rust
compositor.disable_vrr()?;
```

## HDR (High Dynamic Range)

### Detection

HDR capability is detected via the DRM `HDR_OUTPUT_METADATA` property and EDID:

```rust
// Check if HDR is supported
let caps = compositor.capabilities();
if caps.hdr_supported {
    println!("HDR is supported!");

    // Get detailed capability info
    if let Some(hdr_cap) = compositor.hdr_capability() {
        println!("Max luminance: {} nits",
            hdr_cap.max_luminance.unwrap_or(0)
        );
        println!("Transfer functions: {:?}",
            hdr_cap.transfer_functions
        );
    }
}
```

### Transfer Functions

HDR supports multiple Electro-Optical Transfer Functions (EOTF):

| Transfer Function | Description | Standard |
|------------------|-------------|----------|
| `Sdr` | Standard Dynamic Range | Rec.709 / sRGB |
| `Pq` | Perceptual Quantizer | SMPTE ST 2084 (HDR10) |
| `Hlg` | Hybrid Log-Gamma | ARIB STD-B67 (Broadcast HDR) |
| `Linear` | Linear (for processing) | - |

### Color Primaries

Wide color gamut support:

| Color Space | Description | Coverage |
|-------------|-------------|----------|
| `Srgb` | Standard RGB | ~35% Rec.2020 |
| `DciP3` | Digital Cinema | ~54% Rec.2020 |
| `Rec2020` | Ultra Wide Gamut | 100% (reference) |
| `AdobeRgb` | Photography | ~52% Rec.2020 |

### Enabling HDR10

```rust
use void_compositor::{HdrConfig, TransferFunction};

// Create HDR10 configuration (1000 nits peak)
let hdr_config = HdrConfig::hdr10(1000);

// Enable HDR
compositor.enable_hdr(hdr_config)?;

// Check active configuration
if let Some(hdr_cfg) = compositor.hdr_config() {
    println!("Transfer: {}", hdr_cfg.transfer_function.name());
    println!("Color space: {}", hdr_cfg.color_primaries.name());
}
```

### Enabling HLG

```rust
// Create HLG configuration (600 nits peak)
let hdr_config = HdrConfig::hlg(600);
compositor.enable_hdr(hdr_config)?;
```

### Custom HDR Configuration

```rust
use void_compositor::{HdrConfig, TransferFunction, ColorPrimaries};

let mut hdr_config = HdrConfig::default();
hdr_config.enable(TransferFunction::Pq);
hdr_config.color_primaries = ColorPrimaries::DciP3;
hdr_config.max_luminance = 1500; // 1500 nits peak
hdr_config.max_content_light_level = Some(1200);
hdr_config.max_frame_average_light_level = Some(400);

compositor.enable_hdr(hdr_config)?;
```

### Disabling HDR

```rust
compositor.disable_hdr()?;
```

## Frame Scheduler Integration

The FrameScheduler is VRR-aware and adapts frame timing automatically:

```rust
let scheduler = compositor.frame_scheduler();

// Check if VRR is active
if scheduler.is_vrr_active() {
    println!("VRR active");
}

// Get effective frame budget (adapts with VRR)
let budget = scheduler.frame_budget();
println!("Frame budget: {:?}", budget);

// Get current content velocity
let velocity = scheduler.content_velocity();
println!("Content velocity: {:.2}", velocity);
```

## DRM Property Details

### VRR Properties

- **`vrr_capable`**: Read-only, indicates VRR support (0 or 1)
- **`VRR_ENABLED`**: Writable, enables/disables VRR on connector

### HDR Properties

- **`HDR_OUTPUT_METADATA`**: Blob property containing HDR static metadata
  - Display primaries (Rec.2020 coordinates)
  - White point
  - Max/min display mastering luminance
  - Max content light level (MaxCLL)
  - Max frame-average light level (MaxFALL)
  - EOTF type

- **`max_bpc`**: Maximum bits per component (8, 10, 12, 16)

## Performance Considerations

### VRR

- **Latency**: VRR reduces input latency by eliminating vsync wait
- **Smoothness**: Variable framerate appears smooth within VRR range
- **Power**: Higher refresh rates consume more power
- **Hysteresis**: Refresh rate changes have 5 Hz hysteresis to avoid flicker

### HDR

- **Bandwidth**: 10-bit color requires ~25% more bandwidth than 8-bit
- **Processing**: Tone mapping adds minimal GPU overhead
- **Compatibility**: Some applications may need color space conversion

## Troubleshooting

### VRR not working

1. Check kernel version (5.2+ required)
2. Verify display supports VRR (check EDID)
3. Ensure display is set to VRR mode in OSD
4. Check `vrr_capable` property: `drm_info`

### HDR not working

1. Check kernel version (5.12+ recommended)
2. Verify display supports HDR10 (EDID CEA-861.3)
3. Check for `HDR_OUTPUT_METADATA` property
4. Verify cable supports bandwidth (HDMI 2.0a+ or DP 1.4+)

## Example: Complete VRR + HDR Setup

```rust
use void_compositor::{
    Compositor, CompositorConfig, VrrMode, HdrConfig,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut compositor = Compositor::new(CompositorConfig::default())?;

    // Enable VRR if supported
    if compositor.capabilities().vrr_supported {
        compositor.enable_vrr(VrrMode::Auto)?;
        println!("VRR enabled");
    }

    // Enable HDR if supported
    if compositor.capabilities().hdr_supported {
        let hdr_config = HdrConfig::hdr10(1000);
        compositor.enable_hdr(hdr_config)?;
        println!("HDR enabled");
    }

    // Main loop
    while compositor.is_running() {
        compositor.dispatch()?;

        if compositor.should_render() {
            let target = compositor.begin_frame()?;

            // Render content...

            compositor.end_frame(target)?;
        }
    }

    // Cleanup
    compositor.disable_vrr()?;
    compositor.disable_hdr()?;
    compositor.shutdown();

    Ok(())
}
```

## Future Work

- [ ] EDID parsing for precise HDR capabilities
- [ ] HDR10+ dynamic metadata support
- [ ] Dolby Vision support
- [ ] Per-layer HDR metadata
- [ ] VRR min/max range configuration
- [ ] Multi-output VRR sync
- [ ] Color management pipeline integration
- [ ] HDR screenshot support

## References

- [VESA AdaptiveSync](https://www.vesa.org/adaptive-sync/)
- [SMPTE ST 2084 (PQ)](https://ieeexplore.ieee.org/document/7291707)
- [ARIB STD-B67 (HLG)](https://www.arib.or.jp/english/std_tr/broadcasting/desc/std-b67.html)
- [DRM KMS Documentation](https://dri.freedesktop.org/docs/drm/)
- [CTA-861-G (HDR metadata)](https://shop.cta.tech/products/a-dtv-profile-for-uncompressed-high-speed-digital-interfaces)
