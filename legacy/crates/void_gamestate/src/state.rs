//! Game state machine

use serde::{Deserialize, Serialize};

/// Game states
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum GameState {
    /// Initial loading
    Loading,
    /// Main menu
    MainMenu,
    /// In-game playing
    Playing,
    /// Game paused
    Paused,
    /// In a cutscene
    Cutscene,
    /// In dialogue/conversation
    Dialogue,
    /// Viewing inventory/character screen
    Inventory,
    /// In settings menu
    Settings,
    /// Game over screen
    GameOver,
    /// Victory/credits screen
    Victory,
    /// Level transition/loading
    Transition,
    /// Custom state
    Custom(u32),
}

impl Default for GameState {
    fn default() -> Self {
        Self::Loading
    }
}

impl GameState {
    /// Check if game is actively playing (not menu/paused)
    pub fn is_playing(&self) -> bool {
        matches!(self, Self::Playing | Self::Cutscene | Self::Dialogue)
    }

    /// Check if input should be processed
    pub fn accepts_gameplay_input(&self) -> bool {
        matches!(self, Self::Playing)
    }

    /// Check if game should be paused
    pub fn is_paused(&self) -> bool {
        matches!(
            self,
            Self::Paused | Self::Inventory | Self::Settings | Self::MainMenu
        )
    }

    /// Check if this is a menu state
    pub fn is_menu(&self) -> bool {
        matches!(
            self,
            Self::MainMenu | Self::Paused | Self::Settings | Self::GameOver | Self::Victory
        )
    }
}

/// State transition type
#[derive(Debug, Clone)]
pub struct StateTransition {
    /// Previous state
    pub from: GameState,
    /// New state
    pub to: GameState,
    /// Optional transition data
    pub data: Option<String>,
}

impl StateTransition {
    /// Create a new transition
    pub fn new(from: GameState, to: GameState) -> Self {
        Self {
            from,
            to,
            data: None,
        }
    }

    /// Add transition data
    pub fn with_data(mut self, data: impl Into<String>) -> Self {
        self.data = Some(data.into());
        self
    }
}

/// Game state manager with stack-based states
#[derive(Debug, Default)]
pub struct GameStateManager {
    /// State stack (current state is last element)
    stack: Vec<GameState>,
    /// Whether a transition is in progress
    transitioning: bool,
    /// Pending transitions
    pending: Vec<StateTransition>,
    /// State change callbacks (stored transitions for history)
    history: Vec<StateTransition>,
}

impl GameStateManager {
    /// Create a new game state manager
    pub fn new() -> Self {
        Self {
            stack: vec![GameState::Loading],
            transitioning: false,
            pending: Vec::new(),
            history: Vec::new(),
        }
    }

    /// Get current state
    pub fn current(&self) -> GameState {
        self.stack.last().copied().unwrap_or(GameState::Loading)
    }

    /// Get previous state (if any)
    pub fn previous(&self) -> Option<GameState> {
        if self.stack.len() >= 2 {
            self.stack.get(self.stack.len() - 2).copied()
        } else {
            None
        }
    }

    /// Get full state stack
    pub fn stack(&self) -> &[GameState] {
        &self.stack
    }

    /// Push a new state onto the stack
    pub fn push_state(&mut self, state: GameState) {
        let from = self.current();
        self.stack.push(state);

        let transition = StateTransition::new(from, state);
        self.history.push(transition.clone());
        self.pending.push(transition);
    }

    /// Pop the current state (return to previous)
    pub fn pop_state(&mut self) -> Option<GameState> {
        if self.stack.len() > 1 {
            let from = self.stack.pop()?;
            let to = self.current();

            let transition = StateTransition::new(from, to);
            self.history.push(transition.clone());
            self.pending.push(transition);

            Some(from)
        } else {
            None
        }
    }

    /// Replace current state
    pub fn set_state(&mut self, state: GameState) {
        let from = self.current();
        if let Some(last) = self.stack.last_mut() {
            *last = state;
        } else {
            self.stack.push(state);
        }

        let transition = StateTransition::new(from, state);
        self.history.push(transition.clone());
        self.pending.push(transition);
    }

    /// Clear stack and set initial state
    pub fn reset(&mut self, initial: GameState) {
        let from = self.current();
        self.stack.clear();
        self.stack.push(initial);

        let transition = StateTransition::new(from, initial);
        self.history.push(transition.clone());
        self.pending.push(transition);
    }

    /// Check if a specific state is in the stack
    pub fn has_state(&self, state: GameState) -> bool {
        self.stack.contains(&state)
    }

    /// Get stack depth
    pub fn depth(&self) -> usize {
        self.stack.len()
    }

    /// Check if currently transitioning
    pub fn is_transitioning(&self) -> bool {
        self.transitioning
    }

    /// Set transitioning flag
    pub fn set_transitioning(&mut self, transitioning: bool) {
        self.transitioning = transitioning;
    }

    /// Drain pending transitions
    pub fn drain_transitions(&mut self) -> Vec<StateTransition> {
        std::mem::take(&mut self.pending)
    }

    /// Get transition history
    pub fn history(&self) -> &[StateTransition] {
        &self.history
    }

    /// Clear history
    pub fn clear_history(&mut self) {
        self.history.clear();
    }

    // Common state transitions

    /// Start the game (from menu to playing)
    pub fn start_game(&mut self) {
        self.reset(GameState::Playing);
    }

    /// Pause the game
    pub fn pause(&mut self) {
        if self.current() == GameState::Playing {
            self.push_state(GameState::Paused);
        }
    }

    /// Resume from pause
    pub fn resume(&mut self) {
        if self.current() == GameState::Paused {
            self.pop_state();
        }
    }

    /// Open inventory
    pub fn open_inventory(&mut self) {
        if self.current() == GameState::Playing {
            self.push_state(GameState::Inventory);
        }
    }

    /// Close inventory
    pub fn close_inventory(&mut self) {
        if self.current() == GameState::Inventory {
            self.pop_state();
        }
    }

    /// Trigger game over
    pub fn game_over(&mut self) {
        self.set_state(GameState::GameOver);
    }

    /// Return to main menu
    pub fn return_to_menu(&mut self) {
        self.reset(GameState::MainMenu);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_state_manager() {
        let mut manager = GameStateManager::new();

        assert_eq!(manager.current(), GameState::Loading);

        manager.set_state(GameState::MainMenu);
        assert_eq!(manager.current(), GameState::MainMenu);
    }

    #[test]
    fn test_state_stack() {
        let mut manager = GameStateManager::new();

        manager.set_state(GameState::Playing);
        manager.push_state(GameState::Paused);

        assert_eq!(manager.current(), GameState::Paused);
        assert_eq!(manager.previous(), Some(GameState::Playing));

        manager.pop_state();
        assert_eq!(manager.current(), GameState::Playing);
    }

    #[test]
    fn test_common_transitions() {
        let mut manager = GameStateManager::new();

        manager.set_state(GameState::MainMenu);
        manager.start_game();
        assert_eq!(manager.current(), GameState::Playing);

        manager.pause();
        assert_eq!(manager.current(), GameState::Paused);

        manager.resume();
        assert_eq!(manager.current(), GameState::Playing);

        manager.game_over();
        assert_eq!(manager.current(), GameState::GameOver);

        manager.return_to_menu();
        assert_eq!(manager.current(), GameState::MainMenu);
    }

    #[test]
    fn test_state_properties() {
        assert!(GameState::Playing.is_playing());
        assert!(!GameState::Paused.is_playing());

        assert!(GameState::Playing.accepts_gameplay_input());
        assert!(!GameState::Cutscene.accepts_gameplay_input());

        assert!(GameState::MainMenu.is_menu());
        assert!(!GameState::Playing.is_menu());
    }
}
