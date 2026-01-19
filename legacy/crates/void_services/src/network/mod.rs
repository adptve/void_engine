//! # Network Module
//!
//! Provides world state synchronization, entity replication, and networked
//! communication for the Metaverse OS.
//!
//! ## Architecture
//!
//! ```text
//! +-------------+       +-------------+       +-------------+
//! |  Client A   |<----->|   World     |<----->|  Client B   |
//! |  (void_os)  |       |   Server    |       |  (void_os)  |
//! +-------------+       +-------------+       +-------------+
//!        |                    |                     |
//!        v                    v                     v
//! +-------------------------------------------------------+
//! |              State Synchronization Layer               |
//! +-------------------------------------------------------+
//! ```
//!
//! ## Key Components
//!
//! - [`WorldConnection`] - Manages connection to a world server
//! - [`EntityReplicator`] - Synchronizes entity state across nodes
//! - [`Transport`] - Abstract transport layer (WebSocket, QUIC, etc.)
//! - [`WorldRegistry`] - Discovers and joins available worlds
//! - [`ConsistencyConfig`] - Per-component consistency settings
//!
//! ## Usage
//!
//! ```ignore
//! use void_services::network::{NetworkService, WorldConnection};
//!
//! let mut network = NetworkService::new();
//! network.start().await?;
//!
//! // Connect to a world
//! let connection = network.connect("wss://world.example.com").await?;
//!
//! // Replicate entities
//! connection.replicator().claim_authority(my_entity);
//! connection.replicator().replicate_component(my_entity, &position);
//! ```

pub mod connection;
pub mod consistency;
pub mod discovery;
pub mod replication;
pub mod transport;

use std::any::Any;
use std::collections::HashMap;
use std::sync::Arc;
use std::time::{Duration, Instant};

use parking_lot::RwLock;
use thiserror::Error;
use tokio::sync::mpsc;

use crate::service::{Service, ServiceConfig, ServiceHealth, ServiceId, ServiceResult, ServiceState};

pub use connection::{ConnectionState, WorldConnection};
pub use consistency::{ConsistencyConfig, ConsistencyLevel};
pub use discovery::{WorldConfig, WorldMetadata, WorldRegistry, WorldServerInfo};
pub use replication::{Authority, ComponentUpdate, EntityReplicator, EntityUpdate, ReplicatedEntity};
pub use transport::{Transport, TransportError, WebSocketTransport};

/// Network-related errors
#[derive(Debug, Error)]
pub enum NetworkError {
    #[error("Not connected to any world")]
    NotConnected,

    #[error("Connection failed: {0}")]
    ConnectionFailed(String),

    #[error("Authentication failed: {0}")]
    AuthFailed(String),

    #[error("Transport error: {0}")]
    Transport(#[from] TransportError),

    #[error("Serialization error: {0}")]
    Serialization(String),

    #[error("Timeout waiting for response")]
    Timeout,

    #[error("World not found: {0}")]
    WorldNotFound(String),

    #[error("Max reconnect attempts exceeded")]
    MaxReconnectAttempts,

    #[error("Invalid message format")]
    InvalidMessage,

    #[error("Protocol version mismatch: expected {expected}, got {actual}")]
    ProtocolMismatch { expected: u32, actual: u32 },

    #[error("Internal error: {0}")]
    Internal(String),
}

pub type NetworkResult<T> = Result<T, NetworkError>;

/// Network service configuration
#[derive(Debug, Clone)]
pub struct NetworkServiceConfig {
    /// Base service configuration
    pub service: ServiceConfig,
    /// Maximum reconnection attempts before giving up
    pub max_reconnect_attempts: u32,
    /// Initial reconnect delay (doubles with each attempt)
    pub initial_reconnect_delay: Duration,
    /// Maximum reconnect delay
    pub max_reconnect_delay: Duration,
    /// Keep-alive ping interval
    pub ping_interval: Duration,
    /// Connection timeout
    pub connection_timeout: Duration,
    /// Authentication timeout
    pub auth_timeout: Duration,
    /// Default consistency configuration
    pub consistency: ConsistencyConfig,
    /// Enable compression for large messages
    pub enable_compression: bool,
    /// Compression threshold in bytes
    pub compression_threshold: usize,
    /// Maximum message size
    pub max_message_size: usize,
}

impl Default for NetworkServiceConfig {
    fn default() -> Self {
        Self {
            service: ServiceConfig::new("network"),
            max_reconnect_attempts: 5,
            initial_reconnect_delay: Duration::from_millis(500),
            max_reconnect_delay: Duration::from_secs(30),
            ping_interval: Duration::from_secs(15),
            connection_timeout: Duration::from_secs(10),
            auth_timeout: Duration::from_secs(30),
            consistency: ConsistencyConfig::default(),
            enable_compression: true,
            compression_threshold: 1024,
            max_message_size: 1024 * 1024, // 1MB
        }
    }
}

/// Network update types that flow through the kernel
#[derive(Debug, Clone)]
pub enum NetworkUpdate {
    /// Entity updates from the server
    EntityUpdates(Vec<EntityUpdate>),
    /// World event (chat, notifications, etc.)
    WorldEvent(WorldEvent),
    /// Connection state changed
    StateChanged(ConnectionState),
    /// Disconnected from server
    Disconnected { reason: String },
}

/// World events that can be received from the network
#[derive(Debug, Clone)]
pub enum WorldEvent {
    /// Player joined the world
    PlayerJoined {
        player_id: String,
        display_name: String,
    },
    /// Player left the world
    PlayerLeft { player_id: String },
    /// Chat message
    Chat {
        sender_id: String,
        sender_name: String,
        message: String,
        channel: String,
    },
    /// Custom event
    Custom { event_type: String, data: Vec<u8> },
}

/// Statistics for network operations
#[derive(Debug, Clone, Default)]
pub struct NetworkStats {
    /// Total bytes sent
    pub bytes_sent: u64,
    /// Total bytes received
    pub bytes_received: u64,
    /// Total packets sent
    pub packets_sent: u64,
    /// Total packets received
    pub packets_received: u64,
    /// Current round-trip time in milliseconds
    pub rtt_ms: f64,
    /// Average round-trip time
    pub avg_rtt_ms: f64,
    /// Packet loss percentage
    pub packet_loss_percent: f32,
    /// Time of last successful communication
    pub last_communication: Option<Instant>,
    /// Number of reconnection attempts
    pub reconnect_attempts: u32,
}

/// The main network service
///
/// Manages world connections, entity replication, and network communication.
pub struct NetworkService {
    /// Configuration
    config: NetworkServiceConfig,
    /// Current service state
    state: ServiceState,
    /// Active world connection
    connection: Option<WorldConnection>,
    /// World registry for discovery
    registry: Arc<RwLock<WorldRegistry>>,
    /// Network statistics
    stats: NetworkStats,
    /// Channel for network updates
    update_sender: Option<mpsc::UnboundedSender<NetworkUpdate>>,
    /// Pending outgoing updates
    outgoing_queue: Vec<EntityUpdate>,
}

impl NetworkService {
    /// Create a new network service with default configuration
    pub fn new() -> Self {
        Self::with_config(NetworkServiceConfig::default())
    }

    /// Create a new network service with custom configuration
    pub fn with_config(config: NetworkServiceConfig) -> Self {
        Self {
            config,
            state: ServiceState::Stopped,
            connection: None,
            registry: Arc::new(RwLock::new(WorldRegistry::new())),
            stats: NetworkStats::default(),
            update_sender: None,
            outgoing_queue: Vec::new(),
        }
    }

    /// Subscribe to network updates
    pub fn subscribe(&mut self) -> mpsc::UnboundedReceiver<NetworkUpdate> {
        let (tx, rx) = mpsc::unbounded_channel();
        self.update_sender = Some(tx);
        rx
    }

    /// Connect to a world server
    pub async fn connect(&mut self, address: &str) -> NetworkResult<()> {
        if self.connection.is_some() {
            self.disconnect().await?;
        }

        let mut connection = WorldConnection::new(
            address.to_string(),
            self.config.consistency.clone(),
        );

        connection
            .connect(
                self.config.connection_timeout,
                self.config.auth_timeout,
            )
            .await?;

        self.connection = Some(connection);

        // Notify subscribers
        if let Some(sender) = &self.update_sender {
            let _ = sender.send(NetworkUpdate::StateChanged(ConnectionState::Connected {
                session_id: "connected".to_string(),
            }));
        }

        Ok(())
    }

    /// Disconnect from the current world
    pub async fn disconnect(&mut self) -> NetworkResult<()> {
        if let Some(mut connection) = self.connection.take() {
            connection.disconnect().await?;

            if let Some(sender) = &self.update_sender {
                let _ = sender.send(NetworkUpdate::StateChanged(
                    ConnectionState::Disconnected,
                ));
            }
        }
        Ok(())
    }

    /// Check if connected to a world
    pub fn is_connected(&self) -> bool {
        self.connection
            .as_ref()
            .map(|c| c.is_connected())
            .unwrap_or(false)
    }

    /// Get the current connection state
    pub fn connection_state(&self) -> ConnectionState {
        self.connection
            .as_ref()
            .map(|c| c.state())
            .unwrap_or(ConnectionState::Disconnected)
    }

    /// Get a reference to the active connection
    pub fn connection(&self) -> Option<&WorldConnection> {
        self.connection.as_ref()
    }

    /// Get a mutable reference to the active connection
    pub fn connection_mut(&mut self) -> Option<&mut WorldConnection> {
        self.connection.as_mut()
    }

    /// Get the entity replicator
    pub fn replicator(&self) -> Option<&EntityReplicator> {
        self.connection.as_ref().map(|c| c.replicator())
    }

    /// Get a mutable reference to the entity replicator
    pub fn replicator_mut(&mut self) -> Option<&mut EntityReplicator> {
        self.connection.as_mut().map(|c| c.replicator_mut())
    }

    /// Get the world registry
    pub fn registry(&self) -> &Arc<RwLock<WorldRegistry>> {
        &self.registry
    }

    /// Get network statistics
    pub fn stats(&self) -> &NetworkStats {
        &self.stats
    }

    /// Queue an entity update for sending
    pub fn queue_update(&mut self, update: EntityUpdate) {
        self.outgoing_queue.push(update);
    }

    /// Poll for incoming updates (non-blocking)
    pub fn poll_updates(&mut self) -> Vec<EntityUpdate> {
        if let Some(connection) = &mut self.connection {
            connection.poll_incoming()
        } else {
            Vec::new()
        }
    }

    /// Flush outgoing updates
    pub async fn flush(&mut self) -> NetworkResult<()> {
        if let Some(connection) = &mut self.connection {
            let updates = std::mem::take(&mut self.outgoing_queue);
            for update in updates {
                connection.send_update(update).await?;
            }
        }
        Ok(())
    }

    /// Process network tick (call once per frame)
    pub async fn tick(&mut self) -> NetworkResult<()> {
        // Check reconnection needs first
        let needs_reconnect = self.connection.as_ref().map(|c| c.needs_reconnect()).unwrap_or(false);

        if needs_reconnect {
            if self.stats.reconnect_attempts < self.config.max_reconnect_attempts {
                self.stats.reconnect_attempts += 1;
                let delay = self.calculate_backoff_delay();
                tokio::time::sleep(delay).await;

                let conn_timeout = self.config.connection_timeout;
                let auth_timeout = self.config.auth_timeout;

                if let Some(connection) = &mut self.connection {
                    if let Err(e) = connection.reconnect(conn_timeout, auth_timeout).await {
                        log::warn!("Reconnection attempt {} failed: {}", self.stats.reconnect_attempts, e);
                    } else {
                        self.stats.reconnect_attempts = 0;
                    }
                }
            } else {
                return Err(NetworkError::MaxReconnectAttempts);
            }
        }

        // Process incoming messages
        if let Some(connection) = &mut self.connection {
            connection.process_incoming().await?;
        }

        // Send outgoing updates
        self.flush().await?;

        // Update statistics - extract stats first to avoid borrow conflict
        if let Some(connection) = &self.connection {
            let conn_stats = connection.stats();
            self.stats.bytes_sent = conn_stats.bytes_sent;
            self.stats.bytes_received = conn_stats.bytes_received;
            self.stats.packets_sent = conn_stats.packets_sent;
            self.stats.packets_received = conn_stats.packets_received;
            self.stats.rtt_ms = conn_stats.rtt_ms;
            self.stats.avg_rtt_ms = conn_stats.avg_rtt_ms;
            self.stats.last_communication = Some(Instant::now());
        }

        Ok(())
    }

    /// Calculate exponential backoff delay
    fn calculate_backoff_delay(&self) -> Duration {
        let multiplier = 2u32.pow(self.stats.reconnect_attempts.saturating_sub(1));
        let delay = self.config.initial_reconnect_delay * multiplier;
        std::cmp::min(delay, self.config.max_reconnect_delay)
    }

    /// Update network statistics from connection
    fn update_stats(&mut self, connection: &WorldConnection) {
        let conn_stats = connection.stats();
        self.stats.bytes_sent = conn_stats.bytes_sent;
        self.stats.bytes_received = conn_stats.bytes_received;
        self.stats.packets_sent = conn_stats.packets_sent;
        self.stats.packets_received = conn_stats.packets_received;
        self.stats.rtt_ms = conn_stats.rtt_ms;
        self.stats.avg_rtt_ms = conn_stats.avg_rtt_ms;
        self.stats.last_communication = Some(Instant::now());
    }
}

impl Default for NetworkService {
    fn default() -> Self {
        Self::new()
    }
}

impl Service for NetworkService {
    fn id(&self) -> &ServiceId {
        &self.config.service.id
    }

    fn state(&self) -> ServiceState {
        self.state
    }

    fn health(&self) -> ServiceHealth {
        if self.state == ServiceState::Running {
            let mut health = if self.is_connected() {
                ServiceHealth::healthy()
            } else {
                ServiceHealth::degraded("Not connected to any world")
            };

            health.metrics.insert("connected".to_string(), if self.is_connected() { 1.0 } else { 0.0 });
            health.metrics.insert("rtt_ms".to_string(), self.stats.rtt_ms);
            health.metrics.insert("bytes_sent".to_string(), self.stats.bytes_sent as f64);
            health.metrics.insert("bytes_received".to_string(), self.stats.bytes_received as f64);

            health
        } else {
            ServiceHealth::default()
        }
    }

    fn config(&self) -> &ServiceConfig {
        &self.config.service
    }

    fn start(&mut self) -> ServiceResult<()> {
        self.state = ServiceState::Running;
        log::info!("Network service started");
        Ok(())
    }

    fn stop(&mut self) -> ServiceResult<()> {
        // We can't call async disconnect here, so just drop the connection
        self.connection = None;
        self.state = ServiceState::Stopped;
        log::info!("Network service stopped");
        Ok(())
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_network_service_creation() {
        let service = NetworkService::new();
        assert_eq!(service.state(), ServiceState::Stopped);
        assert!(!service.is_connected());
    }

    #[test]
    fn test_network_service_config() {
        let config = NetworkServiceConfig::default();
        assert_eq!(config.max_reconnect_attempts, 5);
        assert!(config.enable_compression);
    }

    #[test]
    fn test_backoff_calculation() {
        let mut service = NetworkService::new();

        // First attempt: 500ms
        service.stats.reconnect_attempts = 1;
        let delay = service.calculate_backoff_delay();
        assert_eq!(delay, Duration::from_millis(500));

        // Second attempt: 1000ms
        service.stats.reconnect_attempts = 2;
        let delay = service.calculate_backoff_delay();
        assert_eq!(delay, Duration::from_millis(1000));

        // Third attempt: 2000ms
        service.stats.reconnect_attempts = 3;
        let delay = service.calculate_backoff_delay();
        assert_eq!(delay, Duration::from_millis(2000));
    }

    #[test]
    fn test_service_lifecycle() {
        let mut service = NetworkService::new();

        assert_eq!(service.state(), ServiceState::Stopped);

        service.start().unwrap();
        assert_eq!(service.state(), ServiceState::Running);

        service.stop().unwrap();
        assert_eq!(service.state(), ServiceState::Stopped);
    }
}
