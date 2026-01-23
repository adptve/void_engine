#pragma once

/// @file instancing.hpp
/// @brief GPU instancing and batching for void_render

#include "fwd.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

namespace void_render {

// =============================================================================
// InstanceData (GPU-ready, Pod/Zeroable equivalent)
// =============================================================================

/// Instance data for GPU instancing (256 bytes, aligned)
/// Shader locations: model_matrix (10-13), normal_matrix (14-16),
///                   color_tint (17), custom (18)
struct alignas(16) InstanceData {
    std::array<std::array<float, 4>, 4> model_matrix;   // 64 bytes (locations 10-13)
    std::array<std::array<float, 4>, 3> normal_matrix;  // 48 bytes (locations 14-16)
    std::array<float, 4> color_tint;                    // 16 bytes (location 17)
    std::array<float, 4> custom;                        // 16 bytes (location 18)

    /// Size in bytes
    static constexpr std::size_t SIZE = 144;

    /// Default constructor (identity transform, white tint)
    InstanceData()
        : model_matrix{{
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}
        }}
        , normal_matrix{{
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0}
        }}
        , color_tint{1, 1, 1, 1}
        , custom{0, 0, 0, 0} {}

    /// Construct with model matrix and color tint (calculates normal matrix)
    InstanceData(const std::array<std::array<float, 4>, 4>& model,
                 const std::array<float, 4>& tint)
        : model_matrix(model)
        , color_tint(tint)
        , custom{0, 0, 0, 0} {
        compute_normal_matrix();
    }

    /// Construct with all data
    InstanceData(const std::array<std::array<float, 4>, 4>& model,
                 const std::array<float, 4>& tint,
                 const std::array<float, 4>& cust)
        : model_matrix(model)
        , color_tint(tint)
        , custom(cust) {
        compute_normal_matrix();
    }

    /// Create from position only
    [[nodiscard]] static InstanceData from_position(float x, float y, float z) {
        InstanceData data;
        data.model_matrix[3][0] = x;
        data.model_matrix[3][1] = y;
        data.model_matrix[3][2] = z;
        return data;
    }

    /// Create from position and uniform scale
    [[nodiscard]] static InstanceData from_position_scale(float x, float y, float z, float scale) {
        InstanceData data;
        data.model_matrix[0][0] = scale;
        data.model_matrix[1][1] = scale;
        data.model_matrix[2][2] = scale;
        data.model_matrix[3][0] = x;
        data.model_matrix[3][1] = y;
        data.model_matrix[3][2] = z;
        data.compute_normal_matrix();
        return data;
    }

    /// Create from position, scale, and color
    [[nodiscard]] static InstanceData from_transform(
        float x, float y, float z,
        float sx, float sy, float sz,
        float r, float g, float b, float a = 1.0f) {
        InstanceData data;
        data.model_matrix[0][0] = sx;
        data.model_matrix[1][1] = sy;
        data.model_matrix[2][2] = sz;
        data.model_matrix[3][0] = x;
        data.model_matrix[3][1] = y;
        data.model_matrix[3][2] = z;
        data.color_tint = {r, g, b, a};
        data.compute_normal_matrix();
        return data;
    }

    /// Set position
    void set_position(float x, float y, float z) {
        model_matrix[3][0] = x;
        model_matrix[3][1] = y;
        model_matrix[3][2] = z;
    }

    /// Get position
    [[nodiscard]] std::array<float, 3> position() const {
        return {model_matrix[3][0], model_matrix[3][1], model_matrix[3][2]};
    }

    /// Set color tint
    void set_color(float r, float g, float b, float a = 1.0f) {
        color_tint = {r, g, b, a};
    }

    /// Compute normal matrix from model matrix (inverse transpose of upper 3x3)
    void compute_normal_matrix() {
        // Extract 3x3 portion
        float m00 = model_matrix[0][0], m01 = model_matrix[0][1], m02 = model_matrix[0][2];
        float m10 = model_matrix[1][0], m11 = model_matrix[1][1], m12 = model_matrix[1][2];
        float m20 = model_matrix[2][0], m21 = model_matrix[2][1], m22 = model_matrix[2][2];

        // Calculate cofactors
        float c00 = m11 * m22 - m12 * m21;
        float c01 = m12 * m20 - m10 * m22;
        float c02 = m10 * m21 - m11 * m20;
        float c10 = m02 * m21 - m01 * m22;
        float c11 = m00 * m22 - m02 * m20;
        float c12 = m01 * m20 - m00 * m21;
        float c20 = m01 * m12 - m02 * m11;
        float c21 = m02 * m10 - m00 * m12;
        float c22 = m00 * m11 - m01 * m10;

        // Determinant
        float det = m00 * c00 + m01 * c01 + m02 * c02;

        if (std::abs(det) > 1e-6f) {
            float inv_det = 1.0f / det;
            // Transpose of cofactor matrix (inverse transpose = adjugate / det)
            normal_matrix[0] = {c00 * inv_det, c10 * inv_det, c20 * inv_det, 0};
            normal_matrix[1] = {c01 * inv_det, c11 * inv_det, c21 * inv_det, 0};
            normal_matrix[2] = {c02 * inv_det, c12 * inv_det, c22 * inv_det, 0};
        } else {
            // Identity for degenerate case
            normal_matrix[0] = {1, 0, 0, 0};
            normal_matrix[1] = {0, 1, 0, 0};
            normal_matrix[2] = {0, 0, 1, 0};
        }
    }
};

static_assert(sizeof(InstanceData) == 144, "InstanceData must be 144 bytes");

// =============================================================================
// BatchKey
// =============================================================================

/// Key for grouping instances into batches
struct BatchKey {
    std::uint64_t mesh_id = 0;
    std::uint64_t material_id = 0;
    std::uint32_t layer_mask = 0xFFFFFFFF;

    /// Default constructor
    BatchKey() = default;

    /// Construct with IDs
    BatchKey(std::uint64_t mesh, std::uint64_t material, std::uint32_t layer = 0xFFFFFFFF)
        : mesh_id(mesh), material_id(material), layer_mask(layer) {}

    /// Comparison for std::map ordering
    bool operator<(const BatchKey& other) const noexcept {
        if (mesh_id != other.mesh_id) return mesh_id < other.mesh_id;
        if (material_id != other.material_id) return material_id < other.material_id;
        return layer_mask < other.layer_mask;
    }

    bool operator==(const BatchKey& other) const noexcept {
        return mesh_id == other.mesh_id &&
               material_id == other.material_id &&
               layer_mask == other.layer_mask;
    }
};

// =============================================================================
// InstanceBatch
// =============================================================================

/// A batch of instances with the same mesh/material
class InstanceBatch {
public:
    /// Construct with key
    explicit InstanceBatch(BatchKey key) : m_key(key) {}

    /// Get batch key
    [[nodiscard]] const BatchKey& key() const noexcept { return m_key; }

    /// Get instances
    [[nodiscard]] const std::vector<InstanceData>& instances() const noexcept {
        return m_instances;
    }

    /// Get mutable instances
    [[nodiscard]] std::vector<InstanceData>& instances() noexcept {
        return m_instances;
    }

    /// Get instance count
    [[nodiscard]] std::size_t size() const noexcept {
        return m_instances.size();
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return m_instances.empty();
    }

    /// Reserve capacity
    void reserve(std::size_t capacity) {
        m_instances.reserve(capacity);
    }

    /// Add instance
    void add(const InstanceData& data) {
        m_instances.push_back(data);
    }

    /// Add instance (move)
    void add(InstanceData&& data) {
        m_instances.push_back(std::move(data));
    }

    /// Clear instances
    void clear() {
        m_instances.clear();
    }

    /// Get raw data pointer (for GPU upload)
    [[nodiscard]] const void* data() const noexcept {
        return m_instances.data();
    }

    /// Get data size in bytes
    [[nodiscard]] std::size_t data_size() const noexcept {
        return m_instances.size() * sizeof(InstanceData);
    }

private:
    BatchKey m_key;
    std::vector<InstanceData> m_instances;
};

// =============================================================================
// BatcherStats
// =============================================================================

/// Statistics for instance batcher
struct BatcherStats {
    std::uint32_t total_instances = 0;
    std::uint32_t batch_count = 0;
    std::uint32_t overflow_count = 0;
    std::uint32_t max_batch_size = 0;
    float avg_batch_size = 0.0f;

    void reset() {
        total_instances = 0;
        batch_count = 0;
        overflow_count = 0;
        max_batch_size = 0;
        avg_batch_size = 0.0f;
    }

    void compute_average() {
        if (batch_count > 0) {
            avg_batch_size = static_cast<float>(total_instances) /
                             static_cast<float>(batch_count);
        }
    }
};

// =============================================================================
// InstanceBatcher
// =============================================================================

/// Manages instance batching for efficient GPU rendering
class InstanceBatcher {
public:
    /// Maximum instances per batch (GPU buffer limit)
    static constexpr std::uint32_t DEFAULT_MAX_INSTANCES = 65536;

    /// Construct with max instances per batch
    explicit InstanceBatcher(std::uint32_t max_instances = DEFAULT_MAX_INSTANCES)
        : m_max_instances_per_batch(std::min(max_instances, DEFAULT_MAX_INSTANCES)) {}

    /// Begin frame (clear all batches)
    void begin_frame() {
        m_batches.clear();
        m_stats.reset();
        m_current_frame++;
    }

    /// Add instance to batcher
    /// Returns true if successfully added, false if batch overflow
    bool add_instance(std::uint64_t entity_id,
                      std::uint64_t mesh_id,
                      std::uint64_t material_id,
                      const std::array<std::array<float, 4>, 4>& model_matrix,
                      const std::array<float, 4>& color_tint,
                      std::uint32_t layer_mask = 0xFFFFFFFF) {
        (void)entity_id;  // Could be used for picking/debugging

        BatchKey key(mesh_id, material_id, layer_mask);

        auto it = m_batches.find(key);
        if (it == m_batches.end()) {
            it = m_batches.emplace(key, InstanceBatch(key)).first;
        }

        auto& batch = it->second;

        if (batch.size() >= m_max_instances_per_batch) {
            m_stats.overflow_count++;
            return false;
        }

        batch.add(InstanceData(model_matrix, color_tint));
        m_stats.total_instances++;

        return true;
    }

    /// Add instance with custom data
    bool add_instance_with_custom(std::uint64_t entity_id,
                                   std::uint64_t mesh_id,
                                   std::uint64_t material_id,
                                   const std::array<std::array<float, 4>, 4>& model_matrix,
                                   const std::array<float, 4>& color_tint,
                                   const std::array<float, 4>& custom_data,
                                   std::uint32_t layer_mask = 0xFFFFFFFF) {
        (void)entity_id;

        BatchKey key(mesh_id, material_id, layer_mask);

        auto it = m_batches.find(key);
        if (it == m_batches.end()) {
            it = m_batches.emplace(key, InstanceBatch(key)).first;
        }

        auto& batch = it->second;

        if (batch.size() >= m_max_instances_per_batch) {
            m_stats.overflow_count++;
            return false;
        }

        batch.add(InstanceData(model_matrix, color_tint, custom_data));
        m_stats.total_instances++;

        return true;
    }

    /// End frame (compute statistics)
    void end_frame() {
        m_stats.batch_count = static_cast<std::uint32_t>(m_batches.size());

        for (const auto& [key, batch] : m_batches) {
            std::uint32_t size = static_cast<std::uint32_t>(batch.size());
            if (size > m_stats.max_batch_size) {
                m_stats.max_batch_size = size;
            }
        }

        m_stats.compute_average();
    }

    /// Get all batches
    [[nodiscard]] const std::map<BatchKey, InstanceBatch>& batches() const noexcept {
        return m_batches;
    }

    /// Get batch by key
    [[nodiscard]] const InstanceBatch* get_batch(const BatchKey& key) const {
        auto it = m_batches.find(key);
        if (it == m_batches.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Get statistics
    [[nodiscard]] const BatcherStats& stats() const noexcept {
        return m_stats;
    }

    /// Get current frame
    [[nodiscard]] std::uint64_t current_frame() const noexcept {
        return m_current_frame;
    }

    /// Get max instances per batch
    [[nodiscard]] std::uint32_t max_instances_per_batch() const noexcept {
        return m_max_instances_per_batch;
    }

    /// Set max instances per batch
    void set_max_instances_per_batch(std::uint32_t max) {
        m_max_instances_per_batch = std::min(max, DEFAULT_MAX_INSTANCES);
    }

    /// Iterate batches with callback
    template<typename F>
    void for_each_batch(F&& callback) const {
        for (const auto& [key, batch] : m_batches) {
            callback(key, batch);
        }
    }

private:
    std::map<BatchKey, InstanceBatch> m_batches;
    std::uint32_t m_max_instances_per_batch;
    std::uint64_t m_current_frame = 0;
    BatcherStats m_stats;
};

} // namespace void_render
