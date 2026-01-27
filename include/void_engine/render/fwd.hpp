#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_render module

#include <cstdint>

namespace void_render {

// =============================================================================
// Resource IDs
// =============================================================================

/// Unique resource identifier
struct ResourceId;

/// Render pass identifier
struct PassId;

/// Layer identifier
struct LayerId;

/// Renderer identifier
struct RendererId;

/// Material identifier
struct MaterialId;

/// Mesh handle
struct MeshHandle;

// =============================================================================
// Resource Types
// =============================================================================

/// Texture format enumeration
enum class TextureFormat : std::uint8_t;

/// Texture dimension
enum class TextureDimension : std::uint8_t;

/// Texture usage flags
enum class TextureUsage : std::uint32_t;

/// Buffer usage flags
enum class BufferUsage : std::uint32_t;

/// Texture descriptor
struct TextureDesc;

/// Buffer descriptor
struct BufferDesc;

/// Sampler descriptor
struct SamplerDesc;

/// Filter mode
enum class FilterMode : std::uint8_t;

/// Address mode
enum class AddressMode : std::uint8_t;

/// Compare function
enum class CompareFunction : std::uint8_t;

/// Load operation
enum class LoadOp : std::uint8_t;

/// Store operation
enum class StoreOp : std::uint8_t;

/// Clear value
struct ClearValue;

/// Attachment descriptor
struct AttachmentDesc;

// =============================================================================
// Render Graph Types
// =============================================================================

/// Graph resource handle
struct GraphHandle;

/// Access mode
enum class AccessMode : std::uint8_t;

/// Resource usage type
enum class ResourceUsageType : std::uint8_t;

/// Resource usage descriptor
struct ResourceUsage;

/// Render pass interface
class RenderPass;

/// Pass builder
class PassBuilder;

/// Pass context
class PassContext;

/// Render graph
class RenderGraph;

/// Color attachment
struct ColorAttachment;

/// Depth stencil attachment
struct DepthStencilAttachment;

// =============================================================================
// Mesh Types
// =============================================================================

/// Vertex data
struct Vertex;

/// Index format
enum class IndexFormat : std::uint8_t;

/// Primitive topology
enum class PrimitiveTopology : std::uint8_t;

/// Mesh data
class MeshData;

/// GPU vertex buffer
struct GpuVertexBuffer;

/// GPU index buffer
struct GpuIndexBuffer;

/// Cached primitive
struct CachedPrimitive;

/// Cached mesh
struct CachedMesh;

/// Mesh cache
class MeshCache;

// =============================================================================
// Instancing Types
// =============================================================================

/// Instance data (GPU-ready)
struct InstanceData;

/// Batch key
struct BatchKey;

/// Instance batch
class InstanceBatch;

/// Instance batcher
class InstanceBatcher;

// =============================================================================
// Draw Command Types
// =============================================================================

/// Mesh type identifier
enum class MeshTypeId : std::uint8_t;

/// Draw command
struct DrawCommand;

/// Draw list
class DrawList;

/// Draw statistics
struct DrawStats;

// =============================================================================
// Light Types
// =============================================================================

/// GPU directional light
struct GpuDirectionalLight;

/// GPU point light
struct GpuPointLight;

/// GPU spot light
struct GpuSpotLight;

/// Light counts
struct LightCounts;

/// Light buffer
class LightBuffer;

/// Light extractor
class LightExtractor;

// =============================================================================
// Shadow Types
// =============================================================================

/// Shadow quality
enum class ShadowQuality : std::uint8_t;

/// Shadow update mode
enum class ShadowUpdateMode : std::uint8_t;

/// Shadow configuration
struct ShadowConfig;

/// Shadow cascades
class ShadowCascades;

/// Shadow allocation
struct ShadowAllocation;

/// Shadow atlas
class ShadowAtlas;

/// Shadow buffer
class ShadowBuffer;

// =============================================================================
// Material Types
// =============================================================================

/// GPU material
struct GpuMaterial;

/// Material buffer
class MaterialBuffer;

// =============================================================================
// Pass System Types
// =============================================================================

/// Render pass flags
enum class RenderPassFlags : std::uint32_t;

/// Pass sort mode
enum class PassSortMode : std::uint8_t;

/// Cull mode
enum class CullMode : std::uint8_t;

/// Blend mode
enum class BlendMode : std::uint8_t;

/// Pass quality
enum class PassQuality : std::uint8_t;

/// Pass configuration
struct PassConfig;

/// Render pass system
class RenderPassSystem;

/// Custom render pass interface
class CustomRenderPass;

/// Pass registry
class PassRegistry;

// =============================================================================
// Layer & Compositor Types
// =============================================================================

/// Layer health
class LayerHealth;

/// Layer configuration
struct LayerConfig;

/// Layer
class Layer;

/// Renderer features
struct RendererFeatures;

/// Compositor renderer interface
class CompositorRenderer;

/// Renderer context
class RendererContext;

/// Compositor
class Compositor;

// =============================================================================
// Camera Types
// =============================================================================

/// Projection type
class Projection;

/// Camera
class Camera;

/// Camera mode
enum class CameraMode : std::uint8_t;

/// Camera input
struct CameraInput;

/// Camera controller
class CameraController;

// =============================================================================
// Picking & Spatial Types
// =============================================================================

/// Raycast hit
struct RaycastHit;

/// Raycast query
struct RaycastQuery;

/// BVH node
struct BVHNode;

/// Bounding volume hierarchy
class BVH;

// =============================================================================
// Debug Types
// =============================================================================

/// Render statistics
struct RenderStats;

/// Statistics collector
class StatsCollector;

/// Debug visualization flags
enum class DebugVisualization : std::uint32_t;

/// Debug configuration
struct DebugConfig;

} // namespace void_render
