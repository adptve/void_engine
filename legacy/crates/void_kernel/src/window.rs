//! App Windowing System
//!
//! Manages app windows similar to a desktop OS (Windows, macOS).
//! Apps can be:
//! - Fullscreen (covers entire viewport)
//! - Windowed (positioned, sized, movable)
//! - Minimized (hidden but running)
//!
//! ## Window Stack (Z-Order)
//!
//! ```text
//! ┌─────────────────────────────────────────────────────────────────┐
//! │                        VIEWPORT                                  │
//! │  ┌─────────────────────────────────────────────────────────────┐│
//! │  │ BASE LAYER                                                   ││
//! │  │                                                              ││
//! │  │    ┌──────────────────┐                                     ││
//! │  │    │ APP A (z: 0)     │    ┌─────────────┐                  ││
//! │  │    │ windowed         │    │ APP B (z:1) │                  ││
//! │  │    │                  │    │ FOCUSED     │                  ││
//! │  │    └──────────────────┘    └─────────────┘                  ││
//! │  │                                                              ││
//! │  └─────────────────────────────────────────────────────────────┘│
//! └─────────────────────────────────────────────────────────────────┘
//! ```
//!
//! New apps spawn at the highest z-order (on top).
//! Clicking/focusing an app brings it to the front.

use std::collections::HashMap;
use std::sync::atomic::{AtomicU32, Ordering};

use crate::overlay::OverlayAppId;

/// Window mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WindowMode {
    /// App covers the entire viewport
    Fullscreen,
    /// App has position and size, can be moved/resized
    Windowed,
    /// App is hidden but still running
    Minimized,
}

/// Window anchor point for positioning
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum WindowAnchor {
    /// Position from top-left corner
    #[default]
    TopLeft,
    /// Position from top-right corner
    TopRight,
    /// Position from bottom-left corner
    BottomLeft,
    /// Position from bottom-right corner
    BottomRight,
    /// Centered in viewport
    Center,
}

/// Window position (in viewport coordinates, pixels or normalized 0-1)
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct WindowPosition {
    /// X position
    pub x: f32,
    /// Y position
    pub y: f32,
    /// Anchor point
    pub anchor: WindowAnchor,
    /// Use normalized coordinates (0-1) instead of pixels
    pub normalized: bool,
}

impl Default for WindowPosition {
    fn default() -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            anchor: WindowAnchor::TopLeft,
            normalized: false,
        }
    }
}

impl WindowPosition {
    /// Create a pixel-based position
    pub fn pixels(x: f32, y: f32) -> Self {
        Self {
            x,
            y,
            anchor: WindowAnchor::TopLeft,
            normalized: false,
        }
    }

    /// Create a normalized position (0-1)
    pub fn normalized(x: f32, y: f32) -> Self {
        Self {
            x,
            y,
            anchor: WindowAnchor::TopLeft,
            normalized: true,
        }
    }

    /// Set anchor point
    pub fn with_anchor(mut self, anchor: WindowAnchor) -> Self {
        self.anchor = anchor;
        self
    }

    /// Center in viewport
    pub fn centered() -> Self {
        Self {
            x: 0.5,
            y: 0.5,
            anchor: WindowAnchor::Center,
            normalized: true,
        }
    }
}

/// Window size
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct WindowSize {
    /// Width
    pub width: f32,
    /// Height
    pub height: f32,
    /// Use normalized coordinates (0-1) instead of pixels
    pub normalized: bool,
}

impl Default for WindowSize {
    fn default() -> Self {
        Self {
            width: 400.0,
            height: 300.0,
            normalized: false,
        }
    }
}

impl WindowSize {
    /// Create a pixel-based size
    pub fn pixels(width: f32, height: f32) -> Self {
        Self {
            width,
            height,
            normalized: false,
        }
    }

    /// Create a normalized size (0-1 of viewport)
    pub fn normalized(width: f32, height: f32) -> Self {
        Self {
            width,
            height,
            normalized: true,
        }
    }

    /// Fill the entire viewport
    pub fn fullscreen() -> Self {
        Self {
            width: 1.0,
            height: 1.0,
            normalized: true,
        }
    }
}

/// Window constraints
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct WindowConstraints {
    /// Minimum width
    pub min_width: f32,
    /// Minimum height
    pub min_height: f32,
    /// Maximum width (None = no limit)
    pub max_width: Option<f32>,
    /// Maximum height (None = no limit)
    pub max_height: Option<f32>,
    /// Allow resize
    pub resizable: bool,
    /// Allow move
    pub movable: bool,
    /// Allow minimize
    pub minimizable: bool,
    /// Allow fullscreen toggle
    pub fullscreenable: bool,
}

impl Default for WindowConstraints {
    fn default() -> Self {
        Self {
            min_width: 100.0,
            min_height: 100.0,
            max_width: None,
            max_height: None,
            resizable: true,
            movable: true,
            minimizable: true,
            fullscreenable: true,
        }
    }
}

/// An app window
#[derive(Debug, Clone)]
pub struct AppWindow {
    /// Associated app ID
    pub app_id: OverlayAppId,
    /// Window title (displayed in title bar if any)
    pub title: String,
    /// Current mode
    pub mode: WindowMode,
    /// Position (when windowed)
    pub position: WindowPosition,
    /// Size (when windowed)
    pub size: WindowSize,
    /// Z-order (higher = on top)
    pub z_order: u32,
    /// Window constraints
    pub constraints: WindowConstraints,
    /// Is this window focused?
    pub focused: bool,
    /// Is window visible?
    pub visible: bool,
    /// Opacity (0-1)
    pub opacity: f32,
    /// Stored position before fullscreen (to restore)
    stored_position: Option<WindowPosition>,
    /// Stored size before fullscreen (to restore)
    stored_size: Option<WindowSize>,
}

impl AppWindow {
    /// Create a new window for an app
    pub fn new(app_id: OverlayAppId, title: impl Into<String>) -> Self {
        static Z_COUNTER: AtomicU32 = AtomicU32::new(1);

        Self {
            app_id,
            title: title.into(),
            mode: WindowMode::Windowed,
            position: WindowPosition::centered(),
            size: WindowSize::default(),
            z_order: Z_COUNTER.fetch_add(1, Ordering::Relaxed),
            constraints: WindowConstraints::default(),
            focused: false,
            visible: true,
            opacity: 1.0,
            stored_position: None,
            stored_size: None,
        }
    }

    /// Create a fullscreen window
    pub fn fullscreen(app_id: OverlayAppId, title: impl Into<String>) -> Self {
        let mut window = Self::new(app_id, title);
        window.mode = WindowMode::Fullscreen;
        window.size = WindowSize::fullscreen();
        window
    }

    /// Set position
    pub fn set_position(&mut self, position: WindowPosition) {
        if self.constraints.movable && self.mode == WindowMode::Windowed {
            self.position = position;
        }
    }

    /// Set size
    pub fn set_size(&mut self, size: WindowSize) {
        if self.constraints.resizable && self.mode == WindowMode::Windowed {
            // Apply constraints
            let mut new_size = size;
            new_size.width = new_size.width.max(self.constraints.min_width);
            new_size.height = new_size.height.max(self.constraints.min_height);

            if let Some(max_w) = self.constraints.max_width {
                new_size.width = new_size.width.min(max_w);
            }
            if let Some(max_h) = self.constraints.max_height {
                new_size.height = new_size.height.min(max_h);
            }

            self.size = new_size;
        }
    }

    /// Toggle fullscreen
    pub fn toggle_fullscreen(&mut self) {
        if !self.constraints.fullscreenable {
            return;
        }

        match self.mode {
            WindowMode::Fullscreen => {
                // Restore windowed mode
                self.mode = WindowMode::Windowed;
                if let Some(pos) = self.stored_position.take() {
                    self.position = pos;
                }
                if let Some(size) = self.stored_size.take() {
                    self.size = size;
                }
            }
            WindowMode::Windowed => {
                // Go fullscreen, store current state
                self.stored_position = Some(self.position);
                self.stored_size = Some(self.size);
                self.mode = WindowMode::Fullscreen;
                self.size = WindowSize::fullscreen();
            }
            WindowMode::Minimized => {
                // Unminimize to fullscreen
                self.mode = WindowMode::Fullscreen;
                self.size = WindowSize::fullscreen();
            }
        }
    }

    /// Minimize the window
    pub fn minimize(&mut self) {
        if self.constraints.minimizable {
            self.mode = WindowMode::Minimized;
            self.focused = false;
        }
    }

    /// Restore from minimized
    pub fn restore(&mut self) {
        if self.mode == WindowMode::Minimized {
            self.mode = WindowMode::Windowed;
            if let Some(pos) = self.stored_position.take() {
                self.position = pos;
            }
            if let Some(size) = self.stored_size.take() {
                self.size = size;
            }
        }
    }

    /// Check if window is minimized
    pub fn is_minimized(&self) -> bool {
        self.mode == WindowMode::Minimized
    }

    /// Check if window is fullscreen
    pub fn is_fullscreen(&self) -> bool {
        self.mode == WindowMode::Fullscreen
    }

    /// Calculate the actual pixel rect given viewport size
    pub fn calculate_rect(&self, viewport_width: f32, viewport_height: f32) -> WindowRect {
        if self.mode == WindowMode::Fullscreen {
            return WindowRect {
                x: 0.0,
                y: 0.0,
                width: viewport_width,
                height: viewport_height,
            };
        }

        // Calculate size
        let width = if self.size.normalized {
            self.size.width * viewport_width
        } else {
            self.size.width
        };
        let height = if self.size.normalized {
            self.size.height * viewport_height
        } else {
            self.size.height
        };

        // Calculate position
        let base_x = if self.position.normalized {
            self.position.x * viewport_width
        } else {
            self.position.x
        };
        let base_y = if self.position.normalized {
            self.position.y * viewport_height
        } else {
            self.position.y
        };

        // Apply anchor
        let (x, y) = match self.position.anchor {
            WindowAnchor::TopLeft => (base_x, base_y),
            WindowAnchor::TopRight => (viewport_width - base_x - width, base_y),
            WindowAnchor::BottomLeft => (base_x, viewport_height - base_y - height),
            WindowAnchor::BottomRight => (viewport_width - base_x - width, viewport_height - base_y - height),
            WindowAnchor::Center => (base_x - width / 2.0, base_y - height / 2.0),
        };

        WindowRect { x, y, width, height }
    }
}

/// Calculated window rectangle in pixels
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct WindowRect {
    /// X position (pixels from left)
    pub x: f32,
    /// Y position (pixels from top)
    pub y: f32,
    /// Width in pixels
    pub width: f32,
    /// Height in pixels
    pub height: f32,
}

impl WindowRect {
    /// Check if a point is inside this rect
    pub fn contains(&self, x: f32, y: f32) -> bool {
        x >= self.x && x < self.x + self.width && y >= self.y && y < self.y + self.height
    }
}

/// Manages all app windows
pub struct WindowManager {
    /// All windows by app ID
    windows: HashMap<OverlayAppId, AppWindow>,
    /// Currently focused window
    focused: Option<OverlayAppId>,
    /// Next z-order value
    next_z: u32,
    /// Viewport size
    viewport_width: f32,
    viewport_height: f32,
}

impl WindowManager {
    /// Create a new window manager
    pub fn new(viewport_width: f32, viewport_height: f32) -> Self {
        Self {
            windows: HashMap::new(),
            focused: None,
            next_z: 1,
            viewport_width,
            viewport_height,
        }
    }

    /// Update viewport size
    pub fn set_viewport(&mut self, width: f32, height: f32) {
        self.viewport_width = width;
        self.viewport_height = height;
    }

    /// Get viewport dimensions
    pub fn viewport(&self) -> (f32, f32) {
        (self.viewport_width, self.viewport_height)
    }

    /// Register a new window for an app
    pub fn create_window(&mut self, app_id: OverlayAppId, title: impl Into<String>) -> &mut AppWindow {
        let mut window = AppWindow::new(app_id, title);
        window.z_order = self.next_z;
        self.next_z += 1;

        // New window gets focus
        self.set_focus(app_id);

        self.windows.insert(app_id, window);
        self.windows.get_mut(&app_id).unwrap()
    }

    /// Create a fullscreen window
    pub fn create_fullscreen_window(&mut self, app_id: OverlayAppId, title: impl Into<String>) -> &mut AppWindow {
        let mut window = AppWindow::fullscreen(app_id, title);
        window.z_order = self.next_z;
        self.next_z += 1;

        // New window gets focus
        self.set_focus(app_id);

        self.windows.insert(app_id, window);
        self.windows.get_mut(&app_id).unwrap()
    }

    /// Remove a window
    pub fn destroy_window(&mut self, app_id: OverlayAppId) {
        self.windows.remove(&app_id);
        if self.focused == Some(app_id) {
            // Focus next highest window
            self.focused = self.top_window();
            if let Some(id) = self.focused {
                if let Some(w) = self.windows.get_mut(&id) {
                    w.focused = true;
                }
            }
        }
    }

    /// Get a window
    pub fn get(&self, app_id: OverlayAppId) -> Option<&AppWindow> {
        self.windows.get(&app_id)
    }

    /// Get a mutable window
    pub fn get_mut(&mut self, app_id: OverlayAppId) -> Option<&mut AppWindow> {
        self.windows.get_mut(&app_id)
    }

    /// Bring window to front and focus it
    pub fn focus(&mut self, app_id: OverlayAppId) {
        if let Some(window) = self.windows.get_mut(&app_id) {
            if window.mode == WindowMode::Minimized {
                window.restore();
            }

            // Bring to front
            window.z_order = self.next_z;
            self.next_z += 1;

            self.set_focus(app_id);
        }
    }

    /// Set focus to a window (internal)
    fn set_focus(&mut self, app_id: OverlayAppId) {
        // Unfocus current
        if let Some(old_id) = self.focused {
            if let Some(old_window) = self.windows.get_mut(&old_id) {
                old_window.focused = false;
            }
        }

        // Focus new
        if let Some(window) = self.windows.get_mut(&app_id) {
            window.focused = true;
            self.focused = Some(app_id);
        }
    }

    /// Get currently focused window
    pub fn focused(&self) -> Option<OverlayAppId> {
        self.focused
    }

    /// Get the topmost (highest z-order) visible window
    pub fn top_window(&self) -> Option<OverlayAppId> {
        self.windows
            .values()
            .filter(|w| w.visible && w.mode != WindowMode::Minimized)
            .max_by_key(|w| w.z_order)
            .map(|w| w.app_id)
    }

    /// Get windows sorted by z-order (bottom to top)
    pub fn sorted_windows(&self) -> Vec<&AppWindow> {
        let mut windows: Vec<_> = self.windows.values().collect();
        windows.sort_by_key(|w| w.z_order);
        windows
    }

    /// Get visible windows sorted by z-order (for rendering)
    pub fn visible_windows(&self) -> Vec<&AppWindow> {
        let mut windows: Vec<_> = self.windows
            .values()
            .filter(|w| w.visible && w.mode != WindowMode::Minimized)
            .collect();
        windows.sort_by_key(|w| w.z_order);
        windows
    }

    /// Hit test - find which window is at a point (topmost first)
    pub fn window_at(&self, x: f32, y: f32) -> Option<OverlayAppId> {
        let mut visible: Vec<_> = self.windows
            .values()
            .filter(|w| w.visible && w.mode != WindowMode::Minimized)
            .collect();

        // Sort by z-order descending (topmost first)
        visible.sort_by_key(|w| std::cmp::Reverse(w.z_order));

        for window in visible {
            let rect = window.calculate_rect(self.viewport_width, self.viewport_height);
            if rect.contains(x, y) {
                return Some(window.app_id);
            }
        }

        None
    }

    /// Handle click at position - focuses the clicked window
    pub fn handle_click(&mut self, x: f32, y: f32) -> Option<OverlayAppId> {
        if let Some(app_id) = self.window_at(x, y) {
            self.focus(app_id);
            Some(app_id)
        } else {
            None
        }
    }

    /// Move a window by delta
    pub fn move_window(&mut self, app_id: OverlayAppId, dx: f32, dy: f32) {
        if let Some(window) = self.windows.get_mut(&app_id) {
            if window.constraints.movable && window.mode == WindowMode::Windowed {
                let mut pos = window.position;
                if pos.normalized {
                    pos.x += dx / self.viewport_width;
                    pos.y += dy / self.viewport_height;
                } else {
                    pos.x += dx;
                    pos.y += dy;
                }
                window.position = pos;
            }
        }
    }

    /// Resize a window by delta
    pub fn resize_window(&mut self, app_id: OverlayAppId, dw: f32, dh: f32) {
        if let Some(window) = self.windows.get_mut(&app_id) {
            if window.constraints.resizable && window.mode == WindowMode::Windowed {
                let mut size = window.size;
                if size.normalized {
                    size.width += dw / self.viewport_width;
                    size.height += dh / self.viewport_height;
                } else {
                    size.width += dw;
                    size.height += dh;
                }
                window.set_size(size);
            }
        }
    }

    /// Minimize a window
    pub fn minimize(&mut self, app_id: OverlayAppId) {
        if let Some(window) = self.windows.get_mut(&app_id) {
            window.minimize();

            // Focus next window if this was focused
            if self.focused == Some(app_id) {
                self.focused = self.top_window();
                if let Some(id) = self.focused {
                    if let Some(w) = self.windows.get_mut(&id) {
                        w.focused = true;
                    }
                }
            }
        }
    }

    /// Restore a minimized window
    pub fn restore(&mut self, app_id: OverlayAppId) {
        if let Some(window) = self.windows.get_mut(&app_id) {
            window.restore();
            self.focus(app_id);
        }
    }

    /// Toggle fullscreen for a window
    pub fn toggle_fullscreen(&mut self, app_id: OverlayAppId) {
        if let Some(window) = self.windows.get_mut(&app_id) {
            window.toggle_fullscreen();
        }
    }

    /// Get all minimized windows
    pub fn minimized_windows(&self) -> Vec<OverlayAppId> {
        self.windows
            .values()
            .filter(|w| w.mode == WindowMode::Minimized)
            .map(|w| w.app_id)
            .collect()
    }

    /// Get window count
    pub fn len(&self) -> usize {
        self.windows.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.windows.is_empty()
    }
}

impl Default for WindowManager {
    fn default() -> Self {
        Self::new(1920.0, 1080.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_window_z_order() {
        let mut manager = WindowManager::new(800.0, 600.0);

        let app1 = OverlayAppId::new();
        let app2 = OverlayAppId::new();
        let app3 = OverlayAppId::new();

        manager.create_window(app1, "App 1");
        manager.create_window(app2, "App 2");
        manager.create_window(app3, "App 3");

        // App 3 should be on top (loaded last)
        assert_eq!(manager.top_window(), Some(app3));
        assert_eq!(manager.focused(), Some(app3));

        // Focus app 1 - should come to top
        manager.focus(app1);
        assert_eq!(manager.top_window(), Some(app1));
        assert_eq!(manager.focused(), Some(app1));
    }

    #[test]
    fn test_window_hit_test() {
        let mut manager = WindowManager::new(800.0, 600.0);

        let app1 = OverlayAppId::new();
        let app2 = OverlayAppId::new();

        // App 1 at top-left
        let w1 = manager.create_window(app1, "App 1");
        w1.position = WindowPosition::pixels(0.0, 0.0);
        w1.size = WindowSize::pixels(200.0, 200.0);

        // App 2 overlapping
        let w2 = manager.create_window(app2, "App 2");
        w2.position = WindowPosition::pixels(100.0, 100.0);
        w2.size = WindowSize::pixels(200.0, 200.0);

        // Point in only app 1
        assert_eq!(manager.window_at(50.0, 50.0), Some(app1));

        // Point in overlap - app 2 is on top
        assert_eq!(manager.window_at(150.0, 150.0), Some(app2));

        // Point in only app 2
        assert_eq!(manager.window_at(250.0, 250.0), Some(app2));

        // Point outside both
        assert_eq!(manager.window_at(500.0, 500.0), None);
    }

    #[test]
    fn test_fullscreen_toggle() {
        let mut manager = WindowManager::new(800.0, 600.0);

        let app = OverlayAppId::new();
        let window = manager.create_window(app, "App");

        // Set initial position/size
        window.position = WindowPosition::pixels(100.0, 100.0);
        window.size = WindowSize::pixels(300.0, 200.0);

        assert_eq!(window.mode, WindowMode::Windowed);

        // Go fullscreen
        manager.toggle_fullscreen(app);
        let window = manager.get(app).unwrap();
        assert_eq!(window.mode, WindowMode::Fullscreen);

        // Restore windowed
        manager.toggle_fullscreen(app);
        let window = manager.get(app).unwrap();
        assert_eq!(window.mode, WindowMode::Windowed);
        assert_eq!(window.position.x, 100.0);
        assert_eq!(window.size.width, 300.0);
    }
}
