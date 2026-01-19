//! Integration with void engine systems.
//!
//! Bridges between the editor and void_ecs, void_ir, void_render.

mod ecs_bridge;
mod ir_bridge;

pub use ecs_bridge::EcsBridge;
pub use ir_bridge::{IrBridge, editor_namespace};
