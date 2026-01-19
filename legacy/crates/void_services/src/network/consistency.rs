//! Consistency Configuration
//!
//! Defines consistency levels and per-component configuration for
//! distributed state synchronization.

use std::collections::HashMap;

use serde::{Deserialize, Serialize};

use super::replication::ComponentTypeId;

/// Consistency levels for distributed state
///
/// Different data types require different consistency guarantees.
/// This enum allows fine-grained control over synchronization behavior.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum ConsistencyLevel {
    /// Eventually consistent - low latency, may see stale data
    ///
    /// Best for: position updates, animations, visual effects
    ///
    /// Characteristics:
    /// - Updates propagate asynchronously
    /// - No ordering guarantees
    /// - May temporarily diverge between nodes
    /// - Lowest latency, highest throughput
    Eventual,

    /// Causal consistency - respects causality, still fast
    ///
    /// Best for: chat messages, action sequences, state machines
    ///
    /// Characteristics:
    /// - If A happens before B, all nodes see A before B
    /// - Concurrent operations may be seen in different orders
    /// - Uses vector clocks or similar mechanism
    Causal,

    /// Strong consistency - all nodes see same state
    ///
    /// Best for: inventory, transactions, scoring
    ///
    /// Characteristics:
    /// - All nodes agree on order of operations
    /// - Higher latency due to consensus
    /// - Prevents anomalies like double-spending
    Strong,

    /// Server authoritative - server always wins
    ///
    /// Best for: anti-cheat, game rules, permissions
    ///
    /// Characteristics:
    /// - Client predictions are overwritten by server
    /// - Server state is always correct
    /// - Best for security-critical data
    ServerAuthoritative,
}

impl ConsistencyLevel {
    /// Get the priority of this consistency level (higher = stricter)
    pub fn priority(&self) -> u8 {
        match self {
            ConsistencyLevel::Eventual => 0,
            ConsistencyLevel::Causal => 1,
            ConsistencyLevel::Strong => 2,
            ConsistencyLevel::ServerAuthoritative => 3,
        }
    }

    /// Check if this level requires ordering guarantees
    pub fn requires_ordering(&self) -> bool {
        matches!(self, ConsistencyLevel::Causal | ConsistencyLevel::Strong)
    }

    /// Check if this level requires consensus
    pub fn requires_consensus(&self) -> bool {
        matches!(self, ConsistencyLevel::Strong)
    }

    /// Check if server is always authoritative
    pub fn is_server_authoritative(&self) -> bool {
        matches!(self, ConsistencyLevel::ServerAuthoritative)
    }

    /// Get human-readable description
    pub fn description(&self) -> &'static str {
        match self {
            ConsistencyLevel::Eventual => "Eventually consistent (fast, may see stale data)",
            ConsistencyLevel::Causal => "Causally consistent (respects order of related events)",
            ConsistencyLevel::Strong => "Strongly consistent (all nodes agree)",
            ConsistencyLevel::ServerAuthoritative => "Server authoritative (server always wins)",
        }
    }

    /// Get recommended use cases
    pub fn use_cases(&self) -> &'static [&'static str] {
        match self {
            ConsistencyLevel::Eventual => &[
                "Position updates",
                "Animation states",
                "Visual effects",
                "Camera movement",
                "Audio states",
            ],
            ConsistencyLevel::Causal => &[
                "Chat messages",
                "Action sequences",
                "State machines",
                "Event logs",
                "Undo/redo stacks",
            ],
            ConsistencyLevel::Strong => &[
                "Inventory",
                "Currency/tokens",
                "Score/leaderboards",
                "Trades",
                "Purchases",
            ],
            ConsistencyLevel::ServerAuthoritative => &[
                "Health/damage",
                "Permissions",
                "Anti-cheat data",
                "Game rules",
                "World state",
            ],
        }
    }
}

impl Default for ConsistencyLevel {
    fn default() -> Self {
        ConsistencyLevel::Eventual
    }
}

impl std::fmt::Display for ConsistencyLevel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ConsistencyLevel::Eventual => write!(f, "Eventual"),
            ConsistencyLevel::Causal => write!(f, "Causal"),
            ConsistencyLevel::Strong => write!(f, "Strong"),
            ConsistencyLevel::ServerAuthoritative => write!(f, "ServerAuthoritative"),
        }
    }
}

/// Configuration for consistency levels
///
/// Allows setting a default level and per-component overrides.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConsistencyConfig {
    /// Default consistency level for unlisted components
    pub default: ConsistencyLevel,

    /// Per-component type overrides
    #[serde(default)]
    pub overrides: HashMap<ComponentTypeId, ConsistencyLevel>,

    /// Maximum age (ms) before eventual-consistent data is considered stale
    #[serde(default = "default_stale_threshold_ms")]
    pub stale_threshold_ms: u64,

    /// Whether to enable conflict detection
    #[serde(default)]
    pub enable_conflict_detection: bool,

    /// Strategy for resolving conflicts
    #[serde(default)]
    pub conflict_resolution: ConflictResolution,
}

fn default_stale_threshold_ms() -> u64 {
    5000 // 5 seconds
}

impl Default for ConsistencyConfig {
    fn default() -> Self {
        Self {
            default: ConsistencyLevel::Eventual,
            overrides: HashMap::new(),
            stale_threshold_ms: default_stale_threshold_ms(),
            enable_conflict_detection: true,
            conflict_resolution: ConflictResolution::default(),
        }
    }
}

impl ConsistencyConfig {
    /// Create a new configuration with a default level
    pub fn new(default: ConsistencyLevel) -> Self {
        Self {
            default,
            ..Default::default()
        }
    }

    /// Create configuration with metaverse defaults
    ///
    /// Pre-configured for common metaverse component types.
    pub fn metaverse_defaults() -> Self {
        let mut overrides = HashMap::new();

        // Fast updates for movement (eventual)
        overrides.insert(
            ComponentTypeId::from_name("Position"),
            ConsistencyLevel::Eventual,
        );
        overrides.insert(
            ComponentTypeId::from_name("Velocity"),
            ConsistencyLevel::Eventual,
        );
        overrides.insert(
            ComponentTypeId::from_name("Rotation"),
            ConsistencyLevel::Eventual,
        );
        overrides.insert(
            ComponentTypeId::from_name("Animation"),
            ConsistencyLevel::Eventual,
        );
        overrides.insert(
            ComponentTypeId::from_name("Transform"),
            ConsistencyLevel::Eventual,
        );

        // Causal for interactions
        overrides.insert(
            ComponentTypeId::from_name("ChatMessage"),
            ConsistencyLevel::Causal,
        );
        overrides.insert(
            ComponentTypeId::from_name("Action"),
            ConsistencyLevel::Causal,
        );
        overrides.insert(
            ComponentTypeId::from_name("Interaction"),
            ConsistencyLevel::Causal,
        );

        // Strong for valuable data
        overrides.insert(
            ComponentTypeId::from_name("Inventory"),
            ConsistencyLevel::Strong,
        );
        overrides.insert(
            ComponentTypeId::from_name("Currency"),
            ConsistencyLevel::Strong,
        );
        overrides.insert(
            ComponentTypeId::from_name("Score"),
            ConsistencyLevel::Strong,
        );
        overrides.insert(
            ComponentTypeId::from_name("Trade"),
            ConsistencyLevel::Strong,
        );

        // Server authoritative for security
        overrides.insert(
            ComponentTypeId::from_name("Health"),
            ConsistencyLevel::ServerAuthoritative,
        );
        overrides.insert(
            ComponentTypeId::from_name("Permissions"),
            ConsistencyLevel::ServerAuthoritative,
        );
        overrides.insert(
            ComponentTypeId::from_name("Stats"),
            ConsistencyLevel::ServerAuthoritative,
        );
        overrides.insert(
            ComponentTypeId::from_name("WorldRules"),
            ConsistencyLevel::ServerAuthoritative,
        );

        Self {
            default: ConsistencyLevel::Eventual,
            overrides,
            stale_threshold_ms: 5000,
            enable_conflict_detection: true,
            conflict_resolution: ConflictResolution::LastWriterWins,
        }
    }

    /// Get the consistency level for a component type
    pub fn get(&self, type_id: ComponentTypeId) -> ConsistencyLevel {
        self.overrides.get(&type_id).copied().unwrap_or(self.default)
    }

    /// Set the consistency level for a component type
    pub fn set(&mut self, type_id: ComponentTypeId, level: ConsistencyLevel) {
        self.overrides.insert(type_id, level);
    }

    /// Remove an override (will use default level)
    pub fn remove(&mut self, type_id: &ComponentTypeId) {
        self.overrides.remove(type_id);
    }

    /// Set the default consistency level
    pub fn set_default(&mut self, level: ConsistencyLevel) {
        self.default = level;
    }

    /// Builder method to add an override
    pub fn with_override(mut self, type_id: ComponentTypeId, level: ConsistencyLevel) -> Self {
        self.overrides.insert(type_id, level);
        self
    }

    /// Builder method to set stale threshold
    pub fn with_stale_threshold(mut self, ms: u64) -> Self {
        self.stale_threshold_ms = ms;
        self
    }

    /// Builder method to set conflict resolution strategy
    pub fn with_conflict_resolution(mut self, strategy: ConflictResolution) -> Self {
        self.conflict_resolution = strategy;
        self
    }

    /// Check if a component type requires strict ordering
    pub fn requires_ordering(&self, type_id: ComponentTypeId) -> bool {
        self.get(type_id).requires_ordering()
    }

    /// Check if a component type requires consensus
    pub fn requires_consensus(&self, type_id: ComponentTypeId) -> bool {
        self.get(type_id).requires_consensus()
    }

    /// Get all component types with a specific consistency level
    pub fn components_with_level(&self, level: ConsistencyLevel) -> Vec<ComponentTypeId> {
        self.overrides
            .iter()
            .filter(|(_, l)| **l == level)
            .map(|(id, _)| *id)
            .collect()
    }

    /// Get statistics about the configuration
    pub fn stats(&self) -> ConsistencyStats {
        let mut level_counts = HashMap::new();

        for level in self.overrides.values() {
            *level_counts.entry(*level).or_insert(0) += 1;
        }

        ConsistencyStats {
            total_overrides: self.overrides.len(),
            eventual_count: level_counts.get(&ConsistencyLevel::Eventual).copied().unwrap_or(0),
            causal_count: level_counts.get(&ConsistencyLevel::Causal).copied().unwrap_or(0),
            strong_count: level_counts.get(&ConsistencyLevel::Strong).copied().unwrap_or(0),
            server_auth_count: level_counts
                .get(&ConsistencyLevel::ServerAuthoritative)
                .copied()
                .unwrap_or(0),
            default_level: self.default,
        }
    }
}

/// Conflict resolution strategies
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ConflictResolution {
    /// Last writer wins (based on timestamp)
    LastWriterWins,

    /// First writer wins (first update is kept)
    FirstWriterWins,

    /// Server value always wins
    ServerWins,

    /// Higher sequence number wins
    HigherSequenceWins,

    /// Merge values (for mergeable types like sets)
    Merge,

    /// Report conflict to application for manual resolution
    Report,
}

impl Default for ConflictResolution {
    fn default() -> Self {
        ConflictResolution::LastWriterWins
    }
}

impl ConflictResolution {
    /// Get human-readable description
    pub fn description(&self) -> &'static str {
        match self {
            ConflictResolution::LastWriterWins => "Latest timestamp wins",
            ConflictResolution::FirstWriterWins => "First update is kept",
            ConflictResolution::ServerWins => "Server value always wins",
            ConflictResolution::HigherSequenceWins => "Higher sequence number wins",
            ConflictResolution::Merge => "Values are merged",
            ConflictResolution::Report => "Conflict reported for manual resolution",
        }
    }
}

/// Statistics about consistency configuration
#[derive(Debug, Clone)]
pub struct ConsistencyStats {
    /// Total number of overrides
    pub total_overrides: usize,
    /// Number of eventual consistency overrides
    pub eventual_count: usize,
    /// Number of causal consistency overrides
    pub causal_count: usize,
    /// Number of strong consistency overrides
    pub strong_count: usize,
    /// Number of server authoritative overrides
    pub server_auth_count: usize,
    /// Default consistency level
    pub default_level: ConsistencyLevel,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_consistency_level_priority() {
        assert!(ConsistencyLevel::Eventual.priority() < ConsistencyLevel::Causal.priority());
        assert!(ConsistencyLevel::Causal.priority() < ConsistencyLevel::Strong.priority());
        assert!(
            ConsistencyLevel::Strong.priority() < ConsistencyLevel::ServerAuthoritative.priority()
        );
    }

    #[test]
    fn test_consistency_level_properties() {
        assert!(!ConsistencyLevel::Eventual.requires_ordering());
        assert!(ConsistencyLevel::Causal.requires_ordering());
        assert!(ConsistencyLevel::Strong.requires_ordering());

        assert!(!ConsistencyLevel::Eventual.requires_consensus());
        assert!(!ConsistencyLevel::Causal.requires_consensus());
        assert!(ConsistencyLevel::Strong.requires_consensus());

        assert!(!ConsistencyLevel::Eventual.is_server_authoritative());
        assert!(ConsistencyLevel::ServerAuthoritative.is_server_authoritative());
    }

    #[test]
    fn test_consistency_config_default() {
        let config = ConsistencyConfig::default();
        assert_eq!(config.default, ConsistencyLevel::Eventual);
        assert!(config.overrides.is_empty());
    }

    #[test]
    fn test_consistency_config_get() {
        let mut config = ConsistencyConfig::new(ConsistencyLevel::Causal);

        let position_id = ComponentTypeId::from_name("Position");
        let inventory_id = ComponentTypeId::from_name("Inventory");

        config.set(inventory_id, ConsistencyLevel::Strong);

        // Position uses default
        assert_eq!(config.get(position_id), ConsistencyLevel::Causal);
        // Inventory has override
        assert_eq!(config.get(inventory_id), ConsistencyLevel::Strong);
    }

    #[test]
    fn test_consistency_config_builder() {
        let config = ConsistencyConfig::new(ConsistencyLevel::Eventual)
            .with_override(ComponentTypeId::from_name("Health"), ConsistencyLevel::ServerAuthoritative)
            .with_stale_threshold(10000)
            .with_conflict_resolution(ConflictResolution::ServerWins);

        assert_eq!(
            config.get(ComponentTypeId::from_name("Health")),
            ConsistencyLevel::ServerAuthoritative
        );
        assert_eq!(config.stale_threshold_ms, 10000);
        assert_eq!(config.conflict_resolution, ConflictResolution::ServerWins);
    }

    #[test]
    fn test_metaverse_defaults() {
        let config = ConsistencyConfig::metaverse_defaults();

        assert_eq!(
            config.get(ComponentTypeId::from_name("Position")),
            ConsistencyLevel::Eventual
        );
        assert_eq!(
            config.get(ComponentTypeId::from_name("ChatMessage")),
            ConsistencyLevel::Causal
        );
        assert_eq!(
            config.get(ComponentTypeId::from_name("Inventory")),
            ConsistencyLevel::Strong
        );
        assert_eq!(
            config.get(ComponentTypeId::from_name("Health")),
            ConsistencyLevel::ServerAuthoritative
        );
    }

    #[test]
    fn test_consistency_config_stats() {
        let config = ConsistencyConfig::metaverse_defaults();
        let stats = config.stats();

        assert!(stats.total_overrides > 0);
        assert!(stats.eventual_count > 0);
        assert!(stats.causal_count > 0);
        assert!(stats.strong_count > 0);
        assert!(stats.server_auth_count > 0);
    }

    #[test]
    fn test_conflict_resolution() {
        assert_eq!(
            ConflictResolution::default(),
            ConflictResolution::LastWriterWins
        );

        assert!(!ConflictResolution::LastWriterWins.description().is_empty());
        assert!(!ConflictResolution::Merge.description().is_empty());
    }

    #[test]
    fn test_components_with_level() {
        let config = ConsistencyConfig::metaverse_defaults();

        let eventual = config.components_with_level(ConsistencyLevel::Eventual);
        let strong = config.components_with_level(ConsistencyLevel::Strong);

        assert!(!eventual.is_empty());
        assert!(!strong.is_empty());

        // Verify components are in correct categories
        assert!(eventual.contains(&ComponentTypeId::from_name("Position")));
        assert!(strong.contains(&ComponentTypeId::from_name("Inventory")));
    }
}
