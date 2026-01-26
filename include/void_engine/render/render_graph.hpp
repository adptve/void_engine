#pragma once

/// @file render_graph.hpp
/// @brief Render graph for declarative render pipeline management

#include "fwd.hpp"
#include "pass.hpp"
#include "resource.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

#include <glm/glm.hpp>

namespace void_render {

// Forward declarations
class RenderPass;
struct PassDescriptor;

// =============================================================================
// RenderGraph
// =============================================================================

/// Render graph for managing render pass execution order and dependencies
class RenderGraph {
public:
    RenderGraph();
    ~RenderGraph();

    /// Add a pass to the graph
    ResourceId add_pass(std::unique_ptr<RenderPass> pass);

    /// Add a callback pass
    ResourceId add_callback_pass(const std::string& name,
                                  RenderPass::RenderCallback callback);

    /// Add dependency between passes (from must complete before to)
    void add_dependency(ResourceId from, ResourceId to);

    /// Remove a pass
    void remove_pass(ResourceId id);

    /// Get pass by ID
    RenderPass* get_pass(ResourceId id);

    /// Compile the graph (topological sort)
    bool compile();

    /// Execute all passes in order
    void execute(const RenderContext& ctx);

    /// Clear all passes
    void clear();

private:
    std::unordered_map<ResourceId, std::unique_ptr<RenderPass>> m_passes;
    std::vector<ResourceId> m_execution_order;
};

// =============================================================================
// LayerId (for render graph layers)
// =============================================================================

/// Layer identifier for render graph layer system
class LayerId {
public:
    constexpr LayerId() noexcept = default;
    constexpr explicit LayerId(std::uint32_t v) noexcept : m_value(v) {}

    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return m_value; }

    constexpr bool operator==(const LayerId& other) const noexcept = default;

    /// Hash function for use with unordered containers
    struct Hash {
        std::size_t operator()(const LayerId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value());
        }
    };

    /// Main render layer
    [[nodiscard]] static LayerId main();
    /// UI render layer
    [[nodiscard]] static LayerId ui();
    /// Debug render layer
    [[nodiscard]] static LayerId debug();

private:
    std::uint32_t m_value = 0;
};

// =============================================================================
// RenderLayer (for render graph)
// =============================================================================

/// Render layer containing passes
class RenderLayer {
public:
    RenderLayer() = default;
    RenderLayer(LayerId id, std::string name);

    [[nodiscard]] LayerId id() const noexcept { return m_id; }
    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<ResourceId>& passes() const noexcept { return m_passes; }

    [[nodiscard]] bool is_visible() const noexcept { return m_visible; }
    void set_visible(bool visible) { m_visible = visible; }

    [[nodiscard]] std::int32_t priority() const noexcept { return m_priority; }
    void set_priority(std::int32_t p) { m_priority = p; }

    void add_pass(ResourceId pass_id);
    void remove_pass(ResourceId pass_id);
    void clear_passes();

private:
    LayerId m_id;
    std::string m_name;
    std::vector<ResourceId> m_passes;
    std::int32_t m_priority = 0;
    bool m_visible = true;
};

// =============================================================================
// LayerManager (for render graph)
// =============================================================================

/// Manages render layers
class LayerManager {
public:
    LayerManager();
    ~LayerManager();

    /// Create a new layer
    LayerId create_layer(std::string name, std::int32_t priority = 0);

    /// Destroy a layer
    void destroy_layer(LayerId id);

    /// Get layer by ID
    RenderLayer* get_layer(LayerId id);

    /// Sort layers by priority
    void sort_layers();

    /// Get sorted layer pointers
    const std::vector<RenderLayer*>& sorted_layers();

private:
    std::unordered_map<LayerId, RenderLayer, LayerId::Hash> m_layers;
    std::vector<RenderLayer*> m_sorted_layers;
    std::uint32_t m_next_id = 0;
};

// =============================================================================
// View (for render graph)
// =============================================================================

/// Represents a camera view for rendering
class View {
public:
    View() = default;
    View(std::uint32_t width, std::uint32_t height);

    void set_viewport(std::uint32_t x, std::uint32_t y,
                      std::uint32_t width, std::uint32_t height);

    void set_camera(const glm::mat4& view, const glm::mat4& projection);

    [[nodiscard]] std::uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] std::uint32_t height() const noexcept { return m_height; }
    [[nodiscard]] const glm::mat4& view_matrix() const noexcept { return m_view; }
    [[nodiscard]] const glm::mat4& projection_matrix() const noexcept { return m_projection; }
    [[nodiscard]] const glm::mat4& view_projection_matrix() const noexcept { return m_view_projection; }

private:
    std::uint32_t m_viewport_x = 0;
    std::uint32_t m_viewport_y = 0;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    glm::mat4 m_view = glm::mat4(1.0f);
    glm::mat4 m_projection = glm::mat4(1.0f);
    glm::mat4 m_view_projection = glm::mat4(1.0f);
};

// =============================================================================
// Compositor (for render graph)
// =============================================================================

/// Compositor for managing views and layers
class Compositor {
public:
    Compositor();
    ~Compositor();

    /// Initialize the compositor
    bool initialize(std::uint32_t width, std::uint32_t height);

    /// Shutdown the compositor
    void shutdown();

    /// Resize
    void resize(std::uint32_t width, std::uint32_t height);

    /// Create a view
    ResourceId create_view(const std::string& name);

    /// Destroy a view
    void destroy_view(ResourceId id);

    /// Get view by ID
    View* get_view(ResourceId id);

    /// Composite all layers
    void composite();

    /// Get layer manager
    LayerManager& layer_manager() noexcept { return m_layer_manager; }

private:
    std::unordered_map<ResourceId, View> m_views;
    LayerManager m_layer_manager;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
};

// =============================================================================
// RenderQueue (for render graph)
// =============================================================================

/// Queue type for render items
enum class QueueType : std::uint8_t {
    Opaque,
    Transparent,
    Overlay,
};

/// Render item for queue submission
struct RenderItem {
    std::uint64_t sort_key = 0;
    float depth = 0.0f;
    void* data = nullptr;
};

/// Render queue for sorting and batching draw calls
class RenderQueue {
public:
    /// Statistics
    struct Stats {
        std::size_t opaque_count = 0;
        std::size_t transparent_count = 0;
        std::size_t overlay_count = 0;
    };

    RenderQueue();

    /// Submit item to queue
    void submit(const RenderItem& item, QueueType queue);

    /// Sort all queues
    void sort();

    /// Clear all queues
    void clear();

    /// Execute with callback
    void execute(const std::function<void(const RenderItem&)>& render_fn);

    /// Get statistics
    [[nodiscard]] Stats stats() const;

private:
    std::vector<RenderItem> m_opaque;
    std::vector<RenderItem> m_transparent;
    std::vector<RenderItem> m_overlay;
};

// =============================================================================
// Builtin Passes Registration
// =============================================================================

namespace builtin_passes {
    /// Register all builtin pass factories
    void register_all();
}

} // namespace void_render
