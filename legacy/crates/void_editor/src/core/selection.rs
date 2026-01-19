//! Selection management with multi-select support.
//!
//! Provides Unity/Unreal-style selection with modifiers:
//! - Click: Replace selection
//! - Shift+Click: Add to selection
//! - Ctrl+Click: Remove from selection
//! - Ctrl+Shift+Click: Toggle selection

use super::EntityId;

/// Selection mode based on modifier keys.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub enum SelectionMode {
    /// Replace current selection (normal click)
    #[default]
    Replace,
    /// Add to current selection (Shift+click)
    Add,
    /// Remove from current selection (Ctrl+click)
    Remove,
    /// Toggle selection state (Ctrl+Shift+click)
    Toggle,
}

impl SelectionMode {
    /// Determine selection mode from modifier keys.
    pub fn from_modifiers(shift: bool, ctrl: bool) -> Self {
        match (shift, ctrl) {
            (true, true) => Self::Toggle,
            (true, false) => Self::Add,
            (false, true) => Self::Remove,
            (false, false) => Self::Replace,
        }
    }
}

/// Manages entity selection with multi-select support.
#[derive(Clone, Debug, Default)]
pub struct SelectionManager {
    /// Currently selected entities (in selection order)
    selected: Vec<EntityId>,
    /// Primary selected entity (receives gizmo, last selected)
    primary: Option<EntityId>,
    /// Whether selection has changed since last frame
    dirty: bool,
}

impl SelectionManager {
    pub fn new() -> Self {
        Self::default()
    }

    /// Get the primary (last selected) entity.
    pub fn primary(&self) -> Option<EntityId> {
        self.primary
    }

    /// Get all selected entities.
    pub fn selected(&self) -> &[EntityId] {
        &self.selected
    }

    /// Get number of selected entities.
    pub fn count(&self) -> usize {
        self.selected.len()
    }

    /// Check if any entities are selected.
    pub fn is_empty(&self) -> bool {
        self.selected.is_empty()
    }

    /// Check if a specific entity is selected.
    pub fn is_selected(&self, id: EntityId) -> bool {
        self.selected.contains(&id)
    }

    /// Check if a specific entity is the primary selection.
    pub fn is_primary(&self, id: EntityId) -> bool {
        self.primary == Some(id)
    }

    /// Check and clear the dirty flag.
    pub fn take_dirty(&mut self) -> bool {
        let was_dirty = self.dirty;
        self.dirty = false;
        was_dirty
    }

    /// Select an entity with the given mode.
    pub fn select(&mut self, id: EntityId, mode: SelectionMode) {
        match mode {
            SelectionMode::Replace => {
                self.selected.clear();
                self.selected.push(id);
                self.primary = Some(id);
            }
            SelectionMode::Add => {
                if !self.selected.contains(&id) {
                    self.selected.push(id);
                }
                self.primary = Some(id);
            }
            SelectionMode::Remove => {
                self.selected.retain(|&e| e != id);
                if self.primary == Some(id) {
                    self.primary = self.selected.last().copied();
                }
            }
            SelectionMode::Toggle => {
                if self.selected.contains(&id) {
                    self.selected.retain(|&e| e != id);
                    if self.primary == Some(id) {
                        self.primary = self.selected.last().copied();
                    }
                } else {
                    self.selected.push(id);
                    self.primary = Some(id);
                }
            }
        }
        self.dirty = true;
    }

    /// Select multiple entities (replaces current selection).
    pub fn select_multiple(&mut self, ids: impl IntoIterator<Item = EntityId>) {
        self.selected.clear();
        self.selected.extend(ids);
        self.primary = self.selected.last().copied();
        self.dirty = true;
    }

    /// Add multiple entities to selection.
    pub fn add_multiple(&mut self, ids: impl IntoIterator<Item = EntityId>) {
        for id in ids {
            if !self.selected.contains(&id) {
                self.selected.push(id);
            }
        }
        self.primary = self.selected.last().copied();
        self.dirty = true;
    }

    /// Clear all selection.
    pub fn clear(&mut self) {
        if !self.selected.is_empty() {
            self.selected.clear();
            self.primary = None;
            self.dirty = true;
        }
    }

    /// Select all entities from a list.
    pub fn select_all(&mut self, all_entities: impl IntoIterator<Item = EntityId>) {
        self.selected.clear();
        self.selected.extend(all_entities);
        self.primary = self.selected.last().copied();
        self.dirty = true;
    }

    /// Remove an entity from selection (e.g., when entity is deleted).
    pub fn remove_entity(&mut self, id: EntityId) {
        let was_selected = self.selected.contains(&id);
        self.selected.retain(|&e| e != id);
        if self.primary == Some(id) {
            self.primary = self.selected.last().copied();
        }
        if was_selected {
            self.dirty = true;
        }
    }

    /// Add a single entity to selection.
    pub fn add(&mut self, id: EntityId) {
        self.select(id, SelectionMode::Add);
    }

    /// Toggle a single entity's selection state.
    pub fn toggle(&mut self, id: EntityId) {
        self.select(id, SelectionMode::Toggle);
    }

    /// Get the selection as indices into an entity list.
    /// Returns None for entities not in the list.
    pub fn selected_indices<'a>(&'a self, entities: &'a [EntityId]) -> impl Iterator<Item = usize> + 'a {
        self.selected.iter().filter_map(move |&id| {
            entities.iter().position(|&e| e == id)
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_selection_replace() {
        let mut sel = SelectionManager::new();
        sel.select(EntityId(1), SelectionMode::Replace);
        sel.select(EntityId(2), SelectionMode::Replace);

        assert_eq!(sel.count(), 1);
        assert!(sel.is_selected(EntityId(2)));
        assert!(!sel.is_selected(EntityId(1)));
    }

    #[test]
    fn test_selection_add() {
        let mut sel = SelectionManager::new();
        sel.select(EntityId(1), SelectionMode::Replace);
        sel.select(EntityId(2), SelectionMode::Add);

        assert_eq!(sel.count(), 2);
        assert!(sel.is_selected(EntityId(1)));
        assert!(sel.is_selected(EntityId(2)));
    }

    #[test]
    fn test_selection_toggle() {
        let mut sel = SelectionManager::new();
        sel.select(EntityId(1), SelectionMode::Replace);
        sel.select(EntityId(1), SelectionMode::Toggle);

        assert!(sel.is_empty());
    }
}
