//! Shader validation
//!
//! Validates shaders for correctness and conformance.

use naga::valid::{Capabilities, ValidationFlags, Validator};
use thiserror::Error;

/// Validation error
#[derive(Debug, Error)]
pub enum ValidationError {
    #[error("Shader validation failed: {0}")]
    ValidationFailed(String),

    #[error("Missing entry point: {0}")]
    MissingEntryPoint(String),

    #[error("Invalid binding: {0}")]
    InvalidBinding(String),

    #[error("Resource limit exceeded: {0}")]
    ResourceLimit(String),
}

/// Result of shader validation
#[derive(Debug, Clone)]
pub struct ValidationResult {
    /// Whether validation passed
    pub valid: bool,
    /// Warnings (non-fatal)
    pub warnings: Vec<String>,
    /// Entry points found
    pub entry_points: Vec<String>,
    /// Binding groups used
    pub binding_groups: Vec<u32>,
}

impl ValidationResult {
    /// Create a successful result
    pub fn success(entry_points: Vec<String>, binding_groups: Vec<u32>) -> Self {
        Self {
            valid: true,
            warnings: Vec::new(),
            entry_points,
            binding_groups,
        }
    }

    /// Add a warning
    pub fn with_warning(mut self, warning: String) -> Self {
        self.warnings.push(warning);
        self
    }
}

/// Shader validator
pub struct ShaderValidator {
    /// Required capabilities
    capabilities: Capabilities,
    /// Validation flags
    flags: ValidationFlags,
    /// Custom validation rules
    rules: Vec<Box<dyn ValidationRule>>,
}

impl ShaderValidator {
    /// Create a new validator with default settings
    pub fn new() -> Self {
        Self {
            capabilities: Capabilities::all(),
            flags: ValidationFlags::all(),
            rules: Vec::new(),
        }
    }

    /// Create a validator with specific capabilities
    pub fn with_capabilities(capabilities: Capabilities) -> Self {
        Self {
            capabilities,
            flags: ValidationFlags::all(),
            rules: Vec::new(),
        }
    }

    /// Set validation flags
    pub fn set_flags(&mut self, flags: ValidationFlags) {
        self.flags = flags;
    }

    /// Add a custom validation rule
    pub fn add_rule(&mut self, rule: Box<dyn ValidationRule>) {
        self.rules.push(rule);
    }

    /// Validate a shader module
    pub fn validate(&self, module: &naga::Module) -> Result<ValidationResult, ValidationError> {
        // Run naga validation
        let mut validator = Validator::new(self.flags, self.capabilities);
        let info = validator.validate(module)
            .map_err(|e| ValidationError::ValidationFailed(format!("{:?}", e)))?;

        // Collect entry points
        let entry_points: Vec<String> = module.entry_points
            .iter()
            .map(|ep| ep.name.clone())
            .collect();

        if entry_points.is_empty() {
            return Err(ValidationError::MissingEntryPoint(
                "No entry points found in shader".to_string()
            ));
        }

        // Collect binding groups
        let mut binding_groups: Vec<u32> = module.global_variables
            .iter()
            .filter_map(|(_, gv)| gv.binding.as_ref().map(|b| b.group))
            .collect();
        binding_groups.sort();
        binding_groups.dedup();

        let mut result = ValidationResult::success(entry_points, binding_groups);

        // Run custom rules
        for rule in &self.rules {
            if let Err(e) = rule.validate(module) {
                return Err(e);
            }
            if let Some(warnings) = rule.warnings(module) {
                for w in warnings {
                    result = result.with_warning(w);
                }
            }
        }

        Ok(result)
    }

    /// Check if shader has a specific entry point
    pub fn has_entry_point(&self, module: &naga::Module, name: &str) -> bool {
        module.entry_points.iter().any(|ep| ep.name == name)
    }

    /// Get all entry point names
    pub fn entry_point_names(&self, module: &naga::Module) -> Vec<String> {
        module.entry_points
            .iter()
            .map(|ep| ep.name.clone())
            .collect()
    }
}

impl Default for ShaderValidator {
    fn default() -> Self {
        Self::new()
    }
}

/// Custom validation rule trait
pub trait ValidationRule: Send + Sync {
    /// Validate the module, return error if invalid
    fn validate(&self, module: &naga::Module) -> Result<(), ValidationError>;

    /// Return any warnings
    fn warnings(&self, module: &naga::Module) -> Option<Vec<String>> {
        None
    }
}

/// Rule to limit maximum bindings per group
pub struct MaxBindingsRule {
    pub max_bindings: u32,
}

impl ValidationRule for MaxBindingsRule {
    fn validate(&self, module: &naga::Module) -> Result<(), ValidationError> {
        let mut group_counts: std::collections::HashMap<u32, u32> = std::collections::HashMap::new();

        for (_, gv) in module.global_variables.iter() {
            if let Some(binding) = &gv.binding {
                *group_counts.entry(binding.group).or_insert(0) += 1;
            }
        }

        for (group, count) in group_counts {
            if count > self.max_bindings {
                return Err(ValidationError::ResourceLimit(format!(
                    "Binding group {} has {} bindings, max is {}",
                    group, count, self.max_bindings
                )));
            }
        }

        Ok(())
    }
}

/// Rule to require specific entry points
pub struct RequiredEntryPointsRule {
    pub required: Vec<String>,
}

impl ValidationRule for RequiredEntryPointsRule {
    fn validate(&self, module: &naga::Module) -> Result<(), ValidationError> {
        for required in &self.required {
            if !module.entry_points.iter().any(|ep| &ep.name == required) {
                return Err(ValidationError::MissingEntryPoint(required.clone()));
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use naga::front::wgsl;

    const VALID_SHADER: &str = r#"
        @vertex
        fn vs_main(@location(0) pos: vec3<f32>) -> @builtin(position) vec4<f32> {
            return vec4<f32>(pos, 1.0);
        }
    "#;

    #[test]
    fn test_validate_valid_shader() {
        let module = wgsl::parse_str(VALID_SHADER).unwrap();
        let validator = ShaderValidator::new();
        let result = validator.validate(&module);
        assert!(result.is_ok());

        let result = result.unwrap();
        assert!(result.valid);
        assert!(result.entry_points.contains(&"vs_main".to_string()));
    }

    #[test]
    fn test_entry_point_detection() {
        let module = wgsl::parse_str(VALID_SHADER).unwrap();
        let validator = ShaderValidator::new();

        assert!(validator.has_entry_point(&module, "vs_main"));
        assert!(!validator.has_entry_point(&module, "nonexistent"));
    }

    #[test]
    fn test_custom_rule() {
        let module = wgsl::parse_str(VALID_SHADER).unwrap();
        let mut validator = ShaderValidator::new();

        // Add rule requiring an entry point that doesn't exist
        validator.add_rule(Box::new(RequiredEntryPointsRule {
            required: vec!["fs_main".to_string()],
        }));

        let result = validator.validate(&module);
        assert!(result.is_err());
    }
}
