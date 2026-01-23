/// @file layer.cpp
/// @brief Layer management implementation
/// @details Matches legacy Rust void_kernel/src/layer.rs

#include "layer.hpp"

#include <algorithm>

namespace void_runtime {

// =============================================================================
// Layer Implementation
// =============================================================================

Layer::Layer(const std::string& name, NamespaceId owner, const LayerConfig& config)
    : id_()
    , name_(name)
    , owner_(owner)
    , config_(config)
    , dirty_(true)
    , last_rendered_frame_(0) {
}

void Layer::set_visible(bool visible) {
    if (config_.visible != visible) {
        config_.visible = visible;
        dirty_ = true;
    }
}

void Layer::set_priority(std::int32_t priority) {
    if (config_.priority != priority) {
        config_.priority = priority;
        dirty_ = true;
    }
}

void Layer::set_blend_mode(BlendMode mode) {
    if (config_.blend_mode != mode) {
        config_.blend_mode = mode;
        dirty_ = true;
    }
}

void Layer::set_render_scale(float scale) {
    if (config_.render_scale != scale) {
        config_.render_scale = std::max(0.1f, std::min(2.0f, scale));
        dirty_ = true;
    }
}

void Layer::set_opacity(float opacity) {
    if (config_.opacity != opacity) {
        config_.opacity = std::max(0.0f, std::min(1.0f, opacity));
        dirty_ = true;
    }
}

void Layer::set_clear_color(const std::array<float, 4>& color) {
    config_.clear_color = color;
    dirty_ = true;
}

void Layer::clear_clear_color() {
    config_.clear_color = std::nullopt;
    dirty_ = true;
}

void Layer::add_entity(std::uint64_t entity_id) {
    std::lock_guard<std::mutex> lock(entities_mutex_);
    auto it = std::find(entities_.begin(), entities_.end(), entity_id);
    if (it == entities_.end()) {
        entities_.push_back(entity_id);
        dirty_ = true;
    }
}

void Layer::remove_entity(std::uint64_t entity_id) {
    std::lock_guard<std::mutex> lock(entities_mutex_);
    auto it = std::find(entities_.begin(), entities_.end(), entity_id);
    if (it != entities_.end()) {
        entities_.erase(it);
        dirty_ = true;
    }
}

bool Layer::has_entity(std::uint64_t entity_id) const {
    std::lock_guard<std::mutex> lock(entities_mutex_);
    return std::find(entities_.begin(), entities_.end(), entity_id) != entities_.end();
}

void Layer::clear_entities() {
    std::lock_guard<std::mutex> lock(entities_mutex_);
    entities_.clear();
    dirty_ = true;
}

// =============================================================================
// LayerManager Implementation
// =============================================================================

LayerManager::LayerManager(std::size_t max_layers)
    : max_layers_(max_layers) {
}

LayerManager::~LayerManager() {
    destroy_all_layers();
}

LayerManager& LayerManager::instance() {
    static LayerManager instance;
    return instance;
}

Layer* LayerManager::create_layer(const std::string& name, NamespaceId owner,
                                   const LayerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check max layers
    if (layers_.size() >= max_layers_) {
        return nullptr;
    }

    // Check for duplicate name
    if (name_to_id_.count(name) > 0) {
        return nullptr;
    }

    // Create layer
    auto layer = std::make_unique<Layer>(name, owner, config);
    LayerId id = layer->id();
    Layer* ptr = layer.get();

    // Store
    layers_[id] = std::move(layer);
    name_to_id_[name] = id;
    namespace_layers_[owner].push_back(id);

    sort_dirty_ = true;

    // Callback
    if (on_created_) {
        on_created_(*ptr);
    }

    return ptr;
}

Layer* LayerManager::create_layer(const std::string& name, const LayerConfig& config) {
    return create_layer(name, NamespaceId::global(), config);
}

bool LayerManager::destroy_layer(LayerId id) {
    std::unique_ptr<Layer> layer;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = layers_.find(id);
        if (it == layers_.end()) {
            return false;
        }

        layer = std::move(it->second);
        layers_.erase(it);

        // Remove from name map
        name_to_id_.erase(layer->name());

        // Remove from namespace map
        auto& ns_layers = namespace_layers_[layer->owner()];
        ns_layers.erase(std::remove(ns_layers.begin(), ns_layers.end(), id), ns_layers.end());

        // Remove from sorted list
        sorted_layers_.erase(std::remove(sorted_layers_.begin(), sorted_layers_.end(), id),
                            sorted_layers_.end());
    }

    // Callback (outside lock)
    if (on_destroyed_ && layer) {
        on_destroyed_(*layer);
    }

    // Remove entity mappings
    {
        std::lock_guard<std::mutex> lock(entity_mutex_);
        for (auto it = entity_to_layer_.begin(); it != entity_to_layer_.end();) {
            if (it->second == id) {
                it = entity_to_layer_.erase(it);
            } else {
                ++it;
            }
        }
    }

    return true;
}

bool LayerManager::destroy_layer(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = name_to_id_.find(name);
    if (it == name_to_id_.end()) {
        return false;
    }

    LayerId id = it->second;
    lock.~lock_guard();  // Release lock before recursive call
    return destroy_layer(id);
}

void LayerManager::destroy_namespace_layers(NamespaceId namespace_id) {
    std::vector<LayerId> to_destroy;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = namespace_layers_.find(namespace_id);
        if (it != namespace_layers_.end()) {
            to_destroy = it->second;
        }
    }

    for (LayerId id : to_destroy) {
        destroy_layer(id);
    }
}

void LayerManager::destroy_all_layers() {
    std::vector<LayerId> to_destroy;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, layer] : layers_) {
            to_destroy.push_back(id);
        }
    }

    for (LayerId id : to_destroy) {
        destroy_layer(id);
    }
}

Layer* LayerManager::get_layer(LayerId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = layers_.find(id);
    return it != layers_.end() ? it->second.get() : nullptr;
}

const Layer* LayerManager::get_layer(LayerId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = layers_.find(id);
    return it != layers_.end() ? it->second.get() : nullptr;
}

Layer* LayerManager::get_layer(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = name_to_id_.find(name);
    if (it == name_to_id_.end()) {
        return nullptr;
    }
    auto layer_it = layers_.find(it->second);
    return layer_it != layers_.end() ? layer_it->second.get() : nullptr;
}

const Layer* LayerManager::get_layer(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = name_to_id_.find(name);
    if (it == name_to_id_.end()) {
        return nullptr;
    }
    auto layer_it = layers_.find(it->second);
    return layer_it != layers_.end() ? layer_it->second.get() : nullptr;
}

Layer* LayerManager::get_or_create_layer(const std::string& name, const LayerConfig& config) {
    Layer* layer = get_layer(name);
    if (layer) {
        return layer;
    }
    return create_layer(name, config);
}

bool LayerManager::has_layer(LayerId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return layers_.count(id) > 0;
}

bool LayerManager::has_layer(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return name_to_id_.count(name) > 0;
}

std::vector<Layer*> LayerManager::all_layers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Layer*> result;
    result.reserve(layers_.size());
    for (auto& [id, layer] : layers_) {
        result.push_back(layer.get());
    }
    return result;
}

std::vector<const Layer*> LayerManager::all_layers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const Layer*> result;
    result.reserve(layers_.size());
    for (const auto& [id, layer] : layers_) {
        result.push_back(layer.get());
    }
    return result;
}

std::vector<Layer*> LayerManager::visible_layers() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sort_dirty_) {
        sort_layers();
    }

    std::vector<Layer*> result;
    for (LayerId id : sorted_layers_) {
        auto it = layers_.find(id);
        if (it != layers_.end() && it->second->visible()) {
            result.push_back(it->second.get());
        }
    }
    return result;
}

std::vector<Layer*> LayerManager::dirty_layers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Layer*> result;
    for (auto& [id, layer] : layers_) {
        if (layer->dirty()) {
            result.push_back(layer.get());
        }
    }
    return result;
}

std::vector<Layer*> LayerManager::layers_by_type(LayerType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Layer*> result;
    for (auto& [id, layer] : layers_) {
        if (layer->type() == type) {
            result.push_back(layer.get());
        }
    }
    return result;
}

std::vector<Layer*> LayerManager::layers_by_namespace(NamespaceId namespace_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Layer*> result;

    auto it = namespace_layers_.find(namespace_id);
    if (it != namespace_layers_.end()) {
        for (LayerId id : it->second) {
            auto layer_it = layers_.find(id);
            if (layer_it != layers_.end()) {
                result.push_back(layer_it->second.get());
            }
        }
    }
    return result;
}

std::size_t LayerManager::layer_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return layers_.size();
}

void LayerManager::mark_all_dirty() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, layer] : layers_) {
        if (layer->visible()) {
            layer->mark_dirty();
        }
    }
}

void LayerManager::clear_all_dirty() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, layer] : layers_) {
        layer->clear_dirty();
    }
}

void LayerManager::mark_rendered(LayerId id, std::uint64_t frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = layers_.find(id);
    if (it != layers_.end()) {
        it->second->set_last_rendered_frame(frame);
        it->second->clear_dirty();
    }
}

std::vector<Layer*> LayerManager::collect_for_render(std::uint64_t current_frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sort_dirty_) {
        sort_layers();
    }

    std::vector<Layer*> result;
    for (LayerId id : sorted_layers_) {
        auto it = layers_.find(id);
        if (it != layers_.end()) {
            Layer* layer = it->second.get();
            if (layer->visible() && (layer->dirty() || layer->last_rendered_frame() < current_frame)) {
                result.push_back(layer);
            }
        }
    }
    return result;
}

void LayerManager::assign_entity_to_layer(std::uint64_t entity_id, const std::string& layer_name) {
    Layer* layer = get_layer(layer_name);
    if (!layer) {
        // Create layer if it doesn't exist
        layer = create_layer(layer_name, LayerConfig::content());
    }

    if (layer) {
        // Remove from old layer
        {
            std::lock_guard<std::mutex> lock(entity_mutex_);
            auto it = entity_to_layer_.find(entity_id);
            if (it != entity_to_layer_.end()) {
                Layer* old_layer = get_layer(it->second);
                if (old_layer) {
                    old_layer->remove_entity(entity_id);
                }
            }
            entity_to_layer_[entity_id] = layer->id();
        }

        // Add to new layer
        layer->add_entity(entity_id);
    }
}

void LayerManager::remove_entity_from_all_layers(std::uint64_t entity_id) {
    std::lock_guard<std::mutex> lock(entity_mutex_);

    auto it = entity_to_layer_.find(entity_id);
    if (it != entity_to_layer_.end()) {
        Layer* layer = get_layer(it->second);
        if (layer) {
            layer->remove_entity(entity_id);
        }
        entity_to_layer_.erase(it);
    }
}

Layer* LayerManager::get_entity_layer(std::uint64_t entity_id) {
    std::lock_guard<std::mutex> lock(entity_mutex_);

    auto it = entity_to_layer_.find(entity_id);
    if (it != entity_to_layer_.end()) {
        return get_layer(it->second);
    }
    return nullptr;
}

void LayerManager::create_default_layers() {
    // Shadow layer - rendered first for shadow maps
    create_layer("shadow", LayerConfig::shadow(-100));

    // Background layer - skybox, distant objects
    create_layer("background", LayerConfig::content(-50));

    // World layer - main content
    create_layer("world", LayerConfig::content(0));

    // Transparent layer - glass, particles
    auto transparent_config = LayerConfig::content(10);
    transparent_config.clear_mode = ClearMode::None;
    create_layer("transparent", transparent_config);

    // Effects layer - post-processing
    create_layer("effects", LayerConfig::effect(50));

    // UI layer - 2D overlay
    create_layer("ui", LayerConfig::overlay(100));

    // Debug layer - debug visualization
    create_layer("debug", LayerConfig::debug(200));
}

void LayerManager::sort_layers() {
    sorted_layers_.clear();
    sorted_layers_.reserve(layers_.size());

    for (const auto& [id, layer] : layers_) {
        sorted_layers_.push_back(id);
    }

    std::sort(sorted_layers_.begin(), sorted_layers_.end(),
              [this](LayerId a, LayerId b) {
                  auto it_a = layers_.find(a);
                  auto it_b = layers_.find(b);
                  if (it_a == layers_.end() || it_b == layers_.end()) {
                      return false;
                  }
                  return it_a->second->priority() < it_b->second->priority();
              });

    sort_dirty_ = false;
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* layer_type_to_string(LayerType type) {
    switch (type) {
        case LayerType::Shadow: return "shadow";
        case LayerType::Content: return "content";
        case LayerType::Overlay: return "overlay";
        case LayerType::Effect: return "effect";
        case LayerType::Portal: return "portal";
        case LayerType::Debug: return "debug";
    }
    return "unknown";
}

LayerType string_to_layer_type(const std::string& str) {
    if (str == "shadow") return LayerType::Shadow;
    if (str == "overlay") return LayerType::Overlay;
    if (str == "effect") return LayerType::Effect;
    if (str == "portal") return LayerType::Portal;
    if (str == "debug") return LayerType::Debug;
    return LayerType::Content;
}

const char* blend_mode_to_string(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal: return "normal";
        case BlendMode::Additive: return "additive";
        case BlendMode::Multiply: return "multiply";
        case BlendMode::Replace: return "replace";
        case BlendMode::Screen: return "screen";
        case BlendMode::Overlay: return "overlay";
        case BlendMode::SoftLight: return "soft_light";
    }
    return "normal";
}

BlendMode string_to_blend_mode(const std::string& str) {
    if (str == "additive") return BlendMode::Additive;
    if (str == "multiply") return BlendMode::Multiply;
    if (str == "replace") return BlendMode::Replace;
    if (str == "screen") return BlendMode::Screen;
    if (str == "overlay") return BlendMode::Overlay;
    if (str == "soft_light") return BlendMode::SoftLight;
    return BlendMode::Normal;
}

// =============================================================================
// LayerStack Implementation
// =============================================================================

void LayerStack::push(Layer* layer) {
    if (layer) {
        layers_.push_back(layer);
    }
}

Layer* LayerStack::pop() {
    if (layers_.empty()) {
        return nullptr;
    }
    Layer* layer = layers_.back();
    layers_.pop_back();
    return layer;
}

Layer* LayerStack::top() const {
    return layers_.empty() ? nullptr : layers_.back();
}

// =============================================================================
// LayerCompositor Implementation
// =============================================================================

void LayerCompositor::set_output_size(int width, int height) {
    output_width_ = std::max(1, width);
    output_height_ = std::max(1, height);
}

void LayerCompositor::composite(const std::vector<Layer*>& layers) {
    begin();
    for (Layer* layer : layers) {
        if (layer && layer->visible()) {
            composite_layer(layer);
        }
    }
    end();
}

void LayerCompositor::composite_layer(Layer* layer) {
    if (!layer || !layer->visible()) {
        return;
    }

    // Calculate effective opacity
    float opacity = layer->config().opacity * global_opacity_;
    if (opacity <= 0.0f) {
        return;
    }

    // In a real implementation, this would:
    // 1. Bind the layer's render target as texture
    // 2. Set up blend state based on layer's blend mode
    // 3. Draw a fullscreen quad with appropriate shader
    // 4. Apply opacity

    // Mark layer as composited
    layer->clear_dirty();
}

void LayerCompositor::begin() {
    // In a real implementation:
    // 1. Bind output framebuffer
    // 2. Clear if needed
    // 3. Set up composition shader
}

void LayerCompositor::end() {
    // In a real implementation:
    // 1. Unbind framebuffer
    // 2. Clean up state
}

} // namespace void_runtime
