/// @file render_graph.cpp
/// @brief Render graph implementation for declarative render pipeline

#include <void_engine/render/pass.hpp>
#include <void_engine/render/compositor.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace void_render {

// =============================================================================
// PassDescriptor Implementation
// =============================================================================

PassDescriptor PassDescriptor::color_pass(std::string name, TextureFormat format,
                                           std::uint32_t width, std::uint32_t height) {
    PassDescriptor desc;
    desc.name = std::move(name);
    desc.color_formats = {format};
    desc.width = width;
    desc.height = height;
    return desc;
}

PassDescriptor PassDescriptor::depth_pass(std::string name, TextureFormat format,
                                           std::uint32_t width, std::uint32_t height) {
    PassDescriptor desc;
    desc.name = std::move(name);
    desc.depth_format = format;
    desc.width = width;
    desc.height = height;
    return desc;
}

PassDescriptor PassDescriptor::shadow_pass(std::string name, std::uint32_t resolution) {
    PassDescriptor desc;
    desc.name = std::move(name);
    desc.depth_format = TextureFormat::Depth32Float;
    desc.width = resolution;
    desc.height = resolution;
    return desc;
}

// =============================================================================
// RenderPass Implementation
// =============================================================================

RenderPass::RenderPass(const PassDescriptor& descriptor)
    : m_descriptor(descriptor)
    , m_id(ResourceId::from_name(descriptor.name))
{
}

void RenderPass::add_dependency(ResourceId pass_id) {
    m_dependencies.push_back(pass_id);
}

void RenderPass::declare_input(ResourceId resource, ResourceState required_state) {
    m_inputs.push_back({resource, required_state});
}

void RenderPass::declare_output(ResourceId resource, ResourceState output_state) {
    m_outputs.push_back({resource, output_state});
}

// =============================================================================
// CallbackPass Implementation
// =============================================================================

CallbackPass::CallbackPass(const PassDescriptor& descriptor, RenderCallback callback)
    : RenderPass(descriptor)
    , m_callback(std::move(callback))
{
}

void CallbackPass::execute(const RenderContext& ctx) {
    if (m_callback) {
        m_callback(ctx);
    }
}

// =============================================================================
// PassRegistry Implementation
// =============================================================================

PassRegistry& PassRegistry::instance() {
    static PassRegistry registry;
    return registry;
}

void PassRegistry::register_pass(const std::string& name, PassFactory factory) {
    m_factories[name] = std::move(factory);
    spdlog::debug("Registered render pass: {}", name);
}

std::unique_ptr<RenderPass> PassRegistry::create(const std::string& name,
                                                  const PassDescriptor& desc) {
    auto it = m_factories.find(name);
    if (it != m_factories.end()) {
        return it->second(desc);
    }
    spdlog::warn("Unknown pass type: {}", name);
    return nullptr;
}

bool PassRegistry::has(const std::string& name) const {
    return m_factories.contains(name);
}

std::vector<std::string> PassRegistry::registered_passes() const {
    std::vector<std::string> names;
    names.reserve(m_factories.size());
    for (const auto& [name, _] : m_factories) {
        names.push_back(name);
    }
    return names;
}

// =============================================================================
// RenderGraph Implementation
// =============================================================================

RenderGraph::RenderGraph() = default;

RenderGraph::~RenderGraph() = default;

ResourceId RenderGraph::add_pass(std::unique_ptr<RenderPass> pass) {
    if (!pass) return ResourceId::invalid();

    ResourceId id = pass->id();
    m_passes[id] = std::move(pass);
    m_execution_order.clear();  // Invalidate order

    return id;
}

ResourceId RenderGraph::add_callback_pass(const std::string& name,
                                           RenderPass::RenderCallback callback) {
    PassDescriptor desc;
    desc.name = name;
    auto pass = std::make_unique<CallbackPass>(desc, std::move(callback));
    return add_pass(std::move(pass));
}

void RenderGraph::add_dependency(ResourceId from, ResourceId to) {
    if (m_passes.contains(from) && m_passes.contains(to)) {
        m_passes[to]->add_dependency(from);
        m_execution_order.clear();  // Invalidate order
    }
}

void RenderGraph::remove_pass(ResourceId id) {
    m_passes.erase(id);
    m_execution_order.clear();
}

RenderPass* RenderGraph::get_pass(ResourceId id) {
    auto it = m_passes.find(id);
    return it != m_passes.end() ? it->second.get() : nullptr;
}

bool RenderGraph::compile() {
    if (m_passes.empty()) {
        spdlog::warn("Render graph is empty");
        return true;
    }

    // Topological sort using Kahn's algorithm
    std::unordered_map<ResourceId, std::size_t> in_degree;
    std::unordered_map<ResourceId, std::vector<ResourceId>> adj;

    // Initialize
    for (const auto& [id, pass] : m_passes) {
        in_degree[id] = 0;
    }

    // Build adjacency list and count in-degrees
    for (const auto& [id, pass] : m_passes) {
        for (const auto& dep_id : pass->dependencies()) {
            adj[dep_id].push_back(id);
            in_degree[id]++;
        }
    }

    // Find all nodes with no incoming edges
    std::queue<ResourceId> queue;
    for (const auto& [id, degree] : in_degree) {
        if (degree == 0) {
            queue.push(id);
        }
    }

    m_execution_order.clear();

    while (!queue.empty()) {
        ResourceId current = queue.front();
        queue.pop();
        m_execution_order.push_back(current);

        for (const auto& neighbor : adj[current]) {
            if (--in_degree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }

    // Check for cycles
    if (m_execution_order.size() != m_passes.size()) {
        spdlog::error("Render graph has cycles!");
        m_execution_order.clear();
        return false;
    }

    spdlog::debug("Render graph compiled: {} passes", m_execution_order.size());
    return true;
}

void RenderGraph::execute(const RenderContext& ctx) {
    if (m_execution_order.empty() && !m_passes.empty()) {
        if (!compile()) {
            spdlog::error("Failed to compile render graph");
            return;
        }
    }

    for (const auto& pass_id : m_execution_order) {
        auto it = m_passes.find(pass_id);
        if (it != m_passes.end() && it->second->is_enabled()) {
            it->second->execute(ctx);
        }
    }
}

void RenderGraph::clear() {
    m_passes.clear();
    m_execution_order.clear();
}

// =============================================================================
// LayerId Implementation
// =============================================================================

LayerId LayerId::main() {
    static LayerId id(0);
    return id;
}

LayerId LayerId::ui() {
    static LayerId id(1000);
    return id;
}

LayerId LayerId::debug() {
    static LayerId id(2000);
    return id;
}

// =============================================================================
// RenderLayer Implementation
// =============================================================================

RenderLayer::RenderLayer(LayerId id, std::string name)
    : m_id(id)
    , m_name(std::move(name))
{
}

void RenderLayer::add_pass(ResourceId pass_id) {
    m_passes.push_back(pass_id);
}

void RenderLayer::remove_pass(ResourceId pass_id) {
    m_passes.erase(std::remove(m_passes.begin(), m_passes.end(), pass_id),
                   m_passes.end());
}

void RenderLayer::clear_passes() {
    m_passes.clear();
}

// =============================================================================
// LayerManager Implementation
// =============================================================================

LayerManager::LayerManager() = default;

LayerManager::~LayerManager() = default;

LayerId LayerManager::create_layer(std::string name, std::int32_t priority) {
    LayerId id(m_next_id++);
    m_layers[id] = RenderLayer(id, std::move(name));
    m_layers[id].set_priority(priority);
    m_sorted_layers.clear();  // Invalidate sorted order
    return id;
}

void LayerManager::destroy_layer(LayerId id) {
    m_layers.erase(id);
    m_sorted_layers.clear();
}

RenderLayer* LayerManager::get_layer(LayerId id) {
    auto it = m_layers.find(id);
    return it != m_layers.end() ? &it->second : nullptr;
}

void LayerManager::sort_layers() {
    m_sorted_layers.clear();
    m_sorted_layers.reserve(m_layers.size());

    for (auto& [id, layer] : m_layers) {
        if (layer.is_visible()) {
            m_sorted_layers.push_back(&layer);
        }
    }

    std::sort(m_sorted_layers.begin(), m_sorted_layers.end(),
              [](const RenderLayer* a, const RenderLayer* b) {
                  return a->priority() < b->priority();
              });
}

const std::vector<RenderLayer*>& LayerManager::sorted_layers() {
    if (m_sorted_layers.empty() && !m_layers.empty()) {
        sort_layers();
    }
    return m_sorted_layers;
}

// =============================================================================
// View Implementation
// =============================================================================

View::View(std::uint32_t width, std::uint32_t height)
    : m_width(width)
    , m_height(height)
{
}

void View::set_viewport(std::uint32_t x, std::uint32_t y,
                        std::uint32_t width, std::uint32_t height) {
    m_viewport_x = x;
    m_viewport_y = y;
    m_width = width;
    m_height = height;
}

void View::set_camera(const glm::mat4& view, const glm::mat4& projection) {
    m_view = view;
    m_projection = projection;
    m_view_projection = projection * view;
}

// =============================================================================
// Compositor Implementation
// =============================================================================

Compositor::Compositor() = default;

Compositor::~Compositor() = default;

bool Compositor::initialize(std::uint32_t width, std::uint32_t height) {
    m_width = width;
    m_height = height;

    // Create default layers
    m_layer_manager.create_layer("background", -1000);
    m_layer_manager.create_layer("main", 0);
    m_layer_manager.create_layer("transparent", 100);
    m_layer_manager.create_layer("ui", 1000);
    m_layer_manager.create_layer("debug", 2000);

    spdlog::info("Compositor initialized: {}x{}", width, height);
    return true;
}

void Compositor::shutdown() {
    m_views.clear();
}

void Compositor::resize(std::uint32_t width, std::uint32_t height) {
    m_width = width;
    m_height = height;

    for (auto& [id, view] : m_views) {
        view.set_viewport(0, 0, width, height);
    }
}

ResourceId Compositor::create_view(const std::string& name) {
    ResourceId id = ResourceId::from_name(name);
    m_views[id] = View(m_width, m_height);
    return id;
}

void Compositor::destroy_view(ResourceId id) {
    m_views.erase(id);
}

View* Compositor::get_view(ResourceId id) {
    auto it = m_views.find(id);
    return it != m_views.end() ? &it->second : nullptr;
}

void Compositor::composite() {
    // Get sorted layers
    auto& layers = m_layer_manager.sorted_layers();

    // Composite each layer in order
    for (const auto* layer : layers) {
        if (!layer->is_visible()) continue;

        // Apply blend mode
        // In a full implementation, this would set up framebuffer blending

        // Execute layer passes
        // In a full implementation, this would render the layer
    }
}

// =============================================================================
// RenderQueue Implementation
// =============================================================================

RenderQueue::RenderQueue() {
    // Pre-allocate some space
    m_opaque.reserve(1024);
    m_transparent.reserve(256);
    m_overlay.reserve(64);
}

void RenderQueue::submit(const RenderItem& item, QueueType queue) {
    switch (queue) {
        case QueueType::Opaque:
            m_opaque.push_back(item);
            break;
        case QueueType::Transparent:
            m_transparent.push_back(item);
            break;
        case QueueType::Overlay:
            m_overlay.push_back(item);
            break;
    }
}

void RenderQueue::sort() {
    // Sort opaque front-to-back (minimize overdraw)
    std::sort(m_opaque.begin(), m_opaque.end(),
              [](const RenderItem& a, const RenderItem& b) {
                  if (a.sort_key != b.sort_key) return a.sort_key < b.sort_key;
                  return a.depth < b.depth;  // Front to back
              });

    // Sort transparent back-to-front (correct blending)
    std::sort(m_transparent.begin(), m_transparent.end(),
              [](const RenderItem& a, const RenderItem& b) {
                  if (a.sort_key != b.sort_key) return a.sort_key < b.sort_key;
                  return a.depth > b.depth;  // Back to front
              });

    // Sort overlay by sort key only
    std::sort(m_overlay.begin(), m_overlay.end(),
              [](const RenderItem& a, const RenderItem& b) {
                  return a.sort_key < b.sort_key;
              });
}

void RenderQueue::clear() {
    m_opaque.clear();
    m_transparent.clear();
    m_overlay.clear();
}

void RenderQueue::execute(const std::function<void(const RenderItem&)>& render_fn) {
    // Render opaque items
    for (const auto& item : m_opaque) {
        render_fn(item);
    }

    // Render transparent items
    for (const auto& item : m_transparent) {
        render_fn(item);
    }

    // Render overlay items
    for (const auto& item : m_overlay) {
        render_fn(item);
    }
}

RenderQueue::Stats RenderQueue::stats() const {
    Stats s;
    s.opaque_count = m_opaque.size();
    s.transparent_count = m_transparent.size();
    s.overlay_count = m_overlay.size();
    return s;
}

// =============================================================================
// Builtin Passes
// =============================================================================

namespace builtin_passes {

void register_all() {
    auto& registry = PassRegistry::instance();

    // Depth prepass
    registry.register_pass("depth_prepass", [](const PassDescriptor& desc) {
        return std::make_unique<CallbackPass>(desc, [](const RenderContext&) {
            // Render depth only
        });
    });

    // Shadow pass
    registry.register_pass("shadow", [](const PassDescriptor& desc) {
        return std::make_unique<CallbackPass>(desc, [](const RenderContext&) {
            // Render shadow maps
        });
    });

    // GBuffer pass
    registry.register_pass("gbuffer", [](const PassDescriptor& desc) {
        return std::make_unique<CallbackPass>(desc, [](const RenderContext&) {
            // Render to GBuffer
        });
    });

    // Lighting pass
    registry.register_pass("lighting", [](const PassDescriptor& desc) {
        return std::make_unique<CallbackPass>(desc, [](const RenderContext&) {
            // Apply lighting
        });
    });

    // SSAO pass
    registry.register_pass("ssao", [](const PassDescriptor& desc) {
        return std::make_unique<CallbackPass>(desc, [](const RenderContext&) {
            // Screen-space ambient occlusion
        });
    });

    // Bloom pass
    registry.register_pass("bloom", [](const PassDescriptor& desc) {
        return std::make_unique<CallbackPass>(desc, [](const RenderContext&) {
            // Bloom effect
        });
    });

    // Tonemapping pass
    registry.register_pass("tonemap", [](const PassDescriptor& desc) {
        return std::make_unique<CallbackPass>(desc, [](const RenderContext&) {
            // HDR tonemapping
        });
    });

    // FXAA pass
    registry.register_pass("fxaa", [](const PassDescriptor& desc) {
        return std::make_unique<CallbackPass>(desc, [](const RenderContext&) {
            // FXAA anti-aliasing
        });
    });

    spdlog::debug("Registered {} builtin passes", registry.registered_passes().size());
}

} // namespace builtin_passes

} // namespace void_render
