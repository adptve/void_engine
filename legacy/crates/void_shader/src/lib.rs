//! # Void Shader
//!
//! Shader pipeline for Void Engine providing:
//! - WGSL parsing and validation via naga
//! - SPIR-V and GLSL compilation
//! - Shader reflection for binding information
//! - Hot-reload with file watching
//! - Shader registry with versioning
//!
//! ## Architecture
//!
//! ```text
//! Source Files (.wgsl) ──► Parser ──► naga::Module ──► Validator ──► Compiler ──► GPU Bytecode
//!                                          │
//!                                          ▼
//!                                    Reflector ──► Binding Info
//! ```

pub mod compiler;
pub mod validator;
pub mod reflect;
pub mod registry;
pub mod variants;
#[cfg(feature = "hot-reload")]
pub mod hot_reload;

pub use compiler::{ShaderCompiler, CompiledShader, ShaderStage, CompileTarget};
pub use validator::{ShaderValidator, ValidationResult, ValidationError};
pub use reflect::{ShaderReflection, BindingInfo, BindGroupLayout};
pub use registry::{ShaderRegistry, ShaderEntry, ShaderId, ShaderVersion};
pub use variants::{ShaderDefine, ShaderVariant, ShaderVariantCollection, VariantBuilder};

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use thiserror::Error;

/// Shader pipeline configuration
#[derive(Debug, Clone)]
pub struct ShaderPipelineConfig {
    /// Base path for shader files
    pub shader_base_path: PathBuf,
    /// Enable validation
    pub validate: bool,
    /// Default compile targets
    pub default_targets: Vec<CompileTarget>,
    /// Maximum cached shaders
    pub max_cached_shaders: usize,
    /// Enable hot-reload (requires feature)
    pub hot_reload: bool,
}

impl Default for ShaderPipelineConfig {
    fn default() -> Self {
        Self {
            shader_base_path: PathBuf::from("shaders"),
            validate: true,
            default_targets: vec![CompileTarget::SpirV],
            max_cached_shaders: 256,
            hot_reload: false,
        }
    }
}

/// Errors from the shader pipeline
#[derive(Debug, Error)]
pub enum ShaderError {
    #[error("Failed to read shader file: {0}")]
    FileRead(#[from] std::io::Error),

    #[error("WGSL parse error: {0}")]
    ParseError(String),

    #[error("Validation error: {0}")]
    ValidationError(#[from] ValidationError),

    #[error("Compilation error: {0}")]
    CompileError(String),

    #[error("Shader not found: {0}")]
    NotFound(String),

    #[error("Hot-reload error: {0}")]
    HotReloadError(String),
}

/// The main shader pipeline
pub struct ShaderPipeline {
    config: ShaderPipelineConfig,
    compiler: ShaderCompiler,
    validator: ShaderValidator,
    registry: ShaderRegistry,
    #[cfg(feature = "hot-reload")]
    watcher: Option<hot_reload::ShaderWatcher>,
    /// Path to shader ID mapping for hot-reload
    path_to_id: std::collections::HashMap<std::path::PathBuf, ShaderId>,
}

impl ShaderPipeline {
    /// Create a new shader pipeline
    pub fn new(config: ShaderPipelineConfig) -> Self {
        Self {
            config: config.clone(),
            compiler: ShaderCompiler::new(),
            validator: ShaderValidator::new(),
            registry: ShaderRegistry::new(config.max_cached_shaders),
            #[cfg(feature = "hot-reload")]
            watcher: None,
            path_to_id: HashMap::new(),
        }
    }

    /// Load a shader from file
    pub fn load_shader(&mut self, path: impl AsRef<Path>) -> Result<ShaderId, ShaderError> {
        let full_path = self.config.shader_base_path.join(path.as_ref());
        let source = std::fs::read_to_string(&full_path)?;
        let name = full_path.file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("unnamed")
            .to_string();

        let id = self.compile_shader(&name, &source)?;

        // Track path for hot-reload
        self.path_to_id.insert(full_path, id);

        Ok(id)
    }

    /// Compile shader from source
    pub fn compile_shader(&mut self, name: &str, source: &str) -> Result<ShaderId, ShaderError> {
        // Parse WGSL
        let module = self.compiler.parse_wgsl(source)?;

        // Validate
        if self.config.validate {
            self.validator.validate(&module)?;
        }

        // Reflect
        let reflection = reflect::reflect_module(&module);

        // Compile to all targets
        let mut compiled = HashMap::new();
        for target in &self.config.default_targets {
            let bytecode = self.compiler.compile(&module, *target)?;
            compiled.insert(*target, bytecode);
        }

        // Create entry
        let entry = ShaderEntry::new(name.to_string(), source.to_string(), reflection, compiled);

        // Register
        let id = self.registry.register(entry);

        log::debug!("Compiled shader '{}' -> {:?}", name, id);
        Ok(id)
    }

    /// Get a compiled shader
    pub fn get_shader(&self, id: ShaderId) -> Option<ShaderEntry> {
        self.registry.get(id)
    }

    /// Get shader by name
    pub fn get_shader_by_name(&self, name: &str) -> Option<ShaderEntry> {
        self.registry.get_by_name(name)
    }

    /// Reload a shader
    pub fn reload_shader(&mut self, id: ShaderId) -> Result<(), ShaderError> {
        let entry = self.registry.get(id)
            .ok_or_else(|| ShaderError::NotFound(format!("{:?}", id)))?;

        let source = entry.source.clone();
        let name = entry.name.clone();

        // Try to recompile
        let result = self.try_recompile(&source);

        match result {
            Ok((reflection, compiled)) => {
                // Update entry
                self.registry.update(id, |entry| {
                    entry.reflection = reflection;
                    entry.compiled = compiled;
                    entry.version = entry.version.next();
                });

                log::info!("Reloaded shader '{}'", name);
                Ok(())
            }
            Err(e) => {
                log::error!("Failed to reload shader '{}': {:?}", name, e);
                log::info!("Rolling back shader '{}' to previous version", name);

                // Attempt rollback
                if let Err(rollback_err) = self.registry.rollback(id) {
                    log::error!("Failed to rollback shader: {}", rollback_err);
                }

                Err(e)
            }
        }
    }

    /// Try to recompile shader source
    fn try_recompile(&self, source: &str) -> Result<(reflect::ShaderReflection, HashMap<CompileTarget, CompiledShader>), ShaderError> {
        // Recompile
        let module = self.compiler.parse_wgsl(source)?;

        if self.config.validate {
            self.validator.validate(&module)?;
        }

        let reflection = reflect::reflect_module(&module);

        let mut compiled = HashMap::new();
        for target in &self.config.default_targets {
            let bytecode = self.compiler.compile(&module, *target)?;
            compiled.insert(*target, bytecode);
        }

        Ok((reflection, compiled))
    }

    /// Reload all shaders
    pub fn reload_all(&mut self) -> Vec<(ShaderId, Result<(), ShaderError>)> {
        let ids: Vec<_> = self.registry.all_ids();
        ids.into_iter()
            .map(|id| (id, self.reload_shader(id)))
            .collect()
    }

    /// Get the registry
    pub fn registry(&self) -> &ShaderRegistry {
        &self.registry
    }

    /// Get the compiler
    pub fn compiler(&self) -> &ShaderCompiler {
        &self.compiler
    }

    /// Get the validator
    pub fn validator(&self) -> &ShaderValidator {
        &self.validator
    }

    /// Start hot-reload watching
    #[cfg(feature = "hot-reload")]
    pub fn start_watching(&mut self) -> Result<(), ShaderError> {
        use hot_reload::ShaderWatcher;

        let watcher = ShaderWatcher::new(&self.config.shader_base_path)
            .map_err(|e| ShaderError::HotReloadError(e.to_string()))?;

        self.watcher = Some(watcher);
        log::info!("Started shader hot-reload watching");
        Ok(())
    }

    /// Poll for file changes and automatically reload affected shaders
    #[cfg(feature = "hot-reload")]
    pub fn poll_changes(&mut self) -> Vec<(PathBuf, ShaderId, Result<(), ShaderError>)> {
        if let Some(ref mut watcher) = self.watcher {
            let changed_paths = watcher.poll_changes();
            let mut results = Vec::new();

            for path in changed_paths {
                if let Some(&shader_id) = self.path_to_id.get(&path) {
                    // Reload the shader from file
                    let reload_result = self.reload_shader_from_path(&path, shader_id);
                    results.push((path.clone(), shader_id, reload_result));
                } else {
                    log::debug!("Changed file {:?} not tracked in shader pipeline", path);
                }
            }

            results
        } else {
            Vec::new()
        }
    }

    /// Reload a shader from its tracked file path
    #[cfg(feature = "hot-reload")]
    fn reload_shader_from_path(&mut self, path: &Path, id: ShaderId) -> Result<(), ShaderError> {
        let source = std::fs::read_to_string(path)?;
        let entry = self.registry.get(id)
            .ok_or_else(|| ShaderError::NotFound(format!("{:?}", id)))?;

        // Try to recompile
        let result = self.try_recompile(&source);

        match result {
            Ok((reflection, compiled)) => {
                // Update entry with new source
                self.registry.update(id, |entry| {
                    entry.source = source;
                    entry.reflection = reflection;
                    entry.compiled = compiled;
                    entry.version = entry.version.next();
                });

                log::info!("Hot-reloaded shader '{}' from {:?}", entry.name, path);
                Ok(())
            }
            Err(e) => {
                log::error!("Failed to hot-reload shader from {:?}: {:?}", path, e);
                log::info!("Rolling back shader to previous version");

                // Attempt rollback
                if let Err(rollback_err) = self.registry.rollback(id) {
                    log::error!("Failed to rollback shader: {}", rollback_err);
                }

                Err(e)
            }
        }
    }
}

impl Default for ShaderPipeline {
    fn default() -> Self {
        Self::new(ShaderPipelineConfig::default())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const TEST_SHADER: &str = r#"
        struct VertexInput {
            @location(0) position: vec3<f32>,
            @location(1) color: vec3<f32>,
        }

        struct VertexOutput {
            @builtin(position) clip_position: vec4<f32>,
            @location(0) color: vec3<f32>,
        }

        @vertex
        fn vs_main(in: VertexInput) -> VertexOutput {
            var out: VertexOutput;
            out.clip_position = vec4<f32>(in.position, 1.0);
            out.color = in.color;
            return out;
        }

        @fragment
        fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
            return vec4<f32>(in.color, 1.0);
        }
    "#;

    #[test]
    fn test_compile_shader() {
        let mut pipeline = ShaderPipeline::default();
        let result = pipeline.compile_shader("test", TEST_SHADER);
        assert!(result.is_ok());

        let id = result.unwrap();
        let entry = pipeline.get_shader(id).unwrap();
        assert_eq!(entry.name, "test");
    }

    #[test]
    fn test_reload_shader() {
        let mut pipeline = ShaderPipeline::default();
        let id = pipeline.compile_shader("test", TEST_SHADER).unwrap();

        let result = pipeline.reload_shader(id);
        assert!(result.is_ok());

        let entry = pipeline.get_shader(id).unwrap();
        assert_eq!(entry.version.raw(), 2);
    }
}
