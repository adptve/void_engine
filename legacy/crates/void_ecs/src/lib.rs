//! # void_ecs - Archetype-based Entity Component System
//!
//! High-performance ECS with:
//! - Archetype storage for cache-efficient iteration
//! - Dynamic component registration (plugins can add new types)
//! - Parallel system execution with conflict detection
//! - Generational entity IDs for use-after-free safety
//!
//! ## Example
//!
//! ```ignore
//! use void_ecs::prelude::*;
//!
//! // Define components
//! #[derive(Clone, Copy)]
//! struct Position { x: f32, y: f32 }
//!
//! #[derive(Clone, Copy)]
//! struct Velocity { x: f32, y: f32 }
//!
//! // Create world
//! let mut world = World::new();
//!
//! // Register components
//! world.register_component::<Position>();
//! world.register_component::<Velocity>();
//!
//! // Spawn entities
//! let entity = world.spawn();
//! world.add_component(entity, Position { x: 0.0, y: 0.0 });
//! world.add_component(entity, Velocity { x: 1.0, y: 0.5 });
//!
//! // Query and update
//! let pos_id = world.component_id::<Position>().unwrap();
//! let vel_id = world.component_id::<Velocity>().unwrap();
//!
//! let query = QueryDescriptor::new()
//!     .write(pos_id)
//!     .read(vel_id)
//!     .build();
//!
//! // ... iterate and update positions
//! ```

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

pub mod entity;
pub mod component;
pub mod archetype;
pub mod query;
pub mod system;
pub mod world;
pub mod render_components;

// Phase 1: Scene Graph - Hierarchy Components and Systems
pub mod hierarchy;
pub mod hierarchy_system;
pub mod hierarchy_commands;

// Phase 2: Camera System
pub mod camera;
pub mod camera_system;

// Phase 8: Keyframe Animation
pub mod animation;

// Phase 9: Animation Blending
pub mod animation_blending;

// Phase 11: Entity-Level Input Events
pub mod input_events;
pub mod input_handler;

// Phase 12: Multi-Pass Rendering
pub mod render_pass;

// Phase 15: Level of Detail
pub mod lod;

// Phase 16: Scene Streaming
pub mod streaming;

// Phase 17: World-Space Precision Management
pub mod precision;

pub use entity::{Entity, EntityAllocator};
pub use component::{Component, ComponentId, ComponentInfo, ComponentRegistry, ComponentStorage};
pub use archetype::{Archetype, ArchetypeId, Archetypes};
pub use query::{Access, ComponentAccess, QueryDescriptor, QueryState, QueryIter};
pub use system::{System, SystemDescriptor, SystemId, SystemStage, SystemScheduler, SystemWorld};
pub use world::{World, EntityLocation};

// Phase 1: Scene Graph exports
pub use hierarchy::{
    Parent, Children, LocalTransform, GlobalTransform,
    InheritedVisibility, HierarchyDepth, HierarchyError,
};
pub use hierarchy_system::{
    HierarchyValidationSystem, TransformPropagationSystem,
    VisibilityPropagationSystem, HierarchyUpdateQueue,
};
pub use hierarchy_commands::{HierarchyCommands, HierarchyBuilder, ChildBuilder};

// Phase 2: Camera System exports
pub use camera::{
    Camera, Projection, Viewport, CameraAnimation, EasingFunction,
    perspective_matrix, orthographic_matrix, multiply_matrices,
    invert_transform_matrix, lerp, IDENTITY_MATRIX,
};
pub use camera_system::{CameraManager, CameraRenderData, CameraUpdateQueue};

// Phase 8: Keyframe Animation exports
pub use animation::{
    // Core types
    KeyframeValue, Keyframe, Interpolation, AnimatedProperty,
    // Track and clip
    AnimationTrack, AnimationClip, AnimationEvent, AnimationPose,
    // Player
    AnimationPlayer, AnimationState, LoopMode,
    // Functions
    lerp_value, slerp, normalize_quat,
    // Hot-reload state
    AnimationPlayerState, AnimationClipState,
};

// Phase 9: Animation Blending exports
pub use animation_blending::{
    // Bone Mask
    BoneMask, LayerBlendMode,
    // Layers
    AnimationLayer, AnimationLayers, LayerState, AnimationLayersState,
    // Blend Trees
    BlendTree, BlendNode, BlendChild1D, BlendChild2D, BlendChildDirect,
    Blend2DType, BlendTreeState,
    // State Machine
    AnimationStateMachine, AnimState, AnimMotion, AnimTransition,
    TransitionCondition, Comparison, InterruptionSource, ActiveTransition,
    AnimParameter, StateMachineState,
    // Functions
    blend_poses, apply_additive, apply_bone_mask, multiply_quat,
};

// Phase 11: Entity-Level Input Events exports
pub use input_events::{
    EntityInputEvent, EntityInputEvents, PointerButton,
};
pub use input_handler::{
    InputHandler, InputEventKind, CursorIcon, InputState as EntityInputState,
};

// Phase 12: Multi-Pass Rendering exports
pub use render_pass::{RenderPasses, RenderPassFlags};

// Phase 15: Level of Detail exports
pub use lod::{
    LodLevel, LodMode, LodFadeState, LodGroup,
    LodSystemConfig, LodUpdate, LodSystem, LodSystemStats, LodSystemState,
};

// Phase 16: Scene Streaming exports
pub use streaming::{
    ChunkId, ChunkState, SceneChunk, ChunkError,
    StreamingConfig, StreamingUpdate, StreamingManager, StreamingStats,
    EntityRecycler, StreamingManagerState, StreamingManagerUpdate,
    StreamingUpdateQueue, StreamingHealthStatus,
};

// Phase 17: Precision Management exports
pub use precision::{
    WorldOrigin, PrecisionPosition, OriginValidationError,
    PrecisionManager, PrecisionUpdate, PrecisionUpdateQueue,
    PrecisionStats, PrecisionSystemState,
    RebaseEvent, RebaseHistory, OriginRebaseConfig,
    RenderMode, determine_render_mode,
};

/// Prelude - commonly used types
pub mod prelude {
    pub use crate::entity::{Entity, EntityAllocator};
    pub use crate::component::{Component, ComponentId, ComponentInfo, ComponentRegistry};
    pub use crate::archetype::{Archetype, ArchetypeId, Archetypes};
    pub use crate::query::{Access, QueryDescriptor, QueryState};
    pub use crate::system::{System, SystemDescriptor, SystemId, SystemStage, SystemScheduler};
    pub use crate::world::{World, EntityLocation};
    pub use crate::{WorldExt, EntityBuilder, Bundle};

    // Phase 1: Scene Graph
    pub use crate::hierarchy::{
        Parent, Children, LocalTransform, GlobalTransform,
        InheritedVisibility, HierarchyDepth, HierarchyError,
    };
    pub use crate::hierarchy_system::{
        HierarchyValidationSystem, TransformPropagationSystem,
        VisibilityPropagationSystem, HierarchyUpdateQueue,
    };
    pub use crate::hierarchy_commands::{HierarchyCommands, HierarchyBuilder};

    // Phase 2: Camera System
    pub use crate::camera::{
        Camera, Projection, Viewport, CameraAnimation, EasingFunction,
    };
    pub use crate::camera_system::{CameraManager, CameraRenderData, CameraUpdateQueue};

    // Phase 8: Keyframe Animation
    pub use crate::animation::{
        KeyframeValue, Keyframe, Interpolation, AnimatedProperty,
        AnimationTrack, AnimationClip, AnimationEvent, AnimationPose,
        AnimationPlayer, AnimationState, LoopMode,
    };

    // Phase 9: Animation Blending
    pub use crate::animation_blending::{
        BoneMask, LayerBlendMode, AnimationLayer, AnimationLayers,
        BlendTree, BlendNode, Blend2DType,
        AnimationStateMachine, AnimState, AnimTransition, Comparison,
    };

    // Phase 11: Entity-Level Input Events
    pub use crate::input_events::{EntityInputEvent, PointerButton};
    pub use crate::input_handler::{InputHandler, InputEventKind, CursorIcon};

    // Phase 12: Multi-Pass Rendering
    pub use crate::render_pass::{RenderPasses, RenderPassFlags};

    // Phase 17: Precision Management
    pub use crate::precision::{
        WorldOrigin, PrecisionPosition, PrecisionManager,
        OriginRebaseConfig, RenderMode,
    };
}

/// Entity builder for ergonomic entity creation
pub struct EntityBuilder<'w> {
    world: &'w mut World,
    entity: Entity,
}

impl<'w> EntityBuilder<'w> {
    /// Create a new entity builder
    pub fn new(world: &'w mut World) -> Self {
        let entity = world.spawn();
        Self { world, entity }
    }

    /// Add a component to the entity
    pub fn with<T: Send + Sync + 'static>(self, component: T) -> Self {
        self.world.add_component(self.entity, component);
        self
    }

    /// Get the entity ID
    pub fn id(&self) -> Entity {
        self.entity
    }

    /// Finish building and return the entity
    pub fn build(self) -> Entity {
        self.entity
    }
}

/// Extension trait for World to provide builder pattern
pub trait WorldExt {
    /// Start building a new entity
    fn build_entity(&mut self) -> EntityBuilder<'_>;
}

impl WorldExt for World {
    fn build_entity(&mut self) -> EntityBuilder<'_> {
        EntityBuilder::new(self)
    }
}

/// Component bundle - group of components that are commonly added together
pub trait Bundle: Send + Sync + 'static {
    /// Get component IDs in this bundle
    fn component_ids(registry: &mut ComponentRegistry) -> alloc::vec::Vec<ComponentId>;

    /// Add components to entity
    fn add_to_entity(self, world: &mut World, entity: Entity);
}

/// Implement Bundle for tuples
macro_rules! impl_bundle_tuple {
    ($($T:ident),*) => {
        impl<$($T: Send + Sync + 'static),*> Bundle for ($($T,)*) {
            fn component_ids(registry: &mut ComponentRegistry) -> alloc::vec::Vec<ComponentId> {
                alloc::vec![$(registry.register::<$T>()),*]
            }

            #[allow(non_snake_case)]
            fn add_to_entity(self, world: &mut World, entity: Entity) {
                let ($($T,)*) = self;
                $(world.add_component(entity, $T);)*
            }
        }
    };
}

// Single element tuple
impl_bundle_tuple!(A);
// Multi-element tuples
impl_bundle_tuple!(A, B);
impl_bundle_tuple!(A, B, C);
impl_bundle_tuple!(A, B, C, D);
impl_bundle_tuple!(A, B, C, D, E);
impl_bundle_tuple!(A, B, C, D, E, F);
impl_bundle_tuple!(A, B, C, D, E, F, G);
impl_bundle_tuple!(A, B, C, D, E, F, G, H);

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Clone, Copy, Debug, PartialEq)]
    struct Position { x: f32, y: f32 }

    #[derive(Clone, Copy, Debug, PartialEq)]
    struct Velocity { x: f32, y: f32 }

    #[derive(Clone, Copy, Debug, PartialEq)]
    struct Health(f32);

    #[test]
    fn test_entity_builder() {
        let mut world = World::new();

        let entity = world.build_entity()
            .with(Position { x: 0.0, y: 0.0 })
            .with(Velocity { x: 1.0, y: 0.5 })
            .with(Health(100.0))
            .build();

        assert!(world.has_component::<Position>(entity));
        assert!(world.has_component::<Velocity>(entity));
        assert!(world.has_component::<Health>(entity));
    }

    #[test]
    fn test_bundle() {
        let mut world = World::new();

        let entity = world.spawn();
        let bundle = (
            Position { x: 5.0, y: 10.0 },
            Velocity { x: 1.0, y: 2.0 },
        );
        bundle.add_to_entity(&mut world, entity);

        assert!(world.has_component::<Position>(entity));
        assert!(world.has_component::<Velocity>(entity));
    }

    #[test]
    fn test_query_matching() {
        let mut world = World::new();

        // Register components
        let pos_id = world.register_component::<Position>();
        let vel_id = world.register_component::<Velocity>();
        let _health_id = world.register_component::<Health>();

        // Create entities with different component sets
        let e1 = world.spawn();
        world.add_component(e1, Position { x: 0.0, y: 0.0 });
        world.add_component(e1, Velocity { x: 1.0, y: 0.0 });

        let e2 = world.spawn();
        world.add_component(e2, Position { x: 5.0, y: 5.0 });

        let e3 = world.spawn();
        world.add_component(e3, Position { x: 10.0, y: 10.0 });
        world.add_component(e3, Velocity { x: 0.0, y: 1.0 });
        world.add_component(e3, Health(100.0));

        // Query for entities with Position and Velocity
        let query_desc = QueryDescriptor::new()
            .read(pos_id)
            .read(vel_id)
            .build();

        let query = world.query(query_desc);
        let matched: alloc::vec::Vec<_> = query.matched_archetypes().to_vec();

        // Count matching entities
        let mut count = 0;
        for arch_id in matched {
            if let Some(arch) = world.archetypes().get(arch_id) {
                count += arch.len();
            }
        }

        // e1 and e3 should match (have both Position and Velocity)
        assert_eq!(count, 2);
    }
}
