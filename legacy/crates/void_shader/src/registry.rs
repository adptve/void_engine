//! Shader registry with versioning
//!
//! Manages compiled shaders with version tracking for hot-reload support.

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use parking_lot::RwLock;

use crate::compiler::{CompiledShader, CompileTarget};
use crate::reflect::ShaderReflection;

/// Unique shader identifier
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ShaderId(u64);

impl ShaderId {
    /// Create a new shader ID
    fn new(id: u64) -> Self {
        Self(id)
    }

    /// Get the raw ID value
    pub fn raw(&self) -> u64 {
        self.0
    }
}

/// Shader version for tracking changes
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ShaderVersion(u32);

impl ShaderVersion {
    /// Initial version
    pub const INITIAL: Self = Self(1);

    /// Create a new version
    pub fn new(version: u32) -> Self {
        Self(version)
    }

    /// Get the raw version number
    pub fn raw(&self) -> u32 {
        self.0
    }

    /// Get the next version
    pub fn next(&self) -> Self {
        Self(self.0.saturating_add(1))
    }
}

impl Default for ShaderVersion {
    fn default() -> Self {
        Self::INITIAL
    }
}

/// A registered shader entry
#[derive(Debug, Clone)]
pub struct ShaderEntry {
    /// Shader name/identifier
    pub name: String,
    /// Original source code
    pub source: String,
    /// Current version
    pub version: ShaderVersion,
    /// Reflection information
    pub reflection: ShaderReflection,
    /// Compiled outputs by target
    pub compiled: HashMap<CompileTarget, CompiledShader>,
    /// Metadata
    pub metadata: ShaderMetadata,
}

impl ShaderEntry {
    /// Create a new shader entry
    pub fn new(
        name: String,
        source: String,
        reflection: ShaderReflection,
        compiled: HashMap<CompileTarget, CompiledShader>,
    ) -> Self {
        Self {
            name,
            source,
            version: ShaderVersion::INITIAL,
            reflection,
            compiled,
            metadata: ShaderMetadata::default(),
        }
    }

    /// Get compiled output for a target
    pub fn get_compiled(&self, target: CompileTarget) -> Option<&CompiledShader> {
        self.compiled.get(&target)
    }

    /// Check if a target is compiled
    pub fn has_target(&self, target: CompileTarget) -> bool {
        self.compiled.contains_key(&target)
    }

    /// Get all available targets
    pub fn available_targets(&self) -> Vec<CompileTarget> {
        self.compiled.keys().copied().collect()
    }

    /// Update with a new version
    pub fn update(&mut self, source: String, reflection: ShaderReflection, compiled: HashMap<CompileTarget, CompiledShader>) {
        self.source = source;
        self.reflection = reflection;
        self.compiled = compiled;
        self.version = self.version.next();
        self.metadata.update_timestamp();
    }
}

/// Shader metadata
#[derive(Debug, Clone, Default)]
pub struct ShaderMetadata {
    /// Creation timestamp (Unix epoch seconds)
    pub created_at: u64,
    /// Last update timestamp
    pub updated_at: u64,
    /// Number of times reloaded
    pub reload_count: u32,
    /// Custom tags
    pub tags: Vec<String>,
}

impl ShaderMetadata {
    /// Update the timestamp
    pub fn update_timestamp(&mut self) {
        self.updated_at = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_secs())
            .unwrap_or(0);
        self.reload_count = self.reload_count.saturating_add(1);
    }
}

/// Shader registry for managing compiled shaders
pub struct ShaderRegistry {
    /// Next ID counter
    next_id: AtomicU64,
    /// Shaders by ID
    shaders: RwLock<HashMap<ShaderId, ShaderEntry>>,
    /// Name to ID mapping
    name_to_id: RwLock<HashMap<String, ShaderId>>,
    /// Maximum cached shaders (0 = unlimited)
    max_cached: usize,
    /// Version change listeners
    listeners: RwLock<Vec<Box<dyn Fn(ShaderId, ShaderVersion) + Send + Sync>>>,
    /// Rollback history
    history: RwLock<HashMap<ShaderId, Vec<ShaderEntry>>>,
    /// Maximum history depth per shader
    max_history: usize,
}

impl ShaderRegistry {
    /// Create a new registry
    pub fn new(max_cached: usize) -> Self {
        Self::with_history(max_cached, 3)
    }

    /// Create a new registry with custom history depth
    pub fn with_history(max_cached: usize, max_history: usize) -> Self {
        Self {
            next_id: AtomicU64::new(1),
            shaders: RwLock::new(HashMap::new()),
            name_to_id: RwLock::new(HashMap::new()),
            max_cached,
            listeners: RwLock::new(Vec::new()),
            history: RwLock::new(HashMap::new()),
            max_history,
        }
    }

    /// Register a new shader
    pub fn register(&self, entry: ShaderEntry) -> ShaderId {
        let id = ShaderId::new(self.next_id.fetch_add(1, Ordering::Relaxed));
        let name = entry.name.clone();
        let version = entry.version;

        // Check if we need to evict old entries
        if self.max_cached > 0 {
            let count = self.shaders.read().len();
            if count >= self.max_cached {
                self.evict_oldest();
            }
        }

        // Insert shader
        self.shaders.write().insert(id, entry);
        self.name_to_id.write().insert(name, id);

        // Notify listeners
        self.notify_listeners(id, version);

        id
    }

    /// Get a shader by ID
    pub fn get(&self, id: ShaderId) -> Option<ShaderEntry> {
        self.shaders.read().get(&id).cloned()
    }

    /// Get a shader by name
    pub fn get_by_name(&self, name: &str) -> Option<ShaderEntry> {
        let id = self.name_to_id.read().get(name).copied()?;
        self.get(id)
    }

    /// Get shader ID by name
    pub fn get_id(&self, name: &str) -> Option<ShaderId> {
        self.name_to_id.read().get(name).copied()
    }

    /// Update a shader entry
    pub fn update<F>(&self, id: ShaderId, f: F)
    where
        F: FnOnce(&mut ShaderEntry),
    {
        let mut shaders = self.shaders.write();
        if let Some(entry) = shaders.get_mut(&id) {
            // Save current version to history before updating
            self.push_history(id, entry.clone());

            f(entry);
            let version = entry.version;
            drop(shaders);
            self.notify_listeners(id, version);
        }
    }

    /// Rollback a shader to previous version
    pub fn rollback(&self, id: ShaderId) -> Result<(), String> {
        let mut history = self.history.write();
        let shader_history = history.get_mut(&id)
            .ok_or_else(|| "No history available for shader".to_string())?;

        let previous = shader_history.pop()
            .ok_or_else(|| "No previous version available".to_string())?;

        let mut shaders = self.shaders.write();
        if let Some(current) = shaders.get_mut(&id) {
            *current = previous;
            let version = current.version;
            drop(shaders);
            drop(history);

            log::info!("Rolled back shader {:?} to version {}", id, version.raw());
            self.notify_listeners(id, version);
            Ok(())
        } else {
            Err("Shader not found in registry".to_string())
        }
    }

    /// Get history depth for a shader
    pub fn history_depth(&self, id: ShaderId) -> usize {
        self.history.read()
            .get(&id)
            .map(|h| h.len())
            .unwrap_or(0)
    }

    /// Clear history for a shader
    pub fn clear_history(&self, id: ShaderId) {
        self.history.write().remove(&id);
    }

    /// Clear all history
    pub fn clear_all_history(&self) {
        self.history.write().clear();
    }

    /// Remove a shader
    pub fn remove(&self, id: ShaderId) -> Option<ShaderEntry> {
        let entry = self.shaders.write().remove(&id)?;
        self.name_to_id.write().remove(&entry.name);
        Some(entry)
    }

    /// Remove by name
    pub fn remove_by_name(&self, name: &str) -> Option<ShaderEntry> {
        let id = self.name_to_id.write().remove(name)?;
        self.shaders.write().remove(&id)
    }

    /// Check if shader exists
    pub fn contains(&self, id: ShaderId) -> bool {
        self.shaders.read().contains_key(&id)
    }

    /// Check if shader exists by name
    pub fn contains_name(&self, name: &str) -> bool {
        self.name_to_id.read().contains_key(name)
    }

    /// Get all shader IDs
    pub fn all_ids(&self) -> Vec<ShaderId> {
        self.shaders.read().keys().copied().collect()
    }

    /// Get all shader names
    pub fn all_names(&self) -> Vec<String> {
        self.name_to_id.read().keys().cloned().collect()
    }

    /// Get total number of shaders
    pub fn count(&self) -> usize {
        self.shaders.read().len()
    }

    /// Get shader version
    pub fn version(&self, id: ShaderId) -> Option<ShaderVersion> {
        self.shaders.read().get(&id).map(|e| e.version)
    }

    /// Add a version change listener
    pub fn add_listener<F>(&self, listener: F)
    where
        F: Fn(ShaderId, ShaderVersion) + Send + Sync + 'static,
    {
        self.listeners.write().push(Box::new(listener));
    }

    /// Clear all listeners
    pub fn clear_listeners(&self) {
        self.listeners.write().clear();
    }

    /// Evict the oldest shader (by update time)
    fn evict_oldest(&self) {
        let mut shaders = self.shaders.write();
        if shaders.is_empty() {
            return;
        }

        // Find oldest
        let oldest = shaders
            .iter()
            .min_by_key(|(_, e)| e.metadata.updated_at)
            .map(|(id, _)| *id);

        if let Some(id) = oldest {
            if let Some(entry) = shaders.remove(&id) {
                self.name_to_id.write().remove(&entry.name);
                log::debug!("Evicted shader '{}' from cache", entry.name);
            }
        }
    }

    /// Notify version change listeners
    fn notify_listeners(&self, id: ShaderId, version: ShaderVersion) {
        let listeners = self.listeners.read();
        for listener in listeners.iter() {
            listener(id, version);
        }
    }

    /// Push current entry to history
    fn push_history(&self, id: ShaderId, entry: ShaderEntry) {
        let mut history = self.history.write();
        let shader_history = history.entry(id).or_insert_with(Vec::new);

        shader_history.push(entry);

        // Trim history if needed
        while shader_history.len() > self.max_history {
            shader_history.remove(0);
        }
    }

    /// Get statistics about the registry
    pub fn stats(&self) -> RegistryStats {
        let shaders = self.shaders.read();
        let total_shaders = shaders.len();
        let total_compilations: usize = shaders.values().map(|e| e.compiled.len()).sum();
        let total_source_bytes: usize = shaders.values().map(|e| e.source.len()).sum();
        let total_compiled_bytes: usize = shaders
            .values()
            .flat_map(|e| e.compiled.values())
            .map(|c| c.size_bytes())
            .sum();

        RegistryStats {
            total_shaders,
            total_compilations,
            total_source_bytes,
            total_compiled_bytes,
            max_cached: self.max_cached,
        }
    }
}

impl Default for ShaderRegistry {
    fn default() -> Self {
        Self::new(256)
    }
}

/// Registry statistics
#[derive(Debug, Clone)]
pub struct RegistryStats {
    /// Total number of shaders
    pub total_shaders: usize,
    /// Total number of compilations (across all targets)
    pub total_compilations: usize,
    /// Total source code bytes
    pub total_source_bytes: usize,
    /// Total compiled code bytes
    pub total_compiled_bytes: usize,
    /// Maximum cached shaders setting
    pub max_cached: usize,
}

#[cfg(test)]
mod tests {
    use super::*;

    fn create_test_entry(name: &str) -> ShaderEntry {
        ShaderEntry::new(
            name.to_string(),
            "// test shader".to_string(),
            ShaderReflection::default(),
            HashMap::new(),
        )
    }

    #[test]
    fn test_register_and_get() {
        let registry = ShaderRegistry::new(10);
        let entry = create_test_entry("test_shader");

        let id = registry.register(entry);
        assert!(registry.contains(id));
        assert!(registry.contains_name("test_shader"));

        let retrieved = registry.get(id).unwrap();
        assert_eq!(retrieved.name, "test_shader");
    }

    #[test]
    fn test_get_by_name() {
        let registry = ShaderRegistry::new(10);
        let entry = create_test_entry("my_shader");

        registry.register(entry);

        let retrieved = registry.get_by_name("my_shader").unwrap();
        assert_eq!(retrieved.name, "my_shader");
    }

    #[test]
    fn test_update() {
        let registry = ShaderRegistry::new(10);
        let entry = create_test_entry("updatable");

        let id = registry.register(entry);
        assert_eq!(registry.version(id), Some(ShaderVersion::INITIAL));

        registry.update(id, |e| {
            e.source = "// updated".to_string();
            e.version = e.version.next();
        });

        let retrieved = registry.get(id).unwrap();
        assert_eq!(retrieved.source, "// updated");
        assert_eq!(retrieved.version.raw(), 2);
    }

    #[test]
    fn test_remove() {
        let registry = ShaderRegistry::new(10);
        let entry = create_test_entry("removable");

        let id = registry.register(entry);
        assert!(registry.contains(id));

        let removed = registry.remove(id).unwrap();
        assert_eq!(removed.name, "removable");
        assert!(!registry.contains(id));
    }

    #[test]
    fn test_eviction() {
        let registry = ShaderRegistry::new(2);

        registry.register(create_test_entry("first"));
        registry.register(create_test_entry("second"));

        assert_eq!(registry.count(), 2);

        // Third entry should trigger eviction
        registry.register(create_test_entry("third"));

        assert_eq!(registry.count(), 2);
        // "first" should have been evicted (oldest)
        assert!(!registry.contains_name("first"));
        assert!(registry.contains_name("second"));
        assert!(registry.contains_name("third"));
    }

    #[test]
    fn test_version_tracking() {
        let version = ShaderVersion::INITIAL;
        assert_eq!(version.raw(), 1);

        let next = version.next();
        assert_eq!(next.raw(), 2);

        let next_next = next.next();
        assert_eq!(next_next.raw(), 3);
    }

    #[test]
    fn test_stats() {
        let registry = ShaderRegistry::new(10);
        registry.register(create_test_entry("a"));
        registry.register(create_test_entry("b"));

        let stats = registry.stats();
        assert_eq!(stats.total_shaders, 2);
    }
}
