#pragma once

/// @file mesh.hpp
/// @brief Mesh and geometry types for void_render

#include "fwd.hpp"
#include "resource.hpp"
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <atomic>
#include <array>

namespace void_render {

// =============================================================================
// Vertex (GPU-ready, #[repr(C)] equivalent)
// =============================================================================

/// Vertex data structure (144 bytes, GPU-aligned)
struct alignas(16) Vertex {
    std::array<float, 3> position;   // 12 bytes
    float _pad0 = 0.0f;              // 4 bytes (alignment)
    std::array<float, 3> normal;     // 12 bytes
    float _pad1 = 0.0f;              // 4 bytes (alignment)
    std::array<float, 4> tangent;    // 16 bytes (w = handedness)
    std::array<float, 2> uv0;        // 8 bytes
    std::array<float, 2> uv1;        // 8 bytes
    std::array<float, 4> color;      // 16 bytes (RGBA)

    /// Default constructor
    Vertex()
        : position{0, 0, 0}
        , normal{0, 1, 0}
        , tangent{1, 0, 0, 1}
        , uv0{0, 0}
        , uv1{0, 0}
        , color{1, 1, 1, 1} {}

    /// Construct with position
    explicit Vertex(float x, float y, float z)
        : position{x, y, z}
        , normal{0, 1, 0}
        , tangent{1, 0, 0, 1}
        , uv0{0, 0}
        , uv1{0, 0}
        , color{1, 1, 1, 1} {}

    /// Construct with position and normal
    Vertex(float px, float py, float pz, float nx, float ny, float nz)
        : position{px, py, pz}
        , normal{nx, ny, nz}
        , tangent{1, 0, 0, 1}
        , uv0{0, 0}
        , uv1{0, 0}
        , color{1, 1, 1, 1} {}

    /// Full constructor
    Vertex(std::array<float, 3> pos, std::array<float, 3> norm,
           std::array<float, 4> tan, std::array<float, 2> u0,
           std::array<float, 2> u1, std::array<float, 4> col)
        : position(pos)
        , normal(norm)
        , tangent(tan)
        , uv0(u0)
        , uv1(u1)
        , color(col) {}

};

/// Size of Vertex in bytes
inline constexpr std::size_t VERTEX_SIZE = 80;

static_assert(sizeof(Vertex) == VERTEX_SIZE, "Vertex must be 80 bytes");

// For compatibility, also provide Vertex::SIZE
struct VertexSizeHelper { static constexpr std::size_t SIZE = sizeof(Vertex); };

// =============================================================================
// Index Format
// =============================================================================

/// Index format
enum class IndexFormat : std::uint8_t {
    U16 = 0,
    U32
};

/// Get index size in bytes
[[nodiscard]] constexpr std::size_t index_size(IndexFormat format) noexcept {
    return format == IndexFormat::U16 ? 2 : 4;
}

// =============================================================================
// Primitive Topology
// =============================================================================

/// Primitive topology
enum class PrimitiveTopology : std::uint8_t {
    PointList = 0,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip
};

// =============================================================================
// MeshData
// =============================================================================

/// CPU-side mesh data
class MeshData {
public:
    /// Default constructor
    MeshData() = default;

    /// Construct with topology
    explicit MeshData(PrimitiveTopology topo)
        : m_topology(topo) {}

    /// Get vertices
    [[nodiscard]] const std::vector<Vertex>& vertices() const noexcept {
        return m_vertices;
    }

    /// Get mutable vertices
    [[nodiscard]] std::vector<Vertex>& vertices() noexcept {
        return m_vertices;
    }

    /// Get indices
    [[nodiscard]] const std::vector<std::uint32_t>& indices() const noexcept {
        return m_indices;
    }

    /// Get mutable indices
    [[nodiscard]] std::vector<std::uint32_t>& indices() noexcept {
        return m_indices;
    }

    /// Get topology
    [[nodiscard]] PrimitiveTopology topology() const noexcept {
        return m_topology;
    }

    /// Set topology
    void set_topology(PrimitiveTopology topo) noexcept {
        m_topology = topo;
    }

    /// Get vertex count
    [[nodiscard]] std::size_t vertex_count() const noexcept {
        return m_vertices.size();
    }

    /// Get index count
    [[nodiscard]] std::size_t index_count() const noexcept {
        return m_indices.size();
    }

    /// Check if indexed
    [[nodiscard]] bool is_indexed() const noexcept {
        return !m_indices.empty();
    }

    /// Get triangle count (for triangle topologies)
    [[nodiscard]] std::size_t triangle_count() const noexcept {
        if (m_topology == PrimitiveTopology::TriangleList) {
            std::size_t count = is_indexed() ? index_count() : vertex_count();
            return count / 3;
        }
        if (m_topology == PrimitiveTopology::TriangleStrip) {
            std::size_t count = is_indexed() ? index_count() : vertex_count();
            return count >= 3 ? count - 2 : 0;
        }
        return 0;
    }

    /// Clear all data
    void clear() {
        m_vertices.clear();
        m_indices.clear();
    }

    /// Reserve vertex capacity
    void reserve_vertices(std::size_t count) {
        m_vertices.reserve(count);
    }

    /// Reserve index capacity
    void reserve_indices(std::size_t count) {
        m_indices.reserve(count);
    }

    /// Add vertex
    void add_vertex(const Vertex& v) {
        m_vertices.push_back(v);
    }

    /// Add index
    void add_index(std::uint32_t idx) {
        m_indices.push_back(idx);
    }

    /// Add triangle (indices)
    void add_triangle(std::uint32_t i0, std::uint32_t i1, std::uint32_t i2) {
        m_indices.push_back(i0);
        m_indices.push_back(i1);
        m_indices.push_back(i2);
    }

    // -------------------------------------------------------------------------
    // Built-in primitives
    // -------------------------------------------------------------------------

    /// Create a quad (centered at origin, facing +Y)
    [[nodiscard]] static MeshData quad(float size = 1.0f) {
        MeshData mesh(PrimitiveTopology::TriangleList);
        float half = size * 0.5f;

        mesh.m_vertices = {
            Vertex({-half, 0, -half}, {0, 1, 0}, {1, 0, 0, 1}, {0, 0}, {0, 0}, {1, 1, 1, 1}),
            Vertex({ half, 0, -half}, {0, 1, 0}, {1, 0, 0, 1}, {1, 0}, {1, 0}, {1, 1, 1, 1}),
            Vertex({ half, 0,  half}, {0, 1, 0}, {1, 0, 0, 1}, {1, 1}, {1, 1}, {1, 1, 1, 1}),
            Vertex({-half, 0,  half}, {0, 1, 0}, {1, 0, 0, 1}, {0, 1}, {0, 1}, {1, 1, 1, 1})
        };
        mesh.m_indices = {0, 1, 2, 0, 2, 3};

        return mesh;
    }

    /// Create a plane (subdivided quad)
    [[nodiscard]] static MeshData plane(float size = 1.0f, std::uint32_t subdivisions = 1) {
        MeshData mesh(PrimitiveTopology::TriangleList);

        std::uint32_t segments = subdivisions + 1;
        float step = size / static_cast<float>(segments);
        float half = size * 0.5f;

        mesh.reserve_vertices(static_cast<std::size_t>((segments + 1) * (segments + 1)));
        mesh.reserve_indices(static_cast<std::size_t>(segments * segments * 6));

        // Generate vertices
        for (std::uint32_t z = 0; z <= segments; ++z) {
            for (std::uint32_t x = 0; x <= segments; ++x) {
                float px = -half + static_cast<float>(x) * step;
                float pz = -half + static_cast<float>(z) * step;
                float u = static_cast<float>(x) / static_cast<float>(segments);
                float v = static_cast<float>(z) / static_cast<float>(segments);

                Vertex vert;
                vert.position = {px, 0, pz};
                vert.normal = {0, 1, 0};
                vert.tangent = {1, 0, 0, 1};
                vert.uv0 = {u, v};
                vert.uv1 = {u, v};
                mesh.add_vertex(vert);
            }
        }

        // Generate indices
        for (std::uint32_t z = 0; z < segments; ++z) {
            for (std::uint32_t x = 0; x < segments; ++x) {
                std::uint32_t i = z * (segments + 1) + x;
                mesh.add_triangle(i, i + segments + 1, i + 1);
                mesh.add_triangle(i + 1, i + segments + 1, i + segments + 2);
            }
        }

        return mesh;
    }

    /// Create a cube
    [[nodiscard]] static MeshData cube(float size = 1.0f) {
        MeshData mesh(PrimitiveTopology::TriangleList);
        float h = size * 0.5f;

        // 24 vertices (4 per face for proper normals)
        mesh.m_vertices = {
            // Front face (+Z)
            Vertex({-h, -h,  h}, {0, 0, 1}, {1, 0, 0, 1}, {0, 0}, {0, 0}, {1, 1, 1, 1}),
            Vertex({ h, -h,  h}, {0, 0, 1}, {1, 0, 0, 1}, {1, 0}, {1, 0}, {1, 1, 1, 1}),
            Vertex({ h,  h,  h}, {0, 0, 1}, {1, 0, 0, 1}, {1, 1}, {1, 1}, {1, 1, 1, 1}),
            Vertex({-h,  h,  h}, {0, 0, 1}, {1, 0, 0, 1}, {0, 1}, {0, 1}, {1, 1, 1, 1}),
            // Back face (-Z)
            Vertex({ h, -h, -h}, {0, 0, -1}, {-1, 0, 0, 1}, {0, 0}, {0, 0}, {1, 1, 1, 1}),
            Vertex({-h, -h, -h}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 0}, {1, 0}, {1, 1, 1, 1}),
            Vertex({-h,  h, -h}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 1}, {1, 1}, {1, 1, 1, 1}),
            Vertex({ h,  h, -h}, {0, 0, -1}, {-1, 0, 0, 1}, {0, 1}, {0, 1}, {1, 1, 1, 1}),
            // Top face (+Y)
            Vertex({-h,  h,  h}, {0, 1, 0}, {1, 0, 0, 1}, {0, 0}, {0, 0}, {1, 1, 1, 1}),
            Vertex({ h,  h,  h}, {0, 1, 0}, {1, 0, 0, 1}, {1, 0}, {1, 0}, {1, 1, 1, 1}),
            Vertex({ h,  h, -h}, {0, 1, 0}, {1, 0, 0, 1}, {1, 1}, {1, 1}, {1, 1, 1, 1}),
            Vertex({-h,  h, -h}, {0, 1, 0}, {1, 0, 0, 1}, {0, 1}, {0, 1}, {1, 1, 1, 1}),
            // Bottom face (-Y)
            Vertex({-h, -h, -h}, {0, -1, 0}, {1, 0, 0, 1}, {0, 0}, {0, 0}, {1, 1, 1, 1}),
            Vertex({ h, -h, -h}, {0, -1, 0}, {1, 0, 0, 1}, {1, 0}, {1, 0}, {1, 1, 1, 1}),
            Vertex({ h, -h,  h}, {0, -1, 0}, {1, 0, 0, 1}, {1, 1}, {1, 1}, {1, 1, 1, 1}),
            Vertex({-h, -h,  h}, {0, -1, 0}, {1, 0, 0, 1}, {0, 1}, {0, 1}, {1, 1, 1, 1}),
            // Right face (+X)
            Vertex({ h, -h,  h}, {1, 0, 0}, {0, 0, -1, 1}, {0, 0}, {0, 0}, {1, 1, 1, 1}),
            Vertex({ h, -h, -h}, {1, 0, 0}, {0, 0, -1, 1}, {1, 0}, {1, 0}, {1, 1, 1, 1}),
            Vertex({ h,  h, -h}, {1, 0, 0}, {0, 0, -1, 1}, {1, 1}, {1, 1}, {1, 1, 1, 1}),
            Vertex({ h,  h,  h}, {1, 0, 0}, {0, 0, -1, 1}, {0, 1}, {0, 1}, {1, 1, 1, 1}),
            // Left face (-X)
            Vertex({-h, -h, -h}, {-1, 0, 0}, {0, 0, 1, 1}, {0, 0}, {0, 0}, {1, 1, 1, 1}),
            Vertex({-h, -h,  h}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 0}, {1, 0}, {1, 1, 1, 1}),
            Vertex({-h,  h,  h}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 1}, {1, 1}, {1, 1, 1, 1}),
            Vertex({-h,  h, -h}, {-1, 0, 0}, {0, 0, 1, 1}, {0, 1}, {0, 1}, {1, 1, 1, 1})
        };

        mesh.m_indices = {
            0,  1,  2,  0,  2,  3,   // Front
            4,  5,  6,  4,  6,  7,   // Back
            8,  9,  10, 8,  10, 11,  // Top
            12, 13, 14, 12, 14, 15,  // Bottom
            16, 17, 18, 16, 18, 19,  // Right
            20, 21, 22, 20, 22, 23   // Left
        };

        return mesh;
    }

    /// Create a sphere
    [[nodiscard]] static MeshData sphere(float radius = 0.5f,
                                          std::uint32_t segments = 32,
                                          std::uint32_t rings = 16) {
        MeshData mesh(PrimitiveTopology::TriangleList);

        mesh.reserve_vertices(static_cast<std::size_t>((rings + 1) * (segments + 1)));
        mesh.reserve_indices(static_cast<std::size_t>(rings * segments * 6));

        const float pi = 3.14159265358979323846f;

        for (std::uint32_t ring = 0; ring <= rings; ++ring) {
            float phi = static_cast<float>(ring) / static_cast<float>(rings) * pi;
            float sin_phi = std::sin(phi);
            float cos_phi = std::cos(phi);

            for (std::uint32_t seg = 0; seg <= segments; ++seg) {
                float theta = static_cast<float>(seg) / static_cast<float>(segments) * 2.0f * pi;
                float sin_theta = std::sin(theta);
                float cos_theta = std::cos(theta);

                float x = cos_theta * sin_phi;
                float y = cos_phi;
                float z = sin_theta * sin_phi;

                Vertex vert;
                vert.position = {x * radius, y * radius, z * radius};
                vert.normal = {x, y, z};
                vert.tangent = {-sin_theta, 0, cos_theta, 1};
                vert.uv0 = {
                    static_cast<float>(seg) / static_cast<float>(segments),
                    static_cast<float>(ring) / static_cast<float>(rings)
                };
                vert.uv1 = vert.uv0;
                mesh.add_vertex(vert);
            }
        }

        for (std::uint32_t ring = 0; ring < rings; ++ring) {
            for (std::uint32_t seg = 0; seg < segments; ++seg) {
                std::uint32_t i = ring * (segments + 1) + seg;
                mesh.add_triangle(i, i + segments + 1, i + 1);
                mesh.add_triangle(i + 1, i + segments + 1, i + segments + 2);
            }
        }

        return mesh;
    }

    /// Create a cylinder
    [[nodiscard]] static MeshData cylinder(float radius = 0.5f, float height = 1.0f,
                                            std::uint32_t segments = 32) {
        MeshData mesh(PrimitiveTopology::TriangleList);

        const float pi = 3.14159265358979323846f;
        float half_height = height * 0.5f;

        // Side vertices
        for (std::uint32_t i = 0; i <= segments; ++i) {
            float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * pi;
            float x = std::cos(theta);
            float z = std::sin(theta);
            float u = static_cast<float>(i) / static_cast<float>(segments);

            // Bottom
            Vertex bottom;
            bottom.position = {x * radius, -half_height, z * radius};
            bottom.normal = {x, 0, z};
            bottom.tangent = {-z, 0, x, 1};
            bottom.uv0 = {u, 0};
            mesh.add_vertex(bottom);

            // Top
            Vertex top;
            top.position = {x * radius, half_height, z * radius};
            top.normal = {x, 0, z};
            top.tangent = {-z, 0, x, 1};
            top.uv0 = {u, 1};
            mesh.add_vertex(top);
        }

        // Side indices
        for (std::uint32_t i = 0; i < segments; ++i) {
            std::uint32_t b0 = i * 2;
            std::uint32_t t0 = i * 2 + 1;
            std::uint32_t b1 = (i + 1) * 2;
            std::uint32_t t1 = (i + 1) * 2 + 1;

            mesh.add_triangle(b0, b1, t0);
            mesh.add_triangle(t0, b1, t1);
        }

        // Cap centers
        std::uint32_t bottom_center = static_cast<std::uint32_t>(mesh.vertex_count());
        Vertex bc;
        bc.position = {0, -half_height, 0};
        bc.normal = {0, -1, 0};
        bc.tangent = {1, 0, 0, 1};
        bc.uv0 = {0.5f, 0.5f};
        mesh.add_vertex(bc);

        std::uint32_t top_center = static_cast<std::uint32_t>(mesh.vertex_count());
        Vertex tc;
        tc.position = {0, half_height, 0};
        tc.normal = {0, 1, 0};
        tc.tangent = {1, 0, 0, 1};
        tc.uv0 = {0.5f, 0.5f};
        mesh.add_vertex(tc);

        // Cap vertices and indices
        std::uint32_t cap_start = static_cast<std::uint32_t>(mesh.vertex_count());
        for (std::uint32_t i = 0; i <= segments; ++i) {
            float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * pi;
            float x = std::cos(theta);
            float z = std::sin(theta);

            // Bottom cap
            Vertex bv;
            bv.position = {x * radius, -half_height, z * radius};
            bv.normal = {0, -1, 0};
            bv.tangent = {1, 0, 0, 1};
            bv.uv0 = {x * 0.5f + 0.5f, z * 0.5f + 0.5f};
            mesh.add_vertex(bv);

            // Top cap
            Vertex tv;
            tv.position = {x * radius, half_height, z * radius};
            tv.normal = {0, 1, 0};
            tv.tangent = {1, 0, 0, 1};
            tv.uv0 = {x * 0.5f + 0.5f, z * 0.5f + 0.5f};
            mesh.add_vertex(tv);
        }

        // Cap indices
        for (std::uint32_t i = 0; i < segments; ++i) {
            // Bottom cap (CCW from below)
            mesh.add_triangle(bottom_center, cap_start + (i + 1) * 2, cap_start + i * 2);
            // Top cap (CCW from above)
            mesh.add_triangle(top_center, cap_start + i * 2 + 1, cap_start + (i + 1) * 2 + 1);
        }

        return mesh;
    }

private:
    std::vector<Vertex> m_vertices;
    std::vector<std::uint32_t> m_indices;
    PrimitiveTopology m_topology = PrimitiveTopology::TriangleList;
};

// =============================================================================
// GPU Buffer Handles
// =============================================================================

/// GPU vertex buffer metadata
struct GpuVertexBuffer {
    std::uint64_t id = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t stride = 0;
    std::uint64_t size_bytes = 0;

    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
};

/// GPU index buffer metadata
struct GpuIndexBuffer {
    std::uint64_t id = 0;
    std::uint32_t index_count = 0;
    IndexFormat format = IndexFormat::U32;
    std::uint64_t size_bytes = 0;

    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
};

// =============================================================================
// Cached Mesh
// =============================================================================

/// Cached primitive (submesh)
struct CachedPrimitive {
    std::uint32_t index = 0;
    GpuVertexBuffer vertex_buffer;
    std::optional<GpuIndexBuffer> index_buffer;
    std::uint32_t triangle_count = 0;
    std::optional<std::uint32_t> material_index;
};

/// Cached mesh data
struct CachedMesh {
    std::uint64_t asset_id = 0;
    std::string path;
    std::vector<CachedPrimitive> primitives;
    std::uint64_t gpu_memory = 0;
    std::uint32_t ref_count = 0;
    std::uint64_t last_access_frame = 0;
};

/// Mesh handle
struct MeshHandle {
    std::uint64_t asset_id = 0;
    std::uint64_t generation = 0;

    [[nodiscard]] bool is_valid() const noexcept {
        return asset_id != 0;
    }

    bool operator==(const MeshHandle& other) const noexcept {
        return asset_id == other.asset_id && generation == other.generation;
    }
};

// =============================================================================
// MeshCache
// =============================================================================

/// Mesh cache statistics
struct MeshCacheStats {
    std::size_t mesh_count = 0;
    std::size_t primitive_count = 0;
    std::uint64_t memory_used = 0;
    std::uint64_t memory_budget = 0;
    std::size_t cache_hits = 0;
    std::size_t cache_misses = 0;
    std::size_t evictions = 0;
};

/// Mesh cache for GPU resources
class MeshCache {
public:
    /// Construct with memory budget in megabytes
    explicit MeshCache(std::uint64_t memory_budget_mb = 256)
        : m_memory_budget(memory_budget_mb * 1024 * 1024) {}

    /// Get or load mesh by path
    [[nodiscard]] std::optional<MeshHandle> get_or_load(const std::string& path) {
        auto it = m_path_to_id.find(path);
        if (it != m_path_to_id.end()) {
            // Cache hit
            auto& mesh = m_meshes[it->second];
            mesh.ref_count++;
            mesh.last_access_frame = m_current_frame;
            m_stats.cache_hits++;
            return MeshHandle{it->second, m_generation};
        }

        m_stats.cache_misses++;

        // Would load from disk here - placeholder
        return std::nullopt;
    }

    /// Get cached mesh
    [[nodiscard]] const CachedMesh* get(MeshHandle handle) const {
        if (!handle.is_valid() || handle.generation != m_generation) {
            return nullptr;
        }
        auto it = m_meshes.find(handle.asset_id);
        if (it == m_meshes.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Add mesh to cache
    MeshHandle add(const std::string& path, const MeshData& data) {
        std::uint64_t id = m_next_asset_id++;

        CachedMesh cached;
        cached.asset_id = id;
        cached.path = path;
        cached.ref_count = 1;
        cached.last_access_frame = m_current_frame;

        // Calculate memory (simplified)
        std::uint64_t vertex_bytes = data.vertex_count() * sizeof(Vertex);
        std::uint64_t index_bytes = data.index_count() * sizeof(std::uint32_t);
        cached.gpu_memory = vertex_bytes + index_bytes;

        // Create single primitive
        CachedPrimitive prim;
        prim.index = 0;
        prim.vertex_buffer.id = m_next_buffer_id++;
        prim.vertex_buffer.vertex_count = static_cast<std::uint32_t>(data.vertex_count());
        prim.vertex_buffer.stride = sizeof(Vertex);
        prim.vertex_buffer.size_bytes = vertex_bytes;

        if (data.is_indexed()) {
            GpuIndexBuffer ib;
            ib.id = m_next_buffer_id++;
            ib.index_count = static_cast<std::uint32_t>(data.index_count());
            ib.format = IndexFormat::U32;
            ib.size_bytes = index_bytes;
            prim.index_buffer = ib;
        }

        prim.triangle_count = static_cast<std::uint32_t>(data.triangle_count());
        cached.primitives.push_back(prim);

        m_memory_usage += cached.gpu_memory;
        m_meshes[id] = std::move(cached);
        m_path_to_id[path] = id;

        // Evict if over budget
        evict_lru();

        return MeshHandle{id, m_generation};
    }

    /// Release reference to mesh
    void release(MeshHandle handle) {
        if (!handle.is_valid()) return;

        auto it = m_meshes.find(handle.asset_id);
        if (it != m_meshes.end() && it->second.ref_count > 0) {
            it->second.ref_count--;
        }
    }

    /// Begin frame (update frame counter)
    void begin_frame() {
        m_current_frame++;
    }

    /// Get statistics
    [[nodiscard]] MeshCacheStats stats() const {
        MeshCacheStats s = m_stats;
        s.mesh_count = m_meshes.size();
        s.memory_used = m_memory_usage;
        s.memory_budget = m_memory_budget;
        for (const auto& [id, mesh] : m_meshes) {
            s.primitive_count += mesh.primitives.size();
        }
        return s;
    }

    /// Get memory budget
    [[nodiscard]] std::uint64_t memory_budget() const noexcept {
        return m_memory_budget;
    }

    /// Get memory usage
    [[nodiscard]] std::uint64_t memory_usage() const noexcept {
        return m_memory_usage;
    }

    /// Clear cache
    void clear() {
        m_meshes.clear();
        m_path_to_id.clear();
        m_memory_usage = 0;
        m_generation++;
    }

private:
    void evict_lru() {
        while (m_memory_usage > m_memory_budget && !m_meshes.empty()) {
            // Find LRU with ref_count == 0
            std::uint64_t lru_id = 0;
            std::uint64_t lru_frame = UINT64_MAX;

            for (const auto& [id, mesh] : m_meshes) {
                if (mesh.ref_count == 0 && mesh.last_access_frame < lru_frame) {
                    lru_id = id;
                    lru_frame = mesh.last_access_frame;
                }
            }

            if (lru_id == 0) break;  // No evictable meshes

            auto it = m_meshes.find(lru_id);
            if (it != m_meshes.end()) {
                m_memory_usage -= it->second.gpu_memory;
                m_path_to_id.erase(it->second.path);
                m_meshes.erase(it);
                m_stats.evictions++;
            }
        }
    }

    std::unordered_map<std::uint64_t, CachedMesh> m_meshes;
    std::unordered_map<std::string, std::uint64_t> m_path_to_id;
    std::uint64_t m_next_buffer_id = 1;
    std::uint64_t m_next_asset_id = 1;
    std::uint64_t m_current_frame = 0;
    std::uint64_t m_memory_budget;
    std::uint64_t m_memory_usage = 0;
    std::uint64_t m_generation = 0;
    MeshCacheStats m_stats;
};

// =============================================================================
// MeshTypeId (built-in primitives)
// =============================================================================

/// Mesh type identifier
enum class MeshTypeId : std::uint8_t {
    Cube = 0,
    Sphere,
    Cylinder,
    Plane,
    Quad,
    Custom  // Uses asset ID
};

} // namespace void_render

// Hash specialization
template<>
struct std::hash<void_render::MeshHandle> {
    std::size_t operator()(const void_render::MeshHandle& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.asset_id) ^
               (std::hash<std::uint64_t>{}(h.generation) << 1);
    }
};
