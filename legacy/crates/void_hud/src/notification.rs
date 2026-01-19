//! Notification system

use serde::{Deserialize, Serialize};
use std::collections::VecDeque;

/// Notification type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum NotificationType {
    /// Info/general notification
    Info,
    /// Success notification
    Success,
    /// Warning notification
    Warning,
    /// Error notification
    Error,
    /// Achievement unlocked
    Achievement,
    /// Quest update
    Quest,
    /// Item acquired
    Item,
    /// Level up
    LevelUp,
    /// Custom type
    Custom(u32),
}

impl NotificationType {
    /// Get default color for this type
    pub fn default_color(&self) -> [f32; 4] {
        match self {
            Self::Info => [0.9, 0.9, 0.9, 1.0],       // White
            Self::Success => [0.2, 0.8, 0.2, 1.0],    // Green
            Self::Warning => [0.9, 0.7, 0.1, 1.0],    // Orange
            Self::Error => [0.9, 0.2, 0.2, 1.0],      // Red
            Self::Achievement => [0.8, 0.6, 0.0, 1.0], // Gold
            Self::Quest => [0.4, 0.6, 0.9, 1.0],      // Blue
            Self::Item => [0.9, 0.9, 0.5, 1.0],       // Yellow
            Self::LevelUp => [0.8, 0.4, 0.9, 1.0],    // Purple
            Self::Custom(_) => [1.0, 1.0, 1.0, 1.0],
        }
    }

    /// Get icon name for this type
    pub fn icon_name(&self) -> &str {
        match self {
            Self::Info => "info",
            Self::Success => "check",
            Self::Warning => "warning",
            Self::Error => "error",
            Self::Achievement => "trophy",
            Self::Quest => "scroll",
            Self::Item => "bag",
            Self::LevelUp => "star",
            Self::Custom(_) => "default",
        }
    }
}

impl Default for NotificationType {
    fn default() -> Self {
        Self::Info
    }
}

/// A notification message
#[derive(Debug, Clone)]
pub struct Notification {
    /// Notification type
    pub notification_type: NotificationType,
    /// Title text
    pub title: String,
    /// Body/description text
    pub body: String,
    /// Icon path (overrides default)
    pub icon: Option<String>,
    /// Display duration in seconds
    pub duration: f32,
    /// Time remaining
    pub time_remaining: f32,
    /// Current opacity (for fade animation)
    pub opacity: f32,
    /// Slide offset (for animation)
    pub slide_offset: f32,
    /// Whether currently animating in
    pub animating_in: bool,
    /// Whether currently animating out
    pub animating_out: bool,
    /// Color override
    pub color: Option<[f32; 4]>,
    /// Sound to play
    pub sound: Option<String>,
    /// Click callback ID (for interactive notifications)
    pub callback_id: Option<u32>,
}

impl Notification {
    /// Create a new notification
    pub fn new(notification_type: NotificationType, title: impl Into<String>) -> Self {
        Self {
            notification_type,
            title: title.into(),
            body: String::new(),
            icon: None,
            duration: 3.0,
            time_remaining: 3.0,
            opacity: 0.0,
            slide_offset: 1.0,
            animating_in: true,
            animating_out: false,
            color: None,
            sound: None,
            callback_id: None,
        }
    }

    /// Set body text
    pub fn with_body(mut self, body: impl Into<String>) -> Self {
        self.body = body.into();
        self
    }

    /// Set duration
    pub fn with_duration(mut self, duration: f32) -> Self {
        self.duration = duration;
        self.time_remaining = duration;
        self
    }

    /// Set icon
    pub fn with_icon(mut self, icon: impl Into<String>) -> Self {
        self.icon = Some(icon.into());
        self
    }

    /// Set color
    pub fn with_color(mut self, color: [f32; 4]) -> Self {
        self.color = Some(color);
        self
    }

    /// Set sound
    pub fn with_sound(mut self, sound: impl Into<String>) -> Self {
        self.sound = Some(sound.into());
        self
    }

    /// Set callback
    pub fn with_callback(mut self, callback_id: u32) -> Self {
        self.callback_id = Some(callback_id);
        self
    }

    /// Get effective color
    pub fn effective_color(&self) -> [f32; 4] {
        self.color.unwrap_or_else(|| self.notification_type.default_color())
    }

    /// Update animation and timer
    pub fn update(&mut self, delta_time: f32) -> bool {
        const ANIM_SPEED: f32 = 8.0;

        if self.animating_in {
            self.opacity = (self.opacity + ANIM_SPEED * delta_time).min(1.0);
            self.slide_offset = (self.slide_offset - ANIM_SPEED * delta_time).max(0.0);

            if self.opacity >= 1.0 && self.slide_offset <= 0.0 {
                self.animating_in = false;
            }
        } else if self.animating_out {
            self.opacity = (self.opacity - ANIM_SPEED * delta_time).max(0.0);
            self.slide_offset = self.slide_offset + ANIM_SPEED * delta_time;

            if self.opacity <= 0.0 {
                return false; // Remove notification
            }
        } else {
            self.time_remaining -= delta_time;
            if self.time_remaining <= 0.0 {
                self.animating_out = true;
            }
        }

        true // Keep notification
    }

    /// Dismiss the notification
    pub fn dismiss(&mut self) {
        self.animating_out = true;
    }
}

// Convenience constructors
impl Notification {
    /// Info notification
    pub fn info(title: impl Into<String>) -> Self {
        Self::new(NotificationType::Info, title)
    }

    /// Success notification
    pub fn success(title: impl Into<String>) -> Self {
        Self::new(NotificationType::Success, title)
    }

    /// Warning notification
    pub fn warning(title: impl Into<String>) -> Self {
        Self::new(NotificationType::Warning, title)
    }

    /// Error notification
    pub fn error(title: impl Into<String>) -> Self {
        Self::new(NotificationType::Error, title)
    }

    /// Achievement notification
    pub fn achievement(title: impl Into<String>) -> Self {
        Self::new(NotificationType::Achievement, title)
            .with_duration(5.0)
    }

    /// Item acquired notification
    pub fn item(name: impl Into<String>) -> Self {
        Self::new(NotificationType::Item, name)
            .with_duration(2.0)
    }
}

/// Notification manager
#[derive(Debug, Default)]
pub struct NotificationManager {
    /// Active notifications
    notifications: VecDeque<Notification>,
    /// Maximum visible notifications
    pub max_visible: usize,
    /// Spacing between notifications
    pub spacing: f32,
    /// Start position Y
    pub start_y: f32,
    /// Notification width
    pub width: f32,
    /// Notification height
    pub height: f32,
    /// Align right (false = left)
    pub align_right: bool,
    /// Screen width (for right alignment)
    pub screen_width: f32,
    /// Margin from edge
    pub margin: f32,
}

impl NotificationManager {
    /// Create a new notification manager
    pub fn new() -> Self {
        Self {
            notifications: VecDeque::new(),
            max_visible: 5,
            spacing: 10.0,
            start_y: 50.0,
            width: 300.0,
            height: 80.0,
            align_right: true,
            screen_width: 1920.0,
            margin: 20.0,
        }
    }

    /// Set screen dimensions
    pub fn set_screen_size(&mut self, width: f32) {
        self.screen_width = width;
    }

    /// Push a notification
    pub fn push(&mut self, notification: Notification) {
        // Limit number of notifications
        while self.notifications.len() >= self.max_visible {
            self.notifications.pop_front();
        }
        self.notifications.push_back(notification);
    }

    /// Update all notifications
    pub fn update(&mut self, delta_time: f32) {
        self.notifications.retain_mut(|n| n.update(delta_time));
    }

    /// Get active notifications with their positions
    pub fn get_notifications(&self) -> Vec<(&Notification, f32, f32)> {
        let x = if self.align_right {
            self.screen_width - self.width - self.margin
        } else {
            self.margin
        };

        self.notifications
            .iter()
            .enumerate()
            .map(|(i, n)| {
                let y = self.start_y + (i as f32) * (self.height + self.spacing);
                let y_with_offset = y - n.slide_offset * self.height;
                (n, x, y_with_offset)
            })
            .collect()
    }

    /// Dismiss all notifications
    pub fn dismiss_all(&mut self) {
        for notification in &mut self.notifications {
            notification.dismiss();
        }
    }

    /// Get notification count
    pub fn count(&self) -> usize {
        self.notifications.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.notifications.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_notification() {
        let notif = Notification::info("Test")
            .with_body("Description")
            .with_duration(5.0);

        assert_eq!(notif.title, "Test");
        assert_eq!(notif.duration, 5.0);
    }

    #[test]
    fn test_notification_update() {
        let mut notif = Notification::info("Test").with_duration(1.0);

        // Animate in
        notif.update(0.5);
        assert!(notif.opacity > 0.0);

        // Let it expire
        notif.animating_in = false;
        notif.opacity = 1.0;
        notif.update(1.5);

        assert!(notif.animating_out);
    }

    #[test]
    fn test_notification_manager() {
        let mut manager = NotificationManager::new();

        manager.push(Notification::info("Test 1"));
        manager.push(Notification::success("Test 2"));

        assert_eq!(manager.count(), 2);

        let positions = manager.get_notifications();
        assert_eq!(positions.len(), 2);
    }

    #[test]
    fn test_notification_types() {
        let info = Notification::info("Info");
        let error = Notification::error("Error");

        assert_eq!(info.notification_type, NotificationType::Info);
        assert_eq!(error.notification_type, NotificationType::Error);
    }
}
