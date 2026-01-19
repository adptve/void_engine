//! System - Logic that operates on components
//!
//! Systems are functions or closures that process entities with specific components.
//! They can be scheduled to run in parallel when they don't conflict.

use crate::component::ComponentId;
use crate::query::{Access, QueryDescriptor};
use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use alloc::collections::BTreeSet;
use core::any::TypeId;
use void_core::Id;

/// Unique identifier for a system
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct SystemId(pub Id);

impl SystemId {
    /// Create a new system ID
    pub fn new(name: &str) -> Self {
        Self(Id::from_name(name))
    }

    /// Get the raw ID
    pub fn id(&self) -> Id {
        self.0
    }
}

/// System execution stage
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum SystemStage {
    /// First stage - initialization, input handling
    First,
    /// Pre-update - before main game logic
    PreUpdate,
    /// Main update - game logic
    Update,
    /// Post-update - after main game logic
    PostUpdate,
    /// Pre-render - before rendering
    PreRender,
    /// Render - actual rendering
    Render,
    /// Post-render - after rendering
    PostRender,
    /// Last stage - cleanup
    Last,
}

impl Default for SystemStage {
    fn default() -> Self {
        Self::Update
    }
}

/// Resource access for a system
#[derive(Clone, Debug)]
pub struct ResourceAccess {
    /// Resource type ID
    pub type_id: TypeId,
    /// Access mode
    pub access: Access,
}

/// Describes a system's resource and component requirements
#[derive(Clone, Debug, Default)]
pub struct SystemDescriptor {
    /// System name
    pub name: String,
    /// Execution stage
    pub stage: SystemStage,
    /// Query descriptors
    pub queries: Vec<QueryDescriptor>,
    /// Resource accesses
    pub resources: Vec<ResourceAccess>,
    /// Systems that must run before this one
    pub run_after: Vec<SystemId>,
    /// Systems that must run after this one
    pub run_before: Vec<SystemId>,
    /// Whether this system can run in parallel with others
    pub exclusive: bool,
}

impl SystemDescriptor {
    /// Create a new system descriptor
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            stage: SystemStage::default(),
            queries: Vec::new(),
            resources: Vec::new(),
            run_after: Vec::new(),
            run_before: Vec::new(),
            exclusive: false,
        }
    }

    /// Set the execution stage
    pub fn stage(mut self, stage: SystemStage) -> Self {
        self.stage = stage;
        self
    }

    /// Add a query
    pub fn query(mut self, query: QueryDescriptor) -> Self {
        self.queries.push(query);
        self
    }

    /// Add a resource read
    pub fn read_resource<R: 'static>(mut self) -> Self {
        self.resources.push(ResourceAccess {
            type_id: TypeId::of::<R>(),
            access: Access::Read,
        });
        self
    }

    /// Add a resource write
    pub fn write_resource<R: 'static>(mut self) -> Self {
        self.resources.push(ResourceAccess {
            type_id: TypeId::of::<R>(),
            access: Access::Write,
        });
        self
    }

    /// System must run after another
    pub fn after(mut self, system: SystemId) -> Self {
        self.run_after.push(system);
        self
    }

    /// System must run before another
    pub fn before(mut self, system: SystemId) -> Self {
        self.run_before.push(system);
        self
    }

    /// Mark as exclusive (can't run in parallel)
    pub fn exclusive(mut self) -> Self {
        self.exclusive = true;
        self
    }

    /// Check if this system conflicts with another
    pub fn conflicts_with(&self, other: &SystemDescriptor) -> bool {
        // Exclusive systems always conflict
        if self.exclusive || other.exclusive {
            return true;
        }

        // Check resource conflicts
        for res1 in &self.resources {
            for res2 in &other.resources {
                if res1.type_id == res2.type_id {
                    // Conflict if either is writing
                    if matches!(res1.access, Access::Write)
                        || matches!(res2.access, Access::Write)
                    {
                        return true;
                    }
                }
            }
        }

        // Check query conflicts (simplified - component-level)
        for q1 in &self.queries {
            for q2 in &other.queries {
                for a1 in q1.accesses() {
                    for a2 in q2.accesses() {
                        if a1.id == a2.id {
                            let is_write1 = matches!(a1.access, Access::Write | Access::OptionalWrite);
                            let is_write2 = matches!(a2.access, Access::Write | Access::OptionalWrite);
                            if is_write1 || is_write2 {
                                return true;
                            }
                        }
                    }
                }
            }
        }

        false
    }
}

/// Trait for systems that can be executed
pub trait System: Send + Sync {
    /// Get the system descriptor
    fn descriptor(&self) -> &SystemDescriptor;

    /// Run the system
    fn run(&mut self, world: &mut dyn SystemWorld);

    /// Called when the system is added to the world
    fn on_add(&mut self, _world: &dyn SystemWorld) {}

    /// Called when the system is removed from the world
    fn on_remove(&mut self, _world: &dyn SystemWorld) {}
}

/// World interface for systems
pub trait SystemWorld {
    /// Get component data (immutable)
    fn get_component_ptr(&self, entity_id: u64, component_id: ComponentId) -> Option<*const u8>;

    /// Get component data (mutable)
    fn get_component_ptr_mut(&self, entity_id: u64, component_id: ComponentId) -> Option<*mut u8>;

    /// Check if entity has component
    fn has_component(&self, entity_id: u64, component_id: ComponentId) -> bool;

    /// Get resource (immutable)
    fn get_resource_ptr(&self, type_id: TypeId) -> Option<*const u8>;

    /// Get resource (mutable)
    fn get_resource_ptr_mut(&self, type_id: TypeId) -> Option<*mut u8>;
}

/// Function system wrapper
pub struct FunctionSystem<F> {
    descriptor: SystemDescriptor,
    func: F,
}

impl<F> FunctionSystem<F>
where
    F: FnMut(&mut dyn SystemWorld) + Send + Sync,
{
    /// Create a new function system
    pub fn new(descriptor: SystemDescriptor, func: F) -> Self {
        Self { descriptor, func }
    }
}

impl<F> System for FunctionSystem<F>
where
    F: FnMut(&mut dyn SystemWorld) + Send + Sync,
{
    fn descriptor(&self) -> &SystemDescriptor {
        &self.descriptor
    }

    fn run(&mut self, world: &mut dyn SystemWorld) {
        (self.func)(world);
    }
}

/// System scheduler for ordering and parallel execution
pub struct SystemScheduler {
    /// Systems by stage
    stages: [Vec<Box<dyn System>>; 8],
    /// System descriptors for conflict detection
    descriptors: Vec<SystemDescriptor>,
    /// Dirty flag for re-scheduling
    dirty: bool,
}

impl SystemScheduler {
    /// Create a new scheduler
    pub fn new() -> Self {
        Self {
            stages: Default::default(),
            descriptors: Vec::new(),
            dirty: false,
        }
    }

    /// Add a system
    pub fn add_system(&mut self, system: Box<dyn System>) {
        let stage = system.descriptor().stage;
        let stage_index = match stage {
            SystemStage::First => 0,
            SystemStage::PreUpdate => 1,
            SystemStage::Update => 2,
            SystemStage::PostUpdate => 3,
            SystemStage::PreRender => 4,
            SystemStage::Render => 5,
            SystemStage::PostRender => 6,
            SystemStage::Last => 7,
        };

        self.descriptors.push(system.descriptor().clone());
        self.stages[stage_index].push(system);
        self.dirty = true;
    }

    /// Run all systems in order
    pub fn run(&mut self, world: &mut dyn SystemWorld) {
        for stage in &mut self.stages {
            for system in stage.iter_mut() {
                system.run(world);
            }
        }
    }

    /// Get systems in a stage
    pub fn systems_in_stage(&self, stage: SystemStage) -> &[Box<dyn System>] {
        let stage_index = match stage {
            SystemStage::First => 0,
            SystemStage::PreUpdate => 1,
            SystemStage::Update => 2,
            SystemStage::PostUpdate => 3,
            SystemStage::PreRender => 4,
            SystemStage::Render => 5,
            SystemStage::PostRender => 6,
            SystemStage::Last => 7,
        };
        &self.stages[stage_index]
    }

    /// Get total number of systems
    pub fn len(&self) -> usize {
        self.stages.iter().map(|s| s.len()).sum()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.stages.iter().all(|s| s.is_empty())
    }
}

impl Default for SystemScheduler {
    fn default() -> Self {
        Self::new()
    }
}

/// Parallel system batch
pub struct SystemBatch {
    /// System indices in this batch
    systems: Vec<usize>,
}

impl SystemBatch {
    /// Create a new batch
    pub fn new() -> Self {
        Self { systems: Vec::new() }
    }

    /// Add a system to the batch
    pub fn add(&mut self, index: usize) {
        self.systems.push(index);
    }

    /// Get systems in this batch
    pub fn systems(&self) -> &[usize] {
        &self.systems
    }
}

impl Default for SystemBatch {
    fn default() -> Self {
        Self::new()
    }
}

/// Create system batches for parallel execution
pub fn create_parallel_batches(descriptors: &[SystemDescriptor]) -> Vec<SystemBatch> {
    let mut batches = Vec::new();
    let mut scheduled: BTreeSet<usize> = BTreeSet::new();

    while scheduled.len() < descriptors.len() {
        let mut batch = SystemBatch::new();
        let mut batch_descriptors: Vec<&SystemDescriptor> = Vec::new();

        for (i, desc) in descriptors.iter().enumerate() {
            if scheduled.contains(&i) {
                continue;
            }

            // Check dependencies
            let deps_satisfied = desc.run_after.iter().all(|dep| {
                descriptors.iter().position(|d| Id::from_name(&d.name) == dep.0)
                    .map(|j| scheduled.contains(&j))
                    .unwrap_or(true)
            });

            if !deps_satisfied {
                continue;
            }

            // Check conflicts with current batch
            let has_conflict = batch_descriptors.iter().any(|bd| desc.conflicts_with(bd));

            if !has_conflict {
                batch.add(i);
                batch_descriptors.push(desc);
                scheduled.insert(i);
            }
        }

        if batch.systems.is_empty() {
            // Deadlock or circular dependency - force schedule one
            for i in 0..descriptors.len() {
                if !scheduled.contains(&i) {
                    batch.add(i);
                    scheduled.insert(i);
                    break;
                }
            }
        }

        if !batch.systems.is_empty() {
            batches.push(batch);
        }
    }

    batches
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_system_descriptor() {
        let desc = SystemDescriptor::new("test_system")
            .stage(SystemStage::Update);

        assert_eq!(desc.name, "test_system");
        assert_eq!(desc.stage, SystemStage::Update);
    }

    #[test]
    fn test_system_conflicts() {
        struct Resource1;
        struct Resource2;

        let desc1 = SystemDescriptor::new("system1")
            .write_resource::<Resource1>();

        let desc2 = SystemDescriptor::new("system2")
            .read_resource::<Resource1>();

        let desc3 = SystemDescriptor::new("system3")
            .read_resource::<Resource2>();

        // desc1 and desc2 conflict (write/read on same resource)
        assert!(desc1.conflicts_with(&desc2));

        // desc1 and desc3 don't conflict (different resources)
        assert!(!desc1.conflicts_with(&desc3));

        // desc2 and desc3 don't conflict (different resources)
        assert!(!desc2.conflicts_with(&desc3));
    }

    #[test]
    fn test_parallel_batches() {
        struct Res;

        let descriptors = vec![
            SystemDescriptor::new("sys1").read_resource::<Res>(),
            SystemDescriptor::new("sys2").read_resource::<Res>(),
            SystemDescriptor::new("sys3").write_resource::<Res>(),
        ];

        let batches = create_parallel_batches(&descriptors);

        // sys1 and sys2 can run in parallel (both read)
        // sys3 must be separate (writes)
        assert!(batches.len() >= 2);
    }
}
