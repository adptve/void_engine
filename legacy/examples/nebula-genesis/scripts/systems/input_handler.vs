// ============================================================================
// INPUT HANDLER
// ============================================================================
// Handles all user input for the galaxy simulation
// ============================================================================

fn create_input_handler() {
    return {
        mouse_x: 0.0,
        mouse_y: 0.0,
        last_mouse_x: 0.0,
        last_mouse_y: 0.0,
        is_dragging: false,
        keys_pressed: {}
    };
}

fn process_input(input, camera, dt) {
    // Get current input state
    let mouse = get_mouse_state();
    let keyboard = get_keyboard_state();

    input.last_mouse_x = input.mouse_x;
    input.last_mouse_y = input.mouse_y;
    input.mouse_x = mouse.x;
    input.mouse_y = mouse.y;

    // Mouse drag for camera rotation
    if mouse.left_button {
        if !input.is_dragging {
            input.is_dragging = true;
        } else {
            let dx = input.mouse_x - input.last_mouse_x;
            let dy = input.mouse_y - input.last_mouse_y;
            camera_handle_mouse_drag(camera, dx, dy);
        }
    } else {
        input.is_dragging = false;
    }

    // Scroll for zoom
    if mouse.scroll_delta != 0.0 {
        camera_handle_scroll(camera, -mouse.scroll_delta);
    }

    // Keyboard controls
    if keyboard.space && !input.keys_pressed.space {
        // Toggle pause
        paused = !paused;
        if paused {
            print("Simulation PAUSED");
        } else {
            print("Simulation RESUMED");
        }
    }

    if keyboard.key_1 {
        settings.color_palette = "cosmic";
        regenerate_colors();
    }
    if keyboard.key_2 {
        settings.color_palette = "fire";
        regenerate_colors();
    }
    if keyboard.key_3 {
        settings.color_palette = "ice";
        regenerate_colors();
    }
    if keyboard.key_4 {
        settings.color_palette = "rainbow";
        regenerate_colors();
    }

    // Bloom adjustment
    if keyboard.up_arrow {
        settings.bloom_intensity = min(settings.bloom_intensity + 0.1, 5.0);
    }
    if keyboard.down_arrow {
        settings.bloom_intensity = max(settings.bloom_intensity - 0.1, 0.0);
    }

    // Store key states for edge detection
    input.keys_pressed.space = keyboard.space;
}

fn regenerate_colors() {
    print("Switching color palette to: " + settings.color_palette);

    // Update all star colors
    for i in range(0, len(galaxy.particles)) {
        let pos = galaxy.particles[i];
        let r = sqrt(pos[0]*pos[0] + pos[2]*pos[2]);
        let distance_normalized = r / settings.galaxy_radius;
        galaxy.colors[i] = get_star_color(distance_normalized, settings.color_palette);
    }

    // Emit patch to update colors
    emit_patch({
        type: "component",
        entity: galaxy.entity,
        component: "ParticleBuffer",
        op: "update",
        fields: {
            colors: galaxy.colors
        }
    });
}
