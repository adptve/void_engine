//! Shader variants and permutations
//!
//! Supports define-based shader variants for different feature sets.

use std::collections::HashMap;
use crate::{ShaderCompiler, CompiledShader, CompileTarget, ShaderError};

/// Shader preprocessor define
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ShaderDefine {
    /// Define name
    pub name: String,
    /// Optional value (None = just defined, Some = value)
    pub value: Option<String>,
}

impl ShaderDefine {
    /// Create a simple define (no value)
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            value: None,
        }
    }

    /// Create a define with a value
    pub fn with_value(name: impl Into<String>, value: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            value: Some(value.into()),
        }
    }

    /// Convert to preprocessor directive
    pub fn to_directive(&self) -> String {
        if let Some(value) = &self.value {
            format!("#define {} {}", self.name, value)
        } else {
            format!("#define {}", self.name)
        }
    }
}

/// A set of shader defines representing a variant
#[derive(Debug, Clone, PartialEq, Eq, Hash, Default)]
pub struct ShaderVariant {
    /// Defines for this variant
    pub defines: Vec<ShaderDefine>,
    /// Variant name/identifier
    pub name: String,
}

impl ShaderVariant {
    /// Create a new variant
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            defines: Vec::new(),
        }
    }

    /// Add a define
    pub fn with_define(mut self, define: ShaderDefine) -> Self {
        self.defines.push(define);
        self
    }

    /// Add multiple defines
    pub fn with_defines(mut self, defines: Vec<ShaderDefine>) -> Self {
        self.defines.extend(defines);
        self
    }

    /// Generate preprocessor header
    pub fn generate_header(&self) -> String {
        let mut header = String::new();
        for define in &self.defines {
            header.push_str(&define.to_directive());
            header.push('\n');
        }
        header
    }

    /// Apply variant to source code
    pub fn apply_to_source(&self, source: &str) -> String {
        let header = self.generate_header();
        format!("{}\n{}", header, source)
    }
}

/// Shader variant collection
pub struct ShaderVariantCollection {
    /// Base shader source (without defines)
    base_source: String,
    /// Variants to compile
    variants: Vec<ShaderVariant>,
    /// Compiled variants cache
    compiled: HashMap<String, HashMap<CompileTarget, CompiledShader>>,
}

impl ShaderVariantCollection {
    /// Create a new variant collection
    pub fn new(base_source: String) -> Self {
        Self {
            base_source,
            variants: Vec::new(),
            compiled: HashMap::new(),
        }
    }

    /// Add a variant
    pub fn add_variant(&mut self, variant: ShaderVariant) {
        self.variants.push(variant);
    }

    /// Get all variant names
    pub fn variant_names(&self) -> Vec<String> {
        self.variants.iter().map(|v| v.name.clone()).collect()
    }

    /// Compile all variants for all targets
    pub fn compile_all(
        &mut self,
        compiler: &ShaderCompiler,
        targets: &[CompileTarget],
    ) -> Result<(), ShaderError> {
        for variant in &self.variants {
            let mut compiled_targets = HashMap::new();

            // Apply variant to source
            let source = variant.apply_to_source(&self.base_source);

            // Parse
            let module = compiler.parse_wgsl(&source)?;

            // Compile for each target
            for &target in targets {
                let compiled = compiler.compile(&module, target)?;
                compiled_targets.insert(target, compiled);
            }

            self.compiled.insert(variant.name.clone(), compiled_targets);
        }

        Ok(())
    }

    /// Get compiled variant
    pub fn get_variant(
        &self,
        variant_name: &str,
        target: CompileTarget,
    ) -> Option<&CompiledShader> {
        self.compiled
            .get(variant_name)
            .and_then(|targets| targets.get(&target))
    }

    /// Check if variant exists
    pub fn has_variant(&self, variant_name: &str) -> bool {
        self.variants.iter().any(|v| v.name == variant_name)
    }

    /// Get total number of compiled shaders
    pub fn compiled_count(&self) -> usize {
        self.compiled.values().map(|v| v.len()).sum()
    }
}

/// Variant builder for common patterns
pub struct VariantBuilder {
    base_name: String,
    feature_flags: Vec<(String, Vec<bool>)>,
}

impl VariantBuilder {
    /// Create a new builder
    pub fn new(base_name: impl Into<String>) -> Self {
        Self {
            base_name: base_name.into(),
            feature_flags: Vec::new(),
        }
    }

    /// Add a feature flag (on/off)
    pub fn with_feature(mut self, feature_name: impl Into<String>) -> Self {
        self.feature_flags.push((feature_name.into(), vec![false, true]));
        self
    }

    /// Build all permutations
    pub fn build(self) -> Vec<ShaderVariant> {
        if self.feature_flags.is_empty() {
            return vec![ShaderVariant::new(self.base_name)];
        }

        let mut variants = Vec::new();
        let total_permutations = self.feature_flags.iter()
            .map(|(_, values)| values.len())
            .product();

        for i in 0..total_permutations {
            let mut defines = Vec::new();
            let mut name_parts = vec![self.base_name.clone()];
            let mut idx = i;

            for (feature_name, values) in &self.feature_flags {
                let value_idx = idx % values.len();
                idx /= values.len();

                if values[value_idx] {
                    defines.push(ShaderDefine::new(feature_name.clone()));
                    name_parts.push(feature_name.to_lowercase());
                }
            }

            let variant_name = name_parts.join("_");
            let variant = ShaderVariant::new(variant_name)
                .with_defines(defines);
            variants.push(variant);
        }

        variants
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_shader_define() {
        let define = ShaderDefine::new("USE_TEXTURE");
        assert_eq!(define.to_directive(), "#define USE_TEXTURE");

        let define = ShaderDefine::with_value("MAX_LIGHTS", "4");
        assert_eq!(define.to_directive(), "#define MAX_LIGHTS 4");
    }

    #[test]
    fn test_variant_header() {
        let variant = ShaderVariant::new("textured")
            .with_define(ShaderDefine::new("USE_TEXTURE"))
            .with_define(ShaderDefine::with_value("MAX_LIGHTS", "4"));

        let header = variant.generate_header();
        assert!(header.contains("#define USE_TEXTURE"));
        assert!(header.contains("#define MAX_LIGHTS 4"));
    }

    #[test]
    fn test_variant_builder() {
        let variants = VariantBuilder::new("test")
            .with_feature("FEATURE_A")
            .with_feature("FEATURE_B")
            .build();

        // Should generate 4 variants: base, a, b, a+b
        assert_eq!(variants.len(), 4);

        let names: Vec<_> = variants.iter().map(|v| v.name.as_str()).collect();
        assert!(names.contains(&"test"));
        assert!(names.contains(&"test_feature_a"));
        assert!(names.contains(&"test_feature_b"));
        assert!(names.contains(&"test_feature_a_feature_b"));
    }

    #[test]
    fn test_variant_collection() {
        let source = "fn main() {}";
        let mut collection = ShaderVariantCollection::new(source.to_string());

        let variant = ShaderVariant::new("base");
        collection.add_variant(variant);

        assert!(collection.has_variant("base"));
        assert!(!collection.has_variant("other"));
    }
}
