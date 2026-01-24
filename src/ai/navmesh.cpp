/// @file navmesh.cpp
/// @brief Navigation mesh implementation for void_ai module

#include <void_engine/ai/navmesh.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <random>

namespace void_ai {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

float vec3_dot(const void_math::Vec3& a, const void_math::Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void_math::Vec3 vec3_cross(const void_math::Vec3& a, const void_math::Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float vec3_length(const void_math::Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

void_math::Vec3 vec3_normalize(const void_math::Vec3& v) {
    float len = vec3_length(v);
    if (len > 1e-6f) {
        return {v.x / len, v.y / len, v.z / len};
    }
    return v;
}

float vec3_distance(const void_math::Vec3& a, const void_math::Vec3& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void_math::Vec3 vec3_subtract(const void_math::Vec3& a, const void_math::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

void_math::Vec3 vec3_add(const void_math::Vec3& a, const void_math::Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

void_math::Vec3 vec3_scale(const void_math::Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

// Point to line segment distance
float point_to_segment_distance(const void_math::Vec3& p,
                                const void_math::Vec3& a,
                                const void_math::Vec3& b,
                                void_math::Vec3& closest) {
    auto ab = vec3_subtract(b, a);
    auto ap = vec3_subtract(p, a);

    float t = vec3_dot(ap, ab) / (vec3_dot(ab, ab) + 1e-6f);
    t = std::clamp(t, 0.0f, 1.0f);

    closest = vec3_add(a, vec3_scale(ab, t));
    return vec3_distance(p, closest);
}

// Check if point is inside triangle (2D, ignoring Y)
bool point_in_triangle_2d(const void_math::Vec3& p,
                          const void_math::Vec3& a,
                          const void_math::Vec3& b,
                          const void_math::Vec3& c) {
    auto sign = [](const void_math::Vec3& p1, const void_math::Vec3& p2, const void_math::Vec3& p3) {
        return (p1.x - p3.x) * (p2.z - p3.z) - (p2.x - p3.x) * (p1.z - p3.z);
    };

    float d1 = sign(p, a, b);
    float d2 = sign(p, b, c);
    float d3 = sign(p, c, a);

    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

// Calculate triangle area
float triangle_area(const void_math::Vec3& a, const void_math::Vec3& b, const void_math::Vec3& c) {
    auto ab = vec3_subtract(b, a);
    auto ac = vec3_subtract(c, a);
    auto cross = vec3_cross(ab, ac);
    return vec3_length(cross) * 0.5f;
}

} // anonymous namespace

// =============================================================================
// NavMesh Implementation
// =============================================================================

NavMesh::NavMesh() {
    // Initialize default area costs
    for (int i = 0; i <= static_cast<int>(AreaType::Custom3); ++i) {
        m_area_costs[static_cast<std::uint8_t>(i)] = 1.0f;
    }
    m_area_costs[static_cast<std::uint8_t>(AreaType::NotWalkable)] = 1000.0f;
}

void NavMesh::add_vertex(const void_math::Vec3& position) {
    NavVertex vertex;
    vertex.position = position;
    vertex.index = static_cast<std::uint32_t>(m_vertices.size());
    m_vertices.push_back(vertex);
    update_bounds();
}

void NavMesh::add_polygon(const std::vector<std::uint32_t>& vertices, std::uint32_t flags) {
    NavPolygon poly;
    poly.vertices = vertices;
    poly.flags = flags;

    // Calculate center
    void_math::Vec3 center{};
    for (std::uint32_t vi : vertices) {
        if (vi < m_vertices.size()) {
            center.x += m_vertices[vi].position.x;
            center.y += m_vertices[vi].position.y;
            center.z += m_vertices[vi].position.z;
        }
    }
    if (!vertices.empty()) {
        float inv_count = 1.0f / static_cast<float>(vertices.size());
        center.x *= inv_count;
        center.y *= inv_count;
        center.z *= inv_count;
    }
    poly.center = center;

    // Calculate area (sum of triangle areas)
    if (vertices.size() >= 3) {
        const auto& v0 = m_vertices[vertices[0]].position;
        for (std::size_t i = 2; i < vertices.size(); ++i) {
            const auto& v1 = m_vertices[vertices[i - 1]].position;
            const auto& v2 = m_vertices[vertices[i]].position;
            poly.area += triangle_area(v0, v1, v2);
        }
    }

    m_total_area += poly.area;
    m_polygons.push_back(poly);
}

void NavMesh::build_connectivity() {
    // Build neighbor connections based on shared edges
    for (std::size_t i = 0; i < m_polygons.size(); ++i) {
        m_polygons[i].neighbors.clear();

        for (std::size_t j = 0; j < m_polygons.size(); ++j) {
            if (i == j) continue;

            // Check for shared edge (2 shared vertices)
            int shared_count = 0;
            for (std::uint32_t vi : m_polygons[i].vertices) {
                for (std::uint32_t vj : m_polygons[j].vertices) {
                    if (vi == vj) {
                        shared_count++;
                        if (shared_count >= 2) {
                            m_polygons[i].neighbors.push_back(static_cast<std::uint32_t>(j));
                            break;
                        }
                    }
                }
                if (shared_count >= 2) break;
            }
        }
    }
}

void NavMesh::calculate_polygon_data() {
    m_total_area = 0;
    for (auto& poly : m_polygons) {
        // Recalculate center and area
        void_math::Vec3 center{};
        for (std::uint32_t vi : poly.vertices) {
            if (vi < m_vertices.size()) {
                center.x += m_vertices[vi].position.x;
                center.y += m_vertices[vi].position.y;
                center.z += m_vertices[vi].position.z;
            }
        }
        if (!poly.vertices.empty()) {
            float inv_count = 1.0f / static_cast<float>(poly.vertices.size());
            center.x *= inv_count;
            center.y *= inv_count;
            center.z *= inv_count;
        }
        poly.center = center;

        poly.area = 0;
        if (poly.vertices.size() >= 3) {
            const auto& v0 = m_vertices[poly.vertices[0]].position;
            for (std::size_t i = 2; i < poly.vertices.size(); ++i) {
                const auto& v1 = m_vertices[poly.vertices[i - 1]].position;
                const auto& v2 = m_vertices[poly.vertices[i]].position;
                poly.area += triangle_area(v0, v1, v2);
            }
        }
        m_total_area += poly.area;
    }
}

const NavPolygon* NavMesh::polygon(std::uint32_t index) const {
    return index < m_polygons.size() ? &m_polygons[index] : nullptr;
}

const NavVertex* NavMesh::vertex(std::uint32_t index) const {
    return index < m_vertices.size() ? &m_vertices[index] : nullptr;
}

bool NavMesh::find_nearest_point(const void_math::Vec3& position,
                                void_math::Vec3& out_nearest,
                                std::uint32_t& out_polygon) const {
    if (m_polygons.empty()) return false;

    float best_dist = std::numeric_limits<float>::max();
    bool found = false;

    for (std::uint32_t pi = 0; pi < m_polygons.size(); ++pi) {
        float dist = point_to_polygon_distance(position, pi);
        if (dist < best_dist) {
            best_dist = dist;
            out_polygon = pi;
            found = true;
        }
    }

    if (found) {
        // Project to polygon
        const auto& poly = m_polygons[out_polygon];

        // Check if point is inside polygon
        if (is_point_in_polygon(position, out_polygon)) {
            // Just project Y to the polygon plane
            out_nearest = position;
            // Simple Y projection using polygon center
            out_nearest.y = poly.center.y;
        } else {
            // Find closest point on polygon edges
            float closest_dist = std::numeric_limits<float>::max();
            for (std::size_t i = 0; i < poly.vertices.size(); ++i) {
                std::size_t j = (i + 1) % poly.vertices.size();
                const auto& a = m_vertices[poly.vertices[i]].position;
                const auto& b = m_vertices[poly.vertices[j]].position;

                void_math::Vec3 closest;
                float d = point_to_segment_distance(position, a, b, closest);
                if (d < closest_dist) {
                    closest_dist = d;
                    out_nearest = closest;
                }
            }
        }
    }

    return found;
}

bool NavMesh::find_random_point(void_math::Vec3& out_point,
                               std::uint32_t& out_polygon) const {
    if (m_polygons.empty() || m_total_area <= 0) return false;

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, m_total_area);
    float target = dist(rng);

    float accumulated = 0;
    for (std::uint32_t pi = 0; pi < m_polygons.size(); ++pi) {
        accumulated += m_polygons[pi].area;
        if (accumulated >= target) {
            out_polygon = pi;

            // Random point in polygon (use center for simplicity)
            const auto& poly = m_polygons[pi];
            if (poly.vertices.size() >= 3) {
                // Random point in first triangle
                const auto& a = m_vertices[poly.vertices[0]].position;
                const auto& b = m_vertices[poly.vertices[1]].position;
                const auto& c = m_vertices[poly.vertices[2]].position;

                std::uniform_real_distribution<float> ud(0.0f, 1.0f);
                float r1 = ud(rng);
                float r2 = ud(rng);
                if (r1 + r2 > 1.0f) {
                    r1 = 1.0f - r1;
                    r2 = 1.0f - r2;
                }
                float r3 = 1.0f - r1 - r2;

                out_point.x = a.x * r1 + b.x * r2 + c.x * r3;
                out_point.y = a.y * r1 + b.y * r2 + c.y * r3;
                out_point.z = a.z * r1 + b.z * r2 + c.z * r3;
            } else {
                out_point = poly.center;
            }
            return true;
        }
    }

    return false;
}

bool NavMesh::find_random_point_in_radius(const void_math::Vec3& center,
                                          float radius,
                                          void_math::Vec3& out_point,
                                          std::uint32_t& out_polygon) const {
    // Try multiple times to find a valid point
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318f);
    std::uniform_real_distribution<float> radius_dist(0.0f, 1.0f);

    for (int attempt = 0; attempt < 30; ++attempt) {
        float angle = angle_dist(rng);
        float r = std::sqrt(radius_dist(rng)) * radius;

        void_math::Vec3 test_point;
        test_point.x = center.x + std::cos(angle) * r;
        test_point.y = center.y;
        test_point.z = center.z + std::sin(angle) * r;

        std::int32_t poly_idx = find_polygon_containing(test_point);
        if (poly_idx >= 0) {
            out_point = test_point;
            out_polygon = static_cast<std::uint32_t>(poly_idx);
            return true;
        }
    }

    return false;
}

bool NavMesh::is_point_in_polygon(const void_math::Vec3& point,
                                  std::uint32_t polygon) const {
    if (polygon >= m_polygons.size()) return false;

    const auto& poly = m_polygons[polygon];
    if (poly.vertices.size() < 3) return false;

    // Check each triangle formed with polygon center
    const auto& v0 = m_vertices[poly.vertices[0]].position;
    for (std::size_t i = 2; i < poly.vertices.size(); ++i) {
        const auto& v1 = m_vertices[poly.vertices[i - 1]].position;
        const auto& v2 = m_vertices[poly.vertices[i]].position;

        if (point_in_triangle_2d(point, v0, v1, v2)) {
            return true;
        }
    }

    return false;
}

std::int32_t NavMesh::find_polygon_containing(const void_math::Vec3& point) const {
    for (std::uint32_t pi = 0; pi < m_polygons.size(); ++pi) {
        if (is_point_in_polygon(point, pi)) {
            return static_cast<std::int32_t>(pi);
        }
    }
    return -1;
}

bool NavMesh::raycast(const void_math::Vec3& start,
                     const void_math::Vec3& end,
                     void_math::Vec3& out_hit,
                     std::uint32_t& out_hit_polygon) const {
    // Simple raycast against polygon edges
    float closest_t = std::numeric_limits<float>::max();
    bool hit = false;

    auto dir = vec3_subtract(end, start);
    float ray_length = vec3_length(dir);
    if (ray_length < 1e-6f) return false;

    for (std::uint32_t pi = 0; pi < m_polygons.size(); ++pi) {
        const auto& poly = m_polygons[pi];

        for (std::size_t i = 0; i < poly.vertices.size(); ++i) {
            std::size_t j = (i + 1) % poly.vertices.size();
            const auto& a = m_vertices[poly.vertices[i]].position;
            const auto& b = m_vertices[poly.vertices[j]].position;

            // 2D line-line intersection (ignoring Y)
            float x1 = start.x, z1 = start.z;
            float x2 = end.x, z2 = end.z;
            float x3 = a.x, z3 = a.z;
            float x4 = b.x, z4 = b.z;

            float denom = (x1 - x2) * (z3 - z4) - (z1 - z2) * (x3 - x4);
            if (std::abs(denom) < 1e-6f) continue;

            float t = ((x1 - x3) * (z3 - z4) - (z1 - z3) * (x3 - x4)) / denom;
            float u = -((x1 - x2) * (z1 - z3) - (z1 - z2) * (x1 - x3)) / denom;

            if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
                if (t < closest_t) {
                    closest_t = t;
                    out_hit.x = x1 + t * (x2 - x1);
                    out_hit.z = z1 + t * (z2 - z1);
                    out_hit.y = start.y + t * (end.y - start.y);
                    out_hit_polygon = pi;
                    hit = true;
                }
            }
        }
    }

    return hit;
}

void NavMesh::add_off_mesh_connection(const OffMeshConnection& connection) {
    m_off_mesh_connections.push_back(connection);
}

void NavMesh::remove_off_mesh_connection(std::uint32_t user_id) {
    m_off_mesh_connections.erase(
        std::remove_if(m_off_mesh_connections.begin(), m_off_mesh_connections.end(),
            [user_id](const OffMeshConnection& c) { return c.user_id == user_id; }),
        m_off_mesh_connections.end());
}

void NavMesh::set_area_cost(AreaType area, float cost) {
    m_area_costs[static_cast<std::uint8_t>(area)] = cost;
}

float NavMesh::area_cost(AreaType area) const {
    auto it = m_area_costs.find(static_cast<std::uint8_t>(area));
    return it != m_area_costs.end() ? it->second : 1.0f;
}

void NavMesh::clear() {
    m_vertices.clear();
    m_polygons.clear();
    m_off_mesh_connections.clear();
    m_total_area = 0;
    m_bounds_min = {};
    m_bounds_max = {};
}

std::vector<std::uint8_t> NavMesh::serialize() const {
    std::vector<std::uint8_t> data;

    // Helper to write data
    auto write = [&data](const void* src, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(src);
        data.insert(data.end(), bytes, bytes + size);
    };

    auto write_u32 = [&write](std::uint32_t value) { write(&value, sizeof(value)); };
    auto write_float = [&write](float value) { write(&value, sizeof(value)); };
    auto write_vec3 = [&write_float](const void_math::Vec3& v) {
        write_float(v.x);
        write_float(v.y);
        write_float(v.z);
    };

    // Magic number and version
    write_u32(0x4E41564D); // "NAVM"
    write_u32(1);          // Version

    // Bounds
    write_vec3(m_bounds_min);
    write_vec3(m_bounds_max);

    // Vertices
    write_u32(static_cast<std::uint32_t>(m_vertices.size()));
    for (const auto& v : m_vertices) {
        write_vec3(v.position);
        write_u32(v.index);
    }

    // Polygons
    write_u32(static_cast<std::uint32_t>(m_polygons.size()));
    for (const auto& poly : m_polygons) {
        // Vertices
        write_u32(static_cast<std::uint32_t>(poly.vertices.size()));
        for (auto vi : poly.vertices) {
            write_u32(vi);
        }
        // Neighbors
        write_u32(static_cast<std::uint32_t>(poly.neighbors.size()));
        for (auto ni : poly.neighbors) {
            write_u32(ni);
        }
        // Properties
        write_vec3(poly.center);
        write_float(poly.area);
        write_u32(poly.flags);
        write_float(poly.cost);
    }

    // Off-mesh connections
    write_u32(static_cast<std::uint32_t>(m_off_mesh_connections.size()));
    for (const auto& conn : m_off_mesh_connections) {
        write_vec3(conn.start);
        write_vec3(conn.end);
        write_float(conn.radius);
        write_float(conn.cost);
        write_u32(conn.flags);
        write_u32(conn.bidirectional ? 1u : 0u);
        write_u32(conn.user_id);
    }

    // Area costs
    write_u32(static_cast<std::uint32_t>(m_area_costs.size()));
    for (const auto& [area, cost] : m_area_costs) {
        data.push_back(area);
        write_float(cost);
    }

    // Total area
    write_float(m_total_area);

    return data;
}

bool NavMesh::deserialize(const std::vector<std::uint8_t>& data) {
    if (data.size() < 8) return false; // At least magic + version

    std::size_t offset = 0;

    // Helper to read data
    auto read = [&data, &offset](void* dst, std::size_t size) -> bool {
        if (offset + size > data.size()) return false;
        std::memcpy(dst, data.data() + offset, size);
        offset += size;
        return true;
    };

    auto read_u32 = [&read]() -> std::uint32_t {
        std::uint32_t value = 0;
        read(&value, sizeof(value));
        return value;
    };
    auto read_float = [&read]() -> float {
        float value = 0;
        read(&value, sizeof(value));
        return value;
    };
    auto read_vec3 = [&read_float]() -> void_math::Vec3 {
        return {read_float(), read_float(), read_float()};
    };

    // Magic number and version
    std::uint32_t magic = read_u32();
    if (magic != 0x4E41564D) return false; // "NAVM"

    std::uint32_t version = read_u32();
    if (version != 1) return false;

    // Clear existing data
    clear();

    // Bounds
    m_bounds_min = read_vec3();
    m_bounds_max = read_vec3();

    // Vertices
    std::uint32_t vertex_count = read_u32();
    m_vertices.reserve(vertex_count);
    for (std::uint32_t i = 0; i < vertex_count; ++i) {
        NavVertex v;
        v.position = read_vec3();
        v.index = read_u32();
        m_vertices.push_back(v);
    }

    // Polygons
    std::uint32_t poly_count = read_u32();
    m_polygons.reserve(poly_count);
    for (std::uint32_t i = 0; i < poly_count; ++i) {
        NavPolygon poly;

        // Vertices
        std::uint32_t vert_count = read_u32();
        poly.vertices.reserve(vert_count);
        for (std::uint32_t v = 0; v < vert_count; ++v) {
            poly.vertices.push_back(read_u32());
        }

        // Neighbors
        std::uint32_t neighbor_count = read_u32();
        poly.neighbors.reserve(neighbor_count);
        for (std::uint32_t n = 0; n < neighbor_count; ++n) {
            poly.neighbors.push_back(read_u32());
        }

        // Properties
        poly.center = read_vec3();
        poly.area = read_float();
        poly.flags = read_u32();
        poly.cost = read_float();

        m_polygons.push_back(std::move(poly));
    }

    // Off-mesh connections
    std::uint32_t conn_count = read_u32();
    m_off_mesh_connections.reserve(conn_count);
    for (std::uint32_t i = 0; i < conn_count; ++i) {
        OffMeshConnection conn;
        conn.start = read_vec3();
        conn.end = read_vec3();
        conn.radius = read_float();
        conn.cost = read_float();
        conn.flags = read_u32();
        conn.bidirectional = read_u32() != 0;
        conn.user_id = read_u32();
        m_off_mesh_connections.push_back(conn);
    }

    // Area costs
    std::uint32_t area_count = read_u32();
    for (std::uint32_t i = 0; i < area_count; ++i) {
        if (offset >= data.size()) return false;
        std::uint8_t area = data[offset++];
        float cost = read_float();
        m_area_costs[area] = cost;
    }

    // Total area
    m_total_area = read_float();

    return offset <= data.size();
}

void NavMesh::update_bounds() {
    if (m_vertices.empty()) {
        m_bounds_min = {};
        m_bounds_max = {};
        return;
    }

    m_bounds_min = m_vertices[0].position;
    m_bounds_max = m_vertices[0].position;

    for (const auto& v : m_vertices) {
        m_bounds_min.x = std::min(m_bounds_min.x, v.position.x);
        m_bounds_min.y = std::min(m_bounds_min.y, v.position.y);
        m_bounds_min.z = std::min(m_bounds_min.z, v.position.z);
        m_bounds_max.x = std::max(m_bounds_max.x, v.position.x);
        m_bounds_max.y = std::max(m_bounds_max.y, v.position.y);
        m_bounds_max.z = std::max(m_bounds_max.z, v.position.z);
    }
}

float NavMesh::point_to_polygon_distance(const void_math::Vec3& point,
                                         std::uint32_t polygon_index) const {
    const auto& poly = m_polygons[polygon_index];

    // If inside polygon, distance is Y difference
    if (is_point_in_polygon(point, polygon_index)) {
        return std::abs(point.y - poly.center.y);
    }

    // Otherwise, find closest edge
    float min_dist = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < poly.vertices.size(); ++i) {
        std::size_t j = (i + 1) % poly.vertices.size();
        const auto& a = m_vertices[poly.vertices[i]].position;
        const auto& b = m_vertices[poly.vertices[j]].position;

        void_math::Vec3 closest;
        float d = point_to_segment_distance(point, a, b, closest);
        min_dist = std::min(min_dist, d);
    }

    return min_dist;
}

// =============================================================================
// NavMeshBuilder Implementation
// =============================================================================

NavMeshBuilder::NavMeshBuilder()
    : m_config() {
}

NavMeshBuilder::NavMeshBuilder(const NavMeshBuildConfig& config)
    : m_config(config) {
}

void NavMeshBuilder::add_triangle(const void_math::Vec3& a,
                                  const void_math::Vec3& b,
                                  const void_math::Vec3& c,
                                  AreaType area) {
    InputTriangle tri;
    tri.v[0] = a;
    tri.v[1] = b;
    tri.v[2] = c;
    tri.area = area;
    m_triangles.push_back(tri);
}

void NavMeshBuilder::add_mesh(const std::vector<void_math::Vec3>& vertices,
                              const std::vector<std::uint32_t>& indices,
                              AreaType area) {
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        add_triangle(vertices[indices[i]],
                    vertices[indices[i + 1]],
                    vertices[indices[i + 2]],
                    area);
    }
}

void NavMeshBuilder::add_box_obstacle(const void_math::Vec3& min,
                                      const void_math::Vec3& max) {
    Obstacle obs;
    obs.type = Obstacle::Type::Box;
    obs.min = min;
    obs.max = max;
    m_obstacles.push_back(obs);
}

void NavMeshBuilder::add_cylinder_obstacle(const void_math::Vec3& center,
                                           float radius,
                                           float height) {
    Obstacle obs;
    obs.type = Obstacle::Type::Cylinder;
    obs.min = center;
    obs.radius = radius;
    obs.height = height;
    m_obstacles.push_back(obs);
}

void NavMeshBuilder::mark_area(const void_math::Vec3& /*min*/,
                               const void_math::Vec3& /*max*/,
                               AreaType /*area*/) {
    // Mark triangles in area with specific type
    // Implementation would modify triangle areas
}

void NavMeshBuilder::mark_convex_area(const std::vector<void_math::Vec3>& /*vertices*/,
                                      float /*min_height*/,
                                      float /*max_height*/,
                                      AreaType /*area*/) {
    // Mark triangles in convex volume
}

std::unique_ptr<NavMesh> NavMeshBuilder::build() {
    auto mesh = std::make_unique<NavMesh>();

    if (m_triangles.empty()) {
        return mesh;
    }

    // Simple navmesh building: use input triangles directly
    // In production, would do proper voxelization and mesh simplification

    // Add vertices (deduplicate)
    std::vector<void_math::Vec3> unique_verts;
    auto find_or_add_vertex = [&](const void_math::Vec3& v) -> std::uint32_t {
        const float epsilon = 0.001f;
        for (std::size_t i = 0; i < unique_verts.size(); ++i) {
            if (std::abs(unique_verts[i].x - v.x) < epsilon &&
                std::abs(unique_verts[i].y - v.y) < epsilon &&
                std::abs(unique_verts[i].z - v.z) < epsilon) {
                return static_cast<std::uint32_t>(i);
            }
        }
        unique_verts.push_back(v);
        return static_cast<std::uint32_t>(unique_verts.size() - 1);
    };

    for (const auto& v : unique_verts) {
        mesh->add_vertex(v);
    }

    // Add triangles as polygons
    for (const auto& tri : m_triangles) {
        // Check slope
        auto ab = vec3_subtract(tri.v[1], tri.v[0]);
        auto ac = vec3_subtract(tri.v[2], tri.v[0]);
        auto normal = vec3_normalize(vec3_cross(ab, ac));

        float slope_angle = std::acos(std::abs(normal.y)) * (180.0f / 3.14159f);
        if (slope_angle > m_config.agent_max_slope) {
            continue;  // Skip steep triangles
        }

        std::uint32_t i0 = find_or_add_vertex(tri.v[0]);
        std::uint32_t i1 = find_or_add_vertex(tri.v[1]);
        std::uint32_t i2 = find_or_add_vertex(tri.v[2]);

        // Re-add vertices if mesh is empty
        while (mesh->vertex_count() < unique_verts.size()) {
            mesh->add_vertex(unique_verts[mesh->vertex_count()]);
        }

        mesh->add_polygon({i0, i1, i2}, static_cast<std::uint32_t>(tri.area));
    }

    mesh->build_connectivity();
    mesh->calculate_polygon_data();

    return mesh;
}

// =============================================================================
// NavMeshQuery Implementation
// =============================================================================

NavMeshQuery::NavMeshQuery(const INavMesh* navmesh)
    : m_navmesh(navmesh) {
}

PathResult NavMeshQuery::find_path(const void_math::Vec3& start,
                                   const void_math::Vec3& end,
                                   const NavAgentConfig& /*agent*/) const {
    PathResult result;

    if (!m_navmesh) {
        return result;
    }

    // Find start and end polygons
    void_math::Vec3 start_nearest, end_nearest;
    std::uint32_t start_poly, end_poly;

    if (!m_navmesh->find_nearest_point(start, start_nearest, start_poly) ||
        !m_navmesh->find_nearest_point(end, end_nearest, end_poly)) {
        return result;
    }

    // A* search
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> open_set;
    std::unordered_map<std::uint32_t, float> g_score;
    std::unordered_map<std::uint32_t, std::uint32_t> came_from;

    AStarNode start_node;
    start_node.polygon = start_poly;
    start_node.g_cost = 0;
    start_node.h_cost = heuristic(start_nearest, end_nearest);
    open_set.push(start_node);
    g_score[start_poly] = 0;

    while (!open_set.empty()) {
        AStarNode current = open_set.top();
        open_set.pop();

        if (current.polygon == end_poly) {
            // Reconstruct path
            auto polygon_path = reconstruct_path(came_from, current.polygon);
            string_pull(polygon_path, start_nearest, end_nearest, result);
            result.complete = true;
            return result;
        }

        const NavPolygon* poly = m_navmesh->polygon(current.polygon);
        if (!poly) continue;

        for (std::uint32_t neighbor : poly->neighbors) {
            if (m_filter && !m_filter(neighbor)) continue;

            const NavPolygon* neighbor_poly = m_navmesh->polygon(neighbor);
            if (!neighbor_poly) continue;

            float edge_cost = vec3_distance(poly->center, neighbor_poly->center);
            edge_cost *= neighbor_poly->cost;

            float tentative_g = current.g_cost + edge_cost;

            auto it = g_score.find(neighbor);
            if (it == g_score.end() || tentative_g < it->second) {
                came_from[neighbor] = current.polygon;
                g_score[neighbor] = tentative_g;

                AStarNode next;
                next.polygon = neighbor;
                next.parent = current.polygon;
                next.g_cost = tentative_g;
                next.h_cost = heuristic(neighbor_poly->center, end_nearest);
                open_set.push(next);
            }
        }
    }

    // No path found
    result.partial = true;
    return result;
}

PathResult NavMeshQuery::find_partial_path(const void_math::Vec3& start,
                                           const void_math::Vec3& end,
                                           std::size_t max_nodes,
                                           const NavAgentConfig& agent) const {
    // Same as find_path but with node limit
    PathResult result;
    // Implementation similar to find_path with iteration limit
    (void)max_nodes;
    return find_path(start, end, agent);
}

void NavMeshQuery::smooth_path(PathResult& path) const {
    if (path.points.size() < 3) return;

    std::vector<PathPoint> smoothed;
    smoothed.push_back(path.points.front());

    std::size_t i = 0;
    while (i < path.points.size() - 1) {
        // Find furthest visible point
        std::size_t furthest = i + 1;
        for (std::size_t j = i + 2; j < path.points.size(); ++j) {
            if (line_of_sight(path.points[i].position, path.points[j].position)) {
                furthest = j;
            } else {
                break;
            }
        }
        smoothed.push_back(path.points[furthest]);
        i = furthest;
    }

    path.points = std::move(smoothed);

    // Recalculate distance
    path.total_distance = 0;
    for (std::size_t j = 1; j < path.points.size(); ++j) {
        path.total_distance += vec3_distance(path.points[j - 1].position,
                                             path.points[j].position);
    }
}

bool NavMeshQuery::line_of_sight(const void_math::Vec3& start,
                                const void_math::Vec3& end) const {
    void_math::Vec3 hit;
    std::uint32_t hit_poly;
    return !m_navmesh->raycast(start, end, hit, hit_poly);
}

void_math::Vec3 NavMeshQuery::move_along_surface(const void_math::Vec3& start,
                                                  const void_math::Vec3& target,
                                                  float max_distance,
                                                  std::uint32_t& out_polygon) const {
    auto dir = vec3_subtract(target, start);
    float dist = vec3_length(dir);
    if (dist < 1e-6f) {
        out_polygon = 0;
        return start;
    }

    float move_dist = std::min(dist, max_distance);
    auto move = vec3_scale(vec3_normalize(dir), move_dist);
    auto result = vec3_add(start, move);

    // Clamp to navmesh
    void_math::Vec3 nearest;
    m_navmesh->find_nearest_point(result, nearest, out_polygon);

    return nearest;
}

float NavMeshQuery::heuristic(const void_math::Vec3& from, const void_math::Vec3& to) const {
    return vec3_distance(from, to);
}

std::vector<std::uint32_t> NavMeshQuery::reconstruct_path(
    const std::unordered_map<std::uint32_t, std::uint32_t>& came_from,
    std::uint32_t current) const {
    std::vector<std::uint32_t> path;
    path.push_back(current);

    while (came_from.find(current) != came_from.end()) {
        current = came_from.at(current);
        path.push_back(current);
    }

    std::reverse(path.begin(), path.end());
    return path;
}

void NavMeshQuery::string_pull(const std::vector<std::uint32_t>& polygon_path,
                               const void_math::Vec3& start,
                               const void_math::Vec3& end,
                               PathResult& result) const {
    result.points.clear();

    // Simple string-pulling: use polygon centers
    PathPoint start_pt;
    start_pt.position = start;
    start_pt.polygon_index = polygon_path.empty() ? 0 : polygon_path.front();
    result.points.push_back(start_pt);

    for (std::uint32_t poly_idx : polygon_path) {
        const NavPolygon* poly = m_navmesh->polygon(poly_idx);
        if (poly) {
            PathPoint pt;
            pt.position = poly->center;
            pt.polygon_index = poly_idx;
            result.points.push_back(pt);
        }
    }

    PathPoint end_pt;
    end_pt.position = end;
    end_pt.polygon_index = polygon_path.empty() ? 0 : polygon_path.back();
    result.points.push_back(end_pt);

    // Calculate total distance
    result.total_distance = 0;
    for (std::size_t i = 1; i < result.points.size(); ++i) {
        result.total_distance += vec3_distance(result.points[i - 1].position,
                                               result.points[i].position);
    }
}

// =============================================================================
// NavPath Implementation
// =============================================================================

NavPath::NavPath(PathResult result)
    : m_result(std::move(result)) {
}

float NavPath::progress() const {
    if (m_result.total_distance <= 0) return 1.0f;

    float traveled = 0;
    for (std::size_t i = 1; i <= m_current_point && i < m_result.points.size(); ++i) {
        traveled += vec3_distance(m_result.points[i - 1].position,
                                  m_result.points[i].position);
    }
    traveled += m_distance_along_segment;

    return traveled / m_result.total_distance;
}

bool NavPath::reached_end() const {
    return m_current_point >= m_result.points.size() - 1 &&
           m_distance_along_segment >= 0;
}

void NavPath::advance(float distance) {
    while (distance > 0 && m_current_point < m_result.points.size() - 1) {
        float segment_length = vec3_distance(
            m_result.points[m_current_point].position,
            m_result.points[m_current_point + 1].position);

        float remaining = segment_length - m_distance_along_segment;
        if (distance < remaining) {
            m_distance_along_segment += distance;
            return;
        }

        distance -= remaining;
        m_distance_along_segment = 0;
        m_current_point++;
    }
}

void_math::Vec3 NavPath::current_position() const {
    if (m_result.points.empty()) return {};
    if (m_current_point >= m_result.points.size() - 1) {
        return m_result.points.back().position;
    }

    const auto& a = m_result.points[m_current_point].position;
    const auto& b = m_result.points[m_current_point + 1].position;
    float segment_length = vec3_distance(a, b);
    if (segment_length < 1e-6f) return a;

    float t = m_distance_along_segment / segment_length;
    return vec3_add(a, vec3_scale(vec3_subtract(b, a), t));
}

void_math::Vec3 NavPath::current_target() const {
    if (m_result.points.empty()) return {};
    if (m_current_point >= m_result.points.size() - 1) {
        return m_result.points.back().position;
    }
    return m_result.points[m_current_point + 1].position;
}

void_math::Vec3 NavPath::direction() const {
    if (m_result.points.size() < 2) return {0, 0, 1};
    if (m_current_point >= m_result.points.size() - 1) return {0, 0, 0};

    return vec3_normalize(vec3_subtract(
        m_result.points[m_current_point + 1].position,
        m_result.points[m_current_point].position));
}

float NavPath::remaining_distance() const {
    if (m_result.points.empty()) return 0;

    float remaining = 0;

    // Current segment
    if (m_current_point < m_result.points.size() - 1) {
        float segment_length = vec3_distance(
            m_result.points[m_current_point].position,
            m_result.points[m_current_point + 1].position);
        remaining += segment_length - m_distance_along_segment;
    }

    // Remaining segments
    for (std::size_t i = m_current_point + 2; i < m_result.points.size(); ++i) {
        remaining += vec3_distance(m_result.points[i - 1].position,
                                   m_result.points[i].position);
    }

    return remaining;
}

void NavPath::set_result(PathResult result) {
    m_result = std::move(result);
    m_current_point = 0;
    m_distance_along_segment = 0;
}

void NavPath::clear() {
    m_result.points.clear();
    m_result.total_distance = 0;
    m_result.complete = false;
    m_result.partial = false;
    m_current_point = 0;
    m_distance_along_segment = 0;
}

// =============================================================================
// NavAgent Implementation
// =============================================================================

NavAgent::NavAgent() = default;

NavAgent::NavAgent(const NavAgentConfig& config)
    : m_config(config) {
}

void NavAgent::set_destination(const void_math::Vec3& destination) {
    m_destination = destination;
    m_path_pending = true;
    m_stopped = false;
}

void NavAgent::stop() {
    m_stopped = true;
    m_velocity = {};
}

void NavAgent::resume() {
    m_stopped = false;
}

void NavAgent::update(float dt, NavMeshQuery& query) {
    // Request new path if needed
    if (m_path_pending) {
        auto result = query.find_path(m_position, m_destination, m_config);
        if (result.complete || result.partial) {
            query.smooth_path(result);
            m_path.set_result(std::move(result));
            if (m_on_path_found) {
                m_on_path_found(true);
            }
        } else {
            if (m_on_path_failed) {
                m_on_path_failed(false);
            }
        }
        m_path_pending = false;
    }

    if (m_stopped || !m_path.is_valid()) {
        return;
    }

    // Move along path
    auto target = m_path.current_target();
    auto to_target = vec3_subtract(target, m_position);
    float dist = vec3_length(to_target);

    if (dist < m_stopping_distance) {
        // Advance to next waypoint
        m_path.advance(dist);

        if (m_path.reached_end()) {
            m_velocity = {};
            m_stopped = true;
            if (m_on_reached) {
                m_on_reached();
            }
            return;
        }

        target = m_path.current_target();
        to_target = vec3_subtract(target, m_position);
        dist = vec3_length(to_target);
    }

    if (dist > 1e-6f) {
        auto desired = vec3_scale(vec3_normalize(to_target), m_speed);

        // Accelerate toward desired velocity
        auto accel = vec3_subtract(desired, m_velocity);
        float accel_mag = vec3_length(accel);
        if (accel_mag > m_acceleration * dt) {
            accel = vec3_scale(vec3_normalize(accel), m_acceleration * dt);
        }
        m_velocity = vec3_add(m_velocity, accel);

        // Limit speed
        float speed = vec3_length(m_velocity);
        if (speed > m_speed) {
            m_velocity = vec3_scale(vec3_normalize(m_velocity), m_speed);
        }

        // Move
        m_position = vec3_add(m_position, vec3_scale(m_velocity, dt));
    }
}

// =============================================================================
// NavigationSystem Implementation
// =============================================================================

NavigationSystem::NavigationSystem() = default;
NavigationSystem::~NavigationSystem() = default;

NavMeshId NavigationSystem::add_navmesh(std::unique_ptr<NavMesh> mesh, std::string_view name) {
    NavMeshId id{m_next_navmesh_id++};
    if (!name.empty()) {
        m_navmesh_names[std::string(name)] = id;
    }
    if (!m_default_navmesh) {
        m_default_navmesh = id;
    }
    m_navmeshes[id] = std::move(mesh);
    return id;
}

void NavigationSystem::remove_navmesh(NavMeshId id) {
    m_navmeshes.erase(id);
    for (auto it = m_navmesh_names.begin(); it != m_navmesh_names.end(); ) {
        if (it->second == id) {
            it = m_navmesh_names.erase(it);
        } else {
            ++it;
        }
    }
    if (m_default_navmesh == id) {
        m_default_navmesh = m_navmeshes.empty() ? NavMeshId{} : m_navmeshes.begin()->first;
    }
}

INavMesh* NavigationSystem::get_navmesh(NavMeshId id) {
    auto it = m_navmeshes.find(id);
    return it != m_navmeshes.end() ? it->second.get() : nullptr;
}

const INavMesh* NavigationSystem::get_navmesh(NavMeshId id) const {
    auto it = m_navmeshes.find(id);
    return it != m_navmeshes.end() ? it->second.get() : nullptr;
}

INavMesh* NavigationSystem::find_navmesh(std::string_view name) {
    auto it = m_navmesh_names.find(std::string(name));
    if (it != m_navmesh_names.end()) {
        return get_navmesh(it->second);
    }
    return nullptr;
}

AgentId NavigationSystem::create_agent(const NavAgentConfig& config) {
    AgentId id{m_next_agent_id++};
    m_agents[id] = std::make_unique<NavAgent>(config);
    return id;
}

void NavigationSystem::destroy_agent(AgentId id) {
    m_agents.erase(id);
}

NavAgent* NavigationSystem::get_agent(AgentId id) {
    auto it = m_agents.find(id);
    return it != m_agents.end() ? it->second.get() : nullptr;
}

NavMeshQuery NavigationSystem::create_query(NavMeshId mesh_id) const {
    return NavMeshQuery(get_navmesh(mesh_id));
}

void NavigationSystem::update(float dt) {
    auto* default_mesh = get_navmesh(m_default_navmesh);
    if (!default_mesh) return;

    NavMeshQuery query(default_mesh);

    for (auto& [id, agent] : m_agents) {
        agent->update(dt, query);
    }
}

} // namespace void_ai
