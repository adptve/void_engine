/// @file navmesh.hpp
/// @brief Navigation mesh and pathfinding system

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_ai {

// =============================================================================
// Navigation Mesh Interface
// =============================================================================

/// @brief Interface for navigation meshes
class INavMesh {
public:
    virtual ~INavMesh() = default;

    // Polygon access
    virtual std::size_t polygon_count() const = 0;
    virtual const NavPolygon* polygon(std::uint32_t index) const = 0;

    // Vertex access
    virtual std::size_t vertex_count() const = 0;
    virtual const NavVertex* vertex(std::uint32_t index) const = 0;

    // Spatial queries
    virtual bool find_nearest_point(const void_math::Vec3& position,
                                    void_math::Vec3& out_nearest,
                                    std::uint32_t& out_polygon) const = 0;

    virtual bool find_random_point(void_math::Vec3& out_point,
                                   std::uint32_t& out_polygon) const = 0;

    virtual bool find_random_point_in_radius(const void_math::Vec3& center,
                                              float radius,
                                              void_math::Vec3& out_point,
                                              std::uint32_t& out_polygon) const = 0;

    // Point-in-polygon
    virtual bool is_point_in_polygon(const void_math::Vec3& point,
                                      std::uint32_t polygon) const = 0;

    virtual std::int32_t find_polygon_containing(const void_math::Vec3& point) const = 0;

    // Line-of-sight
    virtual bool raycast(const void_math::Vec3& start,
                         const void_math::Vec3& end,
                         void_math::Vec3& out_hit,
                         std::uint32_t& out_hit_polygon) const = 0;

    // Off-mesh connections
    virtual void add_off_mesh_connection(const OffMeshConnection& connection) = 0;
    virtual void remove_off_mesh_connection(std::uint32_t user_id) = 0;

    // Area queries
    virtual void set_area_cost(AreaType area, float cost) = 0;
    virtual float area_cost(AreaType area) const = 0;

    // Metadata
    virtual void_math::Vec3 bounds_min() const = 0;
    virtual void_math::Vec3 bounds_max() const = 0;
};

// =============================================================================
// Navigation Mesh Implementation
// =============================================================================

/// @brief Navigation mesh implementation
class NavMesh : public INavMesh {
public:
    NavMesh();
    ~NavMesh() override = default;

    // Construction
    void add_vertex(const void_math::Vec3& position);
    void add_polygon(const std::vector<std::uint32_t>& vertices, std::uint32_t flags = 0);
    void build_connectivity();
    void calculate_polygon_data();

    // INavMesh interface
    std::size_t polygon_count() const override { return m_polygons.size(); }
    const NavPolygon* polygon(std::uint32_t index) const override;

    std::size_t vertex_count() const override { return m_vertices.size(); }
    const NavVertex* vertex(std::uint32_t index) const override;

    bool find_nearest_point(const void_math::Vec3& position,
                           void_math::Vec3& out_nearest,
                           std::uint32_t& out_polygon) const override;

    bool find_random_point(void_math::Vec3& out_point,
                          std::uint32_t& out_polygon) const override;

    bool find_random_point_in_radius(const void_math::Vec3& center,
                                     float radius,
                                     void_math::Vec3& out_point,
                                     std::uint32_t& out_polygon) const override;

    bool is_point_in_polygon(const void_math::Vec3& point,
                             std::uint32_t polygon) const override;

    std::int32_t find_polygon_containing(const void_math::Vec3& point) const override;

    bool raycast(const void_math::Vec3& start,
                const void_math::Vec3& end,
                void_math::Vec3& out_hit,
                std::uint32_t& out_hit_polygon) const override;

    void add_off_mesh_connection(const OffMeshConnection& connection) override;
    void remove_off_mesh_connection(std::uint32_t user_id) override;

    void set_area_cost(AreaType area, float cost) override;
    float area_cost(AreaType area) const override;

    void_math::Vec3 bounds_min() const override { return m_bounds_min; }
    void_math::Vec3 bounds_max() const override { return m_bounds_max; }

    // Serialization
    void clear();
    std::vector<std::uint8_t> serialize() const;
    bool deserialize(const std::vector<std::uint8_t>& data);

private:
    void update_bounds();
    float point_to_polygon_distance(const void_math::Vec3& point,
                                    std::uint32_t polygon_index) const;

    std::vector<NavVertex> m_vertices;
    std::vector<NavPolygon> m_polygons;
    std::vector<OffMeshConnection> m_off_mesh_connections;
    std::unordered_map<std::uint8_t, float> m_area_costs;
    void_math::Vec3 m_bounds_min{};
    void_math::Vec3 m_bounds_max{};
    float m_total_area{0};
};

// =============================================================================
// NavMesh Builder
// =============================================================================

/// @brief Builder for navigation meshes from geometry
class NavMeshBuilder {
public:
    NavMeshBuilder();
    explicit NavMeshBuilder(const NavMeshBuildConfig& config);

    // Input geometry
    void add_triangle(const void_math::Vec3& a,
                      const void_math::Vec3& b,
                      const void_math::Vec3& c,
                      AreaType area = AreaType::Ground);

    void add_mesh(const std::vector<void_math::Vec3>& vertices,
                  const std::vector<std::uint32_t>& indices,
                  AreaType area = AreaType::Ground);

    void add_box_obstacle(const void_math::Vec3& min,
                          const void_math::Vec3& max);

    void add_cylinder_obstacle(const void_math::Vec3& center,
                               float radius,
                               float height);

    // Area marking
    void mark_area(const void_math::Vec3& min,
                   const void_math::Vec3& max,
                   AreaType area);

    void mark_convex_area(const std::vector<void_math::Vec3>& vertices,
                          float min_height,
                          float max_height,
                          AreaType area);

    // Build
    std::unique_ptr<NavMesh> build();

    // Configuration
    void set_config(const NavMeshBuildConfig& config) { m_config = config; }
    const NavMeshBuildConfig& config() const { return m_config; }

private:
    struct InputTriangle {
        void_math::Vec3 v[3];
        AreaType area;
    };

    struct Obstacle {
        enum class Type { Box, Cylinder } type;
        void_math::Vec3 min;
        void_math::Vec3 max;
        float radius{0};
        float height{0};
    };

    NavMeshBuildConfig m_config;
    std::vector<InputTriangle> m_triangles;
    std::vector<Obstacle> m_obstacles;

    // Build phases
    void rasterize_triangles(std::vector<std::vector<float>>& heightfield);
    void filter_walkable(std::vector<std::vector<float>>& heightfield);
    void build_regions(std::vector<std::vector<std::uint32_t>>& regions);
    void build_contours(const std::vector<std::vector<std::uint32_t>>& regions,
                        std::vector<std::vector<void_math::Vec3>>& contours);
    void triangulate_contours(const std::vector<std::vector<void_math::Vec3>>& contours,
                              NavMesh& mesh);
};

// =============================================================================
// Navigation Query
// =============================================================================

/// @brief A* pathfinding query on navigation mesh
class NavMeshQuery {
public:
    explicit NavMeshQuery(const INavMesh* navmesh);

    /// @brief Find path between two points
    PathResult find_path(const void_math::Vec3& start,
                         const void_math::Vec3& end,
                         const NavAgentConfig& agent = NavAgentConfig{}) const;

    /// @brief Find partial path (useful for very long paths)
    PathResult find_partial_path(const void_math::Vec3& start,
                                  const void_math::Vec3& end,
                                  std::size_t max_nodes,
                                  const NavAgentConfig& agent = NavAgentConfig{}) const;

    /// @brief Smooth path using string-pulling algorithm
    void smooth_path(PathResult& path) const;

    /// @brief Check if two points can see each other on navmesh
    bool line_of_sight(const void_math::Vec3& start,
                       const void_math::Vec3& end) const;

    /// @brief Move along the navmesh surface
    void_math::Vec3 move_along_surface(const void_math::Vec3& start,
                                       const void_math::Vec3& target,
                                       float max_distance,
                                       std::uint32_t& out_polygon) const;

    // Query filters
    using PolygonFilter = std::function<bool(std::uint32_t polygon_index)>;
    void set_filter(PolygonFilter filter) { m_filter = std::move(filter); }

private:
    struct AStarNode {
        std::uint32_t polygon{0};
        std::uint32_t parent{0};
        float g_cost{0};
        float h_cost{0};
        float f_cost() const { return g_cost + h_cost; }

        bool operator>(const AStarNode& other) const {
            return f_cost() > other.f_cost();
        }
    };

    const INavMesh* m_navmesh;
    PolygonFilter m_filter;

    float heuristic(const void_math::Vec3& from, const void_math::Vec3& to) const;
    std::vector<std::uint32_t> reconstruct_path(
        const std::unordered_map<std::uint32_t, std::uint32_t>& came_from,
        std::uint32_t current) const;
    void string_pull(const std::vector<std::uint32_t>& polygon_path,
                    const void_math::Vec3& start,
                    const void_math::Vec3& end,
                    PathResult& result) const;
};

// =============================================================================
// Navigation Path
// =============================================================================

/// @brief A navigation path with progress tracking
class NavPath {
public:
    NavPath() = default;
    explicit NavPath(PathResult result);

    // Path validity
    bool is_valid() const { return !m_result.points.empty(); }
    bool is_complete() const { return m_result.complete; }
    bool is_partial() const { return m_result.partial; }

    // Progress
    float progress() const;
    bool reached_end() const;
    void advance(float distance);

    // Current state
    void_math::Vec3 current_position() const;
    void_math::Vec3 current_target() const;
    void_math::Vec3 direction() const;
    float remaining_distance() const;

    // Path points
    const std::vector<PathPoint>& points() const { return m_result.points; }
    float total_distance() const { return m_result.total_distance; }

    // Modification
    void set_result(PathResult result);
    void clear();

private:
    PathResult m_result;
    std::size_t m_current_point{0};
    float m_distance_along_segment{0};
};

// =============================================================================
// Navigation Agent
// =============================================================================

/// @brief Agent that uses navigation mesh for movement
class NavAgent {
public:
    NavAgent();
    explicit NavAgent(const NavAgentConfig& config);

    // Destination
    void set_destination(const void_math::Vec3& destination);
    void stop();
    void resume();

    // Update
    void update(float dt, NavMeshQuery& query);

    // State
    bool has_path() const { return m_path.is_valid(); }
    bool reached_destination() const { return m_path.reached_end(); }
    bool is_stopped() const { return m_stopped; }

    // Position and movement
    void set_position(const void_math::Vec3& position) { m_position = position; }
    const void_math::Vec3& position() const { return m_position; }
    const void_math::Vec3& velocity() const { return m_velocity; }
    const void_math::Vec3& destination() const { return m_destination; }

    // Configuration
    void set_config(const NavAgentConfig& config) { m_config = config; }
    const NavAgentConfig& config() const { return m_config; }

    void set_speed(float speed) { m_speed = speed; }
    float speed() const { return m_speed; }

    void set_acceleration(float accel) { m_acceleration = accel; }
    float acceleration() const { return m_acceleration; }

    void set_angular_speed(float speed) { m_angular_speed = speed; }
    float angular_speed() const { return m_angular_speed; }

    void set_stopping_distance(float dist) { m_stopping_distance = dist; }
    float stopping_distance() const { return m_stopping_distance; }

    // Path access
    const NavPath& path() const { return m_path; }

    // Events
    using PathCallback = std::function<void(bool success)>;
    void on_path_found(PathCallback callback) { m_on_path_found = std::move(callback); }
    void on_path_failed(PathCallback callback) { m_on_path_failed = std::move(callback); }
    void on_destination_reached(std::function<void()> callback) { m_on_reached = std::move(callback); }

private:
    NavAgentConfig m_config;
    void_math::Vec3 m_position{};
    void_math::Vec3 m_velocity{};
    void_math::Vec3 m_destination{};
    NavPath m_path;

    float m_speed{5.0f};
    float m_acceleration{10.0f};
    float m_angular_speed{360.0f};
    float m_stopping_distance{0.1f};
    bool m_stopped{true};
    bool m_path_pending{false};

    PathCallback m_on_path_found;
    PathCallback m_on_path_failed;
    std::function<void()> m_on_reached;
};

// =============================================================================
// Navigation System
// =============================================================================

/// @brief High-level navigation system
class NavigationSystem {
public:
    NavigationSystem();
    ~NavigationSystem();

    // NavMesh management
    NavMeshId add_navmesh(std::unique_ptr<NavMesh> mesh, std::string_view name = "");
    void remove_navmesh(NavMeshId id);
    INavMesh* get_navmesh(NavMeshId id);
    const INavMesh* get_navmesh(NavMeshId id) const;
    INavMesh* find_navmesh(std::string_view name);

    // Agent management
    AgentId create_agent(const NavAgentConfig& config = NavAgentConfig{});
    void destroy_agent(AgentId id);
    NavAgent* get_agent(AgentId id);

    // Queries
    NavMeshQuery create_query(NavMeshId mesh_id) const;

    // Update
    void update(float dt);

    // Statistics
    std::size_t navmesh_count() const { return m_navmeshes.size(); }
    std::size_t agent_count() const { return m_agents.size(); }

private:
    std::unordered_map<NavMeshId, std::unique_ptr<NavMesh>> m_navmeshes;
    std::unordered_map<std::string, NavMeshId> m_navmesh_names;
    std::unordered_map<AgentId, std::unique_ptr<NavAgent>> m_agents;
    std::uint32_t m_next_navmesh_id{1};
    std::uint32_t m_next_agent_id{1};
    NavMeshId m_default_navmesh{};
};

} // namespace void_ai
