//! Pass Registry
//!
//! Manages custom render passes with dependency resolution and resource budgeting.
//!
//! # Example
//!
//! ```ignore
//! use void_render::pass::registry::PassRegistry;
//! use void_render::pass::custom::CustomRenderPass;
//!
//! let mut registry = PassRegistry::new();
//!
//! // Register passes
//! registry.register(BloomPass::new(1.0, 0.5))?;
//! registry.register(OutlinePass::new())?;
//!
//! // Create a pass group
//! registry.create_group("post_effects", vec!["bloom".into(), "outline".into()]);
//!
//! // Execute all passes
//! registry.execute(&mut context)?;
//! ```

use alloc::boxed::Box;
use alloc::collections::{BTreeMap, BTreeSet};
use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use super::custom::{
    CustomPassState, CustomRenderPass, PassConfigData, PassError, PassExecuteContext,
    PassSetupContext, ResourceRequirements,
};

/// Resource budget for custom passes
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ResourceBudget {
    /// Maximum GPU memory for custom passes
    pub max_memory_bytes: u64,

    /// Maximum render targets
    pub max_render_targets: u32,

    /// Maximum time per frame (ms)
    pub max_time_ms: f32,

    /// Current used memory
    #[serde(skip)]
    pub used_memory: u64,

    /// Current used render targets
    #[serde(skip)]
    pub used_render_targets: u32,
}

impl Default for ResourceBudget {
    fn default() -> Self {
        Self {
            max_memory_bytes: 512 * 1024 * 1024, // 512 MB
            max_render_targets: 8,
            max_time_ms: 8.0, // 8ms budget
            used_memory: 0,
            used_render_targets: 0,
        }
    }
}

impl ResourceBudget {
    /// Create a budget with custom limits
    pub fn with_limits(memory_mb: u64, render_targets: u32, time_ms: f32) -> Self {
        Self {
            max_memory_bytes: memory_mb * 1024 * 1024,
            max_render_targets: render_targets,
            max_time_ms: time_ms,
            used_memory: 0,
            used_render_targets: 0,
        }
    }

    /// Check if requirements fit within budget
    pub fn check(&self, reqs: &ResourceRequirements) -> Result<(), PassError> {
        if self.used_memory + reqs.memory_bytes > self.max_memory_bytes {
            return Err(PassError::Budget(alloc::format!(
                "Memory budget exceeded: {} + {} > {} bytes",
                self.used_memory,
                reqs.memory_bytes,
                self.max_memory_bytes
            )));
        }

        if self.used_render_targets + reqs.render_targets > self.max_render_targets {
            return Err(PassError::Budget(alloc::format!(
                "Render target budget exceeded: {} + {} > {}",
                self.used_render_targets,
                reqs.render_targets,
                self.max_render_targets
            )));
        }

        Ok(())
    }

    /// Allocate resources for a pass
    pub fn allocate(&mut self, reqs: &ResourceRequirements) {
        self.used_memory += reqs.memory_bytes;
        self.used_render_targets += reqs.render_targets;
    }

    /// Release resources from a pass
    pub fn release(&mut self, reqs: &ResourceRequirements) {
        self.used_memory = self.used_memory.saturating_sub(reqs.memory_bytes);
        self.used_render_targets = self.used_render_targets.saturating_sub(reqs.render_targets);
    }

    /// Get remaining memory
    pub fn remaining_memory(&self) -> u64 {
        self.max_memory_bytes.saturating_sub(self.used_memory)
    }

    /// Get remaining render targets
    pub fn remaining_render_targets(&self) -> u32 {
        self.max_render_targets.saturating_sub(self.used_render_targets)
    }

    /// Get usage as percentage (0.0 - 1.0)
    pub fn memory_usage_percent(&self) -> f32 {
        if self.max_memory_bytes == 0 {
            0.0
        } else {
            self.used_memory as f32 / self.max_memory_bytes as f32
        }
    }
}

/// Pass registration data
struct PassData {
    /// The pass implementation
    pass: Box<dyn CustomRenderPass>,
    /// Whether the pass is enabled
    enabled: bool,
    /// Failure count for fault tolerance
    failure_count: u32,
}

/// Update types for frame-boundary application
#[derive(Clone, Debug)]
pub enum PassRegistryUpdate {
    /// Register a new pass (pass stored separately)
    RegisterPending(String),
    /// Unregister a pass
    Unregister(String),
    /// Configure a pass
    Configure(String, PassConfigData),
    /// Restore from state
    Restore(CustomPassState),
    /// Rebuild pipelines for passes
    RebuildPipelines(Vec<String>),
    /// Enable/disable a pass
    SetEnabled(String, bool),
    /// Enable/disable a group
    SetGroupEnabled(String, bool),
}

/// Update queue for frame-boundary application
#[derive(Default)]
pub struct PassUpdateQueue {
    /// Pending updates
    pending: Vec<PassRegistryUpdate>,
    /// Pending passes to register (stored separately from updates)
    pending_passes: Vec<Box<dyn CustomRenderPass>>,
}

impl PassUpdateQueue {
    /// Create a new update queue
    pub fn new() -> Self {
        Self::default()
    }

    /// Queue an update
    pub fn queue(&mut self, update: PassRegistryUpdate) {
        self.pending.push(update);
    }

    /// Queue a pass for registration
    pub fn queue_pass(&mut self, pass: Box<dyn CustomRenderPass>) {
        let name = pass.name().to_string();
        self.pending_passes.push(pass);
        self.queue(PassRegistryUpdate::RegisterPending(name));
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.pending.is_empty() && self.pending_passes.is_empty()
    }

    /// Get number of pending updates
    pub fn len(&self) -> usize {
        self.pending.len()
    }
}

/// Manages custom render passes
pub struct PassRegistry {
    /// Registered passes
    passes: BTreeMap<String, PassData>,

    /// Resolved execution order
    order: Vec<String>,

    /// Pass groups (for enabling/disabling sets)
    groups: BTreeMap<String, Vec<String>>,

    /// Resource budget
    budget: ResourceBudget,

    /// Disabled passes (due to repeated failures)
    disabled_passes: BTreeSet<String>,

    /// Failure counts per pass
    failure_counts: BTreeMap<String, u32>,

    /// Fallback pass mappings (pass_name -> fallback_name)
    fallbacks: BTreeMap<String, String>,

    /// Active fallbacks (pass_name -> currently_active_fallback)
    active_fallbacks: BTreeMap<String, String>,

    /// Update queue for frame-boundary updates
    update_queue: PassUpdateQueue,

    /// Maximum failures before disabling a pass
    max_failures: u32,
}

impl Default for PassRegistry {
    fn default() -> Self {
        Self::new()
    }
}

impl PassRegistry {
    /// Create a new pass registry
    pub fn new() -> Self {
        Self {
            passes: BTreeMap::new(),
            order: Vec::new(),
            groups: BTreeMap::new(),
            budget: ResourceBudget::default(),
            disabled_passes: BTreeSet::new(),
            failure_counts: BTreeMap::new(),
            fallbacks: BTreeMap::new(),
            active_fallbacks: BTreeMap::new(),
            update_queue: PassUpdateQueue::new(),
            max_failures: 3,
        }
    }

    /// Create with custom budget
    pub fn with_budget(budget: ResourceBudget) -> Self {
        Self {
            budget,
            ..Self::new()
        }
    }

    /// Register a custom pass
    pub fn register<P: CustomRenderPass + 'static>(
        &mut self,
        pass: P,
    ) -> Result<(), PassError> {
        let name = pass.name().to_string();
        let reqs = pass.resource_requirements();

        // Check budget
        self.budget.check(&reqs)?;

        // Allocate resources
        self.budget.allocate(&reqs);

        // Store pass
        self.passes.insert(
            name.clone(),
            PassData {
                pass: Box::new(pass),
                enabled: true,
                failure_count: 0,
            },
        );

        // Rebuild order
        self.rebuild_order();

        Ok(())
    }

    /// Register a boxed pass
    pub fn register_boxed(
        &mut self,
        pass: Box<dyn CustomRenderPass>,
    ) -> Result<(), PassError> {
        let name = pass.name().to_string();
        let reqs = pass.resource_requirements();

        // Check budget
        self.budget.check(&reqs)?;

        // Allocate resources
        self.budget.allocate(&reqs);

        // Store pass
        self.passes.insert(
            name.clone(),
            PassData {
                pass,
                enabled: true,
                failure_count: 0,
            },
        );

        // Rebuild order
        self.rebuild_order();

        Ok(())
    }

    /// Unregister a pass
    pub fn unregister(&mut self, name: &str) {
        if let Some(data) = self.passes.remove(name) {
            let reqs = data.pass.resource_requirements();
            self.budget.release(&reqs);
            self.rebuild_order();
        }

        // Clean up related state
        self.disabled_passes.remove(name);
        self.failure_counts.remove(name);
        self.active_fallbacks.remove(name);
    }

    /// Rebuild execution order based on dependencies using topological sort
    fn rebuild_order(&mut self) {
        let mut order = Vec::new();
        let mut visited = BTreeSet::new();
        let mut temp = BTreeSet::new();

        // Collect all pass names
        let pass_names: Vec<String> = self.passes.keys().cloned().collect();

        for name in &pass_names {
            if !visited.contains(name) {
                if let Err(e) = self.visit_pass(name, &mut visited, &mut temp, &mut order) {
                    // Log error but continue with partial order
                    let _ = e; // Suppress unused warning
                    continue;
                }
            }
        }

        self.order = order;
    }

    /// Recursive helper for topological sort
    fn visit_pass(
        &self,
        name: &str,
        visited: &mut BTreeSet<String>,
        temp: &mut BTreeSet<String>,
        order: &mut Vec<String>,
    ) -> Result<(), PassError> {
        if temp.contains(name) {
            return Err(PassError::Dependency(alloc::format!(
                "Circular dependency detected: {}",
                name
            )));
        }
        if visited.contains(name) {
            return Ok(());
        }

        temp.insert(name.to_string());

        if let Some(data) = self.passes.get(name) {
            for dep in data.pass.dependencies() {
                self.visit_pass(dep, visited, temp, order)?;
            }
        }

        temp.remove(name);
        visited.insert(name.to_string());
        order.push(name.to_string());

        Ok(())
    }

    /// Execute all passes in order
    pub fn execute(&self, context: &mut PassExecuteContext) -> Result<(), PassError> {
        for name in &self.order {
            if !self.should_execute(name) {
                continue;
            }

            if let Some(data) = self.passes.get(name) {
                if !data.enabled {
                    continue;
                }

                data.pass.execute(context)?;
            }
        }
        Ok(())
    }

    /// Execute all passes with fault isolation
    pub fn execute_with_recovery(
        &mut self,
        context: &mut PassExecuteContext,
    ) -> Result<(), PassError> {
        let mut failed_passes = Vec::new();

        for name in self.order.clone() {
            if !self.should_execute(&name) {
                continue;
            }

            if let Some(data) = self.passes.get(&name) {
                if !data.enabled {
                    continue;
                }

                // Execute the pass
                let result = data.pass.execute(context);

                match result {
                    Ok(()) => {
                        // Success - reset failure count
                        self.failure_counts.remove(&name);
                    }
                    Err(e) => {
                        failed_passes.push((name.clone(), alloc::format!("{:?}", e)));
                        self.handle_pass_failure(&name, &e);
                    }
                }
            }
        }

        if failed_passes.is_empty() {
            Ok(())
        } else {
            Err(PassError::PartialFailure(failed_passes))
        }
    }

    /// Handle a pass failure
    fn handle_pass_failure(&mut self, pass_name: &str, _error: &PassError) {
        // Track failure count
        let count = self
            .failure_counts
            .entry(pass_name.to_string())
            .and_modify(|c| *c += 1)
            .or_insert(1);

        if *count >= self.max_failures {
            // Disable pass after repeated failures
            self.disabled_passes.insert(pass_name.to_string());

            // Try to activate fallback
            if let Some(fallback) = self.fallbacks.get(pass_name).cloned() {
                self.active_fallbacks
                    .insert(pass_name.to_string(), fallback);
            }
        }
    }

    /// Check if pass should execute (not disabled)
    fn should_execute(&self, pass_name: &str) -> bool {
        !self.disabled_passes.contains(pass_name)
    }

    /// Register a fallback pass for another pass
    pub fn register_fallback(&mut self, pass_name: &str, fallback_name: &str) {
        self.fallbacks
            .insert(pass_name.to_string(), fallback_name.to_string());
    }

    /// Reset failure state for a pass (e.g., after hot-reload)
    pub fn reset_failure_state(&mut self, pass_name: &str) {
        self.failure_counts.remove(pass_name);
        self.disabled_passes.remove(pass_name);
        self.active_fallbacks.remove(pass_name);
    }

    /// Reset all failure states
    pub fn reset_all_failures(&mut self) {
        self.failure_counts.clear();
        self.disabled_passes.clear();
        self.active_fallbacks.clear();
    }

    /// Group passes for bulk enable/disable
    pub fn create_group(&mut self, group_name: &str, passes: Vec<String>) {
        self.groups.insert(group_name.to_string(), passes);
    }

    /// Remove a group
    pub fn remove_group(&mut self, group_name: &str) {
        self.groups.remove(group_name);
    }

    /// Enable/disable all passes in a group
    pub fn set_group_enabled(&mut self, group_name: &str, enabled: bool) {
        if let Some(passes) = self.groups.get(group_name).cloned() {
            for pass_name in passes {
                if let Some(data) = self.passes.get_mut(&pass_name) {
                    data.enabled = enabled;
                }
            }
        }
    }

    /// Enable/disable a single pass
    pub fn set_pass_enabled(&mut self, pass_name: &str, enabled: bool) {
        if let Some(data) = self.passes.get_mut(pass_name) {
            data.enabled = enabled;
        }
    }

    /// Check if a pass is enabled
    pub fn is_pass_enabled(&self, pass_name: &str) -> bool {
        self.passes
            .get(pass_name)
            .map(|d| d.enabled)
            .unwrap_or(false)
    }

    /// Get execution order
    pub fn order(&self) -> &[String] {
        &self.order
    }

    /// Get mutable reference to a pass
    pub fn get_pass_mut(&mut self, name: &str) -> Option<&mut dyn CustomRenderPass> {
        match self.passes.get_mut(name) {
            Some(data) => Some(data.pass.as_mut()),
            None => None,
        }
    }

    /// Get reference to a pass
    pub fn get_pass(&self, name: &str) -> Option<&dyn CustomRenderPass> {
        match self.passes.get(name) {
            Some(data) => Some(data.pass.as_ref()),
            None => None,
        }
    }

    /// Check if a pass is registered
    pub fn has_pass(&self, name: &str) -> bool {
        self.passes.contains_key(name)
    }

    /// Get all pass names
    pub fn pass_names(&self) -> impl Iterator<Item = &str> {
        self.passes.keys().map(|s| s.as_str())
    }

    /// Get budget reference
    pub fn budget(&self) -> &ResourceBudget {
        &self.budget
    }

    /// Get mutable budget reference
    pub fn budget_mut(&mut self) -> &mut ResourceBudget {
        &mut self.budget
    }

    /// Set maximum failures before disabling
    pub fn set_max_failures(&mut self, max: u32) {
        self.max_failures = max;
    }

    /// Get all group names
    pub fn group_names(&self) -> impl Iterator<Item = &str> {
        self.groups.keys().map(|s| s.as_str())
    }

    /// Get passes in a group
    pub fn group_passes(&self, group_name: &str) -> Option<&[String]> {
        self.groups.get(group_name).map(|v| v.as_slice())
    }

    /// Queue an update for frame-boundary application
    pub fn queue_update(&mut self, update: PassRegistryUpdate) {
        self.update_queue.queue(update);
    }

    /// Queue a pass for registration at frame boundary
    pub fn queue_register<P: CustomRenderPass + 'static>(&mut self, pass: P) {
        self.update_queue.queue_pass(Box::new(pass));
    }

    /// Apply all pending updates at frame boundary
    pub fn apply_pending_updates(&mut self, setup_ctx: &PassSetupContext) {
        // Take pending updates
        let updates = core::mem::take(&mut self.update_queue.pending);
        let mut pending_passes = core::mem::take(&mut self.update_queue.pending_passes);

        for update in updates {
            match update {
                PassRegistryUpdate::RegisterPending(_name) => {
                    // Pop the next pending pass
                    if let Some(mut pass) = pending_passes.pop() {
                        // Setup the pass
                        if let Err(e) = pass.setup(setup_ctx) {
                            let _ = e; // Log error but continue
                            continue;
                        }
                        let _ = self.register_boxed(pass);
                    }
                }
                PassRegistryUpdate::Unregister(name) => {
                    self.unregister(&name);
                }
                PassRegistryUpdate::Configure(name, config) => {
                    if let Some(data) = self.passes.get_mut(&name) {
                        let _ = data.pass.apply_config(&config);
                        data.enabled = config.enabled;
                    }
                }
                PassRegistryUpdate::Restore(state) => {
                    self.restore_from_state(state);
                }
                PassRegistryUpdate::RebuildPipelines(names) => {
                    for name in names {
                        if let Some(data) = self.passes.get_mut(&name) {
                            let _ = data.pass.setup(setup_ctx);
                        }
                    }
                }
                PassRegistryUpdate::SetEnabled(name, enabled) => {
                    self.set_pass_enabled(&name, enabled);
                }
                PassRegistryUpdate::SetGroupEnabled(name, enabled) => {
                    self.set_group_enabled(&name, enabled);
                }
            }
        }
    }

    /// Restore from serialized state
    fn restore_from_state(&mut self, state: CustomPassState) {
        // Restore enabled states
        for name in &state.enabled {
            self.set_pass_enabled(name, true);
        }

        // Apply configurations
        for config in &state.configs {
            if let Some(data) = self.passes.get_mut(&config.name) {
                let _ = data.pass.apply_config(config);
                data.enabled = config.enabled;
            }
        }

        // Restore groups from configs if needed
        // Groups are typically re-created by the application
    }

    /// Get state for serialization
    pub fn get_state(&self) -> CustomPassState {
        let configs: Vec<PassConfigData> = self
            .passes
            .iter()
            .filter_map(|(name, data)| {
                data.pass.get_config().or_else(|| {
                    Some(PassConfigData {
                        name: name.clone(),
                        enabled: data.enabled,
                        priority: data.pass.priority().0,
                        config: serde_json::Value::Null,
                    })
                })
            })
            .collect();

        let enabled: Vec<String> = self
            .passes
            .iter()
            .filter(|(_, d)| d.enabled)
            .map(|(n, _)| n.clone())
            .collect();

        CustomPassState {
            configs,
            enabled,
            order: self.order.clone(),
        }
    }

    /// Setup all passes
    pub fn setup_all(&mut self, ctx: &PassSetupContext) -> Result<(), PassError> {
        let mut errors = Vec::new();

        for (name, data) in &mut self.passes {
            if let Err(e) = data.pass.setup(ctx) {
                errors.push((name.clone(), alloc::format!("{:?}", e)));
            }
        }

        if errors.is_empty() {
            Ok(())
        } else {
            Err(PassError::PartialFailure(errors))
        }
    }

    /// Cleanup all passes
    pub fn cleanup_all(&mut self) {
        for (_, data) in &mut self.passes {
            data.pass.cleanup();
        }
    }

    /// Handle resize for all passes
    pub fn on_resize(&mut self, new_size: (u32, u32)) {
        for (_, data) in &mut self.passes {
            data.pass.on_resize(new_size);
        }
    }

    /// Get statistics
    pub fn stats(&self) -> PassRegistryStats {
        let total_memory = self.budget.used_memory;
        let total_render_targets = self.budget.used_render_targets;

        let pass_count = self.passes.len();
        let enabled_count = self.passes.values().filter(|d| d.enabled).count();
        let disabled_count = self.disabled_passes.len();

        PassRegistryStats {
            pass_count,
            enabled_count,
            disabled_count,
            total_memory,
            total_render_targets,
            memory_budget: self.budget.max_memory_bytes,
            render_target_budget: self.budget.max_render_targets,
        }
    }
}

/// Statistics for the pass registry
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct PassRegistryStats {
    /// Total number of registered passes
    pub pass_count: usize,
    /// Number of enabled passes
    pub enabled_count: usize,
    /// Number of disabled passes (due to failures)
    pub disabled_count: usize,
    /// Total memory used
    pub total_memory: u64,
    /// Total render targets used
    pub total_render_targets: u32,
    /// Memory budget
    pub memory_budget: u64,
    /// Render target budget
    pub render_target_budget: u32,
}

/// Serializable state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PassRegistryState {
    /// Pass configurations
    pub pass_configs: Vec<PassConfigData>,
    /// Groups
    pub groups: BTreeMap<String, Vec<String>>,
    /// Budget settings
    pub budget: ResourceBudget,
    /// Enabled passes
    pub enabled_passes: Vec<String>,
}

impl PassRegistry {
    /// Serialize state for hot-reload
    pub fn serialize_state(&self) -> PassRegistryState {
        let pass_configs: Vec<PassConfigData> = self
            .passes
            .iter()
            .filter_map(|(name, data)| {
                data.pass.get_config().or_else(|| {
                    Some(PassConfigData {
                        name: name.clone(),
                        enabled: data.enabled,
                        priority: data.pass.priority().0,
                        config: serde_json::Value::Null,
                    })
                })
            })
            .collect();

        let enabled_passes: Vec<String> = self
            .passes
            .iter()
            .filter(|(_, d)| d.enabled)
            .map(|(n, _)| n.clone())
            .collect();

        PassRegistryState {
            pass_configs,
            groups: self.groups.clone(),
            budget: self.budget.clone(),
            enabled_passes,
        }
    }

    /// Deserialize and restore state
    pub fn deserialize_state(&mut self, state: &PassRegistryState) {
        // Restore budget settings (not usage)
        self.budget.max_memory_bytes = state.budget.max_memory_bytes;
        self.budget.max_render_targets = state.budget.max_render_targets;
        self.budget.max_time_ms = state.budget.max_time_ms;

        // Restore groups
        self.groups = state.groups.clone();

        // Apply configurations and enabled states
        for config in &state.pass_configs {
            if let Some(data) = self.passes.get_mut(&config.name) {
                let _ = data.pass.apply_config(config);
            }
        }

        // Set enabled states
        for (name, data) in &mut self.passes {
            data.enabled = state.enabled_passes.contains(name);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestPass {
        name: String,
        deps: Vec<&'static str>,
        enabled: bool,
        reqs: ResourceRequirements,
    }

    impl TestPass {
        fn new(name: &str) -> Self {
            Self {
                name: name.to_string(),
                deps: Vec::new(),
                enabled: true,
                reqs: ResourceRequirements::default(),
            }
        }

        fn with_deps(mut self, deps: Vec<&'static str>) -> Self {
            self.deps = deps;
            self
        }

        fn with_memory(mut self, bytes: u64) -> Self {
            self.reqs.memory_bytes = bytes;
            self
        }
    }

    impl CustomRenderPass for TestPass {
        fn name(&self) -> &str {
            &self.name
        }

        fn dependencies(&self) -> &[&str] {
            &self.deps
        }

        fn execute(&self, _context: &PassExecuteContext) -> Result<(), PassError> {
            if self.enabled {
                Ok(())
            } else {
                Err(PassError::Disabled(self.name.clone()))
            }
        }

        fn resource_requirements(&self) -> ResourceRequirements {
            self.reqs.clone()
        }

        fn is_enabled(&self) -> bool {
            self.enabled
        }
    }

    #[test]
    fn test_pass_registration() {
        let mut registry = PassRegistry::new();

        let pass = TestPass::new("test_pass");
        assert!(registry.register(pass).is_ok());

        assert!(registry.has_pass("test_pass"));
        assert_eq!(registry.order(), &["test_pass"]);
    }

    #[test]
    fn test_pass_unregistration() {
        let mut registry = PassRegistry::new();

        registry.register(TestPass::new("pass1")).unwrap();
        registry.register(TestPass::new("pass2")).unwrap();

        assert!(registry.has_pass("pass1"));
        assert!(registry.has_pass("pass2"));

        registry.unregister("pass1");

        assert!(!registry.has_pass("pass1"));
        assert!(registry.has_pass("pass2"));
    }

    #[test]
    fn test_dependency_ordering() {
        let mut registry = PassRegistry::new();

        // Register B first, which depends on A
        let pass_b = TestPass::new("b").with_deps(vec!["a"]);
        let pass_a = TestPass::new("a");

        registry.register(pass_b).unwrap();
        registry.register(pass_a).unwrap();

        // A should come before B
        assert_eq!(registry.order(), &["a", "b"]);
    }

    #[test]
    fn test_complex_dependency_ordering() {
        let mut registry = PassRegistry::new();

        // C depends on A and B, B depends on A
        let pass_c = TestPass::new("c").with_deps(vec!["a", "b"]);
        let pass_b = TestPass::new("b").with_deps(vec!["a"]);
        let pass_a = TestPass::new("a");

        registry.register(pass_c).unwrap();
        registry.register(pass_b).unwrap();
        registry.register(pass_a).unwrap();

        let order = registry.order();

        // A must come before B and C
        let a_pos = order.iter().position(|x| x == "a").unwrap();
        let b_pos = order.iter().position(|x| x == "b").unwrap();
        let c_pos = order.iter().position(|x| x == "c").unwrap();

        assert!(a_pos < b_pos);
        assert!(a_pos < c_pos);
        assert!(b_pos < c_pos);
    }

    #[test]
    fn test_budget_enforcement() {
        let mut registry = PassRegistry::with_budget(ResourceBudget {
            max_memory_bytes: 100,
            max_render_targets: 2,
            max_time_ms: 8.0,
            ..Default::default()
        });

        // This should fit
        let small_pass = TestPass::new("small").with_memory(50);
        assert!(registry.register(small_pass).is_ok());

        // This should exceed budget
        let big_pass = TestPass::new("big").with_memory(100);
        assert!(registry.register(big_pass).is_err());
    }

    #[test]
    fn test_budget_release_on_unregister() {
        let mut registry = PassRegistry::with_budget(ResourceBudget {
            max_memory_bytes: 100,
            max_render_targets: 2,
            max_time_ms: 8.0,
            ..Default::default()
        });

        let pass = TestPass::new("pass").with_memory(60);
        registry.register(pass).unwrap();

        assert_eq!(registry.budget().used_memory, 60);

        registry.unregister("pass");

        assert_eq!(registry.budget().used_memory, 0);
    }

    #[test]
    fn test_pass_groups() {
        let mut registry = PassRegistry::new();

        registry.register(TestPass::new("bloom")).unwrap();
        registry.register(TestPass::new("outline")).unwrap();
        registry.register(TestPass::new("ssao")).unwrap();

        registry.create_group("post_effects", vec!["bloom".into(), "outline".into()]);

        assert!(registry.is_pass_enabled("bloom"));
        assert!(registry.is_pass_enabled("outline"));

        registry.set_group_enabled("post_effects", false);

        assert!(!registry.is_pass_enabled("bloom"));
        assert!(!registry.is_pass_enabled("outline"));
        assert!(registry.is_pass_enabled("ssao")); // Not in group
    }

    #[test]
    fn test_pass_enable_disable() {
        let mut registry = PassRegistry::new();

        registry.register(TestPass::new("pass")).unwrap();

        assert!(registry.is_pass_enabled("pass"));

        registry.set_pass_enabled("pass", false);
        assert!(!registry.is_pass_enabled("pass"));

        registry.set_pass_enabled("pass", true);
        assert!(registry.is_pass_enabled("pass"));
    }

    #[test]
    fn test_state_serialization() {
        let mut registry = PassRegistry::new();

        registry.register(TestPass::new("bloom")).unwrap();
        registry.register(TestPass::new("outline")).unwrap();
        registry.create_group("effects", vec!["bloom".into(), "outline".into()]);

        let state = registry.serialize_state();

        assert_eq!(state.groups.len(), 1);
        assert!(state.groups.contains_key("effects"));
        assert_eq!(state.enabled_passes.len(), 2);
    }

    #[test]
    fn test_stats() {
        let mut registry = PassRegistry::new();

        registry
            .register(TestPass::new("pass1").with_memory(100))
            .unwrap();
        registry
            .register(TestPass::new("pass2").with_memory(200))
            .unwrap();
        registry.set_pass_enabled("pass2", false);

        let stats = registry.stats();

        assert_eq!(stats.pass_count, 2);
        assert_eq!(stats.enabled_count, 1);
        assert_eq!(stats.total_memory, 300);
    }

    #[test]
    fn test_failure_handling() {
        let mut registry = PassRegistry::new();
        registry.set_max_failures(2);

        registry.register(TestPass::new("failing_pass")).unwrap();
        registry.register_fallback("failing_pass", "fallback_pass");

        // Simulate failures
        let error = PassError::Execute("test error".into());
        registry.handle_pass_failure("failing_pass", &error);
        registry.handle_pass_failure("failing_pass", &error);

        // After max failures, pass should be disabled
        assert!(registry.disabled_passes.contains("failing_pass"));
        assert!(registry.active_fallbacks.contains_key("failing_pass"));
    }

    #[test]
    fn test_reset_failures() {
        let mut registry = PassRegistry::new();
        registry.set_max_failures(1);

        registry.register(TestPass::new("pass")).unwrap();

        let error = PassError::Execute("test".into());
        registry.handle_pass_failure("pass", &error);

        assert!(registry.disabled_passes.contains("pass"));

        registry.reset_failure_state("pass");

        assert!(!registry.disabled_passes.contains("pass"));
        assert!(registry.failure_counts.get("pass").is_none());
    }
}
