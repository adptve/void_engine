//! WASM Plugin Host
//!
//! Manages loading, instantiation, and execution of WASM behavior plugins.

use crate::{
    plugin::{PluginId, PluginInfo, PluginState, PluginExports},
    context::SpawnRequest,
    api::{HostCommand, LogLevel},
    Result, ScriptError,
};
use void_ecs::prelude::Entity;
use void_math::prelude::Vec3;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use parking_lot::RwLock;

#[cfg(feature = "wasmtime-runtime")]
use wasmtime::*;

/// Configuration for the plugin host
#[derive(Debug, Clone)]
pub struct PluginHostConfig {
    /// Base directory for plugin files
    pub plugin_dir: PathBuf,
    /// Maximum memory per plugin (in pages, 64KB each)
    pub max_memory_pages: u32,
    /// Enable debug logging
    pub debug: bool,
}

impl Default for PluginHostConfig {
    fn default() -> Self {
        Self {
            plugin_dir: PathBuf::from("plugins"),
            max_memory_pages: 256, // 16MB default
            debug: false,
        }
    }
}

/// Manages WASM plugins and their execution
pub struct PluginHost {
    config: PluginHostConfig,
    #[cfg(feature = "wasmtime-runtime")]
    engine: Engine,
    plugins: RwLock<HashMap<PluginId, LoadedPlugin>>,
    next_id: RwLock<u32>,
    /// Pending commands from plugins
    pending_commands: RwLock<Vec<(PluginId, HostCommand)>>,
}

#[cfg(feature = "wasmtime-runtime")]
struct LoadedPlugin {
    info: PluginInfo,
    module: Module,
    instance: Option<Instance>,
    store: Store<PluginData>,
}

#[cfg(feature = "wasmtime-runtime")]
struct PluginData {
    entity_id: u32,
    commands: Vec<HostCommand>,
}

impl PluginHost {
    /// Create a new plugin host
    #[cfg(feature = "wasmtime-runtime")]
    pub fn new(config: PluginHostConfig) -> Result<Self> {
        let engine = Engine::default();

        Ok(Self {
            config,
            engine,
            plugins: RwLock::new(HashMap::new()),
            next_id: RwLock::new(1),
            pending_commands: RwLock::new(Vec::new()),
        })
    }

    /// Create a new plugin host (stub when wasmtime is disabled)
    #[cfg(not(feature = "wasmtime-runtime"))]
    pub fn new(config: PluginHostConfig) -> Result<Self> {
        Ok(Self {
            config,
            plugins: RwLock::new(HashMap::new()),
            next_id: RwLock::new(1),
            pending_commands: RwLock::new(Vec::new()),
        })
    }

    /// Load a plugin from a WASM file
    #[cfg(feature = "wasmtime-runtime")]
    pub fn load_plugin<P: AsRef<Path>>(&self, path: P) -> Result<PluginId> {
        let path = path.as_ref();
        let full_path = if path.is_absolute() {
            path.to_path_buf()
        } else {
            self.config.plugin_dir.join(path)
        };

        // Read WASM bytes
        let wasm_bytes = std::fs::read(&full_path)
            .map_err(|e| ScriptError::IoError(format!("{}: {}", full_path.display(), e)))?;

        // Compile module
        let module = Module::new(&self.engine, &wasm_bytes)
            .map_err(|e| ScriptError::LoadError(e.to_string()))?;

        // Determine exported functions
        let exports = self.detect_exports(&module);

        // Allocate ID
        let id = {
            let mut next = self.next_id.write();
            let id = PluginId(*next);
            *next += 1;
            id
        };

        // Create plugin info
        let name = path.file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("unknown")
            .to_string();

        let info = PluginInfo {
            id,
            path: full_path,
            name,
            version: None,
            state: PluginState::Ready,
            exports,
        };

        // Create store with plugin data
        let store = Store::new(&self.engine, PluginData {
            entity_id: 0,
            commands: Vec::new(),
        });

        let plugin = LoadedPlugin {
            info,
            module,
            instance: None,
            store,
        };

        self.plugins.write().insert(id, plugin);

        if self.config.debug {
            log::info!("Loaded plugin {:?} from {}", id, path.display());
        }

        Ok(id)
    }

    /// Load a plugin (stub when wasmtime is disabled)
    #[cfg(not(feature = "wasmtime-runtime"))]
    pub fn load_plugin<P: AsRef<Path>>(&self, path: P) -> Result<PluginId> {
        let id = {
            let mut next = self.next_id.write();
            let id = PluginId(*next);
            *next += 1;
            id
        };

        log::warn!("WASM runtime disabled, plugin {} loaded as stub", path.as_ref().display());
        Ok(id)
    }

    /// Detect which functions a module exports
    #[cfg(feature = "wasmtime-runtime")]
    fn detect_exports(&self, module: &Module) -> PluginExports {
        let mut exports = PluginExports::default();

        for export in module.exports() {
            match export.name() {
                "on_spawn" => exports.on_spawn = true,
                "on_update" => exports.on_update = true,
                "on_interact" => exports.on_interact = true,
                "on_collision" => exports.on_collision = true,
                "on_destroy" => exports.on_destroy = true,
                "on_input" => exports.on_input = true,
                "on_message" => exports.on_message = true,
                _ => {}
            }
        }

        exports
    }

    /// Instantiate a plugin (lazily done on first call)
    #[cfg(feature = "wasmtime-runtime")]
    fn ensure_instantiated(&self, id: PluginId) -> Result<()> {
        let mut plugins = self.plugins.write();
        let plugin = plugins.get_mut(&id)
            .ok_or(ScriptError::PluginNotFound(id))?;

        if plugin.instance.is_some() {
            return Ok(());
        }

        // Create linker with host functions
        let mut linker = Linker::new(&self.engine);

        // Register host functions
        Self::register_host_functions(&mut linker)?;

        // Instantiate
        let instance = linker.instantiate(&mut plugin.store, &plugin.module)
            .map_err(|e| ScriptError::InstantiationError(e.to_string()))?;

        plugin.instance = Some(instance);
        plugin.info.state = PluginState::Ready;

        Ok(())
    }

    /// Register host functions that plugins can call
    #[cfg(feature = "wasmtime-runtime")]
    fn register_host_functions(linker: &mut Linker<PluginData>) -> Result<()> {
        // spawn_entity(prefab_ptr, prefab_len, x, y, z) -> entity_id
        linker.func_wrap("env", "spawn_entity",
            |mut caller: Caller<'_, PluginData>, prefab_ptr: i32, prefab_len: i32, x: f32, y: f32, z: f32| -> i32 {
                // Read prefab name from WASM memory
                let memory = caller.get_export("memory")
                    .and_then(|e| e.into_memory());

                if let Some(memory) = memory {
                    let data = memory.data(&caller);
                    let prefab_result = std::str::from_utf8(&data[prefab_ptr as usize..(prefab_ptr + prefab_len) as usize])
                        .map(|s| s.to_string());

                    if let Ok(prefab) = prefab_result {
                        caller.data_mut().commands.push(HostCommand::Spawn(SpawnRequest {
                            prefab,
                            position: Vec3::new(x, y, z),
                            rotation: Vec3::ZERO,
                            velocity: None,
                            plugin: None,
                            properties: HashMap::new(),
                        }));
                        return 1; // Success
                    }
                }
                0 // Failure
            }
        ).map_err(|e| ScriptError::InstantiationError(e.to_string()))?;

        // log(level, msg_ptr, msg_len)
        linker.func_wrap("env", "log",
            |mut caller: Caller<'_, PluginData>, level: i32, msg_ptr: i32, msg_len: i32| {
                let memory = caller.get_export("memory")
                    .and_then(|e| e.into_memory());

                if let Some(memory) = memory {
                    let data = memory.data(&caller);
                    let msg_result = std::str::from_utf8(&data[msg_ptr as usize..(msg_ptr + msg_len) as usize])
                        .map(|s| s.to_string());

                    if let Ok(message) = msg_result {
                        let log_level = match level {
                            0 => LogLevel::Debug,
                            1 => LogLevel::Info,
                            2 => LogLevel::Warn,
                            _ => LogLevel::Error,
                        };
                        caller.data_mut().commands.push(HostCommand::Log {
                            level: log_level,
                            message,
                        });
                    }
                }
            }
        ).map_err(|e| ScriptError::InstantiationError(e.to_string()))?;

        // play_sound(name_ptr, name_len)
        linker.func_wrap("env", "play_sound",
            |mut caller: Caller<'_, PluginData>, name_ptr: i32, name_len: i32| {
                let memory = caller.get_export("memory")
                    .and_then(|e| e.into_memory());

                if let Some(memory) = memory {
                    let data = memory.data(&caller);
                    let name_result = std::str::from_utf8(&data[name_ptr as usize..(name_ptr + name_len) as usize])
                        .map(|s| s.to_string());

                    if let Ok(name) = name_result {
                        caller.data_mut().commands.push(HostCommand::PlaySound {
                            name,
                            position: None,
                        });
                    }
                }
            }
        ).map_err(|e| ScriptError::InstantiationError(e.to_string()))?;

        // get_time() -> f64
        linker.func_wrap("env", "get_time",
            |_caller: Caller<'_, PluginData>| -> f64 {
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .map(|d| d.as_secs_f64())
                    .unwrap_or(0.0)
            }
        ).map_err(|e| ScriptError::InstantiationError(e.to_string()))?;

        // get_delta_time() -> f32
        linker.func_wrap("env", "get_delta_time",
            |_caller: Caller<'_, PluginData>| -> f32 {
                0.016 // ~60fps, would be passed from engine
            }
        ).map_err(|e| ScriptError::InstantiationError(e.to_string()))?;

        // set_position(entity_id, x, y, z)
        linker.func_wrap("env", "set_position",
            |mut caller: Caller<'_, PluginData>, _entity_id: i32, x: f32, y: f32, z: f32| {
                let entity = Entity::new(caller.data().entity_id, 0);
                caller.data_mut().commands.push(HostCommand::SetPosition {
                    entity,
                    position: Vec3::new(x, y, z),
                });
            }
        ).map_err(|e| ScriptError::InstantiationError(e.to_string()))?;

        // set_velocity(entity_id, vx, vy, vz)
        linker.func_wrap("env", "set_velocity",
            |mut caller: Caller<'_, PluginData>, _entity_id: i32, vx: f32, vy: f32, vz: f32| {
                let entity = Entity::new(caller.data().entity_id, 0);
                caller.data_mut().commands.push(HostCommand::SetVelocity {
                    entity,
                    velocity: Vec3::new(vx, vy, vz),
                });
            }
        ).map_err(|e| ScriptError::InstantiationError(e.to_string()))?;

        Ok(())
    }

    /// Call on_spawn for an entity
    #[cfg(feature = "wasmtime-runtime")]
    pub fn call_on_spawn(&self, plugin_id: PluginId, entity: Entity) -> Result<Vec<HostCommand>> {
        self.ensure_instantiated(plugin_id)?;

        let mut plugins = self.plugins.write();
        let plugin = plugins.get_mut(&plugin_id)
            .ok_or(ScriptError::PluginNotFound(plugin_id))?;

        if !plugin.info.exports.on_spawn {
            return Ok(Vec::new());
        }

        let instance = plugin.instance.as_ref()
            .ok_or_else(|| ScriptError::InvalidState("Not instantiated".into()))?;

        // Set current entity
        plugin.store.data_mut().entity_id = entity.index();
        plugin.store.data_mut().commands.clear();

        // Call on_spawn
        let on_spawn = instance.get_typed_func::<u32, ()>(&mut plugin.store, "on_spawn")
            .map_err(|e| ScriptError::CallError(e.to_string()))?;

        on_spawn.call(&mut plugin.store, entity.index())
            .map_err(|e| ScriptError::CallError(e.to_string()))?;

        // Collect commands
        Ok(std::mem::take(&mut plugin.store.data_mut().commands))
    }

    /// Call on_update for an entity
    #[cfg(feature = "wasmtime-runtime")]
    pub fn call_on_update(&self, plugin_id: PluginId, entity: Entity, dt: f32) -> Result<Vec<HostCommand>> {
        self.ensure_instantiated(plugin_id)?;

        let mut plugins = self.plugins.write();
        let plugin = plugins.get_mut(&plugin_id)
            .ok_or(ScriptError::PluginNotFound(plugin_id))?;

        if !plugin.info.exports.on_update {
            return Ok(Vec::new());
        }

        let instance = plugin.instance.as_ref()
            .ok_or_else(|| ScriptError::InvalidState("Not instantiated".into()))?;

        plugin.store.data_mut().entity_id = entity.index();
        plugin.store.data_mut().commands.clear();

        let on_update = instance.get_typed_func::<(u32, f32), ()>(&mut plugin.store, "on_update")
            .map_err(|e| ScriptError::CallError(e.to_string()))?;

        on_update.call(&mut plugin.store, (entity.index(), dt))
            .map_err(|e| ScriptError::CallError(e.to_string()))?;

        Ok(std::mem::take(&mut plugin.store.data_mut().commands))
    }

    /// Call on_interact between two entities
    #[cfg(feature = "wasmtime-runtime")]
    pub fn call_on_interact(&self, plugin_id: PluginId, entity: Entity, other: Entity) -> Result<Vec<HostCommand>> {
        self.ensure_instantiated(plugin_id)?;

        let mut plugins = self.plugins.write();
        let plugin = plugins.get_mut(&plugin_id)
            .ok_or(ScriptError::PluginNotFound(plugin_id))?;

        if !plugin.info.exports.on_interact {
            return Ok(Vec::new());
        }

        let instance = plugin.instance.as_ref()
            .ok_or_else(|| ScriptError::InvalidState("Not instantiated".into()))?;

        plugin.store.data_mut().entity_id = entity.index();
        plugin.store.data_mut().commands.clear();

        let on_interact = instance.get_typed_func::<(u32, u32), ()>(&mut plugin.store, "on_interact")
            .map_err(|e| ScriptError::CallError(e.to_string()))?;

        on_interact.call(&mut plugin.store, (entity.index(), other.index()))
            .map_err(|e| ScriptError::CallError(e.to_string()))?;

        Ok(std::mem::take(&mut plugin.store.data_mut().commands))
    }

    /// Stubs for when wasmtime is disabled
    #[cfg(not(feature = "wasmtime-runtime"))]
    pub fn call_on_spawn(&self, _plugin_id: PluginId, _entity: Entity) -> Result<Vec<HostCommand>> {
        Ok(Vec::new())
    }

    #[cfg(not(feature = "wasmtime-runtime"))]
    pub fn call_on_update(&self, _plugin_id: PluginId, _entity: Entity, _dt: f32) -> Result<Vec<HostCommand>> {
        Ok(Vec::new())
    }

    #[cfg(not(feature = "wasmtime-runtime"))]
    pub fn call_on_interact(&self, _plugin_id: PluginId, _entity: Entity, _other: Entity) -> Result<Vec<HostCommand>> {
        Ok(Vec::new())
    }

    /// Get info about a loaded plugin
    pub fn get_plugin_info(&self, id: PluginId) -> Option<PluginInfo> {
        #[cfg(feature = "wasmtime-runtime")]
        {
            self.plugins.read().get(&id).map(|p| p.info.clone())
        }
        #[cfg(not(feature = "wasmtime-runtime"))]
        {
            None
        }
    }

    /// Unload a plugin
    pub fn unload_plugin(&self, id: PluginId) -> Result<()> {
        self.plugins.write().remove(&id);
        Ok(())
    }

    /// Get all loaded plugin IDs
    pub fn loaded_plugins(&self) -> Vec<PluginId> {
        #[cfg(feature = "wasmtime-runtime")]
        {
            self.plugins.read().keys().copied().collect()
        }
        #[cfg(not(feature = "wasmtime-runtime"))]
        {
            Vec::new()
        }
    }
}

// Non-wasmtime stub structures
#[cfg(not(feature = "wasmtime-runtime"))]
struct LoadedPlugin {
    info: PluginInfo,
}
