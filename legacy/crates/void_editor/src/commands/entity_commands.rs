//! Entity creation, deletion, and duplication commands.

use void_ecs::WorldExt;
use crate::core::{EditorState, EntityId, SceneEntity, MeshType, Transform};
use super::{Command, CommandResult, CommandError};

/// Command to create a new entity.
pub struct CreateEntityCommand {
    pub name: String,
    pub mesh_type: MeshType,
    pub transform: Transform,
    pub color: [f32; 3],
    // Filled after execution
    created_id: Option<EntityId>,
}

impl CreateEntityCommand {
    pub fn new(name: impl Into<String>, mesh_type: MeshType) -> Self {
        Self {
            name: name.into(),
            mesh_type,
            transform: Transform::new(),
            color: [0.8, 0.8, 0.8],
            created_id: None,
        }
    }

    pub fn with_transform(mut self, transform: Transform) -> Self {
        self.transform = transform;
        self
    }

    pub fn with_color(mut self, color: [f32; 3]) -> Self {
        self.color = color;
        self
    }

    pub fn created_id(&self) -> Option<EntityId> {
        self.created_id
    }
}

impl Command for CreateEntityCommand {
    fn description(&self) -> &str {
        "Create Entity"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        let id = state.create_entity(self.name.clone(), self.mesh_type);

        // Apply transform and color
        if let Some(entity) = state.get_entity_mut(id) {
            entity.transform = self.transform;
            entity.color = self.color;
        }

        self.created_id = Some(id);
        Ok(())
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        if let Some(id) = self.created_id {
            state.remove_entity(id);
            state.console.info(format!("Undid create: {}", self.name));
        }
        Ok(())
    }
}

/// Command to delete an entity.
pub struct DeleteEntityCommand {
    pub entity_id: EntityId,
    // Stored for undo
    deleted_entity: Option<SceneEntity>,
}

impl DeleteEntityCommand {
    pub fn new(entity_id: EntityId) -> Self {
        Self {
            entity_id,
            deleted_entity: None,
        }
    }
}

impl Command for DeleteEntityCommand {
    fn description(&self) -> &str {
        "Delete Entity"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        // Store the entity for undo
        self.deleted_entity = state.remove_entity(self.entity_id);

        if self.deleted_entity.is_some() {
            state.console.info("Deleted entity");
            Ok(())
        } else {
            Err(CommandError::EntityNotFound(self.entity_id))
        }
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        if let Some(entity) = self.deleted_entity.take() {
            let name = entity.name.clone();
            state.add_entity(entity);
            state.console.info(format!("Restored entity: {}", name));
            Ok(())
        } else {
            Err(CommandError::InvalidOperation("No entity to restore".to_string()))
        }
    }
}

/// Command to duplicate an entity.
pub struct DuplicateEntityCommand {
    pub source_id: EntityId,
    pub offset: [f32; 3],
    // Filled after execution
    created_id: Option<EntityId>,
}

impl DuplicateEntityCommand {
    pub fn new(source_id: EntityId) -> Self {
        Self {
            source_id,
            offset: [1.0, 0.0, 0.0],
            created_id: None,
        }
    }

    pub fn with_offset(mut self, offset: [f32; 3]) -> Self {
        self.offset = offset;
        self
    }

    pub fn created_id(&self) -> Option<EntityId> {
        self.created_id
    }
}

impl Command for DuplicateEntityCommand {
    fn description(&self) -> &str {
        "Duplicate Entity"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        let source = state.get_entity(self.source_id)
            .ok_or(CommandError::EntityNotFound(self.source_id))?
            .clone();

        let new_id = state.next_entity_id();

        // Create ECS entity
        let ecs_entity = state.ecs_world.build_entity()
            .with(crate::core::editor_state::NameComponent(format!("{} (Copy)", source.name)))
            .with(crate::core::editor_state::TransformComponent {
                position: [
                    source.transform.position[0] + self.offset[0],
                    source.transform.position[1] + self.offset[1],
                    source.transform.position[2] + self.offset[2],
                ],
                rotation: source.transform.rotation,
                scale: source.transform.scale,
            })
            .with(crate::core::editor_state::MeshComponent {
                mesh_type: source.mesh_type,
                color: source.color,
            })
            .build();

        let new_entity = SceneEntity {
            id: new_id,
            name: format!("{} (Copy)", source.name),
            transform: Transform {
                position: [
                    source.transform.position[0] + self.offset[0],
                    source.transform.position[1] + self.offset[1],
                    source.transform.position[2] + self.offset[2],
                ],
                rotation: source.transform.rotation,
                scale: source.transform.scale,
            },
            mesh_type: source.mesh_type,
            color: source.color,
            ecs_entity: Some(ecs_entity),
            ..Default::default()
        };

        state.console.info(format!("Duplicated entity: {}", new_entity.name));
        state.add_entity(new_entity);
        self.created_id = Some(new_id);

        Ok(())
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        if let Some(id) = self.created_id {
            state.remove_entity(id);
            state.console.info("Undid duplicate");
        }
        Ok(())
    }
}

/// Command to delete multiple entities.
pub struct DeleteMultipleCommand {
    pub entity_ids: Vec<EntityId>,
    deleted_entities: Vec<SceneEntity>,
}

impl DeleteMultipleCommand {
    pub fn new(entity_ids: Vec<EntityId>) -> Self {
        Self {
            entity_ids,
            deleted_entities: Vec::new(),
        }
    }
}

impl Command for DeleteMultipleCommand {
    fn description(&self) -> &str {
        "Delete Entities"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        self.deleted_entities.clear();

        for &id in &self.entity_ids {
            if let Some(entity) = state.remove_entity(id) {
                self.deleted_entities.push(entity);
            }
        }

        if !self.deleted_entities.is_empty() {
            state.console.info(format!("Deleted {} entities", self.deleted_entities.len()));
            Ok(())
        } else {
            Err(CommandError::InvalidOperation("No entities to delete".to_string()))
        }
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        // Restore in reverse order
        for entity in self.deleted_entities.drain(..).rev() {
            let name = entity.name.clone();
            state.add_entity(entity);
            state.console.info(format!("Restored entity: {}", name));
        }
        Ok(())
    }
}

/// Command to change an entity's parent in the hierarchy.
pub struct ReparentCommand {
    pub entity_id: EntityId,
    pub new_parent: Option<EntityId>,
    old_parent: Option<EntityId>,
}

impl ReparentCommand {
    pub fn new(entity_id: EntityId, new_parent: Option<EntityId>) -> Self {
        Self {
            entity_id,
            new_parent,
            old_parent: None,
        }
    }
}

impl Command for ReparentCommand {
    fn description(&self) -> &str {
        "Reparent Entity"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        let entity = state.get_entity_mut(self.entity_id)
            .ok_or(CommandError::EntityNotFound(self.entity_id))?;

        // Store old parent for undo
        self.old_parent = entity.parent;

        // Remove from old parent's children list
        if let Some(old_parent_id) = self.old_parent {
            if let Some(old_parent) = state.get_entity_mut(old_parent_id) {
                old_parent.children.retain(|&id| id != self.entity_id);
            }
        }

        // Re-fetch entity (borrow ended)
        let entity = state.get_entity_mut(self.entity_id)
            .ok_or(CommandError::EntityNotFound(self.entity_id))?;

        // Set new parent
        entity.parent = self.new_parent;

        // Add to new parent's children list
        if let Some(new_parent_id) = self.new_parent {
            if let Some(new_parent) = state.get_entity_mut(new_parent_id) {
                if !new_parent.children.contains(&self.entity_id) {
                    new_parent.children.push(self.entity_id);
                }
            }
        }

        let entity_name = state.get_entity(self.entity_id)
            .map(|e| e.name.clone())
            .unwrap_or_default();
        let parent_name = self.new_parent
            .and_then(|id| state.get_entity(id))
            .map(|e| e.name.clone())
            .unwrap_or_else(|| "root".to_string());

        state.console.info(format!("Reparented {} to {}", entity_name, parent_name));

        Ok(())
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        // Remove from current parent's children list
        if let Some(current_parent_id) = self.new_parent {
            if let Some(current_parent) = state.get_entity_mut(current_parent_id) {
                current_parent.children.retain(|&id| id != self.entity_id);
            }
        }

        // Restore old parent
        let entity = state.get_entity_mut(self.entity_id)
            .ok_or(CommandError::EntityNotFound(self.entity_id))?;
        entity.parent = self.old_parent;

        // Add back to old parent's children list
        if let Some(old_parent_id) = self.old_parent {
            if let Some(old_parent) = state.get_entity_mut(old_parent_id) {
                if !old_parent.children.contains(&self.entity_id) {
                    old_parent.children.push(self.entity_id);
                }
            }
        }

        Ok(())
    }
}
