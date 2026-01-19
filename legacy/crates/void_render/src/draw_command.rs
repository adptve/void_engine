//! Draw Command Abstraction
//!
//! Provides a unified interface for issuing draw commands,
//! supporting both instanced and non-instanced rendering paths.
//!
//! # Draw Command Types
//!
//! - `Instanced`: GPU instanced rendering with instance buffer
//! - `Single`: Fallback for non-instancing GPUs or single entities
//! - `Indirect`: GPU-driven rendering (future)

use alloc::string::String;
use alloc::vec::Vec;
use serde::{Serialize, Deserialize};

use crate::instancing::BatchKey;
use crate::extraction::MeshTypeId;

/// A draw command ready for GPU submission
#[derive(Clone, Debug)]
pub enum DrawCommand {
    /// Instanced indexed draw
    InstancedIndexed {
        /// Batch key for looking up instance buffer
        batch_key: BatchKey,
        /// Mesh primitive index
        primitive_index: u32,
        /// Index count per instance
        index_count: u32,
        /// Instance count
        instance_count: u32,
        /// First instance (for multi-batch scenarios)
        first_instance: u32,
    },

    /// Instanced non-indexed draw
    InstancedNonIndexed {
        /// Batch key for looking up instance buffer
        batch_key: BatchKey,
        /// Mesh primitive index
        primitive_index: u32,
        /// Vertex count per instance
        vertex_count: u32,
        /// Instance count
        instance_count: u32,
        /// First instance
        first_instance: u32,
    },

    /// Single entity draw (fallback path)
    Single {
        /// Entity ID
        entity_id: u64,
        /// Mesh type
        mesh_type: MeshTypeId,
        /// Mesh primitive index
        primitive_index: u32,
        /// Model matrix
        model_matrix: [[f32; 4]; 4],
        /// Material index
        material_index: u32,
    },
}

impl DrawCommand {
    /// Create an instanced indexed draw command
    pub fn instanced_indexed(
        batch_key: BatchKey,
        primitive_index: u32,
        index_count: u32,
        instance_count: u32,
    ) -> Self {
        Self::InstancedIndexed {
            batch_key,
            primitive_index,
            index_count,
            instance_count,
            first_instance: 0,
        }
    }

    /// Create an instanced non-indexed draw command
    pub fn instanced_non_indexed(
        batch_key: BatchKey,
        primitive_index: u32,
        vertex_count: u32,
        instance_count: u32,
    ) -> Self {
        Self::InstancedNonIndexed {
            batch_key,
            primitive_index,
            vertex_count,
            instance_count,
            first_instance: 0,
        }
    }

    /// Create a single entity draw command
    pub fn single(
        entity_id: u64,
        mesh_type: MeshTypeId,
        primitive_index: u32,
        model_matrix: [[f32; 4]; 4],
        material_index: u32,
    ) -> Self {
        Self::Single {
            entity_id,
            mesh_type,
            primitive_index,
            model_matrix,
            material_index,
        }
    }

    /// Check if this is an instanced draw
    pub fn is_instanced(&self) -> bool {
        matches!(self, Self::InstancedIndexed { .. } | Self::InstancedNonIndexed { .. })
    }

    /// Get instance count (1 for single draws)
    pub fn instance_count(&self) -> u32 {
        match self {
            Self::InstancedIndexed { instance_count, .. } => *instance_count,
            Self::InstancedNonIndexed { instance_count, .. } => *instance_count,
            Self::Single { .. } => 1,
        }
    }

    /// Get batch key if instanced
    pub fn batch_key(&self) -> Option<&BatchKey> {
        match self {
            Self::InstancedIndexed { batch_key, .. } => Some(batch_key),
            Self::InstancedNonIndexed { batch_key, .. } => Some(batch_key),
            Self::Single { .. } => None,
        }
    }
}

/// A list of draw commands grouped for efficient rendering
#[derive(Clone, Debug, Default)]
pub struct DrawList {
    /// Opaque instanced draws (front-to-back)
    pub opaque_instanced: Vec<DrawCommand>,

    /// Opaque single draws (front-to-back)
    pub opaque_single: Vec<DrawCommand>,

    /// Transparent instanced draws (back-to-front)
    pub transparent_instanced: Vec<DrawCommand>,

    /// Transparent single draws (back-to-front)
    pub transparent_single: Vec<DrawCommand>,
}

impl DrawList {
    /// Create an empty draw list
    pub fn new() -> Self {
        Self::default()
    }

    /// Add an opaque draw command
    pub fn add_opaque(&mut self, command: DrawCommand) {
        if command.is_instanced() {
            self.opaque_instanced.push(command);
        } else {
            self.opaque_single.push(command);
        }
    }

    /// Add a transparent draw command
    pub fn add_transparent(&mut self, command: DrawCommand) {
        if command.is_instanced() {
            self.transparent_instanced.push(command);
        } else {
            self.transparent_single.push(command);
        }
    }

    /// Get total draw command count
    pub fn total_commands(&self) -> usize {
        self.opaque_instanced.len()
            + self.opaque_single.len()
            + self.transparent_instanced.len()
            + self.transparent_single.len()
    }

    /// Get total instance count across all commands
    pub fn total_instances(&self) -> u32 {
        let opaque: u32 = self.opaque_instanced.iter()
            .chain(self.opaque_single.iter())
            .map(|c| c.instance_count())
            .sum();

        let transparent: u32 = self.transparent_instanced.iter()
            .chain(self.transparent_single.iter())
            .map(|c| c.instance_count())
            .sum();

        opaque + transparent
    }

    /// Clear all draw commands
    pub fn clear(&mut self) {
        self.opaque_instanced.clear();
        self.opaque_single.clear();
        self.transparent_instanced.clear();
        self.transparent_single.clear();
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.opaque_instanced.is_empty()
            && self.opaque_single.is_empty()
            && self.transparent_instanced.is_empty()
            && self.transparent_single.is_empty()
    }
}

/// Draw command statistics
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct DrawStats {
    /// Total draw commands issued
    pub total_commands: u32,
    /// Instanced draw commands
    pub instanced_commands: u32,
    /// Single draw commands
    pub single_commands: u32,
    /// Total instances rendered
    pub total_instances: u32,
    /// Total triangles (if known)
    pub total_triangles: u64,
    /// Draw calls saved by instancing
    pub draw_calls_saved: u32,
}

impl DrawStats {
    /// Calculate statistics from a draw list
    pub fn from_draw_list(list: &DrawList) -> Self {
        let instanced_commands = (list.opaque_instanced.len() + list.transparent_instanced.len()) as u32;
        let single_commands = (list.opaque_single.len() + list.transparent_single.len()) as u32;

        let instanced_instances: u32 = list.opaque_instanced.iter()
            .chain(list.transparent_instanced.iter())
            .map(|c| c.instance_count())
            .sum();

        Self {
            total_commands: instanced_commands + single_commands,
            instanced_commands,
            single_commands,
            total_instances: instanced_instances + single_commands,
            total_triangles: 0, // Would need mesh data
            draw_calls_saved: instanced_instances.saturating_sub(instanced_commands),
        }
    }
}

/// Render pass type for organizing draw commands
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum RenderPassType {
    /// Shadow map pass
    Shadow,
    /// Depth pre-pass
    DepthPrepass,
    /// Main opaque pass
    Opaque,
    /// Transparent pass
    Transparent,
    /// Post-processing
    PostProcess,
    /// UI overlay
    UI,
}

/// A complete render queue for a frame
#[derive(Clone, Debug, Default)]
pub struct RenderQueue {
    /// Draw lists per render pass
    pub passes: alloc::collections::BTreeMap<u8, DrawList>,
}

impl RenderQueue {
    /// Create a new render queue
    pub fn new() -> Self {
        Self::default()
    }

    /// Get or create draw list for a pass
    pub fn get_pass_mut(&mut self, pass: RenderPassType) -> &mut DrawList {
        self.passes.entry(pass as u8).or_insert_with(DrawList::new)
    }

    /// Get draw list for a pass
    pub fn get_pass(&self, pass: RenderPassType) -> Option<&DrawList> {
        self.passes.get(&(pass as u8))
    }

    /// Clear all passes
    pub fn clear(&mut self) {
        for list in self.passes.values_mut() {
            list.clear();
        }
    }

    /// Get total commands across all passes
    pub fn total_commands(&self) -> usize {
        self.passes.values().map(|l| l.total_commands()).sum()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_draw_command_instanced() {
        let key = BatchKey::new(1, 0);
        let cmd = DrawCommand::instanced_indexed(key, 0, 36, 100);

        assert!(cmd.is_instanced());
        assert_eq!(cmd.instance_count(), 100);
        assert_eq!(cmd.batch_key(), Some(&key));
    }

    #[test]
    fn test_draw_command_single() {
        let matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];
        let cmd = DrawCommand::single(42, MeshTypeId::Cube, 0, matrix, 0);

        assert!(!cmd.is_instanced());
        assert_eq!(cmd.instance_count(), 1);
        assert_eq!(cmd.batch_key(), None);
    }

    #[test]
    fn test_draw_list() {
        let mut list = DrawList::new();

        let key = BatchKey::new(1, 0);
        list.add_opaque(DrawCommand::instanced_indexed(key, 0, 36, 50));
        list.add_opaque(DrawCommand::instanced_indexed(key, 0, 36, 30));

        let matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];
        list.add_transparent(DrawCommand::single(1, MeshTypeId::Sphere, 0, matrix, 0));

        assert_eq!(list.total_commands(), 3);
        assert_eq!(list.opaque_instanced.len(), 2);
        assert_eq!(list.transparent_single.len(), 1);
    }

    #[test]
    fn test_draw_stats() {
        let mut list = DrawList::new();

        let key = BatchKey::new(1, 0);
        list.add_opaque(DrawCommand::instanced_indexed(key, 0, 36, 100));

        let stats = DrawStats::from_draw_list(&list);
        assert_eq!(stats.instanced_commands, 1);
        assert_eq!(stats.total_instances, 100);
        assert_eq!(stats.draw_calls_saved, 99); // 100 instances - 1 command
    }

    #[test]
    fn test_render_queue() {
        let mut queue = RenderQueue::new();

        let key = BatchKey::new(1, 0);
        queue.get_pass_mut(RenderPassType::Opaque)
            .add_opaque(DrawCommand::instanced_indexed(key, 0, 36, 50));

        queue.get_pass_mut(RenderPassType::Transparent)
            .add_transparent(DrawCommand::instanced_indexed(key, 0, 36, 10));

        assert_eq!(queue.total_commands(), 2);
        assert!(queue.get_pass(RenderPassType::Opaque).is_some());
        assert!(queue.get_pass(RenderPassType::Shadow).is_none());
    }
}
