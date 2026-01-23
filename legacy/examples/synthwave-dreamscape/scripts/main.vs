// ============================================================================
// SYNTHWAVE DREAMSCAPE - Retro-Futuristic Visual Experience
// ============================================================================
// This demo showcases:
// - Procedural infinite grid with perspective
// - Animated neon sun with scanlines
// - Procedural mountain silhouettes
// - Reactive particle systems
// - CRT/VHS post-processing effects
// - Real-time shader parameter control
// - Audio-reactive elements (simulated)
// ============================================================================

import "systems/grid_system.vs";
import "systems/sun_system.vs";
import "systems/mountain_system.vs";
import "systems/particle_effects.vs";

// ============================================================================
// GLOBAL STATE
// ============================================================================

let time = 0.0;
let beat_time = 0.0;
let speed = 1.0;

// Color palette (classic synthwave)
let colors = {
    sky_top: [0.02, 0.0, 0.08, 1.0],      // Deep purple
    sky_bottom: [0.15, 0.0, 0.1, 1.0],     // Magenta horizon
    sun_top: [1.0, 0.9, 0.3, 1.0],         // Yellow
    sun_bottom: [1.0, 0.2, 0.4, 1.0],      // Hot pink
    grid_primary: [1.0, 0.0, 0.8, 1.0],    // Magenta
    grid_secondary: [0.0, 1.0, 1.0, 1.0],  // Cyan
    mountain_fill: [0.0, 0.0, 0.02, 1.0],  // Near black
    mountain_edge: [1.0, 0.0, 0.5, 1.0],   // Pink edge
    particles: [0.0, 1.0, 0.8, 1.0]        // Cyan particles
};

let settings = {
    grid_speed: 2.0,
    grid_line_width: 0.02,
    grid_glow_intensity: 2.0,
    sun_size: 0.3,
    sun_scanline_count: 20,
    mountain_layers: 3,
    particle_count: 500,
    beat_intensity: 0.0,  // Simulated audio reactivity
    crt_curve: 0.03,
    scanline_intensity: 0.15,
    chromatic_amount: 0.002
};

// ============================================================================
// LIFECYCLE
// ============================================================================

fn on_init() {
    print("=== SYNTHWAVE DREAMSCAPE ===");
    print("Initializing retro-future reality...");

    // Create sky gradient
    create_sky();

    // Create the iconic synthwave sun
    create_sun();

    // Create layered mountain silhouettes
    create_mountains();

    // Create infinite perspective grid
    create_grid();

    // Create floating particles
    create_particles();

    // Setup post-processing
    setup_post_processing();

    print("Reality initialized. Welcome to 1985.");
    print("Controls: Arrow keys adjust speed, 1-5 change color themes");
}

fn on_update(dt) {
    time += dt * speed;

    // Simulate beat (4/4 at 120 BPM)
    let bpm = 120.0;
    let beat_period = 60.0 / bpm;
    beat_time += dt;
    if beat_time >= beat_period {
        beat_time -= beat_period;
        on_beat();
    }

    // Beat intensity decay
    settings.beat_intensity *= 0.95;

    // Update all systems
    update_sky(time);
    update_sun(time, settings.beat_intensity);
    update_mountains(time);
    update_grid(time, settings.grid_speed);
    update_particles(time, dt, settings.beat_intensity);
    update_post_processing(time);

    // Handle input
    handle_input();

    render_ui();
}

fn on_beat() {
    settings.beat_intensity = 1.0;

    // Spawn burst of particles on beat
    spawn_particle_burst(50);
}

fn on_shutdown() {
    print("Returning to the future...");
}

// ============================================================================
// SKY
// ============================================================================

fn create_sky() {
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 1 },
        op: "create",
        archetype: "FullscreenQuad",
        components: {
            "Transform": {
                position: [0.0, 0.0, 0.0],
                rotation: [0.0, 0.0, 0.0, 1.0],
                scale: [1.0, 1.0, 1.0]
            },
            "Material": {
                shader: "assets/shaders/synthwave_sky.wgsl",
                uniforms: {
                    color_top: colors.sky_top,
                    color_bottom: colors.sky_bottom,
                    star_density: 0.001,
                    star_twinkle_speed: 2.0
                }
            },
            "Renderable": {
                mesh: "fullscreen_quad",
                layer: "sky"
            }
        }
    });
}

fn update_sky(time) {
    // Subtle color shift
    let shift = sin(time * 0.1) * 0.02;

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 1 },
        component: "Material",
        op: "update",
        fields: {
            uniforms: {
                time: time,
                color_shift: shift
            }
        }
    });
}

// ============================================================================
// SUN
// ============================================================================

fn create_sun() {
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 10 },
        op: "create",
        archetype: "SynthwaveSun",
        components: {
            "Transform": {
                position: [0.0, 0.2, -100.0],
                rotation: [0.0, 0.0, 0.0, 1.0],
                scale: [settings.sun_size, settings.sun_size, 1.0]
            },
            "Material": {
                shader: "assets/shaders/synthwave_sun.wgsl",
                uniforms: {
                    color_top: colors.sun_top,
                    color_bottom: colors.sun_bottom,
                    scanline_count: settings.sun_scanline_count,
                    glow_intensity: 1.5
                },
                blend_mode: "additive"
            },
            "Renderable": {
                mesh: "circle",
                layer: "sun"
            }
        }
    });
}

fn update_sun(time, beat) {
    // Pulsing glow on beat
    let pulse = 1.0 + beat * 0.3;
    let size = settings.sun_size * pulse;

    // Animated scanlines
    let scanline_offset = time * 0.5;

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 10 },
        component: "Transform",
        op: "update",
        fields: {
            scale: [size, size, 1.0]
        }
    });

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 10 },
        component: "Material",
        op: "update",
        fields: {
            uniforms: {
                time: time,
                scanline_offset: scanline_offset,
                glow_intensity: 1.5 + beat * 0.5
            }
        }
    });
}

// ============================================================================
// MOUNTAINS
// ============================================================================

fn create_mountains() {
    for i in range(0, settings.mountain_layers) {
        let depth = i + 1;
        let height_scale = 0.3 - i * 0.05;
        let y_offset = -0.1 - i * 0.02;

        emit_patch({
            type: "entity",
            entity: { namespace: get_namespace(), local_id: 20 + i },
            op: "create",
            archetype: "MountainLayer",
            components: {
                "Transform": {
                    position: [0.0, y_offset, -50.0 - depth * 20.0],
                    rotation: [0.0, 0.0, 0.0, 1.0],
                    scale: [1.0, height_scale, 1.0]
                },
                "Material": {
                    shader: "assets/shaders/mountain_silhouette.wgsl",
                    uniforms: {
                        fill_color: colors.mountain_fill,
                        edge_color: colors.mountain_edge,
                        edge_glow: 0.5 - i * 0.1,
                        noise_seed: random(0.0, 100.0),
                        noise_scale: 2.0 + i * 0.5
                    }
                },
                "Renderable": {
                    mesh: "mountain_strip",
                    layer: "mountains"
                }
            }
        });
    }
}

fn update_mountains(time) {
    // Subtle parallax movement
    for i in range(0, settings.mountain_layers) {
        let speed = 0.01 * (settings.mountain_layers - i);
        let offset = time * speed;

        emit_patch({
            type: "component",
            entity: { namespace: get_namespace(), local_id: 20 + i },
            component: "Material",
            op: "update",
            fields: {
                uniforms: {
                    time: time,
                    scroll_offset: offset
                }
            }
        });
    }
}

// ============================================================================
// GRID
// ============================================================================

fn create_grid() {
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 50 },
        op: "create",
        archetype: "InfiniteGrid",
        components: {
            "Transform": {
                position: [0.0, -0.3, 0.0],
                rotation: [-0.7, 0.0, 0.0, 0.7],  // Tilted toward horizon
                scale: [100.0, 100.0, 1.0]
            },
            "Material": {
                shader: "assets/shaders/neon_grid.wgsl",
                uniforms: {
                    primary_color: colors.grid_primary,
                    secondary_color: colors.grid_secondary,
                    line_width: settings.grid_line_width,
                    glow_intensity: settings.grid_glow_intensity,
                    grid_scale: 1.0,
                    fade_distance: 50.0
                },
                blend_mode: "additive"
            },
            "Renderable": {
                mesh: "plane",
                layer: "grid"
            }
        }
    });
}

fn update_grid(time, speed) {
    // Scrolling grid
    let scroll = time * speed;

    // Beat reactive glow
    let glow = settings.grid_glow_intensity * (1.0 + settings.beat_intensity * 0.5);

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 50 },
        component: "Material",
        op: "update",
        fields: {
            uniforms: {
                time: time,
                scroll_offset: scroll,
                glow_intensity: glow
            }
        }
    });
}

// ============================================================================
// PARTICLES
// ============================================================================

let particles = [];

fn create_particles() {
    for i in range(0, settings.particle_count) {
        particles.push({
            position: [
                random(-20.0, 20.0),
                random(-2.0, 5.0),
                random(-50.0, 10.0)
            ],
            velocity: [0.0, random(0.5, 2.0), random(1.0, 3.0)],
            size: random(0.02, 0.1),
            phase: random(0.0, 6.28),
            color: colors.particles
        });
    }

    update_particle_buffer();
}

fn update_particles(time, dt, beat) {
    for p in particles {
        // Move toward camera
        p.position[2] += p.velocity[2] * dt;

        // Float upward
        p.position[1] += p.velocity[1] * dt * 0.1;

        // Gentle side-to-side
        p.position[0] += sin(time * 2.0 + p.phase) * dt * 0.5;

        // Reset if past camera
        if p.position[2] > 10.0 {
            p.position[2] = -50.0;
            p.position[0] = random(-20.0, 20.0);
            p.position[1] = random(-2.0, 5.0);
        }

        // Beat pulse
        p.size = p.size * (1.0 + beat * 0.5);
    }

    update_particle_buffer();
}

fn spawn_particle_burst(count) {
    for i in range(0, count) {
        if len(particles) < settings.particle_count + 200 {
            particles.push({
                position: [
                    random(-5.0, 5.0),
                    random(-0.5, 0.5),
                    random(-5.0, 5.0)
                ],
                velocity: [
                    random(-2.0, 2.0),
                    random(2.0, 5.0),
                    random(-1.0, 1.0)
                ],
                size: random(0.05, 0.15),
                phase: random(0.0, 6.28),
                color: [1.0, 0.0, 0.8, 1.0]  // Magenta burst
            });
        }
    }
}

fn update_particle_buffer() {
    let positions = [];
    let colors_arr = [];
    let sizes = [];

    for p in particles {
        positions.push(p.position);
        colors_arr.push(p.color);
        sizes.push(p.size);
    }

    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 100 },
        component: "ParticleBuffer",
        op: "set",
        data: {
            positions: positions,
            colors: colors_arr,
            sizes: sizes,
            count: len(positions)
        }
    });
}

// ============================================================================
// POST-PROCESSING
// ============================================================================

fn setup_post_processing() {
    // Glow/bloom
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 200 },
        op: "create",
        archetype: "PostProcess",
        components: {
            "BloomEffect": {
                intensity: 1.2,
                threshold: 0.5
            },
            "Material": {
                shader: "assets/shaders/bloom.wgsl"
            },
            "Renderable": {
                mesh: "fullscreen_quad",
                layer: "glow"
            }
        }
    });

    // CRT scanlines
    emit_patch({
        type: "entity",
        entity: { namespace: get_namespace(), local_id: 201 },
        op: "create",
        archetype: "PostProcess",
        components: {
            "CRTEffect": {
                scanline_intensity: settings.scanline_intensity,
                curvature: settings.crt_curve,
                vignette: 0.3
            },
            "Material": {
                shader: "assets/shaders/crt_effect.wgsl"
            },
            "Renderable": {
                mesh: "fullscreen_quad",
                layer: "scanlines"
            }
        }
    });
}

fn update_post_processing(time) {
    // Animated scanlines
    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 201 },
        component: "CRTEffect",
        op: "update",
        fields: {
            time: time,
            scanline_intensity: settings.scanline_intensity * (1.0 + settings.beat_intensity * 0.2)
        }
    });
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

fn handle_input() {
    let keyboard = get_keyboard_state();

    // Speed control
    if keyboard.up_arrow {
        speed = min(speed + 0.02, 3.0);
    }
    if keyboard.down_arrow {
        speed = max(speed - 0.02, 0.1);
    }

    // Color themes
    if keyboard.key_1 {
        set_theme("classic");
    }
    if keyboard.key_2 {
        set_theme("sunset");
    }
    if keyboard.key_3 {
        set_theme("midnight");
    }
    if keyboard.key_4 {
        set_theme("neon");
    }
    if keyboard.key_5 {
        set_theme("vapor");
    }
}

fn set_theme(theme) {
    if theme == "classic" {
        colors.grid_primary = [1.0, 0.0, 0.8, 1.0];
        colors.grid_secondary = [0.0, 1.0, 1.0, 1.0];
        colors.sun_top = [1.0, 0.9, 0.3, 1.0];
        colors.sun_bottom = [1.0, 0.2, 0.4, 1.0];
    } else if theme == "sunset" {
        colors.grid_primary = [1.0, 0.5, 0.0, 1.0];
        colors.grid_secondary = [1.0, 0.8, 0.0, 1.0];
        colors.sun_top = [1.0, 0.9, 0.5, 1.0];
        colors.sun_bottom = [1.0, 0.3, 0.0, 1.0];
    } else if theme == "midnight" {
        colors.grid_primary = [0.0, 0.5, 1.0, 1.0];
        colors.grid_secondary = [0.5, 0.0, 1.0, 1.0];
        colors.sun_top = [0.5, 0.5, 1.0, 1.0];
        colors.sun_bottom = [0.2, 0.0, 0.5, 1.0];
    } else if theme == "neon" {
        colors.grid_primary = [0.0, 1.0, 0.0, 1.0];
        colors.grid_secondary = [1.0, 0.0, 1.0, 1.0];
        colors.sun_top = [0.0, 1.0, 0.5, 1.0];
        colors.sun_bottom = [1.0, 0.0, 0.5, 1.0];
    } else if theme == "vapor" {
        colors.grid_primary = [1.0, 0.4, 0.7, 1.0];
        colors.grid_secondary = [0.4, 0.8, 1.0, 1.0];
        colors.sun_top = [1.0, 0.7, 0.8, 1.0];
        colors.sun_bottom = [0.6, 0.4, 0.8, 1.0];
    }

    print("Theme: " + theme);

    // Update all materials with new colors
    apply_theme_colors();
}

fn apply_theme_colors() {
    // Update grid
    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 50 },
        component: "Material",
        op: "update",
        fields: {
            uniforms: {
                primary_color: colors.grid_primary,
                secondary_color: colors.grid_secondary
            }
        }
    });

    // Update sun
    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 10 },
        component: "Material",
        op: "update",
        fields: {
            uniforms: {
                color_top: colors.sun_top,
                color_bottom: colors.sun_bottom
            }
        }
    });
}

// ============================================================================
// UI
// ============================================================================

fn render_ui() {
    emit_patch({
        type: "component",
        entity: { namespace: get_namespace(), local_id: 300 },
        component: "UIText",
        op: "set",
        data: {
            text: "SYNTHWAVE DREAMSCAPE\nSpeed: " + str(floor(speed * 100)) + "%\n1-5: Themes | Arrows: Speed",
            position: [20, 20],
            font_size: 16,
            color: colors.grid_primary,
            layer: "ui"
        }
    });
}
