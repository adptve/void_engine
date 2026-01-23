// ============================================================================
// Infinite Grid Shader
// ============================================================================
// Renders an infinite ground plane with a grid pattern that fades with distance
// ============================================================================

struct CameraUniforms {
    view_proj: mat4x4<f32>,
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    camera_pos: vec3<f32>,
    _padding: f32,
}

struct GridUniforms {
    line_color: vec3<f32>,
    line_width: f32,
    grid_scale: f32,
    fade_distance: f32,
    _pad1: f32,
    _pad2: f32,
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(1) @binding(0) var<uniform> grid: GridUniforms;

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_pos: vec3<f32>,
    @location(1) near_point: vec3<f32>,
    @location(2) far_point: vec3<f32>,
}

// Fullscreen quad vertices
@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> VertexOutput {
    var output: VertexOutput;

    // Generate fullscreen quad
    let x = f32((idx << 1u) & 2u);
    let y = f32(idx & 2u);
    let ndc = vec2<f32>(x * 2.0 - 1.0, y * 2.0 - 1.0);

    output.clip_position = vec4<f32>(ndc, 0.0, 1.0);

    // Unproject to world space for near and far planes
    let inv_view_proj = inverse_mat4(camera.view_proj);

    let near_h = inv_view_proj * vec4<f32>(ndc, -1.0, 1.0);
    let far_h = inv_view_proj * vec4<f32>(ndc, 1.0, 1.0);

    output.near_point = near_h.xyz / near_h.w;
    output.far_point = far_h.xyz / far_h.w;
    output.world_pos = output.near_point;

    return output;
}

// Simple 4x4 matrix inverse (for view_proj)
fn inverse_mat4(m: mat4x4<f32>) -> mat4x4<f32> {
    let a00 = m[0][0]; let a01 = m[0][1]; let a02 = m[0][2]; let a03 = m[0][3];
    let a10 = m[1][0]; let a11 = m[1][1]; let a12 = m[1][2]; let a13 = m[1][3];
    let a20 = m[2][0]; let a21 = m[2][1]; let a22 = m[2][2]; let a23 = m[2][3];
    let a30 = m[3][0]; let a31 = m[3][1]; let a32 = m[3][2]; let a33 = m[3][3];

    let b00 = a00 * a11 - a01 * a10;
    let b01 = a00 * a12 - a02 * a10;
    let b02 = a00 * a13 - a03 * a10;
    let b03 = a01 * a12 - a02 * a11;
    let b04 = a01 * a13 - a03 * a11;
    let b05 = a02 * a13 - a03 * a12;
    let b06 = a20 * a31 - a21 * a30;
    let b07 = a20 * a32 - a22 * a30;
    let b08 = a20 * a33 - a23 * a30;
    let b09 = a21 * a32 - a22 * a31;
    let b10 = a21 * a33 - a23 * a31;
    let b11 = a22 * a33 - a23 * a32;

    let det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    let inv_det = 1.0 / det;

    return mat4x4<f32>(
        vec4<f32>(
            (a11 * b11 - a12 * b10 + a13 * b09) * inv_det,
            (a02 * b10 - a01 * b11 - a03 * b09) * inv_det,
            (a31 * b05 - a32 * b04 + a33 * b03) * inv_det,
            (a22 * b04 - a21 * b05 - a23 * b03) * inv_det
        ),
        vec4<f32>(
            (a12 * b08 - a10 * b11 - a13 * b07) * inv_det,
            (a00 * b11 - a02 * b08 + a03 * b07) * inv_det,
            (a32 * b02 - a30 * b05 - a33 * b01) * inv_det,
            (a20 * b05 - a22 * b02 + a23 * b01) * inv_det
        ),
        vec4<f32>(
            (a10 * b10 - a11 * b08 + a13 * b06) * inv_det,
            (a01 * b08 - a00 * b10 - a03 * b06) * inv_det,
            (a30 * b04 - a31 * b02 + a33 * b00) * inv_det,
            (a21 * b02 - a20 * b04 - a23 * b00) * inv_det
        ),
        vec4<f32>(
            (a11 * b07 - a10 * b09 - a12 * b06) * inv_det,
            (a00 * b09 - a01 * b07 + a02 * b06) * inv_det,
            (a31 * b01 - a30 * b03 - a32 * b00) * inv_det,
            (a20 * b03 - a21 * b01 + a22 * b00) * inv_det
        )
    );
}

// Grid pattern function
fn grid_pattern(pos: vec2<f32>, scale: f32, line_width: f32) -> f32 {
    let coord = pos * scale;
    let grid_coord = abs(fract(coord - 0.5) - 0.5);
    let line = min(grid_coord.x, grid_coord.y);

    // Anti-aliased line
    let derivative = fwidth(coord);
    let draw_width = max(line_width, 1.0) * 0.5;
    let line_aa = 1.0 - smoothstep(0.0, derivative.x * draw_width + derivative.y * draw_width, line);

    return line_aa;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // Ray from near to far plane
    let ray_dir = input.far_point - input.near_point;

    // Find intersection with Y=0 plane (ground)
    let t = -input.near_point.y / ray_dir.y;

    // Discard if no intersection or behind camera
    if t < 0.0 {
        discard;
    }

    // World position on ground plane
    let world_pos = input.near_point + ray_dir * t;

    // Distance from camera for fading
    let dist = length(world_pos - camera.camera_pos);
    let fade = 1.0 - smoothstep(0.0, grid.fade_distance, dist);

    if fade <= 0.001 {
        discard;
    }

    // Main grid (1 unit)
    let grid1 = grid_pattern(world_pos.xz, grid.grid_scale, grid.line_width);

    // Secondary grid (10 units)
    let grid10 = grid_pattern(world_pos.xz, grid.grid_scale * 0.1, grid.line_width * 2.0);

    // Combine grids
    let grid_alpha = max(grid1 * 0.3, grid10 * 0.6);

    // Axis highlight
    var axis_color = vec3<f32>(0.0);
    let axis_width = 0.05;

    // X axis (red)
    if abs(world_pos.z) < axis_width && world_pos.x > 0.0 {
        axis_color = vec3<f32>(0.8, 0.2, 0.2);
    }
    // Z axis (blue)
    if abs(world_pos.x) < axis_width && world_pos.z > 0.0 {
        axis_color = vec3<f32>(0.2, 0.2, 0.8);
    }

    // Final color
    var color = mix(grid.line_color, axis_color, length(axis_color));
    let alpha = grid_alpha * fade;

    if alpha < 0.01 {
        discard;
    }

    return vec4<f32>(color, alpha);
}
