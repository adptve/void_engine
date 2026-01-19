//! Transform manipulation commands (move, rotate, scale).

use crate::core::{EditorState, EntityId, Transform};
use super::{Command, CommandResult, CommandError};

/// Command to set an entity's transform directly.
pub struct SetTransformCommand {
    pub entity_id: EntityId,
    pub new_transform: Transform,
    old_transform: Option<Transform>,
}

impl SetTransformCommand {
    pub fn new(entity_id: EntityId, new_transform: Transform) -> Self {
        Self {
            entity_id,
            new_transform,
            old_transform: None,
        }
    }
}

impl Command for SetTransformCommand {
    fn description(&self) -> &str {
        "Set Transform"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        let entity = state.get_entity_mut(self.entity_id)
            .ok_or(CommandError::EntityNotFound(self.entity_id))?;

        self.old_transform = Some(entity.transform);
        entity.transform = self.new_transform;

        Ok(())
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        if let Some(old) = self.old_transform {
            let entity = state.get_entity_mut(self.entity_id)
                .ok_or(CommandError::EntityNotFound(self.entity_id))?;
            entity.transform = old;
        }
        Ok(())
    }

}

/// Command to move an entity by a delta.
pub struct MoveCommand {
    pub entity_id: EntityId,
    pub delta: [f32; 3],
    old_position: Option<[f32; 3]>,
}

impl MoveCommand {
    pub fn new(entity_id: EntityId, delta: [f32; 3]) -> Self {
        Self {
            entity_id,
            delta,
            old_position: None,
        }
    }

    pub fn to_position(entity_id: EntityId, new_position: [f32; 3], old_position: [f32; 3]) -> Self {
        Self {
            entity_id,
            delta: [
                new_position[0] - old_position[0],
                new_position[1] - old_position[1],
                new_position[2] - old_position[2],
            ],
            old_position: Some(old_position),
        }
    }
}

impl Command for MoveCommand {
    fn description(&self) -> &str {
        "Move"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        let entity = state.get_entity_mut(self.entity_id)
            .ok_or(CommandError::EntityNotFound(self.entity_id))?;

        if self.old_position.is_none() {
            self.old_position = Some(entity.transform.position);
        }

        entity.transform.position[0] += self.delta[0];
        entity.transform.position[1] += self.delta[1];
        entity.transform.position[2] += self.delta[2];

        Ok(())
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        if let Some(old) = self.old_position {
            let entity = state.get_entity_mut(self.entity_id)
                .ok_or(CommandError::EntityNotFound(self.entity_id))?;
            entity.transform.position = old;
        }
        Ok(())
    }

}

/// Command to rotate an entity.
pub struct RotateCommand {
    pub entity_id: EntityId,
    pub delta: [f32; 3],
    old_rotation: Option<[f32; 3]>,
}

impl RotateCommand {
    pub fn new(entity_id: EntityId, delta: [f32; 3]) -> Self {
        Self {
            entity_id,
            delta,
            old_rotation: None,
        }
    }

    pub fn to_rotation(entity_id: EntityId, new_rotation: [f32; 3], old_rotation: [f32; 3]) -> Self {
        Self {
            entity_id,
            delta: [
                new_rotation[0] - old_rotation[0],
                new_rotation[1] - old_rotation[1],
                new_rotation[2] - old_rotation[2],
            ],
            old_rotation: Some(old_rotation),
        }
    }
}

impl Command for RotateCommand {
    fn description(&self) -> &str {
        "Rotate"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        let entity = state.get_entity_mut(self.entity_id)
            .ok_or(CommandError::EntityNotFound(self.entity_id))?;

        if self.old_rotation.is_none() {
            self.old_rotation = Some(entity.transform.rotation);
        }

        entity.transform.rotation[0] += self.delta[0];
        entity.transform.rotation[1] += self.delta[1];
        entity.transform.rotation[2] += self.delta[2];

        Ok(())
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        if let Some(old) = self.old_rotation {
            let entity = state.get_entity_mut(self.entity_id)
                .ok_or(CommandError::EntityNotFound(self.entity_id))?;
            entity.transform.rotation = old;
        }
        Ok(())
    }

}

/// Command to scale an entity.
pub struct ScaleCommand {
    pub entity_id: EntityId,
    pub new_scale: [f32; 3],
    old_scale: Option<[f32; 3]>,
}

impl ScaleCommand {
    pub fn new(entity_id: EntityId, new_scale: [f32; 3]) -> Self {
        Self {
            entity_id,
            new_scale,
            old_scale: None,
        }
    }

    pub fn uniform(entity_id: EntityId, scale: f32) -> Self {
        Self::new(entity_id, [scale, scale, scale])
    }
}

impl Command for ScaleCommand {
    fn description(&self) -> &str {
        "Scale"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        let entity = state.get_entity_mut(self.entity_id)
            .ok_or(CommandError::EntityNotFound(self.entity_id))?;

        self.old_scale = Some(entity.transform.scale);
        entity.transform.scale = self.new_scale;

        Ok(())
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        if let Some(old) = self.old_scale {
            let entity = state.get_entity_mut(self.entity_id)
                .ok_or(CommandError::EntityNotFound(self.entity_id))?;
            entity.transform.scale = old;
        }
        Ok(())
    }

}

/// Command to move multiple entities.
pub struct MoveMultipleCommand {
    pub entity_ids: Vec<EntityId>,
    pub delta: [f32; 3],
    old_positions: Vec<(EntityId, [f32; 3])>,
}

impl MoveMultipleCommand {
    pub fn new(entity_ids: Vec<EntityId>, delta: [f32; 3]) -> Self {
        Self {
            entity_ids,
            delta,
            old_positions: Vec::new(),
        }
    }
}

impl Command for MoveMultipleCommand {
    fn description(&self) -> &str {
        "Move Entities"
    }

    fn execute(&mut self, state: &mut EditorState) -> CommandResult {
        self.old_positions.clear();

        for &id in &self.entity_ids {
            if let Some(entity) = state.get_entity_mut(id) {
                self.old_positions.push((id, entity.transform.position));
                entity.transform.position[0] += self.delta[0];
                entity.transform.position[1] += self.delta[1];
                entity.transform.position[2] += self.delta[2];
            }
        }

        Ok(())
    }

    fn undo(&mut self, state: &mut EditorState) -> CommandResult {
        for (id, old_pos) in &self.old_positions {
            if let Some(entity) = state.get_entity_mut(*id) {
                entity.transform.position = *old_pos;
            }
        }
        Ok(())
    }

}
