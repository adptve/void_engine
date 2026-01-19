//! WGSL Shader loader

use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

/// Compiled shader ready for GPU upload
#[derive(Clone, Debug)]
pub struct ShaderAsset {
    /// Shader source (WGSL)
    pub source: String,
    /// Entry points discovered
    pub entry_points: Vec<ShaderEntryPoint>,
    /// Hash for change detection
    pub hash: u64,
}

/// Shader entry point info
#[derive(Clone, Debug)]
pub struct ShaderEntryPoint {
    pub name: String,
    pub stage: ShaderStage,
}

/// Shader stage type
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ShaderStage {
    Vertex,
    Fragment,
    Compute,
}

/// Loader for WGSL shader files
pub struct ShaderLoader;

impl ShaderLoader {
    /// Load a shader from WGSL source bytes
    pub fn load(data: &[u8], path: &str) -> Result<ShaderAsset, String> {
        let source = std::str::from_utf8(data)
            .map_err(|e| format!("Invalid UTF-8 in shader {}: {}", path, e))?
            .to_string();

        // Calculate hash for change detection
        let mut hasher = DefaultHasher::new();
        source.hash(&mut hasher);
        let hash = hasher.finish();

        // Parse entry points from source
        let entry_points = Self::parse_entry_points(&source);

        Ok(ShaderAsset {
            source,
            entry_points,
            hash,
        })
    }

    /// Parse @vertex, @fragment, @compute entry points from WGSL source
    fn parse_entry_points(source: &str) -> Vec<ShaderEntryPoint> {
        let mut entry_points = Vec::new();

        // Simple regex-free parsing for entry points
        // Look for patterns like: @vertex fn name(
        let lines: Vec<&str> = source.lines().collect();

        for (i, line) in lines.iter().enumerate() {
            let trimmed = line.trim();

            // Check for stage annotations
            let stage = if trimmed.starts_with("@vertex") {
                Some(ShaderStage::Vertex)
            } else if trimmed.starts_with("@fragment") {
                Some(ShaderStage::Fragment)
            } else if trimmed.starts_with("@compute") {
                Some(ShaderStage::Compute)
            } else {
                None
            };

            if let Some(stage) = stage {
                // Find the function name - could be on same line or next line
                let search_text = if trimmed.contains("fn ") {
                    trimmed.to_string()
                } else if i + 1 < lines.len() {
                    lines[i + 1].to_string()
                } else {
                    continue;
                };

                if let Some(name) = Self::extract_fn_name(&search_text) {
                    entry_points.push(ShaderEntryPoint {
                        name: name.to_string(),
                        stage,
                    });
                }
            }
        }

        entry_points
    }

    /// Extract function name from a line containing "fn name("
    fn extract_fn_name(line: &str) -> Option<&str> {
        let fn_pos = line.find("fn ")?;
        let after_fn = &line[fn_pos + 3..];
        let name_end = after_fn.find('(')?;
        Some(after_fn[..name_end].trim())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_entry_points() {
        let source = r#"
@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return vec4(1.0);
}
"#;
        let entries = ShaderLoader::parse_entry_points(source);
        assert_eq!(entries.len(), 2);
        assert_eq!(entries[0].name, "vs_main");
        assert_eq!(entries[0].stage, ShaderStage::Vertex);
        assert_eq!(entries[1].name, "fs_main");
        assert_eq!(entries[1].stage, ShaderStage::Fragment);
    }
}
