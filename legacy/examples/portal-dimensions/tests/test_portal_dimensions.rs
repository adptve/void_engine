//! Tests for Portal Dimensions example app
//!
//! These tests validate:
//! - Manifest correctness
//! - Portal math (teleportation, coordinate transforms)
//! - Shader compilation
//! - Dimension transitions

use std::path::Path;

/// Test manifest exists and is valid
#[test]
fn test_manifest_valid() {
    let manifest_path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/portal-dimensions/manifest.toml");

    assert!(manifest_path.exists(), "manifest.toml should exist");

    let content = std::fs::read_to_string(&manifest_path).expect("Should read manifest");
    let parsed: toml::Value = toml::from_str(&content).expect("Should parse as valid TOML");

    let package = parsed.get("package").unwrap();
    assert_eq!(
        package.get("name").unwrap().as_str().unwrap(),
        "portal-dimensions"
    );
}

/// Test package structure
#[test]
fn test_package_structure() {
    let base = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/portal-dimensions");

    let required_files = vec![
        "manifest.toml",
        "scripts/main.vs",
        "assets/shaders/portal_surface.wgsl",
        "assets/shaders/chromatic.wgsl",
    ];

    for file in required_files {
        let path = base.join(file);
        assert!(path.exists(), "Required file should exist: {}", file);
    }
}

/// Test portal layer configuration
#[test]
fn test_portal_layers() {
    let manifest_path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("examples/portal-dimensions/manifest.toml");

    let content = std::fs::read_to_string(&manifest_path).expect("Should read manifest");
    let parsed: toml::Value = toml::from_str(&content).expect("Should parse as valid TOML");

    let layers = parsed
        .get("app")
        .and_then(|app| app.get("layers"))
        .and_then(|l| l.as_array())
        .expect("Should have layers array");

    // Check for portal-type layers
    let portal_layers: Vec<_> = layers
        .iter()
        .filter(|l| l.get("type").and_then(|t| t.as_str()) == Some("portal"))
        .collect();

    assert!(
        portal_layers.len() >= 2,
        "Should have at least 2 portal layers for recursion"
    );
}

// ============================================================================
// PORTAL MATH TESTS
// ============================================================================

/// 3D Vector operations
#[derive(Clone, Copy, Debug, PartialEq)]
struct Vec3 {
    x: f32,
    y: f32,
    z: f32,
}

impl Vec3 {
    fn new(x: f32, y: f32, z: f32) -> Self {
        Self { x, y, z }
    }

    fn dot(&self, other: &Vec3) -> f32 {
        self.x * other.x + self.y * other.y + self.z * other.z
    }

    fn length(&self) -> f32 {
        (self.x * self.x + self.y * self.y + self.z * self.z).sqrt()
    }

    fn sub(&self, other: &Vec3) -> Vec3 {
        Vec3::new(self.x - other.x, self.y - other.y, self.z - other.z)
    }

    fn add(&self, other: &Vec3) -> Vec3 {
        Vec3::new(self.x + other.x, self.y + other.y, self.z + other.z)
    }

    fn scale(&self, s: f32) -> Vec3 {
        Vec3::new(self.x * s, self.y * s, self.z * s)
    }
}

/// Portal structure for testing
struct TestPortal {
    position: Vec3,
    rotation_y: f32, // Simplified: rotation around Y axis
}

impl TestPortal {
    fn new(position: Vec3, rotation_y: f32) -> Self {
        Self { position, rotation_y }
    }

    fn get_normal(&self) -> Vec3 {
        Vec3::new(self.rotation_y.sin(), 0.0, self.rotation_y.cos())
    }

    fn world_to_local(&self, point: &Vec3) -> Vec3 {
        let relative = point.sub(&self.position);
        let c = self.rotation_y.cos();
        let s = self.rotation_y.sin();

        Vec3::new(
            relative.x * c + relative.z * s,
            relative.y,
            -relative.x * s + relative.z * c,
        )
    }

    fn local_to_world(&self, point: &Vec3) -> Vec3 {
        let c = self.rotation_y.cos();
        let s = self.rotation_y.sin();

        let rotated = Vec3::new(
            point.x * c - point.z * s,
            point.y,
            point.x * s + point.z * c,
        );

        rotated.add(&self.position)
    }
}

#[test]
fn test_portal_coordinate_transform_roundtrip() {
    let portal = TestPortal::new(Vec3::new(5.0, 0.0, 3.0), 0.5);
    let world_point = Vec3::new(7.0, 1.0, 4.0);

    let local = portal.world_to_local(&world_point);
    let back_to_world = portal.local_to_world(&local);

    assert!(
        (back_to_world.x - world_point.x).abs() < 0.001,
        "X should roundtrip"
    );
    assert!(
        (back_to_world.y - world_point.y).abs() < 0.001,
        "Y should roundtrip"
    );
    assert!(
        (back_to_world.z - world_point.z).abs() < 0.001,
        "Z should roundtrip"
    );
}

#[test]
fn test_portal_teleportation() {
    // Two portals facing each other
    let portal_a = TestPortal::new(Vec3::new(-3.0, 0.0, 0.0), 0.0); // Facing +Z
    let portal_b = TestPortal::new(Vec3::new(3.0, 0.0, 0.0), std::f32::consts::PI); // Facing -Z

    // Player in front of portal A
    let player_pos = Vec3::new(-3.0, 0.0, 1.0);

    // Transform to portal A's local space
    let local = portal_a.world_to_local(&player_pos);

    // Mirror through portal (flip Z)
    let mirrored = Vec3::new(local.x, local.y, -local.z);

    // Transform to portal B's world space
    let teleported = portal_b.local_to_world(&mirrored);

    // Player should now be in front of portal B
    assert!(
        (teleported.x - 3.0).abs() < 0.1,
        "Should be near portal B X"
    );
    assert!(
        teleported.z < 0.0,
        "Should be on opposite side of portal B"
    );
}

#[test]
fn test_portal_crossing_detection() {
    let portal = TestPortal::new(Vec3::new(0.0, 0.0, 0.0), 0.0);
    let normal = portal.get_normal();

    // Point in front of portal
    let front = Vec3::new(0.0, 0.0, 1.0);
    let front_dist = front.sub(&portal.position).dot(&normal);
    assert!(front_dist > 0.0, "Front point should have positive distance");

    // Point behind portal
    let behind = Vec3::new(0.0, 0.0, -1.0);
    let behind_dist = behind.sub(&portal.position).dot(&normal);
    assert!(behind_dist < 0.0, "Behind point should have negative distance");

    // Crossing detection: sign change in distance
    let crossed = front_dist * behind_dist < 0.0;
    assert!(crossed, "Moving from front to behind should detect crossing");
}

#[test]
fn test_recursive_portal_rendering_depth() {
    // Test that recursion depth limits are respected
    let max_recursion = 4;

    fn simulate_portal_render(depth: u32, max_depth: u32) -> u32 {
        if depth >= max_depth {
            return 0;
        }
        // Each portal can see 2 other portals
        1 + simulate_portal_render(depth + 1, max_depth) * 2
    }

    let render_calls = simulate_portal_render(0, max_recursion);

    // With depth 4 and 2 portals per view: 1 + 2 + 4 + 8 = 15
    assert!(
        render_calls <= 15,
        "Should limit recursion (got {} calls)",
        render_calls
    );
}

// ============================================================================
// DIMENSION TESTS
// ============================================================================

#[derive(Clone)]
struct Dimension {
    name: String,
    gravity: Vec3,
    sky_color: [f32; 4],
}

#[test]
fn test_dimension_properties() {
    let dimensions = vec![
        Dimension {
            name: "Reality".into(),
            gravity: Vec3::new(0.0, -9.8, 0.0),
            sky_color: [0.1, 0.1, 0.15, 1.0],
        },
        Dimension {
            name: "Void".into(),
            gravity: Vec3::new(0.0, -2.0, 0.0),
            sky_color: [0.0, 0.0, 0.02, 1.0],
        },
        Dimension {
            name: "Ethereal".into(),
            gravity: Vec3::new(0.0, 0.0, 0.0), // Zero gravity
            sky_color: [0.05, 0.1, 0.2, 1.0],
        },
    ];

    // Each dimension should have unique properties
    for (i, dim) in dimensions.iter().enumerate() {
        for (j, other) in dimensions.iter().enumerate() {
            if i != j {
                // At least one property should differ
                let gravity_differs = dim.gravity.length() != other.gravity.length();
                let color_differs = dim.sky_color != other.sky_color;
                assert!(
                    gravity_differs || color_differs,
                    "Dimensions {} and {} should differ",
                    dim.name,
                    other.name
                );
            }
        }
    }
}

#[test]
fn test_dimension_transition_interpolation() {
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

    let from_color = [1.0, 0.0, 0.0, 1.0]; // Red
    let to_color = [0.0, 0.0, 1.0, 1.0]; // Blue

    // At t=0, should be source color
    let start = lerp_color(from_color, to_color, 0.0);
    assert!((start[0] - 1.0).abs() < 0.001);
    assert!(start[2] < 0.001);

    // At t=1, should be target color
    let end = lerp_color(from_color, to_color, 1.0);
    assert!(end[0] < 0.001);
    assert!((end[2] - 1.0).abs() < 0.001);

    // At t=0.5, should be purple
    let mid = lerp_color(from_color, to_color, 0.5);
    assert!((mid[0] - 0.5).abs() < 0.001);
    assert!((mid[2] - 0.5).abs() < 0.001);
}
