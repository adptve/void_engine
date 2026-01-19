//! Kernel Daemon
//!
//! The kernel daemon wraps the core Kernel and provides the systemd service
//! lifecycle: socket listening, client management, frame loop, and state
//! persistence for crash recovery.
//!
//! # Daemon Architecture
//!
//! ```text
//! systemd
//!    |
//!    +-- metaverse-kernel.service (ALWAYS RUNNING)
//!           |
//!           +-- /opt/metaverse/bin/metaverse-kernel
//!           +-- Listens: /run/metaverse/kernel.sock
//!           +-- Auto-restart: always
//!           +-- State: /var/metaverse/state/
//! ```
//!
//! # Main Loop
//!
//! 1. Pet the watchdog
//! 2. Accept new client connections
//! 3. Process client messages
//! 4. Run frame (begin, process transactions, build render graph, end)
//! 5. Send frame notifications to clients
//! 6. Persist state periodically
//! 7. Check for shutdown signal

use parking_lot::RwLock;
use std::collections::VecDeque;
use std::io::{BufReader, BufWriter, Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

#[cfg(unix)]
use std::os::unix::net::{UnixListener, UnixStream};

use void_ecs::World;
use void_ir::{Namespace, NamespaceId, Transaction};

use crate::app::{AppId, AppManifest, AppPermissions, LayerRequest, ResourceRequirements};
use crate::capability::{Capability, CapabilityKind};
use crate::client::{Client, ClientId, ClientManager, ClientState};
use crate::ipc::{
    AppRegistration, CapabilityInfo, CapabilityRequest, ClientMessage, ErrorCode, IpcError,
    KernelMessage, KernelStatusReport, MessageId, SerializableTransaction,
};
use crate::persist::{
    PersistConfig, PersistedApp, PersistedCapability, PersistedLayer, PersistedState,
    StatePersister,
};
use crate::{Kernel, KernelConfig, KernelState};

/// Daemon configuration
#[derive(Debug, Clone)]
pub struct DaemonConfig {
    /// Kernel configuration
    pub kernel: KernelConfig,
    /// Socket path for IPC
    pub socket_path: PathBuf,
    /// State persistence configuration
    pub persist: PersistConfig,
    /// Maximum number of clients
    pub max_clients: usize,
    /// Client idle timeout
    pub client_idle_timeout: Duration,
    /// Target frame time (for frame pacing)
    pub target_frame_time: Duration,
    /// Shutdown grace period
    pub shutdown_grace_period: Duration,
}

impl Default for DaemonConfig {
    fn default() -> Self {
        Self {
            kernel: KernelConfig::default(),
            socket_path: PathBuf::from("/run/metaverse/kernel.sock"),
            persist: PersistConfig::default(),
            max_clients: 256,
            client_idle_timeout: Duration::from_secs(60),
            target_frame_time: Duration::from_secs_f64(1.0 / 60.0),
            shutdown_grace_period: Duration::from_secs(5),
        }
    }
}

impl DaemonConfig {
    /// Development configuration (uses local paths)
    pub fn development() -> Self {
        Self {
            kernel: KernelConfig::default(),
            socket_path: PathBuf::from("./metaverse.sock"),
            persist: PersistConfig::development(),
            max_clients: 64,
            client_idle_timeout: Duration::from_secs(300),
            target_frame_time: Duration::from_secs_f64(1.0 / 60.0),
            shutdown_grace_period: Duration::from_secs(2),
        }
    }
}

/// Daemon state
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DaemonState {
    /// Daemon is starting up
    Starting,
    /// Daemon is recovering from crash
    Recovering,
    /// Daemon is running normally
    Running,
    /// Daemon is shutting down gracefully
    ShuttingDown,
    /// Daemon has stopped
    Stopped,
}

/// Message received from a client (queued for processing)
struct IncomingMessage {
    client_id: ClientId,
    message_id: MessageId,
    message: ClientMessage,
    received_at: Instant,
}

/// The Kernel Daemon - wraps Kernel with IPC and persistence
pub struct KernelDaemon {
    /// Configuration
    config: DaemonConfig,
    /// Core kernel
    kernel: Kernel,
    /// ECS world
    world: World,
    /// Connected clients
    clients: ClientManager,
    /// State persister
    persister: StatePersister,
    /// Daemon state
    state: DaemonState,
    /// Shutdown flag
    shutdown_requested: Arc<AtomicBool>,
    /// Incoming message queue
    incoming_messages: VecDeque<IncomingMessage>,
    /// Frame timing
    last_frame_time: Instant,
    frame_start_time: Instant,
    /// Start time for uptime calculation
    start_time: Instant,
    /// Recovery info (if recovering from crash)
    recovery_info: Option<RecoveryInfo>,
}

/// Information about crash recovery
#[derive(Debug)]
struct RecoveryInfo {
    /// Last frame before crash
    last_frame: u64,
    /// Recovered apps
    recovered_apps: Vec<u64>,
    /// Reason for recovery
    reason: String,
}

impl KernelDaemon {
    /// Create a new kernel daemon
    pub fn new(config: DaemonConfig) -> Self {
        let kernel = Kernel::new(config.kernel.clone());
        let world = World::new();
        let clients = ClientManager::new(config.max_clients);
        let persister = StatePersister::new(config.persist.clone());

        let now = Instant::now();

        Self {
            config,
            kernel,
            world,
            clients,
            persister,
            state: DaemonState::Starting,
            shutdown_requested: Arc::new(AtomicBool::new(false)),
            incoming_messages: VecDeque::new(),
            last_frame_time: now,
            frame_start_time: now,
            start_time: now,
            recovery_info: None,
        }
    }

    /// Get a shutdown flag that can be used to signal shutdown from outside
    pub fn shutdown_flag(&self) -> Arc<AtomicBool> {
        Arc::clone(&self.shutdown_requested)
    }

    /// Initialize the daemon
    pub fn init(&mut self) -> Result<(), DaemonError> {
        log::info!("Initializing kernel daemon...");

        // Initialize state persister
        if let Err(e) = self.persister.init() {
            log::warn!("Failed to initialize state persistence: {}", e);
            // Continue without persistence
        }

        // Check for crash recovery
        if self.persister.has_state() {
            match self.persister.load() {
                Ok(state) => {
                    log::info!("Found persisted state, recovering...");
                    self.state = DaemonState::Recovering;
                    self.recover_from_state(state)?;
                }
                Err(e) => {
                    log::warn!("Failed to load persisted state: {}", e);
                    // Continue with fresh start
                }
            }
        }

        // Start the kernel
        self.kernel.start();

        self.state = DaemonState::Running;
        log::info!("Kernel daemon initialized");

        Ok(())
    }

    /// Recover from persisted state
    fn recover_from_state(&mut self, state: PersistedState) -> Result<(), DaemonError> {
        log::info!(
            "Recovering from state: frame={}, apps={}",
            state.frame,
            state.apps.len()
        );

        let mut recovered_apps = Vec::new();

        // TODO: Restore apps from persisted state
        // For now, just record the recovery info
        for app in &state.apps {
            recovered_apps.push(app.id);
        }

        self.recovery_info = Some(RecoveryInfo {
            last_frame: state.frame,
            recovered_apps,
            reason: "Crash recovery".to_string(),
        });

        Ok(())
    }

    /// Handle a new client connection
    pub fn accept_client(&mut self) -> Result<ClientId, DaemonError> {
        let client_id = self.clients.accept()?;
        log::info!("Accepted new client: {}", client_id);
        Ok(client_id)
    }

    /// Queue a message from a client for processing
    pub fn queue_message(
        &mut self,
        client_id: ClientId,
        message_id: MessageId,
        message: ClientMessage,
    ) {
        self.incoming_messages.push_back(IncomingMessage {
            client_id,
            message_id,
            message,
            received_at: Instant::now(),
        });
    }

    /// Process all queued client messages
    pub fn process_messages(&mut self) {
        while let Some(msg) = self.incoming_messages.pop_front() {
            self.handle_client_message(msg.client_id, msg.message_id, msg.message);
        }
    }

    /// Handle a single client message
    fn handle_client_message(
        &mut self,
        client_id: ClientId,
        message_id: MessageId,
        message: ClientMessage,
    ) {
        // Validate client exists first
        if self.clients.get(client_id).is_none() {
            log::warn!("Message from unknown client {}", client_id);
            return;
        }

        // Handle each message type - handlers look up client by ID to avoid borrow conflicts
        match message {
            ClientMessage::SubmitTransaction { transaction } => {
                self.handle_submit_transaction(client_id, message_id, transaction);
            }
            ClientMessage::RequestCapability { request } => {
                self.handle_request_capability(client_id, message_id, request);
            }
            ClientMessage::Heartbeat { client_timestamp } => {
                self.handle_heartbeat(client_id, message_id, client_timestamp);
            }
            ClientMessage::RegisterApp { registration } => {
                self.handle_register_app(client_id, message_id, registration);
            }
            ClientMessage::UnregisterApp => {
                self.handle_unregister_app(client_id, message_id);
            }
            ClientMessage::QueryStatus => {
                self.handle_query_status(client_id, message_id);
            }
        }
    }

    /// Handle transaction submission
    fn handle_submit_transaction(
        &mut self,
        client_id: ClientId,
        message_id: MessageId,
        tx: SerializableTransaction,
    ) {
        // Get the client's namespace
        let namespace_id = match self.clients.get(client_id).and_then(|c| c.info.namespace_id) {
            Some(ns) => ns,
            None => {
                if let Some(client) = self.clients.get_mut(client_id) {
                    client.queue_response(
                        message_id,
                        KernelMessage::Error {
                            code: ErrorCode::PermissionDenied,
                            message: "Client not registered".to_string(),
                        },
                    );
                }
                return;
            }
        };

        // Convert to Transaction
        let transaction = match tx.to_transaction(namespace_id) {
            Ok(t) => t,
            Err(e) => {
                if let Some(client) = self.clients.get_mut(client_id) {
                    client.queue_response(
                        message_id,
                        KernelMessage::TransactionResult {
                            id: tx.id,
                            success: false,
                            error: Some(format!("Invalid transaction: {}", e)),
                            patches_applied: 0,
                        },
                    );
                    client.info.record_transaction(false);
                }
                return;
            }
        };

        // Submit to patch bus
        let patch_bus = self.kernel.patch_bus();
        patch_bus.submit(transaction.clone());

        // Transaction will be processed at frame boundary
        // For now, acknowledge receipt (actual result comes later)
        if let Some(client) = self.clients.get_mut(client_id) {
            client.queue_response(
                message_id,
                KernelMessage::TransactionResult {
                    id: tx.id,
                    success: true,
                    error: None,
                    patches_applied: transaction.len(),
                },
            );
            client.info.record_transaction(true);
        }
    }

    /// Handle capability request
    fn handle_request_capability(
        &mut self,
        client_id: ClientId,
        message_id: MessageId,
        request: CapabilityRequest,
    ) {
        let namespace_id = match self.clients.get(client_id).and_then(|c| c.info.namespace_id) {
            Some(ns) => ns,
            None => {
                if let Some(client) = self.clients.get_mut(client_id) {
                    client.queue_response(
                        message_id,
                        KernelMessage::CapabilityDenied {
                            reason: "Client not registered".to_string(),
                        },
                    );
                }
                return;
            }
        };

        // Convert to kernel capability kind
        let kind = request.kind.to_capability_kind();

        // Check if this is an allowed capability for apps
        if kind.is_admin() {
            if let Some(client) = self.clients.get_mut(client_id) {
                client.queue_response(
                    message_id,
                    KernelMessage::CapabilityDenied {
                        reason: "Admin capabilities cannot be requested".to_string(),
                    },
                );
            }
            return;
        }

        // Grant the capability
        let kernel_ns = NamespaceId::KERNEL;
        let mut capability = Capability::new(kind, namespace_id, kernel_ns);

        if request.delegable {
            capability = capability.delegable();
        }

        if let Some(reason) = request.reason {
            capability = capability.with_reason(reason);
        }

        if let Some(duration_secs) = request.duration_secs {
            let expires_at = Instant::now() + Duration::from_secs(duration_secs);
            capability = capability.with_expiry(expires_at);
        }

        let cap_info = CapabilityInfo::from_capability(&capability);
        let cap_id = capability.id;

        self.kernel.capability_checker_mut().grant(capability);

        if let Some(client) = self.clients.get_mut(client_id) {
            client.add_capability(cap_id);
            client.queue_response(
                message_id,
                KernelMessage::CapabilityGranted {
                    capability: cap_info,
                },
            );
        }
    }

    /// Handle heartbeat
    fn handle_heartbeat(
        &mut self,
        client_id: ClientId,
        _message_id: MessageId,
        client_timestamp: u64,
    ) {
        // Calculate RTT
        let now_ns = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_nanos() as u64)
            .unwrap_or(0);

        let rtt_ns = now_ns.saturating_sub(client_timestamp);
        let rtt_ms = rtt_ns as f32 / 1_000_000.0;

        if let Some(client) = self.clients.get_mut(client_id) {
            client.info.update_rtt(rtt_ms);
            client.info.touch();
        }
    }

    /// Handle app registration
    fn handle_register_app(
        &mut self,
        client_id: ClientId,
        message_id: MessageId,
        registration: AppRegistration,
    ) {
        // Check if already registered
        if self.clients.get(client_id).map(|c| c.info.app_id.is_some()).unwrap_or(false) {
            if let Some(client) = self.clients.get_mut(client_id) {
                client.queue_response(
                    message_id,
                    KernelMessage::Error {
                        code: ErrorCode::InvalidState,
                        message: "Already registered".to_string(),
                    },
                );
            }
            return;
        }

        // Create app manifest
        let manifest = AppManifest {
            name: registration.name.clone(),
            version: registration.version.clone(),
            description: None,
            author: None,
            layers: Vec::new(),
            resources: ResourceRequirements::default(),
            permissions: AppPermissions::default(),
        };

        // Load the app
        let app_id = match self.kernel.app_manager_mut().load(manifest) {
            Ok(id) => id,
            Err(e) => {
                if let Some(client) = self.clients.get_mut(client_id) {
                    client.queue_response(
                        message_id,
                        KernelMessage::Error {
                            code: ErrorCode::Unknown,
                            message: format!("Failed to register app: {}", e),
                        },
                    );
                }
                return;
            }
        };

        // Get the namespace ID
        let namespace_id = self
            .kernel
            .app_manager()
            .get(app_id)
            .map(|app| app.namespace_id)
            .unwrap();

        // Register client with this app
        if let Err(e) = self.clients.register_client(
            client_id,
            app_id,
            namespace_id,
            registration.name.clone(),
        ) {
            log::error!("Failed to register client: {}", e);
            return;
        }

        // Grant requested capabilities
        let kernel_ns = NamespaceId::KERNEL;
        let mut granted_capabilities = Vec::new();

        for cap_request in registration.capabilities {
            let kind = cap_request.to_capability_kind();

            // Skip admin capabilities
            if kind.is_admin() {
                continue;
            }

            let capability = Capability::new(kind, namespace_id, kernel_ns);
            let cap_info = CapabilityInfo::from_capability(&capability);
            let cap_id = capability.id;

            self.kernel.capability_checker_mut().grant(capability);

            // Update client through client manager
            if let Some(client) = self.clients.get_mut(client_id) {
                client.add_capability(cap_id);
            }

            granted_capabilities.push(cap_info);
        }

        // Grant default capabilities
        self.kernel
            .capability_checker_mut()
            .grant_default_app_capabilities(namespace_id, kernel_ns);

        // Initialize the app
        if let Err(e) = self.kernel.app_manager_mut().initialize(app_id) {
            log::warn!("Failed to initialize app: {}", e);
        }

        // Get client again (borrow checker)
        if let Some(client) = self.clients.get_mut(client_id) {
            client.queue_response(
                message_id,
                KernelMessage::AppRegistered {
                    app_id: app_id.raw(),
                    namespace_id: namespace_id.raw(),
                    capabilities: granted_capabilities,
                },
            );

            // Notify about recovery if applicable
            if let Some(ref recovery) = self.recovery_info {
                client.notify_recovery(recovery.last_frame, &recovery.reason);
            }
        }

        log::info!(
            "Registered app '{}' (ID: {:?}, NS: {})",
            registration.name,
            app_id,
            namespace_id
        );
    }

    /// Handle app unregistration
    fn handle_unregister_app(&mut self, client_id: ClientId, _message_id: MessageId) {
        // Get app_id and capabilities before modifying
        let (app_id, capabilities) = match self.clients.get(client_id) {
            Some(client) => (client.info.app_id, client.info.capabilities.clone()),
            None => return,
        };

        if let Some(app_id) = app_id {
            // Revoke all capabilities
            for cap_id in &capabilities {
                self.kernel.capability_checker_mut().revoke(*cap_id);
            }

            // Unload the app
            if let Err(e) = self.kernel.app_manager_mut().unload(app_id) {
                log::warn!("Failed to unload app {:?}: {}", app_id, e);
            }

            log::info!("Unregistered app {:?}", app_id);
        }

        // Disconnect client
        if let Some(client) = self.clients.get_mut(client_id) {
            client.set_state(ClientState::Disconnecting);
        }
    }

    /// Handle status query
    fn handle_query_status(&mut self, client_id: ClientId, message_id: MessageId) {
        let status = self.kernel.status();

        if let Some(client) = self.clients.get_mut(client_id) {
            client.queue_response(
                message_id,
                KernelMessage::StatusReport {
                    status: KernelStatusReport {
                        frame: status.frame,
                        uptime_secs: status.uptime_secs as f64,
                        avg_fps: status.avg_fps,
                        running_apps: status.running_apps,
                        layer_count: status.layer_count,
                        asset_count: status.asset_count,
                        health: format!("{}", status.health_level),
                        backend: format!("{}", status.backend),
                    },
                },
            );
        }
    }

    /// Run a single frame
    pub fn run_frame(&mut self) {
        let now = Instant::now();
        let delta_time = now.duration_since(self.last_frame_time).as_secs_f32();
        self.last_frame_time = now;
        self.frame_start_time = now;

        // Reset frame quotas
        self.kernel.reset_frame_quotas();

        // Begin frame
        let frame_ctx = self.kernel.begin_frame(delta_time);

        // Notify clients of frame begin
        let timestamp_ns = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_nanos() as u64)
            .unwrap_or(0);

        self.clients
            .broadcast_frame_begin(frame_ctx.frame, timestamp_ns, delta_time);

        // Process transactions
        let _results = self.kernel.process_transactions(&mut self.world);

        // Build render graph
        let _render_graph = self.kernel.build_render_graph();

        // End frame
        self.kernel.end_frame();

        // Calculate frame duration
        let frame_duration = self.frame_start_time.elapsed();
        let frame_duration_ms = frame_duration.as_secs_f32() * 1000.0;

        // Notify clients of frame end
        self.clients
            .broadcast_frame_end(frame_ctx.frame, frame_duration_ms);

        // Garbage collection (every 60 frames)
        if frame_ctx.frame % 60 == 0 {
            self.kernel.gc();
            self.clients.gc_idle_clients();
        }

        // Persist state if needed
        if self.persister.should_persist(frame_ctx.frame, delta_time) {
            if let Err(e) = self.persist_state() {
                log::warn!("Failed to persist state: {}", e);
            }
        }
    }

    /// Build persisted state from current kernel state
    fn build_persisted_state(&self) -> PersistedState {
        let status = self.kernel.status();

        let mut state = PersistedState::new(status.frame, status.uptime_secs);
        state.kernel_state = status.state.into();
        state.backend = format!("{}", status.backend);

        // TODO: Add apps, layers, capabilities
        // For now, just save basic info

        state
    }

    /// Persist current state
    fn persist_state(&mut self) -> Result<(), DaemonError> {
        let state = self.build_persisted_state();
        self.persister.persist(&state)?;
        Ok(())
    }

    /// Check if shutdown has been requested
    pub fn shutdown_requested(&self) -> bool {
        self.shutdown_requested.load(Ordering::Relaxed)
    }

    /// Request shutdown
    pub fn request_shutdown(&self) {
        self.shutdown_requested.store(true, Ordering::Relaxed);
    }

    /// Perform graceful shutdown
    pub fn graceful_shutdown(&mut self) {
        log::info!("Starting graceful shutdown...");
        self.state = DaemonState::ShuttingDown;

        // Notify all clients
        self.clients.broadcast_shutdown(
            "Kernel daemon shutting down",
            self.config.shutdown_grace_period.as_millis() as u64,
        );

        // TODO: Flush outgoing messages to clients

        // Persist final state
        if let Err(e) = self.persist_state() {
            log::warn!("Failed to persist final state: {}", e);
        }

        // Shutdown kernel
        self.kernel.shutdown();

        self.state = DaemonState::Stopped;
        log::info!("Kernel daemon stopped");
    }

    /// Get current daemon state
    pub fn state(&self) -> DaemonState {
        self.state
    }

    /// Get kernel reference
    pub fn kernel(&self) -> &Kernel {
        &self.kernel
    }

    /// Get mutable kernel reference
    pub fn kernel_mut(&mut self) -> &mut Kernel {
        &mut self.kernel
    }

    /// Get world reference
    pub fn world(&self) -> &World {
        &self.world
    }

    /// Get mutable world reference
    pub fn world_mut(&mut self) -> &mut World {
        &mut self.world
    }

    /// Get client manager reference
    pub fn clients(&self) -> &ClientManager {
        &self.clients
    }

    /// Get mutable client manager reference
    pub fn clients_mut(&mut self) -> &mut ClientManager {
        &mut self.clients
    }

    /// Get uptime
    pub fn uptime(&self) -> Duration {
        self.start_time.elapsed()
    }

    /// Get daemon statistics
    pub fn stats(&self) -> DaemonStats {
        let kernel_status = self.kernel.status();
        let client_stats = self.clients.stats();
        let persist_stats = self.persister.stats();

        DaemonStats {
            state: self.state,
            uptime_secs: self.uptime().as_secs_f64(),
            frame: kernel_status.frame,
            avg_fps: kernel_status.avg_fps,
            running_apps: kernel_status.running_apps,
            total_clients: client_stats.total_clients,
            active_clients: client_stats.active,
            total_messages: client_stats.total_messages_received,
            total_persists: persist_stats.persists,
            last_persist_ms: persist_stats.last_persist_ms,
        }
    }
}

/// Daemon statistics
#[derive(Debug, Clone)]
pub struct DaemonStats {
    /// Current state
    pub state: DaemonState,
    /// Uptime in seconds
    pub uptime_secs: f64,
    /// Current frame number
    pub frame: u64,
    /// Average FPS
    pub avg_fps: f32,
    /// Running apps
    pub running_apps: usize,
    /// Total connected clients
    pub total_clients: usize,
    /// Active clients
    pub active_clients: usize,
    /// Total messages processed
    pub total_messages: u64,
    /// Total state persists
    pub total_persists: u64,
    /// Last persist duration (ms)
    pub last_persist_ms: f32,
}

/// Daemon errors
#[derive(Debug)]
pub enum DaemonError {
    /// IO error
    Io(std::io::Error),
    /// Client error
    Client(crate::client::ClientError),
    /// Persist error
    Persist(crate::persist::PersistError),
    /// IPC error
    Ipc(IpcError),
    /// Kernel error
    Kernel(String),
}

impl std::fmt::Display for DaemonError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io(e) => write!(f, "IO error: {}", e),
            Self::Client(e) => write!(f, "Client error: {}", e),
            Self::Persist(e) => write!(f, "Persist error: {}", e),
            Self::Ipc(e) => write!(f, "IPC error: {}", e),
            Self::Kernel(e) => write!(f, "Kernel error: {}", e),
        }
    }
}

impl std::error::Error for DaemonError {}

impl From<std::io::Error> for DaemonError {
    fn from(e: std::io::Error) -> Self {
        Self::Io(e)
    }
}

impl From<crate::client::ClientError> for DaemonError {
    fn from(e: crate::client::ClientError) -> Self {
        Self::Client(e)
    }
}

impl From<crate::persist::PersistError> for DaemonError {
    fn from(e: crate::persist::PersistError) -> Self {
        Self::Persist(e)
    }
}

impl From<IpcError> for DaemonError {
    fn from(e: IpcError) -> Self {
        Self::Ipc(e)
    }
}

/// Run the daemon main loop (blocking)
///
/// This is the entry point for the daemon binary. It sets up signal handlers
/// and runs the main loop until shutdown is requested.
#[cfg(unix)]
pub fn run_daemon(config: DaemonConfig) -> Result<(), DaemonError> {
    use std::os::unix::net::UnixListener;

    log::info!("Starting kernel daemon...");

    let mut daemon = KernelDaemon::new(config.clone());
    daemon.init()?;

    // Remove existing socket file
    let _ = std::fs::remove_file(&config.socket_path);

    // Create socket directory if needed
    if let Some(parent) = config.socket_path.parent() {
        std::fs::create_dir_all(parent)?;
    }

    // Bind socket
    let listener = UnixListener::bind(&config.socket_path)?;
    listener.set_nonblocking(true)?;

    log::info!("Listening on {:?}", config.socket_path);

    // Set up signal handler
    let shutdown_flag = daemon.shutdown_flag();
    ctrlc::set_handler(move || {
        log::info!("Received shutdown signal");
        shutdown_flag.store(true, Ordering::Relaxed);
    })
    .expect("Failed to set signal handler");

    // Main loop
    while !daemon.shutdown_requested() {
        let frame_start = Instant::now();

        // Accept new connections (non-blocking)
        match listener.accept() {
            Ok((stream, _addr)) => {
                // TODO: Spawn a thread or use async to handle this client
                match daemon.accept_client() {
                    Ok(client_id) => {
                        log::debug!("Accepted client: {}", client_id);
                        // TODO: Store the stream with the client for message I/O
                    }
                    Err(e) => {
                        log::warn!("Failed to accept client: {}", e);
                    }
                }
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                // No pending connections
            }
            Err(e) => {
                log::error!("Accept error: {}", e);
            }
        }

        // Process queued messages
        daemon.process_messages();

        // Run frame
        daemon.run_frame();

        // TODO: Send outgoing messages to clients

        // Frame pacing
        let frame_duration = frame_start.elapsed();
        if frame_duration < config.target_frame_time {
            std::thread::sleep(config.target_frame_time - frame_duration);
        }
    }

    daemon.graceful_shutdown();

    // Clean up socket
    let _ = std::fs::remove_file(&config.socket_path);

    Ok(())
}

/// Non-Unix placeholder for run_daemon
#[cfg(not(unix))]
pub fn run_daemon(config: DaemonConfig) -> Result<(), DaemonError> {
    log::info!("Starting kernel daemon (non-Unix mode)...");

    let mut daemon = KernelDaemon::new(config.clone());
    daemon.init()?;

    log::info!("Daemon running in headless mode (no socket support on this platform)");

    // Main loop without socket
    while !daemon.shutdown_requested() {
        let frame_start = Instant::now();

        daemon.process_messages();
        daemon.run_frame();

        let frame_duration = frame_start.elapsed();
        if frame_duration < config.target_frame_time {
            std::thread::sleep(config.target_frame_time - frame_duration);
        }
    }

    daemon.graceful_shutdown();

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_config() -> DaemonConfig {
        DaemonConfig {
            socket_path: PathBuf::from("./test.sock"),
            persist: crate::persist::PersistConfig::testing(),
            ..DaemonConfig::default()
        }
    }

    #[test]
    fn test_daemon_creation() {
        let config = test_config();
        let daemon = KernelDaemon::new(config);

        assert_eq!(daemon.state(), DaemonState::Starting);
    }

    #[test]
    fn test_daemon_init() {
        let config = test_config();
        let mut daemon = KernelDaemon::new(config);

        daemon.init().unwrap();
        assert_eq!(daemon.state(), DaemonState::Running);
    }

    #[test]
    fn test_daemon_frame() {
        let config = test_config();
        let mut daemon = KernelDaemon::new(config);
        daemon.init().unwrap();

        let frame_before = daemon.kernel().frame();
        daemon.run_frame();
        let frame_after = daemon.kernel().frame();

        assert_eq!(frame_after, frame_before + 1);
    }

    #[test]
    fn test_daemon_shutdown() {
        let config = test_config();
        let mut daemon = KernelDaemon::new(config);
        daemon.init().unwrap();

        assert!(!daemon.shutdown_requested());

        daemon.request_shutdown();
        assert!(daemon.shutdown_requested());

        daemon.graceful_shutdown();
        assert_eq!(daemon.state(), DaemonState::Stopped);
    }

    #[test]
    fn test_daemon_client_lifecycle() {
        let config = test_config();
        let mut daemon = KernelDaemon::new(config);
        daemon.init().unwrap();

        // Accept client
        let client_id = daemon.accept_client().unwrap();
        assert_eq!(daemon.clients().len(), 1);

        // Queue and process registration
        daemon.queue_message(
            client_id,
            MessageId::new(),
            ClientMessage::RegisterApp {
                registration: AppRegistration {
                    name: "test_app".to_string(),
                    version: "1.0.0".to_string(),
                    capabilities: vec![],
                },
            },
        );
        daemon.process_messages();

        // Client should be registered
        let client = daemon.clients().get(client_id).unwrap();
        assert_eq!(client.state(), ClientState::Active);
        assert!(client.info.app_id.is_some());
    }

    #[test]
    fn test_daemon_stats() {
        let config = test_config();
        let mut daemon = KernelDaemon::new(config);
        daemon.init().unwrap();

        // Run a few frames
        for _ in 0..10 {
            daemon.run_frame();
            std::thread::sleep(Duration::from_millis(1));
        }

        let stats = daemon.stats();
        assert!(stats.frame >= 10);
        assert!(stats.uptime_secs > 0.0);
    }
}
