// ============================================================================
// PORTAL DIMENSIONS - Recursive Reality Warping
// ============================================================================
// This demo showcases:
// - Recursive portal rendering (portals within portals)
// - Reality distortion shaders
// - Dimension-specific color grading
// - Particle effects at portal boundaries
// - Interactive portal placement
// - Impossible geometry illusions
// ============================================================================

import "systems/portal_system.vs";
import "systems/dimension_manager.vs";
import "systems/effects_controller.vs";

// ============================================================================
// GLOBAL STATE
// ============================================================================

let portals = [];
let dimensions = [];
let current_dimension = 0;
let player = null;
let time = 0.0;

let settings = {
    portal_recursion_depth: 4,
    distortion_strength: 0.3,
    chromatic_intensity: 0.15,
    particle_density: 100,
    dimension_transition_speed: 2.0
};

// ============================================================================
// LIFECYCLE
// ============================================================================

fn on_init() {
    print("=== PORTAL DIMENSIONS ===");
    print("Initializing dimensional fabric...");

    // Create dimensions with unique visual styles
    dimensions = [
        create_dimension("Reality", {
            sky_color: [0.1, 0.1, 0.15, 1.0],
            ambient_color: [0.3, 0.3, 0.4],
            gravity: [0.0, -9.8, 0.0],
            geometry_style: "normal"
        }),
        create_dimension("Void", {
            sky_color: [0.0, 0.0, 0.02, 1.0],
            ambient_color: [0.1, 0.0, 0.2],
            gravity: [0.0, -2.0, 0.0],
            geometry_style: "inverted"
        }),
        create_dimension("Inferno", {
            sky_color: [0.2, 0.05, 0.0, 1.0],
            ambient_color: [1.0, 0.3, 0.1],
            gravity: [0.0, -15.0, 0.0],
            geometry_style: "fractured"
        }),
        create_dimension("Ethereal", {
            sky_color: [0.05, 0.1, 0.2, 1.0],
            ambient_color: [0.4, 0.6, 1.0],
            gravity: [0.0, 0.0, 0.0],
            geometry_style: "floating"
        })
    ];

    // Create player camera
    player = create_player({
        position: [0.0, 1.6, 5.0],
        rotation: [0.0, 0.0, 0.0],
        move_speed: 5.0,
        look_sensitivity: 0.002
    });

    // Create initial portal pair
    create_portal_pair(
        { position: [-3.0, 1.5, 0.0], rotation: [0.0, 0.3, 0.0], dimension: 0 },
        { position: [3.0, 1.5, 0.0], rotation: [0.0, -0.3, 0.0], dimension: 1 }
    );

    // Create world geometry
    create_world_geometry();

    // Setup effects pipeline
    setup_effects();

    print("Dimensional fabric initialized");
    print("Controls: WASD move, Mouse look, E to interact, Q to spawn portal");
}

fn on_update(dt) {
    time += dt;

    // Update player
    update_player(player, dt);

    // Update all portals
    for portal in portals {
        update_portal(portal, time, dt);
    }

    // Check portal collisions
    check_portal_transitions(player, portals);

    // Update dimension-specific effects
    update_dimension_effects(dimensions[current_dimension], time);

    // Update visual effects
    update_effects(time);

    // Render
    render_world(current_dimension);
    render_portals(portals, player, settings.portal_recursion_depth);
    render_ui();
}

fn on_shutdown() {
    print("Collapsing dimensional fabric...");
    for portal in portals {
        destroy_portal(portal);
    }
}

// ============================================================================
// PORTAL CREATION
// ============================================================================

fn create_portal_pair(config_a, config_b) {
    let portal_a = create_portal(config_a, len(portals));
    let portal_b = create_portal(config_b, len(portals) + 1);

    // Link portals
    portal_a.linked_portal = portal_b;
    portal_b.linked_portal = portal_a;

    portals.push(portal_a);
    portals.push(portal_b);

    return [portal_a, portal_b];
}

fn create_portal(config, id) {
    let portal = {
        id: id,
        position: config.position,
        rotation: config.rotation,
        dimension: config.dimension,
        linked_portal: null,
        entity: null,
        particles: [],
        active: true,
        animation_phase: random(0.0, 6.28)
    };

    // Create portal entity
    let entity_id = 100 + id;
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: entity_id },
        op: "create",
        archetype: "Portal",
        components: {
            "Transform": {
                position: config.position,
                rotation: euler_to_quat(config.rotation),
                scale: [2.0, 3.0, 0.1]
            },
            "Portal": {
                target_dimension: config.dimension,
                recursion_depth: settings.portal_recursion_depth,
                distortion: settings.distortion_strength
            },
            "Material": {
                shader: "assets/shaders/portal_surface.wgsl",
                blend_mode: "normal",
                double_sided: true
            },
            "Renderable": {
                mesh: "portal_frame",
                layer: "portal_a"
            }
        }
    });

    portal.entity = { namespace: get_namespace(), local_id: entity_id };

    // Spawn portal edge particles
    spawn_portal_particles(portal);

    return portal;
}

fn spawn_portal_particles(portal) {
    let count = settings.particle_density;

    for i in range(0, count) {
        // Particles along portal edge
        let angle = (i / count) * TAU;
        let edge_x = cos(angle) * 1.0;
        let edge_y = sin(angle) * 1.5;

        portal.particles.push({
            local_offset: [edge_x, edge_y, 0.0],
            phase: random(0.0, TAU),
            speed: random(0.5, 2.0),
            size: random(0.02, 0.08),
            color: get_dimension_color(portal.dimension)
        });
    }
}

fn get_dimension_color(dimension_id) {
    if dimension_id == 0 {
        return [0.3, 0.5, 1.0, 1.0];  // Blue - Reality
    } else if dimension_id == 1 {
        return [0.5, 0.0, 1.0, 1.0];  // Purple - Void
    } else if dimension_id == 2 {
        return [1.0, 0.3, 0.0, 1.0];  // Orange - Inferno
    } else {
        return [0.0, 1.0, 0.8, 1.0];  // Cyan - Ethereal
    }
}

// ============================================================================
// PORTAL UPDATE
// ============================================================================

fn update_portal(portal, time, dt) {
    portal.animation_phase += dt * 2.0;

    // Update portal surface shader uniforms
    let pulse = sin(portal.animation_phase) * 0.5 + 0.5;
    let swirl = time * 0.5;

    emit_patch({
        type: "component",
        entity: portal.entity,
        component: "Material",
        op: "update",
        fields: {
            uniforms: {
                time: time,
                pulse: pulse,
                swirl_angle: swirl,
                dimension_color: get_dimension_color(portal.dimension),
                distortion: settings.distortion_strength * (0.8 + pulse * 0.4)
            }
        }
    });

    // Update particles
    update_portal_particles(portal, time);
}

fn update_portal_particles(portal, time) {
    let particle_positions = [];
    let particle_colors = [];
    let particle_sizes = [];

    for p in portal.particles {
        // Orbit around portal edge
        let orbit_angle = p.phase + time * p.speed;
        let orbit_radius = 0.1 + sin(time * 3.0 + p.phase) * 0.05;

        let x = p.local_offset[0] + cos(orbit_angle) * orbit_radius;
        let y = p.local_offset[1] + sin(orbit_angle) * orbit_radius;
        let z = sin(time * 2.0 + p.phase) * 0.2;

        // Transform to world space
        let world_pos = transform_point(
            [x, y, z],
            portal.position,
            portal.rotation
        );

        particle_positions.push(world_pos);

        // Pulsing color
        let color = p.color;
        let brightness = 0.5 + sin(time * 4.0 + p.phase) * 0.5;
        particle_colors.push([
            color[0] * brightness,
            color[1] * brightness,
            color[2] * brightness,
            color[3]
        ]);

        particle_sizes.push(p.size * (0.8 + brightness * 0.4));
    }

    // Update particle entity
    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 200 + portal.id },
        component: "ParticleBuffer",
        op: "set",
        data: {
            positions: particle_positions,
            colors: particle_colors,
            sizes: particle_sizes,
            count: len(particle_positions)
        }
    });
}

// ============================================================================
// PORTAL TRANSITIONS
// ============================================================================

fn check_portal_transitions(player, portals) {
    for portal in portals {
        if !portal.active || portal.linked_portal == null {
            continue;
        }

        // Check if player is crossing portal plane
        let to_portal = sub3(player.position, portal.position);
        let portal_normal = get_portal_normal(portal);
        let distance = dot3(to_portal, portal_normal);

        // Check if within portal bounds
        let local_pos = world_to_local(player.position, portal.position, portal.rotation);

        if abs(distance) < 0.5 && abs(local_pos[0]) < 1.0 && abs(local_pos[1]) < 1.5 {
            // Check crossing direction
            if player.last_portal_distance * distance < 0 {
                // Crossed the plane! Teleport!
                teleport_through_portal(player, portal);
            }
        }

        player.last_portal_distance = distance;
    }
}

fn teleport_through_portal(player, source_portal) {
    let target = source_portal.linked_portal;

    print("Transitioning to " + dimensions[target.dimension].name + "...");

    // Calculate relative position/rotation
    let local_pos = world_to_local(player.position, source_portal.position, source_portal.rotation);
    let local_vel = world_to_local_dir(player.velocity, source_portal.rotation);

    // Mirror through portal
    local_pos[2] = -local_pos[2];
    local_vel[2] = -local_vel[2];

    // Transform to target portal space
    let new_pos = local_to_world(local_pos, target.position, target.rotation);
    let new_vel = local_to_world_dir(local_vel, target.rotation);

    // Update player
    player.position = new_pos;
    player.velocity = new_vel;

    // Change dimension
    let old_dimension = current_dimension;
    current_dimension = target.dimension;

    // Trigger dimension transition effect
    trigger_dimension_transition(old_dimension, current_dimension);

    // Play transition sound (if audio enabled)
    // play_sound("portal_whoosh");
}

fn trigger_dimension_transition(from, to) {
    // Flash effect
    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 500 },
        component: "FlashEffect",
        op: "set",
        data: {
            color: get_dimension_color(to),
            intensity: 1.0,
            duration: 0.3
        }
    });

    // Update world lighting
    let dim = dimensions[to];
    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 1 },
        component: "Environment",
        op: "update",
        fields: {
            sky_color: dim.config.sky_color,
            ambient_color: dim.config.ambient_color
        }
    });
}

// ============================================================================
// WORLD GEOMETRY
// ============================================================================

fn create_world_geometry() {
    // Floor
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 1 },
        op: "create",
        archetype: "StaticMesh",
        components: {
            "Transform": {
                position: [0.0, 0.0, 0.0],
                rotation: [0.0, 0.0, 0.0, 1.0],
                scale: [50.0, 1.0, 50.0]
            },
            "Material": {
                shader: "assets/shaders/grid_floor.wgsl",
                color: [0.2, 0.2, 0.25, 1.0]
            },
            "Renderable": {
                mesh: "cube",
                layer: "world"
            },
            "Environment": {
                sky_color: [0.1, 0.1, 0.15, 1.0],
                ambient_color: [0.3, 0.3, 0.4]
            }
        }
    });

    // Create floating cubes in various positions
    let cube_positions = [
        [-5.0, 2.0, -5.0],
        [5.0, 3.0, -8.0],
        [-8.0, 1.5, 3.0],
        [0.0, 4.0, -10.0],
        [7.0, 2.5, 2.0]
    ];

    for i in range(0, len(cube_positions)) {
        let pos = cube_positions[i];
        emit_patch({
            type: "entity",
            entity: { namespace: get_namespace(), local_id: 10 + i },
            op: "create",
            archetype: "StaticMesh",
            components: {
                "Transform": {
                    position: pos,
                    rotation: [0.0, 0.0, 0.0, 1.0],
                    scale: [1.0, 1.0, 1.0]
                },
                "Material": {
                    shader: "assets/shaders/holographic.wgsl",
                    color: [0.5, 0.7, 1.0, 0.8]
                },
                "Renderable": {
                    mesh: "cube",
                    layer: "world"
                },
                "Animator": {
                    rotation_speed: [0.2 + i * 0.1, 0.3 + i * 0.05, 0.1],
                    bob_amplitude: 0.2,
                    bob_speed: 1.0 + i * 0.2
                }
            }
        });
    }
}

// ============================================================================
// EFFECTS
// ============================================================================

fn setup_effects() {
    // Distortion effect
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 300 },
        op: "create",
        archetype: "PostProcess",
        components: {
            "DistortionEffect": {
                strength: settings.distortion_strength,
                frequency: 10.0
            },
            "Material": {
                shader: "assets/shaders/distortion.wgsl"
            },
            "Renderable": {
                mesh: "fullscreen_quad",
                layer: "distortion"
            }
        }
    });

    // Chromatic aberration
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 301 },
        op: "create",
        archetype: "PostProcess",
        components: {
            "ChromaticAberration": {
                intensity: settings.chromatic_intensity
            },
            "Material": {
                shader: "assets/shaders/chromatic.wgsl"
            },
            "Renderable": {
                mesh: "fullscreen_quad",
                layer: "chromatic"
            }
        }
    });
}

fn update_effects(time) {
    // Pulsing distortion near portals
    let near_portal = false;
    let min_dist = 1000.0;

    for portal in portals {
        let dist = length3(sub3(player.position, portal.position));
        if dist < min_dist {
            min_dist = dist;
        }
        if dist < 3.0 {
            near_portal = true;
        }
    }

    let distortion_mult = 1.0;
    if near_portal {
        distortion_mult = 1.5 + sin(time * 5.0) * 0.5;
    }

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 300 },
        component: "DistortionEffect",
        op: "update",
        fields: {
            strength: settings.distortion_strength * distortion_mult,
            time: time
        }
    });

    // Chromatic aberration increases when moving fast
    let speed = length3(player.velocity);
    let chromatic_mult = 1.0 + speed * 0.1;

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 301 },
        component: "ChromaticAberration",
        op: "update",
        fields: {
            intensity: settings.chromatic_intensity * chromatic_mult
        }
    });
}

// ============================================================================
// RENDERING
// ============================================================================

fn render_portals(portals, camera, max_recursion) {
    for portal in portals {
        if !portal.active || portal.linked_portal == null {
            continue;
        }

        render_portal_recursive(portal, camera, 0, max_recursion);
    }
}

fn render_portal_recursive(portal, camera, depth, max_depth) {
    if depth >= max_depth {
        return;
    }

    let target = portal.linked_portal;

    // Calculate virtual camera position through portal
    let virtual_camera = calculate_virtual_camera(camera, portal, target);

    // Render what's visible through the portal
    // (In actual implementation, this would render to a texture)
    emit_patch({
        type: "component",
        entity: portal.entity,
        component: "Portal",
        op: "update",
        fields: {
            virtual_camera: virtual_camera,
            recursion_level: depth,
            render_target: "portal_rt_" + str(depth)
        }
    });

    // Recursively render portals visible through this portal
    for other_portal in portals {
        if other_portal.id != portal.id && is_portal_visible(other_portal, virtual_camera) {
            render_portal_recursive(other_portal, virtual_camera, depth + 1, max_depth);
        }
    }
}

fn render_ui() {
    let dim = dimensions[current_dimension];

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 400 },
        component: "UIText",
        op: "set",
        data: {
            text: "DIMENSION: " + dim.name + "\nPortals: " + str(len(portals)) + "\nQ - Spawn Portal | E - Interact",
            position: [20, 20],
            font_size: 18,
            color: get_dimension_color(current_dimension),
            layer: "ui"
        }
    });
}

// ============================================================================
// UTILITIES
// ============================================================================

let TAU = 6.283185307179586;

fn transform_point(point, position, rotation) {
    // Simplified rotation (should use quaternion)
    let c = cos(rotation[1]);
    let s = sin(rotation[1]);
    let rx = point[0] * c - point[2] * s;
    let rz = point[0] * s + point[2] * c;

    return [
        position[0] + rx,
        position[1] + point[1],
        position[2] + rz
    ];
}

fn sub3(a, b) {
    return [a[0]-b[0], a[1]-b[1], a[2]-b[2]];
}

fn dot3(a, b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

fn length3(v) {
    return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

fn euler_to_quat(euler) {
    // Simplified Y-axis rotation only
    let half_y = euler[1] * 0.5;
    return [0.0, sin(half_y), 0.0, cos(half_y)];
}

fn get_portal_normal(portal) {
    let c = cos(portal.rotation[1]);
    let s = sin(portal.rotation[1]);
    return [s, 0.0, c];
}
