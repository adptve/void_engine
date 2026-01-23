//! Tests for Nebula Genesis example app
//!
//! These tests validate:
//! - Manifest correctness
//! - Package structure
//! - Particle system logic
//! - Shader compilation
//! - Resource limits

use std::path::Path;

/// Test manifest file exists and is valid TOML
#[test]
fn test_manifest_exists_and_valid() {
    let manifest_path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/nebula-genesis/manifest.toml");

    assert!(manifest_path.exists(), "manifest.toml should exist");

    let content = std::fs::read_to_string(&manifest_path).expect("Should read manifest");
    let parsed: toml::Value = toml::from_str(&content).expect("Should parse as valid TOML");

    // Verify required fields
    assert!(parsed.get("package").is_some(), "Should have [package] section");
    assert!(parsed.get("app").is_some(), "Should have [app] section");

    let package = parsed.get("package").unwrap();
    assert_eq!(
        package.get("name").unwrap().as_str().unwrap(),
        "nebula-genesis"
    );
    assert!(package.get("version").is_some(), "Should have version");
}

/// Test all required files exist
#[test]
fn test_package_structure() {
    let base = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/nebula-genesis");

    let required_files = vec![
        "manifest.toml",
        "scripts/main.vs",
        "scripts/systems/camera_controller.vs",
        "scripts/systems/input_handler.vs",
        "assets/shaders/galaxy_particle.wgsl",
        "assets/shaders/bloom.wgsl",
        "assets/shaders/nebula_cloud.wgsl",
    ];

    for file in required_files {
        let path = base.join(file);
        assert!(path.exists(), "Required file should exist: {}", file);
    }
}

/// Test shader files are valid WGSL (basic syntax check)
#[test]
fn test_shaders_syntax() {
    let base = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/nebula-genesis/assets/shaders");

    let shaders = vec![
        "galaxy_particle.wgsl",
        "bloom.wgsl",
        "nebula_cloud.wgsl",
    ];

    for shader in shaders {
        let path = base.join(shader);
        let content = std::fs::read_to_string(&path)
            .unwrap_or_else(|_| panic!("Should read shader: {}", shader));

        // Basic WGSL syntax checks
        assert!(
            content.contains("@vertex") || content.contains("@fragment") || content.contains("@compute"),
            "Shader {} should have entry point annotations",
            shader
        );

        // Check for common WGSL constructs
        assert!(
            content.contains("fn ") || content.contains("struct "),
            "Shader {} should have functions or structs",
            shader
        );
    }
}

/// Test manifest resource limits are reasonable
#[test]
fn test_resource_limits() {
    let manifest_path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/nebula-genesis/manifest.toml");

    let content = std::fs::read_to_string(&manifest_path).expect("Should read manifest");
    let parsed: toml::Value = toml::from_str(&content).expect("Should parse as valid TOML");

    let resources = parsed
        .get("app")
        .and_then(|app| app.get("resources"))
        .expect("Should have resources section");

    // Check entity limit is reasonable for particle system
    let max_entities = resources
        .get("max_entities")
        .and_then(|v| v.as_integer())
        .unwrap_or(0);
    assert!(
        max_entities >= 50000,
        "Should allow at least 50k entities for particle system"
    );

    // Check memory limit
    let max_memory = resources
        .get("max_memory")
        .and_then(|v| v.as_integer())
        .unwrap_or(0);
    assert!(
        max_memory >= 256 * 1024 * 1024,
        "Should allow at least 256MB for particle data"
    );
}

/// Test layer configuration
#[test]
fn test_layer_configuration() {
    let manifest_path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/nebula-genesis/manifest.toml");

    let content = std::fs::read_to_string(&manifest_path).expect("Should read manifest");
    let parsed: toml::Value = toml::from_str(&content).expect("Should parse as valid TOML");

    let layers = parsed
        .get("app")
        .and_then(|app| app.get("layers"))
        .and_then(|l| l.as_array())
        .expect("Should have layers array");

    // Check we have multiple layers for the effect
    assert!(layers.len() >= 5, "Should have at least 5 layers for full effect");

    // Verify layer types
    let layer_names: Vec<&str> = layers
        .iter()
        .filter_map(|l| l.get("name").and_then(|n| n.as_str()))
        .collect();

    assert!(layer_names.contains(&"star_field"), "Should have star_field layer");
    assert!(layer_names.contains(&"bloom"), "Should have bloom effect layer");
}

// ============================================================================
// PARTICLE SYSTEM UNIT TESTS
// ============================================================================

/// Simulated particle for testing
#[derive(Clone, Debug)]
struct TestParticle {
    position: [f32; 3],
    velocity: [f32; 3],
    life: f32,
}

impl TestParticle {
    fn new(x: f32, y: f32, z: f32) -> Self {
        Self {
            position: [x, y, z],
            velocity: [0.0, 0.0, 0.0],
            life: 1.0,
        }
    }

    fn update(&mut self, dt: f32, gravity_center: [f32; 3], gravity_strength: f32) {
        // Calculate direction to gravity center
        let dx = gravity_center[0] - self.position[0];
        let dy = gravity_center[1] - self.position[1];
        let dz = gravity_center[2] - self.position[2];

        let dist_sq = dx * dx + dy * dy + dz * dz + 0.1; // Softening
        let dist = dist_sq.sqrt();

        // Apply gravitational acceleration
        let accel = gravity_strength / dist_sq;
        self.velocity[0] += accel * dx / dist * dt;
        self.velocity[1] += accel * dy / dist * dt;
        self.velocity[2] += accel * dz / dist * dt;

        // Update position
        self.position[0] += self.velocity[0] * dt;
        self.position[1] += self.velocity[1] * dt;
        self.position[2] += self.velocity[2] * dt;
    }
}

#[test]
fn test_particle_gravity() {
    let mut particle = TestParticle::new(10.0, 0.0, 0.0);
    let center = [0.0, 0.0, 0.0];
    let gravity = 100.0;

    // Simulate for 1 second
    for _ in 0..60 {
        particle.update(1.0 / 60.0, center, gravity);
    }

    // Particle should have moved toward center
    let final_dist = (particle.position[0].powi(2)
        + particle.position[1].powi(2)
        + particle.position[2].powi(2))
    .sqrt();

    assert!(
        final_dist < 10.0,
        "Particle should move toward gravity center"
    );
}

#[test]
fn test_particle_orbital_motion() {
    // Particle with initial tangential velocity should orbit
    let mut particle = TestParticle::new(10.0, 0.0, 0.0);
    particle.velocity = [0.0, 0.0, 3.16]; // ~sqrt(gravity/distance)

    let center = [0.0, 0.0, 0.0];
    let gravity = 100.0;

    let initial_dist = 10.0;

    // Simulate for many frames
    for _ in 0..600 {
        particle.update(1.0 / 60.0, center, gravity);
    }

    // Should maintain roughly circular orbit
    let final_dist = (particle.position[0].powi(2)
        + particle.position[1].powi(2)
        + particle.position[2].powi(2))
    .sqrt();

    let dist_change = (final_dist - initial_dist).abs();
    assert!(
        dist_change < 5.0,
        "Orbital particle should maintain distance (change: {})",
        dist_change
    );
}

#[test]
fn test_many_particles_performance() {
    use std::time::Instant;

    let particle_count = 100_000;
    let mut particles: Vec<TestParticle> = (0..particle_count)
        .map(|i| {
            let angle = (i as f32) * 0.01;
            let r = 10.0 + (i as f32) * 0.0001;
            TestParticle::new(r * angle.cos(), 0.0, r * angle.sin())
        })
        .collect();

    let center = [0.0, 0.0, 0.0];
    let gravity = 100.0;

    let start = Instant::now();

    // Update all particles for one frame
    for particle in &mut particles {
        particle.update(1.0 / 60.0, center, gravity);
    }

    let elapsed = start.elapsed();
    let elapsed_ms = elapsed.as_secs_f64() * 1000.0;

    println!("Updated {} particles in {:.2}ms", particle_count, elapsed_ms);

    // Should complete in reasonable time (< 50ms for 100k particles)
    assert!(
        elapsed_ms < 50.0,
        "Particle update should be fast (took {:.2}ms)",
        elapsed_ms
    );
}

// ============================================================================
// COLOR UTILITY TESTS
// ============================================================================

fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a + (b - a) * t
}

fn lerp_color(c1: [f32; 4], c2: [f32; 4], t: f32) -> [f32; 4] {
    [
        lerp(c1[0], c2[0], t),
        lerp(c1[1], c2[1], t),
        lerp(c1[2], c2[2], t),
        lerp(c1[3], c2[3], t),
    ]
}

#[test]
fn test_color_lerp() {
    let white = [1.0, 1.0, 1.0, 1.0];
    let black = [0.0, 0.0, 0.0, 1.0];

    let mid = lerp_color(white, black, 0.5);
    assert!((mid[0] - 0.5).abs() < 0.001);
    assert!((mid[1] - 0.5).abs() < 0.001);
    assert!((mid[2] - 0.5).abs() < 0.001);

    let start = lerp_color(white, black, 0.0);
    assert!((start[0] - 1.0).abs() < 0.001);

    let end = lerp_color(white, black, 1.0);
    assert!((end[0] - 0.0).abs() < 0.001);
}

#[test]
fn test_hsv_to_rgb() {
    fn hsv_to_rgb(h: f32, s: f32, v: f32) -> [f32; 3] {
        let c = v * s;
        let x = c * (1.0 - ((h / 60.0) % 2.0 - 1.0).abs());
        let m = v - c;

        let (r, g, b) = if h < 60.0 {
            (c, x, 0.0)
        } else if h < 120.0 {
            (x, c, 0.0)
        } else if h < 180.0 {
            (0.0, c, x)
        } else if h < 240.0 {
            (0.0, x, c)
        } else if h < 300.0 {
            (x, 0.0, c)
        } else {
            (c, 0.0, x)
        };

        [r + m, g + m, b + m]
    }

    // Red
    let red = hsv_to_rgb(0.0, 1.0, 1.0);
    assert!((red[0] - 1.0).abs() < 0.01, "Red should be 1.0");
    assert!(red[1] < 0.01, "Green should be ~0");
    assert!(red[2] < 0.01, "Blue should be ~0");

    // Green
    let green = hsv_to_rgb(120.0, 1.0, 1.0);
    assert!(green[0] < 0.01);
    assert!((green[1] - 1.0).abs() < 0.01);
    assert!(green[2] < 0.01);

    // Blue
    let blue = hsv_to_rgb(240.0, 1.0, 1.0);
    assert!(blue[0] < 0.01);
    assert!(blue[1] < 0.01);
    assert!((blue[2] - 1.0).abs() < 0.01);
}
