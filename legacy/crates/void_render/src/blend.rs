//! Blend mode shaders and utilities
//!
//! This module provides WGSL shader generation for different blend modes
//! used in layer composition.

use crate::layer::BlendMode;
use alloc::format;
use alloc::string::String;

/// Generate a complete WGSL shader for layer composition
pub fn generate_composite_shader(blend_mode: BlendMode, has_opacity: bool) -> String {
    let blend_equation = blend_mode.wgsl_code();

    format!(
        r#"
// Vertex shader
struct VertexInput {{
    @location(0) position: vec2<f32>,
    @location(1) uv: vec2<f32>,
}}

struct VertexOutput {{
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}}

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {{
    var output: VertexOutput;
    output.position = vec4<f32>(input.position, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}}

// Fragment shader
@group(0) @binding(0)
var src_texture: texture_2d<f32>;

@group(0) @binding(1)
var src_sampler: sampler;

@group(0) @binding(2)
var dst_texture: texture_2d<f32>;

@group(0) @binding(3)
var dst_sampler: sampler;

struct Uniforms {{
    opacity: f32,
}}

@group(1) @binding(0)
var<uniform> uniforms: Uniforms;

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {{
    let src = textureSample(src_texture, src_sampler, input.uv);
    let dst = textureSample(dst_texture, dst_sampler, input.uv);

    // Apply blend mode
    let blended = {blend_equation};

    // Apply opacity
    {opacity_code}

    return vec4<f32>(final_color, final_alpha);
}}
"#,
        blend_equation = blend_equation,
        opacity_code = if has_opacity {
            r#"let final_color = blended;
    let final_alpha = src.a * uniforms.opacity;"#
        } else {
            r#"let final_color = blended;
    let final_alpha = src.a;"#
        }
    )
}

/// Generate a simple blit shader (for copying without blending)
pub fn generate_blit_shader() -> String {
    r#"
// Vertex shader
struct VertexInput {
    @location(0) position: vec2<f32>,
    @location(1) uv: vec2<f32>,
}

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    output.position = vec4<f32>(input.position, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}

// Fragment shader
@group(0) @binding(0)
var src_texture: texture_2d<f32>;

@group(0) @binding(1)
var src_sampler: sampler;

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    return textureSample(src_texture, src_sampler, input.uv);
}
"#
    .to_string()
}

/// Generate a solid color shader (for rendering black on shader failure)
pub fn generate_solid_color_shader() -> String {
    r#"
// Vertex shader
struct VertexInput {
    @location(0) position: vec2<f32>,
}

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
}

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    output.position = vec4<f32>(input.position, 0.0, 1.0);
    return output;
}

// Fragment shader
struct Uniforms {
    color: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    return uniforms.color;
}
"#
    .to_string()
}

/// Blend mode metadata for GPU pipeline configuration
#[derive(Clone, Copy, Debug)]
pub struct BlendConfig {
    /// Source color blend factor
    pub src_color_factor: BlendFactor,
    /// Destination color blend factor
    pub dst_color_factor: BlendFactor,
    /// Color blend operation
    pub color_operation: BlendOperation,
    /// Source alpha blend factor
    pub src_alpha_factor: BlendFactor,
    /// Destination alpha blend factor
    pub dst_alpha_factor: BlendFactor,
    /// Alpha blend operation
    pub alpha_operation: BlendOperation,
}

/// Blend factors for GPU blending
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BlendFactor {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
}

/// Blend operations
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BlendOperation {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
}

impl BlendMode {
    /// Get GPU blend configuration for hardware blending
    ///
    /// Note: Some blend modes (like Overlay, SoftLight) require shader-based blending
    /// and will return None.
    pub fn gpu_blend_config(&self) -> Option<BlendConfig> {
        match self {
            BlendMode::Normal => Some(BlendConfig {
                src_color_factor: BlendFactor::SrcAlpha,
                dst_color_factor: BlendFactor::OneMinusSrcAlpha,
                color_operation: BlendOperation::Add,
                src_alpha_factor: BlendFactor::One,
                dst_alpha_factor: BlendFactor::OneMinusSrcAlpha,
                alpha_operation: BlendOperation::Add,
            }),
            BlendMode::Additive => Some(BlendConfig {
                src_color_factor: BlendFactor::One,
                dst_color_factor: BlendFactor::One,
                color_operation: BlendOperation::Add,
                src_alpha_factor: BlendFactor::One,
                dst_alpha_factor: BlendFactor::One,
                alpha_operation: BlendOperation::Add,
            }),
            BlendMode::Multiply => Some(BlendConfig {
                src_color_factor: BlendFactor::DstColor,
                dst_color_factor: BlendFactor::Zero,
                color_operation: BlendOperation::Add,
                src_alpha_factor: BlendFactor::DstAlpha,
                dst_alpha_factor: BlendFactor::Zero,
                alpha_operation: BlendOperation::Add,
            }),
            BlendMode::Replace => Some(BlendConfig {
                src_color_factor: BlendFactor::One,
                dst_color_factor: BlendFactor::Zero,
                color_operation: BlendOperation::Add,
                src_alpha_factor: BlendFactor::One,
                dst_alpha_factor: BlendFactor::Zero,
                alpha_operation: BlendOperation::Add,
            }),
            // Complex blend modes need shader-based blending
            _ => None,
        }
    }

    /// Check if this blend mode needs shader-based blending
    pub fn needs_shader_blend(&self) -> bool {
        self.gpu_blend_config().is_none()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_shader_generation() {
        let shader = generate_composite_shader(BlendMode::Normal, false);
        assert!(shader.contains("vs_main"));
        assert!(shader.contains("fs_main"));
        assert!(shader.contains("src_texture"));
        assert!(shader.contains("dst_texture"));
    }

    #[test]
    fn test_blit_shader() {
        let shader = generate_blit_shader();
        assert!(shader.contains("vs_main"));
        assert!(shader.contains("fs_main"));
        assert!(shader.contains("src_texture"));
        assert!(!shader.contains("dst_texture")); // Blit doesn't need destination
    }

    #[test]
    fn test_solid_color_shader() {
        let shader = generate_solid_color_shader();
        assert!(shader.contains("vs_main"));
        assert!(shader.contains("fs_main"));
        assert!(shader.contains("color"));
    }

    #[test]
    fn test_gpu_blend_config() {
        // Simple modes should have GPU blend config
        assert!(BlendMode::Normal.gpu_blend_config().is_some());
        assert!(BlendMode::Additive.gpu_blend_config().is_some());
        assert!(BlendMode::Replace.gpu_blend_config().is_some());

        // Complex modes need shader blending
        assert!(BlendMode::Overlay.needs_shader_blend());
        assert!(BlendMode::SoftLight.needs_shader_blend());
    }
}
