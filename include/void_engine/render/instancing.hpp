#pragma once

/// @file instancing.hpp
/// @brief GPU instancing and batching for void_render
///
/// Provides hardware-accelerated instance rendering with automatic batching,
/// GPU buffer management, and indirect drawing support.

#include "fwd.hpp"
#include "mesh.hpp"  // For MeshHandle
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <map>
#include <span>
#include <functional>
#include <memory>
#include <glm/glm.hpp>

namespace void_render {

// Forward declaration
class ShaderProgram;

// =============================================================================
// InstanceData (GPU-ready)
// =============================================================================

/// Instance data for GPU instancing (144 bytes, aligned)
struct alignas(16) InstanceData {
    std::array<std::array<float, 4>, 4> model_matrix;   // 64 bytes
    std::array<std::array<float, 4>, 3> normal_matrix;  // 48 bytes
    std::array<float, 4> color_tint;                    // 16 bytes
    std::array<float, 4> custom;                        // 16 bytes

    static constexpr std::size_t SIZE = 144;

    InstanceData();

    InstanceData(const std::array<std::array<float, 4>, 4>& model,
                 const std::array<float, 4>& tint);

    InstanceData(const std::array<std::array<float, 4>, 4>& model,
                 const std::array<float, 4>& tint,
                 const std::array<float, 4>& cust);

    [[nodiscard]] static InstanceData from_position(float x, float y, float z);
    [[nodiscard]] static InstanceData from_position_scale(float x, float y, float z, float scale);
    [[nodiscard]] static InstanceData from_transform(
        float x, float y, float z,
        float sx, float sy, float sz,
        float r, float g, float b, float a = 1.0f);
    [[nodiscard]] static InstanceData from_glm(const glm::mat4& model, const glm::vec4& tint);

    void compute_normal_matrix();
    void set_position(float x, float y, float z);
    void set_scale(float sx, float sy, float sz);
};

// =============================================================================
// BatchKey
// =============================================================================

/// Key for grouping instances by mesh and material
struct BatchKey {
    std::uint64_t mesh_id = 0;
    std::uint64_t material_id = 0;
    std::uint32_t layer_mask = 0xFFFFFFFF;

    BatchKey() = default;
    BatchKey(std::uint64_t mesh, std::uint64_t mat, std::uint32_t layer = 0xFFFFFFFF)
        : mesh_id(mesh), material_id(mat), layer_mask(layer) {}

    bool operator<(const BatchKey& other) const {
        if (mesh_id != other.mesh_id) return mesh_id < other.mesh_id;
        if (material_id != other.material_id) return material_id < other.material_id;
        return layer_mask < other.layer_mask;
    }

    bool operator==(const BatchKey& other) const {
        return mesh_id == other.mesh_id &&
               material_id == other.material_id &&
               layer_mask == other.layer_mask;
    }
};

// =============================================================================
// InstanceBuffer (GPU buffer wrapper)
// =============================================================================

/// GPU buffer for instance data with dynamic resizing
class InstanceBuffer {
public:
    InstanceBuffer();
    ~InstanceBuffer();

    // Move-only
    InstanceBuffer(const InstanceBuffer&) = delete;
    InstanceBuffer& operator=(const InstanceBuffer&) = delete;
    InstanceBuffer(InstanceBuffer&& other) noexcept;
    InstanceBuffer& operator=(InstanceBuffer&& other) noexcept;

    /// Initialize buffer with initial capacity
    [[nodiscard]] bool initialize(std::size_t initial_capacity, std::size_t stride);

    /// Release GPU resources
    void destroy();

    /// Resize buffer capacity
    void resize(std::size_t new_capacity);

    /// Update buffer data
    void update(const void* data, std::size_t count);

    /// Update partial buffer data
    void update_range(const void* data, std::size_t offset, std::size_t count);

    /// Bind buffer for rendering
    void bind() const;

    /// Unbind buffer
    static void unbind();

    /// Clear buffer (reset count, keep capacity)
    void clear();

    /// Get buffer handle
    [[nodiscard]] std::uint32_t buffer() const noexcept { return m_buffer; }

    /// Get current count
    [[nodiscard]] std::size_t count() const noexcept { return m_count; }

    /// Get capacity
    [[nodiscard]] std::size_t capacity() const noexcept { return m_capacity; }

private:
    std::uint32_t m_buffer = 0;
    std::size_t m_capacity = 0;
    std::size_t m_stride = 0;
    std::size_t m_count = 0;
};

// =============================================================================
// IndirectDrawBuffer
// =============================================================================

/// GPU buffer for indirect draw commands
class IndirectDrawBuffer {
public:
    IndirectDrawBuffer();
    ~IndirectDrawBuffer();

    // Move-only
    IndirectDrawBuffer(const IndirectDrawBuffer&) = delete;
    IndirectDrawBuffer& operator=(const IndirectDrawBuffer&) = delete;
    IndirectDrawBuffer(IndirectDrawBuffer&& other) noexcept;
    IndirectDrawBuffer& operator=(IndirectDrawBuffer&& other) noexcept;

    /// Initialize buffer
    [[nodiscard]] bool initialize(std::size_t max_commands);

    /// Release resources
    void destroy();

    /// Get buffer handle
    [[nodiscard]] std::uint32_t buffer() const noexcept { return m_buffer; }

private:
    std::uint32_t m_buffer = 0;
    std::size_t m_capacity = 0;
};

// =============================================================================
// InstanceBatch
// =============================================================================

/// A batch of instances with the same mesh/material and its GPU buffer
class InstanceBatch {
public:
    InstanceBatch();
    ~InstanceBatch();

    // Move-only
    InstanceBatch(const InstanceBatch&) = delete;
    InstanceBatch& operator=(const InstanceBatch&) = delete;
    InstanceBatch(InstanceBatch&& other) noexcept;
    InstanceBatch& operator=(InstanceBatch&& other) noexcept;

    /// Initialize with capacity
    [[nodiscard]] bool initialize(std::size_t initial_capacity);

    /// Add instance to batch
    void add(const InstanceData& instance);

    /// Add multiple instances
    void add_bulk(std::span<const InstanceData> instances);

    /// Clear all instances
    void clear();

    /// Upload data to GPU
    void upload();

    /// Bind instance buffer for rendering
    void bind() const;

    /// Get instance count
    [[nodiscard]] std::size_t size() const noexcept { return m_instances.size(); }

    /// Get instance count (alias)
    [[nodiscard]] std::size_t count() const noexcept { return m_instances.size(); }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept { return m_instances.empty(); }

    /// Get instances
    [[nodiscard]] const std::vector<InstanceData>& instances() const noexcept {
        return m_instances;
    }

private:
    std::vector<InstanceData> m_instances;
    InstanceBuffer m_buffer;
    bool m_dirty = false;
};

// =============================================================================
// InstanceBatcher
// =============================================================================

/// Manages instance batching for efficient GPU rendering
class InstanceBatcher {
public:
    /// Configuration for batcher
    struct Config {
        std::size_t max_batch_size = 65536;
        std::size_t max_batches = 256;
        bool auto_upload = true;
    };

    InstanceBatcher();
    ~InstanceBatcher();

    // Non-copyable
    InstanceBatcher(const InstanceBatcher&) = delete;
    InstanceBatcher& operator=(const InstanceBatcher&) = delete;

    /// Initialize batcher
    [[nodiscard]] bool initialize(const Config& config);

    /// Begin frame (clear all batches)
    void begin_frame();

    /// Submit single instance
    void submit(const BatchKey& key, const InstanceData& instance);

    /// Submit multiple instances
    void submit_bulk(const BatchKey& key, std::span<const InstanceData> instances);

    /// End frame and upload to GPU
    void end_frame();

    /// Iterate over all batches
    void for_each_batch(const std::function<void(const BatchKey&, const InstanceBatch&)>& callback) const;

    /// Get batch count
    [[nodiscard]] std::size_t batch_count() const noexcept { return m_batches.size(); }

    /// Get draw call count
    [[nodiscard]] std::uint32_t draw_calls() const noexcept { return m_draw_calls; }

    /// Get instances rendered
    [[nodiscard]] std::uint64_t instances_rendered() const noexcept { return m_instances_rendered; }

private:
    Config m_config;
    std::map<BatchKey, InstanceBatch> m_batches;
    std::map<BatchKey, std::size_t> m_batch_lookup;
    std::uint32_t m_draw_calls = 0;
    std::uint64_t m_instances_rendered = 0;
};

// =============================================================================
// InstanceRenderer
// =============================================================================

/// High-level GPU instance rendering system
class InstanceRenderer {
public:
    /// Rendering statistics
    struct Stats {
        std::uint32_t draw_calls = 0;
        std::uint64_t instances_rendered = 0;
        std::uint64_t triangles_rendered = 0;
    };

    InstanceRenderer();
    ~InstanceRenderer();

    // Non-copyable
    InstanceRenderer(const InstanceRenderer&) = delete;
    InstanceRenderer& operator=(const InstanceRenderer&) = delete;

    /// Initialize renderer
    [[nodiscard]] bool initialize(std::size_t max_instances = 65536);

    /// Shutdown and release resources
    void shutdown();

    /// Begin frame
    void begin_frame();

    /// Submit single instance
    void submit(MeshHandle mesh, MaterialHandle material, const InstanceData& instance);

    /// Submit batch of instances
    void submit_batch(MeshHandle mesh, MaterialHandle material,
                      std::span<const InstanceData> instances);

    /// End frame
    void end_frame();

    /// Render a single batch
    void render_batch(const BatchKey& key, const InstanceBatch& batch,
                      std::uint32_t vao, int index_count);

    /// Render all batches with callbacks
    void render_all(const std::function<void(const BatchKey&)>& setup_callback,
                    const std::function<std::pair<std::uint32_t, int>(MeshHandle)>& get_mesh);

    /// Get rendering statistics
    [[nodiscard]] Stats stats() const;

private:
    std::size_t m_max_instances = 0;
    InstanceBuffer m_instance_buffer;
    IndirectDrawBuffer m_indirect_buffer;
    InstanceBatcher m_batcher;
    std::vector<InstanceData> m_staging_instances;
    Stats m_draw_stats;
};

// =============================================================================
// MaterialHandle (if not defined elsewhere)
// =============================================================================

/// Handle to a material resource
struct MaterialHandle {
    std::uint64_t id = 0;
    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
};

} // namespace void_render
