//! Tests for Synthwave Dreamscape example app
//!
//! These tests validate:
//! - Manifest correctness
//! - Grid mathematics
//! - Color theme system
//! - Beat synchronization
//! - CRT effect parameters

use std::path::Path;

/// Test manifest exists and is valid
#[test]
fn test_manifest_valid() {
    let manifest_path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/synthwave-dreamscape/manifest.toml");

    assert!(manifest_path.exists(), "manifest.toml should exist");

    let content = std::fs::read_to_string(&manifest_path).expect("Should read manifest");
    let parsed: toml::Value = toml::from_str(&content).expect("Should parse as valid TOML");

    let package = parsed.get("package").unwrap();
    assert_eq!(
        package.get("name").unwrap().as_str().unwrap(),
        "synthwave-dreamscape"
    );
}

/// Test package structure
#[test]
fn test_package_structure() {
    let base = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/synthwave-dreamscape");

    let required_files = vec![
        "manifest.toml",
        "scripts/main.vs",
        "assets/shaders/neon_grid.wgsl",
        "assets/shaders/synthwave_sun.wgsl",
        "assets/shaders/crt_effect.wgsl",
    ];

    for file in required_files {
        let path = base.join(file);
        assert!(path.exists(), "Required file should exist: {}", file);
    }
}

/// Test layer configuration for synthwave effect
#[test]
fn test_layer_stack() {
    let manifest_path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/synthwave-dreamscape/manifest.toml");

    let content = std::fs::read_to_string(&manifest_path).expect("Should read manifest");
    let parsed: toml::Value = toml::from_str(&content).expect("Should parse as valid TOML");

    let layers = parsed
        .get("app")
        .and_then(|app| app.get("layers"))
        .and_then(|l| l.as_array())
        .expect("Should have layers array");

    // Should have proper layer ordering for the synthwave look
    let layer_names: Vec<&str> = layers
        .iter()
        .filter_map(|l| l.get("name").and_then(|n| n.as_str()))
        .collect();

    // Critical layers for the effect
    assert!(layer_names.contains(&"sky"), "Should have sky layer");
    assert!(layer_names.contains(&"sun"), "Should have sun layer");
    assert!(layer_names.contains(&"grid"), "Should have grid layer");
    assert!(layer_names.contains(&"scanlines"), "Should have CRT scanlines layer");
}

// ============================================================================
// GRID MATH TESTS
// ============================================================================

/// Calculate grid line visibility
fn grid_line(coord: f32, width: f32) -> f32 {
    let grid_coord = coord.fract();
    let dist_to_line = (grid_coord - 0.5).abs() - 0.5 + width;
    if dist_to_line < 0.0 {
        1.0
    } else if dist_to_line < width {
        1.0 - dist_to_line / width
    } else {
        0.0
    }
}

#[test]
fn test_grid_line_at_integer() {
    // At integer coordinates, should be on a line
    let on_line = grid_line(5.0, 0.1);
    assert!(on_line > 0.9, "Integer coord should be on line (got {})", on_line);
}

#[test]
fn test_grid_line_between_integers() {
    // At 0.5 offset, should be furthest from lines
    let off_line = grid_line(5.5, 0.05);
    assert!(off_line < 0.1, "Half-integer should be off line (got {})", off_line);
}

#[test]
fn test_grid_scrolling() {
    // Scrolling should shift the grid pattern
    let static_value = grid_line(3.0, 0.1);
    let scrolled_value = grid_line(3.5, 0.1); // Scrolled by 0.5

    // Values should differ (one on line, one off)
    assert!(
        (static_value - scrolled_value).abs() > 0.5,
        "Scrolling should change line visibility"
    );
}

// ============================================================================
// COLOR THEME TESTS
// ============================================================================

#[derive(Clone, Debug)]
struct ColorTheme {
    name: &'static str,
    grid_primary: [f32; 4],
    grid_secondary: [f32; 4],
    sun_top: [f32; 4],
    sun_bottom: [f32; 4],
}

fn get_theme(name: &str) -> ColorTheme {
    match name {
        "classic" => ColorTheme {
            name: "classic",
            grid_primary: [1.0, 0.0, 0.8, 1.0],    // Magenta
            grid_secondary: [0.0, 1.0, 1.0, 1.0],   // Cyan
            sun_top: [1.0, 0.9, 0.3, 1.0],          // Yellow
            sun_bottom: [1.0, 0.2, 0.4, 1.0],       // Hot pink
        },
        "sunset" => ColorTheme {
            name: "sunset",
            grid_primary: [1.0, 0.5, 0.0, 1.0],
            grid_secondary: [1.0, 0.8, 0.0, 1.0],
            sun_top: [1.0, 0.9, 0.5, 1.0],
            sun_bottom: [1.0, 0.3, 0.0, 1.0],
        },
        "midnight" => ColorTheme {
            name: "midnight",
            grid_primary: [0.0, 0.5, 1.0, 1.0],
            grid_secondary: [0.5, 0.0, 1.0, 1.0],
            sun_top: [0.5, 0.5, 1.0, 1.0],
            sun_bottom: [0.2, 0.0, 0.5, 1.0],
        },
        _ => panic!("Unknown theme"),
    }
}

#[test]
fn test_theme_colors_valid() {
    let themes = vec!["classic", "sunset", "midnight"];

    for theme_name in themes {
        let theme = get_theme(theme_name);

        // All color values should be in valid range [0, 1]
        for i in 0..4 {
            assert!(
                theme.grid_primary[i] >= 0.0 && theme.grid_primary[i] <= 1.0,
                "Theme {} grid_primary[{}] should be valid",
                theme_name,
                i
            );
            assert!(
                theme.sun_top[i] >= 0.0 && theme.sun_top[i] <= 1.0,
                "Theme {} sun_top[{}] should be valid",
                theme_name,
                i
            );
        }
    }
}

#[test]
fn test_themes_are_distinct() {
    let classic = get_theme("classic");
    let midnight = get_theme("midnight");

    // Primary colors should differ significantly
    let color_diff = ((classic.grid_primary[0] - midnight.grid_primary[0]).powi(2)
        + (classic.grid_primary[1] - midnight.grid_primary[1]).powi(2)
        + (classic.grid_primary[2] - midnight.grid_primary[2]).powi(2))
    .sqrt();

    assert!(
        color_diff > 0.5,
        "Themes should be visually distinct (diff: {})",
        color_diff
    );
}

// ============================================================================
// BEAT SYNC TESTS
// ============================================================================

struct BeatTracker {
    bpm: f32,
    beat_time: f32,
    beat_intensity: f32,
}

impl BeatTracker {
    fn new(bpm: f32) -> Self {
        Self {
            bpm,
            beat_time: 0.0,
            beat_intensity: 0.0,
        }
    }

    fn update(&mut self, dt: f32) -> bool {
        let beat_period = 60.0 / self.bpm;
        self.beat_time += dt;
        self.beat_intensity *= 0.95; // Decay

        if self.beat_time >= beat_period {
            self.beat_time -= beat_period;
            self.beat_intensity = 1.0;
            return true; // Beat occurred
        }
        false
    }
}

#[test]
fn test_beat_timing_120bpm() {
    let mut tracker = BeatTracker::new(120.0);
    let dt = 1.0 / 60.0; // 60 FPS

    let mut beat_count = 0;
    let frames_per_second = 60;
    let total_frames = frames_per_second * 4; // 4 seconds

    for _ in 0..total_frames {
        if tracker.update(dt) {
            beat_count += 1;
        }
    }

    // At 120 BPM for 4 seconds, should have 8 beats
    assert_eq!(beat_count, 8, "Should have 8 beats in 4 seconds at 120 BPM");
}

#[test]
fn test_beat_intensity_decay() {
    let mut tracker = BeatTracker::new(120.0);

    // Trigger a beat
    tracker.beat_intensity = 1.0;

    // Simulate decay over several frames
    for _ in 0..30 {
        tracker.beat_intensity *= 0.95;
    }

    // Should decay significantly
    assert!(
        tracker.beat_intensity < 0.3,
        "Intensity should decay (got {})",
        tracker.beat_intensity
    );
}

// ============================================================================
// CRT EFFECT TESTS
// ============================================================================

fn barrel_distort(uv: (f32, f32), curvature: f32) -> (f32, f32) {
    let centered = (uv.0 * 2.0 - 1.0, uv.1 * 2.0 - 1.0);
    let offset = (centered.1 * centered.1 * curvature, centered.0 * centered.0 * curvature);
    let curved = (centered.0 + centered.0 * offset.0, centered.1 + centered.1 * offset.1);
    (curved.0 * 0.5 + 0.5, curved.1 * 0.5 + 0.5)
}

#[test]
fn test_crt_barrel_distortion_center() {
    // Center of screen should be unaffected
    let center = (0.5, 0.5);
    let distorted = barrel_distort(center, 0.1);

    assert!(
        (distorted.0 - 0.5).abs() < 0.001,
        "Center X should be unchanged"
    );
    assert!(
        (distorted.1 - 0.5).abs() < 0.001,
        "Center Y should be unchanged"
    );
}

#[test]
fn test_crt_barrel_distortion_edges() {
    // Edges should curve inward
    let edge = (0.0, 0.5);
    let distorted = barrel_distort(edge, 0.1);

    // Left edge should move right (toward center)
    assert!(distorted.0 > 0.0, "Left edge should move toward center");
}

#[test]
fn test_scanline_pattern() {
    fn scanline(y: f32, intensity: f32) -> f32 {
        let scanline_value = (y * std::f32::consts::PI * 2.0).sin();
        1.0 - intensity * (scanline_value * 0.5 + 0.5) * 0.5
    }

    // Scanlines should create alternating bright/dark pattern
    let bright = scanline(0.0, 0.5);
    let dark = scanline(0.5, 0.5);

    assert!(bright > dark, "Scanline pattern should alternate");
    assert!(
        (bright - dark).abs() > 0.1,
        "Scanline contrast should be visible"
    );
}

// ============================================================================
// PARTICLE SYSTEM TESTS
// ============================================================================

#[test]
fn test_particle_respawn() {
    struct Particle {
        z: f32,
    }

    let mut particle = Particle { z: -50.0 };
    let camera_z = 0.0;
    let spawn_distance = 50.0;

    // Simulate movement
    for _ in 0..1000 {
        particle.z += 0.1;

        // Respawn if past camera
        if particle.z > camera_z + 10.0 {
            particle.z = camera_z - spawn_distance;
        }
    }

    // Particle should have respawned and be behind camera
    assert!(
        particle.z < camera_z,
        "Particle should respawn behind camera"
    );
}

#[test]
fn test_particle_burst_spawn() {
    let mut particles: Vec<f32> = Vec::new();
    let max_particles = 500;
    let burst_size = 50;

    // Spawn burst
    for _ in 0..burst_size {
        if particles.len() < max_particles + 200 {
            particles.push(0.0);
        }
    }

    assert_eq!(particles.len(), burst_size, "Burst should add particles");

    // Spawn another burst near limit
    particles.resize(max_particles + 150, 0.0);

    for _ in 0..burst_size {
        if particles.len() < max_particles + 200 {
            particles.push(0.0);
        }
    }

    assert_eq!(
        particles.len(),
        max_particles + 200,
        "Should respect max limit"
    );
}
