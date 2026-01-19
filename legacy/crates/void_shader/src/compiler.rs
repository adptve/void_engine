//! Shader compilation using naga
//!
//! Supports:
//! - WGSL parsing
//! - SPIR-V output
//! - GLSL output

use naga::front::wgsl;
use naga::back::{spv, glsl};
use naga::valid::{Capabilities, ValidationFlags, Validator};
use std::collections::HashMap;

use crate::ShaderError;

/// Shader stage
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ShaderStage {
    Vertex,
    Fragment,
    Compute,
}

/// Compilation target
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum CompileTarget {
    /// SPIR-V bytecode
    SpirV,
    /// WGSL (WebGPU)
    Wgsl,
    /// GLSL ES 300
    GlslEs300,
    /// GLSL ES 310
    GlslEs310,
    /// GLSL 330
    Glsl330,
    /// GLSL 450
    Glsl450,
}

/// Compiled shader output
#[derive(Debug, Clone)]
pub enum CompiledShader {
    /// SPIR-V bytecode
    SpirV(Vec<u32>),
    /// WGSL source
    Wgsl(String),
    /// GLSL source
    Glsl(String),
}

impl CompiledShader {
    /// Get as SPIR-V bytes
    pub fn as_spirv(&self) -> Option<&[u32]> {
        match self {
            Self::SpirV(bytes) => Some(bytes),
            _ => None,
        }
    }

    /// Get as WGSL source
    pub fn as_wgsl(&self) -> Option<&str> {
        match self {
            Self::Wgsl(source) => Some(source),
            _ => None,
        }
    }

    /// Get as GLSL source
    pub fn as_glsl(&self) -> Option<&str> {
        match self {
            Self::Glsl(source) => Some(source),
            _ => None,
        }
    }

    /// Get size in bytes
    pub fn size_bytes(&self) -> usize {
        match self {
            Self::SpirV(bytes) => bytes.len() * 4,
            Self::Wgsl(source) => source.len(),
            Self::Glsl(source) => source.len(),
        }
    }
}

/// Shader compiler
pub struct ShaderCompiler {
    /// Module cache
    module_cache: HashMap<u64, naga::Module>,
}

impl ShaderCompiler {
    /// Create a new compiler
    pub fn new() -> Self {
        Self {
            module_cache: HashMap::new(),
        }
    }

    /// Get default SPIR-V options
    fn spirv_options(&self) -> spv::Options {
        spv::Options {
            lang_version: (1, 0),
            flags: spv::WriterFlags::empty(),
            binding_map: Default::default(),
            capabilities: None,
            bounds_check_policies: Default::default(),
            zero_initialize_workgroup_memory: spv::ZeroInitializeWorkgroupMemoryMode::None,
            debug_info: None,
        }
    }

    /// Parse WGSL source into a naga module
    pub fn parse_wgsl(&self, source: &str) -> Result<naga::Module, ShaderError> {
        wgsl::parse_str(source)
            .map_err(|e| ShaderError::ParseError(format!("{:?}", e)))
    }

    /// Compile a module to the specified target
    pub fn compile(&self, module: &naga::Module, target: CompileTarget) -> Result<CompiledShader, ShaderError> {
        // Validate first
        let mut validator = Validator::new(ValidationFlags::all(), Capabilities::all());
        let info = validator.validate(module)
            .map_err(|e| ShaderError::CompileError(format!("Validation failed: {:?}", e)))?;

        match target {
            CompileTarget::SpirV => self.compile_spirv(module, &info),
            CompileTarget::Wgsl => self.compile_wgsl(module, &info),
            CompileTarget::GlslEs300 => self.compile_glsl(module, &info, glsl::Version::Embedded { version: 300, is_webgl: true }),
            CompileTarget::GlslEs310 => self.compile_glsl(module, &info, glsl::Version::Embedded { version: 310, is_webgl: false }),
            CompileTarget::Glsl330 => self.compile_glsl(module, &info, glsl::Version::Desktop(330)),
            CompileTarget::Glsl450 => self.compile_glsl(module, &info, glsl::Version::Desktop(450)),
        }
    }

    /// Compile to SPIR-V
    fn compile_spirv(&self, module: &naga::Module, info: &naga::valid::ModuleInfo) -> Result<CompiledShader, ShaderError> {
        let options = self.spirv_options();
        let words = spv::write_vec(module, info, &options, None)
            .map_err(|e| ShaderError::CompileError(format!("SPIR-V compilation failed: {:?}", e)))?;

        Ok(CompiledShader::SpirV(words))
    }

    /// Compile to WGSL
    fn compile_wgsl(&self, module: &naga::Module, info: &naga::valid::ModuleInfo) -> Result<CompiledShader, ShaderError> {
        let wgsl = naga::back::wgsl::write_string(module, info, naga::back::wgsl::WriterFlags::empty())
            .map_err(|e| ShaderError::CompileError(format!("WGSL compilation failed: {:?}", e)))?;

        Ok(CompiledShader::Wgsl(wgsl))
    }

    /// Compile to GLSL
    fn compile_glsl(&self, module: &naga::Module, info: &naga::valid::ModuleInfo, version: glsl::Version) -> Result<CompiledShader, ShaderError> {
        let options = glsl::Options {
            version,
            writer_flags: glsl::WriterFlags::empty(),
            binding_map: Default::default(),
            zero_initialize_workgroup_memory: false,
        };

        let pipeline_options = glsl::PipelineOptions {
            shader_stage: naga::ShaderStage::Vertex, // Will be overridden per entry point
            entry_point: "main".to_string(),
            multiview: None,
        };

        let mut output = String::new();
        let mut writer = glsl::Writer::new(
            &mut output,
            module,
            info,
            &options,
            &pipeline_options,
            Default::default(),
        ).map_err(|e| ShaderError::CompileError(format!("GLSL writer creation failed: {:?}", e)))?;

        writer.write()
            .map_err(|e| ShaderError::CompileError(format!("GLSL write failed: {:?}", e)))?;

        Ok(CompiledShader::Glsl(output))
    }

    /// Get SPIR-V version string
    pub fn spirv_version(&self) -> String {
        let opts = self.spirv_options();
        format!("{}.{}", opts.lang_version.0, opts.lang_version.1)
    }
}

impl Default for ShaderCompiler {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const TEST_SHADER: &str = r#"
        @vertex
        fn vs_main(@location(0) pos: vec3<f32>) -> @builtin(position) vec4<f32> {
            return vec4<f32>(pos, 1.0);
        }

        @fragment
        fn fs_main() -> @location(0) vec4<f32> {
            return vec4<f32>(1.0, 0.0, 0.0, 1.0);
        }
    "#;

    #[test]
    fn test_parse_wgsl() {
        let compiler = ShaderCompiler::new();
        let result = compiler.parse_wgsl(TEST_SHADER);
        assert!(result.is_ok());
    }

    #[test]
    fn test_compile_spirv() {
        let compiler = ShaderCompiler::new();
        let module = compiler.parse_wgsl(TEST_SHADER).unwrap();
        let result = compiler.compile(&module, CompileTarget::SpirV);
        assert!(result.is_ok());

        let compiled = result.unwrap();
        assert!(compiled.as_spirv().is_some());
        assert!(compiled.size_bytes() > 0);
    }

    #[test]
    fn test_invalid_shader() {
        let compiler = ShaderCompiler::new();
        let result = compiler.parse_wgsl("invalid shader source { } }");
        assert!(result.is_err());
    }
}
