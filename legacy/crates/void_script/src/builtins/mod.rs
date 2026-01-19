//! Built-in functions for VoidScript
//!
//! Provides 60+ standard library functions organized by category:
//! - I/O: print, println, debug, input
//! - Types: type, str, int, float, bool, is_*
//! - Collections: len, push, pop, keys, values, range, map, filter, reduce
//! - Strings: upper, lower, split, join, trim, contains, replace
//! - Math: abs, min, max, floor, ceil, round, sqrt, pow, sin, cos, random
//! - Entity: spawn, destroy, get_component, set_component, get_position, set_position
//! - Layers: create_layer, destroy_layer, set_layer_visible
//! - Events: on, emit, off
//! - Time: now, sleep, after
//!
//! ## Kernel Integration
//!
//! When a ScriptContext is provided, entity and layer functions emit real
//! IR patches instead of returning mock data:
//!
//! ```ignore
//! // With context - emits EntityPatch::Create to PatchBus
//! let player = spawn("player");
//!
//! // With context - emits ComponentPatch::Set to PatchBus
//! set_position(player, 10, 0, 5);
//! ```

pub mod io;
pub mod types;
pub mod collections;
pub mod strings;
pub mod math;
pub mod entity;
pub mod layers;
pub mod events;
pub mod time;
pub mod utility;
pub mod context_entity;
pub mod context_layers;
pub mod context_patch;

use crate::interpreter::Interpreter;
use crate::context::ScriptContext;

/// Register all built-in functions with the interpreter
pub fn register_builtins(interpreter: &mut Interpreter) {
    // I/O functions
    io::register(interpreter);

    // Type functions
    types::register(interpreter);

    // Collection functions
    collections::register(interpreter);

    // String functions
    strings::register(interpreter);

    // Math functions
    math::register(interpreter);

    // Entity functions (stub implementations for now)
    entity::register(interpreter);

    // Layer functions (stub implementations for now)
    layers::register(interpreter);

    // Event functions (stub implementations for now)
    events::register(interpreter);

    // Time functions
    time::register(interpreter);

    // Utility functions (assert, panic, clone, json, etc.)
    utility::register(interpreter);
}

/// Register context-aware built-in functions
///
/// These replace the stub entity/layer functions with versions that
/// emit real IR patches when a live ScriptContext is provided.
pub fn register_context_builtins(interpreter: &mut Interpreter, context: ScriptContext) {
    // First register all standard builtins
    register_builtins(interpreter);

    // Then override entity and layer functions with context-aware versions
    context_entity::register(interpreter, context.clone());
    context_layers::register(interpreter, context.clone());

    // Register patch and input builtins
    context_patch::register(interpreter, context);
}

#[cfg(test)]
mod tests {
    use crate::VoidScript;
    use crate::value::Value;

    pub fn run(code: &str) -> Value {
        let mut vs = VoidScript::new();
        vs.execute(code).unwrap()
    }

    pub fn eval(code: &str) -> Value {
        let mut vs = VoidScript::new();
        vs.eval(code).unwrap()
    }
}
