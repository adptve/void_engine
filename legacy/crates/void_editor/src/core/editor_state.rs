//! Central editor state - single source of truth.
//!
//! All editor data flows through `EditorState`. Modifications should
//! go through the command system for undo/redo support.

use std::collections::HashMap;
use std::path::PathBuf;
use std::time::Instant;

use void_ecs::{World, Entity, WorldExt};

use super::{EntityId, RecentFiles, SelectionManager, UndoHistory, EditorPreferences};
use crate::panels::{Console, AssetBrowser};
use crate::viewport::{ViewportState, GizmoState};
use crate::tools::{ToolRegistry, SelectionTool, MoveTool, RotateTool, ScaleTool, PrimitiveCreationTool};
use crate::assets::{AssetDatabase, ThumbnailCache, PrefabLibrary, Prefab, PrefabEntity, PrefabTransform, PrefabMesh};

/// Mesh type for editor primitives.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum MeshType {
    Cube,
    Sphere,
    Cylinder,
    Diamond,
    Torus,
    Plane,
}

impl MeshType {
    pub fn name(&self) -> &'static str {
        match self {
            MeshType::Cube => "Cube",
            MeshType::Sphere => "Sphere",
            MeshType::Cylinder => "Cylinder",
            MeshType::Diamond => "Diamond",
            MeshType::Torus => "Torus",
            MeshType::Plane => "Plane",
        }
    }

    pub fn all() -> &'static [MeshType] {
        &[
            MeshType::Cube,
            MeshType::Sphere,
            MeshType::Cylinder,
            MeshType::Diamond,
            MeshType::Torus,
            MeshType::Plane,
        ]
    }
}

/// Transform component for scene entities.
#[derive(Clone, Copy, Debug, Default)]
pub struct Transform {
    pub position: [f32; 3],
    pub rotation: [f32; 3],
    pub scale: [f32; 3],
}

impl Transform {
    pub fn new() -> Self {
        Self {
            position: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0],
            scale: [1.0, 1.0, 1.0],
        }
    }

    pub fn with_position(mut self, pos: [f32; 3]) -> Self {
        self.position = pos;
        self
    }

    pub fn with_scale(mut self, scale: f32) -> Self {
        self.scale = [scale, scale, scale];
        self
    }
}

/// A scene entity with all its properties.
#[derive(Clone, Debug)]
pub struct SceneEntity {
    /// Unique identifier
    pub id: EntityId,
    /// Display name
    pub name: String,
    /// World transform
    pub transform: Transform,
    /// Mesh type
    pub mesh_type: MeshType,
    /// Display color
    pub color: [f32; 3],
    /// Parent entity (for hierarchy)
    pub parent: Option<EntityId>,
    /// Children entities
    pub children: Vec<EntityId>,
    /// Link to void_ecs Entity
    pub ecs_entity: Option<Entity>,
    /// Whether entity is visible
    pub visible: bool,
    /// Whether entity is locked (cannot be selected/edited)
    pub locked: bool,
}

impl Default for SceneEntity {
    fn default() -> Self {
        Self {
            id: EntityId(0),
            name: "Entity".to_string(),
            transform: Transform::new(),
            mesh_type: MeshType::Cube,
            color: [0.8, 0.8, 0.8],
            parent: None,
            children: Vec::new(),
            ecs_entity: None,
            visible: true,
            locked: false,
        }
    }
}

impl SceneEntity {
    pub fn new(id: EntityId, name: impl Into<String>) -> Self {
        Self {
            id,
            name: name.into(),
            ..Default::default()
        }
    }

    pub fn with_mesh(mut self, mesh_type: MeshType) -> Self {
        self.mesh_type = mesh_type;
        self
    }

    pub fn with_transform(mut self, transform: Transform) -> Self {
        self.transform = transform;
        self
    }

    pub fn with_color(mut self, color: [f32; 3]) -> Self {
        self.color = color;
        self
    }
}

/// Central editor state.
pub struct EditorState {
    // Scene data
    pub entities: Vec<SceneEntity>,
    pub entity_map: HashMap<EntityId, usize>,
    next_entity_id: u32,

    // Selection
    pub selection: SelectionManager,

    // History
    pub history: UndoHistory,

    // ECS World integration
    pub ecs_world: World,

    // Scene file
    pub scene_path: Option<PathBuf>,
    pub scene_modified: bool,

    // Recent files
    pub recent_files: RecentFiles,

    // Preferences
    pub preferences: EditorPreferences,

    // Viewport
    pub viewport: ViewportState,

    // Gizmos
    pub gizmos: GizmoState,

    // Tools
    pub tools: ToolRegistry,

    // Console
    pub console: Console,

    // Asset browser
    pub asset_browser: AssetBrowser,

    // Asset database and thumbnails
    pub asset_database: AssetDatabase,
    pub thumbnails: ThumbnailCache,
    pub prefabs: PrefabLibrary,

    // Status message
    pub status_message: String,
    pub status_time: Instant,

    // Dialog state
    pub show_about: bool,
    pub show_shortcuts: bool,
    pub show_new_entity: bool,
    pub new_entity_name: String,
    pub new_entity_type: MeshType,

    // Creation tool state
    pub creation_primitive_type: MeshType,

    // Drag-drop state
    pub drag_asset: Option<DraggedAsset>,
}

/// Asset being dragged from the browser.
#[derive(Clone, Debug)]
pub struct DraggedAsset {
    pub path: PathBuf,
    pub name: String,
    pub asset_type: crate::panels::AssetType,
}

impl Default for EditorState {
    fn default() -> Self {
        Self::new()
    }
}

impl EditorState {
    pub fn new() -> Self {
        let mut ecs_world = World::new();

        // Register common ECS components
        ecs_world.register_cloneable_component::<NameComponent>();
        ecs_world.register_cloneable_component::<TransformComponent>();
        ecs_world.register_cloneable_component::<MeshComponent>();

        let mut asset_browser = AssetBrowser::default();
        asset_browser.refresh();

        // Initialize tool registry with default tools
        let mut tools = ToolRegistry::new();
        tools.register(Box::new(SelectionTool::new()));
        tools.register(Box::new(MoveTool::new()));
        tools.register(Box::new(RotateTool::new()));
        tools.register(Box::new(ScaleTool::new()));
        tools.register(Box::new(PrimitiveCreationTool::new()));

        Self {
            entities: Vec::new(),
            entity_map: HashMap::new(),
            next_entity_id: 1,
            selection: SelectionManager::new(),
            history: UndoHistory::new(),
            ecs_world,
            scene_path: None,
            scene_modified: false,
            recent_files: RecentFiles::new(),
            preferences: EditorPreferences::default(),
            viewport: ViewportState::default(),
            gizmos: GizmoState::default(),
            tools,
            console: Console::default(),
            asset_browser,
            asset_database: AssetDatabase::new(),
            thumbnails: ThumbnailCache::new(),
            prefabs: PrefabLibrary::new(),
            status_message: "Ready".to_string(),
            status_time: Instant::now(),
            show_about: false,
            show_shortcuts: false,
            show_new_entity: false,
            new_entity_name: "New Entity".to_string(),
            new_entity_type: MeshType::Cube,
            creation_primitive_type: MeshType::Cube,
            drag_asset: None,
        }
    }

    /// Set the status bar message.
    pub fn set_status(&mut self, msg: impl Into<String>) {
        self.status_message = msg.into();
        self.status_time = Instant::now();
    }

    /// Generate the next entity ID.
    pub fn next_entity_id(&mut self) -> EntityId {
        let id = EntityId(self.next_entity_id);
        self.next_entity_id += 1;
        id
    }

    /// Get an entity by ID.
    pub fn get_entity(&self, id: EntityId) -> Option<&SceneEntity> {
        self.entity_map.get(&id).map(|&idx| &self.entities[idx])
    }

    /// Get a mutable entity by ID.
    pub fn get_entity_mut(&mut self, id: EntityId) -> Option<&mut SceneEntity> {
        self.entity_map.get(&id).map(|&idx| &mut self.entities[idx])
    }

    /// Add an entity to the scene.
    pub fn add_entity(&mut self, entity: SceneEntity) {
        let id = entity.id;
        let idx = self.entities.len();
        self.entities.push(entity);
        self.entity_map.insert(id, idx);
        self.scene_modified = true;
    }

    /// Remove an entity from the scene.
    pub fn remove_entity(&mut self, id: EntityId) -> Option<SceneEntity> {
        if let Some(&idx) = self.entity_map.get(&id) {
            let entity = self.entities.remove(idx);
            self.entity_map.remove(&id);

            // Update indices for entities after the removed one
            for (eid, eidx) in self.entity_map.iter_mut() {
                if *eidx > idx {
                    *eidx -= 1;
                }
            }

            // Remove from selection
            self.selection.remove_entity(id);

            // Despawn from ECS
            if let Some(ecs_entity) = entity.ecs_entity {
                self.ecs_world.despawn(ecs_entity);
            }

            self.scene_modified = true;
            Some(entity)
        } else {
            None
        }
    }

    /// Create a new entity with default settings.
    pub fn create_entity(&mut self, name: String, mesh_type: MeshType) -> EntityId {
        let id = self.next_entity_id();

        // Create ECS entity
        let ecs_entity = self.ecs_world.build_entity()
            .with(NameComponent(name.clone()))
            .with(TransformComponent {
                position: [0.0, 0.0, 0.0],
                rotation: [0.0, 0.0, 0.0],
                scale: [1.0, 1.0, 1.0],
            })
            .with(MeshComponent {
                mesh_type,
                color: [0.8, 0.8, 0.8],
            })
            .build();

        let entity = SceneEntity {
            id,
            name: name.clone(),
            mesh_type,
            ecs_entity: Some(ecs_entity),
            ..Default::default()
        };

        self.console.info(format!("Created entity: {} ({})", name, mesh_type.name()));
        self.set_status(format!("Created {}", mesh_type.name()));

        self.add_entity(entity);
        id
    }

    /// Delete selected entities.
    pub fn delete_selected(&mut self) {
        let selected: Vec<EntityId> = self.selection.selected().to_vec();
        for id in selected {
            if let Some(entity) = self.remove_entity(id) {
                self.console.info(format!("Deleted entity: {}", entity.name));
            }
        }
        if !self.selection.is_empty() {
            self.set_status("Deleted entities");
        }
    }

    /// Duplicate selected entities.
    pub fn duplicate_selected(&mut self) {
        let selected: Vec<EntityId> = self.selection.selected().to_vec();
        let mut new_ids = Vec::new();

        for id in selected {
            if let Some(src) = self.get_entity(id).cloned() {
                let new_id = self.next_entity_id();

                // Create ECS entity
                let ecs_entity = self.ecs_world.build_entity()
                    .with(NameComponent(format!("{} (Copy)", src.name)))
                    .with(TransformComponent {
                        position: [
                            src.transform.position[0] + 1.0,
                            src.transform.position[1],
                            src.transform.position[2],
                        ],
                        rotation: src.transform.rotation,
                        scale: src.transform.scale,
                    })
                    .with(MeshComponent {
                        mesh_type: src.mesh_type,
                        color: src.color,
                    })
                    .build();

                let mut new_entity = src.clone();
                new_entity.id = new_id;
                new_entity.name = format!("{} (Copy)", new_entity.name);
                new_entity.transform.position[0] += 1.0;
                new_entity.ecs_entity = Some(ecs_entity);

                self.console.info(format!("Duplicated entity: {}", new_entity.name));
                self.add_entity(new_entity);
                new_ids.push(new_id);
            }
        }

        // Select the new entities
        if !new_ids.is_empty() {
            self.selection.select_multiple(new_ids);
            self.set_status("Duplicated entities");
        }
    }

    /// Select all entities.
    pub fn select_all(&mut self) {
        let all: Vec<EntityId> = self.entities.iter().map(|e| e.id).collect();
        self.selection.select_all(all);
    }

    /// Deselect all entities.
    pub fn deselect_all(&mut self) {
        self.selection.clear();
    }

    /// Create a new empty scene.
    pub fn new_scene(&mut self) {
        // Clear ECS world
        self.ecs_world.clear();

        // Clear entities
        self.entities.clear();
        self.entity_map.clear();
        self.next_entity_id = 1;

        // Clear selection and history
        self.selection.clear();
        self.history.clear();

        // Reset scene file
        self.scene_path = None;
        self.scene_modified = false;

        self.console.info("New scene created");
        self.set_status("New scene created");
    }

    /// Check if the scene has been modified.
    pub fn is_modified(&self) -> bool {
        self.scene_modified || self.history.is_dirty()
    }

    /// Mark the scene as saved.
    pub fn mark_saved(&mut self) {
        self.scene_modified = false;
        self.history.mark_saved();
    }

    /// Get all entity IDs.
    pub fn entity_ids(&self) -> Vec<EntityId> {
        self.entities.iter().map(|e| e.id).collect()
    }

    // ========================================================================
    // Command System Integration
    // ========================================================================

    /// Execute a command and add it to history for undo/redo.
    pub fn execute_command(&mut self, mut cmd: Box<dyn crate::commands::Command>) {
        match cmd.execute(self) {
            Ok(()) => {
                self.history.push(cmd);
                self.scene_modified = true;
            }
            Err(e) => {
                self.console.error(format!("Command failed: {}", e));
            }
        }
    }

    /// Undo the last command.
    pub fn undo(&mut self) -> bool {
        // Pop command from undo stack
        if let Some(mut cmd) = self.history.pop_undo() {
            let desc = cmd.description().to_string();
            match cmd.undo(self) {
                Ok(()) => {
                    self.history.push_to_redo(cmd);
                    self.console.info(format!("Undo: {}", desc));
                    self.set_status(format!("Undo: {}", desc));
                    true
                }
                Err(e) => {
                    // Put command back on undo stack
                    self.history.push_to_undo(cmd);
                    self.console.error(format!("Undo failed: {}", e));
                    false
                }
            }
        } else {
            self.set_status("Nothing to undo");
            false
        }
    }

    /// Redo the last undone command.
    pub fn redo(&mut self) -> bool {
        // Pop command from redo stack
        if let Some(mut cmd) = self.history.pop_redo() {
            let desc = cmd.description().to_string();
            match cmd.execute(self) {
                Ok(()) => {
                    self.history.push_to_undo(cmd);
                    self.console.info(format!("Redo: {}", desc));
                    self.set_status(format!("Redo: {}", desc));
                    true
                }
                Err(e) => {
                    // Put command back on redo stack
                    self.history.push_to_redo(cmd);
                    self.console.error(format!("Redo failed: {}", e));
                    false
                }
            }
        } else {
            self.set_status("Nothing to redo");
            false
        }
    }

    /// Check if undo is available.
    pub fn can_undo(&self) -> bool {
        self.history.can_undo()
    }

    /// Check if redo is available.
    pub fn can_redo(&self) -> bool {
        self.history.can_redo()
    }

    /// Get the description of the next undo action.
    pub fn undo_description(&self) -> Option<&str> {
        self.history.undo_description()
    }

    /// Get the description of the next redo action.
    pub fn redo_description(&self) -> Option<&str> {
        self.history.redo_description()
    }

    // ========================================================================
    // Command-based Operations (preferred over direct manipulation)
    // ========================================================================

    /// Create an entity using the command system.
    pub fn create_entity_cmd(&mut self, name: String, mesh_type: MeshType) -> EntityId {
        // Create directly for now - full command support will be added
        self.create_entity(name, mesh_type)
    }

    /// Delete selected entities using the command system.
    pub fn delete_selected_cmd(&mut self) {
        use crate::commands::DeleteMultipleCommand;
        let selected: Vec<EntityId> = self.selection.selected().to_vec();
        if !selected.is_empty() {
            let cmd = Box::new(DeleteMultipleCommand::new(selected));
            self.execute_command(cmd);
        }
    }

    /// Duplicate selected entities using the command system.
    pub fn duplicate_selected_cmd(&mut self) {
        use crate::commands::DuplicateEntityCommand;
        let selected: Vec<EntityId> = self.selection.selected().to_vec();
        for id in selected {
            let cmd = Box::new(DuplicateEntityCommand::new(id));
            self.execute_command(cmd);
        }
    }

    // ========================================================================
    // Asset Pipeline
    // ========================================================================

    /// Initialize asset database with default roots.
    pub fn init_asset_database(&mut self) {
        use crate::assets::AssetGuid;

        // Add common asset directories
        let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));

        // Check for assets folder
        let assets_path = cwd.join("assets");
        if assets_path.is_dir() {
            self.asset_database.add_root(assets_path);
        }

        // Check for crates/void_editor/assets (for development)
        let dev_assets = cwd.join("crates").join("void_editor").join("assets");
        if dev_assets.is_dir() {
            self.asset_database.add_root(dev_assets);
        }

        // Refresh database
        self.asset_database.refresh();

        // Set thumbnail cache directory
        if let Some(cache_dir) = dirs::cache_dir() {
            let thumb_cache = cache_dir.join("void_editor").join("thumbnails");
            self.thumbnails.set_cache_dir(thumb_cache);
        }

        self.console.info(format!(
            "Asset database initialized: {} assets indexed",
            self.asset_database.count()
        ));
    }

    /// Process pending thumbnail generation (call each frame).
    pub fn process_thumbnails(&mut self) {
        // Process up to 2 thumbnails per frame to avoid stalling
        let generated = self.thumbnails.process_pending(2);
        if generated > 0 {
            // Thumbnails were generated, could trigger UI refresh
        }
    }

    /// Request thumbnail for an asset in the browser.
    pub fn request_asset_thumbnail(&mut self, path: &PathBuf) {
        use crate::assets::AssetGuid;
        use crate::panels::AssetType;

        let guid = AssetGuid::from_path(path);
        let asset_type = path.file_name()
            .map(|n| AssetType::from_filename(&n.to_string_lossy()))
            .unwrap_or(AssetType::Unknown);

        self.thumbnails.request(guid, path.clone(), asset_type);
    }

    // ========================================================================
    // Prefab System
    // ========================================================================

    /// Save the selected entity as a prefab.
    pub fn save_selected_as_prefab(&mut self, prefab_name: &str) -> Result<(), String> {
        let selected_id = self.selection.primary()
            .ok_or_else(|| "No entity selected".to_string())?;

        let entity = self.get_entity(selected_id)
            .ok_or_else(|| "Entity not found".to_string())?;

        // Create prefab from entity
        let mut prefab = Prefab::new(prefab_name);
        prefab.root = PrefabEntity {
            name: entity.name.clone(),
            transform: PrefabTransform::from(entity.transform),
            mesh: Some(PrefabMesh::from_primitive(entity.mesh_type, entity.color)),
            properties: std::collections::HashMap::new(),
        };

        // Add to library
        self.prefabs.add(prefab);

        // Save to disk
        self.prefabs.save(prefab_name)
            .map_err(|e| e.to_string())?;

        self.console.info(format!("Saved prefab: {}", prefab_name));
        self.set_status(format!("Prefab saved: {}", prefab_name));

        Ok(())
    }

    /// Instantiate a prefab into the scene.
    pub fn instantiate_prefab(&mut self, prefab_name: &str) -> Option<EntityId> {
        let prefab = self.prefabs.get(prefab_name)?.clone();

        // Create entity from prefab root
        let mesh_type = prefab.root.mesh.as_ref()
            .map(|m| m.to_mesh_type())
            .unwrap_or(MeshType::Cube);

        let id = self.create_entity(prefab.root.name.clone(), mesh_type);

        // Apply transform
        if let Some(entity) = self.get_entity_mut(id) {
            entity.transform = Transform::from(prefab.root.transform);
            if let Some(mesh) = &prefab.root.mesh {
                entity.color = mesh.color;
            }
        }

        self.console.info(format!("Instantiated prefab: {}", prefab_name));
        self.set_status(format!("Created: {} (from prefab)", prefab.root.name));

        Some(id)
    }

    /// Initialize prefab library.
    pub fn init_prefab_library(&mut self) {
        // Set prefab directory
        let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
        let prefab_dir = cwd.join("prefabs");
        self.prefabs.set_prefab_dir(prefab_dir);

        // Load existing prefabs
        let count = self.prefabs.load_all();
        if count > 0 {
            self.console.info(format!("Loaded {} prefabs", count));
        }
    }

    // ========================================================================
    // Scene Save/Load
    // ========================================================================

    /// Save the current scene to a file.
    pub fn save_scene(&mut self, path: PathBuf) -> Result<(), String> {
        use crate::scene::SceneSerializer;

        SceneSerializer::save(self, &path)
            .map_err(|e| e.to_string())?;

        self.scene_path = Some(path.clone());
        self.scene_modified = false;
        self.recent_files.add(path.clone());

        let name = path.file_name()
            .map(|n| n.to_string_lossy().to_string())
            .unwrap_or_else(|| "scene".to_string());

        self.console.info(format!("Saved scene: {}", name));
        self.set_status(format!("Saved: {}", name));

        Ok(())
    }

    /// Load a scene from a file.
    pub fn load_scene(&mut self, path: PathBuf) -> Result<(), String> {
        use crate::scene::SceneSerializer;

        SceneSerializer::load(self, &path)
            .map_err(|e| e.to_string())?;

        self.scene_path = Some(path.clone());
        self.scene_modified = false;
        self.recent_files.add(path.clone());

        let name = path.file_name()
            .map(|n| n.to_string_lossy().to_string())
            .unwrap_or_else(|| "scene".to_string());

        self.console.info(format!("Loaded scene: {} ({} entities)", name, self.entities.len()));
        self.set_status(format!("Loaded: {}", name));

        Ok(())
    }

    /// Save scene to current path, or prompt if no path set.
    pub fn save_scene_current(&mut self) -> Result<bool, String> {
        if let Some(path) = self.scene_path.clone() {
            self.save_scene(path)?;
            Ok(true)
        } else {
            Ok(false) // Need to use Save As
        }
    }
}

// ECS Components for editor entities

#[derive(Clone, Debug)]
pub struct NameComponent(pub String);

#[derive(Clone, Copy, Debug, Default)]
pub struct TransformComponent {
    pub position: [f32; 3],
    pub rotation: [f32; 3],
    pub scale: [f32; 3],
}

#[derive(Clone, Copy, Debug)]
pub struct MeshComponent {
    pub mesh_type: MeshType,
    pub color: [f32; 3],
}
