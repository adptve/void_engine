//! Client Connection Management
//!
//! Manages connected clients (apps, session manager, shell) that communicate
//! with the kernel daemon via IPC.
//!
//! # Client Lifecycle
//!
//! 1. Client connects to socket
//! 2. Client sends RegisterApp message
//! 3. Kernel assigns namespace and grants capabilities
//! 4. Client can submit transactions and receive frame notifications
//! 5. Client disconnects or is killed on failure

use parking_lot::RwLock;
use std::collections::HashMap;
use std::io::{BufReader, BufWriter};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use void_ir::NamespaceId;

use crate::app::AppId;
use crate::capability::CapabilityId;
use crate::ipc::{
    ClientMessage, IpcError, KernelMessage, MessageId,
    write_message, read_client_message,
};

/// Unique identifier for a connected client
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ClientId(u64);

impl ClientId {
    /// Create a new unique client ID
    pub fn new() -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID value
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for ClientId {
    fn default() -> Self {
        Self::new()
    }
}

impl std::fmt::Display for ClientId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "client:{}", self.0)
    }
}

/// State of a client connection
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ClientState {
    /// Client has connected but not registered
    Connected,
    /// Client is registering (sent RegisterApp, awaiting response)
    Registering,
    /// Client is fully registered and active
    Active,
    /// Client is being disconnected
    Disconnecting,
    /// Client has been disconnected
    Disconnected,
}

/// Statistics for a client connection
#[derive(Debug, Clone, Default)]
pub struct ClientStats {
    /// Number of messages received from client
    pub messages_received: u64,
    /// Number of messages sent to client
    pub messages_sent: u64,
    /// Number of transactions submitted
    pub transactions_submitted: u64,
    /// Number of transactions succeeded
    pub transactions_succeeded: u64,
    /// Number of transactions failed
    pub transactions_failed: u64,
    /// Total bytes received
    pub bytes_received: u64,
    /// Total bytes sent
    pub bytes_sent: u64,
    /// Last message timestamp
    pub last_message_at: Option<Instant>,
    /// Average round-trip time (from heartbeats)
    pub avg_rtt_ms: f32,
}

/// Information about a connected client
#[derive(Debug)]
pub struct ClientInfo {
    /// Unique client ID
    pub id: ClientId,
    /// Current state
    pub state: ClientState,
    /// Associated app ID (if registered as an app)
    pub app_id: Option<AppId>,
    /// Associated namespace ID (if registered)
    pub namespace_id: Option<NamespaceId>,
    /// Client name (from registration)
    pub name: Option<String>,
    /// When the client connected
    pub connected_at: Instant,
    /// Last activity timestamp
    pub last_activity: Instant,
    /// Statistics
    pub stats: ClientStats,
    /// Granted capability IDs
    pub capabilities: Vec<CapabilityId>,
    /// Whether client wants frame notifications
    pub subscribe_frames: bool,
}

impl ClientInfo {
    /// Create a new client info
    pub fn new(id: ClientId) -> Self {
        let now = Instant::now();
        Self {
            id,
            state: ClientState::Connected,
            app_id: None,
            namespace_id: None,
            name: None,
            connected_at: now,
            last_activity: now,
            stats: ClientStats::default(),
            capabilities: Vec::new(),
            subscribe_frames: true, // Default to receiving frame notifications
        }
    }

    /// Update last activity timestamp
    pub fn touch(&mut self) {
        self.last_activity = Instant::now();
    }

    /// Check if client has been idle too long
    pub fn is_idle(&self, timeout: Duration) -> bool {
        self.last_activity.elapsed() > timeout
    }

    /// Get connection duration
    pub fn connection_duration(&self) -> Duration {
        self.connected_at.elapsed()
    }

    /// Record a message received
    pub fn record_message_received(&mut self, bytes: u64) {
        self.stats.messages_received += 1;
        self.stats.bytes_received += bytes;
        self.stats.last_message_at = Some(Instant::now());
        self.touch();
    }

    /// Record a message sent
    pub fn record_message_sent(&mut self, bytes: u64) {
        self.stats.messages_sent += 1;
        self.stats.bytes_sent += bytes;
    }

    /// Record a transaction result
    pub fn record_transaction(&mut self, success: bool) {
        self.stats.transactions_submitted += 1;
        if success {
            self.stats.transactions_succeeded += 1;
        } else {
            self.stats.transactions_failed += 1;
        }
    }

    /// Update RTT measurement
    pub fn update_rtt(&mut self, rtt_ms: f32) {
        // Exponential moving average
        if self.stats.avg_rtt_ms == 0.0 {
            self.stats.avg_rtt_ms = rtt_ms;
        } else {
            self.stats.avg_rtt_ms = self.stats.avg_rtt_ms * 0.9 + rtt_ms * 0.1;
        }
    }
}

/// A connected client with its communication channels
pub struct Client {
    /// Client information
    pub info: ClientInfo,
    /// Outgoing message queue
    outgoing: Vec<(MessageId, KernelMessage)>,
}

impl Client {
    /// Create a new client
    pub fn new(id: ClientId) -> Self {
        Self {
            info: ClientInfo::new(id),
            outgoing: Vec::new(),
        }
    }

    /// Get the client ID
    pub fn id(&self) -> ClientId {
        self.info.id
    }

    /// Get the client state
    pub fn state(&self) -> ClientState {
        self.info.state
    }

    /// Set the client state
    pub fn set_state(&mut self, state: ClientState) {
        self.info.state = state;
    }

    /// Queue a message to be sent to this client
    pub fn queue_message(&mut self, msg: KernelMessage) {
        let id = MessageId::new();
        self.outgoing.push((id, msg));
    }

    /// Queue a message with a specific ID (for responses)
    pub fn queue_response(&mut self, request_id: MessageId, msg: KernelMessage) {
        self.outgoing.push((request_id, msg));
    }

    /// Take all queued outgoing messages
    pub fn take_outgoing(&mut self) -> Vec<(MessageId, KernelMessage)> {
        std::mem::take(&mut self.outgoing)
    }

    /// Check if there are outgoing messages
    pub fn has_outgoing(&self) -> bool {
        !self.outgoing.is_empty()
    }

    /// Register this client as an app
    pub fn register_as_app(
        &mut self,
        app_id: AppId,
        namespace_id: NamespaceId,
        name: String,
    ) {
        self.info.app_id = Some(app_id);
        self.info.namespace_id = Some(namespace_id);
        self.info.name = Some(name);
        self.info.state = ClientState::Active;
    }

    /// Add a granted capability
    pub fn add_capability(&mut self, cap_id: CapabilityId) {
        if !self.info.capabilities.contains(&cap_id) {
            self.info.capabilities.push(cap_id);
        }
    }

    /// Remove a revoked capability
    pub fn remove_capability(&mut self, cap_id: CapabilityId) {
        self.info.capabilities.retain(|c| *c != cap_id);
    }

    /// Send frame begin notification
    pub fn notify_frame_begin(&mut self, frame: u64, timestamp_ns: u64, delta_time: f32) {
        if self.info.subscribe_frames && self.info.state == ClientState::Active {
            self.queue_message(KernelMessage::FrameBegin {
                frame,
                timestamp_ns,
                delta_time,
            });
        }
    }

    /// Send frame end notification
    pub fn notify_frame_end(&mut self, frame: u64, duration_ms: f32) {
        if self.info.subscribe_frames && self.info.state == ClientState::Active {
            self.queue_message(KernelMessage::FrameEnd { frame, duration_ms });
        }
    }

    /// Send shutdown notification
    pub fn notify_shutdown(&mut self, reason: &str, grace_period_ms: u64) {
        self.queue_message(KernelMessage::Shutdown {
            reason: reason.to_string(),
            grace_period_ms,
        });
        self.info.state = ClientState::Disconnecting;
    }

    /// Send recovery notification
    pub fn notify_recovery(&mut self, last_frame: u64, reason: &str) {
        self.queue_message(KernelMessage::Recovery {
            last_frame,
            reason: reason.to_string(),
        });
    }
}

/// Manages all connected clients
pub struct ClientManager {
    /// All connected clients
    clients: HashMap<ClientId, Client>,
    /// Client ID by namespace ID lookup
    by_namespace: HashMap<NamespaceId, ClientId>,
    /// Client ID by app ID lookup
    by_app: HashMap<AppId, ClientId>,
    /// Maximum number of clients
    max_clients: usize,
    /// Client idle timeout
    idle_timeout: Duration,
}

impl ClientManager {
    /// Create a new client manager
    pub fn new(max_clients: usize) -> Self {
        Self {
            clients: HashMap::new(),
            by_namespace: HashMap::new(),
            by_app: HashMap::new(),
            max_clients,
            idle_timeout: Duration::from_secs(60),
        }
    }

    /// Set the idle timeout
    pub fn set_idle_timeout(&mut self, timeout: Duration) {
        self.idle_timeout = timeout;
    }

    /// Create a new client connection
    pub fn accept(&mut self) -> Result<ClientId, ClientError> {
        if self.clients.len() >= self.max_clients {
            return Err(ClientError::TooManyClients);
        }

        let id = ClientId::new();
        let client = Client::new(id);
        self.clients.insert(id, client);

        log::info!("Client {} connected", id);
        Ok(id)
    }

    /// Register a client as an app
    pub fn register_client(
        &mut self,
        client_id: ClientId,
        app_id: AppId,
        namespace_id: NamespaceId,
        name: String,
    ) -> Result<(), ClientError> {
        let client = self.clients.get_mut(&client_id)
            .ok_or(ClientError::NotFound(client_id))?;

        if client.state() != ClientState::Connected && client.state() != ClientState::Registering {
            return Err(ClientError::InvalidState);
        }

        client.register_as_app(app_id, namespace_id, name);
        self.by_namespace.insert(namespace_id, client_id);
        self.by_app.insert(app_id, client_id);

        log::info!("Client {} registered as app {:?}", client_id, app_id);
        Ok(())
    }

    /// Disconnect a client
    pub fn disconnect(&mut self, client_id: ClientId) -> Option<ClientInfo> {
        if let Some(client) = self.clients.remove(&client_id) {
            if let Some(ns_id) = client.info.namespace_id {
                self.by_namespace.remove(&ns_id);
            }
            if let Some(app_id) = client.info.app_id {
                self.by_app.remove(&app_id);
            }
            log::info!("Client {} disconnected", client_id);
            Some(client.info)
        } else {
            None
        }
    }

    /// Get a client by ID
    pub fn get(&self, id: ClientId) -> Option<&Client> {
        self.clients.get(&id)
    }

    /// Get a mutable client by ID
    pub fn get_mut(&mut self, id: ClientId) -> Option<&mut Client> {
        self.clients.get_mut(&id)
    }

    /// Get a client by namespace ID
    pub fn get_by_namespace(&self, namespace_id: NamespaceId) -> Option<&Client> {
        self.by_namespace.get(&namespace_id)
            .and_then(|id| self.clients.get(id))
    }

    /// Get a mutable client by namespace ID
    pub fn get_by_namespace_mut(&mut self, namespace_id: NamespaceId) -> Option<&mut Client> {
        if let Some(&id) = self.by_namespace.get(&namespace_id) {
            self.clients.get_mut(&id)
        } else {
            None
        }
    }

    /// Get a client by app ID
    pub fn get_by_app(&self, app_id: AppId) -> Option<&Client> {
        self.by_app.get(&app_id)
            .and_then(|id| self.clients.get(id))
    }

    /// Get a mutable client by app ID
    pub fn get_by_app_mut(&mut self, app_id: AppId) -> Option<&mut Client> {
        if let Some(&id) = self.by_app.get(&app_id) {
            self.clients.get_mut(&id)
        } else {
            None
        }
    }

    /// Get all client IDs
    pub fn all_ids(&self) -> Vec<ClientId> {
        self.clients.keys().copied().collect()
    }

    /// Get all active client IDs
    pub fn active_ids(&self) -> Vec<ClientId> {
        self.clients.iter()
            .filter(|(_, c)| c.state() == ClientState::Active)
            .map(|(id, _)| *id)
            .collect()
    }

    /// Get client count
    pub fn len(&self) -> usize {
        self.clients.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.clients.is_empty()
    }

    /// Iterate over all clients
    pub fn iter(&self) -> impl Iterator<Item = (&ClientId, &Client)> {
        self.clients.iter()
    }

    /// Iterate mutably over all clients
    pub fn iter_mut(&mut self) -> impl Iterator<Item = (&ClientId, &mut Client)> {
        self.clients.iter_mut()
    }

    /// Broadcast a message to all active clients
    pub fn broadcast(&mut self, msg: KernelMessage) {
        for client in self.clients.values_mut() {
            if client.state() == ClientState::Active {
                client.queue_message(msg.clone());
            }
        }
    }

    /// Broadcast frame begin to all subscribed clients
    pub fn broadcast_frame_begin(&mut self, frame: u64, timestamp_ns: u64, delta_time: f32) {
        for client in self.clients.values_mut() {
            client.notify_frame_begin(frame, timestamp_ns, delta_time);
        }
    }

    /// Broadcast frame end to all subscribed clients
    pub fn broadcast_frame_end(&mut self, frame: u64, duration_ms: f32) {
        for client in self.clients.values_mut() {
            client.notify_frame_end(frame, duration_ms);
        }
    }

    /// Broadcast shutdown notification
    pub fn broadcast_shutdown(&mut self, reason: &str, grace_period_ms: u64) {
        for client in self.clients.values_mut() {
            client.notify_shutdown(reason, grace_period_ms);
        }
    }

    /// Find and disconnect idle clients
    pub fn gc_idle_clients(&mut self) -> Vec<ClientInfo> {
        let idle_ids: Vec<_> = self.clients.iter()
            .filter(|(_, c)| c.info.is_idle(self.idle_timeout))
            .map(|(id, _)| *id)
            .collect();

        let mut disconnected = Vec::new();
        for id in idle_ids {
            if let Some(info) = self.disconnect(id) {
                log::info!("Client {} disconnected due to idle timeout", id);
                disconnected.push(info);
            }
        }

        disconnected
    }

    /// Get aggregate statistics
    pub fn stats(&self) -> ClientManagerStats {
        let mut stats = ClientManagerStats::default();
        stats.total_clients = self.clients.len();

        for client in self.clients.values() {
            match client.state() {
                ClientState::Connected => stats.connecting += 1,
                ClientState::Registering => stats.registering += 1,
                ClientState::Active => stats.active += 1,
                ClientState::Disconnecting => stats.disconnecting += 1,
                ClientState::Disconnected => {}
            }

            stats.total_messages_received += client.info.stats.messages_received;
            stats.total_messages_sent += client.info.stats.messages_sent;
            stats.total_transactions += client.info.stats.transactions_submitted;
        }

        stats
    }
}

/// Aggregate statistics for the client manager
#[derive(Debug, Clone, Default)]
pub struct ClientManagerStats {
    /// Total connected clients
    pub total_clients: usize,
    /// Clients in connecting state
    pub connecting: usize,
    /// Clients in registering state
    pub registering: usize,
    /// Active clients
    pub active: usize,
    /// Clients being disconnected
    pub disconnecting: usize,
    /// Total messages received
    pub total_messages_received: u64,
    /// Total messages sent
    pub total_messages_sent: u64,
    /// Total transactions submitted
    pub total_transactions: u64,
}

/// Errors from client operations
#[derive(Debug, Clone)]
pub enum ClientError {
    /// Too many clients connected
    TooManyClients,
    /// Client not found
    NotFound(ClientId),
    /// Invalid state for operation
    InvalidState,
    /// IPC error
    Ipc(String),
}

impl std::fmt::Display for ClientError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TooManyClients => write!(f, "Maximum client count exceeded"),
            Self::NotFound(id) => write!(f, "Client {} not found", id),
            Self::InvalidState => write!(f, "Invalid state for this operation"),
            Self::Ipc(e) => write!(f, "IPC error: {}", e),
        }
    }
}

impl std::error::Error for ClientError {}

impl From<IpcError> for ClientError {
    fn from(e: IpcError) -> Self {
        Self::Ipc(e.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_client_id() {
        let id1 = ClientId::new();
        let id2 = ClientId::new();
        assert_ne!(id1.raw(), id2.raw());
    }

    #[test]
    fn test_client_lifecycle() {
        let id = ClientId::new();
        let mut client = Client::new(id);

        assert_eq!(client.state(), ClientState::Connected);

        // Register
        let app_id = AppId::new();
        let ns_id = NamespaceId::new();
        client.register_as_app(app_id, ns_id, "test_app".to_string());

        assert_eq!(client.state(), ClientState::Active);
        assert_eq!(client.info.app_id, Some(app_id));
        assert_eq!(client.info.namespace_id, Some(ns_id));
    }

    #[test]
    fn test_client_manager() {
        let mut manager = ClientManager::new(10);

        // Accept client
        let client_id = manager.accept().unwrap();
        assert_eq!(manager.len(), 1);

        // Register
        let app_id = AppId::new();
        let ns_id = NamespaceId::new();
        manager.register_client(client_id, app_id, ns_id, "test".to_string()).unwrap();

        // Look up by different keys
        assert!(manager.get(client_id).is_some());
        assert!(manager.get_by_namespace(ns_id).is_some());
        assert!(manager.get_by_app(app_id).is_some());

        // Disconnect
        let info = manager.disconnect(client_id);
        assert!(info.is_some());
        assert_eq!(manager.len(), 0);
    }

    #[test]
    fn test_client_limit() {
        let mut manager = ClientManager::new(2);

        manager.accept().unwrap();
        manager.accept().unwrap();

        // Third should fail
        assert!(matches!(
            manager.accept(),
            Err(ClientError::TooManyClients)
        ));
    }

    #[test]
    fn test_broadcast() {
        let mut manager = ClientManager::new(10);

        // Create and register two clients
        let id1 = manager.accept().unwrap();
        let id2 = manager.accept().unwrap();

        let app1 = AppId::new();
        let app2 = AppId::new();
        let ns1 = NamespaceId::new();
        let ns2 = NamespaceId::new();

        manager.register_client(id1, app1, ns1, "app1".to_string()).unwrap();
        manager.register_client(id2, app2, ns2, "app2".to_string()).unwrap();

        // Broadcast
        manager.broadcast_frame_begin(1, 1234567890, 0.016);

        // Both should have messages
        assert!(manager.get(id1).unwrap().has_outgoing());
        assert!(manager.get(id2).unwrap().has_outgoing());
    }

    #[test]
    fn test_message_queue() {
        let id = ClientId::new();
        let mut client = Client::new(id);

        assert!(!client.has_outgoing());

        client.queue_message(KernelMessage::FrameBegin {
            frame: 1,
            timestamp_ns: 0,
            delta_time: 0.016,
        });

        assert!(client.has_outgoing());

        let messages = client.take_outgoing();
        assert_eq!(messages.len(), 1);
        assert!(!client.has_outgoing());
    }

    #[test]
    fn test_client_stats() {
        let id = ClientId::new();
        let mut client = Client::new(id);

        client.info.record_message_received(100);
        client.info.record_message_sent(50);
        client.info.record_transaction(true);
        client.info.record_transaction(false);

        assert_eq!(client.info.stats.messages_received, 1);
        assert_eq!(client.info.stats.messages_sent, 1);
        assert_eq!(client.info.stats.bytes_received, 100);
        assert_eq!(client.info.stats.bytes_sent, 50);
        assert_eq!(client.info.stats.transactions_submitted, 2);
        assert_eq!(client.info.stats.transactions_succeeded, 1);
        assert_eq!(client.info.stats.transactions_failed, 1);
    }
}
