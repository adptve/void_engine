//! Transport Layer
//!
//! Provides abstract transport interface with WebSocket implementation.
//! Designed for extension to QUIC and WebRTC in the future.

use std::sync::Arc;

use futures_util::{SinkExt, StreamExt};
use parking_lot::Mutex;
use thiserror::Error;
use tokio::net::TcpStream;
use tokio::sync::mpsc;
use tokio_tungstenite::{
    connect_async, tungstenite::protocol::Message, MaybeTlsStream, WebSocketStream,
};

/// Transport errors
#[derive(Debug, Error)]
pub enum TransportError {
    #[error("Connection failed: {0}")]
    ConnectionFailed(String),

    #[error("Connection closed")]
    Closed,

    #[error("Send failed: {0}")]
    SendFailed(String),

    #[error("Receive failed: {0}")]
    ReceiveFailed(String),

    #[error("Invalid URL: {0}")]
    InvalidUrl(String),

    #[error("TLS error: {0}")]
    Tls(String),

    #[error("Timeout")]
    Timeout,

    #[error("Protocol error: {0}")]
    Protocol(String),

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
}

/// Result type for transport operations
pub type TransportResult<T> = Result<T, TransportError>;

/// Abstract transport trait for network communication
#[async_trait::async_trait]
pub trait Transport: Send + Sync {
    /// Send a message with reliable delivery (guaranteed, ordered)
    async fn send_reliable(&mut self, data: &[u8]) -> TransportResult<()>;

    /// Send a message with unreliable delivery (low latency, may drop)
    /// Default implementation falls back to reliable
    async fn send_unreliable(&mut self, data: &[u8]) -> TransportResult<()> {
        self.send_reliable(data).await
    }

    /// Receive a message (blocking)
    async fn receive(&mut self) -> TransportResult<Vec<u8>>;

    /// Check if the transport is connected
    fn is_connected(&self) -> bool;

    /// Close the transport
    async fn close(&mut self) -> TransportResult<()>;

    /// Get transport statistics
    fn stats(&self) -> TransportStats;
}

/// Transport statistics
#[derive(Debug, Clone, Default)]
pub struct TransportStats {
    /// Total bytes sent
    pub bytes_sent: u64,
    /// Total bytes received
    pub bytes_received: u64,
    /// Total messages sent
    pub messages_sent: u64,
    /// Total messages received
    pub messages_received: u64,
    /// Number of send errors
    pub send_errors: u64,
    /// Number of receive errors
    pub receive_errors: u64,
}

/// WebSocket transport implementation
pub struct WebSocketTransport {
    /// WebSocket stream (write half)
    write: Option<
        futures_util::stream::SplitSink<
            WebSocketStream<MaybeTlsStream<TcpStream>>,
            Message,
        >,
    >,
    /// Receiver for incoming messages
    receiver: mpsc::UnboundedReceiver<TransportResult<Vec<u8>>>,
    /// Statistics
    stats: Arc<Mutex<TransportStats>>,
    /// Connection state
    connected: Arc<std::sync::atomic::AtomicBool>,
}

impl WebSocketTransport {
    /// Connect to a WebSocket server
    pub async fn connect(url: &str) -> TransportResult<Self> {
        log::debug!("Connecting to WebSocket: {}", url);

        let (ws_stream, _response) = connect_async(url)
            .await
            .map_err(|e| TransportError::ConnectionFailed(e.to_string()))?;

        log::debug!("WebSocket connection established");

        let (write, read) = ws_stream.split();
        let (tx, rx) = mpsc::unbounded_channel();
        let stats = Arc::new(Mutex::new(TransportStats::default()));
        let connected = Arc::new(std::sync::atomic::AtomicBool::new(true));

        // Spawn read task
        let stats_clone = stats.clone();
        let connected_clone = connected.clone();
        tokio::spawn(async move {
            Self::read_loop(read, tx, stats_clone, connected_clone).await;
        });

        Ok(Self {
            write: Some(write),
            receiver: rx,
            stats,
            connected,
        })
    }

    /// Background read loop
    async fn read_loop(
        mut read: futures_util::stream::SplitStream<
            WebSocketStream<MaybeTlsStream<TcpStream>>,
        >,
        tx: mpsc::UnboundedSender<TransportResult<Vec<u8>>>,
        stats: Arc<Mutex<TransportStats>>,
        connected: Arc<std::sync::atomic::AtomicBool>,
    ) {
        loop {
            match read.next().await {
                Some(Ok(message)) => {
                    match message {
                        Message::Binary(data) => {
                            {
                                let mut stats = stats.lock();
                                stats.bytes_received += data.len() as u64;
                                stats.messages_received += 1;
                            }
                            if tx.send(Ok(data)).is_err() {
                                break;
                            }
                        }
                        Message::Text(text) => {
                            let data = text.into_bytes();
                            {
                                let mut stats = stats.lock();
                                stats.bytes_received += data.len() as u64;
                                stats.messages_received += 1;
                            }
                            if tx.send(Ok(data)).is_err() {
                                break;
                            }
                        }
                        Message::Ping(_) | Message::Pong(_) => {
                            // Handle ping/pong internally
                        }
                        Message::Close(_) => {
                            connected.store(false, std::sync::atomic::Ordering::SeqCst);
                            let _ = tx.send(Err(TransportError::Closed));
                            break;
                        }
                        Message::Frame(_) => {
                            // Raw frame, ignore
                        }
                    }
                }
                Some(Err(e)) => {
                    {
                        let mut stats = stats.lock();
                        stats.receive_errors += 1;
                    }
                    connected.store(false, std::sync::atomic::Ordering::SeqCst);
                    let _ = tx.send(Err(TransportError::ReceiveFailed(e.to_string())));
                    break;
                }
                None => {
                    connected.store(false, std::sync::atomic::Ordering::SeqCst);
                    let _ = tx.send(Err(TransportError::Closed));
                    break;
                }
            }
        }
    }
}

#[async_trait::async_trait]
impl Transport for WebSocketTransport {
    async fn send_reliable(&mut self, data: &[u8]) -> TransportResult<()> {
        let write = self
            .write
            .as_mut()
            .ok_or(TransportError::Closed)?;

        let message = Message::Binary(data.to_vec());
        write
            .send(message)
            .await
            .map_err(|e| {
                let mut stats = self.stats.lock();
                stats.send_errors += 1;
                TransportError::SendFailed(e.to_string())
            })?;

        {
            let mut stats = self.stats.lock();
            stats.bytes_sent += data.len() as u64;
            stats.messages_sent += 1;
        }

        Ok(())
    }

    async fn receive(&mut self) -> TransportResult<Vec<u8>> {
        self.receiver
            .recv()
            .await
            .ok_or(TransportError::Closed)?
    }

    fn is_connected(&self) -> bool {
        self.connected.load(std::sync::atomic::Ordering::SeqCst)
    }

    async fn close(&mut self) -> TransportResult<()> {
        if let Some(mut write) = self.write.take() {
            let _ = write.send(Message::Close(None)).await;
            let _ = write.close().await;
        }
        self.connected
            .store(false, std::sync::atomic::Ordering::SeqCst);
        Ok(())
    }

    fn stats(&self) -> TransportStats {
        self.stats.lock().clone()
    }
}

/// Transport type enumeration for configuration
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TransportType {
    /// WebSocket transport
    WebSocket,
    /// QUIC transport (future)
    Quic,
    /// WebRTC transport (future)
    WebRtc,
    /// Raw TCP transport (fallback)
    Tcp,
}

impl TransportType {
    /// Get the default port for this transport type
    pub fn default_port(&self, secure: bool) -> u16 {
        match (self, secure) {
            (TransportType::WebSocket, true) => 443,
            (TransportType::WebSocket, false) => 80,
            (TransportType::Quic, _) => 443,
            (TransportType::WebRtc, _) => 0, // WebRTC uses dynamic ports
            (TransportType::Tcp, true) => 443,
            (TransportType::Tcp, false) => 8080,
        }
    }

    /// Check if this transport supports unreliable delivery
    pub fn supports_unreliable(&self) -> bool {
        matches!(self, TransportType::Quic | TransportType::WebRtc)
    }
}

/// Transport selector for choosing best available transport
pub struct TransportSelector {
    /// Preferred transports in order of preference
    preferred: Vec<TransportType>,
}

impl TransportSelector {
    /// Create a new transport selector with default preferences
    pub fn new() -> Self {
        Self {
            preferred: vec![
                TransportType::WebSocket, // Most widely supported
                TransportType::Tcp,       // Fallback
            ],
        }
    }

    /// Create with custom transport preferences
    pub fn with_preferences(preferred: Vec<TransportType>) -> Self {
        Self { preferred }
    }

    /// Get the list of preferred transports
    pub fn preferences(&self) -> &[TransportType] {
        &self.preferred
    }

    /// Try to connect using the best available transport
    pub async fn connect(&self, url: &str) -> TransportResult<Box<dyn Transport>> {
        for transport_type in &self.preferred {
            match transport_type {
                TransportType::WebSocket => {
                    match WebSocketTransport::connect(url).await {
                        Ok(transport) => {
                            log::info!("Connected using WebSocket transport");
                            return Ok(Box::new(transport));
                        }
                        Err(e) => {
                            log::warn!("WebSocket connection failed: {}", e);
                            continue;
                        }
                    }
                }
                TransportType::Quic => {
                    log::debug!("QUIC transport not yet implemented");
                    continue;
                }
                TransportType::WebRtc => {
                    log::debug!("WebRTC transport not yet implemented");
                    continue;
                }
                TransportType::Tcp => {
                    log::debug!("Raw TCP transport not yet implemented");
                    continue;
                }
            }
        }

        Err(TransportError::ConnectionFailed(
            "No transport available".to_string(),
        ))
    }
}

impl Default for TransportSelector {
    fn default() -> Self {
        Self::new()
    }
}

/// Message compression utilities
pub mod compression {
    use super::TransportResult;

    /// Compression level
    #[derive(Debug, Clone, Copy)]
    pub enum CompressionLevel {
        /// No compression
        None,
        /// Fast compression (low CPU, moderate ratio)
        Fast,
        /// Default compression (balanced)
        Default,
        /// Best compression (high CPU, best ratio)
        Best,
    }

    impl Default for CompressionLevel {
        fn default() -> Self {
            Self::Default
        }
    }

    /// Compress data using LZ4
    pub fn compress(data: &[u8], _level: CompressionLevel) -> Vec<u8> {
        lz4_flex::compress_prepend_size(data)
    }

    /// Decompress LZ4 data
    pub fn decompress(data: &[u8]) -> TransportResult<Vec<u8>> {
        lz4_flex::decompress_size_prepended(data)
            .map_err(|e| super::TransportError::Protocol(format!("Decompression failed: {}", e)))
    }

    /// Check if compression would be beneficial for this data size
    pub fn should_compress(data_len: usize, threshold: usize) -> bool {
        data_len >= threshold
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_transport_type_ports() {
        assert_eq!(TransportType::WebSocket.default_port(true), 443);
        assert_eq!(TransportType::WebSocket.default_port(false), 80);
        assert_eq!(TransportType::Tcp.default_port(false), 8080);
    }

    #[test]
    fn test_transport_type_unreliable() {
        assert!(!TransportType::WebSocket.supports_unreliable());
        assert!(!TransportType::Tcp.supports_unreliable());
        assert!(TransportType::Quic.supports_unreliable());
        assert!(TransportType::WebRtc.supports_unreliable());
    }

    #[test]
    fn test_transport_selector_preferences() {
        let selector = TransportSelector::new();
        assert!(!selector.preferences().is_empty());
        assert_eq!(selector.preferences()[0], TransportType::WebSocket);

        let custom = TransportSelector::with_preferences(vec![TransportType::Tcp]);
        assert_eq!(custom.preferences().len(), 1);
    }

    #[test]
    fn test_compression() {
        let data = b"Hello, World! This is some test data that should be compressed.";

        let compressed = compression::compress(data, compression::CompressionLevel::Default);
        let decompressed = compression::decompress(&compressed).unwrap();

        assert_eq!(decompressed, data.to_vec());
    }

    #[test]
    fn test_compression_threshold() {
        assert!(!compression::should_compress(100, 1024));
        assert!(compression::should_compress(2048, 1024));
        assert!(compression::should_compress(1024, 1024));
    }

    #[test]
    fn test_transport_stats_default() {
        let stats = TransportStats::default();
        assert_eq!(stats.bytes_sent, 0);
        assert_eq!(stats.bytes_received, 0);
        assert_eq!(stats.messages_sent, 0);
        assert_eq!(stats.messages_received, 0);
    }
}
