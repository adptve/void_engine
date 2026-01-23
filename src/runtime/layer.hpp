/// @file layer.hpp
/// @brief Layer management for isolation and composition
/// @details Matches legacy Rust void_kernel/src/layer.rs

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_runtime {

// =============================================================================
// Layer Identifiers
// =============================================================================

/// @brief Unique identifier for a layer
class LayerId {
public:
    LayerId() : id_(next_id()) {}
    explicit LayerId(std::uint64_t id) : id_(id) {}

    std::uint64_t raw() const { return id_; }
    bool operator==(const LayerId& other) const { return id_ == other.id_; }
    bool operator!=(const LayerId& other) const { return id_ != other.id_; }
    bool operator<(const LayerId& other) const { return id_ < other.id_; }

    static LayerId invalid() { return LayerId(0); }
    bool is_valid() const { return id_ != 0; }

private:
    static std::uint64_t next_id() {
        static std::atomic<std::uint64_t> counter{1};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    std::uint64_t id_;
};

/// @brief Namespace identifier for isolation
class NamespaceId {
public:
    NamespaceId() : id_(0) {}
    explicit NamespaceId(std::uint64_t id) : id_(id) {}

    std::uint64_t raw() const { return id_; }
    bool operator==(const NamespaceId& other) const { return id_ == other.id_; }
    bool operator!=(const NamespaceId& other) const { return id_ != other.id_; }

    static NamespaceId global() { return NamespaceId(0); }
    static NamespaceId create() {
        static std::atomic<std::uint64_t> counter{1};
        return NamespaceId(counter.fetch_add(1, std::memory_order_relaxed));
    }

private:
    std::uint64_t id_;
};

} // namespace void_runtime

// Hash specializations for use in containers
namespace std {
template<>
struct hash<void_runtime::LayerId> {
    std::size_t operator()(const void_runtime::LayerId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.raw());
    }
};

template<>
struct hash<void_runtime::NamespaceId> {
    std::size_t operator()(const void_runtime::NamespaceId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.raw());
    }
};
} // namespace std

namespace void_runtime {

// =============================================================================
// Layer Types
// =============================================================================

/// @brief Type of rendering layer
enum class LayerType {
    Shadow,     // Shadow map generation
    Content,    // Main 3D content rendering
    Overlay,    // UI/HUD elements (2D overlay)
    Effect,     // Post-processing effects
    Portal,     // Render-to-texture (mirrors, portals)
    Debug       // Debug visualization
};

/// @brief Blend mode for layer composition
enum class BlendMode {
    Normal,     // Standard alpha blending
    Additive,   // Add to underlying layers
    Multiply,   // Multiply with underlying layers
    Replace,    // Replace underlying completely
    Screen,     // Screen blend mode
    Overlay,    // Overlay blend mode
    SoftLight   // Soft light blend
};

/// @brief Layer clear mode
enum class ClearMode {
    None,       // Don't clear
    Color,      // Clear to color
    Depth,      // Clear depth only
    Both        // Clear color and depth
};

// =============================================================================
// Layer Configuration
// =============================================================================

/// @brief Configuration for a layer
struct LayerConfig {
    /// Layer type
    LayerType type = LayerType::Content;

    /// Render priority (lower = rendered first)
    std::int32_t priority = 0;

    /// Blend mode for composition
    BlendMode blend_mode = BlendMode::Normal;

    /// Whether the layer is visible
    bool visible = true;

    /// Clear mode
    ClearMode clear_mode = ClearMode::Both;

    /// Optional clear color (RGBA)
    std::optional<std::array<float, 4>> clear_color;

    /// Whether to use depth buffer
    bool use_depth = true;

    /// Render scale (1.0 = full resolution)
    float render_scale = 1.0f;

    /// Opacity for blending (0.0 - 1.0)
    float opacity = 1.0f;

    /// Enable MSAA for this layer
    bool msaa = false;

    /// MSAA sample count (2, 4, 8)
    int msaa_samples = 4;

    /// Post-process effects enabled
    bool post_process = true;

    // Factory methods for common configurations
    static LayerConfig content(std::int32_t priority = 0) {
        LayerConfig config;
        config.type = LayerType::Content;
        config.priority = priority;
        config.use_depth = true;
        return config;
    }

    static LayerConfig shadow(std::int32_t priority = -100) {
        LayerConfig config;
        config.type = LayerType::Shadow;
        config.priority = priority;
        config.use_depth = true;
        config.clear_mode = ClearMode::Depth;
        config.post_process = false;
        return config;
    }

    static LayerConfig overlay(std::int32_t priority = 100) {
        LayerConfig config;
        config.type = LayerType::Overlay;
        config.priority = priority;
        config.use_depth = false;
        config.clear_mode = ClearMode::None;
        return config;
    }

    static LayerConfig effect(std::int32_t priority = 50) {
        LayerConfig config;
        config.type = LayerType::Effect;
        config.priority = priority;
        config.use_depth = false;
        config.clear_mode = ClearMode::None;
        return config;
    }

    static LayerConfig portal(std::int32_t priority = -50) {
        LayerConfig config;
        config.type = LayerType::Portal;
        config.priority = priority;
        config.use_depth = true;
        return config;
    }

    static LayerConfig debug(std::int32_t priority = 200) {
        LayerConfig config;
        config.type = LayerType::Debug;
        config.priority = priority;
        config.use_depth = false;
        config.clear_mode = ClearMode::None;
        return config;
    }
};

// =============================================================================
// Layer
// =============================================================================

/// @brief A render layer
class Layer {
public:
    Layer(const std::string& name, NamespaceId owner, const LayerConfig& config);
    ~Layer() = default;

    // Non-copyable, movable
    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;
    Layer(Layer&&) noexcept = default;
    Layer& operator=(Layer&&) noexcept = default;

    // ==========================================================================
    // Properties
    // ==========================================================================

    /// @brief Get layer ID
    LayerId id() const { return id_; }

    /// @brief Get layer name
    const std::string& name() const { return name_; }

    /// @brief Get owning namespace
    NamespaceId owner() const { return owner_; }

    /// @brief Get layer type
    LayerType type() const { return config_.type; }

    /// @brief Get render priority
    std::int32_t priority() const { return config_.priority; }

    /// @brief Get configuration
    const LayerConfig& config() const { return config_; }

    /// @brief Get mutable configuration
    LayerConfig& config() { return config_; }

    // ==========================================================================
    // State
    // ==========================================================================

    /// @brief Check if layer is visible
    bool visible() const { return config_.visible; }

    /// @brief Set visibility
    void set_visible(bool visible);

    /// @brief Check if layer is dirty (needs re-render)
    bool dirty() const { return dirty_; }

    /// @brief Mark layer as dirty
    void mark_dirty() { dirty_ = true; }

    /// @brief Clear dirty flag
    void clear_dirty() { dirty_ = false; }

    /// @brief Get last rendered frame number
    std::uint64_t last_rendered_frame() const { return last_rendered_frame_; }

    /// @brief Set last rendered frame
    void set_last_rendered_frame(std::uint64_t frame) { last_rendered_frame_ = frame; }

    // ==========================================================================
    // Configuration Updates
    // ==========================================================================

    /// @brief Set priority
    void set_priority(std::int32_t priority);

    /// @brief Set blend mode
    void set_blend_mode(BlendMode mode);

    /// @brief Set render scale
    void set_render_scale(float scale);

    /// @brief Set opacity
    void set_opacity(float opacity);

    /// @brief Set clear color
    void set_clear_color(const std::array<float, 4>& color);

    /// @brief Clear the clear color (no clearing)
    void clear_clear_color();

    // ==========================================================================
    // Entity Management
    // ==========================================================================

    /// @brief Add entity to this layer
    void add_entity(std::uint64_t entity_id);

    /// @brief Remove entity from this layer
    void remove_entity(std::uint64_t entity_id);

    /// @brief Check if entity is in this layer
    bool has_entity(std::uint64_t entity_id) const;

    /// @brief Get all entities in this layer
    const std::vector<std::uint64_t>& entities() const { return entities_; }

    /// @brief Get entity count
    std::size_t entity_count() const { return entities_.size(); }

    /// @brief Clear all entities
    void clear_entities();

private:
    LayerId id_;
    std::string name_;
    NamespaceId owner_;
    LayerConfig config_;

    bool dirty_ = true;
    std::uint64_t last_rendered_frame_ = 0;

    std::vector<std::uint64_t> entities_;
    mutable std::mutex entities_mutex_;
};

// =============================================================================
// Layer Manager
// =============================================================================

/// @brief Manages all layers
class LayerManager {
public:
    explicit LayerManager(std::size_t max_layers = 64);
    ~LayerManager();

    // Non-copyable
    LayerManager(const LayerManager&) = delete;
    LayerManager& operator=(const LayerManager&) = delete;

    // ==========================================================================
    // Singleton
    // ==========================================================================

    /// @brief Get global instance
    static LayerManager& instance();

    // ==========================================================================
    // Layer Creation/Destruction
    // ==========================================================================

    /// @brief Create a new layer
    Layer* create_layer(const std::string& name, NamespaceId owner,
                        const LayerConfig& config = LayerConfig::content());

    /// @brief Create layer with default namespace
    Layer* create_layer(const std::string& name, const LayerConfig& config = LayerConfig::content());

    /// @brief Destroy a layer
    bool destroy_layer(LayerId id);

    /// @brief Destroy a layer by name
    bool destroy_layer(const std::string& name);

    /// @brief Destroy all layers owned by a namespace
    void destroy_namespace_layers(NamespaceId namespace_id);

    /// @brief Destroy all layers
    void destroy_all_layers();

    // ==========================================================================
    // Layer Access
    // ==========================================================================

    /// @brief Get layer by ID
    Layer* get_layer(LayerId id);
    const Layer* get_layer(LayerId id) const;

    /// @brief Get layer by name
    Layer* get_layer(const std::string& name);
    const Layer* get_layer(const std::string& name) const;

    /// @brief Get or create layer by name
    Layer* get_or_create_layer(const std::string& name,
                               const LayerConfig& config = LayerConfig::content());

    /// @brief Check if layer exists
    bool has_layer(LayerId id) const;
    bool has_layer(const std::string& name) const;

    // ==========================================================================
    // Layer Queries
    // ==========================================================================

    /// @brief Get all layers
    std::vector<Layer*> all_layers();
    std::vector<const Layer*> all_layers() const;

    /// @brief Get visible layers sorted by priority
    std::vector<Layer*> visible_layers();

    /// @brief Get dirty layers (need re-render)
    std::vector<Layer*> dirty_layers();

    /// @brief Get layers by type
    std::vector<Layer*> layers_by_type(LayerType type);

    /// @brief Get layers by namespace
    std::vector<Layer*> layers_by_namespace(NamespaceId namespace_id);

    /// @brief Get layer count
    std::size_t layer_count() const;

    /// @brief Get max layers
    std::size_t max_layers() const { return max_layers_; }

    // ==========================================================================
    // Rendering Support
    // ==========================================================================

    /// @brief Mark all visible layers as dirty
    void mark_all_dirty();

    /// @brief Clear dirty flag on all layers
    void clear_all_dirty();

    /// @brief Mark layer as rendered
    void mark_rendered(LayerId id, std::uint64_t frame);

    /// @brief Get layers needing render this frame
    std::vector<Layer*> collect_for_render(std::uint64_t current_frame);

    // ==========================================================================
    // Entity Management
    // ==========================================================================

    /// @brief Assign entity to layer
    void assign_entity_to_layer(std::uint64_t entity_id, const std::string& layer_name);

    /// @brief Remove entity from all layers
    void remove_entity_from_all_layers(std::uint64_t entity_id);

    /// @brief Get layer containing entity
    Layer* get_entity_layer(std::uint64_t entity_id);

    // ==========================================================================
    // Events/Callbacks
    // ==========================================================================

    using LayerCallback = std::function<void(Layer&)>;

    /// @brief Set callback for layer creation
    void on_layer_created(LayerCallback callback) { on_created_ = std::move(callback); }

    /// @brief Set callback for layer destruction
    void on_layer_destroyed(LayerCallback callback) { on_destroyed_ = std::move(callback); }

    // ==========================================================================
    // Predefined Layers
    // ==========================================================================

    /// @brief Create default layer set
    void create_default_layers();

    /// @brief Get shadow layer
    Layer* shadow_layer() { return get_layer("shadow"); }

    /// @brief Get world layer (main content)
    Layer* world_layer() { return get_layer("world"); }

    /// @brief Get UI layer
    Layer* ui_layer() { return get_layer("ui"); }

    /// @brief Get debug layer
    Layer* debug_layer() { return get_layer("debug"); }

private:
    void sort_layers();

    std::unordered_map<LayerId, std::unique_ptr<Layer>> layers_;
    std::unordered_map<std::string, LayerId> name_to_id_;
    std::unordered_map<NamespaceId, std::vector<LayerId>> namespace_layers_;
    std::vector<LayerId> sorted_layers_;  // Sorted by priority

    std::size_t max_layers_;
    bool sort_dirty_ = false;

    mutable std::mutex mutex_;

    LayerCallback on_created_;
    LayerCallback on_destroyed_;

    // Entity to layer mapping
    std::unordered_map<std::uint64_t, LayerId> entity_to_layer_;
    mutable std::mutex entity_mutex_;
};

// =============================================================================
// Layer Utilities
// =============================================================================

/// @brief Convert LayerType to string
const char* layer_type_to_string(LayerType type);

/// @brief Convert string to LayerType
LayerType string_to_layer_type(const std::string& str);

/// @brief Convert BlendMode to string
const char* blend_mode_to_string(BlendMode mode);

/// @brief Convert string to BlendMode
BlendMode string_to_blend_mode(const std::string& str);

// =============================================================================
// Layer Stack
// =============================================================================

/// @brief Stack-based layer management for render passes
class LayerStack {
public:
    LayerStack() = default;

    /// @brief Push layer onto stack
    void push(Layer* layer);

    /// @brief Pop layer from stack
    Layer* pop();

    /// @brief Peek at top layer
    Layer* top() const;

    /// @brief Check if stack is empty
    bool empty() const { return layers_.empty(); }

    /// @brief Get stack size
    std::size_t size() const { return layers_.size(); }

    /// @brief Clear the stack
    void clear() { layers_.clear(); }

    /// @brief Iterate layers (bottom to top)
    std::vector<Layer*>::iterator begin() { return layers_.begin(); }
    std::vector<Layer*>::iterator end() { return layers_.end(); }
    std::vector<Layer*>::const_iterator begin() const { return layers_.begin(); }
    std::vector<Layer*>::const_iterator end() const { return layers_.end(); }

private:
    std::vector<Layer*> layers_;
};

// =============================================================================
// Layer Compositor
// =============================================================================

/// @brief Composites multiple layers together
class LayerCompositor {
public:
    LayerCompositor() = default;

    /// @brief Set output dimensions
    void set_output_size(int width, int height);

    /// @brief Get output width
    int output_width() const { return output_width_; }

    /// @brief Get output height
    int output_height() const { return output_height_; }

    /// @brief Composite layers in order
    void composite(const std::vector<Layer*>& layers);

    /// @brief Composite single layer
    void composite_layer(Layer* layer);

    /// @brief Begin composition pass
    void begin();

    /// @brief End composition pass
    void end();

    /// @brief Set global opacity
    void set_global_opacity(float opacity) { global_opacity_ = opacity; }

    /// @brief Get global opacity
    float global_opacity() const { return global_opacity_; }

private:
    int output_width_ = 1920;
    int output_height_ = 1080;
    float global_opacity_ = 1.0f;
};

} // namespace void_runtime
