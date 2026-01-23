// ============================================================================
// ORBIT CAMERA CONTROLLER
// ============================================================================
// Smooth orbit camera with:
// - Mouse drag rotation
// - Scroll wheel zoom
// - Momentum and damping
// - Pitch limits
// ============================================================================

fn create_orbit_camera(config) {
    return {
        distance: config.distance,
        min_distance: config.min_distance,
        max_distance: config.max_distance,
        target: config.target,
        pitch: config.pitch,
        yaw: config.yaw,
        fov: config.fov,
        smooth_factor: config.smooth_factor,

        // Velocity for smooth movement
        pitch_velocity: 0.0,
        yaw_velocity: 0.0,
        zoom_velocity: 0.0,

        // Current interpolated values
        current_pitch: config.pitch,
        current_yaw: config.yaw,
        current_distance: config.distance,

        // Matrices
        view_matrix: identity_matrix(),
        projection_matrix: identity_matrix(),

        // State
        is_dragging: false,
        last_mouse_x: 0.0,
        last_mouse_y: 0.0
    };
}

fn update_camera(camera, dt) {
    let damping = 0.92;
    let sensitivity = 0.003;

    // Apply velocity damping
    camera.pitch_velocity *= damping;
    camera.yaw_velocity *= damping;
    camera.zoom_velocity *= damping;

    // Update target values
    camera.pitch += camera.pitch_velocity * dt * 60.0;
    camera.yaw += camera.yaw_velocity * dt * 60.0;
    camera.distance += camera.zoom_velocity * dt * 60.0;

    // Clamp pitch to avoid gimbal lock
    camera.pitch = clamp(camera.pitch, -1.4, 1.4);

    // Clamp distance
    camera.distance = clamp(camera.distance, camera.min_distance, camera.max_distance);

    // Smooth interpolation
    let lerp_speed = 1.0 - pow(1.0 - camera.smooth_factor, dt * 60.0);
    camera.current_pitch = lerp(camera.current_pitch, camera.pitch, lerp_speed);
    camera.current_yaw = lerp(camera.current_yaw, camera.yaw, lerp_speed);
    camera.current_distance = lerp(camera.current_distance, camera.distance, lerp_speed);

    // Calculate camera position from spherical coordinates
    let cos_pitch = cos(camera.current_pitch);
    let sin_pitch = sin(camera.current_pitch);
    let cos_yaw = cos(camera.current_yaw);
    let sin_yaw = sin(camera.current_yaw);

    let camera_x = camera.target[0] + camera.current_distance * cos_pitch * sin_yaw;
    let camera_y = camera.target[1] + camera.current_distance * sin_pitch;
    let camera_z = camera.target[2] + camera.current_distance * cos_pitch * cos_yaw;

    // Build view matrix (look-at)
    camera.view_matrix = look_at(
        [camera_x, camera_y, camera_z],
        camera.target,
        [0.0, 1.0, 0.0]
    );

    // Build projection matrix
    let aspect = get_viewport_aspect();
    camera.projection_matrix = perspective(
        radians(camera.fov),
        aspect,
        0.1,
        1000.0
    );
}

fn camera_handle_mouse_drag(camera, dx, dy) {
    let sensitivity = 0.005;
    camera.yaw_velocity += dx * sensitivity;
    camera.pitch_velocity -= dy * sensitivity;
}

fn camera_handle_scroll(camera, delta) {
    let zoom_speed = 0.1;
    camera.zoom_velocity += delta * camera.current_distance * zoom_speed;
}

// ============================================================================
// MATRIX UTILITIES
// ============================================================================

fn identity_matrix() {
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0]
    ];
}

fn look_at(eye, target, up) {
    let f = normalize3(sub3(target, eye));
    let r = normalize3(cross3(f, up));
    let u = cross3(r, f);

    return [
        [r[0], u[0], -f[0], 0.0],
        [r[1], u[1], -f[1], 0.0],
        [r[2], u[2], -f[2], 0.0],
        [-dot3(r, eye), -dot3(u, eye), dot3(f, eye), 1.0]
    ];
}

fn perspective(fov_radians, aspect, near, far) {
    let f = 1.0 / tan(fov_radians / 2.0);

    return [
        [f / aspect, 0.0, 0.0, 0.0],
        [0.0, f, 0.0, 0.0],
        [0.0, 0.0, (far + near) / (near - far), -1.0],
        [0.0, 0.0, (2.0 * far * near) / (near - far), 0.0]
    ];
}

fn normalize3(v) {
    let len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    return [v[0]/len, v[1]/len, v[2]/len];
}

fn sub3(a, b) {
    return [a[0]-b[0], a[1]-b[1], a[2]-b[2]];
}

fn cross3(a, b) {
    return [
        a[1]*b[2] - a[2]*b[1],
        a[2]*b[0] - a[0]*b[2],
        a[0]*b[1] - a[1]*b[0]
    ];
}

fn dot3(a, b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

fn radians(degrees) {
    return degrees * 3.141592653589793 / 180.0;
}
