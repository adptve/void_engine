//! Shader reflection
//!
//! Extracts binding information from shaders.

use std::collections::HashMap;

/// Information about a binding
#[derive(Debug, Clone)]
pub struct BindingInfo {
    /// Binding group
    pub group: u32,
    /// Binding index within group
    pub binding: u32,
    /// Binding type
    pub binding_type: BindingType,
    /// Variable name (if available)
    pub name: Option<String>,
}

/// Type of binding
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BindingType {
    /// Uniform buffer
    UniformBuffer,
    /// Storage buffer (read-only)
    StorageBufferReadOnly,
    /// Storage buffer (read-write)
    StorageBuffer,
    /// Sampler
    Sampler,
    /// Texture (sampled)
    SampledTexture {
        dimension: TextureDimension,
        sample_type: SampleType,
    },
    /// Storage texture
    StorageTexture {
        dimension: TextureDimension,
        access: StorageAccess,
    },
}

/// Texture dimension
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TextureDimension {
    D1,
    D2,
    D2Array,
    D3,
    Cube,
    CubeArray,
}

/// Texture sample type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SampleType {
    Float,
    Sint,
    Uint,
    Depth,
}

/// Storage texture access
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StorageAccess {
    Load,
    Store,
    ReadWrite,
}

/// Bind group layout extracted from shader
#[derive(Debug, Clone, Default)]
pub struct BindGroupLayout {
    /// Group index
    pub group: u32,
    /// Bindings in this group
    pub bindings: Vec<BindingInfo>,
}

impl BindGroupLayout {
    /// Create a new bind group layout
    pub fn new(group: u32) -> Self {
        Self {
            group,
            bindings: Vec::new(),
        }
    }

    /// Add a binding
    pub fn add_binding(&mut self, binding: BindingInfo) {
        self.bindings.push(binding);
    }

    /// Get binding by index
    pub fn get_binding(&self, index: u32) -> Option<&BindingInfo> {
        self.bindings.iter().find(|b| b.binding == index)
    }

    /// Sort bindings by index
    pub fn sort(&mut self) {
        self.bindings.sort_by_key(|b| b.binding);
    }
}

/// Full shader reflection information
#[derive(Debug, Clone, Default)]
pub struct ShaderReflection {
    /// Bind group layouts
    pub bind_groups: HashMap<u32, BindGroupLayout>,
    /// Vertex inputs
    pub vertex_inputs: Vec<VertexInput>,
    /// Fragment outputs
    pub fragment_outputs: Vec<FragmentOutput>,
    /// Push constant ranges
    pub push_constants: Option<PushConstantRange>,
    /// Workgroup size (for compute shaders)
    pub workgroup_size: Option<[u32; 3]>,
}

/// Vertex input attribute
#[derive(Debug, Clone)]
pub struct VertexInput {
    /// Location index
    pub location: u32,
    /// Attribute name
    pub name: Option<String>,
    /// Data format
    pub format: VertexFormat,
}

/// Vertex attribute format
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VertexFormat {
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Sint32,
    Sint32x2,
    Sint32x3,
    Sint32x4,
    Uint32,
    Uint32x2,
    Uint32x3,
    Uint32x4,
}

/// Fragment output
#[derive(Debug, Clone)]
pub struct FragmentOutput {
    /// Location index
    pub location: u32,
    /// Output format
    pub format: VertexFormat,
}

/// Push constant range
#[derive(Debug, Clone)]
pub struct PushConstantRange {
    /// Shader stages that use this range
    pub stages: ShaderStages,
    /// Offset in bytes
    pub offset: u32,
    /// Size in bytes
    pub size: u32,
}

/// Shader stages bitmask
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ShaderStages(u32);

impl ShaderStages {
    pub const NONE: Self = Self(0);
    pub const VERTEX: Self = Self(1);
    pub const FRAGMENT: Self = Self(2);
    pub const COMPUTE: Self = Self(4);
    pub const ALL: Self = Self(7);

    pub fn contains(&self, stage: Self) -> bool {
        (self.0 & stage.0) != 0
    }
}

impl std::ops::BitOr for ShaderStages {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}

/// Reflect a naga module to extract binding information
pub fn reflect_module(module: &naga::Module) -> ShaderReflection {
    let mut reflection = ShaderReflection::default();

    // Extract global variable bindings
    for (_, gv) in module.global_variables.iter() {
        if let Some(binding) = &gv.binding {
            let binding_type = infer_binding_type(&module.types[gv.ty]);

            let info = BindingInfo {
                group: binding.group,
                binding: binding.binding,
                binding_type,
                name: gv.name.clone(),
            };

            reflection.bind_groups
                .entry(binding.group)
                .or_insert_with(|| BindGroupLayout::new(binding.group))
                .add_binding(info);
        }
    }

    // Sort bindings in each group
    for layout in reflection.bind_groups.values_mut() {
        layout.sort();
    }

    // Extract vertex inputs from entry points
    for ep in &module.entry_points {
        if ep.stage == naga::ShaderStage::Vertex {
            extract_vertex_inputs(module, ep, &mut reflection.vertex_inputs);
        }
        if ep.stage == naga::ShaderStage::Fragment {
            extract_fragment_outputs(module, ep, &mut reflection.fragment_outputs);
        }
        if ep.stage == naga::ShaderStage::Compute {
            reflection.workgroup_size = Some(ep.workgroup_size);
        }
    }

    reflection
}

/// Infer binding type from naga type
fn infer_binding_type(ty: &naga::Type) -> BindingType {
    match &ty.inner {
        naga::TypeInner::Sampler { comparison: _ } => BindingType::Sampler,
        naga::TypeInner::Image { dim, class, .. } => {
            let dimension = match dim {
                naga::ImageDimension::D1 => TextureDimension::D1,
                naga::ImageDimension::D2 => TextureDimension::D2,
                naga::ImageDimension::D3 => TextureDimension::D3,
                naga::ImageDimension::Cube => TextureDimension::Cube,
            };

            match class {
                naga::ImageClass::Sampled { kind, .. } => BindingType::SampledTexture {
                    dimension,
                    sample_type: match kind {
                        naga::ScalarKind::Float => SampleType::Float,
                        naga::ScalarKind::Sint => SampleType::Sint,
                        naga::ScalarKind::Uint => SampleType::Uint,
                        naga::ScalarKind::Bool => SampleType::Float,
                    },
                },
                naga::ImageClass::Depth { .. } => BindingType::SampledTexture {
                    dimension,
                    sample_type: SampleType::Depth,
                },
                naga::ImageClass::Storage { access, .. } => BindingType::StorageTexture {
                    dimension,
                    access: if access.contains(naga::StorageAccess::LOAD) && access.contains(naga::StorageAccess::STORE) {
                        StorageAccess::ReadWrite
                    } else if access.contains(naga::StorageAccess::STORE) {
                        StorageAccess::Store
                    } else {
                        StorageAccess::Load
                    },
                },
            }
        }
        naga::TypeInner::Struct { .. } => {
            // Could be uniform or storage buffer
            BindingType::UniformBuffer
        }
        _ => BindingType::UniformBuffer,
    }
}

/// Extract vertex inputs from an entry point
fn extract_vertex_inputs(
    module: &naga::Module,
    entry_point: &naga::EntryPoint,
    inputs: &mut Vec<VertexInput>,
) {
    // In naga 0.14, entry points have their own function directly
    for arg in &entry_point.function.arguments {
        if let Some(naga::Binding::Location { location, .. }) = &arg.binding {
            let format = infer_vertex_format(&module.types[arg.ty]);
            inputs.push(VertexInput {
                location: *location,
                name: arg.name.clone(),
                format,
            });
        }
    }
}

/// Extract fragment outputs from an entry point
fn extract_fragment_outputs(
    module: &naga::Module,
    entry_point: &naga::EntryPoint,
    outputs: &mut Vec<FragmentOutput>,
) {
    // In naga 0.14, entry points have their own function directly
    if let Some(result) = &entry_point.function.result {
        if let Some(naga::Binding::Location { location, .. }) = &result.binding {
            let format = infer_vertex_format(&module.types[result.ty]);
            outputs.push(FragmentOutput {
                location: *location,
                format,
            });
        }
    }
}

/// Infer vertex format from type
fn infer_vertex_format(ty: &naga::Type) -> VertexFormat {
    match &ty.inner {
        naga::TypeInner::Scalar { kind, .. } => match kind {
            naga::ScalarKind::Float => VertexFormat::Float32,
            naga::ScalarKind::Sint => VertexFormat::Sint32,
            naga::ScalarKind::Uint => VertexFormat::Uint32,
            _ => VertexFormat::Float32,
        },
        naga::TypeInner::Vector { size, kind, .. } => {
            match (kind, size) {
                (naga::ScalarKind::Float, naga::VectorSize::Bi) => VertexFormat::Float32x2,
                (naga::ScalarKind::Float, naga::VectorSize::Tri) => VertexFormat::Float32x3,
                (naga::ScalarKind::Float, naga::VectorSize::Quad) => VertexFormat::Float32x4,
                (naga::ScalarKind::Sint, naga::VectorSize::Bi) => VertexFormat::Sint32x2,
                (naga::ScalarKind::Sint, naga::VectorSize::Tri) => VertexFormat::Sint32x3,
                (naga::ScalarKind::Sint, naga::VectorSize::Quad) => VertexFormat::Sint32x4,
                (naga::ScalarKind::Uint, naga::VectorSize::Bi) => VertexFormat::Uint32x2,
                (naga::ScalarKind::Uint, naga::VectorSize::Tri) => VertexFormat::Uint32x3,
                (naga::ScalarKind::Uint, naga::VectorSize::Quad) => VertexFormat::Uint32x4,
                _ => VertexFormat::Float32x4,
            }
        }
        _ => VertexFormat::Float32x4,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use naga::front::wgsl;

    const SHADER_WITH_BINDINGS: &str = r#"
        @group(0) @binding(0)
        var<uniform> uniforms: mat4x4<f32>;

        @group(0) @binding(1)
        var texture: texture_2d<f32>;

        @group(0) @binding(2)
        var sampler_: sampler;

        @vertex
        fn vs_main(@location(0) pos: vec3<f32>, @location(1) uv: vec2<f32>) -> @builtin(position) vec4<f32> {
            return uniforms * vec4<f32>(pos, 1.0);
        }

        @fragment
        fn fs_main() -> @location(0) vec4<f32> {
            return vec4<f32>(1.0);
        }
    "#;

    #[test]
    fn test_reflect_bindings() {
        let module = wgsl::parse_str(SHADER_WITH_BINDINGS).unwrap();
        let reflection = reflect_module(&module);

        // Should have group 0
        assert!(reflection.bind_groups.contains_key(&0));

        let group0 = &reflection.bind_groups[&0];
        assert_eq!(group0.bindings.len(), 3);

        // Check binding 0 (uniform buffer)
        let binding0 = group0.get_binding(0).unwrap();
        assert!(matches!(binding0.binding_type, BindingType::UniformBuffer));

        // Check binding 1 (texture)
        let binding1 = group0.get_binding(1).unwrap();
        assert!(matches!(binding1.binding_type, BindingType::SampledTexture { .. }));

        // Check binding 2 (sampler)
        let binding2 = group0.get_binding(2).unwrap();
        assert!(matches!(binding2.binding_type, BindingType::Sampler));
    }

    #[test]
    fn test_reflect_vertex_inputs() {
        let module = wgsl::parse_str(SHADER_WITH_BINDINGS).unwrap();
        let reflection = reflect_module(&module);

        assert_eq!(reflection.vertex_inputs.len(), 2);
        assert_eq!(reflection.vertex_inputs[0].location, 0);
        assert_eq!(reflection.vertex_inputs[1].location, 1);
    }

    #[test]
    fn test_reflect_fragment_outputs() {
        let module = wgsl::parse_str(SHADER_WITH_BINDINGS).unwrap();
        let reflection = reflect_module(&module);

        assert_eq!(reflection.fragment_outputs.len(), 1);
        assert_eq!(reflection.fragment_outputs[0].location, 0);
    }
}
