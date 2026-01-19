//! Bridge between editor entities and void_ecs World.

use void_ecs::{World, Entity, WorldExt};
use crate::core::{EntityId, SceneEntity};
use crate::core::editor_state::{NameComponent, TransformComponent, MeshComponent};

/// Bridge for synchronizing editor state with void_ecs World.
pub struct EcsBridge;

impl EcsBridge {
    /// Synchronize an editor entity to the ECS world.
    pub fn sync_to_ecs(world: &mut World, entity: &mut SceneEntity) {
        if let Some(ecs_entity) = entity.ecs_entity {
            // Update ECS components
            if let Some(name) = world.get_component_mut::<NameComponent>(ecs_entity) {
                name.0 = entity.name.clone();
            }

            if let Some(transform) = world.get_component_mut::<TransformComponent>(ecs_entity) {
                transform.position = entity.transform.position;
                transform.rotation = entity.transform.rotation;
                transform.scale = entity.transform.scale;
            }

            if let Some(mesh) = world.get_component_mut::<MeshComponent>(ecs_entity) {
                mesh.mesh_type = entity.mesh_type;
                mesh.color = entity.color;
            }
        }
    }

    /// Synchronize from ECS world to editor entity.
    pub fn sync_from_ecs(world: &World, entity: &mut SceneEntity) {
        if let Some(ecs_entity) = entity.ecs_entity {
            if let Some(name) = world.get_component::<NameComponent>(ecs_entity) {
                entity.name = name.0.clone();
            }

            if let Some(transform) = world.get_component::<TransformComponent>(ecs_entity) {
                entity.transform.position = transform.position;
                entity.transform.rotation = transform.rotation;
                entity.transform.scale = transform.scale;
            }

            if let Some(mesh) = world.get_component::<MeshComponent>(ecs_entity) {
                entity.mesh_type = mesh.mesh_type;
                entity.color = mesh.color;
            }
        }
    }

    /// Create a new ECS entity for an editor entity.
    pub fn create_ecs_entity(world: &mut World, entity: &SceneEntity) -> Entity {
        world.build_entity()
            .with(NameComponent(entity.name.clone()))
            .with(TransformComponent {
                position: entity.transform.position,
                rotation: entity.transform.rotation,
                scale: entity.transform.scale,
            })
            .with(MeshComponent {
                mesh_type: entity.mesh_type,
                color: entity.color,
            })
            .build()
    }

    /// Despawn an ECS entity.
    pub fn despawn_ecs_entity(world: &mut World, entity: &SceneEntity) {
        if let Some(ecs_entity) = entity.ecs_entity {
            world.despawn(ecs_entity);
        }
    }
}
