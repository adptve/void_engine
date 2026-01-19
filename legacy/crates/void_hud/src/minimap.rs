//! Minimap system

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Minimap icon type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum MinimapIconType {
    /// Player
    Player,
    /// Enemy
    Enemy,
    /// Ally/friendly NPC
    Ally,
    /// Quest objective
    Objective,
    /// Point of interest
    PointOfInterest,
    /// Vendor/shop
    Vendor,
    /// Waypoint
    Waypoint,
    /// Checkpoint
    Checkpoint,
    /// Item/loot
    Item,
    /// Custom icon
    Custom(u32),
}

/// An icon on the minimap
#[derive(Debug, Clone)]
pub struct MinimapIcon {
    /// Icon type
    pub icon_type: MinimapIconType,
    /// World position
    pub world_position: [f32; 2],
    /// Rotation (for directional icons like player)
    pub rotation: f32,
    /// Color override (None = use default for type)
    pub color: Option<[f32; 4]>,
    /// Scale
    pub scale: f32,
    /// Whether to pulse/animate
    pub pulse: bool,
    /// Custom icon path (for Custom type)
    pub custom_icon: Option<String>,
    /// Label text
    pub label: Option<String>,
    /// Entity ID (for tracking)
    pub entity_id: Option<u64>,
}

impl MinimapIcon {
    /// Create a new icon
    pub fn new(icon_type: MinimapIconType, world_pos: [f32; 2]) -> Self {
        Self {
            icon_type,
            world_position: world_pos,
            rotation: 0.0,
            color: None,
            scale: 1.0,
            pulse: false,
            custom_icon: None,
            label: None,
            entity_id: None,
        }
    }

    /// Set rotation
    pub fn with_rotation(mut self, rotation: f32) -> Self {
        self.rotation = rotation;
        self
    }

    /// Set color
    pub fn with_color(mut self, color: [f32; 4]) -> Self {
        self.color = Some(color);
        self
    }

    /// Set scale
    pub fn with_scale(mut self, scale: f32) -> Self {
        self.scale = scale;
        self
    }

    /// Enable pulsing
    pub fn with_pulse(mut self) -> Self {
        self.pulse = true;
        self
    }

    /// Set label
    pub fn with_label(mut self, label: impl Into<String>) -> Self {
        self.label = Some(label.into());
        self
    }

    /// Set entity ID
    pub fn with_entity(mut self, id: u64) -> Self {
        self.entity_id = Some(id);
        self
    }

    /// Get default color for this icon type
    pub fn default_color(&self) -> [f32; 4] {
        match self.icon_type {
            MinimapIconType::Player => [0.2, 0.6, 1.0, 1.0],      // Blue
            MinimapIconType::Enemy => [1.0, 0.2, 0.2, 1.0],       // Red
            MinimapIconType::Ally => [0.2, 1.0, 0.2, 1.0],        // Green
            MinimapIconType::Objective => [1.0, 0.8, 0.0, 1.0],   // Gold
            MinimapIconType::PointOfInterest => [1.0, 1.0, 1.0, 1.0], // White
            MinimapIconType::Vendor => [0.8, 0.6, 0.2, 1.0],      // Brown
            MinimapIconType::Waypoint => [0.0, 1.0, 1.0, 1.0],    // Cyan
            MinimapIconType::Checkpoint => [0.5, 1.0, 0.5, 1.0],  // Light green
            MinimapIconType::Item => [0.8, 0.8, 0.2, 1.0],        // Yellow
            MinimapIconType::Custom(_) => [1.0, 1.0, 1.0, 1.0],
        }
    }

    /// Get effective color
    pub fn effective_color(&self) -> [f32; 4] {
        self.color.unwrap_or_else(|| self.default_color())
    }
}

/// Minimap component
#[derive(Debug, Clone)]
pub struct Minimap {
    /// Screen position X
    pub x: f32,
    /// Screen position Y
    pub y: f32,
    /// Size (diameter for circular, side for square)
    pub size: f32,
    /// Whether circular (false = square)
    pub circular: bool,
    /// Zoom level (world units visible)
    pub zoom: f32,
    /// Whether to rotate with player
    pub rotate_with_player: bool,
    /// Player position (center of map)
    pub player_position: [f32; 2],
    /// Player rotation
    pub player_rotation: f32,
    /// Background color
    pub background_color: [f32; 4],
    /// Border color
    pub border_color: [f32; 4],
    /// Border width
    pub border_width: f32,
    /// Icons on the map
    pub icons: HashMap<u64, MinimapIcon>,
    /// Next icon ID
    next_icon_id: u64,
    /// Whether visible
    pub visible: bool,
    /// Opacity
    pub opacity: f32,
}

impl Minimap {
    /// Create a new minimap
    pub fn new() -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            size: 200.0,
            circular: true,
            zoom: 100.0,
            rotate_with_player: false,
            player_position: [0.0, 0.0],
            player_rotation: 0.0,
            background_color: [0.1, 0.1, 0.1, 0.7],
            border_color: [0.3, 0.3, 0.3, 1.0],
            border_width: 2.0,
            icons: HashMap::new(),
            next_icon_id: 1,
            visible: true,
            opacity: 1.0,
        }
    }

    /// Set position
    pub fn with_position(mut self, x: f32, y: f32) -> Self {
        self.x = x;
        self.y = y;
        self
    }

    /// Set size
    pub fn with_size(mut self, size: f32) -> Self {
        self.size = size;
        self
    }

    /// Set as square
    pub fn square(mut self) -> Self {
        self.circular = false;
        self
    }

    /// Set zoom
    pub fn with_zoom(mut self, zoom: f32) -> Self {
        self.zoom = zoom.max(10.0);
        self
    }

    /// Enable rotation with player
    pub fn with_rotation(mut self) -> Self {
        self.rotate_with_player = true;
        self
    }

    /// Update player position and rotation
    pub fn update_player(&mut self, position: [f32; 2], rotation: f32) {
        self.player_position = position;
        self.player_rotation = rotation;
    }

    /// Add an icon
    pub fn add_icon(&mut self, icon: MinimapIcon) -> u64 {
        let id = self.next_icon_id;
        self.next_icon_id += 1;
        self.icons.insert(id, icon);
        id
    }

    /// Update icon position
    pub fn update_icon(&mut self, id: u64, position: [f32; 2], rotation: f32) {
        if let Some(icon) = self.icons.get_mut(&id) {
            icon.world_position = position;
            icon.rotation = rotation;
        }
    }

    /// Remove an icon
    pub fn remove_icon(&mut self, id: u64) {
        self.icons.remove(&id);
    }

    /// Clear all icons
    pub fn clear_icons(&mut self) {
        self.icons.clear();
    }

    /// Get icons of a specific type
    pub fn icons_of_type(&self, icon_type: MinimapIconType) -> Vec<&MinimapIcon> {
        self.icons
            .values()
            .filter(|i| i.icon_type == icon_type)
            .collect()
    }

    /// Convert world position to minimap position
    pub fn world_to_map(&self, world_pos: [f32; 2]) -> [f32; 2] {
        let dx = world_pos[0] - self.player_position[0];
        let dy = world_pos[1] - self.player_position[1];

        let (rx, ry) = if self.rotate_with_player {
            let cos = self.player_rotation.cos();
            let sin = self.player_rotation.sin();
            (dx * cos - dy * sin, dx * sin + dy * cos)
        } else {
            (dx, dy)
        };

        let scale = (self.size * 0.5) / self.zoom;
        let map_x = self.x + self.size * 0.5 + rx * scale;
        let map_y = self.y + self.size * 0.5 + ry * scale;

        [map_x, map_y]
    }

    /// Check if a world position is visible on the minimap
    pub fn is_visible(&self, world_pos: [f32; 2]) -> bool {
        let dx = world_pos[0] - self.player_position[0];
        let dy = world_pos[1] - self.player_position[1];
        let dist = (dx * dx + dy * dy).sqrt();
        dist <= self.zoom
    }

    /// Zoom in
    pub fn zoom_in(&mut self, factor: f32) {
        self.zoom = (self.zoom / factor).max(10.0);
    }

    /// Zoom out
    pub fn zoom_out(&mut self, factor: f32) {
        self.zoom *= factor;
    }
}

impl Default for Minimap {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_minimap() {
        let minimap = Minimap::new()
            .with_position(10.0, 10.0)
            .with_size(150.0)
            .with_zoom(50.0);

        assert_eq!(minimap.size, 150.0);
        assert_eq!(minimap.zoom, 50.0);
        assert!(minimap.circular);
    }

    #[test]
    fn test_icons() {
        let mut minimap = Minimap::new();

        let id = minimap.add_icon(MinimapIcon::new(
            MinimapIconType::Enemy,
            [10.0, 20.0],
        ));

        assert!(minimap.icons.contains_key(&id));

        minimap.update_icon(id, [15.0, 25.0], 0.0);
        assert_eq!(minimap.icons.get(&id).unwrap().world_position, [15.0, 25.0]);

        minimap.remove_icon(id);
        assert!(!minimap.icons.contains_key(&id));
    }

    #[test]
    fn test_world_to_map() {
        let mut minimap = Minimap::new()
            .with_size(100.0)
            .with_zoom(100.0);

        minimap.update_player([0.0, 0.0], 0.0);

        // Player at center
        let pos = minimap.world_to_map([0.0, 0.0]);
        assert_eq!(pos, [50.0, 50.0]); // Center of 100x100 map

        // Position at edge of zoom
        let pos = minimap.world_to_map([100.0, 0.0]);
        assert_eq!(pos, [100.0, 50.0]); // Right edge
    }

    #[test]
    fn test_visibility() {
        let mut minimap = Minimap::new().with_zoom(50.0);
        minimap.update_player([0.0, 0.0], 0.0);

        assert!(minimap.is_visible([25.0, 25.0]));
        assert!(!minimap.is_visible([100.0, 100.0]));
    }
}
