# void_render Module

Render graph abstraction for GPU-accelerated rendering with hot-swappable shaders and multi-layer composition.

## Render Graph

Declarative rendering pipeline with automatic resource management:

```cpp
namespace void_engine::render {

class RenderGraph {
public:
    // Resource creation
    TextureHandle create_texture(TextureDesc const& desc);
    BufferHandle create_buffer(BufferDesc const& desc);

    // Pass creation
    template<typename SetupFn, typename ExecuteFn>
    void add_pass(std::string_view name, SetupFn&& setup, ExecuteFn&& execute);

    // Compilation and execution
    void compile();  // Resource aliasing, barrier optimization
    void execute(CommandBuffer& cmd);

    // Hot-reload support
    void invalidate_pass(std::string_view name);
    void rebuild();

private:
    std::vector<RenderPass> m_passes;
    ResourceRegistry m_resources;
    std::vector<ResourceBarrier> m_barriers;
};

struct TextureDesc {
    uint32_t width;
    uint32_t height;
    uint32_t depth = 1;
    TextureFormat format;
    TextureUsage usage;
    uint32_t mip_levels = 1;
    uint32_t sample_count = 1;
    std::string_view label;
};

struct BufferDesc {
    size_t size;
    BufferUsage usage;
    MemoryLocation location;
    std::string_view label;
};

} // namespace void_engine::render
```

## Render Pass

Individual rendering operation:

```cpp
namespace void_engine::render {

struct PassResources {
    std::vector<TextureRead> texture_reads;
    std::vector<TextureWrite> texture_writes;
    std::vector<BufferRead> buffer_reads;
    std::vector<BufferWrite> buffer_writes;
};

class RenderPass {
public:
    std::string_view name() const;
    PassResources const& resources() const;

    virtual void setup(PassResources& resources) = 0;
    virtual void execute(CommandBuffer& cmd, PassData const& data) = 0;
};

// Built-in passes
class GeometryPass : public RenderPass { /* ... */ };
class LightingPass : public RenderPass { /* ... */ };
class ShadowPass : public RenderPass { /* ... */ };
class PostProcessPass : public RenderPass { /* ... */ };
class UIPass : public RenderPass { /* ... */ };

} // namespace void_engine::render
```

## Multi-Layer Composition

Layer-based rendering for visual isolation:

```cpp
namespace void_engine::render {

enum class LayerType {
    Content,   // 3D world content
    Effect,    // Post-processing effects
    Overlay,   // 2D UI overlays
    Portal     // Render-to-texture portals
};

enum class BlendMode {
    Replace,   // Opaque overwrite
    Normal,    // Alpha blend
    Additive,  // Add RGB
    Multiply,  // Multiply RGB
    Screen     // Screen blend
};

struct LayerConfig {
    LayerType type = LayerType::Content;
    int32_t priority = 0;  // Higher = rendered on top
    BlendMode blend = BlendMode::Normal;
    bool visible = true;
    float opacity = 1.0f;
    std::optional<Viewport> viewport;
};

class Layer {
public:
    LayerId id() const;
    std::string_view name() const;
    LayerConfig const& config() const;

    void set_visible(bool visible);
    void set_priority(int32_t priority);
    void set_blend_mode(BlendMode mode);

    // Render target for this layer
    TextureHandle render_target() const;
    TextureHandle depth_target() const;

private:
    LayerId m_id;
    std::string m_name;
    LayerConfig m_config;
    TextureHandle m_color_target;
    TextureHandle m_depth_target;
};

class LayerCompositor {
public:
    void add_layer(Layer layer);
    void remove_layer(LayerId id);
    void update_layer(LayerId id, LayerConfig const& config);

    Layer* get_layer(LayerId id);
    std::span<Layer* const> sorted_layers() const;

    // Composite all layers in priority order
    void composite(CommandBuffer& cmd, TextureHandle output);

    // Hot-swap support
    void on_layer_hot_reload(LayerId id);

private:
    std::vector<std::unique_ptr<Layer>> m_layers;
    std::vector<Layer*> m_sorted;  // By priority
    bool m_needs_sort = false;
};

} // namespace void_engine::render
```

## Material System

PBR materials with shader hot-reload:

```cpp
namespace void_engine::render {

struct MaterialProperties {
    glm::vec4 base_color{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f};
    float emissive_strength = 1.0f;
};

class Material {
public:
    ShaderId shader() const;
    MaterialProperties const& properties() const;
    BlendMode blend_mode() const;
    bool double_sided() const;

    void set_property(std::string_view name, Value value);
    void set_texture(std::string_view slot, TextureHandle texture);

    // Hot-reload callback
    void on_shader_reloaded(ShaderModule const& new_shader);

private:
    ShaderId m_shader;
    MaterialProperties m_properties;
    std::unordered_map<std::string, TextureHandle> m_textures;
    std::unordered_map<std::string, Value> m_uniforms;
    bool m_dirty = false;
};

class MaterialCache {
public:
    MaterialHandle create(std::string_view name, MaterialDesc const& desc);
    Material* get(MaterialHandle handle);
    void reload(MaterialHandle handle);

    // Hot-reload all materials using a shader
    void on_shader_changed(ShaderId shader);

private:
    HandleMap<Material, struct MaterialTag> m_materials;
};

} // namespace void_engine::render
```

## Mesh Rendering

Geometry and instancing:

```cpp
namespace void_engine::render {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 tangent;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    BoundingBox bounds;
};

class Mesh {
public:
    MeshHandle handle() const;
    BoundingBox const& bounds() const;
    size_t vertex_count() const;
    size_t index_count() const;

    // GPU resources
    BufferHandle vertex_buffer() const;
    BufferHandle index_buffer() const;

private:
    MeshHandle m_handle;
    MeshData m_data;
    BufferHandle m_vertex_buffer;
    BufferHandle m_index_buffer;
};

class MeshRenderer {
public:
    void submit(MeshHandle mesh, MaterialHandle material,
                glm::mat4 const& transform, LayerId layer);

    void submit_instanced(MeshHandle mesh, MaterialHandle material,
                          std::span<glm::mat4 const> transforms, LayerId layer);

    // Called by render graph
    void flush(CommandBuffer& cmd, LayerId layer);

private:
    struct DrawCall {
        MeshHandle mesh;
        MaterialHandle material;
        std::vector<glm::mat4> transforms;
    };
    std::unordered_map<LayerId, std::vector<DrawCall>> m_draw_calls;
};

} // namespace void_engine::render
```

## Camera System

```cpp
namespace void_engine::render {

enum class ProjectionType {
    Perspective,
    Orthographic
};

struct CameraData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::vec3 position;
    glm::vec3 forward;
    float near_plane;
    float far_plane;
};

class Camera {
public:
    void set_perspective(float fov_radians, float aspect, float near, float far);
    void set_orthographic(float width, float height, float near, float far);

    void look_at(glm::vec3 position, glm::vec3 target, glm::vec3 up);
    void set_transform(glm::mat4 const& transform);

    CameraData const& data() const;
    Frustum frustum() const;

    // For culling
    bool is_visible(BoundingBox const& bounds) const;
    bool is_visible(BoundingSphere const& bounds) const;

private:
    ProjectionType m_type = ProjectionType::Perspective;
    CameraData m_data;
    Frustum m_frustum;
    bool m_dirty = true;
};

} // namespace void_engine::render
```

## Post-Processing

```cpp
namespace void_engine::render {

class PostProcessStack {
public:
    void add_effect(std::unique_ptr<PostEffect> effect);
    void remove_effect(std::string_view name);
    void set_enabled(std::string_view name, bool enabled);

    void process(CommandBuffer& cmd, TextureHandle input, TextureHandle output);

    // Hot-reload
    void on_shader_changed(ShaderId shader);

private:
    std::vector<std::unique_ptr<PostEffect>> m_effects;
};

class PostEffect {
public:
    virtual std::string_view name() const = 0;
    virtual void process(CommandBuffer& cmd, TextureHandle input, TextureHandle output) = 0;
    virtual void on_shader_reload() = 0;

    bool enabled() const { return m_enabled; }
    void set_enabled(bool enabled) { m_enabled = enabled; }

protected:
    bool m_enabled = true;
};

// Built-in effects
class BloomEffect : public PostEffect { /* ... */ };
class TonemapEffect : public PostEffect { /* ... */ };
class FXAAEffect : public PostEffect { /* ... */ };
class ChromaticAberrationEffect : public PostEffect { /* ... */ };
class VignetteEffect : public PostEffect { /* ... */ };

} // namespace void_engine::render
```

## GPU Backend Abstraction

```cpp
namespace void_engine::render {

// Platform-agnostic command buffer
class CommandBuffer {
public:
    void begin_render_pass(RenderPassDesc const& desc);
    void end_render_pass();

    void set_pipeline(PipelineHandle pipeline);
    void set_vertex_buffer(uint32_t slot, BufferHandle buffer);
    void set_index_buffer(BufferHandle buffer, IndexFormat format);
    void set_bind_group(uint32_t slot, BindGroupHandle group);

    void draw(uint32_t vertex_count, uint32_t instance_count,
              uint32_t first_vertex, uint32_t first_instance);
    void draw_indexed(uint32_t index_count, uint32_t instance_count,
                      uint32_t first_index, int32_t vertex_offset,
                      uint32_t first_instance);

    void dispatch(uint32_t x, uint32_t y, uint32_t z);

    void copy_buffer_to_buffer(BufferHandle src, BufferHandle dst, size_t size);
    void copy_texture_to_texture(TextureHandle src, TextureHandle dst);

private:
    // Backend-specific implementation
};

// Abstract GPU device
class IDevice {
public:
    virtual ~IDevice() = default;

    virtual BufferHandle create_buffer(BufferDesc const& desc) = 0;
    virtual TextureHandle create_texture(TextureDesc const& desc) = 0;
    virtual ShaderHandle create_shader(ShaderDesc const& desc) = 0;
    virtual PipelineHandle create_pipeline(PipelineDesc const& desc) = 0;

    virtual void destroy(BufferHandle handle) = 0;
    virtual void destroy(TextureHandle handle) = 0;

    virtual CommandBuffer begin_frame() = 0;
    virtual void submit(CommandBuffer& cmd) = 0;
    virtual void present() = 0;
};

// Factory
std::unique_ptr<IDevice> create_device(DeviceDesc const& desc);

} // namespace void_engine::render
```
