//! Interaction prompts

use serde::{Deserialize, Serialize};

/// Prompt visual style
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PromptStyle {
    /// Background color
    pub background_color: [f32; 4],
    /// Text color
    pub text_color: [f32; 4],
    /// Key/button highlight color
    pub key_color: [f32; 4],
    /// Border color
    pub border_color: [f32; 4],
    /// Font size
    pub font_size: f32,
    /// Padding
    pub padding: f32,
    /// Corner radius
    pub corner_radius: f32,
    /// Key box size
    pub key_size: f32,
}

impl Default for PromptStyle {
    fn default() -> Self {
        Self {
            background_color: [0.1, 0.1, 0.1, 0.8],
            text_color: [1.0, 1.0, 1.0, 1.0],
            key_color: [0.3, 0.5, 0.8, 1.0],
            border_color: [0.3, 0.3, 0.3, 1.0],
            font_size: 16.0,
            padding: 10.0,
            corner_radius: 5.0,
            key_size: 24.0,
        }
    }
}

/// Input type for prompts
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum InputType {
    /// Keyboard key
    Key(String),
    /// Mouse button
    Mouse(String),
    /// Gamepad button
    Gamepad(String),
    /// Touch/tap
    Touch,
    /// Multiple inputs (any will work)
    Multiple(Vec<InputType>),
}

impl InputType {
    /// Keyboard E key (common interact)
    pub fn interact() -> Self {
        Self::Key("E".to_string())
    }

    /// Keyboard F key
    pub fn use_key() -> Self {
        Self::Key("F".to_string())
    }

    /// Keyboard Space
    pub fn space() -> Self {
        Self::Key("Space".to_string())
    }

    /// Left mouse button
    pub fn left_click() -> Self {
        Self::Mouse("LMB".to_string())
    }

    /// Right mouse button
    pub fn right_click() -> Self {
        Self::Mouse("RMB".to_string())
    }

    /// Xbox A button
    pub fn gamepad_a() -> Self {
        Self::Gamepad("A".to_string())
    }

    /// Get display text
    pub fn display_text(&self) -> String {
        match self {
            Self::Key(k) => k.clone(),
            Self::Mouse(m) => m.clone(),
            Self::Gamepad(g) => g.clone(),
            Self::Touch => "Tap".to_string(),
            Self::Multiple(inputs) => {
                if let Some(first) = inputs.first() {
                    first.display_text()
                } else {
                    "?".to_string()
                }
            }
        }
    }
}

/// An interaction prompt
#[derive(Debug, Clone)]
pub struct InteractionPrompt {
    /// Input required
    pub input: InputType,
    /// Action text (e.g., "Open", "Talk", "Pick up")
    pub action: String,
    /// Object/target name (optional)
    pub target: Option<String>,
    /// World position (for 3D prompts)
    pub world_position: Option<[f32; 3]>,
    /// Screen position (calculated or fixed)
    pub screen_position: [f32; 2],
    /// Style
    pub style: PromptStyle,
    /// Whether visible
    pub visible: bool,
    /// Opacity (for fade effects)
    pub opacity: f32,
    /// Scale (for animations)
    pub scale: f32,
    /// Whether the input is currently held
    pub held: bool,
    /// Hold progress (0.0 - 1.0 for hold-to-interact)
    pub hold_progress: f32,
    /// Required hold time (0 = instant)
    pub hold_time: f32,
}

impl InteractionPrompt {
    /// Create a new interaction prompt
    pub fn new(input: InputType, action: impl Into<String>) -> Self {
        Self {
            input,
            action: action.into(),
            target: None,
            world_position: None,
            screen_position: [0.0, 0.0],
            style: PromptStyle::default(),
            visible: true,
            opacity: 1.0,
            scale: 1.0,
            held: false,
            hold_progress: 0.0,
            hold_time: 0.0,
        }
    }

    /// Set target name
    pub fn with_target(mut self, target: impl Into<String>) -> Self {
        self.target = Some(target.into());
        self
    }

    /// Set world position
    pub fn at_world(mut self, position: [f32; 3]) -> Self {
        self.world_position = Some(position);
        self
    }

    /// Set screen position
    pub fn at_screen(mut self, x: f32, y: f32) -> Self {
        self.screen_position = [x, y];
        self
    }

    /// Set style
    pub fn with_style(mut self, style: PromptStyle) -> Self {
        self.style = style;
        self
    }

    /// Set hold time
    pub fn with_hold_time(mut self, time: f32) -> Self {
        self.hold_time = time;
        self
    }

    /// Get full display text
    pub fn full_text(&self) -> String {
        match &self.target {
            Some(target) => format!("{} {}", self.action, target),
            None => self.action.clone(),
        }
    }

    /// Start holding
    pub fn start_hold(&mut self) {
        self.held = true;
    }

    /// Stop holding
    pub fn stop_hold(&mut self) {
        self.held = false;
        self.hold_progress = 0.0;
    }

    /// Update hold progress
    pub fn update(&mut self, delta_time: f32) -> bool {
        if self.hold_time <= 0.0 {
            return false; // Instant interaction, no hold needed
        }

        if self.held {
            self.hold_progress += delta_time / self.hold_time;
            if self.hold_progress >= 1.0 {
                self.hold_progress = 1.0;
                return true; // Interaction complete
            }
        } else {
            // Decay when not held
            self.hold_progress = (self.hold_progress - delta_time * 2.0).max(0.0);
        }

        false
    }

    /// Check if interaction is complete
    pub fn is_complete(&self) -> bool {
        self.hold_progress >= 1.0
    }

    /// Reset the prompt
    pub fn reset(&mut self) {
        self.held = false;
        self.hold_progress = 0.0;
    }

    /// Show the prompt
    pub fn show(&mut self) {
        self.visible = true;
    }

    /// Hide the prompt
    pub fn hide(&mut self) {
        self.visible = false;
    }
}

impl Default for InteractionPrompt {
    fn default() -> Self {
        Self::new(InputType::interact(), "Interact")
    }
}

// Convenience constructors
impl InteractionPrompt {
    /// "Press E to interact" prompt
    pub fn interact() -> Self {
        Self::new(InputType::interact(), "Interact")
    }

    /// "Press E to talk" prompt
    pub fn talk(name: impl Into<String>) -> Self {
        Self::new(InputType::interact(), "Talk to")
            .with_target(name)
    }

    /// "Press E to open" prompt
    pub fn open(target: impl Into<String>) -> Self {
        Self::new(InputType::interact(), "Open")
            .with_target(target)
    }

    /// "Press E to pick up" prompt
    pub fn pick_up(item: impl Into<String>) -> Self {
        Self::new(InputType::interact(), "Pick up")
            .with_target(item)
    }

    /// "Hold E to use" prompt with hold time
    pub fn hold_to_use(action: impl Into<String>, time: f32) -> Self {
        Self::new(InputType::interact(), action)
            .with_hold_time(time)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_prompt() {
        let prompt = InteractionPrompt::new(InputType::interact(), "Open")
            .with_target("Door");

        assert_eq!(prompt.full_text(), "Open Door");
        assert_eq!(prompt.input.display_text(), "E");
    }

    #[test]
    fn test_hold_interaction() {
        let mut prompt = InteractionPrompt::hold_to_use("Revive", 3.0);

        assert!(!prompt.is_complete());

        prompt.start_hold();
        prompt.update(1.5);
        assert!(prompt.hold_progress > 0.0);
        assert!(!prompt.is_complete());

        prompt.update(2.0);
        assert!(prompt.is_complete());
    }

    #[test]
    fn test_convenience_constructors() {
        let talk = InteractionPrompt::talk("Guard");
        assert_eq!(talk.action, "Talk to");
        assert_eq!(talk.target, Some("Guard".to_string()));

        let pickup = InteractionPrompt::pick_up("Sword");
        assert_eq!(pickup.full_text(), "Pick up Sword");
    }

    #[test]
    fn test_input_types() {
        assert_eq!(InputType::interact().display_text(), "E");
        assert_eq!(InputType::gamepad_a().display_text(), "A");
        assert_eq!(InputType::left_click().display_text(), "LMB");
    }
}
