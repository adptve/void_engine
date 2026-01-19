//! Kernel IPC Protocol
//!
//! Defines the message protocol for communication between the kernel daemon
//! and connected clients (apps, session manager, shell).
//!
//! # Protocol Design
//!
//! - Messages are serialized with bincode for performance
//! - Each message has a header with length and type
//! - Responses include the original request ID for correlation
//! - All messages are versioned for backward compatibility

use serde::{Deserialize, Serialize};
use std::io::{self, Read, Write};
use void_ir::{Transaction, TransactionId, NamespaceId};

use crate::capability::{Capability, CapabilityId, CapabilityKind};
use crate::app::AppId;

/// Protocol version for compatibility checks
pub const PROTOCOL_VERSION: u32 = 1;

/// Maximum message size (16 MB)
pub const MAX_MESSAGE_SIZE: usize = 16 * 1024 * 1024;

/// Unique identifier for a message (for request/response correlation)
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct MessageId(u64);

impl MessageId {
    /// Create a new unique message ID
    pub fn new() -> Self {
        use std::sync::atomic::{AtomicU64, Ordering};
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID value
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for MessageId {
    fn default() -> Self {
        Self::new()
    }
}

/// Message header for framing
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MessageHeader {
    /// Protocol version
    pub version: u32,
    /// Message ID for correlation
    pub id: MessageId,
    /// Message length in bytes (excluding header)
    pub length: u32,
    /// Message type discriminant for quick routing
    pub msg_type: MessageType,
}

impl MessageHeader {
    /// Create a new header
    pub fn new(id: MessageId, length: u32, msg_type: MessageType) -> Self {
        Self {
            version: PROTOCOL_VERSION,
            id,
            length,
            msg_type,
        }
    }

    /// Serialize header to bytes
    pub fn to_bytes(&self) -> Result<Vec<u8>, IpcError> {
        bincode::serialize(self).map_err(|e| IpcError::Serialization(e.to_string()))
    }

    /// Deserialize header from bytes
    pub fn from_bytes(bytes: &[u8]) -> Result<Self, IpcError> {
        bincode::deserialize(bytes).map_err(|e| IpcError::Deserialization(e.to_string()))
    }

    /// Header size in bytes (fixed for efficient reading)
    pub const SIZE: usize = 24;
}

/// Message type discriminant for quick routing
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum MessageType {
    // Client -> Kernel (requests)
    SubmitTransaction = 1,
    RequestCapability = 2,
    Heartbeat = 3,
    RegisterApp = 4,
    UnregisterApp = 5,
    QueryStatus = 6,

    // Kernel -> Client (responses/notifications)
    TransactionResult = 128,
    CapabilityGranted = 129,
    CapabilityDenied = 130,
    FrameBegin = 131,
    FrameEnd = 132,
    Shutdown = 133,
    Recovery = 134,
    StatusReport = 135,
    AppRegistered = 136,
    Error = 255,
}

/// Request for a capability
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CapabilityRequest {
    /// The kind of capability being requested
    pub kind: CapabilityRequestKind,
    /// Reason for the request (for auditing)
    pub reason: Option<String>,
    /// Whether this capability should be delegable
    pub delegable: bool,
    /// Duration in seconds (None = permanent until revoked)
    pub duration_secs: Option<u64>,
}

/// Kinds of capabilities that can be requested
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum CapabilityRequestKind {
    /// Create entities (optionally with max count)
    CreateEntities { max: Option<u32> },
    /// Destroy entities
    DestroyEntities,
    /// Modify components (optionally limited to types)
    ModifyComponents { allowed_types: Option<Vec<String>> },
    /// Create layers (optionally with max count)
    CreateLayers { max: Option<u32> },
    /// Load assets (optionally limited to paths)
    LoadAssets { allowed_paths: Option<Vec<String>> },
    /// Access network (optionally limited to hosts)
    AccessNetwork { allowed_hosts: Option<Vec<String>> },
    /// Access filesystem (optionally limited to paths)
    AccessFilesystem { allowed_paths: Option<Vec<String>> },
    /// Execute scripts
    ExecuteScripts,
}

impl CapabilityRequestKind {
    /// Convert to the kernel's CapabilityKind
    pub fn to_capability_kind(&self) -> CapabilityKind {
        match self {
            Self::CreateEntities { max } => CapabilityKind::CreateEntities { max: *max },
            Self::DestroyEntities => CapabilityKind::DestroyEntities,
            Self::ModifyComponents { allowed_types } => {
                CapabilityKind::ModifyComponents {
                    allowed_types: allowed_types.clone(),
                }
            }
            Self::CreateLayers { max } => CapabilityKind::CreateLayers { max: *max },
            Self::LoadAssets { allowed_paths } => {
                CapabilityKind::LoadAssets {
                    allowed_paths: allowed_paths.clone(),
                }
            }
            Self::AccessNetwork { allowed_hosts } => {
                CapabilityKind::AccessNetwork {
                    allowed_hosts: allowed_hosts.clone(),
                }
            }
            Self::AccessFilesystem { allowed_paths } => {
                CapabilityKind::AccessFilesystem {
                    allowed_paths: allowed_paths.clone(),
                }
            }
            Self::ExecuteScripts => CapabilityKind::ExecuteScripts,
        }
    }
}

/// App registration request
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppRegistration {
    /// App name
    pub name: String,
    /// App version
    pub version: String,
    /// Requested capabilities
    pub capabilities: Vec<CapabilityRequestKind>,
}

/// Serializable capability info for IPC
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CapabilityInfo {
    /// Capability ID
    pub id: u64,
    /// Kind name
    pub kind: String,
    /// Whether it's delegable
    pub delegable: bool,
    /// Seconds until expiration (None = never)
    pub expires_in_secs: Option<u64>,
}

impl CapabilityInfo {
    /// Create from a Capability
    pub fn from_capability(cap: &Capability) -> Self {
        Self {
            id: cap.id.raw(),
            kind: cap.kind.name().to_string(),
            delegable: cap.delegable,
            expires_in_secs: cap.expires_at.map(|exp| {
                exp.saturating_duration_since(std::time::Instant::now())
                    .as_secs()
            }),
        }
    }
}

/// Kernel status report
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KernelStatusReport {
    /// Current frame number
    pub frame: u64,
    /// Uptime in seconds
    pub uptime_secs: f64,
    /// Average FPS
    pub avg_fps: f32,
    /// Number of running apps
    pub running_apps: usize,
    /// Total layer count
    pub layer_count: usize,
    /// Total asset count
    pub asset_count: usize,
    /// Health status
    pub health: String,
    /// Backend name
    pub backend: String,
}

/// Messages that clients can send to the kernel
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ClientMessage {
    /// Submit a transaction for processing
    SubmitTransaction {
        /// The transaction to submit
        transaction: SerializableTransaction,
    },

    /// Request a capability
    RequestCapability {
        /// The capability request
        request: CapabilityRequest,
    },

    /// Heartbeat to keep connection alive
    Heartbeat {
        /// Client's timestamp for RTT calculation
        client_timestamp: u64,
    },

    /// Register as an app
    RegisterApp {
        /// App registration info
        registration: AppRegistration,
    },

    /// Unregister an app
    UnregisterApp,

    /// Query kernel status
    QueryStatus,
}

/// Messages that the kernel sends to clients
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum KernelMessage {
    /// Result of a transaction submission
    TransactionResult {
        /// The transaction ID
        id: u64,
        /// Whether it succeeded
        success: bool,
        /// Error message if failed
        error: Option<String>,
        /// Number of patches applied
        patches_applied: usize,
    },

    /// A capability was granted
    CapabilityGranted {
        /// The granted capability info
        capability: CapabilityInfo,
    },

    /// A capability request was denied
    CapabilityDenied {
        /// Reason for denial
        reason: String,
    },

    /// Frame has begun
    FrameBegin {
        /// Frame number
        frame: u64,
        /// Frame timestamp (nanoseconds since epoch)
        timestamp_ns: u64,
        /// Delta time from last frame (seconds)
        delta_time: f32,
    },

    /// Frame has ended
    FrameEnd {
        /// Frame number
        frame: u64,
        /// Frame duration in milliseconds
        duration_ms: f32,
    },

    /// Kernel is shutting down
    Shutdown {
        /// Reason for shutdown
        reason: String,
        /// Grace period in milliseconds before forced disconnect
        grace_period_ms: u64,
    },

    /// Recovery notification after crash
    Recovery {
        /// Last successfully processed frame before crash
        last_frame: u64,
        /// Reason for recovery
        reason: String,
    },

    /// Status report response
    StatusReport {
        /// The status report
        status: KernelStatusReport,
    },

    /// App was successfully registered
    AppRegistered {
        /// Assigned app ID
        app_id: u64,
        /// Assigned namespace ID
        namespace_id: u64,
        /// Granted capabilities
        capabilities: Vec<CapabilityInfo>,
    },

    /// Error response
    Error {
        /// Error code
        code: ErrorCode,
        /// Error message
        message: String,
    },
}

/// Error codes for IPC errors
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u16)]
pub enum ErrorCode {
    /// Unknown error
    Unknown = 0,
    /// Invalid message format
    InvalidMessage = 1,
    /// Protocol version mismatch
    VersionMismatch = 2,
    /// Message too large
    MessageTooLarge = 3,
    /// Authentication failed
    AuthenticationFailed = 4,
    /// Permission denied
    PermissionDenied = 5,
    /// Resource not found
    NotFound = 6,
    /// Rate limited
    RateLimited = 7,
    /// Kernel is shutting down
    ShuttingDown = 8,
    /// Invalid state
    InvalidState = 9,
    /// Transaction failed
    TransactionFailed = 10,
}

/// Serializable version of Transaction for IPC
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SerializableTransaction {
    /// Transaction ID
    pub id: u64,
    /// Source namespace ID
    pub source: u64,
    /// Patches (serialized as JSON for flexibility)
    pub patches_json: String,
    /// Description
    pub description: Option<String>,
    /// Dependencies
    pub dependencies: Vec<u64>,
}

impl SerializableTransaction {
    /// Create from a Transaction
    pub fn from_transaction(tx: &Transaction) -> Result<Self, IpcError> {
        let patches_json = serde_json::to_string(&tx.patches)
            .map_err(|e| IpcError::Serialization(e.to_string()))?;

        Ok(Self {
            id: tx.id.raw(),
            source: tx.source.raw(),
            patches_json,
            description: tx.description.clone(),
            dependencies: tx.dependencies.iter().map(|d| d.raw()).collect(),
        })
    }

    /// Convert to Transaction (requires namespace validation)
    pub fn to_transaction(&self, source_ns: NamespaceId) -> Result<Transaction, IpcError> {
        let patches: Vec<void_ir::Patch> = serde_json::from_str(&self.patches_json)
            .map_err(|e| IpcError::Deserialization(e.to_string()))?;

        let mut tx = Transaction::new(source_ns);
        tx.description = self.description.clone();
        for patch in patches {
            tx.add_patch(patch);
        }
        tx.submit();

        Ok(tx)
    }
}

/// IPC error types
#[derive(Debug, Clone)]
pub enum IpcError {
    /// IO error
    Io(String),
    /// Serialization error
    Serialization(String),
    /// Deserialization error
    Deserialization(String),
    /// Protocol version mismatch
    VersionMismatch { expected: u32, got: u32 },
    /// Message too large
    MessageTooLarge { size: usize, max: usize },
    /// Connection closed
    ConnectionClosed,
    /// Invalid message type
    InvalidMessageType(u8),
    /// Timeout
    Timeout,
}

impl std::fmt::Display for IpcError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io(e) => write!(f, "IO error: {}", e),
            Self::Serialization(e) => write!(f, "Serialization error: {}", e),
            Self::Deserialization(e) => write!(f, "Deserialization error: {}", e),
            Self::VersionMismatch { expected, got } => {
                write!(f, "Protocol version mismatch: expected {}, got {}", expected, got)
            }
            Self::MessageTooLarge { size, max } => {
                write!(f, "Message too large: {} bytes (max {})", size, max)
            }
            Self::ConnectionClosed => write!(f, "Connection closed"),
            Self::InvalidMessageType(t) => write!(f, "Invalid message type: {}", t),
            Self::Timeout => write!(f, "Operation timed out"),
        }
    }
}

impl std::error::Error for IpcError {}

impl From<io::Error> for IpcError {
    fn from(e: io::Error) -> Self {
        if e.kind() == io::ErrorKind::UnexpectedEof {
            Self::ConnectionClosed
        } else {
            Self::Io(e.to_string())
        }
    }
}

/// Write a message to a stream
pub fn write_message<W: Write>(
    writer: &mut W,
    id: MessageId,
    msg: &KernelMessage,
) -> Result<(), IpcError> {
    // Serialize the message body
    let body = bincode::serialize(msg)
        .map_err(|e| IpcError::Serialization(e.to_string()))?;

    if body.len() > MAX_MESSAGE_SIZE {
        return Err(IpcError::MessageTooLarge {
            size: body.len(),
            max: MAX_MESSAGE_SIZE,
        });
    }

    // Determine message type
    let msg_type = match msg {
        KernelMessage::TransactionResult { .. } => MessageType::TransactionResult,
        KernelMessage::CapabilityGranted { .. } => MessageType::CapabilityGranted,
        KernelMessage::CapabilityDenied { .. } => MessageType::CapabilityDenied,
        KernelMessage::FrameBegin { .. } => MessageType::FrameBegin,
        KernelMessage::FrameEnd { .. } => MessageType::FrameEnd,
        KernelMessage::Shutdown { .. } => MessageType::Shutdown,
        KernelMessage::Recovery { .. } => MessageType::Recovery,
        KernelMessage::StatusReport { .. } => MessageType::StatusReport,
        KernelMessage::AppRegistered { .. } => MessageType::AppRegistered,
        KernelMessage::Error { .. } => MessageType::Error,
    };

    // Create and write header
    let header = MessageHeader::new(id, body.len() as u32, msg_type);
    let header_bytes = header.to_bytes()?;

    writer.write_all(&header_bytes)?;
    writer.write_all(&body)?;
    writer.flush()?;

    Ok(())
}

/// Read a client message from a stream
pub fn read_client_message<R: Read>(
    reader: &mut R,
) -> Result<(MessageId, ClientMessage), IpcError> {
    // Read header
    let mut header_buf = vec![0u8; MessageHeader::SIZE];
    reader.read_exact(&mut header_buf)?;

    let header = MessageHeader::from_bytes(&header_buf)?;

    // Validate version
    if header.version != PROTOCOL_VERSION {
        return Err(IpcError::VersionMismatch {
            expected: PROTOCOL_VERSION,
            got: header.version,
        });
    }

    // Validate size
    if header.length as usize > MAX_MESSAGE_SIZE {
        return Err(IpcError::MessageTooLarge {
            size: header.length as usize,
            max: MAX_MESSAGE_SIZE,
        });
    }

    // Read body
    let mut body = vec![0u8; header.length as usize];
    reader.read_exact(&mut body)?;

    // Deserialize based on type
    let msg: ClientMessage = bincode::deserialize(&body)
        .map_err(|e| IpcError::Deserialization(e.to_string()))?;

    Ok((header.id, msg))
}

/// Write a client message to a stream
pub fn write_client_message<W: Write>(
    writer: &mut W,
    id: MessageId,
    msg: &ClientMessage,
) -> Result<(), IpcError> {
    // Serialize the message body
    let body = bincode::serialize(msg)
        .map_err(|e| IpcError::Serialization(e.to_string()))?;

    if body.len() > MAX_MESSAGE_SIZE {
        return Err(IpcError::MessageTooLarge {
            size: body.len(),
            max: MAX_MESSAGE_SIZE,
        });
    }

    // Determine message type
    let msg_type = match msg {
        ClientMessage::SubmitTransaction { .. } => MessageType::SubmitTransaction,
        ClientMessage::RequestCapability { .. } => MessageType::RequestCapability,
        ClientMessage::Heartbeat { .. } => MessageType::Heartbeat,
        ClientMessage::RegisterApp { .. } => MessageType::RegisterApp,
        ClientMessage::UnregisterApp => MessageType::UnregisterApp,
        ClientMessage::QueryStatus => MessageType::QueryStatus,
    };

    // Create and write header
    let header = MessageHeader::new(id, body.len() as u32, msg_type);
    let header_bytes = header.to_bytes()?;

    writer.write_all(&header_bytes)?;
    writer.write_all(&body)?;
    writer.flush()?;

    Ok(())
}

/// Read a kernel message from a stream
pub fn read_kernel_message<R: Read>(
    reader: &mut R,
) -> Result<(MessageId, KernelMessage), IpcError> {
    // Read header
    let mut header_buf = vec![0u8; MessageHeader::SIZE];
    reader.read_exact(&mut header_buf)?;

    let header = MessageHeader::from_bytes(&header_buf)?;

    // Validate version
    if header.version != PROTOCOL_VERSION {
        return Err(IpcError::VersionMismatch {
            expected: PROTOCOL_VERSION,
            got: header.version,
        });
    }

    // Validate size
    if header.length as usize > MAX_MESSAGE_SIZE {
        return Err(IpcError::MessageTooLarge {
            size: header.length as usize,
            max: MAX_MESSAGE_SIZE,
        });
    }

    // Read body
    let mut body = vec![0u8; header.length as usize];
    reader.read_exact(&mut body)?;

    // Deserialize
    let msg: KernelMessage = bincode::deserialize(&body)
        .map_err(|e| IpcError::Deserialization(e.to_string()))?;

    Ok((header.id, msg))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    #[test]
    fn test_message_id() {
        let id1 = MessageId::new();
        let id2 = MessageId::new();
        assert_ne!(id1.raw(), id2.raw());
    }

    #[test]
    fn test_message_roundtrip() {
        let msg = KernelMessage::FrameBegin {
            frame: 100,
            timestamp_ns: 1234567890,
            delta_time: 0.016,
        };

        let mut buffer = Vec::new();
        let id = MessageId::new();
        write_message(&mut buffer, id, &msg).unwrap();

        let mut cursor = Cursor::new(buffer);
        let (read_id, read_msg) = read_kernel_message(&mut cursor).unwrap();

        assert_eq!(id.raw(), read_id.raw());
        if let KernelMessage::FrameBegin { frame, .. } = read_msg {
            assert_eq!(frame, 100);
        } else {
            panic!("Wrong message type");
        }
    }

    #[test]
    fn test_client_message_roundtrip() {
        let msg = ClientMessage::Heartbeat {
            client_timestamp: 1234567890,
        };

        let mut buffer = Vec::new();
        let id = MessageId::new();
        write_client_message(&mut buffer, id, &msg).unwrap();

        let mut cursor = Cursor::new(buffer);
        let (read_id, read_msg) = read_client_message(&mut cursor).unwrap();

        assert_eq!(id.raw(), read_id.raw());
        if let ClientMessage::Heartbeat { client_timestamp } = read_msg {
            assert_eq!(client_timestamp, 1234567890);
        } else {
            panic!("Wrong message type");
        }
    }

    #[test]
    fn test_capability_request_conversion() {
        let request = CapabilityRequestKind::CreateEntities { max: Some(100) };
        let kind = request.to_capability_kind();

        if let CapabilityKind::CreateEntities { max } = kind {
            assert_eq!(max, Some(100));
        } else {
            panic!("Wrong capability kind");
        }
    }
}
