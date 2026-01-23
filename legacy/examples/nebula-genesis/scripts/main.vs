// ============================================================================
// NEBULA GENESIS - A Procedural Galaxy Simulation
// ============================================================================
// This demo showcases:
// - 100,000+ particles with gravitational physics
// - GPU-accelerated particle rendering via instancing
// - Multi-layer composition with additive blending
// - Real-time bloom and glow effects
// - Interactive camera with orbit controls
// - Hot-swappable shaders for live editing
// ============================================================================

// Import our systems
import "systems/particle_system.vs";
import "systems/camera_controller.vs";
import "systems/input_handler.vs";
import "systems/galaxy_generator.vs";

// ============================================================================
// GLOBAL STATE
// ============================================================================

let galaxy = null;
let camera = null;
let input = null;
let time = 0.0;
let paused = false;

// Visual settings (hot-swappable)
let settings = {
    particle_count: 100000,
    galaxy_radius: 50.0,
    galaxy_thickness: 5.0,
    spiral_arms: 4,
    spiral_tightness: 0.5,
    core_brightness: 2.0,
    star_size_min: 0.02,
    star_size_max: 0.15,
    rotation_speed: 0.1,
    bloom_intensity: 1.5,
    bloom_threshold: 0.8,
    color_palette: "cosmic"  // cosmic, fire, ice, rainbow
};

// ============================================================================
// LIFECYCLE HOOKS
// ============================================================================

fn on_init() {
    print("=== NEBULA GENESIS ===");
    print("Initializing galaxy simulation...");

    // Initialize camera with orbit controls
    camera = create_orbit_camera({
        distance: 100.0,
        min_distance: 20.0,
        max_distance: 500.0,
        target: [0.0, 0.0, 0.0],
        pitch: 0.3,
        yaw: 0.0,
        fov: 60.0,
        smooth_factor: 0.1
    });

    // Initialize input handler
    input = create_input_handler();

    // Generate the galaxy!
    galaxy = generate_galaxy(settings);

    // Create background stars (distant, static)
    create_background_stars(5000);

    // Setup post-processing
    setup_bloom_pass(settings.bloom_intensity, settings.bloom_threshold);

    print("Galaxy initialized with " + str(settings.particle_count) + " stars");
    print("Controls: Mouse drag to orbit, scroll to zoom, SPACE to pause");
}

fn on_update(dt) {
    time += dt;

    // Handle input
    process_input(input, camera, dt);

    if !paused {
        // Update galaxy simulation
        update_galaxy(galaxy, dt, time);

        // Update camera
        update_camera(camera, dt);

        // Update visual effects
        update_effects(time);
    }

    // Always render
    render_galaxy(galaxy, camera, time);
    render_ui();
}

fn on_focus() {
    print("Welcome back to the cosmos!");
}

fn on_blur() {
    paused = true;
}

fn on_shutdown() {
    print("Returning to the void...");
    cleanup_galaxy(galaxy);
}

// ============================================================================
// GALAXY GENERATION
// ============================================================================

fn generate_galaxy(config) {
    let particles = [];
    let velocities = [];
    let colors = [];
    let sizes = [];

    let arm_count = config.spiral_arms;
    let arm_angle = TAU / arm_count;

    for i in range(0, config.particle_count) {
        // Determine which arm this star belongs to
        let arm = i % arm_count;
        let arm_offset = arm * arm_angle;

        // Distance from center (exponential distribution for realistic density)
        let t = random(0.0, 1.0);
        let r = config.galaxy_radius * pow(t, 0.5);

        // Spiral angle based on distance
        let spiral_angle = arm_offset + r * config.spiral_tightness + random(-0.3, 0.3);

        // Add some scatter perpendicular to spiral
        let scatter = random(-1.0, 1.0) * (1.0 - t) * 5.0;

        // Calculate position
        let x = r * cos(spiral_angle) + scatter * sin(spiral_angle);
        let z = r * sin(spiral_angle) - scatter * cos(spiral_angle);
        let y = random(-1.0, 1.0) * config.galaxy_thickness * exp(-r / config.galaxy_radius * 2.0);

        particles.push([x, y, z]);

        // Orbital velocity (Keplerian)
        let orbital_speed = sqrt(1.0 / max(r, 0.1)) * 10.0;
        let vx = -orbital_speed * sin(spiral_angle);
        let vz = orbital_speed * cos(spiral_angle);
        velocities.push([vx, 0.0, vz]);

        // Color based on distance and palette
        let color = get_star_color(r / config.galaxy_radius, config.color_palette);
        colors.push(color);

        // Size based on random + distance (larger near core)
        let size = lerp(config.star_size_max, config.star_size_min, t) * random(0.5, 1.5);
        sizes.push(size);
    }

    // Create entity for the galaxy
    let galaxy_entity = spawn_galaxy_entity(particles, velocities, colors, sizes);

    return {
        entity: galaxy_entity,
        particles: particles,
        velocities: velocities,
        colors: colors,
        sizes: sizes,
        config: config
    };
}

fn get_star_color(distance_normalized, palette) {
    // Distance: 0 = core, 1 = edge
    let t = distance_normalized;

    if palette == "cosmic" {
        // Core: hot white/blue, Edge: cool red/orange
        if t < 0.2 {
            return lerp_color([1.0, 1.0, 1.0, 1.0], [0.8, 0.9, 1.0, 1.0], t / 0.2);
        } else if t < 0.5 {
            return lerp_color([0.8, 0.9, 1.0, 1.0], [1.0, 0.8, 0.5, 1.0], (t - 0.2) / 0.3);
        } else {
            return lerp_color([1.0, 0.8, 0.5, 1.0], [1.0, 0.4, 0.2, 0.8], (t - 0.5) / 0.5);
        }
    } else if palette == "fire" {
        return lerp_color([1.0, 1.0, 0.8, 1.0], [1.0, 0.2, 0.0, 0.9], t);
    } else if palette == "ice" {
        return lerp_color([1.0, 1.0, 1.0, 1.0], [0.2, 0.5, 1.0, 0.8], t);
    } else {
        // Rainbow
        let hue = t * 360.0;
        return hsv_to_rgb(hue, 0.8, 1.0);
    }
}

fn spawn_galaxy_entity(positions, velocities, colors, sizes) {
    // Emit patch to create galaxy entity with instanced particle data
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 1 },
        op: "create",
        archetype: "ParticleSystem",
        components: {
            "Transform": {
                position: [0.0, 0.0, 0.0],
                rotation: [0.0, 0.0, 0.0, 1.0],
                scale: [1.0, 1.0, 1.0]
            },
            "ParticleBuffer": {
                positions: positions,
                velocities: velocities,
                colors: colors,
                sizes: sizes,
                count: len(positions)
            },
            "Material": {
                shader: "assets/shaders/galaxy_particle.wgsl",
                blend_mode: "additive",
                depth_write: false,
                depth_test: true
            },
            "Renderable": {
                mesh: "quad",  // Instanced quads
                instance_count: len(positions),
                layer: "star_field"
            }
        }
    });

    return { namespace: get_namespace(), local_id: 1 };
}

// ============================================================================
// GALAXY UPDATE (Physics Simulation)
// ============================================================================

fn update_galaxy(galaxy, dt, time) {
    let particles = galaxy.particles;
    let velocities = galaxy.velocities;
    let config = galaxy.config;

    // Gravitational constant (tuned for visual appeal)
    let G = 50.0;
    let core_mass = 1000.0;
    let softening = 1.0;  // Prevent singularities

    // Update each particle
    for i in range(0, len(particles)) {
        let pos = particles[i];
        let vel = velocities[i];

        // Distance to core
        let dx = -pos[0];
        let dy = -pos[1];
        let dz = -pos[2];
        let dist_sq = dx*dx + dy*dy + dz*dz + softening;
        let dist = sqrt(dist_sq);

        // Gravitational acceleration toward core
        let accel = G * core_mass / dist_sq;
        let ax = accel * dx / dist;
        let ay = accel * dy / dist;
        let az = accel * dz / dist;

        // Update velocity
        vel[0] += ax * dt;
        vel[1] += ay * dt;
        vel[2] += az * dt;

        // Update position
        pos[0] += vel[0] * dt;
        pos[1] += vel[1] * dt;
        pos[2] += vel[2] * dt;

        // Global rotation
        let rotation_angle = config.rotation_speed * dt;
        let new_x = pos[0] * cos(rotation_angle) - pos[2] * sin(rotation_angle);
        let new_z = pos[0] * sin(rotation_angle) + pos[2] * cos(rotation_angle);
        pos[0] = new_x;
        pos[2] = new_z;
    }

    // Emit patch to update particle buffer
    emit_patch({
        type: "component",
        entity: galaxy.entity,
        component: "ParticleBuffer",
        op: "update",
        fields: {
            positions: particles
        }
    });
}

// ============================================================================
// BACKGROUND STARS
// ============================================================================

fn create_background_stars(count) {
    let positions = [];
    let colors = [];
    let sizes = [];

    for i in range(0, count) {
        // Spherical distribution at far distance
        let theta = random(0.0, TAU);
        let phi = acos(random(-1.0, 1.0));
        let r = random(200.0, 500.0);

        let x = r * sin(phi) * cos(theta);
        let y = r * sin(phi) * sin(theta);
        let z = r * cos(phi);

        positions.push([x, y, z]);

        // Mostly white/blue stars
        let temp = random(0.0, 1.0);
        if temp < 0.7 {
            colors.push([1.0, 1.0, 1.0, random(0.3, 0.8)]);
        } else if temp < 0.9 {
            colors.push([0.8, 0.9, 1.0, random(0.4, 0.9)]);
        } else {
            colors.push([1.0, 0.9, 0.7, random(0.5, 1.0)]);
        }

        sizes.push(random(0.01, 0.05));
    }

    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 2 },
        op: "create",
        archetype: "StaticParticles",
        components: {
            "Transform": {
                position: [0.0, 0.0, 0.0],
                rotation: [0.0, 0.0, 0.0, 1.0],
                scale: [1.0, 1.0, 1.0]
            },
            "ParticleBuffer": {
                positions: positions,
                colors: colors,
                sizes: sizes,
                count: count
            },
            "Material": {
                shader: "assets/shaders/static_star.wgsl",
                blend_mode: "additive"
            },
            "Renderable": {
                mesh: "point",
                instance_count: count,
                layer: "deep_space"
            }
        }
    });
}

// ============================================================================
// POST-PROCESSING
// ============================================================================

fn setup_bloom_pass(intensity, threshold) {
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 100 },
        op: "create",
        archetype: "PostProcess",
        components: {
            "BloomEffect": {
                intensity: intensity,
                threshold: threshold,
                blur_passes: 5,
                blur_scale: 1.0
            },
            "Material": {
                shader: "assets/shaders/bloom.wgsl"
            },
            "Renderable": {
                mesh: "fullscreen_quad",
                layer: "bloom"
            }
        }
    });
}

fn update_effects(time) {
    // Pulsing bloom based on time
    let pulse = 1.0 + sin(time * 0.5) * 0.1;

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 100 },
        component: "BloomEffect",
        op: "update",
        fields: {
            intensity: settings.bloom_intensity * pulse
        }
    });
}

// ============================================================================
// RENDERING
// ============================================================================

fn render_galaxy(galaxy, camera, time) {
    // Update camera uniform
    emit_patch({
        type: "component",
        entity: galaxy.entity,
        component: "Material",
        op: "update",
        fields: {
            uniforms: {
                view_matrix: camera.view_matrix,
                projection_matrix: camera.projection_matrix,
                time: time,
                core_brightness: settings.core_brightness
            }
        }
    });
}

fn render_ui() {
    // Simple stats display
    let fps = get_fps();
    let particle_count = settings.particle_count;

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 200 },
        component: "UIText",
        op: "set",
        data: {
            text: "NEBULA GENESIS\nStars: " + str(particle_count) + "\nFPS: " + str(floor(fps)),
            position: [20, 20],
            font_size: 16,
            color: [1.0, 1.0, 1.0, 0.8],
            layer: "ui"
        }
    });
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

let TAU = 6.283185307179586;
let PI = 3.141592653589793;

fn lerp(a, b, t) {
    return a + (b - a) * t;
}

fn lerp_color(c1, c2, t) {
    return [
        lerp(c1[0], c2[0], t),
        lerp(c1[1], c2[1], t),
        lerp(c1[2], c2[2], t),
        lerp(c1[3], c2[3], t)
    ];
}

fn hsv_to_rgb(h, s, v) {
    let c = v * s;
    let x = c * (1.0 - abs(fmod(h / 60.0, 2.0) - 1.0));
    let m = v - c;

    let r = 0.0;
    let g = 0.0;
    let b = 0.0;

    if h < 60.0 {
        r = c; g = x; b = 0.0;
    } else if h < 120.0 {
        r = x; g = c; b = 0.0;
    } else if h < 180.0 {
        r = 0.0; g = c; b = x;
    } else if h < 240.0 {
        r = 0.0; g = x; b = c;
    } else if h < 300.0 {
        r = x; g = 0.0; b = c;
    } else {
        r = c; g = 0.0; b = x;
    }

    return [r + m, g + m, b + m, 1.0];
}

fn cleanup_galaxy(galaxy) {
    emit_patch({
        type: "entity",
        entity: galaxy.entity,
        op: "destroy"
    });
}
