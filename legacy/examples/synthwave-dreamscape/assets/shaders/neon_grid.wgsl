// ============================================================================
// NEON GRID SHADER
// ============================================================================
// The iconic synthwave infinite perspective grid with:
// - Dual-color grid lines
// - Perspective-correct scrolling
// - Glow effect with falloff
// - Distance fog
// - Beat-reactive intensity
// ============================================================================

struct GridUniforms {
    primary_color: vec4<f32>,
    secondary_color: vec4<f32>,
    line_width: f32,
    glow_intensity: f32,
    grid_scale: f32,
    fade_distance: f32,
    scroll_offset: f32,
    time: f32,
}

@group(0) @binding(0) var<uniform> grid: GridUniforms;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) uv: vec2<f32>,
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_uv: vec2<f32>,
    @location(1) world_pos: vec3<f32>,
}

// ============================================================================
// VERTEX SHADER
// ============================================================================

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    // Apply perspective transformation for infinite grid look
    let z = input.position.y * 50.0 + 1.0;  // Depth
    let x = input.position.x * z * 0.5;      // Wider at distance
    let y = -0.3;                             // Ground plane

    output.world_pos = vec3<f32>(x, y, z);
    output.world_uv = vec2<f32>(input.uv.x * z, input.uv.y * z + grid.scroll_offset);

    // Project to screen
    let fov = 1.5;
    let aspect = 16.0 / 9.0;
    output.clip_position = vec4<f32>(
        x / (z * aspect * fov),
        (y + 0.5) / (z * fov) + 0.2,  // Offset to put horizon in right place
        z / 100.0,
        1.0
    );

    return output;
}

// ============================================================================
// GRID FUNCTIONS
// ============================================================================

fn grid_line(coord: f32, width: f32) -> f32 {
    let grid_coord = fract(coord);
    let line = smoothstep(width, 0.0, abs(grid_coord - 0.5) - 0.5 + width);
    return line;
}

fn grid_pattern(uv: vec2<f32>, scale: f32, width: f32) -> vec2<f32> {
    let scaled_uv = uv * scale;

    // Primary grid (larger squares)
    let primary_x = grid_line(scaled_uv.x, width * 2.0);
    let primary_y = grid_line(scaled_uv.y, width * 2.0);
    let primary = max(primary_x, primary_y);

    // Secondary grid (subdivisions)
    let sub_scale = 4.0;
    let secondary_x = grid_line(scaled_uv.x * sub_scale, width);
    let secondary_y = grid_line(scaled_uv.y * sub_scale, width);
    let secondary = max(secondary_x, secondary_y) * 0.3;

    return vec2<f32>(primary, secondary);
}

// ============================================================================
// FRAGMENT SHADER
// ============================================================================

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let uv = input.world_uv;
    let distance = input.world_pos.z;

    // Calculate grid pattern
    let pattern = grid_pattern(uv, grid.grid_scale, grid.line_width);
    let primary_line = pattern.x;
    let secondary_line = pattern.y;

    // Color mixing
    let primary_color = grid.primary_color.rgb * primary_line;
    let secondary_color = grid.secondary_color.rgb * secondary_line;
    var line_color = primary_color + secondary_color * 0.5;

    // Glow effect
    let glow = (primary_line + secondary_line * 0.3) * grid.glow_intensity;
    line_color += line_color * glow * 0.5;

    // Distance fade
    let fade = exp(-distance * 0.02);
    let horizon_fade = smoothstep(grid.fade_distance, grid.fade_distance * 0.5, distance);

    // Combine
    let intensity = (primary_line + secondary_line) * fade * horizon_fade;

    // Add subtle scanline effect
    let scanline = sin(input.clip_position.y * 2.0) * 0.1 + 0.9;
    line_color *= scanline;

    // Final color with HDR for bloom
    let final_color = line_color * 1.5;
    let alpha = intensity * grid.primary_color.a;

    if alpha < 0.01 {
        discard;
    }

    return vec4<f32>(final_color, alpha);
}
