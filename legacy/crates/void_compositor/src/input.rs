//! Input event handling
//!
//! Provides a unified input API that abstracts over Smithay's input backend.

use glam::Vec2;

/// Input event from any device
#[derive(Debug, Clone)]
pub enum InputEvent {
    /// Keyboard event
    Keyboard(KeyboardEvent),
    /// Pointer (mouse/touchpad) event
    Pointer(PointerEvent),
    /// Touch event
    Touch(TouchEvent),
    /// Device added/removed
    Device(DeviceEvent),
}

/// Keyboard event
#[derive(Debug, Clone)]
pub struct KeyboardEvent {
    /// Key code (hardware-specific)
    pub keycode: u32,
    /// Key state
    pub state: KeyState,
    /// Timestamp in milliseconds
    pub time_ms: u32,
    /// Modifier state
    pub modifiers: Modifiers,
}

/// Key state
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyState {
    Pressed,
    Released,
}

/// Keyboard modifiers
#[derive(Debug, Clone, Copy, Default)]
pub struct Modifiers {
    pub shift: bool,
    pub ctrl: bool,
    pub alt: bool,
    pub logo: bool,  // Windows/Super/Command key
    pub caps_lock: bool,
    pub num_lock: bool,
}

/// Pointer (mouse/touchpad) event
#[derive(Debug, Clone)]
pub enum PointerEvent {
    /// Pointer moved
    Motion {
        /// Absolute position (if known)
        position: Option<Vec2>,
        /// Delta movement
        delta: Vec2,
        /// Timestamp in milliseconds
        time_ms: u32,
    },
    /// Button pressed/released
    Button {
        button: PointerButton,
        state: ButtonState,
        time_ms: u32,
    },
    /// Scroll wheel/gesture
    Axis {
        /// Horizontal scroll
        horizontal: f64,
        /// Vertical scroll
        vertical: f64,
        /// Source (wheel, finger, etc.)
        source: AxisSource,
        time_ms: u32,
    },
}

/// Mouse button
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum PointerButton {
    Left,
    Right,
    Middle,
    /// Additional buttons (back, forward, etc.)
    Other(u32),
}

impl From<u32> for PointerButton {
    fn from(code: u32) -> Self {
        match code {
            0x110 => Self::Left,    // BTN_LEFT
            0x111 => Self::Right,   // BTN_RIGHT
            0x112 => Self::Middle,  // BTN_MIDDLE
            _ => Self::Other(code),
        }
    }
}

/// Button state
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ButtonState {
    Pressed,
    Released,
}

/// Axis event source
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AxisSource {
    /// Mouse wheel
    Wheel,
    /// Touchpad finger
    Finger,
    /// Continuous (trackball, etc.)
    Continuous,
    /// Unknown
    Unknown,
}

/// Touch event
#[derive(Debug, Clone)]
pub enum TouchEvent {
    /// Touch started
    Down {
        slot: i32,
        position: Vec2,
        time_ms: u32,
    },
    /// Touch moved
    Motion {
        slot: i32,
        position: Vec2,
        time_ms: u32,
    },
    /// Touch ended
    Up {
        slot: i32,
        time_ms: u32,
    },
    /// Touch cancelled
    Cancel {
        slot: i32,
        time_ms: u32,
    },
}

/// Device event
#[derive(Debug, Clone)]
pub enum DeviceEvent {
    /// Device added
    Added {
        device_id: u64,
        name: String,
        device_type: DeviceType,
    },
    /// Device removed
    Removed {
        device_id: u64,
    },
}

/// Input device type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeviceType {
    Keyboard,
    Pointer,
    Touch,
    Tablet,
    Other,
}

/// Input state tracker
#[derive(Debug, Default)]
pub struct InputState {
    /// Currently pressed keys (by keycode)
    pressed_keys: std::collections::HashSet<u32>,
    /// Currently pressed mouse buttons
    pressed_buttons: std::collections::HashSet<PointerButton>,
    /// Current pointer position
    pointer_position: Vec2,
    /// Current modifiers
    modifiers: Modifiers,
}

impl InputState {
    /// Create new input state
    pub fn new() -> Self {
        Self::default()
    }

    /// Process an input event
    pub fn handle_event(&mut self, event: &InputEvent) {
        match event {
            InputEvent::Keyboard(ke) => {
                match ke.state {
                    KeyState::Pressed => { self.pressed_keys.insert(ke.keycode); }
                    KeyState::Released => { self.pressed_keys.remove(&ke.keycode); }
                }
                self.modifiers = ke.modifiers;
            }
            InputEvent::Pointer(pe) => {
                match pe {
                    PointerEvent::Motion { position, .. } => {
                        if let Some(pos) = position {
                            self.pointer_position = *pos;
                        }
                    }
                    PointerEvent::Button { button, state, .. } => {
                        match state {
                            ButtonState::Pressed => { self.pressed_buttons.insert(*button); }
                            ButtonState::Released => { self.pressed_buttons.remove(button); }
                        }
                    }
                    _ => {}
                }
            }
            _ => {}
        }
    }

    /// Check if a key is pressed
    pub fn is_key_pressed(&self, keycode: u32) -> bool {
        self.pressed_keys.contains(&keycode)
    }

    /// Check if a button is pressed
    pub fn is_button_pressed(&self, button: PointerButton) -> bool {
        self.pressed_buttons.contains(&button)
    }

    /// Get pointer position
    pub fn pointer_position(&self) -> Vec2 {
        self.pointer_position
    }

    /// Get current modifiers
    pub fn modifiers(&self) -> Modifiers {
        self.modifiers
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_input_state() {
        let mut state = InputState::new();

        // Press a key
        state.handle_event(&InputEvent::Keyboard(KeyboardEvent {
            keycode: 30, // 'A' on most keyboards
            state: KeyState::Pressed,
            time_ms: 0,
            modifiers: Modifiers::default(),
        }));

        assert!(state.is_key_pressed(30));

        // Release the key
        state.handle_event(&InputEvent::Keyboard(KeyboardEvent {
            keycode: 30,
            state: KeyState::Released,
            time_ms: 10,
            modifiers: Modifiers::default(),
        }));

        assert!(!state.is_key_pressed(30));
    }
}
