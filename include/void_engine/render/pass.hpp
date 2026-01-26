#pragma once

/// @file pass.hpp
/// @brief Render pass system for void_render

#include "fwd.hpp"
#include "resource.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <algorithm>

namespace void_render {

// =============================================================================
// PassId
// =============================================================================

/// Render pass identifier
struct PassId {
    std::uint32_t index = UINT32_MAX;

    constexpr PassId() noexcept = default;
    constexpr explicit PassId(std::uint32_t idx) noexcept : index(idx) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return index != UINT32_MAX;
    }

    [[nodiscard]] static constexpr PassId invalid() noexcept {
        return PassId{};
    }

    constexpr bool operator==(const PassId& other) const noexcept = default;
};

// =============================================================================
// PassFlags
// =============================================================================

/// Pass execution flags
enum class PassFlags : std::uint32_t {
    None            = 0,
    ClearColor      = 1 << 0,   // Clear color attachments
    ClearDepth      = 1 << 1,   // Clear depth attachment
    ClearStencil    = 1 << 2,   // Clear stencil attachment
    LoadColor       = 1 << 3,   // Load previous color
    LoadDepth       = 1 << 4,   // Load previous depth
    StoreColor      = 1 << 5,   // Store color results
    StoreDepth      = 1 << 6,   // Store depth results
    MsaaResolve     = 1 << 7,   // Resolve MSAA at end
    DepthTest       = 1 << 8,   // Enable depth testing
    DepthWrite      = 1 << 9,   // Enable depth writing
    StencilTest     = 1 << 10,  // Enable stencil testing
    Blending        = 1 << 11,  // Enable blending
    Wireframe       = 1 << 12,  // Render wireframe
    DoubleSided     = 1 << 13,  // Disable backface culling
    Enabled         = 1 << 14,  // Pass is enabled
    AsyncCompute    = 1 << 15,  // Can run on async compute queue
};

/// Bitwise operators for PassFlags
[[nodiscard]] constexpr PassFlags operator|(PassFlags a, PassFlags b) noexcept {
    return static_cast<PassFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

[[nodiscard]] constexpr PassFlags operator&(PassFlags a, PassFlags b) noexcept {
    return static_cast<PassFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

constexpr PassFlags& operator|=(PassFlags& a, PassFlags b) noexcept {
    a = a | b;
    return a;
}

[[nodiscard]] constexpr bool has_flag(PassFlags flags, PassFlags flag) noexcept {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0;
}

/// Common flag combinations
namespace pass_flags {
    /// Clear all and store
    constexpr PassFlags ClearAll = PassFlags::ClearColor | PassFlags::ClearDepth |
                                   PassFlags::StoreColor | PassFlags::StoreDepth |
                                   PassFlags::DepthTest | PassFlags::DepthWrite |
                                   PassFlags::Enabled;

    /// Load and store (continuation pass)
    constexpr PassFlags LoadStore = PassFlags::LoadColor | PassFlags::LoadDepth |
                                    PassFlags::StoreColor | PassFlags::StoreDepth |
                                    PassFlags::DepthTest | PassFlags::DepthWrite |
                                    PassFlags::Enabled;

    /// Depth-only pass
    constexpr PassFlags DepthOnly = PassFlags::ClearDepth | PassFlags::StoreDepth |
                                    PassFlags::DepthTest | PassFlags::DepthWrite |
                                    PassFlags::Enabled;

    /// Post-processing (fullscreen quad)
    constexpr PassFlags PostProcess = PassFlags::ClearColor | PassFlags::StoreColor |
                                      PassFlags::Enabled;

    /// Transparent pass (blending, no depth write)
    constexpr PassFlags Transparent = PassFlags::LoadColor | PassFlags::LoadDepth |
                                      PassFlags::StoreColor | PassFlags::DepthTest |
                                      PassFlags::Blending | PassFlags::Enabled;
}

// =============================================================================
// PassType
// =============================================================================

// =============================================================================
// ResourceState
// =============================================================================

/// Resource state for synchronization
enum class ResourceState : std::uint8_t {
    Undefined = 0,
    Common,
    RenderTarget,
    DepthWrite,
    DepthRead,
    ShaderResource,
    UnorderedAccess,
    CopySource,
    CopyDest,
    Present,
};

/// Built-in pass types
enum class PassType : std::uint8_t {
    Custom = 0,          // User-defined pass
    DepthPrePass,        // Early depth pass
    ShadowMap,           // Shadow map generation
    GBuffer,             // Deferred geometry pass
    Lighting,            // Deferred lighting
    Forward,             // Forward rendering
    ForwardTransparent,  // Forward transparent objects
    Sky,                 // Skybox/atmosphere
    PostProcess,         // Post-processing
    Tonemapping,         // HDR tonemapping
    Fxaa,                // Fast approximate anti-aliasing
    Taa,                 // Temporal anti-aliasing
    Ssao,                // Screen-space ambient occlusion
    Ssr,                 // Screen-space reflections
    Bloom,               // Bloom effect
    DepthOfField,        // Depth of field
    MotionBlur,          // Motion blur
    Debug,               // Debug visualization
    Ui,                  // UI overlay
};

// =============================================================================
// BlendMode
// =============================================================================

/// Blend modes for render passes
enum class BlendMode : std::uint8_t {
    Opaque = 0,          // No blending
    AlphaBlend,          // Standard alpha blending
    Additive,            // Additive blending
    Multiply,            // Multiply blending
    Premultiplied,       // Premultiplied alpha
};

// =============================================================================
// PassAttachment
// =============================================================================

/// Attachment reference for a pass
struct PassAttachment {
    std::string name;                        // Resource name
    TextureFormat format = TextureFormat::Rgba8Unorm;
    LoadOp load_op = LoadOp::Clear;
    StoreOp store_op = StoreOp::Store;
    ClearValue clear_value;
    std::uint32_t sample_count = 1;          // MSAA samples

    /// Create color attachment
    [[nodiscard]] static PassAttachment color(
        const std::string& name,
        TextureFormat format = TextureFormat::Rgba8Unorm,
        LoadOp load = LoadOp::Clear,
        StoreOp store = StoreOp::Store)
    {
        PassAttachment att;
        att.name = name;
        att.format = format;
        att.load_op = load;
        att.store_op = store;
        att.clear_value = ClearValue::with_color(0, 0, 0, 1);
        return att;
    }

    /// Create depth attachment
    [[nodiscard]] static PassAttachment depth(
        const std::string& name,
        TextureFormat format = TextureFormat::Depth32Float,
        LoadOp load = LoadOp::Clear,
        StoreOp store = StoreOp::Store)
    {
        PassAttachment att;
        att.name = name;
        att.format = format;
        att.load_op = load;
        att.store_op = store;
        att.clear_value = ClearValue::depth_value(0.0f);  // Reverse-Z: 0 is far
        return att;
    }

    /// Create depth-stencil attachment
    [[nodiscard]] static PassAttachment depth_stencil(
        const std::string& name,
        LoadOp load = LoadOp::Clear,
        StoreOp store = StoreOp::Store)
    {
        PassAttachment att;
        att.name = name;
        att.format = TextureFormat::Depth24PlusStencil8;
        att.load_op = load;
        att.store_op = store;
        att.clear_value = ClearValue::depth_stencil_value(0.0f, 0);
        return att;
    }
};

// =============================================================================
// PassInput
// =============================================================================

/// Input resource for a pass
struct PassInput {
    std::string name;                        // Resource name
    std::uint32_t binding = 0;               // Shader binding index
    bool is_texture = true;                  // Texture or buffer

    [[nodiscard]] static PassInput texture(const std::string& name, std::uint32_t binding) {
        return PassInput{name, binding, true};
    }

    [[nodiscard]] static PassInput buffer(const std::string& name, std::uint32_t binding) {
        return PassInput{name, binding, false};
    }
};

// =============================================================================
// PassOutput
// =============================================================================

/// Output resource from a pass
struct PassOutput {
    std::string name;                        // Resource name
    TextureFormat format = TextureFormat::Rgba8Unorm;
    float size_scale = 1.0f;                 // Relative to render size

    [[nodiscard]] static PassOutput color(const std::string& name, TextureFormat format = TextureFormat::Rgba8Unorm, float scale = 1.0f) {
        return PassOutput{name, format, scale};
    }

    [[nodiscard]] static PassOutput depth(const std::string& name, float scale = 1.0f) {
        return PassOutput{name, TextureFormat::Depth32Float, scale};
    }
};

// =============================================================================
// PassDescriptor
// =============================================================================

/// Describes a render pass configuration
struct PassDescriptor {
    std::string name;
    PassType type = PassType::Custom;
    PassFlags flags = pass_flags::ClearAll;
    std::int32_t priority = 0;               // Execution order (lower = earlier)

    // Attachments
    std::vector<PassAttachment> color_attachments;
    std::optional<PassAttachment> depth_attachment;

    // Texture formats (for render graph)
    std::vector<TextureFormat> color_formats;
    TextureFormat depth_format = TextureFormat::Depth24PlusStencil8;

    // Dimensions (for render graph)
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // Resource dependencies
    std::vector<PassInput> inputs;
    std::vector<PassOutput> outputs;

    // Viewport
    float viewport_scale = 1.0f;             // Relative to render size
    std::optional<std::array<std::uint32_t, 2>> fixed_size;  // Override size

    // Blend mode
    BlendMode blend_mode = BlendMode::Opaque;

    // Layer mask (which layers this pass renders)
    std::uint32_t layer_mask = 0xFFFFFFFF;

    /// Builder pattern
    PassDescriptor& with_name(const std::string& n) { name = n; return *this; }
    PassDescriptor& with_type(PassType t) { type = t; return *this; }
    PassDescriptor& with_flags(PassFlags f) { flags = f; return *this; }
    PassDescriptor& with_priority(std::int32_t p) { priority = p; return *this; }
    PassDescriptor& with_color(const PassAttachment& att) { color_attachments.push_back(att); return *this; }
    PassDescriptor& with_depth(const PassAttachment& att) { depth_attachment = att; return *this; }
    PassDescriptor& with_input(const PassInput& in) { inputs.push_back(in); return *this; }
    PassDescriptor& with_output(const PassOutput& out) { outputs.push_back(out); return *this; }
    PassDescriptor& with_blend(BlendMode mode) { blend_mode = mode; return *this; }
    PassDescriptor& with_layer_mask(std::uint32_t mask) { layer_mask = mask; return *this; }
    PassDescriptor& with_scale(float scale) { viewport_scale = scale; return *this; }

    /// Static factory methods
    [[nodiscard]] static PassDescriptor color_pass(std::string name, TextureFormat format,
                                                    std::uint32_t width, std::uint32_t height);
    [[nodiscard]] static PassDescriptor depth_pass(std::string name, TextureFormat format,
                                                    std::uint32_t width, std::uint32_t height);
    [[nodiscard]] static PassDescriptor shadow_pass(std::string name, std::uint32_t resolution);
};

// =============================================================================
// PassContext
// =============================================================================

/// Context passed to pass execute callbacks
struct PassContext {
    std::uint32_t frame_index = 0;
    float delta_time = 0.016f;
    std::array<std::uint32_t, 2> render_size = {1920, 1080};
    std::array<std::uint32_t, 2> viewport_size = {1920, 1080};
    std::array<std::uint32_t, 2> viewport_offset = {0, 0};

    // Resource handles (backend-specific, stored as opaque pointers)
    void* color_target = nullptr;
    void* depth_target = nullptr;
    void* command_encoder = nullptr;

    // User data
    void* user_data = nullptr;
};

/// Alias for PassContext (legacy name)
using RenderContext = PassContext;

/// Render callback type
using RenderCallback = std::function<void(const RenderContext&)>;

// =============================================================================
// RenderPass (abstract base)
// =============================================================================

/// Resource dependency for render passes
struct ResourceDependency {
    ResourceId resource;
    ResourceState state;
};

/// Base class for render passes
class RenderPass {
public:
    /// Render callback type (nested for compatibility)
    using RenderCallback = std::function<void(const RenderContext&)>;

    /// Constructor
    explicit RenderPass(const PassDescriptor& desc)
        : m_descriptor(desc)
        , m_id(ResourceId::from_name(desc.name))
        , m_enabled(has_flag(desc.flags, PassFlags::Enabled)) {}

    /// Virtual destructor
    virtual ~RenderPass() = default;

    /// Get descriptor
    [[nodiscard]] const PassDescriptor& descriptor() const noexcept { return m_descriptor; }

    /// Get ID
    [[nodiscard]] ResourceId id() const noexcept { return m_id; }

    /// Get name
    [[nodiscard]] const std::string& name() const noexcept { return m_descriptor.name; }

    /// Get type
    [[nodiscard]] PassType type() const noexcept { return m_descriptor.type; }

    /// Get priority
    [[nodiscard]] std::int32_t priority() const noexcept { return m_descriptor.priority; }

    /// Check if enabled
    [[nodiscard]] bool is_enabled() const noexcept { return m_enabled; }

    /// Set enabled state
    void set_enabled(bool enabled) { m_enabled = enabled; }

    /// Add pass dependency
    void add_dependency(ResourceId pass_id);

    /// Declare input resource
    void declare_input(ResourceId resource, ResourceState required_state);

    /// Declare output resource
    void declare_output(ResourceId resource, ResourceState output_state);

    /// Get dependencies
    [[nodiscard]] const std::vector<ResourceId>& dependencies() const noexcept { return m_dependencies; }

    /// Get inputs
    [[nodiscard]] const std::vector<ResourceDependency>& inputs() const noexcept { return m_inputs; }

    /// Get outputs
    [[nodiscard]] const std::vector<ResourceDependency>& outputs() const noexcept { return m_outputs; }

    /// Prepare pass (called once before first execute)
    virtual void prepare(const PassContext& ctx) { (void)ctx; }

    /// Execute pass
    virtual void execute(const PassContext& ctx) = 0;

    /// Resize (called when render size changes)
    virtual void resize(std::uint32_t width, std::uint32_t height) {
        (void)width;
        (void)height;
    }

    /// Cleanup
    virtual void cleanup() {}

protected:
    PassDescriptor m_descriptor;
    ResourceId m_id;
    bool m_enabled;
    std::vector<ResourceId> m_dependencies;
    std::vector<ResourceDependency> m_inputs;
    std::vector<ResourceDependency> m_outputs;
};

// =============================================================================
// CallbackPass
// =============================================================================

/// Pass with callback-based execution
class CallbackPass : public RenderPass {
public:
    using ExecuteCallback = std::function<void(const PassContext&)>;
    using ResizeCallback = std::function<void(std::uint32_t, std::uint32_t)>;

    CallbackPass(const PassDescriptor& desc, ExecuteCallback execute_cb)
        : RenderPass(desc)
        , m_execute_cb(std::move(execute_cb)) {}

    void set_resize_callback(ResizeCallback cb) { m_resize_cb = std::move(cb); }

    void execute(const PassContext& ctx) override {
        if (m_execute_cb) {
            m_execute_cb(ctx);
        }
    }

    void resize(std::uint32_t width, std::uint32_t height) override {
        if (m_resize_cb) {
            m_resize_cb(width, height);
        }
    }

private:
    ExecuteCallback m_execute_cb;
    ResizeCallback m_resize_cb;
};

// =============================================================================
// PassFactory
// =============================================================================

/// Factory function for creating render passes
using PassFactory = std::function<std::unique_ptr<RenderPass>(const PassDescriptor&)>;

// =============================================================================
// PassStats
// =============================================================================

/// Statistics for a render pass
struct PassStats {
    std::string name;
    float gpu_time_ms = 0.0f;          // GPU execution time
    float cpu_time_ms = 0.0f;          // CPU submission time
    std::uint32_t draw_calls = 0;
    std::uint32_t triangles = 0;
    std::uint32_t instances = 0;
    std::uint32_t state_changes = 0;   // Pipeline/binding changes

    void reset() {
        gpu_time_ms = 0.0f;
        cpu_time_ms = 0.0f;
        draw_calls = 0;
        triangles = 0;
        instances = 0;
        state_changes = 0;
    }
};

// =============================================================================
// PassRegistry
// =============================================================================

/// Manages render passes
class PassRegistry {
public:
    /// Add pass
    PassId add(std::unique_ptr<RenderPass> pass) {
        std::uint32_t index = static_cast<std::uint32_t>(m_passes.size());
        m_name_to_id[pass->name()] = PassId(index);
        m_passes.push_back(std::move(pass));
        m_stats.push_back(PassStats{});
        m_sorted = false;
        return PassId(index);
    }

    /// Add callback pass
    PassId add_callback(const PassDescriptor& desc, CallbackPass::ExecuteCallback callback) {
        return add(std::make_unique<CallbackPass>(desc, std::move(callback)));
    }

    /// Get pass by ID
    [[nodiscard]] RenderPass* get(PassId id) {
        if (!id.is_valid() || id.index >= m_passes.size()) {
            return nullptr;
        }
        return m_passes[id.index].get();
    }

    [[nodiscard]] const RenderPass* get(PassId id) const {
        if (!id.is_valid() || id.index >= m_passes.size()) {
            return nullptr;
        }
        return m_passes[id.index].get();
    }

    /// Get pass by name
    [[nodiscard]] RenderPass* get(const std::string& name) {
        auto it = m_name_to_id.find(name);
        if (it == m_name_to_id.end()) {
            return nullptr;
        }
        return get(it->second);
    }

    /// Get pass ID by name
    [[nodiscard]] std::optional<PassId> get_id(const std::string& name) const {
        auto it = m_name_to_id.find(name);
        if (it == m_name_to_id.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    /// Remove pass
    bool remove(PassId id) {
        if (!id.is_valid() || id.index >= m_passes.size()) {
            return false;
        }
        m_name_to_id.erase(m_passes[id.index]->name());
        m_passes[id.index].reset();
        return true;
    }

    /// Get pass count
    [[nodiscard]] std::size_t count() const noexcept {
        return m_passes.size();
    }

    /// Sort passes by priority
    void sort() {
        if (m_sorted) return;

        m_execution_order.clear();
        for (std::uint32_t i = 0; i < m_passes.size(); ++i) {
            if (m_passes[i]) {
                m_execution_order.push_back(i);
            }
        }

        std::sort(m_execution_order.begin(), m_execution_order.end(),
            [this](std::uint32_t a, std::uint32_t b) {
                return m_passes[a]->priority() < m_passes[b]->priority();
            });

        m_sorted = true;
    }

    /// Get sorted execution order
    [[nodiscard]] const std::vector<std::uint32_t>& execution_order() const {
        return m_execution_order;
    }

    /// Execute all enabled passes in order
    void execute_all(const PassContext& ctx) {
        sort();

        for (std::uint32_t index : m_execution_order) {
            auto& pass = m_passes[index];
            if (pass && pass->is_enabled()) {
                pass->execute(ctx);
            }
        }
    }

    /// Prepare all passes
    void prepare_all(const PassContext& ctx) {
        for (auto& pass : m_passes) {
            if (pass) {
                pass->prepare(ctx);
            }
        }
    }

    /// Resize all passes
    void resize_all(std::uint32_t width, std::uint32_t height) {
        for (auto& pass : m_passes) {
            if (pass) {
                pass->resize(width, height);
            }
        }
    }

    /// Get stats for pass
    [[nodiscard]] PassStats& stats(PassId id) {
        return m_stats[id.index];
    }

    /// Get all stats
    [[nodiscard]] const std::vector<PassStats>& all_stats() const noexcept {
        return m_stats;
    }

    /// Clear all passes
    void clear() {
        m_passes.clear();
        m_name_to_id.clear();
        m_execution_order.clear();
        m_stats.clear();
        m_sorted = false;
    }

    /// Iterate passes
    template<typename F>
    void for_each(F&& callback) {
        for (auto& pass : m_passes) {
            if (pass) {
                callback(*pass);
            }
        }
    }

    template<typename F>
    void for_each(F&& callback) const {
        for (const auto& pass : m_passes) {
            if (pass) {
                callback(*pass);
            }
        }
    }

    /// Get singleton instance
    [[nodiscard]] static PassRegistry& instance();

    /// Register pass factory
    void register_pass(const std::string& name, PassFactory factory);

    /// Create pass from factory
    [[nodiscard]] std::unique_ptr<RenderPass> create(const std::string& name, const PassDescriptor& desc);

    /// Check if pass type is registered
    [[nodiscard]] bool has(const std::string& name) const;

    /// Get all registered pass names
    [[nodiscard]] std::vector<std::string> registered_passes() const;

private:
    std::vector<std::unique_ptr<RenderPass>> m_passes;
    std::unordered_map<std::string, PassId> m_name_to_id;
    std::unordered_map<std::string, PassFactory> m_factories;
    std::vector<std::uint32_t> m_execution_order;
    std::vector<PassStats> m_stats;
    bool m_sorted = false;
};

// =============================================================================
// Built-in Pass Descriptors
// =============================================================================

namespace builtin_passes {

/// Depth pre-pass descriptor
[[nodiscard]] inline PassDescriptor depth_prepass() {
    PassDescriptor desc;
    desc.name = "depth_prepass";
    desc.type = PassType::DepthPrePass;
    desc.flags = pass_flags::DepthOnly;
    desc.priority = -100;  // Run early
    desc.depth_attachment = PassAttachment::depth("scene_depth");
    return desc;
}

/// Shadow map pass descriptor
[[nodiscard]] inline PassDescriptor shadow_map() {
    PassDescriptor desc;
    desc.name = "shadow_map";
    desc.type = PassType::ShadowMap;
    desc.flags = pass_flags::DepthOnly;
    desc.priority = -90;
    desc.depth_attachment = PassAttachment::depth("shadow_depth", TextureFormat::Depth32Float);
    return desc;
}

/// GBuffer pass descriptor (deferred)
[[nodiscard]] inline PassDescriptor gbuffer() {
    PassDescriptor desc;
    desc.name = "gbuffer";
    desc.type = PassType::GBuffer;
    desc.flags = pass_flags::ClearAll;
    desc.priority = 0;
    desc.color_attachments = {
        PassAttachment::color("gbuffer_albedo", TextureFormat::Rgba8Unorm),
        PassAttachment::color("gbuffer_normal", TextureFormat::Rgba16Float),
        PassAttachment::color("gbuffer_material", TextureFormat::Rgba8Unorm),  // metallic, roughness, ao, flags
        PassAttachment::color("gbuffer_emissive", TextureFormat::Rgba16Float),
    };
    desc.depth_attachment = PassAttachment::depth("scene_depth", TextureFormat::Depth32Float, LoadOp::Load);
    return desc;
}

/// Deferred lighting pass descriptor
[[nodiscard]] inline PassDescriptor deferred_lighting() {
    PassDescriptor desc;
    desc.name = "deferred_lighting";
    desc.type = PassType::Lighting;
    desc.flags = pass_flags::PostProcess;
    desc.priority = 10;
    desc.inputs = {
        PassInput::texture("gbuffer_albedo", 0),
        PassInput::texture("gbuffer_normal", 1),
        PassInput::texture("gbuffer_material", 2),
        PassInput::texture("gbuffer_emissive", 3),
        PassInput::texture("scene_depth", 4),
        PassInput::texture("shadow_depth", 5),
    };
    desc.color_attachments = {
        PassAttachment::color("hdr_color", TextureFormat::Rgba16Float),
    };
    return desc;
}

/// Forward pass descriptor
[[nodiscard]] inline PassDescriptor forward() {
    PassDescriptor desc;
    desc.name = "forward";
    desc.type = PassType::Forward;
    desc.flags = pass_flags::ClearAll;
    desc.priority = 0;
    desc.color_attachments = {
        PassAttachment::color("hdr_color", TextureFormat::Rgba16Float),
    };
    desc.depth_attachment = PassAttachment::depth("scene_depth");
    return desc;
}

/// Forward transparent pass descriptor
[[nodiscard]] inline PassDescriptor forward_transparent() {
    PassDescriptor desc;
    desc.name = "forward_transparent";
    desc.type = PassType::ForwardTransparent;
    desc.flags = pass_flags::Transparent;
    desc.priority = 20;
    desc.blend_mode = BlendMode::AlphaBlend;
    desc.inputs = {
        PassInput::texture("scene_depth", 0),
    };
    desc.color_attachments = {
        PassAttachment::color("hdr_color", TextureFormat::Rgba16Float, LoadOp::Load),
    };
    desc.depth_attachment = PassAttachment::depth("scene_depth", TextureFormat::Depth32Float, LoadOp::Load, StoreOp::Discard);
    return desc;
}

/// Sky pass descriptor
[[nodiscard]] inline PassDescriptor sky() {
    PassDescriptor desc;
    desc.name = "sky";
    desc.type = PassType::Sky;
    desc.flags = PassFlags::LoadColor | PassFlags::LoadDepth | PassFlags::StoreColor |
                 PassFlags::DepthTest | PassFlags::Enabled;
    desc.priority = 15;
    desc.color_attachments = {
        PassAttachment::color("hdr_color", TextureFormat::Rgba16Float, LoadOp::Load),
    };
    desc.depth_attachment = PassAttachment::depth("scene_depth", TextureFormat::Depth32Float, LoadOp::Load, StoreOp::Discard);
    return desc;
}

/// SSAO pass descriptor
[[nodiscard]] inline PassDescriptor ssao() {
    PassDescriptor desc;
    desc.name = "ssao";
    desc.type = PassType::Ssao;
    desc.flags = pass_flags::PostProcess | PassFlags::AsyncCompute;
    desc.priority = 25;
    desc.viewport_scale = 0.5f;  // Half resolution
    desc.inputs = {
        PassInput::texture("scene_depth", 0),
        PassInput::texture("gbuffer_normal", 1),
    };
    desc.color_attachments = {
        PassAttachment::color("ssao", TextureFormat::R8Unorm),
    };
    return desc;
}

/// Bloom pass descriptor
[[nodiscard]] inline PassDescriptor bloom() {
    PassDescriptor desc;
    desc.name = "bloom";
    desc.type = PassType::Bloom;
    desc.flags = pass_flags::PostProcess;
    desc.priority = 50;
    desc.inputs = {
        PassInput::texture("hdr_color", 0),
    };
    desc.color_attachments = {
        PassAttachment::color("bloom", TextureFormat::Rgba16Float),
    };
    return desc;
}

/// Tonemapping pass descriptor
[[nodiscard]] inline PassDescriptor tonemapping() {
    PassDescriptor desc;
    desc.name = "tonemapping";
    desc.type = PassType::Tonemapping;
    desc.flags = pass_flags::PostProcess;
    desc.priority = 100;
    desc.inputs = {
        PassInput::texture("hdr_color", 0),
        PassInput::texture("bloom", 1),
        PassInput::texture("ssao", 2),
    };
    desc.color_attachments = {
        PassAttachment::color("ldr_color", TextureFormat::Rgba8Unorm),
    };
    return desc;
}

/// FXAA pass descriptor
[[nodiscard]] inline PassDescriptor fxaa() {
    PassDescriptor desc;
    desc.name = "fxaa";
    desc.type = PassType::Fxaa;
    desc.flags = pass_flags::PostProcess;
    desc.priority = 110;
    desc.inputs = {
        PassInput::texture("ldr_color", 0),
    };
    desc.color_attachments = {
        PassAttachment::color("final_color", TextureFormat::Rgba8Unorm),
    };
    return desc;
}

/// Debug pass descriptor
[[nodiscard]] inline PassDescriptor debug_overlay() {
    PassDescriptor desc;
    desc.name = "debug_overlay";
    desc.type = PassType::Debug;
    desc.flags = PassFlags::LoadColor | PassFlags::StoreColor | PassFlags::Blending | PassFlags::Enabled;
    desc.priority = 200;
    desc.blend_mode = BlendMode::AlphaBlend;
    desc.color_attachments = {
        PassAttachment::color("final_color", TextureFormat::Rgba8Unorm, LoadOp::Load),
    };
    return desc;
}

/// UI pass descriptor
[[nodiscard]] inline PassDescriptor ui() {
    PassDescriptor desc;
    desc.name = "ui";
    desc.type = PassType::Ui;
    desc.flags = PassFlags::LoadColor | PassFlags::StoreColor | PassFlags::Blending | PassFlags::Enabled;
    desc.priority = 250;
    desc.blend_mode = BlendMode::AlphaBlend;
    desc.color_attachments = {
        PassAttachment::color("final_color", TextureFormat::Rgba8Unorm, LoadOp::Load),
    };
    return desc;
}

} // namespace builtin_passes

} // namespace void_render

// Hash specialization
template<>
struct std::hash<void_render::PassId> {
    std::size_t operator()(const void_render::PassId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.index);
    }
};
