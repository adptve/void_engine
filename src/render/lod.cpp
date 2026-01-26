/// @file lod.cpp
/// @brief Level of Detail (LOD) system for void_render

#include "void_engine/render/mesh.hpp"

#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <functional>
#include <memory>

namespace void_render {

// =============================================================================
// LOD Configuration
// =============================================================================

/// LOD transition mode
enum class LodTransitionMode : std::uint8_t {
    Instant = 0,     // Hard switch between LODs
    CrossFade,       // Alpha blend between LODs
    Dithered,        // Dithered transition (screen-door)
    GeomorphBlend    // Blend vertex positions between LODs
};

/// LOD selection mode
enum class LodSelectionMode : std::uint8_t {
    Distance = 0,    // Based on camera distance
    ScreenSize,      // Based on projected screen size (pixels)
    ScreenCoverage   // Based on percentage of screen covered
};

/// LOD bias settings
struct LodBias {
    float global_multiplier = 1.0f;      // Global LOD bias (higher = lower quality)
    float shadow_multiplier = 2.0f;      // Extra bias for shadow passes
    float reflection_multiplier = 1.5f;  // Extra bias for reflection passes
    float min_screen_size = 16.0f;       // Minimum screen size before culling (pixels)

    [[nodiscard]] float effective_distance(float distance, float pass_multiplier) const noexcept {
        return distance * global_multiplier * pass_multiplier;
    }
};

// =============================================================================
// LOD Level
// =============================================================================

/// Single LOD level
struct LodLevel {
    MeshHandle mesh;
    float screen_size = 0.0f;     // Minimum screen size (pixels) for this LOD
    float distance = 0.0f;        // Maximum distance for this LOD (distance mode)
    float error = 0.0f;           // Geometric error metric (for automatic LOD selection)

    // Mesh statistics for this level
    std::uint32_t vertex_count = 0;
    std::uint32_t triangle_count = 0;

    // Quality factor (0.0 = lowest, 1.0 = highest detail)
    [[nodiscard]] float quality_factor() const noexcept {
        return screen_size > 0 ? screen_size / 1000.0f : 0.0f;
    }
};

// =============================================================================
// LOD Group - Collection of LOD levels for a single object
// =============================================================================

class LodGroup {
public:
    /// Add a LOD level
    void add_level(const LodLevel& level) {
        m_levels.push_back(level);
        // Keep sorted by distance (ascending) or screen size (descending)
        std::sort(m_levels.begin(), m_levels.end(),
            [](const LodLevel& a, const LodLevel& b) {
                return a.distance < b.distance;  // Closer = higher detail
            });
    }

    /// Select LOD based on distance
    [[nodiscard]] std::size_t select_by_distance(float distance, const LodBias& bias) const {
        float biased_distance = bias.effective_distance(distance, 1.0f);

        for (std::size_t i = 0; i < m_levels.size(); ++i) {
            if (biased_distance <= m_levels[i].distance) {
                return i;
            }
        }
        return m_levels.empty() ? 0 : m_levels.size() - 1;
    }

    /// Select LOD based on screen size (pixels)
    [[nodiscard]] std::size_t select_by_screen_size(
        float screen_size,
        const LodBias& bias) const {

        float biased_size = screen_size / bias.global_multiplier;

        // Cull if too small
        if (biased_size < bias.min_screen_size) {
            return SIZE_MAX;  // Indicates culled
        }

        // Find appropriate LOD (levels sorted by increasing detail)
        for (std::size_t i = m_levels.size(); i > 0; --i) {
            if (biased_size >= m_levels[i - 1].screen_size) {
                return i - 1;
            }
        }
        return m_levels.empty() ? 0 : m_levels.size() - 1;  // Lowest detail
    }

    /// Get LOD level
    [[nodiscard]] const LodLevel* get_level(std::size_t index) const {
        if (index >= m_levels.size()) return nullptr;
        return &m_levels[index];
    }

    /// Get level count
    [[nodiscard]] std::size_t level_count() const noexcept {
        return m_levels.size();
    }

    /// Check if group is empty
    [[nodiscard]] bool empty() const noexcept {
        return m_levels.empty();
    }

    /// Get all levels
    [[nodiscard]] const std::vector<LodLevel>& levels() const noexcept {
        return m_levels;
    }

    /// Set transition mode
    void set_transition_mode(LodTransitionMode mode) noexcept {
        m_transition_mode = mode;
    }

    [[nodiscard]] LodTransitionMode transition_mode() const noexcept {
        return m_transition_mode;
    }

    /// Set transition time (seconds)
    void set_transition_time(float seconds) noexcept {
        m_transition_time = seconds;
    }

    [[nodiscard]] float transition_time() const noexcept {
        return m_transition_time;
    }

    /// Set bounding sphere radius (for screen size calculations)
    void set_bounding_radius(float radius) noexcept {
        m_bounding_radius = radius;
    }

    [[nodiscard]] float bounding_radius() const noexcept {
        return m_bounding_radius;
    }

    /// Calculate screen size from distance and projection
    [[nodiscard]] float calculate_screen_size(
        float distance,
        float fov_radians,
        float screen_height) const noexcept {

        if (distance <= 0.0001f) return screen_height;

        // Project bounding sphere radius to screen space
        float projected_size = (m_bounding_radius / distance) *
                               (screen_height / (2.0f * std::tan(fov_radians * 0.5f)));
        return projected_size * 2.0f;  // Diameter
    }

private:
    std::vector<LodLevel> m_levels;
    LodTransitionMode m_transition_mode = LodTransitionMode::Instant;
    float m_transition_time = 0.1f;
    float m_bounding_radius = 1.0f;
};

// =============================================================================
// Mesh Simplification - QEM (Quadric Error Metrics)
// =============================================================================

class MeshSimplifier {
public:
    /// Simplification settings
    struct Settings {
        float target_ratio = 0.5f;           // Target vertex count ratio
        std::uint32_t target_triangles = 0;  // Target triangle count (0 = use ratio)
        float max_error = 0.001f;            // Maximum allowed error
        bool preserve_boundaries = true;
        bool preserve_uv_seams = true;
        bool lock_vertices_on_boundary = false;
        float attribute_weight = 0.0f;       // Weight for UV/normal preservation
    };

    /// Simplify mesh data
    [[nodiscard]] MeshData simplify(const MeshData& input, const Settings& settings) {
        if (input.vertex_count() < 4 || !input.is_indexed()) {
            return input;  // Can't simplify
        }

        m_vertices = input.vertices();
        m_indices.clear();
        const auto& src_indices = input.indices();
        m_indices.insert(m_indices.end(), src_indices.begin(), src_indices.end());

        std::uint32_t target_tris = settings.target_triangles;
        if (target_tris == 0) {
            target_tris = static_cast<std::uint32_t>(
                input.triangle_count() * settings.target_ratio);
        }
        target_tris = std::max(target_tris, 4u);

        // Initialize quadrics
        initialize_quadrics();

        // Find all edges
        build_edge_list();

        // Mark boundary vertices if needed
        if (settings.preserve_boundaries) {
            mark_boundary_vertices();
        }

        // Compute all edge collapse costs
        compute_collapse_costs(settings);

        // Collapse edges until target reached
        std::uint32_t current_tris = static_cast<std::uint32_t>(m_indices.size() / 3);

        while (current_tris > target_tris && !m_edge_heap.empty()) {
            // Find minimum cost edge
            auto best_it = std::min_element(m_edge_heap.begin(), m_edge_heap.end(),
                [](const EdgeCollapse& a, const EdgeCollapse& b) {
                    return a.cost < b.cost;
                });

            if (best_it->cost > settings.max_error * 1000.0f) {
                break;  // Error too high
            }

            // Collapse edge
            collapse_edge(*best_it, settings);
            m_edge_heap.erase(best_it);

            // Update affected edges
            update_affected_edges(settings);

            current_tris = count_valid_triangles();
        }

        // Build output mesh
        return build_output(input.topology());
    }

private:
    struct Quadric {
        double a[10] = {0};  // Symmetric 4x4 matrix in upper-triangular form

        void add(const Quadric& other) {
            for (int i = 0; i < 10; ++i) a[i] += other.a[i];
        }

        [[nodiscard]] double evaluate(float x, float y, float z) const {
            // Q(v) = v^T * A * v + 2 * b^T * v + c
            return a[0]*x*x + 2*a[1]*x*y + 2*a[2]*x*z + 2*a[3]*x +
                             a[4]*y*y + 2*a[5]*y*z + 2*a[6]*y +
                                        a[7]*z*z + 2*a[8]*z +
                                                   a[9];
        }

        [[nodiscard]] static Quadric from_plane(float nx, float ny, float nz, float d) {
            Quadric q;
            q.a[0] = nx * nx;
            q.a[1] = nx * ny;
            q.a[2] = nx * nz;
            q.a[3] = nx * d;
            q.a[4] = ny * ny;
            q.a[5] = ny * nz;
            q.a[6] = ny * d;
            q.a[7] = nz * nz;
            q.a[8] = nz * d;
            q.a[9] = d * d;
            return q;
        }
    };

    struct EdgeCollapse {
        std::uint32_t v0, v1;
        std::array<float, 3> optimal_pos;
        double cost;
        bool valid = true;
    };

    std::vector<Vertex> m_vertices;
    std::vector<std::uint32_t> m_indices;
    std::vector<Quadric> m_quadrics;
    std::vector<EdgeCollapse> m_edge_heap;
    std::unordered_set<std::uint32_t> m_boundary_vertices;
    std::unordered_map<std::uint64_t, std::size_t> m_edge_to_heap;
    std::vector<bool> m_vertex_removed;
    std::vector<std::vector<std::uint32_t>> m_vertex_faces;

    void initialize_quadrics() {
        m_quadrics.resize(m_vertices.size());
        m_vertex_removed.resize(m_vertices.size(), false);
        m_vertex_faces.resize(m_vertices.size());

        // Build vertex-face adjacency
        for (std::size_t i = 0; i < m_indices.size(); i += 3) {
            std::uint32_t tri_idx = static_cast<std::uint32_t>(i / 3);
            m_vertex_faces[m_indices[i]].push_back(tri_idx);
            m_vertex_faces[m_indices[i + 1]].push_back(tri_idx);
            m_vertex_faces[m_indices[i + 2]].push_back(tri_idx);
        }

        // Compute quadric for each vertex from adjacent faces
        for (std::size_t i = 0; i < m_indices.size(); i += 3) {
            const auto& v0 = m_vertices[m_indices[i]];
            const auto& v1 = m_vertices[m_indices[i + 1]];
            const auto& v2 = m_vertices[m_indices[i + 2]];

            // Compute plane equation
            float e1x = v1.position[0] - v0.position[0];
            float e1y = v1.position[1] - v0.position[1];
            float e1z = v1.position[2] - v0.position[2];
            float e2x = v2.position[0] - v0.position[0];
            float e2y = v2.position[1] - v0.position[1];
            float e2z = v2.position[2] - v0.position[2];

            float nx = e1y * e2z - e1z * e2y;
            float ny = e1z * e2x - e1x * e2z;
            float nz = e1x * e2y - e1y * e2x;

            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-8f) {
                nx /= len; ny /= len; nz /= len;
            }

            float d = -(nx * v0.position[0] + ny * v0.position[1] + nz * v0.position[2]);
            Quadric q = Quadric::from_plane(nx, ny, nz, d);

            m_quadrics[m_indices[i]].add(q);
            m_quadrics[m_indices[i + 1]].add(q);
            m_quadrics[m_indices[i + 2]].add(q);
        }
    }

    void build_edge_list() {
        std::unordered_set<std::uint64_t> seen;

        for (std::size_t i = 0; i < m_indices.size(); i += 3) {
            add_edge(m_indices[i], m_indices[i + 1], seen);
            add_edge(m_indices[i + 1], m_indices[i + 2], seen);
            add_edge(m_indices[i + 2], m_indices[i], seen);
        }
    }

    void add_edge(std::uint32_t v0, std::uint32_t v1, std::unordered_set<std::uint64_t>& seen) {
        if (v0 > v1) std::swap(v0, v1);
        std::uint64_t key = (static_cast<std::uint64_t>(v0) << 32) | v1;

        if (seen.insert(key).second) {
            EdgeCollapse edge;
            edge.v0 = v0;
            edge.v1 = v1;
            m_edge_heap.push_back(edge);
            m_edge_to_heap[key] = m_edge_heap.size() - 1;
        }
    }

    void mark_boundary_vertices() {
        // Edge is boundary if it appears in only one triangle
        std::unordered_map<std::uint64_t, int> edge_count;

        for (std::size_t i = 0; i < m_indices.size(); i += 3) {
            auto count_edge = [&](std::uint32_t a, std::uint32_t b) {
                if (a > b) std::swap(a, b);
                std::uint64_t key = (static_cast<std::uint64_t>(a) << 32) | b;
                edge_count[key]++;
            };

            count_edge(m_indices[i], m_indices[i + 1]);
            count_edge(m_indices[i + 1], m_indices[i + 2]);
            count_edge(m_indices[i + 2], m_indices[i]);
        }

        for (const auto& [key, count] : edge_count) {
            if (count == 1) {  // Boundary edge
                std::uint32_t v0 = static_cast<std::uint32_t>(key >> 32);
                std::uint32_t v1 = static_cast<std::uint32_t>(key & 0xFFFFFFFF);
                m_boundary_vertices.insert(v0);
                m_boundary_vertices.insert(v1);
            }
        }
    }

    void compute_collapse_costs(const Settings& settings) {
        for (auto& edge : m_edge_heap) {
            compute_edge_cost(edge, settings);
        }
    }

    void compute_edge_cost(EdgeCollapse& edge, const Settings& settings) {
        const auto& v0 = m_vertices[edge.v0];
        const auto& v1 = m_vertices[edge.v1];

        // Combined quadric
        Quadric q = m_quadrics[edge.v0];
        q.add(m_quadrics[edge.v1]);

        // Try to find optimal position (midpoint as fallback)
        edge.optimal_pos = {{
            (v0.position[0] + v1.position[0]) * 0.5f,
            (v0.position[1] + v1.position[1]) * 0.5f,
            (v0.position[2] + v1.position[2]) * 0.5f
        }};

        // Evaluate cost
        edge.cost = q.evaluate(edge.optimal_pos[0], edge.optimal_pos[1], edge.optimal_pos[2]);

        // Penalty for boundary vertices
        if (settings.preserve_boundaries) {
            bool v0_boundary = m_boundary_vertices.count(edge.v0) > 0;
            bool v1_boundary = m_boundary_vertices.count(edge.v1) > 0;

            if (v0_boundary && v1_boundary && settings.lock_vertices_on_boundary) {
                edge.cost = 1e30;  // Prevent collapse
            } else if (v0_boundary || v1_boundary) {
                edge.cost *= 10.0;  // High penalty
            }
        }

        // Attribute preservation weight
        if (settings.attribute_weight > 0) {
            float uv_dist = std::sqrt(
                std::pow(v0.uv0[0] - v1.uv0[0], 2) +
                std::pow(v0.uv0[1] - v1.uv0[1], 2));
            edge.cost += uv_dist * settings.attribute_weight * 1000.0;
        }
    }

    void collapse_edge(const EdgeCollapse& edge, [[maybe_unused]] const Settings& settings) {
        // Move v0 to optimal position
        m_vertices[edge.v0].position = edge.optimal_pos;

        // Merge quadrics
        m_quadrics[edge.v0].add(m_quadrics[edge.v1]);

        // Replace all references to v1 with v0
        for (std::uint32_t& idx : m_indices) {
            if (idx == edge.v1) {
                idx = edge.v0;
            }
        }

        // Mark v1 as removed
        m_vertex_removed[edge.v1] = true;

        // Invalidate edges containing v1
        for (auto& e : m_edge_heap) {
            if (e.v0 == edge.v1 || e.v1 == edge.v1) {
                e.valid = false;
            }
        }
    }

    void update_affected_edges(const Settings& settings) {
        // Remove invalid edges
        m_edge_heap.erase(
            std::remove_if(m_edge_heap.begin(), m_edge_heap.end(),
                [](const EdgeCollapse& e) { return !e.valid; }),
            m_edge_heap.end());

        // Recompute costs for edges connected to modified vertices
        for (auto& edge : m_edge_heap) {
            compute_edge_cost(edge, settings);
        }
    }

    [[nodiscard]] std::uint32_t count_valid_triangles() const {
        std::uint32_t count = 0;
        for (std::size_t i = 0; i < m_indices.size(); i += 3) {
            std::uint32_t i0 = m_indices[i];
            std::uint32_t i1 = m_indices[i + 1];
            std::uint32_t i2 = m_indices[i + 2];

            // Degenerate triangle check
            if (i0 != i1 && i1 != i2 && i0 != i2) {
                count++;
            }
        }
        return count;
    }

    [[nodiscard]] MeshData build_output(PrimitiveTopology topology) {
        MeshData output(topology);

        // Build vertex remap table
        std::vector<std::int32_t> remap(m_vertices.size(), -1);
        std::uint32_t new_idx = 0;

        for (std::size_t i = 0; i < m_vertices.size(); ++i) {
            if (!m_vertex_removed[i]) {
                remap[i] = static_cast<std::int32_t>(new_idx++);
                output.add_vertex(m_vertices[i]);
            }
        }

        // Add non-degenerate triangles
        for (std::size_t i = 0; i < m_indices.size(); i += 3) {
            std::uint32_t i0 = m_indices[i];
            std::uint32_t i1 = m_indices[i + 1];
            std::uint32_t i2 = m_indices[i + 2];

            // Skip degenerate
            if (i0 == i1 || i1 == i2 || i0 == i2) continue;

            std::int32_t r0 = remap[i0];
            std::int32_t r1 = remap[i1];
            std::int32_t r2 = remap[i2];

            if (r0 >= 0 && r1 >= 0 && r2 >= 0) {
                output.add_triangle(
                    static_cast<std::uint32_t>(r0),
                    static_cast<std::uint32_t>(r1),
                    static_cast<std::uint32_t>(r2));
            }
        }

        return output;
    }
};

// =============================================================================
// LOD Generator - Automatic LOD chain generation
// =============================================================================

class LodGenerator {
public:
    struct Settings {
        std::size_t level_count = 4;                    // Number of LOD levels
        float ratio_step = 0.5f;                        // Triangle reduction ratio per level
        std::vector<float> distance_thresholds;         // Custom distances per level
        std::vector<float> screen_size_thresholds;      // Custom screen sizes per level
        bool preserve_boundaries = true;
        bool generate_imposters = false;                // Generate billboard imposters for furthest LOD

        Settings() = default;
    };

    /// Generate LOD group from base mesh
    [[nodiscard]] LodGroup generate(
        const MeshData& base_mesh,
        MeshCache& cache,
        const std::string& base_name) {
        return generate(base_mesh, cache, base_name, Settings{});
    }

    [[nodiscard]] LodGroup generate(
        const MeshData& base_mesh,
        MeshCache& cache,
        const std::string& base_name,
        const Settings& settings) {

        LodGroup group;

        // LOD 0 = original mesh
        MeshHandle base_handle = cache.add(base_name + "_lod0", base_mesh);
        LodLevel level0;
        level0.mesh = base_handle;
        level0.distance = 0.0f;
        level0.screen_size = 1000.0f;  // Always use for close objects
        level0.vertex_count = static_cast<std::uint32_t>(base_mesh.vertex_count());
        level0.triangle_count = static_cast<std::uint32_t>(base_mesh.triangle_count());
        group.add_level(level0);

        // Generate simplified levels
        MeshSimplifier simplifier;
        MeshData current = base_mesh;
        float cumulative_ratio = 1.0f;

        // Default distance thresholds
        std::vector<float> distances = settings.distance_thresholds;
        if (distances.empty()) {
            distances = {10.0f, 25.0f, 50.0f, 100.0f, 200.0f};
        }

        // Default screen size thresholds
        std::vector<float> screen_sizes = settings.screen_size_thresholds;
        if (screen_sizes.empty()) {
            screen_sizes = {500.0f, 200.0f, 100.0f, 50.0f, 25.0f};
        }

        for (std::size_t i = 1; i < settings.level_count; ++i) {
            cumulative_ratio *= settings.ratio_step;

            MeshSimplifier::Settings simp_settings;
            simp_settings.target_ratio = cumulative_ratio;
            simp_settings.preserve_boundaries = settings.preserve_boundaries;

            MeshData simplified = simplifier.simplify(current, simp_settings);

            // Skip if simplification failed or didn't reduce much
            if (simplified.triangle_count() >= current.triangle_count() * 0.9f) {
                break;
            }

            std::string lod_name = base_name + "_lod" + std::to_string(i);
            MeshHandle handle = cache.add(lod_name, simplified);

            LodLevel level;
            level.mesh = handle;
            level.distance = i < distances.size() ? distances[i] : distances.back() * static_cast<float>(i);
            level.screen_size = i < screen_sizes.size() ? screen_sizes[i] : screen_sizes.back() / static_cast<float>(i);
            level.vertex_count = static_cast<std::uint32_t>(simplified.vertex_count());
            level.triangle_count = static_cast<std::uint32_t>(simplified.triangle_count());
            group.add_level(level);

            current = simplified;
        }

        // Calculate bounding radius from base mesh
        float max_dist = 0.0f;
        for (const auto& v : base_mesh.vertices()) {
            float dist = std::sqrt(
                v.position[0] * v.position[0] +
                v.position[1] * v.position[1] +
                v.position[2] * v.position[2]);
            max_dist = std::max(max_dist, dist);
        }
        group.set_bounding_radius(max_dist);

        return group;
    }
};

// =============================================================================
// LOD Manager - Scene-wide LOD management
// =============================================================================

class LodManager {
public:
    /// LOD selection result
    struct Selection {
        MeshHandle mesh;
        std::size_t lod_level = 0;
        float blend_factor = 0.0f;    // For cross-fade transitions
        bool culled = false;
    };

    /// Register LOD group
    void register_group(std::uint64_t entity_id, LodGroup group) {
        m_groups[entity_id] = std::move(group);
        m_transitions[entity_id] = TransitionState{};
    }

    /// Unregister LOD group
    void unregister_group(std::uint64_t entity_id) {
        m_groups.erase(entity_id);
        m_transitions.erase(entity_id);
    }

    /// Get LOD group
    [[nodiscard]] LodGroup* get_group(std::uint64_t entity_id) {
        auto it = m_groups.find(entity_id);
        return it != m_groups.end() ? &it->second : nullptr;
    }

    /// Select LOD for entity
    [[nodiscard]] Selection select(
        std::uint64_t entity_id,
        float distance,
        float screen_size,
        float dt) {

        Selection result;
        result.culled = true;

        auto group_it = m_groups.find(entity_id);
        if (group_it == m_groups.end() || group_it->second.empty()) {
            return result;
        }

        auto& group = group_it->second;

        // Select LOD based on mode
        std::size_t target_lod;
        if (m_selection_mode == LodSelectionMode::Distance) {
            target_lod = group.select_by_distance(distance, m_bias);
        } else {
            target_lod = group.select_by_screen_size(screen_size, m_bias);
            if (target_lod == SIZE_MAX) {
                return result;  // Culled
            }
        }

        const LodLevel* level = group.get_level(target_lod);
        if (!level) return result;

        result.culled = false;
        result.lod_level = target_lod;
        result.mesh = level->mesh;

        // Handle transitions
        auto& trans = m_transitions[entity_id];
        if (group.transition_mode() != LodTransitionMode::Instant) {
            if (trans.current_lod != target_lod) {
                // Start transition
                trans.previous_lod = trans.current_lod;
                trans.current_lod = target_lod;
                trans.blend_progress = 0.0f;
            }

            if (trans.blend_progress < 1.0f) {
                trans.blend_progress += dt / group.transition_time();
                trans.blend_progress = std::min(trans.blend_progress, 1.0f);
                result.blend_factor = trans.blend_progress;
            }
        } else {
            trans.current_lod = target_lod;
        }

        return result;
    }

    /// Set global selection mode
    void set_selection_mode(LodSelectionMode mode) noexcept {
        m_selection_mode = mode;
    }

    /// Set global bias
    void set_bias(const LodBias& bias) noexcept {
        m_bias = bias;
    }

    /// Get statistics
    struct Stats {
        std::size_t group_count = 0;
        std::size_t total_levels = 0;
        std::array<std::size_t, 8> level_distribution = {};  // How many objects at each LOD
    };

    [[nodiscard]] Stats get_stats() const {
        Stats stats;
        stats.group_count = m_groups.size();

        for (const auto& [id, group] : m_groups) {
            stats.total_levels += group.level_count();
        }

        for (const auto& [id, trans] : m_transitions) {
            if (trans.current_lod < stats.level_distribution.size()) {
                stats.level_distribution[trans.current_lod]++;
            }
        }

        return stats;
    }

    /// Clear all groups
    void clear() {
        m_groups.clear();
        m_transitions.clear();
    }

private:
    struct TransitionState {
        std::size_t previous_lod = 0;
        std::size_t current_lod = 0;
        float blend_progress = 1.0f;
    };

    std::unordered_map<std::uint64_t, LodGroup> m_groups;
    std::unordered_map<std::uint64_t, TransitionState> m_transitions;
    LodSelectionMode m_selection_mode = LodSelectionMode::Distance;
    LodBias m_bias;
};

// =============================================================================
// HLOD - Hierarchical LOD for large scenes
// =============================================================================

class HlodNode {
public:
    /// HLOD node type
    enum class Type : std::uint8_t {
        Leaf,       // Contains actual geometry
        Cluster,    // Contains merged child geometry
        Proxy       // Contains simplified proxy geometry
    };

    HlodNode() = default;

    /// Set as leaf node
    void set_leaf(std::uint64_t entity_id) {
        m_type = Type::Leaf;
        m_entity_id = entity_id;
    }

    /// Set as cluster node with merged mesh
    void set_cluster(MeshHandle merged_mesh) {
        m_type = Type::Cluster;
        m_merged_mesh = merged_mesh;
    }

    /// Add child node
    void add_child(std::size_t child_index) {
        m_children.push_back(child_index);
    }

    /// Set bounds
    void set_bounds(
        const std::array<float, 3>& min,
        const std::array<float, 3>& max) {
        m_bounds_min = min;
        m_bounds_max = max;
    }

    /// Set LOD distance threshold (switch to children when closer than this)
    void set_lod_distance(float distance) { m_lod_distance = distance; }

    [[nodiscard]] Type type() const noexcept { return m_type; }
    [[nodiscard]] std::uint64_t entity_id() const noexcept { return m_entity_id; }
    [[nodiscard]] MeshHandle merged_mesh() const noexcept { return m_merged_mesh; }
    [[nodiscard]] const std::vector<std::size_t>& children() const noexcept { return m_children; }
    [[nodiscard]] float lod_distance() const noexcept { return m_lod_distance; }
    [[nodiscard]] const std::array<float, 3>& bounds_min() const noexcept { return m_bounds_min; }
    [[nodiscard]] const std::array<float, 3>& bounds_max() const noexcept { return m_bounds_max; }

    /// Get center of bounds
    [[nodiscard]] std::array<float, 3> center() const noexcept {
        return {{
            (m_bounds_min[0] + m_bounds_max[0]) * 0.5f,
            (m_bounds_min[1] + m_bounds_max[1]) * 0.5f,
            (m_bounds_min[2] + m_bounds_max[2]) * 0.5f
        }};
    }

private:
    Type m_type = Type::Leaf;
    std::uint64_t m_entity_id = 0;
    MeshHandle m_merged_mesh;
    std::vector<std::size_t> m_children;
    float m_lod_distance = 100.0f;
    std::array<float, 3> m_bounds_min = {0, 0, 0};
    std::array<float, 3> m_bounds_max = {0, 0, 0};
};

class HlodTree {
public:
    /// Build HLOD tree from entities
    void build(
        const std::vector<std::uint64_t>& entity_ids,
        const std::function<std::array<float, 3>(std::uint64_t)>& get_position,
        const std::function<std::array<float, 6>(std::uint64_t)>& get_bounds,
        std::size_t cluster_size = 8) {

        m_nodes.clear();

        if (entity_ids.empty()) return;

        // Create leaf nodes
        std::vector<std::size_t> current_level;
        for (std::uint64_t id : entity_ids) {
            HlodNode node;
            node.set_leaf(id);

            auto bounds = get_bounds(id);
            node.set_bounds(
                {{bounds[0], bounds[1], bounds[2]}},
                {{bounds[3], bounds[4], bounds[5]}}
            );

            current_level.push_back(m_nodes.size());
            m_nodes.push_back(std::move(node));
        }

        // Build hierarchy bottom-up
        float lod_distance = 50.0f;

        while (current_level.size() > 1) {
            std::vector<std::size_t> next_level;

            // Cluster nodes spatially
            std::vector<bool> assigned(current_level.size(), false);

            for (std::size_t i = 0; i < current_level.size(); ++i) {
                if (assigned[i]) continue;

                HlodNode cluster;
                cluster.add_child(current_level[i]);
                assigned[i] = true;

                auto center = m_nodes[current_level[i]].center();

                // Find nearby unassigned nodes
                for (std::size_t j = i + 1;
                     j < current_level.size() && cluster.children().size() < cluster_size;
                     ++j) {

                    if (assigned[j]) continue;

                    auto other_center = m_nodes[current_level[j]].center();
                    float dist = std::sqrt(
                        std::pow(center[0] - other_center[0], 2) +
                        std::pow(center[1] - other_center[1], 2) +
                        std::pow(center[2] - other_center[2], 2));

                    if (dist < lod_distance) {
                        cluster.add_child(current_level[j]);
                        assigned[j] = true;
                    }
                }

                // Compute cluster bounds
                std::array<float, 3> min_bounds = {{1e30f, 1e30f, 1e30f}};
                std::array<float, 3> max_bounds = {{-1e30f, -1e30f, -1e30f}};

                for (std::size_t child_idx : cluster.children()) {
                    const auto& child = m_nodes[child_idx];
                    for (int k = 0; k < 3; ++k) {
                        min_bounds[k] = std::min(min_bounds[k], child.bounds_min()[k]);
                        max_bounds[k] = std::max(max_bounds[k], child.bounds_max()[k]);
                    }
                }

                cluster.set_bounds(min_bounds, max_bounds);
                cluster.set_lod_distance(lod_distance);

                next_level.push_back(m_nodes.size());
                m_nodes.push_back(std::move(cluster));
            }

            current_level = std::move(next_level);
            lod_distance *= 2.0f;
        }

        // Set root
        if (!current_level.empty()) {
            m_root = current_level[0];
        }
    }

    /// Traverse and select visible nodes
    void select_visible(
        const std::array<float, 3>& camera_pos,
        std::vector<std::uint64_t>& visible_entities,
        std::vector<MeshHandle>& visible_clusters) const {

        if (m_nodes.empty()) return;

        traverse(m_root, camera_pos, visible_entities, visible_clusters);
    }

    [[nodiscard]] std::size_t node_count() const noexcept { return m_nodes.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_nodes.empty(); }

private:
    void traverse(
        std::size_t node_idx,
        const std::array<float, 3>& camera_pos,
        std::vector<std::uint64_t>& visible_entities,
        std::vector<MeshHandle>& visible_clusters) const {

        const auto& node = m_nodes[node_idx];

        // Calculate distance to node
        auto center = node.center();
        float distance = std::sqrt(
            std::pow(camera_pos[0] - center[0], 2) +
            std::pow(camera_pos[1] - center[1], 2) +
            std::pow(camera_pos[2] - center[2], 2));

        if (node.type() == HlodNode::Type::Leaf) {
            // Always render leaves
            visible_entities.push_back(node.entity_id());
        } else if (distance < node.lod_distance()) {
            // Close enough - recurse to children
            for (std::size_t child_idx : node.children()) {
                traverse(child_idx, camera_pos, visible_entities, visible_clusters);
            }
        } else {
            // Far enough - use merged cluster
            if (node.merged_mesh().is_valid()) {
                visible_clusters.push_back(node.merged_mesh());
            } else {
                // No merged mesh, recurse anyway
                for (std::size_t child_idx : node.children()) {
                    traverse(child_idx, camera_pos, visible_entities, visible_clusters);
                }
            }
        }
    }

    std::vector<HlodNode> m_nodes;
    std::size_t m_root = 0;
};

} // namespace void_render
