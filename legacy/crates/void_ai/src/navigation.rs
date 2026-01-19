//! Navigation and pathfinding system

use serde::{Deserialize, Serialize};
use std::collections::{BinaryHeap, HashMap, HashSet};

/// A point in the navigation mesh
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct NavPoint {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

impl NavPoint {
    /// Create a new nav point
    pub fn new(x: f32, y: f32, z: f32) -> Self {
        Self { x, y, z }
    }

    /// Create from array
    pub fn from_array(arr: [f32; 3]) -> Self {
        Self {
            x: arr[0],
            y: arr[1],
            z: arr[2],
        }
    }

    /// Convert to array
    pub fn to_array(self) -> [f32; 3] {
        [self.x, self.y, self.z]
    }

    /// Distance to another point
    pub fn distance_to(&self, other: &NavPoint) -> f32 {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        let dz = self.z - other.z;
        (dx * dx + dy * dy + dz * dz).sqrt()
    }

    /// Squared distance (faster for comparisons)
    pub fn distance_squared_to(&self, other: &NavPoint) -> f32 {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        let dz = self.z - other.z;
        dx * dx + dy * dy + dz * dz
    }
}

/// A polygon in the navigation mesh
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NavPolygon {
    /// Vertex indices
    pub vertices: Vec<usize>,
    /// Center point
    pub center: NavPoint,
    /// Neighboring polygon indices
    pub neighbors: Vec<usize>,
    /// Area cost multiplier (higher = harder to traverse)
    pub cost: f32,
    /// Whether this polygon is walkable
    pub walkable: bool,
}

impl NavPolygon {
    /// Create a new polygon
    pub fn new(vertices: Vec<usize>, center: NavPoint) -> Self {
        Self {
            vertices,
            center,
            neighbors: Vec::new(),
            cost: 1.0,
            walkable: true,
        }
    }
}

/// Navigation mesh for pathfinding
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct NavMesh {
    /// Vertices of the mesh
    pub vertices: Vec<NavPoint>,
    /// Polygons of the mesh
    pub polygons: Vec<NavPolygon>,
    /// Grid cell size for spatial queries
    cell_size: f32,
    /// Spatial hash for quick polygon lookup
    #[serde(skip)]
    spatial_hash: HashMap<(i32, i32), Vec<usize>>,
}

impl NavMesh {
    /// Create a new empty nav mesh
    pub fn new() -> Self {
        Self {
            vertices: Vec::new(),
            polygons: Vec::new(),
            cell_size: 5.0,
            spatial_hash: HashMap::new(),
        }
    }

    /// Create a simple grid nav mesh
    pub fn create_grid(width: f32, depth: f32, cell_size: f32) -> Self {
        let mut mesh = Self::new();
        mesh.cell_size = cell_size;

        let cols = (width / cell_size).ceil() as i32;
        let rows = (depth / cell_size).ceil() as i32;

        // Create vertices
        for row in 0..=rows {
            for col in 0..=cols {
                let x = col as f32 * cell_size;
                let z = row as f32 * cell_size;
                mesh.vertices.push(NavPoint::new(x, 0.0, z));
            }
        }

        // Create polygons (quads)
        let stride = (cols + 1) as usize;
        for row in 0..rows {
            for col in 0..cols {
                let base = (row as usize * stride) + col as usize;
                let vertices = vec![base, base + 1, base + stride + 1, base + stride];

                let center_x = (col as f32 + 0.5) * cell_size;
                let center_z = (row as f32 + 0.5) * cell_size;
                let center = NavPoint::new(center_x, 0.0, center_z);

                let polygon = NavPolygon::new(vertices, center);
                mesh.polygons.push(polygon);
            }
        }

        // Set up neighbors
        let poly_stride = cols as usize;
        for row in 0..rows as usize {
            for col in 0..cols as usize {
                let idx = row * poly_stride + col;
                let mut neighbors = Vec::new();

                if col > 0 {
                    neighbors.push(idx - 1);
                }
                if col < poly_stride - 1 {
                    neighbors.push(idx + 1);
                }
                if row > 0 {
                    neighbors.push(idx - poly_stride);
                }
                if row < (rows as usize) - 1 {
                    neighbors.push(idx + poly_stride);
                }

                mesh.polygons[idx].neighbors = neighbors;
            }
        }

        mesh.rebuild_spatial_hash();
        mesh
    }

    /// Rebuild spatial hash for queries
    pub fn rebuild_spatial_hash(&mut self) {
        self.spatial_hash.clear();
        for (idx, poly) in self.polygons.iter().enumerate() {
            let cell_x = (poly.center.x / self.cell_size) as i32;
            let cell_z = (poly.center.z / self.cell_size) as i32;
            self.spatial_hash
                .entry((cell_x, cell_z))
                .or_default()
                .push(idx);
        }
    }

    /// Find the polygon containing a point
    pub fn find_polygon(&self, point: &NavPoint) -> Option<usize> {
        let cell_x = (point.x / self.cell_size) as i32;
        let cell_z = (point.z / self.cell_size) as i32;

        // Check nearby cells
        for dx in -1..=1 {
            for dz in -1..=1 {
                if let Some(indices) = self.spatial_hash.get(&(cell_x + dx, cell_z + dz)) {
                    for &idx in indices {
                        if self.point_in_polygon(point, idx) {
                            return Some(idx);
                        }
                    }
                }
            }
        }

        // Fallback: check all polygons
        for (idx, _) in self.polygons.iter().enumerate() {
            if self.point_in_polygon(point, idx) {
                return Some(idx);
            }
        }

        None
    }

    /// Check if a point is inside a polygon (simple check based on center distance)
    fn point_in_polygon(&self, point: &NavPoint, polygon_idx: usize) -> bool {
        let poly = &self.polygons[polygon_idx];
        let distance = point.distance_to(&poly.center);
        distance < self.cell_size
    }

    /// Find path between two points using A*
    pub fn find_path(&self, start: NavPoint, end: NavPoint) -> Option<NavPath> {
        let start_poly = self.find_polygon(&start)?;
        let end_poly = self.find_polygon(&end)?;

        if start_poly == end_poly {
            return Some(NavPath {
                waypoints: vec![start, end],
                current_index: 0,
            });
        }

        // A* pathfinding
        let path_indices = self.astar(start_poly, end_poly)?;

        // Convert to waypoints
        let mut waypoints = vec![start];
        for &idx in &path_indices {
            waypoints.push(self.polygons[idx].center);
        }
        waypoints.push(end);

        Some(NavPath {
            waypoints,
            current_index: 0,
        })
    }

    /// A* pathfinding on polygon graph
    fn astar(&self, start: usize, goal: usize) -> Option<Vec<usize>> {
        #[derive(Clone, Copy)]
        struct Node {
            idx: usize,
            f_score: f32,
        }

        impl PartialEq for Node {
            fn eq(&self, other: &Self) -> bool {
                self.idx == other.idx
            }
        }

        impl Eq for Node {}

        impl PartialOrd for Node {
            fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
                Some(self.cmp(other))
            }
        }

        impl Ord for Node {
            fn cmp(&self, other: &Self) -> std::cmp::Ordering {
                other
                    .f_score
                    .partial_cmp(&self.f_score)
                    .unwrap_or(std::cmp::Ordering::Equal)
            }
        }

        let mut open_set = BinaryHeap::new();
        let mut came_from: HashMap<usize, usize> = HashMap::new();
        let mut g_score: HashMap<usize, f32> = HashMap::new();
        let mut closed_set: HashSet<usize> = HashSet::new();

        let goal_center = &self.polygons[goal].center;

        g_score.insert(start, 0.0);
        let h = self.polygons[start].center.distance_to(goal_center);
        open_set.push(Node {
            idx: start,
            f_score: h,
        });

        while let Some(current) = open_set.pop() {
            if current.idx == goal {
                // Reconstruct path
                let mut path = vec![goal];
                let mut current_idx = goal;
                while let Some(&prev) = came_from.get(&current_idx) {
                    path.push(prev);
                    current_idx = prev;
                }
                path.reverse();
                return Some(path);
            }

            if closed_set.contains(&current.idx) {
                continue;
            }
            closed_set.insert(current.idx);

            let current_g = *g_score.get(&current.idx).unwrap_or(&f32::MAX);
            let current_poly = &self.polygons[current.idx];

            for &neighbor_idx in &current_poly.neighbors {
                if closed_set.contains(&neighbor_idx) {
                    continue;
                }

                let neighbor_poly = &self.polygons[neighbor_idx];
                if !neighbor_poly.walkable {
                    continue;
                }

                let distance = current_poly.center.distance_to(&neighbor_poly.center);
                let tentative_g = current_g + distance * neighbor_poly.cost;

                let neighbor_g = *g_score.get(&neighbor_idx).unwrap_or(&f32::MAX);
                if tentative_g < neighbor_g {
                    came_from.insert(neighbor_idx, current.idx);
                    g_score.insert(neighbor_idx, tentative_g);

                    let h = neighbor_poly.center.distance_to(goal_center);
                    open_set.push(Node {
                        idx: neighbor_idx,
                        f_score: tentative_g + h,
                    });
                }
            }
        }

        None // No path found
    }

    /// Mark a polygon as unwalkable
    pub fn set_walkable(&mut self, polygon_idx: usize, walkable: bool) {
        if let Some(poly) = self.polygons.get_mut(polygon_idx) {
            poly.walkable = walkable;
        }
    }

    /// Set cost for a polygon
    pub fn set_cost(&mut self, polygon_idx: usize, cost: f32) {
        if let Some(poly) = self.polygons.get_mut(polygon_idx) {
            poly.cost = cost;
        }
    }
}

/// A path through the navigation mesh
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct NavPath {
    /// Waypoints along the path
    pub waypoints: Vec<NavPoint>,
    /// Current waypoint index
    pub current_index: usize,
}

impl NavPath {
    /// Check if path is empty
    pub fn is_empty(&self) -> bool {
        self.waypoints.is_empty()
    }

    /// Check if path is complete
    pub fn is_complete(&self) -> bool {
        self.current_index >= self.waypoints.len()
    }

    /// Get current waypoint
    pub fn current_waypoint(&self) -> Option<&NavPoint> {
        self.waypoints.get(self.current_index)
    }

    /// Get final destination
    pub fn destination(&self) -> Option<&NavPoint> {
        self.waypoints.last()
    }

    /// Advance to next waypoint
    pub fn advance(&mut self) {
        if self.current_index < self.waypoints.len() {
            self.current_index += 1;
        }
    }

    /// Get remaining distance
    pub fn remaining_distance(&self) -> f32 {
        if self.is_complete() {
            return 0.0;
        }

        let mut distance = 0.0;
        for i in self.current_index..self.waypoints.len() - 1 {
            distance += self.waypoints[i].distance_to(&self.waypoints[i + 1]);
        }
        distance
    }

    /// Get total path length
    pub fn total_length(&self) -> f32 {
        let mut length = 0.0;
        for i in 0..self.waypoints.len().saturating_sub(1) {
            length += self.waypoints[i].distance_to(&self.waypoints[i + 1]);
        }
        length
    }
}

/// Navigation agent component
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct NavAgent {
    /// Current position
    pub position: [f32; 3],
    /// Current velocity
    pub velocity: [f32; 3],
    /// Current path
    pub path: Option<NavPath>,
    /// Movement speed
    pub speed: f32,
    /// Arrival threshold
    pub arrival_threshold: f32,
    /// Whether agent is moving
    pub is_moving: bool,
    /// Avoidance radius
    pub avoidance_radius: f32,
}

impl NavAgent {
    /// Create a new navigation agent
    pub fn new(speed: f32) -> Self {
        Self {
            position: [0.0, 0.0, 0.0],
            velocity: [0.0, 0.0, 0.0],
            path: None,
            speed,
            arrival_threshold: 0.5,
            is_moving: false,
            avoidance_radius: 1.0,
        }
    }

    /// Set a new path
    pub fn set_path(&mut self, path: NavPath) {
        self.path = Some(path);
        self.is_moving = true;
    }

    /// Clear the current path
    pub fn clear_path(&mut self) {
        self.path = None;
        self.is_moving = false;
        self.velocity = [0.0, 0.0, 0.0];
    }

    /// Update agent movement
    pub fn update(&mut self, delta_time: f32) {
        if !self.is_moving {
            return;
        }

        let path = match &mut self.path {
            Some(p) => p,
            None => {
                self.is_moving = false;
                return;
            }
        };

        let waypoint = match path.current_waypoint() {
            Some(w) => w,
            None => {
                self.is_moving = false;
                return;
            }
        };

        // Calculate direction to waypoint
        let dx = waypoint.x - self.position[0];
        let dy = waypoint.y - self.position[1];
        let dz = waypoint.z - self.position[2];
        let distance = (dx * dx + dy * dy + dz * dz).sqrt();

        if distance < self.arrival_threshold {
            path.advance();
            if path.is_complete() {
                self.is_moving = false;
                self.velocity = [0.0, 0.0, 0.0];
            }
            return;
        }

        // Move toward waypoint
        let move_distance = self.speed * delta_time;
        let factor = move_distance.min(distance) / distance;

        self.velocity = [dx * factor / delta_time, dy * factor / delta_time, dz * factor / delta_time];
        self.position[0] += dx * factor;
        self.position[1] += dy * factor;
        self.position[2] += dz * factor;
    }

    /// Get direction to current waypoint
    pub fn get_direction(&self) -> Option<[f32; 3]> {
        let path = self.path.as_ref()?;
        let waypoint = path.current_waypoint()?;

        let dx = waypoint.x - self.position[0];
        let dy = waypoint.y - self.position[1];
        let dz = waypoint.z - self.position[2];
        let distance = (dx * dx + dy * dy + dz * dz).sqrt();

        if distance > 0.001 {
            Some([dx / distance, dy / distance, dz / distance])
        } else {
            None
        }
    }

    /// Check if has reached destination
    pub fn has_arrived(&self) -> bool {
        match &self.path {
            Some(path) => path.is_complete(),
            None => true,
        }
    }

    /// Get distance to destination
    pub fn distance_to_destination(&self) -> f32 {
        match &self.path {
            Some(path) => {
                if let Some(dest) = path.destination() {
                    let dx = dest.x - self.position[0];
                    let dy = dest.y - self.position[1];
                    let dz = dest.z - self.position[2];
                    (dx * dx + dy * dy + dz * dz).sqrt()
                } else {
                    0.0
                }
            }
            None => 0.0,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_nav_point() {
        let p1 = NavPoint::new(0.0, 0.0, 0.0);
        let p2 = NavPoint::new(3.0, 4.0, 0.0);

        assert!((p1.distance_to(&p2) - 5.0).abs() < 0.001);
    }

    #[test]
    fn test_nav_mesh_grid() {
        let mesh = NavMesh::create_grid(10.0, 10.0, 5.0);

        // Should have 2x2 = 4 polygons
        assert_eq!(mesh.polygons.len(), 4);
        // Should have 3x3 = 9 vertices
        assert_eq!(mesh.vertices.len(), 9);
    }

    #[test]
    fn test_find_polygon() {
        let mesh = NavMesh::create_grid(10.0, 10.0, 5.0);

        let point = NavPoint::new(2.5, 0.0, 2.5);
        let poly_idx = mesh.find_polygon(&point);
        assert!(poly_idx.is_some());
    }

    #[test]
    fn test_find_path() {
        let mesh = NavMesh::create_grid(20.0, 20.0, 5.0);

        let start = NavPoint::new(2.5, 0.0, 2.5);
        let end = NavPoint::new(17.5, 0.0, 17.5);

        let path = mesh.find_path(start, end);
        assert!(path.is_some());

        let path = path.unwrap();
        assert!(!path.is_empty());
        assert!(path.total_length() > 0.0);
    }

    #[test]
    fn test_path_same_polygon() {
        let mesh = NavMesh::create_grid(10.0, 10.0, 5.0);

        let start = NavPoint::new(1.0, 0.0, 1.0);
        let end = NavPoint::new(2.0, 0.0, 2.0);

        let path = mesh.find_path(start, end);
        assert!(path.is_some());

        let path = path.unwrap();
        assert_eq!(path.waypoints.len(), 2); // Direct path
    }

    #[test]
    fn test_nav_path() {
        let path = NavPath {
            waypoints: vec![
                NavPoint::new(0.0, 0.0, 0.0),
                NavPoint::new(5.0, 0.0, 0.0),
                NavPoint::new(10.0, 0.0, 0.0),
            ],
            current_index: 0,
        };

        assert!(!path.is_empty());
        assert!(!path.is_complete());
        assert_eq!(path.total_length(), 10.0);
    }

    #[test]
    fn test_nav_agent() {
        let mut agent = NavAgent::new(5.0);
        agent.position = [0.0, 0.0, 0.0];

        let path = NavPath {
            waypoints: vec![
                NavPoint::new(0.0, 0.0, 0.0),
                NavPoint::new(10.0, 0.0, 0.0),
            ],
            current_index: 0,
        };

        agent.set_path(path);
        assert!(agent.is_moving);

        // Update for a bit
        for _ in 0..100 {
            agent.update(0.1);
        }

        // Should have moved toward destination
        assert!(agent.position[0] > 0.0);
    }

    #[test]
    fn test_agent_arrival() {
        let mut agent = NavAgent::new(10.0);
        agent.position = [0.0, 0.0, 0.0];
        agent.arrival_threshold = 0.5;

        let path = NavPath {
            waypoints: vec![
                NavPoint::new(0.0, 0.0, 0.0),
                NavPoint::new(1.0, 0.0, 0.0),
            ],
            current_index: 0,
        };

        agent.set_path(path);

        // Should arrive quickly
        for _ in 0..100 {
            agent.update(0.1);
            if agent.has_arrived() {
                break;
            }
        }

        assert!(agent.has_arrived());
    }

    #[test]
    fn test_unwalkable_polygon() {
        let mut mesh = NavMesh::create_grid(15.0, 5.0, 5.0);

        // Block middle polygon
        mesh.set_walkable(1, false);

        let start = NavPoint::new(2.5, 0.0, 2.5);
        let end = NavPoint::new(12.5, 0.0, 2.5);

        // Path should still be found (around the blocked polygon)
        // Note: With this simple grid, if we block the middle,
        // there's no alternative path, so it will fail
        let path = mesh.find_path(start, end);
        assert!(path.is_none()); // No path available
    }

    #[test]
    fn test_polygon_cost() {
        let mut mesh = NavMesh::create_grid(15.0, 10.0, 5.0);

        // Set high cost for middle polygon
        mesh.set_cost(1, 10.0);

        let start = NavPoint::new(2.5, 0.0, 2.5);
        let end = NavPoint::new(12.5, 0.0, 2.5);

        let path = mesh.find_path(start, end);
        assert!(path.is_some());
    }

    #[test]
    fn test_agent_direction() {
        let mut agent = NavAgent::new(5.0);
        agent.position = [0.0, 0.0, 0.0];

        let path = NavPath {
            waypoints: vec![
                NavPoint::new(0.0, 0.0, 0.0),
                NavPoint::new(10.0, 0.0, 0.0),
            ],
            current_index: 0,
        };

        agent.set_path(path);
        agent.path.as_mut().unwrap().advance(); // Move to second waypoint

        let direction = agent.get_direction();
        assert!(direction.is_some());

        let dir = direction.unwrap();
        assert!((dir[0] - 1.0).abs() < 0.001); // Should be pointing right
    }

    #[test]
    fn test_remaining_distance() {
        let path = NavPath {
            waypoints: vec![
                NavPoint::new(0.0, 0.0, 0.0),
                NavPoint::new(5.0, 0.0, 0.0),
                NavPoint::new(10.0, 0.0, 0.0),
            ],
            current_index: 1,
        };

        assert!((path.remaining_distance() - 5.0).abs() < 0.001);
    }
}
