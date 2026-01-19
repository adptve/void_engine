//! Input handling for the runtime
//!
//! Processes keyboard, mouse, touch, and other input events.
//! Supports multiple input sources:
//! - Winit (for windowed mode)
//! - TTY (for DRM mode without X/Wayland)
//! - Smithay/libinput (for full compositor mode)
//!
//! Input events are routed to the focused app layer.

use std::collections::{HashMap, HashSet, VecDeque};
use std::io::Read;
use std::sync::atomic::{AtomicU64, Ordering};

use winit::event::{ElementState, KeyEvent, MouseButton};
use winit::keyboard::Key;
use void_kernel::{AppId, LayerId};

/// Unique input target identifier
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct InputTargetId(u64);

impl InputTargetId {
    /// Create a new unique ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get raw ID value
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for InputTargetId {
    fn default() -> Self {
        Self::new()
    }
}

/// Target for input routing
#[derive(Debug, Clone)]
pub enum InputTarget {
    /// Route to a specific app
    App(AppId),
    /// Route to a specific layer
    Layer(LayerId),
    /// Route to the shell
    Shell,
    /// Route to the compositor itself (for window management)
    Compositor,
    /// No target (discard input)
    None,
}

/// Unified input event type
#[derive(Debug, Clone)]
pub enum UnifiedInputEvent {
    /// Keyboard event
    Keyboard(UnifiedKeyEvent),
    /// Mouse/pointer motion
    PointerMotion {
        x: f32,
        y: f32,
        delta_x: f32,
        delta_y: f32,
        time_ms: u32,
    },
    /// Mouse/pointer button
    PointerButton {
        button: UnifiedMouseButton,
        state: UnifiedButtonState,
        time_ms: u32,
    },
    /// Scroll wheel
    Scroll {
        delta_x: f32,
        delta_y: f32,
        time_ms: u32,
    },
    /// Touch event
    Touch(UnifiedTouchEvent),
    /// Focus change
    Focus {
        gained: bool,
    },
}

/// Unified keyboard event
#[derive(Debug, Clone)]
pub struct UnifiedKeyEvent {
    /// Key code (platform-specific)
    pub keycode: u32,
    /// Key name/symbol
    pub key_name: String,
    /// Key state
    pub state: UnifiedButtonState,
    /// Modifier state
    pub modifiers: InputModifiers,
    /// Time in milliseconds
    pub time_ms: u32,
    /// Whether this is a repeat event
    pub is_repeat: bool,
}

/// Unified button state
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UnifiedButtonState {
    Pressed,
    Released,
}

impl From<ElementState> for UnifiedButtonState {
    fn from(state: ElementState) -> Self {
        match state {
            ElementState::Pressed => Self::Pressed,
            ElementState::Released => Self::Released,
        }
    }
}

/// Unified mouse button
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum UnifiedMouseButton {
    Left,
    Right,
    Middle,
    Back,
    Forward,
    Other(u32),
}

impl From<MouseButton> for UnifiedMouseButton {
    fn from(button: MouseButton) -> Self {
        match button {
            MouseButton::Left => Self::Left,
            MouseButton::Right => Self::Right,
            MouseButton::Middle => Self::Middle,
            MouseButton::Back => Self::Back,
            MouseButton::Forward => Self::Forward,
            MouseButton::Other(n) => Self::Other(n as u32),
        }
    }
}

/// Unified touch event
#[derive(Debug, Clone)]
pub enum UnifiedTouchEvent {
    /// Touch started
    Down {
        id: u64,
        x: f32,
        y: f32,
        time_ms: u32,
    },
    /// Touch moved
    Motion {
        id: u64,
        x: f32,
        y: f32,
        time_ms: u32,
    },
    /// Touch ended
    Up {
        id: u64,
        time_ms: u32,
    },
    /// Touch cancelled
    Cancel {
        id: u64,
        time_ms: u32,
    },
}

/// Keyboard/input modifiers
#[derive(Debug, Clone, Copy, Default)]
pub struct InputModifiers {
    pub shift: bool,
    pub ctrl: bool,
    pub alt: bool,
    pub logo: bool,  // Super/Windows/Command key
    pub caps_lock: bool,
    pub num_lock: bool,
}

/// Input state for winit backend
#[derive(Debug, Default)]
pub struct InputState {
    /// Keys currently pressed
    pub keys_pressed: HashSet<String>,
    /// Mouse position
    pub mouse_position: (f32, f32),
    /// Mouse buttons pressed
    pub mouse_buttons: HashSet<MouseButton>,
    /// Current modifiers
    pub modifiers: InputModifiers,
}

impl InputState {
    /// Create new input state
    pub fn new() -> Self {
        Self::default()
    }

    /// Handle a key event
    pub fn handle_key(&mut self, event: &KeyEvent) {
        let key_str = match &event.logical_key {
            Key::Named(named) => format!("{:?}", named),
            Key::Character(c) => c.to_string(),
            _ => return,
        };

        match event.state {
            ElementState::Pressed => {
                self.keys_pressed.insert(key_str);
            }
            ElementState::Released => {
                self.keys_pressed.remove(&key_str);
            }
        }
    }

    /// Check if a key is pressed
    pub fn is_key_pressed(&self, key: &str) -> bool {
        self.keys_pressed.contains(key)
    }

    /// Update mouse position
    pub fn set_mouse_position(&mut self, x: f32, y: f32) {
        self.mouse_position = (x, y);
    }

    /// Handle mouse button
    pub fn handle_mouse_button(&mut self, button: MouseButton, state: ElementState) {
        match state {
            ElementState::Pressed => {
                self.mouse_buttons.insert(button);
            }
            ElementState::Released => {
                self.mouse_buttons.remove(&button);
            }
        }
    }

    /// Check if mouse button is pressed
    pub fn is_mouse_pressed(&self, button: MouseButton) -> bool {
        self.mouse_buttons.contains(&button)
    }

    /// Convert to unified event
    pub fn key_to_unified(&self, event: &KeyEvent) -> UnifiedInputEvent {
        let key_name = match &event.logical_key {
            Key::Named(named) => format!("{:?}", named),
            Key::Character(c) => c.to_string(),
            _ => "Unknown".to_string(),
        };

        UnifiedInputEvent::Keyboard(UnifiedKeyEvent {
            keycode: 0, // Winit doesn't expose raw keycodes easily
            key_name,
            state: event.state.into(),
            modifiers: self.modifiers,
            time_ms: 0,
            is_repeat: event.repeat,
        })
    }
}

/// Input manager for routing input events
///
/// Manages focus, input routing, and event dispatch to apps.
pub struct InputManager {
    /// Current focus target
    focus_target: InputTarget,
    /// Focus stack for modal dialogs
    focus_stack: Vec<InputTarget>,
    /// Registered input targets
    targets: HashMap<InputTargetId, InputTargetInfo>,
    /// Input event queue
    event_queue: VecDeque<(UnifiedInputEvent, InputTarget)>,
    /// Global keyboard shortcuts
    global_shortcuts: HashMap<ShortcutKey, ShortcutAction>,
    /// Current pointer position
    pointer_position: (f32, f32),
    /// Screen size for hit testing
    screen_size: (u32, u32),
    /// TTY input buffer (for DRM mode)
    tty_buffer: Vec<u8>,
    /// Whether TTY raw mode is enabled
    tty_raw_mode: bool,
}

/// Input target registration info
#[derive(Debug, Clone)]
pub struct InputTargetInfo {
    /// Target ID
    pub id: InputTargetId,
    /// Associated target
    pub target: InputTarget,
    /// Input bounds (screen coordinates)
    pub bounds: (f32, f32, f32, f32), // x, y, width, height
    /// Whether target accepts keyboard input
    pub accepts_keyboard: bool,
    /// Whether target accepts pointer input
    pub accepts_pointer: bool,
    /// Whether target accepts touch input
    pub accepts_touch: bool,
    /// Z-order for hit testing
    pub z_order: i32,
}

/// Global shortcut key
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ShortcutKey {
    /// Key name
    pub key: String,
    /// Required modifiers
    pub modifiers: ShortcutModifiers,
}

/// Shortcut modifier requirements
#[derive(Debug, Clone, PartialEq, Eq, Hash, Default)]
pub struct ShortcutModifiers {
    pub shift: bool,
    pub ctrl: bool,
    pub alt: bool,
    pub logo: bool,
}

/// Shortcut action
#[derive(Debug, Clone)]
pub enum ShortcutAction {
    /// Toggle shell visibility
    ToggleShell,
    /// Switch to next window/app
    NextWindow,
    /// Switch to previous window/app
    PreviousWindow,
    /// Quit/exit
    Quit,
    /// Custom action with name
    Custom(String),
}

/// TTY input events for console mode (DRM backend)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TtyInput {
    /// Regular character
    Character(char),
    /// Escape key
    Escape,
    /// Arrow keys
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    /// Function keys
    FunctionKey(u8),
    /// No input available
    None,
}

impl InputManager {
    /// Create a new input manager
    pub fn new() -> Self {
        let mut manager = Self {
            focus_target: InputTarget::Shell,
            focus_stack: Vec::new(),
            targets: HashMap::new(),
            event_queue: VecDeque::new(),
            global_shortcuts: HashMap::new(),
            pointer_position: (0.0, 0.0),
            screen_size: (1920, 1080),
            tty_buffer: Vec::with_capacity(32),
            tty_raw_mode: false,
        };

        // Register default shortcuts
        manager.register_default_shortcuts();

        manager
    }

    /// Register default global shortcuts
    fn register_default_shortcuts(&mut self) {
        // Backtick toggles shell
        self.global_shortcuts.insert(
            ShortcutKey {
                key: "`".to_string(),
                modifiers: ShortcutModifiers::default(),
            },
            ShortcutAction::ToggleShell,
        );

        // Alt+Tab switches windows
        self.global_shortcuts.insert(
            ShortcutKey {
                key: "Tab".to_string(),
                modifiers: ShortcutModifiers { alt: true, ..Default::default() },
            },
            ShortcutAction::NextWindow,
        );

        // Ctrl+Q quits
        self.global_shortcuts.insert(
            ShortcutKey {
                key: "q".to_string(),
                modifiers: ShortcutModifiers { ctrl: true, ..Default::default() },
            },
            ShortcutAction::Quit,
        );
    }

    /// Set screen size for hit testing
    pub fn set_screen_size(&mut self, width: u32, height: u32) {
        self.screen_size = (width, height);
    }

    /// Get current focus target
    pub fn focus_target(&self) -> &InputTarget {
        &self.focus_target
    }

    /// Set focus target
    pub fn set_focus(&mut self, target: InputTarget) {
        self.focus_target = target;
    }

    /// Push focus (for modal dialogs)
    pub fn push_focus(&mut self, target: InputTarget) {
        self.focus_stack.push(self.focus_target.clone());
        self.focus_target = target;
    }

    /// Pop focus (returning from modal)
    pub fn pop_focus(&mut self) -> InputTarget {
        if let Some(previous) = self.focus_stack.pop() {
            let current = std::mem::replace(&mut self.focus_target, previous);
            current
        } else {
            self.focus_target.clone()
        }
    }

    /// Register an input target
    pub fn register_target(&mut self, info: InputTargetInfo) -> InputTargetId {
        let id = info.id;
        self.targets.insert(id, info);
        id
    }

    /// Unregister an input target
    pub fn unregister_target(&mut self, id: InputTargetId) -> Option<InputTargetInfo> {
        self.targets.remove(&id)
    }

    /// Route a keyboard event
    pub fn route_keyboard(&mut self, event: UnifiedInputEvent) -> Option<(UnifiedInputEvent, InputTarget)> {
        // Check global shortcuts first
        if let UnifiedInputEvent::Keyboard(ref ke) = event {
            if ke.state == UnifiedButtonState::Pressed {
                let shortcut_key = ShortcutKey {
                    key: ke.key_name.clone(),
                    modifiers: ShortcutModifiers {
                        shift: ke.modifiers.shift,
                        ctrl: ke.modifiers.ctrl,
                        alt: ke.modifiers.alt,
                        logo: ke.modifiers.logo,
                    },
                };

                if let Some(action) = self.global_shortcuts.get(&shortcut_key).cloned() {
                    // Return shortcut action as a routed event to compositor
                    return Some((event, InputTarget::Compositor));
                }
            }
        }

        // Route to current focus
        Some((event, self.focus_target.clone()))
    }

    /// Route a pointer event using hit testing
    pub fn route_pointer(&mut self, event: UnifiedInputEvent) -> Option<(UnifiedInputEvent, InputTarget)> {
        // Update pointer position
        if let UnifiedInputEvent::PointerMotion { x, y, .. } = &event {
            self.pointer_position = (*x, *y);
        }

        // Hit test against registered targets
        let target = self.hit_test(self.pointer_position.0, self.pointer_position.1);

        Some((event, target))
    }

    /// Perform hit testing
    fn hit_test(&self, x: f32, y: f32) -> InputTarget {
        // Find topmost target that contains the point
        let mut best_target: Option<&InputTargetInfo> = None;
        let mut best_z = i32::MIN;

        for info in self.targets.values() {
            if !info.accepts_pointer {
                continue;
            }

            let (bx, by, bw, bh) = info.bounds;
            if x >= bx && x < bx + bw && y >= by && y < by + bh {
                if info.z_order > best_z {
                    best_z = info.z_order;
                    best_target = Some(info);
                }
            }
        }

        best_target
            .map(|info| info.target.clone())
            .unwrap_or_else(|| self.focus_target.clone())
    }

    /// Dispatch events to the queue
    pub fn dispatch(&mut self, event: UnifiedInputEvent) {
        let routed = match &event {
            UnifiedInputEvent::Keyboard(_) => self.route_keyboard(event),
            UnifiedInputEvent::PointerMotion { .. }
            | UnifiedInputEvent::PointerButton { .. }
            | UnifiedInputEvent::Scroll { .. } => self.route_pointer(event),
            UnifiedInputEvent::Touch(_) => Some((event, self.focus_target.clone())),
            UnifiedInputEvent::Focus { .. } => Some((event, self.focus_target.clone())),
        };

        if let Some((event, target)) = routed {
            self.event_queue.push_back((event, target));
        }
    }

    /// Poll the event queue
    pub fn poll(&mut self) -> Option<(UnifiedInputEvent, InputTarget)> {
        self.event_queue.pop_front()
    }

    /// Get all queued events (drains the queue)
    pub fn drain_events(&mut self) -> Vec<(UnifiedInputEvent, InputTarget)> {
        self.event_queue.drain(..).collect()
    }

    /// Poll TTY input in non-blocking mode (for DRM/console mode)
    #[cfg(target_os = "linux")]
    pub fn poll_tty_input(&mut self) -> Option<TtyInput> {
        use std::os::unix::io::AsRawFd;

        // Try to read from stdin non-blocking
        let stdin = std::io::stdin();
        let fd = stdin.as_raw_fd();

        // Set non-blocking
        unsafe {
            let flags = libc::fcntl(fd, libc::F_GETFL);
            libc::fcntl(fd, libc::F_SETFL, flags | libc::O_NONBLOCK);
        }

        let mut buf = [0u8; 8];
        let result = stdin.lock().read(&mut buf);

        // Restore blocking
        unsafe {
            let flags = libc::fcntl(fd, libc::F_GETFL);
            libc::fcntl(fd, libc::F_SETFL, flags & !libc::O_NONBLOCK);
        }

        match result {
            Ok(0) => None,
            Ok(n) => {
                // Parse escape sequences
                if buf[0] == 0x1b {
                    if n == 1 {
                        return Some(TtyInput::Escape);
                    }
                    // Check for arrow keys: ESC [ A/B/C/D
                    if n >= 3 && buf[1] == b'[' {
                        return match buf[2] {
                            b'A' => Some(TtyInput::ArrowUp),
                            b'B' => Some(TtyInput::ArrowDown),
                            b'C' => Some(TtyInput::ArrowRight),
                            b'D' => Some(TtyInput::ArrowLeft),
                            _ => None,
                        };
                    }
                }
                // Regular character
                if buf[0].is_ascii() {
                    Some(TtyInput::Character(buf[0] as char))
                } else {
                    None
                }
            }
            Err(_) => None,
        }
    }

    /// Stub for non-Linux platforms
    #[cfg(not(target_os = "linux"))]
    pub fn poll_tty_input(&mut self) -> Option<TtyInput> {
        None
    }

    /// Check if a global shortcut was triggered
    pub fn check_shortcut(&self, key: &str, modifiers: &InputModifiers) -> Option<&ShortcutAction> {
        let shortcut_key = ShortcutKey {
            key: key.to_string(),
            modifiers: ShortcutModifiers {
                shift: modifiers.shift,
                ctrl: modifiers.ctrl,
                alt: modifiers.alt,
                logo: modifiers.logo,
            },
        };

        self.global_shortcuts.get(&shortcut_key)
    }

    /// Register a custom global shortcut
    pub fn register_shortcut(&mut self, key: ShortcutKey, action: ShortcutAction) {
        self.global_shortcuts.insert(key, action);
    }
}

impl Default for InputManager {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_input_manager_creation() {
        let manager = InputManager::new();
        assert!(matches!(manager.focus_target(), InputTarget::Shell));
    }

    #[test]
    fn test_focus_stack() {
        let mut manager = InputManager::new();

        // Initial focus is shell
        assert!(matches!(manager.focus_target(), InputTarget::Shell));

        // Push app focus
        manager.push_focus(InputTarget::Compositor);
        assert!(matches!(manager.focus_target(), InputTarget::Compositor));

        // Pop returns to shell
        manager.pop_focus();
        assert!(matches!(manager.focus_target(), InputTarget::Shell));
    }

    #[test]
    fn test_hit_testing() {
        let mut manager = InputManager::new();

        // Register a target
        let id = InputTargetId::new();
        manager.register_target(InputTargetInfo {
            id,
            target: InputTarget::Compositor,
            bounds: (100.0, 100.0, 200.0, 200.0),
            accepts_keyboard: true,
            accepts_pointer: true,
            accepts_touch: true,
            z_order: 1,
        });

        // Hit test inside bounds
        let target = manager.hit_test(150.0, 150.0);
        assert!(matches!(target, InputTarget::Compositor));

        // Hit test outside bounds
        let target = manager.hit_test(50.0, 50.0);
        assert!(matches!(target, InputTarget::Shell)); // Falls back to focus
    }

    #[test]
    fn test_unified_button_state() {
        assert_eq!(
            UnifiedButtonState::from(ElementState::Pressed),
            UnifiedButtonState::Pressed
        );
        assert_eq!(
            UnifiedButtonState::from(ElementState::Released),
            UnifiedButtonState::Released
        );
    }
}
