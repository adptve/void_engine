//! Render Graph - Declarative rendering pipeline
//!
//! The render graph allows defining complex rendering pipelines
//! with automatic resource management and synchronization.

use crate::resource::{
    ResourceId, TextureDesc, TextureFormat, TextureUsage, BufferDesc, BufferUsage,
    AttachmentDesc, LoadOp, StoreOp, ClearValue,
};
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use core::any::Any;
use void_core::Id;

/// Unique identifier for a render pass
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct PassId(pub Id);

impl PassId {
    pub fn from_name(name: &str) -> Self {
        Self(Id::from_name(name))
    }
}

/// Handle to a graph resource
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct GraphHandle {
    /// Resource ID
    pub id: ResourceId,
    /// Version (for tracking reads/writes)
    pub version: u32,
}

impl GraphHandle {
    pub fn new(id: ResourceId) -> Self {
        Self { id, version: 0 }
    }

    pub fn with_version(id: ResourceId, version: u32) -> Self {
        Self { id, version }
    }

    /// Create a new version of this handle (after a write)
    pub fn next_version(&self) -> Self {
        Self {
            id: self.id,
            version: self.version + 1,
        }
    }
}

/// Resource access mode
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AccessMode {
    Read,
    Write,
    ReadWrite,
}

/// Resource usage in a pass
#[derive(Clone, Debug)]
pub struct ResourceUsage {
    /// Handle to the resource
    pub handle: GraphHandle,
    /// How the resource is accessed
    pub access: AccessMode,
    /// Usage type (for textures/buffers)
    pub usage_type: ResourceUsageType,
}

/// Type of resource usage
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ResourceUsageType {
    /// Used as a texture input (sampled)
    TextureInput,
    /// Used as a storage texture
    StorageTexture,
    /// Used as a color attachment
    ColorAttachment,
    /// Used as a depth/stencil attachment
    DepthStencilAttachment,
    /// Used as a vertex buffer
    VertexBuffer,
    /// Used as an index buffer
    IndexBuffer,
    /// Used as a uniform buffer
    UniformBuffer,
    /// Used as a storage buffer
    StorageBuffer,
    /// Used as an indirect buffer
    IndirectBuffer,
}

/// Render pass descriptor
#[derive(Clone)]
pub struct RenderPassDesc {
    /// Pass name
    pub name: String,
    /// Color attachments
    pub color_attachments: Vec<ColorAttachment>,
    /// Depth/stencil attachment
    pub depth_stencil: Option<DepthStencilAttachment>,
    /// Resource reads
    pub reads: Vec<GraphHandle>,
    /// Resource writes
    pub writes: Vec<GraphHandle>,
}

/// Color attachment for a render pass
#[derive(Clone, Debug)]
pub struct ColorAttachment {
    /// Target handle
    pub target: GraphHandle,
    /// Resolve target (for MSAA)
    pub resolve_target: Option<GraphHandle>,
    /// Load operation
    pub load_op: LoadOp,
    /// Store operation
    pub store_op: StoreOp,
    /// Clear value (if load_op is Clear)
    pub clear_value: [f32; 4],
}

/// Depth/stencil attachment for a render pass
#[derive(Clone, Debug)]
pub struct DepthStencilAttachment {
    /// Target handle
    pub target: GraphHandle,
    /// Depth load operation
    pub depth_load_op: LoadOp,
    /// Depth store operation
    pub depth_store_op: StoreOp,
    /// Depth clear value
    pub depth_clear_value: f32,
    /// Stencil load operation
    pub stencil_load_op: LoadOp,
    /// Stencil store operation
    pub stencil_store_op: StoreOp,
    /// Stencil clear value
    pub stencil_clear_value: u32,
    /// Read-only depth
    pub depth_read_only: bool,
    /// Read-only stencil
    pub stencil_read_only: bool,
}

/// Trait for render pass implementations
pub trait RenderPass: Send + Sync {
    /// Get the pass ID
    fn id(&self) -> PassId;

    /// Get the pass name
    fn name(&self) -> &str;

    /// Setup phase - declare resource requirements
    fn setup(&mut self, builder: &mut PassBuilder);

    /// Execute phase - record commands
    fn execute(&self, context: &mut PassContext);
}

/// Builder for configuring a render pass
pub struct PassBuilder<'a> {
    graph: &'a mut RenderGraph,
    pass_id: PassId,
    color_attachments: Vec<ColorAttachment>,
    depth_stencil: Option<DepthStencilAttachment>,
    reads: Vec<GraphHandle>,
    writes: Vec<GraphHandle>,
}

impl<'a> PassBuilder<'a> {
    pub fn new(graph: &'a mut RenderGraph, pass_id: PassId) -> Self {
        Self {
            graph,
            pass_id,
            color_attachments: Vec::new(),
            depth_stencil: None,
            reads: Vec::new(),
            writes: Vec::new(),
        }
    }

    /// Create a new texture
    pub fn create_texture(&mut self, name: &str, desc: TextureDesc) -> GraphHandle {
        self.graph.create_texture(name, desc)
    }

    /// Use the backbuffer as output
    pub fn use_backbuffer(&mut self) -> GraphHandle {
        self.graph.backbuffer_handle()
    }

    /// Read a texture
    pub fn read_texture(&mut self, handle: GraphHandle) -> GraphHandle {
        self.reads.push(handle);
        handle
    }

    /// Write to a texture (as color attachment)
    pub fn write_color(&mut self, handle: GraphHandle, load_op: LoadOp, clear_value: [f32; 4]) -> GraphHandle {
        let new_handle = handle.next_version();
        self.writes.push(new_handle);
        self.color_attachments.push(ColorAttachment {
            target: new_handle,
            resolve_target: None,
            load_op,
            store_op: StoreOp::Store,
            clear_value,
        });
        new_handle
    }

    /// Write to depth/stencil
    pub fn write_depth_stencil(
        &mut self,
        handle: GraphHandle,
        depth_clear: f32,
    ) -> GraphHandle {
        let new_handle = handle.next_version();
        self.writes.push(new_handle);
        self.depth_stencil = Some(DepthStencilAttachment {
            target: new_handle,
            depth_load_op: LoadOp::Clear,
            depth_store_op: StoreOp::Store,
            depth_clear_value: depth_clear,
            stencil_load_op: LoadOp::Clear,
            stencil_store_op: StoreOp::Store,
            stencil_clear_value: 0,
            depth_read_only: false,
            stencil_read_only: false,
        });
        new_handle
    }

    /// Read depth for depth testing
    pub fn read_depth(&mut self, handle: GraphHandle) -> GraphHandle {
        self.reads.push(handle);
        self.depth_stencil = Some(DepthStencilAttachment {
            target: handle,
            depth_load_op: LoadOp::Load,
            depth_store_op: StoreOp::Store,
            depth_clear_value: 1.0,
            stencil_load_op: LoadOp::Load,
            stencil_store_op: StoreOp::Store,
            stencil_clear_value: 0,
            depth_read_only: true,
            stencil_read_only: true,
        });
        handle
    }

    /// Finalize the pass configuration
    pub fn build(self) -> RenderPassDesc {
        RenderPassDesc {
            name: String::new(),
            color_attachments: self.color_attachments,
            depth_stencil: self.depth_stencil,
            reads: self.reads,
            writes: self.writes,
        }
    }
}

/// Context provided during pass execution
pub struct PassContext<'a> {
    /// The render graph
    pub graph: &'a RenderGraph,
    /// Current pass ID
    pub pass_id: PassId,
    /// User data (backend-specific command buffer, etc.)
    pub user_data: &'a mut dyn Any,
}

/// Transient resource info
#[derive(Clone, Debug)]
struct TransientResource {
    /// Resource name
    name: String,
    /// Texture descriptor
    texture_desc: Option<TextureDesc>,
    /// Buffer descriptor
    buffer_desc: Option<BufferDesc>,
    /// First pass that uses this resource
    first_use: Option<PassId>,
    /// Last pass that uses this resource
    last_use: Option<PassId>,
}

/// Compiled pass info
struct CompiledPass {
    /// Pass ID
    id: PassId,
    /// Pass implementation
    pass: Box<dyn RenderPass>,
    /// Pass descriptor
    desc: RenderPassDesc,
}

/// The render graph
pub struct RenderGraph {
    /// Passes in order
    passes: Vec<CompiledPass>,
    /// Transient resources
    transient_resources: BTreeMap<ResourceId, TransientResource>,
    /// Resource name to ID mapping
    resource_names: BTreeMap<String, ResourceId>,
    /// Next resource ID
    next_resource_id: u32,
    /// Backbuffer handle
    backbuffer: GraphHandle,
    /// Viewport size
    viewport_size: [u32; 2],
    /// Is compiled
    compiled: bool,
}

impl RenderGraph {
    /// Create a new render graph
    pub fn new() -> Self {
        let backbuffer_id = ResourceId::from_name("__backbuffer__");
        Self {
            passes: Vec::new(),
            transient_resources: BTreeMap::new(),
            resource_names: BTreeMap::new(),
            next_resource_id: 1,
            backbuffer: GraphHandle::new(backbuffer_id),
            viewport_size: [1920, 1080],
            compiled: false,
        }
    }

    /// Set viewport size
    pub fn set_viewport_size(&mut self, width: u32, height: u32) {
        self.viewport_size = [width, height];
        self.compiled = false;
    }

    /// Get viewport size
    pub fn viewport_size(&self) -> [u32; 2] {
        self.viewport_size
    }

    /// Get backbuffer handle
    pub fn backbuffer_handle(&self) -> GraphHandle {
        self.backbuffer
    }

    /// Create a transient texture
    pub fn create_texture(&mut self, name: &str, desc: TextureDesc) -> GraphHandle {
        let id = ResourceId(Id::from_bits(self.next_resource_id as u64));
        self.next_resource_id += 1;

        self.resource_names.insert(name.to_string(), id);
        self.transient_resources.insert(id, TransientResource {
            name: name.to_string(),
            texture_desc: Some(desc),
            buffer_desc: None,
            first_use: None,
            last_use: None,
        });

        GraphHandle::new(id)
    }

    /// Create a transient buffer
    pub fn create_buffer(&mut self, name: &str, desc: BufferDesc) -> GraphHandle {
        let id = ResourceId(Id::from_bits(self.next_resource_id as u64));
        self.next_resource_id += 1;

        self.resource_names.insert(name.to_string(), id);
        self.transient_resources.insert(id, TransientResource {
            name: name.to_string(),
            texture_desc: None,
            buffer_desc: Some(desc),
            first_use: None,
            last_use: None,
        });

        GraphHandle::new(id)
    }

    /// Get resource by name
    pub fn get_resource(&self, name: &str) -> Option<GraphHandle> {
        self.resource_names.get(name).map(|&id| GraphHandle::new(id))
    }

    /// Add a render pass
    pub fn add_pass<P: RenderPass + 'static>(&mut self, mut pass: P) {
        let pass_id = pass.id();
        let mut builder = PassBuilder::new(self, pass_id);
        pass.setup(&mut builder);
        let desc = builder.build();

        // Update resource lifetimes
        for handle in &desc.reads {
            if let Some(res) = self.transient_resources.get_mut(&handle.id) {
                if res.first_use.is_none() {
                    res.first_use = Some(pass_id);
                }
                res.last_use = Some(pass_id);
            }
        }
        for handle in &desc.writes {
            if let Some(res) = self.transient_resources.get_mut(&handle.id) {
                if res.first_use.is_none() {
                    res.first_use = Some(pass_id);
                }
                res.last_use = Some(pass_id);
            }
        }

        self.passes.push(CompiledPass {
            id: pass_id,
            pass: Box::new(pass),
            desc,
        });

        self.compiled = false;
    }

    /// Compile the render graph
    pub fn compile(&mut self) -> Result<(), String> {
        if self.passes.is_empty() {
            return Err("No passes in render graph".into());
        }

        // Topological sort would go here for dependency ordering
        // For now, we execute in order of addition

        self.compiled = true;
        Ok(())
    }

    /// Execute the render graph
    pub fn execute(&self, user_data: &mut dyn Any) {
        if !self.compiled {
            return;
        }

        for compiled_pass in &self.passes {
            let mut context = PassContext {
                graph: self,
                pass_id: compiled_pass.id,
                user_data,
            };

            compiled_pass.pass.execute(&mut context);
        }
    }

    /// Clear all passes
    pub fn clear(&mut self) {
        self.passes.clear();
        self.transient_resources.clear();
        self.resource_names.clear();
        self.next_resource_id = 1;
        self.compiled = false;
    }

    /// Get pass count
    pub fn pass_count(&self) -> usize {
        self.passes.len()
    }

    /// Get resource count
    pub fn resource_count(&self) -> usize {
        self.transient_resources.len()
    }

    /// Check if compiled
    pub fn is_compiled(&self) -> bool {
        self.compiled
    }
}

impl Default for RenderGraph {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestPass {
        id: PassId,
        name: String,
    }

    impl TestPass {
        fn new(name: &str) -> Self {
            Self {
                id: PassId::from_name(name),
                name: name.to_string(),
            }
        }
    }

    impl RenderPass for TestPass {
        fn id(&self) -> PassId {
            self.id
        }

        fn name(&self) -> &str {
            &self.name
        }

        fn setup(&mut self, builder: &mut PassBuilder) {
            let backbuffer = builder.use_backbuffer();
            builder.write_color(backbuffer, LoadOp::Clear, [0.0, 0.0, 0.0, 1.0]);
        }

        fn execute(&self, _context: &mut PassContext) {
            // Test pass does nothing
        }
    }

    #[test]
    fn test_render_graph() {
        let mut graph = RenderGraph::new();
        graph.add_pass(TestPass::new("main"));

        assert_eq!(graph.pass_count(), 1);

        let result = graph.compile();
        assert!(result.is_ok());
        assert!(graph.is_compiled());
    }
}
