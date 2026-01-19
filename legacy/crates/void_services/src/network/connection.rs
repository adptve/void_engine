//! World Connection
//!
//! Manages the connection lifecycle to a world server, including
//! authentication, reconnection, and state synchronization.

use std::collections::VecDeque;
use std::sync::Arc;
use std::time::{Duration, Instant};

use parking_lot::RwLock;
use serde::{Deserialize, Serialize};
use tokio::sync::mpsc;
use uuid::Uuid;

use super::consistency::ConsistencyConfig;
use super::replication::{EntityReplicator, EntityUpdate};
use super::transport::{Transport, TransportError, WebSocketTransport};
use super::{NetworkError, NetworkResult};

/// Unique identifier for a world
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct WorldId(Uuid);

impl WorldId {
    /// Create a new random world ID
    pub fn new() -> Self {
        Self(Uuid::new_v4())
    }

    /// Create from a UUID
    pub fn from_uuid(uuid: Uuid) -> Self {
        Self(uuid)
    }

    /// Parse from a string
    pub fn parse(s: &str) -> Result<Self, uuid::Error> {
        Ok(Self(Uuid::parse_str(s)?))
    }

    /// Get the underlying UUID
    pub fn as_uuid(&self) -> &Uuid {
        &self.0
    }
}

impl Default for WorldId {
    fn default() -> Self {
        Self::new()
    }
}

impl std::fmt::Display for WorldId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Server address with parsed components
#[derive(Debug, Clone)]
pub struct ServerAddress {
    /// Full URL
    pub url: String,
    /// Host portion
    pub host: String,
    /// Port number
    pub port: u16,
    /// Whether to use TLS
    pub secure: bool,
}

impl ServerAddress {
    /// Parse a server address from a URL string
    pub fn parse(address: &str) -> NetworkResult<Self> {
        let url = url::Url::parse(address)
            .map_err(|e| NetworkError::ConnectionFailed(format!("Invalid URL: {}", e)))?;

        let secure = url.scheme() == "wss" || url.scheme() == "https";
        let host = url
            .host_str()
            .ok_or_else(|| NetworkError::ConnectionFailed("Missing host".to_string()))?
            .to_string();
        let port = url.port().unwrap_or(if secure { 443 } else { 80 });

        Ok(Self {
            url: address.to_string(),
            host,
            port,
            secure,
        })
    }
}

/// Connection state machine
#[derive(Debug, Clone, PartialEq)]
pub enum ConnectionState {
    /// Not connected to any server
    Disconnected,

    /// Attempting to establish connection
    Connecting {
        /// Server being connected to
        server: String,
        /// When connection attempt started
        started_at: Instant,
    },

    /// Authenticating with the server
    Authenticating {
        /// Session being authenticated
        session_id: String,
    },

    /// Successfully connected and authenticated
    Connected {
        /// Active session ID
        session_id: String,
    },

    /// Connection lost, attempting to reconnect
    Reconnecting {
        /// Number of attempts made
        attempts: u32,
        /// Last error encountered
        last_error: String,
        /// When reconnection started
        started_at: Instant,
    },

    /// Connection has failed and won't retry
    Failed {
        /// Error that caused failure
        error: String,
        /// When failure occurred
        failed_at: Instant,
    },
}

impl ConnectionState {
    /// Check if the connection is active
    pub fn is_connected(&self) -> bool {
        matches!(self, Self::Connected { .. })
    }

    /// Check if currently trying to connect
    pub fn is_connecting(&self) -> bool {
        matches!(
            self,
            Self::Connecting { .. } | Self::Authenticating { .. } | Self::Reconnecting { .. }
        )
    }

    /// Check if connection has failed
    pub fn is_failed(&self) -> bool {
        matches!(self, Self::Failed { .. })
    }
}

/// Connection statistics
#[derive(Debug, Clone, Default)]
pub struct ConnectionStats {
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
    /// Average round-trip time (rolling average)
    pub avg_rtt_ms: f64,
    /// Minimum RTT observed
    pub min_rtt_ms: f64,
    /// Maximum RTT observed
    pub max_rtt_ms: f64,
    /// Number of RTT samples
    rtt_samples: u32,
}

impl ConnectionStats {
    /// Update RTT with a new sample
    pub fn update_rtt(&mut self, rtt_ms: f64) {
        self.rtt_ms = rtt_ms;
        self.rtt_samples += 1;

        if self.rtt_samples == 1 {
            self.avg_rtt_ms = rtt_ms;
            self.min_rtt_ms = rtt_ms;
            self.max_rtt_ms = rtt_ms;
        } else {
            // Exponential moving average
            const ALPHA: f64 = 0.2;
            self.avg_rtt_ms = ALPHA * rtt_ms + (1.0 - ALPHA) * self.avg_rtt_ms;
            self.min_rtt_ms = self.min_rtt_ms.min(rtt_ms);
            self.max_rtt_ms = self.max_rtt_ms.max(rtt_ms);
        }
    }

    /// Record bytes sent
    pub fn record_sent(&mut self, bytes: usize) {
        self.bytes_sent += bytes as u64;
        self.packets_sent += 1;
    }

    /// Record bytes received
    pub fn record_received(&mut self, bytes: usize) {
        self.bytes_received += bytes as u64;
        self.packets_received += 1;
    }
}

/// Protocol messages sent between client and server
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ProtocolMessage {
    /// Initial connection request
    Connect {
        /// Protocol version
        version: u32,
        /// Authentication token (if any)
        auth_token: Option<String>,
        /// Client identifier
        client_id: String,
    },

    /// Connection accepted
    ConnectAck {
        /// Assigned session ID
        session_id: String,
        /// Server protocol version
        server_version: u32,
        /// Server time for sync
        server_time: u64,
    },

    /// Connection rejected
    ConnectReject {
        /// Reason for rejection
        reason: String,
    },

    /// Entity state updates
    EntityUpdates(Vec<EntityUpdate>),

    /// Request full state sync
    SyncRequest {
        /// Last known sequence number
        last_sequence: u64,
    },

    /// Full state sync response
    SyncResponse {
        /// All current entities
        entities: Vec<EntityUpdate>,
        /// Current sequence number
        sequence: u64,
    },

    /// Keep-alive ping
    Ping {
        /// Timestamp for RTT calculation
        timestamp: u64,
        /// Client sequence number
        client_sequence: u64,
    },

    /// Keep-alive pong
    Pong {
        /// Echo of timestamp
        timestamp: u64,
        /// Server time
        server_time: u64,
        /// Server sequence number
        server_sequence: u64,
    },

    /// Graceful disconnect
    Disconnect {
        /// Reason for disconnect
        reason: String,
    },

    /// Error message
    Error {
        /// Error code
        code: u32,
        /// Error message
        message: String,
    },
}

/// Current protocol version
pub const PROTOCOL_VERSION: u32 = 1;

/// Manages a connection to a world server
pub struct WorldConnection {
    /// Server address
    address: ServerAddress,
    /// Current connection state
    state: ConnectionState,
    /// Active transport (if connected)
    transport: Option<Box<dyn Transport>>,
    /// Entity replicator
    replicator: EntityReplicator,
    /// Consistency configuration
    consistency_config: ConsistencyConfig,
    /// Connection statistics
    stats: ConnectionStats,
    /// Incoming message queue
    incoming: VecDeque<EntityUpdate>,
    /// Last ping timestamp for RTT
    last_ping_time: Option<Instant>,
    /// Last ping sequence
    last_ping_sequence: u64,
    /// Session ID (if connected)
    session_id: Option<String>,
    /// Client ID (unique per client instance)
    client_id: String,
    /// Last sequence number received
    last_received_sequence: u64,
}

impl WorldConnection {
    /// Create a new world connection
    pub fn new(address: String, consistency_config: ConsistencyConfig) -> Self {
        let parsed_address = ServerAddress::parse(&address)
            .unwrap_or_else(|_| ServerAddress {
                url: address.clone(),
                host: address.clone(),
                port: 443,
                secure: true,
            });

        Self {
            address: parsed_address,
            state: ConnectionState::Disconnected,
            transport: None,
            replicator: EntityReplicator::new(),
            consistency_config,
            stats: ConnectionStats::default(),
            incoming: VecDeque::new(),
            last_ping_time: None,
            last_ping_sequence: 0,
            session_id: None,
            client_id: Uuid::new_v4().to_string(),
            last_received_sequence: 0,
        }
    }

    /// Get the current connection state
    pub fn state(&self) -> ConnectionState {
        self.state.clone()
    }

    /// Check if connected
    pub fn is_connected(&self) -> bool {
        self.state.is_connected()
    }

    /// Check if connection needs reconnection
    pub fn needs_reconnect(&self) -> bool {
        matches!(self.state, ConnectionState::Reconnecting { .. })
    }

    /// Get the entity replicator
    pub fn replicator(&self) -> &EntityReplicator {
        &self.replicator
    }

    /// Get a mutable reference to the entity replicator
    pub fn replicator_mut(&mut self) -> &mut EntityReplicator {
        &mut self.replicator
    }

    /// Get connection statistics
    pub fn stats(&self) -> &ConnectionStats {
        &self.stats
    }

    /// Connect to the server
    pub async fn connect(
        &mut self,
        connection_timeout: Duration,
        auth_timeout: Duration,
    ) -> NetworkResult<()> {
        log::info!("Connecting to {}", self.address.url);

        self.state = ConnectionState::Connecting {
            server: self.address.url.clone(),
            started_at: Instant::now(),
        };

        // Establish transport
        let transport = self.establish_transport(connection_timeout).await?;
        self.transport = Some(transport);

        // Authenticate
        self.authenticate(auth_timeout).await?;

        Ok(())
    }

    /// Establish the transport connection
    async fn establish_transport(
        &self,
        timeout: Duration,
    ) -> NetworkResult<Box<dyn Transport>> {
        log::debug!("Establishing WebSocket transport to {}", self.address.url);

        let transport = tokio::time::timeout(timeout, async {
            WebSocketTransport::connect(&self.address.url).await
        })
        .await
        .map_err(|_| NetworkError::Timeout)?
        .map_err(NetworkError::Transport)?;

        log::debug!("WebSocket transport established");
        Ok(Box::new(transport))
    }

    /// Authenticate with the server
    async fn authenticate(&mut self, timeout: Duration) -> NetworkResult<()> {
        let transport = self
            .transport
            .as_mut()
            .ok_or(NetworkError::NotConnected)?;

        self.state = ConnectionState::Authenticating {
            session_id: String::new(),
        };

        // Send connect message
        let connect_msg = ProtocolMessage::Connect {
            version: PROTOCOL_VERSION,
            auth_token: None, // TODO: Add authentication token support
            client_id: self.client_id.clone(),
        };

        let msg_bytes = bincode::serialize(&connect_msg)
            .map_err(|e| NetworkError::Serialization(e.to_string()))?;

        transport.send_reliable(&msg_bytes).await?;
        self.stats.record_sent(msg_bytes.len());

        // Wait for response
        let response = tokio::time::timeout(timeout, transport.receive())
            .await
            .map_err(|_| NetworkError::Timeout)?
            .map_err(NetworkError::Transport)?;

        self.stats.record_received(response.len());

        let response_msg: ProtocolMessage = bincode::deserialize(&response)
            .map_err(|e| NetworkError::Serialization(e.to_string()))?;

        match response_msg {
            ProtocolMessage::ConnectAck {
                session_id,
                server_version,
                ..
            } => {
                if server_version != PROTOCOL_VERSION {
                    return Err(NetworkError::ProtocolMismatch {
                        expected: PROTOCOL_VERSION,
                        actual: server_version,
                    });
                }

                log::info!("Connected with session ID: {}", session_id);
                self.session_id = Some(session_id.clone());
                self.state = ConnectionState::Connected { session_id };
                Ok(())
            }
            ProtocolMessage::ConnectReject { reason } => {
                self.state = ConnectionState::Failed {
                    error: reason.clone(),
                    failed_at: Instant::now(),
                };
                Err(NetworkError::AuthFailed(reason))
            }
            ProtocolMessage::Error { message, .. } => {
                self.state = ConnectionState::Failed {
                    error: message.clone(),
                    failed_at: Instant::now(),
                };
                Err(NetworkError::ConnectionFailed(message))
            }
            _ => Err(NetworkError::InvalidMessage),
        }
    }

    /// Disconnect from the server
    pub async fn disconnect(&mut self) -> NetworkResult<()> {
        if let Some(ref mut transport) = self.transport {
            let disconnect_msg = ProtocolMessage::Disconnect {
                reason: "Client disconnect".to_string(),
            };

            if let Ok(msg_bytes) = bincode::serialize(&disconnect_msg) {
                let _ = transport.send_reliable(&msg_bytes).await;
            }

            transport.close().await?;
        }

        self.transport = None;
        self.session_id = None;
        self.state = ConnectionState::Disconnected;

        log::info!("Disconnected from {}", self.address.url);
        Ok(())
    }

    /// Attempt to reconnect after connection loss
    pub async fn reconnect(
        &mut self,
        connection_timeout: Duration,
        auth_timeout: Duration,
    ) -> NetworkResult<()> {
        let attempts = match &self.state {
            ConnectionState::Reconnecting { attempts, .. } => *attempts,
            _ => 0,
        };

        self.state = ConnectionState::Reconnecting {
            attempts: attempts + 1,
            last_error: String::new(),
            started_at: Instant::now(),
        };

        log::info!(
            "Reconnecting to {} (attempt {})",
            self.address.url,
            attempts + 1
        );

        // Clean up old transport
        if let Some(mut transport) = self.transport.take() {
            let _ = transport.close().await;
        }

        // Try to connect
        match self.connect(connection_timeout, auth_timeout).await {
            Ok(()) => {
                // Request state sync after reconnection
                self.request_sync().await?;
                Ok(())
            }
            Err(e) => {
                self.state = ConnectionState::Reconnecting {
                    attempts: attempts + 1,
                    last_error: e.to_string(),
                    started_at: Instant::now(),
                };
                Err(e)
            }
        }
    }

    /// Request a full state sync from the server
    async fn request_sync(&mut self) -> NetworkResult<()> {
        let transport = self
            .transport
            .as_mut()
            .ok_or(NetworkError::NotConnected)?;

        let sync_request = ProtocolMessage::SyncRequest {
            last_sequence: self.last_received_sequence,
        };

        let msg_bytes = bincode::serialize(&sync_request)
            .map_err(|e| NetworkError::Serialization(e.to_string()))?;

        transport.send_reliable(&msg_bytes).await?;
        self.stats.record_sent(msg_bytes.len());

        Ok(())
    }

    /// Send an entity update
    pub async fn send_update(&mut self, update: EntityUpdate) -> NetworkResult<()> {
        let transport = self
            .transport
            .as_mut()
            .ok_or(NetworkError::NotConnected)?;

        let message = ProtocolMessage::EntityUpdates(vec![update]);
        let msg_bytes = bincode::serialize(&message)
            .map_err(|e| NetworkError::Serialization(e.to_string()))?;

        transport.send_reliable(&msg_bytes).await?;
        self.stats.record_sent(msg_bytes.len());

        Ok(())
    }

    /// Send a batch of entity updates
    pub async fn send_updates(&mut self, updates: Vec<EntityUpdate>) -> NetworkResult<()> {
        if updates.is_empty() {
            return Ok(());
        }

        let transport = self
            .transport
            .as_mut()
            .ok_or(NetworkError::NotConnected)?;

        let message = ProtocolMessage::EntityUpdates(updates);
        let msg_bytes = bincode::serialize(&message)
            .map_err(|e| NetworkError::Serialization(e.to_string()))?;

        transport.send_reliable(&msg_bytes).await?;
        self.stats.record_sent(msg_bytes.len());

        Ok(())
    }

    /// Process incoming messages
    pub async fn process_incoming(&mut self) -> NetworkResult<()> {
        let transport = match self.transport.as_mut() {
            Some(t) => t,
            None => return Ok(()),
        };

        // Try to receive messages (non-blocking with timeout)
        let receive_result = tokio::time::timeout(
            Duration::from_millis(1),
            transport.receive(),
        )
        .await;

        match receive_result {
            Ok(Ok(data)) => {
                self.stats.record_received(data.len());
                self.handle_message(&data)?;
            }
            Ok(Err(TransportError::Closed)) => {
                self.handle_connection_lost("Connection closed by server".to_string());
            }
            Ok(Err(e)) => {
                log::warn!("Transport error: {}", e);
            }
            Err(_) => {
                // Timeout - no message available
            }
        }

        Ok(())
    }

    /// Handle a received message
    fn handle_message(&mut self, data: &[u8]) -> NetworkResult<()> {
        let message: ProtocolMessage = bincode::deserialize(data)
            .map_err(|e| NetworkError::Serialization(e.to_string()))?;

        match message {
            ProtocolMessage::EntityUpdates(updates) => {
                for update in updates {
                    self.last_received_sequence = self.last_received_sequence.max(update.sequence);
                    self.incoming.push_back(update);
                }
            }
            ProtocolMessage::SyncResponse { entities, sequence } => {
                self.last_received_sequence = sequence;
                for update in entities {
                    self.incoming.push_back(update);
                }
            }
            ProtocolMessage::Pong {
                timestamp,
                server_sequence,
                ..
            } => {
                if let Some(ping_time) = self.last_ping_time {
                    let rtt = ping_time.elapsed().as_secs_f64() * 1000.0;
                    self.stats.update_rtt(rtt);
                }
                self.last_received_sequence = self.last_received_sequence.max(server_sequence);
            }
            ProtocolMessage::Disconnect { reason } => {
                log::info!("Server disconnected: {}", reason);
                self.handle_connection_lost(reason);
            }
            ProtocolMessage::Error { message, .. } => {
                log::error!("Server error: {}", message);
            }
            _ => {}
        }

        Ok(())
    }

    /// Handle connection loss
    fn handle_connection_lost(&mut self, reason: String) {
        self.transport = None;
        self.state = ConnectionState::Reconnecting {
            attempts: 0,
            last_error: reason,
            started_at: Instant::now(),
        };
    }

    /// Poll for incoming entity updates (non-blocking)
    pub fn poll_incoming(&mut self) -> Vec<EntityUpdate> {
        self.incoming.drain(..).collect()
    }

    /// Send a ping to measure RTT
    pub async fn send_ping(&mut self) -> NetworkResult<()> {
        let transport = self
            .transport
            .as_mut()
            .ok_or(NetworkError::NotConnected)?;

        self.last_ping_sequence += 1;
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis() as u64;

        let ping = ProtocolMessage::Ping {
            timestamp,
            client_sequence: self.last_ping_sequence,
        };

        let msg_bytes = bincode::serialize(&ping)
            .map_err(|e| NetworkError::Serialization(e.to_string()))?;

        self.last_ping_time = Some(Instant::now());
        transport.send_reliable(&msg_bytes).await?;
        self.stats.record_sent(msg_bytes.len());

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_world_id() {
        let id1 = WorldId::new();
        let id2 = WorldId::new();
        assert_ne!(id1, id2);

        let id_str = id1.to_string();
        let parsed = WorldId::parse(&id_str).unwrap();
        assert_eq!(id1, parsed);
    }

    #[test]
    fn test_server_address_parse() {
        let addr = ServerAddress::parse("wss://example.com:8080/world").unwrap();
        assert_eq!(addr.host, "example.com");
        assert_eq!(addr.port, 8080);
        assert!(addr.secure);

        let addr2 = ServerAddress::parse("ws://localhost:9000").unwrap();
        assert_eq!(addr2.host, "localhost");
        assert_eq!(addr2.port, 9000);
        assert!(!addr2.secure);
    }

    #[test]
    fn test_connection_state() {
        let disconnected = ConnectionState::Disconnected;
        assert!(!disconnected.is_connected());
        assert!(!disconnected.is_connecting());

        let connected = ConnectionState::Connected {
            session_id: "test".to_string(),
        };
        assert!(connected.is_connected());
        assert!(!connected.is_connecting());

        let reconnecting = ConnectionState::Reconnecting {
            attempts: 1,
            last_error: String::new(),
            started_at: Instant::now(),
        };
        assert!(!reconnecting.is_connected());
        assert!(reconnecting.is_connecting());
    }

    #[test]
    fn test_connection_stats() {
        let mut stats = ConnectionStats::default();

        stats.update_rtt(50.0);
        assert!((stats.rtt_ms - 50.0).abs() < 0.001);
        assert!((stats.avg_rtt_ms - 50.0).abs() < 0.001);

        stats.update_rtt(100.0);
        assert!((stats.rtt_ms - 100.0).abs() < 0.001);
        // EMA: 0.2 * 100 + 0.8 * 50 = 60
        assert!((stats.avg_rtt_ms - 60.0).abs() < 0.001);

        stats.record_sent(100);
        assert_eq!(stats.bytes_sent, 100);
        assert_eq!(stats.packets_sent, 1);

        stats.record_received(200);
        assert_eq!(stats.bytes_received, 200);
        assert_eq!(stats.packets_received, 1);
    }

    #[test]
    fn test_world_connection_creation() {
        let conn = WorldConnection::new(
            "wss://example.com".to_string(),
            ConsistencyConfig::default(),
        );

        assert_eq!(conn.state(), ConnectionState::Disconnected);
        assert!(!conn.is_connected());
        assert!(!conn.needs_reconnect());
    }
}
